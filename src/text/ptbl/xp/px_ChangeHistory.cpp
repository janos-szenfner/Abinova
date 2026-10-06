/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
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


#include "ut_types.h"
#include "px_ChangeRecord.h"
#include "px_ChangeHistory.h"
#include "px_CR_Span.h"
#include "px_CR_Glob.h"
#include "pt_PieceTable.h"
#include "ut_debugmsg.h"
#include "xap_App.h"
#include "xap_Prefs.h"


// m_undoPosition is the position of the undo pointer.
// a value of zero means no undo history.
// the first undo item is at ...[m_undoPosition-1]
// the first redo item is at ...[m_undoPosition] if present.

px_ChangeHistory::px_ChangeHistory(pt_PieceTable * pPT)
	: m_undoPosition(0),
	  m_savePosition(0),
	  m_pPT(pPT),
	  m_iAdjustOffset(0),
	  m_bOverlap(false),
	  m_iMinUndo(0),
	  m_bScanUndoGLOB(false),
	  m_iUndoOps(0),
	  m_iGlobDepth(0)
{
}

px_ChangeHistory::~px_ChangeHistory()
{
}

// this function is used when restoring an older version of a document
// when maintaining full history, since that makes the info in the
// undo meaningless (and causes crashes)

void px_ChangeHistory::clearHistory()
{
	m_vecChangeRecords.clear();
	m_undoPosition = 0;
	m_savePosition = 0;
	m_iAdjustOffset = 0;
	m_bOverlap = false;
	m_iMinUndo = 0;
	m_bScanUndoGLOB = false;
	m_iUndoOps = 0;
	m_iGlobDepth = 0;
}

void px_ChangeHistory::_invalidateRedo(void)
{
	UT_sint32 kLimit = static_cast<UT_sint32>(m_vecChangeRecords.size());
	UT_return_if_fail (m_undoPosition <= kLimit);

	/* count the ops and net glob markers the erase removes so the
	 * running undo-depth accounting stays exact; the tail normally
	 * starts on an op boundary (undo only ever stops at one) */
	UT_sint32 iTailStarts = 0;
	UT_sint32 iTailEnds = 0;
	UT_sint32 iTailOps = 0;

	UT_sint32 i = m_undoPosition - m_iAdjustOffset;
	for (UT_sint32 k = m_undoPosition - m_iAdjustOffset; k < kLimit; k++)
	{
		PX_ChangeRecord * pcrTemp = _getCR(i);
		if (!pcrTemp)
			break;
		if (pcrTemp->isFromThisDoc())
		{
			if (pcrTemp->getType() == PX_ChangeRecord::PXT_GlobMarker)
			{
				UT_Byte flags =
					static_cast<PX_ChangeRecord_Glob *>(pcrTemp)->getFlags();
				if (flags & (PX_ChangeRecord_Glob::PXF_MultiStepStart |
							 PX_ChangeRecord_Glob::PXF_UserAtomicStart))
				{
					iTailStarts++;
					if (iTailStarts - iTailEnds == 1)
						iTailOps++;
				}
				else if (flags & (PX_ChangeRecord_Glob::PXF_MultiStepEnd |
								  PX_ChangeRecord_Glob::PXF_UserAtomicEnd))
				{
					iTailEnds++;
				}
			}
			else if (iTailStarts == iTailEnds)
			{
				iTailOps++;
			}
		    m_vecChangeRecords.erase(m_vecChangeRecords.begin() + i);
		}
		else
		    i++;
	}
	m_undoPosition = static_cast<UT_sint32>(m_vecChangeRecords.size());
	if (m_savePosition > m_undoPosition)
		m_savePosition = -1;
	m_iAdjustOffset = 0;
	m_iUndoOps -= iTailOps;
	if (m_iUndoOps < 0)
		m_iUndoOps = 0;
	m_iGlobDepth -= (iTailStarts - iTailEnds);
	if (m_iGlobDepth < 0)
		m_iGlobDepth = 0;
}

