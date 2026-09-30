/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource
 *
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
#include "OXML_Element_Annotation.h"

// Internal includes
#include "OXML_Document.h"
#include "OXML_Section.h"

// Abinova includes
#include "ut_types.h"
#include "pd_Document.h"

OXML_Element_Annotation::OXML_Element_Annotation(const std::string & id, bool bEnd) :
	OXML_Element(id, ANNOT_TAG, ANNOT),
	m_bEnd(bEnd)
{
}

OXML_Element_Annotation::~OXML_Element_Annotation()
{
}

UT_Error OXML_Element_Annotation::addToPT(PD_Document * pDocument)
{
	UT_return_val_if_fail(pDocument != nullptr, UT_ERROR);

	if (m_bEnd)
	{
		/* anonymous end marker: closes the innermost open comment
		 * range, matching the end-of-<ann> convention in .abwn */
		return pDocument->appendObject(PTO_Annotation, PP_NOPROPS)
			? UT_OK : UT_ERROR;
	}

	/* w:commentRangeStart: emit the anchor object followed
	 * immediately by the comment shadow section */
	const PP_PropertyVector attr = {
		"annotation-id", getId()
	};
	if (!pDocument->appendObject(PTO_Annotation, attr))
		return UT_ERROR;

	OXML_Document * doc = OXML_Document::getInstance();
	if (doc)
	{
		OXML_SharedSection sect = doc->getAnnotation(getId());
		if (sect)
			return sect->addToPTAsAnnotation(pDocument);
	}
	return UT_OK;
}
