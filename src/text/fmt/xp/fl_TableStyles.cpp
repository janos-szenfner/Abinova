/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
 * Copyright (C) 2025-2026 Abinova contributors
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

#include "fl_TableStyles.h"

#include <cstring>
#include <map>
#include <cstdio>

/* ================================================================
 * small helpers
 * ================================================================ */

std::string FV_TableStyleLook::toString() const
{
	std::string s;
	if (firstRow) s += 'F';
	if (lastRow)  s += 'L';
	if (bandRow)  s += 'B';
	if (firstCol) s += 'f';
	if (lastCol)  s += 'l';
	if (bandCol)  s += 'b';
	return s;
}

FV_TableStyleLook FV_TableStyleLook::fromString(const char * s)
{
	FV_TableStyleLook l;
	l.firstRow = l.lastRow = l.bandRow = l.firstCol = l.lastCol = l.bandCol = false;
	if (!s)
	{
		l.firstRow = l.bandRow = true;	// document default
		return l;
	}
	for (const char * p = s; *p; ++p)
	{
		switch (*p)
		{
		case 'F': l.firstRow = true; break;
		case 'L': l.lastRow  = true; break;
		case 'B': l.bandRow  = true; break;
		case 'f': l.firstCol = true; break;
		case 'l': l.lastCol  = true; break;
		case 'b': l.bandCol  = true; break;
		default: break;
		}
	}
	return l;
}

/* --- prop-string merge: parse "k:v; k:v" pairs --- */

static void fv_parseProps(const std::string & s,
						  std::map<std::string,std::string> & out,
						  std::vector<std::string> & order)
{
	size_t pos = 0;
	while (pos < s.size())
	{
		size_t semi = s.find(';', pos);
		std::string pair = s.substr(pos,
			semi == std::string::npos ? std::string::npos : semi - pos);
		pos = (semi == std::string::npos) ? s.size() : semi + 1;
		size_t c = pair.find(':');
		if (c == std::string::npos)
			continue;
		std::string k = pair.substr(0, c);
		std::string v = pair.substr(c + 1);
		// trim
		auto trim = [](std::string & x) {
			size_t a = x.find_first_not_of(" \t\n");
			size_t b = x.find_last_not_of(" \t\n");
			x = (a == std::string::npos) ? "" : x.substr(a, b - a + 1);
		};
		trim(k); trim(v);
		if (k.empty())
			continue;
		if (out.find(k) == out.end())
			order.push_back(k);
		out[k] = v;
	}
}

std::vector<std::string> FV_tableStyleSplitProps(const std::string & s)
{
	std::map<std::string,std::string> m;
	std::vector<std::string> order;
	fv_parseProps(s, m, order);
	std::vector<std::string> out;
	out.reserve(order.size() * 2);
	for (const std::string & k : order)
	{
		out.push_back(k);
		out.push_back(m[k]);
	}
	return out;
}

std::string FV_tableStyleMergeProps(const std::string & base,
									const std::string & over)
{
	if (over.empty())
		return base;
	if (base.empty())
		return over;
	std::map<std::string,std::string> m;
	std::vector<std::string> order;
	fv_parseProps(base, m, order);
	fv_parseProps(over, m, order);
	std::string r;
	for (const std::string & k : order)
	{
		if (!r.empty())
			r += "; ";
		r += k;
		r += ':';
		r += m[k];
	}
	return r;
}

/* --- colour math --- */

static unsigned fv_hexChan(const std::string & hex, int off)
{
	if (hex.size() < static_cast<size_t>(off + 2))
		return 0;
	unsigned v = 0;
	std::sscanf(hex.substr(off, 2).c_str(), "%x", &v);
	return v & 0xff;
}

static std::string fv_mix(const std::string & hex, double with255, double pct)
{
	unsigned r = fv_hexChan(hex, 0);
	unsigned g = fv_hexChan(hex, 2);
	unsigned b = fv_hexChan(hex, 4);
	char buf[8];
	std::snprintf(buf, sizeof(buf), "%02X%02X%02X",
				  static_cast<unsigned>(r + (with255 - r) * pct + 0.5),
				  static_cast<unsigned>(g + (with255 - g) * pct + 0.5),
				  static_cast<unsigned>(b + (with255 - b) * pct + 0.5));
	return buf;
}

