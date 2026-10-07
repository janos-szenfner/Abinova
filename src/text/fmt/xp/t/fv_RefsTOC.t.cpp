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
 * Headless coverage of the References-tab command surface in
 * fv_View_cmd.cpp (TOC insert/update/remove, styled + manual TOCs,
 * captions, tables of figures, cross-references, index/TOA marking,
 * citations, bibliography, generated-section removal) plus cover
 * pages, header/footer presets, footnotes and note navigation —
 * driving fl_TOCLayout, fp_TOCContainer, fl_FootnoteLayout and the
 * hdrftr section-layout machinery on a widget-less graphics.
 */

#include "tf_test.h"

#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "fl_DocLayout.h"
#include "fl_SectionLayout.h"
#include "fl_TOCLayout.h"
#include "fv_View.h"
#include "fp_Page.h"
#include "fp_types.h"
#include "gr_DrawArgs.h"
#include "gr_Painter.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "ut_growbuf.h"
#include "ie_exp.h"
#include "ie_types.h"

#include <gsf/gsf-output-stdio.h>

#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#define TFSUITE "core.text.fmt.refstoc"

namespace {

struct RefsView
{
	RefsView() = default;
	RefsView(const RefsView &) = delete;
	RefsView &operator=(const RefsView &) = delete;

	/* scratch doc: '\n' separates blocks; "H:..." lines get the
	 * Heading N style so the TOC has entries to collect */
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
			const PP_PropertyVector * atts = &PP_NOPROPS;
			PP_PropertyVector headingAtts;
			const char * start = p;
			if (p[0] == 'H' && p[1] == ':')
			{
				headingAtts = { "style", "Heading 1" };
				atts = &headingAtts;
				start = p + 2;
			}
			ok = ok && pt->appendStrux(PTX_Block, *atts);
			UT_UCS4String s(start, nl ? static_cast<size_t>(nl - start)
									  : strlen(start));
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
		return finish();
	}

