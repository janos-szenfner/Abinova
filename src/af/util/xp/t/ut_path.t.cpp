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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tf_test.h"
#include "ut_go_file.h"
#include "ut_path.h"
#include "ut_raii.h"

#define TFSUITE "core.af.util.path"

TFTEST_MAIN("UT_basename")
{
	TFPASS(strcmp(UT_basename("/usr/local/bin/tool"), "tool") == 0);
	TFPASS(strcmp(UT_basename("plain.txt"), "plain.txt") == 0);
	TFPASS(strcmp(UT_basename("/a/b/c/"), "") == 0);
	TFPASS(strcmp(UT_basename("/"), "") == 0);
	// returns a pointer into the argument, not a copy
	const char * path = "/x/y/z.png";
	TFPASS(UT_basename(path) == path + 5);
}

TFTEST_MAIN("UT_sanitizeFileName")
{
	// allowlist: alnum, space, '.', '-', '_', '+' and UTF-8 bytes
	TFPASS(UT_sanitizeFileName("report 2026.pdf") == "report 2026.pdf");
	// separators and metachars flatten to '_'; a surviving leading
	// '.' is then replaced so the name can't be hidden or '..'
	TFPASS(UT_sanitizeFileName("../../etc/passwd") == "_._.._etc_passwd");
	TFPASS(UT_sanitizeFileName("a\\b/c") == "a_b_c");
	TFPASS(UT_sanitizeFileName("<img onerror=x>") == "_img onerror_x_");
	TFPASS(UT_sanitizeFileName("a\nb\rc") == "a_b_c");
	// a leading dot becomes '_' so the name can't be hidden or '..'
	TFPASS(UT_sanitizeFileName("..") == "_.");
	TFPASS(UT_sanitizeFileName(".hidden") == "_hidden");
	// empty and nullptr degrade to "item"
	TFPASS(UT_sanitizeFileName("") == "item");
	TFPASS(UT_sanitizeFileName(nullptr) == "item");
	// UTF-8 survives untouched
	TFPASS(UT_sanitizeFileName("b\xC3\xA9p.tex") == "b\xC3\xA9p.tex");
}

TFTEST_MAIN("file predicates on real files")
{
	// /tmp always exists as a directory
	TFPASS(UT_directoryExists("/tmp"));
	TFPASS(!UT_directoryExists("/tmp/definitely-not-a-real-dir-xyz"));
	TFPASS(!UT_directoryExists("/dev/null"));

	std::string tmp = UT_createTmpFile("ut_path_pred", ".tmp");
	TFPASS(!tmp.empty());
	{
		UT_FilePtr f(fopen(tmp.c_str(), "w"));
		TFPASS(f != nullptr);
		TFPASS(fputs("0123456789", f.get()) >= 0);
	}
	TFPASS(UT_isRegularFile(tmp.c_str()));
	TFPASS(UT_fileSize(tmp.c_str()) == 10);
	TFPASS(UT_mTime(tmp.c_str()) > 0);
	TFPASS(!UT_directoryExists(tmp.c_str()));

	TFPASS(!UT_isRegularFile("/tmp/definitely-not-a-real-file-xyz"));
	unlink(tmp.c_str());
}

TFTEST_MAIN("UT_go_file uri helpers")
{
	TFPASS(UT_go_path_is_uri("file:///tmp/x") == TRUE);
	TFPASS(UT_go_path_is_uri("https://example.com/x") == TRUE);
	TFPASS(UT_go_path_is_uri("/tmp/x") == FALSE);

	UT_GFreePtr<char> uri(UT_go_filename_to_uri("/tmp/ut_path_test file"));
	TFPASS(uri != nullptr);
	TFPASS(strncmp(uri.get(), "file://", 7) == 0);
	// round-trips back to the same path (escaping undone)
	UT_GFreePtr<char> unuri(UT_go_filename_from_uri(uri.get()));
	TFPASS(unuri != nullptr);
	TFPASS(strcmp(unuri.get(), "/tmp/ut_path_test file") == 0);

	UT_GFreePtr<char> back(UT_go_filename_from_uri("file:///tmp/x.txt"));
	TFPASS(back != nullptr);
	TFPASS(strcmp(back.get(), "/tmp/x.txt") == 0);

	TFPASS(UT_go_url_is_local("file:///etc/passwd") == TRUE);
	TFPASS(UT_go_url_is_local("/etc/passwd") == TRUE);
	TFPASS(UT_go_url_is_local("https://example.com/x") == FALSE);
}

TFTEST_MAIN("UT_createTmpFile creates distinct real files")
{
	std::string a = UT_createTmpFile("ut_path_test", ".tmp");
	std::string b = UT_createTmpFile("ut_path_test", ".tmp");

	TFPASS(!a.empty());
	TFPASS(!b.empty());
	TFPASS(a != b);
	TFPASS(UT_isRegularFile(a.c_str()));
	TFPASS(UT_isRegularFile(b.c_str()));
	TFPASS(a.find("ut_path_test") != std::string::npos);

	unlink(a.c_str());
	unlink(b.c_str());
}

TFTEST_MAIN("UT_go_file_remove_recursive")
{
	// build a small tree under a tmpdir and remove it recursively
	char tmpl[] = "/tmp/ut_path_rr_XXXXXX";
	char * dir = mkdtemp(tmpl);
	TFPASS(dir != nullptr);

	std::string sub = std::string(dir) + "/sub";
	TFPASS(mkdir(sub.c_str(), 0700) == 0);
	std::string file = sub + "/f.txt";
	UT_FilePtr f(fopen(file.c_str(), "w"));
	TFPASS(f != nullptr);
	f.reset();
	TFPASS(UT_isRegularFile(file.c_str()));

	// takes a URI, not a plain path
	UT_GFreePtr<char> diruri(UT_go_filename_to_uri(dir));
	TFPASS(diruri != nullptr);
	TFPASS(UT_go_file_remove_recursive(diruri.get(), nullptr) == TRUE);
	TFPASS(!UT_directoryExists(dir));
}
