/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */

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

#include <string>
#include <vector>
#include "ie_impGraphic.h"

#include "ut_assert.h"
#include "ut_string.h"
#include "ut_misc.h"
#include "ut_bytebuf.h"
#include "ut_vector.h"

#include "ut_go_file.h"

#include "fg_Graphic.h"
#include "fg_GraphicRaster.h"
#include "fg_GraphicVector.h"

/*****************************************************************/
/*****************************************************************/

static std::vector<IE_ImpGraphicSniffer*> 	IE_IMP_GraphicSniffers;
static std::vector<std::string> 		IE_IMP_GraphicMimeTypes;
static std::vector<std::string> 		IE_IMP_GraphicMimeClasses;
static std::vector<std::string> 		IE_IMP_GraphicSuffixes;

void IE_ImpGraphic::registerImporter (IE_ImpGraphicSniffer * s)
{
	UT_sint32 ndx = IE_IMP_GraphicSniffers.size();
	IE_IMP_GraphicSniffers.push_back (s);

	s->setType(ndx+1);
}

void IE_ImpGraphic::unregisterImporter (IE_ImpGraphicSniffer * s)
{
	UT_uint32 ndx = s->getType(); // 1:1 mapping

	IE_IMP_GraphicSniffers.erase(IE_IMP_GraphicSniffers.begin() + (ndx-1));

	// Refactor the indexes
	IE_ImpGraphicSniffer * pSniffer = nullptr;
	UT_uint32 size  = IE_IMP_GraphicSniffers.size();
	UT_uint32 i     = 0;
	for( i = ndx-1; i < size; i++)
	{
		pSniffer = IE_IMP_GraphicSniffers[i];
		if (pSniffer)
        	pSniffer->setType(i+1);
	}
	// Delete the supported types lists
	IE_IMP_GraphicMimeTypes.clear();
	IE_IMP_GraphicMimeClasses.clear();
	IE_IMP_GraphicSuffixes.clear();
}

void IE_ImpGraphic::unregisterAllImporters ()
{
	IE_ImpGraphicSniffer * pSniffer = nullptr;
	UT_uint32 size = IE_IMP_GraphicSniffers.size();

	for (UT_uint32 i = 0; i < size; i++)
	{
		pSniffer = IE_IMP_GraphicSniffers[i];
		DELETEP(pSniffer);
	}

	IE_IMP_GraphicSniffers.clear();
}

/*!
 * Get supported mimetypes by builtin- and plugin-filters.
 */
const std::vector<std::string> & IE_ImpGraphic::getSupportedMimeTypes()
{
	if (IE_IMP_GraphicMimeTypes.size() > 0) {
		return IE_IMP_GraphicMimeTypes;
	}

	const IE_MimeConfidence *mc;
	for (UT_sint32 i = 0; i < IE_IMP_GraphicSniffers.size(); i++) {
		auto sniffer = IE_IMP_GraphicSniffers[i];
		UT_nonnull_or_continue(sniffer);
		mc = sniffer->getMimeConfidence();
		while (mc && mc->match) {
			if (mc->match == IE_MIME_MATCH_FULL) {
				IE_IMP_GraphicMimeTypes.push_back(mc->mimetype);
			}
			mc++;
		}
	}

	/* TODO rob: unique */
	return IE_IMP_GraphicMimeTypes;
}

/*!
 * Get supported mime classes by builtin- and plugin-filters.
 */
const std::vector<std::string> & IE_ImpGraphic::getSupportedMimeClasses()
{
	if (IE_IMP_GraphicMimeClasses.size() > 0) {
		return IE_IMP_GraphicMimeClasses;
	}

	const IE_MimeConfidence *mc;
	for (UT_sint32 i = 0; i < IE_IMP_GraphicSniffers.size(); i++) {
		auto sniffer = IE_IMP_GraphicSniffers[i];
		UT_nonnull_or_continue(sniffer);
		mc = sniffer->getMimeConfidence();
		while (mc && mc->match) {
			if (mc->match == IE_MIME_MATCH_CLASS) {
				IE_IMP_GraphicMimeClasses.push_back(mc->mimetype);
			}
			mc++;
		}
	}

	/* TODO rob: unique */
	return IE_IMP_GraphicMimeClasses;
}

/*!
 * Get supported suffixes by builtin- and plugin-filters.
 */
