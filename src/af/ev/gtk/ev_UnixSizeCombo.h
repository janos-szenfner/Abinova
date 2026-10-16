/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Program Utilities
 * Copyright (C) 2005 Robert Staudinger <robert.staudinger@gmail.com>
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

#ifndef ABI_SIZE_COMBO_H
#define ABI_SIZE_COMBO_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ABI_TYPE_SIZE_COMBO                  (abi_size_combo_get_type ())
#define ABI_SIZE_COMBO(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), ABI_TYPE_SIZE_COMBO, AbiSizeCombo))
#define ABI_SIZE_COMBO_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), ABI_TYPE_SIZE_COMBO, AbiSizeComboClass))
#define ABI_IS_SIZE_COMBO(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ABI_TYPE_SIZE_COMBO))
#define ABI_IS_SIZE_COMBO_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), ABI_TYPE_SIZE_COMBO))
#define ABI_SIZE_COMBO_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), ABI_TYPE_SIZE_COMBO, AbiSizeComboClass))

/* AbiSizeCombo is a GtkBox wrapper like AbiFontCombo: an editable
 * GtkEntry for typing a point size followed by an arrow button
 * opening a popover with the canned sizes.  The entry stays
 * authoritative - a typed size that is not in the list still
 * applies. */
struct AbiSizeCombo {
	GtkBox			 parent;
	GtkWidget		*entry;
	GtkWidget		*arrow;
	GtkWidget		*popover;
	GtkWidget		*listview;
	GtkSingleSelection *sel;
	GtkFilterListModel *filtered;
	GtkStringFilter	*filter;
	GtkStringList	*strings;
	gboolean		 is_disposed;
	gboolean		 updating;
	gboolean		 arrow_open;
};

struct AbiSizeComboClass {
	GtkBoxClass parent;

	void (* changed) (AbiSizeCombo *self);
};

GType abi_size_combo_get_type (void);

GtkWidget *	abi_size_combo_new (void);
void		abi_size_combo_set_sizes (AbiSizeCombo *self, const gchar **sizes);
gboolean	abi_size_combo_select_text (AbiSizeCombo *self, const gchar *text);
gchar *		abi_size_combo_get_active_text (AbiSizeCombo *self);
void		abi_size_combo_unselect (AbiSizeCombo *self);

G_END_DECLS

#endif /* ABI_SIZE_COMBO_H */
