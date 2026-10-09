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

/* COVD03 — legacy Word97 (.doc) importer coverage wave.
 *
 * Drives the ie_imp_MsWord_97 paths the committed corpus never
 * reaches: PAPX-embedded TAP decoding (sprmTDefTable/TDefTableShdRaw/
 * TCellPadding/TDyaRowHeight/TTableHeader, TCGRF horzMerge+vertMerge),
 * the wv table-depth/vmerges machinery, PlcfLst/PlfLfo list decoding
 * through _useInsertNotAppend'd listids, PlcfBkf/PlcfBkl bookmarks,
 * PlcffndRef/PlcffndTxt footnotes, PlcfendRef/PlcfendTxt endnotes,
 * PlcfandRef/PlcfandTxt + SttbfAtnBkmk/PlcfAtnbkf/PlcfAtnbkl/
 * GrpXstAtnOwners annotation ranges and point comments, and a
 * supported " TOC \o " field.  Fixtures are synthesized by
 * tools/mkdoccov.py into test/wp/doccov/ and asserted via .abwn
 * export.
 */

#include "tf_test.h"
#include "pd_Document.h"
#include "ie_exp.h"
#include "ie_imp.h"
#include "ie_types.h"

#include <gsf/gsf-output-stdio.h>
#include <glib/gstdio.h>

#include <string>
#include <vector>

#define TFSUITE "core.wp.impexp.msdoc"

static PD_Document *doc_import(const char *rel)
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

static bool doc_export_abwn(PD_Document * doc, std::string & out)
{
	out.clear();
	std::string tmp = std::string("/tmp/ie_msdoc_") +
		std::to_string(::getpid()) + ".abwn";
	GError * err = nullptr;
	GsfOutput * file = gsf_output_stdio_new(tmp.c_str(), &err);
	bool ok = file &&
		doc->saveAs(file,
					static_cast<int>(IE_Exp::fileTypeForSuffix(".abwn")),
					false, nullptr) == UT_OK;
	if (file)
		g_object_unref(file);
	if (ok) {
		std::vector<char> buf;
		FILE * fp = fopen(tmp.c_str(), "rb");
		if (fp) {
			fseek(fp, 0, SEEK_END);
			long sz = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			buf.resize(sz);
			ok = fread(buf.data(), 1, sz, fp) == static_cast<size_t>(sz);
			fclose(fp);
			out.assign(buf.begin(), buf.end());
		} else {
			ok = false;
		}
	}
	g_unlink(tmp.c_str());
	return ok;
}

