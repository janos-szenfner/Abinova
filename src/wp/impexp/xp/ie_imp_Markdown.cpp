/* AbiWord
 * Copyright (C) 2025 AbiWord contributors
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
 *
 * Built-in Markdown importer. Syntax follows CommonMark plus the
 * extensions documented in the Zettlr Markdown Compendium
 * (https://docs.zettlr.com/en/editor/markdown-compendium.html):
 * ATX/setext headings, emphasis, strong, strikethrough, inline and
 * fenced/indented code, blockquotes, ordered/unordered/task lists,
 * links, images, autolinks, pipe tables and horizontal rules.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

#include <gsf/gsf-input.h>

#include "ie_imp_Markdown.h"
#include "ie_impGraphic.h"
#include "ie_types.h"
#include "fg_Graphic.h"
#include "pd_Document.h"
#include "pt_Types.h"
#include "ut_bytebuf.h"
#include "ut_debugmsg.h"
#include "ut_go_file.h"
#include "ut_mermaid.h"
#include "ut_string.h"
#include "ut_std_string.h"
#include "ut_string_class.h"
#include "ut_types.h"
#include "ut_units.h"
#include "ut_wctomb.h"
#include "ut_locale.h"

/*****************************************************************/
/*****************************************************************/

IE_Imp_Markdown::IE_Imp_Markdown(PD_Document * pDocument)
	: IE_Imp(pDocument),
	  m_nextListID(0),
	  m_nextImage(0),
	  m_nextFootnote(0)
{
}

IE_Imp_Markdown::~IE_Imp_Markdown()
{
}

/*****************************************************************/
/* helpers                                                       */
/*****************************************************************/

static std::string s_trim(const std::string & s)
{
	size_t b = s.find_first_not_of(" \t");
	if (b == std::string::npos)
		return "";
	size_t e = s.find_last_not_of(" \t");
	return s.substr(b, e - b + 1);
}

static bool s_isBlank(const std::string & s)
{
	return s.find_first_not_of(" \t") == std::string::npos;
}

static size_t s_indentWidth(const std::string & s)
{
	size_t w = 0;
	for (char c : s)
	{
		if (c == ' ') w++;
		else if (c == '\t') w += 4;
		else break;
	}
	return w;
}

static bool s_isPunct(char c)
{
	return ispunct(static_cast<unsigned char>(c)) != 0;
}

/*! Split a string on '\n', tolerating \r\n and \r. */
static std::vector<std::string> s_splitLines(const std::string & s)
{
	std::vector<std::string> out;
	std::string cur;
	for (char c : s)
	{
		if (c == '\r')
			continue;
		if (c == '\n')
		{
			out.push_back(cur);
			cur.clear();
		}
		else
			cur += c;
	}
	out.push_back(cur);
	return out;
}

/*! ATX heading: 0-3 spaces, 1-6 '#', space or EOL. Returns level or 0. */
static int s_atxLevel(const std::string & line, std::string & text)
{
	size_t i = 0;
	while (i < line.size() && line[i] == ' ' && i < 4) i++;
	if (i >= 4) return 0;
	size_t h = 0;
	while (i + h < line.size() && line[i + h] == '#') h++;
	if (h < 1 || h > 6) return 0;
	size_t j = i + h;
	if (j < line.size() && line[j] != ' ' && line[j] != '\t')
		return 0;
	std::string t = s_trim(line.substr(j));
	// strip closing #'s
	size_t e = t.find_last_not_of(' ');
	if (e != std::string::npos)
	{
		size_t k = e;
		while (k > 0 && t[k] == '#') k--;
		if (k != e && (k == 0 || t[k - 1] == ' ' || t[k] == '#'))
		{
			if (k > 0 && t[k] == '#')
				t = s_trim(t.substr(0, k));
		}
	}
	text = t;
	return static_cast<int>(h);
}

/*! Horizontal rule: 3+ of '-', '*' or '_' (spaces allowed). */
static bool s_isHR(const std::string & line)
{
	std::string t = s_trim(line);
	if (t.size() < 3) return false;
	char c = t[0];
	if (c != '-' && c != '*' && c != '_') return false;
	for (char x : t)
		if (x != c && x != ' ') return false;
	return true;
}

/*! Setext underline: '===' -> h1, '---' -> h2. */
static int s_setextLevel(const std::string & line)
{
	std::string t = s_trim(line);
	if (t.empty()) return 0;
	bool allEq = true, allDash = true;
	for (char c : t)
	{
		if (c != '=') allEq = false;
		if (c != '-') allDash = false;
	}
	if (allEq) return 1;
	if (allDash) return 2;
	return 0;
}

/*! Fence start: ``` or ~~~ with up to 3 spaces indent. */
static bool s_isFence(const std::string & line, char & fenceChar)
{
	size_t i = 0;
	while (i < line.size() && line[i] == ' ' && i < 4) i++;
	if (i >= 4 || i + 3 > line.size()) return false;
	char c = line[i];
	if (c != '`' && c != '~') return false;
	size_t n = 0;
	while (i + n < line.size() && line[i + n] == c) n++;
	if (n < 3) return false;
	// ``` fences may not have a backtick in the info string
	if (c == '`' && line.find('`', i + n) != std::string::npos)
		return false;
	fenceChar = c;
	return true;
}

/*! Blockquote line: '>' after <=3 spaces. Returns depth and content. */
static int s_quoteDepth(const std::string & line, std::string & content)
{
	size_t i = 0;
	while (i < line.size() && line[i] == ' ' && i < 4) i++;
	if (i >= line.size() || line[i] != '>') return 0;
	int depth = 1;
	i++;
	// continue consuming nested '>'
	for (;;)
	{
		while (i < line.size() && line[i] == ' ') i++;
		if (i < line.size() && line[i] == '>')
		{
			depth++;
			i++;
			continue;
		}
		break;
	}
	while (i < line.size() && line[i] == ' ') i++;
	content = line.substr(i);
	return depth;
}

/*! List item marker. Returns indent level chars consumed, sets
 *  bOrdered, number and content. Returns 0 when not a list item. */
static bool s_listMarker(const std::string & line, size_t & contentPos,
						 bool & bOrdered, int & number)
{
	size_t i = 0;
	while (i < line.size() && line[i] == ' ') i++;
	if (i >= line.size()) return false;

	char c = line[i];
	if (c == '-' || c == '*' || c == '+')
	{
		if (i + 1 >= line.size() || (line[i + 1] != ' ' && line[i + 1] != '\t'))
			return false;
		// avoid "---" hr and "- - -" patterns
		if (s_isHR(line)) return false;
		bOrdered = false;
		number = 1;
		contentPos = i + 1;
		while (contentPos < line.size() &&
			   (line[contentPos] == ' ' || line[contentPos] == '\t'))
			contentPos++;
		return true;
	}

	// ordered: 1-9 digits then '.' or ')'
	size_t j = i;
	while (j < line.size() && isdigit(static_cast<unsigned char>(line[j])))
		j++;
	if (j == i || j - i > 9 || j >= line.size())
		return false;
	if (line[j] != '.' && line[j] != ')')
		return false;
	if (j + 1 >= line.size() || (line[j + 1] != ' ' && line[j + 1] != '\t'))
		return false;
	bOrdered = true;
	number = atoi(line.substr(i, j - i).c_str());
	contentPos = j + 1;
	while (contentPos < line.size() &&
		   (line[contentPos] == ' ' || line[contentPos] == '\t'))
		contentPos++;
	return true;
}

