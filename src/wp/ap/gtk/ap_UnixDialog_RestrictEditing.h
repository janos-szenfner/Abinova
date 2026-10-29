/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
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

#include "xap_UnixDialog.h"
#include "ap_Dialog_RestrictEditing.h"

class XAP_UnixFrame;

/*****************************************************************/

class AP_UnixDialog_RestrictEditing
  : public AP_Dialog_RestrictEditing
  , public XAP_UnixDialog
{
public:
	AP_UnixDialog_RestrictEditing(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id);
	virtual ~AP_UnixDialog_RestrictEditing(void);

	virtual void runModal(XAP_Frame * pFrame) override;

	static XAP_Dialog *		static_constructor(XAP_DialogFactory *, XAP_Dialog_Id id);

protected:
	enum ResponseId: int8_t
	{
		BUTTON_START = 1
	};

	virtual GtkWidget *		_constructWindow(void);
	void					_populateWindowData(void);
	void					_storeWindowData(void);

	// pointers to widgets we need to query/set
	GtkWidget * m_windowMain;
	GtkWidget * m_wRbTracked;
	GtkWidget * m_wRbReadOnly;
	GtkWidget * m_wPassword;
	GtkWidget * m_wConfirm;
};
