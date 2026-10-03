/* AbiSource
 *
 * Copyright (C) 2002 Dom Lachowicz <cinamod@hotmail.com>
 *
 * Copyright (C) 2005 INdT
 * Copyright (C) 2025-2026 Abinova contributors
 * Author: Daniel d'Andrada T. de Carvalho <daniel.carvalho@indt.org.br>
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

// Internal includes
#include "ODe_ListenerAction.h"

// Abiword includes
#include "pl_Listener.h"
#include "ut_vector.h"
#include "ut_string_class.h"

// Standard includes
#include <map>
#include <string>
#include <vector>

// Internal classes
class ODe_AbiDocListenerImpl;
class ODe_AuxiliaryData;
class ODe_DocumentData;

// Abinova classes
class PD_Document;
class PP_AttrProp;
class fd_Field;
class PX_ChangeRecord_Object;

// libgsf (matches gsf's own forward declaration)
typedef struct _GsfOutput GsfOutput;

/**
 * Handles the mess involved in PL_Listener event handling translating then
 * into simple and clean calls to ODe_AbiDocListenerImpl.
 */
class ODe_AbiDocListener : public PL_Listener
{
public:

    ODe_AbiDocListener(PD_Document* pDocument,
                       ODe_AbiDocListenerImpl* pListenerImpl,
                       bool deleteWhenPop,
                       ODe_AuxiliaryData* pAuxData = nullptr);

    virtual ~ODe_AbiDocListener();

    virtual bool populate(fl_ContainerLayout* sfh, const PX_ChangeRecord * pcr) override;

    virtual bool populateStrux(pf_Frag_Strux* sdh,
        const PX_ChangeRecord * pcr, fl_ContainerLayout* * psfh) override;

    virtual bool change(fl_ContainerLayout* sfh, const PX_ChangeRecord * pcr) override;

    virtual bool insertStrux(fl_ContainerLayout* sfh,
                const PX_ChangeRecord * pcr,
                pf_Frag_Strux* sdh,
                PL_ListenerId lid,
                void (* pfnBindHandles)(pf_Frag_Strux* sdhNew,
                            PL_ListenerId lid,
                            fl_ContainerLayout* sfhNew)) override;

    virtual bool signal(UT_uint32 iSignal) override;

    void finished();

private:

    void _openSpan(PT_AttrPropIndex api);
    void _closeSpan();

    void _openBlock(PT_AttrPropIndex api);
    void _closeBlock();

    void _openSection(PT_AttrPropIndex api, bool recursiveCall = false);
    void _closeSection(bool recursiveCall = false);

    void _openField(const PX_ChangeRecord_Object* pcro, PT_AttrPropIndex api);
    void _closeField();

    void _openTable(PT_AttrPropIndex api, bool recursiveCall = false);
    void _closeTable(bool recursiveCall = false);

    void _openCell(PT_AttrPropIndex api, bool recursiveCall = false);
    void _closeCell(bool recursiveCall = false);

    void _openFootnote(PT_AttrPropIndex api);
    void _closeFootnote();

    void _openEndnote(PT_AttrPropIndex api);
    void _closeEndnote();

    void _openAnnotation(PT_AttrPropIndex api, const std::string& defaultName );
    void _closeAnnotation();
    void _endAnnotation(PT_AttrPropIndex api);

    void _openFrame(PT_AttrPropIndex api);
    void _closeFrame();

    void _openTOC(PT_AttrPropIndex api);
    void _closeTOC();

    void _openBookmark(PT_AttrPropIndex api);
    void _closeBookmark(PT_AttrPropIndex api);
    void _closeBookmark(UT_UTF8String &sBookmarkName);

    void _openHyperlink(PT_AttrPropIndex api);
    void _closeHyperlink();

    void _openRDFAnchor(PT_AttrPropIndex api);
    void _closeRDFAnchor(PT_AttrPropIndex api);

    void _insertInlinedImage(PT_AttrPropIndex api);

    void _insertEmbeddedImage(PT_AttrPropIndex api);

    void _insertMath(PT_AttrPropIndex api);

