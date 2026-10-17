/* -*- mode: C++; tab-width: 2; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
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

/* unit tests for the autosave-recovery scan guards in AP_App:
 * recoverAutosavedDocs() auto-imports files found in the autosave
 * directory without any user gesture, so the gate deciding which
 * names are candidates and which sidecar URIs may be adopted has to
 * be conservative. No document or GUI needed. */

#include "tf_test.h"

#include "ap_App.h"
#include "xap_Frame.h"
#include "ut_go_file.h"
#include "ut_path.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <utime.h>
#include <stdio.h>
#include <time.h>
#include <string>

#define TFSUITE "core.wp.ap.autosave"

TFTEST_MAIN("ap_Autosave candidate names")
{
	/* crash-recovery copies written by saveRecoveryFiles() */
	TFPASS(AP_App::isAutosaveCandidateName("doc-a1b2c3d4.saved", ".bak~"));
	TFPASS(AP_App::isAutosaveCandidateName("Untitled-1.abw.saved", ".bak~"));

	/* periodic-autosave files carry the configured extension */
	TFPASS(AP_App::isAutosaveCandidateName("doc-a1b2c3d4.bak~", ".bak~"));
	TFPASS(AP_App::isAutosaveCandidateName("doc.auto", ".auto"));

	/* sidecars and other droppings are never candidates */
	TFPASS(!AP_App::isAutosaveCandidateName("doc-a1b2c3d4.saved.info", ".bak~"));
	TFPASS(!AP_App::isAutosaveCandidateName("doc-a1b2c3d4.saved.part", ".bak~"));
	TFPASS(!AP_App::isAutosaveCandidateName("doc-a1b2c3d4.bak~.info", ".bak~"));
	TFPASS(!AP_App::isAutosaveCandidateName("readme.txt", ".bak~"));
	TFPASS(!AP_App::isAutosaveCandidateName("planted.docx", ".bak~"));
	TFPASS(!AP_App::isAutosaveCandidateName("planted.html", ".bak~"));

	/* degenerate inputs */
	TFPASS(!AP_App::isAutosaveCandidateName(nullptr, ".bak~"));
	TFPASS(!AP_App::isAutosaveCandidateName("", ".bak~"));
	TFPASS(!AP_App::isAutosaveCandidateName("doc.txt", ""));
	TFPASS(!AP_App::isAutosaveCandidateName("doc.txt", nullptr));
	/* an empty configured ext still lets .saved through */
	TFPASS(AP_App::isAutosaveCandidateName("doc.saved", ""));
	/* inter-instance lock sidecars are never candidates either */
	TFPASS(!AP_App::isAutosaveCandidateName("doc.saved.lock", ".bak~"));
}

static std::string s_tmpdir()
{
	std::string tmpl = "/tmp/abinova-autosave-test-XXXXXX";
	gchar * dir = g_mkdtemp(&tmpl[0]);
	return dir ? (std::string(dir) + "/") : std::string();
}

static void s_put(const std::string & path, const char * data,
				  size_t len, time_t mtime)
{
	FILE * f = g_fopen(path.c_str(), "wb");
	if (f)
	{
		if (len)
			fwrite(data, 1, len, f);
		fclose(f);
	}
	struct utimbuf tb;
	tb.actime = tb.modtime = mtime;
	g_utime(path.c_str(), &tb);
}

TFTEST_MAIN("ap_Autosave temp cleanup")
{
	std::string d = s_tmpdir();
	TFPASS(!d.empty());
	const time_t now = time(nullptr);

	s_put(d + "doc.saved.part", "x", 1, now - 3600);	/* abandoned temp */
	s_put(d + "fresh.saved.part", "x", 1, now);		/* still in-flight */
	s_put(d + "orphan.saved.info", "x", 1, now - 3600); /* no payload */
	s_put(d + "kept.saved", "x", 1, now - 3600);		/* real payload */
	s_put(d + "kept.saved.info", "x", 1, now - 3600);	/* its sidecar */
	s_put(d + "doc.saved.lock", "x", 1, now - 3600);	/* never unlinked */
	s_put(d + "bad.saved.crashed", "x", 1, now - 3600); /* quarantine */
	s_put(d + ".recovering", "x", 1, now - 3600);		/* sentinel */
	s_put(d + "new.orphan.saved.info", "x", 1, now);	/* fresh orphan */

	AP_App::cleanupAutosaveTemps(d, 600, now);

	TFPASS(!UT_isRegularFile((d + "doc.saved.part").c_str()));
	TFPASS(UT_isRegularFile((d + "fresh.saved.part").c_str()));
	TFPASS(!UT_isRegularFile((d + "orphan.saved.info").c_str()));
	TFPASS(UT_isRegularFile((d + "kept.saved").c_str()));
	TFPASS(UT_isRegularFile((d + "kept.saved.info").c_str()));
	TFPASS(UT_isRegularFile((d + "doc.saved.lock").c_str()));
	TFPASS(UT_isRegularFile((d + "bad.saved.crashed").c_str()));
	TFPASS(UT_isRegularFile((d + ".recovering").c_str()));
	TFPASS(UT_isRegularFile((d + "new.orphan.saved.info").c_str()));
}