/*! Pipe-table separator row: | --- | :-: | ---: | */
static bool s_isTableSep(const std::string & line, std::vector<int> & aligns)
{
	std::string t = s_trim(line);
	if (t.empty() || t.find('-') == std::string::npos) return false;
	if (t[0] == '|') t = t.substr(1);
	if (!t.empty() && t.back() == '|') t.pop_back();

	aligns.clear();
	size_t start = 0;
	for (;;)
	{
		size_t p = t.find('|', start);
		std::string cell = s_trim(t.substr(start, p == std::string::npos ? p : p - start));
		if (cell.empty()) return false;
		bool l = cell[0] == ':';
		bool r = cell.back() == ':';
		size_t a = l ? 1 : 0, b = cell.size() - (r ? 1 : 0);
		if (a >= b) return false;
		for (size_t k = a; k < b; k++)
			if (cell[k] != '-') return false;
		aligns.push_back(l && r ? 2 : r ? 1 : 0);  // 0 left, 1 right, 2 center
		if (p == std::string::npos) break;
		start = p + 1;
	}
	return !aligns.empty();
}

static std::vector<std::string> s_splitTableRow(const std::string & line)
{
	std::string t = s_trim(line);
	if (!t.empty() && t[0] == '|') t = t.substr(1);
	if (!t.empty() && t.back() == '|') t.pop_back();
	std::vector<std::string> out;
	std::string cur;
	for (size_t i = 0; i < t.size(); i++)
	{
		if (t[i] == '\\' && i + 1 < t.size() && t[i + 1] == '|')
		{
			cur += '|';
			i++;
			continue;
		}
		if (t[i] == '|')
		{
			out.push_back(s_trim(cur));
			cur.clear();
		}
		else
			cur += t[i];
	}
	out.push_back(s_trim(cur));
	return out;
}

/* defined with the inline helpers below */
static size_t s_findClose(const std::string & s, const std::string & delim,
						  size_t from);

/*! Normalize a link-reference id: case-insensitive, whitespace collapsed */
static std::string s_normRef(const std::string & s)
{
	std::string out;
	bool pendingSpace = false;
	for (char c : s_trim(s))
	{
		if (c == ' ' || c == '\t')
		{
			pendingSpace = true;
			continue;
		}
		if (pendingSpace && !out.empty())
			out += ' ';
		pendingSpace = false;
		out += static_cast<char>(tolower(static_cast<unsigned char>(c)));
	}
	return out;
}

/*! Link-reference definition: "[id]: url \"title\"" (up to 3 spaces
 *  indent, <url> allowed). Footnote defs "[^id]: ..." are a different
 *  syntax handled by s_footnoteDef. */
static bool s_linkRefDef(const std::string & line, std::string & id,
						 std::string & url, std::string & title)
{
	size_t i = 0;
	while (i < line.size() && line[i] == ' ' && i < 4) i++;
	if (i >= 4 || i >= line.size() || line[i] != '[') return false;
	size_t close = line.find(']', i + 1);
	if (close == std::string::npos || close == i + 1) return false;
	id = line.substr(i + 1, close - i - 1);
	if (!id.empty() && id[0] == '^') return false; // footnote def
	size_t j = close + 1;
	if (j >= line.size() || line[j] != ':') return false;
	j++;
	while (j < line.size() && (line[j] == ' ' || line[j] == '\t')) j++;
	if (j >= line.size()) return false;
	// url, possibly <url>
	size_t uend;
	if (line[j] == '<')
	{
		uend = line.find('>', j + 1);
		if (uend == std::string::npos) return false;
		url = line.substr(j + 1, uend - j - 1);
		uend++;
	}
	else
	{
		uend = j;
		while (uend < line.size() && line[uend] != ' ' &&
			   line[uend] != '\t') uend++;
		url = line.substr(j, uend - j);
	}
	// optional title: "...", '...', or (...)
	std::string rest = s_trim(line.substr(uend));
	if (!rest.empty())
	{
		char o = rest[0];
		char cl = o == '(' ? ')' : o;
		if ((o == '"' || o == '\'' || o == '(') &&
			rest.size() >= 2 && rest.back() == cl)
			title = rest.substr(1, rest.size() - 2);
		else
			return false; // trailing junk -> not a definition
	}
	return !url.empty();
}

/*! Footnote definition: "[^id]: text" */
static bool s_footnoteDef(const std::string & line, std::string & id,
						  std::string & text)
{
	size_t i = 0;
	while (i < line.size() && line[i] == ' ' && i < 4) i++;
	if (i >= 4 || i + 3 > line.size()) return false;
	if (line[i] != '[' || line[i + 1] != '^') return false;
	size_t close = line.find(']', i + 2);
	if (close == std::string::npos || close == i + 2) return false;
	id = line.substr(i + 2, close - i - 2);
	size_t j = close + 1;
	if (j >= line.size() || line[j] != ':') return false;
	text = s_trim(line.substr(j + 1));
	return true;
}

/*! Reference link/image: "[text][id]" or "[text][]".
 *  pos points at '['. Returns bytes consumed, or 0. */
static size_t s_parseRefLink(const std::string & s, size_t pos,
							 std::string & label, std::string & id)
{
	size_t i = pos;
	if (s[i] == '!') i++;
	if (i >= s.size() || s[i] != '[') return 0;
	size_t close = s_findClose(s, "]", i + 1);
	if (close == std::string::npos) return 0;
	size_t j = close + 1;
	if (j >= s.size() || s[j] != '[') return 0;
	size_t close2 = s.find(']', j + 1);
	if (close2 == std::string::npos) return 0;
	label = s.substr(i + 1, close - i - 1);
	id = s.substr(j + 1, close2 - j - 1);
	if (id.empty()) id = label;	// [text][] -> id is the text
	return close2 + 1 - pos;
}

/*! Inline math: $...$ (not $$, content non-empty, no space inside the
 *  delimiters). Returns bytes consumed, or 0. */
static size_t s_parseMathInline(const std::string & s, size_t pos,
								std::string & tex)
{
	if (s[pos] != '$') return 0;
	if (pos + 1 >= s.size() || s[pos + 1] == '$' || s[pos + 1] == ' ')
		return 0;
	for (size_t i = pos + 1; i < s.size(); i++)
	{
		if (s[i] == '\\') { i++; continue; }
		if (s[i] == '$')
		{
			if (i == pos + 1 || s[i - 1] == ' ') return 0;
			tex = s.substr(pos + 1, i - pos - 1);
			return i + 1 - pos;
		}
	}
	return 0;
}

/*! Strip HTML tags from a string, keeping inner text. */
static std::string s_stripHtmlTags(const std::string & s)
{
	std::string out;
	bool inTag = false;
	for (size_t i = 0; i < s.size(); i++)
	{
		char c = s[i];
		if (inTag)
		{
			if (c == '>') inTag = false;
			continue;
		}
		if (c == '<' && i + 1 < s.size() &&
			(isalpha(static_cast<unsigned char>(s[i + 1])) ||
			 s[i + 1] == '/' || s[i + 1] == '!'))
		{
			inTag = true;
			continue;
		}
		out += c;
	}
	return out;
}

