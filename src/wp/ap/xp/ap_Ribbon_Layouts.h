/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
 * Copyright (C) 2026 AbiSource, Inc.
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
	AP_RIBBON_ITEM_DEAD		= 5,	/* insensitive placeholder button for
								 * Word groups with no engine support
								 * (citations, captions, index, TOA) */
	AP_RIBBON_ITEM_EQSYMBOLS = 6,	/* equation-tab math symbol palette */
	AP_RIBBON_ITEM_EQSTRUCT	= 7		/* equation-tab structure palette */
};

/* ids for AP_RIBBON_ITEM_SPIN rows - not menu/toolbar ids */
enum AP_RibbonSpinId : uint8_t
{
	AP_RIBBON_SPIN_INDENT_LEFT = 0,
	AP_RIBBON_SPIN_INDENT_RIGHT,
	AP_RIBBON_SPIN_BEFORE,
	AP_RIBBON_SPIN_AFTER,
	AP_RIBBON_SPIN_CELL_HEIGHT,
	AP_RIBBON_SPIN_CELL_WIDTH
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
	AP_RIBBON_FLAG_EVEN		= 1 << 6,	/* shares a homogeneous box with
									 * adjacent EVEN items so they all
									 * get the same width */
	AP_RIBBON_FLAG_WRAP		= 1 << 7	/* caption wraps to two lines so
									 * compact buttons stay narrow
									 * ("Insert\nAbove") */
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
#define AP_RIBBON_MENUPOP_S(x)	{ AP_RIBBON_ITEM_MENU,  (uint8_t)(AP_RIBBON_FLAG_MENUPOP), (uint16_t)(x) }
#define AP_RIBBON_MENUPOP_W(x)	{ AP_RIBBON_ITEM_MENU,  (uint8_t)(AP_RIBBON_FLAG_MENUPOP | AP_RIBBON_FLAG_WRAP | AP_RIBBON_FLAG_SLIM), (uint16_t)(x) }
#define AP_RIBBON_MENUPOP_GSE(x)	{ AP_RIBBON_ITEM_MENU,  (uint8_t)(AP_RIBBON_FLAG_ICONONLY | AP_RIBBON_FLAG_GLYPH | AP_RIBBON_FLAG_MENUPOP | AP_RIBBON_FLAG_SLIM | AP_RIBBON_FLAG_EVEN), (uint16_t)(x) }
#define AP_RIBBON_MENUPOP_TB(x)	{ AP_RIBBON_ITEM_TOOLBAR,  (uint8_t)(AP_RIBBON_FLAG_ICONONLY | AP_RIBBON_FLAG_MENUPOP), (uint16_t)(x) }
#define AP_RIBBON_MENUPOP_L(x)	{ AP_RIBBON_ITEM_MENU,  (uint8_t)(AP_RIBBON_FLAG_LARGE | AP_RIBBON_FLAG_MENUPOP), (uint16_t)(x) }
#define AP_RIBBON_MENUPOP_LS(x)	{ AP_RIBBON_ITEM_MENU,  (uint8_t)(AP_RIBBON_FLAG_LARGE | AP_RIBBON_FLAG_MENUPOP | AP_RIBBON_FLAG_SLIM), (uint16_t)(x) }
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
#define AP_RIBBON_MENU_LS(x)	{ AP_RIBBON_ITEM_MENU,    (uint8_t)(AP_RIBBON_FLAG_LARGE | AP_RIBBON_FLAG_SLIM), (uint16_t)(x) }
#define AP_RIBBON_MENU_I(x)	{ AP_RIBBON_ITEM_MENU,    AP_RIBBON_FLAG_ICONONLY, (uint16_t)(x) }
#define AP_RIBBON_MENU_W(x)	{ AP_RIBBON_ITEM_MENU,    (uint8_t)(AP_RIBBON_FLAG_WRAP | AP_RIBBON_FLAG_SLIM), (uint16_t)(x) }
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
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_INSERT_COVERPAGE),
	AP_RIBBON_MENU_LS(AP_MENU_ID_INSERT_BLANKPAGE),
	AP_RIBBON_MENU_LS(AP_MENU_ID_INSERT_PAGEBREAK),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_insert_tables[] =
{
	AP_RIBBON_MENU_LS(AP_MENU_ID_TABLE_INSERT_TABLE),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_insert_illustrations[] =
{
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_INSERT_PICTURES),
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_INSERT_SHAPES),
	AP_RIBBON_MENU_LS(AP_MENU_ID_INSERT_ICONS),
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_INSERT_3DMODELS),
	AP_RIBBON_MENU_LS(AP_MENU_ID_INSERT_SCREENSHOT),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_insert_media[] =
{
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_INSERT_MEDIA),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_insert_links[] =
{
	AP_RIBBON_MENU_LS(AP_MENU_ID_INSERT_HYPERLINK),
	AP_RIBBON_MENU_LS(AP_MENU_ID_INSERT_BOOKMARK),
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_REF_XREF),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_insert_comments[] =
{
	AP_RIBBON_MENU_LS(AP_MENU_ID_TOOLS_ANNOTATIONS_INSERT),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_insert_headerfooter[] =
{
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_INSERT_HEADER),
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_INSERT_FOOTER),
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_INSERT_PAGENO),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_insert_text[] =
{
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_INSERT_TEXTBOX),
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_INSERT_WORDART),
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_INSERT_DROPCAP),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_SIGNATURE),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_DATETIME),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_FIELD),
	AP_RIBBON_MENUPOP_S(AP_MENU_ID_INSERT_OBJECT),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_DIRECTIONMARKER_LRM),
	AP_RIBBON_MENU(AP_MENU_ID_INSERT_DIRECTIONMARKER_RLM),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_insert_symbols[] =
{
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_INSERT_EQUATION),
	AP_RIBBON_MENU_LS(AP_MENU_ID_INSERT_SYMBOL),
	AP_RIBBON_END
};

