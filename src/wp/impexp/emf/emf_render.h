/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* Abinova
 * Copyright (C) 2025-2026 Abinova contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef UT_EMF_RENDER_H
#define UT_EMF_RENDER_H

#include "ut_bytebuf.h"

/* Replay an EMF (Enhanced Metafile, [MS-EMF]) byte stream onto a
 * cairo surface and return the rasterized picture as a PNG buffer.
 * Returns false on malformed input or an empty picture. */
bool UT_renderEMF(const UT_ConstByteBufPtr & pEmf, UT_ByteBufPtr & pPng);

#endif /* UT_EMF_RENDER_H */
