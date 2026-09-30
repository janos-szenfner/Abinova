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
#include "OXMLi_ListenerState_Common.h"

// Internal includes
#include "OXMLi_Types.h"
#include "OXMLi_PackageManager.h"
#include "OXML_Document.h"
#include "OXML_Element.h"
#include "OXML_Element_Run.h"
#include "OXML_Element_Text.h"
#include "OXML_Element_Field.h"
#include "OXML_Element_Annotation.h"
#include "OXML_Types.h"
#include "OXML_Theme.h"
#include "OXML_Style.h"
#include "OXML_Section.h"
#include "OXML_FontManager.h"

// Abinova includes
#include "ut_units.h"
#include "ut_misc.h"
#include "ut_debugmsg.h"
#include "ut_assert.h"

// External includes
#include <cstring>
#include <cstdlib>

/* Adobe Symbol charset -> Unicode. w:sym/w:char stores the codepoint in
 * the symbol font's own encoding; Symbol is the common case. Table covers
 * 0x20-0xFF; 0 = unmapped (font-private extension glyphs). */
static UT_UCS4Char _symbolCharToUnicode(UT_UCS4Char c)
{
	if (c < 0x20 || c > 0xFE)
		return c;
	static const UT_UCS4Char map[] = {
		0x0020,0x0021,0x2200,0x0023,0x2203,0x0025,0x0026,0x220D, /* 20-27 */
		0x0028,0x0029,0x2217,0x002B,0x002C,0x2212,0x002E,0x002F, /* 28-2F */
		0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037, /* 30-37 */
		0x0038,0x0039,0x003A,0x003B,0x003C,0x003D,0x003E,0x003F, /* 38-3F */
		0x2245,0x0391,0x0392,0x03A7,0x0394,0x0395,0x03A6,0x0393, /* 40-47 */
		0x0397,0x0399,0x03D1,0x039A,0x039B,0x039C,0x039D,0x039F, /* 48-4F */
		0x03A0,0x0398,0x03A1,0x03A3,0x03A4,0x03A5,0x03C2,0x03A9, /* 50-57 */
		0x039E,0x03A8,0x0396,0x005B,0x2234,0x005D,0x22A5,0x005F, /* 58-5F */
		0xF8E5,0x03B1,0x03B2,0x03C7,0x03B4,0x03B5,0x03C6,0x03B3, /* 60-67 */
		0x03B7,0x03B9,0x03D5,0x03BA,0x03BB,0x03BC,0x03BD,0x03BF, /* 68-6F */
		0x03C0,0x03B8,0x03C1,0x03C3,0x03C4,0x03C5,0x03D6,0x03C9, /* 70-77 */
		0x03BE,0x03C8,0x03B6,0x007B,0x007C,0x007D,0x223C,0x007F, /* 78-7F */
		0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000, /* 80-87 */
		0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000, /* 88-8F */
		0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000, /* 90-97 */
		0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000, /* 98-9F */
		0x20AC,0x03D2,0x2032,0x2264,0x2044,0x221E,0x0192,0x2663, /* A0-A7 */
		0x2666,0x2665,0x2660,0x2194,0x2190,0x2191,0x2192,0x2193, /* A8-AF */
		0x00B0,0x00B1,0x2033,0x2265,0x00D7,0x221D,0x2202,0x2022, /* B0-B7 */
		0x00F7,0x2260,0x2261,0x2248,0x2026,0xF8E6,0xF8E7,0x21B5, /* B8-BF */
		0x2135,0x2111,0x211C,0x2118,0x2297,0x2295,0x2205,0x2229, /* C0-C7 */
		0x222A,0x2283,0x2287,0x2284,0x2282,0x2286,0x2208,0x2209, /* C8-CF */
		0x2220,0x2207,0x00AE,0x00A9,0x2122,0x220F,0x221A,0x22C5, /* D0-D7 */
		0x00AC,0x2227,0x2228,0x21D4,0x21D0,0x21D1,0x21D2,0x21D3, /* D8-DF */
		0x25CA,0x2329,0xF8E8,0xF8E9,0xF8EA,0x2211,0xF8EB,0xF8EC, /* E0-E7 */
		0xF8ED,0xF8EE,0xF8EF,0xF8F0,0xF8F1,0xF8F2,0xF8F3,0xF8F4, /* E8-EF */
		0x0000,0x232A,0x222B,0x2320,0xF8F5,0x2321,0xF8F6,0xF8F7, /* F0-F7 */
		0xF8F8,0xF8F9,0xF8FA,0xF8FB,0xF8FC,0xF8FD,0xF8FE,0x0000  /* F8-FF */
	};
	return map[c - 0x20];
}

OXMLi_ListenerState_Common::OXMLi_ListenerState_Common() : 
	OXMLi_ListenerState(), 
	m_pendingSectBreak(false),
	m_eqField(false),
	m_pageNumberField(false),
	m_fldChar(false)
{

}

OXMLi_ListenerState_Common::~OXMLi_ListenerState_Common()
{
}

/* Resolves an OOXML <w:themeColor> name (accent1, dark1, ...) to a
 * hex color string using the document's theme; falls back to black. */
static std::string _resolveThemeColor(const gchar * name)
{
	OXML_Document * doc = OXML_Document::getInstance();
	OXML_SharedTheme theme = doc ? doc->getTheme() : OXML_SharedTheme();
	std::string color = "#000000"; //default color in case of illegal themeColor value.
	if (!theme.get() || !name)
		return color;

	if (!strcmp(name,"accent1")) {
		color = theme->getColor(ACCENT1);
	} else if (!strcmp(name,"accent2")) {
		color = theme->getColor(ACCENT2);
	} else if (!strcmp(name,"accent3")) {
		color = theme->getColor(ACCENT3);
	} else if (!strcmp(name,"accent4")) {
		color = theme->getColor(ACCENT4);
	} else if (!strcmp(name,"accent5")) {
		color = theme->getColor(ACCENT5);
	} else if (!strcmp(name,"accent6")) {
		color = theme->getColor(ACCENT6);
	} else if (!strcmp(name,"dark1")) {
		color = theme->getColor(DARK1);
	} else if (!strcmp(name,"dark2")) {
		color = theme->getColor(DARK2);
	} else if (!strcmp(name,"light1")) {
		color = theme->getColor(LIGHT1);
	} else if (!strcmp(name,"light2")) {
		color = theme->getColor(LIGHT2);
	} else if (!strcmp(name,"hlink")) {
		color = theme->getColor(HYPERLINK);
	} else if (!strcmp(name,"folHlink")) {
		color = theme->getColor(FOLLOWED_HYPERLINK);
	}
	return color;
}

