/* AbiSource Application Framework
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

#include "config.h"

#include "ut_screenshot.h"

#include <gio/gio.h>
#include <glib/gstdio.h>

namespace
{

/* the XDG Screenshot portal (xdg-desktop-portal) speaks this API on
 * GNOME, KDE and most other freedesktop desktops, on Wayland and X11
 * alike; off freedesktop platforms there is no session bus and every
 * call below fails cleanly */
const char * const PORTAL_BUS_NAME = "org.freedesktop.portal.Desktop";
const char * const PORTAL_OBJ_PATH = "/org/freedesktop/portal/desktop";

GDBusConnection * s_portal_bus()
{
	/* shared session-bus connection; stays nullptr where no bus
	 * exists (Windows, macOS, headless shells) */
	static GDBusConnection * s_bus = nullptr;
	static bool s_tried = false;
	if (!s_tried)
	{
		s_tried = true;
		s_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
	}
	return s_bus;
}

bool s_portal_has_screenshot()
{
	GDBusConnection * bus = s_portal_bus();
	if (!bus)
		return false;
	/* a property read answers "installed AND activatable" in one call:
	 * method calls auto-start the service when it is dbus-activatable,
	 * and fail with ServiceUnknown when it is not installed */
	GVariant * r = g_dbus_connection_call_sync(bus,
		PORTAL_BUS_NAME, PORTAL_OBJ_PATH,
		"org.freedesktop.DBus.Properties", "Get",
		g_variant_new("(ss)", "org.freedesktop.portal.Screenshot",
					  "version"),
		nullptr, G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, nullptr);
	if (!r)
		return false;
	g_variant_unref(r);
	return true;
}

struct PortalShotCtx
{
	GMainLoop *	loop;
	guint		response = 2;	/* portal codes: 0 ok / 1 user cancelled / 2 error */
	guint		timeoutId = 0;
	std::string	uri;
};

void s_on_portal_response(GDBusConnection *, const gchar *, const gchar *,
						  const gchar *, const gchar *,
						  GVariant * params, gpointer data)
{
	PortalShotCtx * ctx = static_cast<PortalShotCtx *>(data);
	guint32 resp = 2;
	GVariant * results = nullptr;
	g_variant_get(params, "(u@a{sv})", &resp, &results);
	ctx->response = resp;
	if (resp == 0 && results)
	{
		GVariant * v = g_variant_lookup_value(results, "uri",
											G_VARIANT_TYPE_STRING);
		if (v)
		{
			ctx->uri = g_variant_get_string(v, nullptr);
			g_variant_unref(v);
		}
	}
	g_clear_pointer(&results, g_variant_unref);
	g_main_loop_quit(ctx->loop);
}

/* the portal service vanishing mid-request must end the wait -
 * arg0-matched on the portal name; only an empty new owner (service
 * gone, not started or restarted) quits the loop */
void s_on_portal_owner(GDBusConnection *, const gchar *, const gchar *,
					   const gchar *, const gchar *,
					   GVariant * params, gpointer data)
{
	PortalShotCtx * ctx = static_cast<PortalShotCtx *>(data);
	const gchar * newOwner = nullptr;
	g_variant_get(params, "(&s&s&s)", nullptr, nullptr, &newOwner);
	if (!newOwner || !*newOwner)
	{
		ctx->response = 2;
		g_main_loop_quit(ctx->loop);
	}
}

