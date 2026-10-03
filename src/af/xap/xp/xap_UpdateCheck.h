/* AbiSource Application Framework
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

#pragma once

#include "ut_export.h"

#include <string>

/* outcome of a "check for updates" query */
struct XAP_UpdateInfo
{
	bool		fetched = false;	/* the server answered at all */
	bool		newer = false;		/* and its version is newer than this build */
	std::string	version;
	std::string	url;
	std::string	errorDetail;		/* why the check failed, displayable */
};

/* blocking HTTPS query against the GitHub release feed - call only
 * from a worker thread, never the UI thread */
ABI_EXPORT void XAP_updateCheckQuery(XAP_UpdateInfo & info,
									 const char * currentVersion);

/* helpers, exported for the unit tests */
ABI_EXPORT bool XAP_httpsGet(const char * host, const char * path,
							 std::string & bodyOut, std::string & errOut);
ABI_EXPORT bool XAP_parseVersionTag(const std::string & tag, int out[3]);
ABI_EXPORT std::string XAP_jsonStringValue(const std::string & json,
										   const char * key);
