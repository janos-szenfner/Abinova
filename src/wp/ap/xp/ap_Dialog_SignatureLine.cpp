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

#include "xap_Dialog_Id.h"
#include "xap_DialogFactory.h"
#include "ap_Dialog_SignatureLine.h"

AP_Dialog_SignatureLine::AP_Dialog_SignatureLine(XAP_DialogFactory * pDlgFactory,
											   XAP_Dialog_Id id)
	: AP_Dialog_Modal(pDlgFactory, id, "interface/signatureline")
	, m_answer(a_CANCEL)
{
}

AP_Dialog_SignatureLine::~AP_Dialog_SignatureLine(void)
{
}

void AP_Dialog_SignatureLine::setAnswer(AP_Dialog_SignatureLine::tAnswer a)
{
	m_answer = a;
}

AP_Dialog_SignatureLine::tAnswer AP_Dialog_SignatureLine::getAnswer(void) const
{
	return m_answer;
}

const FV_SignatureSetup & AP_Dialog_SignatureLine::getSignatureSetup(void) const
{
	return m_sig;
}

FV_SignatureSetup & AP_Dialog_SignatureLine::getSignatureSetup(void)
{
	return m_sig;
}

void AP_Dialog_SignatureLine::setSignatureSetup(const FV_SignatureSetup & sig)
{
	m_sig = sig;
}
