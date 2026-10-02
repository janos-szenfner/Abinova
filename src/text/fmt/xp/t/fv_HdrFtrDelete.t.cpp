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
#include "pf_Frag_Strux.h"

#include <glib.h>
#include <gsf/gsf-output-stdio.h>

#include <cstdio>
#include <cstring>
#include <string>

#define TFSUITE "core.text.fmt.hdrftrdelete"

namespace {

/* Same widget-less stack as fv_FootnoteDelete/fv_ViewModes: painting
 * goes to a private cairo image surface and widget-dependent paths null
 * out, so the full layout machinery — including header/footer sections
 * and their shadow containers — can be exercised without a display. */
struct HeadlessHdrFtrView
{
	HeadlessHdrFtrView() = default;
	HeadlessHdrFtrView(const HeadlessHdrFtrView &) = delete;
	HeadlessHdrFtrView &operator=(const HeadlessHdrFtrView &) = delete;

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

	/* Drive any pending UT_Worker idle/timer sources (e.g. the HdrFtr
	 * auto-resize change timer) — they are main-loop sources, so they
	 * only fire when the context is pumped. */
	void pumpMainLoop(void)
	{
		for (int i = 0; i < 200 &&
			 g_main_context_iteration(nullptr, FALSE); i++)
		{
		}
	}

	UT_sint32 countDocSections(void) const
	{
		UT_sint32 iCount = 0;
		for (fl_DocSectionLayout * pDSL = layout->getFirstSection();
			 pDSL; pDSL = pDSL->getNextDocSection())
		{
			iCount++;
		}
		return iCount;
	}

	bool pageHasHdrFtr(int iPage, HdrFtrType hfType) const
	{
		fp_Page * pPage = layout->getNthPage(iPage);
		return pPage && pPage->getHdrFtrP(hfType) != nullptr;
	}

