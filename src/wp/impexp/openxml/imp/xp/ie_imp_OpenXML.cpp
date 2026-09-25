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
#include "ie_imp_OpenXML.h"

// Internal includes
#include "OXML_Types.h"
#include "OXML_Element.h"
#include "OXML_Section.h"
#include "OXML_Document.h"
#include "OXML_Style.h"
#include "OXML_Theme.h"
#include "OXMLi_PackageManager.h"

// Abinova includes
#include "ut_types.h"
#include "ut_assert.h"
#include "ut_xml.h"
#include "pd_Document.h"

#include <iostream>

namespace {

/*
 * Maps docProps/core.xml and docProps/app.xml element names to the
 * document's meta-data keys, so Word's document properties (Title,
 * Author, Category, Company, revision, dates, ...) survive import
 * instead of being silently dropped.
 */
struct _CorePropKV { const char * szElement; const char * szMetaKey; };

static const _CorePropKV s_coreProps[] =
{
	{ "title",           PD_META_KEY_TITLE },
	{ "subject",         PD_META_KEY_SUBJECT },
	{ "creator",         PD_META_KEY_CREATOR },
	{ "keywords",        PD_META_KEY_KEYWORDS },
	{ "description",     PD_META_KEY_DESCRIPTION },
	{ "lastModifiedBy",  PD_META_KEY_LASTMODIFIEDBY },
	{ "revision",        PD_META_KEY_REVISION },
	{ "lastPrinted",     PD_META_KEY_LASTPRINTED },
	{ "category",        PD_META_KEY_CATEGORY },
	{ "contentStatus",   PD_META_KEY_CONTENTSTATUS },
	{ "language",        PD_META_KEY_LANGUAGE },
	{ "created",         PD_META_KEY_DATE },
	{ "modified",        PD_META_KEY_DATE_LAST_CHANGED },
	{ nullptr,            nullptr }
};

static const _CorePropKV s_appProps[] =
{
	{ "Company",         PD_META_KEY_COMPANY },
	{ "Manager",         PD_META_KEY_MANAGER },
	{ "Template",        PD_META_KEY_TEMPLATE },
	{ "Application",     PD_META_KEY_GENERATOR },
	{ "TotalTime",       PD_META_KEY_EDITING_DURATION },
	{ nullptr,            nullptr }
};

class _PropsXmlListener : public UT_XML::Listener
{
public:
	_PropsXmlListener(PD_Document * pDoc, const _CorePropKV * table)
		: m_pDoc(pDoc), m_table(table)
	{}

	virtual void startElement(const gchar * /*name*/,
							  const gchar ** /*atts*/) override
	{
		m_data.clear();
	}

	virtual void endElement(const gchar * name) override
	{
		if (!name || m_data.empty())
			return;

		// element names may carry a namespace prefix (dc:, cp:, dcterms:)
		const char * local = strrchr(name, ':');
		local = local ? local + 1 : name;

		for (const _CorePropKV * kv = m_table; kv->szElement; ++kv)
		{
			if (strcmp(kv->szElement, local) == 0)
			{
				m_pDoc->setMetaDataProp(kv->szMetaKey, m_data);
				return;
			}
		}
	}

	virtual void charData(const gchar * buffer, int length) override
	{
		if (buffer && length > 0)
			m_data.append(buffer, length);
	}

private:
	PD_Document *       m_pDoc;
	const _CorePropKV * m_table;
	std::string         m_data;
};

static void _parsePropsPart(GsfInfile * zip, const char * szName,
							const _CorePropKV * table, PD_Document * pDoc)
{
	// szName is "dir/file"; gsf exposes zip members as nested infiles
	std::string name(szName);
	std::string::size_type slash = name.find('/');
	GsfInfile * dir = zip;
	if (slash != std::string::npos)
	{
		GsfInput * sub = gsf_infile_child_by_name(zip, name.substr(0, slash).c_str());
		if (!sub || !GSF_IS_INFILE(sub))
		{
			if (sub) g_object_unref(sub);
			return; // part is optional
		}
		dir = GSF_INFILE(sub);
		name = name.substr(slash + 1);
	}
	GsfInput * stream = gsf_infile_child_by_name(dir, name.c_str());
	if (dir != zip) g_object_unref(dir);
	if (!stream)
		return; // part is optional

	_PropsXmlListener listener(pDoc, table);
	UT_XML reader;
	reader.setListener(&listener);

	size_t len = gsf_input_remaining(stream);
	if (len > 0)
	{
		const guint8 * data = gsf_input_read(stream, len, nullptr);
		if (data)
			reader.parse(reinterpret_cast<const char *>(data), len);
	}
	g_object_unref(G_OBJECT(stream));
}

} // anonymous namespace

