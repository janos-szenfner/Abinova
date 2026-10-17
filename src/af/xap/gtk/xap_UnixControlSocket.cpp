/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* Abinova: a word processor
 *
 * Copyright (C) 2024 Abinova contributors
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
 */

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <cairo.h>
#include <gsk/gsk.h>
#include <string.h>

#include "xap_UnixControlSocket.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"
#include "ev_EditMethod.h"

/*****************************************************************/
/* wire format                                                    */
/*****************************************************************/

/* pull "name":"value" out of our small request lines.  Handles
 * \" and \\ escapes; good enough for the flat, self-produced
 * protocol — hostile-but-well-formed JSON elsewhere in the line
 * just yields no match */
static bool _json_str(const std::string & j, const char * name,
					  std::string & out)
{
	std::string pat = "\"";
	pat += name;
	pat += "\"";
	size_t at = j.find(pat);
	if (at == std::string::npos)
		return false;
	at = j.find(':', at + pat.size());
	if (at == std::string::npos)
		return false;
	++at;
	while (at < j.size() &&
		   (j[at] == ' ' || j[at] == '\t'))
		++at;
	if (at >= j.size() || j[at] != '"')
		return false;
	++at;
	out.clear();
	for (; at < j.size() && j[at] != '"'; ++at)
	{
		if (j[at] == '\\' && at + 1 < j.size())
		{
			++at;
			switch (j[at])
			{
				case 'n':	out += '\n'; break;
				case 't':	out += '\t'; break;
				case 'r':	out += '\r'; break;
				default:	out += j[at]; break;
			}
		}
		else
			out += j[at];
	}
	return true;
}

static bool _json_num(const std::string & j, const char * name,
					  long & out)
{
	std::string pat = "\"";
	pat += name;
	pat += "\"";
	size_t at = j.find(pat);
	if (at == std::string::npos)
		return false;
	at = j.find(':', at + pat.size());
	if (at == std::string::npos)
		return false;
	++at;
	while (at < j.size() &&
		   (j[at] == ' ' || j[at] == '\t'))
		++at;
	char * e = nullptr;
	out = strtol(j.c_str() + at, &e, 10);
	return e != j.c_str() + at;
}

static std::string _json_escape(const std::string & s)
{
	std::string o;
	for (char c : s)
	{
		switch (c)
		{
			case '"':	o += "\\\""; break;
			case '\\':	o += "\\\\"; break;
			case '\n':	o += "\\n"; break;
			case '\r':	o += "\\r"; break;
			case '\t':	o += "\\t"; break;
			default:	o += c;
		}
	}
	return o;
}

/*****************************************************************/
/* server state                                                   */
/*****************************************************************/

struct _ControlState
{
	XAP_App *			app = nullptr;
	GSocketService *	service = nullptr;
	std::string			key;
};

static _ControlState s_ctl;

struct _Conn
{
	GSocketConnection *		conn;
	GDataInputStream *		in;
	GOutputStream *			out;
};

static void _conn_close(_Conn * c)
{
	if (!c)
		return;
	if (c->in)
		g_object_unref(c->in);
	if (c->conn)
	{
		g_io_stream_close(G_IO_STREAM(c->conn),
						  nullptr, nullptr);
		g_object_unref(c->conn);
	}
	delete c;
}

static void _conn_send(_Conn * c, long id, const char * result,
					   const char * err)
{
	std::string r = "{\"id\":";
	char num[32];
	g_snprintf(num, sizeof(num), "%ld", id);
	r += num;
	if (err)
	{
		r += ",\"error\":{\"message\":\"";
		r += _json_escape(err);
		r += "\"}";
	}
	else
	{
		r += ",\"result\":";
		r += result ? result : "true";
	}
	r += "}\n";
	g_output_stream_write(c->out, r.data(), r.size(),
						  nullptr, nullptr);
}

/*****************************************************************/
/* methods                                                        */
/*****************************************************************/

static XAP_Frame * _first_frame()
{
	XAP_App * app = s_ctl.app;
	if (!app)
		return nullptr;
	XAP_Frame * f = app->getLastFocussedFrame();
	if (f)
		return f;
	return app->getFrameCount() > 0 ? app->getFrame(0) : nullptr;
}

