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

#include "ap_UnixRuler.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"
#include "ev_EditBits.h"
#include "ev_EditMethod.h"
#include "gr_UnixCairoGraphics.h"
#include "fv_View.h"
#include "ap_Ruler.h"

static void
ruler_style_context_changed (GObject* /*w*/, GParamSpec* /*pspec*/,
                             AP_UnixRuler* ruler)
{
    ruler->_ruler_style_context_changed();
}

AP_UnixRuler::AP_UnixRuler(XAP_Frame* /*pFrame*/)
    : m_wRuler(nullptr)
    , m_iBackgroundRedrawID(0)
    , m_bDragClaimed(false)
{
    // change ruler color on theme change
    GtkSettings *settings = gtk_settings_get_default();
    m_iBackgroundRedrawID = g_signal_connect(
        G_OBJECT(settings), "notify::gtk-application-prefer-dark-theme",
        G_CALLBACK(ruler_style_context_changed), static_cast<gpointer>(this));
}

void AP_UnixRuler::_aboutToDestroy(XAP_Frame* /*pFrame*/)
{
    GtkSettings *settings = gtk_settings_get_default();
    if (settings && g_signal_handler_is_connected(G_OBJECT(settings), m_iBackgroundRedrawID)) {
        g_signal_handler_disconnect(G_OBJECT(settings), m_iBackgroundRedrawID);
    }
}

void AP_UnixRuler::_ruler_style_context_changed()
{
    AP_Ruler* ruler = dynamic_cast<AP_Ruler*>(this);
    UT_ASSERT(ruler);
    if (ruler) {
        ruler->_refreshView();
    }
}

GtkWidget* AP_UnixRuler::_createWidget(gint w, gint h)
{
    UT_ASSERT(!m_wRuler);

    m_wRuler = gtk_drawing_area_new();

    g_object_set_data(G_OBJECT(m_wRuler), "user_data", this);
    gtk_widget_set_visible(m_wRuler, TRUE);
    gtk_widget_set_size_request(m_wRuler, w, h);

    g_signal_connect_swapped(G_OBJECT(m_wRuler), "realize",
                             G_CALLBACK(_fe::realize), this);

    g_signal_connect_swapped(G_OBJECT(m_wRuler), "unrealize",
                             G_CALLBACK(_fe::unrealize), this);

    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(m_wRuler),
                                   XAP_UnixCustomWidget::_fe::draw,
                                   static_cast<XAP_UnixCustomWidget *>(this), nullptr);

    /* GTK4 has no implicit pointer grab: a button drag that leaves the
     * ruler band stops producing EventControllerMotion "motion" events,
     * which used to silently cancel in-flight marker/tab drags.
     * GtkGestureDrag claims the sequence and keeps reporting drag-update
     * / drag-end (as offsets from the press point) no matter where the
     * pointer goes, so it owns the in-drag motion and the release once
     * a drag starts.  The click gesture still sees every press before
     * any claim (it delivers mousePress, and detects double-click), and
     * its "released" only reaches us for press sequences the drag
     * gesture never claimed — i.e. plain clicks like click-to-add-tab. */
    GtkGesture *click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
    g_signal_connect(G_OBJECT(click), "pressed",
                     G_CALLBACK(_fe::button_pressed), this);
    g_signal_connect(G_OBJECT(click), "released",
                     G_CALLBACK(_fe::button_released), this);
    gtk_widget_add_controller(m_wRuler, GTK_EVENT_CONTROLLER(click));

    GtkGesture *drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), 0);
    g_signal_connect(G_OBJECT(drag), "drag-begin",
                     G_CALLBACK(_fe::drag_begin), this);
    g_signal_connect(G_OBJECT(drag), "drag-update",
                     G_CALLBACK(_fe::drag_update), this);
    g_signal_connect(G_OBJECT(drag), "drag-end",
                     G_CALLBACK(_fe::drag_end), this);
    gtk_widget_add_controller(m_wRuler, GTK_EVENT_CONTROLLER(drag));

    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect(G_OBJECT(motion), "motion",
                     G_CALLBACK(_fe::motion_notify), this);
    gtk_widget_add_controller(m_wRuler, motion);

    g_signal_connect(G_OBJECT(m_wRuler), "resize",
                     G_CALLBACK(_fe::resized), nullptr);

    /* Draws that happen before the widget is mapped only update the
     * backing surface (gtk_widget_queue_draw is a no-op while unmapped),
     * leaving the first, pre-layout paint on screen.  Queue a repaint
     * once the widget maps so the bands are drawn with real metrics. */
    g_signal_connect_swapped(G_OBJECT(m_wRuler), "map",
                             G_CALLBACK(gtk_widget_queue_draw), m_wRuler);

    return m_wRuler;
}

