/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* Abinova
 * Copyright (C) 2001 AbiSource, Inc.
 * Copyright (C) 2002-2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2001-2003 William Lachance (william.lachance@sympatico.ca)
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

/* See bug 1764
 * This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#ifndef IE_IMP_WP_H
#define IE_IMP_WP_H

#include <stdio.h>
#include <memory>
#include <string>
#include <vector>
#include "ut_compiler.h"
ABI_W_NO_SUGGEST_OVERRIDE
#include <librevenge/librevenge.h>
ABI_W_POP
#include "ie_imp.h"
#include "ut_string.h"
#include "ut_string_class.h"
#include "ut_growbuf.h"
#include "ut_mbtowc.h"
#include "pd_Document.h"
#include "fl_AutoNum.h"
#include "fl_TableLayout.h"
#include "fp_types.h"

using namespace std;

#define WP6_NUM_LIST_LEVELS 8  // see WP6FileStructure.h

// ABI_ListDefinition: tracks information on the list
class ABI_ListDefinition
{
public:
    ABI_ListDefinition(int iOutlineHash);
    void setListID(const int iLevel, const UT_uint32 iID) { m_iListIDs[iLevel-1] = iID; }
    UT_uint32 getListID(const int iLevel) const { return m_iListIDs[iLevel-1]; }
    FL_ListType getListType(const int iLevel) const { return m_listTypes[iLevel-1]; }
    void setListType(const int iLevel, const char type);
    void incrementLevelNumber(const int iLevel) { m_iListNumbers[iLevel - 1]++; }
    void setLevelNumber(const int iLevel, const int iNumber) { m_iListNumbers[iLevel - 1] = iNumber; }
    void setListLeftOffset(const int iLevel, const float listLeftOffset) { m_listLeftOffset[iLevel - 1] = listLeftOffset; }
    void setListMinLabelWidth(const int iLevel, const float listMinLabelWidth) { m_listMinLabelWidth[iLevel - 1] = listMinLabelWidth; }
    int getLevelNumber(const int iLevel) const { return m_iListNumbers[iLevel - 1]; }
    float getListLeftOffset(const int iLevel) const { return m_listLeftOffset[iLevel - 1]; }
    float getListMinLabelWidth(const int iLevel) const { return m_listMinLabelWidth[iLevel - 1]; }
    int getOutlineHash() const { return m_iOutlineHash; }

private:
    //int m_iWPOutlineHash; // we don't use this information in Abinova, only for id purposes during filtering
    UT_uint32 m_iListIDs[WP6_NUM_LIST_LEVELS];
    int m_iListNumbers[WP6_NUM_LIST_LEVELS];
    FL_ListType m_listTypes[WP6_NUM_LIST_LEVELS];
    float m_listLeftOffset[WP6_NUM_LIST_LEVELS];
    float m_listMinLabelWidth[WP6_NUM_LIST_LEVELS];
    int m_iOutlineHash;
};

class IE_Imp_WordPerfect_Sniffer final : public IE_ImpSniffer
{
    friend class IE_Imp;
    friend class IE_Imp_WordPerfect;

public:
    IE_Imp_WordPerfect_Sniffer();
    virtual ~IE_Imp_WordPerfect_Sniffer();

	virtual const IE_SuffixConfidence * getSuffixConfidence() override;
	using IE_ImpSniffer::recognizeContents;
	virtual UT_Confidence_t recognizeContents(GsfInput * input) override;
	virtual const IE_MimeConfidence * getMimeConfidence() override { return nullptr; }
	virtual bool getDlgLabels(const char ** szDesc,
			       const char ** szSuffixList,
			       IEFileType * ft) override;
	virtual UT_Error constructImporter(PD_Document * pDocument,
					IE_Imp ** ppie) override;
};

class IE_Imp_WordPerfect : public IE_Imp, public librevenge::RVNGTextInterface
{
public:
    IE_Imp_WordPerfect(PD_Document * pDocument);
    virtual ~IE_Imp_WordPerfect();

    virtual bool pasteFromBuffer(PD_DocumentRange * pDocRange,
				 const UT_uint8 * pData, UT_uint32 lenData, const char * szEncoding = nullptr) override;

	virtual void setDocumentMetaData(const librevenge::RVNGPropertyList &propList) override;

	virtual void startDocument(const librevenge::RVNGPropertyList &propList) override;
	virtual void endDocument() override;

	virtual void defineEmbeddedFont(const librevenge::RVNGPropertyList & /* propList */) override {}

	virtual void definePageStyle(const librevenge::RVNGPropertyList &) override {}
	virtual void openPageSpan(const librevenge::RVNGPropertyList &propList) override;
	virtual void closePageSpan() override;
	virtual void openHeader(const librevenge::RVNGPropertyList &propList) override;
	virtual void closeHeader() override;
	virtual void openFooter(const librevenge::RVNGPropertyList &propList) override;
	virtual void closeFooter() override;

	virtual void defineSectionStyle(const librevenge::RVNGPropertyList &) override {}
	virtual void openSection(const librevenge::RVNGPropertyList &propList) override;
	virtual void closeSection() override {}

	virtual void defineParagraphStyle(const librevenge::RVNGPropertyList &) override {}
	virtual void openParagraph(const librevenge::RVNGPropertyList &propList) override;
	virtual void closeParagraph() override {}

	virtual void defineCharacterStyle(const librevenge::RVNGPropertyList &) override {}
	virtual void openSpan(const librevenge::RVNGPropertyList &propList) override;
	virtual void closeSpan() override {}

	virtual void openLink(const librevenge::RVNGPropertyList &propList) override;
	virtual void closeLink() override;

	virtual void insertTab() override;
	virtual void insertText(const librevenge::RVNGString &text) override;
	virtual void insertSpace() override;
	virtual void insertLineBreak() override;
	virtual void insertField(const librevenge::RVNGPropertyList &propList) override;

	virtual void openOrderedListLevel(const librevenge::RVNGPropertyList &propList) override;
	virtual void openUnorderedListLevel(const librevenge::RVNGPropertyList &propList) override;
	virtual void closeOrderedListLevel() override;
	virtual void closeUnorderedListLevel() override;
	virtual void openListElement(const librevenge::RVNGPropertyList &propList) override;
	virtual void closeListElement() override {}

	virtual void openFootnote(const librevenge::RVNGPropertyList &propList) override;
	virtual void closeFootnote() override;
	virtual void openEndnote(const librevenge::RVNGPropertyList &propList) override;
	virtual void closeEndnote() override;
	virtual void openComment(const librevenge::RVNGPropertyList &propList) override;
	virtual void closeComment() override;
	virtual void openTextBox(const librevenge::RVNGPropertyList &propList) override;
	virtual void closeTextBox() override;

	virtual void openTable(const librevenge::RVNGPropertyList &propList) override;
	virtual void openTableRow(const librevenge::RVNGPropertyList &propList) override;
	virtual void closeTableRow() override {}
	virtual void openTableCell(const librevenge::RVNGPropertyList &propList) override;
	virtual void closeTableCell() override {}
	virtual void insertCoveredTableCell(const librevenge::RVNGPropertyList & /* propList */) override {}
	virtual void closeTable() override;

	virtual void openFrame(const librevenge::RVNGPropertyList &propList) override;
	virtual void closeFrame() override;

	virtual void openGroup(const librevenge::RVNGPropertyList & /* propList */) override {}
	virtual void closeGroup() override {}

	virtual void defineGraphicStyle(const librevenge::RVNGPropertyList & /* propList */) override {}
	virtual void drawRectangle(const librevenge::RVNGPropertyList &propList) override;
	virtual void drawEllipse(const librevenge::RVNGPropertyList &propList) override;
	virtual void drawPolygon(const librevenge::RVNGPropertyList &propList) override;
	virtual void drawPolyline(const librevenge::RVNGPropertyList &propList) override;
	virtual void drawPath(const librevenge::RVNGPropertyList &propList) override;
	virtual void drawConnector(const librevenge::RVNGPropertyList &propList) override;

	virtual void insertBinaryObject(const librevenge::RVNGPropertyList &propList) override;
	virtual void insertEquation(const librevenge::RVNGPropertyList &propList) override;

