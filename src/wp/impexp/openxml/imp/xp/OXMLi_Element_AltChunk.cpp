/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource
 *
 * Copyright (C) 2026 Abinova contributors
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
#include "OXMLi_Element_AltChunk.h"

// Internal includes
#include "OXMLi_PackageManager.h"
#include "OXML_Document.h"

// Abinova includes
#include "ie_imp_XHTML.h"
#include "ie_types.h"
#include "pd_Document.h"
#include "pt_Types.h"
#include "xap_App.h"
#include "xap_Prefs.h"
#include "ut_types.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ut_go_file.h"
#include "pl_Listener.h"
#include "pp_AttrProp.h"
#include "pf_Frag_Strux.h"
#include "px_CR_Span.h"
#include "px_CR_Object.h"
#include "px_CR_Strux.h"

// External includes
#include <algorithm>
#include <string>
#include <vector>

namespace {

std::string s_percentDecode(const std::string& s)
{
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); i++)
	{
		if (s[i] == '%' && i + 2 < s.size() && isxdigit(
				static_cast<unsigned char>(s[i + 1])) && isxdigit(
				static_cast<unsigned char>(s[i + 2])))
		{
			auto hexval = [](char c) -> int {
				if (c >= '0' && c <= '9') return c - '0';
				if (c >= 'a' && c <= 'f') return c - 'a' + 10;
				return c - 'A' + 10;
			};
			out += static_cast<char>((hexval(s[i + 1]) << 4)
					| hexval(s[i + 2]));
			i += 2;
		}
		else
		{
			out += s[i];
		}
	}
	return out;
}

/* Lexically normalize a '/'-separated relative path: collapse empty
 * and "." components and resolve ".." against the accumulated path.
 * Returns false if the path escapes its root (more ".." than leading
 * components) - such paths must never be used for zip lookup.
 */
bool s_normalizePath(const std::string& path, std::string& out)
{
	std::vector<std::string> stack;
	size_t pos = 0;

	while (pos <= path.size())
	{
		size_t slash = path.find('/', pos);
		std::string comp =
				(slash == std::string::npos) ?
					path.substr(pos) : path.substr(pos, slash - pos);
		pos = (slash == std::string::npos) ? path.size() + 1 : slash + 1;

		if (comp.empty() || comp == ".")
		{
			continue;
		}
		if (comp == "..")
		{
			if (stack.empty())
			{
				return false;
			}
			stack.pop_back();
			continue;
		}
		stack.push_back(comp);
	}

	out.clear();
	for (std::vector<std::string>::const_iterator i = stack.begin(); i
			!= stack.end(); i++)
	{
		if (!out.empty())
		{
			out += '/';
		}
		out += *i;
	}

	return true;
}

/* Resolves chunk-relative resource hrefs (<img src>, <link
 * rel=stylesheet href>) to sibling members of the OOXML package.
 * Word writes altChunk resources as parts next to the chunk itself
 * (e.g. word/image1.png for word/chunk1.html), so plain relative
 * hrefs join against the chunk's directory.  Schemes, root-absolute
 * paths and escapes resolve to nothing.
 */
class OXMLi_ChunkResourceProvider : public IE_Imp_XHTML_ResourceProvider
{
public:
	OXMLi_ChunkResourceProvider(OXMLi_PackageManager* mgr,
								const std::string& partPath) :
		m_mgr(mgr),
		m_baseDir()
	{
		size_t slash = partPath.find_last_of('/');
		if (slash != std::string::npos)
		{
			m_baseDir = partPath.substr(0, slash);
		}
	}

	virtual GsfInput* openResource(const char* href) override
	{
		if (href == nullptr || *href == 0 || m_mgr == nullptr)
		{
			return nullptr;
		}
		std::string h(href);
		/* fragment and query never name package members */
		size_t frag = h.find_first_of("#?");
		if (frag != std::string::npos)
		{
			h.resize(frag);
		}
		if (h.empty() || h.find(':') != std::string::npos
				|| h[0] == '/' || h[0] == '\\')
		{
			return nullptr;
		}

		std::string joined = m_baseDir.empty() ? s_percentDecode(h)
				: m_baseDir + "/" + s_percentDecode(h);
		std::string zipPath;
		if (!s_normalizePath(joined, zipPath) || zipPath.empty())
		{
			return nullptr;
		}

		GsfInput* input = m_mgr->openPartByPath(zipPath);
		if (input != nullptr)
		{
			gsf_input_seek(input, 0, G_SEEK_SET);
		}
		return input;
	}

private:
	OXMLi_PackageManager* m_mgr;
	std::string m_baseDir;
};

