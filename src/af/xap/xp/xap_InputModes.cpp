/* AbiSource Application Framework
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2004 Hubert Figuiere
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


#include "ut_assert.h"
#include "ut_string.h"

#include "xap_InputModes.h"

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

XAP_InputModes::XAP_InputModes(void)
{
	m_indexCurrentEventMap = 0;
}

XAP_InputModes::~XAP_InputModes(void)
{
	UT_ASSERT(m_vecEventMaps.size() == m_vecNames.size());

	for (EV_EditEventMapper * _utv_p : m_vecEventMaps) { if (_utv_p) delete(_utv_p); };
	for (char * _utv_p : m_vecNames) { if (_utv_p) g_free(_utv_p); };
}

bool XAP_InputModes::createInputMode(const char * szName,
										EV_EditBindingMap * pBindingMap)
{
	UT_ASSERT(szName && *szName);
	UT_ASSERT(pBindingMap);
	
	char * szDup = nullptr;
	EV_EditEventMapper * pEEM = nullptr;

	szDup = g_strdup(szName);
	UT_ASSERT(szDup);
	
	pEEM = new EV_EditEventMapper(pBindingMap);
	UT_ASSERT(pEEM);

	m_vecEventMaps.push_back(pEEM);
	m_vecNames.push_back(szDup);

	return true;
}

bool XAP_InputModes::setCurrentMap(const char * szName)
{
	UT_uint32 kLimit = m_vecNames.size();
	UT_uint32 k;

	for (k=0; k<kLimit; k++)
		if (g_ascii_strcasecmp(szName, m_vecNames[k]) == 0)
		{
			m_indexCurrentEventMap = k;
			return true;
		}

	return false;
}

EV_EditEventMapper * XAP_InputModes::getCurrentMap(void) const
{
	return m_vecEventMaps[m_indexCurrentEventMap];
}

const char * XAP_InputModes::getCurrentMapName(void) const
{
	return m_vecNames[m_indexCurrentEventMap];
}

EV_EditEventMapper * XAP_InputModes::getMapByName(const char * szName) const
{
	UT_uint32 kLimit = m_vecNames.size();
	UT_uint32 k;

	for (k=0; k<kLimit; k++)
		if (g_ascii_strcasecmp(szName, m_vecNames[k]) == 0)
			return m_vecEventMaps[k];

	return nullptr;
}