/*! Line starts an HTML block (a tag at line start, not autolink). */
static bool s_isHtmlBlockStart(const std::string & line)
{
	std::string t = s_trim(line);
	if (t.size() < 2 || t[0] != '<') return false;
	return isalpha(static_cast<unsigned char>(t[1])) != 0 ||
		t[1] == '!' || t[1] == '/' || t[1] == '?';
}

/*! Common emoji shortcodes -> UTF-8. */
static const char * s_emoji(const std::string & name)
{
	static const std::map<std::string, const char *> s_map = {
		{ "smile", "\xF0\x9F\x98\x84" }, { "smiley", "\xF0\x9F\x98\x83" },
		{ "grin", "\xF0\x9F\x98\x81" }, { "joy", "\xF0\x9F\x98\x82" },
		{ "wink", "\xF0\x9F\x98\x89" }, { "heart", "\xE2\x9D\xA4\xEF\xB8\x8F" },
		{ "thumbsup", "\xF0\x9F\x91\x8D" }, { "+1", "\xF0\x9F\x91\x8D" },
		{ "thumbsdown", "\xF0\x9F\x91\x8E" }, { "-1", "\xF0\x9F\x91\x8E" },
		{ "clap", "\xF0\x9F\x91\x8F" }, { "wave", "\xF0\x9F\x91\x8B" },
		{ "eyes", "\xF0\x9F\x91\x80" }, { "thinking", "\xF0\x9F\xA4\x94" },
		{ "rocket", "\xF0\x9F\x9A\x80" }, { "tada", "\xF0\x9F\x8E\x89" },
		{ "fire", "\xF0\x9F\x94\xA5" }, { "star", "\xE2\xAD\x90" },
		{ "sparkles", "\xE2\x9C\xA8" }, { "bulb", "\xF0\x9F\x92\xA1" },
		{ "book", "\xF0\x9F\x93\x96" }, { "memo", "\xF0\x9F\x93\x9D" },
		{ "pencil", "\xE2\x9C\x8F\xEF\xB8\x8F" }, { "computer", "\xF0\x9F\x92\xBB" },
		{ "bug", "\xF0\x9F\x90\x9B" }, { "lock", "\xF0\x9F\x94\x92" },
		{ "key", "\xF0\x9F\x94\x91" }, { "calendar", "\xF0\x9F\x93\x85" },
		{ "coffee", "\xE2\x98\x95" }, { "pizza", "\xF0\x9F\x8D\x95" },
		{ "beer", "\xF0\x9F\x8D\xBA" }, { "check", "\xE2\x9C\x85" },
		{ "x", "\xE2\x9D\x8C" }, { "warning", "\xE2\x9A\xA0\xEF\xB8\x8F" },
		{ "question", "\xE2\x9D\x93" }, { "sun", "\xE2\x98\x80\xEF\xB8\x8F" },
		{ "moon", "\xF0\x9F\x8C\x99" }, { "cloud", "\xE2\x98\x81\xEF\xB8\x8F" },
		{ "snowflake", "\xE2\x9D\x84\xEF\xB8\x8F" }, { "rainbow", "\xF0\x9F\x8C\x88" },
	};
	auto it = s_map.find(name);
	return it == s_map.end() ? nullptr : it->second;
}

/*! Parse :name: emoji shortcode. Returns bytes consumed, or 0. */
static size_t s_parseEmoji(const std::string & s, size_t pos,
						   const char *& utf8)
{
	if (s[pos] != ':') return 0;
	size_t i = pos + 1;
	while (i < s.size() && (isalnum(static_cast<unsigned char>(s[i])) ||
							s[i] == '_' || s[i] == '+' || s[i] == '-'))
		i++;
	if (i == pos + 1 || i - pos - 1 > 32 || i >= s.size() || s[i] != ':')
		return 0;
	utf8 = s_emoji(s.substr(pos + 1, i - pos - 1));
	return utf8 ? i + 1 - pos : 0;
}

/*! "$$" display-math fence line (exactly $$, optional spaces). */
static bool s_isMathFence(const std::string & line)
{
	std::string t = s_trim(line);
	return t == "$$";
}

/*****************************************************************/
/* file loading                                                  */
/*****************************************************************/

UT_Error IE_Imp_Markdown::_loadFile(GsfInput * input)
{
	gsf_off_t size = gsf_input_size(input);
	if (size <= 0)
		return UT_OK;

	std::string utf8;
	utf8.resize(static_cast<size_t>(size));
	if (!gsf_input_read(input, static_cast<size_t>(size),
						reinterpret_cast<guint8 *>(&utf8[0])))
		return UT_IE_FILENOTFOUND;

	// strip UTF-8 BOM
	if (utf8.size() >= 3 &&
		static_cast<unsigned char>(utf8[0]) == 0xEF &&
		static_cast<unsigned char>(utf8[1]) == 0xBB &&
		static_cast<unsigned char>(utf8[2]) == 0xBF)
		utf8 = utf8.substr(3);

	const char * name = gsf_input_name(input);
	m_fileName = name ? name : "";

	if (!appendStrux(PTX_Section, PP_NOPROPS))
		return UT_IE_NOMEMORY;

	_parseDocument(utf8);
	return UT_OK;
}

/*****************************************************************/
/* block level parsing                                           */
/*****************************************************************/

