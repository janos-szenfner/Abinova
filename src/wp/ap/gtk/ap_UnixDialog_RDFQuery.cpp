/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* Abinova
 * Copyright (C) 2011 AbiSource, Inc.
 * Copyright (C) Ben Martin
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
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "GTKCommon.h"
#include <gtk/gtk.h>
#include "ut_string.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "xap_UnixDialogHelper.h"

#include "xap_Dialog_Id.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"
#include "ap_Dialog_RDFQuery.h"
#include "ap_UnixDialog_RDFQuery.h"

#include <vector>

/* One result-binding row for the GtkColumnView model: a flat array of
 * strings, one per binding column (index = position in the binding
 * map, columns carry their index in factory data). */
#define ABI_TYPE_RDF_QUERY_ROW (abi_rdf_query_row_get_type())
G_DECLARE_FINAL_TYPE (AbiRdfQueryRow, abi_rdf_query_row, ABI, RDF_QUERY_ROW, GObject)

struct _AbiRdfQueryRow
{
	GObject parent_instance;
	gchar **values;   /* nvalues entries */
	guint nvalues;
};

G_DEFINE_TYPE (AbiRdfQueryRow, abi_rdf_query_row, G_TYPE_OBJECT)

static void
abi_rdf_query_row_init (AbiRdfQueryRow * /*self*/)
{
}

static void
abi_rdf_query_row_finalize (GObject *object)
{
	AbiRdfQueryRow *row = ABI_RDF_QUERY_ROW (object);
	g_strfreev (row->values);
	G_OBJECT_CLASS (abi_rdf_query_row_parent_class)->finalize (object);
}

static void
abi_rdf_query_row_class_init (AbiRdfQueryRowClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = abi_rdf_query_row_finalize;
}

static AbiRdfQueryRow *
abi_rdf_query_row_new (const std::vector< std::string >& values)
{
	AbiRdfQueryRow *row =
		ABI_RDF_QUERY_ROW (g_object_new (ABI_TYPE_RDF_QUERY_ROW, nullptr));
	row->nvalues = values.size();
	row->values = g_new0 (gchar*, row->nvalues + 1);
	for (guint i = 0; i < row->nvalues; i++)
		row->values[i] = g_strdup (values[i].c_str());
	return row;
}

static void
s_query_cell_setup (GtkSignalListItemFactory * /*factory*/,
					GtkListItem *item,
					gpointer /*data*/)
{
	GtkWidget *label = gtk_label_new (nullptr);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_list_item_set_child (item, label);
}

static void
s_query_cell_bind (GtkSignalListItemFactory *factory,
				   GtkListItem *item,
				   gpointer /*data*/)
{
	int cidx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (factory), "colidx"));
	GtkLabel *label = GTK_LABEL (gtk_list_item_get_child (item));
	AbiRdfQueryRow *row =
		ABI_RDF_QUERY_ROW (gtk_list_item_get_item (item));
	const gchar *text = (row && static_cast<guint>(cidx) < row->nvalues)
		? row->values[cidx] : "";
	gtk_label_set_text (label, text);
}

static gint
s_sort_bindings (gconstpointer p1, gconstpointer p2, gpointer data)
{
	const AbiRdfQueryRow *a = static_cast<const AbiRdfQueryRow *>(p1);
	const AbiRdfQueryRow *b = static_cast<const AbiRdfQueryRow *>(p2);
	guint cidx = GPOINTER_TO_UINT (data);
	const gchar *sa = (a && cidx < a->nvalues) ? a->values[cidx] : "";
	const gchar *sb = (b && cidx < b->nvalues) ? b->values[cidx] : "";
	return g_utf8_collate (sa ? sa : "", sb ? sb : "");
}

static GtkColumnViewColumn *
s_query_column (const char *title, int cidx)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
	g_object_set_data (G_OBJECT (factory), "colidx", GINT_TO_POINTER (cidx));
	g_signal_connect (factory, "setup", G_CALLBACK (s_query_cell_setup), nullptr);
	g_signal_connect (factory, "bind", G_CALLBACK (s_query_cell_bind), nullptr);

	/* gtk_column_view_column_new is transfer-full on the factory */
	GtkColumnViewColumn *col = gtk_column_view_column_new (title, factory);
	GtkSorter *sorter = GTK_SORTER (gtk_custom_sorter_new (
		s_sort_bindings, GINT_TO_POINTER (cidx), nullptr));
	gtk_column_view_column_set_sorter (col, sorter);
	g_object_unref (sorter);
	gtk_column_view_column_set_resizable (col, TRUE);
	gtk_column_view_column_set_expand (col, TRUE);
	return col;
}

