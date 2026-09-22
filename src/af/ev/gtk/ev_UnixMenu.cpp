/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:t -*- */
/* AbiSource Program Utilities
 * Copyright (C) 1998-2000 AbiSource, Inc.
 * Copyright (C) 2019-2025 Hubert Figuière
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

/*
 * GTK4 port notes:
 *
 * GtkMenu/GtkMenuItem/GtkMenuBar/GtkAccelGroup were removed in GTK4.
 * Menus are now described by a GMenuModel; each menu item activates a
 * GAction.  We keep one GSimpleAction per layout item, owned by a
 * per-menu GSimpleActionGroup which is inserted on the menu widgets
 * under the "menu" prefix.  Checkable items get a boolean-state
 * action, radio groups a shared string-state action.  Separators are
 * expressed as GMenu sections.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>

#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stack>
#include "ut_types.h"
#include "ut_string.h"
#include "ut_string_class.h"
#include "ut_std_vector.h"
#include "ut_debugmsg.h"
#include "xap_Types.h"
#include "ev_UnixMenu.h"
#include "ev_UnixMenuBar.h"
#include "ev_UnixMenuPopup.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"
#include "ev_UnixKeyboard.h"
#include "ev_Menu_Layouts.h"
#include "ev_Menu_Actions.h"
#include "ev_Menu_Labels.h"
#include "ev_EditEventMapper.h"
#include "xap_UnixDialogHelper.h"
#include "ap_Menu_Id.h"

/*****************************************************************/

EV_UnixMenu::_wd::_wd(EV_UnixMenu* pUnixMenu, XAP_Menu_Id id)
	: m_pUnixMenu(pUnixMenu)
	, m_id(id)
{
}

EV_UnixMenu::_wd::~_wd(void)
{
}

void EV_UnixMenu::_wd::s_onActivate(GSimpleAction * /*action*/,
									GVariant * /*param*/,
									gpointer callback_data)
{
	_wd * wd = static_cast<_wd *>(callback_data);
	UT_return_if_fail(wd && wd->m_pUnixMenu);

	wd->m_pUnixMenu->menuEvent(wd->m_id);
	wd->m_pUnixMenu->refreshMenu(wd->m_pUnixMenu->getFrame()->getCurrentView());
}

void EV_UnixMenu::_wd::s_onChangeState(GSimpleAction * action,
									   GVariant * value,
									   gpointer callback_data)
{
	_wd * wd = static_cast<_wd *>(callback_data);
	UT_return_if_fail(wd && wd->m_pUnixMenu);
	UT_return_if_fail(value);

	EV_UnixMenu * menu = wd->m_pUnixMenu;

	if (menu->m_bUpdatingActions)
	{
		// state change originates from refreshMenu, just adopt it
		g_simple_action_set_state(action, value);
		return;
	}

	// For radio groups the action is shared between several items and
	// the selected value is the target ("<id>").  For check items the
	// value is the new boolean state.
	XAP_Menu_Id id = wd->m_id;
	if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING))
	{
		id = static_cast<XAP_Menu_Id>(atoi(g_variant_get_string(value, nullptr)));
	}
	g_simple_action_set_state(action, value);

	menu->menuEvent(id);
	menu->refreshMenu(menu->getFrame()->getCurrentView());
}

/*****************************************************************/

/*
  Unlike the Win32 version, which uses a \t (tab) to seperate the
  feature from the mnemonic in a single label string, this
  function returns two strings, to be put into the two seperate
  labels in a Gtk menu item.

  Oh, and these are static buffers, don't call this function
  twice and expect previous return pointers to have the same
  values at their ends.
*/

static const char ** _ev_GetLabelName(XAP_UnixApp * pUnixApp,
									  XAP_Frame * /*pFrame*/,
									  const EV_Menu_Action * pAction,
									  const EV_Menu_Label * pLabel)
{
	static const char * data[2] = {nullptr, nullptr};

	// hit the static pointers back to null each time around
	data[0] = nullptr;
	data[1] = nullptr;

	const char * szLabelName;

	if (pAction->hasDynamicLabel())
		szLabelName = pAction->getDynamicLabel(pLabel);
	else
		szLabelName = pLabel->getMenuLabel();

	if (!szLabelName || !*szLabelName)
		return data;	// which will be two nulls now

	static UT_String accelbuf;
	{
		// see if this has an associated keybinding
		const char * szMethodName = pAction->getMethodName();
		if (szMethodName)
		{
			const EV_EditMethodContainer * pEMC = pUnixApp->getEditMethodContainer();
			UT_ASSERT(pEMC);

			EV_EditMethod * pEM = pEMC->findEditMethodByName(szMethodName);
			UT_ASSERT(pEM);						// make sure it's bound to something

			const EV_EditEventMapper * pEEM = pUnixApp->getEditEventMapper();
			UT_ASSERT(pEEM);

			const char * string = pEEM->getShortcutFor(pEM);
			if (string && *string)
				accelbuf = string;
			else
				// zero it out for this round
				accelbuf = "";
		}
	}

	// set shortcut mnemonic, if any
	if (!accelbuf.empty())
		data[1] = accelbuf.c_str();

	if (!pAction->raisesDialog())
	{
		data[0] = szLabelName;
		return data;
	}

	// append "..." to menu item if it raises a dialog
	static char buf[128];
	memset(buf,0,G_N_ELEMENTS(buf));
	strncpy(buf,szLabelName,G_N_ELEMENTS(buf)-4);
	strcat(buf,"...");

	data[0] = buf;

	return data;
}

