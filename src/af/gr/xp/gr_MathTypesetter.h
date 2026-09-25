/* Abinova
 * Copyright (C) 2025 AbiSource
 * Copyright (C) 2025-2026 Abinova contributors
 *
 * Self-contained mathematical typesetter for Abinova equations.
 * Parses a LaTeX subset or MathML into a box tree, lays it out and
 * renders it with Cairo. No external math libraries required.
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

#include <cairo.h>
#include <string>
#include <vector>
#include "ut_types.h"
#include "ut_string_class.h"

/*!
 * A node of the math box tree. All metrics are in points (1/72 inch).
 */
struct ABI_EXPORT MNode
{
	enum Kind : uint8_t {
		ROW,      /*!< horizontal sequence of children */
		ATOM,     /*!< glyph run (t = utf8 text) */
		SCRIPT,   /*!< k[0]=base, optional k[1]=sup, k[2]=sub (aux bits mark presence) */
		FRAC,     /*!< k[0]=numerator, k[1]=denominator */
		RADICAL,  /*!< k[0]=radicand, optional k[1]=index */
		FENCE,    /*!< t=open delim, t2=close delim, k[0]=inner; aux: 1=box(enclose) */
		ACCENT,   /*!< t=accent id (hat,tilde,bar,vec,dot,ddot,overline,underline), k[0]=base */
		LIMITS,   /*!< k[0]=base, optional k[1]=over, k[2]=under (aux bits) */
		MATRIX,   /*!< aux=ncols; t=open, t2=close; kids = cells row-major */
		SPACE     /*!< w stored directly, aux = signed width in 1/100 em */
	};

	Kind kind;
	std::vector<MNode*> k;
	std::vector<double> kx;   /*!< child x offset from node left */
	std::vector<double> ky;   /*!< child baseline offset from node top */
	UT_UTF8String t;
	UT_UTF8String t2;
	unsigned fl = 0;
	int aux = 0;
	double sz = 0;  /*!< effective font size this node was laid out at */
	double w = 0;   /*!< width */
	double a = 0;   /*!< ascent (above baseline) */
	double d = 0;   /*!< descent (below baseline) */

	MNode(Kind kk) : kind(kk) {}
	~MNode() { for (auto c : k) delete c; }
};

/* atom flags */
#define MF_ITALIC   0x0001   /*!< italic glyphs (variables) */
#define MF_BOLD     0x0002
#define MF_REL      0x0004   /*!< relation: medium space around */
#define MF_BIN      0x0008   /*!< binary op: small space around */
#define MF_BIGOP    0x0010   /*!< large operator (sum/int) */
#define MF_LIMITS   0x0020   /*!< big op taking under/over limits in display mode */
#define MF_OPEN     0x0040   /*!< opening delimiter */
#define MF_CLOSE    0x0080   /*!< closing delimiter */
#define MF_UPRIGHT  0x0100   /*!< force upright (function names, text) */
#define MF_INNER    0x0200   /*!< inner atom, suppresses binary spacing */
#define MF_NOBAR    0x0400   /*!< fraction without rule (binomial) */
#define MF_NOACC    0x0800   /*!< mover that is a limit, not an accent */

class ABI_EXPORT GR_MathTypesetter
{
public:
	GR_MathTypesetter();
	~GR_MathTypesetter();

	/*! parse LaTeX source; returns false on hard failure (still produces
	 *  whatever could be parsed). */
	bool parseLaTeX(const char *sz);
	/*! parse a MathML document. */
	bool parseMathML(const char *sz, int len = -1);
	/*! serialise the tree as MathML (used for LaTeX->MathML conversion). */
	UT_UTF8String toMathML() const;
	/*! serialise the tree back to LaTeX (best effort). */
	UT_UTF8String toLaTeX() const;

	/*! measure the tree. cr is used for font metrics only. */
	void layout(cairo_t *cr, const char *family, double baseSizePt,
	            bool displayStyle);
	/*! draw at the current cairo origin; top-left of the bounding box
	 *  is (0,0), so translate before calling. */
	void render(cairo_t *cr);

	double width()   const { return m_w; }
	double ascent()  const { return m_a; }
	double descent() const { return m_d; }
	bool  empty()    const { return m_root == nullptr; }
	bool  hasError() const { return m_bError; }
	const char *errorText() const { return m_sErr.c_str(); }

	void setColor(double r, double g, double b) { m_r = r; m_g = g; m_b = b; }

private:
	MNode *m_root;
	double m_w, m_a, m_d;
	double m_r, m_g, m_b;
	bool m_bError;
	std::string m_sErr;
	cairo_t *m_cr;           /*!< valid during layout/render */
	std::string m_family;
	double m_baseSize;
	bool m_display;

	/* layout helpers */
	void   _measure(MNode *n, double size, unsigned inherit);
	void   _atomExtents(const char *txt, double size, bool italic, bool bold,
	                    double &w, double &a, double &d);
	void   _selectFont(double size, bool italic, bool bold);
	void   _draw(MNode *n, double x, double y);
	void   _drawAtom(MNode *n, double x, double y);
	void   _drawDelim(cairo_t *cr, char delim, double x, double yTop,
	                  double h, double size);
	double _delimWidth(char delim, double size) const;
	void   _drawAccent(const char *id, double x, double w, double y, double size);
	/* helpers shared by measure + draw */
	double _scriptShiftUp(MNode *base, double size) const;
	double _scriptShiftDown(MNode *base, double size) const;

	friend struct MLatexParser;
	friend struct MMLParser;
};
