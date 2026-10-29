/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
/* Abinova
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

#include <stdlib.h>

#include "ut_string.h"
#include "ut_string_class.h"
#include "ut_units.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"

// This header defines some functions for Unix dialogs,
// like centering them, measuring them, etc.
#include "xap_UnixDialogHelper.h"
#include "xap_Dlg_MessageBox.h"

#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"
#include "ap_Dialog_Tabs.h"
#include "ap_UnixDialog_Tabs.h"

/*****************************************************************/

XAP_Dialog * AP_UnixDialog_Tabs::static_constructor(XAP_DialogFactory * pFactory,
												  XAP_Dialog_Id id)
{
	return new AP_UnixDialog_Tabs(pFactory,id);
}

AP_UnixDialog_Tabs::AP_UnixDialog_Tabs(XAP_DialogFactory * pDlgFactory,
									   XAP_Dialog_Id id)
	: AP_Dialog_Tabs(pDlgFactory, id),
	  m_windowMain(nullptr),
	  m_wPosEntry(nullptr),
	  m_wStopList(nullptr),
	  m_wDefaultEntry(nullptr),
	  m_wSet(nullptr),
	  m_wClear(nullptr),
	  m_wClearAll(nullptr)
{
	for (GtkWidget *& w : m_wAlign)  w = nullptr;
	for (GtkWidget *& w : m_wLeader) w = nullptr;
}

AP_UnixDialog_Tabs::~AP_UnixDialog_Tabs(void)
{
}

void AP_UnixDialog_Tabs::runModal(XAP_Frame * pFrame)
{
	// Build the dialog's window
	m_windowMain = _constructWindow();
	UT_return_if_fail(m_windowMain);

	// Populate the window's data items
	_populateWindowData();

	switch ( abiRunModalDialog ( GTK_DIALOG(m_windowMain),
								 pFrame, this, BUTTON_OK, false ) )
	{
		case BUTTON_OK:
			m_answer = AP_Dialog_Tabs::a_OK;
			break;
		default:
			m_answer = AP_Dialog_Tabs::a_CANCEL;
			break;
	}

	_storeWindowData();

	abiDestroyWidget ( m_windowMain ) ;
}

/*****************************************************************/

static void s_set_clicked(GtkButton *, AP_UnixDialog_Tabs * me)
{
	me->_onSet();
}

static void s_clear_clicked(GtkButton *, AP_UnixDialog_Tabs * me)
{
	me->_onClear();
}

static void s_clearall_clicked(GtkButton *, AP_UnixDialog_Tabs * me)
{
	me->_onClearAll();
}

static void s_row_selected(GtkListBox *, GtkListBoxRow *, AP_UnixDialog_Tabs * me)
{
	me->_onStopSelected();
}

/*****************************************************************/

static eTabType s_indexToType(gssize i)
{
	static const eTabType types[5] = {
		FL_TAB_LEFT, FL_TAB_CENTER, FL_TAB_RIGHT,
		FL_TAB_DECIMAL, FL_TAB_BAR
	};
	if (i < 0 || i > 4)
		return FL_TAB_LEFT;
	return types[i];
}

static eTabLeader s_indexToLeader(gssize i)
{
	static const eTabLeader leaders[4] = {
		FL_LEADER_NONE, FL_LEADER_DOT, FL_LEADER_HYPHEN, FL_LEADER_UNDERLINE
	};
	if (i < 0 || i > 3)
		return FL_LEADER_NONE;
	return leaders[i];
}

eTabType AP_UnixDialog_Tabs::_selectedType(void) const
{
	for (gssize i = 0; i < 5; i++)
	{
		if (m_wAlign[i] &&
			gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wAlign[i])))
		{
			return s_indexToType(i);
		}
	}
	return FL_TAB_LEFT;
}

eTabLeader AP_UnixDialog_Tabs::_selectedLeader(void) const
{
	for (gssize i = 0; i < 4; i++)
	{
		if (m_wLeader[i] &&
			gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wLeader[i])))
		{
			return s_indexToLeader(i);
		}
	}
	return FL_LEADER_NONE;
}

