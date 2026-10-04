/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* Abinova
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2008 Xun Sun (xun.sun.cn@gmail.com)
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

#include <stdlib.h>
#include <string.h>
#include <deque>
#include <stack>

#include "fp_types.h"
#include "ut_debugmsg.h"
#include "ut_string.h"
#include "ut_bytebuf.h"
#include "ut_base64.h"
#include "ut_Language.h"
#include "ut_path.h"
#include "ut_units.h"
#include "ut_mbtowc.h"
#include "ut_wctomb.h"
#include "pt_Types.h"
#include "ie_exp_LaTeX.h"
#include "pd_Document.h"
#include "pd_Style.h"
#include "pp_AttrProp.h"
#include "px_ChangeRecord.h"
#include "px_CR_Object.h"
#include "px_CR_Span.h"
#include "px_CR_Strux.h"
#include "xap_App.h"
#include "xap_EncodingManager.h"
#include "fd_Field.h"
#include "ie_Table.h"
#include "ut_locale.h"
#include "ut_string_class.h"
#include "ut_misc.h"
#include "ie_math_convert.h"
#include "ie_types.h"

/*****************************************************************/
/*****************************************************************/

IE_Exp_LaTeX_Sniffer::IE_Exp_LaTeX_Sniffer () :
  IE_ExpSniffer(IE_IMPEXPNAME_LATEX, true)
{
  // 
}

bool IE_Exp_LaTeX_Sniffer::recognizeSuffix(const char * szSuffix)
{
	return (!g_ascii_strcasecmp(szSuffix,".tex") || !g_ascii_strcasecmp(szSuffix, ".latex"));
}

UT_Error IE_Exp_LaTeX_Sniffer::constructExporter(PD_Document * pDocument,
						 IE_Exp ** ppie)
{
	IE_Exp_LaTeX * p = new IE_Exp_LaTeX(pDocument);
	*ppie = p;
	return UT_OK;
}

bool IE_Exp_LaTeX_Sniffer::getDlgLabels(const char ** pszDesc,
					const char ** pszSuffixList,
					IEFileType * ft)
{
	*pszDesc = "LaTeX (.tex)";
	*pszSuffixList = "*.tex; *.latex";
	*ft = getFileType();
	return true;
}

/*****************************************************************/
/*****************************************************************/

//#define DEFAULT_SIZE "12pt"
#define EPSILON 0.1

enum JustificationTypes {
	JUSTIFIED,
	CENTER,
	RIGHT,
	LEFT
};

IE_Exp_LaTeX::IE_Exp_LaTeX(PD_Document * pDocument)
	: IE_Exp(pDocument)
{
	m_error = 0;
	m_pListener = nullptr;
}

IE_Exp_LaTeX::~IE_Exp_LaTeX()
{
}

/*****************************************************************/
/*****************************************************************/
typedef UT_UCS4Char U16;
static int wvConvertUnicodeToLaTeX(U16 char16, const char*& out);
static bool _convertLettersToSymbols(char c, const char *& subst);

#define BT_NORMAL		1
#define BT_HEADING1		2
#define BT_HEADING2		3
#define BT_HEADING3		4
#define BT_BLOCKTEXT	5
#define BT_PLAINTEXT	6

class LaTeX_Analysis_Listener : public PL_Listener
{
private:
	ie_Table *  m_pTableHelper;
public:
	bool m_hasEndnotes;
	bool m_hasTable;
	bool m_hasMultiRow;

	LaTeX_Analysis_Listener(PD_Document * pDocument,
							IE_Exp_LaTeX * /*pie*/)
		: m_hasEndnotes(false),
		  m_hasTable(false),
		  m_hasMultiRow(false)
	{
	    m_pTableHelper = new ie_Table(pDocument);
	}

	virtual ~LaTeX_Analysis_Listener()
	{
	    DELETEP(m_pTableHelper);
	}

	virtual bool		populate(fl_ContainerLayout* /*sfh*/,
					    const PX_ChangeRecord * /*pcr*/) override
	{
		return true;	
	}

	virtual bool		populateStrux(pf_Frag_Strux* sdh,
						const PX_ChangeRecord * pcr,
						fl_ContainerLayout* * psfh) override
	{
		UT_ASSERT(pcr->getType() == PX_ChangeRecord::PXT_InsertStrux);
		const PX_ChangeRecord_Strux * pcrx = static_cast<const PX_ChangeRecord_Strux *> (pcr);
		*psfh = nullptr;							// we don't need it.

		switch (pcrx->getStruxType())
		{
		case PTX_SectionEndnote:
		case PTX_EndEndnote:
			m_hasEndnotes = true;
			break;
		case PTX_SectionTable:
		{
			m_pTableHelper->openTable(sdh, pcr->getIndexAP());
			m_hasTable = true;
			break;
		}
		case PTX_EndTable:
		{
			m_pTableHelper->closeTable();
			break;
		}
		case PTX_SectionCell:
			m_pTableHelper->openCell(pcr->getIndexAP());
			if(m_pTableHelper->getBot() - m_pTableHelper->getTop() >1)
				this->m_hasMultiRow = true;
			break;
		case PTX_EndCell:
			m_pTableHelper->closeCell();
			break;
		default:
			break;
		}

		return true;
	}

	virtual bool		change(fl_ContainerLayout* /*sfh*/,
					const PX_ChangeRecord * /*pcr*/) override
	{
		UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
		return false;
	}

	virtual bool		insertStrux(fl_ContainerLayout* /*sfh*/,
					    const PX_ChangeRecord * /*pcr*/,
					    pf_Frag_Strux* /*sdh*/,
					    PL_ListenerId /*lid*/,
					    void (* /*pfnBindHandles*/)(pf_Frag_Strux* sdhNew,
									PL_ListenerId lid,
									fl_ContainerLayout* sfhNew)) override
	{
		UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
		return false;
	}

	virtual bool		signal(UT_uint32 /*iSignal*/) override
	{
		UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
		return false;
	}
};

class s_LaTeX_Listener : public PL_Listener
{
public:
	s_LaTeX_Listener(PD_Document * pDocument,
			    IE_Exp_LaTeX * pie,
			    const LaTeX_Analysis_Listener& analysis);
	virtual ~s_LaTeX_Listener();

	virtual bool		populate(fl_ContainerLayout* sfh,
					const PX_ChangeRecord * pcr) override;

	virtual bool		populateStrux(pf_Frag_Strux* sdh,
						const PX_ChangeRecord * pcr,
						fl_ContainerLayout* * psfh) override;

	virtual bool		change(fl_ContainerLayout* sfh,
					const PX_ChangeRecord * pcr) override;

	virtual bool		insertStrux(fl_ContainerLayout* sfh,
						const PX_ChangeRecord * pcr,
						pf_Frag_Strux* sdh,
						PL_ListenerId lid,
						void (* pfnBindHandles)(pf_Frag_Strux* sdhNew,
							PL_ListenerId lid,
							fl_ContainerLayout* sfhNew)) override;

	virtual bool		signal(UT_uint32 iSignal) override;

protected:
	void				_closeBlock(void);
	void				_closeCell(void);
	void				_closeParagraph(void);
	void				_closeSection(void);
	void				_closeSpan(void);
	void				_closeTable(void);
	void                _closeList(void);
	void                _closeLists(void);
	void				_openCell(PT_AttrPropIndex api);
	void				_openParagraph(PT_AttrPropIndex api);
	void				_openSection(PT_AttrPropIndex api);
	void				_openSpan(PT_AttrPropIndex api);
	void				_openTable(PT_AttrPropIndex api);
	void				_outputBabelPackage(void);
	void				_outputData(const UT_UCS4Char * p, UT_uint32 length);
	void				_handleDataItems(void);
	void				_convertFontSize(UT_String& szDest, const char* pszFontSize);
	void				_convertColor(UT_String& szDest, const char* pszColor);
	void				_handleImage(const PP_AttrProp * pAP);
	
	PD_Document *		m_pDocument;
	IE_Exp_LaTeX *		m_pie;
	bool				m_bInBlock;
	bool				m_bInCell;
	bool				m_bInSection;
	bool				m_bInSpan;
	bool				m_bInList;
	bool				m_bInScript;
	bool				m_bInHeading;
	bool				m_bInFootnote;
	bool				m_bBetweenQuotes;
	const PP_AttrProp*	m_pAP_Span;
	bool                m_bMultiCols;
	bool				m_bInSymbol;
	bool				m_bInEndnote;
	bool				m_bHaveEndnote;
	bool				m_bOverline;
  
	JustificationTypes  m_eJustification;
	bool				m_bLineHeight;
	int 				ChapterNumber;
	
	/* default font size for the current document, in pt,
	 * as defined by the Normal style
	 */
	int 				m_DefaultFontSize;
	
	int                 m_Indent;
	int		    m_NumCloseBrackets; // accessed by _openSpan() and _closeSpan()
	int		    m_TableWidth;
	int		    m_CellLeft;
	int		    m_CellRight;
	int		    m_CellTop;
	int		    m_CellBot;
	
// Type for the last-processed list  
	FL_ListType			list_type;
	std::stack<FL_ListType>	list_stack;
	// Need to look up proper type, and place to stick #defines...

	UT_uint16		m_iBlockType;	// BT_*
	UT_Wctomb		m_wctomb;
	
	// Table utility members
	
	ie_Table *			m_pTableHelper;

