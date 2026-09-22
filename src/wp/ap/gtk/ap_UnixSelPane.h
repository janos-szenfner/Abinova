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

#ifndef AP_UNIXSELPANE_H
#define AP_UNIXSELPANE_H

#include <gtk/gtk.h>
#include <vector>

class XAP_Frame;
class fl_FrameLayout;

/* Word-style Selection pane: docked list of every frame/object in
 * the document (text boxes, positioned images, table/embed
 * wrappers), front-to-back.  Rows select the object in the
 * document, an eye toggle shows/hides it (frame-hidden property),
 * up/down buttons reorder its Z-layer (frame-stack-order) and a
 * double click on the name renames it (frame-name).  All writes go
 * through FV_View so they are undoable and persist in .abw.
 * Owned by AP_UnixFrameImpl; lives in the right-hand deck. */
class AP_UnixSelPane
{
public:
	AP_UnixSelPane(XAP_Frame * pFrame);
	~AP_UnixSelPane();
	AP_UnixSelPane(const AP_UnixSelPane&) = delete;
	AP_UnixSelPane& operator=(const AP_UnixSelPane&) = delete;

	GtkWidget *		createWidget();
	void			rebuildList();
	/* cheap change check for the view listener - rebuilds only
	 * when the frame set actually changed */
	void			refresh();

private:
	bool			_isLive(fl_FrameLayout * pFL);
	void			_selectRow(fl_FrameLayout * pFL);
	void			_toggleHidden(GtkWidget * eyeBtn);
	void			_move(int iDir);
	void			_rename(GtkListBoxRow * row);

	static void		_s_row_selected(GtkListBox * box,
									GtkListBoxRow * row, gpointer data);
	static void		_s_eye_clicked(GtkButton * btn, gpointer data);
	static void		_s_move_clicked(GtkButton * btn, gpointer data);
	static void		_s_close_clicked(GtkButton * btn, gpointer data);
	static void		_s_name_activated(GtkEntry * entry, gpointer data);
	static gboolean	_s_rename_key(GtkEventControllerKey * c,
								  guint keyval, guint keycode,
								  GdkModifierType mods, gpointer data);
	static void		_s_row_gesture(GtkGestureClick * g, int n_press,
								   double x, double y, gpointer data);

	XAP_Frame *		m_pFrame;
	GtkWidget *		m_wList;
	GtkWidget *		m_wUp;
	GtkWidget *		m_wDown;
	fl_FrameLayout *	m_pSelected;
	GtkWidget *		m_wRenamePopover;
	GtkWidget *		m_wRenameEntry;
	GtkListBoxRow *	m_wRenameRow;
	std::vector<fl_FrameLayout *> m_lastFrames;
};

#endif /* AP_UNIXSELPANE_H */
