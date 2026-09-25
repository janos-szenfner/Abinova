/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Copyright (C) 2006 Robert Staudinger <robert.staudinger@gmail.com>
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

#include "xap_Types.h"

#define ABIWORD_STOCK_PREFIX			"abinova"

#define ABIWORD_FILE_NEW				"abinova-file-new"
#define ABIWORD_FILE_OPEN				"abinova-file-open"
#define ABIWORD_FILE_SAVE				"abinova-file-save"
#define ABIWORD_FILE_SAVEAS				"abinova-file-saveas"
#define ABIWORD_FILE_REVERT				"abinova-file-revert"		// GTK_STOCK_REVERT_TO_SAVED
#define ABIWORD_FILE_PRINT				"abinova-file-print"
#define ABIWORD_FILE_PRINT_PREVIEW		"abinova-file-print-preview"
#define ABIWORD_FILE_PROPERTIES			"abinova-file-properties"	// GTK_STOCK_PROPERTIES
#define ABIWORD_FILE_CLOSE				"abinova-file-close"
#define ABIWORD_FILE_EXIT				"abinova-file-exit"			// GTK_STOCK_QUIT

#define ABIWORD_SPELLCHECK				"abinova-spellcheck"

#define ABIWORD_EDIT_CUT				"abinova-edit-cut"
#define ABIWORD_EDIT_COPY				"abinova-edit-copy"
#define ABIWORD_EDIT_PASTE				"abinova-edit-paste"
#define ABIWORD_FMTPAINTER				"abinova-fmtpainter"
#define ABIWORD_EDIT_UNDO				"abinova-edit-undo"
#define ABIWORD_EDIT_REDO				"abinova-edit-redo"
#define ABIWORD_EDIT_CLEAR				"abinova-edit-clear"		// GTK_STOCK_CLEAR
#define ABIWORD_EDIT_FIND				"abinova-edit-find"			// GTK_STOCK_FIND
#define ABIWORD_EDIT_REPLACE 			"abinova-edit-replace"		// GTK_STOCK_FIND_AND_REPLACE
#define ABIWORD_EDIT_GOTO				"abinova-edit-goto"			// GTK_STOCK_JUMP_TO
#define ABIWORD_EDIT_SELECTALL			"abinova-edit-selectall"	// GTK_STOCK_SELECT_ALL
#define ABIWORD_TOOLS_OPTIONS			"abinova-tools-options"		// GTK_STOCK_PREFERENCES

#define ABIWORD_1COLUMN					"abinova-1column"
#define ABIWORD_2COLUMN					"abinova-2column"
#define ABIWORD_3COLUMN					"abinova-3column"

#define ABIWORD_IMG						"abinova-img"
#define ABIWORD_VIEW_SHOWPARA			"abinova-view-showpara"
#define ABIWORD_HELP					"abinova-help"
#define ABIWORD_HELP_ABOUT				"abinova-help-about"		// GTK_STOCK_ABOUT
#define ABIWORD_FMT_FONT				"abinova-fmt-font"
#define ABIWORD_FMT_BOLD				"abinova-fmt-bold"
#define ABIWORD_FMT_ITALIC				"abinova-fmt-italic"
#define ABIWORD_FMT_UNDERLINE			"abinova-fmt-underline"

#define ABIWORD_ALIGN_LEFT				"abinova-align-left"
#define ABIWORD_ALIGN_CENTER			"abinova-align-center"
#define ABIWORD_ALIGN_RIGHT				"abinova-align-right"
#define ABIWORD_ALIGN_JUSTIFY			"abinova-align-justify"

#define ABIWORD_LISTS_NUMBERS			"abinova-lists-numbers"
#define ABIWORD_LISTS_BULLETS			"abinova-lists-bullets"
#define ABIWORD_LISTS_DASHED			"abinova-lists-dashed"

#define ABIWORD_UNINDENT				"abinova-unindent"
#define ABIWORD_INDENT					"abinova-indent"

