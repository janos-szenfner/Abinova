/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
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

#include <gtk/gtk.h>

#include <stdlib.h>
#include <string.h>

#include "ut_string.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "xap_UnixDialogHelper.h"
#include "xap_GtkSignalBlocker.h"
#include "xap_GtkUtils.h"

#include "xap_Dialog_Id.h"
#include "xap_UnixApp.h"
#include "xap_UnixFrameImpl.h"
#include "xap_Frame.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"
#include "ap_Dialog_Lists.h"
#include "ap_UnixDialog_Lists.h"
#include "fp_Line.h"
#include "fp_Column.h"

#include "gr_UnixCairoGraphics.h"

/*****************************************************************/
/* List-type tables — the order of these arrays defines the        */
/* position of each entry in the style drop-down.                  */
/*****************************************************************/

static const FL_ListType s_noneTypes[] =
{
	NOT_A_LIST
};

static const FL_ListType s_numberedTypes[] =
{
	NUMBERED_LIST,
	LOWERCASE_LIST,
	UPPERCASE_LIST,
	LOWERROMAN_LIST,
	UPPERROMAN_LIST,
	ARABICNUMBERED_LIST,
	HEBREW_LIST
};

static const FL_ListType s_bulletedTypes[] =
{
	BULLETED_LIST,
	DASHED_LIST,
	SQUARE_LIST,
	TRIANGLE_LIST,
	DIAMOND_LIST,
	STAR_LIST,
	IMPLIES_LIST,
	TICK_LIST,
	BOX_LIST,
	HAND_LIST,
	HEART_LIST,
	ARROWHEAD_LIST
};

static const XAP_String_Id s_noneStrings[] =
{
	AP_STRING_ID_DLG_Lists_Style_none
};

static const XAP_String_Id s_numberedStrings[] =
{
	AP_STRING_ID_DLG_Lists_Numbered_List,
	AP_STRING_ID_DLG_Lists_Lower_Case_List,
	AP_STRING_ID_DLG_Lists_Upper_Case_List,
	AP_STRING_ID_DLG_Lists_Lower_Roman_List,
	AP_STRING_ID_DLG_Lists_Upper_Roman_List,
	AP_STRING_ID_DLG_Lists_Arabic_List,
	AP_STRING_ID_DLG_Lists_Hebrew_List
};

static const XAP_String_Id s_bulletedStrings[] =
{
	AP_STRING_ID_DLG_Lists_Bullet_List,
	AP_STRING_ID_DLG_Lists_Dashed_List,
	AP_STRING_ID_DLG_Lists_Square_List,
	AP_STRING_ID_DLG_Lists_Triangle_List,
	AP_STRING_ID_DLG_Lists_Diamond_List,
	AP_STRING_ID_DLG_Lists_Star_List,
	AP_STRING_ID_DLG_Lists_Implies_List,
	AP_STRING_ID_DLG_Lists_Tick_List,
	AP_STRING_ID_DLG_Lists_Box_List,
	AP_STRING_ID_DLG_Lists_Hand_List,
	AP_STRING_ID_DLG_Lists_Heart_List,
	AP_STRING_ID_DLG_Lists_Arrowhead_List
};

/* Index of a list type in a style table, or -1. */
static UT_sint32 s_typeIndex(const FL_ListType * table, UT_sint32 count,
							 FL_ListType type)
{
	for (UT_sint32 i = 0; i < count; i++)
		if (table[i] == type)
			return i;
	return -1;
}

static GtkStringList * s_stringListFor(const XAP_StringSet * pSS,
									   const XAP_String_Id * ids,
									   UT_sint32 count)
{
	GtkStringList * list = gtk_string_list_new(nullptr);
	for (UT_sint32 i = 0; i < count; i++)
	{
		std::string s;
		pSS->getValueUTF8(ids[i], s);
		gtk_string_list_append(list, s.c_str());
	}
	return list;
}

/*****************************************************************/
/* Signal trampolines                                             */
/*****************************************************************/

static void s_customChanged(GtkWidget * /*widget*/, AP_UnixDialog_Lists * me)
{
	me->setDirty();
	me->customChanged();
}

static void s_FoldCheck_changed(GtkWidget * widget, AP_UnixDialog_Lists * me)
{
	if (gtk_check_button_get_active(GTK_CHECK_BUTTON(widget)))
	{
		UT_sint32 iLevel = GPOINTER_TO_INT(
			g_object_get_data(G_OBJECT(widget), "level"));
		me->setFoldLevel(iLevel, true);
	}
}

static void s_typeChanged(GObject * /*w*/, GParamSpec * /*pspec*/,
						  AP_UnixDialog_Lists * me)
{
	gint idx = (gint)gtk_drop_down_get_selected(
		GTK_DROP_DOWN(me->typeDrop()));
	if (me->dontUpdate())
		return;

	me->setDirty();
	switch (idx)
	{
		case 0:
			me->styleChanged(0);
			break;
		case 1:
			me->fillUncustomizedValues();
			me->styleChanged(1);
			break;
		case 2:
			me->fillUncustomizedValues();
			me->styleChanged(2);
			break;
		default:
			break;
	}
}

/*!
 * User has changed their list style selection.
 */
static void s_styleChanged(GObject * /*w*/, GParamSpec * /*pspec*/,
						   AP_UnixDialog_Lists * me)
{
	if (me->dontUpdate())
		return;
	me->setDirty();
	me->setListTypeFromWidget();	// sets m_newListType
	me->fillUncustomizedValues();	// defaults for this type
	me->loadXPDataIntoLocal();		// reflect them in the widgets
	me->previewInvalidate();
}

/*!
 * A value in the Customize box has changed.
 */
static void s_valueChanged(GtkWidget * /*widget*/, AP_UnixDialog_Lists * me)
{
	if (me->dontUpdate())
		return;
	me->setDirty();
	me->setXPFromLocal();
	me->previewInvalidate();
}

