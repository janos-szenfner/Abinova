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

/* unit tests for the cairo-independent parts of af/gr: transform
 * math, char-width caches, image bookkeeping (transparent outline,
 * buffer sniffing), the XP render-info item/buffer math, the painter
 * shim, the graphics factory, and the null-graphics unit conversion
 * paths.  The GR_MathTypesetter tests drive real parsing plus layout
 * and rendering against an in-memory cairo image surface -- still no
 * display required. */

#include <string.h>
#include <math.h>

#include <glib.h>
#include <cairo.h>

#include "tf_test.h"

#include "xap_App.h"

#include "ev_EditBits.h"
#include "gr_CairoNullGraphics.h"
#include "gr_CharWidths.h"
#include "gr_CharWidthsCache.h"
#include "gr_DrawArgs.h"
#include "gr_EmbedManager.h"
#include "gr_Graphics.h"
#include "gr_Image.h"
#include "gr_MathTypesetter.h"
#include "gr_Painter.h"
#include "gr_RenderInfo.h"
#include "gr_Transform.h"
#include "gr_VectorImage.h"

#define TFSUITE "core.af.gr.primitives"

namespace {

/* The GR_Graphics drawing entry points are protected -- production
 * code reaches them through GR_Painter.  For direct-call coverage we
 * promote them on a CairoNull_Graphics subclass. */
class TestNullGraphics : public CairoNull_Graphics
{
public:
	using GR_Graphics::_tduY;
	using GR_Graphics::_tduYD;
	using GR_Graphics::_tduR;
	using GR_Graphics::setClipRect;
	using GR_Graphics::setColor;
	using GR_Graphics::getColor;
	using GR_Graphics::setColor3D;
	using GR_Graphics::getColor3D;
	using GR_Graphics::fillRect;
	using GR_Graphics::xorRect;
	using GR_Graphics::invertRect;
	using GR_Graphics::clearArea;
	using GR_Graphics::scroll;
	using GR_Graphics::queueDraw;
	using GR_Graphics::drawLine;
	using GR_Graphics::xorLine;
	using GR_Graphics::setLineWidth;
	using GR_Graphics::setLineProperties;
	using GR_Graphics::polyLine;
	using GR_Graphics::polygon;
	using GR_Graphics::drawChars;
	using GR_Graphics::drawCharsRelativeToBaseline;
	using GR_Graphics::drawGlyph;
	using GR_Graphics::drawImage;
	using GR_Graphics::genImageFromRectangle;
	using GR_Graphics::saveRectangle;
	using GR_Graphics::restoreRectangle;

	/* call the base (non-overridden) implementations directly */
	void baseDrawImage(GR_Image * pImg, UT_sint32 x, UT_sint32 y)
		{ GR_Graphics::drawImage(pImg, x, y); }
	GR_Image * baseCreateNewImage(const char* pszName,
								  const UT_ConstByteBufPtr & pBB,
								  const std::string& mimetype,
								  UT_sint32 iWidth, UT_sint32 iHeight,
								  GR_Image::GRType iType)
		{ return GR_Graphics::createNewImage(pszName, pBB, mimetype,
											iWidth, iHeight, iType); }
};

static TestNullGraphics * tf_null_graphics(void)
{
	static TestNullGraphics * s_pG = nullptr;
	if (!s_pG) {
		s_pG = new TestNullGraphics();
	}
	return s_pG;
}

/* ------------------------------------------------------------------ */
/* a GR_Font with a deterministic width table                           */
/* ------------------------------------------------------------------ */

class TestFont : public GR_Font
{
public:
	TestFont()
	{
		m_hashKey = "covtest-font";
		m_eType = GR_FONT_UNIX;
	}

	UT_sint32 measureUnremappedCharForCache(UT_UCS4Char cChar) const override
	{
		++m_calls;
		if (cChar == 0x0E00)
			return GR_CW_ABSENT;
		return 100 + (cChar & 0xF);
	}
	bool glyphBox(UT_UCS4Char /*g*/, UT_Rect & rec, GR_Graphics *) override
	{
		rec.left = 1;
		rec.top = 2;
		rec.width = 3;
		rec.height = 4;
		return true;
	}
	mutable int m_calls = 0;
};

/* ------------------------------------------------------------------ */
/* a GR_Image with a fixed transparency mask                            */
/* ------------------------------------------------------------------ */

class TestImage : public GR_Image
{
public:
	TestImage(UT_sint32 w, UT_sint32 h, bool bAlpha)
		: m_bAlpha(bAlpha)
	{
		setDisplaySize(w, h);
		renderCount = 0;
	}

	bool convertToBuffer(UT_ConstByteBufPtr & ppBB) const override
	{
		auto bb = UT_ByteBufPtr(new UT_ByteBuf);
		bb->append(reinterpret_cast<const UT_Byte*>("img"), 3);
		ppBB = bb;
		return true;
	}
	bool convertFromBuffer(const UT_ConstByteBufPtr &,
						   const std::string&,
						   UT_sint32 w, UT_sint32 h) override
	{
		setDisplaySize(w, h);
		return true;
	}
	GR_Image * createImageSegment(GR_Graphics *, const UT_Rect &) override
	{
		return new TestImage(2, 2, m_bAlpha);
	}
	bool hasAlpha(void) const override { return m_bAlpha; }
	bool isTransparentAt(UT_sint32 x, UT_sint32 /*y*/) override
	{
		/* transparent edges: 5 columns on each side of a 20-wide image */
		return (x < 5) || (x >= getDisplayWidth() - 5);
	}

	void name(const char * n) { setName(n); }
	void name(const UT_String & n) { setName(n); }

	int renderCount;
private:
	bool m_bAlpha;
};

/* ------------------------------------------------------------------ */
/* expose the protected guts of GR_XPRenderInfo for the buffer tests    */
/* ------------------------------------------------------------------ */

class TestXPItem : public GR_XPItem
{
public:
	TestXPItem(GR_ScriptType t) : GR_XPItem(t) {}
};

class TestXPRenderInfo : public GR_XPRenderInfo
{
public:
	TestXPRenderInfo(GR_ScriptType t) : GR_XPRenderInfo(t) {}

