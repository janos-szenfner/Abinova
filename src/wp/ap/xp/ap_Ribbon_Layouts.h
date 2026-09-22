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
#include "ap_Toolbar_Id.h"

/*
 * Ribbon UI layout description.
 *
 * Modeled on the LibreOffice Writer NotebookBar
 * (sw/uiconfig/swriter/ui/notebookbar.ui): a GtkNotebook whose tabs
 * hold grouped buttons/combos that drive the same edit methods the
 * menubar and classic toolbars use.
 *
 * Items may reference either a menu id (AP_MENU_ID_*) or a toolbar id
 * (AP_TOOLBAR_ID_*).  Menu items resolve through the "menu.*" GAction
 * group; toolbar items resolve through the toolbar action set, which
 * additionally supplies control types (combo box, color picker) and
 * per-cursor state (toggled flags, current font/size/style/zoom).
 *
 * Tabs follow the LibreOffice Writer convention:
 *   File       - document, print/export
 *   Home       - clipboard, font, paragraph, lists, styles, editing
 *   Insert     - pages, tables, illustrations, links, text, symbols,
 *                fields
 *   References - table of contents, footnotes, endnotes
 *   Layout     - page setup, columns, background
 *   Review     - spelling, language, revisions, annotations
 *   View       - view modes, show, zoom, window
 *   Table      - contextual tab, shown only while the caret is in a table
 *   Help       - help, interface switcher
 */

enum AP_RibbonItemKind : uint8_t
{
	AP_RIBBON_ITEM_MENU		= 0,
	AP_RIBBON_ITEM_TOOLBAR	= 1,
	AP_RIBBON_ITEM_STYLEGAL	= 2,	/* Word-style live style preview strip */
	AP_RIBBON_ITEM_ROWEND	= 3,	/* row break - switches the group to
								 * row-major packing (LibreOffice-style
								 * two-row groups) */
	AP_RIBBON_ITEM_SPIN		= 4,	/* labelled spin field (indent/spacing) */
	AP_RIBBON_ITEM_DEAD		= 5		/* insensitive placeholder button for
								 * Word groups with no engine support
								 * (citations, captions, index, TOA) */
};

/* ids for AP_RIBBON_ITEM_SPIN rows - not menu/toolbar ids */
enum AP_RibbonSpinId : uint8_t
{
	AP_RIBBON_SPIN_INDENT_LEFT = 0,
	AP_RIBBON_SPIN_INDENT_RIGHT,
	AP_RIBBON_SPIN_BEFORE,
	AP_RIBBON_SPIN_AFTER
};

/* ids for AP_RIBBON_ITEM_DEAD rows - not menu/toolbar ids */
enum AP_RibbonDeadId : uint8_t
{
	AP_RIBBON_DEAD_CITATION = 0,
	AP_RIBBON_DEAD_SOURCES,
	AP_RIBBON_DEAD_BIBLIOGRAPHY,
	AP_RIBBON_DEAD_CAPTION,
	AP_RIBBON_DEAD_FIGURES,
	AP_RIBBON_DEAD_XREF,
	AP_RIBBON_DEAD_INDEX,
	AP_RIBBON_DEAD_MARKENTRY,
	AP_RIBBON_DEAD_UPDATEINDEX,
	AP_RIBBON_DEAD_TOA,
	AP_RIBBON_DEAD_MARKCITATION,
	AP_RIBBON_DEAD_UPDATETOA
};

