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

/* COVD08 — pin the libwpd/libwps-based WordPerfect importer
 * (wordperfect/ie_imp_WordPerfect.cpp).  The file fixtures already
 * drive a full libwpd parse; libwpd only ever calls isStructured() +
 * getSubStreamByName() on the stream adapter, so the remaining
 * substream virtuals are exercised directly here on flat, zip and OLE
 * inputs.  The librevenge callbacks are also driven directly against a
 * loading piece table, which reaches the guard/defensive branches real
 * documents never produce (unbalanced close calls, missing properties,
 * field types, frame anchoring, empty text boxes, notes before the
 * first section, ...).
 */

#include "tf_test.h"
#include "ut_bytebuf.h"
#include "pd_Document.h"
#include "ie_imp.h"
#include "ie_exp.h"
#include "ie_imp_WordPerfect.h"

#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-output-stdio.h>
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-outfile-zip.h>
#include <glib.h>

#include <cstring>
#include <string>

#define TFSUITE "core.wp.impexp.wpimp"

static GsfInput * wpimp_open_fixture(const char * rel)
{
	std::string path = TF_Test::get_test_src_dir();
	path += "/";
	path += rel;
	return gsf_input_stdio_new(path.c_str(), nullptr);
}

TFTEST_MAIN("wpd stream adapter exposes container members")
{
	GsfInput * input = wpimp_open_fixture("test/wp/wpcov/cov_ole.wpd");
	TFPASS(input != nullptr);
	if (!input)
		return;

	{
		AbiWordperfectInputStream s(input);
		TFPASS(s.isStructured());
		TFPASS(s.subStreamCount() >= 1);
		const char * n0 = s.subStreamName(0);
		TFPASS(n0 != nullptr);
		/* second lookup walks the m_substreams cache path */
		TFPASS(s.subStreamName(0) == n0);
		TFPASS(s.subStreamName(10000) == nullptr);
		if (n0)
		{
			TFPASS(s.existsSubStream(n0));
			librevenge::RVNGInputStream * byName =
				s.getSubStreamByName(n0);
			TFPASS(byName != nullptr);
			delete byName;
		}
		TFPASS(!s.existsSubStream("no_such_member"));
		TFPASS(s.getSubStreamByName("no_such_member") == nullptr);
		TFPASS(s.getSubStreamById(10000) == nullptr);

		/* by-id member stream supports read/seek/tell/isEnd */
		librevenge::RVNGInputStream * sub = s.getSubStreamById(0);
		TFPASS(sub != nullptr);
		if (sub)
		{
			unsigned long got = 0;
			TFPASS(sub->read(8, got) != nullptr && got > 0);
			TFPASS(sub->tell() > 0);
			TFPASS(!sub->isEnd());
			TFPASS(sub->seek(0, librevenge::RVNG_SEEK_SET) == 0);
			TFPASS(sub->tell() == 0);
			TFPASS(sub->seek(0, librevenge::RVNG_SEEK_END) == 0);
			TFPASS(sub->isEnd());
			TFPASS(sub->seek(-2, librevenge::RVNG_SEEK_CUR) == 0);
			delete sub;
		}

		/* the same ops on the wrapping stream read the container file */
		TFPASS(s.seek(0, librevenge::RVNG_SEEK_SET) == 0);
		unsigned long got = 0;
		TFPASS(s.read(4, got) != nullptr && got == 4);
		TFPASS(s.tell() == 4);
		TFPASS(s.seek(0, librevenge::RVNG_SEEK_END) == 0);
		TFPASS(s.isEnd());
	}
	g_object_unref(input);
}

TFTEST_MAIN("wpd stream adapter on flat input reports no container")
{
	GsfInput * input = wpimp_open_fixture("test/wp/wpcov/cov_rich.wpd");
	TFPASS(input != nullptr);
	if (!input)
		return;

	{
		AbiWordperfectInputStream s(input);
		TFPASS(!s.isStructured());
		TFPASS(s.subStreamCount() == 0);
		TFPASS(s.subStreamName(0) == nullptr);
		TFPASS(!s.existsSubStream("PerfectOffice_MAIN"));
		TFPASS(s.getSubStreamByName("PerfectOffice_MAIN") == nullptr);
		TFPASS(s.getSubStreamById(0) == nullptr);

		/* stream ops still work on the flat bytes — but rewind first:
		 * the container probe reads ahead and leaves the cursor at
		 * EOF (libwpd does the same seek(0,SET) before parsing) */
		TFPASS(s.seek(0, librevenge::RVNG_SEEK_SET) == 0 && s.tell() == 0);
		unsigned long got = 0;
		TFPASS(s.read(4, got) != nullptr && got == 4);
		TFPASS(s.tell() == 4);
		TFPASS(s.seek(0, librevenge::RVNG_SEEK_END) == 0 && s.isEnd());
	}
	g_object_unref(input);
}

