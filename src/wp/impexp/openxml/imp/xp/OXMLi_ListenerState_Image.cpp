/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */

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
#include "OXMLi_ListenerState_Image.h"

// Internal includes
#include "OXML_Document.h"
#include "OXML_FontManager.h"
#include "OXMLi_PackageManager.h"

// Abinova includes
#include "ut_assert.h"
#include "ut_misc.h"
#include "ie_impGraphic.h"
#include "fg_GraphicRaster.h"

// External includes
#include <string>
#include <math.h>

/* DrawingML percentage attribute — 1000ths of a percent as a bare int
 * or the occasional "40%" literal */
static double s_fxPct(const gchar * v)
{
	if (!v || !*v)
		return 0.0;
	std::string s(v);
	if (s.back() == '%')
		return UT_convertDimensionless(s.c_str()) / 100.0;
	return UT_convertDimensionless(s.c_str()) / 100000.0;
}

/* sRGB <-> HSL for a:hslClr and the hue/sat transforms carried by
 * DrawingML color elements inside a:duotone */
static void s_rgbToHsl(int ri, int gi, int bi, double & h, double & s,
					   double & l)
{
	double r = ri / 255.0, g = gi / 255.0, b = bi / 255.0;
	double mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
	double mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
	l = (mx + mn) / 2.0;
	if (mx == mn)
	{
		h = s = 0.0;
		return;
	}
	double d = mx - mn;
	s = l > 0.5 ? d / (2.0 - mx - mn) : d / (mx + mn);
	if (mx == r)      h = (g - b) / d + (g < b ? 6.0 : 0.0);
	else if (mx == g) h = (b - r) / d + 2.0;
	else              h = (r - g) / d + 4.0;
	h *= 60.0;
}

static unsigned char s_fxByte(double v)
{
	if (v < 0.0)   return 0;
	if (v > 255.0) return 255;
	return (unsigned char)(v + 0.5);
}

static std::string s_hslToHex(double h, double s, double l)
{
	while (h < 0.0)    h += 360.0;
	while (h >= 360.0) h -= 360.0;
	double c = (1.0 - fabs(2.0 * l - 1.0)) * s;
	double x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
	double r1 = 0.0, g1 = 0.0, b1 = 0.0;
	if      (h < 60.0)  { r1 = c; g1 = x; }
	else if (h < 120.0) { r1 = x; g1 = c; }
	else if (h < 180.0) { g1 = c; b1 = x; }
	else if (h < 240.0) { g1 = x; b1 = c; }
	else if (h < 300.0) { r1 = x; b1 = c; }
	else                { r1 = c; b1 = x; }
	double m = l - c / 2.0;
	char buf[8];
	snprintf(buf, sizeof(buf), "%02X%02X%02X",
			 s_fxByte((r1 + m) * 255.0),
			 s_fxByte((g1 + m) * 255.0),
			 s_fxByte((b1 + m) * 255.0));
	return buf;
}

/* resolve a DrawingML color element (a:srgbClr / a:schemeClr / ...)
 * to a bare "RRGGBB" hex string for the duotone color slots */
