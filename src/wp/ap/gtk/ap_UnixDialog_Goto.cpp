/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* Abinova
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) Robert Staudinger <robsta@stereolyzer.net>
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
#include <string.h>

#include <gtk/gtk.h>

#include "ut_string.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "xap_UnixDialogHelper.h"
#include "xap_GtkSignalBlocker.h"

#include "xap_Dialog_Id.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"
#include "ap_Dialog_Goto.h"
#include "ap_UnixDialog_Goto.h"
#include "xap_GtkListHelpers.h"

#include "GTKCommon.h"


static void AP_UnixDialog_Goto__onSwitchPage (GtkNotebook *notebook,
											  gpointer Page,
											  guint page,
											  gpointer data)
{
	UT_UNUSED(notebook);
	UT_UNUSED(Page);
	UT_DEBUGMSG(("_onSwitchPage() '%d'\n", page));

	if (page == 0)
	{
		AP_UnixDialog_Goto *dlg = static_cast<AP_UnixDialog_Goto *>(data);
		dlg->updatePosition();
	}
}

/*!
* Event dispatcher for spinbutton "page".
*/
void
AP_UnixDialog_Goto__onFocusPage (GtkEventControllerFocus * /*controller*/,
									 gpointer 		  data)
{
	AP_UnixDialog_Goto *dlg = static_cast <AP_UnixDialog_Goto *>(data);
	dlg->updateCache (AP_JUMPTARGET_PAGE);
}

/*!
* Event dispatcher for spinbutton "line".
*/
void
AP_UnixDialog_Goto__onFocusLine (GtkEventControllerFocus * /*controller*/,
									 gpointer 		  data)
{
	AP_UnixDialog_Goto *dlg = static_cast <AP_UnixDialog_Goto *>(data);
	dlg->updateCache (AP_JUMPTARGET_LINE);
}

/*!
* Event dispatcher for treeview "bookmarks".
*/
void
AP_UnixDialog_Goto__onFocusBookmarks (GtkEventControllerFocus * /*controller*/,
									 gpointer 		  data)
{
	AP_UnixDialog_Goto *dlg = static_cast <AP_UnixDialog_Goto *>(data);
	dlg->updateCache (AP_JUMPTARGET_BOOKMARK);
}
void
AP_UnixDialog_Goto__onFocusXMLIDs (GtkEventControllerFocus * /*controller*/,
									 gpointer 		  data)
{
	AP_UnixDialog_Goto *dlg = static_cast <AP_UnixDialog_Goto *>(data);
	dlg->updateCache (AP_JUMPTARGET_XMLID);
}
void
AP_UnixDialog_Goto__onFocusAnno (GtkEventControllerFocus * /*controller*/,
									 gpointer 		  data)
{
	AP_UnixDialog_Goto *dlg = static_cast <AP_UnixDialog_Goto *>(data);
	dlg->updateCache (AP_JUMPTARGET_ANNOTATION);
}

/*!
* Event dispatcher for spinbutton "page".
*/
void 
AP_UnixDialog_Goto__onPageChanged (GtkSpinButton * /*spinbutton*/,
								   gpointer 	  data)
{
	AP_UnixDialog_Goto *dlg = static_cast <AP_UnixDialog_Goto *>(data);
	dlg->onPageChanged ();
}

/*!
* Event dispatcher for spinbutton "line".
*/
void 
AP_UnixDialog_Goto__onLineChanged (GtkSpinButton * /*spinbutton*/,
								   gpointer 	  data)
{
	AP_UnixDialog_Goto *dlg = static_cast <AP_UnixDialog_Goto *>(data);
	dlg->onLineChanged ();
}

/*!
* Event dispatcher for listview "bookmarks".
*/
void
AP_UnixDialog_Goto__onBookmarkDblClicked (GtkListView       * /*view*/,
										  guint               /*position*/,
										  gpointer		    data)
{
	AP_UnixDialog_Goto *dlg = static_cast <AP_UnixDialog_Goto *>(data);
	dlg->onBookmarkDblClicked ();
}
void
AP_UnixDialog_Goto__onXMLIDDblClicked (GtkListView       * /*view*/,
                                       guint               /*position*/,
                                       gpointer		    data)
{
	AP_UnixDialog_Goto *dlg = static_cast <AP_UnixDialog_Goto *>(data);
	dlg->onXMLIDDblClicked ();
}
void
AP_UnixDialog_Goto__onAnnoDblClicked (GtkColumnView     * /*view*/,
                                      guint               /*position*/,
                                      gpointer		    data)
{
	AP_UnixDialog_Goto *dlg = static_cast <AP_UnixDialog_Goto *>(data);
	dlg->onAnnoDblClicked ();
}

