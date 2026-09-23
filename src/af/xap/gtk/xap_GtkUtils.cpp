/* AbiWord
 * Copyright (C) 2011-2016 Hubert Figuiere
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


#include <gtk/gtk.h>

#include "xap_GtkUtils.h"

void XAP_gtk_window_raise(GtkWidget* w)
{
  GtkNative *native = gtk_widget_get_native(w);
  if (native && GTK_IS_WINDOW(native))
    gtk_window_present(GTK_WINDOW(native));
}

gchar* xap_gtk_file_chooser_get_filename(GtkFileChooser* chooser)
{
  GFile *file = gtk_file_chooser_get_file(chooser);
  if (!file)
    return nullptr;
  gchar *path = g_file_get_path(file);
  g_object_unref(file);
  return path;
}

gchar* xap_gtk_file_chooser_get_uri(GtkFileChooser* chooser)
{
  GFile *file = gtk_file_chooser_get_file(chooser);
  if (!file)
    return nullptr;
  gchar *uri = g_file_get_uri(file);
  g_object_unref(file);
  return uri;
}

void xap_gtk_container_add(GtkWidget* container, GtkWidget* child)
{
  if (GTK_IS_SCROLLED_WINDOW(container))
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(container), child);
  else if (GTK_IS_WINDOW(container))
    gtk_window_set_child(GTK_WINDOW(container), child);
  else if (GTK_IS_FRAME(container))
    gtk_frame_set_child(GTK_FRAME(container), child);
  else if (GTK_IS_BOX(container))
    gtk_box_append(GTK_BOX(container), child);
  else if (GTK_IS_POPOVER(container))
    gtk_popover_set_child(GTK_POPOVER(container), child);
  else if (GTK_IS_NOTEBOOK(container))
    gtk_notebook_append_page(GTK_NOTEBOOK(container), child, nullptr);
  else if (GTK_IS_GRID(container))
    gtk_grid_attach(GTK_GRID(container), child, 0, 0, 1, 1);
  else if (GTK_IS_EXPANDER(container))
    gtk_expander_set_child(GTK_EXPANDER(container), child);
  else if (GTK_IS_OVERLAY(container))
    gtk_overlay_set_child(GTK_OVERLAY(container), child);
  else
    gtk_widget_set_parent(child, container);
}

void xap_gtk_container_remove(GtkWidget* /*container*/, GtkWidget* child)
{
  gtk_widget_unparent(child);
}

void XAP_gtk_widget_set_margin(GtkWidget* w, gint margin)
{
  gtk_widget_set_margin_bottom(w, margin);
  gtk_widget_set_margin_top(w, margin);
  gtk_widget_set_margin_start(w, margin);
  gtk_widget_set_margin_end(w, margin);
}


static void s_popover_root_active_cb(GObject * root, GParamSpec *,
                                     gpointer data)
{
  if (!gtk_window_is_active(GTK_WINDOW(root)))
    gtk_popover_popdown(GTK_POPOVER(data));
}

static gboolean s_popover_event_cb(GtkEventControllerLegacy * ctl,
                                   GdkEvent * ev, gpointer data)
{
  if (gdk_event_get_event_type(ev) != GDK_BUTTON_PRESS)
    return FALSE;
  GtkWidget * pop = GTK_WIDGET(data);
  /* presses inside the popover itself also travel up the widget tree
   * (popover -> menubutton -> toplevel): skip them by surface so the
   * popover's own rows can handle their click before we pop down */
  if (gdk_event_get_surface(ev) ==
      gtk_native_get_surface(GTK_NATIVE(pop)))
    return FALSE;
  /* the popover lives on its own surface under XWayland, so any
   * press reaching the toplevel is by definition outside it; let the
   * popover's own toggle button handle its press itself, though */
  GtkWidget * parent = gtk_widget_get_parent(pop);
  double x, y;
  if (parent && gdk_event_get_position(ev, &x, &y))
  {
    graphene_point_t pt;
    graphene_point_t from;
    graphene_point_init(&from, static_cast<float>(x),
                        static_cast<float>(y));
    if (gtk_widget_compute_point(
          gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(ctl)),
          parent, &from, &pt) &&
        pt.x >= 0 && pt.y >= 0 &&
        pt.x <= gtk_widget_get_width(parent) &&
        pt.y <= gtk_widget_get_height(parent))
      return FALSE;
  }
  gtk_popover_popdown(GTK_POPOVER(pop));
  return FALSE;
}

static void s_popover_unmap_cb(GtkWidget * pop, gpointer)
{
  GtkEventController * ctl = GTK_EVENT_CONTROLLER(
    g_object_get_data(G_OBJECT(pop), "xap-click-ctl"));
  GtkRoot * root = gtk_widget_get_root(pop);
  if (ctl && GTK_IS_WIDGET(root))
    gtk_widget_remove_controller(GTK_WIDGET(root), ctl);
  g_object_set_data(G_OBJECT(pop), "xap-click-ctl", nullptr);
}

static gboolean s_popover_add_click_ctl_idle(gpointer data)
{
  GtkWidget * pop = GTK_WIDGET(data);
  if (g_object_get_data(G_OBJECT(pop), "xap-click-ctl") ||
      !gtk_widget_get_mapped(pop))
    return G_SOURCE_REMOVE;
  GtkRoot * root = gtk_widget_get_root(pop);
  if (!GTK_IS_WIDGET(root))
    return G_SOURCE_REMOVE;
  /* the seat-grab autohide relies on does not work under rootless
   * XWayland (outside presses are swallowed without dismissing), so
   * dismiss on any press that reaches the toplevel while the popover
   * is mapped */
  /* a legacy controller observes the raw events without claiming
   * them, so presses still reach their real targets (the popover's
   * toggle button, other buttons, the document) while we pop down */
  GtkEventController * ctl = gtk_event_controller_legacy_new();
  gtk_event_controller_set_propagation_phase(ctl, GTK_PHASE_CAPTURE);
  g_signal_connect(ctl, "event",
                   G_CALLBACK(s_popover_event_cb), pop);
  gtk_widget_add_controller(GTK_WIDGET(root), ctl);
  g_object_set_data(G_OBJECT(pop), "xap-click-ctl", ctl);
  return G_SOURCE_REMOVE;
}

static void s_popover_map_cb(GtkWidget * pop, gpointer)
{
  GtkRoot * root = gtk_widget_get_root(pop);
  if (!GTK_IS_WIDGET(root))
    return;
  /* once the popover is attached to a window, watch that window's
   * activation: when the user switches to another application the
   * popover must go away instead of hovering above the other app */
  g_signal_connect_object(root, "notify::is-active",
                          G_CALLBACK(s_popover_root_active_cb),
                          pop, (GConnectFlags)0);
  /* install the outside-press watcher on idle: the press that opened
   * the popover is still being dispatched right now, and a controller
   * added mid-sequence sees a phantom (0,0) press */
  g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                  s_popover_add_click_ctl_idle,
                  g_object_ref(pop),
                  reinterpret_cast<GDestroyNotify>(g_object_unref));
}

GtkWidget* xap_gtk_popover_new(void)
{
  GtkWidget * pop = gtk_popover_new();
  /* no grab: under rootless XWayland the autohide grab swallows all
   * outside presses without ever dismissing the popover */
  gtk_popover_set_autohide(GTK_POPOVER(pop), FALSE);
  g_signal_connect(pop, "map", G_CALLBACK(s_popover_map_cb), nullptr);
  g_signal_connect(pop, "unmap", G_CALLBACK(s_popover_unmap_cb), nullptr);
  return pop;
}
