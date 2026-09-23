/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
/* AbiWord
 * Copyright (C) 1998-2000 AbiSource, Inc.
 * Copyright (c) 2001,2002 Tomas Frydrych
 * Copyright (c) 2016-2025 Hubert Figuière
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#define BENCHLAYOUT 0
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <locale.h>
#include <vector>
#include <algorithm>

#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ut_go_file.h"
#include "ut_growbuf.h"
#include "ut_misc.h"
#include "ut_units.h"
#include "ut_std_string.h"
#include "ut_string.h"
#include "ut_bytebuf.h"
#include "ut_string_class.h"
#include "ut_timer.h"
#include "ut_Language.h"
#include "ut_uuid.h"
#include "ut_std_string.h"

#include "xav_View.h"
#include "fl_DocLayout.h"
#include "fl_BlockLayout.h"
#ifdef ENABLE_SPELL
#include "fl_Squiggles.h"
#endif
#include "fl_SectionLayout.h"
#include "fl_AutoNum.h"
#include "fp_Page.h"
#include "fp_PageSize.h"
#include "fp_Column.h"
#include "fp_Line.h"
#include "fp_Run.h"
#include "fp_AnnotationRun.h"
#include "fp_TextRun.h"
#include "fg_Graphic.h"
#include "fg_GraphicRaster.h"
#include "pd_Document.h"
#include "pd_Style.h"
#include "pp_Property.h"
#include "pp_AttrProp.h"
#include "gr_Graphics.h"
#include "gr_Painter.h"
#include "gr_DrawArgs.h"
#include "ie_types.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "xap_Clipboard.h"
#include "ap_TopRuler.h"
#include "ap_LeftRuler.h"
#include "ap_Prefs.h"
#include "ap_Strings.h"
#include "fd_Field.h"
#include "pf_Frag_Strux.h"
#include "fp_FootnoteContainer.h"
#include "fv_ViewDoubleBuffering.h"

#ifdef ENABLE_SPELL
#include "spell_manager.h"
#if 1
// todo: work around to remove the INPUTWORDLEN restriction for pspell
#include "ispell_def.h"
#endif
#endif

#include "ut_rand.h"
#include "fp_TableContainer.h"
#include "fl_FootnoteLayout.h"
#include "fl_ContainerLayout.h"
#include "fl_TOCLayout.h"
#include "pp_Revision.h"

#include "ap_Dialog_SplitCells.h"
#include "ev_Mouse.h"
#include "fv_View.h"

// RIVERA
#include "ap_Dialog_Annotation.h"
#include "xap_Dialog.h"
#include "xap_DialogFactory.h"
#include "ap_Dialog_Id.h"

// NB -- irrespective of this size, the piecetable will store
// at max BOOKMARK_NAME_LIMIT of chars as defined in pf_Frag_Bookmark.h
#define BOOKMARK_NAME_SIZE 30
#define CHECK_WINDOW_SIZE if(getWindowHeight() < 20) return;
#if BENCHLAYOUT
#include <time.h>
#endif

#include "pd_DocumentRDF.h"

/****************************************************************/

void FV_View::cmdUnselectSelection(void)
{
	_clearSelection();
}


/*!
  Move point a number of character positions
  \param bForward True if moving forward
  \param count Number of char positions to move

  \note Cursor movement while there's a selection has the effect of
		clearing the selection. And only that. See bug 993.
*/
void FV_View::cmdCharMotion(bool bForward, UT_uint32 count)
{
	if (!isSelectionEmpty())
	{
		_moveToSelectionEnd(bForward);
		_fixInsertionPointCoords();
		_ensureInsertionPointOnScreen();
		notifyListeners(AV_CHG_MOTION);
		return;
	}

	PT_DocPosition iPoint = getPoint();
	if (!_charMotion(bForward, count))
	{
		if(bForward)
		{
//
// Reached end of document.
//
			UT_DEBUGMSG(("SEVIOR: Reached end of document \n"));
			m_bPointEOL = true;
		}
		else
		{
		        if(m_bInsertAtTablePending)
			{
			      m_iInsPoint = iPoint;
			}
			else
			{
			      _setPoint(iPoint);
			}
		}

		bool bOK = true;
		while(bOK && !isPointLegal() && (getPoint() > 2))
		{
			bOK = _charMotion(false,1);
		}
	}
	else
	{
		PT_DocPosition iPoint1 = getPoint();
		if ( iPoint1 == iPoint )
		{
			if(!_charMotion(bForward, count))
			{
				_setPoint(iPoint);
				_fixInsertionPointCoords();
				_ensureInsertionPointOnScreen();
				notifyListeners(AV_CHG_MOTION);
				return;
			}
			if(!isPointLegal())
			{
				_setPoint(iPoint);
				_fixInsertionPointCoords();
				_ensureInsertionPointOnScreen();
				notifyListeners(AV_CHG_MOTION);
				return;
			}
		}
	}
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_MOTION);
}


/*!
 * Split the merged cells located at the current point in the way specified
 * by iSplitType
  */
bool FV_View::cmdSplitCells(AP_CellSplitType iSplitType)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	const pf_Frag_Strux* cellSDH,*tableSDH;
	const pf_Frag_Strux* curSDH;
	const pf_Frag_Strux* prevCellSDH1,*prevCellSDH2;
	PT_DocPosition posTable,posCell,posFirstInsert,posEndTable;
	posFirstInsert = 0;
	UT_sint32 iLeft,iRight,iTop,iBot;
	UT_sint32 jLeft,jRight,jTop,jBot;
	PT_DocPosition posCol = getPoint();
	if(!isInTable(posCol))
	{
		return false;
	}
	getCellParams(posCol, &iLeft, &iRight,&iTop,&iBot);
	std::string sCellProps;
	getCellFormat(posCol,sCellProps);
//
// Find the Row and column of the cell at the current point. The strategy
// will be insert a new cell with the same (row/col depending on the split)
// and to adjust the rest of the table past this point.
//
	UT_sint32 rowSpan = iBot - iTop;
	UT_sint32 colSpan = iRight - iLeft;
	bool bRes = m_pDoc->getStruxOfTypeFromPosition(posCol,PTX_SectionCell,&cellSDH);
	bRes = m_pDoc->getStruxOfTypeFromPosition(posCol,PTX_SectionTable,&tableSDH);
	UT_return_val_if_fail(bRes, false);

	posTable = m_pDoc->getStruxPosition(tableSDH) + 1;
	posCell = m_pDoc->getStruxPosition(cellSDH);
	const pf_Frag_Strux* endTableStrux = m_pDoc->getEndTableStruxFromTableStrux(tableSDH);
	posEndTable = m_pDoc->getStruxPosition(endTableStrux);
	fl_TableLayout * pTL = static_cast<fl_TableLayout*>(m_pDoc->getNthFmtHandle(tableSDH,m_pLayout->getLID()));
	UT_return_val_if_fail(pTL, false);
	// Get the attributes and properties of the current block. These attributes and properties
	// will be copied in the new cell.
	fl_BlockLayout * pBL = getBlockAtPosition(getPoint());
	const PP_AttrProp * pAP = nullptr;
	m_pDoc->getAttrProp(m_pDoc->getAPIFromStrux(pBL->getStruxDocHandle()), &pAP);
	UT_return_val_if_fail(pAP, false);

//
// Got all we need, now set things up to do the insert nicely
//
	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	// Turn off list updates

	m_pDoc->disableListUpdates();
	m_pDoc->beginUserAtomicGlob();
	if (!isSelectionEmpty())
	{
		_clearSelection();
	}
	m_pDoc->setDontImmediatelyLayout(true);
	//
	// Now trigger a rebuild of the whole table by sending a changeStrux to the table strux
	// with property table-wait-index incremented by 1 so that it is set to a value different from 0.
	//
	PP_PropertyVector tableProps = {
		"table-wait-index",	UT_std_string_sprintf("%d",pTL->getTableWaitIndex() + 1)
	};
	m_pDoc->changeStruxFmt(PTC_AddFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);

	UT_sint32 splitLeft,splitRight,splitTop,splitBot;
	splitLeft = splitRight = 0;
	UT_sint32 newLeft,newRight,newTop,newBot;
	newTop = newBot = 0;
	UT_sint32 numRows = 0;
	UT_sint32 numCols = 0;
	bool bDoSplitSolidHori = false;
	bool bDoSplitSolidVert = false;
//
// OK now insert the cell and do the update
//
	m_pDoc-> getRowsColsFromTableStrux(tableSDH, isShowRevisions(), getRevisionLevel(), &numRows, &numCols);

	if(iSplitType <= hori_right)
	{
//
// This is similar to "insert column"
//
		if(iSplitType ==  hori_left)
		{
			splitLeft = iLeft;
			splitRight = iLeft+1;
		}
		else if(iSplitType ==  hori_mid)
		{
			splitLeft = iLeft;
			if(colSpan == 1)
			{
				bDoSplitSolidHori = true;
				splitRight = iLeft+1;
			}
			else
			{
				splitRight = iLeft + colSpan/2;
			}
		}
		else if(iSplitType ==  hori_right)
		{
			splitLeft = iLeft;
			splitRight = iRight -1;
		}
		splitTop = iTop;
		splitBot = iBot;
		newTop = iTop;
		newBot = iBot;
		if(!bDoSplitSolidHori)
		{
			newLeft = splitRight;
			newRight = iRight;
		}
		else
		{
			newLeft = splitRight;
			newRight = newLeft+1;
		}

	}
	else
	{
		if(iSplitType ==  vert_above)
		{
			newTop = iTop;
			newBot = iTop +1;
		}
		else if(iSplitType ==  vert_mid)
		{
			newTop = iTop;
			if(rowSpan == 1)
			{
				bDoSplitSolidVert = true;
				newBot = newTop+1;
			}
			else
			{
				newBot = iTop + rowSpan/2;
			}
		}
		else if(iSplitType ==  vert_below)
		{
			newTop = iTop;
			newBot = iBot -1;
		}
//
// we need to get the location of where to place this cell.
//
		splitLeft = iLeft;
		splitRight = iRight;
		newLeft = iLeft;
		newRight = iRight;
		if(!bDoSplitSolidVert)
		{
			splitTop = newBot;
			splitBot = iBot;
		}
		else
		{
			newTop = iTop;
			newBot = newTop+1;
			splitTop = newBot;
			splitBot = splitTop+1;
		}
//
// OK now we have to find the place to insert this. It should be the cell
// immediately after (splitTop,splitLeft)
//
//(except if it's a splitSolidVert in which case it should be placed as the
// next next on the row in the first cell before (splitleft,splittop)
//
		if(bDoSplitSolidVert)
		{
//
// OK start with cellSDH and scan until we either reach the end of the table
// or we find a cell past where splitcell should be. Then we insert the cell
// at where cell just after splitcell is.
//
			bool bStop = false;
			curSDH = cellSDH;
			while(!bStop)
			{
				posCell = m_pDoc->getStruxPosition(curSDH)+1;
				bRes = getCellParams(posCell,&jLeft,&jRight,&jTop,&jBot);
				UT_ASSERT(bRes);
				if(jTop >= splitTop)
				{
//
// Found it!
//
					bStop = true;
					posCell = m_pDoc->getStruxPosition(curSDH);
					break;
				}
				bRes = m_pDoc->getNextStruxOfType(curSDH,PTX_SectionCell,&curSDH);
				if(!bRes)
				{
					bStop = true;
					posCell = m_pDoc->getStruxPosition(endTableStrux);
					break;
				}
				posCell = m_pDoc->getStruxPosition(curSDH);
				if(posCell > posEndTable)
				{
					bStop = true;
					posCell = m_pDoc->getStruxPosition(endTableStrux);
					break;
				}
			}
		}
		else
		{
			jTop = splitTop;
			jBot = jTop+1;
			jLeft = splitRight;
			if(jLeft >= numCols)
			{
				jLeft = 0;
				jTop += 1;
				jBot +=1;
			}	
			jRight = jLeft+1;
			if(jTop >= numRows)
			{
//
// Place right before endTable Strux
//
				if (endTableStrux == nullptr)
				{
					//
					// Disaster! the table structure in the piecetable is screwed.
					// we're totally stuffed now.
					UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
					return false;
				}
				posCell = m_pDoc->getStruxPosition(endTableStrux);
			}
			else
			{
//
// now we have to loop until we find a cell precisely at jLeft,jTop
//
				bool bFound = false;
				while(!bFound)
				{
					curSDH = m_pDoc-> getCellStruxFromRowCol(tableSDH,isShowRevisions(), getRevisionLevel(), jTop,jLeft);
					if (curSDH == nullptr)
					{
						endTableStrux = m_pDoc->getEndTableStruxFromTableStrux(tableSDH);
						if(endTableStrux == nullptr)
						{
							//
							// Disaster! the table structure in the piecetable is screwed.
							// we're totally stuffed now.
							UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
							return false;
						}
						posCell = m_pDoc->getStruxPosition(endTableStrux);
						bFound = true;
					}
					UT_sint32 kLeft,kRight,kTop,kBot;
					PT_DocPosition posTmp = m_pDoc->getStruxPosition(curSDH)+1;
					getCellParams(posTmp,&kLeft,&kRight,&kTop,&kBot);
					if((kLeft == jLeft) && (kTop == jTop))
					{
						bFound = true;
						posCell = m_pDoc->getStruxPosition(curSDH);
					}
					else
					{
						jLeft++;
						jRight++;
						if(jLeft >= numCols)
						{
							jLeft = 0;
							jTop++;
							jRight =1;
							jBot++;
						}
						if(jTop >= numRows)
						{
							endTableStrux = m_pDoc->getEndTableStruxFromTableStrux(tableSDH);
							if (endTableStrux == nullptr)
							{
								//
								// Disaster! the table structure in the piecetable is screwed.
								// we're totally stuffed now.
								UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
								return false;
							}
							posCell = m_pDoc->getStruxPosition(endTableStrux);
							bFound = true;
						}
					}
				}
			}
		}
	}
//
// OK build the table properties
//
	std::string sRowTop = "top-attach";
	std::string sRowBot = "bot-attach";
	std::string sColLeft = "left-attach";
	std::string sColRight = "right-attach";
	std::string sTop = UT_std_string_sprintf("%d", splitTop);
	std::string sBot = UT_std_string_sprintf("%d", splitBot);
	std::string sLeft = UT_std_string_sprintf("%d", splitLeft);
	std::string sRight = UT_std_string_sprintf("%d", splitRight);
	UT_std_string_setProperty(sCellProps, sRowTop, sTop);
	UT_std_string_setProperty(sCellProps, sRowBot, sBot);
	UT_std_string_setProperty(sCellProps, sColLeft, sLeft);
	UT_std_string_setProperty(sCellProps, sColRight, sRight);
	UT_DEBUGMSG(("Cells props for new cell:\n  %s \n", sCellProps.c_str()));
//
// Insert the cell
//
	PP_PropertyVector atts = {
		"props", ""
	};
	atts[1] = sCellProps;
	bRes = m_pDoc->insertStrux(posCell, PTX_SectionCell, atts, PP_NOPROPS);
	bRes = m_pDoc->insertStrux(posCell+1,PTX_Block,pAP->getAttributes(),pAP->getProperties());
	posFirstInsert = posCell + 2;
//
// Save the cell SDH for later..
//
	m_pDoc->getStruxOfTypeFromPosition(posCell + 1, PTX_SectionCell, &prevCellSDH1);

	bRes = m_pDoc->insertStrux(posCell+2,PTX_EndCell);

// Changes the props of the new cell
	sTop = UT_std_string_sprintf("%d", newTop);
	sBot = UT_std_string_sprintf("%d", newBot);
	sLeft = UT_std_string_sprintf("%d", newLeft);
	sRight = UT_std_string_sprintf("%d", newRight);
	UT_std_string_setProperty(sCellProps, sRowTop, sTop);
	UT_std_string_setProperty(sCellProps, sRowBot, sBot);
	UT_std_string_setProperty(sCellProps, sColLeft, sLeft);
	UT_std_string_setProperty(sCellProps, sColRight, sRight);
	posCell = m_pDoc->getStruxPosition(cellSDH)+1;
	UT_DEBUGMSG(("New Cells props for old cell:\n  %s \n",sCellProps.c_str()));
	atts[1] = sCellProps;
	UT_DebugOnly<bool> bres = m_pDoc->changeStruxFmt(PTC_AddFmt, posCell, posCell, atts, PP_NOPROPS, PTX_SectionCell);
	UT_ASSERT(bres);
	m_pDoc->getStruxOfTypeFromPosition(posCell, PTX_SectionCell, &prevCellSDH2);
	if(bDoSplitSolidHori)
	{
//
// OK now we have to adjust all the cells with left or right >= splitleft
// If left of a given cell is < splitLeft it is not adjusted but the right
// of the cell if it's > splitleft is incremented. In this way our new cell
// span two columns
//
		UT_sint32 myleft,myright,mytop,mybot;
//
// start at the first cell and scan through the table adjusting each cell.
//
		bres = m_pDoc->getStruxOfTypeFromPosition(posTable+1,PTX_SectionCell,&cellSDH);
		UT_ASSERT(bres);
		bool bStop= false;
		while(!bStop)
		{
			posCell = m_pDoc->getStruxPosition(cellSDH)+1;
			bres = getCellParams(posCell,&myleft,&myright,&mytop,&mybot);
			UT_ASSERT(bres);
			bool bChange =false;
			if((cellSDH == prevCellSDH1) || (cellSDH == prevCellSDH2))
			{
//
// Igore me!
//
				bChange = false;
			}
			else 
			{
				if(myright> splitLeft)
				{
					myright++;
					bChange = true;
				}
				if(myleft>splitLeft)
				{
					myleft++;
					bChange = true;
				}
				if(bChange)
				{
// Changes the props of the cell
					sTop = UT_std_string_sprintf("%d", mytop);
					sBot = UT_std_string_sprintf("%d", mybot);
					sLeft = UT_std_string_sprintf("%d", myleft);
					sRight = UT_std_string_sprintf("%d", myright);
					UT_std_string_setProperty(sCellProps, sRowTop, sTop);
					UT_std_string_setProperty(sCellProps, sRowBot, sBot);
					UT_std_string_setProperty(sCellProps, sColLeft, sLeft);
					UT_std_string_setProperty(sCellProps, sColRight, sRight);
					atts[1] = sCellProps;
					m_pDoc->changeStruxFmt(PTC_AddFmt, posCell, posCell, atts, PP_NOPROPS, PTX_SectionCell);
				}
			}
			if(!m_pDoc->getNextStruxOfType(cellSDH,PTX_SectionCell,&cellSDH))
			{
				bStop = true;
				break;
			}
			posCell = m_pDoc->getStruxPosition(cellSDH);
			if(posCell > posEndTable)
			{
				bStop = true;
				break;
			}
		}
	}
	if(bDoSplitSolidVert)
	{
//
// OK now we have to adjust all the cells with top or bot >= newTop
// If top of a given cell is < newTop it is not adjusted but the bot
// of the cell if it's > newTop is incremented. In this way our new cell
// spans two rows
//
		UT_sint32 myleft,myright,mytop,mybot;
//
// start at the first cell and scan through the table adjusting each cell.
//
		m_pDoc->getStruxOfTypeFromPosition(posTable+1,PTX_SectionCell,&cellSDH);
		bool bStop= false;
		while(!bStop)
		{
			posCell = m_pDoc->getStruxPosition(cellSDH) +1;
			getCellParams(posCell,&myleft,&myright,&mytop,&mybot);
			bool bChange =false;
			if((cellSDH == prevCellSDH1) || (cellSDH == prevCellSDH2))
			{
//
// Igore me!
//
				bChange = false;
			}
			else 
			{
				if(mytop>newTop)
				{
					mytop++;
					bChange = true;
				}
				if(mybot>newTop)
				{
					mybot++;
					bChange = true;
				}
				if(bChange)
				{
// Changes the props of the cell
					sTop = UT_std_string_sprintf("%d", mytop);
					sBot = UT_std_string_sprintf("%d", mybot);
					sLeft = UT_std_string_sprintf("%d", myleft);
					sRight = UT_std_string_sprintf("%d", myright);
					UT_std_string_setProperty(sCellProps, sRowTop, sTop);
					UT_std_string_setProperty(sCellProps, sRowBot, sBot);
					UT_std_string_setProperty(sCellProps, sColLeft, sLeft);
					UT_std_string_setProperty(sCellProps, sColRight, sRight);
					atts[1] = sCellProps;
					m_pDoc->changeStruxFmt(PTC_AddFmt, posCell, posCell, atts, PP_NOPROPS, PTX_SectionCell);
				}
			}
			if(!m_pDoc->getNextStruxOfType(cellSDH,PTX_SectionCell,&cellSDH))
			{
				bStop = true;
				break;
			}
			posCell = m_pDoc->getStruxPosition(cellSDH);
			if(posCell > posEndTable)
			{
				bStop = true;
				break;
			}
		}
	}

	//
	// Now trigger a rebuild of the whole table by sending a changeStrux to the table strux
	// with the property table-wait-index decremented by 1 and removed if it is equal to 0.
	//
	if (pTL->getTableWaitIndex() == 1)
	{
		m_pDoc->changeStruxFmt(PTC_RemoveFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);
	}
	else
	{
		tableProps[1] =	UT_std_string_sprintf("%d", pTL->getTableWaitIndex() - 1);
		m_pDoc->changeStruxFmt(PTC_AddFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);
	}
//
// OK finish everything off with the various parameters which allow the formatter to
// be updated.
//
	m_pDoc->setDontImmediatelyLayout(false);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_generalUpdate();

	m_pDoc->endUserAtomicGlob();

	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();
// Put the insertion point in a legal position
//
	setPoint(posFirstInsert);
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_MOTION);
	return true;
}

void  FV_View::cmdSelectTOC(UT_sint32 x, UT_sint32 y)
{
	UT_sint32 xClick=0,yClick = 0;
	fp_Page* pPage = _getPageForXY(x,y,xClick,yClick);
	fl_TOCLayout * pTOCL = pPage->getLastMappedTOC();
	UT_return_if_fail(pTOCL);
	m_Selection.setTOCSelected(pTOCL);
	PT_DocPosition pos = pTOCL->getPosition();
	m_iInsPoint = pos+1;
	if(m_pG)
	{
		m_pG->allCarets()->disable();
	}
	m_countDisable++;
}

/*!
 * Select the column of the table  identified by the document position 
 * posOfColumn
 */
bool FV_View::cmdSelectColumn(PT_DocPosition posOfColumn)
{
	const pf_Frag_Strux* cellSDH, *tableSDH;
	PT_DocPosition posTable;
	UT_sint32 iLeft,iRight,iTop,iBot;
	UT_sint32 Left,Right,Top,Bot;
	bool bEOL = false; // added this stop compiler warning. Tomas
	if(!isInTable(posOfColumn))
	{
		return false;
	}
	if(!isSelectionEmpty())
	{
		_clearSelection();
		_resetSelection();
	}
	getCellParams(posOfColumn, &iLeft, &iRight,&iTop,&iBot);
	bool bRes = m_pDoc->getStruxOfTypeFromPosition(posOfColumn,PTX_SectionCell,&cellSDH);
	bRes = m_pDoc->getStruxOfTypeFromPosition(posOfColumn,PTX_SectionTable,&tableSDH);
	UT_return_val_if_fail(bRes, false);

	posTable = m_pDoc->getStruxPosition(tableSDH) + 1;

//
// Now find the number of rows and columns inthis table. 
//
	UT_sint32 numRows = 0;
	UT_sint32 numCols = 0;
	m_pDoc->getRowsColsFromTableStrux(tableSDH, isShowRevisions(), getRevisionLevel(), &numRows, &numCols);
//
// Ok set the selection type to that of a column
//
	m_Selection.setMode(FV_SelectionMode_TableColumn);

	fl_BlockLayout * pBlock = nullptr;
	fp_Run * pRun = nullptr;
	UT_sint32 xCaret, yCaret;
	UT_uint32 heightCaret;
	UT_sint32 xCaret2, yCaret2;
	bool bDirection;
	_findPositionCoords(posOfColumn, bEOL, xCaret, yCaret, xCaret2, yCaret2, heightCaret, bDirection, &pBlock, &pRun);
	UT_return_val_if_fail(pBlock,false);
	fl_ContainerLayout * pCL2 = pBlock->myContainingLayout();
	UT_return_val_if_fail(pCL2,false);
	pCL2 = pCL2->myContainingLayout();
	UT_return_val_if_fail(pCL2,false);
	UT_return_val_if_fail((pCL2->getContainerType() == FL_CONTAINER_TABLE),false);
	fl_TableLayout * pTL = static_cast<fl_TableLayout *>(pCL2);
	m_Selection.setTableLayout(pTL);
//
// Now loop through the column and collect all the cells.
//
	UT_sint32 j = 0;
	UT_sint32 jPrev = -1;
	for(j=0; j<numRows; j++)
	{
		PT_DocPosition posWork = findCellPosAt(posTable,j,iLeft) +1;
		getCellParams(posWork,&Left,&Right,&Top,&Bot);
		UT_DEBUGMSG(("Adding cell at left %d right %d top %d bot %d posWork %d \n",Left,Right,Top,Bot,posWork));
		if(Top == jPrev)
		{
			continue;
		}
		_findPositionCoords(posWork+1, bEOL, xCaret, yCaret, xCaret2, yCaret2, heightCaret, bDirection, &pBlock, &pRun);
		UT_return_val_if_fail(pBlock,false);
		UT_DEBUGMSG(("Block pos = %d \n",pBlock->getPosition(false)));
		fl_ContainerLayout * pCL = pBlock->myContainingLayout();
		UT_return_val_if_fail((pCL->getContainerType() == FL_CONTAINER_CELL),false);
		fl_CellLayout * pCell = static_cast<fl_CellLayout *>(pCL);
		m_Selection.addCellToSelection(pCell);
		jPrev = j;
	}
	PD_DocumentRange * pRange = getNthSelection(getNumSelections()-1);
	_setPoint(pRange->m_pos2);
	_drawSelection();
	notifyListeners(AV_CHG_MOTION);
	return true;
}

/*!
 * Convert a table to Text with each cell separated by commas, Tabs, 
 * or tabs and commas as follows:
 * iSepType == 0 Use Commas
 * iSepType == 1 Use Tabs
 * iSepType == 2 Use Tabs and Commas
 * We place a paragraph break at the end of of each row but otherwise we simply
 * extract just the text from each cell.
 */ 
bool FV_View::cmdTableToText(PT_DocPosition posSource,UT_sint32 iSepType)
{
	fl_TableLayout * pTL = getTableAtPos(posSource);
	UT_return_val_if_fail(pTL,false);
	if (!isSelectionEmpty())
	{
		_clearSelection();
	}

  	const pf_Frag_Strux* tableSDH;
	bool bRes = m_pDoc->getStruxOfTypeFromPosition(posSource,PTX_SectionTable,&tableSDH);
	UT_return_val_if_fail(bRes, false);
	PT_DocPosition posTable = m_pDoc->getStruxPosition(tableSDH) + 1;

//
// Now find the number of rows and columns inthis table. 
//
	UT_sint32 numRows = 0;
	UT_sint32 numCols = 0;
	m_pDoc->getRowsColsFromTableStrux(tableSDH, isShowRevisions(), getRevisionLevel(), &numRows, &numCols);
	PT_DocPosition posInsert = pTL->getPosition(true);

	// Signal PieceTable Changes
	_saveAndNotifyPieceTableChange();

	// Turn off list updates

	m_pDoc->disableListUpdates();
	m_pDoc->beginUserAtomicGlob();
	setPoint(posInsert);
	insertParagraphBreak();

	UT_sint32 i,j =0;
	fp_TableContainer * pTAB = static_cast<fp_TableContainer *>(pTL->getFirstContainer());
	fp_CellContainer * pCCell = nullptr;
	fl_CellLayout * pCellL = nullptr;
	UT_GrowBufElement iComma = static_cast<UT_GrowBufElement>(',');
	UT_GrowBufElement iTab = static_cast<UT_GrowBufElement>(UCS_TAB);
	for(i=0;i<numRows;i++)
	{
	  for(j=0; j< numCols;j++)
	  {
	    pCCell = pTAB->getCellAtRowColumn(i,j);
	    if(pCCell == nullptr)
	    {
	         continue;
	    }
	    pCellL = static_cast<fl_CellLayout *>(pCCell->getSectionLayout());
	    if(pCellL == nullptr)
	    {
	         continue;
	    }
	    UT_GrowBuf buf;
	    buf.truncate(0);
	    pCellL->appendTextToBuf(buf);
		if (j < numCols - 1)
		{
			switch (iSepType)
			{
			case 0:
				buf.append(&iComma,1);
				break;
			case 1:
				buf.append(&iTab,1);
				break;
			default:
				buf.append(&iTab,1);
				buf.append(&iComma,1);
			}
		}
		if (buf.getPointer(0))
		{
			cmdCharInsert(reinterpret_cast<UT_UCS4Char *>(buf.getPointer(0)),buf.getLength());
		}
	  }
	  if (i < numRows - 1)
	  {
		  insertParagraphBreak();
	  }
	}
	posTable = pTL->getPosition(true) + 2;
	cmdDeleteTable(posTable, true);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();


	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_MOTION);
	return true;
}

/*!
 * Merge the cells located at posSource with posDestination by copying the data from 
 * source to destination. Then deleting source and expanding destination into it's location
 * in the table.
 */
bool FV_View::cmdMergeCells(PT_DocPosition posSource, PT_DocPosition posDestination)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	UT_sint32 sLeft,sRight,sTop,sBot;
	UT_sint32 dLeft,dRight,dTop,dBot;
	UT_sint32 Left,Right,Top,Bot; // need these for working variables.
	getCellParams(posSource,&sLeft,&sRight,&sTop,&sBot);
	getCellParams(posDestination,&dLeft,&dRight,&dTop,&dBot);

	PT_DocPosition posTable,posWork;
	const pf_Frag_Strux* tableSDH;
	bool bRes = m_pDoc->getStruxOfTypeFromPosition(posSource,PTX_SectionTable,&tableSDH);
	UT_return_val_if_fail(bRes, false);
	posTable = m_pDoc->getStruxPosition(tableSDH) + 1;

//
// Now find the number of rows and columns inthis table. 
//
	UT_sint32 numRows = 0;
	UT_sint32 numCols = 0;
	m_pDoc->getRowsColsFromTableStrux(tableSDH, isShowRevisions(), getRevisionLevel(), &numRows, &numCols);
	bool bChanged = false;

