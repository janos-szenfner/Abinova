/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Application Framework
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2025 Hubert Figuière
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

#pragma once

#include <vector>

#include "ut_compiler.h"

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "ut_types.h"
#include "ut_bytebuf.h"
#include "xap_FakeClipboard.h"
#include "xap_UnixApp.h"
#include "xap_EditMethods.h"
#include "ev_EditMethod.h"

//////////////////////////////////////////////////////////////////

class XAP_UnixClipboard
{
public:
	enum T_AllowGet: uint8_t {
		TAG_ClipboardOnly,
		TAG_PrimaryOnly
	};

	XAP_UnixClipboard(XAP_UnixApp * pUnixApp);
	virtual ~XAP_UnixClipboard();

	void				initialize();
	bool				assertSelection();

	bool				addData(T_AllowGet tTo, const char* format, const void* pData,
								UT_sint32 iNumBytes);

	void				clearData(bool bClipboard, bool bPrimary);
	void			finishedAddingData(void);
	bool				getData(T_AllowGet tFrom, const char** formatList,
								void ** ppData, UT_uint32 * pLen,
								const char **pszFormatFound);

	bool				getTextData(T_AllowGet tFrom, void ** ppData,
									UT_uint32 * pLen);

	bool canPaste(T_AllowGet tFrom) const;

	// called by the GdkContentProvider when a pasting peer requests data
	bool				writeData(const char * mime_type, GOutputStream * stream,
								  bool bPrimary, GError ** error);

protected:

	void				AddFmt(const char * fmt);
	void				deleteFmt(const char * fmt);

 private:

	GdkClipboard * clipboardForTarget(XAP_UnixClipboard::T_AllowGet get) const;

	bool				_getDataFromServer(T_AllowGet tFrom, const char** formatList,
							   void ** ppData, UT_uint32 * pLen,
							   const char **pszFormatFound);
	bool				_getDataFromFakeClipboard(T_AllowGet tFrom, const char** formatList,
								  void ** ppData, UT_uint32 * pLen,
								  const char **pszFormatFound);

	std::vector<const char*>  m_vecFormat_MimeType;

	UT_ByteBuf m_databuf; // for gets only

	XAP_UnixApp *		m_pUnixApp;
	XAP_FakeClipboard	m_fakeClipboard;		// internal clipboard to short-circut the XServer.

	XAP_FakeClipboard       m_fakePrimaryClipboard;

	GdkClipboard * m_clip;
	GdkClipboard * m_primary;
};
