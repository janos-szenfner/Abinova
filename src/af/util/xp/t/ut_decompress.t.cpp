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
#include <zlib.h>

#include "tf_test.h"
#include "ut_decompress.h"

#define TFSUITE "core.af.util.decompress"

namespace
{

constexpr int BLK = 512;

// minimal ustar header; only the fields UT_untgz reads need to be right
static void tarHeader(char * blk, const char * name, int size, char typeflag)
{
	memset(blk, 0, BLK);
	// name (offset 0, 100 bytes)
	snprintf(blk, 100, "%s", name);
	// mode / uid / gid / mtime fields may stay zeroed; size is octal
	snprintf(blk + 124, 12, "%011o", size);
	blk[156] = typeflag;
	// magic "ustar"
	memcpy(blk + 257, "ustar", 5);
}

// writes a .tar.gz containing (name,data) entries then two end blocks
static std::string makeTgz(const char * dir,
						   const char * entries[][2], int n)
{
	std::string path = std::string(dir) + "/test.tar.gz";
	gzFile gz = gzopen(path.c_str(), "wb");
	if (!gz)
		return "";

	char blk[BLK];
	for (int i = 0; i < n; i++)
	{
		int len = static_cast<int>(strlen(entries[i][1]));
		tarHeader(blk, entries[i][0], len, '0');
		gzwrite(gz, blk, BLK);
		// data, padded to a 512 multiple
		char data[BLK];
		memset(data, 0, BLK);
		memcpy(data, entries[i][1], len);
		gzwrite(gz, data, ((len + BLK - 1) / BLK) * BLK);
	}
	// two all-zero blocks end the archive
	memset(blk, 0, BLK);
	gzwrite(gz, blk, BLK);
	gzwrite(gz, blk, BLK);
	gzclose(gz);
	return path;
}

} // namespace

TFTEST_MAIN("UT_untgz extracts a member to memory and disk")
{
	gchar * tmpl = g_strdup("ut_untgz_XXXXXX");
	gchar * dir = g_dir_make_tmp(tmpl, nullptr);
	g_free(tmpl);
	TFPASS(dir != nullptr);
	if (!dir) return;

	const char * entries[][2] = {
		{"dir/nested/ignored.txt", "not me"},
		{"some/path/wanted.txt",   "the wanted payload"},
		{"later.txt",              "tail"},
	};
	std::string tgz = makeTgz(dir, entries, 3);
	TFPASS(!tgz.empty());

	// names are compared basename-only; path components are stripped
	char * buf = nullptr;
	int size = -1;
	std::string destdir = std::string(dir) + "/out";
	g_mkdir(destdir.c_str(), 0700);
	TFPASS(UT_untgz(tgz.c_str(), "wanted.txt", destdir.c_str(),
					&buf, &size) == 0);
	TFPASS(buf != nullptr);
	TFPASS(size == 18);
	if (buf)
		TFPASS(memcmp(buf, "the wanted payload", 18) == 0);

	// the file was also written to dest dir under its basename
	std::string extracted = destdir + "/wanted.txt";
	TFPASS(g_file_test(extracted.c_str(), G_FILE_TEST_EXISTS));
	FILE * f = fopen(extracted.c_str(), "rb");
	if (f)
	{
		char disk[20] = {0};
		fread(disk, 1, 19, f);
		fclose(f);
		TFPASS(memcmp(disk, "the wanted payload", 19) == 0);
	}

	// retBuf is freed before reuse
	g_free(buf);
	buf = nullptr;

	// wanted file not present -> success but nothing produced
	size = -1;
	TFPASS(UT_untgz(tgz.c_str(), "absent.txt", destdir.c_str(),
					&buf, &size) == 0);
	TFPASS(buf == nullptr);
	TFPASS(size == -1);

	// extraction without a dest dir still fills retBuf
	size = 0;
	TFPASS(UT_untgz(tgz.c_str(), "later.txt", nullptr, &buf, &size) == 0);
	TFPASS(buf != nullptr);
	TFPASS(size == 4);
	g_free(buf);

	// nonexistent archive -> error
	TFPASS(UT_untgz("/no/such/file.tgz", "x", nullptr, nullptr, nullptr) != 0);

	g_remove(extracted.c_str());
	g_rmdir(destdir.c_str());
	g_remove(tgz.c_str());
	g_rmdir(dir);
	g_free(dir);
}
