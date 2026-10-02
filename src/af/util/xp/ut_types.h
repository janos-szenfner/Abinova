/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Program Utilities
 * Copyright (C) 1998 AbiSource, Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <cstdint>
#include <glib.h>

/*
 *  This macro allow using GNUC extension with -pedantic
 */
#if     __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 8)
#  define GNUC_EXTENSION __extension__
#else
#  define GNUC_EXTENSION
#endif

typedef uint8_t		UT_Byte;

typedef uint32_t        UT_UCS4Char;
typedef uint16_t        UT_UCS2Char;
typedef int32_t         UT_GrowBufElement;

/* NOTA BENE: UT_UCSChar is deprecated; all new code must use
   UT_UCS4Char instead !!! */
[[deprecated("Use UT_UCS4Char instead")]]
typedef UT_UCS4Char		UT_UCSChar;	/* Unicode */

typedef uint8_t       UT_uint8;
typedef int8_t        UT_sint8;

typedef uint16_t		UT_uint16;
typedef int16_t       UT_sint16;

typedef uint32_t		UT_uint32;
typedef int32_t		    UT_sint32;

typedef uint64_t UT_uint64;
typedef int64_t UT_sint64;

#ifdef _WIN64
typedef guint64 	UT_uintptr;
typedef gint64 	UT_sintptr;
#else
typedef uintptr_t UT_uintptr;
typedef intptr_t        UT_sintptr;
#endif

/** use to mark variable as unused */
#define UT_UNUSED(x) static_cast<void>((x));

/** use to mark an argument as used in debug only
 *  otherwise equivalent to UT_UNUSED.
 *  If it is a local variable use UT_DebugOnly<> instead.
 */
#ifdef DEBUG
#define UT_DEBUG_ONLY_ARG(x)
#else
#define UT_DEBUG_ONLY_ARG(x) static_cast<void>((x));
#endif

/*!
 * Confidence heuristic datatype normalized to the range
 * [0,255] with 0 being least confident and 255 being the most confident
 */
typedef UT_uint8 UT_Confidence_t;

#define UT_CONFIDENCE_PERFECT 255
#define UT_CONFIDENCE_GOOD    170
#define UT_CONFIDENCE_SOSO    127
#define UT_CONFIDENCE_POOR     85
#define UT_CONFIDENCE_ZILCH     0

#include "ut_export.h" // ABI_EXPORT is defined in there.

#if __GNUC__
  #define ABI_NORETURN __attribute__((noreturn))
  #define ABI_PRINTF_FORMAT(f,a) __attribute__ ((format (printf, f, a)))
/// Call this way ABI_NONNULL(1,2)
  #define ABI_NONNULL(...) __attribute__((nonnull (__VA_ARGS__)))
  #define ABI_RET_NONNULL __attribute__ ((returns_nonnull))
#else
  #define ABI_NORETURN
  #define ABI_PRINTF_FORMAT(f,a)
  #define ABI_NONNULL(...)
  #define ABI_RET_NONNULL
#endif

/* ABI_FAR_CALL: C function that we want to expose across plugin boundaries */
#define ABI_FAR_CALL extern "C" ABI_PLUGIN_EXPORT

#define _abi_callonce /* only call me once! */

