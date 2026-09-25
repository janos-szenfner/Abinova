/* GTK - The GIMP Toolkit
 * Copyright © 2012 Carlos Garnacho <carlosg@gnome.org>
 * Copyright (C) 2025-2026 Abinova contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/* GTK4 port: the GTK3 implementation created two child GdkWindows
 * overlaid on the drawing area.  GTK4 has no child surfaces, so each
 * handle is now a small GtkDrawingArea added to a GtkOverlay above
 * the document widget.  Handles are positioned through their
 * margin-start/margin-top properties (with halign/valign START), and
 * dragging is handled by a GtkGestureDrag per handle.
 */

#include "gtktexthandleprivate.h"
#include <gtk/gtk.h>
#include <math.h>

enum: uint8_t {
  HANDLE_DRAGGED,
  DRAG_FINISHED,
  LAST_SIGNAL
};

/* handle widget size, in pixels */
#define HANDLE_WIDTH  24
#define HANDLE_HEIGHT 32

struct HandleWidget
{
  GtkWidget *widget;
  GdkRectangle pointing_to;
  /* pointer (overlay coords) to handle tip adjustment, fixed at drag begin */
  gdouble adj_x;
  gdouble adj_y;
  guint dragged : 1;
  guint mode_visible : 1;
  guint user_visible : 1;
  guint has_point : 1;
};

struct FvTextHandlePrivate
{
  HandleWidget windows[2];
  GtkWidget *overlay;
  guint mode : 2;
};

G_DEFINE_TYPE_WITH_PRIVATE(FvTextHandle, _fv_text_handle, G_TYPE_OBJECT)

static guint signals[LAST_SIGNAL] = { 0 };

static void
_fv_text_handle_draw_cb (GtkDrawingArea *area,
                         cairo_t        *cr,
                         gint            width,
                         gint            height,
                         gpointer        user_data)
{
  FvTextHandlePosition pos = (FvTextHandlePosition)GPOINTER_TO_INT (user_data);
  GtkWidget *widget = GTK_WIDGET (area);
  GdkRGBA color;

#if GTK_CHECK_VERSION(4,10,0)
  gtk_widget_get_color (widget, &color);
#else
  /* gtk_widget_get_color() was added in GTK 4.10; fall back to a
   * fixed selection blue on older versions. */
  color.red = 0.2; color.green = 0.45; color.blue = 0.9; color.alpha = 1.0;
#endif

  /* Draw a "teardrop" handle: a bulb with a stem pointing at the text.
   * SELECTION_START hangs above the text (tip down, bulb on top);
   * CURSOR/SELECTION_END sits below the text (tip up, bulb below). */
  double cx = width / 2.0;
  double radius = MIN (width / 2.0, height / 2.0) - 1.0;
  double cy, tip_y, dir;

  if (pos == FV_TEXT_HANDLE_POSITION_SELECTION_START)
    {
      cy = radius + 1.0;
      tip_y = height - 1.0;
      dir = 1.0;
    }
  else
    {
      cy = height - radius - 1.0;
      tip_y = 1.0;
      dir = -1.0;
    }

  cairo_set_source_rgba (cr, color.red, color.green, color.blue, 0.9);

  cairo_new_path (cr);
  cairo_arc (cr, cx, cy, radius, 0, 2 * M_PI);
  cairo_fill (cr);

  /* stem: triangle from a chord of the bulb to the tip */
  cairo_move_to (cr, cx - radius * 0.75, cy + dir * radius * 0.45);
  cairo_line_to (cr, cx, tip_y);
  cairo_line_to (cr, cx + radius * 0.75, cy + dir * radius * 0.45);
  cairo_close_path (cr);
  cairo_fill (cr);
}

static void
_fv_text_handle_update_widget_state (FvTextHandle         *handle,
                                     FvTextHandlePosition  pos)
{
  FvTextHandlePrivate *priv = handle->priv;
  HandleWidget *handle_widget = &priv->windows[pos];

  if (!handle_widget->widget)
    return;

  if (handle_widget->has_point &&
      handle_widget->mode_visible && handle_widget->user_visible)
    {
      gint x, y;

      x = handle_widget->pointing_to.x - HANDLE_WIDTH / 2;
      if (pos == FV_TEXT_HANDLE_POSITION_SELECTION_START)
        y = handle_widget->pointing_to.y - HANDLE_HEIGHT;
      else
        y = handle_widget->pointing_to.y + handle_widget->pointing_to.height;

      gtk_widget_set_margin_start (handle_widget->widget, x);
      gtk_widget_set_margin_top (handle_widget->widget, y);
      gtk_widget_set_visible (handle_widget->widget, TRUE);
    }
  else
    gtk_widget_set_visible (handle_widget->widget, FALSE);
}

