/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
/* Abinova
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

#include <string>
#include <vector>

#include "ut_types.h"
#include "ut_units.h"
#include "xap_Frame.h"
#include "xap_Dialog.h"
#include "pp_Property.h"
#include "fl_BlockLayout.h" // eTabType/eTabLeader

class XAP_Frame;

/* Word-style Tabs dialog (Format > Tabs / ruler double-click): a
 * position entry, the current stops list, alignment and leader
 * selectors and a default-interval field.  The model is the
 * "tabstops"/"default-tab-interval" block property pair; platforms
 * edit through the Stop list and serialize() writes the property
 * string. */
class ABI_EXPORT AP_Dialog_Tabs : public XAP_Dialog_NonPersistent
{
public:
	AP_Dialog_Tabs(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id);
	virtual ~AP_Dialog_Tabs(void);

	virtual void runModal(XAP_Frame * pFrame) override = 0;

	enum tAnswer: uint8_t { a_OK, a_CANCEL };

	AP_Dialog_Tabs::tAnswer	getAnswer(void) const;

	bool setDialogData(const PP_PropertyVector & pProps);
	bool getDialogData(PP_PropertyVector & pProps);

	struct Stop {
		UT_sint32 iPosition;	/* layout units */
		eTabType  iType;
		eTabLeader iLeader;
	};

	/* the stops, sorted by position — Set replaces a stop whose
	 * position matches (within one twip) or inserts in order */
	const std::vector<Stop> &	getStops(void) const { return m_vecStops; }
	void						setStop(UT_sint32 iPosition, eTabType iType, eTabLeader iLeader);
	void						clearStop(UT_sint32 index);
	void						clearAllStops(void);

	/* dimensioned string for the "default tab stops" field
	 * ("default-tab-interval" property) */
	const std::string &			getDefaultInterval(void) const { return m_sDefaultInterval; }
	void						setDefaultInterval(const std::string & s) { m_sDefaultInterval = s; }

	/* ruler units the dialog shows positions in; taken from the
	 * RulerUnits pref at construction */
	UT_Dimension				getDimension(void) const { return m_dim; }
	/* "pos/TL,…" serialization of the model for the tabstops prop;
	 * positions are emitted in the dialog's own dimension */
	std::string					serializeStops(void) const;
	/* one position as a dimensioned display string */
	std::string					formatPosition(UT_sint32 iPosition) const;

protected:
	std::vector<Stop>			m_vecStops;
	std::string					m_sDefaultInterval;
	UT_Dimension				m_dim;
	tAnswer						m_answer;
};
