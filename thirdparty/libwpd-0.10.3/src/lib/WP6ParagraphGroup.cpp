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
 * Copyright (C) 2004 Fridrich Strba (fridrich.strba@bluewin.ch)
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

#include "WP6ParagraphGroup.h"
#include "libwpd_internal.h"
#include "WPXFileStructure.h"
#include "WP6FileStructure.h"
#include "WP6Listener.h"

WP6ParagraphGroup::WP6ParagraphGroup(librevenge::RVNGInputStream *input, WPXEncryption *encryption) :
	WP6VariableLengthGroup(),
	m_subGroupData()
{
	_read(input, encryption);
}

WP6ParagraphGroup::~WP6ParagraphGroup()
{
}

void WP6ParagraphGroup::_readContents(librevenge::RVNGInputStream *input, WPXEncryption *encryption)
{
	switch (getSubGroup())
	{
	case WP6_PARAGRAPH_GROUP_LINE_SPACING:
		m_subGroupData.reset(new WP6ParagraphGroup_LineSpacingSubGroup(input, encryption));
		break;
	case WP6_PARAGRAPH_GROUP_TAB_SET:
		m_subGroupData.reset(new WP6ParagraphGroup_TabSetSubGroup(input, encryption));
		break;
	case WP6_PARAGRAPH_GROUP_JUSTIFICATION:
		m_subGroupData.reset(new WP6ParagraphGroup_JustificationModeSubGroup(input, encryption));
		break;
	case WP6_PARAGRAPH_GROUP_SPACING_AFTER_PARAGRAPH:
		m_subGroupData.reset(new WP6ParagraphGroup_SpacingAfterParagraphSubGroup(input, encryption, getSizeNonDeletable()));
		break;
	case WP6_PARAGRAPH_GROUP_INDENT_FIRST_LINE_OF_PARAGRAPH:
		m_subGroupData.reset(new WP6ParagraphGroup_IndentFirstLineSubGroup(input, encryption));
		break;
	case WP6_PARAGRAPH_GROUP_LEFT_MARGIN_ADJUSTMENT:
		m_subGroupData.reset(new WP6ParagraphGroup_LeftMarginAdjustmentSubGroup(input, encryption));
		break;
	case WP6_PARAGRAPH_GROUP_RIGHT_MARGIN_ADJUSTMENT:
		m_subGroupData.reset(new WP6ParagraphGroup_RightMarginAdjustmentSubGroup(input, encryption));
		break;
	case WP6_PARAGRAPH_GROUP_OUTLINE_DEFINE:
		m_subGroupData.reset(new WP6ParagraphGroup_OutlineDefineSubGroup(input, encryption));
		break;
	default:
		break;
	}
}

void WP6ParagraphGroup::parse(WP6Listener *listener)
{
	WPD_DEBUG_MSG(("WordPerfect: handling a Paragraph group\n"));

	if (m_subGroupData)
		m_subGroupData->parse(listener, getNumPrefixIDs(), getPrefixIDs());
}

WP6ParagraphGroup_LineSpacingSubGroup::WP6ParagraphGroup_LineSpacingSubGroup(librevenge::RVNGInputStream *input, WPXEncryption *encryption) :
	m_lineSpacing(0)
{
	unsigned lineSpacing = readU32(input, encryption);
	auto lineSpacingIntegerPart = (signed short)((lineSpacing & 0xFFFF0000) >> 16);
	double lineSpacingFractionalPart = (double)(lineSpacing & 0xFFFF)/(double)0xFFFF;
	WPD_DEBUG_MSG(("WordPerfect: line spacing integer part: %i fractional part: %f (original value: %i)\n",
	               lineSpacingIntegerPart, lineSpacingFractionalPart, lineSpacing));
	m_lineSpacing = lineSpacingIntegerPart + lineSpacingFractionalPart;
}

