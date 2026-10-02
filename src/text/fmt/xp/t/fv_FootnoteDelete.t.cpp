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
#include "fl_FootnoteLayout.h"
#include "fv_View.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "ie_exp.h"
#include "ie_types.h"
#include "pf_Frag_Strux.h"

#include <gsf/gsf-output-stdio.h>

#include <cstdio>
#include <cstring>
#include <string>

#define TFSUITE "core.text.fmt.footnotedelete"

namespace {

/* Same widget-less stack as fv_ViewModes: painting goes to a private
 * cairo image surface and widget-dependent paths null out, so the full
 * layout machinery — including footnote/annotation containers — can be
 * exercised without a display. */
struct HeadlessNoteView
{
	HeadlessNoteView() = default;
	HeadlessNoteView(const HeadlessNoteView &) = delete;
	HeadlessNoteView &operator=(const HeadlessNoteView &) = delete;

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

	~HeadlessNoteView()
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

bool checkNotePdfFile(const char *path)
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

TFTEST_MAIN("footnote insert, mid-document delete and undo round-trip")
{
	HeadlessNoteView hv;
	TFPASS(hv.load("/test/wp/BillOfRights.abw"));
	if (!hv.view)
		return;

	std::string tmpPdf = std::string("/tmp/abn_footdel_") +
		std::to_string(::getpid()) + ".pdf";

	PT_DocPosition posEnd = 0;
	hv.doc->getBounds(true, posEnd);
	TFPASS(posEnd > 4);

	/* insert a footnote mid-document, then undo and redo it */
	hv.view->setPoint(posEnd / 2);
	TFPASS(hv.layout->countFootnotes() == 0);
	TFPASS(hv.view->insertFootnote(true));
	TFPASS(hv.layout->countFootnotes() == 1);
	hv.view->cmdUndo(1);
	TFPASS(hv.layout->countFootnotes() == 0);
	hv.view->cmdRedo(1);
	TFPASS(hv.layout->countFootnotes() == 1);

	/* delete the footnote by deleting its anchor character, like a
	 * user pressing Del just before the reference mark: the delete
	 * expands over the whole footnote section and must not crash or
	 * corrupt the layout when the fl_FootnoteLayout is destroyed */
	fl_FootnoteLayout * pFL = hv.layout->getNthFootnote(0);
	TFPASS(pFL != nullptr);
	TFPASS(pFL->getStruxDocHandle() != nullptr);
	PT_DocPosition posAnchor =
		hv.doc->getStruxPosition(pFL->getStruxDocHandle()) - 1;
	hv.view->setPoint(posAnchor);
	hv.view->cmdCharDelete(true, 1);
	TFPASS(hv.layout->countFootnotes() == 0);
	hv.view->cmdUndo(1);
	TFPASS(hv.layout->countFootnotes() == 1);
	hv.view->cmdRedo(1);
	TFPASS(hv.layout->countFootnotes() == 0);

	/* annotation insert + delete + undo exercises the same deferred
	 * layout teardown path (fl_EmbedLayout) */
	hv.view->setPoint(posEnd / 2);
	TFPASS(hv.view->cmdInsertComment());
	TFPASS(hv.layout->countAnnotations() == 1);
	fl_AnnotationLayout * pAL = hv.layout->getNthAnnotation(0);
	TFPASS(pAL != nullptr);
	TFPASS(hv.view->delAnnotationLayout(pAL));
	TFPASS(hv.layout->countAnnotations() == 0);
	hv.view->cmdUndo(1);
	TFPASS(hv.layout->countAnnotations() == 1);

	TFPASS(hv.exportPdf(tmpPdf.c_str()) == UT_OK);
	TFPASS(checkNotePdfFile(tmpPdf.c_str()));
}

TFTEST_MAIN("fixture footnote lays out, deletes mid-document and exports")
{
	HeadlessNoteView hv;
	TFPASS(hv.load("/test/wp/long_footnote.doc"));
	if (!hv.view)
		return;

	std::string tmpPdf = std::string("/tmp/abn_footfix_") +
		std::to_string(::getpid()) + ".pdf";

	/* the fixture must contain at least one footnote */
	TFPASS(hv.layout->countFootnotes() >= 1);
	TFPASS(hv.exportPdf(tmpPdf.c_str()) == UT_OK);
	TFPASS(checkNotePdfFile(tmpPdf.c_str()));

	/* delete a footnote mid-document — the crash the LAY10
	 * deferred-delete restructure protects against */
	fl_FootnoteLayout * pFL = hv.layout->getNthFootnote(0);
	TFPASS(pFL != nullptr);
	PT_DocPosition posAnchor =
		hv.doc->getStruxPosition(pFL->getStruxDocHandle()) - 1;
	hv.view->setPoint(posAnchor);
	hv.view->cmdCharDelete(true, 1);
	TFPASS(hv.layout->countFootnotes() == 0);
	hv.view->cmdUndo(1);
	TFPASS(hv.layout->countFootnotes() == 1);

	TFPASS(hv.exportPdf(tmpPdf.c_str()) == UT_OK);
	TFPASS(checkNotePdfFile(tmpPdf.c_str()));
}
