/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
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

#include <stdlib.h>
#include <string.h>

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
#include "ap_Dialog_Spell.h"
#include "ap_UnixDialog_Spell.h"


//! Custom response IDs
enum: uint8_t {
	SPELL_RESPONSE_ADD = 0,
	SPELL_RESPONSE_IGNORE,
	SPELL_RESPONSE_IGNORE_ALL,
	SPELL_RESPONSE_CHANGE,
	SPELL_RESPONSE_CHANGE_ALL
};

/*!
 * Static ctor.
 */
XAP_Dialog *
AP_UnixDialog_Spell::static_constructor (XAP_DialogFactory * pFactory,
										 XAP_Dialog_Id 		 id)
{
	return new AP_UnixDialog_Spell (pFactory,id);
}

/*!
 * Ctor.
 */
AP_UnixDialog_Spell::AP_UnixDialog_Spell (XAP_DialogFactory * pDlgFactory,
										  XAP_Dialog_Id 	  id)
	: AP_Dialog_Spell (pDlgFactory, id)
	, m_wDialog(nullptr)
	, m_txWrong(nullptr)
	, m_eChange(nullptr)
	, m_lbSuggestions(nullptr)
	, m_pMisspellTag(nullptr)
	, m_pBoldTag(nullptr)
	, m_changeHandlerID(0)
	, m_selectHandlerID(0)
	, m_bUiUpdating(false)
{
}

/*!
 * Dtor.
 */
AP_UnixDialog_Spell::~AP_UnixDialog_Spell (void)
{
}

/*****************************************************************/
/* Signal trampolines — buttons emit dialog responses which the    */
/* modal loop below dispatches                                     */
/*****************************************************************/

/* Each content-area button carries its response ID as connect data and
 * a ref to its dialog; clicking emits the response into the modal loop. */
static void s_button_clicked(GtkButton * button, gpointer data)
{
	GtkWidget * dlg = GTK_WIDGET(
		g_object_get_data(G_OBJECT(button), "abi-dialog"));
	if (dlg)
		gtk_dialog_response(GTK_DIALOG(dlg), GPOINTER_TO_INT(data));
}

static GtkWidget * s_response_button(const XAP_StringSet * pSS,
									 XAP_String_Id sid,
									 GtkWidget * dlg,
									 gint response)
{
	GtkWidget * btn = gtk_button_new();
	localizeButtonUnderline(btn, pSS, sid);
	gtk_widget_set_hexpand(btn, TRUE);
	g_object_set_data(G_OBJECT(btn), "abi-dialog", dlg);
	g_signal_connect(btn, "clicked",
					 G_CALLBACK(s_button_clicked), GINT_TO_POINTER(response));
	return btn;
}

static void s_suggestion_row_activated(GtkListBox * /*box*/,
									   GtkListBoxRow * /*row*/,
									   gpointer data)
{
	AP_UnixDialog_Spell *dlg = static_cast<AP_UnixDialog_Spell*>(data);
	dlg->onSuggestionActivated();
}

static void s_suggestion_selected(GtkListBox * /*box*/,
								  GtkListBoxRow * /*row*/,
								  gpointer data)
{
	static_cast<AP_UnixDialog_Spell*>(data)->onSuggestionSelected();
}

static void s_entry_changed(GtkEditable * /*e*/, gpointer data)
{
	static_cast<AP_UnixDialog_Spell*>(data)->onSuggestionChanged();
}

/*****************************************************************/
/* Modal driver                                                   */
/*****************************************************************/

