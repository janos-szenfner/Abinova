/* AbiSource Application Framework
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

/* unit tests for the display-free parts of af/gr/gtk: the GdkPixbuf
 * raster image (buffer round-trip, transparency probing, DrawingML
 * blip effects), the librsvg vector image, the cairo print graphics,
 * the null-widget paths of GR_UnixCairoGraphics, and the math/media
 * embed managers.  Everything here runs on in-memory cairo surfaces
 * and GdkPixbufs -- no display or widget realization is needed. */

#include <string.h>

#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>

#include "tf_test.h"

#include "xap_App.h"

#include "ev_EditBits.h"
#include "gr_CairoImage.h"
#include "gr_CairoNullGraphics.h"
#include "gr_CairoPrintGraphics.h"
#include "gr_GtkMathManager.h"
#include "gr_GtkMediaManager.h"
#include "gr_UnixCairoGraphics.h"
#include "gr_UnixImage.h"
#include "pd_Document.h"
#include "ut_debugmsg.h"

#define TFSUITE "core.af.gr.gtk"

/* TFAssertSilence (assert silencer) and tf_null_graphics() come from
 * ev_Tables.t.cpp / gr_Primitives.t.cpp, included earlier in this TU. */

namespace {

/* a solid 4x4 RGBA pixel block: all opaque except px (1,1) */
static GdkPixbuf * tf_pixbuf(bool hasAlpha)
{
	GdkPixbuf * px = gdk_pixbuf_new(GDK_COLORSPACE_RGB, hasAlpha, 8, 4, 4);
	if (!px)
		return nullptr;
	gdk_pixbuf_fill(px, 0x3366ccff);
	if (hasAlpha) {
		guchar * p = gdk_pixbuf_get_pixels(px)
			+ gdk_pixbuf_get_rowstride(px) * 1 + 4 * 1;
		p[0] = p[1] = p[2] = p[3] = 0; /* fully transparent pixel */
	}
	return px;
}

/* owns the pixbuf it is handed */
static UT_ByteBufPtr tf_png_bytes(GdkPixbuf * px)
{
	gchar * buf = nullptr;
	gsize len = 0;
	UT_ByteBufPtr pBB;
	if (gdk_pixbuf_save_to_buffer(px, &buf, &len, "png", nullptr, nullptr)) {
		pBB.reset(new UT_ByteBuf);
		pBB->append(reinterpret_cast<const UT_Byte *>(buf), len);
	}
	g_free(buf);
	g_object_unref(px);
	return pBB;
}

static PD_Document * tf_doc(void)
{
	PD_Document * doc = new PD_Document;
	doc->newDocument();
	return doc;
}

static UT_ConstByteBufPtr tf_bytes(const char * s)
{
	UT_ByteBuf * b = new UT_ByteBuf;
	b->append(reinterpret_cast<const UT_Byte *>(s), strlen(s));
	return UT_ConstByteBufPtr(b);
}

} /* anonymous namespace */

/* ------------------------------------------------------------------ */
/* GR_UnixImage                                                        */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("GR_UnixImage basics")
{
	/* no image loaded: every accessor has a clean empty answer */
	GR_UnixImage empty(nullptr); /* default-name ctor branch */
	TFPASS(empty.getType() == GR_Image::GRT_Raster);
	TFPASS(empty.getData() == nullptr);
	TFPASS(!empty.hasAlpha());
	TFPASS(empty.rowStride() == 0);

	UT_ConstByteBufPtr bb;
	TFPASS(!empty.convertToBuffer(bb)); /* no pixels -> false */
	TFPASS(bb == nullptr);
	TFPASS(!empty.saveToPNG("/tmp/tf-empty.png"));

	empty.scale(-1, 10);   /* non-positive size is ignored */
	empty.scale(10, -1);
	empty.scale(10, 10);   /* no image: still a no-op */

	/* image-type ctor remembers its tag */
	GR_UnixImage vec(nullptr, GR_Image::GRT_Vector);
	TFPASS(vec.getType() == GR_Image::GRT_Vector);

	GR_UnixImage named("MyName", GR_Image::GRT_Unknown);
	TFPASS(named.getType() == GR_Image::GRT_Unknown);
	UT_String nm;
	named.getName(nm);
	TFPASS(nm == "MyName");
}

