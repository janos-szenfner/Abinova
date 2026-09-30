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

#ifndef _OXMLI_LISTENERSTATE_THEME_H_
#define _OXMLI_LISTENERSTATE_THEME_H_

// Internal includes
#include <OXMLi_ListenerState.h>
#include <OXMLi_Types.h>
#include <OXML_Types.h>
#include "OXML_Theme.h"

/* \class OXMLi_ListenerState_Theme
 * \brief This ListenerState parses the Theme part.
*/
class OXMLi_ListenerState_Theme : public OXMLi_ListenerState
{
public:
	virtual void startElement (OXMLi_StartElementRequest * rqst) override;
	virtual void endElement (OXMLi_EndElementRequest * rqst) override;
	virtual void charData (OXMLi_CharDataRequest * rqst) override;

private:
	OXML_SharedTheme m_theme;

	UT_Error _initializeTheme();
	std::string _getHexFromPreset(std::string preset);

	/* a:fmtScheme parsing — effectStyleLst shadows and lnStyleLst
	 * lines, stored on the theme at their 1-based position for
	 * wps:style *Ref resolution */
	bool m_bInEffectStyleLst = false;
	int  m_effectStyleIdx = 0;
	bool m_bInShdw = false;      // inside a:outerShdw of an effectStyle
	bool m_bShdwColor = false;   // inside the shadow's color element
	OXML_Theme::ThemeShadow m_shdw;
	bool m_bInLnStyleLst = false;
	int  m_lnStyleIdx = 0;
	bool m_bInThemeLn = false;   // inside a top-level a:ln of lnStyleLst
	OXML_Theme::ThemeLine m_ln;
};

#endif //_OXMLI_LISTENERSTATE_THEME_H_

