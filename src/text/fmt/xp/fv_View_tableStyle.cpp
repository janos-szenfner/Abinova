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

/**
 * FV_View glue for the Table Design ribbon tab: applies
 * fl_TableStyles recipes to the piece table, stores the active
 * style id + look flags as attributes on the table strux, and
 * implements border presets, cell shading and hover preview.
 */

#include "fv_View.h"
#include "fv_ViewDoubleBuffering.h"
#include "fl_TableStyles.h"
#include "pd_Document.h"
#include "pt_Types.h"
#include "pp_AttrProp.h"
#include "pf_Frag.h"
#include "pf_Frag_Strux.h"
#include "fl_TableLayout.h"
#include "fl_BlockLayout.h"
#include "fl_ContainerLayout.h"
#include "fp_TableContainer.h"
#include "fp_Page.h"
#include "xap_App.h"
#include "ut_debugmsg.h"

/* attributes stored on the table strux */
#define ABINOVA_TBL_STYLE_ATTR	"tbl-style"
#define ABINOVA_TBL_LOOK_ATTR	"tbl-look"

/* the 13 cell property names a style or border preset can own */
static const char * const s_cellStyleKeys[] = {
	"top-style", "top-color", "top-thickness",
	"bot-style", "bot-color", "bot-thickness",
	"left-style", "left-color", "left-thickness",
	"right-style", "right-color", "right-thickness",
	"background-color"
};

/* border-pen state for the Border Styles / Pen Colour controls;
 * process-wide like Word's "current pen" */
static FV_TablePen s_tablePen;

namespace {

struct FV_TblCell
{
	const pf_Frag_Strux *	sdh;
	PT_DocPosition			pos;
	PT_DocPosition			posEndCell;
	UT_sint32				row, col, rowspan, colspan;
};

struct FV_TblCells
{
	const pf_Frag_Strux *	tableSDH;
	PT_DocPosition			posTable;
	PT_DocPosition			posEnd;
	std::vector<FV_TblCell>	cells;
	UT_sint32				rows, cols;
};

static bool fv_snapshotAP(PD_Document * doc, const pf_Frag_Strux * sdh,
						  PP_PropertyVector & attrs, PP_PropertyVector & props)
{
	const PP_AttrProp * pAP = nullptr;
	if (!doc->getAttrProp(doc->getAPIFromStrux(sdh), &pAP) || !pAP)
		return false;
	const gchar * name = nullptr;
	const gchar * val = nullptr;
	for (UT_uint32 k = 0; pAP->getNthAttribute(k, name, val); ++k)
	{
		attrs.push_back(name);
		attrs.push_back(val);
	}
	for (UT_uint32 k = 0; pAP->getNthProperty(k, name, val); ++k)
	{
		props.push_back(name);
		props.push_back(val);
	}
	return true;
}

static UT_sint32 fv_attachInt(FV_View * view, PD_Document * doc,
							  const pf_Frag_Strux * sdh,
							  const char * att, UT_sint32 dflt)
{
	const gchar * v = nullptr;
	if (doc->getPropertyFromStrux(sdh, view->isShowRevisions(),
								view->getRevisionLevel(), att,
								reinterpret_cast<const char**>(&v)) && v)
		return atoi(v);
	return dflt;
}

} // anonymous namespace

/* ---- find the table at the caret and enumerate its cells ---- */

/* the table the caret/selection refers to: inside the caret's table,
 * inside the anchor's table (whole-cell selections sit the anchor on
 * the strux itself), or the first table fully inside the selection
 * range (select-all / drags spanning the whole table) */
