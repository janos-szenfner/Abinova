/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
 * Copyright (C) 2026 Abinova contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * libFuzzer target for the VENDORED libwps-0.4.14 parser itself —
 * WPSDocument::isFileFormatSupported + WPSDocument::parse over an
 * in-memory RVNGStringStream, dispatching to a text or spreadsheet
 * dummy sink on the detected document kind (mirrors upstream's
 * wpsfuzzer.cpp + 123fuzzer.cpp). Covers the MS Works / Lotus /
 * Quattro / MS Write / XYWrite / PocketWord raw-format parsers and
 * the OLE-container path via RVNGStringStream.
 *
 * Build with tools/build-fuzz.sh (clang -fsanitize=fuzzer,address
 * against an instrumented in-tree copy under fuzz-build/).
 */

#include <cstdint>
#include <cstddef>

#include <librevenge-stream/librevenge-stream.h>
#include <librevenge-generators/RVNGDummyTextGenerator.h>
#include <librevenge-generators/RVNGDummySpreadsheetGenerator.h>

#include <libwps/libwps.h>

#define FUZZ_MAX_INPUT (16u << 20)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (!data || !size || size > FUZZ_MAX_INPUT)
		return 0;

	librevenge::RVNGStringStream input(data, static_cast<unsigned>(size));

	libwps::WPSKind kind = libwps::WPS_TEXT;
	libwps::WPSCreator creator = libwps::WPS_MSWORKS;
	bool needEncoding = false;
	(void)libwps::WPSDocument::isFileFormatSupported(&input, kind, creator, needEncoding);

	input.seek(0, librevenge::RVNG_SEEK_SET);
	if (kind == libwps::WPS_SPREADSHEET)
	{
		librevenge::RVNGDummySpreadsheetGenerator generator;
		(void)libwps::WPSDocument::parse(&input, &generator);
	}
	else
	{
		librevenge::RVNGDummyTextGenerator generator;
		(void)libwps::WPSDocument::parse(&input, &generator);
	}
	return 0;
}
