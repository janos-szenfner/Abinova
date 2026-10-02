/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* AbiSource
 *
 * Copyright (C) 2009 Firat Kiyak <firatkiyak@gmail.com>
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

#include <string>

#include "ut_assert.h"
#include "ut_misc.h"
#include "OXML_Document.h"
#include "OXML_FontManager.h"
#include "OXMLi_ListenerState_Table.h"

OXMLi_ListenerState_Table::OXMLi_ListenerState_Table()
	: OXMLi_ListenerState()
{

}

void OXMLi_ListenerState_Table::startElement (OXMLi_StartElementRequest * rqst)
{
	if (nameMatches(rqst->pName, NS_W_KEY, "tbl"))
	{
		OXML_SharedElement_Table pTable(new OXML_Element_Table(""));
		m_tableStack.push(pTable);
		rqst->stck->push(pTable);
		rqst->handled = true;
		pTable->setCurrentRowNumber(-1);
		pTable->setCurrentColNumber(-1);
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "tr"))
	{
		if(m_tableStack.empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		auto table = m_tableStack.top();
		OXML_Element_Row* pRow = new OXML_Element_Row("", table.get());
		m_rowStack.push(pRow);
		OXML_SharedElement row(pRow);
		rqst->stck->push(row);
		rqst->handled = true;
		table->incrementCurrentRowNumber();
		table->setCurrentColNumber(0);
		pRow->setRowNumber(table->getCurrentRowNumber());
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "tc"))
	{
		if(m_tableStack.empty() || m_rowStack.empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		auto table = m_tableStack.top();
		OXML_Element_Row* row = m_rowStack.top();
		OXML_SharedElement_Cell pCell(
			new OXML_Element_Cell("", table.get(),
								  table->getCurrentColNumber(),
								  table->getCurrentColNumber()+1, //left right
								  table->getCurrentRowNumber(),
								  table->getCurrentRowNumber()+1)); //top,bottom
		pCell->setRow(row);
		m_cellStack.push(pCell);
		OXML_SharedElement cell(pCell);
		rqst->stck->push(cell);
		rqst->handled = true;
		table->incrementCurrentColNumber();
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "gridSpan"))
	{
		if(m_tableStack.empty() || m_cellStack.empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		auto table = m_tableStack.top();
		const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
		if(val)
		{
			int span = atoi(val);
			int left = table->getCurrentColNumber()-1;
			int right = left + span;
			// change current cell's right index
			auto cell = m_cellStack.top();
			cell->setRight(right);
			// update column index of current table
			table->setCurrentColNumber(right);
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "vMerge"))
	{
		if(m_cellStack.empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		auto cell = m_cellStack.top();
		cell->setVerticalMergeStart(false); //default to continue if the attribute is missing
		const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
		if (val && !strcmp(val, "restart"))
		{
			cell->setVerticalMergeStart(true);
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "hMerge"))
	{
		if(m_cellStack.empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		auto cell = m_cellStack.top();
		cell->setHorizontalMergeStart(false); //default to continue if the attribute is missing
		const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
		if(val && !strcmp(val, "restart")) 
		{
			cell->setHorizontalMergeStart(true);
		}
		rqst->handled = true;
	}

	//Table Properties
	else if(nameMatches(rqst->pName, NS_W_KEY, "gridCol") && 
			contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tblGrid"))
	{
		if(m_tableStack.empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		auto table = m_tableStack.top();
		const gchar* w = attrMatches(NS_W_KEY, "w", rqst->ppAtts);
		if (w)
		{
			//append this width to table-column-props property
			const gchar* tableColumnProps = nullptr;
			UT_Error ret = table->getProperty("table-column-props", tableColumnProps);
			if((ret != UT_OK) || !tableColumnProps)
				tableColumnProps = "";
			std::string cols(tableColumnProps);
			cols += _TwipsToPoints(w);
			cols += "pt/";
			ret = table->setProperty("table-column-props", cols);
			if(ret != UT_OK)
			{
				UT_DEBUGMSG(("FRT:OpenXML importer can't set table-column-props:%s\n", cols.c_str()));
			}
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "trHeight") &&
			contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "trPr"))
	{
		if(m_tableStack.empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		auto table = m_tableStack.top();
		const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
		if (val)
		{
			const gchar* tableRowHeights = nullptr;
			UT_Error ret = table->getProperty("table-row-heights", tableRowHeights);
			if((ret != UT_OK) || !tableRowHeights)
				tableRowHeights = "";
			std::string rowHeights(tableRowHeights);
			rowHeights += _TwipsToPoints(val);
			rowHeights += "pt/";
			ret = table->setProperty("table-row-heights", rowHeights);
			if(ret != UT_OK)
			{
				UT_DEBUGMSG(("FRT:OpenXML importer can't set table-row-heights:%s\n", rowHeights.c_str()));
			}
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "vAlign") &&
			contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tcPr"))
	{
		if(m_cellStack.empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}
		auto cell = m_cellStack.top();
		const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
		/* vert-align is a 0-100 offset: top 0, center 50, bottom 100;
		 * both/justify degrade to top */
		const char* va = "0";
		if(val && !strcmp(val, "center"))
			va = "50";
		else if(val && !strcmp(val, "bottom"))
			va = "100";
		cell->setProperty("vert-align", va);
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "tblHeader") &&
			contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "trPr"))
	{
		//repeat-on-each-page header row; marked on the row element,
		//fanned out to its cells when the row flushes
		if(!rqst->stck->empty())
		{
			const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			bool bOn = !val || !*val || !strcmp(val, "true") ||
				!strcmp(val, "1") || !strcmp(val, "on");
			OXML_SharedElement row = OXMLi_elemTop(rqst->stck);
			if(row)
				row->setProperty("tblheader", bOn ? "1" : "0");
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "jc") &&
			!rqst->context->empty() &&
			contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tblPr"))
	{
		//table alignment: left|center|right|start|end
		if(!m_tableStack.empty())
		{
			const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			if(val && *val)
			{
				std::string al(val);
				if(!al.compare("start")) al = "left";
				else if(!al.compare("end")) al = "right";
				m_tableStack.top()->setProperty("table-position", al.c_str());
			}
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "tblpPr") &&
			!rqst->context->empty() &&
			contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tblPr"))
	{
		/* floating table position — preserved on the table as
		 * table-float-* props (no floating-table layout yet) */
		if(!m_tableStack.empty())
		{
			auto tbl = m_tableStack.top();
			struct { const char* a; const char* p; } names[] = {
				{"horzAnchor",  "table-float-hanchor"},
				{"vertAnchor",  "table-float-vanchor"},
				{"tblpXSpec",   "table-float-halign"},
				{"tblpYSpec",   "table-float-valign"} };
			for(auto & e : names)
			{
				const gchar* v = attrMatches(NS_W_KEY, e.a, rqst->ppAtts);
				if(v && *v)
					tbl->setProperty(e.p, v);
			}
			struct { const char* a; const char* p; } dims[] = {
				{"tblpX",          "table-float-x"},
				{"tblpY",          "table-float-y"},
				{"leftFromText",   "table-float-margin-left"},
				{"rightFromText",  "table-float-margin-right"},
				{"topFromText",    "table-float-margin-top"},
				{"bottomFromText", "table-float-margin-bottom"} };
			for(auto & e : dims)
			{
				const gchar* v = attrMatches(NS_W_KEY, e.a, rqst->ppAtts);
				if(v && *v)
				{
					std::string s(_TwipsToPoints(v));
					s += "pt";
					tbl->setProperty(e.p, s.c_str());
				}
			}
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "tblLook") &&
			!rqst->context->empty() &&
			contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tblPr"))
	{
		if(!m_tableStack.empty())
		{
			const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			if(val && *val)
			{
				m_tableStack.top()->setProperty("table-look", val);
			}
			else
			{
				/* pre-2007 form: assemble the bitmask from the
				 * boolean attributes */
				struct { const char* a; int bit; } flags[] = {
					{"firstRow",0x020},{"lastRow",0x040},
					{"firstColumn",0x080},{"lastColumn",0x100},
					{"noHBand",0x200},{"noVBand",0x400} };
				int mask = 0;
				for(auto & e : flags)
				{
					const gchar* v = attrMatches(NS_W_KEY, e.a, rqst->ppAtts);
					if(v && (!strcmp(v,"1") || !strcmp(v,"true") || !strcmp(v,"on")))
						mask |= e.bit;
				}
				char buf[16];
				g_snprintf(buf, sizeof(buf), "%04X", mask);
				m_tableStack.top()->setProperty("table-look", buf);
			}
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "tblCaption") ||
			nameMatches(rqst->pName, NS_W_KEY, "tblDescription"))
	{
		if(!rqst->context->empty() &&
			contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tblPr") &&
			!m_tableStack.empty())
		{
			const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			if(val && *val)
				m_tableStack.top()->setProperty(
					nameMatches(rqst->pName, NS_W_KEY, "tblCaption") ?
						"table-caption" : "table-description", val);
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "bidiVisual") &&
			!rqst->context->empty() &&
			contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tblPr"))
	{
		if(!m_tableStack.empty())
		{
			const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			bool bOn = !val || !*val || !strcmp(val, "true") ||
				!strcmp(val, "1") || !strcmp(val, "on");
			m_tableStack.top()->setProperty("table-bidi-visual", bOn ? "1" : "0");
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "textDirection") &&
			!rqst->context->empty() &&
			contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tcPr"))
	{
		if(!m_cellStack.empty())
		{
			const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			if(val && *val)
				m_cellStack.top()->setProperty("cell-text-direction", val);
		}
		rqst->handled = true;
	}
	else if((nameMatches(rqst->pName, NS_W_KEY, "noWrap") ||
			 nameMatches(rqst->pName, NS_W_KEY, "tcFitText") ||
			 nameMatches(rqst->pName, NS_W_KEY, "hideMark")) &&
			!rqst->context->empty() &&
			contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tcPr"))
	{
		if(!m_cellStack.empty())
		{
			const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
			bool bOn = !val || !*val || !strcmp(val, "true") ||
				!strcmp(val, "1") || !strcmp(val, "on");
			const char* prop =
				nameMatches(rqst->pName, NS_W_KEY, "noWrap") ? "cell-no-wrap" :
				nameMatches(rqst->pName, NS_W_KEY, "tcFitText") ? "cell-fit-text" :
					"cell-hide-mark";
			m_cellStack.top()->setProperty(prop, bOn ? "1" : "0");
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "left") ||
			nameMatches(rqst->pName, NS_W_KEY, "right") ||
			nameMatches(rqst->pName, NS_W_KEY, "top") ||
			nameMatches(rqst->pName, NS_W_KEY, "bottom"))
	{
		rqst->handled = true;

		/* w:tblCellMar / w:tcMar children carry cell padding, not borders */
		if(!rqst->context->empty() &&
			(contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tblCellMar") ||
			 contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tcMar")))
		{
			bool bCellMar = contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tcMar");
			OXML_SharedElement marElem;
			if(bCellMar)
				marElem = m_cellStack.empty() ? OXML_SharedElement() : m_cellStack.top();
			else
				marElem = m_tableStack.empty() ? OXML_SharedElement() : m_tableStack.top();
			if(!marElem)
				return;
			const gchar* w = attrMatches(NS_W_KEY, "w", rqst->ppAtts);
			const gchar* type = attrMatches(NS_W_KEY, "type", rqst->ppAtts);
			if(w && *w && (!type || !strcmp(type, "dxa")))
			{
				std::string edgeName(rqst->pName);
				edgeName = edgeName.substr(strlen(NS_W_KEY)+1);
				std::string propName(bCellMar ? "cell-margin-" : "tblcellmar-");
				propName += edgeName;
				std::string dim(_TwipsToPoints(w));
				dim += "pt";
				marElem->setProperty(propName.c_str(), dim.c_str());
			}
			return;
		}

		const gchar* color = attrMatches(NS_W_KEY, "color", rqst->ppAtts);
		const gchar* sz = attrMatches(NS_W_KEY, "sz", rqst->ppAtts);
		const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);

		UT_Error ret = UT_OK;

		std::string borderName(rqst->pName);
		borderName = borderName.substr(strlen(NS_W_KEY)+1);
		if(!borderName.compare("bottom"))
			borderName = "bot";

		std::string borderStyle = borderName + "-style";
		std::string borderColor = borderName + "-color";
		std::string borderThickness = borderName + "-thickness";

		OXML_SharedElement element;

		if(rqst->context->empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		if(contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tcBorders"))
			element = m_cellStack.empty() ? OXML_SharedElement() : m_cellStack.top();
		else if(contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tblBorders"))
			element = m_tableStack.empty() ? OXML_SharedElement() : m_tableStack.top();

		if(!element)
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		if(color && strcmp(color, "auto")) 
		{
			ret = element->setProperty(borderColor, color);
			if(ret != UT_OK)
			{
				UT_DEBUGMSG(("FRT:OpenXML importer can't set %s:%s\n", borderColor.c_str(), color));
			}
		}
		if(sz) 
		{
			std::string szVal(_EighthPointsToPoints(sz));
			szVal += "pt";
			ret = element->setProperty(borderThickness, szVal);
			if(ret != UT_OK)
			{
				UT_DEBUGMSG(("FRT:OpenXML importer can't set %s:%s\n", borderThickness.c_str(), color));
			}
		}

		std::string styleValue = "1"; //single line border by default
		if(val && *val)
		{
			/* Abinova edge styles: 0 none, 1 solid, 2 dotted,
			 * 3 dashed, 4 double, 5 dashdot, 6 dashdotdot,
			 * 7 longdash, 8 triple, 9 wave */
			if (!strcmp(val, "none") || !strcmp(val, "nil"))
				styleValue = "0";
			else if (!strcmp(val, "dotted"))
				styleValue = "2";
			else if (!strcmp(val, "dashed") ||
					 !strcmp(val, "dashSmallGap"))
				styleValue = "3";
			else if (!strcmp(val, "double") ||
					 !strncmp(val, "thinThick", 9) ||
					 !strncmp(val, "thickThin", 9))
				styleValue = "4";
			else if (!strcmp(val, "dotDash") ||
					 !strcmp(val, "dashDotStroked"))
				styleValue = "5";
			else if (!strcmp(val, "dotDotDash"))
				styleValue = "6";
			else if (!strcmp(val, "dashLargeGap"))
				styleValue = "7";
			else if (!strcmp(val, "triple"))
				styleValue = "8";
			else if (!strcmp(val, "wave") ||
					 !strcmp(val, "doubleWave"))
				styleValue = "9";
		}

		ret = element->setProperty(borderStyle, styleValue);
		if(ret != UT_OK)
		{
			UT_DEBUGMSG(("FRT:OpenXML importer can't set %s:0\n", borderStyle.c_str()));
		}
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "shd"))
	{
		const gchar* fill = attrMatches(NS_W_KEY, "fill", rqst->ppAtts);

		UT_Error ret = UT_OK;
		OXML_SharedElement element;

		if(rqst->context->empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		if(contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tcPr"))
			element = m_cellStack.empty() ? OXML_SharedElement() : m_cellStack.top();
		else if(contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tblPr"))
			element = m_tableStack.empty() ? OXML_SharedElement() : m_tableStack.top();

		if(!element)
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		if(fill && strcmp(fill, "auto")) 
		{
			ret = element->setProperty("background-color", fill);
			if(ret != UT_OK)
			{
				UT_DEBUGMSG(("FRT:OpenXML importer can't set background-color:%s\n", fill));	
			}
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "tblCellSpacing"))
	{
		if(m_tableStack.empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}
		auto table = m_tableStack.top();
		const gchar* w = attrMatches(NS_W_KEY, "w", rqst->ppAtts);
		const gchar* type = attrMatches(NS_W_KEY, "type", rqst->ppAtts);
		if(w && *w && (!type || !strcmp(type, "dxa")))
		{
			std::string dim(_TwipsToPoints(w));
			dim += "pt";
			table->setProperty("table-col-spacing", dim.c_str());
			table->setProperty("table-row-spacing", dim.c_str());
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "tblInd"))
	{
		if(m_tableStack.empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}
		auto table = m_tableStack.top();
		const gchar* w = attrMatches(NS_W_KEY, "w", rqst->ppAtts);
		const gchar* type = attrMatches(NS_W_KEY, "type", rqst->ppAtts);
		if(w && *w && (!type || !strcmp(type, "dxa")))
		{
			std::string dim(_TwipsToPoints(w));
			dim += "pt";
			table->setProperty("table-margin-left", dim.c_str());
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "tblW"))
	{
		if(m_tableStack.empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}
		auto table = m_tableStack.top();
		const gchar* w = attrMatches(NS_W_KEY, "w", rqst->ppAtts);
		const gchar* type = attrMatches(NS_W_KEY, "type", rqst->ppAtts);
		if(w && *w)
		{
			if(type && !strcmp(type, "pct"))
			{
				//50ths of a percent -> percent number
				double pct = UT_convertDimensionless(w) / 50.0;
				table->setProperty("table-rel-width",
								   UT_convertToDimensionlessString(pct));
			}
			else if(!type || !strcmp(type, "dxa"))
			{
				std::string dim(_TwipsToPoints(w));
				dim += "pt";
				table->setProperty("table-width", dim.c_str());
			}
			//type nil/auto: table autosizes - no property needed
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "tblStyle"))
	{
		if(m_tableStack.empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		auto table = m_tableStack.top();
		const gchar* val = attrMatches(NS_W_KEY, "val", rqst->ppAtts);
		if (val && table)
		{
			std::string styleName(val);
			OXML_Document* doc = OXML_Document::getInstance();
			if(doc)
				table->applyStyle(doc->getStyleById(styleName));
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "tblPr"))
	{
		if(m_tableStack.empty())
		{
			//we must be in tblStyle in styles, so let's push the table instance to m_tableStack
			auto tbl = std::static_pointer_cast<OXML_Element_Table>(OXMLi_elemTop(rqst->stck));
			m_tableStack.push(tbl);
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "trPr"))
	{
		if(m_rowStack.empty())
		{
			//we must be in styles, so let's push the row instance to m_rowStack
			OXML_Element_Row* row = static_cast<OXML_Element_Row*>(OXMLi_elemTop(rqst->stck).get());
			m_rowStack.push(row);
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "tcPr"))
	{
		if(m_cellStack.empty())
		{
			//we must be in styles, so let's push the cell instance to m_cellStack
			OXML_SharedElement_Cell cell =
				std::static_pointer_cast<OXML_Element_Cell>(OXMLi_elemTop(rqst->stck));
			m_cellStack.push(cell);
		}
		rqst->handled = true;
	}
	//TODO: more coming here
}

void OXMLi_ListenerState_Table::endElement (OXMLi_EndElementRequest * rqst)
{
	if (nameMatches(rqst->pName, NS_W_KEY, "tbl"))
	{
		if(m_tableStack.empty() || rqst->stck->empty())
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		OXML_SharedElement table = OXMLi_elemTop(rqst->stck);
		rqst->stck->pop(); //pop table
		if(rqst->stck->empty())
		{
			OXML_SharedSection last = OXMLi_sectTop(rqst->sect_stck);
			if (last.get())
				last->appendElement(table);
		}
		else
		{
			OXML_SharedElement container = OXMLi_elemTop(rqst->stck);
			container->appendElement(table);
		}
		m_tableStack.pop();
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "tr"))
	{
		if(m_rowStack.empty() || (rqst->stck->size() < 2))
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		OXML_SharedElement row = OXMLi_elemTop(rqst->stck);
		rqst->stck->pop(); //pop row

		const gchar* hdr = nullptr;
		if(row->getProperty("tblheader", hdr) == UT_OK && hdr && !strcmp(hdr, "1"))
		{
			OXML_ElementVector cells = row->getChildren();
			for(auto c : cells)
				if(c)
					c->setProperty("header-row", "1");
		}

		OXML_SharedElement table = OXMLi_elemTop(rqst->stck);
		table->appendElement(row);
		m_rowStack.pop();
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "tc"))
	{
		if(m_tableStack.empty() || m_cellStack.empty() || (rqst->stck->size() < 2))
		{
			rqst->handled = false;
			rqst->valid = false;
			return;
		}

		OXML_SharedElement cell = OXMLi_elemTop(rqst->stck);
		rqst->stck->pop(); //pop cell
		OXML_SharedElement row = OXMLi_elemTop(rqst->stck);
		auto pCell = m_cellStack.top();
		if(!pCell->startsHorizontalMerge() && !pCell->startsVerticalMerge())
		{
			//do nothing in this case
		}
		else if(!pCell->startsVerticalMerge())
		{
			auto table = m_tableStack.top();
			if(!table->incrementBottomVerticalMergeStart(pCell))
			{
				//this means there is no cell before this starting a vertical merge
				//revert back to vertical merge start instead of continue
				pCell->setVerticalMergeStart(true);
				UT_DEBUGMSG(("FRT:OpenXML importer, invalid <vMerge val=continue> attribute.\n"));
			}
		}
		else if(!pCell->startsHorizontalMerge())
		{
			auto table = m_tableStack.top();
			if(!table->incrementRightHorizontalMergeStart(pCell))
			{
				//this means there is no cell before this starting a horizontal merge
				//revert back to horizontal merge start instead of continue
				pCell->setHorizontalMergeStart(true);
				UT_DEBUGMSG(("FRT:OpenXML importer, invalid <hMerge val=continue> attribute.\n"));
			}
		}
		else //(pCell->startsHorizontalMerge() && pCell->startsVerticalMerge())
		{
			row->appendElement(cell);
		}
		m_cellStack.pop();
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "gridSpan") ||
			nameMatches(rqst->pName, NS_W_KEY, "vMerge") ||
			nameMatches(rqst->pName, NS_W_KEY, "hMerge") ||
			nameMatches(rqst->pName, NS_W_KEY, "gridCol") ||
			nameMatches(rqst->pName, NS_W_KEY, "trHeight") ||
			nameMatches(rqst->pName, NS_W_KEY, "left") ||
			nameMatches(rqst->pName, NS_W_KEY, "right") ||
			nameMatches(rqst->pName, NS_W_KEY, "top") ||
			nameMatches(rqst->pName, NS_W_KEY, "bottom") ||
			nameMatches(rqst->pName, NS_W_KEY, "tblW") ||
			nameMatches(rqst->pName, NS_W_KEY, "tblInd") ||
			nameMatches(rqst->pName, NS_W_KEY, "tblCellSpacing") ||
			nameMatches(rqst->pName, NS_W_KEY, "tblHeader") ||
			nameMatches(rqst->pName, NS_W_KEY, "vAlign") ||
			nameMatches(rqst->pName, NS_W_KEY, "tblCellMar") ||
			nameMatches(rqst->pName, NS_W_KEY, "tcMar") ||
			nameMatches(rqst->pName, NS_W_KEY, "tblpPr") ||
			nameMatches(rqst->pName, NS_W_KEY, "tblLook") ||
			nameMatches(rqst->pName, NS_W_KEY, "tblCaption") ||
			nameMatches(rqst->pName, NS_W_KEY, "tblDescription") ||
			nameMatches(rqst->pName, NS_W_KEY, "bidiVisual") ||
			nameMatches(rqst->pName, NS_W_KEY, "textDirection") ||
			nameMatches(rqst->pName, NS_W_KEY, "noWrap") ||
			nameMatches(rqst->pName, NS_W_KEY, "tcFitText") ||
			nameMatches(rqst->pName, NS_W_KEY, "hideMark") ||
			nameMatches(rqst->pName, NS_W_KEY, "jc") ||
			nameMatches(rqst->pName, NS_W_KEY, "tblStyle"))
	{
		rqst->handled = true;
	}	
	else if(nameMatches(rqst->pName, NS_W_KEY, "tblPr"))
	{
		if(!rqst->context->empty() && !contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tbl") && !m_tableStack.empty())
		{
			m_tableStack.pop(); //pop the dummy table
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "trPr"))
	{
		if(!rqst->context->empty() && !contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tr") && !m_rowStack.empty())
		{
			m_rowStack.pop(); //pop the dummy row
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "tcPr"))
	{
		if(!rqst->context->empty() && !contextMatches(OXMLi_contextBack(rqst->context), NS_W_KEY, "tc") && !m_cellStack.empty())
		{
			m_cellStack.pop(); //pop the dummy cell
		}
		rqst->handled = true;
	}
	else if(nameMatches(rqst->pName, NS_W_KEY, "shd"))
	{
		std::string contextTag = rqst->context->empty() ? "" : OXMLi_contextBack(rqst->context);
		rqst->handled = contextMatches(contextTag, NS_W_KEY, "tcPr") || contextMatches(contextTag, NS_W_KEY, "tblPr");
	}
	//TODO: more coming here
}

void OXMLi_ListenerState_Table::charData (OXMLi_CharDataRequest * /*rqst*/)
{
	//don't do anything here
}
