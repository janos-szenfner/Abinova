/* Abinova
 * Copyright (C) 2009,2012 Hubert Figuiere
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

#include "xap_GtkComboBoxHelpers.h"

void XAP_makeGtkComboBoxText(GtkComboBox * combo, GType secondary)
{
	GtkListStore * store;
	if (secondary != G_TYPE_NONE) {
		store = gtk_list_store_new(2, G_TYPE_STRING, secondary);
	}
	else {
		store = gtk_list_store_new(1, G_TYPE_STRING);
	}
	gtk_combo_box_set_model(combo, GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));

	gtk_cell_layout_clear(GTK_CELL_LAYOUT(combo));
	GtkCellRenderer *cell = GTK_CELL_RENDERER(gtk_cell_renderer_text_new());
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), cell, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), cell,
								   "text", 0, nullptr);
}

void XAP_makeGtkComboBoxText2(GtkComboBox * combo, GType secondary,
							  GType tertiary)
{
	GtkListStore * store;
	store = gtk_list_store_new(3, G_TYPE_STRING, secondary, tertiary);
	gtk_combo_box_set_model(combo, GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	
	gtk_cell_layout_clear(GTK_CELL_LAYOUT(combo));
	GtkCellRenderer *cell = GTK_CELL_RENDERER(gtk_cell_renderer_text_new());
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), cell, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), cell,
								   "text", 0, nullptr);
}

void XAP_populateComboBoxWithIndex(GtkComboBox * combo, 
								   const UT_GenericVector<const char*> & vec)
{
	GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model(combo));
	GtkTreeIter iter;
	
	for(UT_sint32 i = 0; i < vec.getItemCount(); i++) {
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, vec[i], 1, i, -1);
	}
}

void XAP_appendComboBoxText(GtkComboBox* combo, const char* text)
{
	GtkTreeIter iter;
	GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model(combo));
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, text, -1);
}

void XAP_appendComboBoxTextAndInt(GtkComboBox * combo, const char * text,
								  int value)
{
	GtkTreeIter iter;
	GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model(combo));
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, text, 1, value, -1);
}

void XAP_appendComboBoxTextAndString(GtkComboBox * combo, const char * text,
									 const char * value)
{
	GtkTreeIter iter;
	GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model(combo));
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, text, 1, value, -1);
}

void XAP_appendComboBoxTextAndStringString(GtkComboBox * combo, 
										   const char * text,
										   const char * value,
										   const char *value2)
{
	GtkTreeIter iter;
	GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model(combo));
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, text, 1, value, 2, value2, -1);
}

void XAP_appendComboBoxTextAndIntString(GtkComboBox * combo, 
										const char * text,
										const int value,
										const char *value2)
{
	GtkTreeIter iter;
	GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model(combo));
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, text, 1, value, 2, value2, -1);
}

int  XAP_comboBoxGetActiveInt(GtkComboBox * combo)
{
	int value = 0;
	GtkTreeIter iter;
	if (!gtk_combo_box_get_active_iter(combo, &iter))
		return 0;
	GtkTreeModel *store = gtk_combo_box_get_model(combo);
	gtk_tree_model_get(store, &iter, 1, &value, -1);
	return value;
}

std::string XAP_comboBoxGetActiveText(GtkComboBox * combo)
{
	char* value = nullptr;
	GtkTreeIter iter;
	if (!gtk_combo_box_get_active_iter(combo, &iter))
		return std::string();
	GtkTreeModel *store = gtk_combo_box_get_model(combo);
	gtk_tree_model_get(store, &iter, 0, &value, -1);
	std::string result = value ? value : "";
	g_free(value);
	return result;
}

bool XAP_comboBoxSetActiveFromIntCol(GtkComboBox * combo, 
									 int col, int value)
{
	GtkTreeIter iter;
	GtkTreeModel *store = gtk_combo_box_get_model(combo);
	if(gtk_tree_model_get_iter_first(store, &iter)) {
		do {
			int v;
			gtk_tree_model_get(store, &iter, col, &v, -1);
			if(v == value) {
				gtk_combo_box_set_active_iter(combo, &iter);
				return true;
			}
		} while(gtk_tree_model_iter_next(store, &iter));
	}
	return false;
}


/*
 * XAPDropDownItem — one row of a XAP_makeGtkDropDown() model.  Carries
 * the display text plus the payload values the old GtkListStore columns
 * held (int + up to two strings); unused payloads stay NULL/0.
 */

