/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
/* Abinova
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

/*
 * TST04 — deterministic structural asserts on generated content.
 * Instead of comparing pixels, these mains walk the piece table after
 * each generator runs and count what it emitted: cover presets must
 * land their declared frame stack (frame-type / shape-path /
 * strux-image-dataid attributes included) inside exactly one
 * "_cover-page" marker, image presets must register their bundled art
 * as document data items, a TOC must be a SectionTOC/EndTOC strux pair
 * carrying the preset's toc-* props, and header/footer gallery presets
 * must emit their page_number/page_count field objects inside the
 * hdrftr section.  A dropped or misgenerated shape therefore fails a
 * count, not an eyeball.
 */

#include "tf_test.h"

#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "pf_Fragments.h"
#include "pf_Frag_Strux.h"
#include "pf_Frag_Object.h"
#include "pp_AttrProp.h"
#include "fl_DocLayout.h"
#include "fv_View.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "ut_growbuf.h"

#include <cstring>
#include <string>
#include <vector>

#define TFSUITE "core.text.fmt.structcontent"

namespace {

struct GenView
{
	GenView() = default;
	GenView(const GenView &) = delete;
	GenView &operator=(const GenView &) = delete;

	/* scratch doc: '\n' separates blocks; "H:..." lines get the
	 * Heading 1 style so TOCs have entries to collect */
	bool load(const char *text)
	{
		doc = new PD_Document;
		if (doc->createRawDocument() != UT_OK)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		pt_PieceTable * pt = doc->getPieceTable();
		bool ok = pt->appendStrux(PTX_Section, PP_NOPROPS);
		const char *p = text;
		while (p)
		{
			const char *nl = strchr(p, '\n');
			const PP_PropertyVector * atts = &PP_NOPROPS;
			PP_PropertyVector headingAtts;
			const char * start = p;
			if (p[0] == 'H' && p[1] == ':')
			{
				headingAtts = { "style", "Heading 1" };
				atts = &headingAtts;
				start = p + 2;
			}
			ok = ok && pt->appendStrux(PTX_Block, *atts);
			UT_UCS4String s(start, nl ? static_cast<size_t>(nl - start)
								  : strlen(start));
			if (s.length())
			{
				ok = ok && pt->appendSpan(s.ucs4_str(),
							static_cast<UT_uint32>(s.length()));
			}
			p = nl ? nl + 1 : nullptr;
		}
		if (!ok)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		doc->finishRawCreation();
		GR_UnixCairoAllocInfo ai(nullptr);
		graphics = XAP_App::getApp()->newGraphics(GRID_UNIX_PANGO, ai);
		if (!graphics)
			return false;
		layout = new FL_DocLayout(doc, graphics);
		view = new FV_View(XAP_App::getApp(), nullptr, layout);
		layout->fillLayouts();
		layout->formatAll();
		view->setWindowSize(800, 600);
		return layout->countPages() > 0;
	}

	~GenView()
	{
		delete view;
		delete layout;
		delete graphics;
		if (doc)
			doc->unref();
	}

