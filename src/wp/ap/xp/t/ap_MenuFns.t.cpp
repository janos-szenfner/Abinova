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
 * Headless coverage of the wp/ap/xp "function" surfaces:
 *
 *  - the full menu action set: every ap_GetState_* and
 *    ap_GetLabel_* hook registered in AP_CreateMenuActionSet(),
 *    driven on a real FV_View (plain point + active selection so
 *    both branches of selection-gated state functions run)
 *  - the full toolbar action set: every ap_ToolbarGetState_* hook
 *  - XAP_Toolbar_Icons binary-search lookups (ap_Toolbar_Icons.cpp)
 *  - AP_Convert: real headless .abw -> .txt/.rtf conversion,
 *    plus the print paths via a widget-less graphics
 *  - AP_Preview_Abi: preview construction + drawImmediate for the
 *    non-scroll preview modes
 *
 * State/label hooks run behind em_guard's in-process
 * signal/watchdog guard; any hook that cannot run without a frame
 * is reported by name and must be pinned in the expected list —
 * unpinned deaths are regressions and fail the test.
 */

#include "tf_test.h"

#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "fl_DocLayout.h"
#include "fv_View.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "xap_Menu_ActionSet.h"
#include "xap_Toolbar_ActionSet.h"
#include "xap_Toolbar_Icons.h"
#include "ev_Menu_Actions.h"
#include "ev_Menu_Labels.h"
#include "ev_Toolbar_Actions.h"
#include "ap_Menu_Id.h"
#include "ap_Toolbar_Id.h"
#include "ap_Convert.h"
#include "ap_Preview_Abi.h"
#include "ut_growbuf.h"
#include "ut_misc.h"

#include "em_guard.h"

#include <glib.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#define TFSUITE "core.wp.ap.menufns"

namespace {

struct MFView
{
	MFView() = default;
	MFView(const MFView &) = delete;
	MFView &operator=(const MFView &) = delete;

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

