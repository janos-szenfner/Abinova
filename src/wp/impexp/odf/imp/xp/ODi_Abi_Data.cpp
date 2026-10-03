/* AbiSource
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
#include "ODi_Abi_Data.h"

// Abinova includes
#include "pd_Document.h"
#include "pt_Types.h"
#include "ie_impGraphic.h"
#include "fg_GraphicRaster.h"
#include "ie_math_convert.h"

// External includes
#include <glib-object.h>
#include <string.h>

/**
 * Returns true when the stream's XML document element is a MathML
 * <math> element (any namespace prefix: <math>, <math:math>,
 * <mml:math>). ODF stores each embedded object as its own subdocument
 * stream, so the root element identifies the object kind: formulas
 * root at <math...> while charts, spreadsheets, text and presentation
 * objects root at <office:document...> and OLE payloads are binary.
 */
static bool s_streamIsMathML(const UT_ByteBuf& buf)
{
    const UT_uint32 len = buf.getLength();
    if (len == 0) {
        return false;
    }
    const UT_Byte* p = buf.getPointer(0);

    UT_uint32 i = 0;
    // Skip the prolog: whitespace, <?...?>, <!--...--> and
    // <!DOCTYPE ...> declarations so we land on the document element.
    for (UT_uint32 hops = 0; hops < 64; hops++) {
        while (i < len && g_ascii_isspace(static_cast<gchar>(p[i]))) {
            i++;
        }
        if (i + 1 >= len || p[i] != '<') {
            return false; // not XML at all
        }
        if (p[i + 1] == '?') {
            const void* e = memchr(p + i + 2, '>', len - i - 2);
            if (!e) {
                return false;
            }
            i = static_cast<UT_uint32>(static_cast<const UT_Byte*>(e) - p) + 1;
            continue;
        }
        if (p[i + 1] == '!') {
            if (len - i >= 4 && memcmp(p + i, "<!--", 4) == 0) {
                bool closed = false;
                for (i += 4; i + 2 < len; i++) {
                    if (p[i] == '-' && p[i + 1] == '-' && p[i + 2] == '>') {
                        i += 3;
                        closed = true;
                        break;
                    }
                }
                if (!closed) {
                    return false;
                }
            } else {
                // honor a DOCTYPE internal subset so a '>' inside
                // [ ... ] does not end the declaration early
                int brackets = 0;
                bool closed = false;
                for (i += 2; i < len; i++) {
                    if (p[i] == '[') {
                        brackets++;
                    } else if (p[i] == ']') {
                        if (brackets) {
                            brackets--;
                        }
                    } else if (p[i] == '>' && !brackets) {
                        i++;
                        closed = true;
                        break;
                    }
                }
                if (!closed) {
                    return false;
                }
            }
            continue;
        }
        break; // '<' + name char: the document element
    }

    if (i >= len || p[i] != '<') {
        return false;
    }
    i++;
    const UT_uint32 nameStart = i;
    while (i < len &&
           (g_ascii_isalnum(static_cast<gchar>(p[i])) ||
            p[i] == '_' || p[i] == '-' || p[i] == ':' || p[i] == '.')) {
        i++;
    }
    const UT_uint32 nameLen = i - nameStart;
    if (nameLen == 0) {
        return false;
    }

    const std::string name(reinterpret_cast<const char*>(p + nameStart),
                           nameLen);
    const std::string::size_type colon = name.find(':');
    const std::string local =
        (colon == std::string::npos) ? name : name.substr(colon + 1);
    return local == "math";
}

/**
 * Constructor
 */
ODi_Abi_Data::ODi_Abi_Data(PD_Document* pDocument, GsfInfile* pGsfInfile) :
    m_pAbiDocument (pDocument), m_pGsfInfile (pGsfInfile) {
}


/**
 * Adds an data item (<d> tag) in the Abinova document for the specified image.
 * 
 * Code mainly from Dom Lachowicz and/or Robert Staudinger.
 * 
 * @param rDataId Receives the id that has been given to the added data item.
 * @param ppAtts The attributes of a <draw:image> element.
 */