struct _XAPDropDownItem
{
	GObject parent_instance;
	char *text;
	int int_value;
	char *str_value1;
	char *str_value2;
};

enum
{
	PROP_0,
	PROP_TEXT,
	N_PROPERTIES
};

static GParamSpec *s_item_props[N_PROPERTIES] = { nullptr };

G_DEFINE_TYPE(XAPDropDownItem, xap_drop_down_item, G_TYPE_OBJECT)

static void xap_drop_down_item_get_property(GObject * object,
											guint prop_id, GValue * value,
											GParamSpec * pspec)
{
	XAPDropDownItem *item = XAP_DROP_DOWN_ITEM(object);
	if (prop_id == PROP_TEXT) {
		g_value_set_string(value, item->text);
	}
	else {
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

static void xap_drop_down_item_finalize(GObject * object)
{
	XAPDropDownItem *item = XAP_DROP_DOWN_ITEM(object);
	g_free(item->text);
	g_free(item->str_value1);
	g_free(item->str_value2);
	G_OBJECT_CLASS(xap_drop_down_item_parent_class)->finalize(object);
}

static void xap_drop_down_item_class_init(XAPDropDownItemClass * klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS(klass);
	oclass->finalize = xap_drop_down_item_finalize;
	oclass->get_property = xap_drop_down_item_get_property;
	s_item_props[PROP_TEXT] =
		g_param_spec_string("text", nullptr, nullptr, nullptr,
							static_cast<GParamFlags>(G_PARAM_READABLE));
	g_object_class_install_properties(oclass, N_PROPERTIES, s_item_props);
}

static void xap_drop_down_item_init(XAPDropDownItem * /*item*/)
{
}

static XAPDropDownItem * s_drop_down_item_new(const char * text,
											  int int_value,
											  const char * str_value1,
											  const char * str_value2)
{
	XAPDropDownItem *item =
		XAP_DROP_DOWN_ITEM(g_object_new(XAP_TYPE_DROP_DOWN_ITEM, nullptr));
	item->text = g_strdup(text);
	item->int_value = int_value;
	item->str_value1 = g_strdup(str_value1);
	item->str_value2 = g_strdup(str_value2);
	return item;
}

const char * xap_drop_down_item_get_text(XAPDropDownItem * item)
{
	return item ? item->text : nullptr;
}

int xap_drop_down_item_get_int(XAPDropDownItem * item)
{
	return item ? item->int_value : 0;
}

const char * xap_drop_down_item_get_string1(XAPDropDownItem * item)
{
	return item ? item->str_value1 : nullptr;
}

const char * xap_drop_down_item_get_string2(XAPDropDownItem * item)
{
	return item ? item->str_value2 : nullptr;
}

static void s_drop_down_setup(GtkSignalListItemFactory * /*factory*/,
							  GtkListItem * item, gpointer /*data*/)
{
	GtkWidget *label = gtk_label_new(nullptr);
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_list_item_set_child(item, label);
}

static void s_drop_down_bind(GtkSignalListItemFactory * /*factory*/,
							 GtkListItem * item, gpointer /*data*/)
{
	GtkLabel *label = GTK_LABEL(gtk_list_item_get_child(item));
	XAPDropDownItem *row =
		XAP_DROP_DOWN_ITEM(gtk_list_item_get_item(item));
	gtk_label_set_text(label, row ? row->text : nullptr);
}

void XAP_makeGtkDropDown(GtkDropDown * dd)
{
	GListStore *store = g_list_store_new(XAP_TYPE_DROP_DOWN_ITEM);
	gtk_drop_down_set_model(dd, G_LIST_MODEL(store));
	g_object_unref(store);

	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(s_drop_down_setup), nullptr);
	g_signal_connect(factory, "bind", G_CALLBACK(s_drop_down_bind), nullptr);
	gtk_drop_down_set_factory(dd, factory);
	g_object_unref(factory);

	GtkExpression *expr =
		gtk_property_expression_new(XAP_TYPE_DROP_DOWN_ITEM, nullptr, "text");
	gtk_drop_down_set_expression(dd, expr);
	gtk_expression_unref(expr);
}

static void s_drop_down_append(GtkDropDown * dd, const char * text,
							   int int_value, const char * str_value1,
							   const char * str_value2)
{
	GListStore *store =
		G_LIST_STORE(gtk_drop_down_get_model(dd));
	UT_return_if_fail(store && G_IS_LIST_STORE(store));
	XAPDropDownItem *item =
		s_drop_down_item_new(text, int_value, str_value1, str_value2);
	g_list_store_append(store, item);
	g_object_unref(item);
}

void XAP_populateDropDownWithIndex(GtkDropDown * dd,
								   const UT_GenericVector<const char*> & vec)
{
	for(UT_sint32 i = 0; i < vec.getItemCount(); i++) {
		XAP_appendDropDownTextAndInt(dd, vec[i], i);
	}
}

void XAP_appendDropDownText(GtkDropDown * dd, const char * text)
{
	s_drop_down_append(dd, text, 0, nullptr, nullptr);
}

void XAP_appendDropDownTextAndInt(GtkDropDown * dd, const char * text,
								  int value)
{
	s_drop_down_append(dd, text, value, nullptr, nullptr);
}

void XAP_appendDropDownTextAndString(GtkDropDown * dd, const char * text,
									 const char * value)
{
	s_drop_down_append(dd, text, 0, value, nullptr);
}

void XAP_appendDropDownTextAndStringString(GtkDropDown * dd,
										   const char * text,
										   const char * value1,
										   const char * value2)
{
	s_drop_down_append(dd, text, 0, value1, value2);
}

void XAP_appendDropDownTextAndIntString(GtkDropDown * dd,
										const char * text,
										int value1,
										const char * value2)
{
	s_drop_down_append(dd, text, value1, nullptr, value2);
}

XAPDropDownItem * XAP_dropDownGetSelectedItem(GtkDropDown * dd)
{
	/* Fetch the item through the list model — g_list_model_get_item is
	 * unambiguously transfer-full, whereas gtk_drop_down_get_selected_item
	 * returns a borrowed pointer on some GTK builds.  The model keeps the
	 * item alive, so the returned pointer is borrowed. */
	GListModel *model = gtk_drop_down_get_model(dd);
	guint pos = gtk_drop_down_get_selected(dd);
	XAPDropDownItem *item = nullptr;
	if (model && pos != GTK_INVALID_LIST_POSITION)
		item = XAP_DROP_DOWN_ITEM(g_list_model_get_item(model, pos));
	if (item)
		g_object_unref(item);
	return item;
}

int XAP_dropDownGetSelectedInt(GtkDropDown * dd)
{
	return xap_drop_down_item_get_int(XAP_dropDownGetSelectedItem(dd));
}

std::string XAP_dropDownGetSelectedText(GtkDropDown * dd)
{
	const char *text =
		xap_drop_down_item_get_text(XAP_dropDownGetSelectedItem(dd));
	return text ? std::string(text) : std::string();
}

const char * XAP_dropDownGetSelectedString(GtkDropDown * dd)
{
	return xap_drop_down_item_get_string1(XAP_dropDownGetSelectedItem(dd));
}

const char * XAP_dropDownGetSelectedString2(GtkDropDown * dd)
{
	return xap_drop_down_item_get_string2(XAP_dropDownGetSelectedItem(dd));
}

static bool s_drop_down_select_where(GtkDropDown * dd,
									 int int_value, const char * text,
									 const char * str_value1)
{
	GListModel *model = gtk_drop_down_get_model(dd);
	guint n = model ? g_list_model_get_n_items(model) : 0;
	for (guint i = 0; i < n; i++) {
		XAPDropDownItem *item =
			XAP_DROP_DOWN_ITEM(g_list_model_get_item(model, i));
		bool match = text ? (item->text && !g_strcmp0(item->text, text))
			: str_value1 ? (item->str_value1 &&
							!g_strcmp0(item->str_value1, str_value1))
			: item->int_value == int_value;
		g_object_unref(item);
		if (match) {
			gtk_drop_down_set_selected(dd, i);
			return true;
		}
	}
	return false;
}

bool XAP_dropDownSetSelectedFromInt(GtkDropDown * dd, int value)
{
	return s_drop_down_select_where(dd, value, nullptr, nullptr);
}

bool XAP_dropDownSetSelectedFromText(GtkDropDown * dd, const char * text)
{
	return s_drop_down_select_where(dd, 0, text, nullptr);
}

bool XAP_dropDownSetSelectedFromString(GtkDropDown * dd, const char * value)
{
	return s_drop_down_select_where(dd, 0, nullptr, value);
}
