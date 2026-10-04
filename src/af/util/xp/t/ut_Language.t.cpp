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
#include "ut_Language.h"
#include "xap_Strings.h"

#define TFSUITE "core.af.util.Language"

TFTEST_MAIN("UT_Language table lookups")
{
	UT_Language lang;

	TFPASS(lang.getCount() > 100);

	// code -> record
	const UT_LangRecord * e = lang.getLangRecordFromCode("en-US");
	TFPASS(e != nullptr);
	if (e)
	{
		TFPASS(strcmp(e->m_szLangCode, "en-US") == 0);
		TFPASS(e->m_nID == XAP_STRING_ID_LANG_EN_US);
		TFPASS(e->m_eDir == UTLANG_LTR);
	}

	// case-insensitive
	e = lang.getLangRecordFromCode("EN-us");
	TFPASS(e != nullptr && strcmp(e->m_szLangCode, "en-US") == 0);

	// RTL languages report their direction
	TFPASS(lang.getDirFromCode("ar") == UTLANG_RTL);
	TFPASS(lang.getDirFromCode("en-US") == UTLANG_LTR);
	// unknown code -> LTR default
	TFPASS(lang.getDirFromCode("zz-NOSUCH") == UTLANG_LTR);

	// index + id lookups agree with the record
	UT_uint32 idx = lang.getIndxFromCode("de-DE");
	TFPASS(strcmp(lang.getNthLangCode(idx), "de-DE") == 0);
	TFPASS(lang.getIdFromCode("de-DE") == XAP_STRING_ID_LANG_DE_DE);
	TFPASS(lang.getNthId(idx) == XAP_STRING_ID_LANG_DE_DE);

	// getCodeFromCode returns the canonical table pointer
	TFPASS(strcmp(lang.getCodeFromCode("fr-FR"), "fr-FR") == 0);

	// unknown codes
	TFPASS(lang.getLangRecordFromCode("zzz-nothing") == nullptr);
	TFPASS(lang.getIdFromCode("zzz-nothing") == 0);
	TFPASS(lang.getIndxFromCode("zzz-nothing") == 0);
	TFPASS(lang.getCodeFromCode("zzz-nothing") == nullptr);
}

TFTEST_MAIN("UT_Language short-code fallback")
{
	UT_Language lang;

	// a "code-region" lookup that isn't in the table falls back to
	// the bare language code ("bm" is a real entry, "bm-XX" is not)
	const UT_LangRecord * e = lang.getLangRecordFromCode("bm-XX");
	TFPASS(e != nullptr);
	if (e)
		TFPASS(strcmp(e->m_szLangCode, "bm") == 0);

	// same fallback through the index lookup
	UT_uint32 idx = lang.getIndxFromCode("bm-XX");
	TFPASS(strcmp(lang.getNthLangCode(idx), "bm") == 0);
}

TFTEST_MAIN("UT_Language name round trip")
{
	UT_Language lang;

	// every table entry yields a non-empty code
	for (UT_uint32 i = 0; i < lang.getCount(); i++)
		TFPASS(lang.getNthLangCode(i) != nullptr);

	// getCodeFromName compares against the localised names loaded
	// from the stringset; only run it when names were actually set
	if (lang.getNthLangName(1) != nullptr)
	{
		// find an entry with a real name and confirm the mapping
		for (UT_uint32 i = 0; i < lang.getCount(); i++)
		{
			const gchar * name = lang.getNthLangName(i);
			const gchar * code = lang.getNthLangCode(i);
			if (name && *name && strcmp(code, "-none-") != 0)
			{
				const gchar * back = lang.getCodeFromName(name);
				TFPASS(back != nullptr);
				if (back)
					TFPASS(strcmp(back, code) == 0);
				break;
			}
		}

		// unknown name -> nullptr
		TFPASS(lang.getCodeFromName("no such language xyzzy") == nullptr);
	}
}
