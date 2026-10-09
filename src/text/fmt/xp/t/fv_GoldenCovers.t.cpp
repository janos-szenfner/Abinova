/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* Abinova
 * Copyright (C) 2026 Abinova Contributors
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

/*
 * Golden-image render regression suite (TST06).
 *
 * Renders every cover-page preset through the normal FV_View/fp_Page
 * paint path on a widget-less DGP_SCREEN graphics, then diffs the
 * raster against committed reference PNGs in test/wp/tst06/goldens/.
 * A render regression — a missing shape, a lost fill, a stray opaque
 * overlay like the COVER04 seal, wrong colours or broken geometry —
 * shifts enough pixels to fail the diff; a wrong-but-different render
 * is verified to exceed the tolerance.
 *
 * The Word-matching half imports test/wp/tst05/cover-*.docx and
 * compares each imported page 0 against the matching preset render
 * with a generous, downscaled tolerance: fonts and exact coordinates
 * differ by construction, so only the gross composition must agree.
 * That half skips when COVER_FIXTURES_DIR is unset (the corpus is not
 * shipped in dist tarballs).
 *
 * Reference PNGs regenerate in-place with ABINOVA_REGEN_GOLDENS=1;
 * a missing golden without the flag fails loudly, never silently
 * blesses the current render.
 */

#include "tf_test.h"

#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "fl_DocLayout.h"
#include "fl_SectionLayout.h"
#include "fv_View.h"
#include "fp_Page.h"
#include "fp_types.h"
#include "gr_DrawArgs.h"
#include "gr_Painter.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "ie_types.h"

#include <cairo.h>
#include <glib.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define TFSUITE "core.text.fmt.goldencovers"

namespace {

const char * const s_presets[] = {
	"frame", "motion", "sideline", "yearly",
	"austin", "badge", "banded", "crop", "facet", "feathered",
	"filgree", "headiness", "integral", "ion-dark", "ion-light",
	"retrospect", "semaphore", "slice-dark", "slice-light",
	"viewmaster", "whip"
};

struct FidelityPair { const char *preset; const char *file; double tol; };
/* tst05 cover corpus (FIXTURES.md names) -> preset it approximates;
 * frame/motion/sideline/yearly have no corpus counterpart.
 * per-pair tolerance = ~2x the measured 128x166-tile composition diff;
 * loose on integral (photographic asset vs flat vector approximation)
 * and crop/ion-dark (deliberately simplified block geometry) */
const FidelityPair s_pairs[] = {
	{ "austin",     "Austin.docx",           0.08 },
	{ "badge",      "Badge.docx",            0.10 },
	{ "banded",     "banded.docx",           0.08 },
	{ "crop",       "Crop.docx",             0.35 },
	{ "facet",      "facet.docx",            0.20 },
	{ "feathered",  "feathered.docx",        0.22 },
	{ "filgree",    "filgree.docx",          0.10 },
	{ "headiness",  "headiness.docx",        0.10 },
	{ "integral",   "integral.docx",         0.50 },
	{ "ion-dark",   "ion-dark.docx",         0.35 },
	{ "ion-light",  "ion-light.docx",        0.08 },
	{ "retrospect", "retrospect.docx",       0.28 },
	{ "semaphore",  "semaphore.docx",        0.08 },
	{ "slice-dark", "slice-dark.docx",       0.08 },
	{ "slice-light","slice-light.docx",      0.08 },
	{ "viewmaster", "viewmaster.docx",       0.08 },
	{ "whip",       "whip.docx",             0.08 },
};

/* render at half resolution: small enough to keep committed goldens
 * tiny, large enough that a lost band/panel/swoosh moves many pixels */
const int GOLD_ZOOM = 50;

/* a same-pipeline render should agree almost perfectly; the slack
 * covers the @date text ("Month YYYY") in whip/semaphore/ion whose
 * glyphs drift monthly, plus AA noise */
const double GOLD_FRAC_TOL = 0.06;
const int GOLD_CHAN_TOL = 40;

struct GoldView
{
	GoldView() = default;
	GoldView(const GoldView &) = delete;
	GoldView &operator=(const GoldView &) = delete;

