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

/* COVD07 — pin the GTK importer/exporter module (wp/impexp/gtk):
 * ie_impGraphic_GdkPixbuf (gdk-pixbuf loader path, XPM text loader,
 * sniffer surface) and ie_exp_PDF (cairo PDF/PS/SVG backends, the
 * "pages" range property, sniffer surface).  Fixtures live in
 * test/wp/gtkcov/; image round-trips assert the produced buffer
 * re-imports successfully.
 */

#include "tf_test.h"
#include "ut_bytebuf.h"
#include "ie_impGraphic.h"
#include "ie_impGraphic_GdkPixbuf.h"
#include "ie_exp.h"
#include "ie_exp_PDF.h"
#include "pd_Document.h"
#include "fg_Graphic.h"

#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-output-memory.h>
#include <glib.h>

#include <cstring>
#include <string>

#define TFSUITE "core.wp.impexp.gtkio"

static UT_ByteBuf * gtkio_fixture(const char * rel)
{
	std::string path = TF_Test::get_test_src_dir();
	path += "/";
	path += rel;
	GsfInput * input = gsf_input_stdio_new(path.c_str(), nullptr);
	if (!input)
		return nullptr;
	gsf_off_t sz = gsf_input_size(input);
	const guint8 * data = gsf_input_read(input, sz, nullptr);
	UT_ByteBuf * bb = nullptr;
	if (data)
	{
		bb = new UT_ByteBuf;
		bb->append(reinterpret_cast<const UT_Byte *>(data),
				   static_cast<UT_uint32>(sz));
	}
	g_object_unref(input);
	return bb;
}

static bool is_png_buf(const UT_ByteBuf * bb)
{
	static const UT_Byte magic[] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
	return bb && bb->getLength() >= 8 &&
		memcmp(bb->getPointer(0), magic, 8) == 0;
}

static PD_Document * gtkio_doc(void)
{
	std::string data_file;
	if (!TF_Test::ensure_test_data("/test/wp/Gettysburg.abw", data_file))
		return nullptr;
	PD_Document *doc = new PD_Document;
	if (doc->readFromFile(data_file.c_str(), IEFT_Unknown, nullptr) != UT_OK) {
		doc->unref();
		return nullptr;
	}
	return doc;
}

/* -------------------------------------------------- XPM loader path */

TFTEST_MAIN("xpm text image imports via XPM loader and round-trips")
{
	UT_ByteBuf * bb = gtkio_fixture("test/wp/gtkcov/seed.xpm");
	TFPASS(bb != nullptr);
	if (!bb)
		return;

	IE_ImpGraphic_GdkPixbuf imp;
	FG_ConstGraphicPtr pfg;
	UT_ConstByteBufPtr cbb(bb);
	TFPASS(imp.importGraphic(cbb, pfg) == UT_OK);
	TFPASS(pfg != nullptr);
	if (!pfg)
		return;

	/* non-JPEG input converts to a PNG raster */
	TFPASS(is_png_buf(pfg->getBuffer().get()));
	TFPASS(pfg->getMimeType() == "image/png");

	/* round-trip: the produced PNG buffer imports cleanly again */
	IE_ImpGraphic_GdkPixbuf imp2;
	FG_ConstGraphicPtr pfg2;
	TFPASS(imp2.importGraphic(pfg->getBuffer(), pfg2) == UT_OK);
	TFPASS(pfg2 != nullptr);
	if (pfg2)
		TFPASS(is_png_buf(pfg2->getBuffer().get()));
}

