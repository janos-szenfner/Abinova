/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource
 * 
 * Copyright (C) 2007 Philippe Milot <PhilMilot@gmail.com>
 * Copyright (C) 2025-2026 Abinova contributors
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

// Class definition include
#include "OXML_Element.h"

// Internal includes
#include "OXML_Types.h"
#include "OXML_Document.h"

// Abinova includes
#include "ut_types.h"
#include "ut_misc.h"
#include "pd_Document.h"
#include "pt_Types.h"

// External includes
#include <string>
#include <cstdio>

OXML_Element::OXML_Element(const std::string & id, OXML_ElementTag tag, OXML_ElementType type) : 
	OXML_ObjectWithAttrProp(),
	TARGET(0), 
	m_id(id), 
	m_tag(tag), 
	m_type(type)
{
}

OXML_Element::~OXML_Element()
{
	this->clearChildren();
}

bool OXML_Element::operator ==(const std::string & id)
{
	return this->m_id.compare(id) == 0;
}

OXML_SharedElement OXML_Element::getElement(const std::string & id) const
{
	OXML_ElementVector::const_iterator it;
	it = std::find(m_children.begin(), m_children.end(), id);
	return ( it != m_children.end() ) ? (*it) : OXML_SharedElement() ;
}

UT_Error OXML_Element::appendElement(const OXML_SharedElement & obj)
{
	UT_return_val_if_fail(obj.get() != nullptr, UT_ERROR);

	try {
		m_children.push_back(obj);
	} catch(...) {
		UT_DEBUGMSG(("Bad alloc!\n"));
		return UT_OUTOFMEM;
	}

	obj->setTarget(TARGET); //propagate the target

	return UT_OK;
}

UT_Error OXML_Element::clearChildren()
{
	m_children.clear();
	return m_children.size() == 0 ? UT_OK : UT_ERROR;
}

UT_Error OXML_Element::serializeChildren(IE_Exp_OpenXML* exporter)
{
	UT_Error ret = UT_OK;

	OXML_ElementVector::size_type i;
	for (i = 0; i < m_children.size(); i++)
	{
		ret = m_children[i]->serialize(exporter);
		if(ret != UT_OK)
			return ret;
	}

	return ret;
}

UT_Error OXML_Element::serialize(IE_Exp_OpenXML* exporter)
{
	UT_Error ret = UT_OK;
	//Do something here when export filter is implemented

	if (ret != UT_OK)
		return ret;	

	return serializeChildren(exporter);
}

UT_Error OXML_Element::addChildrenToPT(PD_Document * pDocument)
{
	UT_Error ret(UT_OK), temp(UT_OK);

	OXML_ElementVector::size_type i;
	for (i = 0; i < m_children.size(); i++)
	{
		temp = m_children[i]->addToPT(pDocument);
		if (temp != UT_OK)
			ret = temp;
	}
	return ret;
}

UT_Error OXML_Element::addToPT(PD_Document * pDocument)
{
	UT_Error ret = UT_OK;

	if (pDocument == nullptr)
		return UT_ERROR;

	//	const gchar ** atts = getAttributesWithProps();

	switch (m_tag) {
	case PG_BREAK:
	{
		UT_UCS4Char ucs = UCS_FF;
		ret = pDocument->appendSpan(&ucs, 1) ? UT_OK : UT_ERROR;
		UT_return_val_if_fail(ret == UT_OK, ret);
	}
		break;
	case CL_BREAK:
	{
		UT_UCS4Char ucs = UCS_VTAB;
		ret = pDocument->appendSpan(&ucs, 1) ? UT_OK : UT_ERROR;
		UT_return_val_if_fail(ret == UT_OK, ret);
	}
		break;
	
	case LN_BREAK:
	{
		UT_UCS4Char ucs = UCS_LF;
		ret = pDocument->appendSpan(&ucs, 1) ? UT_OK : UT_ERROR;
		UT_return_val_if_fail(ret == UT_OK, ret);
	}
		break;

	case P_TAG: //fall through to default
	case R_TAG: //fall through to default
	case T_TAG: //fall through to default
	default:
		UT_ASSERT_NOT_REACHED(); //We really shouldn't get here.
		break;
	}

	ret = addChildrenToPT(pDocument);
	return ret;
}

void OXML_Element::setTarget(int target)
{
	TARGET = target;
}

