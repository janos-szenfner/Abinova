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
 * Built-in LaTeX importer. Covers the common document subset:
 * \documentclass/\usepackage (skipped), \title/\author/\date with
 * \maketitle, sectioning commands, \text* formatting commands and
 * {\bf ...}-style group forms, itemize/enumerate/description lists,
 * quote/quotation/verse, verbatim/lstlisting, center/flushleft/
 * flushright, tabular, \includegraphics, \footnote, inline and
 * display math (imported as styled text), comments, escaped
 * specials, quote/dash ligatures and accent commands,
 * \hrule/\newpage.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

#include <gsf/gsf-input.h>

#include "ie_imp_LaTeX.h"
#include "ie_impGraphic.h"
#include "ie_types.h"
#include "fg_Graphic.h"
#include "pd_Document.h"
#include "pt_Types.h"
#include "ut_debugmsg.h"
#include "ut_go_file.h"
#include "ut_string.h"
#include "ut_std_string.h"
#include "ut_string_class.h"
#include "ut_types.h"
#include "ut_units.h"
#include "ut_locale.h"

/*****************************************************************/
/*****************************************************************/

IE_Imp_LaTeX::IE_Imp_LaTeX(PD_Document * pDocument)
	: IE_Imp(pDocument),
	  m_nextListID(0),
	  m_nextImage(0),
	  m_nextFootnote(0)
{
}

IE_Imp_LaTeX::~IE_Imp_LaTeX()
{
}

/*****************************************************************/
/* lexer helpers                                                 */
/*****************************************************************/

static std::string s_trim(const std::string & s)
{
	size_t b = s.find_first_not_of(" \t\n\r");
	if (b == std::string::npos)
		return "";
	size_t e = s.find_last_not_of(" \t\n\r");
	return s.substr(b, e - b + 1);
}

/*! Skip whitespace starting at pos. */
static size_t s_skipSpaces(const std::string & s, size_t pos)
{
	while (pos < s.size() &&
		   (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n'))
		pos++;
	return pos;
}

/*! Read a control-sequence name starting at pos (s[pos] == '\\').
 *  Advances pos past the name.  Single non-letter commands (\%, \\,
 *  \$, \&, ...) return their one-char name. */
static std::string s_readCommand(const std::string & s, size_t & pos)
{
	size_t i = pos + 1;
	if (i >= s.size())
		return "";
	if (isalpha(static_cast<unsigned char>(s[i])) || s[i] == '@')
	{
		size_t st = i;
		while (i < s.size() &&
			   (isalpha(static_cast<unsigned char>(s[i])) || s[i] == '@'))
			i++;
		pos = i;
		return s.substr(st, i - st);
	}
	pos = i + 1;
	return s.substr(i, 1);
}

/*! Find the '}' matching the '{' at pos, honoring escapes and
 *  nesting. Returns npos if unbalanced. */
static size_t s_matchBrace(const std::string & s, size_t pos)
{
	int depth = 0;
	for (size_t i = pos; i < s.size(); i++)
	{
		char c = s[i];
		if (c == '\\') { i++; continue; }
		if (c == '{') depth++;
		else if (c == '}')
		{
			if (--depth == 0)
				return i;
		}
	}
	return std::string::npos;
}

/*! Extract a {...} group at pos (leading whitespace tolerated).
 *  On success fills out with the contents and advances pos past the
 *  closing brace. */
static bool s_readGroup(const std::string & s, size_t & pos,
						std::string & out)
{
	size_t p = s_skipSpaces(s, pos);
	if (p >= s.size() || s[p] != '{')
		return false;
	size_t close = s_matchBrace(s, p);
	if (close == std::string::npos)
		return false;
	out = s.substr(p + 1, close - p - 1);
	pos = close + 1;
	return true;
}

/*! Skip an optional [...] argument at pos, if present. */
static void s_skipOptArg(const std::string & s, size_t & pos)
{
	size_t p = s_skipSpaces(s, pos);
	if (p >= s.size() || s[p] != '[')
		return;
	int depth = 0;
	for (size_t i = p; i < s.size(); i++)
	{
		char c = s[i];
		if (c == '\\') { i++; continue; }
		if (c == '[') depth++;
		else if (c == ']')
		{
			if (--depth == 0)
			{
				pos = i + 1;
				return;
			}
		}
	}
	pos = p; // unbalanced: leave pos pointing at '['
}

/*! Strip LaTeX comments: '%' to end of line, unless escaped. */
static std::string s_stripComments(const std::string & s)
{
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); i++)
	{
		char c = s[i];
		if (c == '\\' && i + 1 < s.size())
		{
			out += c;
			out += s[++i];
			continue;
		}
		if (c == '%')
		{
			while (i < s.size() && s[i] != '\n')
				i++;
			if (i < s.size())
				out += '\n';
			continue;
		}
		out += c;
	}
	return out;
}

/*****************************************************************/
/* inline formatting                                             */
/*****************************************************************/

namespace {

struct TexFmt {
	bool bold = false;
	bool italic = false;
	bool underline = false;
	bool mono = false;
	bool superscript = false;
	bool subscript = false;
};

} // anonymous namespace

static std::string s_fmtProps(const TexFmt & f)
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
	if (f.underline) add("text-decoration:underline");
	if (f.mono) add("font-family:Courier New");
	if (f.superscript) add("text-position:superscript");
	if (f.subscript) add("text-position:subscript");
	return p;
}

/*! Emit one text segment under the given fmt state; '\f' and '\n'
 *  inside the text become page/line breaks. */