protected:
	virtual UT_Error _loadFile(GsfInput * input) override;
	using IE_Imp::appendObject;
	using IE_Imp::appendSpan;
	using IE_Imp::appendFmt;
	virtual bool appendStrux(PTStruxType pts, const PP_PropertyVector & attributes) override;
	virtual bool appendSpan(const UT_UCS4Char * p, UT_uint32 length) override;
	virtual bool appendObject(PTObjectType pto, const PP_PropertyVector & attribs,
							  const PP_PropertyVector & props = PP_NOPROPS) override;
	virtual bool appendFmt(const PP_PropertyVector & pVecAttributes) override;
    UT_Error							_appendSection(int numColumns, const float, const float);
//    UT_Error							_appendSpan(const guint32 textAttributeBits, const char *fontName, const float fontSize, UT_uint32 listTag = 0);
    UT_Error                            _appendListSpan(UT_uint32 listTag);
//    UT_Error							_appendParagraph(const guint8 paragraphJustification, const guint32 textAttributeBits,
//										 const gchar *fontName, const float fontSize, const float lineSpacing);
    UT_Error							_updateDocumentOrderedListDefinition(ABI_ListDefinition *pListDefinition,
												     int iLevel, const char listType,
												     const UT_UTF8String &sTextBeforeNumber,
												     const UT_UTF8String &sTextAfterNumber,
												     int iStartingNumber);
    UT_Error							_updateDocumentUnorderedListDefinition(ABI_ListDefinition *pListDefinition,
												       int level);
	/* Header/footer bodies are delivered by libwpd while the page span's
	 * body section does not exist yet, but a PTX_SectionHdrFtr must follow
	 * the body section that references it (populate resolves the id
	 * forwards). The content is therefore captured into a scratch
	 * document and replayed at endDocument(), once per section it was
	 * bound to (one id per section, since an id may only be referenced
	 * once). */
	struct WPHdrFtr
	{
		WPHdrFtr() : doc(nullptr) {}
		~WPHdrFtr() { UNREFP(doc); }
		std::string type; // "header"/"header-even"/"footer"/"footer-even"
		std::vector<std::string> ids;
		PD_Document * doc; // scratch capture document, refcounted
	};

	void								_openHdrFtr(bool bHeader, const librevenge::RVNGPropertyList &propList);
	void								_closeHdrFtr();
	void								_bindHdrFtrToSection(WPHdrFtr * hdrFtr, pf_Frag_Strux * pfs);
	void								_bindPendingHdrFtrs(pf_Frag_Strux * pfs);
	std::string							_frameProps(double frameW, double frameH) const;
	PD_Document *						_doc() { return m_pCaptureDoc ? m_pCaptureDoc : getDoc(); }
