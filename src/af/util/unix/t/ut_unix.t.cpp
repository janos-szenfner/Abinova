/* AbiSource Program Utilities
 * Copyright (C) 2026 Abinova contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

/* unit tests for af/util/unix: PATH lookup, directory creation,
 * file-stat helpers, filename legalization, the stubbed ethernet
 * call, the Unix idle/timer wrappers around GLib sources, the
 * ut_g_silent early return in _UT_OutputMessage, and the interactive
 * assert prompt (ut_unixAssert.cpp never sees NDEBUG — it does not
 * include config.h — so the prompt is compiled even in release
 * builds).
 *
 * The assert prompt reads stdin and may abort() or raise SIGTRAP, so
 * its cases run with a dup2'd stdin: the 'n' (abort) answer is caught
 * in-process through a SIGABRT handler + siglongjmp, and the 'b'
 * (break-into-debugger) answer runs in a forked child on its own
 * process group so kill(0, SIGTRAP) cannot hit the test runner. */

#include "tf_test.h"

#include <csetjmp>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "ut_debugmsg.h"
#include "ut_files.h"
#include "ut_misc.h"
#include "ut_path.h"
#include "ut_types.h"
#include "ut_unixAssert.h"
#include "ut_unixIdle.h"
#include "ut_unixTimer.h"

/* weak so non-coverage builds (san-build, normal) link — NULL when
   libgcov is absent */
extern "C" void __gcov_dump(void) __attribute__((weak)); /* checkpoint coverage counters */

#define TFSUITE "core.af.util.unix"

