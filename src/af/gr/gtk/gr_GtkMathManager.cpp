/* Abinova
 * Copyright (C) 2025 AbiSource
 * Copyright (C) 2025-2026 Abinova contributors
 *
 * Built-in MathML/LaTeX equation renderer for Abinova.
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

#include "gr_GtkMathManager.h"
#include "gr_CairoGraphics.h"
#include "xad_Document.h"
#include "ut_debugmsg.h"
#include "ut_assert.h"
#include "ut_units.h"
#include "ut_bytebuf.h"
#include "ut_mbtowc.h"

#include <cairo-svg.h>
#include <cmath>
#include <cstring>

GR_GtkMathManager::GR_GtkMathManager(GR_Graphics * pG)
	: GR_EmbedManager(pG)
	, m_pDoc(nullptr)
{
}

GR_GtkMathManager::~GR_GtkMathManager()
{
	for (UT_sint32 i = 0; i < m_items.getItemCount(); ++i)
		delete m_items.getNthItem(i);
}

GR_EmbedManager * GR_GtkMathManager::create(GR_Graphics * pG)
{
	return new GR_GtkMathManager(pG);
}

const char * GR_GtkMathManager::getObjectType(void) const
{
	return "mathml";
}

const char * GR_GtkMathManager::getMimeType(void) const
{
	return "application/mathml+xml";
}

const char * GR_GtkMathManager::getMimeTypeDescription(void) const
{
	return "MathML Equation";
}

const char * GR_GtkMathManager::getMimeTypeSuffix(void) const
{
	return ".mml";
}

bool GR_GtkMathManager::isDefault(void)
{
	return false;
}

bool GR_GtkMathManager::isEdittable(UT_sint32 /*uid*/)
{
	return true;
}

EV_EditMouseContext GR_GtkMathManager::getContextualMenu(void) const
{
	return EV_EMC_MATH;
}

UT_sint32 GR_GtkMathManager::_toLU(double pt) const
{
	return (UT_sint32)lrint(pt * UT_LAYOUT_RESOLUTION / 72.0);
}

GR_GtkMathManager::MathItem * GR_GtkMathManager::_item(UT_sint32 uid)
{
	if (uid < 0 || uid >= m_items.getItemCount())
		return nullptr;
	return m_items.getNthItem(uid);
}

UT_sint32 GR_GtkMathManager::makeEmbedView(AD_Document * pDoc, UT_uint32 api,
                                         const char * szDataID)
{
	UT_sint32 uid = GR_EmbedManager::makeEmbedView(pDoc, api, szDataID);
	MathItem *it = new MathItem;
	it->api = api;
	if (szDataID)
		it->dataID = szDataID;
	m_pDoc = pDoc;
	m_items.addItem(it);
	return uid;
}

void GR_GtkMathManager::releaseEmbedView(UT_sint32 uid)
{
	MathItem *it = _item(uid);
	if (it) {
		delete it;
		m_items.setNthItem(uid, nullptr, nullptr);
	}
	GR_EmbedManager::releaseEmbedView(uid);
}

void GR_GtkMathManager::initializeEmbedView(UT_sint32 /*uid*/)
{
}

void GR_GtkMathManager::setRun(UT_sint32 uid, fp_Run * run)
{
	MathItem *it = _item(uid);
	if (it) it->run = run;
}

