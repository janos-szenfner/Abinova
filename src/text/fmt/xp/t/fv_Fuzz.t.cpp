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
 * TST07: seeded random-interaction fuzzing — a bounded property test
 * over the UI-facing command surface.  A deterministic PRNG walks a
 * weighted table of actions (text insert/delete, selection, cursor
 * motion, char-format toggles, undo/redo, view + markup mode
 * switches, revision tracking, field ops, TOC insert/update/remove,
 * cover insert/remove, bookmarks) against a fresh scratch document
 * per seed.
 *
 * Every action runs inside tf_guard::call so a hang or crash becomes
 * a failure naming seed/step/action for exact repro instead of a
 * frozen suite.  Between steps the suite asserts the invariants:
 *   (i)   the document still parses — layout holds pages, the piece
 *         table bounds are sane, text extraction runs;
 *   (ii)  the main loop stays responsive — an idle sentinel must
 *         dispatch after each settle;
 *  (iii)  undo restores — draining the undo stack after the
 *         sequence must return the document text to its seed state;
 *   (iv)  the end state round-trips — the fuzzed document must
 *         serialize to .abwn and reimport cleanly.
 *
 * The seed set is fixed and small so `make check` stays bounded;
 * widen it locally by extending s_seeds when hunting a report.
 */

#include "tf_test.h"
#include "tf_guard.h"

#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "fl_DocLayout.h"
#include "fl_BlockLayout.h"
#include "fl_TOCLayout.h"
#include "fv_View.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "xap_EditMethods.h"
#include "ev_EditMethod.h"
#include "ie_exp.h"
#include "ie_types.h"
#include "ut_growbuf.h"
#include "ut_string_class.h"

#include <gsf/gsf-output-stdio.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define TFSUITE "core.text.fmt.fuzz"

namespace {

/* scratch doc: '\n' separates blocks; "H:..." lines get Heading 1 so
 * TOC actions have entries to collect */
struct FuzzView
{
	FuzzView() = default;
	FuzzView(const FuzzView &) = delete;
	FuzzView &operator=(const FuzzView &) = delete;

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
		view->setWindowSize(1000, 700);
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

	~FuzzView()
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

struct FuzzCtx
{
	FuzzView *hv = nullptr;
	UT_uint64 rng = 1;
	UT_uint32 seed = 0;
	int step = 0;
	const char * action = "";
	std::vector<std::string> * trace = nullptr;

