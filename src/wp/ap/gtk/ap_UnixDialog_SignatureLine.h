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

#include "ap_Dialog_SignatureLine.h"

class XAP_UnixFrame;

class AP_UnixDialog_SignatureLine: public AP_Dialog_SignatureLine
{
public:
	AP_UnixDialog_SignatureLine(XAP_DialogFactory * pDlgFactory,
								XAP_Dialog_Id id);
	virtual ~AP_UnixDialog_SignatureLine(void);

	virtual void runModal(XAP_Frame * pFrame) override;

	static XAP_Dialog *		static_constructor(XAP_DialogFactory *, XAP_Dialog_Id id);

	void event_OK(void);
	void event_Cancel(void);

protected:
	virtual GtkWidget *		_constructWindow(void);
	void _constructWindowContents (GtkWidget * container);

	enum ResponseId: int8_t
	  {
	    BUTTON_CANCEL = GTK_RESPONSE_CANCEL,
	    BUTTON_OK = GTK_RESPONSE_OK
	  };

	GtkWidget * m_windowMain;

	GtkWidget * m_entrySigner;
	GtkWidget * m_entryTitle;
	GtkWidget * m_entryEmail;
	GtkWidget * m_textInstructions;
	GtkWidget * m_checkComments;
	GtkWidget * m_checkShowDate;
};
