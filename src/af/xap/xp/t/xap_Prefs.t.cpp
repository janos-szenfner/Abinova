/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:t; -*- */
/* AbiSource Application Framework Test
 * Copyright (c) 2020 Hubert Figuière
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
#include <unistd.h>

#include <glib.h>

#include "tf_test.h"

#include "xap_App.h"
#include "xap_Prefs.h"
#include "ut_go_file.h"

#define TFSUITE "core.af.xap.prefs"


class TestPrefs
	: public XAP_Prefs
{
public:
	virtual bool loadBuiltinPrefs(void) override
		{
			auto builtinScheme = getBuiltinSchemeName();
			XAP_PrefsScheme* scheme = new XAP_PrefsScheme(this, builtinScheme);

			scheme->setValue("key1", "value1");
			scheme->setValue("key2", "value2");
			scheme->setValue("key3", "value3");
			scheme->setValueBool("key4-bool", false);
			scheme->setValueBool("key5-bool", true);
			scheme->setValueInt("key6-int", 0);
			scheme->setValueInt("key7-int", 32);

			addScheme(scheme);
			return setCurrentScheme(builtinScheme);
		}
	virtual const gchar* getBuiltinSchemeName(void) const override
		{
			return "_builtin_";
		}
	virtual const char* getPrefsPathname(void) const override
		{
			return "/tmp";
		}
	virtual void fullInit(void) override
		{
			loadBuiltinPrefs();
		}
};

TFTEST_MAIN("XAP_PrefsScheme")
{
	TestPrefs pref;
	XAP_PrefsScheme* scheme = new XAP_PrefsScheme(&pref, "_TEST_");

	scheme->setValue("key1", "value1");
	scheme->setValue("key2", "value2");
	scheme->setValue("key3", "value3");
	scheme->setValueBool("key4-bool", false);
	scheme->setValueBool("key5-bool", true);
	scheme->setValueInt("key6-int", 0);
	scheme->setValueInt("key7-int", 32);

	// Test the scheme has values.
	std::string value1;
	bool bool_value2 = false;
	int int_value3 = 0;
	TFPASS(scheme->getValue("key1", value1));
	TFPASS(value1 == "value1");
	TFPASS(scheme->getValueBool("key5-bool", bool_value2));
	TFPASS(bool_value2 == true);
	TFPASS(scheme->getValueInt("key7-int", int_value3));
	TFPASS(int_value3 == 32);

	TFPASS(scheme->getSchemeName() == "_TEST_");

	delete scheme;
}

TFTEST_MAIN("XAP_Prefs")
{
	{
		TestPrefs pref;
		pref.fullInit();

		// We should have a current scheme that is the built in scheme.
		XAP_PrefsScheme* default_scheme = pref.getCurrentScheme();
		TFPASS(default_scheme);
		TFPASS(default_scheme->getSchemeName() == pref.getBuiltinSchemeName());

		// Test the prefs have a default value.
		std::string value2;
		TFPASS(pref.getPrefsValue("key1", value2));
		TFPASS(value2 == "value1");

		// Test adding a scheme
		XAP_PrefsScheme* scheme = new XAP_PrefsScheme(&pref, "_TEST_");
		pref.addScheme(scheme);

		// Test getting a scheme
		TFPASS(pref.getScheme("_TEST_") == scheme);
		TFPASS(pref.getCurrentScheme() == default_scheme);
		TFPASS(!pref.setCurrentScheme("bogus"));
		TFPASS(pref.getCurrentScheme() == default_scheme);

		// Test adding a custom scheme (automatic)
		TFPASS(pref.getCurrentScheme(true) != default_scheme);
		auto custom_scheme = pref.getCurrentScheme();
		TFPASS(custom_scheme->getSchemeName() == "_custom_");

		TFPASS(!pref.setCurrentScheme("bogus"));
		TFPASS(pref.getCurrentScheme() == custom_scheme);
		TFPASS(pref.setCurrentScheme("_TEST_"));
		TFPASS(pref.getCurrentScheme() == scheme);

		// Test indexing schemes
		TFPASS(pref.getNthScheme(0) == default_scheme);
		TFPASS(pref.getNthScheme(1) == scheme);
		TFPASS(pref.getNthScheme(2) == custom_scheme);
		// There is no number 3
		TFPASS(pref.getNthScheme(3) == nullptr);


		TFPASS(pref.setCurrentScheme("_builtin_"));

		// Test the _builtin_ scheme has values.
		{
			std::string value1;
			bool bool_value2 = false;
			int int_value3 = 0;
			TFPASS(pref.getPrefsValue("key1", value1));
			TFPASS(value1 == "value1");
			TFPASS(pref.getPrefsValueBool("key5-bool", bool_value2));
			TFPASS(bool_value2 == true);
			TFPASS(pref.getPrefsValueInt("key7-int", int_value3));
			TFPASS(int_value3 == 32);
		}

		TFPASS(pref.setCurrentScheme("_TEST_"));
		// Test the _TEST_ scheme has values when allowing built-in
		{
			std::string value1;
			bool bool_value2 = false;
			int int_value3 = 0;
			TFPASS(pref.getPrefsValue("key1", value1));
			TFPASS(value1 == "value1");
			TFPASS(pref.getPrefsValueBool("key5-bool", bool_value2));
			TFPASS(bool_value2 == true);
			TFPASS(pref.getPrefsValueInt("key7-int", int_value3));
			TFPASS(int_value3 == 32);
		}
		// Test the _TEST_ scheme does not have values when disallowing built-in
		{
			std::string value1;
			bool bool_value2 = false;
			int int_value3 = 0;
			TFPASS(!pref.getPrefsValue("key1", value1, false));
			TFPASS(!pref.getPrefsValueBool("key5-bool", bool_value2, false));
			TFPASS(!pref.getPrefsValueInt("key7-int", int_value3, false));
		}
	}
}

/* prefs whose file lives at a real path, for save/load tests */
class TestPrefsFile
	: public XAP_Prefs
{
public:
	explicit TestPrefsFile(const std::string &path)
		: m_path(path) {}
	virtual bool loadBuiltinPrefs(void) override
		{
			auto builtinScheme = getBuiltinSchemeName();
			XAP_PrefsScheme* scheme = new XAP_PrefsScheme(this, builtinScheme);
			scheme->setValue("bkey", "bvalue");
			addScheme(scheme);
			return setCurrentScheme(builtinScheme);
		}
	virtual const gchar* getBuiltinSchemeName(void) const override
		{
			return "_builtin_";
		}
	virtual const char* getPrefsPathname(void) const override
		{
			return m_path.c_str();
		}
	virtual void fullInit(void) override
		{
			loadBuiltinPrefs();
		}
	std::string m_path;
};