/*!
* Run dialog.
*/
void
AP_UnixDialog_Spell::runModal (XAP_Frame * pFrame)
{
    // call the base class method to initialize the XP state
    AP_Dialog_Spell::runModal(pFrame);

    bool bRes = nextMisspelledWord();
    if (!bRes)
		return;

	GtkWidget * mainWindow = _constructWindow();
	UT_return_if_fail(mainWindow);

	abiSetupModalDialog(GTK_DIALOG(mainWindow), pFrame, this,
						GTK_RESPONSE_CLOSE);

	// loop while there are still misspelled words
	while (bRes) {

		// show word in main window
		makeWordVisible();

		// update dialog with new misspelled word info/suggestions
		_updateWindow();

		// run into the GTK event loop for this window
		gint response = abiRunModalDialog (GTK_DIALOG(mainWindow), false);

		switch(response) {

			case SPELL_RESPONSE_CHANGE:
				onChangeClicked (); break;
			case SPELL_RESPONSE_CHANGE_ALL:
				onChangeAllClicked (); break;
			case SPELL_RESPONSE_IGNORE:
				onIgnoreClicked (); break;
			case SPELL_RESPONSE_IGNORE_ALL:
				onIgnoreAllClicked (); break;
			case SPELL_RESPONSE_ADD:
				onAddClicked (); break;
			default:
				m_bCancelled = TRUE;
				_purgeSuggestions();
				abiDestroyWidget(m_wDialog); // TOPLEVEL
				m_wDialog = nullptr;
				return;
		}

		_purgeSuggestions();

		// get the next unknown word
		bRes = nextMisspelledWord();
	}

	abiDestroyWidget(mainWindow);
	m_wDialog = nullptr;
}

/*****************************************************************/
/* Construction                                                   */
/*****************************************************************/

