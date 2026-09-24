/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* AbiWord
 * Copyright (C) 2025 AbiWord contributors
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

#include <algorithm>
#include <functional>
#include <string>

#include <glib.h>
#include <gtk/gtk.h>

#include "ut_types.h"
#include "ut_growbuf.h"
#include "ut_debugmsg.h"
#include "ut_assert.h"
#include "ut_std_string.h"

#include "xap_Frame.h"
#include "xav_View.h"
#include "fv_View.h"
#include "fl_DocLayout.h"
#include "fl_ContainerLayout.h"
#include "fl_BlockLayout.h"
#include "pd_Document.h"
#include "ap_FrameData.h"
#include "ap_UnixFrameImpl.h"
#include "ap_UnixNavPane.h"

AP_UnixNavPane::AP_UnixNavPane(XAP_Frame * pFrame)
	: m_pFrame(pFrame)
	, m_wList(nullptr)
	, m_wEmpty(nullptr)
{
}

AP_UnixNavPane::~AP_UnixNavPane()
{
}

/* block text, trimmed and single-lined for the row label */
static std::string s_block_text(fl_BlockLayout * pBL)
{
	UT_GrowBuf gb;
	if (!pBL->getBlockBuf(&gb) || gb.getLength() == 0)
		return std::string();

	const UT_GrowBufElement * pBuf = gb.getPointer(0);
	UT_uint32 len = std::min<UT_uint32>(gb.getLength(), 160);
	std::vector<gunichar> ucs(len);
	for (UT_uint32 i = 0; i < len; ++i)
		ucs[i] = static_cast<gunichar>(pBuf[i]);

	gchar * utf8 = g_ucs4_to_utf8(ucs.data(), len, nullptr, nullptr, nullptr);
	std::string text = utf8 ? utf8 : "";
	g_free(utf8);

	/* collapse whitespace/newlines so the row stays one line */
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

void AP_UnixNavPane::_collect(std::vector<_Heading> & out)
{
	out.clear();
	AP_FrameData * pFrameData = m_pFrame
		? static_cast<AP_FrameData *>(m_pFrame->getFrameData()) : nullptr;
	FL_DocLayout * pDL = pFrameData ? pFrameData->m_pDocLayout : nullptr;
	if (!pDL)
		return;

	std::function<void(fl_ContainerLayout *)> walk =
		[&](fl_ContainerLayout * pCL)
	{
		for (; pCL; pCL = pCL->getNext())
		{
			if (pCL->getContainerType() == FL_CONTAINER_BLOCK)
			{
				fl_BlockLayout * pBL =
					static_cast<fl_BlockLayout *>(pCL);
				UT_UTF8String sStyle;
				pBL->getStyle(sStyle);
				const char * s = sStyle.utf8_str();
				int level = 0;
				if (s && g_str_has_prefix(s, "Heading ") &&
					g_ascii_isdigit(s[8]))
				{
					level = s[8] - '0';
				}
				if (level >= 1 && level <= 9)
				{
					_Heading h;
					h.pos = pBL->getPosition(true);
					h.level = level;
					h.text = s_block_text(pBL);
					out.push_back(std::move(h));
				}
			}
			else if (pCL->getContainerType() != FL_CONTAINER_TABLE)
			{
				/* nested containers (cells, frames, sections) can
				 * hold blocks; TOC containers are skipped so their
				 * Contents-N entries are not listed as headings */
				walk(pCL->getFirstLayout());
			}
		}
	};
	walk(pDL->getFirstSection());
}

GtkWidget * AP_UnixNavPane::createWidget()
{
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	GtkWidget * header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_start(header, 10);
	gtk_widget_set_margin_end(header, 6);
	gtk_widget_set_margin_top(header, 8);
	gtk_widget_set_margin_bottom(header, 4);
	GtkWidget * title = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(title), "<b>Navigation</b>");
	gtk_label_set_xalign(GTK_LABEL(title), 0.0);
	gtk_widget_set_hexpand(title, TRUE);
	gtk_box_append(GTK_BOX(header), title);
	GtkWidget * close = gtk_button_new_from_icon_name("window-close-symbolic");
	gtk_button_set_has_frame(GTK_BUTTON(close), FALSE);
	gtk_widget_set_tooltip_text(close, "Close the Navigation Pane");
	g_signal_connect(close, "clicked",
					 G_CALLBACK(_s_close_clicked), this);
	gtk_box_append(GTK_BOX(header), close);
	gtk_box_append(GTK_BOX(box), header);

	m_wEmpty = gtk_label_new("No headings in this document.\n"
							 "Apply a Heading style to paragraphs\n"
							 "to navigate them here.");
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

	return box;
}

