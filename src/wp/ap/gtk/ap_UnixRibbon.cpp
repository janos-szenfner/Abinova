/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:t -*- */
/* AbiWord
 * Copyright (C) 2026 AbiSource, Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ap_UnixRibbon.h"

#include "ut_vector.h"
#include "ut_debugmsg.h"
#include "ut_string.h"
#include "ut_string_class.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "xap_Strings.h"
#include "xap_EncodingManager.h"
#include "ev_UnixMenuBar.h"
#include "ev_UnixFontCombo.h"
#include "ev_Menu_Labels.h"
#include "ev_Menu_Actions.h"
#include "ev_EditMethod.h"
#include "ev_Toolbar_Actions.h"
#include "ev_Toolbar_Labels.h"
#include "xap_Toolbar_LabelSet.h"
#include "ap_Toolbar_Id.h"
#include "ap_Prefs_SchemeIds.h"
#include "ap_UnixStockIcons.h"
#include "ap_Ribbon_Layouts.h"
#include "gr_CairoGraphics.h"
#include "pt_PieceTable.h"
#include "fv_View.h"

/*
 * Tab and group titles.  The layout table stores untranslated keys;
 * these English titles are the ribbon's labels.  (Localizing them is
 * future work - they are not in the .strings machinery yet.)
 */
struct _ribbon_kv { const char * szKey; const char * szLabel; };

static const _ribbon_kv s_ribbon_tab_labels[] =
{
	{ "file",       "File" },
	{ "home",       "Home" },
	{ "insert",     "Insert" },
	{ "references", "References" },
	{ "layout",     "Layout" },
	{ "review",     "Review" },
	{ "view",       "View" },
	{ "table",      "Table" },
	{ "help",       "Help" },
	{ nullptr,       nullptr }
};

static const _ribbon_kv s_ribbon_group_labels[] =
{
	{ "document",    "Document" },
	{ "print",       "Print" },
	{ "clipboard",   "Clipboard" },
	{ "font",        "Font" },
	{ "paragraph",   "Paragraph" },
	{ "lists",       "Lists" },
	{ "styles",      "Styles" },
	{ "editing",     "Editing" },
	{ "pages",       "Pages" },
	{ "tables",      "Tables" },
	{ "illustrations","Illustrations" },
	{ "links",       "Links" },
	{ "text",        "Text" },
	{ "symbols",     "Symbols" },
	{ "fields",      "Fields" },
	{ "toc",         "Table of Contents" },
	{ "notes",       "Footnotes" },
	{ "views",       "Views" },
	{ "page",        "Page Setup" },
	{ "columns",     "Page Columns" },
	{ "background",  "Page Background" },
	{ "zoom",        "Zoom" },
	{ "proofing",    "Proofing" },
	{ "revisions",   "Revisions" },
	{ "annotations", "Annotations" },
	{ "show",        "Show" },
	{ "window",      "Window" },
	{ "insert",      "Insert" },
	{ "delete",      "Delete" },
	{ "select",      "Select" },
	{ "format",      "Format" },
	{ "help",        "Help" },
	{ "interface",   "Interface" },
	{ nullptr,        nullptr }
};

static const char * _ribbon_label(const char * szKey,
								  const _ribbon_kv * table)
{
	for (; table->szKey; ++table)
		if (strcmp(table->szKey, szKey) == 0)
			return table->szLabel;
	return szKey;
}

/* strip mnemonic markers (& and _) from a menu label */
static void _ribbon_strip_mnemonic(const char * szIn, char * szOut, size_t outSize)
{
	size_t o = 0;
	for (const char * p = szIn; *p && o + 1 < outSize; ++p)
	{
		if ((*p == '&' || *p == '_') && p[1])
			continue;
		szOut[o++] = *p;
	}
	szOut[o] = '\0';
}

/*
 * Per-toolbar-item bookkeeping: the widget, the toolbar id, and a
 * flag that suppresses signal handling while refresh() pokes the
 * widget's state (same role as _wd::m_blockSignal in ev_UnixToolbar).
 */
struct AP_UnixRibbon::_TbCtx
{
	AP_UnixRibbon *	self;
	XAP_Toolbar_Id	id;
	GtkWidget *		widget;
	bool			blockSignal;
	gulong			handlerId;
};

AP_UnixRibbon::AP_UnixRibbon(XAP_Frame * pFrame, EV_UnixMenuBar * pMenu)
	: m_pFrame(pFrame)
	, m_pMenu(pMenu)
	, m_wNotebook(nullptr)
	, m_pTBLabels(nullptr)
	, m_pIconMap(nullptr)
{
}

