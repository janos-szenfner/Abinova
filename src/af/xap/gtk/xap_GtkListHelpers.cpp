/* Abinova
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

#include "xap_GtkListHelpers.h"

#include "ut_assert.h"

GListStore * XAP_list_store_new(void)
{
	return g_list_store_new(XAP_TYPE_DROP_DOWN_ITEM);
}

static void s_list_store_append(GListStore * store, const char * text,
								int int_value, const char * str_value1,
								const char * str_value2)
{
	UT_return_if_fail(store && G_IS_LIST_STORE(store));
	XAPDropDownItem *item =
		xap_drop_down_item_new(text, int_value, str_value1, str_value2);
	g_list_store_append(store, item);
	g_object_unref(item);
}

void XAP_list_store_append_text(GListStore * store, const char * text)
{
	s_list_store_append(store, text, 0, nullptr, nullptr);
}

void XAP_list_store_append_text_and_int(GListStore * store,
										const char * text, int value)
{
	s_list_store_append(store, text, value, nullptr, nullptr);
}

void XAP_list_store_append_text_and_string(GListStore * store,
										   const char * text,
										   const char * value)
{
	s_list_store_append(store, text, 0, value, nullptr);
}

void XAP_list_store_append_text_and_strings(GListStore * store,
											const char * text,
											const char * value1,
											const char * value2)
{
	s_list_store_append(store, text, 0, value1, value2);
}

static void s_list_text_setup(GtkSignalListItemFactory * /*factory*/,
							  GtkListItem * item, gpointer /*data*/)
{
	GtkWidget *label = gtk_label_new(nullptr);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_list_item_set_child(item, label);
}

static void s_list_text_bind(GtkSignalListItemFactory * /*factory*/,
							 GtkListItem * item, gpointer /*data*/)
{
	GtkLabel *label = GTK_LABEL(gtk_list_item_get_child(item));
	gpointer row = gtk_list_item_get_item(item);
	gtk_label_set_text(label,
					   row ? xap_drop_down_item_get_text(
						   XAP_DROP_DOWN_ITEM(row)) : nullptr);
}

GtkListItemFactory * XAP_list_item_text_factory(void)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(s_list_text_setup),
					 nullptr);
	g_signal_connect(factory, "bind", G_CALLBACK(s_list_text_bind),
					 nullptr);
	return factory;
}

static GtkSingleSelection * s_make_selection(GListStore * store)
{
	/* The model must be attached AFTER autoselect is off — passing it
	 * to the ctor makes the default autoselect grab row 0. */
	GtkSingleSelection *sel = gtk_single_selection_new(nullptr);
	/* match the old GtkTreeSelection defaults: no implicit selection,
	 * click-selected rows can be unselected */
	gtk_single_selection_set_autoselect(sel, FALSE);
	gtk_single_selection_set_can_unselect(sel, TRUE);
	gtk_single_selection_set_model(sel, G_LIST_MODEL(store));
	return sel;
}

GtkSingleSelection * XAP_list_view_set_model(GtkListView * view,
											 GListStore * store)
{
	GtkSingleSelection *sel = s_make_selection(store);
	gtk_list_view_set_model(view, GTK_SELECTION_MODEL(sel));
	g_object_unref(sel);

	GtkListItemFactory *factory = XAP_list_item_text_factory();
	gtk_list_view_set_factory(view, factory);
	g_object_unref(factory);

	return GTK_SINGLE_SELECTION(gtk_list_view_get_model(view));
}

GtkWidget * XAP_list_view_new(GListStore * store)
{
	GtkWidget *view = gtk_list_view_new(nullptr, nullptr);
	XAP_list_view_set_model(GTK_LIST_VIEW(view), store);
	return view;
}

GtkSingleSelection * XAP_column_view_set_model(GtkColumnView * view,
											   GListStore * store)
{
	GtkSingleSelection *sel = s_make_selection(store);
	gtk_column_view_set_model(view, GTK_SELECTION_MODEL(sel));
	g_object_unref(sel);
	return GTK_SINGLE_SELECTION(gtk_column_view_get_model(view));
}

GtkColumnViewColumn * XAP_column_view_append_text_column(
	GtkColumnView * view, const char * title, bool sorted)
{
	GtkListItemFactory *factory = XAP_list_item_text_factory();
	GtkColumnViewColumn *col =
		gtk_column_view_column_new(title, factory);
	g_object_unref(factory);

	if (sorted) {
		GtkExpression *expr =
			gtk_property_expression_new(XAP_TYPE_DROP_DOWN_ITEM,
										nullptr, "text");
		GtkSorter *sorter = GTK_SORTER(gtk_string_sorter_new(expr));
		gtk_column_view_column_set_sorter(col, sorter);
		g_object_unref(sorter);
	}

	gtk_column_view_append_column(view, col);
	g_object_unref(col);
	return col;
}

gpointer XAP_single_selection_get_item(GtkSingleSelection * sel)
{
	/* gtk_single_selection_get_selected_item is annotated differently
	 * across GTK builds (borrowed on 4.14); go through the model where
	 * g_list_model_get_item is unambiguously transfer-full. */
	GListModel *model =
		sel ? gtk_single_selection_get_model(sel) : nullptr;
	guint pos = sel ? gtk_single_selection_get_selected(sel)
		: GTK_INVALID_LIST_POSITION;
	gpointer item = nullptr;
	if (model && pos != GTK_INVALID_LIST_POSITION)
		item = g_list_model_get_item(model, pos);
	if (item)
		g_object_unref(item);
	return item;
}

int XAP_single_selection_get_int(GtkSingleSelection * sel)
{
	gpointer p = XAP_single_selection_get_item(sel);
	return p ? xap_drop_down_item_get_int(XAP_DROP_DOWN_ITEM(p)) : -1;
}

const char * XAP_single_selection_get_text(GtkSingleSelection * sel)
{
	gpointer p = XAP_single_selection_get_item(sel);
	return p ? xap_drop_down_item_get_text(XAP_DROP_DOWN_ITEM(p))
		: nullptr;
}

bool XAP_single_selection_select_int(GtkSingleSelection * sel, int value)
{
	GListModel *model =
		sel ? gtk_single_selection_get_model(sel) : nullptr;
	guint n = model ? g_list_model_get_n_items(model) : 0;
	for (guint i = 0; i < n; i++) {
		XAPDropDownItem *item =
			XAP_DROP_DOWN_ITEM(g_list_model_get_item(model, i));
		bool match =
			item && xap_drop_down_item_get_int(item) == value;
		g_object_unref(item);
		if (match) {
			gtk_single_selection_set_selected(sel, i);
			return true;
		}
	}
	return false;
}
