 /* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * go-file.c :
 *
 * Copyright (C) 2009, 2013-2014 Hubert Figuiere <hub@figuiere.net>.
 *     Whose contributions are under GPLv2+
 * Copyright (C) 2004 Morten Welinder (terra@gnome.org)
 * Copyright (C) 2003, Red Hat, Inc.
 * Copyright (C) 2025-2026 Abinova contributors
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ut_go_file.h"
#include "ut_raii.h"
#include <glib/gstdio.h>
#include <libxml/encoding.h>


#include <stdio.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>


#if defined G_OS_WIN32
#include <windows.h>
#include <shellapi.h>
#include <io.h>
#include <fcntl.h>
#include <bcrypt.h>
#endif

#include <string>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
  #ifndef S_ISDIR
  #define S_ISDIR(m) (((m) & _S_IFMT ) == _S_IFDIR)
  #endif
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#if defined(__linux__) || defined(__GLIBC__)
#include <sys/random.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__FreeBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#endif

#include <errno.h>

#include "ut_types.h"

#ifndef _
#define _(X) X
#endif

/* ------------------------------------------------------------------------- */

/* TODO: push this up into libgsf proper */

GsfInput *
gsf_input_memory_new_from_file (FILE * input)
{
	GsfOutput *memory_output;
	GsfInput  *memory_input = nullptr;

	g_return_val_if_fail (input != nullptr, nullptr);

	memory_output = gsf_output_memory_new ();
	while (TRUE) {
		guint8 buf[1024];
		size_t nread;
		gboolean res;

		nread = fread (buf, 1, sizeof(buf), input);
		res = gsf_output_write (memory_output, nread, buf);

		if (ferror (input) || !res) {
			/* trouble reading from @input or trouble writing to @memory_output */
			g_object_unref (G_OBJECT (memory_output));
			return nullptr;
		}
		else if ((nread < sizeof(buf)) && feof (input)) /* hit eof */
			break;
	}

	if (gsf_output_close (memory_output))
		memory_input = gsf_input_memory_new_clone (gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (memory_output)),
							   gsf_output_size (memory_output));

	g_object_unref (G_OBJECT (memory_output));

	return memory_input;
}

/* ------------------------------------------------------------------------- */

/* TODO: push this up into libgsf proper */

#define GSF_OUTPUT_PROXY_TYPE	(gsf_output_proxy_get_type ())
#define GSF_OUTPUT_PROXY(o)	(G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_OUTPUT_PROXY_TYPE, GsfOutputProxy))
#define GSF_IS_OUTPUT_PROXY(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_OUTPUT_PROXY_TYPE))

struct GsfOutputProxy {
	GsfOutput output;
	GsfOutput *memory_output;
	GsfOutput *sink;
};

GType gsf_output_proxy_get_type      (void) G_GNUC_CONST;
void  gsf_output_proxy_register_type (GTypeModule *module);

GsfOutput *gsf_output_proxy_new      (GsfOutput * sink);

enum {
	PROP_0,
	PROP_SINK
};

static GsfOutputClass *parent_class;

struct GsfOutputProxyClass {
	GsfOutputClass output_class;
};

/**
 * gsf_output_proxy_new :
 *
 * Returns a new file or nullptr.
 **/
GsfOutput *
gsf_output_proxy_new (GsfOutput * sink)
{
	g_return_val_if_fail (sink != nullptr, nullptr);
	g_return_val_if_fail (GSF_IS_OUTPUT (sink), nullptr);

	return static_cast<GsfOutput *>(g_object_new (GSF_OUTPUT_PROXY_TYPE, "sink", sink, static_cast<void *>(nullptr)));
}

static gboolean
gsf_output_proxy_close (GsfOutput *object)
{
	GsfOutputProxy *proxy = reinterpret_cast<GsfOutputProxy *>(object);

	if(gsf_output_close (proxy->memory_output))
		{
			const guint8 *bytes;
			size_t num_bytes;

			bytes = gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (proxy->memory_output));
			num_bytes = gsf_output_size (proxy->memory_output);

			if (gsf_output_write (proxy->sink, num_bytes, bytes))
				return gsf_output_close (proxy->sink);
		}

	return FALSE;
}

