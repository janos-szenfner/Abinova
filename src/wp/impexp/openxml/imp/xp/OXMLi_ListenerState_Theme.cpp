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
#include "OXMLi_ListenerState_Theme.h"

// Internal includes
#include "OXML_Types.h"
#include "OXML_Document.h"
#include "OXML_Theme.h"

// Abinova includes
#include "ut_assert.h"
#include "ut_misc.h"

// External includes
#include <math.h>
#include <string>

void OXMLi_ListenerState_Theme::startElement (OXMLi_StartElementRequest * rqst)
{
	UT_return_if_fail(_error_if_fail( UT_OK == _initializeTheme() ));

	if (	nameMatches(rqst->pName, NS_A_KEY, "hslClr") || 
			nameMatches(rqst->pName, NS_A_KEY, "prstClr") || 
			nameMatches(rqst->pName, NS_A_KEY, "schemeClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "scrgbClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "srgbClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "sysClr") ) {

		std::string contextTag = rqst->context->at(rqst->context->size() - 2);
		bool bClrScheme = contextMatches(contextTag, NS_A_KEY, "clrScheme");
		/* fmtScheme colors: the color child of an effectStyle's
		 * a:outerShdw, or a fill color inside an lnStyleLst a:ln */
		bool bShdwClr = m_bInShdw &&
			contextMatches(rqst->context->back(), NS_A_KEY, "outerShdw");
		if (!bClrScheme && !bShdwClr && !m_bInThemeLn) return;

		std::string color = "";

		if (nameMatches(rqst->pName, NS_A_KEY, "hslClr")) {
			//parse Hue, Saturation, Luminance color model
		} else if (nameMatches(rqst->pName, NS_A_KEY, "prstClr")) {
			//parse preset color
			const gchar * val = attrMatches(NS_A_KEY, "val", rqst->ppAtts);
			UT_return_if_fail( this->_error_if_fail(val != nullptr) );
			color = _getHexFromPreset(val);
		} else if (nameMatches(rqst->pName, NS_A_KEY, "schemeClr")) {
			//parse scheme color
			const gchar * val = attrMatches(NS_A_KEY, "val", rqst->ppAtts);
			UT_return_if_fail( this->_error_if_fail(val != nullptr) );
			if (!strcmp(val, "lt1")) { color = m_theme->getColor(LIGHT1);
			} else if (!strcmp(val, "lt2")) { color = m_theme->getColor(LIGHT2);
			} else if (!strcmp(val, "dk1")) { color = m_theme->getColor(DARK1);
			} else if (!strcmp(val, "dk2")) { color = m_theme->getColor(DARK2);
			} else if (!strcmp(val, "accent1")) { color = m_theme->getColor(ACCENT1);
			} else if (!strcmp(val, "accent2")) { color = m_theme->getColor(ACCENT2);
			} else if (!strcmp(val, "accent3")) { color = m_theme->getColor(ACCENT3);
			} else if (!strcmp(val, "accent4")) { color = m_theme->getColor(ACCENT4);
			} else if (!strcmp(val, "accent5")) { color = m_theme->getColor(ACCENT5);
			} else if (!strcmp(val, "accent6")) { color = m_theme->getColor(ACCENT6);
			} else if (!strcmp(val, "hlink")) { color = m_theme->getColor(HYPERLINK);
			} else if (!strcmp(val, "folHlink")) { color = m_theme->getColor(FOLLOWED_HYPERLINK);
			} else if (!strcmp(val, "phClr")) { color = "phClr"; //placeholder — substituted by the *Ref color
			}
		} else if (nameMatches(rqst->pName, NS_A_KEY, "scrgbClr")) {
			//parse RGB color, percentage variant
			const gchar * r = attrMatches(NS_A_KEY, "r", rqst->ppAtts);
			const gchar * g = attrMatches(NS_A_KEY, "g", rqst->ppAtts);
			const gchar * b = attrMatches(NS_A_KEY, "b", rqst->ppAtts);
			UT_return_if_fail( this->_error_if_fail(r != nullptr && g != nullptr && b != nullptr ));
			// scRGB channels are thousandths of a percent; clamp out-of-gamut
			// values so out-of-range input cannot wrap.
			auto scrgbChan = [](const gchar * s) -> unsigned char {
				long v = lround(255.0 * (UT_convertDimensionless(s) / 100000.0));
				return static_cast<unsigned char>(v < 0 ? 0 : v > 255 ? 255 : v);
			};
			unsigned char dR = scrgbChan(r), dG = scrgbChan(g), dB = scrgbChan(b);

			UT_HashColor conv;
			const gchar * result = conv.setColor(dR, dG, dB);
			UT_ASSERT(result != nullptr);
			color = result;

		} else if (nameMatches(rqst->pName, NS_A_KEY, "srgbClr")) {
			//parse RGB color, hex variant
			const gchar * val = attrMatches(NS_A_KEY, "val", rqst->ppAtts);
			UT_return_if_fail( this->_error_if_fail(val != nullptr) );
			color = "#";
			color += val;
		} else if (nameMatches(rqst->pName, NS_A_KEY, "sysClr")) {
			//parse system color; for now we only worry about last computed color.
			const gchar * hexVal = attrMatches(NS_A_KEY, "lastClr", rqst->ppAtts);
			if(hexVal != nullptr) {
				color = "#";
				color += hexVal;
			}
		}

		if (bShdwClr)
		{
			m_bShdwColor = true;
			m_shdw.color = color;
			rqst->handled = true;
			return;
		}
		if (m_bInThemeLn)
		{
			m_ln.color = color;
			rqst->handled = true;
			return;
		}

		if (!color.compare("") || color[0] != '#') return;

		contextTag = rqst->context->back();

		if (contextMatches(contextTag, NS_A_KEY, "accent1")) {
			m_theme->setColor(ACCENT1, color);
		} else if (contextMatches(contextTag, NS_A_KEY, "accent2")) {
			m_theme->setColor(ACCENT2, color);
		} else if (contextMatches(contextTag, NS_A_KEY, "accent3")) {
			m_theme->setColor(ACCENT3, color);
		} else if (contextMatches(contextTag, NS_A_KEY, "accent4")) {
			m_theme->setColor(ACCENT4, color);
		} else if (contextMatches(contextTag, NS_A_KEY, "accent5")) {
			m_theme->setColor(ACCENT5, color);
		} else if (contextMatches(contextTag, NS_A_KEY, "accent6")) {
			m_theme->setColor(ACCENT6, color);
		} else if (contextMatches(contextTag, NS_A_KEY, "dk1")) {
			m_theme->setColor(DARK1, color);
		} else if (contextMatches(contextTag, NS_A_KEY, "dk2")) {
			m_theme->setColor(DARK2, color);
		} else if (contextMatches(contextTag, NS_A_KEY, "lt1")) {
			m_theme->setColor(LIGHT1, color);
		} else if (contextMatches(contextTag, NS_A_KEY, "lt2")) {
			m_theme->setColor(LIGHT2, color);
		} else if (contextMatches(contextTag, NS_A_KEY, "hlink")) {
			m_theme->setColor(HYPERLINK, color);
		} else if (contextMatches(contextTag, NS_A_KEY, "folHlink")) {
			m_theme->setColor(FOLLOWED_HYPERLINK, color);
		}

		rqst->handled = true;

	} else if (	nameMatches(rqst->pName, NS_A_KEY, "latin") ||
				nameMatches(rqst->pName, NS_A_KEY, "ea") ||
				nameMatches(rqst->pName, NS_A_KEY, "cs") ||
				nameMatches(rqst->pName, NS_A_KEY, "font") ) {
		const gchar * typeface = attrMatches(NS_A_KEY, "typeface", rqst->ppAtts);
		UT_return_if_fail( this->_error_if_fail(typeface != nullptr) );

		const gchar * script = nullptr;
		if ( nameMatches(rqst->pName, NS_A_KEY, "latin")) {
			script = "latin";
		} else if ( nameMatches(rqst->pName, NS_A_KEY, "ea")) {
			script = "ea";
		} else if ( nameMatches(rqst->pName, NS_A_KEY, "cs")) {
			script = "cs";
		} else {
			script = attrMatches(NS_A_KEY, "script", rqst->ppAtts);
			UT_return_if_fail( this->_error_if_fail(script != nullptr) );
		}
		//TODO: check for unicode compatibility for typeface name
		std::string contextTag = rqst->context->back();
		if (contextMatches(contextTag, NS_A_KEY, "majorFont")) {
			m_theme->setMajorFont(script, typeface);
		} else if (contextMatches(contextTag, NS_A_KEY, "minorFont")) {
			m_theme->setMinorFont(script, typeface);
		}

		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_A_KEY, "effectStyleLst")) {
		/* a:fmtScheme effect list — each a:effectStyle is positionally
		 * indexed (1-based) for wps:style/a:effectRef lookup */
		m_bInEffectStyleLst = true;
		m_effectStyleIdx = 0;
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_A_KEY, "effectStyle")) {
		if (m_bInEffectStyleLst)
			++m_effectStyleIdx;
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_A_KEY, "outerShdw")) {
		/* drop shadow inside an effectStyle's a:effectLst — its color
		 * child is captured while m_bInShdw, transforms via m_bShdwColor */
		if (m_bInEffectStyleLst && m_effectStyleIdx > 0)
		{
			m_bInShdw = true;
			m_shdw = OXML_Theme::ThemeShadow();
			const gchar * dist = attrMatches(NS_A_KEY, "dist", rqst->ppAtts);
			const gchar * dir = attrMatches(NS_A_KEY, "dir", rqst->ppAtts);
			const gchar * blur = attrMatches(NS_A_KEY, "blurRad", rqst->ppAtts);
			const gchar * rws = attrMatches(NS_A_KEY, "rotWithShape", rqst->ppAtts);
			if (dist)
				m_shdw.distPt = UT_convertDimensionless(dist) / 12700.0;
			if (blur)
				m_shdw.blurPt = UT_convertDimensionless(blur) / 12700.0;
			if (dir)
				m_shdw.dir = (int)UT_convertDimensionless(dir);
			if (rws)
				m_shdw.rotWithShape =
					(strcmp(rws, "0") && strcmp(rws, "false"));
		}
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_A_KEY, "alpha") ||
			   nameMatches(rqst->pName, NS_A_KEY, "lumMod") ||
			   nameMatches(rqst->pName, NS_A_KEY, "lumOff")) {
		/* transform children of the shadow color — a:alpha is the one
		 * themes actually use for effect shadows */
		if (m_bShdwColor && nameMatches(rqst->pName, NS_A_KEY, "alpha"))
		{
			const gchar * v = attrMatches(NS_A_KEY, "val", rqst->ppAtts);
			if (v)
				m_shdw.alpha = UT_convertDimensionless(v) / 100000.0;
		}
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_A_KEY, "lnStyleLst")) {
		m_bInLnStyleLst = true;
		m_lnStyleIdx = 0;
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_A_KEY, "ln")) {
		/* a:ln entries of lnStyleLst (w in EMU — 12700 EMU = 1pt) */
		if (m_bInLnStyleLst && rqst->context && !rqst->context->empty() &&
			contextMatches(rqst->context->back(), NS_A_KEY, "lnStyleLst"))
		{
			++m_lnStyleIdx;
			m_bInThemeLn = true;
			m_ln = OXML_Theme::ThemeLine();
			const gchar * w = attrMatches(NS_A_KEY, "w", rqst->ppAtts);
			if (w)
				m_ln.wPt = UT_convertDimensionless(w) / 12700.0;
			const gchar * cmpd = attrMatches(NS_A_KEY, "cmpd", rqst->ppAtts);
			if (cmpd && *cmpd && strcmp(cmpd, "sng"))
				m_ln.cmpd = cmpd;
		}
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_A_KEY, "prstDash")) {
		if (m_bInThemeLn)
		{
			const gchar * v = attrMatches(NS_A_KEY, "val", rqst->ppAtts);
			if (v)
			{
				if (!strcmp(v, "sysDash") || !strcmp(v, "dash"))
					m_ln.dash = "dashed";
				else if (!strcmp(v, "dot") || !strcmp(v, "sysDot"))
					m_ln.dash = "dotted";
				else if (!strcmp(v, "lgDash"))
					m_ln.dash = "longdash";
				else if (!strcmp(v, "dashDot") || !strcmp(v, "sysDashDot") ||
						 !strcmp(v, "lgDashDot"))
					m_ln.dash = "dashdot";
				else if (!strcmp(v, "sysDashDotDot") ||
						 !strcmp(v, "lgDashDotDot"))
					m_ln.dash = "dashdotdot";
				else
					m_ln.dash = "solid";
			}
		}
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_A_KEY, "noFill")) {
		/* <a:ln><a:noFill/></a:ln> — the referenced line is invisible */
		if (m_bInThemeLn)
			m_ln.dash = "none";
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_A_KEY, "effectLst") ||
			   nameMatches(rqst->pName, NS_A_KEY, "effectDag") ||
			   nameMatches(rqst->pName, NS_A_KEY, "solidFill") ||
			   nameMatches(rqst->pName, NS_A_KEY, "gradFill") ||
			   nameMatches(rqst->pName, NS_A_KEY, "fmtScheme") ||
			   nameMatches(rqst->pName, NS_A_KEY, "fillStyleLst") ||
			   nameMatches(rqst->pName, NS_A_KEY, "bgFillStyleLst")) {
		/* containers we consume quietly */
		rqst->handled = true;
	}
}

