/* AbiWord
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


/*****************************************************************
******************************************************************
** THIS FILE DEFINES THE CONTEXT MENU SHOWN FOR A RIGHT-CLICK
** INSIDE A TABLE CELL, matching the structure Word uses.
** IT IS IMPORTANT THAT THIS FILE ALLOW ITSELF TO BE INCLUDED
** MORE THAN ONE TIME.
******************************************************************
*****************************************************************/

BeginLayout(ContextTable,EV_EMC_TABLE)

	BeginPopupMenu()
		MenuItem(AP_MENU_ID_EDIT_CUT)
		MenuItem(AP_MENU_ID_EDIT_COPY)
		MenuItem(AP_MENU_ID_EDIT_PASTE)
		MenuItem(AP_MENU_ID_EDIT_PASTE_SPECIAL)
		Separator()
		MenuItem(AP_MENU_ID_TABLE_INSERTTABLE)
		Separator()
	BeginSubMenu(AP_MENU_ID_TABLE_INSERT)
		MenuItem(AP_MENU_ID_TABLE_INSERT_ROWS_BEFORE)
		MenuItem(AP_MENU_ID_TABLE_INSERT_ROWS_AFTER)
		MenuItem(AP_MENU_ID_TABLE_INSERT_COLUMNS_BEFORE)
		MenuItem(AP_MENU_ID_TABLE_INSERT_COLUMNS_AFTER)
	EndSubMenu()
	BeginSubMenu(AP_MENU_ID_TABLE_DELETE)
		MenuItem(AP_MENU_ID_TABLE_DELETE_ROWS)
		MenuItem(AP_MENU_ID_TABLE_DELETE_COLUMNS)
		MenuItem(AP_MENU_ID_TABLE_DELETE_TABLE)
	EndSubMenu()
	BeginSubMenu(AP_MENU_ID_TABLE_SELECT)
		MenuItem(AP_MENU_ID_TABLE_SELECT_CELL)
		MenuItem(AP_MENU_ID_TABLE_SELECT_ROW)
		MenuItem(AP_MENU_ID_TABLE_SELECT_COLUMN)
		MenuItem(AP_MENU_ID_TABLE_SELECT_TABLE)
	EndSubMenu()
		Separator()
		MenuItem(AP_MENU_ID_TABLE_MERGE_CELLS)
		MenuItem(AP_MENU_ID_TABLE_SPLIT_CELLS)
		MenuItem(AP_MENU_ID_TABLE_SPLIT_TABLE)
		Separator()
		MenuItem(AP_MENU_ID_TABLE_AUTOFIT)
		MenuItem(AP_MENU_ID_TABLE_AUTOFIT_CONTENTS)
		MenuItem(AP_MENU_ID_TABLE_AUTOFIT_WINDOW)
		MenuItem(AP_MENU_ID_TABLE_AUTOFIT_FIXED)
		MenuItem(AP_MENU_ID_TABLE_DISTRIBUTE_ROWS)
		MenuItem(AP_MENU_ID_TABLE_DISTRIBUTE_COLS)
		Separator()
	BeginSubMenu(AP_MENU_ID_TABLE_TABLETOTEXT)
		MenuItem(AP_MENU_ID_TABLE_TABLETOTEXTCOMMAS)
		MenuItem(AP_MENU_ID_TABLE_TABLETOTEXTTABS)
		MenuItem(AP_MENU_ID_TABLE_TABLETOTEXTCOMMASTABS)
	EndSubMenu()
		Separator()
		MenuItem(AP_MENU_ID_TABLE_FORMAT)
		MenuItem(AP_MENU_ID_TABLE_VIEW_GRIDLINES)
	EndPopupMenu()

EndLayout()