bool ODi_Abi_Data::addImageDataItem(UT_String& rDataId, const gchar** ppAtts) {
    
    const gchar* pHRef = UT_getAttribute ("xlink:href", ppAtts);
    UT_return_val_if_fail(pHRef,false);

    // If we have a string smaller then this we are in trouble. File corrupted?
    UT_return_val_if_fail((strlen(pHRef) >= sizeof("Pictures/a") - 1), false);

    UT_Error error = UT_OK;
    UT_ByteBufPtr img_buf(new UT_ByteBuf);
    GsfInfile* pPictures_dir;
    FG_ConstGraphicPtr pFG;
    UT_ConstByteBufPtr pPictData;
    UT_uint32 imageID;
    
    // The subdirectory that holds the picture. e.g: "ObjectReplacements" or "Pictures"
    UT_String dirName;
    
    // The file name of the picture. e.g.: "Object 1" or "10000201000000D100000108FF0E3707.png" 
    UT_String fileName;
    
    const std::string id = m_href_to_id[pHRef];
    if (!id.empty()) {
        // This image was already added.
        // Use the existing data item id.
        rDataId = id;
        return true;
    }
    
    
    // Get a new, unique, ID.
    imageID = m_pAbiDocument->getUID(UT_UniqueId::Image);
    UT_String_sprintf(rDataId, "%d", imageID);
    
    // Add this id to the list
    UT_DebugOnly<href_id_map_t::iterator> iter = m_href_to_id
		.insert(m_href_to_id.begin(),
			href_id_map_t::value_type(pHRef, 
						  rDataId.c_str()));
    UT_ASSERT(static_cast<href_id_map_t::iterator>(iter )!= m_href_to_id.end());

    _splitDirectoryAndFileName(pHRef, dirName, fileName);

    // flat documents have no package to read embedded files from
    UT_return_val_if_fail(m_pGsfInfile, false);

    pPictures_dir =
        GSF_INFILE(gsf_infile_child_by_name(m_pGsfInfile, dirName.c_str()));

    UT_return_val_if_fail(pPictures_dir, false);

    // Loads img_buf
    error = _loadStream(pPictures_dir, fileName.c_str(), img_buf);
    g_object_unref (G_OBJECT (pPictures_dir));

    if (error != UT_OK) {
        return false;
    }

    return _createImageDataItem(rDataId, img_buf);
}

/**
 * Creates the image data item from raw image bytes.
 * Shared tail of addImageDataItem() and addImageDataItemFromBuffer().
 */
bool ODi_Abi_Data::_createImageDataItem(UT_String& rDataId, const UT_ByteBufPtr& img_buf)
{
    FG_ConstGraphicPtr pFG;

    // Builds pImporter from img_buf
    UT_Error error = IE_ImpGraphic::loadGraphic (img_buf, IEGFT_Unknown, pFG);
    if ((error != UT_OK) || !pFG) {
        // pictData is already freed in ~FG_Graphic
        return false;
    }

    // Builds pPictData from pFG
    // TODO: can we get back a vector graphic?
    UT_ConstByteBufPtr pPictData = pFG->getBuffer();

    if (!pPictData) {
        // i don't think that this could ever happen, but...
        UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
        return false;
    }

    //
    // Create the data item.
    //

    if (!m_pAbiDocument->createDataItem(rDataId.c_str(),
                                        false,
                                        pPictData,
                                        pFG->getMimeType(),
                                        nullptr)) {

        UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
        return false;
    }

    return true;
}

/**
 * Creates the image data item from image bytes already in memory.
 * Used for flat (single-XML) documents whose images arrive as
 * base64 inside <office:binary-data> rather than as package files.
 */
bool ODi_Abi_Data::addImageDataItemFromBuffer(UT_String& rDataId, const UT_ByteBufPtr& img_buf)
{
    UT_return_val_if_fail(img_buf && img_buf->getLength() > 0, false);

    UT_uint32 imageID = m_pAbiDocument->getUID(UT_UniqueId::Image);
    UT_String_sprintf(rDataId, "%d", imageID);

    return _createImageDataItem(rDataId, img_buf);
}

/**
 * Adds an data item (<d> tag) in the Abinova document for the specified image.
 * 
 * Code mainly from Dom Lachowicz and/or Robert Staudinger.
 * 
 * @param rDataId Receives the id that has been given to the added data item.
 * @param ppAtts The attributes of a <draw:image> element.
 */
