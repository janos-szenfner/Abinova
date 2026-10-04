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

/// Baseline accessibility helpers (GTK4 GtkAccessible).
///
/// XAP_gtk_a11y_name gives a widget an accessible name: the string a
/// screen reader announces for it.  Use it on controls that have no
/// visible text label (icon-only buttons, the document canvas, chrome
/// containers).
void XAP_gtk_a11y_name(GtkWidget* w, const char* name);

/// Name a widget from its own tooltip text.  Only applies to
/// interactive widgets that have no visible text label GTK can derive
/// a name from (icon-only buttons) or no name at all (entries,
/// dropdowns, ranges, switches), and only when a tooltip is set.
void XAP_gtk_a11y_name_from_tooltip(GtkWidget* w);

/// Walk the widget subtree under root and apply
/// XAP_gtk_a11y_name_from_tooltip to every interactive descendant.
/// Cheap enough to re-run whenever the tree is populated or shown.
void XAP_gtk_a11y_auto_name(GtkWidget* root);

/// Give every interactive descendant of root that has no visible
/// text label (entries, dropdowns, icon-only buttons) the accessible
/// name @name.  Used for composite controls such as the font combo,
/// where the tooltip/label lives on the wrapper box and the inner
/// entry and arrow button would otherwise stay anonymous to ATs.
void XAP_gtk_a11y_name_descendants(GtkWidget* root, const char* name);

/// Creates a GdkPixbufLoader that bounds the decoded image size to
/// the sane UT_IMAGE_MAX_* limits: images declared larger are asked
/// to scale down during the load itself (honored by loaders with
/// scaled-decode support).  The resulting pixbuf must still be passed
/// through xap_gtk_pixbuf_enforce_limits() — loaders that ignore the
/// hint deliver the full-size image.
GdkPixbufLoader* xap_gtk_pixbuf_loader_new_capped(void);

/// Enforce the sane decoded-image bounds on a loaded pixbuf.
/// Consumes the caller's reference: returns the same pixbuf when it
/// is within limits, a new downscaled pixbuf when it is not, or
/// nullptr when the downscale fails (input already freed).
GdkPixbuf* xap_gtk_pixbuf_enforce_limits(GdkPixbuf* pixbuf);

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

