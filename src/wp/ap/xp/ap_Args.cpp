/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* Abinova
 * Copyright (C) 2002 Patrick Lam
 * Copyright (C) 2008 Robert Staudinger
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ap_Features.h"
#include "xap_Strings.h"
#include "ap_Strings.h"
#include "ap_Prefs_SchemeIds.h"
#include "ap_Args.h"
#include "ap_App.h"
#include "ap_Convert.h"
#include "ut_debugmsg.h"
#include "ut_string.h"
#include "ut_misc.h"

// Static initializations:
#ifdef DEBUG
#endif
const char * AP_Args::m_sGeometry = nullptr;
const char * AP_Args::m_sToFormat = nullptr;
const char * AP_Args::m_sPrintTo = nullptr;
int AP_Args::m_iVerbose = 1;
const char ** AP_Args::m_sFiles = nullptr;
int AP_Args::m_iVersion = 0;
int AP_Args::m_iHelp = 0;
const char * AP_Args::m_sMerge = nullptr;
const char * AP_Args::m_impProps=nullptr;
const char * AP_Args::m_expProps=nullptr;
const char * AP_Args::m_sUserProfile = nullptr;
const char * AP_Args::m_sFileExtension = nullptr;
int AP_Args::m_iToThumb = 0;
const char * AP_Args::m_sName = nullptr; // name of output file
const char *  AP_Args::m_sThumbXY = "100x120"; // number of pixels in thumbnail by default


static GOptionEntry _entries[] = {
        {"geometry", 'g', 0, G_OPTION_ARG_STRING, &AP_Args::m_sGeometry, "Set initial frame geometry", "GEOMETRY"} ,
        {"to", 't', 0, G_OPTION_ARG_STRING, &AP_Args::m_sToFormat, "Target format of the file (abw, zabw, rtf, txt, utf8, html, ...), depends on available filter plugins", "FORMAT"},
        {"verbose", '\0', 0, G_OPTION_ARG_INT, &AP_Args::m_iVerbose, "Set verbosity level (0, 1, 2), with 2 being the most verbose", "LEVEL"},
        {"print", 'p',0, G_OPTION_ARG_STRING, &AP_Args::m_sPrintTo, "Print this file to printer","'Printer name' or '-' for default printer"},        {"merge", 'm', 0, G_OPTION_ARG_STRING, &AP_Args::m_sMerge, "Mail-merge", "FILE"},
        {"imp-props", 'i', 0, G_OPTION_ARG_STRING, &AP_Args::m_impProps, "Importer Arguments", "CSS String"},
        {"exp-props", 'e', 0, G_OPTION_ARG_STRING, &AP_Args::m_expProps, "Exporter Arguments", "CSS String"},
        {"thumb", '\0', 0, G_OPTION_ARG_INT, &AP_Args::m_iToThumb, "Make a thumb nail of the first page",""},
        {"sizeXY",'S', 0, G_OPTION_ARG_STRING, &AP_Args::m_sThumbXY, "Size of PNG thumb nail in pixels","VALxVAL"},
        {"to-name",'o', 0, G_OPTION_ARG_STRING, &AP_Args::m_sName, "Name of output file",nullptr},
        {"import-extension", '\0', 0, G_OPTION_ARG_STRING, &AP_Args::m_sFileExtension, "Override document type detection by specifying a file extension", nullptr},
        {"userprofile", 'u', 0, G_OPTION_ARG_STRING, &AP_Args::m_sUserProfile, "Use specified user profile.",nullptr},
        {"version", '\0', 0, G_OPTION_ARG_NONE, &AP_Args::m_iVersion, "Print Abinova version", nullptr},
        { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &AP_Args::m_sFiles, nullptr,  "[FILE...]" },
#ifdef DEBUG
#endif
        {nullptr, 0, 0, G_OPTION_ARG_NONE, nullptr, nullptr, nullptr }
};



