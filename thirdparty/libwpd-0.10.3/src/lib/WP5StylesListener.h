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
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2006-2007 Fridrich Strba (fridrich.strba@bluewin.ch)
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

#ifndef WP5STYLESLISTENER_H
#define WP5STYLESLISTENER_H

#include "WP5Listener.h"
#include "WPXStylesListener.h"
#include <list>
#include "WPXPageSpan.h"
#include "WPXTable.h"
#include "WP5SubDocument.h"
#include "WPXTableList.h"

class WP5StylesListener : public WP5Listener, protected WPXStylesListener
{
public:
	WP5StylesListener(std::list<WPXPageSpan> &pageList, WPXTableList tableList);

	void startDocument() override {}
	void startSubDocument() override {}
	void setFont(const librevenge::RVNGString & /* fontName */, double /* fontSize */) override {}
	void setTabs(const std::vector<WPXTabStop> & /* tabStops */, unsigned short /* tabOffset */) override {}
	void insertCharacter(unsigned /* character */) override
	{
		/*if (!isUndoOn())*/ m_currentPageHasContent = true;
	}
	void insertTab(unsigned char /* tabType */, double /* tabPosition */) override
	{
		/*if (!isUndoOn())*/ m_currentPageHasContent = true;
	}
	void insertIndent(unsigned char /* indentType */, double /* indentPosition */) override
	{
		/*if (!isUndoOn())*/ m_currentPageHasContent = true;
	}
	void characterColorChange(unsigned char /* red */, unsigned char /* green */, unsigned char /* blue */) override {}
	void insertEOL() override
	{
		/*if (!isUndoOn())*/ m_currentPageHasContent = true;
	}
	void insertBreak(unsigned char breakType) override;
	void attributeChange(bool /* isOn */, unsigned char /* attribute */) override {}
	void lineSpacingChange(double /* lineSpacing */) override {}
	void justificationChange(unsigned char /* justification */) override {}
	void pageMarginChange(unsigned char side, unsigned short margin) override;
	void pageFormChange(unsigned short length, unsigned short width, WPXFormOrientation orientation) override;
	void marginChange(unsigned char side, unsigned short margin) override;
	void endDocument() override;
	void endSubDocument() override;

	void defineTable(unsigned char /* position */, unsigned short /* leftOffset */) override {}
	void addTableColumnDefinition(unsigned /* width */, unsigned /* leftGutter */, unsigned /* rightGutter */,
	                              unsigned /* attributes */, unsigned char /* alignment */) override {}
	void startTable() override;
	void insertRow(unsigned short rowHeight, bool isMinimumHeight, bool isHeaderRow) override;
	void insertCell(unsigned char colSpan, unsigned char rowSpan, unsigned char borderBits,
	                const RGBSColor *cellFgColor, const RGBSColor *cellBgColor,
	                const RGBSColor *cellBorderColor, WPXVerticalAlignment cellVerticalAlignment,
	                bool useCellAttributes, unsigned cellAttributes) override;
	void endTable() override {}

	void insertNoteReference(const librevenge::RVNGString & /* noteReference */) override {}
	void insertNote(WPXNoteType /* noteType */, const WP5SubDocument * /* subDocument */) override {}
	void headerFooterGroup(unsigned char headerFooterType, unsigned char occurrenceBits, const std::shared_ptr<WP5SubDocument> &subDocument) override;
	void suppressPageCharacteristics(unsigned char suppressCode) override;

	void boxOn(unsigned char /* positionAndType */, unsigned char /* alignment */, unsigned short /* width */, unsigned short /* height */, unsigned short /* x */, unsigned short /* y */) override {}
	void boxOff() override {}
	void insertGraphicsData(const librevenge::RVNGBinaryData * /* data */) override {}

protected:
	void _handleSubDocument(const WPXSubDocument *subDocument, WPXSubDocumentType subDocumentType, WPXTableList tableList, int nextTableIndice = 0);

private:
	WP5StylesListener(const WP5StylesListener &);
	WP5StylesListener &operator=(const WP5StylesListener &);
	WPXPageSpan m_currentPage, m_nextPage;

	WPXTableList m_tableList;
	std::shared_ptr<WPXTable> m_currentTable;
	double m_tempMarginLeft, m_tempMarginRight;
	bool m_currentPageHasContent;
	bool m_isSubDocument;
	std::list<WPXPageSpan>::iterator m_pageListHardPageMark;
};

#endif /* WP5STYLESLISTENER_H */
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
