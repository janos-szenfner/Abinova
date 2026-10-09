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

/* TST05 — DOCX fidelity corpus: import each reference cover docx and
 * assert the per-file expectations recorded in
 * test/wp/tst05/FIXTURES.md by inspecting the re-exported .abwn
 * markup (frames, custGeom paths, anchored images, header/footer
 * struxes, resolved fields, theme colors).
 *
 * The fixtures are Microsoft-template-derived .docx files and are NOT
 * committed (licensing).  Point COVER_FIXTURES_DIR at the directory
 * holding them (e.g. ~/Documents); every test in this suite prints
 * SKIP and passes trivially when the variable is unset or a fixture
 * is missing.
 */

#include "tf_test.h"
#include "pd_Document.h"
#include "ie_exp.h"
#include "ie_imp.h"
#include "ie_types.h"
#include "ut_units.h"

#include <gsf/gsf-output-stdio.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#define TFSUITE "core.wp.impexp.covercorpus"

/* NULL when the fixture corpus is unavailable — callers SKIP */
static const char *cc_fixture_dir()
{
	const char *d = getenv("COVER_FIXTURES_DIR");
	return (d && *d) ? d : nullptr;
}

static PD_Document *cc_import_cover(const char *dir, const char *name)
{
	std::string path = std::string(dir) + "/" + name;
	FILE *fp = fopen(path.c_str(), "rb");
	if (!fp)
		return nullptr;
	fclose(fp);

	std::string uri = "file://" + path;
	PD_Document *doc = new PD_Document;
	if (doc->readFromFile(uri.c_str(), IEFT_Unknown, nullptr) != UT_OK) {
		doc->unref();
		return nullptr;
	}
	return doc;
}

static bool cc_export_abwn(PD_Document *doc, std::string &out)
{
	std::string tmp = std::string("/tmp/ie_covercorpus_") +
		std::to_string(::getpid()) + ".abwn";
	GError *err = nullptr;
	GsfOutput *file = gsf_output_stdio_new(tmp.c_str(), &err);
	bool ok = file &&
		doc->saveAs(file,
				   static_cast<int>(IE_Exp::fileTypeForSuffix(".abwn")),
				   false, nullptr) == UT_OK;
	if (file)
		g_object_unref(file);
	if (ok) {
		FILE *fp = fopen(tmp.c_str(), "rb");
		if (fp) {
			fseek(fp, 0, SEEK_END);
			long sz = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			std::vector<unsigned char> buf(sz);
			ok = fread(buf.data(), 1, sz, fp) ==
				static_cast<size_t>(sz);
			if (ok)
				out.assign(reinterpret_cast<const char *>(buf.data()),
						   buf.size());
			fclose(fp);
		} else {
			ok = false;
		}
	}
	unlink(tmp.c_str());
	return ok;
}

static size_t cc_count_of(const std::string &hay, const char *needle)
{
	size_t n = 0, pos = 0;
	std::string s(needle);
	while ((pos = hay.find(s, pos)) != std::string::npos) {
		++n;
		pos += s.size();
	}
	return n;
}

static bool cc_has(const std::string &hay, const char *needle)
{
	return hay.find(needle) != std::string::npos;
}

/* parse "k:v; k:v" prop lists */
static std::map<std::string, std::string> cc_parse_props(const std::string &s)
{
	std::map<std::string, std::string> out;
	size_t pos = 0;
	while (pos < s.size()) {
		size_t end = s.find("; ", pos);
		std::string kv = s.substr(pos, end == std::string::npos
								  ? end : end - pos);
		size_t colon = kv.find(':');
		if (colon != std::string::npos)
			out[kv.substr(0, colon)] = kv.substr(colon + 1);
		if (end == std::string::npos)
			break;
		pos = end + 2;
	}
	return out;
}

/* every <frame> element's attributes + decoded prop map */
struct CCFrame {
	std::map<std::string, std::string> attrs;
	std::map<std::string, std::string> props;
};