static bool _m_engine_execute(const std::string & j, std::string & err)
{
	std::string cmd;
	if (!_json_str(j, "command", cmd) || cmd.empty())
	{
		err = "missing params.command";
		return false;
	}
	EV_EditMethodContainer * emc =
		s_ctl.app ? s_ctl.app->getEditMethodContainer() : nullptr;
	EV_EditMethod * em =
		emc ? emc->findEditMethodByName(cmd.c_str()) : nullptr;
	if (!em)
	{
		err = "unknown command: " + cmd;
		return false;
	}
	XAP_Frame * f = _first_frame();
	AV_View * view = f ? f->getCurrentView() : nullptr;
	const EV_EditMethodType t = em->getType();
	if (!view && !(t & EV_EMT_APP_METHOD))
	{
		err = "no view for document command";
		return false;
	}
	std::string data;
	_json_str(j, "data", data);
	if ((t & EV_EMT_REQUIREDATA) && data.empty())
	{
		err = "command requires params.data";
		return false;
	}
	EV_EditMethodCallData emcd(
		static_cast<const char *>(data.c_str()),
		static_cast<UT_uint32>(data.size()));
	const bool ok = em->Fn(view, &emcd);
	if (!ok)
	{
		err = "command returned false";
		return false;
	}
	return true;
}

static bool _m_screenshot(const std::string & j, std::string & out,
						  std::string & err)
{
	std::string path;
	if (!_json_str(j, "path", path) || path.empty())
	{
		err = "missing params.path";
		return false;
	}
	XAP_Frame * f = _first_frame();
	if (!f)
	{
		err = "no frame";
		return false;
	}
	XAP_UnixFrameImpl * impl =
		static_cast<XAP_UnixFrameImpl *>(f->getFrameImpl());
	GtkWidget * win = impl ? impl->getTopLevelWindow() : nullptr;
	if (!win)
	{
		err = "frame has no window";
		return false;
	}
	const int w = gtk_widget_get_width(win);
	const int h = gtk_widget_get_height(win);
	if (w <= 0 || h <= 0)
	{
		err = "window not mapped";
		return false;
	}

	/* a toplevel has no parent to snapshot through — the widget
	 * paintable renders the widget plus its subtree itself.
	 * An in-flight invalidate can leave the first snapshot empty;
	 * retry once */
	GskRenderNode * node = nullptr;
	for (int tries = 0; tries < 2 && !node; ++tries)
	{
		GtkSnapshot * snap = gtk_snapshot_new();
		GdkPaintable * paintable = gtk_widget_paintable_new(win);
		gdk_paintable_snapshot(paintable, snap, w, h);
		g_object_unref(paintable);
		node = gtk_snapshot_free_to_node(snap);
	}
	if (!node)
	{
		err = "snapshot produced no render node";
		return false;
	}
	/* run the node through the window's own renderer — plain
	 * gsk_render_node_draw into a fresh cairo surface comes out
	 * empty under headless X11 */
	GskRenderer * renderer = gtk_native_get_renderer(GTK_NATIVE(win));
	const graphene_rect_t vp =
		GRAPHENE_RECT_INIT(0.f, 0.f, (float) w, (float) h);
	GdkTexture * tex =
		gsk_renderer_render_texture(renderer, node, &vp);
	gsk_render_node_unref(node);
	if (!tex)
	{
		err = "render to texture failed";
		return false;
	}
	cairo_surface_t * surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, w, h);
	gdk_texture_download(tex, cairo_image_surface_get_data(surf),
						 cairo_image_surface_get_stride(surf));
	g_object_unref(tex);
	cairo_surface_mark_dirty(surf);
	/* texture rows are premultiplied BGRA — swap to cairo's
	 * premultiplied BGRA on little-endian X11 (identical layout) */
	const cairo_status_t st =
		cairo_surface_write_to_png(surf, path.c_str());
	cairo_surface_destroy(surf);
	if (st != CAIRO_STATUS_SUCCESS)
	{
		err = "png write failed";
		return false;
	}
	out = "{\"path\":\"" + _json_escape(path) +
		  "\",\"width\":" + std::to_string(w) +
		  ",\"height\":" + std::to_string(h) + "}";
	return true;
}

