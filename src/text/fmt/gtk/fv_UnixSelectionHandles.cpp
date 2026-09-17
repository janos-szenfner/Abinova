/* AbiWord - unix impl for selection handles
 * Copyright (c) 2012 One laptop per child
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
 *
 * Author: Carlos Garnacho <carlos@lanedo.com>
 */

/*
 * GTK4 port note: the original implementation (gtktexthandle.cpp)
 * created child GdkWindows overlaid on the drawing area.  GTK4 has no
 * child surfaces, so the text handles need to be reimplemented as
 * overlay widgets (e.g. inside a GtkOverlay over the drawing area).
 * Until that rewrite happens the selection handles are disabled;
 * text selection itself is unaffected.
 */

#include "fv_UnixSelectionHandles.h"

FV_UnixSelectionHandles::FV_UnixSelectionHandles(FV_View *view, FV_Selection selection)
	: FV_SelectionHandles (view, selection)
{
}

FV_UnixSelectionHandles::~FV_UnixSelectionHandles()
{
}

void FV_UnixSelectionHandles::hide()
{
}

void FV_UnixSelectionHandles::setCursorCoords(UT_sint32 /*x*/, UT_sint32 /*y*/, UT_uint32 /*height*/, bool /*visible*/)
{
}

void FV_UnixSelectionHandles::setSelectionCoords(UT_sint32 /*start_x*/, UT_sint32 /*start_y*/, UT_uint32 /*start_height*/, bool /*start_visible*/,
                                                 UT_sint32 /*end_x*/, UT_sint32 /*end_y*/, UT_uint32 /*end_height*/, bool /*end_visible*/)
{
}
