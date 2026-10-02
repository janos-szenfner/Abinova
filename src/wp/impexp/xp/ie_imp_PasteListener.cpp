/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2003 Martin Sevior <msevior@physics.unimelb.edu.au>
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


#include "ie_imp_PasteListener.h"
#include "pp_AttrProp.h"
#include "pf_Frag.h"
#include "pf_Frag_Strux.h"
#include "px_CR_FmtMark.h"
#include "px_CR_FmtMarkChange.h"
#include "px_CR_Object.h"
#include "px_CR_ObjectChange.h"
#include "px_CR_Span.h"
#include "px_CR_SpanChange.h"
#include "px_CR_Strux.h"
#include "px_CR_StruxChange.h"

/*!
 * This nifty little class allows all importers to also be used for pasting
 * into the document.
 * The idea is that we create a dummy document which we import to as usual
 * with the impoters.
 * After the Dummy document is completed we do a PD_Document::tellListener on
 * it with this class as the listner class.
 * This class translates all the populate().... and populateStrux()...
 * methods into insertSpan(..) insertStrux(...) methods at the current 
 * insertion point.
 *
 * Hey presto we have pasted into the current document. Pretty cool eh?
 */
IE_Imp_PasteListener::IE_Imp_PasteListener(PD_Document * pDocToPaste, PT_DocPosition insPoint, PD_Document * pSourceDoc) : 
	m_pPasteDocument(pDocToPaste),
	m_insPoint(insPoint),
	m_bFirstSection(true),
	m_bFirstBlock(true),
	m_bAdoptFirstBlockFmt(false),
	m_pSourceDoc(pSourceDoc)
{
}	
bool  IE_Imp_PasteListener::populate(fl_ContainerLayout* /* sfh */,
					 const PX_ChangeRecord * pcr)
{
	PT_AttrPropIndex indexAP = pcr->getIndexAP();
	const PP_AttrProp* pAP = nullptr;
	UT_DEBUGMSG(("SEVIOR: Doing Populate Section in PasteListener \n"));
	PP_PropertyVector atts;
	PP_PropertyVector props;
	if (m_pSourceDoc->getAttrProp(indexAP, &pAP) && pAP)
	{
		atts = pAP->getAttributes();
		props = pAP->getProperties();
	}
	else
	{
		return false;
	}

	switch (pcr->getType())
	{
	case PX_ChangeRecord::PXT_InsertSpan:
	{
		const PX_ChangeRecord_Span * pcrs = static_cast<const PX_ChangeRecord_Span *>(pcr);
		UT_uint32 len = pcrs->getLength();
  
		PT_BufIndex bi = pcrs->getBufIndex();
		const UT_UCS4Char* pChars = 	m_pSourceDoc->getPointer(bi);
		PP_AttrProp* pfAP = const_cast<PP_AttrProp *>(pAP);
		// only advance the insertion point when the span actually
		// landed, otherwise m_insPoint desyncs from the document and
		// every later insert writes at the wrong position
		if (!m_pPasteDocument->insertSpan(m_insPoint,pChars,len,pfAP))
		{
			UT_DEBUGMSG(("PasteListener: insertSpan of %u chars failed at pos %u\n",
						 len, static_cast<unsigned>(m_insPoint)));
			return false;
		}
		m_insPoint += len;
		return true;
	}

	case PX_ChangeRecord::PXT_InsertObject:
	{
		const PX_ChangeRecord_Object * pcro = static_cast<const PX_ChangeRecord_Object *>(pcr);
		if (!m_pPasteDocument->insertObject(m_insPoint,pcro->getObjectType(),atts,props))
		{
			UT_DEBUGMSG(("PasteListener: insertObject type %d failed at pos %u\n",
						 static_cast<int>(pcro->getObjectType()),
						 static_cast<unsigned>(m_insPoint)));
			return false;
		}
		m_insPoint++;
		return true;
	}

	case PX_ChangeRecord::PXT_InsertFmtMark:
	{
		// a failed format change loses formatting only - keep pasting
		if (!m_pPasteDocument->changeSpanFmt(PTC_SetExactly,m_insPoint,m_insPoint,atts,props))
		{
			UT_DEBUGMSG(("PasteListener: changeSpanFmt failed at pos %u\n",
						 static_cast<unsigned>(m_insPoint)));
		}
		return true;
	}
	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		return false;
	}
	return true;
}

