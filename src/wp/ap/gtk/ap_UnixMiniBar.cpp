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

#include <algorithm>
#include <cstring>

#include "ap_UnixMiniBar.h"
#include "ap_UnixFrameImpl.h"
#include "ap_UnixActionIcons.h"
#include "ap_Toolbar_Id.h"
#include "ap_Prefs_SchemeIds.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "xap_GtkUtils.h"
#include "xap_EncodingManager.h"
#include "xap_Toolbar_LabelSet.h"
#include "ev_EditMethod.h"
#include "ev_Toolbar_Actions.h"
#include "ev_Toolbar_Labels.h"
#include "ev_UnixFontCombo.h"
#include "ev_UnixSizeCombo.h"
#include "fv_View.h"
#include "fv_FrameEdit.h"
#include "gr_Graphics.h"
#include "gr_CairoGraphics.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ut_misc.h"
#include "ut_string.h"

/* the toolbar ids this strip mirrors — the edit methods come from the
 * toolbar action set so this stays one code path with the ribbon */
static const XAP_Toolbar_Id s_minibar_toggles[] =
{
	static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_BOLD),
	static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_ITALIC),
	static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_UNDERLINE),
	static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_ALIGN_LEFT),
	static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_ALIGN_CENTER),
	static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_ALIGN_RIGHT),
	static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_ALIGN_JUSTIFY),
};

/* compact palette for the colour popover: neutrals + vivid row +
 * a couple of document-friendly darks */
static const char * const s_minibar_palette[] =
{
	"000000", "444444", "777777", "aaaaaa", "dddddd", "ffffff",
	"d01c11", "e69138", "f1c232", "38761d", "1155cc", "674ea7",
	"990000", "bf9000", "0c8577", "000099", "a64d79", "783f04"
};

AP_UnixMiniBar::AP_UnixMiniBar(XAP_Frame * pFrame)
	: m_pFrame(pFrame)
	, m_wBar(nullptr)
	, m_wFont(nullptr)
	, m_wSize(nullptr)
	, m_wBold(nullptr)
	, m_wItalic(nullptr)
	, m_wUnderline(nullptr)
	, m_wAlign{nullptr, nullptr, nullptr, nullptr}
	, m_wColorDlg(nullptr)
	, m_bSyncing(false)
	, m_bDismissed(false)
	, m_iBarW(-1)
	, m_iBarH(-1)
{
}

AP_UnixMiniBar::~AP_UnixMiniBar()
{
	if (m_wColorDlg)
	{
		g_signal_handlers_disconnect_by_data(m_wColorDlg, this);
		gtk_window_destroy(GTK_WINDOW(m_wColorDlg));
		m_wColorDlg = nullptr;
	}
	if (m_wBar)
	{
		g_signal_handlers_disconnect_by_data(m_wBar, this);
		gtk_widget_unparent(m_wBar);
		m_wBar = nullptr;
	}
	m_wFont = m_wSize = m_wBold = m_wItalic = m_wUnderline = nullptr;
	for (auto & w : m_wAlign)
		w = nullptr;
}

FV_View * AP_UnixMiniBar::_view() const
{
	return m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
}

/* dispatch exactly like a ribbon button: resolve the edit method and
 * run it on the current view, then hand focus back to the canvas so
 * typing continues without a click */
void AP_UnixMiniBar::_invokeMethod(const char * szMethod,
								   const char * szData)
{
	const EV_EditMethodContainer * pEMC =
		XAP_App::getApp()->getEditMethodContainer();
	UT_return_if_fail(pEMC);
	EV_EditMethod * pEM = pEMC->findEditMethodByName(szMethod);
	UT_return_if_fail(pEM);
	AV_View * pView = m_pFrame ? m_pFrame->getCurrentView() : nullptr;
	UT_return_if_fail(pView);
	EV_EditMethodCallData emcd(szData ? szData : "",
							   szData ? strlen(szData) : 0);
	pEM->Fn(pView, &emcd);

	/* the strip is transient chrome: once a command ran, typing
	 * should reach the document, not sit in a toolbar button */
	AP_UnixFrameImpl * impl =
		static_cast<AP_UnixFrameImpl *>(m_pFrame->getFrameImpl());
	if (impl)
		impl->focusDocument();
}

