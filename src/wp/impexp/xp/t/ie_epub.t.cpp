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

/* COVD08 — pin the EPUB importer (epub/imp/xp/ie_imp_EPUB.cpp) and
 * exporter (epub/exp/xp/ie_exp_EPUB*.cpp).  Packages are synthesized
 * in memory with gsf_outfile_zip, which reaches the malformed/error
 * branches (missing META-INF, bad container root, rootfile traversal,
 * missing OPF, empty spine, unresolvable spine items, href escaping)
 * that no committed fixture exercises.
 */

#include "tf_test.h"
#include "ut_bytebuf.h"
#include "pd_Document.h"
#include "ie_imp.h"
#include "ie_exp.h"
#include "ie_imp_EPUB.h"

#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-output-stdio.h>
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-infile-zip.h>
#include <gsf/gsf-outfile-zip.h>
#include <glib.h>

#include <cstring>
#include <string>
#include <vector>
#include <utility>

#define TFSUITE "core.wp.impexp.epub"

/* build a zip package in memory from (member, contents) pairs */
static GsfInput * epub_zip(const std::vector<std::pair<std::string, std::string>> & entries)
{
	GsfOutput * mem = gsf_output_memory_new();
	GsfOutfile * zip = gsf_outfile_zip_new(mem, nullptr);
	for (auto & e : entries)
	{
		GsfOutput * child =
			gsf_outfile_new_child(zip, e.first.c_str(), FALSE);
		gsf_output_write(child, e.second.size(),
						 reinterpret_cast<const guint8 *>(e.second.data()));
		gsf_output_close(child);
		g_object_unref(child);
	}
	gsf_output_close(GSF_OUTPUT(zip));
	g_object_unref(zip);

	/* the memory output owns its buffer; the input needs its own */
	gsf_off_t size = gsf_output_size(mem);
	guint8 * copy = static_cast<guint8 *>(g_malloc(size));
	memcpy(copy, gsf_output_memory_get_bytes(GSF_OUTPUT_MEMORY(mem)),
		   size);
	g_object_unref(mem);
	return gsf_input_memory_new(copy, size, TRUE);
}

static const char * kContainer = "META-INF/container.xml";
static const char * kMime = "application/epub+zip";

static std::string container_xml(const char * rootfiles)
{
	return std::string("<?xml version=\"1.0\"?>\n"
					   "<container xmlns=\"urn:oasis:names:tc:opendocument:"
					   "xmlns:container\" version=\"1.0\"><rootfiles>") +
		rootfiles + "</rootfiles></container>";
}

static std::string rootfile(const char * path, const char * media = nullptr)
{
	std::string r = "<rootfile full-path=\"";
	r += path;
	r += "\"";
	if (media)
	{
		r += " media-type=\"";
		r += media;
		r += "\"";
	}
	r += "/>";
	return r;
}

static std::string opf(const char * metadata, const char * manifest,
					   const char * spine)
{
	return std::string("<?xml version=\"1.0\"?>\n"
					   "<package xmlns=\"http://www.idpf.org/2007/opf\" "
					   "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
					   "unique-identifier=\"id\" version=\"3.0\"><metadata>") +
		(metadata ? metadata : "") + "</metadata><manifest>" +
		(manifest ? manifest : "") + "</manifest><spine>" +
		(spine ? spine : "") + "</spine></package>";
}

static const char * kChapter =
	"<html xmlns=\"http://www.w3.org/1999/xhtml\"><body>"
	"<p>chapter body text</p></body></html>";

