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

#pragma once

#include "ut_types.h"
#include "xap_Frame.h"
#include "xap_Dialog.h"

class XAP_Frame;

class ABI_EXPORT AP_Dialog_TextToTable : public XAP_Dialog_NonPersistent
{
public:
	AP_Dialog_TextToTable(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id);
	virtual ~AP_Dialog_TextToTable(void);

	virtual void runModal(XAP_Frame * pFrame) override = 0;

	enum tAnswer: uint8_t { a_OK, a_CANCEL };
	// separator modes map 1:1 onto FV_View::cmdTextToTable's szSep tokens
	enum sepType: uint8_t { sep_TABS = 0, sep_COMMAS, sep_PARAS, sep_OTHER };

	AP_Dialog_TextToTable::tAnswer	getAnswer(void) const;
	AP_Dialog_TextToTable::sepType	getSepType(void) const;
	// "tabs" | "commas" | "paras" | "other" — the token cmdTextToTable
	// understands; nullptr-safe, always returns a valid token
	const char *					getSepName(void) const;
	UT_UCS4Char						getSepChar(void) const;
	UT_sint32						getNumCols(void) const;
	// delimiter modes can let the converter detect the column count;
	// "paras" always needs an explicit one (one paragraph per cell)
	bool							getAutoCols(void) const;

protected:
	AP_Dialog_TextToTable::tAnswer	m_answer;
	AP_Dialog_TextToTable::sepType	m_sepType;
	UT_UCS4Char						m_sepChar;
	UT_sint32						m_numCols;
	bool							m_bAutoCols;
};
