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

#include <string.h>
#include <string>

#include <gsf/gsf-input.h>
#include <gsf/gsf-output.h>
#include <glib/gstdio.h>

#include "tf_test.h"
#include "ut_go_file.h"
#include "ut_raii.h"

#define TFSUITE "core.af.util.go_file"

namespace
{

static std::string take(char * p)
{
	std::string s = p ? p : "(null)";
	g_free(p);
	return s;
}

} // namespace

TFTEST_MAIN("UT_go_path_is_uri / UT_go_path_is_path")
{
	TFPASS(UT_go_path_is_uri("file:///tmp/x") == TRUE);
	TFPASS(UT_go_path_is_uri("http://example.com") == TRUE);
	TFPASS(UT_go_path_is_uri("mailto:a@b.c") == TRUE);
	TFPASS(UT_go_path_is_uri("/tmp/x") == FALSE);
	TFPASS(UT_go_path_is_uri("plain") == FALSE);

	TFPASS(UT_go_path_is_path("/tmp/x") == TRUE);
	TFPASS(UT_go_path_is_path("a/b") == TRUE);
	TFPASS(UT_go_path_is_path("plain") == FALSE);
}

TFTEST_MAIN("UT_go_filename <-> uri conversion")
{
	TFPASS(take(UT_go_filename_to_uri("/tmp/foo bar.txt"))
		   == "file:///tmp/foo%20bar.txt");
	TFPASS(take(UT_go_filename_from_uri("file:///tmp/foo%20bar.txt"))
		   == "/tmp/foo bar.txt");

	// round-trip a nested path
	char * uri = UT_go_filename_to_uri("/a/b/c");
	TFPASS(take(UT_go_filename_from_uri(uri)) == "/a/b/c");
	g_free(uri);
}

TFTEST_MAIN("UT_go_filename_simplify")
{
	// "/./" and "//" collapse
	TFPASS(take(UT_go_filename_simplify("/a/./b//c", UT_GO_DOTDOT_TEST, FALSE))
		   == "/a/b/c");

	// syntactic dotdot removes the previous component outright
	TFPASS(take(UT_go_filename_simplify("/a/b/../c", UT_GO_DOTDOT_SYNTACTIC, FALSE))
		   == "/a/c");

	// leading "/../" collapses to "/"
	TFPASS(take(UT_go_filename_simplify("/../a", UT_GO_DOTDOT_TEST, FALSE))
		   == "/a");

	// DOTDOT_LEAVE keeps ".."
	TFPASS(take(UT_go_filename_simplify("/a/b/../c", UT_GO_DOTDOT_LEAVE, FALSE))
		   == "/a/b/../c");

	// relative input + make_absolute resolves against cwd
	char * cwd = g_get_current_dir();
	std::string expect = std::string(cwd) + "/rel/file";
	g_free(cwd);
	TFPASS(take(UT_go_filename_simplify("rel/file", UT_GO_DOTDOT_TEST, TRUE))
		   == expect);
}

TFTEST_MAIN("UT_go_url_simplify")
{
	// scheme lower-cased; "//" and "/./" collapsed, "/../" left alone
	TFPASS(take(UT_go_url_simplify("HTTP://example.com/a/./b/../c"))
		   == "http://example.com/a/b/../c");
	TFPASS(take(UT_go_url_simplify("https://example.com/x//y"))
		   == "https://example.com/x/y");
	TFPASS(take(UT_go_url_simplify("ftp://h/a"))
		   == "ftp://h/a");

	// file:/// goes through the filename canonicaliser
	TFPASS(take(UT_go_url_simplify("file:///a//b/./c"))
		   == "file:///a/b/c");

	// no recognised scheme -> unchanged apart from scheme case
	TFPASS(take(UT_go_url_simplify("MAILTO:a@b"))
		   == "mailto:a@b");
}

