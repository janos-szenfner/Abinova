/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
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

#include "config.h"

#include <string>
#include <vector>

#include "ap_UnixThesaurusPane.h"
#include "ap_UnixFrameImpl.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "fv_View.h"
#include "pp_AttrProp.h"
#include "ut_string_class.h"
#include "ut_debugmsg.h"

AP_UnixThesaurusPane::AP_UnixThesaurusPane(XAP_Frame * pFrame)
	: m_pFrame(pFrame)
	, m_wEntry(nullptr)
	, m_wList(nullptr)
	, m_wStatus(nullptr)
	, m_wReplace(nullptr)
	, m_wSelected(nullptr)
{
}

AP_UnixThesaurusPane::~AP_UnixThesaurusPane()
{
}

GtkWidget * AP_UnixThesaurusPane::createWidget()
{
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_size_request(box, 240, -1);

	/* header: title + close */
	GtkWidget * header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_margin_start(header, 8);
	gtk_widget_set_margin_end(header, 4);
	gtk_widget_set_margin_top(header, 4);
	gtk_widget_set_margin_bottom(header, 4);
	GtkWidget * title = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(title), "<b>Thesaurus</b>");
	gtk_label_set_xalign(GTK_LABEL(title), 0.0);
	gtk_widget_set_hexpand(title, TRUE);
	gtk_box_append(GTK_BOX(header), title);
	GtkWidget * close =
		gtk_button_new_from_icon_name("window-close-symbolic");
	gtk_button_set_has_frame(GTK_BUTTON(close), FALSE);
	g_signal_connect(close, "clicked",
					 G_CALLBACK(_s_close_clicked), this);
	gtk_box_append(GTK_BOX(header), close);
	gtk_box_append(GTK_BOX(box), header);
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	/* search row */
	GtkWidget * search = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_margin_start(search, 8);
	gtk_widget_set_margin_end(search, 8);
	gtk_widget_set_margin_top(search, 6);
	gtk_widget_set_margin_bottom(search, 6);
	m_wEntry = gtk_entry_new();
	gtk_entry_set_placeholder_text(GTK_ENTRY(m_wEntry),
								   "Word to look up");
	gtk_widget_set_hexpand(m_wEntry, TRUE);
	g_signal_connect(m_wEntry, "activate",
					 G_CALLBACK(_s_entry_activate), this);
	gtk_box_append(GTK_BOX(search), m_wEntry);
	GtkWidget * go =
		gtk_button_new_from_icon_name("edit-find-symbolic");
	gtk_widget_set_tooltip_text(go, "Look up synonyms");
	g_signal_connect(go, "clicked",
					 G_CALLBACK(_s_lookup_clicked), this);
	gtk_box_append(GTK_BOX(search), go);
	gtk_box_append(GTK_BOX(box), search);
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	/* results */
	GtkWidget * scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
								   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scroll, TRUE);
	m_wList = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(m_wList),
									GTK_SELECTION_SINGLE);
	gtk_list_box_set_placeholder(GTK_LIST_BOX(m_wList),
		gtk_label_new("Synonyms for the word at the insertion point"));
	gtk_list_box_set_activate_on_single_click(
		GTK_LIST_BOX(m_wList), FALSE);
	g_signal_connect(m_wList, "row-selected",
					 G_CALLBACK(_s_row_selected), this);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
								  m_wList);
	gtk_box_append(GTK_BOX(box), scroll);

	m_wStatus = gtk_label_new(nullptr);
	gtk_widget_add_css_class(m_wStatus, "dim-label");
	gtk_widget_set_margin_start(m_wStatus, 8);
	gtk_widget_set_margin_end(m_wStatus, 8);
	gtk_widget_set_margin_top(m_wStatus, 4);
	gtk_label_set_xalign(GTK_LABEL(m_wStatus), 0.0);
	gtk_box_append(GTK_BOX(box), m_wStatus);

	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	/* bottom bar: replace the looked-up word with the selection */
	GtkWidget * bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_margin_start(bar, 8);
	gtk_widget_set_margin_end(bar, 8);
	gtk_widget_set_margin_top(bar, 4);
	gtk_widget_set_margin_bottom(bar, 4);
	m_wReplace = gtk_button_new_with_label("Replace");
	gtk_widget_set_tooltip_text(m_wReplace,
		"Replace the word in the document with the selected synonym");
	gtk_widget_set_hexpand(m_wReplace, TRUE);
	gtk_widget_set_sensitive(m_wReplace, FALSE);
	g_signal_connect(m_wReplace, "clicked",
					 G_CALLBACK(_s_replace_clicked), this);
	gtk_box_append(GTK_BOX(bar), m_wReplace);
	gtk_box_append(GTK_BOX(box), bar);

	return box;
}

