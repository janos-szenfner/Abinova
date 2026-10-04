/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
/* Abinova
 * Copyright (C) 2002 Tomas Frydrych <tomas@frydrych.uklinux.net>
 * Copyright (C) 2021-2025 Hubert Figuière
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

#include <sstream>
#include <string_view>

#include "pp_Revision.h"
#include "pp_AttrProp.h"
#include "pd_Style.h"
#include "pd_Document.h"
#include "ut_debugmsg.h"
#include "ut_misc.h"
#include "ut_std_map.h"

namespace {

// bounded strtok-equivalent over a string_view: skips leading
// delimiter characters (consecutive delimiters collapse, exactly like
// strtok), returns the next token, and advances `rem` past the
// delimiter that terminated it. An empty return means no token
// remains — identical to strtok returning nullptr.
std::string_view s_nextToken(std::string_view & rem, char delim)
{
	const size_t start = rem.find_first_not_of(delim);
	if (start == std::string_view::npos)
	{
		rem = {};
		return {};
	}
	rem.remove_prefix(start);
	const size_t end = rem.find(delim);
	const std::string_view tok = rem.substr(0, end);
	rem = (end == std::string_view::npos) ? std::string_view{} : rem.substr(end + 1);
	return tok;
}

} // anonymous namespace

PP_Revision::PP_Revision(UT_uint32 Id, PP_RevisionType eType, const gchar * props, const gchar * attrs):
	m_iID(Id), m_eType(eType), m_bDirty(true)
{
	if (props)
		_parsePairs(props, true);
	if (attrs)
		_parsePairs(attrs, false);
}

/*! parses the "name:value;name:value" pair strings carried inside a
    revision token. Semantics match the historical strtok loop: name
    terminates at the first ':' (so ';' may appear in a name), value
    terminates at the next ';' (so ':' may appear in a value), and an
    absent or "-/-" value means the pair is present but empty.
*/
void PP_Revision::_parsePairs(std::string_view s, bool isProps)
{
	std::string_view rem = s;
	std::string_view n = s_nextToken(rem, ':');

	while (!n.empty())
	{
		// skip over spaces ...
		while (!n.empty() && n.front() == ' ')
			n.remove_prefix(1);

		std::string_view v = s_nextToken(rem, ';');

		// no value means the property is being removed ...
		if (v == "-/-")
			v = {};

		const std::string name(n);
		const std::string value(v);
		if (isProps)
			setProperty(name, value);
		else
			setAttribute(name.c_str(), value.c_str());

		n = s_nextToken(rem, ':');
	}
}

PP_Revision::PP_Revision(UT_uint32 Id, PP_RevisionType eType,
                         const PP_PropertyVector & props,
                         const PP_PropertyVector & attrs)
	: m_iID(Id)
	, m_eType(eType)
	, m_bDirty(true)
{
	setProperties(props);
	setAttributes(attrs);
}

/*!
    Sets attributes taking care of any nested revision attribute (which needs to be parsed
    and combined with the current AP set.
*/
bool PP_Revision::setAttributes(const PP_PropertyVector & attributes)
{
	if(!PP_AttrProp::setAttributes(attributes)) {
		return false;
	}

	return _handleNestedRevAttr();
}



bool PP_Revision::_handleNestedRevAttr()
{
	const gchar * pNestedRev = nullptr;
	getAttribute("revision", pNestedRev);
	
	if(pNestedRev)
	{
		PP_RevisionAttr NestedAttr(pNestedRev);

		// now remove "revision"
		setAttribute("revision", nullptr);
		prune();

		// overlay the attrs and props from the revision attribute
		for(UT_uint32 i = 0; i < NestedAttr.getRevisionsCount(); ++i)
		{
			const PP_Revision * pRev = NestedAttr.getNthRevision(i);
			UT_return_val_if_fail( pRev, false );

			// ignore inserts and deletes
			if(pRev->getType() == PP_REVISION_ADDITION || pRev->getType() == PP_REVISION_DELETION)
				continue;

			setProperties(pRev->getProperties());
			setAttributes(pRev->getAttributes());
		}

		prune();
	}

	return true;
}


/*! converts the internal vector of properties into XML string */
const gchar * PP_Revision::getPropsString() const
{
	if(m_bDirty)
		_refreshString();

	return static_cast<const gchar*>( m_sXMLProps.c_str());
}

/*! converts the internal vector of attributes into XML string */
const gchar * PP_Revision::getAttrsString() const
{
	if(m_bDirty)
		_refreshString();

	return static_cast<const gchar*>( m_sXMLAttrs.c_str());
}

void PP_Revision::_refreshString() const
{
	m_sXMLProps.clear();
	m_sXMLAttrs.clear();

	UT_uint32 i;
	UT_uint32 iCount = getPropertyCount();
	const gchar * n, *v;

	for(i = 0; i < iCount; i++)
	{
		if(!getNthProperty(i,n,v))
		{
			// UT_ASSERT_HARMLESS( UT_SHOULD_NOT_HAPPEN );
			continue;
		}
		
		if(!v || !*v) v = "-/-";
		
		m_sXMLProps += n;
		m_sXMLProps += ":";
		m_sXMLProps += v;
		if(i < iCount - 1)
			m_sXMLProps += ";";
	}

	iCount = getAttributeCount();
	for(i = 0; i < iCount; i++)
	{
		if(!getNthAttribute(i,n,v))
		{
			// UT_ASSERT_HARMLESS( UT_SHOULD_NOT_HAPPEN );
			continue;
		}
		
		if(!v || !*v) v = "-/-";

		m_sXMLAttrs += n;
		m_sXMLAttrs += ":";
		m_sXMLAttrs += v;
		if(i < iCount - 1)
			m_sXMLAttrs += ";";
	}

	m_bDirty = false;
}

