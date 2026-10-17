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

#ifndef UT_FINDPATTERN_H
#define UT_FINDPATTERN_H

#include <glib.h>
#include <string>
#include <vector>

#include "ut_export.h"
#include "ut_types.h"

/*!
 * Search pattern kinds shared by the find/replace dialog and the
 * FV_View find machinery.  UFP_LITERAL is the classic plain-text
 * search; UFP_WILDCARD implements the wildcard syntax MS Word
 * documents ("?", "*", "[a-z]", "[!a-z]", "<"/">" word anchors,
 * "{n,m}" repeat counts, "@" for one-or-more, "()" groups and
 * "\x"/"^t"/"^n"/"^p"/"^^" escapes); UFP_REGEX hands the pattern
 * verbatim to the PCRE engine backing GRegex.
 */
enum UT_FindPatternKind : uint8_t
{
	UFP_LITERAL		= 0,
	UFP_WILDCARD	= 1,
	UFP_REGEX		= 2
};

/*! Translate a Word-style wildcard pattern into the equivalent
 *  PCRE pattern (UTF-8). */
ABI_EXPORT std::string UT_findPatternWildcardToRegex(const UT_UCS4Char * pattern);

/*! Compile a find pattern for the given kind.  bMatchCase toggles
 *  caseless matching; bWholeWord wraps the pattern in word-boundary
 *  assertions.  nullptr (with error set) on an invalid pattern. */
ABI_EXPORT GRegex * UT_findPatternCompile(UT_FindPatternKind kind,
										 const UT_UCS4Char * pattern,
										 bool bMatchCase,
										 bool bWholeWord,
										 GError ** error);

/*! Expand capture-group references in a replacement string.
 *  groups[0] is the whole match, groups[n] capture group n.
 *  "\0".."\99" insert group n, "\\" emits a literal backslash,
 *  out-of-range references expand to nothing and other backslash
 *  sequences are kept verbatim.  Everything is UTF-8. */
ABI_EXPORT std::string UT_findPatternExpandReplace(
	const std::string & replace,
	const std::vector<std::string> & groups);

#endif /* UT_FINDPATTERN_H */
