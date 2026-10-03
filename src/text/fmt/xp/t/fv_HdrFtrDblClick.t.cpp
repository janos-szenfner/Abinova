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
#include "fp_Page.h"
#include "fp_Column.h"
#include "fv_View.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "ie_exp.h"
#include "ie_types.h"
#include "ut_types.h"

#include <glib.h>
#include <gsf/gsf-output-stdio.h>

#include <cstdio>
#include <cstring>
#include <string>

#define TFSUITE "core.text.fmt.hdrftrdblclick"

namespace {

/* Same widget-less stack as fv_HdrFtrDelete/fv_ViewModes: painting goes
 * to a private cairo image surface so the full layout machinery —
 * including header/footer shadow containers — works headless. */
struct HeadlessDblClickView
{
	HeadlessDblClickView() = default;
	HeadlessDblClickView(const HeadlessDblClickView &) = delete;
	HeadlessDblClickView &operator=(const HeadlessDblClickView &) = delete;

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

	UT_Error exportPdf(const char *path)
	{
		GError *err = nullptr;
		GsfOutput *out = gsf_output_stdio_new(path, &err);
		if (!out)
		{
			g_clear_error(&err);
			return UT_ERROR;
		}
		UT_Error rc = doc->saveAs(out, IE_Exp::fileTypeForSuffix(".pdf"),
								  false, nullptr);
		g_object_unref(out);
		return rc;
	}

	void pumpMainLoop(void)
	{
		for (int i = 0; i < 200 &&
			 g_main_context_iteration(nullptr, FALSE); i++)
		{
		}
	}

	/* centre of the page's header/footer shadow container, in view
	 * (logical) coordinates the way the mouse layer delivers them */
	bool shadowCentre(int iPage, HdrFtrType hfType,
					  UT_sint32 &xView, UT_sint32 &yView) const
	{
		fp_Page * pPage = layout->getNthPage(iPage);
		fp_ShadowContainer * pCon =
			pPage ? pPage->getHdrFtrP(hfType) : nullptr;
		if (!pCon)
			return false;
		UT_sint32 xoff = 0, yoff = 0;
		view->getPageScreenOffsets(pPage, xoff, yoff);
		xView = xoff + pCon->getX() + pCon->getWidth() / 2;
		yView = yoff + pCon->getY() + pCon->getHeight() / 2;
		return true;
	}

	void bodyCentre(int iPage, UT_sint32 &xView, UT_sint32 &yView) const
	{
		fp_Page * pPage = layout->getNthPage(iPage);
		UT_sint32 xoff = 0, yoff = 0;
		view->getPageScreenOffsets(pPage, xoff, yoff);
		xView = xoff + pPage->getWidth() / 2;
		yView = yoff + pPage->getHeight() / 2;
	}