static void s_emitSegment(IE_Imp_LaTeX * imp, const std::string & text,
						  const TexFmt & f)
{
	if (text.empty())
		return;

	std::string props = s_fmtProps(f);
	PP_PropertyVector atts;
	if (!props.empty())
	{
		atts.push_back(PT_PROPS_ATTRIBUTE_NAME);
		atts.push_back(props);
	}
	if (!imp->appendFmtPublic(atts))
		return;

	size_t pos = 0;
	while (pos <= text.size())
	{
		size_t br = text.find_first_of("\n\f", pos);
		std::string piece = text.substr(pos,
				br == std::string::npos ? br : br - pos);
		if (!piece.empty())
		{
			UT_UCS4String u(piece.c_str());
			imp->appendSpanPublic(u.ucs4_str(), u.length());
		}
		if (br == std::string::npos)
			break;
		UT_UCS4Char c = (text[br] == '\f') ? UCS_FF : UCS_LF;
		imp->appendSpanPublic(&c, 1);
		pos = br + 1;
	}
}

/*! Parse a LaTeX accent command at pos: \'e \"o \^a \~n \`e
 *  \c{c} \u{a} \v{s} \={o} \.{i} \r a \k{a} \H{o} \d{u} \b{o}.
 *  Appends the composed UTF-8 sequence to out and advances pos.
 *  Returns false when cmd is not an accent command. */
static bool s_accentChar(const std::string & s, size_t & pos,
						 const std::string & cmd, std::string & out)
{
	static const struct { const char * cmd; const char * dia; } acc[] = {
		{ "'",  "\xCC\x81" },          // combining acute
		{ "`",  "\xCC\x80" },          // grave
		{ "\"", "\xCC\x88" },          // diaeresis
		{ "^",  "\xCC\x82" },          // circumflex
		{ "~",  "\xCC\x83" },          // tilde
		{ "=",  "\xCC\x84" },          // macron
		{ ".",  "\xCC\x87" },          // dot above
		{ "u",  "\xCC\x86" },          // breve
		{ "v",  "\xCC\x8C" },          // caron
		{ "H",  "\xCC\x8B" },          // double acute
		{ "r",  "\xCC\x8A" },          // ring above
		{ "k",  "\xCC\xA8" },          // ogonek
		{ "c",  "\xCC\xA7" },          // cedilla
		{ "d",  "\xCC\xA3" },          // dot below
		{ "b",  "\xCC\xB1" },          // macron below
	};
	const char * dia = nullptr;
	for (const auto & a : acc)
		if (cmd == a.cmd) { dia = a.dia; break; }
	if (!dia)
		return false;

	std::string letter;
	size_t p = s_skipSpaces(s, pos);
	if (p < s.size() && s[p] == '{')
	{
		if (!s_readGroup(s, pos, letter))
			return false;
	}
	else if (p < s.size())
	{
		letter = s.substr(p, 1);
		pos = p + 1;
	}
	else
		return false;
	if (letter.empty())
		return false;
	out += letter;
	out += dia;
	return true;
}

/*! Commands that map to literal text/symbols. */
static const char * s_symbolCommand(const std::string & cmd)
{
	static const struct { const char * cmd; const char * text; } syms[] = {
		{ "LaTeX",            "LaTeX" },
		{ "TeX",              "TeX" },
		{ "ldots",            "\xE2\x80\xA6" },
		{ "dots",             "\xE2\x80\xA6" },
		{ "textellipsis",     "\xE2\x80\xA6" },
		{ "textbackslash",    "\\" },
		{ "textasciitilde",   "~" },
		{ "textasciicircum",  "^" },
		{ "textendash",       "\xE2\x80\x93" },
		{ "textemdash",       "\xE2\x80\x94" },
		{ "textquoteleft",    "\xE2\x80\x98" },
		{ "textquoteright",   "\xE2\x80\x99" },
		{ "textquotedblleft",  "\xE2\x80\x9C" },
		{ "textquotedblright", "\xE2\x80\x9D" },
		{ "copyright",        "\xC2\xA9" },
		{ "S",                "\xC2\xA7" },
		{ "P",                "\xC2\xB6" },
		{ "dag",              "\xE2\x80\xA0" },
		{ "ddag",             "\xE2\x80\xA1" },
		{ "pounds",           "\xC2\xA3" },
		{ "texteuro",         "\xE2\x82\xAC" },
		{ "ae", "\xC3\xA6" }, { "AE", "\xC3\x86" },
		{ "oe", "\xC5\x93" }, { "OE", "\xC5\x92" },
		{ "aa", "\xC3\xA5" }, { "AA", "\xC3\x85" },
		{ "o",  "\xC3\xB8" }, { "O",  "\xC3\x98" },
		{ "l",  "\xC5\x82" }, { "L",  "\xC5\x81" },
		{ "ss", "\xC3\x9F" }, { "SS", "\xE1\xBA\x9E" },
		{ "i",  "\xC4\xB1" }, { "j",  "\xC4\xB3" },
	};
	for (const auto & s : syms)
		if (cmd == s.cmd) return s.text;
	return nullptr;
}

/*! \textXX{...} commands that set fmt for their group argument.
 *  Returns false when cmd is not a fmt command. */
static bool s_fmtCommand(const std::string & cmd, TexFmt & f)
{
	if (cmd == "textbf" || cmd == "mathbf")      f.bold = true;
	else if (cmd == "textit" || cmd == "emph" ||
			 cmd == "textsl" || cmd == "mathit") f.italic = true;
	else if (cmd == "texttt" || cmd == "mathtt") f.mono = true;
	else if (cmd == "underline")                 f.underline = true;
	else if (cmd == "textsuperscript")           f.superscript = true;
	else if (cmd == "textsubscript")             f.subscript = true;
	else if (cmd == "textrm" || cmd == "textsf" ||
			 cmd == "textnormal" || cmd == "textup" ||
			 cmd == "textmd" || cmd == "text" ||
			 cmd == "mathrm" || cmd == "mbox" ||
			 cmd == "textsc")
		return true;   // no fmt we track; still consume the group
	else
		return false;
	return true;
}

