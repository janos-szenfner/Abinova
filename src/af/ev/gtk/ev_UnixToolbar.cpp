/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t; -*- */

/* AbiSource Program Utilities
 * Copyright (C) 1998-2000 AbiSource, Inc.
 * Copyright (C) 2006 Robert Staudinger <robert.staudinger@gmail.com>
 * Copyright (c) 2023 Hubert Figuière
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

/*
 * Port to Maemo Development Platform
 * Author: INdT - Renato Araujo <renato.filho@indt.org.br>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include <algorithm>
#include <string>
#include <vector>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "ap_Features.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ut_string.h"
#include "ev_UnixToolbar.h"
#include "xap_Types.h"
#include "xap_UnixApp.h"
#include "xap_UnixFrameImpl.h"
#include "ev_Toolbar_Actions.h"
#include "ev_Toolbar_Layouts.h"
#include "ev_Toolbar_Labels.h"
#include "ev_Toolbar_Control.h"
#include "ev_EditEventMapper.h"
#include "xap_UnixTableWidget.h"
#include "ev_UnixToolbar_ViewListener.h"
#include "xap_GtkUtils.h"
#include "xav_View.h"
#include "xap_Prefs.h"
#include "fv_View.h"
#include "xap_EncodingManager.h"
#include "xap_UnixDialogHelper.h"
#include "ut_string_class.h"
#include "pt_PieceTable.h"
#include "ap_Toolbar_Id.h"
#include "ap_UnixStockIcons.h"
#include "ev_UnixFontCombo.h"
#include "xap_GtkUtils.h"
#include "xap_GtkComboBoxHelpers.h"


#define TOOLBAR_HSPACING 6
#define TOOLBAR_VSPACING 3



/*!
 * Append a widget to the toolbar,
 */
static GtkWidget *
toolbar_append_item (GtkBox *toolbar,
					 GtkWidget  *widget,
					 const char *text,
					 gboolean	 show)
{
	UT_ASSERT(GTK_IS_BOX (toolbar));
	UT_ASSERT(widget != nullptr);

	gtk_widget_set_tooltip_text(widget, text);
	XAP_gtk_a11y_name_from_tooltip(widget);

	gtk_box_append(GTK_BOX(toolbar), widget);
	if (show) {
		gtk_widget_set_visible(widget, TRUE);
	}

	return widget;
}

/*!
 * Append a GtkButton to the toolbar.
 */
static GtkWidget *
toolbar_append_button (GtkBox 	*toolbar,
					   const gchar	*icon_name,
					   const gchar  *tooltip,
					   GCallback	 handler,
					   gpointer		 data,
					   gulong		*handler_id)
{
	gchar* stock_id = abi_stock_from_toolbar_id(icon_name);
	GtkWidget* item = gtk_button_new_from_icon_name(stock_id);
	g_free(stock_id);
	stock_id = nullptr;
	*handler_id = g_signal_connect(G_OBJECT(item), "clicked", handler, data);

	return toolbar_append_item(toolbar, GTK_WIDGET(item), tooltip, TRUE);
}

/*!
 * Append a GtkToggleButton to the toolbar.
 */
static GtkWidget *
toolbar_append_toggle (GtkBox 	*toolbar,
					   const gchar	*icon_name,
					   const gchar  *tooltip,
					   GCallback	 handler,
					   gpointer		 data,
					   gboolean		 show,
					   gulong		*handler_id)
{
	gchar* stock_id = abi_stock_from_toolbar_id(icon_name);
	GtkWidget* icon = gtk_image_new_from_icon_name(stock_id);
	GtkWidget* item = gtk_toggle_button_new();
	gtk_button_set_child(GTK_BUTTON(item), icon);
	g_free (stock_id);
	stock_id = nullptr;
	*handler_id = g_signal_connect (G_OBJECT (item), "toggled", handler, data);

	return toolbar_append_item (toolbar, item, tooltip, show);
}


/*!
 * Append a GtkSeparator to the toolbar.
 */
static void
toolbar_append_separator (GtkBox *toolbar)
{
	GtkWidget* item = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_widget_set_margin_start(item, TOOLBAR_HSPACING);
	gtk_widget_set_margin_end(item, TOOLBAR_HSPACING);
	gtk_box_append(toolbar, item);
	gtk_widget_set_visible(item, TRUE);
}

/*!
 * Select an item by its display text in a toolbar combo widget.
 * Accepts an AbiFontCombo or a GtkDropDown fed by XAP_makeGtkDropDown().
 */
static gboolean
combo_box_set_active_text (GtkWidget *combo,
						   const gchar *text,
						   gulong		handler_id)
{
	if (ABI_IS_FONT_COMBO (combo)) {
		// the font combo is a custom widget; non existent entries are added
		g_signal_handler_block (G_OBJECT (combo), handler_id);
		if (!abi_font_combo_select_text (ABI_FONT_COMBO (combo), text)) {
			abi_font_combo_insert_font (ABI_FONT_COMBO (combo), text, TRUE);
		}
		g_signal_handler_unblock (G_OBJECT (combo), handler_id);
		return TRUE;
	}

	g_signal_handler_block (G_OBJECT (combo), handler_id);
	gboolean found = XAP_dropDownSetSelectedFromText (GTK_DROP_DOWN (combo), text);
	g_signal_handler_unblock (G_OBJECT (combo), handler_id);
	return found;
}

class _wd								// a private little class to help
{										// us remember all the widgets that
public:									// we create...
	_wd(EV_UnixToolbar * pUnixToolbar, XAP_Toolbar_Id id, GtkWidget * widget = nullptr)
	{
		m_pUnixToolbar = pUnixToolbar;
		m_id = id;
		m_widget = widget;
		m_blockSignal = false;
		m_handlerId = 0;
	};
	
	~_wd(void)
	{
	};

