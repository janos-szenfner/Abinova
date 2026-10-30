/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */

/* Abinova
 * Copyright (C) 2025-2026 Abinova contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

/* EMF ([MS-EMF]) playback onto a cairo image surface, producing a PNG.
 * Covers the common record set: pens/brushes/fonts, lines and shapes,
 * paths, text out, DIB blits, clips and world transforms. Records we
 * do not model (GLS, escapes, comments, palette bookkeeping, color
 * spaces) are skipped harmlessly — the parser is strictly bounded so
 * hostile input cannot overrun.
 */

#include <cairo.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "ut_bytebuf.h"
#include "ut_debugmsg.h"
#include "emf_render.h"

namespace {

/* ---------------------------------------------------------------- */
/* little-endian cursor over the EMF buffer                          */
/* ---------------------------------------------------------------- */

class Buf
{
public:
	const uint8_t * p;
	size_t n;

	bool u8(uint8_t & v, size_t off) const
	{
		if (off + 1 > n) return false;
		v = p[off];
		return true;
	}
	bool u16(uint16_t & v, size_t off) const
	{
		if (off + 2 > n) return false;
		v = static_cast<uint16_t>(p[off] | (p[off + 1] << 8));
		return true;
	}
	bool u32(uint32_t & v, size_t off) const
	{
		if (off + 4 > n) return false;
		v = p[off] | (p[off + 1] << 8) | (p[off + 2] << 16)
			| (static_cast<uint32_t>(p[off + 3]) << 24);
		return true;
	}
	bool i32(int32_t & v, size_t off) const
	{
		uint32_t u = 0;
		if (!u32(u, off)) return false;
		v = static_cast<int32_t>(u);
		return true;
	}
	bool f32(float & v, size_t off) const
	{
		uint32_t u = 0;
		if (!u32(u, off)) return false;
		std::memcpy(&v, &u, 4);
		return true;
	}
	bool rect(double & l, double & t, double & r, double & b,
			  size_t off) const
	{
		int32_t il = 0, it = 0, ir = 0, ib = 0;
		if (!i32(il, off) || !i32(it, off + 4) ||
			!i32(ir, off + 8) || !i32(ib, off + 12))
			return false;
		l = il; t = it; r = ir; b = ib;
		return true;
	}
	bool point(double & x, double & y, size_t off) const
	{
		int32_t ix = 0, iy = 0;
		if (!i32(ix, off) || !i32(iy, off + 4)) return false;
		x = ix; y = iy;
		return true;
	}
	bool point16(double & x, double & y, size_t off) const
	{
		uint16_t ux = 0, uy = 0;
		if (!u16(ux, off) || !u16(uy, off + 2)) return false;
		x = static_cast<int16_t>(ux);
		y = static_cast<int16_t>(uy);
		return true;
	}
	const uint8_t * at(size_t off, size_t len) const
	{
		return (off <= n && len <= n - off) ? p + off : nullptr;
	}
};

/* ---------------------------------------------------------------- */
/* EMF constants ([MS-EMF] 2.x)                                      */
/* ---------------------------------------------------------------- */

enum {
	EMR_HEADER = 1, EMR_POLYBEZIER = 2, EMR_POLYGON = 3,
	EMR_POLYLINE = 4, EMR_POLYBEZIERTO = 5, EMR_POLYLINETO = 6,
	EMR_POLYPOLYLINE = 7, EMR_POLYPOLYGON = 8,
	EMR_SETWINDOWEXTEX = 9, EMR_SETWINDOWORGEX = 10,
	EMR_SETVIEWPORTEXTEX = 11, EMR_SETVIEWPORTORGEX = 12,
	EMR_SETBRUSHORGEX = 13, EMR_EOF = 14, EMR_SETPIXELV = 15,
	EMR_SETMAPPERFLAGS = 16, EMR_SETMAPMODE = 17, EMR_SETBKMODE = 18,
	EMR_SETPOLYFILLMODE = 19, EMR_SETROP2 = 20, EMR_SETSTRETCHBLTMODE = 21,
	EMR_SETTEXTALIGN = 22, EMR_SETCOLORADJUSTMENT = 23,
	EMR_SETTEXTCOLOR = 24, EMR_SETBKCOLOR = 25, EMR_OFFSETCLIPRGN = 26,
	EMR_MOVETOEX = 27, EMR_SETMETARGN = 28, EMR_EXCLUDECLIPRECT = 29,
	EMR_INTERSECTCLIPRECT = 30, EMR_SCALEVIEWPORTEXTEX = 31,
	EMR_SCALEWINDOWEXTEX = 32, EMR_SAVEDC = 33, EMR_RESTOREDC = 34,
	EMR_SETWORLDTRANSFORM = 35, EMR_MODIFYWORLDTRANSFORM = 36,
	EMR_SELECTOBJECT = 37, EMR_CREATEPEN = 38, EMR_CREATEBRUSHINDIRECT = 39,
	EMR_DELETEOBJECT = 40, EMR_ANGLEARC = 41, EMR_ELLIPSE = 42,
	EMR_RECTANGLE = 43, EMR_ROUNDRECT = 44, EMR_ARC = 45, EMR_CHORD = 46,
	EMR_PIE = 47, EMR_SELECTPALETTE = 48, EMR_CREATEPALETTE = 49,
	EMR_SETPALETTEENTRIES = 50, EMR_RESIZEPALETTE = 51,
	EMR_REALIZEPALETTE = 52, EMR_EXTFLOODFILL = 53, EMR_LINETO = 54,
	EMR_ARCTO = 55, EMR_POLYDRAW = 56, EMR_SETARCDIRECTION = 57,
	EMR_SETMITERLIMIT = 58, EMR_BEGINPATH = 59, EMR_ENDPATH = 60,
	EMR_CLOSEFIGURE = 61, EMR_FILLPATH = 62, EMR_STROKEANDFILLPATH = 63,
	EMR_STROKEPATH = 64, EMR_FLATTENPATH = 65, EMR_WIDENPATH = 66,
	EMR_SELECTCLIPPATH = 67, EMR_ABORTPATH = 68, EMR_COMMENT = 70,
	EMR_FILLRGN = 71, EMR_FRAMERGN = 72, EMR_INVERTRGN = 73,
	EMR_PAINTRGN = 74, EMR_EXTSELECTCLIPRGN = 75, EMR_BITBLT = 76,
	EMR_STRETCHBLT = 77, EMR_MASKBLT = 78, EMR_PLGBLT = 79,
	EMR_SETDIBITSTODEVICE = 80, EMR_STRETCHDIBITS = 81,
	EMR_EXTCREATEFONTINDIRECTW = 82, EMR_EXTTEXTOUTA = 83,
	EMR_EXTTEXTOUTW = 84, EMR_POLYBEZIER16 = 85, EMR_POLYGON16 = 86,
	EMR_POLYLINE16 = 87, EMR_POLYBEZIERTO16 = 88, EMR_POLYLINETO16 = 89,
	EMR_POLYPOLYLINE16 = 90, EMR_POLYPOLYGON16 = 91, EMR_POLYDRAW16 = 92,
	EMR_CREATEMONOBRUSH = 93, EMR_CREATEDIBPATTERNBRUSHPT = 94,
	EMR_EXTCREATEPEN = 95, EMR_POLYTEXTOUTA = 96, EMR_POLYTEXTOUTW = 97,
	EMR_SETICMMODE = 98, EMR_ALPHABLEND = 114, EMR_TRANSPARENTBLT = 116,
	EMR_GRADIENTFILL = 118, EMR_SETLAYOUT = 115
};

enum { MM_TEXT = 1, MM_LOMETRIC = 2, MM_HIMETRIC = 3, MM_LOENGLISH = 4,
	   MM_HIENGLISH = 5, MM_TWIPS = 6, MM_ISOTROPIC = 7, MM_ANISOTROPIC = 8 };

enum { PS_SOLID = 0, PS_DASH = 1, PS_DOT = 2, PS_DASHDOT = 3,
	   PS_DASHDOTDOT = 4, PS_NULL = 5, PS_INSIDEFRAME = 6,
	   PS_USERSTYLE = 7, PS_ALTERNATE = 8, PS_STYLE_MASK = 0xF,
	   PS_ENDCAP_MASK = 0xF00, PS_JOIN_MASK = 0xF000,
	   PS_TYPE_MASK = 0xF0000, PS_GEOMETRIC = 0x10000 };

enum { BS_SOLID = 0, BS_NULL = 1, BS_HATCHED = 2, BS_PATTERN = 3,
	   BS_DIBPATTERN = 5, BS_DIBPATTERNPT = 6 };

enum { ETO_OPAQUE = 0x2, ETO_CLIPPED = 0x4, ETO_GLYPH_INDEX = 0x10,
	   ETO_RTLREADING = 0x80, ETO_SMALL_CHARS = 0x100 };

/* object kinds */
enum { K_NONE = 0, K_PEN, K_BRUSH, K_FONT, K_REGION, K_PALETTE };

/* stock object indexes (low bits of a stock handle) */
enum { STOCK_WHITE_BRUSH = 0, STOCK_LTGRAY_BRUSH = 1, STOCK_GRAY_BRUSH = 2,
	   STOCK_DKGRAY_BRUSH = 3, STOCK_BLACK_BRUSH = 4, STOCK_NULL_BRUSH = 5,
	   STOCK_WHITE_PEN = 6, STOCK_BLACK_PEN = 7, STOCK_NULL_PEN = 8,
	   STOCK_OEM_FIXED_FONT = 10, STOCK_ANSI_FIXED_FONT = 11,
	   STOCK_ANSI_VAR_FONT = 12, STOCK_SYSTEM_FONT = 13,
	   STOCK_DEVICE_DEFAULT_FONT = 14, STOCK_DEFAULT_PALETTE = 15,
	   STOCK_SYSTEM_FIXED_FONT = 16, STOCK_DEFAULT_GUI_FONT = 17,
	   STOCK_DC_BRUSH = 18, STOCK_DC_PEN = 19 };

struct Pen
{
	int style = PS_SOLID;
	bool geometric = false;
	double width = 1.0;          /* device px (cosmetic) or logical units */
	uint32_t color = 0;
	std::vector<double> dashes;  /* logical-unit dash pattern */
	cairo_line_cap_t cap = CAIRO_LINE_CAP_ROUND;
	cairo_line_join_t join = CAIRO_LINE_JOIN_ROUND;
};

struct Brush
{
	int style = BS_SOLID;
	uint32_t color = 0xFFFFFF;
	int hatch = 0;
};

struct Font
{
	int height = 12;             /* logical units; negative = char height */
	int width = 0;
	int escapement = 0;          /* tenths of a degree */
	int weight = 400;
	bool italic = false;
	bool underline = false;
	bool strikeout = false;
	bool vertical = false;
	std::string face = "Sans";
};

struct Obj
{
	int kind = K_NONE;
	Pen pen;
	Brush brush;
	Font font;
	std::vector<uint8_t> region; /* raw RGNDATA for region objects */
};

struct DC
{
	/* window -> viewport mapping (logical -> device) */
	int mapMode = MM_TEXT;
	double wnX = 0, wnY = 0, wnCX = 1, wnCY = 1;
	double vpX = 0, vpY = 0, vpCX = 1, vpCY = 1;
	/* world transform eM11 eM12 eM21 eM22 eDx eDy (cairo order) */
	double wt[6] = {1, 0, 0, 1, 0, 0};
	uint32_t pen = 0x80000007u;   /* BLACK_PEN */
	uint32_t brush = 0x80000000u; /* WHITE_BRUSH */
	uint32_t font = 0x8000000Du;  /* SYSTEM_FONT */
	uint32_t dcBrushColor = 0xFFFFFF;
	uint32_t dcPenColor = 0;
	int bkMode = 2;               /* TRANSPARENT=1 OPAQUE=2 */
	uint32_t bkColor = 0xFFFFFF;
	uint32_t textColor = 0;
	uint32_t textAlign = 0;       /* TA_LEFT|TA_TOP */
	int polyFill = 1;             /* ALTERNATE=1 WINDING=2 */
	int arcDir = 1;               /* AD_COUNTERCLOCKWISE=1 CLOCKWISE=2 */
	double miterLimit = 10.0;
	double curX = 0, curY = 0;
};

static inline void crgb(uint32_t rgb, double & r, double & g, double & b)
{
	r = (rgb & 0xFF) / 255.0;
	g = ((rgb >> 8) & 0xFF) / 255.0;
	b = ((rgb >> 16) & 0xFF) / 255.0;
}

/* Decode one TRIVERTEX 16-bit color channel to 0..1 */
static inline double tri16(uint16_t v)
{
	return v / 65535.0;
}

/* ---------------------------------------------------------------- */
/* DIB (BITMAPINFO + bits) -> cairo image surface                    */
/* ---------------------------------------------------------------- */

/* build a cairo ARGB32 (premultiplied) surface from a DIB.
 * Returns nullptr on failure. iUsage: 0 = DIB_RGB_COLORS,
 * 1 = DIB_PAL_COLORS (palette indices — unsupported). */
static cairo_surface_t * dibToSurface(const Buf & rec,
									  size_t offBmi, size_t cbBmi,
									  size_t offBits, size_t cbBits,
									  uint32_t iUsage)
{
	if (!offBmi || cbBmi < 12 || !offBits || !cbBits || iUsage == 1)
		return nullptr;

	uint32_t biSize = 0;
	if (!rec.u32(biSize, offBmi) || biSize < 40)
		return nullptr; /* BITMAPCOREHEADER unsupported */

	int32_t w = 0, h = 0;
	uint16_t planes = 0, bpp = 0;
	uint32_t compression = 0, sizeImage = 0, clrUsed = 0;
	if (!rec.i32(w, offBmi + 4) || !rec.i32(h, offBmi + 8) ||
		!rec.u16(planes, offBmi + 12) || !rec.u16(bpp, offBmi + 14) ||
		!rec.u32(compression, offBmi + 16) ||
		!rec.u32(sizeImage, offBmi + 20) ||
		!rec.u32(clrUsed, offBmi + 32))
		return nullptr;

	if (w <= 0 || h == 0 || w > 16384 || h > 16384 || planes != 1)
		return nullptr;
	const bool topDown = h < 0;
	const int height = h < 0 ? -h : h;
	const int width = w;

	const uint8_t * bits = rec.at(offBits, cbBits);
	if (!bits)
		return nullptr;

	/* PNG/JPEG-compressed DIBs (EMF+ era) — route through gdk-pixbuf */
	if (compression == 4 || compression == 5)
	{
		GInputStream * stream =
			g_memory_input_stream_new_from_data(bits, cbBits, nullptr);
		GdkPixbuf * px = gdk_pixbuf_new_from_stream(stream, nullptr, nullptr);
		g_object_unref(stream);
		if (!px)
			return nullptr;
		const int pw = gdk_pixbuf_get_width(px);
		const int ph = gdk_pixbuf_get_height(px);
		cairo_surface_t * s =
			cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
		uint8_t * dst = cairo_image_surface_get_data(s);
		const int dstride = cairo_image_surface_get_stride(s);
		const guchar * src = gdk_pixbuf_get_pixels(px);
		const int sstride = gdk_pixbuf_get_rowstride(px);
		const int nch = gdk_pixbuf_get_n_channels(px);
		const bool hasAlpha = gdk_pixbuf_get_has_alpha(px) != 0;
		for (int ry = 0; ry < ph; ++ry)
		{
			uint32_t * row =
				reinterpret_cast<uint32_t *>(dst + ry * dstride);
			const guchar * srow = src + ry * sstride;
			for (int rx = 0; rx < pw; ++rx)
			{
				uint32_t r = srow[rx * nch],
					g = srow[rx * nch + 1],
					b = srow[rx * nch + 2];
				uint32_t a = hasAlpha ? srow[rx * nch + 3] : 255;
				r = r * a / 255;
				g = g * a / 255;
				b = b * a / 255;
				row[rx] = (a << 24) | (r << 16) | (g << 8) | b;
			}
		}
		cairo_surface_mark_dirty(s);
		g_object_unref(px);
		return s;
	}

	/* palette */
	uint32_t nPal = clrUsed ? clrUsed : (bpp <= 8 ? (1u << bpp) : 0);
	uint32_t pal[256];
	std::memset(pal, 0, sizeof(pal));
	size_t palOff = offBmi + 14 + biSize;
	uint32_t masks[3] = {0, 0, 0};
	if (compression == 3 && bpp > 8) /* BI_BITFIELDS */
	{
		if (biSize >= 56) /* V4+: masks inside the header */
		{
			rec.u32(masks[0], offBmi + 14 + 40);
			rec.u32(masks[1], offBmi + 14 + 44);
			rec.u32(masks[2], offBmi + 14 + 48);
		}
		else
		{
			rec.u32(masks[0], palOff);
			rec.u32(masks[1], palOff + 4);
			rec.u32(masks[2], palOff + 8);
			palOff += 12;
		}
	}
	if (nPal)
	{
		if (nPal > 256) nPal = 256;
		for (uint32_t i = 0; i < nPal; ++i)
		{
			uint32_t q = 0;
			rec.u32(q, palOff + i * 4);
			pal[i] = q & 0xFFFFFF;
		}
	}

	cairo_surface_t * surf =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS)
	{
		cairo_surface_destroy(surf);
		return nullptr;
	}
	uint8_t * dst = cairo_image_surface_get_data(surf);
	const int stride = cairo_image_surface_get_stride(surf);

