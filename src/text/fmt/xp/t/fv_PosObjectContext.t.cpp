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
#include "fl_SectionLayout.h"
#include "fl_FrameLayout.h"
#include "fp_Page.h"
#include "fp_FrameContainer.h"
#include "fv_View.h"
#include "fv_FrameEdit.h"
#include "gr_UnixCairoGraphics.h"
#include "ev_Menu_Layouts.h"
#include "xap_App.h"
#include "xap_Menu_Layouts.h"
#include "ap_Menu_Id.h"
#include "ap_Menu_Functions.h"
#include "pp_AttrProp.h"
#include "ut_types.h"

#include <glib.h>

#include <cstring>
#include <vector>

#define TFSUITE "core.text.fmt.posobjectcontext"

namespace {

/* Same widget-less stack as fv_MouseContext/fv_FrameContext: painting
 * goes to a private cairo image surface so the full layout machinery
 * works headless. */
struct HeadlessPosObjectView
{
	HeadlessPosObjectView() = default;
	HeadlessPosObjectView(const HeadlessPosObjectView &) = delete;
	HeadlessPosObjectView &operator=(const HeadlessPosObjectView &) = delete;

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

	/* the single positioned frame container on page 0 */
	fp_FrameContainer * findFrame() const
	{
		fp_Page *pPage = layout->getNthPage(0);
		if (!pPage)
			return nullptr;
		for (UT_sint32 i = 0; i < pPage->countAboveFrameContainers(); i++)
		{
			fp_FrameContainer *pFC = pPage->getNthAboveFrameContainer(i);
			if (pFC)
				return pFC;
		}
		for (UT_sint32 i = 0; i < pPage->countBelowFrameContainers(); i++)
		{
			fp_FrameContainer *pFC = pPage->getNthBelowFrameContainer(i);
			if (pFC)
				return pFC;
		}
		return nullptr;
	}

	~HeadlessPosObjectView()
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

TFTEST_MAIN("positioned object context menu resolves and offers Image Properties")
{
	XAP_Menu_Factory *pFactory = XAP_App::getApp()->getMenuFactory();
	TFPASS(pFactory != nullptr);
	if (!pFactory)
		return;

	const char *szName = pFactory->FindContextMenu(EV_EMC_POSOBJECT);
	TFPASS(szName != nullptr && strcmp(szName, "ContextPosObjectT") == 0);
	if (!szName)
		return;

	EV_Menu_Layout *pLayout = pFactory->CreateMenuLayout(szName);
	TFPASS(pLayout != nullptr);
	if (!pLayout)
		return;

	bool bFmt = false;
	bool bProps = false;
	bool bSave = false;
	bool bCut = false;
	bool bCopy = false;
	bool bDelete = false;
	for (UT_uint32 i = 0; i < pLayout->getLayoutItemCount(); i++)
	{
		const EV_Menu_LayoutItem *pItem = pLayout->getLayoutItem(i);
		if (!pItem || pItem->getMenuLayoutFlags() != EV_MLF_Normal)
			continue;
		XAP_Menu_Id id = pItem->getMenuId();
		if (id == static_cast<XAP_Menu_Id>(AP_MENU_ID_FMT_POSIMAGE))
			bFmt = true;
		else if (id == static_cast<XAP_Menu_Id>(AP_MENU_ID_FMT_IMGPROPS))
			bProps = true;
		else if (id == static_cast<XAP_Menu_Id>(AP_MENU_ID_FILE_SAVEIMAGE))
			bSave = true;
		else if (id == static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_CUTIMAGE))
			bCut = true;
		else if (id == static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_COPYIMAGE))
			bCopy = true;
		else if (id == static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_DELETEIMAGE))
			bDelete = true;
	}
	/* Format + Save + Cut/Copy/Delete like before plus the new
	 * Image Properties row immediately after the format entry. */
	TFPASS(bFmt && bProps && bSave && bCut && bCopy && bDelete);
	TFPASS(pLayout->getLayoutIndex(static_cast<XAP_Menu_Id>(AP_MENU_ID_FMT_IMGPROPS))
		   == pLayout->getLayoutIndex(static_cast<XAP_Menu_Id>(AP_MENU_ID_FMT_POSIMAGE)) + 1);
	delete pLayout;
}

