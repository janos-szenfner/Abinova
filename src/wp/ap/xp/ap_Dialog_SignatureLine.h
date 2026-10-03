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

#include "ap_Dialog_Modal.h"
#include "fv_View.h"

class XAP_Frame;

/* Signature Setup dialog (Word parity): collects the suggested
 * signer's details; on OK the caller inserts a signature-line object
 * via FV_View::insertSignatureLine. */
class ABI_EXPORT AP_Dialog_SignatureLine : public AP_Dialog_Modal
{
public:
	AP_Dialog_SignatureLine(XAP_DialogFactory * pDlgFactory,
							XAP_Dialog_Id id);
	virtual ~AP_Dialog_SignatureLine(void);

	virtual void runModal(XAP_Frame * pFrame) override = 0;

	enum tAnswer: uint8_t { a_OK=0, a_CANCEL=1 };

	tAnswer 			getAnswer(void) const;
	void				setAnswer(tAnswer a);

	const FV_SignatureSetup & getSignatureSetup(void) const;
	FV_SignatureSetup &       getSignatureSetup(void);

	void				setSignatureSetup(const FV_SignatureSetup & sig);

private:
	tAnswer				m_answer;
	FV_SignatureSetup	m_sig;
};
