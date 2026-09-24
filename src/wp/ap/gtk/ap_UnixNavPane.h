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

#ifndef AP_UNIXNAVPANE_H
#define AP_UNIXNAVPANE_H

#include <gtk/gtk.h>
#include <vector>
#include <string>
#include "ut_types.h"
#include "pt_Types.h"

class XAP_Frame;

/* Word-style Navigation pane: docked list of the document's
 * headings (paragraphs styled "Heading 1" .. "Heading 9"),
 * indented by level.  Clicking a row moves the insertion point to
 * that heading and scrolls it into view.  Rows store document
 * positions rather than layout pointers so edits do not leave
 * dangling references; the list re-validates positions through the
 * document before jumping.  Owned by AP_UnixFrameImpl; lives in
 * the right-hand deck. */
class AP_UnixNavPane
{
public:
	AP_UnixNavPane(XAP_Frame * pFrame);
	~AP_UnixNavPane();
	AP_UnixNavPane(const AP_UnixNavPane&) = delete;
	AP_UnixNavPane& operator=(const AP_UnixNavPane&) = delete;

	GtkWidget *		createWidget();
	void			rebuildList();
	/* cheap change check for the view listener - rebuilds only
	 * when the heading set actually changed */
	void			refresh();

private:
	struct _Heading
	{
		PT_DocPosition		pos;
		int					level;
		std::string			text;
	};
	void			_collect(std::vector<_Heading> & out);
	static void		_s_row_selected(GtkListBox * box,
									GtkListBoxRow * row, gpointer data);
	static void		_s_close_clicked(GtkButton * btn, gpointer data);

	XAP_Frame *		m_pFrame;
	GtkWidget *		m_wList;
	GtkWidget *		m_wEmpty;
	std::vector<PT_DocPosition> m_lastHeads;
};

#endif /* AP_UNIXNAVPANE_H */