/**
 * Convert an AbiWord accel string (for instance "Ctrl+Alt+F") to the
 * Gtk accelerator textual form ("<Control><Alt>f") used for the
 * GMenuItem "accel" attribute.  Display only - the actual key
 * handling happens in the EV keyboard layer.
 */
void EV_UnixMenu::_convertStringToGtkAccel(const char *str, std::string &out)
{
	out.clear();
	if (str == nullptr || *str == '\0')
		return;

	for (;;)
	{
		if (strncmp(str, "Ctrl+", 5) == 0)
		{
			out += "<Control>";
			str += 5;
		}
		else if (strncmp(str, "Alt+", 4) == 0)
		{
			out += "<Alt>";
			str += 4;
		}
		else if (strncmp(str, "Shift+", 6) == 0)
		{
			out += "<Shift>";
			str += 6;
		}
		else
			break;
	}

	if (strncmp(str, "Del", 3) == 0)
	{
		// Rob: we are not using <del> as accel key, otherwise
		// events are not passed down the widget hierarchy
		// see #1235.
		return;
	}
	else if (str[0] == 'F' &&
			 str[1] >= '0' &&
			 str[1] <= '9')
	{
		out += str;		// "F7" etc. is already a valid accel name
	}
	else if (*str)
	{
		char key[8] = {0,0,0,0,0,0,0,0};
		key[0] = static_cast<char>(tolower(static_cast<unsigned char>(*str)));
		out += key;
	}
}

static guint _ev_get_underlined_char(const char * szString)
{

	UT_ASSERT(szString);

	// return the keycode right after the underline
	const UT_UCS4String str(szString);
	for (UT_uint32 i = 0; i + 1 < str.length(); )
	{
		if (str[i++] == '_')
			return gdk_unicode_to_keyval(str[i]);
	}

	return GDK_KEY_VoidSymbol;
}

static void _ev_strip_underline(char * bufResult,
								const char * szString)
{
	UT_ASSERT(szString && bufResult);

	const char * pl = szString;
	char * b = bufResult;
	while (*pl)
	{
		if (*pl == '_')
			pl++;
		else
			*b++ = *pl++;
	}

	*b = 0;
}

static void _ev_convert(char * bufResult,
						const char * szString) ABI_NONNULL(1, 2);

// change the first '&' character, which we assume to be an accelerator character, to a '_' character as used 
// by GTK+. Furthermore, escape all '_' characters with another '_' character for a literal '_'
static void _ev_convert(char * bufResult,
						const char * szString)
{
	bool foundAmpersand = false;
	const char * src = szString;
	char * dest = bufResult;
	while (*src)
	{
		if (*src == '&' && !foundAmpersand)
		{
			*dest = '_';
			foundAmpersand = true;
		}
		else if (*src == '_')
		{
			*dest = '_';
			dest++;
			*dest = '_';
		}
		else
		{
			*dest = *src;
		}
		dest++;
		src++;
	}
	*dest = 0;
}

/*****************************************************************/

EV_UnixMenu::EV_UnixMenu(XAP_UnixApp * pUnixApp, 
						 XAP_Frame *pFrame, 
						 const char * szMenuLayoutName,
						 const char * szMenuLabelSetName)
	: EV_Menu(pUnixApp, pUnixApp->getEditMethodContainer(), szMenuLayoutName, szMenuLabelSetName),
	  m_pUnixApp(pUnixApp),
	  m_pFrame(pFrame),
	  m_pMenuModel(g_menu_new()),
	  m_isPopup(false),
	  m_actionGroup(g_simple_action_group_new()),
	  m_bUpdatingActions(false),
	  m_rebuildSourceId(0),
	  m_rebuildPending(false),
	  m_rebuildTicks(0)
{
}