/* the combo dropdowns and the colour grid are GtkPopovers parented
 * inside the strip; hide() would otherwise leave one floating over
 * the document after the strip is gone */
static void s_popdown_popovers(GtkWidget * root)
{
	for (GtkWidget * c = gtk_widget_get_first_child(root); c;
		 c = gtk_widget_get_next_sibling(c))
	{
		if (GTK_IS_POPOVER(c))
			gtk_popover_popdown(GTK_POPOVER(c));
		s_popdown_popovers(c);
	}
}

/* position the strip over the selection without covering it.
 * Returns false when there is nothing to anchor to (empty
 * selection, frame-edit mode, or the selection is scrolled fully
 * out of the canvas). */
bool AP_UnixMiniBar::_place()
{
	FV_View * pView = _view();
	if (!pView || !m_wBar || pView->isSelectionEmpty())
		return false;
	FV_FrameEdit * pFE = pView->getFrameEdit();
	if (pFE && pFE->isActive())
		return false;

	GR_Graphics * pG = pView->getGraphics();
	GtkWidget * parent = gtk_widget_get_parent(m_wBar);
	if (!pG || !parent)
		return false;

	UT_Rect rStart, rEnd;
	if (!pView->getSelectionCaretRects(rStart, rEnd))
		return false;

	/* view-window layout units -> device pixels in canvas space;
	 * the overlay child shares the drawing area's coordinates */
	const int xStart = pG->tdu(rStart.left);
	const int yStart = pG->tdu(rStart.top);
	const int hStart = pG->tdu(rStart.height);
	const int xEnd   = pG->tdu(rEnd.left);
	const int yEnd   = pG->tdu(rEnd.top);
	const int hEnd   = pG->tdu(rEnd.height);

	const int areaW = gtk_widget_get_width(parent);
	const int areaH = gtk_widget_get_height(parent);
	if (areaW <= 0 || areaH <= 0)
		return false;

	/* both ends fully off-canvas -> nothing sane to anchor on */
	const int topY = std::min(yStart, yEnd);
	const int botY = std::max(yStart + hStart, yEnd + hEnd);
	if (botY < 0 || topY > areaH)
		return false;

	/* size: the strip's allocation while it is up, else the cached
	 * first measure.  Do NOT re-measure once any combo dropdown may
	 * have been realized — the popovers are parented children of the
	 * combos and inflate the box's natural size. */
	int barW = m_iBarW;
	int barH = m_iBarH;
	if (isVisible())
	{
		barW = gtk_widget_get_width(m_wBar);
		barH = gtk_widget_get_height(m_wBar);
	}
	if (barW <= 0 || barH <= 0)
	{
		s_popdown_popovers(m_wBar);
		gtk_widget_set_visible(m_wBar, TRUE);
		gtk_widget_measure(m_wBar, GTK_ORIENTATION_HORIZONTAL, -1,
						   nullptr, &barW, nullptr, nullptr);
		gtk_widget_measure(m_wBar, GTK_ORIENTATION_VERTICAL, barW,
						   nullptr, &barH, nullptr, nullptr);
		gtk_widget_set_visible(m_wBar, FALSE);
	}
	if (barW <= 0 || barH <= 0)
		return false;
	m_iBarW = barW;
	m_iBarH = barH;

	/* horizontal anchor: the moving end of the selection (the caret
	 * where the gesture finished), clamped inside the canvas */
	int x = xEnd;
	if (x < xStart && yEnd <= yStart)
		x = xStart;					/* rtl/upward drag: right edge */
	x = std::max(4, std::min(x, std::max(4, areaW - barW - 4)));

	/* above the selection's top line; below its bottom when the
	 * strip would run off the top of the canvas */
	int y = topY - barH - 10;
	if (y < 4)
		y = std::min(botY + 10, areaH - barH - 4);
	if (y < 4)
		y = 4;

	gtk_widget_set_margin_start(m_wBar, x);
	gtk_widget_set_margin_top(m_wBar, y);
	return true;
}

