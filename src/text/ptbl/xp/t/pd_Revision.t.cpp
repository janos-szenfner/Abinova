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

/* Document-level tests for the piece-table revision model: fragment
 * marks stamped under marking (+n/-n/!n{...}), AD_Revision
 * author/time records, accept/reject ops, show-level visibility and
 * strux-level marks.  The attr-string grammar itself is covered by
 * pp_Revision.t.cpp. */

#include <cstring>
#include <string>
#include <vector>

#include "tf_test.h"

#include "pd_Document.h"
#include "pd_Iterator.h"
#include "pf_Frag.h"
#include "pf_Frag_Strux.h"
#include "pp_Revision.h"
#include "pt_Types.h"
#include "xad_Document.h"

#define TFSUITE "core.text.ptbl.revisiondoc"

namespace {

/* A minimal document built through the raw-creation path — no file,
 * no template, no layout.  Content: two paragraphs
 *   "base text " / "second para "
 */
struct RevDoc
{
	RevDoc() = default;
	RevDoc(const RevDoc &) = delete;
	RevDoc &operator=(const RevDoc &) = delete;

	bool build()
	{
		doc = new PD_Document;
		if (doc->createRawDocument() != UT_OK)
			return false;

		const UT_UCS4String p1("base text ");
		const UT_UCS4String p2("second para ");

		if (!doc->appendStrux(PTX_Section, PP_NOPROPS, &sdhSection))
			return false;
		if (!doc->appendStrux(PTX_Block, PP_NOPROPS, &sdhBlock1))
			return false;
		if (!doc->appendSpan(p1.ucs4_str(), p1.length()))
			return false;
		if (!doc->appendStrux(PTX_Block, PP_NOPROPS, &sdhBlock2))
			return false;
		if (!doc->appendSpan(p2.ucs4_str(), p2.length()))
			return false;

		doc->finishRawCreation();
		return true;
	}

	~RevDoc()
	{
		if (doc)
			doc->unref();
	}

	PT_DocPosition posBlock1() const
		{ return doc->getStruxPosition(sdhBlock1); }
	PT_DocPosition posBlock2() const
		{ return doc->getStruxPosition(sdhBlock2); }

	PD_Document *doc = nullptr;
	pf_Frag_Strux *sdhSection = nullptr;
	pf_Frag_Strux *sdhBlock1 = nullptr;
	pf_Frag_Strux *sdhBlock2 = nullptr;
};

/* raw "revision" attribute of the frag at a doc position */
std::string fragRevAttr(PD_Document *doc, PT_DocPosition pos)
{
	const pf_Frag *pf = doc->getFragFromPosition(pos);
	if (!pf)
		return {};

	const PP_AttrProp *pAP = nullptr;
	if (!doc->getAttrProp(pf->getIndexAP(), &pAP) || !pAP)
		return {};

	const gchar *v = nullptr;
	if (!pAP->getAttribute(PT_REVISION_ATTRIBUTE_NAME, v) || !v)
		return {};
	return v;
}

/* does the frag at pos carry a revision mark of the given id+type? */
bool fragHasRev(PD_Document *doc, PT_DocPosition pos,
				UT_uint32 id, PP_RevisionType want)
{
	const std::string ra = fragRevAttr(doc, pos);
	if (ra.empty())
		return false;
	PP_RevisionAttr revs(ra);
	UT_uint32 minId = 0;
	const PP_Revision *r = revs.getRevisionWithId(id, minId);
	return r && r->getType() == want;
}

/* exploded span AP at position for the given show settings */
const PP_AttrProp * spanAP(PD_Document *doc, PT_DocPosition pos,
						   bool bShow, UT_uint32 iId, bool &bHidden)
{
	const pf_Frag *pf = doc->getFragFromPosition(pos);
	if (!pf)
		return nullptr;
	const PP_AttrProp *pAP = nullptr;
	std::optional<std::unique_ptr<PP_RevisionAttr>> revs;
	bHidden = false;
	if (!doc->getAttrProp(pf->getIndexAP(), &pAP, revs, bShow, iId,
						  bHidden))
		return nullptr;
	return pAP;
}

/* printable text content of the doc; strux/object positions report
 * UT_IT_NOT_CHARACTER (== UCS_SPACE) so gate on the frag type */
std::string docText(PD_Document *doc)
{
	std::string out;
	PD_DocIterator t(*doc);
	while (t.getStatus() == UTIter_OK)
	{
		const pf_Frag *pf = t.getFrag();
		if (pf && pf->getType() == pf_Frag::PFT_Text)
		{
			const UT_UCS4Char c = t.getChar();
			if (c >= 32 && c < 0x7f)
				out += static_cast<char>(c);
		}
		++t;
	}
	return out;
}

} // namespace

