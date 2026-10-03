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

// Class definition include
#include "ODe_AutomaticStyles.h"

// Internal includes
#include "ODe_Common.h"

// Internal classes
#include "ODe_Style_Style.h"
#include "ODe_Style_PageLayout.h"
#include "ODe_Style_List.h"


/**
 * Destructor
 */
ODe_AutomaticStyles::~ODe_AutomaticStyles() {
    for (auto* pMap : {&m_textStyles, &m_paragraphStyles, &m_sectionStyles,
                       &m_tableStyles, &m_tableColumnStyles, &m_tableRowStyles,
                       &m_tableCellStyles, &m_graphicStyles}) {
        for (auto& kv : *pMap) {
            delete kv.second;
        }
    }

    for (auto& kv : m_pageLayouts) {
        delete kv.second;
    }

    for (auto& kv : m_listStyles) {
        delete kv.second;
    }
}


/**
 * See ODe_AutomaticStyles::_storeStyle
 */
void ODe_AutomaticStyles::storeTextStyle(ODe_Style_Style*& rpTextStyle) {
    _storeStyle(rpTextStyle, m_textStyles, "T");
}


/**
 * See ODe_AutomaticStyles::_storeStyle
 */
void ODe_AutomaticStyles::storeParagraphStyle(ODe_Style_Style*& rpParagraphStyle) {
    _storeStyle(rpParagraphStyle, m_paragraphStyles, "P");
}


/**
 * See ODe_AutomaticStyles::_storeStyle
 */
void ODe_AutomaticStyles::storeSectionStyle(ODe_Style_Style*& rpSectionStyle) {
    _storeStyle(rpSectionStyle, m_sectionStyles, "Sect");
}


/**
 * See ODe_AutomaticStyles::_storeStyle
 */
void ODe_AutomaticStyles::storeGraphicStyle(ODe_Style_Style*& rpGraphicStyle) {
    _storeStyle(rpGraphicStyle, m_graphicStyles, "graphic");
}


/**
 * 
 */
ODe_Style_Style* ODe_AutomaticStyles::addTableStyle(
                                            const UT_UTF8String& rStyleName) {
                                                
    ODe_Style_Style* pStyle;
   
    pStyle = new ODe_Style_Style();
    pStyle->setStyleName(rStyleName);
    pStyle->setFamily("table");
    
    m_tableStyles.emplace(rStyleName.utf8_str(), pStyle);
    
    return pStyle;
}


/**
 * 
 */
ODe_Style_Style* ODe_AutomaticStyles::addTableColumnStyle(
                                            const UT_UTF8String& rStyleName) {
                                                
    ODe_Style_Style* pStyle;
   
    pStyle = new ODe_Style_Style();
    pStyle->setStyleName(rStyleName);
    pStyle->setFamily("table-column");
    
    m_tableColumnStyles.emplace(rStyleName.utf8_str(), pStyle);
    
    return pStyle;
}


/**
 * 
 */
ODe_Style_Style* ODe_AutomaticStyles::addTableRowStyle(
                                            const UT_UTF8String& rStyleName) {
                                                
    ODe_Style_Style* pStyle;
   
    pStyle = new ODe_Style_Style();
    pStyle->setStyleName(rStyleName);
    pStyle->setFamily("table-row");
    
    m_tableRowStyles.emplace(rStyleName.utf8_str(), pStyle);
    
    return pStyle;
}


/**
 * 
 */
ODe_Style_Style* ODe_AutomaticStyles::addTableCellStyle(
                                            const UT_UTF8String& rStyleName) {
    ODe_Style_Style* pStyle;
   
    pStyle = new ODe_Style_Style();
    pStyle->setStyleName(rStyleName);
    pStyle->setFamily("table-cell");
    
    m_tableCellStyles.emplace(rStyleName.utf8_str(), pStyle);
    
    return pStyle;
}


/**
 * 
 */
ODe_Style_PageLayout* ODe_AutomaticStyles::addPageLayout() {
    ODe_Style_PageLayout* pStyle;
    UT_UTF8String styleName;
   
    UT_UTF8String_sprintf(styleName, "PLayout%d", m_pageLayouts.size() + 1);
    
    pStyle = new ODe_Style_PageLayout();
    pStyle->setName(styleName);
    
    m_pageLayouts.emplace(styleName.utf8_str(), pStyle);
    
    return pStyle;
}


/**
 * 
 */
ODe_Style_List* ODe_AutomaticStyles::addListStyle() {
    ODe_Style_List* pStyle;
    UT_UTF8String styleName;
   
    UT_UTF8String_sprintf(styleName, "L%d", m_listStyles.size() + 1);
    
    pStyle = new ODe_Style_List();
    pStyle->setName(styleName);
    
    m_listStyles.emplace(styleName.utf8_str(), pStyle);
    
    return pStyle;
}


/**
 * 
 */
void ODe_AutomaticStyles::addPageLayout(ODe_Style_PageLayout*& pPageLayout) {
    m_pageLayouts.emplace(pPageLayout->getName(), pPageLayout);
}


/**
 * Writes <office:automatic-styles> element.
 */
void ODe_AutomaticStyles::write(GsfOutput* pContentStream) const {
    UT_UTF8String spacesOffset = "  ";

    ODe_writeUTF8String(pContentStream, " <office:automatic-styles>\n");

#define ODE_WRITE_STYLES(styleMap) \
    {for (const auto& kv : styleMap) { \
        kv.second->write(pContentStream, spacesOffset); \
    }}


    ODE_WRITE_STYLES (m_textStyles);
    ODE_WRITE_STYLES (m_paragraphStyles);
    ODE_WRITE_STYLES (m_sectionStyles);
    ODE_WRITE_STYLES (m_tableStyles);
    ODE_WRITE_STYLES (m_tableColumnStyles);
    ODE_WRITE_STYLES (m_tableRowStyles);
    ODE_WRITE_STYLES (m_tableCellStyles);
    ODE_WRITE_STYLES (m_graphicStyles);

#undef ODE_WRITE_STYLES

    for (const auto& kv : m_pageLayouts) {
        kv.second->write(pContentStream, spacesOffset);
    }

    for (const auto& kv : m_listStyles) {
        kv.second->write(pContentStream, spacesOffset);
    }

    ODe_writeUTF8String(pContentStream, " </office:automatic-styles>\n");
}


/**
 * Store the style in this automatic styles holder. As the specified
 * style get's stored here, this class takes care of freeing its memory later, so
 * you don't have to worry about freeing the memory of the stored style.
 * 
 * The style also get's it's unique name on this method.
 * 
 * After calling this method you may end up with your style pointer pointing to
 * a different style. It happens when there is already a stored style equivalent
 * to the one that you sent to be stored. The one that was passed is deleted.
 */
void ODe_AutomaticStyles::_storeStyle(ODe_Style_Style*& rpStyle,
                     std::map<std::string, ODe_Style_Style*>& rStyles,
                     const char* pNamingPrefix) {

    bool isDuplicated = false;

    for (const auto& kv : rStyles) {

        ODe_Style_Style* pStyle = kv.second;
        if ( pStyle->isEquivalentTo(*rpStyle) ) {
            isDuplicated = true; // exit the loop
            delete rpStyle; // We don't want a duplicated style.
            rpStyle = pStyle;
            break;
        }
    }


    if (!isDuplicated) {
        // Let's name and store this style.
        UT_UTF8String styleName;

        UT_UTF8String_sprintf(styleName, "%s%d", pNamingPrefix, rStyles.size() + 1);

        rpStyle->setStyleName(styleName);
        rStyles.emplace(styleName.utf8_str(), rpStyle);
    }
}
