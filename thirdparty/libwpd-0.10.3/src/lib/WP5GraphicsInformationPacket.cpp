/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* libwpd
 * Version: MPL 2.0 / LGPLv2.1+
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Major Contributor(s):
 * Copyright (C) 2005, 2007 Fridrich Strba (fridrich.strba@bluewin.ch)
 *
 * For minor contributions see the git repository.
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU Lesser General Public License Version 2.1 or later
 * (LGPLv2.1+), in which case the provisions of the LGPLv2.1+ are
 * applicable instead of those above.
 *
 * For further information visit http://libwpd.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#include "WP5GraphicsInformationPacket.h"
#include "libwpd_internal.h"

WP5GraphicsInformationPacket::WP5GraphicsInformationPacket(librevenge::RVNGInputStream *input, WPXEncryption *encryption, int /* id */, unsigned dataOffset, unsigned dataSize) :
	WP5GeneralPacketData(),
	m_images()
{
	_read(input, encryption, dataOffset, dataSize);
}

WP5GraphicsInformationPacket::~WP5GraphicsInformationPacket()
{
}

void WP5GraphicsInformationPacket::_readContents(librevenge::RVNGInputStream *input, WPXEncryption *encryption, unsigned /* dataSize */)
{
	unsigned short tmpImagesCount = readU16(input, encryption);
	std::vector<unsigned> tmpImagesSizes;
	for (unsigned short i = 0; i < tmpImagesCount; i++)
		tmpImagesSizes.push_back(readU32(input, encryption));

	for (unsigned short j = 0; j < tmpImagesCount; j++)
	{
		const std::unique_ptr<unsigned char[]> tmpData{new unsigned char[tmpImagesSizes[j]]};

		for (unsigned k = 0; k < tmpImagesSizes[j]; k++)
			tmpData[k] = readU8(input, encryption);
#if 0
		librevenge::RVNGString filename;
		filename.sprintf("binarydump%.4x.wpg", j);
		FILE *f = fopen(filename.cstr(), "wb");
		if (f)
		{
			if (tmpData[0]!=0xff || tmpData[1]!='W'  || tmpData[2]!='P'  || tmpData[3]!='C')
			{
				// Here we create a file header, since it looks like some embedded files do not contain it
				fprintf(f, "%c%c%c%c", 0xff, 0x57, 0x50, 0x43);
				fprintf(f, "%c%c%c%c", 0x10, 0x00, 0x00, 0x00);
				fprintf(f, "%c%c%c%c", 0x01, 0x16, 0x01, 0x00);
				fprintf(f, "%c%c%c%c", 0x00, 0x00, 0x00, 0x00);
			}

			for (unsigned l = 0; l < tmpImagesSizes[j]; l++)
				fprintf(f, "%c", tmpData[l]);
			fclose(f);
		}
#endif
		std::unique_ptr<librevenge::RVNGBinaryData> image {new librevenge::RVNGBinaryData(tmpData.get(), tmpImagesSizes[j])};
		m_images.push_back(std::move(image));
	}
}
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
