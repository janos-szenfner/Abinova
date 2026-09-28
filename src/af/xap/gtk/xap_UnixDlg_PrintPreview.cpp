/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova Application Framework — print preview
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

#include <math.h>

#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ut_units.h"

#include "xap_App.h"
#include "xap_DialogFactory.h"
#include "xap_Dialog_Id.h"
#include "xap_Frame.h"
#include "xap_Strings.h"
#include "xap_UnixFrameImpl.h"

#include "ev_EditMethod.h"
#include "gr_CairoPrintGraphics.h"
#include "gr_DrawArgs.h"

#include "pd_Document.h"
#include "fl_DocLayout.h"
#include "fp_Page.h"
#include "fv_View.h"

#include "xap_UnixDlg_PrintPreview.h"

/* Pixel gap surrounding each page in the preview strip. */
#define PREVIEW_GAP		14
/* 100 % zoom means one screen pixel per CSS-96dpi point. */
#define PREVIEW_DPI_100	96.0
#define PREVIEW_MIN_ZOOM 16.0
#define PREVIEW_MAX_ZOOM 800.0

XAP_Dialog * XAP_UnixDialog_PrintPreview::static_constructor(XAP_DialogFactory * pFactory,
															 XAP_Dialog_Id id)
{
	return new XAP_UnixDialog_PrintPreview(pFactory, id);
}

XAP_UnixDialog_PrintPreview::XAP_UnixDialog_PrintPreview(XAP_DialogFactory * pDlgFactory,
														 XAP_Dialog_Id id)
	: XAP_Dialog_PrintPreview(pDlgFactory, id)
	, m_pFrame(nullptr)
	, m_pView(nullptr)
	, m_pPrintLayout(nullptr)
	, m_pPrintView(nullptr)
	, m_pPrintGraphics(nullptr)
	, m_pLayoutSurface(nullptr)
	, m_pLayoutCairo(nullptr)
	, m_pWindow(nullptr)
	, m_pArea(nullptr)
	, m_pScrolled(nullptr)
	, m_pVAdj(nullptr)
	, m_pPageEntry(nullptr)
	, m_pOfLabel(nullptr)
	, m_pZoomLabel(nullptr)
	, m_pFitWidth(nullptr)
	, m_pFitPage(nullptr)
	, m_bUpdatingUi(false)
	, m_bDidInitialFit(false)
	, m_iPages(0)
	, m_iPage(0)
	, m_iPageW_tdu(0)
	, m_iPageH_tdu(0)
	, m_dZoom(PREVIEW_DPI_100)
	, m_pLoop(nullptr)
	, m_bPrintRequested(false)
{
}

XAP_UnixDialog_PrintPreview::~XAP_UnixDialog_PrintPreview(void)
{
	/* All resources are released at the end of runModal(). */
}

GR_Graphics * XAP_UnixDialog_PrintPreview::getPrinterGraphicsContext(void)
{
	return m_pPrintGraphics;
}

void XAP_UnixDialog_PrintPreview::releasePrinterGraphicsContext(GR_Graphics * pGraphics)
{
	if (pGraphics == m_pPrintGraphics)
	{
		DELETEP(m_pPrintGraphics);
	}
}

/*****************************************************************/
/* Document / rendering                                           */
/*****************************************************************/

