/* AbiSource Application Framework
 * Copyright (C) 1998-2000 AbiSource, Inc.
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

#include "xap_Frame.h"
#include "xap_GtkUtils.h"
#include "xap_UnixFrameImpl.h"
#include "ut_debugmsg.h"
#include "ap_UnixPreview_Annotation.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_UnixDialogHelper.h"

AP_UnixPreview_Annotation::AP_UnixPreview_Annotation(XAP_DialogFactory * pDlgFactory,XAP_Dialog_Id id) : AP_Preview_Annotation(pDlgFactory,id),
  m_gc(nullptr),
  m_pPreviewWindow(nullptr),
  m_pDrawingArea(nullptr)
{
	UT_DEBUGMSG(("AP_UnixPreview_Annotation: Preview annotation for Unix platform\n"));
}

AP_UnixPreview_Annotation::~AP_UnixPreview_Annotation(void)
{
	UT_DEBUGMSG(("Preview Annotation deleted %p \n",this));
	destroy();
}

void AP_UnixPreview_Annotation::runModeless(XAP_Frame * pFrame)
{
	UT_DEBUGMSG(("Preview Annotation runModeless %p \n",this));
	setActiveFrame(pFrame);
	if(m_pPreviewWindow)
	{
		DELETEP(m_gc);
		abiDestroyWidget(m_pPreviewWindow);
		m_pPreviewWindow = nullptr;
		m_pDrawingArea = nullptr;
	}
	setSizeFromAnnotation();
	_constructWindow();

	// make a new Unix GC
	DELETEP(m_gc);
	
	XAP_App *pApp = XAP_App::getApp();
	GR_UnixCairoAllocInfo ai(GTK_WIDGET(m_pDrawingArea));
	m_gc = (GR_CairoGraphics*) pApp->newGraphics(ai);

	_createAnnotationPreviewFromGC(m_gc, m_width, m_height);
	m_gc->setZoomPercentage(100);
	gtk_widget_show(m_pDrawingArea);
}

void AP_UnixPreview_Annotation::activate(void)
{
	UT_return_if_fail(m_pPreviewWindow);
	gtk_popover_popup(GTK_POPOVER(m_pPreviewWindow));
}

static void s_preview_draw(GtkDrawingArea * /*area*/, cairo_t *cr,
						   int /*width*/, int /*height*/, gpointer data)
{
	AP_UnixPreview_Annotation *self = static_cast<AP_UnixPreview_Annotation*>(data);
	UT_return_if_fail(self);
	static_cast<GR_CairoGraphics*>(self->getGraphics())->setCairo(cr);
	self->drawImmediate();
	static_cast<GR_CairoGraphics*>(self->getGraphics())->setCairo(nullptr);
}

XAP_Dialog * AP_UnixPreview_Annotation::static_constructor(XAP_DialogFactory * pFactory, XAP_Dialog_Id id)
{
	return new AP_UnixPreview_Annotation(pFactory,id);
}

void  AP_UnixPreview_Annotation::_constructWindow(void)
{
	XAP_App::getApp()->rememberModelessId(getDialogId(), static_cast<XAP_Dialog_Modeless *>(this));
	UT_DEBUGMSG(("Contructing Window width %d height %d left %d top %d \n",m_width,m_height,m_left,m_top));
	m_pPreviewWindow = xap_gtk_popover_new();
	gtk_widget_set_size_request(m_pPreviewWindow, m_width, m_height);
	m_pDrawingArea = gtk_drawing_area_new();
	gtk_widget_show(GTK_WIDGET(m_pDrawingArea));
	gtk_popover_set_child(GTK_POPOVER(m_pPreviewWindow), m_pDrawingArea);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(m_pDrawingArea),
								   s_preview_draw, this, nullptr);

	GtkWidget *parent = getActiveFrame() && getActiveFrame()->getFrameImpl()
		? static_cast<XAP_UnixFrameImpl*>(getActiveFrame()->getFrameImpl())->getViewWidget()
		: nullptr;
	if (parent) {
		gtk_widget_set_parent(m_pPreviewWindow, parent);
		GdkRectangle rect = { m_left, m_top - (m_height/2 + m_Offset), 1, 1 };
		gtk_popover_set_pointing_to(GTK_POPOVER(m_pPreviewWindow), &rect);
		gtk_popover_popup(GTK_POPOVER(m_pPreviewWindow));
	}
}

void  AP_UnixPreview_Annotation::destroy(void)
{
	modeless_cleanup();

	if (!m_pPreviewWindow)
		return;
	
	DELETEP(m_gc);
	abiDestroyWidget(m_pPreviewWindow); // TOPLEVEL
	m_pPreviewWindow = nullptr;
	m_pDrawingArea = nullptr;
}



