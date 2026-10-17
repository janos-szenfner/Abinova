/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
 * BIDI Copyright (c) 2004, Martin Sevior
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


#include "fp_Run.h"
#include "fp_TableContainer.h"
#include "fl_TableLayout.h"
#include "fv_View.h"
#include "pd_Document.h"
#include "fl_BlockLayout.h"
#include "ut_debugmsg.h"
#include "pd_Document.h"
#include "fp_Line.h"
#include "fp_ContainerObject.h"
#include "fd_Field.h"
#include <string.h>
#include <math.h>
#include <sstream>
#include <vector>
#include <string>
#include <set>
#include <ctype.h>
#include <stdlib.h>

static bool bUseCurrency = false;
static char cCurrency = '$';

static double dGetVal(UT_UTF8String sVal)
{
	std::istringstream iStream(sVal.utf8_str());
	double d;
	iStream >> d;

	// any input succesfully converted?
	if(!iStream)
	{
		return 0;
	}
	// if so, and buffer is not empty yet, check
	// if all remaining characters are whitespaces
	if(iStream.rdbuf()->in_avail() != 0)
	{
		char c;
		do
		{
			iStream.get(c);
			if(!isspace(static_cast<unsigned char>(c)))
			{
				return 0;
			}
		}
		while(iStream.gcount() != 0);
	}

	return d;
}

static void sFormatDouble(UT_UTF8String & sVal, double d)
{
	bool bUseFix2 = bUseCurrency;
	bool bUseInt = false;
	double res = 0.0000000001;
	if(fabs(d) > 0.0000000001)
	{
		res = res*d;
	}
	if(d < 1e9 && !bUseCurrency)
	{
		UT_sint32 iVal = static_cast<UT_sint32>(d);
		if(iVal >= 0)
		{
			double dd = static_cast<double>(iVal);
			if((d - dd) < res)
			{
				bUseInt = true;
			}
			if(!bUseInt)
			{
				dd = static_cast<double>(iVal+1);
				if((dd-d) < res)
				{
					bUseInt = true;
					d += 1.0;
				}
			}
		}
		else
		{
			double dd = static_cast<double>(iVal);
			if((dd -d) < res)
			{
				bUseInt = true;
			}
			if(!bUseInt)
			{
				dd = static_cast<double>(iVal-1);
				if((d - dd) < res)
				{
					bUseInt = true;
					d -= 1;
				}
			}
		}
		if(!bUseInt && !bUseFix2)
		{
			double a = d*100.0;
			double aa = a;
			UT_sint32 iValH = static_cast<UT_sint32>(a);
			if(iValH >= 0)
			{
				aa = static_cast<double>(iValH);
				if((a - aa) < res)
				{
					bUseFix2 = true;
				}
				if(!bUseFix2)
				{
					aa = static_cast<double>(iValH+1);
					if((aa-a) < res)
					{
						bUseFix2 = true;
						a += 1.0;
					}
				}
			}
			else
			{
				aa = static_cast<double>(iValH);
				if((aa -a) < res)
				{
					bUseFix2 = true;
				}
				if(!bUseFix2)
				{
					aa = static_cast<double>(iValH-1);
					if((a - aa) < res)
					{
						bUseFix2 = true;
						a -= 1;
					}
				}
			}
			if(bUseFix2)
			{
				if(fabs(a) < 1e9)
				{
					iValH = static_cast<UT_sint32>(a);
					d = static_cast<double>(iValH)/100.0;
				}
			}
		}
	}
	if(bUseInt)
	{
		UT_sint32 iVal = static_cast<UT_sint32>(d);
		UT_UTF8String_sprintf(sVal,"%d",iVal);
		return;
	}
	if(bUseFix2)
	{
		UT_String sFull("");
		if(bUseCurrency)
		{
			sFull += cCurrency;
		}
		sFull += "%.2f";
		const char * szFmt = sFull.c_str();
		UT_UTF8String_sprintf(sVal,szFmt,d);
		return;
	}
	UT_UTF8String_sprintf(sVal,"%g",d);
	return;
}

	fp_FieldTableSumRows::fp_FieldTableSumRows(fl_BlockLayout* pBL, UT_uint32 iOffsetFirst, UT_uint32 iLen)	: fp_FieldRun(pBL,  iOffsetFirst, iLen)
{
}