//
// Got all we need, now set things up to do the merge nicely
//
//
// OK that's done, now do the merge
//
// Have to worry about merging cell spanning multiple rows and columns. We do this
// by matching the  the widths and heights of the source and destination cells.	
//
	if((sLeft == dLeft) && (sTop == dTop))
	{
		UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
		return false;
	}

	if(sLeft == dLeft)
	{
//
// Merge vertically
//
//  might have  d |  |  |  |
//                ----------
//             s  |        |
// To solve this merge all top cells horizontally then merge this combined set vertically
//
// First check that top row ends at bottom row boundary. We reject attempts to merge cells
// like this
//            d  |  |  |    |
//               ------------
//            s  |        |
		if(sRight >= dRight)
		{
			if(sRight < numCols -1)
			{
				posWork = findCellPosAt(posTable,dTop,sRight) +1;
				getCellParams(posWork,&Left,&Right,&Top,&Bot);
				if(Left != sRight)
				{
					// 
					// Right column of src cell doesn't match the row above it. Bail out.
					//
					// fixme: Put in a dialog to explain this to the user
					//
					return false;
				}
			}
			//
			// OK now merge all the cells in the destination cell together.
			//
			Left = dRight;
			while(Left < sRight)
			{
				posWork = findCellPosAt(posTable,dTop,Left) +1;
				getCellParams(posWork,&Left,&Right,&Top,&Bot);
//
// Do the merge without all the undo/update trimings
// append onto destination
				if(!bChanged)
				{
					bChanged = _changeCellParams(posTable, tableSDH);
				}
				_MergeCells(posDestination,posWork,false);
				Left = Right;
			}
//
// Now merge the merged destination into the source
//
			if(!bChanged)
			{
				bChanged = _changeCellParams(posTable, tableSDH);
			}
			posSource = findCellPosAt(posTable,sTop,sLeft) +1;
			posDestination = findCellPosAt(posTable,dTop,dLeft) +1;
			_MergeCells(posDestination,posSource,true);
		}
		else
		{
//
// Here we have this scenario:
//
//             d |          |
//               ------------
//            s  |  |  |    |
//
// ie destination is narrower than the source
//
//			check that the source column lines up with the destination column.
//
			if(dRight < numCols -1)
			{
				posWork = findCellPosAt(posTable,sTop,dRight) +1;
				getCellParams(posWork,&Left,&Right,&Top,&Bot);
				if(Left != dRight)
				{
					// 
					// Right column of src cell doesn't match the row above it. Bail out.
					// fixme: Put in a dialog to explain this to the user
					//
					return false;
				}
			}
			//
			// OK now merge all the cells in the src cell together.
			//
			Left = sRight;
			while(Left < dRight)
			{
				posWork = findCellPosAt(posTable,sTop,Left) +1;
				getCellParams(posWork,&Left,&Right,&Top,&Bot);
//
// Do the merge without all the undo/update trimings
//
				if(!bChanged)
				{
					bChanged = _changeCellParams(posTable, tableSDH);
				}
				_MergeCells(posSource,posWork,false);
				Left = Right;
			}
//
// Now merge the merged destination into the source
//
			if(!bChanged)
			{
				bChanged = _changeCellParams(posTable, tableSDH);
			}
			posSource = findCellPosAt(posTable,sTop,sLeft) +1;
			posDestination = findCellPosAt(posTable,dTop,dLeft) +1;
			_MergeCells(posDestination,posSource,true);
		}
	}
	else if(sTop == dTop)
	{
//
// Merge horizontally
//
//  might have  rows spanning several columns
//      d    s
//     ---------
//     |   |   |
//     |   -----
//     |   |   |
//     |   -----
//     |   |   |
//     ---------
// To solve this merge all cells vertically then merge this combined set horizontally
//
// First check that left column ends at right column boundary. 
// We reject attempts to merge cells that don't have this condition.
// ie this:
//      d    s
//     ---------
//     |   |   |
//     |   -----
//     |   |   |
//     |   -----
//     |   |   |
//     |   -----
//     |---|   |
//     |   -----
//
		if(dBot >= sBot)
		{
			if(dBot < numRows -1)
			{
				posWork = findCellPosAt(posTable,dBot,sLeft) +1;
				getCellParams(posWork,&Left,&Right,&Top,&Bot);
				if(Top != dBot)
				{
					// 
					// Bot col of src cell doesn't match the column before. Bail out.
					//
					// fixme: Put in a dialog to explain this to the user
					//
					return false;
				}
			}
			//
			// OK now merge all the cells right of the src cell together.
			//
			Bot	 = sBot;
			Top = sBot;
			while(Top < dBot)
			{
				posWork = findCellPosAt(posTable,Top,sLeft) +1;
				getCellParams(posWork,&Left,&Right,&Top,&Bot);
//
// Do the merge without all the undo/update trimings
//
				if(!bChanged)
				{
					bChanged = _changeCellParams(posTable, tableSDH);
				}
				_MergeCells(posSource,posWork,false);
				UT_ASSERT(Bot > Top);
				if(Bot <= Top)
				{
					break;
				}
				Top = Bot;
			}
//
// Now merge the merged destination into the source
//
			if(!bChanged)
			{
				bChanged = _changeCellParams(posTable, tableSDH);
			}
			posSource = findCellPosAt(posTable,sTop,sLeft) +1;
			posDestination = findCellPosAt(posTable,dTop,dLeft) +1;
			_MergeCells(posDestination,posSource,true);
		}
		else
		{
//
// Here we have this scenario:
//
//
//  might have  rows spanning several columns
//      d    s
//     ---------
//     |   |   |
//     -----   |
//     |   |   |
//     -----   |
//     |   |   |
//     ---------
// To solve this merge all cells vertically then merge this combined set horizontally
//
// First check that left column ends at right column boundary. 
// We reject attempts to merge cells that don't have this condition.
// ie this:
//      d    s
//     ---------
//     |   |   |
//     ----|   |
//     |   |   |
//     ----|   |
//     |   |   |
//     |   |   |
//     |---|   |
//     |   -----
//

			if(sBot < numRows -1)
			{
				posWork = findCellPosAt(posTable,sBot,dLeft) +1;
				getCellParams(posWork,&Left,&Right,&Top,&Bot);
				if(Top != sBot)
				{
					// 
					// Right column of src cell doesn't match the row above it. Bail out.
					//
					// fixme: Put in a dialog to explain this to the user
					//
					return false;
				}
			}
			//
			// OK now merge all the cells above the src cell together.
			//
			Top = dBot;
			while(Top < sBot)
			{
				posWork = findCellPosAt(posTable,Top,dLeft) + 1;
				getCellParams(posWork,&Left,&Right,&Top,&Bot);
//
// Do the merge without all the undo/update trimings
//
				if(!bChanged)
				{
					bChanged = _changeCellParams(posTable, tableSDH);
				}
				_MergeCells(posDestination,posWork,false);
				Top = Bot;
			}
//
// Now merge the source into the merged destination
//
			if(!bChanged)
			{
				bChanged = _changeCellParams(posTable, tableSDH);
			}
			posSource = findCellPosAt(posTable,sTop,sLeft) +1;
			posDestination = findCellPosAt(posTable,dTop,dLeft) +1;
			_MergeCells(posDestination,posSource,true);
		}
	}
	else
	{
//
// Neight left or top align of the cells to be merged.
// bali out

		return false;
	}
//
// Now check if we've merged a whole row of height 2 or a whole col of width two//
// Start with whole row.
//
	posDestination = findCellPosAt(posTable,dTop,dLeft) +2;
	getCellParams(posDestination,&dLeft,&dRight,&dTop,&dBot);
	UT_sint32 origTop = dTop;
	if((dLeft==0) && (dRight== numCols))
	{
//
// Yep one whole row merged.
//
// Look for the number of rows spanned now
//
		if(dBot > (dTop+1))
		{
//
// Yep we have problem, we'll fix it. Subtract this number from all the cells
// top and Bottom attach
//
			UT_sint32 diff = dBot - dTop -1;
			const pf_Frag_Strux* sdhCell = nullptr;
			const pf_Frag_Strux* sdhNextCell = nullptr;
			PT_DocPosition posEndTable = 0;
			PT_DocPosition posCell = 0;
			bRes = m_pDoc->getStruxOfTypeFromPosition(posDestination,PTX_SectionCell,&sdhCell);
			UT_return_val_if_fail(bRes,false);
			const pf_Frag_Strux* struxEndTable = m_pDoc->getEndTableStruxFromTableStrux(tableSDH);
			UT_return_val_if_fail(struxEndTable, false);
			posEndTable = m_pDoc->getStruxPosition(struxEndTable);
			bool bKeepGoing = true;
			while(bKeepGoing)
			{
				posCell = m_pDoc->getStruxPosition(sdhCell)+1;
				getCellParams(posCell,&dLeft,&dRight,&dTop,&dBot);
				dBot -= diff;
				UT_sint32 row = dTop;
				if(dTop != origTop)
				{
					dTop -= diff;
				}
				_changeCellTo(posTable,row,dLeft,dLeft,dRight,dTop,dBot); 

				bRes = m_pDoc->getNextStruxOfType(sdhCell,PTX_SectionCell,&sdhNextCell);
				PT_DocPosition posNextCell = 0;
				if(bRes)
				{
					posNextCell = m_pDoc->getStruxPosition(sdhNextCell);
					if(posNextCell > posEndTable)
					{
						posNextCell = 0;
						bKeepGoing = false;
						break;
					}
				}
				else
				{
					bKeepGoing = false;
					break;
				}
				sdhCell = sdhNextCell;
			}

		}
	}
//
// Look for a whole merged column
//
	if((dTop==0) && (dBot == numRows))
	{
//
// Yep one whole col merged.
//
// Look for the number of cols spanned now
//
		if(dRight > (dLeft+1))
		{
//
// Yep we have problem, we'll fix it. Subtract this number from all the cells
// Right attach from this cell and left and right for all cells to the right
// of it
// This is a bit tricky
// because we don't want to subtract the difference twice so we'll make a 
// vector of unique cell sdh's and only do our thing one those that aren't in 
// it
//
			UT_sint32 diff = dRight - dLeft -1;
			UT_sint32 origLeft = dLeft;
			UT_sint32 origRight = dRight;
			const pf_Frag_Strux* sdhCell = nullptr;
			PT_DocPosition posCell = 0;
			UT_GenericVector<const pf_Frag_Strux*> vecCells;
			posCell = findCellPosAt(posTable, dTop, dLeft)+1;
			m_pDoc->getStruxOfTypeFromPosition(posCell,PTX_SectionCell,&sdhCell);
			vecCells.addItem(sdhCell);
			getCellParams(posCell,&dLeft,&dRight,&dTop,&dBot);
			dRight -= diff;
			_changeCellTo(posTable,dTop,dLeft,dLeft,dRight,dTop,dBot); 
			UT_sint32 row,col=0;
			for (col = 0; col < numCols; col++)
			{
				for(row =0; row < numRows;row++)
				{
					posCell = findCellPosAt(posTable, row, col)+1;
					m_pDoc->getStruxOfTypeFromPosition(posCell,PTX_SectionCell,&sdhCell);
					if((sdhCell==nullptr) || (vecCells.findItem(sdhCell) >= 0))
					{
						continue;
					}
					getCellParams(posCell,&dLeft,&dRight,&dTop,&dBot);
					bool bDoIt = false;
					if(dLeft > origLeft)
					{
						dLeft -= diff;
						bDoIt = true;
					}
					if(dRight >= origRight)
					{
						dRight -= diff;
						bDoIt = true;
					}
					if(bDoIt)
					{
						vecCells.addItem(sdhCell);
						_changeCellTo(posTable,row,col,dLeft,dRight,dTop,dBot); 
					}
				}
			}
		}
	}
//
// Now trigger a rebuild of the whole table by sending a changeStrux to the table strux
//
	if (bChanged)
	{
		_restoreCellParams(posTable, tableSDH);
	}
	setPoint(posDestination);
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_MOTION);
	return true;
}


/*!
 * Move the caret to the next or previous cell in a table. If at either end
 * insert a new row.
 */
bool FV_View::cmdAdvanceNextPrevCell(bool bGoNext)
{
	if(!isInTable())
	{
		return false;
	}

	const pf_Frag_Strux* sdhCell = nullptr;
	const pf_Frag_Strux* sdhNextPrevCell = nullptr;
	bool bRes = m_pDoc->getStruxOfTypeFromPosition(getPoint(),PTX_SectionCell,&sdhCell);
	UT_return_val_if_fail(bRes,false);
	fl_CellLayout * pCL = static_cast<fl_CellLayout *>(m_pDoc->getNthFmtHandle(sdhCell,m_pLayout->getLID()));
	UT_return_val_if_fail(pCL,false);
	if (bGoNext && pCL->getNext())
	{
		sdhNextPrevCell = pCL->getNext()->getStruxDocHandle();
		UT_return_val_if_fail(sdhNextPrevCell && (sdhNextPrevCell->getPos() > sdhCell->getPos()),false);
	}
	else if (!bGoNext && pCL->getPrev())
	{
		sdhNextPrevCell = pCL->getPrev()->getStruxDocHandle();
		UT_return_val_if_fail(sdhNextPrevCell && (sdhNextPrevCell->getPos() < sdhCell->getPos()),false);
	}
	else
	{
		return cmdInsertRow(getPoint(),!bGoNext);
	}

	setPoint(sdhNextPrevCell->getPos()+2);
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_MOTION);
	return true;
}

/*!
 * Convert a text fragment to a table to Text.
 * The column delimiters are tabs, commas and spaces as follows:
 * iDelim == 0 Use tabs
 * iDelim == 1 Use commas
 * iDelim == 2 Use spaces
 * iDelim == 3 Use tabs, commas and spaces as delemiters
 * Paragraph breaks are the row delimiters.
 */

bool FV_View::cmdTextToTable(UT_uint32 iDelim)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	UT_return_val_if_fail(!isSelectionEmpty(), false);
	UT_return_val_if_fail(!isInHdrFtr(getPoint()), false);
	UT_return_val_if_fail(getSelectionMode() == FV_SelectionMode_Single, false);
	UT_GenericVector<fl_BlockLayout *> vecBlocks;
	getBlocksInSelection(&vecBlocks, false);
	UT_return_val_if_fail(vecBlocks.getItemCount() > 0, false);
	fl_BlockLayout * pBL = nullptr;
	UT_uint32 numCols = 0;
	PT_DocPosition posStart = 0;
	PT_DocPosition begPos = 0;
	PT_DocPosition endPos = 0;
	UT_UTF8String sWords;
	UT_GrowBuf * pBuf = nullptr;
	for (UT_sint32 k = 0; k < vecBlocks.getItemCount(); k++)
	{
		pBL = vecBlocks.getNthItem(k);
		pBuf = new UT_GrowBuf(1024);
		pBL->getBlockBuf(pBuf);
		posStart = pBL->getPosition(false);
		UT_uint32 count = 0;
		bool bGetNext = true;
		while(bGetNext)
		{
			bGetNext = pBL->getNextTableElement(pBuf,posStart,begPos,endPos, sWords, iDelim);
			if(begPos != 0)
			{
				count++;
				posStart = endPos+1;
			}
		}
		delete pBuf;
		numCols = UT_MAX(numCols, count);
	}
	UT_uint32 numRows = vecBlocks.getItemCount();
	UT_return_val_if_fail(numCols > 0, false);
	UT_return_val_if_fail(numRows > 0, false);

	pBL = vecBlocks.getNthItem(numRows-1);
	PT_DocPosition posTableStart = pBL->getPosition(true) + pBL->getLength();

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	// Turn off list updates

	m_pDoc->disableListUpdates();
	m_pDoc->beginUserAtomicGlob();
	_clearSelection();
	setPoint(posTableStart);
	UT_Error e = UT_OK;
//
// OK let's create that table now!
//
//
// insert a block to terminate the text before this.
//
	PT_DocPosition pointBreak = getPoint();
	e = m_pDoc->insertStrux(getPoint(),PTX_Block);
//
// Insert the table strux at the same spot. This will make the table link correctly in the
// middle of the broken text.
//
	e |= static_cast<UT_sint32>(m_pDoc->insertStrux(getPoint(), PTX_SectionTable, PP_NOPROPS, PP_NOPROPS));
//
// stuff for cell insertion.
//
	const PP_PropertyVector attrs = {
		"style", "Normal"
	};
	PP_PropertyVector props = {
		"top-attach", "",
		"bot-attach", "",
		"left-attach", "",
		"right-attach", ""
	};
	for(UT_uint32 i = 0; i < numRows; i++) {
		props[1] = UT_std_string_sprintf("%d", i);
		props[3] = UT_std_string_sprintf("%d", i + 1);
		for(UT_uint32 j = 0; j < numCols; j++) {
			props[5] = UT_std_string_sprintf("%d", j);
			props[7] = UT_std_string_sprintf("%d", j + 1);
			e |= static_cast<UT_sint32>(m_pDoc->insertStrux(getPoint(), PTX_SectionCell, PP_NOPROPS, props));
			pointBreak = getPoint();
			e |= static_cast<UT_sint32>(m_pDoc->insertStrux(getPoint(), PTX_Block, attrs, PP_NOPROPS));
			UT_DEBUGMSG(("SEVIOR: 4  cur point %d \n",getPoint()));
			if(getPoint() == pointBreak)
			{
				setPoint(pointBreak+1);
			}
			e |= static_cast<UT_sint32>(m_pDoc->insertStrux(getPoint(),PTX_EndCell));
		}
	}
	e |= static_cast<UT_sint32>(m_pDoc->insertStrux(getPoint(),PTX_EndTable));
//
// Done! Now fill it.
//
	posTableStart +=3;
	const pf_Frag_Strux* sdhTable = nullptr;
	const pf_Frag_Strux* sdhCell = nullptr;
	bool b =m_pDoc->getStruxOfTypeFromPosition(posTableStart,PTX_SectionTable,&sdhTable);
	UT_return_val_if_fail(b,false);
	PT_DocPosition posCell = posTableStart;
	for(UT_uint32 i = 0; i < numRows; i++)
	{
		pBL = vecBlocks.getNthItem(i);
		pBuf = new UT_GrowBuf(1024);
		pBL->getBlockBuf(pBuf);
		posStart = pBL->getPosition(false);
		bool bEnd = false;
		for(UT_uint32 j = 0; !bEnd && j < numCols; j++)
		{
			sdhCell = m_pDoc->getCellStruxFromRowCol(sdhTable,isShowRevisions(),PD_MAX_REVISION,i,j);
			posCell = m_pDoc->getStruxPosition(sdhCell)+1; // Points at block
			sWords.clear();
			bEnd = !pBL->getNextTableElement(pBuf,posStart,	begPos,	endPos,	sWords,	iDelim);
			if(begPos == endPos)
			{
			    posStart = endPos+1;
			    continue;
			}
			if(((j < numCols-1) && (begPos > 0)) || ((j == numCols-1) && (endPos - pBL->getPosition(false)) >= pBuf->getLength()))
			{
				copyToLocal(begPos, endPos);
				_pasteFromLocalTo(posCell+1);
				posStart = endPos+1;
			}
			else if((j==numCols-1) && (begPos > 0))
			{
				copyToLocal(begPos, endPos);
				_pasteFromLocalTo(posCell+1);
				posStart = endPos+1;
				break;
			}
		}
		delete pBuf;
	}
	pBL = vecBlocks.getNthItem(0);
	begPos = pBL->getPosition();
	pBL = vecBlocks.getNthItem(numRows-1);
	endPos = pBL->getPosition(true) + pBL->getLength();
	UT_uint32 iRealDeleteCount;

	m_pDoc->deleteSpan(begPos,endPos,nullptr,iRealDeleteCount);
	

	// Signal PieceTable Changes have finished
	while(m_iPieceTableState > 0)
	  _restorePieceTableState();

	_restorePieceTableState();
	m_pDoc->clearDoingPaste();
	m_pDoc->setDontImmediatelyLayout(false);

	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();

	_generalUpdate();

	m_pDoc->endUserAtomicGlob();


	setPoint(posTableStart);
	PT_DocPosition posEOD;
	UT_DebugOnly<bool> bRes = getEditableBounds(true, posEOD);
	UT_ASSERT(bRes);
	while(!isPointLegal() && getPoint() < posEOD)
	{
	  setPoint(getPoint()+1);
	}
	while(!isPointLegal() && (getPoint() > 2))
	{
	  setPoint(getPoint()-1);
	}
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
//
// We're done!
//	
	return true;
}
/*!
 * Make a table columns autosizing by removing all the column properties.
 */
bool FV_View::cmdAutoSizeCols(void)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

//
// Got all we need, now set things up to do the delete nicely
//
	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	// Turn off list updates

	m_pDoc->disableListUpdates();
	m_pDoc->beginUserAtomicGlob();
	PP_PropertyVector tableProps = {
		"table-column-props", "1"
	};
	m_pDoc->changeStruxFmt(PTC_RemoveFmt, getPoint(), getPoint(), PP_NOPROPS, tableProps, PTX_SectionTable);
	tableProps[0] = "table-column-leftpos";
	m_pDoc->changeStruxFmt(PTC_RemoveFmt, getPoint(), getPoint(), PP_NOPROPS, tableProps, PTX_SectionTable);
	m_pDoc->setDontImmediatelyLayout(false);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();


	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_MOTION);
	return true;
}


/*!
 * Make a table Rows autosizing by removing all the row properties.
 */
bool FV_View::cmdAutoSizeRows(void)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

//
// Got all we need, now set things up to do the delete nicely
//
	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	// Turn off list updates

	m_pDoc->disableListUpdates();
	m_pDoc->beginUserAtomicGlob();
	PP_PropertyVector tableProps = {
		"table-row-heights", "1"
	};
	m_pDoc->changeStruxFmt(PTC_RemoveFmt, getPoint(), getPoint(), PP_NOPROPS, tableProps, PTX_SectionTable);
	tableProps[0] = "table-column-leftpos";
	m_pDoc->changeStruxFmt(PTC_RemoveFmt, getPoint(), getPoint(), PP_NOPROPS, tableProps, PTX_SectionTable);
	m_pDoc->setDontImmediatelyLayout(false);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();


	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_MOTION);
	return true;
}


/*!
 * Make a table Rows autosizing by removing all the row and col properties.
 */
bool FV_View::cmdAutoFitTable(void)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

//
// Got all we need, now set things up to do the delete nicely
//
	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	// Turn off list updates

	m_pDoc->disableListUpdates();
	m_pDoc->beginUserAtomicGlob();
	PP_PropertyVector tableProps = {
		"table-row-heights", "1",
		"table-column-leftpos", "1",
		"table-column-props", "1"
	};

	m_pDoc->changeStruxFmt(PTC_RemoveFmt, getPoint(), getPoint(), PP_NOPROPS, tableProps, PTX_SectionTable);

	tableProps.resize(2);
	tableProps[0] = "homogeneous";
	tableProps[1] = "1";

	m_pDoc->changeStruxFmt(PTC_AddFmt, getPoint(), getPoint(), PP_NOPROPS, tableProps, PTX_SectionTable);
	m_pDoc->setDontImmediatelyLayout(false);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();


	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_MOTION);
	return true;
}


/*!
 * Grow or shrink the width of the column containing the insertion
 * point by 0.1 inch. Provides a keyboard-driven equivalent to
 * dragging a table border with the mouse.
 */
bool FV_View::cmdTableColResize(bool bWider)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	PT_DocPosition posCol = getPoint();
	if(!isInTable(posCol))
	{
		return false;
	}

	const pf_Frag_Strux* tableSDH;
	bool bRes = m_pDoc->getStruxOfTypeFromPosition(posCol,PTX_SectionTable,&tableSDH);
	UT_return_val_if_fail(bRes, false);
	PT_DocPosition posTable = m_pDoc->getStruxPosition(tableSDH) + 1;

	UT_sint32 iLeft,iRight,iTop,iBot;
	getCellParams(posCol, &iLeft, &iRight,&iTop,&iBot);

	fl_TableLayout * pTL = static_cast<fl_TableLayout*>(m_pDoc->getNthFmtHandle(tableSDH,m_pLayout->getLID()));
	UT_return_val_if_fail(pTL,false);
	fp_TableContainer * pTab = static_cast<fp_TableContainer*>(pTL->getFirstContainer());
	UT_return_val_if_fail(pTab,false);
	UT_sint32 iNumCols = pTab->getNumCols();
	UT_return_val_if_fail(iNumCols > 0 && iLeft >= 0 && iLeft < iNumCols, false);

	UT_sint32 iDelta = UT_convertToLogicalUnits("0.1in");
	if(!bWider)
	{
		iDelta = -iDelta;
	}
	UT_sint32 iMinWidth = UT_convertToLogicalUnits("0.05in");

	// the column that absorbs the change so the overall table width stays
	// constant: the column immediately right of the cell, or the column to
	// its left if the cell ends at the table edge
	UT_sint32 iSlack = iRight;
	if(iSlack >= iNumCols || (iSlack >= iLeft && iSlack < iRight))
	{
		iSlack = iLeft - 1;
	}

	std::string sColWidth;
	for(UT_sint32 i = 0; i < iNumCols; i++)
	{
		fp_TableRowColumn * pCol = pTab->getNthCol(i);
		UT_sint32 iWidth = pCol ? pCol->allocation : 0;
		if(iWidth <= 0)
		{
			iWidth = UT_convertToLogicalUnits("1.0in");
		}
		if(i >= iLeft && i < iRight)
		{
			iWidth += iDelta;
		}
		else if(i == iSlack)
		{
			iWidth -= iDelta;
		}
		if(iWidth < iMinWidth)
		{
			iWidth = iMinWidth;
		}
		sColWidth += UT_formatDimensionString(DIM_IN,static_cast<double>(iWidth)/UT_LAYOUT_RESOLUTION,nullptr);
		sColWidth += "/";
	}

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	m_pDoc->disableListUpdates();
	m_pDoc->beginUserAtomicGlob();
	if (!isSelectionEmpty())
	{
		_clearSelection();
	}
	m_pDoc->setDontImmediatelyLayout(true);
	PP_PropertyVector tableProps = {
		"table-column-props", sColWidth.c_str()
	};
	m_pDoc->changeStruxFmt(PTC_AddFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);
	m_pDoc->setDontImmediatelyLayout(false);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();

	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_MOTION);
	return true;
}


/*!
 * Grow or shrink the height of the row containing the insertion
 * point by 0.1 inch.
 */
bool FV_View::cmdTableRowResize(bool bTaller)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	PT_DocPosition posCol = getPoint();
	if(!isInTable(posCol))
	{
		return false;
	}

	const pf_Frag_Strux* tableSDH;
	bool bRes = m_pDoc->getStruxOfTypeFromPosition(posCol,PTX_SectionTable,&tableSDH);
	UT_return_val_if_fail(bRes, false);
	PT_DocPosition posTable = m_pDoc->getStruxPosition(tableSDH) + 1;

	UT_sint32 iLeft,iRight,iTop,iBot;
	getCellParams(posCol, &iLeft, &iRight,&iTop,&iBot);

	fl_TableLayout * pTL = static_cast<fl_TableLayout*>(m_pDoc->getNthFmtHandle(tableSDH,m_pLayout->getLID()));
	UT_return_val_if_fail(pTL,false);
	fp_TableContainer * pTab = static_cast<fp_TableContainer*>(pTL->getFirstContainer());
	UT_return_val_if_fail(pTab,false);
	UT_sint32 iNumRows = pTab->getNumRows();
	UT_return_val_if_fail(iNumRows > 0 && iTop >= 0 && iTop < iNumRows, false);

	UT_sint32 iDelta = UT_convertToLogicalUnits("0.1in");
	if(!bTaller)
	{
		iDelta = -iDelta;
	}
	UT_sint32 iMinHeight = UT_convertToLogicalUnits("0.1in");

	std::string sRowHeight;
	for(UT_sint32 i = 0; i < iNumRows; i++)
	{
		fp_TableRowColumn * pRow = pTab->getNthRow(i);
		UT_sint32 iHeight = pRow ? pRow->allocation : 0;
		if(iHeight <= 0)
		{
			iHeight = UT_convertToLogicalUnits("0.25in");
		}
		if(i >= iTop && i < iBot)
		{
			iHeight += iDelta;
		}
		if(iHeight < iMinHeight)
		{
			iHeight = iMinHeight;
		}
		sRowHeight += UT_formatDimensionString(DIM_IN,static_cast<double>(iHeight)/UT_LAYOUT_RESOLUTION,nullptr);
		sRowHeight += "/";
	}

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	m_pDoc->disableListUpdates();
	m_pDoc->beginUserAtomicGlob();
	if (!isSelectionEmpty())
	{
		_clearSelection();
	}
	m_pDoc->setDontImmediatelyLayout(true);
	PP_PropertyVector tableProps = {
		"table-row-heights", sRowHeight.c_str()
	};
	m_pDoc->changeStruxFmt(PTC_AddFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);
	m_pDoc->setDontImmediatelyLayout(false);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();

	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_MOTION);
	return true;
}


/*!
 * Insert a column containing the position posCol, insert the column before the
 * current column.
 */
bool FV_View::cmdInsertCol(PT_DocPosition posCol, bool bBefore)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	UT_sint32 numColsForInsertion = getNumColumnsInSelection();
	if(numColsForInsertion == 0)
	{
		return false;
	}

	const pf_Frag_Strux* cellSDH,*tableSDH;
	bool bRes = m_pDoc->getStruxOfTypeFromPosition(posCol,PTX_SectionCell,&cellSDH);
	UT_return_val_if_fail(bRes, false);
	bRes = m_pDoc->getStruxOfTypeFromPosition(posCol,PTX_SectionTable,&tableSDH);
	UT_return_val_if_fail(bRes, false);
	PT_DocPosition posTable = m_pDoc->getStruxPosition(tableSDH) + 1;
	PT_DocPosition posCell = posCol;
	UT_sint32 iLeft,iRight,iTop,iBot;
	getCellParams(posCell, &iLeft, &iRight,&iTop,&iBot);

	//
	// Now find the number of rows and columns in this table. This is easiest to
	// get from the table container
	//
	fl_TableLayout * pTL = static_cast<fl_TableLayout*>(m_pDoc->getNthFmtHandle(tableSDH,m_pLayout->getLID()));
	UT_return_val_if_fail(pTL,false);
	fp_TableContainer *pTab = static_cast <fp_TableContainer*>(pTL->getFirstContainer());
	UT_return_val_if_fail(pTab,false);
	bool bInsertEnd = (!bBefore && iRight == pTab->getNumCols());

	// Get the attributes and properties of the block containing posCol. These attributes and properties
	// will be copied in the new cells.
	fl_BlockLayout * pBL = getBlockAtPosition(posCol);
	const PP_AttrProp * pAP = nullptr;
	m_pDoc->getAttrProp(m_pDoc->getAPIFromStrux(pBL->getStruxDocHandle()), &pAP);
	UT_return_val_if_fail(pAP, false);

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	// Turn off list updates

	m_pDoc->disableListUpdates();
	m_pDoc->beginUserAtomicGlob();
	if (!isSelectionEmpty())
	{
		_clearSelection();
	}
	m_pDoc->setDontImmediatelyLayout(true);
	//
	// Now trigger a rebuild of the whole table by sending a changeStrux to the table strux
	// with property table-wait-index incremented by 1 so that it is set to a value different from 0.
	//
	PP_PropertyVector tableProps = {
		"table-wait-index", UT_std_string_sprintf("%d", pTL->getTableWaitIndex() + 1)
	};
	m_pDoc->changeStruxFmt(PTC_AddFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);

	//
	// Loop trough the whole table creating new cells where necessary to add a column.
	// Also shift the left and right attach of the existing cells to make place for
	// the new cells.
	//
	fl_CellLayout* pCL = static_cast<fl_CellLayout*>(pTL->getFirstLayout());
	UT_sint32 iLeftInsert = (bBefore ? iLeft:iRight);
	PT_DocPosition posFirstInsert = 0;
	UT_sint32 iCurLeft, iCurRight, iCurTop, iCurBot;
	UT_sint32 iRow = 0;
	while (pCL)
	{
		if (!bInsertEnd && (pCL->getLeftAttach() < iLeftInsert) && (pCL->getRightAttach() > iLeftInsert))
		{
			// There is a merged cell. Do not add cells
			iRow += pCL->getBottomAttach() - pCL->getTopAttach();
		}
		else if ((!bInsertEnd && (pCL->getTopAttach() == iRow) && (pCL->getLeftAttach() >= iLeftInsert)) ||
				 (bInsertEnd && (pCL->getTopAttach() == iRow + 1)))
		{
			// add cells before pCL
			posCell = pCL->getPosition(true);
			if (posFirstInsert == 0)
			{
				posFirstInsert = posCell + 2;
			}
			iCurLeft = iLeftInsert;
			for (UT_sint32 k = 0; k < numColsForInsertion; k++)
			{
				bRes |= _insertCellAt(posCell, iCurLeft, iCurLeft + 1, iRow, iRow + 1, 
									  pAP->getAttributes(), pAP->getProperties());
				UT_ASSERT(bRes);
				posCell += 3;
				iCurLeft++;
			}
			iRow++;  // this variable is incremented when the cells on a given row have been added.
		}

		if (pCL->getRightAttach() > iLeftInsert)
		{
			// shift left and right attach
			iCurLeft = pCL->getLeftAttach();
			iCurRight = pCL->getRightAttach();
			iCurTop = pCL->getTopAttach();
			iCurBot = pCL->getBottomAttach();
			if (pCL->getLeftAttach() >= iLeftInsert)
			{
				iCurLeft += numColsForInsertion;
			}
			iCurRight += numColsForInsertion;
			posCell = m_pDoc->getStruxPosition(pCL->getStruxDocHandle());
			bRes |= _changeCellAttach(posCell+1, iCurLeft, iCurRight, iCurTop, iCurBot);
			UT_ASSERT(bRes);
		}
		pCL = static_cast<fl_CellLayout*>(pCL->getNext());
	}

	if (bInsertEnd)
	{
		// Add cells at the end of the table
		posCell = m_pDoc->getStruxPosition(pTL->getEndStruxDocHandle());
		if (posFirstInsert == 0)
		{
			posFirstInsert = posCell + 2;
		}
		iCurLeft = iLeftInsert;
		for (UT_sint32 k = 0; k < numColsForInsertion; k++)
		{
			bRes |= _insertCellAt(posCell, iCurLeft, iCurLeft + 1, iRow, iRow + 1, 
								  pAP->getAttributes(), pAP->getProperties());
			UT_ASSERT(bRes);
			posCell += 3;
			iCurLeft++;
		}
		iRow++;  // this variable is incremented when the cells on a given row have been added.
	}
	//
	// Now trigger a rebuild of the whole table by sending a changeStrux to the table strux
	// with the property table-wait-index decremented by 1 and removed if it is equal to 0.
	//
	if (pTL->getTableWaitIndex() == 1)
	{
		m_pDoc->changeStruxFmt(PTC_RemoveFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);
	}
	else
	{
		tableProps[1] = UT_std_string_sprintf("%d", pTL->getTableWaitIndex() - 1);
		m_pDoc->changeStruxFmt(PTC_AddFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);
	}
	//
	// OK finish everything off with the various parameters which allow the formatter to
	// be updated.
	//
	m_pDoc->setDontImmediatelyLayout(false);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();

	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();
	// Put the insertion point in a legal position
	//
	setPoint(posFirstInsert);
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_MOTION);
	return bRes;
}

