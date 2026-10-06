/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

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

/* Editing-path coverage for the piece table: span/strux insert and
 * delete at every boundary, format changes on spans and struxes,
 * fmt-marks, objects, undo/redo (which constructs and replays every
 * PX_ChangeRecord subclass through px_ChangeHistory), user-atomic
 * globs, and the PD_DocIterator/PD_StruxIterator text readers. */

#include <cstring>
#include <string>
#include <memory>

#include "tf_test.h"

#include "pd_Document.h"
#include "pd_Iterator.h"
#include "pf_Frag.h"
#include "pf_Frag_Object.h"
#include "pf_Frag_Strux.h"
#include "pf_Frag_Strux_Section.h"
#include "pl_Listener.h"
#include "pl_ListenerCoupleCloser.h"
#include "pp_AttrProp.h"
#include "pp_PropertyMap.h"
#include "pt_PieceTable.h"
#include "pt_Types.h"
#include "px_ChangeRecord.h"
#include "px_CR_Span.h"
#include "ut_growbuf.h"
#include "xad_Document.h"
#include "xap_App.h"
#include "xap_Prefs.h"

#define TFSUITE "core.text.ptbl.docedits"

namespace {

/* printable text content of the doc; strux/object positions report
 * UT_IT_NOT_CHARACTER so gate on the frag type */
std::string editDocText(PD_Document *doc)
{
	std::string out;
	PD_DocIterator t(*doc);
	while (t.getStatus() == UTIter_OK)
	{
		const pf_Frag *pf = t.getFrag();
		if (pf && pf->getType() == pf_Frag::PFT_Text)
		{
			const UT_UCS4Char c = t.getChar();
			if (c >= 32 && c < 0x7f)
				out += static_cast<char>(c);
		}
		++t;
	}
	return out;
}

/* document builder: content is appended while the piece table is in
 * loading state, finish() flips it to editing */
struct EditDoc
{
	EditDoc() = default;
	EditDoc(const EditDoc &) = delete;
	EditDoc &operator=(const EditDoc &) = delete;

	bool build()
	{
		doc = new PD_Document;
		if (doc->createRawDocument() != UT_OK)
			return false;
		return doc->appendStrux(PTX_Section, PP_NOPROPS, &sdhSection);
	}

	bool para(const char *ascii,
			  const PP_PropertyVector & attrs = PP_NOPROPS,
			  pf_Frag_Strux **out = nullptr)
	{
		UT_UCS4String s(ascii);
		if (!doc->appendStrux(PTX_Block, attrs, out))
			return false;
		return doc->appendSpan(s.ucs4_str(), s.length());
	}

	bool object(PTObjectType pto, const PP_PropertyVector & attrs)
	{
		return doc->appendObject(pto, attrs);
	}

	void finish()
	{
		doc->finishRawCreation();
	}

	~EditDoc()
	{
		if (doc)
			doc->unref();
	}

	PD_Document *doc = nullptr;
	pf_Frag_Strux *sdhSection = nullptr;
};

/* first text position of the block (right after its strux) */
PT_DocPosition blockText(PD_Document *doc, pf_Frag_Strux *blk)
{
	return doc->getStruxPosition(blk) + 1;
}

} // namespace

TFTEST_MAIN("insertSpan and deleteSpan at every boundary")
{
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b1 = nullptr;
	pf_Frag_Strux *b2 = nullptr;
	pf_Frag_Strux *b3 = nullptr;
	TFPASS(d.para("AAAA", PP_NOPROPS, &b1));
	TFPASS(d.para("BBBB", PP_NOPROPS, &b2));
	TFPASS(d.para("CCCC", PP_NOPROPS, &b3));
	d.finish();
	TFPASS(editDocText(d.doc) == "AAAABBBBCCCC");

	const PT_DocPosition p1 = blockText(d.doc, b1);
	const PT_DocPosition p2 = blockText(d.doc, b2);

	/* insert at block start, middle, and end */
	const UT_UCS4String ins("x");
	TFPASS(d.doc->insertSpan(p1, ins.ucs4_str(), 1));
	TFPASS(editDocText(d.doc) == "xAAAABBBBCCCC");
	TFPASS(d.doc->insertSpan(p1 + 3, ins.ucs4_str(), 1));
	TFPASS(editDocText(d.doc) == "xAAxAABBBBCCCC");
	TFPASS(d.doc->insertSpan(p2 - 1, ins.ucs4_str(), 1));
	TFPASS(editDocText(d.doc) == "xAAxxAABBBBCCCC");

	/* insertSpan via the std::string overload */
	TFPASS(d.doc->insertSpan(p1 + 5, std::string("y")));
	TFPASS(editDocText(d.doc) == "xAAxxyAABBBBCCCC");

	/* delete inside a block (iRealDeleteCount is only accumulated on
	 * the multi-block path, so don't assert its value) */
	UT_uint32 del = 0;
	TFPASS(d.doc->deleteSpan(p1, p1 + 2, nullptr, del));
	TFPASS(editDocText(d.doc) == "AxxyAABBBBCCCC");

	/* delete spanning the boundary between b1 and b2 removes the
	 * block break entirely */
	const PT_DocPosition p2b = blockText(d.doc, b2);
	TFPASS(d.doc->deleteSpan(p2b - 2, p2b + 2, nullptr, del));
	TFPASS(editDocText(d.doc) == "AxxyABBCCCC");

	/* delete up to the end of the last block */
	const pf_Frag *last = d.doc->getLastFrag();
	TFPASS(last != nullptr);
	const PT_DocPosition endPos = last->getPos() + last->getLength();
	TFPASS(d.doc->deleteSpan(endPos - 2, endPos, nullptr, del));
	TFPASS(editDocText(d.doc) == "AxxyABBCC");
}

TFTEST_MAIN("insertStrux splits and deleteStrux rejoins blocks")
{
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b1 = nullptr;
	TFPASS(d.para("abcdef", PP_NOPROPS, &b1));
	d.finish();

	const PT_DocPosition mid = blockText(d.doc, b1) + 3;

	/* splitting mid-block creates a second block */
	pf_Frag_Strux *bNew = nullptr;
	TFPASS(d.doc->insertStrux(mid, PTX_Block, &bNew));
	TFPASS(bNew != nullptr);
	TFPASS(editDocText(d.doc) == "abcdef");
	TFPASS(d.doc->getStruxPosition(bNew) > d.doc->getStruxPosition(b1));

	/* the new block carries "def" */
	{
		UT_GrowBuf gb;
		TFPASS(d.doc->getBlockBuf(bNew, &gb));
		TFPASS(gb.getLength() == 3);
		TFPASS(*gb.getPointer(0) == 'd');
	}

	/* changeStruxFmt on the split block */
	{
		const PP_PropertyVector props = {"text-align", "center"};
		TFPASS(d.doc->changeStruxFmt(PTC_AddFmt,
				d.doc->getStruxPosition(bNew),
				d.doc->getStruxPosition(bNew) + 1,
				PP_NOPROPS, props, PTX_Block));
	}

	/* deleteStrux at the boundary rejoins the two blocks */
	TFPASS(d.doc->deleteStrux(d.doc->getStruxPosition(bNew),
							  PTX_Block, true));
	TFPASS(editDocText(d.doc) == "abcdef");

	/* undo the rejoin: block boundary comes back */
	TFPASS(d.doc->canDo(true));
	TFPASS(d.doc->undoCmd(1));
	TFPASS(editDocText(d.doc) == "abcdef");
	const pf_Frag_Strux *bAgain = nullptr;
	TFPASS(d.doc->getStruxOfTypeFromPosition(
			   d.doc->getStruxPosition(b1) + 5, PTX_Block, &bAgain));
	TFPASS(bAgain != nullptr);
	TFPASS(bAgain != b1);

	/* redo removes it again */
	TFPASS(d.doc->redoCmd(1));
	TFPASS(editDocText(d.doc) == "abcdef");
}

