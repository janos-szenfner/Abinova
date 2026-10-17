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

#ifndef AP_UNIXCLIPPANE_H
#define AP_UNIXCLIPPANE_H

#include <gtk/gtk.h>
#include "ut_types.h"

class XAP_Frame;

/* Word-style Clipboard task pane: a docked list of the app's
 * clipboard-history ring (most recent first, text payloads only).
 * Clicking an item pastes its plain text at the insertion point;
 * multi-line items become paragraphs.  The pane mirrors the ring
 * live through XAP_App's history listener.  Owned by
 * AP_UnixFrameImpl; lives in the right-hand deck. */
class AP_UnixClipPane
{
public:
	AP_UnixClipPane(XAP_Frame * pFrame);
	~AP_UnixClipPane();
	AP_UnixClipPane(const AP_UnixClipPane&) = delete;
	AP_UnixClipPane& operator=(const AP_UnixClipPane&) = delete;

	GtkWidget *		createWidget();
	void			rebuildList();
	/* inserts history item idx at the caret - shared by the row
	 * click and the pasteClipItem edit method */
	bool			pasteIndex(UT_uint32 idx);

private:
	static void		_s_row_selected(GtkListBox * box,
									GtkListBoxRow * row, gpointer data);
	static void		_s_close_clicked(GtkButton * btn, gpointer data);
	static void		_s_clear_clicked(GtkButton * btn, gpointer data);
	static void		_s_historyChanged(void * ctx);

	XAP_Frame *		m_pFrame;
	GtkWidget *		m_wList;
	GtkWidget *		m_wEmpty;
};

#endif /* AP_UNIXCLIPPANE_H */