static void s_applyClicked(GtkWidget * /*widget*/, AP_UnixDialog_Lists * me)
{
	me->applyClicked();
}

static void s_closeClicked(GtkWidget * /*widget*/, AP_UnixDialog_Lists * me)
{
	me->closeClicked();
}

static void s_preview_draw(GtkDrawingArea * /*area*/, cairo_t *cr,
						   int /*width*/, int /*height*/, gpointer data)
{
	AP_UnixDialog_Lists *dlg = static_cast<AP_UnixDialog_Lists *>(data);
	UT_return_if_fail(dlg);
	dlg->previewDraw(cr);
}

static gboolean s_destroy_clicked(GtkWidget * /*widget*/,
								  AP_UnixDialog_Lists * dlg)
{
	UT_ASSERT(dlg);
	dlg->setAnswer(AP_Dialog_Lists::a_QUIT);
	dlg->destroy();
	return TRUE;
}

/*****************************************************************/
/* Construction / teardown                                        */
/*****************************************************************/

AP_UnixDialog_Lists::AP_UnixDialog_Lists(XAP_DialogFactory * pDlgFactory,
										 XAP_Dialog_Id id)
	: AP_Dialog_Lists(pDlgFactory,id)
	, m_pPreviewWidget(nullptr)
	, m_bManualListStyle(true)
	, m_bDestroy_says_stopupdating(false)
	, m_bAutoUpdate_happening_now(false)
	, m_bDontUpdate(false)
	, m_pAutoUpdateLists(nullptr)
	, m_wApply(nullptr)
	, m_wClose(nullptr)
	, m_wContents(nullptr)
	, m_wStartNewList(nullptr)
	, m_wApplyCurrent(nullptr)
	, m_wStartSubList(nullptr)
	, m_wPreviewArea(nullptr)
	, m_wDelimEntry(nullptr)
	, m_wDecimalEntry(nullptr)
	, m_wAlignListSpin(nullptr)
	, m_wIndentAlignSpin(nullptr)
	, m_wFontDrop(nullptr)
	, m_wCustomGrid(nullptr)
	, m_wStyleDrop(nullptr)
	, m_wTypeDrop(nullptr)
	, m_wStartSpin(nullptr)
	, m_wResetButton(nullptr)

	, m_curStyleTypes(s_numberedTypes)
	, m_curStyleTypeCount(G_N_ELEMENTS(s_numberedTypes))
	, m_idStyleChanged(0)
	, m_idTypeChanged(0)
	, m_idFontChanged(0)
	, m_idDelimChanged(0)
	, m_idDecimalChanged(0)
	, m_idStartChanged(0)
	, m_idAlignChanged(0)
	, m_idIndentChanged(0)
	, m_iPageLists(0)
	, m_iPageFold(0)
{
}

XAP_Dialog * AP_UnixDialog_Lists::static_constructor(XAP_DialogFactory * pFactory,
												   XAP_Dialog_Id id)
{
	return new AP_UnixDialog_Lists(pFactory,id);
}

AP_UnixDialog_Lists::~AP_UnixDialog_Lists(void)
{
	DELETEP(m_pPreviewWidget);
}

/*****************************************************************/
/* Dialog protocol                                                */
/*****************************************************************/

void AP_UnixDialog_Lists::closeClicked(void)
{
	setAnswer(AP_Dialog_Lists::a_QUIT);
	// a registered modeless dialog must run destroy() so the app
	// unregisters it; GTK4's gtk_window_destroy emits no signal, so
	// destroying the widget alone leaves a dangling dialog in the
	// modeless table and the next focus notification crashes
	if (isRunning())
		destroy();
	else
		abiDestroyWidget(m_windowMain);
}

void AP_UnixDialog_Lists::runModal(XAP_Frame * pFrame)
{
	setModal();

	GtkWidget * mainWindow = _constructWindow();
	UT_return_if_fail(mainWindow);

	clearDirty();

	// Populate the dialog
	m_bDontUpdate = false;
	loadXPDataIntoLocal();

	// loadXPDataIntoLocal may reset the type while wiring up the
	// preview; preserve what the XP layer actually asked for
	FL_ListType savedListType = getNewListType();

	// transient before showing: GTK4 maps a GtkWindow the moment it
	// becomes visible and warns about parentless transient windows
	{
		XAP_UnixFrameImpl * pImpl =
			static_cast<XAP_UnixFrameImpl *>(pFrame->getFrameImpl());
		GtkWidget * parentWindow =
			pImpl ? pImpl->getTopLevelWindow() : nullptr;
		if (GTK_IS_WINDOW(parentWindow))
			gtk_window_set_transient_for(GTK_WINDOW(m_windowMain),
									   GTK_WINDOW(parentWindow));
	}

	gtk_window_present(GTK_WINDOW(m_windowMain));

	// Graphics context for the preview widget
	GR_UnixCairoAllocInfo ai(m_wPreviewArea);
	m_pPreviewWidget =
		(GR_CairoGraphics*) XAP_App::getApp()->newGraphics(ai);

	graphene_rect_t bounds;
	UT_uint32 w = 0, h = 0;
	if (gtk_widget_compute_bounds(m_wPreviewArea, m_wPreviewArea, &bounds))
	{
		w = (UT_uint32)bounds.size.width;
		h = (UT_uint32)bounds.size.height;
	}
	if (w == 0 || h == 0)
	{
		// not allocated yet — use the request size
		gtk_widget_get_size_request(m_wPreviewArea,
									reinterpret_cast<int*>(&w),
									reinterpret_cast<int*>(&h));
	}
	_createPreviewFromGC(m_pPreviewWidget, w, h);

	// Restore our value
	setNewListType(savedListType);

	gint response;
	do {
		response = abiRunModalDialog(GTK_DIALOG(mainWindow), pFrame, this,
								   BUTTON_CANCEL, false);
	} while (response == BUTTON_RESET);
	AP_Dialog_Lists::tAnswer res = getAnswer();
	teardown();
	abiDestroyWidget(mainWindow);
	setAnswer(res);
	DELETEP(m_pPreviewWidget);
}

