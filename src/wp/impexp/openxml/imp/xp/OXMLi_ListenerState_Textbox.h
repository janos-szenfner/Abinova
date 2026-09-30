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

// External includes
#include <vector>
#include <map>
#include <utility>

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
	bool m_bInShadow = false; // inside a:outerShdw — colors go to frame-shadow-*
	std::string m_pendColor;
	bool m_bPendOutline = false;
	bool m_bPendShadow = false;
	// a:custGeom freeform capture -> shape-path prop
	bool m_inCustGeom = false;
	char m_pathCmd = 0;
	int m_pathPtN = 0;
	double m_custGeomW = 0.0;
	double m_custGeomH = 0.0;
	std::string m_shapePath;
	// a:gradFill stops -> fill-gradient descriptor "ang;pos:col;pos:col"
	bool m_inGradFill = false;
	std::string m_gradDesc;
	std::string m_gradPos;
	double m_lumMod = 1.0;
	double m_lumOff = 0.0;
	double m_tint = -1.0;
	double m_shade = -1.0;
	double m_alpha = -1.0;
	/* a:ln outline state for the current shape */
	bool m_bInOutline = false;
	bool m_bInOutlineFill = false;
	bool m_bHadExplicitLn = false;      // spPr carried its own a:ln
	bool m_bHadExplicitEffect = false;  // spPr carried its own a:effectLst
	std::string m_outlineColor;
	std::string m_outlineStyle;
	double m_outlineW = -1.0;
	std::string m_shapePrst;

	/* wps:style *Ref state — lnRef/effectRef idx is the 1-based
	 * position in the theme's lnStyleLst/effectStyleLst; fontRef idx
	 * is "minor"|"major". The refs' color child is captured through
	 * m_pendColor and lands in m_refColor. */
	int  m_lnRefIdx = 0;
	int  m_effectRefIdx = 0;
	bool m_fontRefMajor = false;
	bool m_bInRefColor = false;
	bool m_bPendRef = false;
	std::string m_refColor;
	/* fontRef defaults apply to runs parsed later in txbxContent —
	 * keyed by shape element so nested shapes stay separate */
	std::map<const OXML_Element*, std::pair<std::string,std::string>>
		m_fontRefByShape;
	void _applyLnRef(const OXML_SharedElement & shape);
	void _applyEffectRef(const OXML_SharedElement & shape);
	void _applyOutline(const OXML_SharedElement & shape, const std::string & color,
					   const std::string & style, double wPt);
	void _applyFontRefDefaults(OXML_Element * el, bool bParaColor,
							   bool bParaFont, const std::string & color,
							   const std::string & font);

	/* v:group coordinate space stack — children positions/sizes are
	 * in coordorigin/coordsize units and must be scaled into the
	 * group's real page box (nested groups compose) */
	struct VmlGroupX {
		double originX, originY; // coordorigin
		double scaleX, scaleY;   // style size / coordsize
		double offX, offY;       // group position on the page (pt)
	};
	std::vector<VmlGroupX> m_vmlGroupStack;
	double _vmlLenToPt(const std::string & v) const;
	std::string _vmlXformX(const std::string & v) const;
	std::string _vmlXformY(const std::string & v) const;
	std::string _vmlScaleX(const std::string & v) const;
	std::string _vmlScaleY(const std::string & v) const;
};

#endif //_OXMLI_LISTENERSTATE_TEXTBOX_H_