void GR_GtkMathManager::loadEmbedData(UT_sint32 uid)
{
	MathItem *it = _item(uid);
	UT_return_if_fail(it);
	UT_return_if_fail(m_pDoc);

	UT_ConstByteBufPtr pBuf;
	UT_UTF8String sMathML;
	if (!it->dataID.empty() &&
	    m_pDoc->getDataItemDataByName(it->dataID.c_str(), pBuf,
	                                  nullptr, nullptr) &&
	    pBuf && pBuf->getLength() > 0)
	{
		UT_UCS4_mbtowc wc;
		sMathML.appendBuf(pBuf, wc);
	}
	if (sMathML.size() > 0 && strchr(sMathML.utf8_str(), '<')) {
		it->ts.parseMathML(sMathML.utf8_str(), sMathML.size());
	} else {
		/* fall back to the latex source item: cmdInsertLatexMath
		 * pairs "MathLatex<uuid>" with "LatexMath<uuid>" */
		std::string latID;
		const std::string mp = "MathLatex";
		if (it->dataID.compare(0, mp.size(), mp) == 0)
			latID = "LatexMath" + it->dataID.substr(mp.size());
		if (!latID.empty() &&
		    m_pDoc->getDataItemDataByName(latID.c_str(), pBuf,
		                                  nullptr, nullptr) &&
		    pBuf && pBuf->getLength() > 0)
		{
			UT_UCS4_mbtowc wc;
			UT_UTF8String sL;
			sL.appendBuf(pBuf, wc);
			it->ts.parseLaTeX(sL.utf8_str());
		} else {
			it->ts.parseLaTeX("");
		}
	}
	it->dirty = true;
	_relayout(it);
}

void GR_GtkMathManager::_relayout(MathItem *it)
{
	if (!it || !it->dirty)
		return;
	cairo_surface_t *sf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	cairo_t *cr = cairo_create(sf);
	it->ts.setColor(it->r, it->g, it->b);
	it->ts.layout(cr, it->family.c_str(), it->baseSizePt, it->display);
	cairo_destroy(cr);
	cairo_surface_destroy(sf);
	it->dirty = false;
}

void GR_GtkMathManager::setColor(UT_sint32 uid, const UT_RGBColor & c)
{
	MathItem *it = _item(uid);
	UT_return_if_fail(it);
	it->r = c.m_red / 255.0;
	it->g = c.m_grn / 255.0;
	it->b = c.m_blu / 255.0;
	it->ts.setColor(it->r, it->g, it->b);
}

bool GR_GtkMathManager::setFont(UT_sint32 uid, const GR_Font * pFont)
{
	MathItem *it = _item(uid);
	UT_return_val_if_fail(it, false);
	if (pFont && pFont->getFamily() && *pFont->getFamily())
		it->family = pFont->getFamily();
	it->dirty = true;
	return true;
}

void GR_GtkMathManager::setDefaultFontSize(UT_sint32 uid, UT_sint32 iSize)
{
	MathItem *it = _item(uid);
	UT_return_if_fail(it);
	if (iSize > 0 && iSize < 400)
		it->baseSizePt = iSize;
	it->dirty = true;
	_relayout(it);
}

void GR_GtkMathManager::setDisplayMode(UT_sint32 uid, AbiDisplayMode mode)
{
	MathItem *it = _item(uid);
	UT_return_if_fail(it);
	it->display = (mode == ABI_DISPLAY_BLOCK);
	it->dirty = true;
	_relayout(it);
}

UT_sint32 GR_GtkMathManager::getWidth(UT_sint32 uid)
{
	MathItem *it = _item(uid);
	UT_return_val_if_fail(it, 0);
	_relayout(it);
	return _toLU(it->ts.width() + it->baseSizePt * 0.2);
}

UT_sint32 GR_GtkMathManager::getAscent(UT_sint32 uid)
{
	MathItem *it = _item(uid);
	UT_return_val_if_fail(it, 0);
	_relayout(it);
	return _toLU(it->ts.ascent());
}

UT_sint32 GR_GtkMathManager::getDescent(UT_sint32 uid)
{
	MathItem *it = _item(uid);
	UT_return_val_if_fail(it, 0);
	_relayout(it);
	return _toLU(it->ts.descent());
}