bool AP_UnixDialog_Tabs::_parsePosition(UT_sint32 & iPosition) const
{
	const gchar * sz = XAP_gtk_entry_get_text(GTK_EDITABLE(m_wPosEntry));
	if (!sz || !*sz)
		return false;
	/* reject junk outright: a number with no dimension, and anything
	 * UT_convertToLogicalUnits cannot digest into a sane margin */
	const double dInches = UT_convertToInches(sz);
	if (!isfinite(dInches) || dInches < 0.0)
		return false;
	/* UT_convertToInches(atof) returns 0 both for "0in" and for total
	 * garbage — demand at least one digit so garbage fails here */
	bool bDigit = false;
	for (const gchar * p = sz; *p; ++p)
	{
		if (g_ascii_isdigit(*p)) { bDigit = true; break; }
	}
	if (!bDigit)
		return false;
	iPosition = UT_convertToLogicalUnits(sz);
	return true;
}

void AP_UnixDialog_Tabs::_rebuildStopList(void)
{
	GtkWidget * child = gtk_widget_get_first_child(m_wStopList);
	while (child)
	{
		GtkWidget * next = gtk_widget_get_next_sibling(child);
		gtk_list_box_remove(GTK_LIST_BOX(m_wStopList), child);
		child = next;
	}

	const XAP_StringSet * pSS = m_pApp->getStringSet();
	static const XAP_String_Id typeNames[5] = {
		AP_STRING_ID_TabToggleLeftTab, AP_STRING_ID_TabToggleCenterTab,
		AP_STRING_ID_TabToggleRightTab, AP_STRING_ID_TabToggleDecimalTab,
		AP_STRING_ID_TabToggleBarTab
	};

	for (const Stop & s : m_vecStops)
	{
		std::string typeName;
		int idx = 0;
		switch (s.iType)
		{
		case FL_TAB_CENTER:  idx = 1; break;
		case FL_TAB_RIGHT:   idx = 2; break;
		case FL_TAB_DECIMAL: idx = 3; break;
		case FL_TAB_BAR:     idx = 4; break;
		default:             idx = 0; break;
		}
		pSS->getValueUTF8(typeNames[idx], typeName);

		std::string label = formatPosition(s.iPosition);
		label += "  ";
		label += typeName;
		GtkWidget * rowLabel = gtk_label_new(label.c_str());
		gtk_label_set_xalign(GTK_LABEL(rowLabel), 0.0f);
		gtk_widget_set_margin_start(rowLabel, 6);
		gtk_list_box_append(GTK_LIST_BOX(m_wStopList), rowLabel);
	}
	gtk_widget_set_sensitive(m_wClear, !m_vecStops.empty());
	gtk_widget_set_sensitive(m_wClearAll, !m_vecStops.empty());
}

void AP_UnixDialog_Tabs::_onStopSelected(void)
{
	GtkListBoxRow * row = gtk_list_box_get_selected_row(GTK_LIST_BOX(m_wStopList));
	if (!row)
		return;
	const int idx = gtk_list_box_row_get_index(row);
	if (idx < 0 || static_cast<size_t>(idx) >= m_vecStops.size())
		return;
	const Stop & s = m_vecStops[static_cast<size_t>(idx)];

	gtk_editable_set_text(GTK_EDITABLE(m_wPosEntry),
						  formatPosition(s.iPosition).c_str());

	static const eTabType order[5] = {
		FL_TAB_LEFT, FL_TAB_CENTER, FL_TAB_RIGHT,
		FL_TAB_DECIMAL, FL_TAB_BAR
	};
	for (int i = 0; i < 5; i++)
	{
		if (order[i] == s.iType && m_wAlign[i])
		{
			gtk_check_button_set_active(GTK_CHECK_BUTTON(m_wAlign[i]), TRUE);
			break;
		}
	}
	static const eTabLeader lorder[4] = {
		FL_LEADER_NONE, FL_LEADER_DOT, FL_LEADER_HYPHEN, FL_LEADER_UNDERLINE
	};
	for (int i = 0; i < 4; i++)
	{
		if (lorder[i] == s.iLeader && m_wLeader[i])
		{
			gtk_check_button_set_active(GTK_CHECK_BUTTON(m_wLeader[i]), TRUE);
			break;
		}
	}
}