static std::vector<CCFrame> cc_frames_of(const std::string &abwn)
{
	std::vector<CCFrame> out;
	size_t pos = 0;
	while ((pos = abwn.find("<frame ", pos)) != std::string::npos) {
		size_t end = abwn.find('>', pos);
		if (end == std::string::npos)
			break;
		std::string tag = abwn.substr(pos, end - pos);
		CCFrame f;
		size_t apos = 0;
		while ((apos = tag.find("=\"", apos)) != std::string::npos) {
			size_t kstart = tag.rfind(' ', apos);
			std::string key = tag.substr(kstart + 1, apos - kstart - 1);
			size_t vstart = apos + 2;
			size_t vend = tag.find('"', vstart);
			if (vend == std::string::npos)
				break;
			std::string val = tag.substr(vstart, vend - vstart);
			if (key == "props")
				f.props = cc_parse_props(val);
			else
				f.attrs[key] = val;
			apos = vend + 1;
		}
		out.push_back(f);
		pos = end + 1;
	}
	return out;
}

static double cc_dim(const std::map<std::string, std::string> &props,
				  const char *key)
{
	auto it = props.find(key);
	/* UT_convertDimensionless, not atof: an earlier suite may leave
	 * the process on a comma-decimal LC_NUMERIC (gtk_init_check() does
	 * setlocale(LC_ALL,"")), under which atof stops at the '.' and
	 * returns the integer part. */
	return it == props.end() ? -1.0 : UT_convertDimensionless(it->second.c_str());
}

/* shared "abwn in hand" preamble; NULL when fixtures absent */
static bool cc_load_fixture_abwn(const char *name, PD_Document **doc,
							  std::string &abwn)
{
	const char *dir = cc_fixture_dir();
	if (!dir)
		return false;
	*doc = cc_import_cover(dir, name);
	if (!*doc)
		return false;
	if (!cc_export_abwn(*doc, abwn)) {
		(*doc)->unref();
		*doc = nullptr;
		return false;
	}
	return true;
}

// ------------------------------------------------------------------
// corpus presence gate — logs whether the env var reached the suite
// ------------------------------------------------------------------
TFTEST_MAIN("TST05 cover corpus fixture dir gate")
{
	const char *dir = cc_fixture_dir();
	if (!dir) {
		printf("SKIP: COVER_FIXTURES_DIR unset — corpus not run\n");
		return;
	}
	printf("COVER_FIXTURES_DIR=%s\n", dir);
	TFPASS(true);
}

// ------------------------------------------------------------------
// badge-footer.docx (COVER06/07/08): the footer story imports, the
// custGeom scalloped seal lands as a frame shape-path, and the PAGE
// field inside it carries its run's character props
// ------------------------------------------------------------------
TFTEST_MAIN("TST05 badge-footer: footer seal + resolved page field")
{
	PD_Document *doc = nullptr;
	std::string abwn;
	if (!cc_load_fixture_abwn("badge-footer.docx", &doc, abwn)) {
		printf("SKIP: badge-footer.docx unavailable\n");
		return;
	}
	TFPASS(cc_has(abwn, "<section type=\"footer\""));
	TFPASS(cc_has(abwn, "<section type=\"header\""));
	std::vector<CCFrame> fr = cc_frames_of(abwn);
	TFPASS(fr.size() >= 1);
	// the seal: custGeom path + theme accent1 resolved to 4472C4
	bool seal = false;
	for (const CCFrame &f : fr)
		if (f.props.count("shape-path") &&
			f.props.count("background-color") &&
			f.props.at("background-color") == "4472C4")
			seal = true;
	TFPASS(seal);
	// field resolved WITH its run character props (COVER08)
	TFPASS(cc_has(abwn, "<field type=\"page_number\""));
	TFPASS(cc_has(abwn, "font-weight:bold"));
	TFPASS(cc_has(abwn, "char-spacing:1.000000pt"));
	TFPASS(cc_has(abwn, "color:E7E6E6"));
	doc->unref();
}