	int		    m_RowNuminTable; // the current row being handled, starting from 1
	int		    m_ExpectedLeft; // expected left-attach value for the next cell,
					    // to deal with cells spanning multiple columns;
					    // starting from 0

	std::deque<UT_Rect*>	*m_pqRect; // pointer to a UT_Rect (de)queque. each UT_Rect
					   // instance in this queue has a 1:1 correspondence
					   // to a cell spanning multiple rows. The queue is
					   // examined when a table row ends, to output a \hline
					   // or several \cline as appropriate
	unsigned int	    m_index; // (dynamic, increase only) index into m_pqRect; it is safe
				     // to skip anything before m_index
};

void s_LaTeX_Listener::_closeParagraph(void)
{
	if ((!m_bInCell) && (!m_bInFootnote) && (!m_bInEndnote)) m_pie->write("\n");
	m_bInHeading = false;
	return;
}
        
void s_LaTeX_Listener::_closeList(void)
{
	switch (list_type) {
		case NUMBERED_LIST:
			m_pie->write("\\end{enumerate}\n");
			break;
		case BULLETED_LIST:
			m_pie->write("\\end{itemize}\n");
			break;
		default:
			;
	}
	list_stack.pop();
	if (!list_stack.empty())
	{
		list_type = list_stack.top();
	}
}

void s_LaTeX_Listener::_closeLists()
{
	do{
		_closeList();
	} while(!list_stack.empty());
	m_bInList = false;
}

void s_LaTeX_Listener::_closeSection(void)
{
	_closeBlock();
	if (!m_bInSection)
	{
		return;
	}

	if (m_bInList)
	{
		this->_closeLists();
	}
 
	if (m_bMultiCols)
	{
		m_pie->write("\\end{multicols}\n");
		m_bMultiCols = false;
	}

	m_bInSection = false;
	return;
}

void s_LaTeX_Listener::_closeBlock(void)
{ 
	_closeSpan();
	if(m_bInFootnote || m_bInEndnote)
		return;
	if (!m_bInBlock)
		return;
	// if(m_bInCell) m_pie->write("Block end");

	switch (m_iBlockType)
	{
	case BT_NORMAL:
		if (m_bLineHeight)
		  m_pie->write("\n\\end{spacing}");

		switch (m_eJustification)
		{
		case JUSTIFIED:
			break;
		case CENTER:
			m_pie->write("\n\\end{center}");
			break;
		case RIGHT:
			m_pie->write("\n\\end{flushright}");
			break;
		case LEFT:
			m_pie->write("\n\\end{flushleft}");
			break;
		}

		if(!m_bInCell) m_pie->write("\n\n");
		break;
	case BT_HEADING1:
	case BT_HEADING2:
	case BT_HEADING3:
		m_pie->write("}\n");
		break;
	case BT_BLOCKTEXT:
		m_pie->write("\n\\end{quote}\n"); // It's not correct, but I'll leave it by now...
		break;
	case BT_PLAINTEXT:
		m_pie->write("}\n");
		break;
	default:
		m_pie->write("%% oh, oh\n");
	}

	m_bInBlock = false;
	return;
}

void s_LaTeX_Listener::_openCell(PT_AttrPropIndex api)
{
	this->m_pTableHelper->openCell(api);
	m_CellLeft = this->m_pTableHelper->getLeft();
	m_CellTop = this->m_pTableHelper->getTop();
	m_CellRight = this->m_pTableHelper->getRight();
	m_CellBot = this->m_pTableHelper->getBot();
	m_bInCell = true;
	
	if (this->m_pTableHelper->isNewRow())
	{
	    m_ExpectedLeft = 0;
	    if(m_CellTop != 0)
			m_pie->write("\\\\");
	    m_pie->write("\n");
	    if(!m_pqRect || m_pqRect->empty())
			m_pie->write("\\hline");
	    else
	    {
			UT_Rect* p;
			int left=1;
			while(m_index < m_pqRect->size())
			{
				p = m_pqRect->at(m_index);
				if(p->top + p->height -1 > m_RowNuminTable)
					break;
				m_index++;
			}
			for(unsigned int i=m_index; i< m_pqRect->size(); i++)
			{
				p = m_pqRect->at(i);
				if(m_RowNuminTable < p->top)
					break;
				if(left < p->left)
				{
					UT_String str;
					UT_String_sprintf(str, "\\cline{%d-%d}", left, p->left-1);
					m_pie->write(str);
				}
		    
				left = p->left + p->width;
				if(left > this->m_TableWidth)
					break;
			}
			xxx_UT_DEBUGMSG(("left = %d \n", left));
			if(left <= m_TableWidth)
			{
				if(1 == left)
					m_pie->write("\\hline");
				else
				{
					UT_String str;
					UT_String_sprintf(str, "\\cline{%d-%d}", left, m_TableWidth);
					m_pie->write(str);
				}
			}
	    }
	    m_pie->write("\n");
	    m_RowNuminTable = m_CellTop + 1;
	}
	if (m_CellLeft != 0)
	{
	    int i = m_CellLeft - m_ExpectedLeft;
	    for(; i>0; i--)
			m_pie->write("&");
	}
	if(m_CellRight - m_CellLeft >1)
	{
	    UT_String str;
	    UT_String_sprintf(str, "\\multicolumn{%d}{|l|}{", m_CellRight - m_CellLeft);
	    m_pie->write(str);
	}
	if(m_CellBot - m_CellTop >1)
	{
	    UT_String str;
	    UT_String_sprintf(str, "\\multirow{%d}{*}{", m_CellBot - m_CellTop);
	    m_pie->write(str);
	    if(m_pqRect)
	    {
		UT_Rect * p = new UT_Rect(m_CellLeft+1, m_CellTop+1, 
			m_CellRight - m_CellLeft, m_CellBot - m_CellTop);
		if(p)
		    m_pqRect->push_back(p);
	    }
	}
}

void s_LaTeX_Listener::_openParagraph(PT_AttrPropIndex api)
{
	m_bLineHeight = false;

	if (!m_bInSection)
	{
		return;
	}
	
	const PP_AttrProp * pAP = nullptr;
	bool bHaveProp = m_pDocument->getAttrProp(api,&pAP);
	m_iBlockType = BT_NORMAL;
	
	if (bHaveProp && pAP)
	{
		const gchar * szValue;

		if ((pAP->getAttribute(PT_LISTID_ATTRIBUTE_NAME, szValue))
			&& (pAP->getAttribute(PT_STYLE_ATTRIBUTE_NAME, szValue))
			&& (0 == strcmp(szValue, "Normal")))
		{
			int indent = 0;
			bool bNewList = false;
			const gchar * szIndent, * szLeft, * szListStyle;
			szIndent = szLeft = szListStyle = nullptr;
			FL_ListType this_list_type = NOT_A_LIST;
			pAP->getProperty("list-style", szListStyle);
			
			if(szListStyle)
			{
				if (0 == strcmp(szListStyle, "Numbered List") )
					this_list_type = NUMBERED_LIST;
				else if (0 == strcmp(szListStyle, "Bullet List") )
					this_list_type = BULLETED_LIST;
			}
			
			if (this_list_type == NOT_A_LIST)
			{
			    this_list_type = list_type;
			}
			
			if (pAP->getProperty("text-indent", szIndent) && pAP->getProperty("margin-left", szLeft))
			{
				indent = UT_convertToDimension(szIndent, DIM_MM) + UT_convertToDimension(szLeft, DIM_MM);
				if (m_bInList)
				{
					xxx_UT_DEBUGMSG(("      indent = %d, m_Indent = %d\n", indent, m_Indent));
					if(indent > this->m_Indent) //nested list
						bNewList = true;
					else if (indent < this->m_Indent)
					{
						this->_closeList();
					}
					else
					/*
					 * now we have indent == this->m_Indent,
					 * but it is possible that the current list item is
					 * of different style with the last one.
					 */                                    
					{
						if (this_list_type != list_type)
						{
							this->_closeList();
							bNewList = true;
						}
					}
				}
			}
			
			if (bNewList || !m_bInList) 
				//necessary to build a new (possibly nested) list
			{
			    list_type = this_list_type;
			    if (list_type == NUMBERED_LIST)
			    {
					m_pie->write("\\begin{enumerate}\n");
			    }
			    else if (list_type == BULLETED_LIST)
			    {
					m_pie->write("\\begin{itemize}\n");
			    }
			    
			    list_stack.push(list_type);
			    m_bInList = true;
			}
			
			if (szIndent && szLeft)
				this->m_Indent = indent;
			
			m_pie->write("\\item ");
		} else if (m_bInList) {
			this->_closeLists();
		}

		if (pAP->getAttribute(PT_STYLE_ATTRIBUTE_NAME, szValue))
		{
			if (strstr(szValue, "Heading"))
				m_bInHeading = true;
			if(0 == strcmp(szValue, "Heading 1")) 
			{
				m_iBlockType = BT_HEADING1;
				m_pie->write("\\section*{");
			}
			else if(0 == strcmp(szValue, "Heading 2")) 
			{
				m_iBlockType = BT_HEADING2;
				m_pie->write("\\subsection*{");
			}
			else if(0 == strcmp(szValue, "Heading 3")) 
			{
				m_iBlockType = BT_HEADING3;
				m_pie->write("\\subsubsection*{");
			}
			else if(0 == strcmp(szValue, "Numbered Heading 1")) 
			{
				m_iBlockType = BT_HEADING1;
				m_pie->write("\\section{");
			}
			else if(0 == strcmp(szValue, "Numbered Heading 2")) 
			{
				m_iBlockType = BT_HEADING2;
				m_pie->write("\\subsection{");
			}
			else if(0 == strcmp(szValue, "Numbered Heading 3")) 
			{
				m_iBlockType = BT_HEADING3;
				m_pie->write("\\subsubsection{");
			}
			else if (0 == strcmp(szValue, "Chapter Heading")) {
				// TODO: Clean this...
				char			szChapterNumber[12];
				m_iBlockType = BT_HEADING1;
				snprintf(szChapterNumber, 12, "%d", ChapterNumber++);
				m_pie->write ("\n\\newpage \\section*{\\LARGE\\chaptername\\ ");
				m_pie->write(szChapterNumber);
				m_pie->write(" ");	  // \\newline");
        	}
			else if(0 == strcmp(szValue, "Block Text"))
			{
				m_iBlockType = BT_BLOCKTEXT;
				m_pie->write("\\begin{quote}\n");
			}
			else if(0 == strcmp(szValue, "Plain Text"))
			{
				m_iBlockType = BT_PLAINTEXT;
				m_pie->write("\\texttt{");
			}
		}
		
		/* Assumption: never get property set with h1-h3, block text, plain text. Probably true. */
		
		/* In LaTeX, a footnote is enclosed within a parapgraph, so we need to
		 * preserve values of the previous block, in particular m_iBlockType and
		 * m_eJustification, as they affect the behavior of _closeBlock()
		 */
		if (m_iBlockType == BT_NORMAL && !m_bInFootnote)
		{
			m_eJustification = JUSTIFIED;
			if (pAP->getProperty("text-align", szValue))
			{
				if (0 == strcmp(szValue, "center"))
				{
					m_pie->write("\\begin{center}\n");
					m_eJustification = CENTER;
				}
				if (0 == strcmp(szValue, "right"))
				{
					m_pie->write("\\begin{flushright}\n");
					m_eJustification = RIGHT;
				}
				if (0 == strcmp(szValue, "left"))
				{
					m_pie->write("\\begin{flushleft}\n");
					m_eJustification = LEFT;
				}
			}

			if (pAP->getProperty("line-height", szValue))
			{
				double height = atof(szValue);

				if (height < 0.9 || height > 1.1)
				{
					char strH[314];

					/* Assume $baselineskip/fontsize \approx 1.2$, reasonable in most cases */
					snprintf(strH, 314, "%.2f", height / 1.2);
					strH[7] = '\0';

					m_pie->write("\\begin{spacing}{");
					xxx_UT_DEBUGMSG(("m_bLineHeight = true\n"));
					m_bLineHeight = true;

					m_pie->write(strH);
					m_pie->write("}\n");
				}
			}
		}
	}
	
	m_bInBlock = true;
}