AP_UnixRibbon::~AP_UnixRibbon()
{
	// m_wNotebook is owned by the widget tree; nothing to unref here.
	g_clear_pointer(&m_pIconMap, g_hash_table_unref);
	DELETEP(m_pTBLabels);
	UT_VECTOR_PURGEALL(_TbCtx *, m_vecTbCtx);
}

/*
 * Toolbar icons are keyed by toolbar id, while the ribbon is built
 * from menu ids.  Both action sets name the same edit methods, so we
 * bridge them: for every toolbar id, map its edit-method name to the
 * icon the classic toolbar would show.  Ribbon buttons then look up
 * their icon by the menu action's method name.  The toolbar label set
 * is also kept for toolbar-backed items (labels, tooltips, icons).
 */
void AP_UnixRibbon::_buildIconMap()
{
	if (m_pIconMap)
		return;
	m_pIconMap = g_hash_table_new_full(g_str_hash, g_str_equal,
									   g_free, g_free);

	const EV_Toolbar_ActionSet * pTBActions =
		XAP_App::getApp()->getToolbarActionSet();
	if (!pTBActions)
		return;

	/* the label-set language follows the StringSet pref, the same way
	 * xap_Frame::initialize picks m_szToolbarLabelSetName */
	std::string lang;
	if (!XAP_App::getApp()->getPrefsValue(AP_PREF_KEY_StringSet, lang) ||
		lang.empty())
		lang = AP_PREF_DEFAULT_StringSet;
	m_pTBLabels = AP_CreateToolbarLabelSet(lang.c_str());
	if (!m_pTBLabels)
		return;

	for (UT_uint32 tid = 1; tid < (UT_uint32)AP_TOOLBAR_ID__BOGUS2__; ++tid)
	{
		EV_Toolbar_Action * pTBAction =
			pTBActions->getAction((XAP_Toolbar_Id)tid);
		EV_Toolbar_Label * pTBLabel =
			m_pTBLabels->getLabel((XAP_Toolbar_Id)tid);
		if (!pTBAction || !pTBLabel)
			continue;
		const char * szMethod = pTBAction->getMethodName();
		const char * szIcon = pTBLabel->getIconName();
		if (!szMethod || !*szMethod || !szIcon ||
			g_ascii_strcasecmp(szIcon, "NoIcon") == 0)
			continue;
		if (!g_hash_table_contains(m_pIconMap, szMethod))
			g_hash_table_insert(m_pIconMap, g_strdup(szMethod),
								abi_stock_from_toolbar_id(szIcon));
	}
}