AP_Args::AP_Args(XAP_Args * pArgs, const char * /*szAppName*/, AP_App * pApp)
	: XArgs (pArgs), 
	m_pApp(pApp)
{
	m_context = g_option_context_new ("- commandline options");
	g_option_context_add_main_entries (m_context, _entries, nullptr);
}

AP_Args::~AP_Args()
{
	// Only free ctxt if not using gnome, otherwise libgnome owns it
	g_option_context_free (m_context);
}

void AP_Args::addOptions(GOptionGroup *options)
{
	g_option_context_add_group (m_context, options);
}

#ifdef _WIN32

static inline char xdec(const char *s)
{
	int a=0;
	for (int i=0; i<2; i++) {
		if (s[i]>='0' && s[i]<='9') {
			a=(a<<4)+s[i]-'0';
		} else if (s[i]>='a' && s[i]<='f') {
			a=(a<<4)+s[i]-'a'+10;
		} else if (s[i]>='A' && s[i]<='F') {
			a=(a<<4)+s[i]-'A'+10;
		}
	}
	return a;
}

void XX_inplaceDecode(const char *s)
{
	char *d=(char*)s;
	while (*s) {
		if (*s=='%' && s[1] && s[2]) {
			*d++=xdec(s+1);
			s+=3;
		} else {
			*d++=*s++;
		}
	}
	*d=0;
}

#endif

/*! Processes all the command line options and puts them in AP_Args.
 *
 * Note that GNOME does this for us... but fails.
 */
void AP_Args::parseOptions()
{
	GError *err;
	gboolean ret;

	err = nullptr;
	ret = g_option_context_parse (m_context, &XArgs->m_argc, &XArgs->m_argv, &err);
	if (!ret || err) {
		fprintf (stderr, "%s\n", err->message);
		g_error_free (err); err = nullptr;
		return;
	}
#ifdef _WIN32
	// otherwise, decode arguments
	const char **arr;
	arr=m_sFiles;
	if (arr) while (*arr) {
		XX_inplaceDecode(*arr);
		arr++;
	}
	if (m_sMerge) XX_inplaceDecode(m_sMerge);
	if (m_impProps) XX_inplaceDecode(m_impProps);
	if (m_expProps) XX_inplaceDecode(m_expProps);
	if (m_sName) XX_inplaceDecode(m_sName);
	if (m_sFileExtension) XX_inplaceDecode(m_sFileExtension);
	if (m_sUserProfile) XX_inplaceDecode(m_sUserProfile);
#endif
}

/*!
 * Handles arguments which require an XAP_App but no windows.
 * It has a callback to getApp()::doWindowlessArgs().
 */
bool AP_Args::doWindowlessArgs(bool & bSuccessful)
{
  // start out optimistic
  bSuccessful = true;

 	if (m_iVersion)
 	{		
 		printf("%s\n", PACKAGE_VERSION);
		exit(0);
 	}

	if (m_sToFormat) 
	{
		AP_Convert * conv = new AP_Convert();
		conv->setVerbose(m_iVerbose);
		if (m_sMerge)
			conv->setMergeSource (m_sMerge);
		if (m_impProps)
			conv->setImpProps (m_impProps);
		if (m_expProps)
			conv->setExpProps (m_expProps);
		int i = 0;
		while (m_sFiles[i])
		{
			UT_DEBUGMSG(("Converting file (%s) to type (%s)\n", m_sFiles[i], m_sToFormat));
			if(m_sName)
			  bSuccessful = bSuccessful && conv->convertTo(m_sFiles[i], m_sFileExtension, m_sName, m_sToFormat);
			else
			  bSuccessful = bSuccessful && conv->convertTo(m_sFiles[i], m_sFileExtension, m_sToFormat);
			i++;
		}
		delete conv;
		return false;
	}       

	bool appWindowlessArgsWereSuccessful = true;
	bool res = m_pApp->doWindowlessArgs(this, appWindowlessArgsWereSuccessful);
	bSuccessful = bSuccessful && appWindowlessArgsWereSuccessful;
	if(!res)
		return false;

	return true;
}

