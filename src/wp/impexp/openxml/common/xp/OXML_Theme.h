/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource
 *
 * Copyright (C) 2007 Philippe Milot <PhilMilot@gmail.com>
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

#ifndef _OXML_THEME_H_
#define _OXML_THEME_H_

// Internal includes
#include <OXML_Types.h>

// External includes
#include <string>
#include <map>
#include <memory>

/* \class OXML_Theme
 * \brief This class holds all the information contained in a document's Theme part.
 * OXML_Theme keeps mappings of the theme's color names and their hex values, as well
 * as mappings between a script name and its associated font face.  OXML_FontManager uses
 * the latter information to determine which font face is to be used for a particular
 * run of text.
*/
class OXML_Theme
{
public:
	OXML_Theme();

	inline std::string getColor(OXML_ColorName name) { return m_colorScheme[name]; }
	inline void setColor (OXML_ColorName name, std::string val) { m_colorScheme[name] = val; }

	std::string getMajorFont(std::string script);
	std::string getMinorFont(std::string script);
	inline void setMajorFont(std::string script, std::string val) { m_majorFontScheme[script] = val; }
	inline void setMinorFont(std::string script, std::string val) { m_minorFontScheme[script] = val; }

	/* a:fmtScheme style lists — indexed by the 1-based position that
	 * wps:style's a:lnRef/a:effectRef @idx refer to (ECMA-376).
	 * color is "#RRGGBB" or "phClr" (placeholder — substituted by the
	 * referencing *Ref element's own color child). */
	struct ThemeShadow {
		double blurPt = 0.0;      // a:outerShdw@blurRad in pt
		double distPt = 0.0;      // a:outerShdw@dist in pt
		int dir = 0;              // a:outerShdw@dir (60000ths of a degree)
		bool rotWithShape = true; // a:outerShdw@rotWithShape
		std::string color;
		double alpha = -1.0;      // a:alpha on the shadow color
	};
	struct ThemeLine {
		double wPt = -1.0;        // a:ln@w in pt
		std::string color;
		std::string dash;         // border style name; "none" = a:noFill
		std::string cmpd;         // a:ln@cmpd (dbl/thickThin/thinThick/tri)
	};
	const ThemeShadow * getEffectShadow(int idx) const;
	const ThemeLine * getLineStyle(int idx) const;
	inline void setEffectShadow(int idx, const ThemeShadow & s) { m_effectStyles[idx] = s; }
	inline void setLineStyle(int idx, const ThemeLine & l) { m_lineStyles[idx] = l; }

private:
	std::string m_colorScheme[12];
	typedef std::map<std::string, std::string> OXML_FontScheme;
	OXML_FontScheme m_majorFontScheme;
	OXML_FontScheme m_minorFontScheme;
	std::map<int, ThemeShadow> m_effectStyles;
	std::map<int, ThemeLine> m_lineStyles;
};

typedef std::shared_ptr<OXML_Theme> OXML_SharedTheme;

#endif //_OXML_THEME_H_

