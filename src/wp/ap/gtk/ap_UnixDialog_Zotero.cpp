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

#include <gtk/gtk.h>

#include "ut_assert.h"
#include "ut_debugmsg.h"

#include "xap_UnixDialogHelper.h"
#include "xap_GtkUtils.h"

#include "xap_App.h"
#include "xap_Frame.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"
#include "ap_UnixDialog_Zotero.h"

/*****************************************************************/

XAP_Dialog * AP_UnixDialog_Zotero::static_constructor(
	XAP_DialogFactory * pFactory, XAP_Dialog_Id id)
{
	return new AP_UnixDialog_Zotero(pFactory, id);
}

AP_UnixDialog_Zotero::AP_UnixDialog_Zotero(
	XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id)
	: AP_Dialog_Zotero(pDlgFactory, id)
	, m_entry(nullptr)
	, m_styleCombo(nullptr)
	, m_listBox(nullptr)
	, m_status(nullptr)
	, m_insertBtn(nullptr)
	, m_insertBibBtn(nullptr)
{
}

AP_UnixDialog_Zotero::~AP_UnixDialog_Zotero(void)
{
}

void AP_UnixDialog_Zotero::runModal(XAP_Frame * pFrame)
{
	GtkWidget * mainWindow = constructWindow();
	UT_return_if_fail(mainWindow);

	gint rc = abiRunModalDialog(GTK_DIALOG(mainWindow),
							  pFrame, this, BUTTON_CLOSE, false);
	UT_DEBUGMSG(("AP_UnixDialog_Zotero::runModal() rc:%d\n", rc));
	UT_UNUSED(rc);

	m_answer = AP_Dialog_Zotero::a_CANCEL;
	abiDestroyWidget(mainWindow);
}

GtkWidget * AP_UnixDialog_Zotero::constructWindow(void)
{
	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
	std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_Zotero_Title, s);
	GtkWidget * dlg = abiDialogNew("zotero", TRUE, s.c_str());
	GtkWidget * content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));

	GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_visible(vbox, TRUE);
	gtk_box_append(GTK_BOX(content), vbox);
	gtk_widget_set_hexpand(vbox, TRUE);
	gtk_widget_set_vexpand(vbox, TRUE);
	XAP_gtk_widget_set_margin(vbox, 6);

	/* search row: label + entry + Search button */
	GtkWidget * srow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_visible(srow, TRUE);
	gtk_box_append(GTK_BOX(vbox), srow);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Zotero_SearchLabel, s);
	GtkWidget * sl = gtk_label_new(s.c_str());
	gtk_widget_set_visible(sl, TRUE);
	gtk_box_append(GTK_BOX(srow), sl);

	m_entry = gtk_entry_new();
	gtk_widget_set_hexpand(m_entry, TRUE);
	gtk_widget_set_visible(m_entry, TRUE);
	if (!getQuery().empty())
		gtk_editable_set_text(GTK_EDITABLE(m_entry), getQuery().c_str());
	g_signal_connect(G_OBJECT(m_entry), "activate",
					 G_CALLBACK(entry_activate_callback), this);
	gtk_box_append(GTK_BOX(srow), m_entry);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Zotero_Search, s);
	GtkWidget * sbtn = gtk_button_new_with_label(s.c_str());
	gtk_widget_set_visible(sbtn, TRUE);
	g_signal_connect(G_OBJECT(sbtn), "clicked",
					 G_CALLBACK(search_clicked_callback), this);
	gtk_box_append(GTK_BOX(srow), sbtn);

	/* style row: label + dropdown of the CSL styles Zotero always
	 * ships; the service ignores unknown names (falls back to its
	 * default) */
	GtkWidget * strow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_visible(strow, TRUE);
	gtk_box_append(GTK_BOX(vbox), strow);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Zotero_StyleLabel, s);
	GtkWidget * stl = gtk_label_new(s.c_str());
	gtk_widget_set_visible(stl, TRUE);
	gtk_box_append(GTK_BOX(strow), stl);

	GtkStringList * styles = gtk_string_list_new(nullptr);
	static const char * const kStyles[] = {
		"",                          /* service default */
		"apa",
		"chicago-author-date",
		"chicago-note-bibliography",
		"mla",
		"ieee",
		"harvard1",
		"vancouver",
	};
	for (const char * st : kStyles)
		gtk_string_list_append(styles, st);
	m_styleCombo = gtk_drop_down_new(G_LIST_MODEL(styles), nullptr);
	gtk_widget_set_visible(m_styleCombo, TRUE);
	gtk_box_append(GTK_BOX(strow), m_styleCombo);

	/* result list */
	GtkWidget * sw = gtk_scrolled_window_new();
	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sw), 180);
	gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(sw), 420);
	gtk_scrolled_window_set_propagate_natural_height(
		GTK_SCROLLED_WINDOW(sw), TRUE);
	gtk_widget_set_vexpand(sw, TRUE);
	gtk_widget_set_visible(sw, TRUE);
	m_listBox = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(m_listBox),
									GTK_SELECTION_SINGLE);
	gtk_widget_set_visible(m_listBox, TRUE);
	g_signal_connect(G_OBJECT(m_listBox), "row-activated",
					 G_CALLBACK(row_activated_callback), this);
	g_signal_connect(G_OBJECT(m_listBox), "row-selected",
					 G_CALLBACK(row_selected_callback), this);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), m_listBox);
	gtk_box_append(GTK_BOX(vbox), sw);

	/* status line (search errors land here, never a modal box) */
	m_status = gtk_label_new(nullptr);
	gtk_label_set_wrap(GTK_LABEL(m_status), TRUE);
	gtk_label_set_xalign(GTK_LABEL(m_status), 0.0f);
	gtk_widget_set_visible(m_status, TRUE);
	gtk_box_append(GTK_BOX(vbox), m_status);
	pSS->getValueUTF8(AP_STRING_ID_DLG_Zotero_Prompt, s);
	gtk_label_set_text(GTK_LABEL(m_status), s.c_str());

	/* Insert buttons are content-area buttons: they must not emit a
	 * dialog response or abiRunModalDialog would end the modal run */
	GtkWidget * btnrow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_visible(btnrow, TRUE);
	gtk_box_append(GTK_BOX(vbox), btnrow);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Zotero_Insert, s);
	m_insertBtn = gtk_button_new_with_label(s.c_str());
	gtk_widget_set_visible(m_insertBtn, TRUE);
	gtk_widget_set_sensitive(m_insertBtn, FALSE);
	g_signal_connect(G_OBJECT(m_insertBtn), "clicked",
					 G_CALLBACK(insert_clicked_callback), this);
	gtk_box_append(GTK_BOX(btnrow), m_insertBtn);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Zotero_InsertBib, s);
	m_insertBibBtn = gtk_button_new_with_label(s.c_str());
	gtk_widget_set_visible(m_insertBibBtn, TRUE);
	gtk_widget_set_sensitive(m_insertBibBtn, FALSE);
	g_signal_connect(G_OBJECT(m_insertBibBtn), "clicked",
					 G_CALLBACK(insertbib_clicked_callback), this);
	gtk_box_append(GTK_BOX(btnrow), m_insertBibBtn);

	abiAddButton(GTK_DIALOG(dlg),
				 pSS->getValue(XAP_STRING_ID_DLG_Close), BUTTON_CLOSE);
	/* a search dialog starts in its entry — not on the default
	 * button — so typing works without a click */
	gtk_widget_grab_focus(m_entry);
	return dlg;
}

