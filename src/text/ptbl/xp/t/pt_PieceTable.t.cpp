/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiWord
 * Copyright (C) 2011 Hub Figuiere
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


#include "tf_test.h"
#include "pt_PieceTable.h"
#include "pd_Document.h"
#include "pf_Frag_Strux_Block.h"
#include "pp_Property.h"
#include "ut_string_class.h"

#define TFSUITE "core.text.ptbl.piecetable"

// FIXME write real test
TFTEST_MAIN("pt_PieceTable")
{
//	PD_Document doc;
	pt_PieceTable pt(nullptr);

	TFPASS(pt.getDocument() == nullptr);

	// we need to set the state to loading.
	pt.setPieceTableState(PTS_Loading);

	const PP_PropertyVector attrs = {
		"foo", "bar"
	};

	pf_Frag_Strux *frag = nullptr;
	TFPASS(pt.appendStrux(PTX_Block, attrs, &frag));
	TFPASS(frag);
	TFPASS(frag->getType() == pf_Frag::PFT_Strux);

	TFPASS(pt.appendFmtMark());
}

// m_embeddedStrux pairs begin/end note strux pointers. Deleting a note
// strux via the no-change-record paths must drop the pair, otherwise
// isInsideFootnote() dereferences a dangling pf_Frag_Strux*.
TFTEST_MAIN("pt_PieceTable_embeddedStrux")
{
	const PP_PropertyVector attrs = {
		"foo", "bar"
	};

	// Deleting the end note must drop the pair.
	{
		pt_PieceTable pt(nullptr);
		pt.setPieceTableState(PTS_Loading);

		pf_Frag_Strux *begin = nullptr;
		pf_Frag_Strux *inside = nullptr;
		pf_Frag_Strux *end = nullptr;
		TFPASS(pt.appendStrux(PTX_Block, attrs));
		TFPASS(pt.appendStrux(PTX_SectionFootnote, attrs, &begin));
		TFPASS(pt.appendStrux(PTX_Block, attrs, &inside));
		TFPASS(pt.appendStrux(PTX_EndFootnote, attrs, &end));
		TFPASS(begin);
		TFPASS(inside);
		TFPASS(end);

		const pf_Frag *pfBegin = nullptr;
		PT_DocPosition posInside = inside->getPos();
		TFPASS(pt.isInsideFootnote(posInside, &pfBegin));
		TFPASS(pfBegin == begin);

		pt.deleteStruxNoUpdate(end);
		TFPASS(!pt.isInsideFootnote(posInside));
	}

	// Deleting the begin note must drop the pair as well.
	{
		pt_PieceTable pt(nullptr);
		pt.setPieceTableState(PTS_Loading);

		pf_Frag_Strux *begin = nullptr;
		pf_Frag_Strux *inside = nullptr;
		TFPASS(pt.appendStrux(PTX_Block, attrs));
		TFPASS(pt.appendStrux(PTX_SectionEndnote, attrs, &begin));
		TFPASS(pt.appendStrux(PTX_Block, attrs, &inside));
		TFPASS(pt.appendStrux(PTX_EndEndnote, attrs));
		TFPASS(begin);
		TFPASS(inside);

		PT_DocPosition posInside = inside->getPos();
		TFPASS(pt.isInsideFootnote(posInside));

		pt.deleteStruxNoUpdate(begin);
		TFPASS(!pt.isInsideFootnote(posInside));
	}
}

// FRZ02: getContentSerial()/getLastContentChange() back the autosave
// "unchanged since last backup" skip in XAP_Frame::autosaveTick — the
// serial must move on every content mutation (plain insert, coalesced
// typing, undo, redo) and the change time must be stamped, or the tick
// would either skip a genuinely changed doc (lost recovery window) or
// never skip an unchanged one (the periodic stall FRZ02 removes).
TFTEST_MAIN("PD_Document content serial tracks mutations")
{
	PD_Document *doc = new PD_Document;
	TFPASS(doc->createRawDocument() == UT_OK);
	pf_Frag_Strux *blk = nullptr;
	TFPASS(doc->appendStrux(PTX_Block, PP_NOPROPS, &blk));
	const UT_UCS4String seed("seed");
	TFPASS(doc->appendSpan(seed.ucs4_str(), seed.length()));
	doc->finishRawCreation();
	TFPASS(!doc->isPieceTableChanging());

	const UT_sint64 s0 = doc->getContentSerial();
	const time_t t0 = doc->getLastContentChange();
	TFPASS(t0 <= time(nullptr));

	/* a fresh edit bumps the serial and stamps the change time */
	const UT_UCS4String ins("x");
	const PT_DocPosition pos = doc->getStruxPosition(blk) + 1;
	TFPASS(doc->insertSpan(pos, ins.ucs4_str(), 1));
	const UT_sint64 s1 = doc->getContentSerial();
	TFPASS(s1 != s0);
	TFPASS(doc->getLastContentChange() >= t0);
	TFPASS(doc->getLastContentChange() <= time(nullptr));

	/* a second adjacent insert — whether it lands via addChangeRecord
	 * or coalesceHistory, the serial must still move */
	TFPASS(doc->insertSpan(pos + 1, ins.ucs4_str(), 1));
	const UT_sint64 s2 = doc->getContentSerial();
	TFPASS(s2 != s1);
	TFPASS(s2 != s0);

	/* undo and redo are mutations too */
	TFPASS(doc->undoCmd(1));
	const UT_sint64 s3 = doc->getContentSerial();
	TFPASS(s3 != s2);
	TFPASS(doc->redoCmd(1));
	TFPASS(doc->getContentSerial() != s3);

	doc->unref();
}
