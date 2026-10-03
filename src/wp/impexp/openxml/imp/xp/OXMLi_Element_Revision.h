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

#ifndef _OXMLI_ELEMENT_REVISION_H_
#define _OXMLI_ELEMENT_REVISION_H_

// Internal includes
#include "OXML_Element.h"

// External includes
#include <string>

/* \class OXMLi_Element_Revision
 * \brief OOXML run-level tracked-change container (w:ins / w:del /
 * w:moveFrom / w:moveTo — CT_RunTrackChange, ECMA-376 §17.13.5).
 *
 * The element sits on the importer's element stack like a run, so
 * runs/hyperlinks/objects nested inside it become its children through
 * the normal _flushTopLevel plumbing.  At addToPT time it registers a
 * piece-table revision (w:author -> revision description, w:date ->
 * start time) and folds the corresponding "+id"/"-id" token into the
 * revision attribute of every descendant that hands an attr/prop set
 * to the piece table (runs, fields, images, bookmarks).
 *
 * w:del content stays in the document as a deletion revision rather
 * than being flattened away, so it renders struck-through while
 * revisions are shown and disappears in the final view; w:moveFrom /
 * w:moveTo degrade to the matching deletion/insertion and keep the
 * move's w:name in a "revision-move" attribute so a later exporter can
 * re-pair them.
 *
 * Nested scopes are registered in two passes (insertions first, then
 * deletions) so that a deletion wrapping or wrapped by an insertion
 * always owns the highest revision id — i.e. deleted content stays
 * deleted no matter how the changes were nested (w:del over a prior
 * w:ins is the common Word pattern).
 */
class OXMLi_Element_Revision : public OXML_Element
{
public:
	OXMLi_Element_Revision(bool deleted);
	virtual ~OXMLi_Element_Revision();

	//! w:author of the tracked change
	void setAuthor(const std::string & author) { m_author = author; }
	//! w:date of the tracked change (xsd:dateTime)
	void setDate(const std::string & date) { m_date = date; }
	//! w:name linking a moveFrom/moveTo pair
	void setMoveName(const std::string & name) { m_moveName = name; }

	virtual UT_Error addToPT(PD_Document * pDocument) override;

private:
	void _registerScope(PD_Document * pDocument);
	void _registerTree(OXML_Element * elem, PD_Document * pDocument, bool deleted);
	void _register(PD_Document * pDocument);
	void _markDescendants(OXML_Element * elem, const std::string & token);

	bool m_deleted;
	bool m_registered;
	UT_uint32 m_id;
	std::string m_author;
	std::string m_date;
	std::string m_moveName;
};

#endif //_OXMLI_ELEMENT_REVISION_H_
