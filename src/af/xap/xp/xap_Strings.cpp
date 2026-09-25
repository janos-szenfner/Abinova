/* AbiSource Application Framework
 * Copyright (C) 1998-2000 AbiSource, Inc.
 * BIDI Copyright (C) 2001,2002 Tomas Frydrych
 * Copyright (C) 2002 Dom Lachowicz
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

#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ut_string.h"
#include "ut_growbuf.h"
#include "ut_bytebuf.h"
#include "ut_wctomb.h"
#include "ut_iconv.h"
#include "ut_Language.h"

#include "xap_App.h"
#include "xap_Strings.h"
#include "xap_EncodingManager.h"

//////////////////////////////////////////////////////////////////
// base class provides interface regardless of how we got the strings
//////////////////////////////////////////////////////////////////

XAP_StringSet::XAP_StringSet(XAP_App * pApp, const gchar * szLanguageName)
  : m_encoding("UTF-8")
{
	m_pApp = pApp;

	m_szLanguageName = nullptr;
	if (szLanguageName && *szLanguageName)
		m_szLanguageName = g_strdup(szLanguageName);
}

XAP_StringSet::~XAP_StringSet(void)
{
	if (m_szLanguageName)
		g_free(const_cast<gchar *>(m_szLanguageName));
}

const gchar * XAP_StringSet::getLanguageName(void) const
{
	return m_szLanguageName;
}

bool XAP_StringSet::getValue(XAP_String_Id id, const char * inEncoding, std::string &s) const
{
	const char * toTranslate = getValue(id);

	UT_return_val_if_fail(toTranslate != nullptr, false);

	if(!strcmp(m_encoding.c_str(),inEncoding))
	{
		s = toTranslate;
	}
	else
	{
	        UT_iconv_t conv = UT_iconv_open(inEncoding, m_encoding.c_str());
		UT_return_val_if_fail(UT_iconv_isValid(conv), false);
	  
		char * translated = UT_convert_cd(toTranslate, strlen (toTranslate)+1, conv, nullptr, nullptr);
		
		UT_iconv_close(conv);
		
		UT_return_val_if_fail(translated, false);
		s = translated;
		
		g_free(translated);
	}
	
	return true;
}

bool XAP_StringSet::getValueUTF8(XAP_String_Id id, std::string & s) const
{	
	return getValue(id, "UTF-8", s);
}

void XAP_StringSet::setEncoding(const gchar * inEncoding)
{
  UT_return_if_fail(inEncoding != nullptr);
  m_encoding = inEncoding;
}

const char * XAP_StringSet::getEncoding() const
{
  return m_encoding.c_str();
}

//////////////////////////////////////////////////////////////////
// a sub-class to wrap the compiled-in (english) strings
// (there will only be one instance of this sub-class)
// (since these strings are English, we will not bother with any
// (bidi processing, we only need this in the disk stringset)
//////////////////////////////////////////////////////////////////

XAP_BuiltinStringSet::XAP_BuiltinStringSet(XAP_App * pApp, const gchar * szLanguageName)
	: XAP_StringSet(pApp,szLanguageName)
{
#define dcl(id,s)					static_cast<const gchar *>(s),

	static const gchar * s_a[] =
	{
		dcl(__FIRST__,0)			// bogus entry for zero
#include "xap_String_Id.h"
		dcl(__LAST__,0)				// bogus entry for end
	};

	m_arrayXAP = s_a;

#undef dcl
}

XAP_BuiltinStringSet::~XAP_BuiltinStringSet(void)
{
}

const gchar * XAP_BuiltinStringSet::getValue(XAP_String_Id id) const
{
	if ( (id > XAP_STRING_ID__FIRST__) && (id < XAP_STRING_ID__LAST__) )
		return m_arrayXAP[id];

	return nullptr;
}

//////////////////////////////////////////////////////////////////