void WP6ParagraphGroup_LineSpacingSubGroup::parse(WP6Listener *listener, const unsigned char /* numPrefixIDs */,
                                                  const unsigned short * /* prefixIDs */) const
{
	WPD_DEBUG_MSG(("WordPerfect: parsing a line spacing change of: %f\n", m_lineSpacing));
	listener->lineSpacingChange(m_lineSpacing);
}

WP6ParagraphGroup_TabSetSubGroup::WP6ParagraphGroup_TabSetSubGroup(librevenge::RVNGInputStream *input, WPXEncryption *encryption) :
	m_isRelative(false),
	m_tabAdjustValue(0.0),
	m_usePreWP9LeaderMethods(),
	m_tabStops()
{
	unsigned char definition = readU8(input, encryption);
	unsigned short tabAdjustValue = readU16(input, encryption);
	if (definition == 0)
	{
		m_isRelative = false;
		m_tabAdjustValue = 0.0;
	}
	else
	{
		m_isRelative = true;
		m_tabAdjustValue = double(tabAdjustValue)/WPX_NUM_WPUS_PER_INCH;
	}
	unsigned char repetitionCount = 0;
	WPXTabStop tabStop;
	unsigned char numTabStops = readU8(input, encryption);
	bool usePreWP9LeaderMethod = false;
	unsigned char tabType = 0;
	m_tabStops.reserve(numTabStops);
	for (int i = 0; i < numTabStops; i++)
	{
		tabType = readU8(input, encryption);
		if ((tabType & 0x80) != 0)
		{
			repetitionCount = (tabType & 0x7F);
		}
		else
		{
			switch (tabType & 0x0F) //alignment bits
			{
			case 0x00:
				tabStop.m_alignment = LEFT;
				break;
			case 0x01:
				tabStop.m_alignment = CENTER;
				break;
			case 0x02:
				tabStop.m_alignment = RIGHT;
				break;
			case 0x03:
				tabStop.m_alignment = DECIMAL;
				break;
			case 0x04:
				tabStop.m_alignment = BAR;
				break;
			default: // should not happen, maybe corruption
				tabStop.m_alignment = LEFT;
				break;
			}
			tabStop.m_leaderNumSpaces = 0;
			if ((tabType & 0x10) == 0) // no leader character
			{
				tabStop.m_leaderCharacter = '\0';
				usePreWP9LeaderMethod = false;
			}
			else
			{
				switch ((tabType & 0x60) >> 5) // leader character type
				{
				case 0: // pre-WP9 leader method
					tabStop.m_leaderCharacter = '.';
					tabStop.m_leaderNumSpaces = 0;
					usePreWP9LeaderMethod = true;
					break;
				case 1: // dot leader
					tabStop.m_leaderCharacter = '.';
					tabStop.m_leaderNumSpaces = 0;
					usePreWP9LeaderMethod = false;
					break;
				case 2: // hyphen leader
					tabStop.m_leaderCharacter = '-';
					tabStop.m_leaderNumSpaces = 0;
					usePreWP9LeaderMethod = false;
					break;
				case 3: // underscore leader
					tabStop.m_leaderCharacter = '_';
					tabStop.m_leaderNumSpaces = 0;
					usePreWP9LeaderMethod = false;
					break;
				default:
					break;
				}
			}
		}
		unsigned short tabPosition = readU16(input, encryption);
		if (repetitionCount == 0)
		{
			if (tabPosition != 0xFFFF)
			{
				tabStop.m_position = double(tabPosition)/WPX_NUM_WPUS_PER_INCH - m_tabAdjustValue;
				m_tabStops.push_back(tabStop);
				m_usePreWP9LeaderMethods.push_back(usePreWP9LeaderMethod);
			}
		}
		else
		{
			m_tabStops.reserve(m_tabStops.capacity() + repetitionCount);
			for (int k=0; k<repetitionCount; k++)
			{
				tabStop.m_position += double(tabPosition)/WPX_NUM_WPUS_PER_INCH;
				m_tabStops.push_back(tabStop);
				m_usePreWP9LeaderMethods.push_back(usePreWP9LeaderMethod);
			}
			repetitionCount = 0;
		}
	}
	m_tabStops.shrink_to_fit();
}

