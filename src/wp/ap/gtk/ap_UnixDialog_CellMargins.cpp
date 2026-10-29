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
#include "ap_Dialog_CellMargins.h"
#include "ap_UnixDialog_CellMargins.h"

/*****************************************************************/

XAP_Dialog * AP_UnixDialog_CellMargins::static_constructor(XAP_DialogFactory * pFactory,
														 XAP_Dialog_Id id)
{
	return new AP_UnixDialog_CellMargins(pFactory,id);
}

AP_UnixDialog_CellMargins::AP_UnixDialog_CellMargins(XAP_DialogFactory * pDlgFactory,
													 XAP_Dialog_Id id)
	: AP_Dialog_CellMargins(pDlgFactory, id),
	  m_windowMain(nullptr),
	  m_wSpin{ nullptr, nullptr, nullptr, nullptr },
	  m_wUnit(nullptr)
{
}

AP_UnixDialog_CellMargins::~AP_UnixDialog_CellMargins(void)
{
}

void AP_UnixDialog_CellMargins::runModal(XAP_Frame * pFrame)
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
			m_answer = AP_Dialog_CellMargins::a_OK;
			break;
		default:
			m_answer = AP_Dialog_CellMargins::a_CANCEL;
			break;
	}

	_storeWindowData();

	abiDestroyWidget ( m_windowMain ) ;
}

/*****************************************************************/

GtkWidget * AP_UnixDialog_CellMargins::_constructWindow(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();

	GtkBuilder * builder = newDialogBuilderFromResource("ap_UnixDialog_CellMargins.ui");
	GtkWidget * window =
		GTK_WIDGET(gtk_builder_get_object(builder, "ap_UnixDialog_CellMargins"));
	m_wSpin[edge_TOP] = GTK_WIDGET(gtk_builder_get_object(builder, "sbTop"));
	m_wSpin[edge_BOTTOM] = GTK_WIDGET(gtk_builder_get_object(builder, "sbBottom"));
	m_wSpin[edge_LEFT] = GTK_WIDGET(gtk_builder_get_object(builder, "sbLeft"));
	m_wSpin[edge_RIGHT] = GTK_WIDGET(gtk_builder_get_object(builder, "sbRight"));
	m_wUnit = GTK_WIDGET(gtk_builder_get_object(builder, "lbUnit"));

	// set the dialog title
	std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_CellMargins_Title, s);
	abiDialogSetTitle(window, "%s", s.c_str());

	// localize the strings in our dialog
	localizeLabel(GTK_WIDGET(gtk_builder_get_object(builder, "lbTop")),
				  pSS, AP_STRING_ID_DLG_CellMargins_Top);
	localizeLabel(GTK_WIDGET(gtk_builder_get_object(builder, "lbBottom")),
				  pSS, AP_STRING_ID_DLG_CellMargins_Bottom);
	localizeLabel(GTK_WIDGET(gtk_builder_get_object(builder, "lbLeft")),
				  pSS, AP_STRING_ID_DLG_CellMargins_Left);
	localizeLabel(GTK_WIDGET(gtk_builder_get_object(builder, "lbRight")),
				  pSS, AP_STRING_ID_DLG_CellMargins_Right);

	// spins run in the current ruler unit
	gtk_label_set_text(GTK_LABEL(m_wUnit), UT_dimensionName(m_dim));
	const double step = getSpinIncr();
	for (int e = 0; e < 4; ++e)
	{
		gtk_spin_button_set_range(GTK_SPIN_BUTTON(m_wSpin[e]),
								  getSpinMin(), getSpinMax());
		gtk_spin_button_set_increments(GTK_SPIN_BUTTON(m_wSpin[e]),
									   step, step * 4);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_wSpin[e]),
								  getMarginDim(e));
		gtk_spin_button_set_digits(GTK_SPIN_BUTTON(m_wSpin[e]),
								   m_dim == DIM_PT || m_dim == DIM_PI ? 1 : 2);
	}

	{
		std::string sOK;
		pSS->getValueUTF8(XAP_STRING_ID_DLG_OK, sOK);
		gtk_button_set_label(
			GTK_BUTTON(gtk_builder_get_object(builder, "btOK")),
			sOK.c_str());
	}

	g_object_unref(G_OBJECT(builder));

	return window;
}

void AP_UnixDialog_CellMargins::_populateWindowData(void)
{
}

void AP_UnixDialog_CellMargins::_storeWindowData(void)
{
	for (int e = 0; e < 4; ++e)
	{
		setMarginDim(e, gtk_spin_button_get_value(
						 GTK_SPIN_BUTTON(m_wSpin[e])));
	}
}