	/* RLE8 / RLE4 compressed indices */
	if (compression == 1 || compression == 2)
	{
		uint32_t * idx =
			new(std::nothrow) uint32_t[static_cast<size_t>(width) * height];
		if (!idx)
		{
			cairo_surface_destroy(surf);
			return nullptr;
		}
		std::memset(idx, 0, static_cast<size_t>(width) * height * 4);
		size_t bp = 0;
		int x = 0, y = height - 1; /* RLE output is bottom-up */
		const int rleBpp = compression == 1 ? 8 : 4;
		auto putIdx = [&](int px, int py, uint32_t v) {
			if (px >= 0 && px < width && py >= 0 && py < height)
				idx[static_cast<size_t>(py) * width + px] = v;
		};
		bool eob = false;
		while (bp + 2 <= cbBits && !eob)
		{
			uint8_t cnt = bits[bp], val = bits[bp + 1];
			bp += 2;
			if (cnt)
			{
				for (int i = 0; i < cnt; ++i)
				{
					uint32_t v = rleBpp == 8
						? val : ((i & 1) ? (val & 0xF) : (val >> 4));
					putIdx(x + i, y, v);
				}
				x += cnt;
			}
			else
			{
				switch (val)
				{
				case 0: x = 0; --y; break;         /* EOL */
				case 1: eob = true; break;        /* EOB */
				case 2:                           /* delta */
					if (bp + 2 <= cbBits)
					{
						x += bits[bp];
						y -= bits[bp + 1];
						bp += 2;
					}
					break;
				default:                          /* absolute run */
				{
					int n = val;
					for (int i = 0; i < n && bp < cbBits; ++i)
					{
						uint32_t v;
						if (rleBpp == 8)
						{
							v = bits[bp++];
						}
						else
						{
							uint8_t byte = bits[bp];
							v = (i & 1) ? (byte & 0xF) : (byte >> 4);
							if (i & 1) ++bp;
						}
						putIdx(x + i, y, v);
					}
					if (rleBpp == 4 && (n & 1)) ++bp;
					if (bp & 1) ++bp;             /* word align */
					x += n;
					break;
				}
				}
			}
		}
		for (int ry = 0; ry < height; ++ry)
		{
			uint32_t * row =
				reinterpret_cast<uint32_t *>(dst + ry * stride);
			for (int rx = 0; rx < width; ++rx)
			{
				uint32_t c = pal[idx[static_cast<size_t>(ry) * width + rx] & 0xFF];
				row[rx] = 0xFF000000u | c;
			}
		}
		delete[] idx;
		cairo_surface_mark_dirty(surf);
		return surf;
	}

	if (compression != 0 && compression != 3)
	{
		cairo_surface_destroy(surf);
		return nullptr;
	}

	const size_t rowBytes =
		(static_cast<size_t>(width) * bpp + 31) / 32 * 4;
	bool allAlphaZero = bpp == 32;
	auto px = [&](int ix, int iy, uint32_t & a, uint32_t & rgb) -> bool {
		if (ix < 0 || ix >= width || iy < 0 || iy >= height)
			return false;
		size_t base = static_cast<size_t>(iy) * rowBytes;
		uint32_t v = 0;
		a = 255;
		switch (bpp)
		{
		case 32:
			if (base + 4 > cbBits) return false;
			v = bits[base] | (bits[base + 1] << 8) |
				(bits[base + 2] << 16) |
				(static_cast<uint32_t>(bits[base + 3]) << 24);
			a = (v >> 24) & 0xFF;
			rgb = v & 0xFFFFFF;
			if (a) allAlphaZero = false;
			return true;
		case 24:
			if (base + 3 > cbBits) return false;
			rgb = bits[base] | (bits[base + 1] << 8) |
				(bits[base + 2] << 16);
			return true;
		case 16:
		{
			if (base + 2 > cbBits) return false;
			uint32_t v16 = bits[base] | (bits[base + 1] << 8);
			if (masks[0] | masks[1] | masks[2])
			{
				auto expand = [&](uint32_t m) -> uint32_t {
					if (!m) return 0;
					uint32_t val = v16 & m;
					uint32_t sh = 0;
					while (!(m & 1)) { m >>= 1; ++sh; }
					val >>= sh;
					uint32_t maxv = m;
					return (val * 255 + maxv / 2) / maxv;
				};
				rgb = (expand(masks[0]) & 0xFF) |
					((expand(masks[1]) & 0xFF) << 8) |
					((expand(masks[2]) & 0xFF) << 16);
			}
			else /* 5-5-5 */
			{
				uint32_t r5 = (v16 >> 10) & 0x1F,
					g5 = (v16 >> 5) & 0x1F, b5 = v16 & 0x1F;
				rgb = ((r5 * 255 + 15) / 31) |
					(((g5 * 255 + 15) / 31) << 8) |
					(((b5 * 255 + 15) / 31) << 16);
			}
			return true;
		}
		case 8:
			if (base + 1 > cbBits) return false;
			rgb = pal[bits[base] & 0xFF];
			return true;
		case 4:
			if (base + 1 > cbBits) return false;
			rgb = pal[(bits[base] >> ((~ix & 1) * 4)) & 0xF];
			return true;
		case 1:
			if (base + 1 > cbBits) return false;
			rgb = pal[(bits[base] >> (7 - (ix & 7))) & 1];
			return true;
		default:
			return false;
		}
	};