	void fill(const UT_UCS4Char * s, const UT_sint32 * w,
			  UT_uint32 len, UT_uint32 bufsz)
	{
		m_pChars = new UT_UCS4Char[bufsz + 1];
		m_pWidths = new UT_sint32[bufsz + 1];
		for (UT_uint32 i = 0; i < len; i++) {
			m_pChars[i] = s[i];
			m_pWidths[i] = w[i];
		}
		m_pChars[len] = 0;
		m_iLength = len;
		m_iTotalLength = len;
		m_iBufferSize = bufsz;
	}
	UT_UCS4Char * chars() { return m_pChars; }
	UT_sint32 * widths() { return m_pWidths; }
};

} /* anonymous namespace */

/* ------------------------------------------------------------------ */
/* GR_Transform                                                        */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("GR_Transform")
{
	GR_Transform id;
	TFPASS(id == GR_Transform(1, 0, 0, 1, 0, 0));
	TFPASS(!(id != GR_Transform(1, 0, 0, 1, 0, 0)));
	TFPASS(id != GR_Transform(2, 0, 0, 1, 0, 0));

	TFPASS(GR_Transform::scale(2, 3) == GR_Transform(2, 0, 0, 3, 0, 0));
	TFPASS(GR_Transform::translate(5, -2) == GR_Transform(1, 0, 0, 1, 5, -2));

	GR_Transform rot = GR_Transform::rotate(90);
	TFPASS(fabs(rot.getA()) < 1e-10);
	TFPASS(fabs(rot.getB() - 1) < 1e-10);
	TFPASS(fabs(rot.getC() + 1) < 1e-10);
	TFPASS(fabs(rot.getD()) < 1e-10);

	/* affine multiply: scale then translate vs. the other way */
	GR_Transform st = GR_Transform::scale(2, 2) + GR_Transform::translate(1, 0);
	TFPASS(st == GR_Transform(2, 0, 0, 2, 1, 0));
	GR_Transform ts = GR_Transform::translate(1, 0) + GR_Transform::scale(2, 2);
	TFPASS(ts == GR_Transform(2, 0, 0, 2, 2, 0));
	TFPASS(!(st == ts));

	/* copy ctor + assignment + += */
	GR_Transform cp(st);
	TFPASS(cp == st);
	GR_Transform asg;
	asg = ts;
	TFPASS(asg == ts);
	asg += GR_Transform();
	TFPASS(asg == ts);
}

/* ------------------------------------------------------------------ */
/* GR_CharWidths / GR_CharWidthsCache / GR_Font                        */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("GR_CharWidths")
{
	GR_CharWidths cw;
	TFPASS(cw.getWidth('a') == GR_CW_UNKNOWN);
	TFPASS(cw.getWidth(0x1234) == GR_CW_UNKNOWN);

	cw.setWidth('a', 55);
	TFPASS(cw.getWidth('a') == 55);

	/* a high character allocates a sparse hi-byte page */
	cw.setWidth(0x1234, 77);
	TFPASS(cw.getWidth(0x1234) == 77);
	TFPASS(cw.getWidth(0x12FF) == GR_CW_UNKNOWN);

	cw.zeroWidths();
	TFPASS(cw.getWidth('a') == GR_CW_UNKNOWN);
	TFPASS(cw.getWidth(0x1234) == GR_CW_UNKNOWN);
}

TFTEST_MAIN("GR_CharWidthsCache via GR_Font")
{
	TestFont font;
	TFPASS(font.hashKey() == "covtest-font");
	TFPASS(font.getAllocNumber() > 0);
	TFPASS(font.getType() == GR_FONT_UNIX);

	/* hardwired zero-width characters never hit the cache */
	TFPASS(font.getCharWidthFromCache(0xFEFF) == 0);
	TFPASS(font.getCharWidthFromCache(0x200B) == 0);
	TFPASS(font.getCharWidthFromCache(UCS_LIGATURE_PLACEHOLDER) == 0);
	TFPASS(font.m_calls == 0);

	/* first lookup measures + caches; second is a cache hit */
	UT_sint32 w1 = font.getCharWidthFromCache('x');
	TFPASS(w1 == 100 + ('x' & 0xF));
	TFPASS(font.m_calls == 1);
	TFPASS(font.getCharWidthFromCache('x') == w1);
	TFPASS(font.m_calls == 1);

	/* absent glyph reporting */
	TFPASS(font.getCharWidthFromCache(0x0E00) == GR_CW_ABSENT);
	TFPASS(!font.doesGlyphExist(0x0E00));
	TFPASS(font.doesGlyphExist('x'));
	TFPASS(!GR_Font::s_doesGlyphExist(0x0E00, &font));
	TFPASS(GR_Font::s_doesGlyphExist('x', &font));
	TFPASS(!GR_Font::s_doesGlyphExist('x', nullptr));

	UT_Rect box;
	TFPASS(font.glyphBox('a', box, nullptr));
	TFPASS(box.left == 1 && box.top == 2 && box.width == 3 && box.height == 4);
}

