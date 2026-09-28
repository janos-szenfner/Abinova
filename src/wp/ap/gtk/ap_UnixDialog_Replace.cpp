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

//////////////////////////////////////////////////////////////////
// THIS CODE RUNS BOTH THE "Find" AND THE "Find-Replace" DIALOGS.
//////////////////////////////////////////////////////////////////

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
#include "ap_Dialog_Replace.h"
#include "ap_UnixDialog_Replace.h"

XAP_Dialog * AP_UnixDialog_Replace::static_constructor(XAP_DialogFactory * pFactory,
													   XAP_Dialog_Id id)
{
	return new AP_UnixDialog_Replace(pFactory,id);
}

AP_UnixDialog_Replace::AP_UnixDialog_Replace(XAP_DialogFactory * pDlgFactory,
											   XAP_Dialog_Id id)
	: AP_Dialog_Replace(pDlgFactory,id)
	, m_buttonFindNext(nullptr)
	, m_buttonFindPrev(nullptr)
	, m_buttonFindReplace(nullptr)
	, m_buttonReplaceAll(nullptr)
	, m_entryFind(nullptr)
	, m_entryReplace(nullptr)
	, m_historyFind(nullptr)
	, m_historyReplace(nullptr)
	, m_menuBtnFind(nullptr)
	, m_menuBtnReplace(nullptr)
	, m_checkbuttonMatchCase(nullptr)
	, m_checkbuttonWholeWord(nullptr)
{
}

AP_UnixDialog_Replace::~AP_UnixDialog_Replace(void)
{
}

/*****************************************************************/
/* Signal trampolines                                             */
/*****************************************************************/

static void s_response_triggered(GtkWidget * widget, gint resp,
								 AP_UnixDialog_Replace * dlg)
{
	UT_return_if_fail(widget && dlg);

	switch (resp)
	{
		case AP_UnixDialog_Replace::BUTTON_FIND_NEXT:
			dlg->event_FindNext();
			break;
		case AP_UnixDialog_Replace::BUTTON_FIND_PREV:
			dlg->event_FindPrev();
			break;
		case AP_UnixDialog_Replace::BUTTON_REPLACE:
			dlg->event_Replace();
			break;
		case AP_UnixDialog_Replace::BUTTON_REPLACE_ALL:
			dlg->event_ReplaceAll();
			break;
		default:
			// modeless: run destroy() so the dialog is unregistered;
			// GTK4's gtk_window_destroy emits no signal to chain off
			if (dlg->isRunning())
				dlg->destroy();
			else
				abiDestroyWidget(widget);
			break;
	}
}

static void s_find_entry_activate(GtkWidget * /*w*/, AP_UnixDialog_Replace * dlg)
{
	dlg->event_FindNext();
}

static void s_find_entry_change(GtkWidget * /*w*/, AP_UnixDialog_Replace * dlg)
{
	dlg->event_FindEntryChange();
}

static void s_replace_entry_activate(GtkWidget * /*w*/, AP_UnixDialog_Replace * dlg)
{
	dlg->event_Replace();
}

static void s_option_toggled(GtkWidget * /*w*/, AP_UnixDialog_Replace * dlg)
{
	dlg->event_OptionsChanged();
}

static gboolean s_close_request(GtkWidget * /*w*/, AP_UnixDialog_Replace * dlg)
{
	dlg->event_Cancel();
	return TRUE;
}

/* A history row is activated: put its text into the paired entry and
 * close the popover.  The entry pointer is stashed on the row. */
static void s_history_row_activated(GtkListBox * /*box*/, GtkListBoxRow * row,
									gpointer /*data*/)
{
	GtkWidget * entry = GTK_WIDGET(
		g_object_get_data(G_OBJECT(row), "abi-target-entry"));
	GtkWidget * label = gtk_list_box_row_get_child(row);
	if (!entry || !label)
		return;
	gtk_editable_set_text(GTK_EDITABLE(entry),
						  gtk_label_get_text(GTK_LABEL(label)));
	gtk_editable_set_position(GTK_EDITABLE(entry), -1);

	GtkWidget * pop = gtk_widget_get_ancestor(GTK_WIDGET(row),
											  GTK_TYPE_POPOVER);
	if (pop)
		gtk_popover_popdown(GTK_POPOVER(pop));
}

/*****************************************************************/
/* Dialog protocol                                                */
/*****************************************************************/

void AP_UnixDialog_Replace::activate(void)
{
	if (!m_windowMain)
		return;
	ConstructWindowName();
	gtk_window_set_title (GTK_WINDOW (m_windowMain), m_WindowName);
	XAP_gtk_window_raise(m_windowMain);
}

