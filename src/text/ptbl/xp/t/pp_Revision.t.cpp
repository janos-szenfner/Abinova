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

#include <cstring>
#include <string>

#include "tf_test.h"
#include "pp_Revision.h"

#define TFSUITE "core.text.ptbl.revision"

TFTEST_MAIN("pp_RevisionAttr parser")
{
	// ---- valid grammar forms ----
	{
		PP_RevisionAttr ra("+1,-2,!3{font-family:Times New Roman}");
		TFPASS(ra.getRevisionsCount() == 3);
		TFPASS(ra.getNthRevision(0)->getId() == 1);
		TFPASS(ra.getNthRevision(0)->getType() == PP_REVISION_ADDITION);
		TFPASS(ra.getNthRevision(1)->getId() == 2);
		TFPASS(ra.getNthRevision(1)->getType() == PP_REVISION_DELETION);
		TFPASS(ra.getNthRevision(2)->getId() == 3);
		TFPASS(ra.getNthRevision(2)->getType() == PP_REVISION_FMT_CHANGE);
		const gchar * v = nullptr;
		TFPASS(ra.getNthRevision(2)->getProperty("font-family", v));
		TFPASS(v != nullptr && std::strcmp(v, "Times New Roman") == 0);
	}
	{
		// addition carrying props+attrs becomes ADDITION_AND_FMT
		// (attr names may not contain ':' — the pair grammar splits there)
		PP_RevisionAttr ra("7{font-weight:bold}{author:Bob}");
		TFPASS(ra.getRevisionsCount() == 1);
		const PP_Revision * r = ra.getNthRevision(0);
		TFPASS(r->getType() == PP_REVISION_ADDITION_AND_FMT);
		const gchar * v = nullptr;
		TFPASS(r->getProperty("font-weight", v));
		TFPASS(v != nullptr && std::strcmp(v, "bold") == 0);
		const gchar * a = nullptr;
		TFPASS(r->getAttribute("author", a));
		TFPASS(a != nullptr && std::strcmp(a, "Bob") == 0);
	}
	{
		// "-/-" means the pair is present but empty (property removal)
		PP_RevisionAttr ra("9{deadprop:-/-}");
		TFPASS(ra.getRevisionsCount() == 1);
		const gchar * v = nullptr;
		TFPASS(ra.getNthRevision(0)->getProperty("deadprop", v));
		TFPASS(v != nullptr && *v == 0);
	}
	{
		// empty props group + attrs group is legal
		PP_RevisionAttr ra("4{}{k:v}");
		TFPASS(ra.getRevisionsCount() == 1);
		TFPASS(!ra.getNthRevision(0)->hasProperties());
		TFPASS(ra.getNthRevision(0)->hasAttributes());
	}

	// ---- malformed tokens are skipped, never folded into id 0 ----
	{
		PP_RevisionAttr ra("");
		TFPASS(ra.getRevisionsCount() == 0);
	}
	{
		PP_RevisionAttr ra("+");
		TFPASS(ra.getRevisionsCount() == 0);
	}
	{
		PP_RevisionAttr ra("!x{");
		TFPASS(ra.getRevisionsCount() == 0);
	}
	{
		PP_RevisionAttr ra("}{");
		TFPASS(ra.getRevisionsCount() == 0);
	}
	{
		PP_RevisionAttr ra("5{a}{b}{c}");
		TFPASS(ra.getRevisionsCount() == 0);
	}
	{
		PP_RevisionAttr ra("abc");
		TFPASS(ra.getRevisionsCount() == 0);
	}
	{
		// non-digit garbage after a valid prefix is rejected (atol
		// used to silently parse this as id 5)
		PP_RevisionAttr ra("5a}");
		TFPASS(ra.getRevisionsCount() == 0);
	}
	{
		PP_RevisionAttr ra("5{a");
		TFPASS(ra.getRevisionsCount() == 0);
	}
	{
		// deletion may not carry a payload
		PP_RevisionAttr ra("-7{x:1}");
		TFPASS(ra.getRevisionsCount() == 0);
	}
	{
		// id overflowing UT_uint32 is rejected
		PP_RevisionAttr ra("99999999999");
		TFPASS(ra.getRevisionsCount() == 0);
	}
	{
		// bad tokens are skipped while good ones still parse
		PP_RevisionAttr ra("+1,junk,-3");
		TFPASS(ra.getRevisionsCount() == 2);
		TFPASS(ra.getNthRevision(0)->getId() == 1);
		TFPASS(ra.getNthRevision(1)->getId() == 3);
		TFPASS(ra.getNthRevision(1)->getType() == PP_REVISION_DELETION);
	}

	// ---- serialization round-trip ----
	{
		PP_RevisionAttr ra("+1,-2,!3{font-family:Arial}");
		const std::string xml = ra.getXMLstring();
		PP_RevisionAttr rb(xml.c_str());
		TFPASS(rb.getRevisionsCount() == 3);
		TFPASS(rb.getNthRevision(2)->getType() == PP_REVISION_FMT_CHANGE);
		const gchar * v = nullptr;
		TFPASS(rb.getNthRevision(2)->getProperty("font-family", v));
		TFPASS(v != nullptr && std::strcmp(v, "Arial") == 0);
	}

	// ---- last-revision index cache: mutations must invalidate ----
	{
		PP_RevisionAttr ra("+1,+3,+2");
		const PP_Revision * last = ra.getLastRevision();
		TFPASS(last != nullptr && last->getId() == 3);
		// warm the cache, then mutate
		ra.removeAllHigherOrEqualIds(3);
		last = ra.getLastRevision();
		TFPASS(last != nullptr && last->getId() == 2);
		// id change must invalidate too (used to leave a stale pointer)
		TFPASS(ra.changeRevisionId(2, 9));
		last = ra.getLastRevision();
		TFPASS(last != nullptr && last->getId() == 9);
	}
	{
		// pruneForCumulativeResult erases elements — the old pointer
		// cache dangled; the index cache must recompute
		PP_RevisionAttr ra("+1,-2");
		TFPASS(ra.getLastRevision() != nullptr); // warm cache on the deletion
		ra.pruneForCumulativeResult(nullptr);
		TFPASS(ra.getRevisionsCount() == 1);
		const PP_Revision * last = ra.getLastRevision();
		TFPASS(last != nullptr && last->getId() == 1);
		TFPASS(last->getType() == PP_REVISION_ADDITION);
	}
	{
		// empty attribute: getLastRevision may legally return null,
		// and the convenience getters must tolerate it
		PP_RevisionAttr ra;
		TFPASS(ra.getLastRevision() == nullptr);
		TFPASS(ra.getType() == PP_REVISION_FMT_CHANGE);
		const gchar * v = nullptr;
		TFPASS(!ra.hasProperty("anything", v));
	}
}
