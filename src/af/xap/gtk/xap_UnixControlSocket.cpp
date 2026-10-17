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
#include <unistd.h>
#include <vector>

#include "xap_UnixControlSocket.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"
#include "ev_EditMethod.h"
#include "fv_View.h"
#include "pd_Document.h"
#include "ap_FmtInspect.h"
#include "ap_DocInspect.h"
#include "ie_types.h"
#include "ie_exp.h"
#include "pt_PieceTable.h"
#include "ut_string.h"
#include "ut_path.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#endif

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

static bool _json_bool(const std::string & j, const char * name)
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
	return j.compare(at, 4, "true") == 0;
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
	std::string			key_path;
};

static _ControlState s_ctl;

struct _Conn
{
	GSocketConnection *		conn;
	GDataInputStream *		in;
	GOutputStream *			out;
	/* set by the "auth" method or any request that carried a
	 * correct top-level key — requests after it may omit the key */
	bool					authed = false;
	/* closes an unauthenticated connection after 30 s */
	guint					auth_timer = 0;
};

static void _conn_close(_Conn * c)
{
	if (!c)
		return;
	if (c->auth_timer)
		g_source_remove(c->auth_timer);
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
/* automation path roots                                          */
/*****************************************************************/

static std::vector<std::string> s_roots_read;
static std::vector<std::string> s_roots_write;

/* resolve through symlinks as far as the filesystem allows:
 * realpath() when the path exists, else realpath of the deepest
 * existing ancestor with the remaining components re-appended
 * (the case for a write target that does not exist yet) */
static std::string _resolve_path(const std::string & p)
{
	std::string cur = p;
	std::string tail;
	for (int depth = 0; depth < 64; ++depth)
	{
		gchar * real = realpath(cur.c_str(), nullptr);
		if (real)
		{
			std::string out = real;
			free(real);
			if (!tail.empty())
				out += "/" + tail;
			return out;
		}
		gchar * dir = g_path_get_dirname(cur.c_str());
		gchar * base = g_path_get_basename(cur.c_str());
		if (!dir || std::string(dir) == cur || !base)
		{
			g_free(dir);
			g_free(base);
			break;
		}
		tail = tail.empty()
			? std::string(base)
			: std::string(base) + "/" + tail;
		cur = dir;
		g_free(dir);
		g_free(base);
	}
	gchar * can = g_canonicalize_filename(p.c_str(), nullptr);
	std::string out = can ? can : p;
	g_free(can);
	return out;
}

void abi_automation_add_root(bool write, const char * path)
{
	if (!path || !*path)
		return;
	std::vector<std::string> & roots =
		write ? s_roots_write : s_roots_read;
	roots.push_back(_resolve_path(path));
}

bool abi_automation_path_allowed(bool write, const char * path)
{
	const std::vector<std::string> & roots =
		write ? s_roots_write : s_roots_read;
	if (roots.empty())
		return true;
	if (!path || !*path)
		return false;
	std::string p = path;
	/* MRU/control data can arrive as a file:// URI */
	if (p.compare(0, 7, "file://") == 0)
		p = p.substr(7);
	const std::string rp = _resolve_path(p);
	for (const std::string & root : roots)
	{
		if (rp == root)
			return true;
		if (rp.size() > root.size() &&
			rp.compare(0, root.size(), root) == 0 &&
			rp[root.size()] == '/')
			return true;
	}
	return false;
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

/* edit methods whose params.data is a filesystem path — confined
 * by the automation roots when those are configured */
static bool _cmd_reads_path(const char * cmd)
{
	static const char * const kRead[] = { "openRecent", "updateStyles" };
	for (const char * n : kRead)
		if (!strcmp(cmd, n))
			return true;
	return false;
}

static bool _cmd_writes_path(const char * cmd)
{
	static const char * const kWrite[] = {
		"fileSave", "fileSaveAs", "fileExport", "fileSaveImage",
		"fileSaveAsWeb", "fileSaveTemplate"
	};
	for (const char * n : kWrite)
		if (!strcmp(cmd, n))
			return true;
	return false;
}

static bool _exec_named(const std::string & cmd,
						const std::string & data, std::string & err)
{
	if (!data.empty())
	{
		if (_cmd_reads_path(cmd.c_str()) &&
			!abi_automation_path_allowed(false, data.c_str()))
		{
			err = "params.data outside automation-read-root";
			return false;
		}
		if (_cmd_writes_path(cmd.c_str()) &&
			!abi_automation_path_allowed(true, data.c_str()))
		{
			err = "params.data outside automation-write-root";
			return false;
		}
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

static bool _m_engine_execute(const std::string & j, std::string & err)
{
	std::string cmd;
	if (!_json_str(j, "command", cmd) || cmd.empty())
	{
		err = "missing params.command";
		return false;
	}
	std::string data;
	_json_str(j, "data", data);
	return _exec_named(cmd, data, err);
}

static GtkWidget * _frame_window(XAP_Frame * f)
{
	if (!f)
		return nullptr;
	XAP_UnixFrameImpl * impl =
		static_cast<XAP_UnixFrameImpl *>(f->getFrameImpl());
	return impl ? impl->getTopLevelWindow() : nullptr;
}

static bool _m_widget_png(GtkWidget * wid, const std::string & path,
						  std::string & out, std::string & err)
{
	const int w = gtk_widget_get_width(wid);
	const int h = gtk_widget_get_height(wid);
	if (w <= 0 || h <= 0)
	{
		err = "widget not mapped";
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
		GdkPaintable * paintable = gtk_widget_paintable_new(wid);
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
	GtkNative * nat = gtk_widget_get_native(wid);
	GskRenderer * renderer = nat ? gtk_native_get_renderer(nat) : nullptr;
	if (!renderer)
	{
		gsk_render_node_unref(node);
		err = "no renderer";
		return false;
	}
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

static bool _m_screenshot(const std::string & j, std::string & out,
						  std::string & err)
{
	std::string path;
	if (!_json_str(j, "path", path) || path.empty())
	{
		err = "missing params.path";
		return false;
	}
	if (!abi_automation_path_allowed(true, path.c_str()))
	{
		err = "params.path outside automation-write-root";
		return false;
	}
	GtkWidget * win = _frame_window(_first_frame());
	if (!win)
	{
		err = "frame has no window";
		return false;
	}
	return _m_widget_png(win, path, out, err);
}

static bool _m_engine_commands(std::string & out, std::string & err)
{
	EV_EditMethodContainer * emc =
		s_ctl.app ? s_ctl.app->getEditMethodContainer() : nullptr;
	if (!emc)
	{
		err = "no edit method container";
		return false;
	}
	out = "{\"commands\":[";
	const UT_uint32 n = emc->countEditMethods();
	for (UT_uint32 i = 0; i < n; ++i)
	{
		EV_EditMethod * em = emc->getNthEditMethod(i);
		if (!em)
			continue;
		const char * name = em->getName();
		const char * desc = em->getDescription();
		if (i)
			out += ',';
		out += "{\"name\":\"";
		out += _json_escape(name ? name : "");
		out += "\",\"description\":\"";
		out += _json_escape(desc ? desc : "");
		out += "\"}";
	}
	out += "]}";
	return true;
}

static FV_View * _first_view()
{
	XAP_Frame * f = _first_frame();
	return f ? static_cast<FV_View *>(f->getCurrentView()) : nullptr;
}

static bool _m_doc_text(std::string & out, std::string & err)
{
	FV_View * view = _first_view();
	if (!view)
	{
		err = "no view";
		return false;
	}
	PT_DocPosition posEOD = 0;
	view->getEditableBounds(true, posEOD);
	UT_UCS4Char * buf = posEOD > 0
		? view->getTextBetweenPos(0, posEOD) : nullptr;
	std::string text;
	if (buf)
	{
		text = UT_UTF8String(UT_UCS4String(buf)).utf8_str();
		delete[] buf;
	}
	out = "{\"text\":\"" + _json_escape(text) +
		  "\",\"length\":" + std::to_string(posEOD) + "}";
	return true;
}

static bool _m_doc_inspect(std::string & out, std::string & err)
{
	FV_View * view = _first_view();
	XAP_Frame * f = _first_frame();
	if (!view || !f)
	{
		err = "no view";
		return false;
	}
	const FV_DocCount c = view->countWords();
	const char * name = f->getFilename();
	const PT_DocPosition point = view->getPoint();
	const PT_DocPosition anchor = view->getSelectionAnchor();
	char num[80];
	out = "{";
	out += "\"file\":\"";
	out += _json_escape(name ? name : "");
	out += "\",\"dirty\":";
	out += f->isDirty() ? "true" : "false";
	g_snprintf(num, sizeof(num), "%u", (unsigned) point);
	out += ",\"point\":";  out += num;
	g_snprintf(num, sizeof(num), "%u", (unsigned) anchor);
	out += ",\"anchor\":"; out += num;
	g_snprintf(num, sizeof(num), "%u", (unsigned) c.word);
	out += ",\"words\":";  out += num;
	g_snprintf(num, sizeof(num), "%u", (unsigned) c.para);
	out += ",\"paras\":";  out += num;
	g_snprintf(num, sizeof(num), "%u", (unsigned) c.ch_no);
	out += ",\"chars\":";  out += num;
	g_snprintf(num, sizeof(num), "%u", (unsigned) c.page);
	out += ",\"pages\":";  out += num;
	PD_Document * doc = static_cast<PD_Document *>(f->getCurrentDoc());
	std::string sTemplate;
	if (doc && doc->getMetaDataProp("attached-template", sTemplate))
	{
		out += ",\"attachedTemplate\":\"";
		out += _json_escape(sTemplate.c_str());
		out += "\"";
	}
	out += "}";
	return true;
}

/* Reveal Formatting report at the caret — the same sectioned text the
 * docked Formatting pane shows (stylesInspect) */
static bool _m_doc_fmtinspect(std::string & out, std::string & err)
{
	FV_View * view = _first_view();
	if (!view)
	{
		err = "no view";
		return false;
	}
	std::vector<AP_FmtReportSection> secs;
	AP_FmtInspect::buildReport(view, secs);
	out = "{\"report\":\"" + _json_escape(AP_FmtInspect::toText(secs)) +
		  "\"}";
	return true;
}

/* Document Inspector report — the same sectioned text the
 * fileInspectDoc dialog summarizes (W12) */
static bool _m_doc_privacyinspect(std::string & out, std::string & err)
{
	FV_View * view = _first_view();
	if (!view || !view->getDocument())
	{
		err = "no view";
		return false;
	}
	AP_PrivacyReport rep;
	AP_DocInspect::inspect(view->getDocument(), rep);
	out = "{\"report\":\"" + _json_escape(AP_DocInspect::toText(rep)) +
		  "\"}";
	return true;
}

/* run one autosave cycle synchronously — the timer path otherwise
 * waits out the configured period in minutes, which would make the
 * recovery machinery untestable from automation.  Reports the
 * recovery path and whether it exists afterwards */
static bool _m_doc_autosave(std::string & out, std::string & err)
{
	XAP_Frame * f = _first_frame();
	if (!f)
	{
		err = "no frame";
		return false;
	}
	f->autosaveTick();
	std::string path = f->makeBackupName();
	out = "{\"path\":\"" + _json_escape(path) +
		  "\",\"exists\":" +
		  std::string(!path.empty() && UT_isRegularFile(path.c_str())
						  ? "true" : "false") +
		  ",\"dirty\":" +
		  std::string(f->isDirty() ? "true" : "false") + "}";
	return true;
}

/* dialog-free save: params.path is the target file; honours the
 * automation write root when configured.  params.password (optional)
 * applies a save password for exporters that support encryption */
static bool _m_file_save(const std::string & j, std::string & err)
{
	std::string path;
	if (!_json_str(j, "path", path) || path.empty())
	{
		err = "missing params.path";
		return false;
	}
	if (!abi_automation_path_allowed(true, path.c_str()))
	{
		err = "params.path outside automation-write-root";
		return false;
	}
	FV_View * view = _first_view();
	if (!view)
	{
		err = "no view";
		return false;
	}
	std::string pw;
	if (_json_str(j, "password", pw) && !pw.empty())
	{
		XAP_Frame * f = _first_frame();
		PD_Document * doc =
			f ? static_cast<PD_Document *>(f->getCurrentDoc()) : nullptr;
		if (doc)
			doc->setSavePassword(pw);
	}
	/* resolve the exporter from the suffix — IEFT_Unknown does not
	 * infer a format and constructExporter fails with it */
	const char * dot = strrchr(path.c_str(), '.');
	IEFileType ft = IE_Exp::fileTypeForSuffix(dot ? dot : "");
	if (ft == IEFT_Unknown)
		ft = IE_Exp::fileTypeForSuffix(".abw");
	UT_Error e = view->cmdSaveAs(path.c_str(), ft);
	if (e != UT_OK)
	{
		err = "save failed: ";
		err += std::to_string((int) e);
		return false;
	}
	return true;
}

/* closes frames after the reply has been written — closing the
 * last frame exits the app mid-dispatch otherwise.  Routed through
 * the closeWindowX edit method so the full frame lifecycle runs
 * (view teardown, frame removal, app exit on last close) the same
 * as a title-bar close */
struct _CloseJob { std::vector<XAP_Frame *> frames; };

static gboolean _s_close_frames(gpointer data)
{
	_CloseJob * job = static_cast<_CloseJob *>(data);
	EV_EditMethodContainer * emc =
		s_ctl.app ? s_ctl.app->getEditMethodContainer() : nullptr;
	const EV_EditMethod * em =
		emc ? emc->findEditMethodByName("closeWindowX") : nullptr;
	for (XAP_Frame * f : job->frames)
	{
		if (f && em)
			em->Fn(f->getCurrentView(), nullptr);
	}
	delete job;
	return G_SOURCE_REMOVE;
}

static bool _m_file_close(const std::string & j, std::string & err)
{
	XAP_Frame * f = _first_frame();
	if (!f)
	{
		err = "no frame";
		return false;
	}
	const bool force = _json_bool(j, "force");
	if (f->isDirty() && !force)
	{
		err = "document has unsaved changes";
		return false;
	}
	if (force)
	{
		PD_Document * doc = static_cast<PD_Document *>(f->getCurrentDoc());
		if (doc && doc->getPieceTable())
			doc->getPieceTable()->setClean();
	}
	_CloseJob * job = new _CloseJob;
	job->frames.push_back(f);
	g_idle_add(_s_close_frames, job);
	return true;
}

static bool _m_app_quit(const std::string & j, std::string & err)
{
	XAP_App * app = s_ctl.app;
	if (!app)
	{
		err = "no app";
		return false;
	}
	const bool force = _json_bool(j, "force");
	_CloseJob * job = new _CloseJob;
	int dirty = 0;
	for (UT_uint32 i = 0; i < app->getFrameCount(); ++i)
	{
		XAP_Frame * f = app->getFrame(i);
		if (!f)
			continue;
		job->frames.push_back(f);
		if (f->isDirty())
			++dirty;
	}
	if (dirty && !force)
	{
		delete job;
		err = std::to_string(dirty) +
			  " document(s) have unsaved changes";
		return false;
	}
	if (force)
	{
		for (XAP_Frame * f : job->frames)
		{
			PD_Document * doc = static_cast<PD_Document *>(f->getCurrentDoc());
			if (doc && doc->getPieceTable())
				doc->getPieceTable()->setClean();
		}
	}
	g_idle_add(_s_close_frames, job);
	return true;
}

static bool _m_ui_focus(std::string & err)
{
	GtkWidget * win = _frame_window(_first_frame());
	if (!win)
	{
		err = "frame has no window";
		return false;
	}
	gtk_window_present(GTK_WINDOW(win));
	return true;
}

static bool _m_ui_resize(const std::string & j, std::string & err)
{
	long w = 0, h = 0;
	if (!_json_num(j, "width", w) || !_json_num(j, "height", h) ||
		w < 16 || h < 16 || w > 16384 || h > 16384)
	{
		err = "bad width/height (16..16384)";
		return false;
	}
	GtkWidget * win = _frame_window(_first_frame());
	if (!win)
	{
		err = "frame has no window";
		return false;
	}
	gtk_window_unmaximize(GTK_WINDOW(win));
	gtk_window_set_default_size(GTK_WINDOW(win),
								(int) w, (int) h);
	return true;
}

/* the document drawing area, reached without an ap-layer include:
 * largest mapped GtkDrawingArea below the toplevel */
static void _find_drawing_area(GtkWidget * w, GtkWidget ** best,
							   long * best_px)
{
	if (!w)
		return;
	if (GTK_IS_DRAWING_AREA(w))
	{
		const long px = (long) gtk_widget_get_width(w) *
						(long) gtk_widget_get_height(w);
		if (px > *best_px)
		{
			*best_px = px;
			*best = w;
		}
	}
	for (GtkWidget * c = gtk_widget_get_first_child(w); c;
		 c = gtk_widget_get_next_sibling(c))
		_find_drawing_area(c, best, best_px);
}

static bool _m_ui_render(const std::string & j, std::string & out,
						 std::string & err)
{
	std::string path;
	if (!_json_str(j, "path", path) || path.empty())
	{
		err = "missing params.path";
		return false;
	}
	if (!abi_automation_path_allowed(true, path.c_str()))
	{
		err = "params.path outside automation-write-root";
		return false;
	}
	GtkWidget * area = nullptr;
	long px = 0;
	_find_drawing_area(_frame_window(_first_frame()), &area, &px);
	if (!area)
	{
		err = "no document view";
		return false;
	}
	return _m_widget_png(area, path, out, err);
}

static bool _m_ui_text(const std::string & j, std::string & err)
{
	std::string text;
	if (!_json_str(j, "text", text) || text.empty())
	{
		err = "missing params.text";
		return false;
	}
	return _exec_named("insertData", text, err);
}

#ifdef GDK_WINDOWING_X11
static Display * _x_display()
{
	GdkDisplay * d = gdk_display_get_default();
	return GDK_IS_X11_DISPLAY(d) ? GDK_DISPLAY_XDISPLAY(d) : nullptr;
}

static Window _x_frame_window()
{
	GtkWidget * win = _frame_window(_first_frame());
	GdkSurface * s = win ? gtk_native_get_surface(GTK_NATIVE(win))
					   : nullptr;
	return s && GDK_IS_X11_SURFACE(s)
		? gdk_x11_surface_get_xid(s) : 0;
}

static unsigned _x_keysym_for(const char * name)
{
	const guint kv = gdk_keyval_from_name(name);
	if (kv == GDK_KEY_VoidSymbol)
		return 0;
	KeySym ks = XStringToKeysym(gdk_keyval_name(kv));
	return ks ? (unsigned) XKeysymToKeycode(_x_display(), ks) : 0;
}

static bool _m_ui_key(const std::string & j, std::string & err)
{
	/* params.key must be read inside "params" — a bare scan hits the
	 * top-level auth "key" first */
	const size_t pAt = j.find("\"params\"");
	const std::string tail =
		(pAt == std::string::npos) ? j : j.substr(pAt);
	std::string spec;
	if (!_json_str(tail, "key", spec) || spec.empty())
	{
		err = "missing params.key";
		return false;
	}
	Display * dpy = _x_display();
	Window xid = _x_frame_window();
	if (!dpy || !xid)
	{
		err = "ui.key requires the X11 backend";
		return false;
	}
	/* split "ctrl+shift+F5" into modifiers + final key */
	std::vector<unsigned> mods;
	std::string key_name;
	size_t start = 0;
	for (size_t at = 0; at <= spec.size(); ++at)
	{
		if (at != spec.size() && spec[at] != '+')
			continue;
		std::string part = spec.substr(start, at - start);
		start = at + 1;
		if (part.empty())
			continue;
		std::string low = part;
		for (size_t i = 0; i < low.size(); ++i)
			low[i] = (char) g_ascii_tolower(low[i]);
		const char * sym = nullptr;
		if (low == "ctrl" || low == "control")
			sym = "Control_L";
		else if (low == "shift")
			sym = "Shift_L";
		else if (low == "alt" || low == "mod1")
			sym = "Alt_L";
		else if (low == "super" || low == "meta" || low == "win")
			sym = "Super_L";
		if (sym)
		{
			const unsigned kc = _x_keysym_for(sym);
			if (!kc)
			{
				err = "bad modifier: " + part;
				return false;
			}
			mods.push_back(kc);
		}
		else if (at == spec.size())
			key_name = part;
		else
		{
			err = "unknown key: " + part;
			return false;
		}
	}
	const unsigned kc = _x_keysym_for(key_name.c_str());
	if (!kc)
	{
		err = "unknown key: " + key_name;
		return false;
	}
	/* focus the frame so the synthetic input lands in the app
	 * the same way a real WM-delivered event would */
	XSetInputFocus(dpy, xid, RevertToPointerRoot, CurrentTime);
	for (unsigned m : mods)
		XTestFakeKeyEvent(dpy, m, True, CurrentTime);
	XTestFakeKeyEvent(dpy, kc, True, CurrentTime);
	XTestFakeKeyEvent(dpy, kc, False, CurrentTime);
	for (auto it = mods.rbegin(); it != mods.rend(); ++it)
		XTestFakeKeyEvent(dpy, *it, False, CurrentTime);
	XSync(dpy, False);
	return true;
}

static bool _m_ui_click(const std::string & j, std::string & err)
{
	long x = 0, y = 0, button = 1;
	if (!_json_num(j, "x", x) || !_json_num(j, "y", y) ||
		x < 0 || y < 0 || x > 16384 || y > 16384)
	{
		err = "missing/bad params.x,params.y";
		return false;
	}
	_json_num(j, "button", button);
	if (button < 1 || button > 9)
		button = 1;
	const bool dbl = _json_bool(j, "double");
	Display * dpy = _x_display();
	Window xid = _x_frame_window();
	if (!dpy || !xid)
	{
		err = "ui.click requires the X11 backend";
		return false;
	}
	Window root = DefaultRootWindow(dpy), child = 0;
	int rx = 0, ry = 0;
	if (!XTranslateCoordinates(dpy, xid, root,
							   (int) x, (int) y, &rx, &ry, &child))
	{
		err = "coordinate translation failed";
		return false;
	}
	XTestFakeMotionEvent(dpy, -1, rx, ry, CurrentTime);
	/* params.ctrl/shift/alt hold the modifier for the click (needed
	 * to drive modifier-click behaviours like TOC navigation) */
	struct { const char * key; const char * sym; } const kMods[] = {
		{ "ctrl", "Control_L" }, { "shift", "Shift_L" },
		{ "alt", "Alt_L" }, { "meta", "Meta_L" }
	};
	KeyCode mods[G_N_ELEMENTS(kMods)];
	unsigned nMods = 0;
	for (const auto & m : kMods)
	{
		if (_json_bool(j, m.key))
		{
			mods[nMods++] =
				XKeysymToKeycode(dpy, XStringToKeysym(m.sym));
		}
	}
	for (unsigned i = 0; i < nMods; ++i)
		XTestFakeKeyEvent(dpy, mods[i], True, CurrentTime);
	const int clicks = dbl ? 2 : 1;
	for (int i = 0; i < clicks; ++i)
	{
		XTestFakeButtonEvent(dpy, (unsigned) button, True,
							 CurrentTime);
		XSync(dpy, False);
		usleep(20000);
		XTestFakeButtonEvent(dpy, (unsigned) button, False,
							 CurrentTime);
		XSync(dpy, False);
		if (dbl)
			usleep(40000);
	}
	for (unsigned i = 0; i < nMods; ++i)
		XTestFakeKeyEvent(dpy, mods[i], False, CurrentTime);
	XSync(dpy, False);
	return true;
}
#else
static bool _m_ui_key(const std::string &, std::string & err)
{
	err = "ui.key requires the X11 backend";
	return false;
}
static bool _m_ui_click(const std::string &, std::string & err)
{
	err = "ui.click requires the X11 backend";
	return false;
}
#endif

/* returns false when the connection must close after the reply:
 * wordcraft-compatible behaviour for auth failures, oversized
 * lines and non-JSON input (so an HTTP probe can't reach the
 * dispatcher) */
static bool _dispatch(_Conn * c, const std::string & line)
{
	long id = 0;
	_json_num(line, "id", id);

	/* anything that isn't a JSON object gets one error reply */
	size_t i = line.find_first_not_of(" \t\r\n");
	if (i == std::string::npos || line[i] != '{')
	{
		_conn_send(c, id, nullptr, "bad request");
		return false;
	}

	std::string method, key;
	_json_str(line, "method", method);
	_json_str(line, "key", key);

	if (method == "auth")
	{
		if (key == s_ctl.key)
		{
			c->authed = true;
			if (c->auth_timer)
			{
				g_source_remove(c->auth_timer);
				c->auth_timer = 0;
			}
			_conn_send(c, id, "{}", nullptr);
		}
		else
		{
			_conn_send(c, id, nullptr, "unauthorized");
			return false;
		}
		return true;
	}

	if (!c->authed)
	{
		if (!key.empty() && key == s_ctl.key)
			c->authed = true;
		else
		{
			_conn_send(c, id, nullptr, "unauthorized");
			return false;
		}
	}
	if (method.empty())
	{
		_conn_send(c, id, nullptr, "missing method");
		return true;
	}

	std::string err;
	if (method == "ping")
	{
		_conn_send(c, id, "\"pong\"", nullptr);
		return true;
	}
	if (method == "engine.execute")
	{
		if (_m_engine_execute(line, err))
			_conn_send(c, id, "{\"ok\":true}", nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "ui.screenshot")
	{
		std::string out;
		if (_m_screenshot(line, out, err))
			_conn_send(c, id, out.c_str(), nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "engine.commands")
	{
		std::string out;
		if (_m_engine_commands(out, err))
			_conn_send(c, id, out.c_str(), nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "document.text")
	{
		std::string out;
		if (_m_doc_text(out, err))
			_conn_send(c, id, out.c_str(), nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "document.inspect")
	{
		std::string out;
		if (_m_doc_inspect(out, err))
			_conn_send(c, id, out.c_str(), nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "document.formatinspect")
	{
		std::string out;
		if (_m_doc_fmtinspect(out, err))
			_conn_send(c, id, out.c_str(), nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "document.privacyinspect")
	{
		std::string out;
		if (_m_doc_privacyinspect(out, err))
			_conn_send(c, id, out.c_str(), nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "document.autosave")
	{
		std::string out;
		if (_m_doc_autosave(out, err))
			_conn_send(c, id, out.c_str(), nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "file.save")
	{
		if (_m_file_save(line, err))
			_conn_send(c, id, "{\"ok\":true}", nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "file.close")
	{
		if (_m_file_close(line, err))
			_conn_send(c, id, "{\"closed\":true}", nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "app.quit")
	{
		if (_m_app_quit(line, err))
			_conn_send(c, id, "{\"ok\":true}", nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "ui.focus")
	{
		if (_m_ui_focus(err))
			_conn_send(c, id, "{\"ok\":true}", nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "ui.resize")
	{
		if (_m_ui_resize(line, err))
			_conn_send(c, id, "{\"ok\":true}", nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "ui.render")
	{
		std::string out;
		if (_m_ui_render(line, out, err))
			_conn_send(c, id, out.c_str(), nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "ui.text")
	{
		if (_m_ui_text(line, err))
			_conn_send(c, id, "{\"ok\":true}", nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "ui.key")
	{
		if (_m_ui_key(line, err))
			_conn_send(c, id, "{\"ok\":true}", nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	if (method == "ui.click")
	{
		if (_m_ui_click(line, err))
			_conn_send(c, id, "{\"ok\":true}", nullptr);
		else
			_conn_send(c, id, nullptr, err.c_str());
		return true;
	}
	_conn_send(c, id, nullptr, "unknown method");
	return true;
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
	bool keep = true;
	if (len)
	{
		if (len > 1048576)
		{
			_conn_send(c, 0, nullptr, "line too long");
			keep = false;
		}
		else
			keep = _dispatch(c, std::string(line, len));
	}
	g_free(line);
	if (!keep)
	{
		_conn_close(c);
		return;
	}
	g_data_input_stream_read_line_async(
		c->in, G_PRIORITY_DEFAULT, nullptr, _s_line_ready, c);
}

static gboolean _s_auth_timeout(gpointer data)
{
	_Conn * c = static_cast<_Conn *>(data);
	c->auth_timer = 0;
	if (!c->authed)
		_conn_close(c);
	return G_SOURCE_REMOVE;
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
	c->auth_timer = g_timeout_add_seconds(30, _s_auth_timeout, c);
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
	s_ctl.key_path = _key_path(port);
	gchar * dir = g_path_get_dirname(s_ctl.key_path.c_str());
	g_mkdir_with_parents(dir, 0700);
	g_free(dir);
	g_file_set_contents(s_ctl.key_path.c_str(), s_ctl.key.c_str(),
						-1, nullptr);
	g_chmod(s_ctl.key_path.c_str(), 0600);
	/* a stale key file must never outlive the session it unlocks */
	atexit(+[] {
		if (!s_ctl.key_path.empty())
			g_remove(s_ctl.key_path.c_str());
	});

	g_signal_connect(s_ctl.service, "incoming",
					 G_CALLBACK(_s_incoming), nullptr);
	g_socket_service_start(s_ctl.service);
	g_message("control channel listening on 127.0.0.1:%d", port);
	return true;
}
