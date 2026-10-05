/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
 * Copyright (C) 2026 Abinova contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * libFuzzer target for the VENDORED libwpd-0.10.3 parser itself —
 * WPDocument::isFileFormatSupported + WPDocument::parse over an
 * in-memory RVNGStringStream with a dummy text sink, so hostile
 * bytes reach the library's internal parsers directly rather than
 * through the ie_imp_WordPerfect glue (that path is fuzz_wpd.cpp).
 * Mirrors upstream's own src/fuzz/wpdfuzzer.cpp.
 *
 * Build with tools/build-fuzz.sh (clang -fsanitize=fuzzer,address
 * against an instrumented in-tree copy under fuzz-build/).
 */

#include <cstdint>
#include <cstddef>

#include <librevenge-stream/librevenge-stream.h>
#include <librevenge-generators/RVNGDummyTextGenerator.h>

#include <libwpd/libwpd.h>

/* same bound as fuzz_common.h — keep iterations cheap under the
 * loop RSS watchdog */
#define FUZZ_MAX_INPUT (16u << 20)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (!data || !size || size > FUZZ_MAX_INPUT)
		return 0;

	librevenge::RVNGStringStream input(data, static_cast<unsigned>(size));
	librevenge::RVNGDummyTextGenerator generator;

	(void)libwpd::WPDocument::isFileFormatSupported(&input);
	input.seek(0, librevenge::RVNG_SEEK_SET);
	(void)libwpd::WPDocument::parse(&input, &generator, "");
	return 0;
}
