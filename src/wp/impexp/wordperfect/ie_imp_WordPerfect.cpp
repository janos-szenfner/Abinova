/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */

/* Abinova
 * Copyright (C) 2001 AbiSource, Inc.
 * Copyright (C) 2002-2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2002-2005 William Lachance (william.lachance@sympatico.ca)
 * Copyright (C) 2006 Fridrich Strba (fridrich.strba@bluewin.ch)
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

/* See bug 1764
 * "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <map>
#include <string>

#include <gsf/gsf.h>

#include "ut_types.h"
#include "ut_std_string.h"
#include "ut_string.h"
#include "ut_string_class.h"
#include "ut_units.h"
#include "ut_growbuf.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ut_math.h" // for rint (font size)
#include "ut_locale.h"

#include "xap_Frame.h"
#include "xap_EncodingManager.h"

#include "pd_Document.h"
#include "pt_Types.h"
#include "pl_Listener.h"
#include "pp_AttrProp.h"
#include "pf_Frag_Strux.h"
#include "px_ChangeRecord.h"
#include "px_CR_Span.h"
#include "px_CR_Object.h"
#include "px_CR_Strux.h"

#include "fl_AutoLists.h"
#include "fl_AutoNum.h"

#include "ie_imp_WordPerfect.h"
#include "ie_impexp_WordPerfect.h"
#include "ie_impGraphic.h"
#include "fg_Graphic.h"
#include "ut_base64.h"
#include "ut_bytebuf.h"

// Stream class

#include <librevenge-stream/librevenge-stream.h>
#include <libwpd/libwpd.h>

#ifdef HAVE_LIBWPS
#include <libwps/libwps.h>
#endif

class AbiWordperfectInputStream : public librevenge::RVNGInputStream
{
public:
	AbiWordperfectInputStream(GsfInput *input);
	~AbiWordperfectInputStream();

	virtual bool isStructured() override;
	virtual unsigned subStreamCount() override;
	virtual const char* subStreamName(unsigned) override;
	virtual bool existsSubStream(const char*) override;
	virtual librevenge::RVNGInputStream* getSubStreamByName(const char*) override;
	virtual librevenge::RVNGInputStream* getSubStreamById(unsigned) override;
	virtual const unsigned char *read(unsigned long numBytes, unsigned long &numBytesRead) override;
	virtual int seek(long offset, librevenge::RVNG_SEEK_TYPE seekType) override;
	virtual long tell() override;
	virtual bool isEnd() override;

private:

	GsfInput *m_input;
	GsfInfile *m_ole;
	std::map<unsigned, std::string> m_substreams;
};

AbiWordperfectInputStream::AbiWordperfectInputStream(GsfInput *input) :
	librevenge::RVNGInputStream(),
	m_input(input),
	m_ole(nullptr),
	m_substreams()
{
	g_object_ref(G_OBJECT(input));
}

AbiWordperfectInputStream::~AbiWordperfectInputStream()
{
	if (m_ole)
		g_object_unref(G_OBJECT(m_ole));

	g_object_unref(G_OBJECT(m_input));
}

const unsigned char * AbiWordperfectInputStream::read(unsigned long numBytes, unsigned long &numBytesRead)
{
	const unsigned char *buf = gsf_input_read(m_input, numBytes, nullptr);

	if (buf == nullptr)
		numBytesRead = 0;
	else
		numBytesRead = numBytes;

	return buf;
}

int AbiWordperfectInputStream::seek(long offset, librevenge::RVNG_SEEK_TYPE seekType) 
{
	GSeekType gsfSeekType = G_SEEK_SET;
	switch(seekType)
	{
	case librevenge::RVNG_SEEK_CUR:
		gsfSeekType = G_SEEK_CUR;
		break;
	case librevenge::RVNG_SEEK_SET:
		gsfSeekType = G_SEEK_SET;
		break;
	case librevenge::RVNG_SEEK_END:
		gsfSeekType = G_SEEK_END;
		break;
	}

	return gsf_input_seek(m_input, offset, gsfSeekType);
}

bool AbiWordperfectInputStream::isStructured()
{
	if (!m_ole)
		m_ole = GSF_INFILE(gsf_infile_msole_new (m_input, nullptr));

	if (!m_ole)
		m_ole = GSF_INFILE(gsf_infile_zip_new (m_input, nullptr));

	if (m_ole)
		return true;

	return false;
}

unsigned AbiWordperfectInputStream::subStreamCount()
{
	if (!m_ole)
		m_ole = GSF_INFILE(gsf_infile_msole_new (m_input, nullptr));

	if (!m_ole)
		m_ole = GSF_INFILE(gsf_infile_zip_new (m_input, nullptr));

	if (m_ole)
		{
			int numChildren = gsf_infile_num_children(m_ole);
			if (numChildren > 0)
				return numChildren;
			return 0;
		}
	
	return 0;
}

const char * AbiWordperfectInputStream::subStreamName(unsigned id)
{
	if (!m_ole)
		m_ole = GSF_INFILE(gsf_infile_msole_new (m_input, nullptr));

	if (!m_ole)
		m_ole = GSF_INFILE(gsf_infile_zip_new (m_input, nullptr));

	if (m_ole)
		{
			if (static_cast<int>(id )>= gsf_infile_num_children(m_ole))
			{
				return nullptr;
			}
			std::map<unsigned, std::string>::iterator i = m_substreams.lower_bound(id);
			if (i == m_substreams.end() || m_substreams.key_comp()(id, i->first))
				{
					std::string name = gsf_infile_name_by_index(m_ole, static_cast<int>(id));
					i = m_substreams.insert(i, std::map<unsigned, std::string>::value_type(id, name));
				}
			return i->second.c_str();
		}
	
	return nullptr;
}

bool AbiWordperfectInputStream::existsSubStream(const char * name)
{
	if (!m_ole)
		m_ole = GSF_INFILE(gsf_infile_msole_new (m_input, nullptr));

	if (!m_ole)
		m_ole = GSF_INFILE(gsf_infile_zip_new (m_input, nullptr));

	if (m_ole)
		{
			GsfInput *document = gsf_infile_child_by_name(m_ole, name);
			if (document) 
				{
					g_object_unref(G_OBJECT (document));
					return true;
				}
		}
	
	return false;
}

librevenge::RVNGInputStream * AbiWordperfectInputStream::getSubStreamByName(const char * name)
{
	librevenge::RVNGInputStream *documentStream = nullptr;
	
	if (!m_ole)
		m_ole = GSF_INFILE(gsf_infile_msole_new (m_input, nullptr));

	if (!m_ole)
		m_ole = GSF_INFILE(gsf_infile_zip_new (m_input, nullptr));

	if (m_ole)
		{
			GsfInput *document = gsf_infile_child_by_name(m_ole, name);
			if (document) 
				{
					documentStream = new AbiWordperfectInputStream(document);
					g_object_unref(G_OBJECT (document)); // the only reference should be encapsulated within the new stream
				}
		}
	
	return documentStream;
}

librevenge::RVNGInputStream * AbiWordperfectInputStream::getSubStreamById(unsigned id)
{
	librevenge::RVNGInputStream *documentStream = nullptr;

	if (!m_ole)
		m_ole = GSF_INFILE(gsf_infile_msole_new (m_input, nullptr));

	if (!m_ole)
		m_ole = GSF_INFILE(gsf_infile_zip_new (m_input, nullptr));

	if (m_ole)
		{
			GsfInput *document = gsf_infile_child_by_index(m_ole, static_cast<int>(id));
			if (document) 
				{
					documentStream = new AbiWordperfectInputStream(document);
					g_object_unref(G_OBJECT (document)); // the only reference should be encapsulated within the new stream
				}
		}
	
	return documentStream;
}

long AbiWordperfectInputStream::tell()
{
	return gsf_input_tell(m_input);
}

bool AbiWordperfectInputStream::isEnd()
{
	return gsf_input_eof(m_input);
}

// This should probably be defined in pt_Types.h
static const UT_uint32 PT_MAX_ATTRIBUTES = 8;

ABI_ListDefinition::ABI_ListDefinition(int iOutlineHash) :
	m_iOutlineHash(iOutlineHash)
{
	for(int i=0; i<WP6_NUM_LIST_LEVELS; i++) 
	{
		m_iListIDs[i] = 0;
		m_listTypes[i] = BULLETED_LIST;
		m_iListNumbers[i] = 0;
		m_listLeftOffset[i] = 0.0f;
		m_listMinLabelWidth[i] = 0.0f;
	}
}

void ABI_ListDefinition::setListType(const int level, const char type)
{	
	switch (type)
	{
	case '1':
		m_listTypes[level-1] = NUMBERED_LIST;
		break;
	case 'a':
		m_listTypes[level-1] = LOWERCASE_LIST;
		break;
	case 'A':
		m_listTypes[level-1] = UPPERCASE_LIST;
		break;
	case 'i':
		m_listTypes[level-1] = LOWERROMAN_LIST;
		break;
	case 'I':
		m_listTypes[level-1] = UPPERROMAN_LIST;
		break;
	}
}

#define X_CheckDocumentError(v) if (!v) { UT_DEBUGMSG(("X_CheckDocumentError: %d\n", __LINE__)); }

namespace {

/* serialize an attrprop's attributes and properties into one flat
 * attribute vector (the "props" pseudo-attribute holds the property
 * string) so they can be replayed through appendStrux/appendFmt */
void s_attsWithProps(const PP_AttrProp * pAP, PP_PropertyVector & out)
{
	out = pAP->getAttributes();
	const PP_PropertyVector & props = pAP->getProperties();
	if (props.empty())
		return;

	std::string s;
	for (PP_PropertyVector::const_iterator i = props.begin(); i != props.end(); i += 2)
	{
		if (!s.empty())
			s += ";";
		s += *i;
		s += ':';
		s += *(i + 1);
	}
	out.push_back(PT_PROPS_ATTRIBUTE_NAME);
	out.push_back(s);
}