bool fp_FieldTableSumRows::calculateValue(void)
{
	FV_View * pView = _getView();
	const pf_Frag_Strux* tableSDH = nullptr;
	UT_sint32 numRows =0;
	UT_sint32 numCols = 0;
	bUseCurrency = false;
	cCurrency = '$';
	pf_Frag_Strux* sdh = getBlock()->getStruxDocHandle();
	PD_Document * pDoc = getBlock()->getDocument();
	if(pDoc->isPieceTableChanging())
	{
		return false;
	}
	if(getLine() == nullptr)
	{
		return false;
	}
	fp_Container * pCol = getLine()->getColumn();
	if(pCol == nullptr)
	{
		return false;
	}
	fp_ShadowContainer * pShad =nullptr;
	fl_HdrFtrShadow * pShadL = nullptr;
	if(pCol->getContainerType() == FP_CONTAINER_COLUMN_SHADOW)
	{
		pShad = static_cast<fp_ShadowContainer *>(pCol);
		pShadL = pShad->getShadow();
	}
	PT_DocPosition pos = pDoc->getStruxPosition(sdh)+1;
	UT_UTF8String sValF;
	if(!pView->isInTable(pos))
	{
		sValF = "???";
		return _setValue(sValF.ucs4_str().ucs4_str());
	}
	if(!pDoc->getStruxOfTypeFromPosition(pos,PTX_SectionTable,&tableSDH) || !tableSDH)
	{
		sValF = "???";
		return _setValue(sValF.ucs4_str().ucs4_str());
	}
	pDoc->getRowsColsFromTableStrux(tableSDH, pView->isShowRevisions(), pView->getRevisionLevel(), &numRows, &numCols);
	fl_CellLayout * pCell = nullptr;
	UT_sint32 myLeft,myRight,myTop,myBot;
	pView->getCellParams(pos,&myLeft,&myRight,&myTop,&myBot);
	UT_sint32 row = 0;
	UT_sint32 col = myLeft;
	UT_sint32 lastRow = -1;
	double dSum = 0.0;
	for(row = 0; row < numRows; row++)
	{
		const pf_Frag_Strux* sdhCell = pDoc->getCellStruxFromRowCol(tableSDH, true, 99999, row, col);
		UT_sint32 i = getBlock()->getDocLayout()->getLID();
		fl_ContainerLayout* fmtCell = pDoc->getNthFmtHandle(sdhCell,i);
		pCell = static_cast<fl_CellLayout *>(fmtCell);
		if(pCell->getTopAttach() == lastRow)
		{
			continue;
		}
		if((pCell->getTopAttach() == myTop) && (pCell->getLeftAttach() == myLeft))
		{
			continue;
		}
		UT_GrowBuf grText;
		pCell->appendTextToBuf(grText);
		if(grText.getLength() == 0)
		{
			fl_ContainerLayout * pC = pCell->getFirstLayout();
			while(pC)
			{
				if(pC->getContainerType() == FL_CONTAINER_BLOCK)
				{
					fl_BlockLayout * pBL = static_cast<fl_BlockLayout *>(pC);
					if(pShadL)
					{
						pBL = static_cast<fl_BlockLayout *>(pShadL->findMatchingContainer(pBL));
					}
					if(pBL == nullptr)
					{
						continue;
					}
					fp_Run * pRun = pBL->getFirstRun();
					while(pRun)
					{
						if(pRun->getType() == FPRUN_FIELD)
						{
							fp_FieldRun * pFRun = static_cast<fp_FieldRun *>(pRun);
							const UT_UCS4Char * szVal = pFRun->getValue(); 
							sValF.clear();
							sValF.appendUCS4(szVal);
							dSum += dGetVal(sValF.utf8_str());
							pRun = nullptr;
							pC = nullptr;
							break;
						}
						pRun = pRun->getNextRun();
					}
				}
				if(pC)
				{
					pC = pC->getNext();
				}
			}
		}
		else
		{
			sValF.clear();
			sValF.appendUCS4(reinterpret_cast<const UT_UCS4Char *>(grText.getPointer(0)),grText.getLength());
			dSum += dGetVal(sValF.utf8_str());
		}
		lastRow = row;
	}
	sFormatDouble(sValF,dSum);
	return _setValue(sValF.ucs4_str().ucs4_str());
}

	fp_FieldTableSumCols::fp_FieldTableSumCols(fl_BlockLayout* pBL, UT_uint32 iOffsetFirst, UT_uint32 iLen) : fp_FieldRun(pBL,  iOffsetFirst, iLen)
{
}