/* minimal well-formed package, all members present */
static GsfInput * epub_ok(const char * rootfilePath = "OEBPS/book.opf",
						  const char * rootMedia = "application/oebps-package+xml",
						  const char * metadata = "<dc:title>T</dc:title>",
						  const char * manifest = nullptr,
						  const char * spine = nullptr)
{
	std::string m =
		manifest ? manifest
				 : "<item id=\"c1\" href=\"c1.xhtml\" "
				   "media-type=\"application/xhtml+xml\"/>";
	std::string s = spine ? spine : "<itemref idref=\"c1\"/>";
	std::vector<std::pair<std::string, std::string>> e = {
		{ "mimetype", kMime },
		{ kContainer,
		  container_xml(rootfile(rootfilePath, rootMedia).c_str()) },
		{ "OEBPS/book.opf", opf(metadata, m.c_str(), s.c_str()) },
		{ "OEBPS/c1.xhtml", kChapter },
	};
	return epub_zip(e);
}

/* load a package through the importer; doc is always returned for
 * unref by the caller */
static UT_Error epub_load(GsfInput * input, PD_Document ** ppDoc)
{
	PD_Document * doc = new PD_Document;
	doc->createRawDocument();
	UT_Error err = IE_Imp::loadFile(doc, input,
									IE_Imp::fileTypeForSuffix(".epub"));
	if (err == UT_OK)
		doc->finishRawCreation();
	*ppDoc = doc;
	return err;
}

static bool epub_loads(GsfInput * input)
{
	PD_Document * doc = nullptr;
	UT_Error err = epub_load(input, &doc);
	g_object_unref(input);
	doc->unref();
	return err == UT_OK;
}

static bool epub_export_abwn(PD_Document * doc, std::string & out)
{
	std::string tmp = std::string("/tmp/ie_epub_") +
		std::to_string(::getpid()) + ".abwn";
	GError * err = nullptr;
	GsfOutput * file = gsf_output_stdio_new(tmp.c_str(), &err);
	bool ok = file &&
		doc->saveAs(file,
					static_cast<int>(IE_Exp::fileTypeForSuffix(".abwn")),
					false, nullptr) == UT_OK;
	if (file)
		g_object_unref(file);
	if (ok)
	{
		FILE * fp = fopen(tmp.c_str(), "rb");
		if (!fp)
			ok = false;
		else
		{
			fseek(fp, 0, SEEK_END);
			long sz = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			out.resize(static_cast<size_t>(sz));
			ok = fread(&out[0], 1, sz, fp) == static_cast<size_t>(sz);
			fclose(fp);
		}
	}
	unlink(tmp.c_str());
	return ok;
}

TFTEST_MAIN("epub rich fixture imports chapters and metadata")
{
	std::string path = TF_Test::get_test_src_dir();
	path += "/test/wp/cov07/rich.epub";
	PD_Document * doc = new PD_Document;
	TFPASSEQ(doc->readFromFile(path.c_str(), IEFT_Unknown, nullptr),
			 UT_OK);
	std::string abwn;
	TFPASS(epub_export_abwn(doc, abwn));
	/* OPF Dublin Core -> document metadata */
	TFPASS(abwn.find("<m key=\"dc.title\">Coverage Fixture")
		   != std::string::npos);
	TFPASS(abwn.find("<m key=\"dc.creator\">Abinova Tests")
		   != std::string::npos);
	/* several spine chapters joined into one document */
	TFPASS(abwn.find("Rich Coverage Document") != std::string::npos);
	doc->unref();
}

TFTEST_MAIN("epub export round-trips a document package")
{
	std::string path = TF_Test::get_test_src_dir();
	path += "/test/wp/cov07/rich.epub";
	PD_Document * doc = new PD_Document;
	TFPASSEQ(doc->readFromFile(path.c_str(), IEFT_Unknown, nullptr),
			 UT_OK);

	std::string tmp = std::string("/tmp/ie_epub_out_") +
		std::to_string(::getpid()) + ".epub";
	GError * err = nullptr;
	GsfOutput * out = gsf_output_stdio_new(tmp.c_str(), &err);
	TFPASS(out != nullptr);
	bool saved = out &&
		doc->saveAs(out,
					static_cast<int>(IE_Exp::fileTypeForSuffix(".epub")),
					false, nullptr) == UT_OK;
	if (out)
		g_object_unref(out);
	TFPASS(saved);
	doc->unref();

	/* the written package must open in the importer again */
	if (saved)
	{
		GsfInput * in = gsf_input_stdio_new(tmp.c_str(), nullptr);
		TFPASS(in != nullptr);
		if (in)
		{
			GsfInfile * zip = gsf_infile_zip_new(in, nullptr);
			TFPASS(zip != nullptr);
			if (zip)
			{
				TFPASS(gsf_infile_child_by_name(zip, "mimetype")
					   != nullptr);
				TFPASS(gsf_infile_child_by_name(zip, "META-INF")
					   != nullptr);
				TFPASS(gsf_infile_child_by_name(zip, "OEBPS")
					   != nullptr);
				g_object_unref(zip);
			}
			g_object_unref(in);
		}
	}
	unlink(tmp.c_str());
}