/* Walks a captured header/footer scratch document via tellListener()
 * and replays its fragments into the destination with the append*()
 * calls importers use -- the destination is still PTS_Loading at that
 * point, so insertSpan()/insertStrux() would refuse to run. Plain
 * PTX_Section struxes in the capture (the seed section) are skipped:
 * everything else lands inside the PTX_SectionHdrFtr the caller has
 * just appended. */
class WP_HdrFtrReplayListener : public PL_Listener
{
public:
	WP_HdrFtrReplayListener(PD_Document * pDest, PD_Document * pSrc)
		: m_pDest(pDest),
		  m_pSrc(pSrc)
	{
	}

	virtual bool populate(fl_ContainerLayout * /*sfh*/,
						  const PX_ChangeRecord * pcr) override
	{
		const PP_AttrProp * pAP = nullptr;
		PP_PropertyVector atts;
		if (m_pSrc->getAttrProp(pcr->getIndexAP(), &pAP) && pAP)
			s_attsWithProps(pAP, atts);

		switch (pcr->getType())
		{
		case PX_ChangeRecord::PXT_InsertSpan:
		{
			const PX_ChangeRecord_Span * pcrs =
					static_cast<const PX_ChangeRecord_Span *>(pcr);
			const UT_UCS4Char * pChars = m_pSrc->getPointer(pcrs->getBufIndex());
			if (!pChars)
				return false;
			return m_pDest->appendFmt(atts)
					&& m_pDest->appendSpan(pChars, pcrs->getLength());
		}
		case PX_ChangeRecord::PXT_InsertObject:
			return m_pDest->appendObject(
					static_cast<const PX_ChangeRecord_Object *>(pcr)->getObjectType(),
					pAP ? pAP->getAttributes() : PP_PropertyVector());
		case PX_ChangeRecord::PXT_InsertFmtMark:
			return m_pDest->appendFmtMark();
		default:
			return true;
		}
	}

	virtual bool populateStrux(pf_Frag_Strux * /*sdh*/,
							   const PX_ChangeRecord * pcr,
							   fl_ContainerLayout ** /*psfh*/) override
	{
		const PX_ChangeRecord_Strux * pcrx =
				static_cast<const PX_ChangeRecord_Strux *>(pcr);
		if (pcrx->getStruxType() == PTX_Section)
			return true; // capture seed/nesting guard - never replayed

		const PP_AttrProp * pAP = nullptr;
		PP_PropertyVector atts;
		if (m_pSrc->getAttrProp(pcr->getIndexAP(), &pAP) && pAP)
			s_attsWithProps(pAP, atts);

		return m_pDest->appendStrux(pcrx->getStruxType(), atts);
	}

	virtual bool change(fl_ContainerLayout * /*sfh*/,
						const PX_ChangeRecord * /*pcr*/) override
	{
		return true;
	}

	virtual bool insertStrux(fl_ContainerLayout * /*sfh*/,
							 const PX_ChangeRecord * /*pcr*/,
							 pf_Frag_Strux * /*sdhNew*/,
							 PL_ListenerId /*lid*/,
							 void (* /*pfnBindHandles*/)(pf_Frag_Strux *,
									PL_ListenerId,
									fl_ContainerLayout *)) override
	{
		return true;
	}

	virtual bool signal(UT_uint32 /*iSignal*/) override
	{
		return true;
	}

private:
	PD_Document * m_pDest;
	PD_Document * m_pSrc;
};

} // anonymous namespace

IE_Imp_WordPerfect_Sniffer::IE_Imp_WordPerfect_Sniffer()
	: IE_ImpSniffer(IE_MIMETYPE_WP_6)
{
}

IE_Imp_WordPerfect_Sniffer::~IE_Imp_WordPerfect_Sniffer()
{
}

// supported suffixes
static IE_SuffixConfidence IE_Imp_WordPerfect_Sniffer__SuffixConfidence[] = {
	{ "wpd", 	UT_CONFIDENCE_PERFECT 	},
	{ "wp", 	UT_CONFIDENCE_PERFECT 	},
	{ "", 	UT_CONFIDENCE_ZILCH 	}
};

const IE_SuffixConfidence * IE_Imp_WordPerfect_Sniffer::getSuffixConfidence ()
{
	return IE_Imp_WordPerfect_Sniffer__SuffixConfidence;
}

UT_Confidence_t IE_Imp_WordPerfect_Sniffer::recognizeContents (GsfInput * input)
{
	AbiWordperfectInputStream gsfInput(input);

	libwpd::WPDConfidence confidence = libwpd::WPDocument::isFileFormatSupported(&gsfInput);
	
	switch (confidence)
	{
		case libwpd::WPD_CONFIDENCE_NONE:
			return UT_CONFIDENCE_ZILCH;
		case libwpd::WPD_CONFIDENCE_EXCELLENT:
			return UT_CONFIDENCE_PERFECT;
		default:
			return UT_CONFIDENCE_ZILCH;
	}
}

UT_Error IE_Imp_WordPerfect_Sniffer::constructImporter (PD_Document * pDocument,
							IE_Imp ** ppie)
{
	*ppie = new IE_Imp_WordPerfect(pDocument);
	return UT_OK;
}

bool IE_Imp_WordPerfect_Sniffer::getDlgLabels  (const char ** pszDesc,
						const char ** pszSuffixList,
						IEFileType * ft)
{
	*pszDesc = "WordPerfect (.wpd, .wp)";
	*pszSuffixList = "*.wpd; *.wp";
	*ft = getFileType();
	return true;
}

/****************************************************************************/
/****************************************************************************/

IE_Imp_WordPerfect::IE_Imp_WordPerfect(PD_Document * pDocument)
  : IE_Imp (pDocument),
	m_leftPageMargin(1.0f),
	m_rightPageMargin(1.0f),
	m_leftSectionMargin(0.0f),
	m_rightSectionMargin(0.0f),
	m_sectionColumnsCount(0),
	m_topMargin(0.0f),
	m_bottomMargin(0.0f),
	m_leftMarginOffset(0.0f),
	m_rightMarginOffset(0.0f),
	m_textIndent(0.0f),
	m_pCurrentListDefinition(nullptr),
	m_bParagraphChanged(false),
	m_bParagraphInSection(false),
	m_bInSection(false),
	m_bSectionChanged(false),
	m_bRequireBlock(false),
	m_iCurrentListLevel(0),
	m_bInCell(false),
	m_bFrameOpen(false),
	m_iLinkOpenCount(0),
	m_pCaptureDoc(nullptr),
	m_bHdrFtrOpenCount(0),
	m_savedListLevel(0),
	m_savedInSection(false),
	m_savedRequireBlock(false),
	m_savedInCell(false)
{
}

IE_Imp_WordPerfect::~IE_Imp_WordPerfect()
{
	//UT_HASH_PURGEDATA(ABI_ListDefinition *,&m_listStylesHash,delete); 
}

UT_Error IE_Imp_WordPerfect::_loadFile(GsfInput * input)
{
	// the prop sprintf's below format floats; pin the numeric locale for the
	// whole parse so a comma-decimal locale can't corrupt the props
	UT_LocaleTransactor lt(LC_NUMERIC, "C");
	AbiWordperfectInputStream gsfInput(input);
	libwpd::WPDResult error = libwpd::WPDocument::parse(&gsfInput, static_cast<librevenge::RVNGTextInterface *>(this), nullptr);

	if (error != libwpd::WPD_OK)
	{
		UT_DEBUGMSG(("AbiWordPerfect: ERROR: %i!\n", static_cast<int>(error)));
		return UT_IE_IMPORTERROR;
	}

	return UT_OK;
}

bool IE_Imp_WordPerfect::pasteFromBuffer (PD_DocumentRange *,
					  const unsigned char *, UT_uint32, const char *)
{
	return false;
}

void IE_Imp_WordPerfect::setDocumentMetaData(const librevenge::RVNGPropertyList &propList)
{
	// libwpd names doc-summary fields after Dublin Core (dc:*) while
	// libwps uses some librevenge:* keys; cover both spellings. Each
	// lookup must read the same key it tests -- propList[] returns
	// nullptr for an absent key, so a mismatched pair dereferences null
	const librevenge::RVNGProperty * creator =
		propList["dc:author"] ? propList["dc:author"] :
		propList["meta:initial-creator"] ? propList["meta:initial-creator"] :
		propList["dc:creator"];
	if (creator)
		getDoc()->setMetaDataProp(PD_META_KEY_CREATOR, creator->getStr().cstr());
	if (propList["dc:subject"])
		getDoc()->setMetaDataProp(PD_META_KEY_SUBJECT, propList["dc:subject"]->getStr().cstr());
	if (propList["dc:publisher"])
		getDoc()->setMetaDataProp(PD_META_KEY_PUBLISHER, propList["dc:publisher"]->getStr().cstr());
	if (propList["dc:type"])
		getDoc()->setMetaDataProp(PD_META_KEY_TYPE, propList["dc:type"]->getStr().cstr());
	if (propList["librevenge:keywords"])
		getDoc()->setMetaDataProp(PD_META_KEY_KEYWORDS, propList["librevenge:keywords"]->getStr().cstr());
	if (propList["meta:keyword"])
		getDoc()->setMetaDataProp(PD_META_KEY_KEYWORDS, propList["meta:keyword"]->getStr().cstr());
	if (propList["dc:language"])
		getDoc()->setMetaDataProp(PD_META_KEY_LANGUAGE, propList["dc:language"]->getStr().cstr());
	if (propList["librevenge:abstract"])
		getDoc()->setMetaDataProp(PD_META_KEY_DESCRIPTION, propList["librevenge:abstract"]->getStr().cstr());
	if (propList["dc:description"])
		getDoc()->setMetaDataProp(PD_META_KEY_DESCRIPTION, propList["dc:description"]->getStr().cstr());
}

