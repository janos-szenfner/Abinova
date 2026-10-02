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
#include "fv_View.h"
#include "fp_Page.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "ie_exp.h"
#include "ie_types.h"

#include <gsf/gsf-output-stdio.h>

#include <cstdio>
#include <string>

#define TFSUITE "core.text.fmt.viewmodes"

namespace {

const char *FIXTURE = "/test/wp/BillOfRights.abw";

/* A document with a full interactive layout stack on a widget-less
 * GR_UnixCairoGraphics — no display needed: painting goes to a private
 * cairo image surface and every widget-dependent path nulls out. */
struct HeadlessView
{
	HeadlessView() = default;
	HeadlessView(const HeadlessView &) = delete;
	HeadlessView &operator=(const HeadlessView &) = delete;

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

	/* Mirrors the frame resize path in xap_UnixFrameImpl: update the
	 * view's window size, and in web layout re-derive the page width
	 * and re-paginate. */
	void resize(UT_sint32 w, UT_sint32 h)
	{
		view->setWindowSize(w, h);
		if (view->getViewMode() == VIEW_WEB)
		{
			layout->syncWebPageSizeToWindow();
			view->rebuildLayout();
			view->updateScreen(false);
		}
	}

	UT_sint32 pageWidth() const
	{
		return layout->getNthPage(0)->getWidth();
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

	~HeadlessView()
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

bool checkPdfFile(const char *path)
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

}

TFTEST_MAIN("view mode toggles reflow, restore pagination and still export")
{
	HeadlessView hv;
	TFPASS(hv.load(FIXTURE));
	if (!hv.view)
		return;

	std::string tmpPdf = std::string("/tmp/abn_viewmodes_") +
		std::to_string(::getpid()) + ".pdf";

	/* baseline print layout */
	TFPASS(hv.view->getViewMode() == VIEW_PRINT);
	const UT_sint32 printPages = hv.layout->countPages();
	const UT_sint32 printWidth = hv.pageWidth();
	TFPASS(printPages >= 2);	/* the fixture must be multi-page */
	TFPASS(printWidth > 0);
	TFPASS(hv.exportPdf(tmpPdf.c_str()) == UT_OK);
	TFPASS(checkPdfFile(tmpPdf.c_str()));

	/* entering web layout at a wide window must re-derive the page
	 * width from the window and re-paginate — entering used to only
	 * run updateColumnX() and leave the print pagination in place */
	hv.resize(1400, 900);
	hv.view->setViewMode(VIEW_WEB);
	TFPASS(hv.view->getViewMode() == VIEW_WEB);
	const UT_sint32 webWidePages = hv.layout->countPages();
	const UT_sint32 webWideWidth = hv.pageWidth();
	TFPASS(webWideWidth > printWidth);
	TFPASS(webWidePages <= printPages);
	TFPASS(webWidePages >= 1);
	TFPASS(hv.exportPdf(tmpPdf.c_str()) == UT_OK);
	TFPASS(checkPdfFile(tmpPdf.c_str()));

	/* shrinking the window while in web layout reflows again */
	hv.resize(500, 900);
	const UT_sint32 webNarrowWidth = hv.pageWidth();
	const UT_sint32 webNarrowPages = hv.layout->countPages();
	TFPASS(webNarrowWidth < webWideWidth);
	TFPASS(webNarrowPages >= webWidePages);
	TFPASS(hv.exportPdf(tmpPdf.c_str()) == UT_OK);
	TFPASS(checkPdfFile(tmpPdf.c_str()));

	/* normal layout */
	hv.view->setViewMode(VIEW_NORMAL);
	TFPASS(hv.view->getViewMode() == VIEW_NORMAL);
	TFPASS(hv.layout->countPages() >= 1);

	/* back through web to print — the original pagination and page
	 * geometry must be restored exactly */
	hv.view->setViewMode(VIEW_WEB);
	hv.view->setViewMode(VIEW_PRINT);
	TFPASS(hv.view->getViewMode() == VIEW_PRINT);
	TFPASS(hv.layout->countPages() == printPages);
	TFPASS(hv.pageWidth() == printWidth);
	TFPASS(hv.exportPdf(tmpPdf.c_str()) == UT_OK);
	TFPASS(checkPdfFile(tmpPdf.c_str()));
}
