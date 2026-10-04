/* -*- mode: C++; tab-width: 4; c-basic-offset: 4;  indent-tabs-mode: t -*- */
/* Abinova
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (c) 2009-2023 Hubert Figuière
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

#include "ut_string.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "xap_UnixDialogHelper.h"
#include "xap_GtkComboBoxHelpers.h"
#include "xap_GtkListHelpers.h"

#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"
#include "ap_UnixDialog_Styles.h"
#include "fl_DocLayout.h"
#include "fl_BlockLayout.h"
#include "fv_View.h"
#include "pd_Style.h"
#include "ut_string_class.h"
#include "pt_PieceTable.h"

#include "gr_UnixCairoGraphics.h"

// define to 0 to popup dialogs on top of each other, 1 to hide them
#define HIDE_MAIN_DIALOG 0

XAP_Dialog * AP_UnixDialog_Styles::static_constructor(XAP_DialogFactory * pFactory,
						      XAP_Dialog_Id id)
{
	AP_UnixDialog_Styles * p = new AP_UnixDialog_Styles(pFactory,id);
	return p;
}

AP_UnixDialog_Styles::AP_UnixDialog_Styles(XAP_DialogFactory * pDlgFactory,
										 XAP_Dialog_Id id)
  : AP_Dialog_Styles(pDlgFactory,id), m_selStyles(nullptr), m_whichType(AP_UnixDialog_Styles::USED_STYLES)
{
	m_windowMain = nullptr;

	m_btApply = nullptr;
	m_btClose = nullptr;
	m_wGnomeButtons = nullptr;
	m_wParaPreviewArea = nullptr;
	m_pParaPreviewWidget = nullptr;
	m_wCharPreviewArea = nullptr;
	m_pCharPreviewWidget = nullptr;

	m_listStyles = nullptr;
	m_tvStyles = nullptr;
	m_rbList1 = nullptr;
	m_rbList2 = nullptr;
	m_rbList3 = nullptr;
	m_lbAttributes = nullptr;

	m_wModifyDialog = nullptr;
	m_wStyleNameEntry = nullptr;
	m_wBasedOnCombo = nullptr;
	m_wFollowingCombo = nullptr;
	m_wStyleTypeCombo = nullptr;
	m_wStyleTypeEntry = nullptr;
	m_wLabDescription = nullptr;

	m_pAbiPreviewWidget = nullptr;
	m_wModifyDrawingArea = nullptr;

	m_wModifyOk = nullptr;
	m_wModifyCancel = nullptr;
	m_wFormatMenu = nullptr;
	m_wModifyShortCutKey = nullptr;

	m_bBlockModifySignal = false;
}

AP_UnixDialog_Styles::~AP_UnixDialog_Styles(void)
{
	DELETEP (m_pParaPreviewWidget);
	DELETEP (m_pCharPreviewWidget);
	DELETEP (m_pAbiPreviewWidget);
	g_clear_object(&m_listStyles);
}

/*****************************************************************/

static void
s_tvStyles_selection_changed (GObject * /*obj*/, GParamSpec * /*pspec*/,
		gpointer d)
{
	AP_UnixDialog_Styles * dlg = static_cast <AP_UnixDialog_Styles *>(d);
	dlg->event_SelectionChanged();
}

static void
s_typeslist_changed (GtkWidget *w, gpointer d)
{
	AP_UnixDialog_Styles * dlg = static_cast <AP_UnixDialog_Styles *>(d);
	// GTK4: check buttons emit "toggled" for both the deactivated and
	// the activated radio; only act on the newly selected one.
	if (!gtk_check_button_get_active (GTK_CHECK_BUTTON(w)))
		return;
	dlg->event_ListClicked (gtk_check_button_get_label (GTK_CHECK_BUTTON(w)));
}

static void
s_deletebtn_clicked (GtkWidget * /*w*/, gpointer d)
{
	AP_UnixDialog_Styles * dlg = static_cast <AP_UnixDialog_Styles *>(d);
	dlg->event_DeleteClicked ();
}

static void
s_modifybtn_clicked (GtkWidget * /*w*/, gpointer d)
{
	AP_UnixDialog_Styles * dlg = static_cast <AP_UnixDialog_Styles *>(d);
	dlg->event_ModifyClicked ();
}

static void
s_newbtn_clicked (GtkWidget * /*w*/, gpointer d)
{
	AP_UnixDialog_Styles * dlg = static_cast <AP_UnixDialog_Styles *>(d);
	dlg->event_NewClicked ();
}

static void
s_applybtn_clicked (GtkWidget * /*w*/, gpointer d)
{
	AP_UnixDialog_Styles * dlg = static_cast <AP_UnixDialog_Styles *>(d);
	dlg->event_Apply ();
}

static void
s_closebtn_clicked (GtkWidget * /*w*/, gpointer d)
{
	AP_UnixDialog_Styles * dlg = static_cast <AP_UnixDialog_Styles *>(d);
	dlg->event_Close ();
}

static void s_remove_property(GtkWidget * widget, AP_UnixDialog_Styles * me)
{
	UT_UNUSED(widget);
	UT_ASSERT(widget && me);
	me->event_RemoveProperty();
}

static void s_style_name(GtkWidget * widget, AP_UnixDialog_Styles * me)
{
	UT_UNUSED(widget);
	UT_ASSERT(widget && me);
	me->new_styleName();
}


static void s_basedon(GtkWidget * widget, GParamSpec * /*pspec*/, AP_UnixDialog_Styles * me)
{
	UT_UNUSED(widget);
	UT_ASSERT(widget && me);
	if(me->isModifySignalBlocked())
		return;
	me->event_basedOn();
}


static void s_followedby(GtkWidget * widget, GParamSpec * /*pspec*/, AP_UnixDialog_Styles * me)
{
	UT_UNUSED(widget);
	UT_ASSERT(widget && me);
	if(me->isModifySignalBlocked())
		return;
	me->event_followedBy();
}


static void s_styletype_impl(GtkWidget * widget, AP_UnixDialog_Styles * me)
{
	UT_UNUSED(widget);
	UT_ASSERT(widget && me);
	if(me->isModifySignalBlocked())
		return;
	me->event_styleType();
}

static void s_styletype(GtkWidget * widget, GParamSpec * /*pspec*/, AP_UnixDialog_Styles * me)
{
	s_styletype_impl(widget, me);
}

static void s_styletype_entry(GtkWidget * widget, AP_UnixDialog_Styles * me)
{
	s_styletype_impl(widget, me);
}

static void s_paraPreview_draw(GtkDrawingArea * /*area*/, cairo_t *cr,
								   int /*width*/, int /*height*/, gpointer data)
{
	AP_UnixDialog_Styles *me = static_cast<AP_UnixDialog_Styles *>(data);
	UT_return_if_fail(me);
	me->event_paraPreviewDraw(cr);
}


static void s_charPreview_draw(GtkDrawingArea * /*area*/, cairo_t *cr,
								   int /*width*/, int /*height*/, gpointer data)
{
	AP_UnixDialog_Styles *me = static_cast<AP_UnixDialog_Styles *>(data);
	UT_return_if_fail(me);
	me->event_charPreviewDraw(cr);
}


static void s_modifyPreview_draw(GtkDrawingArea * /*area*/, cairo_t *cr,
								   int /*width*/, int /*height*/, gpointer data)
{
	AP_UnixDialog_Styles *me = static_cast<AP_UnixDialog_Styles *>(data);
	UT_return_if_fail(me);
	me->event_ModifyPreviewDraw(cr);
}

