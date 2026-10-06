/* AbiSource Application Framework
 * Copyright (C) 2005 Hubert Figuiere
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

#define TFSUITE "core.af.xap.unixwidget"

#include <stdlib.h>
#include <string.h>

#include "tf_test.h"
#include "ut_string_class.h"
#include "xap_UnixWidget.h"


/* These need GTK initialized.  The test binary deliberately does NOT
 * gtk_init at startup (XAP_UnixClipboard's ctor snapshots the display
 * during app->initialize: GTK4 hands back a display object that never
 * opened when no server exists, which would silently flip the
 * clipboard off its deterministic fake path).  These mains are
 * registered last in the suite, so initializing here is safe; where
 * even that fails we just bail. */
static bool s_gtk_ready()
{
	static int ready = -1;
	if (ready < 0)
		ready = gtk_init_check() ? 1 : 0;
	return ready != 0;
}

TFTEST_MAIN("toggle_button")
{
	if (!s_gtk_ready())
		return;

	GtkWidget *gtkw = gtk_toggle_button_new();
	XAP_Widget *w = new XAP_UnixWidget(gtkw);

	g_object_ref_sink(G_OBJECT(gtkw));

	w->setState(true);
	TFPASS(w->getState());
	w->setState(false);
	TFPASS(w->getState() == false);
	
	w->setValueInt(1);
	TFPASS(w->getValueInt() == 1);
	w->setValueInt(0);
	TFPASS(w->getValueInt() == 0);

	delete w;
	g_object_unref(G_OBJECT(gtkw));
}

TFTEST_MAIN("visibility")
{
	if (!s_gtk_ready())
		return;

	GtkWidget *gtkw = gtk_label_new("");
	XAP_UnixWidget *w = new XAP_UnixWidget(gtkw);

	g_object_ref_sink(G_OBJECT(gtkw));

	/* GTK4 widgets default to visible */
	TFPASS(w->getVisible());
	w->setVisible(false);
	TFPASS(w->getVisible() == false);
	w->setVisible(true);
	TFPASS(w->getVisible());

	delete w;
	g_object_unref(G_OBJECT(gtkw));
}

TFTEST_MAIN("entry")
{
	if (!s_gtk_ready())
		return;

	GtkWidget *gtkw = gtk_entry_new();
	XAP_UnixWidget *w = new XAP_UnixWidget(gtkw);

	g_object_ref_sink(G_OBJECT(gtkw));

	w->setValueInt(42);
	TFPASS(w->getValueInt() == 42);

	UT_UTF8String s;
	w->setValueString(UT_UTF8String("hello"));
	w->getValueString(s);
	TFPASS(s == "hello");

	w->setValueFloat(1.5f);
	w->getValueString(s);
	TFPASS(strtod(s.utf8_str(), nullptr) == 1.5);

	delete w;
	g_object_unref(G_OBJECT(gtkw));
}

TFTEST_MAIN("label")
{
	if (!s_gtk_ready())
		return;

	GtkWidget *gtkw = gtk_label_new("");
	XAP_UnixWidget *w = new XAP_UnixWidget(gtkw);

	g_object_ref_sink(G_OBJECT(gtkw));

	w->setValueInt(7);
	UT_UTF8String s;
	w->getValueString(s);
	TFPASS(s == "7");

	w->setLabel(UT_UTF8String("a caption"));
	TFPASSEQ(strcmp(gtk_label_get_text(GTK_LABEL(gtkw)), "a caption"), 0);

	/* markup path: setData supplies the printf-style markup template
	 * that setLabel substitutes the value into */
	w->setData("<b>%s</b>");
	gtk_label_set_use_markup(GTK_LABEL(gtkw), TRUE);
	w->setLabel(UT_UTF8String("marked"));
	TFPASSEQ(strcmp(gtk_label_get_label(GTK_LABEL(gtkw)), "<b>marked</b>"), 0);

	delete w;
	g_object_unref(G_OBJECT(gtkw));
}

TFTEST_MAIN("button_label")
{
	if (!s_gtk_ready())
		return;

	GtkWidget *gtkw = gtk_button_new();
	XAP_UnixWidget *w = new XAP_UnixWidget(gtkw);

	g_object_ref_sink(G_OBJECT(gtkw));

	w->setLabel(UT_UTF8String("Press me"));
	TFPASSEQ(strcmp(gtk_button_get_label(GTK_BUTTON(gtkw)), "Press me"), 0);

	delete w;
	g_object_unref(G_OBJECT(gtkw));
}

TFTEST_MAIN("window_title")
{
	if (!s_gtk_ready())
		return;

	GtkWidget *gtkw = gtk_window_new();
	XAP_UnixWidget *w = new XAP_UnixWidget(gtkw);

	g_object_ref_sink(G_OBJECT(gtkw));

	w->setLabel(UT_UTF8String("a title"));
	TFPASSEQ(strcmp(gtk_window_get_title(GTK_WINDOW(gtkw)), "a title"), 0);

	delete w;
	/* windows own themselves once realized; destroy, not unref */
	gtk_window_destroy(GTK_WINDOW(gtkw));
}