static const pf_Frag_Strux * fv_tableSDH(FV_View * view, PD_Document * doc)
{
	const pf_Frag_Strux * tableSDH = nullptr;
	if (doc->getStruxOfTypeFromPosition(view->getPoint(),
									  PTX_SectionTable, &tableSDH) &&
		tableSDH)
		return tableSDH;
	if (view->isSelectionEmpty())
		return nullptr;
	PT_DocPosition posA = view->getSelectionAnchor();
	PT_DocPosition posB = view->getPoint();
	if (doc->getStruxOfTypeFromPosition(posA, PTX_SectionTable,
									  &tableSDH) && tableSDH)
		return tableSDH;

	PT_DocPosition posStart = UT_MIN(posA, posB);
	PT_DocPosition posEnd = UT_MAX(posA, posB);
	if (posEnd <= posStart)
		return nullptr;
	/* neither endpoint sits inside a table (partial overlaps were
	 * caught above), so the only hit left is a table fully inside
	 * the selection: scan forward from the section strux */
	const pf_Frag_Strux * sdh = nullptr;
	if (!doc->getStruxOfTypeFromPosition(posStart, PTX_Section, &sdh) ||
		!sdh)
		return nullptr;
	const pf_Frag_Strux * tab = nullptr;
	while (doc->getNextStruxOfType(sdh, PTX_SectionTable, &tab) && tab)
	{
		PT_DocPosition posTab = doc->getStruxPosition(tab);
		if (posTab >= posEnd)
			break;
		if (posTab >= posStart)
			return tab;
		sdh = tab;
	}
	return nullptr;
}

static bool fv_enumTable(FV_View * view, PD_Document * doc,
						 FV_TblCells & out)
{
	UT_return_val_if_fail(view && doc, false);
	const pf_Frag_Strux * tableSDH = fv_tableSDH(view, doc);
	if (!tableSDH)
		return false;

	out.tableSDH = tableSDH;
	out.posTable = doc->getStruxPosition(tableSDH);
	const pf_Frag_Strux * endSDH = doc->getEndTableStruxFromTableStrux(tableSDH);
	if (!endSDH)
		return false;
	out.posEnd = doc->getStruxPosition(endSDH);
	out.rows = 0;
	out.cols = 0;
	out.cells.clear();

	/* collect every cell strux between the table markers; row/col
	 * come from the *-attach attributes (grid coordinates) */
	for (pf_Frag * pf = const_cast<pf_Frag_Strux*>(tableSDH)->getNext();
		 pf && pf != endSDH; pf = pf->getNext())
	{
		if (pf->getType() != pf_Frag::PFT_Strux)
			continue;
		const pf_Frag_Strux * sdh = static_cast<const pf_Frag_Strux*>(pf);
		if (sdh->getStruxType() == PTX_SectionTable)
		{
			/* nested table inside a cell: its cells belong to the
			 * inner table, not to us - skip to its end strux */
			const pf_Frag_Strux * nestEnd =
				doc->getEndTableStruxFromTableStrux(sdh);
			if (nestEnd)
			{
				pf = const_cast<pf_Frag_Strux*>(nestEnd);
				continue;
			}
		}
		if (sdh->getStruxType() != PTX_SectionCell)
			continue;

		FV_TblCell c;
		c.sdh = sdh;
		c.pos = doc->getStruxPosition(sdh);
		/* snapshot the end position now: strux pointers can go stale
		 * across piece-table mutations, positions cannot */
		const pf_Frag_Strux * endCell =
			doc->getEndCellStruxFromCellStrux(sdh);
		c.posEndCell = endCell ? doc->getStruxPosition(endCell) : 0;
		c.row = fv_attachInt(view, doc, sdh, "top-attach", 0);
		c.col = fv_attachInt(view, doc, sdh, "left-attach", 0);
		UT_sint32 bot = fv_attachInt(view, doc, sdh, "bot-attach", c.row + 1);
		UT_sint32 right = fv_attachInt(view, doc, sdh, "right-attach", c.col + 1);
		c.rowspan = bot - c.row;
		c.colspan = right - c.col;
		out.cells.push_back(c);
		out.rows = UT_MAX(out.rows, bot);
		out.cols = UT_MAX(out.cols, right);
	}
	return !out.cells.empty();
}

static FV_TableStyleLook fv_currentLook(const std::string & stored)
{
	return FV_TableStyleLook::fromString(
		stored.empty() ? nullptr : stored.c_str());
}

static PP_PropertyVector fv_propsVec(const std::string & s)
{
	PP_PropertyVector v;
	for (const std::string & kv : FV_tableStyleSplitProps(s))
		v.push_back(kv);
	return v;
}

