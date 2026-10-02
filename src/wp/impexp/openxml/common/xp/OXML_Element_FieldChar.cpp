/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

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

// Class definition include
#include "OXML_Element_FieldChar.h"

// Abinova includes
#include "ut_types.h"

OXML_Element_FieldChar::OXML_Element_FieldChar(const std::string & id, const char* fldCharType) :
	OXML_Element(id, FLD_TAG, FIELD),
	m_fldCharType(fldCharType ? fldCharType : "")
{
}

OXML_Element_FieldChar::OXML_Element_FieldChar(const std::string & id, const std::string & instr) :
	OXML_Element(id, FLD_TAG, FIELD),
	m_instr(instr)
{
}

OXML_Element_FieldChar::~OXML_Element_FieldChar()
{
}

UT_Error OXML_Element_FieldChar::serialize(IE_Exp_OpenXML* exporter)
{
	UT_return_val_if_fail(exporter != nullptr, UT_ERROR);

	if(!m_fldCharType.empty())
		return exporter->setFieldChar(TARGET, m_fldCharType.c_str());

	return exporter->setFieldInstr(TARGET, m_instr);
}
