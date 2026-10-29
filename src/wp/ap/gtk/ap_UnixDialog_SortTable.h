/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
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

#include "xap_UnixDialog.h"
#include "ap_Dialog_SortTable.h"

class XAP_UnixFrame;

/*****************************************************************/

class AP_UnixDialog_SortTable
  : public AP_Dialog_SortTable
  , public XAP_UnixDialog
{
public:
	AP_UnixDialog_SortTable(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id);
	virtual ~AP_UnixDialog_SortTable(void);

	virtual void runModal(XAP_Frame * pFrame) override;

	static XAP_Dialog *		static_constructor(XAP_DialogFactory *, XAP_Dialog_Id id);

protected:
	enum ResponseId: int8_t
	{
		BUTTON_SORT = 1
	};

	virtual GtkWidget *		_constructWindow(void);
	void					_populateWindowData(void);
	void					_storeWindowData(void);

	// pointers to widgets we need to query/set
	GtkWidget * m_windowMain;
	GtkWidget * m_wColumn;
	GtkWidget * m_wTypeText;
	GtkWidget * m_wTypeNumber;
	GtkWidget * m_wTypeDate;
	GtkWidget * m_wAsc;
	GtkWidget * m_wDesc;
	GtkWidget * m_wHeader;
};
