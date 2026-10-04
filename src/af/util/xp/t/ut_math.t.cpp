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
#include "ut_math.h"

#define TFSUITE "core.af.util.math"

TFTEST_MAIN("UT_newNumber")
{
	// strictly increasing, unique values
	UT_uint32 a = UT_newNumber();
	UT_uint32 b = UT_newNumber();
	UT_uint32 c = UT_newNumber();
	TFPASS(a != b && b != c && a != c);
	TFPASS(b == a + 1 && c == b + 1);
}