PD_Document * px_ChangeHistory::getDoc(void) const
{
  return m_pPT->getDocument();
}
        
bool px_ChangeHistory::addChangeRecord(PX_ChangeRecord * pcr)
{
	// add a change record to the history.
	// blow away any redo, since it is now invalid.
	xxx_UT_DEBUGMSG(("Add CR Pos %d Type %d indexAP %x \n",pcr->getPosition(),pcr->getType(),pcr->getIndexAP()));
	xxx_UT_DEBUGMSG(("Before invalidate Undo pos %d savepos %d iAdjust %d \n",m_undoPosition,m_savePosition,m_iAdjustOffset));
	if (pcr && pcr->getDocument() == nullptr)
	{
	    pcr->setDocument(getDoc());
	}
	if (m_bOverlap)
	{
	      clearHistory();
	}
	if (!m_pPT->isDoingTheDo())
	{
		if(pcr && pcr->isFromThisDoc())
		{
			_invalidateRedo();
			m_vecChangeRecords.insert(m_vecChangeRecords.begin() + m_undoPosition++,
									std::unique_ptr<PX_ChangeRecord>(pcr));
			xxx_UT_DEBUGMSG(("After Invalidate Undo pos %d savepos %d iAdjust %d \n",m_undoPosition,m_savePosition,m_iAdjustOffset));
			m_iAdjustOffset = 0;

			/* op accounting: one undo step is a lone record or a whole
			 * glob - a Start marker opens it at depth 0 (nested globs
			 * only deepen it), the matching End closes it */
			if (pcr->getType() == PX_ChangeRecord::PXT_GlobMarker)
			{
				UT_Byte flags =
					static_cast<PX_ChangeRecord_Glob *>(pcr)->getFlags();
				if (flags & (PX_ChangeRecord_Glob::PXF_MultiStepStart |
							 PX_ChangeRecord_Glob::PXF_UserAtomicStart))
				{
					if (m_iGlobDepth == 0)
						m_iUndoOps++;
					m_iGlobDepth++;
				}
				else if ((flags & (PX_ChangeRecord_Glob::PXF_MultiStepEnd |
								   PX_ChangeRecord_Glob::PXF_UserAtomicEnd)) &&
						 m_iGlobDepth > 0)
				{
					m_iGlobDepth--;
				}
			}
			else if (m_iGlobDepth == 0)
			{
				m_iUndoOps++;
			}

			UT_uint32 iLimit = getUndoDepthLimit();
			if (iLimit && m_iUndoOps > static_cast<UT_sint32>(iLimit))
				pruneHistory(iLimit);
			return true;
		}
		else
		{
			m_vecChangeRecords.emplace_back(pcr);
			UT_sint32 iPos = m_undoPosition - m_iAdjustOffset;
			m_undoPosition = static_cast<UT_sint32>(m_vecChangeRecords.size());
			m_iAdjustOffset = m_undoPosition - iPos;
			return true;
		}
	}
	else
	{
//
// Just save the cr for later deletion with the PT
//
		UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
		m_vecChangeRecords.emplace_back(pcr);
		return true;
	}
}

bool px_ChangeHistory::canDo(bool bUndo) const
{
	if(m_bOverlap)
		return false;
	PX_ChangeRecord * pcr;
	UT_sint32 iAdj = m_iAdjustOffset;
	setScanningUndoGLOB(false);
	bool b = (bUndo ? getUndo(&pcr) : getRedo(&pcr));
	setScanningUndoGLOB(false);
	m_iAdjustOffset = iAdj;
	return b;
}

UT_sint32 px_ChangeHistory::getSavePosition(void) const
{
	return m_savePosition;
}

UT_uint32 px_ChangeHistory::getUndoPos(void) const
{
	return (m_undoPosition - m_iAdjustOffset);
}

void px_ChangeHistory::setSavePosition(UT_sint32 savePosition)
{
	m_savePosition = savePosition;
}

