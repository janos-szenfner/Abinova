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
#include "ap_Menu_Functions.h"
#include "ut_types.h"

#include <glib.h>

#include <cstring>
#include <optional>
#include <string>

#define TFSUITE "core.text.fmt.fieldcontext"

namespace {

/* Same widget-less stack as fv_MouseContext/fv_TOCContext: painting
 * goes to a private cairo image surface so the full layout machinery
 * works headless. */
struct HeadlessFieldView
{
	HeadlessFieldView() = default;
	HeadlessFieldView(const HeadlessFieldView &) = delete;
	HeadlessFieldView &operator=(const HeadlessFieldView &) = delete;

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

	/* walk the document's blocks looking for a field run */
	fp_FieldRun * findFieldRun() const
	{
		fl_BlockLayout *pBlock = layout->findBlockAtPosition(2);
		for (; pBlock; pBlock = pBlock->getNextBlockInDocument())
		{
			for (fp_Run *pRun = pBlock->getFirstRun(); pRun;
				 pRun = pRun->getNextRun())
			{
				if (pRun->getType() == FPRUN_FIELD)
					return static_cast<fp_FieldRun *>(pRun);
			}
		}
		return nullptr;
	}

	~HeadlessFieldView()
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

TFTEST_MAIN("field context menu resolves and lists Update/Edit")
{
	XAP_Menu_Factory *pFactory = XAP_App::getApp()->getMenuFactory();
	TFPASS(pFactory != nullptr);
	if (!pFactory)
		return;

	const char *szName = pFactory->FindContextMenu(EV_EMC_FIELD);
	TFPASS(szName != nullptr && strcmp(szName, "ContextField") == 0);
	if (!szName)
		return;

	EV_Menu_Layout *pLayout = pFactory->CreateMenuLayout(szName);
	TFPASS(pLayout != nullptr);
	if (!pLayout)
		return;

	bool bUpdate = false;
	bool bEdit = false;
	bool bCut = false;
	bool bCopy = false;
	bool bPaste = false;
	for (UT_uint32 i = 0; i < pLayout->getLayoutItemCount(); i++)
	{
		const EV_Menu_LayoutItem *pItem = pLayout->getLayoutItem(i);
		if (!pItem || pItem->getMenuLayoutFlags() != EV_MLF_Normal)
			continue;
		if (pItem->getMenuId() == static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_UPDATEFIELD))
			bUpdate = true;
		if (pItem->getMenuId() == static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_FIELD))
			bEdit = true;
		if (pItem->getMenuId() == static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_CUT))
			bCut = true;
		if (pItem->getMenuId() == static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_COPY))
			bCopy = true;
		if (pItem->getMenuId() == static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_PASTE))
			bPaste = true;
	}
	TFPASS(bUpdate);
	TFPASS(bEdit);
	TFPASS(bCut);
	TFPASS(bCopy);
	TFPASS(bPaste);
	delete pLayout;
}

TFTEST_MAIN("click on a field resolves the field context")
{
	HeadlessFieldView hv;
	TFPASS(hv.open("/test/wp/fields.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());

	fp_Page *pPage = hv.layout->getNthPage(0);
	TFPASS(pPage != nullptr);
	if (!pPage)
		return;

	fp_FieldRun *pField = hv.findFieldRun();
	TFPASS(pField != nullptr);
	if (!pField)
		return;

	std::optional<UT_Rect> rect = pField->getScreenRect();
	TFPASS(rect.has_value());
	if (!rect)
		return;

	/* a click on the field run resolves to the field mouse context */
	UT_sint32 cx = rect->left + rect->width / 2;
	UT_sint32 cy = rect->top + rect->height / 2;
	TFPASS(hv.view->getMouseContext(cx, cy) == EV_EMC_FIELD);

	/* the insertion-point context agrees */
	PT_DocPosition pos = pField->getBlock()->getPosition() +
		pField->getBlockOffset();
	hv.view->setPoint(pos);
	TFPASS(hv.view->getFieldRun(pos) == pField);
	/* the point can also sit just after the one-character field
	 * object — getFieldRun probes the neighbouring runs */
	TFPASS(hv.view->getFieldRun(pos + 1) == pField);
	TFPASS(hv.view->getInsertionPointContext(nullptr, nullptr) == EV_EMC_FIELD);
}

TFTEST_MAIN("field actions are gated on a field at the point")
{
	HeadlessFieldView hv;
	TFPASS(hv.open("/test/wp/fields.abw"));
	if (!hv.view)
		return;
	TFPASS(hv.format());

	fp_Page *pPage = hv.layout->getNthPage(0);
	TFPASS(pPage != nullptr);
	if (!pPage)
		return;

	fp_FieldRun *pField = hv.findFieldRun();
	TFPASS(pField != nullptr);
	if (!pField)
		return;

	PT_DocPosition posField = pField->getBlock()->getPosition() +
		pField->getBlockOffset();

	/* with the point on the field the Update/Edit items are enabled
	 * and Update Field re-evaluates the run */
	hv.view->setPoint(posField);
	TFPASS(ap_GetState_FieldOK(hv.view,
		static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_UPDATEFIELD)) == EV_MIS_ZERO);
	TFPASS(ap_GetState_FieldOK(hv.view,
		static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_FIELD)) == EV_MIS_ZERO);
	TFPASS(hv.view->cmdUpdateField());

	/* with the point on ordinary body text the items are greyed and
	 * Update Field is a no-op */
	hv.view->setPoint(posField + 2);
	TFPASS(hv.view->getFieldRun(hv.view->getPoint()) == nullptr);
	TFPASS(ap_GetState_FieldOK(hv.view,
		static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_UPDATEFIELD)) == EV_MIS_Gray);
	TFPASS(!hv.view->cmdUpdateField());
}
