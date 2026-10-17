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

#include <set>
#include <string.h>

#include "ut_types.h"
#include "ut_std_string.h"

#include "pd_Document.h"
#include "pd_Iterator.h"
#include "pd_Style.h"
#include "pf_Frag.h"
#include "pp_AttrProp.h"
#include "pp_Property.h"
#include "pp_Revision.h"
#include "pt_Types.h"
#include "fv_View.h"

#include "ap_DocInspect.h"

/* meta keys that carry live document state rather than Dublin Core
 * properties — stripping them would break features, not privacy:
 * dc.format/abiword.generator are re-stamped on every save anyway,
 * attached-template feeds the template machinery and bib.* keys are
 * the bibliography source store read by getBibSources() */
static bool s_meta_key_is_functional(const std::string & key)
{
	if (key == PD_META_KEY_FORMAT || key == PD_META_KEY_GENERATOR ||
		key == "attached-template")
		return true;
	return key.compare(0, 4, "bib.") == 0;
}

/* evaluated "display" for a text frag: the span AP plus the enclosing
 * block/section APs so a hidden block or a hidden character style
 * counts — same cascade fp_Run::lookupProperties applies */
static bool s_frag_is_hidden_text(PD_Document * pDoc, const pf_Frag * pf,
								  PT_DocPosition pos)
{
	if (!pf || pf->getType() != pf_Frag::PFT_Text || !pf->getLength())
		return false;
	const PP_AttrProp * pSpanAP = nullptr;
	pDoc->getAttrProp(pf->getIndexAP(), &pSpanAP);
	const PP_AttrProp * pBlockAP = nullptr;
	const PP_AttrProp * pSectionAP = nullptr;
	const pf_Frag_Strux * sdh = nullptr;
	if (pDoc->getStruxOfTypeFromPosition(pos, PTX_Block, &sdh) && sdh)
		pDoc->getAttrProp(pDoc->getAPIFromStrux(sdh), &pBlockAP);
	if (pDoc->getStruxOfTypeFromPosition(pos, PTX_Section, &sdh) && sdh)
		pDoc->getAttrProp(pDoc->getAPIFromStrux(sdh), &pSectionAP);
	const gchar * pszDisplay =
		PP_evalProperty("display", pSpanAP, pBlockAP, pSectionAP, pDoc, true);
	return pszDisplay && !strcmp(pszDisplay, "none");
}

