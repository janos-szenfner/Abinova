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

#include "config.h"

#include <cstdlib>
#include <cstring>
#include <glib.h>

#include "ut_Thesaurus.h"

namespace {

/* directories probed for th_<lang>_v2.{idx,dat} before the bundled
 * reduced set in szBundledDir; $ABINOVA_THESAURUS_DIR overrides all */
std::vector<std::string> s_searchDirs(const char * szBundledDir)
{
	std::vector<std::string> dirs;
	const char * env = getenv("ABINOVA_THESAURUS_DIR");
	if (env && *env)
	{
		dirs.emplace_back(env);
		return dirs;
	}
	dirs.emplace_back("/usr/share/mythes");
	dirs.emplace_back("/usr/share/hunspell");
	dirs.emplace_back("/usr/share/myspell");
	dirs.emplace_back("/usr/local/share/mythes");
	const char * home = g_get_home_dir();
	if (home && *home)
		dirs.emplace_back(std::string(home) + "/.local/share/mythes");
	if (szBundledDir && *szBundledDir)
		dirs.emplace_back(szBundledDir);
	return dirs;
}

/* "en-US"/"en_US"/"EN_us" -> "en_US", "" on empty */
std::string s_normLang(const char * szLang)
{
	std::string l = szLang ? szLang : "";
	for (auto & c : l)
	{
		if (c == '-')
			c = '_';
	}
	return l;
}

/* candidate basenames for th_<cand>_v2 files: the full tag, the
 * language part alone, and (handled by dir scan) region variants */
std::vector<std::string> s_langCandidates(const std::string & lang)
{
	std::vector<std::string> c;
	if (lang.empty())
		return c;
	c.push_back(lang);
	std::string::size_type u = lang.find('_');
	if (u != std::string::npos && u > 0)
		c.push_back(lang.substr(0, u));
	return c;
}

/* does <dir>/th_<cand>_v2.{idx,dat} exist? */
bool s_pairIn(const std::string & dir, const std::string & cand,
			  std::string & idxPath, std::string & datPath)
{
	std::string base = dir + "/th_" + cand + "_v2";
	idxPath = base + ".idx";
	datPath = base + ".dat";
	return g_file_test(idxPath.c_str(), G_FILE_TEST_IS_REGULAR) &&
		g_file_test(datPath.c_str(), G_FILE_TEST_IS_REGULAR);
}

/* scan dir for any th_<langprefix>_*_v2.idx pair (region variants) */
bool s_scanVariants(const std::string & dir, const std::string & langPrefix,
					std::string & idxPath, std::string & datPath)
{
	GDir * d = g_dir_open(dir.c_str(), 0, nullptr);
	if (!d)
		return false;
	std::string pfx = "th_" + langPrefix + "_";
	bool found = false;
	const gchar * name;
	std::string best;
	while (!found && (name = g_dir_read_name(d)))
	{
		std::string n = name;
		if (n.compare(0, pfx.size(), pfx) != 0)
			continue;
		if (n.size() < 8 || n.compare(n.size() - 7, 7, "_v2.idx") != 0)
			continue;
		std::string stem = n.substr(0, n.size() - 4);
		std::string dp = dir + "/" + stem + ".dat";
		if (g_file_test(dp.c_str(), G_FILE_TEST_IS_REGULAR))
		{
			idxPath = dir + "/" + n;
			datPath = dp;
			found = true;
		}
	}
	g_dir_close(d);
	return found;
}

/* cached instances keyed by resolved "dir/th_x_v2" stem; a present
 * but null entry marks a language we already failed to resolve */
std::map<std::string, const UT_Thesaurus *> & s_cache()
{
	static std::map<std::string, const UT_Thesaurus *> m;
	return m;
}

} // namespace

UT_Thesaurus::UT_Thesaurus()
	: m_dat(nullptr)
{
}

UT_Thesaurus::~UT_Thesaurus()
{
	if (m_dat)
		fclose(m_dat);
}

