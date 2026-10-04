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

/* TST07 — math/equation round-trip tests pinning MTH01-03:
 *   .tex equation environments  -> real PTO_Math objects (not the
 *                                  old centered-raw-source fallback),
 *   .md $...$ / $$...$$         -> PTO_Math (inline vs display),
 *   PTO_Math -> .md export      -> dollar-delimited LaTeX,
 *   .md/.tex -> .abwn -> .md    -> full round-trips keep the objects.
 * A skipped or dropped math object fails every assertion below.
 */

#include "tf_test.h"
#include "pd_Document.h"
#include "ie_exp.h"
#include "ie_imp.h"
#include "ie_types.h"

#include <gsf/gsf-output-stdio.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#define TFSUITE "core.wp.impexp.math"

static PD_Document *math_import(const char *rel)
{
	std::string data_file;
	if (!TF_Test::ensure_test_data(rel, data_file))
		return nullptr;

	PD_Document *doc = new PD_Document;
	if (doc->readFromFile(data_file.c_str(), IEFT_Unknown, nullptr) != UT_OK) {
		doc->unref();
		return nullptr;
	}
	return doc;
}

static bool math_export_mem(PD_Document * doc, const char * suffix,
							std::vector<unsigned char> & out)
{
	out.clear();
	std::string tmp = std::string("/tmp/ie_math_") +
		std::to_string(::getpid()) + suffix;
	GError * err = nullptr;
	GsfOutput * file = gsf_output_stdio_new(tmp.c_str(), &err);
	bool ok = file &&
		doc->saveAs(file, static_cast<int>(IE_Exp::fileTypeForSuffix(suffix)),
				   false, nullptr) == UT_OK;
	if (file)
		g_object_unref(file);
	if (ok) {
		FILE * fp = fopen(tmp.c_str(), "rb");
		if (fp) {
			fseek(fp, 0, SEEK_END);
			long sz = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			out.resize(sz);
			ok = fread(out.data(), 1, sz, fp) == static_cast<size_t>(sz);
			fclose(fp);
		} else {
			ok = false;
		}
	}
	unlink(tmp.c_str());
	return ok;
}

static bool math_export_abwn(PD_Document * doc, std::string & out)
{
	std::vector<unsigned char> buf;
	if (!math_export_mem(doc, ".abwn", buf))
		return false;
	out.assign(reinterpret_cast<const char *>(buf.data()), buf.size());
	return true;
}

/* save doc to a temp .abwn and reimport it (round-trip leg) */
static PD_Document *math_abwn_roundtrip(PD_Document * doc)
{
	std::string tmp = std::string("/tmp/ie_math_") +
		std::to_string(::getpid()) + ".abwn";
	GError * err = nullptr;
	GsfOutput * file = gsf_output_stdio_new(tmp.c_str(), &err);
	bool ok = file &&
		doc->saveAs(file, static_cast<int>(IE_Exp::fileTypeForSuffix(".abwn")),
				   false, nullptr) == UT_OK;
	if (file)
		g_object_unref(file);
	if (!ok) {
		unlink(tmp.c_str());
		return nullptr;
	}
	PD_Document *back = new PD_Document;
	if (back->readFromFile(tmp.c_str(), IEFT_Unknown, nullptr) != UT_OK) {
		back->unref();
		back = nullptr;
	}
	unlink(tmp.c_str());
	return back;
}

static size_t math_count(const std::string & hay, const char * needle)
{
	size_t n = 0, pos = 0;
	std::string s(needle);
	while ((pos = hay.find(s, pos)) != std::string::npos) {
		++n;
		pos += s.size();
	}
	return n;
}

