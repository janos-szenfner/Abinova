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

#pragma once

#include "ut_types.h"
#include "ut_units.h"
#include "xap_Frame.h"
#include "xap_Dialog.h"

class XAP_Frame;

class ABI_EXPORT AP_Dialog_CellMargins : public XAP_Dialog_NonPersistent
{
public:
	AP_Dialog_CellMargins(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id);
	virtual ~AP_Dialog_CellMargins(void);

	virtual void runModal(XAP_Frame * pFrame) override = 0;

	enum tAnswer: uint8_t { a_OK, a_CANCEL };
	// edge indexes into m_dMargin[]
	enum marginEdge: uint8_t {
		edge_TOP = 0, edge_RIGHT, edge_BOTTOM, edge_LEFT
	};

	AP_Dialog_CellMargins::tAnswer	getAnswer(void) const;

	UT_Dimension					getDim(void) const;
	double							getSpinIncr(void) const;
	double							getSpinMin(void) const;
	double							getSpinMax(void) const;

	// margins live in inches internally; the GUI works in m_dim
	void							setMarginInches(int edge, double v);
	double							getMarginInches(int edge) const;
	void							setMarginDim(int edge, double v);
	double							getMarginDim(int edge) const;

protected:
	AP_Dialog_CellMargins::tAnswer	m_answer;
	UT_Dimension					m_dim;
	double							m_dMargin[4];	/* inches, edge_* */
};
