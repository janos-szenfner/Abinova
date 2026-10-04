/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
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

/* Required to get proper namespace inclusion from PNG code */
#include <string.h>

// AIX does this inside <sys/context.h> but we can't force png to know.
#ifdef _AIX
#define jmpbuf __jmpbuf
#endif
#include <png.h>

#include "ut_assert.h"
#include "ut_bytebuf.h"
#include "ut_debugmsg.h"
#include "ut_image.h"

struct _bb
{
	UT_ConstByteBufPtr pBB;
	UT_uint32 iCurPos;
};

static void _png_read(png_structp png_ptr, png_bytep data, png_size_t length)
{
	struct _bb* p = static_cast<struct _bb*>(png_get_io_ptr(png_ptr));
	const UT_Byte* pBytes = p->pBB->getPointer(0);

	// make sure that we don't read outside of pBytes — compare the
	// request against the remaining bytes, not buflen - length (the
	// unsigned subtraction wraps when length > buflen)
	const png_size_t remaining =
		p->iCurPos < p->pBB->getLength()
			? static_cast<png_size_t>(p->pBB->getLength() - p->iCurPos)
			: 0;
	if (length > remaining) {
		UT_WARNINGMSG(("PNG: Reading past buffer bounds. cur = %u, buflen = %u, length = %lu\n",
					   p->iCurPos, p->pBB->getLength(), static_cast<unsigned long>(length)));
		if (remaining == 0) {
			UT_WARNINGMSG(("PNG: Truncating to ZERO length.\n"));
			png_error(png_ptr, "Premature end of buffer");
			return;
		}
		UT_WARNINGMSG(("PNG: Truncating to %lu.\n", static_cast<unsigned long>(remaining)));
		length = remaining;
	}
	memcpy(data, pBytes + p->iCurPos, length);
	p->iCurPos += length;
}

bool UT_PNG_getDimensions(const UT_ConstByteBufPtr & pBB, UT_sint32& iImageWidth, UT_sint32& iImageHeight)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height;
	int bit_depth, color_type, interlace_type;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, static_cast<void*>(nullptr),
									 nullptr, nullptr);

	if (png_ptr == nullptr)
	{
		return false;
	}

	/* Allocate/initialize the memory for image information.  REQUIRED. */
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == nullptr)
	{
		png_destroy_read_struct(&png_ptr, static_cast<png_infopp>(nullptr), static_cast<png_infopp>(nullptr));
		return false;
	}

	/* Set error handling if you are using the setjmp/longjmp method (this is
	 * the normal method of doing things with libpng).  REQUIRED unless you
	 * set up your own error handlers in the png_create_read_struct() earlier.
	 */
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		/* Free all of the memory associated with the png_ptr and info_ptr */
		png_destroy_read_struct(&png_ptr, &info_ptr, static_cast<png_infopp>(nullptr));
	  
		/* If we get here, we had a problem reading the file */
		return false;
	}

	struct _bb myBB;
	myBB.pBB = pBB;
	myBB.iCurPos = 0;
	
	png_set_read_fn(png_ptr, static_cast<void *>(&myBB), _png_read);

	/* The call to png_read_info() gives us all of the information from the
	 * PNG file before the first IDAT (image data chunk).  REQUIRED
	 */
	png_read_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
				 &interlace_type, nullptr, nullptr);

	/* clean up after the read, and g_free any memory allocated - REQUIRED */
	png_destroy_read_struct(&png_ptr, &info_ptr, static_cast<png_infopp>(nullptr));

	iImageWidth = width;
	iImageHeight = height;

	return true;
}

/* Validate a PNG buffer end to end: libpng checks every chunk CRC and
 * inflates the complete zlib stream, and png_read_end requires a
 * proper IEND.  Decoding is row-at-a-time into a single reusable row,
 * so validation uses O(rowbytes) memory regardless of image size.
 * Buffers whose declared dimensions exceed the sane image limits are
 * rejected up front.  Import paths must use this — UT_PNG_getDimensions
 * only reads the header, so truncated or corrupt bodies used to be
 * stored and only failed at render time. */
bool UT_PNG_validate(const UT_ConstByteBufPtr & pBB)
{
	png_structp png_ptr;
	png_infop info_ptr;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, static_cast<void*>(nullptr),
									 nullptr, nullptr);
	if (png_ptr == nullptr)
	{
		return false;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == nullptr)
	{
		png_destroy_read_struct(&png_ptr, static_cast<png_infopp>(nullptr), static_cast<png_infopp>(nullptr));
		return false;
	}

	png_bytep row = nullptr;

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		delete[] row;
		png_destroy_read_struct(&png_ptr, &info_ptr, static_cast<png_infopp>(nullptr));
		return false;
	}

	struct _bb myBB;
	myBB.pBB = pBB;
	myBB.iCurPos = 0;

	png_set_read_fn(png_ptr, static_cast<void *>(&myBB), _png_read);

	png_read_info(png_ptr, info_ptr);

	png_uint_32 width = 0, height = 0;
	int bit_depth = 0, color_type = 0, interlace_type = 0;
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
				 &interlace_type, nullptr, nullptr);

	if (UT_image_size_exceeds_limits(width, height))
		png_error(png_ptr, "PNG dimensions exceed limits");

	const int passes = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	const png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
	if (rowbytes == 0)
		png_error(png_ptr, "empty PNG row");

	row = new png_byte[rowbytes];

	for (int pass = 0; pass < passes; ++pass)
	{
		for (png_uint_32 y = 0; y < height; ++y)
			png_read_row(png_ptr, row, nullptr);
	}

	png_read_end(png_ptr, info_ptr);

	delete[] row;
	png_destroy_read_struct(&png_ptr, &info_ptr, static_cast<png_infopp>(nullptr));

	return true;
}

