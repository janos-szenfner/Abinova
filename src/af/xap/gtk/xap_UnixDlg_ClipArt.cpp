/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* Abinova
 * Copyright (C) 2006 Rob Staudinger <robert.staudinger@gmail.com>
 * Copyright (c) 2020 Hubert Figuière
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

#include <stdlib.h>
#include <time.h>

#include "ut_string.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"

#include "xap_UnixDialogHelper.h"

#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"

#include "xap_Strings.h"
#include "xap_Dialog_Id.h"
#include "xap_Dlg_ClipArt.h"
#include "xap_UnixDlg_ClipArt.h"

/* One row object per clipart file for the GtkGridView model. */
#define ABI_TYPE_CLIPART_ITEM (abi_clipart_item_get_type())
G_DECLARE_FINAL_TYPE (AbiClipartItem, abi_clipart_item,
					  ABI, CLIPART_ITEM, GObject)

struct _AbiClipartItem
{
	GObject parent_instance;
	gchar *path;
	gchar *name;
	GdkTexture *texture;
};

G_DEFINE_TYPE (AbiClipartItem, abi_clipart_item, G_TYPE_OBJECT)

static void
abi_clipart_item_init (AbiClipartItem * /*self*/)
{
}

static void
abi_clipart_item_finalize (GObject *object)
{
	AbiClipartItem *row = ABI_CLIPART_ITEM (object);
	g_free (row->path);
	g_free (row->name);
	g_clear_object (&row->texture);
	G_OBJECT_CLASS (abi_clipart_item_parent_class)->finalize (object);
}

static void
abi_clipart_item_class_init (AbiClipartItemClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = abi_clipart_item_finalize;
}

static AbiClipartItem *
abi_clipart_item_new (const gchar *path, const gchar *name,
					  GdkPixbuf *pixbuf)
{
	AbiClipartItem *row =
		ABI_CLIPART_ITEM (g_object_new (ABI_TYPE_CLIPART_ITEM, nullptr));
	row->path = g_strdup (path);
	row->name = g_strdup (name);
	/* GdkPixbuf is not a GdkPaintable in GTK4 — go through a texture. */
	row->texture = pixbuf ? gdk_texture_new_for_pixbuf (pixbuf) : nullptr;
	return row;
}

static void
s_clipart_setup (GtkSignalListItemFactory * /*factory*/,
				 GtkListItem *item,
				 gpointer /*data*/)
{
	GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	XAP_gtk_widget_set_margin (box, 6);

	GtkWidget *image = gtk_image_new ();
	gtk_image_set_pixel_size (GTK_IMAGE (image), 48);
	gtk_box_append (GTK_BOX (box), image);

	GtkWidget *label = gtk_label_new (nullptr);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (GTK_LABEL (label), 16);
	gtk_label_set_lines (GTK_LABEL (label), 2);
	gtk_box_append (GTK_BOX (box), label);

	gtk_list_item_set_child (item, box);
}

static void
s_clipart_bind (GtkSignalListItemFactory * /*factory*/,
				GtkListItem *item,
				gpointer /*data*/)
{
	GtkWidget *box = gtk_list_item_get_child (item);
	GtkWidget *image = gtk_widget_get_first_child (box);
	GtkWidget *label = gtk_widget_get_next_sibling (image);
	AbiClipartItem *row =
		ABI_CLIPART_ITEM (gtk_list_item_get_item (item));

	gtk_image_set_from_paintable (GTK_IMAGE (image),
		row && row->texture ? GDK_PAINTABLE (row->texture) : nullptr);
	gtk_label_set_text (GTK_LABEL (label),
						row && row->name ? row->name : "");
}

static gint clipartCount = 0;

/**
 * Create list store for the icon view.
 */
static GListStore *
create_store ()
{
	return g_list_store_new (ABI_TYPE_CLIPART_ITEM);
}

/**
 * Fill list store.
 */