    // Tracked-change export: piece-table revision marks become
    // <text:change-start>/<text:change>/<text:change-end> marks in the
    // content stream plus <text:changed-region> records held in
    // m_pAuxData.  Deleted content is captured into the region's
    // payload rather than written to the live text.
    void _updateChangeScopes(PT_AttrPropIndex api);
    void _pushChangeScope(UT_uint8 type, UT_uint32 revId);
    void _popChangeScope();
    void _closeChangeScopes();
    void _restoreChangeCapture();
    UT_uint32 _mintChangeRegion(UT_uint8 type, UT_uint32 revId);

    void _outputData(const UT_UCS4Char* pData, UT_uint32 length);
    void _appendSpaces(UT_UTF8String* sBuf, UT_uint32 count);

    const gchar* _getObjectKey(const PT_AttrPropIndex& api,
                                  const gchar* key);

    void _handleListenerImplAction();

    ////
    // Abi processing vars

    fd_Field* m_pCurrentField;
    UT_UTF8String m_currentFieldType;
    UT_UTF8String m_bookmarkName;
    PT_AttrPropIndex    m_apiLastSpan;

    bool m_bInSpan;
    bool m_bInBlock;
    bool m_bInBookmark;
    bool m_bInHyperlink;
    bool m_bInSection;
    bool m_bInAnnotation;
    bool m_bPendingAnnotationEnd;
    std::string m_currentAnnotationName;

    UT_sint32 m_iInTable;
    UT_sint32 m_iInCell;

    PD_Document* m_pDocument;

    ////
    // Listener implementation vars

    class StackCell {
    public:
        StackCell() {m_pListenerImpl=nullptr; m_deleteWhenPop=false;}
        StackCell(ODe_AbiDocListenerImpl* pListenerImpl, bool deleteWhenPop) {
            m_deleteWhenPop = deleteWhenPop;
            m_pListenerImpl = pListenerImpl;
        }
        // Work around the "return 0" issue of the UT_GenericVector::getNhItem()
        StackCell(UT_uint32 /*i*/)
			{
				m_pListenerImpl=nullptr;
				m_deleteWhenPop=false;
			}
        StackCell(const StackCell&) = default;
        StackCell& operator=(const StackCell& sc) {
            this->m_deleteWhenPop = sc.m_deleteWhenPop;
            this->m_pListenerImpl = sc.m_pListenerImpl;

            return *this;
        }

        bool m_deleteWhenPop;
        ODe_AbiDocListenerImpl* m_pListenerImpl;
    };

    std::vector<ODe_AbiDocListener::StackCell> m_implStack;

    ODe_AbiDocListenerImpl* m_pCurrentImpl;
    bool m_deleteCurrentWhenPop;

    ODe_ListenerAction m_listenerImplAction;

    ////
    // Tracked-change export state (active only when m_pAuxData set,
    // i.e. during the main content pass).

    class _RevToken {
    public:
        _RevToken() : type(0), revId(0) {}
        _RevToken(UT_uint8 t, UT_uint32 id) : type(t), revId(id) {}
        UT_uint8  type;
        UT_uint32 revId;
        bool operator==(const _RevToken& o) const {
            return type == o.type && revId == o.revId;
        }
    };

    class _ChangeScope {
    public:
        _ChangeScope() : type(0), revId(0), region(0), capture(nullptr) {}
        _ChangeScope(UT_uint8 t, UT_uint32 id, UT_uint32 r,
                     GsfOutput* c) : type(t), revId(id), region(r),
                                     capture(c) {}
        UT_uint8   type;
        UT_uint32  revId;
        UT_uint32  region;     // index into m_pAuxData->m_changeRegions
        GsfOutput* capture;    // payload stream for deletion scopes
    };

    ODe_AuxiliaryData* m_pAuxData;
    std::vector<_ChangeScope> m_openChangeScopes;
    std::vector<_RevToken> m_blockRevisions;
    // (type << 32) | revId -> region index, for insertion/format scopes
    // which may legitimately mark several disjoint ranges.
    std::map<UT_uint64, UT_uint32> m_changeScopeRegions;
    UT_UTF8String m_pendingParaMarkChange;
    PT_AttrPropIndex m_apiBlockRevisionsParsed;
};