/*!
* Event dispatcher for button "jump".
*/
void
AP_UnixDialog_Goto__onJumpClicked (GtkButton * /*button*/,
								   gpointer   data)
{
	AP_UnixDialog_Goto *dlg = static_cast <AP_UnixDialog_Goto *>(data);
	dlg->onJumpClicked ();
}

/*!
* Event dispatcher for button "prev".
*/
void
AP_UnixDialog_Goto__onPrevClicked (GtkButton * /*button*/,
								   gpointer   data)
{
	AP_UnixDialog_Goto *dlg = static_cast <AP_UnixDialog_Goto *>(data);
	dlg->onPrevClicked ();
}

/*!
* Event dispatcher for button "next".
*/
void
AP_UnixDialog_Goto__onNextClicked (GtkButton * /*button*/,
								   gpointer   data)
{
	AP_UnixDialog_Goto *dlg = static_cast <AP_UnixDialog_Goto *>(data);
	dlg->onNextClicked ();
}

/*!
* Event dispatcher for button "close".
*/
void
AP_UnixDialog_Goto__onDialogResponse (GtkDialog * /*dialog*/,
									  gint 		response,
									  gpointer  data)
{
	AP_UnixDialog_Goto *dlg = static_cast <AP_UnixDialog_Goto *>(data);
	if (response == GTK_RESPONSE_CLOSE) {
		dlg->destroy ();		
	}
}

/*!
* Event dispatcher for window.
*/
gboolean
AP_UnixDialog_Goto__onDeleteWindow (GtkWidget * /*widget*/,
									gpointer  data)
{
	AP_UnixDialog_Goto *dlg = static_cast <AP_UnixDialog_Goto *>(data);
	if (dlg->getWindow ()) {
		dlg->destroy ();
	}
	return TRUE;
}



/*!
* Static ctor.
*/
XAP_Dialog * 
AP_UnixDialog_Goto::static_constructor(XAP_DialogFactory *pFactory,
									   XAP_Dialog_Id 	 id)
{
	AP_UnixDialog_Goto *dlg = new AP_UnixDialog_Goto (pFactory, id);
	return dlg;
}

/*!
* Ctor.
*/
AP_UnixDialog_Goto::AP_UnixDialog_Goto(XAP_DialogFactory *pDlgFactory,
									   XAP_Dialog_Id 	 id)
	: AP_Dialog_Goto(pDlgFactory, id),
	  m_wDialog(nullptr),
	  m_nbNotebook(nullptr),
	  m_lbPage(nullptr),
	  m_lbLine(nullptr),
	  m_lbBookmarks(nullptr),
	  m_lbXMLids(nullptr),
	  m_lbAnnotations(nullptr),
	  m_sbPage(nullptr),
	  m_sbLine(nullptr),
	  m_lvBookmarks(nullptr),
	  m_btJump(nullptr),
	  m_btPrev(nullptr),
	  m_btNext(nullptr),
	  m_lvXMLIDs(nullptr),
	  m_lvAnno(nullptr),
	  m_btClose(nullptr),
	  m_selBookmarks(nullptr),
	  m_selXMLIDs(nullptr),
	  m_selAnno(nullptr),
	  m_storeAnno(nullptr),
	  m_iPageConnect(0),
	  m_iLineConnect(0),
	  m_JumpTarget(AP_JUMPTARGET_BOOKMARK)
{
}

/*!
* Dtor.
*/
AP_UnixDialog_Goto::~AP_UnixDialog_Goto ()
{
	UT_DEBUGMSG (("ROB: ~AP_UnixDialog_Goto ()\n"));
}



/*!
* Event handler for spinbutton "page".
*/
void 
AP_UnixDialog_Goto::onPageChanged () 
{
	UT_DEBUGMSG (("ROB: onPageChanged () maxpage='%d'\n", m_DocCount.page));
	m_JumpTarget = AP_JUMPTARGET_PAGE;
	UT_uint32 page = static_cast<UT_uint32>(gtk_spin_button_get_value (GTK_SPIN_BUTTON (m_sbPage)));
	if (page > m_DocCount.page) {
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (m_sbPage), 1);
	}
	onJumpClicked();
}

