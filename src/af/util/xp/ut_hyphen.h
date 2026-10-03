/* Abinova
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

#ifndef UT_HYPHEN_H
#define UT_HYPHEN_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ut_export.h"

/*!
 * TeX-style (Liang) hyphenation on hyph_*.dic pattern dictionaries
 * (the same files libhyphen consumes, e.g. /usr/share/hyphen/hyph_en_US.dic).
 *
 * UT_Hyphenator::getDict() resolves a BCP-47-ish language tag ("en-US")
 * to a loaded dictionary, caching per language for the app lifetime; it
 * returns nullptr when no dictionary is available so callers can degrade
 * silently to "no hyphenation".
 */
class ABI_EXPORT UT_HyphenDict
{
public:
	/*! Load a hyph_*.dic file; nullptr on failure. */
	static std::unique_ptr<UT_HyphenDict> load(const char * szPath);

	/*!
	 * Compute legal break points of a UTF-8 word.
	 * \param sWordUtf8 the word (letters only; no spaces/punctuation)
	 * \param breaks    out: resized to sWordUtf8.size(); breaks[i] != 0
	 *                  means a hyphenation point exists after byte i.
	 */
	void hyphenate(const std::string & sWordUtf8,
				   std::vector<char> & breaks) const;

	int leftMin() const { return m_iLeftMin; }
	int rightMin() const { return m_iRightMin; }

private:
	/* pattern key (letters + optional '.' anchors) -> per-boundary weights;
	 * std::less<> gives heterogeneous string_view lookup */
	std::map<std::string, std::vector<unsigned char>, std::less<>> m_patterns;
	std::vector<std::string> m_noHyph;
	size_t m_iMaxPatLen = 0;
	int m_iLeftMin = 2;
	int m_iRightMin = 3;
};

class ABI_EXPORT UT_Hyphenator
{
public:
	/*! Cached per-language dictionary lookup; nullptr if unavailable. */
	static const UT_HyphenDict * getDict(const char * szLang);
};

#endif /* UT_HYPHEN_H */