enum AP_RibbonItemFlags : uint8_t
{
	AP_RIBBON_FLAG_NONE		= 0,
	AP_RIBBON_FLAG_LARGE	= 1 << 0,	/* icon above label, spans the group height */
	AP_RIBBON_FLAG_ICONONLY	= 1 << 1,	/* compact glyph-only button */
	AP_RIBBON_FLAG_SPLIT	= 1 << 2,	/* trailing drop-arrow opens a popover */
	AP_RIBBON_FLAG_GLYPH	= 1 << 3,	/* text glyph (B, I, U, x2) instead of
									 * a theme icon */
	AP_RIBBON_FLAG_MENUPOP	= 1 << 4,	/* single menu-button: clicking it
									 * opens the dropdown popover (no
									 * separate arrow, no action on the
									 * button itself) */
	AP_RIBBON_FLAG_SLIM		= 1 << 5,	/* reduced button padding */
	AP_RIBBON_FLAG_EVEN		= 1 << 6	/* shares a homogeneous box with
									 * adjacent EVEN items so they all
									 * get the same width */
};

#define AP_RIBBON_ROWEND	{ AP_RIBBON_ITEM_ROWEND,  AP_RIBBON_FLAG_NONE,     0 }
#define AP_RIBBON_MENU_G(x)	{ AP_RIBBON_ITEM_MENU,    (uint8_t)(AP_RIBBON_FLAG_ICONONLY | AP_RIBBON_FLAG_GLYPH), (uint16_t)(x) }
#define AP_RIBBON_MENU_GS(x)	{ AP_RIBBON_ITEM_MENU,    (uint8_t)(AP_RIBBON_FLAG_ICONONLY | AP_RIBBON_FLAG_GLYPH | AP_RIBBON_FLAG_SLIM), (uint16_t)(x) }
#define AP_RIBBON_MENU_GSE(x)	{ AP_RIBBON_ITEM_MENU,    (uint8_t)(AP_RIBBON_FLAG_ICONONLY | AP_RIBBON_FLAG_GLYPH | AP_RIBBON_FLAG_SLIM | AP_RIBBON_FLAG_EVEN), (uint16_t)(x) }

#define AP_RIBBON_SPLIT_MENU(x)	{ AP_RIBBON_ITEM_MENU,    (uint8_t)(AP_RIBBON_FLAG_LARGE | AP_RIBBON_FLAG_SPLIT),    (uint16_t)(x) }
#define AP_RIBBON_SPLIT_TB_I(x)	{ AP_RIBBON_ITEM_TOOLBAR, (uint8_t)(AP_RIBBON_FLAG_ICONONLY | AP_RIBBON_FLAG_SPLIT), (uint16_t)(x) }
#define AP_RIBBON_SPLIT_MENU_G(x)	{ AP_RIBBON_ITEM_MENU,  (uint8_t)(AP_RIBBON_FLAG_ICONONLY | AP_RIBBON_FLAG_GLYPH | AP_RIBBON_FLAG_SPLIT), (uint16_t)(x) }
#define AP_RIBBON_MENUPOP_G(x)	{ AP_RIBBON_ITEM_MENU,  (uint8_t)(AP_RIBBON_FLAG_ICONONLY | AP_RIBBON_FLAG_GLYPH | AP_RIBBON_FLAG_MENUPOP), (uint16_t)(x) }
#define AP_RIBBON_MENUPOP_I(x)	{ AP_RIBBON_ITEM_MENU,  (uint8_t)(AP_RIBBON_FLAG_ICONONLY | AP_RIBBON_FLAG_MENUPOP), (uint16_t)(x) }
#define AP_RIBBON_MENUPOP_GSE(x)	{ AP_RIBBON_ITEM_MENU,  (uint8_t)(AP_RIBBON_FLAG_ICONONLY | AP_RIBBON_FLAG_GLYPH | AP_RIBBON_FLAG_MENUPOP | AP_RIBBON_FLAG_SLIM | AP_RIBBON_FLAG_EVEN), (uint16_t)(x) }
#define AP_RIBBON_MENUPOP_TB(x)	{ AP_RIBBON_ITEM_TOOLBAR,  (uint8_t)(AP_RIBBON_FLAG_ICONONLY | AP_RIBBON_FLAG_MENUPOP), (uint16_t)(x) }
#define AP_RIBBON_MENUPOP_L(x)	{ AP_RIBBON_ITEM_MENU,  (uint8_t)(AP_RIBBON_FLAG_LARGE | AP_RIBBON_FLAG_MENUPOP), (uint16_t)(x) }
#define AP_RIBBON_SPIN(x)		{ AP_RIBBON_ITEM_SPIN,  AP_RIBBON_FLAG_NONE, (uint16_t)(x) }
#define AP_RIBBON_DEAD(x)		{ AP_RIBBON_ITEM_DEAD,  AP_RIBBON_FLAG_NONE, (uint16_t)(x) }

