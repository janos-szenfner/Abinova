/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */

/* AbiSource
 *
 * Copyright (C) 2026 Abinova contributors
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

#ifndef _OXML_ELEMENT_FIELDCHAR_H_
#define _OXML_ELEMENT_FIELDCHAR_H_

// Internal includes
#include "OXML_Element.h"
#include "ie_exp_OpenXML.h"

// Abinova includes
#include "ut_types.h"

/* \class OXML_Element_FieldChar
 * \brief Single run of a complex Word field: w:fldChar
 *        (begin/separate/end) or w:instrText.
 *
 * Complex fields (TOC, hyperlinks, ...) are written in ECMA-376 as
 * a begin run, an instruction run, an optional separate run, the
 * cached result, and an end run. The exporter splices these marker
 * elements into ordinary paragraphs so e.g. a piece-table TOC strux
 * round-trips as a live Word TOC field.
 */
class OXML_Element_FieldChar : public OXML_Element
{
public:
	//! fldChar marker: fldCharType is "begin", "separate" or "end"
	OXML_Element_FieldChar(const std::string & id, const char* fldCharType);
	//! instruction run carrying the field code
	OXML_Element_FieldChar(const std::string & id, const std::string & instr);
	virtual ~OXML_Element_FieldChar();

	virtual UT_Error serialize(IE_Exp_OpenXML* exporter) override;

private:
	std::string m_fldCharType;
	std::string m_instr;
};

#endif //_OXML_ELEMENT_FIELDCHAR_H_