void AP_DocInspect::inspect(PD_Document * pDoc, AP_PrivacyReport & out)
{
	out.cats.clear();

	AP_PrivacyCategory meta, ann, hidden, rev;
	meta.mask = AP_PRIVACY_META;
	meta.title = "Document properties and personal information";
	meta.found = false;
	ann.mask = AP_PRIVACY_ANNOTATIONS;
	ann.title = "Comments and annotations";
	ann.found = false;
	hidden.mask = AP_PRIVACY_HIDDEN_TEXT;
	hidden.title = "Hidden text";
	hidden.found = false;
	rev.mask = AP_PRIVACY_REVISIONS;
	rev.title = "Tracked changes";
	rev.found = false;

	if (!pDoc)
	{
		out.cats = {meta, ann, hidden, rev};
		return;
	}

	/* ---- document properties ----------------------------------- */
	{
		const std::map<std::string,std::string> & mm = pDoc->getMetaData();
		for (const auto & kv : mm)
		{
			if (kv.second.empty() || s_meta_key_is_functional(kv.first))
				continue;
			std::string line = kv.first;
			line += " = ";
			line += kv.second;
			meta.details.push_back(std::move(line));
		}
		meta.found = !meta.details.empty();
	}

	/* ---- comments / annotations -------------------------------- */
	{
		const pf_Frag_Strux * sdhDoc = nullptr;
		pDoc->getStruxOfTypeFromPosition(2, PTX_Section, &sdhDoc);
		const pf_Frag_Strux * sdh = sdhDoc;
		std::set<std::string> authors;
		UT_uint32 n = 0;
		while (sdh &&
			   pDoc->getNextStruxOfType(sdh, PTX_SectionAnnotation, &sdh) && sdh)
		{
			++n;
			const gchar * szAuthor = nullptr;
			if (pDoc->getPropertyFromStrux(sdh, true, PD_MAX_REVISION,
										 "annotation-author", &szAuthor) &&
				szAuthor && szAuthor[0])
				authors.insert(szAuthor);
		}
		if (n)
		{
			ann.found = true;
			std::string line = UT_std_string_sprintf("%u comment%s", n,
													 n == 1 ? "" : "s");
			if (!authors.empty())
			{
				line += " by ";
				bool first = true;
				for (const std::string & a : authors)
				{
					if (!first)
						line += ", ";
					line += a;
					first = false;
				}
			}
			ann.details.push_back(std::move(line));
		}
	}

	/* ---- hidden text + tracked changes (single frag walk) ------- */
	{
		PD_DocIterator t(*pDoc);
		UT_uint32 nHiddenChars = 0;
		UT_uint32 nHiddenRuns = 0;
		UT_uint32 nRevMarks = 0;
		std::set<UT_uint32> revIds;
		bool bInHidden = false;
		while (t.getStatus() == UTIter_OK)
		{
			pf_Frag * pf = t.getFrag();
			if (!pf)
				break;
			const PT_DocPosition pos = t.getPosition();
			bool bHiddenFrag = s_frag_is_hidden_text(pDoc, pf, pos);
			if (bHiddenFrag)
			{
				nHiddenChars += pf->getLength();
				if (!bInHidden)
					++nHiddenRuns;
			}
			bInHidden = bHiddenFrag;

			const PP_AttrProp * pAP = nullptr;
			if (pDoc->getAttrProp(pf->getIndexAP(), &pAP) && pAP)
			{
				const gchar * pszRevision = nullptr;
				if (pAP->getAttribute(PT_REVISION_ATTRIBUTE_NAME, pszRevision) &&
					pszRevision && pszRevision[0])
				{
					PP_RevisionAttr ra(pszRevision);
					ra.pruneForCumulativeResult(pDoc);
					if (ra.getRevisionsCount())
					{
						++nRevMarks;
						for (UT_uint32 i = 0; i < ra.getRevisionsCount(); ++i)
						{
							const PP_Revision * r = ra.getNthRevision(i);
							if (r)
								revIds.insert(r->getId());
						}
					}
				}
			}
			t += pf->getLength() ? pf->getLength() : 1;
		}
		if (nHiddenChars)
		{
			hidden.found = true;
			hidden.details.push_back(UT_std_string_sprintf(
				"%u character%s in %u run%s", nHiddenChars,
				nHiddenChars == 1 ? "" : "s", nHiddenRuns,
				nHiddenRuns == 1 ? "" : "s"));
		}
		UT_uint32 nRevRecords =
			static_cast<UT_uint32>(pDoc->getRevisions().size());
		if (nRevMarks || nRevRecords)
		{
			rev.found = true;
			if (nRevMarks)
				rev.details.push_back(UT_std_string_sprintf(
					"%u text fragment%s carry revision marks "
					"(%u distinct revision%s)", nRevMarks,
					nRevMarks == 1 ? "" : "s",
					static_cast<UT_uint32>(revIds.size()),
					revIds.size() == 1 ? "" : "s"));
			else
				rev.details.push_back(UT_std_string_sprintf(
					"%u revision record%s (no marks left in the text)",
					nRevRecords, nRevRecords == 1 ? "" : "s"));
		}
	}

	out.cats.push_back(std::move(meta));
	out.cats.push_back(std::move(ann));
	out.cats.push_back(std::move(hidden));
	out.cats.push_back(std::move(rev));
}

std::string AP_DocInspect::toText(const AP_PrivacyReport & r)
{
	std::string out;
	for (const AP_PrivacyCategory & c : r.cats)
	{
		out += c.title;
		out += "\n";
		if (c.found)
		{
			for (const std::string & line : c.details)
			{
				out += "  ";
				out += line;
				out += "\n";
			}
		}
		else
		{
			out += "  Nothing found\n";
		}
		out += "\n";
	}
	return out;
}

