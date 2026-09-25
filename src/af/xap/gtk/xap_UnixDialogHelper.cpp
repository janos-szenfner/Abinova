/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Program Utilities
 * Copyright (C) 1998-2000 AbiSource, Inc.
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

/*
 * Port to Maemo Development Platform 
 * Author: INdT - Renato Araujo <renato.filho@indt.org.br>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include <string>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/Xlib.h>
#endif

#include <gdk/gdkkeysyms.h>

#include "ut_debugmsg.h"
#include "ut_assert.h"
#include "ut_string.h"
#include "ut_std_string.h"
#include "xav_View.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"
#include "xap_App.h"
#include "xap_UnixDialogHelper.h"
#include "xap_Dialog.h"
#include "xap_Strings.h"

/*****************************************************************/
/*****************************************************************/

static void focus_in_event(GtkEventControllerFocus* /*controller*/, GtkWidget *widget)
{
      XAP_Frame *pFrame=static_cast<XAP_Frame *>(g_object_get_data(G_OBJECT(widget), "frame"));
	  if (pFrame && pFrame->getCurrentView())
		  pFrame->getCurrentView()->focusChange(AV_FOCUS_NEARBY);
}

static void focus_out_event(GtkEventControllerFocus* /*controller*/, GtkWidget *widget)
{
      XAP_Frame *pFrame=static_cast<XAP_Frame *>(g_object_get_data(G_OBJECT(widget), "frame"));
      if(pFrame == nullptr) return;
      AV_View * pView = pFrame->getCurrentView();
      if(pView!= nullptr)
      {
	     pView->focusChange(AV_FOCUS_NONE);
      }
}

static void focus_out_event_Modeless(GtkEventControllerFocus* /*controller*/, GtkWidget *widget)
{
      XAP_App *pApp = static_cast<XAP_App *>(g_object_get_data(G_OBJECT(widget), "pApp"));
      XAP_Frame *pFrame = pApp->getLastFocussedFrame();
      if(pFrame ==static_cast<XAP_Frame *>(nullptr)) {
          UT_uint32 nframes =  pApp->getFrameCount();
          if(nframes > 0 && nframes < 10) {
              pFrame = pApp->getFrame(0);
          } else {
              return;
          }
      }
      if(pFrame == static_cast<XAP_Frame *>(nullptr)) return;
      AV_View * pView = pFrame->getCurrentView();
      UT_ASSERT_HARMLESS(pView);
      if(pView!= nullptr)
      {
	     pView->focusChange(AV_FOCUS_NONE);
      }
}


static void focus_in_event_Modeless(GtkEventControllerFocus* /*controller*/, GtkWidget *widget)
{
      XAP_App *pApp=static_cast<XAP_App *>(g_object_get_data(G_OBJECT(widget), "pApp"));
      XAP_Frame *pFrame= pApp->getLastFocussedFrame();
      if(pFrame ==static_cast<XAP_Frame *>(nullptr))
      {
             UT_uint32 nframes =  pApp->getFrameCount();
             if(nframes > 0 && nframes < 10)
	     {     
	            pFrame = pApp->getFrame(0);
	     }
             else
	     {
	            return;
	      }
      }
      if(pFrame == static_cast<XAP_Frame *>(nullptr)) return;
      AV_View * pView = pFrame->getCurrentView();
      if(pView!= nullptr)
      {
            pView->focusChange(AV_FOCUS_MODELESS);
      }
}


static void focus_in_event_ModelessOther(GtkEventControllerFocus* /*controller*/,
                                         GtkWidget *widget)
{
      std::function<gboolean(int)> *other_function =
          static_cast<std::function<gboolean(int)> *>(
              g_object_get_data(G_OBJECT(widget), "other-function"));
      XAP_App *pApp = static_cast<XAP_App *>(g_object_get_data(G_OBJECT(widget), "pApp"));
      XAP_Frame *pFrame = pApp->getLastFocussedFrame();
      if (pFrame == nullptr) {
          UT_uint32 nframes =  pApp->getFrameCount();
          if (nframes > 0 && nframes < 10) {
              pFrame = pApp->getFrame(0);
          } else {
              return;
	      }
      }
      if (pFrame == nullptr) {
          return;
      }
      AV_View * pView = pFrame->getCurrentView();
      if(pView!= nullptr) {
            pView->focusChange(AV_FOCUS_MODELESS);
            if (other_function) {
                (*other_function)(0);
            }
      }
}

