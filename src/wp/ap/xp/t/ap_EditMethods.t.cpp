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
 * Coverage sweep over the full ap_EditMethods table (~640 edit
 * methods) against a headless document + FV_View on a widget-less
 * GR_UnixCairoGraphics.
 *
 * Every method runs behind em_guard's in-process signal/watchdog
 * guard so a crashing or hanging method is classified and the
 * sweep continues.  Each call also gets a fresh doc/view (kept on
 * the heap so a fault inside the method skips the destructor
 * rather than dying in it) and the default main context is pumped
 * afterwards — the movement/delete methods arm a deferred
 * UT_Worker that only fires through the context.
 *
 * Methods that die legitimately headless (null-frame derefs, GTK
 * widget creation, message boxes, spell suggestions with no
 * suggestion context) are pinned by name in kExpectedDead — any
 * NEW death is a regression and fails the test.  Methods that
 * would end the process or produce real side effects (open a
 * browser, create frames) are skipped via kSkip instead.
 */

#include "tf_test.h"

#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "fl_DocLayout.h"
#include "fv_View.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "xap_EditMethods.h"
#include "ev_EditMethod.h"
#include "ut_growbuf.h"

#include "em_guard.h"

#include <glib.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

#define TFSUITE "core.wp.ap.editmethods"

namespace {

struct EMView
{
	EMView() = default;
	EMView(const EMView &) = delete;
	EMView &operator=(const EMView &) = delete;

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