TFTEST_MAIN("changeSpanFmt adds and removes props on a range")
{
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b1 = nullptr;
	TFPASS(d.para("normal text", PP_NOPROPS, &b1));
	d.finish();

	const PT_DocPosition s = blockText(d.doc, b1);
	const PT_DocPosition e = s + 6;

	/* add a bold prop to the first word */
	{
		const PP_PropertyVector props = {"font-weight", "bold"};
		TFPASS(d.doc->changeSpanFmt(PTC_AddFmt, s, e,
									PP_NOPROPS, props));
	}

	const pf_Frag *pf = d.doc->getFragFromPosition(s);
	TFPASS(pf != nullptr);
	{
		const PP_AttrProp *pAP = nullptr;
		TFPASS(d.doc->getAttrProp(pf->getIndexAP(), &pAP));
		const gchar *v = nullptr;
		TFPASS(pAP->getProperty("font-weight", v));
		TFPASS(v && !strcmp(v, "bold"));
	}

	/* remove it again */
	{
		const PP_PropertyVector props = {"font-weight", "bold"};
		TFPASS(d.doc->changeSpanFmt(PTC_RemoveFmt, s, e,
									PP_NOPROPS, props));
	}
	pf = d.doc->getFragFromPosition(s);
	{
		const PP_AttrProp *pAP = nullptr;
		TFPASS(d.doc->getAttrProp(pf->getIndexAP(), &pAP));
		const gchar *v = nullptr;
		TFPASS(!pAP->getProperty("font-weight", v));
	}

	/* the strux-level changeStruxFmt overload (auto type), positions
	 * inside the block so first==end strux */
	{
		const PP_PropertyVector props = {"margin-bottom", "0.2in"};
		PT_DocPosition bpos = d.doc->getStruxPosition(b1);
		TFPASS(d.doc->changeStruxFmt(PTC_AddFmt, bpos + 1, bpos + 3,
									PP_NOPROPS, props));
	}

	/* changeStruxFmtNoUndo path */
	{
		const PP_PropertyVector props = {"orphans", "3"};
		TFPASS(d.doc->changeStruxFmtNoUndo(PTC_AddFmt, b1,
										   PP_NOPROPS, props));
	}
}

TFTEST_MAIN("insertFmtMark/deleteFmtMark")
{
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b1 = nullptr;
	TFPASS(d.para("mark me", PP_NOPROPS, &b1));
	d.finish();

	const PT_DocPosition s = blockText(d.doc, b1) + 2;

	PP_AttrProp ap;
	ap.setProperty("font-style", "italic");
	TFPASS(d.doc->insertFmtMark(PTC_AddFmt, s, &ap));

	/* the zero-length fmtmark sits between the two split text frags */
	pf_Frag *pfL = d.doc->getFragFromPosition(s - 1);
	TFPASS(pfL != nullptr);
	TFPASS(pfL->getType() == pf_Frag::PFT_Text);
	pf_Frag *pfM = pfL->getNext();
	TFPASS(pfM != nullptr);
	TFPASS(pfM->getType() == pf_Frag::PFT_FmtMark);
	TFPASS(pfM->getLength() == 0);

	TFPASS(d.doc->deleteFmtMark(s));
	/* no fmtmark frags remain anywhere */
	int marks = 0;
	for (pf_Frag *pfw = d.doc->getFragFromPosition(0);
		 pfw; pfw = pfw->getNext())
		if (pfw->getType() == pf_Frag::PFT_FmtMark)
			++marks;
	TFPASS(marks == 0);
	TFPASS(editDocText(d.doc) == "mark me");
}

TFTEST_MAIN("objects: bookmark and RDF anchor insert/delete")
{
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b1 = nullptr;
	TFPASS(d.para("obj here", PP_NOPROPS, &b1));
	d.finish();

	const PT_DocPosition s = blockText(d.doc, b1) + 3;

	/* bookmark start; the zero-length object frag sits at s */
	{
		const PP_PropertyVector atts = {"name", "bm1", "type", "start"};
		TFPASS(d.doc->insertObject(s, PTO_Bookmark, atts, PP_NOPROPS));
	}
	pf_Frag *pfL = d.doc->getFragFromPosition(s - 1);
	TFPASS(pfL != nullptr && pfL->getType() == pf_Frag::PFT_Text);
	pf_Frag *pfO = pfL->getNext();
	TFPASS(pfO != nullptr);
	TFPASS(pfO->getType() == pf_Frag::PFT_Object);
	TFPASS(static_cast<pf_Frag_Object*>(pfO)->getObjectType()
		   == PTO_Bookmark);

	/* bookmark end after it */
	{
		const PP_PropertyVector atts = {"name", "bm1", "type", "end"};
		TFPASS(d.doc->insertObject(s + 1, PTO_Bookmark, atts,
								   PP_NOPROPS));
	}
	pfO = d.doc->getFragFromPosition(s)->getNext();
	TFPASS(pfO->getType() == pf_Frag::PFT_Object);

	/* RDF anchor object */
	{
		const PP_PropertyVector atts = {"xml:id", "a7"};
		TFPASS(d.doc->insertObject(s + 2, PTO_RDFAnchor, atts,
								   PP_NOPROPS));
	}
	pfO = d.doc->getFragFromPosition(s + 1)->getNext();
	TFPASS(pfO->getType() == pf_Frag::PFT_Object);
	TFPASS(static_cast<pf_Frag_Object*>(pfO)->getObjectType()
		   == PTO_RDFAnchor);

	/* objects occupy positions but contribute no text */
	TFPASS(editDocText(d.doc) == "obj here");
}

TFTEST_MAIN("undo/redo replays every change record")
{
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b1 = nullptr;
	pf_Frag_Strux *b2 = nullptr;
	TFPASS(d.para("one ", PP_NOPROPS, &b1));
	TFPASS(d.para("two ", PP_NOPROPS, &b2));
	d.finish();
	TFPASS(editDocText(d.doc) == "one two ");

	/* a fresh, just-finished doc has nothing to undo */
	TFPASS(!d.doc->canDo(true));
	TFPASS(!d.doc->canDo(false));
	TFPASS(d.doc->undoCount(true) == 0);

	const PT_DocPosition s = blockText(d.doc, b1);

	/* 1: insert text */
	const UT_UCS4String ins("XX");
	TFPASS(d.doc->insertSpan(s, ins.ucs4_str(), 2));
	TFPASS(editDocText(d.doc) == "XXone two ");

	/* 2: format it */
	const PP_PropertyVector props = {"font-weight", "bold"};
	TFPASS(d.doc->changeSpanFmt(PTC_AddFmt, s, s + 2,
								PP_NOPROPS, props));

	/* 3: split the block */
	pf_Frag_Strux *bNew = nullptr;
	TFPASS(d.doc->insertStrux(s + 2, PTX_Block, &bNew));
	TFPASS(editDocText(d.doc) == "XXone two ");

	/* 4: insert an object */
	TFPASS(d.doc->insertObject(s + 3, PTO_Bookmark,
			{"name", "u1", "type", "start"}, PP_NOPROPS));

	/* 5: delete across the split boundary */
	UT_uint32 del = 0;
	PT_DocPosition b2pos = d.doc->getStruxPosition(b2);
	TFPASS(d.doc->deleteSpan(b2pos - 1, b2pos + 2, nullptr, del));

	/* undo each step and verify the document text */
	TFPASS(d.doc->undoCmd(1)); /* step 5 */
	TFPASS(editDocText(d.doc) == "XXone two ");
	TFPASS(d.doc->undoCmd(1)); /* step 4 */
	TFPASS(editDocText(d.doc) == "XXone two ");
	TFPASS(d.doc->undoCmd(1)); /* step 3 */
	TFPASS(editDocText(d.doc) == "XXone two ");
	TFPASS(d.doc->undoCmd(1)); /* step 2 */
	TFPASS(editDocText(d.doc) == "XXone two ");
	TFPASS(d.doc->undoCmd(1)); /* step 1 */
	TFPASS(editDocText(d.doc) == "one two ");
	/* drain any remaining undoable records (strux inserts can emit
	 * more than one change record) */
	while (d.doc->canDo(true))
		TFPASS(d.doc->undoCmd(1));
	TFPASS(editDocText(d.doc) == "one two ");
	TFPASS(d.doc->canDo(false));

	/* redo everything (drain: a bookmark insert emits an extra
	 * invisible change record alongside the visible edits) */
	while (d.doc->canDo(false))
		TFPASS(d.doc->redoCmd(1));
	TFPASS(editDocText(d.doc) == "XXonewo ");
}