static void
_fv_text_handle_emit_at_pointer (FvTextHandle         *handle,
                                 FvTextHandlePosition  pos,
                                 GtkGestureDrag       *gesture)
{
  FvTextHandlePrivate *priv = handle->priv;
  HandleWidget *handle_widget = &priv->windows[pos];
  gdouble px, py, ox, oy;

  if (!handle_widget->widget)
    return;

  /* pointer position in overlay coordinates */
  if (!gtk_gesture_get_point (GTK_GESTURE (gesture), nullptr, &px, &py))
    gtk_gesture_drag_get_start_point (gesture, &px, &py);

  if (!gtk_widget_translate_coordinates (handle_widget->widget,
                                         priv->overlay,
                                         px, py, &ox, &oy))
    return;

  g_signal_emit (handle, signals[HANDLE_DRAGGED], 0, pos,
                 (gint) (ox + handle_widget->adj_x),
                 (gint) (oy + handle_widget->adj_y));
}

static void
_fv_text_handle_drag_begin (GtkGestureDrag *gesture,
                            gdouble         start_x,
                            gdouble         start_y,
                            gpointer        user_data)
{
  FvTextHandle *handle = FV_TEXT_HANDLE (user_data);
  FvTextHandlePrivate *priv = handle->priv;
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  FvTextHandlePosition pos;
  HandleWidget *handle_widget;
  gdouble px, py, tip_x, tip_y;

  if (widget == priv->windows[FV_TEXT_HANDLE_POSITION_SELECTION_START].widget)
    pos = FV_TEXT_HANDLE_POSITION_SELECTION_START;
  else
    pos = FV_TEXT_HANDLE_POSITION_CURSOR;

  handle_widget = &priv->windows[pos];
  handle_widget->dragged = TRUE;

  if (!gtk_widget_translate_coordinates (widget, priv->overlay,
                                         start_x, start_y, &px, &py))
    {
      px = start_x;
      py = start_y;
    }

  /* emitted coords are the position the handle tip points at */
  tip_x = handle_widget->pointing_to.x;
  tip_y = handle_widget->pointing_to.y;
  if (pos == FV_TEXT_HANDLE_POSITION_CURSOR)
    tip_y += handle_widget->pointing_to.height;

  handle_widget->adj_x = tip_x - px;
  handle_widget->adj_y = tip_y - py;
}

static void
_fv_text_handle_drag_update (GtkGestureDrag *gesture,
                             gdouble         /*offset_x*/,
                             gdouble         /*offset_y*/,
                             gpointer        user_data)
{
  FvTextHandle *handle = FV_TEXT_HANDLE (user_data);
  FvTextHandlePrivate *priv = handle->priv;
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  FvTextHandlePosition pos;

  if (widget == priv->windows[FV_TEXT_HANDLE_POSITION_SELECTION_START].widget)
    pos = FV_TEXT_HANDLE_POSITION_SELECTION_START;
  else
    pos = FV_TEXT_HANDLE_POSITION_CURSOR;

  if (!priv->windows[pos].dragged)
    return;

  _fv_text_handle_emit_at_pointer (handle, pos, gesture);
}

static void
_fv_text_handle_drag_end (GtkGestureDrag *gesture,
                          gdouble         /*offset_x*/,
                          gdouble         /*offset_y*/,
                          gpointer        user_data)
{
  FvTextHandle *handle = FV_TEXT_HANDLE (user_data);
  FvTextHandlePrivate *priv = handle->priv;
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  FvTextHandlePosition pos;

  if (widget == priv->windows[FV_TEXT_HANDLE_POSITION_SELECTION_START].widget)
    pos = FV_TEXT_HANDLE_POSITION_SELECTION_START;
  else
    pos = FV_TEXT_HANDLE_POSITION_CURSOR;

  priv->windows[pos].dragged = FALSE;
  g_signal_emit (handle, signals[DRAG_FINISHED], 0, pos);
}