/* ------------------------------------------------------------------ */
/* GR_Image / GR_BlipEffects                                           */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("GR_Image")
{
	TestImage img(20, 10, false);
	img.name("pic1");
	char buf[64];
	img.getName(buf);
	TFPASS(!strcmp(buf, "pic1"));

	std::string s;
	img.getName(s);
	TFPASS(s == "pic1");
	UT_String us;
	img.getName(us);
	TFPASS(us == "pic1");
	img.name(UT_String("renamed"));
	img.getName(buf);
	TFPASS(!strcmp(buf, "renamed"));

	TFPASS(img.getDisplayWidth() == 20);
	TFPASS(img.getDisplayHeight() == 10);
	TFPASS(img.getType() == GR_Image::GRT_Raster);
	TFPASS(!img.hasAlpha());
	TFPASS(!img.isOutLinePresent());

	/* base render() is a no-op stub */
	TestNullGraphics * pG = tf_null_graphics();
	TFPASS(pG != nullptr);
	TFPASS(!img.render(pG, 20, 10));

	/* drawImage() routes through render() on non-null images; the
	 * CairoNull override is a no-op, so call the base impl to cover
	 * the dispatch */
	pG->baseDrawImage(&img, 0, 0);
	pG->baseDrawImage(nullptr, 0, 0);
	pG->drawImage(&img, 0, 0);

	/* blip effects are a no-op on the base class */
	GR_BlipEffects fx;
	TFPASS(!fx.any());
	fx.grayscale = true;
	TFPASS(fx.any());
	fx = GR_BlipEffects();
	fx.alphaMod = 0.5;
	TFPASS(fx.any());
	fx = GR_BlipEffects();
	fx.lum = true;
	TFPASS(fx.any());
	fx = GR_BlipEffects();
	fx.duotone = true;
	TFPASS(fx.any());
	img.applyBlipEffects(fx);

	/* convertToBuffer/convertFromBuffer round-trip */
	UT_ConstByteBufPtr bb;
	TFPASS(img.convertToBuffer(bb));
	TFPASS(bb->getLength() == 3);
	TFPASS(img.convertFromBuffer(bb, "image/x-test", 8, 4));
	TFPASS(img.getDisplayWidth() == 8 && img.getDisplayHeight() == 4);

	/* scaleImageTo uses the graphics unit conversion */
	img.setDisplaySize(20, 10);
	UT_Rect rec(0, 0, 1440, 720);
	img.scaleImageTo(pG, rec);
	TFPASS(img.getDisplayWidth() == pG->tdu(1440));
	TFPASS(img.getDisplayHeight() == pG->tdu(720));

	UT_ConstByteBufPtr seg;
	TestImage * pSeg = static_cast<TestImage *>(img.createImageSegment(pG, rec));
	TFPASS(pSeg != nullptr);
	delete pSeg;
}

TFTEST_MAIN("GR_Image::getBufferType")
{
	const char png[] = "\211PNG\r\n\032\n";
	auto bbPNG = UT_ByteBufPtr(new UT_ByteBuf);
	bbPNG->append(reinterpret_cast<const UT_Byte*>(png), 8);
	TFPASS(GR_Image::getBufferType(bbPNG) == GR_Image::GRT_Raster);

	const char svg[] = "<?xml version=\"1.0\"?><svg xmlns=\"http://www.w3.org/2000/svg\"/>";
	auto bbSVG = UT_ByteBufPtr(new UT_ByteBuf);
	bbSVG->append(reinterpret_cast<const UT_Byte*>(svg), sizeof(svg) - 1);
	TFPASS(GR_Image::getBufferType(bbSVG) == GR_Image::GRT_Vector);

	const char jnk[] = "not an image at all";
	auto bbJunk = UT_ByteBufPtr(new UT_ByteBuf);
	bbJunk->append(reinterpret_cast<const UT_Byte*>(jnk), sizeof(jnk) - 1);
	TFPASS(GR_Image::getBufferType(bbJunk) == GR_Image::GRT_Unknown);

	auto bbShort = UT_ByteBufPtr(new UT_ByteBuf);
	bbShort->append(reinterpret_cast<const UT_Byte*>("ab"), 2);
	TFPASS(GR_Image::getBufferType(bbShort) == GR_Image::GRT_Unknown);
}

TFTEST_MAIN("GR_Image outline + offsets")
{
	TestNullGraphics * pG = tf_null_graphics();
	TFPASS(pG != nullptr);

	/* no alpha -> the pad is returned untouched, no outline work */
	TestImage solid(20, 10, false);
	TFPASS(solid.GetOffsetFromLeft(pG, 100, 0, 200) == 100);
	TFPASS(solid.GetOffsetFromRight(pG, 100, 0, 200) == 100);
	TFPASS(!solid.isOutLinePresent());

	TestImage img(20, 10, true);
	img.GenerateOutline();
	TFPASS(img.isOutLinePresent());

	/* the left outline sits at x=5 (5 transparent columns); the clear
	 * distance is the pad minus the outline x position */
	UT_sint32 off = img.GetOffsetFromLeft(pG, 1000, 0, 200);
	TFPASS(off == pG->tlu(pG->tdu(1000) - 5));

	/* right outline mirrors the same mask */
	off = img.GetOffsetFromRight(pG, 1000, 0, 200);
	TFPASS(off == pG->tlu(pG->tdu(1000) - (20 - 14)));

	/* a height covering only part of the rows exercises the
	 * projected-distance branch */
	off = img.GetOffsetFromLeft(pG, 1000, 0, 100);
	TFPASS(off == pG->tlu(pG->tdu(1000) - 5));

	img.DestroyOutline();
	TFPASS(!img.isOutLinePresent());
}

/* ------------------------------------------------------------------ */
/* GR_VectorImage                                                      */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("GR_VectorImage")
{
	const char svg[] = "<svg xmlns=\"http://www.w3.org/2000/svg\"/>";
	auto bb = UT_ByteBufPtr(new UT_ByteBuf);
	bb->append(reinterpret_cast<const UT_Byte*>(svg), sizeof(svg) - 1);

	GR_VectorImage vimg("v1");
	TFPASS(vimg.getType() == GR_Image::GRT_Vector);
	TFPASS(!vimg.hasAlpha());
	TFPASS(vimg.convertFromBuffer(bb, "image/svg+xml", 40, 30));
	TFPASS(vimg.getDisplayWidth() == 40);
	TFPASS(vimg.getDisplayHeight() == 30);

	UT_ConstByteBufPtr out;
	TFPASS(vimg.convertToBuffer(out));
	TFPASS(out->getLength() == sizeof(svg) - 1);

	/* base-class createNewImage() sniffs the buffer type */
	TestNullGraphics * pG = tf_null_graphics();
	GR_Image * p = pG->baseCreateNewImage(
		"n", bb, "image/svg+xml", 40, 30, GR_Image::GRT_Unknown);
	TFPASS(p != nullptr);
	TFPASS(p->getType() == GR_Image::GRT_Vector);
	delete p;

	/* explicit vector type skips the sniff */
	p = pG->baseCreateNewImage(
		"n", bb, "image/svg+xml", 40, 30, GR_Image::GRT_Vector);
	TFPASS(p != nullptr);
	delete p;

	/* the CairoNull override declines buffer images entirely */
	p = pG->createNewImage("n", bb, "image/svg+xml", 40, 30,
						   GR_Image::GRT_Vector);
	TFPASS(p == nullptr);
}

