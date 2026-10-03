/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

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

#ifndef _OXMLI_LISTENERSTATE_IMAGE_H_
#define _OXMLI_LISTENERSTATE_IMAGE_H_

// Internal includes
#include <OXMLi_ListenerState.h>
#include <OXMLi_Types.h>
#include <OXML_Types.h>
#include <OXML_Element_Image.h>

#include <vector>

/* \class OXMLi_ListenerState_Image
 * \brief This ListenerState parses the Images.
*/
class OXMLi_ListenerState_Image : public OXMLi_ListenerState
{
public:
	OXMLi_ListenerState_Image();
	virtual void startElement (OXMLi_StartElementRequest * rqst) override;
	virtual void endElement (OXMLi_EndElementRequest * rqst) override;
	virtual void charData (OXMLi_CharDataRequest * rqst) override;

private:
	bool addImage(const std::string & id);
	std::string m_style;
	bool m_isEmbeddedObject;
	bool m_isInlineImage;
	bool m_bSimplePos;
	int m_grpPicDepth = 0;
	/* w:object bookkeeping: the o:OLEObject part reference is captured
	 * while the body is suppressed and lands on the image element the
	 * embedded v:imagedata preview produced */
	OXML_SharedElement m_pObjImage;
	std::string m_objRelId;
	std::string m_objProgId;
	std::string m_objType;
	/* a:blip effect capture — a:duotone holds two color elements whose
	 * transform children resolve when the color element closes */
	bool m_bInDuotone = false;
	std::vector<std::string> m_duotone;
	std::string m_pendFxColor;
	double m_fxLumMod = 1.0;
	double m_fxLumOff = 0.0;
	double m_fxTint = -1.0;
	double m_fxShade = -1.0;
	double m_fxSatMod = -1.0;
	double m_fxSatOff = 0.0;
	double m_fxHueMod = -1.0;
	double m_fxHueOff = 0.0;
};

#endif //_OXMLI_LISTENERSTATE_IMAGE_H_

