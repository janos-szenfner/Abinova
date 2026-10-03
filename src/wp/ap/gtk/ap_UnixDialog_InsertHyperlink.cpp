/* Abinova
 * Copyright (C) 2000 AbiSource, Inc.
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
#include "ap_Dialog_InsertHyperlink.h"
#include "ap_UnixDialog_InsertHyperlink.h"

/*****************************************************************/

XAP_Dialog * AP_UnixDialog_InsertHyperlink::static_constructor(XAP_DialogFactory * pFactory,
													 XAP_Dialog_Id id)
{
	AP_UnixDialog_InsertHyperlink * p = new AP_UnixDialog_InsertHyperlink(pFactory,id);
	return p;
}

AP_UnixDialog_InsertHyperlink::AP_UnixDialog_InsertHyperlink(XAP_DialogFactory * pDlgFactory,
										 XAP_Dialog_Id id)
	: AP_Dialog_InsertHyperlink(pDlgFactory,id),
	m_displayEntry(nullptr),
	m_entry(nullptr),
	m_windowMain(nullptr),
	// m_comboEntry(0),
	m_clist(nullptr),
	m_swindow(nullptr),
	m_titleEntry(nullptr),
	m_iRow(-1)
	
{

}

AP_UnixDialog_InsertHyperlink::~AP_UnixDialog_InsertHyperlink(void)
{
}

/*****************************************************************/

static void s_bookmark_setup(GtkSignalListItemFactory * /*factory*/,
							 GtkListItem *item,
							 gpointer /*data*/)
{
	GtkWidget *label = gtk_label_new(nullptr);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_list_item_set_child(item, label);
}

static void s_bookmark_bind(GtkSignalListItemFactory * /*factory*/,
							GtkListItem *item,
							gpointer /*data*/)
{
	GtkStringObject *strobj =
		GTK_STRING_OBJECT(gtk_list_item_get_item(item));
	gtk_label_set_text(GTK_LABEL(gtk_list_item_get_child(item)),
					   gtk_string_object_get_string(strobj));
}

static void s_blist_clicked(GtkSingleSelection * select,
							GParamSpec * /*pspec*/,
							AP_UnixDialog_InsertHyperlink *me)
{
	guint pos = gtk_single_selection_get_selected(select);
	if (pos != GTK_INVALID_LIST_POSITION) {
		me->setRow(static_cast<gint>(pos));
		XAP_gtk_entry_set_text(GTK_EDITABLE(me->m_entry),
					   me->m_pBookmarks[pos].c_str());
	}
}


/***********************************************************************/
void AP_UnixDialog_InsertHyperlink::runModal(XAP_Frame * pFrame)
{
	UT_ASSERT(pFrame);
	// Build the window's widgets and arrange them
	GtkWidget * mainWindow = _constructWindow();
	UT_ASSERT(mainWindow);

	// select the first row of the list (this must come after the
 	// call to _connectSignals)
// 	gtk_clist_unselect_row(GTK_CLIST(m_clist),0,0);

	switch(abiRunModalDialog(GTK_DIALOG(mainWindow), pFrame, this, BUTTON_CANCEL, false))
	  {
	  case BUTTON_OK:
	    event_OK (); break;
	  default:
	    event_Cancel(); break ;
	  }

	abiDestroyWidget(mainWindow);
}

void AP_UnixDialog_InsertHyperlink::event_OK(void)
{
	UT_ASSERT(m_windowMain);
	// get the bookmark name, if any (return cancel if no name given)
	const gchar * res = XAP_gtk_entry_get_text(GTK_EDITABLE(m_entry));
	const gchar * title = XAP_gtk_entry_get_text(GTK_EDITABLE(m_titleEntry));
	const gchar * disp = XAP_gtk_entry_get_text(GTK_EDITABLE(m_displayEntry));
	if(res && *res)
	{
		setAnswer(AP_Dialog_InsertHyperlink::a_OK);
		setHyperlink(res);
		setHyperlinkTitle(title);
		setDisplayText(disp ? disp : "");
	}
	else
	{
		setAnswer(AP_Dialog_InsertHyperlink::a_CANCEL);
	}
}

void AP_UnixDialog_InsertHyperlink::event_Cancel(void)
{
	setAnswer(AP_Dialog_InsertHyperlink::a_CANCEL);
}

