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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ut_locale.h"
#include "ut_string.h"
#include "ut_debugmsg.h"
#include "xap_App.h"
#include "ap_UnixPrefs.h"

/*****************************************************************/

AP_UnixPrefs::AP_UnixPrefs()
	: AP_Prefs()
{
}

const char * AP_UnixPrefs::_getPrefsPathname(void) const
{
	/* return a pointer to a static buffer */
	static std::string buf;

	if(!buf.empty())
	  return buf.c_str();

	const char * szDirectory = XAP_App::getApp()->getUserPrivateDirectory();
	const char * szFile = "profile";

	buf = szDirectory;
	if (!buf.size() || szDirectory[buf.size()-1] != '/')
	  buf += "/";
	buf += szFile;

	// migration / legacy
	XAP_App::getApp()->migrate("/Abinova.Profile", szFile, buf.c_str());

	return buf.c_str();
}

void AP_UnixPrefs::overlayEnvironmentPrefs(void)
{
	// UI localization is fixed to the builtin English string set;
	// nothing in the environment needs to be overlaid.
}