/*!
* Event handler for spinbutton "line".
*/
void 
AP_UnixDialog_Goto::onLineChanged () 
{
	UT_DEBUGMSG (("ROB: onLineChanged () maxline='%d'\n", m_DocCount.line));
	m_JumpTarget = AP_JUMPTARGET_LINE;
	UT_uint32 line = static_cast<UT_uint32>(gtk_spin_button_get_value (GTK_SPIN_BUTTON (m_sbLine)));
	if (line > m_DocCount.line) {
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (m_sbLine), 1);
	}
	if (line == 0) {
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (m_sbLine), m_DocCount.line);
	}
	onJumpClicked();
}

/*!
* Event handler for treeview "bookmarks".
*/
void
AP_UnixDialog_Goto::onBookmarkDblClicked ()
{
	m_JumpTarget = AP_JUMPTARGET_BOOKMARK;
	onJumpClicked();
}
void
AP_UnixDialog_Goto::onXMLIDDblClicked ()
{
	m_JumpTarget = AP_JUMPTARGET_XMLID;
	onJumpClicked();
}
void
AP_UnixDialog_Goto::onAnnoDblClicked ()
{
	m_JumpTarget = AP_JUMPTARGET_ANNOTATION;
	onJumpClicked();
}

/*!
* Event handler for button "jump".
*/
void 
AP_UnixDialog_Goto::onJumpClicked () 
{
    std::string text;
    XAP_GtkSignalBlocker b(G_OBJECT(m_sbLine), m_iLineConnect);

	switch (m_JumpTarget) {
		case AP_JUMPTARGET_PAGE:
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_sbLine), 0);
			text = tostr(GTK_EDITABLE (m_sbPage));
			break;
		case AP_JUMPTARGET_LINE:
			text = tostr(GTK_EDITABLE (m_sbLine));
			if (text == "0") return;
			break;
		case AP_JUMPTARGET_BOOKMARK:
			text = _getSelectedBookmarkLabel ();
			break;
		case AP_JUMPTARGET_XMLID:
			text = _getSelectedXMLIDLabel();
			break;
		case AP_JUMPTARGET_ANNOTATION:
			text = _getSelectedAnnotationLabel();
			break;
		default:
			UT_DEBUGMSG (("AP_UnixDialog_Goto::onJumpClicked () no jump target\n"));
			return;
			// UT_ASSERT_NOT_REACHED ();
	}

	if (text.empty())
		return;		

	performGoto(m_JumpTarget, text.c_str());
}

/*!
* Event handler for button "prev".
*/
void 
AP_UnixDialog_Goto::onPrevClicked () 
{
	UT_DEBUGMSG (("ROB: onPrevClicked () '%d'\n", m_JumpTarget));

	UT_uint32 num = 0;
	switch (m_JumpTarget) {
		case AP_JUMPTARGET_PAGE:
			num = static_cast<UT_uint32>(gtk_spin_button_get_value (GTK_SPIN_BUTTON (m_sbPage)));
			if (num == 1)
				num = m_DocCount.page;
			else
				num--;
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (m_sbPage), num);
			break;
		case AP_JUMPTARGET_LINE:
			num = static_cast<UT_uint32>(gtk_spin_button_get_value (GTK_SPIN_BUTTON (m_sbLine)));
			if (num == 1)
				num = m_DocCount.line;
			else
				num--;
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (m_sbLine), num);
			break;
		case AP_JUMPTARGET_BOOKMARK:
			_selectPrevBookmark ();
			break;
		case AP_JUMPTARGET_XMLID:
            XAP_single_selection_select_prev(m_selXMLIDs);
			break;
		case AP_JUMPTARGET_ANNOTATION:
            XAP_single_selection_select_prev(m_selAnno);
			break;
		default:
			UT_DEBUGMSG (("ROB: AP_UnixDialog_Goto::onPrevClicked () no jump target\n"));
			return;
			// UT_ASSERT_NOT_REACHED ();
	}

	onJumpClicked ();
}