struct AP_RibbonItem
{
	uint8_t		kind;	/* AP_RibbonItemKind */
	uint8_t		flags;	/* AP_RibbonItemFlags */
	uint16_t	id;		/* AP_MENU_ID_* or AP_TOOLBAR_ID_* */
};

#define AP_RIBBON_MENU(x)	{ AP_RIBBON_ITEM_MENU,    AP_RIBBON_FLAG_NONE,     (uint16_t)(x) }
#define AP_RIBBON_MENU_L(x)	{ AP_RIBBON_ITEM_MENU,    AP_RIBBON_FLAG_LARGE,    (uint16_t)(x) }
#define AP_RIBBON_MENU_I(x)	{ AP_RIBBON_ITEM_MENU,    AP_RIBBON_FLAG_ICONONLY, (uint16_t)(x) }
#define AP_RIBBON_TB(x)		{ AP_RIBBON_ITEM_TOOLBAR, AP_RIBBON_FLAG_NONE,     (uint16_t)(x) }
#define AP_RIBBON_TB_I(x)	{ AP_RIBBON_ITEM_TOOLBAR, AP_RIBBON_FLAG_ICONONLY, (uint16_t)(x) }
#define AP_RIBBON_GALLERY	{ AP_RIBBON_ITEM_STYLEGAL,AP_RIBBON_FLAG_NONE,     0 }
#define AP_RIBBON_END		{ AP_RIBBON_ITEM_MENU,    AP_RIBBON_FLAG_NONE,     (uint16_t)AP_MENU_ID__BOGUS1__ }

struct AP_RibbonGroup
{
	const char *			szGroupKey;	/* untranslated group key -> label set */
	const AP_RibbonItem *	items;		/* AP_RIBBON_END-terminated list */
};

struct AP_RibbonTab
{
	const char *				szTabKey;	/* untranslated tab key -> label set */
	const AP_RibbonGroup *		groups;		/* nullptr-terminated group list */
	bool						bContextual;/* show only when context applies */
};

/* --------------------------------------------------------------- File --- */

