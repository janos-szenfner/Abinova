/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiWord
 * Copyright (C) 2026 AbiSource, Inc.
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

#include "ap_Menu_Id.h"

/*
 * Ribbon UI layout description.
 *
 * This table is preparation for a LibreOffice-style ribbon interface.
 * It reuses the existing AP_MENU_ID_* actions, so a ribbon widget
 * (e.g. a GtkNotebook of button groups) can bind the same EV_Menu_ActionSet,
 * EV_Menu_LabelSet and state functions the menubar already uses.  No new
 * actions are defined here; only grouping and ordering.
 *
 * Tabs follow the LibreOffice Writer convention:
 *   Home     - clipboard, font, paragraph, lists, styles
 *   Insert   - breaks, tables, fields, images, links
 *   Layout   - view modes, page setup, zoom
 *   Review   - spelling, language, revisions, annotations
 *   View     - rulers, toolbars, status bar, fullscreen
 *   Table    - contextual tab, shown only while the caret is in a table
 *   Help     - about, credits, help contents
 *
 * The classic menubar remains the default; the ribbon table is consumed
 * by the platform UI layer when a "ribbon" UI mode is requested.
 */

struct AP_RibbonGroup
{
	const char *			szGroupKey;	/* untranslated group key -> label set */
	const uint16_t *		items;		/* AP_MENU_ID__BOGUS1__-terminated list */
};

struct AP_RibbonTab
{
	const char *				szTabKey;	/* untranslated tab key -> label set */
	const AP_RibbonGroup *		groups;		/* nullptr-terminated group list */
	bool						bContextual;/* show only when context applies */
};

/* --------------------------------------------------------------- File --- */

