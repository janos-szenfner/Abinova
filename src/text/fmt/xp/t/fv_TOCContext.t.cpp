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

#include "tf_test.h"

#include "pd_Document.h"
#include "fl_DocLayout.h"
#include "fl_TOCLayout.h"
#include "fp_Page.h"
#include "fp_Column.h"
#include "fp_Line.h"
#include "fp_TOCContainer.h"
#include "fv_View.h"
#include "gr_UnixCairoGraphics.h"
#include "ev_Menu_Layouts.h"
#include "xap_App.h"
#include "xap_Menu_Layouts.h"
#include "ap_Menu_Id.h"
#include "ut_types.h"

#include <glib.h>

#include <cstring>
#include <optional>
#include <string>

#define TFSUITE "core.text.fmt.toccontext"

namespace {

/* Same widget-less stack as fv_MouseContext/fv_ViewModes: painting
 * goes to a private cairo image surface so the full layout machinery
 * works headless. */
struct HeadlessTOCView
{
	HeadlessTOCView() = default;
	HeadlessTOCView(const HeadlessTOCView &) = delete;
	HeadlessTOCView &operator=(const HeadlessTOCView &) = delete;

	bool open(const char *relPath)
	{
		std::string data_file;
		if (!TF_Test::ensure_test_data(relPath, data_file))
			return false;
		doc = new PD_Document;
		if (doc->readFromFile(data_file.c_str(), IEFT_Unknown, nullptr) != UT_OK)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		GR_UnixCairoAllocInfo ai(nullptr);
		graphics = XAP_App::getApp()->newGraphics(GRID_UNIX_PANGO, ai);
		if (!graphics)
			return false;
		layout = new FL_DocLayout(doc, graphics);
		view = new FV_View(XAP_App::getApp(), nullptr, layout);
		return true;
	}

	bool format()
	{
		layout->fillLayouts();
		layout->formatAll();
		return layout->countPages() > 0;
	}

	fp_Line * findFirstLine(fp_ContainerObject *pCon) const
	{
		if (!pCon)
			return nullptr;
		if (pCon->getContainerType() == FP_CONTAINER_LINE)
			return static_cast<fp_Line *>(pCon);
		fp_Container *pCtr = dynamic_cast<fp_Container *>(pCon);
		if (!pCtr)
			return nullptr;
		for (UT_sint32 i = 0; i < pCtr->countCons(); i++)
		{
			fp_Line *pLine = findFirstLine(pCtr->getNthCon(i));
			if (pLine)
				return pLine;
		}
		return nullptr;
	}

	~HeadlessTOCView()
	{
		delete view;    /* before layout: ~FV_View detaches from it */
		delete layout;
		delete graphics;
		if (doc)
			doc->unref();
	}

	PD_Document *doc = nullptr;
	GR_Graphics *graphics = nullptr;
	FL_DocLayout *layout = nullptr;
	FV_View *view = nullptr;
};

} // namespace

TFTEST_MAIN("TOC context menu resolves and lists Update and Format")
{
	XAP_Menu_Factory *pFactory = XAP_App::getApp()->getMenuFactory();
	TFPASS(pFactory != nullptr);
	if (!pFactory)
		return;

	const char *szName = pFactory->FindContextMenu(EV_EMC_TOC);
	TFPASS(szName != nullptr && strcmp(szName, "ContextTOC") == 0);
	if (!szName)
		return;

	EV_Menu_Layout *pLayout = pFactory->CreateMenuLayout(szName);
	TFPASS(pLayout != nullptr);
	if (!pLayout)
		return;

	bool bUpdate = false;
	bool bFormat = false;
	for (UT_uint32 i = 0; i < pLayout->getLayoutItemCount(); i++)
	{
		const EV_Menu_LayoutItem *pItem = pLayout->getLayoutItem(i);
		if (!pItem || pItem->getMenuLayoutFlags() != EV_MLF_Normal)
			continue;
		if (pItem->getMenuId() == static_cast<XAP_Menu_Id>(AP_MENU_ID_REF_UPDATETOC))
			bUpdate = true;
		if (pItem->getMenuId() == static_cast<XAP_Menu_Id>(AP_MENU_ID_FMT_TABLEOFCONTENTS))
			bFormat = true;
	}
	TFPASS(bUpdate);
	TFPASS(bFormat);
	delete pLayout;
}

TFTEST_MAIN("click inside a TOC resolves TOC context; select gates the items")
{
	HeadlessTOCView hv;
	TFPASS(hv.open("/test/wp/toc.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());

	TFPASS(hv.layout->getNumTOCs() == 1);
	fl_TOCLayout *pTOCL = hv.layout->getNthTOC(0);
	TFPASS(pTOCL != nullptr);
	if (!pTOCL)
		return;
	fp_TOCContainer *pTOCCon =
		static_cast<fp_TOCContainer *>(pTOCL->getFirstContainer());
	TFPASS(pTOCCon != nullptr);

	fp_Line *pLine = hv.findFirstLine(pTOCCon);
	TFPASS(pLine != nullptr);
	if (!pLine)
		return;
	std::optional<UT_Rect> rect = pLine->getScreenRect();
	TFPASS(rect.has_value());
	if (!rect)
		return;

	/* a click on a TOC entry line resolves to the TOC mouse context */
	UT_sint32 cx = rect->left + rect->width / 2;
	UT_sint32 cy = rect->top + rect->height / 2;
	TFPASS(hv.view->getMouseContext(cx, cy) == EV_EMC_TOC);

	/* the context menu does not warp the insertion point — the TOC
	 * must be selected for the Update/Format items to be enabled and
	 * to act on the clicked TOC, exactly like contextTOC does */
	TFPASS(!hv.view->isTOCSelected());
	hv.view->cmdSelectTOC(cx, cy);
	TFPASS(hv.view->isTOCSelected());
	TFPASS(hv.view->hasTOC());
	TFPASS(hv.view->cmdUpdateTOC());
}

TFTEST_MAIN("body clicks outside the TOC stay in the text context")
{
	HeadlessTOCView hv;
	TFPASS(hv.open("/test/wp/toc.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());

	fp_Page *pPage = hv.layout->getNthPage(0);
	TFPASS(pPage != nullptr);
	if (!pPage)
		return;

	UT_sint32 xoff = 0, yoff = 0;
	hv.view->getPageScreenOffsets(pPage, xoff, yoff);
	UT_sint32 mx = xoff + pPage->getWidth() / 2;
	UT_sint32 my = yoff + pPage->getHeight() * 4 / 5;
	TFPASS(hv.view->getMouseContext(mx, my) == EV_EMC_TEXT);
	TFPASS(!hv.view->isTOCSelected());
}
