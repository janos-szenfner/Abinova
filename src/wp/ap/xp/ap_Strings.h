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

#include "xap_Strings.h"

//////////////////////////////////////////////////////////////////
// build a table of AP ID values
//////////////////////////////////////////////////////////////////

#define dcl(id,s)					AP_STRING_ID_##id,

enum AP_String_Id_Enum: uint16_t
{
	AP_STRING_ID__FIRST__			= 1000,	/* must be first -- must be >= XAP_STRING_ID__LAST__ */
#include "ap_String_Id.h"
	AP_STRING_ID__LAST__					/* must be last */
};

#undef dcl

//////////////////////////////////////////////////////////////////
// a sub-class to wrap the compiled-in (english) strings
//////////////////////////////////////////////////////////////////

class ABI_EXPORT AP_BuiltinStringSet : public XAP_BuiltinStringSet
{
public:
	AP_BuiltinStringSet(XAP_App * pApp, const gchar * szLanguageName);
	virtual ~AP_BuiltinStringSet(void);

	virtual const gchar *	getValue(XAP_String_Id id) const override;

#ifdef DEBUG
#endif

protected:
	const gchar **			m_arrayAP;
};

//////////////////////////////////////////////////////////////////
// a sub-class to deal with disk-based string sets (translations)
//////////////////////////////////////////////////////////////////

