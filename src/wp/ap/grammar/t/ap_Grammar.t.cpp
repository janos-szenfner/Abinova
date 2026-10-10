/* AbiSource Applications
 * Copyright (C) 2026 Abinova contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

/* unit tests for wp/ap/grammar: PieceOfText::countWords tokenisation,
 * HunspellWrap dictionary discovery (DICPATH, XDG dirs, the en_*.
 * dic fallback scan, and the no-dictionary early returns), and
 * Abi_GrammarCheck::CheckBlock over a headless document layout.
 *
 * AbiGrammar.cpp's two-line notify(AV_View*, mask) overload is
 * unreachable by construction: XAP_App::notifyListeners dispatches
 * PLUGIN_EXTRA listeners to the three-argument notify(), so it is
 * left uncovered on purpose (documented in COVERAGE.md). */

#include "tf_test.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "../AbiGrammarCheck.h"
#include "../AbiGrammarUtil.h"
#include "../HunspellWrap.h"
#include "fl_BlockLayout.h"
#include "fl_DocLayout.h"
#include "fl_Squiggles.h"
#include "fv_View.h"
#include "gr_UnixCairoGraphics.h"
#include "pd_Document.h"
#include "pp_PropertyMap.h"
#include "pt_PieceTable.h"
#include "ut_types.h"
#include "xap_App.h"

#define TFSUITE "core.wp.ap.grammar"

extern "C" void __gcov_dump(void);

namespace {

/* headless doc + layout + view on GR_UnixCairoGraphics — same
 * pattern as fv_EditOps.t.cpp */
struct GrammarDoc
{
	GrammarDoc() = default;
	GrammarDoc(const GrammarDoc &) = delete;
	GrammarDoc &operator=(const GrammarDoc &) = delete;

	bool load(const char *text, const char *lang = "en-US")
	{
		doc = new PD_Document;
		if (doc->createRawDocument() != UT_OK)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		pt_PieceTable *pt = doc->getPieceTable();
		bool ok = pt->appendStrux(PTX_Section, PP_NOPROPS);
		ok = ok && pt->appendStrux(PTX_Block, PP_NOPROPS);
		if (text && *text)
		{
			if (lang)
			{
				/* span properties ride inside the "props" attribute
				 * (name:value; pairs), not as bare attributes */
				ok = ok && pt->appendFmt({PT_PROPS_ATTRIBUTE_NAME,
						std::string("lang:") + lang});
			}
			UT_UCS4String s(text);
			ok = ok && pt->appendSpan(s.ucs4_str(),
						  static_cast<UT_uint32>(s.length()));
		}
		if (!ok)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		doc->finishRawCreation();
		return finish();
	}

