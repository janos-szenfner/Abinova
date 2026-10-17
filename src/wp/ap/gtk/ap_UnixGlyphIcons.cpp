/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Program Utilities
 * Copyright (C) 2025-2026 Abinova contributors
 *
 * Icon shapes adapted from wordcraft's crates/ui-egui/src/icons.rs
 * (https://github.com/storytold/wordcraft, MIT OR Apache-2.0) —
 * original artwork drawn in code on a 20x20 grid, two colours.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ap_UnixGlyphIcons.h"
#include "ut_assert.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ---------------- pen vocabulary (20x20 grid) ---------------- */

struct _GlyphPen
{
	cairo_t *cr;
	double	  s;		/* min(w,h)/20 */
	double	  ox, oy;	/* top-left of the centered grid */
	GdkRGBA	  fg;		/* line colour - theme text colour */
	GdkRGBA	  ac;		/* accent colour - theme accent or blue */
	double	  lw;		/* stroke width in grid units */
};

static void _pen_pt(const _GlyphPen & p, double x, double y,
					double *px, double *py)
{
	*px = p.ox + x * p.s;
	*py = p.oy + y * p.s;
}

static void _pen_color(cairo_t *cr, const GdkRGBA &c)
{
	cairo_set_source_rgba(cr, c.red, c.green, c.blue, c.alpha);
}

/* open polyline, stroked */
static void _pen_line_c(const _GlyphPen & p, const double *pts, int n,
						const GdkRGBA &c)
{
	double x, y;
	_pen_pt(p, pts[0], pts[1], &x, &y);
	cairo_move_to(p.cr, x, y);
	for (int i = 1; i < n; i++)
	{
		_pen_pt(p, pts[i * 2], pts[i * 2 + 1], &x, &y);
		cairo_line_to(p.cr, x, y);
	}
	_pen_color(p.cr, c);
	cairo_set_line_width(p.cr, p.lw * p.s);
	cairo_set_line_cap(p.cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(p.cr, CAIRO_LINE_JOIN_ROUND);
	cairo_stroke(p.cr);
}

#define _pen_line(p, pts) \
	_pen_line_c((p), (pts), G_N_ELEMENTS(pts) / 2, (p)->fg)

/* closed outline, stroked */
static void _pen_closed(const _GlyphPen & p, const double *pts, int n,
						const GdkRGBA &c)
{
	double x, y;
	_pen_pt(p, pts[0], pts[1], &x, &y);
	cairo_move_to(p.cr, x, y);
	for (int i = 1; i < n; i++)
	{
		_pen_pt(p, pts[i * 2], pts[i * 2 + 1], &x, &y);
		cairo_line_to(p.cr, x, y);
	}
	cairo_close_path(p.cr);
	_pen_color(p.cr, c);
	cairo_set_line_width(p.cr, p.lw * p.s);
	cairo_set_line_join(p.cr, CAIRO_LINE_JOIN_ROUND);
	cairo_stroke(p.cr);
}

/* convex polygon, filled */
static void _pen_fill(const _GlyphPen & p, const double *pts, int n,
					  const GdkRGBA &c)
{
	double x, y;
	_pen_pt(p, pts[0], pts[1], &x, &y);
	cairo_move_to(p.cr, x, y);
	for (int i = 1; i < n; i++)
	{
		_pen_pt(p, pts[i * 2], pts[i * 2 + 1], &x, &y);
		cairo_line_to(p.cr, x, y);
	}
	cairo_close_path(p.cr);
	_pen_color(p.cr, c);
	cairo_fill(p.cr);
}

static void _pen_rect(const _GlyphPen & p, double x0, double y0,
					  double x1, double y1, const GdkRGBA &c)
{
	double xa, ya, xb, yb;
	_pen_pt(p, x0, y0, &xa, &ya);
	_pen_pt(p, x1, y1, &xb, &yb);
	cairo_rectangle(p.cr, xa, ya, xb - xa, yb - ya);
	_pen_color(p.cr, c);
	cairo_set_line_width(p.cr, p.lw * p.s);
	cairo_set_line_join(p.cr, CAIRO_LINE_JOIN_ROUND);
	cairo_stroke(p.cr);
}

static void _pen_frect(const _GlyphPen & p, double x0, double y0,
					   double x1, double y1, const GdkRGBA &c)
{
	double xa, ya, xb, yb;
	_pen_pt(p, x0, y0, &xa, &ya);
	_pen_pt(p, x1, y1, &xb, &yb);
	cairo_rectangle(p.cr, xa, ya, xb - xa, yb - ya);
	_pen_color(p.cr, c);
	cairo_fill(p.cr);
}

static void _pen_circle(const _GlyphPen & p, double x, double y, double r,
						const GdkRGBA &c)
{
	double px, py;
	_pen_pt(p, x, y, &px, &py);
	cairo_arc(p.cr, px, py, r * p.s, 0.0, 2.0 * G_PI);
	_pen_color(p.cr, c);
	cairo_set_line_width(p.cr, p.lw * p.s);
	cairo_stroke(p.cr);
}

static void _pen_fcircle(const _GlyphPen & p, double x, double y, double r,
						 const GdkRGBA &c)
{
	double px, py;
	_pen_pt(p, x, y, &px, &py);
	cairo_arc(p.cr, px, py, r * p.s, 0.0, 2.0 * G_PI);
	_pen_color(p.cr, c);
	cairo_fill(p.cr);
}

/* partial arc, stroked */
static void _pen_arc(const _GlyphPen & p, double x, double y, double r,
					 double a0, double a1, const GdkRGBA &c)
{
	double px, py;
	_pen_pt(p, x, y, &px, &py);
	cairo_arc(p.cr, px, py, r * p.s, a0, a1);
	_pen_color(p.cr, c);
	cairo_set_line_width(p.cr, p.lw * p.s);
	cairo_set_line_cap(p.cr, CAIRO_LINE_CAP_ROUND);
	cairo_stroke(p.cr);
}

/* centered text (the toy font API is fine for a glyph or two) */
static void _pen_text(const _GlyphPen & p, double x, double y, double size,
					  const char *text, const GdkRGBA &c, gboolean bold)
{
	cairo_select_font_face(p.cr, "sans",
						   CAIRO_FONT_SLANT_NORMAL,
						   bold ? CAIRO_FONT_WEIGHT_BOLD
								: CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(p.cr, size * p.s);
	cairo_text_extents_t ext;
	cairo_text_extents(p.cr, text, &ext);
	double px, py;
	_pen_pt(p, x, y, &px, &py);
	cairo_move_to(p.cr,
				  px - ext.x_bearing - ext.width / 2.0,
				  py - ext.y_bearing - ext.height / 2.0);
	_pen_color(p.cr, c);
	cairo_show_text(p.cr, text);
}

static void _pen_text_i(const _GlyphPen & p, double x, double y, double size,
						const char *text, const GdkRGBA &c)
{
	cairo_select_font_face(p.cr, "sans",
						   CAIRO_FONT_SLANT_ITALIC,
						   CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(p.cr, size * p.s);
	cairo_text_extents_t ext;
	cairo_text_extents(p.cr, text, &ext);
	double px, py;
	_pen_pt(p, x, y, &px, &py);
	cairo_move_to(p.cr,
				  px - ext.x_bearing - ext.width / 2.0,
				  py - ext.y_bearing - ext.height / 2.0);
	_pen_color(p.cr, c);
	cairo_show_text(p.cr, text);
}

/* horizontal paragraph lines */
static void _pen_lines(const _GlyphPen & p, double x0, double x1,
					   const double *ys, int n)
{
	for (int i = 0; i < n; i++)
	{
		const double l[] = { x0, ys[i], x1, ys[i] };
		_pen_line_c(p, l, 2, p.fg);
	}
}

/* page outline with folded corner */
static void _pen_page(const _GlyphPen & p, double x0, double y0,
					  double x1, double y1)
{
	const double o[] = { x0, y0, x1 - 4.0, y0, x1, y0 + 4.0, x1, y1, x0, y1 };
	_pen_closed(p, o, 5, p.fg);
	const double f[] = { x1 - 4.0, y0, x1 - 4.0, y0 + 4.0, x1, y0 + 4.0 };
	_pen_line_c(p, f, 3, p.fg);
}

/* fixed semantic colours (same picks as wordcraft) */
static const GdkRGBA _YELLOW = { 0xF2 / 255.0, 0xC8 / 255.0, 0x11 / 255.0, 1.0 };
static const GdkRGBA _GREEN  = { 0x2E / 255.0, 0x8B / 255.0, 0x57 / 255.0, 1.0 };
static const GdkRGBA _RED    = { 0xD1 / 255.0, 0x3B / 255.0, 0x3B / 255.0, 1.0 };
static const GdkRGBA _ORANGE = { 0xE0 / 255.0, 0x7B / 255.0, 0x1F / 255.0, 1.0 };

/* ---------------- icon shapes ---------------- */

static void _g_undo(const _GlyphPen & p)
{
	const double a[] = { 5.0, 8.0, 13.0, 8.0 };
	_pen_line_c(p, a, 2, p.fg);
	_pen_arc(p, 13.0, 12.5, 4.5, -G_PI / 2.0, G_PI / 2.0, p.fg);
	const double b[] = { 13.0, 17.0, 8.0, 17.0 };
	_pen_line_c(p, b, 2, p.fg);
	const double h[] = { 2.5, 8.0, 6.5, 4.5, 6.5, 11.5 };
	_pen_fill(p, h, 3, p.ac);
}

static void _g_redo(const _GlyphPen & p)
{
	const double a[] = { 15.0, 8.0, 7.0, 8.0 };
	_pen_line_c(p, a, 2, p.fg);
	_pen_arc(p, 7.0, 12.5, 4.5, G_PI / 2.0, 3.0 * G_PI / 2.0, p.fg);
	const double b[] = { 7.0, 17.0, 12.0, 17.0 };
	_pen_line_c(p, b, 2, p.fg);
	const double h[] = { 17.5, 8.0, 13.5, 4.5, 13.5, 11.5 };
	_pen_fill(p, h, 3, p.ac);
}

/* show/hide formatting marks — the pilcrow, like wordcraft's "pilcrow" */
static void _g_showpara(const _GlyphPen & p)
{
	_pen_text(p, 10.0, 10.5, 15.0, "\xc2\xb6", p.fg, FALSE);
}

static void _g_find(const _GlyphPen & p)
{
	_pen_circle(p, 8.5, 8.5, 5.0, p.fg);
	const double h[] = { 12.0, 12.0, 16.5, 16.5 };
	_pen_line_c(p, h, 2, p.ac);
}

static void _g_replace(const _GlyphPen & p)
{
	_pen_text(p, 6.0, 6.0, 8.0, "ab", p.fg, FALSE);
	_pen_text(p, 13.0, 15.0, 8.0, "ac", p.ac, FALSE);
	const double a[] = { 13.0, 3.5, 16.0, 3.5, 16.0, 8.5 };
	_pen_line_c(p, a, 3, p.fg);
	const double h[] = { 14.5, 7.0, 16.0, 8.5, 17.5, 7.0 };
	_pen_line_c(p, h, 3, p.fg);
}

static void _g_select_all(const _GlyphPen & p)
{
	/* selection frame + a cursor arrow picking it */
	_pen_rect(p, 2.5, 2.5, 17.5, 17.5, p.ac);
	const double a[] = { 5.0, 3.0, 5.0, 16.0, 14.0, 12.0 };
	_pen_fill(p, a, 3, p.fg);
	const double b[] = { 8.0, 13.0, 10.5, 18.0, 12.5, 17.0, 10.0, 12.0 };
	_pen_fill(p, b, 4, p.fg);
}

static void _g_goto(const _GlyphPen & p)
{
	/* text lines with an arrow landing on the middle one */
	const double ys[] = { 4.0, 8.0, 12.0, 16.0 };
	_pen_lines(p, 3.0, 11.0, ys, 4);
	const double a[] = { 12.5, 10.0, 18.0, 10.0 };
	_pen_line_c(p, a, 2, p.ac);
	const double h[] = { 15.5, 7.5, 18.0, 10.0, 15.5, 12.5 };
	_pen_line_c(p, h, 3, p.ac);
}

static void _g_save(const _GlyphPen & p)
{
	const double o[] = { 3.0, 3.0, 14.0, 3.0, 17.0, 6.0, 17.0, 17.0, 3.0, 17.0 };
	_pen_closed(p, o, 5, p.fg);
	_pen_frect(p, 6.0, 3.0, 13.0, 7.5, p.ac);
	_pen_rect(p, 6.0, 11.0, 14.0, 17.0, p.fg);
}

static void _g_open(const _GlyphPen & p)
{
	const double top[] = { 2.5, 5.0, 7.5, 5.0, 9.0, 6.5, 15.5, 6.5, 15.5, 9.0 };
	_pen_closed(p, top, 5, p.fg);
	const double l[] = { 2.5, 5.0, 2.5, 16.0 };
	_pen_line_c(p, l, 2, p.fg);
	const double flap[] = { 2.5, 16.0, 5.0, 9.0, 18.0, 9.0, 15.5, 16.0 };
	_pen_closed(p, flap, 4, p.ac);
}

static void _g_new(const _GlyphPen & p)
{
	_pen_page(p, 4.0, 2.0, 16.0, 18.0);
	const double v[] = { 10.0, 7.0, 10.0, 13.0 };
	_pen_line_c(p, v, 2, p.ac);
	const double h[] = { 7.0, 10.0, 13.0, 10.0 };
	_pen_line_c(p, h, 2, p.ac);
}

static void _g_print(const _GlyphPen & p)
{
	_pen_rect(p, 6.0, 2.5, 14.0, 7.0, p.fg);
	_pen_rect(p, 2.5, 7.0, 17.5, 14.0, p.fg);
	const double sheet[] = { 6.0, 11.5, 14.0, 11.5, 14.0, 17.5, 6.0, 17.5 };
	_pen_closed(p, sheet, 4, p.ac);
	_pen_fcircle(p, 15.0, 9.5, 0.8, p.fg);
}

static void _g_cut(const _GlyphPen & p)
{
	_pen_circle(p, 6.0, 15.0, 2.5, p.fg);
	_pen_circle(p, 14.0, 15.0, 2.5, p.fg);
	const double a[] = { 7.5, 13.0, 14.0, 3.0 };
	_pen_line_c(p, a, 2, p.fg);
	const double b[] = { 12.5, 13.0, 6.0, 3.0 };
	_pen_line_c(p, b, 2, p.fg);
}

static void _g_copy(const _GlyphPen & p)
{
	_pen_rect(p, 3.5, 3.0, 12.5, 14.0, p.fg);
	_pen_rect(p, 7.5, 6.5, 16.5, 17.5, p.ac);
}

static void _g_paste(const _GlyphPen & p)
{
	_pen_rect(p, 4.0, 3.5, 14.0, 17.0, p.fg);
	_pen_frect(p, 7.0, 2.0, 11.0, 5.0, p.ac);
	GdkRGBA tint = p.ac;
	tint.alpha = 0.18;
	_pen_frect(p, 9.0, 8.0, 17.5, 18.5, tint);
	_pen_rect(p, 9.0, 8.0, 17.5, 18.5, p.ac);
	const double ys[] = { 11.0, 13.5, 16.0 };
	for (int i = 0; i < 3; i++)
	{
		const double l[] = { 10.8, ys[i], 15.8, ys[i] };
		_pen_line_c(p, l, 2, p.fg);
	}
}

static void _g_spelling(const _GlyphPen & p)
{
	_pen_text(p, 8.0, 7.0, 9.0, "abc", p.fg, TRUE);
	const double chk[] = { 5.0, 13.0, 8.5, 16.5, 16.0, 9.0 };
	_pen_line_c(p, chk, 3, _GREEN);
}

static void _g_bold(const _GlyphPen & p)
{
	_pen_text(p, 10.0, 10.5, 15.0, "B", p.fg, TRUE);
}

static void _g_italic(const _GlyphPen & p)
{
	_pen_text_i(p, 10.0, 10.5, 15.0, "I", p.fg);
}

static void _g_underline(const _GlyphPen & p)
{
	_pen_text(p, 10.0, 9.0, 14.0, "U", p.fg, FALSE);
	const double u[] = { 5.0, 17.0, 15.0, 17.0 };
	_pen_line_c(p, u, 2, p.fg);
}

static void _g_strike(const _GlyphPen & p)
{
	_pen_text(p, 10.0, 10.0, 12.0, "ab", p.fg, FALSE);
	const double s[] = { 3.0, 10.5, 17.0, 10.5 };
	_pen_line_c(p, s, 2, p.fg);
}

static void _g_align(const _GlyphPen & p, int which)
{
	const double ys0[] = { 4.0, 10.0, 16.0 };
	_pen_lines(p, 3.0, 17.0, ys0, 3);
	const double ys1[] = { 7.0, 13.0 };
	double x1 = which == 1 ? 14.0 : which == 2 ? 17.0 : 12.0;
	double x0 = which == 2 ? 8.0 : which == 1 ? 6.0 : 3.0;
	for (int i = 0; i < 2; i++)
	{
		const double l[] = { x0, ys1[i], x1, ys1[i] };
		_pen_line_c(p, l, 2, p.fg);
	}
}

static void _g_justify(const _GlyphPen & p)
{
	const double ys[] = { 4.0, 7.0, 10.0, 13.0, 16.0 };
	_pen_lines(p, 3.0, 17.0, ys, 5);
}

static void _g_indent(const _GlyphPen & p, gboolean more)
{
	const double ys[] = { 7.0, 10.0, 13.0 };
	for (int i = 0; i < 3; i++)
	{
		const double l[] = { 9.0, ys[i], 17.0, ys[i] };
		_pen_line_c(p, l, 2, p.fg);
	}
	const double top[] = { 3.0, 3.5, 17.0, 3.5 };
	const double bot[] = { 3.0, 16.5, 17.0, 16.5 };
	_pen_line_c(p, top, 2, p.fg);
	_pen_line_c(p, bot, 2, p.fg);
	if (more)
	{
		const double a[] = { 3.0, 7.0, 3.0, 13.0, 6.5, 10.0 };
		_pen_fill(p, a, 3, p.ac);
	}
	else
	{
		const double a[] = { 6.5, 7.0, 6.5, 13.0, 3.0, 10.0 };
		_pen_fill(p, a, 3, p.ac);
	}
}

static void _g_zoom(const _GlyphPen & p, gboolean in)
{
	_pen_circle(p, 8.5, 8.5, 5.5, p.fg);
	const double h[] = { 12.5, 12.5, 17.5, 17.5 };
	_pen_line_c(p, h, 2, p.ac);
	const double minus[] = { 6.0, 8.5, 11.0, 8.5 };
	_pen_line_c(p, minus, 2, p.fg);
	if (in)
	{
		const double plus[] = { 8.5, 6.0, 8.5, 11.0 };
		_pen_line_c(p, plus, 2, p.fg);
	}
}

static void _g_picture(const _GlyphPen & p)
{
	_pen_rect(p, 2.5, 4.0, 17.5, 16.0, p.fg);
	const double m1[] = { 4.0, 14.5, 8.5, 8.0, 12.0, 14.5 };
	_pen_fill(p, m1, 3, p.ac);
	const double m2[] = { 9.5, 14.5, 13.0, 10.0, 16.0, 14.5 };
	_pen_fill(p, m2, 3, _GREEN);
	_pen_fcircle(p, 13.5, 7.0, 1.5, _YELLOW);
}

static void _g_table(const _GlyphPen & p)
{
	_pen_frect(p, 3.0, 3.0, 17.0, 7.0, p.ac);
	_pen_rect(p, 3.0, 3.0, 17.0, 17.0, p.fg);
	const double r1[] = { 3.0, 7.0, 17.0, 7.0 };
	const double r2[] = { 3.0, 12.0, 17.0, 12.0 };
	const double c1[] = { 8.0, 3.0, 8.0, 17.0 };
	const double c2[] = { 12.5, 3.0, 12.5, 17.0 };
	_pen_line_c(p, r1, 2, p.fg);
	_pen_line_c(p, r2, 2, p.fg);
	_pen_line_c(p, c1, 2, p.fg);
	_pen_line_c(p, c2, 2, p.fg);
}

/* ---- Design tab icons (wordcraft's design.* set) ---- */

/* themes: four coloured squares */
static void _g_design_themes(const _GlyphPen & p)
{
	_pen_frect(p, 3.0, 3.0, 10.0, 10.0, p.ac);
	_pen_frect(p, 10.0, 3.0, 17.0, 10.0, _ORANGE);
	_pen_frect(p, 3.0, 10.0, 10.0, 17.0, _GREEN);
	_pen_frect(p, 10.0, 10.0, 17.0, 17.0, _YELLOW);
}

/* theme colours: three filled circles */
static void _g_design_themecolors(const _GlyphPen & p)
{
	_pen_fcircle(p, 7.0, 7.5, 3.5, _RED);
	_pen_fcircle(p, 13.0, 7.5, 3.5, p.ac);
	_pen_fcircle(p, 10.0, 13.0, 3.5, _GREEN);
}

/* theme fonts: "Aa" in accent */
static void _g_design_themefonts(const _GlyphPen & p)
{
	_pen_text(p, 10.0, 10.0, 13.0, "Aa", p.ac, FALSE);
}

/* style sets: accent "A" with a set-mark stroke */
static void _g_design_stylesets(const _GlyphPen & p)
{
	_pen_text(p, 10.0, 10.0, 14.0, "A", p.ac, FALSE);
	const double chk[] = { 13.0, 15.0, 17.0, 11.0 };
	_pen_line_c(p, chk, 2, p.fg);
}

/* paragraph spacing: text lines with an up/down arrow on the left */
static void _g_design_paraspacing(const _GlyphPen & p)
{
	const double ys[] = { 5.0, 9.0, 13.0, 17.0 };
	_pen_lines(p, 6.5, 17.0, ys, 4);
	const double shaft[] = { 3.0, 5.5, 3.0, 14.5 };
	_pen_line_c(p, shaft, 2, p.ac);
	const double up[] = { 1.2, 6.5, 3.0, 4.5, 4.8, 6.5 };
	_pen_line_c(p, up, 3, p.ac);
	const double dn[] = { 1.2, 13.5, 3.0, 15.5, 4.8, 13.5 };
	_pen_line_c(p, dn, 3, p.ac);
}

/* watermark: page with a washed-out bold "W" */
static void _g_design_watermark(const _GlyphPen & p)
{
	_pen_page(p, 4.0, 2.0, 16.0, 18.0);
	GdkRGBA dim = p.ac;
	dim.alpha = 0.6;
	_pen_text(p, 10.0, 11.0, 7.0, "W", dim, TRUE);
}

/* page colour: page with a tinted fill block */
static void _g_design_pagecolor(const _GlyphPen & p)
{
	_pen_page(p, 4.0, 2.0, 16.0, 18.0);
	GdkRGBA tint = p.ac;
	tint.alpha = 0.5;
	_pen_frect(p, 5.0, 9.0, 15.0, 17.0, tint);
}

/* page borders: page with an accent inner frame */
static void _g_design_pageborders(const _GlyphPen & p)
{
	_pen_page(p, 4.0, 2.0, 16.0, 18.0);
	_pen_rect(p, 6.0, 6.5, 14.0, 15.5, p.ac);
}

/* set as default: a single green check stroke (wordcraft setDefault) */
static void _g_design_setdefault(const _GlyphPen & p)
{
	const double chk[] = { 4.0, 10.5, 8.5, 15.0, 16.5, 5.0 };
	_pen_line_c(p, chk, 3, _GREEN);
}

/* ---------------- name table ---------------- */

typedef void (*_GlyphDraw)(const _GlyphPen &);

struct _GlyphEntry
{
	const char *name;
	_GlyphDraw  draw;
};

static void _g_align_left(const _GlyphPen & p)   { _g_align(p, 0); }
static void _g_align_center(const _GlyphPen & p) { _g_align(p, 1); }
static void _g_align_right(const _GlyphPen & p)  { _g_align(p, 2); }
static void _g_indent_more(const _GlyphPen & p)  { _g_indent(p, TRUE); }
static void _g_indent_less(const _GlyphPen & p)  { _g_indent(p, FALSE); }
static void _g_zoom_in(const _GlyphPen & p)      { _g_zoom(p, TRUE); }
static void _g_zoom_out(const _GlyphPen & p)     { _g_zoom(p, FALSE); }

static const _GlyphEntry _glyphs[] =
{
	{ "edit-undo",					_g_undo },
	{ "edit-redo",					_g_redo },
	{ "tb_view_showpara",			_g_showpara },
	{ "edit-find",					_g_find },
	{ "edit-find-replace",			_g_replace },
	{ "edit-select-all",			_g_select_all },
	{ "go-jump",					_g_goto },
	{ "document-save",				_g_save },
	{ "document-save-as",			_g_save },
	{ "document-open",				_g_open },
	{ "document-new",				_g_new },
	{ "document-print",				_g_print },
	{ "edit-cut",					_g_cut },
	{ "edit-copy",					_g_copy },
	{ "edit-paste",					_g_paste },
	{ "tools-check-spelling",		_g_spelling },
	{ "format-text-bold",			_g_bold },
	{ "format-text-italic",			_g_italic },
	{ "format-text-underline",		_g_underline },
	{ "format-text-strikethrough",	_g_strike },
	{ "format-justify-left",		_g_align_left },
	{ "format-justify-center",		_g_align_center },
	{ "format-justify-right",		_g_align_right },
	{ "format-justify-fill",		_g_justify },
	{ "format-indent-more",			_g_indent_more },
	{ "format-indent-less",			_g_indent_less },
	{ "zoom-in",					_g_zoom_in },
	{ "zoom-out",					_g_zoom_out },
	{ "insert-image",				_g_picture },
	{ "insert-table",				_g_table },
	{ "abinova-design-themes",		_g_design_themes },
	{ "abinova-design-themecolors",	_g_design_themecolors },
	{ "abinova-design-themefonts",	_g_design_themefonts },
	{ "abinova-design-stylesets",	_g_design_stylesets },
	{ "abinova-design-paraspacing",	_g_design_paraspacing },
	{ "abinova-design-watermark",	_g_design_watermark },
	{ "abinova-design-pagecolor",	_g_design_pagecolor },
	{ "abinova-design-pageborders",	_g_design_pageborders },
	{ "abinova-design-setdefault",	_g_design_setdefault },
	{ nullptr, nullptr }
};

/* strip a -symbolic suffix so the theme-agnostic name hits the table */
static _GlyphDraw
_glyph_draw_for(const gchar *icon_name)
{
	if (!icon_name || !*icon_name)
		return nullptr;
	char name[64];
	g_strlcpy(name, icon_name, sizeof(name));
	char *suf = strstr(name, "-symbolic");
	if (suf && suf[9] == '\0')
		*suf = '\0';
	for (const _GlyphEntry *e = _glyphs; e->name; e++)
		if (0 == strcmp(e->name, name))
			return e->draw;
	return nullptr;
}

gboolean
abi_glyph_icon_has(const gchar *icon_name)
{
	return _glyph_draw_for(icon_name) != nullptr;
}

gboolean
abi_glyph_icons_forced(void)
{
	static int forced = -1;
	if (forced < 0)
		forced = g_getenv("ABINOVA_GLYPH_ICONS") &&
				 *g_getenv("ABINOVA_GLYPH_ICONS") != '0' ? 1 : 0;
	return forced == 1;
}

static void
_pen_init(_GlyphPen & p, cairo_t *cr, double w, double h,
		  const GdkRGBA &fg, const GdkRGBA &ac)
{
	p.cr = cr;
	p.s = MIN(w, h) / 20.0;
	p.ox = (w - 20.0 * p.s) / 2.0;
	p.oy = (h - 20.0 * p.s) / 2.0;
	p.fg = fg;
	p.ac = ac;
	p.lw = 1.25;
}

void
abi_glyph_icon_paint(const gchar *icon_name, cairo_t *cr,
					 double w, double h,
					 const GdkRGBA &fg, const GdkRGBA &ac)
{
	_GlyphDraw draw = _glyph_draw_for(icon_name);
	UT_return_if_fail(draw);
	_GlyphPen p;
	_pen_init(p, cr, w, h, fg, ac);
	draw(p);
}

/* ---------------- widget ---------------- */

struct _GlyphAreaCtx
{
	char		 name[64];
	GtkWidget	*area;
};

static void
_glyph_area_draw(GtkDrawingArea *area, cairo_t *cr,
				 int w, int h, gpointer data)
{
	_GlyphAreaCtx *ctx = static_cast<_GlyphAreaCtx *>(data);
	UT_return_if_fail(ctx);

	GdkRGBA fg;
	gtk_widget_get_color(GTK_WIDGET(area), &fg);
	const GdkRGBA ac { 0.2, 0.45, 0.9, 1.0 };

	_GlyphPen p;
	_pen_init(p, cr, w, h, fg, ac);
	_GlyphDraw draw = _glyph_draw_for(ctx->name);
	if (draw)
		draw(p);
}

GtkWidget *
abi_glyph_icon_widget(const gchar *icon_name, int pixel_size)
{
	_GlyphDraw draw = _glyph_draw_for(icon_name);
	if (!draw)
		return nullptr;

	_GlyphAreaCtx *ctx = new _GlyphAreaCtx;
	g_strlcpy(ctx->name, icon_name, sizeof(ctx->name));

	GtkWidget *area = gtk_drawing_area_new();
	gtk_widget_set_size_request(area, pixel_size, pixel_size);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), _glyph_area_draw,
								   ctx, [](gpointer d) {
		delete static_cast<_GlyphAreaCtx *>(d);
	});
	return area;
}

/* ---------------- GIcon for menus ---------------- */

static GdkRGBA _BLACK = { 0.16, 0.16, 0.16, 1.0 };

/* Render to PNG bytes and wrap as a GBytesIcon so GMenuModel icons work
 * for names no theme provides.  The foreground can't follow a widget
 * here - a neutral dark-grey keeps menus readable in either mode. */
GIcon *
abi_glyph_icon_gicon(const gchar *icon_name, int pixel_size)
{
	_GlyphDraw draw = _glyph_draw_for(icon_name);
	if (!draw)
		return nullptr;

	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
								   pixel_size, pixel_size);
	cairo_t *cr = cairo_create(surface);
	GdkRGBA ac { 0.2, 0.45, 0.9, 1.0 };
	abi_glyph_icon_paint(icon_name, cr, pixel_size, pixel_size, _BLACK, ac);
	cairo_destroy(cr);

	GBytes *png = nullptr;
	{
		GByteArray *ba = g_byte_array_new();
		cairo_surface_write_to_png_stream(
			surface,
			[](void *ud, const unsigned char *d, unsigned int len) {
				g_byte_array_append(static_cast<GByteArray *>(ud), d, len);
				return CAIRO_STATUS_SUCCESS;
			},
			ba);
		png = g_byte_array_free_to_bytes(ba);
	}
	cairo_surface_destroy(surface);
	GIcon *icon = g_bytes_icon_new(png);
	g_bytes_unref(png);
	return icon;
}
