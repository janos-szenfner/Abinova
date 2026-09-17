/* AbiWord
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

#include "xap_Frame.h"
#include "xap_Dialog.h"
#include "xav_View.h"


#define CURRENT_DATE_TIME_SIZE 256

extern const char *InsertDateTimeFmts[];

class XAP_Frame;

class ABI_EXPORT AP_Dialog_Insert_DateTime : public XAP_Dialog_NonPersistent
{
public:
    enum tAnswer: uint8_t { a_OK, a_CANCEL };

    AP_Dialog_Insert_DateTime(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id);
    AP_Dialog_Insert_DateTime::tAnswer    getAnswer(void) const;
    const char *GetDateTimeFormat(void) const;
protected:
    AP_Dialog_Insert_DateTime::tAnswer    m_answer;
    int m_iFormatIndex;
};
