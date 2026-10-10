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

#ifndef _OXML_DOCUMENT_H_
#define _OXML_DOCUMENT_H_

// Internal includes
#include "OXML_Types.h"
#include "OXML_ObjectWithAttrProp.h"
#include "OXML_Section.h"
#include "OXML_Style.h"
#include "OXML_Theme.h"
#include "OXML_FontManager.h"

// Abinova includes
#include "ut_types.h"
#include "pd_Document.h"
#include "ie_exp_OpenXML.h"

// External includes
#include <map>
#include <string>
#include <vector>

/* \class OXML_Document
 * This class represents the data model representation of the OpenXML document.
 * Since there is only one document to be imported / exported at a time, this is
 * enforced by the use of the Singleton pattern.
 */
class OXML_Document : public OXML_ObjectWithAttrProp
{
public:
	//! Clears any previous document instance and provides a new blank one.
	static OXML_Document* getNewInstance();
	//! Provides a reference to the current OpenXML document.
	static OXML_Document* getInstance();
	//! Frees the current document and all its content from memory.
	static void destroyInstance();
	/* Nested-import guard: a chunk part (w:altChunk) may itself be an
	 * OpenXML package, and importing it builds its own OXML_Document
	 * which would clobber the outer document's data model mid-addToPT.
	 * detachInstance() hands the current instance out of the slot so
	 * the nested import gets a fresh one; restoreInstance() puts it
	 * back, destroying whatever the nested import left installed. */
	static OXML_Document* detachInstance();
	static void restoreInstance(OXML_Document* inst);

	//! Returns a reference to the FIRST style with corresponding ID OR empty SharedStyle if none found.
	OXML_SharedStyle getStyleById(const std::string & id) const;
	//! Returns a reference to the FIRST style with corresponding name OR empty SharedStyle if none found.
	OXML_SharedStyle getStyleByName(const std::string & name) const;
	UT_Error addStyle(const std::string & id, const std::string & name, const gchar ** attributes);
	UT_Error addStyle(const OXML_SharedStyle & obj);
	UT_Error clearStyles();

	UT_Error addList(const OXML_SharedList& obj);
	UT_Error addImage(const OXML_SharedImage& obj);
	OXML_SharedList getListById(UT_uint32 id) const;
	OXML_SharedImage getImageById(const std::string & id) const;

	//! Returns a reference to the FIRST footnote with corresponding ID OR empty SharedSection if none found.
	OXML_SharedSection getFootnote(const std::string & id) const;
	UT_Error addFootnote(const OXML_SharedSection & obj);
	UT_Error clearFootnotes();

	OXML_SharedSection getEndnote(const std::string & id) const;
	UT_Error addEndnote(const OXML_SharedSection & obj);
	UT_Error clearEndnotes();

	//! Comment bodies from word/comments.xml, keyed by comment id.
	OXML_SharedSection getAnnotation(const std::string & id) const;
	UT_Error addAnnotation(const OXML_SharedSection & obj);
	UT_Error clearAnnotations();

	//! Returns a reference to the FIRST header with corresponding ID OR empty SharedSection if none found.
	OXML_SharedSection getHeader(const std::string & id) const;
	UT_Error addHeader(const OXML_SharedSection & obj);
	UT_Error clearHeaders();

	bool isAllDefault(const bool & header) const;
	OXML_SharedSection getHdrFtrById(const bool & header, const std::string & id) const;

	//! Returns a reference to the FIRST footer with corresponding ID OR nullptr if none found.
	OXML_SharedSection getFooter(const std::string & id) const;
	UT_Error addFooter(const OXML_SharedSection & obj);
	UT_Error clearFooters();

	//! Retrieves the last appended section of the document OR empty SharedSection if no sections have been appended.
	OXML_SharedSection getLastSection() const;
	//! Appends a new section at the end of the list.
	UT_Error appendSection(const OXML_SharedSection & obj);
	UT_Error clearSections();

	OXML_SharedTheme getTheme();
	OXML_SharedFontManager getFontManager();

	//! Writes the OpenXML document and all its content to a file on disk.
	/*! This method is used during the export process.
		\param exporter the actual exporter which handles writing the files.
	*/
	UT_Error serialize(IE_Exp_OpenXML* exporter);
	//! Builds the Abiword Piecetable representation of the OpenXML document and all its content.
	/*! This method is used during the import process.
		\param pDocument A valid reference to the PD_Document object.
	*/
	UT_Error addToPT(PD_Document * pDocument);

	std::string getMappedNumberingId(const std::string & numId) const;
	bool setMappedNumberingId(const std::string & numId, const std::string & abstractNumId);