static void fv_writeTableProps(PD_Document * doc,
							   const FV_TblCells & t,
							   const char * styleId,
							   const FV_TableStyleLook & look,
							   bool noUndo)
{
	PP_PropertyVector props;
	if (styleId && *styleId)
	{
		props.push_back(ABINOVA_TBL_STYLE_ATTR);
		props.push_back(styleId);
	}
	props.push_back(ABINOVA_TBL_LOOK_ATTR);
	props.push_back(look.toString());
	if (noUndo)
		doc->changeStruxFmtNoUndo(PTC_SetFmt, const_cast<pf_Frag_Strux*>(t.tableSDH),
								  PP_NOPROPS, props);
	else
		doc->changeStruxFmt(PTC_AddFmt, t.posTable, t.posTable,
							PP_NOPROPS, props, PTX_SectionTable);
}

static void fv_applyStyleCells(PD_Document * doc,
							   const FV_TblCells & t,
							   const FV_TableStyle & st,
							   const FV_TableStyleLook & look,
							   bool noUndo)
{
	for (const FV_TblCell & c : t.cells)
	{
		/* evaluate at the cell's top-left attach; span cells take the
		 * style of their anchor square */
		FV_TableStyleCell cp = FV_tableStyleCellProps(
			st, look, c.row, c.col, t.rows, t.cols);
		if (cp.cellProps.empty())
			continue;
		PP_PropertyVector props = fv_propsVec(cp.cellProps);
		if (noUndo)
			doc->changeStruxFmtNoUndo(PTC_AddFmt, const_cast<pf_Frag_Strux*>(c.sdh),
									  PP_NOPROPS, props);
		else
			doc->changeStruxFmt(PTC_AddFmt, c.pos + 1, c.pos + 1,
								PP_NOPROPS, props, PTX_SectionCell);

		/* char props only on commit - in preview (noUndo) they would
		 * leak: span fmt has no snapshot/restore path here and the
		 * previewEnd cell restore can't undo them (white text on
		 * white cells was the symptom) */
		if (!noUndo && !cp.charProps.empty() && c.posEndCell > c.pos)
		{
			PP_PropertyVector cprops = fv_propsVec(cp.charProps);
			doc->changeSpanFmt(PTC_AddFmt, c.pos + 1, c.posEndCell,
							   PP_NOPROPS, cprops);
		}
	}
}

/* strip every style-owned key (cell borders/fill + char fmt) so a
 * previous style leaves no residue when re-styling or clearing.
 * RemoveFmt vectors are name/value PAIRS (name, "") - mergeAP walks
 * them two entries at a time, a bare-name list runs the iterator
 * past end() and crashes */
static void fv_clearStyleProps(PD_Document * doc, const FV_TblCells & t)
{
	PP_PropertyVector removeProps;
	for (const char * k : s_cellStyleKeys)
	{
		removeProps.push_back(k);
		removeProps.push_back("");
	}
	for (const FV_TblCell & c : t.cells)
	{
		doc->changeStruxFmt(PTC_RemoveFmt, c.pos + 1, c.pos + 1,
							PP_NOPROPS, removeProps, PTX_SectionCell);
		if (c.posEndCell > c.pos)
		{
			PP_PropertyVector rm = { "font-weight", "",
									 "font-style", "",
									 "color", "" };
			doc->changeSpanFmt(PTC_RemoveFmt, c.pos + 1, c.posEndCell,
							   PP_NOPROPS, rm);
		}
	}
}

/* ================================================================
 * public commands
 * ================================================================ */

bool FV_View::cmdTableSetStyle(const char * szStyleId)
{
	UT_return_val_if_fail(szStyleId, false);
	const FV_TableStyle * st = FV_tableStyleById(szStyleId);
	UT_return_val_if_fail(st, false);

	FV_TblCells t;
	if (!fv_enumTable(this, m_pDoc, t))
		return false;

	FV_TableStyleLook look = fv_currentLook(getTableStyleLook());

	_changeCellParams(t.posTable, t.tableSDH);
	fv_writeTableProps(m_pDoc, t, st->id.c_str(), look, false);
	/* clear leftovers from a previous style first: cells where the
	 * new recipe defines nothing must return to the default, not
	 * keep the old style's fills/borders/text colour */
	fv_clearStyleProps(m_pDoc, t);
	fv_applyStyleCells(m_pDoc, t, *st, look, false);

	/* paired with _changeCellParams above: decrements the table
	 * wait-index, re-enables layout/lists, updates dirty lists and
	 * closes the atomic glob it opened */
	_restoreCellParams(t.posTable, t.tableSDH);
	_generalUpdate();
	return true;
}

