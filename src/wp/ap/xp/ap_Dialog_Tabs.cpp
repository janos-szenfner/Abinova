/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
/* Abinova
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

#include <algorithm>

#include "ut_assert.h"
#include "ut_string.h"
#include "ut_units.h"

#include "xap_App.h"
#include "xap_Prefs.h"
#include "ap_Prefs.h"
#include "xap_Dialog_Id.h"
#include "xap_DialogFactory.h"

#include "ap_Dialog_Tabs.h"

static char s_typeLetter(eTabType iType)
{
	switch (iType)
	{
	case FL_TAB_RIGHT:   return 'R';
	case FL_TAB_CENTER:  return 'C';
	case FL_TAB_DECIMAL: return 'D';
	case FL_TAB_BAR:     return 'B';
	case FL_TAB_LEFT:
	default:             return 'L';
	}
}

AP_Dialog_Tabs::AP_Dialog_Tabs(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id)
	: XAP_Dialog_NonPersistent(pDlgFactory, id, "interface/dialogtabs"),
	  m_dim(DIM_IN),
	  m_answer(a_CANCEL)
{
	std::string rulerUnits;
	if (XAP_App::getApp()->getPrefsValue(AP_PREF_KEY_RulerUnits, rulerUnits))
	{
		const UT_Dimension dim = UT_determineDimension(rulerUnits.c_str(), DIM_none);
		if (dim != DIM_none)
		{
			m_dim = dim;
		}
	}
}

AP_Dialog_Tabs::~AP_Dialog_Tabs(void)
{
}

AP_Dialog_Tabs::tAnswer AP_Dialog_Tabs::getAnswer(void) const
{
	return m_answer;
}

bool AP_Dialog_Tabs::setDialogData(const PP_PropertyVector & pProps)
{
	m_vecStops.clear();
	m_sDefaultInterval.clear();

	for (size_t i = 0; i + 1 < pProps.size(); i += 2)
	{
		if (pProps[i] == "tabstops")
		{
			std::vector<std::unique_ptr<fl_TabStop>> vecTabs;
			buildTabStops(pProps[i + 1].c_str(), vecTabs);
			for (auto & pTab : vecTabs)
			{
				m_vecStops.push_back({ pTab->getPosition(),
									   pTab->getType(), pTab->getLeader() });
			}
		}
		else if (pProps[i] == "default-tab-interval")
		{
			m_sDefaultInterval = pProps[i + 1];
		}
	}
	if (m_sDefaultInterval.empty())
	{
		m_sDefaultInterval = "0.5in";
	}
	return true;
}

bool AP_Dialog_Tabs::getDialogData(PP_PropertyVector & pProps)
{
	pProps.push_back("tabstops");
	pProps.push_back(serializeStops());
	if (!m_sDefaultInterval.empty())
	{
		pProps.push_back("default-tab-interval");
		pProps.push_back(m_sDefaultInterval);
	}
	return true;
}

void AP_Dialog_Tabs::setStop(UT_sint32 iPosition, eTabType iType, eTabLeader iLeader)
{
	for (auto & s : m_vecStops)
	{
		if (std::abs(s.iPosition - iPosition) <= 1)
		{
			s.iPosition = iPosition;
			s.iType = iType;
			s.iLeader = iLeader;
			return;
		}
	}
	m_vecStops.push_back({ iPosition, iType, iLeader });
	std::sort(m_vecStops.begin(), m_vecStops.end(),
			  [](const Stop & a, const Stop & b) { return a.iPosition < b.iPosition; });
}

void AP_Dialog_Tabs::clearStop(UT_sint32 index)
{
	if (index < 0 || static_cast<size_t>(index) >= m_vecStops.size())
	{
		return;
	}
	m_vecStops.erase(m_vecStops.begin() + index);
}

void AP_Dialog_Tabs::clearAllStops(void)
{
	m_vecStops.clear();
}

std::string AP_Dialog_Tabs::formatPosition(UT_sint32 iPosition) const
{
	const double dInches = static_cast<double>(iPosition) / UT_LAYOUT_RESOLUTION;
	return UT_convertInchesToDimensionString(m_dim, dInches);
}

std::string AP_Dialog_Tabs::serializeStops(void) const
{
	std::string buf;
	for (const Stop & s : m_vecStops)
	{
		if (!buf.empty())
		{
			buf += ",";
		}
		buf += formatPosition(s.iPosition);
		buf += "/";
		buf += s_typeLetter(s.iType);
		buf += static_cast<char>('0' + static_cast<int>(s.iLeader));
	}
	return buf;
}
