/* AbiSource Program Utilities
 * Copyright (C) 1998-2000 AbiSource, Inc.
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

#include <glib/gstdio.h>

#include "ut_path.h"


UT_UTF8String UT_go_basename(const char* uri)
{
	UT_UTF8String _base_name;
	char *base_name = UT_go_basename_from_uri(uri);
	if(base_name) {
		_base_name = base_name;
		g_free(base_name);
	}
	return _base_name;
}

std::string UT_createTmpFile(const std::string& prefix, const std::string& extension)
{
	// g_file_open_tmp creates the file O_EXCL under an unpredictable
	// name - the old UT_rand-based name was guessable and fopen("w+")
	// follows pre-planted symlinks in the shared tmp dir.
	// NB the template is a BASENAME (a '/' makes it fail outright);
	// g_file_open_tmp puts the file in g_get_tmp_dir() itself.
	std::string tmpl = prefix;
	if (!tmpl.empty() && tmpl.back() != '-')
		tmpl += '-';
	tmpl += "XXXXXX";
	tmpl += extension;

	gchar *filename = nullptr;
	int fd = g_file_open_tmp(tmpl.c_str(), &filename, nullptr);
	if (fd == -1)
		return "";

	g_close(fd, nullptr);
	std::string sName = filename;
	g_free(filename);
	return sName;
}

std::string UT_sanitizeFileName(const char *name)
{
	std::string safe;

	if (name)
	{
		for (const char *p = name; *p; ++p)
		{
			const unsigned char c = static_cast<unsigned char>(*p);
			/* Keep ASCII alnum, a few harmless punctuation chars and
			 * UTF-8 multi-byte sequences; everything else - '/' and
			 * '\\' (path separators), control bytes (CR/LF header
			 * injection), quotes and markup chars (XML attribute
			 * break-out), '%' (URL escapes) and shell-ish metachars -
			 * becomes an underscore. */
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
				|| (c >= '0' && c <= '9') || c >= 0x80
				|| c == '.' || c == '-' || c == '_' || c == '+'
				|| c == ' ')
				safe += static_cast<char>(c);
			else
				safe += '_';
		}
	}

	// a leading '.' would make the name hidden; ".." would traverse up
	if (!safe.empty() && safe[0] == '.')
		safe[0] = '_';

	if (safe.empty())
		safe = "item";

	return safe;
}
