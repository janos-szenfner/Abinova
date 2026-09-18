/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* libwpd
 * Version: MPL 2.0 / LGPLv2.1+
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Major Contributor(s):
 * Copyright (C) 2005 Fridrich Strba (fridrich.strba@bluewin.ch)
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

#include "WPXSubDocument.h"
#include "WP3Parser.h"
#include "libwpd_internal.h"
#include "WPXListener.h"
#include <string.h>

WPXSubDocument::WPXSubDocument(librevenge::RVNGInputStream *input, WPXEncryption *encryption, const unsigned dataSize) :
	m_stream(),
	m_streamData(new unsigned char[dataSize])
{
	unsigned i=0;
	for (; i<dataSize && !input->isEnd(); i++)
		m_streamData[i] = readU8(input, encryption);
	m_stream.reset(new WPXMemoryInputStream(m_streamData.get(), i));
}

WPXSubDocument::WPXSubDocument(unsigned char *streamData, const unsigned dataSize) :
	m_stream(),
	m_streamData()
{
	if (streamData)
		m_stream.reset(new WPXMemoryInputStream(streamData, dataSize));
}

WPXSubDocument::~WPXSubDocument()
{
}
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