void s_LaTeX_Listener::_openSection(PT_AttrPropIndex api)
{
	const PP_AttrProp* pAP = nullptr;
	const gchar* pszNbCols = nullptr;

	m_bBetweenQuotes = false;
	m_bInList = false;
	m_bInFootnote = false;
	m_bMultiCols = false;

	if (m_pDocument->getAttrProp(api, &pAP) && pAP)
	{
		const gchar* pszPageMarginLeft = nullptr;
		const gchar* pszPageMarginRight = nullptr;

		pAP->getProperty("columns", pszNbCols);
		pAP->getProperty("page-margin-right", pszPageMarginLeft);
		pAP->getProperty("page-margin-left", pszPageMarginRight);

		if (pszNbCols != nullptr && ((0 == strcmp(pszNbCols, "2"))
						|| (0 == strcmp(pszNbCols, "3"))))
		{
			m_bMultiCols = true;
		}
		if (pszPageMarginLeft != nullptr)
		{
			m_pie->write("\\setlength{\\oddsidemargin}{");
			m_pie->write(static_cast<const char *> (pszPageMarginLeft));
			m_pie->write("-1in");
			m_pie->write("}\n");
		}
		if (pszPageMarginRight != nullptr)
		{
			m_pie->write("\\setlength{\\textwidth}{\\paperwidth - ");
			m_pie->write(static_cast<const char *> (pszPageMarginRight));
			m_pie->write("-");
			m_pie->write(static_cast<const char *> (pszPageMarginLeft));
			m_pie->write("}\n");
		}
	}

	if (m_bMultiCols)
	{
		m_pie->write("\\begin{multicols}{");
		m_pie->write(static_cast<const char *> (pszNbCols));
		m_pie->write("}\n");
	}
}

void s_LaTeX_Listener::_convertColor(UT_String& szDest, const char* pszColor)
{
	char colors[3][3];
	for (int i=0;i<3;++i)
	{
		strncpy (colors[i],&pszColor[2*i],2);
		colors[i][2]=0;
	}
	UT_LocaleTransactor lt (LC_NUMERIC, "C");
	UT_String_sprintf (szDest, "%.3f,%.3f,%.3f",
			   strtol (&colors[0][0],nullptr,16)/255.,
			   strtol (&colors[1][0],nullptr,16)/255.,
			   strtol (&colors[2][0],nullptr,16)/255.);
}

struct LaTeX_Font_Size
{
	guint8 tiny;
	guint8 scriptsize;
	guint8 footnotesize;
	guint8 small;
	/* int normalsize; */ 
	guint8 large;
	guint8 Large;
	guint8 LARGE;
	guint8 huge;
	guint8 Huge;
};

/*
 * These font sizes in the standard document classes are documented in 
 * "The (Not So) Short Introduction to LaTeX2e" and the following url:
 * http://en.wikibooks.org/wiki/LaTeX/Formatting
 */
static const LaTeX_Font_Size fontsizes[]=
{
	{5, 7, 8, 9, /*10,*/ 12, 14, 17, 20, 25}, // normalsize == 10pt
	{6, 8, 9, 10, /*11,*/ 12, 17, 17, 20, 25}, // normalsize == 11pt
	{6, 8, 10, 11, /*12,*/ 14, 17, 20, 25, 25} // normalsize == 12pt
};

void s_LaTeX_Listener::_convertFontSize(UT_String& szDest, const char* pszFontSize)
{
	double fSizeInPoints = UT_convertToPoints(pszFontSize);
	const LaTeX_Font_Size *fs = nullptr;

	if(m_bInScript) {
		fSizeInPoints -= 4;
	}
	
	if (m_DefaultFontSize == 10)
	{
		fs = &fontsizes[0];
	}
	else if (m_DefaultFontSize == 11)
	{
		fs = &fontsizes[1];
	}
	else // m_DefaultFontSize == 12
	{
		fs = &fontsizes[2];
	}
	
	if (fSizeInPoints <= fs->tiny)
	{
		szDest = "tiny";
	}
	else if (fSizeInPoints <= fs->scriptsize)
	{
		szDest = "scriptsize";
	}
	else if (fSizeInPoints <= fs->footnotesize)
	{
		szDest = "footnotesize";
	}
	else if (fSizeInPoints <= fs->small)
	{
		szDest = "small";
	}
	else if (fSizeInPoints <= m_DefaultFontSize)
	{
		szDest = "normalsize";
	}
	else if (fSizeInPoints <= fs->large)
	{
		szDest = "large";
	}
	else if (fSizeInPoints <= fs->Large)
	{
		szDest = "Large";
	}
	else if (fSizeInPoints <= fs->LARGE)
	{
		szDest = "LARGE";
	}
	else if (fSizeInPoints <= fs->huge)
	{
		szDest = "huge";
	}
	else
	{
		szDest = "Huge";
	}
}