/**
 * Constructor
 */
IE_Imp_OpenXML::IE_Imp_OpenXML (PD_Document * pDocument)
  : IE_Imp (pDocument)
{
}


/*
 * Destructor
 */
IE_Imp_OpenXML::~IE_Imp_OpenXML ()
{
	_cleanup();
}

/**
 * Import the given file
 */
UT_Error IE_Imp_OpenXML::_loadFile (GsfInput * oo_src)
{
	UT_DEBUGMSG(("\n\n\nLoading an OpenXML file\n"));

	UT_Error ret = UT_OK;

	GsfInfile * pGsfInfile = GSF_INFILE (gsf_infile_zip_new (oo_src, nullptr));
    
	if (pGsfInfile == nullptr) {
		return UT_ERROR;
	}

	OXMLi_PackageManager * mgr = OXMLi_PackageManager::getNewInstance();
	if (mgr == nullptr) {
		g_object_unref (G_OBJECT(pGsfInfile));
		_cleanup();
		return UT_ERROR;
	}

	mgr->setContainer(pGsfInfile);

	// document properties (docProps/core.xml + docProps/app.xml) are
	// optional parts; pull them into the document's meta-data map so
	// nothing Word stored is lost
	_parsePropsPart(pGsfInfile, "docProps/core.xml", s_coreProps, getDoc());
	_parsePropsPart(pGsfInfile, "docProps/app.xml", s_appProps, getDoc());

	UT_DEBUGMSG(("Building the data model...\n"));
	//These calls build the data model
	if (UT_OK != (ret = mgr->parseDocumentFootnotes()))
	{
		UT_DEBUGMSG(("OpenXML import: failed to parse the document footnotes\n"));
	}

	if (UT_OK != (ret = mgr->parseDocumentEndnotes()))
	{
		UT_DEBUGMSG(("OpenXML import: failed to parse the document endnotes\n"));
	}

	if (UT_OK != (ret = mgr->parseDocumentTheme()))
	{
		UT_DEBUGMSG(("OpenXML import: failed to parse the document theme\n"));
	}

	if (UT_OK != (ret = mgr->parseDocumentSettings()))
	{
		UT_DEBUGMSG(("OpenXML import: failed to parse the document settings\n"));
	}

	if (UT_OK != (ret = mgr->parseDocumentStyles()))
	{
		UT_DEBUGMSG(("OpenXML import: failed to parse the document styles\n"));
	}

	if (UT_OK != (ret = mgr->parseDocumentNumbering()))
	{
		UT_DEBUGMSG(("OpenXML import: failed to parse the document numbering\n"));
	}

	if (UT_OK != (ret = mgr->parseDocumentStream()))
	{
		_cleanup();
		return ret;
	}

	UT_DEBUGMSG(("Data model built.  Building piecetable...\n"));

	OXML_Document * doc = OXML_Document::getInstance();
	if (doc == nullptr) {
		_cleanup();
		return UT_ERROR;
	}

	//This call builds the piecetable from the data model
	if (UT_OK != (ret = doc->addToPT( getDoc() ))) 
	{
		_cleanup();
		return ret;
	}

	_cleanup();

	UT_DEBUGMSG(("Finished loading OpenXML file\n\n\n\n"));

	return ret;

}

void IE_Imp_OpenXML::_cleanup ()
{
	OXMLi_PackageManager::destroyInstance();
	OXML_Document::destroyInstance();
}

