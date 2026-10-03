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
 * Paste-options smart tag (Word "(Ctrl)" button): cmdPaste arms the
 * view-side state with the pasted range; edits, caret moves, scrolls
 * and new pastes disarm it; applyPasteTagOption swaps the pasted
 * range for a different clipboard flavour inside a single undo atom.
 * The headless view has no frame, so the GtkOverlay widget is a
 * no-op and only the document-side state machine is exercised here.
 * The tests claim the real Gdk clipboard so cmdPaste reads the
 * fake-clipboard data - it is cleared again afterwards.
 */

#include "tf_test.h"

#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "fl_DocLayout.h"
#include "fv_View.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_UnixClipboard.h"
#include "ut_growbuf.h"

#include <string>

#define TFSUITE "core.text.fmt.pastetag"

namespace {

/* A document with a full interactive layout stack on a widget-less
 * GR_UnixCairoGraphics - same pattern as fv_ViewModes.t.cpp. */
struct PasteTagView
{
	PasteTagView() = default;
	PasteTagView(const PasteTagView &) = delete;
	PasteTagView &operator=(const PasteTagView &) = delete;

	bool load()
	{
		doc = new PD_Document;
		if (doc->createRawDocument() != UT_OK)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		pt_PieceTable * pt = doc->getPieceTable();
		UT_UCS4String s("preexisting text");
		bool ok = pt->appendStrux(PTX_Section, PP_NOPROPS)
			&& pt->appendStrux(PTX_Block, PP_NOPROPS)
			&& pt->appendSpan(s.ucs4_str(),
							  static_cast<UT_uint32>(s.length()));
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

	PT_DocPosition eod() const
	{
		PT_DocPosition pos = 0;
		doc->getBounds(true, pos);
		return pos;
	}

	~PasteTagView()
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

/* put data on the same-process fake clipboard and claim the real
 * Gdk clipboard when a display exists so getData() reads it back
 * synchronously; returns false when the test app has no clipboard */
static bool claim_clipboard(const char * mime, const void * data,
							UT_sint32 len)
{
	XAP_UnixClipboard * clip =
		static_cast<XAP_UnixApp*>(XAP_App::getApp())->getClipboard();
	if (!clip)
		return false;
	clip->clearData(true, false);
	if (!clip->addData(XAP_UnixClipboard::TAG_ClipboardOnly, mime,
					   data, len))
		return false;
	clip->finishedAddingData();
	return true;
}

static void release_clipboard()
{
	XAP_UnixClipboard * clip =
		static_cast<XAP_UnixApp*>(XAP_App::getApp())->getClipboard();
	if (clip)
		clip->clearData(true, false);
}

static std::string doc_text(FV_View * v)
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

TFTEST_MAIN("paste tag arms on cmdPaste and disarms on edit, motion, scroll")
{
	PasteTagView hv;
	TFPASS(hv.load());
	FV_View * v = hv.view;
	if (!v)
		return;

	static const char txt[] = "ZZZ";
	TFPASS(claim_clipboard("text/plain", txt, sizeof(txt) - 1));

	/* paste at end of doc arms the tag */
	v->setPoint(hv.eod());
	v->cmdPaste();
	TFPASS(v->hasPasteTag());
	TFPASS(doc_text(v).find("ZZZ") != std::string::npos);

	/* caret move off the pasted range disarms it */
	v->cmdCharMotion(false, 2);
	TFPASS(!v->hasPasteTag());

	/* a new paste re-arms */
	v->setPoint(hv.eod());
	v->cmdPaste();
	TFPASS(v->hasPasteTag());

	/* an edit disarms */
	UT_UCS4Char x('x');
	v->cmdCharInsert(&x, 1);
	TFPASS(!v->hasPasteTag());

	/* paste-special never arms */
	v->setPoint(hv.eod());
	v->cmdPasteAs("text/plain");
	TFPASS(!v->hasPasteTag());

	/* scroll disarms */
	v->setPoint(hv.eod());
	v->cmdPaste();
	TFPASS(v->hasPasteTag());
	v->setXScrollOffset(v->getXScrollOffset() + 40);
	TFPASS(!v->hasPasteTag());

	release_clipboard();
}

TFTEST_MAIN("tag options replace the pasted range inside one undo atom")
{
	PasteTagView hv;
	TFPASS(hv.load());
	FV_View * v = hv.view;
	if (!v)
		return;

	static const char txt[] = "QQQ";
	TFPASS(claim_clipboard("text/plain", txt, sizeof(txt) - 1));

	v->setPoint(hv.eod());
	v->cmdPaste();
	TFPASS(v->hasPasteTag());
	const std::string pasted = doc_text(v);
	TFPASS(pasted.find("QQQ") != std::string::npos);

	/* Keep Text Only swaps the range; content identical here since
	 * the source only offered plain text */
	v->applyPasteTagOption(FV_View::PASTETAG_TEXT_ONLY);
	TFPASS(!v->hasPasteTag());
	TFPASS(doc_text(v).find("QQQ") != std::string::npos);

	/* one undo reverts the whole select->delete->re-paste: the
	 * original paste comes straight back, proving the substitution
	 * was a single atom rather than two steps */
	v->cmdUndo(1);
	TFPASS(doc_text(v) == pasted);

	/* the next undo removes the paste itself */
	v->cmdUndo(1);
	TFPASS(doc_text(v).find("QQQ") == std::string::npos);

	release_clipboard();
}
