/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* AbiWord
 * Copyright (C) 2024 AbiWord contributors
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

#ifndef AP_UNIXSTYLESPANE_H
#define AP_UNIXSTYLESPANE_H

#include <gtk/gtk.h>
#include <string>

class XAP_Frame;
class PD_Style;

/* LibreOffice/Word-style docked Styles pane: current style, style
 * management buttons and a "Apply a style" list rendered with each
 * style's own formatting.  Owned by AP_UnixFrameImpl; the widget it
 * builds lives as the end child of the document GtkPaned. */
class AP_UnixStylesPane
{
public:
	AP_UnixStylesPane(XAP_Frame * pFrame);
	~AP_UnixStylesPane();
	AP_UnixStylesPane(const AP_UnixStylesPane&) = delete;
	AP_UnixStylesPane& operator=(const AP_UnixStylesPane&) = delete;

	GtkWidget *		createWidget();
	/* szCurrentStyle = internal style name at the caret; nullptr keeps
	 * the previous value */
	void			refresh(const char * szCurrentStyle);
	void			rebuildList();

	/* shared helper: pango markup that renders szText in the style's
	 * own family/weight/slant/decoration/size/colour */
	static std::string styleMarkup(const PD_Style * pStyle,
								   const char * szText,
								   double minPt = 8.0,
								   double maxPt = 18.0);

	/* true for the built-in "* List" pseudo-styles (Bullet List,
	 * Numbered List, ...) which are list presets, not real styles */
	static bool		isListPseudoStyle(const char * szInternalName);

private:
	void			_populate(bool bAll);
	void			_applyStyle(const char * szInternalName);
	void			_invokeMethod(const char * szMethod);

	static void		_s_row_activated(GtkListBox * box,
									 GtkListBoxRow * row, gpointer data);
	static void		_s_close_clicked(GtkButton * btn, gpointer data);
	static void		_s_new_style_clicked(GtkButton * btn, gpointer data);
	static void		_s_filter_changed(GtkDropDown * dd,
									  GParamSpec * pspec, gpointer data);

	XAP_Frame *		m_pFrame;
	GtkWidget *		m_wCurrent;
	GtkWidget *		m_wList;
	GtkWidget *		m_wFilter;
	gchar *			m_szCurrent;
};

#endif /* AP_UNIXSTYLESPANE_H */
