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

#ifndef _OXMLI_LISTENERSTATE_TEXTBOX_H_
#define _OXMLI_LISTENERSTATE_TEXTBOX_H_

// Internal includes
#include <OXMLi_ListenerState.h>
#include <OXMLi_Types.h>
#include <OXML_Types.h>
#include <OXML_Element_TextBox.h>

/* \class OXMLi_ListenerState_Textbox
 * \brief This ListenerState parses the Textboxes
*/
class OXMLi_ListenerState_Textbox : public OXMLi_ListenerState
{
public:
	OXMLi_ListenerState_Textbox();
	virtual void startElement (OXMLi_StartElementRequest * rqst) override;
	virtual void endElement (OXMLi_EndElementRequest * rqst) override;
	virtual void charData (OXMLi_CharDataRequest * rqst) override;
private:
	void _markFlatIfHdrFtr(OXML_SharedElement & tb);
	std::string m_style;
	bool m_bDmlTextbox = false;
	int m_wspDepth = 0;
	int m_vShapeDepth = 0; //VML v:rect/oval/line pushed a frame element
	bool m_bInShapeFill = false;
	bool m_bInStyleFill = false;
	std::string m_pendColor;
	bool m_bPendOutline = false;
	double m_lumMod = 1.0;
	double m_lumOff = 0.0;
	double m_tint = -1.0;
	double m_shade = -1.0;
	double m_alpha = -1.0;
	/* a:ln outline state for the current shape */
	bool m_bInOutline = false;
	bool m_bInOutlineFill = false;
	std::string m_outlineColor;
	std::string m_outlineStyle;
	double m_outlineW = -1.0;
	std::string m_shapePrst;
};

#endif //_OXMLI_LISTENERSTATE_TEXTBOX_H_