static void abi_attach_focus_controller(GtkWidget *widget,
                                        GCallback enter_cb,
                                        GCallback leave_cb)
{
      GtkEventController *foc = gtk_event_controller_focus_new();
      if (enter_cb)
          g_signal_connect(foc, "enter", enter_cb, widget);
      if (leave_cb)
          g_signal_connect(foc, "leave", leave_cb, widget);
      gtk_widget_add_controller(widget, foc);
}

/*****************************************************************/

/*
 * GTK4 builder fixup: a plain <child> on a GtkDialog is applied via
 * gtk_window_set_child(), which REPLACES the dialog's internal vbox
 * (the one holding both the content area and the action area) and
 * leaves the content area detached.  Buttons added later through
 * gtk_dialog_add_button() then land in an orphaned action area and
 * never render.  Reattach the .ui-provided widget inside the content
 * area, put the content area back above the action area, and restore
 * the internal vbox as the window child.
 */
static void abiFixupBuilderDialog(GtkDialog * dlg)
{
	GtkWidget * child = gtk_window_get_child(GTK_WINDOW(dlg));
	if (!child)
		return;

	GtkWidget * content = gtk_dialog_get_content_area(dlg);
	if (!content || gtk_widget_get_parent(content))
		return;	/* internal layout intact, nothing to do */

	/* reach the action area through a temporary probe button; there
	 * is no public action-area getter */
	GtkWidget * probe = gtk_dialog_add_button(dlg, "", G_MININT);
	GtkWidget * action_area = probe ? gtk_widget_get_parent(probe) : nullptr;
	if (!action_area)
		return;
	gtk_box_remove(GTK_BOX(action_area), probe);

	/* rebuild the layout the dialog had before the .ui <child>
	 * replaced it: vbox -> [content_area -> ui child, action_area] */
	GtkWidget * new_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	g_object_ref(child);
	gtk_window_set_child(GTK_WINDOW(dlg), nullptr);
	gtk_box_append(GTK_BOX(content), child);
	g_object_unref(child);

	gtk_box_append(GTK_BOX(new_vbox), content);
	gtk_widget_unparent(action_area);
	gtk_box_append(GTK_BOX(new_vbox), action_area);
	gtk_window_set_child(GTK_WINDOW(dlg), new_vbox);
}

static void abiFixupBuilderDialogs(GtkBuilder * builder)
{
	GSList * objects = gtk_builder_get_objects(builder);
	for (GSList * l = objects; l; l = l->next)
	{
		if (GTK_IS_DIALOG(l->data))
			abiFixupBuilderDialog(GTK_DIALOG(l->data));
	}
}

GtkBuilder * newDialogBuilder(const char * name)
{
    UT_ASSERT(name);
	std::string ui_path = static_cast<XAP_UnixApp*>(XAP_App::getApp())->getAbiSuiteAppUIDir() + "/" + name;

	// load the dialog from the UI file
	GtkBuilder* builder = gtk_builder_new_from_file(ui_path.c_str());
	abiFixupBuilderDialogs(builder);
	return builder;
}

GtkBuilder* newDialogBuilderFromResource(const char* name)
{
    UT_ASSERT(name);
	std::string ui_path = std::string("/io/github/janos_szenfner/Abinova/") + name;

	// load the dialog from the UI file
	GtkBuilder* builder = gtk_builder_new_from_resource(ui_path.c_str());
	abiFixupBuilderDialogs(builder);
	return builder;
}


/*****************************************************************/

void connectFocus(GtkWidget *widget,const XAP_Frame *frame)
{
      g_object_set_data(G_OBJECT(widget), "frame",
					  const_cast<void *>(static_cast<const void *>(frame)));
      abi_attach_focus_controller(widget, G_CALLBACK(focus_in_event),
                                  G_CALLBACK(focus_out_event));
}

void connectFocusModelessOther(GtkWidget *widget,const XAP_App * pApp,
                               std::function<gboolean(int)> *other_function)
{
      g_object_set_data(G_OBJECT(widget), "pApp",
					  const_cast<void *>(static_cast<const void *>(pApp)));
      g_object_set_data(G_OBJECT(widget), "other-function",
					  (gpointer) other_function); // leave as C-style cast
      abi_attach_focus_controller(widget, G_CALLBACK(focus_in_event_ModelessOther),
                                  G_CALLBACK(focus_out_event_Modeless));
}


