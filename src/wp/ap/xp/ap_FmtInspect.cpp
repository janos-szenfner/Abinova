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

#include <string>
#include <vector>

#include "ut_types.h"
#include "ut_std_string.h"

#include "fv_View.h"
#include "fl_DocLayout.h"
#include "fl_ContainerLayout.h"
#include "fl_SectionLayout.h"
#include "fl_BlockLayout.h"
#include "pd_Document.h"
#include "pd_Style.h"
#include "pp_AttrProp.h"
#include "pt_PieceTable.h"
#include "pt_Types.h"

#include "ap_FmtInspect.h"

/* localized display name for a style id ("Heading 1" stays as-is on
 * en locales; builtin names map through the translation table) */
static std::string s_style_name(const char * sz)
{
	std::string loc;
	if (sz && sz[0])
		pt_PieceTable::s_getLocalisedStyleName(sz, loc);
	return loc.empty() ? std::string(sz ? sz : "") : loc;
}

/* "Heading 1 (based on Normal -> Body)" style-chain line */
static std::string s_style_chain(PD_Document * pDoc, const char * szName)
{
	std::string line = s_style_name(szName);
	if (!pDoc || !szName || !szName[0])
		return line;

	PD_Style * pStyle = nullptr;
	pDoc->getStyle(szName, &pStyle);
	std::vector<std::string> bases;
	UT_sint32 guard = 0;
	while (pStyle && guard++ < 16)
	{
		PD_Style * pBase = pStyle->getBasedOn();
		if (!pBase)
			break;
		const char * szBase = pBase->getName();
		if (!szBase || !szBase[0])
			break;
		bases.push_back(s_style_name(szBase));
		pStyle = pBase;
	}
	if (!bases.empty())
	{
		line += " (based on ";
		for (size_t i = 0; i < bases.size(); ++i)
		{
			if (i)
				line += " -> ";
			line += bases[i];
		}
		line += ")";
	}
	return line;
}

static void s_dump_props(const PP_AttrProp * pAP,
						 std::vector<std::string> & lines)
{
	if (!pAP)
		return;
	const gchar * szName = nullptr;
	const gchar * szValue = nullptr;
	for (int i = 0; pAP->getNthProperty(i, szName, szValue); ++i)
	{
		if (!szName || !szValue)
			continue;
		std::string line = szName;
		line += " = ";
		line += szValue;
		lines.push_back(std::move(line));
	}
}

/* the resolved-formatting section keeps to the props a user actually
 * inspects — the full prop table carries ~150 boilerplate entries
 * (toc-*, ole-*, altcontent-*, empty values) that would drown the
 * report */
static const char * const s_resolvedCharProps[] =
{
	"font-family", "font-size", "font-weight", "font-style",
	"font-variant", "font-stretch", "text-decoration",
	"text-transform", "text-position", "color", "bgcolor",
	"char-width", "char-spacing", "char-kern", "lang",
	"list-style", "display",
};

static const char * const s_resolvedBlockProps[] =
{
	"text-align", "margin-left", "margin-right", "margin-top",
	"margin-bottom", "text-indent", "line-height", "keep-with-next",
	"keep-together", "widows", "orphans", "dom-dir", "tabstops",
	"default-tab-interval", "column-gap", "background-color",
	"shading-pattern",
};

static bool s_prop_wanted(const std::string & name,
						  const char * const * set, size_t n)
{
	for (size_t i = 0; i < n; ++i)
		if (name == set[i])
			return true;
	return false;
}

static void s_dump_vector(const PP_PropertyVector & props,
						  const char * const * set, size_t nSet,
						  const char * szPrefix,
						  std::vector<std::string> & lines)
{
	for (size_t i = 0; i + 1 < props.size(); i += 2)
	{
		if (props[i + 1].empty() ||
			!s_prop_wanted(props[i], set, nSet))
			continue;
		std::string line = szPrefix ? szPrefix : "";
		line += props[i];
		line += " = ";
		line += props[i + 1];
		lines.push_back(std::move(line));
	}
}