void OXMLi_ListenerState_Common::startElement (OXMLi_StartElementRequest * rqst)
{
	UT_return_if_fail( this->_error_if_fail(rqst != nullptr) );

	/* w:sdt content controls pass their sdtContent children through
	 * normally; showingPlcHdr placeholder text is imported as visible
	 * text to match Word's on-screen rendering. */

	if(nameMatches(rqst->pName, NS_W_KEY, "instrText"))
	{
		rqst->handled = true;
	} else if(nameMatches(rqst->pName, NS_W_KEY, "fldChar")) {
		const gchar* fldCharType = attrMatches(NS_W_KEY, "fldCharType", rqst->ppAtts);
		if(!fldCharType)
		{
			UT_DEBUGMSG(("SERHAT: fldChar tag without fldCharType attribute\n"));
			return;
		}
		if(!strcmp(fldCharType, "begin"))
		{
			m_eqField = false;
			m_pageNumberField = false;
			m_fldChar = true;
		}
		else if(!strcmp(fldCharType, "end"))
		{
			m_eqField = false;
			m_pageNumberField = false;
			m_fldChar = false;
		}
	} else if(nameMatches(rqst->pName, NS_W_KEY, "p")) {
		//New paragraph...
		OXML_SharedElement elem(new OXML_Element_Paragraph(""));

		/* OOXML: a paragraph without w:pStyle uses the document's
		 * Normal style. The importer stores the file's real Normal
		 * under "_Normal" (docDefaults occupies "Normal"), so point
		 * unstyled paragraphs at it; an explicit pStyle overrides. */
		OXML_Document * doc = OXML_Document::getInstance();
		OXML_SharedStyle pNormal =
			doc ? doc->getStyleById("_Normal") : OXML_SharedStyle();
		if (pNormal.get() != nullptr &&
			pNormal->getName().compare(""))
		{
			elem->setAttribute(PT_STYLE_ATTRIBUTE_NAME,
							   pNormal->getName().c_str());
		}

		rqst->stck->push(elem);

		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_W_KEY, "altChunk")) {
		/* external subdocument reference (html/mht/docx/rtf...) —
		 * keep the resolved part path + format on an empty paragraph
		 * so the link survives in .abwn for round-tripping */
		OXML_Element_Paragraph * para = new OXML_Element_Paragraph("");
		const gchar * id = attrMatches(NS_R_KEY, "id", rqst->ppAtts);
		if (id) {
			OXMLi_PackageManager * mgr = OXMLi_PackageManager::getInstance();
			if (mgr) {
				std::string target = mgr->getPartName(id);
				if (!target.empty()) {
					para->setProperty("altchunk-path", target.c_str());
					std::string::size_type dot = target.rfind('.');
					para->setProperty("altchunk-format",
						dot != std::string::npos ? target.substr(dot + 1).c_str() : "");
				}
			}
		}
		rqst->stck->push(OXML_SharedElement(para));
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_W_KEY, "r")) {
		//New text run...
		OXML_SharedElement elem(new OXML_Element_Run(""));
		rqst->stck->push(elem);

		rqst->handled = true;

	} else if (nameMatches(rqst->pName, NS_W_KEY, "t")) {
		if(m_fldChar && m_pageNumberField) // page number is already set with field element correctly.
		{
			rqst->handled = true;
			return;
		}
		//New text...
		OXML_SharedElement elem(new OXML_Element_Text("", 0));
		rqst->stck->push(elem);
		rqst->handled = true;

	} else if (nameMatches(rqst->pName, NS_W_KEY, "sectPr")) {
		//Verify the context...
		std::string contextTag = rqst->context->back();
		if (contextMatches(contextTag, NS_W_KEY, "pPr") ||
			contextMatches(contextTag, NS_W_KEY, "body")) {
			if (contextMatches(contextTag, NS_W_KEY, "pPr") && !rqst->stck->empty()) {
				// This paragraph's mark is the section break. Word does
				// not paint paragraph borders on such break paragraphs;
				// flag it so layout can suppress them (kept in .abw as
				// the "section-break" paragraph property).
				rqst->stck->top()->setProperty("section-break", "1");
			}
			OXML_SharedElement dummy(new OXML_Element_Paragraph(""));
			rqst->stck->push(dummy);

			m_pendingSectBreak = true;
			rqst->handled = true;
		}

/********************************
 ****  PARAGRAPH FORMATTING  ****
 ********************************/
	} else if(nameMatches(rqst->pName, NS_W_KEY, "shd")) {
		std::string contextTag = rqst->context->back();

		if(!contextMatches(contextTag, NS_W_KEY, "pPr") &&
			!contextMatches(contextTag, NS_W_KEY, "rPr"))
			return;

		// w:pPr/w:rPr/w:shd is paragraph-mark shading, not a run or
		// paragraph shading - ignore it.
		if (contextMatches(contextTag, NS_W_KEY, "rPr") &&
			rqst->context->size() >= 3 &&
			contextMatches(rqst->context->at(rqst->context->size() - 2), NS_W_KEY, "pPr"))
		{
			rqst->handled = true;
			return;
		}

		const gchar* fill = attrMatches(NS_W_KEY, "fill", rqst->ppAtts);

		OXML_SharedElement elem = rqst->stck->top();

		if(fill && strcmp(fill, "auto")) 
		{
			UT_Error err = UT_OK;
			err = elem->setProperty("bgcolor", fill);
			if(err != UT_OK) {
				UT_DEBUGMSG(("FRT:OpenXML importer can't set background-color:%s\n", fill));	
			}
		}
		rqst->handled = true;

	} else if ( nameMatches(rqst->pName, NS_W_KEY, "pageBreakBefore")){
		OXML_ElementTag tag = PG_BREAK;
		OXML_SharedElement br ( new OXML_Element("", tag, SPAN) );
		rqst->stck->push(br);
		rqst->handled = true;

	} else if ( nameMatches(rqst->pName, NS_W_KEY, "tab")){
		//verify the context
		std::string contextTag = rqst->context->back();

		if (contextMatches(contextTag, NS_W_KEY, "r")) {
			//This is an actual tab to be inserted
			OXML_SharedElement tab ( new OXML_Element_Text("\t", 2) );
			rqst->stck->push(tab);
			rqst->handled = true;
		}
		else if(contextMatches(contextTag, NS_W_KEY, "tabs")){
			OXML_SharedElement para = rqst->stck->top();
			const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			const gchar* pos = attrMatches(NS_W_KEY, "pos", rqst->ppAtts);
			const gchar* leadCh = attrMatches(NS_W_KEY, "leader", rqst->ppAtts);
			if(!val || !*val || !pos || !*pos)
				return;

			std::string value(val);
			std::string position(_TwipsToInches(pos));
			position += "in";
			std::string leader("0"); //no leader by default
			
			std::string tabstops("");
			const gchar* tabProp = nullptr;
			para->getProperty("tabstops", tabProp);	
			if(tabProp)
			{
				tabstops = tabProp;
				tabstops += ",";
			}

			tabstops += position;
			tabstops += "/";

			if(!value.compare("left"))
				tabstops += "L";
			else if(!value.compare("right"))
				tabstops += "R";
			else if(!value.compare("center"))
				tabstops += "C";
			else if(!value.compare("bar"))
				tabstops += "B";
			else if(!value.compare("decimal"))
				tabstops += "D";

			if(leadCh)
			{
				std::string leaderChar(leadCh);
				if(!leaderChar.compare("dot"))
					leader = "1";
				else if(!leaderChar.compare("heavy"))
					leader = "3";
				else if(!leaderChar.compare("hyphen"))
					leader = "2";
				else if(!leaderChar.compare("middleDot"))
					leader = "1";
				else if(!leaderChar.compare("underscore"))
					leader = "3";
			}
			tabstops += leader;
				
			para->setProperty("tabstops", tabstops);						
		}		
		rqst->handled = true;
	} else if ( nameMatches(rqst->pName, NS_W_KEY, "noBreakHyphen") ||
				nameMatches(rqst->pName, NS_W_KEY, "softHyphen") ) {
		std::string contextTag = rqst->context->back();
		if (contextMatches(contextTag, NS_W_KEY, "r")) {
			//U+2011 NON-BREAKING HYPHEN / U+00AD SOFT HYPHEN (UTF-8)
			const char * ch = nameMatches(rqst->pName, NS_W_KEY, "noBreakHyphen")
				? "\xE2\x80\x91" : "\xC2\xAD";
			OXML_SharedElement t( new OXML_Element_Text(ch, strlen(ch)) );
			rqst->stck->push(t);
		}
		rqst->handled = true;
	} else if ( nameMatches(rqst->pName, NS_W_KEY, "sym")) {
		std::string contextTag = rqst->context->back();
		if (contextMatches(contextTag, NS_W_KEY, "r")) {
			const gchar * font = attrMatches(NS_W_KEY, "font", rqst->ppAtts);
			const gchar * chAttr = attrMatches(NS_W_KEY, "char", rqst->ppAtts);
			if (chAttr && *chAttr) {
				UT_UCS4Char ucs = (UT_UCS4Char)strtoul(chAttr, nullptr, 16);
				if (ucs >= 0xF020 && ucs <= 0xF0FE)
					ucs -= 0xF000; //strip PUA marker used by some writers
				if (font && !strcmp(font, "Symbol"))
					ucs = _symbolCharToUnicode(ucs);
				if (ucs) {
					UT_UCS4String s;
					s += ucs;
					OXML_SharedElement t( new OXML_Element_Text(s.utf8_str(), strlen(s.utf8_str())) );
					rqst->stck->push(t);
				}
			}
		}
		rqst->handled = true;
	} else if ( nameMatches(rqst->pName, NS_W_KEY, "ilvl")){
		//verify the context
		std::string contextTag = rqst->context->back();
		if(contextMatches(contextTag, NS_W_KEY, "numPr")){
			OXML_SharedElement para = rqst->stck->top();
			const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			if(!val || !*val)
				return;
			std::string level(val);
			para->setAttribute("level", level.c_str());	
		}		
		rqst->handled = true;
	} else if ( nameMatches(rqst->pName, NS_W_KEY, "numId")){
		//verify the context
		std::string contextTag = rqst->context->back();
		if(contextMatches(contextTag, NS_W_KEY, "numPr")){
			OXML_SharedElement para = rqst->stck->top();
			const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			if(!val || !*val)
				return;
			std::string numId(val);

			OXML_Document* doc = OXML_Document::getInstance();
			std::string absNumId = doc->getMappedNumberingId(numId);
			if(!absNumId.empty())
				para->setAttribute("listid", absNumId.c_str());					
		}		
		rqst->handled = true;
	} else if ( nameMatches(rqst->pName, NS_W_KEY, "jc") ||
				nameMatches(rqst->pName, NS_W_KEY, "ind") ||
				nameMatches(rqst->pName, NS_W_KEY, "spacing") ||
				nameMatches(rqst->pName, NS_W_KEY, "contextualSpacing") ||
				nameMatches(rqst->pName, NS_W_KEY, "keepNext") ||
				nameMatches(rqst->pName, NS_W_KEY, "keepLines") ||
				nameMatches(rqst->pName, NS_W_KEY, "widowControl") ||
				nameMatches(rqst->pName, NS_W_KEY, "framePr") ||
				nameMatches(rqst->pName, NS_W_KEY, "bidi") ||
				nameMatches(rqst->pName, NS_W_KEY, "outlineLvl") ||
				nameMatches(rqst->pName, NS_W_KEY, "textAlignment") ||
				nameMatches(rqst->pName, NS_W_KEY, "snapToGrid") ||
				nameMatches(rqst->pName, NS_W_KEY, "kinsoku") ||
				nameMatches(rqst->pName, NS_W_KEY, "wordWrap") ||
				nameMatches(rqst->pName, NS_W_KEY, "suppressLineNumbers") ||
				nameMatches(rqst->pName, NS_W_KEY, "suppressAutoHyphens") ||
				nameMatches(rqst->pName, NS_W_KEY, "mirrorIndents") ||
				nameMatches(rqst->pName, NS_W_KEY, "adjustRightInd") ||
				nameMatches(rqst->pName, NS_W_KEY, "autoSpaceDE") ||
				nameMatches(rqst->pName, NS_W_KEY, "autoSpaceDN") ||
				nameMatches(rqst->pName, NS_W_KEY, "overflowPunct") ||
				nameMatches(rqst->pName, NS_W_KEY, "topLinePunct") ||
				nameMatches(rqst->pName, NS_W_KEY, "pStyle")) {
	//Verify the context...
	std::string contextTag = rqst->context->at(rqst->context->size() - 2);
	if (contextMatches(contextTag, NS_W_KEY, "p") ||
		contextMatches(contextTag, NS_W_KEY, "pPrDefault") ||
		contextMatches(contextTag, NS_W_KEY, "lvl") ||  
		contextMatches(contextTag, NS_W_KEY, "style")) { 

		OXML_SharedElement para = rqst->stck->top();

		if (nameMatches(rqst->pName, NS_W_KEY, "jc")) {
			const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);

			if (!val || !*val)
				return;

			if (!strcmp(val, "left")) {
				UT_return_if_fail( _error_if_fail( UT_OK == para->setProperty("text-align", "left") ));
			} else if (!strcmp(val, "center")) {
				UT_return_if_fail( _error_if_fail( UT_OK == para->setProperty("text-align", "center") ));
			} else if (!strcmp(val, "right")) {
				UT_return_if_fail( _error_if_fail( UT_OK == para->setProperty("text-align", "right") ));
			} else if (!strcmp(val, "numTab")) {
				//Deprecated; align left
				UT_return_if_fail( _error_if_fail( UT_OK == para->setProperty("text-align", "left") ));
			} else {
				//We justify for the values of "both", "distribute", "thaiDistribute", and the kashida variants
				UT_return_if_fail( _error_if_fail( UT_OK == para->setProperty("text-align", "justify") ));
			}

		} else if (nameMatches(rqst->pName, NS_W_KEY, "contextualSpacing")) {
			/* OOXML w:contextualSpacing - collapse spacing between
			 * adjacent paragraphs sharing the same style. Optional
			 * w:val on/off switch, on when absent */
			const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			bool bOn = !val || !*val ||
				!strcmp(val, "true") || !strcmp(val, "1") ||
				!strcmp(val, "on");
			UT_return_if_fail( _error_if_fail( UT_OK ==
				para->setProperty("contextual-spacing",
								  bOn ? "1" : "0") ));

		} else if (nameMatches(rqst->pName, NS_W_KEY, "keepNext") ||
				nameMatches(rqst->pName, NS_W_KEY, "keepLines") ||
				nameMatches(rqst->pName, NS_W_KEY, "widowControl")) {
			/* OOXML on/off switches; w:val absent or true/1/on = enabled */
			const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			bool bOn = !val || !*val ||
				!strcmp(val, "true") || !strcmp(val, "1") ||
				!strcmp(val, "on");
			const char * prop = "keep-with-next";
			const char * onVal = "yes", * offVal = "no";
			if (nameMatches(rqst->pName, NS_W_KEY, "keepLines")) {
				prop = "keep-together";
			} else if (nameMatches(rqst->pName, NS_W_KEY, "widowControl")) {
				prop = "widows";
				onVal = "2"; //default widow/orphan count
				offVal = "0";
			}
			UT_return_if_fail( _error_if_fail( UT_OK ==
				para->setProperty(prop, bOn ? onVal : offVal) ));

		} else if (nameMatches(rqst->pName, NS_W_KEY, "bidi")) {
			const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			bool bOn = !val || !*val ||
				!strcmp(val, "true") || !strcmp(val, "1") ||
				!strcmp(val, "on");
			UT_return_if_fail( _error_if_fail( UT_OK ==
				para->setProperty("dom-dir", bOn ? "rtl" : "ltr") ));

		} else if (nameMatches(rqst->pName, NS_W_KEY, "outlineLvl")) {
			const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			if (val && *val)
				para->setProperty("outline-level", val);

		} else if (nameMatches(rqst->pName, NS_W_KEY, "textAlignment")) {
			//auto|baseline|top|center|bottom - vertical run alignment
			const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			if (val && *val)
				para->setProperty("baseline-align", val);

		} else if (nameMatches(rqst->pName, NS_W_KEY, "snapToGrid") ||
				   nameMatches(rqst->pName, NS_W_KEY, "kinsoku") ||
				   nameMatches(rqst->pName, NS_W_KEY, "wordWrap") ||
				   nameMatches(rqst->pName, NS_W_KEY, "suppressLineNumbers") ||
				   nameMatches(rqst->pName, NS_W_KEY, "suppressAutoHyphens") ||
				   nameMatches(rqst->pName, NS_W_KEY, "mirrorIndents") ||
				   nameMatches(rqst->pName, NS_W_KEY, "adjustRightInd") ||
				   nameMatches(rqst->pName, NS_W_KEY, "autoSpaceDE") ||
				   nameMatches(rqst->pName, NS_W_KEY, "autoSpaceDN") ||
				   nameMatches(rqst->pName, NS_W_KEY, "overflowPunct") ||
				   nameMatches(rqst->pName, NS_W_KEY, "topLinePunct")) {
			//OOXML on/off paragraph switches preserved as 0/1 props
			const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			bool bOn = !val || !*val ||
				!strcmp(val, "true") || !strcmp(val, "1") ||
				!strcmp(val, "on");
			const char * prop = nullptr;
			if (nameMatches(rqst->pName, NS_W_KEY, "snapToGrid")) prop = "snap-to-grid";
			else if (nameMatches(rqst->pName, NS_W_KEY, "kinsoku")) prop = "kinsoku";
			else if (nameMatches(rqst->pName, NS_W_KEY, "wordWrap")) prop = "word-wrap";
			else if (nameMatches(rqst->pName, NS_W_KEY, "suppressLineNumbers")) prop = "suppress-line-numbers";
			else if (nameMatches(rqst->pName, NS_W_KEY, "suppressAutoHyphens")) prop = "suppress-auto-hyphens";
			else if (nameMatches(rqst->pName, NS_W_KEY, "mirrorIndents")) prop = "mirror-indents";
			else if (nameMatches(rqst->pName, NS_W_KEY, "adjustRightInd")) prop = "adjust-right-ind";
			else if (nameMatches(rqst->pName, NS_W_KEY, "autoSpaceDE")) prop = "auto-space-de";
			else if (nameMatches(rqst->pName, NS_W_KEY, "autoSpaceDN")) prop = "auto-space-dn";
			else if (nameMatches(rqst->pName, NS_W_KEY, "overflowPunct")) prop = "overflow-punct";
			else if (nameMatches(rqst->pName, NS_W_KEY, "topLinePunct")) prop = "top-line-punct";
			if (prop)
				para->setProperty(prop, bOn ? "1" : "0");

		} else if (nameMatches(rqst->pName, NS_W_KEY, "framePr")) {
			/* w:framePr - paragraph framed as positioned text. Stored as
			 * props; Paragraph::addToPT wraps the block in a frame strux */
			const gchar * v;
			auto twipProp = [&](const char * key, const char * attr) {
				const gchar * a = attrMatches(NS_W_KEY, attr, rqst->ppAtts);
				if (a) {
					std::string dim(_TwipsToPoints(a));
					dim += "pt";
					para->setProperty(key, dim.c_str());
				}
			};
			twipProp("framePr-x", "x");
			twipProp("framePr-y", "y");
			twipProp("framePr-w", "w");
			twipProp("framePr-h", "h");
			if ((v = attrMatches(NS_W_KEY, "hAnchor", rqst->ppAtts)))
				para->setProperty("framePr-hAnchor", v);
			if ((v = attrMatches(NS_W_KEY, "vAnchor", rqst->ppAtts)))
				para->setProperty("framePr-vAnchor", v);
			if ((v = attrMatches(NS_W_KEY, "wrap", rqst->ppAtts)))
				para->setProperty("framePr-wrap", v);

		} else if (nameMatches(rqst->pName, NS_W_KEY, "ind")) {
			const gchar * left = attrMatches(NS_W_KEY, "left", rqst->ppAtts);
			const gchar * right = attrMatches(NS_W_KEY, "right", rqst->ppAtts);
			const gchar * fLine = attrMatches(NS_W_KEY, "firstLine", rqst->ppAtts);
			const gchar * hanging = attrMatches(NS_W_KEY, "hanging", rqst->ppAtts);

			std::string final = "";
			if (left != nullptr) {
				final = _TwipsToPoints(left); //convert to points
				final += "pt";
				UT_return_if_fail( _error_if_fail( UT_OK == para->setProperty("margin-left", final.c_str()) ));
			}
			if (right != nullptr) {
				final = _TwipsToPoints(right); //convert to points
				final += "pt";
				UT_return_if_fail( _error_if_fail( UT_OK == para->setProperty("margin-right", final.c_str()) ));
			}
			if (fLine != nullptr) {
				final = _TwipsToPoints(fLine); //convert to points
				final += "pt";
				UT_return_if_fail( _error_if_fail( UT_OK == para->setProperty("text-indent", final.c_str()) ));
			} else if (hanging != nullptr) {
				final = _TwipsToPoints(hanging); //convert to points
				//This is hanging, invert the sign
				if (final[0] == '-')
					final.erase(0,1);
				else
					final.insert(0,1,'-');
				final += "pt";
				UT_return_if_fail( _error_if_fail( UT_OK == para->setProperty("text-indent", final.c_str()) ));
			}

		} else if (nameMatches(rqst->pName, NS_W_KEY, "spacing")) {
			const gchar * before = attrMatches(NS_W_KEY, "before", rqst->ppAtts);
			const gchar * after = attrMatches(NS_W_KEY, "after", rqst->ppAtts);
			const gchar * lineRule = attrMatches(NS_W_KEY, "lineRule", rqst->ppAtts);

			std::string final = "";
			if (before != nullptr) {
				final = _TwipsToPoints(before); //convert to points
				final += "pt";
				UT_return_if_fail( _error_if_fail( UT_OK == para->setProperty("margin-top", final.c_str()) ));
			}
			if (after != nullptr) {
				final = _TwipsToPoints(after); //convert to points
				final += "pt";
				UT_return_if_fail( _error_if_fail( UT_OK == para->setProperty("margin-bottom", final.c_str()) ));
			}
			if (lineRule != nullptr && !strcmp(lineRule, "auto")) {
				//For now, we only handle "auto".
				const gchar * line = attrMatches(NS_W_KEY, "line", rqst->ppAtts);
				UT_return_if_fail( _error_if_fail(line != nullptr) );
				double ln_spc = UT_convertDimensionless(line) / 240;
				final = UT_convertToDimensionlessString(ln_spc);
				UT_return_if_fail( _error_if_fail( UT_OK == para->setProperty("line-height", final.c_str()) ));
			}
		} else if (nameMatches(rqst->pName, NS_W_KEY, "pStyle")) {
			const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			UT_return_if_fail( _error_if_fail(val != nullptr) );
			if (!strcmp(val, "Normal")) val = "_Normal"; //Cannot interfere with document defaults
			OXML_Document * doc = OXML_Document::getInstance();
			UT_return_if_fail( _error_if_fail(doc != nullptr) );
			OXML_SharedStyle ref = doc->getStyleById(val);
			if (ref.get() != nullptr && ref->getName().compare("")) {
				UT_return_if_fail( _error_if_fail( UT_OK == para->setAttribute(PT_STYLE_ATTRIBUTE_NAME, ref->getName().c_str()) ));
			}

		}

		rqst->handled = true;
	}
	} else if (nameMatches(rqst->pName, NS_W_KEY, "pBdr")) {
		/* Paragraph border container - the <w:top>/<w:left>/
		 * <w:bottom>/<w:right> edges are handled below; nothing
		 * needs to be pushed for the container itself. */
		rqst->handled = true;

	} else if (nameMatches(rqst->pName, NS_W_KEY, "between") ||
			   nameMatches(rqst->pName, NS_W_KEY, "bar")) {
		/* <w:between> and <w:bar> inside <w:pBdr> have no Abinova
		 * equivalent - accept and ignore them. */
		if (!rqst->context->empty() &&
			contextMatches(rqst->context->back(), NS_W_KEY, "pBdr"))
			rqst->handled = true;

	} else if (nameMatches(rqst->pName, NS_W_KEY, "top") ||
			   nameMatches(rqst->pName, NS_W_KEY, "left") ||
			   nameMatches(rqst->pName, NS_W_KEY, "bottom") ||
			   nameMatches(rqst->pName, NS_W_KEY, "right")) {
		/* <w:pBdr> edges land on the paragraph element;
		 * <w:pgBorders> edges land on the section as
		 * page-border-<side>-* properties. */
		bool pgBorder = !rqst->context->empty() &&
			contextMatches(rqst->context->back(), NS_W_KEY, "pgBorders");
		if (rqst->context->empty() ||
			(!contextMatches(rqst->context->back(), NS_W_KEY, "pBdr") &&
			 !pgBorder))
			return;

		if (pgBorder)
		{
			UT_return_if_fail(_error_if_fail(
				rqst->sect_stck && !rqst->sect_stck->empty()));
			OXML_SharedSection sect = rqst->sect_stck->top();
			UT_return_if_fail(_error_if_fail(sect.get() != nullptr));

			std::string pfx("page-border-");
			std::string side(rqst->pName);
			pfx += side.substr(strlen(NS_W_KEY) + 1);

			const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			const gchar * sz = attrMatches(NS_W_KEY, "sz", rqst->ppAtts);
			const gchar * space = attrMatches(NS_W_KEY, "space", rqst->ppAtts);
			const gchar * color = attrMatches(NS_W_KEY, "color", rqst->ppAtts);
			const gchar * theme = attrMatches(NS_W_KEY, "themeColor", rqst->ppAtts);
			const gchar * shadow = attrMatches(NS_W_KEY, "shadow", rqst->ppAtts);

			if (val && *val)
			{
				if (!strcmp(val, "none") || !strcmp(val, "nil"))
					sect->setProperty(pfx.c_str(), "none");
				else
				{
					static const char * lineStyles[] = {
						"single","double","triple","dotted","dashed",
						"dashLargeGap","dashSmallGap","dotDash",
						"dotDotDash","dashDotStroked","doubleWave","wave",
						"thick","thinThickSmallGap","thinThickMediumGap",
						"thinThickLargeGap","thickThinSmallGap",
						"thickThinMediumGap","thickThinLargeGap",
						"thinThickThinSmallGap","thinThickThinMediumGap",
						"thinThickThinLargeGap","threeDEmboss",
						"threeDEngrave","inset","outset", nullptr };
					bool lineStyle = false;
					for (int i = 0; lineStyles[i]; i++)
						if (!strcmp(val, lineStyles[i])) { lineStyle = true; break; }
					if (lineStyle)
						sect->setProperty(pfx.c_str(), val);
					else
					{
						// a Page Border Art name (e.g. "apples")
						sect->setProperty(pfx.c_str(), "art");
						sect->setProperty((pfx + "-art").c_str(), val);
					}
				}
			}
			if (sz && *sz)
			{
				std::string thick(_EighthPointsToPoints(sz));
				thick += "pt";
				sect->setProperty((pfx + "-thickness").c_str(), thick.c_str());
			}
			if (space && *space)
			{
				std::string sp(space);
				sp += "pt";
				sect->setProperty((pfx + "-space").c_str(), sp.c_str());
			}
			if (color && *color && strcmp(color, "auto"))
				sect->setProperty((pfx + "-color").c_str(), color);
			else if (theme && *theme)
			{
				std::string tcolor(_resolveThemeColor(theme));
				if (!tcolor.empty())
					sect->setProperty((pfx + "-color").c_str(), tcolor.c_str());
			}
			if (shadow && *shadow)
				sect->setProperty((pfx + "-shadow").c_str(),
					(!strcmp(shadow,"on") || !strcmp(shadow,"1") ||
					 !strcmp(shadow,"true")) ? "1" : "0");
			rqst->handled = true;
			return;
		}


		OXML_SharedElement para = rqst->stck->top();
		UT_return_if_fail( _error_if_fail( para.get() != nullptr ) );

		std::string edge(rqst->pName);
		edge = edge.substr(strlen(NS_W_KEY) + 1);
		if (!edge.compare("bottom"))
			edge = "bot"; /* Abinova spells it "bot-" */

		const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
		const gchar * sz = attrMatches(NS_W_KEY, "sz", rqst->ppAtts);
		const gchar * space = attrMatches(NS_W_KEY, "space", rqst->ppAtts);
		const gchar * color = attrMatches(NS_W_KEY, "color", rqst->ppAtts);
		const gchar * theme = attrMatches(NS_W_KEY, "themeColor", rqst->ppAtts);

		/* Abinova edge styles: 0 none, 1 solid, 2 dotted, 3 dashed,
		 * 4 double, 5 dashdot, 6 dashdotdot, 7 longdash, 8 triple,
		 * 9 wave.  Remaining OOXML values degrade to solid. */
		std::string style = "1";
		if (val && *val)
		{
			if (!strcmp(val, "none") || !strcmp(val, "nil"))
				style = "0";
			else if (!strcmp(val, "dotted"))
				style = "2";
			else if (!strcmp(val, "dashed") ||
					 !strcmp(val, "dashSmallGap"))
				style = "3";
			else if (!strcmp(val, "double") ||
					 !strncmp(val, "thinThick", 9) ||
					 !strncmp(val, "thickThin", 9))
				style = "4";
			else if (!strcmp(val, "dotDash") ||
					 !strcmp(val, "dashDotStroked"))
				style = "5";
			else if (!strcmp(val, "dotDotDash"))
				style = "6";
			else if (!strcmp(val, "dashLargeGap"))
				style = "7";
			else if (!strcmp(val, "triple"))
				style = "8";
			else if (!strcmp(val, "wave") ||
					 !strcmp(val, "doubleWave"))
				style = "9";
		}
		UT_return_if_fail( _error_if_fail( UT_OK ==
			para->setProperty((edge + "-style").c_str(), style.c_str()) ));

		if (sz && *sz)
		{
			std::string thick(_EighthPointsToPoints(sz));
			thick += "pt";
			UT_return_if_fail( _error_if_fail( UT_OK ==
				para->setProperty((edge + "-thickness").c_str(), thick.c_str()) ));
		}
		if (space && *space)
		{
			std::string sp(space);
			sp += "pt";
			UT_return_if_fail( _error_if_fail( UT_OK ==
				para->setProperty((edge + "-space").c_str(), sp.c_str()) ));
		}
		if (color && *color && strcmp(color, "auto"))
		{
			UT_return_if_fail( _error_if_fail( UT_OK ==
				para->setProperty((edge + "-color").c_str(), color) ));
		}
		else if (theme && *theme)
		{
			std::string tcolor(_resolveThemeColor(theme));
			if (!tcolor.empty())
			{
				UT_return_if_fail( _error_if_fail( UT_OK ==
					para->setProperty((edge + "-color").c_str(), tcolor.c_str()) ));
			}
		}
		rqst->handled = true;
	}