TFTEST_MAIN("click on a positioned image selects the object and enables the items")
{
	HeadlessPosObjectView hv;
	TFPASS(hv.open("/test/wp/posimage.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());

	fp_FrameContainer *pFC = hv.findFrame();
	TFPASS(pFC != nullptr);
	if (!pFC)
		return;

	/* a click inside the positioned image's box resolves the
	 * positioned-object mouse context */
	fp_Page *pPage = hv.layout->getNthPage(0);
	TFPASS(pPage != nullptr);
	if (!pPage)
		return;
	UT_sint32 xoff = 0, yoff = 0;
	hv.view->getPageScreenOffsets(pPage, xoff, yoff);
	UT_sint32 cx = xoff + pFC->getFullX() + pFC->getFullWidth() / 2;
	UT_sint32 cy = yoff + pFC->getFullY() + pFC->getFullHeight() / 2;
	TFPASS(hv.view->getMouseContext(cx, cy) == EV_EMC_POSOBJECT);

	/* before contextPosObject's activation the gated items
	 * (Properties, Cut/Copy/Delete) must be grey - no image selected,
	 * no frame edit active */
	TFPASS(ap_GetState_InImage(hv.view,
							   static_cast<XAP_Menu_Id>(AP_MENU_ID_FMT_IMGPROPS))
		   == EV_MIS_Gray);
	TFPASS(ap_GetState_InImage(hv.view,
							   static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_CUTIMAGE))
		   == EV_MIS_Gray);

	/* the context method activates the frame under the click (the
	 * last mouse coords it recorded) so the items enable and act on
	 * the right object */
	hv.view->activateFrame();
	TFPASS(hv.view->getFrameEdit()->isActive());
	fl_FrameLayout *pFL = hv.view->getFrameLayout();
	TFPASS(pFL != nullptr);
	if (!pFL)
		return;
	TFPASS(pFL->getFrameType() == FL_FRAME_WRAPPER_IMAGE);

	TFPASS(ap_GetState_InImage(hv.view,
							   static_cast<XAP_Menu_Id>(AP_MENU_ID_FMT_IMGPROPS))
		   == EV_MIS_ZERO);
	TFPASS(ap_GetState_InImage(hv.view,
							   static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_CUTIMAGE))
		   == EV_MIS_ZERO);
	TFPASS(ap_GetState_InImage(hv.view,
							   static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_COPYIMAGE))
		   == EV_MIS_ZERO);
	TFPASS(ap_GetState_InImage(hv.view,
							   static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_DELETEIMAGE))
		   == EV_MIS_ZERO);
}

TFTEST_MAIN("positioned image properties write through the frame AP")
{
	HeadlessPosObjectView hv;
	TFPASS(hv.open("/test/wp/posimage.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());

	fp_FrameContainer *pFC = hv.findFrame();
	TFPASS(pFC != nullptr);
	if (!pFC)
		return;

	/* activate the object the way contextPosObject does */
	fp_Page *pPage = hv.layout->getNthPage(0);
	TFPASS(pPage != nullptr);
	if (!pPage)
		return;
	UT_sint32 xoff = 0, yoff = 0;
	hv.view->getPageScreenOffsets(pPage, xoff, yoff);
	hv.view->getMouseContext(xoff + pFC->getFullX() + pFC->getFullWidth() / 2,
							 yoff + pFC->getFullY() + pFC->getFullHeight() / 2);
	hv.view->activateFrame();
	TFPASS(hv.view->getFrameEdit()->isActive());

	fl_FrameLayout *pFL = hv.view->getFrameLayout();
	TFPASS(pFL != nullptr);
	if (!pFL)
		return;

	/* the fixture's frame carries title/alt - the same reads the
	 * properties dialog does to prime its entries */
	const PP_AttrProp *pAP = nullptr;
	pFL->getAP(pAP);
	TFPASS(pAP != nullptr);
	if (!pAP)
		return;
	const gchar *szVal = nullptr;
	TFPASS(pAP->getAttribute("title", szVal) && strcmp(szVal, "Floating logo") == 0);
	szVal = nullptr;
	TFPASS(pAP->getAttribute("alt", szVal) &&
		   strcmp(szVal, "A red square floating in the page") == 0);

	/* dlgImageProperties delegates active positioned objects to
	 * dlgFmtPosImage, which writes title/alt + size through
	 * setFrameFormat - exercise that apply path */
	const PP_PropertyVector attribs = {
		"title", "Company mark",
		"alt", "Red square logo, positioned"
	};
	const PP_PropertyVector props = {
		"frame-width", "2.5000in"
	};
	hv.view->setFrameFormat(attribs, props, nullptr);

	pAP = nullptr;
	pFL->getAP(pAP);
	TFPASS(pAP != nullptr);
	if (!pAP)
		return;
	szVal = nullptr;
	TFPASS(pAP->getAttribute("title", szVal) && strcmp(szVal, "Company mark") == 0);
	szVal = nullptr;
	TFPASS(pAP->getAttribute("alt", szVal) &&
		   strcmp(szVal, "Red square logo, positioned") == 0);
	szVal = nullptr;
	TFPASS(pAP->getProperty("frame-width", szVal) && strcmp(szVal, "2.5000in") == 0);
}

TFTEST_MAIN("body clicks stay in the text context and keep the items grey")
{
	HeadlessPosObjectView hv;
	TFPASS(hv.open("/test/wp/posimage.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());

	fp_Page *pPage = hv.layout->getNthPage(0);
	TFPASS(pPage != nullptr);
	if (!pPage)
		return;

	/* a click on the body text far from the image frame resolves
	 * plain text and the image items stay disabled */
	UT_sint32 xoff = 0, yoff = 0;
	hv.view->getPageScreenOffsets(pPage, xoff, yoff);
	UT_sint32 mx = xoff + pPage->getWidth() / 2;
	UT_sint32 my = yoff + pPage->getHeight() * 4 / 5;
	TFPASS(hv.view->getMouseContext(mx, my) == EV_EMC_TEXT);
	TFPASS(ap_GetState_InImage(hv.view,
							   static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_DELETEIMAGE))
		   == EV_MIS_Gray);
}
