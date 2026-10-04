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

/* COV07 — exercise the importer/exporter registry internals that
 * conversion never reaches: every sniffer's recognizeContents(),
 * the suffix/mime/description lookup tables, getDlgLabels(), and
 * every importer/exporter's constructImporter/constructExporter
 * factory (each of which builds the importer object, running code
 * paths in the ie_imp_/ie_exp_ sources that only fire once per
 * process).
 */

#include "tf_test.h"
#include "pd_Document.h"
#include "ie_imp.h"
#include "ie_exp.h"
#include "ie_types.h"

#include <gsf/gsf-output-stdio.h>
#include <gsf/gsf-input-memory.h>

#include <cstdio>
#include <cstring>
#include <string>

#define TFSUITE "core.wp.impexp.sniffers"

// ------------------------------------------------------------------
// fileTypeForContents: feed representative magic bytes through every
// registered sniffer's recognizeContents()
// ------------------------------------------------------------------
TFTEST_MAIN("contents sniffing recognizes each format's magic")
{
	struct Probe { const char * name; const char bytes[32]; size_t len; };
	const Probe probes[] = {
		{ "abw",   "<?xml version=\"1.0\"?>\n<abiword", 30 },
		{ "rtf",   "{\\rtf1\\ansi\\deff0",             16 },
		{ "doc",   "\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1",  8  },
		{ "docx",  "PK\x03\x04",                        4  },
		{ "odt",   "PK\x03\x04",                        4  },
		{ "wpd",   "\xffWPC\x10",                       5  },
		{ "html",  "<html><body>hi</body></html>",      27 },
		{ "mht",   "MIME-Version: 1.0\nContent-Type:",  28 },
		{ "tex",   "\\documentclass{article}",          22 },
		{ "utf16", "\xff\xfeh\x00i\x00",                6  },
		{ "utf8",  "\xef\xbb\xbfhi there",              9  },
		{ "text",  "plain ascii text\nwith newlines\n", 30 },
	};
	for (const Probe & p : probes) {
		IEFileType ft =
			IE_Imp::fileTypeForContents(p.bytes,
										static_cast<UT_uint32>(p.len));
		TFPASS(ft != IEFT_Bogus);
	}
	// rtf magic must resolve to the rtf importer, not just "any"
	IEFileType rtf = IE_Imp::fileTypeForContents("{\\rtf1\\ansi", 11);
	TFPASS(rtf == IE_Imp::fileTypeForSuffix(".rtf"));
}