TFTEST_MAIN("UT_go_url_resolve_relative")
{
	// plain relative name replaces last component
	TFPASS(take(UT_go_url_resolve_relative("http://a.com/x/y", "z"))
		   == "http://a.com/x/z");

	// ".." climbs
	TFPASS(take(UT_go_url_resolve_relative("http://a.com/x/y", "../w"))
		   == "http://a.com/w");

	// internal "./" and "../" are normalised
	TFPASS(take(UT_go_url_resolve_relative("http://a.com/x/y", "./p/../q"))
		   == "http://a.com/x/q");

	// absolute path replaces the whole path
	TFPASS(take(UT_go_url_resolve_relative("http://a.com/x/y", "/w"))
		   == "http://a.com/w");

	// "//host" inherits only the scheme
	TFPASS(take(UT_go_url_resolve_relative("http://a.com/x/y", "//b.com/w"))
		   == "http://b.com/w");

	// an already-absolute uri is returned (simplified) unchanged
	TFPASS(take(UT_go_url_resolve_relative("http://a.com/x/y", "https://c.com/w"))
		   == "https://c.com/w");

	// fragment-only keeps the base path
	TFPASS(take(UT_go_url_resolve_relative("http://a.com/x/y", "#frag"))
		   == "http://a.com/x/y#frag");

	// base query+fragment are dropped before resolution
	TFPASS(take(UT_go_url_resolve_relative("http://a.com/x/y?q=1#f", "z"))
		   == "http://a.com/x/z");

	// file: URIs resolve too
	TFPASS(take(UT_go_url_resolve_relative("file:///a/b/c", "d"))
		   == "file:///a/b/d");
}

TFTEST_MAIN("UT_go_url_make_relative")
{
	// sibling file in the same directory -> basename
	TFPASS(take(UT_go_url_make_relative("file:///a/b/c", "file:///a/b/d"))
		   == "c");

	// diverging directories -> ../ hops
	TFPASS(take(UT_go_url_make_relative("file:///a/b/c", "file:///a/x/d"))
		   == "../b/c");

	// deeper path -> more hops
	TFPASS(take(UT_go_url_make_relative("file:///a/c", "file:///a/x/y/d"))
		   == "../../c");

	// http on the same host
	TFPASS(take(UT_go_url_make_relative("http://a.com/x/y", "http://a.com/x/z"))
		   == "y");

	// different schemes or hosts can't be made relative
	TFPASS(UT_go_url_make_relative("file:///a", "http://a.com/x") == nullptr);
	TFPASS(UT_go_url_make_relative("http://a.com/x", "http://b.com/x") == nullptr);

	// scheme case is ignored
	TFPASS(take(UT_go_url_make_relative("FILE:///a/b", "file:///a/c"))
		   == "b");
}

TFTEST_MAIN("UT_go_url_is_local")
{
	TFPASS(UT_go_url_is_local(nullptr) == FALSE);
	TFPASS(UT_go_url_is_local("/tmp/x") == TRUE);
	TFPASS(UT_go_url_is_local("file:///tmp/x") == TRUE);
	TFPASS(UT_go_url_is_local("FILE:///tmp/x") == TRUE);
	TFPASS(UT_go_url_is_local("http://x/y") == FALSE);
	TFPASS(UT_go_url_is_local("https://x/y") == FALSE);
	// no "://" marker -> treated as a plain local path
	TFPASS(UT_go_url_is_local("mailto:a@b") == TRUE);
}

