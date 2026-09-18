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
 * Copyright (C) 2006 Fridrich Strba (fridrich.strba@bluewin.ch)
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

#ifndef WP3STYLESLISTENER_H
#define WP3STYLESLISTENER_H

#include "WP3Listener.h"
#include "WPXStylesListener.h"
#include "WPXPageSpan.h"
#include "WPXTable.h"
#include "WPXTableList.h"

class WP3StylesListener : public WP3Listener, protected WPXStylesListener
{
public:
	WP3StylesListener(std::list<WPXPageSpan> &pageList, WPXTableList tableList);

	void startDocument() override {}
	void startSubDocument() override {}
	void insertCharacter(unsigned /* character */) override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void insertTab() override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void insertTab(unsigned char /* tabType */, double /* tabPosition */) override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void insertEOL() override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void insertBreak(unsigned char breakType) override;
	void attributeChange(bool /* isOn */, unsigned char /* attribute */) override {}
	void lineSpacingChange(double /* lineSpacing */) override {}
	void justificationChange(unsigned char /* justification */) override {}
	void pageMarginChange(unsigned char side, unsigned short margin) override;
	void pageFormChange(unsigned short length, unsigned short width, WPXFormOrientation orientation) override;
	void marginChange(unsigned char side, unsigned short margin) override;
	void indentFirstLineChange(double /* offset */) override {}
	void setTabs(bool /* isRelative */, const std::vector<WPXTabStop> /* tabStops */) override {}
	void columnChange(WPXTextColumnType /* columnType */, unsigned char /* numColumns */,
	                  const std::vector<double> & /* columnWidth */, const std::vector<bool> & /* isFixedWidth */) override {}
	void endDocument() override;
	void endSubDocument() override;

	void defineTable(unsigned char /* position */, unsigned short /* leftOffset */) override {}
	void addTableColumnDefinition(unsigned /* width */, unsigned /* leftGutter */, unsigned /* rightGutter */,
	                              unsigned /* attributes */, unsigned char /* alignment */) override {}
	void startTable() override;
	void closeCell() override {}
	void closeRow() override {}
	void setTableCellSpan(unsigned short /* colSpan */, unsigned short /* rowSpan */) override {}
	void setTableCellFillColor(const RGBSColor * /* cellFillColor */) override {}
	void endTable() override {}
	void undoChange(unsigned char undoType, unsigned short undoLevel) override;
	void setTextColor(const RGBSColor * /* fontColor */) override {}
	void setTextFont(const librevenge::RVNGString & /* fontName */) override {}
	void setFontSize(unsigned short /* fontSize */) override {}
	void insertPageNumber(const librevenge::RVNGString & /* pageNumber */) override {}
	void insertNoteReference(const librevenge::RVNGString & /* noteReference */) override {}
	void insertNote(WPXNoteType /* noteType */, const WP3SubDocument * /* subDocument */) override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void headerFooterGroup(unsigned char headerFooterType, unsigned char occurrenceBits, const std::shared_ptr<WP3SubDocument> &subDocument) override;
	void suppressPage(unsigned short suppressCode) override;
	void backTab() override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void leftIndent() override {}
	void leftIndent(double /* offset */) override {}
	void leftRightIndent() override {}
	void leftRightIndent(double /* offset */) override {}
	void insertPicture(double /* height */, double /* width */, double /* verticalOffset */, double /* horizontalOffset */, unsigned char /* leftColumn */, unsigned char /* rightColumn */,
	                   unsigned short /* figureFlags */, const librevenge::RVNGBinaryData & /* binaryData */) override {}
	void insertTextBox(double /* height */, double /* width */, double /* verticalOffset */, double /* horizontalOffset */, unsigned char /* leftColumn */, unsigned char /* rightColumn */,
	                   unsigned short /* figureFlags */, const WP3SubDocument * /* subDocument */, const WP3SubDocument * /* caption */) override {}
	void insertWP51Table(double /* height */, double /* width */, double /* verticalOffset */, double /* horizontalOffset */, unsigned char /* leftColumn */, unsigned char /* rightColumn */,
	                     unsigned short /* figureFlags */, const WP3SubDocument * /* subDocument */, const WP3SubDocument * /* caption */) override {}

protected:
	void _handleSubDocument(const WPXSubDocument *subDocument, WPXSubDocumentType subDocumentType, WPXTableList tableList, int nextTableIndice = 0);

private:
	WP3StylesListener(const WP3StylesListener &);
	WP3StylesListener &operator=(const WP3StylesListener &);
	WPXPageSpan m_currentPage;

	WPXTableList m_tableList;
	std::shared_ptr<WPXTable> m_currentTable;
	double m_tempMarginLeft, m_tempMarginRight;
	bool m_currentPageHasContent;
	bool m_isSubDocument;
	std::list<WPXPageSpan>::iterator m_pageListHardPageMark;
};

#endif /* WP3STYLESLISTENER_H */
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