void IE_Imp_Markdown::_parseDocument(const std::string & utf8)
{
	std::vector<std::string> lines = s_splitLines(utf8);
	size_t n = lines.size();
	size_t i = 0;

	/* YAML frontmatter: a leading --- block carries document metadata,
	 * not body text. Map the common keys to document properties and
	 * skip the block entirely. */
	if (n > 1 && s_trim(lines[0]) == "---")
	{
		size_t end = 1;
		while (end < n && s_trim(lines[end]) != "---" &&
			   s_trim(lines[end]) != "...")
			end++;
		if (end < n)	// found the closing marker
		{
			for (size_t k = 1; k < end; k++)
			{
				const std::string & l = lines[k];
				size_t colon = l.find(':');
				if (colon == std::string::npos) continue;
				std::string key = s_normRef(l.substr(0, colon));
				std::string val = s_trim(l.substr(colon + 1));
				if (val.size() >= 2 &&
					(val.front() == '"' || val.front() == '\'') &&
					val.back() == val.front())
					val = val.substr(1, val.size() - 2);
				if (key == "title")
					getDoc()->setMetaDataProp(PD_META_KEY_TITLE, val);
				else if (key == "author")
					getDoc()->setMetaDataProp(PD_META_KEY_CREATOR, val);
				else if (key == "date")
					getDoc()->setMetaDataProp(PD_META_KEY_DATE, val);
				else if (key == "tags" || key == "keywords")
				{
					if (val.size() >= 2 && val.front() == '[' &&
						val.back() == ']')
						val = val.substr(1, val.size() - 2);
					getDoc()->setMetaDataProp(PD_META_KEY_KEYWORDS, val);
				}
				else if (key == "subject")
					getDoc()->setMetaDataProp(PD_META_KEY_SUBJECT, val);
			}
			i = end + 1;
		}
	}

	/* pass 0: collect link-reference and footnote definitions, skipping
	 * fenced code (their contents are literal). Definition lines are
	 * remembered so the main pass can skip them. */
	bool inFence = false;
	char fenceCh = 0;
	for (size_t k = i; k < n; k++)
	{
		char fc = 0;
		if (s_isFence(lines[k], fc))
		{
			if (!inFence) { inFence = true; fenceCh = fc; }
			else if (fc == fenceCh) inFence = false;
			continue;
		}
		if (inFence) continue;

		std::string id, url, title;
		if (s_linkRefDef(lines[k], id, url, title))
		{
			m_linkRefs[s_normRef(id)] = { url, title };
			m_defLines.insert(k);
			continue;
		}
		if (s_footnoteDef(lines[k], id, url))
		{
			m_footnoteDefs[s_normRef(id)] = url;
			m_defLines.insert(k);
		}
	}

	while (i < n)
	{
		const std::string & line = lines[i];

		// consumed link-reference / footnote definition line
		if (m_defLines.count(i))
		{
			i++;
			continue;
		}

		if (s_isBlank(line))
		{
			_resetLists();
			i++;
			continue;
		}

		// fenced code block
		char fence = 0;
		if (s_isFence(line, fence))
		{
			/* info string after the fence run selects the language */
			size_t fi = line.find(fence);
			while (fi < line.size() && line[fi] == fence) fi++;
			std::string lang = s_trim(line.substr(fi));
			if (!lang.empty())
			{
				size_t sp = lang.find_first_of(" \t");
				if (sp != std::string::npos)
					lang = lang.substr(0, sp);
			}

			std::vector<std::string> code;
			i++;
			while (i < n)
			{
				char c2 = 0;
				if (s_isFence(lines[i], c2) && c2 == fence)
				{
					i++;
					break;
				}
				code.push_back(lines[i]);
				i++;
			}
			if (lang == "mermaid")
				_emitMermaid(code);
			else
				_emitCodeBlock(code);
			continue;
		}

		// HTML comment block: <!-- ... --> possibly spanning lines;
		// comments don't appear in the rendered output
		if (s_trim(line).find("<!--") == 0)
		{
			while (i < n)
			{
				if (lines[i].find("-->") != std::string::npos)
				{
					// keep any text after --> on the same line
					std::string rest = s_trim(
						lines[i].substr(lines[i].find("-->") + 3));
					i++;
					if (!rest.empty())
					{
						// rare: trailing content - emit as paragraph
						_emitParagraph("Normal", "", s_stripHtmlTags(rest));
					}
					break;
				}
				i++;
			}
			continue;
		}

		// display math: $$...$$ inline or $$ ... $$ across lines.
		// Without a MathML engine the LaTeX source is emitted as a
		// centred italic paragraph.
		if (s_isMathFence(line))
		{
			_resetLists();
			std::string tex;
			i++;
			while (i < n && !s_isMathFence(lines[i]))
			{
				if (!tex.empty()) tex += ' ';
				tex += s_trim(lines[i]);
				i++;
			}
			if (i < n) i++;	// closing $$
			_emitMathBlock(tex);
			continue;
		}
		{
			std::string t = s_trim(line);
			if (t.size() > 4 && t.find("$$") == 0)
			{
				size_t cl = t.rfind("$$");
				if (cl >= 2)
				{
					_resetLists();
					_emitMathBlock(s_trim(t.substr(2, cl - 2)));
					i++;
					continue;
				}
			}
		}

		// raw HTML block: gather the block, strip tags, keep the text
		if (s_isHtmlBlockStart(line))
		{
			std::string text;
			while (i < n && !s_isBlank(lines[i]))
			{
				if (!text.empty()) text += ' ';
				text += s_stripHtmlTags(lines[i]);
				i++;
			}
			text = s_trim(text);
			if (!text.empty())
			{
				_resetLists();
				_emitParagraph("Normal", "", text);
			}
			continue;
		}

		// pipe table: header row + separator row
		if (i + 1 < n && line.find('|') != std::string::npos)
		{
			std::vector<int> aligns;
			if (s_isTableSep(lines[i + 1], aligns))
			{
				std::vector<std::vector<std::string> > rows;
				rows.push_back(s_splitTableRow(line));
				i += 2;
				while (i < n && lines[i].find('|') != std::string::npos &&
					   !s_isBlank(lines[i]))
				{
					rows.push_back(s_splitTableRow(lines[i]));
					i++;
				}
				_emitTable(rows, aligns);
				continue;
			}
		}

		// ATX heading
		std::string htext;
		int hlevel = s_atxLevel(line, htext);
		if (hlevel)
		{
			_resetLists();
			_emitHeading(hlevel, htext);
			i++;
			continue;
		}

		// horizontal rule
		if (s_isHR(line))
		{
			_resetLists();
			_emitHR();
			i++;
			continue;
		}

		// blockquote
		std::string qtext;
		int qdepth = s_quoteDepth(line, qtext);
		if (qdepth)
		{
			_resetLists();
			while (i < n)
			{
				std::string qc;
				int d = s_quoteDepth(lines[i], qc);
				if (!d) break;
				// blank line inside quote ends it when the next
				// line is not a quote (simplified single-level quotes)
				_emitBlockQuote(d, qc);
				i++;
			}
			continue;
		}

		// list item
		size_t contentPos = 0;
		bool bOrdered = false;
		int number = 1;
		if (s_listMarker(line, contentPos, bOrdered, number))
		{
			size_t indent = s_indentWidth(line);
			int level = static_cast<int>(indent / 2) + 1;
			_emitListItem(level, bOrdered, number, line.substr(contentPos));
			i++;
			continue;
		}

		// indented code block (4+ spaces / tab)
		if (s_indentWidth(line) >= 4)
		{
			_resetLists();
			std::vector<std::string> code;
			while (i < n && (s_indentWidth(lines[i]) >= 4 || s_isBlank(lines[i])))
			{
				if (s_isBlank(lines[i]))
					code.push_back("");
				else
					code.push_back(lines[i].substr(4));
				i++;
			}
			_emitCodeBlock(code);
			continue;
		}

		// setext heading: text line followed by === or ---
		if (i + 1 < n)
		{
			int sl = s_setextLevel(lines[i + 1]);
			if (sl)
			{
				_resetLists();
				_emitHeading(sl, s_trim(line));
				i += 2;
				continue;
			}
		}

		// plain paragraph: merge consecutive non-special lines
		{
			_resetLists();
			std::string text = s_trim(line);
			i++;
			bool bEmitted = false;
			while (i < n && !s_isBlank(lines[i]) && !m_defLines.count(i))
			{
				// a setext underline converts the accumulated text
				// into a heading
				int sl = s_setextLevel(lines[i]);
				if (sl)
				{
					_emitHeading(sl, text);
					i++;
					bEmitted = true;
					break;
				}
				// stop if the next line starts a special block
				char f2 = 0;
				std::string tmp;
				size_t cp = 0;
				bool bo = false;
				int num = 1;
				if (s_atxLevel(lines[i], tmp) || s_isHR(lines[i]) ||
					s_isFence(lines[i], f2) || s_quoteDepth(lines[i], tmp) ||
					s_listMarker(lines[i], cp, bo, num) ||
					s_isMathFence(lines[i]) ||
					s_trim(lines[i]).find("<!--") == 0 ||
					s_isHtmlBlockStart(lines[i]))
				{
					break;
				}
				// hard line break: two trailing spaces or backslash
				// (check the raw line, text is already trimmed)
				const std::string & raw = lines[i - 1];
				bool hard = (raw.size() >= 2 &&
							 raw.compare(raw.size() - 2, 2, "  ") == 0) ||
							(!raw.empty() && raw.back() == '\\');
				if (hard && !text.empty() && text.back() == '\\')
					text.pop_back();
				if (hard)
					text += '\n';	// marker replaced by UCS_LF later
				else
					text += ' ';
				text += s_trim(lines[i]);
				i++;
			}
			if (!bEmitted)
				_emitParagraph("Normal", "", text);
		}
	}
}

