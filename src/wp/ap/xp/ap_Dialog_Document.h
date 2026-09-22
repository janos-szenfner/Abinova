/* AbiWord
 * Copyright (C) 2026 AbiSource, Inc.
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

#pragma once

#include "ut_types.h"
#include "ut_units.h"
#include "xap_Frame.h"
#include "xap_Dialog.h"
#include "fp_PageSize.h"

/* Word-style "Document" dialog: Margins and Layout pages controlling
 * the section-level page setup (margins, gutter, header/footer edge
 * offsets, odd/even and first-page headers, section start, vertical
 * alignment, apply-to scope) plus access to the print page-setup
 * dialog and the "make default" (normal.awt) action.
 */
class ABI_EXPORT AP_Dialog_Document : public XAP_Dialog_NonPersistent
{
public:
	AP_Dialog_Document(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id);
	virtual ~AP_Dialog_Document() = 0;

	virtual void runModal(XAP_Frame * pFrame) override = 0;

	enum tAnswer : uint8_t { a_OK, a_CANCEL };
	enum tApplyTo : uint8_t
	{
		APPLY_WHOLE_DOC,		/* every section */
		APPLY_THIS_SECTION,		/* section containing the caret */
		APPLY_POINT_FORWARD		/* new section starting at the caret */
	};
	enum tSectionStart : uint8_t
	{
		SECTION_CONTINUOUS, SECTION_NEW_PAGE,
		SECTION_EVEN_PAGE, SECTION_ODD_PAGE
	};
	enum tVAlign : uint8_t
	{
		VALIGN_TOP, VALIGN_CENTER, VALIGN_JUSTIFY, VALIGN_BOTTOM
	};
	enum tMultiPage : uint8_t
	{
		MULTI_NORMAL, MULTI_MIRROR, MULTI_TWO_PER_SHEET, MULTI_BOOK
	};
	enum tGutterPos : uint8_t { GUTTER_LEFT, GUTTER_TOP };
	enum tPage : uint8_t { PAGE_MARGINS, PAGE_LAYOUT };

#define SET_GATHER(a, u)  inline u get##a(void) const {return m_##a;} \
						  inline void set##a(u p##a) {m_##a = p##a;}
	SET_GATHER(MarginUnits,		UT_Dimension);
	SET_GATHER(MarginTop,		float);
	SET_GATHER(MarginBottom,	float);
	SET_GATHER(MarginLeft,		float);
	SET_GATHER(MarginRight,		float);
	SET_GATHER(MarginGutter,	float);
	SET_GATHER(MarginHeader,	float);
	SET_GATHER(MarginFooter,	float);
	SET_GATHER(GutterPosition,	tGutterPos);
	SET_GATHER(MultiplePages,	tMultiPage);
	SET_GATHER(ApplyTo,			tApplyTo);
	SET_GATHER(SectionStart,	tSectionStart);
	SET_GATHER(VerticalAlign,	tVAlign);
	SET_GATHER(DifferentOddEven,bool);
	SET_GATHER(DifferentFirstPage,bool);
	SET_GATHER(PageSize,		fp_PageSize);
	SET_GATHER(PageUnits,		UT_Dimension);
	SET_GATHER(PageScale,		int);
	SET_GATHER(ActivePage,		tPage);
	SET_GATHER(PageSetupChanged,bool);
#undef SET_GATHER

	virtual inline tAnswer getAnswer(void) const { return m_answer; }

	bool validatePageSettings(void) const;

protected:
	inline void setAnswer(tAnswer answer) { m_answer = answer; }
	XAP_Frame *	m_pFrame;
	tAnswer		m_answer;

private:
	UT_Dimension	m_MarginUnits;
	float			m_MarginTop;
	float			m_MarginBottom;
	float			m_MarginLeft;
	float			m_MarginRight;
	float			m_MarginGutter;
	float			m_MarginHeader;
	float			m_MarginFooter;
	tGutterPos		m_GutterPosition;
	tMultiPage		m_MultiplePages;
	tApplyTo		m_ApplyTo;
	tSectionStart	m_SectionStart;
	tVAlign			m_VerticalAlign;
	bool			m_DifferentOddEven;
	bool			m_DifferentFirstPage;
	fp_PageSize		m_PageSize;
	UT_Dimension	m_PageUnits;
	int				m_PageScale;
	tPage			m_ActivePage;
	bool			m_PageSetupChanged;
};