void IE_Imp_WordPerfect::startDocument(const librevenge::RVNGPropertyList & /* propList */)
{
	UT_DEBUGMSG(("AbiWordPerfect: startDocument\n"));
}

void IE_Imp_WordPerfect::endDocument()
{
	UT_DEBUGMSG(("AbiWordPerfect: endDocument\n"));

	/* flush hdrftrs of a trailing page span that never got a section,
	 * then emit one PTX_SectionHdrFtr per bound id (a hdrftr id may
	 * only be referenced by a single section, so each bound section
	 * got its own id and needs its own strux copy) */
	closePageSpan();
	for (auto & hdrFtr : m_hdrFtrs)
	{
		for (const std::string & id : hdrFtr->ids)
		{
			const PP_PropertyVector attribs = {
				"type", hdrFtr->type,
				"id", id,
				"listid", "0",
				"parentid", "0"
			};
			X_CheckDocumentError(getDoc()->appendStrux(PTX_SectionHdrFtr, attribs));

			WP_HdrFtrReplayListener listener(getDoc(), hdrFtr->doc);
			if (!hdrFtr->doc->tellListener(static_cast<PL_Listener *>(&listener)))
			{
				UT_DEBUGMSG(("AbiWordPerfect: hdrftr replay failed for id %s\n", id.c_str()));
			}
		}
	}
	m_hdrFtrs.clear();
}

void IE_Imp_WordPerfect::openPageSpan(const librevenge::RVNGPropertyList &propList)
{
	if (m_bHdrFtrOpenCount) return; // page spans cannot nest in a header/footer
	UT_DEBUGMSG(("AbiWordPerfect: openPageSpan\n"));
	
	float marginLeft = 1.0f, marginRight = 1.0f;

	if (propList["fo:margin-left"])
		marginLeft = propList["fo:margin-left"]->getDouble();
	if (propList["fo:margin-right"])
		marginRight = propList["fo:margin-right"]->getDouble();

	if (marginLeft != m_leftPageMargin || marginRight != m_rightPageMargin /* || */
		/* marginTop != m_marginBottom || marginBottom != m_marginBottom */ )
		m_bSectionChanged = true; // margin properties are section properties in Abinova

	m_leftPageMargin = marginLeft;
	m_rightPageMargin = marginRight;
		
}

/* While a header/footer subdocument is being parsed the append* calls
 * are diverted into a scratch document; it is replayed into a real
 * PTX_SectionHdrFtr at endDocument() (the hdrftr strux must come after
 * the body section that references it, which does not exist yet here).
 * If the capture could not be created the content is swallowed instead
 * of leaking into the body stream. */
bool IE_Imp_WordPerfect::appendStrux(PTStruxType pts, const PP_PropertyVector & attributes)
{
	// any strux landing inside an open text box makes it non-empty
	if (!m_textBoxEmpty.empty() && pts != PTX_EndFrame)
		m_textBoxEmpty.back() = false;

	if (m_bHdrFtrOpenCount)
	{
		if (m_pCaptureDoc)
			return m_pCaptureDoc->appendStrux(pts, attributes);
		return true;
	}
	return IE_Imp::appendStrux(pts, attributes);
}

bool IE_Imp_WordPerfect::appendSpan(const UT_UCS4Char * p, UT_uint32 length)
{
	if (m_bHdrFtrOpenCount)
	{
		if (m_pCaptureDoc)
			return m_pCaptureDoc->appendSpan(p, length);
		return true;
	}
	return IE_Imp::appendSpan(p, length);
}

bool IE_Imp_WordPerfect::appendObject(PTObjectType pto, const PP_PropertyVector & attribs,
									  const PP_PropertyVector & /*props*/)
{
	if (m_bHdrFtrOpenCount)
	{
		if (m_pCaptureDoc)
			return m_pCaptureDoc->appendObject(pto, attribs);
		return true;
	}
	return IE_Imp::appendObject(pto, attribs, PP_NOPROPS);
}

bool IE_Imp_WordPerfect::appendFmt(const PP_PropertyVector & pVecAttributes)
{
	if (m_bHdrFtrOpenCount)
	{
		if (m_pCaptureDoc)
			return m_pCaptureDoc->appendFmt(pVecAttributes);
		return true;
	}
	return IE_Imp::appendFmt(pVecAttributes);
}

void IE_Imp_WordPerfect::closePageSpan()
{
	/* hdrftrs of a span that never produced a body section have no
	 * section of their own to render on; bind them to the current
	 * (previous span's) section rather than leaking the reference into
	 * the next span's first section */
	pf_Frag_Strux * pfs = getDoc()->getLastSectionMutableStrux();
	for (auto * hdrFtr : m_pendingHdrFtrs)
	{
		if (!hdrFtr->ids.empty())
			continue;
		if (!pfs)
		{
			// degenerate document with no body section yet -- give the
			// hdrftrs a section to hang on so repairDoc() doesn't drop them
			X_CheckDocumentError(getDoc()->appendStrux(PTX_Section, PP_NOPROPS, &pfs));
			X_CheckDocumentError(getDoc()->appendStrux(PTX_Block, PP_NOPROPS));
			m_bInSection = true;
			m_bRequireBlock = true;
		}
		if (!pfs)
			break;
		_bindHdrFtrToSection(hdrFtr, pfs);
	}
	m_pendingHdrFtrs.clear();
}

/* give this hdrftr a fresh id and reference it from pfs's attribute
 * named after the hdrftr type ("header", "footer-even", ...) */
void IE_Imp_WordPerfect::_bindHdrFtrToSection(WPHdrFtr * hdrFtr, pf_Frag_Strux * pfs)
{
	std::string id = UT_std_string_sprintf("%u", getDoc()->getUID(UT_UniqueId::HeaderFtr));
	X_CheckDocumentError(getDoc()->changeStruxAttsNoUpdate(pfs, hdrFtr->type.c_str(), id.c_str()));
	hdrFtr->ids.push_back(id);
}

void IE_Imp_WordPerfect::_openHdrFtr(bool bHeader, const librevenge::RVNGPropertyList &propList)
{
	m_bHdrFtrOpenCount++;

	/* headers/footers cannot nest: if libwpd fires one inside another
	 * (it shouldn't), let its content flow into the open capture rather
	 * than starting a second one */
	if (m_pCaptureDoc)
		return;

	// getStr() returns by value, so keep the string alive for the
	// comparison rather than holding onto a dangling cstr()
	const librevenge::RVNGString occurrence =
		propList["librevenge:occurrence"]
		? propList["librevenge:occurrence"]->getStr()
		: librevenge::RVNGString();
	bool bEven = (occurrence == "even");
	const char * type = bHeader ? (bEven ? "header-even" : "header")
								: (bEven ? "footer-even" : "footer");

	PD_Document * capture = new PD_Document();
	if (!capture || capture->createRawDocument() != UT_OK)
	{
		UNREFP(capture);
		UT_DEBUGMSG(("AbiWordPerfect: could not create hdrftr capture document\n"));
		return;
	}
	// seed a section so captured content has somewhere to live; the
	// replay listener never replays PTX_Section
	capture->appendStrux(PTX_Section, PP_NOPROPS);

	std::unique_ptr<WPHdrFtr> hdrFtr(new WPHdrFtr);
	hdrFtr->type = type;
	hdrFtr->doc = capture; // ctor refcount of 1, released in ~WPHdrFtr

	// save importer state that libwpd will drive while parsing the
	// subdocument so the enclosing document's parse state survives
	m_savedListDefinition = std::move(m_pCurrentListDefinition);
	m_savedListLevel = m_iCurrentListLevel;
	m_savedInSection = m_bInSection;
	m_savedRequireBlock = m_bRequireBlock;
	m_savedInCell = m_bInCell;
	m_iCurrentListLevel = 0;
	m_bInCell = false;
	m_bInSection = true;   // the capture doc's seed section
	m_bRequireBlock = true;

	m_pCaptureDoc = capture;
	m_pendingHdrFtrs.push_back(hdrFtr.get());
	m_hdrFtrs.push_back(std::move(hdrFtr));
}

void IE_Imp_WordPerfect::_closeHdrFtr()
{
	if (m_bHdrFtrOpenCount > 0)
		m_bHdrFtrOpenCount--;
	if (m_bHdrFtrOpenCount)
		return; // still inside an outer hdrftr

	if (m_pCaptureDoc)
	{
		m_pCaptureDoc = nullptr;

		m_pCurrentListDefinition = std::move(m_savedListDefinition);
		m_iCurrentListLevel = m_savedListLevel;
		m_bInSection = m_savedInSection;
		m_bRequireBlock = m_savedRequireBlock;
		m_bInCell = m_savedInCell;
	}
}

void IE_Imp_WordPerfect::openHeader(const librevenge::RVNGPropertyList &propList)
{
	_openHdrFtr(true, propList);
}

void IE_Imp_WordPerfect::closeHeader()
{
	_closeHdrFtr();
}

void IE_Imp_WordPerfect::openFooter(const librevenge::RVNGPropertyList &propList)
{
	_openHdrFtr(false, propList);
}

void IE_Imp_WordPerfect::closeFooter()
{
	_closeHdrFtr();
}

/* Each section of a page span shares the span's headers and footers.
 * Since one hdrftr id may only be referenced by a single section, every
 * bound section gets a fresh id; each is replayed as its own
 * PTX_SectionHdrFtr at endDocument(). */
void IE_Imp_WordPerfect::_bindPendingHdrFtrs(pf_Frag_Strux * pfs)
{
	for (auto * hdrFtr : m_pendingHdrFtrs)
		_bindHdrFtrToSection(hdrFtr, pfs);
}

