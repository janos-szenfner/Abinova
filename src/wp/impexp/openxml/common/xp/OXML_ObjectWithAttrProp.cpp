/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */

/* AbiSource
 * 
 * Copyright (C) 2007 Philippe Milot <PhilMilot@gmail.com>
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
#include "OXML_ObjectWithAttrProp.h"

// Abinova includes
#include "ut_units.h"
#include "ut_string.h"
#include "ut_std_string.h"
#include "ie_exp_OpenXML.h"
#include "pd_Document.h"
#include "pt_Types.h"
#include "pp_Revision.h"
#include "OXMLi_Element_Revision.h"

// External includes
#include <cstring>
#include <string>

OXML_ObjectWithAttrProp::OXML_ObjectWithAttrProp() :
	m_pAttributes(new PP_AttrProp())
{
}

OXML_ObjectWithAttrProp::~OXML_ObjectWithAttrProp()
{
	DELETEP(m_pAttributes);
}


UT_Error OXML_ObjectWithAttrProp::setAttribute(const gchar * szName, const gchar * szValue)
{
	return m_pAttributes->setAttribute(szName, szValue) ? UT_OK : UT_ERROR;
}

UT_Error OXML_ObjectWithAttrProp::setProperty(const gchar * szName, const gchar * szValue)
{
	return m_pAttributes->setProperty(szName, szValue) ? UT_OK : UT_ERROR;
}

UT_Error OXML_ObjectWithAttrProp::setProperty(const std::string & szName, const std::string & szValue)
{
	return setProperty(szName.c_str(), szValue.c_str());
}

UT_Error OXML_ObjectWithAttrProp::getAttribute(const gchar * szName, const gchar *& szValue) const
{
    szValue = nullptr;
	UT_return_val_if_fail(szName && *szName, UT_ERROR);
	if(!m_pAttributes)
		return UT_ERROR;

	UT_Error ret;
	ret = m_pAttributes->getAttribute(szName, szValue) ? UT_OK : UT_ERROR;
	if(ret != UT_OK)
		return ret;

	return (szValue && *szValue) ? UT_OK : UT_ERROR;
}

UT_Error OXML_ObjectWithAttrProp::getProperty(const gchar * szName, const gchar *& szValue) const
{
    szValue = nullptr;
	UT_return_val_if_fail(szName && *szName, UT_ERROR);
	if(!m_pAttributes)
		return UT_ERROR;

	UT_Error ret;
	ret = m_pAttributes->getProperty(szName, szValue) ? UT_OK : UT_ERROR;
	if(ret != UT_OK)
		return ret;

	return (szValue && *szValue) ? UT_OK : UT_ERROR;
}

UT_Error OXML_ObjectWithAttrProp::setAttributes(const PP_PropertyVector & attributes)
{
	return m_pAttributes->setAttributes(attributes) ? UT_OK : UT_ERROR;
}

UT_Error OXML_ObjectWithAttrProp::setProperties(const PP_PropertyVector & properties)
{
	return m_pAttributes->setProperties(properties) ? UT_OK : UT_ERROR;
}

UT_Error OXML_ObjectWithAttrProp::appendAttributes(const PP_PropertyVector & attributes)
{
	UT_return_val_if_fail(!attributes.empty(), UT_ERROR);
	UT_Error ret;
	ASSERT_PV_SIZE(attributes);
	for (auto iter = attributes.cbegin(); iter != attributes.end(); iter += 2) {

		ret = setAttribute(iter->c_str(), (iter + 1)->c_str());
		if (ret != UT_OK) {
			return ret;
		}
	}
	return UT_OK;
}

UT_Error OXML_ObjectWithAttrProp::appendProperties(const PP_PropertyVector & properties)
{
	UT_return_val_if_fail(!properties.empty(), UT_ERROR);
	UT_Error ret;
	ASSERT_PV_SIZE(properties);
	for (auto iter = properties.cbegin(); iter != properties.end(); iter += 2) {
		ret = setProperty(iter->c_str(), (iter + 1)->c_str());
		if (ret != UT_OK) {
			return ret;
		}
	}
	return UT_OK;
}

PP_PropertyVector OXML_ObjectWithAttrProp::getAttributes() const
{
	return m_pAttributes->getAttributes();
}

PP_PropertyVector OXML_ObjectWithAttrProp::getProperties() const
{
	return m_pAttributes->getProperties();
}

PP_PropertyVector OXML_ObjectWithAttrProp::getAttributesWithProps()
{
	std::string propstring = _generatePropsString();
	if (propstring.empty())
		return getAttributes();

	// Use fakeprops here to avoid overwriting props attribute if already exists
	UT_return_val_if_fail(UT_OK == setAttribute("fakeprops", propstring.c_str()), PP_NOPROPS);
	PP_PropertyVector atts = getAttributes();
	ASSERT_PV_SIZE(atts);
	for (auto iter = atts.begin(); iter != atts.end(); iter += 2) {
		if (*iter == "fakeprops") {
			*iter = PT_PROPS_ATTRIBUTE_NAME;
		}
	}
	return atts;
}

std::string OXML_ObjectWithAttrProp::_generatePropsString() const
{
	PP_PropertyVector props = getProperties();
	if (props.empty()) {
		return "";
	}
	std::string fmt_props;

	ASSERT_PV_SIZE(props);
	for (auto iter = props.cbegin(); iter != props.cend(); iter += 2) {

		fmt_props += *iter + ":";
		fmt_props += *(iter + 1) + ";";
	}
	fmt_props.resize(fmt_props.length() - 1); //Shave off the last semicolon, appendFmt doesn't like it
	return fmt_props;
}

UT_Error OXML_ObjectWithAttrProp::inheritProperties(OXML_ObjectWithAttrProp* parent)
{
	if(!parent)
		return UT_ERROR;

	UT_Error ret = UT_OK;

	size_t numProps = parent->getPropertyCount();

	const gchar* szName;
	const gchar* szValue;

	for (size_t i = 0; i<numProps; i++) {
		
		if(!parent->getNthProperty(i, szName, szValue))
			break;

		const gchar * prop = nullptr;
		if((getProperty(szName, prop) != UT_OK) || !prop)
		{
			ret = setProperty(szName, szValue);		
			if(ret != UT_OK)
				return ret;
		}
	}

	return ret;
}

size_t OXML_ObjectWithAttrProp::getPropertyCount()
{
	return m_pAttributes->getPropertyCount();
}

bool OXML_ObjectWithAttrProp::getNthProperty(int i, const gchar* & szName, const gchar* & szValue)
{
	return m_pAttributes->getNthProperty(i, szName, szValue);
}

/* Writes a <w:pBdr> block for any paragraph-border properties present on
 * this object (used by both body paragraphs and paragraph styles).
 * Abinova edge props: <edge>-style (0 none/1 solid/2 dotted/3 dashed),
 * <edge>-thickness (pt), <edge>-space (pt), <edge>-color (rrggbb). */
