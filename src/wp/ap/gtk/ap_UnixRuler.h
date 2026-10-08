/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* Abinova
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2019 Hubert Figuière
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

#include "gr_Graphics.h"
#include "xap_UnixCustomWidget.h"

class AV_View;
class GR_UnixCairoGraphics;
class XAP_Frame;

/**
 * This is the Gtk3 implementation for the rulers.
 * There was a lot of cut&paste between AP_UnixLeftRuler and AP_UnixTopRuler.
 *
 */
class AP_UnixRuler
    : virtual public XAP_UnixCustomWidget
{
public:
    AP_UnixRuler(XAP_Frame* pFrame);
    virtual ~AP_UnixRuler() {}

    virtual GtkWidget* getWidget() override
        { return m_wRuler; }

    void _ruler_style_context_changed();
protected:
    GtkWidget* _createWidget(gint w, gint h);
    void _setView(AV_View * pView, GR_UnixCairoGraphics* pG);
    void _aboutToDestroy(XAP_Frame* pFrame);

    virtual XAP_Frame* _getFrame() const = 0;
    virtual GR_Graphics* _getGraphics() const = 0;
    virtual void _setGraphics(GR_Graphics* pG) = 0;
    void _deleteGraphics()
        {
            delete _getGraphics();
            _setGraphics(nullptr);
        }
    virtual void _finishMotionEvent(UT_uint32 x, UT_uint32 y) = 0;

    class _fe
    {
    public:
        static void realize(AP_UnixRuler *self);
        static void unrealize(AP_UnixRuler *self);
        static void button_pressed(GtkGestureClick *g, gint n_press, gdouble x, gdouble y, gpointer data);
        static void button_released(GtkGestureClick *g, gint n_press, gdouble x, gdouble y, gpointer data);
        static void drag_begin(GtkGestureDrag *g, gdouble x, gdouble y, gpointer data);
        static void drag_update(GtkGestureDrag *g, gdouble offset_x, gdouble offset_y, gpointer data);
        static void drag_cancel(GtkGesture *g, GdkEventSequence *sequence, gpointer data);
        static void drag_end(GtkGestureDrag *g, gdouble offset_x, gdouble offset_y, gpointer data);
        static void resized(GtkDrawingArea* w, int width, int height, gpointer data);
        static void motion_notify(GtkEventControllerMotion* c, gdouble x, gdouble y, gpointer data);
    };

    GtkWidget* m_wRuler;
    guint m_iBackgroundRedrawID;
    /* set by drag-begin once GtkGestureDrag claims the press sequence;
     * the click gesture is reset on claim and normally emits no
     * "released", but if one still reaches us this keeps the release
     * from being applied twice (drag-end already did it).  Cleared on
     * the next button press. */
    bool m_bDragClaimed;
    /* Drag bookkeeping cached at drag-begin/each drag-update.  A
     * synthetic buttonless MOTION_NOTIFY (GDK's
     * gdk_surface_ensure_motion, synthesized when a layout change needs
     * pointer-focus refresh) can cancel the gesture mid-drag; the
     * cancel path still emits drag-end but with a stale/zero offset and
     * gtk_gesture_drag_get_start_point() failing, so the release used
     * to be silently dropped.  m_dragLastX/Y hold the last absolute
     * tracked position and are the release point on the cancel path
     * (the pointer is where tracking last saw it); on a real
     * ButtonRelease the emitted offset + m_dragStartX/Y is exact.
     * m_bDragBegun is set at drag-begin and cleared when the release is
     * delivered, so a begun drag releases exactly once even after a
     * cancel — it must NOT be reset on press, since GTK4 can emit
     * drag-begin before the click controller's "pressed" for the same
     * sequence. */
    double m_dragStartX;
    double m_dragStartY;
    double m_dragLastX;
    double m_dragLastY;
    bool m_bDragBegun;
};