TFTEST_MAIN("UT_go_url_is_safe / UT_go_url_show scheme allowlist")
{
	// degenerate inputs
	TFPASS(UT_go_url_is_safe(nullptr) == FALSE);
	TFPASS(UT_go_url_is_safe("") == FALSE);

	// allowed schemes (case-insensitive)
	TFPASS(UT_go_url_is_safe("http://example.com/x") == TRUE);
	TFPASS(UT_go_url_is_safe("HTTPS://EXAMPLE.COM/") == TRUE);
	TFPASS(UT_go_url_is_safe("ftp://ftp.example.com/f") == TRUE);
	TFPASS(UT_go_url_is_safe("mailto:a@b.c?subject=x") == TRUE);

	// executable-in-disguise and app-launcher schemes
	TFPASS(UT_go_url_is_safe("javascript:alert(1)") == FALSE);
	TFPASS(UT_go_url_is_safe("javascript://host/x") == FALSE);
	TFPASS(UT_go_url_is_safe("data:text/html,<h1>x</h1>") == FALSE);
	TFPASS(UT_go_url_is_safe("smb://srv/share/doc") == FALSE);
	TFPASS(UT_go_url_is_safe("ms-word:ofe|u|http://h/d") == FALSE);
	TFPASS(UT_go_url_is_safe("tel:+1234") == FALSE);
	TFPASS(UT_go_url_is_safe("foo:bar") == FALSE);

	// a partially-matching scheme is still rejected
	TFPASS(UT_go_url_is_safe("httpfoo://example.com") == FALSE);
	TFPASS(UT_go_url_is_safe("nothttp://example.com") == FALSE);

	// malformed file URIs resolve to nothing -> rejected
	TFPASS(UT_go_url_is_safe("file://x") == FALSE);

	// file:// and plain paths share the runnable-target rule
	TFPASS(UT_go_url_is_safe("/tmp/definitely-not-here-XYZ") == TRUE);

	gchar * dir = g_dir_make_tmp("ut_go_url_XXXXXX", nullptr);
	TFPASS(dir != nullptr);
	if (dir)
	{
		std::string p = std::string(dir) + "/payload";

		// regular non-executable file -> openable
		TFPASS(g_file_set_contents(p.c_str(), "x", 1, nullptr) == TRUE);
		g_chmod(p.c_str(), 0644);
		TFPASS(UT_go_url_is_safe((std::string("file://") + p).c_str())
			   == TRUE);
		TFPASS(UT_go_url_is_safe(p.c_str()) == TRUE);

		// executable bit -> the handler would run it
		g_chmod(p.c_str(), 0755);
		TFPASS(UT_go_url_is_safe((std::string("file://") + p).c_str())
			   == FALSE);
		TFPASS(UT_go_url_is_safe(p.c_str()) == FALSE);

		// a .desktop launcher is unsafe even without the exec bit
		g_chmod(p.c_str(), 0644);
		std::string dpath = std::string(dir) + "/runme.desktop";
		TFPASS(g_file_set_contents(dpath.c_str(), "x", 1, nullptr)
			   == TRUE);
		TFPASS(UT_go_url_is_safe(dpath.c_str()) == FALSE);

		// directories carry the search bit but are just openable
		TFPASS(UT_go_url_is_safe((std::string("file://") + dir).c_str())
			   == TRUE);

		// UT_go_url_show refuses disallowed schemes without launching
		GError * err = UT_go_url_show("javascript:alert(1)");
		TFPASS(err != nullptr);
		if (err)
		{
			TFPASS(err->domain == G_IO_ERROR);
			g_error_free(err);
		}

		g_remove(dpath.c_str());
		g_remove(p.c_str());
		g_rmdir(dir);
		g_free(dir);
	}
}

TFTEST_MAIN("UT_go_shell_arg_to_uri / basename / dirname")
{
	TFPASS(take(UT_go_shell_arg_to_uri("/tmp/x")) == "file:///tmp/x");

	// fd: URIs pass through untouched
	TFPASS(take(UT_go_shell_arg_to_uri("fd://3")) == "fd://3");

	// existing file: URI round-trips canonicalised
	TFPASS(take(UT_go_shell_arg_to_uri("file:///tmp/x")) == "file:///tmp/x");

	TFPASS(take(UT_go_basename_from_uri("file:///tmp/foo.txt")) == "foo.txt");
	TFPASS(take(UT_go_basename_from_uri("file:///a/b/")) == "b");

	// dirname: full keeps the file:// scheme, brief strips it
	TFPASS(take(UT_go_dirname_from_uri("file:///tmp/foo.txt", FALSE))
		   == "file:///tmp");
	TFPASS(take(UT_go_dirname_from_uri("file:///tmp/foo.txt", TRUE))
		   == "/tmp");
}