	static void s_callback(GtkWidget * /* widget */, gpointer user_data)
	{
		// this is a static callback method and does not have a 'this' pointer.
		// map the user_data into an object and dispatch the event.
	
		_wd * wd = static_cast<_wd *>(user_data);
		UT_return_if_fail(wd);
		if (!wd->m_blockSignal)
		{
			wd->m_pUnixToolbar->toolbarEvent(wd, nullptr, 0);
		}
	};

    // XXX I'm sure this doesn't belong here
	static void s_new_table(GtkWidget * /*table*/, int rows, int cols, gpointer* user_data)
	{
		// this is a static callback method and does not have a 'this' pointer.
		// map the user_data into an object and dispatch the event.

		_wd * wd = reinterpret_cast<_wd *>(user_data);
		UT_return_if_fail(wd);
		if (!wd->m_blockSignal && (rows > 0) && (cols > 0))
		{
			FV_View * pView = static_cast<FV_View *>(wd->m_pUnixToolbar->getFrame()->getCurrentView());
			if (pView)
				pView->cmdInsertTable(rows, cols, PP_NOPROPS);
		}
	}

	/*!
	 * Only accept numeric input in the toolbar's font size combo.
	 */
	static void s_insert_text_cb (GtkEditable *editable,
								  gchar       *new_text,
								  gint         new_text_length,
								  gint        /* *position */ , 
								  gpointer    /* data */)
	{
		gchar		*iter;
		gunichar	 c;

		// insert-text may report -1 when new_text is NUL-terminated
		if (new_text_length < 0)
			new_text_length = strlen(new_text);

		iter = new_text;
		while (iter < (new_text + new_text_length)) {
			c = g_utf8_get_char (iter);
			if (!g_unichar_isdigit (c)) {
				g_signal_stop_emission_by_name (G_OBJECT (editable), "insert-text");
				return;
			}
			iter = g_utf8_next_char (iter);
		}
	}

	/*!
	 * Apply font size upon <return>
	 */
	static gboolean	s_key_press_event_cb (GtkEventControllerKey * /*controller*/,
										  guint        keyval,
										  guint        /*keycode*/,
										  GdkModifierType /*state*/,
										  _wd         *wd)
	{
		if (keyval == GDK_KEY_Return) {
			s_combo_apply_changes (wd->m_widget, wd);
		}

		return FALSE;
	}

	/*!
	 * Apply changes after editing of the font size is done.
	 */
	static void	s_focus_out_event_cb (GtkEventControllerFocus * /*controller*/,
									  _wd           *wd)
	{
		s_combo_apply_changes (wd->m_widget, wd);
	}

	static void s_combo_changed(GtkWidget * combo, _wd * wd)
	{
		UT_return_if_fail(wd);

		// only act if the widget has been shown and embedded in the toolbar
		if (!wd->m_widget || wd->m_blockSignal) {
			return;
		}

		if (wd->m_id == static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_SIZE)) {
			// no updates of the font size while the entry is being edited
			GtkWidget *entry = GTK_WIDGET(g_object_get_data(
				G_OBJECT(wd->m_widget), "abi-size-entry"));
			if (entry && gtk_widget_has_focus(entry)) {
				return;
			}
		}

		s_combo_apply_changes (combo, wd);
	}

	static void s_dropdown_changed(GtkWidget * combo, GParamSpec * /*pspec*/,
								   _wd * wd)
	{
		s_combo_changed (combo, wd);
	}

	static void s_font_combo_changed(GtkWidget * combo, _wd * wd)
	{
		s_combo_changed (combo, wd);
	}

	/*!
	 * The size drop-down mirrors its selection into the paired entry;
	 * selecting an item also applies the size, unless the user is
	 * currently typing in the entry.
	 */
	static void s_size_drop_changed(GtkWidget * drop, GParamSpec * /*pspec*/,
									_wd * wd)
	{
		UT_return_if_fail(wd);
		if (!wd->m_widget || wd->m_blockSignal) {
			return;
		}
		GtkWidget *entry = GTK_WIDGET(g_object_get_data(
			G_OBJECT(wd->m_widget), "abi-size-entry"));
		if (entry && gtk_widget_has_focus(entry)) {
			return;
		}
		std::string sel =
			XAP_dropDownGetSelectedText (GTK_DROP_DOWN (drop));
		if (entry && !sel.empty()) {
			XAP_gtk_entry_set_text (GTK_EDITABLE (entry), sel.c_str());
		}
		s_combo_apply_changes (wd->m_widget, wd);
	}

	/*!
	 * Actually apply changes after a combo has been frobbed.
	 * This is not meant to be used as a signal handler, but rather to 
	 * implement common functionality after the decision has been made 
	 * whether to apply or not.
 	 */
	static void s_combo_apply_changes(GtkWidget * combo, _wd * wd)
	{
		const char *text;
		// TODO Rob: move this into ev_UnixFontCombo
		gchar *buffer = nullptr;
		if (ABI_IS_FONT_COMBO (combo)) {
			buffer = abi_font_combo_get_active_text (ABI_FONT_COMBO (combo));
		} else if (wd->m_id == static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_SIZE)) {
			// the size widget is an entry + drop-down pair; the typed
			// value may not be in the list, so the entry is authoritative
			GtkWidget *entry = GTK_WIDGET(g_object_get_data(
				G_OBJECT(wd->m_widget), "abi-size-entry"));
			if (entry) {
				const char *entryText =
					gtk_editable_get_text (GTK_EDITABLE (entry));
				if (entryText && *entryText) {
					buffer = g_strdup (entryText);
				}
			}
		} else if (GTK_IS_DROP_DOWN (combo)) {
			buffer = g_strdup (XAP_dropDownGetSelectedText (
								   GTK_DROP_DOWN (combo)).c_str());
		}
		if (!buffer) {
			return;
		}

		if (wd->m_id == static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_FONT)) {
			const gchar *font;
			font = XAP_EncodingManager::fontsizes_mapping.lookupByTarget(buffer);
			if (font) {
				g_free (buffer);
				buffer = g_strdup (font);
			}
		}

		if (wd->m_id == static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_STYLE))
			text = pt_PieceTable::s_getUnlocalisedStyleName(buffer);
		else
			text = buffer;

		if (!text) {
			g_free (buffer);
			return;
		}
		UT_UCS4String ucsText(text);
		wd->m_pUnixToolbar->toolbarEvent(wd, ucsText.ucs4_str(), ucsText.length());
		g_free (buffer);
	}

	EV_UnixToolbar *	m_pUnixToolbar;
	XAP_Toolbar_Id		m_id;
	GtkWidget *			m_widget;
	bool				m_blockSignal;
	gulong				m_handlerId;
};