TFTEST_MAIN("AD_Revision table records id, author, time, version")
{
	RevDoc d;
	TFPASS(d.build());

	/* a fresh document has no revision records */
	TFPASS(d.doc->getRevisions().empty());
	TFPASS(d.doc->getHighestRevisionId() == 0);
	TFPASS(d.doc->getRevisionIndxFromId(3) < 0);

	const UT_UCS4String desc1("inserted paragraph");
	const UT_UCS4String desc2("deleted text");
	const time_t t1 = 1700000000;
	const time_t t2 = 1700000200;

	TFPASS(d.doc->addRevision(5, desc1.ucs4_str(), t1, 0, true, "Alice"));
	TFPASS(d.doc->addRevision(2, desc2.ucs4_str(), t2, 3, true, "Bob"));

	/* duplicate ids are rejected */
	TFPASS(!d.doc->addRevision(5, desc1.ucs4_str(), t1, 0, true, "Carol"));

	TFPASS(d.doc->getRevisions().size() == 2);
	const std::vector<AD_Revision> &revs = d.doc->getRevisions();
	TFPASS(revs[0].getId() == 5);
	TFPASS(revs[0].getAuthor() == "Alice");
	TFPASS(revs[0].getStartTime() == t1);
	TFPASS(revs[0].getVersion() == 0);
	TFPASS(revs[0].getDescription() != nullptr);
	TFPASS(UT_UCS4String(revs[0].getDescription()) == desc1);

	TFPASS(revs[1].getId() == 2);
	TFPASS(revs[1].getAuthor() == "Bob");
	TFPASS(revs[1].getStartTime() == t2);
	TFPASS(revs[1].getVersion() == 3);

	/* lookup helpers */
	TFPASS(d.doc->getHighestRevisionId() == 5);
	TFPASS(d.doc->getHighestRevision() != nullptr);
	TFPASS(d.doc->getHighestRevision()->getAuthor() == "Alice");
	TFPASS(d.doc->getRevisionIndxFromId(2) == 1);
	TFPASS(d.doc->getRevisionIndxFromId(77) < 0);
	TFPASS(d.doc->findAutoRevisionId(3) == 2);
	TFPASS(d.doc->findAutoRevisionId(999) == 0);
	/* version 0 (< 2) belongs to revision 5 — the nearest lesser */
	TFPASS(d.doc->findNearestAutoRevisionId(2, true) == 5);
	TFPASS(d.doc->findNearestAutoRevisionId(2, false) == 2);
	TFPASS(d.doc->findNearestAutoRevisionId(4, true) == 2);

	/* the "no author" case yields an empty string, not nullptr */
	{
		RevDoc d2;
		TFPASS(d2.build());
		TFPASS(d2.doc->addRevision(1, nullptr, 0, 0, true, nullptr));
		TFPASS(d2.doc->getRevisions().front().getAuthor().empty());
		TFPASS(d2.doc->getRevisions().front().getStartTime() == 0);
		TFPASS(d2.doc->getRevisions().front().getDescription() != nullptr);
		TFPASS(*d2.doc->getRevisions().front().getDescription() == 0);
	}

	/* marking/show state setters */
	TFPASS(!d.doc->isMarkRevisions());
	d.doc->setMarkRevisions(true);
	TFPASS(d.doc->isMarkRevisions());
	d.doc->toggleMarkRevisions();
	TFPASS(!d.doc->isMarkRevisions());

	d.doc->setRevisionId(9);
	TFPASS(d.doc->getRevisionId() == 9);
	d.doc->setShowRevisionId(4);
	TFPASS(d.doc->getShowRevisionId() == 4);
	d.doc->setShowRevisions(false);
	TFPASS(!d.doc->isShowRevisions());
	d.doc->toggleShowRevisions();
	TFPASS(d.doc->isShowRevisions());
}

