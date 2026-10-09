/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */

/* Abinova
 * Copyright (C) 2001 AbiSource, Inc.
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

#include "ut_types.h"
#include "ut_bytebuf.h"
#include "ut_string.h"
#include "ut_debugmsg.h"
#include "ut_assert.h"

#include "fg_GraphicRaster.h"
#include "ie_impGraphic_WMF.h"

#include <stdio.h>

#include <libwmf/api.h>
#include <libwmf/gd.h>

static int  AbiWord_WMF_read (void * context);
static int  AbiWord_WMF_seek (void * context,long pos);
static long AbiWord_WMF_tell (void * context);
static int  AbiWord_WMF_function (void * context,char * buffer,int length);

struct bbuf_read_info
{
	UT_ConstByteBufPtr pByteBuf;

	UT_uint32 len;
	UT_uint32 pos;
};

struct bbuf_write_info
{
	UT_ByteBufPtr pByteBuf;
};

// supported suffixes
static IE_SuffixConfidence IE_ImpGraphicWMF_Sniffer__SuffixConfidence[] = {
	{ "wmf", 	UT_CONFIDENCE_PERFECT 	},
	{ "", 	UT_CONFIDENCE_ZILCH 	}
};

const IE_SuffixConfidence * IE_ImpGraphicWMF_Sniffer::getSuffixConfidence ()
{
	return IE_ImpGraphicWMF_Sniffer__SuffixConfidence;
}

UT_Confidence_t IE_ImpGraphicWMF_Sniffer::recognizeContents(const char * /*szBuf*/, UT_uint32 /*iNumbytes*/)
{
	return ( UT_CONFIDENCE_POOR ); // Don't know how to recognize metafiles, so say yes
}

bool IE_ImpGraphicWMF_Sniffer::getDlgLabels(const char ** pszDesc,
					const char ** pszSuffixList,
					IEGraphicFileType * ft)
{
	*pszDesc = "Windows Metafile (.wmf)";
	*pszSuffixList = "*.wmf";
	*ft = getType ();
	return true;
}

UT_Error IE_ImpGraphicWMF_Sniffer::constructImporter(IE_ImpGraphic **ppieg)
{
	*ppieg = new IE_ImpGraphic_WMF();
	if (*ppieg == nullptr)
		return UT_IE_NOMEMORY;

	return UT_OK;
}

// This creates our FG_Graphic object for a PNG
UT_Error IE_ImpGraphic_WMF::importGraphic(const UT_ConstByteBufPtr & pBBwmf,
                                          FG_ConstGraphicPtr &pfg)
{
	UT_Error err = UT_OK;

	pfg.reset();
	UT_DEBUGMSG(("IE_ImpGraphic_WMF::importGraphic Begin -\n"));

	/* the libwmf SVG path produces FG_GraphicVector images our cairo
	 * renderers cannot yet paint (blank output); rasterize through
	 * libwmf's GD backend instead so embedded metafiles are visible */
	UT_ConstByteBufPtr pBBpng;

	err = convertGraphic(pBBwmf, pBBpng);
	if (err != UT_OK) {
		UT_DEBUGMSG(("IE_ImpGraphic_WMF::importGraphic Conversion failed...\n"));
		return err;
	}

	FG_GraphicRasterPtr pFGR(new FG_GraphicRaster);
	if(!pFGR) {
		UT_DEBUGMSG(("IE_ImpGraphic_WMF::importGraphic Ins. Mem.\n"));
		err = UT_IE_NOMEMORY;
	}
	else if(!pFGR->setRaster_PNG(pBBpng)) {
		UT_DEBUGMSG(("IE_ImpGraphic_WMF::importGraphic Fake type?\n"));
		err = UT_IE_FAKETYPE;
	}
	else {
		pfg = std::move(pFGR);
	}

	UT_DEBUGMSG(("IE_ImpGraphic_WMF::importGraphic - End\n"));

	return err;
}

