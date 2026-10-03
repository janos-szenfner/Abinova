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

#ifndef _ODI_ELEMENTSTACK_H_
#define _ODI_ELEMENTSTACK_H_

// Abinova includes
#include <vector>
#include "ut_types.h"
#include "ut_vector.h"

// Internal classes
class ODi_StartTag;


/**
 * This class stores info about the parent elements of the current element being
 * parsed.
 *
 * Example:
 *
 * If you are parsing the <style:footnote-sep/> element, this class will hold
 * info about its parents. On this case: <style:page-layout-properties>,
 * <style:page-layout> and <office:automatic-styles>.
 *
 * <office:automatic-styles>
 *    <style:page-layout>
 *        <style:page-layout-properties>
 *            <style:footnote-sep/>
 *        </style:page-layout-properties>
 *    </style:page-layout>
 * </office:automatic-styles>
 *
 *
 * When parsing a start element tag, level 0 is the parent element.
 *
 * When parsing a start element tag, level 0 is its own element, with level 1
 * being the parent and so on.
 *
 */
class ODi_ElementStack {

public:

    ODi_ElementStack();
    ~ODi_ElementStack();

    /**
     * @param level 0 is the immediate parent, 1 is the parent of the parent
     *              and so on.
     *
     * On the startElement method, level 0 is the parent start tag.
     * On the endElement method, level 0 is the corresponding start tag.
     */
    const ODi_StartTag* getStartTag(UT_sint32 level);

    /**
     * Returns the name of the start tag at the given level, or "" if the
     * stack does not reach that level (e.g. a malformed document nests an
     * element shallower than expected).
     */
    const char* getStartTagName(UT_sint32 level);

    /**
     * Returns the value of attribute pName on the start tag at the given
     * level, or nullptr if the stack does not reach that level or the
     * attribute is absent.
     */
    const char* getStartTagAttribute(UT_sint32 level, const char* pName);

    bool hasElement(const gchar* pName) const;

    /**
     * Returns the level of the closest element with the given name.
     */
    UT_sint32 getElementLevel(const gchar* pName) const;

    /**
     * Returns the closest parent with the given name. It returns nullptr if there
     * is no parent with the given name.
     *
     * @param pName Element name.
     * @param fromLevel The level from which the search begins.
     */
    const ODi_StartTag* getClosestElement(const gchar* pName,
                                          UT_sint32 fromLevel = 0) const;


    void startElement (const gchar* pName, const gchar** ppAtts);
    void endElement (const gchar* pName);
    bool isEmpty() const {return m_stackSize==0;}
    void clear() {m_stackSize = 0;}

    UT_sint32 getStackSize() const {return m_stackSize;}


private:

    std::vector<ODi_StartTag*>* m_pStartTags;
    UT_sint32 m_stackSize;
};

#endif //_ODI_ELEMENTSTACK_H_
