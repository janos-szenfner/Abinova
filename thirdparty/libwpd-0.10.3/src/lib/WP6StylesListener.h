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

// WP6StylesListener: creates an intermediate table representation, given a
// sequence of messages passed to it by the parser.

#ifndef WP6STYLESLISTENER_H
#define WP6STYLESLISTENER_H

#include "WP6Listener.h"
#include "WPXStylesListener.h"
#include <vector>
#include <set>
#include <list>
#include "WPXPageSpan.h"
#include "WPXTable.h"

class WPXSubDocument;

class WP6StylesListener : public WP6Listener, protected WPXStylesListener
{
public:
	WP6StylesListener(std::list<WPXPageSpan> &pageList, WPXTableList tableList);

	void setDate(const unsigned short /* type */, const unsigned short /* year */,
	             const unsigned char /* month */, const unsigned char /* day */,
	             const unsigned char /* hour */, const unsigned char /* minute */,
	             const unsigned char /* second */, const unsigned char /* dayOfWeek */,
	             const unsigned char /* timeZone */, const unsigned char /* unused */) override {}
	void setExtendedInformation(const unsigned short /* type */, const librevenge::RVNGString & /*data*/) override {}
	void startDocument() override {}
	void startSubDocument() override {}
	void setAlignmentCharacter(const unsigned /* character */) override {}
	void setLeaderCharacter(const unsigned /* character */, const unsigned char /* numberOfSpaces */) override {}
	void defineTabStops(const bool /* isRelative */, const std::vector<WPXTabStop> & /* tabStops */,
	                    const std::vector<bool> & /* usePreWP9LeaderMethods */) override {}
	void insertCharacter(unsigned /* character */) override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void insertTab(const unsigned char /* tabType */, double /* tabPosition */) override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void handleLineBreak() override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void insertEOL() override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void insertBreak(const unsigned char breakType) override;
	void characterColorChange(const unsigned char /* red */, const unsigned char /* green */, const unsigned char /* blue */) override {}
	void characterShadingChange(const unsigned char /* shading */) override {}
	void highlightChange(const bool /* isOn */, const RGBSColor & /* color */) override {}
	void fontChange(const unsigned short /* matchedFontPointSize */, const unsigned short /* fontPID */, const librevenge::RVNGString & /* fontName */) override {}
	void attributeChange(const bool /* isOn */, const unsigned char /* attribute */) override {}
	void lineSpacingChange(const double /* lineSpacing */) override {}
	void spacingAfterParagraphChange(const double /* spacingRelative */, const double /* spacingAbsolute */) override {}
	void justificationChange(const unsigned char /* justification */) override {}
	void pageNumberingChange(const WPXPageNumberPosition /* page numbering position */, const unsigned short /* matchedFontPointSize */, const unsigned short /* fontPID */) override;
	void pageMarginChange(const unsigned char side, const unsigned short margin) override;
	void pageFormChange(const unsigned short length, const unsigned short width, const WPXFormOrientation orientation) override;
	void marginChange(const unsigned char side, const unsigned short margin) override;
	void paragraphMarginChange(const unsigned char /* side */, const signed short /* margin */) override {}
	void indentFirstLineChange(const signed short /* offset */) override {}
	void columnChange(const WPXTextColumnType /* columnType */, const unsigned char /* numColumns */,
	                  const std::vector<double> & /* columnWidth */, const std::vector<bool> & /* isFixedWidth */) override {}
	void updateOutlineDefinition(const unsigned short /* outlineHash */, const unsigned char * /* numberingMethods */, const unsigned char /* tabBehaviourFlag */) override {}

	void paragraphNumberOn(const unsigned short /* outlineHash */, const unsigned char /* level */, const unsigned char /* flag */) override {}
	void paragraphNumberOff() override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void displayNumberReferenceGroupOn(const unsigned char /* subGroup */, const unsigned char /* level */) override {}
	void displayNumberReferenceGroupOff(const unsigned char /* subGroup */) override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void styleGroupOn(const unsigned char /* subGroup */) override {}
	void styleGroupOff(const unsigned char /* subGroup */) override {}
	void globalOn(const unsigned char /* systemStyle */) override {}
	void globalOff() override {}
	void noteOn(const unsigned short textPID) override;
	void noteOff(const WPXNoteType /* noteType */) override {}
	void headerFooterGroup(const unsigned char headerFooterType, const unsigned char occurrenceBits, const unsigned short textPID) override;
	void suppressPageCharacteristics(const unsigned char suppressCode) override;
	void setPageNumber(const unsigned short pageNumber) override;
	void setPageNumberingType(const WPXNumberingType pageNumberingType) override;

	void endDocument() override;
	void endSubDocument() override;

	void defineTable(const unsigned char position, const unsigned short leftOffset) override;
	void addTableColumnDefinition(const unsigned /* width */, const unsigned /* leftGutter */, const unsigned /* rightGutter */,
	                              const unsigned /* attributes */, const unsigned char /* alignment */) override {}
	void startTable() override;
	void insertRow(const unsigned short rowHeight, const bool isMinimumHeight, const bool isHeaderRow) override;
	void insertCell(const unsigned char colSpan, const unsigned char rowSpan, const unsigned char borderBits,
	                const RGBSColor *cellFgColor, const RGBSColor *cellBgColor,
	                const RGBSColor *cellBorderColor, const WPXVerticalAlignment cellVerticalAlignment,
	                const bool useCellAttributes, const unsigned cellAttributes) override;
	void endTable() override;
	void boxOn(const unsigned char /* anchoringType */, const unsigned char /* generalPositioningFlags */, const unsigned char /* horizontalPositioningFlags */,
	           const signed short /* horizontalOffset */, const unsigned char /* leftColumn */, const unsigned char /* rightColumn */,
	           const unsigned char /* verticalPositioningFlags */, const signed short /* verticalOffset */, const unsigned char /* widthFlags */,
	           const unsigned short /* width */, const unsigned char /* heightFlags */, const unsigned short /* height */, const unsigned char /* boxContentType */,
	           const unsigned short /* nativeWidth */, const unsigned short /* nativeHeight */,
	           const librevenge::RVNGString & /* linkTarget */) override {}
	void boxOff() override {}
	void insertGraphicsData(const unsigned short /* packetId */) override {}
	void insertTextBox(const WP6SubDocument *subDocument) override;
	void commentAnnotation(const unsigned short textPID) override;

	void undoChange(const unsigned char undoType, const unsigned short undoLevel) override;

protected:
	void _handleSubDocument(const WPXSubDocument *subDocument, WPXSubDocumentType subDocumentType, WPXTableList tableList, int nextTableIndice = 0);

	void _flushText() {}
	void _changeList() {}

private:
	WP6StylesListener(const WP6StylesListener &);
	WP6StylesListener &operator=(const WP6StylesListener &);
	WPXPageSpan m_currentPage;

	WPXTableList m_tableList;
	std::shared_ptr<WPXTable> m_currentTable;
	double m_tempMarginLeft, m_tempMarginRight;
	bool m_currentPageHasContent;
	bool m_isTableDefined;
	bool m_isSubDocument;
	std::set <const WPXSubDocument *> m_subDocuments;
	std::list<WPXPageSpan>::iterator m_pageListHardPageMark;
};

#endif /* WP6STYLESLISTENER_H */
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
