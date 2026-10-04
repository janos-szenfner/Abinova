/* AbiSource Application Framework
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

#include <string.h>
#include <stdio.h>

#include <glib.h>

#include "tf_test.h"

#include "xap_Args.h"
#include "xap_Dictionary.h"
#include "xap_Log.h"
#include "xap_StatusBar.h"
#include "xap_FakeClipboard.h"
#include "xap_FontPreview.h"
#include "ev_EditMethod.h"
#include "ut_string_class.h"

#define TFSUITE "core.af.xap.misc"

static std::string tmpfile(const char *name)
{
	gchar *p = g_build_filename(g_get_tmp_dir(), name, nullptr);
	std::string s(p);
	g_free(p);
	return s;
}

/* ------------------------------------------------------------------ */
/* XAP_Args                                                            */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("XAP_Args")
{
	/* the argv-taking ctor just stores the pointers */
	char a0[] = "abinova", a1[] = "--to=pdf", a2[] = "in.abw";
	char *argv[] = {a0, a1, a2, nullptr};
	{
		XAP_Args args(3, argv);
	}

	/* the string ctor tokenizes, honoring quotes */
	{
		XAP_Args args("prog --flag 'two words' \"three more\" tail");
	}
	{
		XAP_Args args("   \t  ");
	}
	{
		XAP_Args args("");
	}
	{
		XAP_Args args(static_cast<const char*>(nullptr));
	}
	/* >10 tokens forces the argv vector to grow */
	{
		XAP_Args args("a b c d e f g h i j k l m n o p q r s t");
	}
	TFPASS(true);
}

/* ------------------------------------------------------------------ */
/* XAP_Dictionary                                                      */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("XAP_Dictionary")
{
	std::string path = tmpfile("xap_dict_test.dic");
	unlink(path.c_str());

	/* load() on a missing file fails */
	{
		XAP_Dictionary dict(path.c_str());
		TFPASS(!dict.load());
		TFPASS(dict.getShortName() == nullptr);
	}

	/* seed file: LF, CR and CRLF terminators + a multibyte word */
	{
		FILE *fp = fopen(path.c_str(), "w");
		TFPASS(fp != nullptr);
		fputs("alpha\nbeta\rgamma\r\ndelta", fp);
		fputs("\n", fp);
		fputs("na\xC3\xAFve\n", fp); /* "naive" with diaeresis */
		fclose(fp);
	}

	{
		XAP_Dictionary dict(path.c_str());
		TFPASS(dict.load());

		/* hardwired words are always added by load() */
		const UT_UCS4Char w_abi[] = {'A','b','i','n','o','v','a'};
		TFPASS(dict.isWord(w_abi, 7));

		const UT_UCS4Char w_alpha[] = {'a','l','p','h','a'};
		TFPASS(dict.isWord(w_alpha, 5));
		const UT_UCS4Char w_beta[] = {'b','e','t','a'};
		TFPASS(dict.isWord(w_beta, 4));
		const UT_UCS4Char w_gamma[] = {'g','a','m','m','a'};
		TFPASS(dict.isWord(w_gamma, 5));
		const UT_UCS4Char w_delta[] = {'d','e','l','t','a'};
		TFPASS(dict.isWord(w_delta, 5));

		const UT_UCS4Char w_absent[] = {'z','z','z','n','o','p','e'};
		TFPASS(!dict.isWord(w_absent, 7));

		/* smart-quote apostrophe: addWord() normalizes the stored
		 * UCS copy, but the hash key is built from the raw char-
		 * truncated input -- so lookup with the same form matches */
		const UT_UCS4Char w_smart[] = {'d','o','n',0x2019,'t'};
		TFPASS(dict.addWord(w_smart, 5));
		TFPASS(dict.isWord(w_smart, 5));

		/* addWord(char*) + empty-string rejection */
		TFPASS(dict.addWord("epsilon"));
		TFPASS(!dict.addWord(""));

		/* countCommonChars scoring */
		UT_UCS4Char hay[] = {'a','l','p','h','a',0};
		UT_UCS4Char nee[] = {'a','l',0};
		TFPASS(dict.countCommonChars(hay, nee) == 2);
		UT_UCS4Char none[] = {'z',0};
		TFPASS(dict.countCommonChars(hay, none) == 0);

		/* suggestWord: near-identical words suggest each other */
		std::vector<UT_UCS4Char *> sugg;
		const UT_UCS4Char w_alpht[] = {'a','l','p','h','t'};
		dict.suggestWord(&sugg, w_alpht, 5);
		TFPASS(!sugg.empty());
		for (auto p : sugg) g_free(p);
		sugg.clear();

		/* completely different word -> no suggestions */
		const UT_UCS4Char w_far[] = {'q','x','j','k','w','v','z'};
		dict.suggestWord(&sugg, w_far, 7);
		for (auto p : sugg) g_free(p);

		/* save() writes the dirty word list */
		TFPASS(dict.save());
	}

	/* the saved file reloads with the new words */
	{
		XAP_Dictionary dict2(path.c_str());
		TFPASS(dict2.load());
		const UT_UCS4Char w_eps[] = {'e','p','s','i','l','o','n'};
		TFPASS(dict2.isWord(w_eps, 7));
	}

	unlink(path.c_str());
}