GtkWidget * AP_UnixRibbon::createWidget()
{
	m_wNotebook = gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(m_wNotebook), TRUE);
	gtk_widget_add_css_class(m_wNotebook, "abiword-ribbon");
	gtk_widget_set_vexpand(m_wNotebook, FALSE);
	gtk_widget_set_hexpand(m_wNotebook, TRUE);
	gtk_widget_set_valign(m_wNotebook, GTK_ALIGN_START);

	/* keep the band compact: small flat-ish buttons, tight frames */
	GtkCssProvider * css = gtk_css_provider_new();
	gtk_css_provider_load_from_string(css,
		".abiword-ribbon button { min-height: 0; padding: 3px 10px; }"
		".abiword-ribbon flowboxchild { padding: 0; }"
		".abiword-ribbon flowbox { padding: 2px; }"
		".abiword-ribbon frame { margin: 2px 4px; }"
		".abiword-ribbon frame > label { font-size: 0.85em; padding: 0 4px; }"
		".abiword-ribbon combobox, .abiword-ribbon dropdown { margin: 1px 2px; }"
		".abiword-ribbon notebook > header { margin-bottom: 0; }");
	gtk_style_context_add_provider_for_display(
		gtk_widget_get_display(m_wNotebook),
		GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(css);

	/* bind the same per-window action group the menubar uses, so
	 * ribbon buttons resolve "menu.<name>" identically */
	gtk_widget_insert_action_group(m_wNotebook, "menu",
								   m_pMenu->getActionGroup());

	_buildIconMap();

	for (const AP_RibbonTab * tab = s_ribbon_tabs; tab->szTabKey; ++tab)
	{
		GtkWidget * page = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_widget_set_valign(page, GTK_ALIGN_START);

		for (const AP_RibbonGroup * group = tab->groups; group->szGroupKey; ++group)
		{
			GtkWidget * frame = gtk_frame_new(_ribbon_label(group->szGroupKey,
															s_ribbon_group_labels));
			/* LibreOffice-style columns of 3 rows: group height stays
			 * constant, extra items wrap into more columns */
			GtkWidget * grid = gtk_grid_new();
			gtk_grid_set_column_spacing(GTK_GRID(grid), 2);
			gtk_widget_set_valign(grid, GTK_ALIGN_START);

			bool bEmpty = true;
			int nCol = 0, nRow = 0;
			for (const AP_RibbonItem * item = group->items;
				 !(item->kind == AP_RIBBON_ITEM_MENU &&
				   item->id == (uint16_t)AP_MENU_ID__BOGUS1__); ++item)
			{
				GtkWidget * w = (item->kind == AP_RIBBON_ITEM_TOOLBAR)
					? _makeToolbarWidget((XAP_Toolbar_Id)item->id)
					: _makeButton((XAP_Menu_Id)item->id);
				if (!w)
					continue;
				bEmpty = false;

				/* combos get a full-height column to themselves; plain
				 * buttons pack 3 rows per column, LibreOffice-style */
				bool bTall = (item->kind == AP_RIBBON_ITEM_TOOLBAR) &&
					(item->id == AP_TOOLBAR_ID_FMT_FONT ||
					 item->id == AP_TOOLBAR_ID_FMT_SIZE ||
					 item->id == AP_TOOLBAR_ID_FMT_STYLE ||
					 item->id == AP_TOOLBAR_ID_ZOOM);
				if (bTall)
				{
					if (nRow != 0) { ++nCol; nRow = 0; }
					gtk_grid_attach(GTK_GRID(grid), w, nCol, 0, 1, 3);
					++nCol;
				}
				else
				{
					gtk_grid_attach(GTK_GRID(grid), w, nCol, nRow, 1, 1);
					if (++nRow == 3) { nRow = 0; ++nCol; }
				}
			}

			if (bEmpty)
			{
				gtk_widget_set_visible(frame, FALSE);
			}
			else
			{
				gtk_frame_set_child(GTK_FRAME(frame), grid);
			}
			gtk_box_append(GTK_BOX(page), frame);
		}

		GtkWidget * tabLabel = gtk_label_new(_ribbon_label(tab->szTabKey,
														 s_ribbon_tab_labels));
		gtk_notebook_append_page(GTK_NOTEBOOK(m_wNotebook), page, tabLabel);

		if (tab->bContextual)
		{
			m_vecContextualPages.addItem(page);
			gtk_widget_set_visible(page, FALSE);
		}
	}

	g_signal_connect(m_wNotebook, "switch-page",
					 G_CALLBACK(_s_switch_page), this);

	GtkEventController * motion = gtk_event_controller_motion_new();
	g_signal_connect(motion, "enter", G_CALLBACK(_s_motion_enter), this);
	gtk_widget_add_controller(m_wNotebook, motion);

	return m_wNotebook;
}