TFTEST_MAIN("wpd stream adapter on real OLE container")
{
	/* a true OLE structured storage (any Word 97 binary .doc is a
	 * CFB container) exercises the msole branch of container() —
	 * the zip fixtures only ever reach the zip fallback */
	GsfInput * input =
		wpimp_open_fixture("test/wp/doccov/cov_table.doc");
	TFPASS(input != nullptr);
	if (!input)
		return;

	{
		AbiWordperfectInputStream s(input);
		TFPASS(s.isStructured());
		TFPASS(s.subStreamCount() >= 1);
		TFPASS(s.subStreamName(0) != nullptr);
		/* second lookup walks the m_substreams cache path */
		TFPASS(s.subStreamName(0) == s.subStreamName(0));
		TFPASS(s.subStreamName(5000) == nullptr);
		TFPASS(s.existsSubStream("WordDocument"));
		TFPASS(!s.existsSubStream("PerfectOffice_MAIN"));
		TFPASS(s.getSubStreamByName("no_such_member") == nullptr);
		TFPASS(s.getSubStreamById(5000) == nullptr);
		librevenge::RVNGInputStream * sub =
			s.getSubStreamByName("WordDocument");
		TFPASS(sub != nullptr);
		if (sub)
		{
			unsigned long got = 0;
			TFPASS(sub->read(4, got) != nullptr && got == 4);
			delete sub;
		}
	}
	g_object_unref(input);
}

TFTEST_MAIN("wpd list definition numbering types")
{
	ABI_ListDefinition def(0);
	def.setListType(1, '1');
	TFPASS(def.getListType(1) == NUMBERED_LIST);
	def.setListType(2, 'a');
	TFPASS(def.getListType(2) == LOWERCASE_LIST);
	def.setListType(3, 'A');
	TFPASS(def.getListType(3) == UPPERCASE_LIST);
	def.setListType(4, 'i');
	TFPASS(def.getListType(4) == LOWERROMAN_LIST);
	def.setListType(5, 'I');
	TFPASS(def.getListType(5) == UPPERROMAN_LIST);
	/* an unrecognized code leaves the bulleted default in place */
	def.setListType(6, '?');
	TFPASS(def.getListType(6) == BULLETED_LIST);
}

TFTEST_MAIN("wpd sniffer recognizes flat and structured wpd")
{
	IE_Imp_WordPerfect_Sniffer sn;

	const IE_SuffixConfidence * sc = sn.getSuffixConfidence();
	TFPASS(sc != nullptr);
	TFPASS(sc && sc[0].confidence == UT_CONFIDENCE_PERFECT &&
		   sc[0].suffix == "wpd");

	const char *desc = nullptr, *suff = nullptr;
	IEFileType ft = IEFT_Unknown;
	TFPASS(sn.getDlgLabels(&desc, &suff, &ft));
	TFPASS(desc && strstr(desc, "WordPerfect"));
	TFPASS(suff && strstr(suff, "wpd"));

	GsfInput * flat = wpimp_open_fixture("test/wp/wpcov/cov_rich.wpd");
	TFPASS(flat != nullptr);
	if (flat)
	{
		TFPASS(sn.recognizeContents(flat) == UT_CONFIDENCE_PERFECT);
		g_object_unref(flat);
	}
	GsfInput * zip = wpimp_open_fixture("test/wp/wpcov/cov_ole.wpd");
	TFPASS(zip != nullptr);
	if (zip)
	{
		TFPASS(sn.recognizeContents(zip) == UT_CONFIDENCE_PERFECT);
		g_object_unref(zip);
	}
	static const guint8 garbage[] = "this is not a wordperfect file";
	GsfInput * bad = gsf_input_memory_new(garbage, sizeof(garbage),
										FALSE);
	TFPASS(sn.recognizeContents(bad) == UT_CONFIDENCE_ZILCH);
	g_object_unref(bad);
}

