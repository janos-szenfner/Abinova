/* AbiSource Program Utilities
 * Copyright (C) 2003-2004 Tomas Frydrych <tomasfrydrych@yahoo.co.uk>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ut_misc.h"
#include "ut_assert.h"
#include <sys/time.h>

/*!
    UT_gettimeofday() fills in the timeval structure with current
    time; the platform implementation needs to be as accurate as
    possible since this function is used in the UT_UUID class.
 */
void UT_gettimeofday(struct timeval *tv)
{
	gettimeofday(tv,nullptr);
}

/*!
    retrieve the 6-byte address of the network card; returns true on success
    This implementation is from libuuid; it has not been debugged or
    even compiled
*/
bool UT_getEthernetAddress(UT_EthernetAddress &a)
{
	UT_UNUSED(a);
	// someone needs to debug/fix, ect., the code above
	UT_ASSERT(UT_NOT_IMPLEMENTED);
    return false;
}

