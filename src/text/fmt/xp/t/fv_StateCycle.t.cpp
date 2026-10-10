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
#include "fp_TextRun.h"
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

	/* text as rendered: only runs the current revision-display
	 * mode leaves visible — unlike text(), hidden revisions are
	 * skipped, so this is what the screen actually shows */
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

/* TST13: the per-mode expectation table — flags are one oracle,
 * rendered visible text is the independent one; a mode is only
 * 'applied' when both agree.  Expected content is identical under
 * both marking states: only the stored level encoding differs
 * ('all' stores 0 while tracking, PD_MAX_REVISION otherwise). */
struct ModeExpect
{
	const char * name;
	bool sR, bars;
	bool seeDeleted, seeInserted;
};

const ModeExpect * expectMode(const char *name)
{
	static const ModeExpect tbl[] = {
		{ "all",      true,  false, true,  true  },
		{ "simple",   false, true,  false, true  },
		{ "none",     false, false, false, true  },
		{ "original", false, false, true,  false },
	};
	for (const ModeExpect & e : tbl)
		if (!strcmp(e.name, name))
			return &e;
	return nullptr;
}

/* seed a tracked deletion of "deleted" plus tracked insertions at
 * two revision ids (INS1 under the first session, INS2 after a
 * revision-id bump) — leaves marking ON so callers set the state
 * per pass */
bool seedTwoIdMarks(CycleView &hv)
{
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;
	doc->setMarkRevisions(true);
	v->setPoint(12);
	v->cmdCharDelete(true, 7);
	v->setPoint(hv.eod());
	v->cmdCharInsert(std::string(" INS1"), false);
	/* a second session id: bump the working id and re-arm marking
	 * so the new id gets a revision-table row — marks at two ids */
	doc->setMarkRevisions(false);
	doc->setRevisionId(doc->getHighestRevisionId() + 1);
	doc->setMarkRevisions(true);
	v->cmdCharInsert(std::string(" INS2"), false);
	settleLayout(hv.layout);
	return doc->getHighestRevisionId() >= 2;
}

/* full landed-state check for one display mode: flag triple,
 * derived level, rendered visible text AND data preservation.
 * Returns a string naming the first failing oracle so the repro
 * line says WHAT diverged. */
