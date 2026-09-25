/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
 * Copyright (C) 2025 Abinova contributors
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

#include "config.h"

#include <cstring>
#include <map>

#include "ap_UnixIconsPane.h"
#include "ap_UnixFrameImpl.h"
#include "xap_Frame.h"
#include "xap_App.h"
#include "xav_View.h"
#include "ev_EditMethod.h"
#include "ut_debugmsg.h"

AP_UnixIconsPane::AP_UnixIconsPane(XAP_Frame * pFrame)
	: m_pFrame(pFrame)
	, m_wSections(nullptr)
	, m_wSearch(nullptr)
{
}

AP_UnixIconsPane::~AP_UnixIconsPane()
{
}

std::string AP_UnixIconsPane::_iconPath(const std::string & name) const
{
	std::string path;
	if (XAP_App::getApp()->findAbiSuiteLibFile(
			path, (name + ".svg").c_str(), "artwork/icons"))
		return path;
	return std::string();
}

/* pretty category title: "food-beverage" -> "Food & Beverage" */
static std::string s_cat_title(const std::string & cat)
{
	std::string t = cat;
	for (auto & c : t)
		if (c == '-')
			c = ' ';
	if (!t.empty())
		t[0] = g_ascii_toupper(t[0]);
	return t;
}

void AP_UnixIconsPane::_buildGrid()
{
	m_rows.clear();
	if (!m_wSections)
		return;
	for (GtkWidget * ch = gtk_widget_get_first_child(m_wSections);
		 ch; )
	{
		GtkWidget * next = gtk_widget_get_next_sibling(ch);
		gtk_box_remove(GTK_BOX(m_wSections), ch);
		ch = next;
	}

	/* index.txt lines: "<icon name>|<lucide category>" */
	std::string indexPath;
	if (!XAP_App::getApp()->findAbiSuiteLibFile(indexPath, "index.txt",
												"artwork/icons"))
		return;
	gchar * contents = nullptr;
	if (!g_file_get_contents(indexPath.c_str(), &contents, nullptr,
							 nullptr))
		return;

	/* group icon names by category, keeping file order */
	std::map<std::string, std::vector<std::string>> groups;
	std::vector<std::string> order;
	for (gchar * line = strtok(contents, "\n"); line;
		 line = strtok(nullptr, "\n"))
	{
		char * bar = strchr(line, '|');
		if (!bar || bar == line)
			continue;
		*bar = 0;
		std::string cat(bar + 1);
		if (groups.find(cat) == groups.end())
			order.push_back(cat);
		groups[cat].push_back(line);
	}
	g_free(contents);

	for (const auto & cat : order)
	{
		GtkWidget * section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		gtk_widget_set_margin_start(section, 8);
		gtk_widget_set_margin_end(section, 8);
		gtk_widget_set_margin_top(section, 6);

		GtkWidget * lbl = gtk_label_new(nullptr);
		char * mk = g_markup_printf_escaped(
			"<span weight='bold' alpha='70%%'>%s</span>",
			s_cat_title(cat).c_str());
		gtk_label_set_markup(GTK_LABEL(lbl), mk);
		g_free(mk);
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(section), lbl);

		GtkWidget * flow = gtk_flow_box_new();
		gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow),
										GTK_SELECTION_NONE);
		gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 2);
		gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 2);
		gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 6);
		gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 6);
		gtk_box_append(GTK_BOX(section), flow);

		for (const auto & name : groups[cat])
		{
			std::string path = _iconPath(name);
			if (path.empty())
				continue;
			GtkWidget * btn = gtk_button_new();
			GtkWidget * pic = gtk_picture_new_for_filename(path.c_str());
			gtk_widget_set_size_request(pic, 26, 26);
			gtk_button_set_child(GTK_BUTTON(btn), pic);
			gtk_widget_add_css_class(btn, "flat");
			std::string tip = name;
			for (auto & c : tip)
				if (c == '-')
					c = ' ';
			gtk_widget_set_tooltip_text(btn, tip.c_str());
			g_object_set_data_full(G_OBJECT(btn), "abi-icon-name",
								   g_strdup(name.c_str()), g_free);
			g_signal_connect(btn, "clicked",
							 G_CALLBACK(_s_icon_clicked), this);
			gtk_flow_box_append(GTK_FLOW_BOX(flow), btn);
			m_rows.push_back({ section, btn, name });
		}
		gtk_box_append(GTK_BOX(m_wSections), section);
	}
	_applyFilter();
}

