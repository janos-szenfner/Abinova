/* Abinova
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2002 Patrick Lam <plam@mit.edu>
 * Copyright (C) 2003 Martin Sevior <msevior@physics.unimelb.edu.au>
 * Copyright (C) 2022 Hubert Figuière
 * Copyright (C) 2025-2026 Abinova contributors
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

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "fp_FrameContainer.h"
#include "fp_Column.h"
#include "fp_Page.h"
#include "fp_Line.h"
#include "fl_DocLayout.h"
#include "pd_Document.h"
#include "fl_SectionLayout.h"
#include "gr_DrawArgs.h"
#include "ut_vector.h"
#include "ut_types.h"
#include "ut_units.h"
#include "ut_debugmsg.h"
#include "ut_assert.h"
#include "fl_FrameLayout.h"
#include "fp_TableContainer.h"
#include "fv_View.h"
#include "gr_Painter.h"
#include "gr_CairoGraphics.h"
#include "fl_BlockLayout.h"

/*!
  Create Frame container
  \param iType Container type
  \param pSectionLayout Section layout type used for this container
 */
fp_FrameContainer::fp_FrameContainer(fl_SectionLayout* pSectionLayout) 
	: fp_VerticalContainer(FP_CONTAINER_FRAME, pSectionLayout),
	  m_pPage(nullptr),
	  m_iXpadLeft(0),
	  m_iXpadRight(0),
	  m_iYpadTop(0),
	  m_iYpadBottom(0),
	  m_bNeverDrawn(true),
	  m_bOverWrote(false),
	  m_bIsWrapped(false),
	  m_bIsTightWrapped(false),
	  m_bIsAbove(true),
	  m_bIsTopBot(false),
	  m_bIsLeftWrapped(false),
	  m_bIsRightWrapped(false),
	  m_iPreferedPageNo(-1),
	  m_iPreferedColumnNo(0)
{
}

/*!
  Destruct container
  \note The Containers in vector of the container are not
        destructed. They are owned by the logical hierarchy (i.e.,
		the fl_Container classes like fl_BlockLayout), not the physical
        hierarchy.
 */
fp_FrameContainer::~fp_FrameContainer()
{
  UT_DEBUGMSG(("Delete FrameContainer %p \n", (void*)this));
	m_pPage = nullptr;
}

void fp_FrameContainer::setPage(fp_Page * pPage)
{
	if(pPage && (m_pPage != nullptr) && m_pPage != pPage)
	{
		clearScreen();
		m_pPage->removeFrameContainer(this);
		getSectionLayout()->markAllRunsDirty();
		
		UT_GenericVector<fl_ContainerLayout *> AllLayouts;
		AllLayouts.clear();
		m_pPage->getAllLayouts(AllLayouts);
		UT_sint32 i = 0;
		for(i=0; i<AllLayouts.getItemCount(); i++)
		{
		      fl_ContainerLayout * pCL = AllLayouts.getNthItem(i);
		      pCL->collapse();
		      pCL->format();
	        }
		m_pPage->getOwningSection()->setNeedsSectionBreak(true,m_pPage);
		
	}
	m_pPage = pPage;
	if(pPage)
	{
		getFillType().setParent(&pPage->getFillType());
	}
	else
	{
		getFillType().setParent(nullptr);
	}
}


bool fp_FrameContainer::isAbove(void)
{
  return  m_bIsAbove;
}

/*!
 * Returns the frame's stacking rank within its page layer (the
 * above-text or below-text vector on fp_Page).  Read from the
 * "frame-stack-order" frame property; defaults to 0.
 */
double fp_FrameContainer::getStackOrder(void)
{
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(getSectionLayout());
	const PP_AttrProp * pAP = nullptr;
	if (pFL)
		pFL->getAP(pAP);
	const gchar * sz = nullptr;
	if (pAP && pAP->getProperty("frame-stack-order", sz) && sz && *sz)
		return g_ascii_strtod(sz, nullptr);
	return 0.0;
}

/*!
 * Returns true when the frame's "frame-hidden" property is set - used
 * by the Selection pane's eye toggle.  Hidden frames keep their
 * layout slot (text still wraps around them) but are neither drawn
 * nor clickable, like hidden objects in Word.
 */
bool fp_FrameContainer::isHidden(void)
{
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(getSectionLayout());
	const PP_AttrProp * pAP = nullptr;
	if (pFL)
		pFL->getAP(pAP);
	const gchar * sz = nullptr;
	return pAP && pAP->getProperty("frame-hidden", sz) &&
		   sz && sz[0] && strcmp(sz, "0") != 0 &&
		   strcmp(sz, "false") != 0;
}

/*!
 * Clockwise rotation angle in degrees, from the "frame-rotation"
 * property.  Rotation is applied as a cairo transform around the
 * frame centre at draw time - layout and wrapping keep using the
 * unrotated rectangle.
 */
double fp_FrameContainer::getRotation(void)
{
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(getSectionLayout());
	const PP_AttrProp * pAP = nullptr;
	if (pFL)
		pFL->getAP(pAP);
	const gchar * sz = nullptr;
	if (pAP && pAP->getProperty("frame-rotation", sz) && sz && *sz)
		return g_ascii_strtod(sz, nullptr);
	return 0.0;
}

/*!
 * Clockwise rotation of the frame's text stack in degrees, from the
 * "frame-text-direction" property (the raw OOXML wps:bodyPr@vert
 * token).  vert/eaVert/mongolianVert run lines top to bottom and
 * stack them right to left (90 degrees); vert270 runs bottom to top
 * stacking left to right (270 degrees).  The wordArt modes stack
 * upright glyphs one per line - a plain 90 degree rotation is the
 * closest single-transform approximation and keeps the tall narrow
 * footprint Word reserves for the box.  The content is laid out in a
 * logical space whose extents are the swapped inner box (see
 * getWidth()/getHeight()) and rotated into place in draw().
 */
int fp_FrameContainer::getTextRotation(void) const
{
	fp_FrameContainer * self = const_cast<fp_FrameContainer *>(this);
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(
		self->getSectionLayout());
	const PP_AttrProp * pAP = nullptr;
	if (pFL)
		pFL->getAP(pAP);
	const gchar * sz = nullptr;
	if (!pAP || !pAP->getProperty("frame-text-direction", sz) ||
		!sz || !*sz)
		return 0;
	if (!strcmp(sz, "vert270"))
		return 270;
	if (!strcmp(sz, "vert") || !strcmp(sz, "eaVert") ||
		!strcmp(sz, "mongolianVert") || !strcmp(sz, "wordArtVert") ||
		!strcmp(sz, "wordArtVertRtl"))
		return 90;
	return 0;
}

static bool s_frameBoolProp(fp_FrameContainer * pFC, const char * szName)
{
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(pFC->getSectionLayout());
	const PP_AttrProp * pAP = nullptr;
	if (pFL)
		pFL->getAP(pAP);
	const gchar * sz = nullptr;
	return pAP && pAP->getProperty(szName, sz) &&
		   sz && sz[0] && strcmp(sz, "0") != 0 &&
		   strcmp(sz, "false") != 0;
}

bool fp_FrameContainer::isFlippedHoriz(void)
{
	return s_frameBoolProp(this, "frame-flip-horiz");
}

bool fp_FrameContainer::isFlippedVert(void)
{
	return s_frameBoolProp(this, "frame-flip-vert");
}

bool fp_FrameContainer::isTransformed(void)
{
	return getRotation() != 0.0 || isFlippedHoriz() || isFlippedVert();
}

/*!
 * The "frame-group" id shared by the members of a group, or nullptr
 * when the frame is not grouped.  The pointer is only valid while the
 * frame's attributes are unchanged - callers should copy it.
 */
const char * fp_FrameContainer::getGroupId(void) const
{
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(
		const_cast<fp_FrameContainer *>(this)->getSectionLayout());
	const PP_AttrProp * pAP = nullptr;
	if (pFL)
		pFL->getAP(pAP);
	const gchar * sz = nullptr;
	if (pAP && pAP->getProperty("frame-group", sz) && sz && *sz)
		return sz;
	return nullptr;
}

/*!
 * Rotates the four corners of a rect around its centre and returns
 * the axis-aligned bounding box of the result.
 */
static void s_rotatedBounds(double rx, double ry, double rw, double rh,
							double deg, UT_Rect & out)
{
	if (deg == 0.0)
	{
		out.left = static_cast<UT_sint32>(rx);
		out.top = static_cast<UT_sint32>(ry);
		out.width = static_cast<UT_sint32>(rw);
		out.height = static_cast<UT_sint32>(rh);
		return;
	}
	double rad = deg * M_PI / 180.0;
	double c = cos(rad), s = sin(rad);
	double cx = rx + rw / 2.0, cy = ry + rh / 2.0;
	double xMin = 1e30, yMin = 1e30, xMax = -1e30, yMax = -1e30;
	static const double corner[4][2] = {
		{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}
	};
	for (int i = 0; i < 4; i++)
	{
		double dx = rx + corner[i][0] * rw - cx;
		double dy = ry + corner[i][1] * rh - cy;
		double px = cx + dx * c - dy * s;
		double py = cy + dx * s + dy * c;
		xMin = UT_MIN(xMin, px); xMax = UT_MAX(xMax, px);
		yMin = UT_MIN(yMin, py); yMax = UT_MAX(yMax, py);
	}
	out.left = static_cast<UT_sint32>(floor(xMin));
	out.top = static_cast<UT_sint32>(floor(yMin));
	out.width = static_cast<UT_sint32>(ceil(xMax)) - out.left;
	out.height = static_cast<UT_sint32>(ceil(yMax)) - out.top;
}

/*!
 * Inverse of the vertical-text paint transform: maps a rect in the
 * frame's physical (page) space to the logical space the content is
 * laid out in.  The rotation is a multiple of 90 degrees so the
 * result stays axis-aligned.  (ox,oy) is the inner box origin, iw/ih
 * the inner box extents, all in layout units.
 */
static void s_unrotateFrameRect(UT_Rect & r, UT_sint32 ox, UT_sint32 oy,
								UT_sint32 iw, UT_sint32 ih, int rot)
{
	UT_Rect t;
	if (rot == 90)
	{
		/* forward map: lx = py - oy, ly = ox + iw - px */
		t.left = r.top - oy;
		t.top = ox + iw - (r.left + r.width);
		t.width = r.height;
		t.height = r.width;
	}
	else
	{
		/* vert270, forward map: lx = oy + ih - py, ly = px - ox */
		t.left = oy + ih - (r.top + r.height);
		t.top = r.left - ox;
		t.width = r.height;
		t.height = r.width;
	}
	r = t;
}