	for (int ry = 0; ry < height; ++ry)
	{
		int sy = topDown ? ry : height - 1 - ry;
		uint32_t * row =
			reinterpret_cast<uint32_t *>(dst + ry * stride);
		for (int rx = 0; rx < width; ++rx)
		{
			uint32_t a = 255, rgb = 0;
			if (!px(rx, sy, a, rgb))
			{
				row[rx] = 0;
				continue;
			}
			uint32_t r = rgb & 0xFF, g = (rgb >> 8) & 0xFF,
				b = (rgb >> 16) & 0xFF;
			/* premultiply */
			r = r * a / 255; g = g * a / 255; b = b * a / 255;
			row[rx] = (a << 24) | (r << 16) | (g << 8) | b;
		}
	}
	if (bpp == 32 && allAlphaZero)
	{
		/* a zero alpha channel is ambiguous in BI_RGB 32bpp — treat
		 * it as opaque, matching GDI */
		for (int ry = 0; ry < height; ++ry)
		{
			uint32_t * row =
				reinterpret_cast<uint32_t *>(dst + ry * stride);
			for (int rx = 0; rx < width; ++rx)
				row[rx] |= 0xFF000000u;
		}
	}
	cairo_surface_mark_dirty(surf);
	return surf;
}

/* ---------------------------------------------------------------- */
/* the player                                                        */
/* ---------------------------------------------------------------- */

class Player
{
public:
	Player(cairo_t * cr, const Buf & file)
		: m_cr(cr), m_file(file)
	{
	}

	/* frame from the header — drives the logical->device metric
	 * modes and the output scale */
	double m_devDpiX = 96.0, m_devDpiY = 96.0;
	double m_playScaleX = 1.0, m_playScaleY = 1.0; /* device->output */
	double m_boundsL = 0, m_boundsT = 0;

	void applyTransform();
	bool run(size_t firstRecOff, size_t fileLen);

private:
	cairo_t * m_cr;
	Buf m_file;
	std::map<uint32_t, Obj> m_objects;
	std::vector<DC> m_stack;
	DC m_dc;
	bool m_inPath = false;

	const Obj * obj(uint32_t h) const;
	Pen resolvePen(const DC & dc) const;
	Brush resolveBrush(const DC & dc) const;
	Font resolveFont(const DC & dc) const;

	/* transform handling */
	void logicalMatrix(cairo_matrix_t & m);
	void windowMatrix(cairo_matrix_t & m);
	double devPerLogX() const;

	/* drawing helpers */
	void setSrcRGB(uint32_t rgb, double alpha = 1.0);
	void applyPen();
	void paintPath(bool fill, bool stroke);
	void shapePath(double l, double t, double r, double b, int shape,
				   double rw = 0, double rh = 0);
	void ellipsePath(double cx, double cy, double rx, double ry,
					 double a0, double a1, bool neg);
	void arcAngles(double l, double t, double r, double b,
				   double sx, double sy, double ex, double ey,
				   double & a0, double & a1);
	void clipRect(double l, double t, double r, double b, bool exclude);
	void regionClip(const uint8_t * rgn, size_t n);
	void fillRegionRects(const uint8_t * rgn, size_t n, const Brush & b);

	void setPixelV(const Buf & rec, size_t base);
	void lineTo(double x, double y);
	void polyPoints(const Buf & rec, size_t ptsOff, size_t nPts,
					bool is16, bool firstIsMove);
	void polyHelper(const Buf & rec, size_t base, bool is16, int what);
	void polyPolyHelper(const Buf & rec, size_t base, bool is16,
						bool polygon);
	void polyDraw(const Buf & rec, size_t base, bool is16);
	void arcLike(const Buf & rec, size_t base, int what);
	void angleArc(const Buf & rec, size_t base);
	void dibBlt(const Buf & rec, size_t base, int what);
	void textOut(const Buf & rec, bool wide,
				 size_t firstTextOff, uint32_t nTexts);
	void oneText(const Buf & rec, size_t etOff, bool wide);
	void gradientFill(const Buf & rec, size_t base);
	void extCreatePen(const Buf & rec, size_t base);
	void createFont(const Buf & rec, size_t base);
	void parseRgn(const uint8_t * data, size_t n,
				  std::vector<double> & rects);

	void savedc();
	void restoredc(int32_t rel);
	void applyWorld(const Buf & rec, size_t base, int mode);
};

/* ---- transform plumbing --------------------------------------- */

/* window(logical) -> device matrix, honouring the map mode */
void Player::windowMatrix(cairo_matrix_t & m)
{
	double sx = 1.0, sy = 1.0, tx = 0.0, ty = 0.0;
	switch (m_dc.mapMode)
	{
	case MM_ANISOTROPIC:
		sx = m_dc.wnCX != 0 ? m_dc.vpCX / m_dc.wnCX : 1.0;
		sy = m_dc.wnCY != 0 ? m_dc.vpCY / m_dc.wnCY : 1.0;
		break;
	case MM_ISOTROPIC:
	{
		double ax = m_dc.wnCX != 0 ? m_dc.vpCX / m_dc.wnCX : 1.0;
		double ay = m_dc.wnCY != 0 ? m_dc.vpCY / m_dc.wnCY : 1.0;
		double s = std::fabs(ax) < std::fabs(ay) ? std::fabs(ax)
											   : std::fabs(ay);
		sx = std::copysign(s, ax);
		sy = std::copysign(s, ay);
		break;
	}
	case MM_LOMETRIC: sx = sy = m_devDpiX / 254.0; break;
	case MM_HIMETRIC: sx = sy = m_devDpiX / 2540.0; break;
	case MM_LOENGLISH: sx = sy = m_devDpiX / 100.0; break;
	case MM_HIENGLISH: sx = sy = m_devDpiX / 1000.0; break;
	case MM_TWIPS: sx = sy = m_devDpiX / 1440.0; break;
	default: break; /* MM_TEXT */
	}
	if (m_dc.mapMode != MM_TEXT &&
		m_dc.mapMode != MM_ANISOTROPIC && m_dc.mapMode != MM_ISOTROPIC)
		sy = -sy; /* metric modes are y-up */
	tx = m_dc.vpX - m_dc.wnX * sx;
	ty = m_dc.vpY - m_dc.wnY * sy;
	cairo_matrix_init(&m, sx, 0, 0, sy, tx, ty);
}

/* logical -> device, i.e. window * world */
void Player::logicalMatrix(cairo_matrix_t & m)
{
	cairo_matrix_t wnd, world;
	windowMatrix(wnd);
	cairo_matrix_init(&world,
					  m_dc.wt[0], m_dc.wt[1], m_dc.wt[2],
					  m_dc.wt[3], m_dc.wt[4], m_dc.wt[5]);
	cairo_matrix_multiply(&m, &wnd, &world);
}

/* device units per logical unit (average magnitude) — for cosmetic
 * pen widths and anything measured in device space */
double Player::devPerLogX() const
{
	cairo_matrix_t m;
	const_cast<Player *>(this)->logicalMatrix(m);
	return std::sqrt(m.xx * m.xx + m.xy * m.xy);
}

void Player::applyTransform()
{
	cairo_matrix_t log;
	logicalMatrix(log);
	/* device -> output: scale by play ratio, shifted so the picture
	 * bounds land at (0,0) */
	cairo_matrix_t out;
	cairo_matrix_init(&out, m_playScaleX, 0, 0, m_playScaleY,
					  -m_boundsL * m_playScaleX,
					  -m_boundsT * m_playScaleY);
	cairo_matrix_t total;
	cairo_matrix_multiply(&total, &out, &log);
	cairo_set_matrix(m_cr, &total);
}

/* ---- object resolution ---------------------------------------- */

const Obj * Player::obj(uint32_t h) const
{
	auto it = m_objects.find(h);
	return it == m_objects.end() ? nullptr : &it->second;
}

Pen Player::resolvePen(const DC & dc) const
{
	Pen p;
	if (dc.pen & 0x80000000u)
	{
		switch (dc.pen & 0x7FFFFFFFu)
		{
		case STOCK_WHITE_PEN: p.color = 0xFFFFFF; break;
		case STOCK_BLACK_PEN: p.color = 0; break;
		case STOCK_NULL_PEN: p.style = PS_NULL; break;
		case STOCK_DC_PEN: p.color = dc.dcPenColor; break;
		default: break;
		}
		return p;
	}
	const Obj * o = obj(dc.pen);
	if (o && o->kind == K_PEN)
		return o->pen;
	return p;
}

Brush Player::resolveBrush(const DC & dc) const
{
	Brush b;
	if (dc.brush & 0x80000000u)
	{
		switch (dc.brush & 0x7FFFFFFFu)
		{
		case STOCK_WHITE_BRUSH: b.color = 0xFFFFFF; break;
		case STOCK_LTGRAY_BRUSH: b.color = 0xC0C0C0; break;
		case STOCK_GRAY_BRUSH: b.color = 0x808080; break;
		case STOCK_DKGRAY_BRUSH: b.color = 0x404040; break;
		case STOCK_BLACK_BRUSH: b.color = 0; break;
		case STOCK_NULL_BRUSH: b.style = BS_NULL; break;
		case STOCK_DC_BRUSH: b.color = dc.dcBrushColor; break;
		default: break;
		}
		return b;
	}
	const Obj * o = obj(dc.brush);
	if (o && o->kind == K_BRUSH)
		return o->brush;
	return b;
}

Font Player::resolveFont(const DC & dc) const
{
	Font f;
	if (dc.font & 0x80000000u)
	{
		f.height = 12;
		switch (dc.font & 0x7FFFFFFFu)
		{
		case STOCK_OEM_FIXED_FONT:
		case STOCK_ANSI_FIXED_FONT:
		case STOCK_SYSTEM_FIXED_FONT:
			f.face = "Monospace";
			break;
		default:
			f.face = "Sans";
			break;
		}
		return f;
	}
	const Obj * o = obj(dc.font);
	if (o && o->kind == K_FONT)
		return o->font;
	return f;
}

/* ---- drawing helpers ------------------------------------------ */

void Player::setSrcRGB(uint32_t rgb, double alpha)
{
	double r, g, b;
	crgb(rgb, r, g, b);
	cairo_set_source_rgba(m_cr, r, g, b, alpha);
}

void Player::applyPen()
{
	Pen p = resolvePen(m_dc);
	if (p.style == PS_NULL)
		return;
	setSrcRGB(p.color);
	double lw;
	if (p.geometric)
		lw = p.width > 0 ? p.width : 1.0;
	else
	{
		/* cosmetic pen is 1 device px wide */
		double s = devPerLogX();
		lw = s > 1e-9 ? 1.0 / s : 1.0;
	}
	cairo_set_line_width(m_cr, lw);
	cairo_set_line_cap(m_cr, p.cap);
	cairo_set_line_join(m_cr, p.join);
	cairo_set_miter_limit(m_cr, m_dc.miterLimit);
	if (!p.dashes.empty())
		cairo_set_dash(m_cr, p.dashes.data(),
					   static_cast<int>(p.dashes.size()), 0);
	else
	{
		static const double dash1[] = {8, 2};
		static const double dash2[] = {2, 2};
		static const double dash3[] = {9, 2, 2, 2};
		static const double dash4[] = {9, 2, 2, 2, 2, 2};
		const double * d = nullptr;
		int n = 0;
		switch (p.style)
		{
		case PS_DASH: d = dash1; n = 2; break;
		case PS_DOT: d = dash2; n = 2; break;
		case PS_DASHDOT: d = dash3; n = 4; break;
		case PS_DASHDOTDOT: d = dash4; n = 6; break;
		default: break;
		}
		if (d)
			cairo_set_dash(m_cr, d, n, 0);
		else
			cairo_set_dash(m_cr, nullptr, 0, 0);
	}
}