void AP_UnixDialog_Lists::runModeless(XAP_Frame * pFrame)
{
	_constructWindow();
	UT_return_if_fail(m_windowMain);
	clearDirty();

	abiSetupModelessDialog(GTK_DIALOG(m_windowMain), pFrame, this,
						   BUTTON_APPLY);
	connectFocusModelessOther(GTK_WIDGET(m_windowMain), m_pApp, nullptr);

	// Populate the dialog
	updateDialog();
	m_bDontUpdate = false;

	gtk_window_present(GTK_WINDOW(m_windowMain));

	// Graphics context for the preview widget
	GR_UnixCairoAllocInfo ai(m_wPreviewArea);
	m_pPreviewWidget =
		(GR_CairoGraphics*) XAP_App::getApp()->newGraphics(ai);

	graphene_rect_t bounds;
	UT_uint32 w = 0, h = 0;
	if (gtk_widget_compute_bounds(m_wPreviewArea, m_wPreviewArea, &bounds))
	{
		w = (UT_uint32)bounds.size.width;
		h = (UT_uint32)bounds.size.height;
	}
	if (w == 0 || h == 0)
	{
		gtk_widget_get_size_request(m_wPreviewArea,
									reinterpret_cast<int*>(&w),
									reinterpret_cast<int*>(&h));
	}
	_createPreviewFromGC(m_pPreviewWidget, w, h);

	// Auto-update timer (500 ms)
	m_pAutoUpdateLists = UT_Timer::static_constructor(autoupdateLists, this);
	m_bDestroy_says_stopupdating = false;
	m_pAutoUpdateLists->set(500);
}

/*!
 * Free per-window state shared by the modal and modeless paths.
 */
void AP_UnixDialog_Lists::teardown(void)
{
	m_glFonts.clear();
}

void AP_UnixDialog_Lists::autoupdateLists(UT_Worker * pWorker)
{
	UT_ASSERT(pWorker);
	AP_UnixDialog_Lists * pDialog =
		static_cast<AP_UnixDialog_Lists *>(pWorker->getInstanceData());
	if (!pDialog)
		return;

	if (pDialog->isDirty())
		return;

	AV_View * view = pDialog->getAvView();
	if (!view)
		return;

	if (view->getTick() != pDialog->getTick())
	{
		pDialog->setTick(view->getTick());
		if (pDialog->m_bDestroy_says_stopupdating != true)
		{
			pDialog->m_bAutoUpdate_happening_now = true;
			pDialog->updateDialog();
			pDialog->previewInvalidate();
			pDialog->m_bAutoUpdate_happening_now = false;
		}
	}
}

void AP_UnixDialog_Lists::previewInvalidate(void)
{
	if (m_pPreviewWidget)
	{
		setbisCustomized(true);
		event_PreviewAreaExposed();
	}
}

void AP_UnixDialog_Lists::previewDraw(cairo_t *cr)
{
	AP_Lists_preview * preview = getListsPreview();
	if (!preview)
		return;

	GR_CairoGraphics *gc =
		static_cast<GR_CairoGraphics*>(preview->getGraphics());
	if (!gc)
		return;

	if (m_pPreviewWidget)
		setbisCustomized(true);

	gc->setCairo(cr);
	preview->drawImmediate();
	gc->setCairo(nullptr);
}

void AP_UnixDialog_Lists::destroy(void)
{
	UT_ASSERT(m_windowMain);
	if (isModal())
	{
		setAnswer(AP_Dialog_Lists::a_QUIT);
	}
	else
	{
		m_bDestroy_says_stopupdating = true;
		if (m_pAutoUpdateLists)
			m_pAutoUpdateLists->stop();
		setAnswer(AP_Dialog_Lists::a_CLOSE);

		teardown();
		modeless_cleanup();
		{
			// clear before teardown: focus notifications re-entered
			// during gtk_window_destroy must see a null window
			GtkWidget * w = m_windowMain;
			m_windowMain = nullptr;
			abiDestroyWidget(w);
		}
		DELETEP(m_pAutoUpdateLists);
		DELETEP(m_pPreviewWidget);
	}
}

/*!
 * Set the Fold level from the XP layer.
 */
void AP_UnixDialog_Lists::setFoldLevelInGUI(void)
{
	setFoldLevel(getCurrentFold(), true);
}

/*!
 * Set the Fold Level in the current List structure.
 */
void AP_UnixDialog_Lists::setFoldLevel(UT_sint32 iLevel, bool bSet)
{
	UT_sint32 count = m_vecFoldCheck.getItemCount();
	if (iLevel < 0 || iLevel >= count)
		return;

	GtkWidget * wF = m_vecFoldCheck.getNthItem(iLevel);
	UT_uint32 ID = m_vecFoldID.getNthItem(iLevel);
	if (!wF)
		return;

	{
		XAP_GtkSignalBlocker b(G_OBJECT(wF), ID);
		gtk_check_button_set_active(GTK_CHECK_BUTTON(wF), TRUE);
	}
	setCurrentFold(bSet ? iLevel : 0);
}

bool AP_UnixDialog_Lists::isPageLists(void) const
{
	if (isModal())
		return true;
	if (!m_wContents)
		return false;
	return gtk_notebook_get_current_page(GTK_NOTEBOOK(m_wContents))
		== m_iPageLists;
}

void AP_UnixDialog_Lists::activate(void)
{
	if (!m_windowMain)
		return;
	ConstructWindowName();
	gtk_window_set_title(GTK_WINDOW(m_windowMain), getWindowName());
	m_bDontUpdate = false;
	updateDialog();
	XAP_gtk_window_raise(m_windowMain);
}

void AP_UnixDialog_Lists::notifyActiveFrame(XAP_Frame * /*pFrame*/)
{
	if (!m_windowMain)
		return;
	ConstructWindowName();
	gtk_window_set_title(GTK_WINDOW(m_windowMain), getWindowName());
	m_bDontUpdate = false;
	updateDialog();
	previewInvalidate();
}

