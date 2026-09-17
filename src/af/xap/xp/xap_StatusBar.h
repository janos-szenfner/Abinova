/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Program Utilities
 *
 * Copyright (C) 2002 Francis James Franklin <fjf@alinameridon.com>
 * Copyright (C) 2002 AbiSource, Inc.
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

class ABI_EXPORT XAP_StatusBar
{
public:
	static void setStatusBar (XAP_StatusBar * statusbar);
	static void unsetStatusBar (XAP_StatusBar * statusbar);

	/* These have subtly different behaviours. message() will write to
	 * both status bars (if set); debugmsg() will write only to the
	 * second (if set).
	 */
	static void message (const char * pbuf, bool urgent = false);
	static void debugmsg (const char * pbuf, bool urgent = false);

	virtual void statusMessage (const char * pbuf, bool urgent = false) = 0;

	virtual ~XAP_StatusBar () { }
};