void AP_UnixMiniBar::show()
{
	if (!m_wBar || m_bDismissed)
		return;
	if (!_place())
		return;
	syncState();
	gtk_widget_set_visible(m_wBar, TRUE);
}

void AP_UnixMiniBar::reposition()
{
	if (!isVisible())
		return;
	if (!_place())
		hide();
}

void AP_UnixMiniBar::hide()
{
	if (!m_wBar)
		return;
	s_popdown_popovers(m_wBar);
	gtk_widget_set_visible(m_wBar, FALSE);
}

void AP_UnixMiniBar::dismiss()
{
	m_bDismissed = true;
	hide();
}

void AP_UnixMiniBar::clearDismissed()
{
	m_bDismissed = false;
}

bool AP_UnixMiniBar::isVisible() const
{
	return m_wBar && gtk_widget_get_visible(m_wBar);
}

bool AP_UnixMiniBar::contains(GtkWidget * w) const
{
	if (!m_wBar || !w)
		return false;
	if (w == m_wBar || gtk_widget_is_ancestor(w, m_wBar))
		return true;
	/* the colour grid and the combo dropdowns are GtkPopovers: their
	 * widgets sit in a separate tree rooted at the popover, which is
	 * itself parented to a widget inside the bar — a popover owned
	 * by other chrome (ribbon menus) does not count */
	GtkWidget * pop = gtk_widget_get_ancestor(w, GTK_TYPE_POPOVER);
	return pop && gtk_widget_get_parent(pop) &&
		gtk_widget_is_ancestor(gtk_widget_get_parent(pop), m_wBar);
}

/* reflect the selection's current char format — toggles pressed,
 * font/size combo text — without re-dispatching */
void AP_UnixMiniBar::syncState()
{
	const EV_Toolbar_ActionSet * pTBActions =
		XAP_App::getApp()->getToolbarActionSet();
	FV_View * pView = _view();
	if (!pTBActions || !pView)
		return;

	m_bSyncing = true;

	GtkWidget * toggles[G_N_ELEMENTS(s_minibar_toggles)] =
		{ m_wBold, m_wItalic, m_wUnderline,
		  m_wAlign[0], m_wAlign[1], m_wAlign[2], m_wAlign[3] };
	for (guint i = 0; i < G_N_ELEMENTS(s_minibar_toggles); ++i)
	{
		EV_Toolbar_Action * pAction =
			pTBActions->getAction(s_minibar_toggles[i]);
		if (!pAction || !toggles[i])
			continue;
		const char * szState = nullptr;
		EV_Toolbar_ItemState tis =
			pAction->getToolbarItemState(pView, &szState);
		gtk_widget_set_sensitive(toggles[i], !EV_TIS_ShouldBeGray(tis));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggles[i]),
									 EV_TIS_ShouldBeToggled(tis));
	}

	EV_Toolbar_Action * pFont =
		pTBActions->getAction(
			static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_FONT));
	EV_Toolbar_Action * pSize =
		pTBActions->getAction(
			static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_SIZE));

	if (pFont && m_wFont)
	{
		const char * szState = nullptr;
		pFont->getToolbarItemState(pView, &szState);
		if (szState && *szState)
		{
			if (!abi_font_combo_select_text(ABI_FONT_COMBO(m_wFont),
											szState))
				abi_font_combo_insert_font(ABI_FONT_COMBO(m_wFont),
										   szState, TRUE);
		}
		else
			abi_font_combo_unselect(ABI_FONT_COMBO(m_wFont));
	}
	if (pSize && m_wSize)
	{
		const char * szState = nullptr;
		pSize->getToolbarItemState(pView, &szState);
		if (szState && *szState)
		{
			const char * fsz =
				XAP_EncodingManager::fontsizes_mapping.lookupBySource(
					szState);
			abi_size_combo_select_text(ABI_SIZE_COMBO(m_wSize),
									   fsz ? fsz : szState);
		}
		else
			abi_size_combo_unselect(ABI_SIZE_COMBO(m_wSize));
	}

	m_bSyncing = false;
}

