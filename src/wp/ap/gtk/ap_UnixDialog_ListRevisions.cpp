/* Abinova
 * Copyright (C) 2002 Dom Lachowicz <cinamod@hotmail.com>
 * Copyright (c) 2009-2023 Hubert Figuière
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

// This header defines some functions for Unix dialogs,
// like centering them, measuring them, etc.
#include "xap_UnixDialogHelper.h"

#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"
#include "ap_Dialog_ListRevisions.h"
#include "ap_UnixDialog_ListRevisions.h"


/* One row object per document revision for the GtkColumnView model. */
#define ABI_TYPE_REV_ROW (abi_rev_row_get_type())
G_DECLARE_FINAL_TYPE (AbiRevRow, abi_rev_row, ABI, REV_ROW, GObject)

struct _AbiRevRow
{
	GObject parent_instance;
	guint revid;
	gchar *comment;
	gchar *author;
	gchar *date;
	gint64 timet;
};

G_DEFINE_TYPE (AbiRevRow, abi_rev_row, G_TYPE_OBJECT)

static void
abi_rev_row_init (AbiRevRow * /*self*/)
{
}

static void
abi_rev_row_finalize (GObject *object)
{
	AbiRevRow *row = ABI_REV_ROW (object);
	g_free (row->comment);
	g_free (row->author);
	g_free (row->date);
	G_OBJECT_CLASS (abi_rev_row_parent_class)->finalize (object);
}

static void
abi_rev_row_class_init (AbiRevRowClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = abi_rev_row_finalize;
}

static AbiRevRow *
abi_rev_row_new (guint revid, const gchar *comment, const gchar *author,
				 const gchar *date, gint64 timet)
{
	AbiRevRow *row =
		ABI_REV_ROW (g_object_new (ABI_TYPE_REV_ROW, nullptr));
	row->revid = revid;
	row->comment = g_strdup (comment);
	row->author = g_strdup (author);
	row->date = g_strdup (date);
	row->timet = timet;
	return row;
}

static void
s_rev_setup (GtkSignalListItemFactory * /*factory*/,
			 GtkListItem *item,
			 gpointer /*data*/)
{
	GtkWidget *label = gtk_label_new (nullptr);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_list_item_set_child (item, label);
}

static void
s_rev_bind_comment (GtkSignalListItemFactory * /*factory*/,
					GtkListItem *item,
					gpointer /*data*/)
{
	AbiRevRow *row = ABI_REV_ROW (gtk_list_item_get_item (item));
	gtk_label_set_text (GTK_LABEL (gtk_list_item_get_child (item)),
						row->comment ? row->comment : "");
}

static void
s_rev_bind_author (GtkSignalListItemFactory * /*factory*/,
				   GtkListItem *item,
				   gpointer /*data*/)
{
	AbiRevRow *row = ABI_REV_ROW (gtk_list_item_get_item (item));
	gtk_label_set_text (GTK_LABEL (gtk_list_item_get_child (item)),
						row->author ? row->author : "");
}

static void
s_rev_bind_date (GtkSignalListItemFactory * /*factory*/,
				 GtkListItem *item,
				 gpointer /*data*/)
{
	AbiRevRow *row = ABI_REV_ROW (gtk_list_item_get_item (item));
	gtk_label_set_text (GTK_LABEL (gtk_list_item_get_child (item)),
						row->date ? row->date : "");
}

static void
s_rev_bind_revid (GtkSignalListItemFactory * /*factory*/,
				  GtkListItem *item,
				  gpointer /*data*/)
{
	AbiRevRow *row = ABI_REV_ROW (gtk_list_item_get_item (item));
	gchar *buf = g_strdup_printf ("%u", row->revid);
	gtk_label_set_text (GTK_LABEL (gtk_list_item_get_child (item)), buf);
	g_free (buf);
}

static GtkListItemFactory *
s_rev_factory (void (*bind) (GtkSignalListItemFactory *,
							 GtkListItem *, gpointer))
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
	g_signal_connect (factory, "setup", G_CALLBACK (s_rev_setup), nullptr);
	g_signal_connect (factory, "bind", G_CALLBACK (bind), nullptr);
	return factory;
}

static gint
s_sort_rev_comment (gconstpointer p1, gconstpointer p2, gpointer /*data*/)
{
	const AbiRevRow *a = static_cast<const AbiRevRow *>(p1);
	const AbiRevRow *b = static_cast<const AbiRevRow *>(p2);
	return g_utf8_collate (a->comment ? a->comment : "",
						   b->comment ? b->comment : "");
}

static gint
s_sort_rev_author (gconstpointer p1, gconstpointer p2, gpointer /*data*/)
{
	const AbiRevRow *a = static_cast<const AbiRevRow *>(p1);
	const AbiRevRow *b = static_cast<const AbiRevRow *>(p2);
	return g_utf8_collate (a->author ? a->author : "",
						   b->author ? b->author : "");
}

static gint
s_sort_rev_timet (gconstpointer p1, gconstpointer p2, gpointer /*data*/)
{
	const AbiRevRow *a = static_cast<const AbiRevRow *>(p1);
	const AbiRevRow *b = static_cast<const AbiRevRow *>(p2);
	return (a->timet > b->timet) - (a->timet < b->timet);
}

