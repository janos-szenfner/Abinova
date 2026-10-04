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
#include <string>
#include <vector>

#include "tf_test.h"
#include "ut_base64.h"

#define TFSUITE "core.af.util.base64"

namespace
{

static UT_ByteBufPtr bbOf(const void * data, UT_uint32 len)
{
	UT_ByteBufPtr bb(new UT_ByteBuf);
	bb->append(static_cast<const UT_Byte *>(data), len);
	return bb;
}

static std::string strOf(const UT_ConstByteBufPtr & bb)
{
	return std::string(reinterpret_cast<const char *>(bb->getPointer(0)),
					 bb->getLength());
}

// stream form: encode `in` into a caller buffer sized exactly right
static std::string utf8Encode(const std::string & in)
{
	std::vector<char> out(((in.size() + 2) / 3) * 4);
	char * b64ptr = out.data();
	size_t b64len = out.size();
	const char * binptr = in.data();
	size_t binlen = in.size();
	if (!UT_UTF8_Base64Encode(b64ptr, b64len, binptr, binlen))
		return "(encode failed)";
	return std::string(out.data(), out.size() - b64len);
}

static bool utf8Decode(const std::string & in, std::string & out)
{
	std::vector<char> buf(in.size()); // decoded <= encoded
	char * binptr = buf.data();
	size_t binlen = buf.size();
	const char * b64ptr = in.data();
	size_t b64len = in.size();
	if (!UT_UTF8_Base64Decode(binptr, binlen, b64ptr, b64len))
		return false;
	out.assign(buf.data(), buf.size() - binlen);
	return true;
}

} // namespace

TFTEST_MAIN("UT_Base64 encode/decode round trip")
{
	// RFC 4648 test vectors
	static const struct { const char * plain; const char * coded; } vec[] = {
		{"",       ""},
		{"f",      "Zg=="},
		{"fo",     "Zm8="},
		{"foo",    "Zm9v"},
		{"foob",   "Zm9vYg=="},
		{"fooba",  "Zm9vYmE="},
		{"foobar", "Zm9vYmFy"},
	};

	for (const auto & v : vec)
	{
		UT_ByteBufPtr enc(new UT_ByteBuf);
		TFPASS(UT_Base64Encode(enc, bbOf(v.plain,
				static_cast<UT_uint32>(strlen(v.plain)))));
		TFPASS(strOf(enc) == v.coded);

		UT_ByteBufPtr dec(new UT_ByteBuf);
		TFPASS(UT_Base64Decode(dec, bbOf(v.coded,
				static_cast<UT_uint32>(strlen(v.coded)))));
		TFPASS(strOf(dec) == v.plain);
	}
}

TFTEST_MAIN("UT_Base64 binary data round trip")
{
	// all 256 byte values survive a round trip
	unsigned char all[256];
	for (int i = 0; i < 256; i++) all[i] = static_cast<unsigned char>(i);

	UT_ByteBufPtr enc(new UT_ByteBuf);
	TFPASS(UT_Base64Encode(enc, bbOf(all, 256)));
	TFPASS(enc->getLength() == 344); // 256/3 -> 86 quads, 344 chars

	UT_ByteBufPtr dec(new UT_ByteBuf);
	TFPASS(UT_Base64Decode(dec, enc));
	TFPASS(dec->getLength() == 256);
	TFPASS(memcmp(dec->getPointer(0), all, 256) == 0);

	// encoding truncates the destination first (no append)
	UT_ByteBufPtr reuse = bbOf("old contents", 12);
	TFPASS(UT_Base64Encode(reuse, bbOf("x", 1)));
	TFPASS(strOf(reuse) == "eA==");
}

TFTEST_MAIN("UT_UTF8_Base64Encode/Decode")
{
	TFPASS(utf8Encode("f") == "Zg==");
	TFPASS(utf8Encode("fo") == "Zm8=");
	TFPASS(utf8Encode("foo") == "Zm9v");
	TFPASS(utf8Encode("foobar") == "Zm9vYmFy");
	TFPASS(utf8Encode("") == "");

	std::string out;
	TFPASS(utf8Decode("Zg==", out) && out == "f");
	TFPASS(utf8Decode("Zm8=", out) && out == "fo");
	TFPASS(utf8Decode("Zm9v", out) && out == "foo");
	TFPASS(utf8Decode("Zm9vYmFy", out) && out == "foobar");

	// whitespace inside the encoded stream is skipped
	TFPASS(utf8Decode("Zm9v\nYmFy", out) && out == "foobar");

	// invalid characters reject
	TFPASS(!utf8Decode("Zm9v$", out));
	// text after padding rejects
	TFPASS(!utf8Decode("Zg==Zg==", out));
	// lone padding rejects
	TFPASS(!utf8Decode("=", out));
}

TFTEST_MAIN("UT_UTF8_Base64Encode tight buffer")
{
	// a destination buffer that is too small fails instead of overrun
	char small[4];
	char * b64ptr = small;
	size_t b64len = 2;
	const char * binptr = "abcdef";
	size_t binlen = 6;
	TFPASS(!UT_UTF8_Base64Encode(b64ptr, b64len, binptr, binlen));

	// decode into a too-small buffer fails on the second quad
	char tiny[3];
	char * op = tiny;
	size_t olen = 3;
	const char * ip = "Zm9vYmFy";
	size_t ilen = 8;
	TFPASS(!UT_UTF8_Base64Decode(op, olen, ip, ilen));
}
