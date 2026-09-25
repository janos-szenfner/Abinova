/* Abinova
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

#include "ut_types.h"
#include "ut_string.h"
#include "ap_Dialog_Document.h"

AP_Dialog_Document::AP_Dialog_Document(XAP_DialogFactory * pDlgFactory,
									   XAP_Dialog_Id id)
	: XAP_Dialog_NonPersistent(pDlgFactory, id, "interface/dialogdocument"),
	m_pFrame(nullptr),
	m_answer(a_CANCEL),
	m_MarginUnits(DIM_IN),
	m_MarginTop(1.0f),
	m_MarginBottom(1.0f),
	m_MarginLeft(1.0f),
	m_MarginRight(1.0f),
	m_MarginGutter(0.0f),
	m_MarginHeader(0.0f),
	m_MarginFooter(0.0f),
	m_GutterPosition(GUTTER_LEFT),
	m_MultiplePages(MULTI_NORMAL),
	m_ApplyTo(APPLY_WHOLE_DOC),
	m_SectionStart(SECTION_NEW_PAGE),
	m_VerticalAlign(VALIGN_TOP),
	m_DifferentOddEven(false),
	m_DifferentFirstPage(false),
	m_PageSize(fp_PageSize::psA4),
	m_PageUnits(DIM_IN),
	m_PageScale(100),
	m_ActivePage(PAGE_MARGINS),
	m_PageSetupChanged(false)
{
}

AP_Dialog_Document::~AP_Dialog_Document(void)
{
}

bool AP_Dialog_Document::validatePageSettings(void) const
{
	if ((m_MarginLeft + m_MarginRight + m_MarginGutter >=
		 m_PageSize.Width(m_MarginUnits)) ||
		(m_MarginTop + m_MarginBottom >=
		 m_PageSize.Height(m_MarginUnits)))
		return false;
	return true;
}