void AP_UnixDialog_Tabs::_onSet(void)
{
	UT_sint32 iPosition = 0;
	if (!_parsePosition(iPosition))
	{
		XAP_Frame * pFrame = XAP_App::getApp()->getLastFocussedFrame();
		if (pFrame)
		{
			pFrame->showMessageBox(AP_STRING_ID_DLG_Tabs_InvalidPos,
								   XAP_Dialog_MessageBox::b_O,
								   XAP_Dialog_MessageBox::a_OK);
		}
		return;
	}
	setStop(iPosition, _selectedType(), _selectedLeader());
	_rebuildStopList();

	/* select the row the set just landed on so Clear applies to it */
	for (size_t i = 0; i < m_vecStops.size(); i++)
	{
		if (m_vecStops[i].iPosition == iPosition)
		{
			GtkListBoxRow * row = gtk_list_box_get_row_at_index(
				GTK_LIST_BOX(m_wStopList), static_cast<int>(i));
			if (row)
				gtk_list_box_select_row(GTK_LIST_BOX(m_wStopList), row);
			break;
		}
	}
}

void AP_UnixDialog_Tabs::_onClear(void)
{
	GtkListBoxRow * row = gtk_list_box_get_selected_row(GTK_LIST_BOX(m_wStopList));
	if (!row)
		return;
	clearStop(gtk_list_box_row_get_index(row));
	_rebuildStopList();
}

void AP_UnixDialog_Tabs::_onClearAll(void)
{
	clearAllStops();
	_rebuildStopList();
}

