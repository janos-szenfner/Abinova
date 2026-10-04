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

TFTEST_MAIN("pp_RevisionAttr mutators")
{
	// ---- ctor with property vectors ----
	{
		PP_PropertyVector attrs = {"author", "alice"};
		PP_PropertyVector props = {"font-weight", "bold"};
		PP_RevisionAttr ra(5, PP_REVISION_ADDITION, attrs, props);
		TFPASS(ra.getRevisionsCount() == 1);
		TFPASS(!ra.empty());
		const PP_Revision * r0 = ra.getNthRevision(0);
		TFPASS(r0 != nullptr);
		TFPASS(r0->getId() == 5);
		TFPASS(r0->getType() == PP_REVISION_ADDITION);
		TFPASS(ra.getHighestId() == 5);
		TFPASS(ra.getLastRevision() == r0);
		TFPASS(ra.getType() == PP_REVISION_ADDITION);
		TFPASS(ra.getType(5) == PP_REVISION_ADDITION);
		TFPASS(ra.getType(2) == PP_REVISION_FMT_CHANGE);
		TFPASS(ra.getXMLstringUpTo(4).empty());
		PP_RevisionAttr cut(ra.getXMLstringUpTo(6));
		TFPASS(cut.getRevisionsCount() == 1);
	}

	// ---- addRevision overloads ----
	{
		PP_RevisionAttr ra;
		ra.addRevision(3, PP_REVISION_ADDITION);
		TFPASS(ra.getRevisionsCount() == 1);
		ra.addRevision(3, PP_REVISION_ADDITION);
		TFPASS(ra.getRevisionsCount() == 1);

		PP_Revision r9(9, PP_REVISION_DELETION,
					   "font-family:Times", "id:99");
		ra.addRevision(&r9);
		TFPASS(ra.getRevisionsCount() == 2);
		TFPASS(ra.getHighestId() == 9);
		TFPASS(ra.getNthRevision(1)->getId() == 9);

		// same id, different type: change-of-heart handling
		PP_RevisionAttr sup("+7");
		sup.addRevision(7, PP_REVISION_DELETION);
		TFPASS(sup.getRevisionsCount() == 1);
		TFPASS(sup.getNthRevision(0)->getType() == PP_REVISION_DELETION);
		TFPASS(sup.isFragmentSuperfluous());
		// another change of heart clears the superfluous flag
		sup.addRevision(7, PP_REVISION_ADDITION);
		TFPASS(sup.empty());
		TFPASS(!sup.isFragmentSuperfluous());

		PP_RevisionAttr fmtdel("!4");
		fmtdel.addRevision(4, PP_REVISION_DELETION);
		TFPASS(fmtdel.getRevisionsCount() == 1);
		TFPASS(fmtdel.getNthRevision(0)->getType() == PP_REVISION_DELETION);

		PP_RevisionAttr delfmt("-6");
		PP_PropertyVector p = {"color", "red"};
		delfmt.addRevision(6, PP_REVISION_FMT_CHANGE, PP_NOPROPS, p);
		TFPASS(delfmt.getRevisionsCount() == 1);
		TFPASS(delfmt.getNthRevision(0)->getType() == PP_REVISION_FMT_CHANGE);

		PP_RevisionAttr addfmt("+8");
		addfmt.addRevision(8, PP_REVISION_FMT_CHANGE, PP_NOPROPS, p);
		TFPASS(addfmt.getRevisionsCount() == 1);
		TFPASS(addfmt.getNthRevision(0)->getType() == PP_REVISION_ADDITION);

		PP_RevisionAttr addandfmt("+8{color:blue}");
		addandfmt.addRevision(8, PP_REVISION_FMT_CHANGE, PP_NOPROPS, p);
		TFPASS(addandfmt.getRevisionsCount() == 1);

		// fmt change merged onto fmt change
		PP_RevisionAttr fmt2("!5{a:b}");
		fmt2.addRevision(5, PP_REVISION_FMT_CHANGE, PP_NOPROPS, p);
		TFPASS(fmt2.getRevisionsCount() == 1);
	}

	// ---- changeRevisionType / changeRevisionId ----
	{
		PP_RevisionAttr ra("+1,-2");
		TFPASS(ra.changeRevisionType(1, PP_REVISION_DELETION));
		TFPASS(ra.getNthRevision(0)->getType() == PP_REVISION_DELETION);
		TFPASS(!ra.changeRevisionType(99, PP_REVISION_ADDITION));
		TFPASS(ra.changeRevisionId(1, 10));
		TFPASS(ra.getNthRevision(0)->getId() == 10);
		TFPASS(!ra.changeRevisionId(10, 3));
		TFPASS(!ra.changeRevisionId(50, 60));
	}

	// ---- remove variants ----
	{
		PP_RevisionAttr ra("+1,-2,+3");
		ra.removeRevisionIdWithType(2, PP_REVISION_ADDITION);
		TFPASS(ra.getRevisionsCount() == 3);
		ra.removeRevisionIdWithType(2, PP_REVISION_DELETION);
		TFPASS(ra.getRevisionsCount() == 2);

		PP_RevisionAttr rb("+4,-5");
		rb.removeRevisionIdTypeless(4);
		TFPASS(rb.getRevisionsCount() == 1);
		TFPASS(rb.getNthRevision(0)->getId() == 5);

		PP_RevisionAttr rc("+1,+2");
		rc.removeRevision(rc.getNthRevision(0));
		TFPASS(rc.getRevisionsCount() == 1);
		TFPASS(rc.getNthRevision(0)->getId() == 2);

		PP_RevisionAttr rd("+1,+3,+5");
		rd.removeAllLesserOrEqualIds(3);
		TFPASS(rd.getRevisionsCount() == 1);
		TFPASS(rd.getNthRevision(0)->getId() == 5);
		rd.removeAllHigherOrEqualIds(5);
		TFPASS(rd.empty());
	}

	// ---- lookup helpers ----
	{
		PP_RevisionAttr ra("+5,+7,+9");
		const PP_Revision * special = nullptr;
		const PP_Revision * r = ra.getGreatestLesserOrEqualRevision(6, &special);
		TFPASS(r != nullptr && r->getId() == 5);
		TFPASS(special == nullptr);
		r = ra.getGreatestLesserOrEqualRevision(7, &special);
		TFPASS(r != nullptr && r->getId() == 7);
		r = ra.getGreatestLesserOrEqualRevision(2, &special);
		TFPASS(r == nullptr);
		TFPASS(special != nullptr);
		r = ra.getGreatestLesserOrEqualRevision(0, &special);
		TFPASS(r != nullptr && r->getId() == 9);

		r = ra.getLowestGreaterOrEqualRevision(6);
		TFPASS(r != nullptr && r->getId() == 7);
		r = ra.getLowestGreaterOrEqualRevision(7);
		TFPASS(r != nullptr && r->getId() == 7);
		TFPASS(ra.getLowestGreaterOrEqualRevision(10) == nullptr);
		TFPASS(ra.getLowestGreaterOrEqualRevision(0) == nullptr);

		UT_uint32 minId = 0;
		r = ra.getRevisionWithId(7, minId);
		TFPASS(r != nullptr && r->getId() == 7);
		TFPASS(minId == PD_MAX_REVISION);
		r = ra.getRevisionWithId(6, minId);
		TFPASS(r == nullptr);
		TFPASS(minId == 7);
		r = ra.getRevisionWithId(20, minId);
		TFPASS(r == nullptr);
		TFPASS(minId == PD_MAX_REVISION);
	}

	// ---- visibility ----
	{
		PP_RevisionAttr add("+5");
		TFPASS(add.isVisible(0));
		TFPASS(!add.isVisible(3));
		TFPASS(add.isVisible(5));

		PP_RevisionAttr del("-5");
		TFPASS(del.isVisible(0));
		TFPASS(!del.isVisible(3));
		TFPASS(del.isVisible(5));

		PP_RevisionAttr fmt("!5");
		TFPASS(fmt.isVisible(3));
	}

	// ---- hasProperty / attribute lookups ----
	{
		PP_RevisionAttr ra("!3{font-weight:bold}{author:bob},+7");
		const gchar * v = nullptr;
		// the no-id form consults the last revision only; +7 has no props
		TFPASS(!ra.hasProperty("font-weight", v));
		v = nullptr;
		TFPASS(ra.hasProperty(3, "font-weight", v));
		TFPASS(v && std::strcmp(v, "bold") == 0);
		TFPASS(!ra.hasProperty(3, "color", v));
		TFPASS(!ra.hasProperty(1, "font-weight", v));
		TFPASS(ra.getHighestRevisionNumberWithAttribute("author") == 3);
		TFPASS(ra.getHighestRevisionNumberWithAttribute("no-such") == 0);
		TFPASS(ra.getType(3) == PP_REVISION_FMT_CHANGE);
		TFPASS(ra.getType(8) == PP_REVISION_ADDITION);

		// the no-id form consults the highest-id revision, so the
		// fmt change has to be the newest entry for it to see props
		PP_RevisionAttr tailfmt("+1,!7{font-weight:bold}");
		v = nullptr;
		TFPASS(tailfmt.hasProperty("font-weight", v));
		TFPASS(v && std::strcmp(v, "bold") == 0);
	}

	// ---- getLowestDeletionRevision ----
	{
		PP_RevisionAttr alldel("-1,-2,-3");
		const PP_Revision * l = alldel.getLowestDeletionRevision();
		TFPASS(l != nullptr && l->getId() == 1);

		PP_RevisionAttr suffix("+1,-2,-3");
		l = suffix.getLowestDeletionRevision();
		TFPASS(l != nullptr && l->getId() == 2);

		PP_RevisionAttr notail("-1,+2");
		TFPASS(notail.getLowestDeletionRevision() == nullptr);

		PP_RevisionAttr empty;
		TFPASS(empty.getLowestDeletionRevision() == nullptr);
	}

	// ---- equality / setRevision / forceDirty ----
	{
		PP_RevisionAttr ra("+4");
		PP_RevisionAttr rb("+4");
		PP_RevisionAttr rc("+5");
		TFPASS(ra == rb);
		TFPASS(!(ra == rc));
		// multi-revision attrs never compare equal (all-pairs check)
		PP_RevisionAttr m1("+1,-2");
		PP_RevisionAttr m2("+1,-2");
		TFPASS(!(m1 == m2));

		rb.setRevision("-9");
		TFPASS(rb.getRevisionsCount() == 1);
		TFPASS(rb.getNthRevision(0)->getType() == PP_REVISION_DELETION);
		rb.setRevision(std::string("+11"));
		TFPASS(rb.getNthRevision(0)->getId() == 11);

		rb.forceDirty();
		TFPASS(!rb.getXMLstring().empty());
	}

	// ---- merge paths ----
	{
		PP_RevisionAttr ra("+1");
		ra.mergeAttr(2, PP_REVISION_ADDITION, "key", "val");
		TFPASS(ra.getRevisionsCount() == 2);
		UT_uint32 minId = 0;
		const PP_Revision * r2 = ra.getRevisionWithId(2, minId);
		TFPASS(r2 != nullptr);
		const gchar * av = nullptr;
		TFPASS(r2->getAttribute("key", av));
		TFPASS(av && std::strcmp(av, "val") == 0);

		// already-present attribute is not overwritten; a parsed
		// addition carrying attrs/props is ADDITION_AND_FMT internally
		ra.mergeAttrIfNotAlreadyThere(2, PP_REVISION_ADDITION_AND_FMT, "key", "other");
		r2 = ra.getRevisionWithId(2, minId);
		av = nullptr;
		TFPASS(r2 && r2->getAttribute("key", av));
		TFPASS(av && std::strcmp(av, "val") == 0);

		// NONE matches any stored type
		ra.mergeAttrIfNotAlreadyThere(2, PP_REVISION_NONE, "key", "other2");
		r2 = ra.getRevisionWithId(2, minId);
		av = nullptr;
		TFPASS(r2 && r2->getAttribute("key", av));
		TFPASS(av && std::strcmp(av, "val") == 0);

		// new attribute on the same id/type merges in
		ra.mergeAttrIfNotAlreadyThere(2, PP_REVISION_ADDITION_AND_FMT, "key2", "v2");
		r2 = ra.getRevisionWithId(2, minId);
		av = nullptr;
		TFPASS(r2 && r2->getAttribute("key2", av));

		PP_RevisionAttr other("+3{font-style:italic}");
		ra.mergeAll(other);
		TFPASS(ra.getRevisionsCount() == 3);
	}
}

