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

/* regression tests for the top-ruler drag math hardening:
 *
 * FV_View::getTopRulerInfo() can leave the caller's AP_TopRulerInfo
 * cache unpopulated (piece table busy, no run at the position). That
 * info reports m_iNumColumns == 0 and null cell vectors. The margin
 * drag paths used it as a divisor (SIGFPE) and the cell-marker paths
 * indexed the vectors unchecked. The helpers exercised here must
 * never divide by zero and must bounds-check every cell index —
 * including the right-edge sentinel index (== cell count) and the
 * m_draggingCell - 1 lower bound.
 *
 * No widget or document is needed: the helpers are pure functions of
 * AP_TopRulerInfo. */

#include "tf_test.h"

#include "ap_TopRuler.h"

#define TFSUITE "core.wp.ap.topruler"

TFTEST_MAIN("ap_TopRuler")
{
	/* ---- column divisor never reaches zero ---- */
	TFPASS(AP_TopRuler::numColumnsForDrag(nullptr) == 1);

	AP_TopRulerInfo info;
	TFPASS(info.m_iNumColumns == 0);           // reset() state: stale/empty
	TFPASS(AP_TopRuler::numColumnsForDrag(&info) == 1);
	info.m_iNumColumns = 1;
	TFPASS(AP_TopRuler::numColumnsForDrag(&info) == 1);
	info.m_iNumColumns = 4;
	TFPASS(AP_TopRuler::numColumnsForDrag(&info) == 4);

	/* ---- columnWidthAfterMarginMove: no SIGFPE on 0 columns ---- */
	// divisor 0 must behave as 1 (the whole delta hits the one column)
	TFPASS(AP_TopRuler::columnWidthAfterMarginMove(1000, 50, 0) == 950);
	// negative column counts must not divide either
	TFPASS(AP_TopRuler::columnWidthAfterMarginMove(1000, 50, -3) == 950);
	// sane case: delta spread over 4 columns
	TFPASS(AP_TopRuler::columnWidthAfterMarginMove(1000, 40, 4) == 990);

	/* ---- leftMarginDragAdjust on a zero-column (stale) info ---- */
	info.reset();
	info.u.c.m_xColumnWidth = 100;
	info.m_xrLeftIndent = 0;
	// delta 200 over a 100-wide column: without the guard this divided
	// by zero; now it computes the exact push-back for nCols == 1:
	// newColumnWidth = 100 - 200 = -100 < min(10) -> adjust = 110 * 1
	TFPASS(AP_TopRuler::leftMarginDragAdjust(&info, 200, 0, 10) == 110);
	// drag that fits: no adjustment
	TFPASS(AP_TopRuler::leftMarginDragAdjust(&info, 50, 0, 10) == 0);
	// null info: no adjustment
	TFPASS(AP_TopRuler::leftMarginDragAdjust(nullptr, 50, 0, 10) == 0);

	/* ---- rightMarginDragX: bounded loop, sane results ---- */
	UT_sint32 newMargin = 0, delta = 0;
	// x already valid: unchanged
	UT_sint32 x = AP_TopRuler::rightMarginDragX(980, 1000, &info, 10, 0, 0,
												newMargin, delta);
	TFPASS(x == 980);
	TFPASS(newMargin == 20);
	TFPASS(delta == 20 - info.u.c.m_xaRightMargin);

	// x = 0 -> margin 1000 -> column would be 100 - 1000 < 10; the
	// bounded loop must push x right by exactly the shortfall
	// (910) and converge, not spin forever (the old while(1) could).
	x = AP_TopRuler::rightMarginDragX(0, 1000, &info, 10, 0, 0,
									  newMargin, delta);
	TFPASS(x == 910);
	TFPASS(newMargin == 90);
	TFPASS(AP_TopRuler::columnWidthAfterMarginMove(
			   info.u.c.m_xColumnWidth, delta, 1) >= 10);

	// extreme drag still terminates inside the 8-pass cap and returns
	// a finite value
	x = AP_TopRuler::rightMarginDragX(-1000000, 1000, &info, 10, 0, 0,
									  newMargin, delta);
	TFPASS(x > -1000000);

	// null info: outputs defined, x passed through
	x = AP_TopRuler::rightMarginDragX(555, 1000, nullptr, 10, 0, 0,
									  newMargin, delta);
	TFPASS(x == 555);
	TFPASS(newMargin == 445);
	TFPASS(delta == 0);

	/* ---- table cell vector bounds ---- */
	AP_TopRulerInfo tinfo;
	// empty/stale info: every index, including the m_draggingCell - 1
	// and right-edge-sentinel cases, is out of bounds
	TFPASS(tinfo.tableCellInfoCount() == 0);
	TFPASS(tinfo.fullTableCellInfoCount() == 0);
	TFPASS(tinfo.tableCellInfo(-1) == nullptr);
	TFPASS(tinfo.tableCellInfo(0) == nullptr);
	TFPASS(tinfo.tableCellInfo(999) == nullptr);
	TFPASS(tinfo.fullTableCellInfo(0) == nullptr);

	// populated vectors: boundaries honoured
	tinfo.m_vecTableColInfo = new std::vector<AP_TopRulerTableInfo *>();
	tinfo.m_vecTableColInfo->push_back(new AP_TopRulerTableInfo);
	tinfo.m_vecTableColInfo->push_back(new AP_TopRulerTableInfo);
	tinfo.m_vecTableColInfo->at(0)->m_iLeftCellPos = 11;
	tinfo.m_vecTableColInfo->at(1)->m_iLeftCellPos = 22;

	TFPASS(tinfo.tableCellInfoCount() == 2);
	TFPASS(tinfo.tableCellInfo(-1) == nullptr);          // lower bound
	TFPASS(tinfo.tableCellInfo(0) != nullptr);
	TFPASS(tinfo.tableCellInfo(0)->m_iLeftCellPos == 11);
	TFPASS(tinfo.tableCellInfo(1)->m_iLeftCellPos == 22);
	// index 2 is the right-edge sentinel: a boundary marker, not a
	// cell — the accessor must return null, not an OOB element
	TFPASS(tinfo.tableCellInfo(2) == nullptr);
	TFPASS(tinfo.tableCellInfo(200) == nullptr);         // upper bound

	tinfo.m_vecFullTable = new std::vector<AP_TopRulerTableInfo *>();
	tinfo.m_vecFullTable->push_back(new AP_TopRulerTableInfo);
	TFPASS(tinfo.fullTableCellInfoCount() == 1);
	TFPASS(tinfo.fullTableCellInfo(0) != nullptr);
	TFPASS(tinfo.fullTableCellInfo(1) == nullptr);

	/* ---- reset() drops a stale table cache ---- */
	tinfo.m_mode = AP_TopRulerInfo::TRI_MODE_TABLE;
	tinfo.m_iNumColumns = 3;
	tinfo.m_iCells = 2;
	tinfo.reset();
	TFPASS(tinfo.m_iNumColumns == 0);
	TFPASS(tinfo.m_iCells == 0);
	TFPASS(tinfo.tableCellInfoCount() == 0);
	TFPASS(tinfo.fullTableCellInfoCount() == 0);
	TFPASS(tinfo.m_mode == AP_TopRulerInfo::TRI_MODE_COLUMNS);
}
