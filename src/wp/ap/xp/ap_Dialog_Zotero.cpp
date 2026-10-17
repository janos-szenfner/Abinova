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

#include "ap_Strings.h"
#include "xap_App.h"

#include "ap_Dialog_Zotero.h"

AP_Dialog_Zotero::AP_Dialog_Zotero(XAP_DialogFactory * pDlgFactory,
								   XAP_Dialog_Id id)
	: XAP_Dialog_NonPersistent(pDlgFactory, id, "interface/dialogzotero")
	, m_pView(nullptr)
	, m_answer(a_CANCEL)
{
}

AP_Dialog_Zotero::~AP_Dialog_Zotero(void)
{
}

bool AP_Dialog_Zotero::search(void)
{
	m_items.clear();
	m_sStatus.clear();
	if (m_sQuery.empty())
		return false;
	std::string err;
	if (!XAP_zoteroSearch(m_sQuery, m_sStyle.c_str(), m_items, err))
	{
		m_sStatus = err;
		return false;
	}
	if (m_items.empty())
	{
		const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
		if (pSS)
			pSS->getValueUTF8(AP_STRING_ID_DLG_Zotero_NoResults, m_sStatus);
		return false;
	}
	return true;
}

UT_uint32 AP_Dialog_Zotero::getResultCount(void) const
{
	return static_cast<UT_uint32>(m_items.size());
}

const char * AP_Dialog_Zotero::getResultTitle(UT_uint32 i) const
{
	if (i >= m_items.size())
		return "";
	return m_items[i].title.empty() ? "(untitled)" : m_items[i].title.c_str();
}

const char * AP_Dialog_Zotero::getResultPreview(UT_uint32 i) const
{
	if (i >= m_items.size())
		return "";
	const XAP_ZoteroItem & it = m_items[i];
	if (!it.bib.empty())
		return it.bib.c_str();
	return it.citation.c_str();
}

bool AP_Dialog_Zotero::insertItem(UT_uint32 i, bool bBib)
{
	UT_return_val_if_fail(m_pView, false);
	if (i >= m_items.size())
		return false;
	const XAP_ZoteroItem & it = m_items[i];
	const std::string & text =
		(bBib && !it.bib.empty()) ? it.bib
		: (!it.citation.empty() ? it.citation : it.bib);
	return m_pView->cmdInsertZoteroCitation(
		it.key.c_str(), m_sStyle.c_str(), bBib ? "bib" : "citation",
		text.c_str()) != UT_ERROR;
}
