/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
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

#pragma once

#include <string>
#include <vector>

#include "xap_Dialog.h"
#include "ap_DocInspect.h"

class FV_View;
class XAP_Frame;

/* Word-style Document Inspector: reports identifying/hidden content
 * found by AP_DocInspect and lets the user pick which categories to
 * strip.  The platform dialog renders one checkbox row per category
 * (insensitive when nothing was found) plus the detail lines. */
class ABI_EXPORT AP_Dialog_DocInspector : public XAP_Dialog_NonPersistent
{
public:
	AP_Dialog_DocInspector(XAP_DialogFactory * pDlgFactory,
						   XAP_Dialog_Id id);
	virtual ~AP_Dialog_DocInspector(void);

	virtual void runModal(XAP_Frame * pFrame) override = 0;

	enum tAnswer: uint8_t { a_CLOSE = 0 };

	tAnswer			getAnswer(void) const { return m_answer; }
	void			setAnswer(tAnswer a) { m_answer = a; }

	void			setView(FV_View * pView);
	void			refresh(void);

	UT_uint32		getCategoryCount(void) const;
	const char *	getCategoryTitle(UT_uint32 i) const;
	bool			isCategoryFound(UT_uint32 i) const;
	std::string		getCategorySummary(UT_uint32 i) const;
	const std::string & getReportText(void) const { return m_sReport; }

	void			setSelected(UT_uint32 i, bool b);
	UT_uint32		getSelectedMask(void) const;

	/* strip the selected categories and re-inspect; returns the mask
	 * of categories that actually had content removed */
	UT_uint32		removeSelected(void);

protected:
	tAnswer					m_answer;

private:
	FV_View *				m_pView;
	AP_PrivacyReport		m_report;
	std::vector<bool>		m_selected;
	std::string				m_sReport;
};