void s_LaTeX_Listener::_openSpan(PT_AttrPropIndex api)
{
	if (!m_bInBlock)
	{
		return;
	}
	
	const PP_AttrProp * pAP = nullptr;
	bool bHaveProp = m_pDocument->getAttrProp(api,&pAP);
	m_bOverline = false;
	m_NumCloseBrackets = 0;
	
	if (bHaveProp && pAP)
	{
		const gchar * szValue;

		if (pAP->getProperty("font-weight", szValue)
			&& !strcmp(szValue, "bold")
			)
		{
			m_pie->write("\\textbf{");
			m_NumCloseBrackets++;
		}
		
		if (pAP->getProperty("font-style", szValue)
			&& !strcmp(szValue, "italic")
			)
		{
			m_pie->write("\\emph{");
			m_NumCloseBrackets++;
		}
		
		if (pAP->getProperty("text-position", szValue))
		{
			if (!strcmp("superscript", szValue))
			{
				m_bInScript = true;
				m_pie->write("\\textsuperscript{");
				m_NumCloseBrackets++;
			}
			else if (!strcmp("subscript", szValue))
			{
				m_bInScript = true;
				m_pie->write("\\textsubscript{");
				m_NumCloseBrackets++;
			}
		}
		
		const gchar* pszColor = nullptr;
		pAP->getProperty("color", pszColor);
		if (pszColor)
		{
		    if ((0 != strcmp("000000", pszColor)) &&
			(0 != strcmp("transparent", pszColor)))
		    {
				UT_String szColor;
				_convertColor(szColor,static_cast<const char*>(pszColor));
				m_pie->write("\\textcolor[rgb]{");
				m_pie->write(szColor);
				m_pie->write("}{");
				m_NumCloseBrackets++;
		    }
		}
		
		const gchar* pszBgColor = nullptr;
		pAP->getProperty("bgcolor", pszBgColor);

		if (pszBgColor)
		{
		  if ((0 != strcmp("000000", pszBgColor)) &&
		      (0 != strcmp("transparent", pszBgColor)))
		    {
		      UT_String szColor;
		      _convertColor(szColor,static_cast<const char*>(pszBgColor));
		      m_pie->write("\\colorbox[rgb]{");
		      m_pie->write(szColor);
		      m_pie->write("}{");
		      m_NumCloseBrackets++;
		    }
		}

 		if (pAP->getProperty("font-size", szValue) && !m_bInHeading)
		{
			if (int(0.5 + UT_convertToPoints(szValue)) != m_DefaultFontSize)
			{
				m_pie->write("{\\");
				UT_String szSize;
				_convertFontSize(szSize, static_cast<const char*>(szValue));
				m_pie->write(szSize);
				m_pie->write(" ");
				m_NumCloseBrackets++;
			}
		}
		
		if (pAP->getProperty("font-family", szValue))
		{
			// TODO: Use a dynamic substitution table
			if (strstr(szValue, "Symbol") && !m_bInHeading)
				m_bInSymbol = true;
			if (strstr(szValue, "Courier") ||
				!strcmp("Luxi Mono",szValue)) {
				m_pie->write("\\texttt{");
				m_NumCloseBrackets++;
			}
			if (!strcmp("Arial", szValue) ||
				!strcmp("Helvetic", szValue) ||
				!strcmp("Luxi Sans",szValue)) {
				m_pie->write("\\textsf{");
				m_NumCloseBrackets++;
			}
			UT_DEBUGMSG (("Latex export: TODO: 'font-family' property\n"));
		}

		if (pAP->getProperty("text-decoration", szValue) && szValue && !m_bInHeading)
		{
			gchar* p = g_strdup(szValue);

			UT_return_if_fail(p);
			gchar*	q = strtok(p, " ");

			// See the ulem.sty documentation (available at www.ctan.org)
			// if you wish to include other kinds of underlines, such as
			// double underlines or wavy underlines
			while (q)
			{
			    if (0 == strcmp(q, "underline"))
			    {
					m_pie->write("\\uline{");
					m_NumCloseBrackets++;
			    }
			    else if(0 == strcmp(q, "overline"))
			    {
					m_bOverline = true;
			    }
			    else if(0 == strcmp(q, "line-through"))
			    {
					m_pie->write("\\sout{");
					m_NumCloseBrackets++;
			    }
			    q = strtok(nullptr, " ");
			}
			
			/* This should be at the very last, in order to match
			 * the close brackets in _closeSpan().
			 */
			if (m_bOverline)
			    m_pie->write("$\\overline{\\textrm{");
			g_free(p);
		}

		m_bInSpan = true;
		m_pAP_Span = pAP;
	}
}

void s_LaTeX_Listener::_openTable(PT_AttrPropIndex /*api*/)
{
	UT_sint32 i = 0;

	m_pie->write("\n\n%");
	m_pie->write("\n% Table begins");
	m_pie->write("\n% ");
	m_pie->write("\n\\begin{table}[h]\\begin{tabular}{|");
	for(i = 0; i < m_pTableHelper->getNumCols(); i++) m_pie->write("l|");
	m_pie->write("}");
//	m_pie->write("\n\\hline\n");
	
	m_RowNuminTable = 1;
	m_ExpectedLeft = 0;
	m_index = 0;
}

void s_LaTeX_Listener::_closeCell(void)
{
	if (m_CellBot - m_CellTop >1)
	    m_pie->write("}");
	if (m_CellRight - m_CellLeft >1)
	    m_pie->write("}");
	m_bInCell = false;
	this->m_pTableHelper->closeCell();
	if(m_CellRight == m_TableWidth)
	{
	    m_ExpectedLeft = 0;
	}
	else
	{
	    m_ExpectedLeft = m_CellRight;
	    m_pie->write("&");	    	    
	}
}

void s_LaTeX_Listener::_closeSpan(void)
{
	if (!m_bInSpan)
		return;

	if (m_bOverline)
	    m_pie->write("}}$");
	
	if (m_pAP_Span)
	{
		m_bInScript = false;
		if (m_bInSymbol)
		    m_bInSymbol = false;
		for(; m_NumCloseBrackets>0; m_NumCloseBrackets--)
			m_pie->write("}");

		m_pAP_Span = nullptr;
	}

	m_bInSpan = false;
	return;
}

void s_LaTeX_Listener::_closeTable(void)
{
    if(m_pqRect)
    {
		for(unsigned int i=0; i<m_pqRect->size(); i++)
		{
			delete m_pqRect->at(i);
			m_pqRect->at(i) = nullptr;
		}
		m_pqRect->clear();
    }
    m_pie->write("\\\\\n\\hline\n");
    m_pie->write("\\end{tabular}\n\\end{table}\n");
}

void s_LaTeX_Listener::_outputData(const UT_UCS4Char * data, UT_uint32 length)
{
	if (!m_bInBlock)
	{
		return;
	}

	UT_String sBuf;
	const UT_UCS4Char * pData;

	UT_ASSERT(sizeof(UT_Byte) == sizeof(char));

	sBuf.reserve(length);
	for (pData = data; (pData < data + length); /**/)
	{
		const char* subst = "";

		if (m_bInSymbol)
		{
			if (_convertLettersToSymbols(*pData, subst))
			{
				while (*subst)
					sBuf += *subst++;
				pData++;
				continue;
			}
		}

		// If you don't know what code is a character,
		// this will print it for you...
		// printf("%c,%d\n",*pData,*pData);

		switch (*pData)
		{

		case ' ':
			if (m_bInScript)
			sBuf += '\\';
			sBuf += ' ';
			pData++;
			break;
			
		case '\\':
			sBuf += "\\ensuremath{\\backslash}";
			pData++;
			break;
			
		case '$':
			sBuf += '\\'; 
			sBuf += '$';
			pData++;
			break;

		case '%':
			sBuf += '\\'; 
			sBuf += '%';
			pData++;
			break;
			
		case '&':
			sBuf += '\\'; 
			sBuf += '&';
			pData++;
			break;

		case '#':
			sBuf += '\\'; 
			sBuf += '#';
			pData++;
			break;

		case '_':
			sBuf += '\\'; 
			sBuf += '_';
			pData++;
			break;

		case '{':
			sBuf += '\\'; 
			sBuf += '{';
			pData++;
			break;

		case '}':
			sBuf += '\\';
			sBuf += '}';
			pData++;
			break;

		case '~':
			sBuf += '\\';
			sBuf += '~';
			sBuf += '{';
			sBuf += '}';
			pData++;
			break;

		case '^':
			sBuf += '\\';
			sBuf += '^';
			sBuf += '{';
			sBuf += '}';
			pData++;
			break;

		case 34:
			(m_bBetweenQuotes = !m_bBetweenQuotes)? sBuf += "{``}" : sBuf += "''";
			pData++;
			break;

		case UCS_LF:					// LF -- representing a Forced-Line-Break
			sBuf += '\\';
			sBuf += '\\';
			pData++;
			break;

		case UCS_VTAB:					// VTAB -- representing a Forced-Column-Break -- TODO
			pData++;
			break;
			
		case UCS_FF:					// FF -- representing a Forced-Page-Break
			sBuf += '\\';
			sBuf += 'n';
			sBuf += 'e';
			sBuf += 'w';
			sBuf += 'p';
			sBuf += 'a';
			sBuf += 'g';
			sBuf += 'e';
			sBuf += '\n';
			pData++;
			break;
			
			
		default:
			int translated =  wvConvertUnicodeToLaTeX(*pData,subst);
			if (translated) 
			{
				while (*subst)
					sBuf += *subst++;
				pData++;
			}
			else 
			{
				char buf[30];
				int len;
				if (m_wctomb.wctomb(buf,len,*pData++)) {
				    for(int i=0;i<len;++i)
						sBuf += buf[i];
				};
			}
			break;
		}
	}

	m_pie->write(sBuf.c_str(),sBuf.size());
}

#define SUB(a,who) case a: subst = "\\(\\" who"\\)"; return true;
#define SUBd(a,who) case a: subst = who; return true;
static bool _convertLettersToSymbols(char c, const char *& subst)
{
	switch (c)
	{
		// only-if-amssymb
// 		SUB('\\', "therefore");

		SUB('\"', "forall");    SUB('$', "exists");
		SUB('\'', "ni");        SUB('@', "cong");
		SUB('^', "perp");       SUB('`', "overline{\\ }");
		SUB('a', "alpha");      SUBd('A', "A");
		SUB('b', "beta"); 	    SUBd('B', "B");
		SUB('c', "chi");  	    SUBd('C', "X");
		SUB('d', "delta");	    SUB('D', "Delta");
		SUB('e', "varepsilon"); SUBd('E', "E");
		SUB('f', "phi");  	    SUB('F', "Phi");
		SUB('g', "gamma");	    SUB('G', "Gamma");
		SUB('h', "eta");	    SUBd('H', "H");
		SUB('i', "iota"); 	    SUBd('I', "I"); 
		SUB('j', "varphi");     SUB('J', "vartheta");
		SUB('k', "kappa"); 	    SUBd('K', "K");
		SUB('l', "lambda");	    SUB('L', "Lambda");
		SUB('m', "mu");    	    SUBd('M', "M");
		SUB('n', "nu");    	    SUBd('N', "N");
		SUBd('o', "o");    	    SUBd('O', "O");
		SUB('p', "pi");    	    SUB('P', "Pi");
		SUB('q', "theta"); 	    SUB('Q', "Theta");
		SUB('r', "rho");   	    SUBd('R', "P");
		SUB('s', "sigma"); 	    SUB('S', "Sigma");
		SUB('t', "tau");   	    SUBd('T', "T");
		SUB('u', "upsilon");    SUBd('U', "Y");
 		SUB('v', "varpi");		SUB('V', "varsigma");
		SUB('w', "omega");      SUB('W', "Omega");
		SUB('x', "xi");         SUB('X', "Xi");
		SUB('y', "psi");        SUB('Y', "Psi");
		SUB('z', "zeta");       SUBd('Z', "Z");
// TODO all those fun upper-ascii letters
	default: return false;
	}
}

