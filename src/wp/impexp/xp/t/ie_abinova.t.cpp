/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
 * Copyright (C) 2025-2026 Abinova contributors
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

#include "tf_test.h"
#include "pd_Document.h"
#include "ie_exp.h"
#include "ie_imp.h"
#include "ie_types.h"
#include "ut_abwncrypt.h"

#include <gsf/gsf-output-stdio.h>

#include <cstring>
#include <string>
#include <vector>

#define TFSUITE "core.wp.impexp.abinova"

static const char *ABW_FILE = "/test/wp/Gettysburg.abw";

// Reads test data via ABI_TEST_SRC_DIR like fl_AutoNum.t.cpp does.
static PD_Document *import_abw()
{
	std::string data_file;
	if (!TF_Test::ensure_test_data(ABW_FILE, data_file))
		return nullptr;

	PD_Document *doc = new PD_Document;
	if (doc->readFromFile(data_file.c_str(), IEFT_Unknown, nullptr) != UT_OK) {
		doc->unref();
		return nullptr;
	}
	return doc;
}

static bool export_mem(PD_Document * doc, std::vector<unsigned char> & out)
{
	out.clear();
	std::string tmp = std::string("/tmp/abn_exp_") +
		std::to_string(::getpid()) + ".abwn";
	GError * err = nullptr;
	GsfOutput * file = gsf_output_stdio_new(tmp.c_str(), &err);
	bool ok = file &&
		doc->saveAs(file, static_cast<int>(IE_Exp::fileTypeForSuffix(".abwn")),
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
			fread(out.data(), 1, sz, fp);
			fclose(fp);
		} else {
			ok = false;
		}
	}
	unlink(tmp.c_str());
	return ok;
}

TFTEST_MAIN("sniffer maps native suffixes")
{
	IEFileType ft = IE_Imp::fileTypeForSuffix(".abwn");
	TFPASS(ft != IEFT_Unknown && ft != IEFT_Bogus);
	TFPASSEQ(IE_Imp::fileTypeForSuffix(".abwn"),
			 IE_Imp::fileTypeForSuffix(".abw"));
	TFPASSEQ(IE_Imp::fileTypeForSuffix(".zabwn"),
			 IE_Imp::fileTypeForSuffix(".abw.gz"));

	IEFileType ex = IE_Exp::fileTypeForSuffix(".abwn");
	TFPASS(ex != IEFT_Unknown && ex != IEFT_Bogus);
	TFPASSEQ(ex, IE_Exp::fileTypeForSuffix(".zabwn"));

	// preferred save suffix is .abwn
	TFPASS(strstr(IE_Exp::preferredSuffixForFileType(ex).utf8_str(), "abwn") != nullptr);
}

TFTEST_MAIN("content sniffer recognizes abinova/abiword XML and envelope")
{
	IEFileType ft = IE_Imp::fileTypeForContents("<abinova version=\"1\"/>", 30);
	TFPASS(ft == IE_Imp::fileTypeForSuffix(".abwn"));

	ft = IE_Imp::fileTypeForContents("<abiword version=\"1\"/>", 30);
	TFPASS(ft == IE_Imp::fileTypeForSuffix(".abw"));

	std::vector<unsigned char> blob;
	TFPASSEQ(static_cast<int>(UT_abwn_encrypt("<x/>", 4, "pw", blob)),
			 static_cast<int>(UT_AbwnCrypt::Ok));
	ft = IE_Imp::fileTypeForContents(
		reinterpret_cast<const char *>(blob.data()), blob.size());
	TFPASS(ft == IE_Imp::fileTypeForSuffix(".abwn"));

	ft = IE_Imp::fileTypeForContents("not a document at all", 21);
	TFPASS(ft == IEFT_Unknown || ft == IEFT_Bogus ||
		   ft != IE_Imp::fileTypeForSuffix(".abwn"));
}

TFTEST_MAIN("legacy .abw imports and exports well-formed .abwn")
{
	PD_Document *doc = import_abw();
	TFPASS(doc);
	if (!doc)
		return;

	std::vector<unsigned char> xml;
	TFPASS(export_mem(doc, xml));
	TFPASS(xml.size() > 256);

	std::string text(reinterpret_cast<const char *>(xml.data()), xml.size());
	TFPASS(text.find("<abinova") != std::string::npos);
	TFPASS(text.find("<section") != std::string::npos);
	TFPASS(text.find("</abinova>") != std::string::npos);
	// document text survives
	TFPASS(text.find("score") != std::string::npos);

	// round-trip: the exported file re-imports
	IEFileType ft = IE_Imp::fileTypeForContents(
		reinterpret_cast<const char *>(xml.data()), xml.size());
	TFPASS(ft == IE_Imp::fileTypeForSuffix(".abwn"));

	doc->unref();
}

TFTEST_MAIN("password-protected export emits ABWNCRP1 envelope")
{
	PD_Document *doc = import_abw();
	TFPASS(doc);
	if (!doc)
		return;

	doc->setSavePassword("testsuite-pw");
	std::vector<unsigned char> blob;
	TFPASS(export_mem(doc, blob));
	TFPASS(UT_abwn_isEncrypted(blob.data(), blob.size()));

	// and it decrypts back to the same XML
	std::vector<unsigned char> plain;
	TFPASSEQ(static_cast<int>(UT_abwn_decrypt(blob.data(), blob.size(),
											"testsuite-pw", plain)),
			 static_cast<int>(UT_AbwnCrypt::Ok));
	std::string text(reinterpret_cast<const char *>(plain.data()), plain.size());
	TFPASS(text.find("<abinova") != std::string::npos);
	TFPASS(text.find("</abinova>") != std::string::npos);

	// wrong password on the real exported blob
	plain.clear();
	TFPASSEQ(static_cast<int>(UT_abwn_decrypt(blob.data(), blob.size(), "nope", plain)),
			 static_cast<int>(UT_AbwnCrypt::WrongPassword));

	doc->unref();
}

TFTEST_MAIN("exported abwn re-imports through readFromFile")
{
	PD_Document *doc = import_abw();
	TFPASS(doc);
	if (!doc)
		return;

	std::vector<unsigned char> xml;
	TFPASS(export_mem(doc, xml));
	doc->unref();

	// write to a temp file and re-import through the normal path
	std::string tmp = std::string("/tmp/abn_testsuite_") +
		std::to_string(::getpid()) + ".abwn";
	FILE * fp = fopen(tmp.c_str(), "wb");
	TFPASS(fp != nullptr);
	if (!fp)
		return;
	TFPASSEQ(fwrite(xml.data(), 1, xml.size(), fp), xml.size());
	fclose(fp);

	PD_Document *doc2 = new PD_Document;
	TFPASSEQ(doc2->readFromFile(tmp.c_str(), IEFT_Unknown, nullptr), UT_OK);
	doc2->unref();
	unlink(tmp.c_str());
}
