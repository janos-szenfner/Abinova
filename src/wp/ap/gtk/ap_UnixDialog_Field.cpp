/* Abinova
 * Copyright (C) 1998-2000 AbiSource, Inc.
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
#include <time.h>

#include "ut_string.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"

// This header defines some functions for Unix dialogs,
// like centering them, measuring them, etc.
#include "xap_UnixDialogHelper.h"
#include "xap_GtkUtils.h"
#include "xap_GtkListHelpers.h"

#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"
#include "ap_Dialog_Field.h"
#include "ap_UnixDialog_Field.h"


/*****************************************************************/

#define	LIST_ITEM_INDEX_KEY "index"
#define CUSTOM_RESPONSE_INSERT 1

/*****************************************************************/

XAP_Dialog * AP_UnixDialog_Field::static_constructor(XAP_DialogFactory * pFactory,
													 XAP_Dialog_Id id)
{
	return new AP_UnixDialog_Field(pFactory,id);
}

AP_UnixDialog_Field::AP_UnixDialog_Field(XAP_DialogFactory * pDlgFactory,
										 XAP_Dialog_Id id)
	: AP_Dialog_Field(pDlgFactory,id)
{
	m_windowMain = nullptr;
	m_listTypes = nullptr;
	m_listFields = nullptr;
	m_entryParam = nullptr;
	m_selTypes = nullptr;
	m_selFields = nullptr;
	m_cursorChangedHandlerId = 0;
	m_rowActivatedHandlerId = 0;
}

AP_UnixDialog_Field::~AP_UnixDialog_Field(void)
{
}

/*****************************************************************/

static void s_types_changed(GObject * /*obj*/, GParamSpec * /*pspec*/,
                            AP_UnixDialog_Field * dlg)
{
	UT_ASSERT(dlg);
	dlg->types_changed();
}

void AP_UnixDialog_Field::s_field_dblclicked(GtkListView * /*listview*/,
											 guint /*position*/,
											 AP_UnixDialog_Field * me)
{
	gtk_dialog_response (GTK_DIALOG(me->m_windowMain), CUSTOM_RESPONSE_INSERT);
}

/*****************************************************************/

void AP_UnixDialog_Field::runModal(XAP_Frame * pFrame)
{
	UT_return_if_fail(pFrame);
	
	// Build the window's widgets and arrange them
	m_windowMain = _constructWindow();
	UT_return_if_fail(m_windowMain);

	// Populate the window's data items
	_populateCatogries();

	switch ( abiRunModalDialog ( GTK_DIALOG(m_windowMain), pFrame, this,
								 CUSTOM_RESPONSE_INSERT, false ) )
	{
		case CUSTOM_RESPONSE_INSERT:
			event_Insert();
			break;
		default:
			m_answer = AP_Dialog_Field::a_CANCEL;
			break;
	}

        // We need to disconnect handler or answer will reset to "Cancel" during
        // destruction
        g_signal_handler_disconnect(G_OBJECT(m_selTypes), m_cursorChangedHandlerId);
        g_signal_handler_disconnect(G_OBJECT(m_listFields), m_rowActivatedHandlerId);
	abiDestroyWidget ( m_windowMain ) ;
}

void AP_UnixDialog_Field::event_Insert(void)
{
	UT_ASSERT(m_windowMain && m_listTypes && m_listFields);

	// find item selected in the Types list box, save it to m_iTypeIndex

	// if there is no selection return cancel.  GTK can make this happen.
	int typeIndex = XAP_single_selection_get_int(m_selTypes);
	if (typeIndex < 0)
	{
		m_answer = AP_Dialog_Field::a_CANCEL;
		return;
	}
	m_iTypeIndex = typeIndex;

	// find item selected in the Field list box, save it to m_iFormatIndex
	int formatIndex = XAP_single_selection_get_int(m_selFields);
	if (formatIndex < 0)
	{
		m_answer = AP_Dialog_Field::a_CANCEL;
		return;
	}
	m_iFormatIndex = formatIndex;

	setParameter(XAP_gtk_entry_get_text(GTK_EDITABLE(m_entryParam)));
	m_answer = AP_Dialog_Field::a_OK;
}


void AP_UnixDialog_Field::types_changed(void)
{
	// if there is no selection return.  GTK can make this happen.
	int typeIndex = XAP_single_selection_get_int(m_selTypes);
	if (typeIndex < 0)
	{
		m_answer = AP_Dialog_Field::a_CANCEL;
		return;
	}

	// Update m_iTypeIndex with the row number
	m_iTypeIndex = typeIndex;

	// Update the fields list with this new Type
	setFieldsList();
}

