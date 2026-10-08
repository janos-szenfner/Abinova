/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource
 *
 * Copyright (C) 2008 Firat Kiyak <firatkiyak@gmail.com>
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

#ifndef _OXML_ELEMENT_IMAGE_H_
#define _OXML_ELEMENT_IMAGE_H_

// Internal includes
#include "OXML_Element.h"
#include "OXML_Image.h"
#include "ie_exp_OpenXML.h"

// Abinova includes
#include "ut_types.h"
#include "ut_string.h"
#include "pd_Document.h"

class OXML_Element_Image : public OXML_Element
{
public:
	OXML_Element_Image(const std::string & id);
	virtual ~OXML_Element_Image();

	virtual UT_Error serialize(IE_Exp_OpenXML* exporter) override;
	virtual UT_Error addToPT(PD_Document * pDocument) override;

	/* anchored images inside header/footer parts would emit frame
	 * struxes that hdrftr shadows cannot lay out — a document
	 * section hoists them into the body flow instead (see
	 * OXML_Section::_emitHdrFtrFrames) */
	virtual bool isHdrFtrFrameCandidate() const override
	{
		/* anchored (non-inline) payloads emit a frame; inline
		 * images carry a "height" property and emit in-flow */
		const gchar * v = nullptr;
		return !getId().empty() &&
			   (getProperty("height", v) != UT_OK || !v);
	}
	virtual UT_Error addToPTAsFrame(PD_Document * pDocument) override;

private:
	bool m_hoisted = false;
	UT_Error _emitToPT(PD_Document * pDocument);
	UT_Error _addMediaEmbedToPT(PD_Document * pDocument,
	                            const OXML_SharedImage & poster,
	                            const gchar * szMediaRid);
};

#endif //_OXML_ELEMENT_IMAGE_H_

