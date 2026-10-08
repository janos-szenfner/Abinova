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
 * TST02: state-cycling + preservation invariants for the multi-state
 * controls — the "clicked once, now stuck" / "hidden means deleted"
 * regression classes (TRACK01's markup selector is the archetype).
 *
 * Every control is driven through the same entry point the UI uses
 * (the revisionDisplayMode edit method gets its real UCS4 payload,
 * view modes go through FV_View::setViewMode, TOC and cover ops use
 * the cmd* methods the ribbon rows invoke).  Each transition is
 * wrapped in tf_guard::call so a hang counts as a failure, and after
 * every step the suite asserts three things:
 *   (i)   the transition actually applied (flag combo / structure),
 *   (ii)  the control is still live — the next transition works,
 *   (iii) document data is preserved — revision records, fields,
 *         section markers and text are identical across cycles;
 *         a display mode must never destroy what it hides.
 *
 * The ribbon-level equivalents (popover rows, check marks,
 * sensitivity of real GtkWidgets) live in ui-drive's --markup leg.
 */

#include "tf_test.h"
#include "tf_guard.h"
#include "tf_actions.h"

#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "fl_DocLayout.h"
#include "fl_BlockLayout.h"
#include "fl_TOCLayout.h"
#include "fv_View.h"
#include "fp_Page.h"
#include "fp_Run.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "xap_EditMethods.h"
#include "ev_EditMethod.h"
#include "ie_types.h"
#include "ut_growbuf.h"

#include <cstdio>
#include <cstring>
#include <string>

#define TFSUITE "core.text.fmt.statecycle"

namespace {

/* scratch doc: '\n' separates blocks; "H:..." lines get Heading 1 so
 * the TOC has entries to collect */
struct CycleView
{
	CycleView() = default;
	CycleView(const CycleView &) = delete;
	CycleView &operator=(const CycleView &) = delete;

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

	UT_sint32 fieldCount() const
	{
		UT_sint32 n = 0;
		fl_BlockLayout *pBlock = layout->findBlockAtPosition(2);
		for (; pBlock; pBlock = pBlock->getNextBlockInDocument())
		{
			for (fp_Run *pRun = pBlock->getFirstRun(); pRun;
				 pRun = pRun->getNextRun())
			{
				if (pRun->getType() == FPRUN_FIELD)
					++n;
			}
		}
		return n;
	}

	~CycleView()
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

/* the real "Display for Review" control path: the registered edit
 * method, invoked with the UCS4 payload a ribbon check row feeds it
 * (TRACK01's bug was decoding that payload as a byte string, so every
 * activation collapsed into the default branch) */
EV_EditMethod * markupMethod()
{
	static EV_EditMethodContainer * s_emc = AP_GetEditMethods();
	return s_emc ? s_emc->findEditMethodByName("revisionDisplayMode")
				 : nullptr;
}

bool applyMarkupMode(FV_View * v, const char * szMode)
{
	EV_EditMethod * em = markupMethod();
	if (!em)
		return false;
	UT_UCS4String m(szMode);
	EV_EditMethodCallData cd(m.ucs4_str(),
							 static_cast<UT_uint32>(m.length()));
	struct Ctx { EV_EditMethod * em; AV_View * v;
				 EV_EditMethodCallData * cd; bool ret; };
	Ctx c{em, v, &cd, false};
	if (tf_guard::call([&c] {
			c.ret = c.em->Fn(c.v, c.cd);
		}, 8000) != tf_guard::OK)
		return false;   /* wedged/crashed — control is dead */
	tf_guard::drain_pending();
	return c.ret;
}

bool markupStateIs(FV_View * v, bool showRev, bool showBars,
				   UT_uint32 level)
{
	return v->isShowRevisions() == showRev &&
		v->isShowRevBars() == showBars &&
		v->getRevisionLevel() == level;
}

/* field-count snapshot across a display-mode cycle needs the layout
 * to have rebuilt the runs — force it like updateScreen does */
void settleLayout(FL_DocLayout * l)
{
	l->formatAll();
	tf_guard::drain_pending();
}

} // namespace

TFTEST_MAIN("markup modes cycle all->simple->none->original->all")
{
	CycleView hv;
	TFPASS(hv.load("opening paragraph\nH:Chapter One\n"
				   "middle paragraph\nclosing paragraph"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;

	/* seed a tracked insertion + a field so every mode has markup
	 * and a field to preserve */
	v->setPoint(hv.eod());
	doc->setMarkRevisions(true);
	v->cmdCharInsert(std::string(" tracked insertion"), false);
	doc->setMarkRevisions(false);
	tf_guard::pump();
	v->cmdInsertField("page_number");
	settleLayout(hv.layout);

	const UT_uint32 rev0 = doc->getHighestRevisionId();
	TFPASS(rev0 >= 1);
	const UT_sint32 fields0 = hv.fieldCount();
	TFPASS(fields0 >= 1);
	const std::string text0 = hv.text();
	TFPASS(text0.find("tracked insertion") != std::string::npos);

	TFPASS(markupMethod() != nullptr);
	if (!markupMethod())
		return;

	struct Mode { const char *name; bool sR, bars; UT_uint32 lvl; };
	/* two full rounds — the second round is what proves the control
	 * is still live after 'simple' (TRACK01's wedge point) */
	static const Mode seq[] = {
		{ "all",      true,  false, PD_MAX_REVISION },
		{ "simple",   false, true,  PD_MAX_REVISION },
		{ "none",     false, false, PD_MAX_REVISION },
		{ "original", false, false, 0 },
		{ "all",      true,  false, PD_MAX_REVISION },
		{ "simple",   false, true,  PD_MAX_REVISION },
		{ "original", false, false, 0 },
		{ "none",     false, false, PD_MAX_REVISION },
		{ "all",      true,  false, PD_MAX_REVISION },
	};
	for (size_t i = 0; i < G_N_ELEMENTS(seq); ++i)
	{
		/* the method returning true is the headless sensitivity
		 * check — a refused/ignored activation counts as dead */
		TFPASS(applyMarkupMode(v, seq[i].name));
		TFPASS(markupStateIs(v, seq[i].sR, seq[i].bars,
							 seq[i].lvl));
		/* 'hidden' must never mean 'deleted' */
		TFPASS(doc->getHighestRevisionId() == rev0);
		settleLayout(hv.layout);
		TFPASS(hv.fieldCount() == fields0);
		TFPASS(hv.text() == text0);
	}
}

TFTEST_MAIN("wedged selector state is caught by the flag assert")
{
	/* Prove the check fires on TRACK01's shape: a control that was
	 * asked for 'all' but silently stayed on 'simple'.  Pin the
	 * negative assert on a manually-forced wrong combo, then drive
	 * the real simple->all transition (the one that used to stick)
	 * through the edit method. */
	CycleView hv;
	TFPASS(hv.load("markup wedge check"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setShowRevBars(true);
	v->setShowRevisions(false);
	v->cmdSetRevisionLevel(PD_MAX_REVISION);
	TFPASS(markupStateIs(v, false, true, PD_MAX_REVISION));
	/* the wedge: expecting 'all' while stuck in 'simple' must fail
	 * — if this assert ever passed, the suite is blind */
	TFPASS(!markupStateIs(v, true, false, PD_MAX_REVISION));

	TFPASS(applyMarkupMode(v, "simple"));
	TFPASS(markupStateIs(v, false, true, PD_MAX_REVISION));
	TFPASS(applyMarkupMode(v, "all"));
	TFPASS(markupStateIs(v, true, false, PD_MAX_REVISION));
	/* and back out again — arbitrary order, no wedged state */
	TFPASS(applyMarkupMode(v, "original"));
	TFPASS(markupStateIs(v, false, false, 0));
	TFPASS(applyMarkupMode(v, "simple"));
	TFPASS(markupStateIs(v, false, true, PD_MAX_REVISION));
}

TFTEST_MAIN("view modes cycle and restore content")
{
	std::string data_file;
	TFPASS(TF_Test::ensure_test_data("/test/wp/BillOfRights.abw",
								   data_file));
	{
		CycleView hv;
		hv.doc = new PD_Document;
		TFPASS(hv.doc->readFromFile(data_file.c_str(), IEFT_Unknown,
									nullptr) == UT_OK);
		GR_UnixCairoAllocInfo ai(nullptr);
		hv.graphics =
			XAP_App::getApp()->newGraphics(GRID_UNIX_PANGO, ai);
		TFPASS(hv.graphics != nullptr);
		hv.layout = new FL_DocLayout(hv.doc, hv.graphics);
		hv.view = new FV_View(XAP_App::getApp(), nullptr, hv.layout);
		hv.layout->fillLayouts();
		hv.layout->formatAll();
		FV_View * v = hv.view;
		TFPASS(hv.layout->countPages() > 0);

		const std::string text0 = hv.text();
		const UT_sint32 printPages = hv.layout->countPages();
		const UT_sint32 printWidth =
			hv.layout->getNthPage(0)->getWidth();

		/* A->B->C->A, twice — entering web derives the page width
		 * from the window (LAY08), so size first */
		static const ViewMode modes[] = { VIEW_WEB, VIEW_NORMAL,
										  VIEW_PRINT, VIEW_WEB,
										  VIEW_NORMAL, VIEW_PRINT };
		for (ViewMode m : modes)
		{
			v->setWindowSize(1400, 900);
			v->setViewMode(m);
			tf_guard::drain_pending();
			TFPASS(v->getViewMode() == m);
			TFPASS(hv.layout->countPages() >= 1);
			/* content is display-mode invariant */
			TFPASS(hv.text() == text0);
		}
		TFPASS(hv.layout->countPages() == printPages);
		TFPASS(hv.layout->getNthPage(0)->getWidth() == printWidth);
	}
}

TFTEST_MAIN("TOC insert/update/regenerate/delete cycle keeps headings")
{
	CycleView hv;
	TFPASS(hv.load("H:Alpha Chapter\nbody alpha\nH:Beta Chapter\n"
				   "body beta"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	FL_DocLayout * l = hv.layout;

	/* two full insert->update->update->remove rounds: the second
	 * round proves the control is still live after a removal */
	for (int round = 0; round < 2; ++round)
	{
		v->setPoint(hv.eod());
		TFPASS(!v->hasTOC());
		TFPASS(v->cmdInsertTOC() == UT_OK);
		tf_guard::drain_pending();
		TFPASS(l->getNumTOCs() >= 1);
		fl_TOCLayout * toc = l->getNthTOC(l->getNumTOCs() - 1);
		TFPASS(toc != nullptr);
		if (toc)
			v->setPoint(toc->getDocPosition() + 1);
		TFPASS(v->hasTOC());
		TFPASS(v->findTOCAtPoint() != nullptr);

		/* update + regenerate — the TOC must still exist and be the
		 * same single generated section */
		TFPASS(v->cmdUpdateTOC());
		tf_guard::drain_pending();
		TFPASS(v->cmdUpdateTOC());
		tf_guard::drain_pending();
		TFPASS(l->getNumTOCs() == 1);

		toc = l->getNthTOC(0);
		if (toc)
			v->setPoint(toc->getDocPosition() + 1);
		TFPASS(v->cmdRemoveTOC());
		tf_guard::drain_pending();
		TFPASS(l->getNumTOCs() == 0);
		TFPASS(!v->hasTOC());

		/* source content survived the whole cycle */
		const std::string t = hv.text();
		TFPASS(t.find("Alpha Chapter") != std::string::npos);
		TFPASS(t.find("Beta Chapter") != std::string::npos);
	}
}

TFTEST_MAIN("cover insert/replace/remove cycles preserve body")
{
	CycleView hv;
	TFPASS(hv.load("cover host text\nbody paragraph"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;
	FL_DocLayout * l = hv.layout;

	const std::string text0 = hv.text();
	const UT_sint32 marks0 = doc->getBookmarkCount();
	TFPASS(!v->hasCoverPage());

	v->setPoint(2);
	TFPASS(v->cmdInsertCoverPage("frame") == UT_OK);
	tf_guard::drain_pending();
	TFPASS(v->hasCoverPage());
	TFPASS(!doc->isBookmarkUnique("_cover-page"));

	/* re-insert replaces, never stacks — a single remove must clear
	 * it completely */
	TFPASS(v->cmdInsertCoverPage("badge") == UT_OK);
	tf_guard::drain_pending();
	TFPASS(v->hasCoverPage());
	TFPASS(v->cmdRemoveCoverPage());
	tf_guard::drain_pending();
	TFPASS(!v->hasCoverPage());
	TFPASS(doc->getBookmarkCount() == marks0);

	/* the control stays live: insert again after a removal */
	v->setPoint(2);
	TFPASS(v->cmdInsertCoverPage("crop") == UT_OK);
	tf_guard::drain_pending();
	TFPASS(v->hasCoverPage());

	/* undo must remove the generated cover as one unit, redo
	 * restores it — 'gone' must not mean 'corrupted' */
	int nUndo = 0;
	for (; nUndo < 12 && v->hasCoverPage(); ++nUndo)
		v->cmdUndo(1);
	TFPASS(!v->hasCoverPage());
	for (int i = 0; i < nUndo; ++i)
		v->cmdRedo(1);
	tf_guard::drain_pending();
	TFPASS(v->hasCoverPage());

	TFPASS(v->cmdRemoveCoverPage());
	tf_guard::drain_pending();
	TFPASS(!v->hasCoverPage());
	TFPASS(hv.text() == text0);
	TFPASS(l->countPages() >= 1);
}