/*!
 * Paint a linear gradient across the frame box from the
 * "fill-gradient" property.  The property serializes DrawingML
 * gradFill as "lin:<ang60000>,<pos>:<RRGGBB>,..." where pos is
 * 0..100000 (fraction of the gradient vector in thousandths of a
 * percent) and ang is the DrawingML angle in 60000ths of a degree,
 * measured clockwise from the 3 o'clock direction with y pointing
 * down.  Returns false when no usable gradient is present (or the
 * graphics isn't cairo mid-paint) so the caller can fall back to the
 * solid fill.
 */
static bool s_paintFrameGradient(GR_Graphics * pG,
								 fp_FrameContainer * pFC,
								 UT_sint32 x, UT_sint32 y,
								 UT_sint32 w, UT_sint32 h)
{
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(pFC->getSectionLayout());
	const PP_AttrProp * pAP = nullptr;
	if (pFL)
		pFL->getAP(pAP);
	const gchar * szGrad = nullptr;
	if (!pAP || !pAP->getProperty("fill-gradient", szGrad) ||
		!szGrad || !*szGrad)
		return false;

	GR_CairoGraphics * pCG = dynamic_cast<GR_CairoGraphics *>(pG);
	if (!pCG)
		return false;
	/* getCairo() auto-beginPaints when no paint is running - that is
	 * harmless on the print/PDF graphics (its _beginPaint is a no-op
	 * counter) but would unbalance the group stack on screen, so only
	 * allow the implicit beginPaint off-screen */
	if (pCG->getPaintCount() <= 0 &&
		pG->queryProperties(GR_Graphics::DGP_SCREEN))
		return false;
	cairo_t * cr = pCG->getCairo();
	if (!cr)
		return false;

	struct GradStop { double pos, r, g, b; };
	std::vector<GradStop> stops;
	double dAng60000 = 5400000.0; /* default: straight down (90 deg) */

	gchar ** toks = g_strsplit(szGrad, ",", -1);
	if (toks)
	{
		for (int i = 0; toks[i]; i++)
		{
			gchar * tok = g_strstrip(toks[i]);
			if (!*tok)
				continue;
			if (strncmp(tok, "lin:", 4) == 0)
			{
				dAng60000 = g_ascii_strtod(tok + 4, nullptr);
				continue;
			}
			gchar * colon = strchr(tok, ':');
			if (!colon)
				continue;
			*colon = 0;
			double pos = g_ascii_strtod(tok, nullptr) / 100000.0;
			const gchar * hex = colon + 1;
			unsigned int rv = 0, gv = 0, bv = 0;
			if (sscanf(hex, "%02x%02x%02x", &rv, &gv, &bv) == 3)
			{
				GradStop gs { pos, rv / 255.0, gv / 255.0, bv / 255.0 };
				stops.push_back(gs);
			}
		}
		g_strfreev(toks);
	}
	if (stops.size() < 2)
		return false;

	/* gradient vector endpoints: project the box corners onto the
	 * direction axis so the first/last stops land on opposite edges */
	double rad = (dAng60000 / 60000.0) * M_PI / 180.0;
	double dx = cos(rad), dy = sin(rad);
	double x0 = pG->tdu(x), y0 = pG->tdu(y);
	double x1 = pG->tdu(x + w), y1 = pG->tdu(y + h);
	double cx = (x0 + x1) / 2.0, cy = (y0 + y1) / 2.0;
	double corners[4][2] = {{x0,y0},{x1,y0},{x1,y1},{x0,y1}};
	double pmin = 1e30, pmax = -1e30;
	for (int i = 0; i < 4; i++)
	{
		double t = (corners[i][0] - cx) * dx + (corners[i][1] - cy) * dy;
		pmin = UT_MIN(pmin, t);
		pmax = UT_MAX(pmax, t);
	}
	cairo_pattern_t * pat = cairo_pattern_create_linear(
		cx + dx * pmin, cy + dy * pmin,
		cx + dx * pmax, cy + dy * pmax);
	for (const GradStop & gs : stops)
	{
		double pos = gs.pos < 0.0 ? 0.0 : (gs.pos > 1.0 ? 1.0 : gs.pos);
		cairo_pattern_add_color_stop_rgb(pat, pos, gs.r, gs.g, gs.b);
	}
	cairo_save(cr);
	cairo_rectangle(cr, x0, y0, x1 - x0, y1 - y0);
	cairo_set_source(cr, pat);
	cairo_fill(cr);
	cairo_restore(cr);
	cairo_pattern_destroy(pat);
	return true;
}

/*!
 * Parse an abwn number that may use a decimal comma ("0,300").
 */
static double s_abwnDouble(const gchar * sz)
{
	if (!sz)
		return 0.0;
	gchar * copy = g_strdup(sz);
	for (gchar * p = copy; *p; p++)
	{
		if (*p == ',')
			*p = '.';
	}
	double v = g_ascii_strtod(copy, nullptr);
	g_free(copy);
	return v;
}

/*!
 * Paint the frame's background color with the transparency from
 * "fill-alpha" (a:alpha on the fill, serialized as a 0..1 fraction).
 * Returns false when fill-alpha is absent or ~opaque so the caller
 * falls back to the plain solid fill.
 */
static bool s_paintFrameAlpha(GR_Graphics * pG,
							  fp_FrameContainer * pFC,
							  UT_sint32 x, UT_sint32 y,
							  UT_sint32 w, UT_sint32 h)
{
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(pFC->getSectionLayout());
	const PP_AttrProp * pAP = nullptr;
	if (pFL)
		pFL->getAP(pAP);
	const gchar * szAlpha = nullptr;
	if (!pAP || !pAP->getProperty("fill-alpha", szAlpha) ||
		!szAlpha || !*szAlpha)
		return false;
	/* abwn writes decimal commas */
	double alpha = s_abwnDouble(szAlpha);
	if (alpha >= 0.999)
		return false;
	if (alpha < 0.0)
		alpha = 0.0;

	GR_CairoGraphics * pCG = dynamic_cast<GR_CairoGraphics *>(pG);
	if (!pCG)
		return false;
	if (pCG->getPaintCount() <= 0 &&
		pG->queryProperties(GR_Graphics::DGP_SCREEN))
		return false;
	cairo_t * cr = pCG->getCairo();
	if (!cr)
		return false;

	const UT_RGBColor * pCol = pFC->getFillType().getColor();
	if (!pCol || pCol->isTransparent())
		return false;

	cairo_save(cr);
	cairo_rectangle(cr, pG->tdu(x), pG->tdu(y),
					pG->tdu(x + w) - pG->tdu(x),
					pG->tdu(y + h) - pG->tdu(y));
	cairo_set_source_rgba(cr,
						  pCol->m_red / 255.0, pCol->m_grn / 255.0,
						  pCol->m_blu / 255.0, alpha);
	cairo_fill(cr);
	cairo_restore(cr);
	return true;
}

/*!
 * Append the "shape-path" freeform geometry (DrawingML a:custGeom
 * serialized as "M x y L x y C x1 y1 x2 y2 x3 y3 Q x1 y1 x2 y2 Z" in
 * a normalized 0..1000 box) to the current cairo path, mapped onto
 * the device-space rectangle (dx0,dy0,dw,dh).  Returns false when the
 * descriptor produced no path.  Used for both painting the shape and
 * building its shadow silhouette.
 */
static bool s_frameShapePath(cairo_t * cr, const gchar * szPath,
							 double dx0, double dy0,
							 double dw, double dh)
{
	/* tokenize: letters start a command, numbers feed the pending
	 * command (M/L need 2, Q needs 4, C needs 6 coordinates) */
	char cmd = 0;
	double nums[6];
	int nNums = 0;
	double cx0 = 0.0, cy0 = 0.0; /* current point, for Q->C conversion */
	bool bAny = false;

	gchar * copy = g_strdup(szPath);
	gchar * save = nullptr;
	for (gchar * tok = strtok_r(copy, " \t", &save); tok;
		 tok = strtok_r(nullptr, " \t", &save))
	{
		if ((tok[0] >= 'A' && tok[0] <= 'Z') ||
			(tok[0] >= 'a' && tok[0] <= 'z'))
		{
			cmd = tok[0];
			nNums = 0;
			if (cmd == 'Z' || cmd == 'z')
			{
				cairo_close_path(cr);
				bAny = true;
			}
			continue;
		}
		double v = g_ascii_strtod(tok, nullptr);
		nums[nNums < 6 ? nNums : 5] = v;
		nNums++;
		double px = 0.0, py = 0.0;
		switch (cmd)
		{
		case 'M':
			if (nNums >= 2)
			{
				px = dx0 + nums[0] * dw / 1000.0;
				py = dy0 + nums[1] * dh / 1000.0;
				cairo_move_to(cr, px, py);
				cx0 = px; cy0 = py;
				nNums = 0; bAny = true;
			}
			break;
		case 'L':
			if (nNums >= 2)
			{
				px = dx0 + nums[0] * dw / 1000.0;
				py = dy0 + nums[1] * dh / 1000.0;
				cairo_line_to(cr, px, py);
				cx0 = px; cy0 = py;
				nNums = 0; bAny = true;
			}
			break;
		case 'C':
			if (nNums >= 6)
			{
				cairo_curve_to(cr,
							   dx0 + nums[0] * dw / 1000.0,
							   dy0 + nums[1] * dh / 1000.0,
							   dx0 + nums[2] * dw / 1000.0,
							   dy0 + nums[3] * dh / 1000.0,
							   dx0 + nums[4] * dw / 1000.0,
							   dy0 + nums[5] * dh / 1000.0);
				cx0 = dx0 + nums[4] * dw / 1000.0;
				cy0 = dy0 + nums[5] * dh / 1000.0;
				nNums = 0; bAny = true;
			}
			break;
		case 'Q':
			if (nNums >= 4)
			{
				/* quadratic -> cubic: c1 = p0 + 2/3(q-p0),
				 * c2 = p2 + 2/3(q-p2) */
				double qx = dx0 + nums[0] * dw / 1000.0;
				double qy = dy0 + nums[1] * dh / 1000.0;
				double ex = dx0 + nums[2] * dw / 1000.0;
				double ey = dy0 + nums[3] * dh / 1000.0;
				cairo_curve_to(cr,
							   cx0 + 2.0 * (qx - cx0) / 3.0,
							   cy0 + 2.0 * (qy - cy0) / 3.0,
							   ex + 2.0 * (qx - ex) / 3.0,
							   ey + 2.0 * (qy - ey) / 3.0,
							   ex, ey);
				cx0 = ex; cy0 = ey;
				nNums = 0; bAny = true;
			}
			break;
		default:
			break;
		}
	}
	g_free(copy);
	return bAny;
}