	PD_Document *doc = nullptr;
	GR_Graphics *graphics = nullptr;
	FL_DocLayout *layout = nullptr;
	FV_View *view = nullptr;
};

std::string gen_doc_text(FV_View * v)
{
	UT_GrowBuf buf;
	v->getTextInDocument(buf);
	std::string out;
	out.reserve(buf.getLength());
	for (UT_uint32 i = 0; i < buf.getLength(); ++i)
	{
		UT_UCS4Char c = static_cast<UT_UCS4Char>(buf.getPointer(i)[0]);
		if (c < 0x80)
			out += static_cast<char>(c);
	}
	return out;
}

/* Piece-table census of generated content.  Frames, cover markers,
 * fields and section struxes are counted with the attributes the
 * generators stamp on them; frames/fields are also bucketed by the
 * region they sit in (inside the _cover-page marker, inside a hdrftr
 * section) so content escaping its wrapper is caught too. */
struct StructStats
{
	int frames = 0, endFrames = 0, frameBlocks = 0;
	int imgFrames = 0, txtFrames = 0, pathFrames = 0;
	int framesWithText = 0;
	int framesInMarker = 0, framesOutMarker = 0;
	int bmCoverStart = 0, bmCoverEnd = 0;
	int hdrftr = 0;
	int pageNumFields = 0, pageCountFields = 0;
	int hdrftrPageNum = 0, hdrftrPageCount = 0;
	int toc = 0, endToc = 0;
	std::vector<std::string> imgDataIds;
	std::vector<std::string> shapePaths;
	std::vector<std::string> hdrftrTypes;
};

void stat_walk(PD_Document * doc, StructStats & st)
{
	pt_PieceTable * pt = doc->getPieceTable();
	int inFrame = 0, inCover = 0, inHdrFtr = 0;
	bool frameHasText = false;
	for (pf_Frag * pf = pt->getFragments().getFirst(); pf;
		 pf = pf->getNext())
	{
		const PP_AttrProp * ap = nullptr;
		switch (pf->getType())
		{
		case pf_Frag::PFT_Strux:
		{
			pf_Frag_Strux * pfs = static_cast<pf_Frag_Strux *>(pf);
			pt->getAttrProp(pf->getIndexAP(), &ap);
			const gchar * v = nullptr;
			switch (pfs->getStruxType())
			{
			case PTX_Section:
				inHdrFtr = 0;
				break;
			case PTX_SectionFrame:
				st.frames++;
				inFrame++;
				frameHasText = false;
				if (inCover)
					st.framesInMarker++;
				else
					st.framesOutMarker++;
				if (ap && ap->getProperty("frame-type", v) && v)
				{
					if (!strcmp(v, "image"))
						st.imgFrames++;
					else if (!strcmp(v, "textbox"))
						st.txtFrames++;
				}
				if (ap && ap->getProperty("shape-path", v) && v)
				{
					st.pathFrames++;
					st.shapePaths.push_back(v);
				}
				v = nullptr;
				if (ap && ap->getAttribute("strux-image-dataid", v) && v)
					st.imgDataIds.push_back(v);
				break;
			case PTX_EndFrame:
				st.endFrames++;
				if (inFrame > 0)
					inFrame--;
				if (frameHasText)
					st.framesWithText++;
				break;
			case PTX_Block:
				if (inFrame)
					st.frameBlocks++;
				break;
			case PTX_SectionHdrFtr:
				st.hdrftr++;
				inHdrFtr = 1;
				if (ap && ap->getAttribute("type", v) && v)
					st.hdrftrTypes.push_back(v);
				break;
			case PTX_SectionTOC:
				st.toc++;
				inHdrFtr = 0;
				break;
			case PTX_EndTOC:
				st.endToc++;
				break;
			default:
				break;
			}
			break;
		}
		case pf_Frag::PFT_Object:
		{
			pf_Frag_Object * pfo = static_cast<pf_Frag_Object *>(pf);
			pt->getAttrProp(pf->getIndexAP(), &ap);
			const gchar * v = nullptr;
			if (pfo->getObjectType() == PTO_Bookmark)
			{
				if (ap && ap->getAttribute("name", v) && v &&
					!strcmp(v, "_cover-page"))
				{
					v = nullptr;
					if (ap->getAttribute("type", v) && v &&
						!strcmp(v, "start"))
					{
						st.bmCoverStart++;
						inCover++;
					}
					else
					{
						st.bmCoverEnd++;
						if (inCover > 0)
							inCover--;
					}
				}
			}
			else if (pfo->getObjectType() == PTO_Field)
			{
				if (ap && ap->getAttribute("type", v) && v)
				{
					if (!strcmp(v, "page_number"))
					{
						st.pageNumFields++;
						if (inHdrFtr)
							st.hdrftrPageNum++;
					}
					else if (!strcmp(v, "page_count"))
					{
						st.pageCountFields++;
						if (inHdrFtr)
							st.hdrftrPageCount++;
					}
				}
			}
			break;
		}
		case pf_Frag::PFT_Text:
			if (inFrame && pf->getLength())
				frameHasText = true;
			break;
		default:
			break;
		}
	}
}

/* Expected cover shape stack, taken from the FV_CoverShape tables in
 * fv_View_cmd.cpp: every preset's frames, how many are picture frames,
 * how many carry a custGeom shape-path, how many block struxes the
 * frame contents should total, and which bundled art data items must
 * be registered. */
struct CoverExpect
{
	const char * szId;
	int frames;
	int imgFrames;
	int pathFrames;
	int frameBlocks;
	const char * szAssets; /* comma-separated data ids, "" for none */
};

const CoverExpect s_coverExpect[] =
{
	{ "frame",       0, 0, 0, 0, "" },
	{ "motion",      0, 0, 0, 0, "" },
	{ "sideline",    0, 0, 0, 0, "" },
	{ "yearly",      0, 0, 0, 0, "" },
	{ "austin",      6, 0, 0, 7, "" },
	{ "badge",       5, 0, 1, 7, "" },
	{ "banded",      3, 0, 0, 4, "" },
	{ "crop",        5, 0, 0, 7, "" },
	{ "facet",       5, 1, 1, 8, "cover-facet-band" },
	{ "feathered",   6, 1, 0, 8, "cover-feathers" },
	{ "filgree",     3, 2, 0, 5, "cover-filgree,cover-filgree-small" },
	{ "headiness",   3, 0, 0, 5, "" },
	{ "integral",    2, 1, 0, 5, "cover-integral" },
	{ "ion-dark",    6, 0, 2, 7, "" },
	{ "ion-light",   2, 0, 0, 4, "" },
	{ "retrospect",  3, 0, 0, 5, "" },
	{ "semaphore",   5, 0, 0, 8, "" },
	{ "slice-dark",  3, 0, 1, 4, "" },
	{ "slice-light", 3, 0, 1, 5, "" },
	{ "viewmaster",  4, 0, 0, 6, "" },
	{ "whip",        6, 0, 3, 8, "" },
};

bool cover_struct_ok(PD_Document * doc, const CoverExpect & e)
{
	StructStats st;
	stat_walk(doc, st);
	if (st.frames != e.frames || st.endFrames != e.frames)
		return false;
	if (st.imgFrames != e.imgFrames || st.pathFrames != e.pathFrames)
		return false;
	if (st.frameBlocks != e.frameBlocks)
		return false;
	if (st.bmCoverStart != 1 || st.bmCoverEnd != 1)
		return false;
	/* every generated frame must sit inside the marker range */
	if (st.framesInMarker != e.frames || st.framesOutMarker != 0)
		return false;
	/* each declared asset must be both referenced by a frame and
	 * registered as a document data item */
	std::string assets(e.szAssets);
	size_t pos = 0;
	while (pos <= assets.size())
	{
		size_t comma = assets.find(',', pos);
		std::string id = assets.substr(pos, comma == std::string::npos
									  ? std::string::npos : comma - pos);
		if (!id.empty())
		{
			bool referenced = false;
			for (const std::string & s : st.imgDataIds)
				if (s == id)
					referenced = true;
			if (!referenced)
				return false;
			UT_ConstByteBufPtr bb;
			std::string mime;
			if (!doc->getDataItemDataByName(id.c_str(), bb, &mime,
											nullptr) || !bb ||
				!bb->getLength() || mime.empty())
				return false;
		}
		if (comma == std::string::npos)
			break;
		pos = comma + 1;
	}
	return true;
}

/* { preset id, expected page_number fields, expected page_count fields }
 * — the \x01/\x02 markers baked into each FV_HdrLine table. */
struct HdrExpect
{
	const char * szId;
	int pageNum;
	int pageCount;
};

const HdrExpect s_hdrExpect[] =
{
	{ "blank", 0, 0 }, { "blank3", 0, 0 }, { "austin", 0, 0 },
	{ "badge", 0, 0 }, { "banded", 0, 0 }, { "crop", 1, 0 },
	{ "faceteven", 1, 0 }, { "facetodd", 1, 0 }, { "feathered", 0, 0 },
	{ "feathered2", 0, 0 }, { "filigree", 0, 0 }, { "headlines", 1, 0 },
	{ "integral", 0, 0 }, { "iondark", 1, 0 }, { "ionlight", 1, 0 },
	{ "retrospect", 0, 0 }, { "semaphore", 0, 0 }, { "slice1", 1, 0 },
	{ "slice2", 1, 0 }, { "viewmaster", 0, 0 }, { "whisp", 0, 0 },
};

const HdrExpect s_ftrExpect[] =
{
	{ "blank", 0, 0 }, { "blank3", 0, 0 }, { "austin", 1, 0 },
	{ "badge", 1, 0 }, { "banded", 1, 0 }, { "crop", 0, 0 },
	{ "faceteven", 0, 0 }, { "facetodd", 0, 0 }, { "feathered", 1, 0 },
	{ "filigree", 0, 0 }, { "headlines", 0, 0 }, { "integral", 1, 0 },
	{ "iondark", 0, 0 }, { "ionlight", 0, 0 }, { "retrospect", 1, 0 },
	{ "semaphore", 1, 1 }, { "slice", 0, 0 }, { "viewmasterh", 1, 0 },
	{ "viewmasterv", 1, 0 }, { "whisp", 1, 0 },
};

}

