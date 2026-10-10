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

/* DRAG01 regression: dragging a selection past the window edge used to
 * fire unclamped scroll requests — cmdScroll() only clamped the offset
 * at 0 — so the request could overshoot the document end and the GTK
 * scrollbar layer rewound the view ("JUMP UP" at the bottom edge).
 * The drag-owned autoscroll machinery also kept its repeating worker
 * alive after mouse-release/abort and kept the acceleration counter
 * in file-static state shared between drag objects.
 *
 * These tests drive both autoscroll paths headlessly:
 *  - the FV_View select-extend autoscroll armed by extSelToXY(bDrag)
 *  - the FV_VisualDragText drag-move autoscroll armed by mouseDrag
 * and assert monotonic clamped scrolling, EOD/BOD selection extension
 * and complete worker/acceleration teardown on release. */

#include "tf_test.h"

#include "pd_Document.h"
#include "fl_DocLayout.h"
#include "fv_View.h"
#include "fv_VisualDragText.h"
#include "fp_Page.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "ie_types.h"

#include <glib.h>

#include <vector>

#define TFSUITE "core.text.fmt.dragautoscroll"

namespace {

const char *DRAG_FIXTURE = "/test/wp/BillOfRights.abw";

/* Widget-less layout stack — same shape as fv_ViewModes/fv_ViewOps:
 * a private cairo image surface backs every paint, so the whole
 * layout/view machinery works without a display. */
struct HeadlessDragView
{
	HeadlessDragView() = default;
	HeadlessDragView(const HeadlessDragView &) = delete;
	HeadlessDragView &operator=(const HeadlessDragView &) = delete;

	bool load(const char *relPath)
	{
		std::string data_file;
		if (!TF_Test::ensure_test_data(relPath, data_file))
			return false;
		doc = new PD_Document;
		if (doc->readFromFile(data_file.c_str(), IEFT_Unknown, nullptr) != UT_OK)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		GR_UnixCairoAllocInfo ai(nullptr);
		graphics = XAP_App::getApp()->newGraphics(GRID_UNIX_PANGO, ai);
		if (!graphics)
			return false;
		layout = new FL_DocLayout(doc, graphics);
		view = new FV_View(XAP_App::getApp(), nullptr, layout);
		layout->fillLayouts();
		layout->formatAll();
		return layout->countPages() >= 2;
	}

