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
#include "OXMLi_ListenerState_DocSettings.h"

// Internal includes
#include "OXML_Document.h"
#include "OXML_FontManager.h"
#include "OXML_Types.h"
#include "OXML_LangToScriptConverter.h"

// Abinova includes
#include "ut_assert.h"
#include "ut_misc.h"

// External includes
#include <string>

void OXMLi_ListenerState_DocSettings::startElement (OXMLi_StartElementRequest * rqst)
{
	if (nameMatches(rqst->pName, NS_W_KEY, "themeFontLang")) {
		const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
		const gchar * eastAsia = attrMatches(NS_W_KEY, "eastAsia", rqst->ppAtts);
		const gchar * bidi = attrMatches(NS_W_KEY, "bidi", rqst->ppAtts);

		OXML_Document * doc = OXML_Document::getInstance();
		UT_return_if_fail( this->_error_if_fail(doc != nullptr) );
		OXML_SharedFontManager fmgr = doc->getFontManager();
		UT_return_if_fail( this->_error_if_fail(fmgr.get() != nullptr) );

		if (val != nullptr) {
			std::string val_str = _convert_ST_LANG(val);
			fmgr->mapRangeToScript(ASCII_RANGE, val_str);
			fmgr->mapRangeToScript(HANSI_RANGE, val_str);
		}
		if (eastAsia != nullptr) {
			std::string eastAsia_str = _convert_ST_LANG(eastAsia);
			fmgr->mapRangeToScript(EASTASIAN_RANGE, eastAsia_str);
		}
		if (bidi != nullptr) {
			std::string bidi_str = _convert_ST_LANG(bidi);
			fmgr->mapRangeToScript(COMPLEX_RANGE, bidi_str);
		}

		rqst->handled = true;
	}
	else if (nameMatches(rqst->pName, NS_W_KEY, "footnotePr") ||
			 nameMatches(rqst->pName, NS_W_KEY, "endnotePr") ||
			 nameMatches(rqst->pName, NS_W_KEY, "footnote") ||
			 nameMatches(rqst->pName, NS_W_KEY, "endnote")) {
		rqst->handled = true;
	}
	else if (nameMatches(rqst->pName, NS_W_KEY, "numFmt") ||
			 nameMatches(rqst->pName, NS_W_KEY, "numStart") ||
			 nameMatches(rqst->pName, NS_W_KEY, "numRestart") ||
			 nameMatches(rqst->pName, NS_W_KEY, "pos")) {
		if (rqst->context == nullptr || rqst->context->empty()) {
			return;
		}
		std::string contextTag = rqst->context->back();
		bool foot = contextMatches(contextTag, NS_W_KEY, "footnotePr");
		bool endn = contextMatches(contextTag, NS_W_KEY, "endnotePr");
		if (!foot && !endn) {
			return;
		}
		const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
		if (val == nullptr || val[0] == '\0') {
			rqst->handled = true;
			return;
		}
		OXML_Document * doc = OXML_Document::getInstance();
		UT_return_if_fail( this->_error_if_fail(doc != nullptr) );

		if (nameMatches(rqst->pName, NS_W_KEY, "numFmt")) {
			std::string mapped = _numFmtToType(val);
			if (!mapped.empty()) {
				doc->setDocProperty(foot ? "document-footnote-type"
										 : "document-endnote-type",
									mapped);
			}
		}
		else if (nameMatches(rqst->pName, NS_W_KEY, "numStart")) {
			doc->setDocProperty(foot ? "document-footnote-initial"
									 : "document-endnote-initial",
								val);
		}
		else if (nameMatches(rqst->pName, NS_W_KEY, "numRestart")) {
			if (strcmp(val, "eachSect") == 0) {
				doc->setDocProperty(foot ? "document-footnote-restart-section"
										 : "document-endnote-restart-section",
									"1");
			}
			else if (foot && strcmp(val, "eachPage") == 0) {
				doc->setDocProperty("document-footnote-restart-page", "1");
			}
		}
		else if (endn && nameMatches(rqst->pName, NS_W_KEY, "pos")) {
			if (strcmp(val, "sectEnd") == 0) {
				doc->setDocProperty("document-endnote-place-endsection", "1");
			}
			else if (strcmp(val, "docEnd") == 0) {
				doc->setDocProperty("document-endnote-place-enddoc", "1");
			}
		}
		rqst->handled = true;
	}
}

void OXMLi_ListenerState_DocSettings::endElement (OXMLi_EndElementRequest * rqst)
{
	if (nameMatches(rqst->pName, NS_W_KEY, "themeFontLang") ||
		nameMatches(rqst->pName, NS_W_KEY, "footnotePr") ||
		nameMatches(rqst->pName, NS_W_KEY, "endnotePr") ||
		nameMatches(rqst->pName, NS_W_KEY, "footnote") ||
		nameMatches(rqst->pName, NS_W_KEY, "endnote") ||
		nameMatches(rqst->pName, NS_W_KEY, "numFmt") ||
		nameMatches(rqst->pName, NS_W_KEY, "numStart") ||
		nameMatches(rqst->pName, NS_W_KEY, "numRestart") ||
		nameMatches(rqst->pName, NS_W_KEY, "pos")) {
		rqst->handled = true;
	}
}

void OXMLi_ListenerState_DocSettings::charData (OXMLi_CharDataRequest * /*rqst*/)
{
	//don't do anything here
}

std::string OXMLi_ListenerState_DocSettings::_convert_ST_LANG(std::string code_in)
{
	//The input value is of the following format:
	//	An ISO 639-1 letter code plus a dash plus an ISO 3166-1 alpha-2 letter code
	//	OR an hexadecimal language code (see ST_LangCode)
	//The return value is of the following format:
	//	An ISO 15924 alpha-4 letter code

	OXML_LangScriptAsso * asso = nullptr;
	OXML_LangToScriptConverter conv;
	std::string substr = code_in.substr(0,2);
	asso = conv.in_word_set(substr.data(), substr.length());
	if (asso != nullptr) {
		return asso->script;
	} else {
		return code_in;
	}
}

//Maps an ST_NumberFormat value (footnote/endnote numFmt) to an Abinova
//footnote-type property value. Returns an empty string when unmappable.
std::string OXMLi_ListenerState_DocSettings::_numFmtToType(const std::string & fmt)
{
	if (fmt == "decimal" || fmt == "decimalFullWidth" || fmt == "chicago") {
		return "numeric";
	}
	else if (fmt == "lowerLetter") {
		return "lower";
	}
	else if (fmt == "upperLetter") {
		return "upper";
	}
	else if (fmt == "lowerRoman") {
		return "lower-roman";
	}
	else if (fmt == "upperRoman") {
		return "upper-roman";
	}
	return "";
}