std::string PP_Revision::toString() const
{
    std::stringstream ret;
    PP_RevisionType r_type = getType();

    if(r_type == PP_REVISION_FMT_CHANGE)
        ret << "!";

    // print the id with appropriate sign
    ret << (r_type == PP_REVISION_DELETION ? -static_cast<int>(getId()) : static_cast<int>(getId()));
    
    if(r_type != PP_REVISION_DELETION)
    {
        // if we have no props but have attribs, we have to issue empty braces so as not to
        // confuse attribs with props
        if(hasProperties() || hasAttributes())
            ret << "{";
        
        if(hasProperties())
            ret << getPropsString();
        
        if(hasProperties() || hasAttributes())
            ret << "}";
			
        if(hasAttributes())
        {
            ret << "{" << getAttrsString() << "}";
        }
    }
    
    return ret.str();
}

bool PP_Revision::onlyContainsAbiwordChangeTrackingMarkup() const
{
    UT_DEBUGMSG(("onlyContainsAbiwordChangeTrackingMarkup(top) ac:%ld pc:%ld\n",
		 static_cast<long>(getAttributeCount()), static_cast<long>(getPropertyCount() )));

    if( !getAttributeCount() )
        return false;
    if( getPropertyCount() )
        return false;
    
    bool ret = true;
	UT_uint32 i;
	UT_uint32 iCount = getAttributeCount();
	const gchar * n, *v;

	for(i = 0; i < iCount; i++)
	{
		if(!getNthAttribute(i,n,v))
		{
			// UT_ASSERT_HARMLESS( UT_SHOULD_NOT_HAPPEN );
			continue;
		}
        UT_DEBUGMSG(("onlyContainsAbiwordChangeTrackingMarkup() n:%s\n", n ));
        
        if( n != strstr( n, "abi-para" ) )
        {
            return false;
        }
    }
    
    return ret;
}


bool PP_Revision::operator == (const PP_Revision &op2) const
{
	// this is quite involved, but we will start with the simple
	// non-equality cases

	if(getId() != op2.getId())
		return false;

	if(getType() != op2.getType())
		return false;


	// OK, so we have the same type and id, do we have the same props ???
	UT_uint32 iPCount1 = getPropertyCount();
	UT_uint32 iPCount2 = op2.getPropertyCount();
	UT_uint32 iACount1 = getAttributeCount();
	UT_uint32 iACount2 = op2.getAttributeCount();

	if((iPCount1 != iPCount2) || (iACount1 != iACount2))
		return false;

	// now the lengthy comparison
	UT_uint32 i;
	const gchar * n;
	const gchar * v1, * v2;

	for(i = 0; i < iPCount1; i++)
	{

		getNthProperty(i,n,v1);
		op2.getProperty(n,v2);

		if(strcmp(v1,v2))
			return false;
	}

	for(i = 0; i < iACount1; i++)
	{

		getNthAttribute(i,n,v1);
		op2.getAttribute(n,v2);

		if(strcmp(v1,v2))
			return false;
	}
	return true;
}


// PP_Revision*
// PP_Revision::clone() const
// {
//     PP_Revision* ret = new PP_Revision( *this );
//     return ret;
// }


/************************************************************
 ************************************************************/

/*! create class instance from an XML attribute string
 */
PP_RevisionAttr::PP_RevisionAttr(const gchar * r)
{
	_init(r);
}

/*! create class instance from a single revision data */
PP_RevisionAttr::PP_RevisionAttr(UT_uint32 iId, PP_RevisionType eType,
                                 const PP_PropertyVector & attrs,
                                 const PP_PropertyVector & props)
{
	m_vRev.push_back(std::make_unique<PP_Revision>(static_cast<UT_uint32>(iId), eType, props, attrs));
}


PP_RevisionAttr::~PP_RevisionAttr() = default;

/*! initialize instance with XML attribute string
 */
void PP_RevisionAttr::setRevision(const gchar * r)
{
	m_vRev.clear();
	_init(r);
}

void
PP_RevisionAttr::setRevision(const std::string&  r)
{
    setRevision( r.c_str() );
}


/*! marks all derived state stale; every mutator must call this so the
    XML string cache and the last-revision index cache are recomputed
    on next use — a missed call returns stale data, not garbage.
*/
void PP_RevisionAttr::_markDirty()
{
	m_bDirty = true;
	m_bLastRevisionDirty = true;
}


