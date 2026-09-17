/* AbiSource Application Framework
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2021-2025 Hubert Figuière
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

#include "ut_types.h"
#include "ut_misc.h"

#include "xap_Preview.h"

class GR_Font;

class ABI_EXPORT XAP_Preview_Zoom : public XAP_Preview
{
public:

	XAP_Preview_Zoom(GR_Graphics * gc);
	virtual ~XAP_Preview_Zoom(void);

	// example placements useful in zoom
	enum tPos: uint8_t { pos_TOP, pos_CENTER, pos_BOTTOM };
	// example fonts useful in zoom (add more later)
	enum tFont: uint8_t { font_NORMAL };

	// data twiddlers
	void	setDrawAtPosition(XAP_Preview_Zoom::tPos pos);
	void	setFont(XAP_Preview_Zoom::tFont f);
	void	setZoomPercent(UT_uint32 percent);

	// set the string you'd like to display
	bool	setString(const char * string);
	bool	setString(UT_UCS4Char * string);

    // where all the zoom-specific drawing happens
	virtual void drawImmediate(const UT_Rect* clip = nullptr) override;

protected:

	XAP_Preview_Zoom::tPos	m_pos;
	XAP_Preview_Zoom::tFont m_previewFont;
	UT_uint32				m_zoomPercent;

	UT_UCS4Char * 			m_string;

	GR_Font *				m_pFont;	// so we can delete it
};