void connectFocusModeless(GtkWidget *widget,const XAP_App * pApp)
{
      g_object_set_data(G_OBJECT(widget), "pApp",
					  const_cast<void *>(static_cast<const void *>(pApp)));
      abi_attach_focus_controller(widget, G_CALLBACK(focus_in_event_Modeless),
                                  G_CALLBACK(focus_out_event_Modeless));
}


bool isTransientWindow(GtkWindow *window,GtkWindow *parent)
{
  GtkWindow *transient;
  if(window)
	{
	  while((transient=gtk_window_get_transient_for(window)))
		{
		  window=transient;
		  if(window==parent)
			return true;
		}
	}
  return false;
}

/****************************************************************/
/****************************************************************/

static void sDoHelp ( XAP_Dialog * pDlg )
{
	// should always be valid, but just in case...
	if (!pDlg)
		return;

	// open the url in the internal help window
	if ( pDlg->getHelpUrl().size () > 0 )
    {
		std::string page = pDlg->getHelpUrl();
		page += ".html";
		XAP_App::getApp()->openHelpWindow(XAP_App::getApp()->getLastFocussedFrame(),
										 page.c_str(), false);
    }
	else
    {
		// TODO: warn no help on this topic
		UT_DEBUGMSG(("NO HELP FOR THIS TOPIC!!\n"));
    }
}

/*!
 * Catch F1 keypress over a dialog and open up the help file, if any
 */
static gboolean modal_keypress_cb ( GtkEventControllerKey * /*controller*/,
									guint keyval, guint /*keycode*/,
									GdkModifierType /*state*/,
									XAP_Dialog * pDlg )
{
	// propagate keypress up if not F1
	if (keyval == GDK_KEY_F1 || keyval == GDK_KEY_Help)
	{
		sDoHelp( pDlg ) ;

		// stop F1 propegation
		return TRUE ;
	}
	
	return FALSE ;		
}

static void abi_attach_help_key_controller(GtkWidget *widget, XAP_Dialog *pDlg)
{
	GtkEventController *keyc = gtk_event_controller_key_new();
	g_signal_connect(keyc, "key-pressed",
					 G_CALLBACK(modal_keypress_cb), pDlg);
	gtk_widget_add_controller(widget, keyc);
}

static void help_button_cb (GObject * /*button*/, XAP_Dialog * pDlg)
{
    if (pDlg) {
        sDoHelp (pDlg);
    }
}

static void sAddHelpButton (GtkDialog * me, XAP_Dialog * pDlg)
{
  // prevent help button from being added twice
    gint has_button = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (me), "has-help-button"));

    if (has_button)
        return;

    if (pDlg && pDlg->getHelpUrl().size () > 0) {

        std::string s;
        const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
        pSS->getValueUTF8(XAP_STRING_ID_DLG_HelpButton, s);

        GtkWidget * button = gtk_dialog_add_button(me,
                                                   convertMnemonics(s).c_str(),
                                                   GTK_RESPONSE_HELP);
        g_signal_connect (G_OBJECT (button), "clicked",
                          G_CALLBACK(help_button_cb), pDlg);
        g_object_set_data (G_OBJECT (me), "has-help-button", GINT_TO_POINTER (1));
    }
}

/*!
 * Centers a dialog, makes it transient, sets up the right window icon
 */
/* GTK4 dropped gtk_window_move/set_position: toplevel placement is the
 * compositor's job. transient_for hands it the hint (GNOME centres
 * transient dialogs over their parent); under plain X11 - and under
 * XWayland sessions whose compositor ignores the hint - nothing moves
 * the window and it lands at 0,0, so we centre it on the parent
 * ourselves once it is realised. */
#ifdef GDK_WINDOWING_X11
static void s_center_window_x11(GtkWidget * child, GtkWidget * parent)
{
	GdkSurface * cs = gtk_native_get_surface(GTK_NATIVE(child));
	GdkSurface * ps = parent ? gtk_native_get_surface(GTK_NATIVE(parent))
							 : nullptr;
	if (!cs || !ps || !GDK_IS_X11_SURFACE(cs) || !GDK_IS_X11_SURFACE(ps))
		return;
	Window cx = gdk_x11_surface_get_xid(cs);
	Window px = gdk_x11_surface_get_xid(ps);
	Display * dpy = gdk_x11_display_get_xdisplay(gdk_surface_get_display(cs));
	Window root; int px_ = 0, py_ = 0, cx_ = 0, cy_ = 0;
	unsigned int pw = 0, ph = 0, cw_ = 0, ch_ = 0, bw = 0, depth = 0;
	if (!XGetGeometry(dpy, px, &root, &px_, &py_, &pw, &ph, &bw, &depth))
		return;
	if (!XGetGeometry(dpy, cx, &root, &cx_, &cy_, &cw_, &ch_, &bw, &depth) ||
		cw_ == 0)
		return;
	XMoveWindow(dpy, cx,
				px_ + (static_cast<int>(pw) - static_cast<int>(cw_)) / 2,
				py_ + (static_cast<int>(ph) - static_cast<int>(ch_)) / 2);
}