static void
s_popdown_color_popover (GtkWidget *widget)
{
	GtkWidget * popover = gtk_widget_get_ancestor(widget, GTK_TYPE_POPOVER);
	if (popover)
		gtk_popover_popdown(GTK_POPOVER(popover));
}

static void
s_fore_color_changed (GtkColorChooser *cc,
					  GdkRGBA 		*color,
					  _wd 			*wd)
{
	UT_UTF8String str;

	UT_return_if_fail (wd);
	UT_return_if_fail (color);

	str = UT_UTF8String_sprintf ("%02x%02x%02x",
								 static_cast<int>(color->red   * 255),
								 static_cast<int>(color->green * 255),
								 static_cast<int>(color->blue  * 255));
	s_popdown_color_popover(GTK_WIDGET(cc));
	UT_UCS4String ucs = str.ucs4_str();
	wd->m_pUnixToolbar->toolbarEvent(wd, ucs.ucs4_str(), str.size());
}

static void
s_back_color_changed (GtkColorChooser *cc,
					  GdkRGBA 		*color,
					  _wd 			*wd)
{
	UT_UTF8String str;

	UT_return_if_fail (wd);
	UT_return_if_fail (color);

	str = UT_UTF8String_sprintf ("%02x%02x%02x",
								 static_cast<int>(color->red   * 255),
								 static_cast<int>(color->green * 255),
								 static_cast<int>(color->blue  * 255));
	s_popdown_color_popover(GTK_WIDGET(cc));
	UT_UCS4String ucs = str.ucs4_str();
	wd->m_pUnixToolbar->toolbarEvent(wd, ucs.ucs4_str(), str.size());
}

// the "automatic"/default entry of the color dropdown
static void
s_fore_color_automatic (GtkWidget * widget, _wd * wd)
{
	UT_return_if_fail (wd);
	s_popdown_color_popover(widget);
	const UT_UCS4Char black[] = {'0','0','0','0','0','0',0};
	wd->m_pUnixToolbar->toolbarEvent(wd, black, 6);
}

static void
s_back_color_automatic (GtkWidget * widget, _wd * wd)
{
	UT_return_if_fail (wd);
	s_popdown_color_popover(widget);
	const UT_UCS4Char transparent[] = {'t','r','a','n','s','p','a','r','e','n','t',0};
	wd->m_pUnixToolbar->toolbarEvent(wd, transparent, 10);
}

/*!
 * GTK4 replacement for go_combo_color_new(): a GtkMenuButton with a
 * popover containing a GtkColorChooserWidget and an "automatic" entry.
 */
static GtkWidget *
abi_color_button_new (const gchar *icon_name,
					  const gchar *automatic_label,
					  gboolean     is_back_color,
					  _wd         *wd)
{
	GtkWidget * button = gtk_menu_button_new();
	gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(button), icon_name);
	gtk_menu_button_set_direction(GTK_MENU_BUTTON(button), GTK_ARROW_DOWN);
	gtk_menu_button_set_has_frame(GTK_MENU_BUTTON(button), FALSE);

	GtkWidget * popover = gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_top(box, 6);
	gtk_widget_set_margin_bottom(box, 6);
	gtk_widget_set_margin_start(box, 6);
	gtk_widget_set_margin_end(box, 6);

	if (automatic_label && *automatic_label)
	{
		GtkWidget * automatic = gtk_button_new_with_label(automatic_label);
		g_signal_connect(G_OBJECT(automatic), "clicked",
						 is_back_color ? G_CALLBACK(s_back_color_automatic)
									   : G_CALLBACK(s_fore_color_automatic), wd);
		gtk_box_append(GTK_BOX(box), automatic);
	}

	GtkWidget * chooser = gtk_color_chooser_widget_new();
	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(chooser), FALSE);
	g_signal_connect(G_OBJECT(chooser), "color-activated",
					 is_back_color ? G_CALLBACK(s_back_color_changed)
								   : G_CALLBACK(s_fore_color_changed), wd);
	gtk_box_append(GTK_BOX(box), chooser);

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);

	return button;
}

EV_UnixToolbar::EV_UnixToolbar(XAP_UnixApp 	*pUnixApp, 
							   XAP_Frame 	*pFrame, 
							   const char 	*szToolbarLayoutName,
							   const char 	*szToolbarLabelSetName)
  : EV_Toolbar(pUnixApp->getEditMethodContainer(),
			   szToolbarLayoutName,
			   szToolbarLabelSetName), 
	m_pUnixApp(pUnixApp),
	m_pFrame(pFrame),
	m_pViewListener(nullptr),
	m_wToolbar(nullptr),
	m_wVSizeGroup(nullptr)
{}

EV_UnixToolbar::~EV_UnixToolbar(void)
{
	for (_wd * _utv_p : m_vecToolbarWidgets) { if (_utv_p) delete(_utv_p); };
	if(m_wVSizeGroup) {
		g_object_unref(m_wVSizeGroup);
	}
	_releaseListener();
}

