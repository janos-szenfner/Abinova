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

#include "ut_types.h"

template<class T>
class UT_DebugOnly
{
public:

#if DEBUG
  T value;
  UT_DebugOnly()
  {
  }
  UT_DebugOnly(const T& other)
   : value(other)
  {
  }
  UT_DebugOnly& operator=(const T& rhs)
  {
      value = rhs;
      return *this;
  }
  void operator++(int)
  {
    value++;
  }
  void operator--(int)
  {
    value--;
  }

  operator T&()
  {
    return value;
  }
  operator const T&() const
  {
    return value;
  }

  T& operator->()
  {
    return value;
  }

#else
  UT_DebugOnly()
  {
  }
  UT_DebugOnly(const T&)
  {
  }
  UT_DebugOnly& operator=(const T&)
  {
    return *this;
  }
  void operator++(int)
  {
  }
  void operator--(int)
  {
  }
#endif

  ~UT_DebugOnly()
  {
  }
};

extern bool ut_g_silent;
ABI_EXPORT void UT_Debug_Init();

ABI_EXPORT void _UT_OutputMessage(const char *s, ...) ABI_PRINTF_FORMAT(1,2);

#ifdef DEBUG
#define UT_DEBUGMSG(M) _UT_OutputMessage M
#else
#define UT_DEBUGMSG(M)
#endif

// define a quick way to no-op a debug message that
// you want to keep and without having to #if 0 it.

#define xxx_UT_DEBUGMSG(M)


/*
 * Similar to UT_DEBUGMSG, except exists even in production (non-debug) builds
 */
#ifdef _WIN32
ABI_EXPORT void _UT_WarningMessage(const char *s, ...);
#else
#define _UT_WarningMessage _UT_OutputMessage
#endif
#define UT_WARNINGMSG(M) _UT_WarningMessage M