static const AP_RibbonItem s_ribbon_file_document[] =
{
	AP_RIBBON_MENU_L(AP_MENU_ID_FILE_NEW),
	AP_RIBBON_MENU_L(AP_MENU_ID_FILE_NEW_USING_TEMPLATE),
	AP_RIBBON_MENU_L(AP_MENU_ID_FILE_OPEN),
	AP_RIBBON_MENU_L(AP_MENU_ID_FILE_SAVE),
	AP_RIBBON_MENU_L(AP_MENU_ID_FILE_SAVEAS),
	AP_RIBBON_MENU_L(AP_MENU_ID_FILE_REVERT),
	AP_RIBBON_MENU_L(AP_MENU_ID_FILE_PROPERTIES),
	AP_RIBBON_MENU_L(AP_MENU_ID_FILE_CLOSE),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_file_print[] =
{
	AP_RIBBON_MENU_L(AP_MENU_ID_FILE_PAGESETUP),
	AP_RIBBON_MENU_L(AP_MENU_ID_FILE_PRINT_PREVIEW),
	AP_RIBBON_MENU_L(AP_MENU_ID_FILE_PRINT),
	AP_RIBBON_END
};

static const AP_RibbonGroup s_ribbon_file_groups[] =
{
	{ "document",	s_ribbon_file_document },
	{ "print",		s_ribbon_file_print },
	{ nullptr,		nullptr }
};

/* --------------------------------------------------------------- Home --- */

static const AP_RibbonItem s_ribbon_home_clipboard[] =
{
	AP_RIBBON_SPLIT_MENU(AP_MENU_ID_EDIT_PASTE),
	AP_RIBBON_MENU_I(AP_MENU_ID_EDIT_CUT),
	AP_RIBBON_MENU_I(AP_MENU_ID_EDIT_COPY),
	AP_RIBBON_TB_I(AP_TOOLBAR_ID_FMTPAINTER),
	AP_RIBBON_END
};

/* LibreOffice Writer NotebookBar font group: row 1 holds the font
 * name combo, size combo, grow/shrink and clear-formatting; row 2 is
 * the inline-format strip (B I U S x2 x2 highlight font-color) plus
 * the Font dialog launcher. */
static const AP_RibbonItem s_ribbon_home_font[] =
{
	AP_RIBBON_TB(AP_TOOLBAR_ID_FMT_FONT),
	AP_RIBBON_TB(AP_TOOLBAR_ID_FMT_SIZE),
	AP_RIBBON_MENU_GSE(AP_MENU_ID_FMT_GROWFONT),
	AP_RIBBON_MENU_GSE(AP_MENU_ID_FMT_SHRINKFONT),
	AP_RIBBON_MENUPOP_GSE(AP_MENU_ID_FMT_TOGGLECASE),
	AP_RIBBON_ROWEND,
	AP_RIBBON_MENU_G(AP_MENU_ID_FMT_BOLD),
	AP_RIBBON_MENU_G(AP_MENU_ID_FMT_ITALIC),
	AP_RIBBON_MENU_G(AP_MENU_ID_FMT_UNDERLINE),
	AP_RIBBON_MENU_G(AP_MENU_ID_FMT_STRIKE),
	AP_RIBBON_MENU_G(AP_MENU_ID_FMT_SUPERSCRIPT),
	AP_RIBBON_MENU_G(AP_MENU_ID_FMT_SUBSCRIPT),
	AP_RIBBON_TB_I(AP_TOOLBAR_ID_COLOR_BACK),
	AP_RIBBON_TB_I(AP_TOOLBAR_ID_COLOR_FORE),
	AP_RIBBON_MENU_I(AP_MENU_ID_FMT_CLEARFMT),
	AP_RIBBON_MENU_I(AP_MENU_ID_FMT_FONT),
	AP_RIBBON_END
};

/* LibreOffice NotebookBar-style paragraph group:
 *   row 1: list dropdowns, indent, sort, pilcrow
 *   row 2: alignment, line/paragraph spacing dropdowns, borders */
static const AP_RibbonItem s_ribbon_home_paragraph[] =
{
	AP_RIBBON_SPLIT_TB_I(AP_TOOLBAR_ID_LISTS_BULLETS),
	AP_RIBBON_SPLIT_TB_I(AP_TOOLBAR_ID_LISTS_NUMBERS),
	AP_RIBBON_SPLIT_TB_I(AP_TOOLBAR_ID_LISTS_DASHED),
	AP_RIBBON_TB_I(AP_TOOLBAR_ID_UNINDENT),
	AP_RIBBON_TB_I(AP_TOOLBAR_ID_INDENT),
	AP_RIBBON_MENUPOP_TB(AP_TOOLBAR_ID_SORT_PARA),
	AP_RIBBON_TB_I(AP_TOOLBAR_ID_VIEW_SHOWPARA),
	AP_RIBBON_ROWEND,
	AP_RIBBON_TB_I(AP_TOOLBAR_ID_ALIGN_LEFT),
	AP_RIBBON_TB_I(AP_TOOLBAR_ID_ALIGN_CENTER),
	AP_RIBBON_TB_I(AP_TOOLBAR_ID_ALIGN_RIGHT),
	AP_RIBBON_TB_I(AP_TOOLBAR_ID_ALIGN_JUSTIFY),
	AP_RIBBON_MENUPOP_TB(AP_TOOLBAR_ID_SINGLE_SPACE),
	AP_RIBBON_MENUPOP_TB(AP_TOOLBAR_ID_PARA_0BEFORE),
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_FMT_BORDERS),
	AP_RIBBON_MENU(AP_MENU_ID_FMT_PARAGRAPH),
	AP_RIBBON_ROWEND,
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_home_styles[] =
{
	AP_RIBBON_GALLERY,
	/* kept in the layout so its toolbar-state updates still drive the
	 * gallery highlight + Styles pane refresh; the widget itself is
	 * hidden (LibreOffice-style group: tiles + Styles Pane only) */
	AP_RIBBON_TB(AP_TOOLBAR_ID_FMT_STYLE),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_home_editing[] =
{
	AP_RIBBON_MENU_I(AP_MENU_ID_EDIT_FIND),
	AP_RIBBON_MENU_I(AP_MENU_ID_EDIT_REPLACE),
	AP_RIBBON_MENU_I(AP_MENU_ID_EDIT_SELECTALL),
	AP_RIBBON_MENU_I(AP_MENU_ID_EDIT_GOTO),
	AP_RIBBON_MENU_I(AP_MENU_ID_EDIT_UNDO),
	AP_RIBBON_MENU_I(AP_MENU_ID_EDIT_REDO),
	AP_RIBBON_END
};

static const AP_RibbonGroup s_ribbon_home_groups[] =
{
	{ "clipboard",	s_ribbon_home_clipboard },
	{ "font",		s_ribbon_home_font },
	{ "paragraph",	s_ribbon_home_paragraph },
	{ "styles",		s_ribbon_home_styles },
	{ "editing",	s_ribbon_home_editing },
	{ nullptr,		nullptr }
};

/* ------------------------------------------------------------- Insert --- */

static const AP_RibbonItem s_ribbon_insert_pages[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_BREAK),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_PAGENO),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_HEADER),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_FOOTER),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_insert_tables[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_INSERT_TABLE),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_insert_illustrations[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_GRAPHIC),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_CLIPART),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_insert_links[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_HYPERLINK),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_BOOKMARK),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_XMLID),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_MAILMERGE),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_FILE),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_insert_text[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_TEXTBOX),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_DIRECTIONMARKER_LRM),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_DIRECTIONMARKER_RLM),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_insert_symbols[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_SYMBOL),
	AP_RIBBON_MENU(AP_MENU_ID_EDIT_LATEXEQUATION),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_insert_fields[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_DATETIME),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_FIELD),
	AP_RIBBON_END
};

