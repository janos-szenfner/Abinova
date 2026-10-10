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

#include <cstdlib>
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
};

LineGeom ar_measure(fp_Line * l)
{
	LineGeom g;
	g.w = l->calculateWidthOfLine();
	g.trail = l->calculateWidthOfTrailingSpaces();
	bool seen = false;
	for (UT_sint32 i = 0; i < l->countRuns(); ++i)
	{
		fp_Run * r = l->getRunAtVisPos(i);
		if (!r || r->isHidden())
			continue;
		if (!seen)
		{
			g.x0 = r->getX();
			seen = true;
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