GtkWidget * AP_UnixRibbon::_makeButton(XAP_Menu_Id id)
{
	const EV_Menu_ActionSet * pActionSet =
		XAP_App::getApp()->getMenuActionSet();
	UT_return_val_if_fail(pActionSet, nullptr);

	const EV_Menu_Action * pAction = pActionSet->getAction(id);
	const EV_Menu_Label * pLabel = m_pMenu->getLabelSet()->getLabel(id);
	if (!pAction || !pLabel)
		return nullptr;

	const char * szLabel = pAction->hasDynamicLabel()
		? pAction->getDynamicLabel(pLabel)
		: pLabel->getMenuLabel();
	if (!szLabel || !*szLabel)
		return nullptr;

	GAction * action = m_pMenu->lookupAction(id);
	if (!action)
		return nullptr;

	char detailed[64];
	g_snprintf(detailed, sizeof(detailed), "menu.%s", g_action_get_name(action));

	char label[256];
	_ribbon_strip_mnemonic(szLabel, label, sizeof(label));

	GtkWidget * btn;
	if (pAction->isCheckable() || pAction->isRadio())
		btn = gtk_toggle_button_new();
	else
		btn = gtk_button_new();

	GtkWidget * wLabel = gtk_label_new(label);
	gtk_label_set_ellipsize(GTK_LABEL(wLabel), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars(GTK_LABEL(wLabel), 18);

	/* reuse the classic toolbar's icon for this edit method, if any */
	const char * szMethod = pAction->getMethodName();
	const char * szIcon = (szMethod && m_pIconMap)
		? static_cast<const char *>(g_hash_table_lookup(m_pIconMap,
														szMethod))
		: nullptr;
	if (szIcon && *szIcon)
	{
		GtkWidget * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
		GtkWidget * image = gtk_image_new_from_icon_name(szIcon);
		gtk_widget_set_valign(image, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(box), image);
		gtk_box_append(GTK_BOX(box), wLabel);
		gtk_button_set_child(GTK_BUTTON(btn), box);
	}
	else
	{
		gtk_button_set_child(GTK_BUTTON(btn), wLabel);
	}

	gtk_actionable_set_action_name(GTK_ACTIONABLE(btn), detailed);
	if (pAction->isRadio())
	{
		char target[32];
		g_snprintf(target, sizeof(target), "%u", static_cast<unsigned>(id));
		gtk_actionable_set_action_target_value(GTK_ACTIONABLE(btn),
											   g_variant_new_string(target));
	}

	const char * szStatus = pLabel->getMenuStatusMessage();
	if (szStatus && *szStatus && strcmp(szStatus, " ") != 0)
		gtk_widget_set_tooltip_text(btn, szStatus);

	return btn;
}

/*****************************************************************/
/* Toolbar-backed items: combos, color pickers and plain buttons
 * that dispatch through the toolbar action set's edit methods,
 * exactly like the classic toolbar does.
 */

void AP_UnixRibbon::_invokeToolbarItem(XAP_Toolbar_Id id,
									   const UT_UCS4Char * pData,
									   UT_uint32 dataLength)
{
	const EV_Toolbar_ActionSet * pTBActions =
		XAP_App::getApp()->getToolbarActionSet();
	UT_return_if_fail(pTBActions);

	EV_Toolbar_Action * pAction = pTBActions->getAction(id);
	UT_return_if_fail(pAction);

	AV_View * pView = m_pFrame ? m_pFrame->getCurrentView() : nullptr;

	/* ignore presses on "down" group buttons (same rule as
	 * EV_UnixToolbar::toolbarEvent) */
	if (pAction->getItemType() == EV_TBIT_GroupButton && pView)
	{
		const char * szState = nullptr;
		EV_Toolbar_ItemState tis = pAction->getToolbarItemState(pView, &szState);
		if (EV_TIS_ShouldBeToggled(tis))
			return;
	}

	const char * szMethodName = pAction->getMethodName();
	UT_return_if_fail(szMethodName);

	const EV_EditMethodContainer * pEMC =
		XAP_App::getApp()->getEditMethodContainer();
	UT_return_if_fail(pEMC);

	EV_EditMethod * pEM = pEMC->findEditMethodByName(szMethodName);
	UT_return_if_fail(pEM);

	EV_EditMethodType t = pEM->getType();
	if (((t & EV_EMT_REQUIREDATA) != 0) && (!pData || !dataLength))
		return;

	EV_EditMethodCallData emcd(pData, dataLength);
	pEM->Fn(pView, &emcd);
}

void AP_UnixRibbon::_s_tb_clicked(GtkWidget * w, gpointer data)
{
	_TbCtx * ctx =
		static_cast<_TbCtx *>(data);
	UT_return_if_fail(ctx && ctx->self);
	if (ctx->blockSignal)
		return;

	/* a "down" group button cannot be un-pressed by clicking it;
	 * throw the widget's state back and ignore the event (same rule
	 * as EV_UnixToolbar::toolbarEvent) */
	const EV_Toolbar_ActionSet * pTBActions =
		XAP_App::getApp()->getToolbarActionSet();
	EV_Toolbar_Action * pAction =
		pTBActions ? pTBActions->getAction(ctx->id) : nullptr;
	if (pAction && pAction->getItemType() == EV_TBIT_GroupButton &&
		GTK_IS_TOGGLE_BUTTON(w))
	{
		AV_View * pView = ctx->self->m_pFrame
			? ctx->self->m_pFrame->getCurrentView() : nullptr;
		const char * szState = nullptr;
		EV_Toolbar_ItemState tis = pView
			? pAction->getToolbarItemState(pView, &szState)
			: EV_TIS_ZERO;
		if (EV_TIS_ShouldBeToggled(tis))
		{
			bool wasBlocked = ctx->blockSignal;
			ctx->blockSignal = true;
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),
				!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));
			ctx->blockSignal = wasBlocked;
			return;
		}
	}
	ctx->self->_invokeToolbarItem(ctx->id);
}


/* get the combo's active/typed text, like s_combo_apply_changes */
gchar * AP_UnixRibbon::_tb_combo_get_text(GtkComboBox * combo)
{
	if (ABI_IS_FONT_COMBO(combo))
		return abi_font_combo_get_active_text(ABI_FONT_COMBO(combo));

	if (GTK_IS_COMBO_BOX_TEXT(combo))
	{
		gchar * buffer =
			gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
		/* combos with an entry (font size) may hold a value that is
		 * not in the list; the typed value must not be lost */
		if (!buffer)
		{
			GtkWidget * child = gtk_combo_box_get_child(combo);
			if (child && GTK_IS_EDITABLE(child))
			{
				const char * entryText =
					gtk_editable_get_text(GTK_EDITABLE(child));
				if (entryText && *entryText)
					buffer = g_strdup(entryText);
			}
		}
		return buffer;
	}
	return nullptr;
}