TFTEST_MAIN("gdkpixbuf importer handles raster formats; JPEG kept raw")
{
	static const char * const seeds[] = {
		"test/wp/gtkcov/seed.png",
		"test/wp/gtkcov/seed.gif",
		"test/wp/gtkcov/seed.bmp",
	};
	for (const char * rel : seeds)
	{
		UT_ByteBuf * bb = gtkio_fixture(rel);
		TFPASS(bb != nullptr);
		if (!bb)
			continue;
		IE_ImpGraphic_GdkPixbuf imp;
		FG_ConstGraphicPtr pfg;
		UT_ConstByteBufPtr cbb(bb);
		TFPASS(imp.importGraphic(cbb, pfg) == UT_OK);
		TFPASS(pfg != nullptr);
		if (pfg)
		{
			TFPASS(is_png_buf(pfg->getBuffer().get()));
			TFPASS(pfg->getMimeType() == "image/png");
		}
	}

	/* JPEG is passed through untouched (keeps the original stream) */
	UT_ByteBuf * jpg = gtkio_fixture("test/wp/gtkcov/seed.jpg");
	TFPASS(jpg != nullptr);
	if (jpg)
	{
		IE_ImpGraphic_GdkPixbuf imp;
		FG_ConstGraphicPtr pfg;
		UT_ConstByteBufPtr cbb(jpg);
		TFPASS(imp.importGraphic(cbb, pfg) == UT_OK);
		TFPASS(pfg != nullptr);
		if (pfg)
		{
			TFPASS(pfg->getMimeType() == "image/jpeg");
			const UT_ByteBuf * out = pfg->getBuffer().get();
			TFPASS(out && out->getLength() >= 2 &&
				   out->getPointer(0)[0] == 0xff &&
				   out->getPointer(0)[1] == 0xd8);
		}
	}
}

TFTEST_MAIN("gdkpixbuf importer rejects malformed and empty input")
{
	IE_ImpGraphic_GdkPixbuf imp;

	/* no loader recognises this */
	UT_ByteBuf * garbage = gtkio_fixture("test/wp/gtkcov/edge_garbage.img");
	TFPASS(garbage != nullptr);
	if (garbage)
	{
		FG_ConstGraphicPtr pfg;
		UT_ConstByteBufPtr cbb(garbage);
		TFPASS(imp.importGraphic(cbb, pfg) == UT_ERROR);
	}

	/* XPM marker but truncated -> _loadXPM bails, no crash/leak */
	UT_ByteBuf * broken = gtkio_fixture("test/wp/gtkcov/edge_broken.xpm");
	TFPASS(broken != nullptr);
	if (broken)
	{
		FG_ConstGraphicPtr pfg;
		UT_ConstByteBufPtr cbb(broken);
		TFPASS(imp.importGraphic(cbb, pfg) == UT_ERROR);
	}

	/* null and empty buffers fail fast */
	FG_ConstGraphicPtr pfg;
	TFPASS(imp.importGraphic(UT_ConstByteBufPtr(), pfg) == UT_ERROR);
	UT_ByteBuf * empty = new UT_ByteBuf;
	TFPASS(imp.importGraphic(UT_ConstByteBufPtr(empty), pfg) == UT_ERROR);
}

/* ----------------------------------------------------- sniffer API */

