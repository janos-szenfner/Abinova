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
#include "OXML_Element_Run.h"

// Abinova includes
#include "ut_types.h"
#include "ut_misc.h"
#include "pd_Document.h"
#include "pt_Types.h"
#include "pp_Revision.h"
#include "ie_exp_OpenXML.h"

// External includes
#include <string>
#include <cstdlib>

OXML_Element_Run::OXML_Element_Run(const std::string & id) : 
	OXML_Element(id, R_TAG, SPAN)
{
	//Intentionally empty
}

OXML_Element_Run::~OXML_Element_Run()
{

}

UT_Error OXML_Element_Run::serializeChildren(IE_Exp_OpenXML* exporter)
{
	UT_Error ret = UT_OK;

	OXML_ElementVector children = getChildren();
	OXML_ElementVector::size_type i;
	for (i = 0; i < children.size(); i++)
	{
		if(getType() == LIST)
			children[i]->setType(LIST);
		ret = children[i]->serialize(exporter);
		if(ret != UT_OK)
			return ret;
	}

	return ret;
}

/* Finds the element carrying the run-level "revision" attribute —
 * the run itself or a payload child (the export listener copies the
 * span AP's attributes onto the text/image element, not the run). */
const OXML_ObjectWithAttrProp * OXML_Element_Run::_revisionSource() const
{
	const gchar * szValue = nullptr;
	if(getAttribute(PT_REVISION_ATTRIBUTE_NAME, szValue) == UT_OK &&
	   szValue && *szValue)
		return this;

	const OXML_ElementVector & children = getChildren();
	for (auto & c : children)
	{
		if (c && c->getAttribute(PT_REVISION_ATTRIBUTE_NAME, szValue) == UT_OK &&
			szValue && *szValue)
			return c.get();
	}
	return nullptr;
}

UT_Error OXML_Element_Run::serialize(IE_Exp_OpenXML* exporter)
{
	UT_Error err = UT_OK;

	/* tracked changes (OXML08): a "+id"/"-id" token in the span's
	 * "revision" attribute becomes a w:ins / w:del wrapper around the
	 * run; w:moveFrom / w:moveTo are reconstructed when the imported
	 * "revision-move" name is present.  The last ins/del token wins —
	 * earlier tokens in a stacked list describe superseded states the
	 * final view does not show. */
	const OXML_ObjectWithAttrProp * revSrc = _revisionSource();
	const PP_Revision * pInsDel = nullptr;
	const gchar * revValue = nullptr;
	if (revSrc)
		revSrc->getAttribute(PT_REVISION_ATTRIBUTE_NAME, revValue);
	PP_RevisionAttr revAttr(revValue);
	if (revSrc)
	{
		for (UT_uint32 i = 0; i < revAttr.getRevisionsCount(); ++i)
		{
			const PP_Revision * r = revAttr.getNthRevision(i);
			if (!r)
				continue;
			PP_RevisionType t = r->getType();
			if (t == PP_REVISION_ADDITION || t == PP_REVISION_DELETION ||
				t == PP_REVISION_ADDITION_AND_FMT)
				pInsDel = r;
		}
	}

	const char * wrapTag = nullptr;
	const gchar * moveName = nullptr;
	if (pInsDel)
	{
		bool bDel = pInsDel->getType() == PP_REVISION_DELETION;
		const gchar * mv = nullptr;
		revSrc->getAttribute("revision-move", mv);
		if (mv && *mv)
		{
			moveName = mv;
			wrapTag = bDel ? "moveFrom" : "moveTo";
		}
		else
			wrapTag = bDel ? "del" : "ins";
	}

	if (wrapTag)
	{
		err = exporter->startRevision(TARGET, wrapTag, pInsDel->getId(),
									  moveName);
		if(err != UT_OK)
			return err;
	}

	err = exporter->startRun(TARGET);
	if(err != UT_OK)
		return err;

	err = serializeProperties(exporter);
	if(err != UT_OK)
		return err;

	err = this->serializeChildren(exporter);
	if(err != UT_OK)
		return err;

	err = exporter->finishRun(TARGET);
	if(err != UT_OK)
		return err;

	if (wrapTag)
		return exporter->finishRevision(TARGET, wrapTag);

	return UT_OK;
}