/* paint the current cairo path as a GDI closed shape: fill with the
 * brush (unless NULL) then stroke with the pen (unless NULL) */
void Player::paintPath(bool doFill, bool doStroke)
{
	Brush b = resolveBrush(m_dc);
	Pen p = resolvePen(m_dc);
	if (doFill && b.style != BS_NULL)
	{
		setSrcRGB(b.color);
		if (doStroke && p.style != PS_NULL)
			cairo_fill_preserve(m_cr);
		else
			cairo_fill(m_cr);
	}
	if (doStroke && p.style != PS_NULL)
	{
		applyPen();
		cairo_stroke(m_cr);
	}
	else if (!doFill)
		cairo_new_path(m_cr);
}

void Player::shapePath(double l, double t, double r, double b,
					   int shape, double rw, double rh)
{
	switch (shape)
	{
	case EMR_RECTANGLE:
		cairo_rectangle(m_cr, l, t, r - l, b - t);
		break;
	case EMR_ELLIPSE:
	{
		double cx = (l + r) / 2, cy = (t + b) / 2;
		cairo_save(m_cr);
		cairo_translate(m_cr, cx, cy);
		cairo_scale(m_cr, (r - l) / 2, (b - t) / 2);
		cairo_arc(m_cr, 0, 0, 1, 0, 2 * M_PI);
		cairo_restore(m_cr);
		break;
	}
	case EMR_ROUNDRECT:
	{
		double rx = rw / 2, ry = rh / 2;
		if (rx <= 0) rx = (r - l) / 10;
		if (ry <= 0) ry = (b - t) / 10;
		cairo_save(m_cr);
		cairo_translate(m_cr, l + rx, t + ry);
		cairo_scale(m_cr, rx, ry);
		cairo_arc(m_cr, 0, 0, 1, M_PI, 1.5 * M_PI);
		cairo_restore(m_cr);
		cairo_line_to(m_cr, r - rx, t);
		cairo_save(m_cr);
		cairo_translate(m_cr, r - rx, t + ry);
		cairo_scale(m_cr, rx, ry);
		cairo_arc(m_cr, 0, 0, 1, 1.5 * M_PI, 2 * M_PI);
		cairo_restore(m_cr);
		cairo_line_to(m_cr, r, b - ry);
		cairo_save(m_cr);
		cairo_translate(m_cr, r - rx, b - ry);
		cairo_scale(m_cr, rx, ry);
		cairo_arc(m_cr, 0, 0, 1, 0, 0.5 * M_PI);
		cairo_restore(m_cr);
		cairo_line_to(m_cr, l + rx, b);
		cairo_save(m_cr);
		cairo_translate(m_cr, l + rx, b - ry);
		cairo_scale(m_cr, rx, ry);
		cairo_arc(m_cr, 0, 0, 1, 0.5 * M_PI, M_PI);
		cairo_restore(m_cr);
		cairo_close_path(m_cr);
		break;
	}
	default:
		break;
	}
}

void Player::ellipsePath(double cx, double cy, double rx, double ry,
						 double a0, double a1, bool neg)
{
	cairo_save(m_cr);
	cairo_translate(m_cr, cx, cy);
	cairo_scale(m_cr, rx, ry);
	if (neg)
		cairo_arc_negative(m_cr, 0, 0, 1, a0, a1);
	else
		cairo_arc(m_cr, 0, 0, 1, a0, a1);
	cairo_restore(m_cr);
}

void Player::arcAngles(double l, double t, double r, double b,
					   double sx, double sy, double ex, double ey,
					   double & a0, double & a1)
{
	double cx = (l + r) / 2, cy = (t + b) / 2;
	double rx = (r - l) / 2, ry = (b - t) / 2;
	if (rx <= 0) rx = 1;
	if (ry <= 0) ry = 1;
	a0 = std::atan2((sy - cy) / ry, (sx - cx) / rx);
	a1 = std::atan2((ey - cy) / ry, (ex - cx) / rx);
	if (m_dc.arcDir == 2) /* AD_CLOCKWISE */
	{
		if (a1 > a0) a1 -= 2 * M_PI;
	}
	else
	{
		if (a1 < a0) a1 += 2 * M_PI;
	}
}

void Player::clipRect(double l, double t, double r, double b,
					  bool exclude)
{
	if (!exclude)
	{
		cairo_rectangle(m_cr, l, t, r - l, b - t);
		cairo_clip(m_cr);
		return;
	}
	/* exclude = huge rect minus the inner one, even-odd */
	cairo_save(m_cr);
	cairo_set_fill_rule(m_cr, CAIRO_FILL_RULE_EVEN_ODD);
	cairo_new_path(m_cr);
	const double B = 1e7;
	cairo_rectangle(m_cr, -B, -B, 2 * B, 2 * B);
	cairo_rectangle(m_cr, l, t, r - l, b - t);
	cairo_clip(m_cr);
	cairo_restore(m_cr);
}

/* RGNDATA -> vector of {l,t,r,b} quads */
void Player::parseRgn(const uint8_t * data, size_t n,
					  std::vector<double> & rects)
{
	Buf b{data, n};
	uint32_t size = 0, type = 0, count = 0;
	if (!b.u32(size, 0) || !b.u32(type, 4) || !b.u32(count, 8) || type != 1)
		return;
	size_t off = 32; /* RGNDATAHEADER */
	for (uint32_t i = 0; i < count && off + 16 <= n; ++i, off += 16)
	{
		double l, t, r, bt;
		if (b.rect(l, t, r, bt, off))
		{
			rects.push_back(l);
			rects.push_back(t);
			rects.push_back(r);
			rects.push_back(bt);
		}
	}
}

void Player::regionClip(const uint8_t * rgn, size_t n)
{
	std::vector<double> rects;
	parseRgn(rgn, n, rects);
	cairo_new_path(m_cr);
	for (size_t i = 0; i + 3 < rects.size(); i += 4)
		cairo_rectangle(m_cr, rects[i], rects[i + 1],
						rects[i + 2] - rects[i], rects[i + 3] - rects[i + 1]);
	cairo_clip(m_cr);
}

void Player::fillRegionRects(const uint8_t * rgn, size_t n,
							 const Brush & b)
{
	if (b.style == BS_NULL)
		return;
	std::vector<double> rects;
	parseRgn(rgn, n, rects);
	for (size_t i = 0; i + 3 < rects.size(); i += 4)
		cairo_rectangle(m_cr, rects[i], rects[i + 1],
						rects[i + 2] - rects[i], rects[i + 3] - rects[i + 1]);
	setSrcRGB(b.color);
	cairo_fill(m_cr);
}

/* ---- records -------------------------------------------------- */

void Player::setPixelV(const Buf & rec, size_t base)
{
	double x, y;
	uint32_t rgb;
	if (!rec.point(x, y, base + 8) || !rec.u32(rgb, base + 16))
		return;
	double dpl = 1.0 / std::max(devPerLogX(), 1e-9);
	cairo_rectangle(m_cr, x, y, dpl, dpl);
	setSrcRGB(rgb);
	cairo_fill(m_cr);
}

void Player::lineTo(double x, double y)
{
	if (m_inPath)
	{
		cairo_line_to(m_cr, x, y);
	}
	else
	{
		cairo_new_path(m_cr);
		cairo_move_to(m_cr, m_dc.curX, m_dc.curY);
		cairo_line_to(m_cr, x, y);
		applyPen();
		cairo_stroke(m_cr);
	}
	m_dc.curX = x;
	m_dc.curY = y;
}

/* emit one poly primitive; what: EMR_POLYGON, EMR_POLYLINE,
 * EMR_POLYBEZIER, EMR_POLYLINETO, EMR_POLYBEZIERTO */
void Player::polyPoints(const Buf & rec, size_t ptsOff, size_t nPts,
						bool is16, bool firstIsMove)
{
	double x, y;
	if (nPts == 0)
		return;
	if (firstIsMove)
	{
		if (is16 ? rec.point16(x, y, ptsOff) : rec.point(x, y, ptsOff))
		{
			cairo_move_to(m_cr, x, y);
			m_dc.curX = x;
			m_dc.curY = y;
		}
		else
			return;
	}
	else
		cairo_move_to(m_cr, m_dc.curX, m_dc.curY);
	size_t step = is16 ? 4 : 8;
	for (size_t i = firstIsMove ? 1 : 0; i < nPts; ++i)
	{
		if (!(is16 ? rec.point16(x, y, ptsOff + i * step)
				   : rec.point(x, y, ptsOff + i * step)))
			break;
		cairo_line_to(m_cr, x, y);
		m_dc.curX = x;
		m_dc.curY = y;
	}
}

void Player::polyHelper(const Buf & rec, size_t base, bool is16,
					  int what)
{
	/* layout: rclBounds(16) cptl(4) points[] */
	uint32_t nPts = 0;
	if (!rec.u32(nPts, base + 24) || nPts == 0 || nPts > (1u << 22))
		return;
	size_t ptsOff = base + 28;
	cairo_new_path(m_cr);
	if (what == EMR_POLYBEZIER || what == EMR_POLYBEZIERTO)
	{
		/* cubic series: [p0] c1 c2 p1 c1 c2 p2 ... */
		size_t i = 0;
		double x, y;
		if (what == EMR_POLYBEZIER)
		{
			if (!(is16 ? rec.point16(x, y, ptsOff)
					   : rec.point(x, y, ptsOff)))
				return;
			cairo_move_to(m_cr, x, y);
			m_dc.curX = x;
			m_dc.curY = y;
			i = 1;
		}
		else
			cairo_move_to(m_cr, m_dc.curX, m_dc.curY);
		size_t step = is16 ? 4 : 8;
		for (; i + 2 < nPts; i += 3)
		{
			double x1, y1, x2, y2;
			bool ok = is16
				? (rec.point16(x1, y1, ptsOff + i * step) &&
				   rec.point16(x2, y2, ptsOff + (i + 1) * step) &&
				   rec.point16(x, y, ptsOff + (i + 2) * step))
				: (rec.point(x1, y1, ptsOff + i * step) &&
				   rec.point(x2, y2, ptsOff + (i + 1) * step) &&
				   rec.point(x, y, ptsOff + (i + 2) * step));
			if (!ok)
				break;
			cairo_curve_to(m_cr, x1, y1, x2, y2, x, y);
			m_dc.curX = x;
			m_dc.curY = y;
		}
		if (m_inPath)
			return;
		applyPen();
		cairo_stroke(m_cr);
		return;
	}

	polyPoints(rec, ptsOff, nPts, is16,
			   what == EMR_POLYGON || what == EMR_POLYLINE);
	if (what == EMR_POLYGON)
	{
		cairo_close_path(m_cr);
		if (m_inPath)
			return;
		cairo_set_fill_rule(m_cr,
							m_dc.polyFill == 2 ? CAIRO_FILL_RULE_WINDING
											   : CAIRO_FILL_RULE_EVEN_ODD);
		paintPath(true, true);
		cairo_set_fill_rule(m_cr, CAIRO_FILL_RULE_WINDING);
	}
	else
	{
		if (m_inPath)
			return;
		applyPen();
		cairo_stroke(m_cr);
	}
}

