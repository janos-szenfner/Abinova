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
#include <string>

#include "tf_test.h"
#include "ut_color.h"

#define TFSUITE "core.af.util.color"

TFTEST_MAIN("UT_RGBColor basics")
{
	UT_RGBColor black;
	TFPASS(black.m_red == 0 && black.m_grn == 0 && black.m_blu == 0);
	TFPASS(!black.isTransparent());

	UT_RGBColor c(10, 20, 30);
	TFPASS(c.m_red == 10 && c.m_grn == 20 && c.m_blu == 30);
	TFPASS(c == UT_RGBColor(10, 20, 30));
	TFPASS(c != black);

	UT_RGBColor t(0, 0, 0, true);
	TFPASS(t.isTransparent());

	UT_RGBColor copy(c);
	TFPASS(copy == c);
	UT_RGBColor asg;
	asg = c;
	TFPASS(asg == c);

	// fieldwise ops
	UT_RGBColor m(100, 100, 100);
	m += static_cast<unsigned char>(50);
	TFPASS(m == UT_RGBColor(150, 150, 150));
	m -= UT_RGBColor(50, 40, 30);
	TFPASS(m == UT_RGBColor(100, 110, 120));
	UT_RGBColor x(0xF0, 0x0F, 0xFF);
	x ^= UT_RGBColor(0xFF, 0xFF, 0x0F);
	TFPASS(x == UT_RGBColor(0x0F, 0xF0, 0xF0));
	UT_RGBColor p(0xF0, 0x0F, 0x33);
	p %= UT_RGBColor(0xFF, 0xFF, 0xFF);
	TFPASS(p == UT_RGBColor(0xF0, 0x0F, 0x33));
}

TFTEST_MAIN("UT_parseColor / UT_RGBColor::setColor")
{
	UT_RGBColor c;

	UT_parseColor("#ff8000", c);
	TFPASS(c == UT_RGBColor(0xff, 0x80, 0x00));

	UT_parseColor("ff8000", c);
	TFPASS(c == UT_RGBColor(0xff, 0x80, 0x00));

	// 3-digit hex is not a valid 6-digit hash: parse fails and the
	// color is left unchanged
	c = UT_RGBColor(1, 2, 3);
	UT_parseColor("#f80", c);
	TFPASS(c == UT_RGBColor(1, 2, 3));

	UT_parseColor("red", c);
	TFPASS(c == UT_RGBColor(0xff, 0x00, 0x00));

	UT_parseColor("gray(128)", c);
	TFPASS(c == UT_RGBColor(128, 128, 128));

	// channels whose ink+black reach 255 keep the previous channel
	// value, so the base color matters: over black, c+k=0 -> red=255
	// and m+k=y+k=255 leave green/blue at 0
	c = UT_RGBColor(0, 0, 0);
	UT_parseColor("cmyk(0,255,255,0)", c);
	TFPASS(c == UT_RGBColor(0xff, 0, 0));

	UT_parseColor("transparent", c);
	TFPASS(c.isTransparent());

	// setColor routes through UT_parseColor and reports change
	c = UT_RGBColor(1, 2, 3);
	TFPASS(c.setColor("#010203") == false); // same value -> no change
	TFPASS(c.setColor("#040506") == true);
	TFPASS(c == UT_RGBColor(4, 5, 6));

	// "transparent" marks transparent and whites-out the color
	c = UT_RGBColor(1, 2, 3);
	TFPASS(c.setColor("transparent"));
	TFPASS(c.isTransparent());

	// nullptr also marks transparent
	c.setColor(nullptr);
	TFPASS(c.isTransparent());
}

TFTEST_MAIN("UT_colorToHex / UT_setColor / UT_HashColor")
{
	TFPASS(UT_colorToHex("red") == "ff0000");
	TFPASS(UT_colorToHex("red", true) == "#ff0000");
	TFPASS(UT_colorToHex("#010203") == "010203");
	TFPASS(UT_colorToHex("") == "");
	TFPASS(UT_colorToHex(nullptr) == "");

	UT_RGBColor c;
	UT_setColor(c, 9, 8, 7);
	TFPASS(c == UT_RGBColor(9, 8, 7));
	TFPASS(!c.isTransparent());
	UT_setColor(c, 0, 0, 0, true);
	TFPASS(c.isTransparent());

	UT_HashColor h;
	TFPASS(h.setColor(static_cast<unsigned char>(0x12),
					  static_cast<unsigned char>(0x34),
					  static_cast<unsigned char>(0x56)) != nullptr);
	TFPASS(h.rgb() == UT_RGBColor(0x12, 0x34, 0x56));
	TFPASS(h.setColor("#abcdef") != nullptr);
	TFPASS(h.rgb() == UT_RGBColor(0xab, 0xcd, 0xef));
	TFPASS(h.setColor("red") != nullptr);
	TFPASS(h.rgb() == UT_RGBColor(0xff, 0, 0));
	TFPASS(h.setColor(nullptr) == nullptr);
	TFPASS(h.setColor("no-such-color") == nullptr);
	TFPASS(h.setHashIfValid("#zzzzzz") == nullptr);
	TFPASS(h.setHashIfValid("123456") != nullptr);
	TFPASS(h.rgb() == UT_RGBColor(0x12, 0x34, 0x56));
}
