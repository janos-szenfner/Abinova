/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t; -*- */
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

#include <stdlib.h>

#include "ut_string.h"
#include "ut_string_class.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"

// This header defines some functions for Unix dialogs,
// like centering them, measuring them, etc.
#include "xap_UnixDialogHelper.h"

#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"
#include "xap_GtkUtils.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"
#include "ap_Dialog_RestrictEditing.h"
#include "ap_UnixDialog_RestrictEditing.h"

/*****************************************************************/

XAP_Dialog * AP_UnixDialog_RestrictEditing::static_constructor(XAP_DialogFactory * pFactory,
															 XAP_Dialog_Id id)
{
	return new AP_UnixDialog_RestrictEditing(pFactory,id);
}

AP_UnixDialog_RestrictEditing::AP_UnixDialog_RestrictEditing(XAP_DialogFactory * pDlgFactory,
														   XAP_Dialog_Id id)
	: AP_Dialog_RestrictEditing(pDlgFactory, id),
	  m_windowMain(nullptr),
	  m_wRbTracked(nullptr),
	  m_wRbReadOnly(nullptr),
	  m_wPassword(nullptr),
	  m_wConfirm(nullptr)
{
}

AP_UnixDialog_RestrictEditing::~AP_UnixDialog_RestrictEditing(void)
{
}

void AP_UnixDialog_RestrictEditing::runModal(XAP_Frame * pFrame)
{
	// Build the dialog's window
	m_windowMain = _constructWindow();
	UT_return_if_fail(m_windowMain);

	// Populate the window's data items
	_populateWindowData();

	switch ( abiRunModalDialog ( GTK_DIALOG(m_windowMain),
								 pFrame, this, BUTTON_START, false ) )
	{
		case BUTTON_START:
			m_answer = AP_Dialog_RestrictEditing::a_OK;
			break;
		default:
			m_answer = AP_Dialog_RestrictEditing::a_CANCEL;
			break;
	}

	_storeWindowData();

	abiDestroyWidget ( m_windowMain ) ;
}

/*****************************************************************/

GtkWidget * AP_UnixDialog_RestrictEditing::_constructWindow(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();

	GtkBuilder * builder = newDialogBuilderFromResource("ap_UnixDialog_RestrictEditing.ui");
	GtkWidget * window =
		GTK_WIDGET(gtk_builder_get_object(builder, "ap_UnixDialog_RestrictEditing"));
	m_wRbTracked = GTK_WIDGET(gtk_builder_get_object(builder, "rbTracked"));
	m_wRbReadOnly = GTK_WIDGET(gtk_builder_get_object(builder, "rbReadOnly"));
	m_wPassword = GTK_WIDGET(gtk_builder_get_object(builder, "enPassword"));
	m_wConfirm = GTK_WIDGET(gtk_builder_get_object(builder, "enConfirm"));

	// set the dialog title
	std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_RestrictEditing_Title, s);
	abiDialogSetTitle(window, "%s", s.c_str());

	// localize the strings in our dialog
	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbRestrict")),
						pSS, AP_STRING_ID_DLG_RestrictEditing_Mode);
	localizeButton(m_wRbTracked, pSS, AP_STRING_ID_DLG_RestrictEditing_Tracked);
	localizeButton(m_wRbReadOnly, pSS, AP_STRING_ID_DLG_RestrictEditing_ReadOnly);
	localizeLabel(GTK_WIDGET(gtk_builder_get_object(builder, "lbPassword")),
				  pSS, AP_STRING_ID_DLG_RestrictEditing_Password);
	localizeLabel(GTK_WIDGET(gtk_builder_get_object(builder, "lbConfirm")),
				  pSS, AP_STRING_ID_DLG_RestrictEditing_Confirm);
	localizeButtonUnderline(GTK_WIDGET(gtk_builder_get_object(builder, "btStart")),
							pSS, AP_STRING_ID_DLG_RestrictEditing_Start);

	gtk_widget_grab_focus(m_wPassword);

	g_object_unref(G_OBJECT(builder));

	return window;
}

void AP_UnixDialog_RestrictEditing::_populateWindowData(void)
{
}

void AP_UnixDialog_RestrictEditing::_storeWindowData(void)
{
	m_mode = gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wRbReadOnly))
		? AP_Dialog_RestrictEditing::mode_READONLY
		: AP_Dialog_RestrictEditing::mode_TRACKED;
	const gchar * pw = XAP_gtk_entry_get_text(GTK_EDITABLE(m_wPassword));
	const gchar * cf = XAP_gtk_entry_get_text(GTK_EDITABLE(m_wConfirm));
	m_password = pw ? pw : "";
	m_confirm = cf ? cf : "";
}