/*! parse given XML attribute string and fill the
    instance with the data
*/
void PP_RevisionAttr::_init(const gchar *r)
{
	if(!r)
		return;

	// the string we are parsing looks like
	// "+1,-2,!3{font-family: Times New Roman}"
	//
	// tokens are comma separated; each is:
	//   [+]n[{props}[{attrs}]]   addition (optionally with fmt payload)
	//   -n                      deletion (never carries a payload)
	//   !n{props}[{attrs}]      format change
	// malformed tokens are skipped, never folded into id 0.

	std::string_view rem(r);
	std::string_view t = s_nextToken(rem, ',');

	while(!t.empty())
	{
		PP_RevisionType eType;

		if(t.front() == '!')
		{
			eType = PP_REVISION_FMT_CHANGE;
			t.remove_prefix(1);
		}
		else if(t.front() == '-')
		{
			eType = PP_REVISION_DELETION;
			t.remove_prefix(1);
		}
		else if(t.front() == '+')
		{
			eType = PP_REVISION_ADDITION;
			t.remove_prefix(1);
		}
		else
			eType = PP_REVISION_ADDITION;

		const size_t op_brace = t.find('{');
		const size_t cl_brace = t.find('}');
		const bool   has_braces = (op_brace != std::string_view::npos)
		                       && (cl_brace != std::string_view::npos)
		                       && (cl_brace > op_brace);

		std::string_view props;
		std::string_view attrs;
		std::string_view id_text;

		if(!has_braces)
		{
			// bare id token; a lone or misordered brace makes the
			// whole token malformed
			if(eType == PP_REVISION_FMT_CHANGE)
			{
				// malformed token, move onto the next one
				UT_DEBUGMSG(("PP_RevisionAttr::_init: invalid ! token [%.*s]\n", (int)t.size(), t.data()));
				t = s_nextToken(rem, ',');
				continue;
			}
			if(op_brace != std::string_view::npos || cl_brace != std::string_view::npos)
			{
				UT_DEBUGMSG(("PP_RevisionAttr::_init: malformed braces in token [%.*s]\n", (int)t.size(), t.data()));
				t = s_nextToken(rem, ',');
				continue;
			}
			id_text = t;
		}
		else
		{
			// props payload present — this must be a fmt change or
			// an addition; a deletion never carries props
			if(eType == PP_REVISION_DELETION)
			{
				// malformed token, move onto the next one
				UT_DEBUGMSG(("PP_RevisionAttr::_init: invalid - token [%.*s]\n", (int)t.size(), t.data()));
				t = s_nextToken(rem, ',');
				continue;
			}

			id_text = t.substr(0, op_brace);
			props   = t.substr(op_brace + 1, cl_brace - op_brace - 1);

			// props may be followed by exactly one {attrs} group that
			// must consume the rest of the token
			const std::string_view rest = t.substr(cl_brace + 1);
			if(!rest.empty())
			{
				if(rest.front() == '{')
				{
					const size_t cl2 = rest.find('}');
					if(cl2 != std::string_view::npos && cl2 == rest.size() - 1)
						attrs = rest.substr(1, cl2 - 1);
				}
				if(attrs.empty() && !(rest.size() == 2 && rest == "{}"))
				{
					UT_DEBUGMSG(( "PP_RevisionAttr::_init: malformed attrs group in token [%.*s]\n", (int)t.size(), t.data() ));
					t = s_nextToken(rem, ',');
					continue;
				}
			}

			if(eType == PP_REVISION_ADDITION)
				eType = PP_REVISION_ADDITION_AND_FMT;
		}

		// the id must be a plain decimal number — garbage ids used to
		// collapse to 0 via atol and silently merge revisions
		UT_uint32 iId = 0;
		bool      ok  = !id_text.empty();
		for(const char c : id_text)
		{
			const UT_uint32 d = static_cast<UT_uint32>(c - '0');
			if(c < '0' || c > '9' || iId > (0xFFFFFFFFU - d) / 10U)
			{
				ok = false;
				break;
			}
			iId = iId * 10U + d;
		}
		if(!ok)
		{
			UT_DEBUGMSG(("PP_RevisionAttr::_init: malformed revision id [%.*s]\n", (int)id_text.size(), id_text.data()));
			t = s_nextToken(rem, ',');
			continue;
		}

		const std::string props_s(props);
		const std::string attrs_s(attrs);
		m_vRev.push_back(std::make_unique<PP_Revision>(iId, eType,
		                                             has_braces ? props_s.c_str() : nullptr,
		                                             has_braces ? attrs_s.c_str() : nullptr));

		t = s_nextToken(rem, ',');
	}

	m_bDirty = true;
	m_iSuperfluous = 0;
	m_bLastRevisionDirty = true;
}

/*!
    changes the type of revision with id iId to eType; if revision
    with that id is not present, returns false
 */
bool PP_RevisionAttr::changeRevisionType(UT_uint32 iId, PP_RevisionType eType)
{
	for (size_t i = 0; i < m_vRev.size(); i++) {
		PP_Revision* r = m_vRev.at(i).get();
		UT_nonnull_or_continue(r);
		if (iId == r->getId()) {
			r->setType(eType);
			_markDirty();
			return true;
		}
	}

	return false;
}

bool PP_RevisionAttr::changeRevisionId(UT_uint32 iOldId, UT_uint32 iNewId)
{
	UT_return_val_if_fail(iNewId >= iOldId, false);

	for (size_t i = 0; i < m_vRev.size(); i++) {
		PP_Revision* r = m_vRev.at(i).get();
		UT_nonnull_or_continue(r);
		if (iOldId == r->getId()) {
			r->setId(iNewId);
			_markDirty();
			return true;
		}
	}

	return false;
}

/*!
    this function removes any revisions that no-longer contribute to the cumulative effect
    it is used in full-history mode when transfering attrs and props from the revision attribute
    into the main attrs and props
*/
void PP_RevisionAttr::pruneForCumulativeResult(PD_Document * pDoc)
{
	// first we are looking for any deletions which cancel anything below them
	bool bDelete = false;
	if (m_vRev.empty()) {
		return;
	}

	_markDirty();

	for (size_t i = m_vRev.size() - 1; i != 0; --i) {
		const PP_Revision* r = m_vRev.at(i).get();
		UT_nonnull_or_continue(r);
		if (!bDelete && r->getType() == PP_REVISION_DELETION) {
			bDelete = true;
			continue; // we do not want the top revision deleted
		}

		if (bDelete) {
			m_vRev.erase(m_vRev.begin() + i);
		}
	}

	// now we merge props and attrs in what is left
	if (m_vRev.empty()) {
		return;
	}

	PP_Revision* r0 = m_vRev.at(0).get();
	UT_nonnull_or_return(r0, );

	for (size_t i = 1; i < m_vRev.size(); ++i) {
		const PP_Revision* r = m_vRev.at(i).get();
		UT_nonnull_or_return(r, );
		r0->setProperties(r->getProperties());
		r0->setAttributes(r->getAttributes());

		m_vRev.erase(m_vRev.begin() + i);
		--i;
	}

    // explode the style if present
	if(pDoc)
		r0->explodeStyle(pDoc);

#if 0
	// I do not think we should do this -- the emptiness indicates that the props and
	// attributes should be removed from the format; if we remove them, we irreversibly
	// lose this information

	// get rid of any empty props and attrs
	r0->prune();
#endif

	// finally, remove the revision attribute if present
	const gchar * v;
	if(r0->getAttribute("revision", v))
		r0->setAttribute("revision", nullptr);

	UT_ASSERT_HARMLESS( m_vRev.size() == 1 );
}