void OXML_Element::resolveAnchorMetrics()
{
	OXML_Document * doc = OXML_Document::getInstance();
	double pageW = 8.5, pageH = 11.0; //US Letter fallback (inches)
	if (doc)
	{
		if (!doc->getPageWidth().empty())
			pageW = UT_convertDimensionless(doc->getPageWidth().c_str());
		if (!doc->getPageHeight().empty())
			pageH = UT_convertDimensionless(doc->getPageHeight().c_str());
	}

	const gchar * v = nullptr;
	char buf[32];

	/* wp14 percent sizing — raw fractions recorded by the listener,
	 * resolved against the real page size now that it is known */
	if (getProperty("pct-width", v) == UT_OK && v)
	{
		g_snprintf(buf, sizeof(buf), "%.4fin",
				   UT_convertDimensionless(v) * pageW);
		setProperty("frame-width", buf);
	}
	if (getProperty("pct-height", v) == UT_OK && v)
	{
		g_snprintf(buf, sizeof(buf), "%.4fin",
				   UT_convertDimensionless(v) * pageH);
		setProperty("frame-height", buf);
	}

	/* wp14 percent position — replaces the Letter estimate the
	 * listener stored in xpos/ypos */
	if (getProperty("pct-pos-x", v) == UT_OK && v)
	{
		g_snprintf(buf, sizeof(buf), "%.4fin",
				   UT_convertDimensionless(v) * pageW);
		setProperty("xpos", buf);
	}
	if (getProperty("pct-pos-y", v) == UT_OK && v)
	{
		g_snprintf(buf, sizeof(buf), "%.4fin",
				   UT_convertDimensionless(v) * pageH);
		setProperty("ypos", buf);
	}

	/* group children: grp-* hold child-space EMU, grp-chOff/chExt the
	 * child coordinate space, grp-ext the group's rect, and base-* the
	 * group's resolved anchor geometry — child pos on the page is
	 * base + (childOff - chOff) * (ext / chExt) */
	if (getProperty("grp-xpos", v) != UT_OK || !v)
		return;
	double grpX = UT_convertDimensionless(v);
	double grpY = 0.0, grpW = 0.0, grpH = 0.0;
	if (getProperty("grp-ypos", v) == UT_OK && v)
		grpY = UT_convertDimensionless(v);
	if (getProperty("grp-width", v) == UT_OK && v)
		grpW = UT_convertDimensionless(v);
	if (getProperty("grp-height", v) == UT_OK && v)
		grpH = UT_convertDimensionless(v);

	double chOffX = 0.0, chOffY = 0.0, chExtX = 0.0, chExtY = 0.0;
	double extX = 0.0, extY = 0.0, offX = 0.0, offY = 0.0;
	if (getProperty("grp-chOffX", v) == UT_OK && v)
		chOffX = UT_convertDimensionless(v);
	if (getProperty("grp-chOffY", v) == UT_OK && v)
		chOffY = UT_convertDimensionless(v);
	if (getProperty("grp-chExtX", v) == UT_OK && v)
		chExtX = UT_convertDimensionless(v);
	if (getProperty("grp-chExtY", v) == UT_OK && v)
		chExtY = UT_convertDimensionless(v);
	if (getProperty("grp-extX", v) == UT_OK && v)
		extX = UT_convertDimensionless(v);
	if (getProperty("grp-extY", v) == UT_OK && v)
		extY = UT_convertDimensionless(v);
	if (getProperty("grp-offX", v) == UT_OK && v)
		offX = UT_convertDimensionless(v);
	if (getProperty("grp-offY", v) == UT_OK && v)
		offY = UT_convertDimensionless(v);

	const double sx = (chExtX > 0.0 && extX > 0.0) ? extX / chExtX : 1.0;
	const double sy = (chExtY > 0.0 && extY > 0.0) ? extY / chExtY : 1.0;

	double baseW = extX / 914400.0, baseH = extY / 914400.0;
	const gchar * bv = nullptr;
	if (getProperty("base-w", bv) == UT_OK && bv)
		baseW = UT_convertToInches(bv);
	if (getProperty("base-h", bv) == UT_OK && bv)
		baseH = UT_convertToInches(bv);
	if (getProperty("base-pctw", bv) == UT_OK && bv)
		baseW = UT_convertDimensionless(bv) * pageW;
	if (getProperty("base-pcth", bv) == UT_OK && bv)
		baseH = UT_convertDimensionless(bv) * pageH;

	double baseX = 0.0, baseY = 0.0;
	if (getProperty("base-xpos", bv) == UT_OK && bv)
		baseX = UT_convertToInches(bv);
	else if (getProperty("base-pctpx", bv) == UT_OK && bv)
		baseX = UT_convertDimensionless(bv) * pageW;
	else if (getProperty("base-halign", bv) == UT_OK && bv)
	{
		if (!strcmp(bv, "center"))
			baseX = (pageW - baseW) / 2.0;
		else if (!strcmp(bv, "right"))
			baseX = pageW - baseW;
	}
	if (getProperty("base-ypos", bv) == UT_OK && bv)
		baseY = UT_convertToInches(bv);
	else if (getProperty("base-pctpy", bv) == UT_OK && bv)
		baseY = UT_convertDimensionless(bv) * pageH;
	else if (getProperty("base-valign", bv) == UT_OK && bv)
	{
		if (!strcmp(bv, "center"))
			baseY = (pageH - baseH) / 2.0;
		else if (!strcmp(bv, "bottom"))
			baseY = pageH - baseH;
	}
	if (baseX < 0.0)
		baseX = 0.0;
	if (baseY < 0.0)
		baseY = 0.0;

	g_snprintf(buf, sizeof(buf), "%.4fin",
			   baseX + (offX + (grpX - chOffX) * sx) / 914400.0);
	setProperty("xpos", buf);
	g_snprintf(buf, sizeof(buf), "%.4fin",
			   baseY + (offY + (grpY - chOffY) * sy) / 914400.0);
	setProperty("ypos", buf);
	if (grpW > 0.0)
	{
		g_snprintf(buf, sizeof(buf), "%.4fin", grpW * sx / 914400.0);
		setProperty("frame-width", buf);
	}
	if (grpH > 0.0)
	{
		g_snprintf(buf, sizeof(buf), "%.4fin", grpH * sy / 914400.0);
		setProperty("frame-height", buf);
	}
}