static void s_modify_format_cb(GtkWidget * widget, GParamSpec * /*pspec*/,
			     AP_UnixDialog_Styles * me)
{
	gint active = static_cast<gint>(gtk_drop_down_get_selected(GTK_DROP_DOWN(widget)));
	if(active) {
		gtk_drop_down_set_selected(GTK_DROP_DOWN(widget), 0);
	}
	switch(active) {
	case 1:
		me->event_ModifyParagraph();
		break;
	case 2:
		me->event_ModifyFont();
		break;
	case 3:
		me->event_ModifyNumbering();
		break;
	case 4:
		me->event_ModifyLanguage();
		break;
	default:
		break;
	}
}


/*****************************************************************/

void AP_UnixDialog_Styles::runModal(XAP_Frame * pFrame)
{

//
// Get View and Document pointers. Place them in member variables
//

	setFrame(pFrame);
	setView(static_cast<FV_View *>(pFrame->getCurrentView()));
	UT_ASSERT(getView());

	setDoc(getView()->getLayout()->getDocument());

	UT_ASSERT(getDoc());

	// Build the window's widgets and arrange them
	m_windowMain = _constructWindow();
	UT_ASSERT(m_windowMain);

	abiSetupModalDialog(GTK_DIALOG(m_windowMain), pFrame, this, GTK_RESPONSE_CLOSE);

	// *** this is how we add the gc for the para and char Preview's ***
	// attach a new graphics context to the drawing area

	UT_ASSERT(m_wParaPreviewArea && XAP_HAS_NATIVE_WINDOW(m_wParaPreviewArea));

	// make a new Unix GC
	DELETEP (m_pParaPreviewWidget);
	{
		GR_UnixCairoAllocInfo ai(m_wParaPreviewArea);
		m_pParaPreviewWidget =
		    static_cast<GR_CairoGraphics*>( XAP_App::getApp()->newGraphics(ai));
	}

	// let the widget materialize

	GtkAllocation allocation;
	gtk_widget_get_allocation(m_wParaPreviewArea, &allocation);
	_createParaPreviewFromGC(m_pParaPreviewWidget,
				 static_cast<UT_uint32>(allocation.width),
				 static_cast<UT_uint32>(allocation.height));


	UT_ASSERT(m_wCharPreviewArea && XAP_HAS_NATIVE_WINDOW(m_wCharPreviewArea));

	// make a new Unix GC
	DELETEP (m_pCharPreviewWidget);
	{
		GR_UnixCairoAllocInfo ai(m_wCharPreviewArea);
		m_pCharPreviewWidget =
		    static_cast<GR_CairoGraphics*>( XAP_App::getApp()->newGraphics(ai));
	}

	// let the widget materialize

	gtk_widget_get_allocation(m_wCharPreviewArea, &allocation);
	_createCharPreviewFromGC(m_pCharPreviewWidget,
				 static_cast<UT_uint32>(allocation.width),
				 static_cast<UT_uint32>(allocation.height));

	// Populate the window's data items
	_populateWindowData();

	// the expose event of the preview
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(m_wParaPreviewArea),
							s_paraPreview_draw,
							reinterpret_cast<gpointer>(this), nullptr);

	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(m_wCharPreviewArea),
							s_charPreview_draw,
							reinterpret_cast<gpointer>(this), nullptr);

	// connect the select_row signal to the style list
	g_signal_connect (G_OBJECT (m_selStyles), "notify::selected",
			  G_CALLBACK (s_tvStyles_selection_changed), reinterpret_cast<gpointer>(this));

	// main loop for the dialog
	gint response;
	while(true)
	{
		response = abiRunModalDialog(GTK_DIALOG(m_windowMain), false);
		if (response == GTK_RESPONSE_APPLY)
			event_Apply();
		else
		{
			event_Close();
			break; // exit the loop
		}
	}

	/* window teardown can fire "notify::selected" while the model is
	 * being disposed; stop the handler before it reads dead rows */
	g_signal_handlers_disconnect_by_data(G_OBJECT(m_selStyles),
										 reinterpret_cast<gpointer>(this));

	abiDestroyWidget(m_windowMain);

	DELETEP (m_pParaPreviewWidget);
	DELETEP (m_pCharPreviewWidget);
}

/*****************************************************************/

void AP_UnixDialog_Styles::event_Apply(void)
{
	// TODO save out state of radio items
	m_answer = AP_Dialog_Styles::a_OK;
	const gchar * szStyle = getCurrentStyle();
	if(szStyle && *szStyle)
	{
		getView()->setStyle(szStyle);
	}
}

void AP_UnixDialog_Styles::event_Close(void)
{
	m_answer = AP_Dialog_Styles::a_CANCEL;
}

void AP_UnixDialog_Styles::event_WindowDelete(void)
{
	m_answer = AP_Dialog_Styles::a_CANCEL;
}

void AP_UnixDialog_Styles::event_paraPreviewDraw(cairo_t *cr)
{
	if (m_pParaPreview) {
		static_cast<GR_CairoGraphics*>(m_pParaPreview->getGraphics())->setCairo(cr);
		m_pParaPreview->drawImmediate();
		static_cast<GR_CairoGraphics*>(m_pParaPreview->getGraphics())->setCairo(nullptr);
	}
}


void AP_UnixDialog_Styles::event_charPreviewInvalidate(void)
{
	if (m_pCharPreview) {
		event_charPreviewUpdated();
	}
}

void AP_UnixDialog_Styles::event_charPreviewDraw(cairo_t *cr)
{
	if (m_pCharPreview) {
		static_cast<GR_CairoGraphics*>(m_pCharPreview->getGraphics())->setCairo(cr);
		m_pCharPreview->drawImmediate();
		static_cast<GR_CairoGraphics*>(m_pCharPreview->getGraphics())->setCairo(nullptr);
	}
}

void AP_UnixDialog_Styles::event_DeleteClicked(void)
{
	const char * szSel = getCurrentStyle();
	if (szSel && *szSel)
    {
		m_sNewStyleName = "";
		// copy — the doc/model churn below re-enters getCurrentStyle()
		// which reuses a static buffer
		const std::string style(szSel);

		UT_DEBUGMSG(("DOM: attempting to delete style %s\n", style.c_str()));

		bool removed = getDoc()->removeStyle(style.c_str()); // actually remove the style

		if (!removed)
		{
			const XAP_StringSet * pSS = m_pApp->getStringSet();
			std::string s;
			pSS->getValueUTF8 (AP_STRING_ID_DLG_Styles_ErrStyleCantDelete,s);

			getFrame()->showMessageBox (s.c_str(),
										XAP_Dialog_MessageBox::b_O,
										XAP_Dialog_MessageBox::a_OK);
			return;
		}

		getFrame()->repopulateCombos();
		_populateWindowData(); // force a refresh
		getDoc()->signalListeners(PD_SIGNAL_UPDATE_LAYOUT);
    }
}

void AP_UnixDialog_Styles::event_NewClicked(void)
{
	setIsNew(true);
	modifyRunModal();
	if(m_answer == AP_Dialog_Styles::a_OK)
	{
		m_sNewStyleName = getNewStyleName();
		createNewStyle(m_sNewStyleName.c_str());
		_populateCList();
	}
}

void AP_UnixDialog_Styles::event_SelectionChanged(void)
{
	// refresh the previews — the current style is read live off the
	// selection model via getCurrentStyle()
	_populatePreviews(false);
}

void AP_UnixDialog_Styles::event_ListClicked(const char * which)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();
	std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_LBL_InUse, s);

	if (s == which)
	{
		m_whichType = USED_STYLES;
	}
	else
	{
		pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_LBL_UserDefined, s);
		if (s == which)
			m_whichType = USER_STYLES;
		else
			m_whichType = ALL_STYLES;
	}

	// force a refresh of everything
	_populateWindowData();
}

/*****************************************************************/

