/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource
 * 
 * Copyright (C) 2008 Firat Kiyak <firatkiyak@gmail.com>
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
#include "OXML_Element_TextBox.h"

// Internal includes
#include "OXML_Document.h"

// Abinova includes
#include "ut_types.h"
#include "ut_misc.h"
#include "ut_std_string.h"
#include "pd_Document.h"

// External includes
#include <cstring>
#include <string>

OXML_Element_TextBox::OXML_Element_TextBox(const std::string & id) : 
	OXML_Element(id, TXTBX_TAG, TEXTBOX)
{
	//Intentionally empty
}

OXML_Element_TextBox::~OXML_Element_TextBox()
{

}

UT_Error OXML_Element_TextBox::serialize(IE_Exp_OpenXML* exporter)
{
	UT_Error err = UT_OK;

	std::string tbId = "textboxId";
	tbId += getId();

	err = exporter->startTextBox(TARGET, tbId.c_str());
	if(err != UT_OK)
		return err;

	err = this->serializeProperties(exporter);
	if(err != UT_OK)
		return err;

	err = exporter->startTextBoxContent(TARGET);
	if(err != UT_OK)
		return err;

	err = this->serializeChildren(exporter);
	if(err != UT_OK)
		return err;

	err = exporter->finishTextBoxContent(TARGET);
	if(err != UT_OK)
		return err;

	/* signature-line frames carry their setup data in signature-*
	 * properties; map them to an o:signatureline child on the
	 * v:shape so Word keeps the object as a signature line. */
	const gchar* szSig = nullptr;
	if(getProperty("signature-line", szSig) == UT_OK
	   && szSig && !strcmp(szSig, "1"))
	{
		std::string s("<o:signatureline xmlns:o=\"urn:schemas-microsoft-com:office:office\" o:issignatureline=\"t\"");
		const gchar* v = nullptr;
		if(getProperty("signature-id", v) == UT_OK && v && *v)
		{
			std::string id(v);
			for(auto & c : id)
				c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
			s += " v:id=\"{" + id + "}\" o:id=\"{" + id + "}\"";
		}
		auto emit = [&](const char* prop, const char* attr) {
			const gchar* pv = nullptr;
			if(getProperty(prop, pv) == UT_OK && pv && *pv)
			{
				s += " ";
				s += attr;
				s += "=\"";
				s += UT_escapeXML(pv);
				s += "\"";
			}
		};
		emit("signature-name", "o:suggestedsigner");
		emit("signature-title", "o:suggestedsigner2");
		emit("signature-email", "o:suggestedsigneremail");
		const gchar* pInstr = nullptr;
		if(getProperty("signature-instructions", pInstr) == UT_OK
		   && pInstr && *pInstr)
		{
			s += " o:signinginstructions=\"" + UT_escapeXML(pInstr)
				 + "\" o:signinginstructionsset=\"t\"";
		}
		const gchar* pFlag = nullptr;
		if(getProperty("signature-allow-comments", pFlag) == UT_OK && pFlag)
		{
			s += " o:allowcomments=\"";
			s += strcmp(pFlag, "0") ? "t" : "f";
			s += "\"";
		}
		pFlag = nullptr;
		if(getProperty("signature-show-date", pFlag) == UT_OK && pFlag)
		{
			s += " o:showsigndate=\"";
			s += strcmp(pFlag, "0") ? "t" : "f";
			s += "\"";
		}
		s += "/>";
		err = exporter->setTextBoxSignatureLine(TARGET, s);
		if(err != UT_OK)
			return err;
	}

	return exporter->finishTextBox(TARGET);
}