void AP_UnixIconsPane::_applyFilter()
{
	std::string q;
	if (m_wSearch)
	{
		const gchar * t = gtk_editable_get_text(
			GTK_EDITABLE(m_wSearch));
		if (t)
			q = t;
	}
	for (auto & c : q)
		c = g_ascii_tolower(c);

	/* hide icons not matching the query; hide empty sections */
	std::map<GtkWidget *, bool> sectionHasVisible;
	for (const auto & r : m_rows)
	{
		bool show = q.empty() ||
			r.name.find(q) != std::string::npos;
		gtk_widget_set_visible(r.btn, show);
		if (show)
			sectionHasVisible[r.section] = true;
	}
	for (const auto & r : m_rows)
		gtk_widget_set_visible(r.section, sectionHasVisible[r.section]);
}

void AP_UnixIconsPane::_s_search_changed(GtkSearchEntry * /*e*/,
										 gpointer data)
{
	static_cast<AP_UnixIconsPane *>(data)->_applyFilter();
}

void AP_UnixIconsPane::_s_icon_clicked(GtkButton * btn, gpointer data)
{
	AP_UnixIconsPane * self = static_cast<AP_UnixIconsPane *>(data);
	const char * name = static_cast<const char *>(
		g_object_get_data(G_OBJECT(btn), "abi-icon-name"));
	UT_return_if_fail(name && self->m_pFrame);

	const EV_EditMethodContainer * pEMC =
		XAP_App::getApp()->getEditMethodContainer();
	UT_return_if_fail(pEMC);
	EV_EditMethod * pEM = pEMC->findEditMethodByName("insertIcon");
	UT_return_if_fail(pEM);
	AV_View * pView = self->m_pFrame->getCurrentView();
	EV_EditMethodCallData emcd(name, strlen(name));
	pEM->Fn(pView, &emcd);
}

void AP_UnixIconsPane::_s_close_clicked(GtkButton * /*btn*/,
										gpointer data)
{
	AP_UnixIconsPane * self = static_cast<AP_UnixIconsPane *>(data);
	if (!self->m_pFrame)
		return;
	AP_UnixFrameImpl * pImpl = static_cast<AP_UnixFrameImpl *>(
		self->m_pFrame->getFrameImpl());
	if (pImpl)
		pImpl->setIconsPaneVisible(false);
}

GtkWidget * AP_UnixIconsPane::createWidget()
{
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_size_request(box, 260, -1);

	/* header: title + close */
	GtkWidget * header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_margin_start(header, 8);
	gtk_widget_set_margin_end(header, 4);
	gtk_widget_set_margin_top(header, 4);
	gtk_widget_set_margin_bottom(header, 4);
	GtkWidget * title = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(title), "<b>Icons</b>");
	gtk_label_set_xalign(GTK_LABEL(title), 0.0);
	gtk_widget_set_hexpand(title, TRUE);
	gtk_box_append(GTK_BOX(header), title);
	GtkWidget * close =
		gtk_button_new_from_icon_name("window-close-symbolic");
	gtk_button_set_has_frame(GTK_BUTTON(close), FALSE);
	g_signal_connect(close, "clicked",
					 G_CALLBACK(_s_close_clicked), this);
	gtk_box_append(GTK_BOX(header), close);
	gtk_box_append(GTK_BOX(box), header);
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	/* search box, like Word's "Search Icons" */
	m_wSearch = gtk_search_entry_new();
	gtk_widget_set_margin_start(m_wSearch, 8);
	gtk_widget_set_margin_end(m_wSearch, 8);
	gtk_widget_set_margin_top(m_wSearch, 6);
	gtk_widget_set_margin_bottom(m_wSearch, 4);
	g_signal_connect(m_wSearch, "search-changed",
					 G_CALLBACK(_s_search_changed), this);
	gtk_box_append(GTK_BOX(box), m_wSearch);

	GtkWidget * scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
								   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scroll, TRUE);
	m_wSections = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
								  m_wSections);
	gtk_box_append(GTK_BOX(box), scroll);

	_buildGrid();
	return box;
}