TFTEST_MAIN("wpd container without PerfectOffice_MAIN fails import")
{
	/* libwpd sees a structured package, asks for its main stream,
	 * gets none -> WPD_OLE_ERROR -> UT_IE_IMPORTERROR */
	GsfOutput * mem = gsf_output_memory_new();
	GsfOutfile * zip = gsf_outfile_zip_new(mem, nullptr);
	GsfOutput * child = gsf_outfile_new_child(zip, "NotTheMainStream",
											FALSE);
	gsf_output_write(child, 4,
					 reinterpret_cast<const guint8 *>("xxxx"));
	gsf_output_close(child);
	g_object_unref(child);
	gsf_output_close(GSF_OUTPUT(zip));
	g_object_unref(zip);

	/* the memory output owns its buffer; the input needs its own */
	gsf_off_t size = gsf_output_size(mem);
	guint8 * copy = static_cast<guint8 *>(g_malloc(size));
	memcpy(copy, gsf_output_memory_get_bytes(GSF_OUTPUT_MEMORY(mem)),
		   size);
	g_object_unref(mem);
	GsfInput * input = gsf_input_memory_new(copy, size, TRUE);
	TFPASS(input != nullptr);
	if (!input)
		return;

	PD_Document * doc = new PD_Document;
	doc->createRawDocument();
	UT_Error err = IE_Imp::loadFile(doc, input,
									IE_Imp::fileTypeForSuffix(".wpd"));
	TFPASS(err != UT_OK);
	g_object_unref(input);
	doc->unref();
}

/* a 1x1 PNG, carried base64 like libwpd's office:binary-data */
static const char s_png_b64[] =
	"iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8"
	"z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==";

static librevenge::RVNGPropertyList props_tabstops()
{
	/* tab stops with every alignment + leader variant openParagraph()
	 * knows about */
	librevenge::RVNGPropertyListVector tabs;
	librevenge::RVNGPropertyList t;
	t.insert("style:position", 1.0);
	t.insert("style:type", "right");
	t.insert("style:leader-text", "-");
	tabs.append(t);
	t.clear();
	t.insert("style:position", 2.0);
	t.insert("style:type", "center");
	t.insert("style:leader-text", "_");
	tabs.append(t);
	t.clear();
	t.insert("style:position", 3.0);
	t.insert("style:type", "char");
	tabs.append(t);
	t.clear();
	t.insert("style:position", 4.0);
	/* no type -> default left, no leader -> 0 */
	tabs.append(t);
	librevenge::RVNGPropertyList props;
	props.insert("style:tab-stops", tabs);
	return props;
}

static PD_Document * wpimp_raw_doc()
{
	PD_Document * doc = new PD_Document;
	doc->createRawDocument();
	return doc;
}