void IE_Imp_WordPerfect::openParagraph(const librevenge::RVNGPropertyList &propList)
{
	UT_DEBUGMSG(("AbiWordPerfect: openParagraph()\n"));
	// for now, we always append these options
	float marginTop = 0.0f, marginBottom = 0.0f;
	float marginLeft = 0.0f, marginRight = 0.0f, textIndent = 0.0f;
	if (propList["fo:margin-top"])
	    marginTop = propList["fo:margin-top"]->getDouble();
	if (propList["fo:margin-bottom"])
	    marginBottom = propList["fo:margin-bottom"]->getDouble();
	if (propList["fo:margin-left"])
	    marginLeft = propList["fo:margin-left"]->getDouble();
	if (propList["fo:margin-right"])
	    marginRight = propList["fo:margin-right"]->getDouble();
	if (propList["fo:text-indent"])
	    textIndent = propList["fo:text-indent"]->getDouble();

	m_topMargin = marginTop;
	m_bottomMargin = marginBottom;
	m_leftMarginOffset = marginLeft;
	m_rightMarginOffset = marginRight;
	m_textIndent = textIndent;

	UT_String propBuffer;
	propBuffer += "text-align:";
	if (propList["fo:text-align"])
	{
		// Abinova follows xsl:fo, except here, for some reason..
		if (propList["fo:text-align"]->getStr() == "end")
			propBuffer += "right";
		else
			propBuffer += propList["fo:text-align"]->getStr().cstr();
	}
	else
		propBuffer += "left";

	float lineSpacing = 1.0f;
	if (propList["fo:line-height"])
		lineSpacing = propList["fo:line-height"]->getDouble();
	
	UT_String tmpBuffer;
	UT_String_sprintf(tmpBuffer, "; margin-top:%dpt; margin-bottom:%dpt; margin-left:%.4fin; margin-right:%.4fin; text-indent:%.4fin; line-height:%.4f",
		static_cast<int>((m_topMargin*72)), static_cast<int>((m_bottomMargin*72)), m_leftMarginOffset, m_rightMarginOffset, m_textIndent, lineSpacing);
	propBuffer += tmpBuffer;
	
	const librevenge::RVNGPropertyListVector *tabStops = propList.child("style:tab-stops");
	
	if (tabStops && tabStops->count()) // Append the tabstop information
	{
		propBuffer += "; tabstops:";
		tmpBuffer = "";
		librevenge::RVNGPropertyListVector::Iter i(*tabStops);
		for (i.rewind(); i.next();)
		{
			propBuffer += tmpBuffer;
			if (i()["style:position"])
			{
				UT_String_sprintf(tmpBuffer, "%.4fin", i()["style:position"]->getDouble());
				propBuffer += tmpBuffer;
			}

			if (i()["style:type"])
				if (i()["style:type"]->getStr() == "right")
					propBuffer += "/R";
				else if (i()["style:type"]->getStr() == "center")
					propBuffer += "/C";
				else if (i()["style:type"]->getStr() == "char")
					propBuffer += "/D";
				else
					propBuffer += "/L";
			else // Left aligned is default
				propBuffer += "/L";

			if (i()["style:leader-text"])
				if (i()["style:leader-text"]->getStr() == "-")
					propBuffer += "2";
				else if (i()["style:leader-text"]->getStr() == "_")
					propBuffer += "3";
				else // default to dot leader if the given leader is dot or is not supported by Abinova
					propBuffer += "1";
			else
				propBuffer += "0";

			tmpBuffer = ",";
		}
	}


	UT_DEBUGMSG(("AbiWordPerfect: Appending paragraph properties: %s\n", propBuffer.c_str()));
	const PP_PropertyVector propsArray = {
		"props", propBuffer.c_str()
	};
	X_CheckDocumentError(appendStrux(PTX_Block, propsArray));
	m_bRequireBlock = false;

	if (propList["fo:break-before"])
	{
		if (strcmp(propList["fo:break-before"]->getStr().cstr(), "page") == 0)
		{
			UT_UCS4Char ucs = UCS_FF;
			X_CheckDocumentError(appendSpan(&ucs,1));
		}
		else if (strcmp(propList["fo:break-before"]->getStr().cstr(), "column") == 0)
		{
			UT_UCS4Char ucs = UCS_VTAB;
			X_CheckDocumentError(appendSpan(&ucs,1));
		}
	}
}

void IE_Imp_WordPerfect::openSpan(const librevenge::RVNGPropertyList &propList)
{
	UT_DEBUGMSG(("AbiWordPerfect: Appending current text properties\n"));
	
	const gchar* pProps = "props";
	UT_String propBuffer;
	UT_String tempBuffer;
	
	// bold
	propBuffer += "font-weight:";
	propBuffer += (propList["fo:font-weight"] ? propList["fo:font-weight"]->getStr().cstr() : "normal");
	
	// italic
	propBuffer += "; font-style:";
	propBuffer += (propList["fo:font-style"] ? propList["fo:font-style"]->getStr().cstr() : "normal");
	
	// superscript or subscript
	if (propList["style:text-position"])
	{
		propBuffer += "; text-position:";
		if (strncmp(propList["style:text-position"]->getStr().cstr(), "super", 5) == 0)
			propBuffer += "superscript"; 
		else 
			propBuffer += "subscript";
	}

	if (propList["style:text-underline-type"] || propList["style:text-line-through-type"])
	{
		propBuffer += "; text-decoration:";
		if (propList["style:text-underline-type"])
			propBuffer += "underline ";
		if (propList["style:text-line-through-type"])
			propBuffer += "line-through";

	}
	
	if (propList["style:font-name"])
	{
		propBuffer += "; font-family:";
		propBuffer += propList["style:font-name"]->getStr().cstr();
	}

	// font face
	if (propList["fo:font-size"])
	{
		propBuffer += "; font-size:";
		propBuffer += propList["fo:font-size"]->getStr().cstr();
	}

	if (propList["fo:color"])
	{
		propBuffer += "; color:";
		propBuffer += propList["fo:color"]->getStr().cstr();
	}

	if (propList["fo:background-color"])
	{
		propBuffer += "; bgcolor:";
		propBuffer += propList["fo:background-color"]->getStr().cstr();
	}

	UT_DEBUGMSG(("AbiWordPerfect: Appending span format: %s\n", propBuffer.c_str()));
	const gchar* propsArray[5];
	
	propsArray[0] = pProps;
	propsArray[1] = propBuffer.c_str();
	propsArray[2] = nullptr;
	X_CheckDocumentError(appendFmt(propsArray));
}

void IE_Imp_WordPerfect::openSection(const librevenge::RVNGPropertyList &propList)
{
	if (m_bHdrFtrOpenCount) return; // body sections cannot nest in a header/footer
	UT_DEBUGMSG(("AbiWordPerfect: openSection\n"));

	float marginLeft = 0.0f, marginRight = 0.0f;
	const librevenge::RVNGPropertyListVector *columns = propList.child("style:columns");
	int columnsCount = ((!columns || !columns->count()) ? 1 : columns->count());

	// TODO: support spaceAfter
	if (propList["fo:start-indent"])
		marginLeft = propList["fo:start-indent"]->getDouble();
	if (propList["fo:end-indent"])
		marginRight = propList["fo:end-indent"]->getDouble();

	if (marginLeft != m_leftSectionMargin || marginRight != m_rightSectionMargin || m_sectionColumnsCount != columnsCount)
		m_bSectionChanged = true;

	m_leftSectionMargin = marginLeft;
	m_rightSectionMargin = marginRight;
	m_sectionColumnsCount = columnsCount;
	
	_appendSection(columnsCount, m_leftPageMargin + m_leftSectionMargin, m_rightPageMargin + m_rightSectionMargin); 
}

void IE_Imp_WordPerfect::insertTab()
{
	UT_DEBUGMSG(("AbiWordPerfect: insertTab\n"));

	UT_UCS4Char ucs = UCS_TAB;
	X_CheckDocumentError(appendSpan(&ucs,1));	
}

void IE_Imp_WordPerfect::insertText(const librevenge::RVNGString &text)
{
	if (text.len())
	{
		UT_DEBUGMSG(("AbiWordPerfect: insertText\n"));
		UT_UCS4String ucs4(text.cstr());
		X_CheckDocumentError(appendSpan(ucs4.ucs4_str(), ucs4.length()));
	}
}

void IE_Imp_WordPerfect::insertSpace()
{
	UT_DEBUGMSG(("AbiWordPerfect: insertSpace\n"));

	UT_UCS4Char ucs = UCS_SPACE;
	X_CheckDocumentError(appendSpan(&ucs,1));	
}

void IE_Imp_WordPerfect::insertLineBreak()
{
	UT_DEBUGMSG(("AbiWordPerfect: insertLineBreak\n"));

	UT_UCS4Char ucs = UCS_LF;
	X_CheckDocumentError(appendSpan(&ucs,1));
}

/* libwpd opens links around box contents (box hypertext data) -- the
 * open object lands inline and the end object marks wherever the
 * frame's strux left the flow, so the link keeps its target instead of
 * being dropped */
void IE_Imp_WordPerfect::openLink(const librevenge::RVNGPropertyList &propList)
{
	UT_DEBUGMSG(("AbiWordPerfect: openLink\n"));

	const librevenge::RVNGProperty * href = propList["xlink:href"];
	if (!href)
	{
		UT_DEBUGMSG(("AbiWordPerfect: openLink with no xlink:href - ignoring\n"));
		return;
	}
	// getStr() returns by value: keep it alive until appendObject is done
	const librevenge::RVNGString target = href->getStr();
	if (target.len() == 0)
	{
		UT_DEBUGMSG(("AbiWordPerfect: openLink with empty xlink:href - ignoring\n"));
		return;
	}

	const PP_PropertyVector atts = {
		"xlink:href", target.cstr()
	};
	X_CheckDocumentError(appendObject(PTO_Hyperlink, atts));
	m_iLinkOpenCount++;
}