static std::string s_fxDmlColor(const std::string & name,
								std::map<std::string, std::string>* atts)
{
	auto attr = [&](const char * n) -> const char * {
		auto it = atts->find(n);
		return it != atts->end() ? it->second.c_str() : nullptr;
	};
	if (name == "A:srgbClr")
	{
		const char * v = attr("A:val");
		return v ? v : "";
	}
	if (name == "A:scrgbClr")
	{
		/* channels are 0..100000 percentages */
		const char * rr = attr("A:r"), * gg = attr("A:g"),
			* bb = attr("A:b");
		if (!rr || !gg || !bb)
			return "";
		char buf[8];
		snprintf(buf, sizeof(buf), "%02X%02X%02X",
				 s_fxByte(s_fxPct(rr) * 255.0),
				 s_fxByte(s_fxPct(gg) * 255.0),
				 s_fxByte(s_fxPct(bb) * 255.0));
		return buf;
	}
	if (name == "A:sysClr")
	{
		/* lastClr is the value at save time — the system name cannot
		 * be resolved headlessly */
		const char * v = attr("A:lastClr");
		if (v)
			return v;
		v = attr("A:val");
		if (v && !strcmp(v, "windowText")) return "000000";
		if (v && !strcmp(v, "window"))     return "FFFFFF";
		return "";
	}
	if (name == "A:prstClr")
	{
		const char * v = attr("A:val");
		if (!v)
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
		auto c = prst.find(v);
		return c != prst.end() ? c->second : "";
	}
	if (name == "A:hslClr")
	{
		double h = UT_convertDimensionless(attr("A:hue") ? attr("A:hue")
										   : "0") / 60000.0;
		return s_hslToHex(h, s_fxPct(attr("A:sat")),
						  s_fxPct(attr("A:lum")));
	}
	if (name == "A:schemeClr")
	{
		const char * v = attr("A:val");
		if (!v)
			return "";
		OXML_Document * doc = OXML_Document::getInstance();
		if (!doc || !doc->getTheme())
			return "";
		/* bg1/bg2/tx1/tx2 map onto dk/lt slots per ECMA-376 */
		OXML_ColorName cn = LIGHT1;
		std::string val(v);
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

/* apply the color transforms recorded while a duotone color element
 * was open — ECMA-376 applies them in HSL space */
static std::string s_fxXformColor(const std::string & hex,
								  double lumMod, double lumOff,
								  double tint, double shade,
								  double satMod, double satOff,
								  double hueMod, double hueOff)
{
	if (hex.size() < 6)
		return hex;
	int r = 0, g = 0, b = 0;
	if (sscanf(hex.c_str(), "%02x%02x%02x", &r, &g, &b) != 3)
		return hex;
	if (lumMod == 1.0 && lumOff == 0.0 && tint < 0.0 && shade < 0.0 &&
		satMod < 0.0 && satOff == 0.0 && hueMod < 0.0 && hueOff == 0.0)
		return hex;
	double h, s, l;
	s_rgbToHsl(r, g, b, h, s, l);
	if (shade >= 0.0) l *= shade;
	if (tint >= 0.0)  l = l * tint + (1.0 - tint);
	if (hueMod >= 0.0) h *= hueMod;
	h += hueOff;
	if (satMod >= 0.0) s *= satMod;
	s += satOff;
	if (s < 0.0) s = 0.0;
	if (s > 1.0) s = 1.0;
	l = l * lumMod + lumOff;
	if (l < 0.0) l = 0.0;
	if (l > 1.0) l = 1.0;
	return s_hslToHex(h, s, l);
}

/* wp:wrap* elements set the text-wrap mode, but wp:anchor
 * behindDoc="1" already put the element in the below-text layer —
 * the wrap style must not clobber the layer flag */
static void _setWrapMode(const OXML_SharedElement & elem, const char * mode)
{
	const gchar * cur = nullptr;
	if (elem->getProperty("wrap-mode", cur) == UT_OK &&
		cur && !strcmp(cur, "below-text"))
		return;
	if (elem->setProperty("wrap-mode", mode) != UT_OK)
		UT_DEBUGMSG(("OpenXML importer image wrap-mode property can't be set\n"));
}

OXMLi_ListenerState_Image::OXMLi_ListenerState_Image()
  : OXMLi_ListenerState(),
	m_style(""),
	m_isEmbeddedObject(false),
	m_isInlineImage(false),
	m_bSimplePos(false)
{

}

void OXMLi_ListenerState_Image::startElement (OXMLi_StartElementRequest * rqst)
{
	if(nameMatches(rqst->pName, NS_W_KEY, "object"))
	{
		// Abiword doesn't support embedded objects, enable this boolean lock when needed
		m_isEmbeddedObject = true;
		rqst->handled = true;
	}
	if(m_isEmbeddedObject)
	{
		return;
	}

	if(nameMatches(rqst->pName, NS_W_KEY, "drawing"))
	{
		OXML_SharedElement imgElem(new OXML_Element_Image(""));
		rqst->stck->push(imgElem);
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_PIC_KEY, "pic"))
	{
		/* a picture that is a child of a wpg:wgp group needs its own
		 * element so its child-space a:xfrm can position it —
		 * top-level pictures share the w:drawing element instead */
		bool inGroup = false;
		if (rqst->context)
			for (const auto & c : *rqst->context)
				if (c == "wpg:wgp")
				{
					inGroup = true;
					break;
				}
		if (inGroup)
		{
			OXML_SharedElement picElem(new OXML_Element_Image(""));
			rqst->stck->push(picElem);
			++m_grpPicDepth;
			rqst->handled = true;
		}
	}
	else if(nameMatches(rqst->pName, NS_WP_KEY, "inline"))
	{
		if(rqst->stck->empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}
		std::string contextTag = "";
		if(!rqst->context->empty())
		{
			contextTag = OXMLi_contextBack(rqst->context);
		}
		int drawing = contextMatches(contextTag, NS_W_KEY, "drawing");
		if(drawing)
		{
			m_isInlineImage = true;
			rqst->handled = true;
		}
	}
	else if(nameMatches(rqst->pName, NS_WP_KEY, "anchor"))
	{
		if(rqst->stck->empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}
		std::string contextTag = "";
		if(!rqst->context->empty())
		{
			contextTag = OXMLi_contextBack(rqst->context);
		}
		int drawing = contextMatches(contextTag, NS_W_KEY, "drawing");
		if(drawing)
		{
			m_isInlineImage = false;
			/* anchored drawings may float behind the text layer
			 * (wp:anchor behindDoc="1") — carry that through as the
			 * frame's below-text wrap mode so artwork doesn't cover
			 * the document text */
			const gchar * behind = attrMatches(NS_WP_KEY, "behindDoc", rqst->ppAtts);
			if (behind && !strcmp(behind, "1"))
			{
				OXML_SharedElement imgElem = OXMLi_elemTop(rqst->stck);
				if (imgElem)
					imgElem->setProperty("wrap-mode", "below-text");
			}
			/* relativeHeight is the OOXML z-order — higher draws in
			 * front. Feed our frame-stack-order property so the page's
			 * frame layer orders like Word */
			const gchar * rh = attrMatches(NS_WP_KEY, "relativeHeight", rqst->ppAtts);
			if (rh && OXMLi_elemTop(rqst->stck))
				{ OXML_SharedElement _e = OXMLi_elemTop(rqst->stck);
				  if (_e.get()) _e->setProperty("frame-stack-order", rh); }
			/* simplePos="1" makes wp:simplePos@x/y the position,
			 * overriding positionH/positionV */
			const gchar * sp = attrMatches(NS_WP_KEY, "simplePos", rqst->ppAtts);
			m_bSimplePos = (sp && !strcmp(sp, "1"));
			rqst->handled = true;
		}
	}
	else if(nameMatches(rqst->pName, NS_WP_KEY, "simplePos"))
	{
		std::string contextTag = "";
		if(!rqst->context->empty())
		{
			contextTag = OXMLi_contextBack(rqst->context);
		}
		if(contextMatches(contextTag, NS_WP_KEY, "anchor"))
		{
			if(m_bSimplePos && !rqst->stck->empty() && OXMLi_elemTop(rqst->stck))
			{
				const gchar * x = attrMatches(NS_WP_KEY, "x", rqst->ppAtts);
				const gchar * y = attrMatches(NS_WP_KEY, "y", rqst->ppAtts);
				if(x && *x)
				{
					std::string position(_EmusToInches(x));
					position += "in";
					{ OXML_SharedElement _e = OXMLi_elemTop(rqst->stck);
					  if (_e.get()) _e->setProperty("xpos", position); }
				}
				if(y && *y)
				{
					std::string position(_EmusToInches(y));
					position += "in";
					{ OXML_SharedElement _e = OXMLi_elemTop(rqst->stck);
					  if (_e.get()) _e->setProperty("ypos", position); }
				}
			}
			m_bSimplePos = false;
			rqst->handled = true;
		}
	}
	else if(nameMatches(rqst->pName, NS_WP_KEY, "positionH"))
	{
		if(rqst->stck->empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}
		std::string contextTag = "";
		if(!rqst->context->empty())
		{
			contextTag = OXMLi_contextBack(rqst->context);
		}
		int anchor = contextMatches(contextTag, NS_WP_KEY, "anchor");
		if(anchor)
		{
			rqst->handled = true;
		}
	}
	else if(nameMatches(rqst->pName, NS_WP_KEY, "positionV"))
	{
		if(rqst->stck->empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}
		std::string contextTag = "";
		if(!rqst->context->empty())
		{
			contextTag = OXMLi_contextBack(rqst->context);
		}
		int anchor = contextMatches(contextTag, NS_WP_KEY, "anchor");
		if(anchor)
		{
			rqst->handled = true;
		}
	}
	else if(nameMatches(rqst->pName, NS_A_KEY, "srcRect"))
	{
		/* a:srcRect - blip crop: l/t/r/b in 1000ths of a percent */
		if(!rqst->stck->empty() && OXMLi_elemTop(rqst->stck))
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
			{ OXML_SharedElement _e = OXMLi_elemTop(rqst->stck);
			  if (_e.get()) _e->setProperty("image-src-rect", rect.c_str()); }
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_A_KEY, "tile"))
	{
		/* a:blipFill/a:tile — tile the blip across the picture
		 * instead of stretching it: tx/ty grid offset in EMUs,
		 * sx/sy tile scale in 1000ths of a percent of the blip's
		 * natural size, flip (none/x/y/xy) mirrors alternate
		 * tiles, algn anchors the tile grid */
		if(!rqst->stck->empty() && OXMLi_elemTop(rqst->stck))
		{
			const gchar * tx = attrMatches(NS_A_KEY, "tx", rqst->ppAtts);
			const gchar * ty = attrMatches(NS_A_KEY, "ty", rqst->ppAtts);
			const gchar * sx = attrMatches(NS_A_KEY, "sx", rqst->ppAtts);
			const gchar * sy = attrMatches(NS_A_KEY, "sy", rqst->ppAtts);
			const gchar * flip = attrMatches(NS_A_KEY, "flip", rqst->ppAtts);
			const gchar * algn = attrMatches(NS_A_KEY, "algn", rqst->ppAtts);
			std::string tile;
			tile += (tx && *tx) ? tx : "0";
			tile += " ";
			tile += (ty && *ty) ? ty : "0";
			tile += " ";
			tile += (sx && *sx) ? sx : "100000";
			tile += " ";
			tile += (sy && *sy) ? sy : "100000";
			tile += " ";
			tile += (flip && *flip) ? flip : "none";
			tile += " ";
			tile += (algn && *algn) ? algn : "tl";
			{ OXML_SharedElement _e = OXMLi_elemTop(rqst->stck);
			  if (_e.get()) _e->setProperty("image-tile", tile.c_str()); }
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_A_KEY, "fillRect"))
	{
		/* a:stretch/a:fillRect — the destination rectangle the blip
		 * is stretched into, l/t/r/b in 1000ths of a percent of the
		 * bounding box (negative insets expand past it) */
		if(!rqst->stck->empty() && OXMLi_elemTop(rqst->stck))
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
			{ OXML_SharedElement _e = OXMLi_elemTop(rqst->stck);
			  if (_e.get()) _e->setProperty("image-fill-rect", rect.c_str()); }
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_WP_KEY, "posOffset"))
	{
		if(rqst->stck->empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}
		std::string contextTag = "";
		if(!rqst->context->empty())
		{
			contextTag = OXMLi_contextBack(rqst->context);
		}
		int positionH = contextMatches(contextTag, NS_WP_KEY, "positionH");
		int positionV = contextMatches(contextTag, NS_WP_KEY, "positionV");
		if(positionH || positionV)
		{
			rqst->handled = true;
		}
	}
	else if(nameMatches(rqst->pName, NS_WP_KEY, "extent"))
	{
		if(rqst->stck->empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		OXML_SharedElement imgElem = OXMLi_elemTop(rqst->stck);
		if(!imgElem)
			return;

		const gchar * cx = attrMatches(NS_WP_KEY, "cx", rqst->ppAtts); //width
		if(cx)
		{
			std::string width(_EmusToInches(cx));
			width += "in";
			if(m_isInlineImage)
			{
				if(imgElem->setProperty("width", width) != UT_OK)
				{
					UT_DEBUGMSG(("SERHAT:OpenXML importer inline image width property can't be set\n"));
				}
			}
			else
			{
				if(imgElem->setProperty("frame-width", width) != UT_OK)
				{
					UT_DEBUGMSG(("SERHAT:OpenXML importer positioned image width property can't be set\n"));
				}
			}
		}

		const gchar * cy = attrMatches(NS_WP_KEY, "cy", rqst->ppAtts); //height
		if(cy)
		{
			std::string height(_EmusToInches(cy));
			height += "in";
			if(m_isInlineImage)
			{
				if(imgElem->setProperty("height", height) != UT_OK)
				{
					UT_DEBUGMSG(("SERHAT:OpenXML importer inline image height property can't be set\n"));
				}
			}
			else
			{
				if(imgElem->setProperty("frame-height", height) != UT_OK)
				{
					UT_DEBUGMSG(("SERHAT:OpenXML importer positioned image height property can't be set\n"));
				}
			}
		}

		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_WP_KEY, "wrapSquare"))
	{
		if(rqst->stck->empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		OXML_SharedElement imgElem = OXMLi_elemTop(rqst->stck);
		if(!imgElem)
			return;

		const gchar * wrapText = attrMatches(NS_WP_KEY, "wrapText", rqst->ppAtts);
		if(wrapText)
		{
			if(!strcmp(wrapText, "bothSides"))
				_setWrapMode(imgElem, "wrapped-both");
			else if(!strcmp(wrapText, "right"))
				_setWrapMode(imgElem, "wrapped-to-right");
			else if(!strcmp(wrapText, "left"))
				_setWrapMode(imgElem, "wrapped-to-left");
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_WP_KEY, "wrapTight") ||
			nameMatches(rqst->pName, NS_WP_KEY, "wrapThrough"))
	{
		if(rqst->stck->empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		OXML_SharedElement imgElem = OXMLi_elemTop(rqst->stck);
		if(!imgElem)
			return;

		const gchar * wrapText = attrMatches(NS_WP_KEY, "wrapText", rqst->ppAtts);
		std::string mode("wrapped-both"); //bothSides is the default
		if(wrapText)
		{
			if(!strcmp(wrapText, "right"))
				mode = "wrapped-to-right";
			else if(!strcmp(wrapText, "left"))
				mode = "wrapped-to-left";
			else if(!strcmp(wrapText, "largest"))
				mode = "wrapped-both";
		}
		_setWrapMode(imgElem, mode.c_str());
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_WP_KEY, "wrapTopAndBottom") ||
			nameMatches(rqst->pName, NS_WP_KEY, "wrapNone"))
	{
		if(rqst->stck->empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		OXML_SharedElement imgElem = OXMLi_elemTop(rqst->stck);
		if(!imgElem)
			return;

		//both modes wrap only above/below; closest is wrapped-topbot
		_setWrapMode(imgElem, "wrapped-topbot");
		rqst->handled = true;
	}
	else if (nameMatches(rqst->pName, NS_A_KEY, "blip"))
	{
		if(rqst->stck->empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		OXML_SharedElement imgElem = OXMLi_elemTop(rqst->stck);
		if(!imgElem)
			return;

		const gchar * id = attrMatches(NS_R_KEY, "embed", rqst->ppAtts);
		if(id)
		{
			std::string imageId(id);
			imgElem->setId(id);
			rqst->handled = addImage(imageId);
		}
	}
	/* CT_Blip children — picture effects (a:duotone, a:grayscl, a:lum,
	 * a:alphaModFix, ...) land on the same element the blip's id did.
	 * Everything under a:blip is swallowed here so the color elements
	 * aren't misread as shape fills by the textbox listener */
	else if (rqst->context && !rqst->context->empty() &&
			 OXMLi_contextBack(rqst->context) == "A:blip")
	{
		if (!rqst->stck->empty() && OXMLi_elemTop(rqst->stck))
		{
			OXML_SharedElement fxElem = OXMLi_elemTop(rqst->stck);
			if (nameMatches(rqst->pName, NS_A_KEY, "duotone"))
			{
				m_bInDuotone = true;
				m_duotone.clear();
				m_pendFxColor.clear();
			}
			else if (nameMatches(rqst->pName, NS_A_KEY, "grayscl"))
				fxElem->setProperty("image-grayscale", "1");
			else if (nameMatches(rqst->pName, NS_A_KEY, "lum"))
			{
				double br = s_fxPct(attrMatches(NS_A_KEY, "bright",
												rqst->ppAtts));
				double ct = s_fxPct(attrMatches(NS_A_KEY, "contrast",
												rqst->ppAtts));
				if (br != 0.0 || ct != 0.0)
				{
					char buf[48];
					g_snprintf(buf, sizeof(buf), "%.4f %.4f", br, ct);
					fxElem->setProperty("image-lum", buf);
				}
			}
			else if (nameMatches(rqst->pName, NS_A_KEY, "alphaModFix") ||
					 nameMatches(rqst->pName, NS_A_KEY, "alphaMod"))
			{
				/* both modulate the alpha channel by a fixed amount;
				 * amt is required by the schema — without it the
				 * effect is a no-op */
				const gchar * amt = attrMatches(NS_A_KEY, "amt",
												rqst->ppAtts);
				if (amt)
				{
					char buf[24];
					g_snprintf(buf, sizeof(buf), "%.4f", s_fxPct(amt));
					fxElem->setProperty("image-alpha-mod", buf);
				}
			}
		}
		rqst->handled = true;
	}
	else if (m_bInDuotone)
	{
		/* a:duotone children: two color specs, each possibly carrying
		 * transform children (a:shade, a:tint, a:satMod, ...) */
		if (nameMatches(rqst->pName, NS_A_KEY, "srgbClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "schemeClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "sysClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "prstClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "scrgbClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "hslClr"))
		{
			m_pendFxColor = s_fxDmlColor(rqst->pName, rqst->ppAtts);
			if (!m_pendFxColor.empty() && m_pendFxColor[0] == '#')
				m_pendFxColor.erase(0, 1);
			m_fxLumMod = 1.0; m_fxLumOff = 0.0;
			m_fxTint = -1.0;  m_fxShade = -1.0;
			m_fxSatMod = -1.0; m_fxSatOff = 0.0;
			m_fxHueMod = -1.0; m_fxHueOff = 0.0;
		}
		else if (!m_pendFxColor.empty())
		{
			const gchar * v = attrMatches(NS_A_KEY, "val", rqst->ppAtts);
			double f = v ? UT_convertDimensionless(v) : 0.0;
			if (rqst->pName == "A:hueOff")
				m_fxHueOff = f / 60000.0;   /* 60000ths of a degree */
			else if (v)
			{
				f /= 100000.0;
				if (rqst->pName == "A:lumMod")      m_fxLumMod = f;
				else if (rqst->pName == "A:lumOff") m_fxLumOff = f;
				else if (rqst->pName == "A:tint")   m_fxTint = f;
				else if (rqst->pName == "A:shade")  m_fxShade = f;
				else if (rqst->pName == "A:satMod") m_fxSatMod = f;
				else if (rqst->pName == "A:satOff") m_fxSatOff = f;
				else if (rqst->pName == "A:hueMod") m_fxHueMod = f;
			}
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_V_KEY, "shape"))
	{
		const gchar* style = attrMatches(NS_V_KEY, "style", rqst->ppAtts);
		if(style)
		{
			m_style = style;
		}
		//don't handle the request here in case shape contains some other structure, ex: textbox
	}
	else if(nameMatches(rqst->pName, NS_V_KEY, "imagedata"))
	{
		const gchar* id = attrMatches(NS_R_KEY, "id", rqst->ppAtts);
		if(id)
		{
			std::string imageId(id);
			OXML_SharedElement imgElem(new OXML_Element_Image(imageId));
			rqst->stck->push(imgElem);

			if(!addImage(imageId))
				return;

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
							imgElem->setProperty("width", attrValue);
						}
						else if(!attrName.compare("height"))
						{
							imgElem->setProperty("height", attrValue);
						}
						//TODO: more attributes coming
					}	
					//finally update the start point for the next attribute
					attrStart = attrEnd+1;
				}
			}
			rqst->handled = true;
		}
	}

	//TODO: more coming here
}

