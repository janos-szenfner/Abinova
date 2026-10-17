/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Program Utilities
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ap_UnixActionIcons.h"
#include "ap_UnixGlyphIcons.h"
#include "ap_UnixStockIcons.h"
#include "ap_Toolbar_Id.h"
#include "ap_Prefs.h"
#include "xap_Toolbar_LabelSet.h"
#include "xap_App.h"
#include "ev_Menu_Actions.h"
#include "ev_Toolbar_Actions.h"
#include "ev_Toolbar_Labels.h"
#include "ut_assert.h"

/* ------------------------------------------------------------------ */

/* toolbar icon id (e.g. "FILE_NEW_de-AT") -> resolved theme name */
const gchar *
abi_action_icon_for_toolbar (const gchar *toolbar_id)
{
	if (!toolbar_id || !*toolbar_id ||
		g_ascii_strcasecmp (toolbar_id, "NoIcon") == 0)
		return nullptr;
	gchar * stock = abi_stock_from_toolbar_id (toolbar_id);
	const gchar * picked = abi_stock_pick_icon (stock);
	g_free (stock);
	return picked;
}

/* edit-method name -> resolved theme name.
 * The classic toolbar carries the richest icon set, so the method's
 * toolbar binding is the canonical icon source (cached on first
 * use from the toolbar label/action sets). */
const gchar *
abi_action_icon_for_method (const gchar *method)
{
	if (!method || !*method)
		return nullptr;

	static GHashTable *s_map = nullptr; /* method -> theme name */
	if (!s_map)
	{
		s_map = g_hash_table_new_full (g_str_hash, g_str_equal,
									   g_free, g_free);
		const EV_Toolbar_ActionSet * pTBActions =
			XAP_App::getApp()->getToolbarActionSet();
		EV_Toolbar_LabelSet * pTBLabels =
			AP_CreateToolbarLabelSet (AP_PREF_DEFAULT_StringSet);
		if (!pTBActions || !pTBLabels)
			return nullptr;

		for (UT_uint32 tid = 1;
			 tid < static_cast<UT_uint32>(AP_TOOLBAR_ID__BOGUS2__);
			 ++tid)
		{
			EV_Toolbar_Action * pTBAction = pTBActions->getAction (
				static_cast<XAP_Toolbar_Id>(tid));
			EV_Toolbar_Label * pTBLabel = pTBLabels->getLabel (
				static_cast<XAP_Toolbar_Id>(tid));
			if (!pTBAction || !pTBLabel)
				continue;
			const char * szMethod = pTBAction->getMethodName ();
			const char * szIcon = pTBLabel->getIconName ();
			if (!szMethod || !*szMethod || !szIcon ||
				g_ascii_strcasecmp (szIcon, "NoIcon") == 0)
				continue;
			if (g_hash_table_contains (s_map, szMethod))
				continue;
			const gchar * picked = abi_action_icon_for_toolbar (szIcon);
			if (picked && *picked)
				g_hash_table_insert (s_map, g_strdup (szMethod),
									 g_strdup (picked));
		}
		delete pTBLabels;
	}

	return static_cast<const gchar *>(
		g_hash_table_lookup (s_map, method));
}

/* menu id -> resolved theme name */
const gchar *
abi_action_icon_for_menu (XAP_Menu_Id id)
{
	/* same edit method as a toolbar button? reuse its icon */
	const EV_Menu_ActionSet * pActionSet =
		XAP_App::getApp()->getMenuActionSet();
	if (pActionSet)
	{
		const EV_Menu_Action * pAction = pActionSet->getAction (id);
		if (pAction)
		{
			const gchar * icon =
				abi_action_icon_for_method (pAction->getMethodName ());
			if (icon && *icon)
				return icon;
		}
	}
	return abi_stock_pick_icon (abi_stock_from_menu_id (id));
}

/* ------------------------------------------------------------------ */

/* icon names whose bundled bitmap predates the vector set — prefer
 * the drawn glyph even though the legacy theme icon exists */
static const gchar * const s_prefer_glyph[] = {
	"tb_view_showpara",
	"abinova-design-themes",
	"abinova-design-themecolors",
	"abinova-design-themefonts",
	"abinova-design-stylesets",
	"abinova-design-watermark",
	"abinova-design-pagecolor",
	"abinova-design-pageborders",
	nullptr
};

gboolean
abi_action_prefers_glyph (const gchar *icon_name)
{
	if (!icon_name || !*icon_name || !abi_glyph_icon_has (icon_name))
		return FALSE;
	if (abi_glyph_icons_forced ())
		return TRUE;
	for (gint i = 0; s_prefer_glyph[i]; i++)
		if (strcmp (icon_name, s_prefer_glyph[i]) == 0)
			return TRUE;
	GdkDisplay * display = gdk_display_get_default ();
	if (!display)
		return TRUE; /* headless-ish: glyph always renders */
	return !gtk_icon_theme_has_icon (
		gtk_icon_theme_get_for_display (display), icon_name);
}

GtkWidget *
abi_action_icon_widget (const gchar *icon_name, int pixel_size)
{
	if (!icon_name || !*icon_name)
		return nullptr;
	if (abi_action_prefers_glyph (icon_name))
		return abi_glyph_icon_widget (icon_name, pixel_size);
	return gtk_image_new_from_icon_name (icon_name);
}

GIcon *
abi_action_gicon (const gchar *icon_name, int pixel_size)
{
	if (!icon_name || !*icon_name)
		return nullptr;
	if (abi_action_prefers_glyph (icon_name))
		return abi_glyph_icon_gicon (icon_name, pixel_size);
	return g_themed_icon_new (icon_name);
}
