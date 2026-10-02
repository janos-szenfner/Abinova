/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

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
#include "OXMLi_PackageManager.h"

// Internal includes
#include "OXML_Types.h"
#include "OXML_Document.h"
#include "OXMLi_StreamListener.h"
#include "OXMLi_ListenerState.h"
#include "OXML_Section.h"

// Abinova includes
#include "ut_types.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ut_xml.h"

OXMLi_PackageManager* OXMLi_PackageManager::s_pInst = nullptr;

OXMLi_PackageManager* OXMLi_PackageManager::getNewInstance()
{
	OXMLi_PackageManager::destroyInstance();
	return OXMLi_PackageManager::getInstance();
}

OXMLi_PackageManager* OXMLi_PackageManager::getInstance()
{
	if (s_pInst == nullptr) {
		try {
			s_pInst = new OXMLi_PackageManager();
		} catch(...) {
			UT_DEBUGMSG(("Could not allocate memory!\n"));
			return nullptr;
		}
	}
	return s_pInst;
}

void OXMLi_PackageManager::destroyInstance()
{
	DELETEP(s_pInst);
}

OXMLi_PackageManager::OXMLi_PackageManager() :
	m_pPkg(nullptr),
	m_pDocPart(nullptr)
{
}

OXMLi_PackageManager::~OXMLi_PackageManager()
{
	if (m_pPkg) {
		g_object_unref (G_OBJECT(m_pPkg));
	}
	if (m_pDocPart) {
		g_object_unref (G_OBJECT(m_pDocPart));
	}
	m_parsedParts.clear();
}

void OXMLi_PackageManager::setContainer(GsfInfile * pPkg)
{
	if (m_pPkg) {
		g_object_unref (G_OBJECT(m_pPkg));
	}
	if (m_pDocPart) {
		g_object_unref (G_OBJECT(m_pDocPart));
	}
	m_pPkg = pPkg;
}

UT_Error OXMLi_PackageManager::parseDocumentStream()
{
	OXMLi_StreamListener listener; 
	listener.setupStates(DOCUMENT_PART);
	return _parseStream( _getDocumentStream(), &listener); 
}

UT_Error OXMLi_PackageManager::parseDocumentHdrFtr( const char * id )
{
	GsfInput * doc = _getDocumentStream();
	UT_return_val_if_fail(doc != nullptr, UT_ERROR);
	OXMLi_StreamListener listener;
	listener.setupStates(HEADER_PART, id); //Doesn't matter whether it's header or footer
	return parseChildById(doc, id, &listener); 
}

UT_Error OXMLi_PackageManager::parseDocumentStyles()
{
	GsfInput * doc = _getDocumentStream();
	UT_return_val_if_fail(doc != nullptr, UT_ERROR);
	OXMLi_StreamListener listener;
	listener.setupStates(STYLES_PART);
	return parseChildByType(doc, STYLES_PART, &listener); 
}

UT_Error OXMLi_PackageManager::parseDocumentTheme()
{
	GsfInput * doc = _getDocumentStream();
	UT_return_val_if_fail(doc != nullptr, UT_ERROR);
	OXMLi_StreamListener listener;
	listener.setupStates(THEME_PART);
	UT_Error err = parseChildByType(doc, THEME_PART, &listener); 
	//themes are optional in .docx files
	if(err != UT_OK){
		UT_DEBUGMSG(("FRT: OpenXML Theme Part is not found\n"));
	}
	return UT_OK;
}

UT_Error OXMLi_PackageManager::parseDocumentSettings()
{
	GsfInput * doc = _getDocumentStream();
	UT_return_val_if_fail(doc != nullptr, UT_ERROR);
	OXMLi_StreamListener listener;
	listener.setupStates(DOCSETTINGS_PART);
	return parseChildByType(doc, DOCSETTINGS_PART, &listener); 
}

UT_Error OXMLi_PackageManager::parseDocumentNumbering()
{
	GsfInput * doc = _getDocumentStream();
	UT_return_val_if_fail(doc != nullptr, UT_ERROR);
	OXMLi_StreamListener listener;
	listener.setupStates(NUMBERING_PART);
	return parseChildByType(doc, NUMBERING_PART, &listener); 
}

UT_Error OXMLi_PackageManager::parseDocumentFootnotes()
{
	GsfInput * doc = _getDocumentStream();
	UT_return_val_if_fail(doc != nullptr, UT_ERROR);
	OXMLi_StreamListener listener;
	listener.setupStates(FOOTNOTES_PART);
	return parseChildByType(doc, FOOTNOTES_PART, &listener); 
}