static GtkWidget *
_fv_text_handle_create_widget (FvTextHandle         *handle,
                               FvTextHandlePosition  pos)
{
  GtkWidget *widget;
  GtkGesture *gesture;

  widget = gtk_drawing_area_new ();
  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (widget), HANDLE_WIDTH);
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (widget), HANDLE_HEIGHT);
  gtk_widget_set_halign (widget, GTK_ALIGN_START);
  gtk_widget_set_valign (widget, GTK_ALIGN_START);
  /* force LTR so that margin-start always means the left edge */
  gtk_widget_set_direction (widget, GTK_TEXT_DIR_LTR);
  gtk_widget_set_visible (widget, FALSE);

  gtk_widget_add_css_class (widget, "cursor-handle");
  gtk_widget_add_css_class (widget,
                            (pos == FV_TEXT_HANDLE_POSITION_SELECTION_START)
                            ? "top" : "bottom");

  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (widget),
                                  _fv_text_handle_draw_cb,
                                  GINT_TO_POINTER ((int) pos),
                                  nullptr);

  gesture = gtk_gesture_drag_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "drag-begin",
                    G_CALLBACK (_fv_text_handle_drag_begin), handle);
  g_signal_connect (gesture, "drag-update",
                    G_CALLBACK (_fv_text_handle_drag_update), handle);
  g_signal_connect (gesture, "drag-end",
                    G_CALLBACK (_fv_text_handle_drag_end), handle);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gesture));

  return widget;
}

static void
_fv_text_handle_update_windows (FvTextHandle *handle)
{
  _fv_text_handle_update_widget_state (handle, FV_TEXT_HANDLE_POSITION_CURSOR);
  _fv_text_handle_update_widget_state (handle, FV_TEXT_HANDLE_POSITION_SELECTION_START);
}

static void
fv_text_handle_finalize (GObject *object)
{
  FvTextHandlePrivate *priv;

  priv = FV_TEXT_HANDLE (object)->priv;

  if (priv->overlay)
    {
      for (int i = 0; i < 2; i++)
        {
          if (priv->windows[i].widget)
            {
              g_object_remove_weak_pointer (G_OBJECT (priv->windows[i].widget),
                                            (gpointer *) &priv->windows[i].widget);
              gtk_overlay_remove_overlay (GTK_OVERLAY (priv->overlay),
                                          priv->windows[i].widget);
            }
        }
      g_object_unref (priv->overlay);
    }

  G_OBJECT_CLASS (_fv_text_handle_parent_class)->finalize (object);
}

static void
_fv_text_handle_class_init (FvTextHandleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = fv_text_handle_finalize;

  signals[HANDLE_DRAGGED] =
    g_signal_new ("handle-dragged",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (FvTextHandleClass, handle_dragged),
		  nullptr, nullptr,
                  g_cclosure_marshal_generic,
		  G_TYPE_NONE, 3,
                  G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);
  signals[DRAG_FINISHED] =
    g_signal_new ("drag-finished",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST, 0,
		  nullptr, nullptr,
                  g_cclosure_marshal_VOID__INT,
                  G_TYPE_NONE, 1, G_TYPE_INT);
}

static void
_fv_text_handle_init (FvTextHandle *handle)
{
  handle->priv = (FvTextHandlePrivate *)_fv_text_handle_get_instance_private (handle);
}

FvTextHandle *
_fv_text_handle_new (GtkWidget *overlay)
{
  FvTextHandle *handle;
  FvTextHandlePrivate *priv;

  g_return_val_if_fail (GTK_IS_OVERLAY (overlay), nullptr);

  handle = (FvTextHandle *) g_object_new (FV_TYPE_TEXT_HANDLE, nullptr);
  priv = handle->priv;
  priv->overlay = GTK_WIDGET (g_object_ref (overlay));

  priv->windows[FV_TEXT_HANDLE_POSITION_CURSOR].widget =
    _fv_text_handle_create_widget (handle, FV_TEXT_HANDLE_POSITION_CURSOR);
  priv->windows[FV_TEXT_HANDLE_POSITION_SELECTION_START].widget =
    _fv_text_handle_create_widget (handle, FV_TEXT_HANDLE_POSITION_SELECTION_START);

  /* the overlay owns the widgets; keep weak pointers so a torn-down
   * widget tree cannot leave us holding dangling pointers */
  g_object_add_weak_pointer (G_OBJECT (priv->windows[FV_TEXT_HANDLE_POSITION_CURSOR].widget),
                             (gpointer *) &priv->windows[FV_TEXT_HANDLE_POSITION_CURSOR].widget);
  g_object_add_weak_pointer (G_OBJECT (priv->windows[FV_TEXT_HANDLE_POSITION_SELECTION_START].widget),
                             (gpointer *) &priv->windows[FV_TEXT_HANDLE_POSITION_SELECTION_START].widget);

  gtk_overlay_add_overlay (GTK_OVERLAY (overlay),
                           priv->windows[FV_TEXT_HANDLE_POSITION_CURSOR].widget);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay),
                           priv->windows[FV_TEXT_HANDLE_POSITION_SELECTION_START].widget);

  return handle;
}

