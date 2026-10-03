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

/*****************************************************************
******************************************************************
** IT IS IMPORTANT THAT THIS FILE ALLOW ITSELF TO BE INCLUDED
** MORE THAN ONE TIME.
******************************************************************
*****************************************************************/

BeginLayout(ContextField,EV_EMC_FIELD)

	BeginPopupMenu()
		MenuItem(AP_MENU_ID_EDIT_UPDATEFIELD)
		MenuItem(AP_MENU_ID_EDIT_FIELD)
		Separator()
		MenuItem(AP_MENU_ID_EDIT_CUT)
		MenuItem(AP_MENU_ID_EDIT_COPY)
		MenuItem(AP_MENU_ID_EDIT_PASTE)
		MenuItem(AP_MENU_ID_EDIT_PASTE_SPECIAL)
	EndPopupMenu()

EndLayout()