/*! return highest revision associated with this revision attribute
    that has ID at most equal to id; the returned PP_Revision can be
    used to determine whether this text should be displayed or hidden
    in revision with the original id

    \param UT_uint32 id : the id of this revision
    \param PP_Revision ** ppR: location where to store pointer to one
                               of the special revisions in case return
                               value is nullptr
                              
    \return : pointer to PP_Revision, or nullptr if the revision
    attribute should be ignored for the present level
*/

// these are special instances of PP_Revision that are used to in the
// following function to handle special cases
static const PP_Revision s_del(0, PP_REVISION_DELETION, nullptr, nullptr);
static const PP_Revision s_add(0, PP_REVISION_ADDITION, nullptr, nullptr);

const PP_Revision*  PP_RevisionAttr::getGreatestLesserOrEqualRevision(UT_uint32 id,
																	   const PP_Revision ** ppR) const
{
	if(ppR)
		*ppR = nullptr;
	
	if(id == 0)
		return getLastRevision();

	const PP_Revision *r = nullptr; // this will be the revision we are looking for
	UT_uint32 r_id = 0;

	const PP_Revision *m = nullptr; // this will be the lowest revision present
	UT_uint32 m_id = 0xFFFF;

	for (size_t i = 0; i < m_vRev.size(); i++) {
		const PP_Revision* t = m_vRev.at(i).get();
		UT_uint32 t_id = t->getId();

		// the special case speedup - if we hit our id, then we can return immediately
		if(t_id == id)
			return t;

		if(t_id < m_id)
		{
			m = t;
			m_id = t_id;
		}

		if((t_id < id) && (t_id > r_id))
		{
			r = t;
			r_id = t_id;
		}
	}

	// now that we have the biggest revision with ID lesser or equal
	// id, we have to deal with the special case when this is nullptr
	// i.e., this fragment only figures in revisions > id; the problem
	// with nullptr is that it is visible if the smallest revision ID is
	// negative, and hidden in the opposite case -- we use the special
	// static variables s_del and s_add to indicate what should
	// be done

	if(r == nullptr && ppR)
	{
		if(!m)
		{
			//UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
			// this happens when there was no revision attribute
			return nullptr;
		}

		if(m->getType() == PP_REVISION_DELETION)
			*ppR = &s_del;
		else if((m->getType() == PP_REVISION_ADDITION)
				||(m->getType() == PP_REVISION_ADDITION_AND_FMT))
			*ppR = &s_add;
		else // the initial revision was fmt change, so ignore it
			*ppR = nullptr;
	}

	return r;
}

const PP_Revision* PP_RevisionAttr::getLowestGreaterOrEqualRevision(UT_uint32 id) const
{
	if(id == 0)
		return nullptr;

	const PP_Revision *r = nullptr; // this will be the revision we are looking for
	UT_uint32 r_id = PD_MAX_REVISION;

	for(size_t i = 0; i < m_vRev.size(); i++)
	{
		const PP_Revision * t = m_vRev.at(i).get();
		UT_nonnull_or_continue(t);
		UT_uint32 t_id = t->getId();

		// the special case speedup - if we hit our id, then we can return immediately
		if(t_id == id)
			return t;

		if((t_id > id) && (t_id < r_id))
		{
			r = t;
			r_id = t_id;
		}
	}

	return r;
}


/*! finds the highest revision number in this attribute
 */
const PP_Revision* PP_RevisionAttr::getLastRevision() const
{
	// cache the index of the highest-id revision; invalidated
	// centrally by _markDirty so a mutation can't leave a stale or
	// dangling entry behind
	if(!m_bLastRevisionDirty)
		return m_iLastRevision >= 0 ? m_vRev.at(m_iLastRevision).get() : nullptr;

	m_iLastRevision = -1;
	UT_uint32 r_id = 0;

	for (size_t i = 0; i < m_vRev.size(); i++)
	{
		const PP_Revision* t = m_vRev.at(i).get();
		UT_nonnull_or_continue(t);
		UT_uint32 t_id = t->getId();

		if(t_id > r_id)
		{
			r_id = t_id;
			m_iLastRevision = static_cast<int>(i);
		}
	}

	m_bLastRevisionDirty = false;
	// it is legal for this to be nullptr -- it happens when the revision was pruned for
	// cumulative effect and the last revision was a deletion.
	return m_iLastRevision >= 0 ? m_vRev.at(m_iLastRevision).get() : nullptr;
}


UT_uint32 PP_RevisionAttr::getHighestId() const
{
    UT_uint32 ret = 0;
	for (size_t i = 0; i < m_vRev.size(); i++)
	{
		const PP_Revision * t = m_vRev.at(i).get();
		UT_nonnull_or_continue(t);
        ret = std::max( ret, t->getId() );
    }
    return ret;
}


