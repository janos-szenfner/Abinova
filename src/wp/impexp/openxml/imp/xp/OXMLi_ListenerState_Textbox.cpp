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

// Class definition include
#include "OXMLi_ListenerState_Textbox.h"

// Internal includes
#include "OXML_Document.h"
#include "OXMLi_StreamListener.h"

// Abinova includes
#include "ut_assert.h"
#include "ut_misc.h"
#include "ut_units.h"

// External includes
#include <string>

OXMLi_ListenerState_Textbox::OXMLi_ListenerState_Textbox():
	OXMLi_ListenerState(), m_style("")
{

}

/* frame strux doesn't layout inside header/footer sections, so mark
 * textboxes parsed there to emit their contents inline */
void OXMLi_ListenerState_Textbox::_markFlatIfHdrFtr(OXML_SharedElement & tb)
{
	OXMLi_StreamListener * l = getListener();
	if (!l)
		return;
	if (l->getPartType() == HEADER_PART || l->getPartType() == FOOTER_PART)
		static_cast<OXML_Element_TextBox*>(tb.get())->setFlattened(true);
}

/* resolve a DrawingML color element (a:srgbClr / a:schemeClr) to a
 * bare "RRGGBB" hex string for our color props */
static std::string s_dmlColor(const std::string & name,
							  std::map<std::string, std::string>* atts)
{
	if (name == "A:srgbClr")
	{
		auto it = atts->find("A:val");
		if (it != atts->end())
			return it->second;
		return "";
	}
	if (name == "A:scrgbClr")
	{
		/* scrgbClr channels are 0–100000 percentages */
		auto ri = atts->find("A:r"), gi = atts->find("A:g"),
			 bi = atts->find("A:b");
		if (ri == atts->end() || gi == atts->end() || bi == atts->end())
			return "";
		char buf[8];
		snprintf(buf, sizeof(buf), "%02X%02X%02X",
				 (int)(UT_convertDimensionless(ri->second.c_str()) *
					   255.0 / 100000.0 + 0.5),
				 (int)(UT_convertDimensionless(gi->second.c_str()) *
					   255.0 / 100000.0 + 0.5),
				 (int)(UT_convertDimensionless(bi->second.c_str()) *
					   255.0 / 100000.0 + 0.5));
		return buf;
	}
	if (name == "A:sysClr")
	{
		/* prefer lastClr (the literal value at save time) over the
		 * system color name which we cannot resolve headlessly */
		auto it = atts->find("A:lastClr");
		if (it != atts->end())
			return it->second;
		it = atts->find("A:val");
		if (it != atts->end() && it->second == "windowText")
			return "000000";
		if (it != atts->end() && it->second == "window")
			return "FFFFFF";
		return "";
	}
	if (name == "A:prstClr")
	{
		/* preset color names — cover the common Office set */
		auto it = atts->find("A:val");
		if (it == atts->end())
			return "";
		static const std::map<std::string, std::string> prst = {
			{"black","000000"},{"white","FFFFFF"},{"red","FF0000"},
			{"green","008000"},{"blue","0000FF"},{"yellow","FFFF00"},
			{"gray","808080"},{"dkGray","A9A9A9"},{"ltGray","D3D3D3"},
			{"orange","FFA500"},{"purple","800080"},{"cyan","00FFFF"},
			{"magenta","FF00FF"},{"brown","A52A2A"},{"navy","000080"},
			{"silver","C0C0C0"},{"maroon","800000"},{"olive","808000"},
			{"teal","008080"},{"lime","00FF00"},{"pink","FFC0CB"},
			{"gold","FFD700"},{"violet","EE82EE"},{"indigo","4B0082"}
		};
		auto c = prst.find(it->second);
		return c != prst.end() ? c->second : "";
	}
	if (name == "A:schemeClr")
	{
		auto it = atts->find("A:val");
		if (it == atts->end())
			return "";
		const std::string & val = it->second;
		OXML_Document * doc = OXML_Document::getInstance();
		if (!doc || !doc->getTheme())
			return "";
		/* bg1/bg2/tx1/tx2 map onto dk/lt slots per ECMA-376 */
		OXML_ColorName cn = LIGHT1;
		if (val == "lt1" || val == "bg1")      cn = LIGHT1;
		else if (val == "lt2" || val == "bg2") cn = LIGHT2;
		else if (val == "dk1" || val == "tx1") cn = DARK1;
		else if (val == "dk2" || val == "tx2") cn = DARK2;
		else if (val == "accent1") cn = ACCENT1;
		else if (val == "accent2") cn = ACCENT2;
		else if (val == "accent3") cn = ACCENT3;
		else if (val == "accent4") cn = ACCENT4;
		else if (val == "accent5") cn = ACCENT5;
		else if (val == "accent6") cn = ACCENT6;
		else if (val == "hlink")   cn = HYPERLINK;
		else if (val == "folHlink") cn = FOLLOWED_HYPERLINK;
		else return "";
		return doc->getTheme()->getColor(cn);
	}
	return "";
}

/* apply the DrawingML color transforms recorded while the color
 * element was open (lumMod/lumOff/tint/shade/alpha) and return the
 * final hex color */
static std::string s_transformColor(const std::string & hex,
									double lumMod, double lumOff,
									double tint, double shade,
									double alpha)
{
	if (hex.size() < 6)
		return hex;
	int r = 0, g = 0, b = 0;
	if (sscanf(hex.c_str(), "%02x%02x%02x", &r, &g, &b) != 3)
		return hex;
	auto xf = [&](int c) -> int {
		double v = c;
		if (shade >= 0.0)
			v *= shade;              /* darken toward black */
		if (tint >= 0.0)
			v = v * tint + 255.0 * (1.0 - tint); /* lighten toward white */
		v = v * lumMod + 255.0 * lumOff;
		if (alpha >= 0.0)
			v = v * alpha + 255.0 * (1.0 - alpha); /* over white */
		if (v < 0.0) v = 0.0;
		if (v > 255.0) v = 255.0;
		return (int)(v + 0.5);
	};
	char buf[8];
	snprintf(buf, sizeof(buf), "%02X%02X%02X", xf(r), xf(g), xf(b));
	return buf;
}

