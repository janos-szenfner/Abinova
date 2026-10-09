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

/* COVD04 — XHTML importer coverage wave (IE_Imp_TableHelper paths).
 *
 * Drives the table helper the corpus barely reaches: <caption>
 * content, <thead>/<tbody>/<tfoot> zone bookkeeping, colspan/rowspan
 * attach maths, ragged-row padding, nested <table> inside <td> and
 * objects inside cells.  Three piece-table regressions are pinned:
 * caption text (insertSpanBeforeFrag anchoring at PTX_SectionTable),
 * caption/cell objects (insertObjectBeforeFrag anchoring at
 * PTX_SectionTable/PTX_EndCell) and char formats applied through an
 * inserted fmt mark.  An inline leg covers the <font> attribute
 * mappers and the CSS color/decor/vertical-align arms of
 * s_props_append().
 */

#include "tf_test.h"
#include "pd_Document.h"
#include "ie_exp.h"
#include "ie_types.h"

#include <gsf/gsf-output-memory.h>
#include <glib/gstdio.h>

#include <string>

#define TFSUITE "core.wp.impexp.xhtml"

static bool xhtml_write_tmp(const std::string & path, const std::string & data)
{
	return g_file_set_contents(path.c_str(), data.data(),
							   static_cast<gssize>(data.size()), nullptr);
}

static std::string xhtml_tmpname(const char * tag)
{
	return std::string("/tmp/ie_xhtml_") + tag + "_" +
		std::to_string(::getpid()) + ".xhtml";
}

static PD_Document * xhtml_import_string(const char * tag,
										 const std::string & data)
{
	std::string path = xhtml_tmpname(tag);
	if (!xhtml_write_tmp(path, data))
		return nullptr;
	PD_Document * doc = new PD_Document;
	UT_Error err = doc->readFromFile(path.c_str(), IEFT_Unknown, nullptr);
	g_unlink(path.c_str());
	if (!UT_IS_IE_SUCCESS(err)) {
		doc->unref();
		return nullptr;
	}
	return doc;
}