/*!
 * posRow is the position of the start of the selection in the table.
 * insert rows into the table.
 */
bool FV_View::cmdInsertRow(PT_DocPosition posRow, bool bBefore)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	UT_sint32 numRowsForInsertion = getNumRowsInSelection();
	if(numRowsForInsertion == 0)
	{
		if(isSelectionEmpty() && isInTable(posRow))
		{
			numRowsForInsertion = 1;
		}
		else
		{
			return false;
		}
	}

	const pf_Frag_Strux* cellSDH,*tableSDH;
	bool bRes = m_pDoc->getStruxOfTypeFromPosition(posRow,PTX_SectionCell,&cellSDH);
	UT_return_val_if_fail(bRes, false);
	bRes = m_pDoc->getStruxOfTypeFromPosition(posRow,PTX_SectionTable,&tableSDH);
	UT_return_val_if_fail(bRes, false);
	PT_DocPosition posTable = m_pDoc->getStruxPosition(tableSDH) + 1;
	PT_DocPosition posCell = posRow;
	UT_sint32 iLeft,iRight,iTop,iBot;
	getCellParams(posCell, &iLeft, &iRight,&iTop,&iBot);

	//
	// Now find the number of rows and columns in this table. This is easiest to
	// get from the table container
	//
	fl_TableLayout * pTL = static_cast<fl_TableLayout*>(m_pDoc->getNthFmtHandle(tableSDH,m_pLayout->getLID()));
	UT_return_val_if_fail(pTL,false);
	fp_TableContainer *pTab = static_cast <fp_TableContainer*>(pTL->getFirstContainer());
	UT_return_val_if_fail(pTab,false);
	UT_sint32 numCols = pTab->getNumCols();
	UT_sint32 numRows = pTab->getNumRows();

	// Get the attributes and properties of the block containing posRow. These attributes and properties
	// will be copied in the new cell. 
	fl_BlockLayout * pBL = getBlockAtPosition(posRow);
	const PP_AttrProp * pAP = nullptr;
	m_pDoc->getAttrProp(m_pDoc->getAPIFromStrux(pBL->getStruxDocHandle()), &pAP);
	UT_return_val_if_fail(pAP, false);

	//
	// Find the position in the piece table to insert the row.
	// Have to handle table with merged cells. Find out if a merged cell
	// interferes with the insert cmd and set bComplexInsert to true in that case.
	// Then create a vector of columns where a new cell should be inserted

	fl_CellLayout* pCL = nullptr;
	fp_CellContainer* pCell = nullptr;
	bool bComplexInsert = false;
	UT_sint32 iTopInsert = (bBefore ? iTop:iBot);
	UT_sint32 prevTop = iTopInsert;
	std::vector<UT_sint32> vColInsert;
	if (bBefore || (iBot < numRows))
	{
		pCell = pTab->getCellAtRowColumn(iTopInsert,0);
		UT_return_val_if_fail(pCell,false);
		UT_sint32 iPrevRight = 0;
		while (pCell && pCell->getTopAttach() < iTopInsert)
		{
			pCell = static_cast<fp_CellContainer*>(pCell->getNext());
		}
		UT_return_val_if_fail(pCell,false);
		pCL = static_cast<fl_CellLayout *>(pCell->getSectionLayout());
		UT_return_val_if_fail(pCL,false);
		posCell = pCL->getPosition(true);
		while (pCell && pCell->getTopAttach() == iTopInsert)
		{
			if (pCell->getLeftAttach() != iPrevRight)
			{
				bComplexInsert = true;
			}
			iPrevRight = pCell->getRightAttach();
			for (UT_sint32 k = pCell->getLeftAttach(); k < pCell->getRightAttach(); k++)
			{
				vColInsert.push_back(k);
			}
			pCell = static_cast<fp_CellContainer*>(pCell->getNext());
		}
		if (iPrevRight != numCols)
		{
			bComplexInsert = true;
		}
	}
	else
	{
		// insert cells at the end of the table
		pf_Frag_Strux* endTableSDH = pTL->getEndStruxDocHandle();
		UT_return_val_if_fail(endTableSDH,false);
		posCell = m_pDoc->getStruxPosition(endTableSDH);
		for (UT_sint32 k = 0; k < numCols; k++)
		{
			vColInsert.push_back(k);
		}
	}

	//
	// Got all we need, now set things up to do the delete nicely
	//

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	// Turn off list updates
	m_pDoc->disableListUpdates();
	m_pDoc->beginUserAtomicGlob();
	if (!isSelectionEmpty())
	{
		_clearSelection();
	}
	m_pDoc->setDontImmediatelyLayout(true);

	//
	// Now trigger a rebuild of the whole table by sending a changeStrux to the table strux
	// with property table-wait-index incremented by 1 so that it is set to a value different from 0.
	//
	PP_PropertyVector tableProps = {
		"table-wait-index", UT_std_string_sprintf("%d", pTL->getTableWaitIndex() + 1)
	};
	m_pDoc->changeStruxFmt(PTC_AddFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);

	//
	// Now insert rows.
	//

	PT_DocPosition posInsert = posCell + 2;

	UT_sint32 j = 0;
	for(j=0; j< numRowsForInsertion; j++)
	{
		std::vector<UT_sint32>::iterator it = vColInsert.begin();
		for(; it != vColInsert.end(); ++it)
		{
			bRes |= _insertCellAt(posCell, (*it), (*it)+1, prevTop, prevTop+1, 
								 pAP->getAttributes(), pAP->getProperties());
			UT_ASSERT(bRes);
			posCell += 3;
		}
		prevTop++;
	}
	//
	// OK! starting immediately after this insert loop through the table adding 1 to top and bottom
	//
	UT_sint32 iCurLeft,iCurRight,iCurTop,iCurBot;
	bRes = m_pDoc->getStruxOfTypeFromPosition(posCell-2,PTX_SectionCell,&cellSDH);
	pCL = static_cast<fl_CellLayout*>(m_pDoc->getNthFmtHandle(cellSDH,m_pLayout->getLID()));
	pCL = static_cast<fl_CellLayout*>(pCL->getNext());
    while(pCL)
	{
		iCurLeft = pCL->getLeftAttach();
		iCurRight = pCL->getRightAttach();
		iCurTop = pCL->getTopAttach() + numRowsForInsertion;
		iCurBot = pCL->getBottomAttach() + numRowsForInsertion;
		posCell = m_pDoc->getStruxPosition(pCL->getStruxDocHandle());
		bRes |= _changeCellAttach(posCell+1,iCurLeft,iCurRight,iCurTop,iCurBot);
		UT_ASSERT(bRes);
		pCL = static_cast<fl_CellLayout*>(pCL->getNext());
	}

	if (bComplexInsert)
	{
		// We also need to modify bot-Attach of the merged cells that interfere with the inserted rows
		pTab = static_cast <fp_TableContainer*>(pTL->getFirstContainer());
		std::vector<UT_sint32>::iterator it = vColInsert.begin();
		for (UT_sint32 k = 0; k < numCols;)
		{
			if (it != vColInsert.end() && k == (*it))
			{
				++k;
				++it;
				continue;
			}
			pCell = pTab->getCellAtRowColumn(iTopInsert,k);
			pCL = static_cast<fl_CellLayout*>(pCell->getSectionLayout());
			UT_ASSERT(pCL);
			posCell = pCL->getPosition(true);
			iCurLeft = pCL->getLeftAttach();
			iCurRight = pCL->getRightAttach();
			iCurTop = pCL->getTopAttach();
			iCurBot = pCL->getBottomAttach() + numRowsForInsertion;
			bRes |= _changeCellAttach(posCell+1,iCurLeft,iCurRight,iCurTop,iCurBot);
			UT_ASSERT(bRes);
			k = iCurRight;
		}
	}

	//
	// Now trigger a rebuild of the whole table by sending a changeStrux to the table strux
	// with the property table-wait-index decremented by 1 and removed if it is equal to 0.
	//
	if (pTL->getTableWaitIndex() == 1)
	{
		m_pDoc->changeStruxFmt(PTC_RemoveFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);
	}
	else
	{
		tableProps[1] = UT_std_string_sprintf("%d", pTL->getTableWaitIndex() - 1);
		m_pDoc->changeStruxFmt(PTC_AddFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);
	}

	//
	// OK finish everything off with the various parameters which allow the formatter to
	// be updated.
	//
	setPoint(posInsert);
	m_pDoc->setDontImmediatelyLayout(false);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();

	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_MOTION);
	return bRes;
}

/*!
 * Delete the column containing the position posCol
 */
bool FV_View::cmdDeleteCol(PT_DocPosition posCol)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	const pf_Frag_Strux* cellSDH, *tableSDH;
	PT_DocPosition posTable,posCell2;
	UT_sint32 iLeft,iRight,iTop,iBot;
	getCellParams(posCol, &iLeft, &iRight,&iTop,&iBot);

	bool bRes = m_pDoc->getStruxOfTypeFromPosition(posCol,PTX_SectionCell,&cellSDH);
	bRes = m_pDoc->getStruxOfTypeFromPosition(posCol,PTX_SectionTable,&tableSDH);
	UT_return_val_if_fail(bRes, false);

	posTable = m_pDoc->getStruxPosition(tableSDH) + 1;
	fl_TableLayout * pTL = static_cast<fl_TableLayout*>(m_pDoc->getNthFmtHandle(tableSDH,m_pLayout->getLID()));
	UT_return_val_if_fail(pTL,false);
//
// Now find the number of rows and columns inthis table. This is easiest to
// get from the table container
//
	fl_BlockLayout * pBL =	m_pLayout->findBlockAtPosition(posCol);
	fp_Run * pRun;
	UT_sint32 xPoint,yPoint,xPoint2,yPoint2,iPointHeight;
	bool bDirection;
	pRun = pBL->findPointCoords(posCol, false, xPoint,
							    yPoint, xPoint2, yPoint2,
							    iPointHeight, bDirection);

	UT_return_val_if_fail(pRun, false);

	fp_Line * pLine = pRun->getLine();
	UT_return_val_if_fail(pLine, false);

	fp_Container * pCon = pLine->getContainer();
	UT_return_val_if_fail(pCon, false);

	fp_TableContainer * pTab = static_cast<fp_TableContainer *>(pCon->getContainer());
	UT_return_val_if_fail(pTab, false);

	UT_sint32 numRows = pTab->getNumRows();
//
// If we delete the last column we're actually deleteing the table. So let's
// do it.
//
	if(pTab->getNumCols() ==1)
	{
		cmdDeleteTable(posCol);
		return true;
	}
//
// Got all we need, now set things up to do the delete nicely
//
	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	// Turn off list updates

	m_pDoc->disableListUpdates();
	m_pDoc->beginUserAtomicGlob();
	if (!isSelectionEmpty() && !m_FrameEdit.isActive())
	{
		m_pDoc->beginUserAtomicGlob();
		PP_AttrProp AttrProp_Before;
		_deleteSelection(&AttrProp_Before);
		m_pDoc->endUserAtomicGlob();
	}
	else if(m_FrameEdit.isActive())
	{
	       m_FrameEdit.setPointInside();
	}
	m_pDoc->setDontImmediatelyLayout(true);
	//
	// Now trigger a rebuild of the whole table by sending a changeStrux to the table strux
	// with property table-wait-index incremented by 1 so that it is set to a value different from 0.
	//
	PP_PropertyVector tableProps = {
		"table-wait-index",	UT_std_string_sprintf("%d", pTL->getTableWaitIndex() + 1)
	};
	m_pDoc->changeStruxFmt(PTC_AddFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);
//
// OK loop through all the rows in this column and delete the entries in the specified
// column if the cell spans just the width of the column..
//
	UT_sint32 i =0;
	for(i=0; i <numRows; i++)
	{
		PT_DocPosition posCell = findCellPosAt(posTable,i,iLeft);
		UT_sint32 Left,Right,Top,Bot;
		getCellParams(posCell+1,&Left,&Right,&Top,&Bot);
		UT_DEBUGMSG(("SEVIOR: Before delete left %d right %d top %d bot %d \n",Left,Right,Top,Bot));
		if((Right - Left) == 1)
		{
			_deleteCellAt(posTable,i, iLeft);
		}
	}
//
// OK now subtract one from all the column coordinates in the table with iLeft > iLeft
// do this by running through the linked list of SectionCell fragments in the piecetable
//
// We stop when the position of the endCell strux is just before the position of
// the endTable strux. So lets's get that now.
//
	const pf_Frag_Strux* endTableSDH;
	bRes = m_pDoc->getNextStruxOfType(tableSDH,PTX_EndTable,&endTableSDH);
	if(!bRes)
	{
		//
		// Disaster! the table structure in the piecetable is screwed.
		// we're totally stuffed now.
		UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
	}
	PT_DocPosition posEndTable = m_pDoc->getStruxPosition(endTableSDH);
	PT_DocPosition posEndCell;
	bool bEnd = false;
	UT_sint32 iCurLeft,iCurRight,iCurTop,iCurBot,iNewLeft,iNewRight;
	cellSDH = tableSDH;
    while(!bEnd)
	{
		bRes = m_pDoc->getNextStruxOfType(cellSDH,PTX_SectionCell,&cellSDH);
		if(!bRes)
		{
			bEnd = true;
			break;
		}
		posCell2 =  m_pDoc->getStruxPosition(cellSDH);
		getCellParams(posCell2+1, &iCurLeft, &iCurRight,&iCurTop,&iCurBot);
		UT_DEBUGMSG(("SEVIOR: Looking at cell left %d right %d top %d bot %d \n",iCurLeft,iCurRight,iCurTop,iCurBot));
		bool bChange = false;
		iNewLeft = iCurLeft;
		iNewRight = iCurRight;
		if(iCurLeft > iLeft)
		{
			bChange = true;
			iNewLeft--;
		}
		if(iCurRight > iLeft)
		{
			bChange = true;
			iNewRight--;
		}
		if(bChange)
		{
			UT_DEBUGMSG(("SEVIOR: changing cell to left %d right %d top %d bot %d \n",iNewLeft,iNewRight,iCurTop,iCurBot));
			PP_PropertyVector props = {
				"left-attach", UT_std_string_sprintf("%d", iNewLeft),
				"right-attach",	UT_std_string_sprintf("%d", iNewRight),
				"top-attach", UT_std_string_sprintf("%d", iCurTop),
				"bot-attach", UT_std_string_sprintf("%d",iCurBot)
			};
			bRes = m_pDoc->changeStruxFmt(PTC_AddFmt, posCell2 + 1, posCell2 + 1, PP_NOPROPS, props, PTX_SectionCell);
		}
		const pf_Frag_Strux* endCellStrux = m_pDoc->getEndCellStruxFromCellStrux(cellSDH);
		posEndCell =  m_pDoc->getStruxPosition(endCellStrux);
		if(posEndCell+1 >= posEndTable)
		{
			bEnd = true;
		}
	}
	//
	// Now trigger a rebuild of the whole table by sending a changeStrux to the table strux
	// with the property table-wait-index decremented by 1 and removed if it is equal to 0.
	//
	if (pTL->getTableWaitIndex() == 1)
	{
		m_pDoc->changeStruxFmt(PTC_RemoveFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);
	}
	else
	{
		tableProps[1] = UT_std_string_sprintf("%d", pTL->getTableWaitIndex() - 1);
		m_pDoc->changeStruxFmt(PTC_AddFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);
	}
//
// OK finish everything off with the various parameters which allow the formatter to
// be updated.
//
	m_pDoc->setDontImmediatelyLayout(false);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();


	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();
	_fixInsertionPointCoords();

	_ensureInsertionPointOnScreen();
    notifyListeners(AV_CHG_MOTION);
	return true;
}


/*!
 * Delete the table containing the position posRow
 */
bool FV_View::cmdDeleteTable(PT_DocPosition posTable, bool bDontNotify)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	const pf_Frag_Strux* tableSDH;
	PT_DocPosition posStartTable,posEndTable;
	bool bRes = m_pDoc->getStruxOfTypeFromPosition(posTable,PTX_SectionTable,&tableSDH);
	if(!bRes)
	{
		return false;
	}
	posStartTable = m_pDoc->getStruxPosition(tableSDH);
	const pf_Frag_Strux* endTableStrux = m_pDoc->getEndTableStruxFromTableStrux(tableSDH);
	posEndTable = m_pDoc->getStruxPosition(endTableStrux) + 1;
//
// Got all we need, now set things up to do the delete nicely
//
	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	// Turn off list updates

	m_pDoc->disableListUpdates();
	m_pDoc->beginUserAtomicGlob();
	if (!isSelectionEmpty())
	{
		_clearSelection();
		_resetSelection();
	}
	m_pDoc->setDontImmediatelyLayout(true);
//
// OK do the delete
//
	UT_uint32 iRealDeleteCount;
	//	if(m_pDoc->isFrameAtPos(posStartTable-1))
	//   posStartTable--;
	m_pDoc->deleteSpan( posStartTable, posEndTable, nullptr,iRealDeleteCount,true);
//
// OK finish everything off with the various parameters which allow the formatter to
// be updated.
//
	m_pDoc->setDontImmediatelyLayout(false);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();

	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();
	setPoint(getPoint());
	//
	// This method could be called from text to tablein which case
	// we don't want to do this.
	//
	if(!bDontNotify)
	{
	     notifyListeners(AV_CHG_ALL);
	     _fixInsertionPointCoords();
	     _ensureInsertionPointOnScreen();
	}
	return true;
}

/*!
 * Delete the row containing the position posRow
 */
bool FV_View::cmdDeleteRow(PT_DocPosition posRow)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	const pf_Frag_Strux *tableSDH;
	PT_DocPosition posTable,posCell2;
	UT_sint32 iLeft,iRight,iTop,iBot;
	getCellParams(posRow, &iLeft, &iRight,&iTop,&iBot);

	const pf_Frag_Strux* cellSDH;
	bool bRes = m_pDoc->getStruxOfTypeFromPosition(posRow,PTX_SectionCell,&cellSDH);
	bRes = m_pDoc->getStruxOfTypeFromPosition(posRow,PTX_SectionTable,&tableSDH);
	UT_return_val_if_fail(bRes, false);

	posTable = m_pDoc->getStruxPosition(tableSDH) + 1;
	fl_TableLayout * pTL = static_cast<fl_TableLayout*>(m_pDoc->getNthFmtHandle(tableSDH,m_pLayout->getLID()));
	UT_return_val_if_fail(pTL,false);
//
// Now find the number of rows and columns inthis table. This is easiest to
// get from the table container
//
	fl_TableLayout * pTabL = getTableAtPos(posRow);
	if(pTabL == nullptr)
	{
	    pTabL = getTableAtPos(posRow+1);
	    if(pTabL == nullptr)
	    {
		pTabL = getTableAtPos(posRow+2);
		UT_return_val_if_fail(pTabL, false);
	    }
	}
	fp_TableContainer * pTab = static_cast<fp_TableContainer *>(pTabL->getFirstContainer());
	UT_return_val_if_fail(pTab, false);
	UT_sint32 numCols = pTab->getNumCols();
//
// If we delete the last row we're actually deleting the table, so do that 
// instead.
//
	UT_sint32 nRows = getNumRowsInSelection();
	if(pTab->getNumRows() == 1 || (nRows == pTab->getNumRows()))
	{
		cmdDeleteTable(posRow);
		return true;
	}
//
// Got all we need, now set things up to do the delete nicely
//
	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	// Turn off list updates

	m_pDoc->disableListUpdates();
	m_pDoc->beginUserAtomicGlob();
	UT_sint32 numRows = getNumRowsInSelection();
	if (!isSelectionEmpty() && !m_FrameEdit.isActive())
	{
		m_pDoc->beginUserAtomicGlob();
		PP_AttrProp AttrProp_Before;
		_deleteSelection(&AttrProp_Before);
		m_pDoc->endUserAtomicGlob();
	}
	else if(m_FrameEdit.isActive())
	{
	       m_FrameEdit.setPointInside();
	}
	m_pDoc->setDontImmediatelyLayout(true);
	//
	// Now trigger a rebuild of the whole table by sending a changeStrux to the table strux
	// with property table-wait-index incremented by 1 so that it is set to a value different from 0.
	//
	PP_PropertyVector tableProps = {
		"table-wait-index", UT_std_string_sprintf("%d", pTL->getTableWaitIndex() + 1)
	};
	m_pDoc->changeStruxFmt(PTC_AddFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);
//
// OK loop through all the rows in this column and delete the entries in the specified
// column if the cell spans just the width of the column..
//
	UT_sint32 i =0;
	UT_sint32 j = 0;
	for(j=numRows-1; j>=0; j--)
	{
		for(i=numCols-1; i >=0; i--)
		{
			PT_DocPosition posCell = findCellPosAt(posTable,iTop+j,i);
			UT_sint32 Left,Right,Top,Bot;
			getCellParams(posCell+1,&Left,&Right,&Top,&Bot);
			UT_DEBUGMSG(("SEVIOR: Before delete left %d right %d top %d bot %d \n",Left,Right,Top,Bot));
			if((Bot - Top) == 1)
			{
				_deleteCellAt(posTable,iTop+j, i);
			}
		}
	}
//
// OK now subtract numRows from all the row coordinates in the table with iTop,iBot > iTop
// do this by running through the linked list of SectionCell fragments in the piecetable
//
// We stop when the position of the endCell strux is just before the position of
// the endTable strux. So lets's get that now.
//
	const pf_Frag_Strux* endTableStrux = m_pDoc->getEndTableStruxFromTableStrux(tableSDH);
	if (!bRes || (endTableStrux == nullptr))
	{
		//
		// Disaster! the table structure in the piecetable is screwed.
		// we're totally stuffed now.
		UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
		m_pDoc->setDontImmediatelyLayout(false);
	
		// Signal PieceTable Changes have finished
		_restorePieceTableState();
		m_pDoc->endUserAtomicGlob();
		return false;
	}
	PT_DocPosition posEndTable = m_pDoc->getStruxPosition(endTableStrux);
	PT_DocPosition posEndCell;
	bool bEnd = false;
	UT_sint32 iCurLeft,iCurRight,iCurTop,iCurBot,iNewTop,iNewBot;
	cellSDH = tableSDH;
    while(!bEnd)
	{
		bRes = m_pDoc->getNextStruxOfType(cellSDH,PTX_SectionCell,&cellSDH);
		if(!bRes)
		{
			bEnd = true;
			break;
		}
		posCell2 =  m_pDoc->getStruxPosition(cellSDH);
		getCellParams(posCell2+1, &iCurLeft, &iCurRight,&iCurTop,&iCurBot);
		UT_DEBUGMSG(("SEVIOR: Looking at cell left %d right %d top %d bot %d \n",iCurLeft,iCurRight,iCurTop,iCurBot));
		bool bChange = false;
		iNewTop = iCurTop;
		iNewBot = iCurBot;
		if(iCurTop > iTop)
		{
			bChange = true;
			iNewTop -= numRows;
		}
		if(iCurBot > iTop)
		{
			bChange = true;
			iNewBot -= numRows;
		}
		if(bChange)
		{
			UT_DEBUGMSG(("SEVIOR: changing cell to left %d right %d top %d bot %d \n",iCurLeft,iCurRight,iNewTop,iNewBot));
			PP_PropertyVector props = {
				"left-attach", UT_std_string_sprintf("%d", iCurLeft),
				"right-attach", UT_std_string_sprintf("%d", iCurRight),
				"top-attach", UT_std_string_sprintf("%d", iNewTop),
				"bot-attach", UT_std_string_sprintf("%d", iNewBot),
			};
			bRes = m_pDoc->changeStruxFmt(PTC_AddFmt, posCell2 + 1, posCell2 + 1, PP_NOPROPS, props, PTX_SectionCell);
		}
		const pf_Frag_Strux* endCellStrux = m_pDoc->getEndCellStruxFromCellStrux(cellSDH);
		posEndCell =  m_pDoc->getStruxPosition(endCellStrux);
		if(posEndCell+1 >= posEndTable)
		{
			bEnd = true;
		}
	}
	//
	// Now trigger a rebuild of the whole table by sending a changeStrux to the table strux
	// with the property table-wait-index decremented by 1 and removed if it is equal to 0.
	//
	if (pTL->getTableWaitIndex() == 1)
	{
		m_pDoc->changeStruxFmt(PTC_RemoveFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);
	}
	else
	{
		tableProps[1] =	UT_std_string_sprintf("%d", pTL->getTableWaitIndex() - 1);
		m_pDoc->changeStruxFmt(PTC_AddFmt, posTable, posTable, PP_NOPROPS, tableProps, PTX_SectionTable);
	}
//
// OK finish everything off with the various parameters which allow the formatter to
// be updated.
//
	m_pDoc->setDontImmediatelyLayout(false);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();


	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();
	_fixInsertionPointCoords();

	_ensureInsertionPointOnScreen();
    notifyListeners(AV_CHG_MOTION);
	return true;
}


/*!
 * Delete the cell at the specified position
 */
bool FV_View::cmdDeleteCell(PT_DocPosition /*cellPos*/ )
{
#if 1
  UT_ASSERT(UT_NOT_IMPLEMENTED);
  return true ;
#else
	pf_Frag_Strux* cellSDH;
	const char * pszLeftAttach =nullptr;
	const char * pszTopAttach = nullptr;
	UT_sint32 iLeft =-999;
	UT_sint32 iTop = -999;
	bool bRes = m_pDoc->getStruxOfTypeFromPosition(cellPos,PTX_SectionCell,&cellSDH);
	if(!bRes)
	{
		return false;
	}
	m_pDoc->getPropertyFromStrux(cellSDH,"left-attach",&pszLeftAttach);
	m_pDoc->getPropertyFromStrux(cellSDH,"top-attach",&pszTopAttach);
	if(pszLeftAttach && *pszLeftAttach)
	{
		iLeft = atoi(pszLeftAttach);
	}
	if(pszTopAttach && *pszTopAttach)
	{
		iTop = atoi(pszTopAttach);
	}
//
// Got all we need, now set things up to do the delete nicely
//
	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	// Turn off list updates

	m_pDoc->disableListUpdates();
	m_pDoc->beginUserAtomicGlob();
	if (!isSelectionEmpty() && !m_FrameEdit.isActive())
	{
		m_pDoc->beginUserAtomicGlob();
		PP_AttrProp AttrProp_Before;
		_deleteSelection(&AttrProp_Before);
		m_pDoc->endUserAtomicGlob();
	}
	else if(m_FrameEdit.isActive())
	{
	       m_FrameEdit.setPointInside();
	}
	m_pDoc->setDontImmediatelyLayout(true);
//
// delete the cell.
//
	_deleteCellAt(cellPos,iTop, iLeft);
//
// OK do all the piecetable finished changing business
//
	m_pDoc->setDontImmediatelyLayout(false);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();


	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();
	_fixInsertionPointCoords();

	_ensureInsertionPointOnScreen();
    notifyListeners(AV_CHG_MOTION);
	return true;
#endif
}


/*!
 * Insert a table of the  number of rows and columns given.
 */
UT_Error FV_View::cmdInsertTable(UT_sint32 numRows, UT_sint32 numCols, const PP_PropertyVector & pPropsArray)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	// TODO -- why does this function return UT_Error? If bool is
	// sufficient, it should return bool, and if not, than the
	// UT_Error & bool operations below are probably not safe
	UT_Error e = UT_OK;
	if(numRows == 0 || numCols==0)
	{
		return 0; 
	}
	if(isInTable(getPoint()-1) && isInTable() && isHdrFtrEdit())
	{
		return 0;
	}
	if(isInTable(getPoint()) && !isSelectionEmpty() && isHdrFtrEdit())
	{
		return 0;
	}
#if BENCHLAYOUT
	printf("Doing Insert Table \n");
	timespec t1;
	clock_gettime(CLOCK_REALTIME, &t1);
#endif


//
// Do all the stuff we need to make this go smoothly and to undo in a single step.
//
	// Signal PieceTable Changes
	_saveAndNotifyPieceTableChange();

	// Turn off list updates

	m_pDoc->disableListUpdates();
	m_pDoc->beginUserAtomicGlob();
	if (!isSelectionEmpty() && !m_FrameEdit.isActive())
	{
		m_pDoc->setDontImmediatelyLayout(true);
		m_pDoc->beginUserAtomicGlob();
		PP_AttrProp AttrProp_Before;
		_deleteSelection(&AttrProp_Before);
		m_pDoc->endUserAtomicGlob();
	}
	else if(m_FrameEdit.isActive())
	{
	       m_FrameEdit.setPointInside();
	}
	else
	{
		m_pDoc->setDontImmediatelyLayout(true);
	}
	if(getHyperLinkRun(getPoint()) != nullptr)
	{

	// Signal PieceTable Changes have finished
	// restore updates and clean up dirty lists
		m_pDoc->enableListUpdates();
		m_pDoc->updateDirtyLists();
		_restorePieceTableState();
		return 0;
	}
//
// Handle corner case of point at endTOC
//
	if(m_pDoc->isTOCAtPos(getPoint()-1))
	{
	 		setPoint(getPoint()-1);
	}
//
// insert a block to terminate the text before this.
//
	PT_DocPosition pointBreak = getPoint();
	PT_DocPosition pointTable = 0;
	//
	// Don't do this if there is a block at pointBreak already.
	//
	bool bInsert = false;
	if((!m_pDoc->isBlockAtPos(getPoint()) && !m_pDoc->isTableAtPos(getPoint()) && !(m_pDoc->isEndFrameAtPos(getPoint()) && m_pDoc->isBlockAtPos(getPoint()-1) )) || m_pDoc->isTOCAtPos(getPoint()-2) )
	{
	         e = m_pDoc->insertStrux(getPoint(),PTX_Block);
		 bInsert = true;
	}
	bool bPointBreak = false;
	if(!bInsert && !m_pDoc->isTableAtPos(getPoint()) && !m_pDoc->isEndFootnoteAtPos(getPoint()-2) && !m_pDoc->isEndFootnoteAtPos(getPoint()-1) && !m_pDoc->isBlockAtPos(getPoint()))
	{
	         pointBreak--;
		 bPointBreak = true;
	}
	if(!bPointBreak && m_pDoc->isBlockAtPos(getPoint()))
	{
	         PT_DocPosition posEnd = 0;
		 getEditableBounds(true,posEnd);
		 if(posEnd == getPoint())
		 {
		         pointBreak--;
			 bPointBreak = true;
		 }
		 else if(m_pDoc->isSectionAtPos(getPoint()-1) || m_pDoc->isEndTableAtPos(getPoint()-1) || m_pDoc->isEndFrameAtPos(getPoint() - 1))
		 {
		         pointBreak--;
			 bPointBreak = true;
		 }
		 else if(m_pDoc->isSectionAtPos(getPoint()-2))
		 {
		         pointBreak--;
			 bPointBreak = true;
		 }
		 if(m_pDoc->isEndFootnoteAtPos(pointBreak))
		 {
		         pointBreak++;
			 bPointBreak = false;
		 }
		 if(bPointBreak && !m_pDoc->isBlockAtPos(pointBreak))
		 {
		         pointBreak++;
			 bPointBreak = false;
		 }

	}
//
// Insert the table strux at the same spot. This will make the table link correctly in the
// middle of the broken text.
//
// Handle special case of not putting a table immediately after a section break
//
	const pf_Frag_Strux* secSDH = nullptr;
	UT_DebugOnly<bool> bres = m_pDoc->getStruxOfTypeFromPosition(pointBreak-1,PTX_Section,&secSDH);
#if DEBUG
	UT_ASSERT(bres);
	PT_DocPosition secPos2 = m_pDoc->getStruxPosition(secSDH);
	UT_DEBUGMSG(("SEVIOR: SecPos %d pointBreak %d \n",secPos2,pointBreak));
#endif
	secSDH = nullptr;
	bres = m_pDoc->getStruxOfTypeFromPosition(pointBreak,PTX_SectionCell,&secSDH);
#if DEBUG
	if(secSDH != nullptr)
	{
		PT_DocPosition secPos = m_pDoc->getStruxPosition(secSDH);
		UT_DEBUGMSG(("SEVIOR: Cell Pos %d pointBreak %d \n",secPos,pointBreak));	
	}