GtkWidget * AP_UnixDialog_Styles::_constructWindow(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();

	// load the dialog from the UI file
	GtkBuilder* builder = newDialogBuilderFromResource("ap_UnixDialog_Styles.ui");

	GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "ap_UnixDialog_Styles"));
	std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_StylesTitle, s);
	gtk_window_set_title (GTK_WINDOW (window), s.c_str());

	// list of styles goes in the top left
	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbStyles")), pSS, AP_STRING_ID_DLG_Styles_Available);
	
	// listview
	m_tvStyles = GTK_WIDGET(gtk_builder_get_object(builder, "tvStyles"));

	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbList")), pSS, AP_STRING_ID_DLG_Styles_List);

	m_rbList1 = GTK_WIDGET(gtk_builder_get_object(builder, "rbList1"));
	localizeButton(m_rbList1, pSS, AP_STRING_ID_DLG_Styles_LBL_InUse);
	m_rbList2 = GTK_WIDGET(gtk_builder_get_object(builder, "rbList2"));
	localizeButton(m_rbList2, pSS, AP_STRING_ID_DLG_Styles_LBL_All);
	m_rbList3 = GTK_WIDGET(gtk_builder_get_object(builder, "rbList3"));
	localizeButton(m_rbList3, pSS, AP_STRING_ID_DLG_Styles_LBL_UserDefined);
	
	// previewing and description goes in the top right

	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbParagraph")), pSS, AP_STRING_ID_DLG_Styles_ParaPrev);
	GtkWidget *frameParaPrev = GTK_WIDGET(gtk_builder_get_object(builder, "frameParagraph"));
	m_wParaPreviewArea = gtk_drawing_area_new();
	gtk_widget_set_size_request(m_wParaPreviewArea, 300, 70);
	xap_gtk_container_add (frameParaPrev, m_wParaPreviewArea);
	gtk_widget_set_visible(m_wParaPreviewArea, TRUE);

	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbCharacter")), pSS, AP_STRING_ID_DLG_Styles_CharPrev);
	GtkWidget *frameCharPrev = GTK_WIDGET(gtk_builder_get_object(builder, "frameCharacter"));
	m_wCharPreviewArea = gtk_drawing_area_new();
	gtk_widget_set_size_request(m_wCharPreviewArea, 300, 50);
	xap_gtk_container_add (frameCharPrev, m_wCharPreviewArea);
	gtk_widget_set_visible(m_wCharPreviewArea, TRUE);

	localizeLabelMarkup(GTK_WIDGET(gtk_builder_get_object(builder, "lbDescription")), pSS, AP_STRING_ID_DLG_Styles_Description);
	m_lbAttributes = GTK_WIDGET(gtk_builder_get_object(builder, "lbAttributes"));

	// Pack buttons at the bottom of the dialog
	m_btNew = GTK_WIDGET(gtk_builder_get_object(builder, "btNew"));
	m_btDelete = GTK_WIDGET(gtk_builder_get_object(builder, "btDelete"));
	m_btModify = GTK_WIDGET(gtk_builder_get_object(builder, "btModify"));
	localizeButton(m_btModify, pSS, AP_STRING_ID_DLG_Styles_Modify);

	m_btApply = GTK_WIDGET(gtk_builder_get_object(builder, "btApply"));
	m_btClose = GTK_WIDGET(gtk_builder_get_object(builder, "btClose"));

	_connectSignals();

	g_object_unref(G_OBJECT(builder));
	return window;
}

void AP_UnixDialog_Styles::_connectSignals(void) const
{
	// connect signal for this list
	g_signal_connect (G_OBJECT(GTK_CHECK_BUTTON(m_rbList1)),
			  "toggled",
			  G_CALLBACK(s_typeslist_changed),
			  const_cast<void*>(reinterpret_cast<gconstpointer>(this)));

	g_signal_connect (G_OBJECT(GTK_CHECK_BUTTON(m_rbList2)),
			  "toggled",
			  G_CALLBACK(s_typeslist_changed),
			  const_cast<void*>(reinterpret_cast<gconstpointer>(this)));

	g_signal_connect (G_OBJECT(GTK_CHECK_BUTTON(m_rbList3)),
			  "toggled",
			  G_CALLBACK(s_typeslist_changed),
			  const_cast<void*>(reinterpret_cast<gconstpointer>(this)));
	
	/*
	g_signal_connect (G_OBJECT(GTK_COMBO(m_cbList)->entry), 
			  "changed",
			  G_CALLBACK(s_typeslist_changed),
			  (void*)reinterpret_cast<gconstpointer>(this));
	*/

	// connect signals for these 3 buttons
	g_signal_connect (G_OBJECT(m_btNew),
			  "clicked",
			  G_CALLBACK(s_newbtn_clicked),
			  const_cast<void*>(reinterpret_cast<gconstpointer>(this)));
	
	g_signal_connect (G_OBJECT(m_btModify),
			  "clicked",
			  G_CALLBACK(s_modifybtn_clicked),
			  const_cast<void*>(reinterpret_cast<gconstpointer>(this)));
	
	g_signal_connect (G_OBJECT(m_btDelete),
			  "clicked",
			  G_CALLBACK(s_deletebtn_clicked),
			  const_cast<void*>(reinterpret_cast<gconstpointer>(this)));
	
	// dialog buttons
	g_signal_connect (G_OBJECT(m_btApply),
			  "clicked",
			  G_CALLBACK(s_applybtn_clicked),
			  const_cast<void*>(reinterpret_cast<gconstpointer>(this)));

	g_signal_connect (G_OBJECT(m_btClose),
			  "clicked",
			  G_CALLBACK(s_closebtn_clicked),
			  const_cast<void*>(reinterpret_cast<gconstpointer>(this)));
}

void AP_UnixDialog_Styles::_populateCList(void)
{
	const PD_Style * pStyle;
	const gchar *org_name;

	size_t nStyles = getDoc()->getStyleCount();
	xxx_UT_DEBUGMSG(("DOM: we have %d styles\n", nStyles));

	if (m_listStyles == nullptr) {
		m_listStyles = XAP_list_store_new();
		/* the GTK3 code wrapped the store in a GtkTreeModelSort ordered
		 * by the localized name — do the same via GtkSortListModel +
		 * a collation-aware GtkStringSorter on the "text" property */
		GtkSorter *sorter = GTK_SORTER(gtk_string_sorter_new(
			gtk_property_expression_new(XAP_TYPE_DROP_DOWN_ITEM,
										nullptr, "text")));
		/* gtk_sort_list_model_new() consumes the model ref it is given,
		 * so hand it an extra ref and keep m_listStyles as our own
		 * reference for later repopulation via g_list_store_remove_all()
		 * + append */
		GtkSortListModel *sort =
			gtk_sort_list_model_new(G_LIST_MODEL(g_object_ref(m_listStyles)),
									sorter);
		g_object_unref(sorter);

		GtkSingleSelection *sel = gtk_single_selection_new(nullptr);
		/* match the old GtkTreeSelection defaults — the model must
		 * attach after autoselect is off or the ctor grabs row 0 */
		gtk_single_selection_set_autoselect(sel, FALSE);
		gtk_single_selection_set_can_unselect(sel, TRUE);
		gtk_single_selection_set_model(sel, G_LIST_MODEL(sort));
		gtk_list_view_set_model(GTK_LIST_VIEW(m_tvStyles),
								GTK_SELECTION_MODEL(sel));
		m_selStyles = sel;
		g_object_unref(sort);
		g_object_unref(sel);

		GtkListItemFactory *factory = XAP_list_item_text_factory();
		gtk_list_view_set_factory(GTK_LIST_VIEW(m_tvStyles), factory);
		g_object_unref(factory);
	} else {
		g_list_store_remove_all(m_listStyles);
	}

	bool highlight = false;
	std::vector<PD_Style*> *pStyles = nullptr;
	getDoc()->enumStyles(pStyles);
	for (UT_uint32 i = 0; i < nStyles; i++)
	{
		pStyle = (*pStyles)[i];

		// style has been deleted probably
		if (!pStyle)
			continue;

		org_name = pStyle->getName();

		std::string sLoc;
		pt_PieceTable::s_getLocalisedStyleName(org_name, sLoc);

		if ((m_whichType == ALL_STYLES) ||
			(m_whichType == USED_STYLES && pStyle->isUsed()) ||
			(m_whichType == USER_STYLES && pStyle->isUserDefined()) ||
			(m_sNewStyleName == sLoc)) /* show newly created style anyways */
		{
			/* localized name displayed, original name kept in string1
			 * for getCurrentStyle()/delete lookups */
			XAP_list_store_append_text_and_string(m_listStyles,
												  sLoc.c_str(), org_name);

			if (m_sNewStyleName == sLoc)
				highlight = true;
		}
	}
	DELETEP(pStyles);

	GListModel *sort = m_selStyles
		? gtk_single_selection_get_model(m_selStyles) : nullptr;
	const guint nRows = sort ? g_list_model_get_n_items(sort) : 0;
	guint pos = GTK_INVALID_LIST_POSITION;
	if (highlight)
	{
		// select new/modified — positions in the sorted model differ
		// from append order, so look the row up by its display name
		for (guint i = 0; i < nRows; i++)
		{
			XAPDropDownItem *item = XAP_DROP_DOWN_ITEM(
				g_list_model_get_item(sort, i));
			const bool match =
				item && m_sNewStyleName == xap_drop_down_item_get_text(item);
			g_object_unref(item);
			if (match)
			{
				pos = i;
				break;
			}
		}
	}
	if (pos == GTK_INVALID_LIST_POSITION && nRows > 0)
	{
		// select first
		pos = 0;
	}
	if (pos != GTK_INVALID_LIST_POSITION)
	{
		gtk_single_selection_set_selected(m_selStyles, pos);
		gtk_list_view_scroll_to(GTK_LIST_VIEW(m_tvStyles), pos,
								static_cast<GtkListScrollFlags>(
									GTK_LIST_SCROLL_FOCUS |
									GTK_LIST_SCROLL_SELECT),
								nullptr);
	}

	// selection "notify::selected" doesn't fire here while the signal
	// isn't connected yet (first populate), so refresh manually
	event_SelectionChanged();
}

