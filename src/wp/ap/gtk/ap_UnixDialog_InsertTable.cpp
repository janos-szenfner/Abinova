/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* Abinova
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (c) 2023 Hubert Figuière
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
#include "ap_Dialog_InsertTable.h"
#include "ap_UnixDialog_InsertTable.h"

/*****************************************************************/

#define	WIDGET_ID_TAG_KEY "id"
#define CUSTOM_RESPONSE_INSERT 1

/*****************************************************************/

// the fixed-width spin is only editable while "Fixed column width" is
// active; connected to each radio's toggled with the fixed radio as data
static void
s_fixed_colsize_toggled (GtkCheckButton *,
                         GtkCheckButton   *fixed)
{
	GtkWidget * spinner = GTK_WIDGET(g_object_get_data(G_OBJECT(fixed),
													 "abi-width-spin"));
	gtk_widget_set_sensitive (GTK_WIDGET(spinner),
							  gtk_check_button_get_active (fixed));
}

XAP_Dialog * AP_UnixDialog_InsertTable::static_constructor(XAP_DialogFactory * pFactory,
													       XAP_Dialog_Id id)
{
	AP_UnixDialog_InsertTable * p = new AP_UnixDialog_InsertTable(pFactory,id);
	return p;
}

AP_UnixDialog_InsertTable::AP_UnixDialog_InsertTable(XAP_DialogFactory * pDlgFactory,
										             XAP_Dialog_Id id)
	: AP_Dialog_InsertTable(pDlgFactory, id)
	, m_windowMain(nullptr)
	, m_autoCol(nullptr)
	, m_fixedCol(nullptr)
	, m_contentsCol(nullptr)
	, m_pColSpin(nullptr)
	, m_pRowSpin(nullptr)
	, m_pColWidthSpin(nullptr)
	, m_pPreview(nullptr)
{
}

AP_UnixDialog_InsertTable::~AP_UnixDialog_InsertTable(void)
{
}

void AP_UnixDialog_InsertTable::runModal(XAP_Frame * pFrame)
{
    // Build the dialog's window
	m_windowMain = _constructWindow();
	UT_return_if_fail(m_windowMain);

	// Populate the window's data items
	_populateWindowData();

	switch ( abiRunModalDialog ( GTK_DIALOG(m_windowMain),
								 pFrame, this, CUSTOM_RESPONSE_INSERT, false ) )
	{
		case CUSTOM_RESPONSE_INSERT:
			m_answer = AP_Dialog_InsertTable::a_OK;
			break;
		default:
			m_answer = AP_Dialog_InsertTable::a_CANCEL;
			break;
	}

	_storeWindowData();

	abiDestroyWidget ( m_windowMain ) ;
}

/*****************************************************************/

GtkWidget * AP_UnixDialog_InsertTable::_constructWindow(void)
{
	GtkWidget * window;
	const XAP_StringSet * pSS = m_pApp->getStringSet();

	GtkBuilder * builder = newDialogBuilderFromResource("ap_UnixDialog_InsertTable.ui");
	// Update our member variables with the important widgets that 
	// might need to be queried or altered later
	window = GTK_WIDGET(gtk_builder_get_object(builder, "ap_UnixDialog_InsertTable"));
	m_pColSpin = GTK_WIDGET(gtk_builder_get_object(builder, "sbNumCols"));
	m_pRowSpin = GTK_WIDGET(gtk_builder_get_object(builder, "sbNumRows"));
	m_pColWidthSpin = GTK_WIDGET(gtk_builder_get_object(builder, "sbColSize"));
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_pColSpin), getNumCols());
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_pRowSpin), getNumRows());

	m_fixedCol = GTK_WIDGET(gtk_builder_get_object(builder, "rbFixedColSize"));
	m_autoCol = GTK_WIDGET(gtk_builder_get_object(builder, "rbAutoColSize"));
	m_contentsCol = GTK_WIDGET(gtk_builder_get_object(builder, "rbAutoContents"));
	g_object_set_data(G_OBJECT(m_fixedCol), "abi-width-spin", m_pColWidthSpin);
	s_fixed_colsize_toggled (nullptr, GTK_CHECK_BUTTON(m_fixedCol));
	g_signal_connect (G_OBJECT (m_fixedCol), "toggled", G_CALLBACK (s_fixed_colsize_toggled), m_fixedCol);
	g_signal_connect (G_OBJECT (m_autoCol), "toggled", G_CALLBACK (s_fixed_colsize_toggled), m_fixedCol);
	g_signal_connect (G_OBJECT (m_contentsCol), "toggled", G_CALLBACK (s_fixed_colsize_toggled), m_fixedCol);

	// live preview: a miniature grid tracking the spin values
	m_pPreview = GTK_WIDGET(gtk_builder_get_object(builder, "daPreview"));
	if (m_pPreview)
	{
		gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(m_pPreview),
									   _s_previewDraw, this, nullptr);
		g_signal_connect_swapped(G_OBJECT(m_pColSpin), "value-changed",
								 G_CALLBACK(gtk_widget_queue_draw), m_pPreview);
		g_signal_connect_swapped(G_OBJECT(m_pRowSpin), "value-changed",
								 G_CALLBACK(gtk_widget_queue_draw), m_pPreview);
	}
	
	// set the dialog title
    std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_InsertTable_TableTitle,s);
	abiDialogSetTitle(window, "%s", s.c_str());
	// Units
	gtk_label_set_text (GTK_LABEL (GTK_WIDGET(gtk_builder_get_object(builder, "lbInch"))), UT_dimensionName(m_dim));
	double spinstep = getSpinIncr ();
	gtk_spin_button_set_increments (GTK_SPIN_BUTTON(m_pColWidthSpin), spinstep, spinstep * 5);
	double spinmin = getSpinMin ();
	gtk_spin_button_set_range (GTK_SPIN_BUTTON(m_pColWidthSpin), spinmin, spinmin * 1000);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_pColWidthSpin), m_columnWidth);
	// localize the strings in our dialog, and set tags for some widgets
	
	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbTableSize")), pSS, AP_STRING_ID_DLG_InsertTable_TableSize);
	localizeLabel(GTK_WIDGET(gtk_builder_get_object(builder, "lbNumCols")), pSS, AP_STRING_ID_DLG_InsertTable_NumCols);
	localizeLabel(GTK_WIDGET(gtk_builder_get_object(builder, "lbNumRows")), pSS, AP_STRING_ID_DLG_InsertTable_NumRows);
	
	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbAutoFit")), pSS, AP_STRING_ID_DLG_InsertTable_AutoFit);

	localizeButton(m_autoCol, pSS, AP_STRING_ID_DLG_InsertTable_AutoFitWindow);
	localizeButton(m_contentsCol, pSS, AP_STRING_ID_DLG_InsertTable_AutoFitContents);
	localizeButton(m_fixedCol, pSS, AP_STRING_ID_DLG_InsertTable_FixedColSize);

	GtkWidget * frPreview = GTK_WIDGET(gtk_builder_get_object(builder, "frPreview"));
	if (frPreview)
	{
		std::string sPrev;
		pSS->getValueUTF8(AP_STRING_ID_DLG_InsertTable_Preview, sPrev);
		gtk_frame_set_label(GTK_FRAME(frPreview), sPrev.c_str());
	}

	// restore the persisted radio choice
	switch (m_columnType)
	{
	case b_FIXEDSIZE:
		gtk_check_button_set_active(GTK_CHECK_BUTTON(m_fixedCol), TRUE);
		break;
	case b_AUTOFIT_CONTENTS:
		gtk_check_button_set_active(GTK_CHECK_BUTTON(m_contentsCol), TRUE);
		break;
	default:
		gtk_check_button_set_active(GTK_CHECK_BUTTON(m_autoCol), TRUE);
		break;
	}

	localizeButtonUnderline(GTK_WIDGET(gtk_builder_get_object(builder, "btInsert")), pSS, AP_STRING_ID_DLG_InsertButton);

	g_object_unref(G_OBJECT(builder));

	return window;
}