void XAP_UnixDialog_PrintPreview::_buildDocument(void)
{
	m_pView = static_cast<FV_View *>(m_pFrame->getCurrentView());
	if (!m_pView)
		return;

	PD_Document * doc = m_pView->getDocument();
	if (!doc)
		return;

	/* The graphics object needs a cairo_t from the start; it is only
	 * ever used while a per-page recording cairo is installed, so a
	 * tiny image surface suffices. */
	m_pLayoutSurface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 16, 16);
	if (cairo_surface_status(m_pLayoutSurface) != CAIRO_STATUS_SUCCESS)
	{
		cairo_surface_destroy(m_pLayoutSurface);
		m_pLayoutSurface = nullptr;
		return;
	}
	m_pLayoutCairo = cairo_create(m_pLayoutSurface);
	m_pPrintGraphics = new GR_CairoPrintGraphics(m_pLayoutCairo, gr_PRINTRES);

	double screenRes = m_pView->getGraphics()
		? m_pView->getGraphics()->getDeviceResolution() : PREVIEW_DPI_100;
	if (screenRes <= 0.0)
		screenRes = PREVIEW_DPI_100;
	m_pPrintGraphics->setResolutionRatio(gr_PRINTRES / screenRes);

	m_pPrintLayout = new FL_DocLayout(doc, m_pPrintGraphics);
	m_pPrintView = new FV_View(XAP_App::getApp(), nullptr, m_pPrintLayout);
	m_pPrintView->setViewMode(VIEW_PRINT);
	m_pPrintLayout->fillLayouts();
	m_pPrintLayout->formatAll();
	m_pPrintLayout->recalculateTOCFields();

	m_iPages = m_pPrintLayout->countPages();
	if (m_iPages > 0)
	{
		fp_Page * p0 = m_pPrintLayout->getNthPage(0);
		if (p0)
		{
			m_iPageW_tdu = p0->getWidth();
			m_iPageH_tdu = p0->getHeight();
		}
	}
	if (m_iPageW_tdu <= 0 || m_iPageH_tdu <= 0)
		m_iPages = 0;

	m_vecPages.assign(m_iPages, nullptr);
}

cairo_surface_t * XAP_UnixDialog_PrintPreview::_renderPage(gint page)
{
	if (page < 0 || page >= m_iPages)
		return nullptr;
	cairo_surface_t * surf = m_vecPages[page];
	if (surf)
		return surf;

	/* Record the page in print-resolution device units. Recording
	 * surfaces replay losslessly at any zoom, so the preview stays
	 * crisp without rasterizing. */
	cairo_rectangle_t ext = {
		0.0, 0.0,
		m_iPageW_tdu * gr_PRINTRES / UT_LAYOUT_RESOLUTION,
		m_iPageH_tdu * gr_PRINTRES / UT_LAYOUT_RESOLUTION
	};
	surf = cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA, &ext);
	if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS)
	{
		cairo_surface_destroy(surf);
		return nullptr;
	}

	cairo_t * cr = cairo_create(surf);
	if (cairo_status(cr) == CAIRO_STATUS_SUCCESS)
	{
		m_pPrintGraphics->setCairo(cr);
		m_pPrintGraphics->beginPaint();
		m_pPrintGraphics->m_iRasterPosition =
			static_cast<UT_uint32>(page) * static_cast<UT_uint32>(m_iPageH_tdu);

		dg_DrawArgs da;
		da.pG = m_pPrintGraphics;
		da.xoff = 0;
		da.yoff = 0;
		m_pPrintView->drawPage(page, &da);

		m_pPrintGraphics->endPaint();
		/* Restore the idle cairo so the just-destroyed context is
		 * never referenced again (the destructor would otherwise
		 * cairo_destroy() a dangling pointer). */
		m_pPrintGraphics->setCairo(m_pLayoutCairo);
	}
	cairo_destroy(cr);

	if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS)
	{
		cairo_surface_destroy(surf);
		return nullptr;
	}
	m_vecPages[page] = surf;
	return surf;
}

void XAP_UnixDialog_PrintPreview::_teardownDocument(void)
{
	for (cairo_surface_t * surf : m_vecPages)
	{
		if (surf)
			cairo_surface_destroy(surf);
	}
	m_vecPages.clear();

	DELETEP(m_pPrintView);
	DELETEP(m_pPrintLayout);
	DELETEP(m_pPrintGraphics);	/* destroys m_pLayoutCairo */
	m_pLayoutCairo = nullptr;
	if (m_pLayoutSurface)
	{
		cairo_surface_destroy(m_pLayoutSurface);
		m_pLayoutSurface = nullptr;
	}
}

/*****************************************************************/
/* Drawing                                                        */
/*****************************************************************/