void Player::polyPolyHelper(const Buf & rec, size_t base, bool is16,
							bool polygon)
{
	/* rclBounds(16) nPolys(4) cptl(4) aPolyCounts[nPolys] points[] */
	uint32_t nPolys = 0, cptl = 0;
	if (!rec.u32(nPolys, base + 24) || !rec.u32(cptl, base + 28) ||
		nPolys == 0 || nPolys > 4096)
		return;
	size_t countsOff = base + 32;
	size_t ptsOff = countsOff + static_cast<size_t>(nPolys) * 4;
	size_t step = is16 ? 4 : 8;
	cairo_new_path(m_cr);
	size_t off = ptsOff;
	for (uint32_t pi = 0; pi < nPolys; ++pi)
	{
		uint32_t cnt = 0;
		if (!rec.u32(cnt, countsOff + pi * 4) || cnt > (1u << 22))
			break;
		polyPoints(rec, off, cnt, is16, true);
		if (polygon)
			cairo_close_path(m_cr);
		off += static_cast<size_t>(cnt) * step;
	}
	if (m_inPath)
		return;
	if (polygon)
	{
		cairo_set_fill_rule(m_cr,
							m_dc.polyFill == 2 ? CAIRO_FILL_RULE_WINDING
											   : CAIRO_FILL_RULE_EVEN_ODD);
		paintPath(true, true);
		cairo_set_fill_rule(m_cr, CAIRO_FILL_RULE_WINDING);
	}
	else
	{
		applyPen();
		cairo_stroke(m_cr);
	}
}

void Player::polyDraw(const Buf & rec, size_t base, bool is16)
{
	/* rclBounds(16) cptl(4) aPts[] abTypes[] */
	uint32_t nPts = 0;
	if (!rec.u32(nPts, base + 24) || nPts == 0 || nPts > (1u << 22))
		return;
	size_t ptsOff = base + 28;
	size_t typesOff = ptsOff + static_cast<size_t>(nPts) * (is16 ? 4 : 8);
	double x, y;
	cairo_new_path(m_cr);
	for (uint32_t i = 0; i < nPts; ++i)
	{
		uint8_t ty = 0;
		if (!rec.u8(ty, typesOff + i))
			break;
		if (!(is16 ? rec.point16(x, y, ptsOff + static_cast<size_t>(i) * (is16 ? 4 : 8))
				   : rec.point(x, y, ptsOff + static_cast<size_t>(i) * 8)))
			break;
		uint8_t code = ty & 0xFE;
		if (code == 0x06) /* PT_MOVETO */
		{
			cairo_move_to(m_cr, x, y);
			m_dc.curX = x;
			m_dc.curY = y;
		}
		else if (code == 0x02) /* PT_LINETO */
		{
			cairo_line_to(m_cr, x, y);
			m_dc.curX = x;
			m_dc.curY = y;
		}
		else if (code == 0x04) /* PT_BEZIERTO: groups of 3 */
		{
			double x2, y2, x3, y3;
			if (i + 2 < nPts)
			{
				bool ok = is16
					? (rec.point16(x2, y2, ptsOff + (i + 1) * 4) &&
					   rec.point16(x3, y3, ptsOff + (i + 2) * 4))
					: (rec.point(x2, y2, ptsOff + (i + 1) * 8) &&
					   rec.point(x3, y3, ptsOff + (i + 2) * 8));
				if (!ok)
					break;
				cairo_curve_to(m_cr, x, y, x2, y2, x3, y3);
				m_dc.curX = x3;
				m_dc.curY = y3;
				i += 2;
			}
		}
		if (ty & 0x01) /* PT_CLOSEFIGURE */
			cairo_close_path(m_cr);
	}
	if (m_inPath)
		return;
	Brush b = resolveBrush(m_dc);
	Pen p = resolvePen(m_dc);
	if (b.style != BS_NULL || p.style != PS_NULL)
	{
		cairo_set_fill_rule(m_cr,
							m_dc.polyFill == 2 ? CAIRO_FILL_RULE_WINDING
											   : CAIRO_FILL_RULE_EVEN_ODD);
		if (b.style != BS_NULL)
		{
			setSrcRGB(b.color);
			cairo_fill_preserve(m_cr);
		}
		if (p.style != PS_NULL)
		{
			applyPen();
			cairo_stroke(m_cr);
		}
		else
			cairo_new_path(m_cr);
		cairo_set_fill_rule(m_cr, CAIRO_FILL_RULE_WINDING);
	}
}

void Player::arcLike(const Buf & rec, size_t base, int what)
{
	double l, t, r, b, sx, sy, ex, ey;
	if (!rec.rect(l, t, r, b, base + 8) ||
		!rec.point(sx, sy, base + 24) || !rec.point(ex, ey, base + 32))
		return;
	double cx = (l + r) / 2, cy = (t + b) / 2;
	double rx = (r - l) / 2, ry = (b - t) / 2;
	double a0, a1;
	arcAngles(l, t, r, b, sx, sy, ex, ey, a0, a1);
	bool neg = m_dc.arcDir == 2;
	if (m_inPath)
	{
		if (what == EMR_ARC || what == EMR_ARCTO)
			ellipsePath(cx, cy, rx, ry, a0, a1, neg);
		else
		{
			ellipsePath(cx, cy, rx, ry, a0, a1, neg);
			if (what == EMR_CHORD)
				cairo_close_path(m_cr);
			else /* PIE */
			{
				cairo_line_to(m_cr, cx, cy);
				cairo_close_path(m_cr);
			}
		}
		return;
	}
	cairo_new_path(m_cr);
	ellipsePath(cx, cy, rx, ry, a0, a1, neg);
	if (what == EMR_ARC || what == EMR_ARCTO)
	{
		if (what == EMR_ARCTO)
		{
			/* arc continues from current position */
			cairo_new_path(m_cr);
			cairo_move_to(m_cr, m_dc.curX, m_dc.curY);
			cairo_line_to(m_cr, sx, sy);
			ellipsePath(cx, cy, rx, ry, a0, a1, neg);
			m_dc.curX = ex;
			m_dc.curY = ey;
		}
		applyPen();
		cairo_stroke(m_cr);
		return;
	}
	if (what == EMR_CHORD)
		cairo_close_path(m_cr);
	else /* PIE */
	{
		cairo_line_to(m_cr, cx, cy);
		cairo_close_path(m_cr);
	}
	paintPath(true, true);
}

void Player::angleArc(const Buf & rec, size_t base)
{
	/* ptlCenter(8) nRadius(4) eStartAngle(4f) eSweepAngle(4f) */
	double cx, cy;
	uint32_t radius = 0;
	float a0 = 0, sweep = 0;
	if (!rec.point(cx, cy, base + 8) ||
		!rec.u32(radius, base + 16) ||
		!rec.f32(a0, base + 20) || !rec.f32(sweep, base + 24))
		return;
	double sa = a0 * M_PI / 180.0;
	double ea = sa + sweep * M_PI / 180.0;
	cairo_new_path(m_cr);
	cairo_move_to(m_cr, m_dc.curX, m_dc.curY);
	cairo_line_to(m_cr,
				  cx + radius * std::cos(sa),
				  cy + radius * std::sin(sa));
	cairo_arc(m_cr, cx, cy, radius, sa, ea);
	if (!m_inPath)
	{
		applyPen();
		cairo_stroke(m_cr);
	}
	m_dc.curX = cx + radius * std::cos(ea);
	m_dc.curY = cy + radius * std::sin(ea);
}

/* EMR_BITBLT / STRETCHBLT / STRETCHDIBITS / SETDIBITSTODEVICE /
 * ALPHABLEND / TRANSPARENTBLT — DIB composite helpers */
void Player::dibBlt(const Buf & rec, size_t base, int what)
{
	double xDest = 0, yDest = 0, cxDest = 0, cyDest = 0;
	double xSrc = 0, ySrc = 0, cxSrc = 0, cySrc = 0;
	uint32_t offBmi = 0, cbBmi = 0, offBits = 0, cbBits = 0;
	uint32_t iUsage = 0, rop = 0;
	double alpha = 1.0;
	size_t hdr;

	switch (what)
	{
	case EMR_STRETCHDIBITS:
		/* xDest,yDest(8) xSrc,ySrc(8) cxSrc,cySrc(8) offBmi,cbBmi(8)
		 * offBits,cbBits(8) iUsage(4) dwRop(4) cxDest,cyDest(8) */
		hdr = base + 24;
		rec.point(xDest, yDest, hdr);
		rec.point(xSrc, ySrc, hdr + 8);
		rec.point(cxSrc, cySrc, hdr + 16);
		rec.u32(offBmi, hdr + 24);
		rec.u32(cbBmi, hdr + 28);
		rec.u32(offBits, hdr + 32);
		rec.u32(cbBits, hdr + 36);
		rec.u32(iUsage, hdr + 40);
		rec.u32(rop, hdr + 44);
		rec.point(cxDest, cyDest, hdr + 48);
		break;
	case EMR_BITBLT:
	case EMR_STRETCHBLT:
	case EMR_MASKBLT:
		/* xDest,yDest,cxDest,cyDest(16) dwRop(4) xSrc,ySrc(8)
		 * xformSrc(24) crBk(4) iUsage(4) offBmi,cbBmi(8)
		 * offBits,cbBits(8) */
		hdr = base + 24;
		rec.point(xDest, yDest, hdr);
		rec.point(cxDest, cyDest, hdr + 8);
		rec.u32(rop, hdr + 16);
		rec.point(xSrc, ySrc, hdr + 20);
		/* skip src xform (24) + bk color (4) */
		rec.u32(iUsage, hdr + 56);
		rec.u32(offBmi, hdr + 60);
		rec.u32(cbBmi, hdr + 64);
		rec.u32(offBits, hdr + 68);
		rec.u32(cbBits, hdr + 72);
		cxSrc = cxDest;
		cySrc = cyDest;
		break;
	case EMR_SETDIBITSTODEVICE:
		/* xDest,yDest(8) xSrc,ySrc(8) cxSrc,cySrc(8)
		 * offBmi,cbBmi(8) offBits,cbBits(8) iUsage(4) iStartScan(4)
		 * cScans(4) */
		hdr = base + 24;
		rec.point(xDest, yDest, hdr);
		rec.point(xSrc, ySrc, hdr + 8);
		rec.point(cxSrc, cySrc, hdr + 16);
		rec.u32(offBmi, hdr + 24);
		rec.u32(cbBmi, hdr + 28);
		rec.u32(offBits, hdr + 32);
		rec.u32(cbBits, hdr + 36);
		rec.u32(iUsage, hdr + 40);
		cxDest = cxSrc;
		cyDest = cySrc;
		break;
	case EMR_ALPHABLEND:
		/* xDest,yDest,cxDest,cyDest(16) dwRop(4) xSrc,ySrc(8)
		 * xform(24) crBk(4) iUsage(4) offBmi,cbBmi(8) offBits,cbBits(8)
		 * cxSrc,cySrc(8) */
		hdr = base + 24;
		rec.point(xDest, yDest, hdr);
		rec.point(cxDest, cyDest, hdr + 8);
		rec.u32(rop, hdr + 16);
		rec.point(xSrc, ySrc, hdr + 20);
		rec.u32(iUsage, hdr + 56);
		rec.u32(offBmi, hdr + 60);
		rec.u32(cbBmi, hdr + 64);
		rec.u32(offBits, hdr + 68);
		rec.u32(cbBits, hdr + 72);
		rec.point(cxSrc, cySrc, hdr + 76);
		alpha = ((rop >> 24) & 0xFF) / 255.0;
		break;
	default:
		return;
	}

	if (rop == 0x00000042) /* BLACKNESS */
	{
		cairo_rectangle(m_cr, xDest, yDest, cxDest, cyDest);
		setSrcRGB(0);
		cairo_fill(m_cr);
		return;
	}
	if (rop == 0x00FF0062) /* WHITENESS */
	{
		cairo_rectangle(m_cr, xDest, yDest, cxDest, cyDest);
		setSrcRGB(0xFFFFFF);
		cairo_fill(m_cr);
		return;
	}
	if (cxSrc <= 0 || cySrc <= 0 || cxDest == 0 || cyDest == 0)
		return;

	cairo_surface_t * dib =
		dibToSurface(rec, base + offBmi, cbBmi,
					 base + offBits, cbBits, iUsage);
	if (!dib)
		return;

	double sx = cxDest / cxSrc;
	double sy = cyDest / cySrc;
	cairo_save(m_cr);
	cairo_rectangle(m_cr, xDest, yDest, cxDest, cyDest);
	cairo_clip(m_cr);
	cairo_translate(m_cr, xDest - xSrc * sx, yDest - ySrc * sy);
	cairo_scale(m_cr, sx, sy);
	cairo_set_source_surface(m_cr, dib, 0, 0);
	cairo_paint_with_alpha(m_cr, alpha);
	cairo_restore(m_cr);
	cairo_surface_destroy(dib);
}

