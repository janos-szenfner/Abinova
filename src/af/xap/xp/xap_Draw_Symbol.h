/* AbiSource Application Framework
 * Copyright (C) 1998 AbiSource, Inc.
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

class ABI_EXPORT XAP_Draw_Symbol : public XAP_Preview
{
public:

	XAP_Draw_Symbol(GR_Graphics * gc);
	virtual ~XAP_Draw_Symbol();

	// data twiddlers
	void						setSelectedFont(const char *font);
	void						setFontString();
	void						setFontStringarea();
	void						setFontToGC(GR_Graphics *p_gc, UT_uint32 MaxWidthAllowable, UT_uint32 MaxHeightAllowable);
	void						setFontfont(GR_Font * font);
	void						setWindowSize(UT_uint32 width, UT_uint32 height);
	void						setAreaSize(UT_uint32 width, UT_uint32 height);
	void						setAreaGc(GR_Graphics *);
	void						setRow(UT_uint32 row);

	const char*					getSelectedFont() const;
	UT_uint32					getSymbolRows () const;

    // where all the Symbol-specific drawing happens

	virtual void drawImmediate(const UT_Rect* clip = nullptr) override;
	void						drawarea(UT_UCS4Char c, UT_UCS4Char p);

	UT_UCS4Char					calcSymbol(UT_uint32 x, UT_uint32 y);
	UT_UCS4Char                  calcSymbolFromCoords(UT_uint32 x, UT_uint32 y);
	void						setCurrent(UT_UCS4Char c);
	UT_UCS4Char					getCurrent() const
	{ return m_CurrentSymbol; }

	void onLeftButtonDown(UT_sint32 x, UT_sint32 y) override;
	void						calculatePosition(UT_UCS4Char c, UT_uint32 &x, UT_uint32 &y);
	virtual GR_Graphics* getGraphics(void) const override
	{ return m_areagc; }

protected:
	GR_Graphics *               m_areagc;
	GR_Font *			        m_pFont;	// so we can delete it

	UT_uint32                   m_drawWidth;
	UT_uint32                   m_drawHeight;
	UT_uint32                   m_drawareaWidth;
	UT_uint32                   m_drawareaHeight;
	UT_sint32                   m_start_base;
	UT_sint32		    m_start_nb_char;

	UT_UCS4Char					m_CurrentSymbol;
	UT_UCS4Char					m_PreviousSymbol;

private:

	UT_NumberVector				m_vCharSet;
	std::string				m_stFont;
};
