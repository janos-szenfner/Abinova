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

#include "ap_Features.h"

#include "ut_types.h"
#include "ut_string.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"

#include "ev_Menu_Labels.h"
#include "ev_Menu.h"

#include "xap_App.h"
#include "xap_Menu_Layouts.h"
#include "xap_Menu_ActionSet.h"
#include "xap_EncodingManager.h"

#include "ap_Menu_Id.h"
#include "ap_Strings.h"

	struct _lt
	{
		EV_Menu_LayoutFlags			m_flags;
		XAP_Menu_Id					m_id;
	};

	struct _tt
	{
		const char *				m_name;
		UT_uint32					m_nrEntries;
		struct _lt *				m_lt;
		EV_EditMouseContext			m_emc;
	};

	class ABI_EXPORT _vectt
	{
	public:
		_vectt(_tt * orig):
			m_Vec_lt(orig->m_nrEntries, 4, true)
			{
				m_name = orig->m_name;
		        m_emc = orig->m_emc;
				m_Vec_lt.clear();
				UT_uint32 k = 0;
				for(k = 0; k < orig->m_nrEntries; k++)
				{
					_lt * plt = new _lt;
					*plt = orig->m_lt[k];
					m_Vec_lt.addItem(plt);
				}
			};
		~_vectt()
			{
				UT_VECTOR_PURGEALL(_lt *,m_Vec_lt);
			};
		UT_uint32 getNrEntries(void)
			{
				return m_Vec_lt.getItemCount();
			};
		_lt * getNth_lt(UT_uint32 n)
			{
				return m_Vec_lt.getNthItem(n);
			};
		const char *				m_name;
		EV_EditMouseContext			m_emc;
	private:
		UT_GenericVector<_lt*>		m_Vec_lt;
	};


/*****************************************************************
******************************************************************
** Here we begin a little CPP magic to load the layout for each
** menu layout in the application.  It is important that all of
** the ...Layout_*.h files allow themselves to be included more
** than one time.
******************************************************************
*****************************************************************/

#define BeginLayout(Name,Cxt)	static struct _lt s_ltTable_##Name[] = {
#define MenuItem(id)			{ EV_MLF_Normal,		static_cast<XAP_Menu_Id>((id)				 )},
#define BeginSubMenu(id)		{ EV_MLF_BeginSubMenu,	static_cast<XAP_Menu_Id>((id)				 )},
#define BeginPopupMenu()		{ EV_MLF_BeginPopupMenu,static_cast<XAP_Menu_Id>(AP_MENU_ID__BOGUS1__ )},
#define Separator()				{ EV_MLF_Separator,		static_cast<XAP_Menu_Id>(AP_MENU_ID__BOGUS1__ )},
#define EndSubMenu()			{ EV_MLF_EndSubMenu,	static_cast<XAP_Menu_Id>(AP_MENU_ID__BOGUS1__ )},
#define EndPopupMenu()			{ EV_MLF_EndPopupMenu,	static_cast<XAP_Menu_Id>(AP_MENU_ID__BOGUS1__ )},
#define EndLayout()				};

#include "ap_Menu_Layouts_All.h"

#undef BeginLayout
#undef MenuItem
#undef BeginSubMenu
#undef BeginPopupMenu
#undef Separator
#undef EndSubMenu
#undef EndPopupMenu
#undef EndLayout


/*****************************************************************
******************************************************************
** Here we begin a little CPP magic to construct a table containing
** the names and addresses of all the tables we constructed in the
** previous section.
******************************************************************
*****************************************************************/

#define BeginLayout(Name,Cxt)	{ #Name, G_N_ELEMENTS(s_ltTable_##Name), s_ltTable_##Name, Cxt },
#define MenuItem(id)			/*nothing*/
#define BeginSubMenu(id)		/*nothing*/
#define BeginPopupMenu()		/*nothing*/
#define Separator()				/*nothing*/
#define EndSubMenu()			/*nothing*/
#define EndPopupMenu()			/*nothing*/
#define EndLayout()				/*nothing*/

static struct _tt s_ttTable[] =
{

#include "ap_Menu_Layouts_All.h"
	
};

#undef BeginLayout
#undef MenuItem
#undef BeginSubMenu
#undef BeginPopupMenu
#undef Separator
#undef EndSubMenu
#undef EndPopupMenu
#undef EndLayout



/*****************************************************************
******************************************************************
** Put it all together and have a "load Layout by Name"
******************************************************************
*****************************************************************/


/*!
 * Load these cleverly constructed static tables into vectors so they can be 
 * manipulated dynamically.
 */
XAP_Menu_Factory::XAP_Menu_Factory(XAP_App * pApp) :
		m_pApp(pApp),
		m_pLabelSet(nullptr)
{
	UT_uint32 k = 0;
	m_vecTT.clear();
	for (k=0; k<G_N_ELEMENTS(s_ttTable); k++)
	{
		_vectt * pVectt = new _vectt(&s_ttTable[k]);
		m_vecTT.addItem(pVectt);
	}
}