void AP_UnixDialog_Replace::notifyActiveFrame(XAP_Frame * /*pFrame*/)
{
	if (!m_windowMain)
		return;
	ConstructWindowName();
	gtk_window_set_title (GTK_WINDOW (m_windowMain), m_WindowName);
}

void AP_UnixDialog_Replace::runModeless(XAP_Frame * pFrame)
{
	GtkWidget * mainWindow = _constructWindow();
	UT_return_if_fail(mainWindow);

	abiSetupModelessDialog (GTK_DIALOG(mainWindow), pFrame, this,
							BUTTON_CANCEL);

	// Populate the window's data items
	_populateWindowData();

	// this dialog needs this
	setView(static_cast<FV_View *> (getActiveFrame()->getCurrentView()));
}

/*****************************************************************/
/* Widget helpers                                                 */
/*****************************************************************/

UT_UCS4String AP_UnixDialog_Replace::_entryText(GtkWidget * entry) const
{
	if (!entry)
		return UT_UCS4String();
	return XAP_gtk_entry_get_text(GTK_EDITABLE(entry));
}

void AP_UnixDialog_Replace::_syncStringsFromWidgets(void)
{
	UT_UCS4String findText = _entryText(m_entryFind);
	setFindString(findText.ucs4_str());

	if (m_id == (XAP_Dialog_Id)AP_DIALOG_ID_REPLACE && m_entryReplace)
	{
		UT_UCS4String replaceText = _entryText(m_entryReplace);
		setReplaceString(replaceText.ucs4_str());
	}
}

void AP_UnixDialog_Replace::_updateSensitivity(void)
{
	bool enable = !_entryText(m_entryFind).empty();
	gtk_widget_set_sensitive(m_buttonFindNext, enable);
	gtk_widget_set_sensitive(m_buttonFindPrev, enable);
	if (m_buttonFindReplace)
		gtk_widget_set_sensitive(m_buttonFindReplace, enable);
	if (m_buttonReplaceAll)
		gtk_widget_set_sensitive(m_buttonReplaceAll, enable);
}

/*****************************************************************/
/* Events                                                         */
/*****************************************************************/

void AP_UnixDialog_Replace::event_FindNext(void)
{
	UT_UCS4String findText = _entryText(m_entryFind);
	if (findText.empty())
		return;

	_syncStringsFromWidgets();
	findNext();
}

void AP_UnixDialog_Replace::event_FindPrev(void)
{
	UT_UCS4String findText = _entryText(m_entryFind);
	if (findText.empty())
		return;

	_syncStringsFromWidgets();
	findPrev();
}

void AP_UnixDialog_Replace::event_Replace(void)
{
	UT_UCS4String findText = _entryText(m_entryFind);
	if (findText.empty())
		return;

	_syncStringsFromWidgets();
	findReplace();
}

void AP_UnixDialog_Replace::event_ReplaceAll(void)
{
	UT_UCS4String findText = _entryText(m_entryFind);
	if (findText.empty())
		return;

	_syncStringsFromWidgets();
	findReplaceAll();
}

void AP_UnixDialog_Replace::event_FindEntryChange(void)
{
	_updateSensitivity();
}

void AP_UnixDialog_Replace::event_OptionsChanged(void)
{
	setMatchCase(gtk_check_button_get_active(
					 GTK_CHECK_BUTTON(m_checkbuttonMatchCase)));
	setWholeWord(gtk_check_button_get_active(
					 GTK_CHECK_BUTTON(m_checkbuttonWholeWord)));
}

void AP_UnixDialog_Replace::event_Cancel(void)
{
	m_answer = AP_Dialog_Replace::a_CANCEL;
	destroy();
}

void AP_UnixDialog_Replace::destroy(void)
{
	_storeWindowData();
	modeless_cleanup();
	abiDestroyWidget(m_windowMain);
	m_windowMain = nullptr;
}

/*****************************************************************/
/* Construction                                                   */
/*****************************************************************/

/* Build an entry + optional history drop-down (a flat menu button
 * with a list box in its popover).  GTK4 has no combo-with-entry;
 * this is the modern equivalent. */
static GtkWidget * s_entry_with_history(GtkWidget *& listBoxOut,
										GtkWidget *& menuBtnOut,
										GtkWidget * entry)
{
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_hexpand(box, TRUE);

	gtk_widget_set_hexpand(entry, TRUE);
	gtk_box_append(GTK_BOX(box), entry);

	GtkWidget * pop = gtk_popover_new();
	GtkWidget * list = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(list),
									GTK_SELECTION_SINGLE);
	g_signal_connect(list, "row-activated",
					 G_CALLBACK(s_history_row_activated), nullptr);
	gtk_popover_set_child(GTK_POPOVER(pop), list);

	GtkWidget * btn = gtk_menu_button_new();
	gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(btn),
								  "pan-down-symbolic");
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(btn), pop);
	gtk_widget_set_tooltip_text(btn, "Search history");
	gtk_widget_add_css_class(btn, "flat");
	gtk_widget_set_visible(btn, FALSE);	/* shown when history exists */
	gtk_box_append(GTK_BOX(box), btn);

	listBoxOut = list;
	menuBtnOut = btn;
	return box;
}

