/*
 *  Copyright (C) 2005 Robert Staudinger
 * Copyright (C) 2025-2026 Abinova contributors
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

/* typing in the entry is the search: the popup opens and the font
 * list filters live on the typed text */
static void
font_combo_entry_changed_cb (GtkEntry * entry, AbiFontCombo *self)
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

/* clicking a row applies that font immediately (LibreOffice-style
 * single-click apply) and closes the popup */
static void
font_combo_selection_cb (GtkSingleSelection *sel,
						 GParamSpec		  * /*pspec*/,
						 AbiFontCombo	  *self)
{
	if (self->updating)
		return;
	gpointer item = gtk_single_selection_get_selected_item (sel);
	if (!item)
		return;
	GtkStringObject *str = GTK_STRING_OBJECT (item);
	gchar * name = g_strdup (gtk_string_object_get_string (str));

	self->updating = TRUE;
	gtk_editable_set_text (GTK_EDITABLE (self->entry), name);
	gtk_single_selection_set_selected (sel, GTK_INVALID_LIST_POSITION);
	self->updating = FALSE;

	gtk_popover_popdown (GTK_POPOVER (self->popover));
	g_signal_emit (self, font_combo_signals[CHANGED], 0);

	/* the toolbar refresh can report an empty font state while focus
	 * is still settling after the popup - keep showing the font the
	 * user just picked */
	self->updating = TRUE;
	gtk_editable_set_text (GTK_EDITABLE (self->entry), name);
	self->updating = FALSE;
	g_free (name);
}

/* commit the typed name on <enter> — a font that isn't installed is
 * still applied (documents can reference missing fonts) */
static void
font_combo_entry_activate_cb (GtkEntry	  * /*entry*/,
							  AbiFontCombo *self)
{
	if (gtk_widget_get_visible (self->popover))
		gtk_popover_popdown (GTK_POPOVER (self->popover));
	gchar * text = g_strdup (gtk_editable_get_text (
		GTK_EDITABLE (self->entry)));
	g_signal_emit (self, font_combo_signals[CHANGED], 0);
	/* a null font-state refresh during focus settle must not wipe
	 * the name the user just committed */
	self->updating = TRUE;
	gtk_editable_set_text (GTK_EDITABLE (self->entry),
						   text ? text : "");
	self->updating = FALSE;
	g_free (text);
}

/* LibreOffice also commits the typed font when the field loses focus.
 * The popover opening steals focus, though - while it is up, selection
 * clicks or <enter> do the committing instead. */
static void
font_combo_entry_leave_cb (GtkEventControllerFocus * /*ctrl*/,
						   AbiFontCombo			 *self)
{
	if (self->updating || gtk_widget_get_visible (self->popover))
		return;
	const gchar *text = gtk_editable_get_text (GTK_EDITABLE (self->entry));
	if (text && *text) {
		g_signal_emit (self, font_combo_signals[CHANGED], 0);
	}
}

static gboolean
font_combo_entry_key_cb (GtkEventControllerKey * /*ctrl*/,
						 guint keyval, guint /*keycode*/,
						 GdkModifierType /*state*/, AbiFontCombo *self)
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

/* the popup's seat grab routes keystrokes to the list - forward
 * editing keys back to the entry so typing keeps filtering */
static gboolean
font_combo_popover_key_cb (GtkEventControllerKey * /*ctrl*/,
						   guint keyval, guint /*keycode*/,
						   GdkModifierType state, AbiFontCombo *self)
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
		g_signal_emit (self, font_combo_signals[CHANGED], 0);
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
		return FALSE;	/* let the list scroll; entry cursor keys are
						 * less useful while the popup is up */
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
 * preselecting and scrolling to the current font */
static void
font_combo_arrow_cb (GtkButton * /*btn*/, AbiFontCombo *self)
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
abi_font_combo_find (AbiFontCombo *self, const gchar *text);

/* after the arrow-opened popup is shown, select the current font and
 * scroll it into view */