void AP_UnixRibbon::_tb_combo_apply(GtkComboBox * combo, _TbCtx * ctx)
{
	UT_return_if_fail(ctx && ctx->self);

	gchar * buffer = _tb_combo_get_text(combo);
	if (!buffer)
		return;

	const char * text = buffer;
	if (ctx->id == (XAP_Toolbar_Id)AP_TOOLBAR_ID_FMT_FONT)
	{
		/* translate the localized size entry back, if it is one */
		const gchar * font =
			XAP_EncodingManager::fontsizes_mapping.lookupByTarget(buffer);
		if (font)
		{
			g_free(buffer);
			buffer = g_strdup(font);
			text = buffer;
		}
	}
	else if (ctx->id == (XAP_Toolbar_Id)AP_TOOLBAR_ID_FMT_STYLE)
	{
		text = pt_PieceTable::s_getUnlocalisedStyleName(buffer);
	}

	if (text)
	{
		UT_UCS4String ucsText(text);
		ctx->self->_invokeToolbarItem(ctx->id, ucsText.ucs4_str(),
									  ucsText.length());
	}
	g_free(buffer);
}

void AP_UnixRibbon::_s_tb_combo_changed(GtkComboBox * combo, gpointer data)
{
	_TbCtx * ctx =
		static_cast<_TbCtx *>(data);
	UT_return_if_fail(ctx);
	if (ctx->blockSignal || !ctx->widget)
		return;

	if (ctx->id == (XAP_Toolbar_Id)AP_TOOLBAR_ID_FMT_SIZE)
	{
		/* no updates of the font size while the entry is being edited */
		GtkWidget * entry = gtk_combo_box_get_child(combo);
		if (entry && gtk_widget_has_focus(entry))
			return;
	}
	_tb_combo_apply(combo, ctx);
}

gboolean AP_UnixRibbon::_s_tb_size_key(GtkEventControllerKey * controller,
							   guint keyval, guint /*keycode*/,
							   GdkModifierType /*state*/, gpointer data)
{
	if (keyval == GDK_KEY_Return)
	{
		GtkWidget * widget =
			gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
		GtkComboBox * combo = GTK_COMBO_BOX(
			gtk_widget_get_ancestor(widget, GTK_TYPE_COMBO_BOX));
		_tb_combo_apply(combo,
						static_cast<_TbCtx *>(data));
	}
	return FALSE;
}

void AP_UnixRibbon::_s_tb_size_focus_out(GtkEventControllerFocus * controller,
								 gpointer data)
{
	GtkWidget * widget =
		gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
	GtkComboBox * combo = GTK_COMBO_BOX(
		gtk_widget_get_ancestor(widget, GTK_TYPE_COMBO_BOX));
	_tb_combo_apply(combo, static_cast<_TbCtx *>(data));
}

/* only accept numeric input in the font size combo */
void AP_UnixRibbon::_s_tb_size_insert_text(GtkEditable * editable,
								   gchar * new_text, gint new_text_length,
								   gint * /*position*/, gpointer /*data*/)
{
	gchar * iter = new_text;
	while (iter < (new_text + new_text_length))
	{
		if (!g_unichar_isdigit(g_utf8_get_char(iter)))
		{
			g_signal_stop_emission_by_name(G_OBJECT(editable), "insert-text");
			return;
		}
		iter = g_utf8_next_char(iter);
	}
}

static void _tb_popdown_popover(GtkWidget * widget)
{
	GtkWidget * popover =
		gtk_widget_get_ancestor(widget, GTK_TYPE_POPOVER);
	if (popover)
		gtk_popover_popdown(GTK_POPOVER(popover));
}

void AP_UnixRibbon::_s_tb_color_activated(GtkColorChooser * cc, GdkRGBA * color,
								  gpointer data)
{
	_TbCtx * ctx =
		static_cast<_TbCtx *>(data);
	UT_return_if_fail(ctx && ctx->self && color);

	UT_UTF8String str = UT_UTF8String_sprintf("%02x%02x%02x",
		static_cast<int>(color->red   * 255),
		static_cast<int>(color->green * 255),
		static_cast<int>(color->blue  * 255));
	_tb_popdown_popover(GTK_WIDGET(cc));
	ctx->self->_invokeToolbarItem(ctx->id,
								  str.ucs4_str().ucs4_str(), str.size());
}

