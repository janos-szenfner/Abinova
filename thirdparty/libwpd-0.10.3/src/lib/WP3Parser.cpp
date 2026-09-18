/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* libwpd
 * Version: MPL 2.0 / LGPLv2.1+
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Major Contributor(s):
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2004 Fridrich Strba (fridrich.strba@bluewin.ch)
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

#include "WP3Parser.h"

#include <memory>

#include "WPXHeader.h"
#include "WP3Part.h"
#include "WP3ContentListener.h"
#include "WP3StylesListener.h"
#include "WP3ResourceFork.h"
#include "libwpd_internal.h"
#include "WPXTable.h"
#include "WPXTableList.h"

WP3Parser::WP3Parser(librevenge::RVNGInputStream *input, WPXHeader *header, WPXEncryption *encryption) :
	WPXParser(input, header, encryption)
{
}

WP3Parser::~WP3Parser()
{
}

WP3ResourceFork *WP3Parser::getResourceFork(librevenge::RVNGInputStream *input, WPXEncryption *encryption)
{
	// Certain WP2 documents actually don't contain resource fork, so check for its existence
	if (!getHeader() || getHeader()->getDocumentOffset() <= 0x10)
	{
		WPD_DEBUG_MSG(("WP3Parser: Document does not contain resource fork\n"));
		return nullptr;
	}

	return new WP3ResourceFork(input, encryption);
}

void WP3Parser::parse(librevenge::RVNGInputStream *input, WPXEncryption *encryption, WP3Listener *listener)
{
	listener->startDocument();

	input->seek(getHeader()->getDocumentOffset(), librevenge::RVNG_SEEK_SET);

	WPD_DEBUG_MSG(("WordPerfect: Starting document body parse (position = %ld)\n",(long)input->tell()));

	parseDocument(input, encryption, listener);

	listener->endDocument();
}

// parseDocument: parses a document body (may call itself recursively, on other streams, or itself)
void WP3Parser::parseDocument(librevenge::RVNGInputStream *input, WPXEncryption *encryption, WP3Listener *listener)
{
	while (!input->isEnd())
	{
		unsigned char readVal;
		readVal = readU8(input, encryption);

		if (readVal == 0 || readVal == 0x7F || readVal == 0xFF)
		{
			// FIXME: VERIFY: is this IF clause correct? (0xFF seems to be OK at least)
			// do nothing: this token is meaningless and is likely just corruption
		}
		else if (readVal >= (unsigned char)0x01 && readVal <= (unsigned char)0x1F)
		{
			// control characters ?
		}
		else if (readVal >= (unsigned char)0x20 && readVal <= (unsigned char)0x7E)
		{
			listener->insertCharacter(readVal);
		}
		else
		{
			std::unique_ptr<WP3Part> part(WP3Part::constructPart(input, encryption, readVal));
			if (part)
				part->parse(listener);
		}
	}
}

void WP3Parser::parse(librevenge::RVNGTextInterface *textInterface)
{
	librevenge::RVNGInputStream *input = getInput();
	WPXEncryption *encryption = getEncryption();
	std::list<WPXPageSpan> pageList;
	WPXTableList tableList;

	try
	{
		const std::unique_ptr<WP3ResourceFork> resourceFork{getResourceFork(input, encryption)};

		// do a "first-pass" parse of the document
		// gather table border information, page properties (per-page)
		WP3StylesListener stylesListener(pageList, tableList);
		stylesListener.setResourceFork(resourceFork.get());
		parse(input, encryption, &stylesListener);

		// postprocess the pageList == remove duplicate page spans due to the page breaks
		auto previousPage = pageList.begin();
		for (auto Iter=pageList.begin(); Iter != pageList.end(); /* Iter++ */)
		{
			if ((Iter != previousPage) && (*previousPage==*Iter))
			{
				(*previousPage).setPageSpan((*previousPage).getPageSpan() + (*Iter).getPageSpan());
				Iter = pageList.erase(Iter);
			}
			else
			{
				previousPage = Iter;
				++Iter;
			}
		}

		// second pass: here is where we actually send the messages to the target app
		// that are necessary to emit the body of the target document
		WP3ContentListener listener(pageList, textInterface); // FIXME: SHOULD BE CONTENT_LISTENER, AND SHOULD BE PASSED TABLE DATA!
		listener.setResourceFork(resourceFork.get());
		parse(input, encryption, &listener);
	}
	catch (FileException)
	{
		WPD_DEBUG_MSG(("WordPerfect: File Exception. Parse terminated prematurely."));
		throw FileException();
	}
}

void WP3Parser::parseSubDocument(librevenge::RVNGTextInterface *textInterface)
{
	std::list<WPXPageSpan> pageList;
	WPXTableList tableList;

	librevenge::RVNGInputStream *input = getInput();

	try
	{
		WP3StylesListener stylesListener(pageList, tableList);
		stylesListener.startSubDocument();
		parseDocument(input, nullptr, &stylesListener);
		stylesListener.endSubDocument();

		input->seek(0, librevenge::RVNG_SEEK_SET);

		WP3ContentListener listener(pageList, textInterface);
		listener.startSubDocument();
		parseDocument(input, nullptr, &listener);
		listener.endSubDocument();
	}
	catch (FileException)
	{
		WPD_DEBUG_MSG(("WordPerfect: File Exception. Parse terminated prematurely."));
		throw FileException();
	}
}

/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
