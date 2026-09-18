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

#ifndef WP1STYLESLISTENER_H
#define WP1STYLESLISTENER_H

#include "WP1Listener.h"
#include "WP1SubDocument.h"
#include "WPXStylesListener.h"
#include <memory>
#include "WPXPageSpan.h"
#include "WPXTable.h"
#include "WPXTableList.h"

class WP1StylesListener : public WP1Listener, protected WPXStylesListener
{
public:
	explicit WP1StylesListener(std::list<WPXPageSpan> &pageList);
	~WP1StylesListener() override {}

	void startDocument() override {}
	void startSubDocument() override {}
	void insertCharacter(unsigned /* character */) override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void insertExtendedCharacter(unsigned char /* extendedCharacter */) override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void insertTab() override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void insertEOL() override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void insertBreak(unsigned char breakType) override;
	void insertNote(WPXNoteType /* noteType */, WP1SubDocument * /* subDocument */) override {}
	void attributeChange(bool /* isOn */, unsigned char /* attribute */) override {}
	void fontPointSize(unsigned char /* pointSize */) override {}
	void fontId(unsigned short /* id */) override {}
	void marginReset(unsigned short leftMargin, unsigned short rightMargin) override;
	void topMarginSet(unsigned short topMargin) override;
	void bottomMarginSet(unsigned short bottomMargin) override;
	void leftIndent(unsigned short /* leftMarginOffset */) override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void leftRightIndent(unsigned short /* leftRightMarginOffset */) override
	{
		if (!isUndoOn()) m_currentPageHasContent = true;
	}
	void leftMarginRelease(unsigned short /* release */) override {}
	void setTabs(const std::vector<WPXTabStop> & /* tabStops */) override {}
	void headerFooterGroup(unsigned char headerFooterDefinition, const std::shared_ptr<WP1SubDocument> &subDocument) override;
	void suppressPageCharacteristics(unsigned char suppressCode) override;
	void justificationChange(unsigned char /* justification */) override {}
	void lineSpacingChange(unsigned char /* spacing */) override {}
	void flushRightOn() override {}
	void flushRightOff() override {}
	void centerOn() override {}
	void centerOff() override {}
	void endDocument() override;
	void endSubDocument() override;
	void insertPicture(unsigned short /* width */, unsigned short /* height */, const librevenge::RVNGBinaryData & /* binaryData */) override {}

protected:
	void _handleSubDocument(const WPXSubDocument *subDocument, WPXSubDocumentType subDocumentType, WPXTableList tableList, int nextTableIndice = 0);

private:
	WPXPageSpan m_currentPage, m_nextPage;
	double m_tempMarginLeft, m_tempMarginRight;
	bool m_currentPageHasContent;
	bool m_isSubDocument;
	std::list<WPXPageSpan>::iterator m_pageListHardPageMark;
};

#endif /* WP1STYLESLISTENER_H */
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