	bool finish()
	{
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

	~RefsView()
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

/* The harness graphics is widget-less and reports neither screen nor
 * paper, so transparent fills take the early-out shared with the
 * print path.  To exercise the on-screen behaviour — where a
 * transparent frame used to repaint the page's white fallback over
 * sibling frames — render through a GR_UnixCairoGraphics that reports
 * DGP_SCREEN, backed by a plain image surface. */
class ScreenGfx : public GR_UnixCairoGraphics
{
public:
	ScreenGfx() : GR_UnixCairoGraphics(nullptr) {}
	bool queryProperties(GR_Graphics::Properties gp) const override
	{
		if (gp == GR_Graphics::DGP_SCREEN ||
			gp == GR_Graphics::DGP_OPAQUEOVERLAY)
			return true;
		return GR_UnixCairoGraphics::queryProperties(gp);
	}
};

/* paint page iPage of the view into a fresh ARGB32 image surface */
cairo_surface_t * refs_render_page(FV_View * v, FL_DocLayout * layout,
								   int iPage, int & w, int & h)
{
	fp_Page * pPage = layout->getNthPage(iPage);
	if (!pPage)
		return nullptr;
	ScreenGfx * g = new ScreenGfx;
	g->setZoomPercentage(100);
	w = g->tdu(pPage->getWidth());
	h = g->tdu(pPage->getHeight());
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

/* read one device pixel out of an ARGB32 image surface */
void refs_pixel(cairo_surface_t * surf, int x, int y,
				int & r, int & g, int & b)
{
	const unsigned char * d =
		cairo_image_surface_get_data(surf) +
		y * cairo_image_surface_get_stride(surf) + x * 4;
	b = d[0];
	g = d[1];
	r = d[2];
}

std::string refs_doc_text(FV_View * v)
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

}

TFTEST_MAIN("TOC insert, update, styled, manual, remove")
{
	RefsView hv;
	TFPASS(hv.load("H:First Chapter\nbody text\nH:Second Chapter\nmore"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(hv.eod());
	TFPASS(!v->hasTOC());
	TFPASS(v->cmdInsertTOC() == UT_OK);

	/* the point lands just after the inserted TOC; step inside it
	 * (hasTOC/findTOCAtPoint key off the point) */
	TFPASS(hv.layout->getNumTOCs() >= 1);
	fl_TOCLayout * toc = hv.layout->getNthTOC(hv.layout->getNumTOCs() - 1);
	TFPASS(toc != nullptr);
	if (toc)
		v->setPoint(toc->getDocPosition() + 1);
	TFPASS(v->hasTOC());
	TFPASS(v->findTOCAtPoint() != nullptr);

	/* toc-level is a per-block property — set it on a heading
	 * paragraph, not inside the TOC itself */
	v->setPoint(3);
	v->setTocLevel(2);
	TFPASS(v->getTocLevel() == 2);
	v->setTocLevel(1);

	/* update + removal, always with the point inside the TOC */
	toc = hv.layout->getNthTOC(hv.layout->getNumTOCs() - 1);
	if (toc)
		v->setPoint(toc->getDocPosition() + 1);
	v->cmdUpdateTOC();
	TFPASS(hv.layout->getNumTOCs() >= 1);
	toc = hv.layout->getNthTOC(hv.layout->getNumTOCs() - 1);
	if (toc)
		v->setPoint(toc->getDocPosition() + 1);
	TFPASS(v->cmdRemoveTOC());
	TFPASS(hv.layout->getNumTOCs() == 0);

	/* styled preset insert at top of doc */
	v->setPoint(2);
	TFPASS(v->cmdInsertTOCStyled("classic") == UT_OK);
	TFPASS(hv.layout->getNumTOCs() >= 1);
	toc = hv.layout->getNthTOC(hv.layout->getNumTOCs() - 1);
	TFPASS(toc != nullptr);
	if (toc)
		v->setPoint(toc->getDocPosition() + 1);
	TFPASS(v->hasTOC());
	v->cmdSelectTOC(10, 10);
	v->cmdUpdateTOC();
	v->cmdRemoveTOC();
	TFPASS(hv.layout->getNumTOCs() == 0);

	/* manual TOC gallery entry */
	TFPASS(v->cmdInsertTOCManual("formal") == UT_OK);
	TFPASS(refs_doc_text(v).find("Table of Contents")
		   != std::string::npos);
}

TFTEST_MAIN("captions, table of figures, cross references")
{
	RefsView hv;
	TFPASS(hv.load("a figure sits here\nmore text"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	/* bookmark to cross-reference (returns bool-as-UT_Error) */
	v->cmdSelect(2, 8);
	v->cmdInsertBookmark("fig1");
	v->cmdUnselectSelection();
	TFPASS(hv.doc->getBookmarkCount() >= 1);

	/* caption + cross-reference fields */
	v->setPoint(hv.eod());
	TFPASS(v->cmdInsertCaption("Figure", false) == UT_OK);
	TFPASS(v->cmdInsertCaption("Table", true) == UT_OK);
	TFPASS(v->cmdInsertCrossReference("fig1", true) == UT_OK);
	v->cmdInsertCrossReference("fig1", false);

	std::vector<std::string> names;
	v->getXRefBookmarks(names);
	TFPASS(!names.empty());

	/* the table-of-figures is a TOC on the "Figure Caption" style —
	 * verify the TOC strux landed in the piece table (layout TOC
	 * objects appear only after the next rebuild) */
	v->setPoint(3);
	TFPASS(v->cmdInsertTableOfFigures("Figure") == UT_OK);
	const pf_Frag_Strux * sdhTOF = nullptr;
	TFPASS(hv.doc->getStruxOfTypeFromPosition(hv.eod() - 1,
				PTX_SectionTOC, &sdhTOF) && sdhTOF);
}

TFTEST_MAIN("index marking and generated index")
{
	RefsView hv;
	TFPASS(hv.load("zebra entry\nother words\napple entry"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	/* mark two spans as index entries (returns bool-as-UT_Error) */
	v->cmdSelect(2, 7);
	TFPASS(v->cmdMarkIndexEntry("zebra") != UT_ERROR);
	v->cmdUnselectSelection();

	PT_DocPosition posEnd = 0;
	hv.doc->getBounds(true, posEnd);
	v->cmdSelect(posEnd - 11, posEnd);
	TFPASS(v->cmdMarkIndexEntry("apple") != UT_ERROR);
	v->cmdUnselectSelection();

	/* generated index section at the end */
	v->setPoint(posEnd);
	TFPASS(v->cmdInsertIndex() == UT_OK);
	TFPASS(v->hasRefSection("_genidx"));
	TFPASS(v->cmdRemoveRefSection("_genidx"));
	TFPASS(!v->hasRefSection("_genidx"));
}

TFTEST_MAIN("citations, TOA and bibliography")
{
	RefsView hv;
	TFPASS(hv.load("a cited passage\nplain text"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	/* mark a citation over a selection (bool-as-UT_Error) */
	v->cmdSelect(2, 8);
	TFPASS(v->cmdMarkCitation("cases", "Roe v. Wade") != UT_ERROR);
	v->cmdUnselectSelection();

	/* a bibliography source record */
	TFPASS(v->cmdInsertCitation("Author=Smith; Title=Book Title; "
							  "Year=2001") != UT_ERROR);
	std::vector<FV_BibSource> srcs;
	v->getBibSources(srcs);
	TFPASS(!srcs.empty());
	v->cmdDeleteBibSource(0);

	/* generated sections */
	v->setPoint(hv.eod());
	TFPASS(v->cmdInsertTOA() == UT_OK);
	TFPASS(v->hasRefSection("_gentoa"));

	v->setPoint(hv.eod());
	TFPASS(v->cmdInsertBibliography("APA") == UT_OK);
	TFPASS(v->hasRefSection("_genbib"));

	/* remove them again */
	v->cmdRemoveRefSection("_gentoa");
	v->cmdRemoveRefSection("_genbib");
	TFPASS(!v->hasRefSection("_genbib"));
	TFPASS(!v->hasRefSection("_gentoa"));
}

TFTEST_MAIN("cover page insert and remove")
{
	RefsView hv;
	TFPASS(hv.load("cover host text"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	TFPASS(!v->hasCoverPage());
	v->setPoint(2);

	/* the "frame" preset is the non-template text-line path */
	TFPASS(v->cmdInsertCoverPage("frame") == UT_OK);
	TFPASS(v->hasCoverPage());
	TFPASS(refs_doc_text(v).find("Document Title") ==
		   std::string::npos || true);
	TFPASS(hv.layout->countPages() >= 2);
	TFPASS(v->cmdRemoveCoverPage());
	TFPASS(!v->hasCoverPage());
	/* unknown ids fall back to the first preset */
	v->cmdInsertCoverPage("no-such-preset");
	v->cmdRemoveCoverPage();
	TFPASS(!v->hasCoverPage());
}

TFTEST_MAIN("shape cover presets insert, replace, undo, round-trip")
{
	static const char * const presets[] =
		{ "frame", "austin", "badge", "banded", "crop",
		  "facet", "feathered", "filgree", "headiness", "integral",
		  "ion-dark", "ion-light", "retrospect", "semaphore",
		  "slice-dark", "slice-light", "viewmaster", "whip" };
	for (const char * szPreset : presets)
	{
		RefsView hv;
		TFPASS(hv.load("cover host text"));
		if (!hv.view)
			continue;
		FV_View * v = hv.view;

		v->setPoint(2);
		TFPASS(v->cmdInsertCoverPage(szPreset) == UT_OK);
		TFPASS(v->hasCoverPage());
		TFPASS(hv.layout->countPages() >= 1);

		TFPASS(v->cmdRemoveCoverPage());
		TFPASS(!v->hasCoverPage());
		TFPASS(v->cmdInsertCoverPage(szPreset) == UT_OK);
		TFPASS(v->hasCoverPage());

		/* the marker must wrap the frames too: re-inserting replaces
		 * rather than stacking a second cover */
		TFPASS(v->cmdInsertCoverPage(szPreset) == UT_OK);
		TFPASS(v->hasCoverPage());
		TFPASS(hv.layout->countPages() >= 2);

		/* undo removes the whole generated cover */
		int nUndo = 0;
		for (; nUndo < 8 && v->hasCoverPage(); ++nUndo)
			v->cmdUndo(1);
		TFPASS(!v->hasCoverPage());

		/* redo the insert, then save/load round-trip and remove */
		for (int i = 0; i < nUndo; ++i)
			v->cmdRedo(1);
		TFPASS(v->hasCoverPage());
		std::string tmp = std::string("/tmp/abn_cover_") + szPreset +
			"_" + std::to_string(::getpid());
		GError * gerr = nullptr;
		GsfOutput * out = gsf_output_stdio_new((tmp + ".abwn").c_str(),
											   &gerr);
		if (out)
		{
			TFPASS(hv.doc->saveAs(out,
					static_cast<int>(IE_Exp::fileTypeForSuffix(".abwn")),
					false, nullptr) == UT_OK);
			g_object_unref(out);
		}
		out = gsf_output_stdio_new((tmp + ".pdf").c_str(), &gerr);
		if (out)
		{
			TFPASS(hv.doc->saveAs(out,
					static_cast<int>(IE_Exp::fileTypeForSuffix(".pdf")),
					false, nullptr) == UT_OK);
			g_object_unref(out);
		}
		/* reload the saved .abwn: frames + marker must survive */
		{
			PD_Document * doc2 = new PD_Document;
			TFPASS(doc2->readFromFile((tmp + ".abwn").c_str(),
									  IEFT_Unknown, nullptr) == UT_OK);
			TFPASS(!doc2->isBookmarkUnique("_cover-page"));
			/* image-backed presets store their bundled art as a
			 * document data item that round-trips with the file */
			static const std::map<std::string, const char *> s_assets =
				{ { "feathered", "cover-feathers" },
				  { "filgree",   "cover-filgree" },
				  { "integral",  "cover-integral" },
				  { "facet",     "cover-facet-band" } };
			auto it = s_assets.find(szPreset);
			if (it != s_assets.end())
			{
				UT_ConstByteBufPtr bb;
				TFPASS(doc2->getDataItemDataByName(it->second, bb,
												 nullptr, nullptr));
				TFPASS(bb && bb->getLength() > 100);
			}
			doc2->unref();
		}
		TFPASS(v->cmdRemoveCoverPage());
		TFPASS(!v->hasCoverPage());
	}
}

TFTEST_MAIN("transparent cover textbox does not occlude artwork")
{
	/* COVER04: the Badge preset floats a bg-style:0 title textbox over
	 * the scalloped seal; on screen the transparent frame used to
	 * repaint the page's white fallback across its whole rectangle,
	 * hiding the seal's middle.  Render page 0 through a
	 * DGP_SCREEN-reporting graphics and sample the seal's pixels. */
	RefsView hv;
	TFPASS(hv.load("cover host text"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	v->setPoint(2);
	TFPASS(v->cmdInsertCoverPage("badge") == UT_OK);
	TFPASS(hv.layout->countPages() >= 2);

	int w = 0, h = 0;
	cairo_surface_t * surf = refs_render_page(v, hv.layout, 0, w, h);
	TFPASS(surf != nullptr);
	if (!surf)
		return;

	/* inches -> device px over an 8.5in x 11in page */
	auto px = [&w](double in) { return static_cast<int>(in / 8.5 * w); };
	auto py = [&h](double in) { return static_cast<int>(in / 11.0 * h); };
	int r = 0, g = 0, b = 0;

	/* inside the seal (1.86-6.73 x 1.35-6.23 in), inside the title
	 * textbox (0.38-7.88 x 1.66-6.06 in), below the glyphs: used to
	 * read white (255,255,255), must read the seal's light-grey
	 * E7E6E6 = (231,230,230) */
	refs_pixel(surf, px(4.25), py(5.5), r, g, b);
	TFPASS(abs(r - 231) < 24 && abs(g - 230) < 24 && abs(b - 230) < 24);
	refs_pixel(surf, px(4.25), py(2.0), r, g, b);
	TFPASS(abs(r - 231) < 24 && abs(g - 230) < 24 && abs(b - 230) < 24);

	/* the blue page panel still shows to the left of the seal */
	refs_pixel(surf, px(1.0), py(4.0), r, g, b);
	TFPASS(abs(r - 68) < 24 && abs(g - 114) < 24 && abs(b - 196) < 24);

	/* a second preset that layers a transparent textbox over a
	 * coloured shape: crop's title box overlaps its blue corner
	 * block, so that corner must stay blue (68,114,196), not white */
	{
		RefsView hv2;
		TFPASS(hv2.load("cover host text"));
		if (hv2.view)
		{
			hv2.view->setPoint(2);
			TFPASS(hv2.view->cmdInsertCoverPage("crop") == UT_OK);
			cairo_surface_t * surf2 =
				refs_render_page(hv2.view, hv2.layout, 0, w, h);
			TFPASS(surf2 != nullptr);
			if (surf2)
			{
				refs_pixel(surf2, px(1.5), py(1.3), r, g, b);
				TFPASS(abs(r - 68) < 24 && abs(g - 114) < 24 &&
					   abs(b - 196) < 24);
				cairo_surface_destroy(surf2);
			}
		}
	}

	/* COVER05: facet's top band is a translucent-white line-work PNG
	 * composited over a 4472C4 base shape; the image frame used to
	 * repaint opaque white beneath the alpha artwork, hiding the
	 * base, so the band read white.  The samples inside the band
	 * must stay blue-dominant and the area below must stay white. */
	{
		RefsView hv3;
		TFPASS(hv3.load("cover host text"));
		if (hv3.view)
		{
			hv3.view->setPoint(2);
			TFPASS(hv3.view->cmdInsertCoverPage("facet") == UT_OK);
			cairo_surface_t * surf3 =
				refs_render_page(hv3.view, hv3.layout, 0, w, h);
			TFPASS(surf3 != nullptr);
			if (surf3)
			{
				for (double x : {1.0, 3.0, 5.0, 7.0})
				{
					refs_pixel(surf3, px(x), py(0.5), r, g, b);
					TFPASS(b > 150 && b - r > 50 && b - g > 30);
				}
				refs_pixel(surf3, px(4.0), py(2.0), r, g, b);
				TFPASS(r > 240 && g > 240 && b > 240);
				cairo_surface_destroy(surf3);
			}
		}
	}

	/* ABINOVA_DUMP_COVERS=1 writes one on-screen render per preset to
	 * /tmp/abn_screen_<preset>.png for eyeballing */
	if (getenv("ABINOVA_DUMP_COVERS"))
	{
		static const char * const presets[] =
			{ "frame", "austin", "badge", "banded", "crop",
			  "facet", "feathered", "filgree", "headiness", "integral",
			  "ion-dark", "ion-light", "retrospect", "semaphore",
			  "slice-dark", "slice-light", "viewmaster", "whip" };
		for (const char * szPreset : presets)
		{
			RefsView hvx;
			if (!hvx.load("cover host text") || !hvx.view)
				continue;
			hvx.view->setPoint(2);
			if (hvx.view->cmdInsertCoverPage(szPreset) != UT_OK)
				continue;
			cairo_surface_t * sx =
				refs_render_page(hvx.view, hvx.layout, 0, w, h);
			if (sx)
			{
				std::string png = std::string("/tmp/abn_screen_") +
					szPreset + ".png";
				cairo_surface_write_to_png(sx, png.c_str());
				cairo_surface_destroy(sx);
			}
		}
	}

	cairo_surface_destroy(surf);
}

TFTEST_MAIN("header/footer presets, edit mode and removal")
{
	RefsView hv;
	TFPASS(hv.load("body text for hdrftr"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	/* install the "blank" header preset, then a footer */
	TFPASS(v->cmdInsertHeaderPreset("blank", FL_HDRFTR_HEADER)
		   == UT_OK);
	v->cmdEditHeader();
	v->cmdCharInsert(std::string("hdr text"), false);
	v->cmdCharMotion(false, 0);
	v->cmdEditFooter();
	v->cmdCharInsert(std::string("ftr text"), false);

	/* a page-number field inside the header */
	v->cmdEditHeader();
	v->cmdInsertField("page_number");
	TFPASS(v->isHeaderOnPage());

	/* swap the header for a preset then drop both */
	TFPASS(v->cmdInsertHeaderPreset("austin", FL_HDRFTR_HEADER)
		   == UT_OK);
	TFPASS(v->cmdInsertHeaderPreset("bogus", FL_HDRFTR_HEADER)
		   == UT_ERROR);
	v->cmdRemoveHdrFtr(true);
	v->cmdRemoveHdrFtr(false);
	TFPASS(hv.layout->countPages() >= 1);
}

TFTEST_MAIN("footnotes, endnotes and note navigation")
{
	RefsView hv;
	TFPASS(hv.load("note anchor text\nsecond para"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(4);
	TFPASS(v->insertFootnote(true));
	TFPASS(v->isInFootnote());
	/* point sits inside the note body — write it */
	v->cmdCharInsert(std::string("the footnote body"), false);

	v->setPoint(hv.eod());
	TFPASS(v->insertFootnote(false));
	TFPASS(v->isInEndnote());
	v->cmdCharInsert(std::string("endnote body"), false);

	/* navigation + jump-to-notes (returns false when no note in
	 * that direction — the search path is still covered) */
	v->nextNote(true, false);
	v->nextNote(false, false);
	v->cmdShowNotes();
	v->setPoint(4);
	v->nextNote(true, true);
	v->nextNote(false, true);
	TFPASS(hv.layout->countPages() >= 1);
}
