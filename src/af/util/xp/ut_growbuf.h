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
/*****************************************************************
** A buffer class which can grow and shrink
*****************************************************************/

#include "ut_types.h"

/* Solaris's unistd.h does #define truncate truncate64, which would
 * rewrite the truncate() member declared below in any TU that
 * included unistd.h first. Undef it here as a guard. - fjf
 */
#ifdef truncate
#undef truncate
#endif

class ABI_EXPORT UT_GrowBuf
{
public:
	UT_GrowBuf(UT_uint32 iChunk = 0);
	~UT_GrowBuf();

	bool				append(const UT_GrowBufElement * pValue, UT_uint32 length);
	bool				ins(UT_uint32 position, const UT_GrowBufElement * pValue, UT_uint32 length);
	bool				ins(UT_uint32 position, UT_uint32 length);
	bool				del(UT_uint32 position, UT_uint32 amount);
	bool				overwrite(UT_uint32 position, UT_GrowBufElement * pValue, UT_uint32 length);
	void				truncate(UT_uint32 position);
	UT_uint32			getLength(void) const;

	UT_GrowBufElement *      getPointer(UT_uint32 position) const; /* temporary use only */

private:
	bool				_growBuf(UT_uint32 spaceNeeded);

	UT_GrowBufElement *		m_pBuf;
	UT_uint32			m_iSize;			/* amount currently used */
	UT_uint32			m_iSpace;			/* space currently allocated */
	UT_uint32			m_iChunk;			/* unit for g_try_realloc */
};
