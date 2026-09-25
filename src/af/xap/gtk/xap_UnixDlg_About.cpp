/* 
 * Copyright (C) 2006 Rob Staudinger <robert.staudinger@gmail.com>
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

#include <gtk/gtk.h>

#include "ut_string.h"
#include "ut_assert.h"

#include "xap_Dialog_Id.h"
#include "xap_Frame.h"

#include "xap_UnixApp.h"
#include "xap_UnixFrameImpl.h"
#include "xap_UnixDlg_About.h"
#include "xap_UnixDialogHelper.h"

XAP_Dialog * XAP_UnixDialog_About::static_constructor(XAP_DialogFactory * pFactory, XAP_Dialog_Id id)
{
	XAP_UnixDialog_About * p = new XAP_UnixDialog_About(pFactory,id);
	return p;
}

XAP_UnixDialog_About::XAP_UnixDialog_About(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id)
  : XAP_Dialog_About(pDlgFactory, id)
{}

XAP_UnixDialog_About::~XAP_UnixDialog_About(void)
{}

static void onAboutDialogActivate (GtkAboutDialog 	* /*about*/,
								   const gchar 		*link,
								   gpointer 		 /*data*/)
{
	XAP_App::getApp()->openURL(link);
}

void XAP_UnixDialog_About::runModal(XAP_Frame * pFrame)
{
	static const gchar *authors[] = {"Janos Szenfner",
									 "Abi the Ant <abi@abisource.com>",
									 nullptr};

	static const gchar *documenters[] = {"David Chart <linux@dchart.demon.co.uk>",
										 nullptr};

	static const gchar *copyright = "(c) 1998-2012 Dom Lachowicz and other contributors";

	static const gchar *comments = "Experimental GTK4 fork of AbiWord";

	static const gchar *website = "https://github.com/janos-szenfner/Exp-Abi";

	static GtkWidget * dlg = nullptr;

	dlg = gtk_about_dialog_new();
	//JEAN: do we really need the "activate-link" signal?
	g_signal_connect(dlg, "activate-link", G_CALLBACK(onAboutDialogActivate), nullptr);
	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dlg), "Abinova");
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(dlg), authors);
	gtk_about_dialog_set_documenters(GTK_ABOUT_DIALOG(dlg), documenters);
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dlg), copyright);
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dlg), comments);
	gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(dlg), GTK_LICENSE_GPL_2_0);
	// resolve the logo through the icon theme: the app icon is compiled
	// into the GResource, so this works without installed files too
	gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(dlg), "abiword");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dlg), XAP_App::s_szBuild_Version);
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dlg), website);
	gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(dlg), website);
	GtkWidget* parent = pFrame ?
		static_cast<XAP_UnixFrameImpl*>(pFrame->getFrameImpl())->getTopLevelWindow() :
		nullptr;
	if (parent) {
		gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(parent));
		centerDialog(parent, dlg, false);
	}
	/* GtkAboutDialog is a GtkWindow, not a GtkDialog, in GTK4: no
	 * response signal, no action area.  Present it directly; its own
	 * Close button dismisses it. */
	gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
	gtk_window_present(GTK_WINDOW(dlg));
}
