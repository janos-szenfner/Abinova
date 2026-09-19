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
 *  You should have received a copy of the GNU General Public License
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
 * Rows in the dropdown list are bound lazily by the GtkListItemFactory,
 * so only the handful of visible entries ever load a font. Rendering
 * every one of the ~2000 installed fonts during popup measure is what
 * used to freeze the UI for seconds.
 */
static void
font_item_setup (GtkSignalListItemFactory * /*factory*/,
				 GtkListItem			 *item,
				 gpointer				  /*data*/)
{
	GtkWidget *label = gtk_label_new (nullptr);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	/* The popup sizes to content; give every row a minimum width so the
	 * font list is wide enough to read, like the LibreOffice font box. */
	gtk_label_set_width_chars (GTK_LABEL (label), 45);
	gtk_label_set_max_width_chars (GTK_LABEL (label), 60);
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

/* The collapsed arrow button shows no text — the font name lives in the
 * GtkEntry in front of it, like the LibreOffice/MS-Word font box. */
static void
button_item_setup (GtkSignalListItemFactory * /*factory*/,
				   GtkListItem			   *item,
				   gpointer				  /*data*/)
{
	gtk_list_item_set_child (item, gtk_label_new (""));
}

static void
button_item_bind (GtkSignalListItemFactory * /*factory*/,
				  GtkListItem			  * /*item*/,
				  gpointer				  /*data*/)
{
}

static void
font_combo_selected_cb (GtkDropDown * /*dropdown*/,
						GParamSpec	 * /*pspec*/,
						AbiFontCombo *self)
{
	if (self->updating) {
		return;
	}
	GtkStringObject *str =
		GTK_STRING_OBJECT (gtk_drop_down_get_selected_item (GTK_DROP_DOWN (self->dropdown)));
	if (str) {
		gtk_editable_set_text (GTK_EDITABLE (self->entry),
							   gtk_string_object_get_string (str));
	}
	g_signal_emit (self, font_combo_signals[CHANGED], 0);
}

/* commit the typed name on <enter> — a font that isn't installed is
 * still applied (documents can reference missing fonts) */
static void
font_combo_entry_activate_cb (GtkEntry	  * /*entry*/,
							  AbiFontCombo *self)
{
	g_signal_emit (self, font_combo_signals[CHANGED], 0);
}

/* LibreOffice also commits the typed font when the field loses focus */
static void
font_combo_entry_leave_cb (GtkEventControllerFocus * /*ctrl*/,
						   AbiFontCombo			 *self)
{
	const gchar *text = gtk_editable_get_text (GTK_EDITABLE (self->entry));
	if (text && *text && !self->updating) {
		g_signal_emit (self, font_combo_signals[CHANGED], 0);
	}
}

static void
abi_font_combo_init (AbiFontCombo *self, gpointer)
{
	self->strings = nullptr;
	self->sort = nullptr;
	self->is_disposed = FALSE;
	self->updating = FALSE;
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
	gtk_widget_add_css_class (GTK_WIDGET (self), "linked");

	/* LibreOffice-style font box: an editable entry for typing font
	 * names, with a dropdown arrow on the right. */
	self->entry = gtk_entry_new ();
	gtk_widget_set_hexpand (self->entry, TRUE);
	gtk_editable_set_width_chars (GTK_EDITABLE (self->entry), 15);
	gtk_box_append (GTK_BOX (self), self->entry);
	g_signal_connect (self->entry, "activate",
					  G_CALLBACK (font_combo_entry_activate_cb), self);
	GtkEventController *focus_ctrl = gtk_event_controller_focus_new ();
	g_signal_connect (focus_ctrl, "leave",
					  G_CALLBACK (font_combo_entry_leave_cb), self);
	gtk_widget_add_controller (self->entry, focus_ctrl);

	self->strings = gtk_string_list_new (nullptr);

	GtkExpression *expr =
		gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, nullptr, "string");
	GtkStringSorter *sorter = gtk_string_sorter_new (expr);
	gtk_string_sorter_set_ignore_case (sorter, TRUE);
	self->sort = gtk_sort_list_model_new (G_LIST_MODEL (g_object_ref (self->strings)),
										  GTK_SORTER (sorter));
	/* sort lazily; the model holds several thousand fonts */
	gtk_sort_list_model_set_incremental (self->sort, TRUE);

	GtkListItemFactory *list_factory = gtk_signal_list_item_factory_new ();
	g_signal_connect (list_factory, "setup", G_CALLBACK (font_item_setup), nullptr);
	g_signal_connect (list_factory, "bind", G_CALLBACK (font_item_bind), nullptr);

	GtkListItemFactory *button_factory = gtk_signal_list_item_factory_new ();
	g_signal_connect (button_factory, "setup", G_CALLBACK (button_item_setup), nullptr);
	g_signal_connect (button_factory, "bind", G_CALLBACK (button_item_bind), nullptr);

	GtkExpression *search_expr =
		gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, nullptr, "string");

	/* the expression must be set before the factories: setting an
	 * expression afterwards makes GtkDropDown reinstall its default
	 * factory, and the button would show the selected item's text */
	self->dropdown = gtk_drop_down_new (G_LIST_MODEL (g_object_ref (self->sort)),
										search_expr);
	gtk_drop_down_set_factory (GTK_DROP_DOWN (self->dropdown), button_factory);
	gtk_drop_down_set_list_factory (GTK_DROP_DOWN (self->dropdown), list_factory);
	gtk_drop_down_set_enable_search (GTK_DROP_DOWN (self->dropdown), TRUE);
	gtk_box_append (GTK_BOX (self), self->dropdown);

	g_object_unref (list_factory);
	g_object_unref (button_factory);
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
	if (abi_font_combo_find (self, font) == GTK_INVALID_LIST_POSITION) {
		gtk_string_list_append (self->strings, font);
	}

	if (select) {
		abi_font_combo_select_text (self, font);
	}
}

gboolean
abi_font_combo_select_text (AbiFontCombo *self, const gchar *text)
{
	self->updating = TRUE;
	gtk_editable_set_text (GTK_EDITABLE (self->entry), text ? text : "");
	guint pos = text ? abi_font_combo_find (self, text)
					 : GTK_INVALID_LIST_POSITION;
	gtk_drop_down_set_selected (GTK_DROP_DOWN (self->dropdown), pos);
	self->updating = FALSE;
	return pos != GTK_INVALID_LIST_POSITION;
}

void
abi_font_combo_unselect (AbiFontCombo *self)
{
	self->updating = TRUE;
	gtk_editable_set_text (GTK_EDITABLE (self->entry), "");
	gtk_drop_down_set_selected (GTK_DROP_DOWN (self->dropdown),
								GTK_INVALID_LIST_POSITION);
	self->updating = FALSE;
}

gchar *
abi_font_combo_get_active_text (AbiFontCombo *self)
{
	const gchar *text = gtk_editable_get_text (GTK_EDITABLE (self->entry));
	return (text && *text) ? g_strdup (text) : nullptr;
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
