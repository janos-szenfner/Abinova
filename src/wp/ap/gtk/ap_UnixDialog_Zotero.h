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

#include <gtk/gtk.h>

#include "ap_Dialog_Zotero.h"

class XAP_UnixFrame;

/*****************************************************************/

class AP_UnixDialog_Zotero : public AP_Dialog_Zotero
{
public:
	AP_UnixDialog_Zotero(XAP_DialogFactory * pDlgFactory,
						 XAP_Dialog_Id id);
	virtual ~AP_UnixDialog_Zotero(void);

	virtual void runModal(XAP_Frame * pFrame) override;

	static XAP_Dialog *		static_constructor(XAP_DialogFactory *,
											   XAP_Dialog_Id id);

protected:
	enum ResponseId: int8_t
	{
		BUTTON_CLOSE = GTK_RESPONSE_CLOSE
	};

	GtkWidget *		constructWindow(void);
	void			rebuildResults(void);
	void			event_Search(void);
	void			event_Insert(bool bBib);
	gint			selectedRow(void) const;

	static void search_clicked_callback(GtkWidget *,
										AP_UnixDialog_Zotero * me)
	{
		me->event_Search();
	}

	static void entry_activate_callback(GtkEntry *,
										AP_UnixDialog_Zotero * me)
	{
		me->event_Search();
	}

	static void insert_clicked_callback(GtkWidget *,
										AP_UnixDialog_Zotero * me)
	{
		me->event_Insert(false);
	}

	static void insertbib_clicked_callback(GtkWidget *,
										   AP_UnixDialog_Zotero * me)
	{
		me->event_Insert(true);
	}

	static void row_activated_callback(GtkListBox *,
									   GtkListBoxRow *,
									   AP_UnixDialog_Zotero * me)
	{
		me->event_Insert(false);
	}

	static void row_selected_callback(GtkListBox *,
									  GtkListBoxRow *,
									  AP_UnixDialog_Zotero * me);

private:
	GtkWidget *				m_entry;
	GtkWidget *				m_styleCombo;
	GtkWidget *				m_listBox;
	GtkWidget *				m_status;
	GtkWidget *				m_insertBtn;
	GtkWidget *				m_insertBibBtn;
};
