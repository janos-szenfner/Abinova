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
#include "ut_iconv.h"

#define TFSUITE "core.af.util.iconv"

TFTEST_MAIN("UT_iconv_open/close/isValid")
{
	UT_iconv_t cd = UT_iconv_open("UTF-8", "ISO-8859-1");
	TFPASS(UT_iconv_isValid(cd));

	// convert e9 -> é (2-byte UTF-8)
	const char * in = "\xe9";
	size_t inleft = 1;
	char outbuf[8] = {0};
	char * outp = outbuf;
	size_t outleft = sizeof(outbuf);
	size_t n = UT_iconv(cd, &in, &inleft, &outp, &outleft);
	TFPASS(n != static_cast<size_t>(-1));
	TFPASS(inleft == 0);
	TFPASS(static_cast<unsigned char>(outbuf[0]) == 0xC3 &&
		   static_cast<unsigned char>(outbuf[1]) == 0xA9);

	UT_iconv_reset(cd);
	TFPASS(UT_iconv_close(cd) == 0);

	// invalid descriptor handling
	TFPASS(!UT_iconv_isValid(UT_ICONV_INVALID));
	TFPASS(UT_iconv_open(nullptr, "UTF-8") == UT_ICONV_INVALID);
	TFPASS(UT_iconv_open("UTF-8", nullptr) == UT_ICONV_INVALID);
	TFPASS(UT_iconv_close(UT_ICONV_INVALID) == -1);
	const char * dummy = "x";
	size_t dl = 1, do_ = 8;
	char * dp = outbuf;
	TFPASS(UT_iconv(UT_ICONV_INVALID, &dummy, &dl, &dp, &do_)
		   == static_cast<size_t>(-1));

	// unknown charset -> invalid handle
	UT_iconv_t bad = UT_iconv_open("UTF-8", "NO-SUCH-CODESET-XYZ");
	TFPASS(!UT_iconv_isValid(bad));
}

TFTEST_MAIN("auto_iconv")
{
	{
		auto_iconv a("ISO-8859-1", "UTF-8");
		TFPASS(UT_iconv_isValid(a.getHandle()));
		TFPASS(static_cast<UT_iconv_t>(a) == a.getHandle());
	}

	// invalid charset pair throws the (invalid) handle
	bool threw = false;
	try
	{
		auto_iconv b("NO-SUCH-CODESET-XYZ", "UTF-8");
	}
	catch (UT_iconv_t)
	{
		threw = true;
	}
	TFPASS(threw);
}

TFTEST_MAIN("UT_convert / UT_convert_cd / internal names")
{
	UT_uint32 rd = 0, wr = 0;
	char * out = UT_convert("\xe9", 1, "ISO-8859-1", "UTF-8", &rd, &wr);
	TFPASS(out != nullptr);
	TFPASS(rd == 1 && wr == 2);
	TFPASS(static_cast<unsigned char>(out[0]) == 0xC3 &&
		   static_cast<unsigned char>(out[1]) == 0xA9);
	g_free(out);

	// no out-params variant
	out = UT_convert("abc", 3, "UTF-8", "UTF-8", nullptr, nullptr);
	TFPASS(out != nullptr && strcmp(out, "abc") == 0);
	g_free(out);

	// invalid conversion returns nullptr
	out = UT_convert("x", 1, "NO-SUCH-CODESET-XYZ", "UTF-8", &rd, &wr);
	TFPASS(out == nullptr);

	// UT_convert_cd reuses an open handle
	UT_iconv_t cd = UT_iconv_open("UTF-8", "ISO-8859-1");
	TFPASS(UT_iconv_isValid(cd));
	out = UT_convert_cd("\xe9", 1, cd, &rd, &wr);
	TFPASS(out != nullptr && wr == 2);
	g_free(out);
	UT_iconv_close(cd);

	// internal charset names are usable
	TFPASS(ucs2Internal() != nullptr);
	TFPASS(ucs4Internal() != nullptr);
	UT_iconv_t c2 = UT_iconv_open(ucs2Internal(), "UTF-8");
	TFPASS(UT_iconv_isValid(c2));
	UT_iconv_close(c2);
	UT_iconv_t c4 = UT_iconv_open(ucs4Internal(), "UTF-8");
	TFPASS(UT_iconv_isValid(c4));
	UT_iconv_close(c4);
}
