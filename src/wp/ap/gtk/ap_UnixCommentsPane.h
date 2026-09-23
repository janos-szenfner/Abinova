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

#ifndef AP_UNIXCOMMENTSPANE_H
#define AP_UNIXCOMMENTSPANE_H

#include <gtk/gtk.h>
#include <string>
#include <vector>

#include "ut_types.h"

class XAP_Frame;

/* Word-style "Reviewing Pane": a docked list of every comment in the
 * document.  Each card shows the author colour, author name, date and
 * the comment text, plus per-comment Resolve/Unresolve and Delete
 * buttons.  Selecting a card jumps to the comment's anchored text.
 * Owned by AP_UnixFrameImpl; lives in the right-hand deck. */
class AP_UnixCommentsPane
{
public:
	AP_UnixCommentsPane(XAP_Frame * pFrame);
	~AP_UnixCommentsPane();
	AP_UnixCommentsPane(const AP_UnixCommentsPane&) = delete;
	AP_UnixCommentsPane& operator=(const AP_UnixCommentsPane&) = delete;

	GtkWidget *		createWidget();
	void			refresh();

private:
	void			_refreshNow();
	void			_rebuild();
	static gboolean	_s_refresh_idle(gpointer data);
	GtkWidget *		_makeCard(UT_uint32 pid, UT_sint32 iIndex);

	static void		_s_row_selected(GtkListBox * box, GtkListBoxRow * row,
									gpointer data);
	static void		_s_reply_clicked(GtkButton * btn, gpointer data);
	static void		_s_resolve_clicked(GtkButton * btn, gpointer data);
	static void		_s_delete_clicked(GtkButton * btn, gpointer data);
	static void		_s_new_clicked(GtkButton * btn, gpointer data);
	static void		_s_close_clicked(GtkButton * btn, gpointer data);
	static void		_s_color_draw(GtkDrawingArea * area, cairo_t * cr,
								  int w, int h, gpointer data);

	XAP_Frame *		m_pFrame;
	GtkWidget *		m_wList;
	std::string		m_sig;
	std::vector<gpointer> m_ctx;
	/* author colours of the current cards, indexed like _makeCard's
	 * iIndex - the draw funcs point into this array, so it must not
	 * move between rebuilds */
	UT_uint8		m_cardColors[64][3];
	guint			m_iRefreshIdle;
};

#endif /* AP_UNIXCOMMENTSPANE_H */
