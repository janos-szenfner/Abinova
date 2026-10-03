/* AbiSource
 *
 * Copyright (C) 2005 INdT
 * Copyright (C) 2025-2026 Abinova contributors
 * Author: Daniel d'Andrada T. de Carvalho <daniel.carvalho@indt.org.br>
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

#include <map>
#include <memory>
#include <string>

// Abinova includes
#include <vector>
#include "ut_string_class.h"

// Internal classes
class ODe_Style_Style;
class ODe_Style_PageLayout;
class ODe_Style_List;

// Abinova classes
typedef struct _GsfOutput GsfOutput;

/**
 * Represents a <office:automatic-styles> element.
 */
class ODe_AutomaticStyles {

public:

    ~ODe_AutomaticStyles();

    void storeTextStyle(ODe_Style_Style*& rpTextStyle);
    void storeParagraphStyle(ODe_Style_Style*& rpParagraphStyle);
    void storeSectionStyle(ODe_Style_Style*& rpSectionStyle);
    void storeGraphicStyle(ODe_Style_Style*& rpGraphicStyle);

    ODe_Style_Style* addTableStyle(const UT_UTF8String& rStyleName);
    ODe_Style_Style* addTableColumnStyle(const UT_UTF8String& rStyleName);
    ODe_Style_Style* addTableRowStyle(const UT_UTF8String& rStyleName);
    ODe_Style_Style* addTableCellStyle(const UT_UTF8String& rStyleName);
	ODe_Style_PageLayout* addPageLayout();
    ODe_Style_List* addListStyle();

    void addPageLayout(ODe_Style_PageLayout*& pPageLayout);

    ODe_Style_PageLayout* getPageLayout(const gchar* pName) {
        auto it = m_pageLayouts.find(pName);
        return it != m_pageLayouts.end() ? it->second : nullptr;
    };

    ODe_Style_PageLayout* getMasterPage(const gchar* pName) {
        auto it = m_pageLayouts.find(pName);
        return it != m_pageLayouts.end() ? it->second : nullptr;
    };

	UT_uint32 getSectionStylesCount() const {
        return m_sectionStyles.size();
    }

    std::unique_ptr<std::vector<ODe_Style_Style*>> getParagraphStyles() const {
        return _mapValues(m_paragraphStyles);
    }

    std::unique_ptr<std::vector<ODe_Style_Style*>> getTextStyles() const {
        return _mapValues(m_textStyles);
    }

    std::unique_ptr<std::vector<ODe_Style_List*>> getListStyles() const {
        return _mapValues(m_listStyles);
    }

    // Writes <office:automatic-styles> element.
    void write(GsfOutput* pContentStream) const;

private:
    void _storeStyle(ODe_Style_Style*& rpStyle,
                     std::map<std::string, ODe_Style_Style*>& rStyles,
                     const char* pNamingPrefix);

    template <typename T>
    static std::unique_ptr<std::vector<T>> _mapValues(
            const std::map<std::string, T>& rMap) {
        auto pVec = std::make_unique<std::vector<T>>();
        pVec->reserve(rMap.size());
        for (const auto& kv : rMap) {
            pVec->push_back(kv.second);
        }
        return pVec;
    }

    std::map<std::string, ODe_Style_Style*> m_textStyles;
    std::map<std::string, ODe_Style_Style*> m_paragraphStyles;
    std::map<std::string, ODe_Style_Style*> m_sectionStyles;
    std::map<std::string, ODe_Style_Style*> m_tableStyles;
    std::map<std::string, ODe_Style_Style*> m_tableColumnStyles;
    std::map<std::string, ODe_Style_Style*> m_tableRowStyles;
    std::map<std::string, ODe_Style_Style*> m_tableCellStyles;
    std::map<std::string, ODe_Style_Style*> m_graphicStyles;
    std::map<std::string, ODe_Style_PageLayout*> m_pageLayouts;
    std::map<std::string, ODe_Style_List*> m_listStyles;
};
