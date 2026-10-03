/* Abinova
 * Copyright (C) 1998 AbiSource, Inc.
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
#include "xap_GtkListHelpers.h"

#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"
#include "ap_Dialog_Insert_DateTime.h"
#include "ap_UnixDialog_Insert_DateTime.h"

/*****************************************************************/

#define	LIST_ITEM_INDEX_KEY "index"
#define CUSTOM_RESPONSE_INSERT 1

/*****************************************************************/

XAP_Dialog * AP_UnixDialog_Insert_DateTime::static_constructor(XAP_DialogFactory * pFactory,
															   XAP_Dialog_Id id)
{
	AP_UnixDialog_Insert_DateTime * p = new AP_UnixDialog_Insert_DateTime(pFactory,id);
	return p;
}

AP_UnixDialog_Insert_DateTime::AP_UnixDialog_Insert_DateTime(XAP_DialogFactory * pDlgFactory,
															 XAP_Dialog_Id id)
	: AP_Dialog_Insert_DateTime(pDlgFactory,id)
{
	m_windowMain = nullptr;
	m_tvFormats = nullptr;
	m_selFormats = nullptr;
}

AP_UnixDialog_Insert_DateTime::~AP_UnixDialog_Insert_DateTime(void)
{
}

/*****************************************************************/
/*****************************************************************/

void AP_UnixDialog_Insert_DateTime::runModal(XAP_Frame * pFrame)
{
	UT_return_if_fail(pFrame);
	
	// Build the window's widgets and arrange them
	m_windowMain = _constructWindow();
	UT_return_if_fail(m_windowMain);

	// Populate the window's data items
	_populateWindowData();

	switch(abiRunModalDialog(GTK_DIALOG(m_windowMain), pFrame, this,
							 CUSTOM_RESPONSE_INSERT, false ))
	{
		case CUSTOM_RESPONSE_INSERT:
			event_Insert();
			break;
		default:
			m_answer = AP_Dialog_Insert_DateTime::a_CANCEL;
			break;
	}

	abiDestroyWidget ( m_windowMain ) ;
}

void AP_UnixDialog_Insert_DateTime::s_date_dblclicked(GtkListView * /*listview*/,
													  guint /*position*/,
													  AP_UnixDialog_Insert_DateTime * me)
{
	gtk_dialog_response (GTK_DIALOG(me->m_windowMain), CUSTOM_RESPONSE_INSERT);
}

void AP_UnixDialog_Insert_DateTime::event_Insert(void)
{
	UT_ASSERT(m_windowMain && m_tvFormats);

	// if there is no selection return cancel.  GTK can make this happen.
	int formatIndex = XAP_single_selection_get_int(m_selFormats);
	if (formatIndex < 0)
	{
		m_answer = AP_Dialog_Insert_DateTime::a_CANCEL;
		return;
	}

	// the ID of the selected DateTime format
	m_iFormatIndex = formatIndex;
	m_answer = AP_Dialog_Insert_DateTime::a_OK;
}

/*****************************************************************/
GtkWidget * AP_UnixDialog_Insert_DateTime::_constructWindow(void)
{
	GtkWidget * window;
	const XAP_StringSet * pSS = m_pApp->getStringSet();

	GtkBuilder * builder = newDialogBuilderFromResource("ap_UnixDialog_Insert_DateTime.ui");

	// Update our member variables with the important widgets that 
	// might need to be queried or altered later
	window = GTK_WIDGET(gtk_builder_get_object(builder, "ap_UnixDialog_Insert_DateTime"));
	m_tvFormats = GTK_WIDGET(gtk_builder_get_object(builder, "tvFormats"));

	GListStore *store = XAP_list_store_new();
	m_selFormats =
		XAP_list_view_set_model(GTK_LIST_VIEW(m_tvFormats), store);
	g_object_unref(store);

	// set the dialog title
	std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_DateTime_DateTimeTitle,s);
	abiDialogSetTitle(window, "%s", s.c_str());

	// the format list needs room to breathe (Word's dialog is a
	// similar size); the .ui width/height requests do not size the
	// window under GTK4, so set the default size explicitly
	gtk_window_set_default_size(GTK_WINDOW(window), 440, 380);
	
	// localize the strings in our dialog
	
	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbAvailableFormats")), pSS, AP_STRING_ID_DLG_DateTime_AvailableFormats);
	localizeButtonUnderline(GTK_WIDGET(gtk_builder_get_object(builder, "btInsert")), pSS, AP_STRING_ID_DLG_InsertButton);
	
	g_signal_connect_after(G_OBJECT(m_tvFormats),
						   "activate",
						   G_CALLBACK(s_date_dblclicked),
						   static_cast<gpointer>(this));
	
	g_object_unref(G_OBJECT(builder));
	return window;
}

void AP_UnixDialog_Insert_DateTime::_populateWindowData(void)
{
	UT_ASSERT(m_windowMain && m_tvFormats);

	// NOTE : this code is similar to the Windows dialog code to do
	// NOTE : the same thing.  if you are implementing this dialog
	// NOTE : for a new front end, this is the formatting logic 
	// NOTE : you'll want to use to populate your list

	UT_sint32 i;

	// this constant comes from ap_Dialog_Insert_DateTime.h
    char szCurrentDateTime[CURRENT_DATE_TIME_SIZE];

    time_t tim = time(nullptr);

    struct tm *pTime = localtime(&tim);

	GListStore *model = G_LIST_STORE(
		gtk_single_selection_get_model(m_selFormats));
	g_list_store_remove_all(model);

 	// build a list of all items
    for (i = 0; InsertDateTimeFmts[i] != nullptr; i++)
	{
		gsize bytes_read = 0, bytes_written = 0;
		char * utf;

        strftime(szCurrentDateTime, CURRENT_DATE_TIME_SIZE, InsertDateTimeFmts[i], pTime);

		utf = g_locale_to_utf8(szCurrentDateTime, -1, &bytes_read, &bytes_written, nullptr);
		if (utf) {
			// Add a new row to the model
			XAP_list_store_append_text_and_int(model, utf, i);
		}
		g_free(utf);
	}

	// now select first item in box
 	gtk_widget_grab_focus (m_tvFormats);
}
