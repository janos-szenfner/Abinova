/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource
 * 
 * Copyright (C) 2009 Firat Kiyak <firatkiyak@gmail.com>
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
#include <OXMLi_Namespace_Common.h>

// Internal includes
#include <OXMLi_Types.h>

OXMLi_Namespace_Common::OXMLi_Namespace_Common()
{
	reset();
}

OXMLi_Namespace_Common::~OXMLi_Namespace_Common()
{

}

void OXMLi_Namespace_Common::reset()
{
	m_nsToURI.clear();
	m_uriToKey.clear();
	m_attsMap.clear();

	//add known URIs here
	m_nsToURI.insert(std::make_pair(NS_R_KEY, NS_R_URI));
	m_nsToURI.insert(std::make_pair(NS_V_KEY, NS_V_URI));
	m_nsToURI.insert(std::make_pair(NS_WX_KEY, NS_WX_URI));
	m_nsToURI.insert(std::make_pair(NS_WP_KEY, NS_WP_URI));
	m_nsToURI.insert(std::make_pair(NS_A_KEY, NS_A_URI));
	m_nsToURI.insert(std::make_pair(NS_W_KEY, NS_W_URI));
	m_nsToURI.insert(std::make_pair(NS_VE_KEY, NS_VE_URI));
	m_nsToURI.insert(std::make_pair(NS_O_KEY, NS_O_URI));
	m_nsToURI.insert(std::make_pair(NS_M_KEY, NS_M_URI));
	m_nsToURI.insert(std::make_pair(NS_W10_KEY, NS_W10_URI));
	m_nsToURI.insert(std::make_pair(NS_WNE_KEY, NS_WNE_URI));
	m_nsToURI.insert(std::make_pair(NS_PIC_KEY, NS_PIC_URI));
	m_nsToURI.insert(std::make_pair(NS_XML_KEY, NS_XML_URI));
	
	m_uriToKey.insert(std::make_pair(NS_R_URI, NS_R_KEY));
	m_uriToKey.insert(std::make_pair(NS_V_URI, NS_V_KEY));
	m_uriToKey.insert(std::make_pair(NS_WX_URI, NS_WX_KEY));
	m_uriToKey.insert(std::make_pair(NS_WP_URI, NS_WP_KEY));
	m_uriToKey.insert(std::make_pair(NS_A_URI, NS_A_KEY));
	m_uriToKey.insert(std::make_pair(NS_W_URI, NS_W_KEY));
	m_uriToKey.insert(std::make_pair(NS_VE_URI, NS_VE_KEY));
	m_uriToKey.insert(std::make_pair(NS_O_URI, NS_O_KEY));
	m_uriToKey.insert(std::make_pair(NS_M_URI, NS_M_KEY));
	m_uriToKey.insert(std::make_pair(NS_W10_URI, NS_W10_KEY));
	m_uriToKey.insert(std::make_pair(NS_WNE_URI, NS_WNE_KEY));
	m_uriToKey.insert(std::make_pair(NS_PIC_URI, NS_PIC_KEY));
	m_uriToKey.insert(std::make_pair(NS_XML_URI, NS_XML_KEY));

	/* ISO/IEC 29500 Strict namespaces (purl.oclc.org) map onto the
	 * same internal keys as their Transitional counterparts —
	 * otherwise strict-conforming documents import as empty */
	m_uriToKey.insert(std::make_pair(
		"http://purl.oclc.org/ooxml/wordprocessingml/main", NS_W_KEY));
	m_uriToKey.insert(std::make_pair(
		"http://purl.oclc.org/ooxml/officeDocument/relationships",
		NS_R_KEY));
	m_uriToKey.insert(std::make_pair(
		"http://purl.oclc.org/ooxml/drawingml/main", NS_A_KEY));
	m_uriToKey.insert(std::make_pair(
		"http://purl.oclc.org/ooxml/drawingml/wordprocessingDrawing",
		NS_WP_KEY));
	m_uriToKey.insert(std::make_pair(
		"http://purl.oclc.org/ooxml/drawingml/picture", NS_PIC_KEY));
	m_uriToKey.insert(std::make_pair(
		"http://purl.oclc.org/ooxml/officeDocument/math", NS_M_KEY));
}

void OXMLi_Namespace_Common::addNamespace(const char* ns, char* uri)
{
	if(!ns || !uri)
	{
		UT_DEBUGMSG(("FRT:OpenMXL importer invalid namespace or uri\n"));
		return;
	}

	std::string szName(ns);
	std::string szValue(uri);
	m_nsToURI.insert(std::make_pair(szName, szValue));
}