// ------------------------------------------------------------------
// crop-header / integral-footer / viewmaster-footer-*: anchored
// header/footer shapes + page-number fields land in the hdrftr
// stories, not the body
// ------------------------------------------------------------------
TFTEST_MAIN("TST05 hdrftr fixtures: shapes + page fields")
{
	const char *dir = cc_fixture_dir();
	if (!dir) {
		printf("SKIP: COVER_FIXTURES_DIR unset\n");
		return;
	}
	struct Row { const char *file; int minframes; const char *bg; };
	const Row rows[] = {
		{ "crop-header.docx",                 2, "44546A" },
		{ "integral-footer.docx",             0, "ED7D31" },
		{ "viewmaster-footer-horizontal.docx",3, "000000" },
		{ "viewmaster-footer-vertical.docx",  3, "000000" },
	};
	for (const Row &r : rows) {
		printf("-- fixture %s\n", r.file);
		PD_Document *doc = cc_import_cover(dir, r.file);
		if (!doc) {
			printf("SKIP: %s unavailable\n", r.file);
			continue;
		}
		std::string abwn;
		TFPASS(cc_export_abwn(doc, abwn));
		doc->unref();
		TFPASS(cc_has(abwn, "<section type=\"header\""));
		TFPASS(cc_has(abwn, "<section type=\"footer\""));
		TFPASS(cc_has(abwn, "<field type=\"page_number\""));
		TFPASS(cc_has(abwn, r.bg));
		std::vector<CCFrame> fr = cc_frames_of(abwn);
		TFPASS(static_cast<int>(fr.size()) >= r.minframes);
	}
	// crop-header keeps its gray corner band as a custGeom frame
	PD_Document *doc = cc_import_cover(dir, "crop-header.docx");
	if (doc) {
		std::string abwn;
		TFPASS(cc_export_abwn(doc, abwn));
		TFPASS(cc_has(abwn, "shape-path:"));
		doc->unref();
	}
}

// ------------------------------------------------------------------
// feathered.docx (COVER06): the near-full-page feather art is one
// anchored image whose bottom edge reaches the page bottom (11in)
// ------------------------------------------------------------------
TFTEST_MAIN("TST05 feathered: anchored art keeps bottom extent")
{
	PD_Document *doc = nullptr;
	std::string abwn;
	if (!cc_load_fixture_abwn("feathered.docx", &doc, abwn)) {
		printf("SKIP: feathered.docx unavailable\n");
		return;
	}
	TFPASS(cc_has(abwn, "frame-type:image"));
	TFPASS(cc_has(abwn, "strux-image-dataid="));
	TFPASS(cc_has(abwn, "mime-type=\"image/png\""));
	bool bottom = false;
	for (const CCFrame &f : cc_frames_of(abwn)) {
		auto it = f.props.find("frame-type");
		if (it == f.props.end() || it->second != "image")
			continue;
		double y = cc_dim(f.props, "frame-page-ypos");
		double h = cc_dim(f.props, "frame-height");
		printf("   image frame ypos=%.4f height=%.4f\n", y, h);
		if (y > 0.0 && y + h >= 10.9)
			bottom = true;
	}
	TFPASS(bottom); // art extends to (past) the 11in page bottom
	TFPASS(cc_count_of(abwn, "shape-path:") >= 2); // frame + sidebar custGeom
	doc->unref();
}

// ------------------------------------------------------------------
// facet.docx (COVER05): the blue base band AND the translucent line-
// work overlay image both import — band is a custGeom frame filled
// with resolved accent1, overlay is a frame-type:image
// ------------------------------------------------------------------
TFTEST_MAIN("TST05 facet: blue base band + overlay image")
{
	PD_Document *doc = nullptr;
	std::string abwn;
	if (!cc_load_fixture_abwn("facet.docx", &doc, abwn)) {
		printf("SKIP: facet.docx unavailable\n");
		return;
	}
	bool band = false, overlay = false;
	for (const CCFrame &f : cc_frames_of(abwn)) {
		auto ty = f.props.find("frame-type");
		auto bg = f.props.find("background-color");
		if (f.props.count("shape-path") &&
			bg != f.props.end() && bg->second == "4472C4")
			band = true;
		if (ty != f.props.end() && ty->second == "image" &&
			f.attrs.count("strux-image-dataid"))
			overlay = true;
	}
	TFPASS(band);    // blue custGeom base band, schemeClr resolved
	TFPASS(overlay); // line-work overlay image anchored on top
	TFPASS(cc_has(abwn, "mime-type=\"image/png\""));
	doc->unref();
}

// ------------------------------------------------------------------
// whip.docx: dense custGeom cover — every freeform shape must survive
// ------------------------------------------------------------------
TFTEST_MAIN("TST05 whip: all custGeom shapes import")
{
	PD_Document *doc = nullptr;
	std::string abwn;
	if (!cc_load_fixture_abwn("whip.docx", &doc, abwn)) {
		printf("SKIP: whip.docx unavailable\n");
		return;
	}
	// document.xml declares 23 <a:custGeom>; >= 20 pinned against drops
	TFPASS(cc_count_of(abwn, "shape-path:") >= 20);
	TFPASS(cc_count_of(abwn, "<frame ") >= 25);
	doc->unref();
}