void OXMLi_ListenerState_Textbox::startElement (OXMLi_StartElementRequest * rqst)
{
	if(nameMatches(rqst->pName, "wps", "wsp"))
	{
		/* a wordprocessing shape (the wps namespace isn't registered,
		 * so the name stays literal). It may be a colored panel, the
		 * host of a textbox, or a group child — treat it as a frame
		 * element so its fill and content land on the page. */
		OXML_SharedElement shapeElem(new OXML_Element_TextBox(""));
		_markFlatIfHdrFtr(shapeElem);
		rqst->stck->push(shapeElem);
		m_shapePrst.clear();
		++m_wspDepth;
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "grpFill"))
	{
		/* group fill — children without their own fill inherit it.
		 * The color resolves onto the w:drawing element and is copied
		 * to fill-less children at flush */
		if (rqst->context && !rqst->context->empty() &&
			rqst->context->back() == "wpg:grpSpPr")
			m_bInShapeFill = true;
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "prstGeom"))
	{
		/* preset geometry name — prstGeom="line" shapes are zero-size
		 * rules drawn with their outline; remembered for a:ln */
		if (rqst->context && !rqst->context->empty() &&
			rqst->context->back() == "wps:spPr")
		{
			const gchar * prst =
				attrMatches(NS_A_KEY, "prst", rqst->ppAtts);
			if (prst)
				m_shapePrst = prst;
		}
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, "wps", "bodyPr"))
	{
		/* textbox text insets (EMU; Word defaults 91440/45720) map to
		 * per-side frame padding; "xpad"/"ypad" keep the max for
		 * readers that only know the symmetric property */
		if (rqst->stck && !rqst->stck->empty())
		{
			auto emu = [&](const char * n) -> double {
				auto it = rqst->ppAtts->find(n);
				return it != rqst->ppAtts->end() ?
					UT_convertDimensionless(it->second.c_str()) : -1.0;
			};
			double l = emu("wps:lIns"), r = emu("wps:rIns");
			double t = emu("wps:tIns"), b = emu("wps:bIns");
			char buf[24];
			if (l >= 0.0 || r >= 0.0)
			{
				double lEff = l >= 0.0 ? l : 0.0;
				double rEff = r >= 0.0 ? r : 0.0;
				g_snprintf(buf, sizeof(buf), "%.4fin",
						   (lEff > rEff ? lEff : rEff) / 914400.0);
				rqst->stck->top()->setProperty("xpad", buf);
				g_snprintf(buf, sizeof(buf), "%.4fin", lEff / 914400.0);
				rqst->stck->top()->setProperty("xpad-left", buf);
				g_snprintf(buf, sizeof(buf), "%.4fin", rEff / 914400.0);
				rqst->stck->top()->setProperty("xpad-right", buf);
			}
			if (t >= 0.0 || b >= 0.0)
			{
				double tEff = t >= 0.0 ? t : 0.0;
				double bEff = b >= 0.0 ? b : 0.0;
				g_snprintf(buf, sizeof(buf), "%.4fin",
						   (tEff > bEff ? tEff : bEff) / 914400.0);
				rqst->stck->top()->setProperty("ypad", buf);
				g_snprintf(buf, sizeof(buf), "%.4fin", tEff / 914400.0);
				rqst->stck->top()->setProperty("ypad-top", buf);
				g_snprintf(buf, sizeof(buf), "%.4fin", bEff / 914400.0);
				rqst->stck->top()->setProperty("ypad-bottom", buf);
			}

			/* vertical text alignment inside the box and writing
			 * direction for vertical text */
			auto it = rqst->ppAtts->find("wps:anchor");
			if (it != rqst->ppAtts->end())
			{
				std::string a(it->second); // t|ctr|b|just|dist
				if (a == "ctr") a = "center";
				else if (a == "b") a = "bottom";
				else if (a == "t") a = "top";
				rqst->stck->top()->setProperty("frame-valign", a.c_str());
			}
			it = rqst->ppAtts->find("wps:vert");
			if (it != rqst->ppAtts->end())
				rqst->stck->top()->setProperty("frame-text-direction",
											 it->second.c_str());
		}
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "spAutoFit") ||
		nameMatches(rqst->pName, NS_A_KEY, "noAutofit"))
	{
		/* textbox autofit: spAutoFit lets the frame grow with its text */
		if (nameMatches(rqst->pName, NS_A_KEY, "spAutoFit") &&
			rqst->stck && !rqst->stck->empty())
		{
			rqst->stck->top()->setProperty("frame-expand-height", "1");
		}
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "headEnd") ||
		nameMatches(rqst->pName, NS_A_KEY, "tailEnd"))
	{
		/* line arrowheads (children of a:ln) */
		if (rqst->stck && !rqst->stck->empty())
		{
			std::string base =
				nameMatches(rqst->pName, NS_A_KEY, "headEnd") ?
					"line-start-arrow" : "line-end-arrow";
			const gchar * type = attrMatches(NS_A_KEY, "type", rqst->ppAtts);
			const gchar * w = attrMatches(NS_A_KEY, "w", rqst->ppAtts);
			const gchar * len = attrMatches(NS_A_KEY, "len", rqst->ppAtts);
			if (type && *type)
				rqst->stck->top()->setProperty(base.c_str(), type);
			if (w && *w)
				rqst->stck->top()->setProperty((base + "-w").c_str(), w);
			if (len && *len)
				rqst->stck->top()->setProperty((base + "-len").c_str(), len);
		}
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "outerShdw"))
	{
		/* drop shadow inside a:effectLst — preserved as frame-shadow-* */
		if (rqst->stck && !rqst->stck->empty())
		{
			rqst->stck->top()->setProperty("frame-shadow", "outer");
			const gchar * dist = attrMatches(NS_A_KEY, "dist", rqst->ppAtts);
			const gchar * dir = attrMatches(NS_A_KEY, "dir", rqst->ppAtts);
			const gchar * blur = attrMatches(NS_A_KEY, "blurRad", rqst->ppAtts);
			if (dist && *dist)
			{
				char buf[24];
				g_snprintf(buf, sizeof(buf), "%.2fpt",
						   UT_convertDimensionless(dist) / 12700.0);
				rqst->stck->top()->setProperty("frame-shadow-offset", buf);
			}
			if (dir && *dir)
				rqst->stck->top()->setProperty("frame-shadow-dir", dir);
			if (blur && *blur)
			{
				char buf[24];
				g_snprintf(buf, sizeof(buf), "%.2fpt",
						   UT_convertDimensionless(blur) / 12700.0);
				rqst->stck->top()->setProperty("frame-shadow-blur", buf);
			}
		}
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "prstTxWarp"))
	{
		const gchar * prst = attrMatches(NS_A_KEY, "prst", rqst->ppAtts);
		if (prst && *prst && rqst->stck && !rqst->stck->empty())
			rqst->stck->top()->setProperty("text-warp", prst);
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "srcRect"))
	{
		/* picture fill crop: l/t/r/b in 1000ths of a percent */
		if (rqst->stck && !rqst->stck->empty() && rqst->stck->top())
		{
			std::string rect;
			const char* sides[] = {"l", "t", "r", "b"};
			for (const char* s : sides)
			{
				const gchar * v = attrMatches(NS_A_KEY, s, rqst->ppAtts);
				rect += (v && *v) ? v : "0";
				rect += " ";
			}
			rect.pop_back();
			rqst->stck->top()->setProperty("image-src-rect", rect.c_str());
		}
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "custGeom"))
	{
		m_inCustGeom = true;
		m_shapePath.clear();
		m_custGeomW = m_custGeomH = 0.0;
		m_pathCmd = 0;
		rqst->handled = true;
		return;
	}
	if (m_inCustGeom && nameMatches(rqst->pName, NS_A_KEY, "path"))
	{
		const gchar * w = attrMatches(NS_A_KEY, "w", rqst->ppAtts);
		const gchar * h = attrMatches(NS_A_KEY, "h", rqst->ppAtts);
		m_custGeomW = w ? UT_convertDimensionless(w) : 0.0;
		m_custGeomH = h ? UT_convertDimensionless(h) : 0.0;
		rqst->handled = true;
		return;
	}
	if (m_inCustGeom &&
		(nameMatches(rqst->pName, NS_A_KEY, "moveTo") ||
		 nameMatches(rqst->pName, NS_A_KEY, "lnTo") ||
		 nameMatches(rqst->pName, NS_A_KEY, "cubicBezTo") ||
		 nameMatches(rqst->pName, NS_A_KEY, "quadBezTo")))
	{
		if (nameMatches(rqst->pName, NS_A_KEY, "moveTo"))
			m_pathCmd = 'M';
		else if (nameMatches(rqst->pName, NS_A_KEY, "lnTo"))
			m_pathCmd = 'L';
		else if (nameMatches(rqst->pName, NS_A_KEY, "cubicBezTo"))
			m_pathCmd = 'C';
		else
			m_pathCmd = 'Q';
		m_pathPtN = 0;
		rqst->handled = true;
		return;
	}
	if (m_inCustGeom && nameMatches(rqst->pName, NS_A_KEY, "close"))
	{
		m_shapePath += "Z ";
		m_pathCmd = 0;
		rqst->handled = true;
		return;
	}
	if (m_inCustGeom && nameMatches(rqst->pName, NS_A_KEY, "pt"))
	{
		const gchar * x = attrMatches(NS_A_KEY, "x", rqst->ppAtts);
		const gchar * y = attrMatches(NS_A_KEY, "y", rqst->ppAtts);
		if (m_pathCmd && x && y)
		{
			//normalize into a 0..1000 box
			double sx = m_custGeomW > 0.0 ?
				UT_convertDimensionless(x) * 1000.0 / m_custGeomW :
				UT_convertDimensionless(x);
			double sy = m_custGeomH > 0.0 ?
				UT_convertDimensionless(y) * 1000.0 / m_custGeomH :
				UT_convertDimensionless(y);
			char buf[64];
			if (m_pathPtN == 0)
				g_snprintf(buf, sizeof(buf), "%c %.1f %.1f ",
						   m_pathCmd, sx, sy);
			else
				g_snprintf(buf, sizeof(buf), "%.1f %.1f ", sx, sy);
			m_shapePath += buf;
			m_pathPtN++;
		}
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "ln"))
	{
		/* shape outline (border). w is EMU — 12700 EMU = 1pt */
		if (rqst->context && !rqst->context->empty() &&
			rqst->context->back() == "wps:spPr")
		{
			m_bInOutline = true;
			m_outlineColor.clear();
			m_outlineStyle = "solid";
			const gchar * w = attrMatches(NS_A_KEY, "w", rqst->ppAtts);
			m_outlineW = w ? UT_convertDimensionless(w) / 12700.0 : -1.0;
		}
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "prstDash"))
	{
		if (m_bInOutline)
		{
			const gchar * v = attrMatches(NS_A_KEY, "val", rqst->ppAtts);
			if (v)
			{
				if (!strcmp(v, "solid") || !strcmp(v, "sysDash") ||
					!strcmp(v, "dash"))
					m_outlineStyle = !strcmp(v, "solid") ? "solid" : "dashed";
				else if (!strcmp(v, "dot") || !strcmp(v, "sysDot"))
					m_outlineStyle = "dotted";
				else if (!strcmp(v, "lgDash"))
					m_outlineStyle = "longdash";
				else if (!strcmp(v, "dashDot") ||
						 !strcmp(v, "sysDashDot") ||
						 !strcmp(v, "lgDashDot"))
					m_outlineStyle = "dashdot";
				else if (!strcmp(v, "sysDashDotDot") ||
						 !strcmp(v, "lgDashDotDot"))
					m_outlineStyle = "dashdotdot";
			}
		}
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "solidFill") ||
		nameMatches(rqst->pName, NS_A_KEY, "gradFill") ||
		nameMatches(rqst->pName, NS_A_KEY, "noFill"))
	{
		/* only the shape's own fill (direct wps:spPr child) is a
		 * panel color — fills under a:ln are outlines, fills under
		 * rPr are text colors. A gradient is approximated by its
		 * first stop color */
		if (rqst->context && !rqst->context->empty())
		{
			if (rqst->context->back() == "wps:spPr")
			{
				m_bInShapeFill = !nameMatches(rqst->pName, NS_A_KEY, "noFill");
				m_inGradFill = nameMatches(rqst->pName, NS_A_KEY, "gradFill");
				if (m_inGradFill)
				{
					m_gradDesc.clear();
					m_gradPos.clear();
				}
			}
			else if (m_bInOutline && rqst->context->back() == "A:ln")
			{
				if (nameMatches(rqst->pName, NS_A_KEY, "noFill"))
					m_outlineStyle = "none";
				else
					m_bInOutlineFill = true;
			}
		}
		rqst->handled = true;
		return;
	}
	if (m_inGradFill && nameMatches(rqst->pName, NS_A_KEY, "gs"))
	{
		const gchar * pos = attrMatches(NS_A_KEY, "pos", rqst->ppAtts);
		m_gradPos = pos ? pos : "0";
		rqst->handled = true;
		return;
	}
	if (m_inGradFill && nameMatches(rqst->pName, NS_A_KEY, "lin"))
	{
		const gchar * ang = attrMatches(NS_A_KEY, "ang", rqst->ppAtts);
		if (ang && *ang)
		{
			std::string a("lin:");
			a += ang;
			a += ",";
			m_gradDesc = a + m_gradDesc;
		}
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "fillRef"))
	{
		/* wps:style/a:fillRef — theme style reference fill. idx>0
		 * means "fill with the referenced scheme color" (variant
		 * shading ignored, base color used) */
		const gchar * idx = attrMatches(NS_A_KEY, "idx", rqst->ppAtts);
		if (rqst->context && !rqst->context->empty() &&
			rqst->context->back() == "wps:style" &&
			idx && strcmp(idx, "0"))
			m_bInStyleFill = true;
		rqst->handled = true;
		return;
	}
	if ((m_bInShapeFill || m_bInStyleFill || m_bInOutlineFill) &&
		(nameMatches(rqst->pName, NS_A_KEY, "srgbClr") ||
		 nameMatches(rqst->pName, NS_A_KEY, "schemeClr") ||
		 nameMatches(rqst->pName, NS_A_KEY, "sysClr") ||
		 nameMatches(rqst->pName, NS_A_KEY, "prstClr") ||
		 nameMatches(rqst->pName, NS_A_KEY, "scrgbClr")))
	{
		/* don't write yet — lumMod/lumOff/tint/shade/alpha children
		 * transform the color; resolve at the element's end tag */
		std::string color = s_dmlColor(rqst->pName, rqst->ppAtts);
		if (!color.empty() && color[0] == '#')
			color.erase(0, 1);
		m_pendColor = color;
		m_bPendOutline = m_bInOutlineFill;
		m_lumMod = 1.0; m_lumOff = 0.0;
		m_tint = -1.0; m_shade = -1.0; m_alpha = -1.0;
		rqst->handled = true;
		return;
	}
	if (!m_pendColor.empty() &&
		(nameMatches(rqst->pName, NS_A_KEY, "lumMod") ||
		 nameMatches(rqst->pName, NS_A_KEY, "lumOff") ||
		 nameMatches(rqst->pName, NS_A_KEY, "tint") ||
		 nameMatches(rqst->pName, NS_A_KEY, "shade") ||
		 nameMatches(rqst->pName, NS_A_KEY, "alpha")))
	{
		const gchar * v = attrMatches(NS_A_KEY, "val", rqst->ppAtts);
		if (v)
		{
			double f = UT_convertDimensionless(v) / 100000.0;
			if (rqst->pName == "A:lumMod")      m_lumMod = f;
			else if (rqst->pName == "A:lumOff") m_lumOff = f;
			else if (rqst->pName == "A:tint")   m_tint = f;
			else if (rqst->pName == "A:shade")  m_shade = f;
			else if (rqst->pName == "A:alpha")  m_alpha = f;
		}
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "off") ||
		nameMatches(rqst->pName, NS_A_KEY, "ext") ||
		nameMatches(rqst->pName, NS_A_KEY, "chOff") ||
		nameMatches(rqst->pName, NS_A_KEY, "chExt"))
	{
		/* a:xfrm children. Inside a group's wpg:grpSpPr they define the
		 * group's rect (a:ext) and child coordinate space (a:chOff +
		 * a:chExt); inside a group child's wps:spPr they give the
		 * child's position in that space. Values stay raw EMU —
		 * resolution happens at addToPT when the page size is known. */
		bool bInGroup = false;
		bool bGrpSpPr = false;
		if (rqst->context)
		{
			for (const auto & c : *rqst->context)
				if (c == "wpg:wgp")
					bInGroup = true;
			if (rqst->context->size() >= 3)
				bGrpSpPr = (rqst->context->at(rqst->context->size() - 3) ==
							"wpg:grpSpPr");
		}
		if ((bInGroup || bGrpSpPr) && rqst->stck && !rqst->stck->empty())
		{
			OXML_SharedElement top = rqst->stck->top();
			auto att = [&](const char * n) -> const char * {
				auto it = rqst->ppAtts->find(n);
				return it != rqst->ppAtts->end() ? it->second.c_str() : nullptr;
			};
			if (bGrpSpPr)
			{
				/* group transform lands on the w:drawing element so all
				 * its children share it. a:off is the group's own
				 * offset in the parent space — kept under a distinct
				 * name so the wrapper never reads it as a child pos */
				const char * x = att("A:x"), * y = att("A:y");
				const char * cx = att("A:cx"), * cy = att("A:cy");
				if (rqst->pName == "A:chOff")
				{
					if (x) top->setProperty("grp-chOffX", x);
					if (y) top->setProperty("grp-chOffY", y);
				}
				else if (rqst->pName == "A:chExt")
				{
					if (cx) top->setProperty("grp-chExtX", cx);
					if (cy) top->setProperty("grp-chExtY", cy);
				}
				else if (rqst->pName == "A:ext")
				{
					if (cx) top->setProperty("grp-extX", cx);
					if (cy) top->setProperty("grp-extY", cy);
				}
				else if (rqst->pName == "A:off")
				{
					if (x) top->setProperty("grp-offX", x);
					if (y) top->setProperty("grp-offY", y);
				}
			}
			else if (bInGroup)
			{
				const char * x = att("A:x"), * y = att("A:y");
				const char * cx = att("A:cx"), * cy = att("A:cy");
				if (rqst->pName == "A:off")
				{
					if (x) top->setProperty("grp-xpos", x);
					if (y) top->setProperty("grp-ypos", y);
				}
				else if (rqst->pName == "A:ext")
				{
					if (cx) top->setProperty("grp-width", cx);
					if (cy) top->setProperty("grp-height", cy);
				}
			}
			else if (rqst->context->size() >= 3 &&
					 rqst->context->at(rqst->context->size() - 3) ==
						 "wps:spPr")
			{
				/* top-level shape's own extent — used for line
				 * direction and as the frame size fallback */
				const char * cx = att("A:cx"), * cy = att("A:cy");
				if (rqst->pName == "A:ext")
				{
					if (cx) top->setProperty("shape-w", cx);
					if (cy) top->setProperty("shape-h", cy);
				}
			}
		}
		rqst->handled = true;
		return;
	}
	if(nameMatches(rqst->pName, NS_V_KEY, "group"))
	{
		/* v:group is a pure coordinate-space container: its own
		 * style positions/sizes the group box on the page, while
		 * children use coordorigin/coordsize units. Record the
		 * transform (composed with any enclosing group) so child
		 * style values can be mapped to page points. No element is
		 * pushed — children emit their own frames. */
		VmlGroupX g;
		g.originX = 0.0; g.originY = 0.0;
		g.scaleX = 1.0;  g.scaleY = 1.0;
		g.offX = 0.0;    g.offY = 0.0;

		const gchar * cs = attrMatches(NS_V_KEY, "coordsize", rqst->ppAtts);
		const gchar * co = attrMatches(NS_V_KEY, "coordorigin", rqst->ppAtts);
		double csizeW = 1000.0, csizeH = 1000.0;
		if (co)
			sscanf(co, "%lf%*[, ]%lf", &g.originX, &g.originY);
		if (cs)
			sscanf(cs, "%lf%*[, ]%lf", &csizeW, &csizeH);

		double ml = 0.0, mt = 0.0, w = 0.0, h = 0.0;
		const gchar * style = attrMatches(NS_V_KEY, "style", rqst->ppAtts);
		if (style)
		{
			std::string s(style);
			size_t pos = 0;
			while (pos < s.length())
			{
				size_t end = s.find(';', pos);
				if (end == std::string::npos)
					end = s.length();
				std::string kv = s.substr(pos, end - pos);
				size_t colon = kv.find(':');
				if (colon != std::string::npos)
				{
					std::string k = kv.substr(0, colon);
					std::string v = kv.substr(colon + 1);
					while (!k.empty() && (k[0] == ' ' || k[0] == '\t')) k.erase(0, 1);
					if (k == "margin-left" || k == "left")
						ml = _vmlLenToPt(v);
					else if (k == "margin-top" || k == "top")
						mt = _vmlLenToPt(v);
					else if (k == "width")
						w = _vmlLenToPt(v);
					else if (k == "height")
						h = _vmlLenToPt(v);
				}
				pos = end + 1;
			}
		}

		/* group position/size are in the PARENT group's coord space */
		if (!m_vmlGroupStack.empty())
		{
			const VmlGroupX & p = m_vmlGroupStack.back();
			ml = p.offX + (ml - p.originX) * p.scaleX;
			mt = p.offY + (mt - p.originY) * p.scaleY;
			w *= p.scaleX;
			h *= p.scaleY;
		}
		g.offX = ml;
		g.offY = mt;
		if (csizeW != 0.0 && w != 0.0)
			g.scaleX = w / csizeW;
		if (csizeH != 0.0 && h != 0.0)
			g.scaleY = h / csizeH;
		m_vmlGroupStack.push_back(g);
		rqst->handled = true;
		return;
	}
	if(nameMatches(rqst->pName, NS_V_KEY, "shape") ||
		nameMatches(rqst->pName, NS_V_KEY, "rect") ||
		nameMatches(rqst->pName, NS_V_KEY, "roundrect") ||
		nameMatches(rqst->pName, NS_V_KEY, "oval") ||
		nameMatches(rqst->pName, NS_V_KEY, "line") ||
		nameMatches(rqst->pName, NS_V_KEY, "polyline"))
	{
		const gchar* style = attrMatches(NS_V_KEY, "style", rqst->ppAtts);
		if(style)
		{
			m_style = style;
		}

		/* v:rect/roundrect/oval/line/polyline are standalone fallback
		 * shapes (no v:imagedata/v:textbox container to push a frame
		 * element), so push a frame here; a nested v:textbox merges
		 * its content in. v:shape keeps the lazy behavior (its
		 * v:textbox/imagedata child owns the frame). */
		if (!nameMatches(rqst->pName, NS_V_KEY, "shape"))
		{
			OXML_SharedElement vShape(new OXML_Element_TextBox(""));
			_markFlatIfHdrFtr(vShape);
			vShape->setProperty("frame-type", "textbox");

			const gchar* fill = attrMatches(NS_V_KEY, "fillcolor", rqst->ppAtts);
			const gchar* strok = attrMatches(NS_V_KEY, "strokecolor", rqst->ppAtts);
			const gchar* filled = attrMatches(NS_V_KEY, "filled", rqst->ppAtts);
			const gchar* stroked = attrMatches(NS_V_KEY, "stroked", rqst->ppAtts);
			if (filled && !strcmp(filled, "f"))
				vShape->setProperty("bg-style", "0");
			else if (fill && *fill && strcmp(fill, "none") && fill[0] == '#')
			{
				vShape->setProperty("background-color", fill + 1);
				vShape->setProperty("bg-style", "1");
			}
			if (stroked && !strcmp(stroked, "f"))
			{
				vShape->setProperty("bot-style", "none");
				vShape->setProperty("top-style", "none");
				vShape->setProperty("left-style", "none");
				vShape->setProperty("right-style", "none");
			}
			else if (strok && *strok)
			{
				std::string sc(strok);
				if (sc[0] == '#')
					sc.erase(0, 1);
				vShape->setProperty("bot-color", sc.c_str());
				vShape->setProperty("top-color", sc.c_str());
				vShape->setProperty("left-color", sc.c_str());
				vShape->setProperty("right-color", sc.c_str());
			}

			//VML inline style: position/margins/size/z-index
			if (style)
			{
				std::string s(style);
				size_t pos = 0;
				while (pos < s.length())
				{
					size_t end = s.find(';', pos);
					if (end == std::string::npos)
						end = s.length();
					std::string kv = s.substr(pos, end - pos);
					size_t colon = kv.find(':');
					if (colon != std::string::npos)
					{
						std::string k = kv.substr(0, colon);
						std::string v = kv.substr(colon + 1);
						if (k == "margin-left" || k == "left")
							vShape->setProperty("xpos", _vmlXformX(v).c_str());
						else if (k == "margin-top" || k == "top")
							vShape->setProperty("ypos", _vmlXformY(v).c_str());
						else if (k == "width")
							vShape->setProperty("frame-width", _vmlScaleX(v).c_str());
						else if (k == "height")
							vShape->setProperty("frame-height", _vmlScaleY(v).c_str());
						else if (k == "z-index")
							vShape->setProperty("frame-stack-order", v.c_str());
						else if (k == "mso-wrap-style" && v == "none")
							vShape->setProperty("wrap-mode", "wrapped-topbot");
					}
					pos = end + 1;
				}
				vShape->setProperty("position-to", "page-above-text");
			}

			rqst->stck->push(vShape);
			m_vShapeDepth++;
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_V_KEY, "textbox"))
	{
		if (m_vShapeDepth > 0)
		{
			//shape frame already on the stack - merge textbox content into it
			rqst->handled = true;
			return;
		}
		OXML_SharedElement textboxElem(new OXML_Element_TextBox(""));
		_markFlatIfHdrFtr(textboxElem);

		if(m_style.compare(""))
		{
			//parse and apply style here
			std::string attrName("");
			std::string attrValue("");
			size_t attrStart = 0;
			size_t attrEnd = 0;
			while(attrStart < m_style.length())
			{
				attrEnd = m_style.find(';', attrStart);
				if(attrEnd == std::string::npos)
				{
					//this should be the last attribute
					attrEnd = m_style.length(); 
				}
				std::string attrNameValPair = m_style.substr(attrStart, attrEnd-attrStart);
				size_t seperator = attrNameValPair.find(':');
				if(seperator != std::string::npos)
				{
					attrName = attrNameValPair.substr(0, seperator);
					attrValue = attrNameValPair.substr(seperator+1);
					
					//convert and apply attributes here
					if(!attrName.compare("width"))
					{
						textboxElem->setProperty("frame-width", _vmlScaleX(attrValue).c_str());
					}
					else if(!attrName.compare("height"))
					{
						textboxElem->setProperty("frame-height", _vmlScaleY(attrValue).c_str());
					}
					//TODO: more attributes coming
				}	
				//finally update the start point for the next attribute
				attrStart = attrEnd+1;
			}
		}
		
		rqst->stck->push(textboxElem);
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "txbxContent"))
	{
		/* DrawingML textboxes (<wps:txbx><w:txbxContent>) have no
		 * <v:textbox> wrapper to push a container element onto the
		 * stack, so push it here. VML already pushed one at
		 * <v:textbox>. The wps: prefix is not a registered
		 * namespace key, so the context tag stays literal. */
		std::string contextTag;
		if (rqst->context && !rqst->context->empty())
			contextTag = rqst->context->back();
		if (contextTag == "wps:txbx")
		{
			if (m_wspDepth > 0)
			{
				/* a wps:wsp frame element is already on the stack —
				 * its txbxContent paragraphs append directly to it
				 * instead of nesting a second frame */
			}
			else
			{
				OXML_SharedElement textboxElem(new OXML_Element_TextBox(""));
				_markFlatIfHdrFtr(textboxElem);
				rqst->stck->push(textboxElem);
				m_bDmlTextbox = true;
			}
		}
		rqst->handled = true;
	}
}

