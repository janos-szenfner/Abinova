/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* Abinova
 * Copyright (C) 1998 AbiSource, Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ap_Features.h"
#ifdef APF_MENU_LAYOUTS_MAIN_MENU
#  include APF_MENU_LAYOUTS_MAIN_MENU
#else

/*****************************************************************
******************************************************************
** IT IS IMPORTANT THAT THIS FILE ALLOW ITSELF TO BE INCLUDED
** MORE THAN ONE TIME.
******************************************************************
*****************************************************************/

BeginLayout(Main,0)

	BeginSubMenu(AP_MENU_ID_FILE)
		MenuItem(AP_MENU_ID_FILE_NEW)
		MenuItem(AP_MENU_ID_FILE_NEW_USING_TEMPLATE)
		MenuItem(AP_MENU_ID_FILE_OPEN)

#if !XAP_SIMPLE_MENU
		MenuItem(AP_MENU_ID_FILE_IMPORTSTYLES)
#endif
		Separator()
		MenuItem(AP_MENU_ID_FILE_SAVE)
		MenuItem(AP_MENU_ID_FILE_SAVEAS)
//		MenuItem(AP_MENU_ID_FILE_SAVE_TEMPLATE)
//		MenuItem(AP_MENU_ID_FILE_IMPORT)
		MenuItem(AP_MENU_ID_FILE_EXPORT)
		MenuItem(AP_MENU_ID_FILE_REVERT)
#ifdef ENABLE_PRINT
		Separator()
		MenuItem(AP_MENU_ID_FILE_PAGESETUP)
		MenuItem(AP_MENU_ID_FILE_PRINT_PREVIEW)
		MenuItem(AP_MENU_ID_FILE_PRINT)
#endif
#if !XAP_SIMPLE_MENU
		Separator()
		MenuItem(AP_MENU_ID_FILE_PROPERTIES)
		Separator()
#endif
#if  !XAP_SIMPLE_MENU
		MenuItem(AP_MENU_ID_FILE_CLOSE)
		MenuItem(AP_MENU_ID_FILE_EXIT)
#endif
	EndSubMenu()

	BeginSubMenu(AP_MENU_ID_EDIT)
		MenuItem(AP_MENU_ID_EDIT_UNDO)
		MenuItem(AP_MENU_ID_EDIT_REDO)
		Separator()
		MenuItem(AP_MENU_ID_EDIT_CUT)
		MenuItem(AP_MENU_ID_EDIT_COPY)
		MenuItem(AP_MENU_ID_EDIT_PASTE)
#if !XAP_SIMPLE_MENU
		MenuItem(AP_MENU_ID_EDIT_PASTE_SPECIAL)
#endif
		Separator()
#if !XAP_SIMPLE_MENU
		MenuItem(AP_MENU_ID_EDIT_CLEAR)
#endif
		MenuItem(AP_MENU_ID_EDIT_SELECTALL)
		Separator()
#if !XAP_SIMPLE_MENU
		MenuItem(AP_MENU_ID_EDIT_REMOVEHEADER)
		MenuItem(AP_MENU_ID_EDIT_REMOVEFOOTER)
		Separator()
#endif
		MenuItem(AP_MENU_ID_EDIT_FIND)
		MenuItem(AP_MENU_ID_EDIT_REPLACE)
#if !XAP_SIMPLE_MENU
		MenuItem(AP_MENU_ID_EDIT_GOTO)
#endif
#if !(XAP_PREFSMENU_UNDER_TOOLS)
		Separator()
		MenuItem(AP_MENU_ID_TOOLS_OPTIONS)
#endif
	EndSubMenu()

	BeginSubMenu(AP_MENU_ID_VIEW)
		MenuItem(AP_MENU_ID_VIEW_NORMAL)
		MenuItem(AP_MENU_ID_VIEW_WEB)
		MenuItem(AP_MENU_ID_VIEW_PRINT)
#if !XAP_SIMPLE_MENU
		Separator()
		BeginSubMenu(AP_MENU_ID_VIEW_TOOLBARS)
			MenuItem(AP_MENU_ID_VIEW_TB_1)
			MenuItem(AP_MENU_ID_VIEW_TB_2)
			MenuItem(AP_MENU_ID_VIEW_TB_3)
			MenuItem(AP_MENU_ID_VIEW_TB_4)
		EndSubMenu()

		MenuItem(AP_MENU_ID_VIEW_RULER)
		MenuItem(AP_MENU_ID_VIEW_STATUSBAR)
		Separator()
		MenuItem(AP_MENU_ID_VIEW_LOCKSTYLES)
