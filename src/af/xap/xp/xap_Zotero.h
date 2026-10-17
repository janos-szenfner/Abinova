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
#include <vector>

/* one hit from the local Zotero service */
struct XAP_ZoteroItem
{
	std::string	key;		/* Zotero item key, e.g. "ABCD2345" */
	std::string	title;		/* plain-text item title */
	std::string	citation;	/* formatted in-text citation, HTML stripped */
	std::string	bib;		/* formatted bibliography entry, HTML stripped */
};

/* The local Zotero REST API mirrors the web API on the loopback
 * connector port (127.0.0.1:23119, Zotero >= 7); ABINOVA_ZOTERO_HOST /
 * ABINOVA_ZOTERO_PORT retarget it (tests, BetterBibTeX proxies,
 * recorded stub servers). */

/* blocking plain-HTTP GET against the local service - short timeout,
 * safe to call on the UI thread (the service either refuses the
 * connection instantly or answers like a localhost process); false on
 * any transport/HTTP error with err naming the cause */
ABI_EXPORT bool XAP_zoteroGet(const char * path,
							  std::string & bodyOut,
							  std::string & errOut);

/* search the user's library (GET /api/users/0/items?q=...);
 * style names a CSL style for the formatted citation/bib output
 * (e.g. "apa", "chicago-author-date") - empty means the service
 * default.  out holds one entry per hit; false leaves it untouched */
ABI_EXPORT bool XAP_zoteroSearch(const std::string & query,
								 const char * style,
								 std::vector<XAP_ZoteroItem> & out,
								 std::string & err);

/* fetch one item's formatted forms by key (GET .../items/<key>) */
ABI_EXPORT bool XAP_zoteroFetchItem(const std::string & key,
									const char * style,
									XAP_ZoteroItem & out,
									std::string & err);

/* strip CSL/HTML markup + decode entities into plain text */
ABI_EXPORT std::string XAP_zoteroStripHtml(const std::string & in);
