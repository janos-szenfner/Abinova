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
 * Word-style content controls (OOXML w:sdt), field type
 * "content_control".
 *
 * The whole control is one atomic field object; its metadata rides on
 * the object's attributes so it round-trips through .abwn:
 *   <field type="content_control" sdt-type="text" sdt-value="Hi"
 *          sdt-title="Name" sdt-tag="name1"
 *          sdt-placeholder="..." sdt-items="a|b|c"/>
 *
 * sdt-type: text | checkbox | dropdown | combo | date
 * For a checkbox the sdt-value "1"/"true"/"on" renders checked.
 *
 * The field is read-only while the view's SDT design mode is off
 * (FV_View guards attr edits and deletion); design mode lets the
 * contentControl* edit methods rewrite the attributes. The bounding
 * box affordance is screen-only like Word's.
 * ================================================================== */

#include "fp_Run.h"
#include "fp_Line.h"
#include "gr_DrawArgs.h"
#include "fl_BlockLayout.h"
#include "fv_View.h"
#include "gr_Painter.h"
#include "pp_AttrProp.h"
#include "ut_debugmsg.h"

fp_FieldContentControlRun::fp_FieldContentControlRun(
	fl_BlockLayout* pBL, UT_uint32 iOffsetFirst, UT_uint32 iLen)
	: fp_FieldRun(pBL, iOffsetFirst, iLen),
	  m_bPlaceholder(false),
	  m_bChecked(false)
{
	/* the base ctor ran fp_FieldRun::_lookupProperties before our
	 * members existed — re-read now that they do */
	lookupProperties();
	calculateValue();
}

void fp_FieldContentControlRun::_lookupProperties(
	const PP_AttrProp * pSpanAP, const PP_AttrProp * pBlockAP,
	const PP_AttrProp * pSectionAP, GR_Graphics * pG)
{
	fp_FieldRun::_lookupProperties(pSpanAP, pBlockAP, pSectionAP, pG);

	/* for object runs the span AP is the object's AP — the same
	 * place the base class reads "type"/"param" from */
	const PP_AttrProp * pAP = pSpanAP ? pSpanAP : pBlockAP;
	m_sCtlType.clear();
	m_sValue.clear();
	m_sTitle.clear();
	m_sTag.clear();
	m_sPlaceholder.clear();
	m_sItems.clear();
	if (pAP)
	{
		const gchar * v = nullptr;
		if (pAP->getAttribute("sdt-type", v) && v)
			m_sCtlType = v;
		if (pAP->getAttribute("sdt-value", v) && v)
			m_sValue = v;
		if (pAP->getAttribute("sdt-title", v) && v)
			m_sTitle = v;
		if (pAP->getAttribute("sdt-tag", v) && v)
			m_sTag = v;
		if (pAP->getAttribute("sdt-placeholder", v) && v)
			m_sPlaceholder = v;
		if (pAP->getAttribute("sdt-items", v) && v)
			m_sItems = v;
	}
	if (m_sCtlType.empty())
		m_sCtlType = "text";
	calculateValue();
}

bool fp_FieldContentControlRun::calculateValue(void)
{
	m_bPlaceholder = false;
	m_bChecked = false;
	UT_UTF8String sVal;
	if (m_sCtlType == "checkbox")
	{
		m_bChecked = (m_sValue == "1" || m_sValue == "true" ||
					  m_sValue == "on");
		/* U+2612/U+2610 — the UTF-8 bytes pass through unchanged */
		sVal = m_bChecked ? "\xE2\x98\x92" : "\xE2\x98\x90";
	}
	else if (!m_sValue.empty())
	{
		sVal = m_sValue.c_str();
	}
	else
	{
		m_bPlaceholder = true;
		sVal = m_sPlaceholder.empty()
			? "Click or tap here to enter text."
			: m_sPlaceholder.c_str();
	}
	return _setValue(sVal.ucs4_str().ucs4_str());
}

void fp_FieldContentControlRun::_draw(dg_DrawArgs* pDA)
{
	GR_Graphics * pG = pDA->pG;

	/* placeholder text renders dimmed, like Word's prompt text */
	UT_RGBColor clrSaved;
	if (m_bPlaceholder)
	{
		clrSaved = _getColorFG();
		_setColorFG(UT_RGBColor(140, 140, 140));
	}
	_defaultDraw(pDA);
	if (m_bPlaceholder)
	{
		_setColorFG(clrSaved);
	}

	/* the bounding box is a screen affordance only — like Word's
	 * sdt frame it does not print */
	if (!pG->queryProperties(GR_Graphics::DGP_SCREEN))
		return;

	const UT_sint32 iBoxX = pDA->xoff;
	const UT_sint32 iBoxY = pDA->yoff - getAscent() - 1;
	const UT_sint32 iBoxW = getWidth();
	const UT_sint32 iBoxH = getAscent() + getDescent();
	if (iBoxW <= 0 || iBoxH <= 0)
		return;

	FV_View * pView = _getView();
	const bool bDesign = pView && pView->isSDTDesignMode();
	GR_Painter painter(pG);

	const UT_RGBColor clrBox = bDesign
		? UT_RGBColor(70, 130, 180)     /* steel blue in design mode */
		: UT_RGBColor(160, 160, 176);   /* quiet grey box otherwise */
	pG->setColor(clrBox);
	painter.drawLine(iBoxX, iBoxY, iBoxX + iBoxW, iBoxY);
	painter.drawLine(iBoxX, iBoxY + iBoxH,
					 iBoxX + iBoxW, iBoxY + iBoxH);
	painter.drawLine(iBoxX, iBoxY, iBoxX, iBoxY + iBoxH);
	painter.drawLine(iBoxX + iBoxW, iBoxY,
					 iBoxX + iBoxW, iBoxY + iBoxH);
	if (bDesign)
	{
		/* a small "handle" tab at the top-left corner, echoing
		 * Word's title tab */
		const UT_sint32 iTab = pG->tlu(4);
		painter.fillRect(clrBox, iBoxX, iBoxY, iTab, iTab);
	}
	pG->setColor(_getColorFG());
}
