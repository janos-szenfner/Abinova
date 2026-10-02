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
		std::string contextTag = OXMLi_contextBack(rqst->context);
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
	else if (nameMatches(rqst->pName, NS_W_KEY, "clrSchemeMapping")) {
		/* theme color slot mapping, packed "bg1=lt1;tx1=dk1;..." */
		OXML_Document * doc = OXML_Document::getInstance();
		UT_return_if_fail( this->_error_if_fail(doc != nullptr) );
		static const char * slots[] = {
			"bg1", "tx1", "bg2", "tx2", "accent1", "accent2", "accent3",
			"accent4", "accent5", "accent6", "hlink", "folHlink", nullptr };
		std::string map;
		for (int i = 0; slots[i]; i++) {
			const gchar * v = attrMatches(NS_W_KEY, slots[i], rqst->ppAtts);
			if (v && *v) {
				map += slots[i];
				map += "=";
				map += v;
				map += ";";
			}
		}
		if (!map.empty())
			doc->setDocProperty("document-clr-scheme-mapping", map);
		rqst->handled = true;
	}
	else if (nameMatches(rqst->pName, NS_W_KEY, "documentProtection")) {
		OXML_Document * doc = OXML_Document::getInstance();
		UT_return_if_fail( this->_error_if_fail(doc != nullptr) );
		const gchar * edit = attrMatches(NS_W_KEY, "edit", rqst->ppAtts);
		const gchar * enf = attrMatches(NS_W_KEY, "enforcement", rqst->ppAtts);
		bool en = !enf || !*enf || !strcmp(enf, "1") ||
			!strcmp(enf, "true") || !strcmp(enf, "on");
		if (edit && *edit) {
			doc->setDocProperty("document-protection-mode", edit);
			doc->setDocProperty("document-protected",
				(en && strcmp(edit, "none")) ? "1" : "0");
		}
		rqst->handled = true;
	}
	else if (nameMatches(rqst->pName, NS_W_KEY, "writeProtection")) {
		OXML_Document * doc = OXML_Document::getInstance();
		if (doc)
			doc->setDocProperty("document-protected", "1");
		rqst->handled = true;
	}
	else if (nameMatches(rqst->pName, NS_W_KEY, "zoom")) {
		const gchar * pct = attrMatches(NS_W_KEY, "percent", rqst->ppAtts);
		OXML_Document * doc = OXML_Document::getInstance();
		if (doc && pct && *pct)
			doc->setDocProperty("document-zoom", pct);
		rqst->handled = true;
	}
	else if (nameMatches(rqst->pName, NS_W_KEY, "defaultTabStop") ||
			 nameMatches(rqst->pName, NS_W_KEY, "hyphenationZone")) {
		const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
		OXML_Document * doc = OXML_Document::getInstance();
		if (doc && val && *val) {
			std::string pt(_TwipsToPoints(val));
			pt += "pt";
			doc->setDocProperty(
				nameMatches(rqst->pName, NS_W_KEY, "defaultTabStop") ?
					"document-default-tab-stop" : "document-hyphenation-zone",
				pt);
		}
		rqst->handled = true;
	}
	else if (nameMatches(rqst->pName, NS_W_KEY, "decimalSymbol") ||
			 nameMatches(rqst->pName, NS_W_KEY, "listSeparator") ||
			 nameMatches(rqst->pName, NS_W_KEY, "consecutiveHyphenLimit")) {
		const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
		OXML_Document * doc = OXML_Document::getInstance();
		if (doc && val && *val) {
			doc->setDocProperty(
				nameMatches(rqst->pName, NS_W_KEY, "decimalSymbol") ?
					"document-decimal-symbol" :
				nameMatches(rqst->pName, NS_W_KEY, "listSeparator") ?
					"document-list-separator" :
					"document-consecutive-hyphen-limit",
				val);
		}
		rqst->handled = true;
	}
	else if (nameMatches(rqst->pName, NS_W_KEY, "trackChanges") ||
			 nameMatches(rqst->pName, NS_W_KEY, "evenAndOddHeaders") ||
			 nameMatches(rqst->pName, NS_W_KEY, "mirrorMargins") ||
			 nameMatches(rqst->pName, NS_W_KEY, "gutterAtTop") ||
			 nameMatches(rqst->pName, NS_W_KEY, "autoHyphenation") ||
			 nameMatches(rqst->pName, NS_W_KEY, "doNotTrackMoves") ||
			 nameMatches(rqst->pName, NS_W_KEY, "doNotTrackFormatting") ||
			 nameMatches(rqst->pName, NS_W_KEY, "bookFoldPrinting") ||
			 nameMatches(rqst->pName, NS_W_KEY, "bookFoldRevPrinting") ||
			 nameMatches(rqst->pName, NS_W_KEY, "removeDateAndTime") ||
			 nameMatches(rqst->pName, NS_W_KEY, "removePersonalInformation")) {
		/* settings.xml on/off switches -> document-* props */
		static const struct { const char * e; const char * p; } m[] = {
			{"trackChanges",             "document-track-changes"},
			{"evenAndOddHeaders",        "document-even-odd-headers"},
			{"mirrorMargins",            "document-mirror-margins"},
			{"gutterAtTop",              "document-gutter-at-top"},
			{"autoHyphenation",          "document-auto-hyphenation"},
			{"doNotTrackMoves",          "document-do-not-track-moves"},
			{"doNotTrackFormatting",     "document-do-not-track-formatting"},
			{"bookFoldPrinting",         "document-book-fold-printing"},
			{"bookFoldRevPrinting",      "document-book-fold-rev"},
			{"removeDateAndTime",        "document-remove-date-info"},
			{"removePersonalInformation","document-remove-personal-info"} };
		const gchar * val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
		bool bOn = !val || !*val || !strcmp(val, "true") ||
			!strcmp(val, "1") || !strcmp(val, "on");
		OXML_Document * doc = OXML_Document::getInstance();
		if (doc) {
			for (auto & e : m) {
				if (nameMatches(rqst->pName, NS_W_KEY, e.e)) {
					doc->setDocProperty(e.p, bOn ? "1" : "0");
					break;
				}
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
		nameMatches(rqst->pName, NS_W_KEY, "pos") ||
		nameMatches(rqst->pName, NS_W_KEY, "clrSchemeMapping") ||
		nameMatches(rqst->pName, NS_W_KEY, "documentProtection") ||
		nameMatches(rqst->pName, NS_W_KEY, "writeProtection") ||
		nameMatches(rqst->pName, NS_W_KEY, "zoom") ||
		nameMatches(rqst->pName, NS_W_KEY, "defaultTabStop") ||
		nameMatches(rqst->pName, NS_W_KEY, "hyphenationZone") ||
		nameMatches(rqst->pName, NS_W_KEY, "decimalSymbol") ||
		nameMatches(rqst->pName, NS_W_KEY, "listSeparator") ||
		nameMatches(rqst->pName, NS_W_KEY, "consecutiveHyphenLimit") ||
		nameMatches(rqst->pName, NS_W_KEY, "trackChanges") ||
		nameMatches(rqst->pName, NS_W_KEY, "evenAndOddHeaders") ||
		nameMatches(rqst->pName, NS_W_KEY, "mirrorMargins") ||
		nameMatches(rqst->pName, NS_W_KEY, "gutterAtTop") ||
		nameMatches(rqst->pName, NS_W_KEY, "autoHyphenation") ||
		nameMatches(rqst->pName, NS_W_KEY, "doNotTrackMoves") ||
		nameMatches(rqst->pName, NS_W_KEY, "doNotTrackFormatting") ||
		nameMatches(rqst->pName, NS_W_KEY, "bookFoldPrinting") ||
		nameMatches(rqst->pName, NS_W_KEY, "bookFoldRevPrinting") ||
		nameMatches(rqst->pName, NS_W_KEY, "removeDateAndTime") ||
		nameMatches(rqst->pName, NS_W_KEY, "removePersonalInformation")) {
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

