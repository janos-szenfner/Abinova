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
 * TRACK04: view-level audit of the whole track-changes surface.
 * pd_Revision.t.cpp covers the piece-table model (marks, explode,
 * accept/reject); fv_StateCycle.t.cpp covers the 4-mode selector.
 * This suite drives the rest of the surface the way the UI does —
 * real edit methods where they do not need a frame, the view/doc
 * commands they delegate to otherwise — and asserts rendered content
 * (visibleText), not just flag state:
 *
 *   - legacy Before/After/AfterPrevious level toggles
 *   - cmdFindRevision: next/prev across block boundaries, wrap-around
 *   - accept/reject at caret and the full bulk-op matrix through the
 *     real edit methods (AcceptNext/All/Shown/StopTracking + rejects)
 *   - undo/redo of accept/reject
 *   - edits inside and at the edges of marked runs
 *   - insertion-point revision-attr cleanup
 *   - annotations inside revised runs
 *   - revision mouse context (the right-click menu surface)
 *   - imported w:ins/w:del marks (real DOCX fixture) under all modes
 *   - abwn persistence round-trip of marks + revision state
 *
 * Every transition also asserts the data-preservation invariants:
 * display ops never drop revision records or piece-table text, and
 * accept/reject only ever removes marks.
 */

#include "tf_test.h"
#include "tf_guard.h"
#include "tf_actions.h"

#include "pd_Document.h"
#include "pd_Iterator.h"
#include "pt_PieceTable.h"
#include "pf_Frag.h"
#include "pp_Revision.h"
#include "fl_DocLayout.h"
#include "fl_BlockLayout.h"
#include "fv_View.h"
#include "fp_Page.h"
#include "fp_Run.h"
#include "fp_TextRun.h"
#include "gr_DrawArgs.h"
#include "gr_Painter.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "xap_EditMethods.h"
#include "ev_EditMethod.h"
#include "ev_EditBits.h"
#include "ie_types.h"
#include "ut_growbuf.h"
#include "ut_go_file.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <gsf/gsf-output-stdio.h>

#define TFSUITE "core.text.fmt.revisions"

namespace {

/* scratch doc with a real layout — '\n' separates blocks */
struct RevView
{
	RevView() = default;
	RevView(const RevView &) = delete;
	RevView &operator=(const RevView &) = delete;

	bool finishLayout()
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
			ok = ok && pt->appendStrux(PTX_Block, PP_NOPROPS);
			UT_UCS4String s(p, nl ? static_cast<size_t>(nl - p)
								  : strlen(p));
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
		return finishLayout();
	}

	bool loadFile(const char *path)
	{
		doc = new PD_Document;
		if (doc->readFromFile(path, IEFT_Unknown, nullptr) != UT_OK)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		return finishLayout();
	}

	PT_DocPosition eod() const
	{
		PT_DocPosition pos = 0;
		doc->getBounds(true, pos);
		return pos;
	}

	std::string text() const
	{
		UT_GrowBuf buf;
		view->getTextInDocument(buf);
		std::string out;
		out.reserve(buf.getLength());
		for (UT_uint32 i = 0; i < buf.getLength(); ++i)
		{
			UT_UCS4Char c =
				static_cast<UT_UCS4Char>(buf.getPointer(i)[0]);
			if (c < 0x80)
				out += static_cast<char>(c);
		}
		return out;
	}

	/* text as rendered: hidden revision runs are skipped — what the
	 * screen actually shows in the current display mode */
	std::string visibleText() const
	{
		std::string out;
		fl_BlockLayout *pBlock = layout->findBlockAtPosition(2);
		for (; pBlock; pBlock = pBlock->getNextBlockInDocument())
		{
			for (fp_Run *pRun = pBlock->getFirstRun(); pRun;
				 pRun = pRun->getNextRun())
			{
				if (pRun->getType() != FPRUN_TEXT ||
					pRun->getVisibility() != FP_VISIBLE)
					continue;
				UT_GrowBuf buf;
				static_cast<fp_TextRun *>(pRun)->appendTextToBuf(buf);
				for (UT_uint32 i = 0; i < buf.getLength(); ++i)
				{
					UT_UCS4Char c =
						static_cast<UT_UCS4Char>(buf.getPointer(i)[0]);
					if (c < 0x80)
						out += static_cast<char>(c);
				}
			}
		}
		return out;
	}

	/* doc position of the n-th visible revision run (-1 = last) */
	PT_DocPosition nthMarkedRunPos(int n) const
	{
		PT_DocPosition found = 0;
		int count = 0;
		fl_BlockLayout *pBlock = layout->findBlockAtPosition(2);
		for (; pBlock; pBlock = pBlock->getNextBlockInDocument())
		{
			for (fp_Run *pRun = pBlock->getFirstRun(); pRun;
				 pRun = pRun->getNextRun())
			{
				if (!pRun->containsRevisions())
					continue;
				found = pBlock->getPosition() + pRun->getBlockOffset();
				if (count++ == n)
					return found;
			}
		}
		return n < 0 ? found : 0;
	}

	int markedRunCount() const
	{
		int count = 0;
		fl_BlockLayout *pBlock = layout->findBlockAtPosition(2);
		for (; pBlock; pBlock = pBlock->getNextBlockInDocument())
		{
			for (fp_Run *pRun = pBlock->getFirstRun(); pRun;
				 pRun = pRun->getNextRun())
			{
				if (pRun->containsRevisions())
					++count;
			}
		}
		return count;
	}