bool fp_FieldTableSumCols::calculateValue(void)
{
	FV_View * pView = _getView();
	const pf_Frag_Strux* tableSDH = nullptr;
	UT_sint32 numRows =0;
	UT_sint32 numCols = 0;
	bUseCurrency = false;
	cCurrency = '$';
	pf_Frag_Strux* sdh = getBlock()->getStruxDocHandle();
	PD_Document * pDoc = getBlock()->getDocument();
	if(pDoc->isPieceTableChanging())
	{
		return false;
	}
	if(getLine() == nullptr)
	{
		return false;
	}
	fp_Container * pCol = getLine()->getColumn();
	if(pCol == nullptr)
	{
		return false;
	}
	fp_ShadowContainer * pShad =nullptr;
	fl_HdrFtrShadow * pShadL = nullptr;
	if(pCol->getContainerType() == FP_CONTAINER_COLUMN_SHADOW)
	{
		pShad = static_cast<fp_ShadowContainer *>(pCol);
		pShadL = pShad->getShadow();
	}
	PT_DocPosition pos = pDoc->getStruxPosition(sdh)+1;
	UT_UTF8String sValF;
	if(!pView->isInTable(pos))
	{
		sValF = "???";
		return _setValue(sValF.ucs4_str().ucs4_str());
	}
	if(!pDoc->getStruxOfTypeFromPosition(pos,PTX_SectionTable,&tableSDH) || !tableSDH)
	{
		sValF = "???";
		return _setValue(sValF.ucs4_str().ucs4_str());
	}
	pDoc->getRowsColsFromTableStrux(tableSDH, pView->isShowRevisions(), pView->getRevisionLevel(), &numRows, &numCols);

	fl_CellLayout * pCell = nullptr;
	UT_sint32 myLeft,myRight,myTop,myBot;
	pView->getCellParams(pos,&myLeft,&myRight,&myTop,&myBot);
	UT_sint32 col = 0;
	UT_sint32 row = myTop;
	UT_sint32 lastCol = -1;
	double dSum = 0.0;
	for(col = 0; col < numCols; col++)
	{
		const pf_Frag_Strux* sdhCell = pDoc->getCellStruxFromRowCol(tableSDH, true, 99999, row, col);
		UT_sint32 i = getBlock()->getDocLayout()->getLID();
		fl_ContainerLayout* fmtCell = pDoc->getNthFmtHandle(sdhCell,i);
		pCell = static_cast<fl_CellLayout *>(fmtCell);
		if(pCell->getLeftAttach() == lastCol)
		{
			continue;
		}
		if((pCell->getTopAttach() == myTop) && (pCell->getLeftAttach() == myLeft))
		{
			continue;
		}
		UT_GrowBuf grText;
		pCell->appendTextToBuf(grText);
		if(grText.getLength() == 0)
		{
			fl_ContainerLayout * pC = pCell->getFirstLayout();
			while(pC)
			{
				if(pC->getContainerType() == FL_CONTAINER_BLOCK)
				{
					fl_BlockLayout * pBL = static_cast<fl_BlockLayout *>(pC);
					if(pShadL)
					{
						pBL = static_cast<fl_BlockLayout *>(pShadL->findMatchingContainer(pBL));
					}
					if(pBL == nullptr)
					{
						continue;
					}
					fp_Run * pRun = pBL->getFirstRun();
					while(pRun)
					{
						if(pRun->getType() == FPRUN_FIELD)
						{
							fp_FieldRun * pFRun = static_cast<fp_FieldRun *>(pRun);
							const  UT_UCS4Char * szVal = pFRun->getValue(); 
							sValF.clear();
							sValF.appendUCS4(szVal);
							dSum += dGetVal(sValF.utf8_str());
							pRun = nullptr;
							pC = nullptr;
							break;
						}
						pRun = pRun->getNextRun();
					}
				}
				if(pC)
				{
					pC = pC->getNext();
				}
			}
		}
		else
		{
			sValF.clear();
			sValF.appendUCS4(reinterpret_cast<const UT_UCS4Char *>(grText.getPointer(0)),grText.getLength());
			dSum += dGetVal(sValF.utf8_str());
		}
		lastCol = col;
	}
	sFormatDouble(sValF,dSum);
	return _setValue(sValF.ucs4_str().ucs4_str());
}


/* ======================================================================
 * Word-style table formula field (field type "table_formula").
 *
 * The formula text lives on the field object's "param" attribute, e.g.
 *   <field type="table_formula" param="=SUM(ABOVE) \# &quot;0.00&quot;"/>
 * A small recursive-descent evaluator resolves cell refs (A1), ranges
 * (A1:B2) and the positional keywords (ABOVE/BELOW/LEFT/RIGHT) against
 * the containing table, then applies the Word function set.
 *
 * Positional keywords follow Word semantics: starting at the cell next
 * to the formula cell, the contiguous run of numeric cells is collected
 * and the scan stops before the first empty or non-numeric cell.
 * ====================================================================== */