/* EMRTEXT: ptlRef(8) nChars(4) offString(4) fOptions(4) rcl(16)
 * offDx(4) — string data offsets are relative to the RECORD start */
void Player::oneText(const Buf & rec, size_t etOff, bool wide)
{
	double x, y;
	uint32_t nChars = 0, offStr = 0, opts = 0, offDx = 0;
	if (!rec.point(x, y, etOff) ||
		!rec.u32(nChars, etOff + 8) ||
		!rec.u32(offStr, etOff + 12) ||
		!rec.u32(opts, etOff + 16) ||
		!rec.u32(offDx, etOff + 36))
		return;
	if (nChars == 0 || nChars > (1u << 20))
		return;
	double rl, rt, rr, rb;
	rec.rect(rl, rt, rr, rb, etOff + 20);

	Font f = resolveFont(m_dc);
	cairo_select_font_face(
		m_cr, f.face.empty() ? "Sans" : f.face.c_str(),
		f.italic ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL,
		f.weight >= 600 ? CAIRO_FONT_WEIGHT_BOLD
						: CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(m_cr, f.height != 0 ? std::fabs(
								static_cast<double>(f.height)) : 12);

	std::string utf8;
	if (wide)
	{
		size_t off = offStr; /* record-relative */
		utf8.reserve(nChars * 3);
		for (uint32_t i = 0; i < nChars; ++i)
		{
			uint16_t ch = 0;
			if (!rec.u16(ch, off + i * 2))
				break;
			if (ch < 0x80)
				utf8 += static_cast<char>(ch);
			else if (ch < 0x800)
			{
				utf8 += static_cast<char>(0xC0 | (ch >> 6));
				utf8 += static_cast<char>(0x80 | (ch & 0x3F));
			}
			else if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < nChars)
			{
				uint16_t lo = 0;
				if (!rec.u16(lo, off + (i + 1) * 2) ||
					lo < 0xDC00 || lo > 0xDFFF)
					continue;
				++i;
				uint32_t cp = 0x10000 + ((ch - 0xD800) << 10) + (lo - 0xDC00);
				utf8 += static_cast<char>(0xF0 | (cp >> 18));
				utf8 += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
				utf8 += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
				utf8 += static_cast<char>(0x80 | (cp & 0x3F));
			}
			else
			{
				utf8 += static_cast<char>(0xE0 | (ch >> 12));
				utf8 += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
				utf8 += static_cast<char>(0x80 | (ch & 0x3F));
			}
		}
	}
	else
	{
		size_t off = offStr;
		std::string bytes;
		const uint8_t * s = rec.at(off, nChars);
		if (s)
			bytes.assign(reinterpret_cast<const char *>(s), nChars);
		/* EXTTEXTOUTA is in the current codepage — CP1252 is the
		 * practical choice for Western clip-art */
		gchar * u = g_convert(bytes.data(), static_cast<gssize>(bytes.size()),
							  "UTF-8", "CP1252", nullptr, nullptr, nullptr);
		if (u)
		{
			utf8 = u;
			g_free(u);
		}
		else
			utf8 = bytes;
	}
	if (utf8.empty())
		return;

	cairo_save(m_cr);
	if (opts & ETO_CLIPPED)
	{
		cairo_rectangle(m_cr, rl, rt, rr - rl, rb - rt);
		cairo_clip(m_cr);
	}
	if (opts & ETO_OPAQUE)
	{
		cairo_save(m_cr);
		cairo_new_path(m_cr);
		cairo_rectangle(m_cr, rl, rt, rr - rl, rb - rt);
		setSrcRGB(m_dc.bkColor);
		cairo_fill(m_cr);
		cairo_restore(m_cr);
	}
	setSrcRGB(m_dc.textColor);

	/* alignment */
	double tx = x, ty = y;
	cairo_text_extents_t ext;
	cairo_text_extents(m_cr, utf8.c_str(), &ext);
	cairo_font_extents_t fext;
	cairo_font_extents(m_cr, &fext);
	unsigned halign = m_dc.textAlign & 0x6;
	unsigned valign = m_dc.textAlign & 0x18;
	if (halign == 0x2) /* TA_RIGHT */
		tx -= ext.x_advance;
	else if (halign == 0x6) /* TA_CENTER */
		tx -= ext.x_advance / 2;
	if (valign == 0x0) /* TA_TOP */
		ty += fext.ascent;
	else if (valign == 0x8) /* TA_BOTTOM */
		ty -= fext.descent;
	/* TA_BASELINE (0x18): ty as given */

	/* dx array: per-glyph origins — distances between adjacent char
	 * origins in logical units */
	if (offDx)
	{
		double ox = tx;
		size_t ci = 0;
		for (uint32_t i = 0; i < nChars && ci < utf8.size(); ++i)
		{
			uint8_t clen = (static_cast<uint8_t>(utf8[ci]) & 0x80)
				? ((static_cast<uint8_t>(utf8[ci]) & 0xE0) == 0xC0 ? 2 :
				   (static_cast<uint8_t>(utf8[ci]) & 0xF0) == 0xE0 ? 3 : 4)
				: 1;
			int32_t dx = 0;
			if (!rec.i32(dx, offDx + i * 4))
				break;
			cairo_move_to(m_cr, ox, ty);
			cairo_show_text(m_cr, utf8.substr(ci, clen).c_str());
			ox += dx;
			ci += clen;
		}
	}
	else
	{
		cairo_move_to(m_cr, tx, ty);
		cairo_show_text(m_cr, utf8.c_str());
	}

	/* underline / strikeout */
	if (f.underline || f.strikeout)
	{
		double yy = ty + (f.underline ? fext.descent * 0.5
									  : -fext.ascent * 0.3);
		cairo_set_line_width(m_cr, std::fabs(
								 static_cast<double>(f.height)) / 14.0);
		cairo_move_to(m_cr, tx, yy);
		cairo_line_to(m_cr, tx + ext.x_advance, yy);
		cairo_stroke(m_cr);
	}
	cairo_restore(m_cr);
}

void Player::textOut(const Buf & rec, bool wide,
					 size_t firstTextOff, uint32_t nTexts)
{
	for (uint32_t i = 0; i < nTexts; ++i)
		oneText(rec, firstTextOff + i * 40, wide);
}

void Player::gradientFill(const Buf & rec, size_t base)
{
	/* rclBounds(16) nVer(4) nTri(4) ulMode(4) vertexes[TRIVERTEX]
	 * meshes[] — implement the two rectangle modes */
	uint32_t nVer = 0, nTri = 0, mode = 0;
	if (!rec.u32(nVer, base + 24) || !rec.u32(nTri, base + 28) ||
		!rec.u32(mode, base + 32) || nVer < 2 || nVer > 4096 || mode > 1)
		return;
	size_t vOff = base + 36;
	size_t mOff = vOff + static_cast<size_t>(nVer) * 16;
	for (uint32_t i = 0; i < nTri; ++i)
	{
		uint32_t ul = 0, lr = 0;
		if (!rec.u32(ul, mOff + i * 8) || !rec.u32(lr, mOff + i * 8 + 4) ||
			ul >= nVer || lr >= nVer)
			break;
		double x1, y1, x2, y2;
		uint16_t r1, g1, b1, r2, g2, b2;
		rec.point(x1, y1, vOff + ul * 16);
		rec.point(x2, y2, vOff + lr * 16);
		rec.u16(r1, vOff + ul * 16 + 8);
		rec.u16(g1, vOff + ul * 16 + 10);
		rec.u16(b1, vOff + ul * 16 + 12);
		rec.u16(r2, vOff + lr * 16 + 8);
		rec.u16(g2, vOff + lr * 16 + 10);
		rec.u16(b2, vOff + lr * 16 + 12);
		cairo_pattern_t * pat =
			mode == 0
				? cairo_pattern_create_linear(x1, y1, x2, y1)
				: cairo_pattern_create_linear(x1, y1, x1, y2);
		cairo_pattern_add_color_stop_rgb(pat, 0, tri16(r1), tri16(g1),
										 tri16(b1));
		cairo_pattern_add_color_stop_rgb(pat, 1, tri16(r2), tri16(g2),
										 tri16(b2));
		cairo_save(m_cr);
		cairo_new_path(m_cr);
		cairo_rectangle(m_cr, std::min(x1, x2), std::min(y1, y2),
						std::fabs(x2 - x1), std::fabs(y2 - y1));
		cairo_set_source(m_cr, pat);
		cairo_fill(m_cr);
		cairo_restore(m_cr);
		cairo_pattern_destroy(pat);
	}
}

void Player::extCreatePen(const Buf & rec, size_t base)
{
	/* ihPen(4) offBmi(4) cbBmi(4) offBits(4) cbBits(4)
	 * elpPenStyle(4) elpWidth(4) elpBrushStyle(4) elpColor(4)
	 * elpHatch(4) elpNumEntries(4) elpStyleEntry[] */
	uint32_t ih = 0, penStyle = 0, width = 0, brushStyle = 0,
		color = 0, nEntries = 0;
	if (!rec.u32(ih, base + 8) || !rec.u32(penStyle, base + 28) ||
		!rec.u32(width, base + 32) || !rec.u32(brushStyle, base + 36) ||
		!rec.u32(color, base + 40) || !rec.u32(nEntries, base + 48))
		return;
	Obj & o = m_objects[ih];
	o = Obj();
	o.kind = K_PEN;
	o.pen.style = penStyle & PS_STYLE_MASK;
	o.pen.geometric = (penStyle & PS_TYPE_MASK) == PS_GEOMETRIC;
	o.pen.width = width;
	o.pen.color = color;
	switch (penStyle & PS_JOIN_MASK)
	{
	case 0x1000: o.pen.join = CAIRO_LINE_JOIN_BEVEL; break;
	case 0x2000: o.pen.join = CAIRO_LINE_JOIN_MITER; break;
	default: o.pen.join = CAIRO_LINE_JOIN_ROUND; break;
	}
	switch (penStyle & PS_ENDCAP_MASK)
	{
	case 0x000: o.pen.cap = CAIRO_LINE_CAP_BUTT; break;
	case 0x100: o.pen.cap = CAIRO_LINE_CAP_ROUND; break;
	case 0x200: o.pen.cap = CAIRO_LINE_CAP_SQUARE; break;
	default: break;
	}
	if (o.pen.style == PS_USERSTYLE && nEntries > 0)
	{
		size_t off = base + 52;
		for (uint32_t i = 0; i < nEntries && i < 64; ++i)
		{
			uint32_t d = 0;
			if (!rec.u32(d, off + i * 4))
				break;
			o.pen.dashes.push_back(d);
		}
	}
}

