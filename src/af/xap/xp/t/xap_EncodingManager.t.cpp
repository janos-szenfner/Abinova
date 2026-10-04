/* AbiSource Application Framework
 * Copyright (C) 2026 Abinova contributors
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

#include "xap_EncodingManager.h"

#define TFSUITE "core.af.xap.encodingmanager"

static XAP_EncodingManager *em()
{
	return XAP_EncodingManager::get_instance();
}

TFTEST_MAIN("XAP_EncodingManager names")
{
	TFPASS(em() != nullptr);

	/* every "must not return nullptr" getter */
	TFPASS(em()->getNativeEncodingName() != nullptr);
	TFPASS(em()->getNativeSystemEncodingName() != nullptr);
	TFPASS(em()->getNative8BitEncodingName() != nullptr);
	TFPASS(em()->getNativeNonUnicodeEncodingName() != nullptr);
	TFPASS(em()->getLanguageISOName() != nullptr);
	(void)em()->getLanguageISOTerritory();

	/* the UCS alias probes run inside initialize(); the names are
	 * allowed to be nullptr when iconv lacks the encoding */
	(void)em()->getNativeUnicodeEncodingName();
	(void)em()->getUCS2BEName();
	(void)em()->getUCS2LEName();
	(void)em()->getUCS4BEName();
	(void)em()->getUCS4LEName();

	TFPASS(em()->getTexPrologue() != nullptr);
	TFPASS(em()->WindowsCharsetName() != nullptr);
	em()->getWinLanguageCode();
	em()->getWinCharsetCode();
	em()->isUnicodeLocale();
	em()->cjk_locale();
	em()->single_case();
}

TFTEST_MAIN("XAP_EncodingManager char conversion")
{
	/* ASCII is its own conversion in every direction on every locale */
	TFPASS(em()->nativeToU('A') == 'A');
	TFPASS(em()->UToNative('A') == 'A');
	TFPASS(em()->WindowsToU('A') == 'A');
	TFPASS(em()->UToWindows('A') == 'A');
	TFPASS(em()->try_UToLatin1('A') == 'A');
	TFPASS(em()->try_nativeToU('A') == 'A');
	TFPASS(em()->try_UToNative('A') == 'A');
	TFPASS(em()->try_WindowsToU('A') == 'A');
	TFPASS(em()->try_UToWindows('A') == 'A');

	/* unrepresentable input falls back rather than crashing */
	TFPASS(em()->fallbackChar(0x20ac) != 0);

	/* U+20AC (euro sign) has no latin-1 representation in most
	 * locales; the wrapper must return *something* (fallback),
	 * never crash */
	TFPASS(em()->UToNative(0x20ac) != 0);
	TFPASS(em()->UToWindows(0x20ac) != 0);
	TFPASS(em()->nativeToU(0x100) != 0); /* >0xff input path */
}

TFTEST_MAIN("XAP_EncodingManager approximate")
{
	char buf[16];

	/* nothing fits in zero bytes */
	TFPASS(em()->approximate(buf, 0, 0x201d) == 0);

	/* curly double quotes degrade to ASCII '"' at max_length==1 */
	buf[0] = 0;
	TFPASS(em()->approximate(buf, 1, 0x201d) == 1);
	TFPASS(buf[0] == '"');
	TFPASS(em()->approximate(buf, 1, 0x201c) == 1);
	TFPASS(buf[0] == '"');

	/* no approximation known for this one */
	TFPASS(em()->approximate(buf, 1, 'x') == 0);

	/* multi-char approximations are unimplemented upstream */
	TFPASS(em()->approximate(buf, 16, 0x201d) == 0);
}

TFTEST_MAIN("XAP_EncodingManager strToNative")
{
	char buf[128];

	/* nullptr/empty charset or input passes the input straight through */
	const char *r1 = em()->strToNative("hello", nullptr);
	TFPASS(r1 != nullptr && !strcmp(r1, "hello"));
	const char *r2 = em()->strToNative("hello", "");
	TFPASS(r2 != nullptr && !strcmp(r2, "hello"));

	buf[0] = 0;
	const char *r3 = em()->strToNative(nullptr, "UTF-8", buf, sizeof(buf));
	TFPASS(r3 == nullptr || !strcmp(r3, ""));

	/* an unknown charset cannot be translated -> input returned */
	TFPASS(!strcmp(em()->strToNative("abc", "X-NO-SUCH-CHARSET-zz"), "abc"));

	/* UTF-8 -> native conversion of pure ASCII is identity */
	const char *out = em()->strToNative("plain ascii", "UTF-8", buf, sizeof(buf));
	TFPASS(out && !strcmp(out, "plain ascii"));

	/* reverse direction (native -> UTF-8) */
	buf[0] = 0;
	out = em()->strToNative("native text", "UTF-8", buf, sizeof(buf), true);
	TFPASS(out != nullptr);

	/* system-encoding variant */
	buf[0] = 0;
	out = em()->strToNative("sys", "UTF-8", buf, sizeof(buf), false, true);
	TFPASS(out != nullptr);
}

