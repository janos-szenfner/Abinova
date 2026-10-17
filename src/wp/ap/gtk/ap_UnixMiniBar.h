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

#ifndef AP_UNIXMINIBAR_H
#define AP_UNIXMINIBAR_H

#include <gtk/gtk.h>

class XAP_Frame;
class FV_View;

/* Word-style mini toolbar: a small floating format strip (font, size,
 * bold/italic/underline, font colour) that hovers just above the
 * current text selection.  It is a plain overlay child of the
 * document canvas — never a grab — so it cannot steal keyboard focus;
 * the frame impl shows it when a selection gesture completes (mouse
 * release or keyboard extend), repositions it on scroll and hides it
 * on collapse/Escape.
 * Every control dispatches through the same EV_EditMethod path the
 * ribbon buttons use.  Owned by AP_UnixFrameImpl. */
class AP_UnixMiniBar
{
public:
	explicit AP_UnixMiniBar(XAP_Frame * pFrame);
	~AP_UnixMiniBar();
	AP_UnixMiniBar(const AP_UnixMiniBar&) = delete;
	AP_UnixMiniBar& operator=(const AP_UnixMiniBar&) = delete;

	/* build the floating strip; starts hidden.  The caller parents
	 * the returned widget into the canvas GtkOverlay. */
	GtkWidget *		createWidget();

	/* measure + anchor over the selection, then present.  A no-op
	 * while dismissed (Esc) or when no text selection exists. */
	void			show();
	/* re-anchor on the live selection (scroll/zoom); hides when the
	 * selection scrolled fully out of view */
	void			reposition();
	void			hide();
	/* Escape: hide and stay hidden until the selection collapses or
	 * a fresh selection gesture starts */
	void			dismiss();
	void			clearDismissed();
	bool			isVisible() const;
	/* true when w is the strip itself or a descendant — including a
	 * widget inside one of its popovers, which live in a separate
	 * widget tree */
	bool			contains(GtkWidget * w) const;

	/* mirror the selection's current character format onto the
	 * toggles and combos (called on show and on FMTCHAR notifies) */
	void			syncState();

private:
	FV_View *		_view() const;
	bool			_place();
	void			_invokeMethod(const char * szMethod,
								  const char * szData = nullptr);
	void			_applyColor(const char * szHex);
	GtkWidget *		_makeColorButton();

	static void		_s_toggle_clicked(GtkWidget * w, gpointer data);
	static void		_s_font_changed(GtkWidget * w, gpointer data);
	static void		_s_size_changed(GtkWidget * w, gpointer data);
	static void		_s_swatch_clicked(GtkWidget * w, gpointer data);
	static void		_s_custom_clicked(GtkWidget * w, gpointer data);
	static void		_s_custom_response(GtkDialog * dlg, gint response,
									   gpointer data);
	static gboolean	_s_key_pressed(GtkEventControllerKey * c,
								   guint keyval, guint keycode,
								   GdkModifierType state, gpointer data);

	XAP_Frame *		m_pFrame;
	GtkWidget *		m_wBar;
	GtkWidget *		m_wFont;
	GtkWidget *		m_wSize;
	GtkWidget *		m_wBold;
	GtkWidget *		m_wItalic;
	GtkWidget *		m_wUnderline;
	bool			m_bSyncing;
	bool			m_bDismissed;
	/* cached strip size: the combos carry parented GtkPopovers whose
	 * size requests pollute gtk_widget_measure() once realized, so
	 * the bar is measured once, then the value is reused */
	int				m_iBarW;
	int				m_iBarH;
};

#endif /* AP_UNIXMINIBAR_H */