// ------------------------------------------------------------------
// MTH02: the .tex equation environment produces a real PTO_Math
// object — before the fix it became a centered paragraph holding the
// raw LaTeX source as text.  Assert object type (the <math> element)
// AND that no source leaked into the document text.
// ------------------------------------------------------------------
TFTEST_MAIN("tex equation env -> PTO_Math object, not centered text")
{
	PD_Document *doc = math_import("/test/wp/tst04/math.tex");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(math_export_abwn(doc, abwn));
	// object type: two real math objects (one inline $..$, one
	// equation env), not paragraphs of source text
	TFPASSEQ(math_count(abwn, "latexid="), 2u);
	TFPASSEQ(math_count(abwn, "display:inline"), 1u);
	TFPASSEQ(math_count(abwn, "display:block"), 1u);
	// not centered text: no raw LaTeX leaked into the doc text — the
	// source lives only base64'd inside the LatexMath data items, so
	// no backslash survives anywhere in the output
	TFPASS(abwn.find('\\') == std::string::npos);
	TFPASS(abwn.find("\\frac") == std::string::npos);
	TFPASS(abwn.find("\\sqrt") == std::string::npos);
	// source + converted MathML kept as paired data items
	TFPASS(abwn.find("LatexMath0") != std::string::npos);
	TFPASS(abwn.find("MathLatex0") != std::string::npos);
	TFPASS(abwn.find("application/mathml+xml") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// MTH02 env coverage: every display math form becomes a display:block
// object, \(..\) becomes display:inline — none may degrade to text
// ------------------------------------------------------------------
TFTEST_MAIN("tex math environments -> PTO_Math objects")
{
	PD_Document *doc = math_import("/test/wp/tst07/math_envs.tex");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(math_export_abwn(doc, abwn));
	// \(..\), displaymath, \[..\], eqnarray* — four objects
	TFPASSEQ(math_count(abwn, "latexid="), 4u);
	TFPASSEQ(math_count(abwn, "display:inline"), 1u);
	TFPASSEQ(math_count(abwn, "display:block"), 3u);
	// nothing fell back to raw-source text
	TFPASS(abwn.find('\\') == std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// MTH01: .md $...$ and $$...$$ become inline/display PTO_Math objects
// sitting inside their paragraphs — not literal dollar text
// ------------------------------------------------------------------
TFTEST_MAIN("md inline + display math -> PTO_Math objects")
{
	PD_Document *doc = math_import("/test/wp/tst04/math.md");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(math_export_abwn(doc, abwn));
	TFPASSEQ(math_count(abwn, "latexid="), 2u);
	TFPASSEQ(math_count(abwn, "display:inline"), 1u);
	TFPASSEQ(math_count(abwn, "display:block"), 1u);
	// the inline object sits in the text flow of its paragraph
	TFPASS(abwn.find("Inline <math") != std::string::npos);
	// neither the delimiters nor the raw source leaked into text
	TFPASS(abwn.find('$') == std::string::npos);
	TFPASS(abwn.find("x^2") == std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// MTH03: markdown export re-emits the stored LaTeX dollar-delimited —
// $...$ for inline, $$...$$ for display
// ------------------------------------------------------------------
TFTEST_MAIN("PTO_Math -> md export emits delimited latex")
{
	PD_Document *doc = math_import("/test/wp/tst04/math.md");
	TFPASS(doc);
	if (!doc)
		return;
	std::vector<unsigned char> md;
	TFPASS(math_export_mem(doc, ".md", md));
	std::string text(reinterpret_cast<const char *>(md.data()), md.size());
	TFPASS(text.find("$x^2 + y_i$") != std::string::npos);
	TFPASS(text.find("$$e^{i\\pi} + 1 = 0$$") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// full round-trip: .md -> .abwn -> reimport -> .md — the math objects
// survive the serialization cycle and re-export delimited again
// ------------------------------------------------------------------
TFTEST_MAIN("md -> abwn -> md round-trip keeps math objects")
{
	PD_Document *doc = math_import("/test/wp/tst04/math.md");
	TFPASS(doc);
	if (!doc)
		return;
	PD_Document *back = math_abwn_roundtrip(doc);
	doc->unref();
	TFPASS(back);
	if (!back)
		return;

	// the reimported document still holds two real math objects
	std::string abwn;
	TFPASS(math_export_abwn(back, abwn));
	TFPASSEQ(math_count(abwn, "latexid="), 2u);

	// and its markdown export still emits the delimited source
	std::vector<unsigned char> md;
	TFPASS(math_export_mem(back, ".md", md));
	std::string text(reinterpret_cast<const char *>(md.data()), md.size());
	TFPASS(text.find("$x^2 + y_i$") != std::string::npos);
	TFPASS(text.find("$$e^{i\\pi} + 1 = 0$$") != std::string::npos);
	back->unref();
}

// ------------------------------------------------------------------
// full round-trip: .tex -> .abwn -> reimport -> .md — same guarantee
// for the LaTeX import path
// ------------------------------------------------------------------
TFTEST_MAIN("tex -> abwn -> md round-trip keeps math objects")
{
	PD_Document *doc = math_import("/test/wp/tst04/math.tex");
	TFPASS(doc);
	if (!doc)
		return;
	PD_Document *back = math_abwn_roundtrip(doc);
	doc->unref();
	TFPASS(back);
	if (!back)
		return;

	std::vector<unsigned char> md;
	TFPASS(math_export_mem(back, ".md", md));
	std::string text(reinterpret_cast<const char *>(md.data()), md.size());
	TFPASS(text.find("$a^2+b^2=c^2$") != std::string::npos);
	TFPASS(text.find("$$\\frac{1}{2} + \\sqrt{x}$$") != std::string::npos);
	back->unref();
}
