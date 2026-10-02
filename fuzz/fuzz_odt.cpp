/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
 * Copyright (C) 2026 Abinova contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * libFuzzer target for the ODF text importer
 * (ie_imp_OpenDocument.cpp — zip container + content.xml/
 * styles.xml/meta.xml/manifest.rdf streams; also the flat-XML
 * .fodt form, same importer).
 *
 * Feeds the input buffer through the IE_Imp sniff + parse entry —
 * IE_Imp::fileTypeForContents() on the raw bytes, then a full
 * PD_Document::readFromFile() pinned to the OpenDocument importer
 * (gsf_infile_zip_new runs directly on the memory input).
 *
 * Build with tools/build-fuzz.sh (clang -fsanitize=fuzzer,address
 * against an instrumented in-tree copy under fuzz-build/).
 */

#include "fuzz_common.h"

static IEFileType s_odtType = IEFT_Unknown;

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	(void)argc;
	(void)argv;

	if (fuzz::initApp())
		return 1;

	s_odtType = fuzz::fileType(".odt");
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	return fuzz::importBuffer(data, size, s_odtType);
}