TFTEST_MAIN("GR_UnixImage buffer round-trip")
{
	UT_ByteBufPtr png = tf_png_bytes(tf_pixbuf(false));
	TFPASS(png && png->getLength() > 0);

	GR_UnixImage img("loaded");
	TFPASS(img.convertFromBuffer(png, "image/png", 4, 4));
	TFPASS(img.getData() != nullptr);
	TFPASS(gdk_pixbuf_get_width(img.getData()) == 4);
	TFPASS(img.getDisplayWidth() == 4 && img.getDisplayHeight() == 4);

	/* the loaded image serialises back out as PNG */
	UT_ConstByteBufPtr out;
	TFPASS(img.convertToBuffer(out));
	TFPASS(out && out->getLength() > 0);

	GR_UnixImage img2("loaded2");
	TFPASS(img2.convertFromBuffer(out, "image/png", 8, 8));
	TFPASS(img2.getDisplayWidth() == 8);

	/* saveToPNG failure reports false */
	TFPASS(img.saveToPNG("/tmp/tf-unix-image.png"));
	TFPASS(!img.saveToPNG("/nonexistent-dir/tf-x.png"));
}

TFTEST_MAIN("GR_UnixImage malformed buffers")
{
	GR_UnixImage img("bad");

	/* empty buffer: the loader closes cleanly but yields no pixbuf */
	UT_ByteBuf * empty = new UT_ByteBuf;
	UT_ConstByteBufPtr pEmpty(empty);
	TFPASS(!img.convertFromBuffer(pEmpty, "image/png", 4, 4));
	TFPASS(img.getData() == nullptr);

	/* garbage that is not an image at all: write or close fails */
	GR_UnixImage img2("bad2");
	UT_ConstByteBufPtr pBad = tf_bytes("this is not an image at all");
	TFPASS(!img2.convertFromBuffer(pBad, "image/png", 4, 4));
	TFPASS(img2.getData() == nullptr);
}

TFTEST_MAIN("GR_UnixImage transparency")
{
	GR_UnixImage img("alpha");
	img.setData(tf_pixbuf(true));
	img.setDisplaySize(4, 4);

	TFPASS(img.hasAlpha());
	TFPASS(img.rowStride() >= 16);

	/* opaque pixel -> not transparent; the zeroed pixel is */
	TFPASS(!img.isTransparentAt(0, 0));
	TFPASS(img.isTransparentAt(1, 1));
	/* out-of-range probes are refused */
	TFPASS(!img.isTransparentAt(-1, 0));
	TFPASS(!img.isTransparentAt(4, 0));
	TFPASS(!img.isTransparentAt(0, 4));

	/* RGB pixels have no alpha to probe */
	GR_UnixImage flat("flat");
	flat.setData(tf_pixbuf(false));
	TFPASS(!flat.hasAlpha());
	TFPASS(!flat.isTransparentAt(0, 0));
}

TFTEST_MAIN("GR_UnixImage crop + cairoSetSource")
{
	GR_Graphics * nullg = tf_null_graphics();

	GR_UnixImage img("src");
	img.setData(tf_pixbuf(false));
	img.setDisplaySize(4, 4);

	/* createImageSegment() routes through makeSubimage() and crops */
	UT_Rect half(0, 0, 1000, 1000);
	GR_Image * seg = img.createImageSegment(nullg, half);
	TFPASS(seg != nullptr);
	delete seg;

	/* cairoSetSource on a real cairo context paints the pixbuf */
	cairo_surface_t * sf =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 8, 8);
	cairo_t * cr = cairo_create(sf);
	img.cairoSetSource(cr);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(sf);

	/* scaleImageTo() converts the logical rect through the graphics */
	UT_Rect r(0, 0, 500, 700);
	img.scaleImageTo(nullg, r);
	TFPASS(img.getDisplayWidth() == nullg->tdu(500));
	TFPASS(img.getDisplayHeight() == nullg->tdu(700));
}

