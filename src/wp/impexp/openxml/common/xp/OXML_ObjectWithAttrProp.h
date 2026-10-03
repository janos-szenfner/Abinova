/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
/* AbiSource
 *
 * Copyright (C) 2007 Philippe Milot <PhilMilot@gmail.com>
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

#ifndef _OXML_OBJECTWITHATTRPROP_H_
#define _OXML_OBJECTWITHATTRPROP_H_

#include <string>
#include <vector>

#include "pp_AttrProp.h"
#include "OXML_Types.h"

class IE_Exp_OpenXML;
class PD_Document;

/* \struct OXML_StruxRevision
 * \brief Deferred strux-level tracked-change record (ECMA-376
 * §17.13.5). The piece table can carry a "revision" attribute on
 * strux attr/prop sets, and the *Change elements (w:pPrChange &c.)
 * carry the PRE-change property set as a child snapshot.  Neither is
 * known fully at parse time — the piece-table revision id is only
 * assigned once a PD_Document is available — so the importer stores
 * pending marks here and materializes them in applyRevisionMarks().
 *
 * Two shapes:
 *  - name == "revision": emitted as revision="+id"/"-id" on the strux
 *    AP (w:cellIns / w:cellDel / w:tr w:ins / w:tr w:del fan-out).
 *  - name == the OOXML element name ("pPrChange", "tblPrChange", ...):
 *    emitted as an inert attribute "name"="!id{props}{attrs}" where
 *    the brace groups hold the captured pre-change property and
 *    attribute sets in the usual name:value; encoding — the change is
 *    accepted (live props keep the new state) but the old snapshot is
 *    preserved for a future exporter.
 */
struct OXML_StruxRevision {
	std::string name;
	bool deletion = false;             // '-' vs '+' for "revision" marks
	std::string author, date;
	PP_PropertyVector props, attrs;    // old property snapshot (*Change)
};

class OXML_ObjectWithAttrProp {
public:
	OXML_ObjectWithAttrProp();
	// owns m_pAttributes; elements are managed by pointer in the doc tree —
	// a shallow copy would double-free
	OXML_ObjectWithAttrProp(const OXML_ObjectWithAttrProp&) = delete;
	OXML_ObjectWithAttrProp& operator=(const OXML_ObjectWithAttrProp&) = delete;
	virtual ~OXML_ObjectWithAttrProp();

	UT_Error setAttribute(const gchar * szName, const gchar * szValue);
	UT_Error setProperty(const gchar * szName, const gchar * szValue);
	UT_Error setProperty(const std::string & szName, const std::string & szValue);
	UT_Error getAttribute(const gchar * szName, const gchar *& szValue) const;
	UT_Error getProperty(const gchar * szName, const gchar *& szValue) const;
	UT_Error setAttributes(const PP_PropertyVector & attributes);
	UT_Error setProperties(const PP_PropertyVector & properties);
	UT_Error appendAttributes(const PP_PropertyVector & attributes);
	UT_Error appendProperties(const PP_PropertyVector & properties);
	PP_PropertyVector getAttributes() const;
	PP_PropertyVector getProperties() const;

	UT_Error inheritProperties(OXML_ObjectWithAttrProp* parent);

	//! Provides the list of attributes including all the properties in one attribute.
	/*! This method takes all the properties of the object and combines them into one string in CSS
 	 *  format.  This string is used as the value of a new attribute whose key is defined by PP_PROPS_ATTRIBUTE_NAME.
	 	\return A list of all the object's attributes and with a new attribute containing all the properties.
	*/
	PP_PropertyVector getAttributesWithProps();

	bool getNthProperty(int i, const gchar* & szName, const gchar* & szValue);
	size_t getPropertyCount();

	//! Writes a <w:pBdr> element for any paragraph-border props
	//! (top/left/bot/right -style/-thickness/-space/-color).
	UT_Error serializeParagraphBorders(IE_Exp_OpenXML* exporter, int target) const;

	//! Record a w:ins/w:del-class strux mark (emitted as
	//! revision="+id"/"-id" once a PD_Document registers it).
	void addRevisionMark(bool deleted, const gchar * author, const gchar * date);
	//! Record a w:*Change property-change mark; props/attrs hold the
	//! captured pre-change snapshot.
	void addChangeMark(const gchar * name, const gchar * author,
					   const gchar * date, const PP_PropertyVector & props,
					   const PP_PropertyVector & attrs);
	//! Copy pending marks verbatim (row marks fan out onto cells).
	void appendRevisionMarks(const std::vector<OXML_StruxRevision> & marks);
	const std::vector<OXML_StruxRevision> & getRevisionMarks() const;
	//! Register pending marks on the document revision table and fold
	//! them into this object's attributes. Call at addToPT time, before
	//! building the emitted attr/prop vector.
	void applyRevisionMarks(PD_Document * pDocument);

private:
	PP_AttrProp* m_pAttributes;
	std::vector<OXML_StruxRevision> m_revMarks;

	std::string _generatePropsString() const;
};

#endif //_OXML_OBJECTWITHATTRPROP_H_