void AP_UnixRibbon::_s_tb_color_automatic(GtkWidget * widget, gpointer data)
{
	_TbCtx * ctx =
		static_cast<_TbCtx *>(data);
	UT_return_if_fail(ctx && ctx->self);
	_tb_popdown_popover(widget);

	if (ctx->id == (XAP_Toolbar_Id)AP_TOOLBAR_ID_COLOR_BACK)
	{
		const UT_UCS4Char transparent[] =
			{'t','r','a','n','s','p','a','r','e','n','t',0};
		ctx->self->_invokeToolbarItem(ctx->id, transparent, 10);
	}
	else
	{
		const UT_UCS4Char black[] = {'0','0','0','0','0','0',0};
		ctx->self->_invokeToolbarItem(ctx->id, black, 6);
	}
}

/* menu button + popover color chooser, mirroring abi_color_button_new */
GtkWidget * AP_UnixRibbon::_tb_color_button_new(const gchar * icon_name,
										const gchar * automatic_label,
										_TbCtx * ctx)
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
						 G_CALLBACK(_s_tb_color_automatic), ctx);
		gtk_box_append(GTK_BOX(box), automatic);
	}

	GtkWidget * chooser = gtk_color_chooser_widget_new();
	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(chooser), FALSE);
	g_signal_connect(G_OBJECT(chooser), "color-activated",
					 G_CALLBACK(_s_tb_color_activated), ctx);
	gtk_box_append(GTK_BOX(box), chooser);

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
	return button;
}

GtkWidget * AP_UnixRibbon::_tb_make_combo(_TbCtx * ctx)
{
	GtkWidget * combo = nullptr;
	switch (ctx->id)
	{
	case AP_TOOLBAR_ID_FMT_FONT:
	{
		combo = abi_font_combo_new();
		gtk_widget_set_name(combo, "AbiFontCombo");
		const std::vector<std::string> & fonts =
			GR_CairoGraphics::getAllFontNames();
		const gchar ** list = g_new0(const gchar *, fonts.size() + 1);
		for (size_t i = 0; i < fonts.size(); ++i)
			list[i] = fonts[i].c_str();
		abi_font_combo_set_fonts(ABI_FONT_COMBO(combo), list);
		g_free(list);
		break;
	}
	case AP_TOOLBAR_ID_FMT_SIZE:
	{
		combo = gtk_combo_box_text_new_with_entry();
		GtkEntry * entry =
			GTK_ENTRY(gtk_combo_box_get_child(GTK_COMBO_BOX(combo)));
		gtk_widget_set_can_focus(GTK_WIDGET(entry), TRUE);
		gtk_editable_set_width_chars(GTK_EDITABLE(entry), 4);
		gtk_editable_set_max_width_chars(GTK_EDITABLE(entry), 6);
		g_signal_connect(G_OBJECT(entry), "insert-text",
						 G_CALLBACK(_s_tb_size_insert_text), nullptr);
		GtkEventController * focus = gtk_event_controller_focus_new();
		g_signal_connect(G_OBJECT(focus), "leave",
						 G_CALLBACK(_s_tb_size_focus_out), ctx);
		gtk_widget_add_controller(GTK_WIDGET(entry), focus);
		GtkEventController * key = gtk_event_controller_key_new();
		g_signal_connect(G_OBJECT(key), "key-pressed",
						 G_CALLBACK(_s_tb_size_key), ctx);
		gtk_widget_add_controller(GTK_WIDGET(entry), key);

		int n = XAP_EncodingManager::fontsizes_mapping.size();
		for (int i = 0; i < n; ++i)
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
				XAP_EncodingManager::fontsizes_mapping.nth2(i));
		break;
	}
	case AP_TOOLBAR_ID_FMT_STYLE:
	{
		combo = gtk_combo_box_text_new();
		gtk_widget_set_name(combo, "AbiStyleCombo");
		/* same hardwired list the classic toolbar seeds with; the
		 * refresh path appends the caret's actual style if missing */
		static const char * styles[] =
		{
			"Normal", "Heading 1", "Heading 2", "Heading 3",
			"Plain Text", "Block Text"
		};
		for (size_t i = 0; i < G_N_ELEMENTS(styles); ++i)
		{
			std::string sLoc;
			pt_PieceTable::s_getLocalisedStyleName(styles[i], sLoc);
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
										   sLoc.c_str());
		}
		break;
	}
	case AP_TOOLBAR_ID_ZOOM:
	{
		combo = gtk_combo_box_text_new();
		gtk_widget_set_name(combo, "AbiZoomCombo");
		static const char * zooms[] =
			{ "200%", "150%", "100%", "75%", "50%", "25%" };
		for (size_t i = 0; i < G_N_ELEMENTS(zooms); ++i)
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
										   zooms[i]);
		const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
		if (pSS)
		{
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
				pSS->getValue(XAP_STRING_ID_TB_Zoom_PageWidth));
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
				pSS->getValue(XAP_STRING_ID_TB_Zoom_WholePage));
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
				pSS->getValue(XAP_STRING_ID_TB_Zoom_Percent));
		}
		break;
	}
	default:
		break;
	}
	return combo;
}