private:
    // section props
    float								m_leftPageMargin;
    float								m_rightPageMargin;
    float								m_leftSectionMargin;
    float								m_rightSectionMargin;
    int									m_sectionColumnsCount;

	// paragraph props
    float								m_topMargin;
    float								m_bottomMargin;
    float								m_leftMarginOffset;
    float								m_rightMarginOffset;
    float								m_textIndent;

    // state handling that libwpd can't account for
    //UT_StringPtrMap						m_listStylesHash;
    std::unique_ptr<ABI_ListDefinition>	m_pCurrentListDefinition;
    bool								m_bParagraphChanged;
    bool								m_bParagraphInSection;
    bool								m_bInSection;
    bool								m_bSectionChanged;
    bool								m_bRequireBlock;

    int							        m_iCurrentListLevel;
    bool								m_bInCell;

	// frame geometry libwpd emits around each embedded object
	// (WP3/WP5 pictures, WP6 figure boxes); insertBinaryObject() and
	// openTextBox() consume it when the payload arrives
	librevenge::RVNGPropertyList		m_frameProps;
	bool								m_bFrameOpen;

	// one entry per open text box: true while no content strux has
	// been appended inside it yet (an empty frame still needs a
	// block before its EndFrame)
	std::vector<bool>					m_textBoxEmpty;
	int									m_iLinkOpenCount;
	std::vector<std::string>			m_commentIds;

	std::vector<std::unique_ptr<WPHdrFtr>>	m_hdrFtrs;
	std::vector<WPHdrFtr *>				m_pendingHdrFtrs;
	PD_Document *						m_pCaptureDoc;
	int									m_bHdrFtrOpenCount;

	// importer state saved while a header/footer is being captured
	std::unique_ptr<ABI_ListDefinition>	m_savedListDefinition;
	int									m_savedListLevel;
	bool								m_savedInSection;
	bool								m_savedRequireBlock;
	bool								m_savedInCell;
};

#ifdef HAVE_LIBWPS

class IE_Imp_MSWorks_Sniffer final : public IE_ImpSniffer
{
    friend class IE_Imp;
    friend class IE_Imp_MSWorks;

public:
    IE_Imp_MSWorks_Sniffer();
    virtual ~IE_Imp_MSWorks_Sniffer();

	virtual const IE_SuffixConfidence * getSuffixConfidence() override;
    using IE_ImpSniffer::recognizeContents;

    virtual UT_Confidence_t recognizeContents(GsfInput * input) override;
	virtual const IE_MimeConfidence * getMimeConfidence() override { return nullptr; }
    virtual bool getDlgLabels (const char ** szDesc,
			       const char ** szSuffixList,
			       IEFileType * ft) override;
    virtual UT_Error constructImporter (PD_Document * pDocument,
					IE_Imp ** ppie) override;
};

#endif

#endif /* IE_IMP_WP_H */
