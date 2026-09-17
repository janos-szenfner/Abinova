/* AbiSource Program Utilities
 * Copyright (C) 2001 AbiSource, Inc.
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

struct enc_entry
{
	gchar const ** encs;
	const gchar * desc;
	UT_uint32  id;
};

class ABI_EXPORT UT_Encoding
{
public:
	UT_Encoding();

	UT_uint32	getCount() const;
	const gchar * 	getNthEncoding(UT_uint32 n) const;
	const gchar * 	getNthDescription(UT_uint32 n) const;
	const gchar * 	getEncodingFromDescription(const gchar * desc) const;
	UT_uint32 	getIndxFromEncoding(const gchar * enc) const;
	UT_uint32 	getIdFromEncoding(const gchar * enc) const;

private:
	static bool	s_Init;
	static UT_uint32	s_iCount;
};
