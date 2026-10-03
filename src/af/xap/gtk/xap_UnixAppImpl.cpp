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

#include <memory>
#include <string>

#include "xap_UnixAppImpl.h"
#include "xap_UnixHelpWindow.h"
#include "xap_UpdateCheck.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"
#include "ut_string_class.h"
#include "ut_go_file.h"
#include "ut_debugmsg.h"

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

/* widgets we touch when the background check finishes; all ref'd */
struct AbiUpdateUI
{
	GtkWidget *	dialog;
	GtkWidget *	label;
	GtkWidget *	link;
};

static void abi_update_check_thread(GTask * task, gpointer /*source*/,
									gpointer /*data*/,
									GCancellable * /*cancellable*/)
{
	std::unique_ptr<XAP_UpdateInfo> info;
	try
	{
		info.reset(new XAP_UpdateInfo);
		XAP_updateCheckQuery(*info, PACKAGE_VERSION);
	}
	catch (...)
	{
		/* a C++ exception escaping into GLib's C worker-thread frames
		 * would terminate() the process, and skipping g_task_return_*
		 * leaves the task uncompleted and the UI hanging; degrade to
		 * "could not check" (nullptr info) instead */
		info.reset();
	}
	g_task_return_pointer(task, info.release(),
		+[](gpointer p) { delete static_cast<XAP_UpdateInfo *>(p); });
}

static void abi_update_check_done(GObject * /*source*/, GAsyncResult * res,
								  gpointer data)
{
	AbiUpdateUI * ui = static_cast<AbiUpdateUI *>(data);
	XAP_UpdateInfo * info = static_cast<XAP_UpdateInfo *>(
		g_task_propagate_pointer(G_TASK(res), nullptr));

	try
	{
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
			/* name the cause when we know it (no TLS backend vs
			 * unreachable server vs HTTP error) */
			std::string text = "Could not check for updates.\n";
			if (info && !info->errorDetail.empty())
				text += info->errorDetail;
			else
				text += "Please try again later.";
			gtk_label_set_text(GTK_LABEL(ui->label), text.c_str());
		}
	}
	catch (...)
	{
		/* the std::string assembly above can throw; an exception
		 * escaping this main-context callback would terminate() */
		UT_DEBUGMSG(("abi_update_check_done: exception while assembling update text\n"));
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
		"https://github.com/janos-szenfner/Abinova/releases",
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
