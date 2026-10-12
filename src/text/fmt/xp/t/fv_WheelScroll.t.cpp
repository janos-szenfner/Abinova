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
 * Wheel-scroll speed (SCROLL02): the discrete wheel-button edit
 * methods and the GDK smooth-scroll delta path must move the view
 * by the same per-notch distance — the view's wheel-scroll step —
 * while fractional touchpad deltas keep their sub-notch precision
 * and page boundaries still clamp.
 */

#include "tf_test.h"

#include "pd_Document.h"
#include "fl_DocLayout.h"
#include "fl_BlockLayout.h"
#include "fl_ContainerLayout.h"
#include "fp_Line.h"
#include "fv_View.h"
#include "gr_UnixCairoGraphics.h"
#include "ev_UnixMouse.h"
#include "ev_EditMethod.h"
#include "xap_App.h"
#include "xap_EditMethods.h"
#include "ie_types.h"

#include <cstdlib>
#include <memory>
#include <string>

#define TFSUITE "core.text.fmt.wheelscroll"

namespace {

const char *WS_FIXTURE = "/test/wp/BillOfRights.abw";

/* A document with a full interactive layout stack on a widget-less
 * GR_UnixCairoGraphics — same fixture pattern as fv_ViewModes. */
struct WS_View
{
	WS_View() = default;
	WS_View(const WS_View &) = delete;
	WS_View &operator=(const WS_View &) = delete;

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
		return layout->countPages() > 0;
	}

	~WS_View()
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

/* Mimic the frame's scroll callbacks: apply the emitted offset the
 * way AP_UnixFrame::_scrollFuncX/Y do, without the ease animation —
 * otherwise a listener-less headless view never moves. */
static void s_applyX(void *p, UT_sint32 xoff, UT_sint32 /*xlimit*/)
{
	static_cast<FV_View *>(p)->setXScrollOffset(xoff);
}
static void s_applyY(void *p, UT_sint32 yoff, UT_sint32 /*ylimit*/)
{
	static_cast<FV_View *>(p)->setYScrollOffset(yoff);
}

}

TFTEST_MAIN("wheel step, smooth/discrete parity and clamping")
{
	WS_View hv;
	TFPASS(hv.load(WS_FIXTURE));
	if (!hv.view)
		return;
	FV_View *v = hv.view;
	v->setWindowSize(800, 600);

	AV_ScrollObj scroller(v, s_applyX, s_applyY);
	v->addScrollListener(&scroller);

	/* the per-notch step is derived from the line at the caret:
	 * 4 text lines — Word is ~3, tuned toward the quicker end of
	 * the requested 2-3x band over the old fixed 36px */
	const UT_sint32 step = v->getWheelScrollStep();
	TFPASS(step > 0);
	fl_BlockLayout *bl = v->getCurrentBlock();
	TFPASS(bl != nullptr);
	fp_Line *line = bl
		? static_cast<fp_Line *>(bl->getFirstContainer()) : nullptr;
	TFPASS(line != nullptr);
	if (line)
		TFPASS(step == 4 * line->getHeight());
	TFPASS(step >= v->getGraphics()->tlu(2 * 36));

	/* discrete wheel-button path: the edit methods bound to
	 * buttons 4/5 move exactly one step per notch */
	std::unique_ptr<EV_EditMethodContainer> pemc(AP_GetEditMethods());
	TFPASS(pemc != nullptr);
	EV_EditMethod *down = pemc->findEditMethodByName("scrollWheelMouseDown");
	EV_EditMethod *up = pemc->findEditMethodByName("scrollWheelMouseUp");
	TFPASS(down != nullptr);
	TFPASS(up != nullptr);
	EV_EditMethodCallData cd;
	if (down)
	{
		TFPASS(down->Fn(v, &cd));
		TFPASS(v->getYScrollOffset() == step);
		down->Fn(v, &cd);
		TFPASS(v->getYScrollOffset() == 2 * step);
	}
	if (up)
	{
		up->Fn(v, &cd);
		TFPASS(v->getYScrollOffset() == step);
	}

	/* smooth-scroll path (touchpad / GDK_SCROLL_SMOOTH): one
	 * notch of delta lands on the same step — consistency between
	 * the two paths is by construction, both read the view step.
	 * tlu/tdu round-tripping may drop a fraction of a pixel. */
	EV_UnixMouse mouse(nullptr);
	mouse.smoothScrollDeltas(v, 0.0, 1.0);
	const UT_sint32 notch = v->getYScrollOffset() - step;
	TFPASS(notch <= step);
	TFPASS(notch >= step - v->getGraphics()->tlu(1));

	/* half a notch moves proportionally — sub-notch deltas are
	 * not collapsed to whole notches */
	const UT_sint32 y1 = v->getYScrollOffset();
	mouse.smoothScrollDeltas(v, 0.0, 0.5);
	const UT_sint32 half = v->getYScrollOffset() - y1;
	TFPASS(half > 0);
	TFPASS(half < notch);
	TFPASS(std::abs(2 * half - notch) <= v->getGraphics()->tlu(2));

	/* sub-pixel touchpad deltas accumulate until they reach a
	 * pixel — they are held, not dropped, and cannot jump */
	const gdouble pixPerNotch = v->getGraphics()->tdu(step);
	TFPASS(pixPerNotch > 0.0);
	const gdouble subPixel = 0.4 / pixPerNotch;	/* 0.4px per event */
	const UT_sint32 y2 = v->getYScrollOffset();
	mouse.smoothScrollDeltas(v, 0.0, subPixel);
	TFPASS(v->getYScrollOffset() == y2);
	mouse.smoothScrollDeltas(v, 0.0, subPixel);
	TFPASS(v->getYScrollOffset() == y2);	/* 0.8px: still held */
	mouse.smoothScrollDeltas(v, 0.0, subPixel);
	/* 1.2px crosses a pixel: scrolls exactly one pixel */
	TFPASS(v->getYScrollOffset() == y2 + v->getGraphics()->tlu(1));

	/* horizontal smooth deltas take the same step and clamp at
	 * the left edge */
	TFPASS(v->getXScrollOffset() == 0);
	mouse.smoothScrollDeltas(v, 1.0, 0.0);
	const UT_sint32 xmoved = v->getXScrollOffset();
	TFPASS(xmoved <= step);
	TFPASS(xmoved >= step - v->getGraphics()->tlu(1));
	mouse.smoothScrollDeltas(v, -1.0, 0.0);
	TFPASS(v->getXScrollOffset() == 0);

	/* a huge delta clamps at the document bottom — no
	 * overshoot, no wrap toward the top */
	mouse.smoothScrollDeltas(v, 0.0, 1.0e5);
	const UT_sint32 maxY = UT_MAX(0, hv.layout->getHeight()
		- v->getWindowHeight());
	TFPASS(v->getYScrollOffset() == maxY);
	mouse.smoothScrollDeltas(v, 0.0, -1.0e5);
	TFPASS(v->getYScrollOffset() == 0);

	v->removeScrollListener(&scroller);
}
