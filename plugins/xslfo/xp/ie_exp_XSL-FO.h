/* AbiWord
 * Copyright (C) 2001 AbiSource, Inc.
 * Copyright (C) 2025 Hubert Figuière
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

#include "ut_std_string.h"
#include "ie_exp.h"
#include "ie_Table.h"
#include "pl_Listener.h"

class PD_Document;
class s_XSL_FO_Listener;

class ListHelper;

// The exporter/writer for the XML/XSL FO spec

class IE_Exp_XSL_FO_Sniffer : public IE_ExpSniffer
{
	friend class IE_Exp;

public:
	IE_Exp_XSL_FO_Sniffer (const char * name);
	virtual ~IE_Exp_XSL_FO_Sniffer () {}

	virtual bool recognizeSuffix(const char * szSuffix) override;
	virtual bool getDlgLabels(const char ** szDesc,
							   const char ** szSuffixList,
							   IEFileType * ft) override;
	virtual UT_Error constructExporter(PD_Document * pDocument,
										IE_Exp ** ppie) override;
};

class IE_Exp_XSL_FO : public IE_Exp
{
public:
	IE_Exp_XSL_FO(PD_Document * pDocument);
	virtual ~IE_Exp_XSL_FO();

protected:
	virtual UT_Error _writeDocument() override;

private:
	s_XSL_FO_Listener *	m_pListener;
	UT_uint32 m_error;
};

/************************************************************/
/************************************************************/

class s_XSL_FO_Listener : public PL_Listener
{
public:

	s_XSL_FO_Listener(PD_Document * pDocument,
					  IE_Exp_XSL_FO * pie);
	virtual ~s_XSL_FO_Listener();

	virtual bool populate(fl_ContainerLayout* sfh,
								 const PX_ChangeRecord * pcr) override;

	virtual bool populateStrux(pf_Frag_Strux* sdh,
									  const PX_ChangeRecord * pcr,
									  fl_ContainerLayout* * psfh) override;

	virtual bool change(fl_ContainerLayout* sfh,
							   const PX_ChangeRecord * pcr) override;

	virtual bool insertStrux(fl_ContainerLayout* sfh,
									const PX_ChangeRecord * pcr,
									pf_Frag_Strux* sdh,
									PL_ListenerId lid,
									void (* pfnBindHandles)(pf_Frag_Strux* sdhNew,
															PL_ListenerId lid,
															fl_ContainerLayout* sfhNew)) override;

	virtual bool signal(UT_uint32 iSignal) override;

protected:

	void                _handlePageSize(PT_AttrPropIndex api);
	void				_handleDataItems(void);
	void				_handleLists(void);
	void				_handleBookmark(PT_AttrPropIndex api);
	void				_handleEmbedded(PT_AttrPropIndex api);
	void				_handleField(const PX_ChangeRecord_Object * pcro, PT_AttrPropIndex api);
	void				_handleFrame(PT_AttrPropIndex api);
	void				_handleHyperlink(PT_AttrPropIndex api);
	void				_handleImage(PT_AttrPropIndex api);
	void				_handleMath(PT_AttrPropIndex api);
	void				_handlePositionedImage(PT_AttrPropIndex api);
	void                _outputData(const UT_UCS4Char * data, UT_uint32 length);

	void				_closeSection(void);
	void				_closeBlock(void);
	void				_closeSpan(void);
	void				_closeLink(void);
	void				_openBlock(PT_AttrPropIndex api);
	void				_openSection(PT_AttrPropIndex api);
	void				_openSpan(PT_AttrPropIndex api);
	void				_openListItem(void);
	void				_popListToDepth(UT_sint32 depth);

	void				_openTable(PT_AttrPropIndex api);
	void				_openRow(void);
	void				_openCell(PT_AttrPropIndex api);
	void				_closeTable(void);
	void				_closeRow(void);
	void				_closeCell(void);
	void				_handleTableColumns(void);

	UT_UTF8String		_getCellColors(void);
	UT_UTF8String		_getCellThicknesses(void);
	UT_UTF8String		_getTableColors(void);
	UT_UTF8String		_getTableThicknesses(void);

	void				_tagClose(UT_uint32 tagID, const UT_UTF8String & content, bool newline = true);
	void				_tagOpen(UT_uint32 tagID, const UT_UTF8String & content, bool newline = true);
	void				_tagOpenClose(const UT_UTF8String & content, bool suppress = true, bool newline = true);
	UT_uint32			_tagTop(void);

private:

	PD_Document *		m_pDocument;
	IE_Exp_XSL_FO *	    m_pie;

	bool				m_bFirstWrite;
	bool				m_bInLink;
	bool				m_bInNote;
	bool				m_bInSection;
	bool				m_bInSpan;
	bool				m_bWroteListField;

	UT_sint32			m_iBlockDepth;
	UT_uint32			m_iLastClosed;
	UT_sint32			m_iListBlockDepth;
	UT_uint32			m_iListID;

	ie_Table			mTableHelper;
	std::vector<std::string> m_utvDataIDs;
	std::stack<UT_uint32>	m_utnsTagStack;
	std::vector<ListHelper *> m_Lists;
};
