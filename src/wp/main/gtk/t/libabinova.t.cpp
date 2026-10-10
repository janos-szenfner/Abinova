/* AbiSource Applications
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

/* coverage for the libabinova embedding entry points
 * (wp/main/gtk/libabinova.cpp).  Each call spins up a second
 * AP_UnixApp singleton, so it runs in a forked child: the harness's
 * own XAP_App stays untouched and the child's gcov counters are
 * dumped before _exit(). */

#include "tf_test.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../libabinova.h"

extern "C" void __gcov_dump(void);

#define TFSUITE "core.wp.main"

namespace {

/* returns the child's exit status, or -1 on signal/error */
int run_in_child(void (*fn)(void))
{
	pid_t pid = fork();
	if (pid == 0)
	{
		fn();
		__gcov_dump();
		_exit(0);
	}
	int status = 0;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

void child_init_noargs(void)
{
	libabinova_init_noargs();
	libabinova_shutdown();
}

void child_init_args(void)
{
	char arg0[] = "libabinova-test";
	char *argv[] = { arg0, nullptr };
	libabinova_init(1, argv);
	libabinova_shutdown();
}

} // namespace

TFTEST_MAIN("libabinova_init_noargs + shutdown")
{
	TFPASS(run_in_child(child_init_noargs) == 0);
}

TFTEST_MAIN("libabinova_init + shutdown")
{
	TFPASS(run_in_child(child_init_args) == 0);
}