static gboolean
fill_store (XAP_UnixDialog_ClipArt *self)
{
	self->clearFillIdleId();
	gboolean ret = self->fillStore();
	if (!ret) {
		GtkWidget *dlg = self->getDialog ();
		const XAP_StringSet *pSS = XAP_App::getApp()->getStringSet ();
		std::string s;
		pSS->getValueUTF8(XAP_STRING_ID_DLG_CLIPART_Error, s);

		GtkWidget *err = gtk_message_dialog_new (GTK_WINDOW (dlg), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", s.c_str());
		abiRunModalDialog (GTK_DIALOG (err), true); // TOPLEVEL
		err = nullptr;

		gtk_dialog_response(GTK_DIALOG(dlg), GTK_RESPONSE_CANCEL);
	}
	return FALSE;
}

/**
 * Clipart clicked handler.
 */
static void
item_activated (GtkGridView 			* /*grid_view*/,
				guint					 /*position*/,
				XAP_UnixDialog_ClipArt 	*self)
{
	self->onItemActivated();
}

/**
 *
 */
XAP_Dialog * XAP_UnixDialog_ClipArt::static_constructor(XAP_DialogFactory * pFactory, XAP_Dialog_Id id)
{
	XAP_UnixDialog_ClipArt * p = new XAP_UnixDialog_ClipArt(pFactory,id);
	return p;
}

/**
 *
 */
XAP_UnixDialog_ClipArt::XAP_UnixDialog_ClipArt(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id id)
  : XAP_Dialog_ClipArt(pDlgFactory, id)
  , fill_idle_id(0)
{}

/**
 *
 */
XAP_UnixDialog_ClipArt::~XAP_UnixDialog_ClipArt()
{
	if (this->fill_idle_id) {
		g_source_remove (this->fill_idle_id);
	}
	this->dir_path = nullptr;
	this->progress = nullptr;
	this->grid_view = nullptr;
	this->store = nullptr;
}

/**
 *
 */
void XAP_UnixDialog_ClipArt::runModal(XAP_Frame * pFrame)
{
	GtkWidget	*scroll;
	GError		*error;

	std::string s;
	const XAP_StringSet *pSS = m_pApp->getStringSet ();

	UT_ASSERT(pFrame);

	this->dlg = abiDialogNew ("clipart dialog", TRUE, pSS->getValue (XAP_STRING_ID_DLG_CLIPART_Title));
	gtk_window_set_default_size (GTK_WINDOW (this->dlg), 640, 480);
	abiAddButton(GTK_DIALOG(this->dlg),
                 pSS->getValue(XAP_STRING_ID_DLG_Cancel),
                 GTK_RESPONSE_CANCEL);
	abiAddButton(GTK_DIALOG(this->dlg),
                 pSS->getValue(XAP_STRING_ID_DLG_OK), GTK_RESPONSE_OK);
	connectFocus(GTK_WIDGET(this->dlg), pFrame);

	GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_vexpand(vbox, TRUE);
	gtk_box_append(GTK_BOX (gtk_dialog_get_content_area(GTK_DIALOG(this->dlg))), vbox);

	pSS->getValueUTF8(XAP_STRING_ID_DLG_CLIPART_Loading, s);
	this->progress = gtk_progress_bar_new ();
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (this->progress), s.c_str());
	gtk_box_append (GTK_BOX (vbox), this->progress);

	scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (scroll), TRUE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
									GTK_POLICY_AUTOMATIC,
									GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scroll, TRUE);
	gtk_box_append (GTK_BOX (vbox), scroll);

	this->store = create_store ();

	GtkSingleSelection *sel = gtk_single_selection_new (nullptr);
	gtk_single_selection_set_autoselect (sel, FALSE);
	gtk_single_selection_set_can_unselect (sel, TRUE);
	gtk_single_selection_set_model (sel, G_LIST_MODEL (this->store));

	GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
	g_signal_connect (factory, "setup", G_CALLBACK (s_clipart_setup), nullptr);
	g_signal_connect (factory, "bind", G_CALLBACK (s_clipart_bind), nullptr);

	/* Build an empty view and attach model/factory via the setters:
	 * passing a late-bound GtkSingleSelection into gtk_grid_view_new()
	 * corrupts the view's factory property on GTK 4.14. */
	this->grid_view = gtk_grid_view_new (nullptr, nullptr);
	gtk_grid_view_set_model (GTK_GRID_VIEW (this->grid_view),
							 GTK_SELECTION_MODEL (sel));
	gtk_grid_view_set_factory (GTK_GRID_VIEW (this->grid_view), factory);
	gtk_grid_view_set_single_click_activate (GTK_GRID_VIEW (this->grid_view), FALSE);
	g_object_unref (sel);
	g_object_unref (factory);
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), this->grid_view);
	g_signal_connect (this->grid_view, "activate", G_CALLBACK (item_activated), static_cast<gpointer>( this));
	g_object_unref (G_OBJECT (this->store));

	gtk_widget_set_visible(this->dlg, TRUE);

	/* Dom says we just use that dir for now and hope for someone to build an openclipart client */
	this->dir_path = getInitialDir ();

	if (!g_file_test (this->dir_path, G_FILE_TEST_IS_DIR)) {
		// Running uninstalled: try the clipart dir from the source
		// tree, relative to the executable (src/abiword ->
		// ../user/wp/clipart).
		gchar * exe = g_file_read_link ("/proc/self/exe", nullptr);
		if (exe) {
			gchar * exe_dir = g_path_get_dirname (exe);
			gchar * clip = g_build_filename (exe_dir, "..", "user", "wp", "clipart", nullptr);
			if (g_file_test (clip, G_FILE_TEST_IS_DIR)) {
				setInitialDir (clip);
				this->dir_path = getInitialDir ();
			}
			g_free (clip);
			g_free (exe_dir);
			g_free (exe);
		}
	}
	fill_idle_id = g_idle_add (reinterpret_cast<GSourceFunc>( fill_store), this);

	switch (abiRunModalDialog(GTK_DIALOG(this->dlg), pFrame, this, GTK_RESPONSE_CANCEL, false)) {
	case GTK_RESPONSE_OK:
	{
		GtkSelectionModel *model =
			gtk_grid_view_get_model (GTK_GRID_VIEW (this->grid_view));
		guint pos = gtk_single_selection_get_selected (
			GTK_SINGLE_SELECTION (model));
		AbiClipartItem *item = nullptr;
		if (pos != GTK_INVALID_LIST_POSITION) {
			item = ABI_CLIPART_ITEM (g_list_model_get_item (
				gtk_single_selection_get_model (
					GTK_SINGLE_SELECTION (model)), pos));
		}
		if (item && item->path) {
			error = nullptr;
			gchar *graphicUri = g_filename_to_uri (item->path, nullptr, &error);
			setGraphicName (graphicUri);
			g_free (graphicUri);
			setAnswer (XAP_Dialog_ClipArt::a_OK);
		}
		else {
			setAnswer (XAP_Dialog_ClipArt::a_CANCEL);
		}
		g_clear_object (&item);
		break;
	}
	default:
		break;
	}

	abiDestroyWidget(this->dlg);
}