#endif
//
// Handle special case of not putting a table immediately after an end text box 
//
	if(m_pDoc->isEndFrameAtPos(pointBreak-1))
	{
		pointBreak--;
	}
	//
	// Handle special case of not putting a table in a TOC
	//
	if(m_pDoc->isTOCAtPos(pointBreak-1))
	{
		pointBreak++;
	}

	setPoint(pointBreak);
	// Get the current attributes and properties. The user likely does not want the block properties as
	// the margins and indents might not be right for a much narrower container (the cell).
	// This is a ugly hack. We will only copy 2 fonts properties, font-family and font-size.
	PP_PropertyVector props_in;
	const char* normalStyleName = "Normal";
	PP_PropertyVector attrsBlock = {
		"style", normalStyleName
	};
	const gchar * propFamily = "font-family";
	const gchar * propSize = "font-size";
	std::string szFamily;
	std::string szSize;
	const gchar * szNormalFamily = nullptr;
	const gchar * szNormalSize = nullptr;
	getCharFormat(props_in);
	if (!props_in.empty())
	{
		szFamily = PP_getAttribute(propFamily, props_in);
		szSize = PP_getAttribute(propSize, props_in);
	}
	m_pDoc->getStyleProperty(normalStyleName, propFamily, szNormalFamily);
	m_pDoc->getStyleProperty(normalStyleName, propSize, szNormalSize);

	bool bFamily = (!szFamily.empty() && (!szNormalFamily || szFamily != szNormalFamily));
	bool bSize = (!szSize.empty() && (!szNormalSize || szSize != szNormalSize));
	PP_PropertyVector propsBlock;
	if (bFamily)
	{
		propsBlock.push_back(propFamily);
		propsBlock.push_back(szFamily);
	}
	if (bSize)
	{
		propsBlock.push_back(propSize);
		propsBlock.push_back(szSize);
	}

	e |= static_cast<UT_sint32>(m_pDoc->insertStrux(getPoint(), PTX_SectionTable, PP_NOPROPS, pPropsArray));
//
// stuff for cell insertion.
//
	UT_sint32 i,j;
	PP_PropertyVector props = {
		"top-attach", "",
		"bot-attach", "",
		"left-attach", "",
		"right-attach", ""
	};
	for(i = 0; i < numRows; i++)
	{
		props[1] = UT_std_string_sprintf("%d", i);
		props[3] = UT_std_string_sprintf("%d", i + 1);
		for(j = 0; j < numCols; j++)
		{
			props[5] = UT_std_string_sprintf("%d", j);
			props[7] = UT_std_string_sprintf("%d", j + 1);
			e |= static_cast<UT_sint32>(m_pDoc->insertStrux(getPoint(), PTX_SectionCell, PP_NOPROPS, props));
			pointBreak = getPoint();
			e |= static_cast<UT_sint32>(m_pDoc->insertStrux(getPoint(), PTX_Block, attrsBlock, propsBlock));
			UT_DEBUGMSG(("SEVIOR: 4  cur point %d \n",getPoint()));
			if(getPoint() == pointBreak)
			{
				setPoint(pointBreak+1);
			}
			if(i == 0 && j==0)
			{
				pointTable = getPoint();
			}
			e |= static_cast<UT_sint32>(m_pDoc->insertStrux(getPoint(),PTX_EndCell));
		}
	}
	m_pDoc->setDontImmediatelyLayout(false);

#if BENCHLAYOUT
	timespec t2;
	clock_gettime(CLOCK_REALTIME, &t2);	
	double millidiff = (t2.tv_sec-t1.tv_sec)*1e3 + (t2.tv_nsec-t1.tv_nsec)/1e6;
	printf("Insert TIME: %lf milliseconds\n", millidiff);  
#endif
	e |= static_cast<UT_sint32>(m_pDoc->insertStrux(getPoint(),PTX_EndTable));

	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_generalUpdate();

	m_pDoc->endUserAtomicGlob();
	setPoint(pointTable);
	_makePointLegal();
	_fixInsertionPointCoords();
	focusChange(AV_FOCUS_HERE);
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	AV_View::notifyListeners (AV_CHG_ALL);
	return e;
}

bool
FV_View::cmdCharInsert(const std::string& s, bool bForce)
{
	UT_UCS4String t( s );
	return cmdCharInsert( t.ucs4_str(), t.length(), bForce );
}


bool FV_View::cmdCharInsert(const UT_UCS4Char * text, UT_uint32 count, bool bForce)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	//
	// Look if we should insert a pargraph before the table
	//
	if(m_bInsertAtTablePending && (count==1) && (text[0] != UCS_FF) && (text[0] != UCS_VTAB))
	{
		m_pDoc->beginUserAtomicGlob();

		// Prevent access to Piecetable for things like spellchecks until
		// paragraphs have stablized
		//
		_saveAndNotifyPieceTableChange();
		m_pDoc->disableListUpdates();
		PT_DocPosition pos = m_iPosAtTable;
		m_pDoc->insertStrux( m_iPosAtTable,PTX_Block);
		m_bInsertAtTablePending = false;

		// Signal piceTable is stable again
		_restorePieceTableState();

		// Signal piceTable is stable again
		// Signal PieceTable Changes have finished
		_generalUpdate();
		// restore updates and clean up dirty lists
		m_pDoc->enableListUpdates();
		m_pDoc->updateDirtyLists();
		setPoint(pos+1);
		m_iPosAtTable = 0;
		_generalUpdate();
		bool res = _charInsert(text, count, bForce);
		m_pDoc->endUserAtomicGlob();
		return res;
	}

	// the code below inserts a direction marker before a space if the automatic insertion of such
	// markers is indicated by user's preferences and if the current keyboard language direction is
	// inconsistent with the dominant direction of the paragraph (this makes phone numbers and similar
	// to work in rtl languages more intuitively)

	// we only do this for space ... (certain other chars can be handled in ap_EditMethods.cpp
	// because they do not need knowledge of block direction)
	fl_BlockLayout * pBlock = nullptr;
	if(count == 1 && text[0] == UCS_SPACE)
	{
		bool bLang = false, bMarker = false;

		XAP_App::getApp()->getPrefsValueBool(XAP_PREF_KEY_ChangeLanguageWithKeyboard, bLang);

		const UT_LangRecord * pLR = nullptr;
	
		if(bLang)
		{
			pLR = XAP_App::getApp()->getKbdLanguage();

			XAP_App::getApp()->getPrefsValueBool(XAP_PREF_KEY_DirMarkerAfterClosingParenthesis, bMarker);
		}

		if(bMarker && pLR)
		{
			pBlock = m_pLayout->findBlockAtPosition(getPoint());

			if(!pBlock)
				goto normal_insert;
			
			{
				UT_BidiCharType iDomDir = pBlock->getDominantDirection();
				
				UT_UCS4Char data[2];
				data[1] = *text;
		
				if(pLR->m_eDir == UTLANG_RTL && iDomDir != UT_BIDI_RTL)
				{
					data[0] = UCS_RLM;
				}
				else if(pLR->m_eDir == UTLANG_LTR  && iDomDir != UT_BIDI_LTR)
				{
					data[0] = UCS_LRM;
				}
				else
				{
					goto normal_insert;
				}

				return _charInsert(&data[0],2,bForce);
			}
		}
	}
	else if(count == 1 && text[0] == UCS_FF)
	{
		m_pDoc->beginUserAtomicGlob();
		bool b = _charInsert(text, count, bForce);
		if(b)
		{
			insertParagraphBreak();
		}
		m_pDoc->endUserAtomicGlob();
		return b;
	}
	else if(count == 1 && text[0] == UCS_VTAB)
	{
		m_pDoc->beginUserAtomicGlob();
		bool b = _charInsert(text, count, bForce);
		if(b)
		{
			insertParagraphBreak();
		}
		m_pDoc->endUserAtomicGlob();
		return b;
	}

normal_insert:
	return _charInsert(text, count, bForce);
}

bool FV_View::cmdStartList(const gchar * style)
{
	m_pDoc->beginUserAtomicGlob();
	fl_BlockLayout * pBlock = getCurrentBlock();
	pBlock->StartList( style);
	m_pDoc->endUserAtomicGlob();

	return true;
}


bool FV_View::cmdStopList(void)
{


	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	m_pDoc->beginUserAtomicGlob();
	fl_BlockLayout * pBlock = getCurrentBlock();
	m_pDoc->StopList(pBlock->getStruxDocHandle());
	m_pDoc->endUserAtomicGlob();

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	return true;
}


void FV_View::cmdCharDelete(bool bForward, UT_uint32 count)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	bool bisList = false;
	fl_BlockLayout * curBlock = nullptr;
	fl_BlockLayout * nBlock = nullptr;
	UT_uint32 iRealDeleteCount = 0;
	bool bSimple = false;

	if (!isSelectionEmpty() && !m_FrameEdit.isActive())
	{

		// Signal PieceTable Change
		_saveAndNotifyPieceTableChange();
		m_pDoc->disableListUpdates();

		_deleteSelection();

		_generalUpdate();

		// restore updates and clean up dirty lists
		m_pDoc->enableListUpdates();
		m_pDoc->updateDirtyLists();
		_fixInsertionPointCoords();
		_ensureInsertionPointOnScreen();
	}
	else if(m_FrameEdit.isActive())
    {
	  deleteFrame();
	}
	else
	{
		bool b_editStarted = false;
		bool b_delBlockWithFrames = false;
		//
		// Look to see if there is a tab - list label deal with these together
		//
		if((bForward == false) && (count == 1))
		{
		    UT_sint32 myCount= 0;
			if(isTabListBehindPoint(myCount) == true)
			{
				curBlock = _findBlockAtPosition(getPoint());
				nBlock = _findBlockAtPosition(getPoint()-myCount);
				if(nBlock == curBlock)
				{
					count = myCount;
					bisList = true;
				}
			}
		}
		if((bForward == true) && (count == 1))
		{
			if(isTabListAheadPoint() == true)
			{
//
// Check we're at the start of a block
//
				if(getPoint() == getCurrentBlock()->getPosition())
				{
					bisList = true;
					count = 2;
				}
			}
		}
//
// Move frames to a good position in the piece table when deleting a block (joining two paragraphs 
// together). Also keep frame at the same position with respect to the top of the first block.
// 
//
		fl_BlockLayout * pFirstBlock = nullptr;
		fl_BlockLayout * pSecondBlock = nullptr;

		if((bForward == false) && (count == 1)) 
		{
			fl_BlockLayout * pBlock = getCurrentBlock();
			if (pBlock && getPoint() == pBlock->getPosition())
			{
				fl_ContainerLayout * pPrevCL = pBlock->getPrev();
				while (pPrevCL && pPrevCL->getContainerType() == FL_CONTAINER_FRAME)
				{
					pPrevCL = pPrevCL->getPrev();
				}
				if (pPrevCL && pPrevCL->getContainerType() == FL_CONTAINER_BLOCK)
				{
					pFirstBlock = static_cast <fl_BlockLayout *> (pPrevCL);
					pSecondBlock = pBlock;
					if ((pFirstBlock->getNumFrames() > 0) || (pSecondBlock->getNumFrames() > 0))
					{
						b_delBlockWithFrames = true;
					}
				}
			}
		}
		else if((bForward == true) && (count == 1))
		{
			fl_BlockLayout * pBlock = getCurrentBlock();
			if (pBlock && (getPoint() == (pBlock->getPosition()+pBlock->getLength()-1)))
			{
				fl_ContainerLayout * pNextCL = pBlock->getNext();
				while (pNextCL && pNextCL->getContainerType() == FL_CONTAINER_FRAME)
				{
					pNextCL = pNextCL->getNext();
				}
				if (pNextCL && pNextCL->getContainerType() == FL_CONTAINER_BLOCK)
				{
					pFirstBlock = pBlock;
					pSecondBlock = static_cast <fl_BlockLayout *> (pNextCL);
					if ((pFirstBlock->getNumFrames() > 0) || (pSecondBlock->getNumFrames() > 0))
					{
						b_delBlockWithFrames = true;
					}
				}
			}
		}

		if (b_delBlockWithFrames)
		{
			_saveAndNotifyPieceTableChange();
			m_pDoc->beginUserAtomicGlob();
			m_pDoc->disableListUpdates();
			b_editStarted = true;

			UT_sint32 extraHeight = 0;
			bool b_evalHeightOfFirstBlock = false;
			UT_sint32 i = 0;
			UT_sint32 imax = pSecondBlock->getNumFrames();
			for(i=0; i < imax;i++)
			{
				fl_FrameLayout * pFL = pSecondBlock->getNthFrameLayout(i);
				if(pFL->isHidden() > FP_VISIBLE)
					continue;
				if(pFL->getContainerType() != FL_CONTAINER_FRAME)
				{
					UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
					continue;
				}
				if(pFL->getFramePositionTo() == FL_FRAME_POSITIONED_TO_BLOCK)
				{
					const PP_AttrProp* pAP = nullptr;
					const gchar * pszYPos = nullptr;
					double ypos = 0.;
					pFL->getAP(pAP);
					if(!pAP || !pAP->getProperty("ypos",pszYPos))
					{
						pszYPos = "0.0in";
					}
					if (!b_evalHeightOfFirstBlock)
					{
						extraHeight = pFirstBlock->getHeightOfBlock(false);
						b_evalHeightOfFirstBlock = true;
					}
					ypos = UT_convertToInches(pszYPos) + double(extraHeight)/double(UT_LAYOUT_RESOLUTION);
					UT_String sValY = UT_formatDimensionString(DIM_IN,ypos);
					const PP_PropertyVector frameProperties = {
						"ypos", sValY.c_str()
					};
					PT_DocPosition posStart = pFL->getPosition(true)+1;
					PT_DocPosition posEnd = posStart;
					UT_DebugOnly<bool> bRet = m_pDoc->changeStruxFmt(PTC_AddFmt,posStart, posEnd, PP_NOPROPS,
																	 frameProperties, PTX_SectionFrame);
					UT_ASSERT(bRet);
				}
			}
			imax = pFirstBlock->getNumFrames();
			for(i=0; i < imax;i++)
			{
				fl_FrameLayout * pFL = pFirstBlock->getNthFrameLayout(0);
				if(pFL->getContainerType() != FL_CONTAINER_FRAME)
				{
					UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
					break;
				}
				m_pLayout->relocateFrame(pFL,pSecondBlock);
			}
			if (bForward)
			{
				setPoint(pFirstBlock->getPosition()+pFirstBlock->getLength()-1);
			}
			else
			{
				setPoint(pSecondBlock->getPosition());
			}
		}
//
// Code to deal with deleting a footnote reference that embeds a footnote Layout.
//
		if(bForward)
		{
			if(!isInFootnote() && isInFootnote(getPoint() + count))
			{
				fl_FootnoteLayout * pFL = getClosestFootnote(getPoint() + count +1);
				count += pFL->getLength();
			}
			if(!isInEndnote() && isInEndnote(getPoint() + count))
			{
				fl_EndnoteLayout * pEL = getClosestEndnote(getPoint() + count +1);
				count += pEL->getLength();
			}
			if(m_pDoc->isTOCAtPos(getPoint()))
			{
				if(m_pDoc->isTOCAtPos(getPoint()-1))
				{
					m_iInsPoint--;
				}
				count++;

			}
		}
		else
		{
			if(!isInFootnote(getPoint()) && isInFootnote(getPoint() - count))
			{
				fl_FootnoteLayout * pFL = getClosestFootnote(getPoint());
				count += pFL->getLength();
			}
			else if(isInFootnote(getPoint()))
			{
				if(!isInFootnote(getPoint() - count))
				{
					return;
				}
				else if(!isInFootnote(getPoint() -2))
				{
//
// Don't delete the paragraph strux in footnote
//
					return;
				}
				else if(!isInFootnote(getPoint() -3))
				{
//
// Don't delete the paragraph strux in footnote
//
					return;
				}
			}
			else if(!isInEndnote() && isInEndnote(getPoint() - count))
			{
				fl_EndnoteLayout * pEL = getClosestEndnote(getPoint());
				count += pEL->getLength();
			}
			else if(isInEndnote(getPoint()))
			{
				if(!isInEndnote(getPoint() - count))
				{
					return;
				}
				else if(!isInEndnote(getPoint() -2))
				{
//
// Don't delete the paragraph strux in endnote
//
					return;
				}
				else if(!isInEndnote(getPoint() -3))
				{
//
// Don't delete the paragraph strux in endnote
//
					return;
				}

			}
			if(m_pDoc->isTOCAtPos(getPoint()-2))
			{
				count +=2;
			}
		}

		// Code that deals with deleting runs that do not like to be deleted ...  This handles runs
		// that return true for deleteFollowingIfAtInsPoint(). Runs in this category are typically
		// not visible on screen, such as hyperlinks and bookmarks, and are deleted through the main
		// menu. Such runs must not be deleted inadvertedly when pressing delete/backpace. Just to
		// exaplain why: for example, in Word bookmarks inadvertedly disappear when the bookmarked
		// text is edited; this is extremely annoying if such bookmarks are referenced from page
		// reference and similar fields, particularly if the document is long and contains many such
		// fields -- imagine printing a 200+ page document, only to discover that on page 178 you
		// have 'error: bookmark not found' where a page number should have been -- we want to
		// minimise this happening and will only delete such runs when (a) the user explicitely asks
		// to, or (b) they are inside a selection.
		if(!curBlock)
			curBlock = _findBlockAtPosition(getPoint());

		if(bForward && count == 1)
		{
			UT_return_if_fail( curBlock );
			
			fp_Run * pRun = curBlock->findRunAtOffset(getPoint() - curBlock->getPosition());

			UT_return_if_fail( pRun );

			UT_uint32 iLength = 0;
			while(pRun && (pRun->deleteFollowingIfAtInsPoint() && (getPoint() == curBlock->getPosition() + pRun->getBlockOffset())))
			{
				iLength += pRun->getLength();
				pRun = pRun->getNextRun();
			}

			_setPoint(m_iInsPoint + iLength);
			
		}
		else if(!bForward && count == 1)
		{
			UT_return_if_fail( curBlock );
			
			fp_Run * pRun = curBlock->findRunAtOffset(getPoint() - curBlock->getPosition());

			UT_return_if_fail( pRun );

			// back one further
			pRun = pRun->getPrevRun();

			UT_uint32 iLength = 0;
			while(pRun && (pRun->deleteFollowingIfAtInsPoint() && (getPoint() == curBlock->getPosition() + pRun->getBlockOffset())) )
			{
				iLength += pRun->getLength();
				pRun = pRun->getPrevRun();
			}

			_setPoint(m_iInsPoint - iLength);
			
		}

		// deal with character clusters, such as base char + vowel + tone mark in Thai
		UT_uint32 pos1 = getPoint();
		if(!bForward)
		{
			UT_ASSERT_HARMLESS( pos1 > count );
			pos1 -= count;
		}
		
		_adjustDeletePosition(pos1, count);

		if(bForward)
			_setPoint(pos1);
		else
			_setPoint(pos1 + count);

		
		// Code to deal with font boundary problem.
		// TODO: This should really be fixed by someone who understands
		// how this code works! In the meantime save current font to be
		// restored after character is deleted.

		PP_PropertyVector props_in;
		getCharFormat(props_in);
		const PP_PropertyVector properties = {
			"font-family", PP_getAttribute("font-family", props_in)
		};
		xxx_UT_DEBUGMSG(("deleteSpan - 1: Inital pos %d count %d \n",getPoint(),count));

		UT_uint32 amt = count;
		UT_uint32 posCur = getPoint();
		UT_uint32 nposCur = getPoint();
		bool fontFlag = false;

		if (!bForward)
		{

			if (!_charMotion(bForward,count, false))
			{
				UT_ASSERT(getPoint() <= posCur);
				xxx_UT_DEBUGMSG(("SEVIOR: posCur %d getPoint() %d \n",posCur,getPoint()));
				amt = posCur - getPoint();
			}

			posCur = getPoint();
			// Code to deal with change of font boundaries:
			if((posCur == nposCur) && (posCur > 0))
			{
				fontFlag = true;
				posCur--;
			}
		}
		else
		{
			PT_DocPosition posEOD;
			UT_DebugOnly<bool> bRes;

			bRes = getEditableBounds(true, posEOD);
			UT_ASSERT(bRes);
			UT_ASSERT(posCur <= posEOD);

			if (posEOD < (posCur+amt))
			{
				amt = posEOD - posCur;
			}
		}

		if(!curBlock)
			curBlock = _findBlockAtPosition(getPoint());

// 
// Code to check for a delete over a frame boundary.
//
		if(isInFrame(posCur) && !isInFrame(posCur+amt))
		{
			fl_FrameLayout * pFL = getFrameLayout(posCur+amt);
			if(pFL != nullptr)
			{
			  //
			  // Delete to edge of text box
			  //
			      PT_DocPosition posFrame = pFL->getPosition(true);
			      amt = posFrame + pFL->getLength() - posCur;
			}
			return;
		}
		if(!isInFrame(posCur) && isInFrame(posCur+amt) && (amt > 1))
		{
			fl_FrameLayout * pFL = getFrameLayout(posCur+amt);
			if(pFL != nullptr)
			{
			  //
			  // delete to start of text box
			  //
			      PT_DocPosition posFrame = pFL->getPosition(true);
			      amt = posCur + amt + 1 - posFrame;
			}
		}
//
// isInFrame will return true if we're right at the frame strux or right
// at the EndFrame strux. If we delete either we're screwed. Handle
// the cases.
//
// Later we want to be clever about moving the frame into a valid position
// in the new merged block. Just fix the crash for now.
//
		if(m_pDoc->isFrameAtPos(posCur) && isInFrame(posCur+amt)  )
		{
			return;
		}

		if(m_pDoc->isEndFrameAtPos(posCur))
		{
			return;
		}

		// Signal PieceTable Change
		if (!b_editStarted)
		{
			_saveAndNotifyPieceTableChange();
			b_editStarted = true;
		}
		if (amt > 0)
		{
			m_pDoc->disableListUpdates();

			nBlock = _findBlockAtPosition(getPoint());
			fl_AutoNumPtr pAuto2 = nBlock->getAutoNum();
			if (pAuto2)
			{
				pf_Frag_Strux* sdh = nBlock->getStruxDocHandle();
				if((bisList == true) && (pAuto2->getFirstItem() == sdh || pAuto2->getLastItem() == sdh))
				{
					m_pDoc->StopList(sdh);
					PT_DocPosition listPoint,posEOD;
					getEditableBounds(true, posEOD);
					listPoint = getPoint();
					fl_AutoNumPtr pAuto = nBlock->getAutoNum();
					if (pAuto)
					{
						if(listPoint + 2 <= posEOD)
							_setPoint(listPoint+2);
						else
							_setPoint(posEOD);
					}
				}
				else if(bisList == true)
				{

					m_pDoc->deleteSpan(posCur, posCur+amt,nullptr, iRealDeleteCount);
					nBlock->remItemFromList();
				}
				else
				{
					m_pDoc->deleteSpan(posCur, posCur+amt,nullptr, iRealDeleteCount);
				}
			}
			else
			{
				UT_DEBUGMSG(("deleteSpan - 2: posCur %d amt %d \n",posCur,amt));
				m_pDoc->deleteSpan(posCur, posCur+amt,nullptr, iRealDeleteCount);
			}

			if(fontFlag)
			{
				_makePointLegal();
				setCharFormat(properties);
			}
		}
//
// Dont leave a List field - tab on a line.
//
		if(isTabListAheadPoint())
		{
			UT_uint32 iRealDeleteCount2;

			m_pDoc->deleteSpan(getPoint(), getPoint()+2,nullptr,iRealDeleteCount2);
			iRealDeleteCount += iRealDeleteCount2;
		}
		else if(count == 1)
		{
			bSimple = true;
		}
		// restore updates and clean up dirty lists
		m_pDoc->enableListUpdates();
		m_pDoc->updateDirtyLists();

		_generalUpdate();

		_fixInsertionPointCoords();
		_ensureInsertionPointOnScreen();

		//special handling is required for delete in revisions mode
		//where we have to move the insertion point
		// only if we are deleting forward; if deleting backwards, the
		// code above already moved the insertion point
		//
		// Tomas, Oct 28, 2003
		// do this if we deleted fewer than count characters
		if(bForward && isMarkRevisions() && (iRealDeleteCount < count))
		{
			UT_ASSERT( iRealDeleteCount <= count );
			_charMotion(bForward,count - iRealDeleteCount);
		}
		if (b_delBlockWithFrames)
		{
			m_pDoc->endUserAtomicGlob();
		}
	}

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_setPoint(getPoint());
	if(!bSimple)
		notifyListeners(AV_CHG_MOTION | AV_CHG_ALL);
	else
		notifyListeners(AV_CHG_MOTION);

}



void FV_View::cmdScroll(AV_ScrollCmd cmd, UT_uint32 iPos)
{

#define HACK_LINE_HEIGHT				20 // TODO Fix this!!

	UT_sint32 lineHeight = iPos;
	bool bVertical = false;
	bool bHorizontal = false;

	if (lineHeight == 0)
		lineHeight = m_pG->tlu(HACK_LINE_HEIGHT);

	UT_sint32 yoff = m_yScrollOffset;
	UT_sint32 xoff = m_xScrollOffset;

	switch(cmd)
	{
	case AV_SCROLLCMD_PAGEDOWN:
		yoff += getWindowHeight();
		bVertical = true;
		break;
	case AV_SCROLLCMD_PAGEUP:
		yoff -= getWindowHeight();
		bVertical = true;
		break;
	case AV_SCROLLCMD_PAGELEFT:
		xoff -= getWindowWidth();
		bHorizontal = true;
		break;
	case AV_SCROLLCMD_PAGERIGHT:
		xoff += getWindowWidth();
		bHorizontal = true;
		break;
	case AV_SCROLLCMD_LINEDOWN:
		yoff += lineHeight;
		bVertical = true;
		break;
	case AV_SCROLLCMD_LINEUP:
		yoff -= lineHeight;
		bVertical = true;
		break;
	case AV_SCROLLCMD_LINELEFT:
		xoff -= lineHeight;
		bHorizontal = true;
		break;
	case AV_SCROLLCMD_LINERIGHT:
		xoff += lineHeight;
		bHorizontal = true;
		break;
	case AV_SCROLLCMD_TOTOP:
		yoff = 0;
		bVertical = true;
		break;
	case AV_SCROLLCMD_TOPOSITION:
		UT_ASSERT(UT_NOT_IMPLEMENTED);
		break;
	case AV_SCROLLCMD_TOBOTTOM:
		fp_Page* pPage = m_pLayout->getFirstPage();
		UT_sint32 iDocHeight = getPageViewTopMargin();
		while (pPage)
		{
			iDocHeight += pPage->getHeight() + getPageViewSep();
			pPage = pPage->getNext();
		}
		yoff = iDocHeight;
		bVertical = true;
		break;
	}

	if (yoff < 0)
		yoff = 0;

	bool bRedrawPoint = true;

	if (bVertical && (yoff != m_yScrollOffset))
	{
		sendVerticalScrollEvent(yoff);
		if ((cmd != AV_SCROLLCMD_PAGEUP
			 && cmd != AV_SCROLLCMD_PAGEDOWN))
		  bRedrawPoint = false;
//		UT_ASSERT(m_yScrollOffset == m_pG->getPrevYOffset());
	}

	if (xoff < 0)
		xoff = 0;

	if (bHorizontal && (xoff != m_xScrollOffset))
	{
		sendHorizontalScrollEvent(xoff);
		bRedrawPoint = false;
	}
	
	if (bRedrawPoint)
		_fixInsertionPointCoords();

}

bool FV_View::cmdSelectNoNotify(PT_DocPosition dpBeg, PT_DocPosition dpEnd)
{
	if (!isSelectionEmpty())
	{
		_clearSelection();
	}
	_setPoint(dpBeg);
	_setSelectionAnchor();
	m_Selection.setSelectionLeftAnchor(dpBeg);
	if(dpBeg < dpEnd - 2)
	{
	     if(m_pDoc->isTableAtPos(dpEnd) && m_pDoc->isEndTableAtPos(dpEnd-1))
	     {
	          dpEnd--;
	     }
	     if(m_pDoc->isCellAtPos(dpEnd))
	     {
	          dpEnd--;
	     }
	}
	m_Selection.setSelectionRightAnchor(dpEnd);
	_setPoint (dpEnd);
// This is no good in abicommand	
//	UT_ASSERT(!isSelectionEmpty());

	if (dpBeg == dpEnd)
	{
		return false;
	}
	return true;
}

void FV_View::cmdSelect(PT_DocPosition dpBeg, PT_DocPosition dpEnd)
{
	if(cmdSelectNoNotify(dpBeg, dpEnd))
	{  
		_drawSelection();
		notifyListeners(AV_CHG_EMPTYSEL);
	}
}

void FV_View::cmdSelect( const std::pair< PT_DocPosition, PT_DocPosition >& range )
{
	if( range.first == 0 && range.second == 0 )
	{
		cmdUnselectSelection();
		return;
	}

	cmdSelect( range.first, range.second );
}



#define IS_SELECTALL(a, b) ((a) == FV_DOCPOS_BOD && (b) == FV_DOCPOS_EOD)

void FV_View::cmdSelect(UT_sint32 xPos, UT_sint32 yPos, FV_DocPos dpBeg, FV_DocPos dpEnd)
{
	UT_DEBUGMSG(("Double click on mouse \n"));

	warpInsPtToXY(xPos, yPos,true);
	PT_DocPosition iPosLeft = _getDocPos(dpBeg, false);
	PT_DocPosition iPosRight = _getDocPos(dpEnd, false);
	if(iPosLeft > iPosRight)
	{
		return;
	}
	if(!isInFrame(iPosLeft) && isInFrame(iPosRight))
	{
		fl_FrameLayout * pFL = getFrameLayout(iPosRight);
		iPosRight =pFL->getPosition(true)-1;
	}
	if(isInFrame(iPosLeft) && !isInFrame(iPosRight))
	{
		fl_FrameLayout * pFL = getFrameLayout(iPosLeft);
		iPosRight =pFL->getPosition(true) + pFL->getLength() -1;
	}
	if(iPosLeft == iPosRight) return;
//
// Code to select a paragraph break on selectLine if on first line of a Block.
//
	bool bRedraw = false;
	if((dpBeg == FV_DOCPOS_BOL) || (dpBeg == FV_DOCPOS_BOP) || (dpBeg == FV_DOCPOS_BOD))
	{
		fl_BlockLayout * pBlock =  _findBlockAtPosition(iPosLeft);
		if(pBlock)
		{
			UT_sint32 x, y, x2, y2, h;
			bool b;
			fp_Run* pRun = pBlock->findPointCoords(m_iInsPoint, false, x, y, x2, y2, h, b);
			if(pRun)
			{
				fp_Line * pLine = pRun->getLine();
				if(pLine == static_cast<fp_Line *>(pBlock->getFirstContainer()))
				{
					PT_DocPosition iPosNew = pBlock->getPosition() -1;
					if(iPosNew < iPosLeft)
					{
						iPosLeft = iPosNew;
					}
					bRedraw = true; // Need to trick a global redraw in 
					// header/footer
				}
			}
		}
	}
	cmdSelect (iPosLeft, iPosRight);
	if(bRedraw && isHdrFtrEdit())
	{
		cmdSelect (iPosLeft+1, iPosRight);
	}
}

void FV_View::cmdHyperlinkJump(UT_sint32 xPos, UT_sint32 yPos)
{
	_clearSelection();
	warpInsPtToXY(xPos, yPos,true);

	fl_BlockLayout * pBlock = getCurrentBlock();
	PT_DocPosition iRelPos = getPoint() - pBlock->getPosition(false);

	fp_Run *pRun = pBlock->getFirstRun();
	while (pRun && pRun->getBlockOffset()+ pRun->getLength() < iRelPos)
		pRun= pRun->getNextRun();

	UT_return_if_fail(pRun);
	pRun->getPrevRun();

	UT_return_if_fail(pRun);
#if 0
	if(pRun->getType()== FPRUN_FMTMARK || pRun->getType()== FPRUN_HYPERLINK || pRun->getType()== FPRUN_BOOKMARK)
		pRun  = pRun->getNextRun();

	UT_ASSERT(pRun);
#endif
	fp_HyperlinkRun * pH = pRun->getHyperlink();

	UT_return_if_fail(pH);

	const gchar * pTarget = pH->getTarget();

	if(*pTarget == '#')
		pTarget++;

	UT_UCS4String pJump(pTarget);

	gotoTarget(AP_JUMPTARGET_BOOKMARK, pJump.ucs4_str());
}