TFTEST_MAIN("gdkpixbuf sniffer surface: contents, suffix, mime, labels")
{
	IE_ImpGraphicGdkPixbuf_Sniffer sn;

	UT_ByteBuf * png = gtkio_fixture("test/wp/gtkcov/seed.png");
	TFPASS(png != nullptr);
	if (png)
		TFPASS(sn.recognizeContents(
				   reinterpret_cast<const char *>(png->getPointer(0)),
				   png->getLength()) != UT_CONFIDENCE_ZILCH);
	delete png;

	UT_ByteBuf * xpm = gtkio_fixture("test/wp/gtkcov/seed.xpm");
	TFPASS(xpm != nullptr);
	if (xpm)
		TFPASS(sn.recognizeContents(
				   reinterpret_cast<const char *>(xpm->getPointer(0)),
				   xpm->getLength()) == UT_CONFIDENCE_PERFECT);
	delete xpm;

	UT_ByteBuf * garbage = gtkio_fixture("test/wp/gtkcov/edge_garbage.img");
	TFPASS(garbage != nullptr);
	if (garbage)
		TFPASS(sn.recognizeContents(
				   reinterpret_cast<const char *>(garbage->getPointer(0)),
				   garbage->getLength()) == UT_CONFIDENCE_ZILCH);
	delete garbage;

	/* suffix confidence table is built once and cached — call twice to
	 * cover both paths; png must be listed with high confidence */
	const IE_SuffixConfidence * sc = sn.getSuffixConfidence();
	const IE_SuffixConfidence * sc2 = sn.getSuffixConfidence();
	TFPASS(sc == sc2);
	TFPASS(sc != nullptr);
	bool saw_png = false;
	if (sc)
		for (int i = 0; sc[i].confidence != UT_CONFIDENCE_ZILCH; i++)
			if (sc[i].suffix == "png")
				saw_png = true;
	TFPASS(saw_png);

	const IE_MimeConfidence * mc = sn.getMimeConfidence();
	const IE_MimeConfidence * mc2 = sn.getMimeConfidence();
	TFPASS(mc == mc2);
	TFPASS(mc != nullptr);
	bool saw_png_mime = false;
	if (mc)
		for (int i = 0; mc[i].match != IE_MIME_MATCH_BOGUS; i++)
			if (mc[i].mimetype == "image/png")
				saw_png_mime = true;
	TFPASS(saw_png_mime);

	/* dialog labels build the dynamic format description once */
	const char *desc = nullptr, *suff = nullptr;
	IEGraphicFileType ft = IEGFT_Unknown;
	TFPASS(sn.getDlgLabels(&desc, &suff, &ft));
	TFPASS(desc && strstr(desc, "image formats"));
	TFPASS(suff && strstr(suff, "*.png"));
	/* second call covers the cached-labels early return; a stack-built
	 * (unregistered) sniffer legitimately reports IEGFT_Unknown */
	TFPASS(sn.getDlgLabels(&desc, &suff, &ft));

	/* mimeTypeForSuffix: direct, dotted, case-insensitive, misses */
	const char * mt = sn.mimeTypeForSuffix("png");
	TFPASS(mt && strcmp(mt, "image/png") == 0);
	TFPASS(sn.mimeTypeForSuffix(".PNG") == mt);
	TFPASS(sn.mimeTypeForSuffix("no_such_fmt") == nullptr);
	TFPASS(sn.mimeTypeForSuffix("") == nullptr);
	TFPASS(sn.mimeTypeForSuffix(nullptr) == nullptr);

	/* registry routing reaches the same mime table */
	TFPASS(IE_ImpGraphic::getMimeTypeForSuffix(".png") != nullptr);

	/* importer construction through the sniffer */
	IE_ImpGraphic * via = nullptr;
	TFPASS(sn.constructImporter(&via) == UT_OK);
	TFPASS(via != nullptr);
	delete via;
}

/* ------------------------------------------------- cairo exporters */

TFTEST_MAIN("cairo exporters: PS and PDF write output, SVG is rejected")
{
	PD_Document * doc = gtkio_doc();
	TFPASS(doc != nullptr);
	if (!doc)
		return;

	IEFileType psft = IE_Exp::fileTypeForSuffix(".ps");
	IEFileType pdfft = IE_Exp::fileTypeForSuffix(".pdf");
	IEFileType svgft = IE_Exp::fileTypeForSuffix(".svg");
	TFPASS(psft != IEFT_Unknown);
	TFPASS(pdfft != IEFT_Unknown);

	/* PS backend produces a PostScript stream */
	GsfOutput * out = gsf_output_memory_new();
	TFPASS(out != nullptr);
	if (out)
	{
		TFPASS(doc->saveAs(out, static_cast<int>(psft), false, nullptr) == UT_OK);
		const guint8 * bytes = gsf_output_memory_get_bytes(GSF_OUTPUT_MEMORY(out));
		TFPASS(bytes && gsf_output_size(out) > 4 &&
			   memcmp(bytes, "%!PS", 4) == 0);
		g_object_unref(out);
	}

	/* PDF backend + the "pages" range property parser */
	out = gsf_output_memory_new();
	if (out)
	{
		TFPASS(doc->saveAs(out, static_cast<int>(pdfft), false,
						   "pages:1-999,zzz,2-1,4") == UT_OK);
		const guint8 * bytes = gsf_output_memory_get_bytes(GSF_OUTPUT_MEMORY(out));
		TFPASS(bytes && gsf_output_size(out) > 5 &&
			   memcmp(bytes, "%PDF-", 5) == 0);
		g_object_unref(out);
	}

	/* the SVG cairo backend is a stub — save must fail cleanly */
	out = gsf_output_memory_new();
	if (out)
	{
		if (svgft != IEFT_Unknown)
			TFPASS(doc->saveAs(out, static_cast<int>(svgft), false,
							   nullptr) != UT_OK);
		else
		{
			/* not registered: drive the stub directly instead */
			IE_Exp_SVG_Sniffer sn;
			IE_Exp * exp = nullptr;
			TFPASS(sn.constructExporter(doc, &exp) == UT_OK);
			TFPASS(exp != nullptr);
			if (exp)
			{
				TFPASS(exp->writeFile(out) != UT_OK);
				delete exp;
			}
		}
		g_object_unref(out);
	}

	doc->unref();
}

