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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ut_findpattern.h"
#include "ut_assert.h"
#include "ut_string.h"
#include "ut_string_class.h"

/* Word characters for the boundary look-arounds: letters, numbers,
 * underscore — the same class Word's "<"/">" anchors and "Find whole
 * words only" use. */
static const char * const s_wordChars = "\\p{L}\\p{N}_";
static const char * const s_wordStart =
	"(?<![\\p{L}\\p{N}_])(?=[\\p{L}\\p{N}_])";
static const char * const s_wordEnd =
	"(?<=[\\p{L}\\p{N}_])(?![\\p{L}\\p{N}_])";

static void s_appendUcs4(std::string & s, UT_UCS4Char c)
{
	char buf[7];
	gint n = g_unichar_to_utf8(static_cast<gunichar>(c), buf);
	s.append(buf, static_cast<size_t>(n));
}

/*!
 * Translate a Word-style wildcard pattern to PCRE.
 *
 * Recognised constructs (MS Word "Use wildcards" syntax):
 *   ?          any single character
 *   *          any run of characters
 *   [a-z]      character class
 *   [!a-z]     negated character class
 *   <          start of word
 *   >          end of word
 *   {n} {n,} {n,m}  repeat count for the previous element
 *   @          one or more of the previous element
 *   (...)      numbered group (references as \1..\9 in replace)
 *   \x         literal x (escapes the wildcard characters)
 *   ^t ^n ^l ^p  tab / line break / manual line break / paragraph mark
 *   ^^         literal caret
 *   anything else  literal
 *
 * Document text is searched one block buffer at a time, so the
 * line/paragraph codes can only match the (rare) literal control
 * characters that survive inside a block.
 */
std::string UT_findPatternWildcardToRegex(const UT_UCS4Char * pattern)
{
	UT_return_val_if_fail(pattern, std::string());

	std::string out;
	std::string lit;
	const UT_UCS4Char * p = pattern;

	auto flushLit = [&out, &lit]() {
		if (!lit.empty())
		{
			gchar * esc = g_regex_escape_string(
				lit.c_str(), static_cast<gint>(lit.size()));
			out += esc;
			g_free(esc);
			lit.clear();
		}
	};

	while (*p)
	{
		switch (*p)
		{
		case '?':
			flushLit();
			out += '.';
			p++;
			break;

		case '*':
			flushLit();
			out += ".*";
			p++;
			break;

		case '@':
			flushLit();
			out += '+';
			p++;
			break;

		case '(':
		case ')':
			flushLit();
			out += static_cast<char>(*p);
			p++;
			break;

		case '<':
			flushLit();
			out += s_wordStart;
			p++;
			break;

		case '>':
			flushLit();
			out += s_wordEnd;
			p++;
			break;

		case '[':
		{
			/* find the closing ']'; contents are emitted nearly
			 * verbatim ('!' -> '^' negation, '\' stays escaped) */
			const UT_UCS4Char * q = p + 1;
			bool bNegate = (*q == '!');
			if (bNegate)
				q++;
			const UT_UCS4Char * body = q;
			while (*q && *q != ']')
				q++;
			if (*q == ']' && q > body)
			{
				flushLit();
				out += '[';
				if (bNegate)
					out += '^';
				for (const UT_UCS4Char * c = body; c < q; c++)
				{
					if (*c == '\\')
						out += "\\\\";
					else if (*c == '^' || *c == '[')
					{
						/* '^' at class start would negate in PCRE and
						 * '[' can open a POSIX class — both are
						 * literal in Word's wildcard syntax */
						out += '\\';
						s_appendUcs4(out, *c);
					}
					else
						s_appendUcs4(out, *c);
				}
				out += ']';
				p = q + 1;
			}
			else
			{
				/* unterminated or empty class: literal '[' */
				lit += '[';
				p++;
			}
			break;
		}

		case '{':
		{
			/* {n} {n,} {n,m} only when well-formed, else literal */
			const UT_UCS4Char * q = p + 1;
			while (*q >= '0' && *q <= '9')
				q++;
			bool bOk = (q > p + 1);
			if (bOk && *q == ',')
			{
				q++;
				while (*q >= '0' && *q <= '9')
					q++;
			}
			if (bOk && *q == '}')
			{
				flushLit();
				for (const UT_UCS4Char * c = p; c <= q; c++)
					out += static_cast<char>(*c);
				p = q + 1;
			}
			else
			{
				lit += '{';
				p++;
			}
			break;
		}

		case '\\':
			/* escape: the next char is literal */
			p++;
			if (*p)
			{
				s_appendUcs4(lit, *p);
				p++;
			}
			else
			{
				lit += '\\';
			}
			break;

		case '^':
			p++;
			switch (*p)
			{
			case 't':
				flushLit();
				out += "\\t";
				p++;
				break;
			case 'n':
			case 'l':
			case 'p':
				flushLit();
				out += "\\n";
				p++;
				break;
			case '^':
				lit += '^';
				p++;
				break;
			default:
				/* unknown code: literal caret */
				lit += '^';
				break;
			}
			break;

		default:
			s_appendUcs4(lit, *p);
			p++;
			break;
		}
	}
	flushLit();
	return out;
}