void AP_UnixRuler::_setView(AV_View* pView, GR_UnixCairoGraphics* pG)
{
    UT_ASSERT(gtk_widget_get_realized(m_wRuler));

    pG->setZoomPercentage(pView->getGraphics()->getZoomPercentage());

    pG->init3dColors(m_wRuler);
    /* the ruler background is always a fixed light gray/white, so its
     * text and tick marks must be unconditionally black to stay
     * readable under any theme (incl. dark themes with light text) */
    pG->override3DColor(GR_Graphics::CLR3D_Foreground, UT_RGBColor(0, 0, 0));
}

void AP_UnixRuler::_fe::realize(AP_UnixRuler* self)
{
    UT_ASSERT(!self->_getGraphics());

    GR_UnixCairoAllocInfo ai(self->m_wRuler);
    self->_setGraphics(XAP_App::getApp()->newGraphics(ai));
    UT_ASSERT(self->_getGraphics());
}

void AP_UnixRuler::_fe::unrealize(AP_UnixRuler* self)
{
    UT_ASSERT(self->_getGraphics());
    self->_deleteGraphics();
}

static EV_EditMouseButton s_buttonToEmb(guint button)
{
    if (1 == button) return EV_EMB_BUTTON1;
    if (2 == button) return EV_EMB_BUTTON2;
    if (3 == button) return EV_EMB_BUTTON3;
    return static_cast<EV_EditMouseButton>(0);
}

static EV_EditModifierState s_eventStateToEms(GdkModifierType ev_state)
{
    EV_EditModifierState ems = 0;
    if (ev_state & GDK_SHIFT_MASK)   ems |= EV_EMS_SHIFT;
    if (ev_state & GDK_CONTROL_MASK) ems |= EV_EMS_CONTROL;
    if (ev_state & GDK_ALT_MASK)     ems |= EV_EMS_ALT;
    return ems;
}

void AP_UnixRuler::_fe::button_pressed(GtkGestureClick* g, gint n_press,
                                       gdouble ev_x, gdouble ev_y, gpointer data)
{
    /* "pressed" fires for every button press — including ones that turn
     * into drags — before GtkGestureDrag claims the sequence, so this
     * is the single point that delivers mousePress.  A press that never
     * drags is completed by button_released below; a press that does is
     * completed by drag_end. */
    AP_UnixRuler* pRuler = static_cast<AP_UnixRuler *>(data);
    AP_Ruler* ruler = dynamic_cast<AP_Ruler*>(pRuler);
    UT_nonnull_or_return(ruler,);

    pRuler->m_bDragClaimed = false;

    FV_View* pView = static_cast<FV_View *>(ruler->getFrame()->getCurrentView());
    if (!pView || pView->getPoint() == 0 || !ruler->getGraphics()) {
        return;
    }

    GdkModifierType ev_state = gtk_event_controller_get_current_event_state(
        GTK_EVENT_CONTROLLER(g));
    EV_EditModifierState ems = s_eventStateToEms(ev_state);
    EV_EditMouseButton emb = s_buttonToEmb(gtk_gesture_single_get_current_button(
        GTK_GESTURE_SINGLE(g)));

    auto pG = ruler->getGraphics();
    ruler->mousePress(ems, emb,
                       pG->tlu(static_cast<UT_sint32>(ev_x)),
                       pG->tlu(static_cast<UT_sint32>(ev_y)));

    /* a double-click opens the paragraph dialog (the closest remaining
     * equivalent of Word's ruler double-click — the dedicated Tabs
     * dialog was removed in the classic-menu prune).  It runs modally,
     * so it must come after the press bookkeeping above. */
    if (n_press != 2 || emb != EV_EMB_BUTTON1) {
        return;
    }
    const EV_EditMethodContainer * pEMC =
        XAP_App::getApp()->getEditMethodContainer();
    UT_return_if_fail(pEMC);
    EV_EditMethod * pEM = pEMC->findEditMethodByName("dlgParagraph");
    UT_return_if_fail(pEM);
    EV_EditMethodCallData emcd;
    pEM->Fn(pView, &emcd);
}