static const AP_RibbonGroup s_ribbon_insert_groups[] =
{
	{ "pages",			s_ribbon_insert_pages },
	{ "tables",			s_ribbon_insert_tables },
	{ "illustrations",	s_ribbon_insert_illustrations },
	{ "links",			s_ribbon_insert_links },
	{ "text",			s_ribbon_insert_text },
	{ "symbols",		s_ribbon_insert_symbols },
	{ "fields",			s_ribbon_insert_fields },
	{ nullptr,			nullptr }
};

/* --------------------------------------------------------- References --- */

static const AP_RibbonItem s_ribbon_references_toc[] =
{
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_REF_TOCPOP),
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_REF_ADDTEXT),
	AP_RIBBON_MENU(AP_MENU_ID_REF_UPDATETOC),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_references_notes[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_FOOTNOTE),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_ENDNOTE),
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_REF_NEXTFN),
	AP_RIBBON_MENU(AP_MENU_ID_REF_SHOWNOTES),
	AP_RIBBON_MENU(AP_MENU_ID_FMT_FOOTNOTES),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_references_citations[] =
{
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_REF_CITATION),
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_REF_SOURCES),
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_REF_BIBLIOGRAPHY),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_references_captions[] =
{
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_REF_CAPTION),
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_REF_TOF),
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_REF_XREF),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_references_index[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_REF_INSERTINDEX),
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_REF_MARKENTRY),
	AP_RIBBON_MENU(AP_MENU_ID_REF_UPDATEINDEX),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_references_toa[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_REF_INSERTTOA),
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_REF_MARKCIT),
	AP_RIBBON_MENU(AP_MENU_ID_REF_UPDATETOA),
	AP_RIBBON_END
};