GtkWidget * AP_UnixDialog_Replace::_constructWindow(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();
	const bool bReplaceMode = (m_id == (XAP_Dialog_Id)AP_DIALOG_ID_REPLACE);

	ConstructWindowName();
	m_windowMain = abiDialogNew("find dialog", FALSE);
	/* title set explicitly: m_WindowName can contain printf-unsafe
	 * characters from the document name */
	gtk_window_set_title(GTK_WINDOW(m_windowMain), m_WindowName);
	GtkWidget * content = gtk_dialog_get_content_area(GTK_DIALOG(m_windowMain));

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

	char * unixstr = nullptr;

	/* ---- Find row ---- */
	GtkWidget * labelFind = gtk_label_new(nullptr);
	CONVERT_TO_UNIX_STRING(dummy, AP_STRING_ID_DLG_FR_FindLabel, unixstr);
	gtk_label_set_text(GTK_LABEL(labelFind), unixstr);
	gtk_label_set_xalign(GTK_LABEL(labelFind), 0.0);
	gtk_grid_attach(GTK_GRID(grid), labelFind, 0, 0, 1, 1);

	m_entryFind = gtk_entry_new();
	gtk_widget_set_size_request(m_entryFind, 260, -1);
	gtk_grid_attach(GTK_GRID(grid),
					s_entry_with_history(m_historyFind, m_menuBtnFind,
										 m_entryFind),
					1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(labelFind), m_entryFind);

	/* ---- Replace row ---- */
	if (bReplaceMode)
	{
		GtkWidget * labelReplace = gtk_label_new(nullptr);
		CONVERT_TO_UNIX_STRING(dummy, AP_STRING_ID_DLG_FR_ReplaceWithLabel,
							   unixstr);
		gtk_label_set_text(GTK_LABEL(labelReplace), unixstr);
		gtk_label_set_xalign(GTK_LABEL(labelReplace), 0.0);
		gtk_grid_attach(GTK_GRID(grid), labelReplace, 0, 1, 1, 1);

		m_entryReplace = gtk_entry_new();
		gtk_grid_attach(GTK_GRID(grid),
						s_entry_with_history(m_historyReplace,
											 m_menuBtnReplace,
											 m_entryReplace),
						1, 1, 1, 1);
		gtk_label_set_mnemonic_widget(GTK_LABEL(labelReplace), m_entryReplace);
	}

	/* ---- Options row ---- */
	GtkWidget * opts = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
	gtk_grid_attach(GTK_GRID(grid), opts, 0, 2, 2, 1);

	CONVERT_TO_ACC_STRING(dummy, AP_STRING_ID_DLG_FR_MatchCase, unixstr);
	m_checkbuttonMatchCase = gtk_check_button_new_with_mnemonic(unixstr);
	gtk_box_append(GTK_BOX(opts), m_checkbuttonMatchCase);

	CONVERT_TO_ACC_STRING(dummy, AP_STRING_ID_DLG_FR_WholeWord, unixstr);
	m_checkbuttonWholeWord = gtk_check_button_new_with_mnemonic(unixstr);
	gtk_box_append(GTK_BOX(opts), m_checkbuttonWholeWord);

	/* ---- Action buttons ---- */
	std::string s;

	pSS->getValueUTF8(AP_STRING_ID_DLG_FR_FindNextButton, s);
	m_buttonFindNext = abiAddButton(GTK_DIALOG(m_windowMain), s,
									BUTTON_FIND_NEXT);

	/* No translated "Find Previous" string exists in the string set;
	 * the & is the mnemonic marker converted by abiAddButton. */
	m_buttonFindPrev = abiAddButton(GTK_DIALOG(m_windowMain),
									std::string("Find Pre&vious"),
									BUTTON_FIND_PREV);

	if (bReplaceMode)
	{
		pSS->getValueUTF8(AP_STRING_ID_DLG_FR_ReplaceButton, s);
		m_buttonFindReplace = abiAddButton(GTK_DIALOG(m_windowMain), s,
										   BUTTON_REPLACE);

		pSS->getValueUTF8(AP_STRING_ID_DLG_FR_ReplaceAllButton, s);
		m_buttonReplaceAll = abiAddButton(GTK_DIALOG(m_windowMain), s,
										BUTTON_REPLACE_ALL);
	}

	pSS->getValueUTF8(XAP_STRING_ID_DLG_Close, s);
	abiAddButton(GTK_DIALOG(m_windowMain), s, BUTTON_CANCEL);

	FREEP(unixstr);

	/* Direction is explicit in the Previous/Next buttons; the legacy
	 * "reverse find" mode flag stays off so Replace always moves
	 * forward. */
	setReverseFind(false);

	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_checkbuttonMatchCase),
								getMatchCase());
	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_checkbuttonWholeWord),
								getWholeWord());

	/* ---- Signals ---- */
	connectBasicSignals();
	g_signal_connect(G_OBJECT(m_windowMain), "response",
					 G_CALLBACK(s_response_triggered), this);

	g_signal_connect(G_OBJECT(m_entryFind), "activate",
					 G_CALLBACK(s_find_entry_activate), this);
	g_signal_connect(G_OBJECT(m_entryFind), "changed",
					 G_CALLBACK(s_find_entry_change), this);
	if (m_entryReplace)
		g_signal_connect(G_OBJECT(m_entryReplace), "activate",
						 G_CALLBACK(s_replace_entry_activate), this);

	g_signal_connect(G_OBJECT(m_checkbuttonMatchCase), "toggled",
					 G_CALLBACK(s_option_toggled), this);
	g_signal_connect(G_OBJECT(m_checkbuttonWholeWord), "toggled",
					 G_CALLBACK(s_option_toggled), this);

	g_signal_connect(G_OBJECT(m_windowMain), "close-request",
					 G_CALLBACK(s_close_request), this);

	_updateSensitivity();
	gtk_window_set_default_widget(GTK_WINDOW(m_windowMain),
								  m_buttonFindNext);

	return m_windowMain;
}