GtkWidget * AP_UnixDialog_Tabs::_constructWindow(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();

	GtkBuilder * builder = newDialogBuilderFromResource("ap_UnixDialog_Tabs.ui");
	GtkWidget * window =
		GTK_WIDGET(gtk_builder_get_object(builder, "ap_UnixDialog_Tabs"));
	m_wPosEntry = GTK_WIDGET(gtk_builder_get_object(builder, "enPos"));
	m_wStopList = GTK_WIDGET(gtk_builder_get_object(builder, "lstStops"));
	m_wAlign[0] = GTK_WIDGET(gtk_builder_get_object(builder, "rbAlignLeft"));
	m_wAlign[1] = GTK_WIDGET(gtk_builder_get_object(builder, "rbAlignCenter"));
	m_wAlign[2] = GTK_WIDGET(gtk_builder_get_object(builder, "rbAlignRight"));
	m_wAlign[3] = GTK_WIDGET(gtk_builder_get_object(builder, "rbAlignDecimal"));
	m_wAlign[4] = GTK_WIDGET(gtk_builder_get_object(builder, "rbAlignBar"));
	m_wLeader[0] = GTK_WIDGET(gtk_builder_get_object(builder, "rbLeaderNone"));
	m_wLeader[1] = GTK_WIDGET(gtk_builder_get_object(builder, "rbLeaderDots"));
	m_wLeader[2] = GTK_WIDGET(gtk_builder_get_object(builder, "rbLeaderHyphens"));
	m_wLeader[3] = GTK_WIDGET(gtk_builder_get_object(builder, "rbLeaderUnderline"));
	m_wDefaultEntry = GTK_WIDGET(gtk_builder_get_object(builder, "enDefault"));
	m_wSet = GTK_WIDGET(gtk_builder_get_object(builder, "btSet"));
	m_wClear = GTK_WIDGET(gtk_builder_get_object(builder, "btClear"));
	m_wClearAll = GTK_WIDGET(gtk_builder_get_object(builder, "btClearAll"));

	// set the dialog title
	std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_Tabs_Title, s);
	abiDialogSetTitle(window, "%s", s.c_str());

	// localize the strings in our dialog
	localizeLabelUnderline(GTK_WIDGET(gtk_builder_get_object(builder, "lbPos")),
						   pSS, AP_STRING_ID_DLG_Tabs_Position);
	localizeLabelUnderline(GTK_WIDGET(gtk_builder_get_object(builder, "lbDefault")),
						   pSS, AP_STRING_ID_DLG_Tabs_Default);
	localizeButtonUnderline(m_wSet, pSS, AP_STRING_ID_DLG_Tabs_Set);
	localizeButtonUnderline(m_wClear, pSS, AP_STRING_ID_DLG_Tabs_Clear);
	localizeButtonUnderline(m_wClearAll, pSS, AP_STRING_ID_DLG_Tabs_ClearAll);

	static const XAP_String_Id alignIds[5] = {
		AP_STRING_ID_DLG_Tabs_AlignLeft, AP_STRING_ID_DLG_Tabs_AlignCenter,
		AP_STRING_ID_DLG_Tabs_AlignRight, AP_STRING_ID_DLG_Tabs_AlignDecimal,
		AP_STRING_ID_DLG_Tabs_AlignBar
	};
	for (int i = 0; i < 5; i++)
		localizeButtonUnderline(m_wAlign[i], pSS, alignIds[i]);

	static const XAP_String_Id leaderIds[4] = {
		AP_STRING_ID_DLG_Tabs_LeaderNone, AP_STRING_ID_DLG_Tabs_LeaderDots,
		AP_STRING_ID_DLG_Tabs_LeaderHyphens, AP_STRING_ID_DLG_Tabs_LeaderUnderline
	};
	for (int i = 0; i < 4; i++)
		localizeButtonUnderline(m_wLeader[i], pSS, leaderIds[i]);

	gtk_frame_set_label(GTK_FRAME(gtk_builder_get_object(builder, "frAlign")),
						nullptr);
	{
		std::string grp;
		pSS->getValueUTF8(AP_STRING_ID_DLG_Tabs_Alignment, grp);
		gtk_frame_set_label(GTK_FRAME(gtk_builder_get_object(builder, "frAlign")),
							grp.c_str());
		pSS->getValueUTF8(AP_STRING_ID_DLG_Tabs_Leader, grp);
		gtk_frame_set_label(GTK_FRAME(gtk_builder_get_object(builder, "frLeader")),
							grp.c_str());
	}

	localizeButtonUnderline(GTK_WIDGET(gtk_builder_get_object(builder, "btOK")),
							pSS, XAP_STRING_ID_DLG_OK);
	localizeButtonUnderline(GTK_WIDGET(gtk_builder_get_object(builder, "btCancel")),
							pSS, XAP_STRING_ID_DLG_Cancel);

	g_signal_connect(G_OBJECT(m_wSet), "clicked",
					 G_CALLBACK(s_set_clicked), this);
	g_signal_connect(G_OBJECT(m_wClear), "clicked",
					 G_CALLBACK(s_clear_clicked), this);
	g_signal_connect(G_OBJECT(m_wClearAll), "clicked",
					 G_CALLBACK(s_clearall_clicked), this);
	g_signal_connect(G_OBJECT(m_wStopList), "row-selected",
					 G_CALLBACK(s_row_selected), this);

	g_object_unref(G_OBJECT(builder));

	return window;
}

void AP_UnixDialog_Tabs::_populateWindowData(void)
{
	_rebuildStopList();
	gtk_editable_set_text(GTK_EDITABLE(m_wDefaultEntry),
						  m_sDefaultInterval.c_str());
}

void AP_UnixDialog_Tabs::_storeWindowData(void)
{
	const gchar * sz = XAP_gtk_entry_get_text(GTK_EDITABLE(m_wDefaultEntry));
	if (sz && *sz)
	{
		m_sDefaultInterval = sz;
	}
}
