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
#include "gr_UnixCairoGraphics.h"
#include "ev_Menu_Layouts.h"
#include "xap_App.h"
#include "xap_Menu_Layouts.h"
#include "ap_Menu_Id.h"
#include "pp_AttrProp.h"
#include "ut_types.h"

#include <glib.h>

#include <cstring>
#include <vector>

#define TFSUITE "core.text.fmt.framecontext"

namespace {

/* Same widget-less stack as fv_MouseContext/fv_TOCContext: painting
 * goes to a private cairo image surface so the full layout machinery
 * works headless. */
struct HeadlessFrameView
{
	HeadlessFrameView() = default;
	HeadlessFrameView(const HeadlessFrameView &) = delete;
	HeadlessFrameView &operator=(const HeadlessFrameView &) = delete;

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

	~HeadlessFrameView()
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

TFTEST_MAIN("frame context menu resolves and offers Format Text Box")
{
	XAP_Menu_Factory *pFactory = XAP_App::getApp()->getMenuFactory();
	TFPASS(pFactory != nullptr);
	if (!pFactory)
		return;

	const char *szName = pFactory->FindContextMenu(EV_EMC_FRAME);
	TFPASS(szName != nullptr && strcmp(szName, "ContextFrameT") == 0);
	if (!szName)
		return;

	EV_Menu_Layout *pLayout = pFactory->CreateMenuLayout(szName);
	TFPASS(pLayout != nullptr);
	if (!pLayout)
		return;

	bool bFormat = false;
	bool bCut = false;
	bool bCopy = false;
	bool bSelect = false;
	bool bDelete = false;
	for (UT_uint32 i = 0; i < pLayout->getLayoutItemCount(); i++)
	{
		const EV_Menu_LayoutItem *pItem = pLayout->getLayoutItem(i);
		if (!pItem || pItem->getMenuLayoutFlags() != EV_MLF_Normal)
			continue;
		XAP_Menu_Id id = pItem->getMenuId();
		if (id == static_cast<XAP_Menu_Id>(AP_MENU_ID_FMT_FRAME))
			bFormat = true;
		else if (id == static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_CUT_FRAME))
			bCut = true;
		else if (id == static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_COPY_FRAME))
			bCopy = true;
		else if (id == static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_SELECT_FRAME))
			bSelect = true;
		else if (id == static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_DELETEFRAME))
			bDelete = true;
	}
	/* Format first, like Word's Format Shape entry at the top of the
	 * text box menu, then the original four entries unchanged. */
	TFPASS(bFormat && bCut && bCopy && bSelect && bDelete);
	TFPASS(pLayout->getLayoutIndex(static_cast<XAP_Menu_Id>(AP_MENU_ID_FMT_FRAME))
		   < pLayout->getLayoutIndex(static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_CUT_FRAME)));
	delete pLayout;
}

TFTEST_MAIN("Format Text Box apply path edits real frame props")
{
	HeadlessFrameView hv;
	TFPASS(hv.open("/test/wp/frame.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());

	fp_Page *pPage = hv.layout->getNthPage(0);
	TFPASS(pPage != nullptr);
	if (!pPage)
		return;

	std::vector<fp_FrameContainer *> frames;
	for (UT_sint32 i = 0; i < pPage->countAboveFrameContainers(); i++)
		frames.push_back(pPage->getNthAboveFrameContainer(i));
	for (UT_sint32 i = 0; i < pPage->countBelowFrameContainers(); i++)
		frames.push_back(pPage->getNthBelowFrameContainer(i));
	TFPASS(frames.size() == 2);

	/* pick the smaller frame (2in x 1in text box) and put the
	 * insertion point inside it the way contextFrame's IP warp does */
	fp_FrameContainer *pFC = frames[0];
	if (pFC->getFullWidth() > frames[1]->getFullWidth())
		pFC = frames[1];
	UT_sint32 xoff = 0, yoff = 0;
	hv.view->getPageScreenOffsets(pPage, xoff, yoff);
	hv.view->warpInsPtToXY(xoff + pFC->getFullX() + pFC->getFullWidth() / 2,
						   yoff + pFC->getFullY() + pFC->getFullHeight() / 2,
						   true);

	fl_FrameLayout *pFL = hv.view->getFrameLayout();
	TFPASS(pFL != nullptr);
	if (!pFL)
		return;
	TFPASS(pFL->getFrameType() == FL_FRAME_TEXTBOX_TYPE);

	/* the dialog writes title/alt attributes plus size/wrap/position
	 * properties through setFrameFormat — exercise the same apply
	 * path and prove the props land on the frame strux */
	const PP_PropertyVector attribs = {
		"title", "Sidebar frame",
		"alt", "Decorative text box"
	};
	const PP_PropertyVector props = {
		"frame-width", "3.0000in",
		"frame-height", "2.0000in",
		"wrap-mode", "wrapped-both",
		"tight-wrap", "1"
	};
	hv.view->setFrameFormat(attribs, props, nullptr);

	const PP_AttrProp *pAP = nullptr;
	pFL->getAP(pAP);
	TFPASS(pAP != nullptr);
	if (!pAP)
		return;

	const gchar *szVal = nullptr;
	TFPASS(pAP->getAttribute("title", szVal) && strcmp(szVal, "Sidebar frame") == 0);
	szVal = nullptr;
	TFPASS(pAP->getAttribute("alt", szVal) && strcmp(szVal, "Decorative text box") == 0);
	szVal = nullptr;
	TFPASS(pAP->getProperty("frame-width", szVal) && strcmp(szVal, "3.0000in") == 0);
	szVal = nullptr;
	TFPASS(pAP->getProperty("wrap-mode", szVal) && strcmp(szVal, "wrapped-both") == 0);
	szVal = nullptr;
	TFPASS(pAP->getProperty("tight-wrap", szVal) && strcmp(szVal, "1") == 0);

	/* the layout picked the new values up too */
	TFPASS(pFL->getFrameWrapMode() == FL_FRAME_WRAPPED_BOTH_SIDES);
	TFPASS(pFL->isTightWrap());
}