/*!
 * Paint a freeform shape from the "shape-path" property - the
 * DrawingML a:custGeom path serialized as "M x y L x y C x1 y1 x2 y2
 * x3 y3 Q x1 y1 x2 y2 Z" with coordinates normalized into a 0..1000
 * box that maps onto the frame rectangle.  Fills the path with the
 * frame's gradient (when present) or solid background color, then
 * strokes it with the frame's border.  Returns false when no usable
 * path is stored or the graphics isn't cairo.
 */
static bool s_paintFrameShape(GR_Graphics * pG,
							  fp_FrameContainer * pFC,
							  UT_sint32 x, UT_sint32 y,
							  UT_sint32 w, UT_sint32 h)
{
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(pFC->getSectionLayout());
	const PP_AttrProp * pAP = nullptr;
	if (pFL)
		pFL->getAP(pAP);
	const gchar * szPath = nullptr;
	if (!pAP || !pAP->getProperty("shape-path", szPath) ||
		!szPath || !*szPath)
		return false;

	GR_CairoGraphics * pCG = dynamic_cast<GR_CairoGraphics *>(pG);
	if (!pCG)
		return false;
	if (pCG->getPaintCount() <= 0 &&
		pG->queryProperties(GR_Graphics::DGP_SCREEN))
		return false;
	cairo_t * cr = pCG->getCairo();
	if (!cr)
		return false;

	double dx0 = pG->tdu(x), dy0 = pG->tdu(y);
	double dw = pG->tdu(x + w) - dx0, dh = pG->tdu(y + h) - dy0;
	if (dw <= 0 || dh <= 0)
		return false;

	cairo_new_path(cr);
	if (!s_frameShapePath(cr, szPath, dx0, dy0, dw, dh))
		return false;

	/* fill: gradient clipped to the path when present, else the
	 * frame's resolved background color */
	cairo_save(cr);
	cairo_clip(cr);
	bool bPainted = s_paintFrameGradient(pG, pFC, x, y, w, h);
	if (!bPainted)
	{
		const UT_RGBColor * pCol = pFC->getFillType().getColor();
		if (pCol && !pCol->isTransparent())
		{
			double alpha = 1.0;
			const gchar * szAlpha = nullptr;
			if (pAP->getProperty("fill-alpha", szAlpha) && szAlpha)
			{
				alpha = s_abwnDouble(szAlpha);
				if (alpha < 0.0)
					alpha = 0.0;
				if (alpha > 1.0)
					alpha = 1.0;
			}
			cairo_set_source_rgba(cr,
								  pCol->m_red / 255.0,
								  pCol->m_grn / 255.0,
								  pCol->m_blu / 255.0,
								  alpha);
			cairo_paint(cr);
		}
	}
	cairo_restore(cr);

	/* stroke with the frame's border (uniform-outline approximation:
	 * uses the top edge's style/color/thickness) */
	const PP_PropertyMap::Line & topLine = pFC->getTopStyle();
	if (topLine.m_t_linestyle != PP_PropertyMap::linestyle_none &&
		topLine.m_thickness > 0)
	{
		const UT_RGBColor & bc = topLine.m_color;
		cairo_set_source_rgb(cr,
							 bc.m_red / 255.0, bc.m_grn / 255.0,
							 bc.m_blu / 255.0);
		cairo_set_line_width(cr, pG->tdu(topLine.m_thickness));
		cairo_stroke(cr);
	}
	cairo_new_path(cr);
	return true;
}

/*!
 * Map an OOXML line-end size token (a:headEnd/a:tailEnd @w/@len:
 * "sm"/"med"/"lg") to the marker-cell multiplier other DrawingML
 * renderers use - sm/med/lg = 2/3/5 times the line width, with the
 * open "arrow" head on a slightly larger 2.5/3.5/5.5 scale.
 */
static double s_arrowScale(const gchar * sz, bool bArrow)
{
	if (!sz || !*sz || !strcmp(sz, "med"))
		return bArrow ? 3.5 : 3.0;
	if (!strcmp(sz, "sm"))
		return bArrow ? 2.5 : 2.0;
	if (!strcmp(sz, "lg"))
		return bArrow ? 5.5 : 5.0;
	return bArrow ? 3.5 : 3.0;
}

/*!
 * Stroke out one line-end marker into the cairo path.  The marker
 * cell is 100x100 with the tip at (50,0) and the body extending to
 * y=100; (mx,my) are cell coordinates in percent, (dx,dy) the unit
 * vector from the tip back into the line body, (px,py) its
 * perpendicular.  fW/fL are the full marker width/length in device
 * units, lw the line width.
 */
static void s_arrowMarkerPath(cairo_t * cr, const gchar * szType,
							  double tipX, double tipY,
							  double dx, double dy,
							  double fW, double fL, double lw)
{
	double px = -dy, py = dx;
	auto pt = [&](double mx, double my) {
		cairo_line_to(cr,
					  tipX + px * (mx - 50.0) / 100.0 * fW +
						  dx * my / 100.0 * fL,
					  tipY + py * (mx - 50.0) / 100.0 * fW +
						  dy * my / 100.0 * fL);
	};
	if (!strcmp(szType, "oval"))
	{
		/* ellipse inscribed in the marker cell, cell +y along (dx,dy) */
		cairo_save(cr);
		cairo_translate(cr, tipX + dx * fL / 2.0, tipY + dy * fL / 2.0);
		cairo_rotate(cr, atan2(dy, dx) - M_PI / 2.0);
		cairo_scale(cr, fW / 2.0, fL / 2.0);
		cairo_arc(cr, 0.0, 0.0, 1.0, 0.0, 2.0 * M_PI);
		cairo_restore(cr);
		return;
	}
	cairo_move_to(cr, tipX, tipY);
	if (!strcmp(szType, "stealth"))
	{
		pt(100.0, 100.0); pt(50.0, 60.0); pt(0.0, 100.0);
	}
	else if (!strcmp(szType, "diamond"))
	{
		pt(100.0, 50.0); pt(50.0, 100.0); pt(0.0, 50.0);
	}
	else if (!strcmp(szType, "arrow"))
	{
		/* open arrow outline - the waist notch follows the line's
		 * half-width expressed in marker-cell percent (tdf#100491) */
		double hw = UT_MAX(50.0 * lw / fW, 1.0);
		pt(100.0, 100.0 - 1.5 * hw);
		pt(100.0 - 1.5 * hw, 100.0);
		pt(50.0 + hw, 5.5 * hw);
		pt(50.0 + hw, 100.0);
		pt(50.0 - hw, 100.0);
		pt(50.0 - hw, 5.5 * hw);
		pt(1.5 * hw, 100.0);
		pt(0.0, 100.0 - 1.5 * hw);
	}
	else /* triangle */
	{
		pt(100.0, 100.0); pt(0.0, 100.0);
	}
	cairo_close_path(cr);
}

/*!
 * Paint OOXML line-end decorations (a:headEnd/a:tailEnd, serialized
 * as the line-start-arrow/line-end-arrow frame props) at the ends of
 * a bar frame.  The importer renders prstGeom="line" shapes as filled
 * bars whose short side is the stroke width, so a decorated end lands
 * centred on the end cap with its tip pointing outward; head is the
 * left/top end, tail the right/bottom end (frame flips transform the
 * whole scene and swap the ends automatically).  Per ECMA-376 line
 * ends only apply to open line elements - frames without the
 * bar-w/bar-h marker prop are skipped.  (x,y,w,h) are layout units.
 */