EV_UnixMenu::~EV_UnixMenu()
{
	if (m_rebuildSourceId)
	{
		g_source_remove(m_rebuildSourceId);
		m_rebuildSourceId = 0;
	}
	m_vecItemRecs.clear();
	UT_std_vector_purgeall(m_vecCallbacks);
	g_object_unref(m_actionGroup);
	g_object_unref(m_pMenuModel);
}

XAP_Frame * EV_UnixMenu::getFrame() const
{
	return m_pFrame;
}

bool EV_UnixMenu::menuEvent(XAP_Menu_Id id) const
{
	// user selected something from the menu.
	// invoke the appropriate function.
	// return true if handled.

	const EV_Menu_ActionSet * pMenuActionSet = m_pUnixApp->getMenuActionSet();
	UT_return_val_if_fail(pMenuActionSet, false);

	const EV_Menu_Action * pAction = pMenuActionSet->getAction(id);
	UT_return_val_if_fail(pAction, false);

	const char * szMethodName = pAction->getMethodName();
	if (!szMethodName)
		return false;

	const EV_EditMethodContainer * pEMC = m_pUnixApp->getEditMethodContainer();
	UT_return_val_if_fail(pEMC, false);

	EV_EditMethod * pEM = pEMC->findEditMethodByName(szMethodName);
	UT_ASSERT(pEM);						// make sure it's bound to something

	UT_String script_name(pAction->getScriptName());
	invokeMenuMethod(m_pFrame->getCurrentView(), pEM, script_name);
	return true;
}

GAction * EV_UnixMenu::lookupAction(XAP_Menu_Id id) const
{
	for (const _ItemRec & rec : m_vecItemRecs)
	{
		if (rec.id == id && rec.present)
			return G_ACTION(rec.action);
	}
	return nullptr;
}

GAction * EV_UnixMenu::ensureAction(XAP_Menu_Id id)
{
	if (GAction * existing = lookupAction(id))
		return existing;

	const EV_Menu_ActionSet * pActionSet =
		XAP_App::getApp()->getMenuActionSet();
	UT_return_val_if_fail(pActionSet, nullptr);
	const EV_Menu_Action * pAction = pActionSet->getAction(id);
	UT_return_val_if_fail(pAction, nullptr);

	GSimpleAction * radioGroup = nullptr;
	GSimpleAction * action = _createAction(id, pAction, &radioGroup);
	UT_return_val_if_fail(action, nullptr);

	_ItemRec rec;
	rec.id = id;
	rec.action = action;
	rec.present = true;
	rec.isRadio = pAction->isRadio();
	m_vecItemRecs.push_back(rec);
	return G_ACTION(action);
}

/*!
 * Create (or return the existing) GSimpleAction for a layout item.
 *
 * For radio items a single shared string-state action is used for the
 * whole group; \a radioGroup carries it between consecutive items and
 * must be reset to nullptr whenever the radio run ends.
 */
GSimpleAction * EV_UnixMenu::_createAction(XAP_Menu_Id id,
										   const EV_Menu_Action * pAction,
										   GSimpleAction ** radioGroup)
{
	char name[64];

	if (pAction->isRadio())
	{
		if (*radioGroup)
			return *radioGroup;

		g_snprintf(name, sizeof(name), "radio_%u", static_cast<unsigned>(id));
		GAction * existing = g_action_map_lookup_action(G_ACTION_MAP(m_actionGroup), name);
		if (existing)
		{
			*radioGroup = G_SIMPLE_ACTION(existing);
			return *radioGroup;
		}
		GSimpleAction * action = g_simple_action_new_stateful(
			name, G_VARIANT_TYPE_STRING, g_variant_new_string(""));
		_wd * wd = new _wd(this, id);
		m_vecCallbacks.push_back(wd);
		g_signal_connect(G_OBJECT(action), "change-state",
						 G_CALLBACK(_wd::s_onChangeState), wd);
		g_action_map_add_action(G_ACTION_MAP(m_actionGroup), G_ACTION(action));
		g_object_unref(action);
		*radioGroup = action;
		return action;
	}

	g_snprintf(name, sizeof(name), "item_%u", static_cast<unsigned>(id));

	GAction * existing = g_action_map_lookup_action(G_ACTION_MAP(m_actionGroup), name);
	if (existing)
		return G_SIMPLE_ACTION(existing);

	GSimpleAction * action;
	if (pAction->isCheckable())
	{
		action = g_simple_action_new_stateful(
			name, nullptr, g_variant_new_boolean(FALSE));
	}
	else
	{
		action = g_simple_action_new(name, nullptr);
	}

	_wd * wd = new _wd(this, id);
	m_vecCallbacks.push_back(wd);
	if (pAction->isCheckable())
	{
		g_signal_connect(G_OBJECT(action), "change-state",
						 G_CALLBACK(_wd::s_onChangeState), wd);
	}
	else
	{
		g_signal_connect(G_OBJECT(action), "activate",
						 G_CALLBACK(_wd::s_onActivate), wd);
	}
	g_action_map_add_action(G_ACTION_MAP(m_actionGroup), G_ACTION(action));
	g_object_unref(action);
	return action;
}

