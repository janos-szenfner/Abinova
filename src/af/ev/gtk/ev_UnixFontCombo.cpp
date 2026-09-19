/*
 *  Copyright (C) 2005 Robert Staudinger
 *
 *  This software is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <gtk/gtk.h>

#include <string.h>

#include "ut_assert.h"
#include "ev_UnixFontCombo.h"

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint font_combo_signals[LAST_SIGNAL] = { 0 };
static GtkBoxClass *abi_font_combo_parent_class = nullptr;

/*
 * Every font name is rendered in its own typeface. Rows are bound
 * lazily by the GtkListItemFactory, so only the handful of visible
 * entries ever load a font. The previous GtkComboBox cell renderer
 * measured every one of the ~2000 model rows on popup and froze
 * the UI for seconds.
 */
static void
font_item_setup (GtkSignalListItemFactory * /*factory*/,
				 GtkListItem			 *item,
				 gpointer				  /*data*/)
{
	GtkWidget *label = gtk_label_new (nullptr);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (GTK_LABEL (label), 30);
	gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
	gtk_list_item_set_child (item, label);
}

static void
font_item_bind (GtkSignalListItemFactory * /*factory*/,
				GtkListItem			 *item,
				gpointer				  /*data*/)
{
	GtkStringObject *str = GTK_STRING_OBJECT (gtk_list_item_get_item (item));
	GtkLabel *label = GTK_LABEL (gtk_list_item_get_child (item));
	const gchar *name = gtk_string_object_get_string (str);

	gtk_label_set_text (label, name);

	PangoAttrList *attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_family_new (name));
	gtk_label_set_attributes (label, attrs);
	pango_attr_list_unref (attrs);
}

static void
font_combo_selected_cb (GtkDropDown * /*dropdown*/,
						GParamSpec	 * /*pspec*/,
						AbiFontCombo *self)
{
	g_signal_emit (self, font_combo_signals[CHANGED], 0);
}

static void
abi_font_combo_init (AbiFontCombo *self, gpointer)
{
	self->strings = nullptr;
	self->sort = nullptr;
	self->is_disposed = FALSE;
}

static void
abi_font_combo_dispose (GObject *instance)
{
	AbiFontCombo *self = ABI_FONT_COMBO (instance);

	if (self->is_disposed) {
		return;
	}

	g_clear_object (&self->strings);
	g_clear_object (&self->sort);

	self->is_disposed = TRUE;
	G_OBJECT_CLASS (abi_font_combo_parent_class)->dispose (instance);
}