void Player::createFont(const Buf & rec, size_t base)
{
	/* ihFont(4) then a LOGFONT-like structure; EMR_EXTCREATEFONTINDIRECTW
	 * carries an EXTLOGFONTW: elfLogFont LOGFONTW (92B) +
	 * elfFullName + elfStyle + elfScript */
	uint32_t ih = 0;
	if (!rec.u32(ih, base + 8))
		return;
	size_t lf = base + 12;
	int32_t height = 0, width = 0, esc = 0, weight = 0, orient = 0;
	uint8_t italic = 0, ul = 0, so = 0, charset = 0;
	rec.i32(height, lf);
	rec.i32(width, lf + 4);
	rec.i32(esc, lf + 8);
	rec.i32(orient, lf + 12);
	rec.i32(weight, lf + 16);
	rec.u8(italic, lf + 20);
	rec.u8(ul, lf + 21);
	rec.u8(so, lf + 22);
	rec.u8(charset, lf + 23);
	Obj & o = m_objects[ih];
	o = Obj();
	o.kind = K_FONT;
	o.font.height = height != 0 ? height : 12;
	o.font.width = width;
	o.font.escapement = esc;
	o.font.weight = weight;
	o.font.italic = italic != 0;
	o.font.underline = ul != 0;
	o.font.strikeout = so != 0;
	o.font.vertical = orient != 0;
	/* face name: WCHAR[32] at lf+28 for LOGFONTW */
	std::string face;
	for (int i = 0; i < 32; ++i)
	{
		uint16_t ch = 0;
		if (!rec.u16(ch, lf + 28 + i * 2) || ch == 0)
			break;
		if (ch < 0x80)
			face += static_cast<char>(ch);
		else if (ch < 0x800)
		{
			face += static_cast<char>(0xC0 | (ch >> 6));
			face += static_cast<char>(0x80 | (ch & 0x3F));
		}
		else
		{
			face += static_cast<char>(0xE0 | (ch >> 12));
			face += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
			face += static_cast<char>(0x80 | (ch & 0x3F));
		}
	}
	if (!face.empty())
		o.font.face = face;
	(void)charset;
}

void Player::savedc()
{
	m_stack.push_back(m_dc);
	cairo_save(m_cr);
}

void Player::restoredc(int32_t rel)
{
	if (m_stack.empty())
		return;
	size_t target;
	if (rel < 0)
		target = static_cast<size_t>(-rel) <= m_stack.size()
			? m_stack.size() - static_cast<size_t>(-rel)
			: 0;
	else if (rel > 0)
		target = static_cast<size_t>(rel) <= m_stack.size()
			? static_cast<size_t>(rel) - 1
			: m_stack.size() - 1;
	else
		return;
	while (m_stack.size() > target + 1)
	{
		m_stack.pop_back();
		cairo_restore(m_cr);
	}
	m_dc = m_stack.back();
	m_stack.pop_back();
	cairo_restore(m_cr);
}

void Player::applyWorld(const Buf & rec, size_t base, int mode)
{
	/* XFORM: eM11 eM12 eM21 eM22 eDx eDy (FLOATL) */
	float m[6];
	for (int i = 0; i < 6; ++i)
		if (!rec.f32(m[i], base + 8 + i * 4))
			return;
	double nw[6];
	switch (mode)
	{
	case 1: /* MWT_IDENTITY */
		m_dc.wt[0] = 1; m_dc.wt[1] = 0; m_dc.wt[2] = 0;
		m_dc.wt[3] = 1; m_dc.wt[4] = 0; m_dc.wt[5] = 0;
		return;
	case 2: /* MWT_LEFTMULTIPLY: new = m * wt */
		nw[0] = m[0] * m_dc.wt[0] + m[2] * m_dc.wt[1];
		nw[1] = m[1] * m_dc.wt[0] + m[3] * m_dc.wt[1];
		nw[2] = m[0] * m_dc.wt[2] + m[2] * m_dc.wt[3];
		nw[3] = m[1] * m_dc.wt[2] + m[3] * m_dc.wt[3];
		nw[4] = m[0] * m_dc.wt[4] + m[2] * m_dc.wt[5] + m[4];
		nw[5] = m[1] * m_dc.wt[4] + m[3] * m_dc.wt[5] + m[5];
		break;
	case 3: /* MWT_RIGHTMULTIPLY: new = wt * m */
		nw[0] = m_dc.wt[0] * m[0] + m_dc.wt[2] * m[1];
		nw[1] = m_dc.wt[1] * m[0] + m_dc.wt[3] * m[1];
		nw[2] = m_dc.wt[0] * m[2] + m_dc.wt[2] * m[3];
		nw[3] = m_dc.wt[1] * m[2] + m_dc.wt[3] * m[3];
		nw[4] = m_dc.wt[0] * m[4] + m_dc.wt[2] * m[5] + m_dc.wt[4];
		nw[5] = m_dc.wt[1] * m[4] + m_dc.wt[3] * m[5] + m_dc.wt[5];
		break;
	default: /* MWT_SET */
		for (int i = 0; i < 6; ++i)
			m_dc.wt[i] = m[i];
		return;
	}
	for (int i = 0; i < 6; ++i)
		m_dc.wt[i] = nw[i];
}

/* ---- main record loop ----------------------------------------- */