GtkWidget * AP_UnixRibbon::_makeToolbarWidget(XAP_Toolbar_Id id)
{
	const EV_Toolbar_ActionSet * pTBActions =
		XAP_App::getApp()->getToolbarActionSet();
	UT_return_val_if_fail(pTBActions, nullptr);

	EV_Toolbar_Action * pAction = pTBActions->getAction(id);
	EV_Toolbar_Label * pLabel =
		m_pTBLabels ? m_pTBLabels->getLabel(id) : nullptr;
	if (!pAction || !pLabel)
		return nullptr;

	_TbCtx * ctx = new _TbCtx;
	ctx->self = this;
	ctx->id = id;
	ctx->widget = nullptr;
	ctx->blockSignal = false;
	ctx->handlerId = 0;

	GtkWidget * w = nullptr;
	const char * szIcon = pLabel->getIconName();
	gchar * szThemeIcon =
		(szIcon && g_ascii_strcasecmp(szIcon, "NoIcon") != 0)
		? abi_stock_from_toolbar_id(szIcon) : nullptr;

	switch (pAction->getItemType())
	{
	case EV_TBIT_PushButton:
		w = gtk_button_new();
		if (szThemeIcon)
			gtk_button_set_icon_name(GTK_BUTTON(w), szThemeIcon);
		ctx->handlerId = g_signal_connect(w, "clicked",
										  G_CALLBACK(_s_tb_clicked), ctx);
		break;

	case EV_TBIT_ToggleButton:
	case EV_TBIT_GroupButton:
		w = gtk_toggle_button_new();
		if (szThemeIcon)
		{
			GtkWidget * image = gtk_image_new_from_icon_name(szThemeIcon);
			gtk_button_set_child(GTK_BUTTON(w), image);
		}
		ctx->handlerId = g_signal_connect(w, "clicked",
										  G_CALLBACK(_s_tb_clicked), ctx);
		break;

	case EV_TBIT_ComboBox:
		w = _tb_make_combo(ctx);
		if (w)
			ctx->handlerId = g_signal_connect(w, "changed",
											  G_CALLBACK(_s_tb_combo_changed),
											  ctx);
		break;

	case EV_TBIT_ColorFore:
	case EV_TBIT_ColorBack:
	{
		const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
		std::string sClear;
		if (pSS)
			pSS->getValueUTF8(
				pAction->getItemType() == EV_TBIT_ColorFore
					? XAP_STRING_ID_TB_ClearForeground
					: XAP_STRING_ID_TB_ClearBackground, sClear);
		w = _tb_color_button_new(
			szThemeIcon ? szThemeIcon : "preferences-color-symbolic",
			sClear.c_str(), ctx);
		break;
	}

	default:
		break;
	}

	if (!w)
	{
		delete ctx;
		return nullptr;
	}

	const char * szToolTip = pLabel->getToolTip();
	if (szToolTip && *szToolTip)
		gtk_widget_set_tooltip_text(w, szToolTip);
	g_free(szThemeIcon);

	gtk_widget_set_hexpand(w, FALSE);
	gtk_widget_set_valign(w, GTK_ALIGN_CENTER);
	gtk_widget_set_visible(w, TRUE);

	ctx->widget = w;
	m_vecTbCtx.addItem(ctx);
	return w;
}

/*
 * Set the combo's displayed text without firing "changed"
 * (combo_box_set_active_text equivalent for ribbon combos).
 */
void AP_UnixRibbon::_tb_combo_set_text(GtkComboBox * combo, const char * text,
							   _TbCtx * ctx)
{
	if (ABI_IS_FONT_COMBO(combo))
	{
		if (!abi_font_combo_select_text(ABI_FONT_COMBO(combo), text))
			abi_font_combo_insert_font(ABI_FONT_COMBO(combo), text, TRUE);
		return;
	}

	GtkTreeModel * model = gtk_combo_box_get_model(combo);
	if (!model)
		return;
	GtkTreeIter iter;
	gboolean next = gtk_tree_model_get_iter_first(model, &iter);
	while (next)
	{
		gchar * value = nullptr;
		gtk_tree_model_get(model, &iter, 0, &value, -1);
		bool bMatch = value && strcmp(text, value) == 0;
		g_free(value);
		if (bMatch)
		{
			gtk_combo_box_set_active_iter(combo, &iter);
			return;
		}
		next = gtk_tree_model_iter_next(model, &iter);
	}

	/* not in the list - append it so the combo shows the real value
	 * (custom zoom levels, document styles not in the seed list) */
	if (GTK_IS_COMBO_BOX_TEXT(combo))
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), text);
		_tb_combo_set_text(combo, text, ctx);	/* re-run to select it */
		return;
	}

	GtkWidget * child = gtk_combo_box_get_child(combo);
	if (child && GTK_IS_EDITABLE(child))
		gtk_editable_set_text(GTK_EDITABLE(child), text);
}