TFTEST_MAIN("user-atomic glob collapses edits into one undo step")
{
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b1 = nullptr;
	TFPASS(d.para("glob text", PP_NOPROPS, &b1));
	d.finish();

	const PT_DocPosition s = blockText(d.doc, b1);
	const UT_UCS4String ins("GG");

	d.doc->beginUserAtomicGlob();
	TFPASS(d.doc->insertSpan(s, ins.ucs4_str(), 2));
	TFPASS(d.doc->insertSpan(s + 2, ins.ucs4_str(), 2));
	pf_Frag_Strux *bNew = nullptr;
	TFPASS(d.doc->insertStrux(s + 4, PTX_Block, &bNew));
	d.doc->endUserAtomicGlob();

	TFPASS(editDocText(d.doc) == "GGGGglob text");

	/* one undo removes all three edits */
	TFPASS(d.doc->undoCmd(1));
	TFPASS(editDocText(d.doc) == "glob text");
	TFPASS(!d.doc->canDo(true));

	/* one redo restores them */
	TFPASS(d.doc->redoCmd(1));
	TFPASS(editDocText(d.doc) == "GGGGglob text");
}

TFTEST_MAIN("PD_DocIterator navigation and find")
{
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b1 = nullptr;
	pf_Frag_Strux *b2 = nullptr;
	TFPASS(d.para("alpha beta", PP_NOPROPS, &b1));
	TFPASS(d.para("gamma", PP_NOPROPS, &b2));
	d.finish();

	PD_DocIterator it(*d.doc);
	TFPASS(it.getStatus() == UTIter_OK);
	TFPASS(it.getPosition() == 0);

	/* forward scan to first text char of block 1 */
	const PT_DocPosition s = blockText(d.doc, b1);
	it.setPosition(s);
	TFPASS(it.getPosition() == static_cast<UT_uint32>(s));
	TFPASS(it.getChar() == 'a');
	++it;
	TFPASS(it.getChar() == 'l');
	it += 5;
	TFPASS(it.getChar() == 'b');
	--it;
	TFPASS(it.getChar() == ' ');
	it -= 5;
	TFPASS(it.getChar() == 'a');

	/* subscript operator repositions */
	TFPASS(it[s + 1] == 'l');
	TFPASS(it.getPosition() == static_cast<UT_uint32>(s + 1));

	/* find, forward and backward */
	UT_UCS4Char needle[5] = {'b', 'e', 't', 'a', 0};
	it.setPosition(0);
	UT_uint32 found = it.find(needle, 4, true);
	TFPASS(found == static_cast<UT_uint32>(s + 6));
	it.setPosition(static_cast<UT_uint32>(s) + 12);
	found = it.find(needle, 4, false);
	TFPASS(found == static_cast<UT_uint32>(s + 6));

	/* miss leaves the iterator out of bounds */
	UT_UCS4Char miss[3] = {'z', 'z', 0};
	it.setPosition(0);
	it.find(miss, 2, true);
	TFPASS(it.getStatus() == UTIter_OutOfBounds);
	/* recoverable via setPosition */
	it.setPosition(s);
	TFPASS(it.getStatus() == UTIter_OK);
	TFPASS(it.getChar() == 'a');

	/* upper limit stops iteration */
	PD_DocIterator lim(*d.doc);
	lim.setUpperLimit(s + 3);
	while (lim.getStatus() == UTIter_OK)
		++lim;
	TFPASS(lim.getPosition() <= static_cast<UT_uint32>(s) + 4);

	/* makeCopy clones position+state */
	PD_DocIterator src(*d.doc);
	src.setPosition(s + 2);
	std::unique_ptr<UT_TextIterator> cpy(src.makeCopy());
	TFPASS(cpy.get() != nullptr);
	TFPASS(cpy->getPosition() == static_cast<UT_uint32>(s + 2));
	TFPASS(cpy->getChar() == 'p');

	/* copy via a second iterator's find(text) overload */
	PD_DocIterator haystack(*d.doc);
	haystack.setPosition(0);
	UT_UCS4Char g[6] = {'g', 'a', 'm', 'm', 'a', 0};
	found = haystack.find(g, 5, true);
	TFPASS(found == static_cast<UT_uint32>(blockText(d.doc, b2)));
}

TFTEST_MAIN("PD_StruxIterator walks a single strux")
{
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b1 = nullptr;
	pf_Frag_Strux *b2 = nullptr;
	TFPASS(d.para("walkme", PP_NOPROPS, &b1));
	TFPASS(d.para("next", PP_NOPROPS, &b2));
	d.finish();

	/* strux-iterator offsets are relative to the strux; offset 0 is
	 * the strux marker itself, text starts at offset 1 */
	PD_StruxIterator it(b1);
	TFPASS(it.getStatus() == UTIter_OK);
	it.setPosition(1);
	TFPASS(it.getChar() == 'w');
	it += 5;
	TFPASS(it.getChar() == 'e');
	++it; /* lands on the next block's strux marker */
	TFPASS(it.getStatus() == UTIter_OK);
	TFPASS(it.getChar() == UT_IT_NOT_CHARACTER);
	--it;
	TFPASS(it.getChar() == 'e');
	it.setPosition(1);
	TFPASS(it.getChar() == 'w');
	TFPASS(it[2] == 'a');
	TFPASS(it.getPosition() == 2);

	std::unique_ptr<UT_TextIterator> cpy(it.makeCopy());
	TFPASS(cpy.get() != nullptr);
	TFPASS(cpy->getPosition() == 2);
}