UT_uint32 AP_DocInspect::maskFromString(const char * s)
{
	if (!s || !s[0])
		return AP_PRIVACY_ALL;
	UT_uint32 mask = 0;
	std::string rest(s);
	size_t start = 0;
	for (;;)
	{
		size_t comma = rest.find(',', start);
		std::string tok = rest.substr(start,
			comma == std::string::npos ? std::string::npos : comma - start);
		while (!tok.empty() && (tok.front() == ' ' || tok.front() == '\t'))
			tok.erase(tok.begin());
		while (!tok.empty() && (tok.back() == ' ' || tok.back() == '\t'))
			tok.pop_back();
		if (tok == "meta" || tok == "properties" || tok == "docprops")
			mask |= AP_PRIVACY_META;
		else if (tok == "comments" || tok == "annotations")
			mask |= AP_PRIVACY_ANNOTATIONS;
		else if (tok == "hidden" || tok == "hiddentext")
			mask |= AP_PRIVACY_HIDDEN_TEXT;
		else if (tok == "revisions" || tok == "changes")
			mask |= AP_PRIVACY_REVISIONS;
		else if (tok == "all")
			mask |= AP_PRIVACY_ALL;
		if (comma == std::string::npos)
			break;
		start = comma + 1;
	}
	return mask ? mask : AP_PRIVACY_ALL;
}

UT_uint32 AP_DocInspect::scrub(FV_View * pView, UT_uint32 mask)
{
	if (!pView || !pView->getDocument() || !(mask & AP_PRIVACY_ALL))
		return 0;
	PD_Document * pDoc = pView->getDocument();
	UT_uint32 done = 0;

	pDoc->notifyPieceTableChangeStart();
	pDoc->beginUserAtomicGlob();

	if (mask & AP_PRIVACY_ANNOTATIONS)
	{
		if (pView->delAllAnnotations())
			done |= AP_PRIVACY_ANNOTATIONS;
	}

	if (mask & AP_PRIVACY_HIDDEN_TEXT)
	{
		bool bAny = false;
		PD_DocIterator t(*pDoc);
		while (t.getStatus() == UTIter_OK)
		{
			pf_Frag * pf = t.getFrag();
			if (!pf)
				break;
			const PT_DocPosition pos = t.getPosition();
			const UT_uint32 len = pf->getLength();
			if (len && s_frag_is_hidden_text(pDoc, pf, pos))
			{
				UT_uint32 iReal = 0;
				pDoc->deleteSpan(pos, pos + len, nullptr, iReal, false);
				bAny = true;
				t.reset(pos, nullptr);
			}
			else
			{
				t += len ? len : 1;
			}
		}
		if (bAny)
			done |= AP_PRIVACY_HIDDEN_TEXT;
	}

	if (mask & AP_PRIVACY_REVISIONS)
	{
		PD_DocIterator t(*pDoc);
		bool bAny = false;
		while (t.getStatus() == UTIter_OK)
		{
			pf_Frag * pf = t.getFrag();
			if (!pf)
				break;
			const PP_AttrProp * pAP = nullptr;
			if (pDoc->getAttrProp(pf->getIndexAP(), &pAP) && pAP)
			{
				const gchar * pszRevision = nullptr;
				if (pAP->getAttribute(PT_REVISION_ATTRIBUTE_NAME, pszRevision) &&
					pszRevision && pszRevision[0])
				{
					bAny = true;
					break;
				}
			}
			t += pf->getLength() ? pf->getLength() : 1;
		}
		if (bAny)
		{
			/* acceptAllRevisions opens its own glob + notification
			 * pair — both nest cleanly inside ours */
			pDoc->acceptAllRevisions();
			done |= AP_PRIVACY_REVISIONS;
		}
		if (!pDoc->getRevisions().empty())
		{
			/* revision table records remain after the marks are gone */
			pDoc->purgeRevisionTable();
			done |= AP_PRIVACY_REVISIONS;
		}
	}

	pDoc->endUserAtomicGlob();
	pDoc->notifyPieceTableChangeEnd();

	if (mask & AP_PRIVACY_META)
	{
		std::vector<std::string> keys;
		const std::map<std::string,std::string> & mm = pDoc->getMetaData();
		for (const auto & kv : mm)
		{
			if (!s_meta_key_is_functional(kv.first))
				keys.push_back(kv.first);
		}
		for (const std::string & k : keys)
		{
			pDoc->removeMetaDataProp(k);
			done |= AP_PRIVACY_META;
		}
	}

	if (done)
	{
		pDoc->signalListeners(PD_SIGNAL_UPDATE_LAYOUT);
		pDoc->forceDirty();
	}
	return done;
}
