/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource
 *
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

#ifndef UT_RAII_H
#define UT_RAII_H

#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>

#include <glib-object.h>
#include <gsf/gsf.h>
#include <cairo.h>

#include "ut_misc.h"

/*!
 * Scoped holders for raw C handles so early returns and error paths
 * cannot leak them.
 *
 * UT_FilePtr          - FILE*            -> fclose
 * UT_GDirPtr          - GDir*            -> g_dir_close
 * UT_GsfInputPtr      - GsfInput*        -> g_object_unref
 * UT_GsfOutputPtr     - GsfOutput*       -> gsf_output_close + g_object_unref
 * UT_GObjPtr<T>       - any GObject      -> g_object_unref
 * UT_CairoPtr         - cairo_t*         -> cairo_destroy
 * UT_CairoSurfacePtr  - cairo_surface_t* -> cairo_surface_destroy
 * UT_ScopedFD         - int fd           -> close
 */

struct UT_fcloser
{
	void operator()(FILE * p) const noexcept
	{
		if (p)
			std::fclose(p);
	}
};

struct UT_gobj_unref
{
	void operator()(gpointer p) const noexcept
	{
		if (p)
			g_object_unref(p);
	}
};

struct UT_gsf_output_close
{
	void operator()(GsfOutput * p) const noexcept
	{
		if (p)
		{
			if (!gsf_output_is_closed(p))
				gsf_output_close(p);
			g_object_unref(p);
		}
	}
};

struct UT_gdir_close
{
	void operator()(GDir * p) const noexcept
	{
		if (p)
			g_dir_close(p);
	}
};

struct UT_cairo_destroy
{
	void operator()(cairo_t * p) const noexcept
	{
		if (p)
			cairo_destroy(p);
	}
};

struct UT_cairo_surface_destroy
{
	void operator()(cairo_surface_t * p) const noexcept
	{
		if (p)
			cairo_surface_destroy(p);
	}
};

typedef std::unique_ptr<FILE, UT_fcloser>					UT_FilePtr;
typedef std::unique_ptr<GDir, UT_gdir_close>				UT_GDirPtr;
typedef std::unique_ptr<GsfInput, UT_gobj_unref>			UT_GsfInputPtr;
typedef std::unique_ptr<GsfOutput, UT_gsf_output_close>	UT_GsfOutputPtr;
typedef std::unique_ptr<cairo_t, UT_cairo_destroy>		UT_CairoPtr;
typedef std::unique_ptr<cairo_surface_t,
					  UT_cairo_surface_destroy>				UT_CairoSurfacePtr;

/* generic GObject-derived handle (GdkPixbuf, GsfInfile, ...) */
template <typename T>
using UT_GObjPtr = std::unique_ptr<T, UT_gobj_unref>;

struct UT_secure_wipe
{
	void operator()(std::string * p) const noexcept
	{
		if (p)
			UT_secureClearString(*p);
	}
};

/*!
 * UT_SecureStringGuard - securely wipes a function-local std::string
 * holding a secret (password, key material) on scope exit, on every
 * return path:
 *
 *   std::string password = ...;
 *   UT_SecureStringGuard wipe(&password);
 */
using UT_SecureStringGuard = std::unique_ptr<std::string, UT_secure_wipe>;

/* scoped POSIX fd (-1 = empty) */
class UT_ScopedFD
{
public:
	explicit UT_ScopedFD(int fd = -1) noexcept : m_fd(fd) {}
	~UT_ScopedFD() { reset(); }

	UT_ScopedFD(const UT_ScopedFD &) = delete;
	UT_ScopedFD & operator=(const UT_ScopedFD &) = delete;

	int get() const noexcept { return m_fd; }
	int release() noexcept { int fd = m_fd; m_fd = -1; return fd; }
	void reset(int fd = -1) noexcept
	{
		if (m_fd >= 0)
			::close(m_fd);
		m_fd = fd;
	}
	explicit operator bool() const noexcept { return m_fd >= 0; }

private:
	int m_fd;
};

#endif /* UT_RAII_H */