TFTEST_MAIN("strux navigation + piece table queries")
{
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b1 = nullptr;
	pf_Frag_Strux *b2 = nullptr;
	TFPASS(d.para("nav1", PP_NOPROPS, &b1));
	TFPASS(d.para("nav2", PP_NOPROPS, &b2));
	d.finish();

	pt_PieceTable *pt = d.doc->getPieceTable();
	TFPASS(pt != nullptr);
	TFPASS(pt->getDocument() == d.doc);

	/* strux-from-position lookups */
	pf_Frag_Strux *sdh = nullptr;
	TFPASS(pt->getStruxOfTypeFromPosition(blockText(d.doc, b2),
										 PTX_Block, &sdh));
	TFPASS(sdh == b2);
	TFPASS(pt->getStruxOfTypeFromPosition(blockText(d.doc, b2),
										 PTX_Section, &sdh));
	TFPASS(sdh == d.sdhSection);

	/* mutable lookup variant on the document */
	pf_Frag_Strux *sdhMut = nullptr;
	TFPASS(d.doc->getMutStruxOfTypeFromPosition(
			   blockText(d.doc, b1), PTX_Block, &sdhMut));
	TFPASS(sdhMut == b1);

	/* block buffer extraction */
	UT_GrowBuf gb;
	TFPASS(d.doc->getBlockBuf(b2, &gb));
	TFPASS(gb.getLength() == 4);
	TFPASS(*gb.getPointer(0) == 'n');

	/* frag navigation: block strux -> text -> next block strux */
	pf_Frag *pfB1 = d.doc->getFragFromPosition(d.doc->getStruxPosition(b1));
	TFPASS(pfB1 == b1);
	TFPASS(pfB1->getType() == pf_Frag::PFT_Strux);
	TFPASS(b1->getStruxType() == PTX_Block);
	pf_Frag *pfText = pfB1->getNext();
	TFPASS(pfText != nullptr);
	TFPASS(pfText->getType() == pf_Frag::PFT_Text);
	TFPASS(pfText->getLength() == 4);
	TFPASS(pfText->getNext() == b2);

	/* document queries */
	TFPASS(d.doc->getFragFromPosition(blockText(d.doc, b1)) == pfText);
	TFPASS(d.doc->getStruxPosition(b1) == b1->getPos());

	/* isDirty after edits */
	d.doc->forceDirty();
	TFPASS(d.doc->isDirty());
}

TFTEST_MAIN("loading-state before-frag insertions")
{
	/* exercises the importer-facing insertBeforeFrag APIs which only
	 * run while the piece table is in loading state */
	pt_PieceTable pt(nullptr);
	pt.setPieceTableState(PTS_Loading);

	pf_Frag_Strux *blk = nullptr;
	TFPASS(pt.appendStrux(PTX_Block, PP_NOPROPS, &blk));
	const UT_UCS4String tail("tail");
	TFPASS(pt.appendSpan(tail.ucs4_str(), tail.length()));

	pf_Frag *fragTail = pt.getFragments().getLast();
	TFPASS(fragTail != nullptr);

	/* insert a second block + content before the tail text */
	pf_Frag_Strux *blk2 = nullptr;
	TFPASS(pt.insertStruxBeforeFrag(fragTail, PTX_Block, PP_NOPROPS,
									&blk2));
	TFPASS(blk2 != nullptr);
	const UT_UCS4String mid("mid");
	TFPASS(pt.insertSpanBeforeFrag(fragTail, mid.ucs4_str(), 3));
	TFPASS(pt.insertFmtMarkBeforeFrag(fragTail));
	TFPASS(pt.insertFmtMarkBeforeFrag(fragTail, {"a", "b"}));
	/* NB: PTO_Bookmark needs a real document (addBookmark); use an
	 * RDF anchor here since this piece table has none */
	TFPASS(pt.insertObjectBeforeFrag(fragTail, PTO_RDFAnchor,
								   {"xml:id", "ra1"}));

	/* ordering: blk, blk2(empty), mid text, fmtmarks, object, tail */
	pf_Frag *pf = pt.getFragments().getFirst();
	TFPASS(pf == blk);
	pf = pf->getNext();
	TFPASS(pf == blk2);
	pf = pf->getNext();
	TFPASS(pf->getType() == pf_Frag::PFT_Text);
	TFPASS(pf->getLength() == 3);
	pf = pf->getNext();
	TFPASS(pf->getType() == pf_Frag::PFT_FmtMark);
	pf = pf->getNext();
	TFPASS(pf->getType() == pf_Frag::PFT_FmtMark);
	pf = pf->getNext();
	TFPASS(pf->getType() == pf_Frag::PFT_Object);
	pf = pf->getNext();
	TFPASS(pf == fragTail);

	/* appendStruxFmt merges attrs onto the strux */
	const PP_PropertyVector extra = {"prop1", "val1"};
	TFPASS(pt.appendStruxFmt(blk, extra));
}

namespace {

/* minimal PL_Listener that just counts callbacks */
class CountingListener : public PL_Listener
{
public:
	bool populate(fl_ContainerLayout * sfh,
				  const PX_ChangeRecord * pcr) override
	{
		++nPopulate;
		lastCR = pcr;
		(void)sfh;
		return true;
	}
	bool populateStrux(pf_Frag_Strux * sdh,
					   const PX_ChangeRecord * pcr,
					   fl_ContainerLayout * * psfh) override
	{
		++nStrux;
		(void)sdh; (void)pcr; (void)psfh;
		return true;
	}
	bool change(fl_ContainerLayout * sfh,
				const PX_ChangeRecord * pcr) override
	{
		++nChange;
		(void)sfh; (void)pcr;
		return true;
	}
	bool insertStrux(fl_ContainerLayout * sfh,
					 const PX_ChangeRecord * pcr,
					 pf_Frag_Strux * sdhNew,
					 PL_ListenerId lid,
					 void (* pfnBindHandles)(pf_Frag_Strux * sdhNew,
											 PL_ListenerId lid,
											 fl_ContainerLayout * sfhNew)) override
	{
		++nInsertStrux;
		(void)sfh; (void)pcr; (void)sdhNew; (void)lid; (void)pfnBindHandles;
		return true;
	}
	bool signal(UT_uint32 iSignal) override
	{
		(void)iSignal;
		return true;
	}

	int nPopulate = 0;
	int nStrux = 0;
	int nChange = 0;
	int nInsertStrux = 0;
	const PX_ChangeRecord * lastCR = nullptr;
};

} // namespace

TFTEST_MAIN("field and hyperlink objects")
{
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b = nullptr;
	TFPASS(d.para("ab", PP_NOPROPS, &b));
	TFPASS(d.para("cd"));
	d.finish();

	pt_PieceTable * pt = d.doc->getPieceTable();
	TFPASS(pt != nullptr);
	UT_uint32 cnt = 0;

	PT_DocPosition pos = blockText(d.doc, b) + 1;
	TFPASS(d.doc->insertObject(pos, PTO_Field,
							   PP_PropertyVector{"type", "time"},
							   PP_NOPROPS));
	const pf_Frag * pf = nullptr;
	PT_BlockOffset off = 0;
	TFPASS(pt->getFragFromPosition(pos, &pf, &off));
	TFPASS(pf != nullptr && pf->getType() == pf_Frag::PFT_Object);
	const pf_Frag_Object * pfo = static_cast<const pf_Frag_Object *>(pf);
	TFPASS(pfo->getObjectType() == PTO_Field);
	TFPASS(pfo->getField() != nullptr);
	TFPASS(editDocText(d.doc) == "abcd");

	// deleting the field frag runs the field tweak + complex delete paths
	TFPASS(pt->deleteFieldFrag(pf));
	pf = nullptr;
	TFPASS(pt->getFragFromPosition(pos, &pf, &off));
	TFPASS(pf && pf->getType() == pf_Frag::PFT_Text);

	// hyperlink pair, then delete spanning the start marker; the HAR
	// path removes the end marker too
	pos = blockText(d.doc, b) + 1;
	TFPASS(d.doc->insertObject(pos, PTO_Hyperlink,
							   PP_PropertyVector{"xlink:href", "http://example.com"},
							   PP_NOPROPS));
	TFPASS(d.doc->insertObject(pos + 2, PTO_Hyperlink, PP_NOPROPS, PP_NOPROPS));
	// deleting the whole marked span exercises the HAR path, which
	// removes both markers along with the linked content
	TFPASS(d.doc->deleteSpan(pos, pos + 3, nullptr, cnt));
	TFPASS(editDocText(d.doc) == "acd");

	// insert a bookmark pair around the remaining char of block 1
	pos = blockText(d.doc, b);
	TFPASS(d.doc->insertObject(pos, PTO_Bookmark,
							   PP_PropertyVector{"type", "start", "name", "bm1"},
							   PP_NOPROPS));
	TFPASS(d.doc->insertObject(pos + 2, PTO_Bookmark,
							   PP_PropertyVector{"type", "end", "name", "bm1"},
							   PP_NOPROPS));
	TFPASS(editDocText(d.doc) == "acd");
}