const UT_Thesaurus * UT_Thesaurus::forLanguage(const char * szLang,
											   const char * szBundledDir)
{
	const std::string lang = s_normLang(szLang);
	const std::string key = lang + "|" + (szBundledDir ? szBundledDir : "");
	auto it = s_cache().find(key);
	if (it != s_cache().end())
		return it->second;

	std::string idxPath, datPath;
	const std::string prefix = lang.substr(0, lang.find('_'));
	for (const std::string & dir : s_searchDirs(szBundledDir))
	{
		bool got = false;
		for (const std::string & cand : s_langCandidates(lang))
		{
			if (s_pairIn(dir, cand, idxPath, datPath))
			{
				got = true;
				break;
			}
		}
		if (!got && !prefix.empty())
			got = s_scanVariants(dir, prefix, idxPath, datPath);
		if (got)
			break;
	}

	UT_Thesaurus * t = nullptr;
	if (!idxPath.empty())
	{
		t = new UT_Thesaurus();
		if (!t->_open(idxPath, datPath))
		{
			delete t;
			t = nullptr;
		}
		else
		{
			t->m_lang = lang;
		}
	}
	s_cache()[key] = t;
	return t;
}

bool UT_Thesaurus::_open(const std::string & idxPath,
						 const std::string & datPath)
{
	FILE * f = fopen(idxPath.c_str(), "rb");
	if (!f)
		return false;
	char line[1024];
	if (!fgets(line, sizeof(line), f))
	{
		fclose(f);
		return false;
	}
	while (fgets(line, sizeof(line), f))
	{
		char * bar = strrchr(line, '|');
		if (!bar || bar == line)
			continue;
		*bar = 0;
		const long off = strtol(bar + 1, nullptr, 10);
		/* the word itself may legally contain '|'?? no — mythes
		 * headwords never do, and the offset is always the last
		 * field; strrchr is correct */
		m_idx[line].push_back(off);
	}
	fclose(f);
	m_datPath = datPath;
	return !m_idx.empty();
}

/* split one dat meaning line "(pos)|t1|t2 (generic term)|t3 (antonym)"
 * into pos + synonym/antonym vectors; relationship qualifiers are
 * stripped so callers show plain terms */
static void s_parseMeaningLine(char * line, UT_Thesaurus::Meaning & m)
{
	char * p = line;
	if (*p == '(')
	{
		char * close = strchr(p, ')');
		if (close)
		{
			m.pos.assign(p + 1, close - p - 1);
			p = close + 1;
		}
	}
	if (*p == '|')
		++p;
	char * save = nullptr;
	for (char * tok = strtok_r(p, "|", &save); tok;
		 tok = strtok_r(nullptr, "|", &save))
	{
		std::string term = tok;
		/* strip a trailing " (qualifier)" — the last paren group is
		 * always the relationship tag */
		bool antonym = false;
		const std::string::size_type ob = term.rfind(" (");
		if (ob != std::string::npos && term.back() == ')')
		{
			const std::string q = term.substr(ob + 2,
											  term.size() - ob - 3);
			antonym = (q.find("antonym") != std::string::npos);
			term.erase(ob);
		}
		if (term.empty())
			continue;
		std::vector<std::string> & dst =
			antonym ? m.antonyms : m.synonyms;
		bool dup = false;
		for (const std::string & s : dst)
			if (s == term)
			{
				dup = true;
				break;
			}
		if (!dup)
			dst.push_back(term);
	}
}

bool UT_Thesaurus::lookup(const char * szWord,
						  std::vector<Meaning> & out) const
{
	out.clear();
	if (!szWord || !*szWord)
		return false;
	auto it = m_idx.find(szWord);
	if (it == m_idx.end())
		return false;
	if (!m_dat)
	{
		m_dat = fopen(m_datPath.c_str(), "rb");
		if (!m_dat)
			return false;
	}
	char line[4096];
	for (const long off : it->second)
	{
		if (fseek(m_dat, off, SEEK_SET) != 0)
			continue;
		if (!fgets(line, sizeof(line), m_dat))
			continue;
		/* "word|n" header */
		char * bar = strrchr(line, '|');
		if (!bar)
			continue;
		const int n = atoi(bar + 1);
		for (int i = 0; i < n; ++i)
		{
			if (!fgets(line, sizeof(line), m_dat))
				break;
			size_t len = strlen(line);
			while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
				line[--len] = 0;
			Meaning m;
			s_parseMeaningLine(line, m);
			out.push_back(std::move(m));
		}
	}
	return !out.empty();
}
