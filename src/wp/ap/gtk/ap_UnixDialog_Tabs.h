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
#include "ap_Dialog_Tabs.h"

class XAP_UnixFrame;

/*****************************************************************/

class AP_UnixDialog_Tabs
  : public AP_Dialog_Tabs
  , public XAP_UnixDialog
{
public:
	AP_UnixDialog_Tabs(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id);
	virtual ~AP_UnixDialog_Tabs(void);

	virtual void runModal(XAP_Frame * pFrame) override;

	static XAP_Dialog *		static_constructor(XAP_DialogFactory *, XAP_Dialog_Id id);

	// called from the listbox/button signal handlers
	void					_onSet(void);
	void					_onClear(void);
	void					_onClearAll(void);
	void					_onStopSelected(void);

protected:
	enum ResponseId: int8_t
	{
		BUTTON_OK = 1
	};

	virtual GtkWidget *		_constructWindow(void);
	void					_populateWindowData(void);
	void					_storeWindowData(void);

	void					_rebuildStopList(void);
	eTabType				_selectedType(void) const;
	eTabLeader				_selectedLeader(void) const;
	/* parse the position entry; returns false on junk the user must
	 * fix before the dialog can accept it */
	bool					_parsePosition(UT_sint32 & iPosition) const;

	// pointers to widgets we need to query/set
	GtkWidget * m_windowMain;
	GtkWidget * m_wPosEntry;
	GtkWidget * m_wStopList;
	GtkWidget * m_wAlign[5];	// Left Center Right Decimal Bar
	GtkWidget * m_wLeader[4];	// None Dots Hyphens Underline
	GtkWidget * m_wDefaultEntry;
	GtkWidget * m_wSet;
	GtkWidget * m_wClear;
	GtkWidget * m_wClearAll;
};