void OXMLi_ListenerState_Image::endElement (OXMLi_EndElementRequest * rqst)
{
	if(nameMatches(rqst->pName, NS_W_KEY, "object"))
	{
		m_isEmbeddedObject = false;
		rqst->handled = true;
		return;
	}
	if(m_isEmbeddedObject)
	{
		return;
	}

	if(nameMatches(rqst->pName, NS_W_KEY, "drawing") || 
		nameMatches(rqst->pName, NS_V_KEY, "imagedata"))
	{
		//image is done
		rqst->handled = (_flushTopLevel(rqst->stck, rqst->sect_stck) == UT_OK);
	}
	else if(nameMatches(rqst->pName, NS_A_KEY, "blip") ||
			nameMatches(rqst->pName, NS_A_KEY, "srcRect") ||
			nameMatches(rqst->pName, NS_A_KEY, "tile") ||
			nameMatches(rqst->pName, NS_A_KEY, "stretch") ||
			nameMatches(rqst->pName, NS_A_KEY, "fillRect") ||
			nameMatches(rqst->pName, NS_A_KEY, "blipFill") ||
			nameMatches(rqst->pName, NS_PIC_KEY, "blipFill") ||
			nameMatches(rqst->pName, NS_WP_KEY, "extent") ||
			nameMatches(rqst->pName, NS_WP_KEY, "wrapSquare") ||
			nameMatches(rqst->pName, NS_WP_KEY, "posOffset") ||
			nameMatches(rqst->pName, NS_WP_KEY, "positionH") ||
			nameMatches(rqst->pName, NS_WP_KEY, "positionV") ||
			nameMatches(rqst->pName, NS_WP_KEY, "simplePos"))
	{
		m_bInDuotone = false;
		m_pendFxColor.clear();
		rqst->handled = true;
	}
	else if (m_bInDuotone)
	{
		if (nameMatches(rqst->pName, NS_A_KEY, "srgbClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "schemeClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "sysClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "prstClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "scrgbClr") ||
			nameMatches(rqst->pName, NS_A_KEY, "hslClr"))
		{
			/* color element closed — apply the recorded transforms
			 * and take the result into the duotone pair */
			if (!m_pendFxColor.empty() && m_duotone.size() < 2)
				m_duotone.push_back(
					s_fxXformColor(m_pendFxColor, m_fxLumMod, m_fxLumOff,
								   m_fxTint, m_fxShade, m_fxSatMod,
								   m_fxSatOff, m_fxHueMod, m_fxHueOff));
			m_pendFxColor.clear();
		}
		else if (nameMatches(rqst->pName, NS_A_KEY, "duotone"))
		{
			if (m_duotone.size() >= 2 && rqst->stck &&
				!rqst->stck->empty() && OXMLi_elemTop(rqst->stck))
			{
				std::string duo = m_duotone[0] + " " + m_duotone[1];
				{ OXML_SharedElement _e = OXMLi_elemTop(rqst->stck);
				  if (_e.get()) _e->setProperty("image-duotone",
											 duo.c_str()); }
			}
			m_bInDuotone = false;
			m_duotone.clear();
			m_pendFxColor.clear();
		}
		rqst->handled = true;
	}
	else if (rqst->context && !rqst->context->empty() &&
			 OXMLi_contextBack(rqst->context) == "A:blip")
	{
		/* blip children ends (lum/grayscl/alphaModFix/extLst) —
		 * captured, or intentionally ignored, at their start tags */
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_WP_KEY, "anchor") ||
			nameMatches(rqst->pName, NS_WP_KEY, "inline"))
	{
		m_isInlineImage = false;
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_PIC_KEY, "pic") &&
			m_grpPicDepth > 0)
	{
		--m_grpPicDepth;
		if (rqst->stck->empty())
		{
			rqst->handled = false;
			return;
		}
		OXML_SharedElement pic = OXMLi_elemTop(rqst->stck);
		rqst->stck->pop();
		if (rqst->stck->empty())
		{
			if (!rqst->sect_stck || rqst->sect_stck->empty())
			{
				rqst->handled = false;
				return;
			}
			rqst->handled =
				(OXMLi_sectTop(rqst->sect_stck)->appendElement(pic) == UT_OK);
			return;
		}
		OXML_SharedElement parent = OXMLi_elemTop(rqst->stck);
		/* carry the group's transform and anchor geometry through as
		 * base-* so addToPT can resolve the child-space offset */
		const gchar * bv = nullptr;
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
		for (const char * pn : grpShare)
			if (parent->getProperty(pn, bv) == UT_OK && bv)
				pic->setProperty(pn, bv);
		for (size_t i = 0; i < sizeof(baseSrc) / sizeof(baseSrc[0]); ++i)
			if (parent->getProperty(baseSrc[i], bv) == UT_OK && bv)
				pic->setProperty(baseDst[i], bv);
		if (parent->getProperty("wrap-mode", bv) == UT_OK && bv)
			pic->setProperty("wrap-mode", bv);
		if (parent->getProperty("frame-stack-order", bv) == UT_OK && bv)
			pic->setProperty("frame-stack-order", bv);
		rqst->handled = (parent->appendElement(pic) == UT_OK);
	}
	else if(nameMatches(rqst->pName, NS_V_KEY, "shape"))
	{
		m_style = "";
	}
	//TODO: more coming here
}