void OXMLi_ListenerState_Textbox::endElement (OXMLi_EndElementRequest * rqst)
{
	if(nameMatches(rqst->pName, "wps", "wsp"))
	{
		if (m_wspDepth > 0)
			--m_wspDepth;
		if (!rqst->stck || rqst->stck->empty())
		{
			rqst->handled = false;
			return;
		}
		OXML_SharedElement shape = rqst->stck->top();
		rqst->stck->pop();
		if (rqst->stck->empty())
		{
			if (!rqst->sect_stck || rqst->sect_stck->empty())
			{
				rqst->handled = false;
				return;
			}
			OXML_SharedSection sect = rqst->sect_stck->top();
			rqst->handled = (sect->appendElement(shape) == UT_OK);
			return;
		}
		OXML_SharedElement parent = rqst->stck->top();
		/* group children carry child-space offsets in grp-* props plus
		 * the parent's anchor geometry as base-* — the final page
		 * position is resolved in addToPT once the page size is known.
		 * Top-level shapes take the anchor geometry the image listener
		 * recorded on the w:drawing element directly. */
		const gchar * v = nullptr;
		const gchar * gv = nullptr;
		if (shape->getProperty("grp-xpos", gv) == UT_OK && gv)
		{
			static const char * grpShare[] = {
				"grp-chOffX", "grp-chOffY", "grp-chExtX", "grp-chExtY",
				"grp-extX", "grp-extY", "grp-offX", "grp-offY" };
			static const char * baseSrc[] = {
				"xpos", "ypos", "halign", "valign", "frame-width",
				"frame-height", "pct-width", "pct-height",
				"pct-pos-x", "pct-pos-y" };
			static const char * baseDst[] = {
				"base-xpos", "base-ypos", "base-halign", "base-valign",
				"base-w", "base-h", "base-pctw", "base-pcth",
				"base-pctpx", "base-pctpy" };
			const gchar * bv = nullptr;
			for (const char * pn : grpShare)
				if (parent->getProperty(pn, bv) == UT_OK && bv)
					shape->setProperty(pn, bv);
			for (size_t i = 0; i < sizeof(baseSrc) / sizeof(baseSrc[0]); ++i)
				if (parent->getProperty(baseSrc[i], bv) == UT_OK && bv)
					shape->setProperty(baseDst[i], bv);
			/* a:grpFill lands on the group — children without their
			 * own fill inherit it */
			if (shape->getProperty("background-color", bv) != UT_OK || !bv)
			{
				if (parent->getProperty("background-color", bv) == UT_OK && bv)
				{
					shape->setProperty("background-color", bv);
					shape->setProperty("bg-style", "1");
				}
			}
		}
		else
		{
			if (parent->getProperty("xpos", v) == UT_OK && v)
				shape->setProperty("xpos", v);
			if (parent->getProperty("ypos", v) == UT_OK && v)
				shape->setProperty("ypos", v);
			if (parent->getProperty("frame-width", v) == UT_OK && v)
				shape->setProperty("frame-width", v);
			if (parent->getProperty("frame-height", v) == UT_OK && v)
				shape->setProperty("frame-height", v);
			if (parent->getProperty("pct-width", v) == UT_OK && v)
				shape->setProperty("pct-width", v);
			if (parent->getProperty("pct-height", v) == UT_OK && v)
				shape->setProperty("pct-height", v);
			if (parent->getProperty("pct-pos-x", v) == UT_OK && v)
				shape->setProperty("pct-pos-x", v);
			if (parent->getProperty("pct-pos-y", v) == UT_OK && v)
				shape->setProperty("pct-pos-y", v);
			if (parent->getProperty("halign", v) == UT_OK && v)
				shape->setProperty("halign", v);
			if (parent->getProperty("valign", v) == UT_OK && v)
				shape->setProperty("valign", v);
		}
		if (parent->getProperty("wrap-mode", v) == UT_OK && v)
			shape->setProperty("wrap-mode", v);
		if (parent->getProperty("frame-stack-order", v) == UT_OK && v)
			shape->setProperty("frame-stack-order", v);
		rqst->handled = (parent->appendElement(shape) == UT_OK);
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "ln"))
	{
		/* write the captured outline as uniform frame borders —
		 * Word outlines apply to all four edges. A prstGeom="line"
		 * shape has no interior — render it as a filled bar whose
		 * short side is the line thickness */
		if (m_bInOutline && !m_outlineStyle.empty() &&
			m_outlineStyle != "none" && rqst->stck &&
			!rqst->stck->empty())
		{
			OXML_SharedElement shape = rqst->stck->top();
			if (m_shapePrst == "line" && !m_outlineColor.empty())
			{
				double sw = 0.0, sh = 0.0;
				const gchar * sv = nullptr;
				if (shape->getProperty("grp-width", sv) == UT_OK && sv)
					sw = UT_convertDimensionless(sv);
				else if (shape->getProperty("shape-w", sv) == UT_OK && sv)
					sw = UT_convertDimensionless(sv);
				if (shape->getProperty("grp-height", sv) == UT_OK && sv)
					sh = UT_convertDimensionless(sv);
				else if (shape->getProperty("shape-h", sv) == UT_OK && sv)
					sh = UT_convertDimensionless(sv);
				double thick = m_outlineW > 0.0 ?
					m_outlineW / 72.0 : 0.03; /* pt -> in */
				char buf[24];
				g_snprintf(buf, sizeof(buf), "%.4fin", thick);
				shape->setProperty("background-color",
								   m_outlineColor.c_str());
				shape->setProperty("bg-style", "1");
				shape->setProperty(sh > sw ? "bar-w" : "bar-h", buf);
			}
			else
			{
				static const char * edges[] =
					{ "top", "bot", "left", "right" };
				for (const char * e : edges)
				{
					std::string k(e);
					shape->setProperty(k + "-style", m_outlineStyle);
					if (m_outlineW > 0.0)
					{
						char buf[24];
						g_snprintf(buf, sizeof(buf), "%.2fpt", m_outlineW);
						shape->setProperty(k + "-thickness", buf);
					}
					if (!m_outlineColor.empty())
						shape->setProperty(k + "-color", m_outlineColor);
				}
			}
		}
		m_bInOutline = false;
		m_bInOutlineFill = false;
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "solidFill") ||
		nameMatches(rqst->pName, NS_A_KEY, "gradFill") ||
		nameMatches(rqst->pName, NS_A_KEY, "grpFill") ||
		nameMatches(rqst->pName, NS_A_KEY, "noFill"))
	{
		if (m_inGradFill && !m_gradDesc.empty() &&
			rqst->stck && !rqst->stck->empty())
			rqst->stck->top()->setProperty("fill-gradient",
										 m_gradDesc.c_str());
		m_inGradFill = false;
		m_gradDesc.clear();
		m_bInShapeFill = false;
		m_bInOutlineFill = false;
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "fillRef"))
	{
		m_bInStyleFill = false;
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "spAutoFit") ||
		nameMatches(rqst->pName, NS_A_KEY, "noAutofit") ||
		nameMatches(rqst->pName, "wps", "bodyPr"))
	{
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "custGeom"))
	{
		/* store the recorded freeform path on the host shape —
		 * normalized "M x y L x y C ... Z" in a 0..1000 box */
		if (!m_shapePath.empty() && rqst->stck && !rqst->stck->empty())
			rqst->stck->top()->setProperty("shape-path",
										 m_shapePath.c_str());
		m_inCustGeom = false;
		m_shapePath.clear();
		rqst->handled = true;
		return;
	}
	if (m_inCustGeom &&
		(nameMatches(rqst->pName, NS_A_KEY, "path") ||
		 nameMatches(rqst->pName, NS_A_KEY, "pathLst") ||
		 nameMatches(rqst->pName, NS_A_KEY, "moveTo") ||
		 nameMatches(rqst->pName, NS_A_KEY, "lnTo") ||
		 nameMatches(rqst->pName, NS_A_KEY, "cubicBezTo") ||
		 nameMatches(rqst->pName, NS_A_KEY, "quadBezTo") ||
		 nameMatches(rqst->pName, NS_A_KEY, "arcTo") ||
		 nameMatches(rqst->pName, NS_A_KEY, "close") ||
		 nameMatches(rqst->pName, NS_A_KEY, "pt") ||
		 nameMatches(rqst->pName, NS_A_KEY, "gdLst") ||
		 nameMatches(rqst->pName, NS_A_KEY, "ahLst") ||
		 nameMatches(rqst->pName, NS_A_KEY, "avLst") ||
		 nameMatches(rqst->pName, NS_A_KEY, "cxnLst") ||
		 nameMatches(rqst->pName, NS_A_KEY, "rect")))
	{
		rqst->handled = true;
		return;
	}
	if (nameMatches(rqst->pName, NS_A_KEY, "headEnd") ||
		nameMatches(rqst->pName, NS_A_KEY, "tailEnd") ||
		nameMatches(rqst->pName, NS_A_KEY, "outerShdw") ||
		nameMatches(rqst->pName, NS_A_KEY, "effectLst") ||
		nameMatches(rqst->pName, NS_A_KEY, "effectRef") ||
		nameMatches(rqst->pName, NS_A_KEY, "gs") ||
		nameMatches(rqst->pName, NS_A_KEY, "gsLst") ||
		nameMatches(rqst->pName, NS_A_KEY, "lin") ||
		nameMatches(rqst->pName, NS_A_KEY, "pathLst") ||
		nameMatches(rqst->pName, NS_A_KEY, "srcRect") ||
		nameMatches(rqst->pName, NS_A_KEY, "blipFill") ||
		nameMatches(rqst->pName, NS_A_KEY, "stretch") ||
		nameMatches(rqst->pName, NS_A_KEY, "fillRect") ||
		nameMatches(rqst->pName, NS_A_KEY, "tile") ||
		nameMatches(rqst->pName, NS_A_KEY, "prstTxWarp"))
	{
		rqst->handled = true;
		return;
	}
	if (!m_pendColor.empty() &&
		(nameMatches(rqst->pName, NS_A_KEY, "srgbClr") ||
		 nameMatches(rqst->pName, NS_A_KEY, "schemeClr") ||
		 nameMatches(rqst->pName, NS_A_KEY, "sysClr") ||
		 nameMatches(rqst->pName, NS_A_KEY, "prstClr") ||
		 nameMatches(rqst->pName, NS_A_KEY, "scrgbClr")))
	{
		/* color element closed — apply recorded transforms and write
		 * the fill or outline color (first color wins) */
		std::string final =
			s_transformColor(m_pendColor, m_lumMod, m_lumOff,
							 m_tint, m_shade, m_alpha);
		bool bOutline = m_bPendOutline;
		double alpha = m_alpha;
		m_pendColor.clear();
		m_bPendOutline = false;
		if (!final.empty() && rqst->stck && !rqst->stck->empty())
		{
			if (bOutline)
			{
				if (m_outlineColor.empty())
					m_outlineColor = final;
			}
			else
			{
				/* gradient stop: record the transformed color at its
				 * a:gs position (appending here, at the color end tag,
				 * picks up lumMod/lumOff/tint/shade children) */
				if (m_inGradFill)
				{
					m_gradDesc += m_gradPos.empty() ? "0" : m_gradPos;
					m_gradDesc += ":";
					m_gradDesc += final;
					m_gradDesc += ",";
				}
				const gchar * existing = nullptr;
				if (rqst->stck->top()->getProperty("background-color", existing) != UT_OK ||
					!existing)
				{
					rqst->stck->top()->setProperty("background-color", final.c_str());
					rqst->stck->top()->setProperty("bg-style", "1");
				}
				if (alpha >= 0.0)
				{
					char abuf[24];
					g_snprintf(abuf, sizeof(abuf), "%.3f", alpha);
					rqst->stck->top()->setProperty("fill-alpha", abuf);
				}
			}
		}
		rqst->handled = true;
		return;
	}
	if(nameMatches(rqst->pName, NS_V_KEY, "group"))
	{
		if (!m_vmlGroupStack.empty())
			m_vmlGroupStack.pop_back();
		rqst->handled = true;
		return;
	}
	if(nameMatches(rqst->pName, NS_V_KEY, "shape") ||
		nameMatches(rqst->pName, NS_V_KEY, "rect") ||
		nameMatches(rqst->pName, NS_V_KEY, "roundrect") ||
		nameMatches(rqst->pName, NS_V_KEY, "oval") ||
		nameMatches(rqst->pName, NS_V_KEY, "line") ||
		nameMatches(rqst->pName, NS_V_KEY, "polyline"))
	{
		m_style = "";
		if (m_vShapeDepth > 0)
		{
			m_vShapeDepth--;
			rqst->handled = (_flushTopLevel(rqst->stck, rqst->sect_stck) == UT_OK);
			return;
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_V_KEY, "textbox"))
	{
		//no element pushed when merged into a v:rect-style frame
		rqst->handled = (m_vShapeDepth > 0) ||
			(_flushTopLevel(rqst->stck, rqst->sect_stck) == UT_OK);
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "txbxContent"))
	{
		if (m_bDmlTextbox)
		{
			m_bDmlTextbox = false;
			if (!rqst->stck || rqst->stck->empty())
			{
				rqst->handled = false;
				return;
			}
			OXML_SharedElement tb = rqst->stck->top();
			rqst->stck->pop();
			if (rqst->stck->empty())
			{
				if (!rqst->sect_stck || rqst->sect_stck->empty())
				{
					rqst->handled = false;
					return;
				}
				OXML_SharedSection sect = rqst->sect_stck->top();
				rqst->handled = (sect->appendElement(tb) == UT_OK);
			}
			else
			{
				OXML_SharedElement parent = rqst->stck->top();
				/* the w:drawing wrapper pushed an Image element that
				 * recorded wp:posOffset/wp:extent as xpos/ypos/
				 * frame-width/frame-height — carry that anchor
				 * geometry onto the frame so the textbox lands
				 * roughly where Word placed it */
				const gchar * v = nullptr;
				if (parent->getProperty("xpos", v) == UT_OK && v)
					tb->setProperty("frame-page-xpos", v);
				if (parent->getProperty("ypos", v) == UT_OK && v)
					tb->setProperty("frame-page-ypos", v);
				if (parent->getProperty("halign", v) == UT_OK && v)
					tb->setProperty("halign", v);
				if (parent->getProperty("valign", v) == UT_OK && v)
					tb->setProperty("valign", v);
				if (parent->getProperty("frame-stack-order", v) == UT_OK && v)
					tb->setProperty("frame-stack-order", v);
				if (parent->getProperty("frame-width", v) == UT_OK && v)
					tb->setProperty("frame-width", v);
				if (parent->getProperty("frame-height", v) == UT_OK && v)
					tb->setProperty("frame-height", v);
				rqst->handled = (parent->appendElement(tb) == UT_OK);
			}
		}
		else
			rqst->handled = true;
	}
}