/******* END OF PARAGRAPH FORMATTING ********/

/**************************
 ****  RUN FORMATTING  ****
 **************************/
	else if (	nameMatches(rqst->pName, NS_W_KEY, "b") || 
				nameMatches(rqst->pName, NS_W_KEY, "i") || 
				nameMatches(rqst->pName, NS_W_KEY, "u") ||
				nameMatches(rqst->pName, NS_W_KEY, "color") ||
				nameMatches(rqst->pName, NS_W_KEY, "vertAlign") || // for subscript and superscript
				nameMatches(rqst->pName, NS_W_KEY, "highlight") ||
				nameMatches(rqst->pName, NS_W_KEY, "strike") ||
				nameMatches(rqst->pName, NS_W_KEY, "dstrike") ||
				nameMatches(rqst->pName, NS_W_KEY, "rFonts") ||
				nameMatches(rqst->pName, NS_W_KEY, "lang") ||
				nameMatches(rqst->pName, NS_W_KEY, "noProof") ||
				nameMatches(rqst->pName, NS_W_KEY, "vanish") ||
				nameMatches(rqst->pName, NS_W_KEY, "specVanish") ||
				nameMatches(rqst->pName, NS_W_KEY, "webHidden") ||
				nameMatches(rqst->pName, NS_W_KEY, "caps") ||
				nameMatches(rqst->pName, NS_W_KEY, "smallCaps") ||
				nameMatches(rqst->pName, NS_W_KEY, "w") ||
				nameMatches(rqst->pName, NS_W_KEY, "rtl") ||
				nameMatches(rqst->pName, NS_W_KEY, "kern") ||
				nameMatches(rqst->pName, NS_W_KEY, "spacing") ||
				nameMatches(rqst->pName, NS_W_KEY, "em") ||
				nameMatches(rqst->pName, NS_W_KEY, "position") ||
				nameMatches(rqst->pName, NS_W_KEY, "sz") ) {
		//Verify the context...
		std::string contextTag = rqst->context->at(rqst->context->size() - 2);
		// w:pPr/w:rPr is the paragraph MARK's formatting, not text
		// formatting. The only property meaningful to us is its font
		// size, which controls the height of an empty paragraph line.
		// (Also skipped inside styles: there pPr/rPr belongs to the
		// style definition and must not override its font-size.)
		bool bParaMark = contextMatches(contextTag, NS_W_KEY, "pPr") &&
						 nameMatches(rqst->pName, NS_W_KEY, "sz") &&
						 rqst->context->size() >= 3 &&
						 contextMatches(rqst->context->at(rqst->context->size() - 3), NS_W_KEY, "p");
		if (contextMatches(contextTag, NS_W_KEY, "r") ||
			contextMatches(contextTag, NS_W_KEY, "rPrDefault") ||
			contextMatches(contextTag, NS_W_KEY, "lvl") ||
			contextMatches(contextTag, NS_W_KEY, "style") ||
			bParaMark) {
			OXML_SharedElement run = rqst->stck->top();

			if (nameMatches(rqst->pName, NS_W_KEY, "b")) {
				const gchar * isOn = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				if (isOn == nullptr || !strcmp(isOn, "on") || !strcmp(isOn, "1") || !strcmp(isOn, "true") ) {
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("font-weight", "bold") ));
				} else {
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("font-weight", "normal") ));
				}

			} else if (nameMatches(rqst->pName, NS_W_KEY, "i")) {
				const gchar * isOn = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				if (isOn == nullptr || !strcmp(isOn, "on") || !strcmp(isOn, "1") || !strcmp(isOn, "true") ) {
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("font-style", "italic") ));
				} else {
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("font-style", "normal") ));
				}

			} else if (nameMatches(rqst->pName, NS_W_KEY, "vertAlign")) {
				const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				if (val == nullptr || !*val || !strcmp(val, "baseline")) {
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("text-position", "normal") ));
				} else if (!strcmp(val, "superscript")) {
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("text-position", "superscript") ));
				} else if (!strcmp(val, "subscript")) {
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("text-position", "subscript") ));
				}
				
			} else if (nameMatches(rqst->pName, NS_W_KEY, "u")) {
				const gchar * newVal = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				if(!newVal)
				{
					newVal = "single";
				}
				std::string final_val = "";
				const gchar * previousVal = nullptr;
				if (UT_OK == run->getProperty("text-decoration", previousVal)) {
					final_val = previousVal;
				}
				if ( !strcmp(newVal, "none") ) { 
					final_val += " ";
					final_val += "none";
				} else { //if NOT "none", we add underline (no matter the underline style, we only support single line)
					final_val += " ";
					final_val += "underline";
				}
				UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("text-decoration", final_val.c_str()) ));

			} else if (nameMatches(rqst->pName, NS_W_KEY, "color")) {
				const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				if (val != nullptr) {
					if (!strcmp(val, "auto")) val = "#000000";
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("color", val)));
				} else {
					val = attrMatches(NS_W_KEY, "themeColor", rqst->ppAtts);
					UT_return_if_fail( this->_error_if_fail(val != nullptr) );
					std::string color = _resolveThemeColor(val);
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("color", color.c_str())));
				}

			} else if (nameMatches(rqst->pName, NS_W_KEY, "highlight")) {
				const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				UT_return_if_fail( this->_error_if_fail(val != nullptr) );
				if (!strcmp(val, "darkYellow")) val = "olive"; //the only value not supported by CSS (equivalent to Olive)
				else if (!strcmp(val, "none")) val = "black"; //bypass inherited color value when "none"
				std::string hex = "";
				for (UT_uint32 i = 0; i <= strlen(val); i++) {
					hex += tolower(val[i]);
				}
				UT_HashColor conv;
				val = conv.setColor(hex.c_str());
				UT_return_if_fail( this->_error_if_fail( nullptr != val ) );
				UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("bgcolor", val)));

			} else if (nameMatches(rqst->pName, NS_W_KEY, "strike") || 
					   nameMatches(rqst->pName, NS_W_KEY, "dstrike")) {
				const gchar * isOn = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				std::string final_val = "";
				const gchar * previousVal = nullptr;
				if (UT_OK == run->getProperty("text-decoration", previousVal)) {
					final_val = previousVal;
				}
				if ( isOn == nullptr || !strcmp(isOn, "on") || !strcmp(isOn, "1") || !strcmp(isOn, "true")  ) {
					final_val += " ";
					final_val += "line-through";
				} else {
					final_val += " ";
					final_val += "none";
				}
				UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("text-decoration", final_val.c_str()) ));

			} else if (nameMatches(rqst->pName, NS_W_KEY, "rFonts")) {
				OXML_Document * doc = OXML_Document::getInstance();
				UT_return_if_fail( this->_error_if_fail(doc != nullptr) );
				OXML_SharedFontManager fmgr = doc->getFontManager();
				UT_return_if_fail( this->_error_if_fail(fmgr.get() != nullptr) );

				std::string fontName;
				OXML_FontLevel level = UNKNOWN_LEVEL;
				OXML_CharRange range = UNKNOWN_RANGE;

				/* Abinova has a single "font-family" property, so resolve the
				 * font for the *Latin* range only: w:ascii/asciiTheme first,
				 * then w:hAnsi/hAnsiTheme.  w:eastAsia and w:cs apply to
				 * East-Asian and complex-script characters only; when no Latin
				 * attribute is present we must leave font-family unset so the
				 * property inherits (as Word does), instead of leaking e.g. an
				 * eastAsia="Verdana" mapping onto Latin text. */
				const gchar * ascii = nullptr;
				const gchar * hAnsi = nullptr;
				if (nullptr != (ascii = attrMatches(NS_W_KEY, "asciiTheme", rqst->ppAtts))) {
					this->getFontLevelRange(ascii, level, range);
					fontName = fmgr->getValidFont(level, range); //Retrieve valid font name from Theme
				} else if (nullptr != (ascii = attrMatches(NS_W_KEY, "ascii", rqst->ppAtts))) {
					fontName = fmgr->getValidFont(ascii); //Make sure the name is valid
				} else if (nullptr != (hAnsi = attrMatches(NS_W_KEY, "hAnsiTheme", rqst->ppAtts))) {
					this->getFontLevelRange(hAnsi, level, range);
					fontName = fmgr->getValidFont(level, range); //Retrieve valid font name from Theme
				} else if (nullptr != (hAnsi = attrMatches(NS_W_KEY, "hAnsi", rqst->ppAtts))) {
					fontName = fmgr->getValidFont(hAnsi); //Make sure the name is valid
				}
				if (!fontName.empty())
					UT_return_if_fail( _error_if_fail( UT_OK == run->setProperty("font-family", fontName.c_str()) ));

			} else if (nameMatches(rqst->pName, NS_W_KEY, "lang")) {
				const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				const gchar * eastAsia = attrMatches(NS_W_KEY, "eastAsia", rqst->ppAtts);
				const gchar * bidi = attrMatches(NS_W_KEY, "bidi", rqst->ppAtts);
				const gchar * previousVal = nullptr;
				if (UT_OK == run->getProperty("lang", previousVal)) {
					if ( 0 != strcmp(previousVal, "-none-"))
						val = previousVal;
				}
				/* w:lang carries three independent attributes: w:val is
				 * the Latin language, w:eastAsia the East-Asian one and
				 * w:bidi the complex-script one.  Only the Latin language
				 * maps onto our single "lang" property; the others are
				 * fallbacks for runs that leave w:val unset. */
				if (val == nullptr)
					val = eastAsia ? eastAsia : bidi;
				if ( val != nullptr)
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("lang", val) ));

			} else if (nameMatches(rqst->pName, NS_W_KEY, "noProof")) {
				//noProof has priority over lang, so no need to check for previous values
				const gchar * isOn = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				if (isOn == nullptr || !strcmp(isOn, "on") || !strcmp(isOn, "1") || !strcmp(isOn, "true") )
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("lang", "-none-") ));

			} else if (nameMatches(rqst->pName, NS_W_KEY, "vanish") ||
					   nameMatches(rqst->pName, NS_W_KEY, "specVanish") ||
					   nameMatches(rqst->pName, NS_W_KEY, "webHidden")) {
				const gchar * isOn = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				if (isOn == nullptr || !strcmp(isOn, "on") || !strcmp(isOn, "1") || !strcmp(isOn, "true") ) {
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("display", "none") ));
				} else {
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("display", "inline") ));
				}

			} else if (nameMatches(rqst->pName, NS_W_KEY, "caps")) {
				const gchar * isOn = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				if (isOn == nullptr || !strcmp(isOn, "on") || !strcmp(isOn, "1") || !strcmp(isOn, "true") ) {
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("text-transform", "uppercase") ));
				} else {
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("text-transform", "none") ));
				}

			} else if (nameMatches(rqst->pName, NS_W_KEY, "smallCaps")) {
				const gchar * isOn = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				if (isOn == nullptr || !strcmp(isOn, "on") || !strcmp(isOn, "1") || !strcmp(isOn, "true") ) {
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("font-variant", "small-caps") ));
				} else {
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("font-variant", "normal") ));
				}

			} else if (nameMatches(rqst->pName, NS_W_KEY, "w")) {
				//character expansion percent -> named stretch
				const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				if (val && *val) {
					double pct = UT_convertDimensionless(val);
					if (pct < 87.5)
						run->setProperty("font-stretch", "Condensed");
					else if (pct > 112.5)
						run->setProperty("font-stretch", "Expanded");
				}

			} else if (nameMatches(rqst->pName, NS_W_KEY, "rtl")) {
				const gchar * isOn = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				if (isOn == nullptr || !strcmp(isOn, "on") || !strcmp(isOn, "1") || !strcmp(isOn, "true") ) {
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("dir-override", "rtl") ));
				} else {
					UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("dir-override", "ltr") ));
				}

			} else if (nameMatches(rqst->pName, NS_W_KEY, "sz")) {
				const gchar * szStr = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				UT_return_if_fail( this->_error_if_fail(szStr != nullptr) );
				double sz = UT_convertDimensionless(szStr) / 2;
				UT_return_if_fail( this->_error_if_fail(sz > 0) );
				std::string pt_value = UT_convertToDimensionlessString(sz);
				pt_value += "pt";
				UT_return_if_fail( this->_error_if_fail( UT_OK == run->setProperty("font-size", pt_value.c_str()) ));

			} else if (nameMatches(rqst->pName, NS_W_KEY, "kern")) {
				//half-point threshold at which kerning applies
				const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				if (val && *val) {
					double hp = UT_convertDimensionless(val);
					std::string pt(UT_convertToDimensionlessString(hp / 2.0));
					pt += "pt";
					run->setProperty("char-kern", pt.c_str());
				}

			} else if (nameMatches(rqst->pName, NS_W_KEY, "spacing")) {
				//letter spacing in twentieths of a point (signed)
				const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				if (val && *val) {
					std::string pt(_TwipsToPoints(val));
					pt += "pt";
					run->setProperty("char-spacing", pt.c_str());
				}

			} else if (nameMatches(rqst->pName, NS_W_KEY, "em")) {
				//emphasis mark: none|dot|comma|circle|underDot
				const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				if (val && *val)
					run->setProperty("char-emphasis", val);

			} else if (nameMatches(rqst->pName, NS_W_KEY, "position")) {
				//raise/lower by half-points (signed)
				const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				if (val && *val) {
					double hp = UT_convertDimensionless(val);
					std::string pt(UT_convertToDimensionlessString(hp / 2.0));
					pt += "pt";
					run->setProperty("vert-position", pt.c_str());
				}
			}
			rqst->handled = true;
		}

