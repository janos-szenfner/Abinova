/* AbiSource Program Utilities
 * Copyright (C) 2026 Abinova developers
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

#include <stdio.h>
#include <string.h>
#include <string>

#include <glib.h>
#include <glib/gstdio.h>

#include "tf_test.h"
#include "ut_Script.h"

#define TFSUITE "core.af.util.Script"

namespace
{

// Records every execute() call; the result it returns is configurable.
struct FakeScript final : UT_Script
{
	static int        s_execCount;
	static std::string s_lastFile;
	static UT_Error   s_result;

	UT_Error execute(const char * scriptName) override
	{
		s_execCount++;
		s_lastFile = scriptName ? scriptName : "";
		return s_result;
	}
	const std::string & errmsg() const override
	{
		static const std::string msg = "fake script blew up";
		return msg;
	}
};
int FakeScript::s_execCount = 0;
std::string FakeScript::s_lastFile;
UT_Error FakeScript::s_result = UT_OK;

// Recognises files whose contents begin with magic and/or ".tsfx" files.
struct FakeSniffer final : UT_ScriptSniffer
{
	const char * magic;
	const char * suffix;
	const char * desc;
	const char * suffixList;

	FakeSniffer(const char * m, const char * sfx, const char * d, const char * sl)
		: magic(m), suffix(sfx), desc(d), suffixList(sl) {}

	bool recognizeContents(const char * szBuf, UT_uint32 iNumbytes) const override
	{
		return magic && iNumbytes >= strlen(magic)
			&& memcmp(szBuf, magic, strlen(magic)) == 0;
	}
	bool recognizeSuffix(const char * szSuffix) const override
	{
		return suffix && szSuffix && strcmp(szSuffix, suffix) == 0;
	}
	bool getDlgLabels(const char ** szDesc,
					  const char ** szSuffixList,
					  UT_ScriptIdType * ft) const override
	{
		*szDesc = desc;
		*szSuffixList = suffixList;
		*ft = getType();
		return true;
	}
	UT_Error constructScript(UT_Script ** ppscript) const override
	{
		*ppscript = new FakeScript;
		return UT_OK;
	}
};

static std::string writeTmp(const char * dir, const char * name, const char * data)
{
	std::string path = std::string(dir) + "/" + name;
	FILE * f = fopen(path.c_str(), "wb");
	if (f)
	{
		fwrite(data, 1, strlen(data), f);
		fclose(f);
	}
	return path;
}

} // namespace

TFTEST_MAIN("UT_ScriptLibrary register/enumerate/unregister")
{
	// static so the singleton pointer stays valid after the test
	static UT_ScriptLibrary lib;
	TFPASS(UT_ScriptLibrary::instance() == &lib);
	TFPASS(lib.getNumScripts() == 0);

	FakeSniffer sA("TSCR", ".tsa", "Fake A scripts", "*.tsa");
	FakeSniffer sB(nullptr, ".tsb", "Fake B scripts", "*.tsb");

	lib.registerScript(&sA);
	lib.registerScript(&sB);
	TFPASS(lib.getNumScripts() == 2);
	TFPASS(sA.getType() == 1);
	TFPASS(sB.getType() == 2);
	TFPASS(sA.supportsType(1) && !sA.supportsType(2));

	// enumerateDlgLabels maps index -> sniffer
	const char * desc = nullptr;
	const char * suffixList = nullptr;
	UT_ScriptIdType ft = -1;
	TFPASS(lib.enumerateDlgLabels(0, &desc, &suffixList, &ft));
	TFPASS(strcmp(desc, "Fake A scripts") == 0);
	TFPASS(strcmp(suffixList, "*.tsa") == 0);
	TFPASS(ft == 1);
	TFPASS(lib.enumerateDlgLabels(1, &desc, &suffixList, &ft));
	TFPASS(strcmp(desc, "Fake B scripts") == 0 && ft == 2);
	TFPASS(!lib.enumerateDlgLabels(2, &desc, &suffixList, &ft));

	// unregisterScript removes the sniffer and renumbers the rest
	lib.unregisterScript(&sA);
	TFPASS(lib.getNumScripts() == 1);
	TFPASS(sB.getType() == 1);

	// unregisterAllScripts deletes them (they must be heap objects)
	lib.unregisterScript(&sB);
	FakeSniffer * heap1 = new FakeSniffer("X", ".x", "x", "*.x");
	FakeSniffer * heap2 = new FakeSniffer("Y", ".y", "y", "*.y");
	lib.registerScript(heap1);
	lib.registerScript(heap2);
	TFPASS(lib.getNumScripts() == 2);
	lib.unregisterAllScripts();
	TFPASS(lib.getNumScripts() == 0);
}

TFTEST_MAIN("UT_ScriptLibrary execute")
{
	// static so the singleton and the registered sniffer stay valid
	static UT_ScriptLibrary lib;
	static FakeSniffer sniffer("TSCR", ".tsa", "Fake A scripts", "*.tsa");
	lib.registerScript(&sniffer);
	FakeScript::s_execCount = 0;
	FakeScript::s_result = UT_OK;

	gchar * tmpl = g_strdup("ut_script_XXXXXX");
	gchar * dir = g_dir_make_tmp(tmpl, nullptr);
	g_free(tmpl);
	TFPASS(dir != nullptr);
	if (!dir) return;

	// contents-based sniffing wins over suffix
	std::string p1 = writeTmp(dir, "a.dat", "TSCR do the thing");
	TFPASS(lib.execute(p1.c_str()) == UT_OK);
	TFPASS(FakeScript::s_execCount == 1);
	TFPASS(FakeScript::s_lastFile == p1);

	// suffix fallback when contents don't match
	std::string p2 = writeTmp(dir, "b.tsa", "no magic here");
	TFPASS(lib.execute(p2.c_str()) == UT_OK);
	TFPASS(FakeScript::s_execCount == 2);

	// explicit type bypasses sniffing entirely — even a missing file
	TFPASS(lib.execute("/no/such/file.anything", sniffer.getType()) == UT_OK);
	TFPASS(FakeScript::s_execCount == 3);

	// unknown contents + unknown suffix + no type -> UT_ERROR
	std::string p3 = writeTmp(dir, "c.bin", "random data");
	TFPASS(lib.execute(p3.c_str()) == UT_ERROR);
	TFPASS(FakeScript::s_execCount == 3);

	// a failing script propagates its error + errmsg
	FakeScript::s_result = UT_ERROR;
	TFPASS(lib.execute(p1.c_str()) == UT_ERROR);
	TFPASS(lib.errmsg() == "fake script blew up");
	FakeScript::s_result = UT_OK;

	g_remove(p1.c_str());
	g_remove(p2.c_str());
	g_remove(p3.c_str());
	g_rmdir(dir);
	g_free(dir);
}