UT_ScreenshotResult s_portal_capture(std::string & outPath)
{
	GDBusConnection * bus = s_portal_bus();
	if (!bus)
		return UT_SCREENSHOT_UNAVAILABLE;

	/* empty parent window token: accepted on X11 and by portals that
	 * do not need a handle; "interactive" gives the area-picker UI the
	 * old gnome-screenshot -a invocation provided */
	GVariant * ret = g_dbus_connection_call_sync(bus,
		PORTAL_BUS_NAME, PORTAL_OBJ_PATH,
		"org.freedesktop.portal.Screenshot", "Screenshot",
		g_variant_new("(s@a{sv})", "",
					  g_variant_new_parsed("{'interactive': <%b>, 'modal': <%b>}",
										   TRUE, TRUE)),
		G_VARIANT_TYPE("(o)"), G_DBUS_CALL_FLAGS_NONE, -1,
		nullptr, nullptr);
	if (!ret)
		return UT_SCREENSHOT_UNAVAILABLE;
	const gchar * handle = nullptr;
	g_variant_get(ret, "(&o)", &handle);
	std::string reqPath = handle ? handle : "";
	g_variant_unref(ret);
	if (reqPath.empty())
		return UT_SCREENSHOT_UNAVAILABLE;

	PortalShotCtx ctx;
	ctx.loop = g_main_loop_new(nullptr, FALSE);

	guint respSub = g_dbus_connection_signal_subscribe(bus,
		PORTAL_BUS_NAME, "org.freedesktop.portal.Request", "Response",
		reqPath.c_str(), nullptr,
		G_DBUS_SIGNAL_FLAGS_NONE, s_on_portal_response, &ctx, nullptr);
	guint ownerSub = g_dbus_connection_signal_subscribe(bus,
		"org.freedesktop.DBus", "org.freedesktop.DBus",
		"NameOwnerChanged", "/org/freedesktop/DBus", PORTAL_BUS_NAME,
		G_DBUS_SIGNAL_FLAGS_NONE, s_on_portal_owner, &ctx, nullptr);

	/* the capture UI is user-paced; the nested loop pumps the default
	 * main context (where the subscriptions dispatch) until Response
	 * lands - the same technique modal dialogs use. A generous
	 * failsafe keeps a wedged portal from freezing the app for good */
	ctx.timeoutId = g_timeout_add_seconds(600, +[](gpointer d) -> gboolean
		{
			PortalShotCtx * c = static_cast<PortalShotCtx *>(d);
			c->response = 2;
			g_main_loop_quit(c->loop);
			return G_SOURCE_REMOVE;
		}, &ctx);
	g_main_loop_run(ctx.loop);
	if (ctx.timeoutId)
		g_source_remove(ctx.timeoutId);

	g_dbus_connection_signal_unsubscribe(bus, respSub);
	g_dbus_connection_signal_unsubscribe(bus, ownerSub);
	g_main_loop_unref(ctx.loop);

	if (ctx.response == 1)
		return UT_SCREENSHOT_CANCELLED;
	if (ctx.response != 0 || ctx.uri.empty())
		return UT_SCREENSHOT_UNAVAILABLE;
	gchar * path = g_filename_from_uri(ctx.uri.c_str(), nullptr, nullptr);
	if (!path)
		return UT_SCREENSHOT_UNAVAILABLE;
	outPath = path;
	g_free(path);
	return UT_SCREENSHOT_OK;
}

/* gnome-screenshot fallback: still shipped by GNOME Flashback and
 * friends; -a gives the interactive area picker, -f the output file */
UT_ScreenshotResult s_gnome_capture(std::string & outPath)
{
	gchar * shot = g_find_program_in_path("gnome-screenshot");
	if (!shot)
		return UT_SCREENSHOT_UNAVAILABLE;

	/* secure temp file: a predictable name in the shared tmp dir
	 * would be a symlink-attack vector */
	gchar * tmp = nullptr;
	int fd = g_file_open_tmp("abinova-screenshot-XXXXXX.png",
							 &tmp, nullptr);
	if (fd == -1)
	{
		g_free(shot);
		return UT_SCREENSHOT_UNAVAILABLE;
	}
	g_close(fd, nullptr);

	/* argv form: no shell parsing or quoting involved */
	gchar * argv[] = { shot, const_cast<gchar *>("-a"),
					   const_cast<gchar *>("-f"), tmp, nullptr };
	gint status = 0;
	gboolean ok = g_spawn_sync(nullptr, argv, nullptr,
							   static_cast<GSpawnFlags>(
								   G_SPAWN_STDOUT_TO_DEV_NULL |
								   G_SPAWN_STDERR_TO_DEV_NULL),
							   nullptr, nullptr, nullptr, nullptr,
							   &status, nullptr);
	g_free(shot);

	if (ok && status == 0 && g_file_test(tmp, G_FILE_TEST_IS_REGULAR))
	{
		outPath = tmp;
		g_free(tmp);
		return UT_SCREENSHOT_OK;
	}
	g_unlink(tmp);
	g_free(tmp);
	/* spawn failed outright vs the tool ran and the user dismissed the
	 * area picker (exit status 1) */
	return ok ? UT_SCREENSHOT_CANCELLED : UT_SCREENSHOT_UNAVAILABLE;
}

} // anonymous namespace

bool UT_screenshot_available(void)
{
	/* cached: which capture mechanisms are installed does not change
	 * during a session, and the probe activates the portal service */
	static int s_available = -1;
	if (s_available < 0)
	{
		gchar * shot = g_find_program_in_path("gnome-screenshot");
		s_available = (s_portal_has_screenshot() || shot) ? 1 : 0;
		g_free(shot);
	}
	return s_available != 0;
}

UT_ScreenshotResult UT_screenshot_capture_area(std::string & outPath)
{
	outPath.clear();
	UT_ScreenshotResult res = s_portal_capture(outPath);
	if (res == UT_SCREENSHOT_UNAVAILABLE)
		res = s_gnome_capture(outPath);
	return res;
}