std::string FV_tableStyleTint(const std::string & hex, double pct)
{
	return fv_mix(hex, 255.0, pct);		// toward white
}

std::string FV_tableStyleShade(const std::string & hex, double pct)
{
	return fv_mix(hex, 0.0, pct);		// toward black
}

/* ================================================================
 * the style table
 * ================================================================ */

/* Office accent palette (accent1..accent6) */
static const char * const s_accents[] = {
	"4472C4",	/* Accent 1 blue   */
	"ED7D31",	/* Accent 2 orange */
	"A5A5A5",	/* Accent 3 gray   */
	"FFC000",	/* Accent 4 gold   */
	"5B9BD5",	/* Accent 5 steel  */
	"70AD47"	/* Accent 6 green  */
};
static const int s_nAccents = 6;

static std::string fv_border(const char * side, const char * style,
							 const std::string & color, const char * thick)
{
	std::string s;
	s += side;
	s += "-style:";
	s += style;
	s += "; ";
	s += side;
	s += "-color:";
	s += color;
	s += "; ";
	s += side;
	s += "-thickness:";
	s += thick;
	return s;
}

static std::string fv_border4(const char * style,
							  const std::string & color, const char * thick)
{
	std::string r;
	static const char * sides[] = { "top", "bot", "left", "right" };
	for (const char * s : sides)
	{
		if (!r.empty()) r += "; ";
		r += fv_border(s, style, color, thick);
	}
	return r;
}

static std::string fv_bg(const std::string & color)
{
	return std::string("background-color:") + color;
}

static void fv_setPart(FV_TableStyle & st, FV_TableStylePart p,
					   const std::string & cell,
					   const std::string & chr = std::string())
{
	st.parts[p].cellProps = cell;
	st.parts[p].charProps = chr;
}

/* build the whole table once */
/* ================================================================
 * theme palette + symbolic colour tokens
 * ================================================================ */

static const std::pair<const char *, const char *> s_themeDefaults[] = {
	{ "accent1", "4472C4" }, { "accent2", "ED7D31" },
	{ "accent3", "A5A5A5" }, { "accent4", "FFC000" },
	{ "accent5", "5B9BD5" }, { "accent6", "70AD47" },
	{ "text1", "000000" },   { "text2", "44546A" },
	{ "background1", "FFFFFF" }, { "background2", "E7E6E6" },
	{ "hyperlink", "0563C1" }, { "followedHyperlink", "954F72" },
};

static std::map<std::string, std::string> & fv_theme()
{
	static std::map<std::string, std::string> m;
	if (m.empty())
		for (const auto & p : s_themeDefaults)
			m[p.first] = p.second;
	return m;
}

void FV_setTableStyleThemeColor(const char * name, const char * hex)
{
	if (!name)
		return;
	if (hex && *hex)
		fv_theme()[name] = hex;
	else
		fv_theme().erase(name);
	/* refill missing entries with defaults */
	for (const auto & p : s_themeDefaults)
		if (!fv_theme().count(p.first))
			fv_theme()[p.first] = p.second;
}

/* "theme:accent1:t25" -> "D0D9EA"-style hex; literal values pass
 * through unchanged */
std::string FV_tableStyleResolveColor(const std::string & tok)
{
	if (tok.compare(0, 6, "theme:") != 0)
		return tok;
	std::string body = tok.substr(6);
	std::string name = body;
	double tTint = 0.0, tShade = 0.0;
	size_t c = body.find(':');
	while (c != std::string::npos)
	{
		size_t e = body.find(':', c + 1);
		std::string mod = body.substr(c + 1,
			e == std::string::npos ? std::string::npos : e - c - 1);
		if (mod.size() > 1 && (mod[0] == 't' || mod[0] == 's'))
		{
			double v = atof(mod.c_str() + 1) / 100.0;
			if (mod[0] == 't') tTint = v;
			else               tShade = v;
		}
		c = e;
	}
	name = body.substr(0, body.find(':'));
	auto it = fv_theme().find(name);
	if (it == fv_theme().end())
		return "";
	std::string hex = it->second;
	if (tTint > 0)
		hex = fv_mix(hex, 255.0, 1.0 - tTint);
	if (tShade > 0)
		hex = fv_mix(hex, 0.0, tShade);
	return hex;
}

/* ================================================================
 * the style table (generated data + hand additions)
 * ================================================================ */

