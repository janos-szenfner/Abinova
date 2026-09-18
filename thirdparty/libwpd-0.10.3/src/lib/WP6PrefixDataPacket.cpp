/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* libwpd
 * Version: MPL 2.0 / LGPLv2.1+
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Major Contributor(s):
 * Copyright (C) 2002 William Lachance (wrlach@gmail.com)
 * Copyright (C) 2002 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2007 Fridrich Strba (fridrich_strba@bluewin.ch)
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

#include "WP6PrefixDataPacket.h"
#include "WP6PrefixIndice.h"
#include "WP6CommentAnnotationPacket.h"
#include "WP6GeneralTextPacket.h"
#include "WP6FontDescriptorPacket.h"
#include "WP6FillStylePacket.h"
#include "WP6DefaultInitialFontPacket.h"
#include "WP6ExtendedDocumentSummaryPacket.h"
#include "WP6OutlineStylePacket.h"
#include "WP6FileStructure.h"
#include "WP6GraphicsBoxStylePacket.h"
#include "WP6TableStylePacket.h"
#include "WP6GraphicsFilenamePacket.h"
#include "WP6GraphicsCachedFileDataPacket.h"
#include "WP6HyperlinkPacket.h"
#include "libwpd_internal.h"

WP6PrefixDataPacket::WP6PrefixDataPacket(librevenge::RVNGInputStream * /* input */, WPXEncryption * /* encryption */) :
	m_dataSize(0)
{
}

std::shared_ptr<WP6PrefixDataPacket> WP6PrefixDataPacket::constructPrefixDataPacket(librevenge::RVNGInputStream *input, WPXEncryption *encryption, const WP6PrefixIndice &prefixIndice)
{
	try
	{
		switch (prefixIndice.getType())
		{
		case WP6_INDEX_HEADER_INITIAL_FONT:
			return std::make_shared<WP6DefaultInitialFontPacket>(input, encryption, prefixIndice.getID(),
			                                                     prefixIndice.getDataOffset(), prefixIndice.getDataSize());
		case WP6_INDEX_HEADER_HYPERLINK:
			return std::make_shared<WP6HyperlinkPacket>(input, encryption, prefixIndice.getID(),
			                                            prefixIndice.getDataOffset(), prefixIndice.getDataSize());
		case WP6_INDEX_HEADER_GENERAL_WORDPERFECT_TEXT:
			return std::make_shared<WP6GeneralTextPacket>(input, encryption, prefixIndice.getID(),
			                                              prefixIndice.getDataOffset(), prefixIndice.getDataSize());
		case WP6_INDEX_HEADER_DESIRED_FONT_DESCRIPTOR_POOL:
			return std::make_shared<WP6FontDescriptorPacket>(input, encryption, prefixIndice.getID(),
			                                                 prefixIndice.getDataOffset(), prefixIndice.getDataSize());
		case WP6_INDEX_HEADER_FILL_STYLE:
			return std::make_shared<WP6FillStylePacket>(input, encryption, prefixIndice.getID(),
			                                            prefixIndice.getDataOffset(), prefixIndice.getDataSize());
		case WP6_INDEX_HEADER_EXTENDED_DOCUMENT_SUMMARY:
			return std::make_shared<WP6ExtendedDocumentSummaryPacket>(input, encryption, prefixIndice.getID(),
			                                                          prefixIndice.getDataOffset(), prefixIndice.getDataSize());
		case WP6_INDEX_HEADER_OUTLINE_STYLE:
			return std::make_shared<WP6OutlineStylePacket>(input, encryption, prefixIndice.getID(),
			                                               prefixIndice.getDataOffset(), prefixIndice.getDataSize());
		case WP6_INDEX_HEADER_GRAPHICS_FILENAME:
			return std::make_shared<WP6GraphicsFilenamePacket>(input, encryption, prefixIndice.getID(), prefixIndice.getFlags(),
			                                                   prefixIndice.getDataOffset(), prefixIndice.getDataSize());
		case WP6_INDEX_HEADER_GRAPHICS_CACHED_FILE_DATA:
			return std::make_shared<WP6GraphicsCachedFileDataPacket>(input, encryption, prefixIndice.getID(),
			                                                         prefixIndice.getDataOffset(), prefixIndice.getDataSize());
		case WP6_INDEX_HEADER_GRAPHICS_BOX_STYLE:
			return std::make_shared<WP6GraphicsBoxStylePacket>(input, encryption, prefixIndice.getID(),
			                                                   prefixIndice.getDataOffset(), prefixIndice.getDataSize());
		case WP6_INDEX_HEADER_TABLE_STYLE:
			return std::make_shared<WP6TableStylePacket>(input, encryption, prefixIndice.getID(),
			                                             prefixIndice.getDataOffset(), prefixIndice.getDataSize());
		case WP6_INDEX_HEADER_COMMENT_ANNOTATION:
			return std::make_shared<WP6CommentAnnotationPacket>(input, encryption, prefixIndice.getID(),
			                                                    prefixIndice.getDataOffset(), prefixIndice.getDataSize());
		default:
			break;
		}
	}
	catch (FileException)
	{
	}
	return nullptr;
}

void WP6PrefixDataPacket::_read(librevenge::RVNGInputStream *input, WPXEncryption *encryption, unsigned dataOffset, unsigned dataSize)
{
	m_dataSize = dataSize;

	if (!m_dataSize)
		return;

	input->seek(dataOffset, librevenge::RVNG_SEEK_SET);

	_readContents(input, encryption);

	// assert that we haven't surpassed the size of the packet?
}
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
