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

// Class definition include
#include "ODi_TrackedChanges_ListenerState.h"

// Internal includes
#include "ODi_ListenerStateAction.h"

// Abinova includes
#include "pd_Document.h"
#include "pp_Property.h"
#include "ut_debugmsg.h"
#include "ut_string.h"

// External includes
#include <cstring>
#include <ctime>

ODi_TrackedChanges_ListenerState::ODi_TrackedChanges_ListenerState(
		PD_Document* pDocument, ODi_ElementStack& rElementStack,
		ODi_Abi_Data* pAbiData) :
	ODi_ListenerState("TrackedChanges", rElementStack),
	m_pAbiDocument(pDocument),
	m_pAbiData(pAbiData),
	m_recordDepth(0),
	m_bInChangeInfo(false),
	m_bCapturingAuthor(false),
	m_bCapturingDate(false)
{
}

/* dc:date is xsd:dateTime, e.g. "2024-03-02T11:00:00" or with a
 * nanosecond/offset tail; UT_strptime parses the leading fixed part
 * (timestamps are informational only). */
static time_t _parseChangeDate(const std::string & date)
{
	struct tm tm;
	memset(&tm, 0, sizeof(tm));
	tm.tm_isdst = 0;
	if (!UT_strptime(date.c_str(), "%Y-%m-%dT%H:%M:%S", &tm))
		return 0;
	return mktime(&tm);
}

void ODi_TrackedChanges_ListenerState::startElement(const gchar* pName,
													const gchar** ppAtts,
													ODi_ListenerStateAction& rAction)
{
	if (m_bInChangeInfo)
	{
		if (!strcmp(pName, "dc:creator"))
			m_bCapturingAuthor = true;
		else if (!strcmp(pName, "dc:date"))
			m_bCapturingDate = true;
		return;
	}

	if (m_recordDepth)
	{
		// Inside <text:deletion>: metadata is captured, not recorded;
		// everything else is deletion payload.
		if (!strcmp(pName, "office:change-info"))
		{
			m_bInChangeInfo = true;
			return;
		}
		m_region.deletion.startElement(pName, ppAtts);
		m_recordDepth++;
		return;
	}

	if (!strcmp(pName, "text:changed-region"))
	{
		const gchar* pVal = UT_getAttribute("text:id", ppAtts);
		if (!pVal)
			pVal = UT_getAttribute("xml:id", ppAtts);
		m_regionId = pVal ? pVal : "";
		m_author.clear();
		m_date.clear();
		m_region = ODi_ChangeRegion();
	}
	else if (!strcmp(pName, "text:insertion"))
	{
		m_region.type = ODi_ChangeRegion::Change_Insertion;
	}
	else if (!strcmp(pName, "text:deletion"))
	{
		m_region.type = ODi_ChangeRegion::Change_Deletion;
		m_recordDepth = 1;
	}
	else if (!strcmp(pName, "text:format-change"))
	{
		m_region.type = ODi_ChangeRegion::Change_Format;
	}
	else if (!strcmp(pName, "office:change-info"))
	{
		m_bInChangeInfo = true;
	}
	else if (!strcmp(pName, "text:tracked-changes"))
	{
		const gchar* pVal = UT_getAttribute("text:track-changes", ppAtts);
		if (pVal && (!strcmp(pVal, "true") || !strcmp(pVal, "1")))
		{
			const PP_PropertyVector props = {
				"document-track-changes", "1"};
			m_pAbiDocument->setProperties(props);
		}
	}
	else
	{
		rAction.ignoreElement();
	}
}

void ODi_TrackedChanges_ListenerState::endElement(const gchar* pName,
												  ODi_ListenerStateAction& rAction)
{
	if (m_bInChangeInfo)
	{
		if (!strcmp(pName, "dc:creator"))
			m_bCapturingAuthor = false;
		else if (!strcmp(pName, "dc:date"))
			m_bCapturingDate = false;
		else if (!strcmp(pName, "office:change-info"))
			m_bInChangeInfo = false;
		return;
	}

	if (m_recordDepth)
	{
		m_recordDepth--;
		if (m_recordDepth)
			m_region.deletion.endElement(pName);
		// reaching 0 closes the <text:deletion> element itself
		return;
	}

	if (!strcmp(pName, "text:changed-region"))
		_commitRegion();
	else if (!strcmp(pName, "text:tracked-changes"))
		rAction.popState();
}

void ODi_TrackedChanges_ListenerState::charData(const gchar* pBuffer, int length)
{
	if (m_bCapturingAuthor)
		m_author.append(pBuffer, length);
	else if (m_bCapturingDate)
		m_date.append(pBuffer, length);
	else if (m_recordDepth && !m_bInChangeInfo)
		m_region.deletion.charData(pBuffer, length);
}

/* Fresh ids are allocated densely rather than trusting text:change-id
 * (arbitrary strings) because explodeRevisions() walks revision ids
 * 1..max on every revised fragment. */
void ODi_TrackedChanges_ListenerState::_commitRegion()
{
	if (m_regionId.empty())
	{
		UT_DEBUGMSG(("text:changed-region without id ignored\n"));
		return;
	}
	if (m_pAbiData->m_changeRegions.count(m_regionId))
	{
		UT_DEBUGMSG(("duplicate change-region id %s ignored\n",
					 m_regionId.c_str()));
		return;
	}

	m_region.revId = m_pAbiDocument->getHighestRevisionId() + 1;
	m_pAbiDocument->addRevision(m_region.revId, nullptr,
							  _parseChangeDate(m_date), 0, false,
							  m_author.empty() ? nullptr : m_author.c_str());
	m_pAbiData->m_changeRegions[m_regionId] = m_region;
}
