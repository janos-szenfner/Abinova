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

/* SEC07 — XXE local-file-disclosure regression pin.
 *
 * Every document-facing XML import funnels through UT_XML::parse, which
 * installs UT_XML_UntrustedParseScope around the libxml2 parse.  These
 * tests exercise each importer family SEC07 names (.abw, .xhtml, .odt,
 * .docx) — plus an external-parameter-entity .abwn variant — with a
 * hostile entity pointing at a local canary file and assert the canary
 * can never reach the piece table.  A clean importer rejection counts
 * as a pass; silently substituting entity text is a failure.  Benign
 * controls prove each fixture is otherwise well-formed and imports.
 */

#include "tf_test.h"
#include "pd_Document.h"
#include "ie_exp.h"
#include "ie_imp.h"
#include "ie_types.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gsf/gsf-output-stdio.h>
#include <gsf/gsf-outfile-zip.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <unistd.h>

#define TFSUITE "core.wp.impexp.xxe"

static const char xxe_secret[] = "XXE-CANARY-SECRET-7f9d2b";
static const char xxe_marker[] = "xxe-visible-marker";

static bool xxe_write(const std::string & path, const std::string & data)
{
	return g_file_set_contents(path.c_str(), data.data(),
							   static_cast<gssize>(data.size()), nullptr);
}

/* write a zip package; when stored_first is set the first member is
 * written uncompressed (the ODF mimetype rule) */
static bool xxe_make_zip(
	const std::string & path,
	const std::vector<std::pair<std::string, std::string>> & members,
	bool stored_first)
{
	GError * err = nullptr;
	GsfOutput * out = gsf_output_stdio_new(path.c_str(), &err);
	if (!out)
		return false;
	GsfOutfile * zip = gsf_outfile_zip_new(out, nullptr);
	g_object_unref(out);
	if (!zip)
		return false;
	bool ok = true;
	bool first = true;
	for (const auto & m : members) {
		GsfOutput * child = (first && stored_first)
			? gsf_outfile_new_child_full(zip, m.first.c_str(), FALSE,
										 "compression-level", 0, nullptr)
			: gsf_outfile_new_child(zip, m.first.c_str(), FALSE);
		first = false;
		ok = child &&
			gsf_output_write(child, m.second.size(),
							 reinterpret_cast<const guint8 *>(m.second.data())) &&
			gsf_output_close(child);
		if (child)
			g_object_unref(child);
		if (!ok)
			break;
	}
	ok = gsf_output_close(GSF_OUTPUT(zip)) && ok;
	g_object_unref(zip);
	return ok;
}

static bool xxe_export_abwn(PD_Document * doc, std::string & out)
{
	out.clear();
	std::string tmp = std::string("/tmp/ie_xxe_") +
		std::to_string(::getpid()) + ".abwn";
	GError * err = nullptr;
	GsfOutput * file = gsf_output_stdio_new(tmp.c_str(), &err);
	bool ok = file &&
		doc->saveAs(file, static_cast<int>(IE_Exp::fileTypeForSuffix(".abwn")),
					false, nullptr) == UT_OK;
	if (file)
		g_object_unref(file);
	if (ok) {
		gchar * contents = nullptr;
		gsize len = 0;
		ok = g_file_get_contents(tmp.c_str(), &contents, &len, nullptr);
		if (ok) {
			out.assign(contents, len);
			g_free(contents);
		}
	}
	unlink(tmp.c_str());
	return ok;
}

/* Import `doc`, export to .abwn, then check the exported text:
 *  - `absent` must never appear (a leaked entity shows up verbatim)
 *  - `present`, when set, must appear — and the import must succeed
 * A failed import is acceptable only when `present` is null.
 */
static bool xxe_import_check(const std::string & doc, const char * absent,
							 const char * present)
{
	PD_Document * d = new PD_Document;
	UT_Error err = d->readFromFile(doc.c_str(), IEFT_Unknown, nullptr);
	if (err != UT_OK) {
		d->unref();
		return present == nullptr;
	}
	std::string out;
	bool ok = xxe_export_abwn(d, out);
	d->unref();
	if (!ok || (absent && out.find(absent) != std::string::npos))
		return false;
	return !present || out.find(present) != std::string::npos;
}