static const AP_RibbonGroup s_ribbon_insert_groups[] =
{
	{ "pages",			s_ribbon_insert_pages },
	{ "tables",			s_ribbon_insert_tables },
	{ "illustrations",	s_ribbon_insert_illustrations },
	{ "media",			s_ribbon_insert_media },
	{ "links",			s_ribbon_insert_links },
	{ "comments",		s_ribbon_insert_comments },
	{ "headerfooter",	s_ribbon_insert_headerfooter },
	{ "text",			s_ribbon_insert_text },
	{ "symbols",		s_ribbon_insert_symbols },
	{ nullptr,			nullptr }
};

/* --------------------------------------------------------- References --- */

static const AP_RibbonItem s_ribbon_references_toc[] =
{
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_REF_TOCPOP),
	AP_RIBBON_MENUPOP_S(AP_MENU_ID_REF_ADDTEXT),
	AP_RIBBON_MENU(AP_MENU_ID_REF_UPDATETOC),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_references_notes[] =
{
	AP_RIBBON_MENU_LS(AP_MENU_ID_INSERT_FOOTNOTE),
	AP_RIBBON_MENU_LS(AP_MENU_ID_INSERT_ENDNOTE),
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_REF_NEXTFN),
	AP_RIBBON_MENU_LS(AP_MENU_ID_REF_SHOWNOTES),
	AP_RIBBON_MENU_LS(AP_MENU_ID_FMT_FOOTNOTES),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_references_citations[] =
{
	AP_RIBBON_MENUPOP_S(AP_MENU_ID_REF_CITATION),
	AP_RIBBON_MENUPOP_S(AP_MENU_ID_REF_BIBLIOGRAPHY),
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_REF_SOURCES),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_references_captions[] =
{
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_REF_CAPTION),
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_REF_TOF),
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_REF_XREF),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_references_index[] =
{
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_REF_MARKENTRY),
	AP_RIBBON_MENU_LS(AP_MENU_ID_REF_INSERTINDEX),
	AP_RIBBON_MENU_LS(AP_MENU_ID_REF_UPDATEINDEX),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_references_toa[] =
{
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_REF_MARKCIT),
	AP_RIBBON_MENU_LS(AP_MENU_ID_REF_INSERTTOA),
	AP_RIBBON_MENU_LS(AP_MENU_ID_REF_UPDATETOA),
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
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_TOOLS_SPELLING_MENUPOP),
	AP_RIBBON_MENU_L(AP_MENU_ID_TOOLS_WORDCOUNT),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_review_language[] =
{
	AP_RIBBON_MENU_L(AP_MENU_ID_FMT_LANGUAGE),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_review_comments[] =
{
	AP_RIBBON_MENU_L(AP_MENU_ID_TOOLS_ANNOTATIONS_INSERT),
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_TOOLS_ANNOTATIONS_MENUPOP_DELETE),
	AP_RIBBON_MENU_L(AP_MENU_ID_TOOLS_ANNOTATIONS_RESOLVE),
	AP_RIBBON_MENU_L(AP_MENU_ID_TOOLS_ANNOTATIONS_PREV),
	AP_RIBBON_MENU_L(AP_MENU_ID_TOOLS_ANNOTATIONS_NEXT),
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_TOOLS_ANNOTATIONS_MENUPOP_SHOW),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_review_tracking[] =
{
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_TRACK),
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_DISPLAY),
	AP_RIBBON_MENU_L(AP_MENU_ID_TOOLS_REVISIONS_PANE),
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_ACCEPT),
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_REJECT),
	AP_RIBBON_MENU_L(AP_MENU_ID_TOOLS_REVISIONS_FIND_PREV),
	AP_RIBBON_MENU_L(AP_MENU_ID_TOOLS_REVISIONS_FIND_NEXT),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_review_compare[] =
{
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_COMPARE),
	AP_RIBBON_END
};