TFTEST_MAIN("marking stamps +n/-n/!n marks on fragments")
{
	RevDoc d;
	TFPASS(d.build());

	const PT_DocPosition posBase = d.posBlock1() + 1;

	/* baseline content, no marks */
	TFPASS(docText(d.doc) == "base text second para ");
	TFPASS(fragRevAttr(d.doc, posBase).empty());

	/* turn marking on — the current revision id must land in the
	 * table with an author record so the marks carry provenance */
	d.doc->setMarkRevisions(true);
	TFPASS(d.doc->isMarkRevisions());
	const UT_uint32 revId = d.doc->getRevisionId();
	TFPASS(d.doc->getRevisionIndxFromId(revId) >= 0);
	TFPASS(!d.doc->getRevisions().back().getAuthor().empty());

	/* marked insertion: "+id" on the new text frag, existing text
	 * keeps its clean AP */
	const UT_UCS4String ins("NEW");
	TFPASS(d.doc->insertSpan(posBase, ins.ucs4_str(), ins.length()));
	TFPASS(fragHasRev(d.doc, posBase, revId, PP_REVISION_ADDITION));
	TFPASS(fragHasRev(d.doc, posBase + 1, revId, PP_REVISION_ADDITION));
	TFPASS(fragRevAttr(d.doc, posBase + ins.length()).empty());
	TFPASS(docText(d.doc) == "NEWbase text second para ");

	/* marked deletion: the span stays in the document with a "-id"
	 * mark — nothing is physically removed */
	UT_uint32 iDeleted = 0;
	TFPASS(d.doc->deleteSpan(posBase + 4, posBase + 7, nullptr, iDeleted));
	TFPASS(fragHasRev(d.doc, posBase + 4, revId, PP_REVISION_DELETION));
	TFPASS(fragHasRev(d.doc, posBase + 6, revId, PP_REVISION_DELETION));
	/* deleted-marked text is still in the piece table */
	TFPASS(docText(d.doc) == "NEWbase text second para ");

	/* marked format change: the span keeps its original props and
	 * gains "!id{new-props}" */
	const PP_PropertyVector boldProp = { "font-weight", "bold" };
	TFPASS(d.doc->changeSpanFmt(PTC_AddFmt, posBase + 8, posBase + 10,
								PP_NOPROPS, boldProp));
	{
		const std::string ra = fragRevAttr(d.doc, posBase + 8);
		PP_RevisionAttr revs(ra);
		UT_uint32 minId = 0;
		const PP_Revision *r = revs.getRevisionWithId(revId, minId);
		TFPASS(r != nullptr);
		TFPASS(r->getType() == PP_REVISION_FMT_CHANGE);
		const gchar *v = nullptr;
		TFPASS(r->getProperty("font-weight", v));
		TFPASS(v != nullptr && std::strcmp(v, "bold") == 0);
		/* the raw AP still carries the OLD formatting — the new
		 * props live inside the mark only */
		const pf_Frag *pf = d.doc->getFragFromPosition(posBase + 8);
		const PP_AttrProp *pAP = nullptr;
		TFPASS(d.doc->getAttrProp(pf->getIndexAP(), &pAP));
		const gchar *prop = nullptr;
		TFPASS(!pAP->getProperty("font-weight", prop) || prop == nullptr);
	}

	/* a strux inserted under marking gets "+id" too — the strux-level
	 * mark that OXML cell/row insertion relies on */
	pf_Frag_Strux *sdhNew = nullptr;
	TFPASS(d.doc->insertStrux(posBase + 10, PTX_Block,
							  PP_NOPROPS, PP_NOPROPS, &sdhNew));
	TFPASS(sdhNew != nullptr);
	{
		const PP_AttrProp *pAP = nullptr;
		TFPASS(d.doc->getAttrProp(d.doc->getAPIFromStrux(sdhNew), &pAP));
		const gchar *v = nullptr;
		TFPASS(pAP->getAttribute(PT_REVISION_ATTRIBUTE_NAME, v));
		PP_RevisionAttr revs(v);
		UT_uint32 minId = 0;
		const PP_Revision *r = revs.getRevisionWithId(revId, minId);
		TFPASS(r != nullptr && r->getType() == PP_REVISION_ADDITION);
	}
}

