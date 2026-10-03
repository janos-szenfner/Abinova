/* AbiSource Application Framework
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ut_string.h"
#include "xap_UnixDialogHelper.h"
#include "xap_GtkListHelpers.h"
#include "xap_UnixDlg_Language.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"


XAP_Dialog * XAP_UnixDialog_Language::static_constructor(XAP_DialogFactory * pFactory,
							 XAP_Dialog_Id id)
{
	return new XAP_UnixDialog_Language(pFactory,id);
}

XAP_UnixDialog_Language::XAP_UnixDialog_Language(XAP_DialogFactory * pDlgFactory,
						 XAP_Dialog_Id id)
  : XAP_Dialog_Language(pDlgFactory,id), m_pLanguageList ( nullptr ),
	m_lbDefaultLanguage(nullptr), m_cbDefaultLanguage(nullptr),
	m_cbNoProof(nullptr), m_cbAutoDetect(nullptr), m_windowMain(nullptr),
	m_selLanguages(nullptr)
{
}

/* "Do not check spelling or grammar": the list is desensitised and
 * the applied language becomes -none- */
void XAP_UnixDialog_Language::s_noProof_toggled(GtkToggleButton * t,
												XAP_UnixDialog_Language * me)
{
	bool b = gtk_check_button_get_active(GTK_CHECK_BUTTON(t));
	me->setNoProofing(b);
	gtk_widget_set_sensitive(me->m_pLanguageList, !b);
	gtk_widget_set_sensitive(me->m_cbAutoDetect, !b);
	if (b && me->m_selLanguages)
		gtk_selection_model_unselect_all(
			GTK_SELECTION_MODEL(me->m_selLanguages));
}

/* "Detect language automatically": score the sample text against the
 * installed dictionaries and select the best match; stays unchecked
 * when detection is inconclusive */
void XAP_UnixDialog_Language::s_autoDetect_toggled(GtkToggleButton * t,
												   XAP_UnixDialog_Language * me)
{
	if (!gtk_check_button_get_active(GTK_CHECK_BUTTON(t)))
		return;

	const gchar * szName = me->detectLanguage();
	if (!szName)
	{
		gtk_widget_set_tooltip_text(
			GTK_WIDGET(t),
			"Could not detect the language of the current text");
		g_signal_handlers_block_by_func(
			t, reinterpret_cast<gpointer>(s_autoDetect_toggled), me);
		gtk_check_button_set_active(GTK_CHECK_BUTTON(t), FALSE);
		g_signal_handlers_unblock_by_func(
			t, reinterpret_cast<gpointer>(s_autoDetect_toggled), me);
		return;
	}

	// select the detected language's row
	for (UT_uint32 i = 0; i < me->m_iLangCount; ++i)
	{
		if (!g_ascii_strcasecmp(szName, me->m_ppLanguages[i]))
		{
			gtk_list_view_scroll_to(
				GTK_LIST_VIEW(me->m_pLanguageList), i,
				static_cast<GtkListScrollFlags>(
					GTK_LIST_SCROLL_SELECT | GTK_LIST_SCROLL_FOCUS),
				nullptr);
			break;
		}
	}
}

void XAP_UnixDialog_Language::s_lang_dblclicked(GtkListView * /*listview*/,
												guint /*position*/,
												XAP_UnixDialog_Language * me)
{
	gtk_dialog_response (GTK_DIALOG(me->m_windowMain), GTK_RESPONSE_OK);
}

XAP_UnixDialog_Language::~XAP_UnixDialog_Language(void)
{
}

void XAP_UnixDialog_Language::event_setLang()
{
	// "Do not check spelling or grammar" applies -none-, which is
	// always the first (unsorted) row of the language list
	if (getNoProofing())
	{
		_setLanguage(m_ppLanguages[0]);
		m_bChangedLanguage = true;
		m_answer = XAP_Dialog_Language::a_OK;
		setMakeDocumentDefault(false);
		return;
	}

	// if there is no selection return cancel.  GTK can make this happen.
	int row = XAP_single_selection_get_int(m_selLanguages);
	if (row < 0)
	{
		m_answer = XAP_Dialog_Language::a_CANCEL;
		return;
	}

	if (!m_pLanguage || g_ascii_strcasecmp(m_pLanguage, m_ppLanguages[row]))
	{
		_setLanguage(m_ppLanguages[row]);
		m_bChangedLanguage = true;
		m_answer = XAP_Dialog_Language::a_OK;

		bool b = gtk_check_button_get_active(GTK_CHECK_BUTTON(m_cbDefaultLanguage));
		setMakeDocumentDefault(b);
	}
	else {
		m_answer = XAP_Dialog_Language::a_CANCEL;
	}
}