#define ABIWORD_COLOR_BACK				"abinova-color_back"
#define ABIWORD_COLOR_FORE				"abinova-color_fore"

#define ABIWORD_INSERT_TABLE			"abinova-insert-table"
#define ABIWORD_ADD_ROW					"abinova-add-row"
#define ABIWORD_ADD_COLUMN				"abinova-add-column"
#define ABIWORD_DELETE_ROW				"abinova-delete-row"
#define ABIWORD_DELETE_COLUMN			"abinova-delete-column"
#define ABIWORD_MERGE_CELLS				"abinova-merge-cells"
#define ABIWORD_SPLIT_CELLS				"abinova-split-cells"

#define ABIWORD_FMT_HYPERLINK			"abinova-fmt-hyperlink"
#define ABIWORD_FMT_BOOKMARK			"abinova-fmt-bookmark"
#define ABIWORD_FMT_OVERLINE			"abinova-fmt-overline"
#define ABIWORD_FMT_STRIKE				"abinova-fmt-strike"
#define ABIWORD_FMT_CLEARFMT			"abinova-fmt-clearfmt"
#define ABIWORD_FMT_FONT				"abinova-fmt-font"
#define ABIWORD_FMT_SUPERSCRIPT			"abinova-fmt-superscript"
#define ABIWORD_FMT_SUBSCRIPT			"abinova-fmt-subscript"

#define ABIWORD_INSERT_SYMBOL			"abinova-insert-symbol"

#define ABIWORD_PARA_0BEFORE			"abinova-para-0before"
#define ABIWORD_PARA_12BEFORE			"abinova-para-12before"

#define ABIWORD_SINGLE_SPACE			"abinova-single-space"
#define ABIWORD_MIDDLE_SPACE			"abinova-middle-space"
#define ABIWORD_DOUBLE_SPACE			"abinova-double-space"

#define ABIWORD_FMT_DIR_OVERRIDE_LTR 	"abinova-fmt-dir-override-ltr"
#define ABIWORD_FMT_DIR_OVERRIDE_RTL 	"abinova-fmt-dir-override-rtl"
#define ABIWORD_FMT_DOM_DIRECTION		"abinova-fmt-dom-direction"

#define ABIWORD_EDIT_HEADER				"abinova-edit-header"
#define ABIWORD_EDIT_FOOTER				"abinova-edit-footer"
#define ABIWORD_EDIT_REMOVEHEADER		"abinova-edit-removeheader"
#define ABIWORD_EDIT_REMOVEFOOTER		"abinova-edit-removefooter"

#define ABIWORD_FMT_CHOOSE              "abinova-fmt-choose"
#define ABIWORD_VIEW_FULL_SCREEN        "abinova-view-full-screen"

#define ABIWORD_REVISIONS_NEW			"abinova-revisions-new"
#define ABIWORD_REVISIONS_SELECT		"abinova-revisions-select"
#define ABIWORD_REVISIONS_SHOW_FINAL	"abinova-revisions-show-final"
#define ABIWORD_REVISIONS_FIND_PREV  	"abinova-revisions-find-prev"
#define ABIWORD_REVISIONS_FIND_NEXT  	"abinova-revisions-find-next"

#define ABIWORD_SEMITEM_THIS            "abinova-semitem-this"
#define ABIWORD_SEMITEM_NEXT            "abinova-semitem-next"
#define ABIWORD_SEMITEM_PREV            "abinova-semitem-prev"
#define ABIWORD_SEMITEM_EDIT            "abinova-semitem-edit"
#define ABIWORD_SEMITEM_STYLESHEET_APPLY "abinova-semitem-stylesheet-apply"

void		  abi_stock_init 				(void);
gchar * 	  abi_stock_from_toolbar_id 	(const gchar *toolbar_id);
const gchar* abi_stock_get_gtk_stock_id(const gchar * abi_stock_id);
const gchar* abi_stock_from_menu_id		(XAP_Menu_Id menu_id);