#endif

		MenuItem(AP_MENU_ID_VIEW_SHOWPARA)
		Separator()

#if !XAP_SIMPLE_MENU
		MenuItem(AP_MENU_ID_VIEW_FULLSCREEN)
#endif
		BeginSubMenu(AP_MENU_ID_VIEW_ZOOM_MENU)
			MenuItem(AP_MENU_ID_VIEW_ZOOM)
			MenuItem(AP_MENU_ID_VIEW_ZOOM_WIDTH)
			MenuItem(AP_MENU_ID_VIEW_ZOOM_WHOLE)
			MenuItem(AP_MENU_ID_VIEW_ZOOM_200)
			MenuItem(AP_MENU_ID_VIEW_ZOOM_100)
			MenuItem(AP_MENU_ID_VIEW_ZOOM_75)
			MenuItem(AP_MENU_ID_VIEW_ZOOM_50)
		EndSubMenu()
	EndSubMenu()

	BeginSubMenu(AP_MENU_ID_INSERT)
		MenuItem(AP_MENU_ID_INSERT_BREAK)
		MenuItem(AP_MENU_ID_INSERT_HEADER)
		MenuItem(AP_MENU_ID_INSERT_FOOTER)

		Separator()

        MenuItem(AP_MENU_ID_TABLE_INSERT_TABLE)
		MenuItem(AP_MENU_ID_INSERT_TEXTBOX)
		MenuItem(AP_MENU_ID_INSERT_TABLEOFCONTENTS)
        MenuItem(AP_MENU_ID_INSERT_FOOTNOTE)
		MenuItem(AP_MENU_ID_INSERT_ENDNOTE)

		Separator()

		MenuItem(AP_MENU_ID_INSERT_SYMBOL)
		MenuItem(AP_MENU_ID_EDIT_LATEXEQUATION)
		MenuItem(AP_MENU_ID_INSERT_PAGENO)
		MenuItem(AP_MENU_ID_INSERT_DATETIME)
		MenuItem(AP_MENU_ID_INSERT_FIELD)
		MenuItem(AP_MENU_ID_INSERT_BOOKMARK)
        MenuItem(AP_MENU_ID_INSERT_XMLID)
		MenuItem(AP_MENU_ID_INSERT_HYPERLINK)

		Separator()

		MenuItem(AP_MENU_ID_INSERT_FILE)

#if defined(TOOLKIT_GTK_ALL) && !XAP_SIMPLE_MENU
		MenuItem(AP_MENU_ID_INSERT_CLIPART)
#endif
		MenuItem(AP_MENU_ID_INSERT_GRAPHIC)

#if !XAP_SIMPLE_MENU
		Separator()

		BeginSubMenu(AP_MENU_ID_INSERT_DIRECTIONMARKER)
 	        MenuItem(AP_MENU_ID_INSERT_DIRECTIONMARKER_LRM)
	        MenuItem(AP_MENU_ID_INSERT_DIRECTIONMARKER_RLM)
	    EndSubMenu()
#endif

	EndSubMenu()

	BeginSubMenu(AP_MENU_ID_FORMAT)
		MenuItem(AP_MENU_ID_FMT_FONT)
		MenuItem(AP_MENU_ID_FMT_PARAGRAPH)
		MenuItem(AP_MENU_ID_FMT_BULLETS)
		MenuItem(AP_MENU_ID_FMT_TABLE)
// #if 0 // someone code and turn this back on
//	Maleesh 6/10/2010 -
		MenuItem(AP_MENU_ID_FMT_BORDERS)
// #endif
		Separator()
		MenuItem(AP_MENU_ID_FMT_COLUMNS)
		MenuItem(AP_MENU_ID_FMT_HDRFTR)
		MenuItem(AP_MENU_ID_FMT_FOOTNOTES)
		MenuItem(AP_MENU_ID_FMT_TABLEOFCONTENTS)
		Separator()
		MenuItem(AP_MENU_ID_FMT_TOGGLECASE)
		Separator()

		BeginSubMenu(AP_MENU_ID_ALIGN)
			MenuItem(AP_MENU_ID_ALIGN_LEFT)
			MenuItem(AP_MENU_ID_ALIGN_CENTER)
			MenuItem(AP_MENU_ID_ALIGN_RIGHT)
			MenuItem(AP_MENU_ID_ALIGN_JUSTIFY)
		EndSubMenu()

		BeginSubMenu(AP_MENU_ID_FMT)
			MenuItem(AP_MENU_ID_FMT_BOLD)
			MenuItem(AP_MENU_ID_FMT_ITALIC)
			MenuItem(AP_MENU_ID_FMT_UNDERLINE)
			MenuItem(AP_MENU_ID_FMT_OVERLINE)
			MenuItem(AP_MENU_ID_FMT_STRIKE)
			//MenuItem(AP_MENU_ID_FMT_TOPLINE)
			//MenuItem(AP_MENU_ID_FMT_BOTTOMLINE)
			MenuItem(AP_MENU_ID_FMT_SUPERSCRIPT)
			MenuItem(AP_MENU_ID_FMT_SUBSCRIPT)
			Separator()
			MenuItem(AP_MENU_ID_FMT_GROWFONT)
			MenuItem(AP_MENU_ID_FMT_SHRINKFONT)
			Separator()
			MenuItem(AP_MENU_ID_FMT_CLEARFMT)
		EndSubMenu()