TFTEST_MAIN("show level controls mark visibility")
{
	RevDoc d;
	TFPASS(d.build());

	const PT_DocPosition posBase = d.posBlock1() + 1;

	/* three distinct marks on three spans */
	d.doc->setMarkRevisions(true);
	const UT_UCS4String ins("NEW");
	TFPASS(d.doc->insertSpan(posBase, ins.ucs4_str(), ins.length()));
	const UT_uint32 idIns = d.doc->getRevisionId();       // +1

	TFPASS(d.doc->addRevision(2, nullptr, 1700000000, 0, true, "Bob"));
	UT_uint32 n = 0;
	TFPASS(d.doc->deleteSpan(posBase + 4, posBase + 7, nullptr, n)); // -2 on "ase"

	TFPASS(d.doc->addRevision(3, nullptr, 1700000100, 0, true, "Carol"));
	const PP_PropertyVector boldProp = { "font-weight", "bold" };
	TFPASS(d.doc->changeSpanFmt(PTC_AddFmt, posBase + 8, posBase + 10,
								PP_NOPROPS, boldProp));   // !3{bold}

	/* ---- marking ON, show off, level = idIns: "document as of
	 * revision 1" — the -2 deletion has not happened yet ---- */
	{
		bool bHidden = true;
		spanAP(d.doc, posBase + 5, false, idIns, bHidden);
		TFPASS(!bHidden);                    // deletion not yet applied
	}

	/* ---- marking ON, level MAX: all marks applied — the deletion
	 * hides its text, the fmt change applies ---- */
	{
		bool bHidden = false;
		spanAP(d.doc, posBase + 5, false, PD_MAX_REVISION, bHidden);
		TFPASS(bHidden);                     // -2 applied

		bHidden = true;
		const PP_AttrProp *pAP =
			spanAP(d.doc, posBase + 8, false, PD_MAX_REVISION, bHidden);
		TFPASS(pAP != nullptr);
		const gchar *v = nullptr;
		TFPASS(pAP->getProperty("font-weight", v));
		TFPASS(v != nullptr && std::strcmp(v, "bold") == 0);
	}

	d.doc->setMarkRevisions(false);

	/* ---- document as before ALL revisions (marking off, show off,
	 * level 0): additions hidden, deletions shown, fmt changes not
	 * applied ---- */
	bool bHidden = true;
	const PP_AttrProp *pAP = spanAP(d.doc, posBase, false, 0, bHidden);
	TFPASS(pAP != nullptr);
	TFPASS(bHidden);                                     // +1 hidden

	bHidden = true;
	pAP = spanAP(d.doc, posBase + 5, false, 0, bHidden);
	TFPASS(pAP != nullptr);
	TFPASS(!bHidden);                                    // -2 shown

	bHidden = true;
	pAP = spanAP(d.doc, posBase + 8, false, 0, bHidden);
	TFPASS(pAP != nullptr);
	TFPASS(!bHidden);
	const gchar *v = nullptr;
	TFPASS(!pAP->getProperty("font-weight", v) || v == nullptr); // !3 not applied

	/* ---- document as after ALL revisions (marking off, level MAX):
	 * addition visible, deletion hidden, fmt applied ---- */
	bHidden = true;
	spanAP(d.doc, posBase, false, PD_MAX_REVISION, bHidden);
	TFPASS(!bHidden);                                    // +1 visible

	bHidden = false;
	pAP = spanAP(d.doc, posBase + 5, false, PD_MAX_REVISION, bHidden);
	TFPASS(bHidden);                                     // -2 applied

	bHidden = true;
	pAP = spanAP(d.doc, posBase + 8, false, PD_MAX_REVISION, bHidden);
	TFPASS(pAP != nullptr);
	v = nullptr;
	TFPASS(pAP->getProperty("font-weight", v));
	TFPASS(v != nullptr && std::strcmp(v, "bold") == 0); // !3 applied

	/* ---- revisions shown (marking off, level 0 = all): everything
	 * visible and the mark survives in the exploded AP so the view
	 * can decorate ---- */
	bHidden = true;
	pAP = spanAP(d.doc, posBase, true, 0, bHidden);
	TFPASS(pAP != nullptr);
	TFPASS(!bHidden);
	v = nullptr;
	TFPASS(pAP->getAttribute(PT_REVISION_ATTRIBUTE_NAME, v));
	TFPASS(v != nullptr && *v != 0);                     // mark retained

	bHidden = true;
	spanAP(d.doc, posBase + 5, true, 0, bHidden);
	TFPASS(!bHidden);                                    // deletion shown stricken

	/* fmt changes apply while shown so the change is visible */
	bHidden = true;
	pAP = spanAP(d.doc, posBase + 8, true, 0, bHidden);
	TFPASS(pAP != nullptr);
	v = nullptr;
	TFPASS(pAP->getProperty("font-weight", v));
	TFPASS(v != nullptr && std::strcmp(v, "bold") == 0);

	/* strux-level: the block strux inserted under marking is hidden
	 * from a pre-revision view */
	{
		RevDoc d2;
		TFPASS(d2.build());
		d2.doc->setMarkRevisions(true);
		const PT_DocPosition pos2 = d2.posBlock1() + 1;
		pf_Frag_Strux *sdhNew = nullptr;
		TFPASS(d2.doc->insertStrux(pos2, PTX_Block,
								   PP_NOPROPS, PP_NOPROPS, &sdhNew));
		d2.doc->setMarkRevisions(false);

		const PP_AttrProp *sAP = nullptr;
		bool bHid = false;
		std::optional<std::unique_ptr<PP_RevisionAttr>> revs;
		TFPASS(d2.doc->getAttrProp(d2.doc->getAPIFromStrux(sdhNew),
								   &sAP, revs, false, 0, bHid));
		TFPASS(bHid);                // added strux hidden pre-revision

		bHid = true;
		TFPASS(d2.doc->getAttrProp(d2.doc->getAPIFromStrux(sdhNew),
								   &sAP, revs, false, PD_MAX_REVISION,
								   bHid));
		TFPASS(!bHid);               // visible once accepted state
	}
}

