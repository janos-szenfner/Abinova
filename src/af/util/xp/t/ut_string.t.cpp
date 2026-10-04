/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* AbiWord
 * Copyright (C) 2016 Hubert Figuiere
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
#include <map>
#include <string>
#include <vector>

#include "tf_test.h"
#include "ut_string.h"
#include "ut_growbuf.h"
#include "ut_unicode.h"

#define TFSUITE "core.af.util.string"

TFTEST_MAIN("UT_ensureValidXML")
{
    std::string str;
    bool result = UT_ensureValidXML(str);

    TFPASS(result);

    str = "foo\nbar\tbaz\rfizz buzz";
    TFPASS(UT_isValidXML(str.c_str()));
    result = UT_ensureValidXML(str);
    TFPASS(result);
    TFPASS(str == "foo\nbar\tbaz\rfizz buzz");

    str = "f\004oo\nbar\tbaz\rfizz\226 buzz";
    TFPASS(!UT_isValidXML(str.c_str()));
    result = UT_ensureValidXML(str);
    TFPASS(!result);
    TFPASS(str == "foo\nbar\tbaz\rfizz buzz");

    str = "poo\nbar\tbaz\rbizz\226 fuzz";
    TFPASS(!UT_isValidXML(str.c_str()));
    result = UT_ensureValidXML(str);
    TFPASS(!result);
    TFPASS(str == "poo\nbar\tbaz\rbizz fuzz");
}

TFTEST_MAIN("UT_checkedPrintfArgCount")
{
	/* plain literals: directive counting */
	TFPASS(UT_checkedPrintfArgCount("no directives", "s") == 0);
	TFPASS(UT_checkedPrintfArgCount("&1 %s", "s") == 1);
	TFPASS(UT_checkedPrintfArgCount("Printing page %d of %d", "diuxX") == 2);
	TFPASS(UT_checkedPrintfArgCount("100%% done", "s") == 0);
	TFPASS(UT_checkedPrintfArgCount("%5.1f", "f") == 1);
	TFPASS(UT_checkedPrintfArgCount("%ld", "diuxX") == 1);
	TFPASS(UT_checkedPrintfArgCount("%2$s %1$d", "ds") == 2);

	/* conversion char not in the caller's set -> unsafe; extra
	 * directives just raise the count above the caller's expectation */
	TFPASS(UT_checkedPrintfArgCount("%s", "d") == -1);
	TFPASS(UT_checkedPrintfArgCount("About %s %s", "s") == 2);
	TFPASS(UT_checkedPrintfArgCount("%d words", "d") == 1);

	/* dangerous or uncheckable forms are rejected */
	TFPASS(UT_checkedPrintfArgCount("%n", "dis") == -1);
	TFPASS(UT_checkedPrintfArgCount("%*d", "d") == -1);
	TFPASS(UT_checkedPrintfArgCount("%.*f", "f") == -1);
	TFPASS(UT_checkedPrintfArgCount("%[abc]", "s") == -1);
	TFPASS(UT_checkedPrintfArgCount("trailing %", "s") == -1);
	TFPASS(UT_checkedPrintfArgCount("%$d", "d") == -1);
	TFPASS(UT_checkedPrintfArgCount("%0$d", "d") == -1);
	TFPASS(UT_checkedPrintfArgCount(nullptr, "s") == -1);
	TFPASS(UT_checkedPrintfArgCount("%s", nullptr) == -1);

	/* flags / length modifiers pass through */
	TFPASS(UT_checkedPrintfArgCount("%+08.3f", "f") == 1);
	TFPASS(UT_checkedPrintfArgCount("%llu", "u") == 1);
	TFPASS(UT_checkedPrintfArgCount("'%d'", "d") == 1);
}


TFTEST_MAIN("UT_ensureValidXML multibyte sequences")
{
	// a truncated 3-byte UTF-8 sequence is stripped out
	std::string str = "ok\xE2\x82" "tail";
	TFPASS(!UT_ensureValidXML(str));
	TFPASS(str == "oktail");

	// a stray continuation byte is dropped
	str = "a\x80" "b";
	TFPASS(!UT_ensureValidXML(str));
	TFPASS(str == "ab");

	// valid multi-byte chars are preserved
	str = "caf\xC3\xA9"; // "café"
	TFPASS(UT_ensureValidXML(str));
	TFPASS(str == "caf\xC3\xA9");
}

