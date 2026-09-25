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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ap_Features.h"

#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ut_string.h"
#include "ut_growbuf.h"
#include "ut_bytebuf.h"
#include "ut_wctomb.h"

#include "xap_App.h"
#include "xap_EncodingManager.h"

#include "ap_Strings.h"

//////////////////////////////////////////////////////////////////
// a sub-class to wrap the compiled-in (english) strings
// (there will only be one instance of this sub-class)
//////////////////////////////////////////////////////////////////

AP_BuiltinStringSet::AP_BuiltinStringSet(XAP_App * pApp, const gchar * szLanguageName)
	: XAP_BuiltinStringSet(pApp,szLanguageName)
{
#define dcl(id,s)					s,

	static const gchar * s_a[] =
	{
		dcl(__FIRST__, nullptr)			// bogus entry for zero
#include "ap_String_Id.h"
		dcl(__LAST__, nullptr)			// bogus entry for end
	};

	m_arrayAP = s_a;

#undef dcl
}

AP_BuiltinStringSet::~AP_BuiltinStringSet(void)
{
}

const gchar * AP_BuiltinStringSet::getValue(XAP_String_Id id) const
{
	// if it's in our range, we fetch it.
	// otherwise, we hand it down to the base class.
	
	if ( (id > AP_STRING_ID__FIRST__) && (id < AP_STRING_ID__LAST__) )
		return m_arrayAP[id-AP_STRING_ID__FIRST__];

	return XAP_BuiltinStringSet::getValue(id);
}


//////////////////////////////////////////////////////////////////
