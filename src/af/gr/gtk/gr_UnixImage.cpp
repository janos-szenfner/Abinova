/* AbiWord
 * Copyright (C) 2001-2002 Dom Lachowicz
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
#include <vector>

#include "gr_UnixImage.h"
#include "ut_assert.h"
#include "ut_bytebuf.h"
#include "ut_debugmsg.h"
#include "gr_Graphics.h"
#include "ut_string_class.h"
#include "xap_GtkUtils.h"
#include <gdk-pixbuf/gdk-pixbuf-loader.h>

/* quick implementation of cropped images, thecrop rectangle should go 
 * to GR_Image in next versions. */
class GR_UnixCroppedImage: public GR_UnixImage
{
public:
	GR_UnixCroppedImage(const char* pszName);
	GR_UnixCroppedImage(const char* pszName, GdkPixbuf * pPixbuf);

	virtual void cairoSetSource(cairo_t *) override;
	void crop(double left, double top, double right, double bottom);
private:
	 double m_CropLeft, m_CropRight, m_CropTop, m_CropBottom;
};

GR_UnixCroppedImage::GR_UnixCroppedImage(const char* pszName):
	GR_UnixImage(pszName),
	m_CropLeft(0.),
	m_CropRight(0.),
	m_CropTop(0.),
	m_CropBottom(0.)
{
}

GR_UnixCroppedImage::GR_UnixCroppedImage(const char* pszName, GdkPixbuf * pPixbuf):
	GR_UnixImage(pszName, pPixbuf),
	m_CropLeft(0.),
	m_CropRight(0.),
	m_CropTop(0.),
	m_CropBottom(0.)
{
}

void GR_UnixCroppedImage::cairoSetSource(cairo_t *cr)
{
	GdkPixbuf const *image = getData();
	UT_return_if_fail(image);
	double w, h;
	w = gdk_pixbuf_get_width(image);
	h = gdk_pixbuf_get_height(image);
	double scaleX = static_cast<double>(getDisplayWidth() )/ w / (1 - m_CropLeft - m_CropRight);
	double scaleY = static_cast<double>(getDisplayHeight() )/ h / (1 - m_CropTop - m_CropBottom);
	cairo_scale(cr, scaleX, scaleY);
	cairo_rectangle(cr, 0., 0.,
	                (1 - m_CropLeft - m_CropRight) * w,
	                (1 - m_CropTop - m_CropBottom) * h);
	cairo_clip(cr);
	xap_gdk_cairo_set_source_pixbuf(cr, image, -m_CropLeft * w,
	                            -m_CropTop * h);
}

void GR_UnixCroppedImage::crop(double left, double top, double right, double bottom)
{
	m_CropLeft = left;
	m_CropTop = top;
	m_CropRight = right;
	m_CropBottom = bottom;
}

/*!
 * From Martin. I spent a LOT of time in class tracking down terrible
 * Memory leaks because the only way to delete a pixbuf from memory
 * is to unref an object of reference count 1. 
 * Unfortunately I discovered from experience that some operations appear
 * to leave reference count in an undefined state.
 *
 * So a note to future hackers in this class. Please keep a close eye on the
 * reference count on the GdkPixbuf's and keep the liberal asserts I've placed.
 */
GR_UnixImage::GR_UnixImage(const char* szName)
  : m_image(nullptr)
{
  if (szName)
  {
    setName ( szName );
  }
  else
  {
    setName ( "GdkPixbufImage" );
  }
  m_ImageType = GR_Image::GRT_Raster;
}

GR_UnixImage::GR_UnixImage(const char* szName,GdkPixbuf * pPixbuf )
  : m_image(pPixbuf)
{
  if (szName)
  {
    setName ( szName );
  }
  else
  {
    setName ( "GdkPixbufImage" );
  }
  m_ImageType = GR_Image::GRT_Raster;
  if(m_image)
      setDisplaySize (gdk_pixbuf_get_width (pPixbuf), gdk_pixbuf_get_height (pPixbuf));
}


GR_UnixImage::GR_UnixImage(const char* szName, GR_Image::GRType imageType)
  : m_image(nullptr)
{
  if (szName)
  {
    setName ( szName );
  }
  else
  {
    setName ( "GdkPixbufImage" );
  }
  m_ImageType = imageType;
}