GtkBox* EV_UnixToolbar::_getContainer()
{
	return GTK_BOX(static_cast<XAP_UnixFrameImpl *>(m_pFrame->getFrameImpl())->getVBoxWidget());
}

bool EV_UnixToolbar::toolbarEvent(_wd 				* wd,
								  const UT_UCS4Char	* pData,
								  UT_uint32 		  dataLength)

{
	// user selected something from this toolbar.
	// invoke the appropriate function.
	// return true iff handled.

	XAP_Toolbar_Id id = wd->m_id;

	const EV_Toolbar_ActionSet * pToolbarActionSet = m_pUnixApp->getToolbarActionSet();
	UT_return_val_if_fail(pToolbarActionSet, false);

	const EV_Toolbar_Action * pAction = pToolbarActionSet->getAction(id);
	UT_return_val_if_fail(pAction, false);

	AV_View * pView = m_pFrame->getCurrentView();
	UT_return_val_if_fail(pView, false);

	// make sure we ignore presses on "down" group buttons
	if (pAction->getItemType() == EV_TBIT_GroupButton)
	{
		const char * szState = nullptr;
		EV_Toolbar_ItemState tis = pAction->getToolbarItemState(pView,&szState);

		if (EV_TIS_ShouldBeToggled(tis))
		{
			// if this assert fires, you got a click while the button is down
			// if your widget set won't let you prevent this, handle it here

			UT_ASSERT(wd && wd->m_widget);
			
			// Block the signal, throw the button back up/down
			bool wasBlocked = wd->m_blockSignal;
			wd->m_blockSignal = true;
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wd->m_widget),
						     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wd->m_widget)));
			wd->m_blockSignal = wasBlocked;

			// can safely ignore this event
			return true;
		}
	}

	const char * szMethodName = pAction->getMethodName();
	if (!szMethodName)
		return false;

	const EV_EditMethodContainer * pEMC = m_pUnixApp->getEditMethodContainer();
	UT_return_val_if_fail(pEMC, false);

	EV_EditMethod * pEM = pEMC->findEditMethodByName(szMethodName);
	UT_return_val_if_fail(pEM, false);	// make sure it's bound to something

	invokeToolbarMethod(pView,pEM,pData,dataLength);
	return true;
}


/*!
 * This method destroys the container widget here and returns the position in
 * the overall vbox container.
 */
UT_sint32 EV_UnixToolbar::destroy(void)
{
	GtkWidget * wBox = GTK_WIDGET(_getContainer());
	UT_sint32  pos = 0;

	bool bFound = false;
	for (GtkWidget * child = gtk_widget_get_first_child(wBox);
		 child && !bFound;
		 child = gtk_widget_get_next_sibling(child))
	{
		if (child == m_wToolbar)
		{
			bFound = true;
			break;
		}
		pos++;
	}
	UT_ASSERT(bFound);
	if(!bFound)
	{
		pos = -1;
	}
//
// Now remove the view listener
//
	AV_View * pView = getFrame()->getCurrentView();
	if (pView)
		pView->removeListener(m_lid);
	_releaseListener();
//
// Finally destroy the old toolbar widget
//
	gtk_widget_unparent(m_wToolbar);
	m_wToolbar = nullptr;
//
// The _wd records describe children of the just-disposed box: drop
// them too.  Leaving them lets a later rebuildToolbar() append fresh
// entries while refreshToolbar() still resolves layout indices to
// the stale head of the vector (use-after-free on the old widgets).
//
	for (_wd * w : m_vecToolbarWidgets)
		delete w;
	m_vecToolbarWidgets.clear();
	return pos;
}

/*!
 * This method rebuilds the toolbar and places it in the position it previously
 * occupied.
 */
void EV_UnixToolbar::rebuildToolbar(UT_sint32 oldpos)
{
  //
  // Build the toolbar, place it in a handlebox at an arbitary place on the 
  // the frame.
  //
    synthesize();
	GtkBox * wBox = _getContainer();
	// GTK4: place the toolbar back at its old position by moving it
	// after the sibling that precedes that slot
	{
		GtkWidget * sibling = nullptr;
		UT_sint32 idx = 0;
		for (GtkWidget * child = gtk_widget_get_first_child(GTK_WIDGET(wBox));
			 child && idx < oldpos;
			 child = gtk_widget_get_next_sibling(child))
		{
			if (child != m_wToolbar)
			{
				sibling = child;
				idx++;
			}
		}
		gtk_box_reorder_child_after(wBox, m_wToolbar, sibling);
	}
//
// bind  view listener
//
	AV_View * pView = getFrame()->getCurrentView();
	if (pView)
		bindListenerToView(pView);
}