void AP_UnixMiniBar::_s_toggle_clicked(GtkWidget * w, gpointer data)
{
	AP_UnixMiniBar * self = static_cast<AP_UnixMiniBar *>(data);
	UT_return_if_fail(self);
	if (self->m_bSyncing)
		return;
	const char * szMethod = static_cast<const char *>(
		g_object_get_data(G_OBJECT(w), "abi-minibar-method"));
	UT_return_if_fail(szMethod && *szMethod);
	self->_invokeMethod(szMethod);
	/* keep the strip focused-look state honest without waiting for
	 * the next view notify */
	self->syncState();
}

void AP_UnixMiniBar::_s_font_changed(GtkWidget * w, gpointer data)
{
	AP_UnixMiniBar * self = static_cast<AP_UnixMiniBar *>(data);
	UT_return_if_fail(self);
	if (self->m_bSyncing)
		return;
	gchar * text = abi_font_combo_get_active_text(ABI_FONT_COMBO(w));
	if (text && *text)
		self->_invokeMethod("fontFamily", text);
	g_free(text);
}

void AP_UnixMiniBar::_s_size_changed(GtkWidget * w, gpointer data)
{
	AP_UnixMiniBar * self = static_cast<AP_UnixMiniBar *>(data);
	UT_return_if_fail(self);
	if (self->m_bSyncing)
		return;
	gchar * text = abi_size_combo_get_active_text(ABI_SIZE_COMBO(w));
	if (text && *text)
	{
		/* translate a localized canned size back, like the ribbon */
		const gchar * font =
			XAP_EncodingManager::fontsizes_mapping.lookupByTarget(text);
		self->_invokeMethod("fontSize", font ? font : text);
	}
	g_free(text);
}

void AP_UnixMiniBar::_applyColor(const char * szHex)
{
	_invokeMethod("colorForeTB", szHex);
}

void AP_UnixMiniBar::_s_swatch_clicked(GtkWidget * w, gpointer data)
{
	AP_UnixMiniBar * self = static_cast<AP_UnixMiniBar *>(data);
	const gchar * hex = static_cast<const gchar *>(
		g_object_get_data(G_OBJECT(w), "abi-color-hex"));
	UT_return_if_fail(self && hex);
	GtkWidget * popover =
		gtk_widget_get_ancestor(w, GTK_TYPE_POPOVER);
	if (popover)
		gtk_popover_popdown(GTK_POPOVER(popover));
	self->_applyColor(hex);
}

void AP_UnixMiniBar::_s_custom_response(GtkDialog * dlg, gint response,
										gpointer data)
{
	AP_UnixMiniBar * self = static_cast<AP_UnixMiniBar *>(data);
	if (response == GTK_RESPONSE_OK && self)
	{
		GdkRGBA color;
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dlg), &color);
		char hex[8];
		g_snprintf(hex, sizeof(hex), "%02x%02x%02x",
				   static_cast<int>(color.red   * 255),
				   static_cast<int>(color.green * 255),
				   static_cast<int>(color.blue  * 255));
		self->_applyColor(hex);
	}
	if (self && self->m_wColorDlg == GTK_WIDGET(dlg))
		self->m_wColorDlg = nullptr;
	gtk_window_destroy(GTK_WINDOW(dlg));
}