/*!
 * Create a GMenuItem for a normal layout item, binding it to the
 * item's action.  \a radioGroup carries the shared radio action for
 * consecutive radio items.
 */
GMenuItem * EV_UnixMenu::_createMenuItem(XAP_Menu_Id id,
										 const EV_Menu_Action * pAction,
										 const char *szLabelName,
										 const char *szMnemonicName,
										 bool isPopup,
										 GSimpleAction ** radioGroup)
{
	char buf[1024];
	// convert label into underscored version
	_ev_convert(buf, szLabelName);

	GMenuItem * item = g_menu_item_new(buf, nullptr);

	GSimpleAction * action = _createAction(id, pAction, radioGroup);
	char actionName[64];
	g_snprintf(actionName, sizeof(actionName), "menu.%s",
			   g_action_get_name(G_ACTION(action)));

	if (pAction->isRadio())
	{
		char target[32];
		g_snprintf(target, sizeof(target), "%u", static_cast<unsigned>(id));
		g_menu_item_set_action_and_target_value(item, actionName,
												g_variant_new_string(target));
	}
	else
	{
		g_menu_item_set_action_and_target_value(item, actionName, nullptr);
	}

	// display-only shortcut label; the EV keyboard layer performs the
	// actual binding
	if (szMnemonicName && *szMnemonicName && !isPopup)
	{
		std::string accel;
		_convertStringToGtkAccel(szMnemonicName, accel);
		if (!accel.empty())
			g_menu_item_set_attribute(item, "accel", "s", accel.c_str());
	}

	return item;
}

/*!
 * (Re)build the whole GMenuModel from the menu layout.  Called from
 * synthesizeMenu() and again from _refreshMenu() when the set of
 * visible items changed (dynamic labels such as the recent-documents
 * list).
 */
