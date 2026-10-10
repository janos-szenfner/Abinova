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
 * Headless coverage of paragraph alignment transitions (ALIGN01):
 * a line that was laid out justified keeps its inflated run widths
 * when the block's alignment changes to right/center/left, so the
 * line lays out wider than the column and right-aligned text is
 * pushed left of the text column into the margin/pasteboard.
 * These tests walk the real fp_Line/fp_Run geometry on a widget-less
 * GR_UnixCairoGraphics so no display is needed.
 */

#include "tf_test.h"

#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "fl_DocLayout.h"
#include "fl_BlockLayout.h"
#include "fb_Alignment.h"
#include "fp_Line.h"
#include "fp_Run.h"
#include "fv_View.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "xap_EditMethods.h"
#include "ev_EditMethod.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define TFSUITE "core.text.fmt.alignreflow"

namespace {

struct AlignView
{
	AlignView() = default;
	AlignView(const AlignView &) = delete;
	AlignView &operator=(const AlignView &) = delete;

	bool loadFile(const char *absPath)
	{
		doc = new PD_Document;
		if (doc->readFromFile(absPath, IEFT_Unknown, nullptr) != UT_OK)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		return finish();
	}

	/* scratch document: one block per '\n' in `text` — same pattern
	 * as fv_EditOps.t.cpp */
	bool load(const std::string &text)
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
		const char *p = text.c_str();
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