void AP_UnixMiniBar::_s_custom_clicked(GtkWidget * w, gpointer data)
{
	AP_UnixMiniBar * self = static_cast<AP_UnixMiniBar *>(data);
	UT_return_if_fail(self);
	GtkWidget * popover =
		gtk_widget_get_ancestor(w, GTK_TYPE_POPOVER);
	if (popover)
		gtk_popover_popdown(GTK_POPOVER(popover));

	GtkWidget * toplevel = GTK_WIDGET(gtk_widget_get_root(w));
	if (self->m_wColorDlg)
	{
		gtk_window_present(GTK_WINDOW(self->m_wColorDlg));
		return;
	}
	GtkWidget * dlg = gtk_color_chooser_dialog_new(
		"Custom Color", toplevel ? GTK_WINDOW(toplevel) : nullptr);
	GdkRGBA initial = { 0.0, 0.0, 0.0, 1.0 };
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dlg), &initial);
	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(dlg), FALSE);
	g_signal_connect(dlg, "response",
					 G_CALLBACK(_s_custom_response), self);
	self->m_wColorDlg = dlg;
	gtk_window_present(GTK_WINDOW(dlg));
}

/* Escape dismisses the strip even when a combo entry inside it owns
 * the key focus */
gboolean AP_UnixMiniBar::_s_key_pressed(GtkEventControllerKey * /*c*/,
										guint keyval, guint /*keycode*/,
										GdkModifierType /*state*/,
										gpointer data)
{
	AP_UnixMiniBar * self = static_cast<AP_UnixMiniBar *>(data);
	UT_return_val_if_fail(self, FALSE);
	if (keyval != GDK_KEY_Escape)
		return FALSE;
	self->dismiss();
	AP_UnixFrameImpl * impl = self->m_pFrame
		? static_cast<AP_UnixFrameImpl *>(self->m_pFrame->getFrameImpl())
		: nullptr;
	if (impl)
		impl->focusDocument();
	return TRUE;
}

GtkWidget * AP_UnixMiniBar::_makeColorButton()
{
	GtkWidget * button = gtk_menu_button_new();
	GtkWidget * glyph = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(glyph),
						 "<span font_weight='bold' underline='single' "
						 "underline_color='#d01c11'>A</span>");
	gtk_menu_button_set_child(GTK_MENU_BUTTON(button), glyph);
	gtk_menu_button_set_direction(GTK_MENU_BUTTON(button),
								  GTK_ARROW_DOWN);
	gtk_menu_button_set_has_frame(GTK_MENU_BUTTON(button), FALSE);
	gtk_widget_add_css_class(button, "flat");

	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_top(box, 6);
	gtk_widget_set_margin_bottom(box, 6);
	gtk_widget_set_margin_start(box, 6);
	gtk_widget_set_margin_end(box, 6);

	GtkWidget * grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 2);
	for (guint i = 0; i < G_N_ELEMENTS(s_minibar_palette); ++i)
	{
		GtkWidget * sw = gtk_button_new();
		gtk_widget_set_size_request(sw, 20, 20);
		gtk_widget_set_tooltip_text(sw, s_minibar_palette[i]);

		char css[160];
		g_snprintf(css, sizeof(css),
				   "button { background-image: none;"
				   " background-color: #%s;"
				   " min-width: 20px; min-height: 20px; padding: 0; }",
				   s_minibar_palette[i]);
		GtkCssProvider * cssProv = gtk_css_provider_new();
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		gtk_css_provider_load_from_string(cssProv, css);
G_GNUC_END_IGNORE_DEPRECATIONS
		gtk_style_context_add_provider(
			gtk_widget_get_style_context(sw),
			GTK_STYLE_PROVIDER(cssProv),
			GTK_STYLE_PROVIDER_PRIORITY_USER);
		g_object_unref(cssProv);

		g_object_set_data_full(G_OBJECT(sw), "abi-color-hex",
							   g_strdup(s_minibar_palette[i]), g_free);
		g_signal_connect(sw, "clicked",
						 G_CALLBACK(_s_swatch_clicked), this);
		gtk_grid_attach(GTK_GRID(grid), sw,
						i % 9, i / 9, 1, 1);
	}
	gtk_box_append(GTK_BOX(box), grid);

	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
	GtkWidget * custom = gtk_button_new_with_label(
		(pSS && pSS->getValue(XAP_STRING_ID_TB_CustomColor))
			? pSS->getValue(XAP_STRING_ID_TB_CustomColor)
			: "Custom Color...");
	g_signal_connect(custom, "clicked",
					 G_CALLBACK(_s_custom_clicked), this);
	gtk_box_append(GTK_BOX(box), custom);

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
	return button;
}