void OXML_Element::resolveAnchorAlignment()
{
	const gchar * szH = nullptr;
	const gchar * szV = nullptr;
	const gchar * szExisting = nullptr;
	const bool bHasH = (getProperty("halign", szH) == UT_OK) && szH &&
		(getProperty("xpos", szExisting) != UT_OK || !szExisting);
	const bool bHasV = (getProperty("valign", szV) == UT_OK) && szV &&
		(getProperty("ypos", szExisting) != UT_OK || !szExisting);
	if (!bHasH && !bHasV)
		return;

	OXML_Document * doc = OXML_Document::getInstance();
	double pageW = 8.5, pageH = 11.0; //US Letter fallback (inches)
	if (doc)
	{
		if (!doc->getPageWidth().empty())
			pageW = UT_convertDimensionless(doc->getPageWidth().c_str());
		if (!doc->getPageHeight().empty())
			pageH = UT_convertDimensionless(doc->getPageHeight().c_str());
	}

	const gchar * szW = nullptr;
	const gchar * szHgt = nullptr;
	double frameW = 0.0, frameH = 0.0;
	if (getProperty("frame-width", szW) == UT_OK && szW)
		frameW = UT_convertToInches(szW);
	if (getProperty("frame-height", szHgt) == UT_OK && szHgt)
		frameH = UT_convertToInches(szHgt);

	char buf[32];
	if (bHasH)
	{
		double x = 0.0;
		if (!strcmp(szH, "center"))
			x = (pageW - frameW) / 2.0;
		else if (!strcmp(szH, "right"))
			x = pageW - frameW;
		/* "left" / "inside"/"outside" fall back to 0 */
		if (x < 0.0)
			x = 0.0;
		g_snprintf(buf, sizeof(buf), "%.4fin", x);
		setProperty("xpos", buf);
	}
	if (bHasV)
	{
		double y = 0.0;
		if (!strcmp(szV, "center"))
			y = (pageH - frameH) / 2.0;
		else if (!strcmp(szV, "bottom"))
			y = pageH - frameH;
		/* "top" / "inside"/"outside" fall back to 0 */
		if (y < 0.0)
			y = 0.0;
		g_snprintf(buf, sizeof(buf), "%.4fin", y);
		setProperty("ypos", buf);
	}
}
