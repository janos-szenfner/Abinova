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

#include "tf_test.h"
#include "ut_string.h"

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


/*UT_XML_cloneNoAmpersands*/

/*UT_XML_transNoAmpersands*/

/*UT_decodeUTF8string*/