/*!
   find revision with id == iId; if revision is not found minId
   contains the smallest id in this set greater than iId; if return value is and minId
   is PD_MAX_REVISION then there are revisions preset
*/
const PP_Revision * PP_RevisionAttr::getRevisionWithId(UT_uint32 iId, UT_uint32 &minId) const
{
	minId = PD_MAX_REVISION;

	for (size_t i = 0; i < m_vRev.size(); i++)
	{
		const PP_Revision * t = m_vRev.at(i).get();
		UT_nonnull_or_continue(t);
		UT_uint32 t_id = t->getId();

		if(t_id == iId)
		{
			return t;
		}

		if(minId > t_id && t_id > iId)
			minId = t_id;
	}

	return nullptr;
}


/*! given revision level id, this function returns true if given
    segment of text is to be visible, false if it is to be hidden
*/
bool PP_RevisionAttr::isVisible(UT_uint32 id) const
{
	if(id == 0)
	{
		// id 0 means show all revisions
		return true;
	}

	const PP_Revision * pSpecial;
	const PP_Revision * pR = getGreatestLesserOrEqualRevision(id, &pSpecial);

	if(pR)
	{
		// found compliant revision ...
		return true;
	}
	

	if(pSpecial)
	{
		// pSpecial is of the same type as the revision with the
		// lowest id
		PP_RevisionType eType = pSpecial->getType();

		// deletions and fmt changes can be ignored; insertions need
		// to be hidden
		return ((eType != PP_REVISION_ADDITION) && (eType == PP_REVISION_ADDITION_AND_FMT));
	}

	// the revision with the lowest id is a change of format, this
	// text has to remain visible
	return true;
}


/*! adds id to the revision vector handling the special cases where id
    is already present in this attribute.
*/
void PP_RevisionAttr::addRevision(UT_uint32 iId, PP_RevisionType eType,
                                  const PP_PropertyVector & pAttrs,
                                  const PP_PropertyVector & pProps)
{
	for (size_t i = 0; i < m_vRev.size(); i++)
	{
		PP_Revision * r = m_vRev.at(i).get();
		UT_nonnull_or_continue(r);
		UT_uint32 r_id = r->getId();
		PP_RevisionType r_type = r->getType();

		if(iId != r_id)
			continue;
		
		if(eType != r_type)
		{
			// we are trying to add a revision id already in the vector
			// but of a different -- this is legal, i.e., the
			// editor just changed his mind
			// we need to make distinction between different cases

			if((eType == PP_REVISION_DELETION) && (   r_type == PP_REVISION_ADDITION
												   || r_type == PP_REVISION_ADDITION_AND_FMT))
			{
				// the editor originally inserted a new segment of
				// text but now wants it out; we cannot just remove
				// the id, because the original operation resulted in
				// a new fragment in the piece table; rather we will
				// mark this with '-' and will remember the superfluous
				// id, so if queried later we can work out if this
				// whole fragment should in fact go

				m_vRev.erase(m_vRev.begin() + i);

				m_iSuperfluous = iId;

				m_vRev.push_back(std::make_unique<PP_Revision>(iId, eType, nullptr, nullptr));
			}
			else if((eType == PP_REVISION_ADDITION) && (r_type == PP_REVISION_DELETION))
			{
				// in the opposite case, when the editor originally
				// marked the text for removal and now wants it back,
				// we just remove the attribute (which we have done)
				// this also happens when we have been left with a
				// superfluous deletion id in the vector; if that is
				// the case we need to reset m_iSuperfluous, since
				// this fragment can no more be superfluous

				m_vRev.erase(m_vRev.begin() + i);

				if(m_iSuperfluous == iId)
				{
					// the editor has had another change of heart
					m_iSuperfluous  = 0;
				}
			}
			else if((eType == PP_REVISION_DELETION) && (r_type == PP_REVISION_FMT_CHANGE))
			{
				// this is the case when the editor changed
				// formatting, but now wants the whole fragment out
				// instead -- we simly replace the old revision with
				// the new, since the original action did not result
				// in inserting new text

				m_vRev.erase(m_vRev.begin() + i);

				m_vRev.push_back(std::make_unique<PP_Revision>(iId, eType, nullptr, nullptr));
			}
			else if((eType == PP_REVISION_FMT_CHANGE) && (r_type == PP_REVISION_DELETION))
			{
				// originally the editor marked the text for deletion,
				// but now he just wants a format change, in this case
				// we just replace the old revision with the new

				m_vRev.erase(m_vRev.begin() + i);

				m_vRev.push_back(std::make_unique<PP_Revision>(iId, eType, pProps, pAttrs));
			}
			else if((eType == PP_REVISION_FMT_CHANGE) && (r_type == PP_REVISION_ADDITION))
			{
				// the editor first added this fragment, and now wants
				// to apply a format change on the top of that
				// so we will keep the old revision record, but need
				// to merge any existing props in the revision with
				// the new ones
				r->setProperties(pProps);
				r->setAttributes(pAttrs);
			}
			else if((eType == PP_REVISION_FMT_CHANGE) && (r_type == PP_REVISION_ADDITION_AND_FMT))
			{
				// addition with a fmt change, just add our changes to it
				r->setProperties(pProps);
				r->setAttributes(pAttrs);
			}

			_markDirty();
			return;
		}
		else //(eType == r_type)
		{
			// we are trying to add a type already in the vector; this is legal but makes sense only
			// if both are fmt changes
			if(!((eType == PP_REVISION_FMT_CHANGE) && (r_type == PP_REVISION_FMT_CHANGE)))
				return;
			
			r->setProperties(pProps);
			r->setAttributes(pAttrs);
			
			_markDirty();
			return;
		}
	}

	// if we got here then the item is not in our vector so add it
	m_vRev.push_back(std::make_unique<PP_Revision>(iId, eType, pProps, pAttrs));
	_markDirty();
}