TFTEST_MAIN("accept/reject a single revision")
{
	/* ---- accept an addition: text stays, mark gone ---- */
	{
		RevDoc d;
		TFPASS(d.build());
		const PT_DocPosition posBase = d.posBlock1() + 1;
		d.doc->setMarkRevisions(true);
		const UT_UCS4String ins("NEW");
		TFPASS(d.doc->insertSpan(posBase, ins.ucs4_str(), ins.length()));
		const UT_uint32 id = d.doc->getRevisionId();
		d.doc->setMarkRevisions(false);

		TFPASS(d.doc->acceptRejectRevision(false, posBase,
										 posBase + 3, id));
		TFPASS(fragRevAttr(d.doc, posBase).empty());
		TFPASS(docText(d.doc) == "NEWbase text second para ");
	}

	/* ---- reject an addition: text physically removed ---- */
	{
		RevDoc d;
		TFPASS(d.build());
		const PT_DocPosition posBase = d.posBlock1() + 1;
		d.doc->setMarkRevisions(true);
		const UT_UCS4String ins("NEW");
		TFPASS(d.doc->insertSpan(posBase, ins.ucs4_str(), ins.length()));
		const UT_uint32 id = d.doc->getRevisionId();
		d.doc->setMarkRevisions(false);

		TFPASS(d.doc->acceptRejectRevision(true, posBase,
										 posBase + 3, id));
		TFPASS(docText(d.doc) == "base text second para ");
		TFPASS(fragRevAttr(d.doc, posBase).empty());
	}

	/* ---- accept a deletion: text physically removed ---- */
	{
		RevDoc d;
		TFPASS(d.build());
		const PT_DocPosition posBase = d.posBlock1() + 1;
		d.doc->setMarkRevisions(true);
		UT_uint32 n = 0;
		TFPASS(d.doc->deleteSpan(posBase, posBase + 4, nullptr, n));
		const UT_uint32 id = d.doc->getRevisionId();
		d.doc->setMarkRevisions(false);
		TFPASS(docText(d.doc) == "base text second para ");

		TFPASS(d.doc->acceptRejectRevision(false, posBase,
										 posBase + 4, id));
		TFPASS(docText(d.doc) == " text second para ");
	}

	/* ---- reject a deletion: mark dropped, text restored ---- */
	{
		RevDoc d;
		TFPASS(d.build());
		const PT_DocPosition posBase = d.posBlock1() + 1;
		d.doc->setMarkRevisions(true);
		UT_uint32 n = 0;
		TFPASS(d.doc->deleteSpan(posBase, posBase + 4, nullptr, n));
		const UT_uint32 id = d.doc->getRevisionId();
		d.doc->setMarkRevisions(false);

		TFPASS(d.doc->acceptRejectRevision(true, posBase,
										 posBase + 4, id));
		TFPASS(docText(d.doc) == "base text second para ");
		TFPASS(fragRevAttr(d.doc, posBase).empty());
	}

	/* ---- accept a fmt change: new props land, mark gone ---- */
	{
		RevDoc d;
		TFPASS(d.build());
		const PT_DocPosition posBase = d.posBlock1() + 1;
		d.doc->setMarkRevisions(true);
		const PP_PropertyVector boldProp = { "font-weight", "bold" };
		TFPASS(d.doc->changeSpanFmt(PTC_AddFmt, posBase, posBase + 3,
									PP_NOPROPS, boldProp));
		const UT_uint32 id = d.doc->getRevisionId();
		d.doc->setMarkRevisions(false);

		TFPASS(d.doc->acceptRejectRevision(false, posBase,
										 posBase + 3, id));
		TFPASS(fragRevAttr(d.doc, posBase).empty());
		const pf_Frag *pf = d.doc->getFragFromPosition(posBase);
		const PP_AttrProp *pAP = nullptr;
		TFPASS(d.doc->getAttrProp(pf->getIndexAP(), &pAP));
		const gchar *v = nullptr;
		TFPASS(pAP->getProperty("font-weight", v));
		TFPASS(v != nullptr && std::strcmp(v, "bold") == 0);
	}

	/* ---- reject a fmt change: mark gone, old props kept ---- */
	{
		RevDoc d;
		TFPASS(d.build());
		const PT_DocPosition posBase = d.posBlock1() + 1;
		d.doc->setMarkRevisions(true);
		const PP_PropertyVector boldProp = { "font-weight", "bold" };
		TFPASS(d.doc->changeSpanFmt(PTC_AddFmt, posBase, posBase + 3,
									PP_NOPROPS, boldProp));
		const UT_uint32 id = d.doc->getRevisionId();
		d.doc->setMarkRevisions(false);

		TFPASS(d.doc->acceptRejectRevision(true, posBase,
										 posBase + 3, id));
		TFPASS(fragRevAttr(d.doc, posBase).empty());
		const pf_Frag *pf = d.doc->getFragFromPosition(posBase);
		const PP_AttrProp *pAP = nullptr;
		TFPASS(d.doc->getAttrProp(pf->getIndexAP(), &pAP));
		const gchar *v = nullptr;
		TFPASS(!pAP->getProperty("font-weight", v) || v == nullptr);
	}
}

