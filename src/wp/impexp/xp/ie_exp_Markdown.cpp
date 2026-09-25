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
 *
 * Built-in Markdown exporter. Emits CommonMark plus the extensions
 * documented in the Zettlr Markdown Compendium
 * (https://docs.zettlr.com/en/editor/markdown-compendium.html):
 * ATX headings, emphasis/strong/strikethrough, inline and fenced
 * code, blockquotes, lists, links, images, pipe tables and
 * horizontal rules.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <map>
#include <string>
#include <vector>

#include "ie_exp_Markdown.h"
#include "ie_types.h"
#include "fd_Field.h"
#include "pd_Document.h"
#include "pp_AttrProp.h"
#include "px_ChangeRecord.h"
#include "px_CR_Object.h"
#include "px_CR_Span.h"
#include "px_CR_Strux.h"
#include "ut_debugmsg.h"
#include "ut_string.h"
#include "ut_std_string.h"
#include "ut_string_class.h"
#include "ut_types.h"
#include "ut_wctomb.h"

/*****************************************************************/
/* listener                                                      */
/*****************************************************************/

class Markdown_Listener : public PL_Listener
{
public:
	Markdown_Listener(PD_Document * pDocument, IE_Exp_Markdown * pie)
		: m_pDocument(pDocument),
		  m_pie(pie),
		  m_bEmittedAnything(false),
		  m_bBold(false),
		  m_bItalic(false),
		  m_bStrike(false),
		  m_bCode(false),
		  m_bCodeBlock(false),
		  m_bInCell(false),
		  m_iTableRow(-1),
		  m_iColsThisRow(0),
		  m_iTableCols(0),
		  m_bCellHasText(false),
		  m_bListItemStart(false),
		  m_bPrevWasList(false),
		  m_bPrevWasQuote(false),
		  m_bSkipBlockContent(false)
	{
	}

	virtual ~Markdown_Listener()
	{
	}

	virtual bool populate(fl_ContainerLayout* /*sfh*/,
						  const PX_ChangeRecord * pcr) override
	{
		switch (pcr->getType())
		{
		case PX_ChangeRecord::PXT_InsertSpan:
		{
			if (m_bSkipBlockContent)
				return true;
			const PX_ChangeRecord_Span * pcrs =
				static_cast<const PX_ChangeRecord_Span *>(pcr);
			PT_AttrPropIndex api = pcr->getIndexAP();

			bool bBold = false, bItalic = false, bStrike = false, bCode = false;
			const PP_AttrProp * pAP = nullptr;
			if (m_pDocument->getAttrProp(api, &pAP) && pAP)
			{
				const gchar * v = nullptr;
				if (pAP->getProperty("font-weight", v) && v &&
					!strcmp(v, "bold"))
					bBold = true;
				if (pAP->getProperty("font-style", v) && v &&
					!strcmp(v, "italic"))
					bItalic = true;
				if (pAP->getProperty("text-decoration", v) && v &&
					strstr(v, "line-through"))
					bStrike = true;
				if (pAP->getProperty("font-family", v) && v &&
					(strstr(v, "Courier") || strstr(v, "mono")))
					bCode = true;
			}

			PT_BufIndex bi = pcrs->getBufIndex();
			const UT_UCS4Char * pData = m_pDocument->getPointer(bi);
			if (!pData) return true;

			std::string text = _ucsToUtf8(pData, pcrs->getLength());
			if (text.empty()) return true;

			// suppress the tab that follows the list-label field
			if (m_bListItemStart)
			{
				m_bListItemStart = false;
				if (text[0] == '\t')
					text = text.substr(1);
				if (text.empty()) return true;
			}

			if (m_bCodeBlock)
			{
				// inside a fenced block: emit verbatim
				_output(text, false);
				return true;
			}

			_setFmt(bBold, bItalic, bStrike, bCode);
			_output(text, true);
			return true;
		}

		case PX_ChangeRecord::PXT_InsertObject:
		{
			const PX_ChangeRecord_Object * pcro =
				static_cast<const PX_ChangeRecord_Object *>(pcr);
			PT_AttrPropIndex api = pcr->getIndexAP();

			switch (pcro->getObjectType())
			{
			case PTO_Field:
			{
				fd_Field * field = pcro->getField();
				if (!field) return true;
				if (field->getFieldType() == fd_Field::FD_ListLabel)
					return true;	// list markers are generated
				if (field->getValue())
				{
					UT_UCS4String ws(field->getValue());
					_output(_ucsToUtf8(ws.ucs4_str(), ws.length()), true);
				}
				return true;
			}

			case PTO_Hyperlink:
			{
				const PP_AttrProp * pAP = nullptr;
				m_pDocument->getAttrProp(api, &pAP);
				const gchar * v = nullptr;
				if (pAP && pAP->getAttribute("xlink:href", v) && v)
				{
					m_hrefStack.push_back(v);
					_output("[", false);
				}
				else
				{
					std::string href = m_hrefStack.empty() ? "" :
						m_hrefStack.back();
					if (!m_hrefStack.empty())
						m_hrefStack.pop_back();
					_output("](" + _escapeUrl(href) + ")", false);
				}
				return true;
			}

			case PTO_Image:
			{
				const PP_AttrProp * pAP = nullptr;
				m_pDocument->getAttrProp(api, &pAP);
				const gchar * alt = nullptr, * title = nullptr;
				std::string sAlt = "image";
				std::string sTarget;
				if (pAP)
				{
					if (pAP->getAttribute("alt", alt) && alt && *alt)
						sAlt = alt;
					if (pAP->getAttribute("title", title) && title && *title)
						sTarget = title;
					else
					{
						const gchar * d = nullptr;
						if (pAP->getAttribute("dataid", d) && d)
							sTarget = d;
					}
				}
				_output("![" + _escapeText(sAlt) + "](" +
						_escapeUrl(sTarget) + ")", false);
				return true;
			}

			case PTO_Bookmark:
			case PTO_Embed:
			case PTO_Math:
			case PTO_Annotation:
			case PTO_RDFAnchor:
			default:
				return true;
			}
		}

		case PX_ChangeRecord::PXT_InsertFmtMark:
			return true;

		default:
			return true;
		}
	}

	virtual bool populateStrux(pf_Frag_Strux* /*sdh*/,
							   const PX_ChangeRecord * pcr,
							   fl_ContainerLayout* * psfh) override
	{
		UT_return_val_if_fail(
			pcr->getType() == PX_ChangeRecord::PXT_InsertStrux, false);
		const PX_ChangeRecord_Strux * pcrx =
			static_cast<const PX_ChangeRecord_Strux *>(pcr);
		*psfh = nullptr;
		m_bSkipBlockContent = false;

		switch (pcrx->getStruxType())
		{
		case PTX_SectionTable:
			if (m_bCodeBlock)
			{
				_output("\n```\n", false);
				m_bCodeBlock = false;
			}
			_sep();
			m_iTableRow = -1;
			m_iTableCols = 0;
			m_iColsThisRow = 0;
			m_listNums.clear();
			return true;

		case PTX_SectionCell:
		{
			const PP_AttrProp * pAP = nullptr;
			int row = 0;
			if (m_pDocument->getAttrProp(pcr->getIndexAP(), &pAP) && pAP)
			{
				const gchar * v = nullptr;
				if (pAP->getProperty("top-attach", v) && v)
					row = atoi(v);
			}
			if (row != m_iTableRow)
			{
				// starting a new row: close the previous one
				if (m_iTableRow == 0)
				{
					// finished the header row
					_output(" |\n", false);
					m_iTableCols = m_iColsThisRow;
					_sepRow();
				}
				else if (m_iTableRow > 0)
					_output(" |\n", false);
				m_iTableRow = row;
				m_iColsThisRow = 0;
				_output("| ", false);
			}
			else
				_output(" | ", false);
			m_iColsThisRow++;
			m_bInCell = true;
			m_bCellHasText = false;
			return true;
		}

		case PTX_EndCell:
			m_bInCell = false;
			if (!m_bCellHasText)
				_output(" ", false);
			return true;

		case PTX_EndTable:
			_output(" |\n\n", false);
			m_iTableRow = -1;
			m_bEmittedAnything = true;
			return true;

		case PTX_Block:
		{
			PT_AttrPropIndex api = pcr->getIndexAP();
			const PP_AttrProp * pAP = nullptr;
			m_pDocument->getAttrProp(api, &pAP);

			const gchar * szStyle = nullptr;
			const gchar * szListId = nullptr;
			const gchar * szLevel = nullptr;
			const gchar * szListStyle = nullptr;
			const gchar * szFont = nullptr;
			const gchar * szBotStyle = nullptr;
			if (pAP)
			{
				pAP->getAttribute("style", szStyle);
				if (!szStyle)
					pAP->getProperty("style", szStyle);
				pAP->getAttribute("listid", szListId);
				pAP->getAttribute("level", szLevel);
				pAP->getProperty("list-style", szListStyle);
				pAP->getProperty("font-family", szFont);
				pAP->getProperty("bot-style", szBotStyle);
			}

			// fenced code state transitions
			bool bIsCode = (szStyle && !strcmp(szStyle, "Plain Text")) ||
				(szFont && (strstr(szFont, "Courier") || strstr(szFont, "mono")));
			if (bIsCode && !m_bCodeBlock)
			{
				_sep();
				_output("```\n", false);
				m_bCodeBlock = true;
				m_bPrevWasList = false;
				m_bPrevWasQuote = false;
				return true;
			}
			else if (!bIsCode && m_bCodeBlock)
			{
				_output("\n```\n", false);
				m_bCodeBlock = false;
			}

			if (m_bInCell)
			{
				// paragraphs inside a table cell stay on the row
				if (m_bCellHasText)
					_output(" ", false);
				m_bCellHasText = true;
				m_bPrevWasList = false;
				return true;
			}

			if (m_bCodeBlock)
			{
				m_bPrevWasList = false;
				m_bPrevWasQuote = false;
				_output("\n", false);
				return true;
			}

			// horizontal rule paragraph (bottom border, no real text)
			if (szBotStyle && strcmp(szBotStyle, "0") &&
				strcmp(szBotStyle, "none"))
			{
				_sep();
				_output("---\n\n", false);
				m_bSkipBlockContent = true;
				return true;
			}

			// heading?
			int heading = 0;
			if (szStyle)
			{
				if (!strncmp(szStyle, "Heading ", 8))
					heading = atoi(szStyle + 8);
				else if (!strncmp(szStyle, "Numbered Heading ", 17))
					heading = atoi(szStyle + 17);
			}

			// list item?
			if (szListId)
			{
				int level = szLevel ? atoi(szLevel) : 1;
				if (level < 1) level = 1;
				bool ordered = szListStyle &&
					strstr(szListStyle, "Numbered") != nullptr;

				if (m_bPrevWasList)
					_listSep();
				else
					_sep();
				m_bPrevWasList = true;
				m_bPrevWasQuote = false;
				for (int k = 1; k < level; k++)
					_output("  ", false);
				if (ordered)
				{
					int num = ++m_listNums[szListId];
					_output(UT_std_string_sprintf("%d. ", num), false);
				}
				else
					_output("- ", false);
				m_bListItemStart = true;
				return true;
			}

			m_bPrevWasList = false;

			if (heading >= 1)
			{
				_sep();
				m_bPrevWasQuote = false;
				heading = heading > 6 ? 6 : heading;
				_output(std::string(heading, '#') + " ", false);
				return true;
			}

			// blockquote?
			bool bIsQuote = (szStyle && !strcmp(szStyle, "Block Text"));
			if (bIsQuote && m_bPrevWasQuote)
				_listSep();
			else
				_sep();
			m_bPrevWasQuote = bIsQuote;
			if (bIsQuote)
				_output("> ", false);
			return true;

			return true;
		}

		case PTX_Section:
		case PTX_SectionHdrFtr:
		case PTX_SectionEndnote:
		case PTX_SectionFootnote:
		case PTX_SectionAnnotation:
		case PTX_EndFootnote:
		case PTX_EndEndnote:
		case PTX_EndAnnotation:
		case PTX_SectionFrame:
		case PTX_EndFrame:
		default:
			return true;
		}
	}

	virtual bool change(fl_ContainerLayout* /*sfh*/,
						const PX_ChangeRecord * /*pcr*/) override
	{
		return true;
	}

	virtual bool insertStrux(fl_ContainerLayout* /*sfh*/,
							 const PX_ChangeRecord * /*pcr*/,
							 pf_Frag_Strux* /*sdhNew*/,
							 PL_ListenerId /*lid*/,
							 void (* /*pfnBindHandles*/)(pf_Frag_Strux*,
														PL_ListenerId,
														fl_ContainerLayout*)) override
	{
		return false;
	}

	virtual bool signal(UT_uint32 /* iSignal */) override
	{
		return true;
	}

	/*! flush any open formatting at end of document */
	void finalize(void)
	{
		_setFmt(false, false, false, false);
		if (m_bCodeBlock)
			_output("```\n", false);
		_output("\n", false);
	}

private:
	void _output(const std::string & s, bool escape)
	{
		std::string t = escape ? _escapeText(s) : s;
		if (t.empty()) return;
		m_pie->write(t.c_str(), t.size());
		m_bEmittedAnything = true;
	}

	/*! paragraph separation: blank line between blocks */
	void _sep(void)
	{
		if (m_bEmittedAnything)
			_output("\n\n", false);
	}

	/*! single newline between list items */
	void _listSep(void)
	{
		if (m_bEmittedAnything)
			_output("\n", false);
	}

	void _sepRow(void)
	{
		std::string sep = "|";
		int cols = m_iTableCols > 0 ? m_iTableCols : 1;
		for (int c = 0; c < cols; c++)
			sep += " --- |";
		sep += "\n";
		_output(sep, false);
	}

	void _setFmt(bool bBold, bool bItalic, bool bStrike, bool bCode)
	{
		if (bBold == m_bBold && bItalic == m_bItalic &&
			bStrike == m_bStrike && bCode == m_bCode)
			return;

		// close active markers, innermost first
		if (m_bCode) _output("`", false);
		if (m_bStrike) _output("~~", false);
		if (m_bItalic) _output("*", false);
		if (m_bBold) _output("**", false);

		if (bBold) _output("**", false);
		if (bItalic) _output("*", false);
		if (bStrike) _output("~~", false);
		if (bCode) _output("`", false);

		m_bBold = bBold;
		m_bItalic = bItalic;
		m_bStrike = bStrike;
		m_bCode = bCode;
	}

	std::string _ucsToUtf8(const UT_UCS4Char * data, UT_uint32 length)
	{
		std::string out;
		for (UT_uint32 i = 0; i < length; i++)
		{
			UT_UCS4Char c = data[i];
			if (c == UCS_LF)
				out += "  \n";			// hard line break
			else if (c == UCS_TAB)
				out += '\t';
			else
			{
				char buf[8];
				int len = 0;
				UT_Wctomb w;
				if (w.wctomb(buf, len, c))
					out.append(buf, len);
			}
		}
		return out;
	}

	std::string _escapeText(const std::string & s)
	{
		std::string out;
		out.reserve(s.size());
		for (char c : s)
		{
			switch (c)
			{
			case '\\': case '*': case '_': case '~':
			case '[': case ']': case '|': case '`':
				out += '\\';
				out += c;
				break;
			default:
				out += c;
			}
		}
		return out;
	}

	std::string _escapeUrl(const std::string & s)
	{
		std::string out;
		out.reserve(s.size());
		for (char c : s)
		{
			if (c == ')' || c == '(' || c == ' ' || c == '\\')
				out += '\\';
			out += c;
		}
		return out;
	}

	PD_Document * m_pDocument;
	IE_Exp_Markdown * m_pie;

	bool m_bEmittedAnything;

	bool m_bBold, m_bItalic, m_bStrike, m_bCode;
	bool m_bCodeBlock;

	bool m_bInCell;
	int  m_iTableRow;
	int  m_iColsThisRow;
	int  m_iTableCols;
	bool m_bCellHasText;

	bool m_bListItemStart;
	bool m_bPrevWasList;
	bool m_bPrevWasQuote;
	bool m_bSkipBlockContent;
	std::map<std::string, int> m_listNums;
	std::vector<std::string> m_hrefStack;
};

/*****************************************************************/
/* exporter                                                      */
/*****************************************************************/

IE_Exp_Markdown::IE_Exp_Markdown(PD_Document * pDocument)
	: IE_Exp(pDocument),
	  m_pListener(nullptr),
	  m_error(UT_OK)
{
}

IE_Exp_Markdown::~IE_Exp_Markdown()
{
}

PL_Listener * IE_Exp_Markdown::_constructListener(void)
{
	return new Markdown_Listener(getDoc(), this);
}

UT_Error IE_Exp_Markdown::_writeDocument(void)
{
	m_pListener = static_cast<Markdown_Listener *>(_constructListener());
	if (!m_pListener)
		return UT_IE_NOMEMORY;

	if (getDocRange())
		getDoc()->tellListenerSubset(m_pListener, getDocRange());
	else
		getDoc()->tellListener(m_pListener);

	m_pListener->finalize();
	DELETEP(m_pListener);

	return (m_error ? UT_IE_COULDNOTWRITE : UT_OK);
}

/*****************************************************************/
/* sniffer                                                       */
/*****************************************************************/

IE_Exp_Markdown_Sniffer::IE_Exp_Markdown_Sniffer()
	: IE_ExpSniffer(IE_IMPEXPNAME_MARKDOWN, true)
{
}

IE_Exp_Markdown_Sniffer::~IE_Exp_Markdown_Sniffer()
{
}

bool IE_Exp_Markdown_Sniffer::recognizeSuffix(const char * szSuffix)
{
	return (!g_ascii_strcasecmp(szSuffix, ".md") ||
			!g_ascii_strcasecmp(szSuffix, ".markdown") ||
			!g_ascii_strcasecmp(szSuffix, ".mdown") ||
			!g_ascii_strcasecmp(szSuffix, ".mkd") ||
			!g_ascii_strcasecmp(szSuffix, ".mkdn"));
}

UT_Confidence_t IE_Exp_Markdown_Sniffer::supportsMIME(const char * szMIME)
{
	if (!g_ascii_strcasecmp(szMIME, "text/markdown") ||
		!g_ascii_strcasecmp(szMIME, "text/x-markdown"))
		return UT_CONFIDENCE_PERFECT;
	return UT_CONFIDENCE_ZILCH;
}

UT_UTF8String IE_Exp_Markdown_Sniffer::getPreferredSuffix()
{
	return UT_UTF8String(".md");
}

UT_Error IE_Exp_Markdown_Sniffer::constructExporter(PD_Document * pDocument,
												  IE_Exp ** ppie)
{
	*ppie = new IE_Exp_Markdown(pDocument);
	return UT_OK;
}

bool IE_Exp_Markdown_Sniffer::getDlgLabels(const char ** pszDesc,
										   const char ** pszSuffixList,
										   IEFileType * ft)
{
	*pszDesc = "Markdown (.md, .markdown)";
	*pszSuffixList = "*.md; *.markdown; *.mdown; *.mkd; *.mkdn";
	*ft = getFileType();
	return true;
}
