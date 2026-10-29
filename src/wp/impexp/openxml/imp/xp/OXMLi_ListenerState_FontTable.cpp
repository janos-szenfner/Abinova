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
#include "OXMLi_Namespace_Common.h"
#include "OXMLi_PackageManager.h"

// Abinova includes
#include "gr_UnixCairoGraphics.h"
#include "ut_assert.h"
#include "ut_bytebuf.h"
#include "ut_debugmsg.h"
#include "ut_misc.h"

// External includes
#include <string>

/* cap on a single embedded-font part — the bytes are handed to
 * FreeType, and no real embedded font approaches 64MiB */
#define OXMLI_MAX_EMBEDDED_FONT_SIZE (64u * 1024u * 1024u)

/* ECMA-376 Part 4 §2.8.1 "Font Embedding": the obfuscated part's first
 * 32 bytes are XORed with the w:fontKey GUID's 16 bytes in REVERSED
 * order (applied twice).  Pure XOR, so the same pass de-obfuscates.
 * Returns false when the key is not a 16-byte GUID. */
static bool s_deobfuscateFont(UT_ByteBuf & buf,
							  const std::string & fontKey)
{
	std::string hex;
	for (char c : fontKey)
		if (g_ascii_isxdigit(c))
			hex += c;
	if (hex.size() != 32 || buf.getLength() < 32)
		return false;
	UT_Byte key[16];
	for (int i = 0; i < 16; i++)
	{
		char pair[3] = { hex[2 * i], hex[2 * i + 1], '\0' };
		key[15 - i] = static_cast<UT_Byte>(strtoul(pair, nullptr, 16));
	}
	UT_Byte head[32];
	memcpy(head, buf.getPointer(0), 32);
	for (int i = 0; i < 32; i++)
		head[i] ^= key[i % 16];
	return buf.overwrite(0, head, 32);
}

/* resolve each w:embed* reference declared on the font, de-obfuscate
 * the payload and register it with fontconfig so the family is usable
 * for layout and export; the font manager is told the family now
 * resolves so it stops substituting */
static void s_registerEmbeddedFonts(const std::string & declaredName,
									const OXML_FontTableEntry & entry,
									std::set<std::string> & seenRelIds,
									OXML_FontManager * fmgr)
{
	if (entry.embedded.empty())
		return;
	OXMLi_PackageManager * mgr = OXMLi_PackageManager::getInstance();
	if (!mgr)
		return;
	for (auto & ref : entry.embedded)
	{
		if (ref.relId.empty() || !seenRelIds.insert(ref.relId).second)
			continue;
		GsfInput * in = mgr->openFontTableRelById(ref.relId.c_str());
		if (!in)
			continue;
		UT_ByteBuf buf;
		gsf_off_t len = gsf_input_size(in);
		if (len > 0 && len <= OXMLI_MAX_EMBEDDED_FONT_SIZE)
			buf.insertFromInput(0, in);
		g_object_unref(in);
		if (buf.getLength() == 0 ||
			!s_deobfuscateFont(buf, ref.fontKey))
			continue;
		std::string family = GR_registerEmbeddedFont(
			buf.getPointer(0), buf.getLength(), declaredName);
		if (!family.empty())
		{
			fmgr->noteEmbeddedFont(declaredName, family);
			UT_DEBUGMSG(("fontTable: embedded font '%s' registered "
						 "as family '%s'\n",
						 declaredName.c_str(), family.c_str()));
		}
		else
		{
			UT_DEBUGMSG(("fontTable: embedded font for '%s' rejected; "
						 "substituting\n", declaredName.c_str()));
		}
	}
}

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
		else if (nameMatches(rqst->pName, NS_W_KEY, "embedRegular") ||
				 nameMatches(rqst->pName, NS_W_KEY, "embedBold") ||
				 nameMatches(rqst->pName, NS_W_KEY, "embedItalic") ||
				 nameMatches(rqst->pName, NS_W_KEY, "embedBoldItalic")) {
			/* r:id -> odttf font part, w:fontKey -> XOR obfuscation
			 * GUID; the payload is resolved and registered when the
			 * enclosing w:font element closes */
			OXML_EmbeddedFontRef ref;
			const gchar * id =
				attrMatches(NS_R_KEY, "id", rqst->ppAtts);
			const gchar * key =
				attrMatches(NS_W_KEY, "fontKey", rqst->ppAtts);
			if (id)
				ref.relId = id;
			if (key)
				ref.fontKey = key;
			if (!ref.relId.empty())
				m_curEntry.embedded.push_back(ref);
		}
		/* w:sig and w:notTrueType are valid children too;
		 * signature bits are not used */
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
				{
					fmgr->addFontTableEntry(m_curName, m_curEntry);
					s_registerEmbeddedFonts(m_curName, m_curEntry,
											m_seenRelIds, fmgr.get());
				}
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
