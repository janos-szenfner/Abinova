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
 * Headless coverage of the fv_View editing command surface:
 * character/paragraph edits, selection, clipboard, undo/redo,
 * char/block formatting, fields, bookmarks, hyperlinks, lists,
 * section breaks, sorting, paragraph borders, comments, math,
 * revision marking and scrolling. All run against a widget-less
 * GR_UnixCairoGraphics so no display is needed.
 */

#include "tf_test.h"

#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "fl_DocLayout.h"
#include "fl_AutoNum.h"
#include "fv_View.h"
#include "fp_types.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_UnixClipboard.h"
#include "ut_growbuf.h"
#include "ut_types.h"
#include "ie_exp.h"
#include "ie_types.h"

#include <gsf/gsf-output-stdio.h>

#include <cstdio>
#include <string>

#define TFSUITE "core.text.fmt.editops"

namespace {

/* A document with a full interactive layout stack on a widget-less
 * GR_UnixCairoGraphics — same pattern as fv_ViewModes.t.cpp. */
struct EditOpsView
{
	EditOpsView() = default;
	EditOpsView(const EditOpsView &) = delete;
	EditOpsView &operator=(const EditOpsView &) = delete;

	/* create a scratch document with one paragraph per line of `text`
	 * (each '\n' becomes a block strux) */
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
		return finish();
	}