void AP_UnixDialog_Styles::_populateWindowData(void)
{
	_populateCList();
	_populatePreviews(false);
}

void AP_UnixDialog_Styles::setDescription(const char * desc) const
{
	UT_ASSERT(m_lbAttributes);
	gtk_label_set_text (GTK_LABEL(m_lbAttributes), desc);
}

const char * AP_UnixDialog_Styles::getCurrentStyle (void) const
{
	static std::string sStyleBuf;

	UT_ASSERT(m_tvStyles);

	gpointer p = XAP_single_selection_get_item(m_selStyles);
	if (!p)
		return nullptr;

	const char * style =
		xap_drop_down_item_get_string1(XAP_DROP_DOWN_ITEM(p));

	if (!style)
		return nullptr;

	sStyleBuf = style;
	return sStyleBuf.c_str();
}

GtkWidget *  AP_UnixDialog_Styles::_constructModifyDialog(void)
{
	GtkWidget *modifyDialog;
	const XAP_StringSet * pSS = m_pApp->getStringSet();

	std::string title;

	if(!isNew())
		pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyTitle,title);
	else
		pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_NewTitle,title);

	modifyDialog = abiDialogNew("modify style dialog", TRUE, title.c_str());
	XAP_gtk_widget_set_margin(modifyDialog, 5);
	gtk_window_set_resizable(GTK_WINDOW(modifyDialog), FALSE);

	_constructModifyDialogContents(gtk_dialog_get_content_area(GTK_DIALOG (modifyDialog)));

	m_wModifyDialog = modifyDialog;

//
// Gnome buttons
//
	_constructGnomeModifyButtons();
//
// Connect signals
//

	_connectModifySignals();
	return modifyDialog;
}

void  AP_UnixDialog_Styles::_constructModifyDialogContents(GtkWidget * container)
{

	GtkWidget *dialog_vbox1 = nullptr;
	GtkWidget *OverallVbox = nullptr;
	GtkWidget *comboTable  = nullptr;
	GtkWidget *nameLabel  = nullptr;
	GtkWidget *basedOnLabel  = nullptr;
	GtkWidget *followingLabel = nullptr;
	GtkWidget *styleTypeLabel = nullptr;
	GtkWidget *styleNameEntry = nullptr;
	GtkWidget *basedOnCombo = nullptr;
	GtkWidget *followingCombo = nullptr;
	GtkWidget *styleTypeCombo = nullptr;
	GtkWidget *styleTypeEntry = nullptr;
	GtkWidget *previewFrame = nullptr;
	GtkWidget *modifyDrawingArea = nullptr;
	GtkWidget *DescriptionText = nullptr;
	GtkWidget *checkBoxRow = nullptr;
	GtkWidget *checkAddTo = nullptr;
	GtkWidget *checkAutoUpdate = nullptr;
	GtkWidget *deletePropCombo = nullptr;
	GtkWidget *deletePropButton = nullptr;
	const XAP_StringSet * pSS = m_pApp->getStringSet();

	dialog_vbox1 = container;
	gtk_widget_set_visible(dialog_vbox1, TRUE);

	OverallVbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_visible(OverallVbox, TRUE);
	gtk_box_append(GTK_BOX(dialog_vbox1), OverallVbox);
			gtk_widget_set_hexpand(OverallVbox, TRUE);
			gtk_widget_set_vexpand(OverallVbox, TRUE);
	XAP_gtk_widget_set_margin(OverallVbox, 5);

	comboTable = gtk_grid_new ();
	gtk_widget_set_hexpand (comboTable, TRUE);
	gtk_widget_set_visible(comboTable, TRUE);
	gtk_box_append(GTK_BOX(OverallVbox), comboTable);
			gtk_widget_set_hexpand(comboTable, TRUE);
			gtk_widget_set_vexpand(comboTable, TRUE);
	XAP_gtk_widget_set_margin(comboTable, 2);
	gtk_grid_set_column_spacing(GTK_GRID(comboTable), 2);

	std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyName,s);
	nameLabel = gtk_label_new(s.c_str());
	g_object_set(G_OBJECT(nameLabel),
                                    "xalign", 0.0, "yalign", 0.5,
                                    "justify", GTK_JUSTIFY_LEFT,
                                    "margin-start", 2, "margin-end", 2, "margin-top", 2, "margin-bottom", 2, "hexpand", TRUE, nullptr);
	gtk_widget_set_visible(nameLabel, TRUE);
	gtk_grid_attach(GTK_GRID (comboTable), nameLabel, 0, 0, 1, 1);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyType,s);
	styleTypeLabel = gtk_label_new(s.c_str());
	g_object_set(G_OBJECT(styleTypeLabel),
                                        "xalign", 0.0, "yalign", 0.5,
                                        "justify", GTK_JUSTIFY_LEFT,
                                        "margin-start", 2, "margin-end", 2, "margin-top", 2, "margin-bottom", 2, "hexpand", TRUE, nullptr);
	gtk_widget_set_visible(styleTypeLabel, TRUE);
	gtk_grid_attach (GTK_GRID (comboTable), styleTypeLabel, 1, 0, 1, 1);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyBasedOn,s);
	basedOnLabel = gtk_label_new(s.c_str());
	g_object_set(G_OBJECT(basedOnLabel),
                                       "xalign", 0.0, "yalign", 0.5,
                                       "justify", GTK_JUSTIFY_LEFT,
                                       "margin-start", 2, "margin-end", 2, "margin-top", 2, "margin-bottom", 2, nullptr);
	gtk_widget_set_visible(basedOnLabel, TRUE);
	gtk_grid_attach (GTK_GRID (comboTable), basedOnLabel, 0, 2, 1, 1);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyFollowing,s);
	followingLabel = gtk_label_new(s.c_str());
	g_object_set(G_OBJECT(followingLabel),
                                         "xalign", 0.0, "yalign", 0.5,
                                         "margin-start", 2, "margin-end", 2, "margin-top", 2, "margin-bottom", 2, nullptr);
	gtk_widget_set_visible(followingLabel, TRUE);
	gtk_grid_attach (GTK_GRID (comboTable), followingLabel, 1, 2, 1, 1);

	styleNameEntry = gtk_entry_new ();
	gtk_widget_set_visible(styleNameEntry, TRUE);
	gtk_grid_attach (GTK_GRID (comboTable), styleNameEntry, 0, 1, 1, 1);
	gtk_widget_set_size_request (styleNameEntry, 158, -1);

	basedOnCombo = gtk_drop_down_new (nullptr, nullptr);
	XAP_makeGtkDropDown(GTK_DROP_DOWN(basedOnCombo));
	gtk_widget_set_visible(basedOnCombo, TRUE);
	gtk_grid_attach (GTK_GRID (comboTable), basedOnCombo, 0, 3, 1, 1);

	followingCombo = gtk_drop_down_new (nullptr, nullptr);
	XAP_makeGtkDropDown(GTK_DROP_DOWN(followingCombo));
	gtk_widget_set_visible(followingCombo, TRUE);
	gtk_grid_attach(GTK_GRID(comboTable), followingCombo, 1, 3, 1, 1);