static const AP_RibbonGroup s_ribbon_review_groups[] =
{
	{ "proofing",	s_ribbon_review_proofing },
	{ "language",	s_ribbon_review_language },
	{ "comments",	s_ribbon_review_comments },
	{ "tracking",	s_ribbon_review_tracking },
	{ "compare",	s_ribbon_review_compare },
	{ nullptr,		nullptr }
};

/* --------------------------------------------------------------- View --- */

static const AP_RibbonItem s_ribbon_view_views[] =
{
	AP_RIBBON_MENU_L(AP_MENU_ID_VIEW_PRINT),
	AP_RIBBON_MENU_L(AP_MENU_ID_VIEW_WEB),
	AP_RIBBON_MENU_L(AP_MENU_ID_VIEW_NORMAL),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_view_immersive[] =
{
	AP_RIBBON_MENU_L(AP_MENU_ID_VIEW_FULLSCREEN),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_view_show[] =
{
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_RULER),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_GRIDLINES),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_NAVPANE),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_STATUSBAR),
	AP_RIBBON_MENU(AP_MENU_ID_VIEW_SHOWPARA),
	AP_RIBBON_MENU(AP_MENU_ID_LAYOUT_SELPANE),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_view_zoom[] =
{
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_VIEW_ZOOM),
	AP_RIBBON_MENU_L(AP_MENU_ID_VIEW_ZOOM_100),
	AP_RIBBON_MENU_L(AP_MENU_ID_VIEW_ZOOM_WHOLE),
	AP_RIBBON_MENU_L(AP_MENU_ID_VIEW_ZOOM_WIDTH),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_view_window[] =
{
	AP_RIBBON_MENU_L(AP_MENU_ID_WINDOW_NEW),
	AP_RIBBON_MENU_L(AP_MENU_ID_WINDOW_ARRANGE),
	AP_RIBBON_MENU_L(AP_MENU_ID_VIEW_SPLIT),
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_WINDOW_MENUPOP_SWITCH),
	AP_RIBBON_END
};

static const AP_RibbonGroup s_ribbon_view_groups[] =
{
	{ "views",		s_ribbon_view_views },
	{ "immersive",	s_ribbon_view_immersive },
	{ "show",		s_ribbon_view_show },
	{ "zoom",		s_ribbon_view_zoom },
	{ "window",		s_ribbon_view_window },
	{ nullptr,	nullptr }
};

/* ------------------------------------ Table Layout (contextual tab) --- */

static const AP_RibbonItem s_ribbon_table_table[] =
{
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_TABLE_SELECT),
	AP_RIBBON_MENU_L(AP_MENU_ID_TABLE_VIEW_GRIDLINES),
	AP_RIBBON_MENU_L(AP_MENU_ID_TABLE_FORMAT),
	AP_RIBBON_MENU_L(AP_MENU_ID_TABLE_DRAW),
	AP_RIBBON_MENU_L(AP_MENU_ID_TABLE_ERASE),
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_TABLE_DELETE),
	AP_RIBBON_END
};

/* row-major:  Rows Above | Columns Left  | Merge Cells
 *             Rows Below | Columns Right | Split Cells
 *             Split Table                               */