/**
 * Logically Performs addRevision( iId, eType, 0, 0 ). This method is
 * mainly useful for loading an ODT+GCT file where you want to add and
 * delete revisions but don't actually care about the attrs/props for
 * that action.
 */
void PP_RevisionAttr::addRevision(UT_uint32 iId, PP_RevisionType eType )
{
    addRevision( iId, eType, PP_NOPROPS, PP_NOPROPS );
}


void
PP_RevisionAttr::addRevision( const PP_Revision* r )
{
    UT_return_if_fail(r);

    // copy the revision record directly; routing through the XML
    // string round-trip corrupts deletion ids (unsigned negation)
    // and cannot represent their payloads
    m_vRev.push_back(std::make_unique<PP_Revision>(r->getId(), r->getType(),
                                                 r->getPropsString(),
                                                 r->getAttrsString()));
    m_iSuperfluous = 0;
    _markDirty();
}

void PP_RevisionAttr::mergeAttr( UT_uint32 iId, PP_RevisionType t,
                                 const gchar* pzName, const gchar* pzValue )
{
    PP_RevisionAttr ra;
    const PP_PropertyVector ppAtts = {
        pzName, pzValue
    };
    ra.addRevision(iId, t, ppAtts, PP_NOPROPS);

    mergeAll( ra );
}

/**
 * Do not replace the attribute/value if it exists in the given revision already.
 */
void PP_RevisionAttr::mergeAttrIfNotAlreadyThere( UT_uint32 iId,
                                                  PP_RevisionType t,
                                                  const gchar* pzName,
                                                  const gchar* pzValue )
{
	for (size_t i = 0; i < m_vRev.size(); i++)
	{
		const PP_Revision * tr = m_vRev.at(i).get();
		UT_nonnull_or_continue(tr);
		UT_uint32 tid = tr->getId();

        if( tid == iId )
        {
            if( t == PP_REVISION_NONE || t == tr->getType() )
            {
                const gchar * tattrs = tr->getAttrsString();
                if( strstr( tattrs, pzName ))
                {
                    return;
                }
            }
        }
    }
    
    return mergeAttr( iId, t, pzName, pzValue );
}



//
//                           getId()    getType()                rev
typedef std::map< std::pair< UT_uint32, PP_RevisionType >, const PP_Revision* > revidx_t;

static revidx_t toIndex( const PP_RevisionAttr& ra )
{
    revidx_t ret;
    for( UT_uint32 i=0; i < ra.getRevisionsCount(); ++i )
    {
        const PP_Revision* r = ra.getNthRevision( i );
        UT_nonnull_or_continue(r);
        ret[ std::make_pair( r->getId(), r->getType() ) ] = r;
    }
    return ret;
}

static std::string mergeAPStrings( const std::string& a, const std::string& b )
{
    if( b.empty() )
        return a;
    if( a.empty() )
        return b;
    std::stringstream ss;
    ss << a << ";" << b;
    return ss.str();
}


#define DEBUG_MERGEALL false

void PP_RevisionAttr::mergeAll( const PP_RevisionAttr& ra )
{
    PP_RevisionAttr us( getXMLstring() );
    m_vRev.clear();
    std::string tmp = static_cast<std::string>(us.getXMLstring() )+ "," + ra.getXMLstring();

    revidx_t oldidx = toIndex( us );
    revidx_t newidx = toIndex( ra );

    /*
     * Iterate over the entries in the oldidx merging the data from
     * newidx if found whenever a entry from newidx is used it is
     * removed from newidx too. This way, we can then just iterate
     * over newidx to add the entries which are in newidx but not in
     * oldidx.
     */
    revidx_t output;
    for( revidx_t::iterator iter = oldidx.begin(); iter != oldidx.end(); ++iter )
    {
        const PP_Revision* r = iter->second;
        revidx_t::iterator niter = newidx.find( iter->first );
        // UT_DEBUGMSG(("ODTCT ra::merge() id:%d attrs:%s props:%s\n",
        //              r->getId(), r->getAttrsString(), r->getPropsString() ));

        /*
         * If there is an entry in oldidx and newidx then merge them
         */
        if( niter != newidx.end() )
        {
            const PP_Revision* nr = niter->second;
            
            std::string attrs = mergeAPStrings( r->getAttrsString(), nr->getAttrsString() );
            std::string props = mergeAPStrings( r->getPropsString(), nr->getPropsString() );
            output[ iter->first ] = new PP_Revision( iter->first.first,
                                                     iter->first.second,
                                                     props.c_str(), attrs.c_str() );
            newidx.erase( niter );
        }
        else
        {
            /*
             * no matching entry in the newidx, just copy the data;
             * a bare revision mark still has to be preserved or the
             * merge would silently drop it
             */
            output[ iter->first ] = new PP_Revision( iter->first.first,
                                                     iter->first.second,
                                                     r->getPropsString(),
                                                     r->getAttrsString() );
        }
    }
    
    /*
     * copy over new revisions which didn't have a matching entry in the oldidx
     */
    for( revidx_t::iterator iter = newidx.begin(); iter != newidx.end(); ++iter )
    {
            output[ iter->first ] = new PP_Revision( iter->first.first,
                                                     iter->first.second,
                                                     iter->second->getPropsString(),
                                                     iter->second->getAttrsString() );
    }

    /*
     * Build the XML string for the merged revision attribute from the output index
     */
    bool outputssVirgin = true;
    std::stringstream outputss;
    for( revidx_t::iterator iter = output.begin(); iter != output.end(); ++iter )
    {
        const PP_Revision* r = iter->second;

        if( DEBUG_MERGEALL )
        {
            UT_DEBUGMSG(("ODTCT ra::merge() output id:%d t:%d attr:%s\n",
                         r->getId(), r->getType(), r->getAttrsString() ));
            UT_DEBUGMSG(("ODTCT ra::merge() output id:%d t:%d prop:%s\n",
                         r->getId(), r->getType(), r->getPropsString() ));
        }
        
        if( outputssVirgin ) outputssVirgin = false;
        else                 outputss << ",";
        
        outputss << r->toString();
    }
    UT_map_delete_all_second( output );
    

    
    setRevision(outputss.str());

    if( DEBUG_MERGEALL )
    {
        UT_DEBUGMSG(("ODTCT ra::merge() outputss::%s\n", outputss.str().c_str() ));
        UT_DEBUGMSG(("ODTCT ra::merge() ret:%s\n", getXMLstring().c_str()));
    }
    return;
}