TFTEST_MAIN("cover presets emit their declared shape structure")
{
	for (const CoverExpect & e : s_coverExpect)
	{
		GenView hv;
		TFPASS(hv.load("cover host text"));
		if (!hv.view)
			continue;
		FV_View * v = hv.view;

		v->setPoint(2);
		TFPASS(v->cmdInsertCoverPage(e.szId) == UT_OK);
		TFPASS(v->hasCoverPage());

		StructStats st;
		stat_walk(hv.doc, st);
		TFPASSEQ(st.frames, e.frames);
		TFPASSEQ(st.endFrames, e.frames);
		TFPASSEQ(st.imgFrames, e.imgFrames);
		TFPASSEQ(st.pathFrames, e.pathFrames);
		TFPASSEQ(st.frameBlocks, e.frameBlocks);
		/* exactly one marker, start+end */
		TFPASSEQ(st.bmCoverStart, 1);
		TFPASSEQ(st.bmCoverEnd, 1);
		/* all frames inside the marker, none outside */
		TFPASSEQ(st.framesInMarker, e.frames);
		TFPASSEQ(st.framesOutMarker, 0);
		/* every frame carries at least one block — a frameless
		 * section-frame breaks the surrounding frame chain */
		if (e.frames)
		{
			TFPASS(st.frameBlocks >= e.frames);
			TFPASSEQ(st.txtFrames + st.imgFrames, e.frames);
		}
		std::string assets(e.szAssets);
		size_t pos = 0;
		while (pos <= assets.size())
		{
			size_t comma = assets.find(',', pos);
			std::string id = assets.substr(pos,
				comma == std::string::npos ? std::string::npos
										 : comma - pos);
			if (!id.empty())
			{
				bool referenced = false;
				for (const std::string & s : st.imgDataIds)
					if (s == id)
						referenced = true;
				TFPASS(referenced);
				UT_ConstByteBufPtr bb;
				std::string mime;
				TFPASS(hv.doc->getDataItemDataByName(id.c_str(), bb,
												   &mime, nullptr));
				TFPASS(bb && bb->getLength() > 0);
				TFPASS(!mime.empty());
			}
			if (comma == std::string::npos)
				break;
			pos = comma + 1;
		}
	}
}