static const AP_RibbonItem s_ribbon_table_rowscols[] =
{
	AP_RIBBON_MENU_W(AP_MENU_ID_TABLE_INSERT_ROWS_BEFORE),
	AP_RIBBON_MENU_W(AP_MENU_ID_TABLE_INSERT_COLUMNS_BEFORE),
	AP_RIBBON_MENUPOP_W(AP_MENU_ID_TABLE_MERGE_CELLS),
	AP_RIBBON_ROWEND,
	AP_RIBBON_MENU_W(AP_MENU_ID_TABLE_INSERT_ROWS_AFTER),
	AP_RIBBON_MENU_W(AP_MENU_ID_TABLE_INSERT_COLUMNS_AFTER),
	AP_RIBBON_MENUPOP_W(AP_MENU_ID_TABLE_SPLIT_CELLS),
	AP_RIBBON_ROWEND,
	AP_RIBBON_MENU_W(AP_MENU_ID_TABLE_SPLIT_TABLE),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_table_align[] =
{
	AP_RIBBON_MENU_GSE(AP_MENU_ID_TABLE_ALIGN_TOPLEFT),
	AP_RIBBON_MENU_GSE(AP_MENU_ID_TABLE_ALIGN_CENTERLEFT),
	AP_RIBBON_MENU_GSE(AP_MENU_ID_TABLE_ALIGN_BOTLEFT),
	AP_RIBBON_MENU_GSE(AP_MENU_ID_TABLE_ALIGN_TOPCENTER),
	AP_RIBBON_MENU_GSE(AP_MENU_ID_TABLE_ALIGN_CENTER),
	AP_RIBBON_MENU_GSE(AP_MENU_ID_TABLE_ALIGN_BOTCENTER),
	AP_RIBBON_MENU_GSE(AP_MENU_ID_TABLE_ALIGN_TOPRIGHT),
	AP_RIBBON_MENU_GSE(AP_MENU_ID_TABLE_ALIGN_CENTERRIGHT),
	AP_RIBBON_MENU_GSE(AP_MENU_ID_TABLE_ALIGN_BOTRIGHT),
	AP_RIBBON_MENUPOP_W(AP_MENU_ID_TABLE_TEXT_DIRECTION),
	AP_RIBBON_MENUPOP_W(AP_MENU_ID_TABLE_CELL_MARGINS),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_table_data[] =
{
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_TABLE_SORT),
	AP_RIBBON_MENU_L(AP_MENU_ID_TABLE_HEADING_ROWS_REPEAT),
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_TABLE_TABLETOTEXT),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_table_cellsize[] =
{
	AP_RIBBON_MENUPOP_L(AP_MENU_ID_TABLE_AUTOFIT),
	AP_RIBBON_SPIN(AP_RIBBON_SPIN_CELL_HEIGHT),
	AP_RIBBON_SPIN(AP_RIBBON_SPIN_CELL_WIDTH),
	AP_RIBBON_MENU_L(AP_MENU_ID_TABLE_DISTRIBUTE_ROWS),
	AP_RIBBON_MENU_L(AP_MENU_ID_TABLE_DISTRIBUTE_COLS),
	AP_RIBBON_END
};

static const AP_RibbonGroup s_ribbon_table_groups[] =
{
	{ "table",		s_ribbon_table_table },
	{ "rowscols",	s_ribbon_table_rowscols },
	{ "cellsize",	s_ribbon_table_cellsize },
	{ "align",		s_ribbon_table_align },
	{ "data",		s_ribbon_table_data },
	{ nullptr,		nullptr }
};

/* -------------------------------------------- Equation (contextual) --- */

static const AP_RibbonItem s_ribbon_equation_eq[] =
{
	AP_RIBBON_MENUPOP_LS(AP_MENU_ID_INSERT_EQUATION),
	AP_RIBBON_MENU_LS(AP_MENU_ID_EDIT_LATEXEQUATION),
	AP_RIBBON_MENU_LS(AP_MENU_ID_EQUATION_DISPLAY),
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_equation_symbols[] =
{
	{ AP_RIBBON_ITEM_EQSYMBOLS, AP_RIBBON_FLAG_NONE, 0 },
	AP_RIBBON_END
};

static const AP_RibbonItem s_ribbon_equation_struct[] =
{
	{ AP_RIBBON_ITEM_EQSTRUCT, AP_RIBBON_FLAG_NONE, 0 },
	AP_RIBBON_END
};

static const AP_RibbonGroup s_ribbon_equation_groups[] =
{
	{ "equation",		s_ribbon_equation_eq },
	{ "symbols",		s_ribbon_equation_symbols },
	{ "structures",		s_ribbon_equation_struct },
	{ nullptr,			nullptr }
};

/* --------------------------------------------------------------- Help --- */

static const AP_RibbonItem s_ribbon_help_items[] =
{
	AP_RIBBON_MENU_L(AP_MENU_ID_HELP_CONTENTS),
	AP_RIBBON_MENU_L(AP_MENU_ID_HELP_SEARCH),
	AP_RIBBON_MENU_L(AP_MENU_ID_HELP_CHECKVER),
	AP_RIBBON_MENU_L(AP_MENU_ID_HELP_REPORT_BUG),
	AP_RIBBON_MENU_L(AP_MENU_ID_HELP_ABOUT),
	AP_RIBBON_END
};

static const AP_RibbonGroup s_ribbon_help_groups[] =
{
	{ "help",		s_ribbon_help_items },
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
	{ "equation",	s_ribbon_equation_groups,	true  },
	{ "help",		s_ribbon_help_groups,		false },
	{ nullptr,		nullptr,					false }
};
