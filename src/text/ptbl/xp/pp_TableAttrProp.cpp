/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
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

#include "ut_types.h"
#include "ut_assert.h"
#include "ut_string.h"
#include "pp_AttrProp.h"
#include "pp_TableAttrProp.h"


/*!
 * Insert pAP into the checksum-sorted table at the position which keeps
 * the table ordered by checksum (first slot not less than the new item).
\param vec the sorted table of non-owning PP_AttrProp pointers
\param pAP the PP_AttrProp to insert
*/
static void insertSortedAP(std::vector<PP_AttrProp *> & vec, PP_AttrProp * pAP)
{
	UT_uint32 checksum = pAP->getCheckSum();
	auto it = std::lower_bound(vec.begin(), vec.end(), checksum,
		[](const PP_AttrProp * pElem, UT_uint32 cs) { return pElem->getCheckSum() < cs; });
	vec.insert(it, pAP);
}

pp_TableAttrProp::pp_TableAttrProp()
{
	m_vecTable.reserve(54); // there seems to be 50+ of these at the moment
	m_vecTableSorted.reserve(54);
}

pp_TableAttrProp::~pp_TableAttrProp()
{
}

bool pp_TableAttrProp::addAP(PP_AttrProp * pAP,
								UT_sint32 * pSubscript)
{
 	UT_sint32 u = static_cast<UT_sint32>(m_vecTable.size());
 	m_vecTable.emplace_back(pAP);

	if (pSubscript)
	{
		*pSubscript = u;
	}
	pAP->setIndex(u);	//$HACK
	insertSortedAP(m_vecTableSorted, pAP);

	return true;
}

bool pp_TableAttrProp::createAP(UT_sint32 * pSubscript)
{
	PP_AttrProp * pNew = new PP_AttrProp();
	if (!pNew)
		return false;
 	UT_sint32 u = static_cast<UT_sint32>(m_vecTable.size());
 	m_vecTable.emplace_back(pNew);

	pNew->setIndex(u);	//$HACK

	if (pSubscript)
 	{
 		*pSubscript = u;
 	}
	else
	{
		// create default empty AP
		pNew->markReadOnly();
		m_vecTableSorted.push_back(pNew);
	} 

	return true;
}

bool pp_TableAttrProp::createAP(const PP_PropertyVector & attributes,
								   const PP_PropertyVector & properties,
								   UT_sint32 * pSubscript)
{
	UT_sint32 subscript;
	if (!createAP(&subscript))
		return false;

	PP_AttrProp * pAP = m_vecTable[subscript].get();
	UT_return_val_if_fail (pAP,false);
	if (!pAP->setAttributes(attributes) || !pAP->setProperties(properties))
		return false;

	pAP->markReadOnly();

	insertSortedAP(m_vecTableSorted, pAP);

	*pSubscript = subscript;
	return true;
}

bool pp_TableAttrProp::createAP(const PP_PropertyVector & pVector,
								   UT_sint32 * pSubscript)
{
	UT_sint32 subscript;
	if (!createAP(&subscript))
		return false;

	PP_AttrProp * pAP = m_vecTable[subscript].get();
	UT_return_val_if_fail (pAP, false);
	if (!pAP->setAttributes(pVector))
		return false;

	pAP->markReadOnly();

	insertSortedAP(m_vecTableSorted, pAP);

	*pSubscript = subscript;
	return true;
}

bool pp_TableAttrProp::findMatch(const PP_AttrProp * pMatch,
									UT_sint32 * pSubscript) const
{
	// return true if we find an AP in our table which is
	// an exact match for the attributes/properties in pMatch.
	// set *pSubscript to the subscript of the matching item.

	UT_uint32 checksum = pMatch->getCheckSum();
	auto it = std::lower_bound(m_vecTableSorted.begin(), m_vecTableSorted.end(), checksum,
		[](const PP_AttrProp * pElem, UT_uint32 cs) { return pElem->getCheckSum() < cs; });

	for (; it != m_vecTableSorted.end(); ++it)
 	{
		PP_AttrProp * pK = *it;
		if (checksum != pK->getCheckSum())
		{
			break;
		}
		if (pMatch->isExactMatch(*pK))
  		{
 			// Need to return an index of the element in the MAIN
 			// vector table
			*pSubscript = pK->getIndex();
			return true;
		}
	}
	return false;
}

	
const PP_AttrProp * pp_TableAttrProp::getAP(UT_sint32 subscript) const
{
	if (subscript >= 0 && subscript < static_cast<UT_sint32>(m_vecTable.size()))
		return m_vecTable[subscript].get();
	else
		return nullptr;
}