void
AP_UnixDialog_RDFQuery__onExecuteClicked ( GtkButton * /*button*/,
                                           gpointer   data )
{
	AP_UnixDialog_RDFQuery *dlg = static_cast <AP_UnixDialog_RDFQuery *>(data);
	dlg->onExecuteClicked ();
}
void
AP_UnixDialog_RDFQuery__onShowAllClicked ( GtkButton * /*button*/,
                                           gpointer   data )
{
	AP_UnixDialog_RDFQuery *dlg = static_cast <AP_UnixDialog_RDFQuery *>(data);
	dlg->onShowAllClicked ();
}



/*!
* Event dispatcher for button "close".
*/
void
AP_UnixDialog_RDFQuery__onDialogResponse (GtkDialog * /*dialog*/,
									  gint 		response,
									  gpointer  data)
{
	AP_UnixDialog_RDFQuery *dlg = static_cast <AP_UnixDialog_RDFQuery *>(data);
	if (response == GTK_RESPONSE_CLOSE) {
		dlg->destroy ();		
	}
}

/*!
* Event dispatcher for window.
*/
gboolean
AP_UnixDialog_RDFQuery__onDeleteWindow (GtkWidget * /*widget*/,
									gpointer  data)
{
	AP_UnixDialog_RDFQuery *dlg = static_cast <AP_UnixDialog_RDFQuery *>(data);
	if (dlg->getWindow ()) {
		dlg->destroy ();
	}
	return TRUE;
}



/*!
* Static ctor.
*/
XAP_Dialog * 
AP_UnixDialog_RDFQuery::static_constructor(XAP_DialogFactory *pFactory,
									   XAP_Dialog_Id 	 id)
{
	AP_UnixDialog_RDFQuery *dlg = new AP_UnixDialog_RDFQuery (pFactory, id);
	return dlg;
}

/*!
* Ctor.
*/
AP_UnixDialog_RDFQuery::AP_UnixDialog_RDFQuery(XAP_DialogFactory *pDlgFactory,
									   XAP_Dialog_Id 	 id)
	: AP_Dialog_RDFQuery   (pDlgFactory, id)
    , m_wDialog(nullptr)
    , m_btClose(nullptr)
    , m_btExecute(nullptr)
    , m_btShowAll(nullptr)
    , m_query(nullptr)
    , m_resultsView(nullptr)
    , m_resultsStore(nullptr)
    , m_sortModel(nullptr)
    , m_status(nullptr)
{
}

/*!
* Dtor.
*/
AP_UnixDialog_RDFQuery::~AP_UnixDialog_RDFQuery ()
{
	UT_DEBUGMSG (("~AP_UnixDialog_RDFQuery ()\n"));
	g_clear_object (&m_resultsStore);
	g_clear_object (&m_sortModel);
}



void
AP_UnixDialog_RDFQuery::clear()
{
    AP_Dialog_RDFQuery::clear();
    if( m_resultsStore )
        g_list_store_remove_all( m_resultsStore );
}

void
AP_UnixDialog_RDFQuery::addStatement( const PD_RDFStatement& st )
{
    AP_Dialog_RDFQuery::addStatement(st);
}

void
AP_UnixDialog_RDFQuery::setupBindingsView( std::map< std::string, std::string >& b )
{
    // the fill loop below writes b.size() + 2 entries
    if( b.size() + 2 > C_COLUMN_ARRAY_SIZE )
    {
        return;
    }

    /* swap in a fresh row store; the sort/selection chain stays */
    GListStore* m = g_list_store_new( ABI_TYPE_RDF_QUERY_ROW );
    gtk_sort_list_model_set_model( m_sortModel, G_LIST_MODEL( m ) );
    g_clear_object( &m_resultsStore );
    m_resultsStore = m;

    GListModel* cols = gtk_column_view_get_columns( m_resultsView );
    while( g_list_model_get_n_items( cols ) )
    {
        GtkColumnViewColumn* c = GTK_COLUMN_VIEW_COLUMN(
            g_list_model_get_item( cols, 0 ) );
        gtk_column_view_remove_column( m_resultsView, c );
        g_object_unref( c );
    }

    /* column cidx = position of the binding in the (sorted) map;
     * display order is reordered below */
    typedef std::list< std::pair< std::string, int > > cols_t;
    cols_t cols2;

    int colid = 0;
    for( std::map< std::string, std::string >::iterator iter = b.begin();
         iter != b.end(); ++iter, ++colid )
    {
        cols2.push_back( std::make_pair( iter->first, colid ) );
    }

    //
    // Make sure some columns appear in the desired order
    // which is not simply lexigraphical
    //
    typedef std::list< std::string > stringlist_t;
    stringlist_t hotColumns;
    hotColumns.push_back("o");
    hotColumns.push_back("p");
    hotColumns.push_back("s");
    hotColumns.push_back("object");
    hotColumns.push_back("predicate");
    hotColumns.push_back("subject");
    for( stringlist_t::iterator si = hotColumns.begin();
         si != hotColumns.end(); ++si )
    {
        std::string cname = *si;

        for( cols_t::iterator ci = cols2.begin(); ci!=cols2.end(); ++ci )
        {
            if( ci->first == cname )
            {
                cols2.push_front( *ci );
                cols2.erase( ci );
                break;
            }
        }
    }

    for( cols_t::iterator ci = cols2.begin(); ci!=cols2.end(); ++ci )
    {
        GtkColumnViewColumn* col = s_query_column( ci->first.c_str(), ci->second );
        gtk_column_view_append_column( m_resultsView, col );
        g_object_unref( col );
    }

}


