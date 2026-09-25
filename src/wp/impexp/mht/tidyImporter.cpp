/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* Abinova: tidyImporter - plugin for Multipart [X]HTML
 * 
 * Copyright (C) 2002-2003 Francis James Franklin <fjf@alinameridon.com>
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

#include <string>

#include <gsf/gsf.h>

#include "tidyReader.h"
#include "tidyImporter.h"
#include "ie_impexp_HTML.h"

IE_Imp_Tidy_Sniffer::IE_Imp_Tidy_Sniffer ()
#ifdef XHTML_NAMED_CONSTRUCTORS
	: IE_ImpSniffer("AbiXHTML::HTML Tidy")
#endif
{
	// 
}

#ifdef XHTML_NAMED_CONSTRUCTORS

// supported mimetypes
static IE_MimeConfidence IE_Imp_Tidy_Sniffer__MimeConfidence[] = {
	{ IE_MIME_MATCH_FULL, 	IE_MIMETYPE_HTML, 	UT_CONFIDENCE_GOOD 	},
	{ IE_MIME_MATCH_BOGUS, 	"", 				UT_CONFIDENCE_ZILCH }
};

const IE_MimeConfidence * IE_Imp_Tidy_Sniffer::getMimeConfidence ()
{
	return IE_Imp_Tidy_Sniffer__MimeConfidence;
}

#endif /* XHTML_NAMED_CONSTRUCTORS */

UT_Confidence_t IE_Imp_Tidy_Sniffer::recognizeContents (const char * /*szBuf*/, UT_uint32 /*iNumbytes*/)
{
	return UT_CONFIDENCE_ZILCH;
}

UT_Confidence_t IE_Imp_Tidy_Sniffer::recognizeSuffix (const char * szSuffix)
{
	if (!(g_ascii_strcasecmp(szSuffix,".html")) || !(g_ascii_strcasecmp(szSuffix,".htm")))
		return UT_CONFIDENCE_GOOD;
	return UT_CONFIDENCE_ZILCH;
}

UT_Error IE_Imp_Tidy_Sniffer::constructImporter (PD_Document * pDocument, IE_Imp ** ppie)
{
	IE_Imp_XHTML * p = new IE_Imp_HTML(pDocument);
	*ppie = p;
	return UT_OK;
}

bool IE_Imp_Tidy_Sniffer::getDlgLabels (const char ** pszDesc,
										const char ** pszSuffixList,
										IEFileType * ft)
{
	*pszDesc = "HTML [via tidy] (.html, .htm)";
	*pszSuffixList = "*.html; *.htm";
	*ft = getFileType();
	return true;
}

IE_Imp_HTML::IE_Imp_HTML (PD_Document * pDocument) :
	IE_Imp_XHTML(pDocument)
{
	// 
}

IE_Imp_HTML::~IE_Imp_HTML ()
{
	// 
}

UT_Error IE_Imp_HTML::_loadFile (GsfInput * input)
{
	// IE_Imp_XML::importFile(data,length) parses the buffer directly and
	// does not use a UT_XML::Reader, so run libtidy over the input first
	// and feed the tidied XHTML to the importer.
	size_t num_bytes = gsf_input_size (input);
	const guint8 * bytes = gsf_input_read (input, num_bytes, nullptr);
	if (!bytes) return UT_ERROR;

	TidyReader reader (bytes, static_cast<UT_uint32>(num_bytes));
	if (!reader.openFile (gsf_input_name (input))) return UT_IE_IMPORTERROR;

	std::string tidied;
	char chunk[4096];
	UT_uint32 n;
	while ((n = reader.readBytes (chunk, sizeof (chunk))) > 0)
		tidied.append (chunk, n);
	reader.closeFile ();

	if (tidied.empty ()) return UT_IE_IMPORTERROR;

	return IE_Imp_XHTML::importFile (tidied.c_str(), static_cast<UT_uint32>(tidied.size()));
}