bool px_ChangeHistory::getUndo(PX_ChangeRecord ** ppcr, bool bStatic) const
{
	if (m_bOverlap)
	{
		*ppcr = nullptr;
		return false;
	}
	UT_sint32 iGLOB = 0;
	bool bGotOne = false;
	PX_ChangeRecord * pcr = nullptr;
	PX_ChangeRecord * pcrFirst = nullptr;
	bool bCorrect = false;
	UT_sint32 iAdjust = m_iAdjustOffset;
	UT_sint32 iLoop = 0;
	//	_printHistory(50);
	while (!bGotOne)
	{
		if ((m_undoPosition - m_iAdjustOffset -iLoop) <= m_iMinUndo)
		{
			if (bStatic)
				m_iAdjustOffset = iAdjust;
			return false;
		}
		
		pcr = _getCR(m_undoPosition-m_iAdjustOffset-1-iLoop);
		UT_return_val_if_fail(pcr, false); // just bail out, everything seems wrong

		//
		// Do Adjustments for blocks of remote CR's. Scan through local globs
		// to check for remote CR's which overlap it.
		//
		if((iGLOB== 0) && !pcr->isFromThisDoc())
		{
			bCorrect = true;
			m_iAdjustOffset++;
			UT_DEBUGMSG(("Doing undo iAdjust incremented to %d \n",m_iAdjustOffset));
		}
		else if ((iGLOB==0) && (pcr->getType() == PX_ChangeRecord::PXT_GlobMarker) && pcr->isFromThisDoc() && !isScanningUndoGLOB() && (m_iAdjustOffset > 0))
		{
			iGLOB++;
			pcrFirst = pcr;
			iLoop++;
			setScanningUndoGLOB(true);
		}
		else if((iGLOB>0) && (pcr->getType() == PX_ChangeRecord::PXT_GlobMarker) &&  pcr->isFromThisDoc())
		{
			if(isScanningUndoGLOB())
				pcr = pcrFirst;
			bGotOne = true;
		}
		else if(iGLOB == 0)
		{
			bGotOne = true;
			if(m_iAdjustOffset > 0)
				bCorrect = true;
		}
		//
		// we're here if we've started scanning through a glob in the local
		// document to see if it overlaps a later remote change.
		//
		else
		{
			PT_DocPosition low, high;
			PT_DocPosition lowWork = 0;
            PT_DocPosition highWork;
			UT_sint32 iAccumOffset = 0;
			getCRRange(pcr, low, high);
			for (UT_sint32 i = 0; i<m_iAdjustOffset;i++)
			{
				PX_ChangeRecord *pcrTmp = _getCR(m_undoPosition-i-1);
				if (!pcrTmp)
					break;
				if (!pcrTmp->isFromThisDoc())
				{
					UT_sint32 iCur = getDoc()->getAdjustmentForCR(pcrTmp);
					if(pcrTmp->getPosition() <= lowWork+iCur)
					{
						iAccumOffset += iCur;
					}
					lowWork = low + iAccumOffset;
					highWork = high + iAccumOffset;
					PT_DocPosition p1,p2;
					getCRRange(pcrTmp,p1,p2);
					bool bZero = (p1 == p2);
					if(bZero)
						lowWork++;
					if (doesOverlap(pcrTmp,lowWork,highWork))
					{
						*ppcr = nullptr;
						//
						// OK now we have to invalidate the undo stack
						// to just before the first pcr we pulled off.
						//
						if(m_undoPosition-iAdjust > 0)
						{
							m_iMinUndo = m_undoPosition-iAdjust-1;
						}
						else
						{
							m_iMinUndo = 0;
						}
						m_iAdjustOffset = iAdjust;
						m_iAdjustOffset++;
						return false;
					}
				}
			}
			
			iLoop++;
		}
	}

	PX_ChangeRecord * pcrOrig = pcr;
	if (bCorrect)
	{
	    pcr->setAdjustment(0);
	    PT_DocPosition pos = pcr->getPosition();
	    UT_sint32 iAdj = 0;
		UT_sint32 iCurrAdj  = 0;
	    PT_DocPosition low, high;
	    getCRRange(pcr, low, high);
	    for (UT_sint32 i = m_iAdjustOffset-1; i>=0;i--)
	    {
			pcr = _getCR(m_undoPosition-i-1);
			if (!pcr)
				break;
			if (!pcr->isFromThisDoc())
			{
				iCurrAdj = getDoc()->getAdjustmentForCR(pcr);
			    if(pcr->getPosition() <= static_cast<PT_DocPosition>(static_cast<UT_sint32>(pos) + iAdj + iCurrAdj))
			    {
					iAdj += iCurrAdj;
					low += iCurrAdj;
					high += iCurrAdj;
			    }
				PT_DocPosition p1,p2;
				getCRRange(pcr,p1,p2);
				bool bZero = (p1 == p2);
				PT_DocPosition low1 = low;
				if(bZero)
					low1++;
			    if (doesOverlap(pcr,low1,high))
			    {
					UT_DEBUGMSG(("CR Type %d adj pos %d Overlaps found with CR pos %d \n",pcrOrig->getType(),pcrOrig->getPosition()+iAdj,pcr->getPosition()));
					UT_DEBUGMSG((" Orig Adj low %d high %d \n",low,high));

					*ppcr = nullptr;
					m_iMinUndo = m_undoPosition-m_iAdjustOffset-1;
					return false;
			    }
			}
	    }
	    pcrOrig->setAdjustment(iAdj);
	    m_iAdjustOffset++;
	}

	UT_ASSERT(pcrOrig->isFromThisDoc());
	*ppcr = pcrOrig;
	if(bStatic)
	    m_iAdjustOffset = iAdjust;
	return true;
}



