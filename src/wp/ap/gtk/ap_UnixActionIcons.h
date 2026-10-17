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

#pragma once

#include <gtk/gtk.h>
#include "xap_Types.h"

/*!
 * Single source for action icon resolution.
 *
 * Ribbon buttons, the classic toolbar, menus and icon maps used to
 * each walk their own path (toolbar icon name -> stock id ->
 * pick_icon, or menu id -> stock_mapping -> pick_icon, or an
 * edit-method -> icon map).  This module centralises that: every
 * surface asks for the icon of an action and gets the same resolved
 * theme name, drawn glyph or GIcon back.
 *
 * Resolution order for a menu id:
 *   1. the classic toolbar icon bound to the same edit method
 *      (richest artwork), then
 *   2. the menu-id column of the stock mapping, then
 *   3. nullptr - callers fall back to a text label.
 * The result is passed through abi_stock_pick_icon() so a theme
 * that only ships the -symbolic variant still resolves.
 *
 * All returned icon-name strings are owned by internal caches; do
 * not free them.
 */

/* resolved theme icon name for a menu id, or nullptr */
const gchar * abi_action_icon_for_menu (XAP_Menu_Id id);

/* resolved theme icon name for an edit-method name, or nullptr */
const gchar * abi_action_icon_for_method (const gchar * method);

/* resolved theme icon name for a classic toolbar icon id, or nullptr */
const gchar * abi_action_icon_for_toolbar (const gchar * toolbar_id);

/* true when a drawn glyph should be used instead of the theme:
 * a glyph exists for the name and either ABINOVA_GLYPH_ICONS is set
 * or the theme does not provide the icon */
gboolean	  abi_action_prefers_glyph (const gchar * icon_name);

/* icon widget: GtkDrawingArea glyph when preferred, else GtkImage
 * bound to the (resolved) theme name.  nullptr when icon_name is
 * null/empty */
GtkWidget *	  abi_action_icon_widget (const gchar * icon_name,
									  int pixel_size);

/* GIcon for GMenu items: PNG bytes of the drawn glyph when preferred,
 * else a GThemedIcon */
GIcon *		  abi_action_gicon (const gchar * icon_name,
								int pixel_size);
