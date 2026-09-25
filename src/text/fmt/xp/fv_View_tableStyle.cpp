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
#include "fl_TableStyles.h"
#include "pd_Document.h"
#include "pt_Types.h"
#include "pp_AttrProp.h"
#include "pf_Frag.h"
#include "pf_Frag_Strux.h"
#include "fl_TableLayout.h"
#include "fl_BlockLayout.h"
#include "fl_ContainerLayout.h"
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

static bool fv_enumTable(FV_View * view, PD_Document * doc,
						 FV_TblCells & out)
{
	UT_return_val_if_fail(view && doc, false);
	PT_DocPosition posCaret = view->getPoint();

	const pf_Frag_Strux * tableSDH = nullptr;
	if (!doc->getStruxOfTypeFromPosition(posCaret, PTX_SectionTable, &tableSDH) ||
		!tableSDH)
	{
		/* whole-table/cell selection: anchor can sit on the strux
		 * itself where the position lookup misses */
		if (!view->isSelectionEmpty() &&
			doc->getStruxOfTypeFromPosition(view->getSelectionAnchor(),
											PTX_SectionTable, &tableSDH) &&
			tableSDH)
		{
			// found via anchor
		}
		else
		{
			return false;
		}
	}

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
		if (!noUndo && !cp.charProps.empty())
		{
			const pf_Frag_Strux * endCell =
				doc->getEndCellStruxFromCellStrux(c.sdh);
			if (endCell)
			{
				PT_DocPosition posE = doc->getStruxPosition(endCell);
				PP_PropertyVector cprops = fv_propsVec(cp.charProps);
				doc->changeSpanFmt(PTC_AddFmt, c.pos + 1, posE,
								   PP_NOPROPS, cprops);
			}
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

	/* drop every style-owned cell prop; border state returns to the
	 * document default */
	PP_PropertyVector removeProps;
	for (const char * k : s_cellStyleKeys)
		removeProps.push_back(k);
	for (const FV_TblCell & c : t.cells)
	{
		m_pDoc->changeStruxFmt(PTC_RemoveFmt, c.pos + 1, c.pos + 1,
							   PP_NOPROPS, removeProps, PTX_SectionCell);
		/* reset any style-driven char fmt */
		const pf_Frag_Strux * endCell =
			m_pDoc->getEndCellStruxFromCellStrux(c.sdh);
		if (endCell)
		{
			PT_DocPosition posE = m_pDoc->getStruxPosition(endCell);
			PP_PropertyVector rm = { "font-weight", "font-style", "color" };
			m_pDoc->changeSpanFmt(PTC_RemoveFmt, c.pos + 1, posE,
								  PP_NOPROPS, rm);
		}
	}

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
			PP_PropertyVector removeProps;
			for (const char * k : s_cellStyleKeys)
				removeProps.push_back(k);
			for (const FV_TblCell & c : t.cells)
				m_pDoc->changeStruxFmt(PTC_RemoveFmt, c.pos + 1, c.pos + 1,
									   PP_NOPROPS, removeProps,
									   PTX_SectionCell);
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
	const pf_Frag_Strux * tableSDH = nullptr;
	if (!m_pDoc->getStruxOfTypeFromPosition(getPoint(), PTX_SectionTable,
										  &tableSDH) || !tableSDH)
	{
		if (isSelectionEmpty() ||
			!m_pDoc->getStruxOfTypeFromPosition(getSelectionAnchor(),
											  PTX_SectionTable, &tableSDH) ||
			!tableSDH)
			return "";
	}
	const gchar * v = nullptr;
	if (m_pDoc->getPropertyFromStrux(tableSDH, isShowRevisions(),
								   getRevisionLevel(), ABINOVA_TBL_STYLE_ATTR,
								   reinterpret_cast<const char**>(&v)) && v)
		return v;
	return "";
}

std::string FV_View::getTableStyleLook() const
{
	const pf_Frag_Strux * tableSDH = nullptr;
	if (!m_pDoc->getStruxOfTypeFromPosition(getPoint(), PTX_SectionTable,
										  &tableSDH) || !tableSDH)
	{
		if (isSelectionEmpty() ||
			!m_pDoc->getStruxOfTypeFromPosition(getSelectionAnchor(),
											  PTX_SectionTable, &tableSDH) ||
			!tableSDH)
			return "";
	}
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