static void s_paintFrameArrows(GR_Graphics * pG,
							   fp_FrameContainer * pFC,
							   UT_sint32 x, UT_sint32 y,
							   UT_sint32 w, UT_sint32 h)
{
	if (w <= 0 || h <= 0)
		return;
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(pFC->getSectionLayout());
	const PP_AttrProp * pAP = nullptr;
	if (pFL)
		pFL->getAP(pAP);
	if (!pAP)
		return;
	const gchar * szHead = nullptr, * szTail = nullptr;
	const gchar * szHeadW = nullptr, * szHeadL = nullptr;
	const gchar * szTailW = nullptr, * szTailL = nullptr;
	const gchar * szBarW = nullptr, * szBarH = nullptr;
	pAP->getProperty("line-start-arrow", szHead);
	pAP->getProperty("line-end-arrow", szTail);
	pAP->getProperty("line-start-arrow-w", szHeadW);
	pAP->getProperty("line-start-arrow-len", szHeadL);
	pAP->getProperty("line-end-arrow-w", szTailW);
	pAP->getProperty("line-end-arrow-len", szTailL);
	bool bHead = szHead && *szHead && strcmp(szHead, "none") != 0;
	bool bTail = szTail && *szTail && strcmp(szTail, "none") != 0;
	if (!bHead && !bTail)
		return;
	bool bBarW = pAP->getProperty("bar-w", szBarW) && szBarW && *szBarW;
	bool bBarH = pAP->getProperty("bar-h", szBarH) && szBarH && *szBarH;
	if (!bBarW && !bBarH)
		return;
	/* vertical when the width carries the stroke thickness */
	bool bVert = (bBarW && !bBarH) || (bBarW && bBarH && h > w);
	double lw = pG->tdu(bVert ? w : h);
	if (lw <= 0.0)
		return;

	GR_CairoGraphics * pCG = dynamic_cast<GR_CairoGraphics *>(pG);
	if (!pCG)
		return;
	/* same implicit-beginPaint guard as the other cairo painters */
	if (pCG->getPaintCount() <= 0 &&
		pG->queryProperties(GR_Graphics::DGP_SCREEN))
		return;
	cairo_t * cr = pCG->getCairo();
	if (!cr)
		return;

	const UT_RGBColor * pFill = pFC->getFillType().getColor();
	UT_RGBColor col;
	if (pFill && !pFill->isTransparent())
		col = *pFill;
	else
		col = pFC->getTopStyle().m_color;

	/* arrows stick out past the bar caps - grow the damage bounds by
	 * the largest possible marker extent (lg len = 5.5x the width) */
	if (pFC->getPage())
	{
		UT_sint32 pad = static_cast<UT_sint32>(6.0 * (bVert ? w : h)) + 1;
		UT_Rect dmg;
		s_rotatedBounds(x - pad, y - pad,
						w + 2 * pad, h + 2 * pad,
						pFC->getRotation(), dmg);
		pFC->getPage()->expandDamageRect(dmg.left, dmg.top,
									   dmg.width, dmg.height);
	}

	cairo_set_source_rgb(cr,
						 col.m_red / 255.0, col.m_grn / 255.0,
						 col.m_blu / 255.0);
	double dx0 = pG->tdu(x), dy0 = pG->tdu(y);
	double dx1 = pG->tdu(x + w), dy1 = pG->tdu(y + h);
	double cx = (dx0 + dx1) / 2.0, cy = (dy0 + dy1) / 2.0;
	for (int end = 0; end < 2; end++)
	{
		const gchar * szType = end ? szTail : szHead;
		if (!szType || !*szType || !strcmp(szType, "none"))
			continue;
		bool bArrow = !strcmp(szType, "arrow");
		double fW = s_arrowScale(end ? szTailW : szHeadW, bArrow) * lw;
		double fL = s_arrowScale(end ? szTailL : szHeadL, bArrow) * lw;
		double tipX, tipY, dx, dy;
		if (bVert)
		{
			/* head at the top cap, tail at the bottom */
			tipX = cx;
			tipY = end ? dy1 : dy0;
			dx = 0.0;
			dy = end ? -1.0 : 1.0;
		}
		else
		{
			/* head at the left cap, tail at the right */
			tipX = end ? dx1 : dx0;
			tipY = cy;
			dx = end ? -1.0 : 1.0;
			dy = 0.0;
		}
		s_arrowMarkerPath(cr, szType, tipX, tipY, dx, dy, fW, fL, lw);
	}
	cairo_fill(cr);
}

/*!
 * Separable box blur over the alpha byte of a CAIRO_FORMAT_ARGB32
 * image surface (the shadow silhouette is opaque black, so RGB stay
 * zero and alpha carries the coverage).  Three passes approximate a
 * gaussian; edges use replicate extension.
 */
static void s_blurShadowAlpha(cairo_surface_t * surf, int radius)
{
	if (radius < 1)
		return;
	int w = cairo_image_surface_get_width(surf);
	int h = cairo_image_surface_get_height(surf);
	int stride = cairo_image_surface_get_stride(surf);
	unsigned char * data = cairo_image_surface_get_data(surf);
	if (!data || w <= 0 || h <= 0)
		return;
	cairo_surface_flush(surf);
	/* ARGB32 stores alpha in the most significant byte of each
	 * native-endian pixel word */
	const int aoff = (G_BYTE_ORDER == G_LITTLE_ENDIAN) ? 3 : 0;
	const int n = 2 * radius + 1;
	std::vector<unsigned char> tmp(w > h ? w : h);
	for (int pass = 0; pass < 3; pass++)
	{
		for (int y = 0; y < h; y++) /* horizontal */
		{
			unsigned char * row = data + y * stride + aoff;
			int sum = 0;
			for (int i = -radius; i <= radius; i++)
				sum += row[4 * (i < 0 ? 0 : (i >= w ? w - 1 : i))];
			for (int x = 0; x < w; x++)
			{
				tmp[x] = static_cast<unsigned char>((sum + n / 2) / n);
				int co = x - radius, cn = x + radius + 1;
				sum -= row[4 * (co < 0 ? 0 : (co >= w ? w - 1 : co))];
				sum += row[4 * (cn < 0 ? 0 : (cn >= w ? w - 1 : cn))];
			}
			for (int x = 0; x < w; x++)
				row[4 * x] = tmp[x];
		}
		for (int x = 0; x < w; x++) /* vertical */
		{
			unsigned char * col = data + x * 4 + aoff;
			int sum = 0;
			for (int i = -radius; i <= radius; i++)
				sum += col[stride * (i < 0 ? 0 : (i >= h ? h - 1 : i))];
			for (int y = 0; y < h; y++)
			{
				tmp[y] = static_cast<unsigned char>((sum + n / 2) / n);
				int ro = y - radius, rn = y + radius + 1;
				sum -= col[stride * (ro < 0 ? 0 : (ro >= h ? h - 1 : ro))];
				sum += col[stride * (rn < 0 ? 0 : (rn >= h ? h - 1 : rn))];
			}
			for (int y = 0; y < h; y++)
				col[stride * y] = tmp[y];
		}
	}
	cairo_surface_mark_dirty(surf);
}

/*!
 * Paint the OOXML a:outerShdw drop shadow (the "frame-shadow",
 * "frame-shadow-offset", "frame-shadow-dir", "frame-shadow-blur",
 * "frame-shadow-color", "frame-shadow-alpha" and "frame-shadow-rot"
 * properties).  The shape's silhouette - its custGeom path when
 * present, else the frame rectangle - is rasterized, box-blurred by
 * blurRad and masked onto the page in the shadow color, offset by
 * dist along dir (60000ths of a degree, clockwise from 3 o'clock,
 * matching the fill-gradient convention).  Painted under the frame's
 * rotation/flip transform so the silhouette follows the shape; a
 * rotWithShape="0" shadow keeps its offset fixed in page space by
 * undoing the transform on the offset vector.  (x,y,w,h) are layout
 * units.
 */
static void s_paintFrameShadow(GR_Graphics * pG,
							   fp_FrameContainer * pFC,
							   UT_sint32 x, UT_sint32 y,
							   UT_sint32 w, UT_sint32 h)
{
	if (w <= 0 || h <= 0)
		return;
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(pFC->getSectionLayout());
	const PP_AttrProp * pAP = nullptr;
	if (pFL)
		pFL->getAP(pAP);
	const gchar * sz = nullptr;
	if (!pAP || !pAP->getProperty("frame-shadow", sz) ||
		!sz || !*sz || !strcmp(sz, "none") || !strcmp(sz, "0"))
		return;

	/* a shape with nothing painted casts no shadow */
	const gchar * szPath = nullptr;
	bool bHasPath = pAP->getProperty("shape-path", szPath) &&
					szPath && *szPath;
	const UT_RGBColor * pFill = pFC->getFillType().getColor();
	bool bHasFill = pFill && !pFill->isTransparent();
	const PP_PropertyMap::Line & topLine = pFC->getTopStyle();
	bool bHasBorder = topLine.m_t_linestyle != PP_PropertyMap::linestyle_none &&
					  topLine.m_thickness > 0;
	if (!bHasFill && !bHasPath && !bHasBorder)
		return;

	UT_sint32 distLU = 0, blurLU = 0;
	if (pAP->getProperty("frame-shadow-offset", sz) && sz && *sz)
		distLU = UT_convertToLogicalUnits(sz);
	if (pAP->getProperty("frame-shadow-blur", sz) && sz && *sz)
		blurLU = UT_convertToLogicalUnits(sz);
	if (distLU <= 0 && blurLU <= 0)
		return;

	double dirRad = 0.0;
	if (pAP->getProperty("frame-shadow-dir", sz) && sz && *sz)
		dirRad = g_ascii_strtod(sz, nullptr) / 60000.0 * M_PI / 180.0;

	double alpha = 0.5;
	if (pAP->getProperty("frame-shadow-alpha", sz) && sz && *sz)
		alpha = s_abwnDouble(sz);
	if (alpha <= 0.0)
		return;
	if (alpha > 1.0)
		alpha = 1.0;

	unsigned int rv = 0, gv = 0, bv = 0;
	if (pAP->getProperty("frame-shadow-color", sz) && sz)
		sscanf(sz, "%02x%02x%02x", &rv, &gv, &bv);

	GR_CairoGraphics * pCG = dynamic_cast<GR_CairoGraphics *>(pG);
	if (!pCG)
		return;
	/* same implicit-beginPaint guard as the other cairo painters */
	if (pCG->getPaintCount() <= 0 &&
		pG->queryProperties(GR_Graphics::DGP_SCREEN))
		return;
	cairo_t * cr = pCG->getCairo();
	if (!cr)
		return;

	double distDev = pG->tdu(distLU);
	double blurDev = pG->tdu(blurLU);
	double ox = cos(dirRad) * distDev;
	double oy = sin(dirRad) * distDev;

	bool bRotWith = true;
	if (pAP->getProperty("frame-shadow-rot", sz) && sz &&
		(!strcmp(sz, "0") || !strcmp(sz, "false")))
		bRotWith = false;
	if (!bRotWith)
	{
		double rad = -pFC->getRotation() * M_PI / 180.0;
		double c = cos(rad), s = sin(rad);
		double ux = ox * c - oy * s;
		double uy = ox * s + oy * c;
		ox = pFC->isFlippedHoriz() ? -ux : ux;
		oy = pFC->isFlippedVert() ? -uy : uy;
	}

	double dx0 = pG->tdu(x), dy0 = pG->tdu(y);
	double dw = pG->tdu(x + w) - dx0, dh = pG->tdu(y + h) - dy0;
	if (dw <= 0.0 || dh <= 0.0)
		return;

	/* the blur spills ~3 radii past the silhouette - grow the damage
	 * rect so a repaint covers the soft edge */
	if (pFC->getPage())
	{
		UT_sint32 padLU = distLU + 3 * blurLU + pG->tlu(2);
		UT_Rect dmg;
		s_rotatedBounds(x - padLU, y - padLU,
						w + 2 * padLU, h + 2 * padLU,
						pFC->getRotation(), dmg);
		pFC->getPage()->expandDamageRect(dmg.left, dmg.top,
									   dmg.width, dmg.height);
	}

	double margin = blurDev * 3.0 + 2.0;
	int sw = static_cast<int>(ceil(dw + 2.0 * margin));
	int sh = static_cast<int>(ceil(dh + 2.0 * margin));
	if (sw <= 0 || sh <= 0 || sw > 8192 || sh > 8192)
		return;
	cairo_surface_t * mask =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sw, sh);
	cairo_t * mcr = cairo_create(mask);
	cairo_translate(mcr, margin, margin);
	cairo_new_path(mcr);
	bool bSilhouette = bHasPath &&
		s_frameShapePath(mcr, szPath, 0.0, 0.0, dw, dh);
	if (!bSilhouette)
		cairo_rectangle(mcr, 0.0, 0.0, dw, dh);
	cairo_set_source_rgba(mcr, 0.0, 0.0, 0.0, 1.0);
	cairo_fill(mcr);
	cairo_destroy(mcr);
	s_blurShadowAlpha(mask, static_cast<int>(blurDev + 0.5));

	cairo_set_source_rgba(cr,
						  rv / 255.0, gv / 255.0, bv / 255.0, alpha);
	cairo_mask_surface(cr, mask,
					   dx0 - margin + ox, dy0 - margin + oy);
	cairo_surface_destroy(mask);
	cairo_new_path(cr);
}

