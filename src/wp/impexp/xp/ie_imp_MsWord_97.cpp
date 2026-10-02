/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */

/* Abinova
 * Copyright (C) 1998-2000 AbiSource, Inc.
 * Copyright (C) 2001 Dom Lachowicz <dominicl@seas.upenn.edu>
 * Copyright (C) 2001-2003 Tomas Frydrych
 * Copyright (C) 2025 Hubert Figuière
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "ut_locale.h"

#include <zlib.h>

#include "wv.h"

#include "ut_string_class.h"
#include "ut_string.h"
#include "ut_std_string.h"
#include "ut_bytebuf.h"
#include "ut_units.h"
#include "ut_math.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"

#include "xap_App.h"
#include "xap_Frame.h"
#include "xap_EncodingManager.h"
#include "xap_DialogFactory.h"
#include "xap_Dlg_Password.h"

#include "fg_Graphic.h"
#include "fg_GraphicRaster.h"
#include "fg_GraphicVector.h"

#include "pd_Document.h"

#include "ie_impexp_MsWord_97.h"
#include "ie_imp_MsWord_97.h"
#include "ie_impGraphic.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"

#include "pf_Frag_Strux.h"
#include "pt_PieceTable.h"
#include "pd_Style.h"

#include "fp_PageSize.h"

#include "ut_Language.h"

#ifdef DEBUG
#define IE_IMP_MSWORD_DUMP
#include "ie_imp_MsWord_dump.h"
#undef IE_IMP_MSWORD_DUMP
#endif

#define X_CheckError(v) 		do { if (!(v)) return 1; } while (0)

// undef this to disable support for older images (<= Word95)
#define SUPPORTS_OLD_IMAGES 1

//#define BIDI_DEBUG
//
// Forward decls. to wv's callbacks
//
static int charProc (wvParseStruct *ps, U16 eachchar, U8 chartype, U16 lid);
static int specCharProc (wvParseStruct *ps, U16 eachchar, CHP* achp);
static int eleProc (wvParseStruct *ps, wvTag tag, void *props, int dirty);
static int docProc (wvParseStruct *ps, wvTag tag);

/*!
    Translates MS numerical id's for standard styles into our names
	The style names that have been commented out are those that do not
    have currently a localised equivalent in AW
*/
static const gchar * s_translateStyleId(UT_uint32 id)
{
	if(id >= 4094)
	{
		return nullptr;
	}

	// The style names that have been commented out are those that do
	// not currently have a localised equivalent in AW
	switch(id)
	{
		case 0:  return "Normal";
		case 1:  return "Heading 1";
		case 2:  return "Heading 2";
		case 3:  return "Heading 3";
		case 4:  return "Heading 4";
		case 5:  return "Heading 5";
		case 6:  return "Heading 6";
		case 7:  return "Heading 7";
		case 8:  return "Heading 8";
		case 9:  return "Heading 9";
		case 10: return nullptr /*"Index 1"*/;  /* Really a dup of 92? */
		case 11: return nullptr /*"Index 2"*/;
		case 12: return nullptr /*"Index 3"*/;
		case 13: return nullptr /*"Index 4"*/;
		case 14: return nullptr /*"Index 5"*/;
		case 15: return nullptr /*"Index 6"*/;
		case 16: return nullptr /*"Index 7"*/;
		case 17: return nullptr /*"Index 8"*/;
		case 18: return nullptr /*"Index 9"*/;
		case 19: return nullptr /*"Contents 1"*/; /* Handled by insertTOC? */
		case 20: return nullptr /*"Contents 2"*/; /* Handled by insertTOC? */
		case 21: return nullptr /*"Contents 3"*/; /* Handled by insertTOC? */
		case 22: return nullptr /*"Contents 4"*/; /* Handled by insertTOC? */
		case 23: return nullptr /*"TOC 5"*/; /* See Contents above for these five as well */
		case 24: return nullptr /*"TOC 6"*/;
		case 25: return nullptr /*"TOC 7"*/;
		case 26: return nullptr /*"TOC 8"*/;
		case 27: return nullptr /*"TOC 9"*/;
		case 28: return nullptr /*"Normal Indent"*/;
		case 29: return "Footnote Text";
		case 30: return nullptr /*"Comment Text"*/;
		case 31: return nullptr /*"Header"*/;
		case 32: return nullptr /*"Footer"*/;
		case 33: return nullptr /*"Index Heading"*/;
		case 34: return nullptr /*"Caption"*/;
		case 35: return nullptr /*"Table of Figures"*/;
		case 36: return nullptr /*"Envelope Address"*/;
		case 37: return nullptr /*"Envelope Return"*/;
		case 38: return "Footnote Reference";
		case 39: return nullptr /*"Comment Reference"*/;
		case 40: return nullptr /*"Line Number"*/;
		case 41: return nullptr /*"Page Number"*/;
		case 42: return "Endnote Reference";
		case 43: return "Endnote Text";
		case 44: return nullptr /*"Index of Authorities"*/;
		case 45: return nullptr /*"Macro Text"*/;
		case 46: return nullptr /*"TOA Heading"*/;
		case 47: return nullptr /*"List"*/;   //WARNING: beginPara appears to handle arbitrary lists via _mapDocToAbiList*
		case 48: return "Bulleted List";
		case 49: return "Numbered List";
		case 50: return nullptr /*"List 2"*/;
		case 51: return nullptr /*"List 3"*/;
		case 52: return nullptr /*"List 4"*/;
		case 53: return nullptr /*"List 5"*/;
		case 54: return nullptr /*"List Bullet 2"*/;
		case 55: return nullptr /*"List Bullet 3"*/;
		case 56: return nullptr /*"List Bullet 4"*/;
		case 57: return nullptr /*"List Bullet 5"*/;
		case 58: return nullptr /*"List Number 2"*/;
		case 59: return nullptr /*"List Number 3"*/;
		case 60: return nullptr /*"List Number 4"*/;
		case 61: return nullptr /*"List Number 5"*/;
		case 62: return "Title";
		case 63: return nullptr /*"Closing"*/;	
		case 64: return nullptr /*"Signature"*/;
		case 65: return nullptr /*"Default Paragraph Font"*/;
		case 66: return nullptr /*"Body Text"*/;
		case 67: return nullptr /*"Body Text Indent"*/;
		case 68: return nullptr /*"List Continue"*/;
		case 69: return nullptr /*"List Continue 2"*/;
		case 70: return nullptr /*"List Continue 3"*/;
		case 71: return nullptr /*"List Continue 4"*/;
		case 72: return nullptr /*"List Continue 5"*/;
		case 73: return nullptr /*"Message Header"*/;
		case 74: return "Subtitle";
		case 75: return nullptr /*"Salutation"*/;
		case 76: return nullptr /*"Date"*/;
		case 77: return nullptr /*"Body Text First Indent"*/;
		case 78: return nullptr /*"Body Text First Indent 2"*/;
		case 79: return nullptr /*"Note Heading"*/;
		case 80: return nullptr /*"Body Text 2"*/;
		case 81: return nullptr /*"Body Text 3"*/;
		case 82: return nullptr /*"Body Text Indent 2"*/;
		case 83: return nullptr /*"Body Text Indent 3"*/;
		case 84: return "Block Text";
		case 85: return nullptr /*"Hyperlink"*/;
		case 86: return nullptr /*"FollowedHyperlink"*/;
		case 87: return "Strong";
		case 88: return "Emphasis";
		case 89: return nullptr /*"Document Map"*/;
		case 90: return "Plain Text"; /* Really a dup of 109? */
		case 91: return nullptr /*"Email Signature"*/;
	    case 92: return nullptr /*"Index 1"*/;  /* Really a dup of 10? */
	    case 93: return nullptr /*"List Bullet"*/;
		case 94: return nullptr /*"Normal (Web)"*/;
		case 95: return nullptr /*"HTML Acronym"*/;
		case 96: return nullptr /*"HTML Address"*/;
		case 97: return nullptr /*"HTML Cite"*/;
		case 98: return nullptr /*"HTML Code"*/;
		case 99: return nullptr /*"HTML Definition"*/;
		case 100: return nullptr /*"HTML Keyboard"*/;
		case 101: return nullptr /*"HTML Preformatted"*/;
		case 102: return nullptr /*"HTML Sample"*/;
		case 103: return nullptr /*"HTML Typewriter"*/;
		case 104: return nullptr /*"HTML Variable"*/;
		case 105: return nullptr /*"Table Normal"*/;
		case 106: return nullptr /*"Comment Subject"*/;
		case 107: return nullptr /*"No List"*/;
		case 108: return nullptr /*"Index Heading"*/;
	    case 109: return "Plain Text";  /* Really a dup of 90? */
	    case 110: return nullptr /*"Hyperlink"*/;
	    case 111: return nullptr /*"FollowedHyperlink"*/;
    	case 112: return "Numbered List"; /* Was EnumList, really a dup of 49? Closer than nothing anyway*/
		case 115: return nullptr /*"Balloon Text"*/;

		case 153: return nullptr /*"Table of Authorities"*/;
		case 154: return nullptr /*"Grille du tableau" in fr_FR*/;

		default:
			UT_DEBUGMSG(("Unknown style Id [%d]; Please submit this document with a bug report!\n", id));
			// Would be nice if we had a UT_USERMSG or something to put up a prompt (with a
			// don't display again option) with the message in normal mode, OutputMsg or silent
			// in commandline or docserver mode, etc.  Because it is the users, not the
			// developers who will have such alien documents.  -MG
		
			UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
			return nullptr;
	}
	return nullptr;
}

/*!
    Strip characters that would confuse either the xml parser or our
    property parser; caller is responsible to g_free the returned pointer
*/
static char * s_stripDangerousChars(const char *s)
{
	UT_uint32 j, k;
	if(!s)
		return nullptr;
	
	char * t = static_cast<char*>( g_try_malloc(strlen(s)+1));
	UT_return_val_if_fail(t,nullptr);
	
	for(j = 0, k = 0; j < strlen(s); )
	{
	    if(s[j] < ' ' && s[j] >= 0 && s[j] != '\t' && s[j] != '\n' && s[j] != '\r')
	    {
	        j++;
	    }
	    else
	    {
		switch(s[j])
		{
			default:
				t[k++] = s[j++];
				break;

				// characters that would confuse the
				// xml parser or our own property parser
			case '<':
			case '>':
			case ':':
			case ';':
			case '&':
			case '\"':
				j++;
				break;
		}
	     }
	}
	
	t[k] = 0;
	
	return t;
}

static char * s_convert_to_utf8 (const wvParseStruct *ps, const char *s)
{
	// strangely wv seems to return an UTF-8 string despite a specified codepage
	// so we must ensure it is UTF-8. This is time consuming. :-(
	// If it is UTF-8 we just g_strdup() it.
	// See bug 13229.
	if (s == nullptr)
		return nullptr;
	if(g_utf8_validate(s, -1, nullptr)) {
		return g_strdup(s);
	}
	const char * encoding = nullptr;
	char fallback = '?';
	encoding = wvLIDToCodePageConverter(ps->fib.lid);
	return g_convert_with_fallback(s, -1, "UTF-8", encoding, &fallback, nullptr, nullptr, nullptr);
}

//
// DOC uses an unsigned int color index
//
typedef UT_uint32 Doc_Color_t;

//
// A mapping between Word's colors and Abi's RGB color scheme;
// if you add colors, _make sure_ to increase the '16' in 
// sMapIcoToColor() below
//
static Doc_Color_t word_colors [][3] = {
	{0x00, 0x00, 0x00}, /* black */
	{0x00, 0x00, 0xff}, /* blue */
	{0x00, 0xff, 0xff}, /* cyan */
	{0x00, 0xff, 0x00}, /* green */
	{0xff, 0x00, 0xff}, /* magenta */
	{0xff, 0x00, 0x00}, /* red */
	{0xff, 0xff, 0x00}, /* yellow */
	{0xff, 0xff, 0xff}, /* white */
	{0x00, 0x00, 0x80}, /* dark blue */
	{0x00, 0x80, 0x80}, /* dark cyan */
	{0x00, 0x80, 0x00}, /* dark green */
	{0x80, 0x00, 0x80}, /* dark magenta */
	{0x80, 0x00, 0x00}, /* dark red */
	{0x80, 0x80, 0x00}, /* dark yellow */
	{0x80, 0x80, 0x80}, /* dark gray */
	{0xc0, 0xc0, 0xc0}, /* light gray */
};

static bool s_mapColorRefToColor(UT_uint32 cv, UT_String & sColor);
static void s_emitParaBorder(UT_String &s, const char * pszSide,
							 const BRC * brc, UT_Dimension dim);

static UT_String sMapIcoToColor (UT_uint16 ico, bool bForeground)
{
	// need to handle the automatic colour 0; see bug 10261 for bounds-check
	if((!ico && bForeground) || (ico > 16))
	{
		ico = 1;  //black
	}
	else if(!ico && !bForeground)
	{
		ico = 8;  //white
	}

	return UT_String_sprintf("%02x%02x%02x",
							 word_colors[ico-1][0],
							 word_colors[ico-1][1],
							 word_colors[ico-1][2]);
}

//
// Field Ids that are useful later for mapping
//
enum Doc_Field_t: uint8_t {
	F_TIME,
	F_DATE,
	F_EDITTIME,
	F_AUTHOR,
	F_PAGE,
	F_NUMCHARS,
	F_NUMPAGES,
	F_NUMWORDS,
	F_FILENAME,
	F_HYPERLINK,
	F_PAGEREF,
	F_EMBED,
	F_TOC,
	F_DateTimePicture,
	F_TOC_FROM_RANGE,
	F_DATEINAME,
	F_SPEICHERDAT,
	F_MERGEFIELD,
	F_OTHER
};

struct field
{
	UT_UCS2Char command [FLD_SIZE];
	UT_UCS2Char argument [FLD_SIZE];
	UT_UCS2Char *fieldWhich;
	UT_sint32	fieldI;
	char *		fieldC;
	UT_sint32   fieldRet;
	Doc_Field_t type;
	// index of this field's begin character in the Plcfld of the
	// document part it lives in (-1 when unknown); used to resolve
	// _PID_HLINKS dwApp values (MS-DOC 2.4.7)
	UT_sint32   iFldStory;
	UT_sint32   iFldIndex;
};


//
// A mapping between DOC's field names and our given IDs
//
struct Doc_Field_Mapping_t
{
	const char * m_name;
	Doc_Field_t m_id;
};

/*
 * This next bit of code enables us to import many of Word's fields
 */

static Doc_Field_Mapping_t s_Tokens[] =
{
	{"TIME",	   F_TIME},
	{"EDITTIME",   F_EDITTIME},
	{"DATE",	   F_DATE},
	{"date",	   F_DATE},
	{"DATEINAME",      F_DATE}, // F_DATEINAME
	{"SPEICHERDAT",    F_DATE}, // F_SPEICHERDAT
	{"\\@", 	   F_DateTimePicture},

	{"FILENAME",   F_FILENAME},
	{"\\filename", F_FILENAME},
	{"PAGE",	   F_PAGE},
	{"\\*Arabisch",F_PAGE},
	{"NUMCHARS",   F_NUMCHARS},
	{"NUMPAGES",   F_NUMPAGES},
	{"NUMWORDS",   F_NUMWORDS},
	{"MERGEFIELD", F_MERGEFIELD},
	// these below aren't handled by Abinova, but they're known about
	{"HYPERLINK",  F_HYPERLINK},
	{"PAGEREF",    F_PAGEREF},
	{"EMBED",	   F_EMBED},
	{"TOC", 	   F_TOC},
	{"\\o", 	   F_TOC_FROM_RANGE},
	{"AUTHOR",	   F_AUTHOR},

	{ "*",		   F_OTHER}
};

#define FieldMappingSize (sizeof(s_Tokens)/sizeof(s_Tokens[0]))

static Doc_Field_t
s_mapNameToField (const char * name)
{
	for (unsigned int k = 0; k < FieldMappingSize; k++)
	{
		// field names can be sometimes in lower-case
		if (!g_ascii_strcasecmp(s_Tokens[k].m_name,name))
			return s_Tokens[k].m_id;
	}
	return F_OTHER;
}

#undef FieldMappingSize

static const char *
s_mapPageIdToString (UT_uint16 id)
{
	// TODO: make me way better when we determine code names

	switch (id)
	{
		case 0:  
		case 1:
			return "Letter";
		case 5:  return "Legal";
		case 7:  return nullptr; //"Executive";
		case 9:  return "A4";
		case 11: return "A5";
		case 13: return "Folio";
		case 14: return nullptr; // in Word this is "B5" but the size
							  // does not correspond to AW's B5
		case 20: return "Envelope No10";
		case 27: return "DL Envelope";
		case 28: return "C5";
		case 34: return "B5"; // in Word this is B5 Envelope ...
		case 37: return nullptr; //"Monarch Envelope";

		case 0xffff:
			// this is a value that wv uses to indicate that page size
			// is customised, just return nullptr
			return nullptr;
			
		default:
			UT_DEBUGMSG(("Unknow page size: please submit this document with a bug report\n"));
			UT_ASSERT_HARMLESS( 0 );
			return nullptr;
	}
}

/*!
  Surprise, surprise, there are more list numerical formats than the 5 the
  MS documentation states happens to mention, so here I will put what I found
  out (later we will move it to some better place)
*/
enum MSWordListIdType: int8_t
{
  WLNF_INVALID		   = -1,
  WLNF_EUROPEAN_ARABIC = 0,
  WLNF_UPPER_ROMAN	   = 1,
  WLNF_LOWER_ROMAN	   = 2,
  WLNF_UPPER_LETTER    = 3,
  WLNF_LOWER_LETTER    = 4,
  WLNF_ORDINAL		   = 5,
  WLNF_BULLETS		   = 23,
  WLNF_HEBREW_NUMBERS  = 45
};

struct ListIdLevelPair {
  UT_uint32 listId;
  UT_uint32 level;
};

/*!
 * Map a Word number format (LVLF.nfc, an MSONFC) plus the level's
 * number text onto an Abi list type.  For bullet levels (nfc 0x17) and
 * numberless levels (nfc 0xFF) the bullet glyph itself selects the Abi
 * bullet type, the same table the OOXML importer uses; Symbol and
 * Wingdings private-use codepoints are covered.
 */
static UT_uint32
s_mapDocToAbiListType (UT_uint32 nfc, const UT_uint16 * pStr,
					   UT_uint32 iLen)
{
  MSWordListIdType id = static_cast<MSWordListIdType>(nfc);

  if (id == WLNF_BULLETS || nfc == 0xFF)
	{
	  UT_uint32 i;
	  for (i = 0; pStr && i < iLen; i++)
		{
		  UT_UCS4Char c = pStr[i];
		  if (c <= 8)
			continue;   /* level placeholders are not the glyph */
		  switch (c)
			{
			case 0x00B7: /* middle dot (Symbol bullet) */
			case 0xF0B7: /* Symbol bullet via font */
			case 0x2022:
			  return 5;  /* BULLETED_LIST */
			case 0x002D:
			case 0x2013:
			  return 6;  /* DASHED_LIST */
			case 0x25A0:
			case 0x25AA:
			case 0xF0A7: /* Wingdings square */
			  return 7;  /* SQUARE_LIST */
			case 0x25B2:
			case 0x25BA:
			case 0xF0D8: /* Wingdings triangle */
			  return 8;  /* TRIANGLE_LIST */
			case 0x25C6:
			case 0x2666:
			case 0xF076: /* Wingdings diamond */
			  return 9;  /* DIAMOND_LIST */
			case 0x002A:
			case 0x2733:
			  return 10; /* STAR_LIST */
			case 0x21D2:
			case 0xF0DE: /* Wingdings double arrow */
			  return 11; /* IMPLIES_LIST */
			case 0x2713:
			case 0x2714:
			case 0xF0FC: /* Wingdings check */
			  return 12; /* TICK_LIST */
			case 0x25A1:
			case 0x2752:
			  return 13; /* BOX_LIST */
			case 0x261B:
			case 0x261E:
			  return 14; /* HAND_LIST */
			case 0x2665:
			case 0xF0A9: /* Wingdings heart */
			  return 15; /* HEART_LIST */
			case 0x27A3:
			case 0xF0D9: /* Wingdings arrowhead */
			  return 16; /* ARROWHEAD_LIST */
			default:
			  return 5;
			}
		}
	  return 5;
	}

  switch (id)
	{
	case WLNF_UPPER_ROMAN:
	  return 4;  /* UPPERROMAN_LIST */

	case WLNF_LOWER_ROMAN:
	  return 3;  /* LOWERROMAN_LIST */

	case WLNF_UPPER_LETTER:
	  return 2;  /* UPPERCASE_LIST */

	case WLNF_LOWER_LETTER:
	  return 1;  /* LOWERCASE_LIST */

	case WLNF_HEBREW_NUMBERS:
	  return 129; /* HEBREW_LIST */

	case WLNF_EUROPEAN_ARABIC:
	case WLNF_ORDINAL:
	default:
	  /* decimal, ordinal/-text, hex, CJK formats: plain decimal is
	     the closest Abi type */
	  return 0;  /* NUMBERED_LIST */
	}
}

/*!
 * Map a Word list level's number text onto Abi's list-delim and
 * list-decimal strings.
 *
 * The MS-DOC number text mixes literal characters with placeholders:
 * a character whose value is a zero-based ilvl (0-8) is replaced by
 * that level's number.  Abi composes a level label recursively as
 *   parentLabel + list-decimal + leftDelim + %L + rightDelim
 * and suppresses an ancestor's rightDelim when it equals that level's
 * own list-decimal (so "1.2.3." does not double the separator).  Hence
 * for level k:
 *   text before our placeholder, when ours is first -> delim prefix
 *   text after our placeholder                     -> delim suffix
 *   text between the previous placeholder and ours -> list-decimal
 *     (or our own suffix when ours is the first placeholder, which is
 *     what makes the ancestor-suffix suppression work)
 *
 * bRefsAncestors is set false when the number text references no
 * earlier level, so the caller can drop the parent link Word would
 * not have rendered.
 */
static void
s_mapDocToAbiListDelim (const UT_uint16 * pStr, UT_uint32 iLen,
						UT_uint32 iLvl, UT_UTF8String & sDelim,
						UT_UTF8String & sDecimal, bool & bRefsAncestors)
{
	std::vector<UT_sint32> ph;
	UT_sint32 i;
	UT_UTF8String sPfx, sSfx, sSep;

	bRefsAncestors = false;
	sDecimal.clear();

	if (!pStr)
		iLen = 0;

	for (i = 0; i < static_cast<UT_sint32>(iLen); i++)
	{
		if (pStr[i] <= 8)
			ph.push_back(i);   /* placeholder: value is the level */
	}

	if (ph.empty())
	{
		/* pure literal text (a bullet glyph, or a static label);
		   keep it as the delim prefix */
		for (i = 0; i < static_cast<UT_sint32>(iLen); i++)
		{
			UT_UCS4Char c = pStr[i];
			sPfx.appendUCS4(&c, 1);
		}
		sDelim = sPfx;
		sDelim += "%L";
		return;
	}

	/* our own placeholder: the first one whose value is this level,
	   falling back to the last placeholder for corrupt level text */
	UT_sint32 iOwn = -1;
	for (i = 0; i < static_cast<UT_sint32>(ph.size()); i++)
	{
		if (pStr[ph[i]] == iLvl)
		{
			iOwn = i;
			break;
		}
	}
	if (iOwn < 0)
		iOwn = static_cast<UT_sint32>(ph.size() )- 1;

	UT_sint32 ownPos = ph[iOwn];
	UT_sint32 nextPos = (iOwn + 1 < static_cast<UT_sint32>(ph.size()))
		? ph[iOwn + 1] : static_cast<UT_sint32>(iLen);

	if (iOwn > 0)
	{
		/* a placeholder for an earlier level precedes ours; the
		   text between the two is the inter-level separator */
		UT_sint32 prevPos = ph[iOwn - 1];
		bRefsAncestors = true;
		for (i = prevPos + 1; i < ownPos; i++)
		{
			UT_UCS4Char c = pStr[i];
			sSep.appendUCS4(&c, 1);
		}
	}
	else
	{
		for (i = 0; i < ownPos; i++)
		{
			UT_UCS4Char c = pStr[i];
			sPfx.appendUCS4(&c, 1);
		}
	}

	for (i = ownPos + 1; i < nextPos; i++)
	{
		UT_UCS4Char c = pStr[i];
		sSfx.appendUCS4(&c, 1);
	}

	sDelim = sPfx;
	sDelim += "%L";
	sDelim += sSfx;

	/* see the function comment: when there is a previous placeholder
	   the separator is the text between it and ours; otherwise our
	   suffix doubles as the decimal so a descendant's equal suffix
	   is suppressed when this level's label is composed into it */
	sDecimal = (iOwn > 0) ? sSep : sSfx;
}

/*!
 * Map an Abi list type back to one of the defined list styles
 */
static const char *
s_mapDocToAbiListStyle (UT_uint32 iType)
{
  switch (iType)
	{
	case 0:  return "Numbered List";
	case 1:  return "Lower Case List";
	case 2:  return "Upper Case List";
	case 3:  return "Lower Roman List";
	case 4:  return "Upper Roman List";
	case 5:  return "Bullet List";
	case 6:  return "Dashed List";
	case 7:  return "Square List";
	case 8:  return "Triangle List";
	case 9:  return "Diamond List";
	case 10: return "Star List";
	case 11: return "Implies List";
	case 12: return "Tick List";
	case 13: return "Box List";
	case 14: return "Hand List";
	case 15: return "Heart List";
	case 16: return "Arrowhead List";
	default: return "Numbered List"; /* hebrew + other numbered */
	}
}

/*!
 * Font for the list label field.  Bullet types keep "NULL" -- the
 * glyph Abi draws is fixed per type -- while numbered levels take the
 * number's own font, which wv has resolved into apap->linfo.chp from
 * the paragraph style plus the level's grpprlChpx.
 */
static std::string
s_fieldFontForList (UT_uint32 iType, wvParseStruct * ps, const CHP * achp)
{
	if (iType >= 5 && iType < 0x7f) /* bullet types */
		return "NULL";

	if (ps && achp)
	{
		char * fname = nullptr;
		if (achp->xchSym)
			fname = wvGetFontnameFromCode (&ps->fonts, achp->ftcSym);
		else if (achp->fBidi)
			fname = wvGetFontnameFromCode (&ps->fonts, achp->ftcBidi);
		else if (!ps->fib.fFarEast)
			fname = wvGetFontnameFromCode (&ps->fonts, achp->ftcAscii);
		else
			fname = wvGetFontnameFromCode (&ps->fonts, achp->ftcFE);

		if (fname)
		{
			std::string sFont = fname;
			FREEP(fname);
			return sFont;
		}
	}
	return "Times New Roman";
}

#if 0

// MS Word uses the langauge codes as explicit overrides when treating
// weak characters; this function translates language id to the
// overrided direction
static bool s_isLanguageRTL(short unsigned int lid)
{
	const char * s = wvLIDToLangConverter (lid);
	UT_Language l;
	return (UTLANG_RTL == l.getOrderFromProperty(s));
}

static FootnoteType s_convertNoteType(UT_uint32 t)
{
	return 	FOOTNOTE_TYPE_NUMERIC;
}

#endif

/****************************************************************************/
/****************************************************************************/

IE_Imp_MsWord_97_Sniffer::IE_Imp_MsWord_97_Sniffer ()
	: IE_ImpSniffer(IE_IMPEXPNAME_MSWORD97)
{
	//
}

// supported suffixes
static IE_SuffixConfidence IE_Imp_MsWord_97_Sniffer__SuffixConfidence[] = {
	{ "doc", 	UT_CONFIDENCE_PERFECT 	},
	{ "dot", 	UT_CONFIDENCE_PERFECT 	},
	{ "", 	UT_CONFIDENCE_ZILCH 	}
};

const IE_SuffixConfidence * IE_Imp_MsWord_97_Sniffer::getSuffixConfidence ()
{
	return IE_Imp_MsWord_97_Sniffer__SuffixConfidence;
}

// supported mimetypes
static IE_MimeConfidence IE_Imp_MsWord_97_Sniffer__MimeConfidence[] = {
	{ IE_MIME_MATCH_FULL, 	IE_MIMETYPE_MSWord, 		UT_CONFIDENCE_GOOD 	},
	{ IE_MIME_MATCH_FULL, 	"application/vnd.ms-word",	UT_CONFIDENCE_GOOD 	},
	{ IE_MIME_MATCH_FULL, 	"text/doc", 				UT_CONFIDENCE_GOOD 	}, // or is it? [TODO: check!]
	{ IE_MIME_MATCH_BOGUS, 	"", 						UT_CONFIDENCE_ZILCH }
};

const IE_MimeConfidence * IE_Imp_MsWord_97_Sniffer::getMimeConfidence ()
{
	return IE_Imp_MsWord_97_Sniffer__MimeConfidence;
}

UT_Confidence_t IE_Imp_MsWord_97_Sniffer::recognizeContents (GsfInput * input)
{
	GsfInfile * ole;

	ole = gsf_infile_msole_new (input, nullptr);

	// invokes the old recognizeContents below, in hopes of identifying
	// pre-OLE files
	if (!ole)
		return IE_ImpSniffer::recognizeContents (input);

	UT_Confidence_t confidence = UT_CONFIDENCE_ZILCH;
	GsfInput * stream = gsf_infile_child_by_name (ole, "WordDocument");
	if (stream)
		{
			g_object_unref (G_OBJECT (stream));
			confidence = UT_CONFIDENCE_PERFECT;
		}

	g_object_unref (G_OBJECT (ole));

	return confidence;
}

UT_Confidence_t IE_Imp_MsWord_97_Sniffer::recognizeContents (const char * szBuf,
															 UT_uint32 iNumbytes)
{
	const char * magic	= nullptr;
	int magicoffset = 0;

	magic = "Microsoft Word 6.0 Document";
	magicoffset = 2080;
	if (iNumbytes > (magicoffset + strlen (magic)))
	{
		if (!strncmp (szBuf + magicoffset, magic, strlen (magic)))
		{
			return UT_CONFIDENCE_PERFECT;
		}
	}

	magic = "Documento Microsoft Word 6";
	magicoffset = 2080;
	if (iNumbytes > (magicoffset + strlen (magic)))
	{
		if (!strncmp(szBuf + magicoffset, magic, strlen (magic)))
		{
			return UT_CONFIDENCE_PERFECT;
		}
	}

	magic = "MSWordDoc";
	magicoffset = 2112;
	if (iNumbytes > (magicoffset + strlen (magic)))
	{
		if (!strncmp (szBuf + magicoffset, magic, strlen (magic)))
		{
			return UT_CONFIDENCE_PERFECT;
		}
	}

	// ok, that didn't work, we'll try to dig through the OLE stream
	if (iNumbytes > 8)
	{
	        // this code is too generic - also picks up .wri documents
		if (szBuf[0] == static_cast<char>(0x31)
			&& static_cast< unsigned char>(szBuf[1]) == static_cast< unsigned char>(0xbe)
			&&  szBuf[2] == static_cast<char>(0)
			&& szBuf[3] == static_cast<char>(0))
		{
		  return UT_CONFIDENCE_SOSO; //POOR
		}

		// this identifies staroffice dox as well
		if (static_cast< unsigned char>(szBuf[0]) == static_cast<unsigned char>(0xd0)
			&& static_cast< unsigned char>(szBuf[1]) == static_cast<unsigned char>(0xcf)
			&& szBuf[2] == static_cast<char>(0x11)
			&& static_cast< unsigned char>(szBuf[3]) == static_cast<unsigned char>(0xe0)
			&& static_cast< unsigned char>(szBuf[4]) == static_cast<unsigned char>(0xa1)
			&& static_cast< unsigned char>(szBuf[5]) == static_cast<unsigned char>(0xb1)
			&& szBuf[6] == static_cast<char>(0x1a)
			&& static_cast< unsigned char>(szBuf[7]) == static_cast<unsigned char>(0xe1))
		{
		  return UT_CONFIDENCE_SOSO; // POOR
		}

		if (szBuf[0] == 'P' && szBuf[1] == 'O' &&
			szBuf[2] == '^' && szBuf[3] == 'Q' && szBuf[4] == '`')
		{
			return UT_CONFIDENCE_POOR;
		}
		if (static_cast< unsigned char>(szBuf[0]) == static_cast<unsigned char>(0xfe)
			&& szBuf[1] == static_cast<char>(0x37)
			&& szBuf[2] == static_cast<char>(0)
			&& szBuf[3] == static_cast<char>(0x23))
		{
			return UT_CONFIDENCE_POOR;
		}

		/* WinWord 2 */
		if (static_cast< unsigned char>(szBuf[0]) == static_cast<unsigned char>(0xdb)
			&& static_cast< unsigned char>(szBuf[1]) == static_cast<unsigned char>(0xa5)
			&& szBuf[2] == static_cast<char>(0x2d)
			&& szBuf[3] == static_cast<char>(0))
		{
			return UT_CONFIDENCE_PERFECT;
		}
	}
	return UT_CONFIDENCE_ZILCH;
}

UT_Error IE_Imp_MsWord_97_Sniffer::constructImporter (PD_Document * pDocument,
													  IE_Imp ** ppie)
{
	IE_Imp_MsWord_97 * p = new IE_Imp_MsWord_97(pDocument);
	*ppie = p;
	return UT_OK;
}

bool	IE_Imp_MsWord_97_Sniffer::getDlgLabels (const char ** pszDesc,
												const char ** pszSuffixList,
												IEFileType * ft)
{
	*pszDesc = "Microsoft Word (.doc, .dot)";
	*pszSuffixList = "*.doc; *.dot";
	*ft = getFileType();
	return true;
}

/****************************************************************************/
/****************************************************************************/

// just buffer sizes, arbitrarily chosen
#define DOC_TEXTRUN_SIZE 2048
#define DOC_PROPBUFFER_SIZE 1024

IE_Imp_MsWord_97::~IE_Imp_MsWord_97()
{
	if(m_pBookmarks)
	{
		// g_free the names from the bookmarks
		for(UT_uint32 i = 0; i < m_iBookmarksCount; i++)
		{
			// make sure we do not delete any name twice
			if(m_pBookmarks[i].name && m_pBookmarks[i].start)
			{
			   delete[] m_pBookmarks[i].name;
			   m_pBookmarks[i].name = nullptr;
			}
		}
		delete [] m_pBookmarks;
	}

	UT_VECTOR_PURGEALL(ListIdLevelPair *, m_vLists);
	UT_VECTOR_PURGEALL(emObject *, m_vecEmObjects);
	UT_VECTOR_PURGEALL(textboxPos *, m_vecTextboxPos);
	UT_VECTOR_PURGEALL(MsTableCtx *, m_vecTableCtx);

	DELETEPV(m_pTextboxes);
	DELETEPV(m_pFootnotes);
	DELETEPV(m_pEndnotes);
	DELETEPV(m_pAnnotations);
	DELETEPV(m_pHeaders);
	_freeFields();
}

IE_Imp_MsWord_97::IE_Imp_MsWord_97(PD_Document * pDocument)
  : IE_Imp (pDocument),
	m_nSections(0),
	m_bSetPageSize(false),
	m_bIsLower(false),
	m_bInSect(false),
	m_bInPara(false),
	m_bLTRCharContext(true),
	m_bLTRParaContext(true),
	m_bBidiMode(false),
	m_bInLink(false),
	m_pBookmarks(nullptr),
	m_iBookmarksCount(0),
	m_pFootnotes(nullptr),
	m_iFootnotesCount(0),
	m_pEndnotes(nullptr),
	m_iEndnotesCount(0),
	m_pTextboxes(nullptr),
	m_iTextboxCount(0),
	m_dSectMarginLeft(1.0),
	m_dSectMarginTop(1.0),
    m_iMSWordListId(0),
    m_bEncounteredRevision(false),
    m_bInTable(false),
	m_iFootnotesStart(0xffffffff),
	m_iFootnotesEnd(0xffffffff),
	m_iEndnotesStart(0xffffffff),
	m_iEndnotesEnd(0xffffffff),
	m_iNextFNote(0),
	m_iNextENote(0),
	m_bInFNotes(false),
	m_bInENotes(false),
	m_pNotesEndSection(nullptr),
	m_pAnnotations(nullptr),
	m_iAnnotationsCount(0),
	m_iNextAnnotation(0),
	m_iAnnAnchor(0),
	m_bInAnnotations(false),
	m_pAnnotationEndSection(nullptr),
	m_pHeaders(nullptr),
	m_iHeadersCount(0),
	m_iHeadersStart(0xffffffff),
	m_iHeadersEnd(0xffffffff),
	m_iCurrentHeader(0),
	m_bInHeaders(false),
	m_iCurrentSectId(0),
	m_iAnnotationsStart(0xffffffff),
	m_iAnnotationsEnd(0xffffffff),
	m_iMacrosStart(0xffffffff),
	m_iMacrosEnd(0xffffffff),
	m_iTextStart(0xffffffff),
	m_iTextEnd(0xffffffff),
	m_bPageBreakPending(false),
    m_bLineBreakPending(false),
	m_bSymbolFont(false),
	m_dim(DIM_IN),
	m_iTextboxesStart(0xffffffff),
	m_iTextboxesEnd(0xffffffff),
	m_iNextTextbox(0),
	m_iPrevHeaderPosition(0xffffffff),
	m_bEvenOddHeaders(false),
	m_bInTOC(false),
	m_bTOCsupported(false),
	m_bInTextboxes(false),
	m_pTextboxEndSection(nullptr),
	m_iLastAppendedHeader(0xffffffff),
	m_iBmCursor(0)
{
  for(UT_uint32 i = 0; i < 9; i++)
	  m_iListIdIncrement[i] = 0;
  m_vecTextboxPos.clear();
  memset(m_aStoryFlds, 0, sizeof(m_aStoryFlds));
}

/****************************************************************************/
/****************************************************************************/

#define ErrCleanupAndExit(code)  do {wvOLEFree (&ps); return(code);} while(0)

#define GetPassword() _getPassword ( XAP_App::getApp()->getLastFocussedFrame() )

#define ErrorMessage(x) do { XAP_Frame *_pFrame = XAP_App::getApp()->getLastFocussedFrame(); if ( _pFrame ) _errorMessage (_pFrame, (x)); } while (0)

static UT_UTF8String _getPassword (XAP_Frame * pFrame)
{
  UT_UTF8String password ( "" );

  if ( pFrame )
    {
      pFrame->raise ();

      XAP_DialogFactory * pDialogFactory
		  = static_cast<XAP_DialogFactory *>((pFrame->getDialogFactory()));

      XAP_Dialog_Password * pDlg = static_cast<XAP_Dialog_Password*>(pDialogFactory->requestDialog(XAP_DIALOG_ID_PASSWORD));
      UT_return_val_if_fail(pDlg, password);

      pDlg->runModal (pFrame);

      XAP_Dialog_Password::tAnswer ans = pDlg->getAnswer();
      bool bOK = (ans == XAP_Dialog_Password::a_OK);

      if (bOK)
		  password = pDlg->getPassword ();

      pDialogFactory->releaseDialog(pDlg);
    }
  else
    {
      // headless (e.g. --to= conversions): allow the password to be
      // supplied via the environment, like the ODF/.abwn paths
      const char * envpw = getenv ("ABINOVA_PASSWORD");
      if (envpw)
		  password = envpw;
    }

  return password;
}

#if 0
static void _errorMessage (XAP_Frame * pFrame, int id)
{
  UT_return_if_fail(pFrame);

  const XAP_StringSet * pSS = XAP_App::getApp ()->getStringSet ();

  const char * text = pSS->getValue (id, pFrame->getApp()->getDefaultEncoding()).c_str();

  pFrame->showMessageBox (text, XAP_Dialog_MessageBox::b_O,
						  XAP_Dialog_MessageBox::a_OK);
}
#endif

static const struct {
  const char * metadata_key;
  const char * abi_metadata_name;
} metadata_names[] = {
  { GSF_META_NAME_TITLE, PD_META_KEY_TITLE },
  { GSF_META_NAME_DESCRIPTION, PD_META_KEY_DESCRIPTION },
  { GSF_META_NAME_SUBJECT, PD_META_KEY_SUBJECT },
  { GSF_META_NAME_DATE_MODIFIED, PD_META_KEY_DATE_LAST_CHANGED },
  { GSF_META_NAME_DATE_CREATED, PD_META_KEY_DATE },
  { GSF_META_NAME_KEYWORDS, PD_META_KEY_KEYWORDS },
  { GSF_META_NAME_LANGUAGE, PD_META_KEY_LANGUAGE },
  { GSF_META_NAME_REVISION_COUNT, PD_META_KEY_REVISION },
  { GSF_META_NAME_EDITING_DURATION, PD_META_KEY_EDITING_DURATION },
  { GSF_META_NAME_TABLE_COUNT, nullptr },
  { GSF_META_NAME_IMAGE_COUNT, nullptr },
  { GSF_META_NAME_OBJECT_COUNT, nullptr },
  { GSF_META_NAME_PAGE_COUNT, nullptr },
  { GSF_META_NAME_PARAGRAPH_COUNT, nullptr },
  { GSF_META_NAME_WORD_COUNT, nullptr },
  { GSF_META_NAME_CHARACTER_COUNT, nullptr },
  { GSF_META_NAME_CELL_COUNT, nullptr },
  { GSF_META_NAME_SPREADSHEET_COUNT, nullptr },
  { GSF_META_NAME_CREATOR, PD_META_KEY_CREATOR },
  { GSF_META_NAME_TEMPLATE, PD_META_KEY_TEMPLATE },
  { GSF_META_NAME_LAST_SAVED_BY, PD_META_KEY_LASTMODIFIEDBY },
  { GSF_META_NAME_LAST_PRINTED, PD_META_KEY_LASTPRINTED },
  { GSF_META_NAME_SECURITY, nullptr },
  { GSF_META_NAME_CATEGORY, PD_META_KEY_CATEGORY },
  { GSF_META_NAME_PRESENTATION_FORMAT, nullptr },
  { GSF_META_NAME_THUMBNAIL, nullptr },
  { GSF_META_NAME_GENERATOR, PD_META_KEY_GENERATOR },
  { GSF_META_NAME_LINE_COUNT, nullptr },
  { GSF_META_NAME_SLIDE_COUNT, nullptr },
  { GSF_META_NAME_NOTE_COUNT, nullptr },
  { GSF_META_NAME_HIDDEN_SLIDE_COUNT, nullptr },
  { GSF_META_NAME_MM_CLIP_COUNT, nullptr },
  { GSF_META_NAME_BYTE_COUNT, nullptr },
  { GSF_META_NAME_SCALE, nullptr },
  { GSF_META_NAME_HEADING_PAIRS, nullptr },
  { GSF_META_NAME_DOCUMENT_PARTS, nullptr },
  { GSF_META_NAME_MANAGER, PD_META_KEY_MANAGER },
  { GSF_META_NAME_COMPANY, PD_META_KEY_COMPANY },
  { GSF_META_NAME_LINKS_DIRTY, nullptr },
  { GSF_META_NAME_MSOLE_UNKNOWN_17, nullptr },
  { GSF_META_NAME_MSOLE_UNKNOWN_18, nullptr },
  { GSF_META_NAME_MSOLE_UNKNOWN_19, nullptr },
  { GSF_META_NAME_MSOLE_UNKNOWN_20, nullptr },
  { GSF_META_NAME_MSOLE_UNKNOWN_21, nullptr },
  { GSF_META_NAME_MSOLE_UNKNOWN_22, nullptr },
  { GSF_META_NAME_MSOLE_UNKNOWN_23, nullptr },
  { GSF_META_NAME_DICTIONARY, nullptr },
  { GSF_META_NAME_LOCALE_SYSTEM_DEFAULT, nullptr },
  { GSF_META_NAME_CASE_SENSITIVE, nullptr }
};
static const gsize nr_metadata_names = G_N_ELEMENTS(metadata_names);

struct DocAndLid
{
	PD_Document *doc;
	int lid;
};

static void
cb_print_property (char const *name, GsfDocProp const *prop, DocAndLid * doc)
{
  GValue const *val = gsf_doc_prop_get_val  (prop);

  if (! VAL_IS_GSF_DOCPROP_VECTOR (const_cast<GValue *>(static_cast<const GValue*>(val)))) {

	  // just scan over the table. consider optimizing if we really care to.
	  for(gsize i = 0; i < nr_metadata_names; i++) {
		  if(strcmp(metadata_names[i].metadata_key, name) == 0) {
			  char const * abi_metadata_name = metadata_names[i].abi_metadata_name;
			
			  if(abi_metadata_name != nullptr) {
				  const char * encoding = nullptr;
				  if (doc->lid >> 8 != 0x04) {
					// header is not utf8 encoded
				  	encoding = wvLIDToCodePageConverter(doc->lid);
				  }
				  char *tmp;

				  if (G_VALUE_HOLDS(val, G_TYPE_STRING))
					  {
						  // special-case strings. it seems that g_value_get_string()
						  // and g_strdup_value_contents() may return different things
						  // check with document from bug 11148
						  const char * contents = g_value_get_string(val);
						  
						  if (encoding && *encoding)
							  {
								  tmp = g_convert_with_fallback(contents, -1, static_cast<gchar*>("UTF-8"), encoding, static_cast<gchar*>("?"), nullptr, nullptr, nullptr);
							  }
						  else
							  {
								  tmp = g_strdup(contents);
							  }
						  
					  }
				  else
					  {
						  // coerce into a string
						  tmp = g_strdup_value_contents(val);
					  }

				  char * meta = tmp;
				  // strip beginning and ending quotes
				  if(meta && strcmp(meta,"\"\"")) { // ignore '""' props
					  if(meta[0] == '"')
						  meta++;
					  int len = strlen(meta);
					  if ((len > 0) && meta[len - 1] == '"') {
						  meta[len - 1] = '\0';
					  }
					  if (*meta) {
						  doc->doc->setMetaDataProp(abi_metadata_name, meta);
					  }
				  }
				  g_free (tmp);			  
			  }
		  }
	  }
  }
}

static void print_summary_stream (GsfInfile * msole,
								  const char * stream_name,
								  int lid,
								  PD_Document * doc)
{
  GsfInput * stream = gsf_infile_child_by_name (msole, stream_name);
  if (stream != nullptr) {
    GsfDocMetaData *meta_data = gsf_doc_meta_data_new ();
    GError    *err = nullptr;

    err = gsf_doc_meta_data_read_from_msole(meta_data, stream);
    if (err != nullptr) {
      g_warning ("Error getting metadata for %s: %s", stream_name, err->message);
      g_error_free (err);
      err = nullptr;
    } else {
		DocAndLid dil;

		dil.doc = doc;
		dil.lid = lid;
		gsf_doc_meta_data_foreach (meta_data,
								   reinterpret_cast<GHFunc>( cb_print_property), &dil);
    }

    g_object_unref (meta_data);
    g_object_unref (G_OBJECT (stream));
  }
}

void IE_Imp_MsWord_97::_handleMetaData(wvParseStruct *ps)
{
	print_summary_stream (GSF_INFILE(ps->ole_file), "\05SummaryInformation", ps->fib.lid, getDoc());
	print_summary_stream (GSF_INFILE(ps->ole_file), "\05DocumentSummaryInformation", ps->fib.lid, getDoc());
}

UT_Error IE_Imp_MsWord_97::_loadFile(GsfInput * fp)
{
  wvParseStruct ps;

  int ret = wvInitParser_gsf(&ps, fp);
  const char * password = nullptr;

  if (ret & 0x8000)		/* Password protected? */
    {
      UT_UTF8String pass (GetPassword());
      if ( pass.size () != 0 )
		  password = pass.utf8_str();

      if ((ret & 0x7fff) == WORD8)
	{
	  ret = 0;
	  if (password == nullptr)
	    {
	      ErrCleanupAndExit(UT_IE_PROTECTED);
	    }
	  else
	    {
	      wvSetPassword (password, &ps);
	      /* MS-DOC 2.2.6: fEncrypted+fObfuscated is XOR obfuscation;
		 fEncrypted alone is RC4 (wvDecrypt97 also rejects the
		 CryptoAPI/AES EncryptionVersionInfo variants) */
	      if (ps.fib.fCrypto
		  ? wvDecryptObfuscated (&ps)
		  : wvDecrypt97 (&ps))
		{
		  ErrCleanupAndExit(UT_IE_PROTECTED);
		}
	    }
	}
      else if (((ret & 0x7fff) == WORD7) || ((ret & 0x7fff) == WORD6))
	{
	  ret = 0;
	  if (password == nullptr)
	    {
	      ErrCleanupAndExit(UT_IE_PROTECTED);
	    }
	  else
	    {
	      wvSetPassword (password, &ps);
	      if (wvDecrypt95 (&ps))
		{
		  ErrCleanupAndExit(UT_IE_PROTECTED);
		}
	    }
	}
      else
	{
	  /* protected pre-Word95 document -- nothing to decrypt it
	     with, but report it as protected rather than corrupt */
	  ErrCleanupAndExit(UT_IE_PROTECTED);
	}
    }

  if (ret) {
    ErrCleanupAndExit(UT_IE_BOGUSDOCUMENT);
  }

  // register ourself as the userData
  ps.userData = this;

  // register callbacks
  wvSetElementHandler (&ps, eleProc);
  wvSetCharHandler (&ps, charProc);
  wvSetSpecialCharHandler(&ps, specCharProc);
  wvSetDocumentHandler (&ps, docProc);

  // need to init doc props
  if(!getLoadStylesOnly())
	  getDoc()->setAttrProp(PP_NOPROPS);
  
  _handleMetaData(&ps);
  _parseHyperlinkProps(&ps);
  wvText(&ps);

  if(getLoadStylesOnly()) {
    wvOLEFree(&ps);
    return UT_OK;
  }

  wvOLEFree(&ps);

  // We can't be in a good state if we didn't add any sections!
  if (m_nSections == 0)
    return UT_IE_BOGUSDOCUMENT;

  return UT_OK;
}

void IE_Imp_MsWord_97::_flush ()
{
  if(!m_pTextRun.size())
	return;

  // we've got to ensure that we're inside of a section & paragraph
  if (!m_bInSect)
	{
	  // append a blank default section - assume it works
	  UT_DEBUGMSG(("#TF: _flush: appending default section\n"));
	  _appendStrux(PTX_Section, PP_NOPROPS);
	  m_bInSect = true;
	  m_nSections++;
	}

  pf_Frag * pF = getDoc()->getLastFrag();
  if (pF && pF->getType() == pf_Frag::PFT_Strux) {
	  pf_Frag_Strux * pFS = static_cast<pf_Frag_Strux*>(pF);
	  if ((pFS->getStruxType() != PTX_Block) && (pFS->getStruxType() != PTX_EndFootnote) && (pFS->getStruxType() != PTX_EndEndnote) && (pFS->getStruxType() != PTX_EndAnnotation))
		  m_bInPara = false;
  }

  if(!m_bInPara)
  {
	  // append a blank defaul paragraph - assume it works
	  UT_DEBUGMSG(("#TF: _flush: appending default block\n"));
	  _appendStrux(PTX_Block, PP_NOPROPS);
	  m_bInPara = true;
	  emObject * pObject = nullptr;
	  if(m_vecEmObjects.getItemCount() > 0)
	  {
		  UT_sint32 i =0;
		  for(i=0;i< m_vecEmObjects.getItemCount(); i++)
		  {
			  pObject = m_vecEmObjects.getNthItem(i);
			  UT_nonnull_or_continue(pObject);
			  if(pObject->objType == PTO_Bookmark)
			  {
				  PP_PropertyVector propsArray = {
					  "name", pObject->props1.c_str(),
					  "type", pObject->props2.c_str()
				  };
				  _appendObject (PTO_Bookmark, propsArray);
			  }
			  else
			  {
				  UT_DEBUGMSG(("MSWord 97 _flush: Object not handled \n"));
				  UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
			  }
			  delete pObject;
		  }
		  m_vecEmObjects.clear();
	  }
  }

  if (m_pTextRun.size())
  {
	  // bidi adjustments for neutrals
	  // 
	  // We have a problem in bidi documents caused by the fact that
	  // Word does not use the Unicode bidi algorithm, but rather one of
	  // its own, which adds keyboard language to the equation. We get
	  // around this by issuing an explicit direction override on the
	  // neutral characters. We do it here in the _flush() function
	  // because when we have both left and right context available
	  // for these characters we can tell if the override is
	  // superfluous, which it is most of the time; omitting the
	  // sufperfluous overrides allows us to import documents in a
	  // manner that will make them feel more like native AW docs.
	  // (This does not get rid of all the unnecessary overrides, for
	  // that we would need to have the text of an entire paragraph)
	  //
	  // I goes without saying that it would be highly desirable to be
	  // able to determine at the start if a document is pure LTR (as
	  // we do in the RTF importer), since that would save us lot of
	  // extra processing
	  // Tomas, May 8, 2003
	  
	  if(m_bBidiMode)
	  {
		  const gchar* pProps = "props";
		  UT_String prop_basic = m_charProps;

		  UT_String prop_ltr = prop_basic;
		  UT_String prop_rtl = prop_basic;

		  if(prop_basic.size())
		  {
			  prop_ltr += ";";
			  prop_rtl += ";";
		  }
		  else
		  {
			  // if the char props are empty, we need replace them
			  // with the following to avoid asserts in PP_AttrProp
			  prop_basic = "dir-override:";
		  }
		  
		  
		  prop_ltr += "dir-override:ltr";
		  prop_rtl += "dir-override:rtl";

		  PP_PropertyVector propsArray = {
			  pProps, prop_basic.c_str()
		  };

		  if(m_charRevs.size())
		  {
			  propsArray.push_back("revision");
			  propsArray.push_back(m_charRevs.c_str());
		  }
		  
		  const UT_UCS4Char * p;
		  const UT_UCS4Char * pStart = m_pTextRun.ucs4_str();
		  UT_uint32 iLen = m_pTextRun.size();
		  
		  UT_BidiCharType iOverride = UT_BIDI_UNSET, cType, cLastType = UT_BIDI_UNSET, cNextType;
		  UT_uint32 iLast = 0;
		  UT_UCS4Char c = *pStart;
	
		  cType = UT_bidiGetCharType(c);
	
		  for(UT_uint32 i = 0; i < iLen; i++)
		  {
			  if(i < iLen - 1 )
			  {
				  c = *(pStart+i+1);
				  cNextType = UT_bidiGetCharType(c);
			  }
			  else
			  {
				  cNextType = UT_BIDI_UNSET;
			  }

			  if(UT_BIDI_IS_NEUTRAL(cType))
			  {
				  if(m_bLTRCharContext
					 && iOverride != UT_BIDI_LTR
					 && (cLastType != UT_BIDI_LTR || cNextType != UT_BIDI_LTR))
				  {
					  if(i - iLast > 0)
					  {
						  p = pStart + iLast;
						  if(!_appendFmt(propsArray))
							  return;

						  if(!_appendSpan(p, i - iLast))
							  return;
					  }
					  iOverride = UT_BIDI_LTR;
					  propsArray[1] = prop_ltr.c_str();
					  iLast = i;
				  }
				  else if(!m_bLTRCharContext
						  && iOverride != UT_BIDI_RTL
						  && (cLastType != UT_BIDI_RTL || cNextType != UT_BIDI_RTL))
				  {
					  if(i - iLast > 0)
					  {
						  p = pStart + iLast;
						  if(!_appendFmt(propsArray))
							  return;

						  if(!_appendSpan(p, i - iLast))
							  return;
					  }
					  iOverride = UT_BIDI_RTL;
					  propsArray[1] = prop_rtl.c_str();
					  iLast = i;
				  }
			  }
			  else
			  {
				  // strong character; if we previously issued an override,
				  // we need to cancel it
				  if(iOverride != static_cast<UT_uint32>(UT_BIDI_UNSET))
				  {
					  if(i - iLast > 0)
					  {
						  p = pStart + iLast;
						  if(!_appendFmt(propsArray))
							  return;
					
						  if(!_appendSpan(p, i - iLast))
							  return;
					  }
					  iOverride = UT_BIDI_UNSET;
					  propsArray[1] = prop_basic.c_str();
					  iLast = i;
				  }
			  }

			  cLastType = cType;
			  cType = cNextType;
		  }

		  // insert what is left over
		  if(iLen - iLast > 0)
		  {
			  p = pStart + iLast;
			  if(!_appendFmt(propsArray))
				  return;
					
			  if(!_appendSpan(p, iLen - iLast))
				  return;
		  }
	  }
	  else
	  {
		  // non-bidi document, just do it the easy way
		  if (!_appendSpan(m_pTextRun.ucs4_str(), m_pTextRun.size()))
		  {
			  UT_DEBUGMSG(("DOM: error appending text run\n"));
			  return;
		  }
	  }
	  
	  m_pTextRun.clear();
  }
}

void IE_Imp_MsWord_97::_appendChar (UT_UCS4Char ch)
{
  if (m_bInTable) {
    /* merge-covered cell slots (TCGRF.horzMerge==1 / vertical
       continuations) hold no content in MS-DOC; anything present is
       pathological and would flush into an orphan block at table
       level, so drop it */
    MsTableCtx * ctx = _curTableCtx();
    if (ctx && ctx->bCoveredCell)
      return;

    switch (ch) {
    case 7:			// eat tab characters
      return;
    case 30:		// ??
      ch = '-';
		  break;
    }
  }

  if ( m_bIsLower )
    ch = UT_UCS4_tolower ( ch );
  m_pTextRun += ch;
}

/****************************************************************************/
/****************************************************************************/

static int s_cmp_bookmarks_qsort(const void * a, const void * b)
{
	const bookmark * A = static_cast<const bookmark *>(a);
	const bookmark * B = static_cast<const bookmark *>(b);

	if(A->pos != B->pos)
		return (A->pos - B->pos);
	else
		// for bookmarks with identical position we want any start bookmarks to be
		// before end bookmarks.
		return static_cast<UT_sint32>(B->start) - static_cast<UT_sint32>(A->start);
}



gchar * IE_Imp_MsWord_97::_getBookmarkName(const wvParseStruct * ps, UT_uint32 pos)
{
	gchar *str;
	UT_UTF8String sUTF8;

	// a malformed document can declare more bookmarks than the STTBF
	// actually contains
	if(pos >= ps->Sttbfbkmk.nostrings)
		return nullptr;

	if(ps->Sttbfbkmk.extendedflag == 0xFFFF)
	{
		// 16 bit stuff
		if(!ps->Sttbfbkmk.u16strings)
			return nullptr;

		const UT_UCS2Char * p = static_cast<const UT_UCS2Char *>(ps->Sttbfbkmk.u16strings[pos]);
		if(p) {
		  UT_uint32 len  = UT_UCS2_strlen(p);
		  sUTF8.clear();
		  sUTF8.appendUCS2(p, len);
		  
		  str = new gchar[sUTF8.byteLength()+1];
		  strcpy(str, sUTF8.utf8_str());
		} else
		  str = nullptr;
	}
	else
	{
		// 8 bit stuff
		// there is a bug in wv, and the table gets incorrectly retrieved
		// if it contains 8-bit strings
		if(ps->Sttbfbkmk.s8strings && ps->Sttbfbkmk.s8strings[pos])
		{
			UT_uint32 len = strlen(ps->Sttbfbkmk.s8strings[pos]);
			str = new gchar[len + 1];
			UT_uint32 i = 0;
			for(i = 0; i < len; i++)
				str[i] = ps->Sttbfbkmk.s8strings[pos][i];
			str[i] = 0;
		}
		else
			str = nullptr;
	}
	
	return str;
}

int IE_Imp_MsWord_97::_docProc (wvParseStruct * ps, UT_uint32 tag)
{
	// flush out any pending character data
	this->_flush ();

	switch (static_cast<wvTag>(tag))
	{
	case DOCBEGIN:

		// test the bidi nature of this document
#ifdef BIDI_DEBUG
		m_bBidiMode = wvIsBidiDocument(ps);
		UT_DEBUGMSG(("IE_Imp_MsWord_97::_docProc: complex %d, bidi %d\n",
					 ps->fib.fComplex,m_bBidiMode));
#else
		// for now we will assume that all documents are bidi
		// documents (Tomas, Apr 12, 2003)
		
		m_bBidiMode = false;
#endif

		m_bEvenOddHeaders = (ps->dop.fFacingPages != 0);
		
		// import styles
		_handleStyleSheet(ps);

		if(getLoadStylesOnly())
			return 1;

		// deal with bookmarks
		m_iBmCursor = 0;
		_handleBookmarks(ps);

		// deal with footnotes and endnotes, headers
		// first, get the doc offsets of the foot/endnote text
		// (We are interested in the offset of this in the document,
		// not in the data stream; therefore, we do not add
		// ps->fib.fcMin for the simple doc
		// Tthere are some strange docs around that have invalid
		// values for the end of endnote section (e.g. the doc from
		// bug 3283); that's what the if's are about.
		m_iTextStart      = 0;
		m_iTextEnd        = ps->fib.ccpText;
		if(m_iTextEnd == 0xffffffff)
			m_iTextEnd = m_iTextStart;
		
		m_iFootnotesStart = m_iTextEnd;
		m_iFootnotesEnd   = m_iFootnotesStart + ps->fib.ccpFtn;
		if(m_iFootnotesEnd == 0xffffffff)
			m_iFootnotesEnd = m_iFootnotesStart;

		m_iHeadersStart   = m_iFootnotesEnd;
		m_iHeadersEnd     = m_iHeadersStart + ps->fib.ccpHdr;
		if(m_iHeadersEnd == 0xffffffff)
			m_iHeadersEnd = m_iHeadersStart;

		m_iMacrosStart    = m_iHeadersEnd;
		m_iMacrosEnd      = m_iMacrosStart + ps->fib.ccpMcr;
		if(m_iMacrosEnd == 0xffffffff)
			m_iMacrosEnd = m_iMacrosStart;

		m_iAnnotationsStart = m_iMacrosEnd;
		m_iAnnotationsEnd = m_iAnnotationsStart + ps->fib.ccpAtn;
		if(m_iAnnotationsEnd == 0xffffffff)
			m_iAnnotationsEnd = m_iAnnotationsStart;

		m_iEndnotesStart  = m_iAnnotationsEnd;
		m_iEndnotesEnd    = m_iEndnotesStart + ps->fib.ccpEdn;
		if(m_iEndnotesEnd == 0xffffffff)
			m_iEndnotesEnd = m_iEndnotesStart;
		
		m_iTextboxesStart = m_iEndnotesEnd;
		m_iTextboxesEnd = m_iTextboxesStart + ps->fib.ccpTxbx;
		UT_DEBUGMSG(("Size of all text in all textboxes %d \n", ps->fib.ccpTxbx));

		if(m_iTextboxesEnd == 0xffffffff)
			m_iTextboxesEnd = m_iTextboxesStart;
		UT_DEBUGMSG(("  Found %d Positioned TextBoxes \n",ps->nooffspa));
		// now retrieve the note info ...
		_handleNotes(ps);
		_handleAnnotations(ps);
		_handleHeaders(ps);
		_handleTextBoxes(ps);

		// per-story field character tables (needed to resolve the
		// _PID_HLINKS dwApp values to fields, MS-DOC 2.4.7)
		_handleFields(ps);
		
		if(m_iAnnotationsEnd != m_iAnnotationsStart)
			{
				UT_DEBUGMSG(("Annotations of length %d in this doc \n",m_iAnnotationsEnd - m_iAnnotationsStart));
			}
		UT_DEBUGMSG(("Fnotes [%d,%d], Enotes [%d,%d]\n",
					 m_iFootnotesStart, m_iFootnotesEnd, m_iEndnotesStart, m_iEndnotesEnd));

		///////////////////////////////////////////////////////////////////////////////
		// Set various revision states
		//
		// unlike Word:
		// 
		//     * we do not differentiate between screen and print: we
		//       print whatever is on screen
		//
		//     * if show revisions is off, Word shows what the
		//       document looks like _after_ the last revision; by
		//       default we show what it looked _before_ first
		//       revision; we can show the post-revision state by
		//       setting the view id to PD_MAX_REVISION
		//
		//     * we currently do not handle the fLockRev parameter
		{
			bool bShow = ps->dop.fRMView == 1 || ps->dop.fRMPrint == 1;
		
			getDoc()->setShowRevisions(bShow);

			if(!bShow)
			{
				getDoc()->setShowRevisionId(PD_MAX_REVISION);
			}
		
			getDoc()->setMarkRevisions(ps->dop.fRevMarking == 1);
		}
		
		break;
		
	case DOCEND:
		// we want to clean up fmt marks
		getDoc()->purgeFmtMarks();
		// drain bookmarks whose CPs were never reached (e.g. a bookmark
		// ending exactly at the end of the document)
		while(m_pBookmarks && m_iBmCursor < m_iBookmarksCount)
			_insertBookmark(&m_pBookmarks[m_iBmCursor++]);
		// close any annotation anchors left dangling, e.g. a comment
		// range that ran to (or past) the last imported CP
		while(!m_vecAnnOpen.empty())
		{
			m_vecAnnOpen.pop_back();
			_appendObject(PTO_Annotation, PP_NOPROPS);
		}
		break;
	default:
		break;
	}

	return 0;
}

bool IE_Imp_MsWord_97::_insertBookmark(bookmark * bm)
{
	// first of all flush what is in the buffers
	this->_flush();
	bool error = false;
	UT_return_val_if_fail(bm, false);

	PP_PropertyVector propsArray = {
		"name", (bm->name ? bm->name : ""),
		"type", bm->start ? "start" : "end"
	};

	if(m_bInTable && !(_curTableCtx() && _curTableCtx()->bCellOpen))
	{
		emObject * pObject = new emObject;
		pObject->props1 = propsArray[1];
		pObject->objType = PTO_Bookmark;
		pObject->props2 = propsArray[3];
		m_vecEmObjects.addItem(pObject);
	}
	else
	{
//
// Bookmarks need to be preceded by Blocks
//
		pf_Frag * pf = getDoc()->getLastFrag();
		while(pf && pf->getType() != pf_Frag::PFT_Strux)
		{
			pf = pf->getPrev();
		}
		if(pf && (pf->getType() == pf_Frag::PFT_Strux) )
		{
			pf_Frag_Strux * pfs = static_cast<pf_Frag_Strux *>(pf);
			if(pfs->getStruxType() != PTX_Block)
			{
				getDoc()->appendStrux(PTX_Block, PP_NOPROPS);
			}
		}
		else if( pf == nullptr)
		{
			getDoc()->appendStrux(PTX_Block, PP_NOPROPS);
		}

		if (!_appendObject (PTO_Bookmark, propsArray))
		{
			UT_DEBUGMSG (("Could not append bookmark object\n"));
			error = true;
		}
	}
	return error;
}

bool IE_Imp_MsWord_97::_insertBookmarkIfAppropriate(UT_uint32 iDocPosition)
{
	// the bookmark array is sorted by position (start bookmarks before
	// end bookmarks at the same position); document positions increase
	// monotonically during import, so a simple cursor suffices. Using
	// <= rather than == means a bookmark whose CP fell inside a field
	// (skipped while ps->fieldstate was set) is still inserted at the
	// first character after the field instead of being dropped.
	bool error = false;
	while(m_pBookmarks && m_iBmCursor < m_iBookmarksCount &&
		  m_pBookmarks[m_iBmCursor].pos <= iDocPosition)
	{
		error |= _insertBookmark(&m_pBookmarks[m_iBmCursor++]);
	}
	return error;
}

int IE_Imp_MsWord_97::_charProc (wvParseStruct *ps, U16 eachchar, U8 chartype, U16 lid)
{
	// make sure we are not past the end of the document ...
	// this can happen with some complex documents
	if(ps->currentcp >= m_iTextboxesEnd)
	{
		UT_DEBUGMSG(("IE_Imp_MsWord_97::_charProc: processing past end of document !!! %d \n",ps->currentcp ));
		return 0;
	}       
	
	// reset the page break tracker
	if(m_bPageBreakPending)
	{
		// we have a page break pending, and being here means that it
		// was not a seciton break; we have to append it first and
		// then continue normal processing
		this->_appendChar (UCS_FF);
		m_bPageBreakPending = false;
	}

	// reset the page break tracker
	if(m_bLineBreakPending)
	{
		// we have a line break pending
		this->_appendChar (UCS_LF);
		m_bLineBreakPending = false;
	}
	
	if(!_handleHeadersText(ps->currentcp,true))
		return 0;
	if(!_handleNotesText(ps->currentcp))
		return 0;
	if(!_handleAnnotationsText(ps->currentcp,eachchar))
		return 0;
	if(!_handleTextboxesText(ps->currentcp,eachchar))
		return 0;

	// insert any required bookmarks, but only if we are not in a
	// field ...
	if(!ps->fieldstate)
		_insertBookmarkIfAppropriate(ps->currentcp);

	if(_insertNoteIfAppropriate(ps->currentcp,eachchar))
		return 0;

	if(_insertAnnotationIfAppropriate(ps->currentcp))
		return 0;

	// convert incoming character to unicode
	if (chartype)
		eachchar = wvHandleCodePage(eachchar, lid);

	switch (eachchar)
	{

	case 11: // forced line break
		eachchar = UCS_LF;
		break;

	case 12: // page or section break
		this->_flush ();
		//eachchar = UCS_FF;
		// we will not append page breaks to the buffer, only mark it
		// as pending append; that will allow us later to decide if we
		// should or should not appended (we want to remove any page
		// break that is at an end of a section
		m_bPageBreakPending = true;
		return 0;

	case 13: // end of paragraph
	  this->_flush();
	  // see bug 9370
	  // <delackner> aaah actually, Cocoa's writer is *definitely* broken
	  // <delackner> ms word thinks the second para is part of the first, but broken with a non-paragraph-breaking-line-break
	  // so we'll treat this like msword does
	  m_bLineBreakPending = true;
	  return 0;

	case 14: // column break
		eachchar = UCS_VTAB;
		break;

	case 19: // field begin
		this->_flush ();
		ps->fieldstate++;
		ps->fieldmiddle = 0;
		this->_fieldProc (ps, eachchar, chartype, lid);
		return 0;

	case 20: // field separator; some docs have spurious 0x14's in
			 // them, see bug 3745
		if (ps->fieldstate)
		{
			this->_fieldProc (ps, eachchar, chartype, lid);
			ps->fieldmiddle = 1;
		}
		return 0;

	case 21: // field end
		if (ps->fieldstate)
		{
			ps->fieldstate--;
			ps->fieldmiddle = 0;
			this->_fieldProc (ps, eachchar, chartype, lid);
		}
		return 0;
	}

	// i'm not sure if this is needed any more
	// yes, it is, for instance hyperlinks need it
	if (ps->fieldstate)
	{
		xxx_UT_DEBUGMSG(("DOM: fieldstate\n"));
		if(this->_fieldProc (ps, eachchar, chartype, lid))
		{
			return 0;
		}
	}

	// take care of any oddities in Microsoft's character encoding
	if (chartype == 1 && eachchar == 146)
		eachchar = 39; // apostrophe

	if(m_bSymbolFont)
	{
		eachchar &= 0x00ff;
	}

	// see bug 9370. we probably got a char 13, but no open paragraph.
	if(!m_bInPara) {
	  this->_appendChar (UCS_LF);
	  _flush();
	}
	
	this->_appendChar (static_cast<UT_UCS4Char>(eachchar));

	return 0;
}

/*! fetch a simple (non-complex) property from an OfficeArtRGFOPTE
 *  (MS-ODRAW 2.2.7); the table is terminated by a zero pid */
static bool s_getOPTProp (const FOPTE * fopte, U32 pid, U32 & val)
{
	if(!fopte)
		return false;
	for(const FOPTE * f = fopte; f->pid; ++f)
	{
		if(f->pid == pid && !f->fComplex)
		{
			val = f->op;
			return true;
		}
	}
	return false;
}

/*! convert an MSOCLR to a "rrggbb" string; only plain RGB colors
 *  (high byte 0) can be resolved without the document color
 *  scheme */
static bool s_msoclrToRGB (U32 clr, UT_String & rgb)
{
	if(clr & 0xff000000)
		return false;
	UT_String_sprintf(rgb, "%02x%02x%02x",
					  clr & 0xff, (clr >> 8) & 0xff, (clr >> 16) & 0xff);
	return true;
}

/*! append "name:<dInches>in; " to sProps; dimensions are formatted
 *  under the C locale so the prop stays locale-independent */
static void s_appendInchProp (std::string & sProps, const char * szName,
							  double dInches)
{
	sProps += szName;
	sProps += ':';
	sProps += UT_convertInchesToDimensionString(DIM_IN, dInches);
	sProps += "; ";
}

int IE_Imp_MsWord_97::_specCharProc (wvParseStruct *ps, U16 eachchar, CHP *achp)
{
	// make sure we are not past the end of the document ...
	// this can happen with some complex documents
	if(ps->currentcp >= m_iTextboxesEnd)
	{
		UT_DEBUGMSG(("IE_Imp_MsWord_97::_specCharProc: processing past end of document !!!\n"));
		return 0;
	}
	
	Blip blip;
	long pos;
	FSPA * fspa;
	//FDOA * fdoa;
#ifdef SUPPORTS_OLD_IMAGES
	wvStream *fil;
	PICF picf;
#endif

	if(!_handleHeadersText(ps->currentcp,true))
		return 0;

	if(!_handleNotesText(ps->currentcp))
		return 0;

	if(!_handleAnnotationsText(ps->currentcp,eachchar))
		return 0;

	if(!_handleTextboxesText(ps->currentcp,eachchar))
		return 0;

	// insert any required bookmarks, but only if we are not in a
	// field ...
	if(!ps->fieldstate)
		_insertBookmarkIfAppropriate(ps->currentcp);

	if(_insertNoteIfAppropriate(ps->currentcp,0))
		return 0;

	if(_insertAnnotationIfAppropriate(ps->currentcp))
		return 0;

	if(eachchar == 0x28)
	{
		// this is a symbol; the font is identified by achp->ftcSym and the char code is
		// achp->xchSym
		this->_appendChar(achp->xchSym);
		return 0;
	}
	
	//
	// This next bit of code is to handle fields
	//

	switch (eachchar)
	{

	case 19: // field begin
		this->_flush ();
		ps->fieldstate++;
		ps->fieldmiddle = 0;
		this->_fieldProc (ps, eachchar, 0, 0x400);
		return 0;

	case 20: // field separator; ignore spurious ones outside a field
		if (ps->fieldstate)
		{
			if (achp->fOle2)
			{
				UT_DEBUGMSG(("Field has an associated embedded OLE object\n"));
			}
			ps->fieldmiddle = 1;
			this->_fieldProc (ps, eachchar, 0, 0x400);
		}
		return 0;

	case 21: // field end; a stray 0x15 must not underflow fieldstate
		if (ps->fieldstate)
		{
			ps->fieldstate--;
			ps->fieldmiddle = 0;
			this->_fieldProc (ps, eachchar, 0, 0x400);
		}
		return 0;

	}

	/* it seems some fields characters slip through here which tricks
	 * the import into thinking it has an image with it really does
	 * not. this catches special characters in a field
	 */
	if (ps->fieldstate) {
		if (this->_fieldProc(ps, eachchar, 0, 0x400))
			return 0;
	}

	//
	// This next bit of code is to handle OLE2 embedded objects and images
	//

	switch (eachchar)
	{
	case 0x01: // Older ( < Word97) image, currently not handled very well
		if (achp->fOle2) {
			UT_DEBUGMSG(("embedded OLE2 component. currently unsupported"));
			return 0;
		}

#ifdef SUPPORTS_OLD_IMAGES
		UT_DEBUGMSG(("Pre W97 Image format.\n"));
		// sprmCPicLocation points at a PICF in the Data stream;
		// reject offsets that can't possibly hold one. fcPic is
		// S32: check < 0 explicitly so a negative offset can't
		// wrap huge against the unsigned stream size.
		if (ps->data == nullptr || achp->fcPic_fcObj_lTagObj < 0 ||
			static_cast<U32>(achp->fcPic_fcObj_lTagObj) >= wvStream_size(ps->data))
		{
			UT_DEBUGMSG(("Bogus fcPic %d\n", achp->fcPic_fcObj_lTagObj));
			return 0;
		}
		pos = wvStream_tell(ps->data);
		wvStream_goto(ps->data, achp->fcPic_fcObj_lTagObj);

		if (1 == wvGetPICF(wvQuerySupported(&ps->fib, nullptr), &picf,
						   ps->data) && nullptr != picf.rgb)
		{
			fil = picf.rgb;

			if (wv0x01(&blip, fil, wvStream_size(fil), ps->data))
			{
				this->_handleImage(&blip, picf.mx * picf.dxaGoal / 1000, picf.my * picf.dyaGoal / 1000, picf.dyaCropTop, picf.dyaCropBottom, picf.dxaCropLeft, picf.dxaCropRight);
			}
			else
			{
				UT_DEBUGMSG(("Dom: no graphic data\n"));
			}

			wvStream_goto(ps->data, pos);

			return 0;
		}
		else
		{
			UT_DEBUGMSG(("Couldn't import graphic!\n"));
			wvStream_goto(ps->data, pos);
			return 0;
		}
#else
		UT_DEBUGMSG(("DOM: 0x01 graphics support is disabled at the moment\n"));
		return 0;
#endif
		break;
	case 0x08: // Word 97, 2000, XP image
		if (wvQuerySupported(&ps->fib, nullptr) >= WORD8) // sanity check
		{
			if (ps->nooffspa > 0)
			{

				fspa = wvGetFSPAFromCP(ps->currentcp, ps->fspa,
									   ps->fspapos, ps->nooffspa);

				if(!fspa)
				{
					UT_DEBUGMSG(("No fspa! Panic and Insanity Abounds!\n"));
					return 0;
				}
				UT_DEBUGMSG(("Found a psfa! \n"));
				double dLeft,dRight,dTop,dBottom = 0.0;
				dLeft = static_cast<double>(fspa->xaLeft)/1440.0;
				dRight = static_cast<double>(fspa->xaRight)/1440.0;
				dTop = static_cast<double>(fspa->yaTop)/1440.0;
				dBottom = static_cast<double>(fspa->yaBottom)/1440.0;
				UT_DEBUGMSG(("Left %f Right %f Top %f Bottom %f \n",dLeft,dRight,dTop,dBottom));
				UT_DEBUGMSG(("spid %d cTxbx %d \n",fspa->spid,fspa->cTxbx));
				UT_DEBUGMSG(("fHdr %d bx %d by %d wr %d wrk %d fRcaSimple %d fBelowText %d fAnchorLock %d \n",fspa->fHdr,fspa->bx,fspa->by,fspa->wr,fspa->wrk,fspa->fRcaSimple,fspa->fBelowText,fspa->fAnchorLock));
				UT_String sImageName;
				bool bPositionObject = false;
				if (wv0x08(&blip, fspa->spid, ps))
				{
//
// FIXME! Put some code in here to make this use Sectionframes!!
//
					UT_DEBUGMSG(("!!!!Found a blip in a fspa!!!!!!!!!! \n"));
					if(UT_OK == this->_handlePositionedImage(&blip, sImageName))
					   bPositionObject = true;
				}
				bool isTextBox = false;
				UT_uint32 i;
				escherstruct item;
				FSPContainer *answer = nullptr;

				UT_DEBUGMSG(("IE_Imp_MsWord_97:: escher: ps->fib.fcDggInfo %d ps->fib.lcbDggInfo %d \n", ps->fib.fcDggInfo,ps->fib.lcbDggInfo));
				wvGetEscher (&item, ps->fib.fcDggInfo, ps->fib.lcbDggInfo, ps->tablefd,
							 ps->data);
				for (i = 0; i < item.dgcontainer.no_spgrcontainer; i++)
				{
					answer = wvFindSPID (&(item.dgcontainer.spgrcontainer[i]), fspa->spid);
					if (answer)
					{
						break;
					}
				}
				/* ungrouped shapes are direct children of the
				 * drawing container, not of a shape group */
				if(!answer)
				{
					for (i = 0; i < item.dgcontainer.no_spcontainer; i++)
					{
						if(item.dgcontainer.spcontainer[i].fsp.spid == static_cast<U32>( fspa->spid))
						{
							answer = &item.dgcontainer.spcontainer[i];
							break;
						}
					}
				}
				if(answer != nullptr)
				{
					ClientTextbox cTextBox = answer->clienttextbox;
					if(cTextBox.textid != nullptr)
					{
						isTextBox = true;
						UT_DEBUGMSG(("Found a Text box! text id is %d \n",*cTextBox.textid));
					}
				}
				/* OfficeArtFSP.grfPersistent (MS-ODRAW 2.2.40):
				 * fDelete 0x0008, fOleShape 0x0010, fFlipH 0x0040,
				 * fFlipV 0x0080, fConnector 0x0100,
				 * fBackground 0x0400; deleted and background-part
				 * shapes carry no document content, do not emit
				 * frames for them */
				const U32 grfPersistent = answer ? answer->fsp.grfPersistent : 0;
				if((isTextBox || bPositionObject || answer != nullptr)
				   && !(grfPersistent & 0x408))
				{
					const char * atts[] = {nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
					if(bPositionObject && sImageName.size())
					{
					  atts[0] =  PT_STRUX_IMAGE_DATAID;
					  atts[1] = sImageName.c_str();
					  atts[2] = "props";
					}
					else
					{
					  atts[0] = "props";
					}
					std::string sProp;
					std::string sProps;
					std::string sVal;
					sProps = "frame-type:";
					if(bPositionObject)
					{
					  sProps += "image; ";
					}
					else
					{
					  sProps += "textbox; ";
					}
					sProps += "position-to:";
					/* Spa.bx/by coordinate origins (MS-DOC 2.9.x SPA):
					 * 0 = page margin, 1 = page edge, 2 = column
					 * edge (bx) / paragraph top (by); only a
					 * paragraph vertical anchor maps to our
					 * block-relative frame type, everything else is
					 * positioned on the page */
					if(fspa->by == 2)
					{
						sVal = "block-above-text; ";
					}
					else
					{
						sVal = "page-above-text; ";
					}
					sProps += sVal;
					/* Spa.wr/wrk: 0 wrap both sides, 1 top/bottom
					 * only, 2 square, 3 float above or below text
					 * (fBelowText selects), 4/5 tight/through;
					 * wrk refines the side text wraps on */
					sProps += "wrap-mode:";
					if(fspa->wr == 1)
					{
						sVal = "wrapped-topbot; ";
					}
					else if(fspa->wr == 3)
					{
						sVal = fspa->fBelowText ? "below-text; " : "above-text; ";
					}
					else
					{
						if(fspa->wrk == 1)
						{
							sVal = "wrapped-to-left; ";
						}
						else if(fspa->wrk == 2)
						{
							sVal = "wrapped-to-right; ";
						}
						else
						{
							sVal = "wrapped-both; ";
						}
						if(fspa->wr == 4 || fspa->wr == 5)
						{
							sProps += "tight-wrap:1; ";
						}
					}
					sProps += sVal;

					/* resolve margin- and column-relative origins to
					 * the absolute coordinate systems the frame
					 * layout uses; the paragraph anchor keeps its
					 * recorded offsets */
					const double dColX = (fspa->bx == 1) ? dLeft - m_dSectMarginLeft : dLeft;
					const double dColY = (fspa->by == 1) ? dTop - m_dSectMarginTop : dTop;
					double dPageX = (fspa->bx == 1) ? dLeft : dLeft + m_dSectMarginLeft;
					double dPageY = (fspa->by == 1) ? dTop : dTop + m_dSectMarginTop;
					if(dPageX < 0.0)
					{
						dPageX = 0.0;
					}
					if(dPageY < 0.0)
					{
						dPageY = 0.0;
					}

					s_appendInchProp(sProps, "xpos", dColX);
					s_appendInchProp(sProps, "ypos", dTop);
					s_appendInchProp(sProps, "frame-col-xpos", dColX);
					s_appendInchProp(sProps, "frame-col-ypos", dColY);
					s_appendInchProp(sProps, "frame-page-xpos", dPageX);
					s_appendInchProp(sProps, "frame-page-ypos", dPageY);

					s_appendInchProp(sProps, "frame-width", dRight-dLeft);
					s_appendInchProp(sProps, "frame-height", dBottom-dTop);
					sProps.resize(sProps.size() - 2); // drop trailing "; "
					if(grfPersistent & 0x40)
					{
						sProps += "; frame-flip-horiz:1";
					}
					if(grfPersistent & 0x80)
					{
						sProps += "; frame-flip-vert:1";
					}

					// OfficeArt shape properties (MS-ODRAW 2.3)
					bool bFilled = true;
					bool bLined = true;
					bool bLineDefined = false;
					UT_String sFillClr;
					UT_String sLineClr;
					double dLineWidthPt = 0.75;
					const char * pszLineStyle = nullptr;
					if(answer && answer->fopte)
					{
						U32 v = 0;
						/* rotation is a 16.16 fixed point angle in
						 * degrees */
						if(s_getOPTProp(answer->fopte, rotation, v) && v)
						{
							double dRot = static_cast<S32>(v) / 65536.0;
							if(dRot < 0.0)
							{
								dRot += 360.0;
							}
							sProps += "; frame-rotation:";
							sProps += UT_convertToDimensionlessString(dRot, ".4");
						}
						// text inset margins, EMU
						if(s_getOPTProp(answer->fopte, dxTextLeft, v))
						{
							sProps += "; xpad-left:";
							sProps += UT_convertInchesToDimensionString(
								DIM_IN, v / 914400.0);
						}
						if(s_getOPTProp(answer->fopte, dxTextRight, v))
						{
							sProps += "; xpad-right:";
							sProps += UT_convertInchesToDimensionString(
								DIM_IN, v / 914400.0);
						}
						if(s_getOPTProp(answer->fopte, dyTextTop, v))
						{
							sProps += "; ypad-top:";
							sProps += UT_convertInchesToDimensionString(
								DIM_IN, v / 914400.0);
						}
						if(s_getOPTProp(answer->fopte, dyTextBottom, v))
						{
							sProps += "; ypad-bottom:";
							sProps += UT_convertInchesToDimensionString(
								DIM_IN, v / 914400.0);
						}
						// text anchor (MSOANCHOR)
						if(s_getOPTProp(answer->fopte, anchorText, v))
						{
							if(v == 1 || v == 4)
							{
								sProps += "; frame-valign:middle";
							}
							else if(v != 0 && v != 3 && v != 6)
							{
								sProps += "; frame-valign:bottom";
							}
						}
						// text flow (MSOTXFL)
						if(s_getOPTProp(answer->fopte, txflTextFlow, v))
						{
							if(v == 2)
							{
								sProps += "; frame-text-direction:vert270";
							}
							else if(v == 1 || v == 3 || v == 5)
							{
								sProps += "; frame-text-direction:vert";
							}
						}
						/* the Fill/Line Style Boolean Properties
						 * records carry the real flags in their low
						 * bits: fFilled 0x10 of fNoFillHitTest
						 * (pid 447), fLine 0x08 of fNoLineDrawDash
						 * (pid 511) */
						if(s_getOPTProp(answer->fopte, fNoFillHitTest, v))
						{
							bFilled = (v & 0x10) != 0;
						}
						if(s_getOPTProp(answer->fopte, fNoLineDrawDash, v))
						{
							bLined = (v & 0x08) != 0;
							bLineDefined = true;
						}
						if(s_getOPTProp(answer->fopte, fillColor, v))
						{
							s_msoclrToRGB(v, sFillClr);
						}
						if(s_getOPTProp(answer->fopte, fillOpacity, v))
						{
							sProps += "; fill-alpha:";
							sProps += UT_convertToDimensionlessString(
								v / 65536.0, ".4");
						}
						if(s_getOPTProp(answer->fopte, lineColor, v))
						{
							s_msoclrToRGB(v, sLineClr);
						}
						if(s_getOPTProp(answer->fopte, lineWidth, v))
						{
							dLineWidthPt = static_cast<S32>(v) / 12700.0;
							bLineDefined = true;
						}
						if(s_getOPTProp(answer->fopte, lineDashing, v))
						{
							/* MSOLINEDASHING: 0 solid, 1/5/6 dotted,
							 * anything else dashed */
							if(v == 0)
							{
								pszLineStyle = "solid";
							}
							else if(v == 1 || v == 5 || v == 6)
							{
								pszLineStyle = "dotted";
							}
							else
							{
								pszLineStyle = "dashed";
							}
							bLineDefined = true;
						}
						if(s_getOPTProp(answer->fopte, lineStyle, v) && v == 1)
						{
							pszLineStyle = "double";
							bLineDefined = true;
						}
					}

					// fills: Word text boxes default to a white fill
					if(bFilled && !bPositionObject)
					{
						sProp = "background-color";
						sVal = sFillClr.size() ? sFillClr.c_str() : "ffffff";
						UT_std_string_setProperty(sProps, sProp, sVal);
					}
					else if(!bFilled)
					{
						sProp = "bg-style";
						sVal = "0";    /* no background */
						UT_std_string_setProperty(sProps, sProp, sVal);
						sProp = "background-color";
						sVal = "transparent";
						UT_std_string_setProperty(sProps, sProp, sVal);
					}

					/* outlines: Word text boxes default to a
					 * 0.75 pt solid black border; positioned images
					 * carry no border unless the shape asks for
					 * one */
					if(!bLined || (bPositionObject && !bLineDefined))
					{
						pszLineStyle = "none";
					}
					if(pszLineStyle == nullptr)
					{
						pszLineStyle = "solid";
					}
					static const char * const sSides[] =
						{"top", "right", "left", "bot"};
					for(UT_uint32 s = 0; s < G_N_ELEMENTS(sSides); s++)
					{
						sProp = sSides[s];
						sProp += "-style";
						sVal = pszLineStyle;
						UT_std_string_setProperty(sProps, sProp, sVal);
						if(strcmp(pszLineStyle, "none") != 0)
						{
							sProp = sSides[s];
							sProp += "-thickness";
							sVal = UT_formatDimensionedValue(dLineWidthPt,
															 "pt", ".2");
							UT_std_string_setProperty(sProps, sProp, sVal);
							if(sLineClr.size())
							{
								sProp = sSides[s];
								sProp += "-color";
								sVal = sLineClr.c_str();
								UT_std_string_setProperty(sProps, sProp, sVal);
							}
						}
					}
					if(bPositionObject)
					{
					  atts[3] = sProps.c_str();
					}
					else
					{
					  atts[1] = sProps.c_str();
					}
					PP_PropertyVector vatts = PP_std_copyProps(atts);
					_appendStrux(PTX_SectionFrame, vatts);
					/* a frame with no content block breaks the
					 * surrounding frame chain (same rule as the
					 * OOXML importer); the textbox story fills
					 * this block in via insert-before-EndFrame */
					_appendStrux(PTX_Block, PP_NOPROPS);
					_appendStrux(PTX_EndFrame, vatts);
					if(isTextBox)
					{
					  textboxPos * pPos = new textboxPos;
					  pPos->lid = fspa->spid;
					  PT_DocPosition posEnd =0;
					  getDoc()->getBounds(true,posEnd); // clean frags!

					  pPos->endFrame = getDoc()->getLastFrag();
					  m_vecTextboxPos.addItem(pPos);
					}
					wvReleaseEscher (&item);
					return true;
				}
				wvReleaseEscher (&item);
			}
			else
			{
				xxx_UT_DEBUGMSG(("nooffspa was <= 0 -- ignoring"));
			}
		}
		else
		{
			UT_DEBUGMSG(("pre Word8 0x08 graphic -- unsupported at the moment"));
			/*fdoa =*/ wvGetFDOAFromCP(ps->currentcp, nullptr, ps->fdoapos,
								   ps->nooffdoa);

			// TODO: do something with the data in this fdoa someday...
		}

		return 0;
	}

	return 0;
}

int IE_Imp_MsWord_97::_beginComment(wvParseStruct * /*ps*/, UT_uint32 /*tag*/,
					void * /*props*/, int /*dirty*/)
{
  UT_DEBUGMSG(("DOM: begin comment\n"));
  return 0;
}

int IE_Imp_MsWord_97::_endComment(wvParseStruct * /*ps*/, UT_uint32 /*tag*/,
				  void * /*props*/, int /*dirty*/)
{
  UT_DEBUGMSG(("DOM: end comment\n"));
  return 0;
}


int IE_Imp_MsWord_97::_eleProc(wvParseStruct *ps, UT_uint32 tag,
							   void *props, int dirty)
{
	// make sure we are not past the end of the document ...
	// this can happen with some complex documents
	if(ps->currentcp >= m_iTextboxesEnd)
	{
		UT_DEBUGMSG(("IE_Imp_MsWord_97::_eleProc: processing past end of document !!! %d \n",ps->currentcp >= m_iTextboxesEnd));
		return 0;
	}
	
	//
	// Marshall these off to the correct handlers
	//

	switch (static_cast<wvTag>(tag))
	{

	case SECTIONBEGIN:
		return _beginSect (ps, tag, props, dirty);

	case SECTIONEND:
		return _endSect (ps, tag, props, dirty);

	case PARABEGIN:
		return _beginPara (ps, tag, props, dirty);

	case PARAEND:
		return _endPara (ps, tag, props, dirty);

	case CHARPROPBEGIN:
		return _beginChar (ps, tag, props, dirty);

	case CHARPROPEND:
		return _endChar (ps, tag, props, dirty);

	case COMMENTBEGIN:
	  return _beginComment (ps, tag, props, dirty);

	case COMMENTEND:
	  return _endComment (ps, tag, props, dirty);

	default:
	  UT_ASSERT_NOT_REACHED();

	}

	return 0;
}

/****************************************************************************/
/****************************************************************************/

/*! map a Word97+ brcType code to the OOXML ST_Border token used by the
    page-border-* section properties (MS-DOC BrcType 2.9.22) */
static const char *
s_mapBrcTypeToOoxml (UT_sint32 brcType)
{
	switch (brcType)
	{
		case 1:  return "single";
		case 3:  return "double";
		case 5:  return "single";		// hairline
		case 6:  return "dotted";
		case 7:  return "dashed";
		case 8:  return "dotDash";
		case 9:  return "dotDotDash";
		case 10: return "triple";
		case 11: return "thinThickSmallGap";
		case 12: return "thickThinSmallGap";
		case 13: return "thinThickThinSmallGap";
		case 14: return "thinThickMediumGap";
		case 15: return "thickThinMediumGap";
		case 16: return "thinThickThinMediumGap";
		case 17: return "thinThickLargeGap";
		case 18: return "thickThinLargeGap";
		case 19: return "thinThickThinLargeGap";
		case 20: return "wave";
		case 21: return "doubleWave";
		case 22: return "dashSmallGap";
		case 23: return "dashDotStroked";
		case 24: return "threeDEmboss";
		case 25: return "threeDEngrave";
		case 26: return "outset";
		case 27: return "inset";
		default: return nullptr;
	}
}

/*! emit one side of a page border as page-border-<side>* props; sets
    *pbHave when the side carries a real border. brcType >= 0x40 is page
    border art which we store under a synthetic ms-art-<n> name */
static void
s_emitPageBorder (UT_String & s, const char * pszSide, const BRC * brc,
				  UT_Dimension dim, bool * pbHave)
{
	UT_String propBuffer;

	if (!brc->brcType)
	{
		return;
	}
	*pbHave = true;

	if (brc->brcType >= 0x40)
	{
		UT_String_sprintf(propBuffer, "%s:art;", pszSide);
		s += propBuffer;
		UT_String_sprintf(propBuffer, "%s-art:ms-art-%d;", pszSide,
						  brc->brcType - 0x40);
		s += propBuffer;
	}
	else
	{
		const char * pszStyle = s_mapBrcTypeToOoxml(brc->brcType);
		UT_String_sprintf(propBuffer, "%s:%s;", pszSide,
						  pszStyle ? pszStyle : "single");
		s += propBuffer;
	}

	UT_String sColor;
	if (brc->fCv)
	{
		if (s_mapColorRefToColor(brc->cv, sColor))
		{
			UT_String_sprintf(propBuffer, "%s-color:%s;", pszSide,
							  sColor.c_str());
			s += propBuffer;
		}
	}
	else if (brc->ico && brc->ico <= 16)
	{
		UT_String_sprintf(propBuffer, "%s-color:%s;", pszSide,
						  sMapIcoToColor(brc->ico, true).c_str());
		s += propBuffer;
	}

	/* dptLineWidth is in 1/8-point increments for brcType < 0x40 (with
	   values < 2 treated as 2), and in whole points for brcType >=
	   0x40 (page-border art) */
	double dPoints;
	if (brc->brcType < 0x40)
	{
		dPoints = (brc->dptLineWidth < 2 ? 2 : brc->dptLineWidth) / 8.0;
	}
	else
	{
		dPoints = brc->dptLineWidth;
	}
	UT_String_sprintf(propBuffer, "%s-thickness:%s;", pszSide,
					  UT_convertInchesToDimensionString(dim, dPoints / 72.0));
	s += propBuffer;

	/* dptSpace is the border distance in points; whether it is measured
	   from the text or the page edge is given by pgbOffsetFrom and
	   emitted once per section */
	if (brc->dptSpace)
	{
		UT_String_sprintf(propBuffer, "%s-space:%s;", pszSide,
						  UT_convertInchesToDimensionString(dim,
											brc->dptSpace / 72.0));
		s += propBuffer;
	}

	if (brc->fShadow)
	{
		UT_String_sprintf(propBuffer, "%s-shadow:1;", pszSide);
		s += propBuffer;
	}
}

int IE_Imp_MsWord_97::_beginSect (wvParseStruct * ps, UT_uint32 /*tag*/,
				  void *prop, int /*dirty*/)
{
	SEP * asep = static_cast <SEP *>(prop);

	const gchar * propsArray[15];  
	UT_String propBuffer;
	UT_String props;

	// flush any character runs
	this->_flush ();

	m_iCurrentSectId++;

	// first we need to deal with page size, because setting page size
	// resets all margins to the AW defaults
	// Sevior: Only do this ONCE!!! Abiword can only handle one page size.
	if(!m_bSetPageSize)
	{
		// all of this data is related to Abi's <pagesize> tag
		m_bSetPageSize = true;
		double page_width  = 0.0;
		double page_height = 0.0;
		double page_scale  = 1.0;

		// dmOrientPage (SBOrientationOperand): 1 = portrait, 2 = landscape
		if (asep->dmOrientPage == 2)
			getDoc()->m_docPageSize.setLandscape ();
		else
			getDoc()->m_docPageSize.setPortrait ();

		page_width = asep->xaPage / 1440.0;
		page_height = asep->yaPage / 1440.0;

		// PROBLEM: there are two separate and independent page sizes
		// given to us, one by the explicit width and height and one
		// by the requested paper size, and we need to decide which
		// one we should follow. There are three scenarios
		//   (1) the explicit size and paper match
		//   (2) the explicit size and paper do not match
		//       (a) the explicit size is the Word default (Letter)
		//       (b) the explicit size is something else than the defaults
		//
		// In case (1) we use the requested paper. Case (2a) happens
		// when the user changes the page size by requesting a
		// different paper size but does not touch the width and
		// height controls -- we use the paper size. Case (2b) happens
		// when the user changes size by the with and height controls;
		// the paper request stored is the one that was in place
		// before the manual adjustment and is no longer valid, so we
		// use the explicit width and height.

		// decide if the explicit width and height are valid, i.e., if
		// they contain the Word defaults the paper request has to be
		// 0 (Letter)
		bool bDoNotUseSize = (asep->xaPage == 12240 &&
							  asep->yaPage == 15840 &&
							  asep->dmPaperReq != 0);
		

		xxx_UT_DEBUGMSG(("DOM: pagesize: landscape: %d, width: %f, height: %f, paper-type: %d\n",
					 asep->dmOrientPage, page_width, page_height, asep->dmPaperReq));

		// map paper to AW page size name string ...
		const char * paper_name = s_mapPageIdToString (asep->dmPaperReq);

		// check if the paper name is valid (i.e., there is a match
		// between the name and the sizes; if not, we use only the sizes
		bool bPaperNameValid = (paper_name != nullptr);
		
		if(bPaperNameValid)
		{
			// construct an instance of fp_PageSize for this paper
			// request; we will use this to verify whether its
			// dimensions match those stored in the explicit width and
			// height but also we will determine appropriate units to
			// be used (i.e., we want to use inches for Letter but
			// metric units for A4, etc.)
			fp_PageSize PageSize(paper_name);

			// if we know that the explicit size is not valid, we do
			// not need any further checking
			if(!bDoNotUseSize)
			{
				// in order to minimize effect of rounding errors, we are
				// better doing the comparison in the twipses; the MS
				// values suffer from rounding (?) error which is quite
				// significant, so we will round to the second least
				// significant digit
			
				double w = PageSize.Width(DIM_IN) * 1440.0;
				double h = PageSize.Height(DIM_IN) * 1440.0;

				UT_uint32 iPaperW10 = (static_cast<UT_uint32>( w))/10 + ((static_cast<UT_uint32>( w))%10 >= 5 ? 1 : 0);
				UT_uint32 iPaperH10 = (static_cast<UT_uint32>( h))/10 + ((static_cast<UT_uint32>( h))%10 >= 5 ? 1 : 0);

				UT_uint32 iPageW10 = asep->xaPage/10 + (asep->xaPage%10 >= 5 ? 1 : 0);
				UT_uint32 iPageH10 = asep->yaPage/10 + (asep->yaPage%10 >= 5 ? 1 : 0);

				if(iPageW10 != iPaperW10 ||
				   iPageH10 != iPaperH10)
				{
					bPaperNameValid = false;
				}
			}

			// if we are to use the paper name, then get the
			// dimensions to be used ...
			if(bPaperNameValid)
			{
				m_dim = PageSize.getDims();
			}
		}
		
		if (bPaperNameValid)
		{
			getDoc()->m_docPageSize.Set (paper_name);
		}
		else
		{
			getDoc()->m_docPageSize.Set ("Custom");
			getDoc()->m_docPageSize.Set (page_width, page_height, DIM_IN);
			getDoc()->m_docPageSize.setScale(page_scale);
		}
	} // end of page size stuff

	if(asep->fBidi)
	{
		// this is an RTL section, set dominant direction to rtl
		props += "dom-dir:rtl;";
	}
	else
	{
		// this is an LTR section, we want to set the direction
		// explicitely so that we do not end up with wrong default
		props += "dom-dir:ltr;";
	}


	if(asep->fPgnRestart)
	{
		// set to 1 when page numbering should be restarted at the beginning of this section
		props += "section-restart:1;";

		// user specified starting page number; sprmSPgnStart/97 are
		// only meaningful when page number restart is enabled
		UT_String_sprintf(propBuffer, "section-restart-value:%u;", asep->pgnStart);
		props += propBuffer;
	}

	// columns
	if (asep->ccolM1) {
		// number of columns
		UT_String_sprintf(propBuffer,"columns:%d;", (asep->ccolM1+1));
		props += propBuffer;

		// columns gap; when columns are not evenly spaced our model
		// cannot express per-column widths -- approximate with the
		// average inter-column spacing from sprmSDxaColSpacing
		double dGap = static_cast<double>(asep->dxaColumns);
		if (!asep->fEvenlySpaced)
		{
			double dSum = 0.0;
			UT_sint32 n = 0;
			for (UT_sint32 c = 0; c < asep->ccolM1 && c < 44; c++)
			{
				if (asep->rgdxaColumnWidthSpacing[c * 2 + 1])
				{
					dSum += asep->rgdxaColumnWidthSpacing[c * 2 + 1];
					n++;
				}
			}
			if (n)
				dGap = dSum / n;
		}
		UT_String_sprintf(propBuffer,"column-gap:%s;",
			UT_convertInchesToDimensionString(m_dim, dGap / 1440));
		props += propBuffer;
	}

	// draw a vertical line between columns
	if (asep->fLBetween == 1)
	{
		props += "column-line:on;";
	}

	// space after section (gutter)
	UT_String_sprintf(propBuffer,"section-space-after:%s;",
			UT_convertInchesToDimensionString(m_dim,
											  (static_cast<double>(asep->dzaGutter) / 1440)));
	props += propBuffer;

	// vertical justification of section content (Vjc)
	if (asep->vjc)
	{
		const char * pszVjc = nullptr;
		switch (asep->vjc)
		{
			case 1: pszVjc = "center"; break;
			case 2: pszVjc = "both"; break;	// vjcBoth == vAlign "both" (justified vertically)
			case 3: pszVjc = "bottom"; break;
		}
		if (pszVjc)
		{
			UT_String_sprintf(propBuffer, "section-y-align:%s;", pszVjc);
			props += propBuffer;
		}
	}

	// text flow (MSOTXFL -> OOXML ST_TextDirection tokens)
	if (asep->wTextFlow)
	{
		const char * pszDir = nullptr;
		switch (asep->wTextFlow)
		{
			case 1: pszDir = "tbRl"; break;	// msotxflVertN
			case 2: pszDir = "btLr"; break;	// msotxflHorzA
			case 3: pszDir = "tbRlV"; break;	// msotxflVert270
			case 4: pszDir = "lrTbV"; break;	// msotxflWordArtVert
			case 5: pszDir = "tbLrV"; break;	// msotxflWordArtVertL
		}
		if (pszDir)
		{
			UT_String_sprintf(propBuffer, "section-text-direction:%s;", pszDir);
			props += propBuffer;
		}
	}

	// printer paper sources (tray indices) for first/other pages
	if (asep->dmBinFirst)
	{
		UT_String_sprintf(propBuffer, "section-paper-src-first:%d;", asep->dmBinFirst);
		props += propBuffer;
	}
	if (asep->dmBinOther)
	{
		UT_String_sprintf(propBuffer, "section-paper-src-other:%d;", asep->dmBinOther);
		props += propBuffer;
	}

	// line numbering (sprmSNLnnMod != 0 enables it)
	if (asep->nLnnMod)
	{
		UT_String_sprintf(propBuffer, "section-ln-count-by:%d;", asep->nLnnMod);
		props += propBuffer;
		if (asep->dxaLnn)
		{
			UT_String_sprintf(propBuffer, "section-ln-distance:%d;", asep->dxaLnn);
			props += propBuffer;
		}
		// SLncOperand -> OOXML lnNumType@restart tokens
		const char * pszLnRestart = (asep->lnc == 0) ? "newPage" :
			(asep->lnc == 1) ? "newSection" : "continuous";
		UT_String_sprintf(propBuffer, "section-ln-restart:%s;", pszLnRestart);
		props += propBuffer;
		// lnnMin is one less than the number of the first line number
		UT_String_sprintf(propBuffer, "section-ln-start:%d;", asep->lnnMin + 1);
		props += propBuffer;
	}

	// document grid (SClmOperand -> OOXML docGrid@type tokens)
	if (asep->clm)
	{
		const char * pszGrid = nullptr;
		switch (asep->clm)
		{
			case 1: pszGrid = "linesAndChars"; break;	// clmCharsAndLines
			case 2: pszGrid = "lines"; break;		// clmLinesOnly
			case 3: pszGrid = "snapToChars"; break;	// clmEnforceGrid
		}
		if (pszGrid)
		{
			UT_String_sprintf(propBuffer, "section-doc-grid:%s;", pszGrid);
			props += propBuffer;
		}
		if (asep->dyaLinePitch)
		{
			UT_String_sprintf(propBuffer, "section-doc-grid-line-pitch:%d;",
							  asep->dyaLinePitch);
			props += propBuffer;
		}
		if (asep->dxtCharSpace)
		{
			// dxtCharSpace is a pitch difference in 1/4096 pt; the
			// OOXML prop carries twentieths of a point
			UT_String_sprintf(propBuffer, "section-doc-grid-char-space:%d;",
							  static_cast<UT_sint32>(asep->dxtCharSpace * 20 / 4096));
			props += propBuffer;
		}
	}

	// endnote suppression (sprmSFEndnote == 0)
	if (!asep->fEndNote)
	{
		props += "section-endnote-suppress:1;";
	}

	// right-to-left gutter position
	if (asep->fRTLGutter)
	{
		props += "section-rtl-gutter:1;";
	}

	// a section is only protected when document protection is on
	// (DopBase.fProtEnabled); sprmSFProtected un-protects a section
	if (ps && ps->dop.fProtEnabled)
	{
		UT_String_sprintf(propBuffer, "section-form-protected:%d;",
						  asep->fUnlocked ? 0 : 1);
		props += propBuffer;
	}

	// page borders
	bool bHavePageBorders = false;
	s_emitPageBorder(props, "page-border-top", &asep->brcTop, m_dim,
					 &bHavePageBorders);
	s_emitPageBorder(props, "page-border-left", &asep->brcLeft, m_dim,
					 &bHavePageBorders);
	s_emitPageBorder(props, "page-border-bottom", &asep->brcBottom, m_dim,
					 &bHavePageBorders);
	s_emitPageBorder(props, "page-border-right", &asep->brcRight, m_dim,
					 &bHavePageBorders);
	if (bHavePageBorders)
	{
		// PgbApplyTo -> OOXML pgBorders@display tokens
		const char * pszDisplay = nullptr;
		switch (asep->pgbApplyTo)
		{
			case 0: pszDisplay = "allPages"; break;
			case 1: pszDisplay = "firstPage"; break;
			case 2: pszDisplay = "notFirstPage"; break;
		}
		if (pszDisplay)
		{
			UT_String_sprintf(propBuffer, "page-border-display:%s;", pszDisplay);
			props += propBuffer;
		}
		// PgbOffsetFrom -> OOXML pgBorders@offsetFrom tokens
		UT_String_sprintf(propBuffer, "page-border-offset:%s;",
						  asep->pgbOffsetFrom ? "page" : "text");
		props += propBuffer;
	}

	// page-margin-left
	UT_String_sprintf(propBuffer, "page-margin-left:%s;",
			UT_convertInchesToDimensionString(m_dim,
											  (static_cast<double>(asep->dxaLeft) / 1440)));
	props += propBuffer;

	// page-margin-right
	UT_String_sprintf(propBuffer, "page-margin-right:%s;",
			UT_convertInchesToDimensionString(m_dim,
											  (static_cast<double>(asep->dxaRight) / 1440)));
	props += propBuffer;

	// page-margin-top
	UT_String_sprintf(propBuffer, "page-margin-top:%s;",
			UT_convertInchesToDimensionString(m_dim,
											  (static_cast<double>(asep->dyaTop) / 1440)));
	props += propBuffer;

	// page-margin-bottom
	UT_String_sprintf(propBuffer, "page-margin-bottom:%s;",
			UT_convertInchesToDimensionString(m_dim,
											  (static_cast<double>(asep->dyaBottom)/1440)));
	props += propBuffer;

	// remember the margins; FSPA origins of anchored frames are
	// frequently relative to the page margins (Spa.bx/by == 0)
	m_dSectMarginLeft = static_cast<double>(asep->dxaLeft) / 1440.0;
	m_dSectMarginTop = static_cast<double>(asep->dyaTop) / 1440.0;

	// page-margin-header
	UT_String_sprintf(propBuffer, "page-margin-header:%s;",
			UT_convertInchesToDimensionString(m_dim,
											  (static_cast<double>(asep->dyaHdrTop)/1440)));
	props += propBuffer;

	// page-margin-footer (word's footer is measured from the bottom
	// edge of the page -- contrary to the docs -- our's from the
	// bottom margin of the page)
	double dFooter = static_cast<double>(asep->dyaBottom) - static_cast<double>(asep->dyaHdrBottom);
	if(dFooter < 0)
	{
		dFooter = -dFooter;
	}
	dFooter = dFooter/1440.;
	UT_String_sprintf(propBuffer, "page-margin-footer:%s",
					  UT_convertInchesToDimensionString(m_dim,dFooter));
	props += propBuffer;
	xxx_UT_DEBUGMSG (("DOM:SEVIOR the section properties are: '%s'\n", props.c_str()));

	
	propsArray[0] = static_cast<const gchar *>("props");
	propsArray[1] = static_cast<const gchar *>(props.c_str());

	UT_uint32 iOff = 2;
	
	// headers/footers
	UT_String id[6];
	UT_uint32 iId = 0;

	// see _handleHeaders() on the contents of the m_pHeaders array,
	// it will make this maths clear (m_iCurrentSectId is 1-based
	// indx)
	// For each section in the document they are six headers/footers;
	// each of these can be in 3 states:
	//      length  > 2: proper header, use it
	//      length == 2: empty header; no header to be inserted
	//      length == 0: use header from the previous section

	if ((m_iCurrentSectId - 1)*6 + 6 < m_iHeadersCount)
	{
		// there are headers defined for this section
		UT_uint32 i = 6 + (m_iCurrentSectId - 1)*6;
		UT_uint32 j = i + 6;
		UT_sint32 k;

		for( ; i < j && i < m_iHeadersCount; i++)
		{
			// skip any unsupported or empty headers
			if(m_pHeaders[i].type == HF_Unsupported || m_pHeaders[i].len == 2)
			{
				continue;
			}

			// if this is a first page hdr/ftr we only use it if appropriate
			if(   (m_pHeaders[i].type == HF_HeaderFirst && !asep->fTitlePage)
			   || (m_pHeaders[i].type == HF_FooterFirst && !asep->fTitlePage))
			{
				// we want to change the type to unsupported to stop it from being
				// inserted into the document
				m_pHeaders[i].type = HF_Unsupported;
				continue;
			}

			k = i;
#if 0
			// For now this code is going to be disabled, since a
			// present AW sections cannot share headers, and this type
			// of a header needs to be replaced by a physical copy of
			// the previous meaningul header
			if(m_pHeaders[i].len == 0)
			{
				// this is the case where the section is to use the
				// header of a previous section -- scroll back until
				// we find one
				k -= 6;
				bool bContinue = false;
				
				while(k > 5)
				{
					if(m_pHeaders[k].len == 2)
					{
						// found empty header 
						bContinue = true;
						break;
					}
					else if(m_pHeaders[k].len == 0)
					{
						// try one section ahead
						k -= 6;
					}
					else
					{
						// found a meaningful header
						break;
					}
				}

				if(bContinue || k < 6)
				{
					continue;
				}
			}
#endif
			switch(m_pHeaders[k].type)
			{
				case HF_HeaderEven:
					propsArray[iOff++] = "header-even";
					break;
				case HF_FooterEven:
					propsArray[iOff++] = "footer-even";
					break;
				case HF_HeaderOdd:
					propsArray[iOff++] = "header";
					break;
				case HF_FooterOdd:
					propsArray[iOff++] = "footer";
					break;
				case HF_HeaderFirst:
					propsArray[iOff++] = "header-first";
					break;
				case HF_FooterFirst:
					propsArray[iOff++] = "footer-first";
					break;
				default:
					UT_ASSERT_HARMLESS(UT_NOT_REACHED);
			}

			UT_String_sprintf(id[iId],"%d",m_pHeaders[k].pid);
			propsArray[iOff++] = id[iId++].c_str();
		}
	}
	
	propsArray[iOff++] = nullptr;
	UT_return_val_if_fail(iOff <= sizeof(propsArray), 1);
	

	if (!_appendStrux(PTX_Section, PP_std_copyProps(&propsArray[0])))
	{
		UT_DEBUGMSG (("DOM: error appending section props!\n"));
		return 1;
	}

	// increment our section count
	m_bInSect = true;
	m_bInPara = false; // reset paragraph status
	m_nSections++;

	// TODO: we need to do some work on Headers/Footers

	/*
	 * break codes (SBkcOperand):
	 * 0 continuous -- the section starts on the next line
	 * 1 new column
	 * 2 new page
	 * 3 even page
	 * 4 odd page
	 */
	if (m_nSections > 1) // don't apply on the 1st page
	{
		// new sections always need a block
		if (!_appendStrux(PTX_Block, PP_NOPROPS))
		{
			UT_DEBUGMSG (("DOM: error appending new block\n"));
			return 1;
		}
		m_bInPara = true;

		UT_UCS4Char ucs = UCS_FF;
		switch (asep->bkc) {
			case 1:
				// a column break in a section without columns is
				// treated as a page break by MSO
				if (asep->ccolM1 > 0)
					ucs = UCS_VTAB;
				X_CheckError(_appendSpan(&ucs,1));
				break;

			case 3: // even page
			case 4: // odd page
				// our model cannot insert the filler page that a
				// true even/odd section break requires; a page break
				// is the closest we can do
				X_CheckError(_appendSpan(&ucs,1));
				break;

			case 2:
				X_CheckError(_appendSpan(&ucs,1));
				break;

			case 0:
			default:
				break;
		}
	}

	return 0;
}

// this function is called from _handleHeadersText() with meaningless
// parameters; if you want to make use of any of the parameters here,
// make sure it will work with nullptrs, etc.
int IE_Imp_MsWord_97::_endSect (wvParseStruct * /* ps */ , UT_uint32  /* tag */ ,
								void * /* prop */, int /* dirty */ )
{
#if 0
	// if we're at the end of a section, we need to check for a section mark
	// at the end of our character stream and remove it (to prevent page breaks
	// between sections)

	// this does not work -- if we are at the end of a section we have
	// already flushed the buffer in _endPara()
	if (m_pTextRun.size() &&
		m_pTextRun[m_pTextRun.size()-1] == UCS_FF)
	  {
		m_pTextRun[m_pTextRun.size()-1] = 0;
	  }
#endif

	// we never appended a paragraph inside of this section. we're naughty. correct that here.
	if (!m_bInPara  && !m_bInTextboxes && !m_bInAnnotations)
		_appendStrux(PTX_Block, PP_NOPROPS);

	// if there is a pending page break it belongs to the section and
	// is to be removed, we just need to set the tracker to false
	m_bPageBreakPending = false;
	m_bLineBreakPending = false;

	m_bInSect = false;
	m_bInPara = false; // reset paragraph status
	return 0;
}

int IE_Imp_MsWord_97::_beginPara (wvParseStruct *ps, UT_uint32 /*tag*/,
				  void *prop, int /*dirty*/)
{

	// if in a header of unsupported type, just return
	// the +1 is to account for the fact that ps->currentcp applies to the previous
	// char position ...
	if(_ignorePosition(ps->currentcp + 1))
		return 0;
	
	PAP *apap = static_cast <PAP *>(prop);

	// the header/footnote/endnote sections are special; because the
	// parser treats them as a continuation of the document, we end up
	// here before we get chance to handle the change from main doc to
	// these sections -- we want the paragraph properties assembled
	// for future use, but we do not want the strux actually inserted
	bool bDoNotInsertStrux = (ps->currentcp == m_iFootnotesStart ||
							  ps->currentcp == m_iEndnotesStart  ||
							  ps->currentcp == m_iAnnotationsStart ||
							  ps->currentcp == m_iHeadersStart);

	// the end of endnotes/fnotes/headers and all other subsections in
	// the main stream always contains a paragraph marker; we do not
	// want it to insert strux on those
	if((ps->currentcp == m_iTextEnd - 1 && m_iTextEnd > m_iTextStart)                ||
	   //(ps->currentcp == m_iTextEnd - 2 && m_iTextEnd > m_iTextStart)                ||
	   (ps->currentcp == m_iFootnotesEnd - 1 && m_iFootnotesEnd > m_iFootnotesStart) ||
	   (ps->currentcp == m_iEndnotesEnd - 1  && m_iEndnotesEnd > m_iEndnotesStart)   ||
	   (ps->currentcp == m_iHeadersEnd - 1 && m_iHeadersEnd > m_iHeadersStart)       ||
	   (ps->currentcp == m_iAnnotationsEnd - 1 && m_iAnnotationsEnd > m_iAnnotationsStart) ||
	   (ps->currentcp == m_iMacrosStart - 1 && m_iMacrosEnd > m_iMacrosStart) ||
	   (ps->currentcp == m_iTextboxesStart - 1 && m_iTextboxesEnd > m_iTextboxesStart))
	{
		bDoNotInsertStrux  = true;
	}

	/* a paragraph beginning exactly at a textbox range boundary is
	 * handled by _handleTextboxesText, which fires once the insert
	 * point has been re-targeted to that box's frame; a strux
	 * appended here would land at the tail of the previous box's
	 * frame (the para's props still get generated below) */
	if(m_bInTextboxes && m_pTextboxes)
	{
		for(UT_sint32 t = 0; t < m_iTextboxCount; t++)
		{
			if(ps->currentcp + 1 == m_pTextboxes[t].txt_pos)
			{
				bDoNotInsertStrux = true;
				break;
			}
		}
	}
	bool bInHdrFtr = false;
	if((ps->currentcp+1 >= m_iHeadersStart) && (ps->currentcp < m_iHeadersEnd))
	{
		bInHdrFtr = true;
	}
	bool bInTextboxes = false;
	if((ps->currentcp+1 >= m_iTextboxesStart) && (ps->currentcp < m_iTextboxesEnd))
	{
		bInTextboxes = true;
	}
	// at the end of each f/enote is a superflous paragraph marker
	// which we do not want imported
	if(m_bInFNotes && m_iNextFNote < m_iFootnotesCount && m_pFootnotes &&
	   m_pFootnotes[m_iNextFNote].txt_pos + m_pFootnotes[m_iNextFNote].txt_len - 1 >= ps->currentcp)
	{
		bDoNotInsertStrux = true;
	}
	
	if(m_bInENotes && m_iNextENote < m_iEndnotesCount && m_pEndnotes &&
	   m_pEndnotes[m_iNextENote].txt_pos + m_pEndnotes[m_iNextENote].txt_len - 1 >= ps->currentcp)
	{
		bDoNotInsertStrux = true;
	}

	// a comment body likewise ends in a superfluous paragraph mark,
	// and paragraph struxes inside the body are suppressed -- the
	// body gets a single block created when its range is entered
	if(m_bInAnnotations && m_iNextAnnotation < m_iAnnotationsCount && m_pAnnotations &&
	   m_pAnnotations[m_iNextAnnotation].txt_len &&
	   m_pAnnotations[m_iNextAnnotation].txt_pos + m_pAnnotations[m_iNextAnnotation].txt_len - 1 >= ps->currentcp)
	{
		bDoNotInsertStrux = true;
	}


	// the header section requires even more special care; since we
	// need to insert the HdrFtr strux for each header before we can
	// insert the block, we do not want a strux inserted at the start
	// position of a header; furthermore, each header ends with a
	// superfluous paragraph marker
	if(m_bInHeaders &&
	   ((m_iCurrentHeader < m_iHeadersCount && m_pHeaders &&
	   (m_pHeaders[m_iCurrentHeader].pos == ps->currentcp ||
		m_pHeaders[m_iCurrentHeader].pos + m_pHeaders[m_iCurrentHeader].len - 1 <= ps->currentcp))
		|| m_iCurrentHeader == m_iHeadersCount))
	{
		//start a new header section
		bDoNotInsertStrux = true;
	}

	{
	  const int tblDepth = apap->fInTable ? wvTableDepth(apap) : 0;

	  /* a paragraph at a shallower depth (or outside any table) closes
	     every deeper table still open */
	  while (static_cast<int>(m_vecTableCtx.getItemCount()) > tblDepth)
	  {
		  _table_pop_level(ps, apap);
	  }

	  if (tblDepth > 0)
	  {
		  // we have to call this unconditionally, since m_bInHeaders set does not mean that
		  // the HdrFtr strux for this section has been inserted.
		  _handleHeadersText(ps->currentcp +1, false);
		  _handleTextboxesText(ps->currentcp+1, 0);

		  while (static_cast<int>(m_vecTableCtx.getItemCount()) < tblDepth)
		  {
			  _table_push_level(ps);
		  }

		  MsTableCtx * ctx = _curTableCtx();
		  UT_return_val_if_fail(ctx, 0);

		  if (tblDepth == 1 && ps->endcell)
		  {
			  /* depth-1 cells end on the 0x07 mark char seen while
			     reading the previous paragraph's text */
			  ps->endcell = 0;
			  _cell_close(ctx);
			  if (ctx->iCellsRemaining > 0)
			  {
				  ctx->iCellsRemaining--;
				  if (ctx->iCellsRemaining == 0)
				  {
					  _row_close(ctx);
				  }
			  }
		  }
		  /* deeper cells end on the cell paragraph's own mark
		     (fInnerTableCell/fInnerTtp) - handled in _endPara() */

	    _row_open(ctx, ps, apap);
	    _cell_open(ctx, ps, apap);

	    if (ctx->iCellsRemaining == 0) {
	      ctx->iCellsRemaining = apap->ptap.itcMac + 1;
	    }

	    if (ctx->iRowsRemaining == 0) {
	      ctx->iRowsRemaining = ps->norows;
	    }

	    ctx->iRowsRemaining--;

	    /* a row-mark paragraph carries the TAP of the row it ends;
	       collect the row heights in document order */
	    if (apap->fTtp || apap->fInnerTtp)
	    {
		ctx->vecRowHeights.addItem(apap->ptap.dyaRowHeight);
	    }

	    /* merge-covered cells carry no rendered content (MS-DOC
	       TCGRF.horzMerge == 1); do not emit a paragraph for them */
	    if (ctx->bCoveredCell)
	    {
		this->_flush ();
		return 0;
	    }
	  }
	  else {
	    m_bInTable = false;
	  }
	}


	// first, flush any character data in any open runs
	// only flush if we are really inserting the strux (so that we can
	// remove any superfluous characters at ends of secitons,
	// e.g. page breaks)
	if(!bDoNotInsertStrux)
	{
		this->_flush ();
	}
	
	if (apap->fTtp || apap->fInnerTtp)
	  {
	    /* row-mark paragraphs carry no text of their own */
	    m_bInPara = true;
		xxx_UT_DEBUGMSG(("m_bInPara set true here -1 \n"));
	    return 0;
	  }

	if (apap->fBidi == 1)
	{
		m_bLTRParaContext = false;
	} else
	{
		m_bLTRParaContext = true;
	}

	m_bBidiMode = false;

	// break before paragraph?
	if (apap->fPageBreakBefore)
	{
		// TODO: this should really set a property in
		// TODO: in the paragraph, instead; but this
		// TODO: gives a similar effect for now.
		// TOOD: when it is handled properly the code needs to be
		// moved into _generateParaProps()
		UT_DEBUGMSG(("_beginPara: appending default block\n"));
		_appendStrux(PTX_Block, PP_NOPROPS);
		UT_UCS4Char ucs = UCS_FF;
		_appendSpan(&ucs,1);
	}

	m_charProps.clear();
	m_charStyle.clear();
	m_paraProps.clear();
	m_paraStyle.clear();
	_generateParaProps(m_paraProps, apap, ps);

	/* lists */
	UT_uint32 myListId = 0;
	UT_uint32 iAWListId = UT_UID_INVALID;
	std::string sLevel, sListId, sParentId;

	// all lists have ilfo set; some lists can be 'customised' by
	// having the number field removed (see bug 3622) -- they are
	// still lists in Word, but do not look like it, and we will not
	// treat them as lists (Tomas, May 26, 2003)
	if(apap->ilfo && apap->linfo.numberstr)
	{
		UT_uint32 j;
		// if we are in a new list, then do some clean up first and remember the list id
		if(m_iMSWordListId != apap->linfo.id)
		{
			m_iMSWordListId = apap->linfo.id;

			for(UT_uint32 i = 0; i < 9; i++)
				m_iListIdIncrement[i] = 0;

			UT_VECTOR_PURGEALL(ListIdLevelPair *, m_vLists);
			m_vLists.clear();
		}

		myListId = apap->linfo.id;

		/*
		  IMPORTANT the list sutff is found in several different
		  places:

		  apap->ilvl - the level of this list (0-8)

		  myListId - the id of this list, we need this to know to which list this
		  paragraph belongs; unfortunately, there seem to be some cases where separate
		  lists *share* the same id, for instance when two lists, of different formatting,
		  are separated by only empty paragraphs. As a hack, I have added the format number
		  to the list id, so gaining different id for different formattings (it is not foolproof,
		  for if id1 + format1 == id2 + format2 then we get two lists joined, but the probability
		  of that should be small). Further problem is that in AW, list id refers to the set of
		  list elements on the same level, while in Word the id is that of the entire list. The
		  easiest way to tranform the Word id to AW id is to add the level to the id, which
		  is what has been done above

		  apap->linfo.start - the stating number of this entire list;

		  apap->linfo.numberstr - the actual number string to display (XCHAR *); we probably need
		  this to work out the number separator, since there does not seem
		  to be any reference to this anywhere

		  apap->linfo.numberstr_size - length of the number string
		  
		  apap->linfo.format - number format (see the enum below)

		  apap->linfo.align	- number alignment [0: lft, 1: rght, 2: cntr]
		  
		  apap->linfo.ixchFollow - what character stands between the number and the para
		  [0:= tab, 1: spc, 2: none]
		*/

		// If a given list id has already been defined, appending a new list with
		// same values will have a harmless effect


		// we will use this to keep track of how many entries of given level we have had
		// every time we get here, we increase the counter for all levels lower than ours
		// then we will add the counter for our level to myListId; this way subsections of
		// the list separated by a higher level list entry will have different id's


		for(j = apap->ilvl + 1; j < 9; j++)
			m_iListIdIncrement[j]++;

		// collision-free list-instance key: the lsid picks the list
		// definition, ilfo the concrete instance (restarts share an
		// lsid), ilvl/format the Abi level slice, and the per-level
		// increment splits sibling segments.  The old additive scheme
		// let e.g. lsid+fmt1+ilvl1+inc1 alias lsid+fmt2+ilvl2+inc2.
		UT_uint64 myListKey =
			(static_cast<UT_uint64>(apap->linfo.id )<< 32)
			| (static_cast<UT_uint64>((apap->ilfo & 0x7ff) )<< 21)
			| (static_cast<UT_uint64>((apap->ilvl & 0xf) )<< 17)
			| (static_cast<UT_uint64>((m_iListIdIncrement[apap->ilvl] & 0x1ff) )<< 8)
			| (apap->linfo.format & 0xff);

		// see if this id is already in our map
		std::map<UT_uint64, UT_uint32>::iterator it =
			m_mListIdMap.find(myListKey);
		if (it != m_mListIdMap.end())
			iAWListId = it->second;

		if(iAWListId == UT_UID_INVALID)
		{
			iAWListId = getDoc()->getUID(UT_UniqueId::List);
			UT_ASSERT_HARMLESS(iAWListId != UT_UID_INVALID);

			m_mListIdMap[myListKey] = iAWListId;
		}

		UT_String propBuffer;

		// parent id
		// we will search backward our list vector for the first entry
		// that has a lower level than we and that will be our parent
		UT_uint32 myParentID = 0;
		for(UT_sint32 n = m_vLists.getItemCount(); n > 0; n--)
		{
			ListIdLevelPair * llp = const_cast<ListIdLevelPair *>(static_cast<const ListIdLevelPair*>((m_vLists.getNthItem(n - 1))));
			UT_nonnull_or_continue(llp);
			if(llp->level < apap->ilvl)
			{
				myParentID = llp->listId;
				break;
			}
		}

		// list delimiter: the Word number text splits into Abi's
		// list-delim (this level's own decorations) and list-decimal
		// (the separator between ancestor and descendant numbers)
		UT_UTF8String sDelim, sDecimal;
		bool bRefsAncestors = true;
		s_mapDocToAbiListDelim (apap->linfo.numberstr,
					apap->linfo.numberstr_size,
					apap->ilvl, sDelim, sDecimal,
					bRefsAncestors);
		char * t = s_stripDangerousChars(sDelim.utf8_str());
		std::string sDlm = t;
		FREEP(t);
		t = s_stripDangerousChars(sDecimal.utf8_str());
		std::string sDec = t;
		FREEP(t);

		// the Abi list type (the bullet glyph picks the bullet type)
		UT_uint32 iAbiType = s_mapDocToAbiListType
			(apap->linfo.format,
			 apap->linfo.numberstr, apap->linfo.numberstr_size);
		std::string sType = UT_std_string_sprintf("%u", iAbiType);

		// if the level's number text references no earlier-level
		// placeholder, Word does not prefix parent numbers -- don't
		// pretend it does by keeping a parent link
		if (!bRefsAncestors)
			myParentID = 0;

		// generate character props for the number
		// TODO -- the properties represented by apap->linfo.chp need
		// to be applied to the list number/bulet. For now, I am going
		// to translate these into a regular props string and attach
		// them to the list attributes, but they need to be passed
		// somehow down to the number field (may need a dedicated
		// _generateListCharProps() for this
		// Tomas, May 12, 2003
		UT_String szNumberProps;
		_generateCharProps(szNumberProps, &apap->linfo.chp, ps);

		std::string startValue = UT_std_string_sprintf("%d", apap->linfo.start);
		sLevel = UT_std_string_sprintf("%d", apap->ilvl + 1); // Word level starts at 0, Abi's at 1
		sListId = UT_std_string_sprintf("%d", iAWListId);
		sParentId = UT_std_string_sprintf("%d", myParentID);
		const PP_PropertyVector list_atts = {
			"id", sListId,
			"parentid", sParentId,
			"type", sType,
			"start-value", startValue,
			"list-delim", sDlm,
			"list-decimal", sDec,
			"level", sLevel,
			"props", szNumberProps.c_str()
		};

		// now add this to our vector of lists
		ListIdLevelPair * llp = new ListIdLevelPair;
		llp->listId = iAWListId;
		llp->level = apap->ilvl;
		m_vLists.addItem(static_cast<void*>(llp));

		getDoc()->appendList(list_atts);
		UT_DEBUGMSG(("DOM: appended a list\n"));

		// start-value
		// Need to put the ";" back in the para string.
		//
		m_paraProps[m_paraProps.size() - 1] = ';';
		m_paraProps += "start-value:";
		m_paraProps += startValue;
		m_paraProps += ";";

		// the delim/decimal as para props too, so the values survive
		// serialization paths that rebuild the list record
		m_paraProps += "list-delim:";
		m_paraProps += sDlm;
		m_paraProps += ";list-decimal:";
		m_paraProps += sDec;
		m_paraProps += ";";

		// list style
		m_paraProps += "list-style:";
		m_paraProps += s_mapDocToAbiListStyle (iAbiType);
		m_paraProps += ";";

		// field-font: the number's own font (bullets take their
		// glyph from the list type and get "NULL")
		m_paraProps += "field-font:";
		m_paraProps += s_fieldFontForList (iAbiType, ps,
						   &apap->linfo.chp);
	} // end of list-related code

 	// props
	PP_PropertyVector propsArray = {
		"props", m_paraProps.c_str()
	};

	// level, or 0 for default, normal level
	if (myListId > 0)
	{
		propsArray.push_back("level");
		propsArray.push_back(sLevel);
		propsArray.push_back("listid");
		propsArray.push_back(sListId);
		propsArray.push_back("parentid");
		propsArray.push_back(sParentId);
	}

	// handle style
	// TODO from wv we get the style props expanded and applied to the
	// characters in the paragraph (i.e., part of the CHP structure);
	// we need to be able to tell to wv not to do this expansion
	if(apap->stylename[0])
	{
		const STD * pSTD = ps->stsh.std;
		UT_uint32 iCount = ps->stsh.Stshi.cstd;

		if(apap->istd != istdNil && apap->istd < iCount)
		{
			propsArray.push_back("style");

			char * t = nullptr;
			const gchar * pName = nullptr;
			if(pSTD)
				pName = s_translateStyleId(pSTD[apap->istd].sti);

			if(pName)
			{
				m_paraStyle = pName;
			}
			else if(pSTD)
			{
				t = s_convert_to_utf8(ps,pSTD[apap->istd].xstzName);
				m_paraStyle = t;
			}

			FREEP(t);
			propsArray.push_back(m_paraStyle.c_str());
		}
	}

	if (!m_bInSect && !bDoNotInsertStrux)
	{
		// check for should-be-impossible case
		UT_ASSERT_NOT_REACHED();
		_appendStrux(PTX_Section, PP_NOPROPS);
		m_bInSect = true ;
	}

	if(!bDoNotInsertStrux)
	{
		xxx_UT_DEBUGMSG(("_beginPara: pos %d [text ends %d]\n", ps->currentcp, m_iFootnotesStart));

		if (!_appendStrux(PTX_Block, propsArray))
		{
			UT_DEBUGMSG(("DOM: error appending paragraph block\n"));
			return 1;
		}
		m_bInPara = true;
	}

	if (myListId > 0 && !bDoNotInsertStrux)
	  {
		// TODO: honor more props
		PP_PropertyVector list_field_fmt = {
			"type", "list_label",
			"props", "text-decoration:none",
		};
		_appendObject(PTO_Field, list_field_fmt);
		m_bInPara = true;

		PP_PropertyVector attribs = {
			"props", "text-decoration:none"
		};
		// the character following the list label - 0=tab, 1=space, 2=none
		if(apap->linfo.ixchFollow == 0) // tab
		{
			getDoc()->appendFmt(attribs);
			UT_UCS4Char tab = UCS_TAB;
			_appendSpan(&tab, 1);
		}
		else if(apap->linfo.ixchFollow == 1) // space
		{
			getDoc()->appendFmt(attribs);
			UT_UCS4Char space = UCS_SPACE;
			_appendSpan(&space, 1);
		}
		// else none
	  }

	return 0;
}

int IE_Imp_MsWord_97::_endPara (wvParseStruct * /*ps*/, UT_uint32 /*tag*/,
								void * prop, int /*dirty*/)
{
	xxx_UT_DEBUGMSG(("#DOM: _endPara\n"));
	// have to flush here, otherwise flushing later on will result in
	// an empty paragraph being inserted

	this->_flush ();
	m_bInPara = false;
	m_bLineBreakPending = false;

	/* inner-table cells end on a 0x0D paragraph mark whose PAP carries
	   fInnerTableCell, and inner rows on fInnerTtp (MS-DOC 2.4.3);
	   depth-1 cells end on the 0x07 char handled in _beginPara */
	const PAP * apap = static_cast<const PAP *>(prop);
	if (apap && m_bInTable)
	{
		MsTableCtx * ctx = _curTableCtx();
		const int depth =
			static_cast<int>(m_vecTableCtx.getItemCount());
		if (ctx && depth > 1 &&
			(apap->fInnerTableCell || apap->fInnerTtp || apap->fTtp))
		{
			_cell_close(ctx);
			if (ctx->iCellsRemaining > 0)
			{
				ctx->iCellsRemaining--;
				if (ctx->iCellsRemaining == 0)
				{
					_row_close(ctx);
				}
			}
		}
	}

	return 0;
}

int IE_Imp_MsWord_97::_beginChar (wvParseStruct *ps, UT_uint32 /*tag*/,
								  void *prop, int /*dirty*/)
{
	// if in a header of unsupported type, just return
	// the +1 is to account for the fact that ps->currentcp applies to the previous
	// char position ...
	if(_ignorePosition(ps->currentcp + 1))
		return 0;
	
	// the header/footnote/endnote sections are special; because the
	// parser treats them as a continuation of the document, we end up
	// here before we get chance to handle the change from main doc to
	// these sections -- we want the char properties assembled
	// for future use, but we do not want them actually appended
	bool bDoNotAppendFmt = (ps->currentcp == m_iFootnotesStart ||
							  ps->currentcp == m_iEndnotesStart  ||
							  ps->currentcp == m_iAnnotationsStart ||
							  ps->currentcp == m_iHeadersStart);

	// the end of endnotes/fnotes/headers and all other subsections in
	// the main stream always contain a paragraph marker; we do not
	// want it to append fmt on those
	if((ps->currentcp == m_iTextEnd - 1 && m_iTextEnd > m_iTextStart)                ||
	   (ps->currentcp == m_iTextEnd - 2 && m_iTextEnd > m_iTextStart)                ||
	   (ps->currentcp == m_iFootnotesEnd - 1 && m_iFootnotesEnd > m_iFootnotesStart) ||
	   (ps->currentcp == m_iEndnotesEnd - 1  && m_iEndnotesEnd > m_iEndnotesStart)   ||
	   (ps->currentcp == m_iHeadersEnd - 1 && m_iHeadersEnd > m_iHeadersStart)       ||
	   (ps->currentcp == m_iAnnotationsEnd - 1 && m_iAnnotationsEnd > m_iAnnotationsStart) ||
	   (ps->currentcp == m_iMacrosStart - 1 && m_iMacrosEnd > m_iMacrosStart))
	{
		bDoNotAppendFmt  = true;
	}
	

	// at the end of each f/enote is a superflous paragraph marker
	// which we do not want imported
	if(m_bInFNotes && m_iNextFNote < m_iFootnotesCount && m_pFootnotes &&
	   m_pFootnotes[m_iNextFNote].txt_pos + m_pFootnotes[m_iNextFNote].txt_len - 1 >= ps->currentcp)
	{
		bDoNotAppendFmt = true;
	}
	
	if(m_bInENotes && m_iNextENote < m_iEndnotesCount && m_pEndnotes &&
	   m_pEndnotes[m_iNextENote].txt_pos + m_pEndnotes[m_iNextENote].txt_len - 1 >= ps->currentcp)
	{
		bDoNotAppendFmt = true;
	}

	if(m_bInAnnotations && m_iNextAnnotation < m_iAnnotationsCount && m_pAnnotations &&
	   m_pAnnotations[m_iNextAnnotation].txt_len &&
	   m_pAnnotations[m_iNextAnnotation].txt_pos + m_pAnnotations[m_iNextAnnotation].txt_len - 1 >= ps->currentcp)
	{
		bDoNotAppendFmt = true;
	}

	// the header section requires even more special care; since we
	// need to insert the HdrFtr strux for each header before we can
	// insert the block, we do not want a strux and fmt inserted at the start
	// position of a header; furthermore, each header ends with a
	// superfluous paragraph marker
	if(m_bInHeaders &&
	   ((m_iCurrentHeader < m_iHeadersCount && m_pHeaders &&
	   (m_pHeaders[m_iCurrentHeader].pos == ps->currentcp ||
		m_pHeaders[m_iCurrentHeader].pos + m_pHeaders[m_iCurrentHeader].len - 1 <= ps->currentcp))
	   || m_iCurrentHeader == m_iHeadersCount))
	{
		//start a new header section
		bDoNotAppendFmt = true;
	}

	// flush any data in our character runs
	// if we are not really appending, then do not flush, so that we
	// are not prevented from removing superflous page breaks at the
	// end of section
	if(!bDoNotAppendFmt)
	{
		this->_flush ();
	}
	

	CHP *achp = static_cast <CHP *>(prop);

	const gchar * propsArray[7];
	UT_uint32 propsOffset = 0;

	m_charProps.clear();
	m_charStyle.clear();

	UT_uint32 iFontType = 0;
	if(achp->xchSym && ps->fonts.ffn && (achp->ftcSym < ps->fonts.nostrings))
	{
		// inserting a symbol char ...
		iFontType = ps->fonts.ffn[achp->ftcSym].chs;
	}
	else if(ps->fonts.ffn && (achp->ftcAscii < ps->fonts.nostrings))
	{
		iFontType = ps->fonts.ffn[achp->ftcAscii].chs;
	}
	
	if(iFontType == 0)
		m_bSymbolFont = false;
	else if(iFontType == 2)
		m_bSymbolFont = true;
	else
	{
		xxx_UT_DEBUGMSG(("IE_Imp_MsWord_97::_beginChar: unknow font encoding %d\n",
					 ps->fonts.ffn[achp->ftcAscii].chs));
		m_bSymbolFont = false;
	}
	
	memset (propsArray, 0, sizeof(propsArray));

	_generateCharProps(m_charProps, achp, ps);

	if (!achp->fBidi)
		m_bLTRCharContext = true;
	else
		m_bLTRCharContext = false;

	// we enter bidi mode if we encounter a character
	// formatting inconsistent with the base direction of the
	// paragraph; once in bidi mode, we have to stay there
	// until the end of the current pragraph
	m_bBidiMode = m_bBidiMode || (m_bLTRCharContext ^ m_bLTRParaContext);

	propsArray[propsOffset++] = static_cast<const gchar *>("props");
	propsArray[propsOffset++] = static_cast<const gchar *>(m_charProps.c_str());

	if(!m_bEncounteredRevision && (achp->fRMark || achp->fRMarkDel))
	{
		// revision "hack" - add a single revision for all revisioned text
		UT_UCS4String revisionStr ("msword_revisioned_text");
		getDoc()->addRevision(1, revisionStr.ucs4_str(), 0, 0);
		m_bEncounteredRevision = true;
	}

	if (achp->fRMark)
	{
	    propsArray[propsOffset++] = static_cast<const gchar *>("revision");
		m_charRevs = "1";
	    propsArray[propsOffset++] = m_charRevs.c_str();
	}
	else if (achp->fRMarkDel)
	{
	    propsArray[propsOffset++] = static_cast<const gchar *>("revision");
		m_charRevs = "-1";
	    propsArray[propsOffset++] = m_charRevs.c_str();
	}
	else
		m_charRevs.clear();
	
	
	if(achp->stylename[0])
	{
		const STD * pSTD = ps->stsh.std;
		UT_uint32 iCount = ps->stsh.Stshi.cstd;
		
		if(achp->istd != istdNil && achp->istd < iCount)
		{
			propsArray[propsOffset++] = static_cast<const gchar *>("style");
			char * t = nullptr;
			const gchar * pName = s_translateStyleId(pSTD[achp->istd].sti);
		
			if(pName)
			{
				m_charStyle = pName;
			}
			else
			{
				m_charStyle = t = s_convert_to_utf8(ps,pSTD[achp->istd].xstzName);
			}

			FREEP(t);
			propsArray[propsOffset++] = m_charStyle.c_str();
		}
	}

	// woah - major error here
	if(!m_bInSect && !bDoNotAppendFmt)
	{
		UT_ASSERT_NOT_REACHED();
		_appendStrux(PTX_Section, PP_NOPROPS);
		m_bInSect = true ;
	}

	// characters belonging to a merge-covered table cell are dropped by
	// _appendChar(); do not resurrect a block for their formatting
	MsTableCtx * pCtxC = _curTableCtx();
	if(pCtxC && pCtxC->bCoveredCell)
		return 0;

	if(!m_bInPara && !bDoNotAppendFmt)
	{
		UT_ASSERT_NOT_REACHED();
		_appendStrux(PTX_Block, PP_NOPROPS);
		m_bInPara = true ;
	}

	if(!bDoNotAppendFmt)
	{
		if (!_appendFmt(PP_std_copyProps(propsArray)))
		{
			UT_DEBUGMSG(("DOM: error appending character formatting\n"));
			return 1;
		}
	}
	
	return 0;
}

int IE_Imp_MsWord_97::_endChar (wvParseStruct * /*ps*/, UT_uint32 /*tag*/,
								void * /*prop*/, int /*dirty*/)
{
	// nothing is needed here
	return 0;
}

/****************************************************************************/
/****************************************************************************/

int IE_Imp_MsWord_97::_fieldProc (wvParseStruct *ps, U16 eachchar,
								  U8 chartype, U16 lid)
{
	xxx_UT_DEBUGMSG(("DOM: fieldProc: %c %x\n", static_cast<char>(eachchar),
					 static_cast<int>(eachchar)));

	//
	// The majority of this code has just been ripped out of wv/field.c
	//
	field * f = nullptr;
	UT_sint32 iRet = 1;
	
	if (eachchar == 0x13) // beginning of a field
	{
		if (!m_stackField.empty())
		{
			// see what kind of field we are in
			f = m_stackField.top();
			UT_return_val_if_fail(f,0);

			switch(f->type)
			{
				case F_TOC:
				case F_TOC_FROM_RANGE:
					if(_isTOCsupported(f))
					{
						break;
					}
					
					// for unsuported TOCs fall through ...
					/* fall through */
				case F_HYPERLINK:
					// for these fields we want to dump into the
					// document anything in the argument
					{
						f->argument[f->fieldI] = 0;
						UT_UCS2Char * a = f->argument;

						if(*a == 0x14)
						{
							a++;
						}
						
						while(*a)
						{
							this->_appendChar(*a++);
						}
						this->_flush();

						f->argument[0] = 0;
						f->fieldI = 0;
					}
					break;
					
				default:
					break;
			}
			
		}

		try
		{		
			f = new field;
		}
		catch(...)
		{
			f = nullptr;
		}

		UT_return_val_if_fail(f,0);
		f->fieldWhich = f->command;
		f->command[0] = 0;
		f->argument[0] = 0;
		f->fieldI = 0;
		f->fieldRet = 1;
		f->type = F_OTHER;
		f->iFldStory = -1;
		f->iFldIndex = -1;
		if(ps)
			_fldIndexAtCp(ps->currentcp, &f->iFldStory, &f->iFldIndex);
		m_stackField.push(f);
	}
	else if (eachchar == 0x14) // field trigger
	{
		f = !m_stackField.empty() ? m_stackField.top() : nullptr;
		UT_return_val_if_fail(f,0);
		
		f->command[f->fieldI] = 0;
		f->fieldC = wvWideStrToMB (f->command);

		if (this->_handleCommandField(f->fieldC))
			f->fieldRet = 1;
		else
			f->fieldRet = 0;

		wvFree(f->fieldC);
		f->fieldWhich = f->argument;
		f->fieldI = 0;
	}
	if(!f)
	{
		if(m_stackField.empty())
			return 1;
		f = m_stackField.top();
	}

	UT_return_val_if_fail(f,0);

	if (!f->fieldWhich) {
		UT_DEBUGMSG(("DOM: _fieldProc - 'which' is null\n"));
		UT_ASSERT_NOT_REACHED();
		return 1;
	}

	if (f->fieldI < FLD_SIZE - 1)
	{
		if (chartype)
			f->fieldWhich[f->fieldI] = wvHandleCodePage(eachchar, lid);
		else
			f->fieldWhich[f->fieldI] = eachchar;

		f->fieldI++;
	}
	// else: oversized field - keep consuming the field characters so
	// the nesting balance is preserved, just don't overrun the buffer

	if (eachchar == 0x15) // end of field marker
	{
		f->fieldWhich[f->fieldI] = 0;
		//I do not think we should convert this -- this is the field value
		//displayed in the document; in most cases we do not need it, as we
		//calulate it ourselves, but for instance for hyperlinks this is the
		//the text to which the link is tied
		//m_fieldA = wvWideStrToMB (m_argument);
		f->fieldC = wvWideStrToMB (f->command);
		_handleFieldEnd (f->fieldC, ps->currentcp);
		wvFree (f->fieldC);
		iRet = f->fieldRet;

		if(m_stackField.empty())
			return iRet;
		f = m_stackField.top();
		m_stackField.pop();
		UT_return_val_if_fail(f,0);
		delete f;
	}
	return iRet;
}

bool IE_Imp_MsWord_97::_handleFieldEnd (char *command, UT_uint32 /*iDocPosition*/)
{
	Doc_Field_t tokenIndex = F_OTHER;
	char *token;
	field * f = nullptr;
	f = m_stackField.empty() ? nullptr : m_stackField.top();
	UT_return_val_if_fail(f, true);
	UT_return_val_if_fail(command, true);

	if (*command != 0x13)
	{
		UT_DEBUGMSG (("field did not begin with 0x13\n"));
		return true;
	}

	if(m_bInTOC && m_bTOCsupported && (   f->type == F_TOC
									   || f->type == F_TOC_FROM_RANGE))
	{
		// end of TOC field in a supported TOC; we do nothing, since the field has already
		// been processed in _handleFieldCommand()
		m_bInTOC = false;
		m_bTOCsupported = false;
		return _insertTOC(f);
	}

	if(m_bInTOC && m_bTOCsupported)
	{
		// end of some non-TOC field inside supported TOC; just return
		return true;
	}

	command++;
	token = strtok (command, "\t, ");
	
	while(token)
	{
		tokenIndex = s_mapNameToField (token);
		switch (tokenIndex)
		{
		    case F_MERGEFIELD:
			{
				PP_PropertyVector atts = {
					"type", "mail_merge",
					"param"
				};

				token = strtok (nullptr, "\"\" ");

				UT_return_val_if_fail(f->argument[f->fieldI - 1] == 0x15, false);
				
				f->argument[f->fieldI - 1] = 0;
				UT_UCS2Char * a = f->argument;

				UT_UTF8String param;
				
				if(*a == 0x14)
					{
						a++;
					}

				while(*a)
					{
						if (!((171 == *a) || (187 == *a))) {
							// @argument looks like <<FieldName>>.
							// strip off the '<<' (171) and '>>' (187)
							param.appendUCS2(a, 1);
						}

						a++;
					}

				atts.push_back(param.utf8_str());

				if (!_appendObject (PTO_Field, atts))
					{
						UT_DEBUGMSG(("Dom: couldn't append field (type = '%s')\n", atts[1].c_str()));
					}
			}
			break;

			case F_HYPERLINK:
				{
					token = strtok (nullptr, "\"\" ");
					UT_return_val_if_fail(f->argument[f->fieldI - 1] == 0x15, false);
					
					f->argument[f->fieldI - 1] = 0;
					UT_UCS2Char * a = f->argument;

					if(*a == 0x14)
					{
						a++;
					}
					
					while(*a)
					{
						this->_appendChar(*a++);
					}
					this->_flush();

					if(!m_bInPara)
					{
						_appendStrux(PTX_Block, PP_NOPROPS);
						m_bInPara = true ;
					}

					// only close the link if the instruction actually
					// opened one, otherwise we'd emit an orphan end
					if(m_bInLink)
					{
						_appendObject(PTO_Hyperlink, PP_NOPROPS);
						m_bInLink = false;
					}
					break;
				}
			case F_TOC:             
			case F_TOC_FROM_RANGE:
				// we only get here for unsupported TOC types, in which case we dump the field
				// result (not ideal, since often the PAGEREF fields inside the TOC have not been
				// updated before save and so we get 'bookmark not found' instead of page numbers,
				// but it is better than nothing at all)
				
				{
					token = strtok (nullptr, "\"\" ");
					UT_return_val_if_fail(f->argument[f->fieldI - 1] == 0x15, false);
					
					f->argument[f->fieldI - 1] = 0;
					UT_UCS2Char * a = f->argument;

					if(*a == 0x14)
					{
						a++;
					}

					while(*a)
					{
						this->_appendChar(*a++);
					}
					this->_flush();
				}

				break;

			default:
				break;
		}

		token = strtok (nullptr, "\t, ");
	}
	return false;
}

/*!
    Word has several different toc tables (TOC, TOA, indexes); at the moment we only
    support TOC and even than only if it is based on heading styles
*/
bool IE_Imp_MsWord_97::_isTOCsupported(field *f)
{
	UT_return_val_if_fail(f,false);

	if(   f->type != F_TOC
	   && f->type != F_TOC_FROM_RANGE
	  )
	{
		return false;
	}
	
	bool bRet = true;
	char * command = wvWideStrToMB (f->command);
	UT_DEBUGMSG(("IE_Imp_MsWord_97::_isTOCsupported: command %s\n", command ? command : ""));

	char * params = nullptr;
	char * t = nullptr;

	if(f->type == F_TOC)
	{
		// command is "<0x13>TOC ..."; anything shorter has no usable params
		if(!command || strlen(command) < 5)
		{
			bRet = false;
			goto finish;
		}
		params = command + 5;
	}
	else if(f->type == F_TOC_FROM_RANGE)
	{
		if(!command || strlen(command) < 4)
		{
			bRet = false;
			goto finish;
		}
		params = command + 4;
	}
	
	// we only support the heading based TOC for now
	t = strstr(params, "\\o");

	if(!t)
		t = strstr(params, "\\t");

	if(!t)
	{
		bRet = false;
		goto finish;
	}

 finish:
	FREEP(command);
	return bRet;
}



/*!
   returns true if the TOC has been handled, false if the TOC type is unsupported
*/

/* Does this handle the contents styles indirectly via inserting the TOC as new and
	letting the default/initial pt code handle it like new rather than actually importing it? */

bool IE_Imp_MsWord_97::_insertTOC(field *f)
{
	UT_return_val_if_fail(f,false);
	bool bRet = true;
	bool bSupported = false;

	UT_sint32 i = 0, i1 = 0, i2 = 0;
	char * t = nullptr, * t1 = nullptr, * t2 = nullptr;
	UT_UTF8String sProps = "toc-has-heading:0;", sTemp, sLeader;

	const gchar * attrs [3] = {"props", nullptr, nullptr};

	char * command = wvWideStrToMB (f->command);
	UT_DEBUGMSG(("IE_Imp_MsWord_97::_insertTOC: command %s\n", command ? command : ""));

	char * params = nullptr;

	if(f->type == F_TOC)
	{
		if(!command || strlen(command) < 5)
		{
			bRet = false;
			goto finish;
		}
		params = command + 5;
	}
	else if(f->type == F_TOC_FROM_RANGE)
	{
		if(!command || strlen(command) < 4)
		{
			bRet = false;
			goto finish;
		}
		params = command + 4;
	}
	else
	{
		bRet = false;
		goto finish;
	}

	if((t = strstr(params, "\\p")))
	{
		// this defines the leader, we parse it first, before we mess up the command
		t1 = strchr(t, '\"');
		if(t1)
		{
			t1++;

			// AW can only use one of the chars (there are up to 5), we will take the first
			switch(*t1)
			{
				default: // not sure, we will treat this as a dot
				case '.': sLeader += "dot";       break;
				case '-': sLeader += "hyphen";    break;
				case '_': sLeader += "underline"; break;
				case ' ': sLeader += "none"; break;
			}
		}
	}

	if((t = strstr(params, "\\b")))
	{
		// a bookmark restricts the range from which the TOC is built
		t1 = strchr(t, '\"');
		if(t1)
		{
			t1++;

			t2 = strchr(t1, '\"');
			if(!t2)
			{
				bRet = false;
				goto finish;
			}

			char c = *t2;
			*t2 = 0;

			sProps += "toc-range-bookmark:";
			sProps += t1;
			sProps += ";";

			*t2 = c; // restore the string
		}
	}

	if((t = strstr(params, "\\o")))
	{
		// heading-based TOC
		// \o param specifies a range of headings to use, e.g., \o "2-4"
		bSupported = true;
		
		t = strchr(t, '\"');
	
		if(!t)
		{
			bRet = false;
			goto finish;
		}

		t++;

		i1 = atoi(t);

		if(!i1)
		{
			bRet = false;
			goto finish;
		}

		t1 = strchr(t, '-');
		t2 = strchr(t, '\"');

		t = UT_MIN(t1, t2);
	
		if(!t)
		{
			bRet = false;
			goto finish;
		}

		i2 = 0;
		if(*t == '\"')
		{
			i2 = i1;
		}
		else
		{
			UT_ASSERT_HARMLESS( *t == '-');
			t++;
			i2 = atoi(t);
		}
	
		if(!i2)
		{
			bRet = false;
			goto finish;
		}
		// now create our TOC attr/props
		//
		// * we do not need to set the source styles, because the Heading
		//   styles are the AW default
		//   
		// * we do have to set the dest styles
		//
		// * I am not sure what to do about toc-id: the AW FV_Fiew::cmdInsertTOC() does not specify the
		//   id, so neither will we
		//   
		// AW currently only uses the first 4 Heading styles, but we will implement this for all 9
		// to avoid future work
		
		for(i = 1; i < i1; ++i)
		{
			UT_UTF8String_sprintf(sTemp, "toc-source-style%d:nonexistentstyle;", i);
			sProps += sTemp;
		}

		UT_sint32 iMin = UT_MIN(i2+1,10);
		
		for(i = i1; i < iMin; ++i)
		{
			UT_UTF8String_sprintf(sTemp, "toc-dest-style%d:TOC %d", i, i);
			sProps += sTemp;
			sProps += ";";

			if(sLeader.size())
			{
				UT_UTF8String_sprintf(sTemp, "toc-tab-leader%d:", i);
				sProps += sTemp;
				sProps += sLeader;
				sProps += ";";
			}
		}

		for(i = iMin; i < 10; ++i)
		{
			UT_UTF8String_sprintf(sTemp, "toc-dest-style%d:nonexistentstyle", i);
			sProps += sTemp;
			sProps += ";";
		}
	}

	// the \t and \o switches can be used simultaneously
	// if both switches define the same level, we are unable to handle that; we will used the style
	// in the \t switch (it is easier since the parsing of the \t parameter is destructive)
	if ((t = strstr(params, "\\t")))
	{
		// style-based toc, the params have the format
		// \t "style,level,style,level ..."
		bSupported = true;
		t1 = strchr(t, '\"');
		if(!t1)
		{
			bRet = false;
			goto finish;
		}

		char * end = strchr(t1+1, '\"');
		if(!end)
		{
			bRet = false;
			goto finish;
		}

		while(t1 && t1 < end)
		{
			t1++;
			t2 = strchr(t1, ',');
			if(!t2)
			{
				bRet = false;
				goto finish;
			}

			*t2 = 0;

			sTemp = t1; // style name
			
			t1 = t2 + 1; // style level
			t2 = strchr(t1, ',');

			if(t2)
				t2 = UT_MIN(t2,end);
			else
				t2 = end;
			
			*t2 = 0;
			
			sProps += "toc-source-style";
			sProps += t1;
			sProps += ":";
			sProps += sTemp;
			sProps += ";";

			sProps += "toc-dest-style";
			sProps += t1;
			sProps += ":TOC ";
			sProps += t1;
			sProps += ";";
			
			if(sLeader.size())
			{
				sProps += "toc-tab-leader";
				sProps += t1;
				sProps += ":";
				sProps += sLeader;
				sProps += ";";
			}
			
			t1 = t2;
		}
	}

	if(!bSupported)
	{
		bRet = false;
		goto finish;
	}

	// remove trailing semicolon (screws up property parser)
	{
		sTemp = sProps;
		const char * c = sTemp.utf8_str();
		if(c[strlen(c)-1] == ';')
		{
			sProps.assign(c, strlen(c)-1);
		}
	}

	attrs[1] = sProps.utf8_str();

	if(!m_bInPara)
	{
		_appendStrux(PTX_Block, PP_NOPROPS);
		m_bInPara = true ;
	}

	_appendStrux(PTX_SectionTOC, PP_std_copyProps(attrs));
	_appendStrux(PTX_EndTOC, PP_NOPROPS);

 finish:
	FREEP(command);
	return bRet;
}


bool IE_Imp_MsWord_97::_handleCommandField (char *command)
{
	// if we are currently inside a supported TOC, just return
	if(m_bInTOC && m_bTOCsupported)
		return true;
	
	Doc_Field_t tokenIndex = F_OTHER;
	char *token = nullptr;
	field * f = m_stackField.empty() ? nullptr : m_stackField.top();
	UT_return_val_if_fail(f,true);
	bool bTypeSet = false;
	
	xxx_UT_DEBUGMSG(("DOM: handleCommandField '%s'\n", command));

	PP_PropertyVector atts = {
		"type"
	};

	if (!command || *command != 0x13)
	{
		UT_DEBUGMSG(("DOM: field did not begin with 0x13\n"));
		return true;
	}

	//first skip the 0x13
	command++;
	token = strtok(command, "\t, ");
	
	while(token)
	{
		tokenIndex = s_mapNameToField (token);
		if(!bTypeSet)
		{
			f->type = tokenIndex;
			bTypeSet = true;
		}
		
		switch (tokenIndex)
		{
			case F_EDITTIME:
			case F_TIME:
				atts.push_back("time");
				break;

			case F_DateTimePicture:
				//seems similar to a creation date
				atts.push_back("meta_date");
				break;

			case F_DATE:
				atts.push_back("date");
				break;

			case F_PAGE:
				atts.push_back("page_number");
				break;

			case F_NUMCHARS:
				atts.push_back("char_count");
				break;

			case F_NUMPAGES:
				atts.push_back("page_count");
				break;

			case F_NUMWORDS:
				atts.push_back("word_count");
				break;

			case F_FILENAME:
				atts.push_back("file_name");
				break;

			case F_PAGEREF:
				token = strtok (nullptr, "\"\" ");
				atts.push_back("page_ref");
				atts.push_back("param");
				if(token)
					atts.push_back(token);
				else
					atts.push_back("no_bookmark_given");
				break;

			case F_HYPERLINK:
				{
					/* HYPERLINK "target" [\o "tip"] [\t frame] [\l "bookmark"] ...
					 * walk the tokens so stray switches don't get mistaken
					 * for the target */
					std::string href;
					token = strtok (nullptr, "\"\" ");
					while(token)
					{
						if(token[0] == '\\')
						{
							if(!strcmp(token, "\\l"))
							{
								// link to a place in this document
								token = strtok (nullptr, "\"\" ");
								if(token)
								{
									href = "#";
									href += token;
								}
								break;
							}
							// \o "tip", \t frame and \* format take an argument
							if(!strcmp(token, "\\o") || !strcmp(token, "\\t") ||
							   !strcmp(token, "\\*"))
								strtok (nullptr, "\"\" ");
							token = strtok (nullptr, "\"\" ");
							continue;
						}
						href = token;
						break;
					}

					/* MS-DOC 2.4.7: the document's _PID_HLINKS property carries
					 * the authoritative target for this field; use it when the
					 * instruction had none (or was flagged out of sync) */
					if(href.empty() || !m_mapFieldHlink.empty())
					{
						std::string vt = _hyperlinkForField(f->iFldStory, f->iFldIndex);
						if(href.empty())
							href = vt;
					}

					if(!href.empty())
					{
					  PP_PropertyVector new_atts = {
						  "xlink:href", href
					  };
					  this->_flush();

					  if(!m_bInPara)
					    {
					      _appendStrux(PTX_Block, PP_NOPROPS);
					      m_bInPara = true ;
					    }

					  if(m_bInLink)
					    {
					      UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
					      _appendObject(PTO_Hyperlink, PP_NOPROPS);
					      m_bInLink = false;
					    }

					  _appendObject(PTO_Hyperlink, new_atts);
					  m_bInLink = true;
					}
					return true;
				}

			case F_TOC:             // for the toc fields we will
			case F_TOC_FROM_RANGE:  // insert the field result for now
				UT_DEBUGMSG(("TOC field encountered\n"));
				m_bInTOC = true;
				m_bTOCsupported = _isTOCsupported(f);

				/* fall through */
			default:
				// unhandled field type
				token = strtok(nullptr, "\t, ");
				continue;
		}

		
		this->_flush();

		if(!m_bInPara)
		{
			_appendStrux(PTX_Block, PP_NOPROPS);
			m_bInPara = true ;
		}

		if (!_appendObject (PTO_Field, atts))
		{
			UT_DEBUGMSG(("Dom: couldn't append field (type = '%s')\n", atts[1].c_str()));
		}

		token = strtok(nullptr, "\t, ");
	}

	return true;
}

enum MSWord_ImageType: uint8_t {
  MSWord_UnknownImage,
  MSWord_VectorImage,
  MSWord_RasterImage
};

static MSWord_ImageType s_determineImageType ( Blip * b )
{
  if ( !b )
	return MSWord_UnknownImage;

  switch ( b->type )
	{
	case msoblipEMF:
	case msoblipWMF:
	case msoblipPICT:
	  return MSWord_VectorImage;

	case msoblipJPEG:
	case msoblipPNG:
	case msoblipDIB:
	  return MSWord_RasterImage;

	case msoblipERROR:
	case msoblipUNKNOWN:
	default:
	  return MSWord_UnknownImage;
	}
}

static IEGraphicFileType s_determineIEGFT ( Blip * b )
{
	if ( !b )
		return IEGFT_Unknown;

	switch ( b->type )
	{
	case msoblipEMF:
		return IEGFT_EMF;
	case msoblipWMF:
		return IEGFT_WMF;

	case msoblipJPEG:
		return IEGFT_JPEG;
	case msoblipPNG:
		return IEGFT_PNG;
	case msoblipDIB:
		return IEGFT_DIB;

	case msoblipPICT:
	case msoblipERROR:
	case msoblipUNKNOWN:
	default:
		return IEGFT_Unknown;
	}
}



UT_Error IE_Imp_MsWord_97::_handleImage (Blip * b, long width, long height, long cropt, long cropb, long cropl, long cropr)
{
	FG_ConstGraphicPtr pFG;
	UT_Error error		= UT_OK;
	UT_ConstByteBufPtr buf;

	std::string propBuffer;
	std::string propsName;

	// suck the data into the ByteBuffer

	MSWord_ImageType imgType = s_determineImageType ( b );
	IEGraphicFileType iegft = s_determineIEGFT( b );

	wvStream *pwv;
	bool decompress = false;

	if ( imgType == MSWord_RasterImage )
	{
		pwv = b->blip.bitmap.m_pvBits;

	}
	else if ( imgType == MSWord_VectorImage )
	{
		pwv = b->blip.metafile.m_pvBits;
		decompress = (b->blip.metafile.m_fCompression == msocompressionDeflate);
	}
	else
	{
		UT_DEBUGMSG(("UNKNOWN IMAGE TYPE!!"));
		return UT_ERROR;
	}

	size_t size = wvStream_size (pwv);
	char *data = new char[size];
	wvStream_rewind(pwv);
	wvStream_read(data,size,sizeof(char),pwv);

	UT_ByteBufPtr pictData(new UT_ByteBuf);
	if (decompress)
	{

		unsigned long uncomprLen, comprLen;
		comprLen = size;
		uncomprLen = b->blip.metafile.m_cb;
		Bytef *uncompr = new Bytef[uncomprLen];
		int err = uncompress (uncompr, &uncomprLen, reinterpret_cast<const unsigned char *>(data), comprLen);
		if (err != Z_OK)
		{
			UT_DEBUGMSG(("Could not uncompress image\n"));
			DELETEPV(uncompr);
			DELETEPV(data);
			goto Cleanup;
		}
		pictData->append(reinterpret_cast<const UT_Byte*>(uncompr), uncomprLen);
		DELETEPV(uncompr);
	}
	else
	{
		pictData->append(reinterpret_cast<const UT_Byte*>(data), size);
	}

	delete [] data;

	if(!pictData->getPointer(0))
		error =  UT_ERROR;
	else
		error = IE_ImpGraphic::loadGraphic (pictData, iegft, pFG);

	if ((error != UT_OK) || !pFG)
	{
		UT_DEBUGMSG(("Could not import graphic\n"));
		return error;
	}

	// Word picture fields can legitimately carry a zero or negative
	// goal size (or a zero scaling factor); fall back to the image's
	// natural size so we don't emit a zero-sized object
	if (width <= 0)
		width = static_cast<long>(pFG->getWidth() * 1440.0);
	if (height <= 0)
		height = static_cast<long>(pFG->getHeight() * 1440.0);

	buf = pFG->getBuffer();

	if (!buf)
	{
		// i don't think that this could ever happen, but...
		UT_DEBUGMSG(("Could not convert to PNG\n"));
		return UT_ERROR;
	}

	//
	// This next bit of code will set up our properties based on the image attributes
	//

	{
		UT_LocaleTransactor t(LC_NUMERIC, "C");
		propBuffer = UT_std_string_sprintf("width:%fin; height:%fin; cropt:%fin; cropb:%fin; cropl:%fin; cropr:%fin",
						  static_cast<double>(width) / static_cast<double>(1440),
						  static_cast<double>(height) / static_cast<double>(1440),
						  static_cast<double>(cropt) / static_cast<double>(1440),
						  static_cast<double>(cropb) / static_cast<double>(1440),
						  static_cast<double>(cropl) / static_cast<double>(1440),
						  static_cast<double>(cropr) / static_cast<double>(1440));
	}

	propsName = UT_std_string_sprintf("%d", getDoc()->getUID(UT_UniqueId::Image));

	if (!_ensureInBlock())
	{
		UT_DEBUGMSG (("_ensureInBlock() failed\n"));
		return UT_ERROR;
	}

	{
		PP_PropertyVector propsArray = {
			"props", propBuffer,
			"dataid", propsName
		};

		if (!_appendObject (PTO_Image, propsArray)) {
			UT_DEBUGMSG (("Could not create append object\n"));
			return UT_ERROR;
		}
	}
	if (!getDoc()->createDataItem(propsName.c_str(), false,
								  buf, pFG->getMimeType(), nullptr))
	{
		UT_DEBUGMSG (("Could not create data item\n"));
		// the mimetype is sunk anyway
		return UT_ERROR;
	}

Cleanup:

	return error;
}



/*!
 * This method imports an image that can be later used as an embedded object.
 * The Blip pointer p contains the MS Word data we use to create the image
 * "width" and "height" are the width and height of the object in inches.
 * The routine returns the name of the data-item it creates is in the 
 * UT_UTF8String sImageName
 */
UT_Error IE_Imp_MsWord_97::_handlePositionedImage (Blip * b, UT_String & sImageName)
{
	FG_ConstGraphicPtr pFG;
	UT_Error error		= UT_OK;
	UT_ConstByteBufPtr buf;

  // suck the data into the ByteBuffer

  MSWord_ImageType imgType = s_determineImageType ( b );

  wvStream *pwv;
  bool decompress = false;

  if ( imgType == MSWord_RasterImage )
	{
	  pwv = b->blip.bitmap.m_pvBits;

	}
  else if ( imgType == MSWord_VectorImage )
	{
	  pwv = b->blip.metafile.m_pvBits;
	  decompress = (b->blip.metafile.m_fCompression == msocompressionDeflate);
	}
  else
	{
	  UT_DEBUGMSG(("UNKNOWN IMAGE TYPE!!"));
	  return UT_ERROR;
	}

  size_t size = wvStream_size (pwv);
  char *data = new char[size];
  wvStream_rewind(pwv);
  wvStream_read(data,size,sizeof(char),pwv);

  UT_ByteBufPtr pictData(new UT_ByteBuf);

  if (decompress)
  {

    unsigned long uncomprLen, comprLen;
    comprLen = size;
    uncomprLen = b->blip.metafile.m_cb;
    Bytef *uncompr = new Bytef[uncomprLen];
    int err = uncompress (uncompr, &uncomprLen, reinterpret_cast<const unsigned char *>(data), comprLen);
    if (err != Z_OK)
    {
        UT_DEBUGMSG(("Could not uncompress image\n"));
        DELETEPV(uncompr);
        DELETEPV(data);
        goto Cleanup;
    }
    pictData->append(reinterpret_cast<const UT_Byte*>(uncompr), uncomprLen);
    DELETEPV(uncompr);
  }
  else
  {
    pictData->append(reinterpret_cast<const UT_Byte*>(data), size);
  }

  delete [] data;

  if(!pictData->getPointer(0))
	  error =  UT_ERROR;
  else
	  error = IE_ImpGraphic::loadGraphic (pictData, IEGFT_Unknown, pFG);

  if ((error != UT_OK) || !pFG)
	{
	  UT_DEBUGMSG(("Could not import graphic\n"));
	  return UT_ERROR;
	}

  // TODO: can we get back a vector graphic?
  buf = pFG->getBuffer();

  if (!buf)
	{
	  // i don't think that this could ever happen, but...
	  UT_DEBUGMSG(("Could not convert to PNG\n"));
	  return UT_ERROR;
	}

  UT_String_sprintf(sImageName, "%d", getDoc()->getUID(UT_UniqueId::Image));

  if (!getDoc()->createDataItem(sImageName.c_str(), false,
                                buf, pFG->getMimeType(), nullptr))
	{
	  UT_DEBUGMSG (("Could not create data item\n"));
	  return UT_ERROR;
	}

 Cleanup:

  return error;
}

/****************************************************************************/
/****************************************************************************/

//
// wv callbacks to marshall data back to our importer class
//

static int charProc (wvParseStruct *ps, U16 eachchar, U8 chartype, U16 lid)
{
	IE_Imp_MsWord_97 * pDocReader = static_cast <IE_Imp_MsWord_97 *> (ps->userData);
	return pDocReader->_charProc (ps, eachchar, chartype, lid);
}

static int specCharProc (wvParseStruct *ps, U16 eachchar, CHP* achp)
{
	IE_Imp_MsWord_97 * pDocReader = static_cast <IE_Imp_MsWord_97 *> (ps->userData);
	return pDocReader->_specCharProc (ps, eachchar, achp);
}

static int eleProc (wvParseStruct *ps, wvTag tag, void *props, int dirty)
{
	IE_Imp_MsWord_97 * pDocReader = static_cast <IE_Imp_MsWord_97 *> (ps->userData);
	return pDocReader->_eleProc (ps, tag, props, dirty);
}

static int docProc (wvParseStruct *ps, wvTag tag)
{
	IE_Imp_MsWord_97 * pDocReader = static_cast <IE_Imp_MsWord_97 *> (ps->userData);
	return pDocReader->_docProc (ps, tag);
}


//--------------------------------------------------------------------------/
//--------------------------------------------------------------------------/

MsTableCtx::~MsTableCtx(void)
{
	UT_VECTOR_PURGEALL(MsColSpan *, vecColumnWidths);
	delete pTapLast;
}

void IE_Imp_MsWord_97::_table_open (MsTableCtx * ctx)
{
  //  _appendStrux(PTX_Block, nullptr); // Don't need/want this after 27/3/2005
  _appendStrux(PTX_SectionTable, PP_NOPROPS);
  m_bInPara = false;
#ifdef DEBUG
  static UT_sint32 sTableCount = 0;
  sTableCount++;
#endif
  UT_DEBUGMSG(("\n<TABLE> [%d]", sTableCount));

  /* remember this table's strux so that close-time property fixups
	 still hit the right table when tables are nested */
  PT_DocPosition posEnd = 0;
  getDoc()->getBounds(true,posEnd); // clean frags!
  ctx->pTableSdH = getDoc()->getLastStruxOfType(PTX_SectionTable);
}

//--------------------------------------------------------------------------/
//--------------------------------------------------------------------------/

MsTableCtx * IE_Imp_MsWord_97::_curTableCtx () const
{
	UT_sint32 iCount = static_cast<UT_sint32>(m_vecTableCtx.getItemCount());
	if (iCount < 1)
		return nullptr;
	return m_vecTableCtx.getNthItem(iCount - 1);
}

//--------------------------------------------------------------------------/
//--------------------------------------------------------------------------/

/*! open a new (possibly nested) table level for the current paragraph */
void IE_Imp_MsWord_97::_table_push_level (const wvParseStruct * ps)
{
	/* a table can start a document before any section strux exists */
	if (!m_bInSect)
	{
		_appendStrux(PTX_Section, PP_NOPROPS);
		m_bInSect = true;
	}

	MsTableCtx * ctx = new MsTableCtx;
	m_vecTableCtx.addItem(ctx);
	m_bInTable = true;
	_table_open(ctx);

	/* snapshot the column grid for this table level */
	if (ps->cellbounds)
	{
		for (UT_sint32 i = 0; i < ps->nocellbounds; i++)
			ctx->vecColumnPositions.addItem(ps->cellbounds[i]);
	}
}

//--------------------------------------------------------------------------/
//--------------------------------------------------------------------------/

/*! close the innermost open table level */
void IE_Imp_MsWord_97::_table_pop_level (const wvParseStruct * ps,
										 const PAP * apap)
{
	MsTableCtx * ctx = _curTableCtx();
	if (!ctx)
		return;

	_table_close(ps, apap, ctx);
	m_vecTableCtx.pop_back();
	delete ctx;

	m_bInTable = (m_vecTableCtx.getItemCount() > 0);
}

//--------------------------------------------------------------------------/
//--------------------------------------------------------------------------/

/*!
 * Exand a vector with zeros to make room for a new value
 */
void IE_Imp_MsWord_97::setNumberVector(UT_NumberVector & vec, UT_sint32 i, UT_sint32 val)
{
	while(i > static_cast<UT_sint32>(vec.size() +1))
	{
		vec.addItem(0);
	}
	vec.addItem(val); // we are sure that it will be appened at index i
}

/*!
 * This method parses the vector of MsColSpans held by m_vecColumnWidths
 * and fills the vector colWidths with the widths of the individual columns.
 *
 * We do this because MSWord provides the widths of column spans, and in 
 * some cases you can get a table with no row fully partitioned into 
 * individual cells.
 */
bool IE_Imp_MsWord_97::_build_ColumnWidths(MsTableCtx * ctx, UT_NumberVector & colWidths)
{

// OK handle the easy cases first and find the maximum value of iRight

	UT_sint32 iMaxRight = 0;
	UT_sint32 i = 0;
	UT_sint32 iLeft,iRight = 0;
	UT_sint32 iSize = static_cast<UT_sint32>(ctx->vecColumnWidths.size());
	for(i=0; i< iSize;i++)
	{
		MsColSpan * pSpan = reinterpret_cast<MsColSpan *>(ctx->vecColumnWidths.getNthItem(i));
		UT_nonnull_or_continue(pSpan);
		iLeft = pSpan->iLeft;
		iRight = pSpan->iRight;
		if(iMaxRight < iRight)
		{
			iMaxRight = iRight;
		}
		if((iLeft + 1) == iRight)
		{
			setNumberVector(colWidths,iLeft,pSpan->width);
			xxx_UT_DEBUGMSG(("_build_ColumnWidths Initial set: Left %d Width %d \n",iLeft,colWidths[iLeft]));
		}
	}
//
// Look to see if we're finished now.
//
	if((colWidths.size() == iMaxRight) && _isVectorFull(colWidths))
	{
		return true;
	}
	if(colWidths.size() < iMaxRight)
	{
		setNumberVector(colWidths,iMaxRight -1,0);
	}
//
// OK Now the hard part. Procede by scanning through the m_vecColWidths,
// Looking for spans, at each span we look to see if we can break the span
// into smaller pieces by subtracting a single span width.
//
// When we have a single column span we insert it in colWidths if colWidths
// is empty at that point.
//
// We continue until colWidths is completely full.
//
	UT_uint32 iLoop = 0;
	while(iLoop < 1000 && !_isVectorFull(colWidths))
	{
		for(i=0; i<static_cast<UT_sint32>(ctx->vecColumnWidths.size()); i++)
		{
			MsColSpan * pSpan = reinterpret_cast<MsColSpan *>(ctx->vecColumnWidths.getNthItem(i));
			UT_nonnull_or_continue(pSpan);
			iLeft = pSpan->iLeft;
			iRight = pSpan->iRight;
			xxx_UT_DEBUGMSG(("Loop %d iLeft %d,iRight %d colWidth[iLeft] %d colWidth[iRight-1] %d\n",iLoop,iLeft,iRight,colWidths[iLeft],colWidths[iRight -1]));
			if(iMaxRight < iRight)
			{
				iMaxRight = iRight;
			}
			if(((iLeft + 1) == iRight) && (colWidths[iLeft] == 0))
			{
				setNumberVector(colWidths,iLeft,pSpan->width);
			}
			else if((iLeft + 1) < iRight)
			{
				if(colWidths[iLeft] > 0)
				{
					if(!findMatchSpan(ctx, iLeft+1,iRight))
					{
						MsColSpan * pNewSpan = new MsColSpan();
						pNewSpan->iLeft = iLeft+1;
						pNewSpan->iRight = iRight;
						pNewSpan->width = pSpan->width - colWidths[iLeft];
						ctx->vecColumnWidths.addItem(pNewSpan);
					}
				}
				else if(colWidths[iRight - 1] > 0)
				{
					if(!findMatchSpan(ctx, iLeft,iRight-1))
					{
						MsColSpan * pNewSpan = new MsColSpan();
						pNewSpan->iLeft = iLeft;
						pNewSpan->iRight = iRight-1;
						pNewSpan->width = pSpan->width - colWidths[iRight-1];
						ctx->vecColumnWidths.addItem(pNewSpan);
					}
				}
//
// OK now look to see if we can fragment this by substracting a span of more 
// than one column from either end.
//
				else
				{
					UT_sint32 k =0;
					for(k=0; k<static_cast<UT_sint32>(ctx->vecColumnWidths.size()); k++)
					{
						MsColSpan * pMulSpan = ctx->vecColumnWidths.getNthItem(i);
						UT_nonnull_or_continue(pMulSpan);
						UT_sint32 iMulLeft = pMulSpan->iLeft;
						UT_sint32 iMulRight = pMulSpan->iRight;
						if(iMulLeft == iLeft && iMulRight < iRight)
						{
//
// Make a new span fragment out of the bit greater than MulRight if one doesn't
// exist
//
							if(!findMatchSpan(ctx, iMulRight+1,iRight))
							{
								MsColSpan * pNewSpan = new MsColSpan();
								pNewSpan->iLeft = iMulRight+1;
								pNewSpan->iRight = iRight;
								pNewSpan->width = pSpan->width - pMulSpan->width;
								ctx->vecColumnWidths.addItem(pNewSpan);
							}

						}
						else if (iMulLeft > iLeft && iMulRight == iRight)
						{
//
// Make a new span fragment out of the bit less than MulLeft
//
							if(!findMatchSpan(ctx, iLeft,iMulLeft))
							{
								MsColSpan * pNewSpan = new MsColSpan();
								pNewSpan->iLeft = iLeft;
								pNewSpan->iRight = iMulLeft;
								pNewSpan->width = pSpan->width - pMulSpan->width;
								ctx->vecColumnWidths.addItem(pNewSpan);
							}							
						}
					}
				}
			}
		}
		iLoop++;
		UT_ASSERT_HARMLESS(0);
	}
	UT_ASSERT_HARMLESS(iLoop < 1000);
	return (iLoop < 1000);
}

/*!
 * Returns true if a span in the ctx->vecColumnWidths span matches the left, right
 * values given
 */
bool IE_Imp_MsWord_97::findMatchSpan(MsTableCtx * ctx, UT_sint32 iLeft,UT_sint32 iRight)
{
	UT_sint32 i =0;
	for(i=0; i< static_cast<UT_sint32>(ctx->vecColumnWidths.size());i++)
	{
		MsColSpan * pSpan = ctx->vecColumnWidths.getNthItem(i);
		UT_nonnull_or_continue(pSpan);
		if(pSpan->iLeft == iLeft && pSpan->iRight == iRight)
		{
			return true;
		}
	}
	return false;
}

/*!
 * Returns false if any element in the vector is non-zero
 */
bool IE_Imp_MsWord_97::_isVectorFull(UT_NumberVector & vec)
{
	UT_sint32 i = 0;
	for(i=0;i< vec.size() ; i++)
	{
		xxx_UT_DEBUGMSG(("isVectorFull i %d val %d \n",i,vec[i]));
		if( vec[i] == 0)
		{
			return false;
			break;
		}
	}
	return true;
}

void IE_Imp_MsWord_97::_table_close (const wvParseStruct * /*ps*/,
									 const PAP * /*apap*/, MsTableCtx * ctx)
{
  _cell_close(ctx);
  _row_close(ctx);

  UT_String props("table-column-props:");
  UT_String propBuffer;

  if (ctx->vecColumnWidths.size() > 0)
  {
	  // build column width properties string
	  UT_NumberVector colWidths;
//
// Some tables maybe too complicated for my simple algorithim to work out
//
	  if(_build_ColumnWidths(ctx, colWidths))
	  {

		  for (UT_sint32 i = 0; i < colWidths.size(); i++)
		  {
			  UT_String_sprintf(propBuffer,"%s/",
							UT_convertInchesToDimensionString(m_dim,
															  (static_cast<double>(colWidths.getNthItem(i)))/1440.0));

			  props += propBuffer;
		  }
	  }

	  props += "; ";

	  UT_String_sprintf(propBuffer,"table-column-leftpos:%s; ",
							UT_convertInchesToDimensionString(m_dim,
															  (static_cast<double>(ctx->iLeftCellPos)/1440.0)));
	  props += propBuffer;
  }

  props += "table-line-ignore:0; table-line-type:1; table-line-thickness:0.8pt;";

  static const TAP s_emptyTap = TAP();
  const TAP * ptap = ctx->pTapLast ? ctx->pTapLast : &s_emptyTap;

  /* cell spacing: sprmTCellSpacingDefault (twips between cell edges)
	 wins over the legacy half-gap estimate */
  if (ptap->fCellSpacing && ptap->cellSpacing > 0)
  {
	  props += UT_String_sprintf("table-col-spacing:%s; ",
				UT_convertInchesToDimensionString(m_dim,
						static_cast<double>(ptap->cellSpacing)/1440.0));
  }
  else if (ptap->dxaGapHalf > 0)
  {
	  props += UT_String_sprintf("table-col-spacing:%s; ",
				UT_convertInchesToDimensionString(m_dim,
						(2.0 * ptap->dxaGapHalf)/1440.0));
  }
  else
  {
	  props += "table-col-spacing:0.03in; ";
  }

  /* row heights collected at each row mark; 0 means auto.  Word's
	 dyaRowHeight is negative for exact heights, positive for
	 at-least, so track the dominant type for table-row-height-type */
  if (ctx->vecRowHeights.getItemCount() > 0)
  {
	  bool bAnyAtLeast = false;
	  bool bAnyExact = false;
	  propBuffer.clear();
	  for (UT_sint32 i = 0;
		   i < static_cast<UT_sint32>(ctx->vecRowHeights.getItemCount()); i++)
	  {
		  UT_sint32 h = ctx->vecRowHeights.getNthItem(i);
		  if (h < 0)
		  {
			  bAnyExact = true;
			  h = -h;
		  }
		  else if (h > 0)
		  {
			  bAnyAtLeast = true;
		  }
		  propBuffer += UT_String_sprintf("%s/",
				UT_convertInchesToDimensionString(m_dim, h/1440.0));
	  }
	  props += UT_String_sprintf("table-row-heights:%s; ", propBuffer.c_str());
	  /* mixed exact/at-least rows cannot be expressed - prefer
		 at-least so content is never clipped */
	  if (bAnyAtLeast)
		  props += "table-row-height-type:at-least; ";
	  else if (bAnyExact)
		  props += "table-row-height-type:exactly; ";
  }

  /* table justification (jc: 0 left, 1 center, 2 right) */
  switch (ptap->jc)
  {
	  case 1: props += "table-position:center; "; break;
	  case 2: props += "table-position:right; "; break;
	  default: break; /* 0 and jcBidi are left-ish */
  }

  /* floating-table position (sprmTPc/DxaAbs/DyaAbs/*FromText); there
	 is no floating-table layout yet, so these are informational */
  if (ptap->pcVert != 3 || ptap->pcHorz != 3)
  {
	  if (ptap->pcHorz != 3)
	  {
		  /* pcHorz: 0 = column/text, 1 = margin, 2 = page */
		  const char * hAnchor = (ptap->pcHorz == 0) ? "text" :
			  (ptap->pcHorz == 1) ? "margin" : "page";
		  props += UT_String_sprintf("table-float-hanchor:%s; ", hAnchor);
	  }
	  if (ptap->pcVert != 3)
	  {
		  /* pcVert: 0 = top margin, 1 = page, 2 = text/paragraph */
		  const char * vAnchor = (ptap->pcVert == 2) ? "text" :
			  (ptap->pcVert == 1) ? "page" : "margin";
		  props += UT_String_sprintf("table-float-vanchor:%s; ", vAnchor);
	  }
	  if (ptap->dxaAbs != 0)
		  props += UT_String_sprintf("table-float-x:%s; ",
				UT_convertInchesToDimensionString(m_dim,
						static_cast<double>(ptap->dxaAbs)/1440.0));
	  if (ptap->dyaAbs != 0)
		  props += UT_String_sprintf("table-float-y:%s; ",
				UT_convertInchesToDimensionString(m_dim,
						static_cast<double>(ptap->dyaAbs)/1440.0));
	  if (ptap->dxaFromText)
		  props += UT_String_sprintf("table-float-margin-left:%s; ",
				UT_convertInchesToDimensionString(m_dim,
						static_cast<double>(ptap->dxaFromText)/1440.0));
	  if (ptap->dxaFromTextRight)
		  props += UT_String_sprintf("table-float-margin-right:%s; ",
				UT_convertInchesToDimensionString(m_dim,
						static_cast<double>(ptap->dxaFromTextRight)/1440.0));
	  if (ptap->dyaFromText)
		  props += UT_String_sprintf("table-float-margin-top:%s; ",
				UT_convertInchesToDimensionString(m_dim,
						static_cast<double>(ptap->dyaFromText)/1440.0));
	  if (ptap->dyaFromTextBottom)
		  props += UT_String_sprintf("table-float-margin-bottom:%s; ",
				UT_convertInchesToDimensionString(m_dim,
						static_cast<double>(ptap->dyaFromTextBottom)/1440.0));
  }

  /* default cell margins (sprmTCellPaddingDefault) */
  if (ptap->fCellPadMask)
  {
	  if (ptap->fCellPadMask & 0x01)
		  props += UT_String_sprintf("tblcellmar-top:%s; ",
				UT_convertInchesToDimensionString(m_dim,
						static_cast<double>(ptap->cellPadTop)/1440.0));
	  if (ptap->fCellPadMask & 0x02)
		  props += UT_String_sprintf("tblcellmar-left:%s; ",
				UT_convertInchesToDimensionString(m_dim,
						static_cast<double>(ptap->cellPadLeft)/1440.0));
	  if (ptap->fCellPadMask & 0x04)
		  props += UT_String_sprintf("tblcellmar-bottom:%s; ",
				UT_convertInchesToDimensionString(m_dim,
						static_cast<double>(ptap->cellPadBottom)/1440.0));
	  if (ptap->fCellPadMask & 0x08)
		  props += UT_String_sprintf("tblcellmar-right:%s; ",
				UT_convertInchesToDimensionString(m_dim,
						static_cast<double>(ptap->cellPadRight)/1440.0));
  }

  /* whole-table shading (sprmTSetShdTable) */
  if (ptap->shdTable.fCv || ptap->shdTable.icoBack)
  {
	  UT_String sBack;
	  if (ptap->shdTable.fCv)
	  {
		  if (s_mapColorRefToColor(ptap->shdTable.cvBack, sBack))
			  props += UT_String_sprintf("bgcolor:%s; ", sBack.c_str());
	  }
	  else
	  {
		  props += UT_String_sprintf("bgcolor:%s; ",
						  sMapIcoToColor(ptap->shdTable.icoBack, false).c_str());
	  }
  }

  /* the props parser rejects a trailing ';' (empty segment) - trim it */
  {
	  size_t nProps = props.size();
	  while (nProps > 0 &&
			 (props[nProps-1] == ';' || isspace(props[nProps-1])))
		  nProps--;
	  props = props.substr(0, nProps);
  }

  // apply properties to this table's strux
  pf_Frag_Strux* sdh = ctx->pTableSdH;
  if (!sdh)
  {
	  PT_DocPosition posEnd = 0;
	  getDoc()->getBounds(true,posEnd); // clean frags!
	  sdh = getDoc()->getLastStruxOfType(PTX_SectionTable);
  }
  if (sdh)
	  getDoc()->changeStruxAttsNoUpdate(sdh,"props",props.c_str());

  // end-of-table
  _appendStrux(PTX_EndTable, PP_NOPROPS);
  m_bInPara = false ;

  UT_DEBUGMSG(("\n</TABLE>\n"));
}

//--------------------------------------------------------------------------/
//--------------------------------------------------------------------------/

void IE_Imp_MsWord_97::_row_open (MsTableCtx * ctx, const wvParseStruct *ps,
								  const PAP *apap)
{
  if (ctx->bRowOpen)
	return;

  if (ctx->iCurrentRow > ps->norows) {
	  //UT_ASSERT(m_iCurrentRow <= ps->norows);
	  return;
  }

  ctx->bRowOpen = true;
  ctx->iCurrentRow++;
  xxx_UT_DEBUGMSG(("imp_MsWord: _row_open: Last Left %d Last Right %d \n",ctx->iLeft,ctx->iRight));
  ctx->iCurrentCell = 0;
  ctx->iLeft = 0;
  ctx->iRight = 0;

  /* the row TAP on this paragraph belongs to the row that is being
	 opened - remember it for table-level properties at close */
  if (!ctx->pTapLast)
	  ctx->pTapLast = new TAP;
  memcpy(ctx->pTapLast, &apap->ptap, sizeof(TAP));

  xxx_UT_DEBUGMSG(("\n\t<ROW:%d>", ctx->iCurrentRow));
}

//--------------------------------------------------------------------------/
//--------------------------------------------------------------------------/

void IE_Imp_MsWord_97::_row_close (MsTableCtx * ctx)
{
  if (ctx->bRowOpen) {
	xxx_UT_DEBUGMSG(("\t</ROW>"));
  }
  ctx->bRowOpen = false;
}

//--------------------------------------------------------------------------/
//--------------------------------------------------------------------------/

// line-style codes understood by PP_PropertyMap::linestyle_type
// (numeric property values): 0=none 1=solid 2=dotted 3=dashed
// 4=double 5=dashdot 6=dashdotdot 7=longdash 8=triple 9=wave
enum: uint8_t
{
  LS_OFF = 0,	        // No line style, which means no line is drawn
  LS_NORMAL = 1 	// A normal solid line
};

static int
sConvertLineStyle (short lineType)
{
  /* Word97 BRC type codes -> our numeric line-style codes */
  switch (lineType)
    {
    case 0:  return LS_OFF;		// none
    case 1:  return LS_NORMAL;		// single
    case 3:  return 4;			// double
    case 5:  return LS_NORMAL;		// hairline: thin solid
    case 6:  return 2;			// dotted
    case 7:  return 3;			// dashed
    case 8:  return 5;			// dotDash
    case 9:  return 6;			// dotDotDash
    case 10: return 8;			// triple
    case 20: return 9;			// wave
    case 21: return 9;			// doubleWave -> wave
    case 22: return 3;			// dashSmallGap -> dashed
    case 23: return 5;			// dashDotStroked -> dashdot
    case 11: // thinThickSmallGap: no exact match, closest is double
    case 12: // thickThinSmallGap
    case 13: // thinThickThinSmallGap
    case 14: // thinThickMediumGap
    case 15: // thickThinMediumGap
    case 16: // thinThickThinMediumGap
    case 17: // thinThickLargeGap
    case 18: // thickThinLargeGap
    case 19: // thinThickThinLargeGap
      return 4;
    default:
      return LS_NORMAL;
    }
}

/*! locate a cell edge (twips) in the table's merged cellbounds grid;
    wv merges bounds that are within 3 twips of each other, so match
    with that tolerance */
static UT_sint32
s_cellBoundIndex (const S16 * bounds, UT_sint32 n, S16 pos)
{
	UT_sint32 i;
	UT_sint32 iBest = -1;

	for (i = 0; i < n; i++)
	{
		if (abs(bounds[i] - pos) <= 3)
			return i;
		if (bounds[i] >= pos && iBest < 0)
			iBest = i;
	}
	return (iBest >= 0) ? iBest : n - 1;
}

/*! emit a cell margin property, preferring the per-cell value
    (TC.fPadMask) and falling back to the row's table-level default */
static void
s_cellMarginProp (UT_String & s, const char * pszSide, U8 bit,
				  const TC * tc, const TAP * ptap, UT_Dimension dim)
{
	UT_sint32 val;
	bool bHave = false;

	if (tc->fPadMask & bit)
	{
		switch (bit)
		{
			case 1: val = tc->padTop; break;
			case 2: val = tc->padLeft; break;
			case 4: val = tc->padBottom; break;
			default: val = tc->padRight; break;
		}
		bHave = true;
	}
	else if (ptap->fCellPadMask & bit)
	{
		switch (bit)
		{
			case 1: val = ptap->cellPadTop; break;
			case 2: val = ptap->cellPadLeft; break;
			case 4: val = ptap->cellPadBottom; break;
			default: val = ptap->cellPadRight; break;
		}
		bHave = true;
	}
	else
	{
		return;
	}

	if (bHave)
	{
		s += UT_String_sprintf("cell-margin-%s:%s; ", pszSide,
				UT_convertInchesToDimensionString(dim, val/1440.0));
	}
}

void IE_Imp_MsWord_97::_cell_open (MsTableCtx * ctx,
								   const wvParseStruct *ps,
								   const PAP *apap)
{
  /* row-mark paragraphs do not start cells: fTtp at depth 1,
	 fInnerTtp inside nested tables */
  if (ctx->bCellOpen || apap->fTtp || apap->fInnerTtp)
	return;

  if (!ctx->bRowOpen || ctx->iCurrentRow > ps->norows) {
	  //UT_ASSERT(m_bRowOpen || m_iCurrentRow <= ps->norows);
	  return;
  }

  ctx->bCellOpen = true;
  ctx->bCoveredCell = false;

  const TAP * ptap = &apap->ptap;
  const UT_sint32 itc = ctx->iCurrentCell;

  /* cell properties live in the row's TC array */
  const bool bHaveTC = (itc >= 0 && itc < ptap->itcMac);
  const TC * tc = bHaveTC ? &ptap->rgtc[itc] : nullptr;

  UT_sint32 vspan = 0;
  UT_String propBuffer;

  const gchar* propsArray[3];
  propsArray[0] = static_cast<const gchar*>("props");
  propsArray[1] = "";
  propsArray[2] = nullptr;

  if(ctx->iCurrentCell == 0 && ctx->iCurrentRow == 1)
  {
//
// Scan the differences in centers for this row so we can work out the column
// widths of the table eventually.
//
	  ctx->iLeftCellPos = 0;
	  UT_sint32 iLeft, iRight, i;
	  if (ps->cellbounds)
		  ctx->iLeftCellPos = ps->cellbounds[0];
	  for(i = 0; i < ps->nocellbounds-1; i++)
	  {
		  iLeft = i;
		  iRight = i+1;
		  UT_sint32 width = ps->cellbounds[iRight] - ps->cellbounds[iLeft];
		  if (width <= 0)
			  break;
		  MsColSpan * pSpan = new MsColSpan();
		  pSpan->iLeft = iLeft;
		  pSpan->iRight = iRight;
		  pSpan->width = width;
		  xxx_UT_DEBUGMSG(("MsImport iLeft %d  iRight %d width  %d \n",iLeft,iRight,width));
		  ctx->vecColumnWidths.addItem(pSpan);
	  }
  }

  /* grid position of this cell: map the row's rgdxaCenter edges onto
	 the whole-table cellbounds grid so that cells covered by merges
	 still advance the column counter */
  if (ps->cellbounds && ps->nocellbounds > 1 &&
	  itc + 1 <= ptap->itcMac)
  {
	  ctx->iLeft = s_cellBoundIndex(ps->cellbounds, ps->nocellbounds,
									ptap->rgdxaCenter[itc]);
	  ctx->iRight = s_cellBoundIndex(ps->cellbounds, ps->nocellbounds,
									ptap->rgdxaCenter[itc + 1]);
  }
  else
  {
	  ctx->iLeft = ctx->iCurrentCell;
	  ctx->iRight = ctx->iLeft + 1;
  }

  /* a horizontally merged cell (TCGRF.horzMerge >= 2) spans the
	 following merge-covered cells as well - its right edge is the
	 last covered cell's right edge */
  if (tc && tc->fFirstMerged)
  {
	  UT_sint32 j = itc + 1;
	  while (j < ptap->itcMac && ptap->rgtc[j].fMerged)
		  j++;
	  if (ps->cellbounds && ps->nocellbounds > 1 && j <= ptap->itcMac)
		  ctx->iRight = s_cellBoundIndex(ps->cellbounds, ps->nocellbounds,
										ptap->rgdxaCenter[j]);
	  else
		  ctx->iRight = ctx->iLeft + (j - itc);
  }

  if (ctx->iRight <= ctx->iLeft)
	  ctx->iRight = ctx->iLeft + 1;

  /* vertical merge: vmerges[row][cell] is the rowspan on the restart
	 cell and 0 on the covered continuation cells */
  if (ps->vmerges && ctx->iCurrentRow - 1 < ps->norows &&
	  ps->vmerges[ctx->iCurrentRow - 1] && bHaveTC)
	vspan = ps->vmerges[ctx->iCurrentRow - 1][itc];

  /* merge-covered cells keep no strux of their own */
  if ((tc && (tc->fMerged || (tc->fVertMerge && !tc->fVertRestart)))
	  || vspan == 0)
  {
	  ctx->bCoveredCell = true;
	  ctx->iCurrentCell++;
	  ctx->iLeft = ctx->iRight;
	  return;
  }

  if (vspan > 0)
	vspan--;
  else
	vspan = 0;

  UT_String_sprintf(propBuffer,
		    "left-attach:%d; right-attach:%d; top-attach:%d; bot-attach:%d; ",
		    ctx->iLeft,
		    ctx->iRight,
		    ctx->iCurrentRow - 1,
		    ctx->iCurrentRow + vspan
		    );

  if(ptap->dyaRowHeight < 0)
  {
	  // absolute height (dyaRowHeight is negative for exact heights)
	  propBuffer += UT_String_sprintf("height:%s;",
				UT_convertInchesToDimensionString(m_dim,
						-ptap->dyaRowHeight/1440.0));
  }
  else if(ptap->dyaRowHeight > 0)
  {
	  // at-least height -- I do not think we support this for now
	  // double dHin = -(ptap->dyaRowHeight/1440);
	  // propBuffer += UT_String_sprintf("height:%fin;",dHin);
  }
  else
  {
	  // auto height, do nothing
  }

  /* header-row marker so the layout can repeat it on each page */
  if (ptap->fTableHeader)
	propBuffer += "header-row:1; ";

  /* cell shading: full color (Shd.cv) when set, indexed otherwise */
  const SHD * shd = (itc >= 0 && itc < itcMax) ? &ptap->rgshd[itc] : nullptr;
  if (shd && shd->fCv)
  {
	  UT_String sFore, sBack;
	  if (s_mapColorRefToColor(shd->cvFore, sFore))
		propBuffer += UT_String_sprintf("color:%s;", sFore.c_str());
	  if (s_mapColorRefToColor(shd->cvBack, sBack))
	  {
		propBuffer += UT_String_sprintf("background-color:%s;", sBack.c_str());
		propBuffer += "bg-style:1;";
	  }
  }
  else
  {
	  const U8 icoFore = shd ? shd->icoFore : 0;
	  const U8 icoBack = shd ? shd->icoBack : 0;
	  propBuffer += UT_String_sprintf("color:%s;", sMapIcoToColor(icoFore, true).c_str());
	  propBuffer += UT_String_sprintf("background-color:%s;", sMapIcoToColor(icoBack, false).c_str());
	  // so long as it's not the "auto" color
	  if (icoBack != 0)
		propBuffer += "bg-style:1;";
  }

  if (tc)
  {
	  /* cell borders reuse the block-level border emitters
		 ("top-style" & friends are shared cell/para props) */
	  s_emitParaBorder(propBuffer, "top", &tc->brcTop, m_dim);
	  s_emitParaBorder(propBuffer, "left", &tc->brcLeft, m_dim);
	  s_emitParaBorder(propBuffer, "bot", &tc->brcBottom, m_dim);
	  s_emitParaBorder(propBuffer, "right", &tc->brcRight, m_dim);

	  /* vertical alignment: 0-100 offset, top=0 center=50 bottom=100 */
	  switch (tc->vertAlign)
	  {
		  case 1: propBuffer += "vert-align:50; "; break;
		  case 2: propBuffer += "vert-align:100; "; break;
		  default: break;
	  }

	  /* text direction, mapped to the OOXML keywords the layout
		 already understands */
	  switch (tc->textFlow)
	  {
		  case 1: propBuffer += "cell-text-direction:tbRl; "; break;
		  case 3: propBuffer += "cell-text-direction:btLr; "; break;
		  case 4: propBuffer += "cell-text-direction:lrTbV; "; break;
		  case 5: propBuffer += "cell-text-direction:tbRlV; "; break;
		  default: break;
	  }

	  if (tc->fFitText)
		propBuffer += "cell-fit-text:1; ";
	  if (tc->fNoWrap)
		propBuffer += "cell-no-wrap:1; ";
	  if (tc->fHideMark)
		propBuffer += "cell-hide-mark:1; ";
  }
  else
  {
	  /* fall back to the table-edge borders when the row carries no
		 per-cell TC records */
	  UT_LocaleTransactor t(LC_NUMERIC, "C");
	  const BRC * brc = &ptap->rgbrcTable[0];
	  propBuffer += UT_String_sprintf("top-style:%d; left-style:%d; bot-style:%d; right-style:%d;",
									  sConvertLineStyle(brc[0].brcType),
									  sConvertLineStyle(brc[1].brcType),
									  sConvertLineStyle(brc[2].brcType),
									  sConvertLineStyle(brc[3].brcType));
  }

  /* cell margins: per-cell overrides, then table defaults */
  if (tc || ptap->fCellPadMask)
  {
	  static const TC s_emptyTC = TC();
	  if (!tc)
		  tc = &s_emptyTC;
	  s_cellMarginProp(propBuffer, "top", 1, tc, ptap, m_dim);
	  s_cellMarginProp(propBuffer, "left", 2, tc, ptap, m_dim);
	  s_cellMarginProp(propBuffer, "bottom", 4, tc, ptap, m_dim);
	  s_cellMarginProp(propBuffer, "right", 8, tc, ptap, m_dim);
  }

  /* the props parser rejects a trailing ';' (empty segment) - trim it */
  {
	  size_t nProps = propBuffer.size();
	  while (nProps > 0 &&
			 (propBuffer[nProps-1] == ';' ||
			  isspace(propBuffer[nProps-1])))
		  nProps--;
	  propBuffer = propBuffer.substr(0, nProps);
  }

  xxx_UT_DEBUGMSG(("propbuffer: %s \n",propBuffer.c_str()));

  propsArray[1] = propBuffer.c_str();

  // do_insert:
  _appendStrux(PTX_SectionCell, PP_std_copyProps(propsArray));
  m_bInPara = false;
  ctx->iCurrentCell++;
  ctx->iLeft = ctx->iRight;
  xxx_UT_DEBUGMSG(("\t<CELL:%d>", static_cast<int>(ctx->iRight - ctx->iLeft)));
}

//--------------------------------------------------------------------------/
//--------------------------------------------------------------------------/

void IE_Imp_MsWord_97::_cell_close (MsTableCtx * ctx)
{
  if (!ctx->bCellOpen)
	return;

  ctx->bCellOpen = false;

  /* merge-covered slots emitted no cell strux, so there is nothing to
	 close */
  if (!ctx->bCoveredCell)
  {
	  _appendStrux(PTX_EndCell, PP_NOPROPS);
	  m_bInPara = false ;
  }
  ctx->bCoveredCell = false;

  xxx_UT_DEBUGMSG(("</CELL>"));
}


void IE_Imp_MsWord_97::_generateCharProps(UT_String &s, const CHP * achp, wvParseStruct *ps)
{
	UT_String propBuffer;

	// set char tolower if fSmallCaps && fLowerCase
	if ( achp->fSmallCaps && achp->fLowerCase )
		m_bIsLower = true;
	else
		m_bIsLower = false;

	// set language based the lid - TODO: do we want to handle -none- differently?
	s += "lang:";

	unsigned short iLid = 0;
	// I am not sure how the various lids are supposed to work, but
	// achp->fBidi does not mean that the lidBidi is set ...
	if (achp->fBidi)
	{
		iLid = achp->lidBidi;
	}
	else if(ps->fib.fFarEast)
	{
		iLid = achp->lidFE;
	}
	else
	{
		iLid = achp->lid;
	}
	

	// if we do not have meaningful lid, try default ...
	if(!iLid)
		iLid = achp->lidDefault;
	
	s += wvLIDToLangConverter (iLid);
	s += ";";

	// decide best codepage based on the lid (as lang code above)
	UT_String codepage;
	if (achp->fBidi)
		codepage = wvLIDToCodePageConverter (achp->lidBidi);
	else if (!ps->fib.fFarEast)
		codepage = wvLIDToCodePageConverter (achp->lidDefault);
	else
		codepage = wvLIDToCodePageConverter (achp->lidFE);

	// watch out for codepage 0 = unicode
	const char * pNUE = XAP_EncodingManager::get_instance()->getNativeUnicodeEncodingName();

	if (codepage == "CP0")
		codepage = pNUE;
	
	// if this is the first codepage we've seen, use it.
	// if we see more than one different codepage in a document, use unicode.
	if (!getDoc()->getEncodingName())
		getDoc()->setEncodingName(codepage.c_str());
	else if (getDoc()->getEncodingName() != codepage)
		getDoc()->setEncodingName(pNUE);

	// bold text
	bool fBold = (achp->fBidi ? achp->fBoldBidi : achp->fBold);
	if (fBold) {
		s += "font-weight:bold;";
	}

	// italic text
	bool fItalic = (achp->fBidi ? achp->fItalicBidi : achp->fItalic);
	if (fItalic) {
		s += "font-style:italic;";
	}

	// all-caps / small-caps (sprmCFCaps / sprmCFSmallCaps); these are
	// what OOXML w:caps / w:smallCaps map to
	if (achp->fCaps) {
		s += "text-transform:uppercase;";
	}
	if (achp->fSmallCaps) {
		s += "font-variant:small-caps;";
	}

	// foreground color: sprmCCv (a full COLORREF, Word 2000+) takes
	// precedence over sprmCIco's 5-bit index because it occurs later in
	// the grpprl (MS-DOC)
	{
		UT_String sColor;
		if (s_mapColorRefToColor(achp->cv, sColor)) {
			UT_String_sprintf(propBuffer, "color:%s;", sColor.c_str());
			s += propBuffer;
		} else {
			U8 ico = (achp->fBidi ? achp->icoBidi : achp->ico);
			if (ico) {
				UT_String_sprintf(propBuffer, "color:%s;",
								  sMapIcoToColor(ico, true).c_str());
				s += propBuffer;
			}
		}
	}

	// background color
	{
		UT_String sColor;
		if (achp->shd.fCv && s_mapColorRefToColor(achp->shd.cvBack, sColor)) {
			// sprmCShd carries full COLORREF shading (solid back color)
			UT_String_sprintf(propBuffer, "bgcolor:%s;", sColor.c_str());
			s += propBuffer;
		} else if (achp->shd.icoBack) {
			if (!achp->fHighlight) {
				// HACK: We don't support borders and shading yet, so it seems safe to use the background
				// color as a substitute when there's no true highlight color (see the doc from Bug 6432)
				UT_String_sprintf(propBuffer, "bgcolor:%s;",
								  sMapIcoToColor(achp->shd.icoBack, false).c_str());
			} else {
				// Note: This property won't be rendered until we have borders and shading support
				UT_String_sprintf(propBuffer, "background-color:%s;",
								  sMapIcoToColor(achp->shd.icoBack, false).c_str());
			}
			s += propBuffer;
		}
	}


	// underline and strike-through
	if (achp->fStrike || achp->fDStrike || achp->kul) {
		s += "text-decoration:";
		if ((achp->fStrike || achp->fDStrike) && achp->kul) {
			s += "underline line-through;";
		} else if (achp->kul) {
			s += "underline;";
		} else {
			s += "line-through;";
		}
	}

	// background color
	if (achp->fHighlight) {
		UT_String_sprintf(propBuffer,"bgcolor:%s;",
						  sMapIcoToColor(achp->icoHighlight, false).c_str());
		s += propBuffer;
	}

	// superscript && subscript
	if (achp->iss == 1) {
		s += "text-position: superscript;";
	} else if (achp->iss == 2) {
		s += "text-position: subscript;";
	} else if (achp->hpsPos > 0) {
		// sprmCHpsPos raised text; we have no fractional raise prop,
		// so approximate with superscript
		s += "text-position: superscript;";
	} else if (achp->hpsPos < 0) {
		s += "text-position: subscript;";
	}

	// letter spacing (sprmCDxaSpace, in twips) -> char-spacing
	if (achp->dxaSpace) {
		UT_String_sprintf(propBuffer, "char-spacing:%s;",
						  UT_convertInchesToDimensionString(m_dim,
												achp->dxaSpace / 1440.0));
		s += propBuffer;
	}

	// font-size threshold above which kerning applies (sprmCHpsKern is
	// in half-points)
	if (achp->hpsKern) {
		UT_String_sprintf(propBuffer, "char-kern:%spt;",
						  UT_convertToDimensionlessString(
							  achp->hpsKern / 2.0));
		s += propBuffer;
	}

	// horizontal character scale, percent (sprmCCharScale)
	if (achp->wCharScale && achp->wCharScale != 100) {
		UT_String_sprintf(propBuffer, "char-width:%d;", achp->wCharScale);
		s += propBuffer;
	}

	// East Asian emphasis marks (sprmCKcd) -> char-emphasis
	switch (achp->kcd) {
		case 1: s += "char-emphasis:dot;";      break; // solid circle above
		case 2: s += "char-emphasis:comma;";    break; // comma above
		case 3: s += "char-emphasis:circle;";   break; // circle above
		case 4: s += "char-emphasis:underDot;"; break; // solid circle below
		default: break;
	}

	if (achp->fVanish)
	{
	    s += "display:none;";
	}

	// font size (hps is half-points)
	// I have seen a bidi doc that had hpsBidi == 0, and the actual size in hps
	U16 hps = (achp->fBidi &&  achp->hpsBidi ? achp->hpsBidi : achp->hps);
	UT_String_sprintf(propBuffer,
					  "font-size:%dpt;", static_cast<int>((hps/2)));
	s += propBuffer;

	// font family
	char *fname;

	// if the FarEast flag is set, use the FarEast font,
	// otherwise, we'll use the ASCII font.
	if(achp->xchSym)
	{
		fname = wvGetFontnameFromCode(&ps->fonts, achp->ftcSym);
	}
	else if (achp->fBidi)
	{
		fname = wvGetFontnameFromCode(&ps->fonts, achp->ftcBidi);
	}
	else if (!ps->fib.fFarEast)
	{
		fname = wvGetFontnameFromCode(&ps->fonts, achp->ftcAscii);
	}
	else
	{
		fname = wvGetFontnameFromCode(&ps->fonts, achp->ftcFE);
	}

	// there are times when we should use the third, Other font,
	// and the logic to know when somehow depends on the
	// character sets or encoding types? it's in the docs.

	UT_ASSERT_HARMLESS(fname != nullptr);
	xxx_UT_DEBUGMSG(("font-family = %s\n", fname));

	s += "font-family:";

	if(fname)
		s += fname;
	else
		s += "Times New Roman";
	FREEP(fname);
}

/*! one MS-DOC "character unit" (sprmPDxc* operands) is the width of a
 *  full-width character in the document's default font, i.e. roughly
 *  the Normal style's font size; resolve it from the Normal style
 *  (sti == 0) when we can */
static UT_sint32
s_docCharUnitTwips(const wvParseStruct *ps)
{
	const UT_sint32 fallback = 240; /* 12pt */
	if (!ps || !ps->stsh.std)
	{
		return fallback;
	}
	for (UT_uint32 i = 0; i < ps->stsh.Stshi.cstd; i++)
	{
		if (ps->stsh.std[i].sti == 0 /* stiNormal */ &&
			ps->stsh.std[i].cupx > 0)
		{
			CHP achp;
			wvInitCHPFromIstd(&achp, static_cast<U16>(i),
							  const_cast<STSH *>(&ps->stsh));
			if (achp.hps)
			{
				return achp.hps * 10; /* half-points -> twips */
			}
			break;
		}
	}
	return fallback;
}

/*! convert a COLORREF (0x00BBGGRR) to an RRGGBB string; returns false
 *  for the "auto" color (0xFF000000, i.e. the fAuto bit of the high
 *  byte set) */
static bool
s_mapColorRefToColor(UT_uint32 cv, UT_String & sColor)
{
	if (cv & 0xFF000000)
	{
		return false;
	}
	sColor = UT_String_sprintf("%02x%02x%02x",
							   cv & 0xFF, (cv >> 8) & 0xFF, (cv >> 16) & 0xFF);
	return true;
}

/*! emit one side of a paragraph border (block-level "X-style",
 *  "X-color", "X-thickness", "X-space", "X-shadow" props) */
static void
s_emitParaBorder(UT_String &s, const char * pszSide, const BRC * brc,
				 UT_Dimension dim)
{
	UT_String propBuffer;

	if (!brc->brcType)
	{
		return;
	}

	UT_String_sprintf(propBuffer, "%s-style:%d;", pszSide,
					  sConvertLineStyle(brc->brcType));
	s += propBuffer;

	UT_String sColor;
	if (brc->fCv)
	{
		if (s_mapColorRefToColor(brc->cv, sColor))
		{
			UT_String_sprintf(propBuffer, "%s-color:%s;", pszSide,
							  sColor.c_str());
			s += propBuffer;
		}
	}
	else if (brc->ico && brc->ico <= 16)
	{
		UT_String_sprintf(propBuffer, "%s-color:%s;", pszSide,
						  sMapIcoToColor(brc->ico, true).c_str());
		s += propBuffer;
	}

	/* dptLineWidth is in 1/8-point increments for brcType < 0x40 (with
	   values < 2 treated as 2), and in whole points for brcType >=
	   0x40 (page-border art) */
	double dPoints;
	if (brc->brcType < 0x40)
	{
		dPoints = (brc->dptLineWidth < 2 ? 2 : brc->dptLineWidth) / 8.0;
	}
	else
	{
		dPoints = brc->dptLineWidth;
	}
	UT_String_sprintf(propBuffer, "%s-thickness:%s;", pszSide,
					  UT_convertInchesToDimensionString(dim, dPoints / 72.0));
	s += propBuffer;

	/* dptSpace is the text-to-border distance in points */
	if (brc->dptSpace)
	{
		UT_String_sprintf(propBuffer, "%s-space:%s;", pszSide,
						  UT_convertInchesToDimensionString(dim,
											brc->dptSpace / 72.0));
		s += propBuffer;
	}

	if (brc->fShadow)
	{
		UT_String_sprintf(propBuffer, "%s-shadow:1;", pszSide);
		s += propBuffer;
	}
}

void IE_Imp_MsWord_97::_generateParaProps(UT_String &s, const PAP * apap, wvParseStruct * ps)
{
	UT_String propBuffer;

	// DOM TODO: i think that this is right
	if (apap->fBidi == 1)
	{
		s += "dom-dir:rtl;";
	}
	else
	{
		s += "dom-dir:ltr;";
	}

	// paragraph alignment/justification
	switch(apap->jc)
	{
		case 0:
			s += "text-align:left;";
			break;
		case 1:
			s += "text-align:center;";
			break;
		case 2:
			s += "text-align:right;";
			break;
		case 3:
			s += "text-align:justify;";
			break;
		case 4: // distribute
		case 5: // mediumKashida
		case 7: // highKashida
		case 8: // lowKashida
		case 9: // thaiDistribute
			/* all of these are justification variants; our closest
			 * match is plain justified text */
			s += "text-align:justify;";
			break;
		default:
			break;
	}

	// keep paragraph together?
	if (apap->fKeep) {
		s += "keep-together:yes;";
	}

	// keep with next paragraph?
	if (apap->fKeepFollow) {
		s += "keep-with-next:yes;";
	}

	// widowed/orphaned lines
	if (!apap->fWidowControl) {
		// these Abinova properties give the same effect
		s += "orphans:0;widows:0;";
	}

	// line spacing (single-spaced, double-spaced, etc.)
	if (apap->lspd.fMultLinespace) {
		UT_String_sprintf(propBuffer,
						  "line-height:%s;",
						  UT_convertToDimensionlessString( (static_cast<double>(apap->lspd.dyaLine) / 240), "1.1"));
		s += propBuffer;
	} else if (apap->lspd.dyaLine < 0) {
		// negative dyaLine is an "exact" line height
		UT_String_sprintf(propBuffer,
						  "line-height:%s;",
						  UT_convertInchesToDimensionString(m_dim,
											(static_cast<double>(-apap->lspd.dyaLine) / 1440)));
		s += propBuffer;
	} else if (apap->lspd.dyaLine > 0) {
		// positive dyaLine without fMultLinespace is an "at least"
		// height; a trailing '+' marks that in our prop syntax
		UT_String_sprintf(propBuffer,
						  "line-height:%s+;",
						  UT_convertInchesToDimensionString(m_dim,
											(static_cast<double>(apap->lspd.dyaLine) / 1440)));
		s += propBuffer;
	}

	/* one character unit is about the width of a full-width char in
	   the default font; one line unit is a single-spaced line of it */
	UT_sint32 iCharUnit = 0;
	UT_sint32 iLineUnit = 0;
	if (apap->dxcRight || apap->dxcLeft || apap->dxcLeft1 ||
		apap->dylBefore || apap->dylAfter)
	{
		iCharUnit = s_docCharUnitTwips(ps);
		iLineUnit = iCharUnit * 6 / 5;
	}

	//
	// margins
	//

	// margin-right
	if (apap->dxaRight || apap->dxcRight) {
		double dTwips = apap->dxaRight;
		if (!dTwips)
		{
			dTwips = static_cast<double>(apap->dxcRight) * iCharUnit / 100;
		}
		UT_String_sprintf(propBuffer,
						  "margin-right:%s;",
						  UT_convertInchesToDimensionString(m_dim, dTwips / 1440));
		s += propBuffer;
	}

	// margin-left
	if (apap->dxaLeft || apap->dxcLeft) {
		double dTwips = apap->dxaLeft;
		if (!dTwips)
		{
			dTwips = static_cast<double>(apap->dxcLeft) * iCharUnit / 100;
		}
		UT_String_sprintf(propBuffer,
						  "margin-left:%s;",
						  UT_convertInchesToDimensionString(m_dim, dTwips / 1440));
		s += propBuffer;
	}

	// margin-left first line (indent)
	if (apap->dxaLeft1 || apap->dxcLeft1) {
		double dTwips = apap->dxaLeft1;
		if (!dTwips)
		{
			dTwips = static_cast<double>(apap->dxcLeft1) * iCharUnit / 100;
		}
		UT_String_sprintf(propBuffer,
						  "text-indent:%s;",
						  UT_convertInchesToDimensionString(m_dim, dTwips / 1440));
		s += propBuffer;
	}

	// margin-top
	if (apap->dyaBefore || apap->dylBefore) {
		double dTwips = apap->dyaBefore;
		if (!dTwips)
		{
			dTwips = static_cast<double>(apap->dylBefore) * iLineUnit / 100;
		}
		UT_String_sprintf(propBuffer,
						  "margin-top:%s;",
						  UT_convertInchesToDimensionString(m_dim, dTwips / 1440));
		s += propBuffer;
	}

	// margin-bottom
	if (apap->dyaAfter || apap->dylAfter) {
		double dTwips = apap->dyaAfter;
		if (!dTwips)
		{
			dTwips = static_cast<double>(apap->dylAfter) * iLineUnit / 100;
		}
		UT_String_sprintf(propBuffer,
						  "margin-bottom:%s;",
						  UT_convertInchesToDimensionString(m_dim, dTwips / 1440));
		s += propBuffer;
	}

	// tab stops
	if (apap->itbdMac) {
		propBuffer += "tabstops:";

		for (int iTab = 0; iTab < apap->itbdMac; iTab++) {
			propBuffer += UT_String_sprintf("%s/",
						UT_convertInchesToDimensionString(m_dim,
										((static_cast<double>(apap->rgdxaTab[iTab])) / 1440)));

			bool bBar = false;
			switch (apap->rgtbd[iTab].jc) {
				case 1:
					propBuffer += "C";
					break;
				case 2:
					propBuffer += "R";
					break;
				case 3:
					propBuffer += "D";
					break;
				case 4:
					propBuffer += "B";
					bBar = true;
					break;
				case 0:
				default:
					propBuffer += "L";
					break;
			}
			/* tab leader, MS-DOC TabLC -> FL_LEADER_*: dot=1,
			   hyphen=2, underscore=3; tlcHeavy is underscore-like
			   and tlcMiddleDot folds to dot; leaders are ignored
			   on bar tabs */
			if (!bBar && apap->rgtbd[iTab].tlc && apap->rgtbd[iTab].tlc < 7)
			{
				static const UT_uint8 s_tlcMap[7] = {0, 1, 2, 3, 3, 1, 0};
				propBuffer += UT_String_sprintf("%d",
						s_tlcMap[apap->rgtbd[iTab].tlc]);
			}
			propBuffer += ",";
		}
		// replace final comma with a semi-colon
		propBuffer[propBuffer.size()-1] = ';';
		s += propBuffer;
	}

	// paragraph borders (w:pBdr); "between" and "bar" borders have no
	// block-level equivalent here
	s_emitParaBorder(s, "top", &apap->brcTop, m_dim);
	s_emitParaBorder(s, "left", &apap->brcLeft, m_dim);
	s_emitParaBorder(s, "bot", &apap->brcBottom, m_dim);
	s_emitParaBorder(s, "right", &apap->brcRight, m_dim);

	// paragraph shading (w:shd): ipat 0 is a solid fill of the back
	// color, ipat 1 is a solid fill of the fore color, higher values
	// are patterns which we approximate with the fore color
	if (apap->shd.fCv)
	{
		UT_String sFore, sBack;
		bool bFore = s_mapColorRefToColor(apap->shd.cvFore, sFore);
		bool bBack = s_mapColorRefToColor(apap->shd.cvBack, sBack);
		if (apap->shd.ipatFull == 0)
		{
			if (bBack)
			{
				UT_String_sprintf(propBuffer, "shading-background-color:%s;",
								  sBack.c_str());
				s += propBuffer;
			}
		}
		else
		{
			UT_String_sprintf(propBuffer, "shading-pattern:%d;",
							  apap->shd.ipatFull);
			s += propBuffer;
			if (bFore)
			{
				UT_String_sprintf(propBuffer, "shading-foreground-color:%s;",
								  sFore.c_str());
				s += propBuffer;
			}
			if (bBack)
			{
				UT_String_sprintf(propBuffer, "shading-background-color:%s;",
								  sBack.c_str());
				s += propBuffer;
			}
		}
	}
	else
	{
		// Shd80: ico values are 1..16; 0 is "auto", and
		// 0x1F/0x1F/0x3F is Shd80Nil (no shading)
		U8 icoBack = apap->shd.icoBack;
		U8 icoFore = apap->shd.icoFore;
		U8 ipat = apap->shd.ipat;
		if (ipat == 0)
		{
			if (icoBack && icoBack <= 16)
			{
				UT_String_sprintf(propBuffer, "shading-background-color:%s;",
								  sMapIcoToColor(icoBack, false).c_str());
				s += propBuffer;
			}
		}
		else if (ipat != 0x3F)
		{
			UT_String_sprintf(propBuffer, "shading-pattern:%d;", ipat);
			s += propBuffer;
			if (icoFore && icoFore <= 16)
			{
				UT_String_sprintf(propBuffer, "shading-foreground-color:%s;",
								  sMapIcoToColor(icoFore, true).c_str());
				s += propBuffer;
			}
			if (icoBack && icoBack <= 16)
			{
				UT_String_sprintf(propBuffer, "shading-background-color:%s;",
								  sMapIcoToColor(icoBack, false).c_str());
				s += propBuffer;
			}
		}
	}

	// paragraph outline level (0-8; 9 is the default "body text")
	if (apap->lvl >= 0 && apap->lvl <= 8)
	{
		UT_String_sprintf(propBuffer, "outline-level:%d;", apap->lvl);
		s += propBuffer;
	}

	// wAlignFont is OOXML w:textAlignment: vertical run alignment
	switch (apap->wAlignFont)
	{
		case 0:
			s += "baseline-align:top;";
			break;
		case 1:
			s += "baseline-align:center;";
			break;
		case 3:
			s += "baseline-align:bottom;";
			break;
		default:
			break; // 2 baseline, 4 auto
	}

	// East Asian / compatibility paragraph toggles, kept as 0/1 props
	// under the same names the docx importer uses; emit only when a
	// sprm moved the value away from its MS-DOC default
	if (apap->fContextualSpacing)
		s += "contextual-spacing:1;";
	if (apap->fMirrorIndents)
		s += "mirror-indents:1;";
	if (apap->fNoLnn)
		s += "suppress-line-numbers:1;";
	if (apap->fNoAutoHyph)
		s += "suppress-auto-hyphens:1;";
	if (apap->fAdjustRight)
		s += "adjust-right-ind:1;";
	if (apap->fTopLinePunct)
		s += "top-line-punct:1;";
	if (!apap->fKinsoku)
		s += "kinsoku:0;";
	if (!apap->fWordWrap)
		s += "word-wrap:0;";
	if (!apap->fOverflowPunct)
		s += "overflow-punct:0;";
	if (!apap->fAutoSpaceDE)
		s += "auto-space-de:0;";
	if (!apap->fAtuoSpaceDN)
		s += "auto-space-dn:0;";

	// remove the trailing semi-colon
	s [s.size()-1] = 0;

}


/*! imports a stylesheet from our document */

#define PT_MAX_ATTRIBUTES 8
void IE_Imp_MsWord_97::_handleStyleSheet(const wvParseStruct *ps)
{
	UT_uint32 iCount = ps->stsh.Stshi.cstd;
//	UT_uint16 iBase  = ps->stsh.Stshi.cbSTDBaseInFile;

	const gchar * attribs[PT_MAX_ATTRIBUTES*2 + 1];
	UT_uint32 iOffset = 0;
	
	const STD * pSTD = ps->stsh.std;
	const STD * pSTDBase = pSTD;
	UT_String props;
	char * s = nullptr;
	char * b = nullptr;
	char * f = nullptr;

	UT_return_if_fail(pSTD != nullptr);

	for(UT_uint32 i = 0; i < iCount; i++, pSTD++)
	{
		iOffset = 0;

		if(!pSTD->xstzName)
		{
			continue;
		}

		if(pSTD->cupx <= 1)
		{
			continue;
		}

		//UT_DEBUGMSG(("Style name: [%s], id: %d\n", pSTD->xstzName, pSTD->sti));

		attribs[iOffset++] = PT_NAME_ATTRIBUTE_NAME;

		// make sure we use standard names for standard styles
		const gchar * pName = s_translateStyleId(pSTD->sti);

		if(pName)
		{
			attribs[iOffset++] = pName;
		}
		else
		{
			s = s_convert_to_utf8(ps, pSTD->xstzName);
			attribs[iOffset++] = s;
		}
		
		UT_DEBUGMSG(("Style name: [%s], id: %d\n", attribs[iOffset-1], pSTD->sti));

		
		attribs[iOffset++] = PT_TYPE_ATTRIBUTE_NAME;
		if(pSTD->sgc == sgcChp)
		{
			attribs[iOffset++] = "C";
		}
		else
		{
			attribs[iOffset++] = "P";

			// also handle the followed-by, since that only applies to
			// paragraph style
			if(pSTD->istdNext != istdNil && pSTD->istdNext<iCount)
			{
				attribs[iOffset++] = PT_FOLLOWEDBY_ATTRIBUTE_NAME;
				const char * t = s_translateStyleId(pSTD->istdNext);
				if(!t)
				{
					t = f = s_convert_to_utf8(ps,(pSTDBase + pSTD->istdNext)->xstzName);					
				}
				attribs[iOffset++] = t;
			}
		}

		if(pSTD->istdBase != istdNil && pSTD->istdBase < iCount)
		{
			attribs[iOffset++] = PT_BASEDON_ATTRIBUTE_NAME;
			const char * t = s_translateStyleId(pSTD->istdBase);
			if(!t)
				t = b = s_convert_to_utf8(ps,(pSTDBase + pSTD->istdBase)->xstzName);
			attribs[iOffset++] = t;
		}
		
		// now we want to generate props
		props.clear();

		wvParseStruct * PS = const_cast<wvParseStruct *>(ps);
		
		CHP achp;
		wvInitCHPFromIstd(&achp, static_cast<U16>(i), &(PS->stsh));
		_generateCharProps(props,&achp,PS);

		if(props.size())
		{
			props += ";";
		}
		
		PAP apap;
		wvInitPAPFromIstd (&apap, static_cast<U16>(i), &(PS->stsh));
		_generateParaProps(props,&apap,PS);

		// remove trailing semicolon
		if(props[props.size()-1] == ';')
		{
			props[props.size()-1] = 0;
		}
		
		xxx_UT_DEBUGMSG(("Style props: %s\n", props.c_str()));

		if(props.size())
		{
			attribs[iOffset++] = PT_PROPS_ATTRIBUTE_NAME;
			attribs[iOffset++] = props.c_str();
		}
		
		attribs[iOffset] = nullptr;

		PD_Style * pStyle = nullptr;
		if(getDoc()->getStyle(pSTD->xstzName, &pStyle))
		{
			xxx_UT_DEBUGMSG(("Redefining style %s\n", pSTD->xstzName));
			pStyle->addAttributes(PP_std_copyProps(attribs));
			pStyle->getBasedOn();
			pStyle->getFollowedBy();
		}
		else
		{
			getDoc()->appendStyle(PP_std_copyProps(attribs));
		}

		FREEP(s);
		FREEP(b);
		FREEP(f);
	}
}

int IE_Imp_MsWord_97::_handleBookmarks(const wvParseStruct *ps)
{
	UT_uint32 i,j;

	if(m_pBookmarks)
	{
		for(i = 0; i < m_iBookmarksCount; i++)
		{
			if(m_pBookmarks[i].name && m_pBookmarks[i].start)
			{
				delete []m_pBookmarks[i].name;
				m_pBookmarks[i].name = nullptr;
			}
		}
		delete [] m_pBookmarks;
	}
	BKF *bkf = nullptr;
	BKL *bkl = nullptr;
	U32 *posf = nullptr, *posl = nullptr, nobkf = 0, nobkl = 0;

	// the wvGet*_PLCF callees free their outputs on failure without
	// NULLing them, so only free on the success paths
	bool bBkfOk = !wvGetBKF_PLCF (&bkf, &posf, &nobkf, ps->fib.fcPlcfbkf, ps->fib.lcbPlcfbkf, ps->tablefd);
	bool bBklOk = !wvGetBKL_PLCF (&bkl, &posl, &nobkl, ps->fib.fcPlcfbkl, ps->fib.lcbPlcfbkl, ps->fib.fcPlcfbkf, ps->fib.lcbPlcfbkf, ps->tablefd);

	if(!bBkfOk || !bBklOk || nobkl != nobkf)
	{
		// a corrupt file can disagree on the bkf/bkl counts; without
		// this reset m_iBookmarksCount would stay > 0 while
		// m_pBookmarks is nullptr and _insertBookmarkIfAppropriate
		// would bsearch() a NULL base
		if(bBkfOk)
		{
			wvFree(bkf);
			wvFree(posf);
		}
		if(bBklOk)
		{
			wvFree(bkl);
			wvFree(posl);
		}
		m_iBookmarksCount = 0;
		return 0;
	}

	m_iBookmarksCount = nobkf + nobkl;
	if(m_iBookmarksCount > 0)
	{
		try
		{
			m_pBookmarks = new bookmark[m_iBookmarksCount];
		}
		catch(...)
		{
			m_pBookmarks = nullptr;
		}

		if(!m_pBookmarks)
		{
			wvFree(bkf);
			wvFree(bkl);
			wvFree(posf);
			wvFree(posl);
			m_iBookmarksCount = 0;
			return 0;
		}
		for(i = 0; i < nobkf; i++)
		{
			m_pBookmarks[i].name = _getBookmarkName(ps, i);
			m_pBookmarks[i].pos  = posf[i];
			m_pBookmarks[i].start = true;
		}

		for(j = i; j < nobkl + i; j++)
		{
			// since the name is shared with the start of the bookmark,
			// we reuse it; ibkf comes from the file so it must be
			// range-checked before it indexes m_pBookmarks
			UT_sint32 iBkf = static_cast<UT_sint32>(bkl[j-i].ibkf) < 0 ? nobkl + static_cast<UT_sint32>(bkl[j-i].ibkf) : bkl[j-i].ibkf;
			if(iBkf < 0 || iBkf >= static_cast<UT_sint32>(i))
			{
				m_pBookmarks[j].name = nullptr;
			}
			else
			{
				m_pBookmarks[j].name = m_pBookmarks[iBkf].name;
			}
			m_pBookmarks[j].pos  = posl[j - i];
			m_pBookmarks[j].start = false;
		}
		// g_free bkf, bkl, posf, posl
		wvFree(bkf);
		wvFree(bkl);
		wvFree(posf);
		wvFree(posl);

		//now sort the bookmarks by position
		qsort(static_cast<void*>(m_pBookmarks),
			  m_iBookmarksCount, sizeof(bookmark),
			  s_cmp_bookmarks_qsort);
		
#ifdef DEBUG
		for(UT_uint32 k = 0; k < m_iBookmarksCount; k++)
		{
			UT_DEBUGMSG(("Bookmark: name [%s], pos %d, start %d\n",
						 m_pBookmarks[k].name,m_pBookmarks[k].pos,m_pBookmarks[k].start));
		}

#endif
	}
	return 0;
}

void IE_Imp_MsWord_97::_freeFields()
{
	for(int i = 0; i < FLDSTORY_COUNT; i++)
	{
		wvFree(m_aStoryFlds[i].cps);
		wvFree(m_aStoryFlds[i].flds);
		m_aStoryFlds[i].count = 0;
	}
	m_mapFieldHlink.clear();
}

/*!
 * reads each document part's Plcfld (MS-DOC 2.8.25) and resolves the
 * _PID_HLINKS hyperlink properties onto their fields (MS-DOC 2.4.7)
 */
void IE_Imp_MsWord_97::_handleFields(const wvParseStruct *ps)
{
	_freeFields();

	// FIB offsets per story, in FLDSTORY_* order
	static const struct { S32 FIB::*fc; U32 FIB::*lcb; } s_fibFld[FLDSTORY_COUNT] = {
		{&FIB::fcPlcffldMom,     &FIB::lcbPlcffldMom},
		{&FIB::fcPlcffldFtn,     &FIB::lcbPlcffldFtn},
		{&FIB::fcPlcffldHdr,     &FIB::lcbPlcffldHdr},
		{&FIB::fcPlcffldMcr,     &FIB::lcbPlcffldMcr},
		{&FIB::fcPlcffldAtn,     &FIB::lcbPlcffldAtn},
		{&FIB::fcPlcffldEdn,     &FIB::lcbPlcffldEdn},
		{&FIB::fcPlcffldTxbx,    &FIB::lcbPlcffldTxbx},
		{&FIB::fcPlcffldHdrTxbx, &FIB::lcbPlcffldHdrTxbx},
	};
	for(UT_sint32 s = 0; s < FLDSTORY_COUNT; s++)
	{
		FLD *flds = nullptr;
		U32 *cps = nullptr, n = 0;
		if(!wvGetFLD_PLCF(&flds, &cps, &n,
						 static_cast<U32>(ps->fib.*(s_fibFld[s].fc)),
						 ps->fib.*(s_fibFld[s].lcb), ps->tablefd))
		{
			m_aStoryFlds[s].cps = cps;
			m_aStoryFlds[s].flds = flds;
			m_aStoryFlds[s].count = n;
		}
	}

	if(m_vecHyperlinks.empty())
		return;

	// index-typed dwApp elements are grouped by document part in this
	// order (MS-DOC 2.4.7), largest index first inside each group; so a
	// dwApp that is a hyperlink-field index in several stories belongs
	// to the earliest group that has not consumed it yet
	static const UT_sint32 s_hlinkGroups[] = {
		FLDSTORY_MOM, FLDSTORY_FTN, FLDSTORY_HDR, FLDSTORY_ATN,
		FLDSTORY_EDN, FLDSTORY_TXBX, FLDSTORY_HDRTXBX
	};
	for(MsHyperlink & hl : m_vecHyperlinks)
	{
			if(hl.dwApp == 0xffffffff)
			continue;	// hyperlink of an OfficeArt shape (dwOfficeArt holds the spid)
		for(UT_sint32 g : s_hlinkGroups)
		{
			const storyFields *sf = &m_aStoryFlds[g];
			UT_uint64 key = (static_cast<UT_uint64>(g) << 32) | hl.dwApp;
			if(hl.dwApp < sf->count &&
			   sf->flds[hl.dwApp].var1.ch == 0x13 &&
			   sf->flds[hl.dwApp].var1.flt == 0x58 /* flt: HYPERLINK */ &&
			   m_mapFieldHlink.find(key) == m_mapFieldHlink.end())
			{
				std::string url = hl.target;
				// a relative target resolves against _PID_LINKBASE
				if(!url.empty() && !m_sLinkBase.empty() &&
				   url.find(':') == std::string::npos &&
				   url[0] != '#' && url[0] != '/')
					url = m_sLinkBase + url;
				if(!hl.location.empty())
				{
					url += '#';
					url += hl.location;
				}
				m_mapFieldHlink[key] = url;
				hl.used = true;
				break;
			}
		}
	}
}

/*!
 * locates the field-begin character at document position \a cp in its
 * document part's Plcfld; on success returns true and fills \a story
 * and \a index (the Plcfld element index used by _PID_HLINKS dwApp)
 */
bool IE_Imp_MsWord_97::_fldIndexAtCp(UT_uint32 cp, UT_sint32 *story,
									 UT_sint32 *index) const
{
	UT_sint32 s;
	UT_uint32 rel;
	if(cp >= m_iTextStart && cp < m_iTextEnd)
		{ s = FLDSTORY_MOM; rel = cp - m_iTextStart; }
	else if(cp >= m_iFootnotesStart && cp < m_iFootnotesEnd)
		{ s = FLDSTORY_FTN; rel = cp - m_iFootnotesStart; }
	else if(cp >= m_iHeadersStart && cp < m_iHeadersEnd)
		{ s = FLDSTORY_HDR; rel = cp - m_iHeadersStart; }
	else if(cp >= m_iMacrosStart && cp < m_iMacrosEnd)
		{ s = FLDSTORY_MCR; rel = cp - m_iMacrosStart; }
	else if(cp >= m_iAnnotationsStart && cp < m_iAnnotationsEnd)
		{ s = FLDSTORY_ATN; rel = cp - m_iAnnotationsStart; }
	else if(cp >= m_iEndnotesStart && cp < m_iEndnotesEnd)
		{ s = FLDSTORY_EDN; rel = cp - m_iEndnotesStart; }
	else if(cp >= m_iTextboxesStart && cp < m_iTextboxesEnd)
		{ s = FLDSTORY_TXBX; rel = cp - m_iTextboxesStart; }
	else
		{ s = FLDSTORY_HDRTXBX; rel = cp - m_iTextboxesEnd; }

	*story = s;
	*index = -1;

	const storyFields *sf = &m_aStoryFlds[s];
	if(!sf->cps || !sf->count)
		return false;

	// cps is sorted ascending (PLC)
	UT_uint32 lo = 0, hi = sf->count;
	while(lo < hi)
	{
		UT_uint32 mid = (lo + hi) / 2;
		if(sf->cps[mid] < rel)
			lo = mid + 1;
		else
			hi = mid;
	}
	if(lo < sf->count && sf->cps[lo] == rel &&
	   sf->flds[lo].var1.ch == 0x13)
	{
		*index = static_cast<UT_sint32>(lo);
		return true;
	}
	return false;
}

/*!
 * returns the _PID_HLINKS-resolved target for the hyperlink field whose
 * begin character is element \a index of story \a story's Plcfld, or ""
 */
std::string IE_Imp_MsWord_97::_hyperlinkForField(UT_sint32 story, UT_sint32 index)
{
	if(story < 0 || index < 0 || m_mapFieldHlink.empty())
		return std::string();
	auto it = m_mapFieldHlink.find((static_cast<UT_uint64>(story) << 32) |
								   static_cast<UT_uint32>(index));
	if(it == m_mapFieldHlink.end())
		return std::string();
	return it->second;
}

static UT_uint16 s_propRd16(const guint8 *p)
{
	return static_cast<UT_uint16>(p[0] | (p[1] << 8));
}
static UT_uint32 s_propRd32(const guint8 *p)
{
	return static_cast<UT_uint32>(p[0]) | (static_cast<UT_uint32>(p[1]) << 8) |
		(static_cast<UT_uint32>(p[2]) << 16) | (static_cast<UT_uint32>(p[3]) << 24);
}

/*!
 * reads a TypedPropertyValue string (VT_LPWSTR/VT_LPSTR) starting at
 * \a off, returns it as UTF-8 and stores the offset just past it in
 * \a next; returns false if the blob is malformed
 */
static bool s_propReadString(const guint8 *buf, gsize sz, UT_uint32 off,
							 UT_uint32 *next, std::string & out)
{
	out.clear();
	if(off + 8 > sz)
		return false;
	UT_uint16 wt = s_propRd16(&buf[off]);
	UT_uint32 p = off + 4;
	UT_uint32 cch = s_propRd32(&buf[p]);
	p += 4;
	if(wt == 0x001f)	// VT_LPWSTR
	{
		if(cch > (sz - p) / 2)
			return false;
		UT_UTF8String s;
		for(UT_uint32 i = 0; i < cch; i++)
		{
			UT_UCS2Char c = s_propRd16(&buf[p + 2 * i]);
			if(c)
				s.appendUCS2(&c, 1);
		}
		out = s.utf8_str();
		// an Lpwstr is padded to a multiple of 4 bytes
		UT_uint32 adv = (4 + 2 * cch + 3) & ~3u;
		*next = off + 4 + adv;
		return true;
	}
	if(wt == 0x001e)	// VT_LPSTR
	{
		if(cch > sz - p)
			return false;
		for(UT_uint32 i = 0; i < cch && buf[p + i]; i++)
			out += static_cast<char>(buf[p + i]);
		UT_uint32 adv = (4 + cch + 3) & ~3u;
		*next = off + 4 + adv;
		return true;
	}
	return false;
}

/*!
 * parses the \005DocumentSummaryInformation property set for the
 * user-defined _PID_HLINKS (VtHyperlinks, [MS-OSHARED] 2.3.3.1.21) and
 * _PID_LINKBASE properties; hyperlink targets are bound to fields once
 * the Plcflds have been read in _handleFields()
 */
void IE_Imp_MsWord_97::_parseHyperlinkProps(const wvParseStruct *ps)
{
	m_vecHyperlinks.clear();
	m_sLinkBase.clear();

	if(!ps->ole_file || !GSF_IS_INFILE(ps->ole_file))
		return;
	GsfInput *st = gsf_infile_child_by_name(GSF_INFILE(ps->ole_file),
										  "\005DocumentSummaryInformation");
	if(!st)
		return;
	// gsf_input_size is signed and can be -1 on error; casting that
	// to gsize would wrap huge and make new[] throw.
	const gsf_off_t rawSz = gsf_input_size(st);
	gsize sz = (rawSz > 0) ? static_cast<gsize>(rawSz) : 0;
	guint8 *buf = sz ? new guint8[sz] : nullptr;
	if(buf && !gsf_input_read(st, sz, buf))
	{
		delete [] buf;
		buf = nullptr;
	}
	g_object_unref(G_OBJECT(st));
	if(!buf)
		return;

	// property set header (MS-OLEPS 2.20)
	if(sz < 48 || s_propRd16(buf) != 0xFFFE)
		goto fail;
	{
		// locate the user-defined section (FMTID
		// {D5CDD505-2E9C-101B-9397-08002B2CF9AE})
		static const guint8 s_userFmtid[16] =
			{0x05, 0xD5, 0xCD, 0xD5, 0x9C, 0x2E, 0x1B, 0x10,
			 0x93, 0x97, 0x08, 0x00, 0x2B, 0x2C, 0xF9, 0xAE};
		UT_uint32 cSections = s_propRd32(&buf[24]);
		UT_uint32 sectOff = 0;
		for(UT_uint32 i = 0; i < cSections && i < 8; i++)
		{
			if(28 + i * 20 + 20 > sz)
				break;
			if(!memcmp(buf + 28 + i * 20, s_userFmtid, 16))
			{
				sectOff = s_propRd32(&buf[28 + i * 20 + 16]);
				break;
			}
		}
		if(!sectOff || sectOff + 8 > sz)
			goto fail;

		UT_uint32 cProps = s_propRd32(&buf[sectOff + 4]);
		if(cProps > (sz - sectOff - 8) / 8)
			goto fail;

		UT_uint32 dictOff = 0, hlinksPid = 0, linkBasePid = 0;
		for(UT_uint32 i = 0; i < cProps; i++)
		{
			if(s_propRd32(&buf[sectOff + 8 + i * 8]) == 0)
				dictOff = s_propRd32(&buf[sectOff + 8 + i * 8 + 4]);
		}
		if(!dictOff || sectOff + dictOff + 4 > sz)
			goto fail;

		// the Dictionary property maps property names to propIds
		{
			UT_uint32 nEnt = s_propRd32(&buf[sectOff + dictOff]);
			UT_uint32 pos = sectOff + dictOff + 4;
			for(UT_uint32 k = 0; k < nEnt && pos + 8 <= sz; k++)
			{
				UT_uint32 pid = s_propRd32(&buf[pos]);
				UT_uint32 len = s_propRd32(&buf[pos + 4]);
				pos += 8;
				bool uni = (pid & 0x80000000) != 0;
				UT_uint32 nbytes = uni ? len * 2 : len;
				if(nbytes > sz - pos)
					break;
				UT_UTF8String sName;
				if(uni)
				{
					for(UT_uint32 i = 0; i < len; i++)
					{
						UT_UCS2Char c = s_propRd16(&buf[pos + 2 * i]);
						if(c)
							sName.appendUCS2(&c, 1);
					}
				}
				else
				{
					for(UT_uint32 i = 0; i < len && buf[pos + i]; i++)
						sName += static_cast<char>(buf[pos + i]);
				}
				pos += (nbytes + 3) & ~3u;
				if(sName == "_PID_HLINKS")
					hlinksPid = pid & 0x7fffffff;
				else if(sName == "_PID_LINKBASE")
					linkBasePid = pid & 0x7fffffff;
			}
		}

		auto propOffset = [&](UT_uint32 pid) -> UT_uint32 {
			for(UT_uint32 i = 0; i < cProps; i++)
				if(s_propRd32(&buf[sectOff + 8 + i * 8]) == pid)
					return s_propRd32(&buf[sectOff + 8 + i * 8 + 4]);
			return 0;
		};

		// _PID_LINKBASE (Lpstr/Lpwstr): base for relative targets
		if(linkBasePid)
		{
			UT_uint32 off = propOffset(linkBasePid);
			if(off && sectOff + off < sz)
			{
				UT_uint32 next;
				s_propReadString(buf, sz, sectOff + off, &next, m_sLinkBase);
			}
		}

		// _PID_HLINKS: VT_BLOB TypedPropertyValue wrapping VecVtHyperlink
		if(!hlinksPid)
			goto fail;
		{
			UT_uint32 off = propOffset(hlinksPid);
			if(!off || sectOff + off + 12 > sz)
				goto fail;
			UT_uint32 p = sectOff + off;
			if(s_propRd16(&buf[p]) != 0x0041)	// VT_BLOB
				goto fail;
			UT_uint32 cbData = s_propRd32(&buf[p + 4]);
			p += 8;
			if(cbData > sz - p)
				cbData = sz - p;
			UT_uint32 blobEnd = p + cbData;
			if(p + 4 > blobEnd)
				goto fail;
			// VecVtHyperlink: cElements counts the 6 values of each
			// VtHyperlink (dwHash, dwApp, dwOfficeArt, dwInfo, hlink1,
			// hlink2)
			UT_uint32 nH = s_propRd32(&buf[p]) / 6;
			p += 4;
			for(UT_uint32 i = 0; i < nH && p + 32 <= blobEnd; i++)
			{
				MsHyperlink hl;
				// four VT_I4 TypedPropertyValues (wType, pad, value)
				for(int v = 0; v < 4; v++)
				{
					if(s_propRd16(&buf[p]) != 0x0003)
						goto fail;
					p += 8;
				}
				hl.dwApp       = s_propRd32(&buf[p - 20]);
				hl.dwOfficeArt = s_propRd32(&buf[p - 12]);
				hl.dwInfo      = s_propRd32(&buf[p - 4]);
				UT_uint32 next;
				if(!s_propReadString(buf, blobEnd, p, &next, hl.target))
					goto fail;
				p = next;
				if(!s_propReadString(buf, blobEnd, p, &next, hl.location))
					goto fail;
				p = next;
				hl.used = false;
				m_vecHyperlinks.push_back(hl);
			}
		}
	}

	UT_DEBUGMSG(("DOM: %u hyperlink properties, linkbase '%s'\n",
				 static_cast<unsigned>(m_vecHyperlinks.size()), m_sLinkBase.c_str()));
	goto out;
fail:
	m_vecHyperlinks.clear();
	m_sLinkBase.clear();
out:
	delete [] buf;
}

void IE_Imp_MsWord_97::_handleNotes(const wvParseStruct *ps)
{
	UT_uint32 i;

	DELETEPV(m_pFootnotes);
	DELETEPV(m_pEndnotes);

	m_iFootnotesCount = 0;
	m_iEndnotesCount = 0;
	UT_uint32 *pPLCF_ref = nullptr;
	UT_uint32 *pPLCF_txt = nullptr;

	bool bNoteError = false;

	if(ps->fib.lcbPlcffndTxt >= 8)
	{
		/* the docs say -1, but that is an error; lcb < 8 would
		   underflow the count to ~4G entries */
		m_iFootnotesCount = ps->fib.lcbPlcffndTxt/4 - 2;
		/* the reference PLCF holds n+1 U32 positions plus n U16
		   flags, i.e. 6n+4 bytes -- never let the count ask for
		   more than it actually contains */
		if(4*(m_iFootnotesCount+1) + 2*m_iFootnotesCount > ps->fib.lcbPlcffndRef)
		{
			m_iFootnotesCount = ps->fib.lcbPlcffndRef >= 4 ? (ps->fib.lcbPlcffndRef-4)/6 : 0;
		}
		try
		{
			m_pFootnotes = new footnote[m_iFootnotesCount];
		}
		catch(...)
		{
			m_pFootnotes = nullptr;
		}

		if(!m_pFootnotes)
		{
			m_iFootnotesCount = 0;
			return;
		}
		
		// this is really quite straight forward; we retrieve the PLCF
		// chunks that describe the references/text of the footnotes, and
		// then use those to init our footnote stucts
		// for n footnotes the reference PLCF is a sequnce of (n+1) doc
		// positions (UT_uint32) followed by n type flags (UT_uint16)
		// the text PLCF is a sequence of n+2 positions (UT_uint32) of the footnote
		// text in its data stream 
		if(wvGetPLCF(&pPLCF_ref, ps->fib.fcPlcffndRef, ps->fib.lcbPlcffndRef, ps->tablefd))
		{
			bNoteError = true;
		}

		if(!bNoteError &&
		   wvGetPLCF(&pPLCF_txt, ps->fib.fcPlcffndTxt, ps->fib.lcbPlcffndTxt, ps->tablefd))
		{
			wvFree(pPLCF_ref);
			bNoteError = true;
		}
	
		if(!bNoteError)
		{
			/* a zero-length PLCF loads as NULL without flagging an
			   error; free whichever sibling was read and leave no
			   count over the uninitialized notes array */
			if(!pPLCF_ref || !pPLCF_txt)
			{
				wvFree(pPLCF_ref);
				wvFree(pPLCF_txt);
				m_iFootnotesCount = 0;
				return;
			}
			for(i = 0; i < m_iFootnotesCount; i++)
			{
				m_pFootnotes[i].ref_pos = pPLCF_ref[i];
				m_pFootnotes[i].txt_pos = pPLCF_txt[i] + m_iFootnotesStart;
				m_pFootnotes[i].txt_len = pPLCF_txt[i+1] - pPLCF_txt[i];
				// idx is an index of int16.
				size_t idx = 2 * (m_iFootnotesCount + 1) + i;
				// If you hit this assert, congratulation, you found a buggy file
				//
				UT_ASSERT(idx * 2 < ps->fib.lcbPlcffndRef);
				if (idx * 2 >= ps->fib.lcbPlcffndRef) {
					bNoteError = true;
					// We are done with the footnotes here.
					// This is as graceful as it can be.
					m_iFootnotesCount--;
					break;
				}
				const UT_Byte * pRef8 = reinterpret_cast<const UT_Byte *>(pPLCF_ref);
				UT_uint32 iType = pRef8[2*idx] | (static_cast<UT_uint32>(pRef8[2*idx+1]) << 8);
				m_pFootnotes[i].type = iType;
				m_pFootnotes[i].pid = getDoc()->getUID(UT_UniqueId::Footnote);
				UT_DEBUGMSG(("IE_Imp_MsWord_97::_handleNotes: fnote %d, rpos %d, tpos %d, type %d\n",
							 i, m_pFootnotes[i].ref_pos, m_pFootnotes[i].txt_pos, iType));
			}

			wvFree(pPLCF_ref);
			wvFree(pPLCF_txt);
		}

		// next, deal footnote formatting matters
		PP_PropertyVector props = {
			"document-footnote-type",            "",
			"document-footnote-initial", UT_std_string_sprintf("%d", ps->dop.nFtn),
			"document-footnote-restart-section", "",
			"document-footnote-restart-page",    "",
		};

		switch(ps->dop.rncFtn)
		{
			case 0:
				props[5] = "0";
				props[7] = "0";
				break;
			case 1:
				props[5] = "1";
				props[7] = "0";
				break;
			case 2:
				props[5] = "0";
				props[7] = "1";
				break;
			default:
				UT_ASSERT_HARMLESS(UT_NOT_REACHED);
		}

		switch(ps->dop.nfcFtnRef)
		{
			case 0:
				props[1] = "numeric";
				break;
			case 1:
				props[1] = "upper-roman";
				break;
			case 2:
				props[1] = "lower-roman";
				break;
			case 3:
				props[1] = "upper";
				break;
			case 4:
				props[1] = "lower";
				break;
			default:
				UT_ASSERT_HARMLESS(UT_NOT_REACHED);
				props[1] = "";
				break;
		}

		getDoc()->setProperties(props);
	}

	if(ps->fib.lcbPlcfendTxt >= 8)
	{
		m_iEndnotesCount  = ps->fib.lcbPlcfendTxt/4 - 2;
		if(4*(m_iEndnotesCount+1) + 2*m_iEndnotesCount > ps->fib.lcbPlcfendRef)
		{
			m_iEndnotesCount = ps->fib.lcbPlcfendRef >= 4 ? (ps->fib.lcbPlcfendRef-4)/6 : 0;
		}
		try
		{
			m_pEndnotes  = new footnote[m_iEndnotesCount];
		}
		catch(...)
		{
			m_pEndnotes = nullptr;
		}

		if(!m_pEndnotes)
		{
			m_iEndnotesCount = 0;
			return;
		}

		bNoteError = false;
		if(wvGetPLCF(&pPLCF_ref, ps->fib.fcPlcfendRef, ps->fib.lcbPlcfendRef, ps->tablefd))
		{
			bNoteError = true;
		}

		if(!bNoteError &&
		   wvGetPLCF(&pPLCF_txt, ps->fib.fcPlcfendTxt, ps->fib.lcbPlcfendTxt, ps->tablefd))
		{
			wvFree(pPLCF_ref);
			bNoteError = true;
		}

		if(!bNoteError)
		{
			if(!pPLCF_ref || !pPLCF_txt)
			{
				wvFree(pPLCF_ref);
				wvFree(pPLCF_txt);
				m_iEndnotesCount = 0;
				return;
			}
			for(i = 0; i < m_iEndnotesCount; i++)
			{
				m_pEndnotes[i].ref_pos = pPLCF_ref[i];
				m_pEndnotes[i].txt_pos = pPLCF_txt[i] + m_iEndnotesStart;
				m_pEndnotes[i].txt_len = pPLCF_txt[i+1] - pPLCF_txt[i];
				// idx is an index of int16.
				size_t idx = 2 * (m_iEndnotesCount + 1) + i;
				// If you hit this assert, congratulation, you found a buggy file
				//
				UT_ASSERT(idx * 2 < ps->fib.lcbPlcfendRef);
				if (idx * 2 >= ps->fib.lcbPlcfendRef) {
					bNoteError = true;
					// We are done with the endnotes here.
					// This is as graceful as it can be.
					m_iEndnotesCount--;
					break;
				}
				const UT_Byte * pRef8 = reinterpret_cast<const UT_Byte *>(pPLCF_ref);
				UT_uint32 iType = pRef8[2*idx] | (static_cast<UT_uint32>(pRef8[2*idx+1]) << 8);
				m_pEndnotes[i].type = iType;
				m_pEndnotes[i].pid = getDoc()->getUID(UT_UniqueId::Endnote);
				UT_DEBUGMSG(("IE_Imp_MsWord_97::_handleNotes: enote %d, rpos %d, tpos %d, type %d\n",
							 i, m_pEndnotes[i].ref_pos, m_pEndnotes[i].txt_pos, iType));
			}

			wvFree(pPLCF_ref);
			wvFree(pPLCF_txt);
		}
		// next, deal endnote formatting matters
		PP_PropertyVector props = {
			"document-endnote-type",            "",
			"document-endnote-initial", UT_std_string_sprintf("%d", ps->dop.nEdn),
			"document-endnote-restart-section", "",
			"document-endnote-restart-page",    "",
			"document-endnote-place-endsection","",
			"document-endnote-place-enddoc",    "",
		};

		switch(ps->dop.rncEdn)
		{
			case 0:
				props[5] = "0";
				props[7] = "0";
				break;
			case 1:
				props[5] = "1";
				props[7] = "0";
				break;
			case 2:
				props[5] = "0";
				props[7] = "1";
				break;

			default:
				UT_ASSERT_HARMLESS(UT_NOT_REACHED);
		}

		switch(ps->dop.nfcEdnRef)
		{
			case 0:
				props[1] = "numeric";
				break;
			case 1:
				props[1] = "upper-roman";
				break;
			case 2:
				props[1] = "lower-roman";
				break;
			case 3:
				props[1] = "upper";
				break;
			case 4:
				props[1] = "lower";
				break;

			default:
				UT_ASSERT_HARMLESS(UT_NOT_REACHED);
		}

		switch(ps->dop.epc)
		{
			case 0:
				props[9]  = "1";
				props[11] = "0";
				break;
			case 3:
				props[9]  = "0";
				props[11] = "1";
				break;
			default:
				UT_ASSERT_HARMLESS(UT_NOT_REACHED);
		}

		getDoc()->setProperties(props);
	}
}

void IE_Imp_MsWord_97::_handleTextBoxes(const wvParseStruct *ps)
{
	DELETEPV(m_pTextboxes);
	m_iTextboxCount = 0;

	if(ps->fib.ccpTxbx == 0)
	{
		return;
	}

	// MS-DOC 2.4.1: PlcftxbxTxt is a PLC of (n+1) CPs into the
	// Textboxes subdocument followed by n FTXBXS records; the lid
	// of each FTXBXS is the spid of the OfficeArt shape (and of the
	// main-story FSPA anchor) that displays this text range
	FTXBXS * pFTXBXS = nullptr;
	U32 *    pPLCF_txt = nullptr;
	U32      nFTXBXS = 0;

	if(wvGetFTXBXS_PLCF(&pFTXBXS, &pPLCF_txt, &nFTXBXS,
					   ps->fib.fcPlcftxbxTxt,
					   ps->fib.lcbPlcftxbxTxt, ps->tablefd))
	{
		UT_DEBUGMSG(("IE_Imp_MsWord_97::_handleTextBoxes: bad PlcftxbxTxt\n"));
		return;
	}
	if(!pFTXBXS || !pPLCF_txt)
	{
		wvFree(pFTXBXS);
		wvFree(pPLCF_txt);
		return;
	}

	/* the last CPs of the PLC delimit the "unused" padding at the
	 * end of the story (MS-DOC 2.4.1); only keep ranges that end
	 * before the story's last paragraph mark */
	const UT_uint32 iEndUsable = (m_iTextboxesEnd > 2) ? m_iTextboxesEnd - 2 : m_iTextboxesEnd;
	while(nFTXBXS > 0 &&
		  pPLCF_txt[nFTXBXS] + m_iTextboxesStart > iEndUsable)
	{
		nFTXBXS--;
	}

	m_pTextboxes = new textbox [nFTXBXS ? nFTXBXS : 1];
	for(U32 i = 0; i < nFTXBXS; i++)
	{
		if(pFTXBXS[i].fReusable)
		{
			// a reusable FTXBXS holds spare text for a text box
			// chain; its range does not belong to its shape
			continue;
		}
		m_pTextboxes[m_iTextboxCount].lid = pFTXBXS[i].lid;
		m_pTextboxes[m_iTextboxCount].txt_pos = pPLCF_txt[i] + m_iTextboxesStart;
		m_pTextboxes[m_iTextboxCount].txt_len = pPLCF_txt[i + 1] - pPLCF_txt[i];
		UT_DEBUGMSG(("IE_Imp_MsWord_97::_handleTextBoxes: Tbox %d, lid %u, tpos %d len %d \n",
					 m_iTextboxCount, m_pTextboxes[m_iTextboxCount].lid,
					 m_pTextboxes[m_iTextboxCount].txt_pos,
					 m_pTextboxes[m_iTextboxCount].txt_len));
		m_iTextboxCount++;
	}

	wvFree(pFTXBXS);
	wvFree(pPLCF_txt);
}

/*!
   Determines whether footnote is to be inserted at present document
   position, and if so takes care of inserting the reference marker,
   note section and anchor marker.

   returns true if a note was successfully inserted, false otherwise;
   if the return value is true, the caller should ignore the present character
   
   we will take advantage of the notes being in document order, so we
   can just remember the last note we inserted, rather than having to
   search through the list

*/
bool IE_Imp_MsWord_97::_insertNoteIfAppropriate(UT_uint32 iDocPosition, UT_UCS4Char c)
{
	if(m_bInFNotes || m_bInENotes)
		return false;
	
	bool res = false;
	//now search for position iDocPosition in our footnnote list;
	if(!m_pFootnotes || m_iFootnotesCount == 0 || m_iNextFNote >= m_iFootnotesCount)
	{
		goto endnotes;
	}

	if(m_pFootnotes[m_iNextFNote].ref_pos == iDocPosition)
	{
		res |= _insertFootnote(m_pFootnotes + m_iNextFNote++,c);
	}
	
 endnotes:
	if(!m_pEndnotes || m_iEndnotesCount == 0 || m_iNextENote >= m_iEndnotesCount)
	{
		goto finish;
	}
	
	if(m_pEndnotes[m_iNextENote].ref_pos == iDocPosition)
	{
		res |= _insertEndnote(m_pEndnotes + m_iNextENote++,c);
	}
	
	
 finish:	
	return res;
}

/* returns true on successful insertion of the reference marker */
bool IE_Imp_MsWord_97::_insertFootnote(const footnote * f, UT_UCS4Char c)
{
	UT_return_val_if_fail(f, true);
	xxx_UT_DEBUGMSG(("IE_Imp_MsWord_97::_insertFootnote: pos: %d, pid %d\n", f->ref_pos, f->pid));

	this->_flush();

	bool res = true;

	std::string footpid = UT_std_string_sprintf("%i", f->pid);
	const PP_PropertyVector attribsS = { "footnote-id", footpid };

	// for attribsR we need to set props and style in order to
	// preserve any formating set by a previous call to _beginChar()
	PP_PropertyVector attribsR = {
		"type", "footnote_ref",
		"footnote-id", footpid,
		"props", m_charProps.c_str()
	};
	if(!m_charStyle.empty())
	{
		attribsR.push_back("style");
		attribsR.push_back(m_charStyle.c_str());
	}

	if(f->type)
	{
		// auto-generated reference -- insert a field
		res &= _appendObject(PTO_Field, attribsR);
	}
	else
	{
		// manually-inserted marker, we need to issue the character
		// TODO -- in word the marker can consist of several
		// characters, but I have no idea how Word knows how many;
		// we at least need to reset the character formatting again
		// after we have inserted the footnote section
		res &= _appendSpan(&c,1);
	}

	_appendStrux(PTX_SectionFootnote,attribsS);
	_appendStrux(PTX_EndFootnote, PP_NOPROPS);

	if(!f->type)
	{
		// set the formatting to whatever it was, in case the footnote
		// marker is longer than one character
		PP_PropertyVector attribsF = {
			"props", m_charProps.c_str()
		};
		if(!m_charStyle.empty())
		{
			attribsF.push_back("style");
			attribsF.push_back(m_charStyle.c_str());
		}
		_appendFmt(attribsF);
	}

	return res;
}

bool IE_Imp_MsWord_97::_insertEndnote(const footnote * f, UT_UCS4Char c)
{
	UT_return_val_if_fail(f, true);
	xxx_UT_DEBUGMSG(("IE_Imp_MsWord_97::_insertEndnote: pos: %d, pid %d\n", f->ref_pos, f->pid));

	this->_flush();

	bool res = true;

	std::string footpid = UT_std_string_sprintf("%i", f->pid);
	const PP_PropertyVector attribsS = {
		"endnote-id", footpid
	};
	// for attribsR we need to set props and style in order to
	// preserve any formating set by a previous call to _beginChar()
	const PP_PropertyVector attribsR = {
		"type", "endnote_ref", "endnote-id", footpid,
		"props", m_charProps.c_str(),
		"style", m_charStyle.c_str()
	};

	if(f->type)
	{
		// auto-generated reference -- insert a field
		res &= _appendObject(PTO_Field, attribsR);
	}
	else
	{
		// manually-inserted marker, we need to issue the character
		// TODO -- in word the marker can consist of several
		// characters, but I have no idea how Word knows how many;
		// we at least need to reset the character formatting again
		// after we have inserted the footnote section
		res &= _appendSpan(&c,1);
	}

	_appendStrux(PTX_SectionEndnote,attribsS);
	_appendStrux(PTX_EndEndnote, PP_NOPROPS);

	if(!f->type)
	{
		// set the formatting to whatever it was, in case the footnote
		// marker is longer than one character
		PP_PropertyVector attribsF = {
			"props", m_charProps.c_str()
		};
		if(!m_charStyle.empty())
		{
			attribsF.push_back("style");
			attribsF.push_back(m_charStyle.c_str());
		}
		_appendFmt(attribsF);
	}

	return res;
}


/*!
    This function makes sure that the insert is happening at the
    correct place if we are in a segment which belongs to one of the
    set of notes (foonotes & endnote, in future also annotations).

    \parameter UT_uint32 iDocPosition: character position in the Word
                                       document stream
    \return returns false if the present character is to be skipped,
            true otherwise
*/
bool IE_Imp_MsWord_97::_handleNotesText(UT_uint32 iDocPosition)
{
	if(iDocPosition >= m_iFootnotesStart && iDocPosition < m_iFootnotesEnd)
	{
		// upon entry into the footnote-land, we will need to search for
		// the first footnote section in our document, note that we are
		// in a footnote section, note at what doc position the current
		// footnote will end, and then let things run until we reach
		// the end of the note; then we need to search for the next
		// doc section, etc.

		// if the footnote marker is auto-generated, we need to remove
		// the special character from the stream (happens
		// automatically)

		// when in a footnote section, all the functions that normally
		// use append methods will need to use insert methods instead

		if(!m_bInFNotes)
		{
			xxx_UT_DEBUGMSG(("In footnote territory: pos %d\n", iDocPosition));
			m_bInFNotes = true;
			m_bInHeaders = false;
			
			// we will reuse the m_iNextFNote variable, noting it
			// refers to the CURRENT footnote
			m_iNextFNote = 0;
			_findNextFNoteSection();
			_endSect(nullptr,0,nullptr,0);
			m_bInSect = true;
		}

		// the current footnote will end at pos
		// f.txt_pos + f.txt_len, 
		if( m_iNextFNote < m_iFootnotesCount && iDocPosition == m_pFootnotes[m_iNextFNote].txt_pos +
		                                                        m_pFootnotes[m_iNextFNote].txt_len)
		{
			m_iNextFNote++;

			// after the last footnote there is an extra paragraph
			// marker that is still a part of the footnote section --
			// we do not want that marker imported
			if(m_iNextFNote < m_iFootnotesCount)
				_findNextFNoteSection();
			else
			{
				UT_DEBUGMSG(("End of footnotes marker at pos %d\n", iDocPosition));
				return false;
			}
		}

		// if this is the first character in a footnote, insert the reference
		if(iDocPosition == m_pFootnotes[m_iNextFNote].txt_pos)
		{
			std::string footpid =
				UT_std_string_sprintf("%i", m_pFootnotes[m_iNextFNote].pid);
			const PP_PropertyVector attribsA = {
				"type", "footnote_anchor",
				"footnote-id", footpid,
				"props",       m_charProps.c_str(),
				"style",       m_charStyle.c_str()
			};

			const PP_PropertyVector attribsB = {
				"props", m_paraProps.c_str(),
				"style", m_paraStyle.c_str()
			};

			_appendStrux(PTX_Block, attribsB);
			m_bInPara = true;

			if(m_pFootnotes[m_iNextFNote].type)
			{
				_appendObject(PTO_Field, attribsA);
				return false;
			}
			return true;
		}

		// do not return !!!
		xxx_UT_DEBUGMSG(("In footnote %d, on pos %d\n", m_iNextFNote, iDocPosition));
	}
	else if(m_bInFNotes)
	{
		m_bInFNotes = false;
		xxx_UT_DEBUGMSG(("Leaving footnote territory\n"));
		// move to the end of the do end of the document ...

		// do not return !!!
	}
	
	if(iDocPosition >= m_iEndnotesStart && iDocPosition < m_iEndnotesEnd)
	{
		if(!m_bInENotes)
		{
			xxx_UT_DEBUGMSG(("In endnote territory: pos %d\n", iDocPosition));
			m_bInENotes = true;
			m_bInHeaders = false;
			m_iNextENote = 0;
			_findNextENoteSection();
			_endSect(nullptr,0,nullptr,0);
			m_bInSect = true;
		}

		if( m_iNextENote < m_iEndnotesCount && iDocPosition == m_pEndnotes[m_iNextENote].txt_pos +
		                   m_pEndnotes[m_iNextENote].txt_len)
		{
			m_iNextENote++;

			// after the last endnote there is an extra paragraph
			// marker that is still a part of the endnote section --
			// we do not want that marker imported
			if(m_iNextENote < m_iEndnotesCount)
				_findNextENoteSection();
			else
			{
				xxx_UT_DEBUGMSG(("End of endnotes marker at pos %d\n", iDocPosition));
				return false;
			}
		}

		// if this is the first character in an endnote, insert the anchor
		if( m_iNextENote < m_iEndnotesCount && iDocPosition == m_pEndnotes[m_iNextENote].txt_pos)
		{
			std::string footpid =
				UT_std_string_sprintf("%i", m_pEndnotes[m_iNextENote].pid);

			const PP_PropertyVector attribsA = {
				"type", "endnote_anchor",
				"endnote-id", footpid,
				"props", m_charProps.c_str(),
				"style", m_charStyle.c_str()
			};

			const PP_PropertyVector attribsB = {
				"props", m_paraProps.c_str(),
				"style", m_paraStyle.c_str()
			};

			_appendStrux(PTX_Block, attribsB);
			m_bInPara = true;

			if(m_pEndnotes[m_iNextENote].type)
			{
				_appendObject(PTO_Field, attribsA);
				return false;
			}
			return true;
		}

		xxx_UT_DEBUGMSG(("In endnote %d, on pos %d\n", m_iNextENote, iDocPosition));
		// do not return !!!
	}
	else if(m_bInENotes)
	{
		m_bInENotes = false;
		xxx_UT_DEBUGMSG(("Leaving endnote territory\n"));
		// move to the end of the document ...

		// do not return !!!
	}

	// we only return here, so that the code above could be extended
	// for handly annotations by simply copy/paste
	return true;
}

/*!
    Retrieve the comment tables (MS-DOC 2.3.4): PlcfandRef gives the
    main-document CP of each 0x05 comment reference mark plus an
    ATRDPre10 of metadata, PlcfandTxt the body ranges in the annotation
    subdocument.  The annotated range itself is resolved through the
    ATRD's lTagBkmk -- the matching ATNBE in SttbfAtnBkmk indexes
    PlcfAtnbkf/PlcfAtnbkl; comments with lTagBkmk -1 anchor at a point.
    Author names come from GrpXstAtnOwners (ATRD.ibst) and initials from
    the ATRD's LPXCharBuffer9; Word 2000+ also carries an AtrdExtra with
    a DTTM per comment.
*/
void IE_Imp_MsWord_97::_handleAnnotations(const wvParseStruct *ps)
{
	UT_uint32 i;

	DELETEPV(m_pAnnotations);
	m_iAnnotationsCount = 0;
	m_iAnnAnchor = 0;
	m_vecAnnOrder.clear();
	m_vecAnnOpen.clear();

	// PlcfandTxt holds n+2 CPs; a smaller lcb cannot describe a comment
	if(ps->fib.lcbPlcfandTxt < 12)
		return;

	UT_uint32 count = ps->fib.lcbPlcfandTxt/4 - 2;
	/* PlcfandRef holds n+1 U32 positions plus n ATRDPre10s of cbATRD
	   bytes each -- never let the count exceed what it can hold */
	UT_uint32 maxByRef = ps->fib.lcbPlcfandRef >= 4 ?
		(ps->fib.lcbPlcfandRef - 4)/(4 + cbATRD) : 0;
	if(count > maxByRef)
		count = maxByRef;
	if(!count)
		return;

	ATRD *atrd = nullptr;
	U32 *posRef = nullptr;
	U32  noatrd = 0;
	if(wvGetATRD_PLCF(&atrd, &posRef, &noatrd, ps->fib.fcPlcfandRef,
					 ps->fib.lcbPlcfandRef, ps->tablefd))
		return;
	if(noatrd < count)
		count = noatrd;

	U32 *pTxt = nullptr;
	if(!count ||
	   wvGetPLCF(&pTxt, ps->fib.fcPlcfandTxt,
				 ps->fib.lcbPlcfandTxt, ps->tablefd))
	{
		wvFree(atrd);
		wvFree(posRef);
		return;
	}

	// annotation bookmarks: lTagBkmk -> ATNBE in SttbfAtnBkmk ->
	// PlcfAtnbkf/PlcfAtnbkl bounds
	STTBF atnbkmk;
	atnbkmk.extendedflag = 0;
	atnbkmk.nostrings = 0;
	atnbkmk.extradatalen = 0;
	atnbkmk.s8strings = nullptr;
	atnbkmk.u16strings = nullptr;
	atnbkmk.extradata = nullptr;
	wvGetSTTBF(&atnbkmk, ps->fib.fcSttbfAtnbkmk,
			   ps->fib.lcbSttbfAtnbkmk, ps->tablefd);

	BKF *bkf = nullptr;
	BKL *bkl = nullptr;
	U32 *posBKF = nullptr, *posBKL = nullptr, nbkf = 0, nbkl = 0;
	bool bBkfOk = !wvGetBKF_PLCF(&bkf, &posBKF, &nbkf,
							   ps->fib.fcPlcfAtnbkf,
							   ps->fib.lcbPlcfAtnbkf, ps->tablefd);
	bool bBklOk = !wvGetBKL_PLCF(&bkl, &posBKL, &nbkl,
							   ps->fib.fcPlcfAtnbkl,
							   ps->fib.lcbPlcfAtnbkl,
							   ps->fib.fcPlcfAtnbkf,
							   ps->fib.lcbPlcfAtnbkf, ps->tablefd);

	// the author names
	STTBF owners;
	wvGetGrpXst(&owners, ps->fib.fcGrpXstAtnOwners,
				ps->fib.lcbGrpXstAtnOwners, ps->tablefd);

	try
	{
		m_pAnnotations = new msAnnotation[count];
	}
	catch(...)
	{
		m_pAnnotations = nullptr;
	}

	if(!m_pAnnotations)
	{
		wvFree(atrd);
		wvFree(posRef);
		wvFree(pTxt);
		if(bBkfOk)
		{
			wvFree(bkf);
			wvFree(posBKF);
		}
		if(bBklOk)
		{
			wvFree(bkl);
			wvFree(posBKL);
		}
		wvReleaseSTTBF(&atnbkmk);
		wvReleaseSTTBF(&owners);
		return;
	}
	m_iAnnotationsCount = count;

	for(i = 0; i < count; i++)
	{
		msAnnotation & a = m_pAnnotations[i];
		a.ref_pos       = posRef[i];
		a.txt_pos       = pTxt[i] + m_iAnnotationsStart;
		a.txt_len       = (pTxt[i+1] > pTxt[i]) ? pTxt[i+1] - pTxt[i] : 0;
		a.pid           = getDoc()->getUID(UT_UniqueId::Annotation);
		a.open          = false;
		a.endSection    = nullptr;

		// default to a point comment anchored at the reference mark
		a.anchor_first  = a.ref_pos;
		a.anchor_last   = a.ref_pos;

		if(atrd[i].lTagBkmk >= 0 && bBkfOk && bBklOk && atnbkmk.extradata)
		{
			for(U32 j = 0; j < atnbkmk.nostrings; j++)
			{
				// ATNBE: bmc (2) + lTag (4) + lTagOld (4)
				if(atnbkmk.extradatalen < 6 || !atnbkmk.extradata[j])
					continue;
				S32 lTag = static_cast<S32>( sread_32ubit(atnbkmk.extradata[j] + 2));
				if(lTag == atrd[i].lTagBkmk && j < nbkf &&
				   bkf[j].ibkl >= 0 && static_cast<U32>( bkf[j].ibkl )< nbkl)
				{
					a.anchor_first = posBKF[j];
					a.anchor_last  = posBKL[bkf[j].ibkl];
					break;
				}
			}
		}
		/* keep the anchor sane: it cannot begin after its reference
		   mark, and it ends where the reference mark sits at the
		   latest */
		if(a.anchor_first > a.ref_pos)
			a.anchor_first = a.ref_pos;
		if(a.anchor_last > a.ref_pos || a.anchor_last < a.anchor_first)
			a.anchor_last = a.ref_pos;

		// author name: ATRD.ibst indexes the GrpXstAtnOwners XSTs
		if(atrd[i].ibst >= 0 && static_cast<U32>( atrd[i].ibst )< owners.nostrings &&
		   owners.u16strings && owners.u16strings[atrd[i].ibst])
		{
			UT_UTF8String s;
			const U16 * p = owners.u16strings[atrd[i].ibst];
			s.appendUCS2(reinterpret_cast<const UT_UCS2Char *>(p),
						 UT_UCS2_strlen(reinterpret_cast<const UT_UCS2Char *>(p)));
			a.author = s.utf8_str();
		}

		// initials: LPXCharBuffer9 is cch (<= 9) then 9 U16 chars
		if(atrd[i].xstUsrInitl[0] && atrd[i].xstUsrInitl[0] <= 9)
		{
			UT_UTF8String s;
			s.appendUCS2(reinterpret_cast<const UT_UCS2Char *>(atrd[i].xstUsrInitl + 1),
						 atrd[i].xstUsrInitl[0]);
			a.initials = s.utf8_str();
		}
	}

	/* AtrdExtra carries one ATRDPost10 (18 bytes) per comment, in the
	   same order; its first member is the comment's DTTM: mint:6,
	   hr:5, dom:5, mon:4, yr:9 (offset 1900), wdy:3 */
	if(ps->fib.lcbAtrdExtra >= count * 18 && ps->fib.fcAtrdExtra > 0)
	{
		for(i = 0; i < count; i++)
		{
			wvStream_goto(ps->tablefd, ps->fib.fcAtrdExtra + i * 18);
			U32 dttm = read_32ubit(ps->tablefd);
			U32 mon = (dttm >> 16) & 0xF;
			U32 dom = (dttm >> 11) & 0x1F;
			U32 yr  = ((dttm >> 20) & 0x1FF) + 1900;
			if(dom && mon)
			{
				m_pAnnotations[i].date =
					UT_std_string_sprintf("%u-%u-%u", mon, dom, yr);
			}
		}
	}

	// anchors fire in document order of their start position, which
	// need not match the comment order in a nested/overlapping range
	for(i = 0; i < count; i++)
		m_vecAnnOrder.push_back(i);
	std::stable_sort(m_vecAnnOrder.begin(), m_vecAnnOrder.end(),
					 [this](UT_uint32 x, UT_uint32 y) {
						 return m_pAnnotations[x].anchor_first <
							 m_pAnnotations[y].anchor_first;
					 });

#ifdef DEBUG
	for(i = 0; i < count; i++)
	{
		UT_DEBUGMSG(("Annotation %d: ref %d anchor [%d,%d] txt [%d,+%d] "
					 "author '%s' date '%s'\n", i, m_pAnnotations[i].ref_pos,
					 m_pAnnotations[i].anchor_first,
					 m_pAnnotations[i].anchor_last,
					 m_pAnnotations[i].txt_pos, m_pAnnotations[i].txt_len,
					 m_pAnnotations[i].author.c_str(),
					 m_pAnnotations[i].date.c_str()));
	}
#endif

	wvFree(atrd);
	wvFree(posRef);
	wvFree(pTxt);
	if(bBkfOk)
	{
		wvFree(bkf);
		wvFree(posBKF);
	}
	if(bBklOk)
	{
		wvFree(bkl);
		wvFree(posBKL);
	}
	wvReleaseSTTBF(&atnbkmk);
	wvReleaseSTTBF(&owners);
}

/*!
    Emit the anchor start object and the (empty) shadow section for a
    comment: [PTO_Annotation][SectionAnnotation][EndAnnotation], the
    same skeleton the OXML importer builds at a w:commentRangeStart.
    The body's own block is created when the decode reaches the
    annotation subdocument.  Author/initials/date ride in the strux
    "props" attribute -- fl_AnnotationLayout reads them as properties.
*/
bool IE_Imp_MsWord_97::_insertAnnotationStart(msAnnotation * a)
{
	UT_return_val_if_fail(a, false);
	this->_flush();

	std::string pid = UT_std_string_sprintf("%u", a->pid);
	const PP_PropertyVector attribsA = {
		"annotation", pid
	};
	_appendObject(PTO_Annotation, attribsA);

	PP_PropertyVector attribsS = {
		"annotation-id", pid
	};
	// ';' and ':' are the prop-list separators in the serialized form
	std::string props;
	for(const std::string * pVal : {&a->author, &a->initials, &a->date})
	{
		if(pVal->empty())
			continue;
		std::string v = *pVal;
		std::replace(v.begin(), v.end(), ';', ',');
		std::replace(v.begin(), v.end(), ':', ',');
		const char * name = (pVal == &a->author) ? "annotation-author" :
			(pVal == &a->initials) ? "annotation-initials" :
			"annotation-date";
		if(!props.empty())
			props += "; ";
		props += name;
		props += ":";
		props += v;
	}
	if(!props.empty())
	{
		attribsS.push_back("props");
		attribsS.push_back(props);
	}

	_appendStrux(PTX_SectionAnnotation, attribsS);
	_appendStrux(PTX_EndAnnotation, PP_NOPROPS);
	a->endSection = getDoc()->getLastFrag();
	a->open = true;
	return true;
}

/*!
    Open/close the comment anchors due at this main-document position.
    The end object is anonymous (it closes the innermost open anchor),
    so a simple stack of open comments suffices even for nested or
    overlapping comment ranges.

    \return true if the character at this position is a comment
            reference mark (0x05) that must not be imported
*/
bool IE_Imp_MsWord_97::_insertAnnotationIfAppropriate(UT_uint32 iDocPosition)
{
	if(!m_pAnnotations || !m_iAnnotationsCount ||
	   iDocPosition >= m_iTextEnd)
		return false;

	// open every anchor whose range begins here
	while(m_iAnnAnchor < m_vecAnnOrder.size() &&
		  m_pAnnotations[m_vecAnnOrder[m_iAnnAnchor]].anchor_first <=
		  iDocPosition)
	{
		_insertAnnotationStart(&m_pAnnotations[m_vecAnnOrder[m_iAnnAnchor]]);
		m_vecAnnOpen.push_back(m_vecAnnOrder[m_iAnnAnchor]);
		m_iAnnAnchor++;
	}

	// close anchors whose range ended before this character; flush
	// first so the last anchored characters land before the end
	// marker instead of after it
	for(size_t k = m_vecAnnOpen.size(); k > 0; )
	{
		k--;
		msAnnotation * pA = &m_pAnnotations[m_vecAnnOpen[k]];
		if(pA->anchor_last <= iDocPosition)
		{
			this->_flush();
			_appendObject(PTO_Annotation, PP_NOPROPS);
			m_vecAnnOpen.erase(m_vecAnnOpen.begin() + k);
		}
	}

	// swallow a reference mark (0x05) that arrived without sprmCFSpec
	bool res = false;
	for(UT_uint32 i = 0; i < m_iAnnotationsCount; i++)
	{
		if(m_pAnnotations[i].ref_pos == iDocPosition)
		{
			res = true;
			break;
		}
	}
	return res;
}

/*!
    Discard a pending line/page break and any run of break characters
    already buffered when a character inside the annotation
    subdocument is swallowed.  Each comment body ends in a paragraph
    mark, which the main char path turns into a pending UCS_LF --
    without this it would surface as a leading break in the next
    comment's text.
*/
void IE_Imp_MsWord_97::_dropAnnotationBreaks(void)
{
	m_bPageBreakPending = false;
	m_bLineBreakPending = false;

	bool bAllBreaks = true;
	for(UT_uint32 i = 0; i < m_pTextRun.size(); i++)
	{
		UT_UCS4Char ch = m_pTextRun[i];
		if(ch != UCS_LF && ch != UCS_FF)
		{
			bAllBreaks = false;
			break;
		}
	}
	if(bAllBreaks)
		m_pTextRun.clear();
}

/*!
    Route the annotation-subdocument text into the comment shadows.
    Mirrors _handleNotesText: entering the story switches the append
    methods over to insert-before the current comment's EndAnnotation
    frag; each body begins with a spec'd 0x05 mark and ends before the
    next range, the story's trailing paragraph mark is dropped.

    \return false if the present character is to be skipped
*/
bool IE_Imp_MsWord_97::_handleAnnotationsText(UT_uint32 iDocPosition,
											  UT_UCS4Char c)
{
	if(iDocPosition >= m_iAnnotationsStart &&
	   iDocPosition < m_iAnnotationsEnd)
	{
		if(!m_bInAnnotations)
		{
			xxx_UT_DEBUGMSG(("In annotation territory: pos %d\n",
							 iDocPosition));
			m_bInAnnotations = true;
			m_bInHeaders = false;
			m_iNextAnnotation = 0;
			_findNextAnnotationSection();
			_endSect(nullptr,0,nullptr,0);
			m_bInSect = true;
		}

		while(m_iNextAnnotation < m_iAnnotationsCount &&
			  iDocPosition >= m_pAnnotations[m_iNextAnnotation].txt_pos +
							  m_pAnnotations[m_iNextAnnotation].txt_len)
		{
			// push out any buffered text while the insert point still
			// refers to the comment it belongs to
			this->_flush();
			m_iNextAnnotation++;
			if(m_iNextAnnotation < m_iAnnotationsCount)
				_findNextAnnotationSection();
		}

		// anything outside a body range -- the story's trailing
		// paragraph mark, gaps between bodies and bodies of comments
		// whose shadow was never created -- contributes nothing
		if(m_iNextAnnotation >= m_iAnnotationsCount ||
		   !m_pAnnotationEndSection ||
		   iDocPosition < m_pAnnotations[m_iNextAnnotation].txt_pos)
		{
			_dropAnnotationBreaks();
			return false;
		}

		msAnnotation * pA = &m_pAnnotations[m_iNextAnnotation];
		if(iDocPosition == pA->txt_pos)
		{
			// the first character of a body is the comment's own 0x05
			// reference mark; start the shadow's paragraph here and
			// drop the mark (the layout draws its own anchor)
			const PP_PropertyVector attribsB = {
				"props", m_paraProps.c_str(),
				"style", m_paraStyle.c_str()
			};
			_appendStrux(PTX_Block, attribsB);
			m_bInPara = true;
			if(c == 0x05)
			{
				_dropAnnotationBreaks();
				return false;
			}
		}

		xxx_UT_DEBUGMSG(("In annotation %d, on pos %d\n",
						 m_iNextAnnotation, iDocPosition));
		return true;
	}
	else if(m_bInAnnotations)
	{
		m_bInAnnotations = false;
		xxx_UT_DEBUGMSG(("Leaving annotation territory\n"));
	}
	return true;
}

bool IE_Imp_MsWord_97::_findNextAnnotationSection()
{
	m_pAnnotationEndSection =
		(m_iNextAnnotation < m_iAnnotationsCount) ?
		m_pAnnotations[m_iNextAnnotation].endSection : nullptr;
	return m_pAnnotationEndSection != nullptr;
}


/*!
    This function makes sure that the insert is happening at the
    correct place if we are in a segment which belongs to one of the
    set of Textboxes

    \parameter UT_uint32 iDocPosition: character position in the Word
                                       document stream
    \return returns false if the present character is to be skipped,
            true otherwise
*/
bool IE_Imp_MsWord_97::_handleTextboxesText(UT_uint32 iDocPosition,
											UT_UCS4Char c)
{
	if(iDocPosition >= m_iTextboxesStart && iDocPosition < m_iTextboxesEnd)
	{
		// upon entry into the Textland-land, we will need to search for
		// the first Textbox section in our document, note that we are
		// in a Textbox section, note at what doc position the current
		// textbox will end, and then let things run until we reach
		// the end of the textbox; then we need to search for the next
		// doc section, etc.


		// when in a Text box section, all the functions that normally
		// use append methods will need to use insert methods instead

		if(!m_bInTextboxes)
		{
			UT_DEBUGMSG(("In Textbox territory: pos %d\n", iDocPosition));
			m_bInTextboxes = true;
			m_bInFNotes = false;
			m_bInHeaders = false;

			// we will reuse the m_iNextTextbox variable, noting it
			// refers to the CURRENT textbox

			m_iNextTextbox = 0;
			_findNextTextboxSection();
			_endSect(nullptr,0,nullptr,0);
			m_bInSect = true;
		}

		// past the end of a box's range the insert point retargets to
		// the frame of the next box in the story
		while(m_iNextTextbox < m_iTextboxCount &&
			  iDocPosition >= m_pTextboxes[m_iNextTextbox].txt_pos +
							  m_pTextboxes[m_iNextTextbox].txt_len)
		{
			// push out any buffered text while the insert point still
			// refers to the box it belongs to
			this->_flush();
			m_iNextTextbox++;
			if(m_iNextTextbox < m_iTextboxCount)
				_findNextTextboxSection();
		}

		// anything outside a box's range -- the story's trailing
		// paragraph marks, gaps between ranges and the text of boxes
		// whose frame was never emitted -- contributes nothing
		if(m_iNextTextbox >= m_iTextboxCount ||
		   !m_pTextboxEndSection ||
		   iDocPosition < m_pTextboxes[m_iNextTextbox].txt_pos)
		{
			if(m_iNextTextbox >= m_iTextboxCount)
			{
				m_pTextboxEndSection = nullptr;
			}
			return false;
		}

		const textbox * pT = &m_pTextboxes[m_iNextTextbox];
		if(iDocPosition == pT->txt_pos)
		{
			/* the frame was emitted with a single empty block; give
			 * it this paragraph's properties (the para strux itself
			 * is suppressed in _beginPara, which fires before the
			 * insert point is re-targeted) */
			pf_Frag * pPrev = m_pTextboxEndSection->getPrev();
			if(pPrev && pPrev->getType() == pf_Frag::PFT_Strux)
			{
				pf_Frag_Strux * pfs =
					static_cast<pf_Frag_Strux *>(pPrev);
				if(pfs->getStruxType() == PTX_Block)
				{
					const PP_PropertyVector attribsB = {
						"props", m_paraProps.c_str(),
						"style", m_paraStyle.c_str()
					};
					getDoc()->changeStruxFormatNoUpdate(PTC_AddFmt,
													  pfs, attribsB);
				}
			}
			m_bInPara = true;
		}

		// the last character of each range is the box's terminating
		// paragraph mark -- not document content
		if(iDocPosition == pT->txt_pos + pT->txt_len - 1 && c == 0x0D)
		{
			return false;
		}

		xxx_UT_DEBUGMSG(("In Textbox %d, on pos %d\n", m_iNextTextbox, iDocPosition));
	}
	else if(m_bInTextboxes)
	{
		m_bInTextboxes = false;
		m_pTextboxEndSection = nullptr;
		UT_DEBUGMSG(("Leaving Textbox territory\n"));
	}

	return true;
}

bool IE_Imp_MsWord_97::_findNextFNoteSection()
{
	if(!m_iNextFNote)
	{
		// move to the start of the doc first
		m_pNotesEndSection = nullptr;
	}

	if(m_pNotesEndSection)
	{
		// move to the next fragment
		m_pNotesEndSection = m_pNotesEndSection->getNext();
		UT_return_val_if_fail(m_pNotesEndSection, false);
	}
	

	m_pNotesEndSection = getDoc()->findFragOfType(pf_Frag::PFT_Strux,
												  static_cast<UT_sint32>(PTX_EndFootnote),
												  m_pNotesEndSection);

	if(!m_pNotesEndSection)
	{
		xxx_UT_DEBUGMSG(("Error: footnote section not found!!!\n"));
		return false;
	}

	return true;
}


///////////////////////////////////////////////////////////////////////
bool IE_Imp_MsWord_97::_findNextTextboxSection()
{
	m_pTextboxEndSection = nullptr;

	if(m_iNextTextbox >= m_iTextboxCount || !m_pTextboxes)
	{
		return false;
	}

	/* the FTXBXS lid is the spid of the shape whose frame was
	 * emitted when its anchor was reached in the main story */
	const UT_uint32 iLid = m_pTextboxes[m_iNextTextbox].lid;
	for(UT_sint32 i = 0; i < m_vecTextboxPos.getItemCount(); i++)
	{
		textboxPos * pPos = m_vecTextboxPos.getNthItem(i);
		if(pPos && pPos->lid == iLid && pPos->endFrame)
		{
			m_pTextboxEndSection = pPos->endFrame;
			return true;
		}
	}

	UT_DEBUGMSG(("No frame found for textbox %u (lid %u); its text "
				 "will be appended to the main document\n",
				 m_iNextTextbox, iLid));
	return false;
}

bool IE_Imp_MsWord_97::_findNextENoteSection()
{
	if(!m_iNextENote)
	{
		// move to the start of the doc first
		m_pNotesEndSection = nullptr;
	}
	
	if(m_pNotesEndSection)
	{
		// move to the next fragment
		m_pNotesEndSection = m_pNotesEndSection->getNext();
		UT_return_val_if_fail(m_pNotesEndSection, false);
	}

	m_pNotesEndSection = getDoc()->findFragOfType(pf_Frag::PFT_Strux,
												  static_cast<UT_sint32>(PTX_EndEndnote),
												  m_pNotesEndSection);

	if(!m_pNotesEndSection)
	{
		UT_DEBUGMSG(("Error: endnote section not found!!!\n"));
		return false;
	}
	
	return true;
}

bool IE_Imp_MsWord_97::_shouldUseInsert() const
{
	return ((m_bInFNotes || m_bInENotes) && !m_bInHeaders && !m_bInTextboxes);
}

bool IE_Imp_MsWord_97::_ensureInBlock()
{

  bool bret = true;

  pf_Frag * pf = getDoc()->getLastFrag();
  while(pf && pf->getType() != pf_Frag::PFT_Strux)
    {
      pf = pf->getPrev();
    }
    if(pf && (pf->getType() == pf_Frag::PFT_Strux) )
    {
      pf_Frag_Strux * pfs = static_cast<pf_Frag_Strux *>(pf);
      if(pfs->getStruxType() != PTX_Block)
      {
        bret = _appendStrux(PTX_Block, PP_NOPROPS);
	if (bret) m_bInPara = true;
      }
    }
    else if( pf == nullptr)
    {
      bret = _appendStrux(PTX_Block, PP_NOPROPS);
      if (bret) m_bInPara = true;
    }

    return bret;
}

bool IE_Imp_MsWord_97::_appendStrux(PTStruxType pts, const PP_PropertyVector & attributes)
{
	if(pts == PTX_SectionFrame)
	{
		UT_DEBUGMSG(("Appending Frame \n"));
	}
	if(pts == PTX_EndFrame)
	{
		UT_DEBUGMSG(("Appending EndFrame \n"));
	}
	if(m_bInHeaders)
	{
		return _appendStruxHdrFtr(pts, attributes);
	}
	else if(_shouldUseInsert() && m_pNotesEndSection)
	{
		return getDoc()->insertStruxBeforeFrag(m_pNotesEndSection, pts, attributes);
	}
	else if(m_bInAnnotations && m_pAnnotationEndSection)
	{
		return getDoc()->insertStruxBeforeFrag(m_pAnnotationEndSection, pts, attributes);
	}
	else if(m_bInTextboxes && m_pTextboxEndSection)
	{
		if(pts == PTX_Block)
		{
			xxx_UT_DEBUGMSG(("Insert block in Text box \n"));
		}
		return getDoc()->insertStruxBeforeFrag(m_pTextboxEndSection, pts, attributes);
	}
	if(pts == PTX_SectionFrame)
	{
//		Make sure any pending text is flushed
		_flush();

//
// Text boxes need to be preceded by Blocks
//
		pf_Frag * pf = getDoc()->getLastFrag();
		while(pf && pf->getType() != pf_Frag::PFT_Strux)
		{
			pf = pf->getPrev();
		}
		if(pf && (pf->getType() == pf_Frag::PFT_Strux) )
		{
			pf_Frag_Strux * pfs = static_cast<pf_Frag_Strux *>(pf);
			if(pfs->getStruxType() != PTX_Block)
			{
				getDoc()->appendStrux(PTX_Block, PP_NOPROPS);
			}
		}
		else if( pf == nullptr)
		{
			getDoc()->appendStrux(PTX_Block, PP_NOPROPS);
		}
	}
	return getDoc()->appendStrux(pts, attributes);
}

bool IE_Imp_MsWord_97::_appendObject(PTObjectType pto, const PP_PropertyVector & attributes)
{
	if(m_bInHeaders)
	{
		return _appendObjectHdrFtr(pto, attributes);
	}
	else if(_shouldUseInsert() && m_pNotesEndSection)
	{
		return getDoc()->insertObjectBeforeFrag(m_pNotesEndSection, pto, attributes);
	}
	else if(m_bInAnnotations && m_pAnnotationEndSection)
	{
		return getDoc()->insertObjectBeforeFrag(m_pAnnotationEndSection, pto, attributes);
	}
	else if(m_bInTextboxes && m_pTextboxEndSection)
	{
		return getDoc()->insertObjectBeforeFrag(m_pTextboxEndSection, pto, attributes);
	}
	if(!m_bInPara)
	{
	  _appendStrux(PTX_Block, PP_NOPROPS);
	  m_bInPara = true;
	}
	return getDoc()->appendObject(pto, attributes);
}

bool IE_Imp_MsWord_97::_appendSpan(const UT_UCS4Char * p, UT_uint32 length)
{
	if(m_bInHeaders)
	{
		return _appendSpanHdrFtr(p, length);
	}
	else if(_shouldUseInsert() && m_pNotesEndSection)
	{
		return getDoc()->insertSpanBeforeFrag(m_pNotesEndSection, p, length);
	}
	else if(m_bInAnnotations && m_pAnnotationEndSection)
	{
		return getDoc()->insertSpanBeforeFrag(m_pAnnotationEndSection, p, length);
	}
	else if(m_bInTextboxes && m_pTextboxEndSection)
	{
		return getDoc()->insertSpanBeforeFrag(m_pTextboxEndSection, p, length);
	}
	return getDoc()->appendSpan(p, length);
}

bool IE_Imp_MsWord_97::_appendFmt(const PP_PropertyVector & attributes)
{
	// no special processing required, this only changes m_loading in
	// the PT
	return getDoc()->appendFmt(attributes);
}

/*!
    The append*HdrFtr() methods below are needed because in AW headers
    cannot be shared among sections; in contrast in Word one header
    can be used by a chain of sections. We get around it by
    duplicating that one header for each section that uses it. Since
    we cannot wind back throught the data stream we have to duplicate
    each shared header as we go using the info stored in the current
    header's d struct.
*/
bool IE_Imp_MsWord_97::_appendStruxHdrFtr(PTStruxType pts, const PP_PropertyVector & attributes)
{
	UT_return_val_if_fail(m_bInHeaders,false);
	UT_return_val_if_fail(m_iCurrentHeader < m_iHeadersCount,false);
	UT_DEBUGMSG(("Inserting strux of type %d in HdrFtr %d\n",pts,m_iCurrentHeader));
	UT_ASSERT(m_bInSect);
	bool bRet = true;
	for(UT_sint32 i = 0; i < m_pHeaders[m_iCurrentHeader].d.frag.getItemCount(); i++)
	{
		pf_Frag * pF = const_cast<pf_Frag*>(static_cast<const pf_Frag*>( m_pHeaders[m_iCurrentHeader].d.frag.getNthItem(i)));
		UT_return_val_if_fail(pF,false);
		UT_DEBUGMSG(("Inserting strux of type %d in Dirivative HdrFtr \n",pts));

		bRet &= getDoc()->insertStruxBeforeFrag(pF, pts, attributes);
	}
	
	bRet &= getDoc()->appendStrux(pts, attributes);
	if(pts != PTX_Block)
	{
		xxx_UT_DEBUGMSG(("m_bInPara set false here -1 \n"));
		m_bInPara = false;
	}
	else
	{
		m_bInPara = true;
	}
	return bRet;
}

bool IE_Imp_MsWord_97::_appendObjectHdrFtr(PTObjectType pto, const PP_PropertyVector & attributes)
{
	UT_return_val_if_fail(m_bInHeaders,false);
	UT_return_val_if_fail(m_iCurrentHeader < m_iHeadersCount,false);

	bool bRet = true;

	for(UT_sint32 i = 0; i < m_pHeaders[m_iCurrentHeader].d.frag.getItemCount(); i++)
	{
		pf_Frag * pF = const_cast<pf_Frag*>(static_cast<const pf_Frag*>( m_pHeaders[m_iCurrentHeader].d.frag.getNthItem(i)));
		UT_return_val_if_fail(pF,false);
		if(!m_bInPara)
		{
			bRet &= getDoc()->insertStruxBeforeFrag(pF, PTX_Block, PP_NOPROPS);
		}
		bRet &= getDoc()->insertObjectBeforeFrag(pF, pto, attributes);
	}
	if(!m_bInPara)
	{
		m_bInPara = true;
		bRet &= getDoc()->appendStrux(PTX_Block, PP_NOPROPS);
	}
	bRet &= getDoc()->appendObject(pto, attributes);
	return bRet;
}

bool IE_Imp_MsWord_97::_appendSpanHdrFtr(const UT_UCS4Char * p, UT_uint32 length)
{
	UT_return_val_if_fail(m_bInHeaders,false);
	UT_return_val_if_fail(m_iCurrentHeader < m_iHeadersCount,false);

	bool bRet = true;
	for(UT_sint32 i = 0; i < m_pHeaders[m_iCurrentHeader].d.frag.getItemCount(); i++)
	{
		pf_Frag * pF = const_cast<pf_Frag*>(static_cast<const pf_Frag*>( m_pHeaders[m_iCurrentHeader].d.frag.getNthItem(i)));
		UT_return_val_if_fail(pF,false);
		if(!m_bInPara)
		{
			bRet &= getDoc()->insertStruxBeforeFrag(pF, PTX_Block, PP_NOPROPS);
		}

		bRet &= getDoc()->insertSpanBeforeFrag(pF, p, length);
	}
	if(!m_bInPara)
	{
		m_bInPara = true;
		bRet &= getDoc()->appendStrux(PTX_Block, PP_NOPROPS);
	}	
	bRet &= getDoc()->appendSpan(p, length);
	return bRet;
}


void IE_Imp_MsWord_97::_handleHeaders(const wvParseStruct *ps)
{
	UT_uint32 i, k;

	DELETEPV(m_pHeaders);

	m_iHeadersCount = 0;
	UT_uint32 *pPLCF_txt = nullptr;

	/*
	   The header/footer PLCF in Word 97+ is organised as follows:

	   indx         |  function
	   -------------------------------------------------------------------------------
	   0-5: document wide settings
	   -------------------------------------------------------------------------------
	    0           |  footnote separator
	    1           |  footnote continuation separator (i.e., continued on next page)
	    2           |  document-wide footnote continuation notice (i.e., continued
                   	   from previous page)
	   3-5          |  as above for endnotes
       -------------------------------------------------------------------------------
	   now for i-th section in document (i >= 0)
	   -------------------------------------------------------------------------------
	   i+6          |  header even pages
	   i+7          |  header odd  pages
	   i+8          |  footer even pages
	   i+9          |  footer odd  pages
	   i+10         |  header first page
	   i+11         |  footer first page
	   -------------------------------------------------------------------------------
       according to the docs now should come the foot/endnote
	   separators but they do not -- those settings appear to be
	   document wide only ...
	   -------------------------------------------------------------------------------
	   i+12 - i+17  |  as the document wide footnote/endnote separators above

	   NB: the record for the last section in the document may be
	       incomplete, i.e., for n sections  m_iHeadersCount <= 6 + 12*n.

	   The even headers are only applied if ps->dop.fFacingPages is set
	*/

	bool bHeaderError = false;

	if(ps->fib.lcbPlcfhdd >= 8)
	{
		/* the docs are ambiguous, at one place saying the PLCF
		   contains n+2 entries, another n+1; I think the former is correct*/
		m_iHeadersCount = ps->fib.lcbPlcfhdd/4 - 2;
		try
		{
			m_pHeaders = new header[m_iHeadersCount];
		}
		catch(...)
		{
			m_pHeaders = nullptr;
		}

		/* leaving a nonzero count with a NULL array would send
		   _beginSect indexing into nothing */
		if(!m_pHeaders)
		{
			m_iHeadersCount = 0;
			return;
		}
		
		// this is really quite straight forward; we retrieve the PLCF
		// which is a sequence of n+2 positions (UT_uint32) of the
		// header text in its data stream
		if(wvGetPLCF(&pPLCF_txt, ps->fib.fcPlcfhdd, ps->fib.lcbPlcfhdd, ps->tablefd))
		{
			bHeaderError = true;
		}

		if(bHeaderError || !pPLCF_txt)
		{
			/* the array was allocated but never filled; do not leave
			   a count that describes uninitialized headers */
			m_iHeadersCount = 0;
			return;
		}

		{
			for(i = 0; i < m_iHeadersCount; i++)
			{
				m_pHeaders[i].pos = pPLCF_txt[i] + m_iHeadersStart;
				m_pHeaders[i].len = pPLCF_txt[i+1] - pPLCF_txt[i];
				m_pHeaders[i].pid = getDoc()->getUID(UT_UniqueId::HeaderFtr);
				m_pHeaders[i].bDerivative = false;

				UT_DEBUGMSG(("Header %d has pid %d \n",i,m_pHeaders[i].pid));
				if(i < 6)
				{
					// document wide footnote/endnote separators
					m_pHeaders[i].type = HF_Unsupported;
				}
				else
				{
					switch((i-6)%6)
					{
						case 0:
							if(m_bEvenOddHeaders)
								m_pHeaders[i].type = HF_HeaderEven;
							else
								m_pHeaders[i].type = HF_Unsupported;
							break;
						case 1:
							m_pHeaders[i].type = HF_HeaderOdd;
							break;
						case 2:
							if(m_bEvenOddHeaders)
								m_pHeaders[i].type = HF_FooterEven;
							else
								m_pHeaders[i].type = HF_Unsupported;
							break;
						case 3:
							m_pHeaders[i].type = HF_FooterOdd;
							break;
						case 4:
							m_pHeaders[i].type = HF_HeaderFirst;
							break;
						case 5:
							m_pHeaders[i].type = HF_FooterFirst;
							break;

						default:
							m_pHeaders[i].type = HF_Unsupported;
					}
				
					UT_DEBUGMSG(("Header no. %d, pos %d, len %d\n",
								 i,m_pHeaders[i].pos,m_pHeaders[i].len));

#if 1
					// this code is here because in AW we currently cannot
					// share headers between sections
					if(m_pHeaders[i].type != HF_Unsupported && m_pHeaders[i].len == 0)
					{
						// this is the case where the section is to use the
						// header of a previous section -- scroll back until
						// we find one
						k = i - 6;
						bool bContinue = false;
				
						while(k > 5)
						{
							if(m_pHeaders[k].len == 2)
							{
								// found empty header
								// set the type of the present header unsupported, so it does not
								// get referenced
								m_pHeaders[i].type = HF_Unsupported;
								bContinue = true;
								break;
							}
							else if(m_pHeaders[k].len == 0)
							{
								// try one section ahead
								k -= 6;
							}
							else
							{
								// found a meaningful header
								break;
							}
						}

						if(bContinue || k < 6)
						{
							// did not find any meaningful headers, set the type to unsupported, so
							// that it does not get referenced
							// 
							// we do not want to do this to the first page hdr/ftr,
							// because in this case len == 0 can mean the header should be
							// empty but present (this is determined by asep->fTitlePage
							if(m_pHeaders[i].type != HF_HeaderFirst && m_pHeaders[i].type != HF_FooterFirst)
								m_pHeaders[i].type = HF_Unsupported;
							
							continue;
						}

						// so we have found a meaningful header k that is to
						// be used in place of header i; we add header
						// i to k's d-struct; the section strux for i is
						// created when k is inserted, so it must not be
						// inserted again when we reach its position

						m_pHeaders[i].bDerivative = true;
						m_pHeaders[k].d.hdr.addItem(static_cast<void*>((m_pHeaders+i)));
					}
#endif
				}
			}

			wvFree(pPLCF_txt);
		}
	}
}

/*!
    A helper function that inserts the header/ftr section
*/
bool IE_Imp_MsWord_97::_insertHeaderSection(bool bDoBlockIns)
{
	// need to insert our header/footer section, preserving
	// any existing formatting ...

	// we need to be able to insert some 0-length headers
	if(m_pHeaders[m_iCurrentHeader].type != HF_Unsupported /*&& m_pHeaders[m_iCurrentHeader].len > 2*/)
	{
		if(m_iCurrentHeader == m_iLastAppendedHeader)
		{
			return false;
		}
		m_iLastAppendedHeader = m_iCurrentHeader;
		PP_PropertyVector attribsB;
		if(m_paraProps.size())
		{
			attribsB.push_back("props");
			attribsB.push_back(m_paraProps.c_str());
		}
		if(m_paraStyle.size())
		{
			attribsB.push_back("style");
			attribsB.push_back(m_paraStyle.c_str());
		}

		PP_PropertyVector attribsC;
		if(m_charProps.size())
		{
			attribsC.push_back("props");
			attribsC.push_back(m_charProps.c_str());
		}
		if(m_charStyle.size())
		{
			attribsC.push_back("style");
			attribsC.push_back(m_charStyle.c_str());
		}

		std::string id = UT_std_string_sprintf("%d", m_pHeaders[m_iCurrentHeader].pid);
		PP_PropertyVector attribsS = {
			"type", "",
			"id",  id
		};
		UT_DEBUGMSG(("Appending Current Header %d pid %s \n",m_iCurrentHeader,id.c_str()));
		switch(m_pHeaders[m_iCurrentHeader].type)
		{
			case HF_HeaderEven:
				attribsS[1] = "header-even";
				break;
			case HF_FooterEven:
				attribsS[1] = "footer-even";
				break;
			case HF_HeaderOdd:
				attribsS[1] = "header";
				break;
			case HF_FooterOdd:
				attribsS[1] = "footer";
				break;
			case HF_HeaderFirst:
				attribsS[1] = "header-first";
				break;
			case HF_FooterFirst:
				attribsS[1] = "footer-first";
				break;
			default:
				UT_ASSERT_HARMLESS(UT_NOT_REACHED);
		}

		// we use the document methods, not the importer methods intentionally
		UT_DEBUGMSG(("Direct Appending HdrFtr in MSWord_import \n"));
		if(!m_bInPara)
		{
			getDoc()->appendStrux(PTX_Block, PP_NOPROPS);
			m_bInPara = true;
		}
		getDoc()->appendStrux(PTX_SectionHdrFtr, attribsS);
		m_bInSect = true;
		m_bInHeaders = true;

		if(bDoBlockIns)
		{
			getDoc()->appendStrux(PTX_Block, attribsB);
			m_bInPara = true;
			_appendFmt(attribsC);
		}

		// now we insert the same for any derivative headers
		// ...
		for (UT_sint32 i = 0; i < m_pHeaders[m_iCurrentHeader].d.hdr.getItemCount(); i++)
		{
			header * pH = const_cast<header*>(static_cast<const header*>(m_pHeaders[m_iCurrentHeader].d.hdr.getNthItem(i)));
			UT_return_val_if_fail(pH, true);

			// skip any unsupported headers (we set the type to
			// unsupported when we find out that it is not used by the
			// section to which it belongs)
			if(pH->type == HF_Unsupported)
			{
				continue;
			}

			id = UT_std_string_sprintf("%d", pH->pid);
			attribsS[3] = id;

			switch(pH->type)
			{
				case HF_HeaderEven:
					attribsS[1] = "header-even";
					break;
				case HF_FooterEven:
					attribsS[1] = "footer-even";
					break;
				case HF_HeaderOdd:
					attribsS[1] = "header";
					break;
				case HF_FooterOdd:
					attribsS[1] = "footer";
					break;
				case HF_HeaderFirst:
					attribsS[1] = "header-first";
					break;
				case HF_FooterFirst:
					attribsS[1] = "footer-first";
					break;
				default:
					UT_ASSERT_HARMLESS(UT_NOT_REACHED);
			}
			UT_DEBUGMSG(("Appending Dirivative HdrFtr in MSWord_import \n"));
			getDoc()->appendStrux(PTX_SectionHdrFtr, attribsS);
			m_bInHeaders = true;

			// we need to remember the HdrFtr fragment for
			// later ...
			pf_Frag * pF = getDoc()->getLastFrag();
			UT_return_val_if_fail(pF && pF->getType() == pf_Frag::PFT_Strux, true);

			pf_Frag_Strux * pFS = static_cast<pf_Frag_Strux*>(pF);
			UT_return_val_if_fail(pFS->getStruxType() == PTX_SectionHdrFtr, true);

			m_pHeaders[m_iCurrentHeader].d.frag.addItem(static_cast<void*>(pF));

			if(bDoBlockIns)
			{
				getDoc()->appendStrux(PTX_Block, attribsB);
				getDoc()->appendFmt(attribsC);
			}
		}

		return true;
	}
	else
	{
		// just gobble the character ...
		m_bInHeaders = true;
		return false;
	}

	return false;
}



/*!
    This function makes sure that the insert is happening at the
    correct place if we are in the header segment.

    \parameter UT_uint32 iDocPosition: character position in the Word
                                       document stream
    \return returns false if the present character is to be skipped,
            true otherwise
*/
bool IE_Imp_MsWord_97::_handleHeadersText(UT_uint32 iDocPosition,bool bDoBlockIns)
{
	if(iDocPosition == m_iPrevHeaderPosition)
	{
		return true;
	}

	if(iDocPosition == m_iHeadersEnd)
	{
		m_iCurrentHeader++;

		if(m_iCurrentHeader < m_iHeadersCount)
		{
			// this is the case where we reached the end of the header segment, but still have
			// some headers in our header array left.
			// if we have any headers other than unsupported, we have to insert them as empty
		
			for(; m_iCurrentHeader < m_iHeadersCount; m_iCurrentHeader++)
			{
				if(m_pHeaders[m_iCurrentHeader].type != HF_Unsupported
				   && !m_pHeaders[m_iCurrentHeader].bDerivative)
					_insertHeaderSection(bDoBlockIns);
			}
		}
	}
	
	if(iDocPosition >= m_iHeadersStart && iDocPosition < m_iHeadersEnd)
	{
		m_iPrevHeaderPosition = iDocPosition;

		// upon entry into the header-land, we will need to search for
		// the first header/footer section in our document, note that we are
		// in a header section, note at what doc position the current
		// header will end, and then let things run until we reach
		// the end of the header; then we need to search for the next
		// doc section, etc.

		// when we scroll through 0-length headers, we need to remember where we started,
		// so we can insert the hdr section later
		bool bScrolledHeader = false;
		UT_uint32 iOrigHeader = 0;

		if(!m_bInHeaders)
		{
			UT_DEBUGMSG(("In headers territory: pos %d\n", iDocPosition));
			m_bInENotes = false;
			m_bInFNotes = false;

			m_iCurrentHeader = 0;

			// we need to close of any open section
			if(m_bInSect)
			{
				_endSect(nullptr,0,nullptr,0);
			}

			// some headers can be 0-length, skip them ... (0-length:  len <=2)
			while(m_iCurrentHeader < m_iHeadersCount && m_pHeaders[m_iCurrentHeader].len <= 2)
			{
				bScrolledHeader = true;
				m_iCurrentHeader++;
			}

			m_bInHeaders = true;
		}
		xxx_UT_DEBUGMSG(("CurrentHeader %d HeaderCount %d \n",m_iCurrentHeader,m_iHeadersCount));
		if (m_iCurrentHeader < m_iHeadersCount) {
			if(iDocPosition == m_pHeaders[m_iCurrentHeader].pos +
			   m_pHeaders[m_iCurrentHeader].len)
			{
				// new header, time to move on ...
				m_iCurrentHeader++;
				iOrigHeader = m_iCurrentHeader;

				// some headers can be 0-length, skip them ... (0-length:  len <=2)
				// some 0-length headers we are actually interested in; the 0-length
				// headers we do not care about should already be marked as HF_Unsupported
				while(m_iCurrentHeader < m_iHeadersCount
					  && (m_pHeaders[m_iCurrentHeader].type == HF_Unsupported
						  || m_pHeaders[m_iCurrentHeader].bDerivative)
					  /*m_pHeaders[m_iCurrentHeader].len <= 2*/)
				{
					bScrolledHeader = true;
					m_iCurrentHeader++;
				}

				// after the last header there is an extra paragraph
				// marker that is still a part of the header section --
				// we do not want that marker imported
				if(m_iCurrentHeader ==  m_iHeadersCount)
				{
					UT_DEBUGMSG(("End of header marker at pos %d\n", iDocPosition));
					return false;
				}
				
				// do not return, processing needs to continue ...
			}
			xxx_UT_DEBUGMSG(("iDocPosition %d m_pHeaders[m_iCurrentHeader].pos %d \n",iDocPosition,m_pHeaders[m_iCurrentHeader].pos));
			if((bScrolledHeader && m_pHeaders[iOrigHeader].pos == iDocPosition) ||
			   (!bScrolledHeader && iDocPosition == m_pHeaders[m_iCurrentHeader].pos))
			{
				return _insertHeaderSection(bDoBlockIns);
			}
		}
		else
		{
			UT_DEBUGMSG(("DOM: bad header joo joo\n"));
			return false;
		}

		// if we got this far, we are somwhere inside the header, just
		// process the character in a normal way
		return (m_pHeaders[m_iCurrentHeader].type != HF_Unsupported);
	}

	return true;
}

/*
   this function returns true if stuff at given position is to be ingored
   For example, the doc might contain headers in it that are not used ...
 */
bool IE_Imp_MsWord_97::_ignorePosition(UT_uint32 iDocPos)
{
	if(m_bInTOC && m_bTOCsupported)
		return true;
	
	if(m_bInHeaders && m_iCurrentHeader < m_iHeadersCount && m_pHeaders)
	{
		if(   m_pHeaders[m_iCurrentHeader].type == HF_Unsupported
		   || iDocPos < m_pHeaders[m_iCurrentHeader].pos)
		{
			return true;
		}
	}
	
	return false;
}