void IE_Imp_WordPerfect::closeLink()
{
	UT_DEBUGMSG(("AbiWordPerfect: closeLink\n"));

	// an end object with no matching open would mark an empty link
	if (m_iLinkOpenCount <= 0)
		return;
	m_iLinkOpenCount--;

	X_CheckDocumentError(appendObject(PTO_Hyperlink, PP_NOPROPS));
}

void IE_Imp_WordPerfect::insertField(const librevenge::RVNGPropertyList &propList)
{
	UT_DEBUGMSG(("AbiWordPerfect: insertField\n"));

	const char * type = nullptr;
	const librevenge::RVNGProperty * fieldType = propList["librevenge:field-type"];
	if (fieldType)
	{
		// getStr() returns by value: it must outlive the comparisons
		const librevenge::RVNGString t = fieldType->getStr();
		if (!strcmp(t.cstr(), "text:page-number"))
			type = "page_number";
		else if (!strcmp(t.cstr(), "text:page-count"))
			type = "page_count";
		else if (!strcmp(t.cstr(), "text:date-time"))
			type = "date";
		else if (!strcmp(t.cstr(), "text:date"))
			type = "date_ddmmyy";
		else if (!strcmp(t.cstr(), "text:time"))
			type = "time";
	}
	if (!type)
	{
		UT_DEBUGMSG(("AbiWordPerfect: unsupported field type '%s' - dropping\n",
					 fieldType ? fieldType->getStr().cstr() : "(none)"));
		return;
	}

	const PP_PropertyVector fielddef = {
		"type", type
	};
	X_CheckDocumentError(appendObject(PTO_Field, fielddef));
}

/* WP comments are point-anchored: the anchor start object plus the
 * shadow section go down together, the body subdocument parses inside
 * the shadow, and closeComment() closes the shadow then the anchor
 * (no anchored text in between, so it marks a point). */
void IE_Imp_WordPerfect::openComment(const librevenge::RVNGPropertyList & /* propList */)
{
	UT_DEBUGMSG(("AbiWordPerfect: openComment\n"));

	std::string pid = UT_std_string_sprintf("%u", getDoc()->getUID(UT_UniqueId::Annotation));
	m_commentIds.push_back(pid);

	const PP_PropertyVector attribsA = {
		"annotation", pid
	};
	X_CheckDocumentError(appendObject(PTO_Annotation, attribsA));

	const PP_PropertyVector attribsS = {
		"annotation-id", pid
	};
	X_CheckDocumentError(appendStrux(PTX_SectionAnnotation, attribsS));
}

void IE_Imp_WordPerfect::closeComment()
{
	UT_DEBUGMSG(("AbiWordPerfect: closeComment\n"));

	if (m_commentIds.empty())
		return;
	m_commentIds.pop_back();

	X_CheckDocumentError(appendStrux(PTX_EndAnnotation, PP_NOPROPS));
	X_CheckDocumentError(appendObject(PTO_Annotation, PP_NOPROPS));
}

/* openFrame() has already stashed the box geometry; the box's own
 * paragraphs arrive through the normal callbacks until closeTextBox() */
void IE_Imp_WordPerfect::openTextBox(const librevenge::RVNGPropertyList & /* propList */)
{
	UT_DEBUGMSG(("AbiWordPerfect: openTextBox\n"));

	double frameW = 0.0, frameH = 0.0;
	if (m_frameProps["svg:width"])
		frameW = m_frameProps["svg:width"]->getDouble();
	if (m_frameProps["svg:height"])
		frameH = m_frameProps["svg:height"]->getDouble();

	// WP text boxes default to a thin drawn border
	std::string props = "frame-type:textbox" + _frameProps(frameW, frameH) +
		"; top-style:solid; bot-style:solid; left-style:solid; right-style:solid";

	const PP_PropertyVector atts = {
		PT_PROPS_ATTRIBUTE_NAME, props
	};
	X_CheckDocumentError(appendStrux(PTX_SectionFrame, atts));
	m_textBoxEmpty.push_back(true);
}

void IE_Imp_WordPerfect::closeTextBox()
{
	UT_DEBUGMSG(("AbiWordPerfect: closeTextBox\n"));

	// a frame must hold at least one block even when the box's
	// subdocument produced no content
	if (!m_textBoxEmpty.empty() && m_textBoxEmpty.back())
		X_CheckDocumentError(appendStrux(PTX_Block, PP_NOPROPS));
	if (!m_textBoxEmpty.empty())
		m_textBoxEmpty.pop_back();

	X_CheckDocumentError(appendStrux(PTX_EndFrame, PP_NOPROPS));
}

/* libwpd wraps every embedded object (WP3/WP5 pictures, WP6 figure
 * boxes) in a frame carrying the box geometry; the payload follows via
 * insertBinaryObject()/openTextBox(), so the props are stashed for it */
void IE_Imp_WordPerfect::openFrame(const librevenge::RVNGPropertyList &propList)
{
	m_frameProps = propList;
	m_bFrameOpen = true;
}

void IE_Imp_WordPerfect::closeFrame()
{
	m_frameProps.clear();
	m_bFrameOpen = false;
}

/* box geometry stashed by openFrame(), serialized into frame props.
 * libwpd's "char" anchor is its page-anchoring workaround, so page
 * boxes key off it plus the vertical relation -- everything else
 * anchors to its block (the mapping the ODF importer applies to
 * draw:frame). Shared by insertBinaryObject() and openTextBox(). */
std::string IE_Imp_WordPerfect::_frameProps(double frameW, double frameH) const
{
	std::string props;

	librevenge::RVNGString anchor;
	if (m_bFrameOpen && m_frameProps["text:anchor-type"])
		anchor = m_frameProps["text:anchor-type"]->getStr();

	const librevenge::RVNGProperty * verticalRel = m_frameProps["style:vertical-rel"];
	if (!strcmp(anchor.cstr(), "char") ||
		(verticalRel &&
		 (!strcmp(verticalRel->getStr().cstr(), "page") ||
		  !strcmp(verticalRel->getStr().cstr(), "page-content"))))
	{
		props += "; position-to:page-above-text";
		if (m_frameProps["svg:x"])
			props += UT_std_string_sprintf("; frame-page-xpos:%.4gin",
										   m_frameProps["svg:x"]->getDouble());
		if (m_frameProps["svg:y"])
			props += UT_std_string_sprintf("; frame-page-ypos:%.4gin",
										   m_frameProps["svg:y"]->getDouble());
	}
	else
	{
		props += "; position-to:block-above-text";
		if (m_frameProps["svg:x"])
			props += UT_std_string_sprintf("; xpos:%.4gin",
										   m_frameProps["svg:x"]->getDouble());
		if (m_frameProps["svg:y"])
			props += UT_std_string_sprintf("; ypos:%.4gin",
										   m_frameProps["svg:y"]->getDouble());
	}

	if (frameW > 0.0)
		props += UT_std_string_sprintf("; frame-width:%.4gin", frameW);
	if (frameH > 0.0)
		props += UT_std_string_sprintf("; frame-height:%.4gin", frameH);

	/* WP3/WP5 boxes can request text wrap; the rest float above */
	if (m_frameProps["style:wrap"] &&
		!strcmp(m_frameProps["style:wrap"]->getStr().cstr(), "dynamic"))
		props += "; wrap-mode:wrapped-both";

	return props;
}

void IE_Imp_WordPerfect::insertBinaryObject(const librevenge::RVNGPropertyList &propList)
{
	const librevenge::RVNGProperty * binaryProp = propList["office:binary-data"];
	if (!binaryProp)
		return;

	/* RVNGProperty only exposes the binary payload as base64 text;
	 * decode it back to the raw image bytes (the same path the ODF
	 * importer uses for flat <office:binary-data>) */
	const librevenge::RVNGString base64 = binaryProp->getStr();
	UT_ByteBufPtr encoded(new UT_ByteBuf);
	encoded->ins(0, reinterpret_cast<const UT_Byte *>(base64.cstr()), base64.len());
	UT_ByteBufPtr imgBuf(new UT_ByteBuf);
	if (!UT_Base64Decode(imgBuf, encoded) || imgBuf->getLength() == 0)
		return;

	IE_ImpGraphic * pieg = nullptr;
	if (IE_ImpGraphic::constructImporter(imgBuf, IEGFT_Unknown, &pieg) != UT_OK || !pieg)
	{
		UT_DEBUGMSG(("AbiWordPerfect: no graphic importer for embedded object\n"));
		return;
	}
	FG_ConstGraphicPtr pfg;
	UT_Error status = pieg->importGraphic(imgBuf, pfg);
	delete pieg;
	if (status != UT_OK || !pfg || !pfg->getBuffer())
	{
		UT_DEBUGMSG(("AbiWordPerfect: embedded graphic import failed\n"));
		return;
	}

	/* the box geometry comes from the openFrame() around the object;
	 * absent that, fall back to the image's natural size (raster
	 * graphics report pixels, vectors already report inches) */
	double frameW = 0.0, frameH = 0.0;
	if (m_frameProps["svg:width"])
		frameW = m_frameProps["svg:width"]->getDouble();
	if (m_frameProps["svg:height"])
		frameH = m_frameProps["svg:height"]->getDouble();
	if (frameW <= 0.0 || frameH <= 0.0)
	{
		double w = pfg->getWidth(), h = pfg->getHeight();
		if (pfg->getType() == FGT_Raster)
		{
			w /= 96.0;
			h /= 96.0;
		}
		if (frameW <= 0.0)
			frameW = w;
		if (frameH <= 0.0)
			frameH = h;
	}
	if (frameW <= 0.0 || frameH <= 0.0)
	{
		UT_DEBUGMSG(("AbiWordPerfect: embedded graphic has no usable size\n"));
		return;
	}

	/* the data item must live in the real document even while a
	 * header/footer capture diverts the object frag into the scratch
	 * document -- the replayed object resolves its dataid there */
	const std::string dataid =
		UT_std_string_sprintf("%u", getDoc()->getUID(UT_UniqueId::Image));
	if (!getDoc()->createDataItem(dataid.c_str(), false, pfg->getBuffer(),
								  pfg->getMimeType(), nullptr))
		return;

	librevenge::RVNGString anchor;
	if (m_bFrameOpen && m_frameProps["text:anchor-type"])
		anchor = m_frameProps["text:anchor-type"]->getStr();

	/* positioned frames cannot live inside the header/footer capture
	 * document (only flat content replays from it), so those images
	 * are inlined like the ODF importer does inside hdrftrs */
	if (!m_bFrameOpen || m_bHdrFtrOpenCount || !strcmp(anchor.cstr(), "as-char"))
	{
		std::string props =
			UT_std_string_sprintf("width:%.4gin; height:%.4gin", frameW, frameH);
		const PP_PropertyVector atts = {
			"dataid", dataid,
			PT_PROPS_ATTRIBUTE_NAME, props
		};
		X_CheckDocumentError(appendObject(PTO_Image, atts));
		return;
	}

	/* a floating box becomes a frame strux */
	std::string props = "frame-type:image" + _frameProps(frameW, frameH);
	/* picture boxes get no drawn frame border */
	props += "; top-style:none; bot-style:none; left-style:none; right-style:none";

	const PP_PropertyVector atts = {
		PT_STRUX_IMAGE_DATAID, dataid,
		PT_PROPS_ATTRIBUTE_NAME, props
	};
	X_CheckDocumentError(appendStrux(PTX_SectionFrame, atts));
	X_CheckDocumentError(appendStrux(PTX_Block, PP_NOPROPS));
	X_CheckDocumentError(appendStrux(PTX_EndFrame, PP_NOPROPS));
}