//
// Cannot modify style type attribute
//
	if(isNew())
	{
		styleTypeCombo = gtk_drop_down_new (nullptr, nullptr);
		XAP_makeGtkDropDown(GTK_DROP_DOWN(styleTypeCombo));
		gtk_widget_set_visible(styleTypeCombo, TRUE);
		gtk_grid_attach (GTK_GRID (comboTable), styleTypeCombo, 1, 1, 1, 1);
	}
	else
	{
		styleTypeEntry = gtk_entry_new ();
		gtk_editable_set_editable(GTK_EDITABLE(styleTypeEntry), FALSE);
		gtk_widget_set_visible(styleTypeEntry, TRUE);
		gtk_grid_attach (GTK_GRID (comboTable), styleTypeEntry, 1, 1, 1, 1);
		gtk_widget_set_size_request (styleTypeEntry, 158, -1);
	}

	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyPreview,s);
	s = "<b>" + s + "</b>";
	GtkWidget *lbPrevFrame = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(lbPrevFrame), s.c_str());
	gtk_widget_set_visible(lbPrevFrame, TRUE);
	previewFrame = gtk_frame_new(nullptr);
	gtk_frame_set_label_widget(GTK_FRAME(previewFrame), lbPrevFrame);
	gtk_widget_set_visible(previewFrame, TRUE);
	gtk_box_append(GTK_BOX(OverallVbox), previewFrame);
			gtk_widget_set_hexpand(previewFrame, TRUE);
			gtk_widget_set_vexpand(previewFrame, TRUE);
	XAP_gtk_widget_set_margin(previewFrame, 3);

	GtkWidget *wDrawFrame = gtk_frame_new(nullptr);
	gtk_widget_set_visible(wDrawFrame, TRUE);
	xap_gtk_container_add (previewFrame, wDrawFrame);
	XAP_gtk_widget_set_margin(wDrawFrame, 6);

	modifyDrawingArea = gtk_drawing_area_new();
	gtk_widget_set_size_request (modifyDrawingArea, -1, 85);
	xap_gtk_container_add (wDrawFrame, modifyDrawingArea);
	gtk_widget_set_visible(modifyDrawingArea, TRUE);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyDescription,s);
	s = "<b>" + s + "</b>";
	GtkWidget *lbDescrFrame = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(lbDescrFrame), s.c_str());
	gtk_widget_set_visible(lbDescrFrame, TRUE);
	GtkWidget *descriptionFrame = gtk_frame_new(nullptr);
	gtk_frame_set_label_widget(GTK_FRAME(descriptionFrame), lbDescrFrame);
	gtk_widget_set_visible(descriptionFrame, TRUE);
	gtk_box_append(GTK_BOX(OverallVbox), descriptionFrame);

	DescriptionText = gtk_label_new(nullptr);
	g_object_set(G_OBJECT(DescriptionText),
					  "margin-start", 0, "margin-end", 0, "margin-top", 6, "margin-bottom", 6,
					  "wrap", TRUE,
					  "max-width-chars", 64,
					  nullptr);
	gtk_widget_set_visible(DescriptionText, TRUE);
	xap_gtk_container_add (descriptionFrame, DescriptionText);
	gtk_widget_set_size_request(DescriptionText, 438, -1);
//
// Code to choose properties to be removed from the current style.
//
	GtkWidget * deleteRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_widget_set_visible(deleteRow, TRUE);
	gtk_box_append(GTK_BOX(OverallVbox), deleteRow);
			gtk_widget_set_hexpand(deleteRow, TRUE);
			gtk_widget_set_vexpand(deleteRow, TRUE);
	XAP_gtk_widget_set_margin(deleteRow, 2);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_RemoveLab,s);
	GtkWidget * deleteLabel = gtk_label_new(s.c_str());
	gtk_widget_set_visible(deleteLabel, TRUE);
	gtk_box_append(GTK_BOX(deleteRow), deleteLabel);
			gtk_widget_set_hexpand(deleteLabel, TRUE);
			gtk_widget_set_vexpand(deleteLabel, TRUE);

	deletePropCombo = gtk_drop_down_new (nullptr, nullptr);
	XAP_makeGtkDropDown(GTK_DROP_DOWN(deletePropCombo));
	gtk_widget_set_visible(deletePropCombo, TRUE);
	gtk_box_append(GTK_BOX(deleteRow), deletePropCombo);
			gtk_widget_set_hexpand(deletePropCombo, TRUE);
			gtk_widget_set_vexpand(deletePropCombo, TRUE);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_RemoveButton,s);
	deletePropButton = gtk_button_new_with_label(s.c_str());
	gtk_widget_set_visible(deletePropButton, TRUE);
	gtk_box_append(GTK_BOX(deleteRow), deletePropButton);
			gtk_widget_set_hexpand(deletePropButton, TRUE);
			gtk_widget_set_vexpand(deletePropButton, TRUE);

	checkBoxRow = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_box_append(GTK_BOX(OverallVbox), checkBoxRow);
			gtk_widget_set_hexpand(checkBoxRow, TRUE);
			gtk_widget_set_vexpand(checkBoxRow, TRUE);
	XAP_gtk_widget_set_margin(checkBoxRow, 2);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyTemplate,s);
	checkAddTo = gtk_check_button_new_with_label (s.c_str());
	gtk_widget_set_visible(checkAddTo, TRUE);
	gtk_box_append(GTK_BOX(checkBoxRow), checkAddTo);
			gtk_widget_set_hexpand(checkAddTo, TRUE);
			gtk_widget_set_vexpand(checkAddTo, TRUE);

	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyAutomatic,s);
	checkAutoUpdate = gtk_check_button_new_with_label (s.c_str());
	gtk_widget_set_visible(checkAutoUpdate, TRUE);
	gtk_box_append(GTK_BOX(checkBoxRow), checkAutoUpdate);
			gtk_widget_set_hexpand(checkAutoUpdate, TRUE);
			gtk_widget_set_vexpand(checkAutoUpdate, TRUE);

	GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_append(GTK_BOX(OverallVbox), box);
			gtk_widget_set_hexpand(box, TRUE);
			gtk_widget_set_vexpand(box, TRUE);
	gtk_widget_set_visible(box, TRUE);
	GtkWidget* formatMenu = gtk_drop_down_new (nullptr, nullptr);
	XAP_makeGtkDropDown(GTK_DROP_DOWN(formatMenu));
	gtk_widget_set_visible(formatMenu, TRUE);
	gtk_box_append(GTK_BOX(box), formatMenu);
	_constructFormatList(formatMenu);