void EV_UnixMenu::_buildItems(GMenu * pMenuRoot, bool isPopup)
{
	const EV_Menu_ActionSet * pMenuActionSet = m_pUnixApp->getMenuActionSet();
	UT_ASSERT(pMenuActionSet);

	size_t nrLabelItemsInLayout = m_pMenuLayout->getLayoutItemCount();
	UT_ASSERT(nrLabelItemsInLayout > 0);

	g_menu_remove_all(pMenuRoot);
	m_vecItemRecs.clear();

	// stacks tracking the menu hierarchy being built
	std::stack<GMenu*> menuStack;
	std::stack<GMenu*> sectionStack;
	menuStack.push(pMenuRoot);

	// every menu level starts with an implicit section so that
	// separators can split the items into groups
	GMenu * firstSection = g_menu_new();
	g_menu_append_section(pMenuRoot, nullptr, G_MENU_MODEL(firstSection));
	sectionStack.push(firstSection);

	GSimpleAction * radioGroup = nullptr;

	for (size_t k = 0; (k < nrLabelItemsInLayout); k++)
	{
		EV_Menu_LayoutItem * pLayoutItem = m_pMenuLayout->getLayoutItem(k);
		UT_continue_if_fail(pLayoutItem);

		XAP_Menu_Id id = pLayoutItem->getMenuId();
		const EV_Menu_Action * pAction = pMenuActionSet->getAction(id);
		const EV_Menu_Label * pLabel = m_pMenuLabelSet->getLabel(id);
		if (!pAction || !pLabel) {
			// missing action or label; keep rec indexes aligned
			// with the layout so _refreshMenu() stays in sync
			UT_DEBUGMSG(("EV_UnixMenu: no action/label for item %u\n",
						 (unsigned)id));
			m_vecItemRecs.push_back(_ItemRec());
			continue;
		}

		switch (pLayoutItem->getMenuLayoutFlags())
		{
		case EV_MLF_Normal:
		{
			const char ** data = getLabelName(m_pUnixApp, pAction, pLabel);
			if (!data) {
				// getLabelName() fails when the bound edit method is
				// not registered; skip instead of dereferencing null
				UT_DEBUGMSG(("EV_UnixMenu: no label name for item %u\n",
							 (unsigned)id));
				m_vecItemRecs.push_back(_ItemRec());
				continue;
			}
			const char * szLabelName = data[0];
			const char * szMnemonicName = data[1];

			_ItemRec rec;
			rec.id = id;
			rec.isRadio = pAction->isRadio();

			if (szLabelName && *szLabelName)
			{
				GMenuItem * item = _createMenuItem(id, pAction,
												 szLabelName, szMnemonicName,
												 isPopup, &radioGroup);
				if (pAction->isRadio())
				{
					// consecutive radio items share the group action
					rec.action = radioGroup;
				}
				else
				{
					char name[64];
					g_snprintf(name, sizeof(name), "item_%u",
							   static_cast<unsigned>(id));
					rec.action = G_SIMPLE_ACTION(g_action_map_lookup_action(
						G_ACTION_MAP(m_actionGroup), name));
					radioGroup = nullptr;
				}
				g_menu_append_item(sectionStack.top(), item);
				g_object_unref(item);
				rec.label = szLabelName;
				rec.present = true;
			}
			else
			{
				radioGroup = pAction->isRadio() ? radioGroup : nullptr;
			}

			m_vecItemRecs.push_back(rec);
			break;
		}
		case EV_MLF_BeginSubMenu:
		{
			const char ** data = _ev_GetLabelName(m_pUnixApp, m_pFrame, pAction, pLabel);
			const char * szLabelName = data[0];
			radioGroup = nullptr;

			GMenu * sub = g_menu_new();
			_ItemRec rec;
			rec.id = id;

			if (szLabelName && *szLabelName)
			{
				char buf[1024];
				_ev_convert(buf, szLabelName);

				// if the underlined mnemonic would collide with an
				// Alt+key binding, drop the underline
				guint keyCode = _ev_get_underlined_char(buf);
				bool bConflict = false;
				if (keyCode != GDK_KEY_VoidSymbol)
				{
					EV_EditEventMapper * pEEM = XAP_App::getApp()->getEditEventMapper();
					UT_ASSERT(pEEM);
					EV_EditMethod * pEM = nullptr;
					pEEM->Keystroke(EV_EKP_PRESS|EV_EMS_ALT|keyCode,&pEM);
					bConflict = (pEM != nullptr);
				}
				if (bConflict)
				{
					char * dup = g_strdup(buf);
					_ev_strip_underline(dup, buf);
					g_strlcpy(buf, dup, sizeof(buf));
					FREEP(dup);
				}

				GMenuItem * item = g_menu_item_new(buf, nullptr);
				g_menu_item_set_submenu(item, G_MENU_MODEL(sub));

				// dummy action purely to control the submenu's
				// sensitivity from _refreshMenu
				char name[64];
				g_snprintf(name, sizeof(name), "sub_%u", static_cast<unsigned>(id));
				GAction * subAction = g_action_map_lookup_action(G_ACTION_MAP(m_actionGroup), name);
				if (!subAction)
				{
					GSimpleAction * sa = g_simple_action_new(name, nullptr);
					g_action_map_add_action(G_ACTION_MAP(m_actionGroup), G_ACTION(sa));
					g_object_unref(sa);
					subAction = G_ACTION(sa);
				}
				char fullName[72];
				g_snprintf(fullName, sizeof(fullName), "menu.%s", name);
				g_menu_item_set_action_and_target_value(item, fullName, nullptr);
				rec.action = G_SIMPLE_ACTION(subAction);
				rec.label = szLabelName;
				rec.present = true;

				g_menu_append_item(sectionStack.top(), item);
				g_object_unref(item);
			}

			menuStack.push(sub);
			GMenu * section = g_menu_new();
			g_menu_append_section(sub, nullptr, G_MENU_MODEL(section));
			sectionStack.push(section);
			m_vecItemRecs.push_back(rec);
			break;
		}
		case EV_MLF_EndSubMenu:
		{
			g_object_unref(menuStack.top());
			menuStack.pop();
			g_object_unref(sectionStack.top());
			sectionStack.pop();
			radioGroup = nullptr;

			m_vecItemRecs.push_back(_ItemRec());
			break;
		}
		case EV_MLF_Separator:
		{
			radioGroup = nullptr;

			// close the current section and open a fresh one
			GMenu * section = g_menu_new();
			g_menu_append_section(menuStack.top(), nullptr, G_MENU_MODEL(section));
			g_object_unref(sectionStack.top());
			sectionStack.pop();
			sectionStack.push(section);

			m_vecItemRecs.push_back(_ItemRec());
			break;
		}

		case EV_MLF_BeginPopupMenu:
		case EV_MLF_EndPopupMenu:
			m_vecItemRecs.push_back(_ItemRec());	// reserve slot so indexes stay in sync
			break;

		default:
			UT_ASSERT(0);
			break;
		}
	}

	UT_ASSERT(menuStack.top() == pMenuRoot);
	menuStack.pop();
	while (!sectionStack.empty())
	{
		g_object_unref(sectionStack.top());
		sectionStack.pop();
	}
}