/*****************************************************************/
/* emitters                                                      */
/*****************************************************************/

bool IE_Imp_Markdown::_emitParagraph(const char * szStyle,
									 const std::string & extraProps,
									 const std::string & text)
{
	PP_PropertyVector atts = {
		"style", szStyle ? szStyle : "Normal"
	};
	if (!extraProps.empty())
	{
		atts.push_back("props");
		atts.push_back(extraProps);
	}
	if (!appendStrux(PTX_Block, atts))
		return false;
	_emitInline(text);
	return true;
}

bool IE_Imp_Markdown::_emitHeading(int level, const std::string & text)
{
	std::string style = "Heading ";
	style += static_cast<char>('0' + (level > 4 ? 4 : level));
	return _emitParagraph(style.c_str(), "", text);
}

bool IE_Imp_Markdown::_emitListItem(int level, bool bOrdered, int startValue,
									const std::string & text)
{
	if (level < 1) level = 1;

	// grow/shrink the active list stack to the needed level
	while (m_listIds.size() < static_cast<size_t>(level))
	{
		UT_uint32 id = ++m_nextListID;
		UT_uint32 parent = m_listIds.empty() ? 0 : m_listIds.back();

		std::string sid = UT_std_string_sprintf("%u", id);
		std::string sparent = UT_std_string_sprintf("%u", parent);

		const PP_PropertyVector latts = {
			"id", sid,
			"parentid", sparent,
			"type", "5",
			"start-value", "0",
			"list-delim", "%L",
			"list-decimal", "NULL"
		};
		if (!getDoc()->appendList(latts))
			return false;
		m_listIds.push_back(id);
		m_listOrdered.push_back(bOrdered);
		m_listStart.push_back(startValue);
	}
	while (m_listIds.size() > static_cast<size_t>(level))
	{
		m_listIds.pop_back();
		m_listOrdered.pop_back();
		m_listStart.pop_back();
	}
	if (m_listOrdered[level - 1] != bOrdered)
	{
		// different list kind at same level: new id
		UT_uint32 id = ++m_nextListID;
		UT_uint32 parent = level > 1 ? m_listIds[level - 2] : 0;

		std::string sid = UT_std_string_sprintf("%u", id);
		std::string sparent = UT_std_string_sprintf("%u", parent);
		const PP_PropertyVector latts = {
			"id", sid,
			"parentid", sparent,
			"type", "5",
			"start-value", "0",
			"list-delim", "%L",
			"list-decimal", "NULL"
		};
		if (!getDoc()->appendList(latts))
			return false;
		m_listIds[level - 1] = id;
		m_listOrdered[level - 1] = bOrdered;
		m_listStart[level - 1] = startValue;
	}

	std::string sid = UT_std_string_sprintf("%u", m_listIds[level - 1]);
	std::string sparent = UT_std_string_sprintf("%u",
						level > 1 ? m_listIds[level - 2] : 0);
	std::string slevel = UT_std_string_sprintf("%d", level);

	std::string props;
	{
		UT_LocaleTransactor t(LC_NUMERIC, "C");
		props = UT_std_string_sprintf(
			"list-style:%s; start-value:%d; text-indent:-0.3in; field-font:NULL; margin-left:%.2fin",
			bOrdered ? "Numbered List" : "Bullet List",
			m_listStart[level - 1],
			level * 0.5);
	}

	const PP_PropertyVector atts = {
		"level", slevel,
		"listid", sid,
		"parentid", sparent,
		"props", props,
		"style", "Normal"
	};
	if (!appendStrux(PTX_Block, atts))
		return false;

	const PP_PropertyVector fatts = {
		"type", "list_label"
	};
	if (!appendObject(PTO_Field, fatts))
		return false;
	appendFmt(PP_NOPROPS);

	UT_UCS4Char tab = UCS_TAB;
	if (!appendSpan(&tab, 1))
		return false;

	// task list item: "- [ ]" / "- [x]"
	std::string body = text;
	if (body.size() >= 3 && body[0] == '[' &&
		(body[1] == ' ' || body[1] == 'x' || body[1] == 'X') &&
		body[2] == ']')
	{
		const char * box = (body[1] == ' ') ? "\xE2\x98\x90 " : "\xE2\x98\x91 ";
		appendSpan(box);
		body = body.substr(3);
		while (!body.empty() && body[0] == ' ') body = body.substr(1);
	}
	_emitInline(body);
	return true;
}

bool IE_Imp_Markdown::_emitCodeBlock(const std::vector<std::string> & lines)
{
	for (const std::string & l : lines)
	{
		PP_PropertyVector atts = {
			"style", "Plain Text"
		};
		if (!appendStrux(PTX_Block, atts))
			return false;
		if (!l.empty())
		{
			const PP_PropertyVector fatts = {
				PT_PROPS_ATTRIBUTE_NAME, "font-family:Courier New"
			};
			appendFmt(fatts);
			appendSpan(l);
		}
	}
	return true;
}

/*! ```mermaid fenced block: render the diagram to PNG with the
 *  built-in renderer and embed it as an image.  Falls back to a
 *  plain code block when the source is not a recognised diagram. */
bool IE_Imp_Markdown::_emitMermaid(const std::vector<std::string> & lines)
{
	std::string src;
	for (const std::string & l : lines)
	{
		src += l;
		src += '\n';
	}

	UT_ByteBufPtr pbb(new UT_ByteBuf);
	if (!UT_Mermaid::renderToPNG(src, *pbb))
		return _emitCodeBlock(lines);

	std::string dataid =
		UT_std_string_sprintf("md-mermaid%u", m_nextImage++);
	const PP_PropertyVector atts = {
		PT_PROPS_ATTRIBUTE_NAME, "",
		"dataid", dataid,
		"title", "Mermaid diagram",
		"alt", "Mermaid diagram"
	};
	if (!appendObject(PTO_Image, atts))
		return false;
	if (!getDoc()->createDataItem(dataid.c_str(), false, pbb,
								  "image/png", nullptr))
		return false;
	return true;
}

bool IE_Imp_Markdown::_emitBlockQuote(int depth, const std::string & text)
{
	std::string props;
	{
		UT_LocaleTransactor t(LC_NUMERIC, "C");
		props = UT_std_string_sprintf(
			"margin-left:%.2fin; margin-right:0.5in; margin-bottom:6pt",
			depth * 0.5 + 0.5);
	}
	return _emitParagraph("Block Text", props, text);
}