/* at "realize" time the window has no usable size yet, so the first
 * centre happens at "map"; an idle pass repeats it once the size has
 * fully settled (dialogs that grow after mapping) */
struct AbiCenterCtx { GtkWidget *child; GtkWidget *parent; };

static gboolean s_center_idle(gpointer data)
{
	AbiCenterCtx * ctx = static_cast<AbiCenterCtx*>(data);
	s_center_window_x11(ctx->child, ctx->parent);
	g_object_unref(ctx->child);
	g_object_unref(ctx->parent);
	g_free(ctx);
	return G_SOURCE_REMOVE;
}

static void s_center_on_parent_map(GtkWidget * child, gpointer parent_ptr)
{
	s_center_window_x11(child, GTK_WIDGET(parent_ptr));
	AbiCenterCtx * ctx = g_new(AbiCenterCtx, 1);
	ctx->child = GTK_WIDGET(g_object_ref(child));
	ctx->parent = GTK_WIDGET(g_object_ref(parent_ptr));
	g_idle_add(s_center_idle, ctx);
}
#endif

void centerDialog(GtkWidget * parent, GtkWidget * child, bool set_transient_for)
{
	UT_return_if_fail(parent);
	UT_return_if_fail(child);

	if(GTK_IS_WINDOW(parent) != TRUE)
		parent  = gtk_widget_get_parent(parent);
	xxx_UT_DEBUGMSG(("center IS WIDGET WINDOW %d \n",(GTK_IS_WINDOW(parent))));
	xxx_UT_DEBUGMSG(("center child IS WIDGET WINDOW %d \n",(GTK_IS_WINDOW(child))));
	if (set_transient_for)
	  gtk_window_set_transient_for(GTK_WINDOW(child),
				       GTK_WINDOW(parent));
#ifdef GDK_WINDOWING_X11
	g_signal_connect_after(child, "map",
						   G_CALLBACK(s_center_on_parent_map), parent);
#endif
}

void abiSetupModalDialog(GtkDialog * dialog, XAP_Frame *pFrame, XAP_Dialog * pDlg, gint defaultResponse)
{
	GtkWidget *popup = GTK_WIDGET (dialog);
	gtk_dialog_set_default_response (GTK_DIALOG (popup), defaultResponse);
	gtk_window_set_modal (GTK_WINDOW(popup), TRUE);

	// To center the dialog, we need the frame of its parent.
	if (pFrame)
	{
		XAP_UnixFrameImpl * pUnixFrameImpl = static_cast<XAP_UnixFrameImpl *>(pFrame->getFrameImpl());
		GtkWidget * parentWindow = pUnixFrameImpl->getTopLevelWindow();
		if (GTK_IS_WINDOW(parentWindow) != TRUE)
			parentWindow  = gtk_widget_get_parent(parentWindow);
		centerDialog (parentWindow, GTK_WIDGET(popup));
	}
	connectFocus (GTK_WIDGET(popup), pFrame);

	// connect F1 to the help subsystem
	abi_attach_help_key_controller(GTK_WIDGET(popup), pDlg);

	// set the default response
	sAddHelpButton (GTK_DIALOG (popup), pDlg);

	// show the window
	gtk_widget_show (GTK_WIDGET (popup));
}

/*
 * GTK4 removed abiRunModalDialog(GTK_DIALOG(), false). Emulate the modal run loop: show the
 * dialog, spin a nested main loop, return on the first non-HELP response
 * or on window close (reported as GTK_RESPONSE_DELETE_EVENT).
 */
typedef struct {
	GMainLoop *loop;
	gint response;
} AbiDialogRun;

static void abi_dlg_response_cb (GtkDialog * /*dlg*/, gint response, gpointer data)
{
	AbiDialogRun *run = static_cast<AbiDialogRun*>(data);
	run->response = response;
	g_main_loop_quit(run->loop);
}