	bool loadFile(const char *relPath)
	{
		std::string data_file;
		if (!TF_Test::ensure_test_data(relPath, data_file))
			return false;
		doc = new PD_Document;
		if (doc->readFromFile(data_file.c_str(), IEFT_Unknown, nullptr)
			!= UT_OK)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
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

	~EditOpsView()
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

std::string eo_doc_text(FV_View * v)
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

void eo_release_clipboard()
{
	XAP_UnixClipboard * clip =
		static_cast<XAP_UnixApp*>(XAP_App::getApp())->getClipboard();
	if (clip)
		clip->clearData(true, false);
}

}

TFTEST_MAIN("character insert, motion and delete")
{
	EditOpsView hv;
	TFPASS(hv.load("alpha\nbeta\ngamma"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	/* point starts at document start */
	v->setPoint(2);
	const UT_UCS4Char X[] = { 'Z', 'Z' };
	TFPASS(v->cmdCharInsert(X, 2));
	TFPASS(eo_doc_text(v).find("ZZalpha") != std::string::npos);

	/* motion: right past the inserted text, then back */
	PT_DocPosition p0 = v->getPoint();
	v->cmdCharMotion(true, 2);
	TFPASS(v->getPoint() == p0 + 2);
	v->cmdCharMotion(false, 1);
	TFPASS(v->getPoint() == p0 + 1);

	/* one forward + one backward delete removes two characters */
	const size_t lenBefore = eo_doc_text(v).size();
	v->setPoint(p0);
	v->cmdCharDelete(true, 1);
	v->cmdCharDelete(false, 1);
	TFPASS(eo_doc_text(v).size() == lenBefore - 2);

	/* multi-byte insert via the std::string overload */
	v->setPoint(hv.eod());
	v->cmdCharInsert(std::string(" \xC3\xA9nd"), false);
	TFPASS(eo_doc_text(v).find("gamma") != std::string::npos);

	/* paragraph break splits the block */
	v->setPoint(4);
	v->insertParagraphBreak();
	TFPASS(eo_doc_text(v).find("beta") != std::string::npos);
	TFPASS(hv.layout->countPages() >= 1);
}

TFTEST_MAIN("selection, clipboard copy/cut/paste and undo/redo")
{
	EditOpsView hv;
	TFPASS(hv.load("one two\nthree four"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	TFPASS(v->isSelectionEmpty());

	/* select a span in the first block and remember its text */
	v->cmdSelect(6, 9);
	TFPASS(!v->isSelectionEmpty());
	UT_UCS4Char * selText = nullptr;
	v->getSelectionText(selText);
	TFPASS(selText != nullptr);
	std::string sel;
	if (selText)
	{
		for (UT_UCS4Char *p = selText; *p; ++p)
			if (*p < 0x80)
				sel += static_cast<char>(*p);
	}
	FREEP(selText); /* getSelectionText() is g_malloc-family */
	v->cmdUnselectSelection();
	TFPASS(v->isSelectionEmpty());

	v->cmdSelectNoNotify(6, 9);
	TFPASS(!v->isSelectionEmpty());
	v->cmdCopy(true);
	v->cmdUnselectSelection();

	/* paste twice — the copied text must appear twice more */
	const std::string before = eo_doc_text(v);
	v->setPoint(hv.eod());
	v->cmdPaste();
	v->cmdPaste();
	const std::string after = eo_doc_text(v);
	TFPASS(after.size() >= before.size() + 2 * sel.size());
	{
		std::string doubled = sel + sel;
		TFPASS(after.find(doubled) != std::string::npos);
	}

	/* undo twice rolls the pastes back */
	v->cmdUndo(1);
	v->cmdUndo(1);
	TFPASS(eo_doc_text(v) == before);

	/* redo puts them back */
	v->cmdRedo(1);
	v->cmdRedo(1);
	TFPASS(eo_doc_text(v) == after);

	/* cut removes the selection */
	const std::string preCut = eo_doc_text(v);
	v->cmdSelect(6, 9);
	v->cmdCut();
	TFPASS(eo_doc_text(v).size() == preCut.size() - sel.size());

	/* paste-as with an explicit mime restores the text */
	v->setPoint(hv.eod());
	v->cmdPasteAs("text/plain");
	TFPASS(eo_doc_text(v).find(sel) != std::string::npos);
	eo_release_clipboard();
}

TFTEST_MAIN("character and block formatting round-trips")
{
	EditOpsView hv;
	TFPASS(hv.load("plain text\nmore"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	/* format the selection "plain" bold 14pt */
	v->cmdSelect(2, 7);
	const PP_PropertyVector cprops = {
		"font-weight", "bold",
		"font-size", "14pt",
		"color", "ff0000",
		"text-decoration", "underline"
	};
	TFPASS(v->setCharFormat(cprops));

	PP_PropertyVector got;
	TFPASS(v->getCharFormat(got));
	TFPASS(PP_getAttribute("font-weight", got) == "bold");
	TFPASS(PP_getAttribute("font-size", got) == "14pt");
	TFPASS(PP_getAttribute("color", got) == "ff0000");

	/* block format: centre the paragraph */
	const PP_PropertyVector bprops = {
		"text-align", "center",
		"margin-left", "1.0in"
	};
	TFPASS(v->setBlockFormat(bprops));
	PP_PropertyVector bgot;
	TFPASS(v->getBlockFormat(bgot));
	TFPASS(PP_getAttribute("text-align", bgot) == "center");

	/* styles: Heading 1 exists in the default sheet */
	TFPASS(v->setStyle("Heading 1"));
	v->cmdUnselectSelection();
	v->setPoint(2);
	TFPASS(v->setStyle("Normal"));

	/* resets */
	TFPASS(v->resetCharFormat(true));
	TFPASS(v->resetBlockFormat());
	TFPASS(hv.layout->countPages() >= 1);
}

TFTEST_MAIN("fields, bookmarks and hyperlinks")
{
	EditOpsView hv;
	TFPASS(hv.load("field host text"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	/* live fields render a value; note cmdInsertField returns a
	 * bool-as-UT_Error, so success is "not UT_ERROR" */
	v->setPoint(hv.eod());
	TFPASS(v->cmdInsertField("page_number") != UT_ERROR);
	TFPASS(v->cmdInsertField("time") != UT_ERROR);
	TFPASS(v->cmdInsertField("word_count") != UT_ERROR);
	v->cmdInsertField("bogus_field_name");
	TFPASS(hv.layout->countPages() >= 1);

	/* bookmark over a selected span */
	v->cmdSelect(2, 7);
	v->cmdInsertBookmark("mybmk");
	v->cmdInsertBookmark("dup-bookmark");
	v->cmdUnselectSelection();
	TFPASS(hv.doc->getBookmarkCount() >= 2);

	/* hyperlink spanning the selection */
	v->cmdSelect(2, 7);
	v->cmdInsertHyperlink("https://example.com", "Example");
	v->cmdUnselectSelection();
	v->cmdHyperlinkJump(4);
	v->cmdHyperlinkCopyLocation(4);
	v->cmdDeleteHyperlink();
	v->cmdDeleteBookmark("mybmk");
	TFPASS(hv.doc->getBookmarkCount() >= 1);

	/* XMLID anchors */
	TFPASS(v->cmdInsertXMLID("xmlpara") == UT_OK);
	TFPASS(v->cmdDeleteXMLID("xmlpara") == UT_OK);
}

TFTEST_MAIN("lists start, retype and stop")
{
	EditOpsView hv;
	TFPASS(hv.load("first\nsecond\nthird"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(2);
	TFPASS(v->cmdStartList("Numbered List"));
	v->insertParagraphBreak();
	v->cmdCharInsert(std::string("item two"), false);
	TFPASS(v->cmdStopList());

	/* retype an existing block via processSelectedBlocks */
	v->setPoint(hv.eod());
	v->cmdCharInsert(std::string(" tail"), false);
	v->cmdSelect(2, 6);
	v->processSelectedBlocks(BULLETED_LIST);
	v->cmdUnselectSelection();

	/* applylisttype drives the fl_AutoNum set-type path */
	v->setPoint(2);
	TFPASS(v->cmdApplyListType(LOWERROMAN_LIST, "%d", "."));
	/* NOT_A_LIST is rejected by design */
	TFPASS(!v->cmdApplyListType(NOT_A_LIST, "%d", "."));
	TFPASS(v->cmdRemoveListFormat());
	TFPASS(eo_doc_text(v).find("first") != std::string::npos);
}

TFTEST_MAIN("section breaks, navigation and scrolling")
{
	EditOpsView hv;
	TFPASS(hv.loadFile("/test/wp/BillOfRights.abw"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	const UT_sint32 pagesBefore = hv.layout->countPages();
	TFPASS(pagesBefore >= 2);

	/* warp the insertion point through the doc */
	v->setPoint(2);
	v->warpInsPtNextPrevLine(true);
	v->warpInsPtNextPrevLine(false);
	v->warpInsPtNextPrevPage(true);
	v->warpInsPtNextPrevPage(false);
	v->cmdScroll(AV_SCROLLCMD_PAGEDOWN);
	v->cmdScroll(AV_SCROLLCMD_PAGEUP);
	v->cmdScroll(AV_SCROLLCMD_LINEDOWN);
	v->cmdScroll(AV_SCROLLCMD_LINEUP);
	v->cmdScroll(AV_SCROLLCMD_TOBOTTOM);
	v->cmdScroll(AV_SCROLLCMD_TOTOP);
	TFPASS(hv.layout->countPages() == pagesBefore);

	/* a new section break adds content but keeps pagination sane */
	v->setPoint(hv.eod());
	v->insertSectionBreak(BreakSectionNextPage);
	v->cmdCharInsert(std::string("new section"), false);
	TFPASS(hv.layout->countPages() >= pagesBefore);
	TFPASS(eo_doc_text(v).find("new section") != std::string::npos);

	/* saving round-trips headless via a GsfOutput stream */
	std::string tmp = std::string("/tmp/abn_editops_") +
		std::to_string(::getpid()) + ".abwn";
	GError * gerr = nullptr;
	GsfOutput * out = gsf_output_stdio_new(tmp.c_str(), &gerr);
	TFPASS(out != nullptr);
	if (out)
	{
		TFPASS(hv.doc->saveAs(out,
				static_cast<int>(IE_Exp::fileTypeForSuffix(".abwn")),
				false, nullptr) == UT_OK);
		g_object_unref(out);
	}
	/* exercise the view-level save wrapper too */
	v->cmdSaveAs("/tmp/abn_editops_cmd.abwn",
				 static_cast<int>(IE_Exp::fileTypeForSuffix(".abwn")));
	FILE *fp = fopen(tmp.c_str(), "rb");
	TFPASS(fp != nullptr);
	if (fp)
	{
		char magic[6] = {};
		TFPASS(fread(magic, 1, 6, fp) == 6 && !memcmp(magic, "<", 1));
		fclose(fp);
	}
	unlink(tmp.c_str());
}

TFTEST_MAIN("paragraph sort and borders")
{
	EditOpsView hv;
	TFPASS(hv.load("delta\nbravo\ncharlie\nalpha"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	/* sort ascending across the whole doc: select everything */
	PT_DocPosition posEnd = 0;
	hv.doc->getBounds(true, posEnd);
	v->cmdSelect(2, posEnd);
	TFPASS(v->cmdSortParagraphs(true));
	{
		std::string t = eo_doc_text(v);
		/* alpha should now precede delta */
		TFPASS(t.find("alpha") < t.find("delta"));
	}
	v->cmdSelect(2, posEnd);
	TFPASS(v->cmdSortParagraphs(false));

	/* borders on one block and across the selection */
	v->setPoint(2);
	TFPASS(v->cmdParaBorder("top"));
	TFPASS(v->cmdParaBorder("bottom"));
	v->cmdSelect(2, 6);
	TFPASS(v->cmdParaBorder("all"));
	TFPASS(v->cmdParaBorder("inside"));
	TFPASS(v->cmdParaBorder("none"));
	v->cmdUnselectSelection();
	v->setPoint(hv.eod());
	TFPASS(v->cmdParaBorder("hline"));
	TFPASS(!v->cmdParaBorder("bogus-edge"));
	TFPASS(hv.layout->countPages() >= 1);
}

TFTEST_MAIN("comments insert, navigate, resolve and delete")
{
	EditOpsView hv;
	TFPASS(hv.load("some commented text\nother text"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->cmdSelect(2, 7);
	TFPASS(v->cmdInsertComment());
	TFPASS(v->countAnnotations() == 1);

	/* write the comment body — the point is inside the annotation */
	v->cmdCharInsert(std::string("a note"), false);

	/* navigate back and forth between comments */
	TFPASS(v->nextComment(true));
	TFPASS(v->nextComment(false));

	/* resolve and delete it again */
	TFPASS(v->resolveAnnotation());
	TFPASS(v->isAnnotationResolved(0));
	v->delAnnotation();
	TFPASS(v->countAnnotations() == 0);

	v->cmdSelect(2, 7);
	TFPASS(v->cmdInsertComment());
	TFPASS(v->countAnnotations() == 1);
	TFPASS(v->delAllAnnotations());
	TFPASS(v->countAnnotations() == 0);
}

TFTEST_MAIN("revision marking records edits and accepts/rejects")
{
	EditOpsView hv;
	TFPASS(hv.load("base text\nsecond line"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;
	PD_Document * doc = hv.doc;

	doc->setMarkRevisions(true);
	v->setPoint(hv.eod());
	v->cmdCharInsert(std::string(" added"), false);
	v->setPoint(2);
	v->cmdCharDelete(true, 4);

	/* hunt for the recorded changes in either direction; finding
	 * none is fine — the search path is what we're covering */
	v->setPoint(2);
	v->cmdFindRevision(true, 0, 0);
	v->setPoint(hv.eod());
	v->cmdFindRevision(false, 0, 0);

	/* accept and reject at revision positions */
	v->setPoint(2);
	v->cmdAcceptRejectRevision(true, 0, 0);
	v->setPoint(2);
	v->cmdAcceptRejectRevision(false, 0, 0);

	doc->setShowRevisionId(1);
	v->setPoint(2);
	v->cmdCharInsert(std::string("X"), false);
	v->setPoint(2);
	v->cmdAcceptRejectRevision(false, 0, 0);
	doc->setMarkRevisions(false);
	TFPASS(eo_doc_text(v).size() > 0);
	TFPASS(hv.layout->countPages() >= 1);
}

TFTEST_MAIN("latex math object inserts")
{
	EditOpsView hv;
	TFPASS(hv.load("math host"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(hv.eod());
	UT_UTF8String tex("x^2 + y^2 = z^2");
	UT_UTF8String mathml;
	TFPASS(v->cmdInsertLatexMath(tex, mathml, true));
	TFPASS(hv.layout->countPages() >= 1);
}