	~AlignView()
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

std::vector<fp_Line *> ar_blockLines(fl_BlockLayout * bl)
{
	std::vector<fp_Line *> lines;
	for (fp_Container * c = bl->getFirstContainer(); c;
		 c = static_cast<fp_Container *>(c->getNext()))
	{
		fp_Line * l = static_cast<fp_Line *>(c);
		lines.push_back(l);
		if (l->isLastLineInBlock())
			break;
	}
	return lines;
}

struct LineGeom
{
	UT_sint32 x0 = 0;	/* x of the first visible run */
	UT_sint32 x1 = 0;	/* right edge of the last visible run */
	UT_sint32 w = 0;	/* sum of visible run widths */
	UT_sint32 trail = 0;/* width of trailing spaces */
	bool seen = false;
};

LineGeom ar_measure(fp_Line * l)
{
	LineGeom g;
	g.w = l->calculateWidthOfLine();
	g.trail = l->calculateWidthOfTrailingSpaces();
	for (UT_sint32 i = 0; i < l->countRuns(); ++i)
	{
		fp_Run * r = l->getRunAtVisPos(i);
		if (!r || r->isHidden())
			continue;
		if (!g.seen)
		{
			g.x0 = r->getX();
			g.seen = true;
		}
		g.x1 = r->getX() + r->getWidth();
	}
	return g;
}

/* justification distributes in layout units at the line level and in
 * pango units at the glyph level, so a reset line can differ from its
 * pre-justify width by a couple of layout units per space; anything
 * that tight still catches a line that stays inflated to the column */
const UT_sint32 AR_TOL = 60;

/* invoke a registered alignment verb the same way the ribbon button
 * and the Ctrl+L/E/R/J binding do — through the app's edit-method
 * container, not by poking the block property directly */
bool callAlignMethod(FV_View * v, const char * name)
{
	static EV_EditMethodContainer * s_emc = AP_GetEditMethods();
	EV_EditMethod * em =
		s_emc ? s_emc->findEditMethodByName(name) : nullptr;
	if (!em)
		return false;
	EV_EditMethodCallData cd;
	return em->Fn(v, &cd);
}

const char * const AR_METHODS[4] = {
	"alignLeft", "alignCenter", "alignRight", "alignJustify"
};
const char * const AR_PROPS[4] = {
	"left", "center", "right", "justify"
};

/* the column invariant, in a line's own coordinate space: no visible
 * run may start left of the text-area edge into the margin/pasteboard
 * and none may end right of the right text edge — except trailing
 * whitespace, which legitimately hangs off the visual end of the
 * line (the right edge for LTR, the left edge for RTL) */
void ar_assertInColumn(fp_Line * l, UT_BidiCharType dom)
{
	LineGeom g = ar_measure(l);
	if (!g.seen)
		return;
	const UT_sint32 lo = l->getLeftThick() -
		(dom == UT_BIDI_RTL ? g.trail : 0) - AR_TOL;
	const UT_sint32 hi = l->getMaxWidth() - l->getRightThick() +
		(dom == UT_BIDI_RTL ? 0 : g.trail) + AR_TOL;
	if (g.x0 < lo || g.x1 > hi)
	{
		fprintf(stderr, "BOUNDS x0=%d x1=%d lo=%d hi=%d lt=%d rt=%d "
				"maxW=%d trail=%d dom=%d nRuns=%d\n",
				g.x0, g.x1, lo, hi, l->getLeftThick(), l->getRightThick(),
				l->getMaxWidth(), g.trail, dom, l->countRuns());
		for (UT_sint32 i = 0; i < l->countRuns(); ++i)
		{
			fp_Run * r = l->getRunAtVisPos(i);
			if (!r)
				continue;
			fprintf(stderr, "  run[%d] type=%d x=%d w=%d hid=%d\n",
					i, r->getType(), r->getX(), r->getWidth(),
					r->isHidden() ? 1 : 0);
		}
	}
	TFPASS(g.x0 >= lo);
	TFPASS(g.x1 <= hi);
}

void ar_assertAllLines(fl_BlockLayout * bl)
{
	bool bad = false;
	std::vector<fp_Line *> lines = ar_blockLines(bl);
	for (fp_Line * l : lines)
	{
		LineGeom g = ar_measure(l);
		if (!g.seen)
			continue;
		const UT_sint32 lo = l->getLeftThick() -
			(bl->getDominantDirection() == UT_BIDI_RTL ? g.trail : 0) - AR_TOL;
		const UT_sint32 hi = l->getMaxWidth() - l->getRightThick() +
			(bl->getDominantDirection() == UT_BIDI_RTL ? 0 : g.trail) + AR_TOL;
		if (g.x0 < lo || g.x1 > hi)
			bad = true;
	}
	if (bad)
	{
		int li = 0;
		for (fp_Line * l : lines)
		{
			fprintf(stderr, "LINE %d maxW=%d x=%d\n", li++,
					l->getMaxWidth(), l->getX());
			for (UT_sint32 i = 0; i < l->countRuns(); ++i)
			{
				fp_Run * r = l->getRunAtVisPos(i);
				if (!r)
					continue;
				fprintf(stderr, "  run[%d] type=%d x=%d w=%d hid=%d "
						"off=%u len=%u\n", i, r->getType(), r->getX(),
						r->getWidth(), r->isHidden() ? 1 : 0,
						r->getBlockOffset(), r->getLength());
			}
		}
	}
	for (fp_Line * l : lines)
		ar_assertInColumn(l, bl->getDominantDirection());
}

}

TFTEST_MAIN("justify -> right/center/left clears stale justification")
{
	AlignView hv;
	std::string text;
	for (int i = 0; i < 80; ++i)
		text += "the quick brown fox jumps over lazy dog " +
			std::to_string(i) + " ";
	TFPASS(hv.load(text));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(2);
	fl_BlockLayout * bl = v->getBlockAtPosition(2);
	TFPASS(bl != nullptr);
	if (!bl)
		return;

	std::vector<fp_Line *> lines = ar_blockLines(bl);
	TFPASS(lines.size() >= 3);
	if (lines.size() < 3)
		return;

	/* baseline geometry in left alignment */
	std::vector<LineGeom> nat;
	for (fp_Line * l : lines)
		nat.push_back(ar_measure(l));
	const UT_sint32 avail = lines[0]->getAvailableWidth();
	TFPASS(avail > 0);

	/* justify the paragraph: every non-last line must inflate to
	 * fill the column — this is the state that used to survive the
	 * next alignment change */
	TFPASS(v->setBlockFormat({"text-align", "justify"}));
	hv.layout->formatAll();
	lines = ar_blockLines(bl);
	TFPASS(lines.size() == nat.size());
	bool sawJustified = false;
	for (size_t i = 0; i + 1 < lines.size(); ++i)
	{
		LineGeom g = ar_measure(lines[i]);
		if (g.w > nat[i].w)
			sawJustified = true;
	}
	TFPASS(sawJustified);

	/* right: the inflated widths must be gone — the line width must
	 * return to the natural width and the laid-out text must end at
	 * the right edge of the text area */
	TFPASS(v->setBlockFormat({"text-align", "right"}));
	hv.layout->formatAll();
	lines = ar_blockLines(bl);
	for (size_t i = 0; i < lines.size(); ++i)
	{
		LineGeom g = ar_measure(lines[i]);
		const UT_sint32 rightEdge =
			lines[i]->getMaxWidth() - lines[i]->getRightThick();
		TFPASS(std::abs(g.w - nat[i].w) <= AR_TOL);
		TFPASS(std::abs(g.x0 - (rightEdge - (g.w - g.trail))) <= AR_TOL);
		TFPASS(g.x0 >= lines[i]->getLeftThick() - AR_TOL);
		TFPASS(g.x1 <= rightEdge + g.trail + AR_TOL);
	}

	/* center: same stale-width check plus symmetric placement */
	TFPASS(v->setBlockFormat({"text-align", "center"}));
	hv.layout->formatAll();
	lines = ar_blockLines(bl);
	for (size_t i = 0; i < lines.size(); ++i)
	{
		LineGeom g = ar_measure(lines[i]);
		const UT_sint32 extra = avail - g.w;
		const UT_sint32 expect =
			lines[i]->getLeftThick() + (extra > 0 ? extra / 2 : 0);
		TFPASS(std::abs(g.w - nat[i].w) <= AR_TOL);
		TFPASS(std::abs(g.x0 - expect) <= AR_TOL);
	}

	/* back to left and once more through justify to make sure the
	 * reset is repeatable, not a one-shot */
	TFPASS(v->setBlockFormat({"text-align", "left"}));
	hv.layout->formatAll();
	lines = ar_blockLines(bl);
	for (size_t i = 0; i < lines.size(); ++i)
	{
		LineGeom g = ar_measure(lines[i]);
		TFPASS(std::abs(g.w - nat[i].w) <= AR_TOL);
		TFPASS(g.x0 == lines[i]->getLeftThick());
	}

	TFPASS(v->setBlockFormat({"text-align", "justify"}));
	hv.layout->formatAll();
	TFPASS(v->setBlockFormat({"text-align", "right"}));
	hv.layout->formatAll();
	lines = ar_blockLines(bl);
	for (size_t i = 0; i < lines.size(); ++i)
	{
		LineGeom g = ar_measure(lines[i]);
		TFPASS(std::abs(g.w - nat[i].w) <= AR_TOL);
		TFPASS(g.x0 >= lines[i]->getLeftThick() - AR_TOL);
	}
}

TFTEST_MAIN("right-aligned text ends at the right edge in a bordered block")
{
	AlignView hv;
	std::string text;
	for (int i = 0; i < 40; ++i)
		text += "a bordered paragraph of words to wrap " +
			std::to_string(i) + " ";
	TFPASS(hv.load(text));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(2);
	fl_BlockLayout * bl = v->getBlockAtPosition(2);
	TFPASS(bl != nullptr);
	if (!bl)
		return;

	/* thick borders so a missing left/right-thickness term shifts
	 * the layout well beyond the tolerance */
	TFPASS(v->setBlockFormat({
		"left-style", "solid",
		"left-color", "000000",
		"left-thickness", "20pt",
		"left-space", "10pt",
		"right-style", "solid",
		"right-color", "000000",
		"right-thickness", "20pt",
		"right-space", "10pt",
	}));
	hv.layout->formatAll();

	std::vector<fp_Line *> lines = ar_blockLines(bl);
	TFPASS(lines.size() >= 2);
	if (lines.size() < 2)
		return;
	TFPASS(lines[0]->getLeftThick() > AR_TOL);
	TFPASS(lines[0]->getRightThick() > AR_TOL);

	TFPASS(v->setBlockFormat({"text-align", "right"}));
	hv.layout->formatAll();
	lines = ar_blockLines(bl);
	for (fp_Line * l : lines)
	{
		LineGeom g = ar_measure(l);
		const UT_sint32 rightEdge =
			l->getMaxWidth() - l->getRightThick();
		/* the text must end at the right edge of the text area,
		 * not short of it by the left border thickness */
		TFPASS(g.x1 >= rightEdge - AR_TOL);
		TFPASS(g.x1 <= rightEdge + g.trail + AR_TOL);
		TFPASS(g.x0 >= l->getLeftThick() - AR_TOL);
	}
}

/* a line wider than the text area must not be pushed left of the
 * text boundary — Word keeps the overflow inside the column.  The
 * over-wide state the ALIGN01 screenshot showed (stale justification,
 * an unsplittable run) is forced directly so the initialize() clamp
 * is what gets exercised */
TFTEST_MAIN("right-align clamps an over-wide line at the left boundary")
{
	AlignView hv;
	std::string text;
	for (int i = 0; i < 80; ++i)
		text += "the quick brown fox jumps over lazy dog " +
			std::to_string(i) + " ";
	TFPASS(hv.load(text));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(2);
	fl_BlockLayout * bl = v->getBlockAtPosition(2);
	TFPASS(bl != nullptr);
	if (!bl)
		return;

	TFPASS(v->setBlockFormat({"text-align", "right"}));
	hv.layout->formatAll();

	std::vector<fp_Line *> lines = ar_blockLines(bl);
	TFPASS(lines.size() >= 2);
	if (lines.size() < 2)
		return;

	fb_Alignment * align = bl->getAlignment();
	TFPASS(align != nullptr);
	if (!align)
		return;
	TFPASS(align->getType() == FB_ALIGNMENT_RIGHT);

	bool sawClamped = false;
	for (fp_Line * l : lines)
	{
		const UT_sint32 avail = l->getAvailableWidth();
		const UT_sint32 iWidth =
			l->calculateWidthOfLine() - l->calculateWidthOfTrailingSpaces();
		if (l->countJustificationPoints() == 0)
			continue;

		/* inflate past the column — the state the ALIGN01 bug left a
		 * line in; the start position must clamp at the left text
		 * boundary, not go negative.  fp_Line::justify can stop
		 * distributing early when the point count runs out, so aim
		 * well past the edge */
		l->justify(avail - iWidth + avail);
		align->initialize(l);
		const UT_sint32 start = align->getStartPosition();
		TFPASS(start >= l->getLeftThick());
		if (start == l->getLeftThick())
			sawClamped = true;
		l->resetJustification(false);
	}
	TFPASS(sawClamped);

	/* and every right-aligned run must still start at or right of
	 * the left text boundary on the relayout */
	hv.layout->formatAll();
	for (fp_Line * l : ar_blockLines(bl))
	{
		LineGeom g = ar_measure(l);
		TFPASS(g.x0 >= l->getLeftThick() - AR_TOL);
	}
}

/* force the stale-justify bug shape directly: inflate a line's runs
 * the way a justify pass does, then re-lay it out under a non-justify
 * alignment — the layout must restore natural widths */
TFTEST_MAIN("layout clears stale justification on non-justify lines")
{
	AlignView hv;
	std::string text;
	for (int i = 0; i < 80; ++i)
		text += "the quick brown fox jumps over lazy dog " +
			std::to_string(i) + " ";
	TFPASS(hv.load(text));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(2);
	fl_BlockLayout * bl = v->getBlockAtPosition(2);
	TFPASS(bl != nullptr);
	if (!bl)
		return;

	TFPASS(v->setBlockFormat({"text-align", "right"}));
	hv.layout->formatAll();

	std::vector<fp_Line *> lines = ar_blockLines(bl);
	TFPASS(lines.size() >= 2);
	if (lines.size() < 2)
		return;

	/* natural right-aligned geometry */
	std::vector<LineGeom> nat;
	for (fp_Line * l : lines)
		nat.push_back(ar_measure(l));

	bool sawInflated = false;
	for (size_t i = 0; i < lines.size(); ++i)
	{
		fp_Line * l = lines[i];
		const UT_sint32 avail = l->getAvailableWidth();
		const UT_sint32 iWidth =
			l->calculateWidthOfLine() - l->calculateWidthOfTrailingSpaces();
		const UT_sint32 extra = avail - iWidth;

		/* mimic a justify pass having left the runs stretched —
		 * the state a stale line carries into a right-align; a
		 * line with no justification points cannot inflate */
		if (extra > 0 && l->countJustificationPoints() > 0)
		{
			l->justify(extra);
			sawInflated = sawInflated ||
				l->calculateWidthOfLine() > nat[i].w;
		}

		l->layout();

		LineGeom g = ar_measure(l);
		/* without the reset in fp_Line::layout the inflated width
		 * stays and the line is pushed left of the column by
		 * exactly that inflation */
		TFPASS(std::abs(g.w - nat[i].w) <= AR_TOL);
		TFPASS(g.x0 >= l->getLeftThick() - AR_TOL);
		TFPASS(g.x1 <= l->getMaxWidth() - l->getRightThick() +
			   g.trail + AR_TOL);
	}
	TFPASS(sawInflated);
}

/* the last line of a justified block must not keep stretched
 * widths — it renders flush-left like Word's last line */
TFTEST_MAIN("justify initialize resets a stale last line")
{
	AlignView hv;
	std::string text;
	for (int i = 0; i < 80; ++i)
		text += "the quick brown fox jumps over lazy dog " +
			std::to_string(i) + " ";
	TFPASS(hv.load(text));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(2);
	fl_BlockLayout * bl = v->getBlockAtPosition(2);
	TFPASS(bl != nullptr);
	if (!bl)
		return;

	TFPASS(v->setBlockFormat({"text-align", "justify"}));
	hv.layout->formatAll();

	std::vector<fp_Line *> lines = ar_blockLines(bl);
	TFPASS(lines.size() >= 2);
	if (lines.size() < 2)
		return;

	fp_Line * last = lines.back();
	TFPASS(last->isLastLineInBlock());
	const UT_sint32 natW = last->calculateWidthOfLine();
	const UT_sint32 avail = last->getAvailableWidth();
	const UT_sint32 extra = avail -
		(natW - last->calculateWidthOfTrailingSpaces());
	TFPASS(extra > AR_TOL);

	/* inflate the last line the way a justify pass left it when it
	 * was not yet the last line — same formula as
	 * fb_Alignment_justify::initialize; distribution can stop early
	 * when the point count runs out, so assert on inflation itself */
	last->justify(extra);
	TFPASS(last->calculateWidthOfLine() > natW);

	last->layout();

	LineGeom g = ar_measure(last);
	TFPASS(std::abs(g.w - natW) <= AR_TOL);
	TFPASS(g.x0 >= last->getLeftThick() - AR_TOL);
}

/* ALIGN02: the four alignment verbs driven through the real
 * edit methods (the path the ribbon buttons and Ctrl+L/E/R/J
 * bindings take), on a plain and a bordered wrapped paragraph,
 * through every ordered pair of alignments — the transition
 * matrix.  After each transition every line's runs must sit
 * inside the text column and the block property must show the
 * verb actually landed. */
TFTEST_MAIN("alignment edit methods keep every line's runs inside the column")
{
	AlignView hv;
	std::string text;
	for (int i = 0; i < 80; ++i)
		text += "the quick brown fox jumps over lazy dog " +
			std::to_string(i) + " ";
	text += "\n";
	for (int i = 0; i < 60; ++i)
		text += "a bordered second paragraph wrapping " +
			std::to_string(i) + " ";
	TFPASS(hv.load(text));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(2);
	fl_BlockLayout * bl1 = v->getBlockAtPosition(2);
	TFPASS(bl1 != nullptr);
	if (!bl1)
		return;
	fl_BlockLayout * bl2 = bl1->getNextBlockInDocument();
	TFPASS(bl2 != nullptr);
	if (!bl2)
		return;

	v->setPoint(bl2->getPosition() + 2);
	TFPASS(v->setBlockFormat({
		"left-style", "solid",
		"left-color", "000000",
		"left-thickness", "20pt",
		"right-style", "solid",
		"right-color", "000000",
		"right-thickness", "20pt",
	}));
	hv.layout->formatAll();
	{
		std::vector<fp_Line *> bl2lines = ar_blockLines(bl2);
		TFPASS(!bl2lines.empty());
		if (!bl2lines.empty())
			TFPASS(bl2lines[0]->getLeftThick() > AR_TOL);
	}

	for (int from = 0; from < 4; ++from)
	{
		for (int to = 0; to < 4; ++to)
		{
			if (to == from)
				continue;
			for (fl_BlockLayout * bl : {bl1, bl2})
			{
				v->setPoint(bl->getPosition() + 2);
				TFPASS(callAlignMethod(v, AR_METHODS[from]));
				TFPASS(callAlignMethod(v, AR_METHODS[to]));
			}
			hv.layout->formatAll();
			for (fl_BlockLayout * bl : {bl1, bl2})
			{
				const char * al = bl->getProperty("text-align");
				TFPASS(al != nullptr);
				TFPASS(al && !strcmp(al, AR_PROPS[to]));
				ar_assertAllLines(bl);
			}
			/* right-align must reach the right text edge — a
			 * missing leftThick anchor lands the text a border's
			 * thickness short of it instead */
			if (to == 2)
			{
				for (fl_BlockLayout * bl : {bl1, bl2})
				{
					for (fp_Line * l : ar_blockLines(bl))
					{
						LineGeom g = ar_measure(l);
						if (!g.seen)
							continue;
						const UT_sint32 re = l->getMaxWidth() -
							l->getRightThick();
						TFPASS(g.x1 >= re - AR_TOL);
					}
				}
			}
			/* center must sit symmetric in the text area when the
			 * line does not fill it */
			if (to == 1)
			{
				for (fl_BlockLayout * bl : {bl1, bl2})
				{
					for (fp_Line * l : ar_blockLines(bl))
					{
						LineGeom g = ar_measure(l);
						if (!g.seen)
							continue;
						const UT_sint32 re = l->getMaxWidth() -
							l->getRightThick();
						const UT_sint32 extra =
							l->getAvailableWidth() - g.w;
						if (extra <= AR_TOL)
							continue;
						TFPASS(std::abs((g.x0 - l->getLeftThick()) -
								(re - g.x1)) <= 2 * AR_TOL);
					}
				}
			}
		}
	}
}

/* paragraph indents reshape the line's own geometry — first-line
 * indent narrows the first line, hanging indent widens it — but
 * the column invariant must hold for every line under every
 * alignment, and every line keeps the same column right edge */
TFTEST_MAIN("first-line and hanging indents keep aligned text in the column")
{
	AlignView hv;
	std::string a, b;
	for (int i = 0; i < 60; ++i)
		a += "first line indented paragraph words " +
			std::to_string(i) + " ";
	for (int i = 0; i < 60; ++i)
		b += "hanging indent paragraph of running text " +
			std::to_string(i) + " ";
	TFPASS(hv.load(a + "\n" + b));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(2);
	fl_BlockLayout * blA = v->getBlockAtPosition(2);
	TFPASS(blA != nullptr);
	if (!blA)
		return;
	fl_BlockLayout * blB = blA->getNextBlockInDocument();
	TFPASS(blB != nullptr);
	if (!blB)
		return;

	v->setPoint(blA->getPosition() + 2);
	TFPASS(v->setBlockFormat({"text-indent", "0.75in"}));
	v->setPoint(blB->getPosition() + 2);
	TFPASS(v->setBlockFormat({
		"margin-left", "0.9in",
		"text-indent", "-0.4in",
	}));
	hv.layout->formatAll();

	/* the indents must actually shape the lines, otherwise the
	 * geometry asserts below are measuring the wrong thing */
	std::vector<fp_Line *> la = ar_blockLines(blA);
	std::vector<fp_Line *> lb = ar_blockLines(blB);
	TFPASS(la.size() >= 2 && lb.size() >= 2);
	if (la.size() < 2 || lb.size() < 2)
		return;
	const UT_sint32 colRightA =
		la[0]->getX() + la[0]->getMaxWidth();
	const UT_sint32 colRightB =
		lb[0]->getX() + lb[0]->getMaxWidth();
	for (fp_Line * l : la)
		TFPASS(l->getX() + l->getMaxWidth() == colRightA);
	for (fp_Line * l : lb)
		TFPASS(l->getX() + l->getMaxWidth() == colRightB);
	TFPASS(la[0]->getX() > la[1]->getX());  /* first-line indent */
	TFPASS(lb[0]->getX() < lb[1]->getX());  /* hanging indent */

	for (const char * m : AR_METHODS)
	{
		for (fl_BlockLayout * bl : {blA, blB})
		{
			v->setPoint(bl->getPosition() + 2);
			TFPASS(callAlignMethod(v, m));
		}
		hv.layout->formatAll();
		ar_assertAllLines(blA);
		ar_assertAllLines(blB);
	}
}

/* tab runs participate in alignment: under every alignment the
 * laid-out runs must stay in the column, and under left-align the
 * run after a tab must land on the stop the line itself reports */
TFTEST_MAIN("tab stops keep aligned text inside the column")
{
	AlignView hv;
	std::string text = "aa\tbb\tcc ";
	for (int i = 0; i < 40; ++i)
		text += "tail words to wrap the line " + std::to_string(i) + " ";
	TFPASS(hv.load(text));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(2);
	fl_BlockLayout * bl = v->getBlockAtPosition(2);
	TFPASS(bl != nullptr);
	if (!bl)
		return;
	TFPASS(v->setBlockFormat({
		"tabstops", "2in/L,4.5in/R",
		"text-align", "left",
	}));
	hv.layout->formatAll();

	std::vector<fp_Line *> lines = ar_blockLines(bl);
	TFPASS(lines.size() >= 2);
	if (lines.empty())
		return;

	/* first line: find the first tab run and assert the run after
	 * it lands on the stop the line's own tab table reports */
	fp_Line * l0 = lines[0];
	fp_Run * tab = nullptr;
	fp_Run * after = nullptr;
	for (UT_sint32 i = 0; i < l0->countRuns(); ++i)
	{
		fp_Run * r = l0->getRunAtVisPos(i);
		if (!r || r->isHidden())
			continue;
		if (!tab && r->getType() == FPRUN_TAB)
		{
			tab = r;
			continue;
		}
		if (tab && !after)
			after = r;
	}
	TFPASS(tab != nullptr && after != nullptr);
	if (tab && after)
	{
		UT_sint32 iPos = 0;
		eTabType type = FL_TAB_LEFT;
		eTabLeader leader = FL_LEADER_NONE;
		TFPASS(l0->findNextTabStop(tab->getX(), iPos, type, leader));
		TFPASS(type == FL_TAB_LEFT);
		TFPASS(std::abs(after->getX() - iPos) <= AR_TOL);
	}

	for (const char * m : AR_METHODS)
	{
		v->setPoint(bl->getPosition() + 2);
		TFPASS(callAlignMethod(v, m));
		hv.layout->formatAll();
		ar_assertAllLines(bl);
	}
}

/* the line breaker force-splits a word wider than the column at an
 * arbitrary character (fb_LineBreaker): the pieces fill the text
 * area exactly, so under every alignment — bordered, where the
 * anchors' leftThick term matters — every split piece must stay
 * inside the column and never shift left into the margin */
TFTEST_MAIN("an over-wide unbreakable word stays inside the column")
{
	AlignView hv;
	std::string text(600, 'w');
	TFPASS(hv.load(text));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(2);
	fl_BlockLayout * bl = v->getBlockAtPosition(2);
	TFPASS(bl != nullptr);
	if (!bl)
		return;
	TFPASS(v->setBlockFormat({
		"left-style", "solid",
		"left-color", "000000",
		"left-thickness", "20pt",
		"right-style", "solid",
		"right-color", "000000",
		"right-thickness", "20pt",
	}));
	hv.layout->formatAll();
	{
		std::vector<fp_Line *> lines = ar_blockLines(bl);
		TFPASS(lines.size() >= 2);
	}

	for (const char * m : AR_METHODS)
	{
		v->setPoint(bl->getPosition() + 2);
		TFPASS(callAlignMethod(v, m));
		hv.layout->formatAll();
		ar_assertAllLines(bl);
	}
}

/* RTL flips the semantics: right-align and justified lines anchor
 * at the RIGHT edge under WORK_BACKWARD, trailing spaces hang off
 * the LEFT edge.  Asymmetric borders make the pre-ALIGN01 anchors
 * visible — they measured from the wrong edge */
TFTEST_MAIN("an RTL block keeps aligned text inside the column")
{
	AlignView hv;
	std::string text;
	/* Hebrew, long enough to wrap several lines */
	for (int i = 0; i < 25; ++i)
		text += "שלום עולם זוהי פסקה ארוכה בעברית לבדיקת יישור ";
	TFPASS(hv.load(text));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(2);
	fl_BlockLayout * bl = v->getBlockAtPosition(2);
	TFPASS(bl != nullptr);
	if (!bl)
		return;
	TFPASS(v->setBlockFormat({
		"dom-dir", "rtl",
		"left-style", "solid",
		"left-color", "000000",
		"left-thickness", "30pt",
		"right-style", "solid",
		"right-color", "000000",
		"right-thickness", "8pt",
	}));
	hv.layout->formatAll();

	TFPASS(bl->getDominantDirection() == UT_BIDI_RTL);
	std::vector<fp_Line *> lines = ar_blockLines(bl);
	TFPASS(lines.size() >= 2);
	if (lines.empty())
		return;
	TFPASS(lines[0]->getLeftThick() > lines[0]->getRightThick());

	for (int a = 0; a < 4; ++a)
	{
		v->setPoint(bl->getPosition() + 2);
		TFPASS(callAlignMethod(v, AR_METHODS[a]));
		hv.layout->formatAll();
		for (fp_Line * l : ar_blockLines(bl))
		{
			ar_assertInColumn(l, UT_BIDI_RTL);
			LineGeom g = ar_measure(l);
			if (!g.seen)
				continue;
			const UT_sint32 re =
				l->getMaxWidth() - l->getRightThick();
			/* right-align and non-last justified lines anchor at
			 * the right text edge; a start position measured from
			 * availableWidth alone lands a leftThick short of it */
			if (a == 2 || (a == 3 && !l->isLastLineInBlock()))
				TFPASS(g.x1 >= re - AR_TOL);
			/* left-align anchors at the left edge minus the
			 * trailing spaces that hang off it */
			if (a == 0)
				TFPASS(std::abs(g.x0 -
						(l->getLeftThick() - g.trail)) <= AR_TOL);
		}
	}
}

/* a list item's first line carries the label field, the label tab
 * and the hanging indent — alignment must keep all of it inside
 * the column just like a plain paragraph */
TFTEST_MAIN("list labels stay inside the column under alignment")
{
	AlignView hv;
	std::string rich;
	TFPASS(TF_Test::ensure_test_data("/test/wp/cov07/rich.abw", rich));
	TFPASS(hv.loadFile(rich.c_str()));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	std::vector<fl_BlockLayout *> listBlocks;
	for (fl_BlockLayout * b = hv.layout->findBlockAtPosition(2); b;
		 b = b->getNextBlockInDocument())
	{
		if (b->getAutoNum())
			listBlocks.push_back(b);
	}
	/* the fixture carries numbered, nested-numbered and bulleted
	 * items — the asserts below are meaningless without them */
	TFPASS(listBlocks.size() >= 3);
	if (listBlocks.empty())
		return;

	for (int a = 0; a < 4; ++a)
	{
		for (fl_BlockLayout * bl : listBlocks)
		{
			v->setPoint(bl->getPosition() + 1);
			TFPASS(callAlignMethod(v, AR_METHODS[a]));
		}
		hv.layout->formatAll();
		for (fl_BlockLayout * bl : listBlocks)
		{
			ar_assertAllLines(bl);
			for (fp_Line * l : ar_blockLines(bl))
			{
				/* the list-label field run must be present and
				 * inside the column too — that's the edge case:
				 * the label hangs in the outdent of the first line */
				for (UT_sint32 i = 0; i < l->countRuns(); ++i)
				{
					fp_Run * r = l->getRunAtVisPos(i);
					if (!r || r->isHidden() ||
						r->getType() != FPRUN_FIELD)
						continue;
					const UT_sint32 re = l->getMaxWidth() -
						l->getRightThick() + l->calculateWidthOfTrailingSpaces();
					TFPASS(r->getX() >=
						   l->getLeftThick() - l->calculateWidthOfTrailingSpaces() - AR_TOL);
					TFPASS(r->getX() + r->getWidth() <= re + AR_TOL);
				}
			}
		}
	}
}