UT_Error OXML_ObjectWithAttrProp::serializeParagraphBorders(IE_Exp_OpenXML* exporter, int target) const
{
	static const char * s_edges[][2] = {
		{ "top", "top" }, { "left", "left" },
		{ "bot", "bottom" }, { "right", "right" }
	};
	std::string pBdr;
	const gchar * szValue = nullptr;

	for (size_t e = 0; e < G_N_ELEMENTS(s_edges); e++)
	{
		std::string prop = std::string(s_edges[e][0]) + "-style";
		if (getProperty(prop.c_str(), szValue) != UT_OK || !szValue)
			continue;
		const char * val = "single";
		if (!strcmp(szValue, "0") || !strcmp(szValue, "none"))
			val = "nil";
		else if (!strcmp(szValue, "2") || !strcmp(szValue, "dotted"))
			val = "dotted";
		else if (!strcmp(szValue, "3") || !strcmp(szValue, "dashed"))
			val = "dashed";

		pBdr += "<w:";
		pBdr += s_edges[e][1];
		pBdr += " w:val=\"";
		pBdr += val;
		pBdr += "\"";

		prop = std::string(s_edges[e][0]) + "-thickness";
		const gchar * szThick = nullptr;
		if (getProperty(prop.c_str(), szThick) == UT_OK && szThick)
		{
			double pt = UT_convertToPoints(szThick);
			if (pt > 0)
			{
				char buf[24];
				g_snprintf(buf, sizeof(buf), " w:sz=\"%d\"",
						   static_cast<int>(pt * 8 + 0.5));
				pBdr += buf;
			}
		}
		prop = std::string(s_edges[e][0]) + "-space";
		const gchar * szSpace = nullptr;
		if (getProperty(prop.c_str(), szSpace) == UT_OK && szSpace)
		{
			double pt = UT_convertToPoints(szSpace);
			if (pt >= 0)
			{
				char buf[24];
				g_snprintf(buf, sizeof(buf), " w:space=\"%d\"",
						   static_cast<int>(pt + 0.5));
				pBdr += buf;
			}
		}
		prop = std::string(s_edges[e][0]) + "-color";
		const gchar * szColor = nullptr;
		if (getProperty(prop.c_str(), szColor) == UT_OK && szColor && *szColor)
		{
			pBdr += " w:color=\"";
			pBdr += (szColor[0] == '#') ? szColor + 1 : szColor;
			pBdr += "\"";
		}
		pBdr += "/>";
	}
	if (pBdr.empty())
		return UT_OK;

	pBdr = "<w:pBdr>" + pBdr + "</w:pBdr>";
	return exporter->setParagraphBorders(target, pBdr.c_str());
}