/*!
 * Bounding box of the possibly-rotated frame in page coordinates -
 * used for damage intersection so rotated frames are still painted.
 */
void fp_FrameContainer::getInkBounds(UT_Rect & r) const
{
	fp_FrameContainer * self = const_cast<fp_FrameContainer *>(this);
	s_rotatedBounds(self->getFullX(), self->getFullY(),
					self->getFullWidth(), self->getFullHeight(),
					self->getRotation(), r);
}

/*!
 * Maps a page-space point back through the frame's rotation so
 * hit-testing can use the unrotated rectangle.
 */
void fp_FrameContainer::unrotatePoint(UT_sint32 & x, UT_sint32 & y) const
{
	double deg = const_cast<fp_FrameContainer *>(this)->getRotation();
	if (deg == 0.0)
		return;
	double cx = getFullX() + getFullWidth() / 2.0;
	double cy = getFullY() + getFullHeight() / 2.0;
	double rad = -deg * M_PI / 180.0;
	double c = cos(rad), s = sin(rad);
	double dx = x - cx, dy = y - cy;
	x = static_cast<UT_sint32>(cx + dx * c - dy * s);
	y = static_cast<UT_sint32>(cy + dx * s + dy * c);
}
/*!
 * Returns true if the supplied screen rectangle overlaps with frame
 * container. This method takes account of transparening and tight wrapping.
 */
bool fp_FrameContainer::overlapsRect(const UT_Rect & rec)
{
     UT_Rect pMyFrameRec = getScreenRect().value();
     fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(getSectionLayout());
     UT_sint32 iextra = pFL->getBoundingSpace() -2;
     pMyFrameRec.left -= iextra;
     pMyFrameRec.top -= iextra;
     pMyFrameRec.width += 2 * iextra;
     pMyFrameRec.height += 2 * iextra;
     xxx_UT_DEBUGMSG(("look at rec.left %d top %d width %d  \n", rec.left, rec.top, rec.width));
     if(rec.intersectsRect(&pMyFrameRec))
     {
         if(!isTightWrapped())
	 {
	      return true;
	 }
	 UT_sint32 iTweak = getGraphics()->tlu(2);
	 pMyFrameRec.left += iextra + iTweak;
	 pMyFrameRec.top += iextra + iTweak;
	 pMyFrameRec.width -= (2 * iextra + 2 * iTweak);
	 pMyFrameRec.height -= (2 * iextra + 2 * iTweak);

	 UT_sint32 y = rec.top - pMyFrameRec.top;
	 UT_sint32 h = rec.height;
	 if(pFL->getBackgroundImage() == nullptr)
	 {
	      return true;
	 }
	 UT_sint32 pad = pFL->getBoundingSpace();
	 UT_sint32 iLeft = pFL->getBackgroundImage()->GetOffsetFromLeft(getGraphics(),pad,y,h);
	 xxx_UT_DEBUGMSG(("iLeft projection %d \n",iLeft));
	 if(iLeft < -getWidth())
	 {
	   //
	   // Pure transparent.
	   //
	   xxx_UT_DEBUGMSG(("Overlaps pure transparent line top %d line height %d image top %d \n",rec.top,rec.height,y));
	      return false;
	 }
	 xxx_UT_DEBUGMSG(("iLeft in overlapRect %d Y %d \n",iLeft,y));
	 if(rec.left < pMyFrameRec.left)
	 {
	      pMyFrameRec.left -= iLeft;
	      xxx_UT_DEBUGMSG(("Moves Image left border by %d to %d \n", -iLeft, pMyFrameRec.left));
	 }
	 else
	 {
	      UT_sint32 iRight = pFL->getBackgroundImage()->GetOffsetFromRight(getGraphics(),pad,y,h);
	      pMyFrameRec.width += iRight;
	      xxx_UT_DEBUGMSG(("Reduce Image width by %d to %d \n", iRight, pMyFrameRec.width));
	 }
	 if(rec.intersectsRect(&pMyFrameRec))
	 {
	   xxx_UT_DEBUGMSG(("Frame Still overlaps \n"));
	   return true;
	 }
	 xxx_UT_DEBUGMSG(("Tight Frame no longer overlaps \n"));
	 xxx_UT_DEBUGMSG(("Line Top %d Height %d left %d width %d \n",rec.top,rec.height,rec.left,rec.width));
	 xxx_UT_DEBUGMSG(("Image Top %d Height %d left %d width %d \n", pMyFrameRec.top, pMyFrameRec.height, pMyFrameRec.left, pMyFrameRec.width));
	 xxx_UT_DEBUGMSG(("Relative Top of line %d \n",y));
     }
     return false;
}

void fp_FrameContainer::setPreferedPageNo(UT_sint32 i)
{
     if(m_iPreferedPageNo == i)
       return;
     m_iPreferedPageNo =  i;
     fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(getSectionLayout());
     FL_DocLayout * pDL = pFL->getDocLayout();
     if(pDL->isLayoutFilling())
       return;
     PD_Document * pDoc = pDL->getDocument();
     UT_UTF8String sVal;
     UT_UTF8String_sprintf(sVal,"%d",i);
     const char * attr = PT_PROPS_ATTRIBUTE_NAME;
     UT_UTF8String sAttVal = "frame-pref-page:";
     sAttVal += sVal.utf8_str();
     
     pDoc->changeStruxAttsNoUpdate(pFL->getStruxDocHandle(),attr,sAttVal.utf8_str());
}

void fp_FrameContainer::setPreferedColumnNo(UT_sint32 i)
{
     if(m_iPreferedColumnNo == i)
       return;
     m_iPreferedColumnNo =  i;
     fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(getSectionLayout());
     FL_DocLayout * pDL = pFL->getDocLayout();
     if(pDL->isLayoutFilling())
       return;
     PD_Document * pDoc = pDL->getDocument();
     UT_UTF8String sVal;
     UT_UTF8String_sprintf(sVal,"%d",i);
     const char * attr = PT_PROPS_ATTRIBUTE_NAME;
     UT_UTF8String sAttVal = "frame-pref-column:";
     sAttVal += sVal.utf8_str();
     
     pDoc->changeStruxAttsNoUpdate(pFL->getStruxDocHandle(),attr,sAttVal.utf8_str());
}

/*!
 * This method returns the padding to be applied between a line approaching
 * wrapped frame or image from the left. 
 * y Is the top of the line in logical units as defined relative to the
 * y position on the screen.
 * height is the height of the line.
 * If tight wrapping is set on a positioned object this number can be negative
 * which means the line can encroach into the rectangular region of the
 * image provided the region is transparent.
 */
UT_sint32 fp_FrameContainer::getLeftPad(UT_sint32 y, UT_sint32 height) const
{
  auto pFL = static_cast<const fl_FrameLayout *>(getSectionLayout());
  UT_sint32 pad = pFL->getBoundingSpace();	
  UT_Rect pRect = getScreenRect().value();
  UT_sint32 yC = pRect.top;
  if(!isTightWrapped() || !isWrappingSet())
  {
    return pad;
  }
  if(FL_FRAME_TEXTBOX_TYPE == pFL->getFrameType())
  {
    return pad;
  }
  if(pFL->getBackgroundImage() == nullptr)
  {
    return pad;
  }
  UT_sint32 iLeft = pFL->getBackgroundImage()->GetOffsetFromLeft(getGraphics(),pad,y - yC,height);
  xxx_UT_DEBUGMSG(("Local Y %d iLeft %d width %d \n",y-yC,iLeft,getFullWidth()));  return iLeft;
}


/*!
 * This method returns the padding to be applied between a line approaching
 * wrapped frame or image from the right. 
 * y Is the top of the line in logical units as defined relative to the
 * y position on the screen.
 * height is the height of the line.
 * If tight wrapping is set on a positioned object this number can be negative
 * which means the line can encroach into the rectangular region of the
 * image provided the region is transparent.
 */
UT_sint32 fp_FrameContainer::getRightPad(UT_sint32 y, UT_sint32 height) const
{
  auto pFL = static_cast<const fl_FrameLayout *>(getSectionLayout());
  UT_sint32 pad = pFL->getBoundingSpace();
  UT_Rect pRect = getScreenRect().value();
  UT_sint32 yC = pRect.top;
  if(!isTightWrapped() || !isWrappingSet())
  {
    return pad;
  }
  if(FL_FRAME_TEXTBOX_TYPE == pFL->getFrameType())
  {
    return pad;
  }
  if(pFL->getBackgroundImage() == nullptr)
  {
    return pad;
  }
  UT_sint32 iRight = pFL->getBackgroundImage()->GetOffsetFromRight(getGraphics(),pad,y - yC,height);
  xxx_UT_DEBUGMSG(("Local Y %d iRight %d width %d \n",y-yC,iRight,getFullWidth()));
  return iRight;
}