GR_UnixImage::~GR_UnixImage()
{
	if(m_image != nullptr)
	{
		UT_ASSERT(G_OBJECT(m_image)->ref_count == 1);
		g_object_unref(G_OBJECT(m_image));
	}
}

GR_Image::GRType GR_UnixImage::getType(void) const
{
	return  m_ImageType;
}


GR_UnixImage *GR_UnixImage::makeSubimage(const std::string & name,
                           UT_sint32 x, UT_sint32 y,
                           UT_sint32 width, UT_sint32 height) const
{
    if(m_image == nullptr)
	    return nullptr;
	GR_UnixCroppedImage * pImage = new GR_UnixCroppedImage(name.c_str());

    pImage->m_image = gdk_pixbuf_copy(m_image);
	if (pImage->m_image == nullptr)
	{
		delete pImage;
		return nullptr;
	}
	pImage->setDisplaySize (getDisplayWidth(), getDisplayHeight());

	pImage->crop(static_cast<double>(x )/ static_cast<double>(getDisplayWidth()),
	             static_cast<double>(y )/ static_cast<double>(getDisplayHeight()),
	             1. - (static_cast<double>(x )+ width) / getDisplayWidth(),
	             1. - (static_cast<double>(y )+ height) / getDisplayHeight());
    return pImage;
}


/*!
 * Scale our image to rectangle given by rec. The dimensions of rec
 * are calculated in logical units.
 */
void GR_UnixImage::scaleImageTo(GR_Graphics * pG, const UT_Rect & rec)
{
	UT_sint32 width = pG->tdu(rec.width);
	UT_sint32 height = pG->tdu(rec.height);
	scale(width,height);
}

static gboolean convCallback(const gchar *buf,
			     gsize count,
				 GError ** /*error*/,
			     gpointer byteBuf)
{
  UT_ByteBuf * pBB = reinterpret_cast<UT_ByteBuf *>(byteBuf);
  pBB->append(reinterpret_cast<const UT_Byte *>(buf),count);
  return TRUE;
}

/*!
 * This method fills a byte buffer with a PNG representation of itself.
 * This can be saved in the PT as a data-item and recreated.
 * ppBB is a pointer to a pointer of a byte buffer. It's the callers
 * job to delete it.
 */
bool  GR_UnixImage::convertToBuffer(UT_ConstByteBufPtr & pBB) const
{
  pBB.reset();
  if (!m_image)
  {
    UT_ASSERT(m_image);
    return false;
  }

  const guchar * pixels = gdk_pixbuf_get_pixels(m_image);

  if (pixels)
  {
    GError    * error =nullptr;
    UT_ByteBufPtr bb(new UT_ByteBuf);
    gdk_pixbuf_save_to_callback(m_image,
				convCallback,
				reinterpret_cast<gpointer>(bb.get()),
				"png",
				&error,nullptr,nullptr);
    if(error != nullptr)
      {
	g_error_free (error);
      }
    pBB = bb;
  }

  // UT_ASSERT(G_OBJECT(m_image)->ref_count == 1);
  return true;
}

bool GR_UnixImage::saveToPNG(const char * szFile)
{
        UT_return_val_if_fail(m_image,false);

	GError * error = nullptr;
	gboolean res = gdk_pixbuf_save (m_image, szFile, "png", &error, nullptr);
	if (error) {
		g_error_free (error);
	}
	return res != FALSE;

}

/*! 
 * Returns true if pixel at point (x,y) in device units is transparent.
 */
bool GR_UnixImage::isTransparentAt(UT_sint32 x, UT_sint32 y)
{
  if(!hasAlpha())
  {
    return false;
  }
  UT_return_val_if_fail(m_image,false);
  // UT_sint32 iBitsPerPixel = gdk_pixbuf_get_bits_per_sample(m_image);
  UT_sint32 iRowStride = gdk_pixbuf_get_rowstride(m_image);
  UT_sint32 iWidth =  gdk_pixbuf_get_width(m_image);
  UT_sint32 iHeight =  gdk_pixbuf_get_height(m_image);
  UT_ASSERT(iRowStride/iWidth == 4);
  UT_return_val_if_fail((x>= 0) && (x < iWidth), false);
  UT_return_val_if_fail((y>= 0) && (y < iHeight), false);
  guchar * pData = gdk_pixbuf_get_pixels(m_image);
  xxx_UT_DEBUGMSG(("BitsPerPixel = %d \n",iBitsPerPixel));
  UT_sint32 iOff = iRowStride*y;
  guchar pix0 = pData[iOff+ x*4];
  guchar pix1 = pData[iOff+ x*4 +1];
  guchar pix2 = pData[iOff+ x*4 +2];
  guchar pix3 = pData[iOff+ x*4 +3];
  if((pix3 | pix0 | pix1 | pix2) == 0)
  {
    return true;
  }
  UT_DEBUGMSG(("x %d y %d pix0 %d pix1 %d pix2 %d pix3 %d \n",x,y,pix0,pix1,pix2,pix3));
  return false;
}

