/* Abinova
 * Copyright (C) 2026 AbiSource, Inc.
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

#pragma once

#include <gtk/gtk.h>

#include "ap_Dialog_Document.h"
#include "ut_units.h"

class XAP_UnixFrame;
class FV_View;

/* GTK implementation of the Word-style Document dialog: a notebook
 * with Margins and Layout pages plus Page Setup…, Default… and
 * Print… actions.
 */
class AP_UnixDialog_Document : public AP_Dialog_Document
{
public:
	AP_UnixDialog_Document(XAP_DialogFactory * pDlgFactory,
						   XAP_Dialog_Id id);
	virtual ~AP_UnixDialog_Document();

	virtual void runModal(XAP_Frame * pFrame) override;

	static XAP_Dialog * static_constructor(XAP_DialogFactory *,
										   XAP_Dialog_Id id);

protected:
	GtkWidget *		_constructWindow();
	GtkWidget *		_constructMarginsPage();
	GtkWidget *		_constructLayoutPage();
	GtkWidget *		_unitSpin(const char * szLabel, GtkWidget * grid,
							  int row, float value);
	void			_readWidgets();
	void			_doPageSetup();
	void			_doDefault();
	void			_doPrint();
	void			_doLineNumbers();
	void			_doBorders();
	void			_writeDefaultTemplate();
	void			_redrawPreview();

	static void		_s_response(GtkDialog * dlg, gint resp, gpointer data);
	static void		_s_default_response(GtkDialog * dlg, gint resp,
										gpointer data);
	static void		_s_spin_changed(GtkSpinButton * spin, gpointer data);
	static void		_s_combo_changed(GtkDropDown * dd, GParamSpec *,
									 gpointer data);
	static void		_s_preview_draw(GtkDrawingArea * area, cairo_t * cr,
									int w, int h, gpointer data);

	GtkWidget *		m_wMainWindow;
	GtkWidget *		m_wNotebook;
	GtkWidget *		m_wSpinTop;
	GtkWidget *		m_wSpinBottom;
	GtkWidget *		m_wSpinLeft;
	GtkWidget *		m_wSpinRight;
	GtkWidget *		m_wSpinGutter;
	GtkWidget *		m_wGutterPos;
	GtkWidget *		m_wMultiPage;
	GtkWidget *		m_wApplyTo;
	GtkWidget *		m_wPreview;
	GtkWidget *		m_wSectionStart;
	GtkWidget *		m_wOddEven;
	GtkWidget *		m_wFirstPage;
	GtkWidget *		m_wSpinHeader;
	GtkWidget *		m_wSpinFooter;
	GtkWidget *		m_wVAlign;
	bool			m_bWantPrint;
};

/* standalone sub-dialogs shared with the Layout ribbon popovers */
void ap_showLineNumbersDialog(GtkWindow * parent, FV_View * pView);
void ap_showHyphenationDialog(GtkWindow * parent, FV_View * pView);
