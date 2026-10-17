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

#include <string>
#include <vector>

#include <glib.h>
#include <gtk/gtk.h>

#include "ut_types.h"

#include "xap_Frame.h"
#include "xav_View.h"
#include "fv_View.h"
#include "ap_FmtInspect.h"
#include "ap_UnixFrameImpl.h"
#include "ap_UnixFmtPane.h"

AP_UnixFmtPane::AP_UnixFmtPane(XAP_Frame * pFrame)
	: m_pFrame(pFrame)
	, m_wBody(nullptr)
{
}

AP_UnixFmtPane::~AP_UnixFmtPane()
{
}

GtkWidget * AP_UnixFmtPane::createWidget()
{
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	GtkWidget * header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_start(header, 10);
	gtk_widget_set_margin_end(header, 6);
	gtk_widget_set_margin_top(header, 8);
	gtk_widget_set_margin_bottom(header, 4);
	GtkWidget * title = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(title), "<b>Reveal Formatting</b>");
	gtk_label_set_xalign(GTK_LABEL(title), 0.0);
	gtk_widget_set_hexpand(title, TRUE);
	gtk_box_append(GTK_BOX(header), title);
	GtkWidget * close = gtk_button_new_from_icon_name("window-close-symbolic");
	gtk_button_set_has_frame(GTK_BUTTON(close), FALSE);
	gtk_widget_set_tooltip_text(close, "Close the Reveal Formatting pane");
	g_signal_connect(close, "clicked",
					 G_CALLBACK(_s_close_clicked), this);
	gtk_box_append(GTK_BOX(header), close);
	gtk_box_append(GTK_BOX(box), header);

	GtkWidget * scroller = gtk_scrolled_window_new();
	gtk_widget_set_vexpand(scroller, TRUE);
	m_wBody = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_start(m_wBody, 10);
	gtk_widget_set_margin_end(m_wBody, 10);
	gtk_widget_set_margin_bottom(m_wBody, 10);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller),
								  m_wBody);
	gtk_box_append(GTK_BOX(box), scroller);

	refresh();
	return box;
}

/* view-listener hook - rebuilds only when the report text changed */
void AP_UnixFmtPane::refresh()
{
	if (!m_wBody)
		return;

	FV_View * pView =
		static_cast<FV_View *>(m_pFrame->getCurrentView());
	std::vector<AP_FmtReportSection> secs;
	AP_FmtInspect::buildReport(pView, secs);
	const std::string report = AP_FmtInspect::toText(secs);
	if (report == m_lastReport)
		return;
	m_lastReport = report;

	GtkWidget * child = gtk_widget_get_first_child(m_wBody);
	while (child)
	{
		GtkWidget * next = gtk_widget_get_next_sibling(child);
		gtk_box_remove(GTK_BOX(m_wBody), child);
		child = next;
	}

	for (const AP_FmtReportSection & sec : secs)
	{
		GtkWidget * head = gtk_label_new(nullptr);
		gchar * mk = g_markup_printf_escaped(
			"<b>%s</b>", sec.heading.c_str());
		gtk_label_set_markup(GTK_LABEL(head), mk);
		g_free(mk);
		gtk_label_set_xalign(GTK_LABEL(head), 0.0);
		gtk_widget_set_margin_top(head, 6);
		gtk_box_append(GTK_BOX(m_wBody), head);

		for (const std::string & line : sec.lines)
		{
			GtkWidget * lbl = gtk_label_new(line.c_str());
			gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
			gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
			gtk_label_set_selectable(GTK_LABEL(lbl), TRUE);
			gtk_widget_set_margin_start(lbl, 8);
			gtk_box_append(GTK_BOX(m_wBody), lbl);
		}
	}
}

void AP_UnixFmtPane::_s_close_clicked(GtkButton * /*btn*/, gpointer data)
{
	AP_UnixFmtPane * self = static_cast<AP_UnixFmtPane *>(data);
	XAP_FrameImpl * pImpl = self->m_pFrame
		? self->m_pFrame->getFrameImpl() : nullptr;
	if (pImpl)
		pImpl->toggleFmtPane();
}
