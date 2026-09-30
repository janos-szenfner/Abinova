/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */

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

#ifndef _OXML_ELEMENT_ANNOTATION_H_
#define _OXML_ELEMENT_ANNOTATION_H_

// Internal includes
#include "OXML_Element.h"
#include "ie_exp_OpenXML.h"

// Abinova includes
#include "ut_types.h"
#include "pd_Document.h"

/* \class OXML_Element_Annotation
 * \brief Piecetable emitter for w:commentRangeStart / w:commentRangeEnd.
 *
 * Word splits a comment across three markup points: commentRangeStart,
 * the anchored range, commentRangeEnd, plus the comment body living in
 * comments.xml.  The AbiWord model stores comments inline instead:
 *
 *   [PTO_Annotation start][SectionAnnotation][comment blocks]
 *   [EndAnnotation][anchored text][PTO_Annotation end]
 *
 * So the range start element emits the start object AND the whole
 * comment shadow (content is fetched from OXML_Document::getAnnotation),
 * and the range end element emits only the anonymous end object.
 */
class OXML_Element_Annotation : public OXML_Element
{
public:
	OXML_Element_Annotation(const std::string & id, bool bEnd);
	virtual ~OXML_Element_Annotation();

	virtual UT_Error addToPT(PD_Document * pDocument) override;

	inline bool isEnd() const { return m_bEnd; }

private:
	bool m_bEnd;
};

#endif //_OXML_ELEMENT_ANNOTATION_H_