bool FV_View::cmdTableClearStyle()
{
	FV_TblCells t;
	if (!fv_enumTable(this, m_pDoc, t))
		return false;

	_changeCellParams(t.posTable, t.tableSDH);

	/* drop every style-owned cell prop + char fmt; border state
	 * returns to the document default */
	fv_clearStyleProps(m_pDoc, t);

	/* clear the stored style + look */
	PP_PropertyVector rmProps = { ABINOVA_TBL_STYLE_ATTR, "",
								ABINOVA_TBL_LOOK_ATTR, "" };
	m_pDoc->changeStruxFmt(PTC_RemoveFmt, t.posTable, t.posTable,
						   PP_NOPROPS, rmProps, PTX_SectionTable);

	/* paired with _changeCellParams above: decrements the table
	 * wait-index, re-enables layout/lists, updates dirty lists and
	 * closes the atomic glob it opened */
	_restoreCellParams(t.posTable, t.tableSDH);
	_generalUpdate();
	return true;
}

bool FV_View::cmdTableSetStyleOption(UT_sint32 iOption, bool bOn)
{
	FV_TblCells t;
	if (!fv_enumTable(this, m_pDoc, t))
		return false;

	FV_TableStyleLook look = fv_currentLook(getTableStyleLook());
	switch (iOption)
	{
	case 0: look.firstRow = bOn; break;
	case 1: look.lastRow  = bOn; break;
	case 2: look.bandRow  = bOn; break;
	case 3: look.firstCol = bOn; break;
	case 4: look.lastCol  = bOn; break;
	case 5: look.bandCol  = bOn; break;
	default: UT_return_val_if_fail(0, false);
	}

	std::string styleId = getTableStyleId();

	_changeCellParams(t.posTable, t.tableSDH);
	fv_writeTableProps(m_pDoc, t,
					   styleId.empty() ? nullptr : styleId.c_str(),
					   look, false);

	/* re-render through the active style so toggles take effect
	 * immediately; the style-owned keys are first stripped so stale
	 * band/colouring doesn't linger */
	if (!styleId.empty())
	{
		const FV_TableStyle * st = FV_tableStyleById(styleId.c_str());
		if (st)
		{
			fv_clearStyleProps(m_pDoc, t);
			fv_applyStyleCells(m_pDoc, t, *st, look, false);
		}
	}

	/* paired with _changeCellParams above: decrements the table
	 * wait-index, re-enables layout/lists, updates dirty lists and
	 * closes the atomic glob it opened */
	_restoreCellParams(t.posTable, t.tableSDH);
	_generalUpdate();
	return true;
}

