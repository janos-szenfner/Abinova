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

#include <vector>
#include <gtk/gtk.h>

#include "ap_Dialog_DocInspector.h"

class XAP_UnixFrame;

/*****************************************************************/

class AP_UnixDialog_DocInspector : public AP_Dialog_DocInspector
{
public:
	AP_UnixDialog_DocInspector(XAP_DialogFactory * pDlgFactory,
							   XAP_Dialog_Id id);
	virtual ~AP_UnixDialog_DocInspector(void);

	virtual void runModal(XAP_Frame * pFrame) override;

	static XAP_Dialog *		static_constructor(XAP_DialogFactory *,
											   XAP_Dialog_Id id);

protected:
	enum ResponseId: int8_t
	{
		BUTTON_CLOSE = GTK_RESPONSE_CLOSE
	};

	GtkWidget *		constructWindow(void);
	void			rebuildCategories(void);
	void			event_Remove(void);
	void			event_Reinspect(void);

	static void check_toggled_callback(GtkCheckButton * cb,
									   AP_UnixDialog_DocInspector * me)
	{
		me->setSelected(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(cb),
														 "cat-index")), 
						gtk_check_button_get_active(cb));
	}

	static void remove_clicked_callback(GtkWidget *,
										AP_UnixDialog_DocInspector * me)
	{
		me->event_Remove();
	}

	static void reinspect_clicked_callback(GtkWidget *,
										   AP_UnixDialog_DocInspector * me)
	{
		me->event_Reinspect();
	}

private:
	GtkWidget *				m_catBox;
	GtkWidget *				m_reportView;
	GtkWidget *				m_removeBtn;
	std::vector<GtkWidget *> m_checks;
};
