/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource
 *
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

#ifndef _OXMLI_LISTENERSTATE_FONTTABLE_H_
#define _OXMLI_LISTENERSTATE_FONTTABLE_H_

// Internal includes
#include <OXMLi_ListenerState.h>
#include <OXMLi_Types.h>
#include <OXML_Types.h>
#include <OXML_FontManager.h>

/* \class OXMLi_ListenerState_FontTable
 * \brief This ListenerState parses the Font Table part
 * (word/fontTable.xml, ECMA-376 CT_Fonts): each <w:font> declaration's
 * substitution metadata (w:altName, w:panose1, w:charset, w:family,
 * w:pitch) is recorded in the document's OXML_FontManager so
 * getValidFont() can pick the declared substitute when a referenced
 * family is not installed.  Embedded font payloads (w:embedRegular/
 * w:embedBold/w:embedItalic/w:embedBoldItalic -> odttf parts) are
 * deliberately never loaded: an attacker-controlled sfnt binary is a
 * FreeType attack surface, and the fidelity loss is covered by
 * w:altName substitution plus the bundled font collection.
*/
class OXMLi_ListenerState_FontTable : public OXMLi_ListenerState
{
public:
	virtual void startElement (OXMLi_StartElementRequest * rqst) override;
	virtual void endElement (OXMLi_EndElementRequest * rqst) override;
	virtual void charData (OXMLi_CharDataRequest * rqst) override;
private:
	std::string m_curName;
	OXML_FontTableEntry m_curEntry;
	bool m_bWarnedEmbed = false;
};

#endif //_OXMLI_LISTENERSTATE_FONTTABLE_H_
