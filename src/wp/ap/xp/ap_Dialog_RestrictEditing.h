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
#include "ut_string_class.h"
#include "xap_Frame.h"
#include "xap_Dialog.h"

class XAP_Frame;

/* "Start Enforcing Protection": pick the restriction mode and an
 * optional password.  Stopping protection reuses the plain
 * XAP_DIALOG_ID_PASSWORD prompt. */
class ABI_EXPORT AP_Dialog_RestrictEditing : public XAP_Dialog_NonPersistent
{
public:
	AP_Dialog_RestrictEditing(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id);
	virtual ~AP_Dialog_RestrictEditing(void);

	virtual void runModal(XAP_Frame * pFrame) override = 0;

	enum tAnswer: uint8_t { a_OK, a_CANCEL };
	// modes map 1:1 onto PD_Document's document-protection-mode
	// tokens (the two enforced modes we implement)
	enum modeType: uint8_t { mode_TRACKED = 0, mode_READONLY };

	AP_Dialog_RestrictEditing::tAnswer	getAnswer(void) const;
	AP_Dialog_RestrictEditing::modeType	getMode(void) const;
	// "trackedChanges" | "readOnly" — the OOXML w:edit token
	const char *					getModeName(void) const;
	const UT_UTF8String &			getPassword(void) const;
	const UT_UTF8String &			getConfirm(void) const;

protected:
	AP_Dialog_RestrictEditing::tAnswer	m_answer;
	AP_Dialog_RestrictEditing::modeType	m_mode;
	UT_UTF8String					m_password;
	UT_UTF8String					m_confirm;
};