/*****************************************************************/
/* Behaviour                                                      */
/*****************************************************************/

/*!
 * Swap the style drop-down to the model matching list category
 * (0 = none, 1 = bulleted, 2 = numbered) and update control
 * sensitivity to match.
 */
void AP_UnixDialog_Lists::styleChanged(gint type)
{
	if (type == 0)
	{
		_setStyleModel(0);
		gtk_drop_down_set_selected(GTK_DROP_DOWN(m_wTypeDrop), 0);
		setNewListType(NOT_A_LIST);
		gtk_widget_set_sensitive(m_wFontDrop, false);
		gtk_widget_set_sensitive(m_wStartSpin, false);
		gtk_widget_set_sensitive(m_wDelimEntry, false);
		gtk_widget_set_sensitive(m_wDecimalEntry, false);
	}
	else if (type == 1)
	{
		_setStyleModel(1);
		gtk_drop_down_set_selected(GTK_DROP_DOWN(m_wTypeDrop), 1);
		setNewListType(BULLETED_LIST);
		gtk_widget_set_sensitive(m_wFontDrop, true);
		gtk_widget_set_sensitive(m_wStartSpin, false);
		gtk_widget_set_sensitive(m_wDelimEntry, false);
		gtk_widget_set_sensitive(m_wDecimalEntry, false);
	}
	else if (type == 2)
	{
		_setStyleModel(2);
		gtk_drop_down_set_selected(GTK_DROP_DOWN(m_wTypeDrop), 2);
		setNewListType(NUMBERED_LIST);
		gtk_widget_set_sensitive(m_wFontDrop, true);
		gtk_widget_set_sensitive(m_wStartSpin, true);
		gtk_widget_set_sensitive(m_wDelimEntry, true);
		gtk_widget_set_sensitive(m_wDecimalEntry, true);
	}

	// Called from loadXPDataIntoLocal too — in that case we must not
	// recurse back into it; m_bDontUpdate guards that.
	if (!dontUpdate())
	{
		fillUncustomizedValues();
		loadXPDataIntoLocal();
		previewInvalidate();
	}
}

/*!
 * Sets m_newListType from the currently selected style item.
 */
void AP_UnixDialog_Lists::setListTypeFromWidget(void)
{
	if (!m_wStyleDrop || !m_curStyleTypes || m_curStyleTypeCount <= 0)
		return;
	guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(m_wStyleDrop));
	if (idx >= (guint)m_curStyleTypeCount)
		return;
	setNewListType(m_curStyleTypes[idx]);
}

/*!
 * Read out all the elements of the GUI and set the XP member
 * variables from them.
 */
void AP_UnixDialog_Lists::setXPFromLocal(void)
{
	setListTypeFromWidget();
	_gatherData();

	if (gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wStartNewList)))
	{
		setbStartNewList(true);
		setbApplyToCurrent(false);
		setbResumeList(false);
	}
	else if (gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wApplyCurrent)))
	{
		setbStartNewList(false);
		setbApplyToCurrent(true);
		setbResumeList(false);
	}
	else if (gtk_check_button_get_active(GTK_CHECK_BUTTON(m_wStartSubList)))
	{
		setbStartNewList(false);
		setbApplyToCurrent(false);
		setbResumeList(true);
	}
}

void AP_UnixDialog_Lists::applyClicked(void)
{
	setXPFromLocal();
	previewInvalidate();
	Apply();
	if (isModal())
		setAnswer(AP_Dialog_Lists::a_OK);
}

void AP_UnixDialog_Lists::customChanged(void)
{
	fillUncustomizedValues();
	loadXPDataIntoLocal();
}

void AP_UnixDialog_Lists::updateFromDocument(void)
{
	PopulateDialogData();
	_setRadioButtonLabels();
	setNewListType(getDocListType());
	loadXPDataIntoLocal();
}

void AP_UnixDialog_Lists::updateDialog(void)
{
	if (!isDirty())
		updateFromDocument();
	else
		setXPFromLocal();
}

void AP_UnixDialog_Lists::setAllSensitivity(void)
{
	PopulateDialogData();
}

/*****************************************************************/
/* Window construction                                            */
/*****************************************************************/

GtkWidget * AP_UnixDialog_Lists::_constructWindow(void)
{
	ConstructWindowName();
	m_windowMain = abiDialogNew("list dialog", TRUE);
	gtk_window_set_title(GTK_WINDOW(m_windowMain), getWindowName());

	GtkWidget * vbox = gtk_dialog_get_content_area(GTK_DIALOG(m_windowMain));
	GtkWidget * contents = _constructWindowContents();
	gtk_box_append(GTK_BOX(vbox), contents);

	const XAP_StringSet* pSS = XAP_App::getApp()->getStringSet();
	std::string s;
	if (!isModal())
	{
		pSS->getValueUTF8(XAP_STRING_ID_DLG_Close, s);
		m_wClose = abiAddButton(GTK_DIALOG(m_windowMain), s, BUTTON_CLOSE);
		pSS->getValueUTF8(XAP_STRING_ID_DLG_Apply, s);
		m_wApply = abiAddButton(GTK_DIALOG(m_windowMain), s, BUTTON_APPLY);
	}
	else
	{
		pSS->getValueUTF8(XAP_STRING_ID_DLG_OK, s);
		m_wApply = abiAddButton(GTK_DIALOG(m_windowMain), s, BUTTON_OK);
		pSS->getValueUTF8(XAP_STRING_ID_DLG_Cancel, s);
		m_wClose = abiAddButton(GTK_DIALOG(m_windowMain), s, BUTTON_CANCEL);
	}

	gtk_window_set_default_widget(GTK_WINDOW(m_windowMain), m_wClose);
	_connectSignals();

	return m_windowMain;
}

