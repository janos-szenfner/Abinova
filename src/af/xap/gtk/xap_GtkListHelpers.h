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

#pragma once

#include <gtk/gtk.h>

#include "xap_GtkComboBoxHelpers.h" /* XAPDropDownItem — the generic row */

/*
 * GtkListView / GtkColumnView helpers — the non-deprecated
 * replacement for GtkTreeView + GtkListStore + GtkCellRenderer
 * (all deprecated since GTK 4.10; our floor is 4.14.5).
 *
 * ===== THE PORTING PATTERN (GTK05-10 batch tasks copy this) =====
 *
 * 1. MODEL — rows are GObjects in a GListStore (NOT GtkTreeModel).
 *    Trivial lists reuse XAPDropDownItem via XAP_list_store_new() +
 *    the XAP_list_store_append_*() calls below; it carries the same
 *    payload the old GtkListStore columns did (text + int + 2 strings).
 *    Dialogs with richer rows declare their own row GObject — copy
 *    AbiRevRow in wp/ap/gtk/ap_UnixDialog_ListRevisions.cpp.  Give the
 *    row real GObject properties if you want GtkExpression-driven
 *    factories/sorters.
 *
 * 2. CELLS — a GtkListItemFactory replaces the cell renderer + column
 *    attribute list.  XAP_list_item_text_factory() renders the row's
 *    "text" property into a left-aligned ellipsizing label and covers
 *    every single-column list and text column.  For custom cells write
 *    a setup/bind pair: "setup" creates the child widget once,
 *    "bind" reads the row via gtk_list_item_get_item() and fills the
 *    widget — see s_rev_setup / s_rev_bind_* in ListRevisions, and the
 *    font-preview bind planned for FontChooser (GTK06).
 *
 * 3. VIEW + SELECTION — GtkSelectionModel replaces GtkTreeSelection.
 *    XAP_list_view_set_model() / XAP_column_view_set_model() wrap the
 *    store in a GtkSingleSelection, install it, and return the
 *    selection model (owned by the view — do not unref).  Defaults
 *    keep TreeView semantics: autoselect off, unselect allowed.
 *    Read the selection through the helpers below or
 *    gtk_single_selection_get_selected()/set_selected(); the latter
 *    takes a POSITION (use GTK_INVALID_LIST_POSITION to clear).
 *
 * 4. DOUBLE-CLICK/ENTER — connect the view's "activate" signal
 *    (replaces GtkTreeView::row-activated); the callback gets the row
 *    position, not a GtkTreePath.
 *
 * 5. SORTING (ColumnView only) — give each column a sorter and point
 *    the model at the view's composite sorter:
 *      gtk_column_view_column_set_sorter(col, GTK_SORTER(sorter));
 *      gtk_sort_list_model_set_sorter(sort,
 *          gtk_column_view_get_sorter(view));
 *    For XAPDropDownItem text columns
 *      gtk_string_sorter_new(gtk_property_expression_new(
 *          XAP_TYPE_DROP_DOWN_ITEM, nullptr, "text"))
 *    works out of the box; custom rows need matching properties or a
 *    GtkCustomSorter (see s_sort_rev_* in ListRevisions).
 *    XAP_column_view_append_text_column(..., sorted=true) installs the
 *    per-column sorter; the model side still needs a GtkSortListModel
 *    wrap — keep it explicit like ListRevisions.
 *
 * 6. TREES — GtkTreeListModel + GtkTreeExpander for hierarchical data;
 *    reference port is ap_UnixDialog_Stylist.cpp (G01).
 *
 * Ownership note: gtk_single_selection_get_selected_item() is
 * annotated differently across GTK builds (observed borrowed on 4.14);
 * fetch selected items through the helpers here, which go via
 * g_list_model_get_item() + unref and are always borrowed.
 */

/// a GListStore of XAPDropDownItem rows
GListStore * XAP_list_store_new(void);

void XAP_list_store_append_text(GListStore * store, const char * text);
void XAP_list_store_append_text_and_int(GListStore * store,
										const char * text, int value);
void XAP_list_store_append_text_and_string(GListStore * store,
										   const char * text,
										   const char * value);
void XAP_list_store_append_text_and_strings(GListStore * store,
											const char * text,
											const char * value1,
											const char * value2);

/// factory rendering the row item's "text" GObject property as a
/// left-aligned ellipsizing label — the default cell for text rows
GtkListItemFactory * XAP_list_item_text_factory(void);

/// Wrap store in a GtkSingleSelection (autoselect off, can-unselect on
/// — the same semantics a GtkTreeView had) and install it plus the
/// default text factory on the view.  Returns the selection model,
/// borrowed from the view (do not unref).
GtkSingleSelection * XAP_list_view_set_model(GtkListView * view,
											 GListStore * store);

/// Standalone single-column text list, ready to pack.
GtkWidget * XAP_list_view_new(GListStore * store);

/// ColumnView twin of XAP_list_view_set_model — install the selection
/// model only; columns (and their factories) are appended separately.
GtkSingleSelection * XAP_column_view_set_model(GtkColumnView * view,
											   GListStore * store);

/// Append a column rendering the row's "text" property.  When sorted
/// is true the column gets a string sorter on "text" — the caller must
/// still wrap the store in a GtkSortListModel fed by
/// gtk_column_view_get_sorter() for the header clicks to reorder.
GtkColumnViewColumn * XAP_column_view_append_text_column(
	GtkColumnView * view, const char * title, bool sorted);

/// Selection accessors.  Returned pointers are borrowed and only
/// valid while the model lives; NULL/-1 when nothing is selected.
gpointer XAP_single_selection_get_item(GtkSingleSelection * sel);
int XAP_single_selection_get_int(GtkSingleSelection * sel);
const char * XAP_single_selection_get_text(GtkSingleSelection * sel);

/// Select the first row whose int payload equals value.
bool XAP_single_selection_select_int(GtkSingleSelection * sel, int value);

/// Move the selection one row forward/back with wrap-around — the
/// semantics of the old selectNext/selectPrev tree-view helpers:
/// next past the last row wraps to the first (and picks the first
/// when nothing is selected), prev before the first row wraps to the
/// last (and picks the last when nothing is selected).  No-op on an
/// empty model.
void XAP_single_selection_select_next(GtkSingleSelection * sel);
void XAP_single_selection_select_prev(GtkSingleSelection * sel);