void AP_UnixDialog_InsertHyperlink::_constructWindowContents ( GtkWidget * vbox2 )
{
  const XAP_StringSet * pSS = m_pApp->getStringSet();

  GtkWidget *label1;
  GtkWidget *label2;

  std::string s;
  /* Word's Insert Hyperlink dialog: "Text to display" first */
  pSS->getValueUTF8(AP_STRING_ID_DLG_InsertHyperlink_DisplayLabel, s);
  GtkWidget * label0 = gtk_label_new(s.c_str());
  gtk_label_set_xalign(GTK_LABEL(label0), 0.0);
  gtk_box_append(GTK_BOX(vbox2), label0);
  gtk_widget_set_visible(label0, TRUE);

  m_displayEntry = gtk_entry_new();
  gtk_box_append(GTK_BOX(vbox2), m_displayEntry);
  gtk_widget_set_visible(m_displayEntry, TRUE);
  const gchar * dispText = getDisplayText();
  if (dispText && *dispText)
  {
      XAP_gtk_entry_set_text(GTK_EDITABLE(m_displayEntry), dispText);
  }

  pSS->getValueUTF8(AP_STRING_ID_DLG_InsertHyperlink_AddressLabel, s);
  GtkWidget * labelA = gtk_label_new(s.c_str());
  gtk_label_set_xalign(GTK_LABEL(labelA), 0.0);
  gtk_box_append(GTK_BOX(vbox2), labelA);
  gtk_widget_set_visible(labelA, TRUE);

  pSS->getValueUTF8(AP_STRING_ID_DLG_InsertHyperlink_Msg,s);
  label1 = gtk_label_new (s.c_str());
  gtk_label_set_xalign(GTK_LABEL(label1), 0.0);
  gtk_widget_set_visible(label1, TRUE);
  gtk_box_append(GTK_BOX(vbox2), label1);

  m_entry = gtk_entry_new();
  gtk_box_append(GTK_BOX(vbox2), m_entry);
  gtk_widget_set_visible(m_entry, TRUE);
  
  const gchar * hyperlink = getHyperlink();

  if (hyperlink && *hyperlink)
  {
    if (*hyperlink == '#')
    {
      XAP_gtk_entry_set_text(GTK_EDITABLE(m_entry), hyperlink + 1) ;
    }
    else
    {
      XAP_gtk_entry_set_text(GTK_EDITABLE(m_entry), hyperlink) ;
    }
  }

  // the bookmark list
  m_swindow  = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (m_swindow),GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_visible(m_swindow, TRUE);
  gtk_box_append(GTK_BOX(vbox2), m_swindow);
			gtk_widget_set_hexpand(m_swindow, TRUE);
			gtk_widget_set_vexpand(m_swindow, TRUE);
   
  m_pBookmarks.clear();

  for (int i = 0; i < static_cast<int>(getExistingBookmarksCount()); i++) {
    m_pBookmarks.push_back(getNthExistingBookmark(i));
  }

  std::sort(m_pBookmarks.begin(), m_pBookmarks.end());

  GtkStringList * store = gtk_string_list_new(nullptr);
  for (int i = 0; i < static_cast<int>(getExistingBookmarksCount()); i++) {
		  gtk_string_list_append(store, m_pBookmarks[i].c_str());
  }

  /* gtk_single_selection_new takes over the store reference
   * (model arg is transfer full) */
  GtkSingleSelection *selection =
	  gtk_single_selection_new(G_LIST_MODEL(store));
  gtk_single_selection_set_autoselect(selection, FALSE);
  gtk_single_selection_set_can_unselect(selection, FALSE);

  GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
  g_signal_connect(factory, "setup", G_CALLBACK(s_bookmark_setup), nullptr);
  g_signal_connect(factory, "bind", G_CALLBACK(s_bookmark_bind), nullptr);

  m_clist = gtk_list_view_new(GTK_SELECTION_MODEL(selection), factory);
  gtk_widget_set_visible(m_clist, TRUE);

  xap_gtk_container_add (m_swindow, m_clist);

  pSS->getValueUTF8(AP_STRING_ID_DLG_InsertHyperlink_TitleLabel, s);
  label2 = gtk_label_new(s.c_str());
  gtk_widget_set_visible(label2, TRUE);
  gtk_box_append(GTK_BOX(vbox2), label2);
			gtk_widget_set_hexpand(label2, TRUE);
			gtk_widget_set_vexpand(label2, TRUE);

  m_titleEntry = gtk_entry_new();
  gtk_box_append(GTK_BOX(vbox2), m_titleEntry);
  gtk_widget_set_visible(m_titleEntry, TRUE);

  const gchar * hyperlinkTitle = getHyperlinkTitle();

  if (hyperlinkTitle && *hyperlinkTitle)
  {
      XAP_gtk_entry_set_text(GTK_EDITABLE(m_titleEntry), hyperlinkTitle);
  }
}

GtkWidget*  AP_UnixDialog_InsertHyperlink::_constructWindow(void)
{
  GtkWidget *vbox2;
  GtkWidget *frame1;

  const XAP_StringSet * pSS = m_pApp->getStringSet();

  std::string s;
  pSS->getValueUTF8(AP_STRING_ID_DLG_InsertHyperlink_Title,s);
  m_windowMain = abiDialogNew("insert table dialog", TRUE, s.c_str());

  frame1 = gtk_frame_new (nullptr);
  gtk_widget_set_visible(frame1, TRUE);
  gtk_box_append(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(m_windowMain))), frame1);
			gtk_widget_set_hexpand(frame1, TRUE);
			gtk_widget_set_vexpand(frame1, TRUE);
  XAP_gtk_widget_set_margin(frame1, 4);

  vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_visible(vbox2, TRUE);
  xap_gtk_container_add (frame1, vbox2);
  XAP_gtk_widget_set_margin(vbox2, 5);

  _constructWindowContents ( vbox2 );

  pSS->getValueUTF8(XAP_STRING_ID_DLG_Cancel, s);
  abiAddButton(GTK_DIALOG(m_windowMain), s, BUTTON_CANCEL);
  pSS->getValueUTF8(XAP_STRING_ID_DLG_OK, s);
  abiAddButton(GTK_DIALOG(m_windowMain), s, BUTTON_OK);

  gtk_widget_grab_focus (m_entry);

  // connect all the signals
  _connectSignals ();

  return m_windowMain;
}

void AP_UnixDialog_InsertHyperlink::_connectSignals (void)
{
	GtkSelectionModel *select = gtk_list_view_get_model(GTK_LIST_VIEW(m_clist));
	g_signal_connect (G_OBJECT(select), "notify::selected-item",
					  G_CALLBACK (s_blist_clicked), this);
}