/*!
* Event handler for button "next".
*/
void 
AP_UnixDialog_Goto::onNextClicked () 
{
	UT_DEBUGMSG (("ROB: onNextClicked () '%d'\n", m_JumpTarget));

	UT_uint32 num = 0;
	switch (m_JumpTarget) {
		case AP_JUMPTARGET_PAGE:
			num = static_cast<UT_uint32>(gtk_spin_button_get_value (GTK_SPIN_BUTTON (m_sbPage)));
			num++;
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (m_sbPage), num);
			break;
		case AP_JUMPTARGET_LINE:
			num = static_cast<UT_uint32>(gtk_spin_button_get_value (GTK_SPIN_BUTTON (m_sbLine)));
			num++;
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (m_sbLine), num);
			break;
		case AP_JUMPTARGET_BOOKMARK:
			_selectNextBookmark ();
			break;
		case AP_JUMPTARGET_XMLID:
            XAP_single_selection_select_next(m_selXMLIDs);
			break;
		case AP_JUMPTARGET_ANNOTATION:
            XAP_single_selection_select_next(m_selAnno);
			break;
		default:
			UT_DEBUGMSG (("ROB: AP_UnixDialog_Goto::onNextClicked () no jump target\n"));
			return;
			// UT_ASSERT_NOT_REACHED ();
	}

	onJumpClicked ();
}

/*!
* Set jump target and update cached data like number of lines and pages.
* @see ap_types.h
*/
void 
AP_UnixDialog_Goto::updateCache (AP_JumpTarget target) 
{
	m_JumpTarget = target;
	updateDocCount ();
}

/*!
* Update cached data like number of lines and pages.
*/
void
AP_UnixDialog_Goto::updateDocCount ()
{
	m_DocCount = getView()->countWords(false); 
	UT_DEBUGMSG (("ROB: updateCache () page='%d' line='%d'\n", m_DocCount.page, m_DocCount.line));
}

void AP_UnixDialog_Goto::updatePosition (void)
{
	// the notebook can emit "switch-page" while the dialog is being
	// destroyed; the spin buttons may already be finalized by then.
	if (!m_sbPage || !m_sbLine)
		return;
	// pages, page increment of 10 is pretty arbitrary (set in the GtkBuilder UI file)
	UT_uint32 currentPage = getView()->getCurrentPageNumForStatusBar ();
	XAP_GtkSignalBlocker b1(G_OBJECT(m_sbPage), m_iPageConnect);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (m_sbPage), currentPage);

	// lines, line increment of 10 is pretty arbitrary (set in the GtkBuilder UI file)
	UT_uint32 currentLine = 0; /* FIXME get current line */
	XAP_GtkSignalBlocker b2(G_OBJECT(m_sbLine), m_iLineConnect);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (m_sbLine), currentLine);
}

void
AP_UnixDialog_Goto::setupXMLIDList( GtkWidget* w )
{
	GListStore *store = XAP_list_store_new();
	m_selXMLIDs =
		XAP_list_view_set_model(GTK_LIST_VIEW(w), store);
	g_object_unref(store);

	{
		GtkEventController *foc = gtk_event_controller_focus_new();
		g_signal_connect (foc, "enter",
						  G_CALLBACK (AP_UnixDialog_Goto__onFocusXMLIDs), static_cast <gpointer>(this));
		gtk_widget_add_controller (w, foc);
	}
	g_signal_connect (w, "activate",
					  G_CALLBACK (AP_UnixDialog_Goto__onXMLIDDblClicked), static_cast <gpointer>(this));
}


/* Annotation column cells — XAPDropDownItem rows carry the annotation
 * index in int, the title in string1 and the author in string2. */

static void s_anno_label_setup(GtkSignalListItemFactory * /*factory*/,
							   GtkListItem * item, gpointer /*data*/)
{
	GtkWidget *label = gtk_label_new(nullptr);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_list_item_set_child(item, label);
}

static void s_anno_bind_id(GtkSignalListItemFactory * /*factory*/,
						   GtkListItem * item, gpointer /*data*/)
{
	GtkLabel *label = GTK_LABEL(gtk_list_item_get_child(item));
	gpointer row = gtk_list_item_get_item(item);
	char buf[16];
	if (row)
		g_snprintf(buf, sizeof(buf), "%d",
				   xap_drop_down_item_get_int(XAP_DROP_DOWN_ITEM(row)));
	gtk_label_set_text(label, row ? buf : nullptr);
}

static void s_anno_bind_title(GtkSignalListItemFactory * /*factory*/,
							  GtkListItem * item, gpointer /*data*/)
{
	GtkLabel *label = GTK_LABEL(gtk_list_item_get_child(item));
	gpointer row = gtk_list_item_get_item(item);
	gtk_label_set_text(label,
					   row ? xap_drop_down_item_get_string1(
						   XAP_DROP_DOWN_ITEM(row)) : nullptr);
}