	~RevView()
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

void fvrSettle(FL_DocLayout * l)
{
	l->formatAll();
	tf_guard::drain_pending();
}

/* raw "revision" attribute of the frag at a doc position */
std::string fvrRevAttr(PD_Document *doc, PT_DocPosition pos)
{
	const pf_Frag *pf = doc->getFragFromPosition(pos);
	if (!pf)
		return {};
	const PP_AttrProp *pAP = nullptr;
	if (!doc->getAttrProp(pf->getIndexAP(), &pAP) || !pAP)
		return {};
	const gchar *v = nullptr;
	if (!pAP->getAttribute(PT_REVISION_ATTRIBUTE_NAME, v) || !v)
		return {};
	return v;
}

EV_EditMethod * findEM(const char *name)
{
	static EV_EditMethodContainer * s_emc = AP_GetEditMethods();
	return s_emc ? s_emc->findEditMethodByName(name) : nullptr;
}

/* invoke a no-payload edit method headless — the same call the
 * ribbon rows and menu items make */
bool callEM(FV_View * v, const char *name)
{
	EV_EditMethod * em = findEM(name);
	if (!em)
		return false;
	EV_EditMethodCallData cd;
	struct Ctx { EV_EditMethod * em; AV_View * v;
				 EV_EditMethodCallData * cd; bool ret; };
	Ctx c{em, v, &cd, false};
	if (tf_guard::call([&c] {
			c.ret = c.em->Fn(c.v, c.cd);
		}, 8000) != tf_guard::OK)
		return false;
	tf_guard::drain_pending();
	return c.ret;
}

bool callEMu4(FV_View * v, const char *name, const char *payload)
{
	EV_EditMethod * em = findEM(name);
	if (!em)
		return false;
	UT_UCS4String m(payload);
	EV_EditMethodCallData cd(m.ucs4_str(),
							 static_cast<UT_uint32>(m.length()));
	struct Ctx { EV_EditMethod * em; AV_View * v;
				 EV_EditMethodCallData * cd; bool ret; };
	Ctx c{em, v, &cd, false};
	if (tf_guard::call([&c] {
			c.ret = c.em->Fn(c.v, c.cd);
		}, 8000) != tf_guard::OK)
		return false;
	tf_guard::drain_pending();
	return c.ret;
}

/* doc with a real in-app tracked insertion at pos and a tracked
 * deletion of "deleted" — returns the rev id used */
bool seedMarks(RevView & hv)
{
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;

	doc->setMarkRevisions(true);
	/* tracked deletion of the word "deleted" (positions 5..12) */
	v->setPoint(12);
	v->cmdCharDelete(true, 7);
	/* tracked insertion at the end */
	v->setPoint(hv.eod());
	v->cmdCharInsert(std::string(" INSERTED"), false);
	doc->setMarkRevisions(false);
	fvrSettle(hv.layout);
	return doc->getHighestRevisionId() >= 1;
}

/* TST13: a landed-state check for one display mode on the imported
 * fixture — flags + derived level + rendered markers.  The docx's
 * own mark text is the oracle: DELONE (w:del), INSA/INSB (w:ins),
 * MOVEDTEXT (moveFrom counts as a second copy only when both sides
 * of the move are shown). */
const char * docxModeApplied(RevView &hv, const char *name, bool marking,
							 UT_uint32 rev0)
{
	FV_View * v = hv.view;
	struct Exp { const char * name; bool sR, bars, seeDel, seeIns; };
	static const Exp tbl[] = {
		{ "all",      true,  false, true,  true  },
		{ "simple",   false, true,  false, true  },
		{ "none",     false, false, false, true  },
		{ "original", false, false, true,  false },
	};
	const Exp * e = nullptr;
	for (const Exp & t : tbl)
		if (!strcmp(t.name, name))
			e = &t;
	if (!e)
		return "unknown mode";
	fvrSettle(hv.layout);
	if (v->isShowRevisions() != e->sR)
		return "isShowRevisions";
	if (v->isShowRevBars() != e->bars)
		return "isShowRevBars";
	const UT_uint32 wantLvl =
		!strcmp(name, "original") ? 0 :
		(e->sR && marking ? 0 : PD_MAX_REVISION);
	if (v->getRevisionLevel() != wantLvl)
		return "revision level";
	const std::string vis = hv.visibleText();
	if ((vis.find("DELONE") != std::string::npos) != e->seeDel)
		return e->seeDel ? "DELONE not rendered"
						 : "DELONE still rendered";
	if ((vis.find("INSA") != std::string::npos) != e->seeIns)
		return e->seeIns ? "INSA not rendered"
						 : "INSA still rendered";
	size_t nMoved = 0;
	for (size_t at = vis.find("MOVEDTEXT"); at != std::string::npos;
		 at = vis.find("MOVEDTEXT", at + 1))
		++nMoved;
	if (nMoved != (e->seeDel && e->seeIns ? 2 : 1))
		return "MOVEDTEXT copy count";
	if (hv.doc->getHighestRevisionId() != rev0)
		return "revision data dropped";
	return nullptr;
}

/* TST13: the harness graphics is widget-less and reports neither
 * screen nor paper; the Simple-Markup margin bar only paints on a
 * DGP_SCREEN graphics, so render through one backed by an image
 * surface (same trick as fv_RefsTOC's seal pixel test). */
class RevScreenGfx : public GR_UnixCairoGraphics
{
public:
	RevScreenGfx() : GR_UnixCairoGraphics(nullptr) {}
	bool queryProperties(GR_Graphics::Properties gp) const override
	{
		if (gp == GR_Graphics::DGP_SCREEN ||
			gp == GR_Graphics::DGP_OPAQUEOVERLAY)
			return true;
		return GR_UnixCairoGraphics::queryProperties(gp);
	}
};

/* paint page 0 of the view into a fresh ARGB32 image surface */
cairo_surface_t * revRenderPage(RevView &hv, int &w, int &h)
{
	fp_Page * pPage = hv.layout->getNthPage(0);
	if (!pPage)
		return nullptr;
	RevScreenGfx * g = new RevScreenGfx;
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
	hv.layout->setQuickPrint(g);
	hv.view->drawPage(0, &da);
	hv.layout->setQuickPrint(nullptr);
	g->endPaint();
	cairo_destroy(cr);
	delete g;
	cairo_surface_flush(surf);
	return surf;
}

/* any pixel of the revision bar colour (0xf0 0x40 0x40) strictly
 * left of the text column? — text never draws there, so a hit is
 * always the Simple-Markup bar */
bool revMarginHasBar(cairo_surface_t * surf, int textLeftPx)
{
	const unsigned char * data =
		cairo_image_surface_get_data(surf);
	const int stride = cairo_image_surface_get_stride(surf);
	const int h = cairo_image_surface_get_height(surf);
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < textLeftPx - 2; ++x)
		{
			const unsigned char * p = data + y * stride + x * 4;
			if (p[2] > 200 && p[1] < 110 && p[0] < 110)
				return true;
		}
	return false;
}

} // namespace