std::string OXMLi_Namespace_Common::processName(const char* name)
{
	std::string name_str(name);	    
  	size_t colon_index = name_str.find(':');

	if ((colon_index != std::string::npos) && (colon_index < name_str.length()-1))
	{
		std::string name_space = name_str.substr(0, colon_index);
		std::string tag_name = name_str.substr(colon_index+1);

		std::map<std::string, std::string>::iterator iter = m_nsToURI.find(name_space);
		if(iter == m_nsToURI.end())
		{
			UT_DEBUGMSG(("FRT:OpenXML importer unhandled URI for namespace:%s\n", name_space.c_str()));
			return name_str;
		}
		std::string uri = iter->second;

		iter = m_uriToKey.find(uri);
		if(iter == m_uriToKey.end())
		{
			UT_DEBUGMSG(("FRT:OpenXML importer unhandled namespace key for uri:%s\n", uri.c_str()));
			return name_str;
		}
		
		std::string pName = iter->second;		
		pName += ":";
		pName += tag_name;
		return pName;
	}
	return name_str;
}

std::map<std::string, std::string>* OXMLi_Namespace_Common::processAttributes(const char* tag, const char** atts)
{
	const char ** pp = atts;  
	m_attsMap.clear();

	std::string name_space("");
	std::string tag_name("");

	while (*pp)
	{
		std::string name_str(pp[0]);	    
	  	size_t colon_index = name_str.find(':');

		if (!((colon_index != std::string::npos) && (colon_index < name_str.length()-1)))
		{
			//attribute doesn't have a namespace prefix
			//try to use tag's namespace
			std::string tag_str(tag);
			colon_index = tag_str.find(':');
			if (!((colon_index != std::string::npos) && (colon_index < tag_str.length()-1)))
			{
				//well, tag also doesn't have a namespace prefix, let's skip this attribute
				UT_DEBUGMSG(("FRT:OpenXML importer unhandled attribute:%s\n", name_str.c_str()));
				pp += 2;
				continue;
			}
			name_space = tag_str.substr(0, colon_index);
			tag_name = name_str;
		}
		else
		{
			//attribute has a namespace prefix, let's extract it
			name_space = name_str.substr(0, colon_index);
			tag_name = name_str.substr(colon_index+1);
		}
			
		if(name_space.compare("xmlns") == 0)
		{
			m_nsToURI.insert(std::make_pair(tag_name,pp[1])); 
		}
		else
		{	
			std::map<std::string, std::string>::iterator iter = m_nsToURI.find(name_space);
			if(iter == m_nsToURI.end())
			{
				UT_DEBUGMSG(("FRT:OpenXML importer unhandled URI for namespace:%s\n", name_space.c_str()));
				pp += 2;
				continue;
			}
			std::string uri = iter->second;
			iter = m_uriToKey.find(uri);
			if(iter == m_uriToKey.end())
			{
				/* unknown namespace — keep the literal prefix:name so
				 * extension attributes (wp14:*, wps:*, ...) still reach
				 * the listeners instead of being dropped */
				std::string pName = name_space;
				pName += ":";
				pName += tag_name;
				m_attsMap.insert(std::make_pair(pName, std::string(pp[1])));
				pp += 2;
				continue;
			}

			std::string pName = iter->second;
			pName += ":";
			pName += tag_name;
			std::string pVal(pp[1]);
			m_attsMap.insert(std::make_pair(pName, pVal));
		}
		pp += 2;
	}

	return &m_attsMap;
}

/* mc:Choice/@Requires lists namespace prefixes a reader must
 * understand; returns true when the importer has meaningful handling
 * for the namespace the document binds to that prefix, either via a
 * registered URI key or as a literal prefix we parse by name */
bool OXMLi_Namespace_Common::isPrefixHandled(const std::string & prefix) const
{
	std::map<std::string, std::string>::const_iterator it =
		m_nsToURI.find(prefix);
	if (it == m_nsToURI.end())
		return false;
	if (m_uriToKey.find(it->second) != m_uriToKey.end())
		return true;

	/* extension namespaces parsed by their literal prefix */
	static const char * handledURIs[] = {
		"http://schemas.microsoft.com/office/word/2010/wordprocessingShape",
		"http://schemas.microsoft.com/office/word/2010/wordprocessingGroup",
		"http://schemas.microsoft.com/office/word/2010/wordprocessingDrawing",
		"http://schemas.microsoft.com/office/word/2010/wordprocessingCanvas",
		"http://schemas.microsoft.com/office/word/2010/wordml",
		"http://schemas.microsoft.com/office/word/2012/wordml",
		"http://schemas.microsoft.com/office/drawing/2010/main"
	};
	for (const char * uri : handledURIs)
		if (it->second == uri)
			return true;
	return false;
}