XAP_Menu_Factory::~XAP_Menu_Factory()
{
    UT_VECTOR_SPARSEPURGEALL(_vectt *,m_vecTT);
	DELETEP(m_pLabelSet);
}

EV_Menu_Layout * XAP_Menu_Factory::CreateMenuLayout(const char * szName)
{
	UT_return_val_if_fail (szName && *szName, nullptr);		// no defaults

	for (UT_sint32 k=0; k< m_vecTT.getItemCount(); k++)
	{
		_vectt * pVectt = m_vecTT.getNthItem(k);
		if (pVectt == nullptr)
			continue;
		if (g_ascii_strcasecmp(szName,pVectt->m_name)==0)
		{
			UT_uint32 NrEntries = pVectt->getNrEntries();
			EV_Menu_Layout * pLayout = new EV_Menu_Layout(pVectt->m_name,NrEntries);
			UT_return_val_if_fail (pLayout, nullptr);
			
			for (UT_uint32 j=0; (j < NrEntries); j++)
			{
				_lt * plt = pVectt->getNth_lt(j);
				UT_DebugOnly<bool> bResult = pLayout->setLayoutItem(j, plt->m_id, plt->m_flags);
				UT_ASSERT_HARMLESS(bResult);
			}

			return pLayout;
		}
	}
	UT_ASSERT_HARMLESS(0);						// no defaults
	return nullptr;
}

const char * XAP_Menu_Factory::FindContextMenu(EV_EditMouseContext emc)
{

	for (UT_sint32 k=0; k< m_vecTT.getItemCount(); k++)
	{
		_vectt * pVectt = m_vecTT.getNthItem(k);
		if (pVectt == nullptr)
			continue;
		UT_DEBUGMSG(("Look menu %s id %x requested %x  \n",pVectt->m_name,pVectt->m_emc,emc));
		if (emc == pVectt->m_emc)
		{
			return pVectt->m_name;
		}
	}
	UT_ASSERT_HARMLESS(UT_NOT_IMPLEMENTED);
	return nullptr;
}

/*!
 * Build a label set in memory that can be cloned by frames.
 */
bool  XAP_Menu_Factory::buildMenuLabelSet(const char * szLanguage_)
{
	char buf[300];
	strncpy(buf,szLanguage_ ? szLanguage_ : "", sizeof(buf)-1);
	char* szLanguage = buf;

	char* dot = strrchr(szLanguage,'.');
	if (dot)
		*dot = '\0'; /* remove encoding part from locale name */

	UT_DEBUGMSG(("CreateMenuLabelSet: szLanguage_ %s, szLanguage %s\n"
				,szLanguage_,szLanguage));


	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
	if( !m_pLabelSet )
	{
		m_pLabelSet = new EV_Menu_LabelSet(szLanguage, static_cast<XAP_Menu_Id>(AP_MENU_ID__BOGUS1__), static_cast<XAP_Menu_Id>(AP_MENU_ID__BOGUS2__));
		std::string s1, s2;
		#define menuitem(id)                                                         \
		{                                                                            \
            pSS->getValueUTF8(AP_STRING_ID_MENU_LABEL_##id, s1);                     \
			pSS->getValueUTF8(AP_STRING_ID_MENU_STATUSLINE_##id, s2);                \
			m_pLabelSet->setLabel(static_cast<XAP_Menu_Id>((AP_MENU_ID_##id)), s1.c_str(), s2.c_str() ); \
	    }
		#include "ap_Menu_Id_List.h"
		#undef menuitem
		return true;
	}
	return false;
}


EV_Menu_LabelSet *  XAP_Menu_Factory::CreateMenuLabelSet(const char * szLanguage_)
{
	char buf[300];
	strncpy(buf,szLanguage_ ? szLanguage_ : "", sizeof(buf)-1);
	char* szLanguage = buf;

	char* dot = strrchr(szLanguage,'.');
	if (dot)
		*dot = '\0'; /* remove encoding part from locale name */

	UT_DEBUGMSG(("CreateMenuLabelSet: szLanguage_ %s, szLanguage %s\n"
				,szLanguage_,szLanguage));


	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();

	if( !m_pLabelSet )
	{
		m_pLabelSet = new EV_Menu_LabelSet(szLanguage, static_cast<XAP_Menu_Id>(AP_MENU_ID__BOGUS1__), static_cast<XAP_Menu_Id>(AP_MENU_ID__BOGUS2__));

		std::string s1, s2;
		#define menuitem(id)                                                          \
		{                                                                             \
		    pSS->getValueUTF8(AP_STRING_ID_MENU_LABEL_##id, s1);                      \
			pSS->getValueUTF8(AP_STRING_ID_MENU_STATUSLINE_##id, s2);                 \
			m_pLabelSet->setLabel( static_cast<XAP_Menu_Id>((AP_MENU_ID_##id)), s1.c_str(), s2.c_str() ); \
		}
			#include "ap_Menu_Id_List.h"
		#undef menuitem
	}

	return new EV_Menu_LabelSet(m_pLabelSet);
}