static void
gsf_output_proxy_finalize (GObject *object)
{
	GsfOutputProxy *proxy = reinterpret_cast<GsfOutputProxy *>(object);
	
	g_object_unref (proxy->memory_output);
	g_object_unref (proxy->sink);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gsf_output_proxy_seek (GsfOutput *object,
		       gsf_off_t offset,
		       GSeekType whence)
{
	GsfOutputProxy *proxy = reinterpret_cast<GsfOutputProxy *>(object);

	return gsf_output_seek (proxy->memory_output, offset, whence);
}


static gboolean
gsf_output_proxy_write (GsfOutput *object,
			size_t num_bytes,
			guint8 const *buffer)
{
	GsfOutputProxy *proxy = reinterpret_cast<GsfOutputProxy *>(object);
	
	return gsf_output_write (proxy->memory_output, num_bytes, buffer);
}

static gsf_off_t gsf_output_proxy_vprintf (GsfOutput *object,
					  char const *format, va_list args) G_GNUC_PRINTF (2, 0);

static gsf_off_t
gsf_output_proxy_vprintf (GsfOutput *object, char const *format, va_list args)
{
	GsfOutputProxy *proxy = reinterpret_cast<GsfOutputProxy *>(object);

	return gsf_output_vprintf (proxy->memory_output, format, args);
}

static void
gsf_output_proxy_get_property (GObject     *object,
			       guint        property_id,
			       GValue      *value,
			       GParamSpec  *pspec)
{
	GsfOutputProxy *proxy = reinterpret_cast<GsfOutputProxy *>(object);

	switch (property_id) {
	case PROP_SINK:
		g_value_set_object (value, proxy->sink);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gsf_output_proxy_set_sink (GsfOutputProxy *proxy, GsfOutput *sink)
{
	g_return_if_fail (GSF_IS_OUTPUT (sink));
	g_object_ref (sink);
	if (proxy->sink)
		g_object_unref (proxy->sink);
	proxy->sink = sink;
}

static void
gsf_output_proxy_set_property (GObject      *object,
			       guint         property_id,
			       GValue const *value,
			       GParamSpec   *pspec)
{
	GsfOutputProxy *proxy = reinterpret_cast<GsfOutputProxy *>(object);

	switch (property_id) {
	case PROP_SINK:
		gsf_output_proxy_set_sink (proxy, static_cast<GsfOutput *>(g_value_get_object (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gsf_output_proxy_init (GObject *object, gpointer)
{
	GsfOutputProxy *proxy = reinterpret_cast<GsfOutputProxy *>(object);

	proxy->memory_output = gsf_output_memory_new ();
	proxy->sink = nullptr;
}

static void
gsf_output_proxy_class_init (GObjectClass *gobject_class, gpointer)
{
	GsfOutputClass *output_class = GSF_OUTPUT_CLASS (gobject_class);
	
	gobject_class->finalize = gsf_output_proxy_finalize;
	gobject_class->set_property = gsf_output_proxy_set_property;
	gobject_class->get_property = gsf_output_proxy_get_property;
	output_class->Close     = gsf_output_proxy_close;
	output_class->Seek      = gsf_output_proxy_seek;
	output_class->Write     = gsf_output_proxy_write;
	output_class->Vprintf   = gsf_output_proxy_vprintf;

	g_object_class_install_property
		(gobject_class,
		 PROP_SINK,
		 g_param_spec_object ("sink", "Sink",
				      "Where the converted data is written.",
				      GSF_OUTPUT_TYPE,
				      static_cast<GParamFlags>((GSF_PARAM_STATIC |
						    G_PARAM_READWRITE |
						    G_PARAM_CONSTRUCT_ONLY))));

	parent_class = GSF_OUTPUT_CLASS (g_type_class_peek_parent (gobject_class));
}

/* GSF_DYNAMIC_CLASS once we move this back into libgsf */
GSF_CLASS (GsfOutputProxy, gsf_output_proxy,
	   gsf_output_proxy_class_init, gsf_output_proxy_init,
	   GSF_OUTPUT_TYPE)

/* ------------------------------------------------------------------------- */

static gboolean
is_fd_uri (const char *uri, int *fd);

/* ------------------------------------------------------------------------- */

/*
 * Return TRUE if @path represents a URI, false if not
 */
gboolean 
UT_go_path_is_uri (const char * path)
{
	// hack until i come up with a better test
	if (g_str_has_prefix (path, "mailto:"))
		return TRUE;
	else
		return (strstr (path, "://") != nullptr);
}

gboolean UT_go_path_is_path (const char * path)
{
	// G_IS_DIR_SEPARATOR covers both '/' and '\\' on Windows
	for (const char *p = path; *p; ++p)
		if (G_IS_DIR_SEPARATOR (*p))
			return TRUE;
	return FALSE;
}

/*
 * Convert an escaped URI into a filename.
 */
char *
UT_go_filename_from_uri (const char *uri)
{
	return g_filename_from_uri (uri, nullptr, nullptr);
}

/*
 * Convert a filename into an escaped URI.
 */
char *
UT_go_filename_to_uri (const char *filename)
{
	char *simp, *uri;

	g_return_val_if_fail (filename != nullptr, nullptr);

	simp = UT_go_filename_simplify (filename, UT_GO_DOTDOT_TEST, TRUE);

	uri = g_filename_to_uri (simp, nullptr, nullptr);
	g_free (simp);
	return uri;
}

char *
UT_go_filename_simplify (const char *filename, UT_GODotDot dotdot,
		      gboolean make_absolute)
{
	char *simp, *p, *q;

	g_return_val_if_fail (filename != nullptr, nullptr);

	if (make_absolute && !g_path_is_absolute (filename)) {
		/*
		 * FIXME: this probably does not work for "c:foo" on
		 * Win32.
		 */
		char *current_dir = g_get_current_dir ();
		simp = g_build_filename (current_dir, filename, nullptr);
		g_free (current_dir);
	} else
		simp = g_strdup (filename);

	for (p = q = simp; *p;) {
		if (p != simp &&
		    G_IS_DIR_SEPARATOR (p[0]) &&
		    G_IS_DIR_SEPARATOR (p[1])) {
			/* "//" --> "/", except initially.  */
			p++;
			continue;
		}

		if (G_IS_DIR_SEPARATOR (p[0]) &&
		    p[1] == '.' &&
		    G_IS_DIR_SEPARATOR (p[2])) {
			/* "/./" -> "/".  */
			p += 2;
			continue;
		}

		if (G_IS_DIR_SEPARATOR (p[0]) &&
		    p[1] == '.' &&
		    p[2] == '.' &&
		    G_IS_DIR_SEPARATOR (p[3])) {
			if (p == simp) {
				/* "/../" --> "/" initially.  */
				p += 3;
				continue;
			} else if (p == simp + 1) {
				/* Nothing, leave "//../" initially alone.  */
			} else {
				/*
				 * "prefix/dir/../" --> "prefix/" if
				 * "dir" is an existing directory (not
				 * a symlink).
				 */
				gboolean isdir;

				switch (dotdot) {
				case UT_GO_DOTDOT_SYNTACTIC:
					isdir = TRUE;
					break;
				case UT_GO_DOTDOT_TEST: {
#if GLIB_CHECK_VERSION(2,26,0) || defined(G_OS_WIN32)
					GStatBuf statbuf;
#else
					struct stat statbuf;
#endif
					char savec = *q;
					/*
					 * Terminate the path so far so we can
					 * it.  Restore because "p" loops over
					 * the same.
					 */
					*q = 0;
					isdir = (g_lstat (simp, &statbuf) == 0) &&
						S_ISDIR (statbuf.st_mode);
					*q = savec;
					break;
				}
				default:
					isdir = FALSE;
					break;
				}

				if (isdir) {
					do {
						g_assert (q != simp);
						q--;
					} while (!G_IS_DIR_SEPARATOR (*q));
					p += 3;
					continue;
				} else {
					/*
					 * Do nothing.
					 *
					 * Maybe the prefix does not
					 * exist, or maybe it is not
					 * a directory (for example
					 * because it is a symlink).
					 */
				}
			}
		}

		*q++ = *p++;
	}
	*q = 0;

	return simp;
}

/*
 * Simplify a potentially non-local path using only slashes.
 */
static char *
simplify_path (const char *uri)
{
	char *simp, *p, *q;

	simp = g_strdup (uri);

	for (p = q = simp; *p;) {
		if (p[0] == '/' && p[1] == '/') {
			/* "//" --> "/".  */
			p++;
			continue;
		}

		if (p[0] == '/' && p[1] == '.' && p[2] == '/') {
			/* "/./" -> "/".  */
			p += 2;
			continue;
		}

		if (p[0] == '/' && p[1] == '.' && p[2] == '.' && p[3] == '/') {
			if (p == simp) {
				/* "/../" --> "/" initially.  */
				p += 3;
				continue;
			} else {
				/* Leave alone */
			}
		}

		*q++ = *p++;
	}
	*q = 0;

	return simp;
}

static char *
simplify_host_path (const char *uri, size_t hstart)
{
	const char *slash = strchr (uri + hstart, '/');
	char *simp, *psimp;
	size_t pstart;

	if (!slash)
		return g_strdup (uri);

	pstart = slash + 1 - uri;
	psimp = simplify_path (slash + 1);
	simp = g_new (char, pstart + strlen (psimp) + 1);
	memcpy (simp, uri, pstart);
	strcpy (simp + pstart, psimp);
	g_free (psimp);
	return simp;
}

char *
UT_go_url_simplify (const char *uri)
{
	char *simp, *p;

	g_return_val_if_fail (uri != nullptr, nullptr);

	if (g_ascii_strncasecmp (uri, "file:///", 8) == 0) {
		char *filename = UT_go_filename_from_uri (uri);
		simp = filename ? UT_go_filename_to_uri (filename) : nullptr;
		g_free (filename);
		return simp;
	}

	if (g_ascii_strncasecmp (uri, "http://", 7) == 0)
		simp = simplify_host_path (uri, 7);
	else if (g_ascii_strncasecmp (uri, "https://", 8) == 0)
		simp = simplify_host_path (uri, 8);
	else if (g_ascii_strncasecmp (uri, "ftp://", 6) == 0)
		simp = simplify_host_path (uri, 6);
	else
		simp = g_strdup (uri);

	/* Lower-case protocol name.  */
	for (p = simp; g_ascii_isalpha (*p); p++)
		*p = g_ascii_tolower (*p);

	return simp;
}

/* code borrowed from gnome-vfs' gnome-vfs-uri.c */

static gboolean
is_uri_relative (const char *uri)
{
        const char *current;

        /* RFC 2396 section 3.1 */
        for (current = uri ;
                *current
		     &&      ((*current >= 'a' && *current <= 'z')
			      || (*current >= 'A' && *current <= 'Z')
			      || (*current >= '0' && *current <= '9')
			      || ('-' == *current)
			      || ('+' == *current)
			      || ('.' == *current)) ;
             current++) {
	}

        return  !(':' == *current);
}

static void
remove_internal_relative_components (char *uri_current)
{
	char *segment_prev, *segment_cur;
	gsize len_prev, len_cur;

	len_prev = len_cur = 0;
	segment_prev = nullptr;

	segment_cur = uri_current;

	while (*segment_cur) {
		len_cur = strcspn (segment_cur, "/");

		if (len_cur == 1 && segment_cur[0] == '.') {
			/* Remove "." 's */
			if (segment_cur[1] == '\0') {
				segment_cur[0] = '\0';
				break;
			} else {
				memmove (segment_cur, segment_cur + 2, strlen (segment_cur + 2) + 1);
				continue;
			}
		} else if (len_cur == 2 && segment_cur[0] == '.' && segment_cur[1] == '.' ) {
			/* Remove ".."'s (and the component to the left of it) that aren't at the
			 * beginning or to the right of other ..'s
			 */
			if (segment_prev) {
				if (! (len_prev == 2
				       && segment_prev[0] == '.'
				       && segment_prev[1] == '.')) {
				       	if (segment_cur[2] == '\0') {
						segment_prev[0] = '\0';
						break;
				       	} else {
						memmove (segment_prev, segment_cur + 3, strlen (segment_cur + 3) + 1);

						segment_cur = segment_prev;
						len_cur = len_prev;

						/* now we find the previous segment_prev */
						if (segment_prev == uri_current) {
							segment_prev = nullptr;
						} else if (segment_prev - uri_current >= 2) {
							segment_prev -= 2;
							for ( ; segment_prev > uri_current && segment_prev[0] != '/' 
							      ; segment_prev-- ) {
							}
							if (segment_prev[0] == '/') {
								segment_prev++;
							}
						}
						continue;
					}
				}
			}
		}

		/*Forward to next segment */

		if (segment_cur [len_cur] == '\0') {
			break;
		}
		 
		segment_prev = segment_cur;
		len_prev = len_cur;
		segment_cur += len_cur + 1;	
	}
	
}

static char *
make_full_uri_from_relative (const char *base_uri, const char *uri)
{
	char *result = nullptr;

	char *mutable_base_uri;
	char *mutable_uri;
	
	char *uri_current;
	gsize base_uri_length;
	char *separator;
	
	/* We may need one extra character
	 * to append a "/" to uri's that have no "/"
	 * (such as help:)
	 */

	mutable_base_uri = static_cast<char *>(g_malloc(strlen(base_uri)+2));
	strcpy (mutable_base_uri, base_uri);
		
	uri_current = mutable_uri = g_strdup (uri);

	/* Chew off Fragment and Query from the base_url */

	separator = strrchr (mutable_base_uri, '#'); 

	if (separator) {
		*separator = '\0';
	}

	separator = strrchr (mutable_base_uri, '?');

	if (separator) {
		*separator = '\0';
	}

	if ('/' == uri_current[0] && '/' == uri_current [1]) {
		/* Relative URI's beginning with the authority
		 * component inherit only the scheme from their parents
		 */

		separator = strchr (mutable_base_uri, ':');

		if (separator) {
			separator[1] = '\0';
		}			  
	} else if ('/' == uri_current[0]) {
		/* Relative URI's beginning with '/' absolute-path based
		 * at the root of the base uri
		 */

		separator = strchr (mutable_base_uri, ':');

		/* g_assert (separator), really */
		if (separator) {
			/* If we start with //, skip past the authority section */
			if ('/' == separator[1] && '/' == separator[2]) {
				separator = strchr (separator + 3, '/');
				if (separator) {
					separator[0] = '\0';
				}
			} else {
				/* If there's no //, just assume the scheme is the root */
				separator[1] = '\0';
			}
		}
	} else if ('#' != uri_current[0]) {
		/* Handle the ".." convention for relative uri's */

		/* If there's a trailing '/' on base_url, treat base_url
		 * as a directory path.
		 * Otherwise, treat it as a file path, and chop off the filename
		 */

		base_uri_length = strlen (mutable_base_uri);
		if (base_uri_length > 0 &&
		    '/' == mutable_base_uri[base_uri_length-1]) {
			/* Trim off '/' for the operation below */
			mutable_base_uri[base_uri_length-1] = 0;
		} else {
			separator = strrchr (mutable_base_uri, '/');
			if (separator) {
				/* Make sure we don't eat a domain part */
				char *tmp = separator - 1;
				if ((separator != mutable_base_uri) && (*tmp != '/')) {
					*separator = '\0';
				} else {
					/* Maybe there is no domain part and this is a toplevel URI's child */
					char *tmp2 = strstr (mutable_base_uri, ":///");
					if (tmp2 != nullptr && tmp2 + 3 == separator) {
						*(separator + 1) = '\0';
					}
				}
			}
		}

		remove_internal_relative_components (uri_current);

		/* handle the "../"'s at the beginning of the relative URI */
		while (0 == strncmp ("../", uri_current, 3)) {
			uri_current += 3;
			separator = strrchr (mutable_base_uri, '/');
			if (separator) {
				*separator = '\0';
			} else {
				/* <shrug> */
				break;
			}
		}

		/* handle a ".." at the end */
		if (uri_current[0] == '.' && uri_current[1] == '.' 
		    && uri_current[2] == '\0') {

			uri_current += 2;
			separator = strrchr (mutable_base_uri, '/');
			if (separator) {
				*separator = '\0';
			}
		}

		/* Re-append the '/' */
		mutable_base_uri [strlen(mutable_base_uri)+1] = '\0';
		mutable_base_uri [strlen(mutable_base_uri)] = '/';
	}

	result = g_strconcat (mutable_base_uri, uri_current, nullptr);
	g_free (mutable_base_uri); 
	g_free (mutable_uri); 
	
	return result;
}

/*
 * More or less the same as gnome_vfs_uri_make_full_from_relative.
 */
char *
UT_go_url_resolve_relative (const char *ref_uri, const char *rel_uri)
{
	char *simp, *uri;

	g_return_val_if_fail (rel_uri != nullptr, nullptr);

	if (is_uri_relative (rel_uri)) {
		g_return_val_if_fail (ref_uri != nullptr, nullptr);
		uri = make_full_uri_from_relative (ref_uri, 
						   rel_uri);
	} else {
		uri = g_strdup (rel_uri);
	}

	simp = UT_go_url_simplify (uri);
	g_free (uri);
	return simp;
}

/*
 * UT_go_url_is_local :
 * @uri : a URI or plain path.
 *
 * TRUE if @uri names a local file, i.e. a plain (scheme-less) path or a
 * file:// URI.  Any other scheme (http, https, ftp, smb, ...) names a remote
 * resource that UT_go_file_open would fetch over the network - importers
 * must not do that merely because a document referenced it (SSRF /
 * tracking-pixel risk).
 */
gboolean
UT_go_url_is_local (const char *uri)
{
	if (uri == nullptr)
		return FALSE;

	/* "scheme://" marks a URI; no marker means a plain local path. */
	if (strstr (uri, "://") == nullptr)
		return TRUE;

	return g_ascii_strncasecmp (uri, "file://", 7) == 0;
}

static char *
make_rel (const char *uri, const char *ref_uri,
	  const char *uri_host, const char *slash)
{
	const char *p, *q;
	int n;
	GString *res;

	if (!slash)
		return nullptr;

	if (uri_host != nullptr &&
	    strncmp (uri_host, ref_uri + (uri_host - uri), slash - uri_host))
		return nullptr;

	for (p = slash; *p; p++) {
		if (*p != ref_uri[p - uri])
			break;
		else if (*p == '/')
			slash = p;
	}
	/* URI components agree until slash.  */

	/* The number of "../" needed equals the number of '/' in the
	   reference URI after the shared component (its directory depth),
	   not in the target URI.  */
	n = 0;
	q = ref_uri + (slash - uri);
	while (1) {
		q = strchr (q + 1, '/');
		if (q)
			n++;
		else
			break;
	}

	res = g_string_new (nullptr);
	while (n-- > 0)
		g_string_append (res, "../");
	g_string_append (res, slash + 1);
	return g_string_free (res, FALSE);
}

char *
UT_go_url_make_relative (const char *uri, const char *ref_uri)
{
	int i;

	/* Check that protocols are the same.  */
	for (i = 0; 1; i++) {
		char c = uri[i];
		char rc = ref_uri[i];

		if (c == 0)
			return nullptr;

		if (c == ':') {
			if (rc == ':')
				break;
			return nullptr;
		}

		if (g_ascii_tolower (c) != g_ascii_tolower (rc))
			return nullptr;
	}

	if (g_ascii_strncasecmp (uri, "file:///", 8) == 0)
		return make_rel (uri, ref_uri, nullptr, uri + 7);  /* Yes, 7.  */

	if (g_ascii_strncasecmp (uri, "http://", 7) == 0)
		return make_rel (uri, ref_uri, uri + 7, strchr (uri + 7, '/'));

	if (g_ascii_strncasecmp (uri, "https://", 8) == 0)
		return make_rel (uri, ref_uri, uri + 8, strchr (uri + 8, '/'));

	if (g_ascii_strncasecmp (uri, "ftp://", 6) == 0)
		return make_rel (uri, ref_uri, uri + 6, strchr (uri + 6, '/'));

	return nullptr;
}

/*
 * Convert a shell argv entry (assumed already translated into filename
 * encoding) to an escaped URI.
 */
char *
UT_go_shell_arg_to_uri (const char *arg)
{
	gchar *tmp;

	if (is_fd_uri (arg, nullptr))
		return g_strdup (arg);

	if (g_path_is_absolute (arg) || strchr (arg, ':') == nullptr)
		return UT_go_filename_to_uri (arg);

	tmp = UT_go_filename_from_uri (arg);
	if (tmp) {
		/*
		 * Do the reverse translation to get a minimum of
		 * canonicalization.
		 */
		char *res = UT_go_filename_to_uri (tmp);
		g_free (tmp);
		return res;
	}

#if !defined(WITH_GSF_INPUT_HTTP)
	{
		GFile *f = g_file_new_for_commandline_arg (arg);
		char *uri = g_file_get_uri (f);
		g_object_unref (G_OBJECT (f));
		if (uri) {
			char *uri2 = UT_go_url_simplify(uri);
			g_free (uri);
			return uri2;
		}
	}
#else
	{
		if (g_ascii_strncasecmp (arg, "http://", strlen ("http://")) == 0) {
			return UT_go_url_simplify (arg);
		}
	}
#endif

	/* Just assume it's a filename.  */
	return UT_go_filename_to_uri (arg);
}

/**
 * UT_go_basename_from_uri:
 * @uri :
 *
 * Decode the final path component.  Returns as UTF-8 encoded suitable
 * for display.
 **/
char *
UT_go_basename_from_uri (const char *uri)
{
	char *res;

	GFile *f = g_file_new_for_uri (uri);
	char *basename = g_file_get_basename (f);
	g_object_unref (G_OBJECT (f));

	res = basename ? g_filename_display_name (basename) : nullptr;
	g_free (basename);
	return res;
}

/**
 * UT_go_dirname_from_uri:
 * @uri :
 * @brief: if TRUE, hide "file://" if present.
 *
 * Decode the all but the final path component.  Returns as UTF-8 encoded
 * suitable for display.
 **/
char *
UT_go_dirname_from_uri (const char *uri, gboolean brief)
{
	char *dirname_utf8, *dirname;

	char *uri_dirname = g_path_get_dirname (uri);
	dirname = uri_dirname ? UT_go_filename_from_uri (uri_dirname) : nullptr;
	if(uri_dirname) {
		g_free (uri_dirname);
	}
	uri_dirname = dirname ? g_strconcat ("file://", dirname, nullptr) : nullptr;
	if(dirname) {
		g_free (dirname);
	}
	dirname = uri_dirname;

	if (brief && dirname &&
	    g_ascii_strncasecmp (dirname, "file:///", 8) == 0) {
		char *temp = g_strdup (dirname + 7);
		g_free (dirname);
		dirname = temp;
	}

	dirname_utf8 = dirname ? g_filename_display_name (dirname) : nullptr;
	g_free (dirname);
	return dirname_utf8;
}


gboolean
UT_go_directory_create (char const *uri, GError **error)
{
	GFile *f = g_file_new_for_uri (uri);
	gboolean res = g_file_make_directory (f, nullptr, error);
	g_object_unref (G_OBJECT (f));
	return res;
}

/* ------------------------------------------------------------------------- */

static gboolean
is_fd_uri (const char *uri, int *fd)
{
	unsigned long ul;
	char *end;

	if (g_ascii_strncasecmp (uri, "fd://", 5))
		return FALSE;
	uri += 5;
	if (!g_ascii_isdigit (*uri))
		return FALSE;  /* Space, for example.  */

	ul = strtoul (uri, &end, 10);
	if (*end != 0 || ul > INT_MAX)
		return FALSE;

	if (fd != nullptr)
		*fd = static_cast<int>(ul);
	return TRUE;
}

/* ------------------------------------------------------------------------- */

static GsfInput *
open_plain_file (const char *path, GError **err)
{
	GsfInput *input = gsf_input_mmap_new (path, nullptr);
	if (input != nullptr)
		return input;
	/* Only report error if stdio fails too */
	return gsf_input_stdio_new (path, err);
}

static GsfInput *
UT_go_file_open_impl (char const *uri, GError **err)
{
	char *filename;
	int fd;

	if (err != nullptr)
		*err = nullptr;
	g_return_val_if_fail (uri != nullptr, nullptr);

	if (uri[0] == G_DIR_SEPARATOR) {
		g_warning ("Got plain filename %s in UT_go_file_open.", uri);
		return open_plain_file (uri, err);
	}

	filename = UT_go_filename_from_uri (uri);
	if (filename) {
		GsfInput *result = open_plain_file (filename, err);
		g_free (filename);
		return result;
	}

	if (is_fd_uri (uri, &fd)) {
#if defined G_OS_WIN32
		setmode (fd, O_BINARY);
#endif
		UT_ScopedFD fd2 (dup (fd));
		UT_FilePtr fil (fd2 ? fdopen (fd2.get(), "rb") : nullptr);
		GsfInput *result;

		if (!fil) {
			g_set_error (err, gsf_output_error_id (), 0,
				     "Unable to read from %s", uri);
			return nullptr;
		}
		fd2.release (); /* fil owns the descriptor now */

		/* guarantee that file descriptors will be seekable */
		result = gsf_input_memory_new_from_file (fil.get());

		return result;
	}

	if (!strncmp (uri, "http://", 7) || !strncmp (uri, "https://", 8))
		return gsf_input_http_new (uri, err);

	return gsf_input_gio_new_for_uri (uri, err);
}

/**
 * UT_go_file_open :
 * @uri :
 * @err : #GError
 *
 * Try all available methods to open a file or return an error
 **/
GsfInput *
UT_go_file_open (char const *uri, GError **err)
{
	GsfInput * input;

	input = UT_go_file_open_impl (uri, err);
	if (input != nullptr)
	{
		GsfInput * uncompress = gsf_input_uncompress (input);
		gsf_input_set_name (uncompress, uri);
		return uncompress;
	}
	return nullptr;
}

static GsfOutput *
gsf_output_proxy_create (GsfOutput *wrapped, char const *uri, GError **err)
{
	if (!wrapped) {
		g_set_error (err, gsf_output_error_id (), 0,
			     "Unable to write to %s", uri);
		return nullptr;
	}

	/* the proxy refs the sink itself, so our ref must go */
	UT_GObjPtr<GsfOutput> sink (wrapped);

	/* guarantee that file descriptors will be seekable */
	return gsf_output_proxy_new (wrapped);
}

static GsfOutput *
UT_go_file_create_impl (char const *uri, GError **err)
{
	char *filename;
	int fd;
	g_return_val_if_fail (uri != nullptr, nullptr);

	std::string path = uri;
        bool is_uri = UT_go_path_is_uri(path.c_str());
        bool is_filename = is_uri ? false : path.find_last_of(G_DIR_SEPARATOR) == std::string::npos;
	bool is_path = !is_uri && !is_filename;

	filename = UT_go_filename_from_uri (uri);
	if (is_path || filename) {
		GsfOutput *result = gsf_output_stdio_new (filename?filename:uri, err);
		if(filename) 
			g_free (filename);
		return result;
	}

	if (is_fd_uri (uri, &fd)) {
#if defined G_OS_WIN32
		setmode (fd, O_BINARY);
#endif
		UT_ScopedFD fd2 (dup (fd));
		UT_FilePtr fil (fd2 ? fdopen (fd2.get(), "wb") : nullptr);
		if (fil)
			fd2.release (); /* fil owns the descriptor now */
		GsfOutput *result = fil ? gsf_output_stdio_new_FILE (uri, fil.get(), FALSE) : nullptr;
		if (result)
			/* keep_file=FALSE: the GsfOutput owns and will fclose fil */
			fil.release ();

		/* guarantee that file descriptors will be seekable */
		return gsf_output_proxy_create(result, uri, err);
	}

	return gsf_output_proxy_create(gsf_output_gio_new_for_uri (uri, err), uri, err);
}

GsfOutput *
UT_go_file_create (char const *uri, GError **err)
{
	GsfOutput * output;

	output = UT_go_file_create_impl (uri, err);
	if (output != nullptr)
	{
		gsf_output_set_name (output, uri);
		return output;
	}
	return nullptr;
}

gboolean
UT_go_file_remove (char const *uri, GError ** err)
{
	char *filename;

	g_return_val_if_fail (uri != nullptr, FALSE);

	filename = UT_go_filename_from_uri (uri);
	if (filename) {
		int result = g_remove (filename);
		g_free (filename);
		return (result == 0);
	}



	GFile *f = g_file_new_for_uri (uri);
	gboolean res = g_file_delete (f, nullptr, err);
	g_object_unref (G_OBJECT (f));

	return res;
}

static gboolean
remove_tree (GFile *file, GError **err)
{
	GFileEnumerator *enumer = g_file_enumerate_children (file,
		G_FILE_ATTRIBUTE_STANDARD_NAME ","
		G_FILE_ATTRIBUTE_STANDARD_TYPE,
		G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
		nullptr, nullptr);

	if (enumer != nullptr) {
		gboolean ok = TRUE;
		GFileInfo *info;
		while (ok
			   && (info = g_file_enumerator_next_file (enumer, nullptr, nullptr)) != nullptr) {
			GFile *child = g_file_get_child (file,
				g_file_info_get_name (info));
			if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
				ok = remove_tree (child, err);
			else
				ok = g_file_delete (child, nullptr, err);
			g_object_unref (G_OBJECT (info));
			g_object_unref (G_OBJECT (child));
		}
		g_object_unref (G_OBJECT (enumer));
		if (!ok)
			return FALSE;
	}

	return g_file_delete (file, nullptr, err);
}

/**
 * UT_go_file_remove_recursive:
 * @uri: uri of a file or directory
 * @err: (allow-none): #GError
 *
 * Deletes @uri; if it names a directory, its children are removed
 * depth-first first. Unlike UT_go_file_remove(), which only removes
 * empty directories, this drops a whole tree. The walk stops on the
 * first failing child; symlinks are deleted, never followed.
 */
gboolean
UT_go_file_remove_recursive (char const *uri, GError ** err)
{
	g_return_val_if_fail (uri != nullptr, FALSE);

	GFile *f = g_file_new_for_uri (uri);
	gboolean res = remove_tree (f, err);
	g_object_unref (G_OBJECT (f));

	return res;
}

/**
 * UT_go_file_atomic_temp_name:
 * @final_path: local filesystem path of the file to be replaced
 *
 * Returns the sibling scratch path a caller writes to before calling
 * UT_go_file_atomic_replace(): @final_path with ".part" appended.
 * Free with g_free().
 */
gchar *
UT_go_file_atomic_temp_name (const gchar *final_path)
{
	g_return_val_if_fail (final_path != nullptr, nullptr);
	return g_strconcat (final_path, ".part", nullptr);
}

/**
 * UT_go_file_atomic_replace:
 * @tmp_path: fully written + closed sibling temp path (see
 *            UT_go_file_atomic_temp_name())
 * @final_path: the local filesystem path @tmp_path replaces
 * @err: (allow-none): #GError
 *
 * Commits an atomic save: moves @tmp_path over @final_path so that an
 * export, disk or encryption failure - or a crash mid-write - can
 * never destroy the previously saved document. On POSIX this is a
 * rename(2) that keeps the old file's permission bits and fsync()s
 * both the file and its directory so the swap survives a power loss;
 * @tmp_path is unlinked if the rename fails. On Windows the swap is a
 * MoveFileExW(MOVEFILE_REPLACE_EXISTING) - rename()/_wrename() refuse
 * to replace an existing destination there - restoring the old file's
 * attributes and flushing the result via _commit() (Windows has no
 * directory fsync). On platforms without either, the staged contents
 * replace the target via g_file_set_contents() - weaker (the fallback
 * is not crash-atomic on every backend), but the target is never
 * truncated in place by callers writing to it directly. @tmp_path is
 * consumed either way.
 *
 * Returns: TRUE if @final_path now holds the staged contents.
 */
gboolean
UT_go_file_atomic_replace (const gchar *tmp_path,
						   const gchar *final_path,
						   GError **err)
{
	g_return_val_if_fail (tmp_path != nullptr, FALSE);
	g_return_val_if_fail (final_path != nullptr, FALSE);

#if defined(G_OS_WIN32)
	/* GLib encodes local filenames as UTF-8; the W APIs want UTF-16 */
	gunichar2 *wtmp = g_utf8_to_utf16 (tmp_path, -1, nullptr, nullptr, err);
	gunichar2 *wfinal = wtmp
		? g_utf8_to_utf16 (final_path, -1, nullptr, nullptr, err)
		: nullptr;
	if (!wfinal) {
		g_free (wtmp);
		(void)g_remove (tmp_path);
		return FALSE;
	}

	/* keep the old file's attributes, like the POSIX branch's
	 * chmod(st_mode); a read-only target would reject the replace, so
	 * the bit is dropped before the move and the full set restored
	 * afterwards */
	DWORD attrs = GetFileAttributesW (reinterpret_cast<LPCWSTR> (wfinal));
	const bool bHadOld = (attrs != INVALID_FILE_ATTRIBUTES);
	if (bHadOld && (attrs & FILE_ATTRIBUTE_READONLY) != 0)
		(void)SetFileAttributesW (reinterpret_cast<LPCWSTR> (wfinal),
								  attrs & ~FILE_ATTRIBUTE_READONLY);

	if (!MoveFileExW (reinterpret_cast<LPCWSTR> (wtmp),
					  reinterpret_cast<LPCWSTR> (wfinal),
					  MOVEFILE_REPLACE_EXISTING)) {
		DWORD dwErr = GetLastError ();
		gchar *emsg = g_win32_error_message (dwErr);
		if (bHadOld && (attrs & FILE_ATTRIBUTE_READONLY) != 0)
			(void)SetFileAttributesW (reinterpret_cast<LPCWSTR> (wfinal),
									  attrs);
		g_free (wtmp);
		g_free (wfinal);
		(void)g_remove (tmp_path);
		g_set_error (err, G_IO_ERROR,
					 g_io_error_from_win32_error (dwErr),
					 "cannot replace '%s': %s", final_path, emsg);
		g_free (emsg);
		return FALSE;
	}

	if (bHadOld)
		(void)SetFileAttributesW (reinterpret_cast<LPCWSTR> (wfinal), attrs);

	/* _commit() is the CRT flush-to-disk analogue of fsync(); there is
	 * no O_CLOEXEC to emulate and directories cannot be synced */
	int fd = _wopen (reinterpret_cast<const wchar_t *> (wfinal),
					 _O_RDONLY | _O_BINARY);
	if (fd >= 0) {
		(void)_commit (fd);
		_close (fd);
	}
	g_free (wtmp);
	g_free (wfinal);
	return TRUE;
#elif defined(UT_GO_NO_POSIX)
	/* No POSIX rename/fsync semantics: replace the target with the
	 * staged contents in one shot. g_file_set_contents() itself
	 * stages through a temp file where the platform allows. */
	gchar *contents = nullptr;
	gsize length = 0;
	if (!g_file_get_contents (tmp_path, &contents, &length, err)) {
		(void)g_remove (tmp_path);
		return FALSE;
	}
	gboolean ok = g_file_set_contents (final_path, contents,
									   (gssize) length, err);
	g_free (contents);
	(void)g_remove (tmp_path);
	return ok;
#else
	/* keep the old file's permissions if it already existed */
	struct stat st;
	const bool bHadOld = (::stat (final_path, &st) == 0);

	if (::rename (tmp_path, final_path) != 0) {
		(void)::unlink (tmp_path);
		g_set_error (err, G_FILE_ERROR,
					 g_file_error_from_errno (errno),
					 "cannot replace '%s': %s",
					 final_path, g_strerror (errno));
		return FALSE;
	}

	if (bHadOld)
		(void)::chmod (final_path, st.st_mode);

	/* fsync the file and its directory so the rename is durable
	 * across a power loss */
	int fd = ::open (final_path, O_RDONLY | O_CLOEXEC);
	if (fd >= 0) {
		(void)::fsync (fd);
		::close (fd);
	}
	char *dir = g_path_get_dirname (final_path);
	int dfd = ::open (dir, O_RDONLY | O_CLOEXEC);
	if (dfd >= 0) {
		(void)::fsync (dfd);
		::close (dfd);
	}
	g_free (dir);
	return TRUE;
#endif
}

/**
 * UT_go_file_atomic_abort:
 * @tmp_path: sibling temp path from UT_go_file_atomic_temp_name()
 *
 * Drops a staged temp file whose export failed or was cancelled,
 * leaving the original target untouched.
 */
void
UT_go_file_atomic_abort (const gchar *tmp_path)
{
	if (tmp_path)
		(void)g_remove (tmp_path);
}

/**
 * UT_go_random_bytes:
 * @buf: destination buffer
 * @len: number of bytes to fill
 *
 * Fills @buf with cryptographically-suitable random bytes from the OS
 * entropy source: BCryptGenRandom(BCRYPT_USE_SYSTEM_PREFERRED_RNG) on
 * Windows (bcrypt.lib at link time), arc4random_buf(3) on macOS and
 * the BSDs, getrandom(2) on Linux/glibc with /dev/urandom as the
 * fallback there and on every other POSIX system. There is
 * deliberately no PRNG fallback: callers must fail rather than emit a
 * predictable salt/IV/key.
 *
 * Returns: TRUE on success, FALSE when no OS entropy source was
 * available.
 */
gboolean
UT_go_random_bytes (guchar *buf, gsize len)
{
	g_return_val_if_fail (buf != nullptr || len == 0, FALSE);
	if (len == 0)
		return TRUE;

#if defined(G_OS_WIN32)
	/* CNG's system-preferred RNG, Vista+; no handle to open. */
	while (len > 0)
	{
		ULONG chunk = len > 0xffffffffUL ? 0xffffffffUL
										 : static_cast<ULONG>(len);
		if (BCryptGenRandom (nullptr, buf, chunk,
							 BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
			return FALSE;
		buf += chunk;
		len -= chunk;
	}
	return TRUE;
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
	  defined(__OpenBSD__) || defined(__DragonFly__)
	/* arc4random_buf(3) cannot fail and has no per-call byte limit
	 * (getentropy(2) caps each call at 256 bytes). Declared in
	 * <stdlib.h>. */
	arc4random_buf (buf, len);
	return TRUE;
#else
#if defined(__linux__) || defined(__GLIBC__)
	{
		/* getrandom(2) needs no fd; ENOSYS (ancient kernel) falls
		 * through to the urandom read below. */
		gsize got = 0;
		while (got < len)
		{
			gssize n = getrandom (buf + got, len - got, 0);
			if (n < 0)
			{
				if (errno == EINTR)
					continue;
				break;
			}
			got += static_cast<gsize>(n);
		}
		if (got == len)
			return TRUE;
	}
#endif
	int fd = ::open ("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return FALSE;
	UT_ScopedFD fdg (fd);
	gsize got = 0;
	while (got < len)
	{
		gssize n = ::read (fd, buf + got, len - got);
		if (n <= 0)
		{
			if (n < 0 && errno == EINTR)
				continue;
			return FALSE;
		}
		got += static_cast<gsize>(n);
	}
	return TRUE;
#endif
}

/**
 * UT_go_self_exe_path:
 *
 * Returns the absolute path of the running executable: /proc/self/exe
 * on Linux (and any other POSIX system where procfs is mounted),
 * _NSGetExecutablePath(3) on macOS, sysctl(KERN_PROC_PATHNAME) on
 * FreeBSD/DragonFly (with /proc/self/exe as fallback when procfs
 * happens to be mounted), GetModuleFileNameW on Windows. Used to
 * locate source-tree resources (artwork/, clipart) when running
 * uninstalled.
 *
 * Returns: a newly-allocated UTF-8 path (free with g_free), or NULL
 * when the platform cannot report it — callers must tolerate NULL.
 */
gchar *
UT_go_self_exe_path (void)
{
#if defined(G_OS_WIN32)
	{
		/* GetModuleFileNameW(NULL, ...) reports the process image
		 * path regardless of argv[0]. Unlike the POSIX branches the
		 * result is not symlink-resolved, which is fine: callers
		 * only dirname-walk it. A return of nSize means the output
		 * was truncated, so grow the buffer from MAX_PATH until the
		 * (possibly long-path-enabled) name fits. */
		DWORD size = MAX_PATH;
		for (;;)
		{
			gunichar2 *wbuf = g_new (gunichar2, size);
			DWORD len = GetModuleFileNameW (nullptr,
							reinterpret_cast<LPWSTR> (wbuf),
							size);
			if (len == 0)
			{
				g_free (wbuf);
				return nullptr;
			}
			if (len < size)
			{
				gchar *out = g_utf16_to_utf8 (wbuf, len,
							      nullptr, nullptr, nullptr);
				g_free (wbuf);
				return out;
			}
			g_free (wbuf);
			if (size >= 32768)
				return nullptr;
			size *= 2;
		}
	}
#elif defined(__APPLE__)
	{
		/* _NSGetExecutablePath may return a path containing symlinks
		 * and ".." segments; resolve it so the callers' dirname walks
		 * behave like the /proc/self/exe result. */
		guint32 size = 0;
		_NSGetExecutablePath (nullptr, &size);
		if (size == 0)
			return nullptr;
		gchar *buf = g_new (gchar, size);
		if (_NSGetExecutablePath (buf, &size) != 0)
		{
			g_free (buf);
			return nullptr;
		}
		gchar *resolved = realpath (buf, nullptr);
		g_free (buf);
		if (!resolved)
			return nullptr;
		gchar *out = g_strdup (resolved);
		free (resolved);
		return out;
	}
#elif defined(__FreeBSD__) || defined(__DragonFly__)
	{
		/* KERN_PROC_PATHNAME works without procfs; procfs mounts are
		 * optional on FreeBSD so it is the better primary source. */
		int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
		size_t size = 0;
		if (sysctl (mib, 4, nullptr, &size, nullptr, 0) != 0 || size == 0)
			return g_file_read_link ("/proc/self/exe", nullptr);
		gchar *buf = g_new (gchar, size);
		if (sysctl (mib, 4, buf, &size, nullptr, 0) != 0 || size == 0)
		{
			g_free (buf);
			return g_file_read_link ("/proc/self/exe", nullptr);
		}
		buf[size - 1] = '\0';
		return buf;
	}
#else
	/* Linux always mounts procfs; on other POSIX systems (NetBSD,
	 * OpenBSD, Solaris) it is optional — NULL simply disables the
	 * build-tree discovery fallback. */
	return g_file_read_link ("/proc/self/exe", nullptr);
#endif
}

gboolean
UT_go_file_exists (char const *uri)
{
	GFile *f = g_file_new_for_uri (uri);
	gboolean res = g_file_query_exists (f, nullptr);
	g_object_unref (G_OBJECT (f));
	return res;
}

UT_GOFilePermissions *
UT_go_get_file_permissions (char const *uri)
{
	UT_GOFilePermissions * file_permissions = nullptr;

#if GLIB_CHECK_VERSION(2,26,0) || defined(G_OS_WIN32)
	GStatBuf file_stat;
#else
	struct stat file_stat;
#endif
	char *filename = UT_go_filename_from_uri (uri);
	int result = filename ? g_stat (filename, &file_stat) : -1;

	g_free (filename);
	if (result == 0) {
		file_permissions = g_new0 (UT_GOFilePermissions, 1);
#if ! defined (G_OS_WIN32)
		/* Owner  Permissions */
		file_permissions->owner_read    = ((file_stat.st_mode & S_IRUSR) != 0);
		file_permissions->owner_write   = ((file_stat.st_mode & S_IWUSR) != 0);
		file_permissions->owner_execute = ((file_stat.st_mode & S_IXUSR) != 0);

		/* Group  Permissions */
		file_permissions->group_read    = ((file_stat.st_mode & S_IRGRP) != 0);
		file_permissions->group_write   = ((file_stat.st_mode & S_IWGRP) != 0);
		file_permissions->group_execute = ((file_stat.st_mode & S_IXGRP) != 0);

		/* Others Permissions */
		file_permissions->others_read    = ((file_stat.st_mode & S_IROTH) != 0);
		file_permissions->others_write   = ((file_stat.st_mode & S_IWOTH) != 0);
		file_permissions->others_execute = ((file_stat.st_mode & S_IXOTH) != 0);
#else
		/* Windows */
		/* Owner  Permissions */
		file_permissions->owner_read    = ((file_stat.st_mode & S_IREAD) != 0);
		file_permissions->owner_write   = ((file_stat.st_mode & S_IWRITE) != 0);
		file_permissions->owner_execute = ((file_stat.st_mode & S_IEXEC) != 0);
#endif
	}
	return file_permissions;
}

void
UT_go_set_file_permissions (char const *uri, UT_GOFilePermissions * file_permissions)
{
#if ! defined (G_OS_WIN32)
	mode_t permissions = 0;
	int result;
	char *filename;

	/* Set owner permissions */
	if (file_permissions->owner_read == TRUE)
		permissions |= S_IRUSR;

	if (file_permissions->owner_write == TRUE)
		permissions |= S_IWUSR;

	if (file_permissions->owner_execute == TRUE)
		permissions |= S_IXUSR;

	/* Set group permissions */
	if (file_permissions->group_read == TRUE)
		permissions |= S_IRGRP;

	if (file_permissions->group_write == TRUE)
		permissions |= S_IWGRP;

	if (file_permissions->group_execute == TRUE)
		permissions |= S_IXGRP;

	/* Set others permissions */
	if (file_permissions->others_read == TRUE)
		permissions |= S_IROTH;

	if (file_permissions->others_write == TRUE)
		permissions |= S_IWOTH;

	if (file_permissions->others_execute == TRUE)
		permissions |= S_IXOTH;

	filename = UT_go_filename_from_uri (uri);

#ifdef HAVE_G_CHMOD
	result = g_chmod (filename, permissions);
#else
	result = chmod (filename, permissions);
#endif

	g_free (filename);

	if (result != 0)
		g_warning ("Error setting permissions for %s.", uri);
#else
	// Win32 - shall we put an assert here?
	UT_UNUSED(uri);
	UT_UNUSED(file_permissions);
#endif
}

enum UT_GOFileDateType {
	UT_GO_FILE_DATE_TYPE_ACCESSED = 0,
	UT_GO_FILE_DATE_TYPE_MODIFIED,
	UT_GO_FILE_DATE_TYPE_CHANGED
};

static time_t
UT_go_file_get_date (char const *uri, UT_GOFileDateType type)
{
	time_t tm = -1;

#if GLIB_CHECK_VERSION(2,26,0) || defined(G_OS_WIN32)
	GStatBuf file_stat;
#else
	struct stat file_stat;
#endif
	char *filename = UT_go_filename_from_uri (uri);
	int result = filename ? g_stat (filename, &file_stat) : -1;

	g_free (filename);
	if (result == 0) {
		switch (type) {
			case UT_GO_FILE_DATE_TYPE_ACCESSED:
				tm = file_stat.st_atime;
				break;
			case UT_GO_FILE_DATE_TYPE_MODIFIED:
				tm = file_stat.st_mtime;
				break;
			case UT_GO_FILE_DATE_TYPE_CHANGED:
				tm = file_stat.st_ctime;
				break;
		}
	}

	return tm;
}

time_t
UT_go_file_get_date_accessed (char const *uri)
{
	return UT_go_file_get_date (uri, UT_GO_FILE_DATE_TYPE_ACCESSED);
}

time_t
UT_go_file_get_date_modified (char const *uri)
{
	return UT_go_file_get_date (uri, UT_GO_FILE_DATE_TYPE_MODIFIED);
}

time_t
UT_go_file_get_date_changed (char const *uri)
{
	return UT_go_file_get_date (uri, UT_GO_FILE_DATE_TYPE_CHANGED);
}

/* ------------------------------------------------------------------------- */
// Fallback for systems where the default URI handler is missing or broken.
// `spec` is a shell-like command line that may embed a %s or %1 placeholder
// for the URL; without one the URL is appended as the last argument.
static gboolean
browser_spawn (char const *spec, gchar const *url, GError **err)
{
	gint    argc = 0;
	gchar **argv = nullptr;

	if (nullptr == spec || !*spec)
		return FALSE;
	if (!g_shell_parse_argv (spec, &argc, &argv, nullptr) || argc < 1) {
		g_strfreev (argv);
		return FALSE;
	}

	/* the program must exist (absolute path) or be found on PATH */
	if (g_path_is_absolute (argv[0])) {
		if (!g_file_test (argv[0], G_FILE_TEST_IS_EXECUTABLE)) {
			g_strfreev (argv);
			return FALSE;
		}
	} else if (nullptr == g_find_program_in_path (argv[0])) {
		g_strfreev (argv);
		return FALSE;
	}

	gint  i;
	char *tmp;
	for (i = 1 ; i < argc ; i++) {
		tmp = strstr (argv[i], "%s");
		if (nullptr == tmp)
			tmp = strstr (argv[i], "%1");
		if (nullptr != tmp) {
			*tmp = '\0';
			tmp = g_strconcat (argv[i], url, tmp + 2, nullptr);
			g_free (argv[i]);
			argv[i] = tmp;
			break;
		}
	}
	if (i == argc) {
		argv = static_cast<gchar **> (g_realloc (argv, (argc + 2) * sizeof (gchar *)));
		argv[argc]     = g_strdup (url);
		argv[argc + 1] = nullptr;
	}

	gboolean spawned = g_spawn_async (nullptr, argv, nullptr,
	                                  G_SPAWN_SEARCH_PATH,
	                                  nullptr, nullptr, nullptr, err);
	g_strfreev (argv);
	return spawned;
}

/* The $BROWSER convention is a colon-separated list of command specs,
   each tried in order (same precedence xdg-open's generic path gives it). */
static gboolean
browser_from_env (gchar const *url, GError **err)
{
	char const *env = getenv ("BROWSER");
	if (nullptr == env || !*env)
		return FALSE;

	gboolean ok = FALSE;
	gchar **entries = g_strsplit (env, ":", -1);
	for (gint i = 0 ; entries[i] && !ok ; i++)
		ok = browser_spawn (entries[i], url, err);
	g_strfreev (entries);
	return ok;
}

static void
fallback_open_uri(const gchar* url, GError** err)
{
	if (browser_from_env (url, err))
		return;

	static char const * const browsers[] = {
		"xdg-open",		/* XDG. you shouldn't need anything else */
		"sensible-browser",	/* debian */
		"epiphany",		/* primary gnome */
		"galeon",		/* secondary gnome */
		"encompass",
		"firefox",
		"mozilla-firebird",
		"mozilla",
		"netscape",
		"konqueror",
		"xterm -e w3m",
		"xterm -e lynx",
		"xterm -e links"
	};
	for (unsigned i = 0 ; i < G_N_ELEMENTS (browsers) ; i++)
		if (browser_spawn (browsers[i], url, err))
			return;

	if (err && nullptr == *err)
		*err = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
		                            "no usable browser found");
}

/* Return a pointer to the ':' terminating an RFC 3986 scheme at the
   start of @url, or NULL when @url does not begin with one
   (scheme = ALPHA *(ALPHA / DIGIT / "+" / "-" / ".") ":"). */
static const char *
url_scheme_end (const char *url)
{
	const char * p = url;

	if (!g_ascii_isalpha (p[0]))
		return nullptr;
	for (p++; g_ascii_isalnum (p[0]) || p[0] == '+' ||
	     p[0] == '-' || p[0] == '.'; p++)
		;
	return (p[0] == ':') ? p : nullptr;
}

static gboolean
url_scheme_is (const char *url, const char *scheme, const char *end)
{
	return strlen (scheme) == (gsize)(end - url) &&
	       g_ascii_strncasecmp (url, scheme, end - url) == 0;
}

/* A local target is only safe to show when the handler would *open* it
   rather than run it: no executable bit, and not a launcher file
   (.desktop / .lnk / .url are programs in file form).  Directories are
   fine - they carry the search bit but open in a file manager.
   Unresolvable targets pass: they cannot run, and the handler reports
   the failure. */
static gboolean
local_target_is_safe (const char *path)
{
	static const char * const launcher_exts[] = {
		".desktop", ".lnk", ".url"
	};
	size_t len = strlen (path);
	for (unsigned i = 0; i < G_N_ELEMENTS (launcher_exts); i++)
	{
		size_t elen = strlen (launcher_exts[i]);
		if (len > elen &&
		    g_ascii_strcasecmp (path + len - elen,
					launcher_exts[i]) == 0)
			return FALSE;
	}

	GFile * file = g_file_new_for_path (path);
	GFileInfo * info = g_file_query_info
		(file, G_FILE_ATTRIBUTE_STANDARD_TYPE ","
		 G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE,
		 G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
	g_object_unref (file);
	if (!info)
		return TRUE;
	gboolean safe =
		g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY ||
		!g_file_info_get_attribute_boolean
		(info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE);
	g_object_unref (info);
	return safe;
}

/*
 * UT_go_url_is_safe :
 * @url : a URI or plain path a document or UI link wants to open.
 *
 * TRUE when @url may be handed to the system "open this URI" handler.
 * The handler treats some schemes as app-launchers rather than
 * document openers: javascript:/data: run script inside the browser
 * context, file:// to an executable (or a .desktop/.lnk/.url launcher
 * file) runs arbitrary code, and any registered scheme (smb:,
 * ms-word:, steam:, ...) invokes whatever owns it.  Since documents
 * carry links, only a small allowlist is safe to launch on a click,
 * and file:// additionally refuses runnable targets.
 */
gboolean
UT_go_url_is_safe (const char *url)
{
	if (!url || !*url)
		return FALSE;

	const char * scheme_end = url_scheme_end (url);

#ifdef G_OS_WIN32
	/* "C:\..." (and "C:/...") is a drive path, not a "c:" scheme */
	if (scheme_end && scheme_end - url == 1 &&
	    (scheme_end[1] == '\\' || scheme_end[1] == '/'))
		scheme_end = nullptr;
#endif

	if (!scheme_end)
		/* no scheme: a plain path, same rules as file:// */
		return local_target_is_safe (url);

	if (url_scheme_is (url, "file", scheme_end))
	{
		/* GIO file URIs are always local (the authority field is
		   ignored); a malformed URI yields NULL and cannot be
		   opened anyway */
		char * path = g_filename_from_uri (url, nullptr, nullptr);
		if (!path)
			return FALSE;
		gboolean safe = local_target_is_safe (path);
		g_free (path);
		return safe;
	}

	/* schemes whose handler opens a resource rather than running
	   a payload */
	static const char * const safe_schemes[] = {
		"http", "https", "ftp", "ftps", "mailto"
	};
	for (unsigned i = 0; i < G_N_ELEMENTS (safe_schemes); i++)
		if (url_scheme_is (url, safe_schemes[i], scheme_end))
			return TRUE;

	return FALSE;
}

GError *
UT_go_url_show (gchar const *url)
{
	GError *err = nullptr;

	if (!UT_go_url_is_safe (url))
		return g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
				    "refusing to open '%s': the URI scheme "
				    "or target is not launchable",
				    url ? url : "(null)");

#ifdef G_OS_UNIX
	/* An explicit $BROWSER choice wins for web URLs on Unix — the schemes
	   the variable is meant for.  Other schemes (mailto:, file:, ...) go
	   straight to the system handler below. */
	if ((g_str_has_prefix (url, "http://") ||
	     g_str_has_prefix (url, "https://")) &&
	    browser_from_env (url, &err))
		return nullptr;
	g_clear_error (&err);
#endif

	/* Portable default-handler path: mimeapps/xdg-open on Linux,
	   LaunchServices on macOS, registry protocol handlers on Windows. */
	if (g_app_info_launch_default_for_uri(url, nullptr, &err))
		return nullptr;
	g_clear_error (&err);

#ifdef G_OS_UNIX
	fallback_open_uri(url, &err);
#elif defined(G_OS_WIN32)
	/* ShellExecute resolves file/protocol associations through the shell
	   even where GLib's registry lookup came up empty. */
	gunichar2 *wurl = g_utf8_to_utf16 (url, -1, nullptr, nullptr, nullptr);
	if (wurl) {
		if (reinterpret_cast<INT_PTR>(ShellExecuteW (nullptr, L"open",
				reinterpret_cast<LPCWSTR>(wurl), nullptr, nullptr,
				SW_SHOWNORMAL)) <= 32)
			err = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
			                           "ShellExecute failed to open the URL");
		g_free (wurl);
	} else
		err = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
		                           "could not convert URL to UTF-16");
#endif
	return err;
}

gchar *
UT_go_get_mime_type (gchar const *uri)
{
	gboolean content_type_uncertain = FALSE;
	char *content_type = g_content_type_guess (uri, nullptr, 0, &content_type_uncertain);
	if (content_type) {
		char *mime_type = g_content_type_get_mime_type (content_type);
		g_free (content_type);

		if (mime_type)
			return mime_type;
	}

	return g_strdup ("application/octet-stream");
}

/* ------------------------------------------------------------------------- */


gint
UT_go_utf8_collate_casefold (const char *a, const char *b)
{
	char *a2 = g_utf8_casefold (a, -1);
	char *b2 = g_utf8_casefold (b, -1);
	int res = g_utf8_collate (a2, b2);
	g_free (a2);
	g_free (b2);
	return res;
}