// _outputBabelPackage should be called only by the constructer, and only once
void s_LaTeX_Listener::_outputBabelPackage(void)
{
	// Language appears in <abiword> as property "lang",
	// es-ES, en-US, and so forth...
	
	const gchar * szLangCode = nullptr;
	m_pDocument->getAttrProp()->getProperty("lang", szLangCode); // language code
	if(szLangCode && *szLangCode)
	{
	    UT_Language lang;
	    UT_uint32 indx = lang.getIndxFromCode(szLangCode);
	    if (indx > 0)
	    {
		char *strLangName = g_strdup(lang.getNthLangCode(indx)); // language name
		if (strLangName)
		{
		    m_pie->write("%% Please revise the following command, if your babel\n");
		    m_pie->write("%% package does not support ");
		    m_pie->write(strLangName);
		    m_pie->write("\n");
		    
		    *strLangName = static_cast<char>(tolower(static_cast<unsigned char>(*strLangName)));
		    
		    const char *q = strtok(strLangName, "-@"); // retrieve the "significant" part
		    if (strcmp(q, "fr") == 0)
			q="frenchb"; // frenchb.ldf
		    else if (strcmp(q, "de") == 0)
			q="germanb"; // germanb.ldf
		    else if (strcmp(q, "pt") == 0)
			q="portuges"; // portuges.ldf
		    else if (strcmp(q, "ru") == 0)
			q="russianb"; // russianb.ldf
		    else if (strcmp(q, "sl") == 0)
			q="slovene"; // slovene.ldf
		    else if (strcmp(q, "uk") == 0)
			q="ukraineb"; // ukraineb.ldf
		    
		    m_pie->write("\\usepackage[");
		    m_pie->write(q);
		    m_pie->write("]{babel}\n");
		    
		    g_free(strLangName);
		}
	    }
	    
	}
}
s_LaTeX_Listener::s_LaTeX_Listener(PD_Document * pDocument, IE_Exp_LaTeX * pie, 
				    const LaTeX_Analysis_Listener& analysis)
  : m_pDocument(pDocument),
	m_pie(pie),
	m_bInBlock(false),
	m_bInCell(false),
	m_bInSection(false),
	m_bInSpan(false),
	m_bInList(false),
	m_bInScript(false),
	m_bInHeading(false),
	m_bInFootnote(false),
	m_bBetweenQuotes(false),
	m_pAP_Span(nullptr),
	m_bMultiCols(false),
	m_bInSymbol(0),
	m_bInEndnote(false),
	m_bHaveEndnote(analysis.m_hasEndnotes),
	m_bOverline(false),
	m_eJustification(JUSTIFIED),
	m_bLineHeight(false),
	ChapterNumber(0),
	m_DefaultFontSize(12),
	m_Indent(0),
	m_NumCloseBrackets(0),
	m_TableWidth(0),
	m_CellLeft(0),
	m_CellRight(0),
	m_CellTop(0),
	m_CellBot(0),
	list_type(BULLETED_LIST),
	m_iBlockType(0),
	m_pTableHelper(nullptr),
	m_RowNuminTable(0),
	m_ExpectedLeft(0),
	m_pqRect(nullptr),
	m_index(0)
{
	m_pie->write("%% ================================================================================\n");
	m_pie->write("%% This LaTeX file was created by Abinova.                                         \n");
	m_pie->write("%% Abinova is a free, Open Source word processor.                                  \n");
	m_pie->write("% More information about Abinova is available at https://github.com/janos-szenfner/Abinova \n");
	m_pie->write("%% ================================================================================\n");
	m_pie->write("\n");

	// If (documentclass == book), numbered headings begin with x.y.
	// If (documentclass == article), there are no chapter headings.
	// We redefine a "chapter" as a section*.

	m_pie->write("\\documentclass[");
	
	fp_PageSize::Predefined ps = pDocument->m_docPageSize.NameToPredefined(pDocument->m_docPageSize.getPredefinedName());
	switch(ps)
	{
	    case fp_PageSize::psA4:
		    m_pie->write("a4paper");
		    break;
	    case fp_PageSize::psA5:
		    m_pie->write("a5paper");
		    break;
	    case fp_PageSize::psB5:
		    m_pie->write("b5paper");
		    break;
	    case fp_PageSize::psLegal:
		    m_pie->write("legalpaper");
		    break;
	    case fp_PageSize::psLetter:
	    default:
		    m_pie->write("letterpaper");
		    break;
	}
	
	if(pDocument->m_docPageSize.isPortrait())
	    m_pie->write(",portrait");
	else
	    m_pie->write(",landscape");
	
	//retrieve the actual font size
	PD_Style * pStyle = nullptr;
	pDocument->getStyle ("Normal", &pStyle);
	if(pStyle)
	{
	    const gchar * szValue = nullptr;
		pStyle->getProperty("font-size", szValue);
		if (szValue)
		{
			// rounding
			m_DefaultFontSize = int(0.5 + UT_convertToPoints(szValue));
			if (m_DefaultFontSize <= 10)
			{
				m_DefaultFontSize = 10;
				m_pie->write(",10pt");
			}
			else if (m_DefaultFontSize <= 11)
			{
				m_DefaultFontSize = 11;
				m_pie->write(",11pt");
			}
		}
	}
	if (m_DefaultFontSize == 12)
		m_pie->write(",12pt");

	m_pie->write("]{article}\n");
	// Better for ISO-8859-1 than previous: [T1] doesn't work very well
	// TODO: Use inputenc from .abw.
	m_pie->write("\\usepackage[latin1]{inputenc}\n");
	m_pie->write("\\usepackage{calc}\n");
	m_pie->write("\\usepackage{setspace}\n");
	m_pie->write("\\usepackage{fixltx2e}\n");  // for \textsubscript
	m_pie->write("\\usepackage{graphicx}\n");
	m_pie->write("\\usepackage{multicol}\n");
	m_pie->write("\\usepackage[normalem]{ulem}\n");

	_outputBabelPackage();
	
	m_pie->write("\\usepackage{color}\n");

	if (m_bHaveEndnote)
		m_pie->write("\\usepackage{endnotes}\n");

	if (analysis.m_hasTable && analysis.m_hasMultiRow)
	{
	    m_pie->write("\\usepackage{multirow}\n");
	    m_pqRect = new std::deque<UT_Rect*>;
	}
	// Must be as late as possible.
	m_pie->write("\\usepackage{hyperref}\n");

	{
	    const char* misc = XAP_EncodingManager::get_instance()->getTexPrologue();
	    if (misc)
		m_pie->write(misc);
	}
	m_pie->write("\n");
	ChapterNumber = 1;
	m_pie->write("\\begin{document}\n\n");
	
	m_pTableHelper = new ie_Table(pDocument);
}

s_LaTeX_Listener::~s_LaTeX_Listener()
{
	//if (!m_bInFootnote) return;
	_closeSection();
	_handleDataItems();
	DELETEP(m_pTableHelper);
	if(m_pqRect)
	{
	    for(unsigned int i=0; i<m_pqRect->size(); i++)
	    {
		delete m_pqRect->at(i);
		m_pqRect->at(i) = nullptr;
	    }
	    delete m_pqRect;
	}
	if (m_bHaveEndnote)
		m_pie->write("\n\\theendnotes");
	m_pie->write("\n\\end{document}\n");
}

