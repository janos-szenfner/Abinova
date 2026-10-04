/* AbiWord
 * Copyright (C) 2014 Hubert Figuiere
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

#include <vector>

#include "tf_test.h"
#include "ut_misc.h"

#define TFSUITE "core.af.util.misc"

TFTEST_MAIN("UT_HeadingDepth")
{
  UT_uint32 depth;
  depth = UT_HeadingDepth("Heading 1");
  TFPASS(depth == 1);

  depth = UT_HeadingDepth("Normal");
  TFPASS(depth == 0);

  depth = UT_HeadingDepth("Heading 10");
  TFPASS(depth == 10);

  depth = UT_HeadingDepth("Numbered Heading 5");
  TFPASS(depth == 5);
}


TFTEST_MAIN("UT_VersionInfo")
{
  UT_VersionInfo v1(1,2,3,4);
  std::string verString = v1.getString();

  TFPASS(verString == "1.2.3.4");
  TFPASS(v1.getMajor() == 1);
  TFPASS(v1.getMinor() == 2);
  TFPASS(v1.getMicro() == 3);
  TFPASS(v1.getNano() == 4);

  UT_VersionInfo v2;

  verString = v2.getString();
  TFPASS(verString == "0.0.0.0");

  TFPASS(v1 > v2);
  TFFAIL(v2 > v1);

  v2.set(1,2,3,5);

  TFPASS(v2 > v1);
  verString = v2.getString();

  TFPASS(verString == "1.2.3.5");
}

// ----------------------------------------------------------------
// Semantics pins added for TST05
// ----------------------------------------------------------------

TFTEST_MAIN("isTrue")
{
	// isTrue is intentionally narrow: nullptr, "0" and "false" are
	// false, EVERYTHING else is true - including "FALSE", "off",
	// "no" and the empty string.  Callers that need case-insensitive
	// parsing must use UT_parseBool.
	TFPASS(!isTrue(nullptr));
	TFPASS(!isTrue("0"));
	TFPASS(!isTrue("false"));
	TFPASS(isTrue("1"));
	TFPASS(isTrue("true"));
	TFPASS(isTrue("FALSE"));
	TFPASS(isTrue("off"));
	TFPASS(isTrue(""));
}

TFTEST_MAIN("UT_parseBool")
{
	// recognised true/false words (case-insensitive)
	for (const char* s : {"true", "TRUE", "1", "yes", "Yes", "allow", "enable", "on"})
		TFPASS(UT_parseBool(s, false));
	for (const char* s : {"false", "FALSE", "0", "no", "No", "disallow", "disable", "off"})
		TFPASS(!UT_parseBool(s, true));

	// anything else - including nullptr and "" - returns the default
	TFPASS(UT_parseBool("banana", true));
	TFPASS(!UT_parseBool("banana", false));
	TFPASS(UT_parseBool(nullptr, true));
	TFPASS(!UT_parseBool(nullptr, false));
	TFPASS(UT_parseBool("", true));

	// quirk: matching is a PREFIX test, so words starting with a
	// recognised token take that token's truth value
	TFPASS(UT_parseBool("onion", false));     // "on"  -> true
	TFPASS(!UT_parseBool("nothing", true));   // "no"  -> false
	TFPASS(!UT_parseBool("offramp", true));   // "off" -> false
	TFPASS(UT_parseBool("1st", false));       // "1"   -> true
}

TFTEST_MAIN("UT_getAttribute")
{
	const gchar* atts[] = {"one", "1", "two", "2", "three", "3", nullptr};
	TFPASS(UT_getAttribute("one", atts) != nullptr);
	TFPASS(strcmp(UT_getAttribute("two", atts), "2") == 0);
	TFPASS(strcmp(UT_getAttribute("three", atts), "3") == 0);
	TFPASS(UT_getAttribute("missing", atts) == nullptr);
	// nullptr atts list is guarded
	TFPASS(UT_getAttribute("one", nullptr) == nullptr);
}

TFTEST_MAIN("UT_splitPropsToArray")
{
	// splits "name:value;name:value" into a NULL-terminated
	// [name, val, name, val, ...] array; the input is destructively
	// modified and the array must be delete[]'d
	gchar props[] = "font-size:24pt;font-family:Arial";
	const gchar** arr = UT_splitPropsToArray(props);
	TFPASS(arr != nullptr);
	TFPASS(strcmp(arr[0], "font-size") == 0);
	TFPASS(strcmp(arr[1], "24pt") == 0);
	TFPASS(strcmp(arr[2], "font-family") == 0);
	TFPASS(strcmp(arr[3], "Arial") == 0);
	TFPASS(arr[4] == nullptr);
	delete[] arr;

	// trailing ';' does not produce a phantom pair
	gchar props2[] = "a:b;";
	const gchar** arr2 = UT_splitPropsToArray(props2);
	TFPASS(arr2 != nullptr);
	TFPASS(strcmp(arr2[0], "a") == 0);
	TFPASS(strcmp(arr2[1], "b") == 0);
	TFPASS(arr2[2] == nullptr);
	delete[] arr2;
}

TFTEST_MAIN("UT_Rect")
{
	UT_Rect r(10, 20, 30, 40);
	TFPASS(r.left == 10 && r.top == 20 && r.width == 30 && r.height == 40);

	// containsPoint is half-open: left/top inclusive, right/bottom exclusive
	TFPASS(r.containsPoint(10, 20));
	TFPASS(r.containsPoint(39, 59));
	TFPASS(!r.containsPoint(40, 20));
	TFPASS(!r.containsPoint(10, 60));
	TFPASS(!r.containsPoint(9, 20));

	// intersectsRect: merely touching edges counts as intersecting
	UT_Rect touch(40, 20, 10, 10);   // left edge at r's right edge
	TFPASS(r.intersectsRect(&touch));
	UT_Rect apart(41, 20, 10, 10);
	TFPASS(!r.intersectsRect(&apart));
	UT_Rect inside(15, 25, 5, 5);
	TFPASS(r.intersectsRect(&inside));
	UT_Rect above(10, 10, 30, 10);   // bottom edge at r's top edge
	TFPASS(r.intersectsRect(&above));

	// unionRect -> smallest rect covering both
	UT_Rect u(0, 0, 10, 10);
	u.unionRect(&touch);
	TFPASS(u.left == 0 && u.top == 0 && u.width == 50 && u.height == 30);

	// set() and copy ctor
	u.set(1, 2, 3, 4);
	TFPASS(u.left == 1 && u.top == 2 && u.width == 3 && u.height == 4);
	UT_Rect cpy(r);
	TFPASS(cpy.left == r.left && cpy.top == r.top &&
	       cpy.width == r.width && cpy.height == r.height);
}

TFTEST_MAIN("UT_UniqueId")
{
	UT_UniqueId u;

	// ids start at 0 for most types; List is pre-reserved to 1000
	// (AUTO_LIST_RESERVED) so document list-ids can't collide with
	// user-defined list styles
	TFPASS(u.getUID(UT_UniqueId::Footnote) == 0);
	TFPASS(u.getUID(UT_UniqueId::Footnote) == 1);
	TFPASS(u.getUID(UT_UniqueId::List) == 1000);
	TFPASS(u.getUID(UT_UniqueId::List) == 1001);

	// isIdUnique(i) is true iff i has NOT been issued yet
	TFPASS(!u.isIdUnique(UT_UniqueId::Footnote, 0));
	TFPASS(!u.isIdUnique(UT_UniqueId::Footnote, 1));
	TFPASS(u.isIdUnique(UT_UniqueId::Footnote, 2));

	// setMinId can only RAISE the counter
	TFPASS(!u.setMinId(UT_UniqueId::Footnote, 0));   // current is 2
	TFPASS(u.setMinId(UT_UniqueId::Footnote, 5));
	TFPASS(u.getUID(UT_UniqueId::Footnote) == 5);
	TFPASS(!u.isIdUnique(UT_UniqueId::Footnote, 4));

	// out-of-range type is guarded
	TFPASS(u.getUID(UT_UniqueId::_Last) == UT_UID_INVALID);
	TFPASS(!u.setMinId(UT_UniqueId::_Last, 0));
	TFPASS(!u.isIdUnique(UT_UniqueId::_Last, 0));
}

TFTEST_MAIN("word helpers")
{
	// signedLoWord/signedHiWord sign-extend the 16-bit halves
	TFPASS(signedLoWord(0x0000FFFF) == -1);
	TFPASS(signedHiWord(0xFFFF0000) == -1);
	TFPASS(signedLoWord(0x12345678) == 0x5678);
	TFPASS(signedHiWord(0x12345678) == 0x1234);
	TFPASS(signedHiWord(0xABCD0000) == static_cast<UT_sint32>(0xFFFFABCD));

	// UT_HeadingDepth extracts the first run of digits
	TFPASS(UT_HeadingDepth("Heading 3") == 3);
	TFPASS(UT_HeadingDepth("List 10 para") == 10);
	TFPASS(UT_HeadingDepth("NoDigits") == 0);
	TFPASS(UT_HeadingDepth("123") == 123);

	// UT_isWordDelimiter: letters/digits are word chars, '_' is a
	// separator, and an apostrophe between two letters is INTERNAL
	// to the word ("don't" -> one word; "'quote" -> word after ')
	TFPASS(!UT_isWordDelimiter('a', 0, 0));
	TFPASS(!UT_isWordDelimiter('Z', 0, 0));
	TFPASS(!UT_isWordDelimiter('5', 0, 0));
	TFPASS(UT_isWordDelimiter('_', 'a', 'b'));
	TFPASS(UT_isWordDelimiter(' ', 'a', 'b'));
	TFPASS(UT_isWordDelimiter('-', 'a', 'b'));
	TFPASS(!UT_isWordDelimiter('\'', 'e', 'n'));   // don't
	TFPASS(UT_isWordDelimiter('\'', 0, 'b'));      // leading '
	TFPASS(UT_isWordDelimiter('\'', 'a', 0));      // trailing '

	// UT_addOrReplacePathSuffix replaces a trailing ".ext", else appends
	std::string p = "/a/b/name.png";
	UT_addOrReplacePathSuffix(p, ".tif");
	TFPASS(p == "/a/b/name.tif");
	p = "/a/b/name";
	UT_addOrReplacePathSuffix(p, ".tif");
	TFPASS(p == "/a/b/name.tif");
	p = "/a.b/name";
	UT_addOrReplacePathSuffix(p, ".tif");   // dot in a dir is ignored
	TFPASS(p == "/a.b/name.tif");

	// hashing is deterministic - and has a real quirk worth pinning:
	// the loop advances the pointer AFTER hashing, so the FIRST byte
	// feeds in twice and the LAST byte is never hashed.  "abc" and
	// "abd" genuinely collide; callers must not rely on last-char
	// sensitivity.
	TFPASS(UT_hash32("abc", 3) == UT_hash32("abc", 3));
	TFPASS(UT_hash32("abc", 3) == UT_hash32("abd", 3));   // last byte ignored
	TFPASS(UT_hash32("abc", 3) != UT_hash32("xbc", 3));
	TFPASS(UT_hash32("ab", 2) == UT_hash32("ax", 2));     // 1st byte twice
	TFPASS(UT_hash64("abc", 3) == UT_hash64("abd", 3));
	TFPASS(UT_hash64("abc", 3) != UT_hash64("xbc", 3));

	// UT_secureZero wipes the buffer
	char buf[16];
	memset(buf, 'X', sizeof(buf));
	UT_secureZero(buf, sizeof(buf));
	for (char c : buf)
		TFPASS(c == 0);
}