bool EV_UnixMenu::synthesizeMenu(GMenu * pMenuRoot, bool isPopup)
{
	m_isPopup = isPopup;
	_buildItems(pMenuRoot, isPopup);
	return true;
}

static bool _ev_has_realized_popover(GtkWidget * widget)
{
	if (!widget)
		return false;
	// realized, not just visible: a dismissed GtkPopoverMenu stays
	// realized through its teardown, and swapping the model while it
	// unmaps makes GTK touch disposed item widgets
	// (gtk_widget_get_mapped(NULL) criticals)
	if (GTK_IS_POPOVER(widget) && gtk_widget_get_realized(widget))
		return true;
	for (GtkWidget * c = gtk_widget_get_first_child(widget); c;
		 c = gtk_widget_get_next_sibling(c))
	{
		if (_ev_has_realized_popover(c))
			return true;
	}
	return false;
}

gboolean EV_UnixMenu::_s_rebuildTick(gpointer data)
{
	EV_UnixMenu * menu = static_cast<EV_UnixMenu*>(data);
	UT_return_val_if_fail(menu, G_SOURCE_REMOVE);

	GtkWidget * bound = menu->_boundWidget();
	bool hasPop = bound && _ev_has_realized_popover(bound);
	if (hasPop && menu->m_rebuildTicks < 30)
	{
		// a GtkPopoverMenu is still open (or tearing down); replacing
		// the model now would remove the item widget under the pointer.
		// bounded so a popover that stays realized after close can't
		// starve the rebuild (~1s at the 30ms tick)
		menu->m_rebuildTicks++;
		return G_SOURCE_CONTINUE;
	}
	menu->m_rebuildTicks = 0;

	menu->m_rebuildSourceId = 0;
	menu->m_rebuildPending = false;
	if (!bound)
		return G_SOURCE_REMOVE;

	GMenu * fresh = g_menu_new();
	menu->_buildItems(fresh, menu->m_isPopup);
	menu->_setModelOnBoundWidget(fresh);
	g_object_unref(menu->m_pMenuModel);
	menu->m_pMenuModel = fresh;

	if (menu->getFrame() && menu->getFrame()->getCurrentView())
		menu->_refreshMenu(menu->getFrame()->getCurrentView());
	return G_SOURCE_REMOVE;
}

void EV_UnixMenu::_rebuildBoundModel()
{
	if (!_hasBoundWidget())
	{
		_buildItems(m_pMenuModel, m_isPopup);
		return;
	}

	// the widget is live: defer the model swap until no popover menu
	// is open (see _s_rebuildTick)
	m_rebuildPending = true;
	if (m_rebuildSourceId == 0)
		m_rebuildSourceId = g_timeout_add(30, _s_rebuildTick, this);
}