void OXMLi_ListenerState_Image::charData (OXMLi_CharDataRequest * rqst)
{
	if(m_isEmbeddedObject)
	{
		return;
	}
	if(!rqst)
	{
		UT_DEBUGMSG(("SERHAT: OpenXML importer invalid NULL request in OXMLi_ListenerState_Image.charData\n"));
		return;
	}
	if(rqst->stck->empty())
	{
		rqst->handled = false;
		rqst->valid = false;
		return;
	}

	std::string contextTag = "";
	if(!rqst->context->empty())
	{
		contextTag = OXMLi_contextBack(rqst->context);
	}
	int posOffset = contextMatches(contextTag, NS_WP_KEY, "posOffset");
	if(posOffset && !m_isInlineImage)
	{
		OXML_SharedElement imgElem = OXMLi_elemTop(rqst->stck);
		rqst->stck->pop();

		if(rqst->context->size() >= 2)
			contextTag = OXMLi_contextParent(rqst->context);
		int positionH = contextMatches(contextTag, NS_WP_KEY, "positionH");
		int positionV = contextMatches(contextTag, NS_WP_KEY, "positionV");
		if(rqst->buffer == nullptr)
		{
			UT_DEBUGMSG(("SERHAT: Unexpected situation, request with a null buffer\n"));
			return;
		}
		if(positionH)
		{
			std::string position(_EmusToInches(rqst->buffer));
			position += "in";
			imgElem->setProperty("xpos", position);
		}
		else if(positionV)
		{
			std::string position(_EmusToInches(rqst->buffer));
			position += "in";
			imgElem->setProperty("ypos", position);
		}
		rqst->stck->push(imgElem);
		return;
	}

	/* <wp:positionH><wp:align>center</wp:align> — alignment-based
	 * anchoring (no offsets). Record the keyword; the position is
	 * resolved in addToPT once extent and page size are known. */
	if (contextMatches(contextTag, NS_WP_KEY, "align"))
	{
		if (rqst->buffer == nullptr || rqst->stck->empty())
			return;
		std::string parentTag;
		if (rqst->context->size() >= 2)
			parentTag = OXMLi_contextParent(rqst->context);
		std::string align(rqst->buffer);
		if (contextMatches(parentTag, NS_WP_KEY, "positionH"))
			{ OXML_SharedElement _e = OXMLi_elemTop(rqst->stck);
			  if (_e.get()) _e->setProperty("halign", align); }
		else if (contextMatches(parentTag, NS_WP_KEY, "positionV"))
			{ OXML_SharedElement _e = OXMLi_elemTop(rqst->stck);
			  if (_e.get()) _e->setProperty("valign", align); }
		rqst->handled = true;
		return;
	}

	/* wp14 percent offsets: <wp:positionH relativeFrom="page">
	 * <wp14:pctPosHOffset>45500</wp14:pctPosHOffset> — 1000ths of a
	 * percent of the page dimension. These sit inside mc:Choice
	 * while the absolute posOffset fallback is inside mc:Fallback
	 * (which the Valid listener drops), so without this branch
	 * anchored textboxes/shapes lose their position entirely.
	 * The page size is not known yet (sectPr comes at the end of
	 * the body), so compute against US Letter defaults. */
	if (contextTag == "wp14:pctPosHOffset" ||
		contextTag == "wp14:pctPosVOffset")
	{
		if (rqst->buffer == nullptr)
			return;
		double pct = UT_convertDimensionless(rqst->buffer) / 100000.0;
		/* keep the raw fraction — addToPT resolves it against the
		 * real page size (sectPr may not have been parsed yet) */
		{ OXML_SharedElement _e = OXMLi_elemTop(rqst->stck);
		  if (_e.get()) _e->setProperty(
			contextTag == "wp14:pctPosHOffset" ? "pct-pos-x" : "pct-pos-y",
			UT_convertToDimensionlessString(pct)); }
		double base = (contextTag == "wp14:pctPosHOffset") ? 8.5 : 11.0;
		char buf[32];
		g_snprintf(buf, sizeof(buf), "%.4fin", pct * base);
		{ OXML_SharedElement _e = OXMLi_elemTop(rqst->stck);
		  if (_e.get()) _e->setProperty(
			contextTag == "wp14:pctPosHOffset" ? "xpos" : "ypos", buf); }
		rqst->handled = true;
	}

	/* wp14 percent sizing: <wp14:sizeRelH relativeFrom="page">
	 * <wp14:pctWidth>100000</wp14:pctWidth> — shape extents as a
	 * fraction of the page. Resolved against the real page size in
	 * addToPT; a Letter estimate is also stored so the frame is
	 * never degenerate. */
	if (contextTag == "wp14:pctWidth" ||
		contextTag == "wp14:pctHeight")
	{
		if (rqst->buffer == nullptr)
			return;
		double pct = UT_convertDimensionless(rqst->buffer) / 100000.0;
		{ OXML_SharedElement _e = OXMLi_elemTop(rqst->stck);
		  if (_e.get()) _e->setProperty(
			contextTag == "wp14:pctWidth" ? "pct-width" : "pct-height",
			UT_convertToDimensionlessString(pct)); }
		double base = (contextTag == "wp14:pctWidth") ? 8.5 : 11.0;
		char buf[32];
		g_snprintf(buf, sizeof(buf), "%.4fin", pct * base);
		{ OXML_SharedElement _e = OXMLi_elemTop(rqst->stck);
		  if (_e.get()) _e->setProperty(
			contextTag == "wp14:pctWidth" ? "frame-width" : "frame-height",
			buf); }
		rqst->handled = true;
	}
}


//helper functions
bool OXMLi_ListenerState_Image::addImage(const std::string & id)
{
	UT_Error err = UT_OK;
	FG_ConstGraphicPtr pFG;
		
	OXMLi_PackageManager * mgr = OXMLi_PackageManager::getInstance();
	UT_ConstByteBufPtr imageData = mgr->parseImageStream(id.c_str());

	if (!imageData)
		return false;

	err = IE_ImpGraphic::loadGraphic(imageData, IEGFT_Unknown, pFG);
	if ((err != UT_OK) || !pFG)
	{
		UT_DEBUGMSG(("FRT:OpenXML importer can't import the picture with id:%s\n", id.c_str()));
		return false;
	}

	OXML_Document * doc = OXML_Document::getInstance();
	if(!doc)
		return false;

	OXML_Image* img = new OXML_Image();
	img->setId(id.c_str());
	img->setGraphic(std::move(pFG));

	OXML_SharedImage shrImg(img);

	return doc->addImage(shrImg) == UT_OK;
}