void OXML_ObjectWithAttrProp::addRevisionMark(bool deleted,
											  const gchar * author,
											  const gchar * date)
{
	OXML_StruxRevision m;
	m.name = PT_REVISION_ATTRIBUTE_NAME;
	m.deletion = deleted;
	if (author) m.author = author;
	if (date) m.date = date;
	m_revMarks.push_back(m);
}

void OXML_ObjectWithAttrProp::addChangeMark(const gchar * name,
											const gchar * author,
											const gchar * date,
											const PP_PropertyVector & props,
											const PP_PropertyVector & attrs)
{
	if (!name || !*name)
		return;
	OXML_StruxRevision m;
	m.name = name;
	if (author) m.author = author;
	if (date) m.date = date;
	m.props = props;
	m.attrs = attrs;
	m_revMarks.push_back(m);
}

void OXML_ObjectWithAttrProp::appendRevisionMarks(
	const std::vector<OXML_StruxRevision> & marks)
{
	m_revMarks.insert(m_revMarks.end(), marks.begin(), marks.end());
}

const std::vector<OXML_StruxRevision> &
OXML_ObjectWithAttrProp::getRevisionMarks() const
{
	return m_revMarks;
}

/* name:value;name:value — the encoding PP_Revision uses inside the
 * brace groups of a "!id" revision token. */
static std::string _pvToChangeString(const PP_PropertyVector & v)
{
	std::string s;
	ASSERT_PV_SIZE(v);
	for (auto it = v.cbegin(); it != v.cend(); it += 2)
	{
		s += *it;
		s += ':';
		s += *(it + 1);
		s += ';';
	}
	if (!s.empty())
		s.resize(s.length() - 1);
	return s;
}

/* Register each pending strux revision mark on the document revision
 * table and fold it into this object's attributes:
 *  - "revision" marks become revision="+id"/"-id" on the strux AP;
 *  - *Change marks become "name"="!id{oldProps}{oldAttrs}", an inert
 *    record mirroring the span-level revision-attr grammar.
 * Safe to call repeatedly — marks are consumed. */
void OXML_ObjectWithAttrProp::applyRevisionMarks(PD_Document * pDocument)
{
	if (m_revMarks.empty() || !pDocument)
		return;
	for (auto & m : m_revMarks)
	{
		UT_uint32 id = OXMLi_Element_Revision::registerRevision(
			pDocument, m.author, m.date);
		if (m.name == PT_REVISION_ATTRIBUTE_NAME)
		{
			std::string tok = m.deletion ? "-" : "+";
			tok += UT_std_string_sprintf("%u", id);
			setAttribute(PT_REVISION_ATTRIBUTE_NAME, tok.c_str());
		}
		else
		{
			std::string tok = "!";
			tok += UT_std_string_sprintf("%u", id);
			tok += '{';
			tok += _pvToChangeString(m.props);
			tok += "}{";
			tok += _pvToChangeString(m.attrs);
			tok += '}';
			setAttribute(m.name.c_str(), tok.c_str());
		}
	}
	m_revMarks.clear();
}

/* Parse an inert "!id{props}{attrs}" change record — the form
 * applyRevisionMarks() writes onto strux APs for w:*Change marks, and
 * the same grammar span-level format changes use inside "revision".
 * The PP_Revision props/attrs vectors hold the pre-change snapshot in
 * the usual name:value pairs, ready to be replayed through the
 * matching element's property serializer on export. */
bool OXML_ObjectWithAttrProp::parseChangeMark(const gchar * szValue,
											  UT_uint32 & revId,
											  PP_PropertyVector & props,
											  PP_PropertyVector & attrs)
{
	revId = 0;
	if (!szValue || szValue[0] != '!')
		return false;

	PP_RevisionAttr ra(szValue);
	if (ra.getRevisionsCount() < 1)
		return false;

	const PP_Revision * r = ra.getNthRevision(0);
	if (!r || r->getType() != PP_REVISION_FMT_CHANGE)
		return false;

	revId = r->getId();
	props = r->getProperties();
	attrs = r->getAttributes();
	return true;
}

/* Case-insensitive attribute lookup for change marks: the OOXML
 * importer stores them under their camelCase element names
 * ("tblGridChange"), but a docx -> abwn -> docx round-trip arrives
 * with the lowercased .abwn spelling ("tblgridchange"). */
UT_Error OXML_ObjectWithAttrProp::getChangeMark(const gchar * szName,
											  const gchar *& szValue) const
{
	szValue = nullptr;
	UT_return_val_if_fail(szName && *szName, UT_ERROR);
	if(!m_pAttributes)
		return UT_ERROR;

	const gchar * pN = nullptr;
	for (size_t i = 0; m_pAttributes->getNthAttribute(static_cast<int>(i), pN, szValue); ++i)
	{
		if (pN && !g_ascii_strcasecmp(pN, szName))
			return (szValue && *szValue) ? UT_OK : UT_ERROR;
	}
	return UT_ERROR;
}