bool EV_UnixMenu::_refreshMenu(AV_View * pView)
{
	if (m_rebuildPending)
	{
		// a deferred model swap is pending; m_vecItemRecs is stale
		// and will be re-synced by _s_rebuildTick after the rebuild
		return true;
	}

	const EV_Menu_ActionSet * pMenuActionSet = m_pUnixApp->getMenuActionSet();
	UT_ASSERT(pMenuActionSet);
	size_t nrLabelItemsInLayout = m_pMenuLayout->getLayoutItemCount();

	if (m_vecItemRecs.size() != nrLabelItemsInLayout)
	{
		// layout changed underneath us (plugin item added)
		_rebuildBoundModel();
		if (m_rebuildPending)
			return true;
	}

	m_bUpdatingActions = true;

	GSimpleAction * radioGroup = nullptr;

	for (size_t k = 0; k < nrLabelItemsInLayout; ++k)
	{
		EV_Menu_LayoutItem * pLayoutItem = m_pMenuLayout->getLayoutItem(k);
		UT_continue_if_fail(pLayoutItem);

		XAP_Menu_Id id = pLayoutItem->getMenuId();
		const EV_Menu_Action * pAction = pMenuActionSet->getAction(id);
		const EV_Menu_Label * pLabel = m_pMenuLabelSet->getLabel(id);
		UT_continue_if_fail(pAction && pLabel);

		EV_Menu_LayoutFlags flags = pLayoutItem->getMenuLayoutFlags();
		if (flags != EV_MLF_Normal && flags != EV_MLF_BeginSubMenu)
		{
			if (flags != EV_MLF_EndSubMenu && flags != EV_MLF_Separator &&
				flags != EV_MLF_BeginPopupMenu && flags != EV_MLF_EndPopupMenu)
				radioGroup = nullptr;
			else
				radioGroup = nullptr;
			continue;
		}

		bool bEnable = true;
		bool bCheck = false;
		if (pAction->hasGetStateFunction())
		{
			EV_Menu_ItemState mis = pAction->getMenuItemState(pView);
			if (mis & EV_MIS_Gray)
				bEnable = false;
			if (mis & EV_MIS_Toggled)
				bCheck = true;
		}

		// dynamic labels can make items appear/disappear - detect and
		// rebuild the model once rather than patching item by item
		const char ** data = _ev_GetLabelName(m_pUnixApp, m_pFrame, pAction, pLabel);
		const char * szLabelName = data[0];
		bool wantPresent = (szLabelName && *szLabelName);

		_ItemRec & rec = m_vecItemRecs[k];
		if (wantPresent != rec.present ||
			(wantPresent && pAction->hasDynamicLabel() && rec.label != szLabelName))
		{
			m_bUpdatingActions = false;
			_rebuildBoundModel();
			m_bUpdatingActions = true;
			if (m_rebuildPending)
			{
				m_bUpdatingActions = false;
				return true;
			}
			// restart; item recs were rebuilt
			k = static_cast<size_t>(-1);
			radioGroup = nullptr;
			continue;
		}

		if (!rec.present)
			continue;

		if (flags == EV_MLF_BeginSubMenu)
		{
			if (rec.action)
				g_simple_action_set_enabled(rec.action, bEnable);
			radioGroup = nullptr;
			continue;
		}

		if (pAction->isRadio())
		{
			radioGroup = rec.action;
			// the checked radio item of a run drives the shared
			// action's state
			if (radioGroup && bCheck)
			{
				char target[32];
				g_snprintf(target, sizeof(target), "%u", static_cast<unsigned>(id));
				g_simple_action_set_state(radioGroup, g_variant_new_string(target));
			}
			if (radioGroup)
				g_simple_action_set_enabled(radioGroup, bEnable);
			continue;
		}

		radioGroup = nullptr;
		if (!rec.action)
			continue;

		g_simple_action_set_enabled(rec.action, bEnable);
		if (pAction->isCheckable())
		{
			g_simple_action_set_state(rec.action, g_variant_new_boolean(bCheck));
		}
	}

	m_bUpdatingActions = false;
	return true;
}

/*!
 * That will add a new menu entry for the menu item at layout_pos.
 *
 * @param layout_pos UT_uint32 with the relative position of the item in the
 * menu.
 * @return true if there were no problems.  False elsewere.
 */
bool EV_UnixMenu::_doAddMenuItem(UT_uint32 layout_pos)
{
	if (layout_pos > 0) {
		m_vecItemRecs.insert(m_vecItemRecs.begin() + layout_pos, _ItemRec());
		// a new layout item appeared: rebuild the model
		_rebuildBoundModel();
		return true;
	}

	return false;
}

/*****************************************************************/

EV_UnixMenuBar::EV_UnixMenuBar(XAP_UnixApp * pUnixApp,
							   XAP_Frame * pFrame,
							   const char * szMenuLayoutName,
							   const char * szMenuLabelSetName)
	: EV_UnixMenu(pUnixApp, pFrame, szMenuLayoutName, szMenuLabelSetName)
	, m_wMenuBar(nullptr)
{
}

EV_UnixMenuBar::~EV_UnixMenuBar()
{
}

void  EV_UnixMenuBar::destroy(void)
{
	if (m_wMenuBar)
	{
		gtk_widget_unparent(m_wMenuBar);
		m_wMenuBar = nullptr;
	}
}

static gboolean _ev_menubar_motion_refresh(GtkEventControllerMotion * /*controller*/,
										   gdouble /*x*/, gdouble /*y*/,
										   gpointer data)
{
	EV_UnixMenuBar * menu = static_cast<EV_UnixMenuBar*>(data);
	if (menu && menu->getFrame() && menu->getFrame()->getCurrentView())
		menu->refreshMenu(menu->getFrame()->getCurrentView());
	return FALSE;
}

GtkWidget * EV_UnixMenuBar::_createMenuBarWidget(GMenu * model)
{
	GtkWidget * bar = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(model));
	gtk_widget_insert_action_group(bar, "menu", G_ACTION_GROUP(m_actionGroup));

	// GMenuModels are live: action states are bound to the displayed
	// items.  We still need a trigger to sync states before a menu
	// opens - pointer entering the bar is a good enough proxy.
	GtkEventController * motion = gtk_event_controller_motion_new();
	g_signal_connect(motion, "enter", G_CALLBACK(_ev_menubar_motion_refresh), this);
	gtk_widget_add_controller(bar, motion);

	return bar;
}

