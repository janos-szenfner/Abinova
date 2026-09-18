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

#ifndef WPXHEADERFOOTER_H
#define WPXHEADERFOOTER_H

#include <memory>

#include "WPXSubDocument.h"
#include "WPXTableList.h"
#include "libwpd_internal.h"

class WPXHeaderFooter
{
public:
	WPXHeaderFooter(const WPXHeaderFooterType headerFooterType, const WPXHeaderFooterOccurrence occurrence,
	                const unsigned char internalType, const std::shared_ptr<WPXSubDocument> &subDocument, WPXTableList tableList);
	WPXHeaderFooter(const WPXHeaderFooterType headerFooterType, const WPXHeaderFooterOccurrence occurrence,
	                const unsigned char internalType, const std::shared_ptr<WPXSubDocument> &subDocument);
	WPXHeaderFooterType getType() const
	{
		return m_type;
	}
	WPXHeaderFooterOccurrence getOccurrence() const
	{
		return m_occurrence;
	}
	unsigned char getInternalType() const
	{
		return m_internalType;
	}
	const std::shared_ptr<WPXSubDocument> &getSubDocument() const
	{
		return m_subDocument;
	}
	WPXTableList getTableList() const
	{
		return m_tableList;
	}

private:
	WPXHeaderFooterType m_type;
	WPXHeaderFooterOccurrence m_occurrence;
	unsigned char m_internalType; // for suppression
	std::shared_ptr<WPXSubDocument> m_subDocument;  // for the actual text
	WPXTableList m_tableList;
};

#endif /* WPXHEADERFOOTER_H */
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
