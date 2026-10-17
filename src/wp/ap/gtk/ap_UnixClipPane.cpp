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

#include <deque>
#include <string>

#include <glib.h>
#include <gtk/gtk.h>

#include "ut_types.h"
#include "ut_string_class.h"

#include "xap_App.h"
#include "xap_Frame.h"
#include "xav_View.h"
#include "fv_View.h"
#include "pd_Document.h"
#include "ap_UnixFrameImpl.h"
#include "ap_UnixClipPane.h"

AP_UnixClipPane::AP_UnixClipPane(XAP_Frame * pFrame)
	: m_pFrame(pFrame)
	, m_wList(nullptr)
	, m_wEmpty(nullptr)
{
	if (XAP_App * app = XAP_App::getApp())
		app->addClipboardHistoryListener(_s_historyChanged, this);
}

AP_UnixClipPane::~AP_UnixClipPane()
{
	if (XAP_App * app = XAP_App::getApp())
		app->removeClipboardHistoryListener(_s_historyChanged, this);
}

/* one-line preview for a history item */
static std::string s_clip_preview(const std::string & text)
{
	std::string out;
	out.reserve(text.size());
	bool bSpace = false;
	for (char c : text)
	{
		if (g_ascii_isspace(c))
		{
			if (!bSpace && !out.empty())
				out += ' ';
			bSpace = true;
		}
		else
		{
			out += c;
			bSpace = false;
		}
	}
	while (!out.empty() && out.back() == ' ')
		out.pop_back();
	return out;
}

GtkWidget * AP_UnixClipPane::createWidget()
{
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	GtkWidget * header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_start(header, 10);
	gtk_widget_set_margin_end(header, 6);
	gtk_widget_set_margin_top(header, 8);
	gtk_widget_set_margin_bottom(header, 4);
	GtkWidget * title = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(title), "<b>Clipboard</b>");
	gtk_label_set_xalign(GTK_LABEL(title), 0.0);
	gtk_widget_set_hexpand(title, TRUE);
	gtk_box_append(GTK_BOX(header), title);
	GtkWidget * clear = gtk_button_new_with_label("Clear All");
	gtk_button_set_has_frame(GTK_BUTTON(clear), FALSE);
	gtk_widget_set_tooltip_text(clear, "Remove all clipboard history");
	g_signal_connect(clear, "clicked",
					 G_CALLBACK(_s_clear_clicked), this);
	gtk_box_append(GTK_BOX(header), clear);
	GtkWidget * close = gtk_button_new_from_icon_name("window-close-symbolic");
	gtk_button_set_has_frame(GTK_BUTTON(close), FALSE);
	gtk_widget_set_tooltip_text(close, "Close the Clipboard Pane");
	g_signal_connect(close, "clicked",
					 G_CALLBACK(_s_close_clicked), this);
	gtk_box_append(GTK_BOX(header), close);
	gtk_box_append(GTK_BOX(box), header);

	m_wEmpty = gtk_label_new("Clipboard is empty.\n"
							 "Copy or cut text and it will\n"
							 "appear here for reuse.");
	gtk_widget_set_margin_top(m_wEmpty, 24);
	gtk_widget_add_css_class(m_wEmpty, "dim-label");
	gtk_widget_set_visible(m_wEmpty, FALSE);
	gtk_box_append(GTK_BOX(box), m_wEmpty);

	GtkWidget * scroller = gtk_scrolled_window_new();
	gtk_widget_set_vexpand(scroller, TRUE);
	m_wList = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(m_wList),
									GTK_SELECTION_SINGLE);
	g_signal_connect(m_wList, "row-selected",
					 G_CALLBACK(_s_row_selected), this);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller),
								  m_wList);
	gtk_box_append(GTK_BOX(box), scroller);

	rebuildList();
	return box;
}