TFTEST_MAIN("UT_XML ampersand helpers")
{
	gchar * dest = nullptr;

	// cloneNoAmpersands strips '&'
	TFPASS(UT_XML_cloneNoAmpersands(dest, "a&b&c"));
	TFPASS(strcmp(dest, "abc") == 0);
	g_free(dest); dest = nullptr;

	TFPASS(!UT_XML_cloneNoAmpersands(dest, nullptr));

	// cloneConvAmpersands: '&' -> '_', but "&&" -> "&"
	TFPASS(UT_XML_cloneConvAmpersands(dest, "a&b&&c"));
	TFPASS(strcmp(dest, "a_b&c") == 0);
	g_free(dest); dest = nullptr;

	TFPASS(!UT_XML_cloneConvAmpersands(dest, nullptr));

	// transNoAmpersands returns a shared static buffer
	const gchar * t = UT_XML_transNoAmpersands("x&y");
	TFPASS(t && strcmp(t, "xy") == 0);
	// a longer second call grows the shared buffer
	t = UT_XML_transNoAmpersands("longer&string&with&ampersands");
	TFPASS(t && strcmp(t, "longerstringwithampersands") == 0);
	TFPASS(UT_XML_transNoAmpersands(nullptr) == nullptr);
}

TFTEST_MAIN("UT_decodeUTF8string")
{
	UT_GrowBuf gb;

	// mixed ASCII + multibyte
	UT_decodeUTF8string("a\xC3\xA9z", 4, &gb);
	const UT_GrowBufElement * data = gb.getPointer(0);
	TFPASS(gb.getLength() == 3);
	TFPASS(data[0] == 'a' && data[1] == 0xE9 && data[2] == 'z');

	// a 3-byte sequence decodes to one UCS4 char
	gb.truncate(0);
	UT_decodeUTF8string("\xE2\x82\xAC", 3, &gb);
	data = gb.getPointer(0);
	TFPASS(gb.getLength() == 1 && data[0] == 0x20AC);
}

TFTEST_MAIN("UT_UCS4 character classes")
{
	TFPASS(UT_UCS4_isupper('A') && !UT_UCS4_isupper('a'));
	TFPASS(UT_UCS4_islower('a') && !UT_UCS4_islower('A'));
	TFPASS(UT_UCS4_isspace(' ') && UT_UCS4_isspace('\t') && !UT_UCS4_isspace('x'));
	TFPASS(UT_UCS4_isalpha('Q') && !UT_UCS4_isalpha('4'));
	TFPASS(UT_UCS4_isdigit('7') && !UT_UCS4_isdigit('x'));
	TFPASS(UT_UCS4_isSentenceSeparator('.') && !UT_UCS4_isSentenceSeparator('a'));
}

TFTEST_MAIN("UT_UCS4 string functions")
{
	static const UT_UCS4Char src[] = {'h','e','l','l','o',' ','w','o','r','l','d',0};

	TFPASS(UT_UCS4_strlen(src) == 11);

	// strcmp
	TFPASS(UT_UCS4_strcmp(src, src) == 0);
	static const UT_UCS4Char other[] = {'h','i',0};
	TFPASS(UT_UCS4_strcmp(src, other) != 0);

	// strcpy / strcmp round trip
	UT_UCS4Char buf[32];
	UT_UCS4_strcpy(buf, src);
	TFPASS(UT_UCS4_strcmp(buf, src) == 0);

	// strncpy pads and terminates
	memset(buf, 0xFF, sizeof(buf));
	UT_UCS4_strncpy(buf, src, 5);
	TFPASS(UT_UCS4_strlen(buf) == 5);
	TFPASS(buf[0] == 'h' && buf[4] == 'o');

	// strnrev reverses in place
	UT_UCS4Char rev[4] = {'a','b','c',0};
	UT_UCS4_strnrev(rev, 3);
	TFPASS(rev[0] == 'c' && rev[1] == 'b' && rev[2] == 'a');

	// strstr finds a needle
	static const UT_UCS4Char needle[] = {'w','o','r',0};
	const UT_UCS4Char * found = UT_UCS4_strstr(src, needle);
	TFPASS(found == src + 6);
	static const UT_UCS4Char absent[] = {'q','q',0};
	TFPASS(UT_UCS4_strstr(src, absent) == nullptr);

	// case folding
	TFPASS(UT_UCS4_toupper('q') == 'Q');
	TFPASS(UT_UCS4_tolower('Q') == 'q');

	// stristr is case insensitive
	static const UT_UCS4Char needleCI[] = {'W','O','R',0};
	TFPASS(UT_UCS4_stristr(src, needleCI) == src + 6);

	// strcpy_char converts an 8-bit native string
	memset(buf, 0, sizeof(buf));
	UT_UCS4_strcpy_char(buf, "hey");
	TFPASS(buf[0] == 'h' && buf[2] == 'y' && buf[3] == 0);

	// strncpy_char caps the length
	memset(buf, 0, sizeof(buf));
	UT_UCS4_strncpy_char(buf, "abcdef", 3);
	TFPASS(UT_UCS4_strlen(buf) == 3);

	// strcpy_utf8_char decodes UTF-8 input
	memset(buf, 0, sizeof(buf));
	UT_UCS4_strcpy_utf8_char(buf, "x\xC3\xA9");
	TFPASS(buf[0] == 'x' && buf[1] == 0xE9 && buf[2] == 0);

	// strcpy_to_char converts back to 8-bit
	char cbuf[16];
	UT_UCS4_strcpy_to_char(cbuf, src);
	TFPASS(strcmp(cbuf, "hello world") == 0);

	// strncpy_to_char caps it
	char cbuf2[8];
	memset(cbuf2, 0, sizeof(cbuf2));
	UT_UCS4_strncpy_to_char(cbuf2, src, 3);
	TFPASS(strncmp(cbuf2, "hel", 3) == 0);

	// cloneString / cloneString_char
	UT_UCS4Char * clone = nullptr;
	TFPASS(UT_UCS4_cloneString(&clone, src));
	TFPASS(UT_UCS4_strcmp(clone, src) == 0);
	FREEP(clone);

	TFPASS(UT_UCS4_cloneString_char(&clone, "abc"));
	TFPASS(clone[0] == 'a' && clone[2] == 'c' && clone[3] == 0);
	FREEP(clone);
}