	bool finish()
	{
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

	fl_BlockLayout *firstBlock()
	{
		fl_BlockLayout *b = layout->findBlockAtPosition(2);
		return b;
	}

	~GrammarDoc()
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

/* Each dictionary-discovery scenario runs in a forked child.
 * GLib caches g_get_system_data_dirs() on its first call, so the
 * environment has to be set before that process first constructs a
 * HunspellWrap — and a child's cache can never poison the parent's
 * own search (used by the CheckBlock tests below).  Returns the
 * child's exit code; each step returns a distinct nonzero code. */
int dict_child(int (*fn)(const char *), const char *arg)
{
	pid_t pid = fork();
	if (pid == 0)
	{
		int rc = fn(arg);
		__gcov_dump();
		_exit(rc);
	}
	int st = 0;
	waitpid(pid, &st, 0);
	return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

/* unmodified environment: the system hunspell en_US must be found */
int dict_child_system(const char *)
{
	HunspellWrap wrap;
	PieceOfText t;
	t.sText = "the quick brown fox jumps over the lazy dog";
	if (!wrap.parseSentence(&t)) return 1;
	if (!(t.m_bGrammarChecked && t.m_bGrammarOK)) return 2;

	/* parseSentence returns true when the sentence checked clean */
	PieceOfText bad;
	bad.sText = "the quick brown fxo jumps over the lzay dog";
	if (wrap.parseSentence(&bad)) return 3;
	if (!(bad.m_bGrammarChecked && !bad.m_bGrammarOK)) return 4;
	if (bad.m_vecGrammarErrors.size() < 2) return 5;
	return 0;
}

/* whole dictionary universe = a private DICPATH dir holding only
 * en_ZZ.* (a symlinked copy of en_US): the en_US/en_GB/en name loop
 * misses and the en_*.dic fallback scan finds it */
int dict_child_isolated(const char *dicdir)
{
	setenv("DICPATH", dicdir, 1);
	setenv("XDG_DATA_HOME", "/nonexistent-xdg-data-home", 1);
	setenv("XDG_DATA_DIRS", "/nonexistent-xdg-data-dirs", 1);

	HunspellWrap wrap;
	PieceOfText t;
	t.sText = "the cat sat";
	if (!wrap.parseSentence(&t)) return 1;
	t.sText = "the catz sat";
	if (wrap.parseSentence(&t)) return 2;
	if (t.m_vecGrammarErrors.empty()) return 3;
	if (!wrap.clear()) return 4;
	return 0;
}

/* no dictionary anywhere: ctor fails and parseSentence is a trivial
 * success.  Returns 2 (skip) when a dictionary was found anyway —
 * i.e. the process inherited a g_get_system_data_dirs() cache that
 * was built from real XDG dirs before the fork. */
int dict_child_nodict(const char *)
{
	setenv("DICPATH", "/nonexistent-dicpath", 1);
	setenv("XDG_DATA_HOME", "/nonexistent-xdg-data-home", 1);
	setenv("XDG_DATA_DIRS", "/nonexistent-xdg-data-dirs", 1);

	HunspellWrap wrap;
	PieceOfText probe;
	probe.sText = "the cat sat";
	if (wrap.parseSentence(&probe) && probe.m_bGrammarChecked)
		return 2;

	PieceOfText t;
	t.sText = "zzzq qqqz";
	if (!wrap.parseSentence(&t)) return 1;
	if (!t.m_vecGrammarErrors.empty()) return 3;
	if (!wrap.parseSentence(nullptr)) return 4;
	return 0;
}

} // namespace

TFTEST_MAIN("PieceOfText countWords")
{
	{
		PieceOfText t;
		t.sText = "hello world this is a sentence";
		t.countWords();
		TFPASS(t.nWords >= 5 && !t.bHasStop);
	}
	{
		/* '.' after a digit is a decimal point, not a stop */
		PieceOfText t;
		t.sText = "the value is 3.5 today";
		t.countWords();
		TFPASS(!t.bHasStop);
	}
	{
		/* a real sentence stop */
		PieceOfText t;
		t.sText = "the end.";
		t.countWords();
		TFPASS(t.bHasStop);
	}
	{
		/* separators ;:, tab */
		PieceOfText t;
		t.sText = "one; two: three, four\tfive.";
		t.countWords();
		TFPASS(t.bHasStop && t.nWords >= 4);
	}
	{
		/* a bare number after a separator is discounted: only the
		 * second real word is counted */
		PieceOfText t;
		t.sText = "word 42 word";
		t.countWords();
		TFPASS(t.nWords == 1);
	}
}

TFTEST_MAIN("HunspellWrap dictionary discovery")
{
	if (!g_file_test("/usr/share/hunspell/en_US.dic", G_FILE_TEST_EXISTS))
		return; /* no system hunspell en_US to test against */

	/* DICPATH dir holding an en_*.dic the en_US/en_GB/en loop cannot
	 * name -> exercises the fallback directory scan */
	std::string dicdir = std::string(g_get_tmp_dir()) + "/abinova-grammar-XXXXXX";
	char *tmpdir = g_strdup(dicdir.c_str());
	TFPASS(g_mkdtemp(tmpdir) != nullptr);
	dicdir = tmpdir;
	g_free(tmpdir);

	for (const char *ext : {".aff", ".dic"})
	{
		std::string dst = dicdir + "/en_ZZ" + ext;
		if (symlink(("/usr/share/hunspell/en_US" + std::string(ext)).c_str(),
			    dst.c_str()) != 0)
		{
			/* symlink failed (e.g. source missing): copy instead */
			std::string src = "/usr/share/hunspell/en_US" + std::string(ext);
			gchar *contents = nullptr;
			gsize len = 0;
			if (!g_file_get_contents(src.c_str(), &contents, &len, nullptr) ||
			    !g_file_set_contents(dst.c_str(), contents, len, nullptr))
			{
				TFPASS(false);
				return;
			}
			g_free(contents);
		}
	}

	/* each scenario forks so its environment (and GLib's cached
	 * data-dirs answer) stays private */
	TFPASS(dict_child(dict_child_system, nullptr) == 0);
	TFPASS(dict_child(dict_child_isolated, dicdir.c_str()) == 0);
	{
		int rc = dict_child(dict_child_nodict, nullptr);
		TFPASS(rc == 0 || rc == 2); /* 2 = dict found anyway: skip */
	}

	for (const char *ext : {".aff", ".dic"})
		g_remove((dicdir + "/en_ZZ" + ext).c_str());
	g_rmdir(dicdir.c_str());
}

TFTEST_MAIN("Abi_GrammarCheck CheckBlock")
{
	Abi_GrammarCheck chk;
	TFPASS(!chk.CheckBlock(nullptr));

	/* empty block: GetEnglishText finds nothing */
	{
		GrammarDoc d;
		TFPASS(d.load(nullptr));
		fl_BlockLayout *b = d.firstBlock();
		TFPASS(b != nullptr);
		TFPASS(chk.CheckBlock(b));
	}

	/* run with no language property -> not English */
	{
		GrammarDoc d;
		TFPASS(d.load("a paragraph without a lang prop.", nullptr));
		fl_BlockLayout *b = d.firstBlock();
		TFPASS(b != nullptr);
		TFPASS(chk.CheckBlock(b));
		{
			UT_sint32 iFirst = -1, iLast = -1;
			TFPASS(!b->getGrammarSquiggles()->findRange(0, 1<<20, iFirst, iLast));
		}
	}

	/* non-English text -> rejected at the language check */
	{
		GrammarDoc d;
		TFPASS(d.load("voici un texte en francais avec des mots.", "fr-FR"));
		fl_BlockLayout *b = d.firstBlock();
		TFPASS(b != nullptr);
		TFPASS(chk.CheckBlock(b));
		{
			UT_sint32 iFirst = -1, iLast = -1;
			TFPASS(!b->getGrammarSquiggles()->findRange(0, 1<<20, iFirst, iLast));
		}
	}

	/* heading-like: few words, no stop -> skipped silently */
	{
		GrammarDoc d;
		TFPASS(d.load("Short Heading Words"));
		fl_BlockLayout *b = d.firstBlock();
		TFPASS(b != nullptr);
		TFPASS(chk.CheckBlock(b));
	}

	/* stopped but tiny -> skipped */
	{
		GrammarDoc d;
		TFPASS(d.load("Hi there."));
		fl_BlockLayout *b = d.firstBlock();
		TFPASS(b != nullptr);
		TFPASS(chk.CheckBlock(b));
	}

	/* a length-1 non-text run (field) contributes a placeholder
	 * space inside GetEnglishText */
	{
		GrammarDoc d;
		TFPASS(d.load("Words before."));
		TFPASS(d.view != nullptr);
		PT_DocPosition eod;
		d.doc->getBounds(true, eod);
		d.view->setPoint(eod > 1 ? eod - 1 : eod);
		TFPASS(d.view->cmdInsertField("time") != UT_ERROR);
		fl_BlockLayout *b = d.firstBlock();
		TFPASS(b != nullptr);
		TFPASS(chk.CheckBlock(b));
	}

	/* a real sentence -> spellchecked; misspellings become grammar
	 * squiggles on the block */
	{
		GrammarDoc d;
		TFPASS(d.load("This paragraph contans a misspeled word and "
			      "it goes on for long enough to matter."));
		fl_BlockLayout *b = d.firstBlock();
		TFPASS(b != nullptr);
		TFPASS(chk.CheckBlock(b));
		{
			UT_sint32 iFirst = -1, iLast = -1;
			TFPASS(b->getGrammarSquiggles()->findRange(0, 1<<20, iFirst, iLast));
		}
	}
}
