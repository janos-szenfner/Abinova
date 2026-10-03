/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource
 * 
 * Copyright (C) 2007 Philippe Milot <PhilMilot@gmail.com>
 * Copyright (C) 2025-2026 Abinova contributors
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

// Class definition include
#include "OXML_FontManager.h"

// Internal includes
#include "OXML_Document.h"
#include "OXML_Theme.h"

// Abinova includes
#include "ut_assert.h"

// External includes
#include <set>
#include <string>

#include <pango/pangocairo.h>

/* Case-folded font name used as a lookup key.  Family matching in
 * Pango/fontconfig is case-insensitive, and Word treats face names
 * the same way. */
static std::string s_fontKey(const std::string & name)
{
	std::string key(name);
	for (auto & c : key)
		c = g_ascii_tolower(c);
	return key;
}

/* The family names the Pango font map can actually render with --
 * the same set GR_CairoGraphics::getAllFontNames enumerates, so the
 * substitution decision matches what layout will do (this includes
 * the bundled fonts added via FcConfigAppFontAddDir at app start). */
static const std::set<std::string> & s_installedFamilies()
{
	static std::set<std::string> families;
	static bool loaded = false;
	if (!loaded)
	{
		loaded = true;
		PangoFontMap * fontmap = pango_cairo_font_map_get_default();
		if (fontmap)
		{
			PangoFontFamily ** fams = nullptr;
			int n = 0;
			pango_font_map_list_families(fontmap, &fams, &n);
			for (int i = 0; i < n; i++)
				families.insert(
					s_fontKey(pango_font_family_get_name(fams[i])));
			g_free(fams);
		}
	}
	return families;
}

static bool s_fontInstalled(const std::string & name)
{
	return s_installedFamilies().count(s_fontKey(name)) != 0;
}

OXML_FontManager::OXML_FontManager() : 
	m_defaultFont("Times New Roman")
{
	m_major_rts.clear();
	m_minor_rts.clear();
	m_fontTable.clear();
}

std::string OXML_FontManager::getValidFont(OXML_FontLevel level, OXML_CharRange range)
{
	UT_return_val_if_fail(	UNKNOWN_LEVEL != level && 
							UNKNOWN_RANGE != range, m_defaultFont);
	//Algorithm:
	// 1) Retrieve the lang code mapped with this level/range combination in the Doc Settings
	std::string script(""), font_name("");
	OXML_RangeToScriptMap::iterator it;
	if (level == MAJOR_FONT) {
		it = m_major_rts.find(range);
		if (it == m_major_rts.end()) {
			switch (range) {
			// 1a) If no mapping exists, then Ascii/HAnsi = "latin"; EastAsia = "ea"; Complex = "cs"
			case (ASCII_RANGE): //fallthrough to HANSI_RANGE
			case (HANSI_RANGE): script = "latin"; break;
			case (COMPLEX_RANGE): script = "cs"; break;
			case (EASTASIAN_RANGE): script = "ea"; break;
			default: UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
			}
		} else {
			script = it->second;
		}
	} else {
		it = m_minor_rts.find(range);
		if (it == m_minor_rts.end()) {
			switch (range) {
			// 1a) If no mapping exists, then Ascii/HAnsi = "latin"; EastAsia = "ea"; Complex = "cs"
			case (ASCII_RANGE): //fallthrough to HANSI_RANGE
			case (HANSI_RANGE): script = "latin"; break;
			case (COMPLEX_RANGE): script = "cs"; break;
			case (EASTASIAN_RANGE): script = "ea"; break;
			default: UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
			}
		} else {
			script = it->second;
		}
	}

	// 2) Retrieve the font name mapped with this country string in the Theme
	OXML_Document * doc = OXML_Document::getInstance();
	if (nullptr == doc) {
		return m_defaultFont;
	}
	OXML_SharedTheme theme = doc->getTheme();
	if (theme.get() == nullptr) {
		return m_defaultFont;
	}
	if (level == MAJOR_FONT)
		font_name = theme->getMajorFont(script);
	else
		font_name = theme->getMinorFont(script);

	/* 2a) If the mapped script has no font in the theme, retry with the
	 * range's default script.  w:themeFontLang maps ranges to *languages*
	 * (e.g. "en-US" -> "Latn"), but theme <a:font> entries are keyed by
	 * ISO-15924 script codes and the Latin typeface lives under "latin",
	 * so the mapped script often has no direct entry. */
	if (!font_name.compare("")) {
		std::string defScript;
		switch (range) {
		case (ASCII_RANGE): //fallthrough to HANSI_RANGE
		case (HANSI_RANGE): defScript = "latin"; break;
		case (COMPLEX_RANGE): defScript = "cs"; break;
		case (EASTASIAN_RANGE): defScript = "ea"; break;
		default: UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
		}
		if (defScript != script) {
			if (level == MAJOR_FONT)
				font_name = theme->getMajorFont(defScript);
			else
				font_name = theme->getMinorFont(defScript);
		}
	}

	// 2b) If still no mapping exists, return the default document font
	if (!font_name.compare("")) return m_defaultFont;

	// 3) Return getValidFont(font name)
	return getValidFont(font_name);
}

std::string OXML_FontManager::getValidFont(std::string name)
{
	if (name.empty() || s_fontInstalled(name))
		return name;

	/* The referenced family cannot be rendered on this system.  The
	 * FontTable part's w:altName is the document's declared substitute
	 * for exactly this case (ECMA-376 17.8.3.1) -- prefer it when it
	 * resolves to an installed family.  Otherwise keep the original
	 * name: the system font matcher (fontconfig, including the bundled
	 * abinova-fonts.conf metric-compatible aliases) still resolves it
	 * at layout, and the declared name survives round-trips. */
	auto it = m_fontTable.find(s_fontKey(name));
	if (it != m_fontTable.end() &&
		!it->second.altName.empty() &&
		s_fontInstalled(it->second.altName))
	{
		return it->second.altName;
	}
	return name;
}

void OXML_FontManager::addFontTableEntry(const std::string & name,
										const OXML_FontTableEntry & entry)
{
	if (name.empty())
		return;
	m_fontTable[s_fontKey(name)] = entry;
}

void OXML_FontManager::mapRangeToScript(OXML_CharRange range, std::string script)
{
	m_major_rts[range] = script;
	m_minor_rts[range] = script;
}