void AP_UnixDialog_Replace::_populateWindowData(void)
{
	UT_ASSERT(m_entryFind && m_checkbuttonMatchCase);

	// restore the most recent find/replace strings
	{
		UT_UCS4Char * str = getFindString();
		if (str)
		{
			UT_UCS4String ucs(str);
			gtk_editable_set_text(GTK_EDITABLE(m_entryFind),
								  ucs.utf8_str());
			FREEP(str);
		}
	}

	if (m_entryReplace)
	{
		UT_UCS4Char * str = getReplaceString();
		if (str)
		{
			UT_UCS4String ucs(str);
			gtk_editable_set_text(GTK_EDITABLE(m_entryReplace),
								  ucs.utf8_str());
			FREEP(str);
		}
	}

	_updateLists();
	_updateSensitivity();

	// Find entry should have focus, for immediate typing
	gtk_widget_grab_focus(m_entryFind);
}

void AP_UnixDialog_Replace::_storeWindowData(void)
{
	// The XP layer already persists find/replace state on every action.
}

void AP_UnixDialog_Replace::_updateLists()
{
	_updateList(GTK_LIST_BOX(m_historyFind), m_entryFind, &m_findList);
	if (m_historyReplace)
		_updateList(GTK_LIST_BOX(m_historyReplace), m_entryReplace,
					&m_replaceList);
	gtk_widget_set_visible(m_menuBtnFind,
						   m_findList.getItemCount() > 0);
	if (m_menuBtnReplace)
		gtk_widget_set_visible(m_menuBtnReplace,
							   m_replaceList.getItemCount() > 0);
}

void AP_UnixDialog_Replace::_updateList(GtkListBox* history,
										GtkWidget * entry,
										UT_GenericVector<UT_UCS4Char*>* list)
{
	if (!history || !list)
		return;

	/* clear existing rows */
	for (;;)
	{
		GtkWidget * child = gtk_widget_get_first_child(GTK_WIDGET(history));
		if (!child)
			break;
		gtk_list_box_remove(history, child);
	}

	for (UT_sint32 i = 0; i < list->getItemCount(); i++)
	{
		UT_UCS4Char * item = list->getNthItem(i);
		if (!item)
			continue;
		UT_UCS4String ucs4s(item);
		GtkWidget * label = gtk_label_new(ucs4s.utf8_str());
		gtk_label_set_xalign(GTK_LABEL(label), 0.0);
		gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
		gtk_widget_set_margin_start(label, 8);
		gtk_widget_set_margin_end(label, 8);
		gtk_widget_set_margin_top(label, 4);
		gtk_widget_set_margin_bottom(label, 4);
		gtk_list_box_append(history, label);
		GtkWidget * row = gtk_widget_get_parent(label);
		if (row)
			g_object_set_data(G_OBJECT(row), "abi-target-entry", entry);
	}
}