void OXMLi_ListenerState_Theme::endElement (OXMLi_EndElementRequest * rqst)
{
	if (	nameMatches(rqst->pName, NS_A_KEY, "hslClr") || 
			nameMatches(rqst->pName, NS_A_KEY, "prstClr") || 
			nameMatches(rqst->pName, NS_A_KEY, "schemeClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "scrgbClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "srgbClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "sysClr") ) {

		std::string contextTag = rqst->context->at(rqst->context->size() - 2);
		if (!contextMatches(contextTag, NS_A_KEY, "clrScheme")) return;
		rqst->handled = true;
	} else if (	nameMatches(rqst->pName, NS_A_KEY, "latin") ||
				nameMatches(rqst->pName, NS_A_KEY, "ea") ||
				nameMatches(rqst->pName, NS_A_KEY, "cs") ||
				nameMatches(rqst->pName, NS_A_KEY, "font") ) {

		std::string contextTag = rqst->context->back();
		if (contextMatches(contextTag, NS_A_KEY, "majorFont") && contextMatches(contextTag, NS_A_KEY, "minorFont"))
			return;
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_A_KEY, "effectStyleLst")) {
		m_bInEffectStyleLst = false;
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_A_KEY, "outerShdw")) {
		if (m_bInShdw)
		{
			m_bInShdw = false;
			m_bShdwColor = false;
			if (m_effectStyleIdx > 0)
				m_theme->setEffectShadow(m_effectStyleIdx, m_shdw);
		}
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_A_KEY, "lnStyleLst")) {
		m_bInLnStyleLst = false;
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_A_KEY, "ln")) {
		if (m_bInThemeLn)
		{
			m_bInThemeLn = false;
			if (m_lnStyleIdx > 0)
				m_theme->setLineStyle(m_lnStyleIdx, m_ln);
		}
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_A_KEY, "effectStyle") ||
			   nameMatches(rqst->pName, NS_A_KEY, "effectLst") ||
			   nameMatches(rqst->pName, NS_A_KEY, "effectDag") ||
			   nameMatches(rqst->pName, NS_A_KEY, "alpha") ||
			   nameMatches(rqst->pName, NS_A_KEY, "lumMod") ||
			   nameMatches(rqst->pName, NS_A_KEY, "lumOff") ||
			   nameMatches(rqst->pName, NS_A_KEY, "noFill") ||
			   nameMatches(rqst->pName, NS_A_KEY, "prstDash") ||
			   nameMatches(rqst->pName, NS_A_KEY, "solidFill") ||
			   nameMatches(rqst->pName, NS_A_KEY, "gradFill") ||
			   nameMatches(rqst->pName, NS_A_KEY, "fmtScheme") ||
			   nameMatches(rqst->pName, NS_A_KEY, "fillStyleLst") ||
			   nameMatches(rqst->pName, NS_A_KEY, "bgFillStyleLst")) {
		rqst->handled = true;
	}

}

