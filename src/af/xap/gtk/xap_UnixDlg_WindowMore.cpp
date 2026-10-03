/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Application Framework
 * Copyright (C) 1998-2000 AbiSource, Inc.
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

#include "ut_string.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"

#include "xap_UnixDialogHelper.h"

#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"

#include "xap_Dialog_Id.h"
#include "xap_Dlg_WindowMore.h"
#include "xap_GtkListHelpers.h"
#include "xap_UnixDlg_WindowMore.h"

/*****************************************************************/

#define CUSTOM_RESPONSE_VIEW 1

/*****************************************************************/

XAP_Dialog * XAP_UnixDialog_WindowMore::static_constructor(XAP_DialogFactory * pFactory,
							   XAP_Dialog_Id id)
{
	return new XAP_UnixDialog_WindowMore(pFactory,id);
}

XAP_UnixDialog_WindowMore::XAP_UnixDialog_WindowMore(XAP_DialogFactory * pDlgFactory,
						     XAP_Dialog_Id id)
  : XAP_Dialog_WindowMore(pDlgFactory,id),
	m_windowMain(nullptr),
	m_listWindows(nullptr),
	m_selDocs(nullptr)
{
}

XAP_UnixDialog_WindowMore::~XAP_UnixDialog_WindowMore(void)
{
}

void XAP_UnixDialog_WindowMore::s_list_dblclicked(GtkListView * /*listview*/,
												  guint /*position*/,
												  XAP_UnixDialog_WindowMore * me)
{
	gtk_dialog_response (GTK_DIALOG(me->m_windowMain), CUSTOM_RESPONSE_VIEW);
}

/*****************************************************************/

void XAP_UnixDialog_WindowMore::runModal(XAP_Frame * pFrame)
{
  // Initialize member so we know where we are now
  m_ndxSelFrame = m_pApp->findFrame(pFrame);
  UT_ASSERT_HARMLESS(m_ndxSelFrame >= 0);

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

void XAP_UnixDialog_WindowMore::event_View(void)
{
	m_answer = XAP_Dialog_WindowMore::a_CANCEL;

	// if there is no selection return cancel.  GTK can make this happen.
	int row = XAP_single_selection_get_int(m_selDocs);

	if (row >= 0) {
		m_ndxSelFrame = static_cast<UT_uint32>(row);
		m_answer = XAP_Dialog_WindowMore::a_OK;
	}
}

void XAP_UnixDialog_WindowMore::event_Cancel(void)
{
  m_answer = XAP_Dialog_WindowMore::a_CANCEL;
}

/*****************************************************************/

GtkWidget * XAP_UnixDialog_WindowMore::_constructWindow(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();

	// load the dialog from the UI file
	GtkBuilder* builder = newDialogBuilderFromResource("xap_UnixDlg_WindowMore.ui");

	// Update our member variables with the important widgets that
	// might need to be queried or altered later
	m_windowMain = GTK_WIDGET(gtk_builder_get_object(builder, "xap_UnixDlg_WindowMore"));
	m_listWindows = GTK_WIDGET(gtk_builder_get_object(builder, "lvAvailableDocuments"));

	std::string s;
	pSS->getValueUTF8(XAP_STRING_ID_DLG_MW_MoreWindows,s);
	gtk_window_set_title (GTK_WINDOW(m_windowMain), s.c_str());
	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbAvailableDocuments")), pSS, XAP_STRING_ID_DLG_MW_AvailableDocuments);
	localizeButtonUnderline(GTK_WIDGET(gtk_builder_get_object(builder, "btView")), pSS, XAP_STRING_ID_DLG_MW_ViewButton);

	// dbl-click / Enter activates the View button
	g_signal_connect_after(G_OBJECT(m_listWindows),
						   "activate",
						   G_CALLBACK(s_list_dblclicked),
						   static_cast<gpointer>(this));

	g_object_unref(G_OBJECT(builder));

	return m_windowMain;
}

void XAP_UnixDialog_WindowMore::_populateWindowData(void)
{
	GListStore *model = XAP_list_store_new();

	for (UT_sint32 i = 0; i < m_pApp->getFrameCount(); i++)
    {
		XAP_Frame * f = m_pApp->getFrame(i);
		UT_return_if_fail(f);

		XAP_list_store_append_text_and_int(model, f->getTitle().c_str(), i);
    }

	m_selDocs =
		XAP_list_view_set_model(GTK_LIST_VIEW(m_listWindows), model);

	// now select first item in box
 	gtk_widget_grab_focus (m_listWindows);

	// select and scroll to the current frame's row
	if (m_ndxSelFrame >= 0 &&
		m_ndxSelFrame < static_cast<UT_sint32>(g_list_model_get_n_items(
			G_LIST_MODEL(model))))
	{
		gtk_list_view_scroll_to(GTK_LIST_VIEW(m_listWindows),
								static_cast<guint>(m_ndxSelFrame),
								static_cast<GtkListScrollFlags>(
									GTK_LIST_SCROLL_SELECT |
									GTK_LIST_SCROLL_FOCUS),
								nullptr);
	}

	g_object_unref (model);
}