/* A labelled grid row helper: label at column 0, widget at column 1. */
static GtkWidget * s_gridRow(GtkGrid * grid, int row,
							 const char * labelText, GtkWidget * child)
{
	GtkWidget * label = gtk_label_new(labelText);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(grid, label, 0, row, 1, 1);
	gtk_widget_set_hexpand(child, TRUE);
	gtk_grid_attach(grid, child, 1, row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), child);
	return label;
}

GtkWidget * AP_UnixDialog_Lists::_constructWindowContents(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();
	std::string s;

	if (isModal())
	{
		// Modal (from the Styles dialog): lists page only, no tabs
		GtkWidget * grid = gtk_grid_new();
		g_object_set(G_OBJECT(grid),
					 "row-spacing", 12,
					 "column-spacing", 12,
					 "margin-top", 12,
					 "margin-bottom", 12,
					 "margin-start", 12,
					 "margin-end", 12,
					 nullptr);
		gtk_grid_attach(GTK_GRID(grid), _constructListsPage(), 0, 0, 1, 1);
		gtk_grid_attach(GTK_GRID(grid), _constructPreview(), 1, 0, 1, 1);
		m_wContents = grid;
		return m_wContents;
	}

	GtkWidget * wNoteBook = gtk_notebook_new();
	gtk_widget_set_margin_start(wNoteBook, 12);
	gtk_widget_set_margin_end(wNoteBook, 12);
	gtk_widget_set_margin_top(wNoteBook, 12);
	gtk_widget_set_margin_bottom(wNoteBook, 6);

	// Lists page: controls + preview side by side
	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_PageProperties, s);
	GtkWidget * lbPageLists = gtk_label_new(s.c_str());
	GtkWidget * listsGrid = gtk_grid_new();
	g_object_set(G_OBJECT(listsGrid),
				 "row-spacing", 12,
				 "column-spacing", 16,
				 "margin-top", 12,
				 "margin-bottom", 12,
				 "margin-start", 8,
				 "margin-end", 8,
				 nullptr);
	gtk_grid_attach(GTK_GRID(listsGrid), _constructListsPage(), 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(listsGrid), _constructPreview(), 1, 0, 1, 1);
	gtk_notebook_append_page(GTK_NOTEBOOK(wNoteBook), listsGrid,
							 lbPageLists);
	m_iPageLists = gtk_notebook_page_num(GTK_NOTEBOOK(wNoteBook),
										 listsGrid);

	// Folding page
	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_PageFolding, s);
	GtkWidget * lbPageFolding = gtk_label_new(s.c_str());
	GtkWidget * wFoldingGrid = _constructFoldingPage();
	gtk_notebook_append_page(GTK_NOTEBOOK(wNoteBook), wFoldingGrid,
							 lbPageFolding);
	m_iPageFold = gtk_notebook_page_num(GTK_NOTEBOOK(wNoteBook),
										wFoldingGrid);

	gtk_notebook_set_current_page(GTK_NOTEBOOK(wNoteBook), m_iPageLists);
	m_wContents = wNoteBook;
	return m_wContents;
}

GtkWidget * AP_UnixDialog_Lists::_constructListsPage(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();
	std::string s;

	GtkWidget * page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

	/* ---- Type / Style row ---- */
	GtkWidget * grid1 = gtk_grid_new();
	g_object_set(G_OBJECT(grid1),
				 "row-spacing", 8,
				 "column-spacing", 12,
				 nullptr);
	gtk_box_append(GTK_BOX(page), grid1);

	// Type: None / Bulleted / Numbered
	m_wTypeDrop = gtk_drop_down_new(nullptr, nullptr);
	{
		GtkStringList * types = gtk_string_list_new(nullptr);
		pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Type_none, s);
		gtk_string_list_append(types, s.c_str());
		pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Type_bullet, s);
		gtk_string_list_append(types, s.c_str());
		pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Type_numbered, s);
		gtk_string_list_append(types, s.c_str());
		gtk_drop_down_set_model(GTK_DROP_DOWN(m_wTypeDrop),
								G_LIST_MODEL(types));
		g_object_unref(types);
	}
	gtk_drop_down_set_selected(GTK_DROP_DOWN(m_wTypeDrop), 0);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Type, s);
	s_gridRow(GTK_GRID(grid1), 0, s.c_str(), m_wTypeDrop);

	// Style drop-down — the model is rebuilt fresh on each type
	// switch; see _setStyleModel for why every model must come
	// through gtk_drop_down_set_model() rather than the constructor.
	m_wStyleDrop = gtk_drop_down_new(nullptr, nullptr);
	{
		GListModel * numbered = G_LIST_MODEL(
			s_stringListFor(pSS, s_numberedStrings,
							G_N_ELEMENTS(s_numberedStrings)));
		gtk_drop_down_set_model(GTK_DROP_DOWN(m_wStyleDrop), numbered);
		g_object_unref(numbered);
	}
	m_curStyleTypes = s_numberedTypes;
	m_curStyleTypeCount = G_N_ELEMENTS(s_numberedTypes);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Style, s);
	s_gridRow(GTK_GRID(grid1), 1, s.c_str(), m_wStyleDrop);

	/* ---- Customize grid ---- */
	m_wCustomGrid = gtk_grid_new();
	g_object_set(G_OBJECT(m_wCustomGrid),
				 "row-spacing", 8,
				 "column-spacing", 12,
				 nullptr);
	gtk_box_append(GTK_BOX(page), m_wCustomGrid);

	// Delimiter (a.k.a. Format) entry
	m_wDelimEntry = gtk_entry_new();
	gtk_editable_set_max_width_chars(GTK_EDITABLE(m_wDelimEntry), 20);
	gtk_editable_set_text(GTK_EDITABLE(m_wDelimEntry), "");
	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Format, s);
	s_gridRow(GTK_GRID(m_wCustomGrid), 0, s.c_str(), m_wDelimEntry);

	// Font drop-down
	m_wFontDrop = gtk_drop_down_new(nullptr, nullptr);
	_fillFontDrop();
	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Font, s);
	s_gridRow(GTK_GRID(m_wCustomGrid), 1, s.c_str(), m_wFontDrop);

	// Decimal entry
	m_wDecimalEntry = gtk_entry_new();
	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_DelimiterString, s);
	s_gridRow(GTK_GRID(m_wCustomGrid), 2, s.c_str(), m_wDecimalEntry);

	// Start-at spin
	m_wStartSpin = gtk_spin_button_new(
		gtk_adjustment_new(1, 0, G_MAXINT32, 1, 10, 0), 1, 0);
	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Start, s);
	s_gridRow(GTK_GRID(m_wCustomGrid), 3, s.c_str(), m_wStartSpin);

	// Text-align spin
	m_wAlignListSpin = gtk_spin_button_new(
		gtk_adjustment_new(0.25, 0, 10, 0.01, 0.2, 0), 0.05, 2);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(m_wAlignListSpin),
									  TRUE);
	gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(m_wAlignListSpin), TRUE);
	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Align, s);
	s_gridRow(GTK_GRID(m_wCustomGrid), 4, s.c_str(), m_wAlignListSpin);

	// Label-align (indent) spin
	m_wIndentAlignSpin = gtk_spin_button_new(
		gtk_adjustment_new(0, 0, 10, 0.01, 0.2, 0), 0.05, 2);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(m_wIndentAlignSpin),
									  TRUE);
	gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(m_wIndentAlignSpin), TRUE);
	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Indent, s);
	s_gridRow(GTK_GRID(m_wCustomGrid), 5, s.c_str(), m_wIndentAlignSpin);

	// "Set Default" — a dialog action button (BUTTON_RESET response);
	// add it now so it exists before _connectSignals
	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_SetDefault, s);
	m_wResetButton = abiAddButton(GTK_DIALOG(m_windowMain), s,
								  BUTTON_RESET);

	/* ---- Apply-mode radio buttons (modeless only) ---- */
	GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
	if (!isModal())
		gtk_box_append(GTK_BOX(page), hbox);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Apply_Current, s);
	m_wApplyCurrent = abi_radio_button_new_with_label(nullptr, s.c_str());
	gtk_box_append(GTK_BOX(hbox), m_wApplyCurrent);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_wApplyCurrent), TRUE);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Start_New, s);
	m_wStartNewList = abi_radio_button_new_with_label(m_wApplyCurrent,
													s.c_str());
	gtk_box_append(GTK_BOX(hbox), m_wStartNewList);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Resume, s);
	m_wStartSubList = abi_radio_button_new_with_label(m_wApplyCurrent,
													  s.c_str());
	gtk_box_append(GTK_BOX(hbox), m_wStartSubList);

	setbisCustomized(false);
	return page;
}

