/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Program Utilities
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef AP_UNIX_GLYPH_ICONS_H
#define AP_UNIX_GLYPH_ICONS_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Drawn action icons: a small vector vocabulary on a 20x20 grid,
 * rendered with Cairo and coloured from the widget theme (fg + accent).
 * Keyed by the freedesktop theme icon names Abinova already maps to
 * ("edit-undo", "go-jump", ...) so a missing theme icon never leaves a
 * ribbon button or menu item bare - the "-symbolic" suffix is stripped
 * before lookup.
 *
 * Set ABINOVA_GLYPH_ICONS=1 to prefer drawn glyphs even when the theme
 * provides the icon (preview/debug). */

gboolean	abi_glyph_icon_has		(const gchar *icon_name);
GtkWidget *	abi_glyph_icon_widget	(const gchar *icon_name, int pixel_size);
GIcon *		abi_glyph_icon_gicon	(const gchar *icon_name, int pixel_size);
void		abi_glyph_icon_paint	(const gchar *icon_name, cairo_t *cr,
									 double w, double h,
									 const GdkRGBA &fg, const GdkRGBA &ac);
gboolean	abi_glyph_icons_forced	(void);

G_END_DECLS

#endif /* AP_UNIX_GLYPH_ICONS_H */
