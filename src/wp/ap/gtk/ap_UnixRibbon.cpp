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
#include "xap_App.h"
#include "xap_Frame.h"
#include "ev_UnixMenuBar.h"
#include "ev_Menu_Labels.h"
#include "ev_Menu_Actions.h"
#include "ev_Toolbar_Actions.h"
#include "ev_Toolbar_Labels.h"
#include "xap_Toolbar_LabelSet.h"
#include "ap_Toolbar_Id.h"
#include "ap_Prefs_SchemeIds.h"
#include "ap_UnixStockIcons.h"
#include "ap_Ribbon_Layouts.h"
#include "fv_View.h"

/*
 * Tab and group titles.  The layout table stores untranslated keys;
 * these English titles are the ribbon's labels.  (Localizing them is
 * future work - they are not in the .strings machinery yet.)
 */
struct _ribbon_kv { const char * szKey; const char * szLabel; };

static const _ribbon_kv s_ribbon_tab_labels[] =
{
	{ "file",   "File" },
	{ "home",   "Home" },
	{ "insert", "Insert" },
	{ "layout", "Layout" },
	{ "review", "Review" },
	{ "view",   "View" },
	{ "table",  "Table" },
	{ "help",   "Help" },
	{ nullptr,   nullptr }
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
	{ "pages",       "Pages" },
	{ "objects",     "Objects" },
	{ "fields",      "Fields" },
	{ "links",       "Links" },
	{ "views",       "Views" },
	{ "page",        "Page Setup" },
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

AP_UnixRibbon::AP_UnixRibbon(XAP_Frame * pFrame, EV_UnixMenuBar * pMenu)
	: m_pFrame(pFrame)
	, m_pMenu(pMenu)
	, m_wNotebook(nullptr)
	, m_pIconMap(nullptr)
{
}

AP_UnixRibbon::~AP_UnixRibbon()
{
	// m_wNotebook is owned by the widget tree; nothing to unref here.
	g_clear_pointer(&m_pIconMap, g_hash_table_unref);
}

/*
 * Toolbar icons are keyed by toolbar id, while the ribbon is built
 * from menu ids.  Both action sets name the same edit methods, so we
 * bridge them: for every toolbar id, map its edit-method name to the
 * icon the classic toolbar would show.  Ribbon buttons then look up
 * their icon by the menu action's method name.
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
	EV_Toolbar_LabelSet * pTBLabels = AP_CreateToolbarLabelSet(lang.c_str());
	if (!pTBLabels)
		return;

	for (UT_uint32 tid = 1; tid < (UT_uint32)AP_TOOLBAR_ID__BOGUS2__; ++tid)
	{
		EV_Toolbar_Action * pTBAction =
			pTBActions->getAction((XAP_Toolbar_Id)tid);
		EV_Toolbar_Label * pTBLabel =
			pTBLabels->getLabel((XAP_Toolbar_Id)tid);
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
	delete pTBLabels;
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
			int nItem = 0;
			for (const uint16_t * item = group->items;
				 *item != (uint16_t)AP_MENU_ID__BOGUS1__; ++item)
			{
				GtkWidget * btn = _makeButton((XAP_Menu_Id)*item);
				if (btn)
				{
					gtk_grid_attach(GTK_GRID(grid), btn,
									nItem / 3, nItem % 3, 1, 1);
					++nItem;
					bEmpty = false;
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

void AP_UnixRibbon::refresh()
{
	AV_View * view = m_pFrame ? m_pFrame->getCurrentView() : nullptr;
	if (view)
		m_pMenu->refreshMenu(view);
	_refreshContextualTabs();
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
