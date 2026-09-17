/* AbiSource Program Utilities
 * Copyright (C) 2001 Dom Lachowicz <dominicl@seas.upenn.edu>
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
#include "ut_worker.h"

class ABI_EXPORT UT_Idle : public UT_Worker
{

 public:
  virtual ~UT_Idle ();

  virtual void stop(void) override = 0;            /* suspend events */
  virtual void start(void) override = 0;           /* resume events */

  static UT_Idle * static_constructor (UT_WorkerCallback pCallback, void* pData); /* implemented in platform-specific code */

 protected:

  UT_Idle ();
  UT_Idle ( UT_WorkerCallback cb, void * data );

 private:

};
