/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Program Utilities
 * Copyright (C) 2005 Robert Staudinger <robert.staudinger@gmail.com>
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

#ifndef ABI_FONT_COMBO_H
#define ABI_FONT_COMBO_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ABI_TYPE_FONT_COMBO                  (abi_font_combo_get_type ())
#define ABI_FONT_COMBO(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), ABI_TYPE_FONT_COMBO, AbiFontCombo))
#define ABI_FONT_COMBO_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), ABI_TYPE_FONT_COMBO, AbiFontComboClass))
#define ABI_IS_FONT_COMBO(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ABI_TYPE_FONT_COMBO))
#define ABI_IS_FONT_COMBO_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), ABI_TYPE_FONT_COMBO))
#define ABI_FONT_COMBO_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), ABI_TYPE_FONT_COMBO, AbiFontComboClass))

/* GtkDropDown is a final GTK4 type and cannot be subclassed, so
 * AbiFontCombo is a GtkBox wrapper: an editable GtkEntry for typing
 * font names (LibreOffice style) followed by a GtkDropDown arrow whose
 * lazy list shows every font in its own typeface. */
struct AbiFontCombo {
	GtkBox			 parent;
	GtkWidget		*entry;
	GtkWidget		*dropdown;
	GtkStringList	*strings;
	GtkSortListModel *sort;
	gboolean		 is_disposed;
	gboolean		 updating;
};

struct AbiFontComboClass {
	GtkBoxClass parent;

	void (* changed) (AbiFontCombo *self);
};

GType abi_font_combo_get_type (void);

GtkWidget * 	abi_font_combo_new (void);
void		abi_font_combo_insert_font (AbiFontCombo *self, const gchar *font, gboolean select);
void		abi_font_combo_set_fonts (AbiFontCombo *self, const gchar **fonts);
gboolean	abi_font_combo_select_text (AbiFontCombo *self, const gchar *text);
gchar *		abi_font_combo_get_active_text (AbiFontCombo *self);
void		abi_font_combo_unselect (AbiFontCombo *self);

G_END_DECLS

#endif /* ABI_FONT_COMBO_H */