void GR_GtkMathManager::render(UT_sint32 uid, UT_Rect & rec)
{
	MathItem *it = _item(uid);
	UT_return_if_fail(it);
	_relayout(it);
	if (rec.width <= 0 || rec.height <= 0 || it->ts.width() <= 0)
		return;
	GR_CairoGraphics *pUGG = static_cast<GR_CairoGraphics *>(getGraphics());
	UT_return_if_fail(pUGG);
	/* render() is invoked from fp_MathRun::_draw inside the paint
	 * callback — only bracket with begin/endPaint when called outside
	 * one, otherwise endPaint() would tear down the active cairo and
	 * force an endless repaint loop */
	bool ownPaint = (pUGG->getPaintCount() == 0);
	if (ownPaint)
		pUGG->beginPaint();
	cairo_t *cr = pUGG->getCairo();
	cairo_save(cr);
	/* rec is in layout units; rec.top is the baseline position */
	double devX = pUGG->tdu(rec.left);
	double devY = pUGG->tdu(rec.top - _toLU(it->ts.ascent()));
	double scale = (double)pUGG->tdu(rec.width) / it->ts.width();
	cairo_translate(cr, devX, devY);
	cairo_scale(cr, scale, scale);
	it->ts.render(cr);
	cairo_restore(cr);
	if (ownPaint)
		pUGG->endPaint();
}

static cairo_status_t s_svgWrite(void *buf, const unsigned char *data,
                                 unsigned int length)
{
	UT_ByteBuf *b = static_cast<UT_ByteBuf *>(buf);
	return b->append(data, length) ? CAIRO_STATUS_SUCCESS
	                               : CAIRO_STATUS_WRITE_ERROR;
}

void GR_GtkMathManager::makeSnapShot(UT_sint32 uid, UT_Rect & /*rec*/)
{
	MathItem *it = _item(uid);
	UT_return_if_fail(it);
	UT_return_if_fail(m_pDoc);
	_relayout(it);
	double w = it->ts.width(), h = it->ts.ascent() + it->ts.descent();
	if (w <= 0 || h <= 0 || it->dataID.empty())
		return;
	UT_ByteBufPtr pBuf(new UT_ByteBuf);
	cairo_surface_t *sf = cairo_svg_surface_create_for_stream(
		(cairo_write_func_t)s_svgWrite, pBuf.get(), w, h);
	cairo_t *cr = cairo_create(sf);
	it->ts.render(cr);
	cairo_destroy(cr);
	cairo_surface_finish(sf);
	cairo_surface_destroy(sf);
	if (!pBuf->getLength())
		return;

	UT_UTF8String sID = "snapshot-svg-";
	sID += it->dataID.c_str();
	std::string mime = "image/svg+xml";
	if (it->hasSnap)
	{
		/* skip the write when the snapshot is unchanged — otherwise every
		 * paint marks the document modified, forcing a re-layout that
		 * regenerates the snapshot again (an endless repaint loop) */
		UT_ConstByteBufPtr pOld;
		if (m_pDoc->getDataItemDataByName(sID.utf8_str(), pOld,
		                                nullptr, nullptr) &&
		    pOld && pOld->getLength() == pBuf->getLength() &&
		    pOld->getLength() > 0 &&
		    !memcmp(pOld->getPointer(0), pBuf->getPointer(0),
		            pBuf->getLength()))
			return;
		m_pDoc->replaceDataItem(sID.utf8_str(), UT_ConstByteBufPtr(pBuf));
	}
	else {
		m_pDoc->createDataItem(sID.utf8_str(), false,
		                       UT_ConstByteBufPtr(pBuf), mime, nullptr);
		it->hasSnap = true;
	}
}

bool GR_GtkMathManager::modify(UT_sint32 /*uid*/)
{
	/* editing is driven by the LaTeX dialog (dlgEditLatexEquation) */
	return false;
}

bool GR_GtkMathManager::convert(UT_uint32 /*iConvType*/,
                                const UT_ConstByteBufPtr & pFrom,
                                const UT_ByteBufPtr & pTo)
{
	UT_return_val_if_fail(pFrom && pTo, false);
	if (!pFrom->getLength())
		return false;
	std::string sLatex((const char *)pFrom->getPointer(0),
	                   pFrom->getLength());
	GR_MathTypesetter ts;
	ts.parseLaTeX(sLatex.c_str());
	UT_UTF8String sML = ts.toMathML();
	pTo->ins(0, (const UT_Byte *)sML.utf8_str(), sML.size());
	return true;
}

void GR_GtkMathManager::updateData(UT_sint32 uid, UT_sint32 api)
{
	MathItem *it = _item(uid);
	UT_return_if_fail(it);
	it->api = api;
}