TFTEST_MAIN("GR_UnixImage blip effects")
{
	GR_UnixImage img("fx");
	img.setData(tf_pixbuf(false));

	/* an all-default effect set is a no-op */
	GR_BlipEffects none;
	img.applyBlipEffects(none);
	TFPASS(!img.hasAlpha());

	/* luminance with negative brightness walks the dark branch */
	GR_BlipEffects dark;
	dark.lum = true;
	dark.lumBright = -0.4;
	dark.lumContrast = 0.0;
	img.applyBlipEffects(dark);

	/* alphaMod on an opaque image adds the channel first */
	GR_BlipEffects fade;
	fade.alphaMod = 0.5;
	img.applyBlipEffects(fade);
	TFPASS(img.hasAlpha());
	TFPASS(img.isTransparentAt(0, 0) == false); /* 50% alpha, not 0 */

	/* grayscale + duotone together */
	GR_BlipEffects duo;
	duo.grayscale = true;
	duo.duotone = true;
	duo.duoLo = UT_RGBColor(0, 0, 0);
	duo.duoHi = UT_RGBColor(255, 255, 255);
	img.applyBlipEffects(duo);
}

/* ------------------------------------------------------------------ */
/* GR_RSVGVectorImage                                                  */
/* ------------------------------------------------------------------ */

static const char * const tf_svg =
	"<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'>"
	"<rect width='8' height='8' fill='#3366cc'/></svg>";

TFTEST_MAIN("GR_RSVGVectorImage")
{
	GR_RSVGVectorImage img(nullptr); /* default "SVGImage" name */
	UT_String nm;
	img.getName(nm);
	TFPASS(nm == "SVGImage");

	/* malformed input is refused */
	UT_ConstByteBufPtr bad = tf_bytes("<svg><bogus</svg>");
	TFPASS(!img.convertFromBuffer(bad, "image/svg+xml", -1, -1));

	/* a real SVG loads and reports its intrinsic size */
	UT_ConstByteBufPtr svg = tf_bytes(tf_svg);
	TFPASS(img.convertFromBuffer(svg, "image/svg+xml", -1, -1));
	TFPASS(img.getDisplayWidth() == 8);
	TFPASS(img.getDisplayHeight() == 8);
	TFPASS(img.hasAlpha());

	/* the raw bytes round-trip back out */
	UT_ConstByteBufPtr out;
	TFPASS(img.convertToBuffer(out));
	TFPASS(out && out->getLength() == strlen(tf_svg));

	/* paint into a real cairo surface and probe a pixel */
	cairo_surface_t * sf =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 8, 8);
	cairo_t * cr = cairo_create(sf);
	img.cairoSetSource(cr);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(sf);

	/* transparency probing renders the image on demand */
	TFPASS(!img.isTransparentAt(4, 4)); /* solid fill */
	TFPASS(!img.isTransparentAt(-1, 0));

	/* scale + segment on a null graphics */
	GR_Graphics * nullg = tf_null_graphics();
	UT_Rect rec(0, 0, 400, 400);
	img.scaleImageTo(nullg, rec);
	TFPASS(img.getDisplayWidth() == nullg->tdu(400));
	GR_Image * seg = img.createImageSegment(nullg, rec);
	TFPASS(seg != nullptr);
	delete seg;
}

/* ------------------------------------------------------------------ */
/* GR_CairoPrintGraphics                                               */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("GR_CairoPrintGraphics")
{
	cairo_surface_t * sf =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
	cairo_t * cr = cairo_create(sf);

	GR_CairoPrintGraphics pg(cr, 300);
	TFPASS(pg.getClassId() == GRID_UNIX_PANGO_PRINT);
	TFPASS(GR_CairoPrintGraphics::s_getClassId() == GRID_UNIX_PANGO_PRINT);
	TFPASS(!strcmp(GR_CairoPrintGraphics::graphicsDescriptor(),
				 "Unix Cairo Print"));
	TFPASS(pg.getCapability() == GRCAP_PRINTER_ONLY);
	TFPASS(pg.canQuickPrint());

	/* print graphics live on paper only */
	TFPASS(!pg.queryProperties(GR_Graphics::DGP_SCREEN));
	TFPASS(!pg.queryProperties(GR_Graphics::DGP_OPAQUEOVERLAY));
	TFPASS(pg.queryProperties(GR_Graphics::DGP_PAPER));

	/* the resolution ratio must stay finite and positive */
	TFPASS(pg.getResolutionRatio() == 1.0);
	pg.setResolutionRatio(2.0);
	TFPASS(pg.getResolutionRatio() == 2.0);
	pg.setResolutionRatio(0.0);
	TFPASS(pg.getResolutionRatio() == 1.0);
	pg.setResolutionRatio(-3.0);
	TFPASS(pg.getResolutionRatio() == 1.0);

	/* page lifecycle drives cairo_show_page */
	TFPASS(pg.startPrint());
	TFPASS(pg.startPage("p1", 1, true, 100, 100));
	TFPASS(pg.startPage("p2", 2, false, 100, 100));
	TFPASS(pg.endPrint());

	/* screen-widget operations assert-not-reached on a print
	 * graphics; exercise the fallbacks with asserts silenced */
	{
		TFAssertSilence quiet;
		pg.setCursor(GR_Graphics::GR_CURSOR_DEFAULT);
		TFPASS(pg.getCursor() == GR_Graphics::GR_CURSOR_INVALID);
		TFPASS(pg.getGUIFont() == nullptr);
		pg.queueDraw(nullptr);
		pg.scroll(1, 1);
		pg.scroll(0, 0, 1, 1, 2, 2);
		UT_Rect r(0, 0, 10, 10);
		TFPASS(pg.genImageFromRectangle(r) == nullptr);
		pg.saveRectangle(r, 0);
		pg.restoreRectangle(0);
		pg.setPageSize(const_cast<char *>("A4"), 0, 0);
	}

	/* ~GR_CairoPrintGraphics() destroys m_cr — only the surface
	 * remains ours */
	cairo_surface_destroy(sf);
}

