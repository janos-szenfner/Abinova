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
 * TST01: shared test-harness primitives — the in-process crash/hang
 * watchdog, main-loop pumps and the idle sentinel, usable by any
 * .t.cpp suite (and by the ui-drive process via the same header).
 *
 * A watchdog thread arms a wall-clock deadline before each guarded
 * call; SIGSEGV/SIGBUS/SIGILL/SIGFPE/SIGABRT plus the watchdog's
 * SIGUSR2 all siglongjmp back to the dispatch site, so a crashing
 * or hanging callee is classified and the sweep continues — a wedge
 * becomes a test failure instead of a frozen run.
 *
 * This runs in-process rather than in a forked child on purpose:
 * anything that loads a *new* font (fmt-mark runs, char-format
 * queries) deadlocks deterministically inside pangoft2's
 * g_cond_wait after fork — in-process the same code runs fine.
 *
 * Main-loop helpers:
 * - pump()           settle deferred (~50ms timer/idle) dispatches
 * - drain_pending()  run only what is already pending, bounded
 * - pump_for(ms)     keep the context serviced for a wall-clock window
 * - idle_sentinel()  post a g_idle probe and require it to dispatch
 *                    within a bound — proves the loop is responsive
 * - responsive_after() watchdog + sentinel combined: the action ran
 *                    AND the main loop still services events after it
 */

#ifndef TF_GUARD_H
#define TF_GUARD_H

#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <setjmp.h>
#include <time.h>
#include <cstring>
#include <functional>

#include <glib.h>

namespace tf_guard {

enum Outcome { OK, FAULT, HUNG };

/* deadline in ms (CLOCK_MONOTONIC), 0 = disarmed */
static volatile sig_atomic_t s_deadline = 0;
static volatile sig_atomic_t s_fired = 0;
static volatile sig_atomic_t s_armed = 0;
static sigjmp_buf s_jmp;
static pthread_t s_main_thread;

static inline long long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

static inline void arm(long ms)
{
	s_deadline = static_cast<sig_atomic_t>(now_ms() + ms);
}

static inline void disarm(void)
{
	s_deadline = 0;
}

static inline void sighandler(int)
{
	if (s_armed)
		siglongjmp(s_jmp, 1);
	_exit(128);
}

static inline void *watchdog_main(void *)
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

static inline void install(void)
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

/* WATCHDOG: run fn(ctx) guarded; ms is the wall-clock budget.  A
 * callee that outlives it is killed with SIGUSR2 and reported HUNG;
 * a fatal signal inside is reported FAULT. */
static inline Outcome call(void (*fn)(void *), void *ctx, long ms)
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

/* std::function flavour so lambdas with captures can be guarded */
static inline void s_call_thunk(void *d)
{
	(*static_cast<const std::function<void(void)> *>(d))();
}

static inline Outcome call(const std::function<void(void)> &fn, long ms)
{
	return call(s_call_thunk, const_cast<std::function<void(void)> *>(&fn), ms);
}

/* pump the default main context so deferred UT_Worker dispatches
 * (idle or ~50ms timer) actually run — bounded to ~200ms total. */
static inline void pump(void)
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

/* drain only the work that is already pending — bound both ways:
 * a self-reposting source (idle that reschedules itself, a chatty
 * dconf watch, an animation's frame clock) keeps pending() true
 * forever, and each iteration can cost real layout/draw work, so
 * cap iterations and wall time. */
static inline void drain_pending(void)
{
	gint64 deadline = g_get_monotonic_time() + 40000;
	for (int i = 0; i < 300 && g_main_context_pending(nullptr); i++) {
		g_main_context_iteration(nullptr, FALSE);
		if (g_get_monotonic_time() >= deadline)
			break;
	}
}

/* keep the default main context serviced for at most `ms` so async
 * map/show handlers get a slice */
static inline void pump_for(guint ms)
{
	GMainContext *ctx = g_main_context_default();
	gint64 deadline = g_get_monotonic_time() + ms * 1000;
	while (g_get_monotonic_time() < deadline) {
		if (!g_main_context_iteration(ctx, FALSE))
			g_usleep(2000);
	}
}

struct SentinelCtx
{
	volatile bool fired;
};

static inline gboolean s_sentinel_cb(gpointer d)
{
	static_cast<SentinelCtx *>(d)->fired = true;
	return G_SOURCE_REMOVE;
}

/* RESPONSIVE-AFTER probe: post an idle callback on the default main
 * context and require it to dispatch within `ms`.  Returns elapsed
 * ms, or -1 if the loop never got to it — a wedged loop (a stranded
 * lock, a self-reposting higher-priority source) shows up as the
 * timeout.  On timeout the pending source is removed so nothing
 * outlives the stack frame. */
static inline long idle_sentinel(long ms)
{
	SentinelCtx ctx { false };
	guint src = g_idle_add(s_sentinel_cb, &ctx);
	gint64 start = g_get_monotonic_time();
	gint64 deadline = start + static_cast<gint64>(ms) * 1000;
	GMainContext *c = g_main_context_default();
	while (!ctx.fired)
	{
		if (g_get_monotonic_time() >= deadline)
		{
			g_source_remove(src);
			return -1;
		}
		g_main_context_iteration(c, FALSE);
		if (!ctx.fired)
			g_usleep(500);
	}
	return static_cast<long>((g_get_monotonic_time() - start) / 1000);
}

/* run fn(ctx) under the watchdog, then post an idle sentinel and
 * require the loop to service it inside the same wall-clock budget
 * — the combined "the action ran AND the main loop is responsive
 * after it" check for UI actions. */
static inline bool responsive_after(void (*fn)(void *), void *ctx,
							 long budget_ms)
{
	gint64 start = g_get_monotonic_time();
	if (call(fn, ctx, budget_ms) != OK)
		return false;
	long remaining = budget_ms -
		static_cast<long>((g_get_monotonic_time() - start) / 1000);
	if (remaining < 1)
		remaining = 1;
	return idle_sentinel(remaining) >= 0;
}

static inline bool responsive_after(const std::function<void(void)> &fn,
							 long budget_ms)
{
	return responsive_after(s_call_thunk,
							const_cast<std::function<void(void)> *>(&fn),
							budget_ms);
}

} /* namespace tf_guard */

#endif /* TF_GUARD_H */