UT_Error IE_ImpGraphic_WMF::convertGraphic(const UT_ConstByteBufPtr & pBBwmf,
					   UT_ConstByteBufPtr & pBBpng)
{
	wmf_error_t err;

	wmf_gd_t * ddata = nullptr;

	wmfAPI * API = nullptr;
	wmfAPI_Options api_options;

	wmfD_Rect bbox;

	unsigned long flags;


	unsigned int width, height;

	bbuf_read_info  read_info;
	bbuf_write_info write_info;

   	if (!pBBwmf) {
		UT_DEBUGMSG(("IE_ImpGraphic_WMF::convertGraphic Bad Arg (1)\n"));
		return UT_ERROR;
	}

	pBBpng.reset();

	flags = WMF_OPT_IGNORE_NONFATAL | WMF_OPT_FUNCTION;

	api_options.function = wmf_gd_function;

	err = wmf_api_create(&API,flags,&api_options);
	if (err != wmf_E_None) {
		UT_DEBUGMSG(("IE_ImpGraphic_WMF::convertGraphic No API\n"));
		return UT_ERROR;
	}

	ddata = WMF_GD_GetData(API);
	if ((ddata->flags & WMF_GD_SUPPORTS_PNG) == 0) { // Impossible, but...
		UT_DEBUGMSG(("IE_ImpGraphic_WMF::convertGraphic No PNG\n"));
		wmf_api_destroy(API);
		return UT_ERROR;
	}

	read_info.pByteBuf = pBBwmf;

	read_info.len = pBBwmf->getLength();
	read_info.pos = 0;

	err = wmf_bbuf_input (API,AbiWord_WMF_read,AbiWord_WMF_seek,AbiWord_WMF_tell,static_cast<void *>( &read_info));
	if (err != wmf_E_None) {
		UT_DEBUGMSG(("IE_ImpGraphic_WMF::convertGraphic Bad input set\n"));
		wmf_api_destroy(API);
		return UT_ERROR;
	}

	err = wmf_scan (API,0,&bbox);
	if (err != wmf_E_None) {
		UT_DEBUGMSG(("IE_ImpGraphic_WMF::convertGraphic Scan failed\n"));
		wmf_api_destroy(API);
		return UT_ERROR;
	}

	/* TODO: be smarter about getting the resolution from screen 
	 */
	double resolution_x, resolution_y;
	resolution_x = resolution_y = 72.0;

	err = wmf_display_size (API, &width, &height, resolution_x, resolution_y);
	if (err != wmf_E_None) {
		UT_DEBUGMSG(("IE_ImpGraphic_WMF::convertGraphic Get size failed\n"));
		wmf_api_destroy(API);
		return UT_ERROR;
	}

	ddata->width  = static_cast<unsigned int>( width);
	ddata->height = static_cast<unsigned int>( height);

	if ((ddata->width == 0) || (ddata->height == 0)) {
		UT_DEBUGMSG(("IE_ImpGraphic_WMF::convertGraphic Size error (1)\n"));
		wmf_api_destroy(API);
		return UT_ERROR;
	}


	if ((ddata->width == 0) || (ddata->height == 0)) {
		UT_DEBUGMSG(("IE_ImpGraphic_WMF::convertGraphic Size error (1)\n"));
		wmf_api_destroy(API);
		return UT_ERROR;
	}

	ddata->bbox = bbox;

	ddata->type = wmf_gd_png;

	UT_ByteBufPtr bb(new UT_ByteBuf);
	if (!bb) {
		UT_DEBUGMSG(("IE_ImpGraphic_WMF::convertGraphic Ins. Mem.\n"));
		wmf_api_destroy(API);
		return UT_IE_NOMEMORY;
	}

	write_info.pByteBuf = bb;

	ddata->flags |= WMF_GD_OUTPUT_MEMORY | WMF_GD_OWN_BUFFER;

	ddata->sink.context = static_cast<void *>( &write_info);
	ddata->sink.function = AbiWord_WMF_function;

	err = wmf_play(API,0,&bbox);
	if (err != wmf_E_None) {
		UT_DEBUGMSG(("IE_ImpGraphic_WMF::convertGraphic Play failed\n"));
	}

	err = wmf_api_destroy(API);

	if (err == wmf_E_None) {
		pBBpng = std::move(bb);
		return UT_OK;
	}

	UT_DEBUGMSG(("IE_ImpGraphic_WMF::convertGraphic Err. on destroy\n"));

	return UT_ERROR;
}

// returns unsigned char cast to int, or EOF
static int AbiWord_WMF_read (void * context)
{
	bbuf_read_info * info = static_cast<bbuf_read_info *>( context);

	const UT_Byte* pByte = nullptr;

	if (info->pos == info->len)
		return EOF;

	pByte = info->pByteBuf->getPointer(info->pos);

	info->pos++;

	return static_cast<int>( (static_cast<unsigned char>( *pByte)));
}

// returns (-1) on error, else 0
static int AbiWord_WMF_seek (void * context,long pos)
{
	bbuf_read_info * info = static_cast<bbuf_read_info *>( context);

	info->pos = static_cast<UT_uint32>( pos);

	return 0;
}

// returns (-1) on error, else pos
static long AbiWord_WMF_tell (void * context)
{
	bbuf_read_info * info = static_cast<bbuf_read_info *>( context);

	return static_cast<long>( info->pos);
}

static int AbiWord_WMF_function (void * context,char * buffer,int length)
{
	bbuf_write_info * info = static_cast<bbuf_write_info *>( context);

	UT_Byte a_byte;

	int i = 0;

	while (i < length) {
		a_byte = static_cast<UT_Byte>( (static_cast<unsigned char>( buffer[i]))); // why char I know not...
		if (!info->pByteBuf->append(&a_byte,1))
			break;
		i++;
	}

	return i;
}
