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

/* HARD06: clipboard payloads over the transport cap are refused
 * instead of being materialized (OOM risk), and the refusal is
 * flagged so the paste paths can tell the user instead of looking
 * like an empty clipboard.  These tests exercise the same-process
 * fake-clipboard path, which needs no display server. */

#define TFSUITE "core.af.xap.unixclipboard"

#include <cstring>
#include <memory>

#include "tf_test.h"
#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_UnixClipboard.h"

/* keep in sync with ABI_CLIPBOARD_MAX_BYTES in xap_UnixClipboard.cpp */
#define T_CAP_BYTES (64u * 1024u * 1024u)

TFTEST_MAIN("oversized clipboard payload is rejected and flagged")
{
	XAP_UnixClipboard * clip =
		static_cast<XAP_UnixApp*>(XAP_App::getApp())->getClipboard();
	if (!clip)
		return;		/* headless builds keep m_pClipboard null */

	/* a sane payload round-trips through the fake clipboard */
	const char small[] = "small payload";
	TFPASS(clip->addData(XAP_UnixClipboard::TAG_ClipboardOnly,
						 "text/plain", small, sizeof(small) - 1));
	TFPASS(!clip->wasDataOversized());

	void * pData = nullptr;
	UT_uint32 len = 0;
	const char * fmt = nullptr;
	const char * formats[] = {"text/plain", nullptr};
	TFPASS(clip->getData(XAP_UnixClipboard::TAG_ClipboardOnly,
						 formats, &pData, &len, &fmt));
	TFPASS(len == sizeof(small) - 1);
	TFPASS(fmt && !std::strcmp(fmt, "text/plain"));

	/* a payload over the cap is refused outright: nothing is stored,
	 * and the oversized flag tells paste paths it was a rejection,
	 * not an empty clipboard */
	std::unique_ptr<char[]> huge(new char[T_CAP_BYTES + 1]);
	std::memset(huge.get(), 'z', T_CAP_BYTES + 1);
	const char * pngFormats[] = {"image/png", nullptr};
	TFPASS(!clip->addData(XAP_UnixClipboard::TAG_ClipboardOnly,
						  "image/png", huge.get(), T_CAP_BYTES + 1));
	TFPASS(clip->wasDataOversized());

	pData = nullptr;
	len = 0;
	fmt = nullptr;
	TFPASS(!clip->getData(XAP_UnixClipboard::TAG_ClipboardOnly,
						  pngFormats, &pData, &len, &fmt));
	TFPASS(pData == nullptr && len == 0);
	/* the flag survives the failed read so the paste caller sees it */
	TFPASS(clip->wasDataOversized());

	/* a fresh copy clears the flag along with the stored data */
	clip->clearData(true, false);
	TFPASS(!clip->wasDataOversized());
	TFPASS(!clip->canPaste(XAP_UnixClipboard::TAG_ClipboardOnly));
}
