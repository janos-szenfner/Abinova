/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
 * Copyright (C) 2026 Abinova contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Shared plumbing for the libFuzzer targets under fuzz/.
 *
 * Each fuzz_<fmt>.cpp defines its own LLVMFuzzerInitialize /
 * LLVMFuzzerTestOneInput and calls:
 *
 *   fuzz::initApp()           — one-time headless AP_UnixApp bootstrap
 *                               (registers every XP sniffer/importer)
 *   fuzz::fileType(".ext")    — resolve the IEFileType the harness pins
 *   fuzz::importBuffer(...)   — sniff the bytes, then run a full
 *                               PD_Document::readFromFile() pinned to
 *                               the target importer through a memory
 *                               GsfInput
 *
 * Template: thirdparty/libwps-0.4.14/src/fuzz/docfuzzer.cpp
 */

#ifndef FUZZ_COMMON_H
#define FUZZ_COMMON_H

#include <cstdint>
#include <cstddef>

#include <gsf/gsf-input-memory.h>

#include "xap_App.h"
#include "ap_UnixApp.h"
#include "pd_Document.h"
#include "ie_imp.h"
#include "ie_types.h"

/* anything larger than this is not a document, it is a zip bomb in
 * waiting — keep iterations cheap and under the loop RSS watchdog */
#define FUZZ_MAX_INPUT (16u << 20)

namespace fuzz {

inline AP_UnixApp *&app()
{
	static AP_UnixApp *s_app = nullptr;
	return s_app;
}

/* mirror src/wp/test/xp/main.cpp — the piece table and importer
 * paths reach the app singleton (prefs, string set, data dirs) */
inline int initApp()
{
	XAP_App::s_szBuild_ID = "FUZZ";
	XAP_App::s_szAbiSuite_Home = "/tmp";
	XAP_App::s_szBuild_Version = "FUZZ";
	XAP_App::s_szBuild_Options = "FUZZ";
	XAP_App::s_szBuild_Target = "FUZZ";

	app() = new AP_UnixApp(PACKAGE);
	if (!app()->initialize(FALSE)) /* has_display=false — headless */
		return 1;
	return 0;
}

/* IE_ImpExp_RegisterXP() ran inside initialize(); all XP sniffers are
 * live, so suffix lookup returns the real importer type */
inline IEFileType fileType(const char *szSuffix)
{
	return IE_Imp::fileTypeForSuffix(szSuffix);
}

inline int importBuffer(const uint8_t *data, size_t size, IEFileType type)
{
	if (!data || !size || size > FUZZ_MAX_INPUT)
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
		doc->readFromFile(input, type, nullptr);
		doc->unref();
	}

	g_object_unref(input);
	return 0;
}

} /* namespace fuzz */

#endif /* FUZZ_COMMON_H */
