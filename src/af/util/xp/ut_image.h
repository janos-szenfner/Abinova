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

#pragma once

#include <math.h>

#include "ut_types.h"

/* Sane bounds on decoded raster images.  A hostile or corrupt file
 * can declare absurd dimensions (20000x20000 decodes to ~1.6 GB of
 * pixels) — import and render paths check these before allocating.
 * 64 Mi pixels is ~256 MiB of RGBA, generous for document images;
 * 16384 stays under cairo's 32767 surface limit. */
inline constexpr UT_sint32 UT_IMAGE_MAX_DIMENSION = 16384;
inline constexpr UT_sint64 UT_IMAGE_MAX_PIXELS = 67108864;

/// True when (w,h) exceeds the sane decoded-image bounds.
inline bool UT_image_size_exceeds_limits(UT_sint64 w, UT_sint64 h)
{
	return w > UT_IMAGE_MAX_DIMENSION || h > UT_IMAGE_MAX_DIMENSION
		|| w * h > UT_IMAGE_MAX_PIXELS;
}

/// Scale (w,h) down, preserving aspect, until it fits the bounds.
inline void UT_image_size_clamp(UT_sint32 & w, UT_sint32 & h)
{
	double scale = 1.0;
	if (w > UT_IMAGE_MAX_DIMENSION)
		scale = static_cast<double>(UT_IMAGE_MAX_DIMENSION) / w;
	if (h > UT_IMAGE_MAX_DIMENSION)
	{
		double s = static_cast<double>(UT_IMAGE_MAX_DIMENSION) / h;
		if (s < scale) scale = s;
	}
	if (static_cast<UT_sint64>(w) * h > UT_IMAGE_MAX_PIXELS)
	{
		double s = sqrt(static_cast<double>(UT_IMAGE_MAX_PIXELS) /
						(static_cast<double>(w) * h));
		if (s < scale) scale = s;
	}
	if (scale < 1.0)
	{
		w = w * scale > 1.0 ? static_cast<UT_sint32>(w * scale) : 1;
		h = h * scale > 1.0 ? static_cast<UT_sint32>(h * scale) : 1;
	}
}