static const AP_RibbonGroup s_ribbon_references_groups[] =
{
	{ "toc",	s_ribbon_references_toc },
	{ "notes",	s_ribbon_references_notes },
	{ "citations",	s_ribbon_references_citations },
	{ "captions",	s_ribbon_references_captions },
	{ "index",	s_ribbon_references_index },
	{ "authorities",	s_ribbon_references_toa },
	{ nullptr,	nullptr }
};

/* ------------------------------------------------------------- Layout --- */

static const AP_RibbonItem s_ribbon_layout_page[] =
{
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_LAYOUT_MARGINS),
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_LAYOUT_ORIENTATION),
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_LAYOUT_SIZE),
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_FMT_COLUMNS),
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_LAYOUT_BREAKS),
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_LAYOUT_LINENUMBERS),
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_LAYOUT_HYPHENATION),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_layout_indent[] =
{
	AP_RIBBON_SPIN(AP_RIBBON_SPIN_INDENT_LEFT),
	AP_RIBBON_SPIN(AP_RIBBON_SPIN_INDENT_RIGHT),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_layout_spacing[] =
{
	AP_RIBBON_SPIN(AP_RIBBON_SPIN_BEFORE),
	AP_RIBBON_SPIN(AP_RIBBON_SPIN_AFTER),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_layout_arrange[] =
{
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_LAYOUT_POSITION),
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_LAYOUT_WRAP),
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_LAYOUT_BRINGFORWARD),
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_LAYOUT_SENDBACKWARD),
	AP_RIBBON_MENU_I(AP_MENU_ID_LAYOUT_SELPANE),
	AP_RIBBON_ROWEND,
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_LAYOUT_ALIGNOBJECTS),
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_LAYOUT_GROUPOBJECTS),
	AP_RIBBON_MENUPOP_I(AP_MENU_ID_LAYOUT_ROTATE),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_layout_background[] =
{
	AP_RIBBON_MENU_L(AP_MENU_ID_FMT_BACKGROUND_PAGE_COLOR),
	AP_RIBBON_MENU_L(AP_MENU_ID_FMT_BACKGROUND_PAGE_IMAGE),
	AP_RIBBON_END
};

static const AP_RibbonGroup s_ribbon_layout_groups[] =
{
	{ "page",		s_ribbon_layout_page },
	{ "indent",		s_ribbon_layout_indent },
	{ "spacing",	s_ribbon_layout_spacing },
	{ "arrange",	s_ribbon_layout_arrange },
	{ "background",	s_ribbon_layout_background },
	{ nullptr,		nullptr }
};

/* ------------------------------------------------------------- Review --- */

static const AP_RibbonItem s_ribbon_review_proofing[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_SPELL),
	AP_RIBBON_MENU(AP_MENU_ID_FMT_LANGUAGE),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_WORDCOUNT),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_review_revisions[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_REVISIONS_MARK),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_REVISIONS_NEW_REVISION),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_REVISIONS_SHOW),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_REVISIONS_SHOW_AFTER),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_REVISIONS_SHOW_AFTERPREV),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_REVISIONS_SHOW_BEFORE),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_REVISIONS_SET_VIEW_LEVEL),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_REVISIONS_FIND_NEXT),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_REVISIONS_FIND_PREV),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_REVISIONS_ACCEPT_REVISION),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_REVISIONS_REJECT_REVISION),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_REVISIONS_PURGE),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_REVISIONS_COMPARE_DOCUMENTS),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_REVISIONS_AUTO),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_HISTORY_SHOW),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_review_annotations[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_ANNOTATIONS_INSERT),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_ANNOTATIONS_INSERT_FROMSEL),
	AP_RIBBON_MENU(AP_MENU_ID_TOOLS_ANNOTATIONS_TOGGLE_DISPLAY),
	AP_RIBBON_END
};

