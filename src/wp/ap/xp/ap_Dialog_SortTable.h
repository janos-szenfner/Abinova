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

#include "ut_types.h"
#include "xap_Frame.h"
#include "xap_Dialog.h"

class XAP_Frame;

class ABI_EXPORT AP_Dialog_SortTable : public XAP_Dialog_NonPersistent
{
public:
	AP_Dialog_SortTable(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id);
	virtual ~AP_Dialog_SortTable(void);

	virtual void runModal(XAP_Frame * pFrame) override = 0;

	enum tAnswer: uint8_t { a_OK, a_CANCEL };
	// key types map onto FV_View::TABLE_SORT_{TEXT,NUMBER,DATE}
	enum sortType: uint8_t { sort_TEXT = 0, sort_NUMBER, sort_DATE };

	AP_Dialog_SortTable::tAnswer	getAnswer(void) const;

	void							setNumColumns(UT_sint32 n);
	UT_sint32						getNumColumns(void) const;
	// 1-based, as the user sees it
	void							setColumn(UT_sint32 c);
	UT_sint32						getColumn(void) const;
	void							setAscending(bool b);
	bool							isAscending(void) const;
	void							setSortType(sortType t);
	AP_Dialog_SortTable::sortType	getSortType(void) const;
	void							setHasHeader(bool b);
	bool							hasHeader(void) const;
	// FV_View::TABLE_SORT_* equivalent of getSortType()
	UT_sint32						mapSortType(void) const;

protected:
	AP_Dialog_SortTable::tAnswer	m_answer;
	UT_sint32						m_numCols;
	UT_sint32						m_column;
	bool							m_bAsc;
	AP_Dialog_SortTable::sortType	m_type;
	bool							m_bHeader;
};
