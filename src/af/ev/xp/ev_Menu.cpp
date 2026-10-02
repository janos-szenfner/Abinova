/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:t -*- */
/* AbiSource Program Utilities
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
 



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ut_std_string.h"
#include "ut_misc.h"
#include "ev_Menu.h"
#include "ev_EditMethod.h"
#include "ev_EditBinding.h"
#include "ev_EditEventMapper.h"
#include "ev_Menu_Actions.h"
#include "ev_Menu_Labels.h"
#include "xap_Menu_Layouts.h"
#include "xap_App.h"
#include "xap_Frame.h"


/*****************************************************************/

EV_Menu::EV_Menu(XAP_App* pApp,
				 EV_EditMethodContainer * pEMC,
				 const char * szMenuLayoutName,
				 const char * szMenuLabelSetName)
	: m_pEMC(pEMC),
	  m_pApp(pApp)
{
	//UT_DEBUGMSG(("EV_Menu: Creating menu for [layout %s, language %s]\n",
	//			 szMenuLayoutName,szMenuLabelSetName));
	
	m_pMenuLayout = m_pApp->getMenuFactory()->CreateMenuLayout(szMenuLayoutName);
	UT_ASSERT(m_pMenuLayout);

	m_pMenuLabelSet = m_pApp->getMenuFactory()->CreateMenuLabelSet(szMenuLabelSetName);
	UT_ASSERT(m_pMenuLabelSet);

}

EV_Menu::~EV_Menu()
{
	DELETEP(m_pMenuLayout);
	DELETEP(m_pMenuLabelSet);
}

bool EV_Menu::invokeMenuMethod(AV_View * pView,
							   EV_EditMethod * pEM,
							   UT_UCS4Char * pData,
							   UT_uint32 dataLength) const
{
	UT_ASSERT(pView);
	UT_return_val_if_fail(pEM, false);

	//UT_DEBUGMSG(("invokeMenuMethod: %s\n",pEM->getName()));

	EV_EditMethodType t = pEM->getType();

	if (((t & EV_EMT_REQUIREDATA) != 0) && (!pData || !dataLength))
	{
		// This method requires character data and the caller did not provide any.
		UT_DEBUGMSG(("    invoke aborted due to lack of data\n"));
		return false;
	}

	EV_EditMethodCallData emcd(pData,dataLength);
	pEM->Fn(pView,&emcd);

	return true;
}

bool EV_Menu::invokeMenuMethod(AV_View * pView,
							   EV_EditMethod * pEM,
							   const UT_String& stScriptName) const
{
	UT_return_val_if_fail(pEM,false);
	EV_EditMethodType t = pEM->getType();
	if (!(t & EV_EMT_APP_METHOD)) {
		UT_ASSERT(pView);
	}
	if ((t & EV_EMT_REQUIREDATA) && stScriptName.size() == 0)
	{
		UT_DEBUGMSG(("    invoke aborted due to lack of script name\n"));
		return false;
	}

	EV_EditMethodCallData emcd(stScriptName);
	pEM->Fn(pView, &emcd);

	return true;
}

/* replace _ev_GetLabelName () */
/* this version taken from ev_UnixMenu.cpp */
const char ** EV_Menu::getLabelName(XAP_App * pApp, 
									const EV_Menu_Action * pAction, const EV_Menu_Label * pLabel) const
{
	static const char * data[2] = {nullptr, nullptr};

	UT_return_val_if_fail( pAction && pLabel, nullptr );
	
	// hit the static pointers back to null each time around
	data[0] = nullptr;
	data[1] = nullptr;
	
	const char * szLabelName;
	
	if (pAction->hasDynamicLabel())
		szLabelName = pAction->getDynamicLabel(pLabel);
	else
		szLabelName = pLabel->getMenuLabel();

	if (!szLabelName || !*szLabelName)
		return data;	// which will be two nulls now

	static char accelbuf[32];
	{
		// see if this has an associated keybinding
		const char * szMethodName = pAction->getMethodName();

		if (szMethodName)
		{
			const EV_EditMethodContainer * pEMC = pApp->getEditMethodContainer();
			UT_return_val_if_fail(pEMC, nullptr);

			EV_EditMethod * pEM = pEMC->findEditMethodByName(szMethodName);
			if(!pEM)
			{
			    UT_DEBUGMSG(("Cannot find EV_Editmethod for %s \n",szMethodName));
			}
			
			// make sure it's bound to something

			UT_return_val_if_fail(pEM, nullptr);

			const EV_EditEventMapper * pEEM = getApp()->getEditEventMapper();
			UT_return_val_if_fail(pEEM, nullptr);

			const char * string = pEEM->getShortcutFor(pEM);
			if (string && *string)
				snprintf(accelbuf, sizeof(accelbuf), "%s", string);
			else
				// zero it out for this round
				*accelbuf = 0;
		}
	}

	// set shortcut mnemonic, if any
	if (*accelbuf) {
		xxx_UT_DEBUGMSG(("found accelerator %s\n", accelbuf));
		data[1] = accelbuf;
	}

	if (!pAction->raisesDialog())
	{
		data[0] = szLabelName;
		return data;
	}

	// append "..." to menu item if it raises a dialog
	static char buf[128];
	memset(buf,0,G_N_ELEMENTS(buf));
	strncpy(buf,szLabelName,G_N_ELEMENTS(buf)-4);
	strcat(buf,"...");

	data[0] = buf;

	return data;
}