static gboolean abi_dlg_close_request_cb (GtkWindow * /*w*/, gpointer data)
{
	AbiDialogRun *run = static_cast<AbiDialogRun*>(data);
	run->response = GTK_RESPONSE_DELETE_EVENT;
	g_main_loop_quit(run->loop);
	/* keep the dialog alive; abiRunModalDialog(GTK_DIALOG(), false) didn't destroy on close */
	return TRUE;
}

gint abiRunModalDialog(GtkDialog * me, bool destroyDialog, GtkAccessibleRole role)
{
	/* GTK4's accessible role is immutable once set; setting it again
	 * logs a critical. Only apply ours when nothing set a role yet. */
	if (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (me)) == GTK_ACCESSIBLE_ROLE_NONE) {
		g_object_set (G_OBJECT (me), "accessible-role", role, NULL);
	}

	/* Callers of this overload skip abiSetupModalDialog, so no transient
	 * parent was set; GTK4 warns when a GtkDialog maps without one. */
	if (!gtk_window_get_transient_for (GTK_WINDOW (me))) {
		XAP_Frame *pFrame = XAP_App::getApp()->getLastFocussedFrame();
		if (pFrame) {
			XAP_FrameImpl *pImpl = pFrame->getFrameImpl();
			if (pImpl) {
				GtkWidget *parent = static_cast<XAP_UnixFrameImpl*>(pImpl)->getTopLevelWindow();
				if (GTK_IS_WINDOW (parent)) {
					gtk_window_set_transient_for (GTK_WINDOW (me), GTK_WINDOW (parent));
#ifdef GDK_WINDOWING_X11
					g_signal_connect_after (GTK_WIDGET (me), "map",
											G_CALLBACK (s_center_on_parent_map), parent);
#endif
				}
			}
		}
	}

	GtkWidget *w = GTK_WIDGET (me);
	g_object_add_weak_pointer (G_OBJECT (w), reinterpret_cast<gpointer*>(&w));

	AbiDialogRun run;
	run.loop = g_main_loop_new (nullptr, FALSE);
	run.response = GTK_RESPONSE_NONE;
	g_signal_connect (me, "response", G_CALLBACK(abi_dlg_response_cb), &run);
	g_signal_connect (me, "close-request", G_CALLBACK(abi_dlg_close_request_cb), &run);
	gtk_window_present (GTK_WINDOW (me));

    // now run the dialog
    gint result = GTK_RESPONSE_NONE;
	do {
		run.response = GTK_RESPONSE_NONE;
		g_main_loop_run (run.loop);
		result = run.response;
	} while (result == GTK_RESPONSE_HELP && w != nullptr);

	g_main_loop_unref (run.loop);

    // destroy the dialog (GTK4's ::response handler already destroys it
    // for real responses; w is a weak pointer, nullptr if finalized)
    if ( destroyDialog && w != nullptr ) {
        abiDestroyWidget ( w );
    }
	if (w != nullptr)
		g_object_remove_weak_pointer (G_OBJECT (w), reinterpret_cast<gpointer*>(&w));

    return result ;
}

/*!
 * Runs the dialog \me as a modal dialog
 * 1) Connect focus to toplevel frame
 * 2) Centers dialog over toplevel window
 * 3) Connects F1 to help system
 * 4) Makes dialog modal
 * 5) Sets the default button to defaultResponse, sets ESC to close
 * 6) Returns value of abiRunModalDialog(GTK_DIALOG(me), false)
 * 7) If \destroyDialog is true, destroys the dialog, else you have to call abiDestroyWidget()
 */
gint abiRunModalDialog(GtkDialog * me, XAP_Frame *pFrame, XAP_Dialog * pDlg,
					   gint defaultResponse, bool destroyDialog, GtkAccessibleRole role)
{
  abiSetupModalDialog(me, pFrame, pDlg, defaultResponse);
  gint ret = abiRunModalDialog(me, destroyDialog, role);
  if( pDlg )
  {
      pDlg->maybeReallowPopupPreviewBubbles();
  }
  return ret;
}

/*!
 * Sets up the dialog \me as a modeless dialog
 * 1) Connect focus to toplevel frame
 * 2) Centers dialog over toplevel window
 * 3) Makes the App remember this modeless dialog
 * 4) Connects F1 to help system
 * 5) Makes dialog non-modal (modeless)
 * 
6) Sets the default button to defaultResponse, sets ESC to close
 */