static std::string prefTmpPath(const char *name)
{
	gchar *p = g_build_filename(g_get_tmp_dir(), name, nullptr);
	std::string s(p);
	g_free(p);
	return s;
}

TFTEST_MAIN("XAP_PrefsScheme value types")
{
	TestPrefs pref;
	pref.fullInit();
	XAP_PrefsScheme* scheme = pref.getCurrentScheme();

	// setValue on an existing key replaces it
	scheme->setValue("key1", "changed");
	std::string v;
	TFPASS(scheme->getValue("key1", v));
	TFPASS(v == "changed");

	// missing key -> false
	TFPASS(!scheme->getValue("no-such-key", v));

	// bool parsing: leading char decides
	bool b = false;
	scheme->setValue("b-t", "true");
	TFPASS(scheme->getValueBool("b-t", b));
	TFPASS(b == true);
	scheme->setValue("b-y", "yes");
	TFPASS(scheme->getValueBool("b-y", b));
	TFPASS(b == true);
	scheme->setValue("b-1", "1");
	TFPASS(scheme->getValueBool("b-1", b));
	TFPASS(b == true);
	scheme->setValue("b-f", "false");
	TFPASS(scheme->getValueBool("b-f", b));
	TFPASS(b == false);
	scheme->setValue("b-e", "");
	TFPASS(!scheme->getValueBool("b-e", b));
	TFPASS(!scheme->getValueBool("b-missing", b));

	// int parsing via atoi
	int n = 0;
	scheme->setValue("i-42", "42");
	TFPASS(scheme->getValueInt("i-42", n));
	TFPASS(n == 42);
	scheme->setValue("i-junk", "junk");
	TFPASS(scheme->getValueInt("i-junk", n));
	TFPASS(n == 0);
	TFPASS(!scheme->getValueInt("i-missing", n));

	// scheme rename
	scheme->setSchemeName("renamed");
	TFPASS(scheme->getSchemeName() == "renamed");
	scheme->setSchemeName(nullptr);
	TFPASS(scheme->getSchemeName() == "");
	scheme->setSchemeName("_builtin_");

	// DeBuG-prefixed keys always resolve via getPrefsValue*
	TFPASS(pref.getPrefsValue("DeBuGThing", v));
	TFPASS(v.empty());
	TFPASS(pref.getPrefsValueBool("debugflag", b));
	TFPASS(b == false);
	TFPASS(pref.getPrefsValueInt("debugint", n));
	TFPASS(n == -1);

	// autosave flag
	pref.setAutoSavePrefs(true);
	TFPASS(pref.getAutoSavePrefs());
	pref.setAutoSavePrefs(false);
	TFPASS(!pref.getAutoSavePrefs());
}