/* drawing shapes inside boxes have no Abinova construct to map to --
 * log instead of silently dropping */
void IE_Imp_WordPerfect::drawRectangle(const librevenge::RVNGPropertyList & /* propList */)
{
	UT_DEBUGMSG(("AbiWordPerfect: drawRectangle unsupported - dropping shape\n"));
}

void IE_Imp_WordPerfect::drawEllipse(const librevenge::RVNGPropertyList & /* propList */)
{
	UT_DEBUGMSG(("AbiWordPerfect: drawEllipse unsupported - dropping shape\n"));
}

void IE_Imp_WordPerfect::drawPolygon(const librevenge::RVNGPropertyList & /* propList */)
{
	UT_DEBUGMSG(("AbiWordPerfect: drawPolygon unsupported - dropping shape\n"));
}

void IE_Imp_WordPerfect::drawPolyline(const librevenge::RVNGPropertyList & /* propList */)
{
	UT_DEBUGMSG(("AbiWordPerfect: drawPolyline unsupported - dropping shape\n"));
}

void IE_Imp_WordPerfect::drawPath(const librevenge::RVNGPropertyList & /* propList */)
{
	UT_DEBUGMSG(("AbiWordPerfect: drawPath unsupported - dropping shape\n"));
}

void IE_Imp_WordPerfect::drawConnector(const librevenge::RVNGPropertyList & /* propList */)
{
	UT_DEBUGMSG(("AbiWordPerfect: drawConnector unsupported - dropping shape\n"));
}

void IE_Imp_WordPerfect::insertEquation(const librevenge::RVNGPropertyList & /* propList */)
{
	UT_DEBUGMSG(("AbiWordPerfect: insertEquation unsupported - dropping equation\n"));
}


void IE_Imp_WordPerfect::openOrderedListLevel(const librevenge::RVNGPropertyList &propList)
{
	UT_DEBUGMSG(("AbiWordPerfect: openOrderedListLevel\n"));
	
	int listID = 0, startingNumber = 0, level = 1;
	char listType = '1';
	UT_UTF8String textBeforeNumber, textAfterNumber;
	float listLeftOffset = 0.0f;
	float listMinLabelWidth = 0.0f;
	
	if (propList["librevenge:id"])
		listID = propList["librevenge:id"]->getInt();
	if (propList["text:start-value"])
		startingNumber = propList["text:start-value"]->getInt();
	if (propList["librevenge:level"])
		level = propList["librevenge:level"]->getInt();
	if (level < 1 || level > WP6_NUM_LIST_LEVELS)
		level = 1; // level indexes fixed-size arrays below
	if (propList["style:num-prefix"])
		textBeforeNumber += propList["style:num-prefix"]->getStr().cstr();
	if (propList["style:num-suffix"])
		textAfterNumber += propList["style:num-suffix"]->getStr().cstr();
	if (propList["style:num-format"])
		listType = propList["style:num-format"]->getStr().cstr()[0];
	if (propList["text:space-before"])
		listLeftOffset = propList["text:space-before"]->getDouble();
	if (propList["text:min-label-width"])
		listMinLabelWidth = propList["text:min-label-width"]->getDouble();

	if (!m_pCurrentListDefinition ||
		m_pCurrentListDefinition->getOutlineHash() != listID ||
		(m_pCurrentListDefinition->getLevelNumber(level) != startingNumber &&
		 level == 1))
	{
		m_pCurrentListDefinition = std::make_unique<ABI_ListDefinition>(listID);
	}

	if (!m_pCurrentListDefinition->getListID(level))
	{
		m_pCurrentListDefinition->setListType(level, listType);
		m_pCurrentListDefinition->setListID(level, getDoc()->getUID(UT_UniqueId::List));
		m_pCurrentListDefinition->setListLeftOffset(level, listLeftOffset);
		m_pCurrentListDefinition->setListMinLabelWidth(level, listMinLabelWidth);
		_updateDocumentOrderedListDefinition(m_pCurrentListDefinition.get(), level, listType, textBeforeNumber, textAfterNumber, startingNumber);
	}

	m_iCurrentListLevel++;
}

void IE_Imp_WordPerfect::closeOrderedListLevel()
{
	UT_DEBUGMSG(("AbiWordPerfect: closeOrderedListLevel (level: %i)\n", m_iCurrentListLevel));

	// every time we close a list level, the level above it is normally renumbered to start at "1"
	// again. this code takes care of that.
	if (m_iCurrentListLevel < (WP6_NUM_LIST_LEVELS-1) && m_iCurrentListLevel >= 0 && m_pCurrentListDefinition)
		m_pCurrentListDefinition->setLevelNumber(m_iCurrentListLevel + 1, 0);

	if (m_iCurrentListLevel > 0)
		m_iCurrentListLevel--;
}

void IE_Imp_WordPerfect::openUnorderedListLevel(const librevenge::RVNGPropertyList &propList)
{
	UT_DEBUGMSG(("AbiWordPerfect: openUNorderedListLevel\n"));
	
	int listID = 0, level = 1;
	librevenge::RVNGString textBeforeNumber, textAfterNumber;
	float listLeftOffset = 0.0f;
	float listMinLabelWidth = 0.0f;
	
	if (propList["librevenge:id"])
		listID = propList["librevenge:id"]->getInt();
	if (propList["librevenge:level"])
		level = propList["librevenge:level"]->getInt();
	if (level < 1 || level > WP6_NUM_LIST_LEVELS)
		level = 1; // level indexes fixed-size arrays below
	if (propList["text:space-before"])
		listLeftOffset = propList["text:space-before"]->getDouble();
	if (propList["text:min-label-width"])
		listMinLabelWidth = propList["text:min-label-width"]->getDouble();

	if (!m_pCurrentListDefinition || m_pCurrentListDefinition->getOutlineHash() != listID)
	{
		m_pCurrentListDefinition = std::make_unique<ABI_ListDefinition>(listID);
	}

	if (!m_pCurrentListDefinition->getListID(level))
	{
		m_pCurrentListDefinition->setListID(level, getDoc()->getUID(UT_UniqueId::List));
		m_pCurrentListDefinition->setListLeftOffset(level, listLeftOffset);
		m_pCurrentListDefinition->setListMinLabelWidth(level, listMinLabelWidth);
		_updateDocumentUnorderedListDefinition(m_pCurrentListDefinition.get(), level);
	}

	m_iCurrentListLevel++;
}

void IE_Imp_WordPerfect::closeUnorderedListLevel()
{
	UT_DEBUGMSG(("AbiWordPerfect: closeUnorderedListLevel (level: %i)\n", m_iCurrentListLevel));

	if (m_iCurrentListLevel > 0)
		m_iCurrentListLevel--;
}