// ------------------------------------------------------------------
// suffix/mime/description lookups for every supported format
// ------------------------------------------------------------------
TFTEST_MAIN("suffix, mimetype and description lookups")
{
	const char * suffixes[] = {
		".abw", ".abwn", ".zabw", ".docx", ".doc", ".odt", ".rtf",
		".wpd", ".html", ".xhtml", ".mht", ".epub", ".tex", ".md",
		".txt", ".xml",
	};
	UT_uint32 recognized = 0;
	for (const char * s : suffixes) {
		IEFileType ft = IE_Imp::fileTypeForSuffix(s);
		if (ft == IEFT_Unknown)
			continue;   // some suffixes (md, xml) sniff by content only
		recognized++;
		// the same type must map back to a description + sniffer
		IE_ImpSniffer * sn = IE_Imp::snifferForFileType(ft);
		TFPASS(sn != nullptr);
		if (sn) {
			TFPASS(sn->supportsFileType(ft));
			const char * desc = nullptr, * suff = nullptr;
			IEFileType ft2 = IEFT_Unknown;
			TFPASS(sn->getDlgLabels(&desc, &suff, &ft2));
			TFPASS(desc && suff);
			TFPASS(sn->getSuffixConfidence() != nullptr);
			// getMimeConfidence() may legitimately return nullptr
			sn->getMimeConfidence();
		}
	}
	TFPASS(recognized > 10);
	// multi-suffix probe resolves to a registered type
	TFPASS(IE_Imp::fileTypeForSuffixes(".rtf;.zzz") ==
		   IE_Imp::fileTypeForSuffix(".rtf"));
	// mime lookups
	TFPASS(IE_Imp::fileTypeForMimetype("application/rtf") ==
		   IE_Imp::fileTypeForSuffix(".rtf"));
	TFPASS(IE_Imp::fileTypeForMimetype(
			   "application/vnd.oasis.opendocument.text") ==
		   IE_Imp::fileTypeForSuffix(".odt"));
	TFPASS(IE_Imp::fileTypeForMimetype("text/plain") ==
		   IE_Imp::fileTypeForSuffix(".txt"));

	// exporter direction: suffix -> type -> sniffer -> labels
	const char * exts[] = {
		".abwn", ".docx", ".doc", ".odt", ".rtf", ".html",
		".xhtml", ".tex", ".latex", ".md", ".txt", ".mht",
		".epub", ".pdf",
	};
	for (const char * s : exts) {
		IEFileType ft = IE_Exp::fileTypeForSuffix(s);
		TFPASS(ft != IEFT_Unknown);
		if (ft != IEFT_Unknown) {
			IE_ExpSniffer * sn = IE_Exp::snifferForFileType(ft);
			TFPASS(sn != nullptr);
			if (sn) {
				const char * desc = nullptr, * suff = nullptr;
				IEFileType ft2 = IEFT_Unknown;
				TFPASS(sn->getDlgLabels(&desc, &suff, &ft2));
				TFPASS(desc && suff && ft2 == ft);
				sn->getPreferredSuffix();
			}
			TFPASS(IE_Exp::suffixesForFileType(ft) != nullptr);
		}
	}
	TFPASS(IE_Exp::fileTypeForMimetype("application/rtf") ==
		   IE_Exp::fileTypeForSuffix(".rtf"));
}

// ------------------------------------------------------------------
// enumerateDlgLabels walks the whole registered table
// ------------------------------------------------------------------
TFTEST_MAIN("enumerateDlgLabels walks the registered tables")
{
	UT_uint32 count = IE_Imp::getImporterCount();
	TFPASS(count > 0);
	UT_uint32 seen = 0;
	const char * desc = nullptr, * suff = nullptr;
	IEFileType ft = IEFT_Unknown;
	while (IE_Imp::enumerateDlgLabels(seen, &desc, &suff, &ft)) {
		TFPASS(desc != nullptr);
		seen++;
	}
	TFPASS(seen > 0);

	count = IE_Exp::getExporterCount();
	TFPASS(count > 0);
	seen = 0;
	while (IE_Exp::enumerateDlgLabels(seen, &desc, &suff, &ft)) {
		TFPASS(desc != nullptr);
		seen++;
	}
	TFPASS(seen > 0);
}

// ------------------------------------------------------------------
// constructImporter for every registered type: runs each importer's
// factory + constructor — code conversion never touches
// ------------------------------------------------------------------
TFTEST_MAIN("every importer's factory constructs an importer")
{
	std::string data_file;
	TFPASS(TF_Test::ensure_test_data("/test/wp/Gettysburg.abw",
								   data_file));
	PD_Document * doc = new PD_Document;
	TFPASS(doc->readFromFile(data_file.c_str(), IEFT_Unknown,
							 nullptr) == UT_OK);
	const char * suffixes[] = {
		".abw", ".abwn", ".zabw", ".docx", ".doc", ".odt", ".rtf",
		".wpd", ".html", ".xhtml", ".mht", ".epub", ".tex", ".md",
		".txt", ".xml", ".psitext", ".psiword",
	};
	UT_uint32 okCount = 0, total = 0;
	for (const char * s : suffixes) {
		IEFileType ft = IE_Imp::fileTypeForSuffix(s);
		if (ft == IEFT_Unknown)
			continue;
		total++;
		IE_Imp * imp = nullptr;
		IEFileType ft2 = IEFT_Unknown;
		if (IE_Imp::constructImporter(doc, ft, &imp, &ft2) == UT_OK &&
			imp) {
			okCount++;
			delete imp;
		}
	}
	TFPASS(total > 10);
	TFPASS(okCount > 10);   // most factories must succeed
	doc->unref();
}

