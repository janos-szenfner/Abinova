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
#include "fp_Page.h"
#include "fp_Column.h"
#include "fp_Line.h"
#include "fp_Run.h"
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

#define TFSUITE "core.text.fmt.imageprops"

namespace {

/* Same widget-less stack as fv_MouseContext/fv_FrameContext: painting
 * goes to a private cairo image surface so the full layout machinery
 * works headless. */
struct HeadlessImageView
{
	HeadlessImageView() = default;
	HeadlessImageView(const HeadlessImageView &) = delete;
	HeadlessImageView &operator=(const HeadlessImageView &) = delete;

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

	/* walk the document's blocks looking for the nth image run */
	fp_ImageRun * findImageRun(UT_uint32 nth = 0) const
	{
		fl_BlockLayout *pBlock = layout->findBlockAtPosition(2);
		for (; pBlock; pBlock = pBlock->getNextBlockInDocument())
		{
			for (fp_Run *pRun = pBlock->getFirstRun(); pRun;
				 pRun = pRun->getNextRun())
			{
				if (pRun->getType() == FPRUN_IMAGE && nth-- == 0)
					return static_cast<fp_ImageRun *>(pRun);
			}
		}
		return nullptr;
	}

	~HeadlessImageView()
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

TFTEST_MAIN("image context menu resolves and offers Image Properties")
{
	XAP_Menu_Factory *pFactory = XAP_App::getApp()->getMenuFactory();
	TFPASS(pFactory != nullptr);
	if (!pFactory)
		return;

	const char *szName = pFactory->FindContextMenu(EV_EMC_IMAGE);
	TFPASS(szName != nullptr && strcmp(szName, "ContextImageT") == 0);
	if (!szName)
		return;

	EV_Menu_Layout *pLayout = pFactory->CreateMenuLayout(szName);
	TFPASS(pLayout != nullptr);
	if (!pLayout)
		return;

	bool bProps = false;
	bool bSetPos = false;
	bool bSave = false;
	for (UT_uint32 i = 0; i < pLayout->getLayoutItemCount(); i++)
	{
		const EV_Menu_LayoutItem *pItem = pLayout->getLayoutItem(i);
		if (!pItem || pItem->getMenuLayoutFlags() != EV_MLF_Normal)
			continue;
		XAP_Menu_Id id = pItem->getMenuId();
		if (id == static_cast<XAP_Menu_Id>(AP_MENU_ID_FMT_IMGPROPS))
			bProps = true;
		else if (id == static_cast<XAP_Menu_Id>(AP_MENU_ID_FMT_SETPOSIMAGE))
			bSetPos = true;
		else if (id == static_cast<XAP_Menu_Id>(AP_MENU_ID_FILE_SAVEIMAGE))
			bSave = true;
	}
	/* Image Properties first, like Word's Format Picture at the top
	 * of the image menu, then the original entries unchanged. */
	TFPASS(bProps && bSetPos && bSave);
	TFPASS(pLayout->getLayoutIndex(static_cast<XAP_Menu_Id>(AP_MENU_ID_FMT_IMGPROPS))
		   < pLayout->getLayoutIndex(static_cast<XAP_Menu_Id>(AP_MENU_ID_FMT_SETPOSIMAGE)));
	delete pLayout;
}

TFTEST_MAIN("inline image props read and write through the object AP")
{
	HeadlessImageView hv;
	TFPASS(hv.open("/test/wp/image_props.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());

	fp_ImageRun *pRun = hv.findImageRun();
	TFPASS(pRun != nullptr);
	if (!pRun)
		return;

	/* the fixture's image carries width/height plus title/alt
	 * metadata on its object AP - the same reads
	 * dlgImageProperties does to prime the dialog */
	const PP_AttrProp *pAP = pRun->getSpanAP();
	TFPASS(pAP != nullptr);
	if (!pAP)
		return;

	const gchar *szVal = nullptr;
	TFPASS(pAP->getProperty("width", szVal) && strcmp(szVal, "0.50in") == 0);
	szVal = nullptr;
	TFPASS(pAP->getProperty("height", szVal) && strcmp(szVal, "0.50in") == 0);
	szVal = nullptr;
	TFPASS(pAP->getAttribute("title", szVal) && strcmp(szVal, "AbiSource logo") == 0);
	szVal = nullptr;
	TFPASS(pAP->getAttribute("alt", szVal) && strcmp(szVal, "A red square logo") == 0);

	PT_DocPosition pos = pRun->getBlock()->getPosition() + pRun->getBlockOffset();

	/* the apply path: select the object, then write size + metadata
	 * through the span format channel like the dialog does */
	hv.view->cmdSelect(pos, pos + 1);

	const fp_Run *pSelRun = nullptr;
	PT_DocPosition posSel = hv.view->getSelectedImage(nullptr, &pSelRun);
	TFPASS(posSel == pos);
	TFPASS(pSelRun == pRun);

	const PP_PropertyVector properties = {
		"width", "2.0000in",
		"height", "1.5000in"
	};
	const PP_PropertyVector attribs = {
		"title", "AbiSource logo",
		"alt", "Company logo - decorative"
	};
	TFPASS(hv.view->setCharFormat(properties, attribs));

	/* re-read the object AP: props + attributes both updated */
	pAP = pRun->getSpanAP();
	TFPASS(pAP != nullptr);
	if (!pAP)
		return;
	szVal = nullptr;
	TFPASS(pAP->getProperty("width", szVal) && strcmp(szVal, "2.0000in") == 0);
	szVal = nullptr;
	TFPASS(pAP->getProperty("height", szVal) && strcmp(szVal, "1.5000in") == 0);
	szVal = nullptr;
	TFPASS(pAP->getAttribute("title", szVal) && strcmp(szVal, "AbiSource logo") == 0);
	szVal = nullptr;
	TFPASS(pAP->getAttribute("alt", szVal) && strcmp(szVal, "Company logo - decorative") == 0);
}

TFTEST_MAIN("image lookup fails cleanly on plain text")
{
	HeadlessImageView hv;
	TFPASS(hv.open("/test/wp/BillOfRights.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());

	TFPASS(!hv.view->isImageSelected());
	const fp_Run *pRun = nullptr;
	const char *dataId = nullptr;
	TFPASS(hv.view->getSelectedImage(&dataId, &pRun) == 0);
	TFPASS(pRun == nullptr);
	TFPASS(dataId == nullptr);
}
