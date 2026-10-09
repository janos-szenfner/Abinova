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

/* TST04 — importer regression fixtures for the landed feature tasks.
 * Each fixture under test/wp/tst04/ pins one feature's observable
 * import behavior by exporting the imported document to .abwn and
 * asserting on the markup (PTO objects, struxes, revision marks,
 * inert change-record attributes).  Regenerate fixtures with
 * tools/mktst04.py (+ mkwpd04.py / mkwpdimg.py for the .wpd ones).
 */

#include "tf_test.h"
#include "pd_Document.h"
#include "ie_exp.h"
#include "ie_imp.h"
#include "ie_types.h"

#include <gsf/gsf-output-stdio.h>
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-infile-zip.h>

#include <cstring>
#include <string>
#include <vector>

#define TFSUITE "core.wp.impexp.fixtures"

static PD_Document *import_file(const char *rel)
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

/* export the document to `suffix` format, returning the bytes */
static bool export_mem(PD_Document * doc, const char * suffix,
					   std::vector<unsigned char> & out)
{
	out.clear();
	std::string tmp = std::string("/tmp/ie_fixtures_") +
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

static bool export_abwn(PD_Document * doc, std::string & out)
{
	std::vector<unsigned char> buf;
	if (!export_mem(doc, ".abwn", buf))
		return false;
	out.assign(reinterpret_cast<const char *>(buf.data()), buf.size());
	return true;
}

/* read one member out of a zip package */
static bool zip_member(const char * zip_path, const char * member,
					   std::string & out)
{
	GError * err = nullptr;
	GsfInput * input = gsf_input_stdio_new(zip_path, &err);
	if (!input)
		return false;
	GsfInfile * zip = gsf_infile_zip_new(input, nullptr);
	g_object_unref(input);
	if (!zip)
		return false;
	/* gsf exposes zip entries hierarchically: word/document.xml is the
	 * child "document.xml" of the "word" directory infile */
	GsfInput * child = GSF_INPUT(zip);
	std::string path(member);
	size_t pos = 0;
	while (true) {
		size_t slash = path.find('/', pos);
		std::string seg = slash == std::string::npos
			? path.substr(pos) : path.substr(pos, slash - pos);
		GsfInput * next =
			gsf_infile_child_by_name(GSF_INFILE(child), seg.c_str());
		if (!GSF_IS_INFILE(next) && slash != std::string::npos)
			next = nullptr;
		if (child != GSF_INPUT(zip))
			g_object_unref(child);
		child = next;
		if (!child || slash == std::string::npos)
			break;
		pos = slash + 1;
	}
	g_object_unref(zip);
	if (!child)
		return false;
	gsf_off_t sz = gsf_input_size(child);
	if (sz <= 0) {
		g_object_unref(child);
		return false;
	}
	std::vector<unsigned char> buf(static_cast<size_t>(sz));
	const guint8 * data = gsf_input_read(child, sz, buf.data());
	bool ok = data != nullptr;
	if (ok)
		out.assign(reinterpret_cast<const char *>(data),
				   static_cast<size_t>(sz));
	g_object_unref(child);
	return ok;
}

static size_t count_of(const std::string & hay, const char * needle)
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
// OXML02: w:altChunk grafts the chunk part instead of a dead link
// ------------------------------------------------------------------
TFTEST_MAIN("OXML02 altChunk html grafts chunk text and formatting")
{
	PD_Document *doc = import_file("/test/wp/tst04/o02_altchunk.docx");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(export_abwn(doc, abwn));
	TFPASS(abwn.find("Before chunk") != std::string::npos);
	TFPASS(abwn.find("Chunk grafted") != std::string::npos);
	TFPASS(abwn.find("BOLDCHUNK") != std::string::npos);
	TFPASS(abwn.find("font-weight:bold") != std::string::npos);
	TFPASS(abwn.find("After chunk") != std::string::npos);
	// placeholder props retained for round-trip
	TFPASS(abwn.find("altchunk-path") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// OXML04: rejected mc:Choice drawings fall back to their picture, and
// fallback-less unsupported payloads leave a visible [kind] marker
// ------------------------------------------------------------------
TFTEST_MAIN("OXML04 chart/SmartArt fallbacks and placeholder markers")
{
	PD_Document *doc = import_file("/test/wp/tst04/o04_fallback.docx");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(export_abwn(doc, abwn));
	// the mc:Fallback v:imagedata picture imported
	TFPASS(abwn.find("<image ") != std::string::npos);
	// and it carries the part link harvested from the rejected Choice
	TFPASS(abwn.find("altcontent-part:word/charts/chart1.xml")
		   != std::string::npos);
	// bare chart + bare SmartArt produce visible markers, not silence
	TFPASS(abwn.find("[chart]") != std::string::npos);
	TFPASS(abwn.find("[diagram]") != std::string::npos);
	TFPASS(abwn.find("altcontent-kind:chart") != std::string::npos);
	TFPASS(abwn.find("altcontent-kind:diagram") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// OXML03/OXML06: run-level tracked changes map to piece-table
// revisions; moves degrade to del+ins keeping the pairing props
// ------------------------------------------------------------------
TFTEST_MAIN("OXML06 docx tracked changes import as revisions")
{
	PD_Document *doc = import_file("/test/wp/tst04/o06_revisions.docx");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(export_abwn(doc, abwn));
	TFPASS(abwn.find("<revisions") != std::string::npos);
	TFPASS(abwn.find("author=\"Alice\"") != std::string::npos);
	TFPASS(abwn.find("author=\"Bob\"") != std::string::npos);
	// adjacent same-author+date insertions share one revision id
	TFPASS(abwn.find("revision=\"+1\">INSAINSB") != std::string::npos);
	TFPASS(abwn.find("revision=\"-2\">DELONE") != std::string::npos);
	TFPASS(abwn.find("revision=\"+3\">INSC") != std::string::npos);
	// moves keep the pairing name + original w:id
	TFPASS(abwn.find("revision-move=\"mv1\"") != std::string::npos);
	TFPASS(abwn.find("revision-move-id") != std::string::npos);
	// deleted paragraph mark degrades to an inert block prop
	TFPASS(abwn.find("para-mark-rev:-6") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// OXML07: strux-level marks — cell ins/del become real revision marks
// on the cell strux; *Change elements land as inert !id{old} records
// ------------------------------------------------------------------
TFTEST_MAIN("OXML07 docx strux revision marks")
{
	PD_Document *doc = import_file("/test/wp/tst04/o07_struxmarks.docx");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(export_abwn(doc, abwn));
	TFPASS(abwn.find("<cell revision=\"+") != std::string::npos);
	TFPASS(abwn.find("<cell revision=\"-") != std::string::npos);
	// deleted cell content is wrapped in the deletion mark, not live
	TFPASS(abwn.find("revision=\"-5\">deleted cell") != std::string::npos);
	// inert pre-change snapshots round-trip as attributes
	TFPASS(abwn.find("pprchange=\"!") != std::string::npos);
	TFPASS(abwn.find("rprchange=\"!") != std::string::npos);
	TFPASS(abwn.find("sectprchange=\"!") != std::string::npos);
	// live state keeps the NEW values (accepted change)
	TFPASS(abwn.find("text-align:center") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// OXML08: exporting a revised doc emits w:ins/w:del + rPrChange with
// author/date from the revision records, not flattened final state
// ------------------------------------------------------------------
TFTEST_MAIN("OXML08 docx export emits revision markup")
{
	PD_Document *doc = import_file("/test/wp/tst04/o06_revisions.docx");
	TFPASS(doc);
	if (!doc)
		return;

	std::string tmp = std::string("/tmp/ie_fixtures_") +
		std::to_string(::getpid()) + ".docx";
	GError * err = nullptr;
	GsfOutput * file = gsf_output_stdio_new(tmp.c_str(), &err);
	bool ok = file &&
		doc->saveAs(file, static_cast<int>(IE_Exp::fileTypeForSuffix(".docx")),
				   false, nullptr) == UT_OK;
	if (file)
		g_object_unref(file);
	doc->unref();
	TFPASS(ok);
	if (!ok) {
		unlink(tmp.c_str());
		return;
	}

	std::string document;
	TFPASS(zip_member(tmp.c_str(), "word/document.xml", document));
	TFPASS(document.find("<w:ins ") != std::string::npos);
	TFPASS(document.find("<w:del ") != std::string::npos);
	TFPASS(document.find("w:author=\"Alice\"") != std::string::npos);
	// move pairing names round-trip back out as moveFrom/moveTo
	TFPASS(document.find("w:moveFrom") != std::string::npos);
	TFPASS(document.find("w:moveTo") != std::string::npos);

	std::string settings;
	if (zip_member(tmp.c_str(), "word/settings.xml", settings))
		TFPASS(settings.find("w:trackChanges") != std::string::npos);
	unlink(tmp.c_str());
}

// ------------------------------------------------------------------
// ODF01: draw:object is math only when the stream is MathML; other
// objects fall back to the ObjectReplacements preview image
// ------------------------------------------------------------------
TFTEST_MAIN("ODF01 object sniffing: math vs preview fallback")
{
	PD_Document *doc = import_file("/test/wp/tst04/o01_objects.odt");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(export_abwn(doc, abwn));
	TFPASS(abwn.find("<math ") != std::string::npos);
	TFPASS(abwn.find("MathLatex") != std::string::npos);
	TFPASS(abwn.find("application/mathml+xml") != std::string::npos);
	// exactly one real image data item: the chart preview (the math
	// object's own preview is suppressed — would be a second one)
	TFPASSEQ(count_of(abwn, "mime-type=\"image/png\""), 1u);
	doc->unref();
}

// ------------------------------------------------------------------
// ODF02: text:tracked-changes changed-regions become AD_Revision
// records + fragment/strux marks; deleted payloads never leak live
// ------------------------------------------------------------------
TFTEST_MAIN("ODF02 tracked changes import as revisions")
{
	PD_Document *doc =
		import_file("/test/wp/odt/trackedchanges/document.odt");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(export_abwn(doc, abwn));
	TFPASS(abwn.find("<revisions") != std::string::npos);
	TFPASS(abwn.find("author=\"Alice Author\"") != std::string::npos);
	TFPASS(abwn.find("revision=\"+1\">inserted text") != std::string::npos);
	TFPASS(abwn.find("revision=\"-2\">deletedword") != std::string::npos);
	TFPASS(abwn.find("para-mark-rev") != std::string::npos);
	// format-change mark
	TFPASS(abwn.find("revision=\"!5{}") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// ODF03: exporting a revised doc emits text:tracked-changes regions
// with office:change-info (author/date) + inline change marks
// ------------------------------------------------------------------
TFTEST_MAIN("ODF03 tracked changes export emits changed-regions")
{
	PD_Document *doc =
		import_file("/test/wp/odt/trackedchanges/document.odt");
	TFPASS(doc);
	if (!doc)
		return;

	std::string tmp = std::string("/tmp/ie_fixtures_") +
		std::to_string(::getpid()) + ".odt";
	GError * err = nullptr;
	GsfOutput * file = gsf_output_stdio_new(tmp.c_str(), &err);
	bool ok = file &&
		doc->saveAs(file, static_cast<int>(IE_Exp::fileTypeForSuffix(".odt")),
				   false, nullptr) == UT_OK;
	if (file)
		g_object_unref(file);
	doc->unref();
	TFPASS(ok);
	if (!ok) {
		unlink(tmp.c_str());
		return;
	}

	std::string content;
	TFPASS(zip_member(tmp.c_str(), "content.xml", content));
	unlink(tmp.c_str());
	TFPASS(content.find("text:tracked-changes") != std::string::npos);
	TFPASS(content.find("changed-region") != std::string::npos);
	TFPASS(content.find("office:change-info") != std::string::npos);
	TFPASS(content.find("Alice Author") != std::string::npos);
}

// ------------------------------------------------------------------
// WP02: header/footer groups import as PTX_SectionHdrFtr struxes
// instead of being swallowed; a footnote inside the header survives
// ------------------------------------------------------------------
TFTEST_MAIN("WP02 wpd headers and footers import")
{
	PD_Document *doc = import_file("/test/wp/tst04/wpd02_hdrftr.wpd");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(export_abwn(doc, abwn));
	TFPASS(abwn.find("type=\"header\"") != std::string::npos);
	TFPASS(abwn.find("type=\"footer\"") != std::string::npos);
	// the body section references both hdrftr ids
	TFPASS(abwn.find("header=\"") != std::string::npos);
	TFPASS(abwn.find("footer=\"") != std::string::npos);
	// the footnote inside the header subdocument was not swallowed
	TFPASS(abwn.find("<foot ") != std::string::npos);
	TFPASS(abwn.find("footnote-id") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// WP03: insertBinaryObject feeds the box's cached PNG through the
// image pipeline; as-char anchors become inline images
// ------------------------------------------------------------------
TFTEST_MAIN("WP03 wpd embedded image imports")
{
	PD_Document *doc = import_file("/test/wp/tst04/wpd_img_char.wpd");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(export_abwn(doc, abwn));
	TFPASS(abwn.find("<image dataid=") != std::string::npos);
	TFPASS(abwn.find("mime-type=\"image/png\"") != std::string::npos);
	doc->unref();

	// the page-anchored box lands as a positioned image frame
	doc = import_file("/test/wp/tst04/wpd_img_page.wpd");
	TFPASS(doc);
	if (!doc)
		return;
	TFPASS(export_abwn(doc, abwn));
	TFPASS(abwn.find("frame-type:image") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// WP04: hyperlink + page-number field + comment + text box all map to
// real piece-table constructs
// ------------------------------------------------------------------
TFTEST_MAIN("WP04 wpd link/field/comment/textbox import")
{
	PD_Document *doc = import_file("/test/wp/tst04/wpd04.wpd");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(export_abwn(doc, abwn));
	TFPASS(abwn.find("xlink:href=\"https://example.com/\"")
		   != std::string::npos);
	TFPASS(abwn.find("type=\"page_number\"") != std::string::npos);
	TFPASS(abwn.find("<ann ") != std::string::npos);
	TFPASS(abwn.find("<annotate") != std::string::npos);
	TFPASS(abwn.find("frame-type:textbox") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// WPCOV: synthetic WP6 kitchen sink (tools/mkwpdcov.py) — doc-summary
// metadata, header/footer variants, ordered+unordered+nested lists,
// foot/endnotes, two tables with cell fills/spans, paragraph and span
// properties, page/column breaks.  The summary packet also pins the
// dc:type-vs-dc:category metadata crash this fixture originally
// exposed.
// ------------------------------------------------------------------
TFTEST_MAIN("WPCOV wpd kitchen sink")
{
	PD_Document *doc = import_file("/test/wp/wpcov/cov_rich.wpd");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(export_abwn(doc, abwn));
	// extended document summary -> document metadata
	TFPASS(abwn.find("<m key=\"dc.creator\">WP Author") != std::string::npos);
	TFPASS(abwn.find("<m key=\"dc.subject\">WP Subject") != std::string::npos);
	TFPASS(abwn.find("<m key=\"dc.publisher\">WP Publisher")
		   != std::string::npos);
	TFPASS(abwn.find("<m key=\"dc.type\">WP Category") != std::string::npos);
	TFPASS(abwn.find("<m key=\"abiword.keywords\">wpk1 wpk2")
		   != std::string::npos);
	TFPASS(abwn.find("<m key=\"dc.description\">WP abstract text")
		   != std::string::npos);
	// header/footer sections incl. the even variant; the footer
	// carries an inline page-number field from the display ref
	TFPASS(abwn.find("type=\"header\"") != std::string::npos);
	TFPASS(abwn.find("type=\"header-even\"") != std::string::npos);
	TFPASS(abwn.find("type=\"footer\"") != std::string::npos);
	TFPASS(abwn.find("type=\"page_number\"") != std::string::npos);
	// lists: bullet list + ordered list + a nested level
	TFPASS(count_of(abwn, "list_label") >= 6);
	TFPASS(abwn.find("list-style:5") != std::string::npos);
	TFPASS(abwn.find("level=\"2\"") != std::string::npos);
	TFPASS(abwn.find("Bullet") != std::string::npos);
	TFPASS(abwn.find("Nested") != std::string::npos);
	// notes
	TFPASS(abwn.find("type=\"footnote_ref\"") != std::string::npos);
	TFPASS(abwn.find("<foot ") != std::string::npos);
	TFPASS(abwn.find("type=\"endnote_ref\"") != std::string::npos);
	TFPASS(abwn.find("<endnote ") != std::string::npos);
	// two tables with cell fill + a spanning attach
	TFPASS(count_of(abwn, "<table") >= 2);
	TFPASS(abwn.find("background-color:ff0000") != std::string::npos);
	TFPASS(abwn.find("background-color:00ff00") != std::string::npos);
	TFPASS(abwn.find("right-attach:2") != std::string::npos);
	// paragraph + span properties
	TFPASS(abwn.find("text-align:center") != std::string::npos);
	TFPASS(abwn.find("text-align:right") != std::string::npos);
	TFPASS(abwn.find("tabstops:") != std::string::npos);
	TFPASS(abwn.find("font-weight:bold") != std::string::npos);
	TFPASS(abwn.find("text-decoration:underline") != std::string::npos);
	TFPASS(abwn.find("text-decoration:line-through") != std::string::npos);
	TFPASS(abwn.find("text-position:superscript") != std::string::npos);
	TFPASS(abwn.find("bgcolor:#ffff00") != std::string::npos);
	TFPASS(abwn.find("color:#ff3333") != std::string::npos);
	TFPASS(abwn.find("font-family:Coverage Serif") != std::string::npos);
	// section/column/page breaks
	TFPASS(abwn.find("columns:2") != std::string::npos);
	TFPASS(abwn.find("<cbr/>") != std::string::npos);
	TFPASS(abwn.find("<pbr/>") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// WPCOV: the same stream inside a structured container
// ("PerfectOffice_MAIN" member of a zip) drives the importer's
// isStructured()/getSubStreamByName() path
// ------------------------------------------------------------------
TFTEST_MAIN("WPCOV wpd structured container")
{
	PD_Document *doc = import_file("/test/wp/wpcov/cov_ole.wpd");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(export_abwn(doc, abwn));
	TFPASS(abwn.find("<m key=\"dc.creator\">WP Author") != std::string::npos);
	TFPASS(abwn.find("Bullet") != std::string::npos);
	TFPASS(abwn.find("type=\"footnote_ref\"") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// MTH01: markdown $...$ and $$...$$ become PTO_Math objects
// ------------------------------------------------------------------
TFTEST_MAIN("MTH01 markdown math becomes PTO_Math")
{
	PD_Document *doc = import_file("/test/wp/tst04/math.md");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(export_abwn(doc, abwn));
	TFPASS(abwn.find("<math ") != std::string::npos);
	TFPASS(abwn.find("display:inline") != std::string::npos);
	TFPASS(abwn.find("display:block") != std::string::npos);
	TFPASS(abwn.find("MathLatex") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// MTH02: LaTeX $...$ and equation environments become PTO_Math objects
// ------------------------------------------------------------------
TFTEST_MAIN("MTH02 latex math becomes PTO_Math")
{
	PD_Document *doc = import_file("/test/wp/tst04/math.tex");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(export_abwn(doc, abwn));
	TFPASS(abwn.find("<math ") != std::string::npos);
	TFPASS(abwn.find("display:inline") != std::string::npos);
	TFPASS(abwn.find("display:block") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// MTH03: markdown export emits dollar-delimited LaTeX for math
// objects (round-trips through MTH01's importer)
// ------------------------------------------------------------------
TFTEST_MAIN("MTH03 markdown export emits math source")
{
	PD_Document *doc = import_file("/test/wp/tst04/math.md");
	TFPASS(doc);
	if (!doc)
		return;
	std::vector<unsigned char> md;
	TFPASS(export_mem(doc, ".md", md));
	std::string text(reinterpret_cast<const char *>(md.data()), md.size());
	// inline math comes back as $...$ latex
	TFPASS(text.find("$x^2") != std::string::npos);
	// display math as $$...$$
	TFPASS(text.find("$$") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// TST09: kitchen-sink RTF drives list tables, list overrides,
// revisions, annotations, cell shading, headers/footers, endnotes and
// field/metadata keyword paths through the RTF importer
// ------------------------------------------------------------------
TFTEST_MAIN("TST09 rtf kitchen sink")
{
	PD_Document *doc = import_file("/test/wp/tst09/kitchen.rtf");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(export_abwn(doc, abwn));
	// lists via listtable + listoverridetable
	TFPASS(count_of(abwn, "list_label") >= 4);
	// tracked changes: insertion + deletion
	TFPASS(abwn.find("<c revision=\"1{") != std::string::npos);
	TFPASS(abwn.find("<c revision=\"-2\"") != std::string::npos);
	// annotation + endnote anchor
	TFPASS(abwn.find("annotation=\"") != std::string::npos);
	TFPASS(abwn.find("endnote_ref") != std::string::npos);
	// tables with cell shading
	TFPASS(count_of(abwn, "<table") >= 2);
	TFPASS(abwn.find("background-color:ff0000") != std::string::npos);
	TFPASS(abwn.find("background-color:00ff00") != std::string::npos);
	// headers/footers incl. even/first variants, page fields
	TFPASS(abwn.find("header-first") != std::string::npos);
	TFPASS(abwn.find("header-even") != std::string::npos);
	TFPASS(abwn.find("footer-first") != std::string::npos);
	TFPASS(abwn.find("type=\"page_number\"") != std::string::npos);
	TFPASS(abwn.find("type=\"page_count\"") != std::string::npos);
	// date/time + metadata fields
	TFPASS(abwn.find("type=\"date\"") != std::string::npos);
	TFPASS(abwn.find("type=\"time\"") != std::string::npos);
	TFPASS(abwn.find("type=\"meta_title\"") != std::string::npos);
	doc->unref();
}
