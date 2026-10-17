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

#include "ut_assert.h"

#include "fv_View.h"
#include "pd_Document.h"

#include "ap_Dialog_DocInspector.h"

AP_Dialog_DocInspector::AP_Dialog_DocInspector(XAP_DialogFactory * pDlgFactory,
											   XAP_Dialog_Id id)
	: XAP_Dialog_NonPersistent(pDlgFactory, id, "interface/dialogdocinspector")
	, m_pView(nullptr)
	, m_answer(a_CLOSE)
{
}

AP_Dialog_DocInspector::~AP_Dialog_DocInspector(void)
{
}

void AP_Dialog_DocInspector::setView(FV_View * pView)
{
	m_pView = pView;
	refresh();
}

void AP_Dialog_DocInspector::refresh(void)
{
	UT_return_if_fail(m_pView && m_pView->getDocument());
	AP_DocInspect::inspect(m_pView->getDocument(), m_report);
	m_sReport = AP_DocInspect::toText(m_report);
	m_selected.assign(m_report.cats.size(), false);
	for (size_t i = 0; i < m_report.cats.size(); ++i)
		m_selected[i] = m_report.cats[i].found;
}

UT_uint32 AP_Dialog_DocInspector::getCategoryCount(void) const
{
	return static_cast<UT_uint32>(m_report.cats.size());
}

UT_uint32 AP_Dialog_DocInspector::getCategoryMask(UT_uint32 i) const
{
	if (i >= m_report.cats.size())
		return 0;
	return m_report.cats[i].mask;
}

const char * AP_Dialog_DocInspector::getCategoryTitle(UT_uint32 i) const
{
	if (i >= m_report.cats.size())
		return "";
	return m_report.cats[i].title.c_str();
}

bool AP_Dialog_DocInspector::isCategoryFound(UT_uint32 i) const
{
	if (i >= m_report.cats.size())
		return false;
	return m_report.cats[i].found;
}

std::string AP_Dialog_DocInspector::getCategorySummary(UT_uint32 i) const
{
	if (i >= m_report.cats.size())
		return "";
	std::string s;
	for (const std::string & line : m_report.cats[i].details)
	{
		if (!s.empty())
			s += "; ";
		s += line;
	}
	return s;
}

void AP_Dialog_DocInspector::setSelected(UT_uint32 i, bool b)
{
	if (i < m_selected.size())
		m_selected[i] = b;
}

bool AP_Dialog_DocInspector::isSelected(UT_uint32 i) const
{
	return i < m_selected.size() ? m_selected[i] : false;
}

UT_uint32 AP_Dialog_DocInspector::getSelectedMask(void) const
{
	UT_uint32 mask = 0;
	for (size_t i = 0; i < m_report.cats.size(); ++i)
		if (i < m_selected.size() && m_selected[i] && m_report.cats[i].found)
			mask |= m_report.cats[i].mask;
	return mask;
}

UT_uint32 AP_Dialog_DocInspector::removeSelected(void)
{
	UT_return_val_if_fail(m_pView, 0);
	UT_uint32 done = AP_DocInspect::scrub(m_pView, getSelectedMask());
	refresh();
	return done;
}
