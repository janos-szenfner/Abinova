/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* AbiSource Application Framework
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2025-2026 Abinova contributors
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

#ifndef XAP_UNIXDIALOG_PRINTPREVIEW_H
#define XAP_UNIXDIALOG_PRINTPREVIEW_H

#include "xap_Dlg_PrintPreview.h"
#include <gtk/gtk.h>
#include <vector>

class FL_DocLayout;
class FV_View;
class GR_CairoPrintGraphics;
class XAP_Frame;

/*****************************************************************/

class XAP_UnixDialog_PrintPreview : public XAP_Dialog_PrintPreview
{
 public:
	XAP_UnixDialog_PrintPreview(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id);
	virtual ~XAP_UnixDialog_PrintPreview(void);

	virtual GR_Graphics *			getPrinterGraphicsContext(void) override;
	virtual void					releasePrinterGraphicsContext(GR_Graphics * pGraphics) override;
	virtual void					runModal(XAP_Frame * pFrame) override;

	static XAP_Dialog * static_constructor(XAP_DialogFactory * pFactory,
					       XAP_Dialog_Id id);

 protected:
	void				_buildDocument(void);
	void				_teardownDocument(void);
	cairo_surface_t *	_renderPage(gint page);
	void				_buildWindow(XAP_Frame * pFrame);
	void				_updatePageLabel(void);
	void				_updateZoomLabel(void);
	void				_fitZoom(void);
	void				_scrollToPage(gint page);
	gint				_pageFromScroll(void) const;
	void				_drawFunc(GtkDrawingArea * area, cairo_t * cr,
								  int width, int height);
	gdouble				_pagePitch(void) const;

	static void			_s_draw(GtkDrawingArea * area, cairo_t * cr,
								int width, int height, gpointer data);
	static void			_s_pageNav(GtkWidget * w, gpointer data);
	static void			_s_pageSpinChanged(GtkSpinButton * spin, gpointer data);
	static void			_s_zoom(GtkWidget * w, gpointer data);
	static void			_s_fitChanged(GtkWidget * w, gpointer data);
	static void			_s_scrolled(GtkAdjustment * adj, gpointer data);
	static void			_s_printClicked(GtkWidget * w, gpointer data);
	static void			_s_closeClicked(GtkWidget * w, gpointer data);
	static gboolean		_s_closeRequest(GtkWindow * win, gpointer data);
	static gboolean		_s_key(GtkEventControllerKey * ctl, guint keyval,
							   guint keycode, GdkModifierType state,
							   gpointer data);

	XAP_Frame *			m_pFrame;
	FV_View *			m_pView;
	FL_DocLayout *		m_pPrintLayout;
	FV_View *			m_pPrintView;
	GR_CairoPrintGraphics *	m_pPrintGraphics;
	cairo_surface_t *	m_pLayoutSurface;	/* dummy surface backing the
										 * graphics' idle cairo */
	cairo_t *			m_pLayoutCairo;
	std::vector<cairo_surface_t *>	m_vecPages;		/* lazily-rendered page
												 * recording surfaces */

	GtkWindow *			m_pWindow;
	GtkDrawingArea *	m_pArea;
	GtkScrolledWindow *	m_pScrolled;
	GtkAdjustment *		m_pVAdj;
	GtkSpinButton *		m_pPageEntry;
	GtkLabel *			m_pOfLabel;
	GtkLabel *			m_pZoomLabel;
	GtkToggleButton *	m_pFitWidth;
	GtkToggleButton *	m_pFitPage;
	gboolean			m_bUpdatingUi;	/* guards signal loops */
	gboolean			m_bDidInitialFit;	/* first-draw fit done */

	gint				m_iPages;
	gint				m_iPage;			/* 0-based current page */
	gint				m_iPageW_tdu;		/* page size in layout units */
	gint				m_iPageH_tdu;
	gdouble				m_dZoom;			/* screen px per layout inch */
	GMainLoop *			m_pLoop;
	gboolean			m_bPrintRequested;
};

#endif /* XAP_UNIXDIALOG_PRINTPREVIEW_H */