WP6ParagraphGroup_TabSetSubGroup::~WP6ParagraphGroup_TabSetSubGroup()
{
}

void WP6ParagraphGroup_TabSetSubGroup::parse(WP6Listener *listener, const unsigned char /* numPrefixIDs */,
                                             const unsigned short * /* prefixIDs */) const
{
#ifdef DEBUG
	WPD_DEBUG_MSG(("Parsing Tab Set (isRelative: %s, positions: ", (m_isRelative?"true":"false")));
	for (std::vector<WPXTabStop>::const_iterator i = m_tabStops.begin(); i != m_tabStops.end(); ++i)
	{
		WPD_DEBUG_MSG((" %.4f", (*i).m_position));
	}
	WPD_DEBUG_MSG((")\n"));
#endif
	listener->defineTabStops(m_isRelative, m_tabStops, m_usePreWP9LeaderMethods);
}

WP6ParagraphGroup_IndentFirstLineSubGroup::WP6ParagraphGroup_IndentFirstLineSubGroup(librevenge::RVNGInputStream *input, WPXEncryption *encryption) :
	m_firstLineOffset(0)
{
	m_firstLineOffset = (signed short)readU16(input, encryption);
	WPD_DEBUG_MSG(("WordPerfect: indent first line: %i\n", m_firstLineOffset));
}

void WP6ParagraphGroup_IndentFirstLineSubGroup::parse(WP6Listener *listener, const unsigned char /* numPrefixIDs */,
                                                      const unsigned short * /* prefixIDs */) const
{
	WPD_DEBUG_MSG(("WordPerfect: parsing first line indent change of: %i\n", m_firstLineOffset));
	listener->indentFirstLineChange(m_firstLineOffset);
}

WP6ParagraphGroup_LeftMarginAdjustmentSubGroup::WP6ParagraphGroup_LeftMarginAdjustmentSubGroup(librevenge::RVNGInputStream *input, WPXEncryption *encryption) :
	m_leftMargin(0)
{
	m_leftMargin = (signed short)readU16(input, encryption);
	WPD_DEBUG_MSG(("WordPerfect: left margin adjustment: %i\n", m_leftMargin));
}

void WP6ParagraphGroup_LeftMarginAdjustmentSubGroup::parse(WP6Listener *listener, const unsigned char /* numPrefixIDs */,
                                                           const unsigned short * /* prefixIDs */) const
{
	WPD_DEBUG_MSG(("WordPerfect: parsing left margin adjustment change of: %i\n", m_leftMargin));
	listener->paragraphMarginChange(WPX_LEFT, m_leftMargin);
}

WP6ParagraphGroup_RightMarginAdjustmentSubGroup::WP6ParagraphGroup_RightMarginAdjustmentSubGroup(librevenge::RVNGInputStream *input, WPXEncryption *encryption) :
	m_rightMargin(0)
{
	m_rightMargin = (signed short)readU16(input, encryption);
	WPD_DEBUG_MSG(("WordPerfect: right margin adjustment: %i\n", m_rightMargin));
}

void WP6ParagraphGroup_RightMarginAdjustmentSubGroup::parse(WP6Listener *listener, const unsigned char /* numPrefixIDs */,
                                                            const unsigned short * /* prefixIDs */) const
{
	WPD_DEBUG_MSG(("WordPerfect: parsing right margin adjustment change of: %i\n", m_rightMargin));
	listener->paragraphMarginChange(WPX_RIGHT, m_rightMargin);
}

WP6ParagraphGroup_JustificationModeSubGroup::WP6ParagraphGroup_JustificationModeSubGroup(librevenge::RVNGInputStream *input, WPXEncryption *encryption) :
	m_justification(0)
{
	m_justification = readU8(input, encryption);
}