extern const FV_TableStyleBuiltin s_tableStyleBuiltin[];

static const std::vector<FV_TableStyle> & fv_build()
{
	static std::vector<FV_TableStyle> v;
	if (!v.empty())
		return v;

	for (const FV_TableStyleBuiltin * b = s_tableStyleBuiltin;
		 b->id; ++b)
	{
		FV_TableStyle st;
		st.id = b->id;
		st.name = b->name;
		st.family = b->family;
		for (int k = 0; k < b->nParts; ++k)
		{
			const FV_TableStyleBuiltinPart & bp = b->parts[k];
			st.parts[bp.part].cellProps = bp.cellProps;
			st.parts[bp.part].charProps = bp.charProps;
		}
		v.push_back(st);
	}

	/* extra plain entries not in the OOXML set */
	{
		FV_TableStyle st;
		st.id = "TableGridLight";
		st.name = "Table Grid Light";
		st.family = FV_TSF_Plain;
		fv_setPart(st, FV_TSP_Whole,
				   fv_border4("solid", "7F7F7F", "0.5pt"));
		v.push_back(st);
	}
	{
		FV_TableStyle st;
		st.id = "NoStyleNoGrid";
		st.name = "No Style, No Grid";
		st.family = FV_TSF_Plain;
		fv_setPart(st, FV_TSP_Whole,
				   fv_border4("none", "auto", "0pt"));
		v.push_back(st);
	}

	return v;
}

const std::vector<FV_TableStyle> & FV_tableStyles()
{
	return fv_build();
}

const FV_TableStyle * FV_tableStyleById(const char * id)
{
	if (!id)
		return nullptr;
	for (const FV_TableStyle & st : FV_tableStyles())
		if (st.id == id)
			return &st;
	return nullptr;
}

const FV_TableStyle * FV_tableStyleByName(const char * name)
{
	if (!name)
		return nullptr;
	for (const FV_TableStyle & st : FV_tableStyles())
		if (st.name == name)
			return &st;
	return nullptr;
}

const char * FV_tableStyleFamilyName(FV_TableStyleFamily fam)
{
	switch (fam)
	{
	case FV_TSF_Plain:	return "Plain Tables";
	case FV_TSF_Grid:	return "Grid Tables";
	case FV_TSF_List:	return "List Tables";
	default:			return "";
	}
}

/* ================================================================
 * cell computation
 * ================================================================ */

