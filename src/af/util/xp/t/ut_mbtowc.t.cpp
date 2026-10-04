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
#include "ut_mbtowc.h"

#define TFSUITE "core.af.util.mbtowc"

TFTEST_MAIN("UT_UCS4_mbtowc UTF-8")
{
	UT_UCS4_mbtowc conv("UTF-8");
	UT_UCS4Char wc = 0;

	// ASCII goes through in one byte
	TFPASS(conv.mbtowc(wc, 'a') == 1);
	TFPASS(wc == 'a');

	// U+00E9 in UTF-8 is 0xC3 0xA9; the lead byte defers
	TFPASS(conv.mbtowc(wc, '\xC3') == 0);
	TFPASS(conv.mbtowc(wc, '\xA9') == 1);
	TFPASS(wc == 0x00E9);

	// U+20AC (EURO SIGN) is a 3-byte sequence
	TFPASS(conv.mbtowc(wc, '\xE2') == 0);
	TFPASS(conv.mbtowc(wc, '\x82') == 0);
	TFPASS(conv.mbtowc(wc, '\xAC') == 1);
	TFPASS(wc == 0x20AC);

	// U+1F600 is a 4-byte sequence
	TFPASS(conv.mbtowc(wc, '\xF0') == 0);
	TFPASS(conv.mbtowc(wc, '\x9F') == 0);
	TFPASS(conv.mbtowc(wc, '\x98') == 0);
	TFPASS(conv.mbtowc(wc, '\x80') == 1);
	TFPASS(wc == 0x1F600);
}

TFTEST_MAIN("UT_UCS4_mbtowc charset switching and reset")
{
	UT_UCS4_mbtowc conv("UTF-16LE");
	UT_UCS4Char wc = 0;

	// 'A' in UTF-16LE = 0x41 0x00
	TFPASS(conv.mbtowc(wc, '\x41') == 0);
	TFPASS(conv.mbtowc(wc, '\x00') == 1);
	TFPASS(wc == 'A');

	// switch mid-stream clears any pending partial char
	TFPASS(conv.mbtowc(wc, '\xC3') == 0); // half of UTF-16LE char pending? no,
	// actually 0xC3 alone is an incomplete LE unit
	conv.setInCharset("UTF-8");
	conv.initialize();
	TFPASS(conv.mbtowc(wc, 'z') == 1);
	TFPASS(wc == 'z');

	// a garbage charset yields a converter that never completes
	UT_UCS4_mbtowc bad("NO-SUCH-CHARSET");
	TFPASS(bad.mbtowc(wc, 'a') == 0);
	TFPASS(bad.mbtowc(wc, 'b') == 0);
}

TFTEST_MAIN("UT_UCS2_mbtowc")
{
	UT_UCS2_mbtowc conv("UTF-8");
	UT_UCS2Char wc = 0;

	TFPASS(conv.mbtowc(wc, 'B') == 1);
	TFPASS(wc == 'B');

	// two-byte UTF-8
	TFPASS(conv.mbtowc(wc, '\xC3') == 0);
	TFPASS(conv.mbtowc(wc, '\xA9') == 1);
	TFPASS(wc == 0x00E9);

	// three-byte UTF-8
	TFPASS(conv.mbtowc(wc, '\xE2') == 0);
	TFPASS(conv.mbtowc(wc, '\x82') == 0);
	TFPASS(conv.mbtowc(wc, '\xAC') == 1);
	TFPASS(wc == 0x20AC);

	// charset switch on the UCS2 converter as well
	conv.setInCharset("UTF-16LE");
	conv.initialize();
	TFPASS(conv.mbtowc(wc, '\x42') == 0);
	TFPASS(conv.mbtowc(wc, '\x00') == 1);
	TFPASS(wc == 'B');

	// invalid charset -> dead converter
	UT_UCS2_mbtowc bad("DEFINITELY-NOT-A-CHARSET");
	TFPASS(bad.mbtowc(wc, 'q') == 0);
}

TFTEST_MAIN("UT_mbtowc overflow resets")
{
	UT_UCS4_mbtowc conv("UTF-8");
	UT_UCS4Char wc = 0;

	// feed continuation bytes with no lead: the converter accumulates
	// them until the buffer overflows, then resets without a char
	for (size_t i = 0; i <= iMbLenMax; i++)
		TFPASS(conv.mbtowc(wc, '\x80') == 0);
	// still works afterwards
	TFPASS(conv.mbtowc(wc, 'k') == 1);
	TFPASS(wc == 'k');
}