GtkWidget *
AP_UnixDialog_Spell::_constructWindow (void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();
	std::string s;

	m_wDialog = abiDialogNew("spelling dialog", TRUE);
	pSS->getValueUTF8(AP_STRING_ID_DLG_Spell_SpellTitle, s);
	gtk_window_set_title (GTK_WINDOW(m_wDialog), s.c_str());

	GtkWidget * content = gtk_dialog_get_content_area(GTK_DIALOG(m_wDialog));

	GtkWidget * grid = gtk_grid_new();
	g_object_set(G_OBJECT(grid),
				 "row-spacing", 8,
				 "column-spacing", 12,
				 "margin-top", 12,
				 "margin-bottom", 6,
				 "margin-start", 12,
				 "margin-end", 12,
				 nullptr);
	gtk_box_append(GTK_BOX(content), grid);

	/* ---- "Not in dictionary" label ---- */
	GtkWidget * lbNotInDict = gtk_label_new(nullptr);
	localizeLabelUnderline(lbNotInDict, pSS,
						   AP_STRING_ID_DLG_Spell_UnknownWord);
	gtk_label_set_xalign(GTK_LABEL(lbNotInDict), 0.0);
	gtk_grid_attach(GTK_GRID(grid), lbNotInDict, 0, 0, 2, 1);

	/* ---- Sentence context (readonly text view) ---- */
	m_txWrong = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(m_txWrong), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(m_txWrong), GTK_WRAP_WORD);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(m_txWrong), FALSE);
	gtk_text_view_set_top_margin(GTK_TEXT_VIEW(m_txWrong), 6);
	gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(m_txWrong), 6);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(m_txWrong), 6);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(m_txWrong), 6);

	GtkWidget * scWrong = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scWrong),
								   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scWrong), m_txWrong);
	gtk_widget_set_size_request(scWrong, 360, 64);
	gtk_widget_set_hexpand(scWrong, TRUE);
	gtk_grid_attach(GTK_GRID(grid), scWrong, 0, 1, 1, 1);

	/* tags for the misspelled word: red + bold, readable on any theme */
	GtkTextBuffer * buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_txWrong));
	m_pMisspellTag = gtk_text_buffer_create_tag(buf, "misspelled",
												"foreground", "#c01010",
												"weight", PANGO_WEIGHT_BOLD,
												nullptr);
	m_pBoldTag = gtk_text_buffer_create_tag(buf, "wordctx",
											nullptr);

	/* ---- Action buttons, right column ---- */
	GtkWidget * btns = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_valign(btns, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), btns, 1, 1, 1, 3);

	gtk_box_append(GTK_BOX(btns),
				   s_response_button(pSS, AP_STRING_ID_DLG_Spell_Ignore,
									 m_wDialog, SPELL_RESPONSE_IGNORE));
	gtk_box_append(GTK_BOX(btns),
				   s_response_button(pSS, AP_STRING_ID_DLG_Spell_IgnoreAll,
									 m_wDialog, SPELL_RESPONSE_IGNORE_ALL));
	gtk_box_append(GTK_BOX(btns),
				   s_response_button(pSS, AP_STRING_ID_DLG_Spell_AddToDict,
									 m_wDialog, SPELL_RESPONSE_ADD));

	/* ---- "Change to" entry ---- */
	GtkWidget * lbChangeTo = gtk_label_new(nullptr);
	localizeLabelUnderline(lbChangeTo, pSS,
						   AP_STRING_ID_DLG_Spell_ChangeTo);
	gtk_label_set_xalign(GTK_LABEL(lbChangeTo), 0.0);
	gtk_grid_attach(GTK_GRID(grid), lbChangeTo, 0, 2, 1, 1);

	GtkWidget * changeRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_grid_attach(GTK_GRID(grid), changeRow, 0, 3, 1, 1);

	m_eChange = gtk_entry_new();
	gtk_widget_set_hexpand(m_eChange, TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(lbChangeTo), m_eChange);
	gtk_box_append(GTK_BOX(changeRow), m_eChange);

	GtkWidget * btChange = s_response_button(pSS, AP_STRING_ID_DLG_Spell_Change,
										   m_wDialog, SPELL_RESPONSE_CHANGE);
	gtk_widget_set_hexpand(btChange, FALSE);
	gtk_box_append(GTK_BOX(changeRow), btChange);

	GtkWidget * btChangeAll = s_response_button(pSS, AP_STRING_ID_DLG_Spell_ChangeAll,
												m_wDialog, SPELL_RESPONSE_CHANGE_ALL);
	gtk_widget_set_hexpand(btChangeAll, FALSE);
	gtk_box_append(GTK_BOX(changeRow), btChangeAll);

	/* ---- Suggestions list ---- */
	GtkWidget * lbSugg = gtk_label_new(nullptr);
	localizeLabelUnderline(lbSugg, pSS, AP_STRING_ID_DLG_Spell_Suggestions);
	gtk_label_set_xalign(GTK_LABEL(lbSugg), 0.0);
	gtk_grid_attach(GTK_GRID(grid), lbSugg, 0, 4, 2, 1);

	m_lbSuggestions = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(m_lbSuggestions),
									GTK_SELECTION_SINGLE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(lbSugg), m_lbSuggestions);

	GtkWidget * scSugg = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scSugg),
								   GTK_POLICY_AUTOMATIC,
								   GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scSugg),
								  m_lbSuggestions);
	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scSugg),
											   120);
	gtk_widget_set_vexpand(scSugg, TRUE);
	gtk_grid_attach(GTK_GRID(grid), scSugg, 0, 5, 2, 1);

	m_selectHandlerID = g_signal_connect(m_lbSuggestions,
										 "row-selected",
										 G_CALLBACK(s_suggestion_selected),
										 this);
	g_signal_connect(m_lbSuggestions, "row-activated",
					 G_CALLBACK(s_suggestion_row_activated), this);
	m_changeHandlerID = g_signal_connect(m_eChange, "changed",
										 G_CALLBACK(s_entry_changed), this);

	/* ---- Close ---- */
	pSS->getValueUTF8(XAP_STRING_ID_DLG_Close, s);
	abiAddButton(GTK_DIALOG(m_wDialog), s, GTK_RESPONSE_CLOSE);

	return m_wDialog;
}

/*****************************************************************/
/* Per-word window update                                         */
/*****************************************************************/

