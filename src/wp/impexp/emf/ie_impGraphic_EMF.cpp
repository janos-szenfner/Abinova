/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */

/* Abinova
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
#include "ut_debugmsg.h"
#include "ut_assert.h"

#include "fg_GraphicRaster.h"
#include "ie_impGraphic_EMF.h"
#include "emf_render.h"

// supported suffixes
static IE_SuffixConfidence IE_ImpGraphicEMF_Sniffer__SuffixConfidence[] = {
	{ "emf", 	UT_CONFIDENCE_PERFECT 	},
	{ "", 	UT_CONFIDENCE_ZILCH 	}
};

static IE_MimeConfidence IE_ImpGraphicEMF_Sniffer__MimeConfidence[] = {
	{ IE_MIME_MATCH_FULL, 	"image/emf", 		UT_CONFIDENCE_PERFECT 	},
	{ IE_MIME_MATCH_FULL, 	"image/x-emf", 		UT_CONFIDENCE_PERFECT 	},
	{ IE_MIME_MATCH_FULL, 	"image/x-win-metafile", UT_CONFIDENCE_GOOD 	},
	{ IE_MIME_MATCH_BOGUS, 	"", 			UT_CONFIDENCE_ZILCH 	}
};

const IE_SuffixConfidence * IE_ImpGraphicEMF_Sniffer::getSuffixConfidence ()
{
	return IE_ImpGraphicEMF_Sniffer__SuffixConfidence;
}

const IE_MimeConfidence * IE_ImpGraphicEMF_Sniffer::getMimeConfidence ()
{
	return IE_ImpGraphicEMF_Sniffer__MimeConfidence;
}

UT_Confidence_t IE_ImpGraphicEMF_Sniffer::recognizeContents(const char * szBuf,
													   UT_uint32 iNumbytes)
{
	/* EMR_HEADER: iType=1 at 0, nSize>=88 at 4, iSignature ' EMF'
	 * (0x464D4520) at offset 40 ([MS-EMF] 2.3.4.1) */
	if (iNumbytes >= 44 &&
		static_cast<unsigned char>(szBuf[0]) == 0x01 &&
		szBuf[1] == 0 && szBuf[2] == 0 && szBuf[3] == 0 &&
		static_cast<unsigned char>(szBuf[40]) == 0x20 &&
		static_cast<unsigned char>(szBuf[41]) == 0x45 &&
		static_cast<unsigned char>(szBuf[42]) == 0x4D &&
		static_cast<unsigned char>(szBuf[43]) == 0x46)
	{
		return UT_CONFIDENCE_PERFECT;
	}
	return UT_CONFIDENCE_ZILCH;
}

bool IE_ImpGraphicEMF_Sniffer::getDlgLabels(const char ** pszDesc,
											const char ** pszSuffixList,
											IEGraphicFileType * ft)
{
	*pszDesc = "Enhanced Metafile (.emf)";
	*pszSuffixList = "*.emf";
	*ft = getType ();
	return true;
}

UT_Error IE_ImpGraphicEMF_Sniffer::constructImporter(IE_ImpGraphic **ppieg)
{
	*ppieg = new IE_ImpGraphic_EMF();
	if (*ppieg == nullptr)
		return UT_IE_NOMEMORY;

	return UT_OK;
}

UT_Error IE_ImpGraphic_EMF::importGraphic(const UT_ConstByteBufPtr & pBBemf,
										FG_ConstGraphicPtr &pfg)
{
	pfg.reset();

	UT_ConstByteBufPtr pBBpng;
	UT_Error err = convertGraphic(pBBemf, pBBpng);
	if (err != UT_OK)
		return err;

	FG_GraphicRasterPtr pFGR(new FG_GraphicRaster);
	if (!pFGR)
	{
		return UT_IE_NOMEMORY;
	}
	if (!pFGR->setRaster_PNG(pBBpng))
	{
		return UT_IE_FAKETYPE;
	}
	pfg = std::move(pFGR);
	return UT_OK;
}

UT_Error IE_ImpGraphic_EMF::convertGraphic(const UT_ConstByteBufPtr & pBBemf,
										 UT_ConstByteBufPtr & pBBpng)
{
	pBBpng.reset();
	if (!pBBemf)
		return UT_ERROR;

	UT_ByteBufPtr bb;
	if (!UT_renderEMF(pBBemf, bb) || !bb)
	{
		UT_DEBUGMSG(("IE_ImpGraphic_EMF::convertGraphic render failed\n"));
		return UT_ERROR;
	}
	pBBpng = std::move(bb);
	return UT_OK;
}
