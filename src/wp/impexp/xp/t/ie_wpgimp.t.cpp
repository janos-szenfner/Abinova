/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
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

/* COVD03 — pin the libwpg-based WordPerfect Graphics importer
 * (wpg/ie_impGraphic_WPG.cpp) through the public IE_ImpGraphic
 * registry.  Fixtures live in test/wp/wpgcov/ and come from the fuzz
 * corpus plus a zip whose PerfectOffice_MAIN member is a WPG stream —
 * that drives the importer's structured-container (subStream) path,
 * which is how a .wpg embedded in a PerfectOffice OLE package reads.
 */

#include "tf_test.h"
#include "ut_bytebuf.h"
#include "ie_impGraphic.h"
#include "ie_impGraphic_WPG.h"
#include "fg_Graphic.h"

#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-input-memory.h>
#include <glib.h>

#include <cstring>
#include <string>

#define TFSUITE "core.wp.impexp.wpgimp"

static GsfInput * open_fixture(const char * rel)
{
	std::string path = TF_Test::get_test_src_dir();
	path += "/";
	path += rel;
	return gsf_input_stdio_new(path.c_str(), nullptr);
}

static UT_ByteBuf * load_fixture(const char * rel)
{
	GsfInput * input = open_fixture(rel);
	if (!input)
		return nullptr;
	gsf_off_t sz = gsf_input_size(input);
	const guint8 * data = gsf_input_read(input, sz, nullptr);
	UT_ByteBuf * bb = nullptr;
	if (data)
	{
		bb = new UT_ByteBuf;
		bb->append(reinterpret_cast<const UT_Byte *>(data),
				   static_cast<UT_uint32>(sz));
	}
	g_object_unref(input);
	return bb;
}

/* does the .wpg suffix resolve to the libwpg importer? */
static bool wpg_sniffer_is_libwpg(IEGraphicFileType ft)
{
	const char * desc = nullptr, * suff = nullptr;
	IEGraphicFileType eft = IEGFT_Unknown;
	for (UT_uint32 i = 0;
		 IE_ImpGraphic::enumerateDlgLabels(i, &desc, &suff, &eft); i++) {
		if (eft == ft)
			return desc && strstr(desc, "WordPerfect");
	}
	return false;
}

TFTEST_MAIN("wpg suffix pins libwpg importer; converts WPG1 to SVG image")
{
	IEGraphicFileType ft = IE_ImpGraphic::fileTypeForSuffix(".wpg");
	TFPASS(ft != IEGFT_Unknown);
	TFPASS(wpg_sniffer_is_libwpg(ft));

	UT_ByteBuf * bb = load_fixture("test/wp/wpgcov/seed_wpg1_rect.wpg");
	TFPASS(bb != nullptr);
	if (!bb)
		return;

	IE_ImpGraphic * imp = nullptr;
	UT_ConstByteBufPtr cbb(bb);
	TFPASS(IE_ImpGraphic::constructImporter(cbb, ft, &imp) == UT_OK);
	TFPASS(imp != nullptr);
	if (!imp)
		return;

	FG_ConstGraphicPtr pfg;
	UT_Error err = imp->importGraphic(cbb, pfg);
	TFPASS(err == UT_OK);
	TFPASS(pfg != nullptr);
	if (pfg) {
		/* libwpg emits SVG which routes through the SVG importer */
		TFPASS(pfg->getBuffer() != nullptr);
		TFPASS(pfg->getBuffer()->getLength() > 0);
		TFPASS(pfg->getMimeType() == "image/svg+xml");
	}
	delete imp;
}

TFTEST_MAIN("wpg OLE container resolves PerfectOffice_MAIN substream")
{
	/* zip holding a WPG stream as PerfectOffice_MAIN — gsf treats it
	 * as structured, exercising isStructured + the subStream getters
	 * and getSubStreamByName */
	GsfInput * input = open_fixture("test/wp/wpgcov/cov_ole.wpg");
	TFPASS(input != nullptr);
	if (!input)
		return;

	IE_ImpGraphic * imp = nullptr;
	IEGraphicFileType ft = IE_ImpGraphic::fileTypeForSuffix(".wpg");
	TFPASS(IE_ImpGraphic::constructImporter(input, ft, &imp) == UT_OK);
	TFPASS(imp != nullptr);
	if (!imp)
	{
		g_object_unref(input);
		return;
	}

	FG_ConstGraphicPtr pfg;
	TFPASS(imp->importGraphic(input, pfg) == UT_OK);
	TFPASS(pfg != nullptr);

	g_object_unref(input);
	delete imp;
}