gdouble XAP_UnixDialog_PrintPreview::_pagePitch(void) const
{
	return m_iPageH_tdu * m_dZoom / UT_LAYOUT_RESOLUTION + PREVIEW_GAP;
}

void XAP_UnixDialog_PrintPreview::_drawFunc(GtkDrawingArea * /*area*/,
											cairo_t * cr, int width, int /*height*/)
{
	cairo_set_source_rgb(cr, 0.42, 0.42, 0.45);
	cairo_paint(cr);

	if (!m_pPrintView || m_iPages < 1)
		return;

	double pw = m_iPageW_tdu * m_dZoom / UT_LAYOUT_RESOLUTION;
	double ph = m_iPageH_tdu * m_dZoom / UT_LAYOUT_RESOLUTION;
	double pitch = _pagePitch();
	double x0 = MAX(PREVIEW_GAP, (width - pw) * 0.5);
	double scale = m_dZoom / gr_PRINTRES;

	double clipTop = 0.0, clipBot = 0.0;
	{
		double cx1, cy1, cx2, cy2;
		cairo_clip_extents(cr, &cx1, &cy1, &cx2, &cy2);
		clipTop = cy1;
		clipBot = cy2;
	}

	gint first = (gint) MAX(0, floor((clipTop - PREVIEW_GAP) / pitch));
	gint last = (gint) MIN(m_iPages - 1, ceil((clipBot - PREVIEW_GAP) / pitch));

	for (gint i = first; i <= last; ++i)
	{
		double y0 = PREVIEW_GAP + i * pitch;
		cairo_surface_t * surf = _renderPage(i);
		if (!surf)
			continue;

		/* drop shadow */
		cairo_set_source_rgba(cr, 0, 0, 0, 0.28);
		cairo_rectangle(cr, x0 + 3, y0 + 3, pw, ph);
		cairo_fill(cr);

		/* paper: print output skips the paper-colour fill (printers
		 * imply it), so paint it ourselves under the recording */
		cairo_set_source_rgb(cr, 1, 1, 1);
		cairo_rectangle(cr, x0, y0, pw, ph);
		cairo_fill(cr);

		/* page contents */
		cairo_save(cr);
		cairo_translate(cr, x0, y0);
		cairo_scale(cr, scale, scale);
		cairo_set_source_surface(cr, surf, 0, 0);
		cairo_paint(cr);
		cairo_restore(cr);

		/* hairline border */
		cairo_set_source_rgb(cr, 0.2, 0.2, 0.25);
		cairo_set_line_width(cr, 1);
		cairo_rectangle(cr, x0 + 0.5, y0 + 0.5, pw - 1, ph - 1);
		cairo_stroke(cr);
	}
}

void XAP_UnixDialog_PrintPreview::_s_draw(GtkDrawingArea * area, cairo_t * cr,
										  int width, int height, gpointer data)
{
	XAP_UnixDialog_PrintPreview * self =
		static_cast<XAP_UnixDialog_PrintPreview *>(data);

	/* The initial fit-to-width cannot run at present() time: the
	 * window still has zero size then.  Defer it until the drawing
	 * area is first drawn with a real allocation. */
	if (!self->m_bDidInitialFit && width > 64)
	{
		self->m_bDidInitialFit = true;
		if (gtk_toggle_button_get_active(self->m_pFitWidth) ||
			gtk_toggle_button_get_active(self->m_pFitPage))
			self->_fitZoom();
	}
	self->_drawFunc(area, cr, width, height);
}

/*****************************************************************/
/* UI                                                             */
/*****************************************************************/

void XAP_UnixDialog_PrintPreview::_updatePageLabel(void)
{
	m_bUpdatingUi = true;
	gtk_spin_button_set_value(m_pPageEntry, m_iPage + 1);
	m_bUpdatingUi = false;
}