static const uint16_t s_ribbon_file_document[] =
{
	AP_MENU_ID_FILE_NEW,
	AP_MENU_ID_FILE_NEW_USING_TEMPLATE,
	AP_MENU_ID_FILE_OPEN,
	AP_MENU_ID_FILE_SAVE,
	AP_MENU_ID_FILE_SAVEAS,
	AP_MENU_ID_FILE_REVERT,
	AP_MENU_ID_FILE_PROPERTIES,
	AP_MENU_ID_FILE_CLOSE,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_file_print[] =
{
	AP_MENU_ID_FILE_PAGESETUP,
	AP_MENU_ID_FILE_PRINT_PREVIEW,
	AP_MENU_ID_FILE_PRINT,
	AP_MENU_ID_FILE_EXPORT,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const AP_RibbonGroup s_ribbon_file_groups[] =
{
	{ "document",	s_ribbon_file_document },
	{ "print",		s_ribbon_file_print },
	{ nullptr,		nullptr }
};

/* --------------------------------------------------------------- Home --- */

static const uint16_t s_ribbon_home_clipboard[] =
{
	AP_MENU_ID_EDIT_CUT,
	AP_MENU_ID_EDIT_COPY,
	AP_MENU_ID_EDIT_PASTE,
	AP_MENU_ID_EDIT_PASTE_SPECIAL,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_home_font[] =
{
	AP_MENU_ID_FMT_BOLD,
	AP_MENU_ID_FMT_ITALIC,
	AP_MENU_ID_FMT_UNDERLINE,
	AP_MENU_ID_FMT_OVERLINE,
	AP_MENU_ID_FMT_STRIKE,
	AP_MENU_ID_FMT_SUPERSCRIPT,
	AP_MENU_ID_FMT_SUBSCRIPT,
	AP_MENU_ID_FMT_FONT,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_home_paragraph[] =
{
	AP_MENU_ID_ALIGN_LEFT,
	AP_MENU_ID_ALIGN_CENTER,
	AP_MENU_ID_ALIGN_RIGHT,
	AP_MENU_ID_ALIGN_JUSTIFY,
	AP_MENU_ID_FMT_PARAGRAPH,
	AP_MENU_ID_FMT_COLUMNS,
	AP_MENU_ID_FMT_BORDERS,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_home_lists[] =
{
	AP_MENU_ID_FMT_BULLETS,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_home_styles[] =
{
	AP_MENU_ID_FMT_STYLIST,
	AP_MENU_ID_FMT_STYLE_DEFINE,
	AP_MENU_ID_FMT_TOGGLECASE,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const AP_RibbonGroup s_ribbon_home_groups[] =
{
	{ "clipboard",	s_ribbon_home_clipboard },
	{ "font",		s_ribbon_home_font },
	{ "paragraph",	s_ribbon_home_paragraph },
	{ "lists",		s_ribbon_home_lists },
	{ "styles",		s_ribbon_home_styles },
	{ nullptr,		nullptr }
};

/* ------------------------------------------------------------- Insert --- */

static const uint16_t s_ribbon_insert_pages[] =
{
	AP_MENU_ID_INSERT_BREAK,
	AP_MENU_ID_INSERT_HEADER,
	AP_MENU_ID_INSERT_FOOTER,
	AP_MENU_ID_INSERT_PAGENO,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_insert_objects[] =
{
	AP_MENU_ID_TABLE_INSERT_TABLE,
	AP_MENU_ID_INSERT_TEXTBOX,
	AP_MENU_ID_INSERT_GRAPHIC,
	AP_MENU_ID_INSERT_CLIPART,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_insert_fields[] =
{
	AP_MENU_ID_INSERT_DATETIME,
	AP_MENU_ID_INSERT_FIELD,
	AP_MENU_ID_INSERT_SYMBOL,
	AP_MENU_ID_INSERT_FOOTNOTE,
	AP_MENU_ID_INSERT_ENDNOTE,
	AP_MENU_ID_INSERT_TABLEOFCONTENTS,
	AP_MENU_ID_INSERT_BOOKMARK,
	AP_MENU_ID_INSERT_XMLID,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_insert_links[] =
{
	AP_MENU_ID_INSERT_HYPERLINK,
	AP_MENU_ID_INSERT_FILE,
	AP_MENU_ID_INSERT_MAILMERGE,
	AP_MENU_ID_INSERT_DIRECTIONMARKER_LRM,
	AP_MENU_ID_INSERT_DIRECTIONMARKER_RLM,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const AP_RibbonGroup s_ribbon_insert_groups[] =
{
	{ "pages",		s_ribbon_insert_pages },
	{ "objects",	s_ribbon_insert_objects },
	{ "fields",		s_ribbon_insert_fields },
	{ "links",		s_ribbon_insert_links },
	{ nullptr,		nullptr }
};

/* ------------------------------------------------------------- Layout --- */

static const uint16_t s_ribbon_layout_views[] =
{
	AP_MENU_ID_VIEW_NORMAL,
	AP_MENU_ID_VIEW_WEB,
	AP_MENU_ID_VIEW_PRINT,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_layout_page[] =
{
	AP_MENU_ID_FILE_PAGESETUP,
	AP_MENU_ID_FMT_BACKGROUND_PAGE_COLOR,
	AP_MENU_ID_FMT_BACKGROUND_PAGE_IMAGE,
	AP_MENU_ID_FMT_HDRFTR,
	AP_MENU_ID_FMT_TABS,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_layout_zoom[] =
{
	AP_MENU_ID_VIEW_ZOOM,
	AP_MENU_ID_VIEW_ZOOM_WIDTH,
	AP_MENU_ID_VIEW_ZOOM_WHOLE,
	AP_MENU_ID_VIEW_ZOOM_200,
	AP_MENU_ID_VIEW_ZOOM_100,
	AP_MENU_ID_VIEW_ZOOM_75,
	AP_MENU_ID_VIEW_ZOOM_50,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const AP_RibbonGroup s_ribbon_layout_groups[] =
{
	{ "views",		s_ribbon_layout_views },
	{ "page",		s_ribbon_layout_page },
	{ "zoom",		s_ribbon_layout_zoom },
	{ nullptr,		nullptr }
};

/* ------------------------------------------------------------- Review --- */

static const uint16_t s_ribbon_review_proofing[] =
{
	AP_MENU_ID_TOOLS_SPELL,
	AP_MENU_ID_FMT_LANGUAGE,
	AP_MENU_ID_TOOLS_WORDCOUNT,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_review_revisions[] =
{
	AP_MENU_ID_TOOLS_REVISIONS_MARK,
	AP_MENU_ID_TOOLS_REVISIONS_NEW_REVISION,
	AP_MENU_ID_TOOLS_REVISIONS_SHOW,
	AP_MENU_ID_TOOLS_REVISIONS_SHOW_AFTER,
	AP_MENU_ID_TOOLS_REVISIONS_SHOW_AFTERPREV,
	AP_MENU_ID_TOOLS_REVISIONS_SHOW_BEFORE,
	AP_MENU_ID_TOOLS_REVISIONS_SET_VIEW_LEVEL,
	AP_MENU_ID_TOOLS_REVISIONS_FIND_NEXT,
	AP_MENU_ID_TOOLS_REVISIONS_FIND_PREV,
	AP_MENU_ID_TOOLS_REVISIONS_ACCEPT_REVISION,
	AP_MENU_ID_TOOLS_REVISIONS_REJECT_REVISION,
	AP_MENU_ID_TOOLS_REVISIONS_PURGE,
	AP_MENU_ID_TOOLS_REVISIONS_COMPARE_DOCUMENTS,
	AP_MENU_ID_TOOLS_REVISIONS_AUTO,
	AP_MENU_ID_TOOLS_HISTORY_SHOW,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_review_annotations[] =
{
	AP_MENU_ID_TOOLS_ANNOTATIONS_INSERT,
	AP_MENU_ID_TOOLS_ANNOTATIONS_INSERT_FROMSEL,
	AP_MENU_ID_TOOLS_ANNOTATIONS_TOGGLE_DISPLAY,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const AP_RibbonGroup s_ribbon_review_groups[] =
{
	{ "proofing",		s_ribbon_review_proofing },
	{ "revisions",		s_ribbon_review_revisions },
	{ "annotations",	s_ribbon_review_annotations },
	{ nullptr,			nullptr }
};

/* --------------------------------------------------------------- View --- */

static const uint16_t s_ribbon_view_show[] =
{
	AP_MENU_ID_VIEW_RULER,
	AP_MENU_ID_VIEW_STATUSBAR,
	AP_MENU_ID_VIEW_TB_1,
	AP_MENU_ID_VIEW_TB_2,
	AP_MENU_ID_VIEW_TB_3,
	AP_MENU_ID_VIEW_TB_4,
	AP_MENU_ID_VIEW_SHOWPARA,
	AP_MENU_ID_VIEW_LOCKSTYLES,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_view_window[] =
{
	AP_MENU_ID_VIEW_FULLSCREEN,
	AP_MENU_ID_WEB_WEBPREVIEW,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const AP_RibbonGroup s_ribbon_view_groups[] =
{
	{ "show",	s_ribbon_view_show },
	{ "window",	s_ribbon_view_window },
	{ nullptr,	nullptr }
};

/* -------------------------------------------- Table (contextual tab) --- */

static const uint16_t s_ribbon_table_insert[] =
{
	AP_MENU_ID_TABLE_INSERT_TABLE,
	AP_MENU_ID_TABLE_INSERT_COLUMNS_BEFORE,
	AP_MENU_ID_TABLE_INSERT_COLUMNS_AFTER,
	AP_MENU_ID_TABLE_INSERT_ROWS_BEFORE,
	AP_MENU_ID_TABLE_INSERT_ROWS_AFTER,
	AP_MENU_ID_TABLE_INSERT_SUMCOLS,
	AP_MENU_ID_TABLE_INSERT_SUMROWS,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_table_delete[] =
{
	AP_MENU_ID_TABLE_DELETE_TABLE,
	AP_MENU_ID_TABLE_DELETE_COLUMNS,
	AP_MENU_ID_TABLE_DELETE_ROWS,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_table_select[] =
{
	AP_MENU_ID_TABLE_SELECT_TABLE,
	AP_MENU_ID_TABLE_SELECT_COLUMN,
	AP_MENU_ID_TABLE_SELECT_ROW,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_table_format[] =
{
	AP_MENU_ID_FMT_TABLE,
	AP_MENU_ID_TABLE_MERGE_CELLS,
	AP_MENU_ID_TABLE_SPLIT_CELLS,
	AP_MENU_ID_TABLE_SPLIT_TABLE,
	AP_MENU_ID_TABLE_AUTOFIT,
	AP_MENU_ID_TABLE_TEXTTOTABLE_ALL,
	AP_MENU_ID_TABLE_TABLETOTEXT,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const AP_RibbonGroup s_ribbon_table_groups[] =
{
	{ "insert",		s_ribbon_table_insert },
	{ "delete",		s_ribbon_table_delete },
	{ "select",		s_ribbon_table_select },
	{ "format",		s_ribbon_table_format },
	{ nullptr,		nullptr }
};

/* --------------------------------------------------------------- Help --- */

static const uint16_t s_ribbon_help_items[] =
{
	AP_MENU_ID_HELP_CONTENTS,
	AP_MENU_ID_HELP_SEARCH,
	AP_MENU_ID_HELP_CHECKVER,
	AP_MENU_ID_HELP_REPORT_BUG,
	AP_MENU_ID_HELP_CREDITS,
	AP_MENU_ID_HELP_ABOUT,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const uint16_t s_ribbon_help_interface[] =
{
	AP_MENU_ID_HELP_UI_CLASSIC,
	AP_MENU_ID_HELP_UI_RIBBON,
	AP_MENU_ID__BOGUS1__ /* list terminator */
};

static const AP_RibbonGroup s_ribbon_help_groups[] =
{
	{ "help",		s_ribbon_help_items },
	{ "interface",	s_ribbon_help_interface },
	{ nullptr,		nullptr }
};

/* ------------------------------------------------- tab table (order) --- */

static const AP_RibbonTab s_ribbon_tabs[] =
{
	{ "file",		s_ribbon_file_groups,		false },
	{ "home",		s_ribbon_home_groups,		false },
	{ "insert",		s_ribbon_insert_groups,		false },
	{ "layout",		s_ribbon_layout_groups,		false },
	{ "review",		s_ribbon_review_groups,		false },
	{ "view",		s_ribbon_view_groups,		false },
	{ "table",		s_ribbon_table_groups,		true  },
	{ "help",		s_ribbon_help_groups,		false },
	{ nullptr,		nullptr,					false }
};