void abiSetupModelessDialog(GtkDialog * me, XAP_Frame * pFrame, XAP_Dialog * pDlg,
							gint defaultResponse, bool abi_modeless, GtkAccessibleRole /*role*/ )
{
	if (abi_modeless)
	{
		// remember the modeless id
		XAP_App::getApp()->rememberModelessId( pDlg->getDialogId(), static_cast<XAP_Dialog_Modeless *>(pDlg));

		// connect focus to our parent frame
		connectFocusModeless(GTK_WIDGET(me), XAP_App::getApp());
	}

	// To center the dialog, we need the frame of its parent.
	if (pFrame)
	{
		XAP_UnixFrameImpl * pUnixFrameImpl = static_cast<XAP_UnixFrameImpl *>(pFrame->getFrameImpl());
		GtkWidget * parentWindow = GTK_WIDGET(gtk_widget_get_root (pUnixFrameImpl->getTopLevelWindow()));
		centerDialog(parentWindow, GTK_WIDGET(me), true);
	}
	
	// connect F1 to the help subsystem
	abi_attach_help_key_controller(GTK_WIDGET(me), pDlg);
	
	// set the default response
	gtk_dialog_set_default_response ( me, defaultResponse ) ;
	sAddHelpButton (me, pDlg);

	// and mark it as modeless
	gtk_window_set_modal ( GTK_WINDOW(me), FALSE ) ;
	if (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (me)) == GTK_ACCESSIBLE_ROLE_NONE) {
		g_object_set (G_OBJECT (me), "accessible-role", GTK_ACCESSIBLE_ROLE_ALERT, NULL);
	}

    pDlg->maybeClosePopupPreviewBubbles();
        
	// show the window
	gtk_widget_show ( GTK_WIDGET(me) ) ;
}

/*!
 * Create a new GtkDialog
 */
GtkWidget * abiDialogNew(const char * role, gboolean resizable)
{
  GtkWidget * dlg = gtk_dialog_new () ;
  // gtk_window_set_role() removed in GTK4; role only affected WM_CLASS hints
  UT_UNUSED(role);
  gtk_window_set_resizable ( GTK_WINDOW(dlg), resizable ) ;
  XAP_gtk_widget_set_margin(dlg, 5);
  gtk_box_set_spacing ( GTK_BOX ( gtk_dialog_get_content_area(GTK_DIALOG (dlg))), 2 ) ;
  return dlg ;
}

/*!
 * Create a new GtkDialog with this title
 */
GtkWidget * abiDialogNew(const char * role, gboolean resizable, const char * title, ...)
{
    GtkWidget * dlg = abiDialogNew(role, resizable);

    if(title && *title)
    {
        std::string titleStr;

        va_list args;
        va_start (args, title);
        titleStr = UT_std_string_vprintf(titleStr, title, args);
        va_end (args);

        // create the title
        gtk_window_set_title(GTK_WINDOW(dlg), titleStr.c_str()) ;
    }

    return dlg ;
}

/*!
 * Set the title of a gtk dialog
 */
void abiDialogSetTitle(GtkWidget * dlg, const char * title, ...)
{
  if ( title != nullptr && strlen ( title ) )
  {
    UT_String titleStr ( "" ) ;

    va_list args;
    va_start (args, title);
    UT_String_vprintf (titleStr, title, args);
    va_end (args);

    // create the title
    gtk_window_set_title ( GTK_WINDOW(dlg), titleStr.c_str() ) ;
  }
}

/*!
 * Add this locale-sensitive button to the dialog and
 * make it sensitive
 */
GtkWidget* abiAddButton(GtkDialog * me, std::string label,
			gint response_id)
{
	UT_return_val_if_fail(me, nullptr);

	// label is UTF-8.
	GtkWidget * wid = gtk_dialog_add_button(me, convertMnemonics(label).c_str(),
                                            response_id);
	gtk_dialog_set_response_sensitive(me, response_id, TRUE);

	return wid ;
}

/*!
 * Calls gtk_widget_destroy on \me if \me is non-null
 * and GTK_IS_WIDGET(me)
 */
void abiDestroyWidget(GtkWidget * me)
{
    if (me) {
        if (GTK_IS_WINDOW(me)) {
            gtk_window_destroy(GTK_WINDOW(me)); // TOPLEVEL
        } else if (GTK_IS_WIDGET(me)) {
            gtk_widget_unparent(me);
        }
    }
}

GtkWidget * abi_radio_button_new_with_label(GtkWidget * group_member, const char * label)
{
    GtkWidget * w = gtk_check_button_new_with_label(label);
    if (group_member) {
        gtk_check_button_set_group(GTK_CHECK_BUTTON(w), GTK_CHECK_BUTTON(group_member));
    }
    return w;
}