TFTEST_MAIN("epub malformed packages reject cleanly")
{
	IEFileType ft = IE_Imp::fileTypeForSuffix(".epub");
	TFPASS(ft != IEFT_Unknown);

	/* not a zip at all */
	{
		static const guint8 garbage[] = "not a zip file";
		GsfInput * in = gsf_input_memory_new(garbage, sizeof(garbage),
										   FALSE);
		TFPASS(!epub_loads(in));
	}
	/* a zip without META-INF */
	{
		std::vector<std::pair<std::string, std::string>> e = {
			{ "mimetype", kMime }, { "OEBPS/book.opf", "<x/>" }
		};
		TFPASS(!epub_loads(epub_zip(e)));
	}
	/* META-INF present but container.xml missing */
	{
		std::vector<std::pair<std::string, std::string>> e = {
			{ "mimetype", kMime },
			{ "META-INF/other.xml", "<x/>" }
		};
		TFPASS(!epub_loads(epub_zip(e)));
	}
	/* container.xml whose root is not <container> */
	{
		std::vector<std::pair<std::string, std::string>> e = {
			{ "mimetype", kMime },
			{ kContainer, "<notcontainer/>" }
		};
		TFPASS(!epub_loads(epub_zip(e)));
	}
	/* container declares no rootfile */
	{
		std::vector<std::pair<std::string, std::string>> e = {
			{ "mimetype", kMime },
			{ kContainer, container_xml("") }
		};
		TFPASS(!epub_loads(epub_zip(e)));
	}
	/* rootfile path escapes the package root */
	{
		std::vector<std::pair<std::string, std::string>> e = {
			{ "mimetype", kMime },
			{ kContainer,
			  container_xml(
				  rootfile("../evil.opf",
						   "application/oebps-package+xml").c_str()) }
		};
		TFPASS(!epub_loads(epub_zip(e)));
	}
	/* rootfile points at a member that isn't there */
	{
		std::vector<std::pair<std::string, std::string>> e = {
			{ "mimetype", kMime },
			{ kContainer,
			  container_xml(
				  rootfile("OEBPS/missing.opf",
						   "application/oebps-package+xml").c_str()) }
		};
		TFPASS(!epub_loads(epub_zip(e)));
	}
	/* the OPF's root isn't <package> */
	{
		std::vector<std::pair<std::string, std::string>> e = {
			{ "mimetype", kMime },
			{ kContainer,
			  container_xml(
				  rootfile("OEBPS/book.opf",
						   "application/oebps-package+xml").c_str()) },
			{ "OEBPS/book.opf", "<notpackage/>" }
		};
		TFPASS(!epub_loads(epub_zip(e)));
	}
	/* a spine with no itemrefs */
	{
		TFPASS(!epub_loads(epub_ok("OEBPS/book.opf",
								   "application/oebps-package+xml",
								   "<dc:title>x</dc:title>", nullptr,
								   "")));
	}
}