void EV_UnixMenuBar::_setModelOnBoundWidget(GMenu * model)
{
	// Do not mutate the live bar's model: GTK4 keeps internal submenu
	// pointers that can go stale and emit criticals on teardown.
	// Replacing the whole bound widget leaves GTK with consistent state.
	GtkWidget * vbox = gtk_widget_get_parent(m_wMenuBar);
	if (!vbox) {
		gtk_popover_menu_bar_set_menu_model(GTK_POPOVER_MENU_BAR(m_wMenuBar),
											G_MENU_MODEL(model));
		return;
	}

	GtkWidget * oldBar = m_wMenuBar;
	GtkWidget * newBar = _createMenuBarWidget(model);
	gtk_widget_insert_after(newBar, GTK_WIDGET(vbox), oldBar);
	m_wMenuBar = newBar;
	// preserve visibility (e.g. hidden in ribbon mode); new widgets
	// default to visible in GTK4
	gtk_widget_set_visible(newBar, gtk_widget_get_visible(oldBar));
	gtk_widget_unparent(oldBar);
}

bool EV_UnixMenuBar::synthesizeMenuBar()
{
	GtkWidget * wVBox = static_cast<XAP_UnixFrameImpl *>(m_pFrame->getFrameImpl())->getVBoxWidget();

	synthesizeMenu(m_pMenuModel, false);

	m_wMenuBar = _createMenuBarWidget(m_pMenuModel);

	gtk_box_append(GTK_BOX(wVBox), m_wMenuBar);

	return true;
}


bool EV_UnixMenuBar::rebuildMenuBar()
{
	destroy();
	return synthesizeMenuBar();
}

bool EV_UnixMenuBar::refreshMenu(AV_View * pView)
{
	// this makes an exception for initialization where a view
	// might not exist... silly to refresh the menu then; it will
	// happen in due course to its first display
	if (pView)
		return _refreshMenu(pView);

	return true;
}

/*****************************************************************/

EV_UnixMenuPopup::EV_UnixMenuPopup(XAP_UnixApp * pUnixApp,
								   XAP_Frame * pFrame,
								   const char * szMenuLayoutName,
								   const char * szMenuLabelSetName)
	: EV_UnixMenu(pUnixApp, pFrame, szMenuLayoutName, szMenuLabelSetName)
	, m_wMenuPopup(nullptr)
{
}

EV_UnixMenuPopup::~EV_UnixMenuPopup()
{
}

GtkWidget * EV_UnixMenuPopup::getMenuHandle() const
{
	return m_wMenuPopup;
}

static void _ev_popup_refresh(GtkWidget * /*widget*/, gpointer data)
{
	EV_UnixMenuPopup * menu = static_cast<EV_UnixMenuPopup*>(data);
	if (menu && menu->getFrame() && menu->getFrame()->getCurrentView())
		menu->refreshMenu(menu->getFrame()->getCurrentView());
}

GtkWidget * EV_UnixMenuPopup::_createPopupWidget(GMenu * model)
{
	GtkWidget * popup = gtk_popover_menu_new_from_model(G_MENU_MODEL(model));
	gtk_widget_insert_action_group(popup, "menu", G_ACTION_GROUP(m_actionGroup));
	gtk_widget_set_parent(popup, static_cast<XAP_UnixFrameImpl *>(
		m_pFrame->getFrameImpl())->getTopLevelWindow());
	return popup;
}

void EV_UnixMenuPopup::_setModelOnBoundWidget(GMenu * model)
{
	// See EV_UnixMenuBar::_setModelOnBoundWidget: replace the bound
	// widget rather than mutating its live model.
	GtkWidget * oldPopup = m_wMenuPopup;
	GtkWidget * newPopup = _createPopupWidget(model);
	m_wMenuPopup = newPopup;
	gtk_widget_unparent(oldPopup);
}

bool EV_UnixMenuPopup::synthesizeMenuPopup()
{
	synthesizeMenu(m_pMenuModel, true);

	m_wMenuPopup = _createPopupWidget(m_pMenuModel);

	// refresh the model just before the popup is displayed so that
	// enable/check states are current
	g_signal_connect(G_OBJECT(m_wMenuPopup), "map",
					 G_CALLBACK(_ev_popup_refresh), this);

	return true;
}

bool EV_UnixMenuPopup::refreshMenu(AV_View * pView)
{
	// this makes an exception for initialization where a view
	// might not exist... silly to refresh the menu then; it will
	// happen in due course to its first display
	if (pView)
		return _refreshMenu(pView);

	return true;
}