static std::string xxe_abw(const std::string & uri, bool hostile)
{
	std::string body = hostile ? "&canary;" : xxe_marker;
	std::string dtd = hostile
		? "<!DOCTYPE abiword [\n<!ENTITY canary SYSTEM \"" + uri + "\">\n]>\n"
		: "";
	return
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + dtd +
		"<abiword template=\"false\" "
		"xmlns:aw=\"http://www.abisource.com/awml.dtd\">\n"
		"<section>\n<p>" + body + "</p>\n</section>\n</abiword>\n";
}

/* external parameter entity: the canary file itself is a DTD fragment
 * defining the entity the document then references — the classic
 * file-exfiltration vector */
static std::string xxe_abw_pe(const std::string & uri)
{
	return
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!DOCTYPE abiword [\n<!ENTITY % ext SYSTEM \"" + uri + "\">\n%ext;\n]>\n"
		"<abiword template=\"false\" "
		"xmlns:aw=\"http://www.abisource.com/awml.dtd\">\n"
		"<section>\n<p>&xxe_injected;</p>\n</section>\n</abiword>\n";
}

static std::string xxe_xhtml(const std::string & uri, bool hostile)
{
	std::string body = hostile ? "&canary;" : xxe_marker;
	std::string dtd = hostile
		? "<!DOCTYPE html [\n<!ENTITY canary SYSTEM \"" + uri + "\">\n]>\n"
		: "";
	return
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + dtd +
		"<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
		"<head><title>xxe</title></head>\n"
		"<body><p>" + body + "</p></body>\n</html>\n";
}

static std::string xxe_odt_content(const std::string & uri, bool hostile)
{
	std::string body = hostile ? "&canary;" : xxe_marker;
	std::string dtd = hostile
		? "<!DOCTYPE office:document-content [\n"
		  "<!ENTITY canary SYSTEM \"" + uri + "\">\n]>\n"
		: "";
	return
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + dtd +
		"<office:document-content "
		"xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
		"xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" "
		"office:version=\"1.2\">\n"
		"<office:body><office:text><text:p>" + body +
		"</text:p></office:text></office:body>\n"
		"</office:document-content>\n";
}

static std::string xxe_docx_document(const std::string & uri, bool hostile)
{
	std::string body = hostile ? "&canary;" : xxe_marker;
	std::string dtd = hostile
		? "<!DOCTYPE w:document [\n"
		  "<!ENTITY canary SYSTEM \"" + uri + "\">\n]>\n"
		: "";
	return
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + dtd +
		"<w:document "
		"xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">\n"
		"<w:body><w:p><w:r><w:t>" + body +
		"</w:t></w:r></w:p></w:body>\n</w:document>\n";
}

static const char xxe_odt_manifest[] =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	"<manifest:manifest "
	"xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\">\n"
	"<manifest:file-entry manifest:full-path=\"/\" "
	"manifest:media-type=\"application/vnd.oasis.opendocument.text\"/>\n"
	"</manifest:manifest>\n";

static const char xxe_docx_types[] =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
	"<Default Extension=\"rels\" "
	"ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
	"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
	"<Override PartName=\"/word/document.xml\" "
	"ContentType=\"application/vnd.openxmlformats-officedocument."
	"wordprocessingml.document.main+xml\"/>\n"
	"</Types>\n";

static const char xxe_docx_rels[] =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	"<Relationships "
	"xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
	"<Relationship Id=\"rId1\" "
	"Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
	"relationships/officeDocument\" Target=\"word/document.xml\"/>\n"
	"</Relationships>\n";

TFTEST_MAIN("SEC07: external entities never resolve to local files")
{
	gchar * dir = g_dir_make_tmp("ie_xxe_XXXXXX", nullptr);
	TFPASS(dir != nullptr);
	if (!dir)
		return;

	const std::string canary = std::string(dir) + "/canary.txt";
	const std::string canary_uri = "file://" + canary;
	const std::string canary_dtd = std::string(dir) + "/canary.dtd";
	const std::string canary_dtd_uri = "file://" + canary_dtd;
	const std::string secret = xxe_secret;

	/* the canary is real and readable — a vulnerable loader would
	 * resolve it */
	TFPASS(xxe_write(canary, secret));
	TFPASS(xxe_write(canary_dtd,
					 "<!ENTITY xxe_injected \"" + secret + "\">\n"));

	std::vector<std::pair<std::string, std::string>> files;

	/* hostile fixtures: one per importer family named in SEC07 */
	const std::string abw = std::string(dir) + "/xxe.abw";
	TFPASS(xxe_write(abw, xxe_abw(canary_uri, true)));
	files.push_back({abw, secret});

	const std::string abwn_pe = std::string(dir) + "/xxe-pe.abw";
	TFPASS(xxe_write(abwn_pe, xxe_abw_pe(canary_dtd_uri)));
	files.push_back({abwn_pe, secret});

	const std::string xhtml = std::string(dir) + "/xxe.xhtml";
	TFPASS(xxe_write(xhtml, xxe_xhtml(canary_uri, true)));
	files.push_back({xhtml, secret});

	const std::string odt = std::string(dir) + "/xxe.odt";
	TFPASS(xxe_make_zip(odt, {
		{"mimetype", "application/vnd.oasis.opendocument.text"},
		{"content.xml", xxe_odt_content(canary_uri, true)},
		{"META-INF/manifest.xml", xxe_odt_manifest},
	}, true));
	files.push_back({odt, secret});

	const std::string docx = std::string(dir) + "/xxe.docx";
	TFPASS(xxe_make_zip(docx, {
		{"[Content_Types].xml", xxe_docx_types},
		{"_rels/.rels", xxe_docx_rels},
		{"word/document.xml", xxe_docx_document(canary_uri, true)},
	}, false));
	files.push_back({docx, secret});

	for (const auto & f : files)
		TFPASS(xxe_import_check(f.first, f.second.c_str(), nullptr));

	/* benign controls: same containers with literal text must import
	 * and carry the marker through export */
	const std::string ctl_abw = std::string(dir) + "/ctl.abw";
	TFPASS(xxe_write(ctl_abw, xxe_abw(canary_uri, false)));
	TFPASS(xxe_import_check(ctl_abw, xxe_secret, xxe_marker));

	const std::string ctl_xhtml = std::string(dir) + "/ctl.xhtml";
	TFPASS(xxe_write(ctl_xhtml, xxe_xhtml(canary_uri, false)));
	TFPASS(xxe_import_check(ctl_xhtml, xxe_secret, xxe_marker));

	const std::string ctl_odt = std::string(dir) + "/ctl.odt";
	TFPASS(xxe_make_zip(ctl_odt, {
		{"mimetype", "application/vnd.oasis.opendocument.text"},
		{"content.xml", xxe_odt_content(canary_uri, false)},
		{"META-INF/manifest.xml", xxe_odt_manifest},
	}, true));
	TFPASS(xxe_import_check(ctl_odt, xxe_secret, xxe_marker));

	const std::string ctl_docx = std::string(dir) + "/ctl.docx";
	TFPASS(xxe_make_zip(ctl_docx, {
		{"[Content_Types].xml", xxe_docx_types},
		{"_rels/.rels", xxe_docx_rels},
		{"word/document.xml", xxe_docx_document(canary_uri, false)},
	}, false));
	TFPASS(xxe_import_check(ctl_docx, xxe_secret, xxe_marker));

	for (const auto & f : files)
		g_remove(f.first.c_str());
	g_remove((std::string(dir) + "/ctl.abw").c_str());
	g_remove((std::string(dir) + "/ctl.xhtml").c_str());
	g_remove((std::string(dir) + "/ctl.odt").c_str());
	g_remove((std::string(dir) + "/ctl.docx").c_str());
	g_remove(canary.c_str());
	g_remove(canary_dtd.c_str());
	g_rmdir(dir);
	g_free(dir);
}
