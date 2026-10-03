/* AbiSource Application Framework
 * Copyright (C) 1998 AbiSource, Inc.
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

#include <vector>
#include "ev_Menu_Layouts.h"
/* #include "ev_Menu_Labels.h" */
#include "ev_EditBits.h"
#include "xap_Features.h"

class EV_Menu_LabelSet;

class XAP_App;
class XAP_StringSet;

class _vectt;

class ABI_EXPORT XAP_Menu_Factory
{
public:

	XAP_Menu_Factory(XAP_App * pApp);
	~XAP_Menu_Factory(void);
	EV_Menu_Layout * CreateMenuLayout(const char * szName);
	const char * FindContextMenu(EV_EditMouseContext emc);
	EV_Menu_LabelSet *  CreateMenuLabelSet(const char * szLanguage_);
	bool         buildMenuLabelSet(const char * szLanguage_);

private:

  std::vector<_vectt *> m_vecTT;
  XAP_App * m_pApp;
  EV_Menu_LabelSet * m_pLabelSet;
};
