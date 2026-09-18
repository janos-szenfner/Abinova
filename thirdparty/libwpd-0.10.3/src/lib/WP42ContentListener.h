/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* libwpd
 * Version: MPL 2.0 / LGPLv2.1+
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Major Contributor(s):
 * Copyright (C) 2003 William Lachance (wrlach@gmail.com)
 * Copyright (C) 2003 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2005-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
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

#ifndef WP42CONTENTLISTENER_H
#define WP42CONTENTLISTENER_H

#include <memory>

#include "WP42Listener.h"
#include "WP42SubDocument.h"
#include "WPXContentListener.h"
#include <libwpd/libwpd.h>

struct WP42ContentParsingState
{
	WP42ContentParsingState();
	~WP42ContentParsingState();
	librevenge::RVNGString m_textBuffer;
};

class WP42ContentListener : public WP42Listener, protected WPXContentListener
{
public:
	WP42ContentListener(std::list<WPXPageSpan> &pageList, librevenge::RVNGTextInterface *documentInterface);
	~WP42ContentListener() override;

	void startDocument() override
	{
		WPXContentListener::startDocument();
	}
	void startSubDocument() override
	{
		WPXContentListener::startSubDocument();
	}
	void insertCharacter(unsigned character) override;
	void insertTab(unsigned char tabType, double tabPosition) override;
	void insertBreak(unsigned char breakType) override
	{
		WPXContentListener::insertBreak(breakType);
	}
	void insertEOL() override;
	void attributeChange(bool isOn, unsigned char attribute) override;
	void marginReset(unsigned char leftMargin, unsigned char rightMargin) override;
	void headerFooterGroup(unsigned char headerFooterDefinition, const std::shared_ptr<WP42SubDocument> &subDocument) override;
	void suppressPageCharacteristics(unsigned char /* suppressCode */) override {}
	void endDocument() override
	{
		WPXContentListener::endDocument();
	}
	void endSubDocument() override
	{
		WPXContentListener::endSubDocument();
	}

protected:
	void _handleSubDocument(const WPXSubDocument *subDocument, WPXSubDocumentType subDocumentType, WPXTableList tableList, unsigned nextTableIndice = 0) override;

	void _flushText() override;
	void _changeList() override {}

private:
	WP42ContentListener(const WP42ContentListener &);
	WP42ContentListener &operator=(const WP42ContentListener &);
	std::unique_ptr<WP42ContentParsingState> m_parseState;
};

#endif /* WP42LISTENER_H */
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
