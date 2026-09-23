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

#ifndef AP_UNIXICONSPANE_H
#define AP_UNIXICONSPANE_H

#include <gtk/gtk.h>
#include <string>
#include <vector>

class XAP_Frame;

/* Word-style "Stock Images > Icons" side pane: a docked, searchable
 * gallery of the bundled Lucide icon set (artwork/icons, ISC
 * licensed).  Icons are grouped by their Lucide category with a
 * bold header per group; clicking an icon inserts the SVG at the
 * caret through the insertIcon edit method.
 * Owned by AP_UnixFrameImpl; lives in the right-hand deck. */
class AP_UnixIconsPane
{
public:
	AP_UnixIconsPane(XAP_Frame * pFrame);
	~AP_UnixIconsPane();
	AP_UnixIconsPane(const AP_UnixIconsPane&) = delete;
	AP_UnixIconsPane& operator=(const AP_UnixIconsPane&) = delete;

	GtkWidget *		createWidget();

private:
	void			_buildGrid();
	void			_applyFilter();
	std::string		_iconPath(const std::string & name) const;

	static void		_s_search_changed(GtkSearchEntry * e, gpointer data);
	static void		_s_icon_clicked(GtkButton * btn, gpointer data);
	static void		_s_close_clicked(GtkButton * btn, gpointer data);

	XAP_Frame *		m_pFrame;
	GtkWidget *		m_wSections;
	GtkWidget *		m_wSearch;
	/* every icon button + its section container, for filtering */
	struct IconRow { GtkWidget * section; GtkWidget * btn; std::string name; };
	std::vector<IconRow> m_rows;
};

#endif /* AP_UNIXICONSPANE_H */
