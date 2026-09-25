/* -*- mode: C++; tab-width: 2; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
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

/* pure unit tests for the Table Design recipe engine - no document,
 * no GTK needed: everything under test is data + math. */

#include "tf_test.h"

#include "fl_TableStyles.h"

#include <set>

#define TFSUITE "core.text.fmt.tablestyles"

namespace {

/* does "key:value" occur as a whole prop in the "k:v; k:v" string? */
static bool propIs(const std::string & props,
				   const char * key, const char * value)
{
	std::string needle = std::string(key) + ':' + value;
	std::vector<std::string> kv = FV_tableStyleSplitProps(props);
	for (size_t i = 0; i + 1 < kv.size(); i += 2)
		if (kv[i] == key && kv[i + 1] == value)
			return true;
	(void)needle;
	return false;
}

static bool propHas(const std::string & props, const char * key)
{
	std::vector<std::string> kv = FV_tableStyleSplitProps(props);
	for (size_t i = 0; i + 1 < kv.size(); i += 2)
		if (kv[i] == key)
			return true;
	return false;
}

}

TFTEST_MAIN("fl_TableStyles")
{
	/* ---------- catalog ---------- */
	const std::vector<FV_TableStyle> & styles = FV_tableStyles();
	TFPASS(styles.size() >= 30);

	/* ids unique; every style has a name + valid family */
	{
		std::set<std::string> ids;
		for (const FV_TableStyle & st : styles)
		{
			ids.insert(st.id);
			TFPASS(!st.name.empty());
			TFPASS(st.family >= FV_TSF_Plain && st.family <= FV_TSF_List);
		}
		TFPASS((ids.size()) == (styles.size()));
	}

	/* lookups */
	const FV_TableStyle * tg = FV_tableStyleById("TableGrid");
	TFPASS(tg != nullptr);
	TFPASS((std::string(tg->name)) == (std::string("Table Grid")));
	TFPASS(FV_tableStyleByName("Table Grid") == tg);
	TFPASS(FV_tableStyleById("no-such-style") == nullptr);
	TFPASS(FV_tableStyleById(nullptr) == nullptr);
	TFPASS(FV_tableStyleByName(nullptr) == nullptr);

	/* every family has styles */
	{
		bool fam[3] = { false, false, false };
		for (const FV_TableStyle & st : styles)
			fam[st.family] = true;
		TFPASS(fam[FV_TSF_Plain] && fam[FV_TSF_Grid] && fam[FV_TSF_List]);
	}
	TFPASS(std::string(FV_tableStyleFamilyName(FV_TSF_Plain)) ==
		   std::string("Plain Tables"));
	TFPASS(std::string(FV_tableStyleFamilyName(FV_TSF_Grid)) ==
		   std::string("Grid Tables"));
	TFPASS(std::string(FV_tableStyleFamilyName(FV_TSF_List)) ==
		   std::string("List Tables"));

	/* ---------- look flags ---------- */
	FV_TableStyleLook look;	/* ctor = document defaults */
	TFPASS(look.firstRow && look.bandRow);
	TFPASS(!look.lastRow && !look.firstCol && !look.lastCol && !look.bandCol);
	TFPASS((look.toString()) == (std::string("FB")));

	{
		FV_TableStyleLook l = FV_TableStyleLook::fromString("FLf");
		TFPASS(l.firstRow && l.lastRow && l.firstCol);
		TFPASS(!l.bandRow && !l.lastCol && !l.bandCol);
	}
	{
		/* no stored value → document defaults */
		FV_TableStyleLook l = FV_TableStyleLook::fromString(nullptr);
		TFPASS(l.firstRow && l.bandRow);
		/* explicit empty → all flags off */
		FV_TableStyleLook e = FV_TableStyleLook::fromString("");
		TFPASS(!e.firstRow && !e.lastRow && !e.bandRow &&
			   !e.firstCol && !e.lastCol && !e.bandCol);
	}
	{
		/* round-trip every combination */
		FV_TableStyleLook a;
		a.lastRow = true; a.firstCol = true; a.bandCol = true;
		FV_TableStyleLook b =
			FV_TableStyleLook::fromString(a.toString().c_str());
		TFPASS(b.firstRow == a.firstRow && b.lastRow == a.lastRow &&
			   b.bandRow == a.bandRow && b.firstCol == a.firstCol &&
			   b.lastCol == a.lastCol && b.bandCol == a.bandCol);
	}

	/* ---------- plain Table Grid ---------- */
	{
		FV_TableStyleCell c =
			FV_tableStyleCellProps(*tg, look, 1, 1, 3, 3);
		TFPASS(propIs(c.cellProps, "top-style", "solid"));
		TFPASS(propIs(c.cellProps, "bot-style", "solid"));
		TFPASS(propIs(c.cellProps, "left-style", "solid"));
		TFPASS(propIs(c.cellProps, "right-style", "solid"));
		TFPASS(!propHas(c.cellProps, "background-color"));

		/* insideH/insideV translate to interior edges: corner cell
		 * has no left/top "inside" contribution but its bot/right
		 * come from them */
		FV_TableStyleCell corner =
			FV_tableStyleCellProps(*tg, look, 0, 0, 3, 3);
		TFPASS(propIs(corner.cellProps, "top-style", "solid"));
		TFPASS(propIs(corner.cellProps, "left-style", "solid"));
		TFPASS(propIs(corner.cellProps, "bot-style", "solid"));
		TFPASS(propIs(corner.cellProps, "right-style", "solid"));
	}

	/* ---------- header row conditional ---------- */
	const FV_TableStyle * g2 = FV_tableStyleById("MediumShading1-Accent1");
	TFPASS(g2 != nullptr);
	{
		FV_TableStyleCell hdr =
			FV_tableStyleCellProps(*g2, look, 0, 0, 3, 3);
		TFPASS(propIs(hdr.cellProps, "background-color", "4472C4"));
		TFPASS(hdr.charProps.find("font-weight:bold") !=
			   std::string::npos);
		TFPASS(hdr.charProps.find("color:FFFFFF") != std::string::npos);

		/* row 1 is band-0 (shaded); row 2 sits in band-1 - unshaded */
		FV_TableStyleCell body =
			FV_tableStyleCellProps(*g2, look, 2, 1, 4, 3);
		TFPASS(!propHas(body.cellProps, "background-color"));
		TFPASS(body.charProps.find("font-weight:bold") ==
			   std::string::npos);

		/* Header Row off → row 0 loses the header formatting */
		FV_TableStyleLook noHdr = look;
		noHdr.firstRow = false;
		FV_TableStyleCell c =
			FV_tableStyleCellProps(*g2, noHdr, 0, 0, 3, 3);
		TFPASS(!propIs(c.cellProps, "background-color", "4472C4"));
		TFPASS(c.charProps.find("font-weight:bold") == std::string::npos);
	}

	/* ---------- banded rows ---------- */
	const FV_TableStyle * g3 = FV_tableStyleById("MediumShading1-Accent1");
	TFPASS(g3 != nullptr);
	{
		const std::string tint = FV_tableStyleTint("4472C4", 0.75);
		/* header on → body row 1 is band 0 (Band1H), body row 2 band 1 */
		FV_TableStyleCell b1 =
			FV_tableStyleCellProps(*g3, look, 1, 1, 4, 3);
		FV_TableStyleCell b2 =
			FV_tableStyleCellProps(*g3, look, 2, 1, 4, 3);
		TFPASS(propIs(b1.cellProps, "background-color", tint.c_str()));
		TFPASS(!propIs(b2.cellProps, "background-color", tint.c_str()));

		/* Banded Rows off → no band shading anywhere */
		FV_TableStyleLook nb = look;
		nb.bandRow = false;
		FV_TableStyleCell c =
			FV_tableStyleCellProps(*g3, nb, 1, 1, 4, 3);
		TFPASS(!propIs(c.cellProps, "background-color", tint.c_str()));
	}

	/* ---------- banded columns ---------- */
	const FV_TableStyle * g4 = FV_tableStyleById("MediumShading1-Accent1");
	TFPASS(g4 != nullptr);
	{
		const std::string tint = FV_tableStyleTint("4472C4", 0.75);
		FV_TableStyleLook cb = look;
		cb.bandCol = true;
		cb.bandRow = false;	/* isolate column banding */
		/* firstCol off → col 0 is band 0 */
		FV_TableStyleCell c0 =
			FV_tableStyleCellProps(*g4, cb, 1, 0, 3, 3);
		FV_TableStyleCell c1 =
			FV_tableStyleCellProps(*g4, cb, 1, 1, 3, 3);
		TFPASS(propIs(c0.cellProps, "background-color", tint.c_str()));
		TFPASS(!propIs(c1.cellProps, "background-color", tint.c_str()));
	}

	/* ---------- list table: horizontal rules only ---------- */
	const FV_TableStyle * lt2 =
		FV_tableStyleById("MediumList1-Accent1");
	TFPASS(lt2 != nullptr);
	{
		FV_TableStyleCell c =
			FV_tableStyleCellProps(*lt2, look, 2, 1, 4, 3);
		/* horizontal rules only - no vertical border props */
		TFPASS(propIs(c.cellProps, "top-style", "solid"));
		TFPASS(propIs(c.cellProps, "bot-style", "solid"));
		TFPASS(!propHas(c.cellProps, "left-style"));
		TFPASS(!propHas(c.cellProps, "right-style"));
	}

	/* ---------- first/last column emphasis ---------- */
	const FV_TableStyle * lt3 =
		FV_tableStyleById("LightList-Accent1");
	TFPASS(lt3 != nullptr);
	{
		FV_TableStyleLook cols = look;
		cols.firstCol = true;
		cols.lastCol = true;
		FV_TableStyleCell f =
			FV_tableStyleCellProps(*lt3, cols, 1, 0, 3, 3);
		FV_TableStyleCell mid =
			FV_tableStyleCellProps(*lt3, cols, 1, 1, 3, 3);
		FV_TableStyleCell l =
			FV_tableStyleCellProps(*lt3, cols, 1, 2, 3, 3);
		TFPASS(f.charProps.find("font-weight:bold") != std::string::npos);
		TFPASS(mid.charProps.find("font-weight:bold") == std::string::npos);
		TFPASS(l.charProps.find("font-weight:bold") != std::string::npos);

		/* flag off → emphasis gone */
		FV_TableStyleCell f2 =
			FV_tableStyleCellProps(*lt3, look, 1, 0, 3, 3);
		TFPASS(f2.charProps.find("font-weight:bold") == std::string::npos);
	}

	/* ---------- edge cases ---------- */
	{
		/* out-of-range cell → empty result */
		FV_TableStyleCell o =
			FV_tableStyleCellProps(*g3, look, 5, 0, 3, 3);
		TFPASS(o.cellProps.empty() && o.charProps.empty());
		o = FV_tableStyleCellProps(*g3, look, -1, 0, 3, 3);
		TFPASS(o.cellProps.empty() && o.charProps.empty());

		/* 1x1: row is both first and last; later part wins */
		FV_TableStyleLook both = look;
		both.lastRow = true;
		const FV_TableStyle * g1 =
			FV_tableStyleById("LightShading-Accent1");
		TFPASS(g1 != nullptr);
		FV_TableStyleCell c =
			FV_tableStyleCellProps(*g1, both, 0, 0, 1, 1);
		TFPASS(!c.cellProps.empty());
	}

	/* ---------- merge / split helpers ---------- */
	{
		std::string mg = FV_tableStyleMergeProps("a:1; b:2", "b:3; c:4");
		TFPASS((mg) == (std::string("a:1; b:3; c:4")));
		TFPASS((FV_tableStyleMergeProps("a:1", "")) == (std::string("a:1")));
		TFPASS((FV_tableStyleMergeProps("", "b:2")) == (std::string("b:2")));

		std::vector<std::string> kv =
			FV_tableStyleSplitProps("x: 1 ; y:2;;");
		TFPASS((kv.size()) == (static_cast<size_t>(4)));
		TFPASS((kv[0]) == (std::string("x")));
		TFPASS((kv[1]) == (std::string("1")));
		TFPASS((kv[2]) == (std::string("y")));
		TFPASS((kv[3]) == (std::string("2")));
	}

	/* ---------- border presets ---------- */
	TFPASS((FV_tableBorderPresetSides(FV_TBP_All, 1, 1, 3, 3)) == (std::string("top bot left right")));
	TFPASS((FV_tableBorderPresetSides(FV_TBP_Outside, 0, 0, 3, 3)) == (std::string("top left")));
	TFPASS((FV_tableBorderPresetSides(FV_TBP_Outside, 2, 2, 3, 3)) == (std::string("bot right")));
	TFPASS((FV_tableBorderPresetSides(FV_TBP_Outside, 1, 1, 3, 3)) == (std::string("")));
	TFPASS((FV_tableBorderPresetSides(FV_TBP_Inside, 1, 1, 3, 3)) == (std::string("top bot left right")));
	TFPASS((FV_tableBorderPresetSides(FV_TBP_Inside, 0, 0, 3, 3)) == (std::string("bot right")));
	TFPASS((FV_tableBorderPresetSides(FV_TBP_InsideH, 0, 0, 3, 3)) == (std::string("")));
	TFPASS((FV_tableBorderPresetSides(FV_TBP_InsideH, 1, 0, 3, 3)) == (std::string("top")));
	TFPASS((FV_tableBorderPresetSides(FV_TBP_InsideV, 0, 1, 3, 3)) == (std::string("left")));
	TFPASS((FV_tableBorderPresetSides(FV_TBP_InsideV, 0, 0, 3, 3)) == (std::string("")));
	TFPASS((FV_tableBorderPresetSides(FV_TBP_Top, 0, 1, 3, 3)) == (std::string("top")));
	TFPASS((FV_tableBorderPresetSides(FV_TBP_Top, 1, 1, 3, 3)) == (std::string("")));
	TFPASS((FV_tableBorderPresetSides(FV_TBP_Bottom, 2, 1, 3, 3)) == (std::string("bot")));
	TFPASS((FV_tableBorderPresetSides(FV_TBP_None, 1, 1, 3, 3)) == (std::string("")));
	/* degenerate tables */
	TFPASS((FV_tableBorderPresetSides(FV_TBP_Outside, 0, 0, 1, 1)) == (std::string("top bot left right")));
	TFPASS((FV_tableBorderPresetSides(FV_TBP_Inside, 0, 0, 1, 1)) == (std::string("")));

	TFPASS(std::string(FV_tableBorderPresetName(FV_TBP_All)) ==
		   std::string("All Borders"));
	TFPASS(std::string(FV_tableBorderPresetName(FV_TBP_InsideH)) ==
		   std::string("Inside Horizontal Border"));

	/* ---------- pen defaults + colour helpers ---------- */
	{
		FV_TablePen pen;
		TFPASS((pen.style) == (std::string("solid")));
		TFPASS((pen.color) == (std::string("000000")));
		TFPASS((pen.thickness) == (std::string("0.5pt")));

		TFPASS((FV_tableStyleTint("000000", 1.0)) == (std::string("FFFFFF")));
		TFPASS((FV_tableStyleTint("000000", 0.0)) == (std::string("000000")));
		TFPASS((FV_tableStyleShade("FFFFFF", 1.0)) == (std::string("000000")));
		/* malformed input must not crash */
		TFPASS((FV_tableStyleTint("F", 0.5)) == (std::string("808080")));
	}

	/* ---------- theme colour tokens ---------- */
	{
		TFPASS((FV_tableStyleResolveColor("theme:accent1")) ==
			   (std::string("4472C4")));
		TFPASS((FV_tableStyleResolveColor("theme:text1")) ==
			   (std::string("000000")));
		TFPASS((FV_tableStyleResolveColor("theme:background1")) ==
			   (std::string("FFFFFF")));
		TFPASS((FV_tableStyleResolveColor("auto")) ==
			   (std::string("auto")));
		TFPASS((FV_tableStyleResolveColor("FF8800")) ==
			   (std::string("FF8800")));
		/* tint token = accent1 blended toward white */
		std::string tinted = FV_tableStyleResolveColor("theme:accent1:t25");
		TFPASS(tinted == FV_tableStyleTint("4472C4", 0.75));
		/* unknown palette entry resolves empty */
		TFPASS((FV_tableStyleResolveColor("theme:nope")) ==
			   (std::string("")));

		/* palette override recolours theme styles */
		FV_setTableStyleThemeColor("accent1", "FF0000");
		TFPASS((FV_tableStyleResolveColor("theme:accent1")) ==
			   (std::string("FF0000")));
		const FV_TableStyle * ms =
			FV_tableStyleById("MediumShading1-Accent1");
		FV_TableStyleCell hc =
			FV_tableStyleCellProps(*ms, look, 0, 0, 3, 3);
		TFPASS(propIs(hc.cellProps, "background-color", "FF0000"));
		FV_setTableStyleThemeColor("accent1", nullptr);
		TFPASS((FV_tableStyleResolveColor("theme:accent1")) ==
			   (std::string("4472C4")));
	}
}