	~HeadlessDragView()
	{
		delete view;    /* before layout: ~FV_View detaches from it */
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

/* A headless stand-in for the GTK scrollbar: plays the AP_UnixFrame
 * _scrollFuncY contract — clamp the requested offset into the scroll
 * range and apply it via setYScrollOffset — while recording every
 * offset the model asked for, so the test can verify the model itself
 * never requests beyond the document bounds and never jumps back. */
struct DragScrollSpy
{
	DragScrollSpy(FV_View *v)
		: view(v),
		  obj(this, &DragScrollSpy::scrollX, &DragScrollSpy::scrollY)
	{
		view->addScrollListener(&obj);
	}
	~DragScrollSpy()
	{
		view->removeScrollListener(&obj);
	}

	UT_sint32 maxOffset() const
	{
		UT_sint32 maxOff = view->getLayout()->getHeight() - view->getWindowHeight();
		return maxOff > 0 ? maxOff : 0;
	}

	void applyY(UT_sint32 yoff)
	{
		requested.push_back(yoff);
		UT_sint32 maxOff = maxOffset();
		UT_sint32 clamped = yoff < 0 ? 0 : (yoff > maxOff ? maxOff : yoff);
		view->setYScrollOffset(clamped);
	}
	void applyX(UT_sint32 xoff)
	{
		if (xoff >= 0)
			view->setXScrollOffset(xoff);
	}

	static void scrollY(void *pData, UT_sint32 yoff, UT_sint32 /*ylim*/)
	{
		static_cast<DragScrollSpy *>(pData)->applyY(yoff);
	}
	static void scrollX(void *pData, UT_sint32 xoff, UT_sint32 /*xlim*/)
	{
		static_cast<DragScrollSpy *>(pData)->applyX(xoff);
	}

	FV_View *view;
	AV_ScrollObj obj;
	std::vector<UT_sint32> requested;
};

/* Pump the default main context so the autoscroll timer (g_timeout)
 * and the scroll worker (g_idle) actually fire.  blocking=true waits
 * for a source each turn, so a 100 ms timer tick costs one iteration. */
void dragPumpMain(int iterations)
{
	GMainContext *ctx = g_main_context_default();
	for (int i = 0; i < iterations; i++)
	{
		g_main_context_iteration(ctx, TRUE);
		TF_Test::pulse();
	}
}

/* Pump until the scroll offset stops changing for a few consecutive
 * turns (or the iteration cap hits).  Returns the settled offset. */
UT_sint32 dragPumpUntilStable(FV_View *view, int maxIter = 600)
{
	UT_sint32 last = view->getYScrollOffset();
	int stable = 0;
	for (int i = 0; i < maxIter && stable < 6; i++)
	{
		g_main_context_iteration(g_main_context_default(), TRUE);
		TF_Test::pulse();
		if (view->getYScrollOffset() == last)
			stable++;
		else
			stable = 0;
		last = view->getYScrollOffset();
	}
	return last;
}

bool dragMonotone(const std::vector<UT_sint32> &v, size_t from, bool increasing)
{
	for (size_t i = from + 1; i < v.size(); i++)
	{
		if (increasing ? v[i] < v[i - 1] : v[i] > v[i - 1])
			return false;
	}
	return true;
}

} // namespace

TFTEST_MAIN("select-drag autoscroll clamps at both document bounds")
{
	HeadlessDragView hv;
	TFPASS(hv.load(DRAG_FIXTURE));
	if (!hv.view)
		return;
	FV_View *v = hv.view;
	v->setWindowSize(700, 400);
	DragScrollSpy spy(v);
	UT_sint32 maxOff = spy.maxOffset();
	TFPASS(maxOff > 0);

	const UT_sint32 midX = v->getWindowWidth() / 2;
	const UT_sint32 winH = v->getWindowHeight();
	const UT_sint32 edge = v->getGraphics()->tlu(400);

	PT_DocPosition posEOD = 0;
	TFPASS(v->getEditableBounds(true, posEOD));

	/* ---- downward: point at BOD, drag below the bottom edge ---- */
	v->moveInsPtTo(FV_DOCPOS_BOD);
	v->extSelToXY(midX, winH + edge, true);
	TFPASS(dragPumpUntilStable(v) == maxOff);

	/* the view scrolled down and parked exactly at the document end */
	TFPASS(!spy.requested.empty());
	TFPASS(v->getYScrollOffset() == maxOff);
	/* every offset the model asked for stayed inside the scroll
	 * range — before the clamp, drag autoscroll issued requests past
	 * the document end, which is what let the scrollbar layer snap
	 * the view back to the top */
	bool inRange = true;
	for (UT_sint32 r : spy.requested)
		inRange = inRange && (r >= 0) && (r <= maxOff);
	TFPASS(inRange);
	/* no earlier-position jump: requests never moved backwards */
	TFPASS(dragMonotone(spy.requested, 0, true));
	/* the selection ran to the end of the document */
	TFPASS(!v->isSelectionEmpty());
	TFPASS(v->getPoint() >= posEOD - 8);

	/* ---- release: arming timer stops and no stray scrolls ---- */
	v->endDrag(midX, winH + edge);
	dragPumpMain(6);	/* let an in-flight worker tick land */
	const size_t settled = spy.requested.size();
	TFPASS(!v->hasPendingScrollWorker());
	dragPumpMain(15);
	TFPASS(spy.requested.size() == settled);
	TFPASS(v->getYScrollOffset() == maxOff);

	/* ---- upward: point at EOD, drag past the top edge ---- */
	const size_t upFrom = spy.requested.size();
	v->moveInsPtTo(FV_DOCPOS_EOD);
	v->extSelToXY(midX, -edge, true);
	TFPASS(dragPumpUntilStable(v) == 0);
	TFPASS(v->getYScrollOffset() == 0);
	TFPASS(dragMonotone(spy.requested, upFrom, false));
	/* selection ran back to the top of the document — the pointer
	 * was mid-line so the leading edge lands inside the first block
	 * rather than exactly on BOD */
	PT_DocPosition posBOD = 0;
	TFPASS(v->getEditableBounds(false, posBOD));
	TFPASS(v->getPoint() < posEOD);
	TFPASS(v->getBlockAtPosition(v->getPoint()) ==
		   v->getBlockAtPosition(posBOD));

	v->endDrag(midX, -edge);
	dragPumpMain(6);
	const size_t settledUp = spy.requested.size();
	TFPASS(!v->hasPendingScrollWorker());
	dragPumpMain(15);
	TFPASS(spy.requested.size() == settledUp);
	TFPASS(v->getYScrollOffset() == 0);
}

TFTEST_MAIN("visual-drag autoscroll worker dies on abort, accel resets")
{
	HeadlessDragView hv;
	TFPASS(hv.load(DRAG_FIXTURE));
	if (!hv.view)
		return;
	FV_View *v = hv.view;
	v->setWindowSize(700, 400);
	DragScrollSpy spy(v);

	const UT_sint32 midX = v->getWindowWidth() / 2;
	const UT_sint32 winH = v->getWindowHeight();
	const UT_sint32 edge = v->getGraphics()->tlu(400);

	PT_DocPosition posBOD = 0, posEOD = 0;
	TFPASS(v->getEditableBounds(false, posBOD));
	TFPASS(v->getEditableBounds(true, posEOD));

	FV_VisualDragText *vd = v->getVisualText();

	/* select a chunk on the first page, then arm a copy-drag and
	 * drag it below the bottom edge of the window */
	v->cmdSelect(posBOD, posBOD + 200 < posEOD ? posBOD + 200 : posEOD);
	TFPASS(!v->isSelectionEmpty());
	vd->mouseCopy(midX, v->getGraphics()->tlu(60));
	vd->mouseDrag(midX, winH + edge);
	vd->mouseDrag(midX + v->getGraphics()->tlu(30), winH + edge);
	TFPASS(vd->getVisualDragMode() == FV_VisualDrag_DRAGGING);

	/* the arming timer hands off to the repeating scroll worker */
	const size_t before = spy.requested.size();
	dragPumpMain(10);
	TFPASS(spy.requested.size() > before);
	TFPASS(vd->isAutoScrollActive());

	/* abort: both the arming timer and the repeating worker must be
	 * dead, and the per-drag acceleration must be back at zero —
	 * before the fix the file-static worker was never stopped and
	 * kept scrolling from stale mouse coordinates after release */
	vd->abortDrag();
	TFPASS(!vd->isAutoScrollActive());
	TFPASS(vd->getScrollAccelExtra() == 0);
	const size_t frozen = spy.requested.size();
	dragPumpMain(20);
	TFPASS(spy.requested.size() == frozen);

	/* a fresh drag starts with clean autoscroll state and scrolls
	 * again — the acceleration counter did not carry over (the view
	 * may already sit at the bottom, so rewind it first or the new
	 * scroll requests would all be no-ops) */
	v->setYScrollOffset(0);
	v->cmdSelect(posBOD, posBOD + 200 < posEOD ? posBOD + 200 : posEOD);
	vd->mouseCopy(midX, v->getGraphics()->tlu(60));
	vd->mouseDrag(midX, winH + edge);
	vd->mouseDrag(midX + v->getGraphics()->tlu(30), winH + edge);
	TFPASS(vd->getVisualDragMode() == FV_VisualDrag_DRAGGING);
	dragPumpMain(10);
	TFPASS(spy.requested.size() > frozen);
	vd->abortDrag();
	TFPASS(!vd->isAutoScrollActive());
	TFPASS(vd->getScrollAccelExtra() == 0);
}