namespace
{

enum FormulaError
{
	FMLERR_OK = 0,
	FMLERR_SYNTAX,      // unparseable expression / unknown name
	FMLERR_ZERODIV,     // division or modulo by zero
	FMLERR_NOTABLE      // cell ref / positional arg outside a table
};

struct FormulaTableCtx
{
	PD_Document * pDoc = nullptr;
	FV_View * pView = nullptr;
	const pf_Frag_Strux * tableSDH = nullptr;
	fl_HdrFtrShadow * pShadL = nullptr;
	UT_sint32 numRows = 0;
	UT_sint32 numCols = 0;
	UT_sint32 myLeft = 0;
	UT_sint32 myRight = 0;
	UT_sint32 myTop = 0;
	UT_sint32 myBot = 0;
	UT_sint32 layoutLID = 0;
};

// does sz hold exactly one number (whitespace, one leading currency
// sign, ',' grouping and a trailing '%' allowed)?
static bool s_parseCellNumber(const char * sz, double & dOut)
{
	dOut = 0.0;
	if(!sz)
		return false;
	while(*sz && isspace(static_cast<unsigned char>(*sz)))
		sz++;
	std::string s(sz);
	std::string::size_type e = s.find_last_not_of(" \t\r\n");
	if(e == std::string::npos)
		return false;
	s.erase(e + 1);

	bool bPercent = false;
	if(!s.empty() && s.back() == '%')
	{
		bPercent = true;
		s.pop_back();
		e = s.find_last_not_of(" \t\r\n");
		if(e == std::string::npos)
			return false;
		s.erase(e + 1);
	}
	// strip a single leading currency sign ($ or the UTF-8 forms of
	// pound/euro/yen)
	if(!s.compare(0, 1, "$"))
		s.erase(0, 1);
	else if(!s.compare(0, 2, "\xC2\xA3") ||   // pound
			!s.compare(0, 2, "\xC2\xA5"))      // yen
		s.erase(0, 2);
	else if(!s.compare(0, 3, "\xE2\x82\xAC")) // euro
		s.erase(0, 3);
	// strip thousands separators
	std::string::size_type p = 0;
	while((p = s.find(',', p)) != std::string::npos)
		s.erase(p, 1);
	if(s.empty())
		return false;

	char * end = nullptr;
	double d = strtod(s.c_str(), &end);
	if(end != s.c_str() + s.size())
		return false;
	dOut = bPercent ? d / 100.0 : d;
	return true;
}

// numeric value of the table cell covering grid position (row, col);
// false when the cell is empty or not a number
static bool s_cellNumber(FormulaTableCtx & ctx, UT_sint32 row, UT_sint32 col,
						 double & dOut)
{
	dOut = 0.0;
	const pf_Frag_Strux* sdhCell = ctx.pDoc->getCellStruxFromRowCol(
		ctx.tableSDH, true, 99999, row, col);
	if(!sdhCell)
		return false;
	fl_ContainerLayout* fmtCell = ctx.pDoc->getNthFmtHandle(sdhCell,
														  ctx.layoutLID);
	fl_CellLayout * pCell = static_cast<fl_CellLayout *>(fmtCell);
	if(!pCell)
		return false;

	UT_GrowBuf grText;
	pCell->appendTextToBuf(grText);
	UT_UTF8String sVal;
	if(grText.getLength() == 0)
	{
		// a cell holding only a field has no piece-table text; the
		// displayed value lives on the field run
		for(fl_ContainerLayout * pC = pCell->getFirstLayout(); pC;
			pC = pC->getNext())
		{
			if(pC->getContainerType() != FL_CONTAINER_BLOCK)
				continue;
			fl_BlockLayout * pBL = static_cast<fl_BlockLayout *>(pC);
			if(ctx.pShadL)
			{
				pBL = static_cast<fl_BlockLayout *>(
					ctx.pShadL->findMatchingContainer(pBL));
			}
			if(!pBL)
				continue;
			for(fp_Run * pRun = pBL->getFirstRun(); pRun;
				pRun = pRun->getNextRun())
			{
				if(pRun->getType() == FPRUN_FIELD)
				{
					sVal.appendUCS4(
						static_cast<fp_FieldRun *>(pRun)->getValue());
				}
			}
		}
	}
	else
	{
		sVal.appendUCS4(reinterpret_cast<const UT_UCS4Char *>(
							grText.getPointer(0)), grText.getLength());
	}
	return s_parseCellNumber(sVal.utf8_str(), dOut);
}

static fl_CellLayout * s_cellLayoutAt(FormulaTableCtx & ctx,
									  UT_sint32 row, UT_sint32 col)
{
	const pf_Frag_Strux* sdhCell = ctx.pDoc->getCellStruxFromRowCol(
		ctx.tableSDH, true, 99999, row, col);
	if(!sdhCell)
		return nullptr;
	return static_cast<fl_CellLayout *>(
		ctx.pDoc->getNthFmtHandle(sdhCell, ctx.layoutLID));
}

class FormulaParser
{
public:
	FormulaParser(const std::string & sText, FormulaTableCtx & ctx)
		: m_s(sText), m_pos(0), m_ctx(ctx), m_err(FMLERR_OK)
	{
	}

	// parse "= expr [\switches]" — returns true on success with the
	// result in dOut; m_pic receives the \# numeric picture if given
	bool parse(double & dOut)
	{
		skipWs();
		if(peek() == '=')
			m_pos++;
		ValList v = parseExpr();
		parseSwitches();
		skipWs();
		if(m_err == FMLERR_OK && m_pos != m_s.size())
			m_err = FMLERR_SYNTAX;
		if(m_err != FMLERR_OK)
			return false;
		dOut = v.empty() ? 0.0 : v[0];
		return true;
	}

	int error() const { return m_err; }
	const std::string & picture() const { return m_pic; }

private:
	typedef std::vector<double> ValList;

	char peek() const
	{
		return m_pos < m_s.size() ? m_s[m_pos] : '\0';
	}
	void skipWs()
	{
		while(m_pos < m_s.size() &&
			  isspace(static_cast<unsigned char>(m_s[m_pos])))
			m_pos++;
	}

	// scalar view of a value list for arithmetic/comparison operands
	double scalar(const ValList & v)
	{
		if(v.empty())
			return 0.0;
		if(v.size() > 1)
			m_err = FMLERR_SYNTAX;
		return v[0];
	}

	bool matchOp(const char * op)
	{
		UT_uint32 n = static_cast<UT_uint32>(strlen(op));
		if(m_s.compare(m_pos, n, op) == 0)
		{
			m_pos += n;
			return true;
		}
		return false;
	}

