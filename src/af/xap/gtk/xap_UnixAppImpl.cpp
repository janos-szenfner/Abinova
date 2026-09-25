/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Application Framework
 * Copyright (C) 1998-2000 AbiSource, Inc.
 * Copyright (C) 2004 Hubert Figuiere
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

#include "config.h"

#include <glib.h>
#include <gtk/gtk.h>

#include <string>

#include "xap_UnixAppImpl.h"
#include "xap_UnixHelpWindow.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"
#include "ut_string_class.h"
#include "ut_files.h"
#include "ut_go_file.h"

std::string XAP_UnixAppImpl::localizeHelpUrl(const char * pathBefore,
											const char * pathAfter,
											const char * remoteURLbase)
{
	return XAP_AppImpl::localizeHelpUrl (pathBefore, pathAfter, remoteURLbase);
}

bool XAP_UnixAppImpl::openHelpURL(const char * url)
{
	return openURL(url);
}

/* in-app help browser - reuses the existing window when it is open */
void XAP_UnixAppImpl::openHelpWindow(XAP_Frame * pFrame, const char * page,
									 bool bFocusSearch)
{
	if (m_wHelpWin)
	{
		XAP_UnixHelpWindow * hw = static_cast<XAP_UnixHelpWindow *>(
			g_object_get_data(G_OBJECT(m_wHelpWin), "help-win-obj"));
		if (hw)
			hw->show(page, bFocusSearch);
		return;
	}

	XAP_UnixHelpWindow * hw = new XAP_UnixHelpWindow(pFrame);
	hw->show(page, bFocusSearch);
	m_wHelpWin = hw->window();
	if (m_wHelpWin)
		g_object_weak_ref(G_OBJECT(m_wHelpWin),
						  +[](gpointer d, GObject * /*dead*/)
						  {
							  *static_cast<GtkWidget **>(d) = nullptr;
						  },
						  &m_wHelpWin);
}

bool XAP_UnixAppImpl::openURL(const char * url)
{
	// Need this to make AbiGimp Load!!!!!
	if (progExists("foo")) {}

	GError * err = nullptr;
	err = UT_go_url_show (url);
	if (err) {
		g_warning ("%s", err->message);
		g_error_free (err);
		return FALSE;
	}
	return TRUE;
}

/* -------------------------------------------------- update check --- */

namespace
{

struct AbiUpdateInfo
{
	bool		fetched = false;	/* server answered with a version */
	bool		newer = false;		/* and it is newer than this build */
	std::string	version;
	std::string	url;
};

/* widgets we touch when the background check finishes; all ref'd */
struct AbiUpdateUI
{
	GtkWidget *	dialog;
	GtkWidget *	label;
	GtkWidget *	link;
};

/* blocking HTTPS GET; true when the server gave a 2xx response (body
 * may still be empty, e.g. an empty JSON list), false on any
 * transport or HTTP error */
static bool abi_https_get(const char * host, const char * path,
						  std::string& bodyOut)
{
	bodyOut.clear();
	GError * err = nullptr;
	GSocketClient * client = g_socket_client_new();
	g_socket_client_set_tls(client, TRUE);
	g_socket_client_set_timeout(client, 15);

	GSocketConnection * conn = g_socket_client_connect_to_host(
		client, host, 443, nullptr, &err);
	g_object_unref(client);
	if (!conn)
	{
		g_clear_error(&err);
		return false;
	}

	std::string req = "GET ";
	req += path;
	req += " HTTP/1.1\r\nHost: ";
	req += host;
	req += "\r\nUser-Agent: abiword-update-check\r\n"
		   "Accept: application/vnd.github+json\r\n"
		   "Connection: close\r\n\r\n";

	GOutputStream * out = g_io_stream_get_output_stream(G_IO_STREAM(conn));
	GInputStream * in = g_io_stream_get_input_stream(G_IO_STREAM(conn));
	bool ok = g_output_stream_write_all(out, req.data(), req.size(),
										nullptr, nullptr, &err) == TRUE;
	g_clear_error(&err);
	if (ok)
	{
		std::string resp;
		char buf[4096];
		for (;;)
		{
			gssize n = g_input_stream_read(in, buf, sizeof(buf),
										   nullptr, &err);
			if (n <= 0)
				break;
			resp.append(buf, n);
		}
		g_clear_error(&err);

		size_t sp = resp.find(' ');
		int status = (sp == std::string::npos)
			? 0 : atoi(resp.c_str() + sp + 1);
		size_t bodyPos = resp.find("\r\n\r\n");
		ok = status >= 200 && status < 300 &&
			bodyPos != std::string::npos;
		if (ok)
			bodyOut = resp.substr(bodyPos + 4);
	}
	g_io_stream_close(G_IO_STREAM(conn), nullptr, nullptr);
	g_object_unref(conn);
	return ok;
}

/* minimal "key":"value" extraction - good enough for tag names/URLs */
static std::string abi_json_string(const std::string& json,
								   const char * key)
{
	std::string k = "\"";
	k += key;
	k += "\"";
	size_t p = json.find(k);
	if (p == std::string::npos)
		return std::string();
	p = json.find(':', p + k.size());
	if (p == std::string::npos)
		return std::string();
	p = json.find('"', p + 1);
	if (p == std::string::npos)
		return std::string();
	size_t e = json.find('"', p + 1);
	if (e == std::string::npos)
		return std::string();
	return json.substr(p + 1, e - p - 1);
}

/* parse "v4.0.0"-style tags into numeric components */
static bool abi_parse_version(const std::string& tag, int out[3])
{
	const char * p = tag.c_str();
	while (*p && !g_ascii_isdigit(*p))
		++p;
	out[0] = out[1] = out[2] = 0;
	int n = 0;
	while (*p && n < 3)
	{
		if (!g_ascii_isdigit(*p))
			break;
		while (g_ascii_isdigit(*p))
		{
			out[n] = out[n] * 10 + (*p - '0');
			++p;
		}
		++n;
		if (*p == '.')
			++p;
	}
	return n > 0;
}

static void abi_update_check_thread(GTask * task, gpointer /*source*/,
									gpointer /*data*/,
									GCancellable * /*cancellable*/)
{
	AbiUpdateInfo * info = new AbiUpdateInfo;

	/* newest published release first... */
	std::string body;
	bool answered = abi_https_get(
		"api.github.com",
		"/repos/janos-szenfner/Exp-Abi/releases/latest", body);
	if (answered)
	{
		info->version = abi_json_string(body, "tag_name");
		info->url = abi_json_string(body, "html_url");
	}

	/* ...otherwise fall back to the newest git tag */
	if (info->version.empty())
	{
		body.clear();
		if (abi_https_get("api.github.com",
						  "/repos/janos-szenfner/Exp-Abi/tags", body))
		{
			answered = true;
			info->version = abi_json_string(body, "name");
		}
	}

	/* the check itself succeeded if the server answered at all, even
	 * when the repo simply has no releases/tags yet */
	info->fetched = answered;
	if (info->fetched)
	{
		int cur[3], lat[3];
		if (abi_parse_version(PACKAGE_VERSION, cur) &&
			abi_parse_version(info->version, lat))
		{
			info->newer = lat[0] > cur[0] ||
				(lat[0] == cur[0] && lat[1] > cur[1]) ||
				(lat[0] == cur[0] && lat[1] == cur[1] && lat[2] > cur[2]);
		}
		if (info->url.empty())
		{
			info->url =
				"https://github.com/janos-szenfner/Exp-Abi/releases/tag/" +
				info->version;
		}
	}
	g_task_return_pointer(task, info,
		+[](gpointer p) { delete static_cast<AbiUpdateInfo *>(p); });
}

static void abi_update_check_done(GObject * /*source*/, GAsyncResult * res,
								  gpointer data)
{
	AbiUpdateUI * ui = static_cast<AbiUpdateUI *>(data);
	AbiUpdateInfo * info = static_cast<AbiUpdateInfo *>(
		g_task_propagate_pointer(G_TASK(res), nullptr));

	if (info && info->fetched && info->newer)
	{
		std::string text = "A new version is available: " +
			info->version + "\nYou are running " + PACKAGE_VERSION + ".";
		gtk_label_set_text(GTK_LABEL(ui->label), text.c_str());
		gtk_link_button_set_uri(GTK_LINK_BUTTON(ui->link),
								info->url.c_str());
		gtk_widget_set_visible(ui->link, TRUE);
	}
	else if (info && info->fetched)
	{
		gtk_label_set_text(GTK_LABEL(ui->label),
						   "There is no update at the moment.");
	}
	else
	{
		gtk_label_set_text(GTK_LABEL(ui->label),
						   "Could not check for updates.\n"
						   "Please try again later.");
	}

	g_object_unref(ui->dialog);
	g_object_unref(ui->label);
	g_object_unref(ui->link);
	delete ui;
}

} // anonymous namespace

