/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* AbiSource Application Framework
 * Copyright (C) 2024 Abinova contributors
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

#ifndef XAP_UNIXHELPWINDOW_H
#define XAP_UNIXHELPWINDOW_H

#include <gtk/gtk.h>
#include <string>
#include <vector>

class XAP_Frame;

/* Internal help browser: renders the bundled HTML manual
 * (<datadir>/help/<lang>/*.html) in a GtkTextView with clickable
 * links, back/home navigation, a language selector and a search
 * field that scans all pages of the current language. */
class XAP_UnixHelpWindow
{
public:
	XAP_UnixHelpWindow(XAP_Frame * pFrame);
	~XAP_UnixHelpWindow();
	XAP_UnixHelpWindow(const XAP_UnixHelpWindow&) = delete;
	XAP_UnixHelpWindow& operator=(const XAP_UnixHelpWindow&) = delete;

	/* shows the window; page is a path relative to the language dir
	 * ("index.html"); bFocusSearch moves focus to the search field */
	void			show(const char * page, bool bFocusSearch);

	/* the help window widget; the object self-deletes when it is
	 * destroyed (via "help-win-obj" data on the widget) */
	GtkWidget *		window() const { return m_wWindow; }

private:
	std::string		_langDir() const;
	std::string		_resolve(const std::string& rel) const;
	void			_navigate(const std::string& rel, bool bRecord);
	void			_loadPage(const std::string& rel);
	void			_renderHtml(const std::string& html);
	void			_clearLinkTags();
	void			_runSearch(const char * query);
	void			_updateNavButtons();
	std::string		_stripTags(const std::string& html) const;
	std::string		_pageTitle(const std::string& html,
							   const std::string& fallback) const;

	static void		_s_back(GtkButton * btn, gpointer data);
	static void		_s_home(GtkButton * btn, gpointer data);
	static void		_s_lang_changed(GtkDropDown * dd, GParamSpec * ps,
									gpointer data);
	static void		_s_search(GtkSearchEntry * e, gpointer data);
	static void		_s_click(GtkGestureClick * g, int n, double x,
							 double y, gpointer data);
	static void		_s_motion(GtkEventControllerMotion * m, double x,
							  double y, gpointer data);
	static gboolean	_s_destroy(GtkWidget * w, gpointer data);

	XAP_Frame *		m_pFrame;
	GtkWidget *		m_wWindow;
	GtkWidget *		m_wBack;
	GtkWidget *		m_wSearch;
	GtkWidget *		m_wLang;
	GtkWidget *		m_wText;
	std::string		m_lang;			/* en-US / fr-FR / pl-PL */
	std::string		m_page;			/* current page rel path */
	std::vector<std::string> m_history;
	guint			m_iSearchTimer;	/* debounce for live search */
};

#endif /* XAP_UNIXHELPWINDOW_H */