	ValList parseExpr()          // comparisons produce 1/0
	{
		ValList left = parseAdd();
		while(m_err == FMLERR_OK)
		{
			skipWs();
			int op = 0;         // 1 '=' 2 '<' 3 '>' 4 '<=' 5 '>=' 6 '<>'
			if(matchOp("<>"))      op = 6;
			else if(matchOp("<=")) op = 4;
			else if(matchOp(">=")) op = 5;
			else if(matchOp("="))  op = 1;
			else if(matchOp("<"))  op = 2;
			else if(matchOp(">"))  op = 3;
			if(!op)
				break;
			ValList right = parseAdd();
			if(m_err != FMLERR_OK)
				break;
			double a = scalar(left), b = scalar(right);
			if(m_err != FMLERR_OK)
				break;
			double r;
			switch(op)
			{
				case 1:  r = (a == b); break;
				case 2:  r = (a <  b); break;
				case 3:  r = (a >  b); break;
				case 4:  r = (a <= b); break;
				case 5:  r = (a >= b); break;
				default: r = (a != b); break;
			}
			left = ValList(1, r);
		}
		return left;
	}

	ValList parseAdd()
	{
		ValList left = parseMul();
		while(m_err == FMLERR_OK)
		{
			skipWs();
			char c = peek();
			if(c != '+' && c != '-')
				break;
			m_pos++;
			ValList right = parseMul();
			if(m_err != FMLERR_OK)
				break;
			double a = scalar(left), b = scalar(right);
			if(m_err != FMLERR_OK)
				break;
			left = ValList(1, c == '+' ? a + b : a - b);
		}
		return left;
	}

	ValList parseMul()
	{
		ValList left = parsePow();
		while(m_err == FMLERR_OK)
		{
			skipWs();
			char c = peek();
			if(c != '*' && c != '/' && c != '%')
				break;
			m_pos++;
			ValList right = parsePow();
			if(m_err != FMLERR_OK)
				break;
			double a = scalar(left), b = scalar(right);
			if(m_err != FMLERR_OK)
				break;
			if(c == '*')
				left = ValList(1, a * b);
			else
			{
				if(b == 0.0)
				{
					m_err = FMLERR_ZERODIV;
					break;
				}
				left = ValList(1, c == '/' ? a / b : fmod(a, b));
			}
		}
		return left;
	}

	ValList parsePow()          // right associative
	{
		ValList base = parseUnary();
		if(m_err != FMLERR_OK)
			return base;
		skipWs();
		if(peek() == '^')
		{
			m_pos++;
			ValList ex = parsePow();
			if(m_err != FMLERR_OK)
				return base;
			double a = scalar(base), b = scalar(ex);
			if(m_err != FMLERR_OK)
				return base;
			return ValList(1, pow(a, b));
		}
		return base;
	}

	ValList parseUnary()
	{
		skipWs();
		char c = peek();
		if(c == '+' || c == '-')
		{
			m_pos++;
			ValList v = parseUnary();
			if(m_err == FMLERR_OK && c == '-')
				for(UT_uint32 i = 0; i < v.size(); i++)
					v[i] = -v[i];
			return v;
		}
		return parsePrimary();
	}

	// word = letters/digits/'$' — could be a function name, a cell
	// ref, a positional keyword or a boolean literal
	ValList parsePrimary()
	{
		skipWs();
		char c = peek();
		if(c == '(')
		{
			m_pos++;
			ValList v = parseExpr();
			skipWs();
			if(peek() != ')')
			{
				m_err = FMLERR_SYNTAX;
				return v;
			}
			m_pos++;
			return v;
		}
		if(isdigit(static_cast<unsigned char>(c)) || c == '.')
		{
			const char * start = m_s.c_str() + m_pos;
			char * end = nullptr;
			double d = strtod(start, &end);
			if(end == start)
			{
				m_err = FMLERR_SYNTAX;
				return ValList();
			}
			m_pos += static_cast<UT_uint32>(end - start);
			return ValList(1, d);
		}
		if(c == '$' || isalpha(static_cast<unsigned char>(c)))
		{
			std::string word;
			if(peek() == '$')
				m_pos++;
			while(m_pos < m_s.size() &&
				  (isalnum(static_cast<unsigned char>(m_s[m_pos])) ||
				   m_s[m_pos] == '_' || m_s[m_pos] == '$'))
				word += m_s[m_pos++];
			std::string up;
			for(char ch : word)
				up += static_cast<char>(toupper(static_cast<unsigned char>(ch)));

			skipWs();
			if(peek() == '(')
			{
				m_pos++;
				return parseCall(up);
			}
			if(up == "TRUE")
				return ValList(1, 1.0);
			if(up == "FALSE")
				return ValList(1, 0.0);
			int dir = dirToken(up);
			if(dir >= 0)
				return evalDirection(dir);
			int col, row;
			if(parseCellRef(up, col, row))
			{
				// optional ':' second corner makes a range
				if(peek() == ':')
				{
					m_pos++;
					skipWs();
					std::string w2;
					if(peek() == '$')
						m_pos++;
					while(m_pos < m_s.size() &&
						  (isalnum(static_cast<unsigned char>(m_s[m_pos])) ||
						   m_s[m_pos] == '$'))
						w2 += m_s[m_pos++];
					std::string up2;
					for(char ch : w2)
						up2 += static_cast<char>(
							toupper(static_cast<unsigned char>(ch)));
					int c2, r2;
					if(!parseCellRef(up2, c2, r2))
					{
						m_err = FMLERR_SYNTAX;
						return ValList();
					}
					return evalRange(col, row, c2, r2);
				}
				return evalCell(col, row);
			}
			m_err = FMLERR_SYNTAX;   // unknown name (no bookmarks)
			return ValList();
		}
		m_err = FMLERR_SYNTAX;
		return ValList();
	}