static gint
s_sort_rev_id (gconstpointer p1, gconstpointer p2, gpointer /*data*/)
{
	const AbiRevRow *a = static_cast<const AbiRevRow *>(p1);
	const AbiRevRow *b = static_cast<const AbiRevRow *>(p2);
	return (a->revid > b->revid) - (a->revid < b->revid);
}

void
AP_UnixDialog_ListRevisions::select_row_cb(GtkSingleSelection * select,
										   GParamSpec * /*pspec*/,
										   AP_UnixDialog_ListRevisions * me )
{
	gpointer item = gtk_single_selection_get_selected_item (select);
	if (item)
    {
        me->select_Row (ABI_REV_ROW (item)->revid);
    }
	else
	{
		me->unselect_Row ();
	}
}


void
AP_UnixDialog_ListRevisions::row_activated_cb(GtkColumnView *,
											  guint /*pos*/,
											  AP_UnixDialog_ListRevisions * me)
{
	UT_DEBUGMSG(("row_activated\n"));
	gtk_dialog_response(GTK_DIALOG(me->m_mainWindow), BUTTON_OK);
}


/*****************************************************************/

XAP_Dialog * AP_UnixDialog_ListRevisions::static_constructor(XAP_DialogFactory * pFactory,
													 XAP_Dialog_Id id)
{
	AP_UnixDialog_ListRevisions * p = new AP_UnixDialog_ListRevisions(pFactory,id);
	return p;
}

AP_UnixDialog_ListRevisions::AP_UnixDialog_ListRevisions(XAP_DialogFactory * pDlgFactory,
							 XAP_Dialog_Id id)
  : AP_Dialog_ListRevisions(pDlgFactory,id)
  , m_mainWindow(nullptr)
  , m_store(nullptr)
{
}

AP_UnixDialog_ListRevisions::~AP_UnixDialog_ListRevisions(void)
{
  if (m_store)
    g_object_unref (G_OBJECT (m_store));
}

void AP_UnixDialog_ListRevisions::runModal(XAP_Frame * pFrame)
{
	m_mainWindow = constructWindow();
	UT_return_if_fail(m_mainWindow);

	switch ( abiRunModalDialog ( GTK_DIALOG(m_mainWindow),
								 pFrame, this, BUTTON_OK, false ) )
	{
		case BUTTON_OK:
			event_OK () ; break ;
		default:
			event_Cancel () ; break ;
	}

	abiDestroyWidget ( m_mainWindow ) ;
}

void AP_UnixDialog_ListRevisions::event_Cancel ()
{
  m_iId = 0 ;
  m_answer = AP_Dialog_ListRevisions::a_CANCEL ;
}

void AP_UnixDialog_ListRevisions::event_OK ()
{
  m_answer = AP_Dialog_ListRevisions::a_OK ;
}

void AP_UnixDialog_ListRevisions::select_Row (guint id)
{
    m_iId = id;
    UT_DEBUGMSG(("DOM: select row: %d\n", m_iId));
}

void AP_UnixDialog_ListRevisions::unselect_Row()
{
  UT_DEBUGMSG(("DOM: unselect row: %d 0\n", m_iId));
  m_iId = 0 ;
}

GtkWidget * AP_UnixDialog_ListRevisions::constructWindow ()
{
  GtkWidget *ap_UnixDialog_ListRevisions;
  GtkWidget *vbDialog;

  const XAP_StringSet *pSS = XAP_App::getApp()->getStringSet();
  ap_UnixDialog_ListRevisions = abiDialogNew ( "list revisions dialog", TRUE, getTitle());

  gtk_window_set_modal (GTK_WINDOW (ap_UnixDialog_ListRevisions), TRUE);
  gtk_window_set_default_size ( GTK_WINDOW(ap_UnixDialog_ListRevisions), 800, 450 ) ;

  vbDialog = gtk_dialog_get_content_area(GTK_DIALOG(ap_UnixDialog_ListRevisions));
  gtk_widget_show (vbDialog);
  XAP_gtk_widget_set_margin(vbDialog, 5);

  constructWindowContents ( vbDialog ) ;

  abiAddButton(GTK_DIALOG(ap_UnixDialog_ListRevisions),
               pSS->getValue(XAP_STRING_ID_DLG_Cancel), BUTTON_CANCEL);
  abiAddButton(GTK_DIALOG(ap_UnixDialog_ListRevisions),
               pSS->getValue(XAP_STRING_ID_DLG_OK), BUTTON_OK);

  return ap_UnixDialog_ListRevisions;
}