namespace {

/* RAII: redirect fd 0 (stdin) to an anonymous file holding `input`,
 * and fd 1 (stdout) to /dev/null so the assert banner does not mix
 * into test output.  Restores both on destruction. */
class StdinFeed
{
public:
	StdinFeed(const char *input)
	{
		fflush(stdout);
		m_savedIn = dup(STDIN_FILENO);
		m_savedOut = dup(STDOUT_FILENO);
		m_tmp = tmpfile();
		if (input)
		{
			fputs(input, m_tmp);
		}
		rewind(m_tmp);
		dup2(fileno(m_tmp), STDIN_FILENO);
		clearerr(stdin);
		int devnull = open("/dev/null", O_WRONLY);
		dup2(devnull, STDOUT_FILENO);
		close(devnull);
	}
	~StdinFeed()
	{
		fflush(stdout);
		dup2(m_savedOut, STDOUT_FILENO);
		close(m_savedOut);
		dup2(m_savedIn, STDIN_FILENO);
		close(m_savedIn);
		fclose(m_tmp);
	}
private:
	int m_savedIn = -1;
	int m_savedOut = -1;
	FILE *m_tmp = nullptr;
};

std::string tf_scratch_path(const char *leaf)
{
	std::string dir = std::string(g_get_tmp_dir()) + "/abinova-ut-unix";
	g_mkdir_with_parents(dir.c_str(), 0700);
	return dir + "/" + leaf;
}

int s_idleCalls = 0;
void idle_cb(UT_Worker *)
{
	s_idleCalls++;
}

int s_timerCalls = 0;
void timer_cb(UT_Worker *)
{
	s_timerCalls++;
}

sigjmp_buf s_abortJmp;
volatile sig_atomic_t s_abortArmed = 0;

void abort_catcher(int)
{
	if (s_abortArmed)
	{
		siglongjmp(s_abortJmp, 1);
	}
	/* not our abort: re-raise for real */
	signal(SIGABRT, SIG_DFL);
	raise(SIGABRT);
}

/* run UT_UnixAssertMsg in a child on its own process group so the
 * 'b' answer's kill(0, SIGTRAP) reaches only the child */
int run_assert_in_child(const char *input)
{
	pid_t pid = fork();
	if (pid == 0)
	{
		setpgid(0, 0);
		StdinFeed feed(input);
		int rc = UT_UnixAssertMsg("test assert", __FILE__, __LINE__);
		if (__gcov_dump)
			__gcov_dump();
		_exit(rc < 0 ? 0 : rc);
	}
	int status = 0;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* one prompt round-trip under redirected stdio (the banner and the
 * input are both swallowed by StdinFeed) */
int call_assert(const char *input)
{
	StdinFeed feed(input);
	return UT_UnixAssertMsg("assert probe", __FILE__, __LINE__);
}

} // namespace

TFTEST_MAIN("progExists")
{
	TFPASS(progExists("sh"));
	TFPASS(!progExists("abinova-test-no-such-program-xyz"));
	TFPASS(progExists("/bin/sh"));
	TFPASS(!progExists("/nonexistent-dir-no-such/sh"));
}

TFTEST_MAIN("UT_createDirectoryIfNecessary")
{
	std::string dir = tf_scratch_path("mkd");
	g_remove(dir.c_str());

	/* fresh creation */
	TFPASS(UT_createDirectoryIfNecessary(dir.c_str()));
	TFPASS(g_file_test(dir.c_str(), G_FILE_TEST_IS_DIR));

	/* already exists */
	TFPASS(UT_createDirectoryIfNecessary(dir.c_str()));

	/* exists but is a file */
	std::string asfile = tf_scratch_path("mkd-file");
	{
		FILE *f = fopen(asfile.c_str(), "w");
		fputs("x", f);
		fclose(f);
	}
	TFPASS(!UT_createDirectoryIfNecessary(asfile.c_str()));

	/* cannot be created (parent is a file) */
	std::string bad = asfile + "/sub";
	TFPASS(!UT_createDirectoryIfNecessary(bad.c_str()));

	/* private (0700) variant */
	std::string priv = tf_scratch_path("mkd-priv");
	g_remove(priv.c_str());
	TFPASS(UT_createDirectoryIfNecessary(priv.c_str(), false));
	GStatBuf st;
	TFPASS(g_stat(priv.c_str(), &st) == 0 &&
	       (st.st_mode & 0777) == 0700);

	g_remove(asfile.c_str());
	g_remove(dir.c_str());
	g_remove(priv.c_str());
}

TFTEST_MAIN("path_stat_helpers")
{
	std::string real = tf_scratch_path("statme");
	{
		FILE *f = fopen(real.c_str(), "w");
		fputs("abcd", f);
		fclose(f);
	}

	TFPASS(UT_isRegularFile(real.c_str()));
	TFPASS(!UT_isRegularFile("/nonexistent-ut-path-xyz"));
	TFPASS(UT_fileSize(real.c_str()) == 4);
	TFPASS(UT_fileSize("/nonexistent-ut-path-xyz") == 0);
	TFPASS(UT_mTime(real.c_str()) != static_cast<time_t>(-1));
	TFPASS(UT_mTime("/nonexistent-ut-path-xyz") == static_cast<time_t>(-1));

	g_remove(real.c_str());
}

TFTEST_MAIN("UT_legalizeFileName")
{
	std::string clean = "plain-name";
	TFPASS(!UT_legalizeFileName(clean));
	TFPASS(clean == "plain-name");

	std::string dirty = "a/b/c";
	TFPASS(UT_legalizeFileName(dirty));
	TFPASS(dirty == "a-b-c");
}

TFTEST_MAIN("UT_getEthernetAddress")
{
	/* stub: always fails (UT_ASSERT is a no-op under NDEBUG) */
	UT_EthernetAddress addr;
	memset(addr, 0, sizeof(addr));
	TFPASS(!UT_getEthernetAddress(addr));
}

TFTEST_MAIN("UT_UnixIdle lifecycle")
{
	s_idleCalls = 0;
	UT_UnixIdle *idle = new UT_UnixIdle(idle_cb, nullptr);
	TFPASS(idle != nullptr);

	/* start twice: the second must be a no-op */
	idle->start();
	idle->start();
	/* the idle callback returns TRUE, so the source repeats: pump a
	 * bounded number of times rather than until empty */
	for (int i = 0; i < 10 && s_idleCalls == 0; i++)
		g_main_context_iteration(nullptr, TRUE);
	TFPASS(s_idleCalls > 0);

	/* destructor stops a running idle */
	delete idle;

	/* explicit stop/start/stop on a fresh idle */
	idle = new UT_UnixIdle(idle_cb, nullptr);
	idle->start();
	idle->stop();
	idle->stop();
	delete idle;
}

TFTEST_MAIN("UT_UNIXTimer lifecycle")
{
	s_timerCalls = 0;
	UT_UNIXTimer *timer = new UT_UNIXTimer(timer_cb, nullptr);
	TFPASS(timer != nullptr);
	TFPASS(timer->getIdentifier() == 0);

	timer->set(1);
	TFPASS(timer->getIdentifier() != 0);
	UT_uint32 id = timer->getIdentifier();

	/* pump the default main context until it fires */
	for (int i = 0; i < 100 && s_timerCalls == 0; i++)
	{
		g_main_context_iteration(nullptr, TRUE);
	}
	TFPASS(s_timerCalls > 0);

	/* the callback keeps returning TRUE: stop it, then re-set to
	 * exercise the clamp and the existing-source stop */
	timer->stop();
	timer->set(0xFFFFFFFFu);
	TFPASS(timer->getIdentifier() == id);
	timer->stop();
	delete timer;
}

TFTEST_MAIN("_UT_OutputMessage silent mode")
{
	bool was = ut_g_silent;
	ut_g_silent = true;
	_UT_OutputMessage("should be dropped");
	ut_g_silent = was;
	TFPASS(true);
}

TFTEST_MAIN("UT_UnixAssertMsg prompt answers")
{
	/* EOF on stdin: continue */
	TFPASS(call_assert(nullptr) == 1);
	/* 'y' and empty line: continue */
	TFPASS(call_assert("y\n") == 1);
	TFPASS(call_assert("\n") == 1);
	/* 'i': ignore, returns -1 */
	TFPASS(call_assert("i\n") == -1);
	/* junk input re-prompts, then 'i' */
	TFPASS(call_assert("?\ni\n") == -1);
}

TFTEST_MAIN("UT_UnixAssertMsg abort answer")
{
	/* 'n' calls abort(); catch SIGABRT and jump back.  stdio is
	 * redirected explicitly (not via the RAII guard) so the longjmp
	 * cannot strand it */
	struct sigaction sa, old;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = abort_catcher;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGABRT, &sa, &old);