bool FV_View::cmdTableBorderPreset(UT_sint32 iPreset)
{
	FV_TableBorderPreset preset = static_cast<FV_TableBorderPreset>(iPreset);
	if (iPreset < 0 || iPreset >= FV_TBP__COUNT)
		return false;

	FV_TblCells t;
	if (!fv_enumTable(this, m_pDoc, t))
		return false;

	_changeCellParams(t.posTable, t.tableSDH);

	for (const FV_TblCell & c : t.cells)
	{
		PP_PropertyVector props;
		if (preset == FV_TBP_None)
		{
			static const char * sides[] = { "top", "bot", "left", "right" };
			for (const char * s : sides)
			{
				props.push_back(std::string(s) + "-style");
				props.push_back("none");
				props.push_back(std::string(s) + "-thickness");
				props.push_back("0pt");
			}
		}
		else
		{
			std::string sides = FV_tableBorderPresetSides(
				preset, c.row, c.col, t.rows, t.cols);
			size_t pos = 0;
			while (pos < sides.size())
			{
				size_t sp = sides.find(' ', pos);
				std::string side = sides.substr(pos,
					sp == std::string::npos ? std::string::npos : sp - pos);
				pos = (sp == std::string::npos) ? sides.size() : sp + 1;
				if (side.empty())
					continue;
				props.push_back(side + "-style");
				props.push_back(s_tablePen.style);
				props.push_back(side + "-color");
				props.push_back(s_tablePen.color);
				props.push_back(side + "-thickness");
				props.push_back(s_tablePen.thickness);
			}
		}
		if (!props.empty())
			m_pDoc->changeStruxFmt(PTC_AddFmt, c.pos + 1, c.pos + 1,
								   PP_NOPROPS, props, PTX_SectionCell);
	}

	/* paired with _changeCellParams above: decrements the table
	 * wait-index, re-enables layout/lists, updates dirty lists and
	 * closes the atomic glob it opened */
	_restoreCellParams(t.posTable, t.tableSDH);
	_generalUpdate();
	return true;
}

bool FV_View::cmdTableCellShading(const char * szColor)
{
	UT_return_val_if_fail(szColor && *szColor, false);
	const PP_PropertyVector props = { "background-color", szColor };
	/* proven Format-Table path: bumps the table change index and
	 * handles the selected-cells range */
	return setCellFormat(props, FORMAT_TABLE_SELECTION, nullptr, "");
}

void FV_View::setTablePen(const char * szStyle, const char * szThickness,
						  const char * szColor)
{
	if (szStyle && *szStyle)
		s_tablePen.style = szStyle;
	if (szThickness && *szThickness)
		s_tablePen.thickness = szThickness;
	if (szColor && *szColor)
		s_tablePen.color = szColor;
}

bool FV_View::getTablePen(std::string & sStyle, std::string & sThickness,
						  std::string & sColor) const
{
	sStyle = s_tablePen.style;
	sThickness = s_tablePen.thickness;
	sColor = s_tablePen.color;
	return true;
}

std::string FV_View::getTableStyleId() const
{
	const pf_Frag_Strux * tableSDH =
		fv_tableSDH(const_cast<FV_View*>(this), m_pDoc);
	if (!tableSDH)
		return "";
	const gchar * v = nullptr;
	if (m_pDoc->getPropertyFromStrux(tableSDH, isShowRevisions(),
								   getRevisionLevel(), ABINOVA_TBL_STYLE_ATTR,
								   reinterpret_cast<const char**>(&v)) && v)
		return v;
	return "";
}

std::string FV_View::getTableStyleLook() const
{
	const pf_Frag_Strux * tableSDH =
		fv_tableSDH(const_cast<FV_View*>(this), m_pDoc);
	if (!tableSDH)
		return "";
	const gchar * v = nullptr;
	if (m_pDoc->getPropertyFromStrux(tableSDH, isShowRevisions(),
								   getRevisionLevel(), ABINOVA_TBL_LOOK_ATTR,
								   reinterpret_cast<const char**>(&v)) && v)
		return v;
	return "";
}

/* ---- hover preview: apply with no undo, restore on leave ---- */

namespace {
struct FV_TSPreview
{
	bool				active = false;
	const pf_Frag_Strux * tableSDH = nullptr;
	std::string			oldStyleId;
	std::string			oldLook;
	struct SavedCell {
		const pf_Frag_Strux *	sdh;
		PP_PropertyVector		attrs;
		PP_PropertyVector		props;
	};
	std::vector<SavedCell>	cells;
} s_preview;
}