TFTEST_MAIN("epub rootfile media-type wins over document order")
{
	std::vector<std::pair<std::string, std::string>> e = {
		{ "mimetype", kMime },
		{ kContainer,
		  container_xml((rootfile("OEBPS/decoy.opf", "text/plain") +
						 rootfile("OEBPS/book.opf",
								  "application/oebps-package+xml"))
							.c_str()) },
		{ "OEBPS/book.opf",
		  opf("<dc:title>multi</dc:title>",
			  "<item id=\"c1\" href=\"c1.xhtml\" "
			  "media-type=\"application/xhtml+xml\"/>",
			  "<itemref idref=\"c1\"/>") },
		{ "OEBPS/c1.xhtml", kChapter },
		{ "OEBPS/decoy.opf", "<notpackage/>" },
	};
	PD_Document * doc = nullptr;
	GsfInput * in = epub_zip(e);
	TFPASSEQ(epub_load(in, &doc), UT_OK);
	g_object_unref(in);
	doc->unref();
}

TFTEST_MAIN("epub rootfile without media-type falls back to first")
{
	TFPASS(epub_loads(epub_ok("OEBPS/book.opf", nullptr)));
}

TFTEST_MAIN("epub rootfile entry without full-path is ignored")
{
	std::vector<std::pair<std::string, std::string>> e = {
		{ "mimetype", kMime },
		{ kContainer,
		  container_xml("<rootfile media-type=\"x\"/>"
						"<rootfile full-path=\"OEBPS/book.opf\"/>") },
		{ "OEBPS/book.opf",
		  opf("<dc:title>x</dc:title>",
			  "<item id=\"c1\" href=\"c1.xhtml\" "
			  "media-type=\"application/xhtml+xml\"/>",
			  "<itemref idref=\"c1\"/>") },
		{ "OEBPS/c1.xhtml", kChapter },
	};
	TFPASS(epub_loads(epub_zip(e)));
}

TFTEST_MAIN("epub spine items skip unresolvable entries")
{
	/* idref without a manifest item, a href that escapes the
	 * package, a member that isn't in the archive, an itemref
	 * with no idref, and a percent-encoded href — only the last
	 * item should import */
	std::string manifest =
		"<item id=\"esc\" href=\"../escape.xhtml\" "
		"media-type=\"application/xhtml+xml\"/>"
		"<item id=\"gone\" href=\"gone.xhtml\" "
		"media-type=\"application/xhtml+xml\"/>"
		"<item id=\"good\" href=\"chap%20ter.xhtml\" "
		"media-type=\"application/xhtml+xml\"/>"
		/* manifest item with no href: skipped by the listener */
		"<item id=\"nohref\" media-type=\"application/xhtml+xml\"/>";
	std::string spine =
		"<itemref/>"
		"<itemref idref=\"notinmanifest\"/>"
		"<itemref idref=\"esc\"/>"
		"<itemref idref=\"gone\"/>"
		"<itemref idref=\"good\"/>";
	std::vector<std::pair<std::string, std::string>> e = {
		{ "mimetype", kMime },
		{ kContainer,
		  container_xml(rootfile("OEBPS/book.opf",
								 "application/oebps-package+xml")
							.c_str()) },
		{ "OEBPS/book.opf",
		  opf("<dc:title>s</dc:title>", manifest.c_str(),
			  spine.c_str()) },
		{ "OEBPS/chap ter.xhtml", kChapter },
	};
	GsfInput * in = epub_zip(e);
	PD_Document * doc = nullptr;
	TFPASS(epub_load(in, &doc) == UT_OK);
	if (doc)
	{
		std::string abwn;
		TFPASS(epub_export_abwn(doc, abwn));
		TFPASS(abwn.find("chapter body text") != std::string::npos);
		doc->unref();
	}
}