bool s_LaTeX_Listener::populate(fl_ContainerLayout* /*sfh*/,
								   const PX_ChangeRecord * pcr)
{
	
	
	switch (pcr->getType())
	{
	case PX_ChangeRecord::PXT_InsertSpan:
		{
			const PX_ChangeRecord_Span * pcrs = static_cast<const PX_ChangeRecord_Span *> (pcr);

			PT_AttrPropIndex api = pcr->getIndexAP();
			if (api)
			{
				_openSpan(api);
			}
			
			PT_BufIndex bi = pcrs->getBufIndex();
			_outputData(m_pDocument->getPointer(bi),pcrs->getLength());

			if (api)
				_closeSpan();
			return true;
		}

	case PX_ChangeRecord::PXT_InsertObject:
		{
			PT_AttrPropIndex api = pcr->getIndexAP();
			const PX_ChangeRecord_Object * pcro = static_cast<const PX_ChangeRecord_Object *> (pcr);

			const PP_AttrProp * pAP = nullptr;
			bool bHaveProp = m_pDocument->getAttrProp(api,&pAP);

			const gchar* szValue = nullptr;

			fd_Field* field = nullptr;

			switch (pcro->getObjectType())
			{
			case PTO_Image:
				// LaTeX assumes images are EPS.
				// PDFLaTeX assumes images are PNG.
				// Currently we can create PNG images only
				// TODO: is it possible to create EPS images after the cairo integration? 
				if(bHaveProp)
					_handleImage(pAP);
				return true;

			case PTO_Field:

			  field = pcro->getField();
			  if(field->getValue())
			      m_pie->write(field->getValue());

				// we do nothing with computed fields.
				return true;

			case PTO_Hyperlink:
				_closeSpan () ;
				if(m_bInHeading) return true;
				if(bHaveProp && pAP && pAP->getAttribute("xlink:href", szValue))
				{
					m_pie->write("\\href{");
					m_pie->write(szValue);
					m_pie->write("}{");
				}
				else
				{
					m_pie->write("}");
				}
				return true;

			case PTO_Bookmark:
				if(m_bInHeading) return true;
				if(bHaveProp && pAP && pAP->getAttribute("type", szValue))
				{
					if(0 == strcmp("start",szValue))
					{
						if(pAP->getAttribute("name", szValue))
						{
							m_pie->write("\\hypertarget{");
							m_pie->write(szValue);
							m_pie->write("}{");
						}
					}
					else if(0 == strcmp("end",szValue)) m_pie->write("}");
				}
				else
				{
					m_pie->write("}");
				}
				return true;
				
			  case PTO_Math:
				_closeSpan () ;
				if(bHaveProp && pAP)
				{
					UT_UTF8String sLatex;
					UT_ConstByteBufPtr pByteBuf;
					UT_UCS4_mbtowc myWC;
					
					if(pAP->getAttribute("latexid", szValue) &&	szValue 
							&& *szValue)
					{					
						bool bFoundLatex = m_pDocument->getDataItemDataByName(szValue, 
						    pByteBuf,
						    nullptr, nullptr);
						if(!bFoundLatex)
						{
							UT_DEBUGMSG(("Equation %s not found in document \n", szValue));
							return true;
						}
						sLatex.appendBuf(pByteBuf, myWC);
						
						m_pie->write("$");
						m_pie->write(sLatex.utf8_str());
						m_pie->write("$");
					}
#ifdef HAVE_LIBXSLT
					else if(pAP->getAttribute("dataid", szValue) &&	szValue 
							&& *szValue)
					{
						UT_UTF8String sMathML;
						bool bFoundMathML = m_pDocument->getDataItemDataByName(szValue, 
						    pByteBuf,
						    nullptr, nullptr);
						if(!bFoundMathML)
						{
							UT_DEBUGMSG(("Equation %s not found in document \n", szValue));
							return true;
						}
						
						sMathML.appendBuf(pByteBuf, myWC);

						if(!convertMathMLtoLaTeX(sMathML, sLatex))
							return true;		
						/*The converted sLatex already contains $s*/
						m_pie->write(sLatex.utf8_str());
					}
#endif
					else
						return true;
				}
				return true;

			default:
				UT_ASSERT_HARMLESS(0);
				return true;
			}
		}

	case PX_ChangeRecord::PXT_InsertFmtMark:
		return true;
		
	default:
		UT_ASSERT_HARMLESS(0);
		return false;
	}
}

bool s_LaTeX_Listener::populateStrux(pf_Frag_Strux* sdh,
										   const PX_ChangeRecord * pcr,
										   fl_ContainerLayout* * psfh)
{
	UT_ASSERT(pcr->getType() == PX_ChangeRecord::PXT_InsertStrux);
	const PX_ChangeRecord_Strux * pcrx = static_cast<const PX_ChangeRecord_Strux *> (pcr);
	*psfh = nullptr;						// we don't need it.

	switch (pcrx->getStruxType())
	{
	case PTX_Section:
	{
		_closeSection();

		PT_AttrPropIndex indexAP = pcr->getIndexAP();
		const PP_AttrProp* pAP = nullptr;
		if (m_pDocument->getAttrProp(indexAP, &pAP) && pAP)
		{
			const gchar* pszSectionType = nullptr;
			pAP->getAttribute("type", pszSectionType);
			if (
				!pszSectionType
				|| (0 == strcmp(pszSectionType, "doc"))
				)
			{
				_openSection(pcr->getIndexAP());
				m_bInSection = true;
			}
			else
			{
				m_bInSection = false;
			}
		}
		else
		{
			m_bInSection = false;
		}
		
		return true;
	}

	case PTX_SectionHdrFtr:
	{
		_closeSection();

		PT_AttrPropIndex indexAP = pcr->getIndexAP();
		const PP_AttrProp* pAP = nullptr;
		if (m_pDocument->getAttrProp(indexAP, &pAP) && pAP)
		{
			const gchar* pszSectionType = nullptr;
			pAP->getAttribute("type", pszSectionType);
			if (
				!pszSectionType
				|| (0 == strcmp(pszSectionType, "doc"))
				)
			{
				_openSection(pcr->getIndexAP());
				m_bInSection = true;
			}
			else
			{
				m_bInSection = false;
			}
		}
		else
		{
			m_bInSection = false;
		}
		
		return true;
	}

	case PTX_Block:
	{
		_closeBlock();
		_closeParagraph();
		_openParagraph(pcr->getIndexAP());
		return true;
	}

	case PTX_SectionTable:
	{
		m_pTableHelper->openTable(sdh, pcr->getIndexAP());
		m_TableWidth = m_pTableHelper->getNumCols();
		_openTable(pcr->getIndexAP());
		return true;
	}

	case PTX_EndTable:
	{
		_closeTable();
		m_pTableHelper->closeTable();
		return true;
	}

	case PTX_SectionCell:
	{
		_openCell(pcr->getIndexAP());
		return true;
	}

	case PTX_EndCell:
	{
		_closeCell();
		return true;
	}

	case PTX_EndFrame:
	case PTX_EndMarginnote:
	case PTX_EndFootnote:
	{
		m_bInFootnote = false;
		m_pie->write("} ");
		return true;
	}

	case PTX_SectionFrame:
	case PTX_SectionMarginnote:
	case PTX_SectionFootnote:
	{
		m_bInFootnote = true;
		m_pie->write("\\footnote{");
		return true;
	}
	
	case PTX_SectionTOC:
	{
		_closeBlock();
		/*
		_closeSection();
		 */
		m_pie->write("\\tableofcontents \n");
		return true;
	}
	case PTX_EndTOC:
		return true;
	
	case PTX_SectionEndnote:
	{
		m_bInEndnote = true;
		m_pie->write("\\endnote{");
		return true;
	}
	case PTX_EndEndnote:
	{
		m_bInEndnote = false;
		m_pie->write("} ");
		return true;
	}
	default:
		UT_ASSERT(UT_TODO);
		return true;
	}
}

bool s_LaTeX_Listener::change(fl_ContainerLayout* /*sfh*/,
									const PX_ChangeRecord * /*pcr*/)
{
	UT_ASSERT(0);						// this function is not used.
	return false;
}

bool s_LaTeX_Listener::insertStrux(fl_ContainerLayout* /*sfh*/,
									 const PX_ChangeRecord * /*pcr*/,
									 pf_Frag_Strux* /*sdh*/,
									 PL_ListenerId /* lid */,
									 void (* /*pfnBindHandles*/)(pf_Frag_Strux* /* sdhNew */,
																 PL_ListenerId /* lid */,
																 fl_ContainerLayout* /* sfhNew */))
{
	UT_ASSERT(0);						// this function is not used.
	return false;
}

bool s_LaTeX_Listener::signal(UT_uint32 /* iSignal */)
{
	UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
	return false;
}


/*****************************************************************/
/*****************************************************************/

UT_Error IE_Exp_LaTeX::_writeDocument(void)
{
	LaTeX_Analysis_Listener analysis(getDoc(), this);
	if (!getDoc()->tellListener(&analysis))
		return UT_ERROR;

	m_pListener = new s_LaTeX_Listener(getDoc(),this, analysis);
	if (!m_pListener)
		return UT_IE_NOMEMORY;
	if (!getDoc()->tellListener(static_cast<PL_Listener *>(m_pListener)))
		return UT_ERROR;
	delete m_pListener;

	m_pListener = nullptr;
	
	return ((m_error) ? UT_IE_COULDNOTWRITE : UT_OK);
}

/*****************************************************************/
/*****************************************************************/

void s_LaTeX_Listener::_handleDataItems(void)
{
}



