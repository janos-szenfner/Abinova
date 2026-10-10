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

#include <stdio.h>
#include <memory>

#include <gsf/gsf.h>

#include "ut_types.h"

/* Solaris's unistd.h does #define truncate truncate64, which would
 * rewrite the truncate() member declared below in any TU that
 * included unistd.h first. Undef it here as a guard. - fjf
 */
#ifdef truncate
#undef truncate
#endif

class ABI_EXPORT UT_ByteBuf
{
public:
	UT_ByteBuf(UT_uint32 iChunk = 0);
	~UT_ByteBuf();

	bool				append(const UT_Byte * pValue, UT_uint32 length);
	bool				ins(UT_uint32 position, const UT_Byte * pValue, UT_uint32 length);
	bool				ins(UT_uint32 position, UT_uint32 length);
	bool				del(UT_uint32 position, UT_uint32 amount);
	bool				overwrite(UT_uint32 position, const UT_Byte * pValue, UT_uint32 length);
	void				truncate(UT_uint32 position);
	UT_uint32			getLength(void) const;
	const UT_Byte *		getPointer(UT_uint32 position) const;				/* temporary use only */
	bool				writeToURI(const char* pszURI) const;
	bool				insertFromFile(UT_uint32 iPosition, const char* pszFilename);
	bool                            insertFromInput(UT_uint32 iPosition, GsfInput * fp);
	bool                insertFromFile(UT_uint32 iPosition, FILE * fp);
private:
	bool				_byteBuf(UT_uint32 spaceNeeded);

	UT_Byte *			m_pBuf;
	UT_uint32			m_iSize;			/* amount currently used */
	UT_uint32			m_iSpace;			/* space currently allocated */
	UT_uint32			m_iChunk;			/* unit for g_try_realloc */
};

typedef std::shared_ptr<UT_ByteBuf> UT_ByteBufPtr;
typedef std::shared_ptr<const UT_ByteBuf> UT_ConstByteBufPtr;