static void s_anno_bind_author(GtkSignalListItemFactory * /*factory*/,
							   GtkListItem * item, gpointer /*data*/)
{
	GtkLabel *label = GTK_LABEL(gtk_list_item_get_child(item));
	gpointer row = gtk_list_item_get_item(item);
	gtk_label_set_text(label,
					   row ? xap_drop_down_item_get_string2(
						   XAP_DROP_DOWN_ITEM(row)) : nullptr);
}

static GtkListItemFactory * s_anno_factory(GCallback bind)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(s_anno_label_setup),
					 nullptr);
	g_signal_connect(factory, "bind", bind, nullptr);
	return factory;
}

static int s_anno_sort_id(gconstpointer a, gconstpointer b,
						  gpointer /*data*/)
{
	int x = xap_drop_down_item_get_int(
		XAP_DROP_DOWN_ITEM(const_cast<gpointer>(a)));
	int y = xap_drop_down_item_get_int(
		XAP_DROP_DOWN_ITEM(const_cast<gpointer>(b)));
	return (x > y) - (x < y);
}

static int s_anno_sort_title(gconstpointer a, gconstpointer b,
							 gpointer /*data*/)
{
	return g_strcmp0(
		xap_drop_down_item_get_string1(
			XAP_DROP_DOWN_ITEM(const_cast<gpointer>(a))),
		xap_drop_down_item_get_string1(
			XAP_DROP_DOWN_ITEM(const_cast<gpointer>(b))));
}

static int s_anno_sort_author(gconstpointer a, gconstpointer b,
							  gpointer /*data*/)
{
	return g_strcmp0(
		xap_drop_down_item_get_string2(
			XAP_DROP_DOWN_ITEM(const_cast<gpointer>(a))),
		xap_drop_down_item_get_string2(
			XAP_DROP_DOWN_ITEM(const_cast<gpointer>(b))));
}

static void s_anno_append_column(GtkColumnView * view, const char * title,
								 GCallback bind,
								 GtkSorter * sorter)
{
	GtkColumnViewColumn *col =
		gtk_column_view_column_new(title, s_anno_factory(bind));
	if (sorter) {
		gtk_column_view_column_set_sorter(col, sorter);
		g_object_unref(sorter);
	}
	gtk_column_view_append_column(view, col);
	g_object_unref(col);
}

void
AP_UnixDialog_Goto::setupAnnotationList( GtkWidget* w )
{
	GtkColumnView *view = GTK_COLUMN_VIEW(w);

	// localization
	const XAP_StringSet * pSS = m_pApp->getStringSet ();
	std::string id, title, author;
	pSS->getValueUTF8(AP_STRING_ID_DLG_Goto_Column_ID, id);
	pSS->getValueUTF8(AP_STRING_ID_DLG_Goto_Column_Title, title);
	pSS->getValueUTF8(AP_STRING_ID_DLG_Goto_Column_Author, author);

	m_storeAnno = XAP_list_store_new();
	GtkSortListModel *sort =
		gtk_sort_list_model_new(G_LIST_MODEL(g_object_ref(m_storeAnno)),
								nullptr);
	GtkSingleSelection *sel =
		gtk_single_selection_new(G_LIST_MODEL(sort));
	gtk_single_selection_set_autoselect(sel, FALSE);
	gtk_single_selection_set_can_unselect(sel, TRUE);
	gtk_column_view_set_model(view, GTK_SELECTION_MODEL(sel));
	g_object_unref(sel);
	m_selAnno = GTK_SINGLE_SELECTION(gtk_column_view_get_model(view));

	s_anno_append_column(view, id.c_str(),
						 G_CALLBACK(s_anno_bind_id),
						 GTK_SORTER(gtk_custom_sorter_new(s_anno_sort_id,
														nullptr, nullptr)));
	s_anno_append_column(view, title.c_str(),
						 G_CALLBACK(s_anno_bind_title),
						 GTK_SORTER(gtk_custom_sorter_new(s_anno_sort_title,
														nullptr, nullptr)));
	s_anno_append_column(view, author.c_str(),
						 G_CALLBACK(s_anno_bind_author),
						 GTK_SORTER(gtk_custom_sorter_new(s_anno_sort_author,
														nullptr, nullptr)));

	// clicking a column header sorts on that column's sorter
	gtk_sort_list_model_set_sorter(sort,
								   gtk_column_view_get_sorter(view));

	{
		GtkEventController *foc = gtk_event_controller_focus_new();
		g_signal_connect (foc, "enter",
						  G_CALLBACK (AP_UnixDialog_Goto__onFocusAnno), static_cast <gpointer>(this));
		gtk_widget_add_controller (w, foc);
	}
	g_signal_connect (w, "activate",
					  G_CALLBACK (AP_UnixDialog_Goto__onAnnoDblClicked), static_cast <gpointer>(this));
}


