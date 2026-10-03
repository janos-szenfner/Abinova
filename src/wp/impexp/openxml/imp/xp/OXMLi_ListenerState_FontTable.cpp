/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

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
#include "OXMLi_ListenerState_FontTable.h"

// Internal includes
#include "OXML_Document.h"
#include "OXML_FontManager.h"
#include "OXML_Types.h"

// Abinova includes
#include "ut_assert.h"
#include "ut_misc.h"

// External includes
#include <string>

void OXMLi_ListenerState_FontTable::startElement (OXMLi_StartElementRequest * rqst)
{
	if (nameMatches(rqst->pName, NS_W_KEY, "font")) {
		/* a w:font child of the root w:fonts element starts a new
		 * declaration; its w:name attribute is the face name */
		if (contextMatches(OXMLi_contextBack(rqst->context),
						   NS_W_KEY, "fonts")) {
			const gchar * name =
				attrMatches(NS_W_KEY, "name", rqst->ppAtts);
			m_curName = name ? name : "";
			m_curEntry = OXML_FontTableEntry();
		}
		rqst->handled = true;
	}
	else if (!m_curName.empty() &&
			 contextMatches(OXMLi_contextBack(rqst->context),
							NS_W_KEY, "font")) {
		/* substitution metadata children of the current w:font */
		const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
		val = val ? val : "";
		if (nameMatches(rqst->pName, NS_W_KEY, "altName")) {
			m_curEntry.altName = val;
		}
		else if (nameMatches(rqst->pName, NS_W_KEY, "panose1")) {
			m_curEntry.panose1 = val;
		}
		else if (nameMatches(rqst->pName, NS_W_KEY, "charset")) {
			m_curEntry.charset = val;
		}
		else if (nameMatches(rqst->pName, NS_W_KEY, "family")) {
			m_curEntry.family = val;
		}
		else if (nameMatches(rqst->pName, NS_W_KEY, "pitch")) {
			m_curEntry.pitch = val;
		}
		/* w:sig, w:embed*, w:notTrueType are valid children too;
		 * embedded font payloads and signature bits are not used yet */
		rqst->handled = true;
	}
	else if (nameMatches(rqst->pName, NS_W_KEY, "fonts")) {
		rqst->handled = true;
	}
}

void OXMLi_ListenerState_FontTable::endElement (OXMLi_EndElementRequest * rqst)
{
	if (nameMatches(rqst->pName, NS_W_KEY, "font")) {
		if (!m_curName.empty())
		{
			OXML_Document * doc = OXML_Document::getInstance();
			if (doc)
			{
				OXML_SharedFontManager fmgr = doc->getFontManager();
				if (fmgr.get())
					fmgr->addFontTableEntry(m_curName, m_curEntry);
			}
			m_curName.clear();
			m_curEntry = OXML_FontTableEntry();
		}
		rqst->handled = true;
	}
	else if (nameMatches(rqst->pName, NS_W_KEY, "fonts") ||
			 nameMatches(rqst->pName, NS_W_KEY, "altName") ||
			 nameMatches(rqst->pName, NS_W_KEY, "panose1") ||
			 nameMatches(rqst->pName, NS_W_KEY, "charset") ||
			 nameMatches(rqst->pName, NS_W_KEY, "family") ||
			 nameMatches(rqst->pName, NS_W_KEY, "pitch") ||
			 nameMatches(rqst->pName, NS_W_KEY, "sig") ||
			 nameMatches(rqst->pName, NS_W_KEY, "notTrueType") ||
			 nameMatches(rqst->pName, NS_W_KEY, "embedRegular") ||
			 nameMatches(rqst->pName, NS_W_KEY, "embedBold") ||
			 nameMatches(rqst->pName, NS_W_KEY, "embedItalic") ||
			 nameMatches(rqst->pName, NS_W_KEY, "embedBoldItalic")) {
		rqst->handled = true;
	}
}

void OXMLi_ListenerState_FontTable::charData (OXMLi_CharDataRequest * /*rqst*/)
{
	//don't do anything here
}
