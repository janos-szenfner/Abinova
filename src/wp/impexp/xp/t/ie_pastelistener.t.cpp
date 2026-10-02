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
 * IE_Imp_PasteListener splices the fragments of a scratch document into
 * a live document during paste.  These tests drive it directly with a
 * hand-built source document (and once end-to-end through the XHTML
 * importer's pasteFromBuffer) and check that the content lands.
 */

#include "tf_test.h"
#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "pf_Fragments.h"
#include "pf_Frag_Strux_Section.h"
#include "ie_imp.h"
#include "ie_exp.h"
#include "ie_imp_PasteListener.h"
#include "ie_imp_XHTML.h"
#include "ut_string_class.h"

#include <gsf/gsf-output-memory.h>
#include <cstring>
#include <string>

#define TFSUITE "core.wp.impexp.pastelistener"

static std::string export_to_string(PD_Document * doc, const char * suffix)
{
	std::string out;
	GsfOutput * mem = gsf_output_memory_new();
	if (mem &&
		doc->saveAs(mem, static_cast<int>(IE_Exp::fileTypeForSuffix(suffix)),
					false, nullptr) == UT_OK)
	{
		gsf_output_close(mem);
		const guint8 * bytes =
			gsf_output_memory_get_bytes(GSF_OUTPUT_MEMORY(mem));
		gsize sz = gsf_output_size(mem);
		if (bytes)
			out.assign(reinterpret_cast<const char *>(bytes), sz);
	}
	if (mem)
		g_object_unref(mem);
	return out;
}

static bool append_text(pt_PieceTable * pt, const char * utf8)
{
	UT_UCS4String s(utf8);
	return pt->appendSpan(s.ucs4_str(), static_cast<UT_uint32>(s.length()));
}

/* A source document shaped like real paste buffers: a paragraph
 * containing a footnote and an annotation section embedded mid-block
 * (that's where note bodies live in the frag stream), a table, and a
 * trailing paragraph. */
static PD_Document * make_source_doc()
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
		&& append_text(pt, "AAA ")
		&& pt->appendStrux(PTX_SectionFootnote,
						   PP_PropertyVector{"footnote-id", "7"})
		&& pt->appendStrux(PTX_Block, PP_NOPROPS)
		&& append_text(pt, "fnbody")
		&& pt->appendStrux(PTX_EndFootnote, PP_NOPROPS)
		&& append_text(pt, " BBB ")
		&& pt->appendStrux(PTX_SectionAnnotation,
						   PP_PropertyVector{"annotation-id", "3"})
		&& pt->appendStrux(PTX_Block, PP_NOPROPS)
		&& append_text(pt, "annbody")
		&& pt->appendStrux(PTX_EndAnnotation, PP_NOPROPS)
		&& append_text(pt, " CCC")
		&& pt->appendStrux(PTX_Block, PP_NOPROPS)
		&& append_text(pt, "secondpara");
	if (!ok)
	{
		doc->unref();
		return nullptr;
	}
	doc->finishRawCreation();
	return doc;
}

TFTEST_MAIN("paste listener splices text, footnote, annotation")
{
	PD_Document * src = make_source_doc();
	TFPASS(src != nullptr);
	PD_Document * dst = new PD_Document;
	TFPASSEQ(dst->newDocument(), UT_OK);

	/* paste at end of the (empty) target document */
	PT_DocPosition posEOD = 0;
	TFPASS(dst->getBounds(true, posEOD));
	{
		IE_Imp_PasteListener listener(dst, posEOD, src);
		TFPASS(src->tellListener(&listener));
	}

	std::string xml = export_to_string(dst, ".abwn");
	TFPASS(xml.find("AAA") != std::string::npos);
	TFPASS(xml.find("fnbody") != std::string::npos);
	TFPASS(xml.find("<foot") != std::string::npos);
	TFPASS(xml.find("BBB") != std::string::npos);
	TFPASS(xml.find("annbody") != std::string::npos);
	TFPASS(xml.find("<annotate") != std::string::npos);
	TFPASS(xml.find("secondpara") != std::string::npos);

	dst->unref();
	src->unref();
}

/* PTX_SectionMarginnote/PTX_EndMarginnote have no _createStrux case, so
 * the paste listener must skip them instead of inserting an arbitrary
 * strux (and must not advance its insertion point).  They cannot be
 * built through appendStrux either, so splice the frags in by hand. */
static PD_Document * make_marginnote_source_doc()
{
	PD_Document * doc = new PD_Document;
	if (doc->createRawDocument() != UT_OK)
	{
		doc->unref();
		return nullptr;
	}
	pt_PieceTable * pt = doc->getPieceTable();
	pf_Frag_Strux * blk = nullptr;
	bool ok = pt->appendStrux(PTX_Section, PP_NOPROPS)
		&& pt->appendStrux(PTX_Block, PP_NOPROPS, &blk)
		&& append_text(pt, "AAA ");
	if (!ok || !blk)
	{
		doc->unref();
		return nullptr;
	}
	PT_AttrPropIndex api = blk->getIndexAP();
	pt->getFragments().appendFrag(
		new pf_Frag_Strux_SectionMarginnote(pt, api));
	ok = pt->appendStrux(PTX_Block, PP_NOPROPS)
		&& append_text(pt, "marginbody");
	pt->getFragments().appendFrag(
		new pf_Frag_Strux_SectionEndMarginnote(pt, api));
	ok = ok && append_text(pt, " BBB");
	if (!ok)
	{
		doc->unref();
		return nullptr;
	}
	doc->finishRawCreation();
	return doc;
}

TFTEST_MAIN("unsupported strux is skipped, content still pastes")
{
	PD_Document * src = make_marginnote_source_doc();
	TFPASS(src != nullptr);
	PD_Document * dst = new PD_Document;
	TFPASSEQ(dst->newDocument(), UT_OK);

	PT_DocPosition posEOD = 0;
	TFPASS(dst->getBounds(true, posEOD));
	{
		IE_Imp_PasteListener listener(dst, posEOD, src);
		TFPASS(src->tellListener(&listener));
	}

	std::string xml = export_to_string(dst, ".abwn");
	/* the note body text degrades to normal body text; no margin strux */
	TFPASS(xml.find("marginbody") != std::string::npos);
	TFPASS(xml.find("BBB") != std::string::npos);
	TFPASS(xml.find("<margin") == std::string::npos);

	dst->unref();
	src->unref();
}

/* the real paste path: XHTML buffer -> scratch doc -> paste listener */
TFTEST_MAIN("xhtml pasteFromBuffer splices content into live doc")
{
	PD_Document * doc = new PD_Document;
	TFPASSEQ(doc->newDocument(), UT_OK);

	PT_DocPosition pos = 0;
	TFPASS(doc->getBounds(true, pos));

	PD_DocumentRange dr(doc, pos, pos);
	IE_Imp_XHTML imp(doc);
	const char * xhtml =
		"<html><body><p>Hello <b>bold</b> tail</p>"
		"<p>second</p></body></html>";
	TFPASS(imp.pasteFromBuffer(&dr,
							 reinterpret_cast<const unsigned char *>(xhtml),
							 static_cast<UT_uint32>(strlen(xhtml))));

	std::string xml = export_to_string(doc, ".abwn");
	TFPASS(xml.find("Hello") != std::string::npos);
	TFPASS(xml.find("bold") != std::string::npos);
	TFPASS(xml.find("tail") != std::string::npos);
	TFPASS(xml.find("second") != std::string::npos);

	doc->unref();
}
