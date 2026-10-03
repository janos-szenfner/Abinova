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
#include "ut_assert.h"
#include "ut_debugmsg.h"

#include "xap_UnixDialogHelper.h"
#include "xap_GtkUtils.h"

#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"
#include "ap_UnixDialog_SignatureLine.h"

/*****************************************************************/

#define BUTTON_INSERT 1

/*****************************************************************/

XAP_Dialog * AP_UnixDialog_SignatureLine::static_constructor(XAP_DialogFactory * pFactory,
															 XAP_Dialog_Id id)
{
	AP_UnixDialog_SignatureLine * p = new AP_UnixDialog_SignatureLine(pFactory,id);
	return p;
}

AP_UnixDialog_SignatureLine::AP_UnixDialog_SignatureLine(XAP_DialogFactory * pDlgFactory,
														 XAP_Dialog_Id id)
	: AP_Dialog_SignatureLine(pDlgFactory,id)
	, m_windowMain(nullptr)
	, m_entrySigner(nullptr)
	, m_entryTitle(nullptr)
	, m_entryEmail(nullptr)
	, m_textInstructions(nullptr)
	, m_checkComments(nullptr)
	, m_checkShowDate(nullptr)
{
}

AP_UnixDialog_SignatureLine::~AP_UnixDialog_SignatureLine(void)
{
}

/*****************************************************************/

void AP_UnixDialog_SignatureLine::runModal(XAP_Frame * pFrame)
{
	UT_return_if_fail(pFrame);
	GtkWidget * mainWindow = _constructWindow();
	UT_return_if_fail(mainWindow);

	switch(abiRunModalDialog(GTK_DIALOG(mainWindow), pFrame, this,
							 BUTTON_INSERT, false))
	{
	case BUTTON_INSERT:
		event_OK() ; break ;
	default:
		event_Cancel() ; break ;
	}

	abiDestroyWidget ( mainWindow ) ;
}

void AP_UnixDialog_SignatureLine::event_OK(void)
{
	FV_SignatureSetup & sig = getSignatureSetup();
	sig.sSigner = XAP_gtk_entry_get_text(GTK_EDITABLE(m_entrySigner));
	sig.sTitle = XAP_gtk_entry_get_text(GTK_EDITABLE(m_entryTitle));
	sig.sEmail = XAP_gtk_entry_get_text(GTK_EDITABLE(m_entryEmail));

	GtkTextBuffer * buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_textInstructions));
	GtkTextIter start, end;
	gtk_text_buffer_get_bounds(buf, &start, &end);
	gchar * text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
	sig.sInstructions = text ? text : "";
	g_free(text);

	sig.bAllowComments = gtk_check_button_get_active(GTK_CHECK_BUTTON(m_checkComments));
	sig.bShowSignDate = gtk_check_button_get_active(GTK_CHECK_BUTTON(m_checkShowDate));

	setAnswer(AP_Dialog_SignatureLine::a_OK);
}

void AP_UnixDialog_SignatureLine::event_Cancel(void)
{
	setAnswer(AP_Dialog_SignatureLine::a_CANCEL);
}

static GtkWidget * s_labeledEntry(GtkWidget * container, const char * label,
								  GtkWidget ** entry)
{
	GtkWidget * lab = gtk_label_new(label);
	gtk_label_set_xalign(GTK_LABEL(lab), 0.0);
	gtk_box_append(GTK_BOX(container), lab);
	GtkWidget * e = gtk_entry_new();
	gtk_widget_set_hexpand(e, TRUE);
	gtk_box_append(GTK_BOX(container), e);
	if(entry)
	{
		*entry = e;
	}
	return e;
}

void AP_UnixDialog_SignatureLine::_constructWindowContents(GtkWidget * container)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();
	std::string s;

	pSS->getValueUTF8(AP_STRING_ID_DLG_SignatureLine_Signer, s);
	s_labeledEntry(container, s.c_str(), &m_entrySigner);
	pSS->getValueUTF8(AP_STRING_ID_DLG_SignatureLine_SignerTitle, s);
	s_labeledEntry(container, s.c_str(), &m_entryTitle);
	pSS->getValueUTF8(AP_STRING_ID_DLG_SignatureLine_Email, s);
	s_labeledEntry(container, s.c_str(), &m_entryEmail);

	pSS->getValueUTF8(AP_STRING_ID_DLG_SignatureLine_Instructions, s);
	GtkWidget * lab = gtk_label_new(s.c_str());
	gtk_label_set_xalign(GTK_LABEL(lab), 0.0);
	gtk_box_append(GTK_BOX(container), lab);

	GtkWidget * scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
								   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll), 60);
	gtk_widget_set_hexpand(scroll, TRUE);
	m_textInstructions = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(m_textInstructions), GTK_WRAP_WORD);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), m_textInstructions);
	gtk_box_append(GTK_BOX(container), scroll);

	pSS->getValueUTF8(AP_STRING_ID_DLG_SignatureLine_AllowComments, s);
	m_checkComments = gtk_check_button_new_with_label(s.c_str());
	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_checkComments),
								getSignatureSetup().bAllowComments);
	gtk_box_append(GTK_BOX(container), m_checkComments);

	pSS->getValueUTF8(AP_STRING_ID_DLG_SignatureLine_ShowDate, s);
	m_checkShowDate = gtk_check_button_new_with_label(s.c_str());
	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_checkShowDate),
								getSignatureSetup().bShowSignDate);
	gtk_box_append(GTK_BOX(container), m_checkShowDate);
}

GtkWidget* AP_UnixDialog_SignatureLine::_constructWindow(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();
	std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_SignatureLine_Title, s);

	m_windowMain = abiDialogNew("signature setup dialog", TRUE, s.c_str());

	GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_visible(vbox, TRUE);
	xap_gtk_container_add(gtk_dialog_get_content_area(GTK_DIALOG(m_windowMain)),
						  vbox);
	XAP_gtk_widget_set_margin(vbox, 5);
	gtk_widget_set_size_request(m_windowMain, 380, -1);

	_constructWindowContents(vbox);

	pSS->getValueUTF8(XAP_STRING_ID_DLG_Cancel, s);
	abiAddButton(GTK_DIALOG(m_windowMain), s, BUTTON_CANCEL);
	pSS->getValueUTF8(XAP_STRING_ID_DLG_OK, s);
	abiAddButton(GTK_DIALOG(m_windowMain), s, BUTTON_INSERT);

	gtk_widget_grab_focus(m_entrySigner);

	return m_windowMain;
}
