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
			contextTag = rqst->context->back();
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
			contextTag = rqst->context->back();
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
				OXML_SharedElement imgElem = rqst->stck->top();
				if (imgElem)
					imgElem->setProperty("wrap-mode", "below-text");
			}
			/* relativeHeight is the OOXML z-order — higher draws in
			 * front. Feed our frame-stack-order property so the page's
			 * frame layer orders like Word */
			const gchar * rh = attrMatches(NS_WP_KEY, "relativeHeight", rqst->ppAtts);
			if (rh && rqst->stck->top())
				rqst->stck->top()->setProperty("frame-stack-order", rh);
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
			contextTag = rqst->context->back();
		}
		if(contextMatches(contextTag, NS_WP_KEY, "anchor"))
		{
			if(m_bSimplePos && !rqst->stck->empty() && rqst->stck->top())
			{
				const gchar * x = attrMatches(NS_WP_KEY, "x", rqst->ppAtts);
				const gchar * y = attrMatches(NS_WP_KEY, "y", rqst->ppAtts);
				if(x && *x)
				{
					std::string position(_EmusToInches(x));
					position += "in";
					rqst->stck->top()->setProperty("xpos", position);
				}
				if(y && *y)
				{
					std::string position(_EmusToInches(y));
					position += "in";
					rqst->stck->top()->setProperty("ypos", position);
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
			contextTag = rqst->context->back();
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
			contextTag = rqst->context->back();
		}
		int anchor = contextMatches(contextTag, NS_WP_KEY, "anchor");
		if(anchor)
		{
			rqst->handled = true;
		}
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
			contextTag = rqst->context->back();
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

		OXML_SharedElement imgElem = rqst->stck->top();
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

		OXML_SharedElement imgElem = rqst->stck->top();
		if(!imgElem)
			return;

		const gchar * wrapText = attrMatches(NS_WP_KEY, "wrapText", rqst->ppAtts);
		if(wrapText)
		{
			if(!strcmp(wrapText, "bothSides"))
			{
				if(imgElem->setProperty("wrap-mode", "wrapped-both") != UT_OK)
				{
					UT_DEBUGMSG(("SERHAT:OpenXML importer image wrap-mode property can't be set\n"));
				}
			}
			else if(!strcmp(wrapText, "right"))
			{
				if(imgElem->setProperty("wrap-mode", "wrapped-to-right") != UT_OK)
				{
					UT_DEBUGMSG(("SERHAT:OpenXML importer image wrap-mode property can't be set\n"));
				}
			}
			else if(!strcmp(wrapText, "left"))
			{
				if(imgElem->setProperty("wrap-mode", "wrapped-to-left") != UT_OK)
				{
					UT_DEBUGMSG(("SERHAT:OpenXML importer image wrap-mode property can't be set\n"));
				}
			}
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

		OXML_SharedElement imgElem = rqst->stck->top();
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
		if(imgElem->setProperty("wrap-mode", mode.c_str()) != UT_OK)
		{
			UT_DEBUGMSG(("OpenXML importer image wrap-mode property can't be set\n"));
		}
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

		OXML_SharedElement imgElem = rqst->stck->top();
		if(!imgElem)
			return;

		//both modes wrap only above/below; closest is wrapped-topbot
		if(imgElem->setProperty("wrap-mode", "wrapped-topbot") != UT_OK)
		{
			UT_DEBUGMSG(("OpenXML importer image wrap-mode property can't be set\n"));
		}
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

		OXML_SharedElement imgElem = rqst->stck->top();
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
			nameMatches(rqst->pName, NS_WP_KEY, "extent") ||
			nameMatches(rqst->pName, NS_WP_KEY, "wrapSquare") ||
			nameMatches(rqst->pName, NS_WP_KEY, "posOffset") ||
			nameMatches(rqst->pName, NS_WP_KEY, "positionH") ||
			nameMatches(rqst->pName, NS_WP_KEY, "positionV") ||
			nameMatches(rqst->pName, NS_WP_KEY, "simplePos"))
	{
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
		OXML_SharedElement pic = rqst->stck->top();
		rqst->stck->pop();
		if (rqst->stck->empty())
		{
			if (!rqst->sect_stck || rqst->sect_stck->empty())
			{
				rqst->handled = false;
				return;
			}
			rqst->handled =
				(rqst->sect_stck->top()->appendElement(pic) == UT_OK);
			return;
		}
		OXML_SharedElement parent = rqst->stck->top();
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
		contextTag = rqst->context->back();
	}
	int posOffset = contextMatches(contextTag, NS_WP_KEY, "posOffset");
	if(posOffset && !m_isInlineImage)
	{
		OXML_SharedElement imgElem = rqst->stck->top();
		rqst->stck->pop();

		if(rqst->context->size() >= 2)
			contextTag = rqst->context->at(rqst->context->size() - 2);
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
			parentTag = rqst->context->at(rqst->context->size() - 2);
		std::string align(rqst->buffer);
		if (contextMatches(parentTag, NS_WP_KEY, "positionH"))
			rqst->stck->top()->setProperty("halign", align);
		else if (contextMatches(parentTag, NS_WP_KEY, "positionV"))
			rqst->stck->top()->setProperty("valign", align);
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
		rqst->stck->top()->setProperty(
			contextTag == "wp14:pctPosHOffset" ? "pct-pos-x" : "pct-pos-y",
			UT_convertToDimensionlessString(pct));
		double base = (contextTag == "wp14:pctPosHOffset") ? 8.5 : 11.0;
		char buf[32];
		g_snprintf(buf, sizeof(buf), "%.4fin", pct * base);
		rqst->stck->top()->setProperty(
			contextTag == "wp14:pctPosHOffset" ? "xpos" : "ypos", buf);
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
		rqst->stck->top()->setProperty(
			contextTag == "wp14:pctWidth" ? "pct-width" : "pct-height",
			UT_convertToDimensionlessString(pct));
		double base = (contextTag == "wp14:pctWidth") ? 8.5 : 11.0;
		char buf[32];
		g_snprintf(buf, sizeof(buf), "%.4fin", pct * base);
		rqst->stck->top()->setProperty(
			contextTag == "wp14:pctWidth" ? "frame-width" : "frame-height",
			buf);
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