TFTEST_MAIN("acceptAllRevisions and rejectAllHigherRevisions")
{
	/* acceptAll: added text stays, deleted text goes, fmt applied */
	{
		RevDoc d;
		TFPASS(d.build());
		const PT_DocPosition posBase = d.posBlock1() + 1;
		d.doc->setMarkRevisions(true);
		const UT_UCS4String ins("NEW");
		TFPASS(d.doc->insertSpan(posBase, ins.ucs4_str(), ins.length()));
		UT_uint32 n = 0;
		TFPASS(d.doc->deleteSpan(posBase + 4, posBase + 7, nullptr, n));
		const PP_PropertyVector boldProp = { "font-weight", "bold" };
		TFPASS(d.doc->changeSpanFmt(PTC_AddFmt, posBase + 8,
									posBase + 10, PP_NOPROPS, boldProp));
		d.doc->setMarkRevisions(false);

		TFPASS(d.doc->acceptAllRevisions());
		TFPASS(docText(d.doc) == "NEWb text second para ");
		TFPASS(fragRevAttr(d.doc, posBase).empty());
		TFPASS(fragRevAttr(d.doc, posBase + 5).empty());
		/* fmt change applied for real */
		const pf_Frag *pf = d.doc->getFragFromPosition(posBase + 5);
		const PP_AttrProp *pAP = nullptr;
		TFPASS(d.doc->getAttrProp(pf->getIndexAP(), &pAP));
		const gchar *v = nullptr;
		TFPASS(pAP->getProperty("font-weight", v));
		TFPASS(v != nullptr && std::strcmp(v, "bold") == 0);
		/* revision table purged once nothing is marked */
		d.doc->purgeRevisionTable();
		TFPASS(d.doc->getRevisions().empty());
	}

	/* rejectAllHigherRevisions(0): every mark reverted — added text
	 * gone, deleted text back, fmt gone */
	{
		RevDoc d;
		TFPASS(d.build());
		const PT_DocPosition posBase = d.posBlock1() + 1;
		d.doc->setMarkRevisions(true);
		const UT_UCS4String ins("NEW");
		TFPASS(d.doc->insertSpan(posBase, ins.ucs4_str(), ins.length()));
		UT_uint32 n = 0;
		TFPASS(d.doc->deleteSpan(posBase + 4, posBase + 7, nullptr, n));
		const PP_PropertyVector boldProp = { "font-weight", "bold" };
		TFPASS(d.doc->changeSpanFmt(PTC_AddFmt, posBase + 8,
									posBase + 10, PP_NOPROPS, boldProp));
		d.doc->setMarkRevisions(false);

		TFPASS(d.doc->rejectAllHigherRevisions(0));
		TFPASS(docText(d.doc) == "base text second para ");
		TFPASS(fragRevAttr(d.doc, posBase).empty());
		const pf_Frag *pf = d.doc->getFragFromPosition(posBase + 5);
		const PP_AttrProp *pAP = nullptr;
		TFPASS(d.doc->getAttrProp(pf->getIndexAP(), &pAP));
		const gchar *v = nullptr;
		TFPASS(!pAP->getProperty("font-weight", v) || v == nullptr);
	}

	/* rejectAllHigherRevisions(iLevel) leaves marks at or below the
	 * level untouched */
	{
		RevDoc d;
		TFPASS(d.build());
		const PT_DocPosition posBase = d.posBlock1() + 1;
		d.doc->setMarkRevisions(true);
		const UT_UCS4String ins("NEW");
		TFPASS(d.doc->insertSpan(posBase, ins.ucs4_str(), ins.length()));
		const UT_uint32 idLow = d.doc->getRevisionId();    // +1
		TFPASS(d.doc->addRevision(2, nullptr, 1700000000, 0, true, "Bob"));
		UT_uint32 n = 0;
		TFPASS(d.doc->deleteSpan(posBase + 4, posBase + 7, nullptr, n)); // -2
		d.doc->setMarkRevisions(false);

		TFPASS(d.doc->rejectAllHigherRevisions(idLow));
		/* the -2 deletion is reverted, the +1 addition survives */
		TFPASS(docText(d.doc) == "NEWbase text second para ");
		TFPASS(fragHasRev(d.doc, posBase, idLow, PP_REVISION_ADDITION));
		TFPASS(fragRevAttr(d.doc, posBase + 5).empty());
	}
}