void XAP_UnixAppImpl::checkForUpdates(XAP_Frame * pFrame)
{
	GtkWindow * parent = nullptr;
	if (pFrame)
	{
		XAP_UnixFrameImpl * pImpl =
			static_cast<XAP_UnixFrameImpl *>(pFrame->getFrameImpl());
		if (pImpl && pImpl->getTopLevelWindow())
			parent = GTK_WINDOW(pImpl->getTopLevelWindow());
	}

	GtkWidget * dlg = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(dlg), "Check for Updates");
	gtk_window_set_resizable(GTK_WINDOW(dlg), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(dlg), 360, -1);
	if (parent)
	{
		gtk_window_set_transient_for(GTK_WINDOW(dlg), parent);
		gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
	}

	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
	gtk_widget_set_margin_start(box, 24);
	gtk_widget_set_margin_end(box, 24);
	gtk_widget_set_margin_top(box, 20);
	gtk_widget_set_margin_bottom(box, 20);

	GtkWidget * label = gtk_label_new("Checking for updates…");
	gtk_label_set_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(box), label);

	GtkWidget * link = gtk_link_button_new_with_label(
		"https://github.com/janos-szenfner/Exp-Abi/releases",
		"Download the latest version");
	gtk_widget_set_halign(link, GTK_ALIGN_CENTER);
	gtk_widget_set_visible(link, FALSE);
	gtk_box_append(GTK_BOX(box), link);

	GtkWidget * closeBtn = gtk_button_new_with_label("Close");
	gtk_widget_set_halign(closeBtn, GTK_ALIGN_CENTER);
	gtk_widget_set_size_request(closeBtn, 110, -1);
	g_signal_connect_swapped(closeBtn, "clicked",
							 G_CALLBACK(gtk_window_destroy), dlg);
	gtk_box_append(GTK_BOX(box), closeBtn);

	gtk_window_set_child(GTK_WINDOW(dlg), box);
	gtk_window_present(GTK_WINDOW(dlg));

	AbiUpdateUI * ui = new AbiUpdateUI;
	ui->dialog = static_cast<GtkWidget *>(g_object_ref(dlg));
	ui->label = static_cast<GtkWidget *>(g_object_ref(label));
	ui->link = static_cast<GtkWidget *>(g_object_ref(link));

	GTask * task = g_task_new(nullptr, nullptr, abi_update_check_done, ui);
	g_task_run_in_thread(task, abi_update_check_thread);
	g_object_unref(task);
}
