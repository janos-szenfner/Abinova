/* AbiSource Program Utilities
 * Copyright (C) 2026 Abinova contributors
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

#include "ut_export.h"

#include <cstdio>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

/*!
 * Offline thesaurus reader for the MyThes (OpenOffice/LibreOffice)
 * file pair th_<lang>_v2.idx / th_<lang>_v2.dat.
 *
 * The .idx maps a headword to byte offset(s) in the .dat; each dat
 * record is "word|n" followed by n meaning lines
 * "(pos)|term|term (qualifier)|...", where the qualifier marks an
 * antonym or a relationship (generic/similar/related term).
 *
 * Lookup order for th_<lang>_v2 files:
 *   $ABINOVA_THESAURUS_DIR
 *   <the usual system dictionary dirs>
 *   szBundledDir (the app's installed thesaurus dir, a reduced set)
 */
class ABI_EXPORT UT_Thesaurus
{
public:
	struct Meaning
	{
		std::string					pos;		// "noun", "verb", ... or ""
		std::vector<std::string>	synonyms;
		std::vector<std::string>	antonyms;
	};

	/*! Return the thesaurus for szLang ("en-US"/"en_US"/"en"), or
	 * nullptr when no data for that language is installed anywhere.
	 * Instances are cached for the process lifetime. */
	static const UT_Thesaurus *	forLanguage(const char * szLang,
											const char * szBundledDir);

	/*! Fetch the meanings of szWord; returns false when the word is
	 * not in this thesaurus. */
	bool	lookup(const char * szWord,
				   std::vector<Meaning> & out) const;

	const std::string &	language() const { return m_lang; }

private:
	UT_Thesaurus();
	~UT_Thesaurus();

	bool	_open(const std::string & idxPath,
				  const std::string & datPath);

	std::string		m_lang;
	std::string		m_datPath;
	std::unordered_map<std::string, std::vector<long>> m_idx;
	mutable FILE *	m_dat;
};