/******* END OF RUN FORMATTING ********/

/******************************
 ****  SECTION FORMATTING  ****
 ******************************/

	} else if (	nameMatches(rqst->pName, NS_W_KEY, "type") ||
				nameMatches(rqst->pName, NS_W_KEY, "footerReference") ||
				nameMatches(rqst->pName, NS_W_KEY, "headerReference") ||
				nameMatches(rqst->pName, NS_W_KEY, "titlePg") ||
				nameMatches(rqst->pName, NS_W_KEY, "pgNumType") ||
				nameMatches(rqst->pName, NS_W_KEY, "pgBorders") ||
				nameMatches(rqst->pName, NS_W_KEY, "vAlign") ||
				nameMatches(rqst->pName, NS_W_KEY, "textDirection") ||
				nameMatches(rqst->pName, NS_W_KEY, "paperSrc") ||
				nameMatches(rqst->pName, NS_W_KEY, "lnNumType") ||
				nameMatches(rqst->pName, NS_W_KEY, "docGrid") ||
				nameMatches(rqst->pName, NS_W_KEY, "rtlGutter") ||
				nameMatches(rqst->pName, NS_W_KEY, "formProt") ||
				nameMatches(rqst->pName, NS_W_KEY, "noEndnote") ||
				nameMatches(rqst->pName, NS_W_KEY, "cols")) {
		//Verify the context...
		std::string contextTag = rqst->context->back();
		if (contextMatches(contextTag, NS_W_KEY, "sectPr")) {
			if (nameMatches(rqst->pName, NS_W_KEY, "titlePg")) {
				const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				bool bOn = !val || !*val || !strcmp(val, "true") ||
					!strcmp(val, "1") || !strcmp(val, "on");
				OXML_SharedSection sect = rqst->sect_stck->top();
				if (sect.get())
					sect->setTitlePg(bOn);
				rqst->handled = true;
			} else if (nameMatches(rqst->pName, NS_W_KEY, "type")) {
				const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				UT_return_if_fail( this->_error_if_fail(val != nullptr) );

				// Per OOXML, w:type describes how the section that this
				// sectPr terminates starts relative to the *previous*
				// section, so it applies to the section on top of the
				// stack, not the section that follows.
				OXML_SharedSection sect = rqst->sect_stck->top();
				UT_return_if_fail( this->_error_if_fail(sect.get() != nullptr) );
				if (!strcmp(val, "continuous")) {
					sect->setBreakType(CONTINUOUS_BREAK);
				} else if (!strcmp(val, "evenPage")) {
					sect->setBreakType(EVENPAGE_BREAK);
				} else if (!strcmp(val, "oddPage")) {
					sect->setBreakType(ODDPAGE_BREAK);
				} else { //nextPage and nextColumn
					sect->setBreakType(NEXTPAGE_BREAK);
				}
				rqst->handled = true;

			} else if (nameMatches(rqst->pName, NS_W_KEY, "pgNumType")) {
				//w:pgNumType@w:start restarts page numbering in this section
				const gchar * start = attrMatches(NS_W_KEY, "start", rqst->ppAtts);
				if (start && *start) {
					OXML_SharedSection sect = rqst->sect_stck->top();
					if (sect.get()) {
						sect->setProperty("section-restart", "1");
						sect->setProperty("section-restart-value", start);
					}
				}
				rqst->handled = true;

			} else if (nameMatches(rqst->pName, NS_W_KEY, "pgBorders")) {
				const gchar * off = attrMatches(NS_W_KEY, "offsetFrom", rqst->ppAtts);
				const gchar * disp = attrMatches(NS_W_KEY, "display", rqst->ppAtts);
				OXML_SharedSection sect = rqst->sect_stck->top();
				if (sect.get()) {
					if (off && *off)
						sect->setProperty("page-border-offset", off);
					if (disp && *disp)
						sect->setProperty("page-border-display", disp);
				}
				rqst->handled = true;

			} else if (nameMatches(rqst->pName, NS_W_KEY, "vAlign")) {
				const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				OXML_SharedSection sect = rqst->sect_stck->top();
				if (sect.get() && val && *val)
					sect->setProperty("section-y-align", val);
				rqst->handled = true;

			} else if (nameMatches(rqst->pName, NS_W_KEY, "textDirection")) {
				const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				OXML_SharedSection sect = rqst->sect_stck->top();
				if (sect.get() && val && *val)
					sect->setProperty("section-text-direction", val);
				rqst->handled = true;

			} else if (nameMatches(rqst->pName, NS_W_KEY, "paperSrc")) {
				OXML_SharedSection sect = rqst->sect_stck->top();
				if (sect.get()) {
					const gchar * f = attrMatches(NS_W_KEY, "first", rqst->ppAtts);
					const gchar * o = attrMatches(NS_W_KEY, "other", rqst->ppAtts);
					if (f && *f)
						sect->setProperty("section-paper-src-first", f);
					if (o && *o)
						sect->setProperty("section-paper-src-other", o);
				}
				rqst->handled = true;

			} else if (nameMatches(rqst->pName, NS_W_KEY, "lnNumType")) {
				OXML_SharedSection sect = rqst->sect_stck->top();
				if (sect.get()) {
					struct { const char * a; const char * p; } m[] = {
						{"countBy", "section-ln-count-by"},
						{"start",   "section-ln-start"},
						{"distance","section-ln-distance"},
						{"restart", "section-ln-restart"} };
					for (auto & e : m) {
						const gchar * v = attrMatches(NS_W_KEY, e.a, rqst->ppAtts);
						if (v && *v)
							sect->setProperty(e.p, v);
					}
				}
				rqst->handled = true;

			} else if (nameMatches(rqst->pName, NS_W_KEY, "docGrid")) {
				OXML_SharedSection sect = rqst->sect_stck->top();
				if (sect.get()) {
					struct { const char * a; const char * p; } m[] = {
						{"type",      "section-doc-grid"},
						{"linePitch", "section-doc-grid-line-pitch"},
						{"charSpace", "section-doc-grid-char-space"} };
					for (auto & e : m) {
						const gchar * v = attrMatches(NS_W_KEY, e.a, rqst->ppAtts);
						if (v && *v)
							sect->setProperty(e.p, v);
					}
				}
				rqst->handled = true;

			} else if (nameMatches(rqst->pName, NS_W_KEY, "rtlGutter") ||
					   nameMatches(rqst->pName, NS_W_KEY, "formProt") ||
					   nameMatches(rqst->pName, NS_W_KEY, "noEndnote")) {
				const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
				bool bOn = !val || !*val || !strcmp(val, "true") ||
					!strcmp(val, "1") || !strcmp(val, "on");
				const char * prop =
					nameMatches(rqst->pName, NS_W_KEY, "rtlGutter") ?
						"section-rtl-gutter" :
					nameMatches(rqst->pName, NS_W_KEY, "formProt") ?
						"section-form-protected" : "section-endnote-suppress";
				OXML_SharedSection sect = rqst->sect_stck->top();
				if (sect.get())
					sect->setProperty(prop, bOn ? "1" : "0");
				rqst->handled = true;

			} else if (nameMatches(rqst->pName, NS_W_KEY, "footerReference")) {
				const gchar * id = attrMatches(NS_R_KEY, "id", rqst->ppAtts);
				UT_return_if_fail( this->_error_if_fail(id != nullptr) );
				OXML_SharedSection last = rqst->sect_stck->top();

				OXMLi_PackageManager * mgr = OXMLi_PackageManager::getInstance();
				UT_return_if_fail( _error_if_fail( UT_OK == mgr->parseDocumentHdrFtr(id) ) );

				OXML_Document * doc = OXML_Document::getInstance();
				UT_return_if_fail(_error_if_fail(doc != nullptr));
				const gchar * type = attrMatches(NS_W_KEY, "type", rqst->ppAtts);
				UT_return_if_fail( this->_error_if_fail(type != nullptr) );

				if (!strcmp(type, "default")) {
					last->setFooterId(id, DEFAULT_HDRFTR);
					type = "footer";
				} else if (!strcmp(type, "even")) {
					last->setFooterId(id, EVENPAGE_HDRFTR);
					type = "footer-even";
				} else {
					last->setFooterId(id, FIRSTPAGE_HDRFTR);
					type = "footer-first";
				}

				OXML_SharedSection ftr = doc->getFooter(id);
				UT_return_if_fail(_error_if_fail( UT_OK == ftr->setAttribute("type", type) ));

			} else if (nameMatches(rqst->pName, NS_W_KEY, "headerReference")) {
				const gchar * id = attrMatches(NS_R_KEY, "id", rqst->ppAtts);
				UT_return_if_fail( this->_error_if_fail(id != nullptr) );
				OXML_SharedSection last = rqst->sect_stck->top();

				OXMLi_PackageManager * mgr = OXMLi_PackageManager::getInstance();
				UT_return_if_fail( _error_if_fail( UT_OK == mgr->parseDocumentHdrFtr(id) ) );

				OXML_Document * doc = OXML_Document::getInstance();
				UT_return_if_fail(_error_if_fail(doc != nullptr));
				const gchar * type = attrMatches(NS_W_KEY, "type", rqst->ppAtts);
				UT_return_if_fail( this->_error_if_fail(type != nullptr) );

				if (!strcmp(type, "default")) {
					last->setHeaderId(id, DEFAULT_HDRFTR);
					type = "header";
				} else if (!strcmp(type, "even")) {
					last->setHeaderId(id, EVENPAGE_HDRFTR);
					type = "header-even";
				} else {
					last->setHeaderId(id, FIRSTPAGE_HDRFTR);
					type = "header-first";
				}

				OXML_SharedSection hdr = doc->getHeader(id);
				UT_return_if_fail(_error_if_fail( UT_OK == hdr->setAttribute("type", type) ));
			}
			else if (nameMatches(rqst->pName, NS_W_KEY, "cols")) {
				const gchar * num = attrMatches(NS_W_KEY, "num", rqst->ppAtts);
				const gchar * sep = attrMatches(NS_W_KEY, "sep", rqst->ppAtts);
				const gchar * space = attrMatches(NS_W_KEY, "space", rqst->ppAtts);

				if(!num || atoi(num)<1)
					num = "1";

				if(!sep)
					sep = "off";
				
				OXML_SharedSection last = rqst->sect_stck->top();
				last->setProperty("columns", num);
				last->setProperty("column-line", sep);
				if(space && *space)
				{
					std::string gap(_TwipsToInches(space));
					gap += "in";
					last->setProperty("column-gap", gap.c_str());
				}
			}
		}

	} else if (nameMatches(rqst->pName, NS_W_KEY, "footnoteReference")) {
		const gchar * id = attrMatches(NS_W_KEY, "id", rqst->ppAtts);
		if(id)
		{
			OXML_SharedElement footnote(new OXML_Element_Field(id, fd_Field::FD_Footnote_Ref, ""));
			rqst->stck->push(footnote);
		}
		rqst->handled = true;

	} else if (nameMatches(rqst->pName, NS_W_KEY, "endnoteReference")) {
		const gchar * id = attrMatches(NS_W_KEY, "id", rqst->ppAtts);
		if(id)
		{
			OXML_SharedElement endnote(new OXML_Element_Field(id, fd_Field::FD_Endnote_Ref, ""));
			rqst->stck->push(endnote);
		}
		rqst->handled = true;

	} else if (nameMatches(rqst->pName, NS_W_KEY, "commentRangeStart")) {
		/* emits the PTO_Annotation start object plus the whole comment
		 * shadow (SectionAnnotation + blocks + EndAnnotation) so the
		 * anchored text that follows sits between the markers */
		const gchar * id = attrMatches(NS_W_KEY, "id", rqst->ppAtts);
		if(id)
		{
			OXML_SharedElement ann(new OXML_Element_Annotation(id, false));
			rqst->stck->push(ann);
		}
		rqst->handled = true;

	} else if (nameMatches(rqst->pName, NS_W_KEY, "commentRangeEnd")) {
		const gchar * id = attrMatches(NS_W_KEY, "id", rqst->ppAtts);
		OXML_SharedElement ann(
			new OXML_Element_Annotation(id ? id : "", true));
		rqst->stck->push(ann);
		rqst->handled = true;

	} else if (nameMatches(rqst->pName, NS_W_KEY, "commentReference") ||
			   nameMatches(rqst->pName, NS_W_KEY, "annotationRef")) {
		/* Word's in-run reference mark — the annotation layout draws
		 * its own anchor, nothing to emit */
		rqst->handled = true;

	} else if (nameMatches(rqst->pName, NS_W_KEY, "hyperlink")) {
		const gchar * id = attrMatches(NS_R_KEY, "id", rqst->ppAtts);
		const gchar * anchor = attrMatches(NS_W_KEY, "anchor", rqst->ppAtts);
		if(id)
		{
			OXMLi_PackageManager * mgr = OXMLi_PackageManager::getInstance();
			std::string target = mgr->getPartName(id);
			OXML_Element_Hyperlink* hyperlink = new OXML_Element_Hyperlink("");
			hyperlink->setHyperlinkTarget(target);				
			OXML_SharedElement elem(hyperlink);
			rqst->stck->push(elem);
		}
		else if(anchor)
		{
			std::string bookmarkAnchor("#");
			bookmarkAnchor += anchor;
			OXML_Element_Hyperlink* hyperlink = new OXML_Element_Hyperlink("");
			hyperlink->setHyperlinkTarget(bookmarkAnchor);				
			OXML_SharedElement elem(hyperlink);
			rqst->stck->push(elem);
		}
		rqst->handled = true;

	} else if (nameMatches(rqst->pName, NS_W_KEY, "bookmarkStart")) {
		const gchar * id = attrMatches(NS_W_KEY, "id", rqst->ppAtts);
		const gchar * name = attrMatches(NS_W_KEY, "name", rqst->ppAtts);
		if(id && name)
		{
			std::string bookmarkId(id);
			std::string bookmarkName(name);
			OXML_Element_Bookmark* bookmark = new OXML_Element_Bookmark(bookmarkId);
			bookmark->setType("start");		
			bookmark->setName(bookmarkName);		
			OXML_SharedElement elem(bookmark);
			rqst->stck->push(elem);
			OXML_Document* pDoc = OXML_Document::getInstance();
			if(!pDoc->setBookmarkName(bookmarkId, bookmarkName))
				return;
		}
		rqst->handled = true;

	} else if (nameMatches(rqst->pName, NS_W_KEY, "bookmarkEnd")) {
		const gchar * id = attrMatches(NS_W_KEY, "id", rqst->ppAtts);
		if(id)
		{
			std::string bookmarkId(id);
			OXML_Element_Bookmark* bookmark = new OXML_Element_Bookmark(bookmarkId);
			bookmark->setType("end");				
			OXML_Document* pDoc = OXML_Document::getInstance();
			bookmark->setName(pDoc->getBookmarkName(bookmarkId));
			OXML_SharedElement elem(bookmark);
			rqst->stck->push(elem);
		}
		rqst->handled = true;

/******* END OF SECTION FORMATTING ********/
		
	} else if (nameMatches(rqst->pName, NS_W_KEY, "lastRenderedPageBreak")) {
		//Word's recorded pagination point; honor it for layout parity
		OXML_SharedElement br ( new OXML_Element("", PG_BREAK, SPAN) );
		rqst->stck->push(br);
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_W_KEY, "br")) {
		const gchar * type = attrMatches(NS_W_KEY, "type", rqst->ppAtts);
// The optional attribute can be missing. In that case a default 
// value is implied.
//		UT_return_if_fail( this->_error_if_fail(type != nullptr) );

		OXML_ElementTag tag;
		if (type && !strcmp(type, "column")) {
			tag = CL_BREAK;
		} else if (type && !strcmp(type, "page")) {
			tag = PG_BREAK;
		} else { //textWrapping
			tag = LN_BREAK;
		}
		OXML_SharedElement br ( new OXML_Element("", tag, SPAN) );
		rqst->stck->push(br);

		rqst->handled = true;
	}
}