/*!
* Build dialog.
*/
void 
AP_UnixDialog_Goto::_constructWindow (XAP_Frame * /*pFrame*/) 
{
	UT_DEBUGMSG (("ROB: _constructWindow ()\n"));		

	// load the dialog from the UI file
	GtkBuilder* builder = newDialogBuilderFromResource("ap_UnixDialog_Goto.ui");

	m_wDialog = GTK_WIDGET(gtk_builder_get_object(builder, "ap_UnixDialog_Goto"));
	m_nbNotebook = GTK_WIDGET(gtk_builder_get_object(builder, "nbNotebook"));
	m_lbPage = GTK_WIDGET(gtk_builder_get_object(builder, "lbPage"));
	m_lbLine = GTK_WIDGET(gtk_builder_get_object(builder, "lbLine"));
	m_lbBookmarks = GTK_WIDGET(gtk_builder_get_object(builder, "lbBookmarks"));
	m_lbXMLids = GTK_WIDGET(gtk_builder_get_object(builder, "lbXMLids"));
	m_lbAnnotations = GTK_WIDGET(gtk_builder_get_object(builder, "lbAnnotations"));
	m_sbPage = GTK_WIDGET(gtk_builder_get_object(builder, "sbPage"));
	m_sbLine = GTK_WIDGET(gtk_builder_get_object(builder, "sbLine"));
	m_lvBookmarks = GTK_WIDGET(gtk_builder_get_object(builder, "lvBookmarks"));
	m_btJump = GTK_WIDGET(gtk_builder_get_object(builder, "btJump"));
	m_btPrev = GTK_WIDGET(gtk_builder_get_object(builder, "btPrev"));
	m_btNext = GTK_WIDGET(gtk_builder_get_object(builder, "btNext"));
	m_lvXMLIDs = GTK_WIDGET(gtk_builder_get_object(builder, "lvXMLIDs"));
	m_lvAnno   = GTK_WIDGET(gtk_builder_get_object(builder, "lvAnno"));
	m_btClose = GTK_WIDGET(gtk_builder_get_object(builder, "btClose"));


	// localise	
	const XAP_StringSet * pSS = m_pApp->getStringSet();
	localizeLabel(GTK_WIDGET(gtk_builder_get_object(builder, "lbPosition")), pSS, AP_STRING_ID_DLG_Goto_Label_Position);
	/* FIXME jump targets localised in xp land, make sure they work for non ascii characters */
	const gchar **targets = getJumpTargets ();
	const gchar *text = nullptr;
	if ((text = targets[AP_JUMPTARGET_PAGE]) != nullptr)
		gtk_label_set_text (GTK_LABEL (m_lbPage), text);
	if ((text = targets[AP_JUMPTARGET_LINE]) != nullptr)
		gtk_label_set_text (GTK_LABEL (m_lbLine), text);
	if ((text = targets[AP_JUMPTARGET_BOOKMARK]) != nullptr)
		gtk_label_set_text (GTK_LABEL (m_lbBookmarks), text);
	if ((text = targets[AP_JUMPTARGET_XMLID]) != nullptr)
		gtk_label_set_text (GTK_LABEL (m_lbXMLids), text);
	if ((text = targets[AP_JUMPTARGET_ANNOTATION]) != nullptr)
		gtk_label_set_text (GTK_LABEL (m_lbAnnotations), text);


    setupXMLIDList( m_lvXMLIDs );
    setupAnnotationList( m_lvAnno );

	// bookmarks ListView
	GListStore *bmStore = XAP_list_store_new();
	m_selBookmarks =
		XAP_list_view_set_model(GTK_LIST_VIEW(m_lvBookmarks), bmStore);
	g_object_unref(bmStore);

	// Signals
	g_signal_connect (GTK_NOTEBOOK (m_nbNotebook), "switch-page", 
					  G_CALLBACK (AP_UnixDialog_Goto__onSwitchPage), static_cast <gpointer>(this)); 
	{
		GtkEventController *foc = gtk_event_controller_focus_new();
		g_signal_connect (foc, "enter",
						  G_CALLBACK (AP_UnixDialog_Goto__onFocusPage), static_cast <gpointer>(this));
		gtk_widget_add_controller (m_sbPage, foc);
	} 
	m_iPageConnect = g_signal_connect (GTK_SPIN_BUTTON (m_sbPage), "value-changed",
					  G_CALLBACK (AP_UnixDialog_Goto__onPageChanged), static_cast <gpointer>(this)); 

	{
		GtkEventController *foc = gtk_event_controller_focus_new();
		g_signal_connect (foc, "enter",
						  G_CALLBACK (AP_UnixDialog_Goto__onFocusLine), static_cast <gpointer>(this));
		gtk_widget_add_controller (m_sbLine, foc);
	} 
	m_iLineConnect = g_signal_connect (GTK_SPIN_BUTTON (m_sbLine), "value-changed",
					  G_CALLBACK (AP_UnixDialog_Goto__onLineChanged), static_cast <gpointer>(this)); 

	{
		GtkEventController *foc = gtk_event_controller_focus_new();
		g_signal_connect (foc, "enter",
						  G_CALLBACK (AP_UnixDialog_Goto__onFocusBookmarks), static_cast <gpointer>(this));
		gtk_widget_add_controller (m_lvBookmarks, foc);
	} 
	g_signal_connect (m_lvBookmarks, "activate",
					  G_CALLBACK (AP_UnixDialog_Goto__onBookmarkDblClicked), static_cast <gpointer>(this));

	g_signal_connect (GTK_BUTTON (m_btJump), "clicked", 
					  G_CALLBACK (AP_UnixDialog_Goto__onJumpClicked), static_cast <gpointer>(this));
	g_signal_connect (GTK_BUTTON (m_btPrev), "clicked", 
					  G_CALLBACK (AP_UnixDialog_Goto__onPrevClicked), static_cast <gpointer>(this));
	g_signal_connect (GTK_BUTTON (m_btNext), "clicked", 
					  G_CALLBACK (AP_UnixDialog_Goto__onNextClicked), static_cast <gpointer>(this));

	g_signal_connect (GTK_DIALOG (m_wDialog), "response",
					  G_CALLBACK (AP_UnixDialog_Goto__onDialogResponse), static_cast <gpointer>(this));
	g_signal_connect (m_wDialog, "close-request",
					  G_CALLBACK (AP_UnixDialog_Goto__onDeleteWindow), static_cast <gpointer>(this));

	g_object_unref(G_OBJECT(builder));
}

