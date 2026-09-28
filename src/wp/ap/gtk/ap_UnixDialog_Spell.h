/* Abinova — spelling dialog (GTK4)
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

#pragma once

#include "ap_Dialog_Spell.h"


class XAP_Frame;


class AP_UnixDialog_Spell : public AP_Dialog_Spell
{
public:

	AP_UnixDialog_Spell (XAP_DialogFactory * pDlgFactory,
						 XAP_Dialog_Id id);
	virtual ~AP_UnixDialog_Spell (void);

	static XAP_Dialog *	static_constructor (XAP_DialogFactory *, XAP_Dialog_Id id);

	virtual void runModal(XAP_Frame * pFrame) override;

	// callbacks can fire these events
	void onChangeClicked	  (void);
	void onChangeAllClicked	  (void);
	void onIgnoreClicked	  (void);
	void onIgnoreAllClicked	  (void);
	void onAddClicked		  (void);
	void onSuggestionSelected (void);
	void onSuggestionChanged  (void);
	void onSuggestionActivated(void);

	GtkWidget * getWindow (void) const { return m_wDialog; }

private:

	GtkWidget * _constructWindow	   (void);
	void 			   _updateWindow 	   (void);

	char 	  * _convertToMB   (const UT_UCS4Char *wword);
	char 	  * _convertToMB   (const UT_UCS4Char *wword,
								UT_sint32 iLength);
	UT_UCS4Char * _convertFromMB (const char *word);

	// pointers to widgets we need to query/set
	GtkWidget * m_wDialog;
	GtkWidget * m_txWrong;
	GtkWidget * m_eChange;
	GtkWidget * m_lbSuggestions;	/* GtkListBox */

	GtkTextTag * m_pMisspellTag;
	GtkTextTag * m_pBoldTag;

	gulong m_changeHandlerID;
	gulong m_selectHandlerID;

	/* suppress entry/selection feedback while repopulating widgets */
	bool   m_bUiUpdating;
};