const std::vector<std::string> & IE_ImpGraphic::getSupportedSuffixes()
{
	if (IE_IMP_GraphicSuffixes.size() > 0) {
		return IE_IMP_GraphicSuffixes;
	}

	const IE_SuffixConfidence *sc;
	for (UT_sint32 i = 0; i < IE_IMP_GraphicSniffers.size(); i++) {
		auto sniffer = IE_IMP_GraphicSniffers[i];
		UT_nonnull_or_continue(sniffer);
		sc = sniffer->getSuffixConfidence();
		while (sc && !sc->suffix.empty()) {
			IE_IMP_GraphicSuffixes.push_back(sc->suffix);
			sc++;
		}
	}
	
	/* TODO rob: unique */
	return IE_IMP_GraphicSuffixes;
}

/*!
 * Map a suffix to the sniffer's mime type for it.  Sniffers whose
 * suffix table does not pair suffixes with mime types inherit the
 * first-entry default.
 */
const char * IE_ImpGraphicSniffer::mimeTypeForSuffix(const char * /*suffix*/)
{
	const IE_MimeConfidence *mc = getMimeConfidence();
	return mc ? mc->mimetype.c_str() : nullptr;
}

/*!
 * Map mime type to a suffix. Returns nullptr if not found.
 */
const char * IE_ImpGraphic::getMimeTypeForSuffix(const char * suffix)
{
	if (!suffix || !(*suffix))
		return nullptr;
		
	if (suffix[0] == '.') {
		suffix++;
	}

	const IE_SuffixConfidence *sc;
	for (UT_sint32 i = 0; i < IE_IMP_GraphicSniffers.size(); i++) {
		IE_ImpGraphicSniffer *sniffer = IE_IMP_GraphicSniffers[i];
		UT_nonnull_or_continue(sniffer);
		sc = sniffer->getSuffixConfidence();
		while (sc && !sc->suffix.empty()) {
			if (0 == g_ascii_strcasecmp(suffix, sc->suffix.c_str())) {
				return sniffer->mimeTypeForSuffix(suffix);
			}
			sc++;
		}
	}

	return nullptr;
}

/*****************************************************************/
/*****************************************************************/

IEGraphicFileType IE_ImpGraphic::fileTypeForMimetype(const char * szMimetype)
{
	if (!szMimetype || !strlen(szMimetype))
		return IEGFT_Unknown;
	
	// we have to construct the loop this way because a
	// given filter could support more than one file type,
	// so we must query a mimetype match for all file types
	UT_uint32 nrElements = getImporterCount();

	IEGraphicFileType best = IEGFT_Unknown;
	UT_Confidence_t   best_confidence = UT_CONFIDENCE_ZILCH;

	for (UT_uint32 k=0; k < nrElements; k++)
	{
		IE_ImpGraphicSniffer * s = IE_IMP_GraphicSniffers[k];
		UT_nonnull_or_continue(s);

		const IE_MimeConfidence * mc = s->getMimeConfidence();
		UT_Confidence_t confidence = UT_CONFIDENCE_ZILCH;
		while (mc && mc->match) {
			if (mc->match == IE_MIME_MATCH_FULL) {
				if (0 == g_ascii_strcasecmp(mc->mimetype.c_str(), szMimetype) && 
					mc->confidence > confidence) {
					confidence = mc->confidence;
				}
			}
			mc++;
		}

		if ((confidence > 0) && ((IEGFT_Unknown == best) || (confidence >= best_confidence)))
		{
			best_confidence = confidence;
			for (UT_sint32 a = 0; a < static_cast<int>(nrElements); a++)
			{
				if (s->supportsType(static_cast<IEGraphicFileType>(a+1)))
				  {
				    best = static_cast<IEGraphicFileType>(a+1);
				    
				    // short-circuit if we're 100% sure
				    if ( UT_CONFIDENCE_PERFECT == best_confidence )
				      return best;
				    break;
				  }
			}
		}
	}

	return best;	
}