//
// Save widget pointers in member variables
//
	m_wStyleNameEntry = styleNameEntry;
	m_wBasedOnCombo = basedOnCombo;
	m_wFollowingCombo = followingCombo;
	m_wStyleTypeCombo = styleTypeCombo;
	m_wStyleTypeEntry = styleTypeEntry;
	m_wModifyDrawingArea = modifyDrawingArea;
	m_wLabDescription = DescriptionText;
	m_wDeletePropCombo = deletePropCombo;
	m_wDeletePropButton = deletePropButton;
	m_wFormatMenu = formatMenu;
}

void   AP_UnixDialog_Styles::_constructGnomeModifyButtons()
{
	GtkWidget *buttonOK;
	GtkWidget *cancelButton;
	GtkWidget *shortCutButton = nullptr;
	const XAP_StringSet * pSS = m_pApp->getStringSet();

	cancelButton = abiAddButton(GTK_DIALOG(m_wModifyDialog),
                                    pSS->getValue(XAP_STRING_ID_DLG_Cancel),
                                    BUTTON_MODIFY_CANCEL);
	buttonOK = abiAddButton(GTK_DIALOG(m_wModifyDialog),
                                pSS->getValue(XAP_STRING_ID_DLG_OK),
                                BUTTON_MODIFY_OK);


	m_wModifyOk = buttonOK;
	m_wModifyCancel = cancelButton;
	m_wModifyShortCutKey = shortCutButton;
}


void  AP_UnixDialog_Styles::_constructFormatList(GtkWidget * FormatCombo)
{
	GtkDropDown *combo = GTK_DROP_DOWN(FormatCombo);
	const XAP_StringSet * pSS = m_pApp->getStringSet();
	std::string s;

	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyFormat,s);
	XAP_appendDropDownText(combo, s.c_str());

	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyParagraph,s);
	XAP_appendDropDownText(combo, s.c_str());

	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyFont,s);
	XAP_appendDropDownText(combo, s.c_str());

	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyNumbering,s);
	XAP_appendDropDownText(combo, s.c_str());

	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyLanguage,s);
	XAP_appendDropDownText(combo, s.c_str());
	gtk_drop_down_set_selected(combo, 0);
}


void AP_UnixDialog_Styles::_connectModifySignals(void)
{
	g_signal_connect(G_OBJECT(m_wFormatMenu),
					   "notify::selected",
					   G_CALLBACK(s_modify_format_cb),
					   reinterpret_cast<gpointer>(this));

	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(m_wModifyDrawingArea),
							s_modifyPreview_draw,
							reinterpret_cast<gpointer>(this), nullptr);

	g_signal_connect(G_OBJECT(m_wDeletePropButton),
					   "clicked",
					   G_CALLBACK(s_remove_property),
					   static_cast<gpointer>(this));

	g_signal_connect(G_OBJECT(m_wStyleNameEntry),
					   "changed",
					   G_CALLBACK(s_style_name),
					   static_cast<gpointer>(this));

	g_signal_connect(G_OBJECT(m_wBasedOnCombo),
					   "notify::selected",
					   G_CALLBACK(s_basedon),
					   static_cast<gpointer>(this));

	g_signal_connect(G_OBJECT(m_wFollowingCombo),
					   "notify::selected",
					   G_CALLBACK(s_followedby),
					   static_cast<gpointer>(this));

	// style type is a GtkDropDown for new styles, a read-only GtkEntry
	// when modifying an existing one — the two take different signals
	if (m_wStyleTypeCombo)
		g_signal_connect(G_OBJECT(m_wStyleTypeCombo),
					   "notify::selected",
					   G_CALLBACK(s_styletype),
					   static_cast<gpointer>(this));
	else if (m_wStyleTypeEntry)
		g_signal_connect(G_OBJECT(m_wStyleTypeEntry),
					   "changed",
					   G_CALLBACK(s_styletype_entry),
					   static_cast<gpointer>(this));
}


bool AP_UnixDialog_Styles::event_Modify_OK(void)
{
  const char * text = XAP_gtk_entry_get_text(GTK_EDITABLE(m_wStyleNameEntry));

  if (!text || !strlen (text))
    {
      // error message!
      const XAP_StringSet * pSS = m_pApp->getStringSet ();
      std::string s;
      pSS->getValueUTF8 (AP_STRING_ID_DLG_Styles_ErrBlankName,s);

      getFrame()->showMessageBox (s.c_str(),
				  XAP_Dialog_MessageBox::b_O,
				  XAP_Dialog_MessageBox::a_OK);

      return false;
    }

	// TODO save out state of radio items
	m_answer = AP_Dialog_Styles::a_OK;
	return true;
}

/*!
 * fill the properties vector with the values the given style.
 */
void AP_UnixDialog_Styles::new_styleName(void)
{
	static char message[200];
	const XAP_StringSet * pSS = m_pApp->getStringSet();
	const gchar * psz = XAP_gtk_entry_get_text(GTK_EDITABLE(m_wStyleNameEntry));
	std::string s;
	std::string s1;
	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_DefNone,s);

	if(psz && strcmp(psz,s.c_str())== 0)
	{
		// TODO: do a real error dialog
		pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ErrNotTitle1,s);
		pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ErrNotTitle2,s1);
		sprintf(message,"%s%s%s",s.c_str(),psz,s1.c_str());
		messageBoxOK(static_cast<const char *>(message));
		return;
	}

	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_DefCurrent,s);
	if(psz && strcmp(psz,s.c_str())== 0)
	{
		// TODO: do a real error dialog
		pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ErrNotTitle1,s);
		pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ErrNotTitle2,s1);
		sprintf(message,"%s%s%s",s.c_str(),psz,s1.c_str());
		messageBoxOK(static_cast<const char *>(message));
		return;
	}

	g_snprintf(static_cast<gchar *>(m_newStyleName),40,"%s",psz);
	PP_addOrSetAttribute(PT_NAME_ATTRIBUTE_NAME, getNewStyleName(), m_vecAllAttribs);
}

/*!
 * Remove the property from the current style shown in the remove combo box
 */
void AP_UnixDialog_Styles::event_RemoveProperty(void)
{
	const std::string prop =
		XAP_dropDownGetSelectedText(GTK_DROP_DOWN(m_wDeletePropCombo));
	PP_removeAttribute(prop.c_str(), m_vecAllProps);
	rebuildDeleteProps();
	updateCurrentStyle();
}

void AP_UnixDialog_Styles::rebuildDeleteProps(void)
{
	GtkDropDown * delCombo = GTK_DROP_DOWN(m_wDeletePropCombo);
	GListStore *model = G_LIST_STORE(gtk_drop_down_get_model(delCombo));

	g_list_store_remove_all(model);

	UT_sint32 i= 0;
	for(auto iter = m_vecAllProps.cbegin(); iter != m_vecAllProps.cend();
		++iter, ++i) {

		if ((i % 2) == 0) {
			XAP_appendDropDownText(delCombo, iter->c_str());
		}
	}
}

/*!
 * Update the properties and Attributes vector given the new basedon name
 */
void AP_UnixDialog_Styles::event_basedOn(void)
{
	const XAP_StringSet *pSS = m_pApp->getStringSet();
	const std::string sel =
		XAP_dropDownGetSelectedText(GTK_DROP_DOWN(m_wBasedOnCombo));
	const gchar * psz = sel.c_str();
	if (strcmp(psz, pSS->getValue(AP_STRING_ID_DLG_Styles_DefNone)) == 0)
		psz = "None";
	else
		psz = pt_PieceTable::s_getUnlocalisedStyleName(psz);
	g_snprintf(static_cast<gchar *>(m_basedonName),40,"%s",psz);
	PP_addOrSetAttribute("basedon", getBasedonName(), m_vecAllAttribs);
	updateCurrentStyle();
}