TFTEST_MAIN("badge cover emits seal shape plus two text frames")
{
	GenView hv;
	TFPASS(hv.load("cover host text"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(2);
	TFPASS(v->cmdInsertCoverPage("badge") == UT_OK);

	StructStats st;
	stat_walk(hv.doc, st);
	TFPASSEQ(st.frames, 5);
	/* the scalloped seal is the one custGeom frame, and its path
	 * data is the measured badge seal (M 1000.0 500.4 …) */
	TFPASSEQ(st.pathFrames, 1);
	TFPASSEQ(static_cast<int>(st.shapePaths.size()), 1);
	if (!st.shapePaths.empty())
	{
		TFPASS(st.shapePaths[0].compare(0, 13, "M 1000.0 500.") == 0);
	}
	/* title + meta textboxes carry text; the three decoration
	 * rects are block-only shells */
	TFPASSEQ(st.framesWithText, 2);
	std::string text = gen_doc_text(v);
	TFPASS(text.find("Document Title") != std::string::npos);
	TFPASS(text.find("Document Subtitle") != std::string::npos);
}

TFTEST_MAIN("facet cover emits blue base band plus overlay image")
{
	GenView hv;
	TFPASS(hv.load("cover host text"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(2);
	TFPASS(v->cmdInsertCoverPage("facet") == UT_OK);

	StructStats st;
	stat_walk(hv.doc, st);
	/* base band: a solid deco shape under the translucent overlay */
	TFPASSEQ(st.pathFrames, 1);
	if (!st.shapePaths.empty())
	{
		TFPASS(st.shapePaths[0].find("L 1000 0") !=
			   std::string::npos);
	}
	/* overlay art: one picture frame bound to the registered
	 * cover-facet-band data item */
	TFPASSEQ(st.imgFrames, 1);
	TFPASSEQ(static_cast<int>(st.imgDataIds.size()), 1);
	if (!st.imgDataIds.empty())
	{
		TFPASS(st.imgDataIds[0] == "cover-facet-band");
	}
	UT_ConstByteBufPtr bb;
	std::string mime;
	TFPASS(hv.doc->getDataItemDataByName("cover-facet-band", bb,
									   &mime, nullptr));
	TFPASS(bb && bb->getLength() > 100);
	TFPASS(!mime.empty());
}

TFTEST_MAIN("cover replace keeps one marker and swaps the stack")
{
	GenView hv;
	TFPASS(hv.load("cover host text"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(2);
	TFPASS(v->cmdInsertCoverPage("badge") == UT_OK);
	TFPASS(v->cmdInsertCoverPage("crop") == UT_OK);

	StructStats st;
	stat_walk(hv.doc, st);
	/* replace, not stack: still exactly one marker pair and the
	 * new preset's frame count — the badge seal is gone */
	TFPASSEQ(st.bmCoverStart, 1);
	TFPASSEQ(st.bmCoverEnd, 1);
	TFPASSEQ(st.frames, 5);
	TFPASSEQ(st.framesInMarker, 5);
	TFPASSEQ(st.framesOutMarker, 0);
	TFPASSEQ(st.pathFrames, 0);
	TFPASS(gen_doc_text(v).find("cover host text") !=
		   std::string::npos);
}

TFTEST_MAIN("TOC insert emits strux pair and preset props")
{
	GenView hv;
	TFPASS(hv.load("H:First Chapter\nbody text\nH:Second Chapter"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	{
		PT_DocPosition posEnd = 0;
		hv.doc->getBounds(true, posEnd);
		v->setPoint(posEnd);
	}
	TFPASS(v->cmdInsertTOC() == UT_OK);

	StructStats st;
	stat_walk(hv.doc, st);
	/* the TOC is exactly one strux pair — SectionTOC immediately
	 * followed by EndTOC */
	TFPASSEQ(st.toc, 1);
	TFPASSEQ(st.endToc, 1);
	PT_DocPosition posEod = 0;
	hv.doc->getBounds(true, posEod);
	const pf_Frag_Strux * sdhTOC = nullptr;
	TFPASS(hv.doc->getStruxOfTypeFromPosition(posEod - 1,
			PTX_SectionTOC, &sdhTOC) && sdhTOC);
	TFPASS(hv.layout->getNumTOCs() >= 1);

	/* a styled insert writes the preset's toc-* props onto the
	 * strux — the "field markers" Word carries in fldChar */
	{
		GenView hv2;
		TFPASS(hv2.load("H:Chapter One\nbody"));
		if (hv2.view)
		{
			hv2.view->setPoint(2);
			TFPASS(hv2.view->cmdInsertTOCStyled("classic") == UT_OK);
			sdhTOC = nullptr;
			PT_DocPosition posEnd2 = 0;
			hv2.doc->getBounds(true, posEnd2);
			TFPASS(hv2.doc->getStruxOfTypeFromPosition(posEnd2 - 1,
					PTX_SectionTOC, &sdhTOC) && sdhTOC);
			if (sdhTOC)
			{
				const PP_AttrProp * ap = nullptr;
				TFPASS(hv2.doc->getPieceTable()->getAttrProp(
						sdhTOC->getIndexAP(), &ap) && ap);
				const gchar * val = nullptr;
				TFPASS(ap && ap->getProperty("toc-heading", val) &&
					   val && !strcmp(val, "Contents"));
				val = nullptr;
				TFPASS(ap && ap->getProperty("toc-indent1", val) &&
					   val && !strcmp(val, "0in"));
				val = nullptr;
				TFPASS(ap && ap->getProperty("toc-tab-leader1", val) &&
					   val && !strcmp(val, "dot"));
			}
		}
	}

	/* the manual table emits literal Contents paragraphs instead
	 * of a TOC strux */
	{
		GenView hv3;
		TFPASS(hv3.load("plain text"));
		if (hv3.view)
		{
			hv3.view->setPoint(2);
			TFPASS(hv3.view->cmdInsertTOCManual("formal") == UT_OK);
			std::string text = gen_doc_text(hv3.view);
			TFPASS(text.find("Table of Contents") !=
				   std::string::npos);
			TFPASS(text.find("Type chapter title (level 4)") !=
				   std::string::npos);
			StructStats st3;
			stat_walk(hv3.doc, st3);
			TFPASSEQ(st3.toc, 0);
		}
	}
}

TFTEST_MAIN("header and footer presets emit their declared fields")
{
	for (const HdrExpect & e : s_hdrExpect)
	{
		GenView hv;
		TFPASS(hv.load("hdrftr host text"));
		if (!hv.view)
			continue;
		TFPASS(hv.view->cmdInsertHeaderPreset(e.szId,
				FL_HDRFTR_HEADER) == UT_OK);
		StructStats st;
		stat_walk(hv.doc, st);
		TFPASSEQ(st.hdrftr, 1);
		TFPASSEQ(st.hdrftrPageNum, e.pageNum);
		TFPASSEQ(st.hdrftrPageCount, e.pageCount);
		/* no field may leak outside the hdrftr region */
		TFPASSEQ(st.pageNumFields, e.pageNum);
		TFPASSEQ(st.pageCountFields, e.pageCount);
		if (!st.hdrftrTypes.empty())
		{
			TFPASS(st.hdrftrTypes[0] == "header");
		}
	}
	for (const HdrExpect & e : s_ftrExpect)
	{
		GenView hv;
		TFPASS(hv.load("hdrftr host text"));
		if (!hv.view)
			continue;
		TFPASS(hv.view->cmdInsertHeaderPreset(e.szId,
				FL_HDRFTR_FOOTER) == UT_OK);
		StructStats st;
		stat_walk(hv.doc, st);
		TFPASSEQ(st.hdrftr, 1);
		TFPASSEQ(st.hdrftrPageNum, e.pageNum);
		TFPASSEQ(st.hdrftrPageCount, e.pageCount);
		TFPASSEQ(st.pageNumFields, e.pageNum);
		TFPASSEQ(st.pageCountFields, e.pageCount);
		if (!st.hdrftrTypes.empty())
		{
			TFPASS(st.hdrftrTypes[0] == "footer");
		}
	}
	/* both directions together: a doc can hold header + footer at
	 * once, each keeping its own fields */
	{
		GenView hv;
		TFPASS(hv.load("hdrftr host text"));
		if (hv.view)
		{
			TFPASS(hv.view->cmdInsertHeaderPreset("crop",
					FL_HDRFTR_HEADER) == UT_OK);
			TFPASS(hv.view->cmdInsertHeaderPreset("semaphore",
					FL_HDRFTR_FOOTER) == UT_OK);
			StructStats st;
			stat_walk(hv.doc, st);
			TFPASSEQ(st.hdrftr, 2);
			TFPASSEQ(st.hdrftrPageNum, 2);
			TFPASSEQ(st.hdrftrPageCount, 1);
		}
	}
}

TFTEST_MAIN("structural census catches a dropped shape")
{
	GenView hv;
	TFPASS(hv.load("cover host text"));
	if (!hv.view)
		return;
	FV_View * v = hv.view;

	v->setPoint(2);
	TFPASS(v->cmdInsertCoverPage("badge") == UT_OK);

	/* sanity: the census passes on the intact cover */
	for (const CoverExpect & e : s_coverExpect)
	{
		if (!strcmp(e.szId, "badge"))
		{
			TFPASS(cover_struct_ok(hv.doc, e));
			break;
		}
	}

	/* drop one frame strux straight out of the piece table —
	 * whatever it leaves behind can no longer pass the census */
	pf_Frag_Strux * victim = nullptr;
	{
		pt_PieceTable * pt = hv.doc->getPieceTable();
		for (pf_Frag * pf = pt->getFragments().getFirst(); pf;
			 pf = pf->getNext())
		{
			if (pf->getType() == pf_Frag::PFT_Strux &&
				static_cast<pf_Frag_Strux *>(pf)->getStruxType()
					== PTX_SectionFrame)
			{
				victim = static_cast<pf_Frag_Strux *>(pf);
				break;
			}
		}
	}
	TFPASS(victim != nullptr);
	if (victim)
	{
		PT_DocPosition pos = hv.doc->getStruxPosition(victim);
		TFPASS(hv.doc->deleteStrux(pos, PTX_SectionFrame, true));
	}
	StructStats st;
	stat_walk(hv.doc, st);
	/* either the frame count or the frame/endframe balance must
	 * have moved — a silent drop is impossible */
	bool detected = (st.frames != 5) || (st.endFrames != 5) ||
		(st.framesInMarker != st.frames) || (st.frameBlocks != 7);
	TFPASS(detected);
	for (const CoverExpect & e : s_coverExpect)
	{
		if (!strcmp(e.szId, "badge"))
		{
			TFPASS(!cover_struct_ok(hv.doc, e));
			break;
		}
	}

	/* the census also discriminates presets: facet expectations
	 * must not pass against a badge document */
	{
		GenView hv2;
		TFPASS(hv2.load("cover host text"));
		if (hv2.view)
		{
			hv2.view->setPoint(2);
			TFPASS(hv2.view->cmdInsertCoverPage("badge") == UT_OK);
			for (const CoverExpect & e : s_coverExpect)
			{
				if (!strcmp(e.szId, "facet"))
				{
					TFPASS(!cover_struct_ok(hv2.doc, e));
					break;
				}
			}
		}
	}
}
