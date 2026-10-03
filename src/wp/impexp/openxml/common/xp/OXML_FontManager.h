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

#ifndef _OXML_FONTMANAGER_H_
#define _OXML_FONTMANAGER_H_

// Internal includes
#include <OXML_Types.h>

// External includes
#include <map>
#include <memory>
#include <string>

/* \struct OXML_FontTableEntry
 * \brief The substitution metadata declared for one font in the
 * document's word/fontTable.xml part (ECMA-376 CT_Font).  All fields
 * are optional in the part; empty strings mean "not declared".
 */
struct OXML_FontTableEntry
{
	std::string altName;	//!< w:altName -- name to substitute when the font is unavailable
	std::string panose1;	//!< w:panose1 -- PANOSE-1 classification
	std::string charset;	//!< w:charset -- ANSI code page id
	std::string family;		//!< w:family -- auto|decorative|modern|roman|script|swiss
	std::string pitch;		//!< w:pitch -- default|fixed|variable
};

/* \class OXML_FontManager
 * \brief This class handles all the font-related activities.
 * OXML_FontManager has two main purposes.  The first is to keep track of mappings
 * between a character range and a specific script.  The character range is either
 * ASCII, East-Asian, Complex, or H-ANSI.  The script is a string in the ISO-15924
 * standard format.
 * The second purpose of OXML_FontManager is to analyze a font face name and validate
 * it against the document's FontTable part to ensure that the proper font face is
 * used.
*/
class OXML_FontManager
{
public:
	OXML_FontManager();

	inline std::string getDefaultFont() { return m_defaultFont; }

	std::string getValidFont(OXML_FontLevel level, OXML_CharRange range);
	std::string getValidFont(std::string name);

	void mapRangeToScript(OXML_CharRange range, std::string script);

	//! Records one <w:font> declaration from the FontTable part.
	/*! \param name   value of the element's w:name attribute
	 *  \param entry  parsed substitution metadata */
	void addFontTableEntry(const std::string & name,
						   const OXML_FontTableEntry & entry);

private:
	std::string m_defaultFont;

	typedef std::map<OXML_CharRange, std::string> OXML_RangeToScriptMap;
	OXML_RangeToScriptMap m_major_rts;
	OXML_RangeToScriptMap m_minor_rts;

	//! FontTable declarations keyed by lower-cased font name
	std::map<std::string, OXML_FontTableEntry> m_fontTable;
};

typedef std::shared_ptr<OXML_FontManager> OXML_SharedFontManager;

#endif //_OXML_FONTMANAGER_H_