/* ------------------------------------------------------------------ */
/* GR_Itemization / GR_XPRenderInfo                                    */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("GR_Itemization")
{
	GR_Itemization I;
	TFPASS(I.getItemCount() == 0);
	TFPASS(I.getLang() == nullptr);
	TFPASS(I.getEmbedingLevel() == 0);
	TFPASS(I.getShowControlChars() == false);

	/* items are owned by the itemization (clear() deletes them) */
	I.addItem(0, new TestXPItem(GRScriptType_Undefined));
	I.addItem(3, new TestXPItem(GRScriptType_Void));
	I.addItem(7, new TestXPItem(GRScriptType_Void));

	TFPASS(I.getItemCount() == 3);
	TFPASS(I.getNthOffset(1) == 3);
	TFPASS(I.getNthItem(0)->getType() == GRScriptType_Undefined);
	TFPASS(I.getNthType(2) == GRScriptType_Void);
	TFPASS(I.getNthLength(0) == 3);
	TFPASS(I.getNthLength(1) == 4);
	/* the trailing sentinel has no length */
	TFPASS(I.getNthLength(2) == 0);

	I.insertItem(1, 2, new TestXPItem(GRScriptType_Undefined));
	TFPASS(I.getItemCount() == 4);
	TFPASS(I.getNthOffset(1) == 2);
	TFPASS(I.getNthLength(0) == 2);

	I.setEmbedingLevel(2);
	I.setDirOverride(UT_BIDI_RTL);
	I.setShowControlChars(true);
	I.setLang("en-US");
	I.setFont(nullptr);
	TFPASS(I.getEmbedingLevel() == 2);
	TFPASS(I.getDirOverride() == UT_BIDI_RTL);
	TFPASS(I.getShowControlChars() == true);
	TFPASS(!strcmp(I.getLang(), "en-US"));
	TFPASS(I.getFont() == nullptr);

	I.clear();
	TFPASS(I.getItemCount() == 0);
}

TFTEST_MAIN("GR_XPRenderInfo append/split/cut")
{
	const UT_UCS4Char s1[] = {'a', 'b', 'c'};
	const UT_sint32   w1[] = {10, 20, 30};
	const UT_UCS4Char s2[] = {'d', 'e'};
	const UT_sint32   w2[] = {40, 50};

	/* reuse path: the buffer is already big enough */
	{
		TestXPRenderInfo ri1(GRScriptType_Undefined), ri2(GRScriptType_Undefined);
		ri1.fill(s1, w1, 3, 8);
		ri2.fill(s2, w2, 2, 8);
		TFPASS(ri1.append(ri2));
		/* append() grows the buffers + m_iTotalLength; m_iLength is
		 * left for the caller to adjust */
		TFPASS(ri1.m_iTotalLength == 5);
		TFPASS(ri1.chars()[4] == 'e');
		TFPASS(ri1.widths()[3] == 40);
	}

	/* realloc path: buffer too small -> a bigger span is allocated */
	{
		TestXPRenderInfo ri1(GRScriptType_Undefined), ri2(GRScriptType_Undefined);
		ri1.fill(s1, w1, 3, 3);
		ri2.fill(s2, w2, 2, 2);
		TFPASS(ri1.append(ri2));
		TFPASS(ri1.m_iTotalLength == 5);
		TFPASS(ri1.chars()[0] == 'a' && ri1.chars()[4] == 'e');
	}

	/* reverse append */
	{
		TestXPRenderInfo ri1(GRScriptType_Undefined), ri2(GRScriptType_Undefined);
		ri1.fill(s1, w1, 3, 3);
		ri2.fill(s2, w2, 2, 2);
		TFPASS(ri1.append(ri2, true));
		TFPASS(ri1.m_iTotalLength == 5);
		TFPASS(ri1.chars()[0] == 'd' && ri1.chars()[4] == 'c');
	}

	/* justification info merges */
	{
		TestXPRenderInfo ri1(GRScriptType_Undefined), ri2(GRScriptType_Undefined);
		ri1.fill(s1, w1, 3, 8);
		ri2.fill(s2, w2, 2, 8);
		ri1.m_iJustificationPoints = 2;
		ri1.m_iJustificationAmount = 10;
		ri2.m_iJustificationPoints = 1;
		ri2.m_iJustificationAmount = 5;
		ri2.m_bLastOnLine = true;
		TFPASS(ri1.append(ri2));
		TFPASS(ri1.m_iJustificationPoints == 3);
		TFPASS(ri1.m_iJustificationAmount == 15);
		TFPASS(ri1.m_bLastOnLine);
	}

	/* split() at m_iOffset divides chars/widths between the pair */
	{
		const UT_UCS4Char s3[] = {'a', 'b', 'c', 'd', 'e'};
		const UT_sint32   w3[] = {10, 20, 30, 40, 50};
		TestXPRenderInfo ri(GRScriptType_Undefined);
		ri.fill(s3, w3, 5, 5);
		ri.m_iOffset = 2;
		ri.m_pItem = new TestXPItem(GRScriptType_Undefined);

		GR_RenderInfo * pri = nullptr;
		TFPASS(ri.split(pri));
		TFPASS(pri != nullptr);
		TFPASS(ri.m_iLength == 2);
		TFPASS(ri.chars()[0] == 'a' && ri.chars()[1] == 'b');
		/* the new half is a GR_XPRenderInfo, not our subclass -- check
		 * the public fields only */
		TFPASS(pri->m_iLength == 3);
		TFPASS(pri->m_pItem != nullptr);
		delete pri->m_pItem;
		delete pri;
		delete ri.m_pItem;
	}

	/* cut() without a backing text iterator fails cleanly */
	{
		TestXPRenderInfo ri(GRScriptType_Undefined);
		ri.fill(s1, w1, 3, 3);
		TFPASS(!ri.cut(0, 1));
	}
}