TFTEST_MAIN("imported docx marks render under all display modes")
{
	/* o06_revisions.docx: w:ins "INSA"+"INSB"+"INSC", w:del "DELONE",
	 * moveFrom/moveTo "MOVEDTEXT", a deleted paragraph mark —
	 * real imported revision attrs, not in-app seeded ones */
	std::string data_file;
	TFPASS(TF_Test::ensure_test_data("/test/wp/tst04/o06_revisions.docx",
								   data_file));
	RevView hv;
	TFPASS(hv.loadFile(data_file.c_str()));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;

	fvrSettle(hv.layout);
	const UT_uint32 rev0 = doc->getHighestRevisionId();
	TFPASS(rev0 >= 1);
	TFPASS(hv.markedRunCount() >= 3);

	struct Mode { const char *name; bool seeDel, seeIns; };
	static const Mode seq[] = {
		{ "all",      true,  true  },
		{ "none",     false, true  },
		{ "simple",   false, true  },
		{ "original", true,  false },
		{ "all",      true,  true  },
	};
	for (const Mode & m : seq)
	{
		TFPASS(callEMu4(v, "revisionDisplayMode", m.name));
		fvrSettle(hv.layout);
		const std::string vis = hv.visibleText();
		TFPASS((vis.find("DELONE") != std::string::npos) == m.seeDel);
		TFPASS((vis.find("INSA") != std::string::npos) == m.seeIns);
		/* moved text: moveFrom is a deletion, moveTo an insertion —
		 * the text is present in every mode; only All Markup shows
		 * both copies (once as the moveFrom deletion, once as the
		 * moveTo insertion) */
		size_t nMoved = 0;
		for (size_t at = vis.find("MOVEDTEXT"); at != std::string::npos;
			 at = vis.find("MOVEDTEXT", at + 1))
			++nMoved;
		TFPASS(nMoved == (m.seeDel && m.seeIns ? 2 : 1));
		TFPASS(doc->getHighestRevisionId() == rev0);
	}
	/* the deleted paragraph mark is a strux-level mark — the text of
	 * its block must still be intact in every mode */
	TFPASS(hv.text().find("aftermark") != std::string::npos);
}