void
_fv_text_handle_set_mode (FvTextHandle     *handle,
                          FvTextHandleMode  mode)
{
  FvTextHandlePrivate *priv;

  g_return_if_fail (FV_IS_TEXT_HANDLE (handle));

  priv = handle->priv;

  if (priv->mode == mode)
    return;

  priv->mode = mode;

  switch (mode)
    {
    case FV_TEXT_HANDLE_MODE_CURSOR:
      priv->windows[FV_TEXT_HANDLE_POSITION_CURSOR].mode_visible = TRUE;
      priv->windows[FV_TEXT_HANDLE_POSITION_SELECTION_START].mode_visible = FALSE;
      if (priv->windows[FV_TEXT_HANDLE_POSITION_CURSOR].widget)
        gtk_widget_add_css_class (priv->windows[FV_TEXT_HANDLE_POSITION_CURSOR].widget,
                                  "insertion-cursor");
      break;
    case FV_TEXT_HANDLE_MODE_SELECTION:
      priv->windows[FV_TEXT_HANDLE_POSITION_CURSOR].mode_visible = TRUE;
      priv->windows[FV_TEXT_HANDLE_POSITION_SELECTION_START].mode_visible = TRUE;
      if (priv->windows[FV_TEXT_HANDLE_POSITION_CURSOR].widget)
        gtk_widget_remove_css_class (priv->windows[FV_TEXT_HANDLE_POSITION_CURSOR].widget,
                                     "insertion-cursor");
      break;
    case FV_TEXT_HANDLE_MODE_NONE:
    default:
      priv->windows[FV_TEXT_HANDLE_POSITION_CURSOR].mode_visible = FALSE;
      priv->windows[FV_TEXT_HANDLE_POSITION_SELECTION_START].mode_visible = FALSE;
      break;
    }

  _fv_text_handle_update_windows (handle);
}

FvTextHandleMode
_fv_text_handle_get_mode (FvTextHandle *handle)
{
  g_return_val_if_fail (FV_IS_TEXT_HANDLE (handle), FV_TEXT_HANDLE_MODE_NONE);

  return (FvTextHandleMode)handle->priv->mode;
}

void
_fv_text_handle_set_position (FvTextHandle         *handle,
                              FvTextHandlePosition  pos,
                              const GdkRectangle   *rect)
{
  FvTextHandlePrivate *priv;
  HandleWidget *handle_widget;

  g_return_if_fail (FV_IS_TEXT_HANDLE (handle));
  g_return_if_fail (rect != nullptr);

  priv = handle->priv;
  pos = (FvTextHandlePosition) CLAMP ((int) pos, FV_TEXT_HANDLE_POSITION_CURSOR,
                                      FV_TEXT_HANDLE_POSITION_SELECTION_START);
  handle_widget = &priv->windows[pos];

  if (priv->mode == FV_TEXT_HANDLE_MODE_NONE ||
      (priv->mode == FV_TEXT_HANDLE_MODE_CURSOR &&
       pos != FV_TEXT_HANDLE_POSITION_CURSOR))
    return;

  handle_widget->pointing_to = *rect;
  handle_widget->has_point = TRUE;

  _fv_text_handle_update_widget_state (handle, pos);
}

void
_fv_text_handle_set_visible (FvTextHandle         *handle,
                             FvTextHandlePosition  pos,
                             gboolean              visible)
{
  FvTextHandlePrivate *priv;

  g_return_if_fail (FV_IS_TEXT_HANDLE (handle));

  priv = handle->priv;
  pos = (FvTextHandlePosition) CLAMP ((int) pos, FV_TEXT_HANDLE_POSITION_CURSOR,
                                      FV_TEXT_HANDLE_POSITION_SELECTION_START);

  if (priv->windows[pos].dragged)
    return;

  priv->windows[pos].user_visible = visible;
  _fv_text_handle_update_widget_state (handle, pos);
}

gboolean
_fv_text_handle_get_is_dragged (FvTextHandle         *handle,
                                FvTextHandlePosition  pos)
{
  FvTextHandlePrivate *priv;

  g_return_val_if_fail (FV_IS_TEXT_HANDLE (handle), FALSE);

  priv = handle->priv;
  pos = (FvTextHandlePosition) CLAMP ((int) pos, FV_TEXT_HANDLE_POSITION_CURSOR,
                                      FV_TEXT_HANDLE_POSITION_SELECTION_START);

  return priv->windows[pos].dragged;
}
