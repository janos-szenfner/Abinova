/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2019 Hubert Figuière
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
#include "gr_UnixCairoGraphics.h"
#include "fv_View.h"
#include "ap_Ruler.h"

static void
ruler_style_context_changed (GObject* /*w*/, GParamSpec* /*pspec*/,
                             AP_UnixRuler* ruler)
{
    ruler->_ruler_style_context_changed();
}

AP_UnixRuler::AP_UnixRuler(XAP_Frame* pFrame)
    : m_wRuler(nullptr)
    , m_iBackgroundRedrawID(0)
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
    gtk_widget_show(m_wRuler);
    gtk_widget_set_size_request(m_wRuler, w, h);

    g_signal_connect_swapped(G_OBJECT(m_wRuler), "realize",
                             G_CALLBACK(_fe::realize), this);

    g_signal_connect_swapped(G_OBJECT(m_wRuler), "unrealize",
                             G_CALLBACK(_fe::unrealize), this);

    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(m_wRuler),
                                   XAP_UnixCustomWidget::_fe::draw,
                                   static_cast<XAP_UnixCustomWidget *>(this), nullptr);

    GtkGesture *click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
    g_signal_connect(G_OBJECT(click), "pressed",
                     G_CALLBACK(_fe::button_pressed), this);
    g_signal_connect(G_OBJECT(click), "released",
                     G_CALLBACK(_fe::button_released), this);
    gtk_widget_add_controller(m_wRuler, GTK_EVENT_CONTROLLER(click));

    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect(G_OBJECT(motion), "motion",
                     G_CALLBACK(_fe::motion_notify), this);
    gtk_widget_add_controller(m_wRuler, motion);

    g_signal_connect(G_OBJECT(m_wRuler), "resize",
                     G_CALLBACK(_fe::resized), nullptr);

    return m_wRuler;
}

void AP_UnixRuler::_setView(AV_View* pView, GR_UnixCairoGraphics* pG)
{
    UT_ASSERT(gtk_widget_get_realized(m_wRuler));

    pG->setZoomPercentage(pView->getGraphics()->getZoomPercentage());

    GtkWidget* w = gtk_entry_new();
    g_object_ref_sink(w);
    pG->init3dColors(w);
    g_object_unref(w);
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

void AP_UnixRuler::_fe::button_pressed(GtkGestureClick* g, gint /*n_press*/,
                                       gdouble ev_x, gdouble ev_y, gpointer data)
{
    AP_UnixRuler* pRuler = static_cast<AP_UnixRuler *>(data);
    AP_Ruler* ruler = dynamic_cast<AP_Ruler*>(pRuler);
    UT_ASSERT(ruler);

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
                       pG->tlu(static_cast<UT_uint32>(ev_x)),
                       pG->tlu(static_cast<UT_uint32>(ev_y)));
}

void AP_UnixRuler::_fe::button_released(GtkGestureClick* g, gint /*n_press*/,
                                        gdouble ev_x, gdouble ev_y, gpointer data)
{
    AP_UnixRuler* pRuler = static_cast<AP_UnixRuler *>(data);
    AP_Ruler* ruler = dynamic_cast<AP_Ruler*>(pRuler);
    UT_ASSERT(ruler);

    FV_View* pView = static_cast<FV_View*>(ruler->getFrame()->getCurrentView());
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
                         pG->tlu(static_cast<UT_uint32>(ev_x)),
                         pG->tlu(static_cast<UT_uint32>(ev_y)));
}

void AP_UnixRuler::_fe::resized(GtkDrawingArea* w, int width, int height, gpointer /*data*/)
{
    AP_UnixRuler* pRuler = static_cast<AP_UnixRuler *>(g_object_get_data(G_OBJECT(w), "user_data"));
    AP_Ruler* ruler = dynamic_cast<AP_Ruler*>(pRuler);
    UT_nonnull_or_return(ruler,);

    // nb: we'd convert here, but we can't: have no graphics class!
    ruler->setHeight(height);
    ruler->setWidth(width);
}

void AP_UnixRuler::_fe::motion_notify(GtkEventControllerMotion* c,
                                      gdouble ev_x, gdouble ev_y, gpointer data)
{
    AP_UnixRuler* pRuler = static_cast<AP_UnixRuler *>(data);
    AP_Ruler* ruler = dynamic_cast<AP_Ruler*>(pRuler);
    UT_ASSERT(ruler);

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

