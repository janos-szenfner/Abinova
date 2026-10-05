/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ut_types.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ut_string.h"
#include "ie_imp_GraphicAsDocument.h"
#include "pd_Document.h"
#include "ie_impGraphic.h"
#include "fg_Graphic.h"
#include "fg_GraphicRaster.h"
#include "fg_GraphicVector.h"

/*****************************************************************/
/*****************************************************************/

/*
 * The user tried to open an image directly, so we'll create an
 * empty document and put the graphic in that.
 */

/*****************************************************************/
/*****************************************************************/

UT_Error IE_Imp_GraphicAsDocument::_loadFile(GsfInput * input)
{
	UT_Error error;

   	UT_DEBUGMSG(("trying to open an image as a document...\n"));
   
	if (!getDoc()->appendStrux(PTX_Section, PP_NOPROPS) ||
	    !getDoc()->appendStrux(PTX_Block, PP_NOPROPS))
     		return UT_IE_NOMEMORY;

	/* the factory is expected to install a graphic importer before
	 * loading, but don't crash if this importer is ever driven
	 * without one */
	if (!m_pGraphicImporter)
		return UT_IE_BOGUSDOCUMENT;

   	FG_ConstGraphicPtr pFG;
   	error = m_pGraphicImporter->importGraphic(input, pFG);

   	if (error != UT_OK)
     		return error;

	if (!pFG)
		return UT_IE_NOMEMORY;

   	const UT_ConstByteBufPtr & buf = pFG->getBuffer();
	if (!buf)
		return UT_IE_NOMEMORY;

	/* create the data item before the object that references it, so
	 * a failed createDataItem() cannot leave an image object pointing
	 * at a nonexistent "image_0" item */
   	if (!getDoc()->createDataItem("image_0", false,
					buf, pFG->getMimeType(), nullptr)) {
	   return UT_IE_NOMEMORY;
	}

   	const PP_PropertyVector propsArray = {
          "dataid", "image_0"
        };

   	if (!getDoc()->appendObject(PTO_Image, propsArray)) {
	   return UT_IE_NOMEMORY;
	}


	return UT_OK;
}

/*****************************************************************/
/*****************************************************************/

IE_Imp_GraphicAsDocument::~IE_Imp_GraphicAsDocument()
{
   	DELETEP(m_pGraphicImporter);
}

IE_Imp_GraphicAsDocument::IE_Imp_GraphicAsDocument(PD_Document * pDocument)
  : IE_Imp(pDocument), m_pGraphicImporter(nullptr)
{
}
