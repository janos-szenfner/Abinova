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
#include "xap_Zotero.h"

class FV_View;
class XAP_Frame;

/* Zotero citation picker: searches the local Zotero service and
 * inserts a CSL-formatted citation (or bibliography entry) for the
 * chosen item as a "zotero" field — the item key + style stay on
 * the field so zoteroRefresh can re-render it later. */
class ABI_EXPORT AP_Dialog_Zotero : public XAP_Dialog_NonPersistent
{
public:
	AP_Dialog_Zotero(XAP_DialogFactory * pDlgFactory,
					 XAP_Dialog_Id id);
	virtual ~AP_Dialog_Zotero(void);

	virtual void runModal(XAP_Frame * pFrame) override = 0;

	enum tAnswer: uint8_t { a_CANCEL = 0 };

	tAnswer			getAnswer(void) const { return m_answer; }
	void			setAnswer(tAnswer a) { m_answer = a; }

	void			setView(FV_View * pView) { m_pView = pView; }

	void			setQuery(const std::string & q) { m_sQuery = q; }
	const std::string & getQuery(void) const { return m_sQuery; }
	void			setStyle(const std::string & s) { m_sStyle = s; }
	const std::string & getStyle(void) const { return m_sStyle; }

	/* run the search; false = service unreachable / bad reply,
	 * m_sStatus names the cause (or "no results") */
	bool			search(void);
	const std::string & getStatus(void) const { return m_sStatus; }

	UT_uint32		getResultCount(void) const;
	const char *	getResultTitle(UT_uint32 i) const;
	const char *	getResultPreview(UT_uint32 i) const;

	/* insert the i-th hit at the caret; bBib picks the bibliography
	 * form over the in-text citation */
	bool			insertItem(UT_uint32 i, bool bBib);

protected:
	tAnswer					m_answer;

private:
	FV_View *				m_pView;
	std::string				m_sQuery;
	std::string				m_sStyle;
	std::string				m_sStatus;
	std::vector<XAP_ZoteroItem> m_items;
};