void FV_View::cmdHyperlinkJump(PT_DocPosition pos)
{
	fp_HyperlinkRun * pH = static_cast<fp_HyperlinkRun *>(getHyperLinkRun(pos));
	UT_return_if_fail(pH);
	if(pH->getHyperlinkType() == HYPERLINK_ANNOTATION)
	{
		fp_AnnotationRun * pAN = static_cast<fp_AnnotationRun *>(pH);
		if(!pAN->displayAnnotations())
		{
			UT_DEBUGMSG(("Can only directly edit if we in show annotation mode \n"));
			UT_DEBUGMSG(("Should pop up a dialog to say this, but not now \n?"));
			return;
		}
		UT_uint32 aid = pAN->getPID();
		fl_AnnotationLayout * pAL = getAnnotationLayout(aid);
		if(pAL == nullptr)
		{
			return;
		}
		//
		// Put caret just past the annotation field
		//
		PT_DocPosition posAn = pAL->getPosition();
		setPoint(posAn);
		_fixInsertionPointCoords();
		_ensureInsertionPointOnScreen();
		notifyListeners(AV_CHG_MOTION);
		_generalUpdate();
		return;
	}
	const gchar * pTarget = pH->getTarget();

	if(*pTarget == '#')
		pTarget++;

	UT_UCS4String pJump(pTarget);

	gotoTarget(AP_JUMPTARGET_BOOKMARK, pJump.ucs4_str());
}

void FV_View::cmdHyperlinkCopyLocation(PT_DocPosition pos)
{
	fp_HyperlinkRun * pH = static_cast<fp_HyperlinkRun *>(getHyperLinkRun(pos));
	if(!pH)
		return;

	const gchar * pTarget = pH->getTarget();

	if(!pTarget || !*pTarget || !strcmp(pTarget,"#"))
		return;

	//skip over internal anchors
	if(*pTarget == '#')
		pTarget++;

	//copy the target to the clipboard
	copyTextToClipboard(pTarget, true);
}


void FV_View::cmdUndo(UT_uint32 count)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	if (!isSelectionEmpty())
		_clearSelection();

	// Temporarily disable smart quotes
	// This allows the smart quote to be reverted by undo.
	m_bAllowSmartQuoteReplacement = false;

	// Signal PieceTable Change
	m_pDoc->notifyPieceTableChangeStart();

	// Turn off list updates
	m_pDoc->disableListUpdates();

// Don't update tables until finished

	m_pDoc->setDontImmediatelyLayout(true);

	// Remember the current position, We might need it later.
	rememberCurrentPosition();
	UT_DEBUGMSG(("SEVIOR: undoing %d operations \n",count));
	m_pDoc->undoCmd(count);
	allowChangeInsPoint();
	m_pDoc->setDontImmediatelyLayout(false);

//
// Now do a general update to make everything look good again.
//
	_generalUpdate();

	notifyListeners(AV_CHG_DIRTY);

// Look to see if we need the saved insertion point after the undo
// 	if(needSavedPosition())
// 	{
//
// We do, so restore insertion point to that value.
// 		_setPoint(getSavedPosition());
// 		clearSavedPosition();
// 	}
	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();

	// Signal PieceTable Changes have finished
	m_pDoc->notifyPieceTableChangeEnd();
	m_iPieceTableState = 0;
	// Move insertion point out of field run if it is in one
	//
	_charMotion(true, 0);
//
// Do a complete update coz who knows what happened in the undo!
//
	notifyListeners(AV_CHG_ALL);
	PT_DocPosition posEnd = 0;
	PT_DocPosition posBOD = 0;
	getEditableBounds(true, posEnd);
	getEditableBounds(true, posBOD);
	bool bOK = true;
	while(bOK && !isPointLegal() && (getPoint() < posEnd))
	{
		bOK = _charMotion(true,1);
	}

	bOK = true;
	while(bOK && !isPointLegal() && (getPoint() > posBOD))
	{
		bOK = _charMotion(false,1);
	}
	setCursorToContext();

	_updateInsertionPoint();

	// Reenable smart quotes
	m_bAllowSmartQuoteReplacement = true;
}

void FV_View::cmdRedo(UT_uint32 count)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION
	
	if (!isSelectionEmpty())
		_clearSelection();

	// Temporarily disable smart quotes
	// This allows the smart quote to be reverted by undo.
	m_bAllowSmartQuoteReplacement = false;

	// Signal PieceTable Change
	m_pDoc->notifyPieceTableChangeStart();

	// Turn off list updates
	m_pDoc->disableListUpdates();
	m_pDoc->setDontImmediatelyLayout(true);

	// Remember the current position, We might need it later.
	rememberCurrentPosition();

	m_pDoc->redoCmd(count);
	allowChangeInsPoint();

// Look to see if we need the saved insertion point after the undo
// 	if(needSavedPosition())
// 	{
//
// We do, so restore insertion point to that value.
//
// 		_setPoint(getSavedPosition());
// 		clearSavedPosition();
// 	}
	m_pDoc->setDontImmediatelyLayout(false);

	// Signal PieceTable Changes have finished
	m_pDoc->notifyPieceTableChangeEnd();
	m_iPieceTableState = 0;

	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();

	_generalUpdate();

//
// Do a complete update coz who knows what happened in the undo!
//
	PT_DocPosition posEnd = 0;
	getEditableBounds(true, posEnd);
	bool bOK = true;
	bool bMoved = false;
	while(bOK && !isPointLegal() && (getPoint() < posEnd))
	{
		bOK = _charMotion(true,1);
		bMoved = true;
	}
	if(getPoint() > posEnd)
	{
		setPoint(posEnd);
		bMoved = true;
	}

	bOK = true;
	while(bOK && !isPointLegal() && (getPoint() > 2))
	{
		bOK = _charMotion(false,1);
		bMoved = true;
	}
	if(!bMoved && (getPoint() != posEnd) )
	{
		bOK = _charMotion(true,1);
		bOK = _charMotion(false,1);
	}

	setCursorToContext();
	_updateInsertionPoint();
	notifyListeners(AV_CHG_ALL);

	// Reenable smart quotes
	m_bAllowSmartQuoteReplacement = true;
}

UT_Error FV_View::cmdSave(void)
{
	// transfer any persistent properties into the doc
	const PP_PropertyVector ppProps = getViewPersistentProps();
	m_pDoc->setProperties(ppProps);
	_updateDatesBeforeSave(false);

	UT_Error error = m_pDoc->save();
	if (!error)
	{
		notifyListeners(AV_CHG_SAVE);
	}
	return error;
}

UT_Error FV_View::cmdSaveAs(const char * szFilename, int ieft, bool cpy)
{
	// transfer any persistent properties into the doc
	const PP_PropertyVector ppProps = getViewPersistentProps();
	m_pDoc->setProperties(ppProps);
	_updateDatesBeforeSave(true);

	UT_Error error = static_cast<AD_Document*>(m_pDoc)->saveAs(szFilename, ieft, cpy);
	if (!error && cpy)
	{
		notifyListeners(AV_CHG_SAVE);
	}
	return error;
}

UT_Error FV_View::cmdSaveAs(const char * szFilename, int ieft)
{
  return cmdSaveAs(szFilename, ieft, true);
}


void FV_View::cmdCut(void)
{
	if (isSelectionEmpty())
	{
		// clipboard does nothing if there is no selection
		return;
	}
	if(m_Selection.getSelectionMode() == FV_SelectionMode_TableColumn)
	{
		PD_DocumentRange * pDR = m_Selection.getNthSelection(0);
		PT_DocPosition pos = 0;
		if(pDR)
		{
		    pos = pDR->m_pos1 +1;
		}
		else
		{
		    pos = getSelectionAnchor();
		    if(pos > getPoint())
		    {
			pos = getPoint();
		    }
		}
		_clearSelection();
		cmdDeleteCol(pos);
		return;
	}
	if(m_Selection.getSelectionMode() == FV_SelectionMode_TableRow)
	{
		PD_DocumentRange * pDR = m_Selection.getNthSelection(0);
		PT_DocPosition pos = 0;
		if(pDR)
		{
		    pos = pDR->m_pos1 +1;
		}
		else
		{
		    pos = getSelectionAnchor();
		    if(pos > getPoint())
		    {
			pos = getPoint();
		    }
		}
		_clearSelection();
		cmdDeleteRow(pos);
		return;
	}
	// Signal PieceTable Change
	m_pDoc->notifyPieceTableChangeStart();

	//
	// Disable list updates until after we've finished
	//
	m_pDoc->disableListUpdates();
	cmdCopy(true);
	_deleteSelection();

	// Signal PieceTable Changes have finished
	m_pDoc->notifyPieceTableChangeEnd();
	m_iPieceTableState = 0;
	_generalUpdate();

	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();

	_setPoint(getPoint());
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_ALL);

	m_SelectionHandles.hide();
}

// bToClipboard is true if you want to copy to the CLIPBOARD
// selection on UNIX, as opposed to some PRIMARY selection
void FV_View::cmdCopy(bool bToClipboard)
{
	if (isSelectionEmpty())
	{
		// clipboard does nothing if there is no selection
		return;
	}

	PD_DocumentRange dr;
	getDocumentRangeOfCurrentSelection(&dr);
	m_pApp->copyToClipboard(&dr, bToClipboard);
	notifyListeners(AV_CHG_CLIPBOARD);
}

void FV_View::cmdPaste(bool bHonorFormatting)
{
//
// Look to see if should paste a table column or row
//
	
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	if((m_Selection.getPrevSelectionMode() == FV_SelectionMode_TableColumn)
	   || (m_Selection.getPrevSelectionMode() == 	FV_SelectionMode_TableRow))
	{
		if(isInTable())
		{
			fl_TableLayout * pTab = getTableAtPos(getPoint());
			if(pTab && pTab == m_Selection.getTableLayout())
			{
				m_Selection.pasteRowOrCol();

				// FIXME does this section get affected
				// by scrolling too?

				return;
			}
		}
	}

	// set UAG markers around everything that the actual paste does
	// so that undo/redo will treat it as one step.

	m_pDoc->beginUserAtomicGlob();

	// Signal PieceTable Change
	m_pDoc->notifyPieceTableChangeStart();

	//
	// Disable list updates until after we've finished
	//
	m_pDoc->disableListUpdates();
	m_pDoc->setDoingPaste();
	setCursorWait();
	m_pDoc->setDontImmediatelyLayout(true);
	_doPaste(true, bHonorFormatting);
	// restore updates and clean up dirty lists
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();
	clearCursorWait();

	// Signal PieceTable Changes have finished
	m_pDoc->notifyPieceTableChangeEnd();
	m_iPieceTableState = 0;

	m_pDoc->clearDoingPaste();
	m_pDoc->endUserAtomicGlob();
	m_iPieceTableState = 0;
	// Move insertion point out of field run if it is in one
	//
	_charMotion(true, 0);
	_makePointLegal();
//
// Do a complete update coz who knows what happened in the paste!
//
	
	// force update the screen before leaving the current view

	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_ALL);
	
}

/*!
 * Paste Special: like cmdPaste() but pastes the clipboard data in the
 * explicitly chosen format instead of the best available.
 */
void FV_View::cmdPasteAs(const char * szMimeType)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	if((m_Selection.getPrevSelectionMode() == FV_SelectionMode_TableColumn)
	   || (m_Selection.getPrevSelectionMode() == 	FV_SelectionMode_TableRow))
	{
		if(isInTable())
		{
			fl_TableLayout * pTab = getTableAtPos(getPoint());
			if(pTab && pTab == m_Selection.getTableLayout())
			{
				m_Selection.pasteRowOrCol();
				return;
			}
		}
	}

	m_pDoc->beginUserAtomicGlob();
	m_pDoc->notifyPieceTableChangeStart();
	m_pDoc->disableListUpdates();
	m_pDoc->setDoingPaste();
	setCursorWait();
	m_pDoc->setDontImmediatelyLayout(true);
	_doPaste(true, true, szMimeType);
	m_pDoc->enableListUpdates();
	m_pDoc->updateDirtyLists();
	clearCursorWait();
	m_pDoc->notifyPieceTableChangeEnd();
	m_iPieceTableState = 0;
	m_pDoc->clearDoingPaste();
	m_pDoc->endUserAtomicGlob();
	m_iPieceTableState = 0;
	_charMotion(true, 0);
	_makePointLegal();
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_ALL);
}

void FV_View::cmdPasteSelectionAt(UT_sint32 xPos, UT_sint32 yPos)
{
	// this is intended for the X11 middle mouse paste trick.
	//
	// if this view has the selection, we need to remember it
	// before we warp to the given (x,y) -- or else there won't
	// be a selection to paste when get there.	this is sort of
	// back door hack and should probably be re-thought.

	// set UAG markers around everything that the actual paste does
	// so that undo/redo will treat it as one step.

	m_pDoc->beginUserAtomicGlob();

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	if (!isSelectionEmpty())
		m_pApp->cacheCurrentSelection(this);
	cmdCopy(false);
	warpInsPtToXY(xPos,yPos,true);
	_doPaste(false, true);
	m_pApp->cacheCurrentSelection(nullptr);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_fixInsertionPointCoords();
	_ensureInsertionPointOnScreen();

	m_pDoc->endUserAtomicGlob();
	m_prevMouseContext =  EV_EMC_TEXT;
	notifyListeners(AV_CHG_ALL);
}

UT_Error FV_View::cmdDeleteBookmark(const char* szName)
{
	return _deleteBookmark(szName, true);
}

UT_Error FV_View::cmdDeleteHyperlink()
{
	PT_DocPosition pos = getPoint();
	UT_DEBUGMSG(("fv_View::cmdDeleteHyperlink: pos %d\n", pos));
	UT_Error err= _deleteHyperlink(pos,true);
	m_prevMouseContext =  EV_EMC_TEXT;
	setCursorToContext();
	notifyListeners(AV_CHG_ALL);
	return err;
}


UT_Error FV_View::cmdHyperlinkStatusBar(UT_sint32 xPos, UT_sint32 yPos)
{
	UT_sint32 xClick, yClick;
	fp_Page* pPage = _getPageForXY(xPos, yPos, xClick, yClick);

	PT_DocPosition pos;
	bool bBOL = false;
	bool bEOL = false;
	bool isTOC = false;
	pPage->mapXYToPosition(xClick, yClick, pos, bBOL, bEOL,isTOC);

	// now get the run at the position and the hyperlink run
	fp_HyperlinkRun * pH1 = nullptr;

	fl_BlockLayout *pBlock = _findBlockAtPosition(pos);
	PT_DocPosition curPos = pos - pBlock->getPosition(false);

	fp_Run * pRun = pBlock->getFirstRun();

	//find the run at pos1
	while(pRun && pRun->getBlockOffset() <= curPos)
		pRun = pRun->getNextRun();

	// this sometimes happens, not sure why
	//UT_ASSERT(pRun);
	if(!pRun)
		return false;

	// now we have the run immediately after the run in question, so
	// we step back
	pRun = pRun->getPrevRun();
	UT_ASSERT(pRun);
	if(!pRun)
		return false;

	xxx_UT_DEBUGMSG(("fv_View::cmdHyperlinkStatusBar: run 0x%x, type %d\n", pRun,pRun->getType()));
	pH1 = pRun->getHyperlink();

	// this happens after a deletion of a hyperlink
	// the mouse processing is in the state of belief
	// that the processing has not finished yet -- this is not specific
	// to hyperlinks, it happens with anything on the context menu, except
	// it goes unobserved since the cursor does not change
	//UT_ASSERT(pH1);
	if(!pH1)
		return false;
	xxx_UT_DEBUGMSG(("fv_View::cmdHyperlinkStatusBar: msg [%s]\n",pH1->getTarget()));
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (getParentData());
	if(pH1->getHyperlinkType() ==  HYPERLINK_NORMAL)
	{
	    UT_UTF8String url = pH1->getTarget();
	    url.decodeURL();
	    pFrame->setStatusMessage(url.utf8_str());
	}
	else
	{
	    if(!isAnnotationPreviewActive())
	    {
	         UT_DEBUGMSG(("popup Annotation preview \n"));
	    }
	}
	return true;
}

bool FV_View::cmdEditAnnotationWithDialog(UT_uint32 aID)
{
	// kill the annotation preview popup if needed
	if(isAnnotationPreviewActive())
		killAnnotationPreview();

	//
	// Get the text fromt he annotation
	//

	// TODO maybe we should not exit if annotation is not present (ex. auto-generated annotations may become not be editable!)
	std::string sText;
	std::string sTitle2;
	std::string sAuthor2;
	bool b = getAnnotationText(aID,sText);
	if(!b)
		return false;
	
	// Optional fields
	getAnnotationTitle(aID,sTitle2);
	getAnnotationAuthor(aID,sAuthor2);
	
	// edit annotation
	
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (getParentData());
	UT_return_val_if_fail(pFrame, false);
	
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail (pApp, false);
	
	pFrame->raise();
	
	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());
	
	AP_Dialog_Annotation * pDialog
		= static_cast<AP_Dialog_Annotation *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_ANNOTATION));
	UT_return_val_if_fail (pDialog, false);
	
	// set initial annotation properties
	// TODO add support for all fields
	pDialog->setTitle(sTitle2);
	pDialog->setAuthor(sAuthor2);
	pDialog->setDescription(sText);
	
	// run the dialog
	
	UT_DEBUGMSG(("cmdEditAnnotationWithDialog: Drawing annotation dialog...\n"));
	pDialog->runModal(pFrame);
	bool bOK = (pDialog->getAnswer() == AP_Dialog_Annotation::a_OK);
	bool bApply = (pDialog->getAnswer() == AP_Dialog_Annotation::a_APPLY);
	fl_AnnotationLayout * pAL = nullptr;

	if (bOK)
	{
		UT_DEBUGMSG(("cmdEditAnnotationWithDialog: Annotation id(\"%d\") edited \n",aID));
		
		for(UT_sint32 i = 0;i < pApp->getFrameCount();++i)
		{
			pApp->getFrame(i)->updateTitle ();
		}	  
		
		const std::string & sDescr = pDialog->getDescription();
		const std::string & sTitle = pDialog->getTitle();
		const std::string & sAuthor = pDialog->getAuthor();
//		bool bReplaceSelection = false;
		
		b = setAnnotationText(aID,sDescr,sAuthor,sTitle);
	}
	else if (bApply)
	{
		pAL = insertAnnotationDescription(aID, pDialog);
		UT_return_val_if_fail(pAL, false);
	}
	// release the dialog
	pDialogFactory->releaseDialog(pDialog);
	//
	// Select the text
	//
	pAL = getAnnotationLayout(aID);
	if(!pAL)
	  return false;
	selectAnnotation(pAL);

	return true;	
}

fl_AnnotationLayout *FV_View::insertAnnotationDescription(UT_uint32 aID, AP_Dialog_Annotation *pDialog)
{
	fl_AnnotationLayout *pAL = getAnnotationLayout(aID);
	UT_return_val_if_fail(pAL, nullptr);

	UT_UCS4String sDescr(pDialog->getDescription());  
	pf_Frag_Strux *sdhAnn = pAL->getStruxDocHandle();
	const pf_Frag_Strux *sdhEnd = nullptr;

	getDocument()->getNextStruxOfType(sdhAnn, PTX_EndAnnotation, &sdhEnd);
	UT_return_val_if_fail(sdhEnd, nullptr);

	// Start of the text covered by the annotations
	PT_DocPosition posStart = getDocument()->getStruxPosition(sdhEnd); 
	posStart++;

	fp_Run *pRun = getHyperLinkRun(posStart);
	UT_return_val_if_fail(pRun, nullptr);

	pRun = pRun->getNextRun();
	while (pRun && (pRun->getType() != FPRUN_HYPERLINK))
		pRun = pRun->getNextRun();
	UT_return_val_if_fail(pRun, nullptr);
	UT_return_val_if_fail(pRun->getType() == FPRUN_HYPERLINK, nullptr);

	PT_DocPosition posEnd = pRun->getBlock()->getPosition(false) + pRun->getBlockOffset();

	if(posStart > posEnd)
		posStart = posEnd;

	cmdSelect(posStart, posEnd);
	cmdCharInsert(sDescr.ucs4_str(), sDescr.size());

	return pAL;
}

UT_Error FV_View::cmdInsertHyperlink(const char * szName, const char * szTitle)
{
	bool bRet;

	if (!szName) szName = "";
	if (!szTitle) szTitle = "";

	PT_DocPosition posStart = getPoint();
	PT_DocPosition posEnd = posStart;
	PT_DocPosition iPointOrig = posStart;
	PT_DocPosition iAnchorOrig = m_Selection.getSelectionAnchor();

	if (!isSelectionEmpty())
	{
		if (m_Selection.getSelectionAnchor() < posStart)
		{
			posStart = m_Selection.getSelectionAnchor();
		}
		else
		{
			posEnd = m_Selection.getSelectionAnchor();
		}

	}
	else
	{
		//No selection
		XAP_Frame * pFrame = static_cast<XAP_Frame *>(getParentData());
		UT_ASSERT((pFrame));

		pFrame->showMessageBox(AP_STRING_ID_MSG_HyperlinkNoSelection, XAP_Dialog_MessageBox::b_O, XAP_Dialog_MessageBox::a_OK);
		return false;
	}

	bool relLink = false;
	if (!UT_go_path_is_uri(szName))
		relLink = m_pDoc->isBookmarkRelativeLink(szName);
	// TODO: After strings freeze is lifted, we should
	// TODO: display a message if relLink is true but
	// TODO: szName does not stat.

	if(!UT_go_path_is_uri(szName) && m_pDoc->isBookmarkUnique(szName) && !relLink)
	{
		//No bookmark of that name in document, tell user.
		XAP_Frame * pFrame = static_cast<XAP_Frame *>(getParentData());
		UT_ASSERT((pFrame));

		pFrame->showMessageBox(AP_STRING_ID_MSG_HyperlinkNoBookmark, 
				       XAP_Dialog_MessageBox::b_O, 
				       XAP_Dialog_MessageBox::a_OK, 
				       szName);
	}

	// Hack for bug 2940
	if (posStart == 1) posStart++;

	// the selection has to be within a single block
	// we could implement hyperlinks spaning arbitrary part of the document
	// but then we could not use <a href=> </a> in the output and
	// I see no obvious need for hyperlinks to span more than a single block
	fl_BlockLayout * pBl1 = _findBlockAtPosition(posStart);
	fl_BlockLayout * pBl2 = _findBlockAtPosition(posEnd);
//
// Handle corner case of selection from outside the left column
//
	if(isInFootnote(posStart))
	{
		if((pBl1 != nullptr) && (pBl1->getPosition(true) == posStart))
		{
			if(posEnd > posStart+1)
			{
				posStart++;
			}
		}
	}
	if(isInEndnote(posStart))
	{
		if((pBl1 != nullptr) && (pBl1->getPosition(true) == posStart))
		{
			if(posEnd > posStart+1)
			{
				posStart++;
			}
		}
	}
	if(pBl1 != pBl2)
	{
		XAP_Frame * pFrame = static_cast<XAP_Frame *>(getParentData());
		UT_ASSERT((pFrame));

		pFrame->showMessageBox(AP_STRING_ID_MSG_HyperlinkCrossesBoundaries, XAP_Dialog_MessageBox::b_O, XAP_Dialog_MessageBox::a_OK);

		return false;
	}
	if(isTOCSelected())
	{
//
// Fixme place message box here
//
		return false;

	}
	// Silently fail (TODO: pop up message) if we try to nest hyperlinks.
	if (_getHyperlinkInRange(posStart, posEnd) != nullptr)
		return false;
//
// Under sum1 induced conditions posEnd could give the same block pointer
// despite being past the end of the block. This extra fail-safe code
// prevents this.
//
	if((pBl1->getPosition() + pBl1->getLength() -1) < posEnd)
	{
		return false;
	}

	std::string target;

	if(UT_go_path_is_uri(szName) || relLink)
	{
		target = szName;
	}
	else
	{
		target = "#";
		target += szName;
	}

	PP_PropertyVector pAttr = {
		"xlink:href", target
	};
	if (szTitle && *szTitle)
	{
		pAttr.push_back("xlink:title");
		pAttr.push_back(szTitle);
	}

	UT_DEBUGMSG(("fv_View::cmdInsertHyperlink: target \"%s\"\n", target.c_str()));

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	// we first insert the end run, so that we can use it as a stop
	// after inserting the start run when marking the runs in between
	// as a hyperlink
	bRet = m_pDoc->insertObject(posEnd, PTO_Hyperlink, PP_NOPROPS, PP_NOPROPS);

	if(bRet)
	{
		bRet = m_pDoc->insertObject(posStart, PTO_Hyperlink, pAttr, PP_NOPROPS);
	}

	if(bRet)
	{
		// because we have inserted two objects around the selection
		// boundaries the original insetion point and selection anchor
		// are now shifted, so we need to fix them
		setPoint(iPointOrig+1);
		m_Selection.setSelectionAnchor(iAnchorOrig + 1);
	}

	// Signal piceTable is stable again
	_restorePieceTableState();

	_generalUpdate();

	return bRet;

}

/******************************************************************/

void FV_View::getCmdInsertRangeVariables( PT_DocPosition& posStart,
										  PT_DocPosition& posEnd,
										  fl_BlockLayout*& pBL1,
										  fl_BlockLayout*& pBL2 )
{
	posStart = getPoint();
	posEnd = posStart;
	
	if (!isSelectionEmpty())
	{
		if (m_Selection.getSelectionAnchor() < posStart)
		{
			posStart = m_Selection.getSelectionAnchor();
		}
		else
		{
			posEnd = m_Selection.getSelectionAnchor();
		}
	}

	// we cannot bookmark lesser position than 2, because the bookmark object has to be located
	// withing a block; this was the cause of bug 7128
	// (we might consider one day to allow the bookmark object before the first block strux, but the
	// complications this would cause are possibly not worth it)
	if(posStart < 2)
		posStart = 2;
	
	posEnd++;

	pBL1 =_findBlockAtPosition(posStart);
	pBL2 =_findBlockAtPosition(posEnd);
	
	//
	// Handle corner case of selection from outside the left column
	//
	if((pBL1!= nullptr) && isInFootnote(posStart) && (pBL1->getPosition(true) == posStart))
	{
		if(posEnd > posStart+1)
		{
			posStart++;
		}
	}
	if((pBL1 != nullptr) && isInEndnote(posStart) && (pBL1->getPosition(true) == posStart))
	{
		if(posEnd > posStart+1)
		{
			posStart++;
		}
	}
}

										  
UT_Error FV_View::cmdInsertBookmark(const char * szName)
{
	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();
	bool bRet;

	PT_DocPosition posStart = 0, posEnd = 0;
	fl_BlockLayout* pBL1 = nullptr;
	fl_BlockLayout* pBL2 = nullptr;
	getCmdInsertRangeVariables( posStart, posEnd, pBL1, pBL2 );
	
	if(pBL1 != pBL2)
	{
		//
		// Fixme put message boxes here
		//
		_restorePieceTableState();
		return false;
	}
	if(isTOCSelected())
	{
		//
		// Fixme put message boxes here
		//
		_restorePieceTableState();
		return false;
	}

	if(!m_pDoc->isBookmarkUnique(static_cast<const gchar*>(szName)))
	{
		XAP_Frame * pFrame = static_cast<XAP_Frame *>(getParentData());
		XAP_Dialog_MessageBox::tAnswer ans = XAP_Dialog_MessageBox::a_NO;

		if(pFrame)
			ans = pFrame->showMessageBox(AP_STRING_ID_MSG_BookmarkAlreadyExists,
									XAP_Dialog_MessageBox::b_YN, XAP_Dialog_MessageBox::a_NO);

		if(ans == XAP_Dialog_MessageBox::a_YES)
		{
			//bookmark already exists -- remove it and then reinsert
			UT_DEBUGMSG(("fv_View::cmdInsertBookmark: bookmark \"%s\" exists - removing\n", szName));
			_deleteBookmark(static_cast<const gchar*>(szName), false, &posStart, &posEnd);
		}
		else
		{
			xxx_UT_DEBUGMSG(("User canceled bookmark replacement\n"));
			return false;
		}
	}


	char name[BOOKMARK_NAME_SIZE + 1];
	strncpy(name, szName, BOOKMARK_NAME_SIZE);
	name[BOOKMARK_NAME_SIZE] = 0;

	PP_PropertyVector pAttr = {
		"name",	name,
		"type",	"start"
	};

	UT_DEBUGMSG(("fv_View::cmdInsertBookmark: szName \"%s\"\n", szName));

	bRet = m_pDoc->insertObject(posStart, PTO_Bookmark, pAttr, PP_NOPROPS);

	if(bRet)
	{
		// override the type to mark the end.
		pAttr [3] = "end";
		bRet = m_pDoc->insertObject(posEnd, PTO_Bookmark, pAttr, PP_NOPROPS);
	}


	// Signal piceTable is stable again
	_restorePieceTableState();
	_generalUpdate();

	return bRet;

}

UT_Error
FV_View::cmdInsertXMLID( const std::string& xmlid )
{
	UT_DEBUGMSG(("fv_View::cmdInsertXMLID: xmlid:%s\n", xmlid.c_str()));

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();
	UT_Error ret;

	PT_DocPosition posStart = 0, posEnd = 0;
	fl_BlockLayout* pBL1 = nullptr;
	fl_BlockLayout* pBL2 = nullptr;
	getCmdInsertRangeVariables( posStart, posEnd, pBL1, pBL2 );
	UT_DEBUGMSG(("fv_View::cmdInsertXMLID: posStart:%d posEnd:%d\n", posStart, posEnd ));

	if(pBL1 != pBL2)
	{
		//
		// Fixme put message boxes here
		//
		UT_DEBUGMSG(("fv_View::cmdInsertXMLID: range contains different blocks! xmlid \"%s\"\n", xmlid.c_str()));
		_restorePieceTableState();
		return UT_ERROR;
	}
	if(isTOCSelected())
	{
		//
		// Fixme put message boxes here
		//
		UT_DEBUGMSG(("fv_View::cmdInsertXMLID: can't insert xmlid in TOC! xmlid \"%s\"\n", xmlid.c_str()));
		_restorePieceTableState();
		return UT_ERROR;
	}

	PD_DocumentRDFHandle rdf = m_pDoc->getDocumentRDF();
    std::set< std::string > allIDs;
	rdf->getAllIDs( allIDs );
	if( allIDs.count( xmlid ))
	{
		UT_DEBUGMSG(("fv_View::cmdInsertXMLID: xmlid already in use... %s\n", xmlid.c_str()));
		XAP_Frame * pFrame = static_cast<XAP_Frame *>(getParentData());
		XAP_Dialog_MessageBox::tAnswer ans = XAP_Dialog_MessageBox::a_NO;

		if(pFrame)
			ans = pFrame->showMessageBox(AP_STRING_ID_MSG_XMLIDAlreadyExists,
										 XAP_Dialog_MessageBox::b_YN, XAP_Dialog_MessageBox::a_NO);

		if(ans == XAP_Dialog_MessageBox::a_YES)
		{
			// already exists -- remove it and then reinsert
			UT_DEBUGMSG(("fv_View::cmdInsertXMLID: xmlid:%s exists - removing\n", xmlid.c_str()));
			_deleteXMLID( xmlid, false, posStart, posEnd );
		}
		else
		{
			xxx_UT_DEBUGMSG(("User canceled xmlid replacement\n"));
			return UT_OK;
		}
	}

	PP_PropertyVector pa = {
		PT_XMLID, xmlid,
		// sanity check
		"this-is-an-rdf-anchor", "yes"
	};

	UT_DEBUGMSG(("fv_View::cmdInsertXMLID: inserting xmlid:%s at posStart:%d posEnd:%d\n",
				 xmlid.c_str(), posStart, posEnd ));

	bool bRet = m_pDoc->insertObject(posStart, PTO_RDFAnchor, pa, PP_NOPROPS);
	if(bRet) {
		pa.push_back(PT_RDF_END);
		pa.push_back("yes");
		bRet = m_pDoc->insertObject(posEnd, PTO_RDFAnchor, pa, PP_NOPROPS);
	}

	if(bRet) {
		ret = UT_OK;
	} else {
		ret = UT_ERROR;
	}

	// Signal piceTable is stable again
	_restorePieceTableState();
	_generalUpdate();

	return ret;
}

UT_Error
FV_View::cmdDeleteXMLID( const std::string& xmlid )
{
	return _deleteXMLID( xmlid, true);
}


/*****************************************************************/