TFTEST_MAIN("piece table and frag helpers")
{
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b1 = nullptr;
	pf_Frag_Strux *b2 = nullptr;
	TFPASS(d.para("aa", PP_NOPROPS, &b1));
	TFPASS(d.para("bb", PP_NOPROPS, &b2));
	d.finish();

	pt_PieceTable * pt = d.doc->getPieceTable();
	PT_DocPosition p1 = blockText(d.doc, b1);
	PT_DocPosition p2 = blockText(d.doc, b2);

	// inSameBlock
	TFPASS(pt->inSameBlock(p1, p1 + 1) != nullptr);
	TFPASS(pt->inSameBlock(p1, p2) == nullptr);

	// getEndOfBlock / getBlockFromPosition / getPosEnd / calcDocsize
	TFPASS(pt->getEndOfBlock(p1, p2 + 1) != nullptr);
	TFPASS(pt->getBlockFromPosition(p1) == b1);
	TFPASS(d.doc->getBlockFromPosition(p2) == b2);
	TFPASS(pt->getPosEnd() > 0);
	TFPASS(pt->calcDocsize() > 0);

	// dumpDoc is a debug aid; it walks the range without crashing
	pt->dumpDoc("test", p1, p2 + 1);

	// frag comparisons and strux helpers
	const pf_Frag * pf1 = nullptr;
	const pf_Frag * pf2 = nullptr;
	PT_BlockOffset off = 0;
	TFPASS(pt->getFragFromPosition(p1, &pf1, &off));
	TFPASS(pt->getFragFromPosition(p2, &pf2, &off));
	TFPASS(pf1 != nullptr && pf2 != nullptr);
	TFPASS(*pf1 == *pf1);
	TFPASS(!(*pf1 == *pf2));
	TFPASS(pf1->isContentEqual(*pf1));

	const pf_Frag * fs = b1;
	TFPASS(fs->getNextStrux(PTX_Block) != nullptr);
	TFPASS(tryDownCastStrux(b1, PTX_Block) == b1);
	TFPASS(tryDownCastStrux(static_cast<const pf_Frag *>(b1), PTX_Block) == b1);
	TFPASS(tryDownCastStrux(static_cast<const pf_Frag *>(b1), PTX_Section) == nullptr);
	TFPASS(tryDownCastStrux(static_cast<const pf_Frag *>(nullptr), PTX_Block) == nullptr);
	// isMatchingType only recognises section/end-section style pairs;
	// a block does not match its own type
	TFPASS(!b1->isMatchingType(PTX_Block));
	TFPASS(!b1->isMatchingType(PTX_Section));

	PX_ChangeRecord * pcr = nullptr;
	TFPASS(b1->createSpecialChangeRecord(&pcr, 0));
	delete pcr;

	// internal consistency check used by debug builds
	pt->getFragments().verifyDoc();

	// static property map helper
	TFPASS(PP_PropertyMap::linestyle_for_CSS("solid") != nullptr);
}

TFTEST_MAIN("styles, strux-format noupdate and lists")
{
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b1 = nullptr;
	pf_Frag_Strux *b2 = nullptr;
	TFPASS(d.para("aa", PP_NOPROPS, &b1));
	TFPASS(d.para("bb", {"style", "MyStyle"}, &b2));
	// loading-time-only calls
	TFPASS(d.doc->appendStyle({"name", "MyStyle",
							   "type", "P",
							   "props", "font-weight:bold"}));
	TFPASS(d.doc->appendStruxFmt(b1, {"listid", "1"}));
	TFPASS(d.doc->appendLastStruxFmt(PTX_Block,
									 PP_NOPROPS,
									 PP_PropertyVector{"text-indent", "0.5in"},
									 false));
	TFPASS(d.doc->appendLastStruxFmt(PTX_Block,
									 PP_NOPROPS,
									 std::string("margin-bottom:0.1in"),
									 false));
	d.finish();

	pt_PieceTable * pt = d.doc->getPieceTable();
	(void)pt;

	// style queries
	TFPASS(d.doc->getDefaultStyle() != nullptr);
	TFPASS(d.doc->getStyleFromStrux(b2) != nullptr);
	const gchar * v = nullptr;
	TFPASS(d.doc->getStyleProperty("MyStyle", "font-weight", v));
	TFPASS(v && std::strcmp(v, "bold") == 0);
	TFPASS(!d.doc->getStyleProperty("NoSuchStyle", "x", v));
	TFPASS(d.doc->addStyleProperties("MyStyle", {"font-size", "12pt"}));

	// style strux walks
	TFPASS(d.doc->findForwardStyleStrux("MyStyle", 0) == b2);
	TFPASS(d.doc->findPreviousStyleStrux("MyStyle",
										 d.doc->getStruxPosition(b2) + 2) == b2);

	std::vector<PD_Style *> styles;
	d.doc->getAllUsedStyles(&styles);
	TFPASS(!styles.empty());

	// strux attribute/property getters incl. revision-filtered variants
	const char * av = nullptr;
	TFPASS(d.doc->getAttributeFromStrux(b1, false, 0, "listid", &av));
	TFPASS(av && std::strcmp(av, "1") == 0);
	TFPASS(d.doc->getAPIFromStrux(b1) != 0);

	// no-update formatting entry points (import-time API)
	TFPASS(d.doc->changeStruxFormatNoUpdate(PTC_AddFmt, b1,
											{"text-align", "center"}));
	TFPASS(d.doc->changeLastStruxFmtNoUndo(d.doc->getStruxPosition(b2),
										   PTX_Block,
										   PP_NOPROPS,
										   PP_PropertyVector{"color", "green"},
										   false));
	TFPASS(d.doc->changeLastStruxFmtNoUndo(d.doc->getStruxPosition(b2),
										   PTX_Block,
										   PP_NOPROPS,
										   std::string("color:blue"),
										   false));

	// list change on a strux
	TFPASS(d.doc->changeStruxForLists(b1, "0"));

	// raw piece-table no-update insert path
	pf_Frag_Strux * before = nullptr;
	TFPASS(d.doc->getMutStruxOfTypeFromPosition(blockText(d.doc, b2),
											  PTX_Block, &before));
	TFPASS(before == b2);
	TFPASS(d.doc->insertStruxNoUpdateBefore(b2, PTX_Block,
										  {"inserted", "yes"}));

	// strux ordering helper
	TFPASS(d.doc->isStruxBeforeThis(d.sdhSection, PTX_Block) == false ||
		   d.doc->isStruxBeforeThis(d.sdhSection, PTX_Section) == false);

	// fmt handle list is empty with no layouts attached
	TFPASS(d.doc->getNthFmtHandle(b1, 0) == nullptr);
}

