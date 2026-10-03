/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
/* Abinova — find / replace dialog (GTK4)
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

#include <vector>
#include "xap_UnixDialog.h"
#include "ap_Dialog_Replace.h"

class XAP_UnixFrame;

/*****************************************************************/

class AP_UnixDialog_Replace
  : public AP_Dialog_Replace
  , public XAP_UnixDialog
{
public:
	AP_UnixDialog_Replace(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id);
	virtual ~AP_UnixDialog_Replace(void);


	virtual void			runModal(XAP_Frame * /*pFrame*/) override {};
	virtual void			runModeless(XAP_Frame * pFrame) override;
	virtual void			notifyActiveFrame(XAP_Frame *pFrame) override;
	virtual void			notifyCloseFrame(XAP_Frame * /*pFrame*/) override {};
	virtual void			destroy(void) override;
	virtual void			activate(void) override;

	static XAP_Dialog *		static_constructor(XAP_DialogFactory *, XAP_Dialog_Id id);

	// callbacks can fire these events
	void			event_FindNext(void);
	void			event_FindPrev(void);
	void			event_Replace(void);
	void			event_ReplaceAll(void);
	void			event_OptionsChanged(void);
	void			event_FindEntryChange(void);
	void			event_Cancel(void);

	enum ResponseId: int8_t
	  {
	    BUTTON_CANCEL = GTK_RESPONSE_CANCEL,
	    // enum GtkResponseType seems to only use negative integers, so we'll use positive ones to
	    // prevent potential conflicts (the cause of Bug 11583)
	    BUTTON_FIND_NEXT = 0,
	    BUTTON_FIND_PREV = 1,
	    BUTTON_REPLACE = 2,
	    BUTTON_REPLACE_ALL = 3
	  };

protected:

	virtual void			_updateLists() override;

private:

	// private construction functions
	GtkWidget * _constructWindow(void);
	void		_populateWindowData(void);
	void 		_storeWindowData(void);

	UT_UCS4String _entryText(GtkWidget * entry) const;
	void		_syncStringsFromWidgets(void);
	void		_updateSensitivity(void);
	void		_updateList(GtkListBox* history, GtkWidget * entry,
							std::vector<UT_UCS4Char*>* list);

	// pointers to widgets we need to query/set
	GtkWidget *	m_buttonFindNext;
	GtkWidget *	m_buttonFindPrev;
	GtkWidget *	m_buttonFindReplace;
	GtkWidget *	m_buttonReplaceAll;

	GtkWidget * m_entryFind;
	GtkWidget * m_entryReplace;
	GtkWidget * m_historyFind;		/* list box inside the history popover */
	GtkWidget * m_historyReplace;
	GtkWidget * m_menuBtnFind;
	GtkWidget * m_menuBtnReplace;

	GtkWidget * m_checkbuttonMatchCase;
	GtkWidget * m_checkbuttonWholeWord;
};
