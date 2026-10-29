/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t; -*- */
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
#include "ap_Dialog_TextToTable.h"
#include "ap_UnixDialog_TextToTable.h"

/*****************************************************************/

XAP_Dialog * AP_UnixDialog_TextToTable::static_constructor(XAP_DialogFactory * pFactory,
														 XAP_Dialog_Id id)
{
	return new AP_UnixDialog_TextToTable(pFactory,id);
}

AP_UnixDialog_TextToTable::AP_UnixDialog_TextToTable(XAP_DialogFactory * pDlgFactory,
												   XAP_Dialog_Id id)
	: AP_Dialog_TextToTable(pDlgFactory, id),
	  m_windowMain(nullptr),
	  m_wSepTabs(nullptr),
	  m_wSepCommas(nullptr),
	  m_wSepParas(nullptr),
	  m_wSepOther(nullptr),
	  m_wOtherEntry(nullptr),
	  m_wAutoCols(nullptr),
	  m_wNumCols(nullptr)
{
}

AP_UnixDialog_TextToTable::~AP_UnixDialog_TextToTable(void)
{
}

void AP_UnixDialog_TextToTable::runModal(XAP_Frame * pFrame)
{
	// Build the dialog's window
	m_windowMain = _constructWindow();
	UT_return_if_fail(m_windowMain);

	// Populate the window's data items
	_populateWindowData();

	switch ( abiRunModalDialog ( GTK_DIALOG(m_windowMain),
								 pFrame, this, BUTTON_CONVERT, false ) )
	{
		case BUTTON_CONVERT:
			m_answer = AP_Dialog_TextToTable::a_OK;
			break;
		default:
			m_answer = AP_Dialog_TextToTable::a_CANCEL;
			break;
	}

	_storeWindowData();

	abiDestroyWidget ( m_windowMain ) ;
}

/*****************************************************************/

// "Other" entry only editable while its radio is active; auto-detect is
// unavailable for "Paragraphs" (one paragraph per cell, row-major — the
// column count is what defines the grid there)
void AP_UnixDialog_TextToTable::_syncSensitivity(void)
{
	const bool bParas =
		gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wSepParas));
	const bool bOther =
		gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wSepOther));
	gtk_widget_set_sensitive(m_wOtherEntry, bOther);
	gtk_widget_set_sensitive(m_wAutoCols, !bParas);
	if (bParas)
	{
		gtk_check_button_set_active(GTK_CHECK_BUTTON(m_wAutoCols), FALSE);
	}
	const bool bAuto =
		!bParas &&
		gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wAutoCols));
	gtk_widget_set_sensitive(m_wNumCols, !bAuto);
}

static void s_sep_toggled(GtkCheckButton *, AP_UnixDialog_TextToTable * me)
{
	me->_syncSensitivity();
}

static void s_auto_toggled(GtkCheckButton *, AP_UnixDialog_TextToTable * me)
{
	me->_syncSensitivity();
}

GtkWidget * AP_UnixDialog_TextToTable::_constructWindow(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();

	GtkBuilder * builder = newDialogBuilderFromResource("ap_UnixDialog_TextToTable.ui");
	GtkWidget * window =
		GTK_WIDGET(gtk_builder_get_object(builder, "ap_UnixDialog_TextToTable"));
	m_wSepTabs = GTK_WIDGET(gtk_builder_get_object(builder, "rbSepTabs"));
	m_wSepCommas = GTK_WIDGET(gtk_builder_get_object(builder, "rbSepCommas"));
	m_wSepParas = GTK_WIDGET(gtk_builder_get_object(builder, "rbSepParas"));
	m_wSepOther = GTK_WIDGET(gtk_builder_get_object(builder, "rbSepOther"));
	m_wOtherEntry = GTK_WIDGET(gtk_builder_get_object(builder, "enOther"));
	m_wAutoCols = GTK_WIDGET(gtk_builder_get_object(builder, "cbAutoCols"));
	m_wNumCols = GTK_WIDGET(gtk_builder_get_object(builder, "sbNumCols"));

	// set the dialog title
	std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_TextToTable_Title, s);
	abiDialogSetTitle(window, "%s", s.c_str());

	// localize the strings in our dialog
	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbSepAt")),
						pSS, AP_STRING_ID_DLG_TextToTable_Separate);
	localizeButton(m_wSepTabs, pSS, AP_STRING_ID_DLG_TextToTable_Tabs);
	localizeButton(m_wSepCommas, pSS, AP_STRING_ID_DLG_TextToTable_Commas);
	localizeButton(m_wSepParas, pSS, AP_STRING_ID_DLG_TextToTable_Paragraphs);
	localizeButton(m_wSepOther, pSS, AP_STRING_ID_DLG_TextToTable_Other);
	localizeButton(m_wAutoCols, pSS, AP_STRING_ID_DLG_TextToTable_AutoCols);
	localizeLabel(GTK_WIDGET(gtk_builder_get_object(builder, "lbNumCols")),
				  pSS, AP_STRING_ID_DLG_TextToTable_NumCols);
	localizeButtonUnderline(GTK_WIDGET(gtk_builder_get_object(builder, "btConvert")),
							pSS, AP_STRING_ID_DLG_TextToTable_Convert);

	for (GtkWidget * w : { m_wSepTabs, m_wSepCommas, m_wSepParas, m_wSepOther })
	{
		g_signal_connect(G_OBJECT(w), "toggled",
						 G_CALLBACK(s_sep_toggled), this);
	}
	g_signal_connect(G_OBJECT(m_wAutoCols), "toggled",
					 G_CALLBACK(s_auto_toggled), this);

	g_object_unref(G_OBJECT(builder));

	return window;
}

void AP_UnixDialog_TextToTable::_populateWindowData(void)
{
	_syncSensitivity();
}

void AP_UnixDialog_TextToTable::_storeWindowData(void)
{
	if (gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wSepCommas)))
	{
		m_sepType = sep_COMMAS;
	}
	else if (gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wSepParas)))
	{
		m_sepType = sep_PARAS;
	}
	else if (gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wSepOther)))
	{
		m_sepType = sep_OTHER;
	}
	else
	{
		m_sepType = sep_TABS;
	}
	m_bAutoCols =
		gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wAutoCols));
	m_numCols = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(m_wNumCols));
	// the custom separator is the entry's first character; empty keeps
	// the constructor default
	const gchar * szOther = XAP_gtk_entry_get_text(GTK_EDITABLE(m_wOtherEntry));
	if (szOther && *szOther)
	{
		UT_UCS4String sOther(szOther);
		if (sOther.size() > 0)
		{
			m_sepChar = sOther.ucs4_str()[0];
		}
	}
}