/*!
 * This method returns the nth element off the undo stack.
 * 0 returns the top element
 * 1 returns the next element
 * etc
 * The result is not adjusted for undo's in the presence of remote
 * changerecords and no attempt is made to see if an undo is legal 
 * (ie doesn't overlap with a later remote CR) or not.
 */
bool px_ChangeHistory::getNthUndo(PX_ChangeRecord ** ppcr, UT_uint32 undoNdx) const
{
	UT_sint32 iAdjust = static_cast<UT_sint32>(m_undoPosition) - m_iAdjustOffset;
	UT_sint32 iAdjIdx = static_cast<UT_sint32>(undoNdx);
	bool bGotOne = false;
	while(!bGotOne)
	{
		if (static_cast<UT_sint32>(iAdjust - iAdjIdx -1) <= static_cast<UT_sint32>(m_iMinUndo))
			return false;
	
		PX_ChangeRecord * pcr = _getCR(iAdjust-iAdjIdx-1);
		UT_return_val_if_fail(pcr, false);
		if(pcr->isFromThisDoc())
		{
			*ppcr = pcr;
			return true;
		}
		else
		{
			iAdjust--;
		}
	}
	return false;
}


bool px_ChangeHistory::getRedo(PX_ChangeRecord ** ppcr) const
{
	if ((m_iAdjustOffset == 0) && (m_undoPosition >= static_cast<UT_sint32>(m_vecChangeRecords.size())))
		return false;
	
	if (m_bOverlap)
		return false;
	
	UT_sint32 iRedoPos = m_undoPosition-m_iAdjustOffset;
	if(iRedoPos <0)
		return false;
	PX_ChangeRecord * pcr = _getCR(iRedoPos);
	UT_return_val_if_fail(pcr, false);

	// leave records from external documents in place so we can correct
	bool bIncrementAdjust = false;

	if (pcr->isFromThisDoc())
	{
		*ppcr = pcr;
		if (m_iAdjustOffset == 0)
		{
		     return true;
		}
		else
		{
		     bIncrementAdjust = true;
		     m_iAdjustOffset--;
		}
	}
	
	while (pcr && !pcr->isFromThisDoc() && (m_iAdjustOffset > 0))
	{
	    pcr = _getCR(iRedoPos);
	    m_iAdjustOffset--;
		iRedoPos++;
	    bIncrementAdjust = true;
	    xxx_UT_DEBUGMSG(("AdjustOffset decremented -1 %d ", m_iAdjustOffset));
	}
	
	if (pcr && bIncrementAdjust)
	{
	    PX_ChangeRecord * pcrOrig = pcr;
	    pcr->setAdjustment(0);
	    PT_DocPosition low,high;
	    getCRRange(pcr,low,high);
	    PT_DocPosition pos = pcr->getPosition();
	    UT_sint32 iAdj = 0;
	    for (UT_sint32 i = m_iAdjustOffset; i >= 1;i--)
	    {
			pcr = _getCR(m_undoPosition-i);
			if (!pcr)
				break;
			if (!pcr->isFromThisDoc())
			{
				UT_sint32 iCur = getDoc()->getAdjustmentForCR(pcr);
			    if (pcr->getPosition() <= static_cast<PT_DocPosition>(static_cast<UT_sint32>(pos) + iAdj + iCur))
			    {
					iAdj += iCur; 
					low += iCur;
					high += iCur;
			    }
				PT_DocPosition p1,p2;
				getCRRange(pcr,p1,p2);
				bool bZero = (p1 == p2);
				if(bZero)
					m_bOverlap = doesOverlap(pcr,low+1,high);
				else
					m_bOverlap = doesOverlap(pcr,low,high);
			    if (m_bOverlap)
			    {
					*ppcr = nullptr;
					return false;
			    }
			}
	    }
	    pcr = pcrOrig;
	    pcr->setAdjustment(iAdj);
	    xxx_UT_DEBUGMSG(("Redo Adjustment set to %d \n",iAdj));
	}
	
	if (pcr && pcr->isFromThisDoc())
	{  
	    *ppcr = pcr;
	    if(bIncrementAdjust)
	    {
	        m_iAdjustOffset += 1; // for didRedo
	        xxx_UT_DEBUGMSG(("AdjustOffset incremented -2 %d \n", m_iAdjustOffset));
	    }
	    return true;
	}

	*ppcr = nullptr;
	return false;
}

