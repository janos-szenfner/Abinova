/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource
 *
 * Copyright (C) 2026 Abinova contributors
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

#ifndef _OXMLI_ELEMENT_ALTCHUNK_H_
#define _OXMLI_ELEMENT_ALTCHUNK_H_

// Internal includes
#include "OXML_Element_Paragraph.h"

// External includes
#include <string>

/* \class OXMLi_Element_AltChunk
 * \brief OOXML <w:altChunk r:id="..."> element — a block-level
 * subdocument reference (ECMA-376 §17.17.2.1).
 *
 * Structurally it behaves like the placeholder paragraph it replaces:
 * it sits in the section's child list as a P_TAG block carrying the
 * altchunk-path/altchunk-format props for .abwn round-tripping.  At
 * addToPT time it emits that block, then resolves the relationship to
 * the chunk part, runs the matching internal importer on it into a
 * scratch document, and splices the result into the piece table at the
 * insertion point (the paragraph's own position).  When the chunk
 * cannot be opened or imported the empty placeholder paragraph is what
 * remains - the same output the importer produced before.
 */
class OXMLi_Element_AltChunk : public OXML_Element_Paragraph
{
public:
	OXMLi_Element_AltChunk();
	virtual ~OXMLi_Element_AltChunk();

	//! The r:id relationship attribute pointing at the chunk part.
	void setRelId(const std::string & id) { m_relId = id; }
	//! The resolved in-package part path (e.g. "word/chunk1.html").
	void setPartPath(const std::string & path) { m_partPath = path; }

	virtual UT_Error addToPT(PD_Document * pDocument) override;

private:
	void _graftChunk(PD_Document * pDocument);

	std::string m_relId;
	std::string m_partPath;
};

#endif //_OXMLI_ELEMENT_ALTCHUNK_H_
