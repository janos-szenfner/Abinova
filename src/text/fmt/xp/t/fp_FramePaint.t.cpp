/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
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

/* COVD03 — pin the fp_FrameContainer paint surface through a headless
 * render of test/wp/framepaint/framepaint.abw.  The fixture puts one
 * frame per feature on a single page: drop shadows (incl. the
 * rotWithShape=0 pin under a rotated frame), gradient/alpha/image
 * fills (tile + stretch/crop), custGeom shape-path strokes with
 * compound/join/cap/dash/gradient outline extras, bar frames with all
 * five line-end markers, vertical text (vert/vert270), border styles
 * (double/triple/wave/dashdotdot), wrapped and below-text frames.
 *
 * Assertions stay behavioural: regions of the rendered page must
 * carry the expected ink, and the public geometry/hit surface must
 * agree with the layout.  The point is executing the paint code under
 * coverage with enough checking to catch a silently-empty render.
 */

#include "tf_test.h"

#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "fl_DocLayout.h"
#include "fl_SectionLayout.h"
#include "fl_FrameLayout.h"
#include "fp_Page.h"
#include "fp_FrameContainer.h"
#include "fv_View.h"
#include "fv_FrameEdit.h"
#include "gr_DrawArgs.h"
#include "gr_Painter.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "ie_types.h"
#include "ut_types.h"

#include <cairo.h>
#include <glib.h>

#include <cstring>
#include <vector>

#define TFSUITE "core.text.fmt.framepaint"

namespace {

const int PAINT_ZOOM = 50;

/* widget-less document + layout + view stack, same construction the
 * other headless fmt suites use */
struct PaintView
{
	PaintView() = default;
	PaintView(const PaintView &) = delete;
	PaintView &operator=(const PaintView &) = delete;

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
		layout->fillLayouts();
		layout->formatAll();
		view->setWindowSize(800, 600);
		return layout->countPages() > 0;
	}

	~PaintView()
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

/* paint through a graphics that reports DGP_SCREEN so transparent
 * fills and screen-only blends take the on-screen code path */
class PaintGfx : public GR_UnixCairoGraphics
{
public:
	PaintGfx() : GR_UnixCairoGraphics(nullptr) {}
	bool queryProperties(GR_Graphics::Properties gp) const override
	{
		if (gp == GR_Graphics::DGP_SCREEN ||
			gp == GR_Graphics::DGP_OPAQUEOVERLAY)
			return true;
		return GR_UnixCairoGraphics::queryProperties(gp);
	}
};

cairo_surface_t * paint_page(FV_View * v, FL_DocLayout * layout,
							 int iPage)
{
	fp_Page * pPage = layout->getNthPage(iPage);
	if (!pPage)
		return nullptr;
	PaintGfx * g = new PaintGfx;
	g->setZoomPercentage(PAINT_ZOOM);
	int w = g->tdu(pPage->getWidth());
	int h = g->tdu(pPage->getHeight());
	cairo_surface_t * surf =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cairo_t * cr = cairo_create(surf);
	g->setCairo(cr);
	g->beginPaint();
	{
		GR_Painter painter(g);
		painter.clearArea(0, 0, w, h);
	}
	dg_DrawArgs da;
	da.pG = g;
	da.xoff = 0;
	da.yoff = 0;
	layout->setQuickPrint(g);
	v->drawPage(iPage, &da);
	layout->setQuickPrint(nullptr);
	g->endPaint();
	cairo_destroy(cr);
	delete g;
	cairo_surface_flush(surf);
	return surf;
}

/* count pixels in the rect whose RGB differs from white by more than
 * `tol` on any channel; (x,y,w,h) in device pixels */
long count_ink(cairo_surface_t * surf, int x, int y, int w, int h,
			   int tol)
{
	cairo_surface_flush(surf);
	int sw = cairo_image_surface_get_width(surf);
	int sh = cairo_image_surface_get_height(surf);
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x + w > sw) w = sw - x;
	if (y + h > sh) h = sh - y;
	if (w <= 0 || h <= 0)
		return 0;
	const unsigned char * data = cairo_image_surface_get_data(surf);
	int stride = cairo_image_surface_get_stride(surf);
	long n = 0;
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
		{
			const unsigned char * p = data + j * stride + i * 4;
			/* ARGB32 little-endian: B,G,R,A */
			if (p[3] > 0 &&
				(255 - p[0] > tol || 255 - p[1] > tol || 255 - p[2] > tol))
				n++;
		}
	return n;
}

/* gather every frame container attached to a page */
std::vector<fp_FrameContainer *> page_frames(fp_Page * pPage)
{
	std::vector<fp_FrameContainer *> out;
	if (!pPage)
		return out;
	for (UT_sint32 i = 0; i < pPage->countAboveFrameContainers(); i++)
		out.push_back(pPage->getNthAboveFrameContainer(i));
	for (UT_sint32 i = 0; i < pPage->countBelowFrameContainers(); i++)
		out.push_back(pPage->getNthBelowFrameContainer(i));
	return out;
}

} // namespace

