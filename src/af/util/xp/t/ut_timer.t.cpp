/* AbiSource Program Utilities
 * Copyright (C) 2026 Abinova developers
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

#include "tf_test.h"
#include "ut_timer.h"

#define TFSUITE "core.af.util.timer"

namespace
{

// minimal concrete UT_Timer: records set/stop/start calls, never fires
class FakeTimer final : public UT_Timer
{
public:
	FakeTimer() : m_setCalls(0), m_stopped(0), m_started(0) {}

	UT_sint32 set(UT_uint32) override { m_setCalls++; return 0; }
	void stop() override { m_stopped++; }
	void start() override { m_started++; }

	int m_setCalls;
	int m_stopped;
	int m_started;
};

static void noop(UT_Worker *) {}

} // namespace

TFTEST_MAIN("UT_Timer base class and registry")
{
	FakeTimer * t = new FakeTimer;
	t->set(10);
	TFPASS(t->m_setCalls == 1);
	t->stop();
	t->start();
	TFPASS(t->m_stopped == 1 && t->m_started == 1);

	// callback / instance data round trip
	t->setCallback(noop);
	TFPASS(t->getCallback() == noop);
	int data = 7;
	t->setInstanceData(&data);
	TFPASS(t->getInstanceData() == &data);

	// identifiers are looked up through the static registry
	t->setIdentifier(0xABCD);
	TFPASS(UT_Timer::findTimer(0xABCD) == t);
	TFPASS(UT_Timer::findTimer(0xDEAD) == nullptr);

	// destruction removes the timer from the registry
	delete t;
	TFPASS(UT_Timer::findTimer(0xABCD) == nullptr);
}