TFTEST_MAIN("XAP_EncodingManager XML unknown-encoding handler")
{
	/* built without expat support this is a stub returning 0 */
	TFPASS(XAP_EncodingManager::XAP_XML_UnknownEncodingHandler(
			   nullptr, "CP1251", nullptr) == 0);
}

TFTEST_MAIN("XAP_EncodingManager codepage maps")
{
	/* known codepages resolve to iconv-friendly names */
	TFPASS(!strcmp(em()->charsetFromCodepage(1252), "CP1252"));
	TFPASS(!strcmp(em()->charsetFromCodepage(932), "SJIS"));
	TFPASS(!strcmp(em()->charsetFromCodepage(936), "GBK"));
	TFPASS(!strcmp(em()->charsetFromCodepage(65001), "UTF-8"));

	/* unknown codepage comes back as the CPn string */
	TFPASS(!strcmp(em()->charsetFromCodepage(42), "CP42"));

	/* charset -> codepage reverse map */
	TFPASS(!strcmp(em()->CodepageFromCharset("SJIS"), "CP932"));
	TFPASS(!strcmp(em()->CodepageFromCharset("GBK"), "CP936"));
	TFPASS(!strcmp(em()->CodepageFromCharset("BIG5"), "CP950"));

	/* unknown charset passes through unchanged */
	TFPASS(!strcmp(em()->CodepageFromCharset("CP1252"), "CP1252"));
}

TFTEST_MAIN("XAP_EncodingManager CJK classification")
{
	/* in a non-CJK locale every char reports non-CJK; in a CJK
	 * locale chars >0xff are CJK -- either way calls must work */
	const bool cjk = em()->cjk_locale();
	TFPASS(em()->is_cjk_letter(0x4e00) == cjk);
	TFPASS(em()->is_cjk_letter('a') == false);

	const UT_UCS4Char latin[] = {'h','e','l','l','o',0};
	TFPASS(em()->noncjk_letters(latin, 5) == true);

	const UT_UCS4Char mixed[] = {'a',0x4e00,0};
	TFPASS(em()->noncjk_letters(mixed, 2) == !cjk);
}

TFTEST_MAIN("XAP_EncodingManager canBreakBetween")
{
	UT_UCS4Char c[2];

	/* NONATOMIC|NONATOMIC: no break inside words */
	c[0] = 'a'; c[1] = 'b';
	TFPASS(em()->canBreakBetween(c) == false);

	/* em-dash pair can never break */
	c[0] = c[1] = UCS_EM_DASH;
	TFPASS(em()->canBreakBetween(c) == false);

	/* right double quote before a letter: Finnish special case */
	c[0] = UCS_RDBLQUOTE; c[1] = 'a';
	TFPASS(em()->canBreakBetween(c) == false);

	/* CJK ideograph (ATOMIC) breaks freely either side of latin */
	c[0] = 0x4e00; c[1] = 'a';
	TFPASS(em()->canBreakBetween(c) == true);
	c[0] = 'a'; c[1] = 0x4e00;
	TFPASS(em()->canBreakBetween(c) == true);

	/* no break before a PUNCNOSTART char (e.g. CJK close bracket) */
	c[0] = 'a'; c[1] = 0x300d;
	TFPASS(em()->canBreakBetween(c) == false);
	c[0] = 0x300d; c[1] = 'a';
	TFPASS(em()->canBreakBetween(c) == true);

	/* no break after PUNCNOEND (CJK open bracket) */
	c[0] = 0x3008; c[1] = 'a';
	TFPASS(em()->canBreakBetween(c) == false);

	/* en-dash (PUNCFORCE) breaks both ways */
	c[0] = 0x2013; c[1] = 'a';
	TFPASS(em()->canBreakBetween(c) == true);
	c[0] = 'a'; c[1] = 0x2013;
	TFPASS(em()->canBreakBetween(c) == true);
}

