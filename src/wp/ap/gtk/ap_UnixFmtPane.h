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

#ifndef AP_UNIXFMTPANE_H
#define AP_UNIXFMTPANE_H

#include <string>

#include <gtk/gtk.h>

class XAP_Frame;

/* Word-style Reveal Formatting (Shift+F1) pane: a read-only docked
 * report of the style chain and formatting layers at the caret —
 * paragraph style + based-on chain, character style, direct
 * formatting deltas, resolved formatting, and list/table/frame
 * context.  Owned by AP_UnixFrameImpl; lives in the side deck and
 * refreshes off the view listener. */
class AP_UnixFmtPane
{
public:
	AP_UnixFmtPane(XAP_Frame * pFrame);
	~AP_UnixFmtPane();
	AP_UnixFmtPane(const AP_UnixFmtPane&) = delete;
	AP_UnixFmtPane& operator=(const AP_UnixFmtPane&) = delete;

	GtkWidget *		createWidget();
	void			refresh();

private:
	static void		_s_close_clicked(GtkButton * btn, gpointer data);

	XAP_Frame *		m_pFrame;
	GtkWidget *		m_wBody;
	std::string		m_lastReport;
};

#endif /* AP_UNIXFMTPANE_H */