TFTEST_MAIN("wpg importer degrades gracefully on malformed input")
{
	IEGraphicFileType ft = IE_ImpGraphic::fileTypeForSuffix(".wpg");
	if (ft == IEGFT_Unknown)
		return;

	static const char * const edges[] = {
		"test/wp/wpgcov/edge_badmagic.wpg",
		"test/wp/wpgcov/edge_header_only.wpg",
		"test/wp/wpgcov/edge_garbage.wpg",
		"test/wp/wpgcov/seed_wpg1_group.wpg",
		"test/wp/wpgcov/seed_wpg2.wpg",
	};
	for (const char * rel : edges)
	{
		UT_ByteBuf * bb = load_fixture(rel);
		TFPASS(bb != nullptr);
		if (!bb)
			continue;
		UT_ConstByteBufPtr cbb(bb);
		IE_ImpGraphic * imp = nullptr;
		if (IE_ImpGraphic::constructImporter(cbb, ft, &imp) != UT_OK || !imp)
			continue;
		FG_ConstGraphicPtr pfg;
		/* parse attempt must not crash; valid seeds may import */
		imp->importGraphic(cbb, pfg);
		delete imp;
	}
	TFPASS(true);
}

TFTEST_MAIN("wpg contents sniffing exercises every graphic sniffer")
{
	UT_ByteBuf * bb = load_fixture("test/wp/wpgcov/seed_wpg1_rect.wpg");
	TFPASS(bb != nullptr);
	if (!bb)
		return;
	/* WPG magic round-trips through every registered sniffer's
	 * recognizeContents, including the libwpg one */
	IEGraphicFileType ft = IE_ImpGraphic::fileTypeForContents(
		reinterpret_cast<const char *>(bb->getPointer(0)),
		bb->getLength());
	TFPASS(ft == IE_ImpGraphic::fileTypeForSuffix(".wpg") ||
		   ft != IEGFT_Unknown);
	delete bb;
}

/* ------------------------------------------------- COVD07 additions
 * Drive the librevenge stream adapter directly: libwpg only ever calls
 * isStructured() + getSubStreamByName(), so the remaining substream
 * virtuals are exercised here on both container and flat inputs. */

TFTEST_MAIN("wpg stream adapter exposes container members")
{
	GsfInput * input = open_fixture("test/wp/wpgcov/cov_ole.wpg");
	TFPASS(input != nullptr);
	if (!input)
		return;

	{
		AbiWordPerfectGraphicsInputStream s(input);
		TFPASS(s.isStructured());
		TFPASS(s.subStreamCount() >= 1);
		const char * n0 = s.subStreamName(0);
		TFPASS(n0 != nullptr);
		/* second lookup walks the m_substreams cache path */
		TFPASS(s.subStreamName(0) == n0);
		TFPASS(s.subStreamName(10000) == nullptr);
		if (n0)
		{
			TFPASS(s.existsSubStream(n0));
			librevenge::RVNGInputStream * byName =
				s.getSubStreamByName(n0);
			TFPASS(byName != nullptr);
			delete byName;
		}
		TFPASS(!s.existsSubStream("no_such_member"));
		TFPASS(s.getSubStreamByName("no_such_member") == nullptr);
		TFPASS(s.getSubStreamById(10000) == nullptr);

		/* by-id member stream supports read/seek/tell/isEnd */
		librevenge::RVNGInputStream * sub = s.getSubStreamById(0);
		TFPASS(sub != nullptr);
		if (sub)
		{
			unsigned long got = 0;
			TFPASS(sub->read(8, got) != nullptr && got > 0);
			TFPASS(sub->tell() > 0);
			TFPASS(!sub->isEnd());
			TFPASS(sub->seek(0, librevenge::RVNG_SEEK_SET) == 0);
			TFPASS(sub->tell() == 0);
			TFPASS(sub->seek(0, librevenge::RVNG_SEEK_END) == 0);
			TFPASS(sub->isEnd());
			TFPASS(sub->seek(-2, librevenge::RVNG_SEEK_CUR) == 0);
			delete sub;
		}

		/* the same ops on the wrapping stream read the container file */
		TFPASS(s.seek(0, librevenge::RVNG_SEEK_SET) == 0);
		unsigned long got = 0;
		TFPASS(s.read(4, got) != nullptr && got == 4);
		TFPASS(s.tell() == 4);
		TFPASS(s.seek(0, librevenge::RVNG_SEEK_END) == 0);
		TFPASS(s.isEnd());
	}
	g_object_unref(input);
}