/*!
 * Localizes a label given the string id
 */
void localizeLabel(GtkWidget * widget, const XAP_StringSet * pSS, XAP_String_Id id)
{
	gchar * unixstr = nullptr;	// used for conversions
	std::string s;
	pSS->getValueUTF8(id,s);
	UT_XML_cloneNoAmpersands(unixstr, s.c_str());
	gtk_label_set_text (GTK_LABEL(widget), unixstr);
	FREEP(unixstr);	
}

void convertMnemonics(gchar * s)
{
	UT_return_if_fail(s);

	for (UT_uint32 i = 0; s[i] != 0; i++) 
	{
		if ( s[i] == '&' ) {
			if (i > 0 && s[i-1] == '\\')
			{
				s[i-1] = '&';
				strcpy( &s[i], &s[i+1]);
				i--;
				}
			else
				s[i] = '_';
		}
	}
}


// probably much slower....
std::string & convertMnemonics(std::string & s)
{
	for (UT_uint32 i = 0; s[i] != 0; i++) 
	{
		if ( s[i] == '&' ) {
			if (i > 0 && s[i-1] == '\\')
			{
				s[i-1] = '&';
                s.erase(i);
				i--;
            }
			else
				s[i] = '_';
		}
	}

    return s;
}

/*!
 * Localizes the label of a widget given the string id
 * Ampersands will be converted to underscores/mnemonics
 */
void localizeLabelUnderline(GtkWidget * widget, const XAP_StringSet * pSS, XAP_String_Id id)
{
	std::string s;
	pSS->getValueUTF8(id,s);
	gchar * newlbl = g_strdup(s.c_str());
	UT_ASSERT(newlbl);
	convertMnemonics(newlbl);
	gtk_label_set_text_with_mnemonic (GTK_LABEL(widget), newlbl);
	FREEP(newlbl);	
}

/*!
 * Localizes the label of a widget given the string id
 * It formats the label using the current label of the widget as a format
 * string. The current label is assumed to be something like
 * "<span size="larger">%s</span>".
 */
void localizeLabelMarkup(GtkWidget * widget, const XAP_StringSet * pSS, XAP_String_Id id)
{
	gchar * unixstr = nullptr;	// used for conversions
	std::string s;
	pSS->getValueUTF8(id,s);
	UT_XML_cloneNoAmpersands(unixstr, s.c_str());
	std::string markupStr = UT_std_string_sprintf(gtk_label_get_label (GTK_LABEL(widget)), unixstr);
	gtk_label_set_markup (GTK_LABEL(widget), markupStr.c_str());
	FREEP(unixstr);	
}

/* GTK4: GtkCheckButton is no longer a GtkButton. Dispatch label
 * setting so callers can pass either widget kind. */
static void
abi_widget_set_label(GtkWidget * widget, const gchar * label)
{
	if (GTK_IS_CHECK_BUTTON(widget))
		gtk_check_button_set_label(GTK_CHECK_BUTTON(widget), label);
	else
		gtk_button_set_label(GTK_BUTTON(widget), label);
}

static void
abi_widget_set_use_underline(GtkWidget * widget, gboolean use)
{
	if (GTK_IS_CHECK_BUTTON(widget))
		gtk_check_button_set_use_underline(GTK_CHECK_BUTTON(widget), use);
	else
		gtk_button_set_use_underline(GTK_BUTTON(widget), use);
}

/*!
 * Localizes a button given the string id
 */
void localizeButton(GtkWidget * widget, const XAP_StringSet * pSS, XAP_String_Id id)
{
	gchar * unixstr = nullptr;	// used for conversions
	std::string s;
	pSS->getValueUTF8(id,s);
	UT_XML_cloneNoAmpersands(unixstr, s.c_str());
	abi_widget_set_label (widget, unixstr);
	FREEP(unixstr);	
}

/*!
 * Localizes a button given the string id
 * Ampersands will be converted to underscores/mnemonics
 */
void localizeButtonUnderline(GtkWidget * widget, const XAP_StringSet * pSS, XAP_String_Id id)
{
	std::string s;
	pSS->getValueUTF8(id,s);
	gchar * newlbl = g_strdup(s.c_str());
	UT_ASSERT(newlbl);
	convertMnemonics(newlbl);
	abi_widget_set_use_underline (widget, TRUE);
	abi_widget_set_label (widget, newlbl);
	FREEP(newlbl);	
}