/* ------------------------------------------------------------------ */
/* GR_UnixCairoGraphics with no widget                                 */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("GR_UnixCairoGraphics null widget")
{
	GR_UnixCairoAllocInfo ai(static_cast<GtkWidget *>(nullptr));
	GR_Graphics * pG = GR_UnixCairoGraphics::graphicsAllocator(ai);
	TFPASS(pG != nullptr);
	GR_UnixCairoGraphics * g = static_cast<GR_UnixCairoGraphics *>(pG);

	TFPASS(g->getClassId() == GRID_UNIX_PANGO);
	TFPASS(!strcmp(GR_UnixCairoGraphics::graphicsDescriptor(),
				 "Unix Cairo Pango"));
	TFPASS(g->getWidget() == nullptr);
	TFPASS(g->getWindow() == nullptr);

	/* without a widget no property can be satisfied by a window */
	TFPASS(!g->queryProperties(GR_Graphics::DGP_SCREEN));
	TFPASS(!g->queryProperties(GR_Graphics::DGP_OPAQUEOVERLAY));
	TFPASS(!g->queryProperties(GR_Graphics::DGP_PAPER));

	/* 3D theme colors: the two slots needing a widget report false,
	 * the rest fall through to the cairo base table */
	UT_RGBColor c;
	TFPASS(!g->getColor3D(GR_Graphics::CLR3D_Background, c));
	TFPASS(!g->getColor3D(GR_Graphics::CLR3D_Highlight, c));
	g->getColor3D(GR_Graphics::CLR3D_Foreground, c);

	/* fillRect on a 3D slot with no cairo context exits early */
	{
		UT_Rect r(0, 0, 10, 10);
		g->fillRect(GR_Graphics::CLR3D_Background, 0, 0, 10, 10);
		g->fillRect(GR_Graphics::CLR3D_Highlight, r.left, r.top,
					r.width, r.height);
	}

	/* a screenshot needs the widget's render node */
	UT_Rect shot(0, 0, 100, 100);
	TFPASS(g->genImageFromRectangle(shot) == nullptr);

	/* beginFrame creates a backing surface with no widget around;
	 * endFrame blits it into the supplied gtk cairo context */
	cairo_t * frame = g->beginFrame();
	TFPASS(frame != nullptr);
	cairo_surface_t * target =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 16, 16);
	cairo_t * gtkCr = cairo_create(target);
	g->endFrame(gtkCr);
	cairo_destroy(gtkCr);
	cairo_surface_destroy(target);

	/* queueing invalidations is a no-op without a widget (the guard
	 * asserts first) */
	{
		TFAssertSilence quiet;
		g->queueDraw(nullptr);
		g->flush();
	}

	/* init3dColors() needs a live GTK display for donor-widget style
	 * queries — untestable headless. The 3D override table itself is
	 * display-free. */
	g->override3DColor(GR_Graphics::CLR3D_Foreground,
					 UT_RGBColor(9, 9, 9));
	UT_RGBColor fg;
	TFPASS(!g->getColor3D(GR_Graphics::CLR3D_Foreground, fg));

	/* cursor names resolve statically */
	TFPASS(!strcmp(GR_UnixCairoGraphics::_getCursor(
					 GR_Graphics::GR_CURSOR_DEFAULT), "default"));
	TFPASS(!strcmp(GR_UnixCairoGraphics::_getCursor(
					 GR_Graphics::GR_CURSOR_IBEAM), "text"));

	/* the GUI font resolves through pango without a widget */
	GR_Font * gui = g->getGUIFont();
	TFPASS(gui != nullptr);

	/* createNewImage dispatches on the requested type */
	UT_ByteBufPtr png = tf_png_bytes(tf_pixbuf(false));
	GR_Image * ri = g->createNewImage("r.png", png, "image/png", 4, 4,
									GR_Image::GRT_Raster);
	TFPASS(ri != nullptr && ri->getType() == GR_Image::GRT_Raster);
	delete ri;

	UT_ConstByteBufPtr svg = tf_bytes(tf_svg);
	GR_Image * vi = g->createNewImage("v.svg", svg, "image/svg+xml",
									8, 8, GR_Image::GRT_Vector);
	TFPASS(vi != nullptr && vi->getType() == GR_Image::GRT_Vector);
	delete vi;

	delete g;
}