static void
font_combo_popover_show_cb (GtkWidget * /*popover*/, AbiFontCombo *self)
{
	if (!self->arrow_open)
		return;
	self->arrow_open = FALSE;

	const gchar *text = gtk_editable_get_text (GTK_EDITABLE (self->entry));
	guint pos = (text && *text) ? abi_font_combo_find (self, text)
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
abi_font_combo_init (AbiFontCombo *self, gpointer)
{
	self->strings = nullptr;
	self->sort = nullptr;
	self->filtered = nullptr;
	self->filter = nullptr;
	self->sel = nullptr;
	self->is_disposed = FALSE;
	self->updating = FALSE;
	self->arrow_open = FALSE;
}

static void
abi_font_combo_dispose (GObject *instance)
{
	AbiFontCombo *self = ABI_FONT_COMBO (instance);

	if (self->is_disposed) {
		return;
	}

	if (self->popover)
		gtk_widget_unparent (self->popover);

	g_clear_object (&self->strings);
	g_clear_object (&self->sort);
	g_clear_object (&self->filtered);
	g_clear_object (&self->filter);
	g_clear_object (&self->sel);

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
	 * names, with a dropdown arrow on the right. Typing in the entry
	 * is the search — the list filters live on the text. */
	self->entry = gtk_entry_new ();
	gtk_widget_set_hexpand (self->entry, TRUE);
	gtk_editable_set_width_chars (GTK_EDITABLE (self->entry), 15);
	gtk_box_append (GTK_BOX (self), self->entry);
	g_signal_connect (self->entry, "activate",
					  G_CALLBACK (font_combo_entry_activate_cb), self);
	g_signal_connect (self->entry, "changed",
					  G_CALLBACK (font_combo_entry_changed_cb), self);
	GtkEventController *focus_ctrl = gtk_event_controller_focus_new ();
	g_signal_connect (focus_ctrl, "leave",
					  G_CALLBACK (font_combo_entry_leave_cb), self);
	gtk_widget_add_controller (self->entry, focus_ctrl);
	GtkEventController *key_ctrl = gtk_event_controller_key_new ();
	g_signal_connect (key_ctrl, "key-pressed",
					  G_CALLBACK (font_combo_entry_key_cb), self);
	gtk_widget_add_controller (self->entry, key_ctrl);

	self->arrow = gtk_button_new_from_icon_name ("pan-down-symbolic");
	gtk_box_append (GTK_BOX (self), self->arrow);
	g_signal_connect (self->arrow, "clicked",
					  G_CALLBACK (font_combo_arrow_cb), self);

	self->strings = gtk_string_list_new (nullptr);

	GtkExpression *expr =
		gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, nullptr, "string");
	GtkStringSorter *sorter = gtk_string_sorter_new (expr);
	gtk_string_sorter_set_ignore_case (sorter, TRUE);
	self->sort = gtk_sort_list_model_new (G_LIST_MODEL (g_object_ref (self->strings)),
										  GTK_SORTER (sorter));
	/* sort lazily; the model holds several thousand fonts */
	gtk_sort_list_model_set_incremental (self->sort, TRUE);

	/* typed text filters the list live */
	GtkExpression *filter_expr =
		gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, nullptr, "string");
	self->filter = gtk_string_filter_new (filter_expr);
	gtk_string_filter_set_match_mode (self->filter,
									  GTK_STRING_FILTER_MATCH_MODE_SUBSTRING);
	gtk_string_filter_set_ignore_case (self->filter, TRUE);
	self->filtered = gtk_filter_list_model_new (
		G_LIST_MODEL (g_object_ref (self->sort)), GTK_FILTER (self->filter));
	gtk_filter_list_model_set_incremental (self->filtered, TRUE);

	self->sel = gtk_single_selection_new (
		G_LIST_MODEL (g_object_ref (self->filtered)));
	gtk_single_selection_set_autoselect (self->sel, FALSE);
	g_signal_connect (self->sel, "notify::selected-item",
					  G_CALLBACK (font_combo_selection_cb), self);

	GtkListItemFactory *list_factory = gtk_signal_list_item_factory_new ();
	g_signal_connect (list_factory, "setup", G_CALLBACK (font_item_setup), nullptr);
	g_signal_connect (list_factory, "bind", G_CALLBACK (font_item_bind), nullptr);

	self->listview = gtk_list_view_new (GTK_SELECTION_MODEL (self->sel),
									  list_factory);

	GtkWidget * scroll = gtk_scrolled_window_new ();
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
									GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroll), 400);
	gtk_scrolled_window_set_propagate_natural_height (
		GTK_SCROLLED_WINDOW (scroll), TRUE);
	gtk_scrolled_window_set_propagate_natural_width (
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
					  G_CALLBACK (font_combo_popover_show_cb), self);
	/* capture keys before the list's typeahead so typing keeps going
	 * to the entry */
	GtkEventController *pop_key = gtk_event_controller_key_new ();
	gtk_event_controller_set_propagation_phase (pop_key, GTK_PHASE_CAPTURE);
	g_signal_connect (pop_key, "key-pressed",
					  G_CALLBACK (font_combo_popover_key_cb), self);
	gtk_widget_add_controller (self->popover, pop_key);

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
	gtk_string_filter_set_search (self->filter, nullptr);
	guint pos = text ? abi_font_combo_find (self, text)
					 : GTK_INVALID_LIST_POSITION;
	gtk_single_selection_set_selected (self->sel, pos);
	self->updating = FALSE;
	return pos != GTK_INVALID_LIST_POSITION;
}

void
abi_font_combo_unselect (AbiFontCombo *self)
{
	/* LibreOffice keeps showing the last font name even when the
	 * view has no font context - blanking the field mid-edit just
	 * loses what the user typed */
	self->updating = TRUE;
	gtk_string_filter_set_search (self->filter, nullptr);
	gtk_single_selection_set_selected (self->sel,
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