	fflush(stdout);
	int savedIn = dup(STDIN_FILENO);
	int savedOut = dup(STDOUT_FILENO);
	FILE *tmp = tmpfile();
	fputs("n\n", tmp);
	rewind(tmp);
	dup2(fileno(tmp), STDIN_FILENO);
	clearerr(stdin);
	int devnull = open("/dev/null", O_WRONLY);
	dup2(devnull, STDOUT_FILENO);
	close(devnull);

	if (sigsetjmp(s_abortJmp, 1) == 0)
	{
		s_abortArmed = 1;
		UT_UnixAssertMsg("abort", __FILE__, __LINE__);
		TFPASS(false); /* abort() did not fire */
	}
	s_abortArmed = 0;

	fflush(stdout);
	dup2(savedOut, STDOUT_FILENO);
	close(savedOut);
	dup2(savedIn, STDIN_FILENO);
	close(savedIn);
	fclose(tmp);
	sigaction(SIGABRT, &old, nullptr);
	TFPASS(true);
}

TFTEST_MAIN("UT_UnixAssertMsg break answer")
{
	/* 'b' sends SIGTRAP to the process group; run it in a child
	 * on its own group, then answer 'i' to leave the loop */
	int rc = run_assert_in_child("b\ni\n");
	TFPASS(rc == 0);
}