UT_Error OXML_Element_TextBox::serializeProperties(IE_Exp_OpenXML* exporter)
{
	//TODO: Add all the property serializations here
	UT_Error err = UT_OK;
	const gchar* szValue = nullptr;

	err = exporter->startTextBoxProperties(TARGET);
	if(err != UT_OK)
		return err;

	if(getProperty("frame-width", szValue) == UT_OK)
	{
		err = exporter->setTextBoxWidth(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	if(getProperty("frame-height", szValue) == UT_OK)
	{
		err = exporter->setTextBoxHeight(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	return exporter->finishTextBoxProperties(TARGET);
}

UT_Error OXML_Element_TextBox::addToPT(PD_Document* pDocument)
{
	UT_Error ret = UT_OK;

	/* PTX_SectionFrame doesn't layout inside PTX_SectionHdrFtr; in
	 * header/footer parts emit the textbox's children inline so the
	 * content is still visible rather than silently dropped */
	if (m_flatten)
		return addChildrenToPT(pDocument);

	/* invisible empty shapes (no content, no fill) produce nothing —
	 * skipping them avoids polluting the piece table */
	const gchar * szFillChk = nullptr;
	if (getChildren().empty() &&
		(getProperty("background-color", szFillChk) != UT_OK || !szFillChk))
		return UT_OK;

	/* a:blipFill inside a shape gives the element an image id — emit
	 * an image frame so the picture fill renders */
	OXML_Document * model = OXML_Document::getInstance();
	if (!getId().empty() && model && model->getImageById(getId()))
	{
		ret = setProperty("frame-type", "image");
		if (ret == UT_OK)
			ret = setAttribute("strux-image-dataid", getId().c_str());
	}
	else
		ret = setProperty("frame-type", "textbox");
	if(ret != UT_OK)
		return ret;

	/* wp14 percent metrics and group child-space offsets resolve to
	 * page coordinates now that the page size is known, then
	 * wp:align keywords fill any remaining position */
	resolveAnchorMetrics();
	resolveAnchorAlignment();

	/* prstGeom="line" shapes are rendered as filled bars — the
	 * short side carries the outline thickness */
	const gchar * szBar = nullptr;
	if (getProperty("bar-w", szBar) == UT_OK && szBar)
		setProperty("frame-width", szBar);
	if (getProperty("bar-h", szBar) == UT_OK && szBar)
		setProperty("frame-height", szBar);

	/* halign/valign resolve to xpos/ypos which aren't frame props —
	 * translate to page offsets */
	const gchar * szX = nullptr;
	const gchar * szY = nullptr;
	if (getProperty("xpos", szX) == UT_OK && szX)
		setProperty("frame-page-xpos", szX);
	if (getProperty("ypos", szY) == UT_OK && szY)
		setProperty("frame-page-ypos", szY);

	/* DrawingML anchors are positioned relative to the page; when the
	 * importer carried frame-page-xpos/ypos over from wp:anchor, use
	 * page anchoring instead of the column default */
	const gchar * szPagePos = nullptr;
	if (getProperty("frame-page-xpos", szPagePos) == UT_OK && szPagePos)
		ret = setProperty("position-to", "page-above-text");
	else
		ret = setProperty("position-to", "column-above-text");
	if(ret != UT_OK)
		return ret;

	const gchar * szWrap = nullptr;
	if (getProperty("wrap-mode", szWrap) != UT_OK || !szWrap)
		ret = setProperty("wrap-mode", "wrapped-both");
	if(ret != UT_OK)
		return ret;

	/* Word textboxes default to no outline and no fill; our frames
	 * default to a solid 1-unit border, so suppress it explicitly */
	const gchar * szHas = nullptr;
	if (getProperty("top-style", szHas) != UT_OK || !szHas)
	{
		setProperty("top-style", "none");
		setProperty("bot-style", "none");
		setProperty("left-style", "none");
		setProperty("right-style", "none");
	}
	if ((getProperty("background-color", szHas) != UT_OK || !szHas) &&
		(getProperty("bgcolor", szHas) != UT_OK || !szHas))
		setProperty("bg-style", "0");

	const PP_PropertyVector attr = this->getAttributesWithProps();
	ret = pDocument->appendStrux(PTX_SectionFrame, attr) ? UT_OK : UT_ERROR;
	if(ret != UT_OK)
		return ret;

	if (getChildren().empty())
	{
		/* a frame with no content block breaks the surrounding frame
		 * chain — filled panels still get a (minimal) block so the
		 * colored background renders */
		ret = pDocument->appendStrux(PTX_Block, PP_NOPROPS) ? UT_OK : UT_ERROR;
		if(ret != UT_OK)
			return ret;
	}
	else
	{
		ret = this->addChildrenToPT(pDocument);
		if(ret != UT_OK)
			return ret;
	}

	ret = pDocument->appendStrux(PTX_EndFrame, PP_NOPROPS) ? UT_OK : UT_ERROR;
	return ret;
}

