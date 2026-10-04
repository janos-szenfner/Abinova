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
#include <vector>

#include <jpeglib.h>

#include "tf_test.h"
#include "ut_jpeg.h"
#include "ut_bytebuf.h"

#define TFSUITE "core.af.util.jpeg"

namespace
{

// encode `pixels` (w*h*comps) into an in-memory JPEG; guarantees the test
// data is well-formed and has known content
static std::string encodeJPEG(int w, int h, int comps, J_COLOR_SPACE cs,
							  const unsigned char * pixels, int quality)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	unsigned char * mem = nullptr;
	unsigned long memSize = 0;
	jpeg_mem_dest(&cinfo, &mem, &memSize);

	cinfo.image_width = w;
	cinfo.image_height = h;
	cinfo.input_components = comps;
	cinfo.in_color_space = cs;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	jpeg_start_compress(&cinfo, TRUE);

	while (cinfo.next_scanline < cinfo.image_height)
	{
		JSAMPROW row = const_cast<JSAMPROW>(
			reinterpret_cast<const JSAMPLE *>(pixels) +
			cinfo.next_scanline * w * comps);
		jpeg_write_scanlines(&cinfo, &row, 1);
	}
	jpeg_finish_compress(&cinfo);

	std::string out(reinterpret_cast<const char *>(mem), memSize);
	free(mem);
	jpeg_destroy_compress(&cinfo);
	return out;
}

static UT_ByteBufPtr bbOf(const std::string & s)
{
	UT_ByteBufPtr bb(new UT_ByteBuf);
	bb->append(reinterpret_cast<const UT_Byte *>(s.data()), s.size());
	return bb;
}

// 4x3 RGB: top-left 2x2 block green (aligned to 2x2 chroma subsampling
// blocks so no colour bleed), everything else red
static std::string makeRGB()
{
	unsigned char px[4 * 3 * 3];
	for (int y = 0; y < 3; y++)
		for (int x = 0; x < 4; x++)
		{
			int i = (y * 4 + x) * 3;
			if (x < 2 && y < 2)
			{
				px[i] = 0; px[i + 1] = 255; px[i + 2] = 0; // green
			}
			else
			{
				px[i] = 255; px[i + 1] = 0; px[i + 2] = 0; // red
			}
		}
	return encodeJPEG(4, 3, 3, JCS_RGB, px, 95);
}

// 4x3 grayscale, all pixels mid-gray 128
static std::string makeGray()
{
	unsigned char px[4 * 3];
	memset(px, 128, sizeof(px));
	return encodeJPEG(4, 3, 1, JCS_GRAYSCALE, px, 95);
}

// 4x3 CMYK: Adobe-inverted samples for yellow (C=0,M=0,Y=255,K=0 ->
// stored inverted as 255,255,0,255) everywhere
static std::string makeCmyk()
{
	unsigned char px[4 * 3 * 4];
	for (int i = 0; i < 4 * 3; i++)
	{
		px[i * 4]     = 255;
		px[i * 4 + 1] = 255;
		px[i * 4 + 2] = 0;
		px[i * 4 + 3] = 255;
	}
	return encodeJPEG(4, 3, 4, JCS_CMYK, px, 95);
}

} // namespace

TFTEST_MAIN("UT_JPEG_getDimensions")
{
	UT_sint32 w = 0, h = 0;

	TFPASS(UT_JPEG_getDimensions(bbOf(makeRGB()), w, h));
	TFPASS(w == 4 && h == 3);

	TFPASS(UT_JPEG_getDimensions(bbOf(makeGray()), w, h));
	TFPASS(w == 4 && h == 3);

	TFPASS(UT_JPEG_getDimensions(bbOf(makeCmyk()), w, h));
	TFPASS(w == 4 && h == 3);

	// NB: malformed JPEG input is not testable: libjpeg's default
	// error_exit() terminates the process (ut_jpeg installs no
	// error manager).
}

TFTEST_MAIN("UT_JPEG_getRGBData color conversion")
{
	const std::string rgbJpeg = makeRGB();
	UT_Byte rgb[4 * 3 * 3];
	memset(rgb, 0, sizeof(rgb));

	TFPASS(UT_JPEG_getRGBData(bbOf(rgbJpeg), rgb, 4 * 3, false, false));

	// top-left pixel is green; allow JPEG tolerance
	TFPASS(rgb[0] < 80 && rgb[1] > 175 && rgb[2] < 80);
	// the top-right pixel is red
	TFPASS(rgb[9] > 175 && rgb[10] < 90 && rgb[11] < 90);

	// BGR order: green pixel -> (b,g,r) = (0,255,0)
	UT_Byte bgr[4 * 3 * 3];
	TFPASS(UT_JPEG_getRGBData(bbOf(rgbJpeg), bgr, 4 * 3, true, false));
	TFPASS(bgr[0] < 80 && bgr[1] > 175 && bgr[2] < 80);
	// red pixel in BGR: blue byte first, red last
	TFPASS(bgr[9] < 90 && bgr[10] < 90 && bgr[11] > 175);

	// "bFlipHoriz" actually writes rows bottom-up (vertical flip):
	// the green 2x2 block moves to the bottom-left
	UT_Byte flip[4 * 3 * 3];
	TFPASS(UT_JPEG_getRGBData(bbOf(rgbJpeg), flip, 4 * 3, false, true));
	TFPASS(flip[0] > 175 && flip[1] < 90);                      // top-left now red
	TFPASS(flip[24] < 80 && flip[25] > 175 && flip[26] < 80);  // bottom-left green

	// grayscale expands to grey RGB
	UT_Byte gray[4 * 3 * 3];
	TFPASS(UT_JPEG_getRGBData(bbOf(makeGray()), gray, 4 * 3, false, false));
	TFPASS(gray[0] > 110 && gray[0] < 150);
	TFPASS(gray[0] == gray[1] && gray[1] == gray[2]);

	// CMYK (0,255,255,0) -> yellow: r&g high, b low
	UT_Byte cmyk[4 * 3 * 3];
	TFPASS(UT_JPEG_getRGBData(bbOf(makeCmyk()), cmyk, 4 * 3, false, false));
	TFPASS(cmyk[0] > 175 && cmyk[1] > 175 && cmyk[2] < 80);

	// null checks
	TFPASS(!UT_JPEG_getRGBData(bbOf(rgbJpeg), nullptr, 4 * 3, false, false));
	TFPASS(!UT_JPEG_getRGBData(UT_ByteBufPtr(), rgb, 4 * 3, false, false));
}
