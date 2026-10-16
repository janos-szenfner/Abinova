/*
 *  Copyright (C) 2005 Robert Staudinger
 *  Copyright (C) 2025-2026 Abinova contributors
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
#include "ev_UnixSizeCombo.h"

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint size_combo_signals[LAST_SIGNAL] = { 0 };
static GtkBoxClass *abi_size_combo_parent_class = nullptr;

static void
size_item_setup (GtkSignalListItemFactory * /*factory*/,
				 GtkListItem			 *item,
				 gpointer				  /*data*/)
{
	GtkWidget *label = gtk_label_new (nullptr);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
	gtk_label_set_width_chars (GTK_LABEL (label), 5);
	gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
	gtk_list_item_set_child (item, label);
}

static void
size_item_bind (GtkSignalListItemFactory * /*factory*/,
				GtkListItem			 *item,
				gpointer				  /*data*/)
{
	GtkStringObject *str = GTK_STRING_OBJECT (gtk_list_item_get_item (item));
	gtk_label_set_text (GTK_LABEL (gtk_list_item_get_child (item)),
						gtk_string_object_get_string (str));
}

/* typing in the entry is the search: the popup opens and the size
 * list filters live on the typed text, like the font combo */
static void
size_combo_entry_changed_cb (GtkEntry * entry, AbiSizeCombo *self)
{
	if (self->updating)
		return;
	self->updating = TRUE;
	gtk_string_filter_set_search (self->filter,
								  gtk_editable_get_text (GTK_EDITABLE (entry)));
	self->updating = FALSE;
	if (!gtk_widget_get_visible (self->popover))
		gtk_popover_popup (GTK_POPOVER (self->popover));
}

/* clicking a row applies that size immediately and closes the popup */
static void
size_combo_selection_cb (GtkSingleSelection *sel,
						 GParamSpec		  * /*pspec*/,
						 AbiSizeCombo	  *self)
{
	if (self->updating)
		return;
	gpointer item = gtk_single_selection_get_selected_item (sel);
	if (!item)
		return;
	GtkStringObject *str = GTK_STRING_OBJECT (item);
	const gchar * name = gtk_string_object_get_string (str);

	self->updating = TRUE;
	gtk_editable_set_text (GTK_EDITABLE (self->entry), name);
	gtk_single_selection_set_selected (sel, GTK_INVALID_LIST_POSITION);
	self->updating = FALSE;

	gtk_popover_popdown (GTK_POPOVER (self->popover));
	g_signal_emit (self, size_combo_signals[CHANGED], 0);
}

/* commit the typed size on <enter> — a size that is not in the list
 * is still applied (fractional sizes like 10.5 are valid) */
static void
size_combo_entry_activate_cb (GtkEntry	  * /*entry*/,
							  AbiSizeCombo *self)
{
	if (gtk_widget_get_visible (self->popover))
		gtk_popover_popdown (GTK_POPOVER (self->popover));
	g_signal_emit (self, size_combo_signals[CHANGED], 0);
}

/* commit the typed size when the field loses focus; while the popover
 * is up, selection clicks or <enter> do the committing instead */
static void
size_combo_entry_leave_cb (GtkEventControllerFocus * /*ctrl*/,
						   AbiSizeCombo			 *self)
{
	if (self->updating || gtk_widget_get_visible (self->popover))
		return;
	const gchar *text = gtk_editable_get_text (GTK_EDITABLE (self->entry));
	if (text && *text) {
		g_signal_emit (self, size_combo_signals[CHANGED], 0);
	}
}

static gboolean
size_combo_entry_key_cb (GtkEventControllerKey * /*ctrl*/,
						 guint keyval, guint /*keycode*/,
						 GdkModifierType /*state*/, AbiSizeCombo *self)
{
	if (keyval == GDK_KEY_Down &&
		!gtk_widget_get_visible (self->popover))
	{
		gtk_popover_popup (GTK_POPOVER (self->popover));
		return TRUE;
	}
	if (keyval == GDK_KEY_Escape &&
		gtk_widget_get_visible (self->popover))
	{
		gtk_popover_popdown (GTK_POPOVER (self->popover));
		return TRUE;
	}
	return FALSE;
}