void s_LaTeX_Listener::_handleImage(const PP_AttrProp * pAP)
{	
	/* Part of code taken from the HTML exporter */
	UT_ConstByteBufPtr pByteBuf;
	const gchar *szHeight = nullptr, *szWidth = nullptr, *szDataID = nullptr;
    std::string mimeType;
	
	if (! pAP)
		return;
	if (! pAP->getAttribute("dataid", szDataID))
		return;

	if(!m_pDocument->getDataItemDataByName(szDataID, pByteBuf,
                                             &mimeType, nullptr))
		return;
	if ((pByteBuf == nullptr) || (mimeType.empty()))
        return; // ??

    const char * extension = ".png";
    if(mimeType == "image/jpeg")
    {
        extension = ".jpg";
    }
	else if (mimeType != "image/png")
	{
		UT_DEBUGMSG(("Object not of MIME type image/png or image/jpeg but %s - ignoring...\n", mimeType.c_str()));
		return;
	}

	gchar *imagedir = UT_go_dirname_from_uri(m_pie->getFileName(), true);
	
    /* szDataID is document-controlled; sanitize so the emitted
	 * \includegraphics name stays a flat filename matching the
	 * sanitized file writeBufferToFile() actually creates */
    std::string filename = UT_sanitizeFileName(szDataID);
	filename += extension;
	
	/* save the image as imagedir/filename */
	IE_Exp::writeBufferToFile(pByteBuf, imagedir, filename);
	FREEP(imagedir);
	
	m_pie->write("\\includegraphics");
	if (pAP->getProperty("height", szHeight) && pAP->getProperty("width", szWidth))
	{
		m_pie->write("[height=");
		m_pie->write(szHeight);
		m_pie->write(",width=");
		m_pie->write(szWidth);
		m_pie->write("]");
	}

	m_pie->write("{");
	m_pie->write(filename.c_str());
	m_pie->write("}\n");

	return;	
}
/*
  This is a copy from wv. Returns 1 if was translated, 0 if wasn't.
  It can convert to empty string.

  Historically this was a ~1100-line switch whose cases wrote through a
  "#define printf(x) out = (x);" hack; it is now a sorted lookup table.
*/
struct s_U2LxMapping
{
	U16			code;
	const char *	repl;
};

