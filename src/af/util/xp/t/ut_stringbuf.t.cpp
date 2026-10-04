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

#include "tf_test.h"
#include "ut_stringbuf.h"
#include "ut_string_class.h"

#define TFSUITE "core.af.util.stringbuf"

TFTEST_MAIN("UT_UTF8Stringbuf charCode")
{
	TFPASS(UT_UTF8Stringbuf::charCode(nullptr) == 0);
	TFPASS(UT_UTF8Stringbuf::charCode("") == 0);
	TFPASS(UT_UTF8Stringbuf::charCode("a") == 'a');
	TFPASS(UT_UTF8Stringbuf::charCode("\xC3\xA9") == 0x00E9);       // é
	TFPASS(UT_UTF8Stringbuf::charCode("\xE2\x82\xAC") == 0x20AC);  // €
	TFPASS(UT_UTF8Stringbuf::charCode("\xF0\x9F\x98\x80") == 0x1F600);

	// truncated / invalid sequences yield 0
	TFPASS(UT_UTF8Stringbuf::charCode("\xC3") == 0);
	TFPASS(UT_UTF8Stringbuf::charCode("\x80") == 0);   // stray continuation
	TFPASS(UT_UTF8Stringbuf::charCode("\xE2\x82") == 0);
}

TFTEST_MAIN("UT_UTF8Stringbuf assign/append")
{
	UT_UTF8Stringbuf buf;
	TFPASS(buf.empty());
	TFPASS(buf.byteLength() == 0 && buf.utf8Length() == 0);

	buf.assign("ab");
	TFPASS(!buf.empty());
	TFPASS(buf.byteLength() == 2 && buf.utf8Length() == 2);
	TFPASS(strcmp(buf.data(), "ab") == 0);

	// multi-byte chars count once towards utf8Length
	buf.assign("\xC3\xA9\xE2\x82\xAC");
	TFPASS(buf.byteLength() == 5 && buf.utf8Length() == 2);

	// append with explicit length does not need NUL termination
	buf.assign("x");
	buf.append("yzw", 2);
	TFPASS(strcmp(buf.data(), "xyz") == 0);
	TFPASS(buf.utf8Length() == 3);

	// nullptr is a no-op
	buf.append(nullptr, 0);
	TFPASS(strcmp(buf.data(), "xyz") == 0);

	// append of another buffer
	UT_UTF8Stringbuf tail("q");
	buf.append(tail);
	TFPASS(strcmp(buf.data(), "xyzq") == 0);

	// copy construction and assignment
	UT_UTF8Stringbuf copy(buf);
	TFPASS(copy.byteLength() == buf.byteLength());
	TFPASS(strcmp(copy.data(), "xyzq") == 0);
	UT_UTF8Stringbuf other;
	other = buf;
	TFPASS(strcmp(other.data(), "xyzq") == 0);

	// clear
	copy.clear();
	TFPASS(copy.empty());

	// reserve keeps contents intact
	buf.reserve(100);
	TFPASS(strcmp(buf.data(), "xyzq") == 0);
}

TFTEST_MAIN("UT_UTF8Stringbuf appendUCS2/UCS4")
{
	UT_UTF8Stringbuf buf;

	const UT_UCS4Char u4[] = {'a', 0x20AC, 0};
	buf.appendUCS4(u4, 0);
	TFPASS(buf.byteLength() == 4 && buf.utf8Length() == 2);
	TFPASS(strcmp(buf.data(), "a\xE2\x82\xAC") == 0);

	buf.clear();
	const UT_UCS2Char u2[] = {'b', 0x00E9, 0};
	buf.appendUCS2(u2, 2);
	TFPASS(strcmp(buf.data(), "b\xC3\xA9") == 0);
	TFPASS(buf.utf8Length() == 2);

	// invalid UCS-4 code points are skipped
	buf.clear();
	const UT_UCS4Char bad[] = {0xFFFFFFFFu, 'x', 0};
	buf.appendUCS4(bad, 0);
	TFPASS(strcmp(buf.data(), "x") == 0);
}

TFTEST_MAIN("UT_UTF8Stringbuf escape (search/replace)")
{
	UT_UTF8Stringbuf buf("a-b-c");

	// replace with a longer string (grow path)
	buf.escape(UT_UTF8String("-"), UT_UTF8String("+X+"));
	TFPASS(strcmp(buf.data(), "a+X+b+X+c") == 0);

	// replace with a shorter string (shrink path)
	buf.escape(UT_UTF8String("+X+"), UT_UTF8String("_"));
	TFPASS(strcmp(buf.data(), "a_b_c") == 0);

	// no match leaves the string alone
	buf.escape(UT_UTF8String("zzz"), UT_UTF8String("q"));
	TFPASS(strcmp(buf.data(), "a_b_c") == 0);

	// equal-length replacement
	buf.escape(UT_UTF8String("_"), UT_UTF8String("|"));
	TFPASS(strcmp(buf.data(), "a|b|c") == 0);
}