TFTEST_MAIN("UT_UCS2_strlen")
{
	// only UT_UCS2_strlen is compiled in this build
	// (the rest of the UCS2 API needs ENABLE_UCS2_STRINGS)
	static const UT_UCS2Char src[] = {'h','e','l','l','o',0};
	TFPASS(UT_UCS2_strlen(src) == 5);
	static const UT_UCS2Char empty[] = {0};
	TFPASS(UT_UCS2_strlen(empty) == 0);
}

TFTEST_MAIN("UT_isSmartQuotable/Quoted")
{
	TFPASS(UT_isSmartQuotableCharacter('"'));
	TFPASS(UT_isSmartQuotableCharacter('\''));
	TFPASS(UT_isSmartQuotableCharacter('`'));
	TFPASS(!UT_isSmartQuotableCharacter('a'));

	TFPASS(UT_isSmartQuotedCharacter(UCS_LQUOTE));
	TFPASS(UT_isSmartQuotedCharacter(UCS_RQUOTE));
	TFPASS(UT_isSmartQuotedCharacter(UCS_LDBLQUOTE));
	TFPASS(UT_isSmartQuotedCharacter(UCS_RDBLQUOTE));
	TFPASS(UT_isSmartQuotedCharacter(0x201a));
	TFPASS(UT_isSmartQuotedCharacter(0x2039));
	TFPASS(UT_isSmartQuotedCharacter(0x300c));
	TFPASS(UT_isSmartQuotedCharacter('"'));
	TFPASS(UT_isSmartQuotedCharacter('\''));
	TFPASS(!UT_isSmartQuotedCharacter('x'));
}

TFTEST_MAIN("UT_parse_attributes")
{
	std::map<std::string, std::string> m;

	UT_parse_attributes("a=\"1\" b='2' c=\"x y\"", m);
	TFPASS(m.size() == 3);
	TFPASS(m["a"] == "1");
	TFPASS(m["b"] == "2");
	TFPASS(m["c"] == "x y");

	// nullptr / empty / malformed inputs just yield nothing
	m.clear();
	UT_parse_attributes(nullptr, m);
	UT_parse_attributes("", m);
	UT_parse_attributes("noequals", m);
	UT_parse_attributes("a=2", m);          // value must be quoted
	UT_parse_attributes("=\"v\"", m);       // empty name
	TFPASS(m.empty());
}

TFTEST_MAIN("UT_parse_properties")
{
	std::map<std::string, std::string> m;

	UT_parse_properties("color:red; font-size:12pt; margin-left: 1.0in;", m);
	TFPASS(m["color"] == "red");
	TFPASS(m["font-size"] == "12pt");
	TFPASS(m["margin-left"] == "1.0in");

	// stray colon/empty names are skipped
	m.clear();
	UT_parse_properties(":oops; good:yes", m);
	TFPASS(m.count("good") == 1);

	// nullptr / empty input -> nothing
	m.clear();
	UT_parse_properties(nullptr, m);
	UT_parse_properties("", m);
	TFPASS(m.empty());
}

TFTEST_MAIN("std_size_string")
{
	// C-locale fixed formatting with a '.' decimal separator
	TFPASS(strcmp(std_size_string(1.5f), "1.5") == 0);
	TFPASS(strcmp(std_size_string(0.0f), "0") == 0);
	TFPASS(strcmp(std_size_string(2.0f), "2") == 0);
}