	~MFView()
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

const char *kMFDoc =
	"alpha first paragraph\n"
	"beta second paragraph\n"
	"gamma third paragraph\n"
	"delta fourth paragraph\n"
	"epsilon fifth paragraph\n";

struct MenuProbeCtx
{
	const EV_Menu_Action *action;
	FV_View *view;
	EV_Menu_Label *label;
};

static void s_menu_state_thunk(void *v)
{
	MenuProbeCtx *c = static_cast<MenuProbeCtx *>(v);
	(void)c->action->getMenuItemState(c->view);
}

static void s_menu_label_thunk(void *v)
{
	MenuProbeCtx *c = static_cast<MenuProbeCtx *>(v);
	(void)c->action->getDynamicLabel(c->label);
}

struct ToolbarProbeCtx
{
	const EV_Toolbar_Action *action;
	FV_View *view;
};

static void s_toolbar_state_thunk(void *v)
{
	ToolbarProbeCtx *c = static_cast<ToolbarProbeCtx *>(v);
	const char *sz = nullptr;
	(void)c->action->getToolbarItemState(c->view, &sz);
}

/* expose the protected XAP_Toolbar_Icons lookup helpers */
struct TestIcons : public XAP_Toolbar_Icons
{
	static bool findName(const char *id, const char **name)
	{
		return _findIconNameForID(id, name);
	}
	static bool findData(const char *id, const char ***data,
						 UT_uint32 *size)
	{
		return _findIconDataByName(id, data, size);
	}
};

struct ConvertCtx
{
	const char *src;
	GR_Graphics *graphics;
	PD_Document *doc;
};

static void s_convert_print_thunk(void *v)
{
	ConvertCtx *c = static_cast<ConvertCtx *>(v);
	AP_Convert conv(1);
	conv.print(c->src, c->graphics, ".abw");
	if (c->doc)
		conv.printFirstPage(c->graphics, c->doc);
}

/* ap_GetLabel_Suggest dereferences the last-focussed frame
 * unconditionally and needs a word under the context-menu cursor;
 * neither exists headless */
static bool s_expected_menu_label_dead(const char *mname)
{
	return mname && !strncmp(mname, "spellSuggest_", 13);
}

}

TFTEST_MAIN("menu action-set state and label sweep")
{
	XAP_App *app = XAP_App::getApp();
	TFPASS(app != nullptr);
	const EV_Menu_ActionSet *as = app->getMenuActionSet();
	TFPASS(as != nullptr);

	MFView hv;
	TFPASS(hv.load(kMFDoc));
	hv.view->setPoint(6);

	em_guard::install();

	/* every hook runs guarded; anything that dies or hangs is a
	 * headless regression reported by name */
	std::vector<std::string> dead;
	UT_uint32 nActions = 0;
	for (int pass = 0; pass < 2; ++pass)
	{
		if (pass)
			hv.view->cmdSelect(2, 30);
		else
			hv.view->setPoint(6);
		for (int id = static_cast<int>(AP_MENU_ID__BOGUS1__);
			 id < static_cast<int>(AP_MENU_ID__BOGUS2__); ++id)
		{
			const EV_Menu_Action *act =
				as->getAction(static_cast<XAP_Menu_Id>(id));
			if (!act)
				continue;
			if (pass == 0)
				++nActions;
			EV_Menu_Label lbl(static_cast<XAP_Menu_Id>(id),
							  "Item %s", "status");
			MenuProbeCtx ctx{act, hv.view, &lbl};
			const char *mname =
				act->getMethodName() ? act->getMethodName() : "?";
			if (act->hasGetStateFunction() &&
				em_guard::call(s_menu_state_thunk, &ctx, 4000) !=
					em_guard::OK)
			{
				dead.push_back(std::string("state:") + mname);
				fprintf(stderr, "MENUSTATE-DEAD id=%d method=%s\n",
						id, mname);
			}
			if (act->hasDynamicLabel() &&
				em_guard::call(s_menu_label_thunk, &ctx, 4000) !=
					em_guard::OK)
			{
				if (!s_expected_menu_label_dead(mname))
					dead.push_back(std::string("label:") + mname);
				fprintf(stderr, "MENULABEL-DEAD id=%d method=%s\n",
						id, mname);
			}
		}
		TF_Test::pulse();
	}
	TFPASS(nActions > 200);
	TFPASS(dead.empty());
}

TFTEST_MAIN("toolbar action-set state sweep")
{
	XAP_App *app = XAP_App::getApp();
	TFPASS(app != nullptr);
	const EV_Toolbar_ActionSet *as = app->getToolbarActionSet();
	TFPASS(as != nullptr);

	MFView hv;
	TFPASS(hv.load(kMFDoc));
	hv.view->setPoint(6);

	em_guard::install();

	std::vector<std::string> dead;
	UT_uint32 nActions = 0;
	for (int pass = 0; pass < 2; ++pass)
	{
		if (pass)
			hv.view->cmdSelect(2, 30);
		else
			hv.view->setPoint(6);
		for (int id = static_cast<int>(AP_TOOLBAR_ID__BOGUS1__);
			 id < static_cast<int>(AP_TOOLBAR_ID__BOGUS2__); ++id)
		{
			const EV_Toolbar_Action *act =
				as->getAction(static_cast<XAP_Toolbar_Id>(id));
			if (!act)
				continue;
			if (pass == 0)
				++nActions;
			ToolbarProbeCtx ctx{act, hv.view};
			if (em_guard::call(s_toolbar_state_thunk, &ctx, 4000) !=
				em_guard::OK)
			{
				const char *mname =
					act->getMethodName() ? act->getMethodName() : "?";
				/* zoom's state fn needs the frame's zoom control */
				if (mname && strcmp(mname, "zoom"))
					dead.push_back(mname);
				fprintf(stderr, "TBSTATE-DEAD id=%d method=%s\n",
						id, mname);
			}
		}
	}
	TFPASS(nActions > 30);
	TFPASS(dead.empty());
}

TFTEST_MAIN("toolbar icon table lookups")
{
	TestIcons icons;
	const char *name = nullptr;

	/* direct hits in the id->name map */
	TFPASS(TestIcons::findName("EDIT_UNDO", &name));
	TFPASS(name && strstr(name, "undo") != nullptr);
	TFPASS(TestIcons::findName("FILE_OPEN", &name));
	TFPASS(TestIcons::findName("1COLUMN", &name));

	/* language-suffix fallback: FOO_XX strips the last component */
	TFPASS(TestIcons::findName("EDIT_UNDO_xx", &name));

	/* misses */
	TFPASS(!TestIcons::findName("ZZZ_NOT_A_REAL_ICON", &name));
	TFPASS(!TestIcons::findName("EDIT_UNDO_xx_yy", &name));
	TFPASS(!TestIcons::findName("", &name));
	TFPASS(!TestIcons::findName(nullptr, &name));

	/* id->data hits and misses (the data lookup keys on the icon
	 * ID, which is mapped to an icon name internally) */
	const char **data = nullptr;
	UT_uint32 sz = 0;
	TFPASS(TestIcons::findData("EDIT_UNDO", &data, &sz));
	TFPASS(data != nullptr && sz > 0);
	TFPASS(TestIcons::findData("FILE_OPEN", &data, &sz));
	TFPASS(TestIcons::findData("1COLUMN", &data, &sz));
	TFPASS(!TestIcons::findData("ZZZ_NOT_A_REAL_ICON", &data, &sz));
	TFPASS(!TestIcons::findData("", &data, &sz));
	TFPASS(!TestIcons::findData(nullptr, &data, &sz));
	(void)icons;
}

TFTEST_MAIN("headless document conversion")
{
	/* write a minimal .abw into /tmp */
	const char *src = "/tmp/cov08_src.abw";
	FILE *f = fopen(src, "w");
	TFPASS(f != nullptr);
	fputs("<?xml version=\"1.0\"?>\n"
		  "<abiword template=\"false\" file-format=\"1.0\">\n"
		  "<section><p>conversion coverage text</p>"
		  "<p>second para</p></section>\n"
		  "</abiword>\n", f);
	fclose(f);

	AP_Convert conv(2); /* max verbosity: covers the report paths */

	const char *dst = "/tmp/cov08_out.txt";
	remove(dst);
	TFPASS(conv.convertTo(src, ".abw", dst, ".txt"));
	f = fopen(dst, "r");
	TFPASS(f != nullptr);
	if (f)
	{
		char buf[256] = {0};
		size_t n = fread(buf, 1, sizeof(buf) - 1, f);
		fclose(f);
		TFPASS(n > 0);
		TFPASS(strstr(buf, "conversion coverage") != nullptr);
	}

	/* bare-suffix resolution path (no leading dot) */
	remove(dst);
	TFPASS(conv.convertTo(src, ".abw", dst, "txt"));

	/* three-arg variant derives the target name from the suffix */
	remove("/tmp/cov08_src.rtf");
	TFPASS(conv.convertTo(src, ".abw", "rtf"));

	/* three-arg variant with a full target path */
	TFPASS(conv.convertTo(src, ".abw", "/tmp/cov08_q.rtf"));

	/* bad input is a clean failure, not a crash */
	TFPASS(!conv.convertTo("/tmp/does-not-exist.abw", ".abw",
						   dst, ".txt"));
	TFPASS(!conv.convertTo(src, ".abw", dst, ".no-such-format"));

	/* print paths: GR_CairoGraphics::startPrint is a headless
	 * stub that may assert — run guarded so either way the body
	 * of print()/printFirstPage() still runs */
	MFView hv;
	TFPASS(hv.load(kMFDoc));
	em_guard::install();
	ConvertCtx ctx{src, hv.graphics, hv.doc};
	em_guard::Outcome o =
		em_guard::call(s_convert_print_thunk, &ctx, 15000);
	TFPASS(o != em_guard::HUNG);
}

TFTEST_MAIN("preview xp paths")
{
	MFView hv;
	TFPASS(hv.load(kMFDoc));

	/* each non-scroll preview mode constructs a full
	 * FL_DocLayout + FV_View inside AP_Preview_Abi */
	const PreViewMode modes[] = {
		PREVIEW_ZOOMED, PREVIEW_CLIPPED, PREVIEW_ADJUSTED_PAGE
	};
	for (PreViewMode mode : modes)
	{
		/* the preview unrefs the doc on destruction, so take a
		 * ref for its ownership */
		hv.doc->ref();
		AP_Preview_Abi *prev =
			new AP_Preview_Abi(hv.graphics, 400, 300, nullptr,
							   mode, hv.doc);
		TFPASS(prev != nullptr);
		if (!prev)
			continue;
		TFPASS(prev->getView() != nullptr);
		TFPASS(prev->getDoc() == hv.doc);
		prev->drawImmediate(nullptr);
		UT_Rect clip(0, 0, 200, 150);
		prev->drawImmediate(&clip);
		delete prev;
		TF_Test::pulse();
	}

	/* no-doc variant builds an empty document internally */
	AP_Preview_Abi *prev =
		new AP_Preview_Abi(hv.graphics, 300, 200, nullptr,
						   PREVIEW_ZOOMED, nullptr);
	TFPASS(prev != nullptr);
	if (prev)
	{
		TFPASS(prev->getView() != nullptr);
		prev->drawImmediate(nullptr);
		delete prev;
	}
}
