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

#ifndef AP_FMTINSPECT_H
#define AP_FMTINSPECT_H

#include <string>
#include <vector>

class FV_View;

/* one headed section of the formatting report */
struct AP_FmtReportSection
{
	std::string					heading;
	std::vector<std::string>	lines;
};

/* Word-style Reveal Formatting (Shift+F1) report builder.  Pure xp:
 * reads the style chain, the direct-formatting layers and the layout
 * context at the caret and returns them as headed text sections.
 * The docked Formatting pane renders the sections as widgets; the
 * control socket serializes them via toText(). */
class AP_FmtInspect
{
public:
	static void			buildReport(FV_View * pView,
									std::vector<AP_FmtReportSection> & out);
	static std::string	toText(const std::vector<AP_FmtReportSection> & secs);
};

#endif /* AP_FMTINSPECT_H */