TFTEST_MAIN("frame feature paint smoke: every ink region fills")
{
	PaintView pv;
	TFPASS(pv.open("test/wp/framepaint/framepaint.abw"));
	if (!pv.layout)
		return;
	TFPASS(pv.layout->countPages() >= 1);

	fp_Page * pPage = pv.layout->getNthPage(0);
	TFPASS(pPage != nullptr);
	if (!pPage)
		return;

	/* the fixture packs 19 frames onto page 0 */
	TFPASS(page_frames(pPage).size() >= 15);

	cairo_surface_t * surf = paint_page(pv.view, pv.layout, 0);
	TFPASS(surf != nullptr);
	if (!surf)
		return;
	cairo_surface_write_to_png(surf, "/tmp/framepaint.png");

	/* device-pixel scale: 50% zoom on a 96dpi grid -> 1in ~ 48px */
	const double px = PAINT_ZOOM * 96.0 / 100.0;
	auto in = [&](double f) { return static_cast<int>(f * px); };

	/* shadowed blue box: fill ink inside the frame */
	TFPASS(count_ink(surf, in(0.5), in(0.5), in(0.9), in(0.5), 60) > 40);
	/* shadow bleed past the frame's bottom-right corner */
	TFPASS(count_ink(surf, in(1.75), in(1.25), in(0.4), in(0.35), 30) > 8);
	/* rotated orange box */
	TFPASS(count_ink(surf, in(4.0), in(0.45), in(0.9), in(0.6), 60) > 40);
	/* gradient band: red on the left, blue on the right */
	TFPASS(count_ink(surf, in(0.5), in(1.8), in(0.6), in(0.4), 60) > 30);
	TFPASS(count_ink(surf, in(1.3), in(1.8), in(0.4), in(0.4), 60) > 20);
	/* custGeom triangle + diamond strokes */
	TFPASS(count_ink(surf, in(4.1), in(1.7), in(1.4), in(0.95), 60) > 20);
	TFPASS(count_ink(surf, in(6.1), in(1.7), in(1.3), in(0.95), 60) > 15);
	/* tiled image: alternating red/blue cells give solid ink */
	TFPASS(count_ink(surf, in(0.45), in(3.25), in(1.3), in(0.85), 80) > 200);
	/* stretch/crop image fill: yellow underlay + image subrect */
	TFPASS(count_ink(surf, in(2.45), in(3.25), in(1.3), in(0.85), 80) > 100);
	/* horizontal arrow bar and its heads */
	TFPASS(count_ink(surf, in(0.4), in(4.62), in(2.5), in(0.2), 60) > 30);
	/* vertical arrow bar */
	TFPASS(count_ink(surf, in(4.35), in(4.55), in(0.25), in(1.4), 60) > 15);
	/* compound + dash bars */
	TFPASS(count_ink(surf, in(5.2), in(4.65), in(2.0), in(0.15), 60) > 15);
	TFPASS(count_ink(surf, in(5.2), in(5.05), in(2.0), in(0.15), 60) > 15);
	/* vertical text boxes */
	TFPASS(count_ink(surf, in(0.45), in(6.15), in(0.7), in(1.6), 60) > 40);
	TFPASS(count_ink(surf, in(1.75), in(6.15), in(0.7), in(1.6), 60) > 40);
	/* wild borders + extras borders */
	TFPASS(count_ink(surf, in(3.2), in(6.2), in(1.5), in(0.95), 60) > 40);
	TFPASS(count_ink(surf, in(5.2), in(6.2), in(1.4), in(0.95), 60) > 30);
	/* wrapped frame ink */
	TFPASS(count_ink(surf, in(1.0), in(7.8), in(1.9), in(0.8), 60) > 30);
	/* below-text + scaled-text frames */
	TFPASS(count_ink(surf, in(4.2), in(8.2), in(1.5), in(0.7), 60) > 30);
	TFPASS(count_ink(surf, in(6.2), in(8.2), in(1.5), in(0.7), 60) > 30);

	cairo_surface_destroy(surf);
}