bool px_ChangeHistory::didUndo(void)
{
	xxx_UT_DEBUGMSG((" Doing Undo void in PT undopos %d savePos pos %d \n",m_undoPosition,m_savePosition));
	if (m_bOverlap)
	{
	    clearHistory();
	    return false;
	}
	
	UT_return_val_if_fail(m_undoPosition > 0, false);
	UT_return_val_if_fail(m_undoPosition - m_iAdjustOffset > m_iMinUndo, false);

	PX_ChangeRecord * pcr = _getCR(m_undoPosition-m_iAdjustOffset-1);
	UT_return_val_if_fail(pcr && pcr->isFromThisDoc(), false);

	if (m_iAdjustOffset == 0)
		m_undoPosition--;
	pcr = _getCR(m_undoPosition-m_iAdjustOffset);
	if (pcr && !pcr->getPersistance() && (m_savePosition >= 0))
	{
		/* a save position of -1 means the saved state was pruned out
		 * of the history; the doc stays dirty and we simply stop
		 * tracking it rather than blocking undo */
		UT_return_val_if_fail(m_savePosition > 0,false);
		m_savePosition--;
	}
	return true;
}

bool px_ChangeHistory::didRedo(void)
{
	xxx_UT_DEBUGMSG((" Doing didRedo void in PT undopos %d savePos pos %d iAdjustOffset %d \n",m_undoPosition,m_savePosition,m_iAdjustOffset));
	if (m_bOverlap)
	{
	    clearHistory();
	    return false;
	}
	if ((m_undoPosition - m_iAdjustOffset) >= static_cast<UT_sint32>(m_vecChangeRecords.size()))
		return false;
	PX_ChangeRecord * pcr = _getCR(m_undoPosition - m_iAdjustOffset);

	// leave records from external documents in place so we can correct

	if (pcr && !pcr->isFromThisDoc() && (m_iAdjustOffset == 0))
	        return false;
	if (m_iAdjustOffset > 0)
	{
		m_iAdjustOffset--;
		xxx_UT_DEBUGMSG(("AdjustOffset decremented -3 redo %d ", m_iAdjustOffset));
	}
	else
	{
		xxx_UT_DEBUGMSG(("Undo Position incremented in redo \n"));
		m_undoPosition++;
	}
	if (pcr && !pcr->getPersistance() && (m_savePosition >= 0))
		m_savePosition++;
	return true;
}

