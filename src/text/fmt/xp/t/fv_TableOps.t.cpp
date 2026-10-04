/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
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

/*
 * Headless coverage of the fv_View table command surface
 * (fv_View_cmd.cpp ~2000 lines, fv_View_tableStyle.cpp) plus the
 * fp_TableContainer/fl_TableLayout machinery those commands drive:
 * insert, row/column edits, merge/split, autosizing, sorting,
 * header repetition, alignment, borders, styles, table-to-text.
 */

#include "tf_test.h"

#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "fl_DocLayout.h"
#include "fl_TableStyles.h"
#include "fv_View.h"
#include "fp_types.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "ut_growbuf.h"
#include "ie_exp.h"
#include "ie_types.h"

#include <cstdio>
#include <string>

#define TFSUITE "core.text.fmt.tableops"

namespace {

struct TableOpsView
{
	TableOpsView() = default;
	TableOpsView(const TableOpsView &) = delete;
	TableOpsView &operator=(const TableOpsView &) = delete;

	bool load(const char *text)
	{
		doc = new PD_Document;
		if (doc->createRawDocument() != UT_OK)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		pt_PieceTable * pt = doc->getPieceTable();
		bool ok = pt->appendStrux(PTX_Section, PP_NOPROPS);
		const char *p = text;
		while (p)
		{
			const char *nl = strchr(p, '\n');
			ok = ok && pt->appendStrux(PTX_Block, PP_NOPROPS);
			UT_UCS4String s(p, nl ? static_cast<size_t>(nl - p)
								  : strlen(p));
			if (s.length())
			{
				ok = ok && pt->appendSpan(s.ucs4_str(),
							static_cast<UT_uint32>(s.length()));
			}
			p = nl ? nl + 1 : nullptr;
		}
		if (!ok)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		doc->finishRawCreation();

		GR_UnixCairoAllocInfo ai(nullptr);
		graphics = XAP_App::getApp()->newGraphics(GRID_UNIX_PANGO, ai);
		if (!graphics)
			return false;
		layout = new FL_DocLayout(doc, graphics);
		view = new FV_View(XAP_App::getApp(), nullptr, layout);
		layout->fillLayouts();
		layout->formatAll();
		view->setWindowSize(800, 600);
		return layout->countPages() > 0;
	}

	PT_DocPosition eod() const
	{
		PT_DocPosition pos = 0;
		doc->getBounds(true, pos);
		return pos;
	}

	/* position of the table strux containing the view's point */
	PT_DocPosition tablePos() const
	{
		const pf_Frag_Strux * sdh = nullptr;
		if (!doc->getStruxOfTypeFromPosition(view->getPoint(),
										   PTX_SectionTable, &sdh) || !sdh)
			return 0;
		return doc->getStruxPosition(sdh);
	}

	bool tableDims(UT_sint32 & rows, UT_sint32 & cols) const
	{
		const pf_Frag_Strux * sdh = nullptr;
		if (!doc->getStruxOfTypeFromPosition(view->getPoint(),
										   PTX_SectionTable, &sdh) || !sdh)
			return false;
		return doc->getRowsColsFromTableStrux(sdh,
											  view->isShowRevisions(),
											  view->getRevisionLevel(),
											  &rows, &cols);
	}

	~TableOpsView()
	{
		delete view;
		delete layout;
		delete graphics;
		if (doc)
			doc->unref();
	}

	PD_Document *doc = nullptr;
	GR_Graphics *graphics = nullptr;
	FL_DocLayout *layout = nullptr;
	FV_View *view = nullptr;
};

std::string table_doc_text(FV_View * v)
{
	UT_GrowBuf buf;
	v->getTextInDocument(buf);
	std::string out;
	out.reserve(buf.getLength());
	for (UT_uint32 i = 0; i < buf.getLength(); ++i)
	{
		UT_UCS4Char c = static_cast<UT_UCS4Char>(buf.getPointer(i)[0]);
		if (c < 0x80)
			out += static_cast<char>(c);
	}
	return out;
}

/* find the on-screen coordinates of a doc position so the border-
 * painting commands get real x/y */
bool posToXY(FV_View * v, PT_DocPosition pos, UT_sint32 & x, UT_sint32 & y)
{
	for (y = 0; y < 2000; y += 4)
	{
		for (x = 0; x < 1200; x += 4)
		{
			if (v->getDocPositionFromXY(x, y) == pos)
				return true;
		}
	}
	return false;
}

}