/* ------------------------------------------------------------------ */
/* XAP_Log                                                             */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("XAP_Log")
{
	/* ctor is private; the singleton writes "fixme_log.txt" in cwd.
	 * get_instance() opens the file and emits the prolog. */
	XAP_Log *log = XAP_Log::get_instance();
	TFPASS(log != nullptr);
	TFPASS(log == XAP_Log::get_instance());

	/* the log file exists in cwd; output stays buffered until the
	 * singleton's destructor flushes it at process exit */
	const char *logpath = "fixme_log.txt";
	FILE *fp = fopen(logpath, "r");
	TFPASS(fp != nullptr);
	if (fp)
		fclose(fp);
	/* keep the FILE* usable while removing the directory entry */
	unlink(logpath);

	log->log("fileInsertText", nullptr, nullptr);

	/* EV_EditMethodCallData's dtor does delete[] on m_pData
	 * unconditionally, so the payload must be heap-allocated */
	EV_EditMethodCallData cd;
	cd.m_xPos = 3;
	cd.m_yPos = 4;
	cd.m_pData = nullptr;
	cd.m_dataLength = 0;
	cd.m_bAllocatedData = false;
	log->log("keyPress", nullptr, &cd);

	cd.m_pData = new UT_UCS4Char[6];
	UT_UCS4Char payload[] = {'h','e','l','l','o',0};
	memcpy(cd.m_pData, payload, sizeof(payload));
	cd.m_dataLength = 5;
	cd.m_bAllocatedData = true;
	log->log("insData", nullptr, &cd);
}

/* ------------------------------------------------------------------ */
/* XAP_StatusBar                                                       */
/* ------------------------------------------------------------------ */

namespace {
class TestStatusBar : public XAP_StatusBar
{
public:
	void statusMessage (const char * pbuf, bool urgent) override
	{
		lastMsg = pbuf ? pbuf : "";
		lastUrgent = urgent;
		count++;
	}
	std::string lastMsg;
	bool lastUrgent = false;
	int count = 0;
};
}

TFTEST_MAIN("XAP_StatusBar")
{
	TestStatusBar sb1, sb2, sb3;

	/* no bars registered: messages are dropped silently */
	XAP_StatusBar::message("quiet");
	XAP_StatusBar::debugmsg("quiet");

	XAP_StatusBar::setStatusBar(&sb1);
	XAP_StatusBar::setStatusBar(&sb2);
	XAP_StatusBar::setStatusBar(&sb3); /* "too many" message path */

	XAP_StatusBar::message("hello bar");
	TFPASS(sb1.lastMsg == "hello bar");
	TFPASS(sb2.lastMsg == "hello bar");
	TFPASS(sb1.count == 2 && sb2.count == 2); /* +1 from "too many" */

	/* debugmsg only reaches the second bar */
	XAP_StatusBar::debugmsg("debug only");
	TFPASS(sb2.lastMsg == "debug only");
	TFPASS(sb1.lastMsg == "hello bar");

	XAP_StatusBar::unsetStatusBar(&sb1);
	XAP_StatusBar::message("one bar left");
	TFPASS(sb1.lastMsg == "hello bar"); /* unchanged */
	TFPASS(sb2.lastMsg == "one bar left");

	XAP_StatusBar::unsetStatusBar(&sb2);
	XAP_StatusBar::unsetStatusBar(&sb3);
}

/* ------------------------------------------------------------------ */
/* XAP_FakeClipboard                                                   */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("XAP_FakeClipboard")
{
	XAP_FakeClipboard clip;

	TFPASS(!clip.hasFormat("text/plain"));

	void *pData = nullptr;
	UT_uint32 len = 999;
	TFPASS(!clip.getClipboardData("text/plain", &pData, &len));
	TFPASS(pData == nullptr && len == 0);

	const char payload[] = "clipboard contents";
	TFPASS(clip.addData("text/plain", payload, sizeof(payload)));
	TFPASS(clip.hasFormat("text/plain"));
	TFPASS(!clip.hasFormat("image/png"));

	/* formats match case-insensitively */
	TFPASS(clip.hasFormat("TEXT/PLAIN"));

	TFPASS(clip.getClipboardData("text/plain", &pData, &len));
	TFPASS(len == sizeof(payload));
	TFPASS(!memcmp(pData, payload, len));

	/* re-adding a format replaces the payload */
	const char shorter[] = "x";
	TFPASS(clip.addData("text/plain", shorter, sizeof(shorter)));
	TFPASS(clip.getClipboardData("text/plain", &pData, &len));
	TFPASS(len == sizeof(shorter));

	TFPASS(clip.clearClipboard());
	TFPASS(!clip.hasFormat("text/plain"));
}

/* ------------------------------------------------------------------ */
/* XAP_FontPreview (property map only; GC paths need a window)         */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("XAP_FontPreview")
{
	XAP_FontPreview fp;
	fp.addOrReplaceVecProp("font-family", "Sans");
	fp.setFontFamily("Serif");
	fp.draw(); /* no preview object yet -> no-op */
	TFPASS(true);
}