	std::string getBookmarkName(const std::string & bookmarkId) const;
	std::string getBookmarkId(const std::string & bookmarkName) const;
	bool setBookmarkName(const std::string & bookmarkId, const std::string & bookmarkName);

	void setPageWidth(const std::string & width);
	void setPageHeight(const std::string & height);
	const std::string & getPageWidth() const { return m_pageWidth; }
	const std::string & getPageHeight() const { return m_pageHeight; }
	void setPageOrientation(const std::string & orientation);
	void setPageMargins(const std::string & top, const std::string & left, const std::string & right, const std::string & bottom);
	const std::string & getPageMarginTop() const { return m_pageMarginTop; }
	const std::string & getPageMarginLeft() const { return m_pageMarginLeft; }
	const std::string & getPageMarginRight() const { return m_pageMarginRight; }
	const std::string & getPageMarginBottom() const { return m_pageMarginBottom; }
	void setColumns(const std::string & colNum, const std::string & colSep);

	//! Document-level settings properties (settings.xml footnotePr etc.)
	void setDocProperty(const std::string & name, const std::string & val)
		{ m_docProps[name] = val; }
	bool getDocProperty(const std::string & name, std::string & val) const
	{
		std::map<std::string, std::string>::const_iterator it =
			m_docProps.find(name);
		if (it == m_docProps.end())
			return false;
		val = it->second;
		return true;
	}

	/* Import bookkeeping for TOC complex fields: the importer flags
	 * the field's result paragraphs so OXML_Section::addToPT can wrap
	 * the run in a TOC strux; the first paragraph carries the field
	 * instruction. */
	void markTOCParagraph(const OXML_Element* pPara, const std::string & instr)
		{ m_tocParagraphs[pPara] = instr; }
	bool isTOCParagraph(const OXML_Element* pPara) const
		{ return m_tocParagraphs.find(pPara) != m_tocParagraphs.end(); }
	std::string getTOCInstr(const OXML_Element* pPara) const
		{ auto it = m_tocParagraphs.find(pPara);
		  return it != m_tocParagraphs.end() ? it->second : ""; }

	/* Unsupported drawing payloads (chart/SmartArt/OLE parts that
	 * cannot be rendered): the Valid listener records the
	 * relationship references it swallows inside rejected mc:Choice
	 * branches so the Image listener can move them onto the imported
	 * fallback picture — keeps the part link alive for round-trips. */
	void noteDroppedObjectUri(const std::string & uri)
		{ m_droppedObjUri = uri; }
	void noteDroppedObjectRel(const std::string & attr,
							  const std::string & rid)
		{ m_droppedObjRels.push_back(attr + "=" + rid); }
	std::string takeDroppedObjectUri()
		{ std::string u; u.swap(m_droppedObjUri); return u; }
	std::vector<std::string> takeDroppedObjectRels()
		{ std::vector<std::string> r; r.swap(m_droppedObjRels);
		  return r; }
	void clearDroppedObject()
		{ m_droppedObjUri.clear(); m_droppedObjRels.clear(); }

private:
	static OXML_Document* s_docInst;
	OXML_Document();
	virtual ~OXML_Document();

	OXML_SectionVector m_sections;

	OXML_SectionMap m_headers;
	OXML_SectionMap m_footers;
	OXML_SectionMap m_footnotes;
	OXML_SectionMap m_endnotes;
	OXML_SectionMap m_annotations;

	OXML_StyleMap m_styles_by_id;
	OXML_StyleMap m_styles_by_name;

	OXML_SharedTheme m_theme;
	OXML_SharedFontManager m_fontManager;

	OXML_ListMap m_lists_by_id;
	OXML_ImageMap m_images_by_id;

	std::map<std::string, std::string> m_numberingMap;
	std::map<std::string, std::string> m_bookmarkMap;

	std::string m_pageWidth;
	std::string m_pageHeight;
	std::string m_pageOrientation;

	std::map<std::string, std::string> m_docProps;

	std::map<const OXML_Element*, std::string> m_tocParagraphs;

	std::string m_droppedObjUri;
	std::vector<std::string> m_droppedObjRels;

	std::string m_pageMarginTop;
	std::string m_pageMarginLeft;
	std::string m_pageMarginRight;
	std::string m_pageMarginBottom;

	std::string m_colNum;
	std::string m_colSep;

	void _assignHdrFtrIds();
	UT_Error applyPageProps(PD_Document* pDocument);
};

#endif //_OXML_DOCUMENT_H_