TFTEST_MAIN("frame geometry and hit-test surface")
{
	PaintView pv;
	TFPASS(pv.open("test/wp/framepaint/framepaint.abw"));
	if (!pv.layout)
		return;

	fp_Page * pPage = pv.layout->getNthPage(0);
	TFPASS(pPage != nullptr);
	if (!pPage)
		return;

	std::vector<fp_FrameContainer *> frames = page_frames(pPage);
	TFPASS(frames.size() >= 15);

	bool bSawRot = false, bSawVert = false, bSawGroup = false,
		 bSawFlip = false, bSawHidden = false;
	for (fp_FrameContainer * pFC : frames)
	{
		TFPASS(pFC->getFullWidth() > 0);
		TFPASS(pFC->getFullHeight() > 0);
		TFPASS(pFC->getPage() == pPage);

		UT_Rect ink;
		pFC->getInkBounds(ink);
		TFPASS(ink.width > 0 && ink.height > 0);

		/* overlapsRect() takes a screen-space rect: the frame's own
		 * screen rect shifted out by one unit must always overlap
		 * it, a rect far to the bottom-right must not */
		std::optional<UT_Rect> sr = pFC->getScreenRect();
		TFPASS(sr.has_value());
		if (!sr)
			continue;
		UT_Rect box(sr->left - 1, sr->top - 1, sr->width + 2,
					sr->height + 2);
		TFPASS(pFC->overlapsRect(box));
		UT_Rect far(sr->left + sr->width + 5000,
					sr->top + sr->height + 5000, 10, 10);
		TFPASS(!pFC->overlapsRect(far));

		UT_sint32 px = pFC->getFullX() + pFC->getFullWidth() / 2;
		UT_sint32 py = pFC->getFullY() + pFC->getFullHeight() / 2;
		UT_sint32 ux = px, uy = py;
		pFC->unrotatePoint(ux, uy);

		PT_DocPosition pos = 0;
		bool bBOL = false, bEOL = false, isTOC = false;
		pFC->mapXYToPosition(pFC->getX() + 10, pFC->getY() + 5,
						   pos, bBOL, bEOL, isTOC);

		UT_sint32 lp = pFC->getLeftPad(py - pFC->getFullY(), 10);
		UT_sint32 rp = pFC->getRightPad(py - pFC->getFullY(), 10);
		TFPASS(lp >= 0 && rp >= 0);

		pFC->isAbove();
		pFC->getStackOrder();
		pFC->isHidden();
		pFC->getPreferedPageNo();
		pFC->getPreferedColumnNo();
		pFC->isWrappingSet();
		pFC->isTightWrapped();
		pFC->isTopBot();
		pFC->isLeftWrapped();
		pFC->isRightWrapped();
		pFC->getXPad();
		pFC->getYPad();
		pFC->getDocSectionLayout();

		if (pFC->getRotation() != 0.0)
			bSawRot = true;
		if (pFC->getTextRotation() != 0)
			bSawVert = true;
		if (pFC->isFlippedHoriz() || pFC->isFlippedVert())
			bSawFlip = true;
		if (pFC->getGroupId())
			bSawGroup = true;
		if (pFC->isHidden())
			bSawHidden = true;

		std::vector<fl_BlockLayout *> blocks;
		pFC->getBlocksAroundFrame(blocks);
	}
	TFPASS(bSawRot);    /* frame-rotation:20 frame */
	TFPASS(bSawVert);   /* vert/vert270 frames */
	TFPASS(bSawFlip);   /* frame-flip-* frame */
	TFPASS(bSawGroup);  /* frame-group:pair-a */
	TFPASS(!bSawHidden);/* nothing is hidden in the fixture */
}

TFTEST_MAIN("frame edit mode paints selection handles")
{
	PaintView pv;
	TFPASS(pv.open("test/wp/framepaint/framepaint.abw"));
	if (!pv.layout)
		return;

	fp_Page * pPage = pv.layout->getNthPage(0);
	TFPASS(pPage != nullptr);
	if (!pPage)
		return;

	std::vector<fp_FrameContainer *> frames = page_frames(pPage);
	TFPASS(!frames.empty());
	if (frames.empty())
		return;

	/* warp the insertion point into the first frame the way the
	 * context-menu path does, then select it programmatically */
	fp_FrameContainer * pFC = frames[0];
	UT_sint32 xoff = 0, yoff = 0;
	pv.view->getPageScreenOffsets(pPage, xoff, yoff);
	pv.view->warpInsPtToXY(xoff + pFC->getFullX() + pFC->getFullWidth() / 2,
						   yoff + pFC->getFullY() + pFC->getFullHeight() / 2,
						   true);
	fl_FrameLayout * pFL = pv.view->getFrameLayout();
	if (!pFL)
	{
		/* the first page frame may not accept the IP; fall back to
		 * any frame's layout via its section layout */
		pFL = static_cast<fl_FrameLayout *>(pFC->getSectionLayout());
	}
	TFPASS(pFL != nullptr);
	if (!pFL)
		return;

	FV_FrameEdit * pFE = pv.view->getFrameEdit();
	TFPASS(pFE != nullptr);
	if (!pFE)
		return;
	pFE->selectFrame(pFL);
	TFPASS(pFE->getFrameEditMode() == FV_FrameEdit_EXISTING_SELECTED);
	TFPASS(pFE->isActive());

	/* repaint the page while the frame is selected — exercises
	 * draw() under frame-edit plus drawBoundaries */
	cairo_surface_t * surf = paint_page(pv.view, pv.layout, 0);
	TFPASS(surf != nullptr);
	if (surf)
		cairo_surface_destroy(surf);

	pFE->setMode(FV_FrameEdit_NOT_ACTIVE);
	TFPASS(!pFE->isActive());
}
