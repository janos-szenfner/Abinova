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
#include "OXML_Element_Paragraph.h"

// Internal includes
#include "OXMLi_Element_Revision.h"

// Abinova includes
#include "ut_types.h"
#include "ut_string.h"
#include "ut_std_string.h"
#include "pd_Document.h"

// External includes
#include <cstdlib>

OXML_Element_Paragraph::OXML_Element_Paragraph(const std::string & id) :
	OXML_Element(id, P_TAG, BLOCK), pageBreak(false),
	m_paraMarkDeleted(false), m_hasParaMarkChange(false),
	m_paraMarkAuthor(), m_paraMarkDate(), m_section(nullptr)
{
}

OXML_Element_Paragraph::~OXML_Element_Paragraph()
{

}

UT_Error OXML_Element_Paragraph::serialize(IE_Exp_OpenXML* exporter)
{
	UT_Error err = UT_OK;

	err = exporter->startParagraph(TARGET);
	if(err != UT_OK)
		return err;

	err = this->serializeProperties(exporter); // Paragraph properties
	if(err != UT_OK)
		return err;

	err = this->serializeChildren(exporter);
	if(err != UT_OK)
		return err;

	return exporter->finishParagraph(TARGET);
}

UT_Error OXML_Element_Paragraph::serializeChildren(IE_Exp_OpenXML* exporter)
{
	UT_Error ret = UT_OK;

	bool includesList = false;

	OXML_ElementVector::size_type i;
	OXML_ElementVector children = getChildren();
	for (i = 0; i < children.size(); i++)
	{
		// LIST children are handled in serializeProperties function
		if(children[i]->getType() != LIST)
		{	
			if(includesList)
				children[i]->setType(LIST);
			ret = children[i]->serialize(exporter);
			if(ret != UT_OK)
				return ret;
		}
		else 
			includesList = true;
	}

	return ret;
}

