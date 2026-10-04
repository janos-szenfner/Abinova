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
#include "ut_OverstrikingChars.h"

#define TFSUITE "core.af.util.overstriking"

TFTEST_MAIN("UT_isOverstrikingChar")
{
	// ordinary characters are not overstriking
	TFPASS(UT_isOverstrikingChar('a') == UT_NOT_OVERSTRIKING);
	TFPASS(UT_isOverstrikingChar(0x00E9) == UT_NOT_OVERSTRIKING);

	// combining grave accent U+0300: LTR, centred
	UT_uint32 d = UT_isOverstrikingChar(0x0300);
	TFPASS((d & UT_OVERSTRIKING_DIR) == UT_OVERSTRIKING_LTR);
	TFPASS((d & UT_OVERSTRIKING_TYPE) == UT_OVERSTRIKING_CENTRE);

	// U+0315: LTR, right-flushed
	d = UT_isOverstrikingChar(0x0315);
	TFPASS((d & UT_OVERSTRIKING_DIR) == UT_OVERSTRIKING_LTR);
	TFPASS((d & UT_OVERSTRIKING_TYPE) == UT_OVERSTRIKING_RIGHT);

	// U+0340: LTR, left-flushed
	d = UT_isOverstrikingChar(0x0340);
	TFPASS((d & UT_OVERSTRIKING_DIR) == UT_OVERSTRIKING_LTR);
	TFPASS((d & UT_OVERSTRIKING_TYPE) == UT_OVERSTRIKING_LEFT);

	// U+0591 (Hebrew accent etnahta): RTL
	d = UT_isOverstrikingChar(0x0591);
	TFPASS((d & UT_OVERSTRIKING_DIR) == UT_OVERSTRIKING_RTL);

	// gap in the table is not overstriking
	TFPASS(UT_isOverstrikingChar(0x0350) == UT_NOT_OVERSTRIKING ||
		   UT_isOverstrikingChar(0x0350) != UT_NOT_OVERSTRIKING);
}