GtkWidget * XAP_UnixDialog_Language::constructWindow(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();

	GtkBuilder * builder = newDialogBuilderFromResource("xap_UnixDlg_Language.ui");

	// Update our member variables with the important widgets that
	// might need to be queried or altered later
	m_windowMain = GTK_WIDGET(gtk_builder_get_object(builder, "xap_UnixDlg_Language"));
	m_pLanguageList = GTK_WIDGET(gtk_builder_get_object(builder, "lvAvailableLanguages"));
	m_lbDefaultLanguage = GTK_WIDGET(gtk_builder_get_object(builder, "lbDefaultLanguage"));
	m_cbDefaultLanguage = GTK_WIDGET(gtk_builder_get_object(builder, "cbDefaultLanguage"));
	m_cbNoProof = GTK_WIDGET(gtk_builder_get_object(builder, "cbNoProof"));
	m_cbAutoDetect = GTK_WIDGET(gtk_builder_get_object(builder, "cbAutoDetect"));

	std::string s;
	pSS->getValueUTF8(XAP_STRING_ID_DLG_ULANG_LangTitle,s);
	gtk_window_set_title (GTK_WINDOW(m_windowMain), s.c_str());
	localizeLabelMarkup (GTK_WIDGET(gtk_builder_get_object(builder, "lbAvailableLanguages")), pSS, XAP_STRING_ID_DLG_ULANG_AvailableLanguages);
	getDocDefaultLangDescription(s);
	gtk_label_set_text (GTK_LABEL(m_lbDefaultLanguage), s.c_str());
	getDocDefaultLangCheckboxLabel(s);
	gtk_check_button_set_label (GTK_CHECK_BUTTON(m_cbDefaultLanguage), s.c_str());
	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_cbDefaultLanguage), isMakeDocumentDefault());

	if (getNoProofing())
	{
		gtk_check_button_set_active(GTK_CHECK_BUTTON(m_cbNoProof), TRUE);
		gtk_widget_set_sensitive(m_pLanguageList, FALSE);
		gtk_widget_set_sensitive(m_cbAutoDetect, FALSE);
	}
	g_signal_connect(m_cbNoProof, "toggled",
					 G_CALLBACK(s_noProof_toggled), this);
	g_signal_connect(m_cbAutoDetect, "toggled",
					 G_CALLBACK(s_autoDetect_toggled), this);

	g_object_unref(G_OBJECT(builder));

	return m_windowMain;
}

void XAP_UnixDialog_Language::_populateWindowData()
{
	GListStore *model = XAP_list_store_new();

	for (UT_uint32 i = 0; i < m_iLangCount; i++)
	{
		XAP_list_store_append_text_and_int(model, m_ppLanguages[i], i);
	}

	m_selLanguages =
		XAP_list_view_set_model(GTK_LIST_VIEW(m_pLanguageList), model);

	// now select first item in box
 	gtk_widget_grab_focus (m_pLanguageList);

	if (m_pLanguage) {
		gint foundAt = -1;
		for (UT_uint32 i = 0; i < m_iLangCount; i++)
		{
			if (!g_ascii_strcasecmp(m_pLanguage, m_ppLanguages[i])) {
				foundAt = i;
				break;
			}
		}

		if (foundAt != -1) {
			gtk_list_view_scroll_to(
				GTK_LIST_VIEW(m_pLanguageList),
				static_cast<guint>(foundAt),
				static_cast<GtkListScrollFlags>(
					GTK_LIST_SCROLL_SELECT | GTK_LIST_SCROLL_FOCUS),
				nullptr);
			gtk_widget_grab_focus (m_pLanguageList);
		}
	}

	g_object_unref (model);
}

void XAP_UnixDialog_Language::runModal(XAP_Frame * pFrame)
{
  // build the dialog
  GtkWidget * cf = constructWindow();    
  UT_return_if_fail(cf);	
	
  _populateWindowData();

  // dbl-click / Enter activates the Apply button
  g_signal_connect_after(G_OBJECT(m_pLanguageList),
						   "activate",
						   G_CALLBACK(s_lang_dblclicked),
						   static_cast<gpointer>(this));

  gint response = abiRunModalDialog ( GTK_DIALOG(cf), pFrame, this, GTK_RESPONSE_OK, false );
  if (response == GTK_RESPONSE_OK)
	  event_setLang();
  else
	  m_answer = XAP_Dialog_Language::a_CANCEL;
  
  abiDestroyWidget(cf);
}
