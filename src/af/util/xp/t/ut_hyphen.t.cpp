// Copyright (C) 2026 Abinova contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tf_test.h"
#include "ut_hyphen.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

#define TFSUITE "core.af.util.hyphen"

namespace {

// write a dictionary body to a unique temp file; "" on failure
std::string writeTmpDict(const std::string & dir, const char * name,
						 const char * body)
{
	std::string path = dir + "/" + name;
	FILE * fp = fopen(path.c_str(), "wb");
	if (!fp)
		return {};
	fputs(body, fp);
	fclose(fp);
	return path;
}

std::string joinBreaks(const std::vector<char> & breaks)
{
	std::string s;
	for (size_t i = 0; i < breaks.size(); ++i)
		if (breaks[i])
		{
			if (!s.empty())
				s += ',';
			s += std::to_string(i);
		}
	return s;
}

} // anonymous namespace

TFTEST_MAIN("UT_HyphenDict parses TeX patterns and hyphenates")
{
	char tmpl[] = "/tmp/abinova_hyph_XXXXXX";
	char * dir = mkdtemp(tmpl);
	TFPASS(dir != nullptr);
	std::string sDir(dir);

	// odd pattern weights yield breaks, even ones do not;
	// default minima are left=2 right=3
	std::string path = writeTmpDict(sDir, "hyph_te_ST.dic",
		"UTF-8\n"
		"% a comment\n"
		"hy3phe\n"
		"phe3n\n"
		"se3n\n"
		"no2se\n"
		"NOHYPHEN seno\n");
	TFPASS(!path.empty());
	auto dict = UT_HyphenDict::load(path.c_str());
	TFPASS(dict != nullptr);
	TFPASSEQ(dict->leftMin(), 2);
	TFPASSEQ(dict->rightMin(), 3);

	std::vector<char> breaks;

	// hy3phe -> hy|phenate ; phe3n -> hyphe|nate
	dict->hyphenate("hyphenate", breaks);
	TFPASSEQ((int)breaks.size(), 9);
	TFPASSEQ(joinBreaks(breaks).c_str(), "1,4");

	// se3n -> se|nder (no2se never matches, being even anyway)
	dict->hyphenate("sender", breaks);
	TFPASSEQ(joinBreaks(breaks).c_str(), "1");

	// the even-weighted no2se pattern never yields a break
	dict->hyphenate("nosebox", breaks);
	TFPASSEQ(joinBreaks(breaks).c_str(), "");

	// ... unless the break sits inside a NOHYPHEN sequence:
	// "xse|nox" is a legal interior break silenced by "seno"
	dict->hyphenate("xsenox", breaks);
	TFPASSEQ(joinBreaks(breaks).c_str(), "");

	// control: same pattern position, no forbidden sequence
	dict->hyphenate("xsenax", breaks);
	TFPASSEQ(joinBreaks(breaks).c_str(), "2");

	// shorter than leftMin+rightMin+1: no break at all
	dict->hyphenate("hype", breaks);
	TFPASSEQ(joinBreaks(breaks).c_str(), "");

	// missing file -> nullptr, not a crash
	TFPASS(UT_HyphenDict::load((sDir + "/nope.dic").c_str()) ==
		   nullptr);

	// LEFTHYPHENMIN / RIGHTHYPHENMIN directives widen the breakable
	// interior: 1/2 makes "ab|cd" legal in a 4-letter word
	std::string path2 = writeTmpDict(sDir, "hyph_mi_NI.dic",
		"UTF-8\n"
		"LEFTHYPHENMIN 1\n"
		"RIGHTHYPHENMIN 2\n"
		"ab3cd\n");
	auto dict2 = UT_HyphenDict::load(path2.c_str());
	TFPASS(dict2 != nullptr);
	TFPASSEQ(dict2->leftMin(), 1);
	TFPASSEQ(dict2->rightMin(), 2);
	dict2->hyphenate("abcd", breaks);
	TFPASSEQ(joinBreaks(breaks).c_str(), "1");

	// UT_Hyphenator resolves lang tags through
	// $ABINOVA_HYPHEN_PATH and misses silently
	setenv("ABINOVA_HYPHEN_PATH", sDir.c_str(), 1);
	TFPASS(UT_Hyphenator::getDict("te-ST") != nullptr);
	TFPASS(UT_Hyphenator::getDict("te") == nullptr); // no hyph_te.dic
	TFPASS(UT_Hyphenator::getDict("zz-ZZ") == nullptr);

	unlink(path.c_str());
	unlink(path2.c_str());
	rmdir(sDir.c_str());
}
