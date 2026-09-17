/* AbiSource Program Utilities
 * Copyright (C) 2001 Tomas Frydrych
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

#include "ut_types.h"

/* for comments on signficance of these constants see the cpp file */

#define UT_NOT_OVERSTRIKING 0
#define UT_OVERSTRIKING_LTR 1
#define UT_OVERSTRIKING_RTL 2

#define UT_OVERSTRIKING_RIGHT  0x10000000
#define UT_OVERSTRIKING_LEFT   0x20000000
#define UT_OVERSTRIKING_CENTRE 0x00000000
#define UT_OVERSTRIKING_DIR    0x0fffffff
#define UT_OVERSTRIKING_TYPE   0xf0000000

UT_uint32 UT_isOverstrikingChar(UT_UCS4Char c);