/*! Old-style declarations {\bf ...}: set fmt, return false when the
 *  command is not a declaration. */
static bool s_declCommand(const std::string & cmd, TexFmt & f)
{
	if (cmd == "bf" || cmd == "bfseries")        { f.bold = true; return true; }
	if (cmd == "it" || cmd == "em" ||
		 cmd == "itshape" || cmd == "sl" ||
		 cmd == "slshape")                       { f.italic = true; return true; }
	if (cmd == "tt" || cmd == "ttfamily")        { f.mono = true; return true; }
	if (cmd == "uline" || cmd == "ul")           { f.underline = true; return true; }
	if (cmd == "sc" || cmd == "scshape" ||
		 cmd == "rm" || cmd == "sf" ||
		 cmd == "rmfamily" || cmd == "sffamily" ||
		 cmd == "normalfont" || cmd == "upshape" ||
		 cmd == "mdseries")
		return true;   // recognized, no fmt change we track
	return false;
}

/*! Font-size declarations -> point size. 0 when not a size cmd. */
static int s_fontSizeCommand(const std::string & cmd)
{
	static const struct { const char * cmd; int pt; } sizes[] = {
		{ "tiny", 6 }, { "scriptsize", 8 }, { "footnotesize", 9 },
		{ "small", 10 }, { "normalsize", 11 }, { "large", 13 },
		{ "Large", 15 }, { "LARGE", 18 }, { "huge", 22 },
		{ "Huge", 25 },
	};
	for (const auto & sz : sizes)
		if (cmd == sz.cmd) return sz.pt;
	return 0;
}

static void s_emitTeXInline(IE_Imp_LaTeX * imp, const std::string & text,
							const TexFmt & base);

/*! Emit one inline run of text under fmt; recurses into groups and
 *  \textXX{...} commands. */