void OXMLi_ListenerState_Theme::charData (OXMLi_CharDataRequest * /*rqst*/)
{
	UT_ASSERT ( UT_SHOULD_NOT_HAPPEN );
}

UT_Error OXMLi_ListenerState_Theme::_initializeTheme()
{
	if (m_theme.get() == nullptr) {
		OXML_Document * doc = OXML_Document::getInstance();
		UT_return_val_if_fail(_error_if_fail(doc != nullptr), UT_ERROR);
		m_theme = doc->getTheme();
		UT_return_val_if_fail(_error_if_fail(m_theme.get() != nullptr), UT_ERROR);
	}
	return UT_OK;
}

std::string OXMLi_ListenerState_Theme::_getHexFromPreset(std::string preset)
{
	UT_return_val_if_fail(preset.length() >= 3, "#000000");
	//First, we make the proper transformations.
	if (preset[0] == 'd' && preset[1] == 'k') {
		preset.insert(1, "ar"); //string must start with 'dark' to match, not 'dk'
	} else if (preset[0] == 'l' && preset[1] == 't') {
		preset.insert(1, "igh"); //string must start with 'light' to match, not 'lt'
	} else if (preset[0] == 'm' && preset[1] == 'e' && preset[2] == 'd') {
		preset.insert(3,"ium"); //string must start with 'medium' to match, not 'med'
	}

	//must not have capital letters
	std::string::iterator it;
	for (it = preset.begin(); it != preset.end(); it++) {
		(*it) = tolower((*it));
	}

	//Get the hex value and return as string
	UT_HashColor conv;
	const gchar * ret = conv.lookupNamedColor(preset.c_str());
	UT_ASSERT(ret != nullptr);
	return ret != nullptr ? ret : "#000000";
}