UT_Error FV_View::cmdInsertTOC(void)
{
	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();
	m_pDoc->beginUserAtomicGlob();
	bool bRet = false; // was not initialised; since ret value is
					   // UT_Error, false should correspond to OK. Tomas


	if (!isSelectionEmpty())
	{
		_deleteSelection();
		_generalUpdate();
		fl_BlockLayout * pBL = _findBlockAtPosition(getPoint());
		if(pBL != nullptr)
		{
			fl_ContainerLayout * pCL = pBL->myContainingLayout();
			if(pCL->getContainerType() != FL_CONTAINER_DOCSECTION)
			{
				m_pDoc->endUserAtomicGlob();

				// Signal piceTable is stable again
				_restorePieceTableState();
				_generalUpdate();
				notifyListeners(AV_CHG_MOTION | AV_CHG_ALL);
				return bRet;
			}
		}
		else
		{
			m_pDoc->endUserAtomicGlob();
			
			// Signal piceTable is stable again
			_restorePieceTableState();
			_generalUpdate();
			notifyListeners(AV_CHG_MOTION | AV_CHG_ALL);
			return bRet;
		}
	}
//
// Check if there is a hyperlink here
//
	if(getHyperLinkRun(getPoint()) != nullptr)
	{
		return false;
	}
	if(!isPointLegal())
	{
	  _charMotion(true,1);
	}
	PT_DocPosition posEnd = 0;
	getEditableBounds(true, posEnd);
	if(getPoint() >= posEnd && !isPointLegal())
	{
	  _charMotion(false,1);
	}
//
// Close off the current block
//
	insertParagraphBreak();
//
// insert just before this block to make the TOC gets inserted just BEFORE
// the Block we just created.
//
// we want this sort of structure in the PT
//
// <Strux Block><Frag><frag><frag><TOC></TOC><Strux Block>|
//                                                        |
// Point is here after insert TOC-------------------------|
//
	fl_BlockLayout * pBL = getCurrentBlock();
	PT_DocPosition pos = pBL->getPosition(true);
	if((pBL->getNext() == nullptr) || (pBL->getPrev() == nullptr))
	{
		insertParagraphBreak();
		pBL = getCurrentBlock();
		pos = pBL->getPosition(true);
	}
	if(pBL != nullptr)
	{
		fl_ContainerLayout * pCL = pBL->myContainingLayout();
		if(pCL->getContainerType() != FL_CONTAINER_DOCSECTION)
		{
				// Signal piceTable is stable again
			_restorePieceTableState();
			_generalUpdate();
			m_pDoc->endUserAtomicGlob();

			notifyListeners(AV_CHG_MOTION | AV_CHG_ALL);
			return bRet;
		}
	}
	else
	{
		
		// Signal piceTable is stable again
		_restorePieceTableState();
		_generalUpdate();
		m_pDoc->endUserAtomicGlob();
		notifyListeners(AV_CHG_MOTION | AV_CHG_ALL);
		return bRet;
	}
	m_pDoc->insertStrux(pos,PTX_SectionTOC);
	pos++;
	m_pDoc->insertStrux(pos,PTX_EndTOC);
	setPoint(pos+1);
	insertParaBreakIfNeededAtPos(getPoint());
	//
	// Now move the point forward until we're in a legal position
	//
	_makePointLegal();
	// Signal piceTable is stable again
	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();
	notifyListeners(AV_CHG_MOTION | AV_CHG_ALL);

	return bRet;

}

//
// Word-style TOC presets used by the References ribbon gallery. Each preset
// sets the props of the "Contents 1-4" and "Contents Header" styles plus the
// strux props of the newly inserted TOC. The full char-format property set
// is written for every style so switching presets leaves no residue.
// Word's Calibri maps to the bundled metric-compatible Carlito.
//
struct FV_TOCStylePreset
{
	const char * szId;
	const char * szStruxProps;
	const char * szContentsProps[4];
	const char * szHeaderProps;
};

static const FV_TOCStylePreset s_TOCPresets[] =
{
	{ "classic",
	  "toc-indent1:0in; toc-indent2:0.25in; toc-indent3:0.5in; toc-indent4:0.75in; "
	  "toc-tab-leader1:dot; toc-tab-leader2:dot; toc-tab-leader3:dot; toc-tab-leader4:dot; "
	  "toc-heading:Contents",
	  { "font-family:Carlito; font-size:14pt; font-weight:bold; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000",
	    "font-family:Carlito; font-size:12pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000",
	    "font-family:Carlito; font-size:11pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000",
	    "font-family:Carlito; font-size:11pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000" },
	  "font-family:Carlito; font-size:16pt; font-weight:bold; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; text-align:center; margin-bottom:24pt; color:000000" },

	{ "contemporary",
	  "toc-indent1:0in; toc-indent2:0.25in; toc-indent3:0.5in; toc-indent4:0.75in; "
	  "toc-tab-leader1:underline; toc-tab-leader2:underline; toc-tab-leader3:underline; toc-tab-leader4:underline; "
	  "toc-heading:Table of Contents",
	  { "font-family:Carlito; font-size:14pt; font-weight:bold; font-style:normal; text-transform:uppercase; font-variant:normal; text-decoration:none; color:000000",
	    "font-family:Carlito; font-size:12pt; font-weight:normal; font-style:normal; text-transform:uppercase; font-variant:normal; text-decoration:none; color:000000",
	    "font-family:Carlito; font-size:11pt; font-weight:normal; font-style:normal; text-transform:uppercase; font-variant:normal; text-decoration:none; color:000000",
	    "font-family:Carlito; font-size:11pt; font-weight:normal; font-style:normal; text-transform:uppercase; font-variant:normal; text-decoration:none; color:000000" },
	  "font-family:Carlito; font-size:16pt; font-weight:bold; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; text-align:left; margin-bottom:24pt; color:000000" },

	{ "formal",
	  "toc-indent1:0in; toc-indent2:0.25in; toc-indent3:0.5in; toc-indent4:0.75in; "
	  "toc-tab-leader1:dot; toc-tab-leader2:dot; toc-tab-leader3:dot; toc-tab-leader4:dot; "
	  "toc-heading:Table of Contents",
	  { "font-family:Carlito; font-size:14pt; font-weight:bold; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000",
	    "font-family:Carlito; font-size:12pt; font-weight:bold; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000",
	    "font-family:Carlito; font-size:11pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000",
	    "font-family:Carlito; font-size:11pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000" },
	  "font-family:Carlito; font-size:18pt; font-weight:bold; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; text-align:center; margin-bottom:24pt; color:000000" },

	{ "modern",
	  "toc-indent1:0in; toc-indent2:0.25in; toc-indent3:0.5in; toc-indent4:0.75in; "
	  "toc-tab-leader1:none; toc-tab-leader2:none; toc-tab-leader3:none; toc-tab-leader4:none; "
	  "toc-heading:Table of Contents",
	  { "font-family:Carlito; font-size:12pt; font-weight:bold; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000",
	    "font-family:Carlito; font-size:11pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000",
	    "font-family:Carlito; font-size:10pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000",
	    "font-family:Carlito; font-size:10pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000" },
	  "font-family:Carlito; font-size:16pt; font-weight:bold; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; text-align:left; margin-bottom:12pt; color:000000" },

	{ "simple",
	  "toc-indent1:0in; toc-indent2:0.25in; toc-indent3:0.5in; toc-indent4:0.75in; "
	  "toc-tab-leader1:dot; toc-tab-leader2:dot; toc-tab-leader3:dot; toc-tab-leader4:dot; "
	  "toc-heading:Table of Contents",
	  { "font-family:Carlito; font-size:11pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000",
	    "font-family:Carlito; font-size:10pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000",
	    "font-family:Carlito; font-size:10pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000",
	    "font-family:Carlito; font-size:10pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; color:000000" },
	  "font-family:Carlito; font-size:14pt; font-weight:bold; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; text-align:left; margin-bottom:12pt; color:000000" },
};

static const FV_TOCStylePreset * _findTOCPreset(const char * szId)
{
	for(UT_uint32 i = 0; i < G_N_ELEMENTS(s_TOCPresets); i++)
	{
		if(0 == strcmp(s_TOCPresets[i].szId, szId))
		{
			return &s_TOCPresets[i];
		}
	}
	return nullptr;
}

void FV_View::_applyTOCStyleProps(const char * szStyle, const char * szProps)
{
	PP_PropertyVector atts = {
		"props", szProps
	};
	m_pDoc->addStyleAttributes(szStyle, atts);
	m_pDoc->updateDocForStyleChange(szStyle, true);
}

/*!
 * Insert a TOC at the point using a Word-style gallery preset
 * ("classic", "contemporary", "formal", "modern", "simple").
 * Restyles the Contents 1-4 / Contents Header styles to the preset's
 * look, inserts the TOC strux and writes the preset's strux props
 * (indents, tab leaders) onto it.
 */
/*!
 * Insert a TOC at the point, write the given strux props onto it and
 * clean up the empty paragraph cmdInsertTOC splits off in front of
 * it. Shared by the styled TOC gallery presets and the table of
 * figures.
 */
UT_Error FV_View::_insertTOCWithProps(const char * szProps)
{
	m_pDoc->beginUserAtomicGlob();
	_saveAndNotifyPieceTableChange();

	UT_Error e = cmdInsertTOC();

	// Find the TOC we just inserted at piece-table level (layout
	// positions do not map 1:1 to PT positions). The point is inside
	// or just after the new TOC, so the nearest preceding
	// PTX_SectionTOC is ours.
	const pf_Frag_Strux * sdhTOC = nullptr;
	PT_DocPosition posTOC = 0;
	if(m_pDoc->getStruxOfTypeFromPosition(getPoint(), PTX_SectionTOC, &sdhTOC) &&
	   sdhTOC)
	{
		// changeStruxFmt looks the strux up by a position contained
		// in it; a strux frag at position i contains position i+1.
		posTOC = m_pDoc->getStruxPosition(sdhTOC);
		setTOCProps(posTOC + 1, szProps);
	}

	// cmdInsertTOC splits the current block even when the point is at
	// its start, which leaves an empty paragraph before the TOC that
	// would itself be picked up as a TOC entry.
	if(posTOC > 1)
	{
		// Position posTOC is contained by the block frag immediately
		// before the TOC strux; if that block's own position is
		// posTOC-1 it holds no text. The block strux itself cannot
		// be deleted (empty blocks are zero-length frags), but its
		// heading style would make it show up as a bogus TOC entry,
		// so demote it to Normal.
		const pf_Frag_Strux * sdhPrev = nullptr;
		if(m_pDoc->getStruxOfTypeFromPosition(posTOC, PTX_Block, &sdhPrev) &&
		   sdhPrev &&
		   m_pDoc->getStruxPosition(sdhPrev) == posTOC - 1)
		{
			const PP_PropertyVector atts = { "style", "Normal" };
			m_pDoc->changeStruxFmt(PTC_AddStyle, posTOC, posTOC,
								   atts, PP_NOPROPS, PTX_Block);
		}
	}
	_makePointLegal();
	_makePointLegal();

	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();
	notifyListeners(AV_CHG_MOTION | AV_CHG_ALL);
	return e;
}

/*!
 * Insert a TOC at the point using a Word-style gallery preset
 * ("classic", "contemporary", "formal", "modern", "simple").
 * Restyles the Contents 1-4 / Contents Header styles to the preset's
 * look, inserts the TOC strux and writes the preset's strux props
 * (indents, tab leaders) onto it.
 */
UT_Error FV_View::cmdInsertTOCStyled(const char * szPreset)
{
	const FV_TOCStylePreset * pPreset = _findTOCPreset(szPreset);
	UT_return_val_if_fail(pPreset, UT_ERROR);

	// Insert first: the Contents 1-4 / Contents Header styles are
	// created by the TOC insertion itself, so preset style props can
	// only be applied afterwards.
	UT_Error e = _insertTOCWithProps(pPreset->szStruxProps);

	for(UT_uint32 i = 0; i < 4; i++)
	{
		UT_UTF8String sStyle = UT_UTF8String_sprintf("Contents %d", i + 1);
		_applyTOCStyleProps(sStyle.utf8_str(), pPreset->szContentsProps[i]);
	}
	_applyTOCStyleProps("Contents Header", pPreset->szHeaderProps);

	return e;
}

/*!
 * Insert a literal, non-updating placeholder TOC: a "Table of Contents"
 * heading plus one Contents N paragraph per level for the user to type
 * over, mirroring Word's "Manual Table" gallery entries. szPreset
 * (nullptr or one of "classic"/"contemporary"/"formal"/"modern"/
 * "simple") picks which built-in look the Contents styles get.
 */
UT_Error FV_View::cmdInsertTOCManual(const char * szPreset)
{
	m_pDoc->beginUserAtomicGlob();
	_saveAndNotifyPieceTableChange();

	const FV_TOCStylePreset * pPreset =
		szPreset ? _findTOCPreset(szPreset) : nullptr;

	// A manual TOC uses the same built-in style names as an automatic
	// one (Contents Header / Contents 1-4 == Word's TOC Heading /
	// TOC 1-4). With no preset, apply Word's Manual Table formatting:
	// 16pt bold centred heading, 11/10/10/10pt normal entries indented
	// 0/0.25/0.5/0.75in.
	static const char * s_manualContents[4] =
	{
		"font-family:Carlito; font-size:11pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; margin-left:0in; text-indent:0in; color:000000",
		"font-family:Carlito; font-size:10pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; margin-left:0.25in; text-indent:0in; color:000000",
		"font-family:Carlito; font-size:10pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; margin-left:0.5in; text-indent:0in; color:000000",
		"font-family:Carlito; font-size:10pt; font-weight:normal; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; margin-left:0.75in; text-indent:0in; color:000000"
	};
	static const char * s_manualHeader =
		"font-family:Carlito; font-size:16pt; font-weight:bold; font-style:normal; text-transform:none; font-variant:normal; text-decoration:none; text-align:center; margin-bottom:24pt; color:000000";

	for(UT_uint32 i = 0; i < 4; i++)
	{
		UT_UTF8String sStyle = UT_UTF8String_sprintf("Contents %d", i + 1);
		_applyTOCStyleProps(sStyle.utf8_str(),
							pPreset ? pPreset->szContentsProps[i]
									: s_manualContents[i]);
	}
	_applyTOCStyleProps("Contents Header",
						pPreset ? pPreset->szHeaderProps : s_manualHeader);

	// tabstop leader follows the preset's toc-tab-leader prop
	// (FL_LEADER_* index appended to the right-aligned tab position)
	char szLeader = '1'; // dot
	if(pPreset)
	{
		const char * p = strstr(pPreset->szStruxProps,
								"toc-tab-leader1:");
		if(p)
		{
			p += strlen("toc-tab-leader1:");
			if(0 == strncmp(p, "none", 4))			szLeader = '0';
			else if(0 == strncmp(p, "hyphen", 6))	szLeader = '2';
			else if(0 == strncmp(p, "underline", 9)) szLeader = '3';
		}
	}
	UT_UTF8String sTabs = UT_UTF8String_sprintf("6.0in/R%c", szLeader);

	insertParagraphBreak();
	setStyle("Contents Header", true);
	cmdCharInsert("Table of Contents", false);
	for(UT_uint32 i = 0; i < 4; i++)
	{
		insertParagraphBreak();
		UT_UTF8String sStyle = UT_UTF8String_sprintf("Contents %d", i + 1);
		setStyle(sStyle.utf8_str(), true);
		PP_PropertyVector props = { "tabstops", sTabs.utf8_str() };
		setBlockFormat(props);
		UT_UTF8String sText = UT_UTF8String_sprintf("Type chapter title (level %d)\t%d", i + 1, i + 1);
		cmdCharInsert(std::string(sText.utf8_str()), false);
	}
	insertParagraphBreak();
	setStyle("Normal", true);

	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();
	notifyListeners(AV_CHG_MOTION | AV_CHG_ALL);
	return UT_OK;
}

/*!
 * Returns the TOC layout containing the point, or nullptr.
 */
fl_TOCLayout * FV_View::findTOCAtPoint(void) const
{
	UT_return_val_if_fail(m_pLayout, nullptr);
	UT_sint32 iNum = m_pLayout->getNumTOCs();
	for(UT_sint32 i = 0; i < iNum; i++)
	{
		fl_TOCLayout * pTOC = m_pLayout->getNthTOC(i);
		if(!pTOC)
		{
			continue;
		}
		PT_DocPosition posStart = pTOC->getDocPosition();
		PT_DocPosition posEnd = posStart + pTOC->getLength();
		if(getPoint() > posStart && getPoint() <= posEnd)
		{
			return pTOC;
		}
	}
	return nullptr;
}

bool FV_View::hasTOC(void) const
{
	return findTOCAtPoint() != nullptr;
}

/*!
 * Regenerate the TOC at the point from the current document contents,
 * mirroring Word's "Update Table".
 */
bool FV_View::cmdUpdateTOC(void)
{
	fl_TOCLayout * pTOC = findTOCAtPoint();
	UT_return_val_if_fail(pTOC, false);
	return pTOC->fillTOC();
}

/*!
 * Remove the TOC at the point entirely, mirroring Word's
 * "Remove Table of Contents".
 */
bool FV_View::cmdRemoveTOC(void)
{
	fl_TOCLayout * pTOC = findTOCAtPoint();
	UT_return_val_if_fail(pTOC, false);

	const pf_Frag_Strux * sdh = pTOC->getStruxDocHandle();
	PT_DocPosition posStart = m_pDoc->getStruxPosition(sdh);
	PT_DocPosition posEnd = posStart + pTOC->getLength();

	m_pDoc->beginUserAtomicGlob();
	_saveAndNotifyPieceTableChange();

	PP_AttrProp * pAP = nullptr;
	UT_uint32 iRealDeleteCount = 0;
	bool bRet = m_pDoc->deleteSpan(posStart, posEnd, pAP, iRealDeleteCount, true);

	setPoint(posStart);
	insertParaBreakIfNeededAtPos(getPoint());
	_makePointLegal();

	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();
	notifyListeners(AV_CHG_MOTION | AV_CHG_ALL);
	return bRet;
}

/*!
 * The explicit toc-level property of the current block (-1 when unset,
 * 0 for "do not show in table of contents", 1-4 for TOC levels).
 */
UT_sint32 FV_View::getTocLevel(void) const
{
	fl_BlockLayout * pBL = getCurrentBlock();
	UT_return_val_if_fail(pBL, -1);
	const char * szLevel = pBL->getProperty("toc-level");
	if(szLevel && *szLevel)
	{
		return atoi(szLevel);
	}
	return -1;
}

/*!
 * Set the explicit toc-level property on the block(s) spanned by the
 * selection (or the current block), backing the ribbon's "Add Text"
 * dropdown. iLevel 0 hides the paragraph from the TOC.
 */
void FV_View::setTocLevel(UT_sint32 iLevel)
{
	char szBuf[8];
	snprintf(szBuf, sizeof(szBuf), "%d", iLevel);
	PP_PropertyVector props = {
		"toc-level", szBuf
	};
	setBlockFormat(props);
}

/*!
 * Jump the insertion point to the next or previous footnote/endnote
 * reference, mirroring Word's Next Footnote/Previous Footnote.
 */
bool FV_View::nextNote(bool bFootnote, bool bForward)
{
	PTStruxType pts = bFootnote ? PTX_SectionFootnote : PTX_SectionEndnote;
	const pf_Frag_Strux * sdh = nullptr;
	const pf_Frag_Strux * seed = nullptr;
	if(m_pDoc->getStruxOfTypeFromPosition(getPoint(), pts, &sdh) && sdh)
	{
		seed = sdh;
	}
	else if(m_pDoc->getStruxOfTypeFromPosition(getPoint(), PTX_Block, &sdh) && sdh)
	{
		seed = sdh;
	}
	UT_return_val_if_fail(seed, false);

	const pf_Frag_Strux * target = nullptr;
	if(bForward)
	{
		m_pDoc->getNextStruxOfType(seed, pts, &target);
	}
	else
	{
		m_pDoc->getPrevStruxOfType(seed, pts, &target);
	}
	UT_return_val_if_fail(target, false);

	setPoint(m_pDoc->getStruxPosition(target) + 1);
	ensureInsertionPointOnScreen();
	return true;
}

/*!
 * Jump the insertion point into the nearest footnote or endnote text,
 * mirroring Word's "Show Notes". Prefers the note containing the point,
 * then the next one in the document.
 */