bool GR_UnixImage::hasAlpha (void) const
{
	UT_return_val_if_fail(m_image, false);
	return (gdk_pixbuf_get_has_alpha (m_image) ? true : false);
}

UT_sint32 GR_UnixImage::rowStride (void) const
{
	UT_return_val_if_fail(m_image, 0);
	return static_cast<UT_sint32>(gdk_pixbuf_get_rowstride (m_image));
}

// note that this does take device units, unlike everything else.
void GR_UnixImage::scale (UT_sint32 iDisplayWidth, 
						  UT_sint32 iDisplayHeight)
{
	UT_return_if_fail(m_image);
	
	// don't scale if passed -1 for either, or either is 0
	if (iDisplayWidth <= 0 || iDisplayHeight <= 0)
		return;

	setDisplaySize (iDisplayWidth, iDisplayHeight);
}

/*!
Loads an image from from a byte buffer. Note: If the specified width and/or
height does not match the size of the image contained in the byte buffer,
some image information will be lost. WARNING: TODO
@param iDisplayWidth the width to which the image needs to be scaled. Values
are in device units. Setting this to -1 will cause the image not to be scaled.
@param iDisplayheight the height to which the image needs to be scaled. Values
are in device units. Setting this to -1 will cause the image not to be scaled.
@return true if successful, false otherwise
*/
bool GR_UnixImage::convertFromBuffer(const UT_ConstByteBufPtr & pBB,
                                     const std::string & /*mimetype */,
                                     UT_sint32 iDisplayWidth,
                                     UT_sint32 iDisplayHeight)
{
	// assert no image loaded yet

	UT_ASSERT(!m_image);

	
	GError * err = nullptr;
	GdkPixbufLoader * ldr = xap_gtk_pixbuf_loader_new_capped ();

	if (!ldr)
	{
		UT_DEBUGMSG (("GdkPixbuf: couldn't create loader! WTF?\n"));
		UT_ASSERT (ldr);
		return false;
	}

	// We explicitely do not scale the image on load, since cairo can do
	// that during rendering. Scaling during loading will result in lost
	// image information, which in turns results in bugs like
	// in bugs like #12183.
	// So simply remember the display size for drawing later.
	setDisplaySize(iDisplayWidth, iDisplayHeight);
	
	if ( !gdk_pixbuf_loader_write (ldr, static_cast<const guchar *>(pBB->getPointer (0)),static_cast<gsize>(pBB->getLength ()), &err) )
	{
		if(err != nullptr)
		{
			UT_DEBUGMSG(("DOM: couldn't write to loader:%s \n", err->message));
			g_error_free(err);
		}
		gdk_pixbuf_loader_close (ldr, nullptr);
		g_object_unref(G_OBJECT(ldr));
		return false;
	}

	if ( !gdk_pixbuf_loader_close (ldr, &err) )
	{
		if(err != nullptr)
		{
			UT_DEBUGMSG(("DOM: couldn't close loader:%s \n", err->message));
			g_error_free(err);
		}
		g_object_unref(G_OBJECT(ldr));
		return false;
	}

//
// This is just pointer to the buffer in the loader. This can be deleted
// when we close the loader.
//
	m_image = gdk_pixbuf_loader_get_pixbuf (ldr);
	if (!m_image)
	{
		UT_DEBUGMSG (("GdkPixbuf: couldn't get image from loader!\n"));
		gdk_pixbuf_loader_close (ldr, nullptr);
		g_object_unref(G_OBJECT(ldr));
		return false;
	}

//
// Have to put a reference on it to prevent it being deleted during the close.
//
	g_object_ref(G_OBJECT(m_image));
	if ( FALSE == gdk_pixbuf_loader_close (ldr, &err) )
	{
		UT_DEBUGMSG(("DOM: error closing loader. Corrupt image: %s\n", err->message));
		g_error_free(err);
		g_object_unref(G_OBJECT(m_image));
		return false;
	}
	g_object_unref(G_OBJECT(ldr));
//
// Adjust the reference count so it's always 1. Unfortunately 
// the reference count after the close is undefined.
//
	while(G_OBJECT(m_image)->ref_count > 1)
	{
		g_object_unref(G_OBJECT(m_image));
	}
	UT_ASSERT(G_OBJECT(m_image)->ref_count == 1);

	UT_ASSERT(G_OBJECT(m_image)->ref_count == 1);

	// loaders that ignored the scaled-decode hint deliver the full-size
	// image — bound it here (consumes our reference)
	m_image = xap_gtk_pixbuf_enforce_limits(m_image);
	if (!m_image)
	{
		UT_DEBUGMSG (("GdkPixbuf: image too large to keep!\n"));
		return false;
	}

	return true;
}