static void
abi_font_combo_class_init (AbiFontComboClass *klass, gpointer)
{
	abi_font_combo_parent_class = GTK_BOX_CLASS (g_type_class_peek_parent (klass));
	G_OBJECT_CLASS (klass)->dispose = abi_font_combo_dispose;

	font_combo_signals[CHANGED] =
		g_signal_new ("changed",
			G_OBJECT_CLASS_TYPE (klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET (AbiFontComboClass, changed),
			nullptr, nullptr,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);
}

GType
abi_font_combo_get_type (void)
{
        static GType type = 0;
        if (!type) {
                static const GTypeInfo info = {
                        sizeof (AbiFontComboClass),
                        nullptr,           /* base_init */
                        nullptr,           /* base_finalize */
                        (GClassInitFunc) abi_font_combo_class_init,
                        nullptr,           /* class_finalize */
                        nullptr,           /* class_data */
                        sizeof (AbiFontCombo),
                        0,              /* n_preallocs */
                        (GInstanceInitFunc) abi_font_combo_init,
						nullptr
                };
                type = g_type_register_static (GTK_TYPE_BOX,
					       "AbiFontCombo", &info,
					       (GTypeFlags)0);
        }
        return type;
}

GtkWidget *
abi_font_combo_new (void)
{
	AbiFontCombo *self = (AbiFontCombo *) g_object_new (ABI_TYPE_FONT_COMBO,
													   "orientation", GTK_ORIENTATION_HORIZONTAL,
													   nullptr);

	self->strings = gtk_string_list_new (nullptr);

	GtkExpression *expr =
		gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, nullptr, "string");
	GtkStringSorter *sorter = gtk_string_sorter_new (expr);
	gtk_string_sorter_set_ignore_case (sorter, TRUE);
	self->sort = gtk_sort_list_model_new (G_LIST_MODEL (g_object_ref (self->strings)),
										  GTK_SORTER (sorter));
	/* sort lazily; the model holds several thousand fonts */
	gtk_sort_list_model_set_incremental (self->sort, TRUE);

	GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
	g_signal_connect (factory, "setup", G_CALLBACK (font_item_setup), nullptr);
	g_signal_connect (factory, "bind", G_CALLBACK (font_item_bind), nullptr);

	GtkExpression *search_expr =
		gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, nullptr, "string");

	self->dropdown = gtk_drop_down_new (G_LIST_MODEL (g_object_ref (self->sort)),
										nullptr);
	g_object_set (self->dropdown,
				  "factory", factory,
				  "enable-search", TRUE,
				  "expression", search_expr,
				  nullptr);
	gtk_widget_set_hexpand (self->dropdown, TRUE);
	gtk_box_append (GTK_BOX (self), self->dropdown);

	g_object_unref (factory);
	gtk_expression_unref (search_expr);

	g_signal_connect (self->dropdown, "notify::selected",
					  G_CALLBACK (font_combo_selected_cb), self);

	return GTK_WIDGET (self);
}

static guint
abi_font_combo_find (AbiFontCombo *self, const gchar *text)
{
	GListModel *model = G_LIST_MODEL (self->sort);
	guint n = g_list_model_get_n_items (model);

	for (guint i = 0; i < n; i++) {
		GtkStringObject *str = GTK_STRING_OBJECT (g_list_model_get_item (model, i));
		const gchar *name = gtk_string_object_get_string (str);
		gboolean found = name && (0 == strcmp (name, text));
		g_object_unref (str);
		if (found)
			return i;
	}
	return GTK_INVALID_LIST_POSITION;
}

void
abi_font_combo_insert_font (AbiFontCombo 	*self,
			    const gchar		*font,
			    gboolean 		 select)
{
	gtk_string_list_append (self->strings, font);

	if (select) {
		guint pos = abi_font_combo_find (self, font);
		gtk_drop_down_set_selected (GTK_DROP_DOWN (self->dropdown), pos);
	}
}

gboolean
abi_font_combo_select_text (AbiFontCombo *self, const gchar *text)
{
	guint pos = abi_font_combo_find (self, text);
	if (pos == GTK_INVALID_LIST_POSITION)
		return FALSE;
	gtk_drop_down_set_selected (GTK_DROP_DOWN (self->dropdown), pos);
	return TRUE;
}

void
abi_font_combo_unselect (AbiFontCombo *self)
{
	gtk_drop_down_set_selected (GTK_DROP_DOWN (self->dropdown),
								GTK_INVALID_LIST_POSITION);
}

gchar *
abi_font_combo_get_active_text (AbiFontCombo *self)
{
	GtkStringObject *str =
		GTK_STRING_OBJECT (gtk_drop_down_get_selected_item (GTK_DROP_DOWN (self->dropdown)));
	return str ? g_strdup (gtk_string_object_get_string (str)) : nullptr;
}

/*!
 * Use this for updating the whole combo.
 * \param self
 * \param fonts NUL-terminated array of fonts.
 */
void
abi_font_combo_set_fonts (AbiFontCombo 	 *self,
			  const gchar 	**fonts)
{
	g_return_if_fail (fonts);

	guint n = g_list_model_get_n_items (G_LIST_MODEL (self->strings));
	gtk_string_list_splice (self->strings, 0, n, fonts);
}