void fp_FrameContainer::clearScreen(void)
{
	fp_Page * pPage = getPage();
	if(pPage == nullptr)
	{
		return;
	}
	if(getView() == nullptr)
	{
		return;
	}

	UT_sint32 srcX,srcY;
	UT_sint32 xoff,yoff;
	getView()->getPageScreenOffsets(pPage,xoff,yoff);
	xxx_UT_DEBUGMSG(("pagescreenoffsets xoff %d yoff %d \n",xoff,yoff));
	UT_sint32 leftThick = m_lineLeft.m_thickness;
	UT_sint32 rightThick = m_lineRight.m_thickness;
	UT_sint32 topThick = m_lineTop.m_thickness;
	UT_sint32 botThick = m_lineBottom.m_thickness;

	srcX = getFullX() - leftThick;
	srcY = getFullY() - topThick;

	xoff += getFullX() - leftThick;
	yoff += getFullY() - topThick;
	getFillType().getParent()->Fill(getGraphics(),srcX,srcY,xoff,yoff,getFullWidth()+leftThick+rightThick,getFullHeight()+topThick+botThick+getGraphics()->tlu(1) +1);
	fp_Container * pCon = nullptr;
	UT_sint32 i = 0;
	for(i=0; i< countCons(); i++)
	{
		pCon = static_cast<fp_Container *>(getNthCon(i));
		pCon->clearScreen();
	}
	m_bNeverDrawn = true;
}

/*!
 * All these methods are used to implement an X and Y padding around the
 * Frame
 */
UT_sint32 fp_FrameContainer::getFullWidth(void) const
{
	return fp_VerticalContainer::getWidth();
}

UT_sint32 fp_FrameContainer::getFullHeight(void) const
{
	return fp_VerticalContainer::getHeight();
}

UT_sint32 fp_FrameContainer::getFullX(void) const
{
	return fp_VerticalContainer::getX();
}

UT_sint32 fp_FrameContainer::getFullY(void) const
{
	return fp_VerticalContainer::getY();
}


UT_sint32 fp_FrameContainer::getWidth(void) const
{
	/* "frame-text-direction": vertical text is laid out in a logical
	 * space rotated 90 degrees against the box - the logical line
	 * width is the box's inner height and the line stack advances
	 * across the inner width.  Everything that queries the content
	 * area (line breaking, justification, overflow) sees the swapped
	 * extents; the physical box stays in getFullWidth/getFullHeight. */
	if (getTextRotation() != 0)
		return fp_VerticalContainer::getHeight()
			- m_iYpadTop - m_iYpadBottom;
	return fp_VerticalContainer::getWidth()
		- m_iXpadLeft - m_iXpadRight;
}

UT_sint32 fp_FrameContainer::getX(void) const
{
	UT_sint32 iX = fp_VerticalContainer::getX() + m_iXpadLeft;
	return iX;
}


UT_sint32 fp_FrameContainer::getY(void) const
{
	UT_sint32 iY = fp_VerticalContainer::getY() + m_iYpadTop;
	return iY;
}

UT_sint32 fp_FrameContainer::getHeight(void) const
{
	if (getTextRotation() != 0)
		return fp_VerticalContainer::getWidth()
			- m_iXpadLeft - m_iXpadRight;
	return fp_VerticalContainer::getHeight()
		- m_iYpadTop - m_iYpadBottom;
}

	
void fp_FrameContainer::setContainer(fp_Container * /*pContainer*/)
{
	UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
}

fl_DocSectionLayout * fp_FrameContainer::getDocSectionLayout(void) const
{
	auto pFL = static_cast<const fl_FrameLayout *>(getSectionLayout());
	auto pDSL = static_cast<fl_DocSectionLayout *>(pFL->getSectionLayout());
	UT_ASSERT(pDSL && (pDSL->getContainerType() == FL_CONTAINER_DOCSECTION));
	return pDSL;
}

/*!
 * Fill the supplied vector with a list of the blocks whose lines are affected
 * by the Frame.
 */ 
void fp_FrameContainer::getBlocksAroundFrame(UT_GenericVector<fl_BlockLayout *> & vecBlocks)
{
  fp_Page * pPage = getPage();
  if(pPage == nullptr)
  {
    return;
  }
  UT_sint32 iColLeader = 0;
  fp_Column * pCol = nullptr;
  fl_BlockLayout * pCurBlock = nullptr;
  fp_Line * pCurLine = nullptr;
  fp_Container * pCurCon = nullptr;
  if(pPage->countColumnLeaders() == 0)
  {
      UT_sint32 iPage = getPreferedPageNo();
      if(iPage >0)
          setPreferedPageNo(iPage-1);
      return;
  }
  for(iColLeader = 0; iColLeader < pPage->countColumnLeaders(); iColLeader++)
  {
      pCol = pPage->getNthColumnLeader(iColLeader);
      while(pCol)
      {
          UT_sint32 i = 0;
          UT_sint32 iYCol = pCol->getY(); // Vertical position relative to page.
          for(i=0; i< pCol->countCons(); i++)
          {
              pCurCon = static_cast<fp_Container *>(pCol->getNthCon(i));
              if(pCurCon->getContainerType() == FP_CONTAINER_LINE)
              {
                  pCurLine = static_cast<fp_Line *>(pCurCon);
                  UT_sint32 iYLine = iYCol + pCurLine->getY();
                  xxx_UT_DEBUGMSG(("iYLine %d FullY %d FullHeight %d \n",iYLine,getFullY(),getFullHeight()));
                  if((iYLine + pCurLine->getHeight() > getFullY()) && (iYLine < (getFullY() + getFullHeight())))
                  {
                      //
                      // Line overlaps frame in Height. Add it's block to the vector.
                      //
                      if(pCurLine->getBlock() != pCurBlock)
                      {
                          pCurBlock = pCurLine->getBlock();
                          vecBlocks.addItem(pCurBlock);
                          xxx_UT_DEBUGMSG(("Add Block %x to vector \n",pCurBlock));
                      }
                  }
              }
          }
          pCol = pCol->getFollower();
      }
  }
  if(vecBlocks.getItemCount() == 0)
  {
      pCol = pPage->getNthColumnLeader(0);
      fp_Container * pCon = pCol->getFirstContainer();
      fl_BlockLayout * pB = nullptr;
      if(pCon && pCon->getContainerType() == FP_CONTAINER_LINE)
      {
          pB = static_cast<fp_Line *>(pCon)->getBlock();
      }
      else if(pCon)
      {
          fl_ContainerLayout * pCL = static_cast<fl_ContainerLayout *>(pCon->getSectionLayout());
          pB = pCL->getNextBlockInDocument();
      }
      if(pB != nullptr)
          vecBlocks.addItem(pB);
  }

}

/* just a little helper function
 */
void fp_FrameContainer::_drawLine (const PP_PropertyMap::Line & style,
								  UT_sint32 left, UT_sint32 top, UT_sint32 right, UT_sint32 bot,GR_Graphics * pGr)
{
	GR_Painter painter(pGr);

	if (style.m_t_linestyle == PP_PropertyMap::linestyle_none)
		return; // do not draw	
	
	GR_Graphics::JoinStyle js = GR_Graphics::JOIN_MITER;
	GR_Graphics::CapStyle  cs = GR_Graphics::CAP_PROJECTING;

	UT_sint32 iLineWidth = static_cast<UT_sint32>(style.m_thickness);
	pGr->setLineWidth (iLineWidth);
	pGr->setColor (style.m_color);

	switch (style.m_t_linestyle)
	{
		case PP_PropertyMap::linestyle_dotted:
			pGr->setLineProperties (iLineWidth, js, cs, GR_Graphics::LINE_DOTTED);
			break;
		case PP_PropertyMap::linestyle_dashed:
			pGr->setLineProperties (iLineWidth, js, cs, GR_Graphics::LINE_ON_OFF_DASH);
			break;
		case PP_PropertyMap::linestyle_dashdot:
			pGr->setLineProperties (iLineWidth, js, cs, GR_Graphics::LINE_DASH_DOT);
			break;
		case PP_PropertyMap::linestyle_dashdotdot:
			pGr->setLineProperties (iLineWidth, js, cs, GR_Graphics::LINE_DASH_DOT_DOT);
			break;
		case PP_PropertyMap::linestyle_longdash:
			pGr->setLineProperties (iLineWidth, js, cs, GR_Graphics::LINE_LONG_DASH);
			break;
		case PP_PropertyMap::linestyle_solid:
		default:
			pGr->setLineProperties (iLineWidth, js, cs, GR_Graphics::LINE_SOLID);
			break;
	}

	xxx_UT_DEBUGMSG(("_drawLine: top %d bot %d \n",top,bot));

	painter.drawLine (left, top, right, bot);
	
	pGr->setLineProperties (pGr->tlu(1), js, cs, GR_Graphics::LINE_SOLID);
}

/*!
 * Draw the frame boundaries
 */
void  fp_FrameContainer::drawBoundaries(dg_DrawArgs * pDA)
{
	UT_sint32 iXlow = pDA->xoff - m_iXpadLeft;
	UT_sint32 iXhigh = iXlow + getFullWidth() ;
	UT_sint32 iYlow = pDA->yoff - m_iYpadTop;
	UT_sint32 iYhigh = iYlow + getFullHeight();
	GR_Graphics * pG = pDA->pG;
	if(getPage())
	{
		getPage()->expandDamageRect(iXlow,iYlow,getFullWidth(),getFullHeight());

		//
		// Only fill to the bottom of the viewed page.
		//
		UT_sint32 iFullHeight = getFullHeight();
		fl_DocSectionLayout * pDSL = getDocSectionLayout();
		UT_sint32 iMaxHeight = 0;
		if(!pG->queryProperties(GR_Graphics::DGP_PAPER) && (getView()->getViewMode() != VIEW_PRINT))
		{
		        iMaxHeight = pDSL->getActualColumnHeight();
		}
		else
		{
		        iMaxHeight = getPage()->getHeight();
		}
		UT_sint32 iBot = getFullY()+iFullHeight;
		if(iBot > iMaxHeight)
		{
		        iFullHeight = iFullHeight - (iBot-iMaxHeight);
			iYhigh = iFullHeight;
		}
	}
	/* custGeom freeforms stroke their actual path in s_paintFrameShape;
	 * drawing the rectangular border too would add a phantom box */
	{
		fl_FrameLayout * pFLb = static_cast<fl_FrameLayout *>(getSectionLayout());
		const PP_AttrProp * pAPb = nullptr;
		const gchar * szPath = nullptr;
		if (pFLb)
			pFLb->getAP(pAPb);
		if (pAPb && pAPb->getProperty("shape-path", szPath) &&
			szPath && *szPath)
			return;
	}
	_drawLine(m_lineTop,iXlow,iYlow,iXhigh,iYlow,pDA->pG); // top
	_drawLine(m_lineRight,iXhigh,iYlow,iXhigh,iYhigh,pDA->pG); // right
	_drawLine(m_lineBottom,iXlow,iYhigh,iXhigh,iYhigh,pDA->pG); // bottom
	_drawLine(m_lineLeft,iXlow,iYlow,iXlow,iYhigh,pDA->pG); // left
}