	~EMView()
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

const char *kEMDoc =
	"alpha first paragraph\n"
	"beta second paragraph\n"
	"gamma third paragraph\n"
	"delta fourth paragraph\n"
	"epsilon fifth paragraph\n";

static bool s_run_method(EV_EditMethod *em, AV_View *view)
{
	/* generic payload: a few chars for insertData-style methods
	 * and a fake click position for the mouse-driven ones */
	UT_UCS4Char data[4] = { 'a', 'b', 'x', 0 };
	EV_EditMethodCallData cd(data, 3);
	cd.m_xPos = 50;
	cd.m_yPos = 50;
	return em->Fn(view, &cd);
}

struct EMCallCtx
{
	EV_EditMethod *em;
	EV_EditMethodCallData *empty;
	EMView *hv;
	bool sel;
	bool called;
};

static void s_em_thunk(void *v)
{
	EMCallCtx *c = static_cast<EMCallCtx *>(v);
	if (!c->hv->load(kEMDoc))
		return;
	c->hv->view->setPoint(6);
	if (c->sel)
		c->hv->view->cmdSelect(3, 10);
	s_run_method(c->em, c->hv->view);
	em_guard::pump();
	c->called = true;
	delete c->hv;
	c->hv = nullptr;
}

/* Run one method on a fresh doc/view under the signal guard. */
static em_guard::Outcome s_guarded_call(EV_EditMethod *em, bool bSelect)
{
	EMCallCtx ctx{em, nullptr, new EMView, bSelect, false};
	em_guard::Outcome out = em_guard::call(s_em_thunk, &ctx, 8000);
	if (out == em_guard::OK && !ctx.called)
	{
		/* view construction failed — infra failure, count as dead */
		delete ctx.hv;
		out = em_guard::FAULT;
	}
	else if (out != em_guard::OK && ctx.hv)
	{
		/* view is intentionally leaked — its state may be corrupt */
		ctx.hv->doc = nullptr;
	}
	return out;
}

/* methods never invoked in-process: they would end the test
 * process (exit/reallyExit paths), produce real-world side
 * effects (open a browser), or mutate global frame state
 * (create a zombie frame that then "owns" the app) */
const char *const kSkip[] = {
	"fileNew",
	"fileNewUsingTemplate",
	"fileOpen",
	"openRecent",
	"newWindow",
	"revisionCompareDocuments",
	"helpReportBug",
};

/* methods that legitimately cannot run headless: null-frame
 * derefs, GTK widget creation, message boxes, context menus,
 * spell-suggestion lookups with no suggestion context.  Anything
 * dying that is NOT in this list is a regression — the test
 * fails on it. */
const char *const kExpectedDead[] = {
	/* null XAP_Frame derefs */
	"beginHDrag",
	"fileRevert",
	"fileSaveAsWeb",
	"formatTOC",
	"refXRef",
	/* set the platform cursor on a real widget */
	"cursorDefault",
	"cursorHline",
	"cursorIBeam",
	"cursorImage",
	"cursorImageSize",
	"cursorLeftArrow",
	"cursorRightArrow",
	"cursorTOC",
	"cursorTopCell",
	"cursorVline",
	/* annotation at point (none) */
	"editAnnotation",
	/* RDF dialogs dereference the frame */
	"rdfInsertNewContact",
	"rdfInsertRef",
	"rdfStylesheetSettings",
	/* spell suggestions need an active context-menu word */
	"spellSuggest_1",
	"spellSuggest_2",
	"spellSuggest_3",
	"spellSuggest_4",
	"spellSuggest_5",
	"spellSuggest_6",
	"spellSuggest_7",
	"spellSuggest_8",
	"spellSuggest_9",
};

std::string em_doc_text(FV_View * v)
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

TFTEST_MAIN("edit-method table sweep (guarded in-process)")
{
	std::unique_ptr<EV_EditMethodContainer> pemc(AP_GetEditMethods());
	TFPASS(pemc != nullptr);
	const UT_uint32 count = pemc->countEditMethods();
	TFPASS(count > 500);

	em_guard::install();

	std::set<std::string> skip(kSkip, kSkip + G_N_ELEMENTS(kSkip));
	std::set<std::string> expected(kExpectedDead,
		kExpectedDead + G_N_ELEMENTS(kExpectedDead));

	UT_uint32 ran = 0;
	std::vector<std::string> dead, hung;
	for (UT_uint32 i = 0; i < count; ++i)
	{
		EV_EditMethod *em = pemc->getNthEditMethod(i);
		if (!em || !em->getName())
			continue;
		const std::string name = em->getName();
		if (skip.count(name) || expected.count(name))
			continue;
		switch (s_guarded_call(em, (i & 1) != 0))
		{
		case em_guard::OK:
			++ran;
			break;
		case em_guard::FAULT:
			dead.push_back(name);
			fprintf(stderr, "EMDEAD %s\n", name.c_str());
			break;
		case em_guard::HUNG:
			hung.push_back(name);
			fprintf(stderr, "EMHUNG %s\n", name.c_str());
			break;
		}
		TF_Test::pulse();
	}
	fprintf(stderr, "EMRAN %u of %u, dead %zu, hung %zu\n",
			ran, count, dead.size(), hung.size());
	/* anything that died or hung and is not explicitly expected is
	 * a headless regression */
	for (const std::string &n : dead)
		fprintf(stderr, "EMDEAD-unexpected %s\n", n.c_str());
	for (const std::string &n : hung)
		fprintf(stderr, "EMHUNG-unexpected %s\n", n.c_str());
	TFPASS(dead.empty());
	TFPASS(hung.empty());
	TFPASS(ran > count / 2);
}

TFTEST_MAIN("edit-method behavior spot checks")
{
	std::unique_ptr<EV_EditMethodContainer> pemc(AP_GetEditMethods());
	TFPASS(pemc != nullptr);

	EMView hv;
	TFPASS(hv.load(kEMDoc));
	FV_View * v = hv.view;

	EV_EditMethodCallData empty;

	/* insertData inserts the call-data text at the point
	 * (position 2 is the first character; position 1 is the
	 * block strux) */
	EV_EditMethod *em = pemc->findEditMethodByName("insertData");
	TFPASS(em != nullptr);
	v->setPoint(3);
	s_run_method(em, v);
	em_guard::pump();
	TFPASS(em_doc_text(v).find("aabxlpha") != std::string::npos);

	/* delEOL deletes from the point to the end of the line —
	 * synchronous (unlike delLeft/delRight which defer through
	 * the frequent-repeat worker and need a live frame) */
	em = pemc->findEditMethodByName("delEOL");
	TFPASS(em != nullptr);
	const size_t len0 = em_doc_text(v).size();
	v->setPoint(3);
	em->Fn(v, &empty);
	em_guard::pump();
	TFPASS(em_doc_text(v).size() < len0);

	/* selectAll selects the whole document */
	em = pemc->findEditMethodByName("selectAll");
	TFPASS(em != nullptr);
	em->Fn(v, &empty);
	TFPASS(v->isSelectionEmpty() == false);

	/* undo restores, redo re-applies */
	em = pemc->findEditMethodByName("undo");
	TFPASS(em != nullptr);
	const size_t len1 = em_doc_text(v).size();
	em->Fn(v, &empty);
	em_guard::pump();
	TFPASS(em_doc_text(v).size() != len1);
	em = pemc->findEditMethodByName("redo");
	TFPASS(em != nullptr);
	em->Fn(v, &empty);
	em_guard::pump();
	TFPASS(em_doc_text(v).size() == len1);

	/* unknown names return nullptr */
	TFPASS(pemc->findEditMethodByName("noSuchMethod") == nullptr);
}