#if !XAP_SIMPLE_MENU
		BeginSubMenu(AP_MENU_ID_FMT_BACKGROUND)
			MenuItem(AP_MENU_ID_FMT_BACKGROUND_PAGE_IMAGE)
			MenuItem(AP_MENU_ID_FMT_BACKGROUND_PAGE_COLOR)
		EndSubMenu()
#endif

#if !XAP_SIMPLE_MENU
		Separator()
		MenuItem(AP_MENU_ID_FMT_STYLE_DEFINE)
#endif

	EndSubMenu()

	BeginSubMenu(AP_MENU_ID_TOOLS)

#ifdef ENABLE_SPELL
		MenuItem(AP_MENU_ID_TOOLS_SPELL)
#endif
		MenuItem(AP_MENU_ID_FMT_LANGUAGE)
		MenuItem(AP_MENU_ID_TOOLS_WORDCOUNT)

		Separator()

		BeginSubMenu(AP_MENU_ID_TOOLS_REVISIONS)
			MenuItem(AP_MENU_ID_TOOLS_REVISIONS_MARK)
		    Separator()
	        MenuItem(AP_MENU_ID_TOOLS_REVISIONS_SHOW)
			MenuItem(AP_MENU_ID_TOOLS_REVISIONS_SHOW_AFTER)
			MenuItem(AP_MENU_ID_TOOLS_REVISIONS_SHOW_AFTERPREV)
			MenuItem(AP_MENU_ID_TOOLS_REVISIONS_SHOW_BEFORE)
			MenuItem(AP_MENU_ID_TOOLS_REVISIONS_SET_VIEW_LEVEL)
	        Separator()
			MenuItem(AP_MENU_ID_TOOLS_REVISIONS_FIND_NEXT)
			MenuItem(AP_MENU_ID_TOOLS_REVISIONS_FIND_PREV)
	        Separator()
			MenuItem(AP_MENU_ID_TOOLS_REVISIONS_ACCEPT_REVISION)
			MenuItem(AP_MENU_ID_TOOLS_REVISIONS_REJECT_REVISION)
		EndSubMenu()

		BeginSubMenu(AP_MENU_ID_TOOLS_ANNOTATIONS)
			MenuItem(AP_MENU_ID_TOOLS_ANNOTATIONS_INSERT)
			MenuItem(AP_MENU_ID_TOOLS_ANNOTATIONS_INSERT_FROMSEL)
			Separator()
			MenuItem(AP_MENU_ID_TOOLS_ANNOTATIONS_PREV)
			MenuItem(AP_MENU_ID_TOOLS_ANNOTATIONS_NEXT)
			Separator()
			MenuItem(AP_MENU_ID_TOOLS_ANNOTATIONS_DELETE)
			Separator()
			MenuItem(AP_MENU_ID_TOOLS_ANNOTATIONS_TOGGLE_DISPLAY)
		EndSubMenu()

	    Separator()

#if !XAP_SIMPLE_MENU
#if XAP_PREFSMENU_UNDER_TOOLS
		Separator()
		MenuItem(AP_MENU_ID_TOOLS_OPTIONS)
#endif
#endif
	EndSubMenu()

	BeginSubMenu(AP_MENU_ID_TABLE)

		BeginSubMenu(AP_MENU_ID_TABLE_INSERT)
            MenuItem(AP_MENU_ID_TABLE_INSERT_TABLE)
			MenuItem(AP_MENU_ID_TABLE_INSERT_COLUMNS_BEFORE)
			MenuItem(AP_MENU_ID_TABLE_INSERT_COLUMNS_AFTER)
			MenuItem(AP_MENU_ID_TABLE_INSERT_ROWS_BEFORE)
			MenuItem(AP_MENU_ID_TABLE_INSERT_ROWS_AFTER)
			MenuItem(AP_MENU_ID_TABLE_INSERT_SUMCOLS)
			MenuItem(AP_MENU_ID_TABLE_INSERT_SUMROWS)
