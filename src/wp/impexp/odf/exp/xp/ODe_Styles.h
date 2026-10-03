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

// Internal includes
#include "ODe_DefaultStyles.h"

// Internal classes
class ODe_Style_Style;
class ODe_Style_MasterPage;
class ODe_Style_PageLayout;

// Abinova classes
class PD_Document;
class PP_AttrProp;
class PD_Style;
typedef struct _GsfOutput GsfOutput;

/**
 * This class stores all normal and automatic styles.
 */
class ODe_Styles {
public:

    ODe_Styles(PD_Document* pAbiDoc);
    ODe_Styles(const ODe_Styles&) = delete;
    ODe_Styles& operator=(const ODe_Styles&) = delete;

    ~ODe_Styles();

    // Fetch all regular <style:style> elements (the ones that will be defined
    // inside <office:styles>).
    bool fetchRegularStyleStyles();

    // Writes the <office:styles> element.
    bool write(GsfOutput* pODT) const;

    ODe_DefaultStyles& getDefaultStyles() {
        return m_defaultStyles;
    }

	std::unique_ptr<std::vector<ODe_Style_Style*>> getParagraphStylesEnumeration() const {
        return _mapValues(m_paragraphStyles);
    }

    std::unique_ptr<std::vector<ODe_Style_Style*>> getTextStylesEnumeration() const {
        return _mapValues(m_textStyles);
    }

    std::unique_ptr<std::vector<ODe_Style_Style*>> getGraphicStylesEnumeration() const {
        return _mapValues(m_graphicStyles);
    }

	ODe_Style_Style* getGraphicsStyle(const gchar* name) {
        auto it = m_graphicStyles.find(name);
        return it != m_graphicStyles.end() ? it->second : nullptr;
	}

	void addGraphicsStyle(ODe_Style_Style* pStyle);

    void addStyle(const UT_UTF8String& sStyle);

private:
    bool _addStyle(const PP_AttrProp* pAP);
    bool _writeStyles(GsfOutput* pODT, const std::unique_ptr<std::vector<ODe_Style_Style*>>& pStyleVector) const;

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

	PD_Document* m_pAbiDoc;
    ODe_DefaultStyles m_defaultStyles;
    std::map<std::string, ODe_Style_Style*> m_textStyles;
    std::map<std::string, ODe_Style_Style*> m_paragraphStyles;
	std::map<std::string, ODe_Style_Style*> m_graphicStyles;
};
