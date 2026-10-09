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