/*!
 * Draw the frame handles
 */
void  fp_FrameContainer::drawHandles(dg_DrawArgs * pDA)
{
	if(getView() == nullptr)
	{
	     getSectionLayout()->format();
	     getSectionLayout()->setNeedsReformat(getSectionLayout());
	}
	if(getView() == nullptr)
	{
	     return;
	}
	if(!getPage())
	{
	     return;
	}
	//
	// Only fill to the bottom of the viewed page.
	//
	GR_Graphics * pG = pDA->pG;
	UT_sint32 iFullHeight = getFullHeight();
	fl_DocSectionLayout * pDSL = getDocSectionLayout();
	UT_sint32 iMaxHeight = 0;
	if(!pG->queryProperties(GR_Graphics::DGP_PAPER) && (getView()->getViewMode() != VIEW_PRINT))
	{
	    iMaxHeight = pDSL->getActualColumnHeight();
	}
	else
	{
	    iMaxHeight = getPage()->getHeight();
	}
	UT_sint32 iBot = getFullY()+iFullHeight;
	if(iBot > iMaxHeight)
	{
	    iFullHeight = iFullHeight - (iBot-iMaxHeight);
	}
	UT_sint32 iXlow = pDA->xoff - m_iXpadLeft;
	UT_sint32 iYlow = pDA->yoff - m_iYpadTop;

	UT_Rect box(iXlow + pDA->pG->tlu(2), iYlow + pDA->pG->tlu(2), getFullWidth() - pDA->pG->tlu(4), iFullHeight - pDA->pG->tlu(4));
	UT_Rect inkBox;
	s_rotatedBounds(iXlow, iYlow, getFullWidth(), getFullHeight(),
					getRotation(), inkBox);
	getPage()->expandDamageRect(inkBox.left, inkBox.top,
								inkBox.width, inkBox.height);
	/* draw the handles under the same transform as the frame so the
	 * selection outline follows the rotated object */
	cairo_t * cr = nullptr;
	double rot = getRotation();
	bool bFlipH = isFlippedHoriz(), bFlipV = isFlippedVert();
	if (rot != 0.0 || bFlipH || bFlipV)
	{
		GR_CairoGraphics * pCG = dynamic_cast<GR_CairoGraphics *>(pG);
		/* getCairo() calls beginPaint() when no paint is running -
		 * doing that outside a real draw unbalances the paint/group
		 * stack and blanks the canvas on screen, so only transform
		 * mid-paint; on the print/PDF graphics _beginPaint is a no-op
		 * counter and the implicit begin is harmless */
		if (pCG && (pCG->getPaintCount() > 0 ||
					!pG->queryProperties(GR_Graphics::DGP_SCREEN)))
			cr = pCG->getCairo();
	}
	if (cr)
	{
		double dcx = pG->tdu(iXlow + getFullWidth() / 2);
		double dcy = pG->tdu(iYlow + getFullHeight() / 2);
		cairo_save(cr);
		cairo_translate(cr, dcx, dcy);
		cairo_rotate(cr, rot * M_PI / 180.0);
		cairo_scale(cr, bFlipH ? -1.0 : 1.0, bFlipV ? -1.0 : 1.0);
		cairo_translate(cr, -dcx, -dcy);
	}
	getView()->drawSelectionBox(box, true);
	if (cr)
		cairo_restore(cr);
}

/*!
 Draw container content
 \param pDA Draw arguments
 */
void fp_FrameContainer::draw(dg_DrawArgs* pDA)
{
	FV_View * pView = getView();
	UT_return_if_fail( pView);
	
	xxx_UT_DEBUGMSG(("FrameContainer %x called, page %x \n",this,getPage()));
	if(getPage() == nullptr)
	{
	     getSectionLayout()->format();
	     getSectionLayout()->setNeedsReformat(getSectionLayout());
	     if(getPage() == nullptr)
	     {
			 return;
	     }
	}
	if(pView)
	{
		if(pView->getFrameEdit()->getFrameEditMode() == FV_FrameEdit_DRAG_EXISTING)
		{
			if((pView->getFrameEdit()->getFrameContainer() == this))
			{
				return;
			}
		}
	}
//
// Only draw the lines in the clipping region.
//
/*
	[Somewhere down here is where the logic to only draw the region of the frame which
	is within the complement of the union of all higher frames needs to be. We need to
	draw the applicable region of the rectangle we're on, then unify it with (if
	applicable) the higher union.] <-- Possibly obsolete comment, not sure.
	I think I might have landed on an alternative solution involving more rearranging
	of the storage of the FrameContainers, based on their z-index.  Not sure how far
	I got with that or if it worked either.  See also abi bug 7664 and the original
	discussions about defining the undefinedness of layered frame behaviour.
*/

	if(m_bOverWrote)
	{
		pDA->bDirtyRunsOnly = false;
	}
	dg_DrawArgs da = *pDA;
	GR_Graphics * pG = da.pG;
	UT_return_if_fail( pG);

	UT_sint32 x = pDA->xoff - m_iXpadLeft;
	UT_sint32 y = pDA->yoff - m_iYpadTop;
	UT_Rect inkBounds;
	s_rotatedBounds(x, y, getFullWidth(), getFullHeight(),
					getRotation(), inkBounds);
	getPage()->expandDamageRect(inkBounds.left, inkBounds.top,
								inkBounds.width, inkBounds.height);
	/* "frame-rotation" / "frame-flip-*": rotate the cairo context
	 * around the frame centre so everything painted below (fill,
	 * content, borders) lands transformed.  The clip set while
	 * transformed becomes the rotated frame outline, and the final
	 * cairo_restore pops it again so the caller's clip state is
	 * unchanged. */
	cairo_t * cr = nullptr;
	double rot = getRotation();
	bool bFlipH = isFlippedHoriz(), bFlipV = isFlippedVert();
	if (rot != 0.0 || bFlipH || bFlipV)
	{
		GR_CairoGraphics * pCG = dynamic_cast<GR_CairoGraphics *>(pG);
		/* guard against getCairo()'s implicit beginPaint() when this
		 * draw runs outside a paint cycle (frame-edit redraw) - an
		 * unbalanced beginPaint corrupts the canvas group stack on
		 * screen; the print graphics' beginPaint is a no-op counter */
		if (pCG && (pCG->getPaintCount() > 0 ||
					!pG->queryProperties(GR_Graphics::DGP_SCREEN)))
			cr = pCG->getCairo();
	}
	if (cr)
	{
		double dcx = pG->tdu(x + getFullWidth() / 2);
		double dcy = pG->tdu(y + getFullHeight() / 2);
		cairo_save(cr);
		cairo_translate(cr, dcx, dcy);
		cairo_rotate(cr, rot * M_PI / 180.0);
		cairo_scale(cr, bFlipH ? -1.0 : 1.0, bFlipV ? -1.0 : 1.0);
		cairo_translate(cr, -dcx, -dcy);
	}
	if(!pDA->bDirtyRunsOnly || m_bNeverDrawn)
	{
		if(m_bNeverDrawn)
		{
			pDA->bDirtyRunsOnly= false;
		} 
		UT_sint32 srcX,srcY;
		getSectionLayout()->checkGraphicTick(pG);
		srcX = -m_iXpadLeft;
		srcY = -m_iYpadTop;
		//
		// Only fill to the bottom of the viewed page.
		//
		UT_sint32 iFullHeight = getFullHeight();
		fl_DocSectionLayout * pDSL = getDocSectionLayout();
		UT_sint32 iMaxHeight = 0;
		if(!pG->queryProperties(GR_Graphics::DGP_PAPER) && (pView->getViewMode() != VIEW_PRINT))
		{
		        iMaxHeight = pDSL->getActualColumnHeight();
		}
		else
		{
		        iMaxHeight = getPage()->getHeight();
		}
		UT_sint32 iBot = getFullY()+iFullHeight;
		if(iBot > iMaxHeight)
		{
		        iFullHeight = iFullHeight - (iBot-iMaxHeight);
		}
		/* a:outerShdw drop shadow paints under the frame transform,
		 * before the fill - Word draws it beneath the shape */
		s_paintFrameShadow(pG, this, x, y, getFullWidth(), iFullHeight);
		if (!s_paintFrameShape(pG, this, x, y,
							   getFullWidth(), iFullHeight) &&
			!s_paintFrameGradient(pG, this, x, y,
								  getFullWidth(), iFullHeight) &&
			!s_paintFrameAlpha(pG, this, x, y,
							   getFullWidth(), iFullHeight))
		{
			getFillType().Fill(pG,srcX,srcY,x,y,getFullWidth(),iFullHeight);
		}
		m_bNeverDrawn = false;
	}
	UT_uint32 count = countCons();
	xxx_UT_DEBUGMSG(("Number of containers in frame %d \n",count));
	auto p = pDA->pG->getClipRect();
	std::unique_ptr<UT_Rect> pPrevRect(p ? new UT_Rect(*p) : nullptr);
	UT_Rect pRect = getScreenRect().value();
	UT_Rect newRect;
	bool bRemoveRectAfter = false;
	bool bSetOrigClip = false;
	bool bSkip = false;
	/* "frame-text-direction": vertical text is laid out in a logical
	 * space with swapped extents and rotated into the box below.
	 * The GR clip applies lazily under that rotation, so the stored
	 * rect has to be inverse-rotated into the logical space - it then
	 * lands on the physical box.  The skip decision keeps using the
	 * physical rect. */
	int iTextRot = getTextRotation();
	cairo_t * crT = nullptr;
	if (iTextRot != 0)
	{
		GR_CairoGraphics * pCG = dynamic_cast<GR_CairoGraphics *>(pG);
		/* same implicit-beginPaint guard as the frame rotation */
		if (pCG && (pCG->getPaintCount() > 0 ||
					!pG->queryProperties(GR_Graphics::DGP_SCREEN)))
			crT = pCG->getCairo();
		else
			iTextRot = 0;
	}
	UT_sint32 iInnerW = getFullWidth() - m_iXpadLeft - m_iXpadRight;
	UT_sint32 iInnerH = getFullHeight() - m_iYpadTop - m_iYpadBottom;
	if((pPrevRect == nullptr) && pG->queryProperties(GR_Graphics::DGP_SCREEN))
	{
		UT_Rect rClip = pRect;
		if (iTextRot)
			s_unrotateFrameRect(rClip, pDA->xoff, pDA->yoff,
								iInnerW, iInnerH, iTextRot);
		pDA->pG->setClipRect(&rClip);
		UT_DEBUGMSG(("Clip bottom is %d \n", pRect.top + pRect.height));
		bRemoveRectAfter = true;
	}
	else if(pPrevRect && !pRect.intersectsRect(pPrevRect.get()))
	{
		bSkip = true;
		xxx_UT_DEBUGMSG(("External Clip bottom is %d \n", pRect.top + pRect.height));
	}
	else if(pPrevRect)
	{
		// Clip the frame's contents to its own rectangle on BOTH axes -
		// the old code only clamped top/bottom, so text that ran past
		// the left or right border painted outside the box.
		newRect.top = UT_MAX(pPrevRect->top, pRect.top);
		UT_sint32 iBotPrev = pPrevRect->height + pPrevRect->top;
		UT_sint32 iBot = pRect.height + pRect.top;
		newRect.height = UT_MIN(iBotPrev,iBot) - newRect.top;
		newRect.left = UT_MAX(pPrevRect->left, pRect.left);
		UT_sint32 iRightPrev = pPrevRect->left + pPrevRect->width;
		UT_sint32 iRight = pRect.left + pRect.width;
		newRect.width = UT_MIN(iRightPrev,iRight) - newRect.left;
		if((newRect.height > 0) && (newRect.width > 0) &&
		   pDA->pG->queryProperties(GR_Graphics::DGP_SCREEN))
		{
			if (iTextRot)
				s_unrotateFrameRect(newRect, pDA->xoff, pDA->yoff,
									iInnerW, iInnerH, iTextRot);
			pDA->pG->setClipRect(&newRect);
			bSetOrigClip = true;
		}
		else
		{
			bSkip = true;
		}
	}
	if(!bSkip)
	{
		if (crT)
		{
			/* rotate the logical content space onto the box:
			 * vert(90) maps the line axis onto +y (text runs down)
			 * and the stack axis onto -x (lines go right to left);
			 * vert270 mirrors that - text runs up, lines go left
			 * to right */
			double ox = pG->tdu(pDA->xoff), oy = pG->tdu(pDA->yoff);
			cairo_save(crT);
			if (iTextRot == 90)
				cairo_translate(crT, pG->tdu(pDA->xoff + iInnerW), oy);
			else
				cairo_translate(crT, ox, pG->tdu(pDA->yoff + iInnerH));
			cairo_rotate(crT,
						 (iTextRot == 90 ? 1.0 : -1.0) * M_PI_2);
			cairo_translate(crT, -ox, -oy);
		}
		for (UT_uint32 i = 0; i<count; i++)
		{
			fp_ContainerObject* pContainer = static_cast<fp_ContainerObject*>(getNthCon(i));
			da.xoff = pDA->xoff + pContainer->getX();
			da.yoff = pDA->yoff + pContainer->getY();
			pContainer->draw(&da);
		}
		if (crT)
		{
			cairo_restore(crT);
		}
	}
	m_bNeverDrawn = false;
	m_bOverWrote = false;
	if(bRemoveRectAfter)
	{
		pDA->pG->setClipRect(nullptr);
	}
	if(bSetOrigClip)
	{
		pDA->pG->setClipRect(pPrevRect.get());
	}
	drawBoundaries(pDA);
	/* OOXML head/tail line ends decorate the bar frames that stand
	 * in for prstGeom="line" shapes - painted under the same
	 * rotation/flip transform as the rest of the frame */
	s_paintFrameArrows(pG, this, x, y, getFullWidth(), getFullHeight());
	if (cr)
	{
		cairo_restore(cr);
	}
}

