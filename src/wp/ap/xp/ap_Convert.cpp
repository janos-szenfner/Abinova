/* AbiWord
 * Copyright (C) 2000 AbiSource, Inc.
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

#include <stdio.h>
#include <string.h>
#include <string>
#include <map>

#include "ap_Convert.h"
#include "xap_App.h"
#include "ie_exp.h"
#include "ie_imp.h"
#include "ut_types.h"
#include "ut_string.h"
#include "ut_string_class.h"
#include "ut_misc.h"

#include "ut_assert.h"
#include "ut_debugmsg.h"

#include "gr_DrawArgs.h"
#include "gr_Graphics.h"
#include "fv_View.h"
#include "fl_BlockLayout.h"
#include "ap_EditMethods.h"

// needed for convertToPNG
#include "ie_impGraphic.h"
#include "ut_bytebuf.h"

//////////////////////////////////////////////////////////////////

AP_Convert::AP_Convert(int inVerbose)
	: m_iVerbose(inVerbose)
{
}

AP_Convert::~AP_Convert(void)
{
}

static IEFileType getImportFileType(const char * szSuffixOrMime)
{
  IEFileType ieft = IEFT_Unknown;

  if(szSuffixOrMime && *szSuffixOrMime) {
    IE_Imp::fileTypeForMimetype(szSuffixOrMime);
    if(ieft == IEFT_Unknown) {
      UT_String suffix;

      if(*szSuffixOrMime != '.')
	suffix = ".";
      suffix += szSuffixOrMime;
      ieft = IE_Imp::fileTypeForSuffix(suffix.c_str());
    }
  }

  return ieft;
}

static IEFileType getExportFileType(const char * szSuffixOrMime)
{
  IEFileType ieft = IEFT_Unknown;

  if(szSuffixOrMime && *szSuffixOrMime) {
    IE_Exp::fileTypeForMimetype(szSuffixOrMime);
    if(ieft == IEFT_Unknown) {
      UT_String suffix;

      if(*szSuffixOrMime != '.')
	suffix = ".";
      suffix += szSuffixOrMime;
      ieft = IE_Exp::fileTypeForSuffix(suffix.c_str());
    }
  }

  return ieft;
}

bool AP_Convert::convertTo(const char * szSourceFilename,
			   IEFileType sourceFormat,
			   const char * szTargetFilename,
			   IEFileType targetFormat)
{
	UT_Error error = UT_OK;

	UT_return_val_if_fail(targetFormat != IEFT_Unknown, false);
	UT_return_val_if_fail(szSourceFilename != nullptr, false);
	UT_return_val_if_fail(szTargetFilename != nullptr, false);

	PD_Document * pNewDoc = new PD_Document();
	UT_return_val_if_fail(pNewDoc, false);

	char * uri = UT_go_shell_arg_to_uri (szSourceFilename);
	error = pNewDoc->readFromFile(uri, sourceFormat, m_impProps.utf8_str());
	g_free (uri);

	if (!UT_IS_IE_SUCCESS(error)) {
		switch (error) {
		case UT_INVALIDFILENAME:
			if (m_iVerbose > 0)
				fprintf(stderr, "Abinova: [%s] is not a valid file name.\n", szSourceFilename);
			break;
		case UT_IE_NOMEMORY:
			if (m_iVerbose > 0)
				fprintf(stderr, "Abinova: Arrrgh... I don't have enough memory!\n");
			break;
		case UT_IE_PROTECTED:
			if (m_iVerbose > 0)
				fprintf(stderr, "Abinova: [%s] is password-protected and cannot be opened.\n", szSourceFilename);
			break;
		case UT_NOPIECETABLE:
			// TODO
		default:
			if (m_iVerbose > 0)
				fprintf(stderr, "Abinova: could not open the file [%s]\n", szSourceFilename);
		}

		UNREFP(pNewDoc);
		return (error == UT_OK);
	}

	uri = UT_go_shell_arg_to_uri (szTargetFilename);
	error = static_cast<AD_Document*>(pNewDoc)->saveAs(uri, targetFormat, m_expProps.utf8_str());
	g_free(uri);

	switch (error) {
	case UT_OK:
		if (m_iVerbose > 1)
			printf("Abinova: [%s] -> [%s]\tConversion ok!\n", szSourceFilename, szTargetFilename);
		break;
	case UT_SAVE_EXPORTERROR:
		if (m_iVerbose > 0)
			fprintf(stderr, "Abinova: Uch! Are you sure that you've specified a valid exporter?\n");
		break;
	case UT_SAVE_WRITEERROR:
		if (m_iVerbose > 0)
			fprintf(stderr, "Abinova: Uch! Could not write the file [%s]\n", szTargetFilename);
		break;
	default:
		if (m_iVerbose > 0)
			fprintf(stderr, "Abinova: could not write the file [%s]\n", szTargetFilename);
		break;
	}

	UNREFP(pNewDoc);

	return UT_IS_IE_SUCCESS(error);
}

bool AP_Convert::convertTo(const char * szFilename,
			   const char * szSourceSuffixOrMime,
			   const char * szTargetFilename,
			   const char * szTargetSuffixOrMime)
{
  return convertTo(szFilename, getImportFileType(szSourceSuffixOrMime), szTargetFilename, getExportFileType(szTargetSuffixOrMime));
}

bool AP_Convert::convertTo(const char * szFilename,
			   const char * szSourceSuffixOrMime,
			   const char * szTargetSuffixOrMime)
{
  UT_return_val_if_fail(szTargetSuffixOrMime, false);
  UT_return_val_if_fail(strlen(szTargetSuffixOrMime)>0, false);

  UT_String ext;
  IEFileType ieft = IEFT_Unknown;

  UT_String file;

  // maybe it is a mime type. try that first
  ieft = IE_Exp::fileTypeForMimetype(szTargetSuffixOrMime);
  if(ieft != IEFT_Unknown) {
    ext = IE_Exp::preferredSuffixForFileType(ieft).utf8_str();
  }
  else
    {
      std::string suffix = UT_pathSuffix(szTargetSuffixOrMime);
      if (!suffix.empty())
	{
	  // suffix is ".txt" or ".html"
	  ieft = IE_Exp::fileTypeForSuffix(suffix.c_str());

	  // szTargetSuffixOrMime is something like "file://home/dom/foo.html", so use it as our target filename
	  if (suffix.size() != strlen(szTargetSuffixOrMime))
	    file = szTargetSuffixOrMime;
	}
      else
	{
	  // assume that szSourceSuffixOrMime is "txt" or "html"
	  ext = ".";
	  ext += szTargetSuffixOrMime;
	  ieft = IE_Exp::fileTypeForSuffix(ext.c_str());
	}

      // unknown suffix and mime type
      if(ieft == IEFT_Unknown)
	return false;
    }

  if (file.empty())
    {
      char * fileDup = g_strdup ( szFilename );

      char *tmp = strrchr(fileDup, '.');
      if (tmp != nullptr)
	*tmp = '\0';

      file = fileDup;
      file += ext;

      FREEP(fileDup);
    }

  return convertTo(szFilename, getImportFileType(szSourceSuffixOrMime), file.c_str(), ieft);
}

void AP_Convert::setVerbose(int level)
{
	if ((level >= 0) && (level <= 2))
		m_iVerbose = level;
}

bool AP_Convert::print(const char * szFile, GR_Graphics * pGraphics, const char * szFileExtensionOrMime)
{
	// get the current document
	PD_Document *pDoc = new PD_Document();
	UT_Error err;
	char * uri = UT_go_shell_arg_to_uri (szFile);

	IEFileType ieft = getImportFileType(szFileExtensionOrMime);

	err = pDoc->readFromFile(uri, ieft, m_impProps.utf8_str());
	g_free(uri);

	if( err != UT_OK)
	{
		fprintf(stderr, "Abinova: Error importing file. [%s]  Could not print \n", szFile);
		UNREFP(pDoc);
		return false;
	}
	// create a new layout and view object for the doc
	FL_DocLayout *pDocLayout = new FL_DocLayout(pDoc,pGraphics);
	FV_View printView(XAP_App::getApp(), nullptr, pDocLayout);
	pDocLayout->setView (&printView);
	pDocLayout->fillLayouts();
	pDocLayout->formatAll();
	pDocLayout->recalculateTOCFields();

	bool bCollate = true;
	UT_sint32 nCopies = 1;
	std::set<UT_sint32> pages;

	std::map<std::string, std::string> props_map;
	UT_parse_properties(m_expProps.utf8_str(), props_map);

	if (props_map.find("collate") != props_map.end())
	  {
	    bCollate = UT_parseBool(props_map["collate"].c_str(), true);
	  }

	if (props_map.find("copies") != props_map.end())
	  {
	    nCopies = atoi(props_map["copies"].c_str());
	    if (nCopies <= 0)
	      nCopies = 1;
	  }

	if (props_map.find("pages") != props_map.end())
	  {
	    char **page_descriptions;

	    page_descriptions = g_strsplit(props_map["pages"].c_str(), ",", -1);

	    int i = 0;
	    while (page_descriptions[i] != nullptr)
	      {
		char *description = page_descriptions[i];
		i++;

		int start_page, end_page;

		if (2 == sscanf(description, "%d-%d", &start_page, &end_page))
		  {
		  }
		else if (1 == sscanf(description, "%d", &start_page))
		  {
		    end_page = start_page;
		  }
		else
		  {
		    // invalid page specification
		    continue;
		  }

		for (int pageno = start_page; pageno <= end_page; pageno++)
		  {
		    if ((pageno > 0) && (pageno <= static_cast<int>(pDocLayout->countPages())))
		      pages.insert(pageno);
		  }
	      }

	    g_strfreev(page_descriptions);
	  }

	if (pages.empty())
	  {
	    for (UT_sint32 i = 1; i <= pDocLayout->countPages(); i++)
	      {
		pages.insert(i);
	      }
	  }

	if(!s_actuallyPrint (pDoc, pGraphics,
			     &printView, szFile,
			     nCopies, bCollate,
			     pDocLayout->getWidth(), pDocLayout->getHeight() / pDocLayout->countPages(),
			     pages))
	  err = UT_SAVE_WRITEERROR;

	DELETEP(pDocLayout);

	UNREFP(pDoc);

	return (err == UT_OK);
}


bool AP_Convert::printFirstPage(GR_Graphics * pGraphics,PD_Document * pDoc)
{
		// create a new layout and view object for the doc

	FL_DocLayout *pDocLayout = new FL_DocLayout(pDoc,pGraphics);
	FV_View printView(XAP_App::getApp(), nullptr, pDocLayout);
	pDocLayout->setView (&printView);
	pDocLayout->fillLayouts();
	pDocLayout->formatAll();

	bool success = s_actuallyPrint (pDoc, pGraphics,
					&printView, "pngThumb",
					1, true,
					pDocLayout->getWidth(), pDocLayout->getHeight() / pDocLayout->countPages(),
					1, 1);

	DELETEP(pDocLayout);

	return success;
}