// ASSUMPTION: We assume that unordered lists will always pass a number of "0". unpredictable behaviour
// may result otherwise
void IE_Imp_WordPerfect::openListElement(const librevenge::RVNGPropertyList &propList)
{
	UT_DEBUGMSG(("AbiWordPerfect: openListElement\n"));

	// a malformed doc can emit a list element without an open level;
	// UT_ASSERT compiles out in release, so guard for real
	if (!m_pCurrentListDefinition || m_iCurrentListLevel < 1 ||
		m_iCurrentListLevel > WP6_NUM_LIST_LEVELS)
	{
		UT_DEBUGMSG(("AbiWordPerfect: openListElement with no open list level (level: %i) - ignoring\n", m_iCurrentListLevel));
		return;
	}
	
	// Paragraph properties for our list element
	UT_String szListID;
	UT_String szParentID;
	UT_String szLevel;
	UT_String_sprintf(szListID,"%d",m_pCurrentListDefinition->getListID(m_iCurrentListLevel));
	if (m_iCurrentListLevel > 1) 
		UT_String_sprintf(szParentID,"%d", m_pCurrentListDefinition->getListID((m_iCurrentListLevel-1)));
	else
		UT_String_sprintf(szParentID,"0"); 
	UT_String_sprintf(szLevel,"%d", m_iCurrentListLevel);
	
	const gchar* listAttribs[PT_MAX_ATTRIBUTES*2 + 1];
	UT_uint32 attribsCount=0;
	
	listAttribs[attribsCount++] = PT_LISTID_ATTRIBUTE_NAME;
	listAttribs[attribsCount++] = szListID.c_str();
	listAttribs[attribsCount++] = PT_PARENTID_ATTRIBUTE_NAME;
	listAttribs[attribsCount++] = szParentID.c_str();
	listAttribs[attribsCount++] = PT_LEVEL_ATTRIBUTE_NAME;
	listAttribs[attribsCount++] = szLevel.c_str();
	
	// Now handle the Abi List properties
	UT_String propBuffer;
	UT_String tempBuffer;
	UT_String_sprintf(tempBuffer,"list-style:%i;", m_pCurrentListDefinition->getListType(m_iCurrentListLevel));
	propBuffer += tempBuffer;


	if (m_pCurrentListDefinition->getListType(m_iCurrentListLevel) == BULLETED_LIST)
		UT_String_sprintf(tempBuffer, "field-font:Symbol; ");
	else
		UT_String_sprintf(tempBuffer, "field-font:nullptr; ");
	
	m_pCurrentListDefinition->incrementLevelNumber(m_iCurrentListLevel);
	
	propBuffer += tempBuffer;
	UT_String_sprintf(tempBuffer, "start-value:%i; ", 1);
	propBuffer += tempBuffer;

	UT_String_sprintf(tempBuffer, "margin-left:%.4fin; ", m_pCurrentListDefinition->getListLeftOffset(m_iCurrentListLevel)
					+ m_pCurrentListDefinition->getListMinLabelWidth(m_iCurrentListLevel)
					- (propList["fo:text-indent"] ? propList["fo:text-indent"]->getDouble() : 0.0f));
	propBuffer += tempBuffer;
	UT_String_sprintf(tempBuffer, "text-indent:%.4fin", - m_pCurrentListDefinition->getListMinLabelWidth(m_iCurrentListLevel)
					+ (propList["fo:text-indent"] ? propList["fo:text-indent"]->getDouble() : 0.0f));
	propBuffer += tempBuffer;

	listAttribs[attribsCount++] = PT_PROPS_ATTRIBUTE_NAME;
	listAttribs[attribsCount++] = propBuffer.c_str();
	listAttribs[attribsCount++] = nullptr;

	X_CheckDocumentError(appendStrux(PTX_Block, PP_std_copyProps(listAttribs)));
	m_bRequireBlock = false;

	// hang text off of a list label
	_doc()->appendFmtMark();
	UT_DEBUGMSG(("WordPerfect: LISTS - Appended a list tag def'n (character props)\n"));

	// append a list field label
	PP_PropertyVector fielddef = {
		"type", "list_label"
	};
	X_CheckDocumentError(appendObject(PTO_Field,fielddef));
	UT_DEBUGMSG(("WordPerfect: LISTS - Appended a field def'n\n"));

	// insert a tab
	UT_UCS4Char ucs = UCS_TAB;
	X_CheckDocumentError(appendSpan(&ucs,1));
}

void IE_Imp_WordPerfect::openFootnote(const librevenge::RVNGPropertyList & /*propList*/)
{
	if (!m_bInSection && !m_bHdrFtrOpenCount)
	{
		X_CheckDocumentError(appendStrux(PTX_Section, PP_NOPROPS));
		X_CheckDocumentError(appendStrux(PTX_Block,PP_NOPROPS));
		m_bInSection = true;
	}

	std::string footnoteId = UT_std_string_sprintf("%u", getDoc()->getUID(UT_UniqueId::Footnote));

	PP_PropertyVector propsArray = {
		"type",	"footnote_ref",
		"footnote-id", footnoteId
	};
	X_CheckDocumentError(appendObject(PTO_Field, propsArray));

	const PP_PropertyVector attribs = {
		"footnote-id", footnoteId
	};
	X_CheckDocumentError(appendStrux(PTX_SectionFootnote, attribs));

	X_CheckDocumentError(appendStrux(PTX_Block, PP_NOPROPS));
	m_bRequireBlock = false;

	// just change the type.
	propsArray[1] = "footnote_anchor";
	X_CheckDocumentError(appendObject(PTO_Field, propsArray));
}

void IE_Imp_WordPerfect::closeFootnote()
{
	X_CheckDocumentError(appendStrux(PTX_EndFootnote,PP_NOPROPS));
}

void IE_Imp_WordPerfect::openEndnote(const librevenge::RVNGPropertyList & /*propList*/)
{
	std::string endnoteId = UT_std_string_sprintf("%u", getDoc()->getUID(UT_UniqueId::Endnote));

	PP_PropertyVector propsArray = {
		"type",	"endnote_ref",
		"endnote-id", endnoteId
	};
	X_CheckDocumentError(appendObject(PTO_Field, propsArray));

	const PP_PropertyVector attribs = {
		"endnote-id", endnoteId
	};
	X_CheckDocumentError(appendStrux(PTX_SectionEndnote, attribs));

	X_CheckDocumentError(appendStrux(PTX_Block, PP_NOPROPS));
	m_bRequireBlock = false;

	propsArray [1] = "endnote_anchor";
	X_CheckDocumentError(appendObject(PTO_Field, propsArray));
}

void IE_Imp_WordPerfect::closeEndnote()
{
	X_CheckDocumentError(appendStrux(PTX_EndEndnote, PP_NOPROPS));
}

void IE_Imp_WordPerfect::openTable(const librevenge::RVNGPropertyList &propList)
{
	// TODO: handle 'marginLeftOffset' and 'marginRightOffset'
	UT_DEBUGMSG(("AbiWordPerfect: openTable\n"));
	
	UT_String propBuffer;

	if (propList["table:align"])
	{
		// no need to support left: default behaviour

		//if (strcmp(propList["table:align"]->getStr().cstr(), "right"))
		// abiword does not support this I think
		//if (strcmp(propList["table:align"]->getStr().cstr(), "center"))
		// abiword does not support this I think
		//if (strcmp(propList["table:align"]->getStr().cstr(), "margins"))
		// abiword does not support this I think
		if (strcmp(propList["table:align"]->getStr().cstr(), "margins"))
		{
			if (propList["fo:margin-left"])
				UT_String_sprintf(propBuffer, "table-column-leftpos:%s; ", propList["fo:margin-left"]->getStr().cstr());
		}
	}
	
	const librevenge::RVNGPropertyListVector *columns = propList.child("librevenge:table-columns");
	if (columns)
	{
		propBuffer += "table-column-props:";
		librevenge::RVNGPropertyListVector::Iter i(*columns);
		for (i.rewind(); i.next();)
		{
			UT_String tmpBuffer;
			if (i()["style:column-width"])
				UT_String_sprintf(tmpBuffer, "%s/", i()["style:column-width"]->getStr().cstr());
			propBuffer += tmpBuffer;
		}
	}

	const PP_PropertyVector propsArray = {
		"props", propBuffer.c_str()
	};
	X_CheckDocumentError(appendStrux(PTX_SectionTable, propsArray));
}

void IE_Imp_WordPerfect::openTableRow(const librevenge::RVNGPropertyList & /*propList*/)
{
	UT_DEBUGMSG(("AbiWordPerfect: openRow\n"));
	if (m_bInCell)
	{
		X_CheckDocumentError(appendStrux(PTX_EndCell, PP_NOPROPS));
	}

	m_bInCell = false;
}

void IE_Imp_WordPerfect::openTableCell(const librevenge::RVNGPropertyList &propList)
{
	int col =0,  row = 0, colSpan = 0, rowSpan = 0;
	if (propList["librevenge:column"])
		col = propList["librevenge:column"]->getInt();
	if (propList["librevenge:row"])
		row = propList["librevenge:row"]->getInt();
	if (propList["table:number-columns-spanned"])
		colSpan = propList["table:number-columns-spanned"]->getInt();
	if (propList["table:number-rows-spanned"])
		rowSpan = propList["table:number-rows-spanned"]->getInt();

	UT_DEBUGMSG(("AbiWordPerfect: openCell(col: %d, row: %d, colSpan: %d, rowSpan: %d\n", col, row, colSpan, rowSpan));
	if (m_bInCell)
	{
		X_CheckDocumentError(appendStrux(PTX_EndCell, PP_NOPROPS));
	}
	
	UT_String propBuffer;
	UT_String_sprintf(propBuffer, "left-attach:%d; right-attach:%d; top-attach:%d; bot-attach:%d",
					col, col+colSpan, row, row+rowSpan);
	
	UT_String borderStyle;
	// we only support bg-style:1 for now
	bool borderLeftSolid = false;
	bool borderRightSolid = false;
	bool borderTopSolid = false;
	bool borderBottomSolid = false;
	if (propList["fo:border-left"])
		borderLeftSolid = strncmp(propList["fo:border-left"]->getStr().cstr(), "0.0inch", 7);
	if (propList["fo:border-right"])
		borderRightSolid = strncmp(propList["fo:border-right"]->getStr().cstr(), "0.0inch", 7);
	if (propList["fo:border-top"])
		borderTopSolid = strncmp(propList["fo:border-top"]->getStr().cstr(), "0.0inch", 7);
	if (propList["fo:border-bottom"])
		borderBottomSolid = strncmp(propList["fo:border-bottom"]->getStr().cstr(), "0.0inch", 7);

	UT_String_sprintf(borderStyle, "; left-style:%s; right-style:%s; top-style:%s; bot-style:%s", 
					  (borderLeftSolid ? "solid" : "none"),
					  (borderRightSolid ? "solid" : "none"), 
					  (borderTopSolid ? "solid" : "none"), 
					  (borderBottomSolid ? "solid" : "none"));
	propBuffer += borderStyle;
		
	// we only support bg-style:1 for now
	if (propList["fo:background-color"])
	{
		UT_String bgCol;
		UT_String_sprintf(bgCol, "; bg-style:1; background-color:%s", &(propList["fo:background-color"]->getStr().cstr()[1]));
		propBuffer += bgCol;
	}
	
	UT_DEBUGMSG(("AbiWordPerfect: Inserting a Cell definition: %s\n", propBuffer.c_str()));
	
	const PP_PropertyVector propsArray = {
		"props", propBuffer.c_str()
	};

	X_CheckDocumentError(appendStrux(PTX_SectionCell, propsArray));
	m_bInCell = true;
}

