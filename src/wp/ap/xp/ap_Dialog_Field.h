/* AbiWord
 * Copyright (C) 1998-2000 AbiSource, Inc.
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

#include "xap_Frame.h"
#include "xap_Dialog.h"
#include "xav_View.h"
#include "fp_Run.h"

#define CURRENT_FIELD_SIZE 256

class XAP_Frame;

class ABI_EXPORT AP_Dialog_Field : public XAP_Dialog_NonPersistent
{
public:
	enum tAnswer: uint8_t { a_OK, a_CANCEL };

	AP_Dialog_Field(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id);
	virtual ~AP_Dialog_Field();
	AP_Dialog_Field::tAnswer	getAnswer(void) const;
	const char *GetFieldFormat(void) const;
	const gchar * getParameter(void) const {return m_pParameter;};
	void	setParameter(const gchar * pParam);
protected:
	AP_Dialog_Field::tAnswer	m_answer;
	int m_iTypeIndex;
	int m_iFormatIndex;
private:
	gchar * m_pParameter;
};