#if DEBUG
void px_ChangeHistory::_printHistory(UT_sint32 iPrev) const
{
	UT_sint32 i = 0;
	UT_sint32 iStop = 0;
	UT_sint32 iStart = 0;
	if(iPrev>0)
	{
		iStop =m_undoPosition-1 - iPrev;
		iStart = m_undoPosition-1;
	}
	else
	{
		iStart = static_cast<UT_sint32>(m_vecChangeRecords.size()) -1;
		iStop = iStart + iPrev;
	}
	if(iStop <0)
		iStop =0;
	for(i=iStart; i>= iStop;i--)
	{
			PX_ChangeRecord * pcr = _getCR(i);
			if(i != (m_undoPosition-m_iAdjustOffset-1))
			{
					UT_DEBUGMSG((" loc %d pos %d type %d isLocal %d \n",i,pcr->getPosition(),pcr->getType(),pcr->isFromThisDoc()));
			}
			else
			{
					UT_DEBUGMSG((" loc %d pos %d type %d isLocal %d <- Current undo record \n",i,pcr->getPosition(),pcr->getType(),pcr->isFromThisDoc()));
			}
	}
}
#endif

void px_ChangeHistory::coalesceHistory(const PX_ChangeRecord * pcr)
{
	// coalesce this record with the current undo record.

	UT_sint32 iAdj = m_iAdjustOffset;
	PX_ChangeRecord * pcrUndo = _getCR(m_undoPosition-1);
	UT_return_if_fail (pcrUndo);
	UT_return_if_fail (pcr->getType() == pcrUndo->getType());

	switch (pcr->getType())
	{
		case PX_ChangeRecord::PXT_InsertSpan:
		case PX_ChangeRecord::PXT_DeleteSpan:
			{
				const PX_ChangeRecord_Span * pcrSpan = static_cast<const PX_ChangeRecord_Span *>(pcr);
				PX_ChangeRecord_Span * pcrSpanUndo = static_cast<PX_ChangeRecord_Span *>(pcrUndo);

				if(pcr->isFromThisDoc())
				{
				  _invalidateRedo();
				  m_iAdjustOffset = 0;
				}
				else if(iAdj > 0) 
				{
				    m_iAdjustOffset = iAdj - 1;
				    xxx_UT_DEBUGMSG(("AdjustOffset decremented - 3 %d ", m_iAdjustOffset));

				}
				pcrSpanUndo->coalesce(pcrSpan);
			}
			return;

		default:
			UT_ASSERT_HARMLESS(0);
			return;
	}
}

void px_ChangeHistory::setClean(void)
{
	m_savePosition = m_undoPosition;
}

bool px_ChangeHistory::isDirty(void) const
{
	return (m_savePosition != m_undoPosition);
}

bool px_ChangeHistory::getCRRange(PX_ChangeRecord * pcr,PT_DocPosition & posLow, PT_DocPosition &posHigh) const
{
	PT_DocPosition length = 0;
	length = static_cast<PT_DocPosition>(abs(getDoc()->getAdjustmentForCR(pcr)));
	posLow = pcr->getPosition();
	posHigh = posLow+length;
	return true;
}

bool px_ChangeHistory:: doesOverlap(PX_ChangeRecord * pcr, PT_DocPosition low, PT_DocPosition high) const
{
	PT_DocPosition crLow = 0, crHigh = 0;
	getCRRange(pcr, crLow, crHigh);
	if ((crLow>=low) && (crLow<high))
		return true;
	if ((crHigh>low) && (crHigh<=high))
		return true;
	return false;
}

