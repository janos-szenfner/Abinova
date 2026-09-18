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
 * Copyright (C) 2005-2007 Fridrich Strba (fridrich.strba@bluewin.ch)
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

#ifndef WP5CONTENTLISTENER_H
#define WP5CONTENTLISTENER_H

#include "WP5Listener.h"
#include "WPXContentListener.h"
#include "WP5SubDocument.h"
#include "libwpd_internal.h"
#include <memory>

struct WP5ContentParsingState
{
	WP5ContentParsingState();
	~WP5ContentParsingState();
	librevenge::RVNGString m_textBuffer;
	librevenge::RVNGString m_noteReference;

	WPXTableList m_tableList;

	bool m_isFrameOpened;
};

class WP5ContentListener : public WP5Listener, protected WPXContentListener
{
public:
	WP5ContentListener(std::list<WPXPageSpan> &pageList, librevenge::RVNGTextInterface *documentInterface);
	~WP5ContentListener() override;

	void startDocument() override
	{
		WPXContentListener::startDocument();
	}
	void startSubDocument() override
	{
		WPXContentListener::startSubDocument();
	}
	void setFont(const librevenge::RVNGString &fontName, double fontSize) override;
	void setTabs(const std::vector<WPXTabStop> &tabStops, unsigned short tabOffset) override;
	void insertCharacter(unsigned character) override;
	void insertTab(unsigned char tabType, double tabPosition) override;
	void insertIndent(unsigned char indentType, double indentPosition) override;
	void insertEOL() override;
	void insertBreak(unsigned char breakType) override
	{
		WPXContentListener::insertBreak(breakType);
	}
	void lineSpacingChange(double lineSpacing) override
	{
		WPXContentListener::lineSpacingChange(lineSpacing);
	}
	void justificationChange(unsigned char justification) override
	{
		WPXContentListener::justificationChange(justification);
	}
	void characterColorChange(unsigned char red, unsigned char green, unsigned char blue) override;
	void attributeChange(bool isOn, unsigned char attribute) override;
	void pageMarginChange(unsigned char /* side */, unsigned short /* margin */) override {}
	void pageFormChange(unsigned short /* length */, unsigned short /* width */, WPXFormOrientation /* orientation */) override {}
	void marginChange(unsigned char side, unsigned short margin) override;
	void paragraphMarginChange(unsigned char /* side */, signed short /* margin */) {}
	void endDocument() override
	{
		WPXContentListener::endDocument();
	}
	void endSubDocument() override
	{
		WPXContentListener::endSubDocument();
	}

	void defineTable(unsigned char position, unsigned short leftOffset) override;
	void addTableColumnDefinition(unsigned width, unsigned leftGutter, unsigned rightGutter,
	                              unsigned attributes, unsigned char alignment) override;
	void startTable() override;
	void insertRow(unsigned short rowHeight, bool isMinimumHeight, bool isHeaderRow) override;
	void insertCell(unsigned char colSpan, unsigned char rowSpan, unsigned char borderBits,
	                const RGBSColor *cellFgColor, const RGBSColor *cellBgColor,
	                const RGBSColor *cellBorderColor, WPXVerticalAlignment cellVerticalAlignment,
	                bool useCellAttributes, unsigned cellAttributes) override;
	void endTable() override;

	void insertNoteReference(const librevenge::RVNGString &noteReference) override;
	void insertNote(WPXNoteType noteType, const WP5SubDocument *subDocument) override;
	void headerFooterGroup(unsigned char headerFooterType, unsigned char occurrenceBits, const std::shared_ptr<WP5SubDocument> &subDocument) override;
	void suppressPageCharacteristics(unsigned char /* suppressCode */) override {}

	void setDefaultFont(const librevenge::RVNGString &fontName, double fontSize);

	void boxOn(unsigned char positionAndType, unsigned char alignment, unsigned short width, unsigned short height, unsigned short x, unsigned short y) override;
	void boxOff() override;
	void insertGraphicsData(const librevenge::RVNGBinaryData *data) override;

protected:
	void _handleSubDocument(const WPXSubDocument *subDocument, WPXSubDocumentType subDocumentType, WPXTableList tableList, unsigned nextTableIndice = 0) override;

	void _flushText() override;
	void _changeList() override {}

private:
	WP5ContentListener(const WP5ContentListener &);
	WP5ContentListener &operator=(const WP5ContentListener &);
	std::unique_ptr<WP5ContentParsingState> m_parseState;
	double m_defaultFontSize;
	librevenge::RVNGString m_defaultFontName;
};

#endif /* WP5CONTENTLISTENER_H */
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