void IE_Imp_WordPerfect::closeTable()
{
	UT_DEBUGMSG(("AbiWordPerfect: Closing table\n"));
	
	if (m_bInCell)
	{
		X_CheckDocumentError(appendStrux(PTX_EndCell, PP_NOPROPS));
	}
	X_CheckDocumentError(appendStrux(PTX_EndTable, PP_NOPROPS));
	m_bInCell = false;
	
	// we need to open a new paragraph after a table, since libwpd does NOT do it
	// FIXME: NEED TO PASS THE CURRENT PROPERTIES INSTEAD OF nullptr
	// NOTE: THIS SUCKS.........
	X_CheckDocumentError(appendStrux(PTX_Block, PP_NOPROPS));
	m_bRequireBlock = false;
}

UT_Error IE_Imp_WordPerfect::_appendSection(int numColumns, const float marginLeft, const float marginRight)
{
	UT_DEBUGMSG(("AbiWordPerfect: Appending section\n"));
	
	UT_String myProps("") ;
	UT_LocaleTransactor lt(LC_NUMERIC, "C");
	myProps += UT_String_sprintf("columns:%d; page-margin-left:%.4fin; page-margin-right:%.4fin", numColumns, marginLeft, marginRight);

	if(m_bInSection && m_bRequireBlock) // Abinova will hang on an empty <section>
	{
		X_CheckDocumentError(appendStrux(PTX_Block,PP_NOPROPS));
	}

	const PP_PropertyVector propsArray = {
		"props", myProps.c_str()
	};
	pf_Frag_Strux * pfs = nullptr;
	X_CheckDocumentError(getDoc()->appendStrux(PTX_Section, propsArray, &pfs));

	// the page span's headers/footers apply to every section the span
	// produces; each binding gets a fresh id since an id may only be
	// referenced once
	if (pfs)
		_bindPendingHdrFtrs(pfs);

	m_bInSection = true;
	m_bRequireBlock = true;

	m_bSectionChanged = false;

	return UT_OK;
}

// NB: Abinova-2.0 doesn't properly support nested lists with different nested styles: only "1" style
// really looks proper. We hack around this be only using the style given at level "1"
// NB: Abinova-2.0 doesn't properly support setting list delimeters at levels greater than 1,
// we hack around this by using only "plain" (e.g.: nullptr) list delimeters on levels greater than 1.
UT_Error IE_Imp_WordPerfect::_updateDocumentOrderedListDefinition(ABI_ListDefinition *pListDefinition, int iLevel, 
																  const char /*listType*/, const UT_UTF8String &sTextBeforeNumber, 
																  const UT_UTF8String &sTextAfterNumber, int iStartingNumber)
{
	UT_DEBUGMSG(("AbiWordPerfect: Updating document list definition (iLevel: %i)\n", iLevel));

	if (iLevel > 1) {
        UT_DEBUGMSG(("WLACH: Parent's list id is.. %i\n", pListDefinition->getListID((iLevel-1))));
    }

	// finally, set the document's list identification info..
	fl_AutoNumPtr pAuto = getDoc()->getListByID(pListDefinition->getListID(iLevel));
	// not in document yet, we should create a list for it
	if (!pAuto) {
		UT_DEBUGMSG(("AbiWordPerfect: pAuto is nullptr: creating a list\n"));
		if (iLevel > 1) {
			pAuto = std::make_shared<fl_AutoNum>(pListDefinition->getListID(iLevel),
												 pListDefinition->getListID((iLevel-1)),
												 pListDefinition->getListType(1),
												 iStartingNumber, "%L", ".",
												 getDoc(), nullptr);
		} else {
			UT_UTF8String sNumberingString;
			UT_UTF8String sNumber("%L", static_cast<size_t>(0));

			sNumberingString += sTextBeforeNumber;
			sNumberingString += sNumber;
			sNumberingString += sTextAfterNumber;

			pAuto = std::make_shared<fl_AutoNum>(pListDefinition->getListID(iLevel), 0,
												 pListDefinition->getListType(iLevel),
												 iStartingNumber, sNumberingString.utf8_str(),
												 ".", getDoc(), nullptr);
		}
		getDoc()->addList(pAuto);
	} else {
		// we should update what we have
		UT_DEBUGMSG(("AbiWordPerfect: pAuto already exists\n"));
	}

	pAuto->fixHierarchy();

	return UT_OK;
}

UT_Error IE_Imp_WordPerfect::_updateDocumentUnorderedListDefinition(ABI_ListDefinition *pListDefinition, int iLevel)
{
	UT_DEBUGMSG(("AbiWordPerfect: Updating document list definition (iLevel: %i)\n", iLevel));
	
	// finally, set the document's list identification info..
	fl_AutoNumPtr pAuto = getDoc()->getListByID(pListDefinition->getListID(iLevel));
	// not in document yet, we should create a list for it
	if (!pAuto)	{
		UT_DEBUGMSG(("AbiWordPerfect: pAuto is nullptr: creating a list\n"));
		if (iLevel > 1) {
			pAuto = std::make_shared<fl_AutoNum>(pListDefinition->getListID(iLevel),
												 pListDefinition->getListID((iLevel-1)),
												 pListDefinition->getListType(1), 0, "%L", ".",
												 getDoc(), nullptr);
		} else {
			pAuto = std::make_shared<fl_AutoNum>(pListDefinition->getListID(iLevel), 0,
												 pListDefinition->getListType(iLevel), 0,
												 "%L", ".", getDoc(), nullptr);
		}
		getDoc()->addList(pAuto);
	} else {
		// we should update what we have
		UT_DEBUGMSG(("AbiWordPerfect: pAuto already exists\n"));
	}

	pAuto->fixHierarchy();

	return UT_OK;
}

#ifdef HAVE_LIBWPS

class IE_Imp_MSWorks : public IE_Imp_WordPerfect
{
public:

    IE_Imp_MSWorks(PD_Document * pDocument)
		: IE_Imp_WordPerfect(pDocument)
	{
	}

    ~IE_Imp_MSWorks()
	{
	}
    
protected:
    virtual UT_Error _loadFile(GsfInput * input) override
	{
		UT_LocaleTransactor lt(LC_NUMERIC, "C"); // same prop sprintf's as the wpd path
		AbiWordperfectInputStream gsfInput(input);
		libwps::WPSResult error = libwps::WPSDocument::parse(&gsfInput, static_cast<librevenge::RVNGTextInterface *>(this), nullptr, nullptr);

		if (error != libwps::WPS_OK)
			{
				UT_DEBUGMSG(("AbiMSWorks: ERROR: %i!\n", static_cast<int>(error)));
				return UT_IE_IMPORTERROR;
			}
		
		return UT_OK;
	}
};

/****************************************************************************************/
/****************************************************************************************/

IE_Imp_MSWorks_Sniffer::IE_Imp_MSWorks_Sniffer()
	: IE_ImpSniffer("application/vnd.ms-works")
{
}

IE_Imp_MSWorks_Sniffer::~IE_Imp_MSWorks_Sniffer()
{
}

// supported suffixes
static IE_SuffixConfidence IE_Imp_MSWorks_Sniffer__SuffixConfidence[] = {
	{ "wps", 	UT_CONFIDENCE_PERFECT 	},
	{ "", 	UT_CONFIDENCE_ZILCH 	}
};

const IE_SuffixConfidence * IE_Imp_MSWorks_Sniffer::getSuffixConfidence ()
{
	return IE_Imp_MSWorks_Sniffer__SuffixConfidence;
}

UT_Confidence_t IE_Imp_MSWorks_Sniffer::recognizeContents (GsfInput * input) 
{
	AbiWordperfectInputStream gsfInput(input);

	libwps::WPSCreator creator;
	libwps::WPSKind kind;
	bool needEncoding;
	libwps::WPSConfidence confidence = libwps::WPSDocument::isFileFormatSupported(&gsfInput, kind, creator, needEncoding);
	
	if (kind != libwps::WPS_TEXT)
		confidence = libwps::WPS_CONFIDENCE_NONE;

	switch (confidence)
	{
		case libwps::WPS_CONFIDENCE_NONE:
			return UT_CONFIDENCE_ZILCH;
		case libwps::WPS_CONFIDENCE_EXCELLENT:
			return UT_CONFIDENCE_PERFECT;
		default:
			return UT_CONFIDENCE_ZILCH;
	}
}

UT_Error IE_Imp_MSWorks_Sniffer::constructImporter (PD_Document * pDocument,
							IE_Imp ** ppie)
{
	*ppie = new IE_Imp_MSWorks(pDocument);
	return UT_OK;
}

bool IE_Imp_MSWorks_Sniffer::getDlgLabels  (const char ** pszDesc,
						const char ** pszSuffixList,
						IEFileType * ft)
{
	*pszDesc = "Microsoft Works (.wps)";
	*pszSuffixList = "*.wps";
	*ft = getFileType();
	return true;
}

#endif