#if 0
// Not for 2.4
			MenuItem(AP_MENU_ID_TABLE_INSERT_CELLS)
#endif
		EndSubMenu()

		BeginSubMenu(AP_MENU_ID_TABLE_DELETE)
			MenuItem(AP_MENU_ID_TABLE_DELETE_TABLE)
			MenuItem(AP_MENU_ID_TABLE_DELETE_COLUMNS)
			MenuItem(AP_MENU_ID_TABLE_DELETE_ROWS)
#if 0
// Not for 2.4
			MenuItem(AP_MENU_ID_TABLE_DELETE_CELLS)
#endif
		EndSubMenu()
		MenuItem(AP_MENU_ID_TABLE_FORMAT)
   		BeginSubMenu(AP_MENU_ID_TABLE_SELECT)
			MenuItem(AP_MENU_ID_TABLE_SELECT_TABLE)
			MenuItem(AP_MENU_ID_TABLE_SELECT_COLUMN)
			MenuItem(AP_MENU_ID_TABLE_SELECT_ROW)
			MenuItem(AP_MENU_ID_TABLE_SELECT_CELL)
		EndSubMenu()

		Separator()
		MenuItem(AP_MENU_ID_TABLE_MERGE_CELLS)
		MenuItem(AP_MENU_ID_TABLE_SPLIT_CELLS)
#if 0
// Not for 2.4
		MenuItem(AP_MENU_ID_TABLE_SPLIT_TABLE)
#endif
#if DEBUG
	    BeginSubMenu(AP_MENU_ID_TABLE_SORT)
			MenuItem(AP_MENU_ID_TABLE_SORTROWSASCEND)
			MenuItem(AP_MENU_ID_TABLE_SORTROWSDESCEND)
			MenuItem(AP_MENU_ID_TABLE_SORTCOLSASCEND)
			MenuItem(AP_MENU_ID_TABLE_SORTCOLSDESCEND)
	    EndSubMenu()
#endif
#if !XAP_SIMPLE_MENU
	    BeginSubMenu(AP_MENU_ID_TABLE_TABLETOTEXT)
	       MenuItem(AP_MENU_ID_TABLE_TABLETOTEXTCOMMAS)
	       MenuItem(AP_MENU_ID_TABLE_TABLETOTEXTTABS)
	       MenuItem(AP_MENU_ID_TABLE_TABLETOTEXTCOMMASTABS)
	    EndSubMenu()
#endif
		MenuItem(AP_MENU_ID_TABLE_AUTOFIT)
#if DEBUG
	    BeginSubMenu(AP_MENU_ID_TABLE_HEADING_ROWS_REPEAT)
                MenuItem(AP_MENU_ID_TABLE_HEADING_ROWS_REPEAT_THIS)
                MenuItem(AP_MENU_ID_TABLE_HEADING_ROWS_REPEAT_REMOVE)
	    EndSubMenu()
#endif
	EndSubMenu()

#if !XAP_SIMPLE_MENU
	BeginSubMenu(AP_MENU_ID_WINDOW)
		MenuItem(AP_MENU_ID_WINDOW_NEW)
		Separator()
		MenuItem(AP_MENU_ID_WINDOW_1)
		MenuItem(AP_MENU_ID_WINDOW_2)
		MenuItem(AP_MENU_ID_WINDOW_3)
		MenuItem(AP_MENU_ID_WINDOW_4)
		MenuItem(AP_MENU_ID_WINDOW_5)
		MenuItem(AP_MENU_ID_WINDOW_6)
		MenuItem(AP_MENU_ID_WINDOW_7)
		MenuItem(AP_MENU_ID_WINDOW_8)
		MenuItem(AP_MENU_ID_WINDOW_9)
		MenuItem(AP_MENU_ID_WINDOW_MORE)
	EndSubMenu()
	BeginSubMenu(AP_MENU_ID_HELP)
		MenuItem(AP_MENU_ID_HELP_CONTENTS)
		MenuItem(AP_MENU_ID_HELP_INTRO)
		MenuItem(AP_MENU_ID_HELP_SEARCH)
		MenuItem(AP_MENU_ID_HELP_CHANGELOG)
		MenuItem(AP_MENU_ID_HELP_CHECKVER)
		MenuItem(AP_MENU_ID_HELP_REPORT_BUG)
		Separator()
		Separator()
		MenuItem(AP_MENU_ID_HELP_ABOUT)
	EndSubMenu()
#endif

EndLayout()

#endif