TFTEST_MAIN("pp_Revision")
{
	// ---- string-argument ctor, strings, toString ----
	{
		PP_Revision r(7, PP_REVISION_ADDITION,
					  "font-weight:bold", "author:bob");
		TFPASS(r.getId() == 7);
		TFPASS(r.getType() == PP_REVISION_ADDITION);
		TFPASS(r.getPropsString() != nullptr);
		TFPASS(std::strcmp(r.getPropsString(), "font-weight:bold") == 0);
		TFPASS(r.getAttrsString() != nullptr);
		TFPASS(std::strcmp(r.getAttrsString(), "author:bob") == 0);
		TFPASS(r.toString() == "7{font-weight:bold}{author:bob}");

		PP_Revision del(3, PP_REVISION_DELETION, "", "");
		TFPASS(del.toString() == "-3");

		PP_Revision fmt(4, PP_REVISION_FMT_CHANGE, "color:red", "");
		TFPASS(fmt.toString() == "!4{color:red}");
	}

	// ---- vector ctor + mutation ----
	{
		PP_PropertyVector props = {"font-family", "Times"};
		PP_Revision r(2, PP_REVISION_DELETION, props, PP_NOPROPS);
		TFPASS(r.getId() == 2);
		TFPASS(r.getType() == PP_REVISION_DELETION);
		r.setId(8);
		TFPASS(r.getId() == 8);
		r.setType(PP_REVISION_ADDITION);
		TFPASS(r.getType() == PP_REVISION_ADDITION);

		std::vector<std::string> atts = {"k1", "v1", "k2", "v2"};
		TFPASS(r.setAttributes(atts));
		const gchar * av = nullptr;
		TFPASS(r.getAttribute("k2", av));
		TFPASS(av && std::strcmp(av, "v2") == 0);
	}

	// ---- equality ----
	{
		PP_Revision a(1, PP_REVISION_ADDITION, "x:y", "p:q");
		PP_Revision b(1, PP_REVISION_ADDITION, "x:y", "p:q");
		PP_Revision c(2, PP_REVISION_ADDITION, "x:y", "p:q");
		PP_Revision d(1, PP_REVISION_DELETION, "x:y", "p:q");
		PP_Revision e(1, PP_REVISION_ADDITION, "x:z", "p:q");
		PP_Revision f(1, PP_REVISION_ADDITION, "x:y", "p:r");
		TFPASS(a == b);
		TFPASS(!(a == c));
		TFPASS(!(a == d));
		TFPASS(!(a == e));
		TFPASS(!(a == f));
	}

	// ---- abi-para-only attribute detection ----
	{
		PP_Revision clean(1, PP_REVISION_FMT_CHANGE, nullptr, nullptr);
		TFPASS(!clean.onlyContainsAbiwordChangeTrackingMarkup());

		std::vector<std::string> atts = {"abi-para-list", "1"};
		PP_Revision abi(1, PP_REVISION_FMT_CHANGE, nullptr, nullptr);
		TFPASS(abi.setAttributes(atts));
		TFPASS(abi.onlyContainsAbiwordChangeTrackingMarkup());

		std::vector<std::string> mixed = {"abi-para-x", "1", "other", "2"};
		PP_Revision m(1, PP_REVISION_FMT_CHANGE, nullptr, nullptr);
		TFPASS(m.setAttributes(mixed));
		TFPASS(!m.onlyContainsAbiwordChangeTrackingMarkup());
	}
}

TFTEST_MAIN("pp_Revision UT_get helpers")
{
	PP_AttrProp ap;
	ap.setAttribute("plain", "pval");
	ap.setAttribute("revision", "+5{}{revattr:rval}");
	TFPASS(UT_getAttribute(&ap, "plain", nullptr) != nullptr);
	TFPASS(std::strcmp(UT_getAttribute(&ap, "plain", nullptr), "pval") == 0);
	TFPASS(UT_getAttribute(&ap, "missing", "dflt") != nullptr);
	TFPASS(std::strcmp(UT_getAttribute(&ap, "missing", "dflt"), "dflt") == 0);

	// latest value prefers the revision attribute over the plain one
	TFPASS(UT_getLatestAttribute(&ap, "revattr", "d") == "rval");
	TFPASS(UT_getLatestAttribute(&ap, "plain", "d") == "pval");
	TFPASS(UT_getLatestAttribute(&ap, "missing", "d") == "d");

	PP_AttrProp norev;
	norev.setAttribute("k", "v");
	TFPASS(UT_getLatestAttribute(&norev, "k", "d") == "v");
}