/* ------------------------------------------------------------------ */
/* dg_DrawArgs / GR_EmbedManager / GR_GraphicsFactory                  */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("dg_DrawArgs")
{
	dg_DrawArgs da;
	TFPASS(da.pG == nullptr);
	TFPASS(da.xoff == 0 && da.yoff == 0);
	TFPASS(!da.bDirtyRunsOnly);
}

TFTEST_MAIN("GR_EmbedManager")
{
	TestNullGraphics * pG = tf_null_graphics();
	TFPASS(pG != nullptr);

	/* no plugin registered for a bogus object type -> the base
	 * "default" manager is created */
	GR_EmbedManager * em =
		XAP_App::getApp()->getEmbeddableManager(pG, "no.such.object.type");
	TFPASS(em != nullptr);
	TFPASS(em->getGraphics() == pG);
	em->initialize();
	TFPASS(!strcmp(em->getObjectType(), "default"));
	TFPASS(!strcmp(em->getMimeType(), "text/plain"));
	TFPASS(!strcmp(em->getMimeTypeDescription(), "plain text document"));
	TFPASS(!strcmp(em->getMimeTypeSuffix(), ".txt"));
	TFPASS(em->isDefault());
	TFPASS(!em->isEdittable(0));
	TFPASS(em->isResizeable(0));
	TFPASS(em->getDescent(0) == 0);
	TFPASS(em->getContextualMenu() == EV_EMC_EMBED);
	em->setColor(0, UT_RGBColor(1, 2, 3));
	em->setGraphics(pG);
	TFPASS(em->getGraphics() == pG);
	delete em;
}

TFTEST_MAIN("GR_GraphicsFactory")
{
	GR_GraphicsFactory f;
	TFPASS(f.getClassCount() == 0);
	TFPASS(!f.isRegistered(GRID_CAIRO_NULL));

	static GR_Graphics* (*allocfn)(GR_AllocInfo&) =
		[](GR_AllocInfo&) -> GR_Graphics* { return nullptr; };
	static const char* (*descfn)(void) =
		[](void) -> const char* { return "test graphics"; };

	TFPASS(f.registerClass(allocfn, descfn, GRID_QT));
	TFPASS(f.isRegistered(GRID_QT));
	TFPASS(f.getClassCount() == 1);
	TFPASS(!strcmp(f.getClassDescription(GRID_QT), "test graphics"));
	TFPASS(f.getClassDescription(GRID_COCOA) == nullptr);

	GR_CairoNullGraphicsAllocInfo ai;
	TFPASS(f.newGraphics(GRID_QT, ai) == nullptr); /* our stub alloc */
	TFPASS(f.newGraphics(GRID_COCOA, ai) == nullptr); /* unregistered */

	UT_uint32 plugId = f.registerPluginClass(allocfn, descfn);
	TFPASS(plugId > GRID_LAST_EXTENSION);
	TFPASS(f.isRegistered(plugId));

	f.registerAsDefault(GRID_QT, true);
	f.registerAsDefault(plugId, false);
	TFPASS(f.getDefaultClass(true) == GRID_QT);
	TFPASS(f.getDefaultClass(false) == plugId);

	TFPASS(f.unregisterClass(plugId));
	TFPASS(!f.isRegistered(plugId));
	TFPASS(!f.unregisterClass(plugId));

	/* the app factory has the real classes registered */
	GR_GraphicsFactory * appf = XAP_App::getApp()->getGraphicsFactory();
	TFPASS(appf != nullptr);
	TFPASS(appf->isRegistered(GRID_CAIRO_NULL));
	TFPASS(!strcmp(appf->getClassDescription(GRID_CAIRO_NULL),
				   "Cairo Null Graphics"));
}

/* ------------------------------------------------------------------ */
/* CairoNull_Graphics instance -- unit conversion + misc paths         */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("GR_Graphics unit conversion")
{
	TestNullGraphics * pG = tf_null_graphics();
	TFPASS(pG != nullptr);
	TFPASS(pG->getClassId() == GRID_CAIRO_NULL);
	TFPASS(pG->getDeviceResolution() > 0);
	TFPASS(GR_Graphics::getResolution() == UT_LAYOUT_RESOLUTION);

	UT_uint32 devRes = pG->getDeviceResolution();

	pG->setZoomPercentage(100);
	TFPASS(pG->getZoomPercentage() == 100);

	/* at 100% zoom, one layout inch equals the device resolution */
	TFPASS(pG->tdu(UT_LAYOUT_RESOLUTION) == static_cast<UT_sint32>(devRes));
	TFPASS(pG->tdu(0) == 0);
	/* tlu inverts tdu on exact layout-inch boundaries */
	TFPASS(pG->tlu(pG->tdu(UT_LAYOUT_RESOLUTION))
		   == static_cast<UT_sint32>(UT_LAYOUT_RESOLUTION));
	TFPASS(pG->tduD(UT_LAYOUT_RESOLUTION) > 0);
	TFPASS(pG->tluD(pG->tduD(500)) > 499.0);
	TFPASS(pG->tluD(pG->tduD(500)) < 501.0);

	/* zoom scales device units */
	pG->setZoomPercentage(200);
	TFPASS(pG->getZoomPercentage() == 200);
	TFPASS(pG->tdu(UT_LAYOUT_RESOLUTION) == static_cast<UT_sint32>(devRes * 2));
	pG->setZoomPercentage(100);

	/* scroll-offset compensated conversions */
	pG->setPrevXOffset(0);
	pG->setPrevYOffset(0);
	TFPASS(pG->_tduX(500) == pG->tdu(500));
	TFPASS(pG->_tduY(500) == pG->tdu(500));
	TFPASS(pG->_tduXD(500) == pG->tduD(500));
	TFPASS(pG->_tduYD(500) == pG->tduD(500));
	pG->setPrevXOffset(120);
	pG->setPrevYOffset(240);
	TFPASS(pG->_tduX(500) == pG->tdu(620) - pG->tdu(120));
	TFPASS(pG->_tduY(500) == pG->tdu(740) - pG->tdu(240));
	TFPASS(pG->getPrevXOffset() == 120);
	TFPASS(pG->getPrevYOffset() == 240);

	/* _tduR rounds up when the truncated conversion loses a unit */
	TFPASS(pG->_tduR(UT_LAYOUT_RESOLUTION) >=
		   static_cast<UT_sint32>(devRes));

	/* font units are zoom-independent */
	TFPASS(pG->ftlu(devRes) == UT_LAYOUT_RESOLUTION);
	TFPASS(pG->ftluD(devRes) == static_cast<double>(UT_LAYOUT_RESOLUTION));

	pG->setZoomPercentage(100);
	pG->setPrevXOffset(0);
	pG->setPrevYOffset(0);
}