/**
 * Fill list store updating progress bar as we go.
 */
gboolean XAP_UnixDialog_ClipArt::fillStore()
{
	GDir 		*dir;
	const gchar *name;
	GdkPixbuf	*pixbuf;
	GError		*error;
	gint		 _count;

	if (!g_file_test (this->dir_path, G_FILE_TEST_IS_DIR)) {
		return FALSE;
	}

	error = nullptr;
	dir = g_dir_open (this->dir_path, 0, &error);
	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		return FALSE;
	}

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (this->progress), 0.);
	_count = 0;
	while ((name = g_dir_read_name (dir)) != nullptr) {

		gchar *file_path, *display_name;

		/* We ignore hidden files that start with a '.' */
		if (name[0] == '.') {
			continue;
		}

		file_path = g_build_filename (this->dir_path, name, nullptr);
		if (g_file_test (file_path, G_FILE_TEST_IS_DIR)) {
			g_free (file_path);
			continue;
		}

		display_name = g_filename_to_utf8 (name, -1, nullptr, nullptr, nullptr);
		error = nullptr;
		pixbuf = gdk_pixbuf_new_from_file_at_size (file_path, 48, 48, &error);
		if (error) {
			g_warning ("%s", error->message);
			g_error_free (error);
			g_free (file_path);
			g_free (display_name);
			continue;
		}

		{
			AbiClipartItem *item =
				abi_clipart_item_new (file_path, display_name, pixbuf);
			g_list_store_append (this->store, item);
			g_object_unref (item);
		}
		g_free(file_path);
		file_path = nullptr;
		g_free(display_name);
		display_name = nullptr;
		g_object_unref(G_OBJECT (pixbuf));
		pixbuf = nullptr;

		if (clipartCount) {
			gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (this->progress),
										   _count / clipartCount * 100.);
		}
		else {
			gtk_progress_bar_pulse (GTK_PROGRESS_BAR (this->progress));
		}
		_count++;
		if (_count % 10 == 0) {
			g_main_context_iteration(nullptr, false);
		}
	}
	g_dir_close (dir);
	clipartCount = _count;

	gtk_widget_set_visible(this->progress, FALSE);

	return TRUE;
}

/**
 * Clipart clicked handler.
 */
void XAP_UnixDialog_ClipArt::onItemActivated()
{
	gtk_dialog_response(GTK_DIALOG(this->dlg), GTK_RESPONSE_OK);
}
