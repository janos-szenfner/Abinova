/* Abinova
 * Copyright (C) 2011-2019 Hubert Figuiere
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
#include <gtk/gtk.h>

#define XAP_HAS_NATIVE_WINDOW(w) \
  (gtk_widget_get_native(w) != nullptr && \
   gtk_native_get_surface(gtk_widget_get_native(w)) != nullptr)

/// GTK4: GtkFileChooser only returns GFile; convenience wrappers
/// returning a newly-allocated path / uri (or nullptr).
gchar* xap_gtk_file_chooser_get_filename(GtkFileChooser* chooser);
gchar* xap_gtk_file_chooser_get_uri(GtkFileChooser* chooser);

/// GTK4 dropped the generic GtkContainer API; these dispatch to the
/// container-type-specific child setter.
void xap_gtk_container_add(GtkWidget* container, GtkWidget* child);
void xap_gtk_container_remove(GtkWidget* container, GtkWidget* child);

/// Convenience to raise the widget window.
void XAP_gtk_window_raise(GtkWidget*);

/// Convenience to set the same margin on all side.
void XAP_gtk_widget_set_margin(GtkWidget* w, gint margin);

/// Creates a GtkPopover with sane defaults for this port: dismiss on
/// outside presses and when the toplevel window loses activation.
/// GTK's own autohide grab does not dismiss reliably under rootless
/// XWayland, so the dismissal is handled manually.
GtkWidget* xap_gtk_popover_new(void);

/// Convenience to get the entry text. Takes GtkEditable so it works
/// for GtkSpinButton too (no longer a GtkEntry in GTK4).
inline
const gchar* XAP_gtk_entry_get_text(GtkEditable* editable)
{
    return gtk_editable_get_text(editable);
}

/// Convenience to set the entry text.
inline
void XAP_gtk_entry_set_text(GtkEditable* editable, const gchar* text)
{
  gtk_editable_set_text(editable, text);
}

