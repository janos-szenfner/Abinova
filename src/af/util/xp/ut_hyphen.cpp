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

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>

#include <glib/gstdio.h>
#include <unordered_map>

#include "ut_debugmsg.h"
#include "ut_hyphen.h"

namespace {

bool s_isDirectiveChar(char c)
{
	return c >= 'A' && c <= 'Z';
}

/* Parse "name value" directive lines; returns the directive name and the
 * integer value (or the raw tail for NOHYPHEN). */
std::string_view s_directiveName(std::string_view line)
{
	size_t sp = line.find(' ');
	return line.substr(0, sp == std::string_view::npos ? line.size() : sp);
}

/* Parse one pattern line ("hy3phe", ".a2ch4", "an3ti.").
 * Returns false for unparseable/non-standard (contains '/') patterns. */
bool s_parsePattern(std::string_view line,
					std::string & key,
					std::vector<unsigned char> & weights)
{
	key.clear();
	key.reserve(line.size());
	weights.clear();

	unsigned w = 0;
	for (char ch : line)
	{
		if (ch == '/')
		{
			/* non-standard hyphenation (replacement text, e.g. German
			 * ck -> k-k): we do not support substitutions, so the
			 * pattern is skipped entirely -- conservative: it simply
			 * does not yield a break point. */
			return false;
		}
		if (ch >= '0' && ch <= '9')
		{
			w = w * 10 + static_cast<unsigned>(ch - '0');
			continue;
		}
		/* a new boundary slot begins with this letter */
		key += ch;
		weights.push_back(static_cast<unsigned char>(std::min(w, 9u)));
		w = 0;
	}
	weights.push_back(static_cast<unsigned char>(std::min(w, 9u)));

	return !key.empty();
}

} // anonymous namespace

std::unique_ptr<UT_HyphenDict> UT_HyphenDict::load(const char * szPath)
{
	FILE * fp = g_fopen(szPath, "rb");
	if (!fp)
		return nullptr;

	auto pDict = std::unique_ptr<UT_HyphenDict>(new UT_HyphenDict);
	std::string line;
	bool bFirstContent = true;
	char buf[512];

	while (fgets(buf, sizeof(buf), fp))
	{
		line.assign(buf);
		/* long lines: keep consuming the tail so the next fgets starts
		 * on a line boundary */
		while (!line.empty() && line.back() != '\n' && !feof(fp))
		{
			if (!fgets(buf, sizeof(buf), fp))
				break;
			line += buf;
		}
		while (!line.empty() &&
			   (line.back() == '\n' || line.back() == '\r' ||
				line.back() == ' ' || line.back() == '\t'))
			line.pop_back();
		if (line.empty() || line[0] == '%')
			continue;

		std::string_view sv(line);
		std::string_view name = s_directiveName(sv);

		if (bFirstContent && name.size() == line.size() &&
			std::all_of(name.begin(), name.end(),
						[](char c) { return s_isDirectiveChar(c) || c == '-' ||
										 (c >= '0' && c <= '9'); }))
		{
			/* encoding header, e.g. "UTF-8" or "ISO8859-1".  Patterns are
			 * matched on raw bytes, so a non-UTF-8 dictionary can still
			 * be loaded -- its non-ASCII patterns simply never match
			 * UTF-8 words (conservative). */
			bFirstContent = false;
			continue;
		}
		bFirstContent = false;

		bool bDirective = !name.empty() && s_isDirectiveChar(name[0]);
		if (bDirective)
		{
			std::string_view tail = sv.size() > name.size()
				? sv.substr(name.size() + 1) : std::string_view();
			int val = tail.empty() ? 0 : atoi(std::string(tail).c_str());

			if (name == "LEFTHYPHENMIN")
				pDict->m_iLeftMin = std::max(1, val);
			else if (name == "RIGHTHYPHENMIN")
				pDict->m_iRightMin = std::max(1, val);
			else if (name == "COMPOUNDLEFTHYPHENMIN")
				pDict->m_iLeftMin = std::max(pDict->m_iLeftMin, val);
			else if (name == "COMPOUNDRIGHTHYPHENMIN")
				pDict->m_iRightMin = std::max(pDict->m_iRightMin, val);
			else if (name == "NEXTLEVEL")
			{
				/* libhyphen gives second-level patterns separate
				 * treatment inside compound words; we merge them into
				 * the one pattern map -- an approximation that only
				 * adds break points on compound boundaries. */
			}
			else if (name == "NOHYPHEN")
			{
				size_t pos = 0;
				while (pos <= tail.size())
				{
					size_t comma = tail.find(',', pos);
					std::string seq(tail.substr(pos,
						comma == std::string_view::npos
							? std::string_view::npos : comma - pos));
					if (!seq.empty())
						pDict->m_noHyph.push_back(seq);
					if (comma == std::string_view::npos)
						break;
					pos = comma + 1;
				}
			}
			continue;
		}

		std::string key;
		std::vector<unsigned char> weights;
		if (!s_parsePattern(sv, key, weights))
			continue;

		pDict->m_iMaxPatLen = std::max(pDict->m_iMaxPatLen, key.size());
		auto it = pDict->m_patterns.find(key);
		if (it == pDict->m_patterns.end())
		{
			pDict->m_patterns.emplace(std::move(key), std::move(weights));
		}
		else
		{
			/* duplicate pattern across NEXTLEVEL blocks: keep the max
			 * weight per boundary slot */
			for (size_t j = 0; j < weights.size() && j < it->second.size(); ++j)
				it->second[j] = std::max(it->second[j], weights[j]);
		}
	}

	fclose(fp);
	if (pDict->m_patterns.empty())
		return nullptr;
	return pDict;
}