/*!
* Update dialog's data.
*/
void 
AP_UnixDialog_Goto::_updateWindow ()
{
	UT_DEBUGMSG (("ROB: _updateWindow () #bookmarks='%d', mapped='%d'\n", getExistingBookmarksCount(), gtk_widget_get_mapped(m_wDialog)));

	ConstructWindowName ();
	gtk_window_set_title (GTK_WINDOW (m_wDialog), m_WindowName);

	// position: pages and lines
	updatePosition();

	// bookmarks
	GListStore *bmModel = G_LIST_STORE(
		gtk_single_selection_get_model(m_selBookmarks));
	g_list_store_remove_all(bmModel);

	UT_uint32 numBookmarks = getExistingBookmarksCount();
	for (UT_uint32 i = 0; i < numBookmarks; i++) {
		const std::string & name = getNthExistingBookmark(i);
		UT_DEBUGMSG (("    ROB: '%s'\n", name.c_str()));
		XAP_list_store_append_text(bmModel, name.c_str());
	}

    updateXMLIDList();
    updateAnnotationList();
	updateDocCount ();
}


void
AP_UnixDialog_Goto::updateXMLIDList( void )
{
	GListStore *model = G_LIST_STORE(
		gtk_single_selection_get_model(m_selXMLIDs));
	g_list_store_remove_all(model);

    if( PD_DocumentRDFHandle rdf = getRDF() )
    {
        std::set< std::string > xmlids;
        rdf->getAllIDs( xmlids );
        UT_DEBUGMSG (("MIQ: xmlids.sz:%lu\n", static_cast<long unsigned>(xmlids.size() )));

        for( std::set< std::string >::iterator xiter = xmlids.begin();
             xiter != xmlids.end(); ++xiter )
        {
            std::string name = *xiter;
            UT_DEBUGMSG (("    MIQ: '%s'\n", name.c_str()));
            XAP_list_store_append_text(model, name.c_str());
        }
    }
}