GRegex * UT_findPatternCompile(UT_FindPatternKind kind,
							   const UT_UCS4Char * pattern,
							   bool bMatchCase,
							   bool bWholeWord,
							   GError ** error)
{
	UT_return_val_if_fail(pattern, nullptr);

	std::string rx;
	if (kind == UFP_WILDCARD)
	{
		rx = UT_findPatternWildcardToRegex(pattern);
	}
	else
	{
		UT_UTF8String utf8(pattern);
		if (kind == UFP_LITERAL)
		{
			/* callers search literally through a different path, but
			 * keep the compiled form honest for any direct caller */
			gchar * esc = g_regex_escape_string(
				utf8.utf8_str(),
				static_cast<gint>(strlen(utf8.utf8_str())));
			rx = esc ? esc : "";
			g_free(esc);
		}
		else
			rx = utf8.utf8_str();
	}

	if (bWholeWord)
	{
		rx = "(?<![" + std::string(s_wordChars) + "])(?:" + rx
			+ ")(?![" + s_wordChars + "])";
	}

	GRegexCompileFlags flags = static_cast<GRegexCompileFlags>(0);
	if (!bMatchCase)
		flags = G_REGEX_CASELESS;

	return g_regex_new(rx.c_str(), flags,
					   static_cast<GRegexMatchFlags>(0), error);
}

std::string UT_findPatternExpandReplace(
	const std::string & replace,
	const std::vector<std::string> & groups)
{
	std::string out;
	for (size_t i = 0; i < replace.size(); i++)
	{
		if (replace[i] != '\\')
		{
			out += replace[i];
			continue;
		}
		if (i + 1 >= replace.size())
		{
			out += '\\';
			break;
		}
		char next = replace[i + 1];
		if (next == '\\')
		{
			out += '\\';
			i++;
			continue;
		}
		if (next >= '0' && next <= '9')
		{
			size_t num = static_cast<size_t>(next - '0');
			size_t j = i + 2;
			if (j < replace.size() && replace[j] >= '0' && replace[j] <= '9')
			{
				/* a second digit only binds when the two-digit group
				 * exists — \12 means group 1 + "2" otherwise (the
				 * JS $12 rule) so single-digit refs can't be eaten */
				size_t num2 = num * 10 + static_cast<size_t>(replace[j] - '0');
				if (num2 < groups.size())
				{
					num = num2;
					j++;
				}
			}
			if (num < groups.size())
				out += groups[num];
			i = j - 1;
			continue;
		}
		out += '\\';
		out += next;
		i++;
	}
	return out;
}