void AP_UnixRibbon::_refreshToolbarItems()
{
	const EV_Toolbar_ActionSet * pTBActions =
		XAP_App::getApp()->getToolbarActionSet();
	if (!pTBActions)
		return;

	AV_View * pView = m_pFrame ? m_pFrame->getCurrentView() : nullptr;

	UT_sint32 count = m_vecTbCtx.getItemCount();
	for (UT_sint32 i = 0; i < count; ++i)
	{
		_TbCtx * ctx = m_vecTbCtx.getNthItem(i);
		EV_Toolbar_Action * pAction = pTBActions->getAction(ctx->id);
		if (!pAction || !ctx->widget)
			continue;

		const char * szState = nullptr;
		EV_Toolbar_ItemState tis = pView
			? pAction->getToolbarItemState(pView, &szState)
			: EV_TIS_Gray;
		if (tis & EV_TIS_Hidden)
			tis = (EV_Toolbar_ItemState)(tis | EV_TIS_Gray);

		bool bGrayed = EV_TIS_ShouldBeGray(tis);
		gtk_widget_set_sensitive(ctx->widget, !bGrayed);
		gtk_widget_set_visible(ctx->widget, !EV_TIS_ShouldBeHidden(tis));

		bool wasBlocked = ctx->blockSignal;
		ctx->blockSignal = true;

		switch (pAction->getItemType())
		{
		case EV_TBIT_ToggleButton:
		case EV_TBIT_GroupButton:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctx->widget),
										 EV_TIS_ShouldBeToggled(tis));
			break;

		case EV_TBIT_ComboBox:
			if (!szState)
			{
				if (ABI_IS_FONT_COMBO(ctx->widget))
					abi_font_combo_unselect(ABI_FONT_COMBO(ctx->widget));
				else
					gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->widget), -1);
			}
			else if (ctx->id == (XAP_Toolbar_Id)AP_TOOLBAR_ID_FMT_SIZE)
			{
				const char * fsz =
					XAP_EncodingManager::fontsizes_mapping.lookupBySource(szState);
				_tb_combo_set_text(GTK_COMBO_BOX(ctx->widget),
								   fsz ? fsz : szState, ctx);
			}
			else if (ctx->id == (XAP_Toolbar_Id)AP_TOOLBAR_ID_FMT_STYLE)
			{
				std::string sLoc;
				pt_PieceTable::s_getLocalisedStyleName(szState, sLoc);
				_tb_combo_set_text(GTK_COMBO_BOX(ctx->widget),
								   sLoc.c_str(), ctx);
			}
			else if (ABI_IS_FONT_COMBO(ctx->widget))
			{
				if (!abi_font_combo_select_text(ABI_FONT_COMBO(ctx->widget),
												szState))
					abi_font_combo_insert_font(ABI_FONT_COMBO(ctx->widget),
											   szState, TRUE);
			}
			else
			{
				_tb_combo_set_text(GTK_COMBO_BOX(ctx->widget), szState, ctx);
			}
			break;

		default:
			break;
		}

		ctx->blockSignal = wasBlocked;
	}
}

void AP_UnixRibbon::refresh()
{
	AV_View * view = m_pFrame ? m_pFrame->getCurrentView() : nullptr;
	if (view)
		m_pMenu->refreshMenu(view);
	_refreshContextualTabs();
	_refreshToolbarItems();
}

void AP_UnixRibbon::_refreshContextualTabs()
{
	FV_View * view = static_cast<FV_View *>(
		m_pFrame ? m_pFrame->getCurrentView() : nullptr);
	bool bInTable = view && view->isInTable();

	UT_sint32 count = m_vecContextualPages.getItemCount();
	for (UT_sint32 i = 0; i < count; ++i)
		gtk_widget_set_visible(m_vecContextualPages.getNthItem(i), bInTable);
}

void AP_UnixRibbon::_s_switch_page(GtkNotebook * /*book*/,
								   GtkWidget * /*page*/,
								   guint /*page_num*/,
								   gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	if (self)
		self->refresh();
}

void AP_UnixRibbon::_s_motion_enter(GtkEventControllerMotion * /*ctrl*/,
									gdouble /*x*/, gdouble /*y*/,
									gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	if (self)
		self->refresh();
}
