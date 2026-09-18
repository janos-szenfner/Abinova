/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:t -*- */
/* AbiSource Program Utilities
 * Copyright (C) 1998-2000 AbiSource, Inc.
 * Copyright (C) 2019 Hubert Figuière
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

#ifndef EV_UNIXMENU_H
#define EV_UNIXMENU_H

#include <string>
#include <vector>

#include <gtk/gtk.h>

#include "ut_types.h"
#include "xap_Types.h"
#include "ev_Menu.h"

class AV_View;
class XAP_UnixApp;
class XAP_UnixFrameImpl;
class XAP_Frame;

/*****************************************************************/

class EV_UnixMenu : public EV_Menu
{
public:

	EV_UnixMenu(XAP_UnixApp * pUnixApp,
		    XAP_Frame * pFrame,
		    const char * szMenuLayoutName,
		    const char * szMenuLabelSetName);
	virtual ~EV_UnixMenu();

	bool				synthesizeMenu(GMenu * pMenuRoot, bool isPopup);
	bool				menuEvent(XAP_Menu_Id id) const;
	virtual bool		refreshMenu(AV_View * pView) = 0;

	XAP_Frame * 	getFrame() const;
	GMenu *			getMenuModel() const { return m_pMenuModel; }
	GActionGroup *	getActionGroup() const { return G_ACTION_GROUP(m_actionGroup); }

protected:
	bool				_refreshMenu(AV_View * pView);
	virtual bool		_doAddMenuItem(UT_uint32 layout_pos) override;

	// Rebuild the menu model. When a widget is already bound to
	// m_pMenuModel (menubar/popup), the model must not be mutated in
	// place: removing items from a live model while a GtkPopoverMenu is
	// open crashes inside GTK. A fresh model is built and swapped in
	// via _setModelOnBoundWidget() instead.
	void				_rebuildBoundModel();
	virtual bool		_hasBoundWidget() const { return false; }
	virtual void		_setModelOnBoundWidget(GMenu * /*model*/) {}

protected: // FIXME! These variables should be private.
	XAP_UnixApp *		m_pUnixApp;
	XAP_Frame *  	m_pFrame;

	// The root of the menu model. Sections inside it are used to
	// render separators, like GtkMenu did.
	GMenu *				m_pMenuModel;
	bool				m_isPopup;

	// One action group per menu instance, inserted on the menu's
	// root widget under the "menu" prefix so that items resolve
	// actions named "menu.item_<id>".
	GSimpleActionGroup *	m_actionGroup;

	// Set while refreshMenu updates action states, so the
	// change-state handlers don't fire menu events recursively.
	bool				m_bUpdatingActions;

	struct _ItemRec
	{
		XAP_Menu_Id			id = XAP_Menu_Id(0);
		GSimpleAction *		action = nullptr;   // non-owning
		std::string			label;              // last synced label
		bool				present = false;
		bool				isRadio = false;
	};

	std::vector<_ItemRec> m_vecItemRecs;

	class _wd
	{
	public:
		_wd(EV_UnixMenu * pUnixMenu, XAP_Menu_Id id);
		~_wd();
		static void s_onActivate(GSimpleAction * action, GVariant * param,
								 gpointer callback_data);
		static void s_onChangeState(GSimpleAction * action, GVariant * value,
									gpointer callback_data);

		EV_UnixMenu* m_pUnixMenu;
		XAP_Menu_Id	m_id;
	};

	std::vector<_wd*> m_vecCallbacks;
private:
	void _convertStringToGtkAccel(const char *s, std::string &out);
	GMenuItem * _createMenuItem(XAP_Menu_Id id,
								const EV_Menu_Action * pAction,
								const char *szLabelName,
								const char *szMnemonicName,
								bool isPopup,
								GSimpleAction ** radioGroup);
	GSimpleAction * _createAction(XAP_Menu_Id id,
								  const EV_Menu_Action * pAction,
								  GSimpleAction ** radioGroup);
	void _buildItems(GMenu * pMenuRoot, bool isPopup);
};

#endif /* EV_UNIXMENU_H */