void
AP_UnixDialog_Spell::_updateWindow (void)
{
	GtkTextBuffer * buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_txWrong));
	GtkTextIter iter2;

	gtk_text_buffer_set_text(buffer, "", -1);

	const UT_UCS4Char *p;
	UT_sint32 iLength;

	// insert start of sentence
	p = m_pWordIterator->getPreWord(iLength);
	if (0 < iLength)
	{
		gchar * preword = (gchar*) _convertToMB(p, iLength);
		gtk_text_buffer_set_text(buffer, preword, -1);
		FREEP(preword);
	}

	// insert misspelled word (red + bold tag)
	p = m_pWordIterator->getCurrentWord(iLength);
	gchar * word = (gchar*) _convertToMB(p, iLength);
	gtk_text_buffer_get_end_iter(buffer, &iter2);
	gtk_text_buffer_insert_with_tags(buffer, &iter2, word, -1,
									 m_pMisspellTag, nullptr);

	// insert end of sentence
	p = m_pWordIterator->getPostWord(iLength);
	if (0 < iLength)
	{
		gchar * postword = (gchar*) _convertToMB(p, iLength);
		gtk_text_buffer_get_end_iter(buffer, &iter2);
		gtk_text_buffer_insert(buffer, &iter2, postword, -1);
		FREEP(postword);
	}
	else
	{
		// Trailing space so the highlight tag visually closes (GTK
		// needs content after a tag for it to render).
		gtk_text_buffer_get_end_iter(buffer, &iter2);
		gtk_text_buffer_insert(buffer, &iter2, " ", -1);
	}

	/* ---- rebuild the suggestions list ---- */
	m_bUiUpdating = true;

	for (;;)
	{
		GtkWidget * child = gtk_widget_get_first_child(m_lbSuggestions);
		if (!child)
			break;
		gtk_list_box_remove(GTK_LIST_BOX(m_lbSuggestions), child);
	}

	GtkListBoxRow * firstRow = nullptr;

	if (!m_Suggestions || m_Suggestions->getItemCount() == 0)
	{
		const XAP_StringSet * pSS = m_pApp->getStringSet();
		std::string s;
		pSS->getValueUTF8(AP_STRING_ID_DLG_Spell_NoSuggestions, s);

		GtkWidget * label = gtk_label_new(s.c_str());
		gtk_label_set_xalign(GTK_LABEL(label), 0.0);
		gtk_widget_set_margin_start(label, 8);
		gtk_widget_set_margin_top(label, 4);
		gtk_widget_set_margin_bottom(label, 4);
		gtk_widget_set_sensitive(label, FALSE);
		gtk_list_box_append(GTK_LIST_BOX(m_lbSuggestions), label);

		gtk_editable_set_text(GTK_EDITABLE(m_eChange), word ? word : "");
	}
	else
	{
		for (UT_sint32 i = 0; i < m_Suggestions->getItemCount(); i++)
		{
			gchar * suggest = (gchar*) _convertToMB(
				(UT_UCS4Char*)m_Suggestions->getNthItem(i));
			GtkWidget * label = gtk_label_new(suggest ? suggest : "");
			gtk_label_set_xalign(GTK_LABEL(label), 0.0);
			gtk_widget_set_margin_start(label, 8);
			gtk_widget_set_margin_end(label, 8);
			gtk_widget_set_margin_top(label, 4);
			gtk_widget_set_margin_bottom(label, 4);
			gtk_list_box_append(GTK_LIST_BOX(m_lbSuggestions), label);
			FREEP(suggest);

			if (i == 0)
				firstRow = GTK_LIST_BOX_ROW(gtk_widget_get_parent(label));
		}

		gchar * suggest = (gchar*) _convertToMB(
			(UT_UCS4Char*)m_Suggestions->getNthItem(0));
		gtk_editable_set_text(GTK_EDITABLE(m_eChange),
							  suggest ? suggest : "");
		FREEP(suggest);
	}

	m_bUiUpdating = false;

	/* select first suggestion after the update flag clears */
	if (firstRow)
		gtk_list_box_select_row(GTK_LIST_BOX(m_lbSuggestions), firstRow);

	FREEP(word);
}

/*****************************************************************/
/* Button events                                                  */
/*****************************************************************/

void
AP_UnixDialog_Spell::onChangeClicked ()
{
	UT_UCS4Char * replace =
		_convertFromMB(XAP_gtk_entry_get_text(GTK_EDITABLE(m_eChange)));
	if (!replace || !UT_UCS4_strlen(replace))
	{
		FREEP(replace);
		return;
	}
	changeWordWith(replace);
	FREEP(replace);
}

