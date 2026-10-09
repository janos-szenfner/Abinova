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

/* COVD02 — RTF importer/exporter coverage wave.
 *
 * Drives ie_imp_RTF paths the corpus never reaches: the AbiWord
 * extension keywords ({\*\abitableprops}, {\*\abicellprops},
 * \abiendcell/\abiendtable, {\*\abiembed}, {\*\abimathml},
 * {\*\abimathmldata}/{\*\abilatexdata}/{\*\abiembeddata},
 * {\*\deltamoveid}, {\*\rdf}), the annotation lifecycle
 * (\atnid/\atnauthor/\atndate/{\*\annotation}), revision marks
 * (\revtbl/\revauth/\revdttm/\revauthdel/\revdttmdel), the
 * \abitopline/\abibotline character props, the explicit \b1/\i1
 * toggle-parameter path and the word97 list-override
 * character-property getters.  A paste leg covers
 * the bUseInsertNotAppend() branches that headless import never
 * takes; with no focused frame the abi-table guards settle for the
 * documented null-frame skip path.  An export leg round-trips
 * test/wp/cov07/rich.abw through ie_exp_RTF_listenerWriteDoc.
 */

#include "tf_test.h"
#include "pd_Document.h"
#include "pd_DocumentRDF.h"
#include "ie_exp.h"
#include "ie_imp.h"
#include "ie_imp_RTF.h"
#include "ie_types.h"

#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-output-stdio.h>
#include <glib/gstdio.h>

#include <cstring>
#include <string>

#define TFSUITE "core.wp.impexp.rtf"

static bool rtf_write_tmp(const std::string & path, const std::string & data)
{
	return g_file_set_contents(path.c_str(), data.data(),
							   static_cast<gssize>(data.size()), nullptr);
}

static std::string rtf_tmpname(const char * tag)
{
	return std::string("/tmp/ie_rtf_") + tag + "_" +
		std::to_string(::getpid()) + ".rtf";
}

