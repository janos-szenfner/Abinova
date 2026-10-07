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

/* COV15 — unit coverage for the PX_ChangeRecord subclasses that no
 * piece-table path ever instantiates through undo (data items, doc
 * props, object changes, style add/remove).  Each record is
 * self-contained: construct it, verify the stored fields, reverse()
 * it, and verify the inverse.
 */

#include <memory>

#include "tf_test.h"
#include "px_ChangeRecord.h"
#include "px_CR_DataItem.h"
#include "px_CR_DocProp.h"
#include "px_CR_ObjectChange.h"
#include "px_CR_Style.h"
#include "px_CR_Glob.h"

#define TFSUITE "core.text.ptbl.changerecord"

TFTEST_MAIN("PX_ChangeRecord subclass reverse()")
{
	// DataItem — self-inverse record carrying an xid
	{
		PX_ChangeRecord_DataItem cr(PX_ChangeRecord::PXT_CreateDataItem,
									17, 4, 99);
		TFPASS(cr.getType() == PX_ChangeRecord::PXT_CreateDataItem);
		TFPASS(cr.getPosition() == 17);
		TFPASS(cr.getIndexAP() == 4);
		TFPASS(cr.getXID() == 99);
		TFPASS(cr.getRevType() == PX_ChangeRecord::PXT_CreateDataItem);

		std::unique_ptr<PX_ChangeRecord> rev(cr.reverse());
		TFPASS(rev.get() != nullptr);
		TFPASS(rev->getType() == PX_ChangeRecord::PXT_CreateDataItem);
		TFPASS(rev->getPosition() == 17);
		TFPASS(rev->getXID() == 99);
	}

	// DocProp — self-inverse
	{
		PX_ChangeRecord_DocProp cr(PX_ChangeRecord::PXT_ChangeDocProp,
								   0, 12, 7);
		TFPASS(cr.getType() == PX_ChangeRecord::PXT_ChangeDocProp);
		TFPASS(cr.getIndexAP() == 12);

		std::unique_ptr<PX_ChangeRecord> rev(cr.reverse());
		TFPASS(rev.get() != nullptr);
		TFPASS(rev->getType() == PX_ChangeRecord::PXT_ChangeDocProp);
		TFPASS(rev->getIndexAP() == 12);
	}

	// ObjectChange — self-inverse, keeps object type + block offset
	{
		PX_ChangeRecord_ObjectChange cr(PX_ChangeRecord::PXT_ChangeObject,
										31, 5, 9, PTO_Image, 3, false);
		TFPASS(cr.getType() == PX_ChangeRecord::PXT_ChangeObject);
		TFPASS(cr.getObjectType() == PTO_Image);
		TFPASS(cr.getOldIndexAP() == 5);
		TFPASS(cr.getBlockOffset() == 3);
		TFPASS(!cr.isRevisionDelete());
		cr.AdjustBlockOffset(8);
		TFPASS(cr.getBlockOffset() == 8);

		std::unique_ptr<PX_ChangeRecord> rev(cr.reverse());
		TFPASS(rev.get() != nullptr);
		TFPASS(rev->getType() == PX_ChangeRecord::PXT_ChangeObject);
		PX_ChangeRecord_ObjectChange * ro =
			static_cast<PX_ChangeRecord_ObjectChange *>(rev.get());
		TFPASS(ro->getObjectType() == PTO_Image);
		TFPASS(ro->getBlockOffset() == 8);
	}

	// AddStyle reverses to RemoveStyle and back
	{
		PX_ChangeRecord_AddStyle cr(PX_ChangeRecord::PXT_AddStyle,
									3, 44, 55);
		TFPASS(cr.getRevType() == PX_ChangeRecord::PXT_RemoveStyle);
		std::unique_ptr<PX_ChangeRecord> rev(cr.reverse());
		TFPASS(rev.get() != nullptr);
		TFPASS(rev->getType() == PX_ChangeRecord::PXT_RemoveStyle);
		TFPASS(rev->getPosition() == 3);
		TFPASS(rev->getIndexAP() == 44);
		TFPASS(rev->getXID() == 55);

		std::unique_ptr<PX_ChangeRecord> back(rev->reverse());
		TFPASS(back.get() != nullptr);
		TFPASS(back->getType() == PX_ChangeRecord::PXT_AddStyle);
	}
	{
		PX_ChangeRecord_RemoveStyle cr(PX_ChangeRecord::PXT_RemoveStyle,
									   9, 10, 11);
		TFPASS(cr.getRevType() == PX_ChangeRecord::PXT_AddStyle);
		std::unique_ptr<PX_ChangeRecord> rev(cr.reverse());
		TFPASS(rev.get() != nullptr);
		TFPASS(rev->getType() == PX_ChangeRecord::PXT_AddStyle);
		TFPASS(rev->getPosition() == 9);
	}

	// Glob — reverses the multi-step/user-atomic bracket flags
	{
		PX_ChangeRecord_Glob beg(PX_ChangeRecord::PXT_GlobMarker,
								 PX_ChangeRecord_Glob::PXF_MultiStepStart);
		std::unique_ptr<PX_ChangeRecord> end(beg.reverse());
		TFPASS(end.get() != nullptr);
		TFPASS(end->getType() == PX_ChangeRecord::PXT_GlobMarker);
		TFPASS(static_cast<PX_ChangeRecord_Glob *>(end.get())->getFlags()
			   == PX_ChangeRecord_Glob::PXF_MultiStepEnd);

		PX_ChangeRecord_Glob uend(PX_ChangeRecord::PXT_GlobMarker,
								  PX_ChangeRecord_Glob::PXF_UserAtomicEnd);
		std::unique_ptr<PX_ChangeRecord> ubeg(uend.reverse());
		TFPASS(static_cast<PX_ChangeRecord_Glob *>(ubeg.get())->getFlags()
			   == PX_ChangeRecord_Glob::PXF_UserAtomicStart);

		PX_ChangeRecord_Glob nil(PX_ChangeRecord::PXT_GlobMarker,
								 PX_ChangeRecord_Glob::PXF_Null);
		std::unique_ptr<PX_ChangeRecord> rnil(nil.reverse());
		TFPASS(static_cast<PX_ChangeRecord_Glob *>(rnil.get())->getFlags()
			   == PX_ChangeRecord_Glob::PXF_Null);
	}

	// base-class member bits used by undo bookkeeping
	{
		PX_ChangeRecord cr(PX_ChangeRecord::PXT_ChangePoint, 5, 2, 41);
		TFPASS(cr.getPosition() == 5);
		TFPASS(cr.getIndexAP() == 2);
		TFPASS(cr.getXID() == 41);
		cr.setPersistance(true);
		TFPASS(cr.getPersistance());
		cr.setPersistance(false);
		TFPASS(!cr.getPersistance());
		cr.setAdjustment(-7);
		TFPASS(cr.getAdjustment() == -7);
	}
}