/* export a loading doc to .abwn in /tmp and return its text */
static bool wpimp_export_abwn(PD_Document * doc, std::string & out)
{
	std::string tmp = std::string("/tmp/ie_wpimp_") +
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

TFTEST_MAIN("wpd importer callback branches")
{
	PD_Document * doc = wpimp_raw_doc();
	TFPASS(doc != nullptr);
	IE_Imp_WordPerfect imp(doc);

	/* the empty style hooks libwpd fires during real parses */
	librevenge::RVNGPropertyList empty;
	imp.startDocument(empty);
	imp.defineEmbeddedFont(empty);
	imp.definePageStyle(empty);
	imp.defineSectionStyle(empty);
	imp.defineParagraphStyle(empty);
	imp.defineCharacterStyle(empty);
	imp.defineGraphicStyle(empty);

	/* dc:* and librevenge:* spellings of the summary props */
	librevenge::RVNGPropertyList meta;
	meta.insert("librevenge:keywords", "kw1 kw2");
	meta.insert("librevenge:abstract", "the abstract");
	imp.setDocumentMetaData(meta);
	std::string mv;
	TFPASS(doc->getMetaDataProp(PD_META_KEY_KEYWORDS, mv) &&
		   mv == "kw1 kw2");
	TFPASS(doc->getMetaDataProp(PD_META_KEY_DESCRIPTION, mv) &&
		   mv == "the abstract");

	/* a footnote before the first section creates one implicitly */
	imp.openFootnote(empty);
	imp.insertText(librevenge::RVNGString("fn body"));
	imp.closeFootnote();
	imp.openEndnote(empty);
	imp.closeEndnote();

	imp.openSection(empty);

	/* margins that differ from the page span flag a section change */
	librevenge::RVNGPropertyList span;
	span.insert("fo:margin-left", 1.5);
	span.insert("fo:margin-right", 1.5);
	imp.openPageSpan(span);

	/* second section while the first still wants a block */
	imp.openSection(empty);
	imp.openSection(empty);

	/* no text-align -> left; "end" -> right; tab stops; breaks */
	imp.openParagraph(empty);
	librevenge::RVNGPropertyList para;
	para.insert("fo:text-align", "end");
	imp.openParagraph(para);
	imp.openParagraph(props_tabstops());
	librevenge::RVNGPropertyList pb;
	pb.insert("fo:break-before", "page");
	imp.openParagraph(pb);
	pb.clear();
	pb.insert("fo:break-before", "column");
	imp.openParagraph(pb);

	/* a span carrying every mapped char prop */
	librevenge::RVNGPropertyList chr;
	chr.insert("fo:font-weight", "bold");
	chr.insert("fo:font-style", "italic");
	chr.insert("style:text-position", "sub 50%");
	chr.insert("style:text-underline-type", "single");
	chr.insert("style:text-line-through-type", "single");
	chr.insert("style:font-name", "Courier");
	chr.insert("fo:font-size", "12pt");
	chr.insert("fo:color", "#112233");
	chr.insert("fo:background-color", "#ffff00");
	imp.openSpan(chr);
	imp.insertText(librevenge::RVNGString("styled text"));
	imp.insertSpace();
	imp.insertLineBreak();
	imp.insertTab();
	imp.closeSpan();

	/* fields: each supported type, then unknown and missing */
	for (const char * t : { "text:page-number", "text:page-count",
						  "text:date-time", "text:date", "text:time" })
	{
		librevenge::RVNGPropertyList f;
		f.insert("librevenge:field-type", t);
		imp.insertField(f);
	}
	librevenge::RVNGPropertyList f;
	f.insert("librevenge:field-type", "text:bogus");
	imp.insertField(f);
	imp.insertField(empty);

	/* links: missing href, empty href, real pair, extra close */
	imp.openLink(empty);
	f.clear();
	f.insert("xlink:href", "");
	imp.openLink(f);
	f.clear();
	f.insert("xlink:href", "http://example.com/");
	imp.openLink(f);
	imp.closeLink();
	imp.closeLink();

	/* comment + underflowing close */
	imp.openComment(empty);
	imp.insertText(librevenge::RVNGString("note text"));
	imp.closeComment();
	imp.closeComment();

	/* text boxes: an empty one still gets a block, a full one doesn't */
	imp.openFrame(empty);
	imp.openTextBox(empty);
	imp.closeTextBox();
	imp.openFrame(empty);
	imp.openTextBox(empty);
	imp.openParagraph(empty);
	imp.insertText(librevenge::RVNGString("box"));
	imp.closeTextBox();
	imp.closeFrame();

	/* drawing shapes have no mapping — covered as graceful drops */
	imp.drawRectangle(empty);
	imp.drawEllipse(empty);
	imp.drawPolygon(empty);
	imp.drawPolyline(empty);
	imp.drawPath(empty);
	imp.drawConnector(empty);
	imp.insertEquation(empty);

	/* ordered levels in A/i/I formats, nested level, renumber */
	librevenge::RVNGPropertyList lv;
	lv.insert("librevenge:id", 7);
	lv.insert("librevenge:level", 1);
	lv.insert("style:num-format", "A");
	lv.insert("text:start-value", 3);
	lv.insert("style:num-prefix", "(");
	lv.insert("style:num-suffix", ")");
	lv.insert("text:space-before", 0.1);
	lv.insert("text:min-label-width", 0.2);
	imp.openOrderedListLevel(lv);
	imp.openListElement(empty);
	lv.clear();
	lv.insert("librevenge:id", 7);
	lv.insert("librevenge:level", 2);
	lv.insert("style:num-format", "i");
	imp.openOrderedListLevel(lv);
	lv.clear();
	lv.insert("fo:text-indent", 0.15);
	imp.openListElement(lv);
	imp.closeOrderedListLevel();
	imp.closeOrderedListLevel();

	/* unordered levels, nested */
	lv.clear();
	lv.insert("librevenge:id", 8);
	lv.insert("librevenge:level", 1);
	imp.openUnorderedListLevel(lv);
	lv.clear();
	lv.insert("librevenge:id", 8);
	lv.insert("librevenge:level", 2);
	imp.openUnorderedListLevel(lv);
	imp.openListElement(empty);
	imp.closeUnorderedListLevel();
	imp.closeUnorderedListLevel();

	/* a list element with no open level is ignored */
	imp.openListElement(empty);

	/* table: non-"margins" alignment emits fo:margin-left as
	 * table-column-leftpos; plus column vector, spans, borders, fill */
	librevenge::RVNGPropertyList tbl;
	tbl.insert("table:align", "right");
	tbl.insert("fo:margin-left", "0.5in");
	librevenge::RVNGPropertyListVector cols;
	librevenge::RVNGPropertyList c;
	c.insert("style:column-width", "1.5in");
	cols.append(c);
	cols.append(c);
	tbl.insert("librevenge:table-columns", cols);
	imp.openTable(tbl);
	librevenge::RVNGPropertyList cell;
	cell.insert("librevenge:column", 0);
	cell.insert("librevenge:row", 0);
	cell.insert("table:number-columns-spanned", 2);
	cell.insert("table:number-rows-spanned", 1);
	cell.insert("fo:border-left", "0.01inch solid #000000");
	cell.insert("fo:border-right", "0.0inch solid #000000");
	cell.insert("fo:background-color", "#ff0000");
	imp.openTableCell(cell);
	imp.openParagraph(empty);
	imp.insertText(librevenge::RVNGString("cell"));
	imp.openTableRow(empty); /* implicitly ends the open cell */
	cell.clear();
	cell.insert("librevenge:column", 0);
	cell.insert("librevenge:row", 1);
	cell.insert("fo:border-left", "0.0inch solid #000000");
	imp.openTableCell(cell);
	imp.openParagraph(empty);
	imp.insertCoveredTableCell(empty);
	imp.closeTable();

	/* the remaining no-op callbacks */
	imp.closeSection();
	imp.closeParagraph();
	imp.closeSpan();
	imp.closeListElement();
	imp.closeTableRow();
	imp.closeTableCell();
	imp.openGroup(empty);
	imp.closeGroup();

	imp.endDocument();
	doc->finishRawCreation();

	std::string abwn;
	TFPASS(wpimp_export_abwn(doc, abwn));
	TFPASS(abwn.find("styled text") != std::string::npos);
	TFPASS(abwn.find("text-align:left") != std::string::npos);
	TFPASS(abwn.find("text-align:right") != std::string::npos);
	TFPASS(abwn.find("tabstops:") != std::string::npos);
	TFPASS(abwn.find("type=\"page_count\"") != std::string::npos);
	TFPASS(abwn.find("type=\"date_ddmmyy\"") != std::string::npos);
	TFPASS(abwn.find("annotation") != std::string::npos);
	TFPASS(abwn.find("xlink:href=\"http://example.com/\"")
		   != std::string::npos);
	TFPASS(abwn.find("list-style:2") != std::string::npos);
	TFPASS(abwn.find("list-style:3") != std::string::npos);
	TFPASS(abwn.find("right-attach:2") != std::string::npos);
	TFPASS(abwn.find("background-color:ff0000") != std::string::npos);
	TFPASS(abwn.find("table-column-leftpos") != std::string::npos);
	doc->unref();
}

TFTEST_MAIN("wpd importer frame and image branches")
{
	PD_Document * doc = wpimp_raw_doc();
	TFPASS(doc != nullptr);
	IE_Imp_WordPerfect imp(doc);
	librevenge::RVNGPropertyList empty;
	imp.openSection(empty);
	imp.openParagraph(empty);

	/* text boxes: an empty one still gets a block, a full one doesn't */
	imp.openFrame(empty);
	imp.openTextBox(empty);
	imp.closeTextBox();
	imp.openFrame(empty);
	imp.openTextBox(empty);
	imp.openParagraph(empty);
	imp.insertText(librevenge::RVNGString("box"));
	imp.closeTextBox();
	imp.closeFrame();

	/* frames: page-anchored "char" box with wrap */
	librevenge::RVNGPropertyList frame;
	frame.insert("svg:width", 1.0);
	frame.insert("svg:height", 0.5);
	frame.insert("svg:x", 0.25);
	frame.insert("svg:y", 0.25);
	frame.insert("text:anchor-type", "char");
	frame.insert("style:wrap", "dynamic");
	imp.openFrame(frame);
	imp.openTextBox(empty);
	imp.openParagraph(empty);
	imp.insertText(librevenge::RVNGString("anchored box"));
	imp.closeTextBox();
	imp.closeFrame();

	/* insertBinaryObject: missing payload, bad base64, unknown image */
	imp.openFrame(empty);
	imp.insertBinaryObject(empty);
	librevenge::RVNGPropertyList bin;
	bin.insert("office:binary-data", "!!! not base64 !!!");
	imp.insertBinaryObject(bin);
	bin.clear();
	bin.insert("office:binary-data", "aGVsbG8gd29ybGQ="); /* "hello world" */
	imp.insertBinaryObject(bin);

	/* a real PNG with no frame metrics falls back to natural size */
	bin.clear();
	bin.insert("office:binary-data", s_png_b64);
	imp.insertBinaryObject(bin);

	/* as-char anchored box inlines the image */
	librevenge::RVNGPropertyList asChar;
	asChar.insert("text:anchor-type", "as-char");
	imp.openFrame(asChar);
	imp.insertBinaryObject(bin);
	imp.closeFrame();

	imp.endDocument();
	doc->finishRawCreation();

	std::string abwn;
	TFPASS(wpimp_export_abwn(doc, abwn));
	TFPASS(abwn.find("frame-type:textbox") != std::string::npos);
	TFPASS(abwn.find("position-to:page-above-text") != std::string::npos);
	TFPASS(abwn.find("wrap-mode:wrapped-both") != std::string::npos);
	/* images: natural-size + as-char inline */
	TFPASS(abwn.find("image/png") != std::string::npos);
	doc->unref();
}

TFTEST_MAIN("wpd hdrftr capture replays list and image")
{
	PD_Document * doc = wpimp_raw_doc();
	TFPASS(doc != nullptr);
	IE_Imp_WordPerfect imp(doc);
	librevenge::RVNGPropertyList empty;

	/* captured into a scratch document: text, a list (fmtmark frag)
	 * and an inline image — libwpd always opens a paragraph inside
	 * the subdocument first */
	imp.openHeader(empty);
	imp.openParagraph(empty);
	imp.insertText(librevenge::RVNGString("hdr"));
	librevenge::RVNGPropertyList hlv;
	hlv.insert("librevenge:id", 9);
	hlv.insert("librevenge:level", 1);
	hlv.insert("style:num-format", "I");
	imp.openOrderedListLevel(hlv);
	imp.openListElement(empty);
	imp.insertText(librevenge::RVNGString("item"));
	imp.closeOrderedListLevel();
	librevenge::RVNGPropertyList bin;
	bin.insert("office:binary-data", s_png_b64);
	imp.insertBinaryObject(bin);
	imp.closeHeader();
	librevenge::RVNGPropertyList even;
	even.insert("librevenge:occurrence", "even");
	imp.openFooter(even);
	imp.openParagraph(empty);
	imp.insertText(librevenge::RVNGString("ftr"));
	imp.closeFooter();

	/* two sections while hdrftrs are pending: each binds a fresh id */
	imp.openSection(empty);
	imp.openParagraph(empty);
	imp.insertText(librevenge::RVNGString("body"));
	imp.openSection(empty);
	imp.closePageSpan();

	imp.endDocument();
	doc->finishRawCreation();

	std::string abwn;
	TFPASS(wpimp_export_abwn(doc, abwn));
	TFPASS(abwn.find("type=\"header\"") != std::string::npos);
	TFPASS(abwn.find("type=\"footer-even\"") != std::string::npos);
	TFPASS(abwn.find("hdr") != std::string::npos);
	/* the image captured inside the header lands as a data item */
	TFPASS(abwn.find("image/png") != std::string::npos);
	doc->unref();
}

TFTEST_MAIN("wpd hdrftr on a section-less doc gets a seed section")
{
	/* a header on a document that never opens a page-span section:
	 * closePageSpan must synthesize one for the hdrftr to bind to */
	PD_Document * doc = new PD_Document;
	TFPASSEQ(doc->createRawDocument(), UT_OK);
	IE_Imp_WordPerfect imp(doc);
	librevenge::RVNGPropertyList empty;
	imp.openHeader(empty);
	imp.openParagraph(empty);
	imp.insertText(librevenge::RVNGString("orphan header"));
	imp.closeHeader();
	imp.endDocument();
	doc->finishRawCreation();
	doc->unref();
	TFPASS(true);
}

TFTEST_MAIN("wpd pasteFromBuffer declines wpd clipboard data")
{
	PD_Document * doc = new PD_Document;
	TFPASSEQ(doc->newDocument(), UT_OK);
	IE_Imp_WordPerfect imp(doc);
	PT_DocPosition pos = 0;
	TFPASS(doc->getBounds(true, pos));
	PD_DocumentRange dr(doc, pos, pos);
	const UT_uint8 data[] = "x";
	TFPASS(!imp.pasteFromBuffer(&dr, data, 1, nullptr));
	doc->unref();
}