void AP_UnixRuler::_fe::button_released(GtkGestureClick* g, gint /*n_press*/,
                                        gdouble ev_x, gdouble ev_y, gpointer data)
{
    /* When GtkGestureDrag claims the press sequence this gesture is
     * reset and "released" is not normally emitted — and even if one
     * still slips through, m_bDragClaimed keeps it from being applied
     * on top of drag-end.  A release that lands here is a plain click:
     * pair it with the press delivered in button_pressed so the XP
     * layer sees the same press/release sequence GTK3 produced (this is
     * what makes click-to-add-tab and the tab-toggle cycler work). */
    AP_UnixRuler* pRuler = static_cast<AP_UnixRuler *>(data);
    AP_Ruler* ruler = dynamic_cast<AP_Ruler*>(pRuler);
    UT_nonnull_or_return(ruler,);

    if (pRuler->m_bDragClaimed) {
        return;
    }

    FV_View* pView = static_cast<FV_View *>(ruler->getFrame()->getCurrentView());
    if (!pView || pView->getPoint() == 0 || !ruler->getGraphics()) {
        return;
    }

    GdkModifierType ev_state = gtk_event_controller_get_current_event_state(
        GTK_EVENT_CONTROLLER(g));
    EV_EditModifierState ems = s_eventStateToEms(ev_state);
    EV_EditMouseButton emb = s_buttonToEmb(gtk_gesture_single_get_current_button(
        GTK_GESTURE_SINGLE(g)));

    auto pG = ruler->getGraphics();
    ruler->mouseRelease(ems, emb,
                         pG->tlu(static_cast<UT_sint32>(ev_x)),
                         pG->tlu(static_cast<UT_sint32>(ev_y)));
}

void AP_UnixRuler::_fe::drag_begin(GtkGestureDrag* /*g*/,
                                   gdouble /*start_x*/, gdouble /*start_y*/,
                                   gpointer data)
{
    /* the press half was already delivered by button_pressed (the drag
     * start point is the press point); this gesture now owns the
     * sequence, so keep button_released from re-applying it. */
    AP_UnixRuler* pRuler = static_cast<AP_UnixRuler *>(data);
    UT_nonnull_or_return(pRuler,);
    pRuler->m_bDragClaimed = true;
}

void AP_UnixRuler::_fe::drag_update(GtkGestureDrag* g,
                                    gdouble offset_x, gdouble offset_y,
                                    gpointer data)
{
    AP_UnixRuler* pRuler = static_cast<AP_UnixRuler *>(data);
    AP_Ruler* ruler = dynamic_cast<AP_Ruler*>(pRuler);
    UT_nonnull_or_return(ruler,);

    FV_View* pView = static_cast<FV_View *>(ruler->getFrame()->getCurrentView());
    if (!pView || pView->getPoint() == 0 || !ruler->getGraphics()) {
        return;
    }

    /* drag-update reports an offset from the press point; recovering the
     * absolute widget position keeps working when the pointer is outside
     * the allocation — negative or > width/height coordinates are what
     * let the XP layer see an off-band drag (and apply or delete on
     * release), so the cast below must stay signed. */
    gdouble start_x = 0.0, start_y = 0.0;
    if (!gtk_gesture_drag_get_start_point(g, &start_x, &start_y)) {
        return;
    }
    const gdouble ev_x = start_x + offset_x;
    const gdouble ev_y = start_y + offset_y;

    GdkModifierType ev_state = gtk_event_controller_get_current_event_state(
        GTK_EVENT_CONTROLLER(g));
    EV_EditModifierState ems = s_eventStateToEms(ev_state);

    auto pG = ruler->getGraphics();
    UT_sint32 x = pG->tlu(static_cast<UT_sint32>(ev_x));
    UT_sint32 y = pG->tlu(static_cast<UT_sint32>(ev_y));
    ruler->mouseMotion(ems, x, y);
    pRuler->_finishMotionEvent(x, y);
}

