/* AbiWord
 * Copyright (C) 2025 AbiWord contributors
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

#ifndef IE_IMP_MARKDOWN_H
#define IE_IMP_MARKDOWN_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ie_imp.h"
#include "ie_FileInfo.h"
#include "ut_string_class.h"

class PD_Document;

// The importer class

class IE_Imp_Markdown : public IE_Imp
{
public:
	IE_Imp_Markdown(PD_Document * pDocument);
	virtual ~IE_Imp_Markdown();

	// public thunks used by the static inline-emitter helpers
	bool appendFmtPublic(const PP_PropertyVector & attributes)
		{ return appendFmt(attributes); }
	bool appendSpanPublic(const UT_UCS4Char * p, UT_uint32 length)
		{ return appendSpan(p, length); }
	bool appendObjectPublic(PTObjectType pto, const PP_PropertyVector & attribs)
		{ return appendObject(pto, attribs); }
	bool emitImagePublic(const std::string & url, const std::string & alt,
						 const std::string & title)
		{ return _emitImage(url, alt, title); }
	bool emitFootnotePublic(const std::string & text)
		{ return _emitFootnote(text); }
	bool lookupLinkRef(const std::string & id, std::string & url,
					   std::string & title) const;
	bool lookupFootnoteDef(const std::string & id, std::string & text) const;

protected:
	virtual UT_Error _loadFile(GsfInput * input) override;

private:
	void _parseDocument(const std::string & utf8);
	bool _emitParagraph(const char * szStyle, const std::string & extraProps,
						const std::string & text);
	void _emitInline(const std::string & text);
	bool _emitHeading(int level, const std::string & text);
	bool _emitListItem(int level, bool bOrdered, int startValue,
					   const std::string & text);
	bool _emitCodeBlock(const std::vector<std::string> & lines);
	bool _emitBlockQuote(int depth, const std::string & text);
	bool _emitHR(void);
	bool _emitTable(const std::vector<std::vector<std::string> > & rows,
					const std::vector<int> & aligns);
	bool _emitImage(const std::string & url, const std::string & alt,
					const std::string & title);
	bool _emitFootnote(const std::string & text);
	bool _emitMathBlock(const std::string & tex);
	void _resetLists(void);

	struct LinkRef { std::string url; std::string title; };

	std::map<std::string, LinkRef>	m_linkRefs;		// [id] -> target
	std::map<std::string, std::string>	m_footnoteDefs;	// [^id] -> text
	std::set<size_t>		m_defLines;		// consumed definition lines
	std::vector<UT_uint32>	m_listIds;		// active list ids by level
	std::vector<bool>		m_listOrdered;	// ordered flag by level
	std::vector<int>		m_listStart;	// start value by level
	UT_uint32				m_nextListID;
	UT_uint32				m_nextImage;
	UT_uint32				m_nextFootnote;
	std::string				m_fileName;
};

class ABI_EXPORT IE_Imp_Markdown_Sniffer : public IE_ImpSniffer
{
public:
	IE_Imp_Markdown_Sniffer();
	virtual ~IE_Imp_Markdown_Sniffer();

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

#endif /* IE_IMP_MARKDOWN_H */