bool Player::run(size_t pos, size_t fileLen)
{
	size_t guard = 0;
	while (pos + 8 <= m_file.n && pos < fileLen)
	{
		uint32_t type = 0, size = 0;
		if (!m_file.u32(type, pos) || !m_file.u32(size, pos + 4) ||
			size < 8 || pos + size > m_file.n)
			break;
		if (++guard > 1000000)
			break;

		Buf rec{m_file.p + pos, size};
		size_t base = 0; /* record-relative offsets */

		/* recompute the logical->output transform: map mode, window
		 * and viewport parameters and the world transform may all
		 * have changed since the previous record */
		applyTransform();

		switch (type)
		{
		case EMR_EOF:
			return true;

		case EMR_SETWINDOWEXTEX:
			rec.point(m_dc.wnCX, m_dc.wnCY, base + 8);
			break;
		case EMR_SETWINDOWORGEX:
			rec.point(m_dc.wnX, m_dc.wnY, base + 8);
			break;
		case EMR_SETVIEWPORTEXTEX:
			rec.point(m_dc.vpCX, m_dc.vpCY, base + 8);
			break;
		case EMR_SETVIEWPORTORGEX:
			rec.point(m_dc.vpX, m_dc.vpY, base + 8);
			break;
		case EMR_SCALEVIEWPORTEXTEX:
		{
			uint32_t xn = 0, xd = 0, yn = 0, yd = 0;
			if (rec.u32(xn, base + 8) && rec.u32(xd, base + 12) &&
				rec.u32(yn, base + 16) && rec.u32(yd, base + 20) &&
				xd && yd)
			{
				m_dc.vpCX = m_dc.vpCX * xn / xd;
				m_dc.vpCY = m_dc.vpCY * yn / yd;
			}
			break;
		}
		case EMR_SCALEWINDOWEXTEX:
		{
			uint32_t xn = 0, xd = 0, yn = 0, yd = 0;
			if (rec.u32(xn, base + 8) && rec.u32(xd, base + 12) &&
				rec.u32(yn, base + 16) && rec.u32(yd, base + 20) &&
				xd && yd)
			{
				m_dc.wnCX = m_dc.wnCX * xn / xd;
				m_dc.wnCY = m_dc.wnCY * yn / yd;
			}
			break;
		}
		case EMR_SETMAPMODE:
		{
			uint32_t mm = 0;
			if (rec.u32(mm, base + 8))
				m_dc.mapMode = static_cast<int>(mm);
			break;
		}
		case EMR_SETBKMODE:
		{
			uint32_t v = 0;
			if (rec.u32(v, base + 8))
				m_dc.bkMode = static_cast<int>(v);
			break;
		}
		case EMR_SETPOLYFILLMODE:
		{
			uint32_t v = 0;
			if (rec.u32(v, base + 8))
				m_dc.polyFill = static_cast<int>(v);
			break;
		}
		case EMR_SETTEXTALIGN:
			rec.u32(m_dc.textAlign, base + 8);
			break;
		case EMR_SETTEXTCOLOR:
			rec.u32(m_dc.textColor, base + 8);
			break;
		case EMR_SETBKCOLOR:
			rec.u32(m_dc.bkColor, base + 8);
			break;
		case EMR_SETARCDIRECTION:
		{
			uint32_t v = 0;
			if (rec.u32(v, base + 8))
				m_dc.arcDir = static_cast<int>(v);
			break;
		}
		case EMR_SETMITERLIMIT:
		{
			float v = 0;
			if (rec.f32(v, base + 8))
				m_dc.miterLimit = v;
			break;
		}
		case EMR_MOVETOEX:
			rec.point(m_dc.curX, m_dc.curY, base + 8);
			if (m_inPath)
				cairo_move_to(m_cr, m_dc.curX, m_dc.curY);
			break;
		case EMR_LINETO:
		{
			double x, y;
			if (rec.point(x, y, base + 8))
				lineTo(x, y);
			break;
		}
		case EMR_SETPIXELV:
			setPixelV(rec, base);
			break;
		case EMR_RECTANGLE:
		case EMR_ELLIPSE:
		case EMR_ROUNDRECT:
		{
			double l, t, r, b;
			if (!rec.rect(l, t, r, b, base + 8))
				break;
			double rw = 0, rh = 0;
			if (type == EMR_ROUNDRECT)
			{
				uint32_t w = 0, h = 0;
				rec.u32(w, base + 24);
				rec.u32(h, base + 28);
				rw = w;
				rh = h;
			}
			cairo_new_path(m_cr);
			shapePath(l, t, r, b, type, rw, rh);
			if (!m_inPath)
				paintPath(true, true);
			break;
		}
		case EMR_ARC:
		case EMR_CHORD:
		case EMR_PIE:
		case EMR_ARCTO:
			arcLike(rec, base, type);
			break;
		case EMR_ANGLEARC:
			angleArc(rec, base);
			break;
		case EMR_POLYGON:
		case EMR_POLYLINE:
		case EMR_POLYBEZIER:
		case EMR_POLYBEZIERTO:
		case EMR_POLYLINETO:
			polyHelper(rec, base, false, type);
			break;
		case EMR_POLYGON16:
		case EMR_POLYLINE16:
		case EMR_POLYBEZIER16:
		case EMR_POLYBEZIERTO16:
		case EMR_POLYLINETO16:
			polyHelper(rec, base, true,
					   type == EMR_POLYGON16 ? EMR_POLYGON :
					   type == EMR_POLYLINE16 ? EMR_POLYLINE :
					   type == EMR_POLYBEZIER16 ? EMR_POLYBEZIER :
					   type == EMR_POLYBEZIERTO16 ? EMR_POLYBEZIERTO :
					   EMR_POLYLINETO);
			break;
		case EMR_POLYPOLYGON:
		case EMR_POLYPOLYLINE:
			polyPolyHelper(rec, base, false, type == EMR_POLYPOLYGON);
			break;
		case EMR_POLYPOLYGON16:
		case EMR_POLYPOLYLINE16:
			polyPolyHelper(rec, base, true, type == EMR_POLYPOLYGON16);
			break;
		case EMR_POLYDRAW:
		case EMR_POLYDRAW16:
			polyDraw(rec, base, type == EMR_POLYDRAW16);
			break;
		case EMR_BEGINPATH:
			m_inPath = true;
			cairo_new_path(m_cr);
			break;
		case EMR_ENDPATH:
			m_inPath = false;
			break;
		case EMR_CLOSEFIGURE:
			cairo_close_path(m_cr);
			break;
		case EMR_ABORTPATH:
			m_inPath = false;
			cairo_new_path(m_cr);
			break;
		case EMR_FILLPATH:
		case EMR_STROKEPATH:
		case EMR_STROKEANDFILLPATH:
		{
			cairo_set_fill_rule(m_cr,
								m_dc.polyFill == 2 ? CAIRO_FILL_RULE_WINDING
												   : CAIRO_FILL_RULE_EVEN_ODD);
			bool fill = type != EMR_STROKEPATH;
			bool stroke = type != EMR_FILLPATH;
			paintPath(fill, stroke);
			cairo_set_fill_rule(m_cr, CAIRO_FILL_RULE_WINDING);
			break;
		}
		case EMR_SELECTCLIPPATH:
		{
			uint32_t mode = 1;
			rec.u32(mode, base + 8);
			cairo_set_fill_rule(m_cr,
								m_dc.polyFill == 2 ? CAIRO_FILL_RULE_WINDING
												   : CAIRO_FILL_RULE_EVEN_ODD);
			cairo_clip(m_cr);
			cairo_set_fill_rule(m_cr, CAIRO_FILL_RULE_WINDING);
			m_inPath = false;
			break;
		}
		case EMR_INTERSECTCLIPRECT:
		{
			double l, t, r, b;
			if (rec.rect(l, t, r, b, base + 8))
				clipRect(l, t, r, b, false);
			break;
		}
		case EMR_EXCLUDECLIPRECT:
		{
			double l, t, r, b;
			if (rec.rect(l, t, r, b, base + 8))
				clipRect(l, t, r, b, true);
			break;
		}
		case EMR_EXTSELECTCLIPRGN:
		{
			/* cbRgnData(4) RegionMode(4) then the RGNDATA */
			uint32_t mode = 0, cb = 0;
			if (rec.u32(cb, base + 8) && rec.u32(mode, base + 12) && cb)
			{
				const uint8_t * rgn = rec.at(16, cb);
				if (rgn)
					regionClip(rgn, cb);
			}
			break;
		}
		case EMR_FILLRGN:
		case EMR_PAINTRGN:
		{
			uint32_t cb = 0, ih = 0;
			if (rec.u32(cb, base + 24) &&
				rec.u32(ih, base + 28) && cb)
			{
				const uint8_t * rgn = rec.at(32, cb);
				if (rgn)
				{
					if (type == EMR_FILLRGN)
					{
						const Obj * bo = obj(ih);
						if (bo && bo->kind == K_BRUSH)
							fillRegionRects(rgn, cb, bo->brush);
					}
					else
						fillRegionRects(rgn, cb, resolveBrush(m_dc));
				}
			}
			break;
		}
		case EMR_SAVEDC:
			savedc();
			break;
		case EMR_RESTOREDC:
		{
			int32_t rel = 0;
			if (rec.i32(rel, base + 8))
				restoredc(rel);
			break;
		}
		case EMR_SETWORLDTRANSFORM:
			applyWorld(rec, base, 4);
			break;
		case EMR_MODIFYWORLDTRANSFORM:
		{
			uint32_t mode = 0;
			if (rec.u32(mode, base + 32))
				applyWorld(rec, base, static_cast<int>(mode));
			break;
		}
		case EMR_CREATEPEN:
		{
			uint32_t ih = 0, style = 0, w = 0, color = 0;
			if (rec.u32(ih, base + 8) && rec.u32(style, base + 12) &&
				rec.u32(w, base + 16) && rec.u32(color, base + 24))
			{
				Obj & o = m_objects[ih];
				o = Obj();
				o.kind = K_PEN;
				o.pen.style = style & PS_STYLE_MASK;
				o.pen.geometric = false;
				o.pen.width = w;
				o.pen.color = color;
				/* cosmetic-pen end caps are round by definition */
				o.pen.cap = CAIRO_LINE_CAP_ROUND;
			}
			break;
		}
		case EMR_EXTCREATEPEN:
			extCreatePen(rec, base);
			break;
		case EMR_CREATEBRUSHINDIRECT:
		{
			uint32_t ih = 0, style = 0, color = 0, hatch = 0;
			if (rec.u32(ih, base + 8) && rec.u32(style, base + 12) &&
				rec.u32(color, base + 16) && rec.u32(hatch, base + 20))
			{
				Obj & o = m_objects[ih];
				o = Obj();
				o.kind = K_BRUSH;
				o.brush.style = static_cast<int>(style);
				o.brush.color = color;
				o.brush.hatch = static_cast<int>(hatch);
			}
			break;
		}
		case EMR_EXTCREATEFONTINDIRECTW:
			createFont(rec, base);
			break;
		case EMR_CREATEMONOBRUSH:
		case EMR_CREATEDIBPATTERNBRUSHPT:
		case EMR_CREATEPALETTE:
		{
			uint32_t ih = 0;
			if (rec.u32(ih, base + 8))
			{
				Obj & o = m_objects[ih];
				o = Obj();
				o.kind = type == EMR_CREATEPALETTE ? K_PALETTE : K_BRUSH;
				if (o.kind == K_BRUSH)
				{
					o.brush.style = BS_SOLID;
					o.brush.color = 0x808080;
				}
			}
			break;
		}
		case EMR_SELECTOBJECT:
		{
			uint32_t ih = 0;
			if (!rec.u32(ih, base + 8))
				break;
			if (ih & 0x80000000u)
			{
				uint32_t idx = ih & 0x7FFFFFFFu;
				switch (idx)
				{
				case STOCK_WHITE_BRUSH: case STOCK_LTGRAY_BRUSH:
				case STOCK_GRAY_BRUSH: case STOCK_DKGRAY_BRUSH:
				case STOCK_BLACK_BRUSH: case STOCK_NULL_BRUSH:
				case STOCK_DC_BRUSH:
					m_dc.brush = ih;
					break;
				case STOCK_WHITE_PEN: case STOCK_BLACK_PEN:
				case STOCK_NULL_PEN: case STOCK_DC_PEN:
					m_dc.pen = ih;
					break;
				case STOCK_DEFAULT_PALETTE:
					break;
				default:
					m_dc.font = ih;
					break;
				}
			}
			else
			{
				const Obj * o = obj(ih);
				if (o)
				{
					switch (o->kind)
					{
					case K_PEN: m_dc.pen = ih; break;
					case K_BRUSH: m_dc.brush = ih; break;
					case K_FONT: m_dc.font = ih; break;
					case K_REGION:
					{
						/* selecting a region installs it as the clip */
						const Obj & ro = *o;
						if (!ro.region.empty())
							regionClip(ro.region.data(),
									   ro.region.size());
						break;
					}
					default: break;
					}
				}
			}
			break;
		}
		case EMR_DELETEOBJECT:
		{
			uint32_t ih = 0;
			if (rec.u32(ih, base + 8))
				m_objects.erase(ih);
			break;
		}
		case EMR_BITBLT:
		case EMR_STRETCHBLT:
		case EMR_MASKBLT:
		case EMR_SETDIBITSTODEVICE:
		case EMR_STRETCHDIBITS:
		case EMR_ALPHABLEND:
			dibBlt(rec, base, type);
			break;
		case EMR_EXTTEXTOUTA:
		case EMR_EXTTEXTOUTW:
			/* EMRTEXT follows type,size(8) rclBounds(16)
			 * iGraphicsMode(4) exScale(4) eyScale(4) */
			textOut(rec, type == EMR_EXTTEXTOUTW, base + 36, 1);
			break;
		case EMR_POLYTEXTOUTA:
		case EMR_POLYTEXTOUTW:
		{
			uint32_t nStr = 0;
			if (rec.u32(nStr, base + 36))
				textOut(rec, type == EMR_POLYTEXTOUTW,
						base + 40, nStr > 256 ? 256 : nStr);
			break;
		}
		case EMR_GRADIENTFILL:
			gradientFill(rec, base);
			break;
		default:
			break; /* palettes, escapes, comments, GLS, ICM... */
		}

		pos += size;
	}
	return true;
}

/* PNG write -> UT_ByteBuf */
static cairo_status_t pngSink(void * ctx, const unsigned char * data,
							  unsigned int length)
{
	UT_ByteBuf * bb = static_cast<UT_ByteBuf *>(ctx);
	return bb->append(reinterpret_cast<const UT_Byte *>(data), length)
		? CAIRO_STATUS_SUCCESS
		: CAIRO_STATUS_WRITE_ERROR;
}

} // anonymous namespace

bool UT_renderEMF(const UT_ConstByteBufPtr & pEmf, UT_ByteBufPtr & pPng)
{
	pPng.reset();
	if (!pEmf || pEmf->getLength() < 88)
		return false;

	Buf f{pEmf->getPointer(0), pEmf->getLength()};

	uint32_t type = 0, size = 0, sig = 0;
	if (!f.u32(type, 0) || type != EMR_HEADER ||
		!f.u32(size, 4) || size < 88 || !f.u32(sig, 40) ||
		sig != 0x464D4520u) /* ' EMF' */
		return false;

	double bl = 0, bt = 0, br = 0, bb = 0;
	double fl = 0, ft = 0, fr = 0, fb = 0;
	if (!f.rect(bl, bt, br, bb, 8) || !f.rect(fl, ft, fr, fb, 24))
		return false;
	uint32_t nBytes = 0;
	f.u32(nBytes, 48);
	size_t fileLen = nBytes && nBytes <= f.n ? nBytes : f.n;

	double bw = br - bl, bh = bb - bt;
	if (bw <= 0 || bh <= 0)
		return false;
	double fw = fr - fl, fh = fb - ft;

	/* output pixel size: prefer the physical size from rclFrame at
	 * ~96dpi (matches the screen-res convention the raster path uses),
	 * falling back to raw device bounds when the frame is bogus */
	double outW, outH;
	if (fw > 0 && fh > 0)
	{
		outW = fw * 0.01 / 25.4 * 96.0;
		outH = fh * 0.01 / 25.4 * 96.0;
	}
	else
	{
		outW = bw;
		outH = bh;
	}
	/* clamp to a sane range */
	while (outW > 4096 || outH > 4096)
	{
		outW /= 2;
		outH /= 2;
	}
	if (outW < 1 || outH < 1)
		return false;

	cairo_surface_t * surf =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
								   static_cast<int>(std::lround(outW)),
								   static_cast<int>(std::lround(outH)));
	if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS)
	{
		cairo_surface_destroy(surf);
		return false;
	}

	cairo_t * cr = cairo_create(surf);
	Player player(cr, f);
	player.m_playScaleX = outW / bw;
	player.m_playScaleY = outH / bh;
	player.m_boundsL = bl;
	player.m_boundsT = bt;
	if (fw > 0 && fh > 0)
	{
		player.m_devDpiX = bw / (fw * 0.01 / 25.4);
		player.m_devDpiY = bh / (fh * 0.01 / 25.4);
	}
	player.applyTransform();

	bool ok = player.run(size, fileLen);
	if (ok && cairo_status(cr) != CAIRO_STATUS_SUCCESS)
		ok = false;
	cairo_destroy(cr);

	if (ok)
	{
		UT_ByteBufPtr png(new UT_ByteBuf);
		if (png &&
			cairo_surface_write_to_png_stream(surf, pngSink, png.get()) ==
				CAIRO_STATUS_SUCCESS)
			pPng = std::move(png);
		else
			ok = false;
	}
	cairo_surface_destroy(surf);
	return ok && pPng && pPng->getLength() > 0;
}