TFTEST_MAIN("GR_Graphics misc queries")
{
	TestNullGraphics * pG = tf_null_graphics();
	TFPASS(pG != nullptr);

	TFPASS(pG->queryProperties(GR_Graphics::DGP_PAPER));
	TFPASS(!pG->queryProperties(GR_Graphics::DGP_SCREEN));
	TFPASS(!pG->queryProperties(GR_Graphics::DGP_OPAQUEOVERLAY));

	TFPASS(pG->startPrint());
	TFPASS(pG->startPage("p1", 1, true, 800, 600));
	TFPASS(pG->endPrint());
	pG->setPageSize(const_cast<char*>("A4"), 100, 200);
	pG->setPageCount(2);
	pG->flush();
	pG->invalidateCache();
	TFPASS(!pG->canQuickPrint());
	TFPASS(pG->getResolutionRatio() == 1.0);
	TFPASS(pG->getGUIFont() == nullptr);
	TFPASS(pG->getCapability() == GRCAP_UNKNOWN);

	pG->setColorSpace(GR_Graphics::GR_COLORSPACE_GRAYSCALE);
	TFPASS(pG->getColorSpace() == GR_Graphics::GR_COLORSPACE_COLOR);

	pG->setCursor(GR_Graphics::GR_CURSOR_IBEAM);
	TFPASS(pG->getCursor() == GR_Graphics::GR_CURSOR_INVALID);

	UT_RGBColor black(0, 0, 0);
	UT_RGBColor out(1, 2, 3);
	pG->setColor(black);
	pG->getColor(out);
	pG->setColor3D(GR_Graphics::CLR3D_Foreground);
	UT_RGBColor c3d;
	pG->getColor3D(GR_Graphics::CLR3D_BevelUp, c3d);
	pG->fillRect(GR_Graphics::CLR3D_Background, 0, 0, 10, 10);
	UT_Rect r3d(0, 0, 4, 4);
	pG->fillRect(GR_Graphics::CLR3D_Highlight, r3d);

	pG->setPortrait(true);
	TFPASS(pG->isPortrait());
	pG->setPortrait(false);
	TFPASS(!pG->isPortrait());

	pG->antiAliasAlways(true);
	TFPASS(pG->getAntiAliasAlways());
	pG->antiAliasAlways(false);
	TFPASS(!pG->getAntiAliasAlways());

	/* transform accept/reject round trip -- the base _setTransform
	 * declines so the transform must stay at identity */
	TFPASS(pG->getTransform() == GR_Transform());
	GR_Transform tr = GR_Transform::scale(2, 2);
	bool bOk = pG->setTransform(tr);
	if (bOk) {
		TFPASS(pG->getTransform() == tr);
	} else {
		TFPASS(pG->getTransform() == GR_Transform());
	}

	/* paint pairing (the null graphics' _begin/_endPaint are no-ops) */
	pG->beginPaint();
	pG->beginPaint();
	pG->endPaint();
	pG->endPaint();

	/* text-effects bag */
	GR_TextEffects fx;
	fx.m_bOutline = true;
	fx.m_colOutline = UT_RGBColor(9, 9, 9);
	pG->setTextEffects(&fx);
	TFPASS(pG->getTextEffects() != nullptr);
	TFPASS(pG->getTextEffects()->m_bOutline);
	pG->setTextEffects(nullptr);
	TFPASS(pG->getTextEffects() == nullptr);
}

TFTEST_MAIN("GR_Graphics clip + fill/xor + polygon")
{
	TestNullGraphics * pG = tf_null_graphics();
	TFPASS(pG != nullptr);

	UT_RGBColor red(255, 0, 0), blue(0, 0, 255);
	pG->setColor(blue);

	TFPASS(pG->getClipRect() == nullptr);
	TFPASS(!pG->getClipRectOptional().has_value());
	UT_Rect clip(10, 10, 50, 50);
	pG->setClipRect(&clip);
	UT_Rect clip2(20, 20, 60, 60);
	pG->setClipRect(&clip2);
	pG->setClipRect(nullptr);

	pG->fillRect(red, 0, 0, 30, 20);
	UT_Rect fr(5, 5, 10, 10);
	pG->fillRect(red, fr);
	pG->xorRect(1, 1, 8, 8);
	pG->xorRect(fr);
	pG->invertRect(&fr);
	pG->clearArea(0, 0, 5, 5);
	pG->scroll(3, 4);
	pG->scroll(0, 0, 1, 1, 10, 10);
	pG->queueDraw(&fr);

	pG->drawLine(0, 0, 9, 9);
	pG->xorLine(0, 9, 9, 0);
	pG->setLineWidth(2);
	pG->setLineProperties(1.5, GR_Graphics::JOIN_ROUND,
						  GR_Graphics::CAP_ROUND,
						  GR_Graphics::LINE_DOTTED);

	UT_Point pts[4] = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
	pG->polyLine(pts, 4);

	/* polygon() runs the real point-in-polygon scan over the bbox */
	UT_Point tri[3] = {{0, 0}, {8, 0}, {4, 8}};
	pG->polygon(red, tri, 3);
	UT_Point sq[4] = {{0, 0}, {6, 0}, {6, 6}, {0, 6}};
	pG->polygon(blue, sq, 4);
	/* degenerate inputs are rejected */
	pG->polygon(red, sq, 1);
	pG->polygon(red, nullptr, 4);

	const UT_UCS4Char txt[] = {'h', 'i'};
	pG->drawChars(txt, 0, 2, 5, 5);
	pG->drawCharsRelativeToBaseline(txt, 0, 2, 5, 20);
	pG->drawImage(nullptr, 0, 0);
	TFPASS(pG->genImageFromRectangle(fr) == nullptr);
	UT_Rect saved(0, 0, 2, 2);
	pG->saveRectangle(saved, 0);
	pG->restoreRectangle(0);
}