void AP_UnixDialog_ListRevisions::constructWindowContents ( GtkWidget * vbDialog )
{
  GtkWidget *vbContent;
  GtkWidget *lbExistingRevisions;
  GtkWidget *swExistingRevisions;
  GtkWidget *clExistingRevisions;

  vbContent = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_show (vbContent);
  xap_gtk_container_add (vbDialog, vbContent);
  XAP_gtk_widget_set_margin(vbContent, 5);

  std::string s("<b>");
  s += getLabel1();
  s += "</b>";
  lbExistingRevisions = gtk_label_new (s.c_str());
  g_object_set(G_OBJECT(lbExistingRevisions),
                                        "use-markup", TRUE,
                                        "xalign", 0.0, "yalign", 0.5,
                                        nullptr);
  gtk_widget_show (lbExistingRevisions);
  gtk_box_append(GTK_BOX(vbContent), lbExistingRevisions);

  swExistingRevisions = gtk_scrolled_window_new();
  gtk_widget_show (swExistingRevisions);
  xap_gtk_container_add (vbContent, swExistingRevisions);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swExistingRevisions), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  m_store = g_list_store_new(ABI_TYPE_REV_ROW);

  GtkSortListModel *sortModel =
	  gtk_sort_list_model_new(G_LIST_MODEL(g_object_ref(m_store)), nullptr);
  GtkSingleSelection *selection =
	  gtk_single_selection_new(G_LIST_MODEL(sortModel));
  gtk_single_selection_set_autoselect(selection, FALSE);
  gtk_single_selection_set_can_unselect(selection, TRUE);

  clExistingRevisions = gtk_column_view_new (GTK_SELECTION_MODEL(selection));
  gtk_widget_set_visible(clExistingRevisions, TRUE);
  xap_gtk_container_add (swExistingRevisions, clExistingRevisions);

  GtkColumnView *view = GTK_COLUMN_VIEW(clExistingRevisions);
  GtkColumnViewColumn *col;
  GtkColumnViewColumn *dateCol;
  GtkSorter *sorter;

  // comment column
  col = gtk_column_view_column_new(getColumn3Label(),
								   s_rev_factory(s_rev_bind_comment));
  sorter = GTK_SORTER(gtk_custom_sorter_new(s_sort_rev_comment,
											nullptr, nullptr));
  gtk_column_view_column_set_sorter(col, sorter);
  g_object_unref(sorter);
  gtk_column_view_column_set_expand(col, TRUE);
  gtk_column_view_append_column(view, col);
  g_object_unref(col);

  // author column
  col = gtk_column_view_column_new(getColumn4Label(),
								   s_rev_factory(s_rev_bind_author));
  sorter = GTK_SORTER(gtk_custom_sorter_new(s_sort_rev_author,
											nullptr, nullptr));
  gtk_column_view_column_set_sorter(col, sorter);
  g_object_unref(sorter);
  gtk_column_view_column_set_fixed_width(col, 140);
  gtk_column_view_append_column(view, col);
  g_object_unref(col);

  // revision date column
  col = gtk_column_view_column_new(getColumn2Label(),
								   s_rev_factory(s_rev_bind_date));
  // we sort on the numerical timet instead of the human readable text
  sorter = GTK_SORTER(gtk_custom_sorter_new(s_sort_rev_timet,
											nullptr, nullptr));
  gtk_column_view_column_set_sorter(col, sorter);
  g_object_unref(sorter);
  gtk_column_view_column_set_fixed_width(col, 120);
  gtk_column_view_append_column(view, col);
  dateCol = col;
  g_object_unref(col);

  // revision # column
  col = gtk_column_view_column_new(getColumn1Label(),
								   s_rev_factory(s_rev_bind_revid));
  sorter = GTK_SORTER(gtk_custom_sorter_new(s_sort_rev_id,
											nullptr, nullptr));
  gtk_column_view_column_set_sorter(col, sorter);
  g_object_unref(sorter);
  gtk_column_view_column_set_fixed_width(col, 80);
  gtk_column_view_append_column(view, col);
  g_object_unref(col);

  // clicking a column header sorts on that column's sorter
  gtk_sort_list_model_set_sorter(sortModel,
								 gtk_column_view_get_sorter(view));
  // initial sort is date desc.
  gtk_column_view_sort_by_column(view, dateCol, GTK_SORT_DESCENDING);

  UT_uint32 itemCnt = getItemCount () ;

  UT_DEBUGMSG(("DOM: %d items\n", itemCnt));

  for ( UT_uint32 i = 0; i < itemCnt; i++ )
  {
    char buf [ 35 ] ;

    g_snprintf(buf, 35, "%d", getNthItemId(i));

    gchar * txt = getNthItemText(i, true);
    gchar * itemtime = g_locale_to_utf8(getNthItemTime(i), -1, nullptr, nullptr, nullptr);
    AbiRevRow *row = abi_rev_row_new(getNthItemId(i),
									 txt ? txt : "",
									 getNthItemAuthor(i),
									 itemtime ? itemtime : "",
									 getNthItemTimeT(i));
    g_list_store_append(m_store, row);
    g_object_unref(row);
    UT_DEBUGMSG(("appending revision %s : %s, %s\n", itemtime, buf, txt));

    g_free(itemtime);

    FREEP(txt);
  }

  g_signal_connect (G_OBJECT(selection), "notify::selected-item",
					G_CALLBACK(select_row_cb), this);

  g_signal_connect(G_OBJECT(clExistingRevisions),
		   "activate",
		   G_CALLBACK(row_activated_cb),
		   static_cast<gpointer>(this));
}
