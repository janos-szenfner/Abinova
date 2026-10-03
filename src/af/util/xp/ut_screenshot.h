/* AbiSource Application Framework
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

#pragma once

#include "ut_export.h"

#include <string>

/* outcome of an interactive screen-area capture request */
enum UT_ScreenshotResult
{
	UT_SCREENSHOT_OK = 0,		/* outPath holds the captured image file; the
							 * caller owns it and removes it when done */
	UT_SCREENSHOT_CANCELLED,	/* the user dismissed the capture UI */
	UT_SCREENSHOT_UNAVAILABLE	/* no capture mechanism exists on this system */
};

/* true when some capture path exists: the XDG Screenshot portal
 * (xdg-desktop-portal, GNOME/KDE, Wayland and X11) or the
 * gnome-screenshot tool */
ABI_EXPORT bool UT_screenshot_available(void);

/* capture a user-selected screen area; on UT_SCREENSHOT_OK outPath
 * holds the captured PNG. Caller must be on the GUI thread */
ABI_EXPORT UT_ScreenshotResult UT_screenshot_capture_area(std::string & outPath);