bool EV_UnixToolbar::synthesize(void)
{
	// create a GTK toolbar from the info provided.
	const EV_Toolbar_ActionSet * pToolbarActionSet = m_pUnixApp->getToolbarActionSet();
	UT_return_val_if_fail(pToolbarActionSet, false);

	XAP_Toolbar_ControlFactory * pFactory = m_pUnixApp->getControlFactory();
	UT_return_val_if_fail(pFactory, false);

	UT_uint32 nrLabelItemsInLayout = m_pToolbarLayout->getLayoutItemCount();
	UT_ASSERT(nrLabelItemsInLayout > 0);

	m_wToolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	UT_ASSERT(m_wToolbar);
	gtk_widget_set_margin_top(m_wToolbar, TOOLBAR_VSPACING);
	gtk_widget_set_margin_bottom(m_wToolbar, TOOLBAR_VSPACING);
	gtk_widget_set_margin_start(m_wToolbar, TOOLBAR_HSPACING);
	gtk_widget_set_margin_end(m_wToolbar, TOOLBAR_HSPACING);

//	gtk_toolbar_set_tooltips(GTK_TOOLBAR(m_wToolbar), TRUE);
// XXX gtk3 - porting to GtkBox
//	gtk_toolbar_set_show_arrow(GTK_TOOLBAR(m_wToolbar), TRUE);

	m_wVSizeGroup = gtk_size_group_new(GTK_SIZE_GROUP_VERTICAL);

	for (UT_uint32 k=0; (k < nrLabelItemsInLayout); k++)
	{
		/* The widget vector must stay index-aligned with the layout:
		 * refreshToolbar()/repopulateStyles() look entries up by layout
		 * index, so every slot pushes exactly one entry (nullptr for a
		 * slot that has no layout item). */
		EV_Toolbar_LayoutItem * pLayoutItem = m_pToolbarLayout->getLayoutItem(k);
		if (!pLayoutItem)
		{
			m_vecToolbarWidgets.push_back(nullptr);
			continue;
		}

		XAP_Toolbar_Id id = pLayoutItem->getToolbarId();
		EV_Toolbar_Action * pAction = pToolbarActionSet->getAction(id);
		EV_Toolbar_Label * pLabel = m_pToolbarLabelSet->getLabel(id);

		const char * szToolTip = nullptr;
		if (pLabel)
		{
			szToolTip = pLabel->getToolTip();
			if (!szToolTip || !*szToolTip)
				szToolTip = pLabel->getStatusMsg();
		}

		switch (pLayoutItem->getToolbarLayoutFlags())
		{
		case EV_TLF_Normal:
		{
			_wd * wd = new _wd(this,id);
			UT_ASSERT(wd);

			if (pAction && pLabel)
			switch (pAction->getItemType())
			{
			case EV_TBIT_PushButton:
			{
				UT_ASSERT(g_ascii_strcasecmp(pLabel->getIconName(),"NoIcon")!=0);
				if(pAction->getToolbarId() != static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_INSERT_TABLE))
				{
					wd->m_widget = toolbar_append_button(GTK_BOX(m_wToolbar), pLabel->getIconName(),
														  szToolTip,
														  reinterpret_cast<GCallback>( _wd::s_callback), static_cast<gpointer>( wd), 
														  &(wd->m_handlerId));
				}
				else
				{
//
// Hardwire the cool insert table widget onto the toolbar
//
					GtkWidget * abi_table = abi_table_new();
					const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
                    std::string sTable;
					pSS->getValueUTF8(XAP_STRING_ID_TB_Rows_x_Cols_Table,sTable);
                    std::string sCancel;
					pSS->getValueUTF8(XAP_STRING_ID_DLG_Cancel,sCancel);

					abi_table_set_labels(ABITABLE_WIDGET(abi_table),sTable.c_str(),sCancel.c_str());
					gtk_widget_set_visible(abi_table, TRUE);
					UT_DEBUGMSG(("SEVIOR: Made insert table widget \n"));
					wd->m_handlerId = g_signal_connect(abi_table, "selected",
													   G_CALLBACK (_wd::s_new_table),
													   static_cast<gpointer>(wd));

					UT_DEBUGMSG(("SEVIOR: Made connected to callback \n"));
                    std::string s;
					pSS->getValueUTF8(XAP_STRING_ID_TB_InsertNewTable, s);
					toolbar_append_item(GTK_BOX(m_wToolbar), abi_table,
										 s.c_str(), TRUE);
					gtk_widget_set_visible(abi_table, TRUE);
					wd->m_widget = abi_table;
				}
			}
			break;

			case EV_TBIT_ToggleButton:
			case EV_TBIT_GroupButton:
				{
					UT_ASSERT(g_ascii_strcasecmp(pLabel->getIconName(),"NoIcon")!=0);

					gboolean bShow = TRUE;
					wd->m_widget = toolbar_append_toggle(GTK_BOX(m_wToolbar), pLabel->getIconName(),
														  szToolTip,
														  reinterpret_cast<GCallback>( _wd::s_callback), static_cast<gpointer>( wd), 
														  bShow, &(wd->m_handlerId));
				}
				break;

			case EV_TBIT_EditText:
				break;

			case EV_TBIT_DropDown:
				break;

			case EV_TBIT_ComboBox:
			{
				EV_Toolbar_Control * pControl = pFactory->getControl(this, id);
				UT_ASSERT(pControl);

				GtkWidget *combo = nullptr;
				GtkWidget *listWidget = nullptr; /* the widget whose selection we watch */
				if (wd->m_id == static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_SIZE)) {
					/* GTK4 has no combo-with-entry: pair a GtkEntry
					 * with a GtkDropDown for the canned sizes. */
					combo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
					GtkWidget *entry = gtk_entry_new();
					gtk_widget_set_can_focus (entry, TRUE);
					gtk_editable_set_width_chars (GTK_EDITABLE(entry), 4);
					gtk_editable_set_max_width_chars (GTK_EDITABLE(entry), 6);
					g_signal_connect (G_OBJECT (entry), "insert-text", G_CALLBACK (_wd::s_insert_text_cb), nullptr);
					GtkEventController *focusController = gtk_event_controller_focus_new();
					g_signal_connect (G_OBJECT (focusController), "leave", G_CALLBACK (_wd::s_focus_out_event_cb), static_cast<gpointer>( wd));
					gtk_widget_add_controller (entry, focusController);
					GtkEventController *keyController = gtk_event_controller_key_new();
					g_signal_connect (G_OBJECT (keyController), "key-pressed", G_CALLBACK (_wd::s_key_press_event_cb), static_cast<gpointer>( wd));
					gtk_widget_add_controller (entry, keyController);
					gtk_box_append (GTK_BOX (combo), entry);
					listWidget = gtk_drop_down_new (nullptr, nullptr);
					XAP_makeGtkDropDown (GTK_DROP_DOWN (listWidget));
					gtk_box_append (GTK_BOX (combo), listWidget);
					g_object_set_data (G_OBJECT (combo), "abi-size-entry", entry);
					g_object_set_data (G_OBJECT (combo), "abi-size-drop", listWidget);
					wd->m_handlerId = g_signal_connect (G_OBJECT(listWidget), "notify::selected",
														G_CALLBACK(_wd::s_size_drop_changed),
														wd);
				}
				else if (wd->m_id == static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_FONT)) {
					combo = abi_font_combo_new ();
					gtk_widget_set_name (combo, "AbiFontCombo");
					listWidget = combo;
					wd->m_handlerId = g_signal_connect (G_OBJECT(listWidget), "changed",
														G_CALLBACK(_wd::s_font_combo_changed),
														wd);
				}
				else if (wd->m_id == static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_ZOOM)) {
					combo = gtk_drop_down_new (nullptr, nullptr);
					XAP_makeGtkDropDown (GTK_DROP_DOWN (combo));
					gtk_widget_set_name (combo, "AbiZoomCombo");
					listWidget = combo;
					wd->m_handlerId = g_signal_connect (G_OBJECT(listWidget), "notify::selected",
														G_CALLBACK(_wd::s_dropdown_changed),
														wd);
				}
				else if (wd->m_id == static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_STYLE)) {
					combo = gtk_drop_down_new (nullptr, nullptr);
					XAP_makeGtkDropDown (GTK_DROP_DOWN (combo));
					gtk_widget_set_name (combo, "AbiStyleCombo");
					listWidget = combo;
					wd->m_handlerId = g_signal_connect (G_OBJECT(listWidget), "notify::selected",
														G_CALLBACK(_wd::s_dropdown_changed),
														wd);
				}
				else {
					g_assert_not_reached();
				}

				// populate it
				if (pControl) {
					pControl->populate();
					const std::vector<const char*> * v = pControl->getContents();
					UT_ASSERT(v);
					gint items = v->size();
					if (ABI_IS_FONT_COMBO (combo)) {
						const gchar **fonts = g_new0 (const gchar *, items + 1);
						for (gint m=0; m < items; m++) {
							fonts[m] = (*v)[m];
						}						
						abi_font_combo_set_fonts (ABI_FONT_COMBO (combo), fonts);
						g_free (fonts); fonts = nullptr;
					}
					else {
						for (gint m=0; m < items; m++) {
							const char * sz = (*v)[m];
							std::string sLoc;
							if (wd->m_id == static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_STYLE))
							{
								pt_PieceTable::s_getLocalisedStyleName(sz, sLoc);
								sz = sLoc.c_str();
							}
							XAP_appendDropDownText (GTK_DROP_DOWN (listWidget), sz);
						}
					}
				}

				gtk_size_group_add_widget (m_wVSizeGroup, combo);
				// GTK4: a combo's internal entry/box is hexpand, which
				// propagates via gtk_widget_compute_expand and makes the
				// combo eat all free space in the toolbar box.
				gtk_widget_set_hexpand(combo, FALSE);
				gtk_widget_set_valign(combo, GTK_ALIGN_CENTER);
				gtk_widget_set_visible(combo, TRUE);
				toolbar_append_item(GTK_BOX(m_wToolbar), combo,
									szToolTip, TRUE);
				wd->m_widget = combo;
				// for now, we never repopulate, so can just toss it
				DELETEP(pControl);
			}
			break;

			case EV_TBIT_ColorFore:
			case EV_TBIT_ColorBack:
			{
				GtkWidget		*combo;

				const gchar* abi_stock_id;
				XAP_String_Id label_id;
				gboolean is_back_color;

				UT_ASSERT (g_ascii_strcasecmp(pLabel->getIconName(),"NoIcon") != 0);

				if (pAction->getItemType() == EV_TBIT_ColorFore) {
					abi_stock_id = ABIWORD_COLOR_FORE;
					label_id = XAP_STRING_ID_TB_ClearForeground;
					is_back_color = FALSE;
				} else {
					abi_stock_id = ABIWORD_COLOR_BACK;
					label_id = XAP_STRING_ID_TB_ClearBackground;
					is_back_color = TRUE;
				}
				const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
				std::string sClear;
				pSS->getValueUTF8(label_id, sClear);

				combo = abi_color_button_new(abi_stock_get_gtk_stock_id(abi_stock_id),
											 sClear.c_str(), is_back_color, wd);

				wd->m_widget = combo;

				toolbar_append_item(GTK_BOX(m_wToolbar), combo, szToolTip, TRUE);
			}
			break;
				
			case EV_TBIT_StaticLabel:
				// TODO do these...
				break;
					
			case EV_TBIT_Spacer:
				break;

			case EV_TBIT_MenuButton:
			case EV_TBIT_BOGUS:
			default:
				break;
			}
		// add item after bindings to catch widget returned to us
			m_vecToolbarWidgets.push_back(wd);
		}
		break;
			
		case EV_TLF_Spacer:
		{
			// Append to the vector even if spacer, to sync up with refresh
			// which expects each item in the layout to have a place in the
			// vector.
			_wd * wd = new _wd(this,id);
			UT_ASSERT(wd);
			m_vecToolbarWidgets.push_back(wd);

			toolbar_append_separator(GTK_BOX(m_wToolbar));
			break;
		}
		
		default:
			UT_ASSERT(0);
		}
	}

	GtkBox * wBox = _getContainer();

	// show the complete thing
	gtk_widget_set_visible(m_wToolbar, TRUE);

	// put it in the vbox
	gtk_box_append(wBox, m_wToolbar);

	setDetachable(getDetachable());

	return true;
}