static void s_emitInlineRun(IE_Imp_LaTeX * imp, const std::string & text,
							size_t & i, TexFmt & fmt, std::string & pending)
{
	const size_t n = text.size();
	char c = text[i];

	auto flush = [&]() {
		s_emitSegment(imp, pending, fmt);
		pending.clear();
	};

	if (c == '\\')
	{
		size_t save = i;
		std::string cmd = s_readCommand(text, i);
		if (cmd.empty()) { pending += '\\'; return; }

		// escaped specials / breaks
		if (cmd == "%") { pending += '%'; return; }
		if (cmd == "&") { pending += '&'; return; }
		if (cmd == "#") { pending += '#'; return; }
		if (cmd == "_") { pending += '_'; return; }
		if (cmd == "$") { pending += '$'; return; }
		if (cmd == "{") { pending += '{'; return; }
		if (cmd == "}") { pending += '}'; return; }
		if (cmd == " " || cmd == "," || cmd == ";" || cmd == ":")
		{
			pending += ' ';
			return;
		}
		if (cmd == "!" || cmd == "-") { return; } // spacing/hyphen hints
		if (cmd == "\\" || cmd == "newline" || cmd == "linebreak")
		{
			pending += '\n';
			s_skipOptArg(text, i);
			return;
		}
		if (cmd == "par") { pending += '\n'; pending += '\n'; return; }
		if (cmd == "~") { pending += "\xC2\xA0"; return; }
		if (cmd == "|") { pending += '|'; return; }
		if (cmd == "(") // \( ... \) inline math
		{
			size_t end = text.find("\\)", i);
			if (end == std::string::npos) { pending += "\\("; return; }
			flush();
			TexFmt mf = fmt; mf.italic = true;
			s_emitSegment(imp, text.substr(i, end - i), mf);
			i = end + 2;
			return;
		}
		if (cmd == ")") { return; }

		// accent commands: \' \" \` \^ \~ \= \. \u \v \H \r \k \c \d \b
		{
			size_t accPos = i;
			if (s_accentChar(text, accPos, cmd, pending))
			{
				i = accPos;
				return;
			}
		}

		// \textXX{...} / \emph{...} / \underline{...}
		{
			TexFmt nf = fmt;
			if (s_fmtCommand(cmd, nf))
			{
				std::string arg;
				size_t p = i;
				if (s_readGroup(text, p, arg))
				{
					flush();
					s_emitTeXInline(imp, arg, nf);
					i = p;
					return;
				}
				// no group: emit the command name literally
				i = save;
				pending += '\\';
				pending += cmd;
				i = save + 1 + cmd.size();
				return;
			}
		}

		// \verb<d>...<d> / \verb*<d>...<d>
		if (cmd == "verb")
		{
			size_t p = i;
			if (p < n && text[p] == '*') p++;
			if (p < n && text[p] != '{' && text[p] != ' ' && text[p] != '\n')
			{
				char d = text[p++];
				size_t end = text.find(d, p);
				if (end != std::string::npos)
				{
					flush();
					TexFmt vf = fmt; vf.mono = true;
					s_emitSegment(imp, text.substr(p, end - p), vf);
					i = end + 1;
					return;
				}
			}
			pending += "\\verb";
			return;
		}

		// \footnote{...}
		if (cmd == "footnote")
		{
			std::string arg;
			size_t p = i;
			s_skipOptArg(text, p);
			if (s_readGroup(text, p, arg))
			{
				flush();
				imp->_emitFootnotePublic(arg);
				i = p;
				return;
			}
		}

		// \href{url}{text} / \url{...} / \path|...|
		if (cmd == "href")
		{
			std::string url, label;
			size_t p = i;
			if (s_readGroup(text, p, url) && s_readGroup(text, p, label))
			{
				pending += label;
				i = p;
				return;
			}
		}
		if (cmd == "url" || cmd == "path")
		{
			std::string arg;
			size_t p = i;
			if (s_readGroup(text, p, arg))
			{
				pending += arg;
				i = p;
				return;
			}
		}

		// \includegraphics[..]{file}
		if (cmd == "includegraphics")
		{
			std::string arg;
			size_t p = i;
			s_skipOptArg(text, p);
			if (s_readGroup(text, p, arg))
			{
				flush();
				imp->_emitImagePublic(arg);
				i = p;
				return;
			}
		}

		// font-size declarations inside text
		if (s_fontSizeCommand(cmd))
			return;

		// {\bf ...}-style declarations used bare: set current fmt
		{
			TexFmt nf = fmt;
			if (s_declCommand(cmd, nf))
			{
				flush();
				fmt = nf;
				return;
			}
		}

		// symbol commands
		if (const char * sym = s_symbolCommand(cmd))
		{
			pending += sym;
			// swallow an empty {} following \LaTeX etc.
			size_t p = s_skipSpaces(text, i);
			if (p + 1 < n && text[p] == '{' && text[p + 1] == '}')
				i = p + 2;
			return;
		}

		// unknown command: emit its name literally so text survives
		pending += cmd;
		return;
	}

	if (c == '{')
	{
		size_t close = s_matchBrace(text, i);
		if (close == std::string::npos)
		{
			pending += c;
			i++;
			return;
		}
		std::string inner = text.substr(i + 1, close - i - 1);
		// leading declarations inside the group: {\bf text}
		TexFmt gf = fmt;
		size_t k = s_skipSpaces(inner, 0);
		while (k < inner.size() && inner[k] == '\\')
		{
			size_t kp = k;
			std::string dc = s_readCommand(inner, kp);
			TexFmt tmp = gf;
			if (s_declCommand(dc, tmp)) { gf = tmp; k = kp; continue; }
			if (s_fontSizeCommand(dc)) { k = kp; continue; }
			break;
		}
		flush();
		s_emitTeXInline(imp, inner.substr(k), gf);
		i = close + 1;
		return;
	}

	if (c == '$')
	{
		if (i + 1 < n && text[i + 1] == '$')
		{
			size_t end = text.find("$$", i + 2);
			if (end != std::string::npos)
			{
				flush();
				TexFmt mf = fmt; mf.italic = true;
				s_emitSegment(imp, text.substr(i + 2, end - i - 2), mf);
				i = end + 2;
				return;
			}
		}
		size_t end = i + 1;
		while (end < n && text[end] != '$')
		{
			if (text[end] == '\\') end++;
			if (end < n) end++;
		}
		if (end < n)
		{
			flush();
			TexFmt mf = fmt; mf.italic = true;
			s_emitSegment(imp, text.substr(i + 1, end - i - 1), mf);
			i = end + 1;
			return;
		}
		pending += c; i++; return;
	}

	if (c == '~') { pending += "\xC2\xA0"; i++; return; }

	// ligatures
	if (c == '-' && i + 2 < n && text[i + 1] == '-' && text[i + 2] == '-')
	{
		pending += "\xE2\x80\x94"; i += 3; return;
	}
	if (c == '-' && i + 1 < n && text[i + 1] == '-')
	{
		pending += "\xE2\x80\x93"; i += 2; return;
	}
	if (c == '`' && i + 1 < n && text[i + 1] == '`')
	{
		pending += "\xE2\x80\x9C"; i += 2; return;
	}
	if (c == '\'' && i + 1 < n && text[i + 1] == '\'')
	{
		pending += "\xE2\x80\x9D"; i += 2; return;
	}
	if (c == '`') { pending += "\xE2\x80\x98"; i++; return; }

	pending += c;
	i++;
}

/*! Full inline parse of a text fragment under a base fmt. */
static void s_emitTeXInline(IE_Imp_LaTeX * imp, const std::string & text,
							const TexFmt & base)
{
	TexFmt fmt = base;
	std::string pending;
	for (size_t i = 0; i < text.size(); )
		s_emitInlineRun(imp, text, i, fmt, pending);
	s_emitSegment(imp, pending, fmt);
}

void IE_Imp_LaTeX::_emitInline(const std::string & text)
{
	TexFmt fmt;
	s_emitTeXInline(this, text, fmt);
}

/*****************************************************************/
/* block emitters                                                */
/*****************************************************************/

