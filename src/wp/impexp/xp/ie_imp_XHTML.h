/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */

/* AbiWord
 * Copyright (C) 1998-2000 AbiSource, Inc.
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

#pragma once

#include <map>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "ie_imp_XML.h"
#include "fg_Graphic.h"

/* NOTE: I'm trying to keep the code similar across versions,
 *       and therefore features are enabled/disabled here:
 */

/* Define if the base unicode char is UCS-4
 */
#define XHTML_UCS4

/* Define if the sniffers need to pass export name to parent
 */
#define XHTML_NAMED_CONSTRUCTORS

/* Define if the tables are supported
 */
#define XHTML_TABLES_SUPPORTED 1

/* Define if meta information is supported
 */
#define XHTML_META_SUPPORTED

/* Define if meta information is supported
 */
#define XHTML_RUBY_SUPPORTED

class PD_Document;
class IE_Imp_TableHelperStack;

// The importer/reader for XHTML 1.0

class ABI_EXPORT IE_Imp_XHTML_Sniffer : public IE_ImpSniffer
{
	friend class IE_Imp;

public:
	IE_Imp_XHTML_Sniffer();
	virtual ~IE_Imp_XHTML_Sniffer() {}

	virtual const IE_SuffixConfidence * getSuffixConfidence() override;
	virtual const IE_MimeConfidence * getMimeConfidence() override;
	using IE_ImpSniffer::recognizeContents;

	virtual UT_Confidence_t recognizeContents (const char * szBuf,
									UT_uint32 iNumbytes) override;
	virtual bool getDlgLabels (const char ** szDesc,
							   const char ** szSuffixList,
							   IEFileType * ft) override;
	virtual UT_Error constructImporter (PD_Document * pDocument,
										IE_Imp ** ppie) override;

};

class ABI_EXPORT IE_Imp_XHTML : public IE_Imp_XML
{
public:
	IE_Imp_XHTML (PD_Document * pDocument);

	virtual ~IE_Imp_XHTML ();

	virtual void startElement (const gchar * name, const gchar ** atts) override;
	virtual void endElement (const gchar * name) override;

	virtual void charData (const gchar * buffer, int length) override;

	virtual bool pasteFromBuffer(PD_DocumentRange * pDocRange,
										const unsigned char * pData,
										UT_uint32 lenData,
										const char * szEncoding = nullptr) override;

	virtual bool appendStrux(PTStruxType pts, const PP_PropertyVector & attributes) override;
	virtual bool appendFmt(const PP_PropertyVector & vecAttributes) override;
	using IE_Imp::appendSpan;

	virtual bool appendSpan(const UT_UCS4Char * p, UT_uint32 length) override;
	virtual bool  appendObject(PTObjectType pto, const PP_PropertyVector & attributes,
							   const PP_PropertyVector & props = PP_NOPROPS) override;

	/* EPUB3 note support: _loadFile's capture pass indexes the inner
	 * markup of every element carrying epub:type="footnote|rearnote|
	 * endnote" plus an id, so that an <a epub:type="noteref"> can replay
	 * the body inside a real note strux at its own position - the aside
	 * holding the body follows its anchor in reading order, which a
	 * single streaming pass cannot reach (see insertNoteRef)
	 */
	struct s_NoteBody
	{
		bool		bEndnote;
		std::string	xml;
	};

protected:
	virtual UT_Error _loadFile (GsfInput * input) override;
	virtual FG_ConstGraphicPtr importImage(const gchar * szSrc);

private:
	FG_ConstGraphicPtr	importDataURLImage(const gchar * szData);
	void					loadStyleSheet (const char * href);
	std::string				cascadeStyle (const gchar * name,
										  const PP_PropertyVector & atts) const;

	bool					pushInline (const char * props);
	bool					newBlock (const char * style, const char * css, const char * align);
	bool					requireBlock ();
	bool					requireSection ();
	bool					childOfSection ();

	IE_Imp_TableHelperStack *	m_TableHelperStack;

	enum listType: uint8_t {L_NONE = 0, L_OL = 1, L_UL = 2 } m_listType;
	UT_uint16	m_iListID;
	UT_uint16	m_iNewListID;
	UT_uint16	m_iNewImage;

	std::stack<UT_uint16>	m_utsParents;
	std::string m_szBookMarkName;

	bool        m_addedPTXSection;

	UT_uint16	m_iPreCount;

	UT_Vector	m_divClasses;
	UT_GenericVector<UT_UTF8String *>	m_divStyles;
	bool        bInTable(void);
	bool        m_bFirstBlock;
	bool		m_bInMath;
	UT_ByteBufPtr m_pMathBB;
	std::string m_Title;

	/* <style> chardata accumulator + collected stylesheet rules
	 * ((compound selector, declaration block) in source order) -
	 * see cascadeStyle() for the supported selector subset
	 */
	bool        m_bInStyle;
	std::string m_styleText;
	std::vector<std::pair<std::string, std::string> >	m_cssRules;

	std::map<std::string, s_NoteBody>	m_notes;
	UT_uint32	m_iSkipDepth;	/* swallow subtrees while > 0 */
	UT_uint32	m_iNoteDepth;	/* > 0 while replaying a note body */

	bool					insertNoteRef (const s_NoteBody & note);
};