void EV_UnixToolbar::_releaseListener(void)
{
	if (!m_pViewListener)
		return;
	DELETEP(m_pViewListener);
	m_pViewListener = nullptr;
	m_lid = 0;
}
	
bool EV_UnixToolbar::bindListenerToView(AV_View * pView)
{
	_releaseListener();
	
	m_pViewListener =
		new EV_UnixToolbar_ViewListener(this,pView);
	UT_ASSERT(m_pViewListener);

	bool bResult = pView->addListener(static_cast<AV_Listener *>(m_pViewListener),&m_lid);
	UT_ASSERT(bResult);
	m_pViewListener->setLID(m_lid);
	if(pView->isDocumentPresent())
	{
		refreshToolbar(pView, AV_CHG_ALL);
	}
	return bResult;
}

bool EV_UnixToolbar::refreshToolbar(AV_View * pView, AV_ChangeMask mask)
{
	// make the toolbar reflect the current state of the document
	// at the current insertion point or selection.

	const EV_Toolbar_ActionSet * pToolbarActionSet = m_pUnixApp->getToolbarActionSet();
	UT_ASSERT(pToolbarActionSet);
	
	UT_uint32 nrLabelItemsInLayout = m_pToolbarLayout->getLayoutItemCount();
	for (UT_uint32 k=0; (k < nrLabelItemsInLayout); k++)
	{
		EV_Toolbar_LayoutItem * pLayoutItem = m_pToolbarLayout->getLayoutItem(k);
		UT_continue_if_fail(pLayoutItem);

		XAP_Toolbar_Id id = pLayoutItem->getToolbarId();
		EV_Toolbar_Action * pAction = pToolbarActionSet->getAction(id);
		UT_continue_if_fail(pAction);
		UT_continue_if_fail(k < m_vecToolbarWidgets.size());

		AV_ChangeMask maskOfInterest = pAction->getChangeMaskOfInterest();
		if ((maskOfInterest & mask) == 0)					// if this item doesn't care about
			continue;										// changes of this type, skip it...

		switch (pLayoutItem->getToolbarLayoutFlags())
		{
		case EV_TLF_Normal:
			{
				const char * szState = nullptr;
				std::string sLoc;
				EV_Toolbar_ItemState tis = pAction->getToolbarItemState(pView,&szState);

                if( tis & EV_TIS_Hidden )
                    tis = static_cast<EV_Toolbar_ItemState>((tis | EV_TIS_Gray));
                
				switch (pAction->getItemType())
				{
				case EV_TBIT_PushButton:
				{
					bool bGrayed = EV_TIS_ShouldBeGray(tis);

					_wd * wd = m_vecToolbarWidgets[k];
					if (!wd || !wd->m_widget)
						continue;
					gtk_widget_set_sensitive(wd->m_widget, !bGrayed);     					
					gtk_widget_set_visible(wd->m_widget, !EV_TIS_ShouldBeHidden(tis));
				}
				break;
			
				case EV_TBIT_ToggleButton:
				case EV_TBIT_GroupButton:
				{
					bool bGrayed = EV_TIS_ShouldBeGray(tis);
					bool bToggled = EV_TIS_ShouldBeToggled(tis);

					_wd * wd = m_vecToolbarWidgets[k];
					if (!wd || !wd->m_widget)
						continue;
					// Block the signal, throw the toggle event
					bool wasBlocked = wd->m_blockSignal;
					wd->m_blockSignal = true;
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wd->m_widget), bToggled);
					wd->m_blockSignal = wasBlocked;
						
					// Disable/enable toolbar item
					gtk_widget_set_sensitive(wd->m_widget, !bGrayed);				
				}
				break;

				case EV_TBIT_EditText:
					break;
				case EV_TBIT_DropDown:
					break;
				case EV_TBIT_ComboBox:
				{
					bool bGrayed = EV_TIS_ShouldBeGray(tis);
					_wd * wd = m_vecToolbarWidgets[k];

					UT_nonnull_or_return(wd, false);
					UT_nonnull_or_return(wd->m_widget, false);

					GtkWidget * combo = wd->m_widget; /* font and size combos are wrappers */
					UT_ASSERT(combo);
					// Disable/enable toolbar combo
					gtk_widget_set_sensitive(combo, !bGrayed);

					// Block the signal, set the contents
					bool wasBlocked = wd->m_blockSignal;
					wd->m_blockSignal = true;
					if (!szState) {
						if (ABI_IS_FONT_COMBO (combo))
							abi_font_combo_unselect (ABI_FONT_COMBO (combo));
						else if (GTK_IS_DROP_DOWN (combo))
							gtk_drop_down_set_selected (GTK_DROP_DOWN (combo),
														GTK_INVALID_LIST_POSITION);
						else {
							GtkWidget *drop = GTK_WIDGET(g_object_get_data(
								G_OBJECT(combo), "abi-size-drop"));
							if (drop)
								gtk_drop_down_set_selected (GTK_DROP_DOWN (drop),
															GTK_INVALID_LIST_POSITION);
						}
					}
					else if (wd->m_id == static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_SIZE)) {
						GtkWidget *drop = GTK_WIDGET(g_object_get_data(
							G_OBJECT(combo), "abi-size-drop"));
						GtkWidget *entry = GTK_WIDGET(g_object_get_data(
							G_OBJECT(combo), "abi-size-entry"));
						const char * fsz = XAP_EncodingManager::fontsizes_mapping.lookupBySource(szState);
						gboolean ret = FALSE;
						if (drop && fsz) {
							ret = combo_box_set_active_text(drop, fsz, wd->m_handlerId);
						}
						if (entry) {
							XAP_gtk_entry_set_text(GTK_EDITABLE(entry),
											   ret ? fsz : szState);
						}
					}
					else if (wd->m_id == static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_STYLE)) {
#define BUILTIN_INDEX "builtin-index"
						pt_PieceTable::s_getLocalisedStyleName(szState, sLoc);
						szState = sLoc.c_str();
						gint idx = GPOINTER_TO_INT(g_object_steal_data(G_OBJECT(combo), BUILTIN_INDEX));
						if (idx > 0) {
							GListStore *store = G_LIST_STORE(
								gtk_drop_down_get_model (GTK_DROP_DOWN (combo)));
							guint n = g_list_model_get_n_items (G_LIST_MODEL (store));
							if (static_cast<guint>(idx) < n)
								g_list_store_remove (store, idx);
						}
						gboolean ret = combo_box_set_active_text(combo, szState, wd->m_handlerId);
						if (!ret) {
							// try again
							repopulateStyles();
							ret = combo_box_set_active_text(combo, szState, wd->m_handlerId);
							if (!ret) {
								// still not, hmm, this seems to be an internal style
								// we'll just display it and remove the entry when the carent moves away
								XAP_appendDropDownText (GTK_DROP_DOWN(combo), szState);
								combo_box_set_active_text(combo, szState, wd->m_handlerId);
								g_object_set_data (G_OBJECT (combo), BUILTIN_INDEX,
												   GINT_TO_POINTER(static_cast<gint>(
													   gtk_drop_down_get_selected(
														   GTK_DROP_DOWN(combo)))));
							}
						}
#undef BUILTIN_INDEX
					}
					else {
						gboolean ret = combo_box_set_active_text(combo, szState, wd->m_handlerId);
						if (!ret && wd->m_id == static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_ZOOM)) {
							// zoom set via dialog/keys (e.g. 125%) is not in
							// the static list; append it so the combo
							// reflects the real zoom (Debian #1010880)
							XAP_appendDropDownText (GTK_DROP_DOWN(combo), szState);
							combo_box_set_active_text(combo, szState, wd->m_handlerId);
						}
					}
					wd->m_blockSignal = wasBlocked;					
				}
				break;

                case EV_TBIT_ColorFore:
                case EV_TBIT_ColorBack:
                {
					bool bGrayed = EV_TIS_ShouldBeGray(tis);
					
					_wd * wd = m_vecToolbarWidgets[k];
					if (!wd || !wd->m_widget)
						continue;
					gtk_widget_set_sensitive(GTK_WIDGET(wd->m_widget), !bGrayed);   // Disable/enable toolbar item
                }
				break;

				case EV_TBIT_StaticLabel:
					break;
				case EV_TBIT_Spacer:
					break;
				case EV_TBIT_BOGUS:
					break;
				default:
					UT_ASSERT(0);
					break;
				}
			}
			break;
			
		case EV_TLF_Spacer:
			break;
			
		default:
			UT_ASSERT(0);
			break;
		}
	}

	return true;
}