TFTEST_MAIN("XAP_Prefs recent")
{
	TestPrefs pref;
	pref.fullInit();

	pref.setMaxRecent(3);
	TFPASS(pref.getMaxRecent() == 3);
	TFPASS(pref.getRecentCount() == 0);

	pref.addRecent("file:///one.abw");
	pref.addRecent("file:///two.abw");
	pref.addRecent("file:///three.abw");
	TFPASS(pref.getRecentCount() == 3);
	// most recent first
	TFPASS(pref.getRecent(1) && !strcmp(pref.getRecent(1), "file:///three.abw"));
	TFPASS(pref.getRecent(3) && !strcmp(pref.getRecent(3), "file:///one.abw"));

	// re-adding moves to the front rather than duplicating
	pref.addRecent("file:///one.abw");
	TFPASS(pref.getRecentCount() == 3);
	TFPASS(pref.getRecent(1) && !strcmp(pref.getRecent(1), "file:///one.abw"));

	// exceeding the max prunes the tail
	pref.addRecent("file:///four.abw");
	TFPASS(pref.getRecentCount() == 3);
	TFPASS(pref.getRecent(1) && !strcmp(pref.getRecent(1), "file:///four.abw"));
	TFPASS(pref.getRecent(3) && !strcmp(pref.getRecent(3), "file:///three.abw"));

	// shrinking the max prunes immediately
	pref.setMaxRecent(2);
	TFPASS(pref.getRecentCount() == 3); /* setMaxRecent does not prune by itself */
	pref.addRecent("file:///five.abw");
	TFPASS(pref.getRecentCount() == 2);

	// ignore flag is consumed by the next addRecent
	pref.setIgnoreNextRecent();
	TFPASS(pref.isIgnoreRecent());
	pref.addRecent("file:///ignored.abw");
	TFPASS(!pref.isIgnoreRecent());
	TFPASS(pref.getRecentCount() == 2);
	TFPASS(!pref.getRecent(1) || strcmp(pref.getRecent(1), "file:///ignored.abw") != 0);

	// max of 0 disables the list entirely
	pref.setMaxRecent(0);
	TFPASS(pref.getRecent(0) == nullptr);
	pref.addRecent("file:///noop.abw");
	TFPASS(pref.getRecentCount() == 2); /* unchanged: addRecent is a NOOP */
	pref.clearRecent();
	TFPASS(pref.getRecentCount() == 0);

	// removeRecent is one-based
	pref.setMaxRecent(5);
	pref.addRecent("file:///a.abw");
	pref.addRecent("file:///b.abw");
	pref.removeRecent(1);
	TFPASS(pref.getRecentCount() == 1);
	TFPASS(pref.getRecent(1) && !strcmp(pref.getRecent(1), "file:///a.abw"));
}

