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

#ifndef WP42STYLESLISTENER_H
#define WP42STYLESLISTENER_H

#include "WP42Listener.h"
#include "WP42SubDocument.h"
#include "WPXStylesListener.h"
#include <vector>
#include "WPXPageSpan.h"
#include "WPXTable.h"
#include "WPXTableList.h"

class WP42StylesListener : public WP42Listener, protected WPXStylesListener
{
public:
	explicit WP42StylesListener(std::list<WPXPageSpan> &pageList);

	void startDocument() override {}
	void startSubDocument() override {}
	void insertCharacter(unsigned /* character */) override
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
	void marginReset(unsigned char /* leftMargin */, unsigned char /* rightMargin */) override {}
	void headerFooterGroup(unsigned char headerFooterDefinition, const std::shared_ptr<WP42SubDocument> &subDocument) override;
	void suppressPageCharacteristics(unsigned char suppressCode) override;
	void endDocument() override;
	void endSubDocument() override;

protected:
	void _handleSubDocument(const WPXSubDocument *subDocument, WPXSubDocumentType subDocumentType, WPXTableList tableList, int nextTableIndice = 0);

private:
	WPXPageSpan m_currentPage, m_nextPage;
	double m_tempMarginLeft, m_tempMarginRight;
	bool m_currentPageHasContent;
	bool m_isSubDocument;
	std::list<WPXPageSpan>::iterator m_pageListHardPageMark;
};

#endif /* WP42STYLESLISTENER_H */
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
