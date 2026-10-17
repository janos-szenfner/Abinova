/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
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

#ifndef AP_DOCINSPECT_H
#define AP_DOCINSPECT_H

#include <string>
#include <vector>

#include "ut_types.h"

class PD_Document;
class FV_View;

/* Document Inspector category bitmask — which classes of identifying
 * or hidden content a scrub pass should strip */
enum AP_PrivacyMask : UT_uint32
{
	AP_PRIVACY_META			= 1 << 0,	/* document properties + personal info */
	AP_PRIVACY_ANNOTATIONS	= 1 << 1,	/* comments */
	AP_PRIVACY_HIDDEN_TEXT	= 1 << 2,	/* display:none runs */
	AP_PRIVACY_REVISIONS	= 1 << 3,	/* tracked changes */
	AP_PRIVACY_ALL			= AP_PRIVACY_META | AP_PRIVACY_ANNOTATIONS |
							  AP_PRIVACY_HIDDEN_TEXT | AP_PRIVACY_REVISIONS
};

struct AP_PrivacyCategory
{
	UT_uint32				mask;
	std::string				title;
	bool					found;
	std::vector<std::string> details;
};

struct AP_PrivacyReport
{
	std::vector<AP_PrivacyCategory> cats;	/* fixed order, one per mask bit */
};

/* Word-style Document Inspector ("Remove personal information").
 * inspect() is a pure read: it walks the meta-data map, the annotation
 * struxes and the piece table and reports what identifying or hidden
 * content the document carries.  scrub() removes the selected
 * categories under a single user-atomic glob so the strip is one undo
 * step.  Pure xp — the modal dialog and the control socket both drive
 * the same code. */
class AP_DocInspect
{
public:
	static void			inspect(PD_Document * pDoc, AP_PrivacyReport & out);
	static UT_uint32	scrub(FV_View * pView, UT_uint32 mask);
	static std::string	toText(const AP_PrivacyReport & r);
	/* "meta,comments,hidden,revisions" — empty or nullptr means ALL */
	static UT_uint32	maskFromString(const char * s);
};

#endif /* AP_DOCINSPECT_H */