	~HeadlessDblClickView()
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

bool checkDblClickPdfFile(const char *path)
{
	FILE *fp = fopen(path, "rb");
	if (!fp)
		return false;
	char magic[5] = {};
	bool ok = fread(magic, 1, 5, fp) == 5 && !memcmp(magic, "%PDF-", 5);
	fclose(fp);
	unlink(path);
	return ok;
}

HdrFtrType editShadowType(FV_View *view)
{
	fl_HdrFtrShadow * pShadow = view->getEditShadow();
	if (!pShadow || !pShadow->getHdrFtrSectionLayout())
		return FL_HDRFTR_NONE;
	return pShadow->getHdrFtrSectionLayout()->getHFType();
}

}

TFTEST_MAIN("double-click enters, switches and leaves hdrftr edit")
{
	HeadlessDblClickView hv;
	TFPASS(hv.load("/test/wp/footer.abw"));
	if (!hv.view)
		return;

	/* the fixture ships a "Page N of M" footer shadow on every page */
	UT_sint32 fx = 0, fy = 0, bx = 0, by = 0;
	TFPASS(hv.shadowCentre(0, FL_HDRFTR_FOOTER, fx, fy));
	hv.bodyCentre(0, bx, by);

	std::string tmpPdf = std::string("/tmp/abn_dblclk_") +
		std::to_string(::getpid()) + ".pdf";

	TFPASS(!hv.view->isHdrFtrEdit());
	TFPASS(hv.view->getViewMode() == VIEW_PRINT);

	/* a plain click inside the footer shadow region must NOT enter
	 * edit mode — entry is the double-click gesture's job (Word) */
	hv.view->warpInsPtToXY(fx, fy, true);
	TFPASS(!hv.view->isHdrFtrEdit());

	/* a double-click in the body while not editing is a no-op for the
	 * mode state and falls through to the bound select method */
	TFPASS(!hv.view->cmdDoubleClick(bx, by));
	TFPASS(!hv.view->isHdrFtrEdit());

	/* double-click inside the footer shadow enters footer editing and
	 * lands the insertion point inside the hdrftr region */
	TFPASS(hv.view->cmdDoubleClick(fx, fy));
	TFPASS(hv.view->isHdrFtrEdit());
	TFPASS(hv.view->getEditShadow() != nullptr);
	TFPASS(editShadowType(hv.view) == FL_HDRFTR_FOOTER);
	TFPASS(hv.view->isInHdrFtr(hv.view->getPoint()));

	/* a second double-click inside the region being edited is not
	 * consumed — it keeps its normal word-select meaning */
	TFPASS(!hv.view->cmdDoubleClick(fx, fy));
	TFPASS(hv.view->isHdrFtrEdit());

	/* double-click in the body exits back */
	TFPASS(!hv.view->cmdDoubleClick(bx, by));
	TFPASS(!hv.view->isHdrFtrEdit());

	/* single click in the body while editing also exits (existing
	 * affordance kept) */
	TFPASS(hv.view->cmdDoubleClick(fx, fy));
	TFPASS(hv.view->isHdrFtrEdit());
	hv.view->warpInsPtToXY(bx, by, true);
	TFPASS(!hv.view->isHdrFtrEdit());
}

TFTEST_MAIN("double-click switches between header and footer shadows")
{
	HeadlessDblClickView hv;
	TFPASS(hv.load("/test/wp/footer.abw"));
	if (!hv.view)
		return;

	/* add a header so both regions exist, then leave edit mode */
	hv.view->insertHeaderFooter(FL_HDRFTR_HEADER);
	hv.pumpMainLoop();
	TFPASS(hv.view->isHdrFtrEdit());
	hv.view->clearHdrFtrEdit();
	TFPASS(!hv.view->isHdrFtrEdit());

	UT_sint32 hx = 0, hy = 0, fx = 0, fy = 0;
	TFPASS(hv.shadowCentre(0, FL_HDRFTR_HEADER, hx, hy));
	TFPASS(hv.shadowCentre(0, FL_HDRFTR_FOOTER, fx, fy));

	/* double-click the header region -> header edit context */
	TFPASS(hv.view->cmdDoubleClick(hx, hy));
	TFPASS(hv.view->isHdrFtrEdit());
	TFPASS(editShadowType(hv.view) == FL_HDRFTR_HEADER);

	/* double-click the footer region -> switches edit context */
	TFPASS(hv.view->cmdDoubleClick(fx, fy));
	TFPASS(hv.view->isHdrFtrEdit());
	TFPASS(editShadowType(hv.view) == FL_HDRFTR_FOOTER);

	/* and back to the body */
	UT_sint32 bx = 0, by = 0;
	hv.bodyCentre(0, bx, by);
	TFPASS(!hv.view->cmdDoubleClick(bx, by));
	TFPASS(!hv.view->isHdrFtrEdit());

	TFPASS(hv.exportPdf(std::string("/tmp/abn_dblclk2_" +
		std::to_string(::getpid()) + ".pdf").c_str()) == UT_OK);
	TFPASS(checkDblClickPdfFile((std::string("/tmp/abn_dblclk2_") +
		std::to_string(::getpid()) + ".pdf").c_str()));
}

TFTEST_MAIN("no shadow region means no mode toggle")
{
	HeadlessDblClickView hv;
	TFPASS(hv.load("/test/wp/footer.abw"));
	if (!hv.view)
		return;

	/* remove the footer so the page has no hdrftr regions at all */
	hv.view->cmdRemoveHdrFtr(false);
	hv.pumpMainLoop();
	fp_Page * pPage = hv.layout->getNthPage(0);
	TFPASS(pPage && pPage->getHdrFtrP(FL_HDRFTR_FOOTER) == nullptr);

	/* double-clicking the bottom margin area now stays a normal click */
	UT_sint32 xoff = 0, yoff = 0;
	hv.view->getPageScreenOffsets(pPage, xoff, yoff);
	UT_sint32 mx = xoff + pPage->getWidth() / 2;
	UT_sint32 my = yoff + pPage->getHeight() - hv.graphics->tlu(10);
	TFPASS(!hv.view->cmdDoubleClick(mx, my));
	TFPASS(!hv.view->isHdrFtrEdit());
}