/* Serializes a source document's AttrProp back into the flat vector
 * the load-time append API takes: plain attributes, then the
 * properties packed under the "props" pseudo-attribute.
 */
void s_attsWithProps(const PP_AttrProp* pAP, PP_PropertyVector& out)
{
	out = pAP->getAttributes();
	const PP_PropertyVector & props = pAP->getProperties();
	if (!props.empty())
	{
		std::string s;
		for (PP_PropertyVector::const_iterator i = props.begin(); i
				!= props.end(); i += 2)
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
}

/* Loading-time counterpart to IE_Imp_PasteListener: walks a scratch
 * document via tellListener() and replays its fragments into the
 * destination with the append*() calls importers use.  Needed because
 * PD_Document::insertSpan()/insertStrux() refuse to run while the
 * piece table is still PTS_Loading - which is exactly when the outer
 * docx import's addToPT() runs.
 *
 * The first source block merges into the block already emitted at the
 * insertion point (the altChunk placeholder paragraph); its AP is
 * adopted, with keepProps (the altchunk-path/-format round-trip
 * properties) folded back in.  The first source section is skipped -
 * the destination already sits inside one.
 */
class OXMLi_AppendListener : public PL_Listener
{
public:
	OXMLi_AppendListener(PD_Document* pDest, PD_Document* pSrc,
						pf_Frag_Strux* pFirstBlock,
						const std::string& keepProps) :
		m_pDest(pDest),
		m_pSrc(pSrc),
		m_pFirstBlock(pFirstBlock),
		m_keepProps(keepProps),
		m_bFirstSection(true),
		m_bFirstBlock(true)
	{
	}

	virtual bool populate(fl_ContainerLayout* /*sfh*/,
						  const PX_ChangeRecord* pcr) override
	{
		const PP_AttrProp* pAP = nullptr;
		PP_PropertyVector atts;
		if (m_pSrc->getAttrProp(pcr->getIndexAP(), &pAP) && pAP)
		{
			s_attsWithProps(pAP, atts);
		}

		switch (pcr->getType())
		{
		case PX_ChangeRecord::PXT_InsertSpan:
		{
			const PX_ChangeRecord_Span* pcrs =
					static_cast<const PX_ChangeRecord_Span*>(pcr);
			const UT_UCS4Char* pChars =
					m_pSrc->getPointer(pcrs->getBufIndex());
			if (!pChars)
				return false;
			return m_pDest->appendFmt(atts)
					&& m_pDest->appendSpan(pChars, pcrs->getLength());
		}
		case PX_ChangeRecord::PXT_InsertObject:
			return m_pDest->appendObject(
					static_cast<const PX_ChangeRecord_Object*>(pcr)->getObjectType(),
					atts);
		case PX_ChangeRecord::PXT_InsertFmtMark:
			return m_pDest->appendFmtMark();
		default:
			return true;
		}
	}

	virtual bool populateStrux(pf_Frag_Strux* /*sdh*/,
							   const PX_ChangeRecord* pcr,
							   fl_ContainerLayout** /*psfh*/) override
	{
		const PX_ChangeRecord_Strux* pcrx =
				static_cast<const PX_ChangeRecord_Strux*>(pcr);
		const PP_AttrProp* pAP = nullptr;
		PP_PropertyVector atts;
		if (m_pSrc->getAttrProp(pcr->getIndexAP(), &pAP) && pAP)
		{
			s_attsWithProps(pAP, atts);
		}

		switch (pcrx->getStruxType())
		{
		case PTX_Section:
			if (m_bFirstSection)
			{
				m_bFirstSection = false;
				return true;
			}
			break;
		case PTX_Block:
			if (m_bFirstBlock)
			{
				m_bFirstBlock = false;
				if (m_pFirstBlock && !atts.empty())
				{
					if (!m_keepProps.empty())
					{
						/* fold the altchunk-* props into the adopted
						 * props so the link survives on the merged
						 * paragraph */
						for (PP_PropertyVector::iterator i = atts.begin();
								i != atts.end(); ++i)
						{
							if (*i == PT_PROPS_ATTRIBUTE_NAME && (i + 1)
									!= atts.end())
							{
								*(i + 1) += ";" + m_keepProps;
								break;
							}
						}
						if (std::find(atts.begin(), atts.end(),
									  PT_PROPS_ATTRIBUTE_NAME) == atts.end())
						{
							atts.push_back(PT_PROPS_ATTRIBUTE_NAME);
							atts.push_back(m_keepProps);
						}
					}
					m_pDest->appendStruxFmt(m_pFirstBlock, atts);
				}
				return true;
			}
			break;
		default:
			break;
		}
		return m_pDest->appendStrux(pcrx->getStruxType(), atts);
	}

	virtual bool change(fl_ContainerLayout* /*sfh*/,
						const PX_ChangeRecord* /*pcr*/) override
	{
		return true;
	}

	virtual bool insertStrux(fl_ContainerLayout* /*sfh*/,
							 const PX_ChangeRecord* /*pcr*/,
							 pf_Frag_Strux* /*sdhNew*/,
							 PL_ListenerId /*lid*/,
							 void (* /*pfnBindHandles*/)(pf_Frag_Strux*,
									PL_ListenerId,
									fl_ContainerLayout*)) override
	{
		return true;
	}

	virtual bool signal(UT_uint32 /*iSignal*/) override
	{
		return true;
	}

private:
	PD_Document* m_pDest;
	PD_Document* m_pSrc;
	pf_Frag_Strux* m_pFirstBlock;
	std::string m_keepProps;
	bool m_bFirstSection;
	bool m_bFirstBlock;
};

} // anonymous namespace