TFTEST_MAIN("XAP_Prefs geometry and log")
{
	TestPrefs pref;
	pref.fullInit();

	// no geometry yet -> false
	UT_sint32 px = 0, py = 0;
	UT_uint32 w = 0, h = 0, fl = 0;
	TFPASS(!pref.getGeometry(&px, &py, &w, &h, &fl));

	TFPASS(pref.setGeometry(10, 20, 1024, 768, PREF_FLAG_GEOMETRY_SIZE | PREF_FLAG_GEOMETRY_POS));
	TFPASS(pref.getGeometry(&px, &py, &w, &h, &fl));
	TFPASS(px == 10 && py == 20 && w == 1024 && h == 768);
	TFPASS((fl & (PREF_FLAG_GEOMETRY_SIZE | PREF_FLAG_GEOMETRY_POS)) != 0);
	// setGeometry turns autosave on
	TFPASS(pref.getAutoSavePrefs());

	// nullptr out-params are legal
	TFPASS(pref.getGeometry(nullptr, nullptr, nullptr, nullptr, nullptr));

	// log entries of every level; "--" is collapsed for the XML comment
	pref.log("test-where", "plain message");
	pref.log("test-where", "watch -- out", Warning);
	pref.log("test-where", "broken", Error);
}

static void countingListener(XAP_Prefs*, const XAP_PrefsChangeSet* pChangeSet, void* data)
{
	int *counter = static_cast<int*>(data);
	(*counter)++;
	if (pChangeSet)
		for (auto key : *pChangeSet)
			counter[1]++;
}

TFTEST_MAIN("XAP_Prefs listeners")
{
	TestPrefs pref;
	pref.fullInit();

	int counter[2] = {0, 0};
	pref.addListener(countingListener, counter);

	// a setValue outside a block fires immediately
	pref.getCurrentScheme()->setValue("lv1", "a");
	TFPASS(counter[0] == 1 && counter[1] == 1);

	// setting the same value again does not refire
	pref.getCurrentScheme()->setValue("lv1", "a");
	TFPASS(counter[0] == 1 && counter[1] == 1);

	// inside a block, notifications are coalesced
	pref.startBlockChange();
	pref.getCurrentScheme()->setValue("lv2", "b");
	pref.getCurrentScheme()->setValue("lv3", "c");
	TFPASS(counter[0] == 1);
	pref.endBlockChange();
	TFPASS(counter[0] == 2);
	TFPASS(counter[1] == 3); // lv1 + {lv2, lv3}

	// removing with the same data unregisters
	pref.removeListener(countingListener, counter);
	pref.getCurrentScheme()->setValue("lv4", "d");
	TFPASS(counter[0] == 2);

	// removing with no data removes all bindings of the func
	pref.addListener(countingListener, counter);
	pref.removeListener(countingListener);
	pref.getCurrentScheme()->setValue("lv5", "e");
	TFPASS(counter[0] == 2);
}

TFTEST_MAIN("XAP_FontSettings")
{
	XAP_FontSettings fs;

	TFPASS(!fs.getIncludeFlag());
	TFPASS(!fs.haveFontsToInclude());
	TFPASS(!fs.haveFontsToExclude());
	TFPASS(!fs.isOnExcludeList("Anything"));

	fs.addFont("Comic Sans");
	TFPASS(fs.haveFontsToExclude());
	TFPASS(fs.isOnExcludeList("Comic Sans"));
	TFPASS(!fs.isOnExcludeList("Serif"));
	TFPASS(fs.getFonts().size() == 1);

	fs.setIncludeFlag(true);
	TFPASS(fs.getIncludeFlag());
	TFPASS(fs.haveFontsToInclude());
	// in include mode names are never "excluded"
	TFPASS(!fs.isOnExcludeList("Comic Sans"));

	// same through the prefs accessor
	TestPrefs pref;
	pref.fullInit();
	pref.getFontSettings().addFont("Wingdings");
	TFPASS(pref.getFontSettings().isOnExcludeList("Wingdings"));
}