IEGraphicFileType IE_ImpGraphic::fileTypeForSuffix(const char * szSuffix)
{
	if (!szSuffix || !strlen(szSuffix))
		return IEGFT_Unknown;
	
	// we have to construct the loop this way because a
	// given filter could support more than one file type,
	// so we must query a suffix match for all file types
	UT_uint32 nrElements = getImporterCount();

	IEGraphicFileType best = IEGFT_Unknown;
	UT_Confidence_t   best_confidence = UT_CONFIDENCE_ZILCH;

	for (UT_uint32 k=0; k < nrElements; k++)
	{
		IE_ImpGraphicSniffer * s = IE_IMP_GraphicSniffers[k];
		UT_nonnull_or_continue(s);

		const IE_SuffixConfidence * sc = s->getSuffixConfidence();
		UT_Confidence_t confidence = UT_CONFIDENCE_ZILCH;
		while (sc && !sc->suffix.empty()) {
			/* suffixes do not have a leading '.' */
			if (0 == g_ascii_strcasecmp(sc->suffix.c_str(), szSuffix+1) && 
				sc->confidence > confidence) {
				confidence = sc->confidence;
			}
			sc++;
		}

		if ((confidence > 0) && ((IEGFT_Unknown == best) || (confidence >= best_confidence)))
		{
		        best_confidence = confidence;
			for (UT_sint32 a = 0; a < static_cast<int>(nrElements); a++)
			{
				if (s->supportsType(static_cast<IEGraphicFileType>(a+1)))
				  {
				    best = static_cast<IEGraphicFileType>(a+1);
				    
				    // short-circuit if we're 100% sure
				    if ( UT_CONFIDENCE_PERFECT == best_confidence )
				      return best;
				    break;
				  }
			}
		}
	}

	return best;	
}
	
IEGraphicFileType IE_ImpGraphic::fileTypeForContents(const char * szBuf, UT_uint32 iNumbytes)
{
	GsfInput * input = gsf_input_memory_new (const_cast<guint8 *>(reinterpret_cast<const guint8*>(szBuf)), static_cast<gsf_off_t>(iNumbytes), FALSE);
	if (!input)
		return IEGFT_Unknown;

	// we have to construct the loop this way because a
	// given filter could support more than one file type,
	// so we must query a match for all file types
	UT_uint32 nrElements = getImporterCount();

	IEGraphicFileType best = IEGFT_Unknown;
	UT_Confidence_t   best_confidence = UT_CONFIDENCE_ZILCH;

	for (UT_uint32 k=0; k < nrElements; k++)
	{
		IE_ImpGraphicSniffer * s = IE_IMP_GraphicSniffers[k];
		UT_nonnull_or_continue(s);
		UT_Confidence_t confidence = s->recognizeContents(input);
		if ((confidence > 0) && ((IEGFT_Unknown == best) || (confidence >= best_confidence)))
		{
			best_confidence = confidence;
			for (UT_sint32 a = 0; a < static_cast<int>(nrElements); a++)
			{
				if (s->supportsType(static_cast<IEGraphicFileType>( (a+1))))
				  {
				    best = static_cast<IEGraphicFileType>(a+1);
				    
				    // short-circuit if we're 100% sure
					if ( UT_CONFIDENCE_PERFECT == best_confidence ) {
						g_object_unref (G_OBJECT (input));
						return best;
					}
				    break;
				  }
			}
		}
	}

	g_object_unref (G_OBJECT (input));

	return best;
}
	
bool IE_ImpGraphic::enumerateDlgLabels(UT_uint32 ndx,
					  const char ** pszDesc,
					  const char ** pszSuffixList,
					  IEGraphicFileType * ft)
{
	UT_uint32 nrElements = getImporterCount();
	if (ndx < nrElements)
	{
		IE_ImpGraphicSniffer * s = IE_IMP_GraphicSniffers[ndx];
		UT_nonnull_or_return(s, false);
		return s->getDlgLabels(pszDesc,pszSuffixList,ft);
	}

	return false;
}

UT_uint32 IE_ImpGraphic::getImporterCount(void)
{
	return IE_IMP_GraphicSniffers.size ();
}


UT_Error IE_ImpGraphic::constructImporter(const UT_ConstByteBufPtr & bytes,
					   IEGraphicFileType ft,
					   IE_ImpGraphic **ppieg)
{
	// construct an importer of the right type.
	// caller is responsible for deleting the importer object
	// when finished with it.
	UT_return_val_if_fail(ppieg, UT_ERROR);

	// no filter will support IEGFT_Unknown, so we detect from the
	// suffix of the filename and the contents of the file, the real 
        // importer to use and assign that back to ieft.
	if (ft == IEGFT_Unknown)
	{
	  ft = IE_ImpGraphic::fileTypeForContents(reinterpret_cast<const char *>(bytes->getPointer(0)),
						   bytes->getLength());
	}

	// use the importer for the specified file type
	for (UT_sint32 k=0; (k < IE_IMP_GraphicSniffers.size()); k++)
	{
		IE_ImpGraphicSniffer * s = IE_IMP_GraphicSniffers[k];
		UT_nonnull_or_continue(s);
		if (s->supportsType(ft))
			return s->constructImporter(ppieg);
	}

	// if we got here, no registered importer handles the
	// type of file we're supposed to be reading.
	return UT_IE_UNKNOWNTYPE;
}

