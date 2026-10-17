/* AbiSource Program Utilities
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

#pragma once

#include <gdk/gdk.h>

#include "ut_types.h"
#include "ev_Mouse.h"
#include "ev_EditBits.h"

/*****************************************************************/

/* GDK modifier state -> EV modifier mask, shared by every mouse op
 * (click/up/motion/scroll).  Command on macOS arrives as Meta (or
 * Super via XQuartz) and folds into Control -- the same fold the
 * keyboard path applies in ev_UnixKeyboard.cpp -- so Cmd+Click and
 * Cmd+wheel resolve the same bindings as their Ctrl equivalents.
 * Free function (not a member) so the binding unit test can pin it. */
EV_EditModifierState EV_UnixMouse_GdkModsToEMS(GdkModifierType state);

class EV_UnixMouse : public EV_Mouse
{
public:
	EV_UnixMouse(EV_EditEventMapper * pEEM);

	/* x/y are in the coordinate space of the widget the event
	 * controller is attached to. Do NOT use gdk_event_get_position()
	 * here: under GTK4 it reports surface-relative coordinates, which
	 * differ from widget coordinates by the height of the header bar,
	 * menubar/ribbon and rulers above the drawing area. */
	void mouseClick(AV_View* pView, GdkEvent* e, gdouble x, gdouble y, gint n_press);
	void mouseUp(AV_View* pView, GdkEvent* e, gdouble x, gdouble y);
	void mouseMotion(AV_View* pView, GdkEvent *event, gdouble x, gdouble y);
	void mouseScroll(AV_View* pView, GdkEvent *e, gdouble x, gdouble y);
	// GDK_SCROLL_SMOOTH deltas (in wheel-notch units); split out of
	// mouseScroll so the delta path is directly drivable — GDK smooth
	// scroll events cannot be synthesized by the test harness.
	void smoothScrollDeltas(AV_View* pView, gdouble delta_x, gdouble delta_y);

protected:
	// accumulators for GDK_SCROLL_SMOOTH deltas (in wheel-notch units)
	double m_dSmoothScrollX = 0.0;
	double m_dSmoothScrollY = 0.0;
};
