/* AbiSource Program Utilities
 * Copyright (C) 2026 Abinova developers
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

#include <string.h>
#include <math.h>
#include <string>
#include <vector>

#include "tf_test.h"
#include "ut_svg.h"
#include "ut_bytebuf.h"

#define TFSUITE "core.af.util.svg"

namespace
{

static bool near(float a, float b)
{
	return fabsf(a - b) < 0.001f;
}

static bool matNear(const UT_SVGMatrix & m,
					float a, float b, float c, float d, float e, float f)
{
	return near(m.a, a) && near(m.b, b) && near(m.c, c) &&
		   near(m.d, d) && near(m.e, e) && near(m.f, f);
}

static UT_ByteBufPtr bufOf(const char * s)
{
	UT_ByteBufPtr bb(new UT_ByteBuf);
	bb->append(reinterpret_cast<const UT_Byte *>(s),
			   static_cast<UT_uint32>(strlen(s)));
	return bb;
}

struct SvgEvents
{
	std::vector<std::string> starts;
	std::vector<std::string> ends;
	std::vector<std::string> texts;
};

static void cbStart(void * ud, const char * name, const char ** /*atts*/)
{
	static_cast<SvgEvents *>(ud)->starts.push_back(name);
}

static void cbEnd(void * ud, const char * name)
{
	static_cast<SvgEvents *>(ud)->ends.push_back(name);
}

static void cbText(void * ud, const UT_ConstByteBufPtr & text)
{
	SvgEvents * ev = static_cast<SvgEvents *>(ud);
	ev->texts.push_back(
		std::string(reinterpret_cast<const char *>(text->getPointer(0)),
					text->getLength()));
}

} // namespace

TFTEST_MAIN("UT_SVGMatrix arithmetic")
{
	// identity defaults
	UT_SVGMatrix id;
	TFPASS(matNear(id, 1, 0, 0, 1, 0, 0));

	// multiply = matrix composition
	UT_SVGMatrix m(1, 2, 3, 4, 5, 6);
	UT_SVGMatrix n(7, 8, 9, 10, 11, 12);
	UT_SVGMatrix p = m.multiply(n);
	// [a c e] [7 9 11]   [1*7+3*8 1*9+3*10 1*11+3*12+5]
	// [b d f]*[8 10 12] = [2*7+4*8 2*9+4*10 2*11+4*12+6]
	TFPASS(matNear(p, 31, 46, 39, 58, 52, 76));

	// inverse of translate+scale returns identity when multiplied
	UT_SVGMatrix t = id.translate(10, 20).scale(2);
	UT_SVGMatrix ti = t.inverse();
	UT_SVGMatrix prod = t.multiply(ti);
	TFPASS(matNear(prod, 1, 0, 0, 1, 0, 0));

	// singular matrix inverts to identity
	UT_SVGMatrix sing(0, 0, 0, 0, 5, 5);
	TFPASS(matNear(sing.inverse(), 1, 0, 0, 1, 0, 0));

	// translate/scale/scaleNonUniform are pure affine ops
	TFPASS(matNear(UT_SVGMatrix().translate(3, -4), 1, 0, 0, 1, 3, -4));
	TFPASS(matNear(UT_SVGMatrix().scale(5), 5, 0, 0, 5, 0, 0));
	TFPASS(matNear(UT_SVGMatrix().scaleNonUniform(2, 3), 2, 0, 0, 3, 0, 0));

	// scale keeps existing translation
	TFPASS(matNear(UT_SVGMatrix(1, 0, 0, 1, 7, 9).scale(2), 2, 0, 0, 2, 7, 9));

	// rotate 90 degrees
	UT_SVGMatrix r = UT_SVGMatrix().rotate(90);
	TFPASS(matNear(r, 0, 1, -1, 0, 0, 0));

	// rotateFromVector: unit x-axis -> identity rotation
	TFPASS(matNear(UT_SVGMatrix().rotateFromVector(1, 0), 1, 0, 0, 1, 0, 0));
	// zero vector returns the unchanged matrix
	UT_SVGMatrix keep(1, 0, 0, 1, 4, 4);
	TFPASS(matNear(keep.rotateFromVector(0, 0), 1, 0, 0, 1, 4, 4));

	// flips
	TFPASS(matNear(UT_SVGMatrix(1, 0, 0, 1, 3, 3).flipX(), -1, 0, 0, 1, 3, 3));
	TFPASS(matNear(UT_SVGMatrix(1, 0, 0, 1, 3, 3).flipY(), 1, 0, 0, -1, 3, 3));

	// skew 45deg gives tan(45)=1
	TFPASS(matNear(UT_SVGMatrix().skewX(45), 1, 0, 1, 1, 0, 0));
	TFPASS(matNear(UT_SVGMatrix().skewY(45), 1, 1, 0, 1, 0, 0));

	// ~90deg skews are rejected (unchanged matrix)
	TFPASS(matNear(UT_SVGMatrix(2, 0, 0, 2, 1, 1).skewX(90), 2, 0, 0, 2, 1, 1));
	TFPASS(matNear(UT_SVGMatrix(2, 0, 0, 2, 1, 1).skewY(90.05), 2, 0, 0, 2, 1, 1));

	// angles wrap into [0,180)
	TFPASS(matNear(UT_SVGMatrix().skewX(225), 1, 0, 1, 1, 0, 0));
	TFPASS(matNear(UT_SVGMatrix().skewY(-135), 1, 1, 0, 1, 0, 0));

	// UT_SVGPoint is a plain pair
	UT_SVGPoint pt(1.5f, -2.5f);
	TFPASS(near(pt.x, 1.5f) && near(pt.y, -2.5f));
	UT_SVGPoint zero;
	TFPASS(near(zero.x, 0) && near(zero.y, 0));
}

