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

#include "config.h"
#include "tf_test.h"

#include "pd_Document.h"
#include "fl_DocLayout.h"
#include "fl_BlockLayout.h"
#include "fl_AutoNum.h"
#include "fl_SectionLayout.h"
#include "fl_TableLayout.h"
#include "fp_Page.h"
#include "fp_Run.h"
#include "fv_View.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "pp_AttrProp.h"
#include "ut_growbuf.h"
#include "ut_string.h"
#include "ut_types.h"

#include <glib.h>

#include <cstring>
#include <vector>

#define TFSUITE "core.text.fmt.viewops"

namespace {

/* Same widget-less stack as fv_MouseContext/fv_ImageProps: painting
 * goes to a private cairo image surface so the full layout machinery
 * works headless. */
struct HeadlessOpsView
{
	HeadlessOpsView() = default;
	HeadlessOpsView(const HeadlessOpsView &) = delete;
	HeadlessOpsView &operator=(const HeadlessOpsView &) = delete;

	bool open(const char *relPath)
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
		return true;
	}

	bool format()
	{
		layout->fillLayouts();
		layout->formatAll();
		return layout->countPages() > 0;
	}

	/* first block whose paragraph is part of a list */
	fl_BlockLayout * findListBlock() const
	{
		for (fl_BlockLayout *b = layout->findBlockAtPosition(2); b;
			 b = b->getNextBlockInDocument())
		{
			if (b->getAutoNum())
				return b;
		}
		return nullptr;
	}

	/* first block nested inside a table cell */
	fl_BlockLayout * findCellBlock() const
	{
		for (fl_BlockLayout *b = layout->findBlockAtPosition(2); b;
			 b = b->getNextBlockInDocument())
		{
			fl_ContainerLayout *cl = b->myContainingLayout();
			if (cl && cl->getContainerType() == FL_CONTAINER_CELL)
				return b;
		}
		return nullptr;
	}

	fp_Run * findRunOfType(FP_RUN_TYPE type) const
	{
		for (fl_BlockLayout *b = layout->findBlockAtPosition(2); b;
			 b = b->getNextBlockInDocument())
		{
			for (fp_Run *r = b->getFirstRun(); r; r = r->getNextRun())
			{
				if (r->getType() == type)
					return r;
			}
		}
		return nullptr;
	}

	~HeadlessOpsView()
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

} // namespace