void FV_View::cmdShowNotes(void)
{
	static const PTStruxType aTypes[] = { PTX_SectionFootnote, PTX_SectionEndnote };
	for(UT_uint32 t = 0; t < G_N_ELEMENTS(aTypes); t++)
	{
		const pf_Frag_Strux * sdh = nullptr;
		if(m_pDoc->getStruxOfTypeFromPosition(getPoint(), aTypes[t], &sdh) && sdh)
		{
			setPoint(m_pDoc->getStruxPosition(sdh) + 1);
			ensureInsertionPointOnScreen();
			return;
		}
	}
	for(UT_uint32 t = 0; t < G_N_ELEMENTS(aTypes); t++)
	{
		const pf_Frag_Strux * sdh = nullptr;
		const pf_Frag_Strux * target = nullptr;
		if(m_pDoc->getStruxOfTypeFromPosition(getPoint(), PTX_Block, &sdh) && sdh
		   && m_pDoc->getNextStruxOfType(sdh, aTypes[t], &target) && target)
		{
			setPoint(m_pDoc->getStruxPosition(target) + 1);
			ensureInsertionPointOnScreen();
			return;
		}
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * References tab support: captions, tables of figures, cross-
 * references, indexes, tables of authorities, citations and
 * bibliographies.
 *
 * Marked entries are stored as document bookmarks with reserved
 * prefixes so they survive round-trips through the .abw format:
 *
 *   _idx_<n>        index entries; the bookmarked text is the entry
 *   _toa_<cat>_<n>  table-of-authorities citations; the bookmarked
 *                   text is the citation, <cat> the category
 *   _bib_<n>        bibliography citations; the bookmarked text is the
 *                   inline citation and the source record lives in the
 *                   document metadata keys bib.<n>.<field>
 *   _genidx/_gentoa/_genbib
 *                   bookmarks wrapping a generated index, table of
 *                   authorities or bibliography section so that it can
 *                   be found, updated and removed again
 *
 * Generated sections write their page numbers as live page_ref fields
 * pointing back at the entry bookmarks, so they keep tracking the
 * document the same way Word fields do.
 */

#define FV_BM_IDX_PREFIX "_idx_"
#define FV_BM_TOA_PREFIX "_toa_"
#define FV_BM_BIB_PREFIX "_bib_"

struct FV_BookmarkSpan
{
	PT_DocPosition posStart;  // position of the start marker object
	PT_DocPosition posEnd;    // position of the end marker object
	PT_DocPosition posFirst;  // first position of the bookmarked text
	PT_DocPosition posLast;   // one past the last bookmarked position
};

/*!
 * Find the layout runs of both markers of the named bookmark and return
 * the marker positions plus the span between them.
 */
bool FV_View::_findBookmarkSpan(const char * szName, FV_BookmarkSpan & span) const
{
	fp_BookmarkRun * pStart = nullptr;
	fp_BookmarkRun * pEnd = nullptr;
	fl_SectionLayout * pSection = m_pLayout ? m_pLayout->getFirstSection() : nullptr;
	while(pSection && !pEnd)
	{
		fl_BlockLayout * pBlock = static_cast<fl_BlockLayout *>(pSection->getFirstLayout());
		while(pBlock && !pEnd)
		{
			for(fp_Run * pRun = pBlock->getFirstRun(); pRun; pRun = pRun->getNextRun())
			{
				if(pRun->getType() != FPRUN_BOOKMARK)
				{
					continue;
				}
				fp_BookmarkRun * pB = static_cast<fp_BookmarkRun *>(pRun);
				if(0 != strcmp(pB->getName(), szName))
				{
					continue;
				}
				if(pB->isStartOfBookmark())
				{
					pStart = pB;
				}
				else
				{
					pEnd = pB;
				}
			}
			pBlock = static_cast<fl_BlockLayout *>(pBlock->getNext());
		}
		pSection = static_cast<fl_SectionLayout *>(pSection->getNext());
	}
	if(!pStart || !pEnd)
	{
		return false;
	}
	span.posStart = pStart->getBlock()->getPosition(false) + pStart->getBlockOffset();
	span.posEnd = pEnd->getBlock()->getPosition(false) + pEnd->getBlockOffset();
	span.posFirst = pStart->getBookmarkedDocPosition(true);
	span.posLast = pEnd->getBookmarkedDocPosition(false);
	return true;
}

/*!
 * The text wrapped by the named bookmark, e.g. the marked index entry
 * or citation text. Returns false when the bookmark does not exist.
 */
bool FV_View::_getBookmarkText(const char * szName, UT_UTF8String & sText) const
{
	FV_BookmarkSpan span;
	if(!_findBookmarkSpan(szName, span) || span.posLast <= span.posFirst)
	{
		return false;
	}
	UT_UCS4Char * pText = getTextBetweenPos(span.posFirst, span.posLast);
	if(!pText)
	{
		return false;
	}
	sText = UT_UCS4String(pText).utf8_str();
	FREEP(pText);
	return true;
}

/*!
 * All bookmark names carrying the given prefix, in document order.
 */
void FV_View::_getBookmarksWithPrefix(const char * szPrefix,
									  std::vector<std::string> & names) const
{
	const UT_sint32 iCount = m_pDoc->getBookmarkCount();
	const size_t iPrefix = strlen(szPrefix);
	for(UT_sint32 i = 0; i < iCount; i++)
	{
		const std::string & sName = m_pDoc->getNthBookmark(i);
		if(0 == sName.compare(0, iPrefix, szPrefix))
		{
			names.push_back(sName);
		}
	}
}

/*!
 * Lowest positive suffix not used by any bookmark with the prefix.
 */
UT_uint32 FV_View::_nextBookmarkSuffix(const char * szPrefix) const
{
	std::vector<std::string> names;
	_getBookmarksWithPrefix(szPrefix, names);
	UT_uint32 iMax = 0;
	for(const std::string & s : names)
	{
		const UT_uint32 n = strtoul(s.c_str() + strlen(szPrefix), nullptr, 10);
		if(n > iMax)
		{
			iMax = n;
		}
	}
	return iMax + 1;
}

/*!
 * Create a paragraph style if it does not exist yet.
 */
void FV_View::_ensureRefStyle(const char * szName, const char * szProps) const
{
	PD_Style * pStyle = nullptr;
	if(m_pDoc->getStyle(szName, &pStyle) && pStyle)
	{
		return;
	}
	const PP_PropertyVector atts = {
		PT_NAME_ATTRIBUTE_NAME, szName,
		PT_TYPE_ATTRIBUTE_NAME, "P",
		"basedon", "Normal",
		"followedby", "Normal",
		"props", szProps
	};
	m_pDoc->appendStyle(atts);
	m_pDoc->updateDocForStyleChange(szName, true);
}

/*!
 * Insert a page_ref field at the point, mirroring Word's
 * { PAGEREF bookmark \h }.
 */
void FV_View::_insertPageRefField(const char * szBookmark)
{
	const PP_PropertyVector atts = {
		"type", "page_ref",
		"param", szBookmark
	};
	const PT_DocPosition pos = getPoint();
	if(m_pDoc->insertObject(pos, PTO_Field, atts, PP_NOPROPS))
	{
		setPoint(pos + 1);
	}
}

/*!
 * Delete the generated section wrapped by the named marker bookmark,
 * markers included. Returns the position where the section used to
 * start so it can be regenerated in place, or 0 when absent.
 */
PT_DocPosition FV_View::_deleteGeneratedSection(const char * szMarker)
{
	FV_BookmarkSpan span;
	if(!_findBookmarkSpan(szMarker, span))
	{
		return 0;
	}
	PP_AttrProp * pAP = nullptr;
	UT_uint32 iRealDeleteCount = 0;
	if(!m_pDoc->deleteSpan(span.posStart, span.posEnd + 1, pAP,
						   iRealDeleteCount, true))
	{
		return 0;
	}
	return span.posStart;
}

/*!
 * Write a generated section (index, table of authorities,
 * bibliography) at the point wrapped in a marker bookmark so that it
 * can be updated or removed later. Each entry becomes a paragraph in
 * the given style holding "text<tab>page"; page numbers are live
 * page_ref fields onto the entry bookmarks.
 */
void FV_View::_writeGeneratedSection(const char * szMarker,
									 const char * szHeading,
									 const char * szHeadingStyle,
									 const std::vector<FV_RefEntry> & entries)
{
	_ensureRefStyle(szHeadingStyle,
		"font-family:Carlito; font-size:16pt; font-weight:bold; "
		"margin-bottom:24pt; text-align:left");
	_ensureRefStyle("Ref Section 1",
		"font-family:Carlito; font-size:11pt; margin-bottom:0pt");
	_ensureRefStyle("Ref Section 2",
		"font-family:Carlito; font-size:11pt; margin-bottom:0pt; "
		"margin-left:0.25in");
	_ensureRefStyle("Ref Category",
		"font-family:Carlito; font-size:11pt; font-weight:bold; "
		"margin-top:12pt");

	// A live selection would be deleted by the paragraph breaks
	// below, so collapse it first.
	if(!isSelectionEmpty())
	{
		m_Selection.clearSelection();
	}
	// Write the section content first and record the position where
	// the first paragraph starts, then wrap it in the marker
	// bookmarks. Inserting the markers before the paragraph breaks
	// leaves them inside the wrong block.
	insertParagraphBreak();
	const PT_DocPosition posMark = getPoint();
	setStyle(szHeadingStyle);
	cmdCharInsert(std::string(szHeading), false);

	for(const FV_RefEntry & e : entries)
	{
		insertParagraphBreak();
		setStyle(e.iLevel > 1 ? "Ref Section 2"
				 : (e.bCategory ? "Ref Category" : "Ref Section 1"));
		if(e.sText.size())
		{
			cmdCharInsert(std::string(e.sText.utf8_str()), false);
		}
		for(UT_uint32 i = 0; i < e.bookmarks.size(); i++)
		{
			cmdCharInsert(i ? ", " : "\t", false);
			_insertPageRefField(e.bookmarks[i].c_str());
		}
	}

	// Close with a Normal paragraph so typing continues normally
	// after the section.
	insertParagraphBreak();
	setStyle("Normal");

	PP_PropertyVector atts = {
		"name", szMarker,
		"type", "end"
	};
	const PT_DocPosition posEnd = getPoint();
	m_pDoc->insertObject(posEnd, PTO_Bookmark, atts, PP_NOPROPS);
	atts[3] = "start";
	m_pDoc->insertObject(posMark, PTO_Bookmark, atts, PP_NOPROPS);
	// Leave the insertion point after the section so later inserts
	// do not split its paragraphs or the marker objects.
	setPoint(posEnd + 1);
}

/*!
 * Regenerate the section wrapped by the named marker at its current
 * position (or at the point when it does not exist yet).
 */
void FV_View::_regenerateSection(const char * szMarker,
								 const char * szHeading,
								 const char * szHeadingStyle,
								 const std::vector<FV_RefEntry> & entries)
{
	m_pDoc->beginUserAtomicGlob();
	_saveAndNotifyPieceTableChange();

	PT_DocPosition pos = _deleteGeneratedSection(szMarker);
	if(pos == 0)
	{
		pos = getPoint();
	}
	setPoint(pos);
	_writeGeneratedSection(szMarker, szHeading, szHeadingStyle, entries);
	insertParaBreakIfNeededAtPos(getPoint());
	_makePointLegal();

	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();
	notifyListeners(AV_CHG_MOTION | AV_CHG_ALL);
}

/*!
 * Remove the section wrapped by the named marker, mirroring Word's
 * "Remove Table of Contents" for the generated references sections.
 */
bool FV_View::cmdRemoveRefSection(const char * szMarker)
{
	m_pDoc->beginUserAtomicGlob();
	_saveAndNotifyPieceTableChange();
	PT_DocPosition pos = _deleteGeneratedSection(szMarker);
	if(pos > 0)
	{
		setPoint(pos);
		insertParaBreakIfNeededAtPos(getPoint());
		_makePointLegal();
	}
	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();
	notifyListeners(AV_CHG_MOTION | AV_CHG_ALL);
	return pos > 0;
}

bool FV_View::hasRefSection(const char * szMarker) const
{
	FV_BookmarkSpan span;
	return _findBookmarkSpan(szMarker, span);
}

/*!
 * Insert a numbered caption ("Figure 3", "Table 1", ...) in a
 * per-label "<Label> Caption" style above or below the current block,
 * mirroring Word's Insert Caption dialog.
 */
UT_Error FV_View::cmdInsertCaption(const char * szLabel, bool bAbove)
{
	UT_return_val_if_fail(szLabel && *szLabel, UT_ERROR);
	fl_BlockLayout * pBL = getCurrentBlock();
	UT_return_val_if_fail(pBL, UT_ERROR);

	const UT_UTF8String sStyle =
		UT_UTF8String_sprintf("%s Caption", szLabel);
	_ensureRefStyle(sStyle.utf8_str(),
		"font-family:Carlito; font-size:11pt; margin-top:6pt; "
		"margin-bottom:6pt; text-align:centered");

	// Next number for this label: highest existing "<Label> N" + 1.
	UT_uint32 iMax = 0;
	fl_SectionLayout * pSection = m_pLayout->getFirstSection();
	while(pSection)
	{
		fl_BlockLayout * pBlock = static_cast<fl_BlockLayout *>(pSection->getFirstLayout());
		while(pBlock)
		{
			UT_UTF8String sBlockStyle;
			pBlock->getStyle(sBlockStyle);
			if(0 == strcmp(sBlockStyle.utf8_str(), sStyle.utf8_str()))
			{
				UT_UCS4Char * pText = getTextBetweenPos(
					pBlock->getPosition(false) + 1,
					pBlock->getPosition(false) + pBlock->getLength() - 1);
				if(pText)
				{
					const UT_UTF8String s(UT_UCS4String(pText).utf8_str());
					FREEP(pText);
					const char * p = s.utf8_str() + strlen(szLabel);
					while(*p == ' ')
					{
						p++;
					}
					const UT_uint32 n = strtoul(p, nullptr, 10);
					if(n > iMax)
					{
						iMax = n;
					}
				}
			}
			pBlock = static_cast<fl_BlockLayout *>(pBlock->getNext());
		}
		pSection = static_cast<fl_SectionLayout *>(pSection->getNext());
	}

	const UT_UTF8String sText =
		UT_UTF8String_sprintf("%s %u", szLabel, iMax + 1);
	const PT_DocPosition posBlock = pBL->getPosition(false);

	m_pDoc->beginUserAtomicGlob();
	_saveAndNotifyPieceTableChange();

	if(bAbove)
	{
		// Split at the block start: an empty paragraph is left above
		// holding the caption, the old text stays below.
		setPoint(posBlock + 1);
		insertParagraphBreak();
		setPoint(posBlock + 1);
		setStyle(sStyle.utf8_str());
		cmdCharInsert(std::string(sText.utf8_str()), false);
	}
	else
	{
		// Split at the block end: a new paragraph after the block
		// holds the caption.
		setPoint(posBlock + pBL->getLength() - 1);
		insertParagraphBreak();
		setStyle(sStyle.utf8_str());
		cmdCharInsert(std::string(sText.utf8_str()), false);
	}

	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();
	notifyListeners(AV_CHG_MOTION | AV_CHG_ALL);
	return UT_OK;
}

/*!
 * Insert a table of figures listing every caption paragraph of the
 * given label, implemented as a TOC whose single source style is
 * "<Label> Caption".
 */
UT_Error FV_View::cmdInsertTableOfFigures(const char * szLabel)
{
	UT_return_val_if_fail(szLabel && *szLabel, UT_ERROR);
	const UT_UTF8String sStyle =
		UT_UTF8String_sprintf("%s Caption", szLabel);
	const UT_UTF8String sProps = UT_UTF8String_sprintf(
		"toc-source-style1:%s; toc-source-style2:none; "
		"toc-source-style3:none; toc-source-style4:none; "
		"toc-dest-style1:Contents 1; toc-dest-style2:Contents 2; "
		"toc-dest-style3:Contents 3; toc-dest-style4:Contents 4; "
		"toc-has-label1:0; toc-has-label2:0; toc-has-label3:0; "
		"toc-has-label4:0; "
		"toc-heading:Table of Figures; "
		"toc-heading-style:Contents Header; toc-has-heading:1; "
		"toc-indent1:0in; toc-indent2:0.25in; toc-indent3:0.5in; "
		"toc-indent4:0.75in; "
		"toc-tab-leader1:dot; toc-tab-leader2:dot; "
		"toc-tab-leader3:dot; toc-tab-leader4:dot",
		sStyle.utf8_str());
	return _insertTOCWithProps(sProps.utf8_str());
}

/*!
 * Insert a cross-reference to a bookmark: either its page number as a
 * live page_ref field, or the bookmarked text as a clickable
 * #bookmark hyperlink, mirroring Word's Cross-reference dialog.
 */
UT_Error FV_View::cmdInsertCrossReference(const char * szBookmark,
										  bool bPageNumber)
{
	UT_return_val_if_fail(szBookmark && *szBookmark, UT_ERROR);
	m_pDoc->beginUserAtomicGlob();
	_saveAndNotifyPieceTableChange();

	bool bRet = false;
	if(bPageNumber)
	{
		_insertPageRefField(szBookmark);
		bRet = true;
	}
	else
	{
		UT_UTF8String sText;
		if(!_getBookmarkText(szBookmark, sText) || !sText.size())
		{
			sText = szBookmark;
		}
		const UT_UCS4String sUCS4(sText.utf8_str());
		const PT_DocPosition pos = getPoint();
		if(m_pDoc->insertSpan(pos, sUCS4.ucs4_str(), sUCS4.length()))
		{
			setPoint(pos);
			m_Selection.setMode(FV_SelectionMode_Single);
			m_Selection.setSelectionAnchor(pos + sUCS4.length());
			bRet = (UT_OK == cmdInsertHyperlink(szBookmark));
			m_Selection.clearSelection();
		}
	}

	_restorePieceTableState();
	_generalUpdate();
	m_pDoc->endUserAtomicGlob();
	notifyListeners(AV_CHG_MOTION | AV_CHG_ALL);
	return bRet ? UT_OK : UT_ERROR;
}

/*!
 * All bookmark names usable as cross-reference targets, i.e. every
 * bookmark not used internally by the references machinery.
 */
void FV_View::getXRefBookmarks(std::vector<std::string> & names) const
{
	const UT_sint32 iCount = m_pDoc->getBookmarkCount();
	for(UT_sint32 i = 0; i < iCount; i++)
	{
		const std::string & s = m_pDoc->getNthBookmark(i);
		if(s.empty() || s[0] == '_')
		{
			continue;
		}
		names.push_back(s);
	}
}

/*!
 * Mark the selection (or the given literal text when nothing is
 * selected) as an index entry, mirroring Word's Mark Entry.
 */
UT_Error FV_View::cmdMarkIndexEntry(const char * szEntry)
{
	char name[BOOKMARK_NAME_SIZE + 1];
	snprintf(name, sizeof(name), FV_BM_IDX_PREFIX "%u",
			 _nextBookmarkSuffix(FV_BM_IDX_PREFIX));
	return _markTextAsBookmark(szEntry, name);
}

/*!
 * Mark the selection (or the given literal text) as a table of
 * authorities citation in the given category, mirroring Word's
 * Mark Citation.
 */
UT_Error FV_View::cmdMarkCitation(const char * szCategory,
								  const char * szCitation)
{
	UT_return_val_if_fail(szCategory && *szCategory, UT_ERROR);
	UT_UTF8String sPrefix =
		UT_UTF8String_sprintf(FV_BM_TOA_PREFIX "%s_", szCategory);
	UT_UTF8String sName =
		UT_UTF8String_sprintf("%s%u", sPrefix.utf8_str(),
							  _nextBookmarkSuffix(sPrefix.utf8_str()));
	return _markTextAsBookmark(szCitation, sName.utf8_str());
}

/*!
 * Bookmark the selection under the given name, or insert the literal
 * text first and bookmark it when the selection is empty.
 */
UT_Error FV_View::_markTextAsBookmark(const char * szText,
									  const char * szBookmark)
{
	if(isSelectionEmpty())
	{
		UT_return_val_if_fail(szText && *szText, UT_ERROR);
		const UT_UCS4String sUCS4(szText);
		const PT_DocPosition pos = getPoint();
		if(!m_pDoc->insertSpan(pos, sUCS4.ucs4_str(), sUCS4.length()))
		{
			return UT_ERROR;
		}
		setPoint(pos);
		m_Selection.setMode(FV_SelectionMode_Single);
		m_Selection.setSelectionAnchor(pos + sUCS4.length());
	}
	UT_Error err = cmdInsertBookmark(szBookmark);
	// Drop the selection so a subsequent generated-section insert
	// cannot delete the marked text, and move the point after the
	// bookmark so paragraph breaks do not split its markers.
	m_Selection.clearSelection();
	FV_BookmarkSpan span;
	if(_findBookmarkSpan(szBookmark, span))
	{
		setPoint(span.posEnd + 1);
	}
	return err;
}

/*!
 * Collect all marked entries carrying the given bookmark prefix into
 * sorted, grouped reference entries ready for section generation.
 * Entry text may contain a ':' separating main entry and subentry.
 */
void FV_View::_collectMarkedEntries(const char * szPrefix,
									std::vector<FV_RefEntry> & entries) const
{
	std::vector<std::string> names;
	_getBookmarksWithPrefix(szPrefix, names);

	// Group identical entry text so repeated marks merge their page
	// numbers like Word does.
	std::vector<UT_UTF8String> keys;
	std::vector<std::vector<std::string>> grouped;
	for(const std::string & s : names)
	{
		UT_UTF8String sText;
		if(!_getBookmarkText(s.c_str(), sText) || !sText.size())
		{
			continue;
		}
		bool bFound = false;
		for(UT_uint32 i = 0; i < keys.size(); i++)
		{
			if(0 == strcmp(keys[i].utf8_str(), sText.utf8_str()))
			{
				grouped[i].push_back(s);
				bFound = true;
				break;
			}
		}
		if(!bFound)
		{
			keys.push_back(sText);
			grouped.push_back({s});
		}
	}

	// Sort by entry text, case-insensitive.
	std::vector<UT_uint32> order(keys.size());
	for(UT_uint32 i = 0; i < keys.size(); i++)
	{
		order[i] = i;
	}
	std::sort(order.begin(), order.end(),
			  [&](UT_uint32 a, UT_uint32 b)
			  {
				  gchar * ca = g_utf8_casefold(keys[a].utf8_str(), -1);
				  gchar * cb = g_utf8_casefold(keys[b].utf8_str(), -1);
				  const int r = g_utf8_collate(ca, cb);
				  g_free(ca);
				  g_free(cb);
				  return r < 0;
			  });

	for(UT_uint32 i : order)
	{
		const std::string sKey(keys[i].utf8_str());
		const size_t iColon = sKey.find(':');
		if(iColon != std::string::npos && iColon + 1 < sKey.size())
		{
			// "Main:Sub" becomes a level-1 header plus a level-2 entry.
			FV_RefEntry eMain;
			eMain.sText = sKey.substr(0, iColon).c_str();
			eMain.iLevel = 1;
			eMain.bCategory = true;
			entries.push_back(eMain);
			FV_RefEntry eSub;
			eSub.sText = sKey.substr(iColon + 1).c_str();
			eSub.iLevel = 2;
			eSub.bookmarks = grouped[i];
			entries.push_back(eSub);
		}
		else
		{
			FV_RefEntry e;
			e.sText = keys[i];
			e.iLevel = 1;
			e.bookmarks = grouped[i];
			entries.push_back(e);
		}
	}
}

/*!
 * Insert (or regenerate) the alphabetical index of all _idx_ marked
 * entries, mirroring Word's Insert Index / Update Index.
 */
UT_Error FV_View::cmdInsertIndex(void)
{
	std::vector<FV_RefEntry> entries;
	_collectMarkedEntries(FV_BM_IDX_PREFIX, entries);
	_regenerateSection("_genidx", "Index", "Index Heading", entries);
	return UT_OK;
}

/*!
 * Insert (or regenerate) the table of authorities of all _toa_ marked
 * citations, grouped by category like Word's Insert Table of
 * Authorities.
 */
UT_Error FV_View::cmdInsertTOA(void)
{
	static const char * s_Cats[] =
		{ "cases", "statutes", "regulations", "other" };
	static const char * s_CatNames[] =
		{ "Cases", "Statutes", "Regulations", "Other Authorities" };

	std::vector<FV_RefEntry> entries;
	for(UT_uint32 c = 0; c < G_N_ELEMENTS(s_Cats); c++)
	{
		const UT_UTF8String sPrefix =
			UT_UTF8String_sprintf(FV_BM_TOA_PREFIX "%s_", s_Cats[c]);
		std::vector<FV_RefEntry> catEntries;
		_collectMarkedEntries(sPrefix.utf8_str(), catEntries);
		if(catEntries.empty())
		{
			continue;
		}
		FV_RefEntry eCat;
		eCat.sText = s_CatNames[c];
		eCat.iLevel = 1;
		eCat.bCategory = true;
		entries.push_back(eCat);
		entries.insert(entries.end(), catEntries.begin(), catEntries.end());
	}
	_regenerateSection("_gentoa", "Table of Authorities",
					   "TOA Heading", entries);
	return UT_OK;
}

/*!
 * Insert an inline citation "(Author, Year)" bookmarked as _bib_<n>
 * and store the full source record in the document metadata as
 * bib.<n>.author/.year/.title/.publisher/.type, mirroring Word's
 * Insert Citation. szFields is pipe-separated:
 * "author|year|title|publisher|type".
 */
UT_Error FV_View::cmdInsertCitation(const char * szFields)
{
	UT_return_val_if_fail(szFields && *szFields, UT_ERROR);
	char fields[5][256] = {{0}};
	{
		const char * p = szFields;
		for(UT_uint32 i = 0; i < 5 && p; i++)
		{
			const char * q = strchr(p, '|');
			const size_t len = q ? static_cast<size_t>(q - p) : strlen(p);
			strncpy(fields[i], p, len > 255 ? 255 : len);
			p = q ? q + 1 : nullptr;
		}
	}
	UT_return_val_if_fail(*fields[0] || *fields[2], UT_ERROR);

	const UT_uint32 n = _nextBookmarkSuffix(FV_BM_BIB_PREFIX);
	const std::string sKey = UT_UTF8String_sprintf("bib.%u", n).utf8_str();
	static const char * s_Names[5] =
		{ "author", "year", "title", "publisher", "type" };
	for(UT_uint32 i = 0; i < 5; i++)
	{
		if(*fields[i])
		{
			m_pDoc->setMetaDataProp(sKey + "." + s_Names[i], fields[i]);
		}
	}

	const UT_UTF8String sInline = *fields[0]
		? UT_UTF8String_sprintf("(%s, %s)", fields[0], fields[1])
		: UT_UTF8String_sprintf("(%s)", fields[2]);
	const UT_UTF8String sBookmark =
		UT_UTF8String_sprintf(FV_BM_BIB_PREFIX "%u", n);
	return _markTextAsBookmark(sInline.utf8_str(), sBookmark.utf8_str());
}

/*!
 * All stored bibliography source records for the Manage Sources
 * popover and the bibliography generator.
 */
void FV_View::getBibSources(std::vector<FV_BibSource> & sources) const
{
	const std::map<std::string,std::string> & meta = m_pDoc->getMetaData();
	for(const auto & kv : meta)
	{
		const std::string & key = kv.first;
		if(key.compare(0, 4, "bib.") != 0)
		{
			continue;
		}
		const size_t dot = key.find('.', 4);
		if(dot == std::string::npos)
		{
			continue;
		}
		const UT_uint32 n = strtoul(key.c_str() + 4, nullptr, 10);
		const std::string sField = key.substr(dot + 1);
		FV_BibSource * pSrc = nullptr;
		for(FV_BibSource & s : sources)
		{
			if(s.n == n)
			{
				pSrc = &s;
				break;
			}
		}
		if(!pSrc)
		{
			FV_BibSource s;
			s.n = n;
			sources.push_back(s);
			pSrc = &sources.back();
		}
		if(sField == "author") pSrc->author = kv.second;
		else if(sField == "year") pSrc->year = kv.second;
		else if(sField == "title") pSrc->title = kv.second;
		else if(sField == "publisher") pSrc->publisher = kv.second;
		else if(sField == "type") pSrc->type = kv.second;
	}
}

/*!
 * Delete a stored bibliography source and its inline _bib_<n>
 * bookmark, backing the Manage Sources list's delete action.
 */
void FV_View::cmdDeleteBibSource(UT_uint32 n)
{
	const std::string sKey = UT_UTF8String_sprintf("bib.%u", n).utf8_str();
	static const char * s_Names[5] =
		{ "author", "year", "title", "publisher", "type" };
	for(UT_uint32 i = 0; i < 5; i++)
	{
		m_pDoc->setMetaDataProp(sKey + "." + s_Names[i], "");
	}
	const UT_UTF8String sBookmark =
		UT_UTF8String_sprintf(FV_BM_BIB_PREFIX "%u", n);
	_deleteBookmark(sBookmark.utf8_str(), true);
	_generalUpdate();
}

/*!
 * Insert (or regenerate) the bibliography of all stored sources in
 * the requested style, mirroring Word's Bibliography gallery.
 * szStyle is "apa", "mla", "chicago" or "ieee".
 */
UT_Error FV_View::cmdInsertBibliography(const char * szStyle)
{
	UT_return_val_if_fail(szStyle && *szStyle, UT_ERROR);
	std::vector<FV_BibSource> sources;
	getBibSources(sources);
	UT_return_val_if_fail(!sources.empty(), UT_ERROR);

	std::sort(sources.begin(), sources.end(),
			  [](const FV_BibSource & a, const FV_BibSource & b)
			  {
				  if(a.author != b.author) return a.author < b.author;
				  return a.year < b.year;
			  });

	std::vector<FV_RefEntry> entries;
	for(const FV_BibSource & s : sources)
	{
		FV_RefEntry e;
		e.iLevel = 1;
		if(0 == strcmp(szStyle, "mla"))
		{
			// Author. Title. Publisher, Year.
			e.sText = UT_UTF8String_sprintf("%s. %s. %s, %s.",
				s.author.c_str(), s.title.c_str(),
				s.publisher.c_str(), s.year.c_str());
		}
		else if(0 == strcmp(szStyle, "chicago"))
		{
			// Author. Year. Title. Publisher.
			e.sText = UT_UTF8String_sprintf("%s. %s. %s. %s.",
				s.author.c_str(), s.year.c_str(),
				s.title.c_str(), s.publisher.c_str());
		}
		else if(0 == strcmp(szStyle, "ieee"))
		{
			// Author, "Title," Publisher, Year.
			e.sText = UT_UTF8String_sprintf("%s, \"%s,\" %s, %s.",
				s.author.c_str(), s.title.c_str(),
				s.publisher.c_str(), s.year.c_str());
		}
		else
		{
			// APA: Author (Year). Title. Publisher.
			e.sText = UT_UTF8String_sprintf("%s (%s). %s. %s.",
				s.author.c_str(), s.year.c_str(),
				s.title.c_str(), s.publisher.c_str());
		}
		entries.push_back(e);
	}
	_regenerateSection("_genbib", "Bibliography",
					   "Bibliography Heading", entries);
	return UT_OK;
}


/*****************************************************************/
UT_Error FV_View::cmdInsertField(const char* szName, const PP_PropertyVector & extra_attrs, const PP_PropertyVector & extra_props)
{
	bool bResult = true;

/*
  currently unused
  fl_BlockLayout* pBL = _findBlockAtPosition(getPoint());
*/

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();
	_insertField(szName, extra_attrs, extra_props);


	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	_generalUpdate();

	_fixInsertionPointCoords();
	if (!_ensureInsertionPointOnScreen())
	{
//
// Handle End of Paragraph case
//
		PT_DocPosition posEOD;
		getEditableBounds(true, posEOD);
		if(getPoint() == posEOD)
		{
			m_bPointEOL = true;
		}
		_fixInsertionPointCoords();
	}
	return bResult;
}

UT_Error FV_View::cmdInsertGraphic(const FG_ConstGraphicPtr& pFG)
{
	bool bDidGlob = false;

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	if (!isSelectionEmpty() && !m_FrameEdit.isActive())
	{
		bDidGlob = true;
		m_pDoc->beginUserAtomicGlob();
		_deleteSelection();
	}
	else if(m_FrameEdit.isActive())
	{
	       m_FrameEdit.setPointInside();
	}
	/*
	  Create a unique identifier for the data item.
	*/
	UT_UUIDPtr uuid = m_pDoc->getNewUUID();
	UT_return_val_if_fail(uuid != nullptr, UT_ERROR);
	std::string s = uuid->toString().value_or("");

	UT_Error errorCode = _insertGraphic(pFG, s.c_str());
	if(m_FrameEdit.isActive())
	{
		m_FrameEdit.setMode(FV_FrameEdit_NOT_ACTIVE);
	}

	_restorePieceTableState();

	_generalUpdate();
	if (bDidGlob)
		m_pDoc->endUserAtomicGlob();
	_updateInsertionPoint();

	return errorCode;
}

UT_Error FV_View::cmdInsertPositionedGraphic(const FG_ConstGraphicPtr& pFG)
{
	fl_BlockLayout * pBlock = nullptr;
	fp_Run * pRun = nullptr;
	UT_sint32 xCaret, yCaret;
	UT_uint32 heightCaret;
	UT_sint32 xCaret2, yCaret2;
	bool bDirection;
	bool bEOL = false;
	_findPositionCoords(getPoint(), bEOL, xCaret, yCaret, xCaret2, yCaret2, heightCaret, bDirection, &pBlock, &pRun);
	UT_return_val_if_fail(pBlock,UT_ERROR);
	return	cmdInsertPositionedGraphic(pFG,xCaret,yCaret);

}


UT_Error FV_View::cmdInsertPositionedGraphic(const FG_ConstGraphicPtr& pFG, UT_sint32 mouseX, UT_sint32 mouseY)
{
	m_pDoc->beginUserAtomicGlob();

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	if (!isSelectionEmpty())
	{
	       _clearSelection();
	}

	/*
	  Create a unique identifier for the data item.
	*/
	UT_UUIDPtr uuid = m_pDoc->getNewUUID();
	UT_return_val_if_fail(uuid != nullptr, UT_ERROR);
	std::string s = uuid->toString().value_or("");
	//
	// Find a document position close to the requested position
	//
	PT_DocPosition pos = getDocPositionFromXY(mouseX,mouseY);
	fl_BlockLayout * pBlock = getBlockAtPosition(pos);
	fp_Run *  pRun = nullptr;
	bool bEOL,bDir;
	bEOL = false;
	UT_sint32 x1,y1,x2,y2,iHeight;
	if(pBlock)
	{
		pRun = pBlock->findPointCoords(pos,bEOL,x1,y1,x2,y2,iHeight,bDir);
	}
	UT_nonnull_or_return(pRun, false);
	fp_Line * pLine = pRun->getLine();
	if(pLine == nullptr)
	{
	        return false;
	}

	// Also get max width, height

	fl_DocSectionLayout * pDSL = pBlock->getDocSectionLayout();
	double maxW = static_cast<double>(pDSL->getActualColumnWidth())*0.5/UT_LAYOUT_RESOLUTION;
	double maxH = static_cast<double>(pDSL->getActualColumnHeight())*0.5/ UT_LAYOUT_RESOLUTION;
	//
	// OK calculate all the properties of this image
	//
	std::string sWidth;
	std::string sHeight;
	double ratw = 1.0;
	double rath = 1.0;
	double rat = 1.0;
	double dw = static_cast<double>(pFG->getWidth());
	double dh = static_cast<double>(pFG->getHeight());

	if(dw > maxW/2.)
	{
	     ratw = maxW/dw;
	}
	if(dh > maxH/2.)
	{
	     rath = maxH/dh;
	}
	if(ratw < rath)
	{
	    rat = ratw;
	}
	else
	{
	    rat = rath;
	}
	// This preserves the aspect ratio and limits the size of the images
	dw = dw*rat;
	dh = dh*rat;
	sWidth =  UT_formatDimensionedValue(dw,"in", nullptr);
	sHeight =  UT_formatDimensionedValue(dh,"in", nullptr);
//
// Create a dataid for the object
//

	const char * dataID = pFG->createDataItem(m_pDoc, s.c_str());
	std::string sFrameProps;
	std::string sProp;
	std::string sVal;
	sProp = "frame-type";
	sVal = "image";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
//
// Turn off the borders.
//
	sProp = "top-style";
	sVal = "none";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	sProp = "right-style";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	sProp = "left-style";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	sProp = "bot-style";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
//
// Set width/Height
//
	sProp = "frame-width";
	sVal = sWidth;
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	sProp = "frame-height";
	sVal = sHeight;
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	double xpos = 0.0;
	double ypos= 0.0;

	sProp = "position-to";
	sVal = "column-above-text";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	if(isInHdrFtr(pos))
	{
		clearHdrFtrEdit();
		warpInsPtToXY(0,0,false);
		pos = getPoint();
	}

//
// Now calculate the Y offset to the Column
//
	fp_Column * pCol = static_cast<fp_Column *>(pLine->getColumn());
	UT_sint32 ixoff,iyoff;
	fp_Page * pPage = pCol->getPage();
	pPage->getScreenOffsets(static_cast<fp_Container *>(pCol),ixoff,iyoff);
	iHeight = static_cast<UT_sint32>(dh*UT_LAYOUT_RESOLUTION);
	UT_sint32 iposy = mouseY - iyoff - iHeight/2;
	ypos = static_cast<double>(iposy)/static_cast<double>(UT_LAYOUT_RESOLUTION);
	sProp = "frame-col-ypos";
	sVal = UT_formatDimensionedValue(ypos,"in", nullptr);
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	sProp = "wrap-mode";
	sVal = "wrapped-both";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	UT_sint32 iWidth = static_cast<UT_sint32>(dw * UT_LAYOUT_RESOLUTION);
	UT_sint32 iposx = mouseX - ixoff - iWidth/2;
	UT_sint32 iColW = static_cast<UT_sint32>(maxW*2.*UT_LAYOUT_RESOLUTION);
	if((iposx + iWidth) > (pCol->getX() + iColW))
	{
	  iposx = iColW - iWidth - pCol->getX();
	}
	if(iposx < pCol->getX())
	{
	      iposx = 0;
	}

	UT_DEBUGMSG(("iposx %d pCol->getX() %d \n",iposx,pCol->getX()));
	xpos =  static_cast<double>(iposx)/static_cast<double>(UT_LAYOUT_RESOLUTION);

	sProp = "frame-col-xpos";
	sVal = UT_formatDimensionedValue(xpos, "in", nullptr);
	UT_DEBUGMSG((" %s %s \n", sProp.c_str(), sVal.c_str()));
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
//
// Wrapped Mode
//
	sProp = "wrap-mode";
	sVal = "wrapped-both";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
//
// Now define the Frame attributes strux
//
	PP_PropertyVector attributes = {
		PT_STRUX_IMAGE_DATAID, dataID,
		"props", sFrameProps
	};
//
// This should place the the frame strux immediately after the block containing
// position posXY.
// It returns the Frag_Strux of the new frame.
//
	fl_BlockLayout * pBL = pBlock;
	if((pBL == nullptr) || (pRun == nullptr))
	{
	  return UT_ERROR;
	}
	fl_BlockLayout * pPrevBL = pBL;
	while(pBL && ((pBL->myContainingLayout()->getContainerType() == FL_CONTAINER_ENDNOTE) || (pBL->myContainingLayout()->getContainerType() == FL_CONTAINER_FOOTNOTE) || (pBL->myContainingLayout()->getContainerType() == FL_CONTAINER_TOC)|| (pBL->myContainingLayout()->getContainerType() == FL_CONTAINER_FRAME)))
	{
        UT_DEBUGMSG(("Skipping Block %p \n", (void*)pBL));
		pPrevBL = pBL;
		pBL = pBL->getPrevBlockInDocument();
	}
	if(pBL == nullptr)
	{
		pBL = pPrevBL;
	}
	UT_ASSERT((pBL->myContainingLayout()->getContainerType() != FL_CONTAINER_HDRFTR) 
		  && (pBL->myContainingLayout()->getContainerType() != FL_CONTAINER_SHADOW));
	pos = pBL->getPosition();
	pf_Frag_Strux * pfFrame = nullptr;
	m_pDoc->insertStrux(pos, PTX_SectionFrame, attributes, PP_NOPROPS, &pfFrame);
	PT_DocPosition posFrame = pfFrame->getPos();
	m_pDoc->insertStrux(posFrame+1,PTX_EndFrame);
	insertParaBreakIfNeededAtPos(posFrame+2);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();
	m_pDoc->endUserAtomicGlob();
	_generalUpdate();
	if(!isPointLegal())
	{
	      _makePointLegal();
	}
	_ensureInsertionPointOnScreen();
	notifyListeners(AV_CHG_MOTION | AV_CHG_ALL);
	return UT_OK;
}

/*!
 * This method inserts a MathML object and it's Latex representation
 * at the current insertion point.
 * It leaves the Object selected so it can be altered as needed.
 */
bool FV_View::cmdInsertLatexMath(UT_UTF8String & sLatex,
						   UT_UTF8String & sMath, bool compact)
{
  //
  // First create the Data Items
  //
        UT_UTF8String sMathName;
        UT_UTF8String sLatexName;
        sMathName = "MathLatex";
	sLatexName = "LatexMath";
	/*
	  Create a unique identifier for the data item.
	*/
	UT_UUIDPtr uuid = m_pDoc->getNewUUID();
	UT_return_val_if_fail(uuid != nullptr, false);
	auto result = uuid->toString();
	if (result.has_value()) {
		std::string s = result.value();
		sMathName += s;
		sLatexName += s;
	}

	UT_DEBUGMSG(("Inserting latex id name %s \n",sLatexName.utf8_str()));
	//
	// Insert these into the Piece Table
	//
	UT_ByteBufPtr mathBuf(new UT_ByteBuf);
	UT_ByteBufPtr latexBuf(new UT_ByteBuf);
	mathBuf->ins(0, reinterpret_cast<const UT_Byte *>(sMath.utf8_str()), static_cast<UT_uint32>(sMath.size()));
	latexBuf->ins(0, reinterpret_cast<const UT_Byte *>(sLatex.utf8_str()), static_cast<UT_uint32>(sLatex.size()));
	m_pDoc->createDataItem(sMathName.utf8_str(), false, mathBuf, "", nullptr);
	m_pDoc->createDataItem(sLatexName.utf8_str(), false, latexBuf, "", nullptr);

	// OK Insert the MathML Object
	PP_PropertyVector atts = {
		"dataid", sMathName.utf8_str(),
		"latexid", sLatexName.utf8_str(),
		"props", ""
	};

	const gchar *cur_style = nullptr;
	getStyle(&cur_style);
	if((cur_style != nullptr) && (*cur_style) && (strcmp(cur_style,"None") != 0))
	{
		atts.push_back(PT_STYLE_ATTRIBUTE_NAME);
		atts.push_back(cur_style);
	}

	bool bDidGlob = false;
	PP_PropertyVector props;

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();
	PT_DocPosition pos = getPoint();
	if (!isSelectionEmpty())
	{
		getCharFormat(props, false, pos);
		bDidGlob = true;
		m_pDoc->beginUserAtomicGlob();
		_deleteSelection();
	}
	else
	{
		getCharFormat(props, false, pos);
	}
	pos = getPoint();

	std::string sNewProps;
	ASSERT_PV_SIZE(props);
	for(auto iter = props.cbegin(); iter != props.cend(); iter += 2) {
		UT_std_string_setProperty(sNewProps, *iter, *(iter + 1));
	}

	UT_std_string_setProperty(sNewProps, "display", compact ? "inline": "block");
	atts[5] = sNewProps;
	m_pDoc->insertObject(pos, PTO_Math, atts, PP_NOPROPS);

	if (bDidGlob)
		m_pDoc->endUserAtomicGlob();

	_generalUpdate();
	_restorePieceTableState();
	cmdSelect(pos,pos+1);
	return true;
}

/*!
 * This method inserts a MathML object at the point presented.
 * It assumes that a data item with a name of the supplied filename has
 * already been inserted.
 */
bool FV_View::cmdInsertMathML(const char * szUID,PT_DocPosition pos)
{
	UT_DebugOnly<PT_DocPosition> posDebug = pos;
	UT_DEBUGMSG(("Insert Math Object at %d name %s \n",(PT_DocPosition)posDebug,szUID));

	PP_PropertyVector atts = {
		"dataid", szUID
	};
	const gchar *cur_style = nullptr;
	getStyle(&cur_style);
	if((cur_style != nullptr) && (*cur_style) && (strcmp(cur_style,"None") != 0))
	{
		atts.push_back(PT_STYLE_ATTRIBUTE_NAME);
		atts.push_back(cur_style);
	}
	bool bDidGlob = false;

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	if (!isSelectionEmpty())
	{
		bDidGlob = true;
		m_pDoc->beginUserAtomicGlob();
		_deleteSelection();
	}
	_makePointLegal();
	PP_PropertyVector props;
	getCharFormat(props, false, getPoint());
	m_pDoc->insertObject(getPoint(), PTO_Math, atts, props);

	if (bDidGlob)
		m_pDoc->endUserAtomicGlob();

	_generalUpdate();

	_restorePieceTableState();
	_updateInsertionPoint();
	return true;
}


/*!
 * This method inserts a Embed object at the point presented.
 * The calling routine should pass in a pointer to bytebuf that represents the
 * object.
 * Also needed are strings for the Mime-type and the type of Embeded object.
 *
 * eg for a GNOME-Office chart we'll have MIME-TYPE "application/chart+xml"
 * and sProps="embed-type: GOChart";
 */
bool FV_View::cmdInsertEmbed(const UT_ConstByteBufPtr & pBuf, PT_DocPosition pos, const char * szMime, const char * szProps)
{
	std::string sUID = "obj-";
	UT_UUIDPtr uuid = m_pDoc->getNewUUID();
	UT_return_val_if_fail(uuid != nullptr, false);
	auto result = uuid->toString();
	if (result) {
		sUID += result.value();
	}

	PP_PropertyVector atts = {
		"dataid", sUID,
		"props", ""
	};
	const gchar *cur_style = nullptr;
	UT_String sBuf(reinterpret_cast<const char *>(pBuf->getPointer(0)),pBuf->getLength());
	UT_DEBUGMSG(("Chart text is... \n %s \n",sBuf.c_str()));
	bool created = m_pDoc->createDataItem(sUID.c_str(), false, pBuf, szMime, nullptr);
	if(!created)
	{
	    return created;
	}
	getStyle(&cur_style);
	if((cur_style != nullptr) && (*cur_style) && (strcmp(cur_style,"None") != 0))
	{
		atts.push_back(PT_STYLE_ATTRIBUTE_NAME);
		atts.push_back(cur_style);
	}
	bool bDidGlob = false;
	PP_PropertyVector props;

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	if (!isSelectionEmpty())
	{
		bDidGlob = true;
		m_pDoc->beginUserAtomicGlob();
		_deleteSelection();
		// Reevaluate pos after deleting the selection
		pos = getPoint();
	}
	getCharFormat(props, false, pos);

	std::string sFullProps;
	for(auto iter = props.cbegin(); iter != props.cend(); iter += 2) {
		UT_std_string_setProperty(sFullProps, *iter, *(iter + 1));
	}

	UT_DEBUGMSG(("Supplied props %s \n", szProps));
	UT_std_string_addPropertyString(sFullProps, szProps);
	UT_DEBUGMSG(("Property String at Update Object is %s \n", sFullProps.c_str()));
	atts[3] = sFullProps;
	m_pDoc->insertObject(pos, PTO_Embed, atts, PP_NOPROPS);
	if (bDidGlob)
		m_pDoc->endUserAtomicGlob();

	_generalUpdate();

	_restorePieceTableState();
	_updateInsertionPoint();
	cmdSelect(pos,pos+1);
	return true;
}

/*!
 * This method updates the Embedded object currently selected with a new
 * object defined with the supplied bytebuffer, as well as strings to represent
 * the MIME/Type and Object type.
 * 
 * eg for a GNOME-Office chart we'll have MIME-TYPE "application/chart+xml"
 * and sProps="embed-type: GOChart";
 */
bool FV_View::cmdUpdateEmbed(const UT_ConstByteBufPtr & pBuf, const char * szMime, const char * szProps)
{
	if (isSelectionEmpty())
	{
	     return false;
	}
	PT_DocPosition pos1 = getPoint();
	PT_DocPosition pos2 = getSelectionAnchor();
	PT_DocPosition posTemp = 0;
	if(pos2 < pos1)
	{
	  posTemp = pos2;
	  pos2 = pos1;
	  pos1 = posTemp;
	}
	fl_BlockLayout * pBL =	getCurrentBlock();
	if(!pBL)
		return false;
	fp_Run * pRun;
	UT_sint32 xPoint,yPoint,xPoint2,yPoint2,iPointHeight;
	bool bDirection;
	pRun = pBL->findPointCoords(pos1, false, xPoint,
								yPoint, xPoint2, yPoint2,
								iPointHeight, bDirection);
	if(pRun && (pRun->getType() != FPRUN_EMBED) )
	{
	  pos1 = pos2;
	}
	pRun = pBL->findPointCoords(pos1, false, xPoint,
								yPoint, xPoint2, yPoint2,
								iPointHeight, bDirection);
	if(pRun == nullptr)
	{
	  return false;
	}
	if(pRun->getType() != FPRUN_EMBED)
	{
	  return false;
	}

	std::string sUID="obj-";
	UT_UUIDPtr uuid = m_pDoc->getNewUUID();
	UT_return_val_if_fail(uuid != nullptr, false);
	auto result = uuid->toString();
	if (result) {
		sUID += result.value();
	}
	PP_PropertyVector atts = {
		"dataid", sUID,
		"props", ""
	};

	bool bres = m_pDoc->createDataItem(sUID.c_str(),false,pBuf, szMime, nullptr);
	UT_return_val_if_fail(bres,false)

	const gchar *cur_style = nullptr;
	getStyle(&cur_style);
	if((cur_style != nullptr) && (*cur_style) && (strcmp(cur_style,"None") != 0))
	{
		atts.push_back(PT_STYLE_ATTRIBUTE_NAME);
		atts.push_back(cur_style);
	}
	PP_PropertyVector props;

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();
	m_pDoc->beginUserAtomicGlob();
	getCharFormat(props, false, pos1);
	std::string sFullProps;

	ASSERT_PV_SIZE(props);

	for (auto iter = props.cbegin(); iter != props.cend(); iter += 2) {
		xxx_UT_DEBUGMSG(("Update Embed Prop %s val %s \n", props[i], props[i+1]));
	    UT_std_string_setProperty(sFullProps, *iter, *(iter + 1));
	}

	UT_DEBUGMSG(("Supplied props %s \n", szProps));
	UT_std_string_addPropertyString(sFullProps, szProps);
	atts[3] = sFullProps;
	UT_DEBUGMSG(("Property String at Update Object is %s \n", sFullProps.c_str()));
	_deleteSelection();
	m_pDoc->insertObject(pos1, PTO_Embed, atts, PP_NOPROPS);
	m_pDoc->endUserAtomicGlob();

	_generalUpdate();
	_restorePieceTableState();
	_updateInsertionPoint();
	cmdSelect(pos1,pos1+1);
	return true;
}

/*!
 * This method updates the Embedded object in pRun with a new
 * object defined with the supplied bytebuffer, as well as strings to represent
 * the MIME/Type and Object type.
 * 
 * eg for a GNOME-Office chart we'll have MIME-TYPE "application/chart+xml"
 * and sProps="embed-type: GOChart";
 */
bool FV_View::cmdUpdateEmbed(fp_Run * pRun, const UT_ConstByteBufPtr & pBuf, const char * szMime, const char * szProps)
{
	if(pRun == nullptr || pRun->getType() != FPRUN_EMBED)
	{
	  return false;
	}
	PT_DocPosition pos;
	bool flag;
	pRun->mapXYToPosition(0, 0, pos, flag, flag, flag);
	cmdSelect (pos, pos+1);
	std::string sUID = "obj-";
	std::string s;
	UT_UUIDPtr uuid = m_pDoc->getNewUUID();
	UT_return_val_if_fail(uuid != nullptr, false);
	auto result = uuid->toString();
	if (result) {
		sUID += result.value();
	}

	PP_PropertyVector atts = {
		"dataid", sUID,
		"props", ""
	};
	bool bres = m_pDoc->createDataItem(sUID.c_str(), false, pBuf, szMime, nullptr);
	UT_return_val_if_fail(bres, false)
	const gchar *cur_style = nullptr;
	getStyle(&cur_style);
	if((cur_style != nullptr) && (*cur_style) && (strcmp(cur_style,"None") != 0))
	{
		atts.push_back(PT_STYLE_ATTRIBUTE_NAME);
		atts.push_back(cur_style);
	}

	PP_PropertyVector props;

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();
	m_pDoc->beginUserAtomicGlob();
	getCharFormat(props, false, pos);

	std::string sFullProps;
	ASSERT_PV_SIZE(props);
	for(auto iter = props.cbegin(); iter != props.cend(); iter += 2) {

		bool remove = false;
		const std::string & prop = *iter;
		// Filter out size properties
		if (prop == "width" || prop == "height" ||
			prop == "descent" || prop == "ascent") {
			remove = true;
		}
	    xxx_UT_DEBUGMSG(("Update Embed Prop %s val %s \n",props[i],props[i+1]));
	    UT_std_string_setProperty(sFullProps, prop, remove ? "" : *(iter + 1));
	}
	UT_DEBUGMSG(("Supplied props %s \n", szProps));
	UT_std_string_addPropertyString(sFullProps, szProps);
	atts[3] = sFullProps;
	UT_DEBUGMSG(("Property String at Update Object is %s \n", sFullProps.c_str()));
	m_pDoc->changeSpanFmt(PTC_AddFmt, pos, pos + 1, atts, PP_NOPROPS);
	m_pDoc->endUserAtomicGlob();

	_generalUpdate();
	_restorePieceTableState();
	_updateInsertionPoint();
	cmdSelect(pos,pos+1);
	return true;
}

/*!
 * This method deletes the Embedded object in pRun.
 */
bool FV_View::cmdDeleteEmbed(fp_Run * pRun)
{
	if(pRun == nullptr || pRun->getType() != FPRUN_EMBED)
	{
	  return false;
	}
	PT_DocPosition pos;
	bool flag;
	pRun->mapXYToPosition(0, 0, pos, flag, flag, flag);
	cmdSelect (pos, pos+1);
	_saveAndNotifyPieceTableChange();
	m_pDoc->beginUserAtomicGlob();
	_deleteSelection();
	m_pDoc->endUserAtomicGlob();

	_generalUpdate();
	_restorePieceTableState();
	_updateInsertionPoint();
	cmdSelect(pos,pos);
	return true;
}

/*!
 * This method inserts an image at the strux of type iStruxType at the 
 * point given by ipos.
 * This is useful for speficifying images as backgrounds to pages and cells.
 */
UT_Error FV_View::cmdInsertGraphicAtStrux(const FG_ConstGraphicPtr& pFG, PT_DocPosition iPos, PTStruxType iStruxType)
{
	bool bDidGlob = false;

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	/*
	  Create a unique identifier for the data item.
	*/
	UT_UUIDPtr uuid = m_pDoc->getNewUUID();
	UT_return_val_if_fail(uuid != nullptr, UT_ERROR);
	std::string s = uuid->toString().value_or("");

	UT_Error errorCode = pFG->insertAtStrux(m_pDoc,
											m_pG->getDeviceResolution(),
											iPos,
											iStruxType, s.c_str());

	_restorePieceTableState();

	_generalUpdate();
	if (bDidGlob)
		m_pDoc->endUserAtomicGlob();
	_updateInsertionPoint();

	return errorCode;
}

#ifdef ENABLE_SPELL
void FV_View::cmdContextSuggest(UT_uint32 ndx, fl_BlockLayout * ppBL,
								const fl_PartOfBlockPtr& ppPOB)
{
	// locate the squiggle
	PT_DocPosition pos = getPoint();
	fl_BlockLayout* pBL;
	fl_PartOfBlockPtr pPOB;

	if (!ppBL)
		pBL = _findBlockAtPosition(pos);
	else
		pBL = ppBL;
	UT_ASSERT(pBL);

	if (!ppPOB)
		pPOB = pBL->getSpellSquiggles()->get(pos - pBL->getPosition());
	else
		pPOB = ppPOB;
	UT_ASSERT(pPOB);

	// grab the suggestion
	UT_UCS4Char * replace = _lookupSuggestion(pBL, pPOB, ndx);

	if (!replace)
		return;

	// make the change
	UT_ASSERT(isSelectionEmpty());

	moveInsPtTo(static_cast<PT_DocPosition>(pBL->getPosition() + pPOB->getOffset()));
	extSelHorizontal(true, pPOB->getPTLength());

	UT_UCS4Char * selection;
	getSelectionText(selection);
	getDictForSelection ()->correctWord (selection, UT_UCS4_strlen (selection),
										 replace, UT_UCS4_strlen (replace));
	cmdCharInsert(replace, UT_UCS4_strlen(replace));
	FREEP(selection);
	FREEP(replace);
}

void FV_View::cmdContextIgnoreAll(void)
{
	// locate the squiggle
	PT_DocPosition pos = getPoint();
	fl_BlockLayout* pBL = _findBlockAtPosition(pos);
	UT_return_if_fail(pBL);
	const fl_PartOfBlockPtr& pPOB = pBL->getSpellSquiggles()->get(pos - pBL->getPosition());
	if(!pPOB) // this can happen with very rapid right-clicks
	{
		return;
	}

	// grab a copy of the word
	UT_GrowBuf pgb(1024);
	bool bRes = pBL->getBlockBuf(&pgb);
	UT_ASSERT(bRes);
	if(!bRes) 
	{
		UT_WARNINGMSG(("getBlockBuf() failed in %s:%d",
					   __FILE__, __LINE__));
	}

	const UT_UCS4Char * pBuf;
	UT_sint32 iLength, iPTLength, iBlockPos;

	fl_BlockSpellIterator BSI(pBL, pPOB->getOffset());
	BSI.nextWordForSpellChecking(pBuf, iLength, iBlockPos, iPTLength);
	
	// make the change
	getDictForSelection ()->ignoreWord ((const UT_UCS4Char *)pBuf, (size_t)iLength);
	{
		// remove the squiggles, too
		fl_DocSectionLayout * pSL = m_pLayout->getFirstSection();
		if(pSL)
		{
			fl_BlockLayout* b = pSL->getNextBlockInDocument();
			while (b)
			{
				// TODO: just check and remove matching squiggles
				// for now, destructively recheck the whole thing
				m_pLayout->queueBlockForBackgroundCheck(FL_DocLayout::bgcrSpelling, b);
				b = static_cast<fl_BlockLayout *>(b->getNextBlockInDocument());
			}
		}
	}
}

void FV_View::cmdContextAdd(void)
{
	// locate the squiggle
	PT_DocPosition pos = getPoint();
	fl_BlockLayout* pBL = _findBlockAtPosition(pos);
	UT_return_if_fail(pBL);
	const fl_PartOfBlockPtr& pPOB = pBL->getSpellSquiggles()->get(pos - pBL->getPosition());
	if(!pPOB) // this can happen with very rapid right-clicks
	{
		return;
	}

	// grab a copy of the word
	UT_GrowBuf pgb(1024);
	bool bRes = pBL->getBlockBuf(&pgb);
	UT_ASSERT(bRes);
	if(!bRes) 
	{
		UT_WARNINGMSG(("getBlockBuf() failed in %s:%d",
					   __FILE__, __LINE__));
	}

	const UT_UCS4Char * pBuf;
	UT_sint32 iLength, iPTLength, iBlockPos;

	fl_BlockSpellIterator BSI(pBL, pPOB->getOffset());
	BSI.nextWordForSpellChecking(pBuf, iLength, iBlockPos, iPTLength);

	// make the change
	if (getDictForSelection ()->addToCustomDict (pBuf, iLength))
	{
		// remove the squiggles, too
		fl_DocSectionLayout * pSL = m_pLayout->getFirstSection();
		if(pSL)
		{
			fl_BlockLayout* b = pSL->getNextBlockInDocument();
			while (b)
			{
				// TODO: just check and remove matching squiggles
				// for now, destructively recheck the whole thing
				if(b->getContainerType() == FL_CONTAINER_BLOCK)
				{
					m_pLayout->queueBlockForBackgroundCheck(FL_DocLayout::bgcrSpelling, b);
					b = static_cast<fl_BlockLayout *>(b->getNextBlockInDocument());
				}
				else
				{
					b = static_cast<fl_BlockLayout *>(b->getNext());
				}
			}
		}
	}
}
#endif


/*!
 * Remove all the Headers or footers from the section owning the current Page.
\param bool isHeader remove the header if true, the footer if false.
*/
void FV_View::cmdRemoveHdrFtr( bool isHeader)
{
//
// Branch to Header/Footer sections.
//
	fp_ShadowContainer * pHFCon = nullptr;
	fl_HdrFtrShadow * pShadow = nullptr;
	fl_HdrFtrSectionLayout * pHdrFtr = nullptr;

	if(isHeader)
	{
		fp_Page * pPage = getCurrentPage();
		pHFCon = pPage->getHdrFtrP(FL_HDRFTR_HEADER);
		if(pHFCon == nullptr)
		{
			return;
		}
//
// Now see if we are in the header to be removed. If so, jump out.
//
		if (!isSelectionEmpty())
			_clearSelection();
		if(isHdrFtrEdit())
		{
			clearHdrFtrEdit();
			_setPoint(pPage->getFirstLastPos(true));
		}
	}
	else
	{
		fp_Page * pPage = getCurrentPage();
		pHFCon = pPage->getHdrFtrP(FL_HDRFTR_FOOTER);
		if(pHFCon == nullptr)
		{
			return;
		}
//
// Now see if we are in the Footer to be removed. If so, jump out.
//
		if (!isSelectionEmpty())
			_clearSelection();
		if(isHdrFtrEdit())
		{
			clearHdrFtrEdit();
			_setPoint(pPage->getFirstLastPos(false));
		}
	}
	pShadow = pHFCon->getShadow();
	UT_return_if_fail(pShadow);

	m_pDoc->beginUserAtomicGlob();

	_saveAndNotifyPieceTableChange();
//
// Save current document position.
//
	PT_DocPosition curPoint = getPoint();
//
// Get the hdrftrSectionLayout
// Get it's position.
// Find the last run in the Section and get it's position.
//
// Need code here to remove all the header/footers.
//
	pHdrFtr = pShadow->getHdrFtrSectionLayout();
	fl_DocSectionLayout * pDSL = pHdrFtr->getDocSectionLayout();
//
// Repeat this code 4 times to remove all the DocSection Layouts.
//
	setCursorWait();
	if(isHeader)
	{
		pHdrFtr = pDSL->getHeader();
		if(pHdrFtr)
		{
			_removeThisHdrFtr(pHdrFtr);
		}
		pHdrFtr = pDSL->getHeaderEven();
		if(pHdrFtr)
		{
			_removeThisHdrFtr(pHdrFtr);
		}
		pHdrFtr = pDSL->getHeaderFirst();
		if(pHdrFtr)
		{
			_removeThisHdrFtr(pHdrFtr);
		}
		pHdrFtr = pDSL->getHeaderLast();
		if(pHdrFtr)
		{
			_removeThisHdrFtr(pHdrFtr);
		}
	}
	else
	{
		pHdrFtr = pDSL->getFooter();
		if(pHdrFtr)
		{
			_removeThisHdrFtr(pHdrFtr);
		}
		pHdrFtr = pDSL->getFooterEven();
		if(pHdrFtr)
		{
			_removeThisHdrFtr(pHdrFtr);
		}
		pHdrFtr = pDSL->getFooterFirst();
		if(pHdrFtr)
		{
			_removeThisHdrFtr(pHdrFtr);
		}
		pHdrFtr = pDSL->getFooterLast();
		if(pHdrFtr)
		{
			_removeThisHdrFtr(pHdrFtr);
		}
	}
//
// After erarsing the cursor, Restore to the point before all this mess started.
//
	_setPoint(curPoint);

	// Signal PieceTable Changes have finished
	_restorePieceTableState();

	_generalUpdate();
	updateScreen(true); // fix 1803, force screen update/redraw

	_updateInsertionPoint();
	m_pDoc->endUserAtomicGlob();
	clearCursorWait();
	notifyListeners (AV_CHG_HDRFTR | AV_CHG_FMTSECTION);
}

/*!
 * Start edit header mode. If there is no header one will be inserted.
 * otherwise start editing the header on the current page.
 */
void FV_View::cmdEditHeader(void)
{
	_cmdEditHdrFtr(FL_HDRFTR_HEADER);
	notifyListeners (AV_CHG_HDRFTR | AV_CHG_FMTSECTION);
}

/*!
 * Start edit footer mode. If there is no footer one will be inserted.
 * otherwise start editing the footer on the current page.
 */
void FV_View::cmdEditFooter(void)
{
	_cmdEditHdrFtr(FL_HDRFTR_FOOTER);
	notifyListeners (AV_CHG_HDRFTR | AV_CHG_FMTSECTION);
}

void FV_View::cmdAcceptRejectRevision(bool bReject, UT_sint32 xPos, UT_sint32 yPos)
{
	UT_DEBUGMSG(( "FV_View::cmdAcceptRejectRevision [bReject=%d]\n",bReject ));

	PT_DocPosition iStart, iEnd;
	fl_BlockLayout * pBlock = nullptr;
	fp_Run *pRun = nullptr;

	// Signal PieceTable Change
	_saveAndNotifyPieceTableChange();

	if(isSelectionEmpty())
	{
		if(xPos || yPos) // if given 0,0 use current position
		{
			warpInsPtToXY(xPos, yPos,true);
		}

		pBlock = getCurrentBlock();
		PT_DocPosition iRelPos = getPoint() - pBlock->getPosition(false);

		pRun = pBlock->getFirstRun();
		while (pRun && pRun->getNextRun() && pRun->getBlockOffset()+ pRun->getLength() <= iRelPos)
			pRun= pRun->getNextRun();

		UT_return_if_fail(pRun);

		iStart = pBlock->getPosition(false) + pRun->getBlockOffset();
		iEnd = pBlock->getPosition(false) + pRun->getBlockOffset() + pRun->getLength();

	}
	else
	{
		iStart = getPoint();
		iEnd   = getSelectionAnchor();
	}

	// remove the selection, since things will get inserted, deleted, etc.
	_clearSelection();
	
	m_pDoc->acceptRejectRevision(bReject,iStart,iEnd,m_iViewRevision);
	_restorePieceTableState();
	_generalUpdate();
}

void FV_View::cmdSetRevisionLevel(UT_uint32 i)
{
	UT_return_if_fail( i <= PD_MAX_REVISION );
	// first set the same level in Doc; we do this unconditionally,
	// this way the doc will always save the level the user last used
	// NB: the doc id and the view id can be differnt if the user
	// changed it in some other view
	m_pDoc->setShowRevisionId(i);

	if(m_iViewRevision != i)
	{
		m_iViewRevision = i;

		// need to rebuild the doc to reflect the new level ...
		m_pLayout->rebuildFromHere(static_cast<fl_DocSectionLayout *>(m_pLayout->getFirstSection()));

		// we have to force redraw here, see bug 10486
		queueDraw(nullptr);
	}
}

/*!
    finds the next/previous revision and sets selection to it
    TODO the selection will not cross block boundaries; it probably should

    \param bNext: if true the search is carried out in forward direction
    \return returns true on succes
*/
bool FV_View::cmdFindRevision(bool bNext, UT_sint32 xPos, UT_sint32 yPos)
{
	if(xPos || yPos)
	{
		// this is the case we were called from context menu ...
		warpInsPtToXY(xPos, yPos,true);
	}

	if(!isSelectionEmpty())
	{
		_moveToSelectionEnd(bNext);
	}
	
	fl_BlockLayout * pBL =	getCurrentBlock();

	if(!pBL)
		return false;
	
	fl_DocSectionLayout * pSL = pBL->getDocSectionLayout();

	if(!pSL)
		return false;
	
	fp_Run * pRun;
	UT_sint32 xPoint,yPoint,xPoint2,yPoint2,iPointHeight;
	bool bDirection;

	pRun = pBL->findPointCoords(getPoint(), false, xPoint,
								yPoint, xPoint2, yPoint2,
								iPointHeight, bDirection);

	if(!pRun)
		return false;

	if(bNext)
	{
		pRun = pRun->getNextRun();

		while(pSL)
		{
			while(pBL)
			{
				while(pRun)
				{
					if(pRun->containsRevisions() && !pRun->isHidden())
					{
						goto move_point;
					}

					pRun = pRun->getNextRun();
				}

				pBL = pBL->getNextBlockInDocument();
			}

			pSL = pSL->getNextDocSection();
		}
	}
	else
	{
		pRun = pRun->getPrevRun();

		while(pSL)
		{
			while(pBL)
			{
				while(pRun)
				{
					if(pRun->containsRevisions() && !pRun->isHidden())
					{
						goto move_point;
					}

					pRun = pRun->getPrevRun();
				}

				pBL = pBL->getPrevBlockInDocument();
			}

			pSL = pSL->getPrevDocSection();
		}
	}

	return false;
	
 move_point:
	UT_return_val_if_fail(pRun && pBL, false);

	// we want to span the selection not only over this run, but also
	// all subesequent runs that contain the same revions
	// TODO: probably should do this across block/section boundaries
	fp_Run * pRun2 = bNext ? pRun->getNextRun() : pRun->getPrevRun();
	fp_Run * pOldRun2 = pRun;

	auto& pR1 = pRun->getRevisions();

	while(pRun2)
	{
		if(pRun2->containsRevisions() && !pRun2->isHidden())
		{
			// test the two runs, if their revions are the same
			// include this one as well
			auto& pR2 = pRun2->getRevisions();

			if(!(*pR1 == *pR2))
				break;
		}
		else
		{
			break;
		}

		pOldRun2 = pRun2;
		pRun2 = bNext ? pRun2->getNextRun() : pRun2->getPrevRun();
	}

	// backtrack (we want pRun2 to be the last run in the selection
	pRun2 = pOldRun2;
	UT_return_val_if_fail(pRun2, false);
	
	PT_DocPosition dpos1, dpos2;

	if(bNext)
	{
		dpos1 = pBL->getPosition() + pRun->getBlockOffset();
		dpos2 = pRun2->getBlock()->getPosition() + pRun2->getBlockOffset() + pRun2->getLength();
	}
	else
	{
		dpos1 = pRun2->getBlock()->getPosition() + pRun2->getBlockOffset();
		dpos2 = pBL->getPosition() + pRun->getBlockOffset() + pRun->getLength();
	}
	
	cmdSelect(dpos1, dpos2);
	
	return true;
}

void FV_View::_updateDatesBeforeSave(bool bOverwriteCreated)
{
    time_t now = time(nullptr);
    std::string timeStr = ctime(&now);
    
    if (bOverwriteCreated)
    {
        m_pDoc->setMetaDataProp(PD_META_KEY_DATE, timeStr);
    } else
    {
        std::string metaValue;
        if(!m_pDoc->getMetaDataProp(PD_META_KEY_DATE, metaValue))
        {
            m_pDoc->setMetaDataProp(PD_META_KEY_DATE, timeStr);
        }
    }
    
    m_pDoc->setMetaDataProp(PD_META_KEY_DATE_LAST_CHANGED, timeStr);
}


/*!
 * Sort the paragraphs touched by the selection alphabetically.
 *
 * Paragraph properties (lists, indentation, alignment) stay in place -
 * only the text content moves between paragraphs.  A paragraph's
 * replacement text takes the character formatting of its original
 * first character.
 */
bool FV_View::cmdSortParagraphs(bool bAscending)
{
	STD_DOUBLE_BUFFERING_FOR_THIS_FUNCTION

	UT_return_val_if_fail(!isSelectionEmpty(), false);
	UT_return_val_if_fail(getSelectionMode() == FV_SelectionMode_Single,
						  false);

	UT_GenericVector<fl_BlockLayout *> vecBlocks;
	getBlocksInSelection(&vecBlocks);
	UT_sint32 nBlocks = vecBlocks.getItemCount();
	UT_return_val_if_fail(nBlocks >= 2, false);

	/* collect each block's text in document order */
	std::vector<UT_UCS4String> origText;
	origText.reserve(nBlocks);
	for (UT_sint32 i = 0; i < nBlocks; ++i)
	{
		fl_BlockLayout * pBL = vecBlocks.getNthItem(i);
		if (!pBL || pBL->getContainerType() != FL_CONTAINER_BLOCK)
			return false;
		PT_DocPosition pos = pBL->getPosition(true);
		UT_sint32 len = pBL->getLength();
		UT_UCS4Char * pText = len > 0
			? getTextBetweenPos(pos, pos + len) : nullptr;
		if (pText)
			origText.emplace_back(pText);
		else
			origText.emplace_back();
		FREEP(pText);
	}

	/* sorted copy - stable so equal lines keep their original order */
	std::vector<UT_UCS4String> sorted(origText);
	std::stable_sort(sorted.begin(), sorted.end(),
		[bAscending](const UT_UCS4String & a, const UT_UCS4String & b)
		{
			gint cmp = UT_go_utf8_collate_casefold(
				const_cast<UT_UCS4String &>(a).utf8_str(),
				const_cast<UT_UCS4String &>(b).utf8_str());
			return bAscending ? cmp < 0 : cmp > 0;
		});

	m_pDoc->beginUserAtomicGlob();
	for (UT_sint32 i = 0; i < nBlocks; ++i)
	{
		if (!UT_UCS4_strcmp(sorted[i].ucs4_str(),
							origText[i].ucs4_str()))
			continue;
		fl_BlockLayout * pBL = vecBlocks.getNthItem(i);
		/* getPosition(true) is the block strux frag position and
		 * getLength() counts it too - the text lives at pos+1 and
		 * is len-1 characters long.  Never touch the strux frag or
		 * the paragraph structure collapses. */
		PT_DocPosition posText = pBL->getPosition(true) + 1;
		UT_sint32 lenText = pBL->getLength() - 1;
		const UT_UCS4String & newText = sorted[i];
		if (newText.length())
		{
			/* insert before the old text so it inherits the first
			 * character's formatting, then remove the old text */
			m_pDoc->insertSpan(posText, newText.ucs4_str(),
							   newText.length());
		}
		if (lenText > 0)
		{
			UT_uint32 iCount = 0;
			m_pDoc->deleteSpan(posText + newText.length(),
							   posText + newText.length() + lenText,
							   nullptr, iCount);
		}
	}
	m_pDoc->endUserAtomicGlob();

	_generalUpdate();
	return true;
}

/*!
 * Quick paragraph-border presets for the ribbon Borders menu.
 * szWhich: bottom/top/left/right/none/all/outside/inside/insideh/hline.
 * Edges get a 0.5pt solid black border; "none" clears all four.
 */
bool FV_View::cmdParaBorder(const char * szWhich)
{
	UT_return_val_if_fail(szWhich && *szWhich, false);
	std::string which(szWhich);

	if (which == "hline")
	{
		/* horizontal rule: an empty current block just gets a bottom
		 * border, otherwise break the paragraph first */
		fl_BlockLayout * pBL = getCurrentBlock();
		UT_return_val_if_fail(pBL, false);
		if (pBL->getLength() > 1)
		{
			moveInsPtTo(FV_DOCPOS_EOB);
			insertParagraphBreak();
			pBL = getCurrentBlock();
			UT_return_val_if_fail(pBL, false);
		}
		PT_DocPosition pos = pBL->getPosition();
		const PP_PropertyVector props = {
			"bot-style", "solid",
			"bot-color", "000000",
			"bot-thickness", "0.5pt",
		};
		m_pDoc->changeStruxFmt(PTC_AddFmt, pos, pos,
							   PP_NOPROPS, props, PTX_Block);
		_generalUpdate();
		return true;
	}

	UT_GenericVector<fl_BlockLayout *> vBlock;
	getBlocksInSelection(&vBlock);
	UT_sint32 nBlocks = vBlock.getItemCount();
	UT_return_val_if_fail(nBlocks > 0, false);

	bool on[4] = {false, false, false, false};	/* top bot left right */
	UT_sint32 iLastBlock = nBlocks;

	if (which == "top")			on[0] = true;
	else if (which == "bottom")	on[1] = true;
	else if (which == "left")	on[2] = true;
	else if (which == "right")	on[3] = true;
	else if (which == "all" || which == "outside")
		on[0] = on[1] = on[2] = on[3] = true;
	else if (which == "inside" || which == "insideh")
	{
		/* lines between stacked paragraphs: bottom edge on every
		 * block but the last */
		on[1] = true;
		if (nBlocks > 1)
			iLastBlock = nBlocks - 1;
	}
	else if (which != "none")
		return false;

	m_pDoc->beginUserAtomicGlob();
	/* edge property names must outlive props - static storage */
	static const char * edgeProps[4][3] = {
		{"top-style", "top-color", "top-thickness"},
		{"bot-style", "bot-color", "bot-thickness"},
		{"left-style", "left-color", "left-thickness"},
		{"right-style", "right-color", "right-thickness"},
	};
	for (UT_sint32 i = 0; i < iLastBlock; ++i)
	{
		fl_BlockLayout * pBL = vBlock.getNthItem(i);
		UT_nonnull_or_continue(pBL);
		PT_DocPosition pos = pBL->getPosition();
		PP_PropertyVector props;
		for (int e = 0; e < 4; ++e)
		{
			if (!on[e])
				continue;
			props.push_back(edgeProps[e][0]);
			props.push_back("solid");
			props.push_back(edgeProps[e][1]);
			props.push_back("000000");
			props.push_back(edgeProps[e][2]);
			props.push_back("0.5pt");
		}
		if (!props.empty())
			m_pDoc->changeStruxFmt(PTC_AddFmt, pos, pos,
								   PP_NOPROPS, props, PTX_Block);
	}
	if (which == "none")
	{
		for (UT_sint32 i = 0; i < nBlocks; ++i)
		{
			fl_BlockLayout * pBL = vBlock.getNthItem(i);
			UT_nonnull_or_continue(pBL);
			PT_DocPosition pos = pBL->getPosition();
			const PP_PropertyVector props = {
				"top-style", "", "bot-style", "",
				"left-style", "", "right-style", "",
				"top-color", "", "bot-color", "",
				"left-color", "", "right-color", "",
				"top-thickness", "", "bot-thickness", "",
				"left-thickness", "", "right-thickness", "",
			};
			m_pDoc->changeStruxFmt(PTC_RemoveFmt, pos, pos,
								   PP_NOPROPS, props, PTX_Block);
		}
	}
	m_pDoc->endUserAtomicGlob();
	_generalUpdate();
	return true;
}