void
AP_UnixDialog_RDFQuery::addBinding( std::map< std::string, std::string >& b )
{
    xxx_UT_DEBUGMSG(("addBinding() b.size(): %u\n", b.size()));
    if( b.size() >= C_COLUMN_ARRAY_SIZE )
    {
        return;
    }
    AP_Dialog_RDFQuery::addBinding(b);

    std::vector< std::string > values;
    for( std::map< std::string, std::string >::iterator iter = b.begin();
         iter != b.end(); ++iter )
    {
        xxx_UT_DEBUGMSG(("addBinding() iter->second: %d\n", iter->second.c_str()));
        values.push_back( uriToPrefixed( iter->second ) );
    }

    AbiRdfQueryRow* row = abi_rdf_query_row_new( values );
    g_list_store_append( m_resultsStore, row );
    g_object_unref( row );
}



void
AP_UnixDialog_RDFQuery::onExecuteClicked()
{
    UT_DEBUGMSG(("onExecuteClicked() store:%p\n", m_resultsStore ));
    UT_DEBUGMSG(("onExecuteClicked() model2:%p\n", gtk_column_view_get_model( m_resultsView ) ));

    std::string q = tostr(GTK_TEXT_VIEW (m_query));
    executeQuery( q );
}

void
AP_UnixDialog_RDFQuery::onShowAllClicked()
{
    UT_DEBUGMSG(("onShowAllClicked()\n" ));
    showAllRDF();
}


void
AP_UnixDialog_RDFQuery::setStatus( const std::string& msg )
{
    gtk_label_set_text( GTK_LABEL(m_status), msg.c_str() );
}

void
AP_UnixDialog_RDFQuery::setQueryString( const std::string& sparql )
{
    GtkTextBuffer* b = gtk_text_view_get_buffer( GTK_TEXT_VIEW (m_query) );
    gtk_text_buffer_set_text( b, sparql.c_str(), -1 );
}