void OXMLi_ListenerState_Common::endElement (OXMLi_EndElementRequest * rqst)
{
	UT_return_if_fail( this->_error_if_fail(rqst != nullptr) );

	if (nameMatches(rqst->pName, NS_W_KEY, "sdt") ||
		nameMatches(rqst->pName, NS_W_KEY, "showingPlcHdr") ||
		nameMatches(rqst->pName, NS_W_KEY, "sdtContent")) {
		rqst->handled = true;
		return;
	}

	if (nameMatches(rqst->pName, NS_W_KEY, "p")) {
		//Paragraph is done, appending it.
		if (rqst->stck->size() == 1) { //Only the paragraph is on the stack, append to section
			OXML_SharedElement elem = rqst->stck->top();
			UT_return_if_fail( this->_error_if_fail(elem.get() != nullptr) );
			OXML_SharedSection sect = rqst->sect_stck->top();
			UT_return_if_fail( this->_error_if_fail(sect.get() != nullptr) );
			UT_return_if_fail( this->_error_if_fail(UT_OK == sect->appendElement(elem) ) );
			rqst->stck->pop();
		} else { //Append to next element on the stack
			UT_return_if_fail( this->_error_if_fail( UT_OK == _flushTopLevel(rqst->stck, rqst->sect_stck) ) );
		}

		//Perform the section break if any
		if (m_pendingSectBreak) {
			OXML_Document * doc = OXML_Document::getInstance();
			UT_return_if_fail(_error_if_fail(doc != nullptr));
			OXML_SharedSection sect(new OXML_Section());
			rqst->sect_stck->push(sect);
			m_pendingSectBreak = false;
		}

		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_W_KEY, "altChunk")) {
		//close the reference paragraph pushed at startElement
		if (rqst->stck->size() == 1) {
			OXML_SharedElement elem = rqst->stck->top();
			UT_return_if_fail( this->_error_if_fail(elem.get() != nullptr) );
			OXML_SharedSection sect = rqst->sect_stck->top();
			UT_return_if_fail( this->_error_if_fail(sect.get() != nullptr) );
			UT_return_if_fail( this->_error_if_fail(UT_OK == sect->appendElement(elem) ) );
			rqst->stck->pop();
		} else {
			UT_return_if_fail( this->_error_if_fail( UT_OK == _flushTopLevel(rqst->stck, rqst->sect_stck) ) );
		}
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_W_KEY, "r")) {
		//Run is done, appending it.
		UT_return_if_fail( this->_error_if_fail( UT_OK == _flushTopLevel(rqst->stck, rqst->sect_stck) ) );

		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_W_KEY, "t")) {
		if(m_fldChar && m_pageNumberField) // page number is already set with field element correctly.
		{
			rqst->handled = true;
			return;
		}
		//Text is done, appending it.
		UT_return_if_fail( this->_error_if_fail( UT_OK == _flushTopLevel(rqst->stck, rqst->sect_stck) ) );
		rqst->handled = true;
	} else if(nameMatches(rqst->pName, NS_W_KEY, "instrText")) {
		if(m_eqField || m_pageNumberField)
		{
			//instrText including equation or page number field is done, appending it.
			UT_return_if_fail( this->_error_if_fail( UT_OK == _flushTopLevel(rqst->stck, rqst->sect_stck) ) );
		}
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_W_KEY, "sectPr")) {
		std::string contextTag = rqst->context->back();
		if (contextMatches(contextTag, NS_W_KEY, "pPr") ||
			contextMatches(contextTag, NS_W_KEY, "body")) {
			OXML_SharedSection sect = rqst->sect_stck->top();
			UT_return_if_fail(_error_if_fail(sect.get() != nullptr));
			OXML_SharedElement dummy = rqst->stck->top();
			PP_PropertyVector atts = dummy->getAttributes();
			if (!atts.empty()) {
				UT_return_if_fail(_error_if_fail(UT_OK == sect->appendAttributes(atts)));
			}
			atts = dummy->getProperties();
			if (!atts.empty()) {
				UT_return_if_fail(_error_if_fail(UT_OK == sect->appendProperties(atts)));
			}
			rqst->stck->pop();

			rqst->handled = true;
		}
	} else if (	nameMatches(rqst->pName, NS_W_KEY, "jc") || 
				nameMatches(rqst->pName, NS_W_KEY, "ind") ||
				nameMatches(rqst->pName, NS_W_KEY, "spacing") ) {
		rqst->handled = true;
	} else if (	nameMatches(rqst->pName, NS_W_KEY, "b") || 
				nameMatches(rqst->pName, NS_W_KEY, "i") || 
				nameMatches(rqst->pName, NS_W_KEY, "u") ||
				nameMatches(rqst->pName, NS_W_KEY, "color") ||
				nameMatches(rqst->pName, NS_W_KEY, "vertAlign") ||
				nameMatches(rqst->pName, NS_W_KEY, "highlight") ||
				nameMatches(rqst->pName, NS_W_KEY, "strike") ||
				nameMatches(rqst->pName, NS_W_KEY, "dstrike") ||
				nameMatches(rqst->pName, NS_W_KEY, "rFonts") ||
				nameMatches(rqst->pName, NS_W_KEY, "lang") ||
				nameMatches(rqst->pName, NS_W_KEY, "noProof") ||
				nameMatches(rqst->pName, NS_W_KEY, "vanish") ||
				nameMatches(rqst->pName, NS_W_KEY, "specVanish") ||
				nameMatches(rqst->pName, NS_W_KEY, "webHidden") ||
				nameMatches(rqst->pName, NS_W_KEY, "caps") ||
				nameMatches(rqst->pName, NS_W_KEY, "smallCaps") ||
				nameMatches(rqst->pName, NS_W_KEY, "w") ||
				nameMatches(rqst->pName, NS_W_KEY, "rtl") ||
				nameMatches(rqst->pName, NS_W_KEY, "keepNext") ||
				nameMatches(rqst->pName, NS_W_KEY, "keepLines") ||
				nameMatches(rqst->pName, NS_W_KEY, "widowControl") ||
				nameMatches(rqst->pName, NS_W_KEY, "framePr") ||
				nameMatches(rqst->pName, NS_W_KEY, "bidi") ||
				nameMatches(rqst->pName, NS_W_KEY, "fldChar") ||
				nameMatches(rqst->pName, NS_W_KEY, "outlineLvl") ||
				nameMatches(rqst->pName, NS_W_KEY, "textAlignment") ||
				nameMatches(rqst->pName, NS_W_KEY, "snapToGrid") ||
				nameMatches(rqst->pName, NS_W_KEY, "kinsoku") ||
				nameMatches(rqst->pName, NS_W_KEY, "wordWrap") ||
				nameMatches(rqst->pName, NS_W_KEY, "suppressLineNumbers") ||
				nameMatches(rqst->pName, NS_W_KEY, "suppressAutoHyphens") ||
				nameMatches(rqst->pName, NS_W_KEY, "mirrorIndents") ||
				nameMatches(rqst->pName, NS_W_KEY, "adjustRightInd") ||
				nameMatches(rqst->pName, NS_W_KEY, "autoSpaceDE") ||
				nameMatches(rqst->pName, NS_W_KEY, "autoSpaceDN") ||
				nameMatches(rqst->pName, NS_W_KEY, "overflowPunct") ||
				nameMatches(rqst->pName, NS_W_KEY, "topLinePunct") ||
				nameMatches(rqst->pName, NS_W_KEY, "kern") ||
				nameMatches(rqst->pName, NS_W_KEY, "spacing") ||
				nameMatches(rqst->pName, NS_W_KEY, "em") ||
				nameMatches(rqst->pName, NS_W_KEY, "position") ||
				nameMatches(rqst->pName, NS_W_KEY, "sz") ) {
		rqst->handled = true;
	} else if (	nameMatches(rqst->pName, NS_W_KEY, "type") ||
				nameMatches(rqst->pName, NS_W_KEY, "footerReference") ||
				nameMatches(rqst->pName, NS_W_KEY, "headerReference") ||
				nameMatches(rqst->pName, NS_W_KEY, "titlePg") ||
				nameMatches(rqst->pName, NS_W_KEY, "pgNumType") ||
				nameMatches(rqst->pName, NS_W_KEY, "pgBorders") ||
				nameMatches(rqst->pName, NS_W_KEY, "vAlign") ||
				nameMatches(rqst->pName, NS_W_KEY, "textDirection") ||
				nameMatches(rqst->pName, NS_W_KEY, "paperSrc") ||
				nameMatches(rqst->pName, NS_W_KEY, "lnNumType") ||
				nameMatches(rqst->pName, NS_W_KEY, "docGrid") ||
				nameMatches(rqst->pName, NS_W_KEY, "rtlGutter") ||
				nameMatches(rqst->pName, NS_W_KEY, "formProt") ||
				nameMatches(rqst->pName, NS_W_KEY, "noEndnote") ||
				nameMatches(rqst->pName, NS_W_KEY, "cols")) {
		std::string contextTag = rqst->context->back();
		if (contextMatches(contextTag, NS_W_KEY, "sectPr")) {
			rqst->handled = true;
		}
	} else if (nameMatches(rqst->pName, NS_W_KEY, "tab") ||
			   nameMatches(rqst->pName, NS_W_KEY, "noBreakHyphen") ||
			   nameMatches(rqst->pName, NS_W_KEY, "softHyphen") ||
			   nameMatches(rqst->pName, NS_W_KEY, "lastRenderedPageBreak") ||
			   nameMatches(rqst->pName, NS_W_KEY, "sym")) {
		std::string contextTag = rqst->context->back();
		if (contextMatches(contextTag, NS_W_KEY, "r")) {
			UT_return_if_fail( this->_error_if_fail( UT_OK == _flushTopLevel(rqst->stck, rqst->sect_stck) ) );
			rqst->handled = true;
		}
		else if(contextMatches(contextTag, NS_W_KEY, "tabs") &&
				nameMatches(rqst->pName, NS_W_KEY, "tab"))
			rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_W_KEY, "br")) {
		UT_return_if_fail( this->_error_if_fail( UT_OK == _flushTopLevel(rqst->stck, rqst->sect_stck) ) );
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_W_KEY, "footnoteReference") ||
			   nameMatches(rqst->pName, NS_W_KEY, "endnoteReference") ||
			   nameMatches(rqst->pName, NS_W_KEY, "commentRangeStart") ||
			   nameMatches(rqst->pName, NS_W_KEY, "commentRangeEnd")) {
		UT_return_if_fail( this->_error_if_fail( UT_OK == _flushTopLevel(rqst->stck, rqst->sect_stck) ) );
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_W_KEY, "commentReference") ||
			   nameMatches(rqst->pName, NS_W_KEY, "annotationRef")) {
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_W_KEY, "hyperlink")) {
		UT_return_if_fail( this->_error_if_fail( UT_OK == _flushTopLevel(rqst->stck, rqst->sect_stck) ) );
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_W_KEY, "bookmarkStart") ||
				nameMatches(rqst->pName, NS_W_KEY, "bookmarkEnd")) {
		UT_return_if_fail( this->_error_if_fail( UT_OK == _flushTopLevel(rqst->stck, rqst->sect_stck) ) );
		rqst->handled = true;		
	} else if (nameMatches(rqst->pName, NS_W_KEY, "pageBreakBefore")) {
		UT_return_if_fail( this->_error_if_fail( UT_OK == _flushTopLevel(rqst->stck, rqst->sect_stck) ) );
		rqst->handled = true;
	} else if (nameMatches(rqst->pName, NS_W_KEY, "shd")) {
		std::string contextTag = rqst->context->back();
		rqst->handled = contextMatches(contextTag, NS_W_KEY, "pPr") || contextMatches(contextTag, NS_W_KEY, "rPr");
	}
}