static UT_Confidence_t s_condfidence_heuristic ( UT_Confidence_t content_confidence, 
						 UT_Confidence_t suffix_confidence )
{
  return static_cast<UT_Confidence_t>( ( (static_cast<double>(content_confidence) * 0.85) + (static_cast<double>(suffix_confidence) * 0.15) ) );
}

UT_Error IE_ImpGraphic::constructImporter(const char * szFilename,
										  IEGraphicFileType ft,
										  IE_ImpGraphic **ppieg)
{
	GsfInput * input;

	input = UT_go_file_open (szFilename, nullptr);
	if (!input)
		return UT_IE_FILENOTFOUND;

	UT_Error result = constructImporter (input, ft, ppieg);

	g_object_unref (G_OBJECT (input));

	return result;
}

#define CONFIDENCE_THRESHOLD 72

UT_Error IE_ImpGraphic::constructImporter(GsfInput * input,
										  IEGraphicFileType ft,
										  IE_ImpGraphic **ppieg)
{
  // construct an importer of the right type.
  // caller is responsible for deleting the importer object
  // when finished with it.
  UT_return_val_if_fail(ppieg, UT_ERROR);
  
  UT_uint32 nrElements = IE_IMP_GraphicSniffers.size();
  
  // no filter will support IEGFT_Unknown, so we detect from the
  // suffix of the filename and the contents of the file, the real 
  // importer to use and assign that back to ft.
  if (ft == IEGFT_Unknown)
    { 
		UT_return_val_if_fail (input != nullptr, UT_IE_FILENOTFOUND);

		UT_Confidence_t   best_confidence = UT_CONFIDENCE_ZILCH;
		
		for (UT_uint32 k=0; k < nrElements; k++)
			{
				IE_ImpGraphicSniffer * s = IE_IMP_GraphicSniffers[k];
				UT_nonnull_or_continue(s);

				UT_Confidence_t content_confidence = UT_CONFIDENCE_ZILCH;
				UT_Confidence_t suffix_confidence = UT_CONFIDENCE_ZILCH;
				
				{
					GsfInputMarker marker(input);
					content_confidence = s->recognizeContents(input);
				}

				const char * name = gsf_input_name (input);
				// we can have an empty name (nullptr) because we can have a memory stream.
				if(name) {
					const IE_SuffixConfidence * sc = s->getSuffixConfidence();
					while (sc && !sc->suffix.empty() && suffix_confidence != UT_CONFIDENCE_PERFECT) {
						/* suffixes do not have a leading '.' */
						// we use g_str_has_suffix like this to make sure we properly autodetect the extensions
						// of files that have dots in their names, like foo.bar.png
						std::string suffix = std::string(".") + sc->suffix;
						if (g_str_has_suffix(name, suffix.c_str()) && 
							sc->confidence > suffix_confidence) {
							suffix_confidence = sc->confidence;
						}
						sc++;
					}
				}
				UT_Confidence_t confidence = s_condfidence_heuristic ( content_confidence, 
																	   suffix_confidence ) ;
				
				if ( confidence > CONFIDENCE_THRESHOLD && confidence >= best_confidence )
					{
						best_confidence = confidence;
						ft = static_cast<IEGraphicFileType>((k+1));
					}
			}
    }
  
  // use the importer for the specified file type
  for (UT_uint32 k=0; (k < nrElements); k++)
	  {
      IE_ImpGraphicSniffer * s = IE_IMP_GraphicSniffers[k];
      UT_nonnull_or_continue(s);
      if (s->supportsType(ft))
		  return s->constructImporter(ppieg);
	  }
  
  // if we got here, no registered importer handles the
  // type of file we're supposed to be reading.
  return UT_IE_UNKNOWNTYPE;
}


//  Load the contents of the file into a ByteBuffer, and pass it to
//  the other importGraphic function.  Used as a convenience for importing
//  graphics from a file on disk.