bool IE_Imp_LaTeX::_emitParagraph(const char * szStyle,
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

bool IE_Imp_LaTeX::_emitHeading(int level, const std::string & text)
{
	std::string style = "Heading ";
	style += static_cast<char>('0' + (level > 4 ? 4 : level));
	return _emitParagraph(style.c_str(), "", text);
}

bool IE_Imp_LaTeX::_emitListItem(int level, bool bOrdered,
								 const std::string & text)
{
	if (level < 1) level = 1;

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
	}
	while (m_listIds.size() > static_cast<size_t>(level))
	{
		m_listIds.pop_back();
		m_listOrdered.pop_back();
	}
	if (m_listOrdered[level - 1] != bOrdered)
	{
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
	}

	std::string sid = UT_std_string_sprintf("%u", m_listIds[level - 1]);
	std::string sparent = UT_std_string_sprintf("%u",
						level > 1 ? m_listIds[level - 2] : 0);
	std::string slevel = UT_std_string_sprintf("%d", level);

	std::string props;
	{
		UT_LocaleTransactor t(LC_NUMERIC, "C");
		props = UT_std_string_sprintf(
			"list-style:%s; start-value:0; text-indent:-0.3in; field-font:NULL; margin-left:%.2fin",
			bOrdered ? "Numbered List" : "Bullet List",
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

	_emitInline(text);
	return true;
}

bool IE_Imp_LaTeX::_emitVerbatim(const std::string & text)
{
	size_t pos = 0;
	while (pos <= text.size())
	{
		size_t nl = text.find('\n', pos);
		std::string line = text.substr(pos,
				nl == std::string::npos ? nl : nl - pos);
		PP_PropertyVector atts = {
			"style", "Plain Text"
		};
		if (!appendStrux(PTX_Block, atts))
			return false;
		if (!line.empty())
		{
			const PP_PropertyVector fatts = {
				PT_PROPS_ATTRIBUTE_NAME, "font-family:Courier New"
			};
			appendFmt(fatts);
			appendSpan(line);
		}
		if (nl == std::string::npos)
			break;
		pos = nl + 1;
	}
	return true;
}

bool IE_Imp_LaTeX::_emitTable(const std::vector<std::vector<std::string> > & rows)
{
	if (rows.empty()) return false;
	size_t cols = 0;
	for (const auto & r : rows)
		if (r.size() > cols) cols = r.size();
	if (!cols) return false;

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

bool IE_Imp_LaTeX::_emitHR(void)
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

bool IE_Imp_LaTeX::_emitPageBreak(void)
{
	PP_PropertyVector atts = {
		"style", "Normal"
	};
	if (!appendStrux(PTX_Block, atts))
		return false;
	UT_UCS4Char ff = UCS_FF;
	appendSpan(&ff, 1);
	return true;
}

/*! Emit a real footnote: anchor field inline, then the footnote
 *  section strux with its own block. */
void IE_Imp_LaTeX::_emitFootnote(const std::string & text)
{
	UT_uint32 id = getDoc()->getUID(UT_UniqueId::Footnote);
	std::string sid = UT_std_string_sprintf("%u", id);

	const PP_PropertyVector aatts = {
		"type", "footnote_anchor",
		"footnote-id", sid
	};
	if (!appendObject(PTO_Field, aatts))
		return;
	UT_UCS4Char tab = UCS_TAB;
	appendSpan(&tab, 1);

	const PP_PropertyVector satts = {
		"footnote-id", sid
	};
	if (!appendStrux(PTX_SectionFootnote, satts))
		return;
	const PP_PropertyVector batts = {
		"style", "Footnote Text"
	};
	if (!appendStrux(PTX_Block, batts))
		return;
	_emitInline(text);
	appendStrux(PTX_EndFootnote, PP_NOPROPS);
	appendFmt(PP_NOPROPS);
}

void IE_Imp_LaTeX::_emitImagePublic(const std::string & file)
{
	char * resolved = UT_go_url_resolve_relative(m_fileName.c_str(),
												 file.c_str());
	if (!resolved)
		return;
	FG_ConstGraphicPtr pfg;
	UT_Error err = IE_ImpGraphic::loadGraphic(resolved, IEGFT_Unknown, pfg);
	g_free(resolved);
	if (err != UT_OK || !pfg)
		return;

	std::string dataid = UT_std_string_sprintf("tex-image%u", m_nextImage++);
	const PP_PropertyVector atts = {
		PT_PROPS_ATTRIBUTE_NAME, "",
		"dataid", dataid,
	};
	if (!appendObject(PTO_Image, atts))
		return;
	getDoc()->createDataItem(dataid.c_str(), false, pfg->getBuffer(),
							 pfg->getMimeType(), nullptr);
}

void IE_Imp_LaTeX::_resetLists(void)
{
	m_listIds.clear();
	m_listOrdered.clear();
}

/*****************************************************************/
/* block-level parsing                                           */
/*****************************************************************/

namespace {

struct EnvFrame {
	std::string name;
};

} // anonymous namespace

/*! Environment name from "\begin{xxx}" or "\end{xxx}" at pos. */
static std::string s_envName(const std::string & s, size_t pos)
{
	size_t p = pos;
	std::string cmd = s_readCommand(s, p);      // begin/end
	size_t g = s_skipSpaces(s, p);
	if (g >= s.size() || s[g] != '{')
		return "";
	size_t close = s_matchBrace(s, g);
	if (close == std::string::npos)
		return "";
	return s.substr(g + 1, close - g - 1);
}

/*! Skip "\begin{xxx}" / "\end{xxx}" at pos entirely. */
static size_t s_skipEnvCmd(const std::string & s, size_t pos)
{
	size_t p = pos;
	s_readCommand(s, p);
	s_skipOptArg(s, p);
	std::string dummy;
	s_readGroup(s, p, dummy);
	s_skipOptArg(s, p);
	return p;
}

/*! Find the "\end{name}" matching the environment opened at pos
 *  (pos points at the '\' of the \begin). Returns the position of the
 *  \end and the content range [contentStart, contentEnd). */
static bool s_findEnvEnd(const std::string & s, size_t pos,
						 const std::string & name,
						 size_t & contentStart, size_t & contentEnd,
						 size_t & afterEnd)
{
	contentStart = s_skipEnvCmd(s, pos);
	std::string open = "\\begin{" + name + "}";
	std::string close = "\\end{" + name + "}";
	int depth = 1;
	size_t i = contentStart;
	while (i < s.size())
	{
		if (s[i] == '\\')
		{
			if (s.compare(i, open.size(), open) == 0)
			{
				depth++;
				i += open.size();
				continue;
			}
			if (s.compare(i, close.size(), close) == 0)
			{
				if (--depth == 0)
				{
					contentEnd = i;
					afterEnd = s_skipEnvCmd(s, i);
					return true;
				}
				i += close.size();
				continue;
			}
		}
		i++;
	}
	return false;
}

/*! Sectioning command -> heading level (0 = not a heading). */
static int s_sectionLevel(const std::string & cmd)
{
	if (cmd == "part")          return 1;
	if (cmd == "chapter")       return 1;
	if (cmd == "section")       return 1;
	if (cmd == "subsection")    return 2;
	if (cmd == "subsubsection") return 3;
	if (cmd == "paragraph")     return 4;
	if (cmd == "subparagraph")  return 4;
	return 0;
}

/*! Split tabular rows on '\\' and cells on unescaped '&'. */
static std::vector<std::vector<std::string> >
s_parseTabular(const std::string & body)
{
	std::vector<std::vector<std::string> > rows;
	std::vector<std::string> row;
	std::string cell;
	for (size_t i = 0; i < body.size(); i++)
	{
		char c = body[i];
		if (c == '\\' && i + 1 < body.size() && body[i + 1] == '\\')
		{
			row.push_back(s_trim(cell));
			cell.clear();
			// skip rows that are empty (e.g. only \hline rules)
			bool allEmpty = true;
			for (const auto & rc : row)
				if (!rc.empty()) { allEmpty = false; break; }
			if (!allEmpty)
				rows.push_back(row);
			row.clear();
			i++;
			continue;
		}
		if (c == '\\')
		{
			// \hline, \cline{..}, \midrule etc. — skip the command
			size_t p = i;
			std::string cmd = s_readCommand(body, p);
			if (cmd == "hline" || cmd == "cline" || cmd == "midrule" ||
				cmd == "toprule" || cmd == "bottomrule" || cmd == "cmidrule" ||
				cmd == "vline")
			{
				i = p;
				std::string dummy;
				s_readGroup(body, i, dummy);   // \cline{1-2}
				continue;
			}
			// keep other commands for the inline parser
			cell += c;
			continue;
		}
		if (c == '&')
		{
			row.push_back(s_trim(cell));
			cell.clear();
			continue;
		}
		cell += c;
	}
	if (!row.empty() || !s_trim(cell).empty())
	{
		row.push_back(s_trim(cell));
		rows.push_back(row);
	}
	return rows;
}

UT_Error IE_Imp_LaTeX::_loadFile(GsfInput * input)
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

void IE_Imp_LaTeX::_parseDocument(const std::string & utf8)
{
	std::string doc = s_stripComments(utf8);

	// locate \begin{document} ... \end{document}
	std::string preamble;
	std::string body = doc;
	size_t bd = doc.find("\\begin{document}");
	if (bd != std::string::npos)
	{
		preamble = doc.substr(0, bd);
		size_t ed = doc.find("\\end{document}", bd);
		body = doc.substr(bd + strlen("\\begin{document}"),
						  ed == std::string::npos ? ed : ed - bd - strlen("\\begin{document}"));
	}

	// preamble: \title{} \author{} \date{}
	{
		size_t p = 0;
		while (p < preamble.size())
		{
			if (preamble[p] == '\\')
			{
				size_t q = p;
				std::string cmd = s_readCommand(preamble, q);
				if (cmd == "title" || cmd == "author" || cmd == "date")
				{
					std::string arg;
					if (s_readGroup(preamble, q, arg))
					{
						if (cmd == "title")  m_docTitle = arg;
						if (cmd == "author") m_docAuthor = arg;
						if (cmd == "date")   m_docDate = arg;
					}
				}
				p = q;
			}
			p++;
		}
	}

	_parseText(body);
}

void IE_Imp_LaTeX::_parseText(const std::string & text)
{
	size_t i = 0;
	const size_t n = text.size();
	std::string para;              // pending paragraph source
	bool paraHasText = false;

	auto flushPara = [&]() {
		std::string t = s_trim(para);
		para.clear();
		if (!t.empty() || paraHasText)
			_emitParagraph("Normal", "", t);
		paraHasText = false;
	};

	while (i < n)
	{
		char c = text[i];

		// blank line ends the current paragraph
		if (c == '\n')
		{
			size_t j = i + 1;
			while (j < n && (text[j] == ' ' || text[j] == '\t'))
				j++;
			if (j >= n || text[j] == '\n')
			{
				flushPara();
				i = j;
				continue;
			}
			// single newline inside a paragraph -> space
			if (!para.empty() && para.back() != ' ' &&
				para.back() != '\n' && para.back() != '\f')
				para += ' ';
			i++;
			continue;
		}

		if (c == '\\')
		{
			size_t save = i;
			size_t p = i;
			std::string cmd = s_readCommand(text, p);

			// block-level commands
			if (cmd == "begin" || cmd == "end")
			{
				std::string env = s_envName(text, i);
				if (cmd == "end")
				{
					// stray \end — skip it
					i = s_skipEnvCmd(text, i);
					continue;
				}

				// dispatch on the environment
				size_t cStart = 0, cEnd = 0, aEnd = 0;
				bool found = s_findEnvEnd(text, i, env, cStart, cEnd, aEnd);
				std::string contents = found
					? text.substr(cStart, cEnd - cStart) : "";
				size_t next = found ? aEnd : s_skipEnvCmd(text, i);

				if (env == "document")
				{
					// nested \begin{document} inside body: recurse
					_parseText(contents);
					i = next;
					continue;
				}
				if (env == "itemize" || env == "itemize*" ||
					env == "enumerate" || env == "enumerate*" ||
					env == "description")
				{
					flushPara();
					bool ordered = (env == "enumerate" || env == "enumerate*");
					// split contents on \item
					size_t k = 0;
					int itemLevel = 0;
					// count nesting depth for the level
					{
						// level = number of enclosing list envs + 1
						// we don't track env stack; approximate by
						// counting \begin{itemize/enumerate} inside
						// contents later — instead track via member
						itemLevel = static_cast<int>(m_listIds.size()) + 1;
					}
					while (k < contents.size())
					{
						size_t it = contents.find("\\item", k);
						if (it == std::string::npos)
							break;
						size_t bodyStart = it + strlen("\\item");
						s_skipOptArg(contents, bodyStart);
						size_t bodyEnd = contents.find("\\item", bodyStart);
						// but a nested \begin{...} inside may contain
						// \item too — find matching boundaries naively
						// by scanning for the next \item at depth 0
						{
							int depth = 0;
							size_t scan = bodyStart;
							bodyEnd = contents.size();
							while (scan < contents.size())
							{
								if (contents.compare(scan, 6, "\\begin") == 0)
									depth++;
								else if (contents.compare(scan, 4, "\\end") == 0 &&
										 depth > 0)
									depth--;
								else if (contents.compare(scan, 5, "\\item") == 0 &&
										 depth == 0)
								{
									bodyEnd = scan;
									break;
								}
								scan++;
							}
						}
						std::string itemText = s_trim(
							contents.substr(bodyStart, bodyEnd - bodyStart));
						// nested list inside the item: split it out
						size_t nest = itemText.find("\\begin{itemize");
						size_t nestEnum = itemText.find("\\begin{enumerate");
						size_t nestPos = std::min(
							nest == std::string::npos ? itemText.size() : nest,
							nestEnum == std::string::npos ? itemText.size() : nestEnum);
						if (nestPos < itemText.size())
						{
							// emit the item text before the nested env
							std::string head = s_trim(itemText.substr(0, nestPos));
							if (!head.empty())
								_emitListItem(itemLevel, ordered, head);
							// recurse into the nested env text
							_parseText(itemText.substr(nestPos));
						}
						else
							_emitListItem(itemLevel, ordered, itemText);
						k = bodyEnd;
					}
					_resetLists();
					i = next;
					continue;
				}
				if (env == "quote" || env == "quotation" || env == "verse")
				{
					flushPara();
					// paragraphs inside the quote
					size_t k = 0;
					while (k <= contents.size())
					{
						size_t nl = contents.find("\n\n", k);
						std::string blk = contents.substr(k,
								nl == std::string::npos ? nl : nl - k);
						std::string t = s_trim(blk);
						if (!t.empty())
							_emitParagraph("Block Text",
										   "margin-left:0.5in; margin-right:0.5in",
										   t);
						if (nl == std::string::npos) break;
						k = nl + 2;
					}
					i = next;
					continue;
				}
				if (env == "verbatim" || env == "verbatim*" ||
					env == "lstlisting")
				{
					flushPara();
					_emitVerbatim(contents);
					i = next;
					continue;
				}
				if (env == "center" || env == "flushleft" ||
					env == "flushright")
				{
					flushPara();
					const char * align = env == "center" ? "centered"
						: env == "flushright" ? "right" : "left";
					std::string props = std::string("text-align:") + align;
					std::string t = s_trim(contents);
					if (!t.empty())
						_emitParagraph("Normal", props, t);
					i = next;
					continue;
				}
				if (env == "tabular" || env == "tabular*" ||
					env == "array" || env == "longtable")
				{
					flushPara();
					// contents starts with the column spec "{lll}"
					// (s_skipEnvCmd consumed only \begin{tabular}) —
					// skip the leading brace group before parsing rows
					{
						size_t sp = s_skipSpaces(contents, 0);
						if (sp < contents.size() && contents[sp] == '{')
						{
							size_t ce = s_matchBrace(contents, sp);
							if (ce != std::string::npos)
								contents = contents.substr(ce + 1);
						}
					}
					std::vector<std::vector<std::string> > rows =
						s_parseTabular(contents);
					_emitTable(rows);
					i = next;
					continue;
				}
				if (env == "equation" || env == "equation*" ||
					env == "displaymath" || env == "eqnarray" ||
					env == "eqnarray*" || env == "align" ||
					env == "align*" || env == "math")
				{
					flushPara();
					std::string t = s_trim(contents);
					if (!t.empty())
					{
						_emitParagraph("Normal", "text-align:centered",
									   "$" + t + "$");
					}
					i = next;
					continue;
				}
				if (env == "figure" || env == "table" ||
					env == "wrapfigure" || env == "minipage" ||
					env == "abstract")
				{
					flushPara();
					// \caption inside -> centered paragraph; rest normal
					_parseText(contents);
					i = next;
					continue;
				}
				// unknown environment: parse contents normally
				_parseText(contents);
				i = next;
				continue;
			}

			// sectioning commands
			{
				int lvl = s_sectionLevel(cmd);
				if (lvl)
				{
					flushPara();
					i = p;
					if (i < n && text[i] == '*') i++;   // starred form
					s_skipOptArg(text, i);              // [short title]
					std::string title;
					if (s_readGroup(text, i, title))
						_emitHeading(lvl, s_trim(title));
					continue;
				}
			}

			// \maketitle
			if (cmd == "maketitle")
			{
				flushPara();
				if (!m_docTitle.empty())
					_emitParagraph("Title", "text-align:centered", m_docTitle);
				if (!m_docAuthor.empty())
					_emitParagraph("Normal", "text-align:centered", m_docAuthor);
				if (!m_docDate.empty())
					_emitParagraph("Normal", "text-align:centered", m_docDate);
				i = p;
				continue;
			}

			// page breaks / rules
			if (cmd == "newpage" || cmd == "clearpage" ||
				cmd == "pagebreak" || cmd == "cleardoublepage")
			{
				flushPara();
				_emitPageBreak();
				i = p;
				s_skipOptArg(text, i);
				continue;
			}
			if (cmd == "hrule" || cmd == "hrulefill")
			{
				flushPara();
				_emitHR();
				i = p;
				continue;
			}

			// vertical spacing — just end the paragraph
			if (cmd == "bigskip" || cmd == "medskip" || cmd == "smallskip" ||
				cmd == "vspace" || cmd == "vspace*" || cmd == "vfill" ||
				cmd == "noindent" || cmd == "indent" || cmd == "par" ||
				cmd == "hfill" || cmd == "hspace")
			{
				if (cmd == "par")
				{
					flushPara();
					i = p;
					continue;
				}
				std::string dummy;
				s_readGroup(text, p, dummy);   // \vspace{1cm}
				para += ' ';
				i = p;
				continue;
			}

			// everything else (inline commands, escapes, math...) goes
			// into the pending paragraph for the inline parser
			paraHasText = true;
			para += text.substr(save, p - save);
			i = p;
			continue;
		}

		para += c;
		if (!isspace(static_cast<unsigned char>(c)))
			paraHasText = true;
		i++;
	}
	flushPara();
}

/*****************************************************************/
/* sniffer                                                       */
/*****************************************************************/

IE_Imp_LaTeX_Sniffer::IE_Imp_LaTeX_Sniffer()
	: IE_ImpSniffer(IE_IMPEXPNAME_LATEX, true)
{
}

IE_Imp_LaTeX_Sniffer::~IE_Imp_LaTeX_Sniffer()
{
}

static IE_SuffixConfidence IE_Imp_LaTeX_Sniffer__SuffixConfidence[] = {
	{ ".tex",	UT_CONFIDENCE_PERFECT },
	{ ".latex",	UT_CONFIDENCE_PERFECT },
	{ ".ltx",	UT_CONFIDENCE_PERFECT },
	{ "", 		UT_CONFIDENCE_ZILCH 	}
};

const IE_SuffixConfidence * IE_Imp_LaTeX_Sniffer::getSuffixConfidence()
{
	return IE_Imp_LaTeX_Sniffer__SuffixConfidence;
}

static IE_MimeConfidence IE_Imp_LaTeX_Sniffer__MimeConfidence[] = {
	{ IE_MIME_MATCH_FULL,	"text/x-tex",		UT_CONFIDENCE_GOOD 	},
	{ IE_MIME_MATCH_FULL,	"text/x-latex",		UT_CONFIDENCE_GOOD 	},
	{ IE_MIME_MATCH_FULL,	"application/x-latex", UT_CONFIDENCE_GOOD },
	{ IE_MIME_MATCH_FULL,	"application/x-tex", UT_CONFIDENCE_GOOD },
	{ IE_MIME_MATCH_BOGUS,	"", 				UT_CONFIDENCE_ZILCH }
};

const IE_MimeConfidence * IE_Imp_LaTeX_Sniffer::getMimeConfidence()
{
	return IE_Imp_LaTeX_Sniffer__MimeConfidence;
}

UT_Confidence_t IE_Imp_LaTeX_Sniffer::recognizeContents(const char * szBuf,
														UT_uint32 iNumbytes)
{
	// LaTeX is plain text — score characteristic commands and only
	// claim the file on clear evidence so plain-text stays with the
	// Text importer.
	if (!szBuf || iNumbytes < 8)
		return UT_CONFIDENCE_ZILCH;

	for (UT_uint32 k = 0; k < iNumbytes && k < 512; k++)
	{
		unsigned char c = static_cast<unsigned char>(szBuf[k]);
		if (c == 0 || (c < 0x09) || (c > 0x0d && c < 0x20 && c != 0x1b))
			return UT_CONFIDENCE_ZILCH;
	}

	std::string text(szBuf, iNumbytes > 8192 ? 8192 : iNumbytes);

	int score = 0;
	if (text.find("\\documentclass") != std::string::npos) score += 10;
	if (text.find("\\begin{document}") != std::string::npos) score += 8;
	if (text.find("\\usepackage") != std::string::npos) score += 4;
	if (text.find("\\section{") != std::string::npos) score += 4;
	if (text.find("\\begin{") != std::string::npos) score += 3;
	if (text.find("\\textbf{") != std::string::npos ||
		text.find("\\textit{") != std::string::npos ||
		text.find("\\emph{") != std::string::npos) score += 2;
	if (text.find("\\item") != std::string::npos) score += 2;
	if (text.find("\\maketitle") != std::string::npos) score += 2;

	if (score >= 8)
		return UT_CONFIDENCE_PERFECT;
	if (score >= 4)
		return UT_CONFIDENCE_GOOD;
	if (score >= 2)
		return UT_CONFIDENCE_SOSO;
	return UT_CONFIDENCE_ZILCH;
}

UT_Error IE_Imp_LaTeX_Sniffer::constructImporter(PD_Document * pDocument,
											   IE_Imp ** ppie)
{
	*ppie = new IE_Imp_LaTeX(pDocument);
	return UT_OK;
}

bool IE_Imp_LaTeX_Sniffer::getDlgLabels(const char ** pszDesc,
										const char ** pszSuffixList,
										IEFileType * ft)
{
	*pszDesc = "LaTeX (.tex, .latex)";
	*pszSuffixList = "*.tex; *.latex; *.ltx";
	*ft = getFileType();
	return true;
}