bool  IE_Imp_PasteListener::populateStrux(pf_Frag_Strux* sdh,
									  const PX_ChangeRecord * pcr,
										  fl_ContainerLayout* * /* psfh */)
{
//
// TODO graphics in struxes
// TODO UID stuff
//
	UT_ASSERT(pcr->getType() == PX_ChangeRecord::PXT_InsertStrux);
	const PX_ChangeRecord_Strux * pcrx = static_cast<const PX_ChangeRecord_Strux *> (pcr);
	PT_AttrPropIndex indexAP = pcr->getIndexAP();
	const PP_AttrProp* pAP = nullptr;
	UT_DEBUGMSG(("SEVIOR: Doing Populate Strux in PasteListener \n"));
	PP_PropertyVector atts;
	PP_PropertyVector props;
	if (m_pSourceDoc->getAttrProp(indexAP, &pAP) && pAP)
	{
		atts = pAP->getAttributes();
		props = pAP->getProperties();
	}
	else
	{
		return false;
	}
	switch (pcrx->getStruxType())
	{
	case PTX_Section:
	{
		if(m_bFirstSection)
		{
//
// Every doc has a first section. Now is good time to extract all the 
// data items from the source document and stuff them into pasted doc
//
// Now these can be found via the properties of the spans and strux's
//
			PD_DataItemHandle pHandle = nullptr;
			std::string mimeType;
			const char * szName= nullptr;
			UT_ConstByteBufPtr pBuf;
			UT_sint32 k = 0;
			while (m_pSourceDoc->enumDataItems(k, &pHandle, &szName, pBuf, &mimeType))
			{
				if (!m_pPasteDocument->createDataItem(szName,false,pBuf,mimeType,&pHandle))
				{
					UT_DEBUGMSG(("createDataItem %s failed\n", szName));
				}
				k++;
			}
			m_bFirstSection = false;
			if (sdh->getNext() && (sdh->getNext()->getType() == pf_Frag::PFT_Strux) &&
			    (static_cast<pf_Frag_Strux*>(sdh->getNext())->getStruxType() != PTX_Block))
			{
			    // The second frag is not a PXT_Block (it is probably a PTX_SectionTable)
			    // The first block encountered needs to be inserted in the piece table
			    m_bFirstBlock = false;
			}
			return true;
		}
		//
		// We don't actually paste in a section though. Since a paste 
		// is not meant to insert a section break
		//
		//m_pPasteDocument->insertStrux(m_insPoint,PTX_Section,atts,props);
		// m_insPoint++;
		return true;
	}
	case PTX_Block:
	{
		if(m_bFirstBlock)
		{
			m_bFirstBlock = false;
			if (m_bAdoptFirstBlockFmt && (!atts.empty() || !props.empty()))
			{
				/* the first source block is merged into the block
				 * containing the insertion point - donate its
				 * attributes/props so e.g. a pasted chapter heading
				 * keeps its style */
				m_pPasteDocument->changeStruxFmt(PTC_AddFmt,
						m_insPoint, m_insPoint, atts, props, PTX_Block);
			}
			return true;
		}
		if (!_insertStrux(pcrx->getStruxType(),atts,props))
		{
			return false;
		}
		if (m_bAdoptFirstBlockFmt)
		{
			/* insertStrux inherits the previous block's attr/props
			 * (paragraph-split semantics); when splicing whole
			 * documents the source block's AP is authoritative */
			m_pPasteDocument->changeStruxFmt(PTC_SetExactly,
					m_insPoint, m_insPoint, atts, props, PTX_Block);
		}
		return true;
	}

	// structure types that are safe to splice into the target document:
	// all are creatable via pt_PieceTable::_createStrux() and arrive as
	// balanced begin/end pairs from the source walk
	case PTX_SectionFootnote:
	case PTX_SectionEndnote:
	case PTX_SectionHdrFtr:
	case PTX_SectionTable:
	case PTX_SectionCell:
	case PTX_SectionFrame:
	case PTX_SectionTOC:
	case PTX_SectionAnnotation:
	case PTX_EndFootnote:
	case PTX_EndEndnote:
	case PTX_EndTable:
	case PTX_EndCell:
	case PTX_EndFrame:
	case PTX_EndTOC:
	case PTX_EndAnnotation:
		return _insertStrux(pcrx->getStruxType(),atts,props);

	default:
		// PTX_SectionMarginnote/PTX_EndMarginnote have no _createStrux
		// support (insertStrux would always fail) and PTX_StruxDummy is
		// an internal sentinel - skip unknown struxes instead of
		// inserting them blindly; their content still pastes as normal
		// body text
		UT_DEBUGMSG(("PasteListener: skipping unsupported strux type %d\n",
					 static_cast<int>(pcrx->getStruxType())));
		return true;
	}

	return true;
}

/*!
 * Insert a strux frag at the paste insertion point and advance
 * m_insPoint only when the insert actually landed - a failed insert
 * must not move the tracked position or every later insert would be
 * written at the wrong offset.
 */
bool IE_Imp_PasteListener::_insertStrux(PTStruxType pts,
										const PP_PropertyVector & atts,
										const PP_PropertyVector & props)
{
	if (!m_pPasteDocument->insertStrux(m_insPoint,pts,atts,props))
	{
		UT_DEBUGMSG(("PasteListener: insertStrux type %d failed at pos %u\n",
					 static_cast<int>(pts),
					 static_cast<unsigned>(m_insPoint)));
		return false;
	}
	m_insPoint++;
	return true;
}

PD_Document * IE_Imp_PasteListener::getDoc(void) const
{
	return m_pPasteDocument;
}