bool IE_Imp_Markdown::_emitHR(void)
{
	PP_PropertyVector atts = {
		"style", "Normal",
		"props", "bot-style:1; bot-thickness:0.01in; bot-color:000000; margin-top:6pt; margin-bottom:6pt"
	};
	if (!appendStrux(PTX_Block, atts))
		return false;
	appendSpan(" ");
	return true;
}

bool IE_Imp_Markdown::_emitTable(const std::vector<std::vector<std::string> > & rows,
								 const std::vector<int> & aligns)
{
	if (rows.empty()) return false;
	size_t cols = aligns.size();
	for (const auto & r : rows)
		if (r.size() > cols) cols = r.size();

	_resetLists();

	std::string tprops = "table-column-relwidths:";
	{
		UT_LocaleTransactor t(LC_NUMERIC, "C");
		for (size_t c = 0; c < cols; c++)
		{
			if (c) tprops += " ";
			tprops += UT_std_string_sprintf("%f", 1.0 / cols);
		}
	}

	PP_PropertyVector tatts = {
		"props", tprops
	};
	if (!appendStrux(PTX_SectionTable, tatts))
		return false;

	for (size_t r = 0; r < rows.size(); r++)
	{
		for (size_t c = 0; c < cols; c++)
		{
			std::string cellText = c < rows[r].size() ? rows[r][c] : "";
			std::string cprops = UT_std_string_sprintf(
				"top-attach:%u; bot-attach:%u; left-attach:%u; right-attach:%u",
				static_cast<unsigned>(r), static_cast<unsigned>(r + 1),
				static_cast<unsigned>(c), static_cast<unsigned>(c + 1));
			if (c < aligns.size() && aligns[c])
				cprops += aligns[c] == 1 ? "; text-align:right" : "; text-align:center";

			PP_PropertyVector catts = {
				"props", cprops
			};
			if (!appendStrux(PTX_SectionCell, catts))
				return false;

			PP_PropertyVector batts = {
				"style", "Normal"
			};
			if (!appendStrux(PTX_Block, batts))
				return false;
			if (!cellText.empty())
				_emitInline(cellText);

			if (!appendStrux(PTX_EndCell, PP_NOPROPS))
				return false;
		}
	}
	if (!appendStrux(PTX_EndTable, PP_NOPROPS))
		return false;
	return true;
}

bool IE_Imp_Markdown::_emitImage(const std::string & url, const std::string & alt,
								 const std::string & title)
{
	char * resolved = UT_go_url_resolve_relative(m_fileName.c_str(), url.c_str());
	if (!resolved)
		return false;

	FG_ConstGraphicPtr pfg;
	UT_Error err = IE_ImpGraphic::loadGraphic(resolved, IEGFT_Unknown, pfg);
	g_free(resolved);
	if (err != UT_OK || !pfg)
		return false;

	std::string dataid = UT_std_string_sprintf("md-image%u", m_nextImage++);

	const PP_PropertyVector atts = {
		PT_PROPS_ATTRIBUTE_NAME, "",
		"dataid", dataid,
		"title", title,
		"alt", alt
	};
	if (!appendObject(PTO_Image, atts))
		return false;
	if (!getDoc()->createDataItem(dataid.c_str(), false, pfg->getBuffer(),
								  pfg->getMimeType(), nullptr))
		return false;
	return true;
}

/*! Footnote: embedded strux at the marker position, containing the
 *  definition text collected by the definition pass. */
bool IE_Imp_Markdown::_emitFootnote(const std::string & text)
{
	std::string id = UT_std_string_sprintf("%u", ++m_nextFootnote);
	const PP_PropertyVector atts = {
		"footnote-id", id
	};
	if (!appendStrux(PTX_SectionFootnote, atts))
		return false;
	if (!appendStrux(PTX_Block, PP_NOPROPS))
		return false;
	_emitInline(text);
	if (!appendStrux(PTX_EndFootnote, PP_NOPROPS))
		return false;
	return true;
}

/*! Display math: no MathML engine is guaranteed, so emit the LaTeX
 *  source centred and italic - readable and clearly marked. */
bool IE_Imp_Markdown::_emitMathBlock(const std::string & tex)
{
	PP_PropertyVector atts = {
		"style", "Normal",
		"props", "text-align:center; font-style:italic"
	};
	if (!appendStrux(PTX_Block, atts))
		return false;
	if (!tex.empty())
		appendSpan(tex);
	return true;
}

bool IE_Imp_Markdown::lookupLinkRef(const std::string & id, std::string & url,
									std::string & title) const
{
	auto it = m_linkRefs.find(s_normRef(id));
	if (it == m_linkRefs.end())
		return false;
	url = it->second.url;
	title = it->second.title;
	return true;
}

bool IE_Imp_Markdown::lookupFootnoteDef(const std::string & id,
										std::string & text) const
{
	auto it = m_footnoteDefs.find(s_normRef(id));
	if (it == m_footnoteDefs.end())
		return false;
	text = it->second;
	return true;
}

void IE_Imp_Markdown::_resetLists(void)
{
	m_listIds.clear();
	m_listOrdered.clear();
	m_listStart.clear();
}

/*****************************************************************/
/* inline level parsing                                          */
/*****************************************************************/

namespace {

struct MDFmt {
	bool bold = false;
	bool italic = false;
	bool strike = false;
	bool code = false;
};

} // anonymous namespace

static std::string s_fmtProps(const MDFmt & f)
{
	// NOTE: a trailing ';' leaves an empty trailing property, which
	// PP_AttrProp::setAttribute() rejects -> the whole fmt mark fails.
	std::string p;
	auto add = [&p](const char * prop) {
		if (!p.empty()) p += "; ";
		p += prop;
	};
	if (f.bold) add("font-weight:bold");
	if (f.italic) add("font-style:italic");
	if (f.strike) add("text-decoration:line-through");
	if (f.code) add("font-family:Courier New");
	return p;
}

static void s_emitSegment(IE_Imp_Markdown * imp, PD_Document * doc,
						  const std::string & text, const MDFmt & f)
{
	(void)doc;
	if (text.empty()) return;

	std::string props = s_fmtProps(f);
	PP_PropertyVector atts;
	if (!props.empty())
	{
		atts.push_back(PT_PROPS_ATTRIBUTE_NAME);
		atts.push_back(props);
	}
	if (!imp->appendFmtPublic(atts))
		return;

	// emit text, honoring the '\n' hard-break marker as a forced break
	std::string cur = text;
	size_t pos = 0;
	while (pos <= cur.size())
	{
		size_t nl = cur.find('\n', pos);
		std::string piece = cur.substr(pos, nl == std::string::npos ? nl : nl - pos);
		if (!piece.empty())
		{
			UT_UCS4String u(piece.c_str());
			imp->appendSpanPublic(u.ucs4_str(), u.length());
		}
		if (nl == std::string::npos) break;
		UT_UCS4Char lf = UCS_LF;
		imp->appendSpanPublic(&lf, 1);
		pos = nl + 1;
	}
}

/*! Find the closing delimiter respecting escapes. Returns position of
 *  the first delimiter char, or npos. */