/* single horizontal+vertical box-blur pass over a pixbuf's RGB
 * channels in place; alpha is left untouched */
static void s_pixbufBoxBlur(GdkPixbuf * pPixbuf, gint radius)
{
	const gint w = gdk_pixbuf_get_width(pPixbuf);
	const gint h = gdk_pixbuf_get_height(pPixbuf);
	const gint nc = gdk_pixbuf_get_n_channels(pPixbuf);
	const gint stride = gdk_pixbuf_get_rowstride(pPixbuf);
	guchar * px = gdk_pixbuf_get_pixels(pPixbuf);
	if (radius < 1 || w < 2 || h < 2)
		return;

	std::vector<guchar> tmp(static_cast<size_t>(stride) * h);
	const gint cch = nc > 3 ? 3 : nc;
	/* horizontal pass */
	for (gint y = 0; y < h; ++y)
	{
		guchar * row = px + y * stride;
		guchar * dst = tmp.data() + y * stride;
		for (gint c = 0; c < cch; ++c)
		{
			gint sum = 0, count = 0;
			for (gint k = -radius; k <= radius; ++k)
			{
				if (k < w) { sum += row[k * nc + c]; ++count; }
			}
			for (gint x = 0; x < w; ++x)
			{
				dst[x * nc + c] = static_cast<guchar>(sum / count);
				if (x + radius + 1 < w) { sum += row[(x + radius + 1) * nc + c]; ++count; }
				if (x - radius >= 0)    { sum -= row[(x - radius) * nc + c];     --count; }
			}
		}
	}
	/* vertical pass back into the pixbuf */
	for (gint x = 0; x < w; ++x)
	{
		for (gint c = 0; c < cch; ++c)
		{
			gint sum = 0, count = 0;
			for (gint k = -radius; k <= radius; ++k)
			{
				if (k < h) { sum += tmp[k * stride + x * nc + c]; ++count; }
			}
			for (gint y = 0; y < h; ++y)
			{
				px[y * stride + x * nc + c] =
					static_cast<guchar>(sum / count);
				if (y + radius + 1 < h) { sum += tmp[(y + radius + 1) * stride + x * nc + c]; ++count; }
				if (y - radius >= 0)    { sum -= tmp[(y - radius) * stride + x * nc + c];     --count; }
			}
		}
	}
}

/* 1.0 when pixel (x+.5, y+.5) is inside the named crop shape, else 0 */
static inline double s_cropShapeCover(int shape, double nx, double ny,
									  double w, double h)
{
	switch (shape)
	{
	case 1: /* ellipse */
	{
		double dx = (nx - 0.5) * 2.0, dy = (ny - 0.5) * 2.0;
		return (dx * dx + dy * dy <= 1.0) ? 1.0 : 0.0;
	}
	case 2: /* rounded rectangle — corner arcs at 15% of the short side */
	{
		double r = (w < h ? w : h) * 0.15;
		double ax = nx * w, ay = ny * h;
		double cx = ax < r ? r : (ax > w - r ? w - r : ax);
		double cy = ay < r ? r : (ay > h - r ? h - r : ay);
		double dx = ax - cx, dy = ay - cy;
		if (ax >= r && ax <= w - r) dx = 0.0;
		if (ay >= r && ay <= h - r) dy = 0.0;
		return (dx * dx + dy * dy <= r * r) ? 1.0 : 0.0;
	}
	case 3: /* diamond */
		return (fabs(nx - 0.5) + fabs(ny - 0.5) <= 0.5) ? 1.0 : 0.0;
	case 4: /* triangle — apex at top centre, base along the bottom */
		return (ny >= fabs(nx - 0.5) * 2.0) ? 1.0 : 0.0;
	default:
		return 1.0;
	}
}