void WP6ParagraphGroup_JustificationModeSubGroup::parse(WP6Listener *listener, const unsigned char /* numPrefixIDs */,
                                                        const unsigned short * /* prefixIDs */) const
{
	listener->justificationChange(m_justification);
}

WP6ParagraphGroup_SpacingAfterParagraphSubGroup::WP6ParagraphGroup_SpacingAfterParagraphSubGroup(librevenge::RVNGInputStream *input,
                                                                                                 WPXEncryption *encryption, const unsigned short sizeNonDeletable) :
	m_spacingAfterParagraphAbsolute(0.0),
	m_spacingAfterParagraphRelative(1.0),
	m_sizeNonDeletable(sizeNonDeletable)
{
	unsigned spacingAfterRelative = readU32(input, encryption);
	auto spacingAfterIntegerPart = (signed short)((spacingAfterRelative & 0xFFFF0000) >> 16);
	double spacingAfterFractionalPart = (double)(spacingAfterRelative & 0xFFFF)/(double)0xFFFF;
	WPD_DEBUG_MSG(("WordPerfect: spacing after paragraph relative integer part: %i fractional part: %f (original value: %i)\n",
	               spacingAfterIntegerPart, spacingAfterFractionalPart, spacingAfterRelative));
	m_spacingAfterParagraphRelative = spacingAfterIntegerPart + spacingAfterFractionalPart;
	if (m_sizeNonDeletable == (unsigned short)0x06) // Let us use the optional information that is in WPUs
	{
		unsigned short spacingAfterAbsolute = readU16(input, encryption);
		m_spacingAfterParagraphAbsolute = double(spacingAfterAbsolute) / WPX_NUM_WPUS_PER_INCH;
		WPD_DEBUG_MSG(("WordPerfect: spacing after paragraph absolute: %i\n", spacingAfterAbsolute));
	}
}

void WP6ParagraphGroup_SpacingAfterParagraphSubGroup::parse(WP6Listener *listener, const unsigned char /* numPrefixIDs */,
                                                            const unsigned short * /* prefixIDs */) const
{
	WPD_DEBUG_MSG(("WordPerfect: parsing a change of spacing after paragraph: relative %f, absolute %f\n",
	               m_spacingAfterParagraphRelative, m_spacingAfterParagraphAbsolute));
	listener->spacingAfterParagraphChange(m_spacingAfterParagraphRelative, m_spacingAfterParagraphAbsolute);
}

WP6ParagraphGroup_OutlineDefineSubGroup::WP6ParagraphGroup_OutlineDefineSubGroup(librevenge::RVNGInputStream *input, WPXEncryption *encryption) :
	m_outlineHash(0),
	m_tabBehaviourFlag(0)
{
	// NB: this is identical to WP6OutlineStylePacket::_readContents!!
	m_outlineHash = readU16(input, encryption);
	for (unsigned char &numberingMethod : m_numberingMethods)
		numberingMethod = readU8(input, encryption);
	m_tabBehaviourFlag = readU8(input, encryption);

	WPD_DEBUG_MSG(("WordPerfect: Read Outline Style Packet (, outlineHash: %i, tab behaviour flag: %i)\n", (int) m_outlineHash, (int) m_tabBehaviourFlag));
	WPD_DEBUG_MSG(("WordPerfect: Read Outline Style Packet (m_numberingMethods: %i %i %i %i %i %i %i %i)\n",
	               m_numberingMethods[0], m_numberingMethods[1], m_numberingMethods[2], m_numberingMethods[3],
	               m_numberingMethods[4], m_numberingMethods[5], m_numberingMethods[6], m_numberingMethods[7]));
}

void WP6ParagraphGroup_OutlineDefineSubGroup::parse(WP6Listener *listener, const unsigned char /* numPrefixIDs */,
                                                    const unsigned short * /* prefixIDs */) const
{
	listener->updateOutlineDefinition(m_outlineHash, m_numberingMethods, m_tabBehaviourFlag);
}
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