/* only accept a point size in the entry: digits and one '.' */
static void
size_combo_insert_text_cb (GtkEditable *editable,
						   gchar		*new_text,
						   gint		 new_text_length,
						   gint		* /*position*/,
						   gpointer	 /*data*/)
{
	if (new_text_length < 0)
		new_text_length = strlen (new_text);

	const gchar *cur = gtk_editable_get_text (editable);
	gboolean haveDot = cur && strchr (cur, '.');

	gchar * iter = new_text;
	while (iter < (new_text + new_text_length))
	{
		gunichar c = g_utf8_get_char (iter);
		if (!g_unichar_isdigit (c))
		{
			if (c != '.' || haveDot)
			{
				g_signal_stop_emission_by_name (
					G_OBJECT (editable), "insert-text");
				return;
			}
			haveDot = TRUE;
		}
		iter = g_utf8_next_char (iter);
	}
}

/* the popup's seat grab routes keystrokes to the list - forward
 * editing keys back to the entry so typing keeps filtering */
static gboolean
size_combo_popover_key_cb (GtkEventControllerKey * /*ctrl*/,
						   guint keyval, guint /*keycode*/,
						   GdkModifierType state, AbiSizeCombo *self)
{
	GtkEditable *entry = GTK_EDITABLE (self->entry);

	if (keyval == GDK_KEY_Escape)
	{
		gtk_popover_popdown (GTK_POPOVER (self->popover));
		gtk_widget_grab_focus (self->entry);
		return TRUE;
	}
	if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
	{
		gtk_popover_popdown (GTK_POPOVER (self->popover));
		gtk_widget_grab_focus (self->entry);
		g_signal_emit (self, size_combo_signals[CHANGED], 0);
		return TRUE;
	}
	if (state & (GDK_CONTROL_MASK | GDK_ALT_MASK))
		return FALSE;
	if (keyval == GDK_KEY_BackSpace)
	{
		int start = 0, end = 0;
		if (gtk_editable_get_selection_bounds (entry, &start, &end) &&
			start != end)
			gtk_editable_delete_text (entry, start, end);
		else
		{
			int pos = gtk_editable_get_position (entry);
			if (pos > 0)
				gtk_editable_delete_text (entry, pos - 1, pos);
		}
		return TRUE;
	}
	if (keyval == GDK_KEY_Delete)
	{
		int start = 0, end = 0;
		if (gtk_editable_get_selection_bounds (entry, &start, &end) &&
			start != end)
			gtk_editable_delete_text (entry, start, end);
		else
		{
			int pos = gtk_editable_get_position (entry);
			gtk_editable_delete_text (entry, pos, pos + 1);
		}
		return TRUE;
	}
	if (keyval == GDK_KEY_Left || keyval == GDK_KEY_Right ||
		keyval == GDK_KEY_Home || keyval == GDK_KEY_End)
		return FALSE;
	gunichar ch = gdk_keyval_to_unicode (keyval);
	if (ch && g_unichar_isprint (ch))
	{
		char utf8[8];
		int len = g_unichar_to_utf8 (ch, utf8);
		utf8[len] = '\0';
		int pos = gtk_editable_get_position (entry);
		gtk_editable_insert_text (entry, utf8, len, &pos);
		gtk_editable_set_position (entry, pos);
		return TRUE;
	}
	return FALSE;
}

/* arrow button: open the popup below the field with the full list,
 * preselecting and scrolling to the current size */
static void
size_combo_arrow_cb (GtkButton * /*btn*/, AbiSizeCombo *self)
{
	if (gtk_widget_get_visible (self->popover))
	{
		gtk_popover_popdown (GTK_POPOVER (self->popover));
		return;
	}
	self->updating = TRUE;
	gtk_string_filter_set_search (self->filter, nullptr);
	self->arrow_open = TRUE;
	gtk_popover_popup (GTK_POPOVER (self->popover));
	self->updating = FALSE;
}

static guint
abi_size_combo_find (AbiSizeCombo *self, const gchar *text);

/* after the arrow-opened popup is shown, select the current size and
 * scroll it into view */
static void
size_combo_popover_show_cb (GtkWidget * /*popover*/, AbiSizeCombo *self)
{
	if (!self->arrow_open)
		return;
	self->arrow_open = FALSE;

	const gchar *text = gtk_editable_get_text (GTK_EDITABLE (self->entry));
	guint pos = (text && *text) ? abi_size_combo_find (self, text)
								: GTK_INVALID_LIST_POSITION;
	if (pos == GTK_INVALID_LIST_POSITION)
		return;
	self->updating = TRUE;
	gtk_single_selection_set_selected (self->sel, pos);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_list_view_scroll_to (GTK_LIST_VIEW (self->listview), pos,
							 GTK_LIST_SCROLL_NONE, nullptr);
G_GNUC_END_IGNORE_DEPRECATIONS
	self->updating = FALSE;
}