void fp_FrameContainer::setBackground (const PP_PropertyMap::Background & style)
{
	m_background = style;
	PP_PropertyMap::Background background = m_background;
	if(background.m_t_background == PP_PropertyMap::background_solid)
	{
		getFillType().setColor(background.m_color);
	}
}


/*!
 * FrameContainers are not in the linked list of physical containers
 */
fp_Container * fp_FrameContainer::getNextContainerInSection() const
{
	return nullptr;
}

/*!
 * FrameContainers are not in the linked list of physical containers
 */
fp_Container * fp_FrameContainer::getPrevContainerInSection() const
{
	return nullptr;
}

void fp_FrameContainer::layout(void)
{
	_setMaxContainerHeight(0);
	UT_sint32 iY = 0, iPrevY = 0;
	/* "frame-valign": vertical alignment of the content inside the
	 * padded box (Word's v:textAnchor / wps:bodyPr@anchor).  The
	 * children are stacked from iY relative to the content origin, so
	 * center/bottom just start the stack at an offset when the total
	 * content height is smaller than the inner box height. */
	UT_sint32 iYOffset = 0;
	UT_sint32 iContentH = 0;
	for (UT_uint32 iH = 0; iH < countCons(); iH++)
	{
		fp_Container * pHC = static_cast<fp_Container*>(getNthCon(iH));
		iContentH += pHC->getHeight() + pHC->getMarginAfter();
	}
	if (iContentH < getHeight())
	{
		const gchar * szValign = nullptr;
		fl_FrameLayout * pFLv = static_cast<fl_FrameLayout *>(getSectionLayout());
		const PP_AttrProp * pAPv = nullptr;
		if (pFLv)
			pFLv->getAP(pAPv);
		if (pAPv && pAPv->getProperty("frame-valign", szValign) && szValign)
		{
			if (strcmp(szValign, "center") == 0 || strcmp(szValign, "middle") == 0)
				iYOffset = (getHeight() - iContentH) / 2;
			else if (strcmp(szValign, "bottom") == 0)
				iYOffset = getHeight() - iContentH;
		}
	}
	iY = iYOffset;
	UT_uint32 iCountContainers = countCons();
	fp_Container *pContainer, *pPrevContainer = nullptr;
	for (UT_uint32 i=0; i < iCountContainers; i++)
	{
		pContainer = static_cast<fp_Container*>(getNthCon(i));
//
// This is to speedup redraws.
//
		if(pContainer->getHeight() > _getMaxContainerHeight())
			_setMaxContainerHeight(pContainer->getHeight());

		if(pContainer->getY() != iY)
		{
			pContainer->clearScreen();
		}
		if(iY > getHeight())
		{
			pContainer->setY(-1000000);
		}
		else
		{
			pContainer->setY(iY);
		}
		UT_sint32 iContainerHeight = pContainer->getHeight();
		UT_sint32 iContainerMarginAfter = pContainer->getMarginAfter();
		if(pContainer->getContainerType() == FP_CONTAINER_TABLE)
		{
			fp_TableContainer * pTab = static_cast<fp_TableContainer *>(pContainer);
			iContainerHeight = pTab->getHeight();
			if(!pTab->isThisBroken() && (pTab->getFirstBrokenTable() == nullptr))
			{
				/*fp_Container * pBroke = static_cast<fp_Container *> */(pTab->VBreakAt(0));
			}
		}

		iY += iContainerHeight;
		iY += iContainerMarginAfter;
		//iY +=  0.5;

		if (pPrevContainer)
		{
			pPrevContainer->setAssignedScreenHeight(iY - iPrevY);
		}
		pPrevContainer = pContainer;
		iPrevY = iY;
	}

	// Correct height position of the last line
	if (pPrevContainer)
	{
		if(iY > getHeight())
		{
			pPrevContainer->setAssignedScreenHeight(-1000000);
		}
		else
		{
			pPrevContainer->setAssignedScreenHeight(iY - iPrevY + 1);
		}
	}
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(getSectionLayout());
	if(pFL->expandHeight())
	{
		if (getTextRotation() != 0)
		{
			/* vertical text: spAutoFit grows the box along the
			 * stacking axis - its width; the specified inner width
			 * acts as the minimum */
			if (iY > getHeight())
				setWidth(iY + m_iXpadLeft + m_iXpadRight);
		}
		else if (iY > pFL->minHeight())
		{
		     setHeight(iY+m_iYpadTop+m_iYpadBottom);
		}
	}
}

void fp_FrameContainer::setWidth(UT_sint32 iW)
{
        if(iW != getFullWidth())
	{
	     clearScreen();
	     fp_VerticalContainer::setWidth(iW);
	     fp_Page * pPage = getPage();
	     getDocSectionLayout()->setNeedsSectionBreak(true,pPage);
	}
}

void fp_FrameContainer::setHeight(UT_sint32 iY)
{
        if(iY != getFullHeight())
	{
	     xxx_UT_DEBUGMSG((" SetHeight Frame iY %d Fullheight %d Height %d \n",iY,getFullHeight(),getHeight()));
	     clearScreen();
	     fp_VerticalContainer::setHeight(iY);
	     fp_Page * pPage = getPage();
	     getDocSectionLayout()->setNeedsSectionBreak(true,pPage);
	}
}

/*!
 * Map a point inside the frame to a document position.  Vertical
 * text first un-rotates the point into the logical layout space the
 * children were laid out in - the runs know nothing about the
 * rotation.  (x,y) are relative to the inner box origin.
 */
void fp_FrameContainer::mapXYToPosition(UT_sint32 x, UT_sint32 y,
										PT_DocPosition& pos, bool& bBOL,
										bool& bEOL, bool & isTOC)
{
	int rot = getTextRotation();
	if (rot != 0)
	{
		UT_sint32 iw = getFullWidth() - m_iXpadLeft - m_iXpadRight;
		UT_sint32 ih = getFullHeight() - m_iYpadTop - m_iYpadBottom;
		UT_sint32 lx, ly;
		if (rot == 90)
		{
			lx = y;
			ly = iw - x;
		}
		else
		{
			lx = ih - y;
			ly = x;
		}
		fp_VerticalContainer::mapXYToPosition(lx, ly, pos,
										  bBOL, bEOL, isTOC);
		return;
	}
	fp_VerticalContainer::mapXYToPosition(x, y, pos, bBOL, bEOL, isTOC);
}
