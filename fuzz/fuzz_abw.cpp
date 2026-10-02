/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
 * Copyright (C) 2026 Abinova contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * libFuzzer target for the native .abw/.abwn importer.
 *
 * Feeds the input buffer through the IE_Imp sniff + parse entry —
 * IE_Imp::fileTypeForContents() on the raw bytes, then a full
 * PD_Document::readFromFile() pinned to the Abinova document
 * importer (the file type ".abw"/".abwn"/".zabwn" all map to).
 *
 * Template: thirdparty/libwps-0.4.14/src/fuzz/docfuzzer.cpp
 *
 * Build with tools/build-fuzz.sh (clang -fsanitize=fuzzer,address
 * against an instrumented in-tree copy under fuzz-build/).
 */

#include <cstdint>
#include <cstddef>

#include <gsf/gsf-input-memory.h>

#include "xap_App.h"
#include "ap_UnixApp.h"
#include "pd_Document.h"
#include "ie_imp.h"
#include "ie_types.h"

/* an .abwn larger than this is not a document, it is a zip bomb in
 * waiting — keep iterations cheap and under the loop RSS watchdog */
#define FUZZ_ABW_MAX_INPUT (16u << 20)

static AP_UnixApp *s_app = nullptr;
static IEFileType s_abwType = IEFT_Unknown;

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	(void)argc;
	(void)argv;

	/* mirror src/wp/test/xp/main.cpp — the piece table and importer
	 * paths reach the app singleton (prefs, string set, data dirs) */
	XAP_App::s_szBuild_ID = "FUZZ";
	XAP_App::s_szAbiSuite_Home = "/tmp";
	XAP_App::s_szBuild_Version = "FUZZ";
	XAP_App::s_szBuild_Options = "FUZZ";
	XAP_App::s_szBuild_Target = "FUZZ";

	s_app = new AP_UnixApp(PACKAGE);
	if (!s_app->initialize(FALSE)) /* has_display=false — headless */
		return 1;

	/* IE_ImpExp_RegisterXP() ran inside initialize(); all XP
	 * sniffers are live, so the sniff stage below is the real one */
	s_abwType = IE_Imp::fileTypeForSuffix(".abw");
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (!data || !size || size > FUZZ_ABW_MAX_INPUT)
		return 0;

	/* sniff the buffer — exercises every registered
	 * recognizeContents() on attacker bytes */
	(void)IE_Imp::fileTypeForContents(
		reinterpret_cast<const char *>(data),
		static_cast<UT_uint32>(size));

	GsfInput *input = gsf_input_memory_new(
		const_cast<guint8 *>(reinterpret_cast<const guint8 *>(data)),
		static_cast<gsf_off_t>(size), FALSE);
	if (!input)
		return 0;

	PD_Document *doc = new PD_Document;
	if (doc)
	{
		doc->readFromFile(input, s_abwType, nullptr);
		doc->unref();
	}

	g_object_unref(input);
	return 0;
}