bool FV_View::cmdTableStylePreviewBegin(const char * szStyleId)
{
	UT_return_val_if_fail(szStyleId, false);
	const FV_TableStyle * st = FV_tableStyleById(szStyleId);
	UT_return_val_if_fail(st, false);

	/* a preview already running: restore first, then re-preview */
	if (s_preview.active)
		cmdTableStylePreviewEnd();

	FV_TblCells t;
	if (!fv_enumTable(this, m_pDoc, t))
		return false;

	s_preview.active = true;
	s_preview.tableSDH = t.tableSDH;
	s_preview.oldStyleId = getTableStyleId();
	s_preview.oldLook = getTableStyleLook();
	s_preview.cells.clear();
	s_preview.cells.reserve(t.cells.size());
	for (const FV_TblCell & c : t.cells)
	{
		FV_TSPreview::SavedCell sc;
		sc.sdh = c.sdh;
		if (fv_snapshotAP(m_pDoc, c.sdh, sc.attrs, sc.props))
			s_preview.cells.push_back(sc);
	}

	FV_TableStyleLook look = fv_currentLook(s_preview.oldLook);

	m_pDoc->setDontImmediatelyLayout(true);
	fv_writeTableProps(m_pDoc, t, st->id.c_str(), look, true);
	fv_applyStyleCells(m_pDoc, t, *st, look, true);
	m_pDoc->setDontImmediatelyLayout(false);
	_generalUpdate();
	return true;
}

void FV_View::cmdTableStylePreviewEnd()
{
	if (!s_preview.active)
		return;
	s_preview.active = false;

	m_pDoc->setDontImmediatelyLayout(true);
	for (const FV_TSPreview::SavedCell & sc : s_preview.cells)
	{
		if (!sc.sdh)
			continue;
		m_pDoc->changeStruxFmtNoUndo(PTC_SetExactly,
									 const_cast<pf_Frag_Strux*>(sc.sdh),
									 sc.attrs, sc.props);
	}
	const pf_Frag_Strux * tableSDH = s_preview.tableSDH;
	s_preview.cells.clear();
	s_preview.tableSDH = nullptr;
	if (tableSDH)
	{
		PP_PropertyVector props = {
			ABINOVA_TBL_STYLE_ATTR, s_preview.oldStyleId,
			ABINOVA_TBL_LOOK_ATTR, s_preview.oldLook
		};
		m_pDoc->changeStruxFmtNoUndo(PTC_SetFmt,
									 const_cast<pf_Frag_Strux*>(tableSDH),
									 PP_NOPROPS, props);
	}
	m_pDoc->setDontImmediatelyLayout(false);
	_generalUpdate();
}

/* ---- Border Painter / Border Sampler ---- */

/* locate the cell border nearest to the click point; posCell gets a
 * position inside that cell and iEdge the edge index
 * (0 top, 1 bottom, 2 left, 3 right).  false when the click is
 * outside a table or too far from every edge */
bool FV_View::_borderEdgeAtXY(UT_sint32 xPos, UT_sint32 yPos,
							  PT_DocPosition & posCell, int & iEdge)
{
	PT_DocPosition pos = getDocPositionFromXY(xPos, yPos);
	if (!isInTable(pos))
	{
		return false;
	}
	fp_CellContainer * pCell = getCellAtPos(pos);
	UT_return_val_if_fail(pCell, false);

	fp_TableContainer * pTab =
		static_cast<fp_TableContainer *>(pCell->getTopmostTable());
	fp_TableContainer * pBroke = pTab ? pTab->getFirstBrokenTable() : nullptr;
	UT_return_val_if_fail(pBroke && pBroke->getPage(), false);

	UT_sint32 xoff = 0, yoff = 0;
	fp_Page * pPage = pBroke->getPage();
	getPageScreenOffsets(pPage, xoff, yoff);
	fp_Container * pCon = static_cast<fp_Container *>(pCell);
	while (pCon && !pCon->isColumnType())
	{
		xoff += pCon->getX();
		yoff += pCon->getY();
		pCon = pCon->getContainer();
	}
	if (pCon)
	{
		xoff += pCon->getX();
		yoff += pCon->getY();
	}
	yoff -= pBroke->getYBreak();

	/* everything stays in layout units: xPos/yPos come from the
	 * edit method's m_xPos which is tluD() (device -> layout) */
	UT_Rect r;
	r.left = xoff;
	r.top = yoff;
	r.width = pCell->getWidth();
	r.height = pCell->getHeight();

	UT_sint32 dL = labs(xPos - r.left);
	UT_sint32 dR = labs(xPos - (r.left + r.width));
	UT_sint32 dT = labs(yPos - r.top);
	UT_sint32 dB = labs(yPos - (r.top + r.height));
	UT_sint32 dMin = UT_MIN(UT_MIN(dL, dR), UT_MIN(dT, dB));
	/* ~12 px of tolerance at 1440 lu/in, 96 dpi */
	const UT_sint32 iSlop = 200;
	if (dMin > iSlop)
	{
		return false;
	}
	iEdge = (dMin == dT) ? 0 : (dMin == dB) ? 1 : (dMin == dL) ? 2 : 3;
	posCell = pos;
	return true;
}

