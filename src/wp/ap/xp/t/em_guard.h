/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
/* Abinova
 * Copyright (C) 2026 Abinova contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

/*
 * In-process crash/hang guard for the ap/xp coverage sweeps.
 *
 * A watchdog thread arms a wall-clock deadline before each guarded
 * call; SIGSEGV/SIGBUS/SIGILL/SIGFPE/SIGABRT plus the watchdog's
 * SIGUSR2 all siglongjmp back to the dispatch site, so a crashing
 * or hanging callee is classified and the sweep continues.
 *
 * This runs in-process rather than in a forked child on purpose:
 * anything that loads a *new* font (fmt-mark runs, char-format
 * queries) deadlocks deterministically inside pangoft2's
 * g_cond_wait after fork — in-process the same code runs fine.
 */

#ifndef EM_GUARD_H
#define EM_GUARD_H

#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <setjmp.h>
#include <time.h>
#include <cstring>

#include <glib.h>

namespace em_guard {

enum Outcome { OK, FAULT, HUNG };

/* deadline in ms (CLOCK_MONOTONIC), 0 = disarmed */
static volatile sig_atomic_t s_deadline = 0;
static volatile sig_atomic_t s_fired = 0;
static volatile sig_atomic_t s_armed = 0;
static sigjmp_buf s_jmp;
static pthread_t s_main_thread;

static long long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

static void arm(long ms)
{
	s_deadline = static_cast<sig_atomic_t>(now_ms() + ms);
}

static void disarm(void)
{
	s_deadline = 0;
}

static void sighandler(int)
{
	if (s_armed)
		siglongjmp(s_jmp, 1);
	_exit(128);
}

static void *watchdog_main(void *)
{
	const struct timespec req = { 0, 4000 * 1000 }; /* 4ms poll */
	for (;;)
	{
		sig_atomic_t dl = s_deadline;
		if (dl && now_ms() >= dl)
		{
			disarm();
			s_fired = 1;
			pthread_kill(s_main_thread, SIGUSR2);
		}
		nanosleep(&req, nullptr);
	}
	return nullptr;
}

static void install(void)
{
	static bool s_installed = false;
	if (s_installed)
		return;
	s_installed = true;

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sighandler;
	sigemptyset(&sa.sa_mask);
	const int sigs[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT,
						 SIGUSR2 };
	for (int s : sigs)
		sigaction(s, &sa, nullptr);

	s_main_thread = pthread_self();
	pthread_t wd;
	pthread_create(&wd, nullptr, watchdog_main, nullptr);
	pthread_detach(wd);
}

/* run fn(ctx) guarded; ms is the wall-clock budget */
static Outcome call(void (*fn)(void *), void *ctx, long ms)
{
	Outcome out = OK;
	s_fired = 0;
	s_armed = 1;
	if (sigsetjmp(s_jmp, 1) == 0)
	{
		arm(ms);
		fn(ctx);
		out = OK;
	}
	else
	{
		out = s_fired ? HUNG : FAULT;
	}
	disarm();
	s_armed = 0;
	return out;
}

/* pump the default main context so deferred UT_Worker dispatches
 * (idle or ~50ms timer) actually run — bounded to ~200ms total. */
static void pump(void)
{
	GMainContext *c = g_main_context_default();
	for (int i = 0; i < 20; ++i)
	{
		if (!g_main_context_pending(c))
		{
			g_usleep(10000);
			continue;
		}
		g_main_context_iteration(c, FALSE);
	}
}

} /* namespace em_guard */

#endif /* EM_GUARD_H */