TFTEST_MAIN("XAP_Prefs file load errors")
{
	std::string path = prefTmpPath("xap_prefs_missing.xml");
	unlink(path.c_str());

	// missing file -> false
	{
		TestPrefsFile pref(path);
		pref.fullInit();
		TFPASS(!pref.loadPrefsFile());
	}

	// malformed XML -> false
	{
		FILE *fp = fopen(path.c_str(), "w");
		TFPASS(fp != nullptr);
		fputs("this is not xml at all <<<", fp);
		fclose(fp);
		TestPrefsFile pref(path);
		pref.fullInit();
		TFPASS(!pref.loadPrefsFile());
	}

	// valid XML but not a prefs document -> false (no AbiPreferences)
	{
		FILE *fp = fopen(path.c_str(), "w");
		TFPASS(fp != nullptr);
		fputs("<SomethingElse/>\n", fp);
		fclose(fp);
		TestPrefsFile pref(path);
		pref.fullInit();
		TFPASS(!pref.loadPrefsFile());
	}

	// AbiPreferences but no <Select> -> false
	{
		FILE *fp = fopen(path.c_str(), "w");
		TFPASS(fp != nullptr);
		fputs("<AbiPreferences ver=\"1.0\"><Scheme name=\"x\"/></AbiPreferences>\n", fp);
		fclose(fp);
		TestPrefsFile pref(path);
		pref.fullInit();
		TFPASS(!pref.loadPrefsFile());
	}

	// Select without a scheme attr -> false
	{
		FILE *fp = fopen(path.c_str(), "w");
		TFPASS(fp != nullptr);
		fputs("<AbiPreferences><Select/></AbiPreferences>\n", fp);
		fclose(fp);
		TestPrefsFile pref(path);
		pref.fullInit();
		TFPASS(!pref.loadPrefsFile());
	}

	// Select points at a scheme that does not exist -> false
	{
		FILE *fp = fopen(path.c_str(), "w");
		TFPASS(fp != nullptr);
		fputs("<AbiPreferences><Select scheme=\"nosuch\"/>"
			  "<Scheme name=\"other\"/></AbiPreferences>\n", fp);
		fclose(fp);
		TestPrefsFile pref(path);
		pref.fullInit();
		TFPASS(!pref.loadPrefsFile());
	}

	// a prefs file for a different application -> false
	{
		FILE *fp = fopen(path.c_str(), "w");
		TFPASS(fp != nullptr);
		fputs("<AbiPreferences app=\"NotThisApp\">"
			  "<Select scheme=\"s\"/><Scheme name=\"s\"/></AbiPreferences>\n", fp);
		fclose(fp);
		TestPrefsFile pref(path);
		pref.fullInit();
		TFPASS(!pref.loadPrefsFile());
	}

	unlink(path.c_str());
}

TFTEST_MAIN("XAP_Prefs file load")
{
	std::string path = prefTmpPath("xap_prefs_load.xml");
	unlink(path.c_str());

	const char *appName = XAP_App::getApp()->getApplicationName();

	// a fully populated document: reserved + duplicate schemes are
	// skipped, a <Face> outside <Fonts> is ignored, plain paths in
	// <Recent> are converted to URIs.
	{
		FILE *fp = fopen(path.c_str(), "w");
		TFPASS(fp != nullptr);
		fprintf(fp,
				"<AbiPreferences app=\"%s\" ver=\"1.0\">\n"
				"\t<Select scheme=\"user\" autosaveprefs=\"1\"/>\n"
				"\t<Scheme name=\"_builtin_\" hijack=\"1\"/>\n"
				"\t<Scheme name=\"user\" customkey=\"customval\" extra=\"e\"/>\n"
				"\t<Scheme name=\"user\" dup=\"1\"/>\n"
				"\t<Recent max=\"4\" name1=\"/tmp/plainpath.abw\" name2=\"file:///already/uri.abw\"/>\n"
				"\t<Geometry width=\"111\" height=\"222\" posx=\"5\" posy=\"6\" flags=\"3\"/>\n"
				"\t<Face name=\"IgnoredFace\"/>\n"
				"\t<Fonts include=\"1\"><Face name=\"ListedFace\" weird=\"1\"/></Fonts>\n"
				"\t<Log><line/></Log>\n"
				"\t<UnknownElement foo=\"bar\"/>\n"
				"</AbiPreferences>\n", appName);
		fclose(fp);
	}

	{
		TestPrefsFile pref(path);
		pref.fullInit();
		TFPASS(pref.loadPrefsFile());

		// the selected scheme is live with its values
		TFPASS(pref.getCurrentScheme()->getSchemeName() == "user");
		std::string v;
		TFPASS(pref.getPrefsValue("customkey", v));
		TFPASS(v == "customval");
		// values declared only in the dup'd scheme must be absent
		TFPASS(!pref.getPrefsValue("dup", v, false));
		// builtin scheme was not hijacked by the file
		TFPASS(pref.getPrefsValue("bkey", v));
		TFPASS(v == "bvalue");
		TFPASS(pref.getAutoSavePrefs());

		// recent list: plain path became a URI
		TFPASS(pref.getRecentCount() == 2);
		const char *r1 = pref.getRecent(1);
		TFPASS(r1 != nullptr);
		TFPASS(UT_go_path_is_uri(r1));
		TFPASS(pref.getRecent(2) && !strcmp(pref.getRecent(2), "file:///already/uri.abw"));

		// geometry read back
		UT_sint32 px = 0, py = 0;
		UT_uint32 w = 0, h = 0, fl = 0;
		TFPASS(pref.getGeometry(&px, &py, &w, &h, &fl));
		TFPASS(w == 111 && h == 222);
		TFPASS(px == 5 && py == 6);

		// fonts: include mode + ListedFace only
		TFPASS(pref.getFontSettings().haveFontsToInclude());
		TFPASS(pref.getFontSettings().getFonts().size() == 1);
		TFPASS(pref.getFontSettings().getFonts()[0] == "ListedFace");
	}
	unlink(path.c_str());
}