static const char * const s_edgeNames[4] = { "top", "bot", "left", "right" };

/* position of the cell across the given edge, or 0 on the table's
 * outer rim */
static PT_DocPosition fv_neighborCellPos(FV_View * view, PD_Document * doc,
										 PT_DocPosition posCell, int iEdge)
{
	UT_sint32 iLeft, iRight, iTop, iBot;
	view->getCellParams(posCell, &iLeft, &iRight, &iTop, &iBot);
	const pf_Frag_Strux * tableSDH = nullptr;
	if (!doc->getStruxOfTypeFromPosition(posCell, PTX_SectionTable,
									   &tableSDH) || !tableSDH)
		return 0;
	UT_sint32 numRows = 0, numCols = 0;
	doc->getRowsColsFromTableStrux(tableSDH, view->isShowRevisions(),
								   view->getRevisionLevel(),
								   &numRows, &numCols);
	PT_DocPosition posTable = doc->getStruxPosition(tableSDH) + 1;
	switch (iEdge)
	{
	case 0: return iTop > 0 ? view->findCellPosAt(posTable, iTop - 1, iLeft) : 0;
	case 1: return iBot < numRows ? view->findCellPosAt(posTable, iBot, iLeft) : 0;
	case 2: return iLeft > 0 ? view->findCellPosAt(posTable, iTop, iLeft - 1) : 0;
	default: return iRight < numCols ? view->findCellPosAt(posTable, iTop, iRight) : 0;
	}
}

/* Border Painter: stamp the current table pen onto the nearest cell
 * edge.  The complementary edge of the neighbour cell is painted too
 * so a shared border keeps a single look */
bool FV_View::cmdBorderPaintAt(UT_sint32 xPos, UT_sint32 yPos)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	PT_DocPosition pos = 0;
	int iEdge = 0;
	if (!_borderEdgeAtXY(xPos, yPos, pos, iEdge))
	{
		return false;
	}
	const pf_Frag_Strux * cellSDH = nullptr;
	UT_return_val_if_fail(m_pDoc->getStruxOfTypeFromPosition(
		pos, PTX_SectionCell, &cellSDH) && cellSDH, false);
	const pf_Frag_Strux * tableSDH = nullptr;
	UT_return_val_if_fail(m_pDoc->getStruxOfTypeFromPosition(
		pos, PTX_SectionTable, &tableSDH) && tableSDH, false);
	PT_DocPosition posTable = m_pDoc->getStruxPosition(tableSDH) + 1;
	PT_DocPosition posCell = m_pDoc->getStruxPosition(cellSDH) + 1;

	std::string pStyle = std::string(s_edgeNames[iEdge]) + "-style";
	std::string pThick = std::string(s_edgeNames[iEdge]) + "-thickness";
	std::string pColor = std::string(s_edgeNames[iEdge]) + "-color";

	_changeCellParams(posTable, tableSDH);
	const PP_PropertyVector props = {
		pStyle.c_str(), s_tablePen.style.c_str(),
		pThick.c_str(), s_tablePen.thickness.c_str(),
		pColor.c_str(), s_tablePen.color.c_str()
	};
	m_pDoc->changeStruxFmt(PTC_AddFmt, posCell, posCell,
						   PP_NOPROPS, props, PTX_SectionCell);

	/* shared edge: paint the neighbour's complementary side too */
	const int oppEdge[4] = { 1, 0, 3, 2 };
	PT_DocPosition posOther = fv_neighborCellPos(this, m_pDoc, pos, iEdge);
	if (posOther)
	{
		const pf_Frag_Strux * otherSDH = nullptr;
		if (m_pDoc->getStruxOfTypeFromPosition(posOther, PTX_SectionCell,
											 &otherSDH) && otherSDH)
		{
			std::string oStyle = std::string(s_edgeNames[oppEdge[iEdge]]) + "-style";
			std::string oThick = std::string(s_edgeNames[oppEdge[iEdge]]) + "-thickness";
			std::string oColor = std::string(s_edgeNames[oppEdge[iEdge]]) + "-color";
			PT_DocPosition posOC = m_pDoc->getStruxPosition(otherSDH) + 1;
			const PP_PropertyVector oprops = {
				oStyle.c_str(), s_tablePen.style.c_str(),
				oThick.c_str(), s_tablePen.thickness.c_str(),
				oColor.c_str(), s_tablePen.color.c_str()
			};
			m_pDoc->changeStruxFmt(PTC_AddFmt, posOC, posOC,
								   PP_NOPROPS, oprops, PTX_SectionCell);
		}
	}
	_restoreCellParams(posTable, tableSDH);
	_generalUpdate();
	return true;
}