static const s_U2LxMapping s_UnicodeToLaTeX[] = {
	{ 0x0007, "" },
	{ 0x000b, "\\\\\n" },
	{ 0x000c, "" },
	{ 0x000d, "" },
	{ 0x000e, "" },
	{ 0x001e, "" },
	{ 0x001f, "" },
	{ 0x0022, "\"" },
	{ 0x0023, "\\#" },
	{ 0x0024, "\\$" },
	{ 0x0025, "\\%%" },
	{ 0x0026, "\\&" },
	{ 0x002d, "-" },
	{ 0x003c, "$<$" },
	{ 0x003e, "$>$" },
	{ 0x00a1, "!`" },
	{ 0x00b1, "$\\pm$" },
	{ 0x00b2, "$\\mathtwosuperior$" },
	{ 0x00b3, "$\\maththreesuperior$" },
	{ 0x00b5, "$\\mu$" },
	{ 0x00b9, "$\\mathonesuperior$" },
	{ 0x00bf, "?`" },
	{ 0x00c0, "\\`{A}" },
	{ 0x00c1, "\\\'{A}" },
	{ 0x00c2, "\\^{A}" },
	{ 0x00c3, "\\~{A}" },
	{ 0x00c4, "\\\"{A}" },
	{ 0x00c7, "\\c{C}" },
	{ 0x00c8, "\\`{E}" },
	{ 0x00c9, "\\\'{E}" },
	{ 0x00ca, "\\^{E}" },
	{ 0x00cb, "\\\"{E}" },
	{ 0x00cd, "\\\'{I}" },
	{ 0x00ce, "\\^{I}" },
	{ 0x00d1, "\\~{N}" },
	{ 0x00d3, "\\\'{O}" },
	{ 0x00d4, "\\^{O}" },
	{ 0x00d5, "\\~{O}" },
	{ 0x00d6, "\\\"{O}" },
	{ 0x00d8, "{\\O}" },
	{ 0x00da, "\\\'{U}" },
	{ 0x00db, "\\^{U}" },
	{ 0x00dc, "\\ldots{}" },
	{ 0x00dd, "\\\'{Y}" },
	{ 0x00df, "\\ss{}" },
	{ 0x00e0, "\\`{a}" },
	{ 0x00e1, "\\\'{a}" },
	{ 0x00e2, "\\^{a}" },
	{ 0x00e3, "\\~{a}" },
	{ 0x00e4, "\\\"{a}" },
	{ 0x00e7, "\\c{c}" },
	{ 0x00e8, "\\`{e}" },
	{ 0x00e9, "\\\'{e}" },
	{ 0x00ea, "\\^{e}" },
	{ 0x00eb, "\\\"{e}" },
	{ 0x00ed, "\\\'{i}" },
	{ 0x00ee, "\\^{i}" },
	{ 0x00f1, "\\~{n}" },
	{ 0x00f2, "\\`{o}" },
	{ 0x00f3, "\\\'{o}" },
	{ 0x00f4, "\\^{o}" },
	{ 0x00f5, "\\~{o}" },
	{ 0x00f6, "\\\"{o}" },
	{ 0x00f8, "{\\o}" },
	{ 0x00fa, "\\\'{u}" },
	{ 0x00fb, "\\^{u}" },
	{ 0x00fc, "\\\"{u}" },
	{ 0x00fd, "\\\'{y}" },
	{ 0x0100, "\\=A" },
	{ 0x0101, "\\=a" },
	{ 0x0102, "\\u{A}" },
	{ 0x0103, "\\u{a}" },
	{ 0x0106, "\\'C" },
	{ 0x0107, "\\'c" },
	{ 0x0108, "\\^C" },
	{ 0x0109, "\\^c" },
	{ 0x010a, "\\.C" },
	{ 0x010b, "\\.c" },
	{ 0x010c, "\\v{C}" },
	{ 0x010d, "\\v{c}" },
	{ 0x010e, "\\v{D}" },
	{ 0x010f, "\\v{d}" },
	{ 0x0110, "\\DJ{}" },
	{ 0x0111, "\\dj{}" },
	{ 0x0112, "\\=E" },
	{ 0x0113, "\\=e" },
	{ 0x0114, "\\u{E}" },
	{ 0x0115, "\\u{e}" },
	{ 0x0116, "\\.E" },
	{ 0x0117, "\\.e" },
	{ 0x011a, "\\v{E}" },
	{ 0x011b, "\\v{e}" },
	{ 0x011c, "\\^G" },
	{ 0x011d, "\\^g" },
	{ 0x011e, "\\u{G}" },
	{ 0x011f, "\\u{g}" },
	{ 0x0120, "\\.G" },
	{ 0x0121, "\\u{g}" },
	{ 0x0122, "^H" },
	{ 0x0123, "^h" },
	{ 0x0128, "\\~I" },
	{ 0x0129, "\\~{\\i}" },
	{ 0x012a, "\\=I" },
	{ 0x012b, "\\={\\i}" },
	{ 0x012c, "\\u{I}" },
	{ 0x012d, "\\u{\\i}" },
	{ 0x0130, "\\.I" },
	{ 0x0131, "\\i{}" },
	{ 0x0132, "IJ" },
	{ 0x0133, "ij" },
	{ 0x0134, "\\^J" },
	{ 0x0135, "\\^{\\j}" },
	{ 0x0136, "\\c{K}" },
	{ 0x0137, "\\c{k}" },
	{ 0x0138, "k" },
	{ 0x0139, "\\'L" },
	{ 0x013a, "\\'l" },
	{ 0x013b, "\\c{L}" },
	{ 0x013c, "\\c{l}" },
	{ 0x013d, "\\v{L}" },
	{ 0x013e, "\\v{l}" },
	{ 0x0141, "\\L{}" },
	{ 0x0142, "\\l{}" },
	{ 0x0143, "\\'N" },
	{ 0x0144, "\\'n" },
	{ 0x0145, "\\c{N}" },
	{ 0x0146, "\\c{n}" },
	{ 0x0147, "\\v{N}" },
	{ 0x0148, "\\v{n}" },
	{ 0x0149, "'n" },
	{ 0x014a, "\\NG{}" },
	{ 0x014b, "\\ng{}" },
	{ 0x014c, "\\=O" },
	{ 0x014d, "\\=o" },
	{ 0x014e, "\\u{O}" },
	{ 0x014f, "\\u{o}" },
	{ 0x0150, "\\H{O}" },
	{ 0x0151, "\\H{o}" },
	{ 0x0152, "\\OE{}" },
	{ 0x0153, "\\oe{}" },
	{ 0x0154, "\\'R" },
	{ 0x0155, "\\'r" },
	{ 0x0156, "\\c{R}" },
	{ 0x0157, "\\c{r}" },
	{ 0x0158, "\\v{R}" },
	{ 0x0159, "\\v{r}" },
	{ 0x015a, "\\'S" },
	{ 0x015b, "\\'s" },
	{ 0x015c, "\\^S" },
	{ 0x015d, "\\^s" },
	{ 0x015e, "\\c{S}" },
	{ 0x015f, "\\c{s}" },
	{ 0x0160, "\\v{S}" },
	{ 0x0161, "\\v{s}" },
	{ 0x0162, "\\c{T}" },
	{ 0x0163, "\\c{t}" },
	{ 0x0164, "\\v{T}" },
	{ 0x0165, "\\v{t}" },
	{ 0x0168, "\\~U" },
	{ 0x0169, "\\~u" },
	{ 0x016a, "\\=U" },
	{ 0x016b, "\\=u" },
	{ 0x016c, "\\u{U}" },
	{ 0x016d, "\\u{u}" },
	{ 0x016e, "\\r{U}" },
	{ 0x016f, "\\r{u}" },
	{ 0x0170, "\\H{U}" },
	{ 0x0171, "\\H{u}" },
	{ 0x0174, "\\^W" },
	{ 0x0175, "\\^w" },
	{ 0x0176, "\\^Y" },
	{ 0x0177, "\\^y" },
	{ 0x0178, "\\\"Y" },
	{ 0x0179, "\\'Z" },
	{ 0x017a, "\\'z" },
	{ 0x017b, "\\.Z" },
	{ 0x017c, "\\.z" },
	{ 0x017d, "\\v{Z}" },
	{ 0x017e, "\\v{z}" },
	{ 0x01c7, "LJ" },
	{ 0x01c8, "Lj" },
	{ 0x01c9, "lj" },
	{ 0x01ca, "NJ" },
	{ 0x01cb, "Nj" },
	{ 0x01cc, "nj" },
	{ 0x01cd, "\\v{A}" },
	{ 0x01ce, "\\v{a}" },
	{ 0x01cf, "\\v{I}" },
	{ 0x01d0, "\\v{\\i}" },
	{ 0x01d1, "\\v{O}" },
	{ 0x01d2, "\\v{o}" },
	{ 0x01d3, "\\v{U}" },
	{ 0x01d4, "\\v{u}" },
	{ 0x01e6, "\\v{G}" },
	{ 0x01e7, "\\v{g}" },
	{ 0x01e8, "\\v{K}" },
	{ 0x01e9, "\\v{k}" },
	{ 0x01f0, "\\v{\\j}" },
	{ 0x01f1, "DZ" },
	{ 0x01f2, "Dz" },
	{ 0x01f3, "dz" },
	{ 0x01f4, "\\'G" },
	{ 0x01f5, "\\'g" },
	{ 0x01fa, "\\'{\\AA}" },
	{ 0x01fb, "\\'{\\aa}" },
	{ 0x01fc, "\\'{\\AE}" },
	{ 0x01fd, "\\'{\\ae}" },
	{ 0x01fe, "\\'{\\O}" },
	{ 0x01ff, "\\'{\\o}" },
	{ 0x0391, "$\\Alpha$" },
	{ 0x0392, "$\\Beta$" },
	{ 0x0393, "$\\Gamma$" },
	{ 0x0394, "$\\Delta$" },
	{ 0x0395, "$\\Epsilon$" },
	{ 0x0396, "$\\Zeta$" },
	{ 0x0397, "$\\Eta$" },
	{ 0x0398, "$\\Theta$" },
	{ 0x0399, "$\\Iota$" },
	{ 0x039a, "$\\Kappa$" },
	{ 0x039b, "$\\Lambda$" },
	{ 0x039c, "$\\Mu$" },
	{ 0x039d, "$\\Nu$" },
	{ 0x039e, "$\\Xi$" },
	{ 0x039f, "$\\Omicron$" },
	{ 0x03a0, "$\\Pi$" },
	{ 0x03a1, "$\\Rho$" },
	{ 0x03a3, "$\\Sigma$" },
	{ 0x03a4, "$\\Tau$" },
	{ 0x03a5, "$\\Upsilon$" },
	{ 0x03a6, "$\\Phi$" },
	{ 0x03a7, "$\\Chi$" },
	{ 0x03a8, "$\\Psi$" },
	{ 0x03a9, "$\\Omega$" },
	{ 0x03b1, "$\\alpha$" },
	{ 0x03b2, "$\\beta$" },
	{ 0x03b3, "$\\gamma$" },
	{ 0x03b4, "$\\delta$" },
	{ 0x03b5, "$\\epsilon$" },
	{ 0x03b6, "$\\zeta$" },
	{ 0x03b7, "$\\eta$" },
	{ 0x03b8, "$\\theta$" },
	{ 0x03b9, "$\\iota$" },
	{ 0x03ba, "$\\kappa$" },
	{ 0x03bb, "$\\lambda$" },
	{ 0x03bc, "$\\mu$" },
	{ 0x03bd, "$\\nu$" },
	{ 0x03be, "$\\xi$" },
	{ 0x03bf, "$\\omicron$" },
	{ 0x03c0, "$\\pi$" },
	{ 0x03c1, "$\\rho$" },
	{ 0x03c3, "$\\sigma$" },
	{ 0x03c4, "$\\tau$" },
	{ 0x03c5, "$\\upsilon$" },
	{ 0x03c6, "$\\phi$" },
	{ 0x03c7, "$\\chi$" },
	{ 0x03c8, "$\\psi$" },
	{ 0x03c9, "$\\omega$" },
	{ 0x2010, "-" },
	{ 0x2011, "-" },
	{ 0x2012, "--" },
	{ 0x2013, "--" },
	{ 0x2014, "---" },
	{ 0x2018, "{`}" },
	{ 0x2019, "'" },
	{ 0x201a, "\\quotesinglbase{}" },
	{ 0x201c, "{``}" },
	{ 0x201d, "''" },
	{ 0x201e, "\\quotedblbase{}" },
	{ 0x2020, "\\dag{}" },
	{ 0x2021, "\\ddag{}" },
	{ 0x2022, "$\\bullet$" },
	{ 0x2023, "$\\bullet$" },
	{ 0x2024, "." },
	{ 0x2025, ".." },
	{ 0x2026, "\\ldots{}" },
	{ 0x2030, "o/oo" },
	{ 0x2039, "\\guilsinglleft{}" },
	{ 0x203a, "\\guilsinglright{}" },
	{ 0x203c, "!!" },
	{ 0x20ac, "\\euro" },
	{ 0x2111, "$\\Im$" },
	{ 0x2118, "$\\wp$" },
	{ 0x211c, "$\\Re$" },
	{ 0x2135, "$\\aleph$" },
	{ 0x2160, "I" },
	{ 0x2161, "II" },
	{ 0x2162, "III" },
	{ 0x2163, "IV" },
	{ 0x2164, "V" },
	{ 0x2165, "VI" },
	{ 0x2166, "VII" },
	{ 0x2167, "VIII" },
	{ 0x2168, "IX" },
	{ 0x2169, "X" },
	{ 0x216a, "XI" },
	{ 0x216b, "XII" },
	{ 0x216c, "L" },
	{ 0x216d, "C" },
	{ 0x216e, "D" },
	{ 0x216f, "M" },
	{ 0x2170, "i" },
	{ 0x2171, "ii" },
	{ 0x2172, "iii" },
	{ 0x2173, "iv" },
	{ 0x2174, "v" },
	{ 0x2175, "vi" },
	{ 0x2176, "vii" },
	{ 0x2177, "viii" },
	{ 0x2178, "ix" },
	{ 0x2179, "x" },
	{ 0x217a, "xi" },
	{ 0x217b, "xiii" },
	{ 0x217c, "l" },
	{ 0x217d, "c" },
	{ 0x217e, "d" },
	{ 0x217f, "m" },
	{ 0x2190, "$\\leftarrow$" },
	{ 0x2191, "$\\uparrow$" },
	{ 0x2192, "$\\rightarrow$" },
	{ 0x2193, "$\\downarrow$" },
	{ 0x21d0, "$\\Leftarrow$" },
	{ 0x21d1, "$\\Uparrow$" },
	{ 0x21d2, "$\\Rightarrow$" },
	{ 0x21d3, "$\\Downarrow$" },
	{ 0x21d4, "$\\Leftrightarrow$" },
	{ 0x2200, "$\\forall$" },
	{ 0x2202, "$\\partial$" },
	{ 0x2203, "$\\exists$" },
	{ 0x2205, "$\\emptyset$" },
	{ 0x2207, "$\\nabla$" },
	{ 0x2208, "$\\in$" },
	{ 0x2209, "$\\notin$" },
	{ 0x220b, "$\\ni$" },
	{ 0x2212, "$-$" },
	{ 0x2215, "$/$" },
	{ 0x221a, "$\\surd$" },
	{ 0x221d, "$\\propto$" },
	{ 0x221e, "$\\infty$" },
	{ 0x2220, "$\\angle$" },
	{ 0x2227, "$\\land$" },
	{ 0x2228, "$\\lor$" },
	{ 0x2229, "$\\cap$" },
	{ 0x222a, "$\\cup$" },
	{ 0x223c, "$\\sim$" },
	{ 0x2248, "$\\approx$" },
	{ 0x2260, "$\\neq$" },
	{ 0x2261, "$\\equiv$" },
	{ 0x2264, "$\\leq$" },
	{ 0x2265, "$\\geq$" },
	{ 0x2282, "$\\subset$" },
	{ 0x2283, "$\\supset$" },
	{ 0x2284, "$\\notsubset$" },
	{ 0x2286, "$\\subseteq$" },
	{ 0x2287, "$\\supseteq$" },
	{ 0x2295, "$\\oplus$" },
	{ 0x2297, "$\\otimes$" },
	{ 0x22a5, "$\\perp$" },
	{ 0x2660, "$\\spadesuit$" },
	{ 0x2663, "$\\clubsuit$" },
	{ 0x2665, "$\\heartsuit$" },
	{ 0x2666, "$\\diamondsuit$" },
	{ 0xf8e7, "_" },
};

static int s_cmpU2LxMapping(const void * a, const void * b)
{
	const s_U2LxMapping * e = static_cast<const s_U2LxMapping *>(b);
	return static_cast<int>(*static_cast<const U16 *>(a))
		- static_cast<int>(e->code);
}

static int wvConvertUnicodeToLaTeX(U16 char16,const char*& out)
{
	out = ""; //this is needed

	const s_U2LxMapping * e = static_cast<const s_U2LxMapping *>(
		bsearch(&char16, s_UnicodeToLaTeX,
				sizeof(s_UnicodeToLaTeX) / sizeof(s_UnicodeToLaTeX[0]),
				sizeof(s_UnicodeToLaTeX[0]), &s_cmpU2LxMapping));
	if (!e)
		return 0;
	out = e->repl;
	return 1;
}
