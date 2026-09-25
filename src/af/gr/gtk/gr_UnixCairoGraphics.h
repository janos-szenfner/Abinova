/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
/* Abinova
 * Copyright (C) 2004-2006 Tomas Frydrych <dr.tomas@yahoo.co.uk>
 * Copyright (C) 2009-2021 Hubert Figuière
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

#pragma once

#include <gdk/gdk.h>

#include "gr_CairoGraphics.h"

class ABI_EXPORT GR_UnixCairoAllocInfo : public GR_CairoAllocInfo
{
public:
	GR_UnixCairoAllocInfo(GtkWidget *widget)
		: GR_CairoAllocInfo(false, false, true),
		m_win(widget)
	{}

	GR_UnixCairoAllocInfo(bool bPreview)
		: GR_CairoAllocInfo(bPreview, true, false),
		  m_win(nullptr){}

	cairo_t *createCairo() override {return nullptr;} // we need this since otherwise the class would be abstract
	GtkWidget     * m_win;
};

class ABI_EXPORT GR_UnixCairoGraphicsBase
	: public GR_CairoGraphics
{
 public:
	~GR_UnixCairoGraphicsBase();

	virtual GR_Image*	createNewImage(const char* pszName,
									   const UT_ConstByteBufPtr& pBB,
		                               const std::string& mimetype,
									   UT_sint32 iDisplayWidth,
									   UT_sint32 iDisplayHeight,
									   GR_Image::GRType = GR_Image::GRT_Raster) override;
 protected:
	GR_UnixCairoGraphicsBase();
	GR_UnixCairoGraphicsBase(cairo_t *cr, UT_uint32 iDeviceResolution);

};


class ABI_EXPORT GR_UnixCairoGraphics
	: public GR_UnixCairoGraphicsBase
{
public:
	~GR_UnixCairoGraphics();
	static UT_uint32       s_getClassId() {return GRID_UNIX_PANGO;}
	virtual UT_uint32      getClassId() override {return s_getClassId();}

	static const char *    graphicsDescriptor(){return "Unix Cairo Pango";}
	static GR_Graphics *   graphicsAllocator(GR_AllocInfo&);
	GdkSurface *  getWindow () {return const_cast<GR_UnixCairoGraphics*>(this)->_getWindow();}

	virtual GR_Font * getGUIFont(void) override;

	virtual void		setCursor(GR_Graphics::Cursor c) override;
	virtual void		scroll(UT_sint32, UT_sint32) override;
	virtual void		scroll(UT_sint32 x_dest, UT_sint32 y_dest,
						   UT_sint32 x_src, UT_sint32 y_src,
						   UT_sint32 width, UT_sint32 height) override;
	virtual GR_Image *  genImageFromRectangle(const UT_Rect & r) override;

	void				init3dColors(GtkWidget* w);
	/* override a theme-derived 3D color with an explicit value (used by
	 * the rulers, whose background is a fixed light gray/white) */
	void				override3DColor(GR_Color3D name, const UT_RGBColor & color);
	virtual bool		queryProperties(GR_Graphics::Properties gp) const override;

	/** In the UnixCairoGraphics, color3D are mostly invalid. */
	virtual bool        getColor3D(GR_Color3D name, UT_RGBColor &color) override;

	virtual void		fillRect(GR_Color3D c,
								 UT_sint32 x, UT_sint32 y,
								 UT_sint32 w, UT_sint32 h) override;
	virtual void queueDraw(const UT_Rect* pRect) override;
	virtual void      flush(void) override;

	// Return the cursor name.
	static const char* _getCursor(GR_Graphics::Cursor c);

	/* GTK4 draw callbacks hand us a cairo_t targeting a recording
	 * surface; unlike a GdkWindow it cannot be read back, and caret
	 * save/restore pixel-scraping plus other direct-surface tricks do
	 * not survive it. Instead we keep a persistent image surface that
	 * emulates the old window framebuffer: all painting happens into it
	 * (both inside and outside the draw callback) and the draw callback
	 * blits it to GTK's cairo_t in a single paint. */
	cairo_t *beginFrame();
	void endFrame(cairo_t *gtkCr);

protected:
	void _initWidget();
	virtual void		_resetClip(void) override;
	static void		widget_resize (GtkDrawingArea   *area,
								   int               width,
								   int               height,
								   GR_UnixCairoGraphics *me);
	GR_UnixCairoGraphics(GtkWidget* win = nullptr);
	virtual GdkSurface * _getWindow(void) const;

	virtual void _beginPaint() override;
	virtual void _endPaint() override;

private:
	void ensureBackSurface();

	cairo_surface_t* m_backSurface;
	cairo_t* m_frameCr;
	int m_backW;
	int m_backH;
	bool m_CairoCreated;
	bool m_Painting;
	gulong m_Signal;
	GtkWidget *m_Widget;
	GtkStyleContext* m_styleBg;
	GtkStyleContext* m_styleHighlight;
};