/* ------------------------------------------------------------------ */
/* GR_GtkMathManager                                                   */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("GR_GtkMathManager")
{
	GR_UnixCairoAllocInfo ai(static_cast<GtkWidget *>(nullptr));
	GR_Graphics * pG = GR_UnixCairoGraphics::graphicsAllocator(ai);
	GR_GtkMathManager mgr(pG);

	/* factory + identity */
	GR_EmbedManager * clone = mgr.create(pG);
	TFPASS(clone != nullptr);
	delete clone;
	TFPASS(!strcmp(mgr.getObjectType(), "mathml"));
	TFPASS(!strcmp(mgr.getMimeType(), "application/mathml+xml"));
	TFPASS(!strcmp(mgr.getMimeTypeDescription(), "MathML Equation"));
	TFPASS(!strcmp(mgr.getMimeTypeSuffix(), ".mml"));
	TFPASS(!mgr.isDefault());
	TFPASS(mgr.isEdittable(0));
	TFPASS(mgr.getContextualMenu() == EV_EMC_MATH);

	/* bad uids are a clean miss everywhere */
	TFPASS(mgr.getWidth(-1) == 0);
	TFPASS(mgr.getAscent(99) == 0);
	TFPASS(mgr.getDescent(99) == 0);
	TFPASS(!mgr.setFont(7, nullptr));
	mgr.setColor(-3, UT_RGBColor(0, 0, 0));
	mgr.setDefaultFontSize(-1, 12);
	mgr.setRun(-1, nullptr);
	mgr.initializeEmbedView(-1);
	mgr.releaseEmbedView(-1);
	mgr.updateData(-1, 0);
	mgr.loadEmbedData(-1);

	PD_Document * doc = tf_doc();
	UT_sint32 uid = mgr.makeEmbedView(doc, 7, "MathLatexU");
	TFPASS(uid >= 0);

	/* LatexMath<suffix> is the fallback source for MathLatex<uuid> */
	std::string mime = "text/x-latex";
	TFPASS(doc->createDataItem("LatexMathU", false,
							   tf_bytes("x+1"), mime, nullptr));
	mgr.loadEmbedData(uid);

	mgr.setColor(uid, UT_RGBColor(64, 64, 64));
	TFPASS(mgr.setFont(uid, nullptr)); /* null font keeps the family */
	mgr.setDefaultFontSize(uid, 24);
	mgr.setDefaultFontSize(uid, 999); /* rejected, still relayouts */
	mgr.setDisplayMode(uid, ABI_DISPLAY_INLINE);
	mgr.setDisplayMode(uid, ABI_DISPLAY_BLOCK);

	TFPASS(mgr.getWidth(uid) > 0);
	TFPASS(mgr.getAscent(uid) > 0);
	TFPASS(mgr.getDescent(uid) >= 0);

	/* render() early-outs on degenerate rects, then really paints */
	UT_Rect empty(0, 0, 0, 0);
	mgr.render(uid, empty);
	GR_UnixCairoGraphics * ug = static_cast<GR_UnixCairoGraphics *>(pG);
	cairo_t * frame = ug->beginFrame();
	UT_Rect rec(0, 0, 2000, 800);
	mgr.render(uid, rec);
	ug->endFrame(frame);

	/* the snapshot lands in the document as an SVG data item; a
	 * second identical snapshot is skipped */
	mgr.makeSnapShot(uid, rec);
	UT_ConstByteBufPtr snap;
	TFPASS(doc->getDataItemDataByName("snapshot-svg-MathLatexU", snap,
									nullptr, nullptr));
	TFPASS(snap && snap->getLength() > 0);
	mgr.makeSnapShot(uid, rec); /* unchanged -> skip write */

	/* a mathml data id goes through parseMathML instead */
	UT_sint32 uid2 = mgr.makeEmbedView(doc, 8, "mml2");
	std::string mmlMime = "application/mathml+xml";
	TFPASS(doc->createDataItem("mml2", false,
							   tf_bytes("<math><mi>a</mi></math>"),
							   mmlMime, nullptr));
	mgr.loadEmbedData(uid2);
	TFPASS(mgr.getWidth(uid2) > 0);

	/* a data id with no matching item still lays out cleanly */
	UT_sint32 uid3 = mgr.makeEmbedView(doc, 9, "missing");
	mgr.loadEmbedData(uid3);
	TFPASS(mgr.getWidth(uid3) >= 0);

	/* modify is dialog-driven -> false; convert turns latex into
	 * mathml; updateData just re-tags the api */
	TFPASS(!mgr.modify(uid));
	UT_ByteBufPtr to(new UT_ByteBuf);
	TFPASS(mgr.convert(0, tf_bytes("x^2"), to));
	TFPASS(to->getLength() > 0);
	TFPASS(to->getPointer(0)[0] == '<');
	TFPASS(!mgr.convert(0, nullptr, to));
	UT_ByteBufPtr toEmpty(new UT_ByteBuf);
	TFPASS(!mgr.convert(0, tf_bytes(""), toEmpty));
	mgr.updateData(uid, 42);

	mgr.releaseEmbedView(uid);
	mgr.setRun(uid, nullptr); /* released slot is a clean miss */

	doc->unref();
	delete pG;
}

