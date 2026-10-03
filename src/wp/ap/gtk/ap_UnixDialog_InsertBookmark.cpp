/* Abinova
 * Copyright (C) 2001 AbiSource, Inc.
 * Copyright (C) 2009 Hubert Figuiere
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

#include <list>
#include <string>

#include "ut_string.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"

// This header defines some functions for Unix dialogs,
// like centering them, measuring them, etc.
#include "xap_UnixDialogHelper.h"
#include "xap_GtkUtils.h"

#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"
#include "ap_Dialog_InsertBookmark.h"
#include "ap_UnixDialog_InsertBookmark.h"

/*****************************************************************/

#define BUTTON_INSERT 1

/*****************************************************************/

XAP_Dialog * AP_UnixDialog_InsertBookmark::static_constructor(XAP_DialogFactory * pFactory,
													 XAP_Dialog_Id id)
{
	AP_UnixDialog_InsertBookmark * p = new AP_UnixDialog_InsertBookmark(pFactory,id);
	return p;
}

AP_UnixDialog_InsertBookmark::AP_UnixDialog_InsertBookmark(XAP_DialogFactory * pDlgFactory,
										 XAP_Dialog_Id id)
	: AP_Dialog_InsertBookmark(pDlgFactory,id)
	, m_windowMain(nullptr)
	, m_entryBookmark(nullptr)
	, m_listBookmarks(nullptr)
	, m_btnBookmarks(nullptr)
	, m_buttonInsert(nullptr)
{
}

AP_UnixDialog_InsertBookmark::~AP_UnixDialog_InsertBookmark(void)
{
}

/*****************************************************************/
/***********************************************************************/

void AP_UnixDialog_InsertBookmark::runModal(XAP_Frame * pFrame)
{
	UT_return_if_fail(pFrame);
	// Build the window's widgets and arrange them
	GtkWidget * mainWindow = _constructWindow();
	UT_return_if_fail(mainWindow);

	// Populate the window's data items
	_setList();

	switch(abiRunModalDialog(GTK_DIALOG(mainWindow), pFrame, this,
				 BUTTON_INSERT, false))
	  {
	  case BUTTON_INSERT:
	    event_OK () ; break ;
	  case BUTTON_DELETE:
	    event_Delete () ; break ;
	  default:
	    event_Cancel () ; break ;
	  }
	
	abiDestroyWidget ( mainWindow ) ;
}

void AP_UnixDialog_InsertBookmark::event_OK(void)
{
	UT_ASSERT(m_windowMain);
	// get the bookmark name, if any (return cancel if no name given)
	const gchar *mark = XAP_gtk_entry_get_text(GTK_EDITABLE(m_entryBookmark));
	if(mark && *mark)
	{
		xxx_UT_DEBUGMSG(("InsertBookmark: OK pressed, first char 0x%x\n", static_cast<UT_uint32>(*mark)));
		setAnswer(AP_Dialog_InsertBookmark::a_OK);
		setBookmark(mark);
	}
	else
	{
		setAnswer(AP_Dialog_InsertBookmark::a_CANCEL);
	}
}

void AP_UnixDialog_InsertBookmark::event_Cancel(void)
{
	setAnswer(AP_Dialog_InsertBookmark::a_CANCEL);
}

void AP_UnixDialog_InsertBookmark::event_Delete(void)
{
	const gchar *mark = XAP_gtk_entry_get_text(GTK_EDITABLE(m_entryBookmark));
	if (mark && *mark)
		setBookmark(mark);
	setAnswer(AP_Dialog_InsertBookmark::a_DELETE);
}

void AP_UnixDialog_InsertBookmark::_setList(void)
{
	std::list<std::string> bookmarks;

	for(UT_sint32 i = 0; i < getExistingBookmarksCount(); i++) {
		bookmarks.push_back(getNthExistingBookmark(i));
	}

	if (bookmarks.size())
	{
		bookmarks.sort();
		std::list<std::string>::iterator iter(bookmarks.begin());
		for( ; iter != bookmarks.end(); ++iter) {
			GtkWidget * label = gtk_label_new(iter->c_str());
			gtk_label_set_xalign(GTK_LABEL(label), 0.0);
			GtkWidget * row = gtk_list_box_row_new();
			gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
			g_object_set_data(G_OBJECT(row), "abi-target-entry",
							  m_entryBookmark);
			gtk_list_box_append(GTK_LIST_BOX(m_listBookmarks), row);
		}
		gtk_widget_set_visible(m_btnBookmarks, TRUE);
	}

	GtkEntry *entry = GTK_ENTRY(m_entryBookmark);
	if (getBookmark() && strlen(getBookmark()) > 0)
	{
	    XAP_gtk_entry_set_text(GTK_EDITABLE(entry), getBookmark());
	}
	else
	{
	    const UT_UCS4String suggestion = getSuggestedBM ();
	    if (suggestion.size()>0)
		{
			UT_UTF8String utf8 (suggestion);
			XAP_gtk_entry_set_text (GTK_EDITABLE(entry), utf8.utf8_str());
		}
	}
}