void AP_UnixRuler::_fe::drag_end(GtkGestureDrag* g,
                                 gdouble offset_x, gdouble offset_y,
                                 gpointer data)
{
    AP_UnixRuler* pRuler = static_cast<AP_UnixRuler *>(data);
    AP_Ruler* ruler = dynamic_cast<AP_Ruler*>(pRuler);
    UT_nonnull_or_return(ruler,);

    FV_View* pView = static_cast<FV_View*>(ruler->getFrame()->getCurrentView());
    if (!pView || pView->getPoint() == 0 || !ruler->getGraphics()) {
        return;
    }

    gdouble start_x = 0.0, start_y = 0.0;
    if (!gtk_gesture_drag_get_start_point(g, &start_x, &start_y)) {
        return;
    }
    const gdouble ev_x = start_x + offset_x;
    const gdouble ev_y = start_y + offset_y;

    GdkModifierType ev_state = gtk_event_controller_get_current_event_state(
        GTK_EVENT_CONTROLLER(g));
    EV_EditModifierState ems = s_eventStateToEms(ev_state);
    EV_EditMouseButton emb = s_buttonToEmb(gtk_gesture_single_get_current_button(
        GTK_GESTURE_SINGLE(g)));

    if (getenv("RULER_TRACE"))
        fprintf(stderr, "drag_end w(%g,%g) ems=%08x emb=%d\n",
                ev_x, ev_y, static_cast<unsigned>(ems),
                static_cast<int>(emb));
    auto pG = ruler->getGraphics();
    ruler->mouseRelease(ems, emb,
                         pG->tlu(static_cast<UT_sint32>(ev_x)),
                         pG->tlu(static_cast<UT_sint32>(ev_y)));
}

void AP_UnixRuler::_fe::resized(GtkDrawingArea* w, int width, int height, gpointer /*data*/)
{
    AP_UnixRuler* pRuler = static_cast<AP_UnixRuler *>(g_object_get_data(G_OBJECT(w), "user_data"));
    AP_Ruler* ruler = dynamic_cast<AP_Ruler*>(pRuler);
    UT_nonnull_or_return(ruler,);

    // nb: we'd convert here, but we can't: have no graphics class!
    ruler->setHeight(height);
    ruler->setWidth(width);
    // The new size invalidates the previous paint; queue a redraw so the
    // bands, ticks and markers are recomputed for the real allocation.
    ruler->queueDraw();
}

void AP_UnixRuler::_fe::motion_notify(GtkEventControllerMotion* c,
                                      gdouble ev_x, gdouble ev_y, gpointer data)
{
    AP_UnixRuler* pRuler = static_cast<AP_UnixRuler *>(data);
    AP_Ruler* ruler = dynamic_cast<AP_Ruler*>(pRuler);
    UT_nonnull_or_return(ruler,);

    XAP_App* pApp = XAP_App::getApp();
    XAP_Frame* pFrame = pApp->getLastFocussedFrame();
    if (pFrame == nullptr) {
        return;
    }

    AV_View * pView = pFrame->getCurrentView();
    if(pView == nullptr || pView->getPoint() == 0 || !ruler->getGraphics()) {
        return;
    }

    GdkModifierType ev_state = gtk_event_controller_get_current_event_state(
        GTK_EVENT_CONTROLLER(c));
    EV_EditModifierState ems = s_eventStateToEms(ev_state);

    // x/y are already relative to the ruler widget in GTK4
    UT_uint32 x = ruler->getGraphics()->tlu(static_cast<UT_uint32>(ev_x));
    UT_uint32 y = ruler->getGraphics()->tlu(static_cast<UT_uint32>(ev_y));
    ruler->mouseMotion(ems, x, y);
    pRuler->_finishMotionEvent(x, y);
}