/*
	UT_Error should be used far more than it is.  Any function
	which reasonably could fail at runtime for anything other than
	a coding error or bug should return an error code.  Error codes
	should be propogated properly.

	Addendum: 1-23-99
	If you have any problems with or suggestions for error codes,
	please send them to Sam Tobin-Hochstadt (sytobinh@uchicago.edu).
	I am the person that has worked the most with them.
*/
typedef	UT_sint32		UT_Error;
#define	UT_OK			(static_cast<UT_Error>( 0))
#define	UT_ERROR            	(static_cast<UT_Error>( -1)) 	/* VERY generic */
#define UT_OUTOFMEM		(static_cast<UT_Error>( -100))
#define UT_SAVE_WRITEERROR      (static_cast<UT_Error>( -201))
#define UT_SAVE_NAMEERROR       (static_cast<UT_Error>( -202))
#define UT_SAVE_EXPORTERROR     (static_cast<UT_Error>( -203))
#define UT_EXTENSIONERROR       (static_cast<UT_Error>( -204))
#define UT_SAVE_CANCELLED       (static_cast<UT_Error>( -205))
#define UT_SAVE_OTHERERROR      (static_cast<UT_Error>( -200)) 	/* This should eventually dissapear. */
#define UT_IE_FILENOTFOUND      (static_cast<UT_Error>( -301))
#define UT_IE_NOMEMORY          (static_cast<UT_Error>( -302))
#define UT_IE_UNKNOWNTYPE       (static_cast<UT_Error>( -303))
#define UT_IE_BOGUSDOCUMENT     (static_cast<UT_Error>( -304))
#define UT_IE_COULDNOTOPEN      (static_cast<UT_Error>( -305))
#define UT_IE_COULDNOTWRITE     (static_cast<UT_Error>( -306))
#define UT_IE_FAKETYPE          (static_cast<UT_Error>( -307))
#define UT_INVALIDFILENAME      (static_cast<UT_Error>( -308))
#define UT_NOPIECETABLE         (static_cast<UT_Error>( -309))
#define UT_IE_ADDLISTENERERROR  (static_cast<UT_Error>( -310))
#define UT_IE_UNSUPTYPE         (static_cast<UT_Error>( -311))
#define UT_IE_PROTECTED         (static_cast<UT_Error>( -312))       // (pass) protected doc
#define UT_IE_SKIPINVALID       (static_cast<UT_Error>( -313))       // (pass) protected doc
#define UT_IE_IMPORTERROR       (static_cast<UT_Error>( -300)) 	/* The general case */
#define UT_IE_IMPSTYLEUNSUPPORTED  (static_cast<UT_Error>( -314))
#define UT_IE_XMLNOANGLEBRACKET    (static_cast<UT_Error>( -360))
#define UT_IE_TRY_RECOVER          (static_cast<UT_Error>( -350))    // try recovering the document. ie, we have
                                                        // imported something

#define UT_IS_IE_SUCCESS(x) (((x) == UT_OK) || ((x) == UT_IE_TRY_RECOVER))

ABI_EXPORT UT_Error UT_errnoToUTError (void);

/* defined in ut_misc.cpp */
ABI_EXPORT void * UT_calloc ( UT_uint32 nmemb, UT_uint32 size );

/*
	The MSVC debug runtime library can track leaks back to the
	original allocation via the following black magic.
*/
#if defined(_MSC_VER) && defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#include <crtdbg.h>
#define UT_DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new UT_DEBUG_NEW
#endif /* _MSC_VER && _DEBUG && _CRTDBG_MAP_ALLOC */


/* Unicode character constants.  Try to use these rather than
** decimal or hex constants throughout the code.  See also bug
** 512.
*/

/* When objects (fields, etc) must be represented in unicode, use the
   BELL code and let UT_isWordDelimiter recognize it as a word
   character. See bug 223.  */
#define UCS_ABI_OBJECT	(static_cast<UT_UCS4Char>(0x0007))

#define UCS_TAB			(static_cast<UT_UCS4Char>(0x0009))
#define UCS_LF			(static_cast<UT_UCS4Char>(0x000a))
#define UCS_VTAB		(static_cast<UT_UCS4Char>(0x000b))
#define UCS_FF			(static_cast<UT_UCS4Char>(0x000c))
#define UCS_CR			(static_cast<UT_UCS4Char>(0x000d))
#define UCS_SPACE		(static_cast<UT_UCS4Char>(0x0020))
#define UCS_NBSP		(static_cast<UT_UCS4Char>(0x00a0))
#define UCS_PILCROW		(static_cast<UT_UCS4Char>(0x00b6))
#define UCS_LINESEP		(static_cast<UT_UCS4Char>(0x2028))			/* Unicode line separator */
#define UCS_PARASEP		(static_cast<UT_UCS4Char>(0x2029))			/* Unicode paragraph separator */
#define UCS_BOM			(static_cast<UT_UCS4Char>(0xFEFF))			/* Byte order mark */
#define UCS_REPLACECHAR	(static_cast<UT_UCS4Char>(0xFFFD))
#define UCS_HYPHEN      (static_cast<UT_UCS4Char>(0x2010))
#define UCS_MINUS       (static_cast<UT_UCS4Char>(0x2d))

