/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource
 *
 * Copyright (C) 2009 Firat Kiyak <firatkiyak@gmail.com>
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

#ifndef _OXMLI_LISTENERSTATE_NUMBERING_H_
#define _OXMLI_LISTENERSTATE_NUMBERING_H_

// Internal includes
#include <OXMLi_ListenerState.h>
#include <OXMLi_Types.h>
#include <OXML_Types.h>

// External includes
#include <vector>

/* \class OXMLi_ListenerState_Numbering
 * \brief This ListenerState parses the Document Numbering part.
*/
class OXMLi_ListenerState_Numbering : public OXMLi_ListenerState
{
public:
	OXMLi_ListenerState_Numbering();
	virtual ~OXMLi_ListenerState_Numbering();
	virtual void startElement (OXMLi_StartElementRequest * rqst) override;
	virtual void endElement (OXMLi_EndElementRequest * rqst) override;
	virtual void charData (OXMLi_CharDataRequest * rqst) override;

private:
	OXML_List* m_currentList;
	std::string m_currentNumId;
	std::string m_parentListId;

	/* w:num > w:lvlOverride capture: per-instance level overrides
	 * (startOverride and/or a replacement w:lvl) get cloned lists under
	 * a synthetic "9"+numId root applied at </w:num> */
	bool m_inLvlOverride;
	int m_overrideIlvl;
	int m_overrideStart; // -1 = none
	OXML_List* m_overrideLvl; // replacement lvl template, owned
	struct OverrideRec {
		int ilvl;
		int start;
		OXML_List* lvl;
	};
	std::vector<OverrideRec> m_numOverrides;

	void handleLevel(const gchar* ilvl);
	void handleFormattingType(const gchar* val);
	void applyNumOverrides();

};

#endif //_OXMLI_LISTENERSTATE_NUMBERING_H_