// ------------------------------------------------------------------
// constructExporter for every registered type: runs each exporter's
// factory + constructor
// ------------------------------------------------------------------
TFTEST_MAIN("every exporter's factory constructs an exporter")
{
	std::string data_file;
	TFPASS(TF_Test::ensure_test_data("/test/wp/Gettysburg.abw",
								   data_file));
	PD_Document * doc = new PD_Document;
	TFPASS(doc->readFromFile(data_file.c_str(), IEFT_Unknown,
							 nullptr) == UT_OK);
	std::string tmp = std::string("/tmp/ie_sniffers_") +
		std::to_string(::getpid()) + ".out";
	GError * err = nullptr;
	GsfOutput * out = gsf_output_stdio_new(tmp.c_str(), &err);
	TFPASS(out != nullptr);
	const char * exts[] = {
		".abwn", ".docx", ".doc", ".odt", ".rtf", ".html",
		".xhtml", ".tex", ".latex", ".md", ".txt", ".mht",
		".epub", ".pdf",
	};
	UT_uint32 okCount = 0, total = 0;
	for (const char * s : exts) {
		IEFileType ft = IE_Exp::fileTypeForSuffix(s);
		if (ft == IEFT_Unknown)
			continue;
		total++;
		IE_Exp * exp = nullptr;
		IEFileType ft2 = IEFT_Unknown;
		if (IE_Exp::constructExporter(doc, out, ft, &exp, &ft2) ==
			UT_OK && exp) {
			okCount++;
			delete exp;
		}
	}
	if (out)
		g_object_unref(out);
	unlink(tmp.c_str());
	doc->unref();
	TFPASS(total > 10);
	TFPASS(okCount > 10);
}

// ------------------------------------------------------------------
// GsfInput-based recognizeContents + constructImporter: the second
// content-sniffing entry point conversion doesn't exercise
// ------------------------------------------------------------------
TFTEST_MAIN("gsf input sniffing on real fixture bytes")
{
	const struct { const char * file; const char * suffix; } cases[] = {
		{ "/test/wp/Gettysburg.abw",           ".abw"  },
		{ "/test/wp/Word97Test.doc",           ".doc"  },
		{ "/test/wp/rtftest.rtf",              ".rtf"  },
		{ "/test/wp/cov07/rich.docx",          ".docx" },
		{ "/test/wp/cov07/rich.odt",           ".odt"  },
		{ "/test/wp/markdown-formatting.md",   ".md"   },
	};
	for (const auto & c : cases) {
		std::string path;
		if (!TF_Test::ensure_test_data(c.file, path))
			continue;
		// ensure_test_data returns a file:// URI; fopen needs the path
		FILE * fp = fopen(path.c_str() + 7, "rb");
		TFPASS(fp != nullptr);
		if (!fp)
			continue;
		char buf[4096];
		size_t n = fread(buf, 1, sizeof(buf), fp);
		fclose(fp);
		IEFileType sniffed =
			IE_Imp::fileTypeForContents(buf, static_cast<UT_uint32>(n));
		IEFileType bySuffix = IE_Imp::fileTypeForSuffix(c.suffix);
		// sniffed type may differ for ambiguous containers (zips,
		// markdown is plain-text-shaped) — just require *some*
		// confident answer, and identity for unambiguous magic.
		TFPASS(sniffed != IEFT_Bogus);
		if (!strcmp(c.suffix, ".abw") || !strcmp(c.suffix, ".doc") ||
			!strcmp(c.suffix, ".rtf"))
			TFPASS(sniffed == bySuffix);
	}
}