GtkWidget * AP_UnixDialog_Lists::_constructFoldingPage(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();
	std::string s;

	GtkWidget * grid = gtk_grid_new();
	g_object_set(G_OBJECT(grid),
				 "row-spacing", 6,
				 "column-spacing", 12,
				 "margin-top", 12,
				 "margin-bottom", 12,
				 "margin-start", 12,
				 "margin-end", 12,
				 nullptr);

	GtkWidget * lbFoldHeading = gtk_label_new("<b>%s</b>");
	gtk_label_set_use_markup(GTK_LABEL(lbFoldHeading), TRUE);
	localizeLabelMarkup(lbFoldHeading, pSS,
						AP_STRING_ID_DLG_Lists_FoldingLevelexp);
	gtk_grid_attach(GTK_GRID(grid), lbFoldHeading, 0, 0, 2, 1);

	static const XAP_String_Id foldIds[] =
	{
		AP_STRING_ID_DLG_Lists_FoldingLevel0,
		AP_STRING_ID_DLG_Lists_FoldingLevel1,
		AP_STRING_ID_DLG_Lists_FoldingLevel2,
		AP_STRING_ID_DLG_Lists_FoldingLevel3,
		AP_STRING_ID_DLG_Lists_FoldingLevel4
	};

	m_vecFoldCheck.clear();
	m_vecFoldID.clear();

	GtkWidget * group = nullptr;
	for (UT_sint32 i = 0; i < (UT_sint32)G_N_ELEMENTS(foldIds); i++)
	{
		pSS->getValueUTF8(foldIds[i], s);
		GtkWidget * wF = abi_radio_button_new_with_label(group,
													   s.c_str());
		group = wF;
		g_object_set_data(G_OBJECT(wF), "level", GINT_TO_POINTER(i));
		gulong ID = g_signal_connect(G_OBJECT(wF), "toggled",
									 G_CALLBACK(s_FoldCheck_changed),
									 (gpointer)this);
		gtk_grid_attach(GTK_GRID(grid), wF, 0, i + 1, 1, 1);
		gtk_widget_set_margin_start(wF, 18);
		m_vecFoldCheck.addItem(wF);
		m_vecFoldID.addItem(ID);
	}

	return grid;
}

GtkWidget * AP_UnixDialog_Lists::_constructPreview(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();
	std::string s;

	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Preview, s);
	GtkWidget * preview_lb = gtk_label_new(s.c_str());
	gtk_label_set_xalign(GTK_LABEL(preview_lb), 0.0);
	gtk_box_append(GTK_BOX(box), preview_lb);

	m_wPreviewArea = gtk_drawing_area_new();
	gtk_widget_set_size_request(m_wPreviewArea, 180, 225);
	gtk_widget_set_margin_start(m_wPreviewArea, 18);
	gtk_widget_add_css_class(m_wPreviewArea, "view");
	gtk_box_append(GTK_BOX(box), m_wPreviewArea);

	return box;
}

/*****************************************************************/
/* Data flow                                                      */
/*****************************************************************/