gint AP_UnixDialog_Zotero::selectedRow(void) const
{
	GtkListBoxRow * row =
		gtk_list_box_get_selected_row(GTK_LIST_BOX(m_listBox));
	return row ? gtk_list_box_row_get_index(row) : -1;
}

void AP_UnixDialog_Zotero::row_selected_callback(
	GtkListBox *, GtkListBoxRow *, AP_UnixDialog_Zotero * me)
{
	const bool has = me->selectedRow() >= 0;
	gtk_widget_set_sensitive(me->m_insertBtn, has);
	gtk_widget_set_sensitive(me->m_insertBibBtn, has);
}

void AP_UnixDialog_Zotero::rebuildResults(void)
{
	for (GtkWidget * w = gtk_widget_get_first_child(m_listBox); w;)
	{
		GtkWidget * next = gtk_widget_get_next_sibling(w);
		gtk_list_box_remove(GTK_LIST_BOX(m_listBox), w);
		w = next;
	}
	gtk_widget_set_sensitive(m_insertBtn, FALSE);
	gtk_widget_set_sensitive(m_insertBibBtn, FALSE);

	for (UT_uint32 i = 0; i < getResultCount(); ++i)
	{
		GtkWidget * v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		GtkWidget * t = gtk_label_new(getResultTitle(i));
		gtk_label_set_xalign(GTK_LABEL(t), 0.0f);
		GtkStyleContext * ctx = gtk_widget_get_style_context(t);
		gtk_style_context_add_class(ctx, "heading");
		gtk_widget_set_visible(t, TRUE);
		gtk_box_append(GTK_BOX(v), t);

		GtkWidget * p = gtk_label_new(getResultPreview(i));
		gtk_label_set_xalign(GTK_LABEL(p), 0.0f);
		gtk_label_set_wrap(GTK_LABEL(p), TRUE);
		gtk_widget_set_visible(p, TRUE);
		gtk_box_append(GTK_BOX(v), p);
		gtk_widget_set_visible(v, TRUE);
		gtk_list_box_append(GTK_LIST_BOX(m_listBox), v);
	}

	/* first hit selected and focused: Enter inserts it, arrows walk
	 * the list — Enter is the row's "activate" (same as the Insert
	 * button) */
	GtkListBoxRow * first =
		gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_listBox), 0);
	if (first)
	{
		gtk_list_box_select_row(GTK_LIST_BOX(m_listBox), first);
		gtk_widget_grab_focus(m_listBox);
	}
}

void AP_UnixDialog_Zotero::event_Search(void)
{
	setQuery(gtk_editable_get_text(GTK_EDITABLE(m_entry)));
	GObject * item = G_OBJECT(gtk_drop_down_get_selected_item(
		GTK_DROP_DOWN(m_styleCombo)));
	setStyle(item ? gtk_string_object_get_string(
		GTK_STRING_OBJECT(item)) : "");

	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
	std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_Zotero_Searching, s);
	gtk_label_set_text(GTK_LABEL(m_status), s.c_str());

	/* the local service either refuses instantly or answers like a
	 * localhost process — a synchronous call here stays snappy */
	search();
	rebuildResults();
	gtk_label_set_text(GTK_LABEL(m_status), getStatus().c_str());
}

void AP_UnixDialog_Zotero::event_Insert(bool bBib)
{
	const gint i = selectedRow();
	if (i < 0)
		return;
	if (insertItem(static_cast<UT_uint32>(i), bBib))
	{
		m_answer = AP_Dialog_Zotero::a_CANCEL;
		/* done — end the modal run as if Close were hit */
		GtkWidget * dlg = gtk_widget_get_ancestor(m_listBox,
												  GTK_TYPE_DIALOG);
		if (dlg)
			gtk_dialog_response(GTK_DIALOG(dlg), BUTTON_CLOSE);
	}
}
