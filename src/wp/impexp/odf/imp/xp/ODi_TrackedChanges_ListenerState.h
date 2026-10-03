/* AbiSource
 *
 * Copyright (C) 2025-2026 Abinova contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#pragma once

#include <string>

// Internal includes
#include "ODi_ListenerState.h"
#include "ODi_Abi_Data.h"
#include "ODi_ElementStack.h"

// Abinova classes
class PD_Document;

/**
 * Parses <text:tracked-changes>: each <text:changed-region> declares a
 * type element (<text:insertion>, <text:deletion> or
 * <text:format-change>) plus an <office:change-info> carrying
 * <dc:creator>/<dc:date>.  The region is registered as an
 * AD_Revision so the body marks (<text:change-start>, <text:change>
 * and <text:change-end>) that reference it can stamp piece-table
 * revision tokens.  Unlike OOXML, ODF moves deleted content out of
 * the body into the region's <text:deletion> element, so its inner
 * XML is recorded for the text-content state to replay at the mark.
 */
class ODi_TrackedChanges_ListenerState : public ODi_ListenerState {

public:

	ODi_TrackedChanges_ListenerState(PD_Document* pDocument,
					 ODi_ElementStack& rElementStack,
					 ODi_Abi_Data* pAbiData);

    void startElement (const gchar* pName, const gchar** ppAtts,
                       ODi_ListenerStateAction& rAction) override;

    void endElement (const gchar* pName,
                     ODi_ListenerStateAction& rAction) override;

    void charData (const gchar* pBuffer, int length) override;

private:

	void _commitRegion();

	PD_Document* m_pAbiDocument;
	ODi_Abi_Data* m_pAbiData;

	ODi_ChangeRegion m_region;
	std::string m_regionId;
	std::string m_author;
	std::string m_date;

	// XML recording depth inside the current <text:deletion>; its
	// children are captured for replay, not emitted as live text.
	UT_uint32 m_recordDepth;

	// <office:change-info> may sit either directly under
	// <text:changed-region> or inside the type element; its children
	// are captured as region metadata, never recorded.
	bool m_bInChangeInfo;
	bool m_bCapturingAuthor;
	bool m_bCapturingDate;
};