TFTEST_MAIN("table strux navigation")
{
	PD_Document * doc = new PD_Document;
	TFPASS(doc->createRawDocument() == UT_OK);
	TFPASS(doc->appendStrux(PTX_Section, PP_NOPROPS));

	// table: one cell containing a block
	pf_Frag_Strux * tbl = nullptr;
	pf_Frag_Strux * cell = nullptr;
	TFPASS(doc->appendStrux(PTX_SectionTable, {"table-column-props",
											 "1in"}, &tbl));
	TFPASS(doc->appendStrux(PTX_SectionCell, PP_NOPROPS, &cell));
	// getCellStruxFromRowCol reads the attach coordinates as strux
	// properties, not attributes
	TFPASS(doc->appendLastStruxFmt(PTX_SectionCell, PP_NOPROPS,
								   PP_PropertyVector{"left-attach", "0",
													 "right-attach", "1",
													 "top-attach", "0",
													 "bot-attach", "1"},
								   false));
	UT_UCS4String s("cell");
	TFPASS(doc->appendStrux(PTX_Block, PP_NOPROPS));
	TFPASS(doc->appendSpan(s.ucs4_str(), s.length()));
	TFPASS(doc->appendStrux(PTX_EndCell, PP_NOPROPS));
	TFPASS(doc->appendStrux(PTX_EndTable, PP_NOPROPS));
	TFPASS(doc->appendStrux(PTX_Block, PP_NOPROPS));
	doc->finishRawCreation();
	TFPASS(tbl != nullptr && cell != nullptr);

	PT_DocPosition posTable = doc->getStruxPosition(tbl);
	TFPASS(doc->getEndTableStruxFromTableStrux(tbl) != nullptr);
	// the Pos variant wants a position inside the table, not the strux
	// boundary itself
	TFPASS(doc->getEndTableStruxFromTablePos(posTable + 1) != nullptr);
	TFPASS(doc->getEndCellStruxFromCellStrux(cell) != nullptr);
	TFPASS(doc->getEndCellMutStruxFromCellStrux(cell) != nullptr);
	TFPASS(doc->getCellStruxFromRowCol(tbl, false, 0, 0, 0) == cell);

	// strux-of-type queries over embedded content
	const pf_Frag_Strux * found = nullptr;
	TFPASS(doc->getStruxOfTypeFromPosition(posTable + 1, PTX_SectionTable,
										 &found));
	TFPASS(found == tbl);

	doc->unref();
}

TFTEST_MAIN("revisions: marked edits, accept and reject")
{
	// additions get a revision attribute; acceptAll folds them in
	{
		EditDoc d;
		TFPASS(d.build());
		pf_Frag_Strux *b = nullptr;
		TFPASS(d.para("base", PP_NOPROPS, &b));
		d.finish();

		// setMarkRevisions auto-records the current revision id, so the
		// id must be chosen first for getHighestRevisionId to see it
		d.doc->setRevisionId(4);
		d.doc->setMarkRevisions(true);
		TFPASS(d.doc->isMarkRevisions());

		UT_UCS4String ins("NEW");
		PT_DocPosition pos = blockText(d.doc, b);
		TFPASS(d.doc->insertSpan(pos, ins.ucs4_str(), ins.length()));
		TFPASS(d.doc->getHighestRevisionId() >= 4);
		TFPASS(d.doc->getRevisionId() == 4);
		d.doc->setShowRevisionId(PD_MAX_REVISION);
		TFPASS(d.doc->getShowRevisionId() == PD_MAX_REVISION);

		// explodeRevisions exposes the marked AP
		std::unique_ptr<PP_RevisionAttr> exploded;
		const PP_AttrProp * ap = nullptr;
		PT_AttrPropIndex api = d.doc->getAPIFromStrux(b);
		TFPASS(d.doc->getAttrProp(api, &ap));
		bool hidden = false;
		d.doc->explodeRevisions(exploded, ap, true, 4, hidden);

		TFPASS(d.doc->acceptAllRevisionsUpTo(4));
		TFPASS(editDocText(d.doc) == "NEWbase");

		d.doc->setMarkRevisions(false);
	}

	// rejectAll removes the added text
	{
		EditDoc d;
		TFPASS(d.build());
		pf_Frag_Strux *b = nullptr;
		TFPASS(d.para("keep", PP_NOPROPS, &b));
		d.finish();

		d.doc->setMarkRevisions(true);
		d.doc->setRevisionId(9);
		UT_UCS4String ins("ZZ");
		PT_DocPosition pos = blockText(d.doc, b);
		TFPASS(d.doc->insertSpan(pos, ins.ucs4_str(), ins.length()));
		TFPASS(d.doc->rejectAllRevisionsUpTo(9));
		TFPASS(editDocText(d.doc) == "keep");
	}

	// acceptRejectRevision over a range + rejectAllHigherRevisions
	{
		EditDoc d;
		TFPASS(d.build());
		pf_Frag_Strux *b = nullptr;
		TFPASS(d.para("xx", PP_NOPROPS, &b));
		d.finish();

		d.doc->setMarkRevisions(true);
		d.doc->setRevisionId(7);
		UT_UCS4String ins("Y");
		PT_DocPosition pos = blockText(d.doc, b);
		TFPASS(d.doc->insertSpan(pos, ins.ucs4_str(), ins.length()));
		// bReject=true removes the rev-7 addition, leaving "xx"
		TFPASS(d.doc->acceptRejectRevision(true, pos, pos + 1, 7));
		TFPASS(editDocText(d.doc) == "xx");

		TFPASS(d.doc->acceptAllRevisions());
		TFPASS(d.doc->rejectAllHigherRevisions(PD_MAX_REVISION));
		d.doc->purgeRevisionTable(true);
		d.doc->purgeRevisionTable();
	}
}

