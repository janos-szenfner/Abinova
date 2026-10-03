/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource
 * 
 * Copyright (C) 2008 Firat Kiyak <firatkiyak@gmail.com>
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

// Class definition include
#include "OXML_Element_Image.h"

// Abinova includes
#include "ut_std_string.h"
#include "ut_types.h"
#include "ut_string.h"
#include "ut_path.h"
#include "pd_Document.h"
#include "pp_AttrProp.h"
#include "OXMLi_PackageManager.h"

#include <gsf/gsf.h>

OXML_Element_Image::OXML_Element_Image(const std::string & id) : 
	OXML_Element(id, IMG_TAG, IMAGE)
{
}

OXML_Element_Image::~OXML_Element_Image()
{

}

UT_Error OXML_Element_Image::serialize(IE_Exp_OpenXML* exporter)
{
	UT_Error err = UT_OK;
	const gchar* szValue;
	const gchar* height = "1.0in";
	const gchar* width = "1.0in";
	const gchar* xpos = "0.0in";
	const gchar* ypos = "0.0in";
	const gchar* wrapMode = nullptr;
	bool bPositionedImage = false;

	bPositionedImage = (getAttribute("strux-image-dataid", szValue) == UT_OK);

	if (!bPositionedImage)
	{
		getAttribute("dataid", szValue);
	}

	/* szValue is a document-controlled data-item id; sanitize so the
	 * filename is a flat, quote-free zip member name that matches the
	 * sanitized name OXML_Image::serialize writes into word/media/ */
	std::string filename = UT_sanitizeFileName(szValue);

	std::string extension;
	if(!exporter->getDoc()->getDataItemFileExtension(szValue, extension))
		extension = ".png";
	filename += extension;

	std::string relId("rId");
	relId += getId();

	err = exporter->setImageRelation(filename.c_str(), relId.c_str());
	if(err != UT_OK)
		return err;

	/* media objects: the payload data item becomes a word/media part
	 * plus a:videoFile/a:audioFile (and p14:media for Word's playback
	 * path) inside the picture's pic:nvPr — the poster stays the
	 * a:blip so every reader still renders the frame */
	std::string mediaNvPr;
	const gchar * szMediaDataID = nullptr;
	if (getProperty("media-dataid", szMediaDataID) == UT_OK &&
		szMediaDataID && *szMediaDataID)
	{
		const gchar * szKind = nullptr;
		getProperty("media-kind", szKind);
		bool bVideo = szKind && !strcmp(szKind, "video");
		bool bAudio = szKind && !strcmp(szKind, "audio");
		if (bVideo || bAudio)
		{
			UT_ConstByteBufPtr pMedia;
			if (exporter->getDoc()->getDataItemDataByName(
					szMediaDataID, pMedia, nullptr, nullptr) && pMedia)
			{
				std::string mediaFile = UT_sanitizeFileName(szMediaDataID);
				std::string mExt;
				if (!exporter->getDoc()->getDataItemFileExtension(
						szMediaDataID, mExt))
				{
					mExt = ".bin";
					const gchar * szMName = nullptr;
					if (getProperty("media-name", szMName) == UT_OK && szMName)
					{
						const char * dot = strrchr(szMName, '.');
						if (dot && dot[1])
							mExt = dot;
					}
				}
				mediaFile += mExt;
				if (exporter->writeImage(mediaFile.c_str(), pMedia) == UT_OK)
				{
					std::string mediaRelId = relId;
					mediaRelId += "m";
					std::string p14RelId = relId;
					p14RelId += "p";
					exporter->setMediaRelation(mediaFile.c_str(),
						mediaRelId.c_str(),
						"http://schemas.openxmlformats.org/officeDocument/2006/relationships/media");
					exporter->setMediaRelation(mediaFile.c_str(),
						p14RelId.c_str(),
						"http://schemas.microsoft.com/office/2007/relationships/media");
					mediaNvPr = "<pic:nvPr><a:";
					mediaNvPr += bVideo ? "videoFile" : "audioFile";
					mediaNvPr += " r:link=\"" + mediaRelId +
						"\"/><p14:media xmlns:p14=\"http://schemas.microsoft.com/office/powerpoint/2010/main\" r:embed=\"" +
						p14RelId + "\"/></pic:nvPr>";
				}
			}
		}
	}
	const char * szNvPr = mediaNvPr.empty() ? nullptr : mediaNvPr.c_str();

	if(bPositionedImage)
	{
		// positioned image
		getProperty("wrap-mode", wrapMode);
		getProperty("frame-height", height);
		getProperty("frame-width", width);
		getProperty("xpos", xpos);
		getProperty("ypos", ypos);
		err = exporter->setPositionedImage(getId().c_str(), relId.c_str(), filename.c_str(), width, height, xpos, ypos, wrapMode, szNvPr);
		if(err != UT_OK)
			return err;
	}
	else
	{
		// inline image
		getProperty("height", height);
		getProperty("width", width);

		/* embeds may carry no explicit size — the run falls back to
		 * the manager's natural size, so do the same here by reading
		 * the poster PNG's IHDR (96 device px per inch) */
		std::string wFromSnap, hFromSnap;
		if ((!width || UT_convertToInches(width) <= 0) &&
			szValue && *szValue)
		{
			UT_ConstByteBufPtr pSnap;
			if (exporter->getDoc()->getDataItemDataByName(
					szValue, pSnap, nullptr, nullptr) &&
				pSnap && pSnap->getLength() >= 24)
			{
				const UT_Byte * d = pSnap->getPointer(0);
				if (d && !memcmp(d, "\x89PNG\r\n\x1a\n", 8))
				{
					const UT_Byte * ihdr = d + 16;
					UT_uint32 pw = (ihdr[0] << 24) | (ihdr[1] << 16) |
						(ihdr[2] << 8) | ihdr[3];
					UT_uint32 ph = (ihdr[4] << 24) | (ihdr[5] << 16) |
						(ihdr[6] << 8) | ihdr[7];
					if (pw && ph)
					{
						wFromSnap = UT_convertToDimensionlessString(pw / 96.0, ".4");
						wFromSnap += "in";
						hFromSnap = UT_convertToDimensionlessString(ph / 96.0, ".4");
						hFromSnap += "in";
					}
				}
			}
		}
		if (!width || UT_convertToInches(width) <= 0)
			width = wFromSnap.empty() ? "1.0in" : wFromSnap.c_str();
		if (!height || UT_convertToInches(height) <= 0)
			height = hFromSnap.empty() ? "1.0in" : hFromSnap.c_str();
		err = exporter->setImage(getId().c_str(), relId.c_str(), filename.c_str(), width, height, szNvPr);
		if(err != UT_OK)
			return err;
	}
	return UT_OK;
}