/* A history row is activated: put its text into the entry and close
 * the popover.  GTK4 has no combo-with-entry; this entry + popover
 * pair is the modern equivalent. */
static void s_bookmark_row_activated(GtkListBox * /*box*/, GtkListBoxRow * row,
									 gpointer /*data*/)
{
	GtkWidget * entry = GTK_WIDGET(
		g_object_get_data(G_OBJECT(row), "abi-target-entry"));
	GtkWidget * label = gtk_list_box_row_get_child(row);
	if (!entry || !label)
		return;
	gtk_editable_set_text(GTK_EDITABLE(entry),
						  gtk_label_get_text(GTK_LABEL(label)));
	gtk_editable_set_position(GTK_EDITABLE(entry), -1);

	GtkWidget * pop = gtk_widget_get_ancestor(GTK_WIDGET(row),
											  GTK_TYPE_POPOVER);
	if (pop)
		gtk_popover_popdown(GTK_POPOVER(pop));
}

void  AP_UnixDialog_InsertBookmark::_constructWindowContents(GtkWidget * container )
{
  GtkWidget *label1;
  const XAP_StringSet * pSS = m_pApp->getStringSet();
  std::string s;
  pSS->getValueUTF8(AP_STRING_ID_DLG_InsertBookmark_Msg,s);
  label1 = gtk_label_new (s.c_str());
  gtk_widget_set_visible(label1, TRUE);
  gtk_box_append(GTK_BOX(container), label1);

  GtkWidget * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand(box, TRUE);

  m_entryBookmark = gtk_entry_new();
  gtk_widget_set_hexpand(m_entryBookmark, TRUE);
  gtk_box_append(GTK_BOX(box), m_entryBookmark);

  GtkWidget * pop = gtk_popover_new();
  m_listBookmarks = gtk_list_box_new();
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(m_listBookmarks),
								  GTK_SELECTION_SINGLE);
  g_signal_connect(m_listBookmarks, "row-activated",
				   G_CALLBACK(s_bookmark_row_activated), nullptr);
  gtk_popover_set_child(GTK_POPOVER(pop), m_listBookmarks);

  m_btnBookmarks = gtk_menu_button_new();
  gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(m_btnBookmarks),
								"pan-down-symbolic");
  gtk_menu_button_set_popover(GTK_MENU_BUTTON(m_btnBookmarks), pop);
  gtk_widget_add_css_class(m_btnBookmarks, "flat");
  gtk_widget_set_visible(m_btnBookmarks, FALSE);
  gtk_box_append(GTK_BOX(box), m_btnBookmarks);

  gtk_widget_set_visible(box, TRUE);
  gtk_box_append(GTK_BOX(container), box);
}

GtkWidget*  AP_UnixDialog_InsertBookmark::_constructWindow(void)
{
  GtkWidget *vbox;

  const XAP_StringSet * pSS = m_pApp->getStringSet();
  std::string s;
  pSS->getValueUTF8(AP_STRING_ID_DLG_InsertBookmark_Title,s);
  
  m_windowMain = abiDialogNew("insert bookmark dialog", TRUE, s.c_str());
  
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_visible(vbox, TRUE);
  xap_gtk_container_add (gtk_dialog_get_content_area(GTK_DIALOG (m_windowMain)), vbox);
  XAP_gtk_widget_set_margin(vbox, 5);

  _constructWindowContents ( vbox );

  pSS->getValueUTF8(XAP_STRING_ID_DLG_Cancel, s);
  abiAddButton(GTK_DIALOG(m_windowMain), s, BUTTON_CANCEL);
  pSS->getValueUTF8(XAP_STRING_ID_DLG_Delete, s);
  abiAddButton(GTK_DIALOG(m_windowMain), s, BUTTON_DELETE);
  pSS->getValueUTF8(AP_STRING_ID_DLG_InsertButton, s);
  m_buttonInsert = abiAddButton(GTK_DIALOG(m_windowMain), s, BUTTON_INSERT);

  gtk_widget_grab_focus (m_entryBookmark);

  return m_windowMain;
}