TFTEST_MAIN("GR_Graphics dimensions helpers")
{
	TestNullGraphics * pG = tf_null_graphics();
	TFPASS(pG != nullptr);

	/* invertDimension renders layout units back as a dim string */
	const char * sIn = pG->invertDimension(DIM_IN, UT_LAYOUT_RESOLUTION);
	TFPASS(sIn && strstr(sIn, "1.0") == sIn);
	const char * sCm = pG->invertDimension(DIM_CM, UT_LAYOUT_RESOLUTION);
	TFPASS(sCm && strstr(sCm, "2.54") == sCm);
	pG->invertDimension(DIM_PT, UT_LAYOUT_RESOLUTION);
	pG->invertDimension(DIM_PI, UT_LAYOUT_RESOLUTION);
	pG->invertDimension(DIM_none, 100);

	/* scaleDimensions: numeric left+width */
	UT_sint32 left = -1;
	UT_uint32 width = 0;
	TFPASS(pG->scaleDimensions("0.5in", "1.0in", 1440 * 3, &left, &width));
	TFPASS(left == 720);
	TFPASS(width == 1440);

	/* '*' width takes the remaining space */
	TFPASS(pG->scaleDimensions("0.5in", "*", 1440 * 3, &left, &width));
	TFPASS(left == 720);
	TFPASS(width == 1440 * 3 - 720);

	/* null out-params are allowed */
	TFPASS(pG->scaleDimensions("0.5in", "*", 1440, nullptr, nullptr));
}

TFTEST_MAIN("GR_Graphics fonts + measure")
{
	TestNullGraphics * pG = tf_null_graphics();
	TFPASS(pG != nullptr);

	/* findNearestFont resolves family names headlessly */
	const char * near = GR_Graphics::findNearestFont(
		"Serif", "normal", "normal", "normal", "normal", "12pt", "en-US");
	TFPASS(near != nullptr);

	GR_Font * f = pG->findFont("Serif", "normal", "normal", "normal",
							  "normal", "12pt", "en-US");
	TFPASS(f != nullptr);
	if (!f)
		return;

	pG->setFont(f);
	TFPASS(pG->getFontAscent() > 0);
	TFPASS(pG->getFontDescent() >= 0);
	TFPASS(pG->getFontHeight() >= pG->getFontAscent());
	TFPASS(pG->getFontAscent(f) == pG->getFontAscent());
	TFPASS(pG->getFontDescent(f) == pG->getFontDescent());
	TFPASS(pG->getFontHeight(f) == pG->getFontHeight());
	TFPASS(pG->getFontAscent(nullptr) == 0);
	TFPASS(pG->getFontDescent(nullptr) == 0);
	TFPASS(pG->getFontHeight(nullptr) == 0);

	const UT_UCS4Char txt[] = {'a', 'b', 'c'};
	UT_GrowBufElement widths[3] = {0, 0, 0};
	UT_uint32 w = pG->measureString(txt, 0, 3, widths);
	TFPASS(w > 0);

	UT_uint32 maxW = 0, maxH = 0;
	pG->getMaxCharacterDimension(txt, 3, maxW, maxH);
	TFPASS(maxW > 0);
	TFPASS(maxH > 0);

	std::vector<UT_sint32> coverage;
	pG->getCoverage(coverage);
	pG->clearFont();
	pG->getCoverage(coverage); /* no font -> early return */
}

/* ------------------------------------------------------------------ */
/* carets                                                              */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("GR_Graphics carets")
{
	TestNullGraphics * pG = tf_null_graphics();
	TFPASS(pG != nullptr);

	AllCarets * ac = pG->allCarets();
	TFPASS(ac != nullptr);
	TFPASS(ac->getBaseCaret() == nullptr);

	GR_Caret * c1 = pG->createCaret("caret1");
	TFPASS(c1 != nullptr);
	GR_Caret * c2 = pG->createCaret("caret2");
	TFPASS(c2 != nullptr);

	TFPASS(pG->getNthCaret(0) == c1);
	TFPASS(pG->getNthCaret(1) == c2);
	TFPASS(pG->getNthCaret(2) == nullptr);
	TFPASS(pG->getCaret("caret2") == c2);
	TFPASS(pG->getCaret("nope") == nullptr);

	ac->getBaseCaret();
	ac->enable();
	ac->setBlink(true);
	ac->setWindowSize(200, 100);
	ac->setCoords(10, 10, 20);
	UT_RGBColor cc(0, 200, 0);
	ac->setCoords(1, 2, 3, 4, 5, 6, true, &cc);
	ac->setInsertMode(false);
	ac->JustErase(10, 10);
	ac->forceDraw();
	ac->doBlinkIfNeeded();
	ac->setPendingBlink();
	ac->disable();
	ac->disable(true);
	pG->disableAllCarets();
	pG->enableAllCarets();

	pG->removeCaret("caret1");
	TFPASS(pG->getCaret("caret1") == nullptr);
	TFPASS(pG->getNthCaret(0) == c2);
	pG->removeCaret("caret2");
	TFPASS(pG->getNthCaret(0) == nullptr);
}