UT_Error OXML_Element_Image::addToPT(PD_Document * pDocument)
{
	OXML_Document* doc = OXML_Document::getInstance();
	if(!doc)
	{
		/* even without a document context, still flush children:
		 * w:drawing wraps shapes/textboxes too, and a bare
		 * OXML_Element_Image pushed for them can own a TextBox
		 * child that must not be dropped */
		return addChildrenToPT(pDocument);
	}
	OXML_SharedImage sImage = doc->getImageById(getId());

	/* an a:graphicData payload with no importer support (chart,
	 * SmartArt diagram, OLE object, DrawingML table, ...) produces
	 * no image — the listener tagged it altcontent-kind so a
	 * placeholder marker can stand in instead of dropping it */
	const gchar * szKind = nullptr;
	const bool bPlaceholder =
		(getProperty("altcontent-kind", szKind) == UT_OK && szKind
		 && !sImage);

	if(!sImage && !bPlaceholder)
	{
		UT_DEBUGMSG(("SERHAT: Skipping image element in import, since fail occurred in import of image data previously\n"));
		return addChildrenToPT(pDocument);
	}

	UT_Error ret = UT_OK;
	bool bInline = false;
	const gchar* szValue = nullptr;

	ret = getProperty("height", szValue);
	if(ret == UT_OK && szValue)
	{
		bInline = true;
	}

	if(!bInline)
	{
		ret = setProperty("frame-type", bPlaceholder ? "textbox" : "image");
		if(ret != UT_OK)
			return ret;

		/* wp14 percent metrics and wp:align anchoring resolve to
		 * offsets now that page size and extent are known */
		resolveAnchorMetrics();
		resolveAnchorAlignment();

		/* the listener records wp:anchor offsets as xpos/ypos which are
		 * not frame props — translate them to page anchoring */
		const gchar * szPos = nullptr;
		if (getProperty("xpos", szPos) == UT_OK && szPos)
		{
			setProperty("frame-page-xpos", szPos);
			setProperty("position-to", "page-above-text");
		}
		if (getProperty("ypos", szPos) == UT_OK && szPos)
			setProperty("frame-page-ypos", szPos);

		if (bPlaceholder)
		{
			/* a frame needs an anchor even when the drawing carried
			 * no usable position */
			if (getProperty("frame-page-xpos", szPos) != UT_OK || !szPos)
				setProperty("position-to", "column-above-text");
		}
		else
		{
			/* Word pictures don't carry our default frame outline */
			const gchar * szHas = nullptr;
			if (getProperty("top-style", szHas) != UT_OK || !szHas)
			{
				setProperty("top-style", "none");
				setProperty("bot-style", "none");
				setProperty("left-style", "none");
				setProperty("right-style", "none");
			}
			if (getProperty("bg-style", szHas) != UT_OK || !szHas)
				setProperty("bg-style", "0");
		}
	}

	/* no image data but an unsupported payload — emit a visible
	 * "[kind]" marker where the object should be */
	if (bPlaceholder)
	{
		std::string mark("[");
		mark += szKind;
		mark += "]";
		UT_UCS4String ucs(mark.c_str());
		if (bInline)
		{
			/* keep the object's rel metadata on the marker span so
			 * a round-trip still knows what part was dropped */
			if (!pDocument->appendFmt(getAttributesWithProps()))
				return UT_ERROR;
			if (!pDocument->appendSpan(ucs.ucs4_str(), ucs.length()))
				return UT_ERROR;
			if (!pDocument->appendFmt(PP_NOPROPS))
				return UT_ERROR;
			return addChildrenToPT(pDocument);
		}
		/* anchored: a bordered frame keeps the drawing's real
		 * position and size around the marker */
		const PP_PropertyVector patts = getAttributesWithProps();
		if (!pDocument->appendStrux(PTX_SectionFrame, patts))
			return UT_ERROR;
		if (!pDocument->appendStrux(PTX_Block, PP_NOPROPS))
			return UT_ERROR;
		if (!pDocument->appendSpan(ucs.ucs4_str(), ucs.length()))
			return UT_ERROR;
		ret = addChildrenToPT(pDocument);
		if (ret != UT_OK)
			return ret;
		return pDocument->appendStrux(PTX_EndFrame, PP_NOPROPS)
			? UT_OK : UT_ERROR;
	}

	if(getId().empty())
	{
		return addChildrenToPT(pDocument);
	}

	/* a:videoFile/a:audioFile/p14:media inside pic:nvPr made this
	 * picture a media object — rebuild it as a playable embed with the
	 * blip as its poster instead of a plain inline image */
	{
		const gchar * szMediaRid = nullptr;
		if (sImage &&
			getProperty("media-rid", szMediaRid) == UT_OK && szMediaRid &&
			*szMediaRid &&
			_addMediaEmbedToPT(pDocument, sImage, szMediaRid) == UT_OK)
			return UT_OK;
	}

	if(bInline)
	{
		ret = setAttribute("dataid", getId().c_str());
		if(ret != UT_OK)
			return ret;
	}
	else
	{
		ret = setAttribute("strux-image-dataid", getId().c_str());
		if(ret != UT_OK)
			return ret;
	}

	const PP_PropertyVector atts = getAttributesWithProps();

	if(bInline)
	{
		if(!pDocument->appendObject(PTO_Image, atts))
			return UT_ERROR;
	}
	else
	{
		ret = pDocument->appendStrux(PTX_SectionFrame, atts) ? UT_OK : UT_ERROR;
		if(ret != UT_OK)
			return ret;

		ret = this->addChildrenToPT(pDocument);
		if(ret != UT_OK)
			return ret;

		ret = pDocument->appendStrux(PTX_EndFrame, PP_NOPROPS) ? UT_OK : UT_ERROR;
		if(ret != UT_OK)
			return ret;
	}
	return UT_OK;
}