	static bool parseCellRef(const std::string & up, int & col, int & row)
	{
		UT_uint32 i = 0;
		int c = 0;
		while(i < up.size() && isalpha(static_cast<unsigned char>(up[i])))
		{
			c = c * 26 + (up[i] - 'A' + 1);
			if(c > 16384)     // absurd column -> not a cell ref
				return false;
			i++;
		}
		if(i == 0 || i >= up.size())
			return false;
		int r = 0;
		for(; i < up.size(); i++)
		{
			if(!isdigit(static_cast<unsigned char>(up[i])))
				return false;
			r = r * 10 + (up[i] - '0');
			if(r > 32767 * 32)
				return false;
		}
		col = c - 1;
		row = r - 1;
		return true;
	}

	static int dirToken(const std::string & up)
	{
		if(up == "ABOVE") return 0;
		if(up == "BELOW") return 1;
		if(up == "LEFT")  return 2;
		if(up == "RIGHT") return 3;
		return -1;
	}

	ValList evalCell(int col, int row)
	{
		if(!m_ctx.tableSDH)
		{
			m_err = FMLERR_NOTABLE;
			return ValList();
		}
		double d = 0.0;
		if(row >= 0 && col >= 0 &&
		   row < m_ctx.numRows && col < m_ctx.numCols)
			s_cellNumber(m_ctx, row, col, d);
		return ValList(1, d);
	}

	ValList evalRange(int c1, int r1, int c2, int r2)
	{
		if(!m_ctx.tableSDH)
		{
			m_err = FMLERR_NOTABLE;
			return ValList();
		}
		if(c1 > c2) std::swap(c1, c2);
		if(r1 > r2) std::swap(r1, r2);
		c1 = UT_MAX(0, c1); r1 = UT_MAX(0, r1);
		c2 = UT_MIN(m_ctx.numCols - 1, c2);
		r2 = UT_MIN(m_ctx.numRows - 1, r2);
		ValList out;
		std::set<std::pair<int,int> > seen;
		for(int r = r1; r <= r2; r++)
		{
			for(int c = c1; c <= c2; c++)
			{
				fl_CellLayout * pCell = s_cellLayoutAt(m_ctx, r, c);
				if(!pCell)
					continue;
				// merged cells cover several grid positions — count once
				std::pair<int,int> att(pCell->getTopAttach(),
									   pCell->getLeftAttach());
				if(!seen.insert(att).second)
					continue;
				double d;
				if(s_cellNumber(m_ctx, r, c, d))
					out.push_back(d);
			}
		}
		return out;
	}

	// contiguous run of numeric cells from the neighbor outward
	// (Word: the scan stops before the first empty/text cell)
	ValList evalDirection(int dir)
	{
		ValList out;
		if(!m_ctx.tableSDH)
		{
			m_err = FMLERR_NOTABLE;
			return out;
		}
		UT_sint32 r = m_ctx.myTop;
		UT_sint32 c = m_ctx.myLeft;
		int lastTop = -1, lastLeft = -1;
		while(m_err == FMLERR_OK)
		{
			switch(dir)
			{
				case 0:  r--; break;
				case 1:  r++; break;
				case 2:  c--; break;
				default: c++; break;
			}
			if(r < 0 || c < 0 || r >= m_ctx.numRows || c >= m_ctx.numCols)
				break;
			fl_CellLayout * pCell = s_cellLayoutAt(m_ctx, r, c);
			if(!pCell)
				break;
			if(pCell->getTopAttach() == lastTop &&
			   pCell->getLeftAttach() == lastLeft)
				continue;               // merged cell already counted
			double d;
			if(!s_cellNumber(m_ctx, r, c, d))
				break;
			out.push_back(d);
			lastTop = pCell->getTopAttach();
			lastLeft = pCell->getLeftAttach();
		}
		return out;
	}

	ValList parseCall(const std::string & fn)
	{
		// collect one ValList per argument
		std::vector<ValList> args;
		skipWs();
		if(peek() == ')')
		{
			m_pos++;
		}
		else
		{
			while(m_err == FMLERR_OK)
			{
				args.push_back(parseExpr());
				if(m_err != FMLERR_OK)
					return ValList();
				skipWs();
				if(peek() == ',')
				{
					m_pos++;
					continue;
				}
				if(peek() == ')')
				{
					m_pos++;
					break;
				}
				m_err = FMLERR_SYNTAX;
				return ValList();
			}
		}
		return callFunction(fn, args);
	}

