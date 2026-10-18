/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
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

/* ==================================================================
 * Zotero citation field (type "zotero").
 *
 * The citation is one atomic field object; its marker data rides on
 * the object's attributes so it round-trips through .abwn:
 *   <field type="zotero" zotero-key="ABCD2345" zotero-style="apa"
 *          zotero-mode="citation" zotero-text="(Smith, 2020)"/>
 *
 * zotero-mode: citation | bib   (in-text citation vs. bibliography
 * entry — which CSL output was fetched).  zotero-text is the cached
 * formatted output so the document renders the citation even when
 * no Zotero is running; zoteroRefresh rewrites it from the service.
 * ================================================================== */

#include "fp_Run.h"
#include "pp_AttrProp.h"

fp_FieldZoteroRun::fp_FieldZoteroRun(
	fl_BlockLayout* pBL, UT_uint32 iOffsetFirst, UT_uint32 iLen)
	: fp_FieldRun(pBL, iOffsetFirst, iLen)
{
	/* the base ctor ran fp_FieldRun::_lookupProperties before our
	 * members existed — re-read now that they do */
	lookupProperties();
	calculateValue();
}

void fp_FieldZoteroRun::_lookupProperties(
	const PP_AttrProp * pSpanAP, const PP_AttrProp * pBlockAP,
	const PP_AttrProp * pSectionAP, GR_Graphics * pG)
{
	fp_FieldRun::_lookupProperties(pSpanAP, pBlockAP, pSectionAP, pG);

	/* for object runs the span AP is the object's AP — the same
	 * place the base class reads "type"/"param" from */
	const PP_AttrProp * pAP = pSpanAP ? pSpanAP : pBlockAP;
	m_sMode.clear();
	m_sText.clear();
	if (pAP)
	{
		const gchar * v = nullptr;
		if (pAP->getAttribute("zotero-mode", v) && v)
			m_sMode = v;
		if (pAP->getAttribute("zotero-text", v) && v)
			m_sText = v;
	}
	if (m_sMode.empty())
		m_sMode = "citation";
	calculateValue();
}

bool fp_FieldZoteroRun::calculateValue(void)
{
	UT_UTF8String sVal;
	if (!m_sText.empty())
	{
		sVal = m_sText.c_str();
	}
	else
	{
		/* nothing fetched yet (or the service was unreachable at
		 * insert time) — show a Word-like field placeholder */
		sVal = m_sMode == "bib"
			? "[Zotero bibliography entry]"
			: "[Zotero citation]";
	}
	return _setValue(sVal.ucs4_str().ucs4_str());
}