OXMLi_Element_AltChunk::OXMLi_Element_AltChunk() :
	OXML_Element_Paragraph("")
{
}

OXMLi_Element_AltChunk::~OXMLi_Element_AltChunk()
{
}

UT_Error OXMLi_Element_AltChunk::addToPT(PD_Document * pDocument)
{
	UT_return_val_if_fail(pDocument != nullptr, UT_ERROR);

	/* the placeholder block carries the altchunk-path/-format props
	 * for .abwn round-trip; on a successful graft it also becomes the
	 * chunk's first block, on failure it is all that remains */
	UT_Error ret = OXML_Element_Paragraph::addToPT(pDocument);
	UT_return_val_if_fail(ret == UT_OK, ret);

	if (m_relId.empty())
		return UT_OK;

	_graftChunk(pDocument);
	return UT_OK;
}

/* Chunk formats ECMA-376 §17.17.2 defines (plus plain text, which
 * AbiWord's own file-open path also accepts).  Anything else -
 * .bin blobs, images, spreadhseet parts - is an unknown chunk type
 * and keeps the placeholder rather than letting the text sniffer
 * graft garbage.
 */
bool s_chunkTypeSupported(const std::string & partPath)
{
	std::string::size_type dot = partPath.rfind('.');
	if (dot == std::string::npos)
		return false;
	std::string ext = partPath.substr(dot + 1);
	for (std::string::iterator i = ext.begin(); i != ext.end(); ++i)
		*i = static_cast<char>(tolower(static_cast<unsigned char>(*i)));

	static const char * const types[] = {
		"html", "htm", "mht", "mhtml",
		"rtf", "txt", "xml",
		"doc", "docx", "docm", "dot", "dotx", "dotm"
	};
	for (size_t i = 0; i < G_N_ELEMENTS(types); i++)
	{
		if (ext == types[i])
			return true;
	}
	return false;
}