static std::string xhtml_export_abwn(PD_Document * doc)
{
	std::string out;
	GsfOutput * mem = gsf_output_memory_new();
	if (mem &&
		doc->saveAs(mem, static_cast<int>(IE_Exp::fileTypeForSuffix(".abwn")),
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

/* 8x8 red PNG, embedded as a data: URI so no filesystem access is
 * needed. */
static const char xhtml_png_b64[] =
	"iVBORw0KGgoAAAANSUhEUgAAAAgAAAAICAIAAABLbSnc"
	"AAAAEklEQVR4nGP4z8CAFWEXHbQSACj/P8Fu7N9hAAAAAElFTkSuQmCC";

/* A table exercising every helper axis at once: caption with inline
 * format, three zones, colspan/rowspan, a ragged row needing pad
 * cells, an object inside a cell and a nested table. */
static std::string xhtml_table_doc()
{
	std::string s =
		"<html><body>\n"
		"<p>intro</p>\n"
		"<table border=\"1\" cellspacing=\"4\">\n"
		"<caption>Table <b>One</b> Caption</caption>\n"
		"<thead>\n"
		"<tr><th>H1</th><th colspan=\"2\">H2 wide</th></tr>\n"
		"</thead>\n"
		"<tbody>\n"
		"<tr><td rowspan=\"2\">vmerge</td><td>b12</td><td>"
		"<img src=\"data:image/png;base64,";
	s += xhtml_png_b64;
	s += "\" width=\"8\" height=\"8\"/></td></tr>\n"
		"<tr><td>b22</td></tr>\n"
		"<tr><td>c1</td><td colspan=\"2\">c23 wide</td></tr>\n"
		"</tbody>\n"
		"<tfoot>\n"
		"<tr><td colspan=\"3\">foot all</td></tr>\n"
		"</tfoot>\n"
		"</table>\n"
		"<p>between</p>\n"
		"<table><tr><td>outer-left</td><td><table><tr><td>nested-a</td>"
		"<td>nested-b</td></tr></table></td></tr></table>\n"
		"<p>outro</p>\n"
		"</body></html>\n";
	return s;
}

TFTEST_MAIN("xhtml table: caption text and inline format survive")
{
	PD_Document * doc = xhtml_import_string("captab", xhtml_table_doc());
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	std::string abwn = xhtml_export_abwn(doc);
	doc->unref();

	/* the caption paragraph lands before the table, with the bold
	 * inline run intact (was silently dropped: caption content
	 * anchored before the PTX_SectionTable strux) */
	size_t pos = abwn.find("Table ");
	TFPASS(pos != std::string::npos);
	TFPASS(pos < abwn.find("<table"));
	TFPASS(abwn.find("Caption") != std::string::npos);
	TFPASS(abwn.find("font-weight:bold") != std::string::npos);
	TFPASS(abwn.find(">One<") != std::string::npos);
	TFPASS(abwn.find("intro") != std::string::npos);
	TFPASS(abwn.find("between") != std::string::npos);
	TFPASS(abwn.find("outro") != std::string::npos);
}

TFTEST_MAIN("xhtml table: zones, spans and ragged rows")
{
	PD_Document * doc = xhtml_import_string("zones", xhtml_table_doc());
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	std::string abwn = xhtml_export_abwn(doc);
	doc->unref();

	/* thead */
	TFPASS(abwn.find(">H1<") != std::string::npos);
	TFPASS(abwn.find(">H2 wide<") != std::string::npos);
	/* colspan=2 in the header gives right-attach:3 */
	TFPASS(abwn.find("right-attach:3") != std::string::npos);
	/* rowspan=2 body cell runs top:1 to bot:3 */
	TFPASS(abwn.find("bot-attach:3; top-attach:1") != std::string::npos);
	/* tbody cells */
	TFPASS(abwn.find(">b12<") != std::string::npos);
	TFPASS(abwn.find(">b22<") != std::string::npos);
	TFPASS(abwn.find(">c1<") != std::string::npos);
	TFPASS(abwn.find(">c23 wide<") != std::string::npos);
	/* tfoot single cell spans all three columns on row 4 */
	TFPASS(abwn.find(">foot all<") != std::string::npos);
	TFPASS(abwn.find("top-attach:4") != std::string::npos);
	/* the ragged b22 row gained a pad cell: 12 cells in table one */
	int cells = 0;
	for (size_t p = abwn.find("<cell "); p != std::string::npos;
		 p = abwn.find("<cell ", p + 1))
		cells++;
	/* 2 thead + 3 + 2 + 2 tbody + 1 tfoot + 2 outer + 2 nested = 14 */
	TFPASSEQ(cells, 14);
}

TFTEST_MAIN("xhtml table: image object inside a cell")
{
	PD_Document * doc = xhtml_import_string("imgcell", xhtml_table_doc());
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	std::string abwn = xhtml_export_abwn(doc);
	doc->unref();

	/* the <img> inside <td> used to be dropped at the PTX_EndCell
	 * anchor gate; now it lands with a data item behind it */
	TFPASS(abwn.find("<image") != std::string::npos);
	TFPASS(abwn.find("dataid=\"image0\"") != std::string::npos);
	TFPASS(abwn.find("name=\"image0\" mime-type=\"image/png\"") !=
		   std::string::npos);
}

TFTEST_MAIN("xhtml table: nested table inside a cell")
{
	PD_Document * doc = xhtml_import_string("nested", xhtml_table_doc());
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	std::string abwn = xhtml_export_abwn(doc);
	doc->unref();

	TFPASS(abwn.find(">outer-left<") != std::string::npos);
	TFPASS(abwn.find(">nested-a<") != std::string::npos);
	TFPASS(abwn.find(">nested-b<") != std::string::npos);
	int tables = 0;
	for (size_t p = abwn.find("<table"); p != std::string::npos;
		 p = abwn.find("<table", p + 1))
		tables++;
	/* the fixture table, the outer table and the nested one */
	TFPASSEQ(tables, 3);
}

TFTEST_MAIN("xhtml inline: font tag attributes")
{
	std::string s =
		"<html><body>\n"
		"<p><font color=\"#ff0000\" face=\"Courier\" size=\"4\" "
		"background=\"#00ff00\">fonttag</font></p>\n"
		"</body></html>\n";
	PD_Document * doc = xhtml_import_string("font", s);
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	std::string abwn = xhtml_export_abwn(doc);
	doc->unref();

	TFPASS(abwn.find(">fonttag<") != std::string::npos);
	TFPASS(abwn.find("color:ff0000") != std::string::npos);
	TFPASS(abwn.find("bgcolor:00ff00") != std::string::npos);
	TFPASS(abwn.find("font-family:Courier") != std::string::npos);
}

TFTEST_MAIN("xhtml inline: css color arms")
{
	std::string s =
		"<html><body>\n"
		"<p><span style=\"color: rgb(10, 20, 30);\">rgbrun</span></p>\n"
		"<p><span style=\"color: rgb(50%, 100%, 0%);\">rgbpct</span></p>\n"
		"<p><span style=\"color: #abc;\">shorthex</span></p>\n"
		"<p><span style=\"background: #aabbcc;\">bgx</span></p>\n"
		"</body></html>\n";
	PD_Document * doc = xhtml_import_string("csscolor", s);
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	std::string abwn = xhtml_export_abwn(doc);
	doc->unref();

	TFPASS(abwn.find(">rgbrun<") != std::string::npos);
	TFPASS(abwn.find("color:#0a141e") != std::string::npos);
	TFPASS(abwn.find("color:#80ff00") != std::string::npos);
	TFPASS(abwn.find("color:#aabbcc") != std::string::npos);
	TFPASS(abwn.find("bgcolor:aabbcc") != std::string::npos);
}

TFTEST_MAIN("xhtml inline: css decor, valign, weight, align")
{
	std::string s =
		"<html><body>\n"
		"<p><span style=\"text-decoration: underline overline;\">"
		"decor</span></p>\n"
		"<p><span style=\"vertical-align: super;\">supr</span>"
		"<span style=\"vertical-align: sub;\">subt</span></p>\n"
		"<p><span style=\"font-weight: 700;\">w700</span>"
		"<span style=\"font-weight: 400;\">w400</span></p>\n"
		"<p align=\"right\">alright</p>\n"
		"</body></html>\n";
	PD_Document * doc = xhtml_import_string("cssmisc", s);
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	std::string abwn = xhtml_export_abwn(doc);
	doc->unref();

	TFPASS(abwn.find("text-decoration:underline overline") !=
		   std::string::npos);
	TFPASS(abwn.find("text-position:superscript") != std::string::npos);
	TFPASS(abwn.find("text-position:subscript") != std::string::npos);
	TFPASS(abwn.find("font-weight:bold\">w700<") != std::string::npos);
	TFPASS(abwn.find("font-weight:normal\">w400<") != std::string::npos);
	TFPASS(abwn.find("text-align:right") != std::string::npos);
}