/* ------------------------------------------------------------------ */
/* GR_Painter (thin forwarding layer over GR_Graphics)                 */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("GR_Painter")
{
	TestNullGraphics * pG = tf_null_graphics();
	TFPASS(pG != nullptr);

	GR_Painter painter(pG);
	UT_RGBColor red(200, 0, 0);

	painter.drawLine(0, 0, 10, 10);
	painter.xorLine(0, 10, 10, 0);
	painter.xorRect(1, 1, 5, 5);
	UT_Rect r(2, 2, 6, 6);
	painter.xorRect(r);
	painter.invertRect(&r);
	painter.fillRect(red, 0, 0, 4, 4);
	painter.fillRect(red, r);
	painter.fillRect(GR_Graphics::CLR3D_Highlight, 1, 1, 2, 2);
	painter.fillRect(GR_Graphics::CLR3D_BevelDown, r);
	painter.clearArea(0, 0, 3, 3);
	painter.drawImage(nullptr, 0, 0);

	UT_Point pts[3] = {{0, 0}, {5, 5}, {10, 0}};
	painter.polygon(red, pts, 3);
	painter.polyLine(pts, 3);

	const UT_UCS4Char txt[] = {'x'};
	painter.drawChars(txt, 0, 1, 0, 0);
	painter.drawCharsRelativeToBaseline(txt, 0, 1, 0, 10);
	painter.drawGlyph(0, 0, 0);

	/* renderChars on an empty XP render info is a no-op draw */
	TestXPRenderInfo ri(GRScriptType_Undefined);
	ri.m_pGraphics = pG;
	painter.renderChars(ri);

	TFPASS(painter.genImageFromRectangle(r) == nullptr);

	/* image-segment fill path -- the segment is drawn into the null
	 * graphics' no-op drawImage then freed */
	TestImage img(10, 10, false);
	UT_Rect src(0, 0, 10, 10), dest(0, 0, 20, 20);
	painter.fillRect(&img, src, dest);

	GR_Painter painter2(pG, false);
	painter2.drawLine(0, 0, 1, 1);
}

/* ------------------------------------------------------------------ */
/* GR_MathTypesetter -- real parse + layout + render, headless cairo   */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("GR_MathTypesetter")
{
	GR_MathTypesetter ts;
	TFPASS(ts.empty());
	TFPASS(ts.width() == 0);

	TFPASS(ts.parseLaTeX("x^2 + \\frac{1}{y} - \\sqrt{z}"));
	TFPASS(!ts.empty());
	TFPASS(!ts.hasError());

	UT_UTF8String mml = ts.toMathML();
	TFPASS(mml.byteLength() > 0);
	TFPASS(strstr(mml.utf8_str(), "math") != nullptr);

	UT_UTF8String tex = ts.toLaTeX();
	TFPASS(tex.byteLength() > 0);

	/* MathML round trip */
	GR_MathTypesetter ts2;
	TFPASS(ts2.parseMathML(mml.utf8_str(), -1));
	TFPASS(!ts2.empty());

	/* empty/null input fails the early-out check without flagging a
	 * parse error */
	GR_MathTypesetter ts3;
	TFPASS(!ts3.parseMathML(""));
	TFPASS(!ts3.parseMathML(nullptr));
	TFPASS(!ts3.hasError());

	/* parser resource limits: pathological input must fail cleanly
	 * instead of exhausting the stack or memory */
	{
		/* brace nesting far beyond the depth cap */
		GR_MathTypesetter tsd;
		std::string deep(4096, '{');
		deep += 'x';
		deep.append(4096, '}');
		TFPASS(!tsd.parseLaTeX(deep.c_str()));
		TFPASS(tsd.hasError());
	}
	{
		/* nested \frac chains trip the same depth cap */
		GR_MathTypesetter tsf;
		std::string frac;
		for (int k = 0; k < 512; ++k) frac += "\\frac{";
		frac += 'x';
		for (int k = 0; k < 512; ++k) frac += "}{y}";
		TFPASS(!tsf.parseLaTeX(frac.c_str()));
		TFPASS(tsf.hasError());
	}
	{
		/* deep MathML nesting trips the cap too */
		GR_MathTypesetter tsm;
		std::string mmlDeep = "<math xmlns=\"http://www.w3.org/1998/Math/MathML\">";
		for (int k = 0; k < 400; ++k) mmlDeep += "<mrow>";
		mmlDeep += "<mi>x</mi>";
		for (int k = 0; k < 400; ++k) mmlDeep += "</mrow>";
		mmlDeep += "</math>";
		TFPASS(!tsm.parseMathML(mmlDeep.c_str(), -1));
		TFPASS(tsm.hasError());
	}
	{
		/* huge flat input trips the node budget */
		GR_MathTypesetter tsw;
		std::string wide;
		wide.reserve(140000);
		for (int k = 0; k < 70000; ++k) wide += "x ";
		TFPASS(!tsw.parseLaTeX(wide.c_str()));
		TFPASS(tsw.hasError());
	}
	{
		/* sane nesting stays well under the caps and still parses */
		GR_MathTypesetter tsn;
		std::string nest(64, '{');
		nest += "\\frac{a}{b}";
		nest.append(64, '}');
		TFPASS(tsn.parseLaTeX(nest.c_str()));
		TFPASS(!tsn.empty());
	}
	{
		/* a real document-size expression is unaffected */
		GR_MathTypesetter tsnorm;
		TFPASS(tsnorm.parseLaTeX(
			"\\sum_{i=1}^{n} i = \\frac{n(n+1)}{2} + "
			"\\int_0^1 x^2\\,dx - \\sqrt[3]{27}"));
		TFPASS(!tsnorm.hasError());
	}

	/* layout + render against an in-memory image surface */
	cairo_surface_t * surf =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 200, 100);
	cairo_t * cr = cairo_create(surf);
	ts.setColor(0.2, 0.4, 0.6);
	ts.layout(cr, "Serif", 12.0, true);
	TFPASS(ts.width() > 0);
	TFPASS(ts.ascent() > 0);
	TFPASS(ts.descent() >= 0);
	cairo_translate(cr, 0, ts.ascent());
	ts.render(cr);

	/* layout on an empty tree is a fast path */
	GR_MathTypesetter ts4;
	ts4.layout(cr, nullptr, 12.0, false);
	TFPASS(ts4.width() == 0);
	ts4.render(cr);

	cairo_destroy(cr);
	cairo_surface_destroy(surf);
}

