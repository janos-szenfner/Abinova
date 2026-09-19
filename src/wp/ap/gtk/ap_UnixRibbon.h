/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:t -*- */
/* AbiWord
 * Copyright (C) 2026 AbiSource, Inc.
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

#ifndef AP_UNIXRIBBON_H
#define AP_UNIXRIBBON_H

#include <gtk/gtk.h>

#include "ut_types.h"
#include "ut_vector.h"
#include "xap_Types.h"

class XAP_Frame;
class AV_View;
class EV_UnixMenuBar;

/*****************************************************************/
/* LibreOffice-style ribbon: a GtkNotebook whose pages are
 * horizontal strips of labelled groups (GtkFrame + GtkFlowBox of
 * buttons).  Each button binds the same "menu.*" GActions the
 * classic menubar uses, so enablement and toggle/radio state stay
 * in sync through the normal EV_UnixMenu refresh path.
 *
 * The tab/group layout lives in ap_Ribbon_Layouts.h.
 */

class AP_UnixRibbon
{
public:
	AP_UnixRibbon(XAP_Frame * pFrame, EV_UnixMenuBar * pMenu);
	~AP_UnixRibbon();

	/* builds the widget; caller packs it */
	GtkWidget *		createWidget();
	GtkWidget *		getWidget() const { return m_wNotebook; }

	/* re-sync action states and contextual tab visibility */
	void			refresh();

private:
	GtkWidget *		_makeButton(XAP_Menu_Id id);
	void			_refreshContextualTabs();

	static void		_s_switch_page(GtkNotebook * book, GtkWidget * page,
								   guint page_num, gpointer data);
	static void		_s_motion_enter(GtkEventControllerMotion * ctrl,
									gdouble x, gdouble y, gpointer data);

	XAP_Frame *			m_pFrame;
	EV_UnixMenuBar *	m_pMenu;
	GtkWidget *			m_wNotebook;
	UT_GenericVector<GtkWidget*>	m_vecContextualPages;
};

#endif /* AP_UNIXRIBBON_H */
