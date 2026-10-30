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

#ifndef AP_UNIXBACKSTAGE_H
#define AP_UNIXBACKSTAGE_H

#include <gtk/gtk.h>

#include <string>
#include <vector>

class XAP_Frame;
class FV_View;

/* Word-style Backstage (File) view: clicking the File tab swaps the
 * whole window content for a full-page view instead of a ribbon
 * page.  A fixed-width left column offers Info, New, Open, Save,
 * Save As, Print, Share and Options; the right pane is a GtkStack
 * with one page per non-action entry.
 *
 * The Info page shows live document statistics (pages, words,
 * paragraphs, characters, lines, comments) and the document
 * properties, plus buttons that reuse the privacy inspector
 * (fileInspectDoc), the metadata dialog (dlgMetaData) and the
 * Restrict Editing entry points from W44 (restrictEditing).
 * Save/Options are actions, not pages: Save runs fileSave and
 * closes; Options opens the Options dialog modally over the view.
 * Escape or the back chevron returns to the document.
 *
 * Owned by AP_UnixFrameImpl; hosted as the second page of the
 * top-level GtkStack wrapping the frame's main vbox.  Every command
 * dispatches through the same EV_EditMethod path the ribbon and
 * menus use. */
class AP_UnixBackstage
{
public:
	explicit AP_UnixBackstage(XAP_Frame * pFrame);
	~AP_UnixBackstage();
	AP_UnixBackstage(const AP_UnixBackstage&) = delete;
	AP_UnixBackstage& operator=(const AP_UnixBackstage&) = delete;

	/* build the backstage widget; the caller parents it into the
	 * frame's top-level GtkStack under the "backstage" name */
	GtkWidget *		createWidget();
	GtkWidget *		getWidget() const { return m_wRoot; }

	/* switch the content stack to a named page: info/new/open/
	 * saveas/print/share; unknown names land on info.  Refreshes
	 * the page's dynamic content (stats, recent list). */
	void			setPage(const char * szName);
	const char *	page() const;

	/* repopulate the visible page's live data (Info stats +
	 * properties, Open recent list) — called by the frame impl
	 * whenever the backstage is shown */
	void			refresh();

	/* keyboard focus target for when the backstage opens */
	void			grabFocus();

private:
	FV_View *		_view() const;
	void			_invokeMethod(const char * szMethod,
								  const char * szData = nullptr);
	void			_close();
	/* run an edit method and close the backstage afterwards (New,
	 * Open, Save, Browse, Print, Share rows); dialog rows keep the
	 * backstage behind them via bStayOpen */
	void			_runAndClose(const char * szMethod,
								 const char * szData, bool bStayOpen);

	GtkWidget *		_navRow(const char * szLabel, const char * szPage,
							bool bAction);
	GtkWidget *		_bigRow(const char * szLabel, const char * szDetail,
							const char * szMethod, const char * szData,
							bool bStayOpen);
	GtkWidget *		_page(const char * szTitle);
	GtkWidget *		_makeInfoPage();
	GtkWidget *		_makeNewPage();
	GtkWidget *		_makeOpenPage();
	GtkWidget *		_makeSaveAsPage();
	GtkWidget *		_makePrintPage();
	GtkWidget *		_makeSharePage();
	void			_refreshInfo();
	void			_refreshRecent();
	GtkWidget *		_propRow(GtkWidget * grid, int row,
							 const char * szLabel);
	void			_statRow(const char * szLabel, const char * szValue,
							 int row);

	static void		_s_nav_clicked(GtkWidget * w, gpointer data);
	static void		_s_back_clicked(GtkWidget * w, gpointer data);
	static void		_s_row_clicked(GtkWidget * w, gpointer data);
	static gboolean	_s_key(GtkEventControllerKey * c, guint keyval,
						   guint keycode, GdkModifierType state,
						   gpointer data);

	XAP_Frame *		m_pFrame;
	GtkWidget *		m_wRoot;
	GtkWidget *		m_wNav;
	GtkWidget *		m_wStack;
	GtkWidget *		m_wBackBtn;

	/* Info page live labels */
	GtkWidget *		m_wPropTitle;
	GtkWidget *		m_wPropSubject;
	GtkWidget *		m_wPropAuthor;
	GtkWidget *		m_wPropKeywords;
	GtkWidget *		m_wPropCategory;
	GtkWidget *		m_wStatGrid;
	GtkWidget *		m_wLocation;
	GtkWidget *		m_wProtectStatus;
	GtkWidget *		m_wRecentBox;
};

#endif /* AP_UNIXBACKSTAGE_H */
