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

#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>

#include "ut_Thesaurus.h"

class XAP_Frame;

/* Word-style Thesaurus task pane in the side deck (Review >
 * Proofing > Thesaurus): looks up the word under the insertion
 * point (or the text typed into the search row) in the offline
 * MyThes data and offers a case-preserving Replace. */
class AP_UnixThesaurusPane
{
public:
	AP_UnixThesaurusPane(XAP_Frame * pFrame);
	~AP_UnixThesaurusPane();

	GtkWidget *	createWidget();
	/* seed the search row with the word at the caret and run the
	 * lookup — called when the pane is shown */
	void		lookupCaretWord();

private:
	void		_lookup(const char * szWord);
	void		_rebuild();
	GtkWidget *	_synonymRow(const std::string & term);
	std::string	_selectedTerm() const;
	static void _s_lookup_clicked(GtkButton * btn, gpointer data);
	static void _s_entry_activate(GtkEntry * entry, gpointer data);
	static void _s_row_selected(GtkListBox * box, GtkListBoxRow * row,
							  gpointer data);
	static void _s_replace_clicked(GtkButton * btn, gpointer data);
	static void _s_close_clicked(GtkButton * btn, gpointer data);

	XAP_Frame *	m_pFrame;
	GtkWidget * m_wEntry;
	GtkWidget * m_wList;
	GtkWidget * m_wStatus;
	GtkWidget * m_wReplace;
	GtkWidget * m_wSelected;
	std::vector<UT_Thesaurus::Meaning> m_meanings;
};