UT_Error OXMLi_PackageManager::parseDocumentEndnotes()
{
	GsfInput * doc = _getDocumentStream();
	UT_return_val_if_fail(doc != nullptr, UT_ERROR);
	OXMLi_StreamListener listener;
	listener.setupStates(ENDNOTES_PART);
	return parseChildByType(doc, ENDNOTES_PART, &listener);
}

UT_Error OXMLi_PackageManager::parseDocumentComments()
{
	GsfInput * doc = _getDocumentStream();
	UT_return_val_if_fail(doc != nullptr, UT_ERROR);
	OXMLi_StreamListener listener;
	listener.setupStates(COMMENTS_PART);
	return parseChildByType(doc, COMMENTS_PART, &listener);
}

GsfInput* OXMLi_PackageManager::getChildById( GsfInput * parent, const char * id )
{
	GsfInput * pInput =
		gsf_open_pkg_open_rel_by_id(parent, id, nullptr);
	if (!pInput)
		pInput = _relLookup(parent, id, nullptr);
	return pInput;
}

GsfInput* OXMLi_PackageManager::getChildByType( GsfInput * parent, OXML_PartType type )
{
	const char * fulltype;
	fulltype = _getFullType(type);
	UT_return_val_if_fail(fulltype != nullptr, nullptr);
	GsfInput * pInput =
		gsf_open_pkg_open_rel_by_type(parent, fulltype, nullptr);
	if (!pInput)
	{
		/* ISO Strict packages use purl.oclc.org relationship types —
		 * retry with the strict URI so those documents still open */
		static const char * transPrefix =
			"http://schemas.openxmlformats.org/officeDocument/2006/relationships/";
		static const char * strictPrefix =
			"http://purl.oclc.org/ooxml/officeDocument/relationships/";
		if (!strncmp(fulltype, transPrefix, strlen(transPrefix)))
		{
			std::string strictType = strictPrefix;
			strictType += fulltype + strlen(transPrefix);
			pInput = gsf_open_pkg_open_rel_by_type(
				parent, strictType.c_str(), nullptr);
			/* when libgsf cannot read strict .rels parts at all,
			 * resolve the relationship from our own map instead */
			if (!pInput)
				pInput = _relLookup(parent, nullptr, strictType.c_str());
		}
		if (!pInput)
			pInput = _relLookup(parent, nullptr, fulltype);
	}
	return pInput;
}

const std::string & OXMLi_PackageManager::_docDir()
{
	if (m_docDir.empty())
	{
		if (!m_rootRelsLoaded)
		{
			m_rootRelsLoaded = true;
			_loadRels("_rels/.rels", m_rootRels);
		}
		for (auto & kv : m_rootRels)
		{
			if (kv.second.target.empty())
				continue;
			size_t slash = kv.second.target.find('/');
			if (slash != std::string::npos &&
				kv.second.target.find("document.xml") != std::string::npos)
			{
				m_docDir = kv.second.target.substr(0, slash + 1);
				break;
			}
		}
		if (m_docDir.empty())
			m_docDir = "word/";
	}
	return m_docDir;
}

/* open a zip member by its slash-separated path — gsf children are
 * addressed one path component at a time */
GsfInput * OXMLi_PackageManager::_childByPath(const std::string & path)
{
	if (!m_pPkg || path.empty() || path[0] == '/')
		return nullptr;
	GsfInput * cur = GSF_INPUT(m_pPkg);
	g_object_ref(cur);
	size_t pos = 0;
	while (pos < path.size() && cur)
	{
		size_t slash = path.find('/', pos);
		std::string comp = path.substr(
			pos, slash == std::string::npos ? std::string::npos :
				slash - pos);
		if (comp.empty() || comp == "." || comp == "..")
			return (g_object_unref(cur), nullptr);
		GsfInput * next =
			gsf_infile_child_by_name(GSF_INFILE(cur), comp.c_str());
		g_object_unref(cur);
		cur = next;
		pos = (slash == std::string::npos) ? path.size() : slash + 1;
	}
	return cur;
}

/* read a .rels part directly from the package and build an
 * Id -> {type,target} map. Used when libgsf does not recognise the
 * rels namespace (ISO Strict packages). */