static size_t s_findClose(const std::string & s, const std::string & delim,
						  size_t from)
{
	for (size_t i = from; i + delim.size() <= s.size(); i++)
	{
		if (s[i] == '\\')
		{
			i++;
			continue;
		}
		if (s.compare(i, delim.size(), delim) == 0)
			return i;
	}
	return std::string::npos;
}

/*! Parse "[text](url \"title\")" or "![alt](url \"title\")".
 *  pos points at '['. Returns bytes consumed incl. leading '!', or 0. */
static size_t s_parseLink(const std::string & s, size_t pos,
						  std::string & label, std::string & url,
						  std::string & title)
{
	size_t i = pos;
	if (s[i] == '!') i++;
	if (i >= s.size() || s[i] != '[') return 0;
	size_t close = s_findClose(s, "]", i + 1);
	if (close == std::string::npos) return 0;
	label = s.substr(i + 1, close - i - 1);
	size_t j = close + 1;
	if (j >= s.size() || s[j] != '(') return 0;
	size_t pend = s_findClose(s, ")", j + 1);
	if (pend == std::string::npos) return 0;
	std::string inside = s_trim(s.substr(j + 1, pend - j - 1));
	// split url and optional "title"
	size_t sp = inside.find(' ');
	if (sp == std::string::npos) sp = inside.find('\t');
	if (sp != std::string::npos)
	{
		url = inside.substr(0, sp);
		std::string t = s_trim(inside.substr(sp));
		if (t.size() >= 2 && (t[0] == '"' || t[0] == '\'') && t.back() == t[0])
			title = t.substr(1, t.size() - 2);
	}
	else
		url = inside;
	return pend + 1 - pos;
}

/*! Autolink <scheme:...> or <email>. */
static size_t s_parseAutoLink(const std::string & s, size_t pos,
							  std::string & url)
{
	if (s[pos] != '<') return 0;
	size_t end = s.find('>', pos + 1);
	if (end == std::string::npos || end - pos > 200) return 0;
	std::string inside = s.substr(pos + 1, end - pos - 1);
	if (inside.find(' ') != std::string::npos) return 0;
	if (inside.find("://") == std::string::npos &&
		inside.find('@') == std::string::npos &&
		inside.find("mailto:") == std::string::npos)
		return 0;
	url = inside;
	return end + 1 - pos;
}

static void s_emitInlineRec(IE_Imp_Markdown * imp, PD_Document * doc,
							const std::string & s, const MDFmt & fmt)
{
	std::string run;
	auto flush = [&]() {
		s_emitSegment(imp, doc, run, fmt);
		run.clear();
	};

	size_t i = 0;
	while (i < s.size())
	{
		char c = s[i];

		// backslash escape
		if (c == '\\' && i + 1 < s.size() && s_isPunct(s[i + 1]))
		{
			run += s[i + 1];
			i += 2;
			continue;
		}

		// inline code (escapes do NOT apply inside code spans, so a
		// plain find is correct: `\` is literal text)
		if (c == '`')
		{
			size_t n = 1;
			while (i + n < s.size() && s[i + n] == '`') n++;
			std::string delim = s.substr(i, n);
			size_t close = s.find(delim, i + n);
			if (close != std::string::npos)
			{
				flush();
				MDFmt f = fmt;
				f.code = true;
				std::string code = s.substr(i + n, close - i - n);
				s_emitSegment(imp, doc, code, f);
				i = close + n;
				continue;
			}
			run += c;
			i++;
			continue;
		}

		// strong / emphasis
		if ((c == '*' || c == '_'))
		{
			size_t n = 1;
			while (i + n < s.size() && s[i + n] == c && n < 3) n++;
			if (n >= 2)
			{
				std::string delim = s.substr(i, n >= 3 ? 3 : 2);
				size_t close = s_findClose(s, delim, i + delim.size());
				if (close != std::string::npos)
				{
					flush();
					MDFmt f = fmt;
					f.bold = true;
					if (n >= 3) f.italic = true;
					s_emitInlineRec(imp, doc,
									s.substr(i + delim.size(), close - i - delim.size()), f);
					i = close + delim.size();
					continue;
				}
			}
			std::string delim(1, c);
			size_t close = s_findClose(s, delim, i + 1);
			if (close != std::string::npos && close > i + 1)
			{
				flush();
				MDFmt f = fmt;
				f.italic = true;
				s_emitInlineRec(imp, doc, s.substr(i + 1, close - i - 1), f);
				i = close + 1;
				continue;
			}
			run += c;
			i++;
			continue;
		}

		// strikethrough
		if (c == '~' && i + 1 < s.size() && s[i + 1] == '~')
		{
			size_t close = s_findClose(s, "~~", i + 2);
			if (close != std::string::npos)
			{
				flush();
				MDFmt f = fmt;
				f.strike = true;
				s_emitInlineRec(imp, doc, s.substr(i + 2, close - i - 2), f);
				i = close + 2;
				continue;
			}
			run += c;
			i++;
			continue;
		}

		// image: ![alt](url) or ![alt][ref]
		if (c == '!' && i + 1 < s.size() && s[i + 1] == '[')
		{
			std::string label, url, title;
			size_t len = s_parseLink(s, i, label, url, title);
			if (!len)
			{
				std::string id;
				len = s_parseRefLink(s, i, label, id);
				if (len && !imp->lookupLinkRef(id, url, title))
					len = 0;
			}
			if (len)
			{
				flush();
				if (!imp->emitImagePublic(url, label, title))
				{
					// fallback: alt text as a link to the target
					const PP_PropertyVector hatts = {
						"xlink:href", url
					};
					imp->appendObjectPublic(PTO_Hyperlink, hatts);
					s_emitSegment(imp, doc, label, fmt);
					imp->appendObjectPublic(PTO_Hyperlink, PP_NOPROPS);
				}
				i += len;
				continue;
			}
			run += c;
			i++;
			continue;
		}

		// footnote reference / link: [^id], [text](url), [text][ref],
		// or the [ref] shortcut form
		if (c == '[')
		{
			// footnote marker [^id]
			if (i + 2 < s.size() && s[i + 1] == '^')
			{
				size_t close = s.find(']', i + 2);
				if (close != std::string::npos)
				{
					std::string note;
					std::string id = s.substr(i + 2, close - i - 2);
					if (imp->lookupFootnoteDef(id, note))
					{
						flush();
						imp->emitFootnotePublic(note);
						i = close + 1;
						continue;
					}
				}
			}

			std::string label, url, title;
			size_t len = s_parseLink(s, i, label, url, title);
			if (!len)
			{
				// reference form: [text][id] / [text][]
				std::string id;
				len = s_parseRefLink(s, i, label, id);
				if (len && !imp->lookupLinkRef(id, url, title))
					len = 0;
			}
			if (!len)
			{
				// shortcut form: [id] where id is a known reference
				size_t close = s_findClose(s, "]", i + 1);
				if (close != std::string::npos)
				{
					std::string id = s.substr(i + 1, close - i - 1);
					if (imp->lookupLinkRef(id, url, title))
					{
						label = id;
						len = close + 1 - i;
					}
				}
			}
			if (len)
			{
				flush();
				const PP_PropertyVector hatts = {
					"xlink:href", url
				};
				imp->appendObjectPublic(PTO_Hyperlink, hatts);
				s_emitInlineRec(imp, doc, label, fmt);
				imp->appendObjectPublic(PTO_Hyperlink, PP_NOPROPS);
				i += len;
				continue;
			}
			run += c;
			i++;
			continue;
		}

		// inline HTML comment <!-- ... --> or autolink <...>
		if (c == '<')
		{
			if (s.compare(i, 4, "<!--") == 0)
			{
				size_t end = s.find("-->", i + 4);
				if (end != std::string::npos)
				{
					i = end + 3;
					continue;
				}
			}
			std::string url;
			size_t len = s_parseAutoLink(s, i, url);
			if (len)
			{
				flush();
				std::string href = url;
				if (url.find("://") == std::string::npos &&
					url.find("mailto:") == std::string::npos)
					href = "mailto:" + url;
				const PP_PropertyVector hatts = {
					"xlink:href", href
				};
				imp->appendObjectPublic(PTO_Hyperlink, hatts);
				s_emitSegment(imp, doc, url, fmt);
				imp->appendObjectPublic(PTO_Hyperlink, PP_NOPROPS);
				i += len;
				continue;
			}
			// bare HTML tag: drop the markup, keep the text
			if (i + 1 < s.size() &&
				(isalpha(static_cast<unsigned char>(s[i + 1])) ||
				 s[i + 1] == '/' || s[i + 1] == '!' || s[i + 1] == '?'))
			{
				size_t end = s.find('>', i + 1);
				if (end != std::string::npos)
				{
					i = end + 1;
					continue;
				}
			}
			run += c;
			i++;
			continue;
		}

		// inline math $...$ -> italic run of the LaTeX source
		if (c == '$')
		{
			std::string tex;
			size_t len = s_parseMathInline(s, i, tex);
			if (len)
			{
				flush();
				MDFmt f = fmt;
				f.italic = true;
				s_emitSegment(imp, doc, tex, f);
				i += len;
				continue;
			}
			run += c;
			i++;
			continue;
		}

		// emoji shortcode :name:
		if (c == ':')
		{
			const char * utf8 = nullptr;
			size_t len = s_parseEmoji(s, i, utf8);
			if (len)
			{
				run += utf8;
				i += len;
				continue;
			}
			run += c;
			i++;
			continue;
		}

		run += c;
		i++;
	}
	flush();
}