/*!
 * Localizes a button given the string id
 * It formats its label using the current button label as a format
 * string. It is assumed to be something like
 * "<span size="larger">%s</span>".
 * Note that in addition to doing markup, ampersands will be converted
 * to underscores/mnemonic since this makes sense for buttons
 */
/* GTK4's GtkCheckButton keeps its label inside a private container
 * (indicator + label), so the first child is not the GtkLabel. */
static GtkWidget * _find_label_child(GtkWidget * widget)
{
	for (GtkWidget * c = gtk_widget_get_first_child(widget); c;
		 c = gtk_widget_get_next_sibling(c))
	{
		if (GTK_IS_LABEL(c))
			return c;
		GtkWidget * r = _find_label_child(c);
		if (r)
			return r;
	}
	return nullptr;
}

void localizeButtonMarkup(GtkWidget * widget, const XAP_StringSet * pSS, XAP_String_Id id)
{
	std::string s;
	pSS->getValueUTF8(id,s);
	gchar * newlbl = g_strdup(s.c_str());
	UT_ASSERT(newlbl);
	convertMnemonics(newlbl);
	const gchar * cur_label = GTK_IS_CHECK_BUTTON(widget)
		? gtk_check_button_get_label(GTK_CHECK_BUTTON(widget))
		: gtk_button_get_label(GTK_BUTTON(widget));
	std::string markupStr = UT_std_string_sprintf(cur_label ? cur_label : "%s", newlbl);
	abi_widget_set_use_underline (widget, TRUE);
	abi_widget_set_label (widget, markupStr.c_str());

	// by default, they don't like markup, so we teach them
	GtkWidget * button_child = GTK_IS_CHECK_BUTTON(widget)
		? _find_label_child(widget)
		: gtk_button_get_child(GTK_BUTTON(widget));
	if (GTK_IS_LABEL (button_child))
		gtk_label_set_use_markup (GTK_LABEL(button_child), TRUE);

	FREEP(newlbl);	
}

/*!
 * Localizes the label of a Menu Item widget given the string id
 */
void localizeMenuItem(GtkWidget * widget, const XAP_StringSet * pSS, XAP_String_Id id)
{
	gchar *unixstr = nullptr;
	std::string s;
	pSS->getValueUTF8(id, s);
	UT_XML_cloneConvAmpersands(unixstr, s.c_str());
	// GTK4 removed GtkMenuItem; set the "label" property on whatever
	// menu-related widget the builder produced (e.g. GtkMenuButton)
	if (g_object_class_find_property(G_OBJECT_GET_CLASS(widget), "label"))
		g_object_set(widget, "label", unixstr, nullptr);
	FREEP(unixstr);
}

/*!
 * Sets the label of "widget" to "str".
 * It formats the label using the current label of the widget as a format string. The
 * current label is assumed to be something like "<span size="larger">%s</span>".
 */
void setLabelMarkup(GtkWidget * widget, const gchar * str)
{
	std::string markupStr = UT_std_string_sprintf(gtk_label_get_label (GTK_LABEL(widget)), str);
	gtk_label_set_markup (GTK_LABEL(widget), markupStr.c_str());
}

/*!
 * This is a small message box for startup warnings and/or
 * errors.  Please do NOT use this for normal system execution
 * user messages; use the XAP_UnixDialog_MessageBox class for that.
 * We can't use that here because there is no parent frame, etc.
 */
void messageBoxOK(const char * message)
{
	GtkWidget * msg = gtk_message_dialog_new ( nullptr,
						   GTK_DIALOG_MODAL,
						   GTK_MESSAGE_INFO,
						   GTK_BUTTONS_OK,
						   "%s", message ) ;

	gtk_window_set_title(GTK_WINDOW(msg), "Abinova");

	gtk_widget_show ( msg ) ;
	abiRunModalDialog(GTK_DIALOG(msg), true);
}

/****************************************************************/
/****************************************************************/

static void activate_button( GtkEntry * /*entry*/, gpointer user_data )
{
//    UT_DEBUGMSG(("activate_button() ud:%x\n", user_data ));

    GtkWidget* w = GTK_WIDGET(user_data);
    gtk_widget_activate(w);
}

/**
 * When the source widget gets the activate signal, sent activate to the button.
 * This allows GtkEntry widgets to explicitly close the dialog with the OK button
 * when the user presses return while leaving the default dialog action to be CANCEL.
 */
void abiSetActivateOnWidgetToActivateButton( GtkWidget* source, GtkWidget* button )
{
    g_signal_connect( G_OBJECT( source ), "activate",
                      G_CALLBACK(activate_button), button );
}

