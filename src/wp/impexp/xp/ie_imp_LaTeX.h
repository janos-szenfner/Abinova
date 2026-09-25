/* Abinova
 * Copyright (C) 2025 Abinova contributors
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

#ifndef IE_IMP_LATEX_H
#define IE_IMP_LATEX_H

#include <string>
#include <vector>

#include "ie_imp.h"
#include "ie_FileInfo.h"
#include "ut_string_class.h"

class PD_Document;

// The importer class

class IE_Imp_LaTeX : public IE_Imp
{
public:
	IE_Imp_LaTeX(PD_Document * pDocument);
	virtual ~IE_Imp_LaTeX();

	// public thunks used by the static inline-emitter helpers
	bool appendFmtPublic(const PP_PropertyVector & attributes)
		{ return appendFmt(attributes); }
	bool appendSpanPublic(const UT_UCS4Char * p, UT_uint32 length)
		{ return appendSpan(p, length); }
	bool appendSpanPublic(const std::string & s)
		{ return appendSpan(s); }
	bool appendObjectPublic(PTObjectType pto, const PP_PropertyVector & attribs)
		{ return appendObject(pto, attribs); }
	bool appendStruxPublic(PTStruxType pts, const PP_PropertyVector & attribs)
		{ return appendStrux(pts, attribs); }
	void _emitFootnotePublic(const std::string & text)
		{ _emitFootnote(text); }
	void _emitImagePublic(const std::string & file);

protected:
	virtual UT_Error _loadFile(GsfInput * input) override;

private:
	void _parseDocument(const std::string & utf8);
	void _parseText(const std::string & text);
	bool _emitParagraph(const char * szStyle, const std::string & extraProps,
						const std::string & text);
	void _emitInline(const std::string & text);
	bool _emitHeading(int level, const std::string & text);
	bool _emitListItem(int level, bool bOrdered, const std::string & text);
	bool _emitVerbatim(const std::string & text);
	bool _emitTable(const std::vector<std::vector<std::string> > & rows);
	bool _emitHR(void);
	bool _emitPageBreak(void);
	void _emitFootnote(const std::string & text);
	void _resetLists(void);

	std::vector<UT_uint32>	m_listIds;
	std::vector<bool>		m_listOrdered;
	UT_uint32				m_nextListID;
	UT_uint32				m_nextImage;
	UT_uint32				m_nextFootnote;
	std::string				m_fileName;
	std::string				m_docTitle;
	std::string				m_docAuthor;
	std::string				m_docDate;
};

class ABI_EXPORT IE_Imp_LaTeX_Sniffer : public IE_ImpSniffer
{
public:
	IE_Imp_LaTeX_Sniffer();
	virtual ~IE_Imp_LaTeX_Sniffer();

	virtual UT_Confidence_t recognizeContents(const char * szBuf,
											  UT_uint32 iNumbytes) override;
	virtual const IE_SuffixConfidence * getSuffixConfidence() override;
	virtual const IE_MimeConfidence * getMimeConfidence() override;
	virtual UT_Error constructImporter(PD_Document * pDocument,
									   IE_Imp ** ppie) override;
	virtual bool getDlgLabels(const char ** szDesc,
							  const char ** szSuffixList,
							  IEFileType * ft) override;
};

#endif /* IE_IMP_LATEX_H */