	bool loadText(const char *text)
	{
		doc = new PD_Document;
		if (doc->createRawDocument() != UT_OK)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		pt_PieceTable * pt = doc->getPieceTable();
		bool ok = pt->appendStrux(PTX_Section, PP_NOPROPS);
		const char *p = text;
		while (p)
		{
			const char *nl = strchr(p, '\n');
			const PP_PropertyVector * atts = &PP_NOPROPS;
			PP_PropertyVector headingAtts;
			const char * start = p;
			if (p[0] == 'H' && p[1] == ':')
			{
				headingAtts = { "style", "Heading 1" };
				atts = &headingAtts;
				start = p + 2;
			}
			ok = ok && pt->appendStrux(PTX_Block, *atts);
			UT_UCS4String s(start, nl ? static_cast<size_t>(nl - start)
									  : strlen(start));
			if (s.length())
			{
				ok = ok && pt->appendSpan(s.ucs4_str(),
							static_cast<UT_uint32>(s.length()));
			}
			p = nl ? nl + 1 : nullptr;
		}
		if (!ok)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		doc->finishRawCreation();
		return finish();
	}

	bool loadFile(const std::string &path)
	{
		std::string uri = "file://" + path;
		doc = new PD_Document;
		if (doc->readFromFile(uri.c_str(), IEFT_Unknown, nullptr) != UT_OK)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		return finish();
	}

	bool finish()
	{
		GR_UnixCairoAllocInfo ai(nullptr);
		graphics = XAP_App::getApp()->newGraphics(GRID_UNIX_PANGO, ai);
		if (!graphics)
			return false;
		layout = new FL_DocLayout(doc, graphics);
		view = new FV_View(XAP_App::getApp(), nullptr, layout);
		layout->fillLayouts();
		layout->formatAll();
		view->setWindowSize(800, 600);
		return layout->countPages() > 0;
	}

	~GoldView()
	{
		delete view;
		delete layout;
		delete graphics;
		if (doc)
			doc->unref();
	}