void XAP_UnixDialog_PrintPreview::_updateZoomLabel(void)
{
	gchar buf[16];
	g_snprintf(buf, sizeof(buf), "%d%%", (int) floor(m_dZoom / PREVIEW_DPI_100 * 100.0 + 0.5));
	gtk_label_set_text(m_pZoomLabel, buf);

	/* Recompute the scrollable content size. */
	if (m_iPages > 0)
	{
		gint cw = (gint) ceil(m_iPageW_tdu * m_dZoom / UT_LAYOUT_RESOLUTION) + 2 * PREVIEW_GAP;
		gint ch = (gint) ceil(_pagePitch() * m_iPages + PREVIEW_GAP);
		gtk_drawing_area_set_content_width(m_pArea, cw);
		gtk_drawing_area_set_content_height(m_pArea, ch);
		gtk_widget_queue_draw(GTK_WIDGET(m_pArea));
	}
}

void XAP_UnixDialog_PrintPreview::_fitZoom(void)
{
	if (!m_pScrolled || m_iPages < 1)
		return;

	gdouble availW = gtk_widget_get_width(GTK_WIDGET(m_pScrolled)) - 2 * PREVIEW_GAP;
	gdouble availH = gtk_widget_get_height(GTK_WIDGET(m_pScrolled)) - 2 * PREVIEW_GAP;
	if (availW < 32)
		availW = 32;
	if (availH < 32)
		availH = 32;

	gdouble pageW_in = m_iPageW_tdu / (gdouble) UT_LAYOUT_RESOLUTION;
	gdouble pageH_in = m_iPageH_tdu / (gdouble) UT_LAYOUT_RESOLUTION;

	gdouble zoom = availW / pageW_in;
	if (m_pFitPage && gtk_toggle_button_get_active(m_pFitPage))
		zoom = MIN(zoom, availH / pageH_in);

	m_dZoom = CLAMP(zoom, PREVIEW_MIN_ZOOM, PREVIEW_MAX_ZOOM);
	_updateZoomLabel();
}

void XAP_UnixDialog_PrintPreview::_scrollToPage(gint page)
{
	page = CLAMP(page, 0, m_iPages - 1);
	m_iPage = page;
	_updatePageLabel();
	if (m_pVAdj)
	{
		gtk_adjustment_set_value(m_pVAdj,
								 gtk_adjustment_get_lower(m_pVAdj) +
								 PREVIEW_GAP + page * _pagePitch());
	}
}

gint XAP_UnixDialog_PrintPreview::_pageFromScroll(void) const
{
	if (!m_pVAdj || m_iPages < 1)
		return 0;
	gdouble v = gtk_adjustment_get_value(m_pVAdj)
		+ gtk_adjustment_get_page_size(m_pVAdj) * 0.25;
	return CLAMP((gint) floor(v / _pagePitch()), 0, m_iPages - 1);
}

/*****************************************************************/
/* Signal callbacks                                               */
/*****************************************************************/

void XAP_UnixDialog_PrintPreview::_s_pageNav(GtkWidget * w, gpointer data)
{
	XAP_UnixDialog_PrintPreview * self =
		static_cast<XAP_UnixDialog_PrintPreview *>(data);
	int dir = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "nav-dir"));
	self->_scrollToPage(self->m_iPage + dir);
}

void XAP_UnixDialog_PrintPreview::_s_pageSpinChanged(GtkSpinButton * spin, gpointer data)
{
	XAP_UnixDialog_PrintPreview * self =
		static_cast<XAP_UnixDialog_PrintPreview *>(data);
	if (self->m_bUpdatingUi)
		return;
	self->_scrollToPage(gtk_spin_button_get_value_as_int(spin) - 1);
}

