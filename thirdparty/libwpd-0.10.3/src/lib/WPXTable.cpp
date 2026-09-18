/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* libwpd
 * Version: MPL 2.0 / LGPLv2.1+
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Major Contributor(s):
 * Copyright (C) 2002 William Lachance (wrlach@gmail.com)
 * Copyright (C) 2002 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 *
 * For minor contributions see the git repository.
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU Lesser General Public License Version 2.1 or later
 * (LGPLv2.1+), in which case the provisions of the LGPLv2.1+ are
 * applicable instead of those above.
 *
 * For further information visit http://libwpd.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#include "WPXTable.h"

#include "libwpd_internal.h"

WPXTableCell::WPXTableCell(unsigned char colSpan, unsigned char rowSpan, unsigned char borderBits) :
	m_colSpan(colSpan),
	m_rowSpan(rowSpan),
	m_borderBits(borderBits)
{
}

WPXTable::~WPXTable()
{
}

void WPXTable::insertRow()
{
	m_tableRows.emplace_back();
}

void WPXTable::insertCell(unsigned char colSpan, unsigned char rowSpan, unsigned char borderBits)
{
	if (m_tableRows.empty())
		throw ParseException();
	m_tableRows.back().emplace_back(colSpan, rowSpan, borderBits);
}

// makeConsistent: make the table border specification (defined per-cell) consistent, with no
// duplicated borders
// pre-condition: the table must be completely built/read before this function is called
void WPXTable::makeBordersConsistent()
{
	// make the top/bottom table borders consistent
	for (unsigned i=0; i<m_tableRows.size(); i++)
	{
		for (unsigned j=0; j<m_tableRows[i].size(); j++)
		{
			if (i < (m_tableRows.size()-1))
			{
				std::vector<WPXTableCell *> cellsBottomAdjacent = _getCellsBottomAdjacent(i, j);
				_makeCellBordersConsistent((m_tableRows[i])[j], cellsBottomAdjacent,
				                           WPX_TABLE_CELL_BOTTOM_BORDER_OFF, WPX_TABLE_CELL_TOP_BORDER_OFF);
			}
			if (j < (m_tableRows[i].size()-1))
			{
				std::vector<WPXTableCell *> cellsRightAdjacent = _getCellsRightAdjacent(i, j);
				_makeCellBordersConsistent((m_tableRows[i])[j], cellsRightAdjacent,
				                           WPX_TABLE_CELL_RIGHT_BORDER_OFF, WPX_TABLE_CELL_LEFT_BORDER_OFF);
			}
		}
	}
}

void WPXTable::_makeCellBordersConsistent(WPXTableCell &cell, std::vector<WPXTableCell *> &adjacentCells,
                                          int adjacencyBitCell, int adjacencyBitBoundCells)
{
	if (!adjacentCells.empty())
	{
		// if this cell is adjacent to > 1 cell, and it has no border
		// make the cells below have no border
		// NB: there is a corner case where this will not work but it
		// is not resolvable given how WP/OOo define table borders. see BUGS
		if (cell.m_borderBits & adjacencyBitCell)
		{
			for (auto &adjacentCell : adjacentCells)
			{
				adjacentCell->m_borderBits |= (unsigned char)(adjacencyBitBoundCells & 0xff);
			}
		}
		// otherwise we can get the same effect by bottom border from
		// this cell-- if the adjacent cells have/don't have borders, this will be
		// picked up automatically
		else
			cell.m_borderBits |= (unsigned char)(adjacencyBitCell & 0xff);
	}
}

std::vector<WPXTableCell *> WPXTable::_getCellsBottomAdjacent(int i, int j)
{
	int bottomAdjacentRow = i + (m_tableRows[i])[j].m_rowSpan;
	std::vector<WPXTableCell *>  cellsBottomAdjacent;

	if ((long)bottomAdjacentRow >= (long)m_tableRows.size())
		return cellsBottomAdjacent;

	for (int j1=0; j1<(int)m_tableRows[bottomAdjacentRow].size(); j1++)
	{
		if (((j1 + (m_tableRows[bottomAdjacentRow])[j1].m_colSpan) > j) &&
		        (j1 < (j + (m_tableRows[i])[j].m_colSpan)))
		{
			cellsBottomAdjacent.push_back(&(m_tableRows[bottomAdjacentRow])[j1]);
		}
	}

	return cellsBottomAdjacent;
}

std::vector<WPXTableCell *> WPXTable::_getCellsRightAdjacent(int i, int j)
{
	int rightAdjacentCol = j + 1;
	std::vector<WPXTableCell *> cellsRightAdjacent;

	if ((long)rightAdjacentCol >= (long)m_tableRows[i].size()) // num cols is uniform across table: this comparison is valid
		return cellsRightAdjacent;

	for (int i1=0; i1<(int)m_tableRows.size(); i1++)
	{
		if ((long)(m_tableRows[i1]).size() > (long)rightAdjacentCol) // ignore cases where the right adjacent column
		{
			// pushes us beyond table borders (FIXME: good idea?)
			if (((i1 + (m_tableRows[i1])[rightAdjacentCol].m_rowSpan) > i) &&
			        (i1 < (i + (m_tableRows[i])[j].m_rowSpan)))
			{
				cellsRightAdjacent.push_back(&(m_tableRows[i1])[rightAdjacentCol]);
			}
		}
	}

	return cellsRightAdjacent;
}

/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
