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

#ifndef ABI_OPT_WIDGET

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include "ut_debugmsg.h"
#include "ap_UnixApp.h"

int main(int argc, char ** argv)
{
#ifdef G_OS_WIN32
	/* WIN01: crash-recovery helper mode — a dying process respawns
	 * this exe with --abinova-crash-recover so the .saved recovery
	 * files and a minidump get written by a process with a healthy
	 * heap. Runs before any toolkit/console setup: the helper is
	 * headless and must not recurse into the normal app path. */
	if (argc >= 2 && g_strcmp0(argv[1], AP_WIN_CRASH_ARG) == 0)
		return AP_UnixApp::crashRecoveryHelper(argc - 2, argv + 2);
#endif
	UT_Debug_Init();
#ifdef G_OS_WIN32
	/* abinova.exe stays a console-subsystem binary: --to=pdf and friends
	   must keep working from cmd.exe, PowerShell and mintty alike (a
	   GUI-subsystem exe cannot print there, and cmd would not wait for
	   it). The price is that Windows allocates a console when the exe is
	   launched without one (Explorer, mintty); if the console was created
	   solely for this process, free it so no stray console window stays
	   behind. Piped/redirected stdio is unaffected. */
	DWORD consoleProcs[2];
	if (GetConsoleProcessList(consoleProcs, 2) == 1)
		FreeConsole();

	/* DLL-planting hardening: drop the current directory from the DLL
	   search path. Even with SafeDllSearchMode the CWD sits in the
	   standard order, so a DLL dropped next to a document (or in a
	   directory the user cd'd into) could shadow a dependency when a
	   bundled copy were missing. The application directory is always
	   searched first, so the DLLs the bundle places beside abinova.exe
	   resolve normally, and the explicit LOAD_LIBRARY_SEARCH_* flags
	   ut_abwncrypt.cpp passes to LoadLibraryEx are unaffected. */
	SetDllDirectoryW(L"");
	/* Narrow every explicit LOAD_LIBRARY_SEARCH_* load to the app dir,
	   System32 and registered user dirs — no PATH/CWD fallback. */
	SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
		LOAD_LIBRARY_SEARCH_SYSTEM32 |
		LOAD_LIBRARY_SEARCH_USER_DIRS);

	/* argv from the C runtime is in the ANSI codepage; GLib's copy is
	   UTF-8, so non-ASCII filenames on the command line survive. */
	gchar ** utf8argv = g_win32_get_command_line();
	argc = g_strv_length(utf8argv);
	int ret = AP_UnixApp::main(PACKAGE_NAME, argc, utf8argv);
	g_strfreev(utf8argv);
	return ret;
#endif
	return AP_UnixApp::main(PACKAGE_NAME, argc, argv);
}

#endif