/*! removes id from this revision, respecting the sign, i.e., it will
  not remove -5 if given 5
 */
void PP_RevisionAttr::removeRevisionIdWithType(UT_uint32 iId, PP_RevisionType eType)
{
	for (size_t i = 0; i < m_vRev.size(); i++)
	{
		PP_Revision * r = m_vRev.at(i).get();
		UT_nonnull_or_continue(r);

		if((iId == r->getId()) && (eType == r->getType()))
		{
			m_vRev.erase(m_vRev.begin() + i);
			_markDirty();
			return;
		}
	}
}

/*! removes id from the attribute disregarding sign, i.e.,
    if given 5 it will remove both -5 and +5
*/
void PP_RevisionAttr::removeRevisionIdTypeless(UT_uint32 iId)
{
	for (size_t i = 0; i < m_vRev.size(); i++)
	{
		PP_Revision * r = m_vRev.at(i).get();
		UT_nonnull_or_continue(r);

		if(iId == r->getId())
		{
			m_vRev.erase(m_vRev.begin() + i);
			_markDirty();
			return;
		}
	}
}

/*! removes pRev unconditionally from the attribute
*/
void PP_RevisionAttr::removeRevision(const PP_Revision * pRev)
{
	for (size_t i = 0; i < m_vRev.size(); i++)
	{
		PP_Revision * r = m_vRev.at(i).get();

		if(r == pRev)
		{
			m_vRev.erase(m_vRev.begin() + i);
			_markDirty();
			return;
		}
	}
}


/*! removes all IDs from the attribute whose value is lesser or
    equal the given id
*/
void PP_RevisionAttr::removeAllLesserOrEqualIds(UT_uint32 iId)
{
	for (size_t i = 0; i < m_vRev.size(); i++)
	{
		PP_Revision * r = m_vRev.at(i).get();
		UT_nonnull_or_continue(r);

		if(iId >= r->getId())
		{
			m_vRev.erase(m_vRev.begin() + i);
			--i; // the vector just shrunk
		}
	}

	_markDirty();
}

/*! removes all IDs from the attribute whose value is higher or
    equal the given id
*/
void PP_RevisionAttr::removeAllHigherOrEqualIds(UT_uint32 iId)
{
	for (size_t i = 0; i < m_vRev.size(); i++)
	{
		PP_Revision * r = m_vRev.at(i).get();
		UT_nonnull_or_continue(r);

		if(iId <= r->getId())
		{
			m_vRev.erase(m_vRev.begin() + i);
			--i; // the vector just shrunk
		}
	}

	_markDirty();
}


/*! create XML string from our vector
 */
void PP_RevisionAttr::_refreshString() const
{
  //	char buf[30];
	m_sXMLstring.clear();
	size_t iCount = m_vRev.size();

	for (size_t i = 0; i < iCount; i++)
	{
		const PP_Revision * r = m_vRev.at(i).get();

        if( !m_sXMLstring.empty() )
            m_sXMLstring += ",";
        
        m_sXMLstring += r->toString();
            
		// PP_Revision * r = (PP_Revision *)m_vRev.getNthItem(i);
		// PP_RevisionType r_type = r->getType();

		// if(r_type == PP_REVISION_FMT_CHANGE)
		// 	m_sXMLstring += "!";

		// // print the id with appropriate sign
		// sprintf(buf,"%d",r->getId()* ((r_type == PP_REVISION_DELETION)?-1:1));
		// m_sXMLstring += buf;

		// if(r_type != PP_REVISION_DELETION)
		// {
		// 	// if we have no props but have attribs, we have to issue empty braces so as not to
		// 	// confuse attribs with props
		// 	if(r->hasProperties() || r->hasAttributes())
		// 		m_sXMLstring += "{";
			
        // if(r->hasProperties())
        // 	m_sXMLstring += r->getPropsString();
			
        // if(r->hasProperties() || r->hasAttributes())
        // 	m_sXMLstring += "}";
			
		// 	if(r->hasAttributes())
		// 	{
		// 		m_sXMLstring += "{";
		// 		m_sXMLstring += r->getAttrsString();
		// 		m_sXMLstring += "}";
		// 	}
		// };

		// if(i != iCount - 1)
		// {
		// 	//not the last itteration, append ','
		// 	m_sXMLstring += ",";
		// }

	}
	m_bDirty = false;
}



/*! get an gchar string representation of this revision
 */
const std::string& PP_RevisionAttr::getXMLstring() const
{
	if(m_bDirty)
		_refreshString();

	return m_sXMLstring;
}