void IE_Imp_Markdown::_emitInline(const std::string & text)
{
	MDFmt fmt;
	s_emitInlineRec(this, getDoc(), text, fmt);
}

/*****************************************************************/
/* sniffer                                                       */
/*****************************************************************/

IE_Imp_Markdown_Sniffer::IE_Imp_Markdown_Sniffer()
	: IE_ImpSniffer(IE_IMPEXPNAME_MARKDOWN, true)
{
}

IE_Imp_Markdown_Sniffer::~IE_Imp_Markdown_Sniffer()
{
}

static IE_SuffixConfidence IE_Imp_Markdown_Sniffer__SuffixConfidence[] = {
	{ "md",			UT_CONFIDENCE_PERFECT	},
	{ "markdown",	UT_CONFIDENCE_PERFECT	},
	{ "mdown",		UT_CONFIDENCE_GOOD		},
	{ "mkd",		UT_CONFIDENCE_GOOD		},
	{ "mkdn",		UT_CONFIDENCE_GOOD		},
	{ "",			UT_CONFIDENCE_ZILCH		}
};

const IE_SuffixConfidence * IE_Imp_Markdown_Sniffer::getSuffixConfidence()
{
	return IE_Imp_Markdown_Sniffer__SuffixConfidence;
}

static IE_MimeConfidence IE_Imp_Markdown_Sniffer__MimeConfidence[] = {
	{ IE_MIME_MATCH_FULL,	"text/markdown",	UT_CONFIDENCE_PERFECT	},
	{ IE_MIME_MATCH_FULL,	"text/x-markdown",	UT_CONFIDENCE_GOOD		},
	{ IE_MIME_MATCH_BOGUS,	"",					UT_CONFIDENCE_ZILCH		}
};

const IE_MimeConfidence * IE_Imp_Markdown_Sniffer::getMimeConfidence()
{
	return IE_Imp_Markdown_Sniffer__MimeConfidence;
}

UT_Confidence_t IE_Imp_Markdown_Sniffer::recognizeContents(const char * szBuf,
														   UT_uint32 iNumbytes)
{
	// Markdown is plain text, so content detection must be careful:
	// score characteristic block/inline patterns and only claim the
	// file when the evidence is clear. Plain-text files keep winning
	// through the Text importer because its suffix confidence is high.
	if (!szBuf || iNumbytes < 8)
		return UT_CONFIDENCE_ZILCH;

	// refuse buffers that look binary
	for (UT_uint32 k = 0; k < iNumbytes && k < 512; k++)
	{
		unsigned char c = static_cast<unsigned char>(szBuf[k]);
		if (c == 0 || (c < 0x09) || (c > 0x0d && c < 0x20 && c != 0x1b))
			return UT_CONFIDENCE_ZILCH;
	}

	std::string text(szBuf, iNumbytes);
	std::vector<std::string> lines = s_splitLines(text);
	size_t n = lines.size() < 300 ? lines.size() : 300;

	int score = 0;
	bool inFence = false;
	for (size_t i = 0; i < n; i++)
	{
		const std::string & l = lines[i];
		std::string t = s_trim(l);
		if (t.empty()) continue;

		char fc = 0;
		if (s_isFence(l, fc))
		{
			score += inFence ? 0 : 3;
			inFence = !inFence;
			continue;
		}
		if (inFence) continue;

		std::string tmp;
		if (s_atxLevel(l, tmp))				score += 3;
		else if (s_quoteDepth(l, tmp))		score += 2;
		else if (s_isHR(l))					score += 2;

		size_t cp = 0;
		bool bo = false;
		int num = 1;
		if (s_listMarker(l, cp, bo, num))		score += 1;
		if (s_setextLevel(l))				score += 2;
		if (l.find('|') != std::string::npos)
		{
			std::vector<int> al;
			if (i + 1 < n && s_isTableSep(lines[i + 1], al))
				score += 3;
		}
		if (l.find("](") != std::string::npos) score += 2;
		if (l.find("**") != std::string::npos ||
			l.find("__") != std::string::npos ||
			l.find("~~") != std::string::npos ||
			l.find('`') != std::string::npos)
			score += 1;
	}

	if (score >= 6)
		return UT_CONFIDENCE_PERFECT;
	if (score >= 3)
		return UT_CONFIDENCE_GOOD;
	if (score >= 1)
		return UT_CONFIDENCE_SOSO;
	return UT_CONFIDENCE_ZILCH;
}

UT_Error IE_Imp_Markdown_Sniffer::constructImporter(PD_Document * pDocument,
												  IE_Imp ** ppie)
{
	*ppie = new IE_Imp_Markdown(pDocument);
	return UT_OK;
}

bool IE_Imp_Markdown_Sniffer::getDlgLabels(const char ** pszDesc,
										   const char ** pszSuffixList,
										   IEFileType * ft)
{
	*pszDesc = "Markdown (.md, .markdown)";
	*pszSuffixList = "*.md; *.markdown; *.mdown; *.mkd; *.mkdn";
	*ft = getFileType();
	return true;
}
