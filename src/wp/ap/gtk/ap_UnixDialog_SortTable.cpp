/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* Abinova
 * Copyright (C) 2026 Abinova contributors
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
#include "ut_assert.h"
#include "ut_debugmsg.h"

// This header defines some functions for Unix dialogs,
// like centering them, measuring them, etc.
#include "xap_UnixDialogHelper.h"

#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"
#include "ap_Dialog_SortTable.h"
#include "ap_UnixDialog_SortTable.h"

/*****************************************************************/

XAP_Dialog * AP_UnixDialog_SortTable::static_constructor(XAP_DialogFactory * pFactory,
													   XAP_Dialog_Id id)
{
	return new AP_UnixDialog_SortTable(pFactory,id);
}

AP_UnixDialog_SortTable::AP_UnixDialog_SortTable(XAP_DialogFactory * pDlgFactory,
											   XAP_Dialog_Id id)
	: AP_Dialog_SortTable(pDlgFactory, id),
	  m_windowMain(nullptr),
	  m_wColumn(nullptr),
	  m_wTypeText(nullptr),
	  m_wTypeNumber(nullptr),
	  m_wTypeDate(nullptr),
	  m_wAsc(nullptr),
	  m_wDesc(nullptr),
	  m_wHeader(nullptr)
{
}

AP_UnixDialog_SortTable::~AP_UnixDialog_SortTable(void)
{
}

void AP_UnixDialog_SortTable::runModal(XAP_Frame * pFrame)
{
	// Build the dialog's window
	m_windowMain = _constructWindow();
	UT_return_if_fail(m_windowMain);

	// Populate the window's data items
	_populateWindowData();

	switch ( abiRunModalDialog ( GTK_DIALOG(m_windowMain),
								 pFrame, this, BUTTON_SORT, false ) )
	{
		case BUTTON_SORT:
			m_answer = AP_Dialog_SortTable::a_OK;
			break;
		default:
			m_answer = AP_Dialog_SortTable::a_CANCEL;
			break;
	}

	_storeWindowData();

	abiDestroyWidget ( m_windowMain ) ;
}

/*****************************************************************/

GtkWidget * AP_UnixDialog_SortTable::_constructWindow(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();

	GtkBuilder * builder = newDialogBuilderFromResource("ap_UnixDialog_SortTable.ui");
	GtkWidget * window =
		GTK_WIDGET(gtk_builder_get_object(builder, "ap_UnixDialog_SortTable"));
	m_wColumn = GTK_WIDGET(gtk_builder_get_object(builder, "sbColumn"));
	m_wTypeText = GTK_WIDGET(gtk_builder_get_object(builder, "rbTypeText"));
	m_wTypeNumber = GTK_WIDGET(gtk_builder_get_object(builder, "rbTypeNumber"));
	m_wTypeDate = GTK_WIDGET(gtk_builder_get_object(builder, "rbTypeDate"));
	m_wAsc = GTK_WIDGET(gtk_builder_get_object(builder, "rbAsc"));
	m_wDesc = GTK_WIDGET(gtk_builder_get_object(builder, "rbDesc"));
	m_wHeader = GTK_WIDGET(gtk_builder_get_object(builder, "cbHeader"));

	// set the dialog title
	std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_SortTable_Title, s);
	abiDialogSetTitle(window, "%s", s.c_str());

	// localize the strings in our dialog
	localizeLabel(GTK_WIDGET(gtk_builder_get_object(builder, "lbColumn")),
				  pSS, AP_STRING_ID_DLG_SortTable_SortBy);
	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbType")),
						pSS, AP_STRING_ID_DLG_SortTable_Type);
	localizeButton(m_wTypeText, pSS, AP_STRING_ID_DLG_SortTable_Text);
	localizeButton(m_wTypeNumber, pSS, AP_STRING_ID_DLG_SortTable_Number);
	localizeButton(m_wTypeDate, pSS, AP_STRING_ID_DLG_SortTable_Date);
	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbOrder")),
						pSS, AP_STRING_ID_DLG_SortTable_Order);
	localizeButton(m_wAsc, pSS, AP_STRING_ID_DLG_SortTable_Ascending);
	localizeButton(m_wDesc, pSS, AP_STRING_ID_DLG_SortTable_Descending);
	localizeButton(m_wHeader, pSS, AP_STRING_ID_DLG_SortTable_Header);
	localizeButtonUnderline(GTK_WIDGET(gtk_builder_get_object(builder, "btSort")),
							pSS, AP_STRING_ID_DLG_SortTable_Sort);

	g_object_unref(G_OBJECT(builder));

	return window;
}

void AP_UnixDialog_SortTable::_populateWindowData(void)
{
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(m_wColumn),
							  1.0, static_cast<double>(m_numCols));
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_wColumn),
							  static_cast<double>(m_column));
	switch (m_type)
	{
	case sort_NUMBER:
		gtk_check_button_set_active(GTK_CHECK_BUTTON(m_wTypeNumber), TRUE);
		break;
	case sort_DATE:
		gtk_check_button_set_active(GTK_CHECK_BUTTON(m_wTypeDate), TRUE);
		break;
	default:
		gtk_check_button_set_active(GTK_CHECK_BUTTON(m_wTypeText), TRUE);
		break;
	}
	gtk_check_button_set_active(
		GTK_CHECK_BUTTON(m_bAsc ? m_wAsc : m_wDesc), TRUE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_wHeader), m_bHeader);
}

void AP_UnixDialog_SortTable::_storeWindowData(void)
{
	m_column = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(m_wColumn));
	m_bAsc = gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wAsc));
	m_bHeader = gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wHeader));
	if (gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wTypeNumber)))
	{
		m_type = sort_NUMBER;
	}
	else if (gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wTypeDate)))
	{
		m_type = sort_DATE;
	}
	else
	{
		m_type = sort_TEXT;
	}
}