	PD_Document *doc = nullptr;
	GR_Graphics *graphics = nullptr;
	FL_DocLayout *layout = nullptr;
	FV_View *view = nullptr;
};

/* paint through a graphics that reports DGP_SCREEN so transparent
 * fills and screen-only blends take the on-screen code path */
class GoldGfx : public GR_UnixCairoGraphics
{
public:
	GoldGfx() : GR_UnixCairoGraphics(nullptr) {}
	bool queryProperties(GR_Graphics::Properties gp) const override
	{
		if (gp == GR_Graphics::DGP_SCREEN ||
			gp == GR_Graphics::DGP_OPAQUEOVERLAY)
			return true;
		return GR_UnixCairoGraphics::queryProperties(gp);
	}
};

cairo_surface_t * gold_render(FV_View * v, FL_DocLayout * layout,
							  int iPage)
{
	fp_Page * pPage = layout->getNthPage(iPage);
	if (!pPage)
		return nullptr;
	GoldGfx * g = new GoldGfx;
	g->setZoomPercentage(GOLD_ZOOM);
	int w = g->tdu(pPage->getWidth());
	int h = g->tdu(pPage->getHeight());
	cairo_surface_t * surf =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cairo_t * cr = cairo_create(surf);
	g->setCairo(cr);
	g->beginPaint();
	{
		GR_Painter painter(g);
		painter.clearArea(0, 0, w, h);
	}
	dg_DrawArgs da;
	da.pG = g;
	da.xoff = 0;
	da.yoff = 0;
	layout->setQuickPrint(g);
	v->drawPage(iPage, &da);
	layout->setQuickPrint(nullptr);
	g->endPaint();
	cairo_destroy(cr);
	delete g;
	cairo_surface_flush(surf);
	return surf;
}

/* bilinear-ish rescale into a fresh w x h surface (cairo's own filter
 * smooths out 1-3px placement differences between the preset and the
 * docx-imported layout so the fidelity diff measures composition,
 * not sub-pixel geometry) */
cairo_surface_t * gold_scaled(cairo_surface_t * src, int w, int h)
{
	cairo_surface_t * out =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cairo_t * cr = cairo_create(out);
	double sx = static_cast<double>(w) /
				cairo_image_surface_get_width(src);
	double sy = static_cast<double>(h) /
				cairo_image_surface_get_height(src);
	cairo_scale(cr, sx, sy);
	cairo_set_source_surface(cr, src, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_flush(out);
	return out;
}

/* fraction of pixels whose biggest BGR channel delta exceeds chanTol;
 * surfaces must be same size or everything counts as different */
double gold_diff_frac(cairo_surface_t * a, cairo_surface_t * b,
					  int chanTol)
{
	int w = cairo_image_surface_get_width(a);
	int h = cairo_image_surface_get_height(a);
	if (w != cairo_image_surface_get_width(b) ||
		h != cairo_image_surface_get_height(b))
		return 1.0;
	int sa = cairo_image_surface_get_stride(a);
	int sb = cairo_image_surface_get_stride(b);
	const unsigned char * da = cairo_image_surface_get_data(a);
	const unsigned char * db = cairo_image_surface_get_data(b);
	long diff = 0;
	for (int y = 0; y < h; ++y)
	{
		const unsigned char * ra = da + y * sa;
		const unsigned char * rb = db + y * sb;
		for (int x = 0; x < w; ++x)
		{
			int d0 = std::abs(int(ra[x*4+0]) - int(rb[x*4+0]));
			int d1 = std::abs(int(ra[x*4+1]) - int(rb[x*4+1]));
			int d2 = std::abs(int(ra[x*4+2]) - int(rb[x*4+2]));
			int m = d0;
			if (d1 > m) m = d1;
			if (d2 > m) m = d2;
			if (m > chanTol)
				++diff;
		}
	}
	return static_cast<double>(diff) /
		   (static_cast<double>(w) * static_cast<double>(h));
}

std::string gold_path(const char * name)
{
	std::string p = TF_Test::get_test_src_dir();
	p += "/test/wp/tst06/goldens/";
	p += name;
	p += ".png";
	return p;
}

bool gold_regen()
{
	const char * e = getenv("ABINOVA_REGEN_GOLDENS");
	return e && e[0] && e[0] != '0';
}

/* true when surf matches the committed golden (or the golden was just
 * (re)written).  Missing golden => failure with regen instructions. */
bool gold_verify(const char * name, cairo_surface_t * surf,
				 double fracTol)
{
	std::string path = gold_path(name);
	if (gold_regen())
	{
		std::string dir = path.substr(0, path.rfind('/'));
		g_mkdir_with_parents(dir.c_str(), 0755);
		printf("WROTE golden %s\n", path.c_str());
		return cairo_surface_write_to_png(surf, path.c_str()) ==
			   CAIRO_STATUS_SUCCESS;
	}
	cairo_surface_t * ref = cairo_image_surface_create_from_png(path.c_str());
	if (cairo_surface_status(ref) != CAIRO_STATUS_SUCCESS)
	{
		cairo_surface_destroy(ref);
		printf("missing golden %s — regenerate with "
			   "ABINOVA_REGEN_GOLDENS=1\n", path.c_str());
		return false;
	}
	double frac = gold_diff_frac(ref, surf, GOLD_CHAN_TOL);
	cairo_surface_destroy(ref);
	if (frac > fracTol)
	{
		printf("golden %s: %.2f%% pixels differ (tol %.2f%%)\n",
			   path.c_str(), frac * 100., fracTol * 100.);
		return false;
	}
	return true;
}

}

TFTEST_MAIN("cover preset render goldens")
{
	for (const char * szPreset : s_presets)
	{
		TF_Test::pulse();
		GoldView v;
		TFPASS(v.loadText("Cover-host body paragraph one.\n"
						  "Cover-host body paragraph two."));
		if (!v.view)
			continue;
		v.view->setPoint(2);
		TFPASS(v.view->cmdInsertCoverPage(szPreset) == UT_OK);
		TFPASS(v.view->hasCoverPage());
		cairo_surface_t * surf = gold_render(v.view, v.layout, 0);
		TFPASS(surf != nullptr);
		if (!surf)
			continue;
		std::string name = std::string("cover-") + szPreset;
		TFPASS(gold_verify(name.c_str(), surf, GOLD_FRAC_TOL));
		cairo_surface_destroy(surf);
	}
}

TFTEST_MAIN("golden diff is stable and catches a wrong render")
{
	/* same preset rendered twice: identical raster */
	GoldView a, b;
	TFPASS(a.loadText("Stability body.\nSecond body."));
	TFPASS(b.loadText("Stability body.\nSecond body."));
	a.view->setPoint(2);
	b.view->setPoint(2);
	TFPASS(a.view->cmdInsertCoverPage("badge") == UT_OK);
	TFPASS(b.view->cmdInsertCoverPage("badge") == UT_OK);
	cairo_surface_t * sa = gold_render(a.view, a.layout, 0);
	cairo_surface_t * sb = gold_render(b.view, b.layout, 0);
	TFPASS(sa && sb);
	if (sa && sb)
	{
		double self = gold_diff_frac(sa, sb, GOLD_CHAN_TOL);
		if (self > 0.01)
			printf("self-diff %.2f%%\n", self * 100.);
		TFPASS(self <= 0.01);
	}

	/* a different preset must exceed the golden tolerance */
	GoldView w;
	TFPASS(w.loadText("Stability body.\nSecond body."));
	w.view->setPoint(2);
	TFPASS(w.view->cmdInsertCoverPage("whip") == UT_OK);
	cairo_surface_t * sw = gold_render(w.view, w.layout, 0);
	TFPASS(sw != nullptr);
	if (sa && sw)
	{
		double cross = gold_diff_frac(sa, sw, GOLD_CHAN_TOL);
		if (cross <= GOLD_FRAC_TOL)
			printf("badge-vs-whip diff only %.2f%%\n", cross * 100.);
		TFPASS(cross > GOLD_FRAC_TOL);
	}
	cairo_surface_destroy(sa);
	cairo_surface_destroy(sb);
	cairo_surface_destroy(sw);
}

TFTEST_MAIN("toc and table render goldens")
{
	/* TOC render: headings + inserted TOC on page 0 */
	{
		GoldView v;
		TFPASS(v.loadText("H:Alpha\nBody under alpha.\n"
						  "H:Beta\nBody under beta."));
		if (v.view)
		{
			v.view->setPoint(2);
			TFPASS(v.view->cmdInsertTOC() == UT_OK);
			cairo_surface_t * surf = gold_render(v.view, v.layout, 0);
			TFPASS(surf != nullptr);
			if (surf)
			{
				TFPASS(gold_verify("toc", surf, GOLD_FRAC_TOL));
				cairo_surface_destroy(surf);
			}
		}
	}

	/* table render: a small table at the top of page 0 */
	{
		GoldView v;
		TFPASS(v.loadText("table host"));
		if (v.view)
		{
			v.view->setPoint(2);
			TFPASS(v.view->cmdInsertTable(3, 3, PP_NOPROPS) != UT_ERROR);
			v.layout->formatAll();
			cairo_surface_t * surf = gold_render(v.view, v.layout, 0);
			TFPASS(surf != nullptr);
			if (surf)
			{
				TFPASS(gold_verify("table", surf, GOLD_FRAC_TOL));
				cairo_surface_destroy(surf);
			}
		}
	}
}

TFTEST_MAIN("docx cover fidelity against preset renders")
{
	const char * dir = getenv("COVER_FIXTURES_DIR");
	if (!dir || !dir[0])
	{
		printf("SKIP: COVER_FIXTURES_DIR unset — corpus not run\n");
		return;
	}

	for (const FidelityPair & p : s_pairs)
	{
		TF_Test::pulse();

		/* the docx render against its own committed golden; a missing
		 * fixture skips the leg, it does not fail it */
		std::string path = std::string(dir) + "/" + p.file;
		FILE *fp = fopen(path.c_str(), "rb");
		if (!fp)
		{
			printf("SKIP: %s unavailable\n", p.file);
			continue;
		}
		fclose(fp);
		GoldView d;
		TFPASS(d.loadFile(path));
		if (!d.view)
			continue;
		cairo_surface_t * sd = gold_render(d.view, d.layout, 0);
		TFPASS(sd != nullptr);
		if (!sd)
			continue;
		std::string dname = std::string("docx-") + p.preset;
		TFPASS(gold_verify(dname.c_str(), sd, GOLD_FRAC_TOL));

		/* the matching preset render */
		GoldView v;
		TFPASS(v.loadText("Cover-host body paragraph one.\n"
						  "Cover-host body paragraph two."));
		if (!v.view)
		{
			cairo_surface_destroy(sd);
			continue;
		}
		v.view->setPoint(2);
		TFPASS(v.view->cmdInsertCoverPage(p.preset) == UT_OK);
		cairo_surface_t * sp = gold_render(v.view, v.layout, 0);
		TFPASS(sp != nullptr);
		if (!sp)
		{
			cairo_surface_destroy(sd);
			continue;
		}

		/* downscale both to a common 128x166 tile and compare gross
		 * composition — the preset mimics the Word layout, so the
		 * broad regions of colour must overlap */
		cairo_surface_t * ds = gold_scaled(sd, 128, 166);
		cairo_surface_t * ps = gold_scaled(sp, 128, 166);
		double frac = gold_diff_frac(ds, ps, 56);
		printf("%s vs %s: %.2f%% composition diff (tol %.2f%%)\n",
			   p.file, p.preset, frac * 100., p.tol * 100.);
		TFPASS(frac <= p.tol);

		cairo_surface_destroy(ds);
		cairo_surface_destroy(ps);
		cairo_surface_destroy(sd);
		cairo_surface_destroy(sp);
	}
}
