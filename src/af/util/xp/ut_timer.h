/* AbiSource Program Utilities
 * Copyright (C) 1998 AbiSource, Inc.
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

#include "ut_types.h"
#include "ut_vector.h"
#include "ut_worker.h"

class GR_Graphics;

// simple backwards-compatible definition
typedef UT_WorkerCallback UT_TimerCallback;

/*
	UT_Timer is an abstract class which encapsulates the platform-specific
	details for managing timers.
*/
class ABI_EXPORT UT_Timer : public UT_Worker
{
public:
	virtual ~UT_Timer();

	virtual void setCallback(UT_WorkerCallback p);
	void setInstanceData(void*);

	virtual UT_sint32 set(UT_uint32 iMilliseconds) = 0;	/* set freq and start */
	virtual void stop(void) override = 0;		/* suspend events */
	virtual void start(void) override = 0;		/* resume events */

	void setIdentifier(UT_uint32);
	UT_uint32 getIdentifier() const;

	static UT_Timer* findTimer(UT_uint32 iIdentifier);

	/*
		Note that the static_constructor is actually implemented in
		*platform* code, so that it can instantiate the appropriate
		platform-specific subclass.
	*/
	static UT_Timer* static_constructor(UT_WorkerCallback pCallback, void* pData);

protected:
	UT_Timer();		// should only be called from static_constructor()
	static UT_GenericVector<UT_Timer*> & _getVecTimers ();

 private:
	UT_uint32 m_iIdentifier;
};