void AP_UnixClipPane::rebuildList()
{
	if (!m_wList)
		return;

	GtkListBoxRow * old;
	while ((old = gtk_list_box_get_row_at_index(
				GTK_LIST_BOX(m_wList), 0)))
		gtk_list_box_remove(GTK_LIST_BOX(m_wList), GTK_WIDGET(old));

	XAP_App * app = XAP_App::getApp();
	static const std::deque<std::string> s_empty;
	const std::deque<std::string> & hist =
		app ? app->clipboardHistory() : s_empty;

	gtk_widget_set_visible(m_wEmpty, hist.empty());

	UT_uint32 idx = 0;
	for (const std::string & item : hist)
	{
		GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_widget_set_margin_start(hbox, 6);
		gtk_widget_set_margin_end(hbox, 6);
		gtk_widget_set_margin_top(hbox, 3);
		gtk_widget_set_margin_bottom(hbox, 3);

		GtkWidget * icon =
			gtk_image_new_from_icon_name("edit-paste-symbolic");
		gtk_widget_add_css_class(icon, "dim-label");
		gtk_box_append(GTK_BOX(hbox), icon);

		const std::string preview = s_clip_preview(item);
		GtkWidget * lbl = gtk_label_new(
			preview.empty() ? "(whitespace)" : preview.c_str());
		gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
		gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
		gtk_widget_set_hexpand(lbl, TRUE);
		gtk_widget_set_tooltip_text(lbl,
									"Click to paste at the caret");
		gtk_box_append(GTK_BOX(hbox), lbl);

		GtkWidget * row = gtk_list_box_row_new();
		gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);
		g_object_set_data(G_OBJECT(row), "idx", GUINT_TO_POINTER(idx));
		gtk_list_box_append(GTK_LIST_BOX(m_wList), row);
		++idx;
	}
}

bool AP_UnixClipPane::pasteIndex(UT_uint32 idx)
{
	XAP_App * app = XAP_App::getApp();
	if (!app || idx >= app->clipboardHistory().size())
		return false;
	/* take a copy: the ring is app state, and edits below can pump
	 * code paths that push further clipboard changes */
	const std::string text = app->clipboardHistory()[idx];
	if (text.empty())
		return false;

	FV_View * pView =
		static_cast<FV_View *>(m_pFrame->getCurrentView());
	if (!pView || !pView->getDocument())
		return false;

	PD_Document * pDoc = pView->getDocument();
	UT_UCS4String us(text);
	const UT_UCS4Char * buf = us.ucs4_str();
	const UT_uint32 total = us.length();

	/* one undo unit for the whole paste; line breaks become real
	 * paragraph breaks so multi-line items reflow properly */
	pDoc->beginUserAtomicGlob();
	UT_uint32 segStart = 0;
	for (UT_uint32 i = 0; i <= total; ++i)
	{
		if (i < total && buf[i] != '\n' && buf[i] != '\r')
			continue;
		if (i > segStart)
			pView->cmdCharInsert(buf + segStart, i - segStart);
		if (i < total)
		{
			/* fold \r\n into a single break */
			if (buf[i] == '\r' && i + 1 < total && buf[i + 1] == '\n')
				++i;
			pView->insertParagraphBreak();
			segStart = i + 1;
		}
	}
	pDoc->endUserAtomicGlob();

	pView->ensureInsertionPointOnScreen();
	return true;
}

void AP_UnixClipPane::_s_row_selected(GtkListBox * /*box*/,
									  GtkListBoxRow * row, gpointer data)
{
	AP_UnixClipPane * self = static_cast<AP_UnixClipPane *>(data);
	if (!row || !self->m_pFrame)
		return;
	const guint idx = GPOINTER_TO_UINT(
		g_object_get_data(G_OBJECT(row), "idx"));
	self->pasteIndex(idx);
	/* return focus to the document so typing continues there */
	XAP_FrameImpl * pImpl = self->m_pFrame->getFrameImpl();
	if (pImpl)
	{
		AP_UnixFrameImpl * pUImpl =
			static_cast<AP_UnixFrameImpl *>(pImpl);
		pUImpl->focusDocument();
	}
}

void AP_UnixClipPane::_s_clear_clicked(GtkButton * /*btn*/, gpointer data)
{
	AP_UnixClipPane * self = static_cast<AP_UnixClipPane *>(data);
	if (XAP_App * app = XAP_App::getApp())
		app->clearClipboardHistory();
	XAP_FrameImpl * pImpl = self->m_pFrame
		? self->m_pFrame->getFrameImpl() : nullptr;
	if (pImpl)
	{
		AP_UnixFrameImpl * pUImpl =
			static_cast<AP_UnixFrameImpl *>(pImpl);
		pUImpl->focusDocument();
	}
}

void AP_UnixClipPane::_s_close_clicked(GtkButton * /*btn*/, gpointer data)
{
	AP_UnixClipPane * self = static_cast<AP_UnixClipPane *>(data);
	XAP_FrameImpl * pImpl = self->m_pFrame
		? self->m_pFrame->getFrameImpl() : nullptr;
	if (pImpl)
		pImpl->toggleClipPane();
}

void AP_UnixClipPane::_s_historyChanged(void * ctx)
{
	static_cast<AP_UnixClipPane *>(ctx)->rebuildList();
}