void AP_UnixDialog_InsertTable::_populateWindowData(void)
{
	// We're (still) a stateless dialog, so there are 
	// no member variables to setyet
}

void AP_UnixDialog_InsertTable::_storeWindowData(void)
{
	m_columnType = _getActiveRadioItem();
	m_numRows = static_cast<UT_uint32>(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(m_pRowSpin)));
	m_numCols = static_cast<UT_uint32>(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(m_pColSpin)));
	m_columnWidth = static_cast<float>(gtk_spin_button_get_value(GTK_SPIN_BUTTON(m_pColWidthSpin)));
	if (m_answer == AP_Dialog_InsertTable::a_OK)
		saveLastUsed();
}

AP_Dialog_InsertTable::columnType AP_UnixDialog_InsertTable::_getActiveRadioItem(void)
{
	if (gtk_check_button_get_active(GTK_CHECK_BUTTON(m_fixedCol))) {
		return AP_Dialog_InsertTable::b_FIXEDSIZE;
	}
	if (m_contentsCol &&
		gtk_check_button_get_active(GTK_CHECK_BUTTON(m_contentsCol))) {
		return AP_Dialog_InsertTable::b_AUTOFIT_CONTENTS;
	}

	return AP_Dialog_InsertTable::b_AUTOFIT_WINDOW;
}

/* miniature live preview: draws the rows/cols grid like the picker */
void
AP_UnixDialog_InsertTable::_s_previewDraw (GtkDrawingArea * /*da*/,
										   cairo_t * cr,
										   int w, int h, gpointer data)
{
	AP_UnixDialog_InsertTable * dlg =
		static_cast<AP_UnixDialog_InsertTable *>(data);
	UT_return_if_fail(dlg);

	int nCols = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(dlg->m_pColSpin));
	int nRows = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(dlg->m_pRowSpin));
	nCols = CLAMP(nCols, 1, 20);
	nRows = CLAMP(nRows, 1, 20);

	double pad = 6.0;
	double gw = w - 2 * pad;
	double gh = h - 2 * pad;
	double cw = gw / nCols;
	double ch = gh / nRows;

	/* paper */
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
	cairo_rectangle(cr, pad, pad, gw, gh);
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, 0.55, 0.55, 0.55, 1.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	/* grid lines */
	cairo_set_source_rgba(cr, 0.35, 0.35, 0.35, 0.8);
	cairo_set_line_width(cr, 0.7);
	for (int i = 1; i < nCols; ++i)
	{
		double x = pad + i * cw;
		cairo_move_to(cr, x, pad);
		cairo_line_to(cr, x, pad + gh);
	}
	for (int j = 1; j < nRows; ++j)
	{
		double y = pad + j * ch;
		cairo_move_to(cr, pad, y);
		cairo_line_to(cr, pad + gw, y);
	}
	cairo_stroke(cr);
}