/*!
 * Update the Attributes vector given the new followedby name
 */
void AP_UnixDialog_Styles::event_followedBy(void)
{
	const XAP_StringSet *pSS = m_pApp->getStringSet();
	const std::string sel =
		XAP_dropDownGetSelectedText(GTK_DROP_DOWN(m_wFollowingCombo));
	const gchar * psz = sel.c_str();
	if (strcmp(psz, pSS->getValue(AP_STRING_ID_DLG_Styles_DefCurrent)) == 0)
		psz = "Current Settings";
	else
		psz = pt_PieceTable::s_getUnlocalisedStyleName(psz);
	g_snprintf(static_cast<gchar *>(m_followedbyName),40,"%s",psz);
	PP_addOrSetAttribute("followedby",getFollowedbyName(), m_vecAllAttribs);
}


/*!
 * Update the Attributes vector given the new Style Type
 */
void AP_UnixDialog_Styles::event_styleType(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();
	std::string s;

	const std::string sel = m_wStyleTypeCombo
		? XAP_dropDownGetSelectedText(GTK_DROP_DOWN(m_wStyleTypeCombo))
		: std::string(XAP_gtk_entry_get_text(GTK_EDITABLE(m_wStyleTypeEntry)));
	const gchar * psz = sel.c_str();
	g_snprintf(static_cast<gchar *>(m_styleType),40,"%s",psz);
	const gchar * pszSt = "P";
	pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyCharacter,s);
	if (strstr(m_styleType, s.c_str()) != nullptr)
		pszSt = "C";
	PP_addOrSetAttribute("type", pszSt, m_vecAllAttribs);
}

void AP_UnixDialog_Styles::event_Modify_Cancel(void)
{
	m_answer = AP_Dialog_Styles::a_CANCEL;
}

void AP_UnixDialog_Styles::event_ModifyDelete(void)
{
	m_answer = AP_Dialog_Styles::a_CANCEL;
}

void  AP_UnixDialog_Styles::modifyRunModal(void)
{
//
// OK Construct the new dialog and make it modal.
//
//
// pointer to the widget is stored in m_wModifyDialog
//
// Center our new dialog in its parent and make it a transient

	_constructModifyDialog();

//
// populate the dialog with useful info
//
	if(!_populateModify())
	{
		abiDestroyWidget(m_wModifyDialog);
		return;
	}

        abiSetupModalDialog(GTK_DIALOG(m_wModifyDialog), getFrame(), this, BUTTON_MODIFY_CANCEL);

	// make a new Unix GC

	DELETEP (m_pAbiPreviewWidget);
	GR_UnixCairoAllocInfo ai(m_wModifyDrawingArea);
	m_pAbiPreviewWidget =
	    static_cast<GR_CairoGraphics*>( XAP_App::getApp()->newGraphics(ai));

	// let the widget materialize

	GtkAllocation allocation;
	gtk_widget_get_allocation(m_wModifyDrawingArea, &allocation);
	_createAbiPreviewFromGC(m_pAbiPreviewWidget,
				static_cast<UT_uint32>(allocation.width),
				static_cast<UT_uint32>(allocation.height));
	_populateAbiPreview(isNew());

	bool inputValid;
	do
	{
		switch(abiRunModalDialog(GTK_DIALOG(m_wModifyDialog), false))
		{
			case BUTTON_MODIFY_OK:
				inputValid = event_Modify_OK();
				break;
			default:
				event_Modify_Cancel();
				inputValid = true;
				break ;
		}
	} while (!inputValid);

	if(m_wModifyDialog && GTK_IS_WIDGET(m_wModifyDialog))
	{
//
// Free the old glists
//
		m_gbasedOnStyles.clear();
		m_gfollowedByStyles.clear();
		m_gStyleType.clear();
		abiDestroyWidget(m_wModifyDialog); // TOPLEVEL
	}
//
// Have to delete this now since the destructor is not run till later
//
	destroyAbiPreview();
	DELETEP(m_pAbiPreviewWidget);
}

void AP_UnixDialog_Styles::event_ModifyPreviewInvalidate(void)
{
	invalidatePreview();
}

void AP_UnixDialog_Styles::event_ModifyPreviewDraw(cairo_t *cr)
{
	if (m_pAbiPreview) {
		static_cast<GR_CairoGraphics*>(m_pAbiPreview->getGraphics())->setCairo(cr);
		m_pAbiPreview->drawImmediate();
		static_cast<GR_CairoGraphics*>(m_pAbiPreview->getGraphics())->setCairo(nullptr);
	}
}


void AP_UnixDialog_Styles::event_ModifyClicked(void)
{
	PD_Style * pStyle = nullptr;
	const char * szCurrentStyle = getCurrentStyle ();
	m_sNewStyleName = szCurrentStyle ? szCurrentStyle : "";

	if(szCurrentStyle)
		getDoc()->getStyle(szCurrentStyle, &pStyle);

	if (!pStyle)
	{
		// TODO: error message - nothing selected
		return;
	}
//
// Allow built-ins to be modified
//

#if HIDE_MAIN_DIALOG
//
// Hide the old window
//
    gtk_widget_set_visible(m_windowMain, FALSE);
#endif
//
// fill the data structures needed for the Modify dialog
//
	setIsNew(false);

	modifyRunModal();
	if(m_answer == AP_Dialog_Styles::a_OK)
	{
		applyModifiedStyleToDoc();
		getDoc()->updateDocForStyleChange(getCurrentStyle(),true);
		getDoc()->signalListeners(PD_SIGNAL_UPDATE_LAYOUT);
	}
	else
	{
//
// Do other stuff
//
	}
//
// Restore the values in the main dialog
//

#if HIDE_MAIN_DIALOG
//
// Reveal main window again
//
	gtk_widget_set_visible( m_windowMain, TRUE);
#endif
}

void  AP_UnixDialog_Styles::setModifyDescription( const char * desc)
{
	UT_ASSERT(m_lbAttributes);
	gtk_label_set_text (GTK_LABEL(m_wLabDescription), desc);
}



static void
setComboContent(GtkDropDown * combo, const std::list<std::string> & content)
{
	GListStore *model =
		G_LIST_STORE(gtk_drop_down_get_model(combo));
	if (model)
		g_list_store_remove_all(model);
	std::list<std::string>::const_iterator iter(content.begin());
	for(; iter != content.end(); iter++) {
		XAP_appendDropDownText(combo, iter->c_str());
	}
}


bool  AP_UnixDialog_Styles::_populateModify(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();
//
// Don't do any callback while setting up stuff here.
//
	setModifySignalBlocked(true);
	setModifyDescription( m_curStyleDesc.c_str());
//
// Get Style name and put in in the text entry
//
	const char * szCurrentStyle = nullptr;
	std::string s;

	if(!isNew())
	{
		szCurrentStyle= getCurrentStyle();
		if(!szCurrentStyle)
		{
			// TODO: change me to use a real messagebox
			pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ErrNoStyle,s);
			messageBoxOK( s.c_str());
			m_answer = AP_Dialog_Styles::a_CANCEL;
			return false;
		}
		std::string sLoc;
		pt_PieceTable::s_getLocalisedStyleName(getCurrentStyle(), sLoc);
		XAP_gtk_entry_set_text(GTK_EDITABLE(m_wStyleNameEntry), sLoc.c_str());
		gtk_editable_set_editable(GTK_EDITABLE(m_wStyleNameEntry),FALSE );
	}
	else
	{
		gtk_editable_set_editable(GTK_EDITABLE(m_wStyleNameEntry),TRUE );
	}