std::string
PP_RevisionAttr::getXMLstringUpTo( UT_uint32 iId ) const
{
    PP_RevisionAttr rat;
    rat.setRevision( getXMLstring() );
    UT_DEBUGMSG(("PP_RevisionAttr::getXMLstringUpTo() id:%d before:%s\n", iId, rat.getXMLstring().c_str()));
    rat.removeAllHigherOrEqualIds( iId );
    UT_DEBUGMSG(("PP_RevisionAttr::getXMLstringUpTo() id:%d  after:%s\n", iId, rat.getXMLstring().c_str()));
    // PP_RevisionAttr rat;
    // const PP_Revision* r = 0;
    // for( int raIdx = 0;
    //      raIdx < iId && (r = ra.getNthRevision( raIdx ));
    //      raIdx++ )
    // {
    //     rat.addRevision( r );
    // }
    return rat.getXMLstring();
}



/*! returns true if the fragment marked by this attribute is
    superfluous, i.e, it was created in the process of the present
    revision but the editor has later changed his/her mind and decided
    it should go away
*/
bool PP_RevisionAttr::isFragmentSuperfluous() const
{
	// the fragment is superfluous if the superfluous flag is set
	// and the fragment belongs only to a single revision level
	if (m_iSuperfluous != 0 && m_vRev.size() == 1) {
		auto rev = m_vRev.at(0).get();
		UT_nonnull_or_return(rev, false);
		UT_return_val_if_fail (rev->getId() == m_iSuperfluous,false);
		return true;
	}
	else
		return false;
}

bool PP_RevisionAttr::operator== (const PP_RevisionAttr &op2) const
{
	for (size_t i = 0; i < m_vRev.size(); i++) {
		const PP_Revision * r1 = m_vRev.at(i).get();

		for (size_t j = 0; j < op2.m_vRev.size(); j++) {
			const PP_Revision * r2 = op2.m_vRev.at(j).get();

			if(!(*r1 == *r2))
				return false;
		}
	}
	return true;
}

// PP_RevisionAttr&
// PP_RevisionAttr::operator=(const PP_RevisionAttr &rhs)
// {
//     setRevision( rhs.getXMLstring() );
//     return *this;
// }


/*! returns true if after revision iId this fragment carries revised
    property pName, the value of which will be stored in pValue; see
    notes on PP_Revision::hasProperty(...)
*/
bool PP_RevisionAttr::hasProperty(UT_uint32 iId, const gchar * pName, const gchar * &pValue) const
{
	const PP_Revision * s;
	const PP_Revision * r = getGreatestLesserOrEqualRevision(iId, &s);

	if(r)
		return r->getProperty(pName, pValue);

	return false;
}

/*! returns true if after the last revision this fragment carries revised
    property pName, the value of which will be stored in pValue; see
    notes on PP_Revision::hasProperty(...)
*/
bool PP_RevisionAttr::hasProperty(const gchar * pName, const gchar * &pValue) const
{
	const PP_Revision * r = getLastRevision();
	return r && r->getProperty(pName, pValue);
}

/*! returns the type of cumulative revision up to iId represented by this attribute
 */
PP_RevisionType PP_RevisionAttr::getType(UT_uint32 iId) const
{
	const PP_Revision * s;
	const PP_Revision * r = getGreatestLesserOrEqualRevision(iId,&s);

	if(!r)
	{
		// HACK need to return something
		return PP_REVISION_FMT_CHANGE;
	}
	
	return r->getType();
}

/*! returns the type of overall cumulative revision represented by this attribute
 */
PP_RevisionType PP_RevisionAttr::getType() const
{
	const PP_Revision * r = getLastRevision();
	return r ? r->getType() : PP_REVISION_FMT_CHANGE;
}


UT_uint32 PP_RevisionAttr::getHighestRevisionNumberWithAttribute( const gchar * attrName ) const
{
    const PP_Revision* r = nullptr;

    for( UT_uint32 raIdx = 0;
         raIdx < getRevisionsCount() && (r = getNthRevision( raIdx ));
         raIdx++ )
    {
        if (UT_getAttribute(r, attrName, nullptr))
            return r->getId();
    }
    return 0;
}


const char* UT_getAttribute( const PP_AttrProp* pAP, const char* name, const char* def  )
{
    const gchar* pValue = nullptr;

    bool ok = pAP->getAttribute(name, pValue);
    if (!ok)
    {
        pValue = def;
    }
    return pValue;
}

const PP_Revision *
PP_RevisionAttr::getLowestDeletionRevision() const
{
    if( !getRevisionsCount() )
        return nullptr;

    UT_uint32 rmax = getRevisionsCount();
    const PP_Revision* last  = getNthRevision( rmax-1 );
    UT_nonnull_or_return(last, nullptr);
    if( last->getType() != PP_REVISION_DELETION )
        return nullptr;
    
    for( long idx = rmax - 1; idx >= 0; --idx )
    {
        const PP_Revision* p = getNthRevision( idx );
        UT_nonnull_or_continue(p);
        if( p->getType() != PP_REVISION_DELETION )
        {
            return last;
        }
        last = p;
    }
    return last;
}


std::string UT_getLatestAttribute( const PP_AttrProp* pAP,
                                   const char* name,
                                   const char* def )
{
    const char* t = nullptr;
    std::string ret = def;
    bool ok = false;
    
    if (const char* revisionString = UT_getAttribute(pAP, "revision", nullptr))
    {
        PP_RevisionAttr ra( revisionString );
        const PP_Revision* r = nullptr;
            
        for( int raIdx = ra.getRevisionsCount()-1;
             raIdx >= 0 && (r = ra.getNthRevision( raIdx ));
             --raIdx )
        {
            ok = r->getAttribute( name, t );
            if (ok)
            {
                ret = t;
                return ret;
            }
        }
    }

    ok = pAP->getAttribute( name, t );
    if (ok)
    {
        ret = t;
        return ret;
    }
    ret = def;
    
    return ret;
}