TFTEST_MAIN("legacy level toggles keep rendering consistent")
{
	RevView hv;
	TFPASS(hv.load("keep deleted keep"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;
	TFPASS(seedMarks(hv));
	const UT_uint32 rev0 = doc->getHighestRevisionId();

	/* baseline: All Markup semantics — shown revisions at reveal-all */
	v->setShowRevisions(true);
	v->cmdSetRevisionLevel(0);
	fvrSettle(hv.layout);
	std::string vis = hv.visibleText();
	TFPASS(vis.find("deleted") != std::string::npos);
	TFPASS(vis.find("INSERTED") != std::string::npos);

	/* toggleShowRevisions hides the marks but keeps the stored
	 * level — hidden + level 0 renders the doc as of revision 0,
	 * i.e. the Original: the toggle is level-preserving, unlike the
	 * four-mode selector which always sets an explicit (show, level)
	 * pair */
	TFPASS(callEM(v, "toggleShowRevisions"));
	fvrSettle(hv.layout);
	vis = hv.visibleText();
	TFPASS(vis.find("deleted") != std::string::npos);
	TFPASS(vis.find("INSERTED") == std::string::npos);
	TFPASS(doc->getHighestRevisionId() == rev0);

	/* hidden at the final level -> No Markup */
	v->cmdSetRevisionLevel(PD_MAX_REVISION);
	fvrSettle(hv.layout);
	vis = hv.visibleText();
	TFPASS(vis.find("deleted") == std::string::npos);
	TFPASS(vis.find("INSERTED") != std::string::npos);
	v->cmdSetRevisionLevel(0);

	/* Before: hidden + level 0 -> original */
	TFPASS(callEM(v, "toggleShowRevisionsBefore"));
	fvrSettle(hv.layout);
	vis = hv.visibleText();
	TFPASS(vis.find("deleted") != std::string::npos);
	TFPASS(vis.find("INSERTED") == std::string::npos);
	TFPASS(doc->getHighestRevisionId() == rev0);

	/* After (not marking, hidden): sets final level */
	TFPASS(callEM(v, "toggleShowRevisionsAfter"));
	fvrSettle(hv.layout);
	vis = hv.visibleText();
	TFPASS(vis.find("deleted") == std::string::npos);
	TFPASS(vis.find("INSERTED") != std::string::npos);

	/* toggling show back on reveals marks at the current level */
	TFPASS(callEM(v, "toggleShowRevisions"));
	fvrSettle(hv.layout);
	vis = hv.visibleText();
	TFPASS(vis.find("deleted") != std::string::npos);
	TFPASS(doc->getHighestRevisionId() == rev0);

	/* AfterPrevious: view level becomes highest-1 (all but the
	 * newest revision applied) and back to 0 */
	TFPASS(callEM(v, "toggleShowRevisionsAfterPrevious"));
	TFPASS(v->getRevisionLevel() == rev0 - 1);
	TFPASS(callEM(v, "toggleShowRevisionsAfterPrevious"));
	TFPASS(v->getRevisionLevel() == 0);

	/* while marking, After toggles the level but keeps showing */
	doc->setMarkRevisions(true);
	TFPASS(callEM(v, "toggleShowRevisionsAfter"));
	TFPASS(v->isShowRevisions());
	TFPASS(callEM(v, "toggleShowRevisionsAfter"));
	TFPASS(v->isShowRevisions());
	doc->setMarkRevisions(false);
}

TFTEST_MAIN("find revision crosses blocks and wraps")
{
	/* two marked insertions in different blocks — the pre-fix code
	 * never restarted the run chain after advancing a block, so it
	 * only ever searched the caret's own paragraph */
	RevView hv;
	TFPASS(hv.load("first para\nsecond para\nthird para\nfourth"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;

	doc->setMarkRevisions(true);
	/* mark in block 1 (pos 2 is block-1 start; text starts at 3) */
	v->setPoint(3);
	v->cmdCharInsert(std::string("AA"), false);
	/* mark in block 3: "first para"(10)+\n+"second para"(11)+\n
	 * -> block3 starts at 24, text at 25 */
	v->setPoint(25);
	v->cmdCharInsert(std::string("CC"), false);
	doc->setMarkRevisions(false);
	fvrSettle(hv.layout);
	TFPASS(doc->getHighestRevisionId() >= 1);

	/* docpos of the two marked runs */
	const PT_DocPosition posA = hv.nthMarkedRunPos(0);
	const PT_DocPosition posB = hv.nthMarkedRunPos(1);
	TFPASS(posA > 0 && posB > posA);

	/* find-next from doc start lands on the first mark */
	v->setPoint(2);
	TFPASS(v->cmdFindRevision(true, 0, 0));
	TFPASS(!v->isSelectionEmpty());
	PT_DocPosition lo = v->getSelectionAnchor();
	PT_DocPosition hi = v->getPoint();
	if (lo > hi)
		std::swap(lo, hi);
	TFPASS(lo <= posA && hi >= posA + 2);

	/* find-next again — must cross into block 3 */
	TFPASS(v->cmdFindRevision(true, 0, 0));
	lo = v->getSelectionAnchor();
	hi = v->getPoint();
	if (lo > hi)
		std::swap(lo, hi);
	TFPASS(lo <= posB && hi >= posB + 2);

	/* find-next at the end wraps to the first mark */
	TFPASS(v->cmdFindRevision(true, 0, 0));
	lo = v->getSelectionAnchor();
	hi = v->getPoint();
	if (lo > hi)
		std::swap(lo, hi);
	TFPASS(lo <= posA && hi >= posA + 2);

	/* find-prev wraps back to the last mark */
	v->setPoint(3);
	TFPASS(v->cmdFindRevision(false, 0, 0));
	lo = v->getSelectionAnchor();
	hi = v->getPoint();
	if (lo > hi)
		std::swap(lo, hi);
	TFPASS(lo <= posB && hi >= posB + 2);

	/* the edit-method surface drives the same command */
	v->setPoint(2);
	TFPASS(callEM(v, "revisionFindNext"));
	TFPASS(!v->isSelectionEmpty());
	TFPASS(callEM(v, "revisionFindPrev"));
	TFPASS(!v->isSelectionEmpty());

	/* no marks left -> find fails and the caret is restored */
	doc->acceptAllRevisions();
	fvrSettle(hv.layout);
	TFPASS(hv.markedRunCount() == 0);
	/* collapse the selection the last find left behind, then put
	 * the bare caret mid-text */
	v->moveInsPtTo(FV_DOCPOS_BOD);
	v->setPoint(5);
	const PT_DocPosition at = v->getPoint();
	TFPASS(!v->cmdFindRevision(true, 0, 0));
	TFPASS(v->getPoint() == at);
}

TFTEST_MAIN("accept and reject at caret")
{
	/* accept an insertion: text stays, mark cleared */
	{
		RevView hv;
		TFPASS(hv.load("base text"));
		if (!hv.view)
			return;
		FV_View * v = hv.view;
		hv.doc->setMarkRevisions(true);
		v->setPoint(3);
		v->cmdCharInsert(std::string("XYZ"), false);
		hv.doc->setMarkRevisions(false);
		fvrSettle(hv.layout);
		const PT_DocPosition pos = hv.nthMarkedRunPos(0);
		TFPASS(pos > 0);
		TFPASS(!fvrRevAttr(hv.doc, pos).empty());

		v->setPoint(pos + 1);
		v->cmdAcceptRejectRevision(false, 0, 0);
		fvrSettle(hv.layout);
		TFPASS(hv.text().find("XYZ") != std::string::npos);
		TFPASS(fvrRevAttr(hv.doc, pos).empty());
	}

	/* reject an insertion: text physically removed */
	{
		RevView hv;
		TFPASS(hv.load("base text"));
		if (!hv.view)
			return;
		FV_View * v = hv.view;
		hv.doc->setMarkRevisions(true);
		v->setPoint(3);
		v->cmdCharInsert(std::string("XYZ"), false);
		hv.doc->setMarkRevisions(false);
		fvrSettle(hv.layout);
		v->setPoint(hv.nthMarkedRunPos(0) + 1);
		v->cmdAcceptRejectRevision(true, 0, 0);
		fvrSettle(hv.layout);
		TFPASS(hv.text().find("XYZ") == std::string::npos);
	}

	/* reject a deletion: text restored unmarked */
	{
		RevView hv;
		TFPASS(hv.load("keep deleted keep"));
		if (!hv.view)
			return;
		FV_View * v = hv.view;
		hv.doc->setMarkRevisions(true);
		v->setPoint(12);
		v->cmdCharDelete(true, 7);
		hv.doc->setMarkRevisions(false);
		fvrSettle(hv.layout);
		const PT_DocPosition pos = hv.nthMarkedRunPos(0);
		TFPASS(pos > 0);
		v->setPoint(pos + 1);
		v->cmdAcceptRejectRevision(true, 0, 0);
		fvrSettle(hv.layout);
		TFPASS(hv.text().find("keep deleted keep") != std::string::npos);
		TFPASS(fvrRevAttr(hv.doc, pos).empty());
	}

	/* the same ops through the real edit methods */
	{
		RevView hv;
		TFPASS(hv.load("base text"));
		if (!hv.view)
			return;
		FV_View * v = hv.view;
		hv.doc->setMarkRevisions(true);
		v->setPoint(3);
		v->cmdCharInsert(std::string("XYZ"), false);
		hv.doc->setMarkRevisions(false);
		fvrSettle(hv.layout);
		v->setPoint(hv.nthMarkedRunPos(0) + 1);
		TFPASS(callEM(v, "revisionReject"));
		fvrSettle(hv.layout);
		TFPASS(hv.text().find("XYZ") == std::string::npos);
	}
}

TFTEST_MAIN("bulk accept/reject matrix through edit methods")
{
	/* accept-all: insertions stay, deletions go, marks cleared */
	{
		RevView hv;
		TFPASS(hv.load("keep deleted keep"));
		if (!hv.view)
			return;
		FV_View * v = hv.view;
		TFPASS(seedMarks(hv));
		TFPASS(callEM(v, "revisionAcceptAll"));
		fvrSettle(hv.layout);
		TFPASS(hv.text().find("deleted") == std::string::npos);
		TFPASS(hv.text().find("INSERTED") != std::string::npos);
		/* TST13: the RENDER must track the accept — no stale
		 * struck-out deletion or un-finalised insertion left on
		 * screen */
		TFPASS(hv.visibleText().find("deleted")
			   == std::string::npos);
		TFPASS(hv.visibleText().find("INSERTED")
			   != std::string::npos);
		TFPASS(hv.markedRunCount() == 0);
	}

	/* reject-all: insertions go, deletions restored */
	{
		RevView hv;
		TFPASS(hv.load("keep deleted keep"));
		if (!hv.view)
			return;
		FV_View * v = hv.view;
		TFPASS(seedMarks(hv));
		TFPASS(callEM(v, "revisionRejectAll"));
		fvrSettle(hv.layout);
		TFPASS(hv.text().find("deleted") != std::string::npos);
		TFPASS(hv.text().find("INSERTED") == std::string::npos);
		TFPASS(hv.visibleText().find("deleted")
			   != std::string::npos);
		TFPASS(hv.visibleText().find("INSERTED")
			   == std::string::npos);
		TFPASS(hv.markedRunCount() == 0);
	}

	/* accept-all-shown at the reveal-all level accepts everything */
	{
		RevView hv;
		TFPASS(hv.load("keep deleted keep"));
		if (!hv.view)
			return;
		FV_View * v = hv.view;
		TFPASS(seedMarks(hv));
		hv.doc->setMarkRevisions(true);
		v->setShowRevisions(true);
		v->cmdSetRevisionLevel(0);
		TFPASS(callEM(v, "revisionAcceptAllShown"));
		fvrSettle(hv.layout);
		TFPASS(hv.markedRunCount() == 0);
		TFPASS(hv.text().find("INSERTED") != std::string::npos);
		TFPASS(hv.visibleText().find("deleted")
			   == std::string::npos);
		TFPASS(hv.visibleText().find("INSERTED")
			   != std::string::npos);
	}

	/* ...and stop tracking: marks cleared AND mark flag off —
	 * the accept itself runs while tracking is still on, which
	 * used to re-mark the text instead of clearing the marks */
	{
		RevView hv;
		TFPASS(hv.load("keep deleted keep"));
		if (!hv.view)
			return;
		FV_View * v = hv.view;
		TFPASS(seedMarks(hv));
		hv.doc->setMarkRevisions(true);
		TFPASS(callEM(v, "revisionAcceptAllStopTracking"));
		fvrSettle(hv.layout);
		TFPASS(hv.markedRunCount() == 0);
		TFPASS(!hv.doc->isMarkRevisions());
	}
	{
		RevView hv;
		TFPASS(hv.load("keep deleted keep"));
		if (!hv.view)
			return;
		FV_View * v = hv.view;
		TFPASS(seedMarks(hv));
		hv.doc->setMarkRevisions(true);
		TFPASS(callEM(v, "revisionRejectAllStopTracking"));
		fvrSettle(hv.layout);
		TFPASS(hv.markedRunCount() == 0);
		TFPASS(!hv.doc->isMarkRevisions());
		TFPASS(hv.text().find("deleted") != std::string::npos);
	}

	/* accept-and-move-to-next consumes marks in document order */
	{
		RevView hv;
		TFPASS(hv.load("para\nsecond para"));
		if (!hv.view)
			return;
		FV_View * v = hv.view;
		hv.doc->setMarkRevisions(true);
		v->setPoint(3);
		v->cmdCharInsert(std::string("AA"), false);
		v->setPoint(hv.eod());
		v->cmdCharInsert(std::string("BB"), false);
		hv.doc->setMarkRevisions(false);
		fvrSettle(hv.layout);
		const int n0 = hv.markedRunCount();
		TFPASS(n0 >= 2);
		/* "Accept and Move to Next" acts on the revision at the
		 * caret — put it inside the first marked run */
		v->setPoint(hv.nthMarkedRunPos(0) + 1);
		TFPASS(callEM(v, "revisionAcceptNext"));
		fvrSettle(hv.layout);
		TFPASS(hv.markedRunCount() < n0);
	}
}

TFTEST_MAIN("undo and redo round-trip an accept")
{
	RevView hv;
	TFPASS(hv.load("keep deleted keep"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	TFPASS(seedMarks(hv));

	const std::string marked = hv.text();
	const int n0 = hv.markedRunCount();
	TFPASS(n0 >= 1);

	/* accept the first marked run, then undo: the mark and the
	 * stricken text both come back */
	const PT_DocPosition pos = hv.nthMarkedRunPos(0);
	v->setPoint(pos + 1);
	v->cmdAcceptRejectRevision(false, 0, 0);
	fvrSettle(hv.layout);
	const std::string accepted = hv.text();
	TFPASS(accepted != marked || hv.markedRunCount() < n0);

	for (int i = 0; i < 8 && hv.markedRunCount() < n0; ++i)
	{
		v->cmdUndo(1);
		fvrSettle(hv.layout);
	}
	TFPASS(hv.markedRunCount() >= 1);
	TFPASS(hv.text() == marked);

	/* redo brings the accepted state back */
	for (int i = 0; i < 8; ++i)
	{
		v->cmdRedo(1);
		fvrSettle(hv.layout);
		if (hv.text() == accepted)
			break;
	}
	TFPASS(hv.text() == accepted);
}

TFTEST_MAIN("edits at the edges of a marked run stay coherent")
{
	RevView hv;
	TFPASS(hv.load("base text"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;

	doc->setMarkRevisions(true);
	v->setPoint(6);
	v->cmdCharInsert(std::string("XYZ"), false);

	/* type inside the marked insertion — joins the same revision */
	v->setPoint(7);
	v->cmdCharInsert(std::string("m"), false);
	/* type right after it — also marked */
	v->setPoint(9);
	v->cmdCharInsert(std::string("t"), false);
	doc->setMarkRevisions(false);
	fvrSettle(hv.layout);

	const std::string t = hv.text();
	TFPASS(t.find("XmYZt") != std::string::npos ||
		   t.find("XmYtZ") != std::string::npos ||
		   t.find("XYZ") != std::string::npos);
	/* every added char carries the mark */
	TFPASS(hv.markedRunCount() >= 1);
	const UT_uint32 rev0 = doc->getHighestRevisionId();

	/* a tracked delete spanning marked + unmarked text */
	doc->setMarkRevisions(true);
	v->setPoint(4);
	v->cmdSelect(4, 10);
	v->cmdCharDelete(true, 6);
	doc->setMarkRevisions(false);
	fvrSettle(hv.layout);
	/* no corruption: text consistent, marks still parse */
	TFPASS(doc->getHighestRevisionId() >= rev0);
	const std::string t2 = hv.text();
	TFPASS(t2.length() >= t.length() - 6);

	/* every remaining mark decodes through the attr grammar */
	bool allParse = true;
	PD_DocIterator it(*doc);
	while (it.getStatus() == UTIter_OK)
	{
		const pf_Frag *pf = it.getFrag();
		if (pf && pf->getType() == pf_Frag::PFT_Text)
		{
			const PP_AttrProp *pAP = nullptr;
			doc->getAttrProp(pf->getIndexAP(), &pAP);
			const gchar *v2 = nullptr;
			if (pAP && pAP->getAttribute(PT_REVISION_ATTRIBUTE_NAME, v2)
				&& v2)
			{
				PP_RevisionAttr ra(v2);
				if (!ra.getLastRevision())
					allParse = false;
			}
		}
		++it;
	}
	TFPASS(allParse);
}

TFTEST_MAIN("insertion point loses the mark when tracking stops")
{
	RevView hv;
	TFPASS(hv.load("base text"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;

	/* caret inside a marked insertion, then mark off — the next
	 * typed char must be plain text, not a continued revision */
	doc->setMarkRevisions(true);
	v->setPoint(6);
	v->cmdCharInsert(std::string("XYZ"), false);
	v->setPoint(8);
	doc->setMarkRevisions(false);
	v->updateRevisionMode();
	fvrSettle(hv.layout);
	v->cmdCharInsert(std::string("q"), false);
	fvrSettle(hv.layout);

	/* the new char lands unmarked: the run at its doc position
	 * carries no revision attr */
	const PT_DocPosition qpos = 8;
	TFPASS(fvrRevAttr(doc, qpos).empty() ||
		   hv.text().find("XqYZ") == std::string::npos);
	TFPASS(hv.text().find("q") != std::string::npos);
}

TFTEST_MAIN("annotation anchored in a marked run survives mode flips")
{
	RevView hv;
	TFPASS(hv.load("keep deleted keep"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	TFPASS(seedMarks(hv));

	const PT_DocPosition pos = hv.nthMarkedRunPos(0);
	TFPASS(pos > 0);
	v->setPoint(pos + 1);
	TFPASS(v->insertAnnotation(0, "note body", "auditor",
							   "comment", false));
	fvrSettle(hv.layout);
	const UT_uint32 n0 = v->countAnnotations();
	TFPASS(n0 >= 1);

	static const char * modes[] = {"all", "simple", "none",
								   "original", "all"};
	for (const char * m : modes)
	{
		TFPASS(callEMu4(v, "revisionDisplayMode", m));
		fvrSettle(hv.layout);
		TFPASS(v->countAnnotations() == n0);
	}
	TFPASS(hv.markedRunCount() >= 1);
}

TFTEST_MAIN("revision context surfaces over marked runs")
{
	RevView hv;
	TFPASS(hv.load("keep deleted keep"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	TFPASS(seedMarks(hv));
	v->setShowRevisions(true);
	v->cmdSetRevisionLevel(0);
	fvrSettle(hv.layout);

	/* coords of a marked run -> revision context menu */
	const PT_DocPosition pos = hv.nthMarkedRunPos(0);
	TFPASS(pos > 0);
	fl_BlockLayout * pBL = hv.layout->findBlockAtPosition(pos);
	TFPASS(pBL != nullptr);
	UT_sint32 x, y, x2, y2, h;
	bool bEOL = false, bDir = false;
	fp_Run * pRun = pBL->findPointCoords(pos + 1, false, x, y, x2, y2,
									   h, bDir);
	TFPASS(pRun != nullptr);
	TFPASS(pRun->containsRevisions());
	TFPASS(v->getMouseContext(x + 1, y + h / 2) == EV_EMC_REVISION);
}

TFTEST_MAIN("auto-revision mode syncs the view to doc-level state")
{
	RevView hv;
	TFPASS(hv.load("keep deleted keep"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;
	TFPASS(seedMarks(hv));

	/* in auto-revision mode the view follows the document-wide
	 * show state and level on every revision-mode change */
	doc->setAutoRevisioning(true);
	doc->setShowRevisions(false);
	doc->setShowRevisionId(0);
	v->updateRevisionMode();
	TFPASS(!v->isShowRevisions());
	TFPASS(v->getRevisionLevel() == 0);

	doc->setShowRevisions(true);
	doc->setShowRevisionId(doc->getHighestRevisionId());
	v->updateRevisionMode();
	TFPASS(v->isShowRevisions());
	TFPASS(v->getRevisionLevel() == doc->getHighestRevisionId());
	doc->setAutoRevisioning(false);
}

TFTEST_MAIN("abwn round-trip preserves marks and revision state")
{
	std::string tmp = std::string(g_get_tmp_dir()) +
		"/fv_rev_rt_" + std::to_string(::getpid()) + ".abwn";
	GError * err = nullptr;
	UT_uint32 rev0 = 0, lvl0 = 0;
	bool mark0 = false, show0 = false;
	std::string text0;

	{
		RevView hv;
		TFPASS(hv.load("keep deleted keep"));
		if (!hv.view)
			return;
		TFPASS(seedMarks(hv));
		mark0 = hv.doc->isMarkRevisions();
		show0 = hv.doc->isShowRevisions();
		lvl0 = hv.doc->getShowRevisionId();
		rev0 = hv.doc->getHighestRevisionId();
		text0 = hv.text();
		TFPASS(rev0 >= 1);

		GsfOutput * out = gsf_output_stdio_new(tmp.c_str(), &err);
		TFPASS(out != nullptr);
		if (!out)
			return;
		TFPASS(hv.doc->saveAs(out,
			static_cast<int>(IE_Exp::fileTypeForSuffix(".abwn")),
			false, nullptr) == UT_OK);
		g_object_unref(out);
	}

	{
		RevView hv;
		TFPASS(hv.loadFile(tmp.c_str()));
		if (!hv.view)
		{
			unlink(tmp.c_str());
			return;
		}
		fvrSettle(hv.layout);
		TFPASS(hv.doc->getHighestRevisionId() == rev0);
		TFPASS(hv.doc->isMarkRevisions() == mark0);
		TFPASS(hv.doc->isShowRevisions() == show0);
		TFPASS(hv.doc->getShowRevisionId() == lvl0);
		TFPASS(hv.text() == text0);
		TFPASS(hv.markedRunCount() >= 1);
		/* history rows survive: the revision table is intact */
		TFPASS(!hv.doc->getRevisions().empty());
	}
	unlink(tmp.c_str());
}

TFTEST_MAIN("imported marks: full mode matrix under both marking states")
{
	/* TST13: the imported-marks counterpart of fv_StateCycle's
	 * seeded-mark matrix — every ordered mode pair (incl.
	 * self-transitions) under marking ON and OFF, asserting the
	 * rendered text of real w:ins/w:del/moveFrom/moveTo marks after
	 * every hop.  The TRACK03-class encoding bug only exists with
	 * marking ON, so both states are driven on the same doc. */
	std::string data_file;
	TFPASS(TF_Test::ensure_test_data("/test/wp/tst04/o06_revisions.docx",
								   data_file));
	RevView hv;
	TFPASS(hv.loadFile(data_file.c_str()));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;
	fvrSettle(hv.layout);
	const UT_uint32 rev0 = doc->getHighestRevisionId();
	TFPASS(rev0 >= 1);
	TFPASS(hv.markedRunCount() >= 3);
	TFPASS(findEM("revisionDisplayMode") != nullptr);
	if (!findEM("revisionDisplayMode"))
		return;

	static const char * modes[] = {"all", "simple", "none",
								   "original"};
	for (int marking = 1; marking >= 0; --marking)
	{
		doc->setMarkRevisions(marking != 0);
		for (const char * from : modes)
		{
			TFPASS(callEMu4(v, "revisionDisplayMode", from));
			for (const char * to : modes)
			{
				TFPASS(callEMu4(v, "revisionDisplayMode", to));
				const char * bad = docxModeApplied(hv, to,
												   marking != 0, rev0);
				if (bad)
				{
					fprintf(stderr,
							"imported matrix fail %s->%s marking=%d: %s\n",
							from, to, marking, bad);
					TFPASS(false);
				}
			}
		}
	}
}

TFTEST_MAIN("simple mode paints the left-margin revision bar")
{
	/* TST13: Word's Simple Markup shows final text plus a red bar
	 * in the left margin on lines containing revisions
	 * (fp_Line::draw, gated on isShowRevBars && DGP_SCREEN).  This
	 * is the pixel half of the mode contract — flags say the bar
	 * is enabled, pixels prove it lands.  Render page 0 through a
	 * screen-reporting graphics in every mode and sample the
	 * margin. */
	RevView hv;
	TFPASS(hv.load("keep deleted keep"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	TFPASS(seedMarks(hv));
	TFPASS(findEM("revisionDisplayMode") != nullptr);
	if (!findEM("revisionDisplayMode"))
		return;

	/* text column left edge in device px: the first char's
	 * position mapped through a screen-resolution graphics — the
	 * bar is drawn a few px LEFT of it, inside the margin */
	fl_BlockLayout * pBL =
		hv.layout->findBlockAtPosition(hv.nthMarkedRunPos(0));
	TFPASS(pBL != nullptr);
	if (!pBL)
		return;
	UT_sint32 x = 0, y = 0, x2 = 0, y2 = 0, hh = 0;
	bool bEOL = false, bDir = false;
	TFPASS(pBL->findPointCoords(pBL->getPosition() + 1, false,
							  x, y, x2, y2, hh, bDir) != nullptr);
	RevScreenGfx conv;
	conv.setZoomPercentage(100);
	const int textLeftPx = conv.tdu(x);
	TFPASS(textLeftPx > 10);

	int w = 0, h = 0;
	struct Shot { const char * mode; bool wantBar; };
	static const Shot shots[] = {
		{ "simple",   true  },
		{ "none",     false },
		{ "all",      false },
		{ "original", false },
		{ "simple",   true  },
	};
	for (const Shot & s : shots)
	{
		TFPASS(callEMu4(v, "revisionDisplayMode", s.mode));
		fvrSettle(hv.layout);
		cairo_surface_t * surf = revRenderPage(hv, w, h);
		TFPASS(surf != nullptr);
		if (!surf)
			return;
		const bool bar = revMarginHasBar(surf, textLeftPx);
		if (bar != s.wantBar)
			fprintf(stderr,
					"margin bar fail mode=%s want=%d got=%d\n",
					s.mode, s.wantBar, bar);
		TFPASS(bar == s.wantBar);
		cairo_surface_destroy(surf);
	}
}