//
// Next interogate the current style and find the based on and followed by
// Styles
//
	const char * szBasedOn = nullptr;
	const char * szFollowedBy = nullptr;
	PD_Style * pBasedOnStyle = nullptr;
	PD_Style * pFollowedByStyle = nullptr;
	if(!isNew())
	{
		PD_Style * pStyle = nullptr;
		if(szCurrentStyle)
			getDoc()->getStyle(szCurrentStyle,&pStyle);
		if(!pStyle)
		{
			// TODO: do a real error dialog
			pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ErrStyleNot,s);
			messageBoxOK( s.c_str());
			m_answer = AP_Dialog_Styles::a_CANCEL;
			return false;
		}
//
// Valid style get the Based On and followed by values
//
	    pBasedOnStyle = pStyle->getBasedOn();
		pFollowedByStyle = pStyle->getFollowedBy();
	}
//
// Next make a glists of all styles and attach them to the BasedOn and FollowedBy
//
	std::vector<PD_Style*> * pStyles = nullptr;
	getDoc()->enumStyles(pStyles);
	UT_sint32 nStyles = pStyles ? static_cast<UT_sint32>(pStyles->size()) : 0;
	for (UT_sint32 i = 0; i < nStyles; i++)
	{
		const PD_Style * pcStyle = (*pStyles)[i];
		UT_nonnull_or_continue(pcStyle);
		const char * name = pcStyle->getName();
		std::string sLoc;
		pt_PieceTable::s_getLocalisedStyleName(name, sLoc);
		if(pBasedOnStyle && pcStyle == pBasedOnStyle)
		{
			szBasedOn = name;
		}
		if(pFollowedByStyle && pcStyle == pFollowedByStyle)
			szFollowedBy = name;
		if(szCurrentStyle && strcmp(name,szCurrentStyle) != 0)
			m_gbasedOnStyles.push_back(sLoc);
		else if(szCurrentStyle == nullptr)
			m_gbasedOnStyles.push_back(sLoc);

		m_gfollowedByStyles.push_back(sLoc);
	}
	DELETEP(pStyles);

	m_gfollowedByStyles.sort();
	m_gfollowedByStyles.push_back(pSS->getValue(AP_STRING_ID_DLG_Styles_DefCurrent));
	m_gbasedOnStyles.sort();
	m_gbasedOnStyles.push_back(pSS->getValue(AP_STRING_ID_DLG_Styles_DefNone));
	m_gStyleType.push_back(pSS->getValue(AP_STRING_ID_DLG_Styles_ModifyParagraph));
	m_gStyleType.push_back(pSS->getValue(AP_STRING_ID_DLG_Styles_ModifyCharacter));

//
// Set the popdown list
//
	setComboContent(GTK_DROP_DOWN(m_wBasedOnCombo),m_gbasedOnStyles);
	setComboContent(GTK_DROP_DOWN(m_wFollowingCombo),m_gfollowedByStyles);
	if(isNew())
	{
		setComboContent(GTK_DROP_DOWN(m_wStyleTypeCombo),m_gStyleType);
	}
//
// OK here we set intial values for the basedOn and followedBy
//
	if(!isNew())
	{
		std::string sLoc;

		if(pBasedOnStyle != nullptr)
		{
			pt_PieceTable::s_getLocalisedStyleName(szBasedOn, sLoc);
			XAP_dropDownSetSelectedFromText(GTK_DROP_DOWN(m_wBasedOnCombo), sLoc.c_str());
		}
		else
		{
			pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_DefNone,s);
			XAP_dropDownSetSelectedFromText(GTK_DROP_DOWN(m_wBasedOnCombo), s.c_str());
		}

		if(pFollowedByStyle != nullptr)
		{
			pt_PieceTable::s_getLocalisedStyleName(szFollowedBy, sLoc);
			XAP_dropDownSetSelectedFromText(GTK_DROP_DOWN(m_wFollowingCombo), sLoc.c_str());
		}
		else
		{
			pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_DefCurrent,s);
			XAP_dropDownSetSelectedFromText(GTK_DROP_DOWN(m_wFollowingCombo), s.c_str());
		}

		const std::string & sType = PP_getAttribute(PT_TYPE_ATTRIBUTE_NAME, m_vecAllAttribs);
		if(sType.find("P") != std::string::npos)
		{
			pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyParagraph,s);
			XAP_gtk_entry_set_text(GTK_EDITABLE(m_wStyleTypeEntry),s.c_str());
		}
		else
		{
			pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyCharacter,s);
			XAP_gtk_entry_set_text(GTK_EDITABLE(m_wStyleTypeEntry),s.c_str());
		}
	}
	else
	{
//
// Hardwire defaults for "new"
//
		pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_DefNone,s);
		XAP_dropDownSetSelectedFromText(GTK_DROP_DOWN(m_wBasedOnCombo), s.c_str());
		pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_DefCurrent,s);
		XAP_dropDownSetSelectedFromText(GTK_DROP_DOWN(m_wFollowingCombo), s.c_str());
		pSS->getValueUTF8(AP_STRING_ID_DLG_Styles_ModifyParagraph,s);
		XAP_dropDownSetSelectedFromText(GTK_DROP_DOWN(m_wStyleTypeCombo), s.c_str());
	}
//
// Set these in our attributes vector
//
	event_basedOn();
	event_followedBy();
	event_styleType();
	if(isNew())
	{
		fillVecFromCurrentPoint();
	}
	else
	{
		fillVecWithProps(szCurrentStyle,true);
	}
//
// Allow callback's now.
//
	setModifySignalBlocked(false);
//
// Now set the list of properties which can be deleted.
//
	rebuildDeleteProps();
	gtk_drop_down_set_selected(GTK_DROP_DOWN(m_wDeletePropCombo),
							   GTK_INVALID_LIST_POSITION);
	return true;
}

void   AP_UnixDialog_Styles::event_ModifyParagraph()
{
#if HIDE_MAIN_DIALOG
//
// Hide this window
//
    gtk_widget_set_visible(m_wModifyDialog, FALSE);
#endif

//
// Can do all this in XP land.
//
	ModifyParagraph();
	rebuildDeleteProps();
#if HIDE_MAIN_DIALOG
//
// Restore this window
//
    gtk_widget_set_visible(m_wModifyDialog, TRUE);
#endif

//
// This applies the changes to current style and displays them
//
	updateCurrentStyle();
}

void   AP_UnixDialog_Styles::event_ModifyFont()
{
#if HIDE_MAIN_DIALOG
//
// Hide this window
//
    gtk_widget_set_visible(m_wModifyDialog, FALSE);
#endif

//
// Can do all this in XP land.
//
	ModifyFont();
	rebuildDeleteProps();
#if HIDE_MAIN_DIALOG
//
// Restore this window
//
    gtk_widget_set_visible(m_wModifyDialog, TRUE);
#endif

//
// This applies the changes to current style and displays them
//
	updateCurrentStyle();
}

void AP_UnixDialog_Styles::event_ModifyLanguage()
{
#if HIDE_MAIN_DIALOG
	gtk_widget_set_visible(m_wModifyDialog, FALSE);
#endif

	ModifyLang();
	rebuildDeleteProps();
#if HIDE_MAIN_DIALOG
	gtk_widget_set_visible(m_wModifyDialog, TRUE);
#endif

	updateCurrentStyle();
}

void   AP_UnixDialog_Styles::event_ModifyNumbering()
{
#if HIDE_MAIN_DIALOG
//
// Hide this window
//
    gtk_widget_set_visible(m_wModifyDialog, FALSE);
#endif

//
// Can do all this in XP land.
//
	ModifyLists();
	rebuildDeleteProps();
#if HIDE_MAIN_DIALOG
//
// Restore this window
//
    gtk_widget_set_visible(m_wModifyDialog, TRUE);
#endif

//
// This applies the changes to current style and displays them
//
	updateCurrentStyle();

}



bool  AP_UnixDialog_Styles::isModifySignalBlocked(void) const
{
	return m_bBlockModifySignal;
}

void  AP_UnixDialog_Styles::setModifySignalBlocked( bool val)
{
	m_bBlockModifySignal = val;
}