TFTEST_MAIN("UT_SVGMatrix::applyTransform")
{
	UT_SVGMatrix m;

	// nullptr transform is a no-op success
	TFPASS(UT_SVGMatrix::applyTransform(&m, nullptr));
	TFPASS(matNear(m, 1, 0, 0, 1, 0, 0));

	// each transform keyword
	TFPASS(UT_SVGMatrix::applyTransform(&m, "matrix(1 2 3 4 5 6)"));
	TFPASS(matNear(m, 1, 2, 3, 4, 5, 6));

	m = UT_SVGMatrix();
	TFPASS(UT_SVGMatrix::applyTransform(&m, "translate(10 20)"));
	TFPASS(matNear(m, 1, 0, 0, 1, 10, 20));

	m = UT_SVGMatrix();
	TFPASS(UT_SVGMatrix::applyTransform(&m, "translate(5)")); // single-arg
	TFPASS(matNear(m, 1, 0, 0, 1, 5, 0));

	m = UT_SVGMatrix();
	TFPASS(UT_SVGMatrix::applyTransform(&m, "scale(2)"));
	TFPASS(matNear(m, 2, 0, 0, 2, 0, 0));

	m = UT_SVGMatrix();
	TFPASS(UT_SVGMatrix::applyTransform(&m, "scale(2,3)"));
	TFPASS(matNear(m, 2, 0, 0, 3, 0, 0));

	m = UT_SVGMatrix();
	TFPASS(UT_SVGMatrix::applyTransform(&m, "rotate(90)"));
	TFPASS(matNear(m, 0, 1, -1, 0, 0, 0));

	// rotate about a point = T(ox,oy) * R * T(-ox,-oy)
	m = UT_SVGMatrix();
	TFPASS(UT_SVGMatrix::applyTransform(&m, "rotate(90 10 10)"));
	{
		UT_SVGMatrix expect = UT_SVGMatrix().translate(10, 10)
											.rotate(90)
											.translate(-10, -10);
		TFPASS(matNear(m, expect.a, expect.b, expect.c,
					  expect.d, expect.e, expect.f));
	}

	m = UT_SVGMatrix();
	TFPASS(UT_SVGMatrix::applyTransform(&m, "skewX(45)"));
	TFPASS(matNear(m, 1, 0, 1, 1, 0, 0));

	m = UT_SVGMatrix();
	TFPASS(UT_SVGMatrix::applyTransform(&m, "skewY(45)"));
	TFPASS(matNear(m, 1, 1, 0, 1, 0, 0));

	// chained transforms compose in order
	m = UT_SVGMatrix();
	TFPASS(UT_SVGMatrix::applyTransform(&m, "translate(10 0) scale(2)"));
	TFPASS(matNear(m, 2, 0, 0, 2, 10, 0));

	// comma separators and scientific notation numbers
	m = UT_SVGMatrix();
	TFPASS(UT_SVGMatrix::applyTransform(&m, "translate(1e1,-2.5)"));
	TFPASS(matNear(m, 1, 0, 0, 1, 10, -2.5));

	// malformed inputs fail cleanly
	m = UT_SVGMatrix();
	TFPASS(!UT_SVGMatrix::applyTransform(&m, "translate(10"));       // unclosed
	TFPASS(!UT_SVGMatrix::applyTransform(&m, "matrix(1 2 3)"));      // arity
	TFPASS(!UT_SVGMatrix::applyTransform(&m, "scale(x)"));           // bad number
	TFPASS(!UT_SVGMatrix::applyTransform(&m, "rotate(10,)"));        // missing arg
	TFPASS(!UT_SVGMatrix::applyTransform(&m, "skewX()"));            // no arg
	TFPASS(!UT_SVGMatrix::applyTransform(&m, "matrix("));            // empty
	// unknown tokens stop the loop but don't flag a parse error
	TFPASS(UT_SVGMatrix::applyTransform(&m, "bogus(1 2)"));
}

