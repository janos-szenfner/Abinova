/* Abinova
 * Copyright (C) 2025 AbiSource
 * Copyright (C) 2025-2026 Abinova contributors
 *
 * Built-in MathML/LaTeX equation renderer for Abinova, replacing the
 * removed lasem-based mathview plugin. Renders through
 * GR_MathTypesetter (pure Cairo) so equations work on every build.
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

#pragma once

#include "gr_EmbedManager.h"
#include "gr_MathTypesetter.h"
#include "ut_string_class.h"
#include "ut_vector.h"

class AD_Document;
class fp_Run;

class ABI_EXPORT GR_GtkMathManager : public GR_EmbedManager
{
public:
	GR_GtkMathManager(GR_Graphics * pG);
	virtual ~GR_GtkMathManager();

	virtual GR_EmbedManager * create(GR_Graphics * pG) override;
	virtual const char *   getObjectType(void) const override;
	virtual const char *   getMimeType(void) const override;
	virtual const char *   getMimeTypeDescription(void) const override;
	virtual const char *   getMimeTypeSuffix(void) const override;
	virtual UT_sint32      makeEmbedView(AD_Document * pDoc, UT_uint32 api,
	                                     const char * szDataID) override;
	virtual void           loadEmbedData(UT_sint32 uid) override;
	virtual void           releaseEmbedView(UT_sint32 uid) override;
	virtual void           initializeEmbedView(UT_sint32 uid) override;
	virtual void           setColor(UT_sint32 uid, const UT_RGBColor & c) override;
	virtual bool           setFont(UT_sint32 uid, const GR_Font * pFont) override;
	virtual void           setDefaultFontSize(UT_sint32 uid, UT_sint32 iSize) override;
	virtual void           setDisplayMode(UT_sint32 uid, AbiDisplayMode mode) override;
	virtual UT_sint32      getWidth(UT_sint32 uid) override;
	virtual UT_sint32      getAscent(UT_sint32 uid) override;
	virtual UT_sint32      getDescent(UT_sint32 uid) override;
	virtual void           render(UT_sint32 uid, UT_Rect & rec) override;
	virtual void           makeSnapShot(UT_sint32 uid, UT_Rect & rec) override;
	virtual bool           isDefault(void) override;
	virtual bool           isEdittable(UT_sint32 uid) override;
	virtual bool           modify(UT_sint32 uid) override;
	virtual bool           convert(UT_uint32 iConvType,
	                               const UT_ConstByteBufPtr & pFrom,
	                               const UT_ByteBufPtr & pTo) override;
	virtual void           updateData(UT_sint32 uid, UT_sint32 api) override;
	virtual void           setRun(UT_sint32 uid, fp_Run * run) override;
	virtual EV_EditMouseContext getContextualMenu(void) const override;

private:
	struct MathItem {
		GR_MathTypesetter ts;
		double      baseSizePt = 12.0;
		double      r = 0, g = 0, b = 0;
		bool        display = true;
		bool        dirty = true;
		bool        hasSnap = false;
		std::string family;
		std::string dataID;
		UT_uint32   api = 0;
		fp_Run *    run = nullptr;
	};

	MathItem *          _item(UT_sint32 uid);
	void                _relayout(MathItem *it);
	UT_sint32           _toLU(double pt) const;
	UT_GenericVector<MathItem *> m_items;
	AD_Document *       m_pDoc;
};
