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

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "tf_test.h"
#include "ut_raii.h"

#define TFSUITE "core.af.util.raii"

TFTEST_MAIN("UT_ScopedFD closes and transfers")
{
	// open a real fd via tmpfile's fileno-independent path: use /dev/null
	UT_ScopedFD fd(open("/dev/null", O_RDONLY));
	TFPASS(static_cast<bool>(fd));
	TFPASS(fd.get() >= 0);

	// release() hands out ownership without closing
	int raw = fd.release();
	TFPASS(raw >= 0);
	TFPASS(!static_cast<bool>(fd));
	TFPASS(fd.get() == -1);

	// adopting the fd back and letting the guard die closes it
	fd.reset(raw);
	TFPASS(static_cast<bool>(fd));
	{
		UT_ScopedFD inner(open("/dev/null", O_RDONLY));
		TFPASS(inner.get() >= 0);
	} // closes on scope exit

	// reset(-1) on an empty guard is safe
	UT_ScopedFD empty;
	TFPASS(!static_cast<bool>(empty));
	empty.reset();
	TFPASS(!static_cast<bool>(empty));
}

TFTEST_MAIN("UT_FilePtr closes the stream")
{
	UT_FilePtr f(tmpfile());
	TFPASS(f != nullptr);
	TFPASS(fputs("data", f.get()) >= 0);
	// dtor fclose()s: write flushed before destruct
	fflush(f.get());
	f.reset();
	TFPASS(f == nullptr);
}

TFTEST_MAIN("UT_GFreePtr frees with g_free")
{
	UT_GFreePtr<char> p(g_strdup("heap"));
	TFPASS(p != nullptr);
	TFPASS(strcmp(p.get(), "heap") == 0);
	// move transfers ownership
	UT_GFreePtr<char> q(std::move(p));
	TFPASS(p == nullptr);
	TFPASS(strcmp(q.get(), "heap") == 0);
}

TFTEST_MAIN("gsf input/output holders")
{
	// UT_GsfInputPtr unrefs the input on scope exit
	const guint8 bytes[] = {1, 2, 3, 4};
	{
		UT_GsfInputPtr in(gsf_input_memory_new(bytes, sizeof(bytes), FALSE));
		TFPASS(in != nullptr);
		TFPASS(gsf_input_size(in.get()) == 4);
	}

	// UT_GsfOutputPtr closes + unrefs the output on scope exit
	{
		UT_GsfOutputPtr out(GSF_OUTPUT(gsf_output_memory_new()));
		TFPASS(out != nullptr);
		TFPASS(gsf_output_write(out.get(), 4, bytes));
		// scope exit calls gsf_output_close + unref
	}
}

TFTEST_MAIN("GObject and cairo holders")
{
	{
		UT_GObjPtr<GObject> o(static_cast<GObject*>(g_object_new(G_TYPE_OBJECT, nullptr)));
		TFPASS(o != nullptr);
		TFPASS(G_IS_OBJECT(o.get()));
	}
	{
		UT_CairoSurfacePtr s(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 4, 4));
		TFPASS(s != nullptr);
		TFPASS(cairo_surface_status(s.get()) == CAIRO_STATUS_SUCCESS);

		UT_CairoPtr cr(cairo_create(s.get()));
		TFPASS(cr != nullptr);
		cairo_paint(cr.get());
	}
}

TFTEST_MAIN("UT_SecureStringGuard wipes the secret")
{
	std::string secret = "sup3r-s3cret-value";
	{
		UT_SecureStringGuard guard(&secret);
		TFPASS(secret.size() == 18);
	} // guard fires: whole allocation zeroed then string cleared
	TFPASS(secret.empty());
	TFPASS(secret.size() == 0);
}