bool OXMLi_PackageManager::_loadRels(const std::string & zipPath,
									 std::map<std::string, _RelRec> & out)
{
	GsfInput * rels = _childByPath(zipPath);
	if (!rels)
		return false;
	gsf_off_t len = gsf_input_remaining(rels);
	// reject absurd declared sizes — the zip directory is
	// attacker-controlled and the read materializes it all
	const guint8 * data =
		(len > 0 && len <= UT_MAX_ARCHIVE_MEMBER_SIZE)
			? gsf_input_read(rels, len, nullptr) : nullptr;
	if (!data)
	{
		g_object_unref(rels);
		return false;
	}
	std::string xml(reinterpret_cast<const char *>(data),
					static_cast<size_t>(len));
	g_object_unref(rels);

	size_t pos = 0;
	while ((pos = xml.find("<Relationship", pos)) != std::string::npos)
	{
		size_t end = xml.find('>', pos);
		if (end == std::string::npos)
			break;
		std::string tag = xml.substr(pos, end - pos);
		pos = end + 1;
		auto attr = [&tag](const char * n) -> std::string {
			std::string pat = std::string(n) + "=";
			size_t a = tag.find(pat);
			if (a == std::string::npos)
				return "";
			a += pat.size();
			if (a >= tag.size() || (tag[a] != '"' && tag[a] != '\''))
				return "";
			char q = tag[a++];
			size_t b = tag.find(q, a);
			return b == std::string::npos ? "" : tag.substr(a, b - a);
		};
		std::string id = attr("Id");
		if (id.empty())
			continue;
		_RelRec rec;
		rec.type = attr("Type");
		rec.target = attr("Target");
		rec.external = (attr("TargetMode") == "External");
		out[id] = rec;
	}
	return true;
}

/* fallback rel resolution for packages libgsf cannot enumerate —
 * supports the package root and the main document part */
GsfInput * OXMLi_PackageManager::_relLookup(GsfInput * parent,
											const char * id,
											const char * type)
{
	const std::map<std::string, _RelRec> * rels = nullptr;
	std::string base;
	if (m_pPkg && parent == GSF_INPUT(m_pPkg))
	{
		if (!m_rootRelsLoaded)
		{
			m_rootRelsLoaded = true;
			_loadRels("_rels/.rels", m_rootRels);
		}
		rels = &m_rootRels;
	}
	else if (m_pDocPart && parent == m_pDocPart)
	{
		if (!m_docRelsLoaded)
		{
			m_docRelsLoaded = true;
			_loadRels(_docDir() + "_rels/document.xml.rels", m_docRels);
		}
		rels = &m_docRels;
		base = _docDir();
	}
	if (!rels)
		return nullptr;
	for (auto & kv : *rels)
	{
		if ((id && kv.first == id) ||
			(type && !kv.second.type.empty() && kv.second.type == type))
		{
			if (kv.second.external || kv.second.target.empty())
				return nullptr;
			return _childByPath(base + kv.second.target);
		}
	}
	return nullptr;
}

UT_Error OXMLi_PackageManager::parseChildById( GsfInput * parent, const char * id, OXMLi_StreamListener * pListener)
{
	GsfInput * pInput = getChildById(parent, id);
	UT_return_val_if_fail(pInput != nullptr, UT_ERROR);
	auto error = _parseStream( pInput, pListener);
	g_object_unref(pInput);
	return error;
}

UT_Error OXMLi_PackageManager::parseChildByType( GsfInput * parent, OXML_PartType type, OXMLi_StreamListener * pListener)
{
	GsfInput * pInput = getChildByType(parent, type);
	if(!pInput)
		return UT_ERROR;

	auto error =  _parseStream( pInput, pListener);
	g_object_unref(pInput);
	return error;
}

const char * OXMLi_PackageManager::_getFullType( OXML_PartType type )
{	//There's probably a better way to do this...
	const char * ret;
	switch (type)
	{
	case ALTERNATEFORMAT_PART:
		ret = ALTERNATEFORMAT_REL_TYPE;
		break;
	case COMMENTS_PART:
		ret = COMMENTS_REL_TYPE;
		break;
	case DOCSETTINGS_PART:
		ret = DOCSETTINGS_REL_TYPE;
		break;
	case DOCUMENT_PART:
		ret = DOCUMENT_REL_TYPE;
		break;
	case ENDNOTES_PART:
		ret = ENDNOTES_REL_TYPE;
		break;
	case FONTTABLE_PART:
		ret = FONTTABLE_REL_TYPE;
		break;
	case FOOTER_PART:
		ret = FOOTER_REL_TYPE;
		break;
	case FOOTNOTES_PART:
		ret = FOOTNOTES_REL_TYPE;
		break;
	case GLOSSARY_PART:
		ret = GLOSSARY_REL_TYPE;
		break;
	case HEADER_PART:
		ret = HEADER_REL_TYPE;
		break;
	case NUMBERING_PART:
		ret = NUMBERING_REL_TYPE;
		break;
	case STYLES_PART:
		ret = STYLES_REL_TYPE;
		break;
	case WEBSETTINGS_PART:
		ret = WEBSETTINGS_REL_TYPE;
		break;
	case IMAGE_PART:
		ret = IMAGE_REL_TYPE;
		break;
	case THEME_PART:
		ret = THEME_REL_TYPE;
		break;
	default:
		ret = nullptr;
	}
	return ret;
}

