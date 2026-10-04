/* AbiWord
 * Copyright (C) 2005 Hubert Figuiere
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



#include <stdio.h>
#include <string.h>
#include "tf_test.h"

#include "ut_locale.h"

#define TFSUITE "core.af.util.locale"

// On the uselocale() implementation the process-wide locale is not touched;
// on the setlocale() implementations (Windows / fallback) it is.
#if defined(G_OS_WIN32) || defined(UT_NO_USELOCALE)
#define UT_LOCALE_PROCESS_WIDE 1
#else
#define UT_LOCALE_PROCESS_WIDE 0
#endif

TFTEST_MAIN("UT_LocaleTransactor")
{
	char msg[128];
	// A comma-decimal locale that is reasonably common on dev boxes;
	// fall back through a couple of candidates.
	const char * loc = nullptr;
	for (const char * cand : { "hu_HU.utf8", "en_DK.utf8", "fr_FR", "de_DE.UTF-8" })
	{
		if (setlocale(LC_ALL, cand) != nullptr)
		{
			loc = cand;
			break;
		}
	}
	if (loc == nullptr)
	{
		printf("Test skipped, no comma-decimal locale on this system\n");
		return;
	}

	sprintf(msg, "%f", 1.0f);
	TFPASS(strstr(msg, "1,0") == msg);

	TFPASS(strcmp(setlocale(LC_NUMERIC, nullptr), loc) == 0);

	{
		UT_LocaleTransactor t(LC_NUMERIC, "C");
		sprintf(msg, "%f", 1.0f);
		TFPASS(strstr(msg, "1.0") == msg);
#if UT_LOCALE_PROCESS_WIDE
		TFPASS(strcmp(setlocale(LC_NUMERIC, nullptr), "C") == 0);
#else
		// uselocale() path: the effective locale is C but the process-wide
		// locale must be left alone for other threads.
		TFPASS(strcmp(setlocale(LC_NUMERIC, nullptr), loc) == 0);
#endif

		// nested transactors restore in order
		{
			UT_LocaleTransactor t2(LC_NUMERIC, "C");
			sprintf(msg, "%f", 1.0f);
			TFPASS(strstr(msg, "1.0") == msg);
		}
		sprintf(msg, "%f", 1.0f);
		TFPASS(strstr(msg, "1.0") == msg);
	}
	TFPASS(strcmp(setlocale(LC_NUMERIC, nullptr), loc) == 0);
	sprintf(msg, "%f", 1.0f);
	TFPASS(strstr(msg, "1,0") == msg);

	// restore the process locale for the rest of the suite
	setlocale(LC_ALL, "C");
}
