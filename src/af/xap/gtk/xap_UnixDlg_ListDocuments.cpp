/* Abinova
 * Copyright (C) 2000 AbiSource, Inc.
 * Copyright (C) 2004 Hubert Figuiere
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
#include <time.h>

#include "ut_string.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"

// This header defines some functions for Unix dialogs,
// like centering them, measuring them, etc.
#include "xap_UnixDialogHelper.h"

#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"

#include "xap_Strings.h"
#include "xap_Dialog_Id.h"
#include "xap_Dlg_ListDocuments.h"
#include "xap_GtkListHelpers.h"
#include "xap_UnixDlg_ListDocuments.h"

#define CUSTOM_RESPONSE_VIEW 1

/*****************************************************************/

XAP_Dialog * XAP_UnixDialog_ListDocuments::static_constructor(XAP_DialogFactory * pFactory,
													 XAP_Dialog_Id id)
{
	XAP_UnixDialog_ListDocuments * p = new XAP_UnixDialog_ListDocuments(pFactory,id);
	return p;
}

XAP_UnixDialog_ListDocuments::XAP_UnixDialog_ListDocuments(XAP_DialogFactory * pDlgFactory,
										 XAP_Dialog_Id id)
	: XAP_Dialog_ListDocuments(pDlgFactory,id),
		m_listWindows(nullptr),
		m_selDocs(nullptr),
		m_windowMain(nullptr)
{
}

XAP_UnixDialog_ListDocuments::~XAP_UnixDialog_ListDocuments(void)
{
}

void XAP_UnixDialog_ListDocuments::s_list_activated(GtkListView * /*listview*/,
													 guint /*position*/,
													 XAP_UnixDialog_ListDocuments * me)
{
	gtk_dialog_response (GTK_DIALOG(me->m_windowMain), CUSTOM_RESPONSE_VIEW);
}

void XAP_UnixDialog_ListDocuments::runModal(XAP_Frame * pFrame)
{
  // Build the window's widgets and arrange them
  GtkWidget * mainWindow = _constructWindow();
  UT_return_if_fail(mainWindow);
	
  // Populate the window's data items
  _populateWindowData();

  switch ( abiRunModalDialog ( GTK_DIALOG(mainWindow), pFrame, this, CUSTOM_RESPONSE_VIEW, false ) )
    {
    case CUSTOM_RESPONSE_VIEW:
      event_View () ; break ;
    default:
      event_Cancel (); break ;
    }

  abiDestroyWidget ( mainWindow ) ;
}

void XAP_UnixDialog_ListDocuments::event_View(void)
{
	// no selection means nothing to view — GTK can make this happen
	int row = XAP_single_selection_get_int(m_selDocs);

	if (row >= 0) {
		_setSelDocumentIndx(static_cast<UT_uint32>(row));
	}
}

void XAP_UnixDialog_ListDocuments::event_Cancel(void)
{
}

/*****************************************************************/

GtkWidget * XAP_UnixDialog_ListDocuments::_constructWindow(void)
{
	GtkWidget *w;

	// load the dialog from the UI file
	GtkBuilder* builder = newDialogBuilderFromResource("xap_UnixDlg_ListDocuments.ui");

	// Update our member variables with the important widgets that
	// might need to be queried or altered later
	m_windowMain = GTK_WIDGET(gtk_builder_get_object(builder, "xap_UnixDlg_ListDocuments"));
	m_listWindows = GTK_WIDGET(gtk_builder_get_object(builder, "lvAvailableDocuments"));

	gtk_window_set_title (GTK_WINDOW(m_windowMain), _getTitle());
	w = GTK_WIDGET(gtk_builder_get_object(builder, "lbAvailableDocuments"));
	setLabelMarkup(w, _getHeading());
	w = GTK_WIDGET(gtk_builder_get_object(builder, "btView"));
	gtk_button_set_label(GTK_BUTTON(w), _getOKButtonText());

	// dbl-click / Enter activates the View button
	g_signal_connect_after(G_OBJECT(m_listWindows),
						   "activate",
						   G_CALLBACK(s_list_activated),
						   static_cast<gpointer>(this));

	g_object_unref(G_OBJECT(builder));

	return m_windowMain;
}

void XAP_UnixDialog_ListDocuments::_populateWindowData(void)
{
	GListStore *model = XAP_list_store_new();

	for (UT_sint32 i = 0; i < _getDocumentCount(); i++)
    {
		const char *s = _getNthDocumentName(i);
		if (!s || !*s)
			s = "Untitled"; // unsaved documents have no filename
		XAP_list_store_append_text_and_int(model, s, i);
    }

	m_selDocs =
		XAP_list_view_set_model(GTK_LIST_VIEW(m_listWindows), model);

	g_object_unref (model);

	// now select first item in box
 	gtk_widget_grab_focus (m_listWindows);
}