void XAP_UnixDialog_PrintPreview::_s_zoom(GtkWidget * w, gpointer data)
{
	XAP_UnixDialog_PrintPreview * self =
		static_cast<XAP_UnixDialog_PrintPreview *>(data);
	gdouble pct = (gdouble) GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "zoom-pct"));
	gdouble keep = self->m_iPage * self->_pagePitch()
		- (self->m_pVAdj ? gtk_adjustment_get_value(self->m_pVAdj) : 0);

	if (pct > 0.0)
		self->m_dZoom = PREVIEW_DPI_100 * pct / 100.0;
	else
		self->m_dZoom *= GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "zoom-dir")) > 0
			? 1.25 : 0.8;
	self->m_dZoom = CLAMP(self->m_dZoom, PREVIEW_MIN_ZOOM, PREVIEW_MAX_ZOOM);

	gtk_toggle_button_set_active(self->m_pFitWidth, FALSE);
	gtk_toggle_button_set_active(self->m_pFitPage, FALSE);
	self->_updateZoomLabel();

	/* keep the same document position visible across the zoom change */
	if (self->m_pVAdj)
		gtk_adjustment_set_value(self->m_pVAdj,
								 self->m_iPage * self->_pagePitch() - keep);
}

void XAP_UnixDialog_PrintPreview::_s_zoomChoose(GtkWidget * w, GParamSpec * /*pspec*/,
												gpointer data)
{
	/* reserved for future use */
	UT_UNUSED(w);
	UT_UNUSED(data);
}

void XAP_UnixDialog_PrintPreview::_s_fitChanged(GtkWidget * w, gpointer data)
{
	XAP_UnixDialog_PrintPreview * self =
		static_cast<XAP_UnixDialog_PrintPreview *>(data);
	if (self->m_bUpdatingUi)
		return;
	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
		return;

	self->m_bUpdatingUi = true;
	gtk_toggle_button_set_active(self->m_pFitWidth, w == GTK_WIDGET(self->m_pFitWidth));
	gtk_toggle_button_set_active(self->m_pFitPage, w == GTK_WIDGET(self->m_pFitPage));
	self->m_bUpdatingUi = false;

	self->_fitZoom();
	self->_scrollToPage(self->m_iPage);
}

void XAP_UnixDialog_PrintPreview::_s_scrolled(GtkAdjustment * /*adj*/, gpointer data)
{
	XAP_UnixDialog_PrintPreview * self =
		static_cast<XAP_UnixDialog_PrintPreview *>(data);
	gint page = self->_pageFromScroll();
	if (page != self->m_iPage)
	{
		self->m_iPage = page;
		self->_updatePageLabel();
	}
}

void XAP_UnixDialog_PrintPreview::_s_printClicked(GtkWidget * /*w*/, gpointer data)
{
	XAP_UnixDialog_PrintPreview * self =
		static_cast<XAP_UnixDialog_PrintPreview *>(data);
	self->m_bPrintRequested = true;
	gtk_window_close(self->m_pWindow);
}

void XAP_UnixDialog_PrintPreview::_s_closeClicked(GtkWidget * /*w*/, gpointer data)
{
	gtk_window_close(static_cast<XAP_UnixDialog_PrintPreview *>(data)->m_pWindow);
}

gboolean XAP_UnixDialog_PrintPreview::_s_closeRequest(GtkWindow * /*win*/, gpointer data)
{
	XAP_UnixDialog_PrintPreview * self =
		static_cast<XAP_UnixDialog_PrintPreview *>(data);
	gtk_widget_set_visible(GTK_WIDGET(self->m_pWindow), FALSE);
	if (self->m_pLoop && g_main_loop_is_running(self->m_pLoop))
		g_main_loop_quit(self->m_pLoop);
	return TRUE;	/* we destroy the window ourselves after teardown */
}

gboolean XAP_UnixDialog_PrintPreview::_s_key(GtkEventControllerKey * /*ctl*/, guint keyval,
							  guint /*keycode*/, GdkModifierType /*state*/,
							  gpointer data)
{
	XAP_UnixDialog_PrintPreview * self =
		static_cast<XAP_UnixDialog_PrintPreview *>(data);
	switch (keyval)
	{
		case GDK_KEY_Escape:
			gtk_window_close(self->m_pWindow);
			return TRUE;
		case GDK_KEY_plus:
		case GDK_KEY_equal:
			self->m_dZoom = CLAMP(self->m_dZoom * 1.25,
								  PREVIEW_MIN_ZOOM, PREVIEW_MAX_ZOOM);
			self->_updateZoomLabel();
			return TRUE;
		case GDK_KEY_minus:
			self->m_dZoom = CLAMP(self->m_dZoom * 0.8,
								  PREVIEW_MIN_ZOOM, PREVIEW_MAX_ZOOM);
			self->_updateZoomLabel();
			return TRUE;
		case GDK_KEY_Page_Down:
			self->_scrollToPage(self->m_iPage + 1);
			return TRUE;
		case GDK_KEY_Page_Up:
			self->_scrollToPage(self->m_iPage - 1);
			return TRUE;
	}
	return FALSE;
}