/*!
 * Configured ceiling on the number of undoable ops kept in the
 * history (MaxUndoOps pref; default 200, 0 disables the cap).
 * Negative or absent values fall back to the compiled default.
 */
UT_uint32 px_ChangeHistory::getUndoDepthLimit(void) const
{
	int iLimit = 200;
	XAP_Prefs * pPrefs = XAP_App::getApp() ? XAP_App::getApp()->getPrefs() : nullptr;
	if (pPrefs)
		pPrefs->getPrefsValueInt(XAP_PREF_KEY_MaxUndoOps, iLimit);
	if (iLimit < 0)
		iLimit = 0;
	return static_cast<UT_uint32>(iLimit);
}

/*!
 * Drop the oldest ops from the front of the history so at most
 * maxOps undoable ops remain.  An "op" is one user-level undo step:
 * a lone change record or a whole glob sequence (a MultiStep or
 * UserAtomic Start marker through its matching End marker, nested
 * globs included).  The cut lands only on op boundaries, so undo can
 * never stop inside a glob.
 *
 * Called from addChangeRecord() right after _invalidateRedo(), so the
 * tail holds no stale local redo records.  Records not from this
 * document (collab remote CRs) are erased with the prefix: they sit
 * below every surviving local record, and the undo/redo adjustment
 * scans only consult records *above* the one being replayed.
 */
void px_ChangeHistory::pruneHistory(UT_uint32 maxOps)
{
	if (!maxOps)
		return;

	std::vector<UT_sint32> opStarts;
	UT_sint32 iDepth = 0;
	const UT_sint32 iSize = static_cast<UT_sint32>(m_vecChangeRecords.size());
	for (UT_sint32 i = 0; i < iSize; i++)
	{
		PX_ChangeRecord * pcr = m_vecChangeRecords[i].get();
		if (!pcr || !pcr->isFromThisDoc())
			continue;
		if (pcr->getType() == PX_ChangeRecord::PXT_GlobMarker)
		{
			UT_Byte flags =
				static_cast<PX_ChangeRecord_Glob *>(pcr)->getFlags();
			if (flags & (PX_ChangeRecord_Glob::PXF_MultiStepStart |
						 PX_ChangeRecord_Glob::PXF_UserAtomicStart))
			{
				if (iDepth == 0)
					opStarts.push_back(i);
				iDepth++;
			}
			else if ((flags & (PX_ChangeRecord_Glob::PXF_MultiStepEnd |
							   PX_ChangeRecord_Glob::PXF_UserAtomicEnd)) &&
					 iDepth > 0)
			{
				iDepth--;
			}
			/* an End marker at depth 0 pairs with a Start that was
			 * already undone past - malformed, keep it inside the
			 * following op's span rather than letting it become a
			 * boundary undo could stop on */
		}
		else if (iDepth == 0)
		{
			opStarts.push_back(i);
		}
	}

	const UT_sint32 iOps = static_cast<UT_sint32>(opStarts.size());
	m_iUndoOps = iOps;
	m_iGlobDepth = iDepth;
	if (iOps <= static_cast<UT_sint32>(maxOps))
		return;

	const UT_sint32 iCut = opStarts[iOps - static_cast<UT_sint32>(maxOps)];
	m_vecChangeRecords.erase(m_vecChangeRecords.begin(),
							 m_vecChangeRecords.begin() + iCut);

	m_undoPosition -= iCut;
	if (m_undoPosition < 0)
		m_undoPosition = 0;
	m_iMinUndo -= iCut;
	if (m_iMinUndo < 0)
		m_iMinUndo = 0;
	if (m_savePosition >= 0)
	{
		m_savePosition -= iCut;
		if (m_savePosition < 0)
			m_savePosition = -1;	// saved state pruned - undo can never reach it
	}
	m_iUndoOps = static_cast<UT_sint32>(maxOps);
}
