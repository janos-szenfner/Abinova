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

#include "ap_Dialog_RestrictEditing.h"

AP_Dialog_RestrictEditing::AP_Dialog_RestrictEditing(XAP_DialogFactory * pDlgFactory,
												   XAP_Dialog_Id id)
	: XAP_Dialog_NonPersistent(pDlgFactory, id),
	  m_answer(a_CANCEL),
	  m_mode(mode_TRACKED)
{
}

AP_Dialog_RestrictEditing::~AP_Dialog_RestrictEditing(void)
{
}

AP_Dialog_RestrictEditing::tAnswer AP_Dialog_RestrictEditing::getAnswer(void) const
{
	return m_answer;
}

AP_Dialog_RestrictEditing::modeType AP_Dialog_RestrictEditing::getMode(void) const
{
	return m_mode;
}

const char * AP_Dialog_RestrictEditing::getModeName(void) const
{
	return m_mode == mode_TRACKED ? "trackedChanges" : "readOnly";
}

const UT_UTF8String & AP_Dialog_RestrictEditing::getPassword(void) const
{
	return m_password;
}

const UT_UTF8String & AP_Dialog_RestrictEditing::getConfirm(void) const
{
	return m_confirm;
}