XAP_UnixApp * EV_UnixToolbar::getApp(void)
{
	return m_pUnixApp;
}

XAP_Frame * EV_UnixToolbar::getFrame(void)
{
	return m_pFrame;
}

void EV_UnixToolbar::show(void)
{
	if (m_wToolbar) {
		gtk_widget_set_visible(m_wToolbar, TRUE);
	}
}

void EV_UnixToolbar::hide(void)
{

	if (m_wToolbar) {
		gtk_widget_set_visible(m_wToolbar, FALSE);
	}
	EV_Toolbar::hide();
}

/*!
 * This method examines the current document and repopulates the Styles
 * Combo box with what is in the document. It returns false if no styles 
 * combo box was found. True if it all worked.
 */
bool EV_UnixToolbar::repopulateStyles(void)
{
//
// First off find the Styles combobox in a toolbar somewhere
//
	UT_uint32 count = m_pToolbarLayout->getLayoutItemCount();
	UT_uint32 i =0;
	EV_Toolbar_LayoutItem * pLayoutItem = nullptr;
	XAP_Toolbar_Id id = static_cast<XAP_Toolbar_Id>(0);
	_wd * wd = nullptr;
	for(i=0; i < count; i++)
	{
		pLayoutItem = m_pToolbarLayout->getLayoutItem(i);
		if (!pLayoutItem || i >= m_vecToolbarWidgets.size())
			continue;
		id = pLayoutItem->getToolbarId();
		wd = m_vecToolbarWidgets[i];
		if(id == static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_STYLE))
			break;
	}
	if(i>=count || !wd || !wd->m_widget)
		return false;