static void
abi_size_combo_init (AbiSizeCombo *self, gpointer)
{
	self->strings = nullptr;
	self->filtered = nullptr;
	self->filter = nullptr;
	self->sel = nullptr;
	self->is_disposed = FALSE;
	self->updating = FALSE;
	self->arrow_open = FALSE;
}

static void
abi_size_combo_dispose (GObject *instance)
{
	AbiSizeCombo *self = ABI_SIZE_COMBO (instance);

	if (self->is_disposed) {
		return;
	}
	self->is_disposed = TRUE;

	/* Silence callbacks before releasing the models: clearing the
	 * list below emits items-changed into the selection model, whose
	 * notify handler pokes the popover and the entry - both may
	 * already be gone while the frame is being torn down. */
	self->updating = TRUE;
	if (self->sel)
		g_signal_handlers_disconnect_by_data (self->sel, self);

	if (self->popover) {
		gtk_widget_unparent (self->popover);
		self->popover = nullptr;
	}
	self->listview = nullptr;

	g_clear_object (&self->strings);
	g_clear_object (&self->filtered);
	/* self->filter went to gtk_filter_list_model_new() and self->sel
	 * to gtk_list_view_new(), both (transfer full) - unreffing here
	 * would be a double-free */
	self->filter = nullptr;
	self->sel = nullptr;

	G_OBJECT_CLASS (abi_size_combo_parent_class)->dispose (instance);
}

static void
abi_size_combo_class_init (AbiSizeComboClass *klass, gpointer)
{
	abi_size_combo_parent_class = GTK_BOX_CLASS (g_type_class_peek_parent (klass));
	G_OBJECT_CLASS (klass)->dispose = abi_size_combo_dispose;

	size_combo_signals[CHANGED] =
		g_signal_new ("changed",
			G_OBJECT_CLASS_TYPE (klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET (AbiSizeComboClass, changed),
			nullptr, nullptr,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);
}

GType
abi_size_combo_get_type (void)
{
        static GType type = 0;
        if (!type) {
                static const GTypeInfo info = {
                        sizeof (AbiSizeComboClass),
                        nullptr,           /* base_init */
                        nullptr,           /* base_finalize */
                        reinterpret_cast<GClassInitFunc>( abi_size_combo_class_init),
                        nullptr,           /* class_finalize */
                        nullptr,           /* class_data */
                        sizeof (AbiSizeCombo),
                        0,              /* n_preallocs */
                        reinterpret_cast<GInstanceInitFunc>( abi_size_combo_init),
						nullptr
                };
                type = g_type_register_static (GTK_TYPE_BOX,
					       "AbiSizeCombo", &info,
					       static_cast<GTypeFlags>(0));
        }
        return type;
}

GtkWidget *
abi_size_combo_new (void)
{
	AbiSizeCombo *self = static_cast<AbiSizeCombo *>( g_object_new (ABI_TYPE_SIZE_COMBO,
													   "orientation", GTK_ORIENTATION_HORIZONTAL,
													   nullptr));
	gtk_widget_add_css_class (GTK_WIDGET (self), "linked");

	/* same field shape as the font combo: an editable entry for
	 * typing a point size, with a dropdown arrow on the right */
	self->entry = gtk_entry_new ();
	gtk_widget_set_hexpand (self->entry, FALSE);
	gtk_editable_set_width_chars (GTK_EDITABLE (self->entry), 4);
	gtk_editable_set_max_width_chars (GTK_EDITABLE (self->entry), 6);
	gtk_editable_set_alignment (GTK_EDITABLE (self->entry), 0.5f);
	gtk_box_append (GTK_BOX (self), self->entry);
	g_signal_connect (self->entry, "activate",
					  G_CALLBACK (size_combo_entry_activate_cb), self);
	g_signal_connect (self->entry, "changed",
					  G_CALLBACK (size_combo_entry_changed_cb), self);
	g_signal_connect (self->entry, "insert-text",
					  G_CALLBACK (size_combo_insert_text_cb), nullptr);
	GtkEventController *focus_ctrl = gtk_event_controller_focus_new ();
	g_signal_connect (focus_ctrl, "leave",
					  G_CALLBACK (size_combo_entry_leave_cb), self);
	gtk_widget_add_controller (self->entry, focus_ctrl);
	GtkEventController *key_ctrl = gtk_event_controller_key_new ();
	g_signal_connect (key_ctrl, "key-pressed",
					  G_CALLBACK (size_combo_entry_key_cb), self);
	gtk_widget_add_controller (self->entry, key_ctrl);

	self->arrow = gtk_button_new_from_icon_name ("pan-down-symbolic");
	gtk_box_append (GTK_BOX (self), self->arrow);
	g_signal_connect (self->arrow, "clicked",
					  G_CALLBACK (size_combo_arrow_cb), self);

	self->strings = gtk_string_list_new (nullptr);

	/* typed text filters the list live */
	GtkExpression *filter_expr =
		gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, nullptr, "string");
	self->filter = gtk_string_filter_new (filter_expr);
	gtk_string_filter_set_match_mode (self->filter,
									  GTK_STRING_FILTER_MATCH_MODE_SUBSTRING);
	self->filtered = gtk_filter_list_model_new (
		G_LIST_MODEL (g_object_ref (self->strings)), GTK_FILTER (self->filter));

	self->sel = gtk_single_selection_new (
		G_LIST_MODEL (g_object_ref (self->filtered)));
	gtk_single_selection_set_autoselect (self->sel, FALSE);
	g_signal_connect (self->sel, "notify::selected-item",
					  G_CALLBACK (size_combo_selection_cb), self);

	GtkListItemFactory *list_factory = gtk_signal_list_item_factory_new ();
	g_signal_connect (list_factory, "setup", G_CALLBACK (size_item_setup), nullptr);
	g_signal_connect (list_factory, "bind", G_CALLBACK (size_item_bind), nullptr);

	self->listview = gtk_list_view_new (GTK_SELECTION_MODEL (self->sel),
										list_factory);

	GtkWidget * scroll = gtk_scrolled_window_new ();
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
									GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroll), 300);
	gtk_scrolled_window_set_propagate_natural_height (
		GTK_SCROLLED_WINDOW (scroll), TRUE);
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), self->listview);

	/* popover parents to the combo box so the list drops below the
	 * field, flush with its left edge */
	self->popover = gtk_popover_new ();
	gtk_widget_set_parent (self->popover, GTK_WIDGET (self));
	gtk_popover_set_position (GTK_POPOVER (self->popover), GTK_POS_BOTTOM);
	gtk_popover_set_has_arrow (GTK_POPOVER (self->popover), FALSE);
	gtk_popover_set_child (GTK_POPOVER (self->popover), scroll);
	g_signal_connect (self->popover, "show",
					  G_CALLBACK (size_combo_popover_show_cb), self);
	/* capture keys before the list's typeahead so typing keeps going
	 * to the entry */
	GtkEventController *pop_key = gtk_event_controller_key_new ();
	gtk_event_controller_set_propagation_phase (pop_key, GTK_PHASE_CAPTURE);
	g_signal_connect (pop_key, "key-pressed",
					  G_CALLBACK (size_combo_popover_key_cb), self);
	gtk_widget_add_controller (self->popover, pop_key);

	return GTK_WIDGET (self);
}