/*!
 * DrawingML a:blip picture effects and Picture Format corrections:
 * remap the pixbuf's pixels in place.  Order: geometry (flip, rotate)
 * first, then blur/sharpen convolution, then the canonical ECMA-376
 * color order — luminance, grayscale/sepia/biLevel, duotone — the
 * crop-shape alpha mask, and finally the fixed alpha modulation.
 */
void GR_UnixImage::applyBlipEffects(const GR_BlipEffects & fx)
{
	if (!m_image || !fx.any())
		return;

	if (fx.flipH || fx.flipV)
	{
		if (fx.flipH)
		{
			GdkPixbuf * p = gdk_pixbuf_flip(m_image, TRUE);
			if (p) { g_object_unref(G_OBJECT(m_image)); m_image = p; }
		}
		if (fx.flipV)
		{
			GdkPixbuf * p = gdk_pixbuf_flip(m_image, FALSE);
			if (p) { g_object_unref(G_OBJECT(m_image)); m_image = p; }
		}
	}
	if (fx.rotate)
	{
		/* our value is degrees clockwise; gdkpixbuf names the 90/180/
		 * 270 constants counterclockwise-first */
		GdkPixbufRotation rot = GDK_PIXBUF_ROTATE_NONE;
		switch (((fx.rotate % 360) + 360) % 360)
		{
		case 90:  rot = GDK_PIXBUF_ROTATE_CLOCKWISE;        break;
		case 180: rot = GDK_PIXBUF_ROTATE_UPSIDEDOWN;       break;
		case 270: rot = GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE; break;
		default: break;
		}
		if (rot != GDK_PIXBUF_ROTATE_NONE)
		{
			GdkPixbuf * p = gdk_pixbuf_rotate_simple(m_image, rot);
			if (p) { g_object_unref(G_OBJECT(m_image)); m_image = p; }
		}
	}

	const bool bSoft = (fx.blur >= 0.0) || (fx.sharpen > 0.0);
	if (bSoft)
	{
		gint rad = static_cast<gint>(fx.blur >= 0.0 ? fx.blur + 0.5 : 1);
		if (rad < 1) rad = 1;
		if (fx.sharpen > 0.0)
		{
			/* unsharp mask: original + amount*(original - blurred) —
			 * the blur radius tracks any declared soften so both
			 * corrections compose like Word's gallery presets */
			GdkPixbuf * pBlur = gdk_pixbuf_copy(m_image);
			if (pBlur)
			{
				s_pixbufBoxBlur(pBlur, rad);
				const gint w = gdk_pixbuf_get_width(m_image);
				const gint h = gdk_pixbuf_get_height(m_image);
				const gint nc = gdk_pixbuf_get_n_channels(m_image);
				const gint stride = gdk_pixbuf_get_rowstride(m_image);
				const gint bstride = gdk_pixbuf_get_rowstride(pBlur);
				const gint bnc = gdk_pixbuf_get_n_channels(pBlur);
				guchar * px = gdk_pixbuf_get_pixels(m_image);
				const guchar * bpx = gdk_pixbuf_get_pixels(pBlur);
				const gint cch = nc > 3 ? 3 : nc;
				for (gint y = 0; y < h; ++y)
					for (gint x = 0; x < w; ++x)
						for (gint c = 0; c < cch; ++c)
						{
							double v = px[y * stride + x * nc + c] +
								fx.sharpen * (px[y * stride + x * nc + c] -
											  bpx[y * bstride + x * bnc + c]);
							px[y * stride + x * nc + c] =
								v < 0.0 ? 0 : (v > 255.0 ? 255
								   : static_cast<guchar>(v + 0.5));
						}
				g_object_unref(pBlur);
			}
		}
		if (fx.blur >= 0.0)
			s_pixbufBoxBlur(m_image, rad);
	}

	/* a:alphaModFix or a crop-shape mask on an opaque image still
	 * needs an alpha channel — add one (all pixels start opaque) */
	if ((fx.alphaMod >= 0.0 || fx.cropShape) &&
		!gdk_pixbuf_get_has_alpha(m_image))
	{
		GdkPixbuf * pAlpha = gdk_pixbuf_add_alpha(m_image,
												  FALSE, 0, 0, 0);
		if (pAlpha)
		{
			g_object_unref(G_OBJECT(m_image));
			m_image = pAlpha;
		}
	}

	const gint w = gdk_pixbuf_get_width(m_image);
	const gint h = gdk_pixbuf_get_height(m_image);
	const gint nChannels = gdk_pixbuf_get_n_channels(m_image);
	const gint stride = gdk_pixbuf_get_rowstride(m_image);
	const bool hasAlpha = gdk_pixbuf_get_has_alpha(m_image);
	guchar * pixels = gdk_pixbuf_get_pixels(m_image);

	const double lo_r = fx.duoLo.m_red, lo_g = fx.duoLo.m_grn,
		lo_b = fx.duoLo.m_blu;
	const double hi_r = fx.duoHi.m_red, hi_g = fx.duoHi.m_grn,
		hi_b = fx.duoHi.m_blu;

	for (gint y = 0; y < h; ++y)
	{
		guchar * row = pixels + y * stride;
		for (gint x = 0; x < w; ++x)
		{
			guchar * p = row + x * nChannels;
			double r = p[0], g = p[1], b = p[2];

			/* a:lum: bright lerps toward white (positive) or black
			 * (negative); contrast pivots the result on mid-gray */
			if (fx.lum)
			{
				double br = fx.lumBright;
				double ct = 1.0 + fx.lumContrast;
				auto adj = [br, ct](double v) -> double {
					v = br >= 0.0 ? v + (255.0 - v) * br
								  : v * (1.0 + br);
					return (v - 127.5) * ct + 127.5;
				};
				r = adj(r);
				g = adj(g);
				b = adj(b);
			}

			/* a:grayscl: Rec.601 luminance into all channels */
			if (fx.grayscale)
			{
				double lum = 0.299 * r + 0.587 * g + 0.114 * b;
				r = g = b = lum;
			}

			/* sepia-tone color mode: the classic sepia matrix */
			if (fx.sepia)
			{
				double tr = 0.393 * r + 0.769 * g + 0.189 * b;
				double tg = 0.349 * r + 0.686 * g + 0.168 * b;
				double tb = 0.272 * r + 0.534 * g + 0.131 * b;
				r = tr; g = tg; b = tb;
			}

			/* a:biLevel: luminance threshold to pure black or white */
			if (fx.bilevel)
			{
				double lum = 0.299 * r + 0.587 * g + 0.114 * b;
				r = g = b = (lum >= fx.biThresh * 255.0) ? 255.0 : 0.0;
			}

			/* a:duotone: the pixel's luminance lerps between the two
			 * declared colors */
			if (fx.duotone)
			{
				double t = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0;
				r = lo_r + (hi_r - lo_r) * t;
				g = lo_g + (hi_g - lo_g) * t;
				b = lo_b + (hi_b - lo_b) * t;
			}

			p[0] = r < 0.0 ? 0 : (r > 255.0 ? 255 : static_cast<guchar>((r + 0.5)));
			p[1] = g < 0.0 ? 0 : (g > 255.0 ? 255 : static_cast<guchar>((g + 0.5)));
			p[2] = b < 0.0 ? 0 : (b > 255.0 ? 255 : static_cast<guchar>((b + 0.5)));
			if (hasAlpha)
			{
				double a = p[3];
				if (fx.cropShape)
					a *= s_cropShapeCover(fx.cropShape,
										  (x + 0.5) / w, (y + 0.5) / h,
										  w, h);
				if (fx.alphaMod >= 0.0)
					a *= fx.alphaMod;
				p[3] = a < 0.0 ? 0 : (a > 255.0 ? 255 : static_cast<guchar>((a + 0.5)));
			}
		}
	}
}

void GR_UnixImage::cairoSetSource(cairo_t * cr)
{
	UT_return_if_fail(m_image);
	double scaleX = static_cast<double>(getDisplayWidth() )/ static_cast<double>(gdk_pixbuf_get_width (m_image));
	double scaleY = static_cast<double>(getDisplayHeight() )/ static_cast<double>(gdk_pixbuf_get_height(m_image));
	cairo_scale(cr, scaleX, scaleY);
	xap_gdk_cairo_set_source_pixbuf(cr, m_image, 0, 0);
}