void
AP_UnixDialog_Goto::updateAnnotationList( void )
{
	g_list_store_remove_all(m_storeAnno);

    FV_View* pView = getView();
	UT_uint32 max = pView->countAnnotations();
    for( UT_uint32 i=0; i<max; ++i )
    {
        std::string name   = tostr(i);
        std::string title  = pView->getAnnotationTitle(i);
        std::string author = pView->getAnnotationAuthor(i);

        UT_DEBUGMSG (("    MIQ: '%s'\n", name.c_str()));
        XAPDropDownItem *item =
			xap_drop_down_item_new(name.c_str(), i,
								   title.c_str(), author.c_str());
        g_list_store_append(m_storeAnno, item);
        g_object_unref(item);
    }
}



void 
AP_UnixDialog_Goto::runModeless (XAP_Frame * pFrame)
{
	UT_DEBUGMSG (("ROB: runModeless ()\n"));
	_constructWindow (pFrame);
	UT_ASSERT (m_wDialog);
	_updateWindow ();
	abiSetupModelessDialog (GTK_DIALOG (m_wDialog), pFrame, this, GTK_RESPONSE_CLOSE);
	gtk_widget_set_visible(m_wDialog, TRUE);
	gtk_window_present (GTK_WINDOW (m_wDialog));
}

void 
AP_UnixDialog_Goto::notifyActiveFrame (XAP_Frame * /*pFrame*/)
{
	UT_DEBUGMSG (("ROB: notifyActiveFrame ()\n"));
	UT_ASSERT (m_wDialog);

	_updateWindow ();
	/* default to page */
	m_JumpTarget = AP_JUMPTARGET_PAGE;
}

void 
AP_UnixDialog_Goto::activate (void)
{
	UT_ASSERT (m_wDialog);
	UT_DEBUGMSG (("ROB: AP_UnixDialog_Goto::activate ()\n"));
	_updateWindow ();
	gtk_window_present (GTK_WINDOW (m_wDialog));
}

void 
AP_UnixDialog_Goto::destroy ()
{
	UT_DEBUGMSG (("ROB: AP_UnixDialog_Goto::destroy ()\n"));
	modeless_cleanup ();
	if (m_wDialog) {
		// widgets are finalized with the window; clear them first so
		// callbacks emitted during teardown don't touch dead objects
		m_sbPage = nullptr;
		m_sbLine = nullptr;
		m_selBookmarks = nullptr;
		m_selXMLIDs = nullptr;
		m_selAnno = nullptr;
		m_storeAnno = nullptr;
		abiDestroyWidget(m_wDialog); // TOPLEVEL
		m_wDialog = nullptr;
	}
}

/**
* Try to select the bookmark before the current one.
* If none is selected the last in the list is picked.
* Wraps around.
*/
void
AP_UnixDialog_Goto::_selectPrevBookmark () 
{
	UT_DEBUGMSG (("ROB: AP_UnixDialog_Goto::_selectPrevBookmark ()\n"));
    XAP_single_selection_select_prev(m_selBookmarks);
}

/**
* Try to select the bookmark after the current one.
* If none is selected the first in the list is picked.
* Wraps around.
*/
void
AP_UnixDialog_Goto::_selectNextBookmark () 
{
	UT_DEBUGMSG (("ROB: AP_UnixDialog_Goto::_selectNextBookmark ()\n"));
    XAP_single_selection_select_next(m_selBookmarks);
}


/*!
* Get the label of the currently selected bookmark in the list.
*/
std::string
AP_UnixDialog_Goto::_getSelectedBookmarkLabel () 
{
	UT_DEBUGMSG (("ROB: AP_UnixDialog_Goto::_getSelectedBookmarkLabel ()\n"));
    std::string ret;
    const char *text = XAP_single_selection_get_text(m_selBookmarks);
    if (text) {
        ret = text;
    }
	return ret;
}

std::string
AP_UnixDialog_Goto::_getSelectedXMLIDLabel()
{
	UT_DEBUGMSG (("MIQ: AP_UnixDialog_Goto::_getSelectedXMLIDLabel ()\n"));
    std::string ret;
    const char *text = XAP_single_selection_get_text(m_selXMLIDs);
    if (text) {
        ret = text;
    }
	return ret;
}

std::string
AP_UnixDialog_Goto::_getSelectedAnnotationLabel()
{
	UT_DEBUGMSG (("MIQ: AP_UnixDialog_Goto::_getSelectedAnnotationLabel ()\n"));
    std::string ret;
    int id = XAP_single_selection_get_int(m_selAnno);
    if (id >= 0) {
        ret = tostr(id);
    }
	return ret;
}