/* Swap the style drop-down's model + parallel type table.
 *
 * GTK 4.14's GtkDropDown has two defects that shape this code:
 *
 * 1. A model installed via gtk_drop_down_new() gets fewer internal
 *    references than one installed via gtk_drop_down_set_model(),
 *    so swapping out a constructor-installed model over-unrefs it
 *    and corrupts the next model. Every model must therefore be
 *    installed via set_model() on a drop-down created with NULL.
 * 2. Re-setting a GListModel instance that was previously attached
 *    to the drop-down crashes in stale selection/factory state, so
 *    a fresh GtkStringList is built on every swap rather than
 *    caching and reusing models.
 */
void AP_UnixDialog_Lists::_setStyleModel(gint which)
{
	const XAP_String_Id * ids;
	const FL_ListType * types;
	UT_sint32 count;

	switch (which)
	{
		case 0:
			ids = s_noneStrings;
			types = s_noneTypes;
			count = G_N_ELEMENTS(s_noneTypes);
			break;
		case 1:
			ids = s_bulletedStrings;
			types = s_bulletedTypes;
			count = G_N_ELEMENTS(s_bulletedStrings);
			break;
		default:
			ids = s_numberedStrings;
			types = s_numberedTypes;
			count = G_N_ELEMENTS(s_numberedStrings);
			break;
	}

	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
	UT_return_if_fail(pSS);
	GListModel * model = G_LIST_MODEL(s_stringListFor(pSS, ids, count));

	XAP_GtkSignalBlocker b(G_OBJECT(m_wStyleDrop), m_idStyleChanged);
	gtk_drop_down_set_model(GTK_DROP_DOWN(m_wStyleDrop), model);
	/* GTK refs the model internally; release our reference. */
	g_object_unref(model);
	m_curStyleTypes = types;
	m_curStyleTypeCount = count;
}

void AP_UnixDialog_Lists::_fillFontDrop(void)
{
	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
	std::string s;

	_getGlistFonts(m_glFonts);

	GtkStringList * list = gtk_string_list_new(nullptr);
	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Current_Font, s);
	gtk_string_list_append(list, s.c_str());
	for (const std::string & name : m_glFonts)
		gtk_string_list_append(list, name.c_str());

	/* Installed via set_model on a NULL-created drop-down so the
	 * ref bookkeeping stays consistent — see _setStyleModel. */
	gtk_drop_down_set_model(GTK_DROP_DOWN(m_wFontDrop),
							G_LIST_MODEL(list));
	g_object_unref(list);
}

/*
 * Collect all available fonts. Adapted from
 * xap_UnixDialog_Insert_Symbol.
 */
void AP_UnixDialog_Lists::_getGlistFonts(std::vector<std::string> & glFonts)
{
	glFonts.clear();
	GR_GraphicsFactory * pGF = XAP_App::getApp()->getGraphicsFactory();
	UT_return_if_fail(pGF);

	const std::vector<std::string> & names =
		GR_CairoGraphics::getAllFontNames();

	std::string currentfont;
	for (const std::string & lgn : names)
	{
		if (currentfont.empty() ||
			(strstr(currentfont.c_str(), lgn.c_str()) == nullptr) ||
			currentfont.size() != lgn.size())
		{
			currentfont = lgn;
			glFonts.push_back(lgn);
		}
	}
}

void AP_UnixDialog_Lists::_setRadioButtonLabels(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();
	std::string s;
	PopulateDialogData();
	// Button 0 is Start New List, button 2 is resume list
	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Start_New, s);
	gtk_check_button_set_label(GTK_CHECK_BUTTON(m_wStartNewList),
							   s.c_str());
	pSS->getValueUTF8(AP_STRING_ID_DLG_Lists_Resume, s);
	gtk_check_button_set_label(GTK_CHECK_BUTTON(m_wStartSubList),
							   s.c_str());
}

void AP_UnixDialog_Lists::_connectSignals(void)
{
	connectBasicSignals();

	g_signal_connect(G_OBJECT(m_wApply), "clicked",
					 G_CALLBACK(s_applyClicked), this);
	g_signal_connect(G_OBJECT(m_wClose), "clicked",
					 G_CALLBACK(s_closeClicked), this);
	if (m_wResetButton)
		g_signal_connect(G_OBJECT(m_wResetButton), "clicked",
						 G_CALLBACK(s_customChanged), this);

	m_idTypeChanged = g_signal_connect(G_OBJECT(m_wTypeDrop),
									   "notify::selected",
									   G_CALLBACK(s_typeChanged), this);
	m_idStyleChanged = g_signal_connect(G_OBJECT(m_wStyleDrop),
										"notify::selected",
										G_CALLBACK(s_styleChanged), this);
	m_idFontChanged = g_signal_connect(G_OBJECT(m_wFontDrop),
									   "notify::selected",
									   G_CALLBACK(s_valueChanged), this);

	m_idStartChanged = g_signal_connect(
		G_OBJECT(gtk_spin_button_get_adjustment(
					 GTK_SPIN_BUTTON(m_wStartSpin))),
		"value-changed", G_CALLBACK(s_valueChanged), this);
	m_idDecimalChanged = g_signal_connect(G_OBJECT(m_wDecimalEntry),
										  "changed",
										  G_CALLBACK(s_valueChanged), this);
	m_idAlignChanged = g_signal_connect(
		G_OBJECT(gtk_spin_button_get_adjustment(
					 GTK_SPIN_BUTTON(m_wAlignListSpin))),
		"value-changed", G_CALLBACK(s_valueChanged), this);
	m_idIndentChanged = g_signal_connect(
		G_OBJECT(gtk_spin_button_get_adjustment(
					 GTK_SPIN_BUTTON(m_wIndentAlignSpin))),
		"value-changed", G_CALLBACK(s_valueChanged), this);
	m_idDelimChanged = g_signal_connect(G_OBJECT(m_wDelimEntry),
										"changed",
										G_CALLBACK(s_valueChanged), this);

	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(m_wPreviewArea),
								   s_preview_draw,
								   reinterpret_cast<gpointer>(this),
								   nullptr);
	g_signal_connect(G_OBJECT(m_windowMain), "close-request",
					 G_CALLBACK(s_destroy_clicked),
					 static_cast<gpointer>(this));
}