	ValList callFunction(const std::string & fn,
						 const std::vector<ValList> & args)
	{
		UT_uint32 n = 0;
		for(const ValList & v : args)
			n += static_cast<UT_uint32>(v.size());
		double first0 = args.size() > 0 && !args[0].empty() ? args[0][0] : 0.0;
		double first1 = args.size() > 1 && !args[1].empty() ? args[1][0] : 0.0;
		double first2 = args.size() > 2 && !args[2].empty() ? args[2][0] : 0.0;

		if(fn == "SUM" || fn == "PRODUCT" || fn == "MIN" ||
		   fn == "MAX" || fn == "AVERAGE" || fn == "COUNT" ||
		   fn == "AND" || fn == "OR")
		{
			if(fn == "COUNT")
				return ValList(1, static_cast<double>(n));
			if(fn == "AVERAGE")
			{
				if(n == 0)
				{
					m_err = FMLERR_ZERODIV;
					return ValList();
				}
				double sum = 0.0;
				for(const ValList & v : args)
					for(double d : v)
						sum += d;
				return ValList(1, sum / n);
			}
			if(fn == "AND" || fn == "OR")
			{
				bool any = false, all = n > 0;
				for(const ValList & v : args)
					for(double d : v)
					{
						any |= (d != 0.0);
						all &= (d != 0.0);
					}
				return ValList(1, fn == "AND" ? all : any);
			}
			double acc = (fn == "PRODUCT") ? 1.0 : 0.0;
			bool first = true;
			for(const ValList & v : args)
			{
				for(double d : v)
				{
					if(fn == "SUM")
						acc += d;
					else if(fn == "PRODUCT")
						acc *= d;
					else if(fn == "MIN")
					{
						if(first || d < acc)
							acc = d;
					}
					else if(first || d > acc)
						acc = d;
					first = false;
				}
			}
			if(n == 0)
				acc = 0.0;
			return ValList(1, acc);
		}
		if(fn == "TRUE")
			return ValList(1, 1.0);
		if(fn == "FALSE")
			return ValList(1, 0.0);
		if(fn == "ABS" && args.size() == 1)
			return ValList(1, fabs(first0));
		if(fn == "SIGN" && args.size() == 1)
			return ValList(1, first0 > 0 ? 1.0 : (first0 < 0 ? -1.0 : 0.0));
		if(fn == "INT" && args.size() == 1)
			return ValList(1, floor(first0));
		if(fn == "ROUND" && (args.size() == 1 || args.size() == 2))
		{
			int digs = static_cast<int>(first1);
			if(digs > 30) digs = 30;
			if(digs < -30) digs = -30;
			double scale = pow(10.0, digs);
			double r = floor(fabs(first0) * scale + 0.5) / scale;
			return ValList(1, first0 < 0 ? -r : r);
		}
		if(fn == "MOD" && args.size() == 2)
		{
			if(first1 == 0.0)
			{
				m_err = FMLERR_ZERODIV;
				return ValList();
			}
			return ValList(1, first0 - first1 * floor(first0 / first1));
		}
		if(fn == "NOT" && args.size() == 1)
			return ValList(1, first0 == 0.0 ? 1.0 : 0.0);
		if(fn == "IF" && (args.size() == 2 || args.size() == 3))
			return ValList(1, first0 != 0.0 ? first1 : first2);
		m_err = FMLERR_SYNTAX;
		return ValList();
	}

	// "\# "picture"" / "\* word" style switches after the expression
	void parseSwitches()
	{
		while(m_err == FMLERR_OK)
		{
			skipWs();
			if(peek() != '\\')
				return;
			m_pos++;
			skipWs();
			char sw = peek();
			if(!sw)
			{
				m_err = FMLERR_SYNTAX;
				return;
			}
			m_pos++;
			skipWs();
			if(peek() == '"')
			{
				m_pos++;
				std::string arg;
				while(m_pos < m_s.size() && m_s[m_pos] != '"')
					arg += m_s[m_pos++];
				if(peek() == '"')
					m_pos++;
				else
					m_err = FMLERR_SYNTAX;
				if(sw == '#')
					m_pic = arg;
			}
			else
			{
				std::string arg;
				while(m_pos < m_s.size() &&
					  !isspace(static_cast<unsigned char>(m_s[m_pos])))
					arg += m_s[m_pos++];
				if(sw == '#')
					m_pic = arg;
			}
		}
	}