void
AP_UnixDialog_Spell::onChangeAllClicked ()
{
	UT_UCS4Char * replace =
		_convertFromMB(XAP_gtk_entry_get_text(GTK_EDITABLE(m_eChange)));
	if (!replace || !UT_UCS4_strlen(replace))
	{
		FREEP(replace);
		return;
	}
	addChangeAll(replace);
	changeWordWith(replace);
	FREEP(replace);
}

void
AP_UnixDialog_Spell::onIgnoreClicked ()
{
	ignoreWord();
}

void
AP_UnixDialog_Spell::onIgnoreAllClicked ()
{
	addIgnoreAll();
	ignoreWord();
}

void
AP_UnixDialog_Spell::onAddClicked ()
{
	addToDict();
	ignoreWord();
}

/*!
* Selecting a suggestion copies it into the change entry.
*/
void
AP_UnixDialog_Spell::onSuggestionSelected ()
{
	if (m_bUiUpdating || !m_Suggestions || !m_Suggestions->getItemCount())
		return;

	GtkListBoxRow * row =
		gtk_list_box_get_selected_row(GTK_LIST_BOX(m_lbSuggestions));
	if (!row)
		return;

	GtkWidget * label = gtk_list_box_row_get_child(row);
	if (!label)
		return;

	m_bUiUpdating = true;
	gtk_editable_set_text(GTK_EDITABLE(m_eChange),
						  gtk_label_get_text(GTK_LABEL(label)));
	m_bUiUpdating = false;
}

/*!
* Double-click (row-activated) applies the suggestion as Change.
*/
void
AP_UnixDialog_Spell::onSuggestionActivated ()
{
	if (m_bUiUpdating)
		return;
	onSuggestionSelected();
	gtk_dialog_response(GTK_DIALOG(m_wDialog), SPELL_RESPONSE_CHANGE);
}

/*!
* Typing in the entry highlights the closest matching suggestion.
*/
void
AP_UnixDialog_Spell::onSuggestionChanged ()
{
	if (m_bUiUpdating)
		return;

	const gchar * modtext = XAP_gtk_entry_get_text(GTK_EDITABLE(m_eChange));
	if (!modtext || !*modtext)
	{
		gtk_list_box_unselect_all(GTK_LIST_BOX(m_lbSuggestions));
		return;
	}

	gsize modlen = strlen(modtext);

	GtkWidget * child = gtk_widget_get_first_child(m_lbSuggestions);
	while (child)
	{
		GtkWidget * label = gtk_list_box_row_get_child(GTK_LIST_BOX_ROW(child));
		const gchar * text = label ? gtk_label_get_text(GTK_LABEL(label))
								   : nullptr;
		if (text && g_ascii_strncasecmp(modtext, text, modlen) == 0)
		{
			gtk_list_box_select_row(GTK_LIST_BOX(m_lbSuggestions),
									GTK_LIST_BOX_ROW(child));
			return;
		}
		child = gtk_widget_get_next_sibling(child);
	}
	gtk_list_box_unselect_all(GTK_LIST_BOX(m_lbSuggestions));
}

/*****************************************************************/
/* Conversion helpers                                             */
/*****************************************************************/

char *
AP_UnixDialog_Spell::_convertToMB (const UT_UCS4Char *wword)
{
	if (!wword)
		return g_strdup("");
	UT_UCS4String ucs4(wword);
	return g_strdup(ucs4.utf8_str());
}

char *
AP_UnixDialog_Spell::_convertToMB (const UT_UCS4Char *wword,
								   UT_sint32 iLength)
{
	if (!wword || iLength <= 0)
		return g_strdup("");
	UT_UCS4String ucs4(wword, iLength);
	return g_strdup(ucs4.utf8_str());
}

UT_UCS4Char *
AP_UnixDialog_Spell::_convertFromMB (const char *word)
{
	if (!word)
		return nullptr;
	UT_UCS4Char * str = nullptr;
	UT_UCS4String ucs4(word);
	UT_UCS4_cloneString(&str, ucs4.ucs4_str());
	return str;
}