TFTEST_MAIN("find and replace walks the document in both directions")
{
	HeadlessOpsView hv;
	TFPASS(hv.open("/test/wp/BillOfRights.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());
	FV_View *v = hv.view;

	const UT_UCS4Char find[] = {'t','h','e',0};
	const UT_UCS4Char repl[] = {'X','Y','Z',0};

	bool bDone = false;
	v->findSetStartAt(2);
	TFPASS(v->findNext(find, bDone));  /* string overload covers findSetFindString */
	TFPASS(!v->isSelectionEmpty());

	UT_UCS4Char *cur = v->findGetFindString();
	TFPASS(cur != nullptr && UT_UCS4_strlen(cur) == 3);

	v->findSetMatchCase(false);
	v->findSetWholeWord(false);
	TFPASS(!v->findGetReverseFind());

	/* match stays selected; replace it, then keep looking */
	v->findSetReplaceString(repl);
	cur = v->findGetReplaceString();
	TFPASS(cur != nullptr && UT_UCS4_strcmp(cur, repl) == 0);
	bDone = false;
	v->findReplace(bDone);

	/* now search backwards from the insertion point */
	v->findSetStartAtInsPoint();
	bool bDoneBack = false;
	v->findPrev(bDoneBack);
	v->findAgain();

	/* reverse replace exercises the other _findReplace leg */
	bDoneBack = false;
	v->findReplaceReverse(bDoneBack);

	/* replace-all on a fresh start point */
	v->findSetStartAt(2);
	v->findSetFindString(find);
	UT_uint32 n = v->findReplaceAll();
	TFPASS(n > 0);
}

TFTEST_MAIN("position and text queries follow the caret")
{
	HeadlessOpsView hv;
	TFPASS(hv.open("/test/wp/BillOfRights.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());
	FV_View *v = hv.view;

	TFPASS(v->isInDocSection(2));

	v->setPoint(5);
	UT_GrowBuf buf;
	v->getTextInCurrentBlock(buf);
	TFPASS(buf.getLength() > 0);

	UT_GrowBuf sbuf;
	v->getTextInCurrentSection(sbuf);
	TFPASS(sbuf.getLength() > 0);

	PT_DocPosition lo = 0, hi = 0;
	TFPASS(v->getLineBounds(5, &lo, &hi));
	TFPASS(lo <= 5 && hi > lo);

	/* both selectRange overloads produce a real selection */
	v->selectRange(2, 8);
	TFPASS(!v->isSelectionEmpty());
	v->selectRange(std::make_pair<PT_DocPosition, PT_DocPosition>(3, 9));
	TFPASS(!v->isSelectionEmpty());

	std::vector<fl_BlockLayout *> blocks;
	v->getBlocksInSelection(&blocks);
	TFPASS(!blocks.empty());

	const PP_AttrProp *pSpan = nullptr;
	const PP_AttrProp *pBlock = nullptr;
	const PP_AttrProp *pSection = nullptr;
	const PP_AttrProp *pDoc = nullptr;
	TFPASS(v->getAllAttrProp(pSpan, pBlock, pSection, pDoc));
	TFPASS(pSpan != nullptr && pBlock != nullptr);

	UT_UTF8String val;
	bool bExplicit = false, bMixed = false;
	v->queryCharFormat("font-family", val, bExplicit, bMixed);
	UT_UTF8String val2;
	bool bExplicit2 = false;
	v->queryCharFormat("font-size", val2, bExplicit2, 4);

	/* a text selection is not an object selection */
	TFPASS(v->getSelectedObject() == nullptr);

	/* caret end of the selection is inside the selected range */
	TFPASS(v->isPosSelected(4));

	v->undoCount(true);
	v->undoCount(false);
}

TFTEST_MAIN("view state setters stick without a frame")
{
	HeadlessOpsView hv;
	TFPASS(hv.open("/test/wp/BillOfRights.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());
	FV_View *v = hv.view;

	v->setSelectionMode(FV_SelectionMode_Single);
	TFPASS(v->getSelectionMode() == FV_SelectionMode_Single);
	v->setSelectionMode(FV_SelectionMode_Multiple);
	TFPASS(v->getSelectionMode() == FV_SelectionMode_Multiple);
	TFPASS(v->getPrevSelectionMode() == FV_SelectionMode_Single);
	v->setSelectionMode(FV_SelectionMode_NONE);

	v->setPaperColor("ffffff");
	v->killBlink();
	v->fixInsertionPointCoords();
	v->remeasureCharsWithoutRebuild();
	v->dismissPasteTag();

	bool bWasMarking = v->isMarkRevisions();
	v->toggleMarkRevisions();
	TFPASS(v->isMarkRevisions() != bWasMarking);
	v->toggleMarkRevisions();
	TFPASS(v->isMarkRevisions() == bWasMarking);

	v->setShowRevisions(true);
	TFPASS(v->isShowRevisions());
	v->setShowRevisions(false);
	TFPASS(!v->isShowRevisions());

#ifdef ENABLE_SPELL
	v->isTextMisspelled();
#endif
}

TFTEST_MAIN("comments insert, edit, resolve and delete headless")
{
	HeadlessOpsView hv;
	TFPASS(hv.open("/test/wp/BillOfRights.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());
	FV_View *v = hv.view;

	/* anchor on a short range inside one block */
	v->cmdSelect(5, 9);
	TFPASS(v->insertAnnotation(77, "first note", "tester", "coverage", false));
	TFPASS(v->countAnnotations() >= 1);

	TFPASS(v->getAnnotationText(77) == "first note");
	TFPASS(v->setAnnotationText(77, "updated note"));
	TFPASS(v->getAnnotationText(77) == "updated note");

	std::string s;
	TFPASS(v->getAnnotationTitle(77, s) && s == "coverage");
	TFPASS(v->setAnnotationTitle(77, "new title"));
	TFPASS(v->getAnnotationTitle(77) == "new title");
	TFPASS(v->getAnnotationAuthor(77, s) && s == "tester");
	TFPASS(v->setAnnotationAuthor(77, "reviewer"));
	TFPASS(v->getAnnotationAuthor(77) == "reviewer");

	/* a second comment via the comment-command path */
	v->setPoint(20);
	TFPASS(v->cmdInsertComment());
	TFPASS(v->countAnnotations() >= 2);

	/* jump between comments, resolve the closest one */
	TFPASS(v->nextComment(true));
	TFPASS(v->resolveAnnotation());
	TFPASS(v->nextComment(false));

	/* delete one comment near the point, then all of them */
	v->nextComment(true);
	v->delAnnotation();
	TFPASS(v->delAllAnnotations());
	TFPASS(v->countAnnotations() == 0);
}

TFTEST_MAIN("styles append and list blocks report their list")
{
	HeadlessOpsView hv;
	TFPASS(hv.open("/test/wp/BillOfRights.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());
	FV_View *v = hv.view;

	const PP_PropertyVector style = {
		"name", "covwave-style",
		"type", "P",
		"basedon", "Normal",
		"props", "font-family:serif; font-size:10pt"
	};
	TFPASS(v->appendStyle(style));

	PD_Style *pStyle = nullptr;
	TFPASS(v->getDocument()->getStyle("covwave-style", &pStyle) && pStyle != nullptr);

	/* the fixture's amendments are list items */
	fl_BlockLayout *pListBlock = hv.findListBlock();
	TFPASS(pListBlock != nullptr);
	if (!pListBlock)
		return;

	v->setPoint(pListBlock->getPosition() + 1);
	std::vector<fl_BlockLayout *> listed;
	v->getAllBlocksInList(&listed);
	TFPASS(!listed.empty());

	const fl_AutoNumPtr &pAuto = pListBlock->getAutoNum();
	v->changeListStyle(pAuto, NUMBERED_LIST, 1, "%d.", ".",
					 "Times New Roman", 0.5f, 0.25f);
}

TFTEST_MAIN("symbol, mathml and embed objects insert at the caret")
{
	HeadlessOpsView hv;
	TFPASS(hv.open("/test/wp/BillOfRights.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());
	FV_View *v = hv.view;

	v->setPoint(10);
	PT_DocPosition before = v->getPoint();
	v->insertSymbol(static_cast<UT_UCS4Char>(0x03A9), "Symbol");
	TFPASS(v->getPoint() > before);

	/* MathML object insertion only needs a data-id attribute */
	TFPASS(v->cmdInsertMathML("cov-math-1", v->getPoint()));

	/* embed object: create a data item, update it, delete it */
	UT_ConstByteBufPtr pBuf(new UT_ByteBuf);
	static const UT_Byte payload[] = {'h','e','l','l','o'};
	const_cast<UT_ByteBuf *>(pBuf.get())->append(payload, sizeof(payload));
	TFPASS(v->cmdInsertEmbed(pBuf, v->getPoint(), "application/x-cov",
						   "embed-type: cov-test"));

	fp_Run *pEmbed = hv.findRunOfType(FPRUN_EMBED);
	TFPASS(pEmbed != nullptr);
	if (pEmbed)
	{
		TFPASS(v->cmdUpdateEmbed(pEmbed, pBuf, "application/x-cov",
								 "embed-type: cov-test2"));
		TFPASS(v->cmdDeleteEmbed(pEmbed));
	}
}

TFTEST_MAIN("table cells expose geometry and borders")
{
	HeadlessOpsView hv;
	TFPASS(hv.open("/test/wp/table.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());
	FV_View *v = hv.view;

	fl_BlockLayout *pCellBlock = hv.findCellBlock();
	TFPASS(pCellBlock != nullptr);
	if (!pCellBlock)
		return;

	PT_DocPosition posCell = pCellBlock->getPosition() + 1;
	TFPASS(v->isInTable(posCell));
	TFPASS(v->getTableAtPos(posCell) != nullptr);

	v->setPoint(posCell);
	TFPASS(v->isInTable());

	UT_sint32 l = 0, r = 0, t = 0, b = 0;
	TFPASS(v->getCellLineStyle(posCell, &l, &r, &t, &b));

	UT_sint32 cl = 0, cr = 0, ct = 0, cb = 0;
	TFPASS(v->getCellParams(posCell, &cl, &cr, &ct, &cb));
	TFPASS(cr > cl);

	gchar *pVal = nullptr;
	TFPASS(v->getCellProperty(posCell, "left-attach", pVal));
	pVal = nullptr;
	TFPASS(v->getCellProperty(posCell, "no-such-prop", pVal) == false);
}

TFTEST_MAIN("header/footer lifecycle and page number field")
{
	HeadlessOpsView hv;
	TFPASS(hv.open("/test/wp/BillOfRights.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());
	FV_View *v = hv.view;

	TFPASS(!v->isHdrFtrEdit());
	v->createThisHdrFtr(FL_HDRFTR_FOOTER, false);
	v->createThisHdrFtr(FL_HDRFTR_HEADER, false);

	/* insert a page-number field through the public cmd path */
	const PP_PropertyVector align = {
		"text-align", "center"
	};
	TFPASS(v->processPageNumber(FL_HDRFTR_FOOTER, align));
	/* a second call sees the existing page number and just reformats */
	TFPASS(v->processPageNumber(FL_HDRFTR_FOOTER, align));

	v->removeThisHdrFtr(FL_HDRFTR_FOOTER, false);
	v->removeThisHdrFtr(FL_HDRFTR_HEADER, false);
	TFPASS(!v->isHdrFtrEdit());

	/* frame grouping rejects degenerate input rather than crashing */
	std::vector<fl_FrameLayout *> empty;
	TFPASS(!v->groupFrames(empty));
	TFPASS(!v->ungroupFrames(empty));
}

TFTEST_MAIN("field runs update through the view")
{
	HeadlessOpsView hv;
	TFPASS(hv.open("/test/wp/fields.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());
	FV_View *v = hv.view;

	fp_Run *pField = hv.findRunOfType(FPRUN_FIELD);
	TFPASS(pField != nullptr);
	if (!pField)
		return;

	PT_DocPosition pos = pField->getBlock()->getPosition()
					   + pField->getBlockOffset();
	TFPASS(v->getFieldRun(pos) == pField);
	v->setPoint(pos);
	TFPASS(v->cmdUpdateField());

	/* no hyperlink at a plain text position in this fixture */
	v->getHyperLinkRun(2);
}
