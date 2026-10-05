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

#include <glib.h>

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