UT_Error OXML_Element_Paragraph::serializeProperties(IE_Exp_OpenXML* exporter)
{
	//TODO: Add all the property serializations here
	UT_Error err = UT_OK;
	const gchar* szValue = nullptr;

	err = exporter->startParagraphProperties(TARGET);
	if(err != UT_OK)
		return err;

	if(pageBreak)
	{
		err = exporter->setPageBreak(TARGET);
		if(err != UT_OK)
			return err;	
	}

	if(getAttribute(PT_STYLE_ATTRIBUTE_NAME, szValue) == UT_OK)
	{
		err = exporter->setParagraphStyle(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	/* paragraph borders - <w:pBdr> comes after pStyle/numPr and
	 * before shd/spacing/ind/jc in the OOXML schema order */
	err = serializeParagraphBorders(exporter, TARGET);
	if (err != UT_OK)
		return err;

	if(getProperty("widows", szValue) == UT_OK)
	{
		err = exporter->setWidows(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	if(getProperty("text-align", szValue) == UT_OK)
	{
		if(!strcmp(szValue, "justify"))
		{
			err = exporter->setTextAlignment(TARGET, "both");
		}
		else if(!strcmp(szValue, "center"))
		{
			err = exporter->setTextAlignment(TARGET, "center");
		}
		else if(!strcmp(szValue, "right"))
		{
			err = exporter->setTextAlignment(TARGET, "right");
		}
		else if(!strcmp(szValue, "left"))
		{
			err = exporter->setTextAlignment(TARGET, "left");
		}

		if(err != UT_OK)
			return err;
	}

	if(getProperty("text-indent", szValue) == UT_OK)
	{
		err = exporter->setTextIndentation(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	if(getProperty("margin-left", szValue) == UT_OK)
	{
		err = exporter->setParagraphLeftMargin(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	if(getProperty("margin-right", szValue) == UT_OK)
	{
		err = exporter->setParagraphRightMargin(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	if(getProperty("margin-bottom", szValue) == UT_OK)
	{
		err = exporter->setParagraphBottomMargin(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	if(getProperty("margin-top", szValue) == UT_OK)
	{
		err = exporter->setParagraphTopMargin(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	if(getProperty("line-height", szValue) == UT_OK)
	{
		err = exporter->setLineHeight(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	if(getProperty("tabstops", szValue) == UT_OK)
	{
		err = exporter->setTabstops(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	if(getProperty("bgcolor", szValue) == UT_OK)
	{
		err = exporter->setBackgroundColor(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	//serialize List here if any list appended to the paragraph since we need properties of
	//list to be included in paragraph properties section
	//also inherit the properties

	OXML_ElementVector::size_type i;
	OXML_ElementVector children = getChildren();
	for (i = 0; i < children.size(); i++)
	{
		children[i]->inheritProperties(this);
		if(children[i]->getType() == LIST)
		{
			err = children[i]->serialize(exporter);
			if(err != UT_OK)
				return err;
		}
	}

	/* tracked paragraph-mark change (imported w:pPr/w:rPr/w:ins|w:del):
	 * the piece table stores the registered id as the inert
	 * "para-mark-rev" property; in DOCX the mark rides inside the
	 * pPr-level w:rPr, which CT_PPrBase places before sectPr */
	const gchar* pmRev = nullptr;
	if(getProperty("para-mark-rev", pmRev) == UT_OK && pmRev &&
	   (pmRev[0] == '+' || pmRev[0] == '-'))
	{
		err = exporter->startRunProperties(TARGET);
		if(err != UT_OK)
			return err;
		err = exporter->setRevisionMark(TARGET,
										pmRev[0] == '-' ? "del" : "ins",
										strtoul(pmRev + 1, nullptr, 10));
		if(err != UT_OK)
			return err;
		err = exporter->finishRunProperties(TARGET);
		if(err != UT_OK)
			return err;
	}

	if(m_section)
	{
		err = m_section->serializeProperties(exporter, this); // Section properties
		if(err != UT_OK)
			return err;
	}

	/* a captured w:pPrChange snapshot lands on the block strux AP as
	 * an inert "pPrChange"="!id{props}{attrs}" attribute — replay it
	 * through a scratch paragraph's pPr emit (CT_PPrBase puts
	 * pPrChange last, after sectPr) */
	const gchar* ppc = nullptr;
	UT_uint32 chId = 0;
	PP_PropertyVector chProps, chAttrs;
	if(getChangeMark("pPrChange", ppc) == UT_OK &&
	   parseChangeMark(ppc, chId, chProps, chAttrs))
	{
		err = exporter->startRevision(TARGET, "pPrChange", chId, nullptr);
		if(err != UT_OK)
			return err;

		OXML_Element_Paragraph oldPara("");
		oldPara.setProperties(chProps);
		oldPara.setAttributes(chAttrs);
		err = oldPara.serializeProperties(exporter);
		if(err != UT_OK)
			return err;

		err = exporter->finishRevision(TARGET, "pPrChange");
		if(err != UT_OK)
			return err;
	}

	return exporter->finishParagraphProperties(TARGET);
}


void OXML_Element_Paragraph::setParaMarkChange(bool deleted,
											   const gchar * author,
											   const gchar * date)
{
	m_hasParaMarkChange = true;
	m_paraMarkDeleted = deleted;
	m_paraMarkAuthor = author ? author : "";
	m_paraMarkDate = date ? date : "";
}

/* w:pPr/w:rPr/w:ins|w:del marks a tracked insertion/deletion of this
 * paragraph's mark.  There is no strux-break revision mark in the
 * piece table, so the change degrades to a recorded-but-live break:
 * register it on the document revision table (author/date preserved)
 * and store the "+id"/"-id" token as the inert "para-mark-rev" block
 * property, which round-trips through .abwn for a future exporter. */
void OXML_Element_Paragraph::_applyParaMarkChange(PD_Document * pDocument)
{
	if (!m_hasParaMarkChange)
		return;
	m_hasParaMarkChange = false;

	UT_uint32 id = OXMLi_Element_Revision::registerRevision(
		pDocument, m_paraMarkAuthor, m_paraMarkDate);

	std::string token = m_paraMarkDeleted ? "-" : "+";
	token += UT_std_string_sprintf("%d", id);
	setProperty("para-mark-rev", token.c_str());
}

UT_Error OXML_Element_Paragraph::addToPT(PD_Document * pDocument)
{
	UT_Error ret = UT_OK;

	if (pDocument == nullptr)
		return UT_ERROR;

	_applyParaMarkChange(pDocument);

	/* strux revision marks recorded during parse (w:pPrChange /
	 * w:numberingChange snapshots) register themselves and land on
	 * the block strux AP */
	applyRevisionMarks(pDocument);

	//update list id and parent id here
	const gchar* pListId = getListId();
	const gchar* pListLevel = getListLevel();
	if(pListId && pListLevel)
	{
		std::string listid(pListId);
		std::string level(pListLevel);
		std::string parentid(pListId);
		parentid += "0";
		listid += level;
		if(!level.compare("0"))
		{	
			parentid = "0";		
		}
		
		ret = setAttribute("level", pListLevel);
		if(ret != UT_OK)
			return ret;
	
		ret = setAttribute("listid", listid.c_str());
		if(ret != UT_OK)
			return ret;

		ret = setAttribute("parentid", parentid.c_str());
		if(ret != UT_OK)
			return ret; 	

		//now copy over properties from the corresponding OXML_List object
		OXML_Document* doc = OXML_Document::getInstance();
		if(doc)
		{
			OXML_SharedList sList = doc->getListById(atoi(listid.c_str()));
			if(sList)
			{
				ret = setProperties(sList->getProperties());
				if(ret != UT_OK)
					return ret; 	
			}
		}

	}

	if(pageBreak)
	{
		UT_UCS4Char ucs = UCS_FF;
		ret = pDocument->appendSpan(&ucs, 1) ? UT_OK : UT_ERROR;
		if(ret != UT_OK) 
		{
			return ret;
		}

	}

	const PP_PropertyVector atts = getAttributesWithProps();

	/* w:framePr - the paragraph is a positioned frame (legacy pre-VML
	 * textbox form). Wrap the block in a frame strux anchored to the
	 * page/column per hAnchor/vAnchor. */
	const gchar* fpX = nullptr;
	const gchar* fpY = nullptr;
	const gchar* fpW = nullptr;
	getProperty("framePr-x", fpX);
	getProperty("framePr-y", fpY);
	getProperty("framePr-w", fpW);
	if (fpX || fpY || fpW) {
		PP_PropertyVector frameProps;
		const gchar* anchor = nullptr;
		getProperty("framePr-vAnchor", anchor);
		bool bPage = anchor && !strcmp(anchor, "page");
		if (!bPage) {
			getProperty("framePr-hAnchor", anchor);
			bPage = anchor && !strcmp(anchor, "page");
		}
		frameProps.push_back("position-to");
		frameProps.push_back(bPage ? "page-above-text" : "column-above-text");

		const gchar* wrap = nullptr;
		getProperty("framePr-wrap", wrap);
		const char* wrapMode = "wrapped-both"; //around/auto/through/tight
		if (wrap && (!strcmp(wrap, "none") || !strcmp(wrap, "notBeside")))
			wrapMode = "wrapped-topbot";
		frameProps.push_back("wrap-mode");
		frameProps.push_back(wrapMode);

		frameProps.push_back("frame-type");
		frameProps.push_back("textbox");
		if (fpX) {
			frameProps.push_back("xpos");
			frameProps.push_back(fpX);
		}
		if (fpY) {
			frameProps.push_back("ypos");
			frameProps.push_back(fpY);
		}
		if (fpW) {
			frameProps.push_back("frame-width");
			frameProps.push_back(fpW);
		}
		const gchar* fpH = nullptr;
		getProperty("framePr-h", fpH);
		if (fpH) {
			frameProps.push_back("frame-height");
			frameProps.push_back(fpH);
		}
		if (!pDocument->appendStrux(PTX_SectionFrame, frameProps))
			return UT_ERROR;
		if (!pDocument->appendStrux(PTX_Block, atts.empty() ? PP_NOPROPS : atts))
			return UT_ERROR;
		ret = addChildrenToPT(pDocument);
		if (ret != UT_OK)
			return ret;
		if (!pDocument->appendStrux(PTX_EndFrame, PP_NOPROPS))
			return UT_ERROR;
		return UT_OK;
	}

	if (!atts.empty()) {
		ret = pDocument->appendStrux(PTX_Block, atts) ? UT_OK : UT_ERROR;
		if(ret != UT_OK) {
			UT_ASSERT_HARMLESS(ret == UT_OK);
			return ret;
		}
	} else {
		ret = pDocument->appendStrux(PTX_Block, PP_NOPROPS) ? UT_OK : UT_ERROR;
	}


	if(pListId && pListLevel)
	{
		ret = setAttribute("type", "list_label");
		if(ret != UT_OK)
			return ret;

		const PP_PropertyVector ppAttr = getAttributesWithProps();

		if(!pDocument->appendObject(PTO_Field, ppAttr))
			return ret;

		pDocument->appendFmt(ppAttr);

		UT_UCS4String string = "\t";
		pDocument->appendSpan(string.ucs4_str(), string.size());
	}

	return addChildrenToPT(pDocument);
}

const gchar* OXML_Element_Paragraph::getListLevel()
{
	UT_Error err = UT_OK;
	const gchar* szValue;

	err = getAttribute("level", szValue);
	if(err != UT_OK)
	{
		return nullptr;
	}
	return szValue;
}

const gchar* OXML_Element_Paragraph::getListId()
{
	UT_Error err = UT_OK;
	const gchar* szValue;

	err = getAttribute("listid", szValue);
	if(err != UT_OK)
	{
		return nullptr;
	}
	return szValue;
}

void OXML_Element_Paragraph::setPageBreak()
{
	pageBreak = true;
}

bool OXML_Element_Paragraph::isNumberedList()
{
	UT_Error err = UT_OK;
	const gchar* szValue;

	err = getProperty("list-style", szValue);
	if(err != UT_OK)
	{
		return false;
	}
	if(!strcmp(szValue, "Numbered List"))
	{
		return true;
	}
	return false;
}

void OXML_Element_Paragraph::setSection(OXML_Section* section)
{
	m_section = section;
}