TFTEST_MAIN("UT_SVG_recognizeContent and getDimensions")
{
	// a genuine minimal svg root is recognized
	static const char svg[] =
		"<svg width=\"100\" height=\"2in\"><rect/></svg>";
	TFPASS(UT_SVG_recognizeContent(svg, static_cast<UT_uint32>(sizeof(svg) - 1)));

	// namespaced root counts too
	static const char nssvg[] = "<svg:svg xmlns:svg=\"x\"></svg:svg>";
	TFPASS(UT_SVG_recognizeContent(nssvg, static_cast<UT_uint32>(sizeof(nssvg) - 1)));

	// binary junk containing "<svg" bytes is NOT svg (root never arrives)
	static const char bin[] = "\x89PNG<svg xmlns=\"x\"></svg>";
	TFPASS(!UT_SVG_recognizeContent(bin, static_cast<UT_uint32>(sizeof(bin) - 1)));

	static const char html[] = "<html><body>svg</body></html>";
	TFPASS(!UT_SVG_recognizeContent(html, static_cast<UT_uint32>(sizeof(html) - 1)));

	TFPASS(!UT_SVG_recognizeContent(nullptr, 10));
	TFPASS(!UT_SVG_recognizeContent(svg, 0));

	// dimensions: px values scale to layout units (twips) at 96dpi base
	UT_sint32 dw = 0, dh = 0, lw = 0, lh = 0;
	TFPASS(UT_SVG_getDimensions(bufOf(svg), nullptr, dw, dh, lw, lh));
	// width="100" -> 100 px display, 100 * 1440/72 = 2000 twips
	TFPASS(dw == 100);
	TFPASS(lw == 2000);
	// height="2in" -> display 2*72=144 px (no graphics -> 72dpi), layout 2880
	TFPASS(dh == 144);
	TFPASS(lh == 2880);

	// non-svg buffer fails
	UT_sint32 z = 0;
	TFPASS(!UT_SVG_getDimensions(bufOf("not xml at all"), nullptr, z, z, z, z));
}

TFTEST_MAIN("UT_svg parse mode fires element and text callbacks")
{
	static const char svg[] =
		"<svg width=\"10\"><g><rect/></g>"
		"<text>hello</text>"
		"<text><tspan>one</tspan><tspan>two</tspan></text></svg>";

	UT_svg parser(nullptr, UT_svg::pm_parse);
	SvgEvents ev;
	parser.cb_userdata = &ev;
	parser.cb_start = cbStart;
	parser.cb_end = cbEnd;
	parser.cb_text = cbText;

	TFPASS(parser.parse(bufOf(svg)));
	TFPASS(parser.m_bSVG);

	// start/end callbacks see every element:
	// svg, g, rect, text, text, tspan, tspan
	TFPASS(ev.starts.size() == 7);
	TFPASS(ev.ends.size() == 7);

	// plain text fires one cb_text; tspan text fires per-tspan,
	// and the mixed text element discards its non-tspan buffer
	TFPASS(ev.texts.size() == 3);
	if (ev.texts.size() == 3)
	{
		TFPASS(ev.texts[0] == "hello");
		TFPASS(ev.texts[1] == "one");
		TFPASS(ev.texts[2] == "two");
	}
}

TFTEST_MAIN("UT_svg getAttribute")
{
	UT_svg parser(nullptr, UT_svg::pm_parse);
	const char * atts[] = {"width", "10", "fill", "red", nullptr};
	TFPASS(parser.getAttribute("width", atts) != nullptr);
	TFPASS(strcmp(parser.getAttribute("fill", atts), "red") == 0);
	TFPASS(parser.getAttribute("stroke", atts) == nullptr);
	const char * none[] = {nullptr};
	TFPASS(parser.getAttribute("width", none) == nullptr);
}
