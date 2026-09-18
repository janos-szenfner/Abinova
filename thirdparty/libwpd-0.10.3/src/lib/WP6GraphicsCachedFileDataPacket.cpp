/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* libwpd
 * Version: MPL 2.0 / LGPLv2.1+
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Major Contributor(s):
 * Copyright (C) 2007 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2007 Novell Inc. (http://www.novell.com)
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

#include "WP6GraphicsCachedFileDataPacket.h"
#include "libwpd_internal.h"

WP6GraphicsCachedFileDataPacket::WP6GraphicsCachedFileDataPacket(librevenge::RVNGInputStream *input, WPXEncryption *encryption, int  id, unsigned dataOffset, unsigned dataSize):
	WP6PrefixDataPacket(input, encryption),
	m_id(id),
	m_object()
{
	_read(input, encryption, dataOffset, dataSize);
}

WP6GraphicsCachedFileDataPacket::~WP6GraphicsCachedFileDataPacket()
{
}

void WP6GraphicsCachedFileDataPacket::_readContents(librevenge::RVNGInputStream *input, WPXEncryption *encryption)
{
	unsigned tmpDataSize = getDataSize();
	std::vector<unsigned char> data;
	data.reserve(tmpDataSize);
	for (unsigned i = 0; i < tmpDataSize; i++)
		data.push_back(readU8(input, encryption));
#if 0
	librevenge::RVNGString filename;
	filename.sprintf("binarydump%.4x.wpg", m_id);
	FILE *f = fopen(filename.cstr(), "wb");
	if (f) // don't crash when current directory is on read-only file-system
	{
		for (unsigned j = 0; j < tmpDataSize; j++)
			fprintf(f, "%c", data[j]);
		fclose(f);
	}
#endif
	m_object.reset(new librevenge::RVNGBinaryData(data.data(), data.size()));
}
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