UT_Error IE_ImpGraphic::importGraphic(const UT_ConstByteBufPtr & byteBuf,
									  FG_ConstGraphicPtr& pfg)
{
	UT_return_val_if_fail (byteBuf != nullptr, UT_IE_FILENOTFOUND);

	GsfInput * input = gsf_input_memory_new_clone (byteBuf->getPointer(0), byteBuf->getLength());

	if (!input)
		return UT_IE_NOMEMORY;

	UT_Error result = importGraphic(input, pfg);

	g_object_unref (G_OBJECT (input));

	return result;
}

UT_Error IE_ImpGraphic::importGraphic(GsfInput * input,
									  FG_ConstGraphicPtr& pfg)
{
	UT_return_val_if_fail (input != nullptr, UT_IE_FILENOTFOUND);

	UT_ByteBufPtr pBB(new UT_ByteBuf);

	if (pBB == nullptr)
		return UT_IE_NOMEMORY;

	if (!pBB->insertFromInput(0, input))
		{
			return UT_IE_FILENOTFOUND;
		}

	//  The ownership of pBB changes here.  The subclass of IE_ImpGraphic
	//  should either delete pBB when it is done importing, or give it
	//  to the FG_Graphic object which is eventually constructed.
	return importGraphic(pBB, pfg);
}

UT_Error IE_ImpGraphic::importGraphic(const char * szFilename,
									  FG_ConstGraphicPtr& pfg)
{
	GsfInput * input;

	input = UT_go_file_open (szFilename, nullptr);
	if (!input)
		return UT_IE_FILENOTFOUND;

	UT_Error res = importGraphic(input, pfg);

	g_object_unref (G_OBJECT (input));
	return res;
}


UT_Error IE_ImpGraphic::loadGraphic(const char * szFilename,
									IEGraphicFileType iegft,
									FG_ConstGraphicPtr& pfg)
{
	GsfInput *input;
	
	input = UT_go_file_open (szFilename, nullptr);
	if (!input)
		return UT_IE_FILENOTFOUND;

	UT_Error result = loadGraphic (input, iegft, pfg);

	g_object_unref (G_OBJECT (input));

	return result;
}

UT_Error IE_ImpGraphic::loadGraphic(GsfInput * input,
									IEGraphicFileType iegft,
									FG_ConstGraphicPtr& pfg)
{
	UT_return_val_if_fail (input != nullptr, UT_IE_FILENOTFOUND);

	IE_ImpGraphic *importer;
	
	UT_Error result = constructImporter(input, iegft, &importer);
	if (result != UT_OK || !importer)
		return UT_ERROR;

	result = importer->importGraphic (input, pfg);

	delete importer;

	return result;
}

UT_Error IE_ImpGraphic::loadGraphic(const UT_ConstByteBufPtr &pBB,
									IEGraphicFileType iegft,
									FG_ConstGraphicPtr& pfg)
{
	GsfInput * input;

	input = gsf_input_memory_new(pBB->getPointer (0), pBB->getLength(), FALSE);
	if (!input)
		return UT_IE_NOMEMORY;

	UT_Error result = loadGraphic (input, iegft, pfg);

	g_object_unref (G_OBJECT (input));

	return result;
}

UT_Confidence_t IE_ImpGraphicSniffer::recognizeContents (GsfInput * input)
{
	char szBuf[4097] = "";  // 4096+nul ought to be enough
	// gsf_input_size is a signed gsf_off_t and can be -1 on error;
	// clamping that through UT_MIN into UT_uint32 would wrap huge and
	// overflow szBuf.
	const gsf_off_t inputSize = gsf_input_size(input);
	UT_uint32 iNumbytes = (inputSize > 0)
		? static_cast<UT_uint32>(UT_MIN(inputSize, static_cast<gsf_off_t>(4096))) : 0;
	gsf_input_read(input, iNumbytes, reinterpret_cast<guint8 *>((szBuf)));
	szBuf[iNumbytes] = '\0';

	return recognizeContents(szBuf, iNumbytes);
}

UT_Confidence_t IE_ImpGraphicSniffer::recognizeContents (const char * /*szBuf*/, 
														 UT_uint32 /*iNumbytes*/)
{
	// should be explicitly overriden, or not return anything
	UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);

	return UT_CONFIDENCE_ZILCH;
}
