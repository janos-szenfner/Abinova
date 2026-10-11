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

#pragma once

#include "ut_timer.h"
#include "ut_worker.h"

/*!
 * Shared owner for the two-stage autoscroll machinery used while
 * dragging text, images or frames, or extending a selection, past
 * the edge of the view window.
 *
 * FV_VisualDragText, FV_VisualInlineImage, FV_FrameEdit and FV_View
 * each ran the same scheme off four private copies of the state:
 * an arming UT_Timer whose tick (the owner's static _autoScroll)
 * spawns a repeating UT_Worker (the owner's _actuallyScroll) which
 * performs the per-class scroll step.  This helper owns the
 * timer/worker pair and the acceleration counter so the lifecycle
 * rules live in exactly one place; the divergent parts — the
 * scroll math, the teardown gates and when to arm — stay in the
 * owning classes.
 *
 * Lifetime rules baked in here:
 *  - the worker carries owner state as instance data and must never
 *    fire on a dead owner or after its drag ends, so every owner
 *    calls stopAll() from its destructor and its drag-end paths;
 *  - the worker can outlive the arming timer by a main-loop turn, so
 *    owners reap it from inside its own fire() via stopWorker():
 *    stop() runs before the delete so a TIMER-mode worker
 *    unschedules its GLib source first (its _Timer_Proc returns TRUE
 *    and would otherwise reschedule onto a dead object);
 *  - the acceleration counter resets on every spawn and every
 *    stopAll(), so a later drag can never inherit it.
 */
class ABI_EXPORT FV_AutoScroll
{
public:
	FV_AutoScroll() = default;
	~FV_AutoScroll() { stopAll(); }
	FV_AutoScroll(const FV_AutoScroll &) = delete;
	FV_AutoScroll & operator=(const FV_AutoScroll &) = delete;

	/* ---- arming timer: its tick invokes the owner's static
	 * _autoScroll, which decides whether to spawn the worker ---- */

	/* create and schedule the arming timer; no-op when already armed */
	void arm(UT_WorkerCallback cb, void * inst, UT_uint32 msecs)
	{
		if (!m_pTimer)
		{
			m_pTimer = UT_Timer::static_constructor(cb, inst);
			if (m_pTimer)
				m_pTimer->set(msecs);
		}
	}
	/* suspend without deleting; rearmTimer() resumes it with the
	 * period it was armed with */
	void pauseTimer(void) { if (m_pTimer) m_pTimer->stop(); }
	void rearmTimer(void) { if (m_pTimer) m_pTimer->start(); }
	/* run one tick synchronously (endDrag's final scroll step) */
	void fireTimer(void)  { if (m_pTimer) m_pTimer->fire(); }
	void killTimer(void)
	{
		if (m_pTimer)
		{
			m_pTimer->stop();
			delete m_pTimer;
			m_pTimer = nullptr;
		}
	}
	bool armed(void) const { return m_pTimer != nullptr; }

	/* ---- repeating scroll worker ---- */

	/* spawn the worker (UT_Idle preferred, UT_Timer fallback at
	 * msecs); returns false — without touching the existing worker —
	 * when one is already running, so the caller can apply its
	 * still-running policy (bump acceleration, drop the tick, ...) */
	bool spawnWorker(UT_WorkerCallback cb, void * inst, UT_uint32 msecs)
	{
		if (m_pWorker)
			return false;
		int inMode = UT_WorkerFactory::IDLE | UT_WorkerFactory::TIMER;
		UT_WorkerFactory::ConstructMode outMode = UT_WorkerFactory::NONE;
		m_pWorker = UT_WorkerFactory::static_constructor(cb, inst,
													   inMode, outMode);
		if (!m_pWorker)
			return false;
		// If the worker is working on a timer instead of in the idle
		// time, set the frequency of the checks.
		if (UT_WorkerFactory::TIMER == outMode)
		{
			// this is really a timer, so it's safe to static_cast it
			static_cast<UT_Timer*>(m_pWorker)->set(msecs);
		}
		m_iScrollExtra = 0;
		m_pWorker->start();
		return true;
	}
	void stopWorker(void)
	{
		if (m_pWorker)
		{
			m_pWorker->stop();
			delete m_pWorker;
			m_pWorker = nullptr;
		}
	}
	bool workerRunning(void) const { return m_pWorker != nullptr; }

	/* ---- extra scroll distance accrued by the +step-per-arm-tick
	 * acceleration while a worker is already running ---- */
	UT_sint32 accel(void) const { return m_iScrollExtra; }
	void bumpAccel(UT_sint32 step, UT_sint32 cap)
	{ if (m_iScrollExtra < cap) m_iScrollExtra += step; }
	void resetAccel(void) { m_iScrollExtra = 0; }

	bool active(void) const { return armed() || workerRunning(); }

	/* kill every autoscroll source this helper owns: the arming
	 * timer, the repeating worker and the accrued acceleration */
	void stopAll(void)
	{
		killTimer();
		stopWorker();
		resetAccel();
	}

private:
	UT_Timer *  m_pTimer = nullptr;
	UT_Worker * m_pWorker = nullptr;
	UT_sint32   m_iScrollExtra = 0;
};
