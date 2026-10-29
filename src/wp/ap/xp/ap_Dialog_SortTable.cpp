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

#include <stdlib.h>

#include "ut_assert.h"
#include "ut_string.h"
#include "ut_string_class.h"
#include "ut_debugmsg.h"

#include "xap_Dialog_Id.h"
#include "xap_DialogFactory.h"
#include "fv_View.h"
#include "ap_Dialog_SortTable.h"

AP_Dialog_SortTable::AP_Dialog_SortTable(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id)
	: XAP_Dialog_NonPersistent(pDlgFactory, id, "interface/dialogsorttable"),
	  m_answer(a_CANCEL),
	  m_numCols(1),
	  m_column(1),
	  m_bAsc(true),
	  m_type(sort_TEXT),
	  m_bHeader(false)
{
}

AP_Dialog_SortTable::~AP_Dialog_SortTable(void)
{
}

AP_Dialog_SortTable::tAnswer AP_Dialog_SortTable::getAnswer(void) const
{
	return m_answer;
}

void AP_Dialog_SortTable::setNumColumns(UT_sint32 n)
{
	m_numCols = n > 0 ? n : 1;
}

UT_sint32 AP_Dialog_SortTable::getNumColumns(void) const
{
	return m_numCols;
}

void AP_Dialog_SortTable::setColumn(UT_sint32 c)
{
	m_column = c;
}

UT_sint32 AP_Dialog_SortTable::getColumn(void) const
{
	return m_column;
}

void AP_Dialog_SortTable::setAscending(bool b)
{
	m_bAsc = b;
}

bool AP_Dialog_SortTable::isAscending(void) const
{
	return m_bAsc;
}

void AP_Dialog_SortTable::setSortType(AP_Dialog_SortTable::sortType t)
{
	m_type = t;
}

AP_Dialog_SortTable::sortType AP_Dialog_SortTable::getSortType(void) const
{
	return m_type;
}

void AP_Dialog_SortTable::setHasHeader(bool b)
{
	m_bHeader = b;
}

bool AP_Dialog_SortTable::hasHeader(void) const
{
	return m_bHeader;
}

UT_sint32 AP_Dialog_SortTable::mapSortType(void) const
{
	switch (m_type)
	{
	case sort_NUMBER:
		return FV_View::TABLE_SORT_NUMBER;
	case sort_DATE:
		return FV_View::TABLE_SORT_DATE;
	case sort_TEXT:
	default:
		return FV_View::TABLE_SORT_TEXT;
	}
}
