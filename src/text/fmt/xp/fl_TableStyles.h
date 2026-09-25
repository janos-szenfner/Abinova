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

#pragma once

#include <string>
#include <vector>

/**
 * Table Design recipes (Table Styles), modelled on the OOXML
 * tblStyle/tblLook scheme.
 *
 * A style is a set of *conditional parts* (whole table, first/last
 * row, first/last column, row/column bands).  Which parts apply to a
 * given cell is driven by the six Table Style Options flags (the
 * "tbl look").  Everything here is pure data + math: no GTK, no
 * piece-table access, so it is fully unit-testable.  The FV_View
 * glue in fv_View_tableStyle.cpp turns the computed property
 * strings into cell strux changes.
 */

/* the six Table Style Options checkboxes */
struct FV_TableStyleLook
{
	bool	firstRow;		// Header Row
	bool	lastRow;		// Total Row
	bool	bandRow;		// Banded Rows
	bool	firstCol;		// First Column
	bool	lastCol;		// Last Column
	bool	bandCol;		// Banded Columns

	FV_TableStyleLook()
		: firstRow(true), lastRow(false), bandRow(true),
		  firstCol(true), lastCol(false), bandCol(false) {}

	/* compact on-strux form: "F L B f l b" letters present = ON,
	 * e.g. "FBf" = header row + banded rows + first column, the
	 * default (matches OOXML's shipped tblLook val="04A0") */
	std::string toString() const;
	static FV_TableStyleLook fromString(const char * s);
};

/* conditional part indices - mirror the OOXML tblStylePr types */
enum FV_TableStylePart : int
{
	FV_TSP_Whole = 0,
	FV_TSP_FirstRow,
	FV_TSP_LastRow,
	FV_TSP_FirstCol,
	FV_TSP_LastCol,
	FV_TSP_Band1H,
	FV_TSP_Band2H,
	FV_TSP_Band1V,
	FV_TSP_Band2V,
	FV_TSP__COUNT
};

enum FV_TableStyleFamily : int
{
	FV_TSF_Plain = 0,
	FV_TSF_Grid,
	FV_TSF_List,
	FV_TSF__COUNT
};

struct FV_TableStylePartDef
{
	std::string	cellProps;	/* "k:v; k:v" cell borders/shading, may be "" */
	std::string	charProps;	/* optional char fmt for the cell range, "" */
};

struct FV_TableStyle
{
	std::string				id;			/* "grid-table-3-accent-1" */
	std::string				name;		/* "Grid Table 3 Accent 1" */
	FV_TableStyleFamily		family;
	FV_TableStylePartDef	parts[FV_TSP__COUNT];
	bool					gallery = true;	/* shown in the style gallery */
};

/* merged result for one cell */
struct FV_TableStyleCell
{
	std::string	cellProps;	/* may be empty */
	std::string	charProps;	/* may be empty */
};

const std::vector<FV_TableStyle> & FV_tableStyles();
const FV_TableStyle * FV_tableStyleById(const char * id);
const FV_TableStyle * FV_tableStyleByName(const char * name);
const char * FV_tableStyleFamilyName(FV_TableStyleFamily fam);

/* compute the merged cell+char props for cell (row,col) of a
 * (rows x cols) table under the given look flags */
FV_TableStyleCell FV_tableStyleCellProps(const FV_TableStyle & st,
										const FV_TableStyleLook & look,
										int row, int col,
										int rows, int cols);

/* ---- border presets (the Borders dropdown) ---- */

enum FV_TableBorderPreset : int
{
	FV_TBP_None = 0,		/* remove all borders */
	FV_TBP_All,
	FV_TBP_Outside,
	FV_TBP_Inside,
	FV_TBP_InsideH,
	FV_TBP_InsideV,
	FV_TBP_Top,
	FV_TBP_Bottom,
	FV_TBP_Left,
	FV_TBP_Right,
	FV_TBP__COUNT
};

struct FV_TablePen
{
	std::string style;		/* solid | dashed | dotted | double | none */
	std::string color;		/* "4472C4" or "auto" */
	std::string thickness;	/* "0.5pt", "1pt", ... */

	FV_TablePen() : style("solid"), color("000000"), thickness("0.5pt") {}
};

const char * FV_tableBorderPresetName(FV_TableBorderPreset p);

/* which of the four borders of cell (row,col) receive the pen
 * under a preset: returns a string like "top bot" listing the sides,
 * or "" when the cell is untouched */
std::string FV_tableBorderPresetSides(FV_TableBorderPreset preset,
									  int row, int col,
									  int rows, int cols);

/* merge helper: overlay "k:v; k:v" strings (later wins per key) */
std::string FV_tableStyleMergeProps(const std::string & base,
									const std::string & over);

/* split "k:v; k:v" into a flat name,value,name,value list */
std::vector<std::string> FV_tableStyleSplitProps(const std::string & s);

/* small colour helpers used by the recipe generator */
std::string FV_tableStyleTint(const std::string & hex, double pct);
std::string FV_tableStyleShade(const std::string & hex, double pct);

/* ---- theme colours ----
 * Recipe colour values may be literal "RRGGBB"/"auto" or a symbolic
 * theme token "theme:<name>" optionally followed by ":tNN" (keep NN%
 * of the colour, blend the rest toward white) or ":sNN" (keep NN% of
 * the colour, i.e. scale toward black to NN% - OOXML themeShade).
 * They resolve through the active theme palette - the
 * Office default until a document theme overrides it. */
std::string FV_tableStyleResolveColor(const std::string & tok);

/* override one palette entry ("accent1".."accent6", "text1/2",
 * "background1/2"); nullptr resets to defaults.  Re-applying a
 * style afterwards picks the new colours - explicit RGB recipes
 * are unaffected. */
void FV_setTableStyleThemeColor(const char * name, const char * hex);

/* generated built-in recipe row (fl_TableStylesBuiltin.cpp) */
struct FV_TableStyleBuiltinPart
{
	FV_TableStylePart	part;
	const char *		cellProps;
	const char *		charProps;
};
struct FV_TableStyleBuiltin
{
	const char *				id;			/* OOXML styleId */
	const char *				name;
	FV_TableStyleFamily			family;
	int							nParts;
	FV_TableStyleBuiltinPart	parts[16];
};

/* built-in recipes kept for document compatibility but not shown
 * in the gallery (older-generation and Word-2003 styles) */
extern const FV_TableStyleBuiltin s_tableStyleBuiltinHidden[];
