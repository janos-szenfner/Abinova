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

#include <stdlib.h>

#include "ut_assert.h"
#include "ut_string.h"
#include "ut_string_class.h"
#include "ut_debugmsg.h"

#include "xap_Dialog_Id.h"
#include "xap_DialogFactory.h"
#include "xap_Prefs.h"
#include "xap_App.h"
#include "ap_Prefs_SchemeIds.h"
#include "ap_Dialog_CellMargins.h"

AP_Dialog_CellMargins::AP_Dialog_CellMargins(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id)
	: XAP_Dialog_NonPersistent(pDlgFactory, id, "interface/dialogcellmargins"),
	  m_answer(a_CANCEL),
	  m_dim(DIM_IN),
	  m_dMargin{ 0.0, 0.0, 0.0, 0.0 }
{
	std::string rulerUnits;
	if (getApp()->getPrefsValue(AP_PREF_KEY_RulerUnits, rulerUnits))
	{
		m_dim = UT_determineDimension(rulerUnits.c_str());
	}
}

AP_Dialog_CellMargins::~AP_Dialog_CellMargins(void)
{
}

AP_Dialog_CellMargins::tAnswer AP_Dialog_CellMargins::getAnswer(void) const
{
	return m_answer;
}

UT_Dimension AP_Dialog_CellMargins::getDim(void) const
{
	return m_dim;
}

double AP_Dialog_CellMargins::getSpinIncr(void) const
{
	switch (m_dim)
	{
	case DIM_IN:	return 0.05;
	case DIM_CM:	return 0.1;
	case DIM_MM:	return 1.0;
	case DIM_PI:	return 0.5;
	case DIM_PT:	return 1.0;
	default:		return 0.1;
	}
}

double AP_Dialog_CellMargins::getSpinMin(void) const
{
	return 0.0;
}

double AP_Dialog_CellMargins::getSpinMax(void) const
{
	// generous bound; cell padding beyond a page is pointless anyway
	switch (m_dim)
	{
	case DIM_IN:	return 22.0;
	case DIM_CM:	return 55.0;
	case DIM_MM:	return 550.0;
	case DIM_PI:	return 132.0;
	case DIM_PT:	return 1584.0;
	default:		return 100.0;
	}
}

void AP_Dialog_CellMargins::setMarginInches(int edge, double v)
{
	if (edge >= 0 && edge < 4)
	{
		m_dMargin[edge] = v;
	}
}

double AP_Dialog_CellMargins::getMarginInches(int edge) const
{
	if (edge >= 0 && edge < 4)
	{
		return m_dMargin[edge];
	}
	return 0.0;
}

void AP_Dialog_CellMargins::setMarginDim(int edge, double v)
{
	setMarginInches(edge, UT_convertDimToInches(v, m_dim));
}

double AP_Dialog_CellMargins::getMarginDim(int edge) const
{
	return UT_convertInchesToDimension(getMarginInches(edge), m_dim);
}