void AP_UnixDialog_Lists::loadXPDataIntoLocal(void)
{
	// Block all widget feedback while syncing
	XAP_GtkSignalBlocker b1(
		G_OBJECT(gtk_spin_button_get_adjustment(
					 GTK_SPIN_BUTTON(m_wAlignListSpin))),
		m_idAlignChanged);
	XAP_GtkSignalBlocker b2(
		G_OBJECT(gtk_spin_button_get_adjustment(
					 GTK_SPIN_BUTTON(m_wIndentAlignSpin))),
		m_idIndentChanged);
	XAP_GtkSignalBlocker b3(G_OBJECT(m_wDecimalEntry), m_idDecimalChanged);
	XAP_GtkSignalBlocker b4(G_OBJECT(m_wDelimEntry), m_idDelimChanged);

	m_bDontUpdate = true;

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_wAlignListSpin),
							  getfAlign());
	float indent = getfAlign() + getfIndent();
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_wIndentAlignSpin),
							  indent);
	if ((getfIndent() + getfAlign()) < 0.0)
	{
		setfIndent(-getfAlign());
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_wIndentAlignSpin),
								  0.0);
	}

	// Font: index 0 is "current font"; fonts follow at i+1
	if (getFont() == "nullptr")
	{
		gtk_drop_down_set_selected(GTK_DROP_DOWN(m_wFontDrop), 0);
	}
	else
	{
		size_t i = 0;
		for (; i < m_glFonts.size(); i++)
			if (m_glFonts[i] == getFont())
				break;
		gtk_drop_down_set_selected(GTK_DROP_DOWN(m_wFontDrop),
								   (i < m_glFonts.size()) ? i + 1 : 0);
	}

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_wStartSpin),
							  static_cast<float>(getiStartValue()));

	gtk_editable_set_text(GTK_EDITABLE(m_wDecimalEntry),
						  getDecimal().c_str());
	gtk_editable_set_text(GTK_EDITABLE(m_wDelimEntry),
						  getDelim().c_str());

	// List type and style
	FL_ListType save = getNewListType();
	if (getNewListType() == NOT_A_LIST)
	{
		styleChanged(0);
		setNewListType(save);
		gtk_drop_down_set_selected(GTK_DROP_DOWN(m_wTypeDrop), 0);
		gtk_drop_down_set_selected(GTK_DROP_DOWN(m_wStyleDrop), 0);
	}
	else if (IS_BULLETED_LIST_TYPE(getNewListType()))
	{
		styleChanged(1);
		setNewListType(save);
		gtk_drop_down_set_selected(GTK_DROP_DOWN(m_wTypeDrop), 1);
		UT_sint32 idx = s_typeIndex(s_bulletedTypes,
									G_N_ELEMENTS(s_bulletedTypes),
									getNewListType());
		gtk_drop_down_set_selected(GTK_DROP_DOWN(m_wStyleDrop),
								   idx >= 0 ? idx : 0);
	}
	else
	{
		styleChanged(2);
		setNewListType(save);
		gtk_drop_down_set_selected(GTK_DROP_DOWN(m_wTypeDrop), 2);
		UT_sint32 idx = s_typeIndex(s_numberedTypes,
									G_N_ELEMENTS(s_numberedTypes),
									getNewListType());
		gtk_drop_down_set_selected(GTK_DROP_DOWN(m_wStyleDrop),
								   idx >= 0 ? idx : 0);
	}

	m_bDontUpdate = false;
}

bool AP_UnixDialog_Lists::dontUpdate(void)
{
	return m_bDontUpdate;
}

/*!
 * Read the Customize box widgets into the XP member variables.
 */
void AP_UnixDialog_Lists::_gatherData(void)
{
	UT_sint32 maxWidth = 0;
	fl_BlockLayout * block = getBlock();
	if (block && block->getDocSectionLayout())
		maxWidth = block->getDocSectionLayout()->getActualColumnWidth();
	if (block && block->getFirstContainer())
	{
		if (block->getFirstContainer()->getContainer())
			maxWidth =
				block->getFirstContainer()->getContainer()->getWidth();
	}
	if (maxWidth <= 0)
		maxWidth = 600;	// sane default ~6in at 100px/in

	// screen resolution is 100 pixels/inch
	float fmaxWidthIN = (static_cast<float>(maxWidth) / 100.) - 0.6;
	setiLevel(1);
	float f = gtk_spin_button_get_value(GTK_SPIN_BUTTON(m_wAlignListSpin));
	if (f > fmaxWidthIN)
	{
		f = fmaxWidthIN;
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_wAlignListSpin), f);
	}
	setfAlign(f);
	float indent =
		gtk_spin_button_get_value(GTK_SPIN_BUTTON(m_wIndentAlignSpin));
	if ((indent - f) > fmaxWidthIN)
	{
		indent = fmaxWidthIN + f;
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_wIndentAlignSpin),
								  indent);
	}
	setfIndent(indent - getfAlign());
	if ((getfIndent() + getfAlign()) < 0.0)
	{
		setfIndent(-getfAlign());
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_wIndentAlignSpin),
								  0.0);
	}
	guint ifont = gtk_drop_down_get_selected(GTK_DROP_DOWN(m_wFontDrop));
	if (ifont == 0 || ifont > m_glFonts.size())
	{
		copyCharToFont("nullptr");
	}
	else
	{
		copyCharToFont(m_glFonts[ifont - 1]);
	}
	const gchar * pszDec =
		XAP_gtk_entry_get_text(GTK_EDITABLE(m_wDecimalEntry));
	copyCharToDecimal(pszDec ? pszDec : "");
	setiStartValue(gtk_spin_button_get_value_as_int(
					   GTK_SPIN_BUTTON(m_wStartSpin)));
	const gchar * pszDel =
		XAP_gtk_entry_get_text(GTK_EDITABLE(m_wDelimEntry));
	copyCharToDelim(pszDel ? pszDel : "");
}
