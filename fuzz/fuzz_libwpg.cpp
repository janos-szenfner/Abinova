/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
 * Copyright (C) 2026 Abinova contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * libFuzzer target for the VENDORED libwpg-0.3.4 parser itself —
 * WPGraphics::isSupported + WPGraphics::parse over an in-memory
 * RVNGStringStream with a dummy drawing sink (mirrors upstream's
 * wpgfuzzer.cpp). Covers the WPG1/WPG2 record parsers that run when
 * a .wpg arrives embedded in a WordPerfect graphic or standalone.
 *
 * Build with tools/build-fuzz.sh (clang -fsanitize=fuzzer,address
 * against an instrumented in-tree copy under fuzz-build/).
 */

#include <cstdint>
#include <cstddef>

#include <librevenge-stream/librevenge-stream.h>
#include <librevenge-generators/RVNGDummyDrawingGenerator.h>

#include <libwpg/libwpg.h>

#define FUZZ_MAX_INPUT (16u << 20)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (!data || !size || size > FUZZ_MAX_INPUT)
		return 0;

	librevenge::RVNGStringStream input(data, static_cast<unsigned>(size));
	librevenge::RVNGDummyDrawingGenerator generator;

	(void)libwpg::WPGraphics::isSupported(&input);
	input.seek(0, librevenge::RVNG_SEEK_SET);
	(void)libwpg::WPGraphics::parse(&input, &generator);
	return 0;
}