/*****************************************************************/
/* Window construction                                            */
/*****************************************************************/

void XAP_UnixDialog_PrintPreview::_buildWindow(XAP_Frame * pFrame)
{
	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();

	m_pWindow = GTK_WINDOW(gtk_window_new());
	const char * docTitle = m_szDocumentTitle ? m_szDocumentTitle : "";
	gchar * title = g_strdup_printf("%s — %s",
									pSS->getValue(XAP_STRING_ID_DLG_UP_PrintPreviewTitle),
									docTitle);
	gtk_window_set_title(m_pWindow, title);
	g_free(title);

	gtk_window_set_default_size(m_pWindow, 880, 700);
	gtk_window_set_modal(m_pWindow, TRUE);

	XAP_UnixFrameImpl * pImpl = static_cast<XAP_UnixFrameImpl *>(pFrame->getFrameImpl());
	if (pImpl && pImpl->getTopLevelWindow())
	{
		gtk_window_set_transient_for(m_pWindow,
									 GTK_WINDOW(pImpl->getTopLevelWindow()));
	}
	gtk_window_set_destroy_with_parent(m_pWindow, TRUE);

	/* ---- header bar ---- */
	GtkWidget * bar = gtk_header_bar_new();

	GtkWidget * btnPrev = gtk_button_new_from_icon_name("go-previous-symbolic");
	gtk_widget_set_tooltip_text(btnPrev, "Previous Page");
	g_object_set_data(G_OBJECT(btnPrev), "nav-dir", GINT_TO_POINTER(-1));
	g_signal_connect(btnPrev, "clicked", G_CALLBACK(_s_pageNav), this);
	gtk_header_bar_pack_start(GTK_HEADER_BAR(bar), btnPrev);

	GtkWidget * btnNext = gtk_button_new_from_icon_name("go-next-symbolic");
	gtk_widget_set_tooltip_text(btnNext, "Next Page");
	g_object_set_data(G_OBJECT(btnNext), "nav-dir", GINT_TO_POINTER(1));
	g_signal_connect(btnNext, "clicked", G_CALLBACK(_s_pageNav), this);
	gtk_header_bar_pack_start(GTK_HEADER_BAR(bar), btnNext);

	GtkAdjustment * pageAdj = gtk_adjustment_new(1, 1, MAX(m_iPages, 1), 1, 5, 0);
	m_pPageEntry = GTK_SPIN_BUTTON(gtk_spin_button_new(pageAdj, 1, 0));
	gtk_spin_button_set_numeric(m_pPageEntry, TRUE);
	gtk_widget_set_tooltip_text(GTK_WIDGET(m_pPageEntry), "Go to page");
	g_signal_connect(m_pPageEntry, "value-changed",
					 G_CALLBACK(_s_pageSpinChanged), this);
	gtk_header_bar_pack_start(GTK_HEADER_BAR(bar), GTK_WIDGET(m_pPageEntry));

	gchar buf[32];
	g_snprintf(buf, sizeof(buf), "/ %d", m_iPages);
	m_pOfLabel = GTK_LABEL(gtk_label_new(buf));
	gtk_header_bar_pack_start(GTK_HEADER_BAR(bar), GTK_WIDGET(m_pOfLabel));

	/* ---- zoom controls ---- */
	GtkWidget * btnOut = gtk_button_new_from_icon_name("zoom-out-symbolic");
	gtk_widget_set_tooltip_text(btnOut, "Zoom Out");
	g_object_set_data(G_OBJECT(btnOut), "zoom-dir", GINT_TO_POINTER(0));
	g_signal_connect(btnOut, "clicked", G_CALLBACK(_s_zoom), this);
	gtk_header_bar_pack_end(GTK_HEADER_BAR(bar), btnOut);

	m_pZoomLabel = GTK_LABEL(gtk_label_new("100%"));
	GtkWidget * zoomBtn = gtk_menu_button_new();
	gtk_menu_button_set_child(GTK_MENU_BUTTON(zoomBtn), GTK_WIDGET(m_pZoomLabel));
	gtk_widget_set_tooltip_text(zoomBtn, "Zoom");

	GtkWidget * pop = gtk_popover_new();
	GtkWidget * popBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	static const int presets[] = { 200, 150, 125, 100, 75, 50, 25 };
	for (int pct : presets)
	{
		gchar lbl[16];
		g_snprintf(lbl, sizeof(lbl), "%d%%", pct);
		GtkWidget * b = gtk_button_new_with_label(lbl);
		gtk_widget_add_css_class(b, "flat");
		g_object_set_data(G_OBJECT(b), "zoom-pct", GINT_TO_POINTER(pct));
		g_signal_connect(b, "clicked", G_CALLBACK(_s_zoom), this);
		g_signal_connect_swapped(b, "clicked",
								 G_CALLBACK(gtk_popover_popdown), pop);
		gtk_box_append(GTK_BOX(popBox), b);
	}
	gtk_popover_set_child(GTK_POPOVER(pop), popBox);
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(zoomBtn), pop);
	gtk_header_bar_pack_end(GTK_HEADER_BAR(bar), zoomBtn);

	GtkWidget * btnIn = gtk_button_new_from_icon_name("zoom-in-symbolic");
	gtk_widget_set_tooltip_text(btnIn, "Zoom In");
	g_object_set_data(G_OBJECT(btnIn), "zoom-dir", GINT_TO_POINTER(1));
	g_signal_connect(btnIn, "clicked", G_CALLBACK(_s_zoom), this);
	gtk_header_bar_pack_end(GTK_HEADER_BAR(bar), btnIn);

	gtk_header_bar_pack_end(GTK_HEADER_BAR(bar),
							gtk_separator_new(GTK_ORIENTATION_VERTICAL));

	/* string-set labels use '&' mnemonics; strip them for button faces */
	auto stripMnemonic = [](const gchar * in) -> gchar * {
		if (!in)
			return g_strdup("");
		gchar * out = g_strdup(in);
		gchar * w = out;
		for (gchar * r = out; *r; ++r)
			if (*r != '&')
				*w++ = *r;
		*w = '\0';
		return out;
	};

	gchar * lblPage = stripMnemonic(
		pSS->getValue(XAP_STRING_ID_DLG_Zoom_WholePage));
	m_pFitPage = GTK_TOGGLE_BUTTON(gtk_toggle_button_new_with_label(lblPage));
	g_free(lblPage);
	gtk_widget_set_tooltip_text(GTK_WIDGET(m_pFitPage), "Fit whole page");
	g_signal_connect(m_pFitPage, "toggled", G_CALLBACK(_s_fitChanged), this);
	gtk_header_bar_pack_end(GTK_HEADER_BAR(bar), GTK_WIDGET(m_pFitPage));

	gchar * lblWidth = stripMnemonic(
		pSS->getValue(XAP_STRING_ID_DLG_Zoom_PageWidth));
	m_pFitWidth = GTK_TOGGLE_BUTTON(gtk_toggle_button_new_with_label(lblWidth));
	g_free(lblWidth);
	gtk_widget_set_tooltip_text(GTK_WIDGET(m_pFitWidth), "Fit page width");
	gtk_toggle_button_set_active(m_pFitWidth, TRUE);
	g_signal_connect(m_pFitWidth, "toggled", G_CALLBACK(_s_fitChanged), this);
	gtk_header_bar_pack_end(GTK_HEADER_BAR(bar), GTK_WIDGET(m_pFitWidth));

	gtk_header_bar_pack_end(GTK_HEADER_BAR(bar),
							gtk_separator_new(GTK_ORIENTATION_VERTICAL));

	GtkWidget * btnClose = gtk_button_new_with_label(
		pSS->getValue(XAP_STRING_ID_DLG_Close));
	g_signal_connect(btnClose, "clicked", G_CALLBACK(_s_closeClicked), this);
	gtk_header_bar_pack_end(GTK_HEADER_BAR(bar), btnClose);

	GtkWidget * btnPrint = gtk_button_new_with_label(
		pSS->getValue(XAP_STRING_ID_DLG_UP_PrintButton));
	gtk_widget_add_css_class(btnPrint, "suggested-action");
	gtk_widget_set_tooltip_text(btnPrint, "Open the print dialog");
	g_signal_connect(btnPrint, "clicked", G_CALLBACK(_s_printClicked), this);
	gtk_header_bar_pack_end(GTK_HEADER_BAR(bar), btnPrint);

	gtk_window_set_titlebar(m_pWindow, bar);

	/* ---- page strip ---- */
	m_pArea = GTK_DRAWING_AREA(gtk_drawing_area_new());
	gtk_drawing_area_set_draw_func(m_pArea, _s_draw, this, nullptr);

	m_pScrolled = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
	gtk_scrolled_window_set_child(m_pScrolled, GTK_WIDGET(m_pArea));
	gtk_widget_set_vexpand(GTK_WIDGET(m_pScrolled), TRUE);
	gtk_widget_set_hexpand(GTK_WIDGET(m_pScrolled), TRUE);

	gtk_window_set_child(m_pWindow, GTK_WIDGET(m_pScrolled));

	m_pVAdj = gtk_scrolled_window_get_vadjustment(m_pScrolled);
	g_signal_connect(m_pVAdj, "value-changed", G_CALLBACK(_s_scrolled), this);

	GtkEventController * keys = gtk_event_controller_key_new();
	g_signal_connect(keys, "key-pressed", G_CALLBACK(_s_key), this);
	gtk_widget_add_controller(GTK_WIDGET(m_pWindow), keys);

	g_signal_connect(m_pWindow, "close-request",
					 G_CALLBACK(_s_closeRequest), this);
}