TFTEST_MAIN("XAP_Prefs save-load round-trip")
{
	std::string path = prefTmpPath("xap_prefs_roundtrip.xml");
	unlink(path.c_str());

	{
		TestPrefsFile pref(path);
		pref.fullInit();

		// customize the current (non-builtin) scheme
		XAP_PrefsScheme *user = new XAP_PrefsScheme(&pref, "user");
		pref.addScheme(user);
		TFPASS(pref.setCurrentScheme("user"));
		user->setValue("rt-key", "rt-value");
		user->setValue("special", "a<b>&\"c");
		user->setValueInt("rt-int", 77);
		user->setValueBool("rt-bool", true);

		pref.addRecent("file:///rt/one.abw");
		pref.addRecent("file:///rt/two.abw");
		pref.setGeometry(3, 4, 640, 480, PREF_FLAG_GEOMETRY_SIZE);
		pref.getFontSettings().addFont("RoundTripFace");
		pref.log("test", "round-trip log entry");

		TFPASS(pref.savePrefsFile());
	}

	{
		TestPrefsFile pref2(path);
		pref2.fullInit();
		TFPASS(pref2.loadPrefsFile());

		TFPASS(pref2.getCurrentScheme()->getSchemeName() == "user");
		std::string v;
		TFPASS(pref2.getPrefsValue("rt-key", v));
		TFPASS(v == "rt-value");
		TFPASS(pref2.getPrefsValue("special", v));
		TFPASS(v == "a<b>&\"c");
		int n = 0;
		TFPASS(pref2.getPrefsValueInt("rt-int", n));
		TFPASS(n == 77);
		bool b = false;
		TFPASS(pref2.getPrefsValueBool("rt-bool", b));
		TFPASS(b);

		TFPASS(pref2.getRecentCount() == 2);
		/* "two" was added last, so it sits at index 1 */
		TFPASS(pref2.getRecent(1) && !strcmp(pref2.getRecent(1), "file:///rt/two.abw"));
		TFPASS(pref2.getRecent(2) && !strcmp(pref2.getRecent(2), "file:///rt/one.abw"));

		UT_uint32 w = 0, h = 0;
		TFPASS(pref2.getGeometry(nullptr, nullptr, &w, &h, nullptr));
		TFPASS(w == 640 && h == 480);

		TFPASS(pref2.getFontSettings().isOnExcludeList("RoundTripFace"));
	}
	unlink(path.c_str());
}

TFTEST_MAIN("XAP_Prefs system defaults")
{
	std::string path = prefTmpPath("xap_prefs_sysdefaults.xml");
	unlink(path.c_str());

	// overlays values onto the builtin scheme; "name" is skipped
	FILE *fp = fopen(path.c_str(), "w");
	TFPASS(fp != nullptr);
	fputs("<root><SystemDefaults syskey=\"sysval\" name=\"x\"/>"
		  "<Ignored foo=\"bar\"/></root>\n", fp);
	fclose(fp);

	TestPrefsFile pref(path);
	pref.fullInit();
	TFPASS(pref.loadSystemDefaultPrefsFile(path.c_str()));

	std::string v;
	TFPASS(pref.getPrefsValue("syskey", v));
	TFPASS(v == "sysval");

	// missing file -> false
	TFPASS(!pref.loadSystemDefaultPrefsFile("/nonexistent/sysdefaults.xml"));
	unlink(path.c_str());
}