void AP_UnixNavPane::rebuildList()
{
	if (!m_wList)
		return;

	GtkListBoxRow * old;
	while ((old = gtk_list_box_get_row_at_index(
				GTK_LIST_BOX(m_wList), 0)))
		gtk_list_box_remove(GTK_LIST_BOX(m_wList), GTK_WIDGET(old));

	std::vector<_Heading> heads;
	_collect(heads);

	m_lastHeads.clear();
	m_lastHeads.reserve(heads.size());
	for (const _Heading & h : heads)
		m_lastHeads.push_back(h.pos);

	gtk_widget_set_visible(m_wEmpty, heads.empty());

	for (const _Heading & h : heads)
	{
		GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_widget_set_margin_start(hbox, 6 + 14 * (h.level - 1));
		gtk_widget_set_margin_end(hbox, 6);
		gtk_widget_set_margin_top(hbox, 3);
		gtk_widget_set_margin_bottom(hbox, 3);

		GtkWidget * lbl = gtk_label_new(
			h.text.empty() ? "(empty heading)" : h.text.c_str());
		gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
		gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
		gtk_widget_set_hexpand(lbl, TRUE);
		if (h.level == 1)
		{
			char * mk = g_markup_printf_escaped(
				"<b>%s</b>", h.text.empty() ? "(empty heading)"
										   : h.text.c_str());
			gtk_label_set_markup(GTK_LABEL(lbl), mk);
			g_free(mk);
		}
		gtk_box_append(GTK_BOX(hbox), lbl);

		GtkWidget * row = gtk_list_box_row_new();
		gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);
		g_object_set_data(G_OBJECT(row), "pos",
						  GINT_TO_POINTER(h.pos));
		gtk_list_box_append(GTK_LIST_BOX(m_wList), row);
	}
}

/* view-listener hook - rebuilds only when the heading set changed */
void AP_UnixNavPane::refresh()
{
	if (!m_wList)
		return;
	std::vector<_Heading> heads;
	_collect(heads);
	if (heads.size() != m_lastHeads.size())
	{
		rebuildList();
		return;
	}
	for (size_t i = 0; i < heads.size(); ++i)
	{
		if (heads[i].pos != m_lastHeads[i])
		{
			rebuildList();
			return;
		}
	}
}

void AP_UnixNavPane::_s_row_selected(GtkListBox * /*box*/,
									 GtkListBoxRow * row, gpointer data)
{
	AP_UnixNavPane * self = static_cast<AP_UnixNavPane *>(data);
	if (!row || !self->m_pFrame)
		return;
	FV_View * pView =
		static_cast<FV_View *>(self->m_pFrame->getCurrentView());
	if (!pView)
		return;
	PT_DocPosition pos = GPOINTER_TO_INT(
		g_object_get_data(G_OBJECT(row), "pos"));
	PD_Document * pDoc = pView->getDocument();
	PT_DocPosition posEOD = 0;
	if (pDoc)
		pDoc->getBounds(true, posEOD);
	if (!pDoc || pos <= 0 || pos >= posEOD)
		return;
	pView->moveInsPtTo(pos);
	pView->ensureInsertionPointOnScreen();
	/* return focus to the document so typing continues there */
	XAP_FrameImpl * pImpl = self->m_pFrame->getFrameImpl();
	if (pImpl)
	{
		AP_UnixFrameImpl * pUImpl =
			static_cast<AP_UnixFrameImpl *>(pImpl);
		pUImpl->focusDocument();
	}
}

void AP_UnixNavPane::_s_close_clicked(GtkButton * /*btn*/, gpointer data)
{
	AP_UnixNavPane * self = static_cast<AP_UnixNavPane *>(data);
	XAP_FrameImpl * pImpl = self->m_pFrame
		? self->m_pFrame->getFrameImpl() : nullptr;
	if (pImpl)
		pImpl->toggleNavPane();
}