TFTEST_MAIN("listener subset walk and misc document queries")
{
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b1 = nullptr;
	pf_Frag_Strux *b2 = nullptr;
	TFPASS(d.para("pre", PP_NOPROPS, &b1));
	// bookmark pair around the second block's text
	TFPASS(d.object(PTO_Bookmark,
					PP_PropertyVector{"type", "start", "name", "mk"}));
	TFPASS(d.para("mid", PP_NOPROPS, &b2));
	TFPASS(d.object(PTO_Bookmark,
					PP_PropertyVector{"type", "end", "name", "mk"}));
	TFPASS(d.para("post"));
	d.finish();

	// bookmark registry queries
	TFPASS(d.doc->getBookmarkCount() == 1);
	TFPASS(!d.doc->isBookmarkUnique("mk"));
	TFPASS(d.doc->isBookmarkUnique("not-there"));
	TFPASS(!d.doc->isBookmarkRelativeLink("mk"));
	// the end marker lands inside b2 after the 3-char "mid" span
	po_Bookmark * bm = d.doc->getBookmark(b2, 3);
	TFPASS(bm != nullptr);
	TFPASS(std::strcmp(bm->getName(), "mk") == 0);
	d.doc->removeBookmark("mk");
	TFPASS(d.doc->getBookmarkCount() == 0);

	// tellListenerSubset with a couple closer walks the range
	CountingListener sink;
	PL_ListenerCoupleCloser closer;
	closer.setDocument(d.doc);
	closer.setDelegate(&sink);
	PD_DocumentRange range;
	range.set(d.doc, 0, d.doc->getStruxPosition(b2) + 3);
	TFPASS(d.doc->tellListenerSubset(&sink, &range, &closer));
	TFPASS(sink.nPopulate > 0 || sink.nStrux > 0);

	// whole-document tellListener
	CountingListener all;
	TFPASS(d.doc->tellListener(&all));
	TFPASS(all.nStrux > 0);

	// listener registration lifecycle; ids are recycled vector
	// indices so the first registration legitimately returns 0
	CountingListener reg;
	PL_ListenerId lid = 0;
	TFPASS(d.doc->addListener(&reg, &lid));
	CountingListener reg2;
	PL_ListenerId lid2 = 0;
	TFPASS(d.doc->addListener(&reg2, &lid2));
	TFPASS(lid2 != lid);
	TFPASS(d.doc->removeListener(lid2));
	TFPASS(reg.nStrux > 0);   // registration populates the whole doc
	TFPASS(d.doc->removeListener(lid));

	// misc query APIs
	TFPASS(!d.doc->isConnected());
	d.doc->removeCaret("nonexistent-caret");
	TFPASS(d.doc->getNumAuthors() == 0);
	TFPASS(d.doc->getNthAuthor(0) == nullptr);
	TFPASS(d.doc->getLastAuthorInt() == -1);
	TFPASS(d.doc->getAllViews().empty());
	d.doc->setShowAuthors(true);
	d.doc->setShowAuthors(false);

	// annotation props are an unimplemented stub that always reports
	// the same canned value
	d.doc->setAnnotationProp("k", "v");
	std::string outProp;
	TFPASS(d.doc->getAnnotationProp("k", outProp));
	TFPASS(outProp == "Dummy value");

	UT_UTF8String inches;
	TFPASS(d.doc->convertPercentToInches("50%", inches));

	UT_uint32 diffpos = 0;
	TFPASS(d.doc->areDocumentFormatsEqual(*d.doc, diffpos));
	TFPASS(d.doc->areDocumentStylesheetsEqual(*d.doc));
	PT_DocPosition dpos = 0;
	UT_sint32 off2 = 0;
	TFPASS(!d.doc->findFirstDifferenceInContent(dpos, off2, *d.doc));
	UT_uint32 knownLen = 0;
	// identical docs trivially "resume" similarity at the start
	TFPASS(d.doc->findWhereSimilarityResumes(dpos, off2, knownLen, *d.doc));

	// document property change records; the dispatcher keys off the
	// "docprop" attribute
	TFPASS(d.doc->createAndSendDocPropCR({"meta:k1", "v1"}, PP_NOPROPS));
	TFPASS(d.doc->changeDocPropeties({"docprop", "metadata"},
									 {"meta:k2", "v2"}));

	// frag/xid version lookup
	const pf_Frag * pf = nullptr;
	PT_BlockOffset fo = 0;
	TFPASS(d.doc->getPieceTable()->getFragFromPosition(
		blockText(d.doc, b1), &pf, &fo));
	TFPASS(d.doc->getFragXIDforVersion(pf, 0) >= 0);

	// signal broadcast + deferred notifications are safe with no listeners
	TFPASS(d.doc->signalListeners(0));
	d.doc->deferNotifications();
	d.doc->processDeferredNotifications();
}

TFTEST_MAIN("undo/redo leave the piece table editable")
{
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b1 = nullptr;
	TFPASS(d.para("keep going", PP_NOPROPS, &b1));
	d.finish();

	const PT_DocPosition s = blockText(d.doc, b1) + 2;

	/* a second fmtmark at the same spot folds into a ChangeFmtMark
	 * record; undoing it replays PXT_ChangeFmtMark through _doTheDo.
	 * m_bDoingTheDo must be reset on every exit path — a leak leaves
	 * isDoingTheDo() stuck and every PD_Document edit entry point
	 * refuses work from then on. */
	PP_AttrProp ap1, ap2;
	ap1.setProperty("font-style", "italic");
	ap2.setProperty("font-weight", "bold");
	TFPASS(d.doc->insertFmtMark(PTC_AddFmt, s, &ap1));
	TFPASS(d.doc->insertFmtMark(PTC_AddFmt, s, &ap2));
	TFPASS(d.doc->undoCmd(1));
	TFPASS(!d.doc->isDoingTheDo());
	UT_UCS4String more("!");
	TFPASS(d.doc->insertSpan(s, more.ucs4_str(), 1));
	TFPASS(editDocText(d.doc) == "ke!ep going");

	/* undoing a block-strux delete replays PXT_InsertStrux — the
	 * other exit that used to leak the flag */
	pf_Frag_Strux *b2 = nullptr;
	TFPASS(d.doc->insertStrux(s, PTX_Block, &b2));
	TFPASS(b2 != nullptr);
	TFPASS(d.doc->deleteStrux(d.doc->getStruxPosition(b2),
							  PTX_Block, true));
	TFPASS(d.doc->undoCmd(1));
	TFPASS(!d.doc->isDoingTheDo());
	TFPASS(d.doc->insertSpan(blockText(d.doc, b1),
							 more.ucs4_str(), 1));
	TFPASS(!d.doc->isDoingTheDo());
}

TFTEST_MAIN("annotation strux pairing and record position clamp")
{
	EditDoc d;
	TFPASS(d.build());
	TFPASS(d.para("aa"));
	d.finish();
	pt_PieceTable * pt = d.doc->getPieceTable();

	/* unattached frags are enough: these queries only look at the
	 * strux type, never at tree membership */
	pf_Frag_Strux_SectionAnnotation annBeg(pt, 0);
	pf_Frag_Strux_SectionEndAnnotation annEnd(pt, 0);
	TFPASS(annBeg.isMatchingType(PTX_EndAnnotation));
	TFPASS(annEnd.isMatchingType(PTX_SectionAnnotation));
	TFPASS(!annBeg.isMatchingType(PTX_Block));
	TFPASS(!annEnd.isMatchingType(PTX_EndMarginnote));
	/* every begin-section strux participates in the fixMissingXIDs
	 * safety net used by exporters */
	TFPASS(annBeg.usesXID());
	TFPASS(!annEnd.usesXID());

	/* getPosition must saturate instead of wrapping when the collab
	 * adjustment would take the position out of range */
	PX_ChangeRecord crPos(PX_ChangeRecord::PXT_ChangePoint, 100, 0, 0);
	crPos.setAdjustment(-200);
	TFPASS(crPos.getPosition() == 0);
	crPos.setAdjustment(0);
	TFPASS(crPos.getPosition() == 100);
}

TFTEST_MAIN("strux downcast on unattached frag and ctor types")
{
	EditDoc d;
	TFPASS(d.build());
	TFPASS(d.para("aa"));
	d.finish();
	pt_PieceTable * pt = d.doc->getPieceTable();

	/* a frag that was never linked into the tree has no node;
	 * tryDownCastStrux must answer nullptr instead of dereferencing
	 * the null iterator value */
	pf_Frag_Strux_SectionAnnotation loose(pt, 0);
	TFPASS(loose.tryDownCastStrux(PTX_SectionAnnotation) == nullptr);
	TFPASS(tryDownCastStrux(&loose, PTX_SectionAnnotation) == nullptr);

	/* on an attached strux the same query resolves the frag itself,
	 * and a wrong type still misses */
	pf_Frag * first = pt->getFragments().getFirst();
	TFPASS(first != nullptr);
	TFPASS(first->tryDownCastStrux(PTX_Section) ==
		   static_cast<pf_Frag_Strux *>(first));
	TFPASS(first->tryDownCastStrux(PTX_Block) == nullptr);

	/* the section-strux constructors used to pass a copy-pasted
	 * PTX_SectionHdrFtr to the base and patch m_struxType afterwards —
	 * getStruxType must report the real type from the start */
	pf_Frag_Strux_SectionMarginnote mn(pt, 0);
	TFPASS(mn.getStruxType() == PTX_SectionMarginnote);
	pf_Frag_Strux_SectionFrame fr(pt, 0);
	TFPASS(fr.getStruxType() == PTX_SectionFrame);
	pf_Frag_Strux_SectionEndCell ec(pt, 0);
	TFPASS(ec.getStruxType() == PTX_EndCell);
	pf_Frag_Strux_SectionTOC toc(pt, 0);
	TFPASS(toc.getStruxType() == PTX_SectionTOC);
}