void AP_UnixDialog_Field::setTypesList(void)
{
	UT_ASSERT(m_listTypes && m_selTypes);

	UT_sint32 i;

	GListStore *model = G_LIST_STORE(
		gtk_single_selection_get_model(m_selTypes));
	g_list_store_remove_all(model);

 	// build a list of all items
    for (i = 0; fp_FieldTypes[i].m_Desc != nullptr; i++)
	{
		// Add a new row to the model
		XAP_list_store_append_text_and_int(model, fp_FieldTypes[i].m_Desc, i);
	}

	// now select first item in box
 	gtk_widget_grab_focus (m_listTypes);

	gtk_single_selection_set_selected(m_selTypes, 0);

	m_iTypeIndex = 0;
}

void AP_UnixDialog_Field::setFieldsList(void)
{
	UT_ASSERT(m_listFields && m_selFields);

	fp_FieldTypesEnum FType = fp_FieldTypes[m_iTypeIndex].m_Type;

	UT_sint32 i;

	GListStore *model = G_LIST_STORE(
		gtk_single_selection_get_model(m_selFields));
	g_list_store_remove_all(model);

 	// build a list of all items
    for (i = 0; fp_FieldFmts[i].m_Tag != nullptr; i++)
	{
		if((fp_FieldFmts[i].m_Num != FPFIELD_endnote_anch) &&
		   (fp_FieldFmts[i].m_Num != FPFIELD_endnote_ref) &&
		   (fp_FieldFmts[i].m_Num != FPFIELD_footnote_anch) &&
		   (fp_FieldFmts[i].m_Num != FPFIELD_footnote_ref))
		{

			if (fp_FieldFmts[i].m_Type == FType)
			{
				// Add a new row to the model
				XAP_list_store_append_text_and_int(model, fp_FieldFmts[i].m_Desc, i);
			}
		}
	}

	// now select first item in box
 	gtk_widget_grab_focus (m_listFields);
}


/*****************************************************************/


GtkWidget * AP_UnixDialog_Field::_constructWindow(void)
{
	GtkWidget * window;
	const XAP_StringSet * pSS = m_pApp->getStringSet();

	GtkBuilder * builder = newDialogBuilderFromResource("ap_UnixDialog_Field.ui");
	
	// Update our member variables with the important widgets that 
	// might need to be queried or altered later
	window = GTK_WIDGET(gtk_builder_get_object(builder, "ap_UnixDialog_Field"));
	m_listTypes = GTK_WIDGET(gtk_builder_get_object(builder, "tvTypes"));
	m_listFields = GTK_WIDGET(gtk_builder_get_object(builder, "tvFields"));
	m_entryParam = GTK_WIDGET(gtk_builder_get_object(builder, "edExtraParameters"));
	
	GListStore *typesStore = XAP_list_store_new();
	m_selTypes =
		XAP_list_view_set_model(GTK_LIST_VIEW(m_listTypes), typesStore);
	g_object_unref(typesStore);

	GListStore *fieldsStore = XAP_list_store_new();
	m_selFields =
		XAP_list_view_set_model(GTK_LIST_VIEW(m_listFields), fieldsStore);
	g_object_unref(fieldsStore);

	// set the dialog title
	std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_Field_FieldTitle,s);
	abiDialogSetTitle(window, "%s", s.c_str());
	
	// localize the strings in our dialog, and set some userdata for some widg

	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbTypes")), pSS, AP_STRING_ID_DLG_Field_Types_No_Colon);
	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbFields")), pSS, AP_STRING_ID_DLG_Field_Fields_No_Colon);
	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbExtraParameters")), pSS, AP_STRING_ID_DLG_Field_Parameters);
	localizeButtonUnderline(GTK_WIDGET(gtk_builder_get_object(builder, "btInsert")), pSS, AP_STRING_ID_DLG_InsertButton);

	// refill the fields list when the selected type changes

	m_cursorChangedHandlerId = g_signal_connect_after(G_OBJECT(m_selTypes),
						   "notify::selected",
						   G_CALLBACK(s_types_changed),
						   static_cast<gpointer>(this));

	m_rowActivatedHandlerId = g_signal_connect_after(G_OBJECT(m_listFields),
						   "activate",
						   G_CALLBACK(s_field_dblclicked),
						   static_cast<gpointer>(this));

	g_object_unref(G_OBJECT(builder));

	return window;
}

void AP_UnixDialog_Field::_populateCatogries(void)
{
	// Fill in the two lists
	setTypesList();
	setFieldsList();
}