void UT_HyphenDict::hyphenate(const std::string & sWordUtf8,
							  std::vector<char> & breaks) const
{
	const size_t n = sWordUtf8.size();
	breaks.assign(n, 0);
	if (n < static_cast<size_t>(m_iLeftMin + m_iRightMin + 1))
		return;

	/* Liang's algorithm: pad with '.', find every pattern occurrence and
	 * take the maximum weight at each boundary; odd weights mean "break". */
	std::string padded;
	padded.reserve(n + 2);
	padded += '.';
	for (char c : sWordUtf8)
		padded += (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
	padded += '.';

	std::vector<unsigned char> scores(padded.size() + 1, 0);
	std::string_view pv(padded);
	for (size_t i = 0; i < padded.size(); ++i)
	{
		size_t iMax = std::min(m_iMaxPatLen, padded.size() - i);
		for (size_t len = 1; len <= iMax; ++len)
		{
			auto it = m_patterns.find(pv.substr(i, len));
			if (it == m_patterns.end())
				continue;
			const std::vector<unsigned char> & w = it->second;
			for (size_t j = 0; j < w.size(); ++j)
				if (scores[i + j] < w[j])
					scores[i + j] = w[j];
		}
	}

	const size_t iFirst = static_cast<size_t>(m_iLeftMin) - 1;
	const size_t iLast = n - static_cast<size_t>(m_iRightMin);
	for (size_t i = iFirst; i < iLast; ++i)
	{
		/* break after word byte i is the boundary before padded[i+2] */
		if (scores[i + 2] & 1)
			breaks[i] = static_cast<char>(scores[i + 2]);
	}

	for (const std::string & seq : m_noHyph)
	{
		size_t pos = padded.find(seq);
		while (pos != std::string::npos)
		{
			/* suppress breaks inside the forbidden sequence (pos-1 maps
			 * padded indices back to word byte indices) */
			for (size_t i = pos; i + 1 < pos + seq.size() && i - 1 < n; ++i)
				if (i >= 1)
					breaks[i - 1] = 0;
			pos = padded.find(seq, pos + 1);
		}
	}
}

namespace {

void s_candidateDirs(std::vector<std::string> & dirs)
{
	/* explicit override (used by tests + packagers); separated by the
	   platform search-path separator (':' POSIX, ';' Windows) */
	const char * env = getenv("ABINOVA_HYPHEN_PATH");
	if (env && *env)
	{
		gchar ** paths = g_strsplit(env, G_SEARCHPATH_SEPARATOR_S, -1);
		for (gchar ** pp = paths; pp && *pp; pp++)
			dirs.push_back(*pp);
		g_strfreev(paths);
		return;
	}

#ifdef G_OS_WIN32
	/* mirror the hunspell dictionary search dirs: hyph_*.dic files ship
	   in the same locations on Windows */
	const char * appdata = getenv("APPDATA");
	if (appdata && *appdata)
	{
		dirs.push_back(std::string(appdata) + "/hunspell");
		dirs.push_back(std::string(appdata) + "/hyphen");
	}
	const char * localappdata = getenv("LOCALAPPDATA");
	if (localappdata && *localappdata)
	{
		dirs.push_back(std::string(localappdata) + "/hunspell");
		dirs.push_back(std::string(localappdata) + "/hyphen");
	}
	gchar * installDir = g_win32_get_package_installation_directory_of_module(nullptr);
	if (installDir)
	{
		dirs.push_back(std::string(installDir) + "/hunspell");
		dirs.push_back(std::string(installDir) + "/hyphen");
		dirs.push_back(std::string(installDir) + "/share/hunspell");
		dirs.push_back(std::string(installDir) + "/share/hyphen");
		g_free(installDir);
	}
#else
	/* g_get_user_data_dir() honours XDG_DATA_HOME and falls back to
	   ~/.local/share; g_get_system_data_dirs() honours XDG_DATA_DIRS
	   with the /usr/local/share:/usr/share default */
	dirs.push_back(std::string(g_get_user_data_dir()) + "/hyphen");

	for (const gchar * const * pp = g_get_system_data_dirs(); pp && *pp; pp++)
	{
		dirs.push_back(std::string(*pp) + "/hyphen");
		dirs.push_back(std::string(*pp) + "/hunspell");
	}

#ifdef __APPLE__
	const gchar * home = g_get_home_dir();
	if (home)
		dirs.push_back(std::string(home) + "/Library/Spelling");
	dirs.push_back("/Library/Spelling");
#endif
#endif
}

} // anonymous namespace

const UT_HyphenDict * UT_Hyphenator::getDict(const char * szLang)
{
	if (!szLang || !*szLang)
		return nullptr;

	/* "en-US" -> "en_US" (hyph filenames keep their case: hyph_en_US.dic) */
	std::string lang(szLang);
	for (char & c : lang)
		if (c == '-')
			c = '_';

	static std::mutex s_mutex;
	static std::unordered_map<std::string, std::unique_ptr<UT_HyphenDict>>
		s_cache;
	std::lock_guard<std::mutex> lock(s_mutex);

	auto it = s_cache.find(lang);
	if (it != s_cache.end())
		return it->second.get();

	std::vector<std::string> dirs;
	s_candidateDirs(dirs);

	/* filename candidates: hyph_<lang>.dic then base-language fallbacks */
	std::vector<std::string> names;
	names.push_back("hyph_" + lang + ".dic");
	size_t us = lang.find('_');
	if (us != std::string::npos)
		names.push_back("hyph_" + lang.substr(0, us) + ".dic");

	std::unique_ptr<UT_HyphenDict> pDict;
	for (const std::string & d : dirs)
	{
		if (pDict)
			break;
		for (const std::string & f : names)
		{
			std::string path = d + "/" + f;
			pDict = UT_HyphenDict::load(path.c_str());
			if (pDict)
			{
				xxx_UT_DEBUGMSG(("loaded hyphenation dictionary %s\n",
								 path.c_str()));
				break;
			}
		}
	}

	const UT_HyphenDict * pRet = pDict.get();
	s_cache.emplace(lang, std::move(pDict));
	return pRet;
}
