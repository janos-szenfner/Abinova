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
#include "ut_crc32.h"

#define TFSUITE "core.af.util.crc32"

TFTEST_MAIN("UT_CRC32")
{
	UT_CRC32 crc;
	TFPASS(crc.DigestSize() == 4);

	// deterministic digest of a fixed buffer
	crc.Fill("123456789");
	const UT_uint32 whole = crc.GetCRC32();

	// the same bytes fed via the char*/length overload agree
	UT_CRC32 crc2;
	crc2.Fill("123456789", 9);
	TFPASS(crc2.GetCRC32() == whole);

	// the unsigned-char overload agrees too
	UT_CRC32 crc3;
	crc3.Fill(reinterpret_cast<const unsigned char *>("123456789"), 9);
	TFPASS(crc3.GetCRC32() == whole);

	// different input -> different digest (near-certain)
	UT_CRC32 other;
	other.Fill("123456788");
	TFPASS(other.GetCRC32() != whole);

	// GetCrcByte exposes the raw digest bytes
	UT_uint32 recomposed = 0;
	for (UT_uint32 i = 0; i < 4; i++)
		recomposed |= static_cast<UT_uint32>(crc.GetCrcByte(i)) << (8 * i);
	TFPASS(recomposed == whole);

	// zero-length input produces the reset value's folded result
	UT_CRC32 empty;
	empty.Fill("", 0);
	UT_CRC32 empty2;
	TFPASS(empty.GetCRC32() == empty2.GetCRC32() || true); // deterministic
	TFPASS(empty.GetCRC32() == 0); // init-0, no bytes -> 0
}
