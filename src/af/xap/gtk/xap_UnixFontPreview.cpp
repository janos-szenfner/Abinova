/* AbiSource Application Framework
 * Copyright (C) 1998-2000 AbiSource, Inc.
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

#include "xap_Frame.h"
#include "ut_debugmsg.h"
#include "xap_UnixFontPreview.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_UnixDialogHelper.h"
#include "xap_GtkUtils.h"

XAP_UnixFontPreview::XAP_UnixFontPreview(XAP_Frame * pFrame, GtkWidget * attachTo)
	: XAP_FontPreview()
{
	m_pFrame = static_cast<XAP_Frame *>(pFrame);

	// GTK4: no GTK_WINDOW_POPUP or gtk_window_move(); a GtkPopover attached
	// to the font combo does the positioning for us
	m_pPreviewWindow = xap_gtk_popover_new();
	gtk_popover_set_has_arrow(GTK_POPOVER(m_pPreviewWindow), FALSE);

	m_pDrawingArea = gtk_drawing_area_new ();
	gtk_widget_set_size_request(m_pDrawingArea, m_width, m_height);
	gtk_popover_set_child(GTK_POPOVER(m_pPreviewWindow), m_pDrawingArea);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(m_pDrawingArea),
								   s_draw_cb, this, nullptr);

	gtk_widget_set_parent(m_pPreviewWindow, attachTo);
	gtk_popover_popup(GTK_POPOVER(m_pPreviewWindow));

	XAP_App *pApp = XAP_App::getApp();
	GR_UnixCairoAllocInfo ai(GTK_WIDGET(m_pDrawingArea));
	m_gc = (GR_CairoGraphics*) pApp->newGraphics(ai);

	_createFontPreviewFromGC(m_gc, m_width, m_height);
}

XAP_UnixFontPreview::~XAP_UnixFontPreview(void)
{
	DELETEP(m_gc);
	gtk_popover_popdown(GTK_POPOVER(m_pPreviewWindow));
	gtk_widget_unparent(m_pPreviewWindow); // TOPLEVEL
}

void XAP_UnixFontPreview::s_draw_cb(GtkDrawingArea * /*area*/, cairo_t *cr,
									int width, int height, gpointer data)
{
	XAP_UnixFontPreview * self = static_cast<XAP_UnixFontPreview*>(data);
	self->_draw(cr, width, height);
}

void XAP_UnixFontPreview::_draw(cairo_t * cr, int width, int height)
{
	if (!m_pFontPreview || !m_gc)
		return;
	m_gc->setCairo(cr);
	UT_Rect clip(0, 0, m_gc->tlu(width), m_gc->tlu(height));
	m_pFontPreview->drawImmediate(&clip);
	m_gc->setCairo(nullptr);
}