const char * modeApplied(CycleView &hv, const char *name, bool marking,
						 UT_uint32 rev0)
{
	FV_View * v = hv.view;
	const ModeExpect * e = expectMode(name);
	if (!e)
		return "unknown mode";
	settleLayout(hv.layout);
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
	if ((vis.find("deleted") != std::string::npos) != e->seeDeleted)
		return e->seeDeleted ? "deleted text not rendered"
							 : "deleted text still rendered";
	if ((vis.find("INS1") != std::string::npos) != e->seeInserted)
		return e->seeInserted ? "inserted text not rendered"
							  : "inserted text still rendered";
	if ((vis.find("INS2") != std::string::npos) != e->seeInserted)
		return "second-id insertion visibility";
	if (hv.doc->getHighestRevisionId() != rev0)
		return "revision data dropped";
	return nullptr;
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

TFTEST_MAIN("markup modes render marked/final/original text")
{
	/* TRACK03: flag combos alone did not catch that 'all' under
	 * active tracking collapsed to the final text and 'original'
	 * rendered a union of both.  Assert the rendered (visible)
	 * text itself under BOTH isMarkRevisions states — the level
	 * encoding differs (reveal-all is level 0 while tracking),
	 * the pixels must not. */
	CycleView hv;
	TFPASS(hv.load("keep deleted keep"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;

	/* seed a tracked deletion + insertion; tracking stays ON for
	 * the first pass so the marking-mode level encoding is hit */
	doc->setMarkRevisions(true);
	v->setPoint(7);
	v->cmdCharDelete(true, 7);
	v->setPoint(hv.eod());
	v->cmdCharInsert(std::string(" inserted"), false);
	settleLayout(hv.layout);

	const UT_uint32 rev0 = doc->getHighestRevisionId();
	TFPASS(rev0 >= 1);
	TFPASS(markupMethod() != nullptr);
	if (!markupMethod())
		return;

	struct Mode { const char *name; bool sR, bars;
				  bool seeDeleted, seeInserted; };
	static const Mode seq[] = {
		{ "all",      true,  false, true,  true  },
		{ "simple",   false, true,  false, true  },
		{ "none",     false, false, false, true  },
		{ "original", false, false, true,  false },
		{ "all",      true,  false, true,  true  },
		{ "original", false, false, true,  false },
		{ "none",     false, false, false, true  },
		{ "simple",   false, true,  false, true  },
	};

	for (int marking = 1; marking >= 0; --marking)
	{
		doc->setMarkRevisions(marking != 0);
		/* 'all' stores the reveal-all level 0 under tracking,
		 * PD_MAX_REVISION otherwise */
		const UT_uint32 allLvl = marking ? 0 : PD_MAX_REVISION;
		for (size_t i = 0; i < G_N_ELEMENTS(seq); ++i)
		{
			TFPASS(applyMarkupMode(v, seq[i].name));
			TFPASS(v->isShowRevisions() == seq[i].sR);
			TFPASS(v->isShowRevBars() == seq[i].bars);
			const UT_uint32 want =
				!strcmp(seq[i].name, "original") ? 0 :
				(seq[i].sR ? allLvl : PD_MAX_REVISION);
			TFPASS(v->getRevisionLevel() == want);
			settleLayout(hv.layout);
			const std::string vis = hv.visibleText();
			TFPASS((vis.find("deleted") != std::string::npos)
				   == seq[i].seeDeleted);
			TFPASS((vis.find("inserted") != std::string::npos)
				   == seq[i].seeInserted);
			/* display mode must never destroy revision data */
			TFPASS(doc->getHighestRevisionId() == rev0);
		}
	}
}

TFTEST_MAIN("markup modes: full 4x4 matrix lands every transition")
{
	/* TST13: every ordered mode pair incl. self-transitions, under
	 * both marking states.  A transition whose stored triple
	 * partially matches the target can leave the render on the old
	 * mode when nothing rebuilds — the flag asserts alone cannot
	 * see that, so the rendered visible text is asserted after
	 * EVERY hop on a fixture with marks at two revision ids. */
	CycleView hv;
	TFPASS(hv.load("keep deleted keep"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;
	TFPASS(seedTwoIdMarks(hv));
	const UT_uint32 rev0 = doc->getHighestRevisionId();
	TFPASS(markupMethod() != nullptr);
	if (!markupMethod())
		return;

	static const char * modes[] = {"all", "simple", "none",
								   "original"};
	for (int marking = 1; marking >= 0; --marking)
	{
		doc->setMarkRevisions(marking != 0);
		for (const char * from : modes)
		{
			TFPASS(applyMarkupMode(v, from));
			for (const char * to : modes)
			{
				TFPASS(applyMarkupMode(v, to));
				const char * bad = modeApplied(hv, to,
											   marking != 0, rev0);
				if (bad)
				{
					fprintf(stderr,
							"mode matrix fail %s->%s marking=%d: %s\n",
							from, to, marking, bad);
					TFPASS(false);
				}
			}
		}
	}
}

TFTEST_MAIN("markup modes: seeded random order stays rendered-correct")
{
	/* TST13: multi-cycle random-order transitions — fv_Fuzz's
	 * harness shape applied to the selector.  A deterministic
	 * xorshift walk over the four modes asserts rendered content
	 * after every hop so a path-dependent wedge can't hide behind
	 * the fixed sequences above. */
	CycleView hv;
	TFPASS(hv.load("keep deleted keep"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;
	TFPASS(seedTwoIdMarks(hv));
	const UT_uint32 rev0 = doc->getHighestRevisionId();
	TFPASS(markupMethod() != nullptr);
	if (!markupMethod())
		return;

	static const char * modes[] = {"all", "simple", "none",
								   "original"};
	static const UT_uint64 seeds[] = {0x9E3779B9ULL, 424242ULL};
	for (int marking = 1; marking >= 0; --marking)
	{
		doc->setMarkRevisions(marking != 0);
		for (UT_uint64 seed : seeds)
		{
			UT_uint64 rng = seed;
			for (int step = 0; step < 24; ++step)
			{
				rng ^= rng << 13;
				rng ^= rng >> 7;
				rng ^= rng << 17;
				const char * to = modes[rng % 4];
				TFPASS(applyMarkupMode(v, to));
				const char * bad = modeApplied(hv, to,
											   marking != 0, rev0);
				if (bad)
				{
					fprintf(stderr,
							"random seq fail seed=%llu step=%d to=%s"
							" marking=%d: %s\n",
							static_cast<unsigned long long>(seed),
							step, to, marking, bad);
					TFPASS(false);
				}
			}
		}
	}
}

TFTEST_MAIN("pinned pre-TRACK03 'all' state fails the content oracle")
{
	/* TST13 negative control for the TRACK03 class: under marking
	 * ON the pre-fix 'all' branch stored PD_MAX_REVISION — a flag
	 * triple (show, !bars, MAX) the OLD flag asserts accepted as
	 * All Markup while the render showed No Markup text.  Pin that
	 * exact state through the public API and prove the content
	 * oracle disagrees where the flag table cannot. */
	CycleView hv;
	TFPASS(hv.load("keep deleted keep"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;
	TFPASS(seedTwoIdMarks(hv));

	/* the pre-fix 'all' landing, verbatim */
	v->setShowRevBars(false);
	v->setShowRevisions(true);
	v->cmdSetRevisionLevel(PD_MAX_REVISION);
	settleLayout(hv.layout);

	/* flags look exactly like the old 'all' expectation… */
	TFPASS(v->isShowRevisions());
	TFPASS(!v->isShowRevBars());
	TFPASS(v->getRevisionLevel() == PD_MAX_REVISION);
	/* …but the render is No Markup: 'deleted' hidden means the
	 * content assert FAILS on this state — an assert that can't
	 * fail tests nothing */
	std::string vis = hv.visibleText();
	TFPASS(vis.find("deleted") == std::string::npos);
	TFPASS(vis.find("INS1") != std::string::npos);

	/* the SAME flag triple under marking OFF is a legitimate All
	 * Markup — flags alone are ambiguous across marking states,
	 * which is why only rendered content can arbitrate */
	doc->setMarkRevisions(false);
	settleLayout(hv.layout);
	vis = hv.visibleText();
	TFPASS(v->isShowRevisions());
	TFPASS(v->getRevisionLevel() == PD_MAX_REVISION);
	TFPASS(vis.find("deleted") != std::string::npos);
	TFPASS(vis.find("INS1") != std::string::npos);

	/* and back to a real landed mode */
	TFPASS(applyMarkupMode(v, "none"));
	TFPASS(modeApplied(hv, "none", false,
					 doc->getHighestRevisionId()) == nullptr);
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
