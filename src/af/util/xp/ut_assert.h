/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Program Utilities
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2023 Hubert Figuière
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

#include "ut_debugmsg.h"

// TODO move these declarations into platform directories.

#if (defined (WIN32) || defined (_WIN32) || defined (_WIN64))
// The 'cool' win32 assert, at least with VC6, corrups memory (probably calling sprintf on
// a static buffer without checking bounds), so we implement our own assert dialog, which
// is even cooler. TF

#include "ut_types.h"

#ifdef NDEBUG
#  define UT_ASSERT(x)
#else
// This function is implemented in ut_Win32Misc.cpp. It is not declared in any header file (it is
// only to be referenced from here and we want to reduce the files we include here to a
// bare minimum for performance reasons, as this file gets included from pretty much
// everywhere).
extern int ABI_EXPORT UT_Win32ThrowAssert(const char * pCondition, const char * pFile, int iLine, int iCount);

// We want to track the number of times we have been through this assert and have the
// option of disabling this assert for the rest of the session; we use the __iCount and
// __bOnceOnly vars for this (this adds a few bytes to the code and footprint, but on
// large scale of things, this is quite negligible for the debug build).
#define UT_ASSERT(x)                                                        \
{                                                                           \
	static bool __bOnceOnly = false;                                        \
	static long __iCount = 0;                                               \
	if(!__bOnceOnly && !(x))                                                \
	{                                                                       \
		__iCount++;                                                         \
		int __iRet = UT_Win32ThrowAssert(#x,__FILE__, __LINE__, __iCount);  \
        if(__iRet == 0)                                                     \
		{                                                                   \
		   G_BREAKPOINT();                                                  \
		}                                                                   \
		else if(__iRet < 0)                                                 \
		{                                                                   \
			__bOnceOnly = true;                                             \
		}                                                                   \
	}                                                                       \
}

#endif // ifdef NDEBUG

// MacOS code.
#elif defined(TOOLKIT_COCOA)
#	ifdef NDEBUG
// When NDEBUG is defined, assert() does nothing.
// So we let the system header files take care of it.
#		include <assert.h>
#		define UT_ASSERT assert
#	else
// Please keep the "/**/" to stop MSVC dependency generator complaining.
#		include /**/ "xap_CocoaAssert.h"
#			define UT_ASSERT(expr)								\
			{												\
				static bool __bOnceOnly = false;			\
				if (!__bOnceOnly && !(expr)) {				\
					if (XAP_CocoaAssertMsg(#expr,			\
						__FILE__, __LINE__)) {				\
						__bOnceOnly = true;					\
					}										\
				} \
			}
#	endif
#else

// A Unix variant, possibly Gnome.

#	ifdef NDEBUG

		// When NDEBUG is defined, assert() does nothing.
		// So we let the system header files take care of it.

#		include <assert.h>
#		define UT_ASSERT assert
#	else
		// Otherwise, we want a slighly modified behavior.
		// We'd like assert() to ask us before crashing.
		// We treat asserts as logic flaws, which are sometimes
		// recoverable, but that should be noted.

#		include <assert.h>
// Please keep the "/**/" to stop MSVC dependency generator complaining.
#		include /**/ "ut_unixAssert.h"
#		define UT_ASSERT(expr)								\
		{										\
	        if (!ut_g_silent) { \
                static bool __bOnceOnly = false;                \
                if (!__bOnceOnly && !(expr)) {                        \
                    if (UT_UnixAssertMsg(#expr, __FILE__, __LINE__) == -1) { \
                        __bOnceOnly = true;                             \
                    } \
                }     \
            } \
		}
#	endif

#endif


/*!
 * This part is not implemented. Same as UT_TODO
 */
#define UT_NOT_IMPLEMENTED		0

/*!
 * This really shouldn't happen
 */
#define UT_SHOULD_NOT_HAPPEN	0

/*!
 * This part is left TODO
 */
#define UT_TODO					0

/*!
 * This line of code should not be reached
 */
#define UT_NOT_REACHED 0

/*!
 * This line of code should not be reached
 */
#define UT_ASSERT_NOT_REACHED() UT_ASSERT(UT_NOT_REACHED)

/*!
 * Trigger a debug assertion, but let the normal flow of code progress
 */
#define UT_ASSERT_HARMLESS(cond) UT_ASSERT(cond)

/*!
 * Just return from a function if this condition fails
 */
#define UT_return_if_fail(cond) if (!(cond)) { UT_ASSERT(cond); return; }

/*!
 * Just return this value from a function if this condition fails
 */
#define UT_return_val_if_fail(cond, val) if (!(cond)) { UT_ASSERT(cond); return (val); }

/*!
 * Throw if this condition fails
 */
#define UT_throw_if_fail(cond, val) if (!(cond)) { UT_ASSERT(cond); throw val; }

/*!
 * Continue if this condition fails
 */
#define UT_continue_if_fail(cond) if (!(cond)) { UT_ASSERT(cond); continue; }


// Assertions for nonnull. These assertion are mean to provide a semantic meaning
// of "a nullptr is an error".

/*!
 * Break out of loop if pointer is nullptr
 */
#define UT_nonnull_or_break(ptr) if (!ptr) { UT_ASSERT(ptr); break; }

/*!
 * Continue lookp if pointer is nullptr
 */
#define UT_nonnull_or_continue(ptr) if (!ptr) { UT_ASSERT(ptr); continue; }

/*!
 * Return if pointer is nullptr. Make `value` empty (still with the comma) to return
 * `void`.
 */
#define UT_nonnull_or_return(ptr, value) if (!ptr) { UT_ASSERT(ptr); return value; }