void OXMLi_ListenerState_Common::charData (OXMLi_CharDataRequest * rqst)
{
	if(!rqst)
	{
		UT_DEBUGMSG(("FRT: OpenXML importer invalid NULL request in OXMLi_ListenerState_Common.charData\n"));
		return;
	}
	
	if(rqst->stck->empty())
		return;

	std::string contextTag = "";
	if(!rqst->context->empty())
	{
		contextTag = rqst->context->back();
	}
	int instrText = contextMatches(contextTag, NS_W_KEY, "instrText");
	if(instrText)
	{
		UT_ASSERT(rqst->buffer != nullptr);
		OXML_SharedElement run = rqst->stck->top();
		OXML_SharedElement sharedElem(new OXML_Element_Text("", 0));
		std::string overline = "\\to";
		std::string underline = "\\bo";
		std::string eq = "EQ";
		std::string pageNumber = "PAGE   \\* MERGEFORMAT";
		std::string v(rqst->buffer);
		std::string value = "";
		size_t isOverline = v.find(overline);
		size_t isUnderline = v.find(underline);
		size_t isEQ = v.find(eq);
		size_t isPageNumber = v.find(pageNumber);
		size_t openingParenthesis;
		size_t closingParenthesis;
		UT_Error err;
		if(isEQ != std::string::npos) // handle EQ fields
		{
			if(isOverline != std::string::npos && isUnderline == std::string::npos)
			{
				// if starts with "EQ \x \to", then overline property is used
				err = run->setProperty("text-decoration", "overline");
				if(err != UT_OK)
				{
					UT_DEBUGMSG(("SERHAT: Could not set overline property!\n"));
					return;
				}
			}
			else if(isUnderline != std::string::npos && isOverline == std::string::npos)
			{
				// if starts with "EQ \x \bo", then underline property is used
				err = run->setProperty("text-decoration", "underline");
				if(err != UT_OK)
				{
					UT_DEBUGMSG(("SERHAT: Could not set underline property!\n"));
					return;
				}
			}
			rqst->stck->push(sharedElem);
			m_eqField = true;
			m_pageNumberField = false;
			openingParenthesis = v.find("(");
			closingParenthesis = v.find(")");
			value = v.substr(int(openingParenthesis)+1, int(closingParenthesis)-int(openingParenthesis)-1); // the string between parentheses is extracted
			OXML_Element* elem = sharedElem.get();
			OXML_Element_Text* textElement = static_cast<OXML_Element_Text*>(elem);
			textElement->setText(value.c_str(), value.size());
		}
		else if(isPageNumber != std::string::npos) // handle page number fields
		{
			m_pageNumberField = true;
			m_eqField = false;
			OXML_SharedElement fieldElem(new OXML_Element_Field("", v, ""));
			rqst->stck->push(fieldElem);
		}
		else
		{
			m_eqField = false;
			m_pageNumberField = false;
		}
	}
	else
	{
		OXML_SharedElement sharedElem = rqst->stck->top();
		OXML_Element* elem = sharedElem.get();

		if(!elem || (elem->getTag() != T_TAG))
			return;

		OXML_Element_Text* textElement = static_cast<OXML_Element_Text*>(elem);
		textElement->setText(rqst->buffer, rqst->length);
	}
}
