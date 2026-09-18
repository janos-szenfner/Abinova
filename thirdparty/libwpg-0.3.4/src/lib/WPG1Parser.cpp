/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* libwpg
 * Version: MPL 2.0 / LGPLv2.1+
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Major Contributor(s):
 * Copyright (C) 2006 Ariya Hidayat (ariya@kde.org)
 * Copyright (C) 2005 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Oude Kotte (marc@solcon.nl)
 *
 * For minor contributions see the git repository.
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU Lesser General Public License Version 2.1 or later
 * (LGPLv2.1+), in which case the provisions of the LGPLv2.1+ are
 * applicable instead of those above.
 *
 * For further information visit http://libwpg.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#include "WPG1Parser.h"

#include <array>
#include <cassert>
#include <utility>

#include <librevenge/librevenge.h>

// #include <libwpd/libwpd.h>

#include <libwpg/libwpg.h>

// #include "WPGTextDataHandler.h"
#include "libwpg_utils.h"

static const unsigned char defaultWPG1PaletteRed[] =
{
	0x00, 0x00, 0x00, 0x00, 0x7F, 0x7F, 0x7F, 0x7F,
	0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x14, 0x20, 0x2C, 0x38, 0x45, 0x51, 0x61,
	0x71, 0x82, 0x92, 0xA2, 0xB6, 0xCB, 0xE3, 0xFF,
	0x00, 0x41, 0x7D, 0xBE, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBE, 0x7D, 0x41,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x7D, 0x9E, 0xBE, 0xDF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xDF, 0xBE, 0x9E,
	0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D,
	0xB6, 0xC7, 0xDB, 0xEB, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEB, 0xDB, 0xC7,
	0xB6, 0xB6, 0xB6, 0xB6, 0xB6, 0xB6, 0xB6, 0xB6,
	0x00, 0x1C, 0x38, 0x55, 0x71, 0x71, 0x71, 0x71,
	0x71, 0x71, 0x71, 0x71, 0x71, 0x55, 0x38, 0x1C,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x38, 0x45, 0x55, 0x61, 0x71, 0x71, 0x71, 0x71,
	0x71, 0x71, 0x71, 0x71, 0x71, 0x61, 0x55, 0x45,
	0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,
	0x51, 0x59, 0x61, 0x69, 0x71, 0x71, 0x71, 0x71,
	0x71, 0x71, 0x71, 0x71, 0x71, 0x69, 0x61, 0x59,
	0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51,
	0x00, 0x10, 0x20, 0x30, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x30, 0x20, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x20, 0x28, 0x30, 0x38, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x38, 0x30, 0x28,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x3C, 0x30, 0x34, 0x3C, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x3C, 0x34, 0x30,
	0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x2C,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char defaultWPG1PaletteGreen[] =
{
	0x00, 0x00, 0x7F, 0x7F, 0x00, 0x00, 0x3F, 0x7F,
	0xC0, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
	0x00, 0x14, 0x20, 0x2C, 0x38, 0x45, 0x51, 0x61,
	0x71, 0x82, 0x92, 0xA2, 0xB6, 0xCB, 0xE3, 0xFF,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x41, 0x7D, 0xBE, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBE, 0x7D, 0x41,
	0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D,
	0x7D, 0x9E, 0xBE, 0xDF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xDF, 0xBE, 0x9E,
	0xB6, 0xB6, 0xB6, 0xB6, 0xB6, 0xB6, 0xB6, 0xB6,
	0xB6, 0xC7, 0xDB, 0xEB, 0xFF, 0xFF, 0xFF, 0xFF,
	0xDF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEB, 0xDB, 0xC7,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x1C, 0x38, 0x55, 0x71, 0x71, 0x71, 0x71,
	0x71, 0x71, 0x71, 0x71, 0x71, 0x55, 0x38, 0x1C,
	0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,
	0x38, 0x45, 0x55, 0x61, 0x71, 0x71, 0x71, 0x71,
	0x71, 0x71, 0x71, 0x71, 0x71, 0x61, 0x55, 0x45,
	0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51,
	0x51, 0x59, 0x61, 0x69, 0x71, 0x71, 0x71, 0x71,
	0x71, 0x71, 0x71, 0x71, 0x71, 0x69, 0x61, 0x59,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x10, 0x20, 0x30, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x30, 0x20, 0x10,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x28, 0x30, 0x38, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x38, 0x30, 0x28,
	0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C,
	0x3C, 0x30, 0x34, 0x3C, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x3C, 0x34, 0x30,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char defaultWPG1PaletteBlue[] =
{
	0x00, 0x7F, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x7F,
	0xC0, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0x14, 0x20, 0x2C, 0x38, 0x45, 0x51, 0x61,
	0x71, 0x82, 0x92, 0xA2, 0xB6, 0xCB, 0xE3, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBE, 0x7D, 0x41,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x41, 0x7D, 0xBE, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xDF, 0xBE, 0x9E,
	0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D,
	0x7D, 0x9E, 0xBE, 0xDF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEB, 0xDB, 0xC7,
	0xB6, 0xB6, 0xB6, 0xB6, 0xB6, 0xB6, 0xB6, 0xB6,
	0xB6, 0xC7, 0xDB, 0xEB, 0xFF, 0xFF, 0xFF, 0xFF,
	0x71, 0x71, 0x71, 0x71, 0x71, 0x55, 0x38, 0x1C,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x1C, 0x38, 0x55, 0x71, 0x71, 0x71, 0x71,
	0x71, 0x71, 0x71, 0x71, 0x71, 0x61, 0x55, 0x45,
	0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,
	0x38, 0x45, 0x55, 0x61, 0x71, 0x71, 0x71, 0x71,
	0x71, 0x71, 0x71, 0x71, 0x71, 0x69, 0x61, 0x59,
	0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51,
	0x51, 0x59, 0x61, 0x69, 0x71, 0x71, 0x71, 0x71,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x30, 0x20, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x10, 0x20, 0x30, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x38, 0x30, 0x28,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x28, 0x30, 0x38, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x3C, 0x34, 0x30,
	0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C,
	0x2C, 0x30, 0x34, 0x3C, 0x41, 0x41, 0x41, 0x41,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


WPG1Parser::WPG1Parser(librevenge::RVNGInputStream *input, librevenge::RVNGDrawingInterface *painter):
	WPGXParser(input, painter),
	m_recordLength(0), m_recordEnd(0),
	m_groupEnd(),
	m_success(true), m_exit(false), m_graphicsStarted(false),
	m_width(0), m_height(0), m_style(),
	m_penForeColor(0,0,0),
	m_penBackColor(0xff,0xff,0xff),
	m_brushForeColor(0,0,0),
	m_brushBackColor(0xff,0xff,0xff),
	m_dashArray(), m_gradient(),
	m_fontColor(0, 0, 0),
	m_fontSize(0),
	m_avgCharWidth(0),
	m_textAlign(0)
{
	m_style.insert("draw:fill", "solid");
	m_style.insert("svg:stroke-color", m_penForeColor.getColorString());
	m_style.insert("svg:stroke-opacity", m_penForeColor.getOpacity(), librevenge::RVNG_PERCENT);
	m_style.insert("draw:fill-color", m_brushForeColor.getColorString());
	m_style.insert("draw:opacity", m_brushForeColor.getOpacity(), librevenge::RVNG_PERCENT);
}

bool WPG1Parser::parse()
{
	typedef void (WPG1Parser::*Method)();

	struct RecordHandler
	{
		int type;
		const char *name;
		Method handler;
	};

	static const struct RecordHandler handlers[] =
	{
		{ 0x01, "Fill Attributes",           &WPG1Parser::handleFillAttributes },
		{ 0x02, "Line Attributes",           &WPG1Parser::handleLineAttributes },
		{ 0x03, "Marker Atttibutes",         nullptr },
		{ 0x04, "Polymarker",                nullptr },
		{ 0x05, "Line",                      &WPG1Parser::handleLine },
		{ 0x06, "Polyline",                  &WPG1Parser::handlePolyline },
		{ 0x07, "Rectangle",                 &WPG1Parser::handleRectangle },
		{ 0x08, "Polygon",                   &WPG1Parser::handlePolygon },
		{ 0x09, "Ellipse",                   &WPG1Parser::handleEllipse },
		{ 0x0a, "Reserved",                  nullptr },
		{ 0x0b, "Bitmap (Type 1)",           &WPG1Parser::handleBitmapTypeOne },
		{ 0x0c, "Graphics Text (Type 1)",    &WPG1Parser::handleGraphicsTextTypeOne },
		{ 0x0d, "Graphics Text Attributes",  &WPG1Parser::handleGraphicsTextAttributes },
		{ 0x0e, "Colormap",                  &WPG1Parser::handleColormap },
		{ 0x0f, "Start WPG",                 &WPG1Parser::handleStartWPG },
		{ 0x10, "End WPG",                   &WPG1Parser::handleEndWPG },
		{ 0x11, "Postscript Data (Type 1)",  &WPG1Parser::handlePostscriptTypeOne },
		{ 0x12, "Output Attributes",         nullptr },
		{ 0x13, "Curved Polyline",           &WPG1Parser::handleCurvedPolyline },
		{ 0x14, "Bitmap (Type 2)",           &WPG1Parser::handleBitmapTypeTwo },
		{ 0x15, "Start Group",               &WPG1Parser::handleGroup },
		{ 0x16, "Start Chart",               nullptr },
		{ 0x17, "Planperfect Data",          nullptr },
		{ 0x18, "Graphics Text (Type 2)",    &WPG1Parser::handleGraphicsTextTypeTwo },
		{ 0x19, "PageFormat",                &WPG1Parser::handlePageFormat },
		{ 0x1a, "Graphics Text (Type 3)",    nullptr },
		{ 0x1b, "Postscript Data (Type 2)",  &WPG1Parser::handlePostscriptTypeTwo },
		{ 0x00, nullptr, nullptr } // end marker
	};

	// initialization
	m_recordLength = 0;
	m_recordEnd = 0;
	m_success = true;
	m_exit = false;
	m_graphicsStarted = false;

	// default style
	m_penForeColor = libwpg::WPGColor(0,0,0);
	m_penBackColor = libwpg::WPGColor(0,0,0);
	m_style.insert("svg:stroke-width", 0.0);
	m_style.insert("draw:stroke", "solid");
	m_dashArray = libwpg::WPGDashArray();
	m_brushForeColor = libwpg::WPGColor(0,0,0);
	m_brushBackColor = libwpg::WPGColor(0,0,0);
	m_style.insert("svg:stroke-color", m_penForeColor.getColorString());
	m_style.insert("svg:stroke-opacity", m_penForeColor.getOpacity(), librevenge::RVNG_PERCENT);
	m_style.insert("draw:fill-color", m_brushForeColor.getColorString());
	m_style.insert("draw:opacity", m_brushForeColor.getOpacity(), librevenge::RVNG_PERCENT);
	resetPalette();

	while (!m_input->isEnd())
	{
#ifdef DEBUG
		long recordPos = m_input->tell();
#endif
		int recordType = readU8();
		if (recordType == 0)
			break;
		unsigned recordLength = readVariableLengthInteger();
		const unsigned long maxLength = libwpg::getRemainingLength(m_input) + 1;
		if (recordLength > maxLength)
			recordLength = unsigned(maxLength);
		m_recordLength = (int) recordLength;
		m_recordEnd = m_input->tell() + m_recordLength - 1;
		if (m_recordEnd < 0)
		{
			m_recordEnd = 0;
			m_recordLength = 0;
		}

		// search function to handler this record
		int index = -1;
		for (int i = 0; (index < 0) && handlers[i].name; i++)
			if (handlers[i].type == recordType)
				index = i;

		// close finished group, if any
		closeGroup();

		WPG_DEBUG_MSG(("\n"));
		if (index < 0)
			WPG_DEBUG_MSG(("Unknown record type 0x%02x at %li  size %d\n",
			               recordType, recordPos, m_recordLength));
		else
		{
			Method recordHandler = handlers[index].handler;
			if (!recordHandler)
				WPG_DEBUG_MSG(("Record '%s' (ignored) type 0x%02x at %li  size %d\n",
				               handlers[index].name, recordType, recordPos, m_recordLength));
			else
			{
				WPG_DEBUG_MSG(("Record '%s' type 0x%02x at %li  size %d\n",
				               handlers[index].name, recordType, recordPos, m_recordLength));

				// invoke the handler for this record
				(this->*recordHandler)();
			}
		}

		WPG_DEBUG_MSG(("Current stream position: %li\n", m_input->tell()));

		if (m_exit) break;

		m_input->seek(m_recordEnd+1, librevenge::RVNG_SEEK_SET);
	}

	if (!m_exit)
		handleEndWPG();

	return m_success;
}

void WPG1Parser::handleStartWPG()
{
	if (m_graphicsStarted)
	{
		handleEndWPG();
		return;
	}
	m_input->seek(2, librevenge::RVNG_SEEK_CUR);
	m_width = readU16();
	m_height = readU16();

	librevenge::RVNGPropertyList propList;
	propList.insert("svg:width", (double)m_width/1200.0);
	propList.insert("svg:height", (double)m_height/1200.0);
	m_painter->startDocument(librevenge::RVNGPropertyList());
	m_painter->startPage(propList);
	m_graphicsStarted = true;

	WPG_DEBUG_MSG(("StartWPG\n"));
}

void WPG1Parser::handleEndWPG()
{
	if (!m_graphicsStarted)
		return;

	closeGroup(true);
	m_painter->endPage();
	m_painter->endDocument();
	m_exit = true;

	WPG_DEBUG_MSG(("EndWPG\n"));
}

void WPG1Parser::handlePageFormat()
{
	if (!m_graphicsStarted)
		return;

	m_input->seek(3, librevenge::RVNG_SEEK_CUR);

	const unsigned bgColor = readU8();
	const unsigned fgColor = readU8();

	m_input->seek(6, librevenge::RVNG_SEEK_CUR);

	const unsigned leftMargin = readU16();
	const unsigned rightMargin = readU16();
	const unsigned topMargin = readU16();
	const unsigned bottomMargin = readU16();

	m_input->seek(2, librevenge::RVNG_SEEK_CUR);

	const unsigned width = readU16();
	const unsigned height = readU16();

#if 0
	m_input->seek(1, librevenge::RVNG_SEEK_CUR);

	unsigned formatNameLen = readU8();
	const unsigned long pos = m_input->tell();
	std::string formatName;
	formatName.reserve(formatNameLen);
	for (; formatNameLen > 0 && !m_input->isEnd(); --formatNameLen)
		formatName.push_back(readU8());

	m_input->seek(pos + 46, librevenge::RVNG_SEEK_SET);
#else
	m_input->seek(48, librevenge::RVNG_SEEK_CUR);
#endif

	const unsigned gridXStep = readU16();
	const unsigned gridYStep = readU16();
	const unsigned gridInterval = readU8();

	WPG_DEBUG_MSG(("Page format\n"));
	WPG_DEBUG_MSG(("    Background color index: %u\n", bgColor));
	WPG_DEBUG_MSG(("    Foreground color index: %u\n", fgColor));
	WPG_DEBUG_MSG(("               Left margin: %u\n", leftMargin));
	WPG_DEBUG_MSG(("              Right margin: %u\n", rightMargin));
	WPG_DEBUG_MSG(("                Top margin: %u\n", topMargin));
	WPG_DEBUG_MSG(("             Bottom margin: %u\n", bottomMargin));
	WPG_DEBUG_MSG(("                Page width: %u\n", width));
	WPG_DEBUG_MSG(("               Page height: %u\n", height));
	// WPG_DEBUG_MSG(("         Page format name?: %s\n", formatName.c_str()));
	WPG_DEBUG_MSG(("               Grid X Step: %u\n", gridXStep));
	WPG_DEBUG_MSG(("               Grid Y Step: %u\n", gridYStep));
	WPG_DEBUG_MSG(("     Grid Display Interval: %u\n", gridInterval));
}

void WPG1Parser::handleColormap()
{
	if (!m_graphicsStarted)
		return;
	unsigned startIndex = readU16();
	unsigned numEntries = readU16();
	if (startIndex > 255 || numEntries > 256 || startIndex + numEntries > 256)
		return;

	WPG_DEBUG_MSG(("Colormap\n"));
	for (unsigned int i = 0; i < numEntries; i++)
	{
		unsigned char red = readU8();
		unsigned char green = readU8();
		unsigned char blue = readU8();
		libwpg::WPGColor color(red, green, blue);
		m_colorPalette[int(startIndex+i)] = color;
		WPG_DEBUG_MSG(("Index#%d: RGB %s\n", startIndex+i, color.getColorString().cstr()));
	}
}

void WPG1Parser::handleFillAttributes()
{
	if (!m_graphicsStarted)
		return;
	unsigned char style = readU8();
	unsigned char color = readU8();

	if (style == 0)
		m_style.insert("draw:fill", "none");
	if (style == 1)
		m_style.insert("draw:fill", "solid");

	m_brushForeColor = m_colorPalette[color];
	m_style.insert("draw:fill-color", m_brushForeColor.getColorString());
	m_style.insert("draw:opacity", m_brushForeColor.getOpacity(), librevenge::RVNG_PERCENT);

	WPG_DEBUG_MSG(("Fill Attributes\n"));
	WPG_DEBUG_MSG(("         Fill style: %d\n", style));
	WPG_DEBUG_MSG(("   Fill color index: %d\n", color));
}

static const std::array<libwpg::WPGDashArray, 16> &getLineStyles()
{
	// NOTE: The lengths are in pixels and have been measured by a software ruler
	// on screen. Actual length of the solid line is 0.1 in.
	// TODO: It doesn't look quite right in LibreOffice.
	static const int dashes[] =
	{
		1,   85,  0,                    // style #0 (no line)
		1,   85,  0,                    // style #1 (solid)
		1,   20,  5,                    // style #2
		1,    4,  4,                    // style #3
		2,   12,  5,  4,  5,            // style #4
		1,   12, 12,                    // style #5
		3,    9,  4,  2,  4,  2,  4,    // style #6
		1,    7,  7,                    // style #7
		2,   12,  4,  5,  4,            // style #8
		1,   22,  4,                    // style #9
		1,    2,  2,                    // style #10
		1,    2,  4,                    // style #11
		1,    6,  6,                    // style #12
		2,   13,  4,  5,  4,            // style #13
		2,   16,  4,  2,  4,            // style #14
		2,   13,  3,  3,  3,            // style #15
	};

	static std::array<libwpg::WPGDashArray, 16> lineStyles;
	static bool initialized = false;

	if (!initialized)
	{
		for (size_t i = 0, style = 0; i < sizeof(dashes) / sizeof(dashes[0]); style++)
		{
			size_t segments = 2 * dashes[i++];
			for (size_t j = 0; j < segments; j++, i++)
				lineStyles[style].add(dashes[i] * 0.1 / 85.);
		}

		initialized = true;
	}

	return lineStyles;
}

void WPG1Parser::handleLineAttributes()
{
	if (!m_graphicsStarted)
		return;
	unsigned char style = readU8();
	unsigned char color = readU8();
	unsigned int width = readU16();

	if (style >= 16)
	{
		WPG_DEBUG_MSG(("Invalid style value: %u\n", unsigned(style)));
		style = 1; // set invalid style to solid
	}

	if (!style || !width)
		m_style.insert("draw:stroke", "none");
	else if (style == 1)
		m_style.insert("draw:stroke", "solid");
	else
	{
		m_style.insert("draw:stroke", "dash");

		const auto &lineStyle = getLineStyles()[style];
		m_style.insert("draw:dots1", lineStyle.getDots1());
		m_style.insert("draw:dots1-length", 72 * lineStyle.getDots1Length(), librevenge::RVNG_POINT);
		m_style.insert("draw:dots2", lineStyle.getDots2());
		m_style.insert("draw:dots2-length", 72 * lineStyle.getDots2Length(), librevenge::RVNG_POINT);
		m_style.insert("draw:distance", 72 * lineStyle.getDistance(), librevenge::RVNG_POINT);
	}

	m_penForeColor = m_colorPalette[color];
	m_style.insert("svg:stroke-color", m_penForeColor.getColorString());
	m_style.insert("svg:stroke-opacity", m_penForeColor.getOpacity(), librevenge::RVNG_PERCENT);

	if (!width && style) // CHECKME: !width or !style
		m_style.insert("svg:stroke-width", 0.0);
	else
		m_style.insert("svg:stroke-width", (double)width / 1200.0);

	WPG_DEBUG_MSG(("Line Attributes\n"));
	WPG_DEBUG_MSG(("         Line style: %d\n", style));
	WPG_DEBUG_MSG(("   Line color index: %d\n", color));
	WPG_DEBUG_MSG(("         Line width: %d\n", (int) width));
}

void WPG1Parser::handleLine()
{
	if (!m_graphicsStarted)
		return;
	int sx = readS16();
	int sy = readS16();
	int ex = readS16();
	int ey = readS16();

	librevenge::RVNGPropertyListVector points;
	librevenge::RVNGPropertyList point;
	point.insert("svg:x", (double)sx/1200.0);
	point.insert("svg:y", (double)(m_height-sy)/1200.0);
	points.append(point);
	point.clear();
	point.insert("svg:x", (double)ex/1200.0);
	point.insert("svg:y", (double)(m_height-ey)/1200.0);
	points.append(point);

	librevenge::RVNGPropertyList tmpStyle(m_style);
	if (m_gradient.count())
		tmpStyle.insert("svg:linearGradient", m_gradient);
	m_painter->setStyle(tmpStyle);

	librevenge::RVNGPropertyList tmpPoints;
	tmpPoints.insert("svg:points", points);
	m_painter->drawPolyline(tmpPoints);

	WPG_DEBUG_MSG(("Line\n"));
	WPG_DEBUG_MSG(("    Starting point: %d,%d\n", sx, sy));
	WPG_DEBUG_MSG(("         End point: %d,%d\n", ex, ey));
}

void WPG1Parser::handlePolyline()
{
	if (!m_graphicsStarted)
		return;
	unsigned int count = readU16();
	if (m_input->tell() + int(count) > m_recordEnd)
		count = (m_recordEnd - m_input->tell()) / 4;

	librevenge::RVNGPropertyListVector points;
	librevenge::RVNGPropertyList point;
	for (unsigned int i = 0; i < count; i++)
	{
		point.clear();
		long x = readS16();
		long y = readS16();
		point.insert("svg:x", (double)x/1200.0);
		point.insert("svg:y", (double)(m_height-y)/1200.0);
		points.append(point);
	}

	m_painter->setStyle(m_style);

	librevenge::RVNGPropertyList tmpPoints;
	tmpPoints.insert("svg:points", points);
	m_painter->drawPolyline(tmpPoints);

	WPG_DEBUG_MSG(("Polyline\n"));
}

void WPG1Parser::handleRectangle()
{
	if (!m_graphicsStarted)
		return;
	int x = readS16();
	int y = readS16();
	int w = readS16();
	int h = readS16();

	librevenge::RVNGPropertyList propList;
	propList.insert("svg:x", (double)x/1200.0);
	// in WPG, we have the coordinate of lower left point, in SVG-ish coordinates we have to get upper left
	propList.insert("svg:y", (double)(m_height - h - y)/1200.0);
	propList.insert("svg:width", (double)w/1200.0);
	propList.insert("svg:height",(double)h/1200.0);

	librevenge::RVNGPropertyList tmpStyle(m_style);
	if (m_gradient.count())
		tmpStyle.insert("svg:linearGradient", m_gradient);
	m_painter->setStyle(tmpStyle);

	m_painter->drawRectangle(propList);

	WPG_DEBUG_MSG(("Line\n"));
	WPG_DEBUG_MSG(("    Corner point: %d,%d\n", x, y));
	WPG_DEBUG_MSG(("           Width: %d\n", w));
	WPG_DEBUG_MSG(("          Height: %d\n", h));
}

void WPG1Parser::handlePolygon()
{
	if (!m_graphicsStarted)
		return;
	unsigned int count = readU16();
	if (m_input->tell() + int(count) > m_recordEnd)
		count = (m_recordEnd - m_input->tell()) / 4;

	librevenge::RVNGPropertyListVector points;
	librevenge::RVNGPropertyList point;
	for (unsigned int i = 0; i < count; i++)
	{
		point.clear();
		long x = readS16();
		long y = readS16();
		point.insert("svg:x", (double)x/1200.0);
		point.insert("svg:y", (double)(m_height-y)/1200.0);
		points.append(point);
	}

	librevenge::RVNGPropertyList tmpStyle(m_style);
	if (m_gradient.count())
		tmpStyle.insert("svg:linearGradient", m_gradient);
	m_painter->setStyle(tmpStyle);

	librevenge::RVNGPropertyList tmpPoints;
	tmpPoints.insert("svg:points", points);
	m_painter->drawPolygon(tmpPoints);

	WPG_DEBUG_MSG(("Polygon\n"));
}

void WPG1Parser::handleEllipse()
{
	if (!m_graphicsStarted)
		return;

	librevenge::RVNGPropertyList propList;
	propList.insert("svg:cx", (double)readS16()/1200.0);
	propList.insert("svg:cy", (double)(m_height-readS16())/1200.0);
	propList.insert("svg:rx", (double)readS16()/1200.0);
	propList.insert("svg:ry", (double)readS16()/1200.0);
	propList.insert("librevenge:rotate", (double)readS16());

#if 0
	int beginAngle = readS16();
	int endAngle = readS16();
	unsigned flags = readU16();
#endif
	librevenge::RVNGPropertyList tmpStyle(m_style);
	if (m_gradient.count())
		tmpStyle.insert("svg:linearGradient", m_gradient);
	m_painter->setStyle(tmpStyle);

	m_painter->drawEllipse(propList);

	WPG_DEBUG_MSG(("Ellipse\n"));
	WPG_DEBUG_MSG(("    Center point: %s,%s\n", propList["svg:cx"]->getStr().cstr(), propList["svg:cy"]->getStr().cstr()));
	WPG_DEBUG_MSG(("        Radius x: %s\n", propList["svg:rx"]->getStr().cstr()));
	WPG_DEBUG_MSG(("        Radius y: %s\n", propList["svg:ry"]->getStr().cstr()));
}

void WPG1Parser::handleCurvedPolyline()
{
	if (!m_graphicsStarted)
		return;
	readU32();
	unsigned int count = readU16();
	if (m_input->tell() + int(count) > m_recordEnd)
		count = (m_recordEnd - m_input->tell() - 4) / 12;
	if (!count)
		return;
	librevenge::RVNGPropertyListVector path;
	librevenge::RVNGPropertyList element;

	long xInitial = readS16();
	long yInitial = readS16();
	element.insert("librevenge:path-action", "M");
	element.insert("svg:x", (double)xInitial/1200.0);
	element.insert("svg:y", (double)(m_height-yInitial)/1200.0);
	path.append(element);
	for (unsigned i = 1; i < (count-1)/3; i++)
	{
		long xControl1 = readS16();
		long yControl1 = readS16();
		long xControl2 = readS16();
		long yControl2 = readS16();
		long xCoordinate = readS16();
		long yCoordinate = readS16();

		element.clear();
		element.insert("librevenge:path-action", "C");
		element.insert("svg:x1", (double)xControl1/1200.0);
		element.insert("svg:y1", (double)(m_height-yControl1)/1200.0);
		element.insert("svg:x2", (double)xControl2/1200.0);
		element.insert("svg:y2", (double)(m_height-yControl2)/1200.0);
		element.insert("svg:x", (double)xCoordinate/1200.0);
		element.insert("svg:y", (double)(m_height-yCoordinate)/1200.0);
		path.append(element);

	}

	librevenge::RVNGPropertyList tmpStyle(m_style);
	if (m_gradient.count())
		tmpStyle.insert("svg:linearGradient", m_gradient);
	m_painter->setStyle(tmpStyle);
	librevenge::RVNGPropertyList propList;
	propList.insert("svg:d", path);
	m_painter->drawPath(propList);

	WPG_DEBUG_MSG(("Curved Polyline\n"));
}

void WPG1Parser::handleGroup()
{
	if (!m_graphicsStarted)
		return;

	// groups cannot be nested, so any overlap must be caused by a damaged input
	closeGroup(true);

	const unsigned long endOffset = readU32();
	m_groupEnd = m_input->tell() + std::min(endOffset, libwpg::getRemainingLength(m_input));

	m_painter->openGroup(librevenge::RVNGPropertyList());

	WPG_DEBUG_MSG(("Group\n"));
	WPG_DEBUG_MSG(("    Ends at position: %ld\n", get(m_groupEnd)));
}

void WPG1Parser::closeGroup(const bool force)
{
	if (!m_graphicsStarted)
		return;
	if (!m_groupEnd)
		return;

	if (force || m_input->tell() >= get(m_groupEnd))
	{
		m_painter->closeGroup();
		m_groupEnd.reset();
	}
}

bool WPG1Parser::decodeRLE(std::vector<unsigned char> &buffer, unsigned width, unsigned height, unsigned depth)
{
	buffer.clear();

	// This are the known depth values for WPG1, no point to try to decode others since they are likely to indicate corruption
	if (depth != 8 && depth != 4 && depth != 2 && depth != 1)
		return false;
	if (m_recordEnd <= m_input->tell()) // there is nothing to read
		return false;

	// round to the next byte
	unsigned scanline_width = (width * depth + 7)/8;
	unsigned tmpBufferSize = scanline_width * height;
	// The upper bound of the number of bytes encoded in this RLE record
	const long maxSize = ((m_recordEnd - m_input->tell()) / 3 + 1) * 255;
	if (tmpBufferSize > static_cast<unsigned long>(maxSize))
		return false;

	WPG_DEBUG_MSG(("Scanline width: %d\n", (int) scanline_width));
	WPG_DEBUG_MSG(("Output size: %d\n", scanline_width * height));

	WPG_DEBUG_MSG(("Decoding RLE data\n"));

	buffer.reserve(tmpBufferSize);
	while (m_input->tell() < m_recordEnd && !m_input->isEnd() && buffer.size() < tmpBufferSize)
	{
		unsigned char opcode = readU8();

		if (opcode & 0x80)
		{
			// run of byte
			auto count = (int)(opcode & 0x7f);
			unsigned char pixel = (count > 0) ? readU8() : 0xff;
			if (count == 0)
				count = (int)readU8();
			for (; count ; --count)
				buffer.push_back(pixel);
		}
		else
		{
			auto count = (int)(opcode & 0x7f);
			if (count > 0)
			{
				// literal byte copy
				for (; count ; --count)
					buffer.push_back(readU8());
			}
			else
			{
				// copy entire scan line
				count = (int)readU8();
				if (buffer.size() < scanline_width)
				{
					WPG_DEBUG_MSG(("Cannot copy the scanline, not enough data %li\n", (long)buffer.size()));
					break;
				}
				auto raster_source = unsigned(buffer.size() - scanline_width);
				for (; count; --count)
					for (unsigned r = 0; r < scanline_width ; r++)
					{
						unsigned char pixel = buffer[raster_source + r];
						buffer.push_back(pixel);
					}
			}
		}
	}
	WPG_DEBUG_MSG(("Finish decoding RLE data\n"));
	WPG_DEBUG_MSG(("Buffer length: %li\n", (long)buffer.size()));

	while (buffer.size() < tmpBufferSize)
		buffer.push_back(0);

	return !buffer.empty();
}

void WPG1Parser::fillPixels(libwpg::WPGBitmap &bitmap, const unsigned char *buffer, unsigned width, unsigned height, unsigned depth)
{
	// sanity
	if (!buffer)
		return;

	if (depth != 8 && depth != 4 && depth != 2 && depth != 1)
		return;

	// round to the next byte
	unsigned scanline_width = (width * depth + 7)/8;

	// 1-bit image: black and white
	if (depth == 1)
	{
		libwpg::WPGColor black(0, 0, 0);
		libwpg::WPGColor white(255, 255, 255);
		for (unsigned y = 0; y < height; y++)
		{
			const unsigned char *buf = buffer + y * scanline_width;
			for (unsigned x = 0; x < width; x++)
				if (buf[x/8] & (0x80 >> (x & 7)))
					bitmap.setPixel((int)x, (int)y, white);
				else
					bitmap.setPixel((int)x, (int)y, black);
		}
	}
	// 2-bit image: 4-color bitmap (indexed)
	else if (depth == 2)
	{
		unsigned i = 0;
		for (unsigned y = 0; y < height; y++)
			for (unsigned x = 0; x < width; x++, i++)
			{
				if ((x==0) && (i % 4 != 0))
					i = (i/4 + 1) * 4;
				unsigned index = ((buffer[i/4] & (0x03 << 2*(3 - (i % 4)))) >> 2*(3 - (i % 4)));
				const libwpg::WPGColor &color = m_colorPalette[(int)index];
				bitmap.setPixel((int)x, (int)y, color);
			}
	}
	// 4 -bit image: 16-colour bitmap (indexed)
	else if (depth == 4)
	{
		unsigned i = 0;
		for (unsigned y = 0; y < height; y++)
			for (unsigned x = 0; x < width; x++, i++)
			{
				if ((x==0) && (i % 2 != 0))
					i = (i/2 + 1) * 2;
				unsigned index = ((buffer[i/2] & (0x0f << 4*(1 - (i % 2)))) >> 4*(1 - (i % 2)));
				const libwpg::WPGColor &color = m_colorPalette[(int) index];
				bitmap.setPixel((int) x, (int) y, color);
			}
	}
	// 8-bit image: 256-colour image (indexed)
	else if (depth == 8)
	{
		for (unsigned y = 0; y < height; y++)
		{
			const unsigned char *buf = buffer + y * scanline_width;
			for (unsigned x = 0; x < width; x++)
			{
				const libwpg::WPGColor &color = m_colorPalette[buf[x]];
				bitmap.setPixel((int) x, (int) y, color);
			}
		}

	}
}

void WPG1Parser::handleBitmapTypeOne()
{
	if (!m_graphicsStarted)
		return;
	int width = readS16();
	int height = readS16();
	int depth = readS16();
	int hres = readS16();
	int vres = readS16();

	WPG_DEBUG_MSG(("Bitmap\n"));
	WPG_DEBUG_MSG(("                Width: %d\n", width));
	WPG_DEBUG_MSG(("               Height: %d\n", height));
	WPG_DEBUG_MSG(("                Depth: %d\n", depth));
	WPG_DEBUG_MSG(("Horizontal resolution: %d\n", hres));
	WPG_DEBUG_MSG(("  Vertical resolution: %d\n", vres));

	// if this happens, likely corruption, bail out.
	if (depth != 1 && depth != 2 && depth != 4 && depth != 8)
		return;

	// Sanity checks
	if (hres <= 0)
		hres = 72;
	if (vres <= 0)
		vres = 72;
	if (width < 0)
		width = 0;
	if (height < 0)
		height = 0;

	std::vector<unsigned char> buffer;
	if (decodeRLE(buffer, (unsigned int)width, (unsigned int)height, (unsigned int) depth))
	{
		libwpg::WPGBitmap bitmap(width, height, vres, hres);
		fillPixels(bitmap, &buffer[0], (unsigned int)width, (unsigned int)height, (unsigned int)depth);

		// Bitmap Type 1 does not specify position
		// Assume on the corner (0,0)

		librevenge::RVNGPropertyList propList;
		propList.insert("svg:x", 0.0);
		propList.insert("svg:y", 0.0);
		propList.insert("svg:width", (double)width/(double)hres);
		propList.insert("svg:height", (double)height/(double)vres);
		propList.insert("librevenge:mime-type", "image/bmp");
		propList.insert("office:binary-data", bitmap.getDIB());
		m_painter->drawGraphicObject(propList);
	}
}

void WPG1Parser::handleBitmapTypeTwo()
{
	if (!m_graphicsStarted)
		return;
	int rotation = readS16();
	int x1 = readS16();
	int y1 = readS16();
	int x2 = readS16();
	int y2 = readS16();
	int width = readS16();
	int height = readS16();
	int depth = readS16();
	int hres = readS16();
	int vres = readS16();

	WPG_DEBUG_MSG(("Bitmap\n"));
	WPG_DEBUG_MSG(("       Rotation Angle: %d\n", rotation));
	WPG_DEBUG_MSG(("      Top left corner: %d,%d\n", x1, y1));
	WPG_DEBUG_MSG(("  Bottom right corner: %d,%d\n", x2, y2));
	WPG_DEBUG_MSG(("                Width: %d\n", width));
	WPG_DEBUG_MSG(("               Height: %d\n", height));
	WPG_DEBUG_MSG(("                Depth: %d\n", depth));
	WPG_DEBUG_MSG(("Horizontal resolution: %d\n", hres));
	WPG_DEBUG_MSG(("  Vertical resolution: %d\n", vres));

	// if this happens, likely corruption, bail out.
	if (rotation < 0 || rotation > 359)
		return;
	if (depth != 1 && depth != 2 && depth != 4 && depth != 8)
		return;

	// Sanity checks
	if (hres <= 0)
		hres = 72;
	if (vres <= 0)
		vres = 72;
	if (width < 0)
		width = 0;
	if (height < 0)
		height = 0;

	y1 = m_height - y1;
	y2 = m_height - y2;

	long xs1 = (x1 <= x2) ? x1 : x2;
	long xs2 = (x1 <= x2) ? x2 : x1;
	long ys1 = (y1 <= y2) ? y1 : y2;
	long ys2 = (y1 <= y2) ? y2 : y1;
	WPG_DEBUG_MSG(("%li %li   %li %li\n", xs1, ys1, xs2, ys2));

	std::vector<unsigned char> buffer;
	if (decodeRLE(buffer, (unsigned int)width, (unsigned int)height, (unsigned int)depth))
	{
		libwpg::WPGBitmap bitmap(width, height, vres, hres);
		fillPixels(bitmap, &buffer[0], (unsigned int)width, (unsigned int)height, (unsigned int)depth);

		librevenge::RVNGPropertyList propList;
		propList.insert("svg:x", (double)xs1/(double)hres);
		propList.insert("svg:y", (double)(ys1)/(double)vres);
		propList.insert("svg:width", (double)(xs2-xs1)/(double)hres);
		propList.insert("svg:height", (double)(ys2-ys1)/(double)vres);
		propList.insert("librevenge:mime-type", "image/bmp");
		propList.insert("office:binary-data", bitmap.getDIB());
		m_painter->drawGraphicObject(propList);
	}
}

void WPG1Parser::handlePostscriptTypeOne()
{
	if (!m_graphicsStarted)
		return;
	long x1 = readS16();
	long y1 = readS16();
	long x2 = readS16();
	long y2 = readS16();

	librevenge::RVNGPropertyList propList;

	propList.insert("svg:x", (double)x1/72.0);
	propList.insert("svg:y", (double)m_height/1200.0 - (double)y1/72.0);
	propList.insert("svg:width", ((double)x2 - (double)x1)/72.0);
	propList.insert("svg:height", ((double)y1 - (double)y2)/72.0);
	propList.insert("librevenge:mime-type", "application/x-postscript");

	librevenge::RVNGBinaryData data;
	data.clear();
	while (!m_input->isEnd() && m_input->tell() <= m_recordEnd)
		data.append((unsigned char)readU8());
	if (data.size())
	{
		propList.insert("office:binary-data", data);
		m_painter->drawGraphicObject(propList);
	}
}

void WPG1Parser::handlePostscriptTypeTwo()
{
	if (!m_graphicsStarted)
		return;
#ifdef DEBUG
	unsigned lengthOfData = readU32();
	int rotation = readS16();
#else
	readU32();
	readS16();
#endif
	int x1 = readS16();
	int y1 = readS16();
	int x2 = readS16();
	int y2 = readS16();

	WPG_DEBUG_MSG(("Postscript (Type 2)\n"));
	WPG_DEBUG_MSG(("       Length of Data: %u\n", lengthOfData));
	WPG_DEBUG_MSG(("       Rotation Angle: %d\n", rotation));
	WPG_DEBUG_MSG(("   Bottom left corner: %d,%d\n", x1, y1));
	WPG_DEBUG_MSG(("     Top right corner: %d,%d\n", x2, y2));

	y1 = m_height - y1;
	y2 = m_height - y2;

	long xs1 = (x1 <= x2) ? x1 : x2;
	long xs2 = (x1 <= x2) ? x2 : x1;
	long ys1 = (y1 <= y2) ? y1 : y2;
	long ys2 = (y1 <= y2) ? y2 : y1;
	WPG_DEBUG_MSG(("%li %li   %li %li\n", xs1, ys1, xs2, ys2));

	librevenge::RVNGPropertyList propList;

	propList.insert("svg:x", (double)xs1/1200.0);
	propList.insert("svg:y", (double)(ys1)/1200.0);

	propList.insert("svg:width", ((double)xs2 - (double)xs1)/1200.0);
	propList.insert("svg:height", ((double)ys2 -(double)ys1)/1200.0);

	propList.insert("librevenge:mime-type", "image/x-eps");

	m_input->seek(48, librevenge::RVNG_SEEK_CUR);

	librevenge::RVNGBinaryData data;
	data.clear();
	while (!m_input->isEnd() && m_input->tell() <= m_recordEnd)
		data.append((unsigned char)readU8());
	if (data.size())
	{
		propList.insert("office:binary-data", data);
		m_painter->drawGraphicObject(propList);
	}
}

void WPG1Parser::handleGraphicsTextAttributes()
{
	if (!m_graphicsStarted)
		return;

	m_avgCharWidth = readU16();
	m_fontSize = readU16();
	// cap height? - U16
	// x-height? - U16

	m_input->seek(13, librevenge::RVNG_SEEK_CUR);

	m_textAlign = readU8();
	if (m_textAlign > 2)
	{
		WPG_DEBUG_MSG(("Invalid text alignment value %u, resetting to 'left'\n", unsigned(m_textAlign)));
		m_textAlign = 0;
	}

	m_input->seek(1, librevenge::RVNG_SEEK_CUR);

	const unsigned color = readU8();
	m_fontColor = m_colorPalette[color];

	WPG_DEBUG_MSG(("Graphics Text Attributes\n"));
	WPG_DEBUG_MSG(("    Average character width: %u\n", m_avgCharWidth));
	WPG_DEBUG_MSG(("                  Font size: %u\n", m_fontSize));
	WPG_DEBUG_MSG(("             Text alignment: %u\n", m_textAlign));
	WPG_DEBUG_MSG(("           Text color index: %u\n", color));
}

void WPG1Parser::handleGraphicsTextTypeOne()
{
	if (!m_graphicsStarted)
		return;
	unsigned short textLength = readU16();
	int x = readS16();
	int y = readS16();
	librevenge::RVNGString textString;
	for (unsigned short i=0; i < textLength; i++)
		textString.append((char)readU8());

	WPG_DEBUG_MSG(("Graphics Text (Type 1)\n"));
	WPG_DEBUG_MSG(("  Length of String: %d\n", (int) textLength));
	WPG_DEBUG_MSG(("   String position: %d,%d\n", x, y));
	WPG_DEBUG_MSG(("            String: %s\n", textString.cstr()));

	y = m_height - y;

	librevenge::RVNGPropertyList propList;

	unsigned xAlign = 0;
	if (m_avgCharWidth > 0)
	{
		const unsigned width = textLength * m_avgCharWidth;
		// propList.insert("svg:width", width / 1200.0);

		// x depends on alignment. For right-aligned text it's on the right edge of the text
		// and for centered text in the middle.
		switch (m_textAlign)
		{
		case 0: // left
			break;
		case 1: // center
			xAlign = width / 2;
			break;
		case 2: // right
			xAlign = width;
			break;
		default:
			assert(!"Impossible text alignment value!");
		}
	}

	propList.insert("svg:x", double(x - xAlign) / 1200.0);
	// NOTE: Adjustment by font size is a guess: it might be baseline height instead. I'm not quite sure where the
	// "y" position on a text line is (but I suspect it's on the baseline).
	propList.insert("svg:y", double(y - m_fontSize) / 1200.0);

	m_painter->startTextObject(propList);
	m_painter->openParagraph(librevenge::RVNGPropertyList());

	librevenge::RVNGPropertyList textPropList;

	textPropList.insert("fo:color", m_fontColor.getColorString());
	if (m_fontSize > 0)
		textPropList.insert("fo:font-size", 72 * m_fontSize / 1200., librevenge::RVNG_POINT);

	m_painter->openSpan(textPropList);

	m_painter->insertText(textString);

	m_painter->closeSpan();

	m_painter->closeParagraph();
	m_painter->endTextObject();
}

void WPG1Parser::handleGraphicsTextTypeTwo()
{
	if (!m_graphicsStarted)
		return;
#ifdef DEBUG
	int sizeEquivalentData = readS32();
	short rotationAngle = readS16();
#endif
	unsigned short textLength = readU16();
#ifdef DEBUG
	int x1 = readS16();
	int y1 = readS16();
	int x2 = readS16();
	int y2 = readS16();
	int sx = readS16();
	int sy = readS16();
	unsigned char type = readU8();
#endif
	librevenge::RVNGBinaryData textString;
	for (unsigned short i=0; i < textLength; i++)
		textString.append(readU8());

	WPG_DEBUG_MSG(("Graphics Text (Type 1)\n"));
	WPG_DEBUG_MSG(("  Size of Equivalent data: %d\n", sizeEquivalentData));
	WPG_DEBUG_MSG(("           Rotation Angle: %d\n", rotationAngle));
	WPG_DEBUG_MSG(("         Length of String: %d\n", (int) textLength));
	WPG_DEBUG_MSG(("           Start position: %d,%d\n", x1, y1));
	WPG_DEBUG_MSG(("             End position: %d,%d\n", x2, y2));
	WPG_DEBUG_MSG(("             Scale factor: %d,%d\n", sx, sy));
	WPG_DEBUG_MSG(("                     Type: %d\n", type));

#if 0
	librevenge::RVNGPropertyList propList;

	propList.insert("svg:x", double(x1) / 1200.0);
	propList.insert("svg:y", double(m_height - y1) / 1200.0);
	propList.insert("svg:width", double(x2 - x1) / 1200.0);
	propList.insert("svg:height", double(y2 - y1) / 1200.0);

	m_painter->startTextObject(propList);

	WPGTextDataHandler handler(m_painter);
	libwpd::WPDocument::parseSubDocument(
	    const_cast<librevenge::RVNGInputStream *>(textString.getDataStream()),
	    &handler,
	    libwpd::WPD_FILE_FORMAT_WP5);

	m_painter->endTextObject();
#endif
}

void WPG1Parser::resetPalette()
{
	m_colorPalette.clear();
	for (int i=0; i<256; i++)
	{
		unsigned char red = defaultWPG1PaletteRed[i];
		unsigned char green = defaultWPG1PaletteGreen[i];
		unsigned char blue = defaultWPG1PaletteBlue[i];
		libwpg::WPGColor color(red, green, blue);
		m_colorPalette[i] = color;
	}
}

/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
