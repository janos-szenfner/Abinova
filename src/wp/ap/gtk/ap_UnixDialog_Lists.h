/* Abinova — bullets & numbering dialog (GTK4)
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

#include <vector>
#include <string>

#include "xap_UnixDialog.h"
#include "ap_Dialog_Lists.h"
#include "ut_timer.h"

class XAP_UnixFrame;
class GR_CairoGraphics;

/*****************************************************************/

class AP_UnixDialog_Lists
    : public AP_Dialog_Lists
    , public XAP_UnixDialog
{
 public:
	AP_UnixDialog_Lists(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id);
	virtual ~AP_UnixDialog_Lists(void);

	static XAP_Dialog *		static_constructor(XAP_DialogFactory *, XAP_Dialog_Id id);

	virtual void runModeless(XAP_Frame * pFrame) override;
	virtual void destroy(void) override;
	virtual void activate(void) override;
	virtual void notifyActiveFrame(XAP_Frame *pFrame) override;
	virtual void runModal(XAP_Frame * pFrame) override;
	/* CALLBACKS */

	void					customChanged(void);
	void					applyClicked(void);
	void					closeClicked(void);
	void					styleChanged(gint style);
	void					previewInvalidate(void);
	void					previewDraw(cairo_t *cr);
	void					setFoldLevel(UT_sint32 iLevel, bool bSet);

	/* Just Plain Useful Functions */

	void					setListTypeFromWidget(void);
	void					setXPFromLocal(void);
	void					loadXPDataIntoLocal(void);
	void					updateFromDocument(void);
	void					setAllSensitivity(void);
	void					updateDialog(void);
	bool					dontUpdate(void);
	static void				autoupdateLists(UT_Worker * pTimer);
	virtual bool			isPageLists(void) const override;
	virtual void			setFoldLevelInGUI(void) override;

	void					teardown(void);

	/* for the drop-down notify trampolines */
	GtkWidget *				typeDrop(void) const { return m_wTypeDrop; }

 protected:
	virtual GtkWidget* _constructWindow(void);
	GtkWidget *				_constructWindowContents(void);
	GtkWidget *				_constructListsPage(void);
	GtkWidget *				_constructFoldingPage(void);
	GtkWidget *				_constructPreview(void);
	void					_setRadioButtonLabels(void);
	void					_connectSignals(void);
	void					_gatherData(void);
	void					_getGlistFonts(std::vector<std::string> & glFonts);
	void					_fillFontDrop(void);
	void					_setStyleModel(gint which);

	inline GtkWidget *		_getMainWindow(void) { return m_windowMain; }

 private:
	enum ResponseId: int8_t
	{
		BUTTON_OK = GTK_RESPONSE_OK,
		BUTTON_CANCEL = GTK_RESPONSE_CANCEL,
		BUTTON_CLOSE = GTK_RESPONSE_CLOSE,
		BUTTON_APPLY = GTK_RESPONSE_APPLY,
		BUTTON_RESET
	};

	std::vector<std::string>	m_glFonts;
	GR_CairoGraphics *			m_pPreviewWidget;

	bool						m_bManualListStyle;
	bool						m_bDestroy_says_stopupdating;
	bool						m_bAutoUpdate_happening_now;
	bool						m_bDontUpdate;
	UT_Timer *					m_pAutoUpdateLists;

	GtkWidget *	m_wApply;
	GtkWidget *	m_wClose;
	GtkWidget *	m_wContents;
	GtkWidget *	m_wStartNewList;
	GtkWidget *	m_wApplyCurrent;
	GtkWidget *	m_wStartSubList;
	GtkWidget *	m_wPreviewArea;
	GtkWidget *	m_wDelimEntry;
	GtkWidget *	m_wDecimalEntry;
	GtkWidget *	m_wAlignListSpin;
	GtkWidget *	m_wIndentAlignSpin;
	GtkWidget *	m_wFontDrop;			/* GtkDropDown */
	GtkWidget *	m_wCustomGrid;
	GtkWidget *	m_wStyleDrop;			/* GtkDropDown */
	GtkWidget *	m_wTypeDrop;			/* GtkDropDown */
	GtkWidget *	m_wStartSpin;
	GtkWidget *	m_wResetButton;

	/* NOTE: GtkDropDown models are fully owned by GTK here — this
	 * object never stores a model pointer. GTK 4.14 crashes when a
	 * GListModel instance that was previously attached to a
	 * GtkDropDown is re-set, so a fresh GtkStringList is created on
	 * every style-model swap. */

	/* the FL_ListType table matching the current style model order */
	const FL_ListType *	m_curStyleTypes;
	UT_sint32			m_curStyleTypeCount;

	gulong m_idStyleChanged;
	gulong m_idTypeChanged;
	gulong m_idFontChanged;
	gulong m_idDelimChanged;
	gulong m_idDecimalChanged;
	gulong m_idStartChanged;
	gulong m_idAlignChanged;
	gulong m_idIndentChanged;

	UT_sint32  m_iPageLists;
	UT_sint32  m_iPageFold;
	UT_GenericVector<GtkWidget*>  m_vecFoldCheck;
	UT_NumberVector  m_vecFoldID;
};