//
// GOT IT!
//
	UT_ASSERT(wd->m_id == static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_STYLE));
	XAP_Toolbar_ControlFactory * pFactory = m_pUnixApp->getControlFactory();
	UT_return_val_if_fail(pFactory, false);
	EV_Toolbar_Control * pControl = pFactory->getControl(this, id);
	AP_UnixToolbar_StyleCombo * pStyleC = static_cast<AP_UnixToolbar_StyleCombo *>(pControl);
	UT_return_val_if_fail(pStyleC, false);
	pStyleC->repopulate();
	GtkDropDown * combo = GTK_DROP_DOWN (wd->m_widget);
//
// Now the combo box has to be refilled from this
//
	const std::vector<const char*> * v = pControl->getContents();
	UT_ASSERT(v);
//
// Try this....
//
	bool wasBlocked = wd->m_blockSignal;
	wd->m_blockSignal = true; // block the signal, so we don't try to read the text entry while this is happening..
	GListStore *store = G_LIST_STORE (gtk_drop_down_get_model (combo));
	g_list_store_remove_all (store);

//
// Localise and sort the style names, then refill the drop-down.
//
	gint items = v->size();
	std::vector<std::string> sorted;
	sorted.reserve (items);
	for (gint m=0; m < items; m++) {
		std::string sLoc;
		const char * sz = (*v)[m];
		pt_PieceTable::s_getLocalisedStyleName(sz, sLoc);
		sorted.push_back (sLoc);
	}
	std::sort (sorted.begin (), sorted.end ());
	for (const std::string & entry : sorted) {
		XAP_appendDropDownText (combo, entry.c_str ());
	}

	wd->m_blockSignal = wasBlocked;

//
// Don't need this anymore and we don't like memory leaks in abi
//
	delete pStyleC;
//
// I think we've finished!
//
	return true;
}