TFTEST_MAIN("cairo exporter sniffers: mime, suffix, labels")
{
	IEFileType psft = IE_Exp::fileTypeForSuffix(".ps");
	IEFileType pdfft = IE_Exp::fileTypeForSuffix(".pdf");

	TFPASS(IE_Exp::fileTypeForMimetype("application/pdf") == pdfft);
	TFPASS(IE_Exp::fileTypeForMimetype("application/postscript") == psft);
	TFPASS(IE_Exp::fileTypeForMimetype("bogus/mimetype") == IEFT_Unknown);

	/* the SVG sniffer isn't registered — drive it directly */
	IE_Exp_SVG_Sniffer ssn;
	TFPASS(ssn.supportsMIME("image/svg+xml") == UT_CONFIDENCE_PERFECT);
	TFPASS(ssn.supportsMIME("image/svg") == UT_CONFIDENCE_PERFECT);
	TFPASS(ssn.supportsMIME("text/plain") == UT_CONFIDENCE_ZILCH);
	TFPASS(ssn.recognizeSuffix(".svg"));
	TFPASS(!ssn.recognizeSuffix(".pdf"));

	IE_Exp_PS_Sniffer psn;
	TFPASS(psn.supportsMIME("application/postscript") == UT_CONFIDENCE_PERFECT);
	TFPASS(psn.supportsMIME("application/pdf") == UT_CONFIDENCE_ZILCH);
	TFPASS(psn.recognizeSuffix(".ps"));
	TFPASS(!psn.recognizeSuffix(".eps"));

	IE_Exp_PDF_Sniffer pdfn;
	TFPASS(pdfn.supportsMIME("application/pdf") == UT_CONFIDENCE_PERFECT);
	TFPASS(pdfn.supportsMIME("application/postscript") == UT_CONFIDENCE_ZILCH);
	TFPASS(pdfn.recognizeSuffix(".pdf"));
	TFPASS(!pdfn.recognizeSuffix(".ps"));

	const char *desc = nullptr, *suff = nullptr;
	IEFileType ft = IEFT_Unknown;
	TFPASS(ssn.getDlgLabels(&desc, &suff, &ft) && suff && strstr(suff, "svg"));
	TFPASS(psn.getDlgLabels(&desc, &suff, &ft) && suff && strstr(suff, "ps"));
	TFPASS(pdfn.getDlgLabels(&desc, &suff, &ft) && suff && strstr(suff, "pdf"));

	/* exporter construction via sniffer */
	PD_Document * doc = gtkio_doc();
	TFPASS(doc != nullptr);
	if (doc)
	{
		IE_Exp * exp = nullptr;
		TFPASS(psn.constructExporter(doc, &exp) == UT_OK);
		delete exp;
		exp = nullptr;
		TFPASS(pdfn.constructExporter(doc, &exp) == UT_OK);
		delete exp;
		exp = nullptr;
		TFPASS(ssn.constructExporter(doc, &exp) == UT_OK);
		delete exp;
		doc->unref();
	}
}