	const std::string & m_s;
	UT_uint32 m_pos;
	FormulaTableCtx & m_ctx;
	int m_err;
	std::string m_pic;
};

// format d per a Word \# numeric picture: supports '0'/'#' digit
// placeholders, '.' decimals, ',' grouping, '%' (scales by 100) and
// literal prefix/suffix text; the first ';' section is used
static void s_formatNumericPicture(UT_UTF8String & sVal, double d,
								   const std::string & pic)
{
	std::string fmt = pic;
	std::string::size_type semi = fmt.find(';');
	if(semi != std::string::npos)
		fmt.erase(semi);
	// drop quote marks, keeping the literal text they wrap
	std::string f;
	bool inQ = false;
	for(char ch : fmt)
	{
		if(ch == '"')
			inQ = !inQ;
		else
			f += ch;
	}

	std::string::size_type firstPh = f.find_first_of("0#@");
	std::string::size_type lastPh = f.find_last_of("0#@.");
	std::string prefix, suffix;
	if(firstPh != std::string::npos)
		prefix = f.substr(0, firstPh);
	if(lastPh != std::string::npos && lastPh + 1 < f.size())
		suffix = f.substr(lastPh + 1);

	// digits after the '.' inside the placeholder run
	int decimals = -1;
	bool seenDot = false;
	for(std::string::size_type i =
			(firstPh == std::string::npos ? 0 : firstPh);
		i < f.size() &&
			(lastPh == std::string::npos || i <= lastPh); i++)
	{
		char ch = f[i];
		if(ch == '.')
			seenDot = true;
		else if(seenDot && (ch == '0' || ch == '#'))
			decimals = decimals < 0 ? 1 : decimals + 1;
	}
	if(decimals < 0)
		decimals = 0;
	else if(decimals > 15)
		decimals = 15;
	bool bGroup = f.find(',') != std::string::npos;
	int pct = 0;
	for(char ch : f)
		if(ch == '%')
			pct++;
	while(pct-- > 0)
		d *= 100.0;
	char buf[512];
	snprintf(buf, sizeof(buf), "%.*f", decimals, fabs(d));
	std::string num = buf;
	if(bGroup)
	{
		std::string::size_type dp = num.find('.');
		std::string::size_type end =
			(dp == std::string::npos) ? num.size() : dp;
		for(std::string::size_type i = end; i > 3; i -= 3)
			num.insert(i - 3, ",");
	}
	if(d < 0)
		prefix += '-';
	sVal = (prefix + num + suffix).c_str();
}

} // anonymous namespace


	fp_FieldTableFormulaRun::fp_FieldTableFormulaRun(fl_BlockLayout* pBL, UT_uint32 iOffsetFirst, UT_uint32 iLen)	: fp_FieldRun(pBL,  iOffsetFirst, iLen)
{
}

bool fp_FieldTableFormulaRun::calculateValue(void)
{
	FV_View * pView = _getView();
	UT_UTF8String sValF;
	pf_Frag_Strux* sdh = getBlock()->getStruxDocHandle();
	PD_Document * pDoc = getBlock()->getDocument();
	if(pDoc->isPieceTableChanging() || !pView)
	{
		return false;
	}
	if(getLine() == nullptr)
	{
		return false;
	}
	fp_Container * pCol = getLine()->getColumn();
	if(pCol == nullptr)
	{
		return false;
	}

	FormulaTableCtx ctx;
	ctx.pDoc = pDoc;
	ctx.pView = pView;
	if(pCol->getContainerType() == FP_CONTAINER_COLUMN_SHADOW)
	{
		fp_ShadowContainer * pShad = static_cast<fp_ShadowContainer *>(pCol);
		ctx.pShadL = pShad->getShadow();
	}
	ctx.layoutLID = getBlock()->getDocLayout()->getLID();

	PT_DocPosition pos = pDoc->getStruxPosition(sdh)+1;
	if(pView->isInTable(pos))
	{
		const pf_Frag_Strux* tableSDH = nullptr;
		if(pDoc->getStruxOfTypeFromPosition(pos,PTX_SectionTable,&tableSDH) && tableSDH)
		{
			ctx.tableSDH = tableSDH;
			pDoc->getRowsColsFromTableStrux(tableSDH,
											pView->isShowRevisions(),
											pView->getRevisionLevel(),
											&ctx.numRows, &ctx.numCols);
			pView->getCellParams(pos,&ctx.myLeft,&ctx.myRight,
								 &ctx.myTop,&ctx.myBot);
		}
	}

	// formula text comes from the field's param attribute; the
	// Formula dialog default is =SUM(ABOVE)
	const gchar * pszFormula = _getParameter();
	std::string sFormula = (pszFormula && *pszFormula) ? pszFormula
													 : "=SUM(ABOVE)";

	double dRes = 0.0;
	FormulaParser parser(sFormula, ctx);
	if(parser.parse(dRes))
	{
		if(!parser.picture().empty())
			s_formatNumericPicture(sValF, dRes, parser.picture());
		else
			sFormatDouble(sValF, dRes);
	}
	else
	{
		switch(parser.error())
		{
			case FMLERR_ZERODIV:
				sValF = "!Zero Divide";
				break;
			case FMLERR_NOTABLE:
				sValF = "???";
				break;
			default:
				sValF = "!Syntax Error";
				break;
		}
	}
	// keep the piece-table field's cached value current so exporters
	// can write it as the field result
	if(getField())
	{
		getField()->setValue(sValF.utf8_str());
	}
	return _setValue(sValF.ucs4_str().ucs4_str());
}