TFTEST_MAIN("XAP_EncodingManager langinfo lookup")
{
	const XAP_LangInfo *en =
		XAP_EncodingManager::findLangInfo("en", XAP_LangInfo::isoshortname_idx);
	TFPASS(en != nullptr);
	TFPASS(!strcmp(en->fields[XAP_LangInfo::longname_idx], "English"));

	TFPASS(XAP_EncodingManager::findLangInfo(
			   "French", XAP_LangInfo::longname_idx) != nullptr);
	TFPASS(XAP_EncodingManager::findLangInfo(
			   "langRussian", XAP_LangInfo::macname_idx) != nullptr);
	TFPASS(XAP_EncodingManager::findLangInfo(
			   "zz", XAP_LangInfo::isoshortname_idx) == nullptr);

	/* the table is NUL-terminated on field 0 */
	TFPASS(XAP_EncodingManager::langinfo[0].fields[0] != nullptr);

	/* smart quote table: index 0 is English double quotes */
	TFPASS(XAP_EncodingManager::smartQuoteStyles[0].leftQuote == UCS_LDBLQUOTE);
	TFPASS(XAP_EncodingManager::smartQuoteStyles[0].rightQuote == UCS_RDBLQUOTE);

	/* findLangInfoByLocale: bare code, country-qualified, fallback */
	const XAP_LangInfo *de = XAP_EncodingManager::findLangInfoByLocale("de");
	TFPASS(de != nullptr);
	TFPASS(!strcmp(de->fields[XAP_LangInfo::longname_idx], "German"));

	const XAP_LangInfo *dech = XAP_EncodingManager::findLangInfoByLocale("de_CH");
	TFPASS(dech != nullptr);
	TFPASS(!strcmp(dech->fields[XAP_LangInfo::countrycode_idx], "CH"));

	/* unknown country falls back to the unqualified record */
	const XAP_LangInfo *fr = XAP_EncodingManager::findLangInfoByLocale("fr_FR");
	TFPASS(fr != nullptr);
	TFPASS(!strcmp(fr->fields[XAP_LangInfo::longname_idx], "French"));

	TFPASS(XAP_EncodingManager::findLangInfoByLocale("zz") == nullptr);
	TFPASS(XAP_EncodingManager::findLangInfoByLocale(nullptr) == nullptr);
}

TFTEST_MAIN("XAP_EncodingManager localeinfo_combinations")
{
	const char **pp = localeinfo_combinations("pre", "suf", "sep", false);
	TFPASS(pp != nullptr);

	/* slot 0 is prefix+suffix (the fallback entry) */
	TFPASS(!strcmp(pp[0], "presuf"));

	/* remaining slots are prefix+sep+<part>+suffix */
	TFPASS(strncmp(pp[1], "presep", 6) == 0);
	for (int i = 1; i < 5; i++)
	{
		const size_t l = strlen(pp[i]);
		TFPASS(l >= 3 + 4 + 3); /* pre sep ... suf */
		TFPASS(!strcmp(pp[i] + l - 3, "suf"));
	}
	TFPASS(pp[5] == nullptr);

	/* skip_fallback drops the plain-prefix slot -- but buf[0] is
	 * static and is NOT reset in that mode, so the previous
	 * call's slot-0 content is reused (a quirk of the impl) */
	localeinfo_combinations("X", "", ".", false); /* leaves buf[0]="X" */
	const char **pp2 = localeinfo_combinations("pre", "suf", "sep", true);
	TFPASS(strncmp(pp2[0], "Xsep", 4) == 0); /* stale "X" + sep + lang... */
	TFPASS(strncmp(pp2[1], "presep", 6) == 0);
}

TFTEST_MAIN("XAP_EncodingManager misc tables")
{
	/* font size bijection was populated by initialize() */
	TFPASS(XAP_EncodingManager::fontsizes_mapping.size() > 0);
	const char * twelve =
		XAP_EncodingManager::fontsizes_mapping.lookupBySource("12");
	TFPASS(twelve != nullptr || em()->cjk_locale());

	/* the extern "C" pspell hook */
	extern const char * xap_encoding_manager_get_language_iso_name(void);
	TFPASS(xap_encoding_manager_get_language_iso_name() != nullptr);
}