// ------------------------------------------------------------------
// TAP sprms: borders, shading, padding, header row, exact height,
// vertical merge (restart over two rows) and horizontal merge
// ------------------------------------------------------------------
TFTEST_MAIN("doc97 table: borders shading merges")
{
	PD_Document *doc = doc_import("/test/wp/doccov/cov_table.doc");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(doc_export_abwn(doc, abwn));
	// two table struxes: the 2x2 merge table and the hmerge table
	TFPASS(abwn.find("<table ") != std::string::npos);
	// sprmTDefTable cell grid -> two 2in columns
	TFPASS(abwn.find("table-column-props:2.0000in/2.0000in/") != std::string::npos);
	// sprmTDyaRowHeight -720 twips -> exact 0.5in row height
	TFPASS(abwn.find("table-row-height-type:exactly") != std::string::npos);
	TFPASS(abwn.find("height:0.5000in") != std::string::npos);
	// sprmTTableHeader
	TFPASS(abwn.find("header-row:1") != std::string::npos);
	// sprmTDefTableShdRaw: COLORREF 00FF00 -> green, 0000FF -> red
	TFPASS(abwn.find("background-color:00ff00") != std::string::npos);
	TFPASS(abwn.find("background-color:ff0000") != std::string::npos);
	// sprmTCellPadding mask 0x0f, 120 twips on all sides
	TFPASS(abwn.find("cell-margin-top:0.0833in") != std::string::npos);
	TFPASS(abwn.find("cell-margin-left:0.0833in") != std::string::npos);
	// TCGRF borders -> per-cell style/thickness props
	TFPASS(abwn.find("bot-style:1") != std::string::npos);
	TFPASS(abwn.find("top-thickness:0.0139in") != std::string::npos);
	// vertical merge: restart cell spans rows 0..2, covered cell absent
	TFPASS(abwn.find("top-attach:0") != std::string::npos);
	TFPASS(abwn.find("bot-attach:2") != std::string::npos);
	TFPASS(abwn.find("r1c1") != std::string::npos);
	TFPASS(abwn.find("r2c1") == std::string::npos);
	TFPASS(abwn.find("r2c2") != std::string::npos);
	// horizontal merge: one cell spanning attach 0..2, text preserved
	TFPASS(abwn.find("wide cell") != std::string::npos);
	TFPASS(abwn.find("right-attach:2") != std::string::npos);
	TFPASS(abwn.find("tail") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// PlcfLst + PlfLfo: lstid -> lfo overrides, ilvl -> level, nfc ->
// list-style, bullet char in the label
// ------------------------------------------------------------------
TFTEST_MAIN("doc97 lists: lst/lfo/lvl decoding")
{
	PD_Document *doc = doc_import("/test/wp/doccov/cov_list.doc");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(doc_export_abwn(doc, abwn));
	// one <lists> block with an entry per used level
	TFPASS(abwn.find("<lists>") != std::string::npos);
	TFPASS(abwn.find("<l id=\"") != std::string::npos);
	// ilfo paras carry listid + level and the resolved list-style
	TFPASS(abwn.find("listid=\"") != std::string::npos);
	TFPASS(abwn.find("level=\"2\"") != std::string::npos);
	TFPASS(abwn.find("level=\"3\"") != std::string::npos);
	TFPASS(abwn.find("Numbered List") != std::string::npos);
	TFPASS(abwn.find("Lower Case List") != std::string::npos);
	TFPASS(abwn.find("Lower Roman List") != std::string::npos);
	TFPASS(abwn.find("Square List") != std::string::npos);
	// list labels emit a list_label field + tab
	TFPASS(abwn.find("type=\"list_label\"") != std::string::npos);
	// all item text survives
	TFPASS(abwn.find("first item") != std::string::npos);
	TFPASS(abwn.find("second level") != std::string::npos);
	TFPASS(abwn.find("third level") != std::string::npos);
	TFPASS(abwn.find("bullet") != std::string::npos);
	TFPASS(abwn.find("tail") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// PlcfBkf/PlcfBkl + SttbfBkmk bookmarks, PlcffndRef/Txt footnote,
// PlcfendRef/Txt endnote, and a " TOC \o " field
// ------------------------------------------------------------------
TFTEST_MAIN("doc97 bookmarks notes and TOC field")
{
	PD_Document *doc = doc_import("/test/wp/doccov/cov_marks.doc");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(doc_export_abwn(doc, abwn));
	// bookmark 'bmOne' over "text"
	TFPASS(abwn.find("<bookmark type=\"start\" name=\"bmOne\"") != std::string::npos);
	TFPASS(abwn.find("<bookmark type=\"end\" name=\"bmOne\"") != std::string::npos);
	// footnote: ref field + body text in a <foot> object
	TFPASS(abwn.find("type=\"footnote_ref\"") != std::string::npos);
	TFPASS(abwn.find("<foot ") != std::string::npos);
	TFPASS(abwn.find("footnote body") != std::string::npos);
	// endnote: ref field + body text in an <endnote> object
	TFPASS(abwn.find("type=\"endnote_ref\"") != std::string::npos);
	TFPASS(abwn.find("<endnote ") != std::string::npos);
	TFPASS(abwn.find("endnote body") != std::string::npos);
	// supported " TOC \o "1-3" " instruction -> toc strux
	TFPASS(abwn.find("<toc ") != std::string::npos);
	TFPASS(abwn.find("toc-dest-style1") != std::string::npos);
	// surrounding body text intact
	TFPASS(abwn.find("Para one.") != std::string::npos);
	TFPASS(abwn.find("See bookmarked") != std::string::npos);
	TFPASS(abwn.find("too.") != std::string::npos);
	doc->unref();
}

// ------------------------------------------------------------------
// PlcfandRef/PlcfandTxt comments: ranged anchor via ATNBE/lTagBkmk
// plus a point comment; GrpXstAtnOwners author + ATRD initials
// ------------------------------------------------------------------
TFTEST_MAIN("doc97 annotations: ranged and point comments")
{
	PD_Document *doc = doc_import("/test/wp/doccov/cov_annot.doc");
	TFPASS(doc);
	if (!doc)
		return;
	std::string abwn;
	TFPASS(doc_export_abwn(doc, abwn));
	// annotation 0: ranged anchor over "Commented span" (lTagBkmk=7)
	TFPASS(abwn.find("<ann annotation=\"0\"") != std::string::npos);
	TFPASS(abwn.find("annotation-id=\"0\"") != std::string::npos);
	TFPASS(abwn.find("Commented span") != std::string::npos);
	// annotation 1: point comment (lTagBkmk=-1) at the second 0x05
	TFPASS(abwn.find("<ann annotation=\"1\"") != std::string::npos);
	TFPASS(abwn.find("annotation-id=\"1\"") != std::string::npos);
	// author + initials metadata decoded from ATRD/GrpXstAtnOwners
	TFPASS(abwn.find("annotation-author:Devin QA") != std::string::npos);
	TFPASS(abwn.find("annotation-initials:DV") != std::string::npos);
	// both comment bodies from the annotation subdocument
	TFPASS(abwn.find("first comment") != std::string::npos);
	TFPASS(abwn.find("second comment") != std::string::npos);
	// the 0x05 reference marks themselves are swallowed
	TFPASS(abwn.find("Normal text") != std::string::npos);
	TFPASS(abwn.find(" here.") != std::string::npos);
	doc->unref();
}