/* ------------------------------------------------------------------ */
/* GR_GtkMediaManager                                                  */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("GR_GtkMediaManager")
{
	GR_UnixCairoAllocInfo ai(static_cast<GtkWidget *>(nullptr));
	GR_Graphics * pG = GR_UnixCairoGraphics::graphicsAllocator(ai);

	GR_GtkMediaManager mgr(pG, "media");
	TFPASS(!strcmp(mgr.getObjectType(), "media"));
	TFPASS(!strcmp(mgr.getMimeType(), "application/octet-stream"));
	TFPASS(!strcmp(mgr.getMimeTypeDescription(), "Embedded media"));
	TFPASS(!strcmp(mgr.getMimeTypeSuffix(), ".bin"));
	TFPASS(!mgr.isDefault());
	TFPASS(mgr.isEdittable(0));
	TFPASS(mgr.isResizeable(0));

	GR_EmbedManager * clone = mgr.create(pG);
	TFPASS(clone != nullptr);
	delete clone;

	/* uid bounds */
	mgr.setRun(-1, nullptr);
	mgr.setRun(0, nullptr); /* no items yet -> clean miss */
	TFPASS(!mgr.modify(0));

	PD_Document * doc = tf_doc();

	/* a view with no data id cannot be modified */
	UT_sint32 uid = mgr.makeEmbedView(doc, 3, nullptr);
	TFPASS(uid >= 0);
	TFPASS(!mgr.modify(uid)); /* empty dataID -> early out */
	mgr.setRun(uid, nullptr);

	/* a view naming a data item the document does not have fails
	 * before any temp file is written */
	UT_sint32 uid2 = mgr.makeEmbedView(doc, 4, "no-such-item");
	TFPASS(!mgr.modify(uid2));

	mgr.releaseEmbedView(uid);
	mgr.releaseEmbedView(uid2);
	mgr.releaseEmbedView(uid);  /* released twice -> clean miss */

	doc->unref();
	delete pG;
}
