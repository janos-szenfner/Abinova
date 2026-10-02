/* AbiWord
 * Copyright (C) 1998,1999 AbiSource, Inc.
 * BIDI Copyright (c) 2001,2002 Tomas Frydrych, Yaacov Akiba Slama
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
#ifndef FL_AUTOLISTS_H
#define FL_AUTOLISTS_H

#include "ut_types.h"
#include "ut_xml.h"
#include "fp_types.h"

///////////////////////////////////////////////////////////////////////
// Lists Definitions are in fp_types.h
///////////////////////////////////////////////////////////////////////

class ABI_EXPORT fl_AutoLists
{
	public:
		UT_uint32 getXmlListsSize();
		UT_uint32 getFmtListsSize();

		const gchar * getXmlList(UT_uint32 i);
		const char *     getFmtList(UT_uint32 i);
};

#define IS_NUMBERED_LIST_TYPE(x) (((x) >= NUMBERED_LIST && (x) < BULLETED_LIST) || ((x) > OTHER_NUMBERED_LISTS && (x) < NOT_A_LIST))
#define IS_BULLETED_LIST_TYPE(x) ((x) >= BULLETED_LIST && (x) < OTHER_NUMBERED_LISTS)

#define IS_NONE_LIST_TYPE(x) ((x) == NOT_A_LIST)

#define  XML_NUMBERED_LIST (( const gchar *) "Numbered List")
#define  XML_LOWERCASE_LIST (static_cast<const gchar *>( "Lower Case List"))
#define  XML_UPPERCASE_LIST (static_cast<const gchar *>( "Upper Case List"))
#define  XML_LOWERROMAN_LIST (static_cast<const gchar *>( "Lower Roman List"))
#define  XML_UPPERROMAN_LIST (static_cast<const gchar *>( "Upper Roman List"))
#define  XML_HEBREW_LIST (static_cast<const gchar *>( "Hebrew List"))
#define  XML_ARABICNUM_LIST (static_cast<const gchar *>( "Arabic List"))
#define  XML_BULLETED_LIST (static_cast<const gchar *>( "Bullet List"))
#define  XML_DASHED_LIST (static_cast<const gchar *>( "Dashed List"))
#define  XML_SQUARE_LIST (static_cast<const gchar *>( "Square List"))
#define  XML_TRIANGLE_LIST (static_cast<const gchar *>( "Triangle List"))
#define  XML_DIAMOND_LIST (static_cast<const gchar *>( "Diamond List"))
#define  XML_STAR_LIST (static_cast<const gchar *>( "Star List"))
#define  XML_IMPLIES_LIST (static_cast<const gchar *>( "Implies List"))
#define  XML_TICK_LIST (static_cast<const gchar *>( "Tick List"))
#define  XML_BOX_LIST (static_cast<const gchar *>( "Box List"))
#define  XML_HAND_LIST (static_cast<const gchar *>( "Hand List"))
#define  XML_HEART_LIST (static_cast<const gchar *>( "Heart List"))
#define  XML_ARROWHEAD_LIST (static_cast<const gchar *>( "Arrowhead List"))

//
// MS Word uses 0.5 inches so we do too.
#define  LIST_DEFAULT_INDENT 0.50

// May need to tweak this some more...

#define  LIST_DEFAULT_INDENT_LABEL 0.30

//
// Reserved the first 10000 id's for useful purposes....
//
#define  AUTO_LIST_RESERVED 1000

#define fmt_NUMBERED_LIST (static_cast<const char *>("%*%d"))
#define  fmt_LOWERCASE_LIST (static_cast<const char *>("%*%a"))
#define  fmt_UPPERCASE_LIST (static_cast<const char *>("%*%A"))
#define  fmt_LOWERROMAN_LIST  (static_cast<const char *>("%*%r"))
#define  fmt_UPPERROMAN_LIST  (static_cast<const char *>("%*%R"))
#define  fmt_BULLETED_LIST (static_cast<const char *>("%b"))
#define  fmt_DASHED_LIST (static_cast<const char *>("%c"))
#define  fmt_HEBREW_LIST (static_cast<const char *>("%*%h"))
#define  fmt_ARABICNUM_LIST (static_cast<const char *>("%*%i"))


#endif