/* tooltip helper: pull the classic toolbar's tooltip for an id so
 * the strip speaks the same strings as the ribbon */
static const char * s_tip(EV_Toolbar_LabelSet * labels,
						  XAP_Toolbar_Id id, const char * fallback)
{
	EV_Toolbar_Label * pLabel = labels ? labels->getLabel(id) : nullptr;
	const char * tip = pLabel ? pLabel->getToolTip() : nullptr;
	return (tip && *tip) ? tip : fallback;
}

GtkWidget * AP_UnixMiniBar::createWidget()
{
	if (m_wBar)
		return m_wBar;

	EV_Toolbar_LabelSet * labels =
		AP_CreateToolbarLabelSet(AP_PREF_DEFAULT_StringSet);
	const EV_Toolbar_ActionSet * pTBActions =
		XAP_App::getApp()->getToolbarActionSet();

	m_wBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_widget_add_css_class(m_wBar, "abinova-minibar");
	gtk_widget_set_halign(m_wBar, GTK_ALIGN_START);
	gtk_widget_set_valign(m_wBar, GTK_ALIGN_START);
	gtk_widget_set_visible(m_wBar, FALSE);
	XAP_gtk_a11y_name(m_wBar, "Formatting mini toolbar");

	/* compact floating-card look, same family as the popovers */
	static const char minibar_css[] =
		".abinova-minibar {"
		"  background: @theme_bg_color;"
		"  border: 1px solid alpha(@theme_fg_color, 0.30);"
		"  border-radius: 6px; padding: 2px 4px;"
		"  box-shadow: 0 2px 8px alpha(black, 0.35);"
		"}"
		".abinova-minibar button { min-height: 0; padding: 2px 5px; }"
		".abinova-minibar menubutton { min-height: 0; }"
		".abinova-minibar entry { min-height: 0; }"
		".abinova-minibar separator { margin: 4px 2px; }";
	GtkCssProvider * css = gtk_css_provider_new();
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_css_provider_load_from_string(css, minibar_css);
G_GNUC_END_IGNORE_DEPRECATIONS
	gtk_style_context_add_provider_for_display(
		gtk_widget_get_display(m_wBar),
		GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(css);

	/* font family + size combos — the same widgets the ribbon rows
	 * host, slimmed down */
	m_wFont = abi_font_combo_new();
	{
		const std::vector<std::string> & fonts =
			GR_CairoGraphics::getAllFontNames();
		const gchar ** list = g_new0(const gchar *, fonts.size() + 1);
		for (size_t i = 0; i < fonts.size(); ++i)
			list[i] = fonts[i].c_str();
		abi_font_combo_set_fonts(ABI_FONT_COMBO(m_wFont), list);
		g_free(list);
		gtk_editable_set_width_chars(
			GTK_EDITABLE(ABI_FONT_COMBO(m_wFont)->entry), 10);
	}
	gtk_widget_set_tooltip_text(m_wFont,
		s_tip(labels,
			  static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_FONT),
			  "Font"));
	g_signal_connect(m_wFont, "changed",
					 G_CALLBACK(_s_font_changed), this);
	gtk_box_append(GTK_BOX(m_wBar), m_wFont);

	m_wSize = abi_size_combo_new();
	{
		int n = XAP_EncodingManager::fontsizes_mapping.size();
		const gchar ** list = g_new0(const gchar *, n + 1);
		for (int i = 0; i < n; ++i)
			list[i] = XAP_EncodingManager::fontsizes_mapping.nth2(i);
		abi_size_combo_set_sizes(ABI_SIZE_COMBO(m_wSize), list);
		g_free(list);
		gtk_editable_set_width_chars(
			GTK_EDITABLE(ABI_SIZE_COMBO(m_wSize)->entry), 3);
	}
	gtk_widget_set_tooltip_text(m_wSize,
		s_tip(labels,
			  static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID_FMT_SIZE),
			  "Font size"));
	g_signal_connect(m_wSize, "changed",
					 G_CALLBACK(_s_size_changed), this);
	gtk_box_append(GTK_BOX(m_wBar), m_wSize);

	gtk_box_append(GTK_BOX(m_wBar),
				   gtk_separator_new(GTK_ORIENTATION_VERTICAL));

	GtkWidget ** toggles[] =
		{ &m_wBold, &m_wItalic, &m_wUnderline,
		  &m_wAlign[0], &m_wAlign[1], &m_wAlign[2], &m_wAlign[3] };
	static const char * const fallbacks[] =
		{ "Bold", "Italic", "Underline",
		  "Align Left", "Align Center", "Align Right", "Justify" };
	for (guint i = 0; i < G_N_ELEMENTS(s_minibar_toggles); ++i)
	{
		/* the alignment cluster sits behind its own hairline */
		if (i == 3)
			gtk_box_append(GTK_BOX(m_wBar),
						   gtk_separator_new(GTK_ORIENTATION_VERTICAL));
		EV_Toolbar_Action * pAction =
			pTBActions ? pTBActions->getAction(s_minibar_toggles[i])
					   : nullptr;
		const char * szMethod =
			pAction ? pAction->getMethodName() : nullptr;
		if (!szMethod)
			continue;
		GtkWidget * b = gtk_toggle_button_new();
		gtk_widget_add_css_class(b, "flat");
		const char * szIcon = abi_action_icon_for_method(szMethod);
		GtkWidget * image = abi_action_icon_widget(szIcon, 16);
		if (image)
			gtk_button_set_child(GTK_BUTTON(b), image);
		else
			gtk_button_set_label(GTK_BUTTON(b), fallbacks[i]);
		gtk_widget_set_tooltip_text(
			b, s_tip(labels, s_minibar_toggles[i], fallbacks[i]));
		XAP_gtk_a11y_name_from_tooltip(b);
		g_object_set_data_full(G_OBJECT(b), "abi-minibar-method",
							   g_strdup(szMethod), g_free);
		g_signal_connect(b, "clicked",
						 G_CALLBACK(_s_toggle_clicked), this);
		gtk_box_append(GTK_BOX(m_wBar), b);
		*toggles[i] = b;
	}

	gtk_box_append(GTK_BOX(m_wBar),
				   gtk_separator_new(GTK_ORIENTATION_VERTICAL));

	GtkWidget * color = _makeColorButton();
	gtk_widget_set_tooltip_text(color, "Font color");
	XAP_gtk_a11y_name_from_tooltip(color);
	gtk_box_append(GTK_BOX(m_wBar), color);

	/* Escape while a combo inside the strip has focus dismisses it */
	GtkEventController * key = gtk_event_controller_key_new();
	g_signal_connect(key, "key-pressed",
					 G_CALLBACK(_s_key_pressed), this);
	gtk_widget_add_controller(m_wBar, key);

	DELETEP(labels);
	return m_wBar;
}
