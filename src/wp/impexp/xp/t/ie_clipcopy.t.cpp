/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
 * Copyright (C) 2025-2026 Abinova contributors
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
 * Lazy clipboard copy: AP_UnixApp::copyToClipboard freezes the copied
 * range into a scratch document via IE_Exp_DocRangeListener and
 * AP_UnixClipboard::_materializeData exports rtf/xhtml/html4/odt/text
 * from that snapshot only when a paste actually asks for them.  These
 * tests drive the same machinery directly: splice a range into a
 * snapshot, then verify the materialized exports reflect the document
 * as it was at copy time - even after the source has been edited.
 */

#include "tf_test.h"
#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "pf_Fragments.h"
#include "ie_exp_DocRangeListener.h"
#include "pl_ListenerCoupleCloser.h"
#include "ie_exp_RTF.h"
#include "ie_exp_HTML.h"
#include "ie_exp_Text.h"
#include "ut_bytebuf.h"
#include "ut_string_class.h"

#include <string>

#define TFSUITE "core.wp.impexp.clipcopy"

static bool clip_append_text(pt_PieceTable * pt, const char * utf8)
{
	UT_UCS4String s(utf8);
	return pt->appendSpan(s.ucs4_str(), static_cast<UT_uint32>(s.length()));
}

/* the same splice AP_UnixApp::copyToClipboard uses to freeze the
 * copied range into a standalone snapshot document */
static PD_Document * snapshot_range(PD_DocumentRange * dr)
{
	PD_Document * snap = new PD_Document;
	if (snap->createRawDocument() != UT_OK)
	{
		snap->unref();
		return nullptr;
	}
	IE_Exp_DocRangeListener listener(dr, snap);
	{
		PL_ListenerCoupleCloser closer;
		if (!dr->m_pDoc->tellListenerSubset(&listener, dr, &closer))
		{
			snap->unref();
			return nullptr;
		}
	}
	snap->finishRawCreation();
	return snap;
}

static PD_DocumentRange whole_doc_range(PD_Document * doc)
{
	PT_DocPosition posBOD = 0;
	PT_DocPosition posEOD = 0;
	doc->getBounds(false, posBOD);
	doc->getBounds(true, posEOD);
	return PD_DocumentRange(doc, posBOD, posEOD);
}

static std::string buf_str(const UT_ByteBuf & buf)
{
	return std::string(reinterpret_cast<const char *>(buf.getPointer(0)),
					   buf.getLength());
}

/* what AP_UnixClipboard::_materializeData produces for each mime tag */
static std::string materialize_rtf(PD_Document * snap)
{
	PD_DocumentRange dr = whole_doc_range(snap);
	IE_Exp_RTF exp(snap);
	UT_ByteBuf buf;
	exp.copyToBuffer(&dr, &buf);
	return buf_str(buf);
}

static std::string materialize_html(PD_Document * snap, bool html4)
{
	PD_DocumentRange dr = whole_doc_range(snap);
	IE_Exp_HTML exp(snap);
	exp.set_HTML4(html4);
	UT_ByteBuf buf;
	exp.copyToBuffer(&dr, &buf);
	return buf_str(buf);
}

static std::string materialize_text(PD_Document * snap)
{
	PD_DocumentRange dr = whole_doc_range(snap);
	IE_Exp_Text exp(snap, "UTF-8");
	UT_ByteBuf buf;
	exp.copyToBuffer(&dr, &buf);
	return buf_str(buf);
}

static PD_Document * make_two_para_doc(pf_Frag_Strux ** blk2 = nullptr)
{
	PD_Document * doc = new PD_Document;
	if (doc->createRawDocument() != UT_OK)
	{
		doc->unref();
		return nullptr;
	}
	pt_PieceTable * pt = doc->getPieceTable();
	bool ok = pt->appendStrux(PTX_Section, PP_NOPROPS)
		&& pt->appendStrux(PTX_Block, PP_NOPROPS)
		&& clip_append_text(pt, "AAA")
		&& pt->appendStrux(PTX_Block, PP_NOPROPS, blk2)
		&& clip_append_text(pt, "BBB");
	if (!ok)
	{
		doc->unref();
		return nullptr;
	}
	doc->finishRawCreation();
	return doc;
}

TFTEST_MAIN("copy snapshot freezes content against later edits")
{
	PD_Document * src = make_two_para_doc();
	TFPASS(src != nullptr);

	PD_DocumentRange dr = whole_doc_range(src);
	PD_Document * snap = snapshot_range(&dr);
	TFPASS(snap != nullptr);

	/* edit the source after the copy: the clipboard content must not
	 * follow the document */
	PT_DocPosition posEOD = 0;
	TFPASS(src->getBounds(true, posEOD));
	UT_UCS4String zzz("ZZZ");
	TFPASS(src->getPieceTable()->insertSpan(posEOD, zzz.ucs4_str(),
										  static_cast<UT_uint32>(zzz.length())));

	std::string rtf = materialize_rtf(snap);
	TFPASS(rtf.find("AAA") != std::string::npos);
	TFPASS(rtf.find("BBB") != std::string::npos);
	TFPASS(rtf.find("ZZZ") == std::string::npos);

	std::string xhtml = materialize_html(snap, false);
	TFPASS(xhtml.find("AAA") != std::string::npos);
	TFPASS(xhtml.find("BBB") != std::string::npos);
	TFPASS(xhtml.find("ZZZ") == std::string::npos);

	std::string html4 = materialize_html(snap, true);
	TFPASS(html4.find("AAA") != std::string::npos);
	TFPASS(html4.find("BBB") != std::string::npos);
	TFPASS(html4.find("ZZZ") == std::string::npos);

	std::string text = materialize_text(snap);
	TFPASS(text.find("AAA") != std::string::npos);
	TFPASS(text.find("BBB") != std::string::npos);
	TFPASS(text.find("ZZZ") == std::string::npos);

	/* the mutation really did land on the source */
	TFPASS(materialize_text(src).find("ZZZ") != std::string::npos);

	snap->unref();
	src->unref();
}

TFTEST_MAIN("copy snapshot contains only the copied range")
{
	pf_Frag_Strux * blk2 = nullptr;
	PD_Document * src = make_two_para_doc(&blk2);
	TFPASS(src != nullptr);
	TFPASS(blk2 != nullptr);

	/* copy only the second block */
	PT_DocPosition posEOD = 0;
	TFPASS(src->getBounds(true, posEOD));
	PD_DocumentRange dr(src, blk2->getPos(), posEOD);
	PD_Document * snap = snapshot_range(&dr);
	TFPASS(snap != nullptr);

	std::string text = materialize_text(snap);
	TFPASS(text.find("BBB") != std::string::npos);
	TFPASS(text.find("AAA") == std::string::npos);

	snap->unref();
	src->unref();
}