TFTEST_MAIN("unbalanced glob end and degenerate span record")
{
	EditDoc d;
	TFPASS(d.build());
	TFPASS(d.para("aa"));
	d.finish();

	/* endUserAtomicGlob with no matching begin used to wrap the
	 * unsigned nesting counter to UINT32_MAX; after that no begin/end
	 * pair could ever reach zero again, so the next balanced pair
	 * silently lost its end marker and every later edit globbed into
	 * one giant undo unit */
	const UT_uint32 n0 = d.doc->undoCount(true);
	d.doc->endUserAtomicGlob();		/* unbalanced: must be a no-op */
	TFPASS(d.doc->undoCount(true) == n0);
	d.doc->beginUserAtomicGlob();
	d.doc->endUserAtomicGlob();		/* balanced: start + end markers */
	TFPASS(d.doc->undoCount(true) == n0 + 2);

	/* a zero-length span record is bogus but must still come out of
	 * the ctor fully initialised — the early return used to skip the
	 * member assignments entirely */
	PX_ChangeRecord_Span crZero(PX_ChangeRecord::PXT_InsertSpan,
							  7, 0, 3, 0, 0, nullptr);
	TFPASS(crZero.getLength() == 0);
	TFPASS(crZero.getBufIndex() == 3);
	TFPASS(crZero.getField() == nullptr);
}

namespace {

/* run fn with MaxUndoOps clamped low, restoring the pref afterwards
 * (TFPASS doesn't abort, so the restore has to be unconditional) */
struct ScopedUndoCap
{
	ScopedUndoCap(int limit)
	{
		pPrefs = XAP_App::getApp() ? XAP_App::getApp()->getPrefs() : nullptr;
		if (pPrefs)
		{
			pPrefs->getPrefsValueInt(XAP_PREF_KEY_MaxUndoOps, oldLimit);
			pPrefs->getCurrentScheme(true)->setValueInt(
				XAP_PREF_KEY_MaxUndoOps, limit);
		}
	}
	~ScopedUndoCap()
	{
		if (pPrefs)
			pPrefs->getCurrentScheme(true)->setValueInt(
				XAP_PREF_KEY_MaxUndoOps, oldLimit);
	}
	XAP_Prefs *pPrefs = nullptr;
	int oldLimit = 200;
};

} // namespace

TFTEST_MAIN("undo history cap prunes whole ops at the boundary")
{
	ScopedUndoCap cap(4);
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b = nullptr;
	TFPASS(d.para("cap", PP_NOPROPS, &b));
	d.finish();

	px_ChangeHistory * hist = d.doc->getPieceTable()->getChangeHistory();
	TFPASS(hist != nullptr);
	TFPASS(hist->getUndoDepthLimit() == 4);

	/* seven single-char inserts at the same spot — repeated inserts
	 * at a fixed position can't coalesce (that needs appending at the
	 * tail of the previous record), so each is its own op */
	const PT_DocPosition s = blockText(d.doc, b);
	const UT_UCS4String ins("x");
	for (int i = 0; i < 7; i++)
		TFPASS(d.doc->insertSpan(s, ins.ucs4_str(), 1));
	TFPASS(editDocText(d.doc) == "xxxxxxxcap");

	/* the history kept only the newest 4 ops — one record apiece */
	TFPASS(hist->getOpCount() == 4);
	TFPASS(hist->getRecordCount() == 4);
	TFPASS(d.doc->undoCount(true) == 4);

	/* undo stops exactly at the cap boundary and the pruned inserts
	 * stay put — the doc is left sane, not corrupted */
	for (int i = 0; i < 4; i++)
		TFPASS(d.doc->undoCmd(1));
	TFPASS(!d.doc->canDo(true));
	TFPASS(editDocText(d.doc) == "xxxcap");

	/* a fresh edit invalidates the redo tail and re-counts ops */
	TFPASS(d.doc->insertSpan(s, ins.ucs4_str(), 1));
	TFPASS(hist->getOpCount() == 1);
	TFPASS(editDocText(d.doc) == "xxxxcap");
}

TFTEST_MAIN("undo cap drops whole globs atomically")
{
	ScopedUndoCap cap(3);
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b = nullptr;
	TFPASS(d.para("keep", PP_NOPROPS, &b));
	d.finish();

	px_ChangeHistory * hist = d.doc->getPieceTable()->getChangeHistory();
	const PT_DocPosition s = blockText(d.doc, b);
	const UT_UCS4String ins("q");

	/* op1: lone insert (will be pruned), op2: 2-insert glob,
	 * ops 3-4: lone inserts — 4 ops > cap 3, so op1's record goes */
	TFPASS(d.doc->insertSpan(s, ins.ucs4_str(), 1));
	d.doc->beginUserAtomicGlob();
	TFPASS(d.doc->insertSpan(s, ins.ucs4_str(), 1));
	TFPASS(d.doc->insertSpan(s, ins.ucs4_str(), 1));
	d.doc->endUserAtomicGlob();
	TFPASS(d.doc->insertSpan(s, ins.ucs4_str(), 1));
	TFPASS(d.doc->insertSpan(s, ins.ucs4_str(), 1));
	TFPASS(editDocText(d.doc) == "qqqqqkeep");

	/* kept: glob op (4 records) + 2 lone records = 3 ops, 6 records */
	TFPASS(hist->getOpCount() == 3);
	TFPASS(hist->getRecordCount() == 6);

	/* three undos: lone, lone, then the glob's two inserts in one
	 * step — pruning can never leave undo able to stop mid-glob */
	TFPASS(d.doc->undoCmd(1));
	TFPASS(editDocText(d.doc) == "qqqqkeep");
	TFPASS(d.doc->undoCmd(1));
	TFPASS(editDocText(d.doc) == "qqqkeep");
	TFPASS(d.doc->undoCmd(1));
	TFPASS(editDocText(d.doc) == "qkeep");
	TFPASS(!d.doc->canDo(true));

	/* redo replays the surviving ops cleanly */
	TFPASS(d.doc->redoCmd(1));
	TFPASS(d.doc->redoCmd(1));
	TFPASS(d.doc->redoCmd(1));
	TFPASS(editDocText(d.doc) == "qqqqqkeep");
	TFPASS(!d.doc->canDo(false));
}

TFTEST_MAIN("MaxUndoOps=0 leaves the history unbounded")
{
	ScopedUndoCap cap(0);
	EditDoc d;
	TFPASS(d.build());
	pf_Frag_Strux *b = nullptr;
	TFPASS(d.para("free", PP_NOPROPS, &b));
	d.finish();

	px_ChangeHistory * hist = d.doc->getPieceTable()->getChangeHistory();
	TFPASS(hist->getUndoDepthLimit() == 0);

	const PT_DocPosition s = blockText(d.doc, b);
	const UT_UCS4String ins("y");

	/* the first insert into a fresh paragraph also emits a fmtmark
	 * fixup op and a multistep glob — absorb it before baselining so
	 * the delta below is exactly the plain inserts */
	TFPASS(d.doc->insertSpan(s, ins.ucs4_str(), 1));
	const UT_sint32 n0 = hist->getOpCount();
	TFPASS(n0 > 0);

	for (int i = 0; i < 6; i++)
		TFPASS(d.doc->insertSpan(s, ins.ucs4_str(), 1));

	/* no pruning: every op is still there and undoable */
	TFPASS(hist->getOpCount() == n0 + 6);
	for (int i = 0; i < 6; i++)
		TFPASS(d.doc->undoCmd(1));
	TFPASS(editDocText(d.doc) == "yfree");
	TFPASS(hist->getOpCount() == n0 + 6);
}
