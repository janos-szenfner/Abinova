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

void xap_gtk_container_remove(GtkWidget* container, GtkWidget* child)
{
  if (GTK_IS_BOX(container))
    gtk_box_remove(GTK_BOX(container), child);
  else if (GTK_IS_WINDOW(container) || GTK_IS_FRAME(container) ||
           GTK_IS_SCROLLED_WINDOW(container) || GTK_IS_POPOVER(container) ||
           GTK_IS_EXPANDER(container))
    gtk_widget_unparent(child);
  else
    gtk_widget_unparent(child);
}

void XAP_gtk_widget_set_margin(GtkWidget* w, gint margin)
{
  gtk_widget_set_margin_bottom(w, margin);
  gtk_widget_set_margin_top(w, margin);
  gtk_widget_set_margin_start(w, margin);
  gtk_widget_set_margin_end(w, margin);
}

void XAP_gtk_keyboard_ungrab(GtkWidget* /*widget*/)
{
  /* GTK4 removed the seat grab APIs; grabs are managed internally. */
}