void AP_FmtInspect::buildReport(FV_View * pView,
								std::vector<AP_FmtReportSection> & out)
{
	out.clear();
	if (!pView || !pView->getDocument() ||
		!pView->getLayout() || !pView->getLayout()->getFirstSection())
	{
		AP_FmtReportSection sec;
		sec.heading = "Formatting";
		sec.lines.push_back("No document is open.");
		out.push_back(std::move(sec));
		return;
	}
	PD_Document * pDoc = pView->getDocument();
	PT_DocPosition posStart = pView->getPoint();
	PT_DocPosition posEnd = posStart;
	bool bSelEmpty = pView->isSelectionEmpty();
	if (!bSelEmpty)
	{
		const PT_DocPosition anchor = pView->getSelectionAnchor();
		if (anchor < posStart)
			posStart = anchor;
		else
			posEnd = anchor;
	}
	if (posStart < 2)
		posStart = 2;

	fl_BlockLayout * pBlock = pView->getBlockAtPosition(posStart);
	if (!pBlock)
	{
		AP_FmtReportSection sec;
		sec.heading = "Formatting";
		sec.lines.push_back("No text at the insertion point.");
		out.push_back(std::move(sec));
		return;
	}

	const PP_AttrProp * pBlockAP = nullptr;
	pBlock->getAP(pBlockAP);
	const UT_uint32 blockPosition = pBlock->getPosition();
	if (blockPosition > posStart)
	{
		posStart = blockPosition;
		if (posEnd < posStart)
			posEnd = posStart;
	}
	const PP_AttrProp * pSpanAP = nullptr;
	/* same edge rule as getCharFormat: with a selection we want the
	 * interior format, not the format to the left of its start */
	pBlock->getSpanAP(posStart - blockPosition, bSelEmpty, pSpanAP);
	/* ---- Paragraph style ---------------------------------------- */
	{
		AP_FmtReportSection sec;
		sec.heading = "Paragraph Style";
		const gchar * szStyle = nullptr;
		if (pBlockAP)
			pBlockAP->getAttribute(PT_STYLE_ATTRIBUTE_NAME, szStyle);
		if (szStyle && szStyle[0])
		{
			sec.lines.push_back(s_style_chain(pDoc, szStyle));
			PD_Style * pStyle = nullptr;
			pDoc->getStyle(szStyle, &pStyle);
			PD_Style * pNext = pStyle ? pStyle->getFollowedBy() : nullptr;
			if (pNext && pNext->getName() && pNext->getName()[0])
			{
				std::string line = "Style for following paragraph: ";
				line += s_style_name(pNext->getName());
				sec.lines.push_back(std::move(line));
			}
		}
		else
		{
			sec.lines.push_back("None (default paragraph)");
		}
		out.push_back(std::move(sec));
	}

	/* ---- Character style ---------------------------------------- */
	{
		AP_FmtReportSection sec;
		sec.heading = "Character Style";
		const gchar * szStyle = nullptr;
		if (pSpanAP)
			pSpanAP->getAttribute(PT_STYLE_ATTRIBUTE_NAME, szStyle);
		if (szStyle && szStyle[0])
			sec.lines.push_back(s_style_chain(pDoc, szStyle));
		else
			sec.lines.push_back("Default Paragraph Font");
		out.push_back(std::move(sec));
	}

	/* ---- Direct formatting -------------------------------------- */
	{
		AP_FmtReportSection sec;
		sec.heading = "Direct Formatting";
		std::vector<std::string> c, p;
		s_dump_props(pSpanAP, c);
		s_dump_props(pBlockAP, p);
		if (c.empty() && p.empty())
		{
			sec.lines.push_back("None - all formatting comes from styles.");
		}
		else
		{
			for (const std::string & s : c)
				sec.lines.push_back("character: " + s);
			for (const std::string & s : p)
				sec.lines.push_back("paragraph: " + s);
		}
		out.push_back(std::move(sec));
	}

	/* ---- Resolved formatting ------------------------------------ */
	{
		PP_PropertyVector cprops, bprops;
		std::vector<std::string> lines;
		if (pView->getCharFormat(cprops))
			s_dump_vector(cprops, s_resolvedCharProps,
						  sizeof(s_resolvedCharProps) /
						  sizeof(s_resolvedCharProps[0]),
						  nullptr, lines);
		if (pView->getBlockFormat(bprops))
			s_dump_vector(bprops, s_resolvedBlockProps,
						  sizeof(s_resolvedBlockProps) /
						  sizeof(s_resolvedBlockProps[0]),
						  "paragraph ", lines);
		if (!lines.empty())
		{
			AP_FmtReportSection sec;
			sec.heading = "Resolved Formatting";
			sec.lines = std::move(lines);
			out.push_back(std::move(sec));
		}
	}

	/* ---- Context ------------------------------------------------ */
	{
		AP_FmtReportSection sec;
		sec.heading = "Context";

		if (pBlockAP)
		{
			const gchar * szListId = nullptr;
			const gchar * szLevel = nullptr;
			pBlockAP->getAttribute(PT_LISTID_ATTRIBUTE_NAME, szListId);
			pBlockAP->getAttribute(PT_LEVEL_ATTRIBUTE_NAME, szLevel);
			if (szListId && szListId[0] && strcmp(szListId, "0") != 0)
			{
				std::string line = "List item: listid ";
				line += szListId;
				if (szLevel && szLevel[0])
				{
					line += ", level ";
					line += szLevel;
				}
				sec.lines.push_back(std::move(line));
			}
		}

		/* walk the containment chain up to (not including) the doc
		 * section — its myContainingLayout() is itself, so the loop
		 * must stop on type, not on nullptr; the self-check also
		 * guards any other self-pointing container */
		for (fl_ContainerLayout * pCL = pBlock->getSectionLayout();
			 pCL && pCL->getContainerType() != FL_CONTAINER_DOCSECTION &&
			 pCL->myContainingLayout() != pCL;
			 pCL = pCL->myContainingLayout())
		{
			switch (pCL->getContainerType())
			{
			case FL_CONTAINER_CELL:
			{
				const PP_AttrProp * pCellAP = nullptr;
				pCL->getAP(pCellAP);
				const gchar * szRow = nullptr;
				const gchar * szCol = nullptr;
				if (pCellAP)
				{
					pCellAP->getProperty("top-attach", szRow);
					pCellAP->getProperty("left-attach", szCol);
				}
				std::string line = "Inside table cell";
				if (szRow && szCol)
				{
					line += " (row ";
					line += szRow;
					line += ", column ";
					line += szCol;
					line += ")";
				}
				sec.lines.push_back(std::move(line));
				break;
			}
			case FL_CONTAINER_FRAME:
				sec.lines.push_back("Inside a text frame");
				break;
			case FL_CONTAINER_HDRFTR:
			{
				fl_HdrFtrSectionLayout * pHF =
					static_cast<fl_HdrFtrSectionLayout *>(pCL);
				const char * szWhat = "header or footer";
				switch (pHF->getHFType())
				{
				case FL_HDRFTR_HEADER:
				case FL_HDRFTR_HEADER_EVEN:
				case FL_HDRFTR_HEADER_FIRST:
				case FL_HDRFTR_HEADER_LAST:
					szWhat = "header";
					break;
				case FL_HDRFTR_FOOTER:
				case FL_HDRFTR_FOOTER_EVEN:
				case FL_HDRFTR_FOOTER_FIRST:
				case FL_HDRFTR_FOOTER_LAST:
					szWhat = "footer";
					break;
				default:
					break;
				}
				std::string line = "Inside a ";
				line += szWhat;
				sec.lines.push_back(std::move(line));
				break;
			}
			case FL_CONTAINER_FOOTNOTE:
				sec.lines.push_back("Inside a footnote");
				break;
			case FL_CONTAINER_ENDNOTE:
				sec.lines.push_back("Inside an endnote");
				break;
			case FL_CONTAINER_MARGINNOTE:
				sec.lines.push_back("Inside a margin note");
				break;
			case FL_CONTAINER_ANNOTATION:
				sec.lines.push_back("Inside a comment");
				break;
			case FL_CONTAINER_TOC:
				sec.lines.push_back("Inside a table of contents");
				break;
			default:
				break;
			}
		}
		if (sec.lines.empty())
			sec.lines.push_back("Body text");
		out.push_back(std::move(sec));
	}
}

std::string AP_FmtInspect::toText(const std::vector<AP_FmtReportSection> & secs)
{
	std::string out;
	for (const AP_FmtReportSection & sec : secs)
	{
		out += sec.heading;
		out += "\n";
		for (const std::string & line : sec.lines)
		{
			out += "  ";
			out += line;
			out += "\n";
		}
		out += "\n";
	}
	return out;
}