/*****************************************************************/
/* Entry point                                                    */
/*****************************************************************/

void XAP_UnixDialog_PrintPreview::runModal(XAP_Frame * pFrame)
{
	m_pFrame = pFrame;
	m_bPrintRequested = false;

	_buildDocument();
	if (!m_pPrintView || m_iPages < 1)
	{
		_teardownDocument();
		pFrame->showMessageBox("Could not generate a print preview for this document.",
							   XAP_Dialog_MessageBox::b_O,
							   XAP_Dialog_MessageBox::a_OK);
		return;
	}

	_buildWindow(pFrame);

	/* Initial zoom: fit the page width once the window has a real size. */
	gtk_window_present(m_pWindow);
	_fitZoom();
	_updatePageLabel();

	/* Modal loop matching the XP runModal() contract: block until the
	 * user closes the preview. */
	m_pLoop = g_main_loop_new(nullptr, FALSE);
	g_main_loop_run(m_pLoop);
	g_main_loop_unref(m_pLoop);
	m_pLoop = nullptr;

	gtk_window_destroy(m_pWindow);
	m_pWindow = nullptr;
	m_pArea = nullptr;
	m_pScrolled = nullptr;
	m_pVAdj = nullptr;

	_teardownDocument();

	if (m_bPrintRequested)
	{
		/* Hand off to the normal print dialog. */
		const EV_EditMethodContainer * emc =
			XAP_App::getApp()->getEditMethodContainer();
		EV_EditMethod * em = emc ? emc->findEditMethodByName("print") : nullptr;
		if (em && m_pView)
		{
			EV_EditMethodCallData callData("", 0);
			em->Fn(m_pView, &callData);
		}
	}
}
