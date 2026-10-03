/* AbiSource Program Utilities
 * 
 * Copyright (C) 2005 Daniel d'Andrada T. de Carvalho
 * Copyright (C) 2025-2026 Abinova contributors
 * <daniel.carvalho@indt.org.br>
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
#include <vector>
#include "ODi_ElementStack.h"
 
// Internal includes
#include "ODi_StartTag.h"

// Abinova includes
#include "ut_string.h"


/**
 * Constructor
 */
ODi_ElementStack::ODi_ElementStack() :
                   m_pStartTags(nullptr),
                   m_stackSize(0) {
            
}


/**
 * Destructor
 */
ODi_ElementStack::~ODi_ElementStack() {

    if (m_pStartTags) {
        for (ODi_StartTag* _utv_p : (*m_pStartTags)) { if (_utv_p) delete(_utv_p); };
    }
    DELETEP(m_pStartTags);
}


/**
 * Must be the last command called by the starElement method of the listener
 * class.
 */
void ODi_ElementStack::startElement (const gchar* pName,
                                                 const gchar** ppAtts) {

    ODi_StartTag* pStartTag = nullptr;

    if (!m_pStartTags) {
        m_pStartTags = new std::vector<ODi_StartTag*> ();
    }

    if (m_stackSize == m_pStartTags->size()) { 
        
        pStartTag = new ODi_StartTag();
        m_pStartTags->push_back(pStartTag);
        
    } else if (m_stackSize < m_pStartTags->size()) {
        
        pStartTag = (*m_pStartTags)[m_stackSize];
        
    } else {
        UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
    }

    UT_return_if_fail(pStartTag != nullptr);
    pStartTag->set(pName, ppAtts);
    
    m_stackSize++;
}


/**
 * Must be the last command called by the endElement method of the listener
 * class.
 */
void ODi_ElementStack::endElement (const gchar* /*pName*/) {
    UT_ASSERT(m_pStartTags != nullptr);
    UT_return_if_fail(m_stackSize > 0);
    m_stackSize--;
}


/**
 * @param level 0 is the immediate parent, 1 is the parent of the parent
 *              and so on.
 * 
 * On the startElement method, level 0 is the parent start tag.
 * On the endElement method, level 0 is the corresponding start tag.
 */
const ODi_StartTag* ODi_ElementStack::getStartTag(UT_sint32 level) {
    
    if (m_pStartTags) {
        if (m_stackSize > level) {
            // The level is counted from the top of the vector down to the bottom
            // so, level 0 is m_pStartTags[lastIndex] and
            // level max is m_pStartTags[0]
            return (*m_pStartTags)[m_stackSize - (level+1)];
        } else {
            return nullptr;
        }
    } else {
        return nullptr;
    }
}



/**
 * Returns the name of the start tag at the given level, or "" if the stack
 * does not reach that level.
 */
const char* ODi_ElementStack::getStartTagName(UT_sint32 level) {

    const ODi_StartTag* pStartTag = getStartTag(level);
    return pStartTag ? pStartTag->getName() : "";
}



/**
 * Returns the value of attribute pName on the start tag at the given level,
 * or nullptr if the stack does not reach that level or the attribute is
 * absent.
 */
const char* ODi_ElementStack::getStartTagAttribute(UT_sint32 level,
                                                 const char* pName) {

    const ODi_StartTag* pStartTag = getStartTag(level);
    return pStartTag ? pStartTag->getAttributeValue(pName) : nullptr;
}



/**
 * @return True if at least one of the stack elements has the specified name.
 */
bool ODi_ElementStack::hasElement(const gchar* pName) const {
    UT_sint32 i;
    ODi_StartTag* pStartTag;
    
    for (i=0; i<m_stackSize; i++) {
        pStartTag = (*m_pStartTags)[i];
        UT_nonnull_or_continue(pStartTag);
        if (!strcmp(pStartTag->getName(), pName)) {
            return true;
        }
    }
    
    // If the execution reached this line it's because no match was found.
    return false;
}


/**
 * Returns the closest parent with the given name. It returns nullptr if there
 * is no parent with the given name.
 * 
 * @param pName Element name.
 * @param fromLevel The level from which the search begins.
 */
const ODi_StartTag* ODi_ElementStack::getClosestElement(
                                                  const gchar* pName,
                                                  UT_sint32 fromLevel) const {
                                                    
    if (m_pStartTags && fromLevel < m_stackSize) {
        UT_sint32 level;
        ODi_StartTag* pStartTag;
        
        for (level=fromLevel; level<m_stackSize; level++) {
            // The level is counted from the top of the vector down to the bottom
            // so, level 0 is m_pStartTags[lastIndex] and
            // level max is m_pStartTags[0]
            pStartTag = (*m_pStartTags)[m_stackSize - (level+1)];
            UT_nonnull_or_continue(pStartTag);
            if (!strcmp(pStartTag->getName(), pName)) {
                return pStartTag;
            }
        }

    }
    
    // Nothing was found.
    return nullptr;
}


/**
 * Returns the level of the closest element with the given name.
 */
UT_sint32 ODi_ElementStack::getElementLevel(const gchar* pName) const {
    if (m_pStartTags) {
        UT_sint32 level;
        ODi_StartTag* pStartTag;
        
        for (level=0; level<m_stackSize; level++) {
            // The level is counted from the top of the vector down to the bottom
            // so, level 0 is m_pStartTags[lastIndex] and
            // level max is m_pStartTags[0]
            pStartTag = (*m_pStartTags)[m_stackSize - (level+1)];
            UT_nonnull_or_continue(pStartTag);
            if (!strcmp(pStartTag->getName(), pName)) {
                return level;
            }
        }

    }
    
    UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
    return 0;
}