TFTEST_MAIN("UT_go mime type + collate")
{
	TFPASS(take(UT_go_get_mime_type("file:///tmp/x.txt")) == "text/plain");
	TFPASS(take(UT_go_get_mime_type("file:///tmp/x.zzz_no_such_ext"))
		   == "application/octet-stream");

	TFPASS(UT_go_utf8_collate_casefold("a", "B") < 0);
	TFPASS(UT_go_utf8_collate_casefold("b", "B") == 0);
	TFPASS(UT_go_utf8_collate_casefold("C", "a") > 0);
}

TFTEST_MAIN("UT_go file lifecycle on a real tmp tree")
{
	gchar * tmpl = g_strdup("ut_go_file_XXXXXX");
	gchar * dir = g_dir_make_tmp(tmpl, nullptr);
	g_free(tmpl);
	TFPASS(dir != nullptr);
	if (!dir) return;

	std::string duri = std::string("file://") + dir;
	std::string fpath = std::string(dir) + "/a.txt";
	std::string furi = "file://" + fpath;

	// directory_create + file_exists
	TFPASS(UT_go_directory_create((duri + "/sub").c_str(), nullptr) == TRUE);
	TFPASS(UT_go_file_exists((duri + "/sub").c_str()) == TRUE);
	TFPASS(UT_go_file_exists((duri + "/nope").c_str()) == FALSE);

	// create + write + open + read round-trip
	{
		GsfOutput * out = UT_go_file_create(furi.c_str(), nullptr);
		TFPASS(out != nullptr);
		if (out)
		{
			TFPASS(gsf_output_puts(out, "hello world"));
			TFPASS(gsf_output_close(out));
			g_object_unref(out);
		}
	}
	TFPASS(UT_go_file_exists(furi.c_str()) == TRUE);
	{
		GsfInput * in = UT_go_file_open(furi.c_str(), nullptr);
		TFPASS(in != nullptr);
		if (in)
		{
			TFPASS(gsf_input_size(in) == 11);
			guint8 const * data = gsf_input_read(in, 11, nullptr);
			TFPASS(data != nullptr && memcmp(data, "hello world", 11) == 0);
			g_object_unref(in);
		}
	}

	// plain filesystem paths also work for open (falls back with a warning)
	{
		GsfInput * in = UT_go_file_open(fpath.c_str(), nullptr);
		TFPASS(in != nullptr);
		if (in) g_object_unref(in);
	}

	// permissions: a normal file is owner read+write
	UT_GOFilePermissions * perms = UT_go_get_file_permissions(furi.c_str());
	TFPASS(perms != nullptr);
	if (perms)
	{
		TFPASS(perms->owner_read == TRUE);
		TFPASS(perms->owner_write == TRUE);
		g_free(perms);
	}
	// nonexistent -> nullptr
	TFPASS(UT_go_get_file_permissions((duri + "/nope").c_str()) == nullptr);

	// dates: just-written file has all three timestamps set
	TFPASS(UT_go_file_get_date_modified(furi.c_str()) > 0);
	TFPASS(UT_go_file_get_date_accessed(furi.c_str()) > 0);
	TFPASS(UT_go_file_get_date_changed(furi.c_str()) > 0);
	TFPASS(UT_go_file_get_date_modified((duri + "/nope").c_str()) == -1);

	// remove a file, then the whole tree
	TFPASS(UT_go_file_remove(furi.c_str(), nullptr) == TRUE);
	TFPASS(UT_go_file_exists(furi.c_str()) == FALSE);

	// remove_recursive deletes a populated tree
	FILE * fp = fopen((std::string(dir) + "/sub/inner.txt").c_str(), "w");
	if (fp) { fputs("x", fp); fclose(fp); }
	TFPASS(UT_go_file_remove_recursive(duri.c_str(), nullptr) == TRUE);
	TFPASS(UT_go_file_exists(duri.c_str()) == FALSE);
	g_free(dir);
}