static void _dispatch(_Conn * c, const std::string & line)
{
	long id = 0;
	_json_num(line, "id", id);

	std::string key;
	if (!_json_str(line, "key", key) || key != s_ctl.key)
	{
		_conn_send(c, id, nullptr, "bad key");
		return;
	}
	std::string method;
	if (!_json_str(line, "method", method))
	{
		_conn_send(c, id, nullptr, "missing method");
		return;
	}
	std::string err;
	if (method == "ping")
	{
		_conn_send(c, id, "\"pong\"", nullptr);
		return;
	}
	if (method == "engine.execute")
	{
		if (_m_engine_execute(line, err))
			_conn_send(c, id, "{\"ok\":true}", nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return;
	}
	if (method == "ui.screenshot")
	{
		std::string out;
		if (_m_screenshot(line, out, err))
			_conn_send(c, id, out.c_str(), nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return;
	}
	_conn_send(c, id, nullptr, "unknown method");
}

/*****************************************************************/
/* connection plumbing                                            */
/*****************************************************************/

static void _s_line_ready(GObject * src, GAsyncResult * res,
						  gpointer data)
{
	_Conn * c = static_cast<_Conn *>(data);
	gsize len = 0;
	char * line = g_data_input_stream_read_line_finish(
		c->in, res, &len, nullptr);
	if (!line)
	{
		_conn_close(c);
		return;
	}
	if (len && len < 65536)
		_dispatch(c, std::string(line, len));
	g_free(line);
	g_data_input_stream_read_line_async(
		c->in, G_PRIORITY_DEFAULT, nullptr, _s_line_ready, c);
}

static gboolean _s_incoming(GSocketService * /*svc*/,
							GSocketConnection * conn,
							GObject * /*src*/, gpointer /*data*/)
{
	_Conn * c = new _Conn;
	c->conn = G_SOCKET_CONNECTION(g_object_ref(conn));
	c->in = g_data_input_stream_new(
		g_io_stream_get_input_stream(G_IO_STREAM(conn)));
	c->out = g_io_stream_get_output_stream(G_IO_STREAM(conn));
	g_data_input_stream_read_line_async(
		c->in, G_PRIORITY_DEFAULT, nullptr, _s_line_ready, c);
	return G_SOURCE_CONTINUE;
}

static std::string _key_path(int port)
{
	const char * dir = XAP_App::getApp()->getUserPrivateDirectory();
	char name[80];
	g_snprintf(name, sizeof(name), "/control-key.local-%d", port);
	return std::string(dir ? dir : "") + name;
}

bool abi_control_start(XAP_App * pApp, int port)
{
	if (!pApp || s_ctl.service)
		return s_ctl.service != nullptr;
	s_ctl.app = pApp;
	if (port <= 0 || port > 65535)
		return false;

	GError * ge = nullptr;
	GSocketAddress * addr = g_inet_socket_address_new_from_string(
		"127.0.0.1", static_cast<guint16>(port));
	s_ctl.service = g_socket_service_new();
	const bool ok = g_socket_listener_add_address(
		G_SOCKET_LISTENER(s_ctl.service), addr,
		G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP,
		nullptr, nullptr, &ge);
	g_object_unref(addr);
	if (!ok)
	{
		UT_DEBUGMSG(("control channel: %s\n",
					 ge ? ge->message : "bind failed"));
		if (ge)
			g_error_free(ge);
		g_clear_object(&s_ctl.service);
		return false;
	}

	/* per-instance auth key, next to ribbon.json */
	char key[33];
	for (int i = 0; i < 32; ++i)
		key[i] = "0123456789abcdef"[g_random_int_range(0, 16)];
	key[32] = 0;
	s_ctl.key = key;
	gchar * dir = g_path_get_dirname(_key_path(port).c_str());
	g_mkdir_with_parents(dir, 0700);
	g_free(dir);
	g_file_set_contents(_key_path(port).c_str(), s_ctl.key.c_str(),
						-1, nullptr);
	g_chmod(_key_path(port).c_str(), 0600);

	g_signal_connect(s_ctl.service, "incoming",
					 G_CALLBACK(_s_incoming), nullptr);
	g_socket_service_start(s_ctl.service);
	g_message("control channel listening on 127.0.0.1:%d", port);
	return true;
}
