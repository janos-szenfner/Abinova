/* AbiSource Application Framework
 * Copyright (C) 1998,1999 AbiSource, Inc.
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

#include <stdio.h>

#include "ut_types.h"
#include "ut_hash.h"

/*
	A simple custom dictionary class, which allows the user to add words
	which aren't in the standard dictionary.  The contents are stored as
	UTF-8 plaintext, with one word per line.
*/
class ABI_EXPORT XAP_Dictionary
{
public:
	XAP_Dictionary(const char * szFilename);
	~XAP_Dictionary();

	const char *		getShortName(void) const;

	bool				load(void);
	bool				save(void);
	UT_uint32                       countCommonChars(UT_UCS4Char * pszNeedle, UT_UCS4Char *pszHaystack);
	void                            suggestWord(UT_GenericVector<UT_UCS4Char *> * pVecSuggestions, const UT_UCS4Char * pWord, UT_uint32 len);
	bool                            addWord(const char * pWord);
	bool				addWord(const UT_UCS4Char * pWord, UT_uint32 len);
	bool				isWord(const UT_UCS4Char * pWord, UT_uint32 len) const;

protected:
	bool				_openFile(const char * mode);
	UT_uint32			_writeBytes(const UT_Byte * pBytes, UT_uint32 length);
	bool				_writeBytes(const UT_Byte * sz);
	bool				_closeFile(void);
	void				_abortFile(void);

	bool				_parseUTF8(void);
	void				_outputUTF8(const UT_UCS4Char * data, UT_uint32 length);

	char *		m_szFilename;

	bool				m_bDirty;
	UT_GenericStringMap<UT_UCS4Char *>	    m_hashWords;

private:
	FILE *				m_fp;
};
