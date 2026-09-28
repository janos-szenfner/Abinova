/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova — cairo print graphics
 * Copyright (C) 2008 Robert Staudinger
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

#include "gr_CairoPrintGraphics.h"

#include <math.h>

GR_CairoPrintGraphics::GR_CairoPrintGraphics(cairo_t *cr, UT_uint32 iDeviceResolution)
  : GR_UnixCairoGraphicsBase(cr, iDeviceResolution),
	m_bDoShowPage(false),
	m_dResRatio(1.0)
{

}
	
GR_CairoPrintGraphics::~GR_CairoPrintGraphics()
{
	UT_DEBUGMSG(("Deleting CairoPrint graphics %p \n",this));
}


GR_Font * GR_CairoPrintGraphics::getGUIFont(void)
{
    UT_ASSERT_NOT_REACHED ();
    return nullptr;
}

void GR_CairoPrintGraphics::queueDraw(const UT_Rect*)
{
    UT_ASSERT_NOT_REACHED();
}

bool GR_CairoPrintGraphics::queryProperties(GR_Graphics::Properties gp) const
{
	switch (gp)
	{
		case DGP_SCREEN:
		case DGP_OPAQUEOVERLAY:
			return false;
		case DGP_PAPER:
			return true;
		default:
			UT_ASSERT_NOT_REACHED ();
			return false;
	}
}
/*!
 * This number is the ratio of the screen resolution to the printer resolution.
 * It is vital that the method that creates CairoPrintGraphics set this value.
 * Otherwise printing of Maths will look weird.
 *
 * See XAP_UnixDlg_Print::beginPrint(...)
 */
void GR_CairoPrintGraphics::setResolutionRatio(double dres)
{
	/* Reject non-finite and non-positive ratios: they would poison
	 * every font-size conversion downstream and are only produced by
	 * a zero/degenerate screen resolution. */
	if (!isfinite(dres) || dres <= 0.0)
		dres = 1.0;
	m_dResRatio = dres;
}

double GR_CairoPrintGraphics::getResolutionRatio(void) const
{
	return 	m_dResRatio;
}

bool GR_CairoPrintGraphics::startPrint(void)
{
	m_bDoShowPage = false;
	return (m_cr != nullptr);
}

bool GR_CairoPrintGraphics::startPage(const char * /*szPagelabel*/, UT_uint32 /*pageNumber*/,
									  bool /*bPortrait*/, UT_uint32 /*iWidth*/, UT_uint32 /*iHeight*/)
{
	if (m_bDoShowPage && m_cr) {
		cairo_show_page(m_cr);
	}

	m_bDoShowPage = true;

	return (m_cr != nullptr);
}

bool GR_CairoPrintGraphics::endPrint(void)
{
	if (m_bDoShowPage && m_cr) {
		cairo_show_page(m_cr);
		m_bDoShowPage = false;
	}
	return true;
}