TFTEST_MAIN("ap_Autosave recovery decode validation")
{
	std::string d = s_tmpdir();
	TFPASS(!d.empty());
	const time_t now = time(nullptr);

	static const char xml[] =
		"<?xml version=\"1.0\"?>\n"
		"<abiword><section><p>hello</p></section></abiword>\n";

	std::string good = d + "good.abwn";
	s_put(good, xml, sizeof(xml) - 1, now);
	TFPASS(XAP_Frame::isValidRecoveryCopy(good.c_str()));

	static const char trunc[] =
		"<?xml version=\"1.0\"?>\n<abiword><section><p>hello";
	std::string bad = d + "trunc.abwn";
	s_put(bad, trunc, sizeof(trunc) - 1, now);
	TFPASS(!XAP_Frame::isValidRecoveryCopy(bad.c_str()));

	std::string wrong = d + "wrong.abwn";
	s_put(wrong, "<html><body/></html>", 19, now);
	TFPASS(!XAP_Frame::isValidRecoveryCopy(wrong.c_str()));

	std::string junk = d + "junk.abwn";
	s_put(junk, "\x01\x02\x03not xml at all", 16, now);
	TFPASS(!XAP_Frame::isValidRecoveryCopy(junk.c_str()));

	std::string enc = d + "enc.abwn";
	s_put(enc, "ABWNCRP\x01\x02\x03\x04", 11, now);
	TFPASS(!XAP_Frame::isValidRecoveryCopy(enc.c_str()));

	std::string empty = d + "empty.abwn";
	s_put(empty, "", 0, now);
	TFPASS(!XAP_Frame::isValidRecoveryCopy(empty.c_str()));

	TFPASS(!XAP_Frame::isValidRecoveryCopy(
			   (d + "nonexistent.abwn").c_str()));

	/* gzip-compressed copies must decode transparently */
	GsfOutput * mem = gsf_output_memory_new();
	GsfOutput * gz = gsf_output_gzip_new(mem, nullptr);
	bool gzok = gz && gsf_output_write(
		gz, sizeof(xml) - 1,
		reinterpret_cast<const guint8 *>(xml)) && gsf_output_close(gz);
	TFPASS(gzok);
	if (gzok)
	{
		const guint8 * bytes =
			gsf_output_memory_get_bytes(GSF_OUTPUT_MEMORY(mem));
		std::string z = d + "good.gz.abwn";
		s_put(z, reinterpret_cast<const char *>(bytes),
			  static_cast<size_t>(gsf_output_size(mem)), now);
		TFPASS(XAP_Frame::isValidRecoveryCopy(z.c_str()));
	}
	g_object_unref(gz);
	g_object_unref(mem);
}

TFTEST_MAIN("ap_Autosave sidecar uri")
{
	/* legitimate originals written by _writeBackupInfo() */
	TFPASS(AP_App::isAutosaveSidecarUriSafe("file:///home/u/doc.abw"));
	TFPASS(AP_App::isAutosaveSidecarUriSafe("file:///tmp/a%20b.abwn"));
	TFPASS(AP_App::isAutosaveSidecarUriSafe("/home/u/doc.abw"));

	/* anything else could redirect the next Save of the recovered
	 * document — remote schemes especially must not be adopted */
	TFPASS(!AP_App::isAutosaveSidecarUriSafe("davs://evil.example/x"));
	TFPASS(!AP_App::isAutosaveSidecarUriSafe("sftp://evil.example/x"));
	TFPASS(!AP_App::isAutosaveSidecarUriSafe("smb://evil.example/x"));
	TFPASS(!AP_App::isAutosaveSidecarUriSafe("http://evil.example/x"));
	TFPASS(!AP_App::isAutosaveSidecarUriSafe("https://evil.example/x"));
	TFPASS(!AP_App::isAutosaveSidecarUriSafe("ftp://evil.example/x"));

	/* non-URI junk and empty values */
	TFPASS(!AP_App::isAutosaveSidecarUriSafe("relative/name.abw"));
	TFPASS(!AP_App::isAutosaveSidecarUriSafe("just-a-name"));
	TFPASS(!AP_App::isAutosaveSidecarUriSafe(""));
	TFPASS(!AP_App::isAutosaveSidecarUriSafe(nullptr));
}