TFTEST_MAIN("wpg stream adapter on flat input reports no container")
{
	GsfInput * input = open_fixture("test/wp/wpgcov/seed_wpg1_rect.wpg");
	TFPASS(input != nullptr);
	if (!input)
		return;

	{
		AbiWordPerfectGraphicsInputStream s(input);
		TFPASS(!s.isStructured());
		TFPASS(s.subStreamCount() == 0);
		TFPASS(s.subStreamName(0) == nullptr);
		TFPASS(!s.existsSubStream("PerfectOffice_MAIN"));
		TFPASS(s.getSubStreamByName("PerfectOffice_MAIN") == nullptr);
		TFPASS(s.getSubStreamById(0) == nullptr);

		/* stream ops still work on the flat bytes — but rewind first:
		 * the container probe reads ahead and leaves the cursor at
		 * EOF (libwpg does the same seek(0,SET) before parsing) */
		TFPASS(s.seek(0, librevenge::RVNG_SEEK_SET) == 0 && s.tell() == 0);
		unsigned long got = 0;
		TFPASS(s.read(4, got) != nullptr && got == 4);
		TFPASS(s.tell() == 4);
		TFPASS(s.seek(0, librevenge::RVNG_SEEK_END) == 0 && s.isEnd());
	}
	g_object_unref(input);
}

TFTEST_MAIN("wpg container without PerfectOffice_MAIN rejects cleanly")
{
	GsfInput * input = open_fixture("test/wp/wpgcov/cov_nomember.wpg");
	TFPASS(input != nullptr);
	if (!input)
		return;

	/* it IS a structured zip, just lacking the drawing member */
	{
		AbiWordPerfectGraphicsInputStream s(input);
		TFPASS(s.isStructured());
		TFPASS(s.subStreamCount() == 1);
		TFPASS(!s.existsSubStream("PerfectOffice_MAIN"));
		TFPASS(s.getSubStreamByName("PerfectOffice_MAIN") == nullptr);
	}

	IE_Imp_WordPerfectGraphics_Sniffer sn;
	TFPASS(sn.recognizeContents(input) == UT_CONFIDENCE_ZILCH);

	IE_Imp_WordPerfectGraphics imp;
	FG_ConstGraphicPtr pfg;
	TFPASS(imp.importGraphic(input, pfg) == UT_ERROR);
	g_object_unref(input);
}

TFTEST_MAIN("wpg sniffer surface: labels, suffix table, import")
{
	IE_Imp_WordPerfectGraphics_Sniffer sn;

	const IE_SuffixConfidence * sc = sn.getSuffixConfidence();
	TFPASS(sc != nullptr);
	TFPASS(sc && sc[0].confidence == UT_CONFIDENCE_PERFECT &&
		   sc[0].suffix == "wpg");

	const char *desc = nullptr, *suff = nullptr;
	IEGraphicFileType ft = IEGFT_Unknown;
	TFPASS(sn.getDlgLabels(&desc, &suff, &ft));
	TFPASS(desc && strstr(desc, "WordPerfect"));
	TFPASS(suff && strstr(suff, "wpg"));

	GsfInput * in = open_fixture("test/wp/wpgcov/seed_wpg1_rect.wpg");
	TFPASS(in != nullptr);
	if (in)
	{
		TFPASS(sn.recognizeContents(in) == UT_CONFIDENCE_PERFECT);
		g_object_unref(in);
	}
	in = open_fixture("test/wp/wpgcov/edge_garbage.wpg");
	if (in)
	{
		TFPASS(sn.recognizeContents(in) == UT_CONFIDENCE_ZILCH);
		g_object_unref(in);
	}

	IE_ImpGraphic * imp = nullptr;
	TFPASS(sn.constructImporter(&imp) == UT_OK);
	TFPASS(imp != nullptr);
	delete imp;
}
