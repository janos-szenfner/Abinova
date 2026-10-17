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
#include "ap_UnixDialog_DocInspector.h"

/*****************************************************************/

XAP_Dialog * AP_UnixDialog_DocInspector::static_constructor(
	XAP_DialogFactory * pFactory, XAP_Dialog_Id id)
{
	return new AP_UnixDialog_DocInspector(pFactory, id);
}

AP_UnixDialog_DocInspector::AP_UnixDialog_DocInspector(
	XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id)
	: AP_Dialog_DocInspector(pDlgFactory, id)
	, m_catBox(nullptr)
	, m_reportView(nullptr)
	, m_removeBtn(nullptr)
{
}

AP_UnixDialog_DocInspector::~AP_UnixDialog_DocInspector(void)
{
}

void AP_UnixDialog_DocInspector::runModal(XAP_Frame * pFrame)
{
	GtkWidget * mainWindow = constructWindow();
	UT_return_if_fail(mainWindow);

	gint rc = abiRunModalDialog(GTK_DIALOG(mainWindow),
							  pFrame, this, BUTTON_CLOSE, false);
	UT_DEBUGMSG(("AP_UnixDialog_DocInspector::runModal() rc:%d\n", rc));
	UT_UNUSED(rc);

	m_answer = AP_Dialog_DocInspector::a_CLOSE;
	abiDestroyWidget(mainWindow);
}

GtkWidget * AP_UnixDialog_DocInspector::constructWindow(void)
{
	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
	std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_DocInspector_Title, s);
	GtkWidget * dlg = abiDialogNew("doc inspector", TRUE, s.c_str());
	GtkWidget * content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));

	GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_visible(vbox, TRUE);
	gtk_box_append(GTK_BOX(content), vbox);
	gtk_widget_set_hexpand(vbox, TRUE);
	gtk_widget_set_vexpand(vbox, TRUE);
	XAP_gtk_widget_set_margin(vbox, 6);

	pSS->getValueUTF8(AP_STRING_ID_DLG_DocInspector_Intro, s);
	GtkWidget * intro = gtk_label_new(s.c_str());
	gtk_label_set_wrap(GTK_LABEL(intro), TRUE);
	gtk_label_set_xalign(GTK_LABEL(intro), 0.0f);
	gtk_widget_set_visible(intro, TRUE);
	gtk_box_append(GTK_BOX(vbox), intro);

	m_catBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_visible(m_catBox, TRUE);
	gtk_box_append(GTK_BOX(vbox), m_catBox);

	/* the detail report below the category checkboxes, Word-style */
	GtkWidget * sw = gtk_scrolled_window_new();
	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sw), 120);
	gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(sw), 360);
	gtk_widget_set_vexpand(sw, TRUE);
	gtk_widget_set_visible(sw, TRUE);
	m_reportView = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(m_reportView), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(m_reportView), FALSE);
	gtk_widget_set_visible(m_reportView, TRUE);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), m_reportView);
	gtk_box_append(GTK_BOX(vbox), sw);

	/* Re-inspect + Remove are ordinary content-area buttons: they must
	 * not emit a dialog response or abiRunModalDialog would end the
	 * modal run.  Only Close lives in the action area. */
	GtkWidget * btnrow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_visible(btnrow, TRUE);
	gtk_box_append(GTK_BOX(vbox), btnrow);

	pSS->getValueUTF8(AP_STRING_ID_DLG_DocInspector_Reinspect, s);
	GtkWidget * reinsp = gtk_button_new_with_label(s.c_str());
	gtk_widget_set_visible(reinsp, TRUE);
	g_signal_connect(G_OBJECT(reinsp), "clicked",
					 G_CALLBACK(reinspect_clicked_callback), this);
	gtk_box_append(GTK_BOX(btnrow), reinsp);

	pSS->getValueUTF8(AP_STRING_ID_DLG_DocInspector_Remove, s);
	m_removeBtn = gtk_button_new_with_label(s.c_str());
	gtk_widget_set_visible(m_removeBtn, TRUE);
	g_signal_connect(G_OBJECT(m_removeBtn), "clicked",
					 G_CALLBACK(remove_clicked_callback), this);
	gtk_box_append(GTK_BOX(btnrow), m_removeBtn);

	rebuildCategories();

	abiAddButton(GTK_DIALOG(dlg),
				 pSS->getValue(XAP_STRING_ID_DLG_Close), BUTTON_CLOSE);
	return dlg;
}

/* rebuild the per-category checkbox rows + report text from the
 * current AP_DocInspect results */
void AP_UnixDialog_DocInspector::rebuildCategories(void)
{
	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
	std::string sNone;
	pSS->getValueUTF8(AP_STRING_ID_DLG_DocInspector_NoneFound, sNone);

	for (GtkWidget * w = gtk_widget_get_first_child(m_catBox); w;)
	{
		GtkWidget * next = gtk_widget_get_next_sibling(w);
		gtk_box_remove(GTK_BOX(m_catBox), w);
		w = next;
	}
	m_checks.clear();

	for (UT_uint32 i = 0; i < getCategoryCount(); ++i)
	{
		std::string label = getCategoryTitle(i);
		if (isCategoryFound(i))
		{
			label += ": ";
			label += getCategorySummary(i);
		}
		else
		{
			label += ": ";
			label += sNone;
		}
		GtkWidget * cb = gtk_check_button_new_with_label(label.c_str());
		gtk_check_button_set_active(GTK_CHECK_BUTTON(cb),
									isCategoryFound(i));
		gtk_widget_set_sensitive(cb, isCategoryFound(i));
		gtk_widget_set_visible(cb, TRUE);
		g_object_set_data(G_OBJECT(cb), "cat-index", GUINT_TO_POINTER(i));
		g_signal_connect(G_OBJECT(cb), "toggled",
						 G_CALLBACK(check_toggled_callback), this);
		gtk_box_append(GTK_BOX(m_catBox), cb);
		m_checks.push_back(cb);
		setSelected(i, isCategoryFound(i));
	}

	gtk_text_buffer_set_text(
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_reportView)),
		getReportText().c_str(), -1);

	gboolean any = getSelectedMask() != 0;
	gtk_widget_set_sensitive(m_removeBtn, any);
}

void AP_UnixDialog_DocInspector::event_Remove(void)
{
	removeSelected();
	rebuildCategories();
}

void AP_UnixDialog_DocInspector::event_Reinspect(void)
{
	refresh();
	rebuildCategories();
}