GsfInput * OXMLi_PackageManager::_getDocumentStream()
{
	UT_return_val_if_fail(m_pPkg != nullptr, nullptr);

	if (m_pDocPart == nullptr)
		m_pDocPart = getChildByType ( GSF_INPUT (m_pPkg), DOCUMENT_PART );
	return m_pDocPart;
}

UT_Error OXMLi_PackageManager::_parseStream( GsfInput * stream, OXMLi_StreamListener * pListener)
{
	UT_return_val_if_fail(stream != nullptr && pListener != nullptr , UT_ERROR);

	//First, we check if this stream has already been parsed before
	std::string part_name = gsf_input_name(stream); //TODO: determine if part names are truly unique
	std::map<std::string, bool>::iterator it;
	it = m_parsedParts.find(part_name);
	if (it != m_parsedParts.end() && it->second) {
		//this stream has already been parsed successfully
		return UT_OK;
	}

	UT_Error ret = UT_OK;
	guint8 const *data = nullptr;
	const char * cdata = nullptr;
	size_t len = 0;

	UT_XML reader;
	reader.setListener(pListener);

	// reject members whose declared size is absurd — the zip directory
	// is attacker-controlled and gsf_input_read materializes it all
	if (gsf_input_size (stream) > UT_MAX_ARCHIVE_MEMBER_SIZE) {
		return UT_ERROR;
	}

	if (gsf_input_size (stream) > 0) {
		len = gsf_input_remaining (stream);
		if (len > 0) {
			data = gsf_input_read (stream, len, nullptr);
			if (nullptr == data) {
				// caller owns stream and unrefs it — do not unref here
				return UT_ERROR;
			}
			cdata = reinterpret_cast<const char *>(data);
			ret = reader.parse (cdata, len);
		}
	}

	//There are two error codes to check here.  
	if (ret == UT_OK && pListener->getStatus() == UT_OK)
		m_parsedParts[part_name] = true;

	//We prioritize the one from UT_XML when returning.
	return ret == UT_OK ? pListener->getStatus() : ret;
}

/**
 * Parses the image stream and returns the image data
 */
UT_ConstByteBufPtr OXMLi_PackageManager::parseImageStream(const char * id)
{
	GsfInput * parent = _getDocumentStream();
	GsfInput * stream = getChildById(parent, id);

	//the image relationship may not exist (broken or strict package)
	if (stream == nullptr)
		return nullptr;

	// reject members whose declared size is absurd — the zip directory
	// is attacker-controlled
	if (gsf_input_size(stream) > UT_MAX_ARCHIVE_MEMBER_SIZE)
	{
		g_object_unref (G_OBJECT (stream));
		return nullptr;
	}

	//First, we check if this stream has already been parsed before
	std::string part_name = gsf_input_name(stream); //TODO: determine if part names are truly unique
	std::map<std::string, bool>::iterator it;
	it = m_parsedParts.find(part_name);
	if (it != m_parsedParts.end() && it->second) {
		//this stream has already been parsed successfully
		return nullptr;
	}

	UT_ByteBufPtr buffer(new UT_ByteBuf);
	buffer->insertFromInput(0, stream);
	g_object_unref (G_OBJECT (stream));

	m_parsedParts[part_name] = true;

	return buffer;
}

/**
 * This function is needed for external targets. Ex: hyperlinks, bookmarks.
 */
std::string OXMLi_PackageManager::getPartName(const char * id)
{
	GsfInput * parent = _getDocumentStream();
	const char* target = nullptr;
	if (parent)
	{
		const GsfOpenPkgRel * rel =
			gsf_open_pkg_lookup_rel_by_id(parent, id);
		if (rel)
			target = gsf_open_pkg_rel_get_target(rel);
	}
	if (!target && parent)
	{
		/* strict packages: look the id up in the rel map and return
		 * the recorded target without opening the part */
		if (!m_docRelsLoaded)
		{
			m_docRelsLoaded = true;
			_loadRels(_docDir() + "_rels/document.xml.rels", m_docRels);
		}
		auto it = m_docRels.find(id);
		if (it != m_docRels.end())
			return _docDir() + it->second.target;
		return "";
	}
	return target ? std::string(target) : std::string();
}

