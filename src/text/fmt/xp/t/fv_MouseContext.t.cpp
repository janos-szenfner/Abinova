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

#include "tf_test.h"

#include "pd_Document.h"
#include "fl_DocLayout.h"
#include "fl_SectionLayout.h"
#include "fl_FrameLayout.h"
#include "fp_Page.h"
#include "fp_Column.h"
#include "fp_Line.h"
#include "fp_FrameContainer.h"
#include "fp_TableContainer.h"
#include "fv_View.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "ut_types.h"

#include <glib.h>

#include <cstdio>
#include <cstring>
#include <string>

#define TFSUITE "core.text.fmt.mousecontext"

namespace {

/* Same widget-less stack as fv_HdrFtrDblClick/fv_ViewModes: painting
 * goes to a private cairo image surface so the full layout machinery —
 * including frame containers — works headless. */
struct HeadlessMouseView
{
	HeadlessMouseView() = default;
	HeadlessMouseView(const HeadlessMouseView &) = delete;
	HeadlessMouseView &operator=(const HeadlessMouseView &) = delete;

	bool open(const char *relPath)
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
		return true;
	}

	bool format()
	{
		layout->fillLayouts();
		layout->formatAll();
		return layout->countPages() > 0;
	}

	/* gather every frame container on the page, above- and
	 * below-text */
	void collectFrames(fp_Page *pPage,
					   std::vector<fp_FrameContainer *> &frames) const
	{
		for (UT_sint32 i = 0; i < pPage->countAboveFrameContainers(); i++)
			frames.push_back(pPage->getNthAboveFrameContainer(i));
		for (UT_sint32 i = 0; i < pPage->countBelowFrameContainers(); i++)
			frames.push_back(pPage->getNthBelowFrameContainer(i));
	}

	fp_Line * findFirstLine(fp_ContainerObject *pCon) const
	{
		if (!pCon)
			return nullptr;
		if (pCon->getContainerType() == FP_CONTAINER_LINE)
			return static_cast<fp_Line *>(pCon);
		fp_Container *pCtr = dynamic_cast<fp_Container *>(pCon);
		if (!pCtr)
			return nullptr;
		for (UT_sint32 i = 0; i < pCtr->countCons(); i++)
		{
			fp_Line *pLine = findFirstLine(pCtr->getNthCon(i));
			if (pLine)
				return pLine;
		}
		return nullptr;
	}

	~HeadlessMouseView()
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

} // namespace

TFTEST_MAIN("no layouts and off-page clicks give UNKNOWN")
{
	HeadlessMouseView hv;
	TFPASS(hv.open("/test/wp/frame.abw"));
	if (!hv.view)
		return;

	/* before fillLayouts the view has no pages and no insertion
	 * point — a click cannot map to a context */
	TFPASS(hv.view->getMouseContext(500, 500) == EV_EMC_UNKNOWN);

	TFPASS(hv.format());

	/* far outside every page rectangle */
	TFPASS(hv.view->getMouseContext(-5000, -5000) == EV_EMC_UNKNOWN);
}

TFTEST_MAIN("frame borders and interiors resolve real contexts")
{
	HeadlessMouseView hv;
	TFPASS(hv.open("/test/wp/frame.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());

	fp_Page *pPage = hv.layout->getNthPage(0);
	TFPASS(pPage != nullptr);

	std::vector<fp_FrameContainer *> frames;
	hv.collectFrames(pPage, frames);
	TFPASS(frames.size() == 2);

	UT_sint32 xoff = 0, yoff = 0;
	hv.view->getPageScreenOffsets(pPage, xoff, yoff);

	/* the text frame is the smaller of the two (2in x 1in vs
	 * 3in x 1.5in) */
	fp_FrameContainer *pTextFrame = frames[0];
	fp_FrameContainer *pTableFrame = frames[1];
	if (pTextFrame->getFullWidth() > pTableFrame->getFullWidth())
		std::swap(pTextFrame, pTableFrame);
	TFPASS(hv.findFirstLine(pTableFrame) != nullptr);

	/* just inside the frame's left edge — within the 40-unit border
	 * band _getMouseContext tests — must report the frame context */
	UT_sint32 bx = xoff + pTextFrame->getFullX() + 1;
	UT_sint32 by = yoff + pTextFrame->getFullY() +
		pTextFrame->getFullHeight() / 2;
	TFPASS(hv.view->getMouseContext(bx, by) == EV_EMC_FRAME);

	/* the frame's interior text is a normal text context */
	UT_sint32 cx = xoff + pTextFrame->getFullX() +
		pTextFrame->getFullWidth() / 2;
	UT_sint32 cy = yoff + pTextFrame->getFullY() +
		pTextFrame->getFullHeight() / 2;
	TFPASS(hv.view->getMouseContext(cx, cy) == EV_EMC_TEXT);

	/* a body click away from all frames resolves to text, not the
	 * frame context */
	UT_sint32 mx = xoff + pPage->getWidth() / 2;
	UT_sint32 my = yoff + pPage->getHeight() * 4 / 5;
	TFPASS(hv.view->getMouseContext(mx, my) == EV_EMC_TEXT);
}

TFTEST_MAIN("click inside a table nested in a frame")
{
	HeadlessMouseView hv;
	TFPASS(hv.open("/test/wp/frame.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());

	fp_Page *pPage = hv.layout->getNthPage(0);
	TFPASS(pPage != nullptr);

	std::vector<fp_FrameContainer *> frames;
	hv.collectFrames(pPage, frames);
	TFPASS(frames.size() == 2);

	fp_FrameContainer *pTableFrame =
		frames[0]->getFullWidth() > frames[1]->getFullWidth()
		? frames[0] : frames[1];

	UT_sint32 xoff = 0, yoff = 0;
	hv.view->getPageScreenOffsets(pPage, xoff, yoff);

	/* click on cell text inside the nested table.  pos resolves
	 * inside the cell, so the frame border test must walk the
	 * containing-layout chain up from the table layout to the real
	 * frame layout — the old code cast the table layout itself to
	 * fl_FrameLayout and read garbage geometry. */
	UT_sint32 bx = xoff + pTableFrame->getFullX() + 1;
	UT_sint32 by = yoff + pTableFrame->getFullY() +
		pTableFrame->getFullHeight() / 2;
	TFPASS(hv.view->getMouseContext(bx, by) == EV_EMC_FRAME);

	/* the centre of the first cell's line is far from both the frame
	 * border and the cell borders — a plain table context */
	fp_Line *pLine = hv.findFirstLine(pTableFrame);
	TFPASS(pLine != nullptr);
	if (pLine)
	{
		std::optional<UT_Rect> rect = pLine->getScreenRect();
		TFPASS(rect.has_value());
		if (rect)
		{
			UT_sint32 lx = rect->left + rect->width / 2;
			UT_sint32 ly = rect->top + rect->height / 2;
			TFPASS(hv.view->getMouseContext(lx, ly) == EV_EMC_TABLE);
		}
	}
}