/* the language tag at the caret decides which th_<lang>_v2 file to
 * use; "en-US"-style tags are normalised inside UT_Thesaurus */
static std::string s_caretLang(FV_View * pView)
{
	std::string lang = "en_US";
	if (!pView)
		return lang;
	PP_PropertyVector props;
	if (pView->getCharFormat(props))
	{
		const std::string & l = PP_getAttribute("lang", props);
		if (!l.empty())
			lang = l;
	}
	return lang;
}

static const UT_Thesaurus * s_thesaurusFor(FV_View * pView)
{
	std::string dir;
	XAP_App * app = XAP_App::getApp();
	if (app)
		dir = app->getAbiSuiteLibDir() + std::string("/thesaurus");
	return UT_Thesaurus::forLanguage(s_caretLang(pView).c_str(),
									 dir.empty() ? nullptr : dir.c_str());
}

void AP_UnixThesaurusPane::lookupCaretWord()
{
	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	if (!pView || !m_wEntry)
		return;
	UT_UCS4String sWord;
	if (pView->getThesaurusWord(sWord) && sWord.length())
	{
		gtk_editable_set_text(GTK_EDITABLE(m_wEntry),
							  sWord.utf8_str());
		_lookup(sWord.utf8_str());
	}
	else
		_lookup(gtk_editable_get_text(GTK_EDITABLE(m_wEntry)));
}

void AP_UnixThesaurusPane::_lookup(const char * szWord)
{
	m_meanings.clear();
	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	const UT_Thesaurus * thes = s_thesaurusFor(pView);
	if (thes && szWord && *szWord)
		thes->lookup(szWord, m_meanings);

	if (!thes)
		gtk_label_set_text(GTK_LABEL(m_wStatus),
						   "No thesaurus data for this language");
	else if (!szWord || !*szWord)
		gtk_label_set_text(GTK_LABEL(m_wStatus),
						   "No word at the insertion point");
	else if (m_meanings.empty())
		gtk_label_set_text(GTK_LABEL(m_wStatus), "Word not found");
	else
		gtk_label_set_text(GTK_LABEL(m_wStatus), "");
	_rebuild();
}

GtkWidget * AP_UnixThesaurusPane::_synonymRow(const std::string & term)
{
	GtkWidget * l = gtk_label_new(term.c_str());
	gtk_label_set_xalign(GTK_LABEL(l), 0.0);
	gtk_label_set_wrap(GTK_LABEL(l), TRUE);
	gtk_widget_set_margin_start(l, 16);
	gtk_widget_set_margin_end(l, 8);
	gtk_widget_set_margin_top(l, 2);
	gtk_widget_set_margin_bottom(l, 2);
	g_object_set_data_full(G_OBJECT(l), "term",
						   g_strdup(term.c_str()), g_free);
	return l;
}

static GtkWidget * s_headerRow(const char * szText)
{
	GtkWidget * l = gtk_label_new(nullptr);
	gchar * esc = g_markup_escape_text(szText, -1);
	gchar * markup = g_strdup_printf("<b>%s</b>", esc);
	g_free(esc);
	gtk_label_set_markup(GTK_LABEL(l), markup);
	g_free(markup);
	gtk_label_set_xalign(GTK_LABEL(l), 0.0);
	gtk_widget_set_margin_start(l, 8);
	gtk_widget_set_margin_top(l, 6);
	gtk_widget_add_css_class(l, "dim-label");
	/* headers must not be selectable as the replacement term */
	GtkWidget * row = gtk_list_box_row_new();
	gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
	gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
	gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), l);
	return row;
}