/* Border Sampler: copy the nearest cell edge's pen back into the
 * table pen, then hand over to the Border Painter (the Word
 * paintbrush workflow: sample here, paint there) */
bool FV_View::cmdBorderSampleAt(UT_sint32 xPos, UT_sint32 yPos)
{
	PT_DocPosition pos = 0;
	int iEdge = 0;
	if (!_borderEdgeAtXY(xPos, yPos, pos, iEdge))
	{
		return false;
	}
	const pf_Frag_Strux * cellSDH = nullptr;
	UT_return_val_if_fail(m_pDoc->getStruxOfTypeFromPosition(
		pos, PTX_SectionCell, &cellSDH) && cellSDH, false);

	auto readEdge = [&](const pf_Frag_Strux * sdh, int edge,
						std::string & sStyle, std::string & sThick,
						std::string & sColor) -> bool {
		const gchar * v = nullptr;
		bool bAny = false;
		std::string base = s_edgeNames[edge];
		if (m_pDoc->getPropertyFromStrux(sdh, isShowRevisions(),
									   getRevisionLevel(),
									   (base + "-style").c_str(),
									   reinterpret_cast<const char**>(&v)) && v)
		{ sStyle = v; bAny = true; }
		v = nullptr;
		if (m_pDoc->getPropertyFromStrux(sdh, isShowRevisions(),
									   getRevisionLevel(),
									   (base + "-thickness").c_str(),
									   reinterpret_cast<const char**>(&v)) && v)
		{ sThick = v; bAny = true; }
		v = nullptr;
		if (m_pDoc->getPropertyFromStrux(sdh, isShowRevisions(),
									   getRevisionLevel(),
									   (base + "-color").c_str(),
									   reinterpret_cast<const char**>(&v)) && v)
		{ sColor = v; bAny = true; }
		return bAny;
	};

	std::string sStyle, sThick, sColor;
	if (!readEdge(cellSDH, iEdge, sStyle, sThick, sColor))
	{
		/* try the neighbour's complementary edge - shared borders
		 * may be owned by either side */
		const int oppEdge[4] = { 1, 0, 3, 2 };
		PT_DocPosition posOther = fv_neighborCellPos(this, m_pDoc, pos, iEdge);
		if (posOther)
		{
			const pf_Frag_Strux * otherSDH = nullptr;
			if (m_pDoc->getStruxOfTypeFromPosition(posOther,
												 PTX_SectionCell,
												 &otherSDH) && otherSDH)
				readEdge(otherSDH, oppEdge[iEdge], sStyle, sThick, sColor);
		}
	}
	setTablePen(sStyle.empty() ? nullptr : sStyle.c_str(),
				sThick.empty() ? nullptr : sThick.c_str(),
				sColor.empty() ? nullptr : sColor.c_str());
	/* sampled pen goes straight to the painter, like Word */
	setBorderSamplerMode(false);
	setBorderPainterMode(true);
	/* mode changed: nudge listeners so the ribbon toggle syncs */
	_generalUpdate();
	return true;
}
