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

#include <stdlib.h>

#include "ut_assert.h"
#include "ut_string.h"
#include "ut_string_class.h"
#include "ut_debugmsg.h"

#include "xap_Dialog_Id.h"
#include "xap_DialogFactory.h"
#include "ap_Dialog_TextToTable.h"

AP_Dialog_TextToTable::AP_Dialog_TextToTable(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id)
	: XAP_Dialog_NonPersistent(pDlgFactory, id, "interface/dialogtexttotable"),
	  m_answer(a_CANCEL),
	  m_sepType(sep_TABS),
	  m_sepChar(';'),
	  m_numCols(1),
	  m_bAutoCols(true)
{
}

AP_Dialog_TextToTable::~AP_Dialog_TextToTable(void)
{
}

AP_Dialog_TextToTable::tAnswer AP_Dialog_TextToTable::getAnswer(void) const
{
	return m_answer;
}

AP_Dialog_TextToTable::sepType AP_Dialog_TextToTable::getSepType(void) const
{
	return m_sepType;
}

const char * AP_Dialog_TextToTable::getSepName(void) const
{
	switch (m_sepType)
	{
	case sep_COMMAS:
		return "commas";
	case sep_PARAS:
		return "paras";
	case sep_OTHER:
		return "other";
	case sep_TABS:
	default:
		return "tabs";
	}
}

UT_UCS4Char AP_Dialog_TextToTable::getSepChar(void) const
{
	return m_sepChar;
}

UT_sint32 AP_Dialog_TextToTable::getNumCols(void) const
{
	// auto-detect is signalled to cmdTextToTable as <= 0; "paras" always
	// needs an explicit count (one paragraph per cell, row-major)
	return (m_bAutoCols && m_sepType != sep_PARAS) ? 0 : m_numCols;
}

bool AP_Dialog_TextToTable::getAutoCols(void) const
{
	return m_bAutoCols;
}
