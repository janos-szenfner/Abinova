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

#include <string.h>

#include "tf_test.h"
#include "ut_Encoding.h"
#include "ut_AdobeEncoding.h"

#define TFSUITE "core.af.util.encoding"

TFTEST_MAIN("UT_Encoding table")
{
	// the ctor probes the system iconv once and prunes the master
	// table to the encodings actually supported; needs the app
	// string set, which the test harness initialises
	UT_Encoding enc;
	UT_uint32 count = enc.getCount();
	TFPASS(count > 0);

	// every entry has a non-empty primary name and a description
	for (UT_uint32 i = 0; i < count; i++)
	{
		TFPASS(enc.getNthEncoding(i) != nullptr);
		TFPASS(strlen(enc.getNthEncoding(i)) > 0);
		TFPASS(enc.getNthDescription(i) != nullptr);
	}

	// UTF-8 is always available via iconv on this platform and must
	// be in the table
	UT_uint32 utf8 = enc.getIndxFromEncoding("UTF-8");
	TFPASS(utf8 < count);
	TFPASS(strcmp(enc.getNthEncoding(utf8), "UTF-8") == 0);

	// quirk: lookup matches only the PRIMARY name (encs[0]) and
	// returns 0 - a perfectly valid index - when there is no match
	TFPASS(enc.getIndxFromEncoding("no-such-encoding-xyz") == 0);

	// getIdFromEncoding returns 0 for unknown encodings.  NB: it
	// binary-searches a table that is only approximately ordered by
	// primary name, so positive lookups are not pinned here.
	TFPASS(enc.getIdFromEncoding("no-such-encoding-xyz") == 0);

	// description -> name lookup is exact-match; misses give nullptr
	TFPASS(enc.getEncodingFromDescription("no such description") == nullptr);
}

TFTEST_MAIN("UT_AdobeEncoding lookup")
{
	// small caller-supplied LUT, sorted by adb name (adobeToUcs
	// binary-searches it)
	static const encoding_pair lut[] = {
		{"Alpha", 0x0391},
		{"Beta",  0x0392},
		{"space", 0x0020},
	};
	UT_AdobeEncoding ae(lut, G_N_ELEMENTS(lut));

	// table hit / miss
	TFPASS(ae.adobeToUcs("space") == 0x20);
	TFPASS(ae.adobeToUcs("Alpha") == 0x0391);
	TFPASS(ae.adobeToUcs("not-in-table") == 0);

	// "uniXXXX" names bypass the table entirely (4 hex digits)
	TFPASS(ae.adobeToUcs("uni03B1") == 0x03B1);
	TFPASS(ae.adobeToUcs("uni0041") == 'A');

	// reverse lookup: hit returns the adb name; miss produces a
	// synthesized "uniXXXX" name in an internal buffer
	TFPASS(strcmp(ae.ucsToAdobe(0x20), "space") == 0);
	TFPASS(strcmp(ae.ucsToAdobe(0x0391), "Alpha") == 0);
	TFPASS(strcmp(ae.ucsToAdobe(0x03B1), "uni03b1") == 0);
	TFPASS(strcmp(ae.ucsToAdobe(0x1F600), "uni1f600") == 0);
}