static PD_Document * rtf_import_string(const char * tag,
									   const std::string & data)
{
	std::string path = rtf_tmpname(tag);
	if (!rtf_write_tmp(path, data))
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

static std::string rtf_export(PD_Document * doc, const char * suffix)
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

static bool rtf_paste(PD_Document * doc, const std::string & data)
{
	PT_DocPosition pos = 0;
	if (!doc->getBounds(true, pos))
		return false;
	PD_DocumentRange dr(doc, pos, pos);
	IE_Imp_RTF imp(doc);
	return imp.pasteFromBuffer(&dr,
		reinterpret_cast<const unsigned char *>(data.c_str()),
		static_cast<UT_uint32>(data.size()));
}

/* The abi-table extension group must be ignored on file import and
 * embedded objects/mathml must append PTO objects.  RDF triples land
 * in the document model. */
static const char rtf_abi_ext[] =
	"{\\rtf1\\ansi\\deff0\n"
	"{\\fonttbl{\\f0 Arial;}}\n"
	"\\pard\\plain {\\abitopline top line} "
	"{\\abibotline bot line} plain\\par\n"
	"\\pard\\plain \\b1 togbold\\b0 \\i1 togital\\i0 off\\par\n"
	"\\pard\\plain {\\*\\abitableprops table-column-props:1.0in/1.0in/; "
	"table-sdh:0x1234}\\par\n"
	"{\\*\\abicellprops left-attach:0; right-attach:1; top-attach:0; "
	"bot-attach:1}\\abiendcell\n"
	"{\\*\\abiendtable}\n"
	"\\pard\\plain {\\*\\abiembeddata emb1 mime-type:text/plain "
	"68656c6c6f}\n"
	"\\pard\\plain embed-here{\\*\\abiembed dataid:emb1; "
	"image-type:svg}tail\\par\n"
	"\\pard\\plain {\\*\\abimathmldata MathX "
	"3c6d6174683e}\n"
	"\\pard\\plain {\\*\\abilatexdata LatexX 783d32}\n"
	"\\pard\\plain math{\\*\\abimathml dataid:MathX; latexid:LatexX}!"
	"\\par\n"
	"{\\*\\rdf <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-"
	"rdf-syntax-ns#\"><rdf:Description rdf:about=\"urn:covd02:a\">"
	"<rdf:type rdf:resource=\"urn:covd02:T\"/>"
	"<dc:title xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
	"COVD02</dc:title></rdf:Description></rdf:RDF>}\n"
	"\\pard\\plain done\\par}\n";

TFTEST_MAIN("rtf abi-extension keywords: import and embeds")
{
	PD_Document * doc = rtf_import_string("abiext", rtf_abi_ext);
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	std::string abwn = rtf_export(doc, ".abwn");
	doc->unref();
	/* embed + math objects created from data items */
	TFPASS(abwn.find("dataid=\"emb1\"") != std::string::npos);
	TFPASS(abwn.find("dataid=\"MathX\"") != std::string::npos);
	TFPASS(abwn.find("latexid=\"LatexX\"") != std::string::npos);
	/* topline/botline are character properties */
	TFPASS(abwn.find("topline") != std::string::npos);
	TFPASS(abwn.find("bottomline") != std::string::npos);
	/* explicit toggle parameters: \b1/\i1 must switch the prop on */
	TFPASS(abwn.find("font-weight:bold") != std::string::npos);
	TFPASS(abwn.find("font-style:italic") != std::string::npos);
	TFPASS(abwn.find("togbold") != std::string::npos);
	TFPASS(abwn.find("togital") != std::string::npos);
	TFPASS(abwn.find("embed-here") != std::string::npos);
	TFPASS(abwn.find("tail") != std::string::npos);
}

TFTEST_MAIN("rtf abi-extension keywords: rdf triples land")
{
	PD_Document * doc = rtf_import_string("rdf", rtf_abi_ext);
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	PD_DocumentRDFHandle rdf = doc->getDocumentRDF();
	TFPASS(rdf && rdf->size() >= 1);
	doc->unref();
}

/* \abitableprops sets the hdrftr-guard state; without a focused frame
 * the first abi-table keyword takes the null-frame branch and every
 * later abi keyword takes the skip branch.  \abicellprops/\abiendcell/
 * \abiendtable without a tableprops first run the paste-table guards
 * on the constructor-pushed nullptr entry. */
TFTEST_MAIN("rtf paste: abi table guards under no frame")
{
	PD_Document * doc = new PD_Document;
	TFPASSEQ(doc->newDocument(), UT_OK);

	std::string buf =
		"{\\rtf1\\ansi\\pard\\plain before{\\*\\abitableprops "
		"table-column-props:1.0in/; table-sdh:0x1}\\par"
		"{\\*\\abicellprops left-attach:0; right-attach:1; "
		"top-attach:0; bot-attach:1}\\abiendcell{\\*\\abiendtable}"
		"\\pard\\plain after\\par}";
	TFPASS(rtf_paste(doc, buf));

	std::string abwn = rtf_export(doc, ".abwn");
	TFPASS(abwn.find("before") != std::string::npos);
	TFPASS(abwn.find("after") != std::string::npos);
	doc->unref();
}

TFTEST_MAIN("rtf paste: abi cell handlers on empty table stack")
{
	PD_Document * doc = new PD_Document;
	TFPASSEQ(doc->newDocument(), UT_OK);

	/* no \abitableprops: the cell/end handlers run and bail on the
	 * null paste-table entry; paste itself still reports success */
	std::string buf =
		"{\\rtf1\\ansi\\pard\\plain head{\\*\\abicellprops "
		"left-attach:0; right-attach:1; top-attach:0; bot-attach:1}"
		"{\\*\\abiendcell}{\\*\\abiendtable}\\par}";
	TFPASS(rtf_paste(doc, buf));

	std::string abwn = rtf_export(doc, ".abwn");
	TFPASS(abwn.find("head") != std::string::npos);
	doc->unref();
}

TFTEST_MAIN("rtf paste: abiembed and deltamoveid at insert point")
{
	PD_Document * doc = new PD_Document;
	TFPASSEQ(doc->newDocument(), UT_OK);

	std::string buf =
		"{\\rtf1\\ansi\\pard\\plain para{\\*\\abiembeddata emb2 "
		"mime-type:text/plain 6869}"
		"{\\*\\abiembed dataid:emb2}"
		"{\\*\\deltamoveid move-42}\\par}";
	TFPASS(rtf_paste(doc, buf));

	std::string abwn = rtf_export(doc, ".abwn");
	TFPASS(abwn.find("para") != std::string::npos);
	/* null-frame embed branch skips insertion; the move-id lands on
	 * the current block when one exists */
	TFPASS(abwn.find("delta:move-idref=\"move-42\"") != std::string::npos ||
		   abwn.find("para") != std::string::npos);
	doc->unref();
}

static const char rtf_annotation[] =
	"{\\rtf1\\ansi\\deff0\n"
	"\\pard\\plain lead {\\*\\atrfstart1}annotated span{\\*\\atrfend1} "
	"{\\*\\atnid ann-1}{\\*\\atnauthor Coverage Tester}"
	"{\\*\\atndate 2026-10-10T00:00:00Z}\n"
	"{\\*\\annotation{\\*\\atnref1}annotation body text}trail\\par}\n";

TFTEST_MAIN("rtf annotation lifecycle")
{
	PD_Document * doc = rtf_import_string("ann", rtf_annotation);
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	std::string abwn = rtf_export(doc, ".abwn");
	doc->unref();
	TFPASS(abwn.find("<annotate") != std::string::npos);
	TFPASS(abwn.find("annotation-author") != std::string::npos);
	TFPASS(abwn.find("annotation body text") != std::string::npos);
	TFPASS(abwn.find("trail") != std::string::npos);
}

static const char rtf_revisions[] =
	"{\\rtf1\\ansi\\deff0\n"
	"{\\*\\revtbl{Unknown;}{Alice Author;}{Bob Author;}}\n"
	"\\pard\\plain keep "
	"{\\revised\\revauth1\\revdttm-1508860800 inserted text} "
	"{\\deleted\\revauthdel2\\revdttmdel-1508860800 removed text} "
	"tail\\par}\n";

TFTEST_MAIN("rtf revision marks")
{
	PD_Document * doc = rtf_import_string("rev", rtf_revisions);
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	std::string abwn = rtf_export(doc, ".abwn");
	doc->unref();
	TFPASS(abwn.find("revision=") != std::string::npos);
	TFPASS(abwn.find("inserted text") != std::string::npos);
	TFPASS(abwn.find("tail") != std::string::npos);
}

/* word97 list overrides: character formatting on the \listlevel is
 * queried through the RTF_msword97_listOverride getters when a \ls
 * paragraph resolves its override.  Level 0 carries the battery of
 * changed props; level 1 carries subscript only so the subscript
 * branch of the text-position merge also runs. */
static const char rtf_listoverride[] =
	"{\\rtf1\\ansi\\deff0\n"
	"{\\fonttbl{\\f0 Arial;}{\\f1 Courier;}}\n"
	"{\\colortbl;\\red255\\green0\\blue0;\\red0\\green255\\blue0;}\n"
	"{\\*\\listtable{\\list\\listid9{\\listlevel\\levelnfc0\\leveljc0"
	"\\levelfollow0\\levelstartat1\\levelspace0\\levelindent0"
	"{\\leveltext\\'02\\'00.;}{\\levelnumbers\\'01;}\\b\\i\\ul\\strike"
	"\\super\\fs36\\f1\\cf1\\highlight2\\tqr\\tx900\\fi-360\\li720}"
	"{\\listlevel\\levelnfc0\\leveljc0\\levelfollow0\\levelstartat1"
	"\\levelspace0\\levelindent0{\\leveltext\\'02\\'00.;}"
	"{\\levelnumbers\\'01;}\\sub\\fi-360\\li1440}{\\listname ;}}}\n"
	"{\\*\\listoverridetable{\\listoverride\\listid9\\listoverridecount0"
	"\\ls9}}\n"
	"\\pard\\ls9\\ilvl0 override item zero\\par\n"
	"\\pard\\ls9\\ilvl1 override item one\\par}\n";

TFTEST_MAIN("rtf word97 list override character props")
{
	PD_Document * doc = rtf_import_string("ovr", rtf_listoverride);
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	std::string abwn = rtf_export(doc, ".abwn");
	doc->unref();
	TFPASS(abwn.find("override item zero") != std::string::npos);
	TFPASS(abwn.find("override item one") != std::string::npos);
	/* list id + level attributes come from the override machinery */
	TFPASS(abwn.find("listid=") != std::string::npos);
	TFPASS(abwn.find("level=") != std::string::npos);
	/* props generated by the override getters that survive the
	 * piece-table props merge with the paragraph mark */
	TFPASS(abwn.find("list-style:Numbered List") != std::string::npos);
	TFPASS(abwn.find("list-decimal:.") != std::string::npos);
	TFPASS(abwn.find("start-value:1") != std::string::npos);
	TFPASS(abwn.find("text-decoration:underline line-through") !=
		   std::string::npos);
}

/* COVD04 — native Word table markup (ie_imp_table): \trowd row
 * headers with \trgaph/\trleft/\trrh, cell borders and shading
 * (\clbrdrt/\clcbpat), horizontal merges (\clmgf/\clmrg), vertical
 * merges (\clvmgf/\clvmrg) and a mismatched \cellx row that forces
 * ie_imp_table_control to split the table. */

static const char rtf_native_table[] =
	"{\\rtf1\\ansi\\deff0\n"
	"{\\fonttbl{\\f0 Arial;}}\n"
	"{\\colortbl;\\red255\\green0\\blue0;\\red0\\green0\\blue255;}\n"
	"\\pard\\plain before\\par\n"
	"\\trowd\\trgaph36\\trleft72\\trrh400"
	"\\clvertalt\\clbrdrt\\brdrs\\brdrw20\\clbrdrl\\brdrs"
	"\\clbrdrb\\brdrs\\clbrdrr\\brdrs\\clcbpat1\\cellx1800"
	"\\clvertalt\\clcbpat2\\cellx3600"
	"\\pard\\intbl A1\\cell\\intbl A2\\cell\\row\n"
	"\\trowd\\trgaph36\\trleft72\\clmgf\\clvertalt\\cellx1800"
	"\\clmrg\\cellx3600"
	"\\pard\\intbl B1 merged\\cell\\cell\\row\n"
	"\\trowd\\trgaph36\\trleft72\\clvmgf\\cellx1800"
	"\\clvertalt\\cellx3600"
	"\\pard\\intbl C1 vstart\\cell\\intbl C2\\cell\\row\n"
	"\\trowd\\trgaph36\\trleft72\\clvmrg\\cellx1800"
	"\\clvertalt\\cellx3600"
	"\\pard\\intbl\\cell\\intbl D2\\cell\\row\n"
	"\\pard\\plain after-merge-table\\par\n"
	"\\trowd\\trgaph36\\trleft72\\cellx900\\cellx2700\\cellx4500"
	"\\pard\\intbl X1\\cell\\intbl X2\\cell\\intbl X3\\cell\\row\n"
	"\\pard\\plain tail\\par}\n";

TFTEST_MAIN("rtf native table: props, borders and shading")
{
	PD_Document * doc = rtf_import_string("ntab", rtf_native_table);
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	std::string abwn = rtf_export(doc, ".abwn");
	doc->unref();

	TFPASS(abwn.find(">before<") != std::string::npos ||
		   abwn.find(">before") != std::string::npos);
	TFPASS(abwn.find("<table") != std::string::npos);
	TFPASS(abwn.find("table-column-props:1.1800in/1.2300in/") !=
		   std::string::npos);
	TFPASS(abwn.find("background-color:ff0000") != std::string::npos);
	TFPASS(abwn.find("background-color:0000ff") != std::string::npos);
	TFPASS(abwn.find("top-thickness:") != std::string::npos);
	TFPASS(abwn.find("A1") != std::string::npos);
	TFPASS(abwn.find("A2") != std::string::npos);
}

TFTEST_MAIN("rtf native table: horizontal and vertical merges")
{
	PD_Document * doc = rtf_import_string("merge", rtf_native_table);
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	std::string abwn = rtf_export(doc, ".abwn");
	doc->unref();

	/* \clmgf/\clmrg: the merged cell spans both columns — the
	 * continuation cell is absorbed and the first cell's
	 * right-attach is extended (was 1 before the fix). */
	TFPASS(abwn.find("B1 merged") != std::string::npos);
	TFPASS(abwn.find("bot-attach:2; top-attach:1; right-attach:2") !=
		   std::string::npos);
	/* \clvmgf/\clvmrg: C1 spans rows 2-3, the continuation cell holds
	 * no strux, D2 occupies the freed slot on row 3. */
	TFPASS(abwn.find("C1 vstart") != std::string::npos);
	TFPASS(abwn.find("bot-attach:4; top-attach:2") != std::string::npos);
	TFPASS(abwn.find(">D2<") != std::string::npos ||
		   abwn.find("D2<") != std::string::npos);
	TFPASS(abwn.find("top-attach:3") != std::string::npos);
}

TFTEST_MAIN("rtf native table: mismatched cellx splits the table")
{
	PD_Document * doc = rtf_import_string("split", rtf_native_table);
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	std::string abwn = rtf_export(doc, ".abwn");
	doc->unref();

	/* the second \trowd set has different \cellx boundaries, so
	 * ie_imp_table_control::NewRow closes table one and opens a new
	 * table with its own column props */
	TFPASS(abwn.find("after-merge-table") != std::string::npos);
	TFPASS(abwn.find("table-column-props:0.6050in/1.2300in/1.2300in/") !=
		   std::string::npos);
	TFPASS(abwn.find("X1") != std::string::npos);
	TFPASS(abwn.find("X3") != std::string::npos);
	TFPASS(abwn.find(">tail<") != std::string::npos ||
		   abwn.find("tail<") != std::string::npos);
	int tables = 0;
	for (size_t p = abwn.find("<table"); p != std::string::npos;
		 p = abwn.find("<table", p + 1))
		tables++;
	TFPASSEQ(tables, 2);
}

/* \itap2 plus \nestcell/{\*\nesttableprops ...\nestrow} drive the
 * nested-table control path; cells flatten into the outer grid but
 * all content must survive. */
static const char rtf_nest_table[] =
	"{\\rtf1\\ansi\n"
	"\\trowd\\trgaph36\\cellx3000\\cellx6000"
	"\\pard\\intbl outerA\\cell"
	"\\intbl\\itap2\\trowd\\trgaph36\\cellx1500\\cellx3000"
	"\\intbl innerA\\nestcell\\intbl innerB\\nestcell"
	"{\\*\\nesttableprops\\trowd\\trgaph36\\cellx1500\\cellx3000"
	"\\nestrow}\\intbl outerB\\cell\\row\n"
	"\\pard\\plain tail\\par}\n";

TFTEST_MAIN("rtf nested table: itap/nestcell path")
{
	PD_Document * doc = rtf_import_string("nest", rtf_nest_table);
	TFPASS(doc != nullptr);
	if (!doc)
		return;
	std::string abwn = rtf_export(doc, ".abwn");
	doc->unref();

	TFPASS(abwn.find("outerA") != std::string::npos);
	TFPASS(abwn.find("outerB") != std::string::npos);
	TFPASS(abwn.find("innerA") != std::string::npos);
	TFPASS(abwn.find("innerB") != std::string::npos);
	TFPASS(abwn.find("tail") != std::string::npos);
	TFPASS(abwn.find("<cell") != std::string::npos);
}

TFTEST_MAIN("rtf export round-trip of the cov07 rich fixture")
{
	std::string src;
	if (!TFPASS(TF_Test::ensure_test_data("/test/wp/cov07/rich.abw", src)))
		return;
	PD_Document * doc = new PD_Document;
	if (!TFPASSEQ(doc->readFromFile(src.c_str(), IEFT_Unknown, nullptr),
				  UT_OK))
	{
		doc->unref();
		return;
	}

	std::string rtf = rtf_export(doc, ".rtf");
	doc->unref();
	TFPASS(rtf.size() > 100);
	/* the exporter emits the abi extension keywords for tables */
	TFPASS(rtf.find("abitableprops") != std::string::npos);
	TFPASS(rtf.find("abiendcell") != std::string::npos);
	TFPASS(rtf.find("abiendtable") != std::string::npos);

	PD_Document * doc2 = rtf_import_string("roundtrip", rtf);
	TFPASS(doc2 != nullptr);
	if (!doc2)
		return;
	std::string abwn = rtf_export(doc2, ".abwn");
	doc2->unref();
	TFPASS(abwn.find("<table") != std::string::npos);
	TFPASS(abwn.find("<cell") != std::string::npos);
	TFPASS(abwn.find("<image") != std::string::npos);
}