UT_Error OXML_Element_Run::serializeProperties(IE_Exp_OpenXML* exporter)
{
	//TODO: Add all the property serializations here
	UT_Error err = UT_OK;
	const gchar* szValue = nullptr;

	err = exporter->startRunProperties(TARGET);
	if(err != UT_OK)
		return err;

	if(getProperty("lang", szValue) == UT_OK)
	{
		if(!strcmp(szValue, "-none-"))
			err = exporter->setNoProof(TARGET);
		else
			err = exporter->setLanguage(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	if(getProperty("font-family", szValue) == UT_OK)
	{
		err = exporter->setFontFamily(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	if(getProperty("font-weight", szValue) == UT_OK)
	{
		if(!strcmp(szValue, "bold"))
		{
			err = exporter->setBold(TARGET);
			if(err != UT_OK)
				return err;
		}
	}

	if(getProperty("font-style", szValue) == UT_OK)
	{
		if(!strcmp(szValue, "italic"))
		{
			err = exporter->setItalic(TARGET);
			if(err != UT_OK)
				return err;
		}
	}

	if(getProperty("font-size", szValue) == UT_OK)
	{
		err = exporter->setFontSize(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}
	
	if(getProperty("text-decoration", szValue) == UT_OK)
	{
		if(strstr(szValue, "underline"))
		{
			err = exporter->setUnderline(TARGET);
			if(err != UT_OK)
				return err;
		}

		if(strstr(szValue, "overline"))
		{
			err = exporter->setOverline();
			if(err != UT_OK)
				return err;
		}

		if(strstr(szValue, "line-through"))
		{
			err = exporter->setLineThrough(TARGET);
			if(err != UT_OK)
				return err;
		}
	}

	if(getProperty("text-position", szValue) == UT_OK)
	{
		if(!strcmp(szValue, "superscript"))
		{
			err = exporter->setSuperscript(TARGET);
			if(err != UT_OK)
				return err;
		}

		else if(!strcmp(szValue, "subscript"))
		{
			err = exporter->setSubscript(TARGET);
			if(err != UT_OK)
				return err;
		}
	}

	if(getProperty("color", szValue) == UT_OK)
	{
		err = exporter->setTextColor(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	if(getProperty("bgcolor", szValue) == UT_OK)
	{
		err = exporter->setBackgroundColor(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	if(getProperty("dir-override", szValue) == UT_OK)
	{
		err = exporter->setTextDirection(TARGET, szValue);
		if(err != UT_OK)
			return err;
	}

	err = _serializeFormatChanges(exporter, _revisionSource());
	if(err != UT_OK)
		return err;

	return exporter->finishRunProperties(TARGET);
}

/* Emits one <w:rPrChange w:id w:author w:date><w:rPr>old-rPr</w:rPr>
 * </w:rPrChange> per recorded format change.  Two storage forms
 * carry the pre-change snapshot: "!id{props}{attrs}" marks inside
 * the span's "revision" attribute (AbiWord's own format-change
 * records, found on revSrc), and a dedicated inert "rPrChange"
 * attribute captured by the DOCX importer — both replay the stored
 * prop/attr set through the normal rPr mapping on a scratch run. */
UT_Error OXML_Element_Run::_serializeFormatChanges(IE_Exp_OpenXML* exporter,
												   const OXML_ObjectWithAttrProp * revSrc)
{
	UT_Error err = UT_OK;

	if (revSrc)
	{
		const gchar * v = nullptr;
		revSrc->getAttribute(PT_REVISION_ATTRIBUTE_NAME, v);
		PP_RevisionAttr ra(v);
		for (UT_uint32 i = 0; i < ra.getRevisionsCount(); ++i)
		{
			const PP_Revision * r = ra.getNthRevision(i);
			if (!r || r->getType() != PP_REVISION_FMT_CHANGE)
				continue;

			err = exporter->startRevision(TARGET, "rPrChange",
										  r->getId(), nullptr);
			if(err != UT_OK)
				return err;

			OXML_Element_Run oldRun("");
			oldRun.setProperties(r->getProperties());
			oldRun.setAttributes(r->getAttributes());
			err = oldRun.serializeProperties(exporter);
			if(err != UT_OK)
				return err;

			err = exporter->finishRevision(TARGET, "rPrChange");
			if(err != UT_OK)
				return err;
		}
	}

	const gchar * changeVal = nullptr;
	const OXML_ObjectWithAttrProp * changeSrc = nullptr;
	if (getChangeMark("rPrChange", changeVal) == UT_OK && changeVal && *changeVal)
		changeSrc = this;
	else
	{
		const OXML_ElementVector & children = getChildren();
		for (auto & c : children)
		{
			if (c && c->getChangeMark("rPrChange", changeVal) == UT_OK &&
				changeVal && *changeVal)
			{
				changeSrc = c.get();
				break;
			}
			changeVal = nullptr;
		}
	}

	if (changeSrc)
	{
		UT_uint32 revId = 0;
		PP_PropertyVector props, attrs;
		if (parseChangeMark(changeVal, revId, props, attrs))
		{
			err = exporter->startRevision(TARGET, "rPrChange",
										  revId, nullptr);
			if(err != UT_OK)
				return err;

			OXML_Element_Run oldRun("");
			oldRun.setProperties(props);
			oldRun.setAttributes(attrs);
			err = oldRun.serializeProperties(exporter);
			if(err != UT_OK)
				return err;

			err = exporter->finishRevision(TARGET, "rPrChange");
			if(err != UT_OK)
				return err;
		}
	}

	return UT_OK;
}

UT_Error OXML_Element_Run::addToPT(PD_Document * pDocument)
{
	UT_return_val_if_fail(pDocument != nullptr, UT_ERROR);

	UT_Error ret = UT_OK;

	/* a w:rPrChange snapshot recorded during parse lands on the
	 * run's fmt AP as an inert "rPrChange"="!id{...}" attribute */
	applyRevisionMarks(pDocument);

	PP_PropertyVector atts = getAttributesWithProps();
	if (!atts.empty()) {
		//We open the formatting tag
		ret = pDocument->appendFmt(atts) ? UT_OK : UT_ERROR;
		if(ret != UT_OK)
		{
			UT_ASSERT_HARMLESS(ret == UT_OK);
			return ret;
		}
	}

	ret = addChildrenToPT(pDocument);
	if(ret != UT_OK)
	{
		UT_ASSERT_HARMLESS(ret == UT_OK);
		return ret;
	}

	if (!atts.empty()) {
		//We close the formatting tag
		ret = pDocument->appendFmt(PP_NOPROPS) ? UT_OK : UT_ERROR;
		UT_return_val_if_fail(ret == UT_OK, ret);
	}
	return ret;
}