/*!
* Build dialog.
*/
void 
AP_UnixDialog_RDFQuery::_constructWindow (XAP_Frame * /*pFrame*/) 
{
	UT_DEBUGMSG (("ROB: _constructWindow ()\n"));		

	const XAP_StringSet *pSS = m_pApp->getStringSet();
	std::string text;

	// load the dialog from the UI file
	GtkBuilder* builder = newDialogBuilderFromResource("ap_UnixDialog_RDFQuery.ui");

	m_wDialog = GTK_WIDGET(gtk_builder_get_object(builder, "ap_UnixDialog_RDFQuery"));
	m_btClose = GTK_WIDGET(gtk_builder_get_object(builder, "btClose"));
	m_btExecute = GTK_WIDGET(gtk_builder_get_object(builder, "btExecute"));
    m_btShowAll = GTK_WIDGET(gtk_builder_get_object(builder, "btShowAll"));
    m_query     = GTK_WIDGET(gtk_builder_get_object(builder, "query"));
	m_resultsView  = GTK_COLUMN_VIEW(gtk_builder_get_object(builder, "resultsView"));
    m_status    = GTK_WIDGET(gtk_builder_get_object(builder, "status"));

    // localization
    localizeButton(m_btShowAll, pSS, AP_STRING_ID_DLG_RDF_Query_ShowAll); 
    localizeButton(m_btExecute, pSS, AP_STRING_ID_DLG_RDF_Query_Execute); 
    GtkTextIter iter;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_query));
    gtk_text_buffer_get_iter_at_line(buffer, &iter, 0);
    pSS->getValueUTF8(AP_STRING_ID_DLG_RDF_Query_Comment, text);
    gtk_text_buffer_insert(buffer, &iter, text.c_str(), -1);     

    /* the GTK4 model-wrapper ctors are transfer-full: ref what we keep */
    m_resultsStore = g_list_store_new( ABI_TYPE_RDF_QUERY_ROW );
    m_sortModel = gtk_sort_list_model_new(
        G_LIST_MODEL( g_object_ref( m_resultsStore ) ), nullptr );
    GtkMultiSelection* selection =
        gtk_multi_selection_new( G_LIST_MODEL( g_object_ref( m_sortModel ) ) );
    gtk_column_view_set_model( m_resultsView, GTK_SELECTION_MODEL( selection ) );
    g_object_unref( selection );
    gtk_sort_list_model_set_sorter( m_sortModel,
                                    gtk_column_view_get_sorter( m_resultsView ) );

    const XAP_String_Id titles[3] = {
        AP_STRING_ID_DLG_RDF_Query_Column_Subject,
        AP_STRING_ID_DLG_RDF_Query_Column_Predicate,
        AP_STRING_ID_DLG_RDF_Query_Column_Object
    };
    for( int colid = 0; colid < 3; ++colid )
    {
        pSS->getValueUTF8( titles[ colid ], text );
        GtkColumnViewColumn* col = s_query_column( text.c_str(), colid );
        gtk_column_view_append_column( m_resultsView, col );
        g_object_unref( col );
    }

    /////////////
	/// Signals
    ///
	g_signal_connect (GTK_BUTTON (m_btExecute), "clicked", 
					  G_CALLBACK (AP_UnixDialog_RDFQuery__onExecuteClicked), static_cast <gpointer>(this));
	g_signal_connect (GTK_BUTTON (m_btShowAll), "clicked", 
					  G_CALLBACK (AP_UnixDialog_RDFQuery__onShowAllClicked), static_cast <gpointer>(this));
	g_signal_connect (GTK_DIALOG (m_wDialog), "response",
					  G_CALLBACK (AP_UnixDialog_RDFQuery__onDialogResponse), static_cast <gpointer>(this));
	g_signal_connect (m_wDialog, "close-request",
					  G_CALLBACK (AP_UnixDialog_RDFQuery__onDeleteWindow), static_cast <gpointer>(this));

#ifndef WITH_REDLAND
	gtk_widget_set_sensitive(m_btExecute, FALSE);  
	gtk_widget_set_sensitive(m_btShowAll, FALSE);  
#endif

	g_object_unref(G_OBJECT(builder));
}

/*!
* Update dialog's data.
*/
void 
AP_UnixDialog_RDFQuery::_updateWindow ()
{
    UT_DEBUGMSG(("RDFQuery::_updateWindow()\n"));
	ConstructWindowName ();
	gtk_window_set_title (GTK_WINDOW (m_wDialog), m_WindowName.c_str() );
}

void 
AP_UnixDialog_RDFQuery::runModeless (XAP_Frame * pFrame)
{
	UT_DEBUGMSG (("MIQ: runModeless ()\n"));
	_constructWindow (pFrame);
	UT_ASSERT (m_wDialog);
	_updateWindow ();
	abiSetupModelessDialog (GTK_DIALOG (m_wDialog), pFrame, this, GTK_RESPONSE_CLOSE);
	gtk_widget_set_visible(m_wDialog, TRUE);
	gtk_window_present (GTK_WINDOW (m_wDialog));
}

void 
AP_UnixDialog_RDFQuery::notifyActiveFrame (XAP_Frame * /*pFrame*/)
{
	UT_DEBUGMSG (("MIQ: notifyActiveFrame ()\n"));
	UT_ASSERT (m_wDialog);
	_updateWindow ();
}

void 
AP_UnixDialog_RDFQuery::activate (void)
{
	UT_ASSERT (m_wDialog);
	UT_DEBUGMSG (("MIQ: AP_UnixDialog_RDFQuery::activate ()\n"));
	_updateWindow ();
	gtk_window_present (GTK_WINDOW (m_wDialog));
}

void 
AP_UnixDialog_RDFQuery::destroy ()
{
	UT_DEBUGMSG (("MIQ: AP_UnixDialog_RDFQuery::destroy ()\n"));
	modeless_cleanup ();
	if (m_wDialog) {
		abiDestroyWidget(m_wDialog); // TOPLEVEL
		m_wDialog = nullptr;
	}
}