	~HeadlessHdrFtrView()
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

bool checkHdrFtrPdfFile(const char *path)
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

TFTEST_MAIN("header/footer insert, edit and remove round-trip")
{
	HeadlessHdrFtrView hv;
	TFPASS(hv.load("/test/wp/footer.abw"));
	if (!hv.view)
		return;

	std::string tmpPdf = std::string("/tmp/abn_hdrftrdel_") +
		std::to_string(::getpid()) + ".pdf";

	/* fixture ships a "Page N of M" footer shadowed on every page */
	TFPASS(hv.pageHasHdrFtr(0, FL_HDRFTR_FOOTER));

	/* remove it — fl_HdrFtrSectionLayout::doclistener_deleteStrux now
	 * defers destruction via FL_DocLayout::queueLayoutForDeletion; the
	 * rest of the dispatch (updateLayout + listener notify) must not
	 * touch the dying layout */
	hv.view->cmdRemoveHdrFtr(false);
	hv.pumpMainLoop();
	TFPASS(!hv.pageHasHdrFtr(0, FL_HDRFTR_FOOTER));

	hv.view->cmdUndo(1);
	hv.pumpMainLoop();
	TFPASS(hv.pageHasHdrFtr(0, FL_HDRFTR_FOOTER));
	hv.view->cmdRedo(1);
	hv.pumpMainLoop();
	TFPASS(!hv.pageHasHdrFtr(0, FL_HDRFTR_FOOTER));
	hv.view->cmdUndo(1);
	hv.pumpMainLoop();
	TFPASS(hv.pageHasHdrFtr(0, FL_HDRFTR_FOOTER));

	/* insert a header, enter edit mode, fill it with enough text to
	 * force the auto-resize path, then remove it — twice */
	for (int round = 0; round < 2; round++)
	{
		hv.view->insertHeaderFooter(FL_HDRFTR_HEADER);
		hv.pumpMainLoop();
		TFPASS(hv.pageHasHdrFtr(0, FL_HDRFTR_HEADER));
		TFPASS(hv.view->isHdrFtrEdit());

		/* enough wrapping text to grow the header past its fixed slot —
		 * arms the auto-resize worker (m_pHdrFtrChangeTimer) which the
		 * pump below fires, running _HdrFtrChangeCallback's rebuilt
		 * teardown ordering */
		const std::string sLine =
			"HEADER FILLER TEXT THAT WRAPS ACROSS SEVERAL LINES "
			"TO GROW THE HEADER PAST ITS SLOT ";
		for (int i = 0; i < 8; i++)
		{
			hv.view->cmdCharInsert(sLine, false);
		}
		hv.layout->formatAll();
		hv.pumpMainLoop();

		hv.view->clearHdrFtrEdit();
		hv.view->cmdRemoveHdrFtr(true);
		hv.pumpMainLoop();
		TFPASS(!hv.pageHasHdrFtr(0, FL_HDRFTR_HEADER));
	}

	/* the footer must have survived the header churn on every page */
	for (int i = 0; i < hv.layout->countPages(); i++)
	{
		TFPASS(hv.pageHasHdrFtr(i, FL_HDRFTR_FOOTER));
	}

	TFPASS(hv.exportPdf(tmpPdf.c_str()) == UT_OK);
	TFPASS(checkHdrFtrPdfFile(tmpPdf.c_str()));
}

TFTEST_MAIN("section break merge removes a DocSectionLayout cleanly")
{
	HeadlessHdrFtrView hv;
	TFPASS(hv.load("/test/wp/footer.abw"));
	if (!hv.view)
		return;

	std::string tmpPdf = std::string("/tmp/abn_sectdel_") +
		std::to_string(::getpid()) + ".pdf";

	/* give the doc a header so the DocSectionLayout being torn down
	 * owns live HdrFtr sections when it is deleted */
	hv.view->insertHeaderFooter(FL_HDRFTR_HEADER);
	hv.pumpMainLoop();
	TFPASS(hv.pageHasHdrFtr(0, FL_HDRFTR_HEADER));
	hv.view->clearHdrFtrEdit();

	PT_DocPosition posEnd = 0;
	hv.doc->getBounds(true, posEnd);
	TFPASS(posEnd > 4);
	hv.view->setPoint(posEnd / 2);

	UT_sint32 iSections = hv.countDocSections();
	TFPASS(iSections == 1);

	/* split into two doc sections, then undo — the merge back runs
	 * fl_DocSectionLayout::doclistener_deleteStrux which now queues the
	 * layout for deferred destruction instead of `delete this` inside
	 * the change-record callback */
	hv.view->insertSectionBreak();
	hv.pumpMainLoop();
	TFPASS(hv.countDocSections() == iSections + 1);

	hv.view->cmdUndo(1);
	hv.pumpMainLoop();
	TFPASS(hv.countDocSections() == iSections);

	hv.view->cmdRedo(1);
	hv.pumpMainLoop();
	TFPASS(hv.countDocSections() == iSections + 1);

	/* header still renders on all pages of both sections */
	for (int i = 0; i < hv.layout->countPages(); i++)
	{
		TFPASS(hv.pageHasHdrFtr(i, FL_HDRFTR_HEADER));
	}

	/* remove the header again — cmdRemoveHdrFtr acts on the current
	 * page's owning doc section, so park the point in the first one */
	hv.view->setPoint(posEnd / 4);
	hv.view->cmdRemoveHdrFtr(true);
	hv.pumpMainLoop();
	TFPASS(!hv.pageHasHdrFtr(0, FL_HDRFTR_HEADER));

	/* then two undos: first restores the header, second re-merges the
	 * split sections through the same deferred-teardown path */
	hv.view->cmdUndo(1);
	hv.pumpMainLoop();
	TFPASS(hv.pageHasHdrFtr(0, FL_HDRFTR_HEADER));
	hv.view->cmdUndo(1);
	hv.pumpMainLoop();
	TFPASS(hv.countDocSections() == iSections);

	TFPASS(hv.exportPdf(tmpPdf.c_str()) == UT_OK);
	TFPASS(checkHdrFtrPdfFile(tmpPdf.c_str()));
}