void OXMLi_Element_AltChunk::_graftChunk(PD_Document * pDocument)
{
	if (!s_chunkTypeSupported(m_partPath))
	{
		UT_DEBUGMSG(("altChunk %s is not a known subdocument type - "
					 "keeping placeholder\n", m_partPath.c_str()));
		return;
	}

	OXMLi_PackageManager * mgr = OXMLi_PackageManager::getInstance();
	if (!mgr)
		return;

	GsfInput * part = mgr->openPartByRelId(m_relId.c_str());
	if (!part)
	{
		UT_DEBUGMSG(("altChunk r:id %s resolves to no package part\n",
					 m_relId.c_str()));
		return;
	}

	/* materialize the member: the import machinery's sniffers need a
	 * seekable input, and a named memory stream gives suffix-based
	 * detection a real name too; the declared size is zip-controlled */
	gsf_off_t size = gsf_input_size(part);
	if (size <= 0 || size > UT_MAX_ARCHIVE_MEMBER_SIZE)
	{
		g_object_unref(G_OBJECT(part));
		return;
	}
	/* materialize the member: gsf_input_read with a NULL buffer may hand
	 * back the part's internal storage, so read into a buffer we own and
	 * can hand to the memory input outright */
	guint8 * copy = static_cast<guint8*>(g_malloc(size));
	gsf_input_seek(part, 0, G_SEEK_SET);
	const guint8 * data = gsf_input_read(part, size, copy);
	g_object_unref(G_OBJECT(part));
	if (!data)
	{
		g_free(copy);
		return;
	}

	std::string name = m_partPath.empty() ? "altchunk" : m_partPath;
	GsfInput * input = gsf_input_memory_new(copy,
										  static_cast<gsf_off_t>(size), TRUE);
	if (!input)
	{
		g_free(copy);
		return;
	}
	/* name it after the part so suffix-based import detection gets
	 * the real extension ("word/chunk1.html" -> ".html") */
	gsf_input_set_name(input, name.c_str());

	PD_Document * scratch = nullptr;
	try {
		scratch = new PD_Document();
	} catch(...) {
		scratch = nullptr;
	}
	if (!scratch)
	{
		g_object_unref(G_OBJECT(input));
		return;
	}

	/* A nested importer can own the same singletons we do - a docx
	 * chunk builds its own OXMLi_PackageManager + OXML_Document and
	 * its cleanup would destroy OUR data model mid-addToPT - so hand
	 * the outer instances out of the slots and take them back after. */
	OXML_Document * outerDoc = OXML_Document::detachInstance();
	OXMLi_PackageManager * outerMgr = OXMLi_PackageManager::detachInstance();

	/* chunk <img>/<link> hrefs resolve to sibling package parts */
	OXMLi_ChunkResourceProvider provider(mgr, m_partPath);
	IE_Imp_XHTML::setResourceProvider(&provider);

	if (XAP_App::getApp() && XAP_App::getApp()->getPrefs())
		XAP_App::getApp()->getPrefs()->setIgnoreNextRecent();
	UT_Error err = scratch->importFile(input, IEFT_Unknown,
									 true, false, NULL);

	IE_Imp_XHTML::setResourceProvider(nullptr);
	OXMLi_PackageManager::restoreInstance(outerMgr);
	OXML_Document::restoreInstance(outerDoc);
	g_object_unref(G_OBJECT(input));

	if (err != UT_OK)
	{
		UT_DEBUGMSG(("altChunk %s import failed (%d) - keeping "
					 "placeholder\n", name.c_str(),
					 static_cast<int>(err)));
		UNREFP(scratch);
		return;
	}
	scratch->finishRawCreation();

	/* images and other embedded payloads travel as data items; copy
	 * them across so chunk objects keep resolving (a name collision
	 * skips just that item - createDataItem never clobbers) */
	PD_DataItemHandle h = nullptr;
	std::string mimeType;
	const char * dataName = nullptr;
	UT_ConstByteBufPtr buf;
	for (UT_sint32 k = 0; scratch->enumDataItems(k, &h, &dataName, buf,
												 &mimeType); k++)
	{
		pDocument->createDataItem(dataName, false, buf, mimeType,
								  nullptr);
	}

	/* keep the altchunk-* props on the merged first block so the
	 * link round-trips through .abwn */
	std::string keepProps = "altchunk-path:" + m_partPath
			+ ";altchunk-format:";
	{
		std::string::size_type dot = m_partPath.rfind('.');
		if (dot != std::string::npos)
			keepProps += m_partPath.substr(dot + 1);
	}

	/* the placeholder block just emitted becomes the graft's first
	 * block: the chunk's opening paragraph folds into it */
	pf_Frag_Strux * pFirstBlock = pDocument->getLastStruxOfType(
			PTX_Block);
	OXMLi_AppendListener listener(pDocument, scratch, pFirstBlock,
								  keepProps);
	scratch->tellListener(static_cast<PL_Listener *>(&listener));

	UNREFP(scratch);
}