// ------------------------------------------------------------------
// Badge.docx (D01/D02): custGeom seal + the 8pt-tracked title
// ------------------------------------------------------------------
TFTEST_MAIN("TST05 Badge: seal path + 8pt tracked title")
{
	PD_Document *doc = nullptr;
	std::string abwn;
	if (!cc_load_fixture_abwn("Badge.docx", &doc, abwn)) {
		printf("SKIP: Badge.docx unavailable\n");
		return;
	}
	TFPASS(cc_has(abwn, "shape-path:"));
	TFPASS(cc_has(abwn, "char-spacing:8.000000pt"));
	TFPASS(cc_has(abwn, "background-color:4472C4"));
	TFPASS(cc_count_of(abwn, "<frame ") >= 5);
	doc->unref();
}

// ------------------------------------------------------------------
// filgree.docx (D06) + integral.docx: inline images land as data
// items; filgree's flowers carry the duotone blip effect
// ------------------------------------------------------------------
TFTEST_MAIN("TST05 filgree/integral: images import as data items")
{
	PD_Document *doc = nullptr;
	std::string abwn;
	if (cc_load_fixture_abwn("filgree.docx", &doc, abwn)) {
		TFPASSEQ(cc_count_of(abwn, "<image "), 2u);
		TFPASSEQ(cc_count_of(abwn, "mime-type=\"image/png\""), 2u);
		TFPASS(cc_count_of(abwn, "image-duotone:123065 FFFFFF") == 2);
		doc->unref();
	} else {
		printf("SKIP: filgree.docx unavailable\n");
	}
	doc = nullptr;
	abwn.clear();
	if (cc_load_fixture_abwn("integral.docx", &doc, abwn)) {
		TFPASS(cc_count_of(abwn, "<image ") >= 1);
		TFPASS(cc_count_of(abwn, "mime-type=\"image/") >= 1);
		doc->unref();
	} else {
		printf("SKIP: integral.docx unavailable\n");
	}
}

// ------------------------------------------------------------------
// generic sweep — every remaining cover fixture imports to a
// sectioned doc with its anchored frames and design text
// ------------------------------------------------------------------
TFTEST_MAIN("TST05 cover sweep: frames + text survive import")
{
	const char *dir = cc_fixture_dir();
	if (!dir) {
		printf("SKIP: COVER_FIXTURES_DIR unset\n");
		return;
	}
	struct Row {
		const char *file;
		int minframes;
		int minshapepaths;
		const char *text;
	};
	const Row rows[] = {
		{ "Austin.docx",    6, 0, "[Document title]" },
		{ "banded.docx",    3, 0, "[Document title]" },
		{ "Crop.docx",      5, 2, "[Document Title]" },
		{ "headiness.docx", 3, 0, "[Document Title]" },
		{ "ion-dark.docx",  5, 2, "[Document title]" },
		{ "ion-light.docx", 2, 0, "[Document title]" },
		{ "retrospect.docx",3, 0, "[Document title]" },
		{ "semaphore.docx", 5, 0, "[Document title]" },
		{ "slice-dark.docx",2, 0, "[Document title]" },
		{ "slice-light.docx",2, 0,"[Document title]" },
		{ "viewmaster.docx",3, 0, "[Document title]" },
	};
	for (const Row &r : rows) {
		printf("-- fixture %s\n", r.file);
		PD_Document *doc = cc_import_cover(dir, r.file);
		if (!doc) {
			printf("SKIP: %s unavailable\n", r.file);
			continue;
		}
		std::string abwn;
		TFPASS(cc_export_abwn(doc, abwn));
		doc->unref();
		TFPASS(cc_has(abwn, "<section"));
		TFPASS(cc_has(abwn, "<pagesize"));
		TFPASS(static_cast<int>(cc_count_of(abwn, "<frame ")) >= r.minframes);
		TFPASS(static_cast<int>(cc_count_of(abwn, "shape-path:"))
			   >= r.minshapepaths);
		TFPASS(cc_has(abwn, r.text));
	}
}