/* mime guess for a word/media part name — [Content_Types].xml isn't
 * consulted, the extension is authoritative enough for playback */
static std::string s_mimeForMediaPart(const std::string & partPath)
{
	static const struct { const char * ext; const char * mime; } map[] = {
		{".mp4", "video/mp4"}, {".m4v", "video/mp4"},
		{".mov", "video/quicktime"}, {".avi", "video/x-msvideo"},
		{".mkv", "video/x-matroska"}, {".webm", "video/webm"},
		{".wmv", "video/x-ms-wmv"}, {".ogv", "video/ogg"},
		{".mpg", "video/mpeg"}, {".mpeg", "video/mpeg"},
		{".3gp", "video/3gpp"}, {".flv", "video/x-flv"},
		{".mp3", "audio/mpeg"}, {".m4a", "audio/mp4"},
		{".wav", "audio/wav"}, {".ogg", "audio/ogg"},
		{".oga", "audio/ogg"}, {".flac", "audio/flac"},
		{".aac", "audio/aac"}, {".wma", "audio/x-ms-wma"},
		{".weba", "audio/webm"}, {".mid", "audio/midi"}
	};
	size_t dot = partPath.rfind('.');
	if (dot == std::string::npos)
		return "application/octet-stream";
	std::string ext = partPath.substr(dot);
	for (auto & c : ext)
		c = static_cast<char>(g_ascii_tolower(c));
	for (const auto & e : map)
		if (ext == e.ext)
			return e.mime;
	return "application/octet-stream";
}

