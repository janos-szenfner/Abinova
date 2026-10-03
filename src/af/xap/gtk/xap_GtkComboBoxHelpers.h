/* AbiWord
 * Copyright (C) 2009,2012 Hubert Figuiere
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

#include "ut_vector.h"
#include <string>

void XAP_makeGtkComboBoxText(GtkComboBox * combo, GType secondaryType);
void XAP_makeGtkComboBoxText2(GtkComboBox * combo, GType secondaryType,
							  GType tertiaryType);
void XAP_populateComboBoxWithIndex(GtkComboBox * combo,
								   const UT_GenericVector<const char*> & vec);

void XAP_appendComboBoxText(GtkComboBox* combo, const char* text);
void XAP_appendComboBoxTextAndInt(GtkComboBox * combo, const char * text, int value);
void XAP_appendComboBoxTextAndString(GtkComboBox * combo, const char * text,
									 const char * value);
void XAP_appendComboBoxTextAndStringString(GtkComboBox * combo,
										   const char * text,
										   const char * value1,
										   const char * value2);
void XAP_appendComboBoxTextAndIntString(GtkComboBox * combo,
										   const char * text,
										   int value1,
										   const char * value2);
int  XAP_comboBoxGetActiveInt(GtkComboBox * combo);
std::string XAP_comboBoxGetActiveText(GtkComboBox * combo);



/** set the active item based on a column value
 * @param combo the combobox
 * @param col the column
 * @param value the value to look for
 * @return true if set, false if not found.
 */
bool XAP_comboBoxSetActiveFromIntCol(GtkComboBox * combo,
									 int col, int value);


/*
 * GtkDropDown helpers — the non-deprecated replacement for the
 * GtkComboBox helpers above (GtkComboBox is deprecated since GTK 4.10).
 *
 * A GtkDropDown is fed a GListModel, not a GtkTreeModel, so the model
 * columns are replaced by a small item object carrying the same payload
 * shapes the helpers used: a display text plus optional int and two
 * string values.
 */

#define XAP_TYPE_DROP_DOWN_ITEM (xap_drop_down_item_get_type())
G_DECLARE_FINAL_TYPE(XAPDropDownItem, xap_drop_down_item,
					 XAP, DROP_DOWN_ITEM, GObject)

const char * xap_drop_down_item_get_text(XAPDropDownItem * item);
int          xap_drop_down_item_get_int(XAPDropDownItem * item);
const char * xap_drop_down_item_get_string1(XAPDropDownItem * item);
const char * xap_drop_down_item_get_string2(XAPDropDownItem * item);

/** install the item store + text factory on a GtkDropDown created by
 * gtk_drop_down_new() or loaded from a builder file */
void XAP_makeGtkDropDown(GtkDropDown * dd);

void XAP_populateDropDownWithIndex(GtkDropDown * dd,
								   const UT_GenericVector<const char*> & vec);

void XAP_appendDropDownText(GtkDropDown * dd, const char * text);
void XAP_appendDropDownTextAndInt(GtkDropDown * dd, const char * text,
								  int value);
void XAP_appendDropDownTextAndString(GtkDropDown * dd, const char * text,
									 const char * value);
void XAP_appendDropDownTextAndStringString(GtkDropDown * dd,
										   const char * text,
										   const char * value1,
										   const char * value2);
void XAP_appendDropDownTextAndIntString(GtkDropDown * dd,
										const char * text,
										int value1,
										const char * value2);

XAPDropDownItem * XAP_dropDownGetSelectedItem(GtkDropDown * dd);
int  XAP_dropDownGetSelectedInt(GtkDropDown * dd);
std::string XAP_dropDownGetSelectedText(GtkDropDown * dd);
/** borrowed pointers, owned by the selected item — do not free */
const char * XAP_dropDownGetSelectedString(GtkDropDown * dd);
const char * XAP_dropDownGetSelectedString2(GtkDropDown * dd);

/** set the selected item based on a payload value
 * @param dd the drop-down
 * @param value the value to look for
 * @return true if set, false if not found.
 */
bool XAP_dropDownSetSelectedFromInt(GtkDropDown * dd, int value);
bool XAP_dropDownSetSelectedFromText(GtkDropDown * dd, const char * text);
bool XAP_dropDownSetSelectedFromString(GtkDropDown * dd, const char * value);
