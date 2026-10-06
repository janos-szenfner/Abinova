/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
 * Copyright (C) 2026 Abinova contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * libFuzzer target for the WordPerfect Graphics image importer
 * (ie_impGraphic_WPG.cpp — the AbiWordPerfectGraphicsInputStream
 * OLE/gsf adapter, libwpg WPGraphics::parse into the SVG drawing
 * generator, then the generated-SVG re-import through
 * IE_ImpGraphic::loadGraphic).  .wpg is an image format, so this
 * drives the IE_ImpGraphic sniffer/importer entry rather than
 * PD_Document::readFromFile.
 *
 * The vendored-parser-level coverage lives in fuzz_libwpg.cpp; this
 * target exercises OUR entry glue around it.
 *
 * Build with tools/build-fuzz.sh (clang -fsanitize=fuzzer,address
 * against an instrumented in-tree copy under fuzz-build/).
 */

#include "fuzz_common.h"

#include "ie_impGraphic.h"

static IEGraphicFileType s_wpgType = IEGFT_Unknown;

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	(void)argc;
	(void)argv;

	if (fuzz::initApp())
		return 1;

	s_wpgType = IE_ImpGraphic::fileTypeForSuffix(".wpg");
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (!data || !size || size > FUZZ_MAX_INPUT)
		return 0;

	/* sniff the buffer — exercises every registered graphic
	 * recognizeContents() on attacker bytes */
	(void)IE_ImpGraphic::fileTypeForContents(
		reinterpret_cast<const char *>(data),
		static_cast<UT_uint32>(size));

	GsfInput *input = gsf_input_memory_new(
		const_cast<guint8 *>(reinterpret_cast<const guint8 *>(data)),
		static_cast<gsf_off_t>(size), FALSE);
	if (!input)
		return 0;

	FG_ConstGraphicPtr graphic;
	(void)IE_ImpGraphic::loadGraphic(input, s_wpgType, graphic);

	g_object_unref(input);
	return 0;
}