FV_TableStyleCell FV_tableStyleCellProps(const FV_TableStyle & st,
										const FV_TableStyleLook & look,
										int row, int col,
										int rows, int cols)
{
	FV_TableStyleCell out;
	if (row < 0 || col < 0 || row >= rows || col >= cols)
		return out;

	/* precedence: Whole -> bands -> first/last col -> first/last row
	 * (later parts win per property, mirroring the OOXML cascade) */

	auto apply = [&](FV_TableStylePart p) {
		std::string cell = st.parts[p].cellProps;
		if (cell.find("inside") != std::string::npos)
		{
			/* insideh-* = interior horizontal edges: the cell's top
			 * (row>0) and bottom (row<rows-1); insidev-* likewise
			 * for left/right.  Only meaningful for the whole-table
			 * part - part-level inside* keys are OOXML "cancel the
			 * interior borders of my region" guards, which our
			 * per-cell model never adds anyway */
			std::string out2;
			std::vector<std::string> kv = FV_tableStyleSplitProps(cell);
			for (size_t i = 0; i + 1 < kv.size(); i += 2)
			{
				const std::string & k = kv[i];
				const std::string & val = kv[i + 1];
				std::string pre;
				if (k.compare(0, 8, "insideh-") == 0)
					pre = "insideh-";
				else if (k.compare(0, 8, "insidev-") == 0)
					pre = "insidev-";
				if (pre.empty())
				{
					out2 += k; out2 += ':'; out2 += val; out2 += ';';
					continue;
				}
				if (p != FV_TSP_Whole)
					continue;
				const char * s0 = nullptr, * s1 = nullptr;
				if (pre == "insideh-")
				{
					if (row > 0)        s0 = "top";
					if (row < rows - 1) s1 = "bot";
				}
				else
				{
					if (col > 0)        s0 = "left";
					if (col < cols - 1) s1 = "right";
				}
				std::string suf = k.substr(8);
				for (const char * s : { s0, s1 })
					if (s)
					{
						out2 += s; out2 += '-'; out2 += suf;
						out2 += ':'; out2 += val; out2 += ';';
					}
			}
			cell = out2;
		}
		out.cellProps = FV_tableStyleMergeProps(out.cellProps, cell);
		out.charProps = FV_tableStyleMergeProps(out.charProps,
											  st.parts[p].charProps);
	};

	apply(FV_TSP_Whole);

	/* band numbering runs over the body: when the header row is on,
	 * row 0 is excluded and banding starts at row 1 */
	if (look.bandRow)
	{
		int body = look.firstRow ? row - 1 : row;
		if (body >= 0)
			apply((body % 2 == 0) ? FV_TSP_Band1H : FV_TSP_Band2H);
	}
	if (look.bandCol)
	{
		int body = look.firstCol ? col - 1 : col;
		if (body >= 0)
			apply((body % 2 == 0) ? FV_TSP_Band1V : FV_TSP_Band2V);
	}

	if (look.firstCol && col == 0)
		apply(FV_TSP_FirstCol);
	if (look.lastCol && col == cols - 1)
		apply(FV_TSP_LastCol);
	if (look.firstRow && row == 0)
		apply(FV_TSP_FirstRow);
	if (look.lastRow && row == rows - 1)
		apply(FV_TSP_LastRow);

	/* resolve symbolic theme colours against the active palette */
	if (out.cellProps.find("theme:") != std::string::npos ||
		out.charProps.find("theme:") != std::string::npos)
	{
		auto resolve = [](const std::string & s) {
			std::string r;
			std::vector<std::string> kv = FV_tableStyleSplitProps(s);
			for (size_t i = 0; i + 1 < kv.size(); i += 2)
			{
				r += kv[i];
				r += ':';
				r += FV_tableStyleResolveColor(kv[i + 1]);
				r += ';';
			}
			return r;
		};
		out.cellProps = resolve(out.cellProps);
		out.charProps = resolve(out.charProps);
	}

	return out;
}

/* ================================================================
 * border presets
 * ================================================================ */

const char * FV_tableBorderPresetName(FV_TableBorderPreset p)
{
	switch (p)
	{
	case FV_TBP_None:		return "No Border";
	case FV_TBP_All:		return "All Borders";
	case FV_TBP_Outside:	return "Outside Borders";
	case FV_TBP_Inside:		return "Inside Borders";
	case FV_TBP_InsideH:	return "Inside Horizontal Border";
	case FV_TBP_InsideV:	return "Inside Vertical Border";
	case FV_TBP_Top:		return "Top Border";
	case FV_TBP_Bottom:		return "Bottom Border";
	case FV_TBP_Left:		return "Left Border";
	case FV_TBP_Right:		return "Right Border";
	default:				return "";
	}
}

std::string FV_tableBorderPresetSides(FV_TableBorderPreset preset,
									  int row, int col,
									  int rows, int cols)
{
	const bool topEdge = (row == 0);
	const bool botEdge = (row == rows - 1);
	const bool leftEdge = (col == 0);
	const bool rightEdge = (col == cols - 1);

	std::string s;
	auto add = [&s](const char * side) {
		if (!s.empty()) s += ' ';
		s += side;
	};

	switch (preset)
	{
	case FV_TBP_None:		// handled by caller as "clear all four"
		break;
	case FV_TBP_All:
		s = "top bot left right";
		break;
	case FV_TBP_Outside:
		if (topEdge) add("top");
		if (botEdge) add("bot");
		if (leftEdge) add("left");
		if (rightEdge) add("right");
		break;
	case FV_TBP_Inside:
		if (!topEdge) add("top");
		if (!botEdge) add("bot");
		if (!leftEdge) add("left");
		if (!rightEdge) add("right");
		break;
	case FV_TBP_InsideH:
		if (!topEdge) add("top");
		break;
	case FV_TBP_InsideV:
		if (!leftEdge) add("left");
		break;
	case FV_TBP_Top:
		if (topEdge) add("top");
		break;
	case FV_TBP_Bottom:
		if (botEdge) add("bot");
		break;
	case FV_TBP_Left:
		if (leftEdge) add("left");
		break;
	case FV_TBP_Right:
		if (rightEdge) add("right");
		break;
	default:
		break;
	}
	return s;
}