static const AP_RibbonGroup s_ribbon_review_groups[] =
{
	{ "proofing",		s_ribbon_review_proofing },
	{ "revisions",		s_ribbon_review_revisions },
	{ "annotations",	s_ribbon_review_annotations },
	{ nullptr,			nullptr }
};

/* --------------------------------------------------------------- View --- */

static const AP_RibbonItem s_ribbon_view_views[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_NORMAL),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_WEB),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_PRINT),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_view_show[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_RULER),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_STATUSBAR),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_TB_1),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_TB_2),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_TB_3),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_TB_4),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_SHOWPARA),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_LOCKSTYLES),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_view_zoom[] =
{
	AP_RIBBON_TB(AP_TOOLBAR_ID_ZOOM),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_ZOOM_WIDTH),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_ZOOM_WHOLE),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_ZOOM_200),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_ZOOM_100),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_ZOOM_75),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_ZOOM_50),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_ZOOM),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_view_window[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_FULLSCREEN),
	AP_RIBBON_MENU(AP_MENU_ID_WEB_WEBPREVIEW),
	AP_RIBBON_END
};

static const AP_RibbonGroup s_ribbon_view_groups[] =
{
	{ "views",	s_ribbon_view_views },
	{ "show",	s_ribbon_view_show },
	{ "zoom",	s_ribbon_view_zoom },
	{ "window",	s_ribbon_view_window },
	{ nullptr,	nullptr }
};

/* -------------------------------------------- Table (contextual tab) --- */

static const AP_RibbonItem s_ribbon_table_insert[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_INSERT_TABLE),
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_INSERT_COLUMNS_BEFORE),
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_INSERT_COLUMNS_AFTER),
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_INSERT_ROWS_BEFORE),
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_INSERT_ROWS_AFTER),
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_INSERT_SUMCOLS),
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_INSERT_SUMROWS),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_table_delete[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_DELETE_TABLE),
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_DELETE_COLUMNS),
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_DELETE_ROWS),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_table_select[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_SELECT_TABLE),
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_SELECT_COLUMN),
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_SELECT_ROW),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_table_format[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_FMT_TABLE),
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_MERGE_CELLS),
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_SPLIT_CELLS),
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_AUTOFIT),
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_TEXTTOTABLE_ALL),
	AP_RIBBON_MENU(AP_MENU_ID_TABLE_TABLETOTEXT),
	AP_RIBBON_END
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

static const AP_RibbonItem s_ribbon_help_items[] =
{
	AP_RIBBON_MENU_L(AP_MENU_ID_HELP_CONTENTS),
	AP_RIBBON_MENU_L(AP_MENU_ID_HELP_SEARCH),
	AP_RIBBON_MENU_L(AP_MENU_ID_HELP_CHECKVER),
	AP_RIBBON_MENU_L(AP_MENU_ID_HELP_REPORT_BUG),
	AP_RIBBON_MENU_L(AP_MENU_ID_HELP_CREDITS),
	AP_RIBBON_MENU_L(AP_MENU_ID_HELP_ABOUT),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_help_interface[] =
{
	AP_RIBBON_MENU_L(AP_MENU_ID_HELP_UI_CLASSIC),
	AP_RIBBON_MENU_L(AP_MENU_ID_HELP_UI_RIBBON),
	AP_RIBBON_END
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
	{ "references",	s_ribbon_references_groups,	false },
	{ "layout",		s_ribbon_layout_groups,		false },
	{ "review",		s_ribbon_review_groups,		false },
	{ "view",		s_ribbon_view_groups,		false },
	{ "table",		s_ribbon_table_groups,		true  },
	{ "help",		s_ribbon_help_groups,		false },
	{ nullptr,		nullptr,					false }
};
