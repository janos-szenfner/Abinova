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

// Class definition include
#include "OXMLi_Element_Revision.h"

// Abinova includes
#include "ut_types.h"
#include "ut_string.h"
#include "ut_string_class.h"
#include "ut_std_string.h"
#include "pd_Document.h"

// External includes
#include <ctime>
#include <cstring>

OXMLi_Element_Revision::OXMLi_Element_Revision(bool deleted) :
	OXML_Element("", REV_TAG, SPAN),
	m_deleted(deleted),
	m_registered(false),
	m_id(0)
{
}

OXMLi_Element_Revision::~OXMLi_Element_Revision()
{
}

/* w:date is xsd:dateTime, e.g. "2026-10-03T04:30:00Z" or with a
 * "+02:00" offset; UT_strptime parses the leading fixed part and the
 * zone tail is ignored (timestamps are informational only). */
static time_t _parseRevisionDate(const std::string & date)
{
	struct tm tm;
	memset(&tm, 0, sizeof(tm));
	tm.tm_isdst = 0;
	if (!UT_strptime(date.c_str(), "%Y-%m-%dT%H:%M:%S", &tm))
		return 0;
	return mktime(&tm);
}

UT_Error OXMLi_Element_Revision::addToPT(PD_Document * pDocument)
{
	UT_return_val_if_fail(pDocument != nullptr, UT_ERROR);

	if (!m_registered)
		_registerScope(pDocument);

	std::string token = m_deleted ? "-" : "+";
	token += UT_std_string_sprintf("%d", m_id);

	_markDescendants(this, token);

	/* the runs/fields/images below were just given the revision
	 * attribute and emit it through their own fmt; the ambient fmt
	 * here additionally covers children that carry no attrs of their
	 * own (e.g. text outside a run) */
	const PP_PropertyVector revAtts = {"revision", token.c_str()};
	pDocument->appendFmt(revAtts);
	UT_Error ret = addChildrenToPT(pDocument);
	pDocument->appendFmt(PP_NOPROPS);

	return ret;
}

/* Two tracked-change elements describe the same logical change when
 * they share direction, author, date and move name — Word emits a run
 * of such siblings for a single editing burst (w:id is per-element,
 * so it cannot group them). */
bool OXMLi_Element_Revision::_sameChange(const OXMLi_Element_Revision & other) const
{
	return m_deleted == other.m_deleted &&
		m_author == other.m_author &&
		m_date == other.m_date &&
		m_moveName == other.m_moveName;
}

/* Fresh ids are allocated densely here rather than trusting w:id
 * (which can be any integer) because explodeRevisions() walks
 * revision ids 1..max on every revised fragment. */
UT_uint32 OXMLi_Element_Revision::registerRevision(PD_Document * pDocument,
												   const std::string & author,
												   const std::string & date)
{
	UT_uint32 id = pDocument->getHighestRevisionId() + 1;

	/* w:author is a display name, not a comment — store it in the
	 * revision's author field (OOXML ins/del carry no description) */
	pDocument->addRevision(id, nullptr, _parseRevisionDate(date), 0, false,
						   author.empty() ? nullptr : author.c_str());
	return id;
}

/* One document revision per logical change: when the immediately
 * preceding sibling element is an already-registered tracked change
 * with the same signature, this element reuses its revision id so the
 * burst collapses into a single AD_Revision record (the same grouping
 * ie_imp_RTF gets by trusting the file's own revision ids).  Sibling
 * order in the tree is document order, and addToPT walks it in order,
 * so a registered previous sibling is fully registered by now. */
void OXMLi_Element_Revision::_register(PD_Document * pDocument)
{
	OXML_Element * prev = getPrevSibling();
	if (prev && prev->getTag() == REV_TAG)
	{
		OXMLi_Element_Revision * prevRev =
			static_cast<OXMLi_Element_Revision*>(prev);
		if (prevRev->m_registered && _sameChange(*prevRev))
		{
			m_id = prevRev->m_id;
			m_registered = true;
			return;
		}
	}

	m_id = registerRevision(pDocument, m_author, m_date);
	m_registered = true;
}

/* Additions are registered before deletions throughout this element's
 * subtree, so a deletion ends up with the highest revision id inside
 * the scope and dominates — deleted content stays deleted whether the
 * file nested w:del around w:ins or w:ins around w:del. */
void OXMLi_Element_Revision::_registerScope(PD_Document * pDocument)
{
	_registerTree(this, pDocument, false);
	_registerTree(this, pDocument, true);
}

void OXMLi_Element_Revision::_registerTree(OXML_Element * elem,
										   PD_Document * pDocument,
										   bool deleted)
{
	OXMLi_Element_Revision * rev =
		(elem->getTag() == REV_TAG) ?
		static_cast<OXMLi_Element_Revision*>(elem) : nullptr;
	if (rev && !rev->m_registered && rev->m_deleted == deleted)
		rev->_register(pDocument);

	const OXML_ElementVector & children = elem->getChildren();
	OXML_ElementVector::const_iterator it;
	for (it = children.begin(); it != children.end(); ++it)
	{
		OXML_Element * child = it->get();
		if (child)
			_registerTree(child, pDocument, deleted);
	}
}

/* Fold the revision token into the "revision" attribute of every
 * descendant element.  Run elements carry it into the piece table via
 * appendFmt; field/image/bookmark elements pass their attribute list
 * to appendObject.  Elements that never emit attributes (plain text
 * children) inherit the run's fmt anyway, so the mark is inert on
 * them.  A descendant that already carries a revision (e.g. a nested
 * w:del inside w:ins) keeps it and gains this one, so later edits
 * stack correctly. */
void OXMLi_Element_Revision::_markDescendants(OXML_Element * elem, const std::string & token)
{
	const OXML_ElementVector & children = elem->getChildren();
	OXML_ElementVector::const_iterator it;
	for (it = children.begin(); it != children.end(); ++it)
	{
		OXML_Element * child = it->get();
		if (!child)
			continue;

		const gchar * existing = nullptr;
		std::string val = token;
		if (child->getAttribute("revision", existing) == UT_OK &&
			existing && *existing)
		{
			val = std::string(existing) + "," + token;
		}
		child->setAttribute("revision", val.c_str());
		if (!m_moveName.empty())
			child->setAttribute("revision-move", m_moveName.c_str());
		if (!m_moveId.empty())
			child->setAttribute("revision-move-id", m_moveId.c_str());

		_markDescendants(child, token);
	}
}