TFTEST_MAIN("insert table, fill cells, navigate")
{
	TableOpsView hv;
	TFPASS(hv.load("above\nbelow"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(4); /* inside first block */
	TFPASS(v->cmdInsertTable(3, 3, PP_NOPROPS) != UT_ERROR);
	TFPASS(v->isInTable());

	UT_sint32 rows = 0, cols = 0;
	TFPASS(hv.tableDims(rows, cols));
	TFPASSEQ(rows, 3);
	TFPASSEQ(cols, 3);

	/* fill the first row's cells via next-cell navigation */
	TFPASS(v->cmdCharInsert(std::string("r1c1"), false));
	TFPASS(v->cmdAdvanceNextPrevCell(true));
	v->cmdCharInsert(std::string("r1c2"), false);
	TFPASS(v->cmdAdvanceNextPrevCell(true));
	v->cmdCharInsert(std::string("r1c3"), false);
	TFPASS(v->cmdAdvanceNextPrevCell(true));
	TFPASS(v->cmdAdvanceNextPrevCell(false));
	std::string t = table_doc_text(v);
	TFPASS(t.find("r1c1") != std::string::npos);
	TFPASS(t.find("r1c3") != std::string::npos);

	/* table sits in the layout; undo removes it whole */
	TFPASS(hv.layout->countPages() >= 1);
	v->cmdUndo(1);
	v->setPoint(hv.eod());
	TFPASS(!v->isInTable());
	v->cmdRedo(1);
	TFPASS(v->isInTable(4) || v->isInTable());
}

TFTEST_MAIN("row and column insert/delete")
{
	TableOpsView hv;
	TFPASS(hv.load("tbl host"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(4);
	TFPASS(v->cmdInsertTable(2, 2, PP_NOPROPS) != UT_ERROR);
	PT_DocPosition posTable = hv.tablePos();
	TFPASS(posTable > 0);

	UT_sint32 rows = 0, cols = 0;
	PT_DocPosition posCell = v->findCellPosAt(posTable + 1, 0, 0);
	TFPASS(posCell > 0);

	/* add a row after and a column after the first cell; positions
	 * shift after each structural edit, so re-look-up each time */
	TFPASS(v->cmdInsertRow(posCell + 1, false));
	TFPASS(hv.tableDims(rows, cols));
	TFPASSEQ(rows, 3);

	posCell = v->findCellPosAt(posTable + 1, 0, 0);
	TFPASS(v->cmdInsertCol(posCell + 1, false));
	TFPASS(hv.tableDims(rows, cols));
	TFPASSEQ(cols, 3);

	/* delete them again */
	posCell = v->findCellPosAt(posTable + 1, 0, 0);
	TFPASS(v->cmdDeleteRow(posCell + 1));
	TFPASS(hv.tableDims(rows, cols));
	TFPASSEQ(rows, 2);
	posCell = v->findCellPosAt(posTable + 1, 0, 0);
	v->setPoint(posCell + 2); /* strictly inside the cell's block */
	TFPASS(v->cmdDeleteCol(v->getPoint()));
	TFPASS(hv.tableDims(rows, cols));
	TFPASSEQ(cols, 2);

	/* bad positions fail cleanly (cmdDeleteCell only warns) */
	TFPASS(!v->cmdInsertRow(1, false));
	TFPASS(!v->cmdDeleteRow(1));
	TFPASS(!v->cmdInsertCol(1, false));
	TFPASS(!v->cmdDeleteCol(1));
	v->cmdDeleteCell(1);
	TFPASS(!v->cmdDeleteTable(1));

	/* delete the whole table */
	PT_DocPosition posC = v->findCellPosAt(posTable + 1, 0, 0);
	TFPASS(v->cmdDeleteTable(posC + 1));
	TFPASS(!v->isInTable(4));
}

TFTEST_MAIN("merge, split, autosize, distribute")
{
	TableOpsView hv;
	TFPASS(hv.load("merge host"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(4);
	TFPASS(v->cmdInsertTable(4, 4, PP_NOPROPS) != UT_ERROR);
	PT_DocPosition posTable = hv.tablePos();
	TFPASS(posTable > 0);

	/* merge cell (1,1) right into (1,2) */
	PT_DocPosition posA = v->findCellPosAt(posTable + 1, 1, 1);
	PT_DocPosition posB = v->findCellPosAt(posTable + 1, 1, 2);
	TFPASS(posA > 0 && posB > 0);
	TFPASS(v->cmdMergeCells(posA + 1, posB + 1));

	/* merge down via the directional variant */
	v->setPoint(v->findCellPosAt(posTable + 1, 0, 0) + 1);
	TFPASS(v->cmdMergeCellsDir(3)); /* below */
	TFPASS(!v->cmdMergeCellsDir(2)); /* above row 0 -> false */

	/* split a merged/regular cell both ways */
	v->setPoint(v->findCellPosAt(posTable + 1, 3, 3) + 1);
	TFPASS(v->cmdSplitCells(hori_mid));
	v->setPoint(v->findCellPosAt(posTable + 1, 3, 3) + 1);
	TFPASS(v->cmdSplitCells(vert_mid));
	TFPASS(!v->cmdSplitCells(hori_mid) ||
		   v->cmdSplitCells(hori_mid)); /* idempotent-ish, keep going */

	/* autosize + distribute paths */
	TFPASS(v->cmdAutoSizeCols());
	TFPASS(v->cmdAutoSizeRows());
	TFPASS(v->cmdAutoFitTable());
	TFPASS(v->cmdAutoFitWindow());
	TFPASS(v->cmdFixColumnWidths());
	TFPASS(v->cmdDistributeCols());
	TFPASS(v->cmdDistributeRows());

	/* interactive-resize equivalents */
	v->setPoint(v->findCellPosAt(posTable + 1, 0, 0) + 1);
	TFPASS(v->cmdTableColResize(true));
	TFPASS(v->cmdTableColResize(false));
	TFPASS(v->cmdTableRowResize(true));
	TFPASS(v->cmdTableRowResize(false));
	TFPASS(v->cmdTableColWidth("2.0in", v->getPoint()));
	TFPASS(v->cmdTableRowHeight("0.5in", v->getPoint()));

	double w = 0, h = 0;
	TFPASS(v->getTableCellDims(DIM_IN, w, h));
	TFPASS(w > 0 && h > 0);
}

TFTEST_MAIN("sort rows, header row, cell alignment, text direction")
{
	TableOpsView hv;
	TFPASS(hv.load("sort host"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(4);
	TFPASS(v->cmdInsertTable(3, 2, PP_NOPROPS) != UT_ERROR);
	PT_DocPosition posTable = hv.tablePos();

	/* fill the first column out of order */
	static const char * vals[3] = { "delta", "alpha", "charlie" };
	for (int r = 0; r < 3; ++r)
	{
		v->setPoint(v->findCellPosAt(posTable + 1, r, 0) + 1);
		v->cmdCharInsert(std::string(vals[r]), false);
	}

	/* sort ascending on column 0 without touching a header row */
	TFPASS(v->cmdSortTableRows(true, 0, false));
	TFPASS(v->cmdSortTableRows(false, 0, false));

	/* header repetition toggle */
	bool wasOn = v->isRepeatHeaderOn();
	TFPASS(v->cmdToggleRepeatHeader());
	TFPASS(v->isRepeatHeaderOn() != wasOn);
	TFPASS(v->cmdToggleRepeatHeader());

	/* alignment + direction on the caret cell */
	v->setPoint(v->findCellPosAt(posTable + 1, 0, 0) + 1);
	TFPASS(v->cmdTableCellAlign(50, "center"));
	TFPASS(v->cmdTableCellAlign(0, "left"));
	TFPASS(v->cmdTableCellAlign(100, "right"));
	TFPASS(!v->cmdTableCellAlign(33, "bogus"));
	/* "ltr"/"rtl" are the only accepted directions */
	TFPASS(v->cmdCellTextDirection("rtl"));
	TFPASS(v->cmdCellTextDirection("ltr"));
	TFPASS(!v->cmdCellTextDirection("btlr"));

	/* whole-column selection (needs a strictly-in-cell position) */
	v->setPoint(v->findCellPosAt(posTable + 1, 0, 0) + 2);
	TFPASS(v->cmdSelectColumn(v->getPoint()));
	TFPASS(v->getNumColumnsInSelection() >= 1);
	v->cmdUnselectSelection();
}

TFTEST_MAIN("table styles, border presets, cell shading")
{
	TableOpsView hv;
	TFPASS(hv.load("style host"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(4);
	TFPASS(v->cmdInsertTable(3, 3, PP_NOPROPS) != UT_ERROR);
	PT_DocPosition posTable = hv.tablePos();
	v->setPoint(v->findCellPosAt(posTable + 1, 1, 1) + 1);
	TFPASS(v->isInTable());

	/* apply + look options + preview + clear */
	TFPASS(v->cmdTableSetStyle("TableGrid"));
	for (int opt = 0; opt < 6; ++opt)
		TFPASS(v->cmdTableSetStyleOption(opt, true));
	TFPASS(v->cmdTableStylePreviewBegin("TableGrid"));
	v->cmdTableStylePreviewEnd();
	TFPASS(v->cmdTableClearStyle());
	TFPASS(!v->cmdTableSetStyle("NoSuchStyle"));
	TFPASS(!v->cmdTableStylePreviewBegin("NoSuchStyle"));

	/* border presets incl. invalid */
	for (int p = 0; p < FV_TBP__COUNT; ++p)
		TFPASS(v->cmdTableBorderPreset(p));
	TFPASS(!v->cmdTableBorderPreset(-1));
	TFPASS(!v->cmdTableBorderPreset(FV_TBP__COUNT));

	/* cell shading via the format path */
	TFPASS(v->cmdTableCellShading("ffcc00"));
	TFPASS(!v->cmdTableCellShading(""));

	/* pen + paint/erase at real screen coords */
	v->setTablePen("solid", "1.0pt", "0000ff");
	UT_sint32 x = 0, y = 0;
	PT_DocPosition posCell = v->findCellPosAt(posTable + 1, 0, 0) + 1;
	if (posToXY(v, posCell, x, y))
	{
		v->cmdBorderPaintAt(x, y);
		v->cmdEraseTableBorder(x, y);
	}
	/* out-of-table coords still exercise the lookup path */
	v->cmdEraseTableBorder(-50, -50);
	TFPASS(hv.layout->countPages() >= 1);
}

TFTEST_MAIN("split table and table-to-text")
{
	TableOpsView hv;
	TFPASS(hv.load("split host"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(4);
	TFPASS(v->cmdInsertTable(4, 2, PP_NOPROPS) != UT_ERROR);
	PT_DocPosition posTable = hv.tablePos();

	/* split at row 2 -> two tables */
	v->setPoint(v->findCellPosAt(posTable + 1, 2, 0) + 1);
	TFPASS(v->cmdSplitTable());
	TFPASS(hv.layout->countPages() >= 1);

	/* undo merges it back, then convert to text */
	v->cmdUndo(1);
	v->setPoint(v->findCellPosAt(posTable + 1, 0, 0) + 1);
	if (!v->isInTable())
	{
		v->setPoint(4);
		v->cmdCharInsert(std::string("x"), false);
	}

	/* table-to-text on the (possibly still split) first table */
	if (v->isInTable())
	{
		PT_DocPosition pt = hv.tablePos();
		v->setPoint(v->findCellPosAt(pt + 1, 0, 0) + 2);
		TFPASS(v->cmdTableToText(v->getPoint(), 0));
	}
}