/* Note: the following are our interpretations, not Unicode's */
/* Note: use Unicode Private Use Area 0xE000 - 0xF8FF         */
/* Note: GB18030 mandates U+E000 - U+E765 for UDAs 1, 2 and 3 */
/* Note: BIG5-HKSCS uses U+E000 - U+F848                      */
/* Note: Please update UCS_ABICONTROL_START/END if more       */
/* Note: special values are added.  We need to watch out for  */
/* Note: them during import                                   */
#define UCS_ABICONTROL_START	(UCS_FIELDSTART)
#define UCS_FIELDSTART		(static_cast<UT_UCS4Char>(0xF850))
#define UCS_FIELDEND		(static_cast<UT_UCS4Char>(0xF851))
#define UCS_BOOKMARKSTART	(static_cast<UT_UCS4Char>(0xF852))
#define UCS_BOOKMARKEND		(static_cast<UT_UCS4Char>(0xF853))
#define UCS_LIGATURE_PLACEHOLDER (static_cast<UT_UCS4Char>(0xF854))
#define UCS_ABICONTROL_END	(UCS_LIGATURE_PLACEHOLDER)


#if 1 /* try to use the unicode values for special chars */
#define UCS_EN_SPACE		(static_cast<UT_UCS4Char>(0x2002))
#define UCS_EM_SPACE		(static_cast<UT_UCS4Char>(0x2003))
#define UCS_EN_DASH		(static_cast<UT_UCS4Char>(0x2013))
#define UCS_EM_DASH		(static_cast<UT_UCS4Char>(0x2014))
#define UCS_BULLET		(static_cast<UT_UCS4Char>(0x2022))
/* TODO Quote marks need to be localized - not hard-coded */
#define UCS_LQUOTE		(static_cast<UT_UCS4Char>(0x2018))
#define UCS_RQUOTE		(static_cast<UT_UCS4Char>(0x2019))
#define UCS_LDBLQUOTE		(static_cast<UT_UCS4Char>(0x201c))
#define UCS_RDBLQUOTE		(static_cast<UT_UCS4Char>(0x201d))

/* Note: the following is our interpretation, not Unicode's */
#define UCS_UNKPUNK 		(static_cast<UT_UCS4Char>(0xFFFF))  /* "unknown punctuation" used with UT_isWordDelimiter() */

#else /* see bug 512 */

#define UCS_EN_SPACE		(static_cast<UT_UCS4Char>(0x0020))
#define UCS_EM_SPACE		(static_cast<UT_UCS4Char>(0x0020))
#define UCS_EN_DASH		(static_cast<UT_UCS4Char>(0x002d))
#define UCS_EM_DASH		(static_cast<UT_UCS4Char>(0x002d))
#define UCS_BULLET		(static_cast<UT_UCS4Char>(0x0095))
#define UCS_LQUOTE		(static_cast<UT_UCS4Char>(0x0027))
#define UCS_RQUOTE		(static_cast<UT_UCS4Char>(0x0027))
#define UCS_LDBLQUOTE		(static_cast<UT_UCS4Char>(0x0022))
#define UCS_RDBLQUOTE		(static_cast<UT_UCS4Char>(0x0022))
#define UCS_UNKPUNK 		(static_cast<UT_UCS4Char>(0x00FF))

#endif

/* direction markers */
#define UCS_LRM 0x200E
#define UCS_RLM 0x200F
#define UCS_LRE 0x202a
#define UCS_RLE 0x202b
#define UCS_PDF 0x202c
#define UCS_LRO 0x202d
#define UCS_RLO 0x202e

/*
** Some useful macros that we use throughout
*/

#define FREEP(p)		do { if (p) { g_free(const_cast<void *>(static_cast<const void *>(p))); (p)=nullptr; } } while (0)
#define DELETEP(p)		do { if (p) { delete(p); (p)=nullptr; } } while (0)
#define DELETEPV(pa)	do { if (pa) { delete [] (pa); (pa)=nullptr; } } while (0)
#define REPLACEP(p,q)		do { if (p) delete p; p = q; } while (0)
#define REFP(p)			((p)->ref(), (p))
#define UNREFP(p)		do { if (p) { (p)->unref(); (p)=nullptr; } } while (0)
#define CLONEP(p,q)		do { FREEP(p); if (q && *q) p = g_strdup(q); } while (0)

#define E2B(err)		((err) == UT_OK)

/* This is a value from the private-use space of FriBidi */
#define FRIBIDI_TYPE_UNSET -1
#define FRIBIDI_TYPE_IGNORE -2

// this is maximum revision level; it is intentionally not defined as
// 0xffffffff to avoid problems with bad 64-bit compilers

#define PD_MAX_REVISION 0x0fffffff