void OXMLi_ListenerState_Textbox::charData (OXMLi_CharDataRequest * /*rqst*/)
{
	//don't do anything here
}

/* VML length → points. VML style values carry units (pt/in/cm/mm/pc/px);
 * bare numbers are points. Returns NAN for unparseable input. */
double OXMLi_ListenerState_Textbox::_vmlLenToPt(const std::string & v) const
{
	char * end = nullptr;
	double n = strtod(v.c_str(), &end);
	if (!end || end == v.c_str())
		return 0.0;
	std::string u(end);
	if (u.empty() || u == "pt")
		return n;
	if (u == "in") return n * 72.0;
	if (u == "cm") return n * 72.0 / 2.54;
	if (u == "mm") return n * 7.2 / 0.254;
	if (u == "pc") return n * 12.0;
	if (u == "px") return n * 0.75;
	return n; // unknown unit — treat as pt
}

static std::string _vmlPt(double pt)
{
	char buf[32];
	g_snprintf(buf, sizeof(buf), "%.2fpt", pt);
	return std::string(buf);
}

/* Transform a child position/size through the top group's coord
 * space: page = off + (coord - origin) * scale. No group →
 * pass through unchanged. */
std::string OXMLi_ListenerState_Textbox::_vmlXformX(const std::string & v) const
{
	if (m_vmlGroupStack.empty())
		return v;
	const VmlGroupX & g = m_vmlGroupStack.back();
	return _vmlPt(g.offX + (_vmlLenToPt(v) - g.originX) * g.scaleX);
}

std::string OXMLi_ListenerState_Textbox::_vmlXformY(const std::string & v) const
{
	if (m_vmlGroupStack.empty())
		return v;
	const VmlGroupX & g = m_vmlGroupStack.back();
	return _vmlPt(g.offY + (_vmlLenToPt(v) - g.originY) * g.scaleY);
}

std::string OXMLi_ListenerState_Textbox::_vmlScaleX(const std::string & v) const
{
	if (m_vmlGroupStack.empty())
		return v;
	return _vmlPt(_vmlLenToPt(v) * m_vmlGroupStack.back().scaleX);
}

std::string OXMLi_ListenerState_Textbox::_vmlScaleY(const std::string & v) const
{
	if (m_vmlGroupStack.empty())
		return v;
	return _vmlPt(_vmlLenToPt(v) * m_vmlGroupStack.back().scaleY);
}