bool ODi_Abi_Data::addObjectDataItem(UT_String& rDataId, const gchar** ppAtts, int& pto_Type) {

    const gchar* pHRef = UT_getAttribute ("xlink:href", ppAtts);
    UT_return_val_if_fail(pHRef,false);

    // If we have a string smaller then this we are in trouble. File corrupted?
    UT_return_val_if_fail((strlen(pHRef) >= sizeof("Object a/") - 1), false);

    GsfInfile* pObjects_dir;

    // The subdirectory that holds the picture. e.g: "ObjectReplacements" or "Pictures"
    UT_String dirName;

    // The file name of the picture. e.g.: "Object 1" or "10000201000000D100000108FF0E3707.png"
    UT_String fileName;

    const std::string id = m_href_to_id[pHRef];
    if (!id.empty()) {
        // This object was already added.
        // Use the existing data item id.
        rDataId = id;
        return true;
    }

    _splitDirectoryAndFileName(pHRef, dirName, fileName);

    if (fileName.empty ())
      fileName = "content.xml";

    // flat documents have no package to read embedded files from
    UT_return_val_if_fail(m_pGsfInfile, false);

    pObjects_dir =
        GSF_INFILE(gsf_infile_child_by_name(m_pGsfInfile, dirName.c_str()));


    UT_return_val_if_fail(pObjects_dir, false);

    // Loads object_buf
    UT_ByteBufPtr object_buf(new UT_ByteBuf);
    UT_Error error = _loadStream(pObjects_dir, fileName.c_str(), object_buf);
    g_object_unref (G_OBJECT (pObjects_dir));

    if (error != UT_OK) {
        return false;
    }

    // Embedded objects are only claimed as math when the stream is
    // really a MathML subdocument. Charts, spreadsheets, OLE payloads
    // and other object kinds are rejected here so the caller can fall
    // back to the ObjectReplacements preview image instead of creating
    // a corrupt math item.
    if (!s_streamIsMathML(*object_buf)) {
        UT_DEBUGMSG(("ODT import: %s is not a MathML object, using preview\n", pHRef));
        return false;
    }

    // Get a new, unique, ID.
    const UT_uint32 objectID = m_pAbiDocument->getUID(UT_UniqueId::Math);
    UT_String_sprintf(rDataId, "MathLatex%d", objectID);

    std::string rLatexId;
    rLatexId.assign("LatexMath");
    rLatexId.append((rDataId.substr(9,rDataId.length()-8)).c_str());
    // Add this id to the list
    UT_DebugOnly<href_id_map_t::iterator> iter = m_href_to_id
		.insert(m_href_to_id.begin(),
			href_id_map_t::value_type(pHRef,
						  rDataId.c_str()));
    UT_ASSERT(static_cast<href_id_map_t::iterator>(iter )!= m_href_to_id.end());

    //
    // Create the data item.
    //

    UT_ByteBufPtr latexBuf(new UT_ByteBuf);
    UT_UTF8String PbMathml(reinterpret_cast<const char*>(object_buf->getPointer(0)),
                           object_buf->getLength());
    UT_UTF8String PbLatex,Pbitex;

    if (!m_pAbiDocument->createDataItem(rDataId.c_str(), false, object_buf,"application/mathml+xml", nullptr))
    {
        UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
        return false;
    }

    if(convertMathMLtoLaTeX(PbMathml, PbLatex) && convertLaTeXtoEqn(PbLatex,Pbitex))
    {

	// Conversion of MathML to LaTeX and the Equation Form suceeds
	latexBuf->ins(0, reinterpret_cast<const UT_Byte *>(Pbitex.utf8_str()), static_cast<UT_uint32>(Pbitex.size()));
	if(!m_pAbiDocument->createDataItem(rLatexId.c_str(), false, latexBuf, "", nullptr))
	{
	    UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
	    return false;
	}
    }

    pto_Type = PTO_Math;
    return true;
}


/**
 * Code from Dom Lachowicz and/or Robert Staudinger.
 */
UT_Error ODi_Abi_Data::_loadStream (GsfInfile* oo,
                                   const char* stream,
                                   const UT_ByteBufPtr & buf)
{
    guint8 const *data = nullptr;
    size_t len = 0;
    static const size_t BUF_SZ = 4096;
  
    buf->truncate(0);
    GsfInput * input = gsf_infile_child_by_name(oo, stream);

    if (!input){
    	return UT_ERROR;
    }

    // reject members whose declared size is absurd — the zip directory
    // is attacker-controlled
    if (gsf_input_size (input) > UT_MAX_ARCHIVE_MEMBER_SIZE) {
        g_object_unref (G_OBJECT (input));
        return UT_ERROR;
    }

    if (gsf_input_size (input) > 0) {
        while ((len = gsf_input_remaining (input)) > 0) {
            len = UT_MIN (len, BUF_SZ);
            if (nullptr == (data = gsf_input_read (input, len, nullptr))) {
                g_object_unref (G_OBJECT (input));
                return UT_ERROR;
            }
            buf->append(static_cast<const UT_Byte *>(data), len);
        }
    }
  
    g_object_unref (G_OBJECT (input));
    return UT_OK;
}

/**
 * Takes a string like "./ObjectReplacements/Object 1" and split it into
 * subdirectory name ("ObjectReplacements") and file name ("Object 1").
 */
void ODi_Abi_Data::_splitDirectoryAndFileName(const gchar* pHRef, UT_String& dirName, UT_String& fileName) const {
    UT_String href = pHRef;

    int iStart;
    // Get the directory name
    UT_String str = href.substr(0, 2);
    if (str == "./") {
        iStart = 2;
    } else {
        iStart = 0;
    }

    int nChars = 0;
    int len = href.length();
    for (int i = iStart; i < len; i++) {
        if (href[i] == '/') {
            break;
        } else {
            nChars++;
        }
    }

    dirName = href.substr(iStart, nChars);

    if (nChars == len - 1)
    {
        fileName = "";
    }
    else
    {
        UT_ASSERT (nChars > 0 && nChars < len);
        // Get the file name
        iStart = iStart + nChars + 1;
        nChars = len - iStart;
        UT_ASSERT (nChars); // The file name must have at least one char.
        fileName = href.substr(iStart, nChars);
    }
}