TFTEST_MAIN("epub metadata joins repeated dc elements")
{
	std::string meta =
		"<dc:title>Joined</dc:title>"
		"<dc:creator>Author One</dc:creator>"
		"<dc:creator>Author Two</dc:creator>"
		"<dc:subject>   \n  </dc:subject>" /* whitespace-only: dropped */
		"<dc:publisher>Pub</dc:publisher>"
		"<dc:nonexistent>nope</dc:nonexistent>";
	GsfInput * in = epub_ok("OEBPS/book.opf",
							"application/oebps-package+xml",
							meta.c_str());
	PD_Document * doc = nullptr;
	TFPASS(epub_load(in, &doc) == UT_OK);
	if (doc)
	{
		std::string v;
		TFPASS(doc->getMetaDataProp(PD_META_KEY_TITLE, v) &&
			   v == "Joined");
		TFPASS(doc->getMetaDataProp(PD_META_KEY_CREATOR, v) &&
			   v == "Author One; Author Two");
		TFPASS(doc->getMetaDataProp(PD_META_KEY_PUBLISHER, v) &&
			   v == "Pub");
		TFPASS(!doc->getMetaDataProp(PD_META_KEY_SUBJECT, v));
		doc->unref();
	}
}

TFTEST_MAIN("epub chapter hrefs resolve inside the package only")
{
	/* img/link hrefs: relative member, percent-encoded member,
	 * nested "..", plus rejects for scheme, absolute, fragment
	 * and escaping paths — the good one becomes a data item */
	const char * chapter =
		"<html xmlns=\"http://www.w3.org/1999/xhtml\"><head>"
		"<link rel=\"stylesheet\" href=\"style.css\"/>"
		"<link rel=\"stylesheet\" href=\"http://evil.example/x.css\"/>"
		"<link rel=\"stylesheet\" href=\"/abs.css\"/>"
		"</head><body>"
		"<p>has image <img src=\"pic.png\"/></p>"
		"<p><img src=\"sub/../enc%20oded.png\"/></p>"
		"<p><img src=\"../../escape.png\"/></p>"
		"<p><img src=\"#fragment\"/></p>"
		"<p><img src=\"\"/></p>"
		"</body></html>";
	/* a 1x1 png */
	static const char png_b64[] =
		"\x89PNG\r\n\x1a\n";
	std::string png(png_b64, 12); /* header only is enough for the
									   provider-resolution branches */
	std::vector<std::pair<std::string, std::string>> e = {
		{ "mimetype", kMime },
		{ kContainer,
		  container_xml(rootfile("OEBPS/book.opf",
								 "application/oebps-package+xml")
							.c_str()) },
		{ "OEBPS/book.opf",
		  opf("<dc:title>r</dc:title>",
			  "<item id=\"c1\" href=\"c1.xhtml\" "
			  "media-type=\"application/xhtml+xml\"/>"
			  "<item id=\"i1\" href=\"pic.png\" "
			  "media-type=\"image/png\"/>",
			  "<itemref idref=\"c1\"/>") },
		{ "OEBPS/c1.xhtml", chapter },
		{ "OEBPS/pic.png", png },
		{ "OEBPS/enc oded.png", png },
		{ "OEBPS/style.css", "p { margin: 0; }" },
	};
	TFPASS(epub_loads(epub_zip(e)));
}

TFTEST_MAIN("epub pasteFromBuffer splices package content")
{
	PD_Document * doc = new PD_Document;
	TFPASSEQ(doc->newDocument(), UT_OK);
	PT_DocPosition pos = 0;
	TFPASS(doc->getBounds(true, pos));

	/* paste needs the bytes in memory; build a small package and
	 * pull its bytes back out of a memory input */
	GsfInput * pkg = epub_ok();
	gsf_off_t sz = gsf_input_size(pkg);
	const guint8 * bytes = gsf_input_read(pkg, sz, nullptr);
	TFPASS(bytes != nullptr);
	std::vector<unsigned char> data(bytes, bytes + sz);
	g_object_unref(pkg);

	PD_DocumentRange dr(doc, pos, pos);
	IE_Imp_EPUB imp(doc);
	TFPASS(imp.pasteFromBuffer(&dr, data.data(),
							 static_cast<UT_uint32>(data.size())));

	std::string abwn;
	TFPASS(epub_export_abwn(doc, abwn));
	TFPASS(abwn.find("chapter body text") != std::string::npos);
	doc->unref();
}