	UT_uint64 next()
	{
		rng ^= rng << 13;
		rng ^= rng >> 7;
		rng ^= rng << 17;
		return rng;
	}
	UT_uint32 pick(UT_uint32 n)
	{
		return n ? static_cast<UT_uint32>(next() % n) : 0;
	}
	/* any legal insertion point: somewhere in [2, eod) — position 2
	 * is the first char slot after the first block strux */
	PT_DocPosition randPos()
	{
		PT_DocPosition e = hv->eod();
		if (e <= 3)
			return 2;
		PT_DocPosition p = 2 + static_cast<PT_DocPosition>(
				pick(static_cast<UT_uint32>(e - 3)));
		return p >= e ? e - 1 : p;
	}
};

/* every failure runs through here so the repro line is always
 * emitted before the assert fires */
bool fuzzFail(const FuzzCtx &c, const char *what)
{
	fprintf(stderr,
			"FUZZ-FAIL seed=%llu step=%d action=%s detail=%s\n",
			static_cast<unsigned long long>(c.seed), c.step,
			c.action ? c.action : "?", what);
	return false;
}

typedef void (*ActionFn)(FuzzCtx &);

struct FuzzAction
{
	const char * name;
	UT_uint32 weight;
	ActionFn fn;
};

/* Guard budgets are wall-clock; instrumented runs (valgrind, ASan)
 * slow actions ~10-40x, so a legit step can outlive the nominal
 * budget — and a watchdog reap mid-action abandons in-flight
 * allocations, which a leak checker then reports as definite leaks.
 * ABINOVA_FUZZ_BUDGET_SCALE multiplies every guard budget (like
 * TF_MAX_TEST_TIME scales the per-assert alarm); unset means 1. */
long budgetScale()
{
	static const long v = [] {
		const char * e = getenv("ABINOVA_FUZZ_BUDGET_SCALE");
		long n = e ? strtol(e, nullptr, 10) : 0;
		return n > 0 ? n : 1;
	}();
	return v;
}

/* run one action under the watchdog; the drain stays INSIDE the
 * guarded region because the deferred idle/timer work an action
 * queued is part of that action — a crash there is still its fault.
 * A HUNG/FAULT outcome is a step failure carrying the repro triple */
bool runStep(FuzzCtx &c, const FuzzAction &a, long budget_ms)
{
	c.action = a.name;
	tf_guard::Outcome o = tf_guard::call([&c, &a] {
		a.fn(c);
		tf_guard::drain_pending();
	}, budget_ms * budgetScale());
	if (o != tf_guard::OK)
		return fuzzFail(c, o == tf_guard::HUNG ? "action HUNG"
											   : "action FAULTED");
	return true;
}

/* invoke a registered edit method the way the UI does — UCS4
 * payload or none */
bool callEditMethod(FV_View *v, const char *name, const char *mode)
{
	static EV_EditMethodContainer * s_emc = AP_GetEditMethods();
	EV_EditMethod * em =
		s_emc ? s_emc->findEditMethodByName(name) : nullptr;
	if (!em)
		return false;
	if (mode)
	{
		UT_UCS4String m(mode);
		EV_EditMethodCallData cd(m.ucs4_str(),
								 static_cast<UT_uint32>(m.length()));
		em->Fn(v, &cd);
	}
	else
	{
		EV_EditMethodCallData cd;
		em->Fn(v, &cd);
	}
	return true;
}

/* ---- the action table --------------------------------------- */

void a_ins_text(FuzzCtx &c)
{
	static const char * const words[] = {
		"the ", "quick ", "brown fox ", "jumps ", "over ",
		"lazy dog. ", "caf\xC3\xA9 ", "na\xC3\xAFve ",
		"one two three ", "x", "  spaced  ", "endo."
	};
	c.hv->view->cmdCharInsert(std::string(words[c.pick(
				G_N_ELEMENTS(words))]), false);
}

void a_para_break(FuzzCtx &c)
{
	c.hv->view->insertParagraphBreak();
}

void a_del_char(FuzzCtx &c)
{
	c.hv->view->cmdCharDelete(c.pick(2) != 0, 1);
}

void a_sel_delete(FuzzCtx &c)
{
	FV_View * v = c.hv->view;
	PT_DocPosition beg = c.randPos();
	PT_DocPosition end = beg + static_cast<PT_DocPosition>(1 + c.pick(8));
	PT_DocPosition e = c.hv->eod();
	if (end > e)
		end = e;
	if (end <= beg)
		return;
	v->cmdSelectNoNotify(beg, end);
	v->cmdCharDelete(false, 1);   /* selection deletes as one op */
	v->cmdUnselectSelection();
}

void a_move(FuzzCtx &c)
{
	c.hv->view->setPoint(c.randPos());
}

void a_motion(FuzzCtx &c)
{
	c.hv->view->cmdCharMotion(c.pick(2) != 0, 1 + c.pick(8));
}

void a_toggle_fmt(FuzzCtx &c)
{
	static const char * const tgl[] = {
		"toggleBold", "toggleItalic", "toggleUline",
		"toggleStrike", "toggleSuper", "toggleSub"
	};
	callEditMethod(c.hv->view, tgl[c.pick(G_N_ELEMENTS(tgl))], nullptr);
}

void a_undo(FuzzCtx &c)
{
	if (c.hv->view->canDo(true))
		c.hv->view->cmdUndo(1);
}

void a_redo(FuzzCtx &c)
{
	if (c.hv->view->canDo(false))
		c.hv->view->cmdRedo(1);
}

void a_viewmode(FuzzCtx &c)
{
	static const ViewMode modes[] = { VIEW_WEB, VIEW_NORMAL,
									  VIEW_PRINT };
	c.hv->view->setViewMode(modes[c.pick(G_N_ELEMENTS(modes))]);
}

void a_markup(FuzzCtx &c)
{
	static const char * const mm[] = {
		"all", "simple", "none", "original"
	};
	callEditMethod(c.hv->view, "revisionDisplayMode",
				   mm[c.pick(G_N_ELEMENTS(mm))]);
}

void a_revtrack(FuzzCtx &c)
{
	c.hv->doc->setMarkRevisions(c.pick(2) != 0);
}

void a_field_ins(FuzzCtx &c)
{
	static const char * const ft[] = {
		"page_number", "page_count", "date", "time"
	};
	c.hv->view->cmdInsertField(ft[c.pick(G_N_ELEMENTS(ft))]);
}

void a_field_upd(FuzzCtx &c)
{
	c.hv->view->cmdUpdateField();   /* refuses cleanly off a field */
}

void a_toc(FuzzCtx &c)
{
	FV_View * v = c.hv->view;
	FL_DocLayout * l = c.hv->layout;
	if (!v->hasTOC())
	{
		v->setPoint(c.randPos());
		v->cmdInsertTOC();
		return;
	}
	fl_TOCLayout * toc = l->getNumTOCs() > 0 ? l->getNthTOC(0)
										   : nullptr;
	if (toc)
		v->setPoint(toc->getDocPosition() + 1);
	if (c.pick(2))
		v->cmdUpdateTOC();
	else
		v->cmdRemoveTOC();
}

void a_cover(FuzzCtx &c)
{
	FV_View * v = c.hv->view;
	if (v->hasCoverPage() && c.pick(2))
	{
		v->cmdRemoveCoverPage();
		return;
	}
	static const char * const presets[] = {
		"frame", "badge", "crop", "austin", "whip", "retrospect",
		"ion-dark", "filgree", "semaphore", "slice-light"
	};
	v->setPoint(2);
	v->cmdInsertCoverPage(presets[c.pick(G_N_ELEMENTS(presets))]);
}

void a_bookmark(FuzzCtx &c)
{
	UT_uint32 n = c.pick(6);
	char name[32];
	snprintf(name, sizeof(name), "fzbm%u", n);
	if (n < 4)
		c.hv->view->cmdInsertBookmark(name);
	else
		c.hv->view->cmdDeleteBookmark(name);
}

static const FuzzAction s_actions[] = {
	{ "ins_text",    12, a_ins_text },
	{ "para_break",   6, a_para_break },
	{ "del_char",     8, a_del_char },
	{ "sel_delete",   3, a_sel_delete },
	{ "move_point",   6, a_move },
	{ "char_motion",  4, a_motion },
	{ "toggle_fmt",   5, a_toggle_fmt },
	{ "undo",         6, a_undo },
	{ "redo",         4, a_redo },
	{ "view_mode",    2, a_viewmode },
	{ "markup_mode",  2, a_markup },
	{ "revtrack",     2, a_revtrack },
	{ "field_ins",    3, a_field_ins },
	{ "field_upd",    2, a_field_upd },
	{ "toc",          2, a_toc },
	{ "cover",        2, a_cover },
	{ "bookmark",     2, a_bookmark },
};

const FuzzAction & pickAction(FuzzCtx &c)
{
	UT_uint32 total = 0;
	for (const FuzzAction &a : s_actions)
		total += a.weight;
	UT_uint32 r = c.pick(total);
	for (const FuzzAction &a : s_actions)
	{
		if (r < a.weight)
			return a;
		r -= a.weight;
	}
	return s_actions[0];
}

/* invariant settle: the document must still parse (layout alive,
 * bounds sane, text extractable) and the main loop must still
 * service an idle callback.  Guarded like an action — deferred
 * dispatches can surface a crash here and it still needs the
 * repro line. */
bool settle(FuzzCtx &c)
{
	c.action = "settle";
	bool inner = false;
	tf_guard::Outcome o = tf_guard::call([&c, &inner] {
		tf_guard::drain_pending();
		if (c.hv->layout->countPages() < 1)
			return;
		PT_DocPosition e = c.hv->eod();
		if (e < 2)
			return;
		(void)c.hv->text();
		inner = true;
	}, 3000 * budgetScale());
	if (o != tf_guard::OK)
		return fuzzFail(c, o == tf_guard::HUNG ? "settle HUNG"
											   : "settle FAULTED");
	if (!inner)
		return fuzzFail(c, "layout/page-bounds invariant broke");
	if (tf_guard::idle_sentinel(600 * budgetScale()) < 0)
		return fuzzFail(c, "idle sentinel starved — loop wedged");
	return true;
}

/* undo everything the sequence did — 'undo restores' means the
 * drained document is textually identical to its seed state */
bool undoAll(FuzzCtx &c, int cap)
{
	FV_View * v = c.hv->view;
	c.action = "undo-all";
	for (int i = 0; v->canDo(true); ++i)
	{
		if (i >= cap)
			return fuzzFail(c, "undo stack did not drain");
		tf_guard::Outcome o = tf_guard::call([&v] {
			v->cmdUndo(1);
			tf_guard::drain_pending();
		}, 3000 * budgetScale());
		if (o != tf_guard::OK)
		{
			fprintf(stderr, "FUZZ-FAIL undo index %d outcome=%s\n",
					i, o == tf_guard::HUNG ? "HUNG" : "FAULT");
			return fuzzFail(c, "undo step hung/faulted");
		}
	}
	c.hv->doc->setMarkRevisions(false);
	c.hv->layout->formatAll();
	return true;
}

/* 'doc parses' at the end state: serialize to .abwn and reimport
 * it through the real importer — a corrupted piece table surfaces
 * here even when every single step looked fine.  Uses a temp file
 * because readFromFile(GsfInput*) reopens the input by NAME (it
 * ignores memory streams — no filename means a bogus reopen). */
bool exportReimport(PD_Document * doc)
{
	std::string tmp = std::string(g_get_tmp_dir()) +
		"/fv_fuzz_rt_" + std::to_string(::getpid()) + ".abwn";
	GError * err = nullptr;
	GsfOutput * out = gsf_output_stdio_new(tmp.c_str(), &err);
	bool ok = out &&
		doc->saveAs(out,
			static_cast<int>(IE_Exp::fileTypeForSuffix(".abwn")),
			false, nullptr) == UT_OK;
	if (out)
		g_object_unref(out);
	if (err)
		g_error_free(err);
	if (ok)
	{
		PD_Document * d2 = new PD_Document;
		ok = d2->readFromFile(tmp.c_str(), IEFT_Unknown,
							  nullptr) == UT_OK;
		d2->unref();
	}
	unlink(tmp.c_str());
	return ok;
}

/* one complete seed: load -> STEP_COUNT actions -> end-state
 * invariants -> undo-all -> text restored */
bool runSeed(FuzzCtx &c, UT_uint64 seed, int steps)
{
	tf_guard::Outcome lo = tf_guard::call([&c] {
		c.hv = new FuzzView;
	}, 5000 * budgetScale());
	if (lo != tf_guard::OK)
		return fuzzFail(c, "FuzzView allocation faulted");
	c.seed = static_cast<UT_uint32>(seed & 0xFFFFFFFF);
	c.rng = seed ? seed : 1;
	c.step = -1;
	c.action = "load";
	/* unbuffered stderr marker per seed — if a heap abort escapes
	 * the watchdog the repro still names the seed */
	fprintf(stderr, "FUZZ-SEED %llu start\n",
			static_cast<unsigned long long>(seed));
	if (tf_guard::call([&c] {
			c.hv->load("H:Alpha Chapter\nbody one alpha\n"
					   "body two alpha\nH:Beta Chapter\n"
					   "body beta\nlast body paragraph");
		}, 8000 * budgetScale()) != tf_guard::OK)
		return fuzzFail(c, "seed document load hung/faulted");
	if (!c.hv->view)
		return fuzzFail(c, "seed document failed to load");
	FV_View * v = c.hv->view;
	v->setPoint(2);
	const std::string text0 = c.hv->text();

	for (c.step = 0; c.step < steps; ++c.step)
	{
		const FuzzAction &a = pickAction(c);
		if (c.trace)
			c.trace->push_back(a.name);
		if (!runStep(c, a, 4000))
			return false;
		if (c.step % 6 == 5 && !settle(c))
			return false;
	}
	if (!settle(c))
		return false;

	c.action = "export-reimport";
	bool parsed = false;
	if (tf_guard::call([&c, &parsed] {
			parsed = exportReimport(c.hv->doc);
		}, 15000 * budgetScale()) != tf_guard::OK)
		return fuzzFail(c, "abwn export/reimport hung/faulted");
	if (!parsed)
		return fuzzFail(c, "fuzzed document fails abwn reimport");

	if (!undoAll(c, steps * 4 + 64))
		return false;
	if (c.hv->text() != text0)
		return fuzzFail(c, "undo-all did not restore the seed text");
	return true;
}

/* guarded teardown — if a fuzzed action corrupted the heap, the
 * FuzzView destructor is where it often surfaces */
bool dropView(FuzzCtx &c)
{
	c.action = "teardown";
	return tf_guard::call([&c] {
		delete c.hv;
		c.hv = nullptr;
	}, 8000 * budgetScale()) == tf_guard::OK;
}

} // namespace

TFTEST_MAIN("seeded action fuzz keeps doc and UI invariants")
{
	tf_guard::install();
	fflush(stdout);   /* don't lose the suite banner on a hard abort */
	static const UT_uint64 seeds[] = {
		0xA5A5A5A5ULL, 7ULL, 0x1BADB002ULL,
		424242ULL, 0xFEEDFACEULL, 99991ULL
	};
	/* repro override: ABINOVA_FUZZ_SEEDS="7,424242" narrows the set,
	 * ABINOVA_FUZZ_STEPS=N lengthens it — both stay deterministic */
	std::vector<UT_uint64> runSeeds(seeds, seeds + G_N_ELEMENTS(seeds));
	if (const char * env = getenv("ABINOVA_FUZZ_SEEDS"))
	{
		runSeeds.clear();
		const char * p = env;
		while (*p)
		{
			runSeeds.push_back(strtoull(p, nullptr, 0));
			while (*p && *p != ',')
				++p;
			if (*p == ',')
				++p;
		}
		if (runSeeds.empty())
			runSeeds.push_back(1);
	}
	int steps = 45;
	if (const char * env = getenv("ABINOVA_FUZZ_STEPS"))
	{
		int n = atoi(env);
		if (n > 0)
			steps = n;
	}
	for (UT_uint64 seed : runSeeds)
	{
		TF_Test::pulse();   /* each seed is internally bounded by the
		 * watchdog; reset the per-test alarm between them */
		FuzzCtx c;
		bool ok = runSeed(c, seed, steps);
		if (!ok)
		{
			fprintf(stderr, "FUZZ-FAIL repro: seed=%llu\n",
					static_cast<unsigned long long>(seed));
		}
		ok = dropView(c) && ok;
		TFPASS(ok);
		if (!ok)
			return;
	}
}

TFTEST_MAIN("same seed replays the same action sequence")
{
	/* determinism is the repro promise: two runs of one seed must
	 * choose identical actions — the recorded trace is what a bug
	 * report replays */
	tf_guard::install();
	std::vector<std::string> t1, t2;
	{
		FuzzCtx c;
		c.trace = &t1;
		TFPASS(runSeed(c, 0xC0FFEEULL, 40));
		dropView(c);
	}
	{
		FuzzCtx c;
		c.trace = &t2;
		TFPASS(runSeed(c, 0xC0FFEEULL, 40));
		dropView(c);
	}
	TFPASS(t1.size() == 40);
	TFPASS(t1 == t2);
}

TFTEST_MAIN("the fuzz path flags a wedged action instead of hanging")
{
	/* Negative control: a poisoned action that sleeps past its
	 * budget must be reaped by the watchdog, reported with the
	 * seed/action triple, and the dispatcher must stay usable for
	 * the next step — otherwise the fuzz suite itself is blind. */
	tf_guard::install();
	FuzzCtx c;
	c.seed = 0xDEAD;
	c.step = 7;
	/* the wedge must oversleep the (scaled) budget and the elapsed
	 * bound must leave room for the scaled watchdog interval */
	FuzzAction wedge{ "wedge-sleep", 1,
		[](FuzzCtx &) { g_usleep(3 * G_USEC_PER_SEC * budgetScale()); } };
	gint64 t0 = g_get_monotonic_time();
	TFPASS(!runStep(c, wedge, 300));
	TFPASS(g_get_monotonic_time() - t0 < 2500000 * budgetScale());
	FuzzAction noop{ "noop", 1, [](FuzzCtx &) {} };
	TFPASS(runStep(c, noop, 500));
}