void AP_UnixThesaurusPane::_rebuild()
{
	if (!m_wList)
		return;
	GtkListBoxRow * old;
	while ((old = gtk_list_box_get_row_at_index(
				GTK_LIST_BOX(m_wList), 0)))
		gtk_list_box_remove(GTK_LIST_BOX(m_wList), GTK_WIDGET(old));
	m_wSelected = nullptr;
	gtk_widget_set_sensitive(m_wReplace, FALSE);

	for (const UT_Thesaurus::Meaning & m : m_meanings)
	{
		std::string head = m.pos.empty() ? "Meaning" : m.pos;
		gtk_list_box_append(GTK_LIST_BOX(m_wList),
							s_headerRow(head.c_str()));
		for (const std::string & s : m.synonyms)
			gtk_list_box_append(GTK_LIST_BOX(m_wList),
								_synonymRow(s));
		if (!m.antonyms.empty())
		{
			gtk_list_box_append(GTK_LIST_BOX(m_wList),
								s_headerRow("Antonyms"));
			for (const std::string & s : m.antonyms)
				gtk_list_box_append(GTK_LIST_BOX(m_wList),
									_synonymRow(s));
		}
	}
}

std::string AP_UnixThesaurusPane::_selectedTerm() const
{
	if (!m_wSelected)
		return std::string();
	const char * t = static_cast<const char *>(
		g_object_get_data(G_OBJECT(m_wSelected), "term"));
	return t ? t : "";
}

void AP_UnixThesaurusPane::_s_lookup_clicked(GtkButton * /*btn*/,
											 gpointer data)
{
	AP_UnixThesaurusPane * self =
		static_cast<AP_UnixThesaurusPane *>(data);
	self->_lookup(gtk_editable_get_text(GTK_EDITABLE(self->m_wEntry)));
}

void AP_UnixThesaurusPane::_s_entry_activate(GtkEntry * entry,
											 gpointer data)
{
	AP_UnixThesaurusPane * self =
		static_cast<AP_UnixThesaurusPane *>(data);
	self->_lookup(gtk_editable_get_text(GTK_EDITABLE(entry)));
}

void AP_UnixThesaurusPane::_s_row_selected(GtkListBox * /*box*/,
										   GtkListBoxRow * row,
										   gpointer data)
{
	AP_UnixThesaurusPane * self =
		static_cast<AP_UnixThesaurusPane *>(data);
	if (!self || !row)
		return;
	GtkWidget * child = gtk_list_box_row_get_child(row);
	const char * term = child
		? static_cast<const char *>(
			g_object_get_data(G_OBJECT(child), "term"))
		: nullptr;
	self->m_wSelected = term ? child : nullptr;
	gtk_widget_set_sensitive(self->m_wReplace, term != nullptr);
}

/* pane buttons take keyboard focus on click; hand it back to the
 * document canvas so the next keystroke reaches the view */
static void s_focusDoc(XAP_Frame * pFrame)
{
	AP_UnixFrameImpl * pImpl = pFrame
		? static_cast<AP_UnixFrameImpl *>(pFrame->getFrameImpl())
		: nullptr;
	if (pImpl)
		pImpl->focusDocument();
}

void AP_UnixThesaurusPane::_s_replace_clicked(GtkButton * /*btn*/,
											  gpointer data)
{
	AP_UnixThesaurusPane * self =
		static_cast<AP_UnixThesaurusPane *>(data);
	FV_View * pView = self->m_pFrame
		? static_cast<FV_View *>(self->m_pFrame->getCurrentView())
		: nullptr;
	if (!pView)
		return;
	const std::string term = self->_selectedTerm();
	if (term.empty())
		return;
	UT_UCS4String u(term);
	pView->replaceThesaurusWord(u.ucs4_str(), u.length());
	s_focusDoc(self->m_pFrame);
}

void AP_UnixThesaurusPane::_s_close_clicked(GtkButton * /*btn*/,
											gpointer data)
{
	AP_UnixThesaurusPane * self =
		static_cast<AP_UnixThesaurusPane *>(data);
	if (!self || !self->m_pFrame)
		return;
	AP_UnixFrameImpl * pImpl = static_cast<AP_UnixFrameImpl *>(
		self->m_pFrame->getFrameImpl());
	if (pImpl)
		pImpl->setThesaurusPaneVisible(false);
}