static guint
abi_size_combo_find (AbiSizeCombo *self, const gchar *text)
{
	GListModel *model = G_LIST_MODEL (self->strings);
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

gboolean
abi_size_combo_select_text (AbiSizeCombo *self, const gchar *text)
{
	self->updating = TRUE;
	gtk_editable_set_text (GTK_EDITABLE (self->entry), text ? text : "");
	gtk_string_filter_set_search (self->filter, nullptr);
	guint pos = text ? abi_size_combo_find (self, text)
					 : GTK_INVALID_LIST_POSITION;
	gtk_single_selection_set_selected (self->sel, pos);
	self->updating = FALSE;
	return pos != GTK_INVALID_LIST_POSITION;
}

void
abi_size_combo_unselect (AbiSizeCombo *self)
{
	self->updating = TRUE;
	gtk_editable_set_text (GTK_EDITABLE (self->entry), "");
	gtk_string_filter_set_search (self->filter, nullptr);
	gtk_single_selection_set_selected (self->sel,
									   GTK_INVALID_LIST_POSITION);
	self->updating = FALSE;
}

gchar *
abi_size_combo_get_active_text (AbiSizeCombo *self)
{
	const gchar *text = gtk_editable_get_text (GTK_EDITABLE (self->entry));
	return (text && *text) ? g_strdup (text) : nullptr;
}

/*!
 * Use this for updating the whole combo.
 * \param self
 * \param sizes NUL-terminated array of point sizes.
 */
void
abi_size_combo_set_sizes (AbiSizeCombo 	 *self,
			  const gchar 	**sizes)
{
	g_return_if_fail (sizes);

	guint n = g_list_model_get_n_items (G_LIST_MODEL (self->strings));
	gtk_string_list_splice (self->strings, 0, n, sizes);
}
