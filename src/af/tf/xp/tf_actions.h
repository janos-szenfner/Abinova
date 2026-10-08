/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
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

/*
 * TST01: reusable action enumerators — every registered edit method,
 * menu action and toolbar action, in declaration order, with the
 * empty slots a sparse action set keeps between its bounds filtered
 * out.  The registered sets ARE the test list: sweeps like
 * 'fire every action on a fresh doc' iterate these instead of
 * open-coding the __BOGUS1__..__BOGUS2__ sentinel loops.
 */

#ifndef TF_ACTIONS_H
#define TF_ACTIONS_H

#include <string>
#include <vector>

#include "ut_types.h"
#include "xap_Types.h"
#include "ev_EditMethod.h"
#include "ev_Menu_Actions.h"
#include "ev_Toolbar_Actions.h"

namespace tf_actions {

/* every edit method in the container, table order */
inline std::vector<EV_EditMethod *>
edit_methods(EV_EditMethodContainer *emc)
{
	std::vector<EV_EditMethod *> out;
	if (!emc)
		return out;
	const UT_uint32 n = emc->countEditMethods();
	for (UT_uint32 i = 0; i < n; ++i)
	{
		EV_EditMethod *em = emc->getNthEditMethod(i);
		if (em)
			out.push_back(em);
	}
	return out;
}

inline std::vector<std::string>
edit_method_names(EV_EditMethodContainer *emc)
{
	std::vector<std::string> out;
	for (EV_EditMethod *em : edit_methods(emc))
		out.emplace_back(em->getName() ? em->getName() : "");
	return out;
}

/* every registered menu action in id order — the sentinel ids
 * (AP_MENU_ID__BOGUS1__/__BOGUS2__) carry EV_Menu_Action rows with
 * null method names, so callers that invoke must check */
inline std::vector<const EV_Menu_Action *>
menu_actions(const EV_Menu_ActionSet *as)
{
	std::vector<const EV_Menu_Action *> out;
	if (!as)
		return out;
	for (UT_sint32 id = as->getFirstId(); id <= as->getLastId(); ++id)
	{
		const EV_Menu_Action *a =
			as->getAction(static_cast<XAP_Menu_Id>(id));
		if (a)
			out.push_back(a);
	}
	return out;
}

inline std::vector<XAP_Menu_Id>
menu_action_ids(const EV_Menu_ActionSet *as)
{
	std::vector<XAP_Menu_Id> out;
	for (const EV_Menu_Action *a : menu_actions(as))
		out.push_back(a->getMenuId());
	return out;
}

/* every registered toolbar action in id order */
inline std::vector<const EV_Toolbar_Action *>
toolbar_actions(const EV_Toolbar_ActionSet *as)
{
	std::vector<const EV_Toolbar_Action *> out;
	if (!as)
		return out;
	for (UT_sint32 id = as->getFirstId(); id <= as->getLastId(); ++id)
	{
		const EV_Toolbar_Action *a =
			as->getAction(static_cast<XAP_Toolbar_Id>(id));
		if (a)
			out.push_back(a);
	}
	return out;
}

inline std::vector<XAP_Toolbar_Id>
toolbar_action_ids(const EV_Toolbar_ActionSet *as)
{
	std::vector<XAP_Toolbar_Id> out;
	for (const EV_Toolbar_Action *a : toolbar_actions(as))
		out.push_back(a->getToolbarId());
	return out;
}

} /* namespace tf_actions */

#endif /* TF_ACTIONS_H */