/*!
 * The drawing's pic:nvPr referenced a word/media part (a:videoFile,
 * a:audioFile or p14:media) — import the payload as a piece-table
 * embedded object: the media bytes become the data item, the picture's
 * blip becomes its "snapshot-png-" poster, and the object is appended
 * as a PTO_Embed so the media manager can play it.
 */
UT_Error OXML_Element_Image::_addMediaEmbedToPT(
	PD_Document * pDocument, const OXML_SharedImage & poster,
	const gchar * szMediaRid)
{
	OXMLi_PackageManager * mgr = OXMLi_PackageManager::getInstance();
	if (!mgr || !szMediaRid || !*szMediaRid)
		return UT_ERROR;
	GsfInput * part = mgr->openPartByRelId(szMediaRid);
	if (!part)
		return UT_ERROR;

	UT_ByteBufPtr mbuf(new UT_ByteBuf);
	while (gsf_input_remaining(part) > 0)
	{
		gsf_off_t len = gsf_input_remaining(part);
		const guint8 * d = gsf_input_read(part, len, nullptr);
		if (!d)
			break;
		mbuf->append(d, static_cast<UT_uint32>(len));
	}
	g_object_unref(part);
	if (!mbuf->getLength())
		return UT_ERROR;

	std::string partPath = mgr->getPartPath(szMediaRid);
	std::string mime = s_mimeForMediaPart(partPath);
	std::string dataID = "obj-media-";
	dataID += szMediaRid;

	if (!pDocument->createDataItem(dataID.c_str(), false, mbuf,
	                             mime, nullptr))
		return UT_ERROR;

	/* the blip keeps its own data item for the document; copy it into
	 * the embed's poster slot so the media manager renders it */
	if (poster && poster->getBuffer())
	{
		std::string snapID = "snapshot-png-";
		snapID += dataID;
		std::string snapMime = poster->getMimeType();
		if (snapMime.empty())
			snapMime = "image/png";
		pDocument->createDataItem(snapID.c_str(), false,
		                        poster->getBuffer(), snapMime, nullptr);
	}

	/* rebuild the object attrs: keep the picture's real props
	 * (height/width/title) but drop image attrs and our internal
	 * media bookkeeping */
	PP_PropertyVector props = getProperties();
	PP_PropertyVector keep;
	for (size_t i = 0; i + 1 < props.size(); i += 2)
	{
		const std::string & n = props[i];
		if (n == "media-rid" || n == "media-dataid" ||
			n.compare(0, 11, "altcontent-") == 0)
			continue;
		keep.push_back(props[i]);
		keep.push_back(props[i + 1]);
	}
	PP_addOrSetAttribute("embed-type", "media", keep);
	{
		const gchar * szKind = nullptr;
		if (getProperty("media-kind", szKind) != UT_OK || !szKind || !*szKind)
			szKind = "video";
		PP_addOrSetAttribute("media-kind", szKind, keep);
	}
	if (!partPath.empty())
	{
		const char * base = UT_basename(partPath.c_str());
		if (base && *base)
			PP_addOrSetAttribute("media-name", base, keep);
	}
	std::string pstr;
	for (size_t i = 0; i + 1 < keep.size(); i += 2)
	{
		pstr += keep[i];
		pstr += ":";
		pstr += keep[i + 1];
		pstr += ";";
	}
	if (!pstr.empty())
		pstr.resize(pstr.length() - 1);

	PP_PropertyVector atts;
	atts.push_back("dataid");
	atts.push_back(dataID);
	if (!pstr.empty())
	{
		atts.push_back("props");
		atts.push_back(pstr);
	}
	return pDocument->appendObject(PTO_Embed, atts) ? UT_OK : UT_ERROR;
}