TFTEST_MAIN("UT_UTF8Stringbuf escapeXML/decodeXML")
{
	UT_UTF8Stringbuf buf("<a href=\"x\">&</a>");
	buf.escapeXML();
	TFPASS(strcmp(buf.data(),
				  "&lt;a href=&quot;x&quot;&gt;&amp;&lt;/a&gt;") == 0);

	buf.decodeXML();
	TFPASS(strcmp(buf.data(), "<a href=\"x\">&</a>") == 0);

	// an ampersand not starting an entity is copied through
	UT_UTF8Stringbuf raw("a & b &zz;");
	raw.decodeXML();
	TFPASS(strcmp(raw.data(), "a & b &zz;") == 0);

	// empty buffer: both are no-ops
	UT_UTF8Stringbuf empty;
	empty.escapeXML();
	empty.decodeXML();
	TFPASS(empty.empty());
}

TFTEST_MAIN("UT_UTF8Stringbuf escapeMIME")
{
	UT_UTF8Stringbuf buf("a=b\xC3\xA9");
	buf.escapeMIME();
	// '=' -> =3D, utf8 bytes -> =XX each
	TFPASS(strstr(buf.data(), "=3D") != nullptr);
	TFPASS(strstr(buf.data(), "=C3=A9") != nullptr);

	// plain ascii stays readable; the output always ends in "=\r\n"
	UT_UTF8Stringbuf plain("hello");
	plain.escapeMIME();
	TFPASS(strcmp(plain.data(), "hello=\r\n") == 0);

	// long strings are wrapped at 70 chars with =<CR><LF>
	std::string longline(80, 'w');
	UT_UTF8Stringbuf wrap(longline.c_str());
	wrap.escapeMIME();
	TFPASS(strstr(wrap.data(), "=\r\n") != nullptr);

	// empty is a no-op
	UT_UTF8Stringbuf empty;
	empty.escapeMIME();
	TFPASS(empty.empty());
}

TFTEST_MAIN("UT_UTF8Stringbuf escapeURL/decodeURL")
{
	UT_UTF8Stringbuf buf("a b&c\xC3\xA9");
	buf.escapeURL();
	// xmlURIEscape percent-encodes spaces and non-ascii
	TFPASS(strchr(buf.data(), ' ') == nullptr);
	TFPASS(strstr(buf.data(), "%") != nullptr);

	// decode simple percent sequences
	UT_UTF8Stringbuf enc("a%20b%26c");
	enc.decodeURL();
	TFPASS(strcmp(enc.data(), "a b&c") == 0);

	// a UTF-8 multi-byte percent sequence decodes to the char
	UT_UTF8Stringbuf euro("%E2%82%AC");
	euro.decodeURL();
	TFPASS(strcmp(euro.data(), "\xE2\x82\xAC") == 0);

	// '%' not followed by two hex digits is left alone
	UT_UTF8Stringbuf odd("100%sure");
	odd.decodeURL();
	TFPASS(strcmp(odd.data(), "100%sure") == 0);

	// empty is a no-op
	UT_UTF8Stringbuf empty;
	empty.escapeURL();
	empty.decodeURL();
	TFPASS(empty.empty());
}

TFTEST_MAIN("UT_UTF8Stringbuf UTF8Iterator")
{
	UT_UTF8Stringbuf buf("a\xC3\xA9z");
	UT_UTF8Stringbuf::UTF8Iterator it(&buf);

	TFPASS(it.start() == buf.data());
	TFPASS(*it.current() == 'a');

	// advance lands on the next UTF-8 character
	const char * p = it.advance();
	TFPASS(p == buf.data() + 1);
	TFPASS(UT_UTF8Stringbuf::charCode(p) == 0x00E9);

	// ++ moves past the multi-byte char
	++it;
	TFPASS(*it.current() == 'z');

	// retreat goes back one character
	it.retreat();
	TFPASS(UT_UTF8Stringbuf::charCode(it.current()) == 0x00E9);

	// -- to the start, end() is past the last byte
	--it;
	TFPASS(*it.current() == 'a');
	TFPASS(it.end() == buf.data() + buf.byteLength());

	// assignment positions the iterator
	it = buf.data() + 1;
	TFPASS(UT_UTF8Stringbuf::charCode(it.current()) == 0x00E9);
}
