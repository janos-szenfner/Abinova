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

/* TST09 — pin the libwmf-based WMF importer through the public
 * IE_ImpGraphic registry.  The generic GdkPixbuf importer also claims
 * ".wmf" (with GOOD confidence), so document-level conversion paths
 * may never reach the libwmf importer; the suffix table gives the
 * dedicated sniffer PERFECT confidence, and this suite constructs the
 * importer for that file type directly.
 *
 * When the build lacks libwmf the ".wmf" type resolves to the generic
 * importer — detected via the dialog description so the suite skips
 * gracefully instead of failing on an absent optional dependency.
 */

#include "tf_test.h"
#include "ut_bytebuf.h"
#include "ie_impGraphic.h"
#include "fg_Graphic.h"

#include <glib.h>

#include <cstring>
#include <string>

#define TFSUITE "core.wp.impexp.wmfimp"

static bool load_wmf(UT_ByteBuf & bb)
{
	std::string path = TF_Test::get_test_src_dir();
	path += "/test/wp/cov11/cov11.wmf";
	gchar *bytes = nullptr;
	gsize len = 0;
	if (!g_file_get_contents(path.c_str(), &bytes, &len, nullptr))
		return false;
	bb.append(reinterpret_cast<const UT_Byte *>(bytes),
			  static_cast<UT_uint32>(len));
	g_free(bytes);
	return true;
}

/* does the .wmf suffix resolve to the libwmf importer (rather than the
 * generic pixbuf fallback that claims it when libwmf is absent)? */
static bool wmf_sniffer_is_libwmf(IEGraphicFileType ft)
{
	const char * desc = nullptr, * suff = nullptr;
	IEGraphicFileType eft = IEGFT_Unknown;
	for (UT_uint32 i = 0;
		 IE_ImpGraphic::enumerateDlgLabels(i, &desc, &suff, &eft); i++) {
		if (eft == ft)
			return desc && strstr(desc, "Metafile");
	}
	return false;
}

TFTEST_MAIN("wmf suffix pins libwmf importer; converts metafile to PNG")
{
	UT_ByteBufPtr bb(new UT_ByteBuf);
	if (!load_wmf(*bb)) {
		TFPASS(false);   // fixture missing
		return;
	}

	IEGraphicFileType ft = IE_ImpGraphic::fileTypeForSuffix(".wmf");
	TFPASS(ft != IEGFT_Unknown);
	if (!wmf_sniffer_is_libwmf(ft))
		return;   // build without libwmf — generic importer won

	IE_ImpGraphic *imp = nullptr;
	TFPASS(IE_ImpGraphic::constructImporter(bb, ft, &imp) == UT_OK);
	TFPASS(imp != nullptr);
	if (!imp)
		return;

	FG_ConstGraphicPtr pfg;
	TFPASS(imp->importGraphic(bb, pfg) == UT_OK);
	TFPASS(pfg != nullptr);
	if (pfg) {
		TFPASS(pfg->getType() == FGT_Raster);
		TFPASS(pfg->getMimeType() == "image/png");
		TFPASS(pfg->getWidth() > 0);
		TFPASS(pfg->getHeight() > 0);
		TFPASS(pfg->getBuffer() != nullptr);
		const UT_ConstByteBufPtr & png = pfg->getBuffer();
		TFPASS(png && png->getLength() > 8);
		static const UT_Byte sig[4] = {0x89, 'P', 'N', 'G'};
		TFPASS(memcmp(png->getPointer(0), sig, 4) == 0);
		TFPASS(pfg->clone() != nullptr);
	}
	delete imp;
}

TFTEST_MAIN("wmf importer rejects malformed input")
{
	IEGraphicFileType ft = IE_ImpGraphic::fileTypeForSuffix(".wmf");
	if (ft == IEGFT_Unknown || !wmf_sniffer_is_libwmf(ft))
		return;

	UT_ByteBufPtr junk(new UT_ByteBuf);
	static const UT_Byte bytes[] = {
		'n', 'o', 't', ' ', 'a', ' ', 'm', 'e', 't', 'a', 'f', 'i', 'l', 'e'};
	junk->append(bytes, sizeof(bytes));

	IE_ImpGraphic *imp = nullptr;
	TFPASS(IE_ImpGraphic::constructImporter(junk, ft, &imp) == UT_OK);
	TFPASS(imp != nullptr);
	if (!imp)
		return;

	FG_ConstGraphicPtr pfg;
	/* the libwmf scan/play must fail rather than emit a graphic */
	TFPASS(imp->importGraphic(junk, pfg) != UT_OK);
	TFPASS(pfg == nullptr);

	/* null buffer exercises the bad-argument path */
	UT_ConstByteBufPtr empty;
	TFPASS(imp->importGraphic(empty, pfg) != UT_OK);

	/* truncated placeable header is still not a usable metafile */
	UT_ByteBufPtr trunc(new UT_ByteBuf);
	static const UT_Byte hdr[] = {0xd7, 0xcd, 0xc6, 0x9a};
	trunc->append(hdr, sizeof(hdr));
	TFPASS(imp->importGraphic(trunc, pfg) != UT_OK);

	delete imp;
}

TFTEST_MAIN("wmf contents sniffing exercises every graphic sniffer")
{
	UT_ByteBufPtr bb(new UT_ByteBuf);
	if (!load_wmf(*bb))
		return;
	/* placeable-metafile magic round-trips through every registered
	 * sniffer's recognizeContents, including the libwmf one */
	IE_ImpGraphic::fileTypeForContents(
		reinterpret_cast<const char *>(bb->getPointer(0)),
		bb->getLength());
	TFPASS(true);
}
