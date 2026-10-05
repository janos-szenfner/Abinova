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
#include "ut_go_file.h"

#include "xap_Dialog_Id.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"
#include "ap_Dialog_RDFEditor.h"
#include "ap_UnixDialog_RDFEditor.h"
#include "pd_RDFSupport.h"
#include "xap_GtkComboBoxHelpers.h"

#include <sstream>

const char* GOBJ_COL_NUM = "GOBJ_COL_NUM";
static const char RDF_ROW_DATA[] = "rdf-triple-row";

/* One row object per RDF statement for the GtkColumnView model; holds
 * the three prefixed display strings the editable cells write to. */
#define ABI_TYPE_RDF_TRIPLE_ROW (abi_rdf_triple_row_get_type())
G_DECLARE_FINAL_TYPE (AbiRdfTripleRow, abi_rdf_triple_row, ABI, RDF_TRIPLE_ROW, GObject)

struct _AbiRdfTripleRow
{
	GObject parent_instance;
	gchar *subj;
	gchar *pred;
	gchar *obj;
};

G_DEFINE_TYPE (AbiRdfTripleRow, abi_rdf_triple_row, G_TYPE_OBJECT)

static void
abi_rdf_triple_row_init (AbiRdfTripleRow * /*self*/)
{
}

static void
abi_rdf_triple_row_finalize (GObject *object)
{
	AbiRdfTripleRow *row = ABI_RDF_TRIPLE_ROW (object);
	g_free (row->subj);
	g_free (row->pred);
	g_free (row->obj);
	G_OBJECT_CLASS (abi_rdf_triple_row_parent_class)->finalize (object);
}

static void
abi_rdf_triple_row_class_init (AbiRdfTripleRowClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = abi_rdf_triple_row_finalize;
}

static AbiRdfTripleRow *
abi_rdf_triple_row_new (const gchar *subj, const gchar *pred,
						const gchar *obj)
{
	AbiRdfTripleRow *row =
		ABI_RDF_TRIPLE_ROW (g_object_new (ABI_TYPE_RDF_TRIPLE_ROW, nullptr));
	row->subj = g_strdup (subj);
	row->pred = g_strdup (pred);
	row->obj  = g_strdup (obj);
	return row;
}

static const char *
s_row_field (const AbiRdfTripleRow *row, int cidx)
{
	switch (cidx)
	{
		case 0: return row->subj;
		case 1: return row->pred;
		default: return row->obj;
	}
}

static void
s_row_set_field (AbiRdfTripleRow *row, int cidx, const gchar *text)
{
	gchar **slot = (cidx == 0) ? &row->subj : (cidx == 1) ? &row->pred : &row->obj;
	g_free (*slot);
	*slot = g_strdup (text);
}

static gint
s_sort_triples (gconstpointer p1, gconstpointer p2, gpointer data)
{
	const AbiRdfTripleRow *a = static_cast<const AbiRdfTripleRow *>(p1);
	const AbiRdfTripleRow *b = static_cast<const AbiRdfTripleRow *>(p2);
	int cidx = GPOINTER_TO_INT (data);
	const gchar *sa = s_row_field (a, cidx);
	const gchar *sb = s_row_field (b, cidx);
	return g_utf8_collate (sa ? sa : "", sb ? sb : "");
}

static void
s_cell_commit (GtkWidget *text, AP_UnixDialog_RDFEditor *dlg)
{
	AbiRdfTripleRow *row = static_cast<AbiRdfTripleRow*>(
		g_object_get_data (G_OBJECT (text), RDF_ROW_DATA));
	if (!row)
		return;
	int cidx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (text), GOBJ_COL_NUM));
	dlg->commitCellEdit (row, gtk_editable_get_text (GTK_EDITABLE (text)), cidx);
}

static void
s_cell_activate (GtkText *text, gpointer data)
{
	s_cell_commit (GTK_WIDGET (text), static_cast<AP_UnixDialog_RDFEditor*>(data));
}

static void
s_cell_pressed (GtkGestureClick *gesture, gint /*n_press*/,
				gdouble /*x*/, gdouble /*y*/, gpointer data)
{
	GtkWidget *text = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
	AbiRdfTripleRow *row = static_cast<AbiRdfTripleRow*>(
		g_object_get_data (G_OBJECT (text), RDF_ROW_DATA));
	if (!row)
		return;
	GdkModifierType state = gtk_event_controller_get_current_event_state (
		GTK_EVENT_CONTROLLER (gesture));
	static_cast<AP_UnixDialog_RDFEditor*>(data)->selectRowForCellClick (
		row, static_cast<guint>(state));
}

static void
s_cell_focus_left (GtkEventControllerFocus *ctrl, gpointer data)
{
	s_cell_commit (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (ctrl)),
				   static_cast<AP_UnixDialog_RDFEditor*>(data));
}

static void
s_rdf_cell_setup (GtkSignalListItemFactory *factory,
				  GtkListItem *item,
				  gpointer data)
{
	AP_UnixDialog_RDFEditor *dlg = static_cast<AP_UnixDialog_RDFEditor*>(data);
	int cidx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (factory), GOBJ_COL_NUM));

	GtkWidget *text = gtk_text_new ();
	g_object_set_data (G_OBJECT (text), GOBJ_COL_NUM, GINT_TO_POINTER (cidx));
	g_signal_connect (text, "activate",
					  G_CALLBACK (s_cell_activate), dlg);

	GtkEventController *focus = gtk_event_controller_focus_new ();
	g_signal_connect (focus, "leave",
					  G_CALLBACK (s_cell_focus_left), dlg);
	gtk_widget_add_controller (text, focus);

	/* capture-phase click: drive the row's selection state before the
	 * GtkText claims the press for editing */
	GtkGesture *click = gtk_gesture_click_new ();
	gtk_event_controller_set_propagation_phase (
		GTK_EVENT_CONTROLLER (click), GTK_PHASE_CAPTURE);
	g_signal_connect (click, "pressed",
					  G_CALLBACK (s_cell_pressed), dlg);
	gtk_widget_add_controller (text, GTK_EVENT_CONTROLLER (click));

	gtk_list_item_set_child (item, text);
}

static void
s_rdf_cell_bind (GtkSignalListItemFactory *factory,
				 GtkListItem *item,
				 gpointer /*data*/)
{
	int cidx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (factory), GOBJ_COL_NUM));
	GtkWidget *text = GTK_WIDGET (gtk_list_item_get_child (item));
	AbiRdfTripleRow *row =
		ABI_RDF_TRIPLE_ROW (gtk_list_item_get_item (item));
	g_object_set_data (G_OBJECT (text), RDF_ROW_DATA, row);
	const char *field = row ? s_row_field (row, cidx) : nullptr;
	gtk_editable_set_text (GTK_EDITABLE (text), field ? field : "");
}

static void
s_rdf_cell_unbind (GtkSignalListItemFactory * /*factory*/,
				   GtkListItem *item,
				   gpointer /*data*/)
{
	GtkWidget *text = GTK_WIDGET (gtk_list_item_get_child (item));
	if (text)
		g_object_set_data (G_OBJECT (text), RDF_ROW_DATA, nullptr);
}

static GtkListItemFactory *
s_rdf_cell_factory (AP_UnixDialog_RDFEditor *dlg, int cidx)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
	g_object_set_data (G_OBJECT (factory), GOBJ_COL_NUM, GINT_TO_POINTER (cidx));
	g_signal_connect (factory, "setup", G_CALLBACK (s_rdf_cell_setup), dlg);
	g_signal_connect (factory, "bind", G_CALLBACK (s_rdf_cell_bind), dlg);
	g_signal_connect (factory, "unbind", G_CALLBACK (s_rdf_cell_unbind), dlg);
	return factory;
}


void
AP_UnixDialog_RDFEditor__onActionNew(GAction*, GVariant*, gpointer data)
{
    AP_UnixDialog_RDFEditor *dlg = static_cast <AP_UnixDialog_RDFEditor*>(data);
    dlg->createStatement();
}
void
AP_UnixDialog_RDFEditor__onActionCopy(GAction*, GVariant*, gpointer data)
{
    AP_UnixDialog_RDFEditor *dlg = static_cast <AP_UnixDialog_RDFEditor*>(data);
    dlg->copyStatement();
}


static void s_OnXMLIDChanged(GtkWidget * widget, GParamSpec */*pspec*/, AP_UnixDialog_RDFEditor* dlg)
{
	UT_UNUSED(widget);
	UT_ASSERT(widget && dlg);

    std::string xmlid = XAP_dropDownGetSelectedText( GTK_DROP_DOWN( widget ));
   	dlg->setRestrictedXMLID( xmlid );
}

void
AP_UnixDialog_RDFEditor__onActionDelete(GAction*, GVariant*, gpointer data)
{
    AP_UnixDialog_RDFEditor *dlg = static_cast <AP_UnixDialog_RDFEditor*>(data);
    dlg->onDelClicked();
}

void
AP_UnixDialog_RDFEditor__onActionImportRDFXML(GAction*, GVariant*, gpointer data)
{
    AP_UnixDialog_RDFEditor *dlg = static_cast <AP_UnixDialog_RDFEditor*>(data);
    dlg->onImportRDFXML();
}

void
AP_UnixDialog_RDFEditor__onActionExportRDFXML(GAction*, GVariant*, gpointer data)
{
    AP_UnixDialog_RDFEditor *dlg = static_cast <AP_UnixDialog_RDFEditor*>(data);
    dlg->onExportRDFXML();
}

void
AP_UnixDialog_RDFEditor__onShowAllClicked ( GtkButton * /*button*/,
                                            gpointer   data )
{
    AP_UnixDialog_RDFEditor *dlg = static_cast <AP_UnixDialog_RDFEditor *>(data);
    dlg->onShowAllClicked ();
}



/*!
* Event dispatcher for button "close".
*/
void
AP_UnixDialog_RDFEditor__onDialogResponse ( GtkDialog * /*dialog*/,
                                            gint 		response,
                                            gpointer  data )
{
	AP_UnixDialog_RDFEditor *dlg = static_cast <AP_UnixDialog_RDFEditor *>(data);
	if (response == GTK_RESPONSE_CLOSE)
    {
		dlg->destroy ();
	}
}

/*!
* Event dispatcher for window.
*/
gboolean
AP_UnixDialog_RDFEditor__onDeleteWindow ( GtkWidget * /*widget*/,
                                          gpointer  data )
{
	AP_UnixDialog_RDFEditor *dlg = static_cast <AP_UnixDialog_RDFEditor *>(data);
	if (dlg->getWindow ())
    {
		dlg->destroy ();
	}
	return TRUE;
}

void
AP_UnixDialog_RDFEditor__onCursorChanged ( GtkSelectionModel*,
                                           guint, guint,
                                           gpointer  data )
{
	AP_UnixDialog_RDFEditor *dlg = static_cast <AP_UnixDialog_RDFEditor *>(data);
    dlg->onCursorChanged();
}


/*!
* Static ctor.
*/
XAP_Dialog * 
AP_UnixDialog_RDFEditor::static_constructor( XAP_DialogFactory *pFactory,
                                             XAP_Dialog_Id 	 id )
{
	AP_UnixDialog_RDFEditor *dlg = new AP_UnixDialog_RDFEditor (pFactory, id);
	return dlg;
}

/*!
* Ctor.
*/
AP_UnixDialog_RDFEditor::AP_UnixDialog_RDFEditor( XAP_DialogFactory *pDlgFactory,
                                                  XAP_Dialog_Id 	 id )
	: AP_Dialog_RDFEditor   (pDlgFactory, id)
    , m_wDialog(nullptr)
    , m_btClose(nullptr)
    , m_btShowAll(nullptr)
    , m_resultsView(nullptr)
    , m_resultsStore(nullptr)
    , m_sortModel(nullptr)
    , m_status(nullptr)
    , m_anewtriple(nullptr)
    , m_acopytriple(nullptr)
    , m_adeletetriple(nullptr)
    , m_aimportrdfxml(nullptr)
    , m_aexportrdfxml(nullptr)
    , m_selectedxmlid(nullptr)
    , m_restrictxmlidhidew(nullptr)
{
}

/*!
* Dtor.
*/
AP_UnixDialog_RDFEditor::~AP_UnixDialog_RDFEditor ()
{
	UT_DEBUGMSG (("~AP_UnixDialog_RDFEditor ()\n"));
	g_clear_object (&m_resultsStore);
	g_clear_object (&m_sortModel);
}



void
AP_UnixDialog_RDFEditor::clear()
{
    AP_Dialog_RDFEditor::clear();
    if (m_resultsStore)
        g_list_store_remove_all( m_resultsStore );
}

void
AP_UnixDialog_RDFEditor::addStatement( const PD_RDFStatement& stc )
{
    AP_Dialog_RDFEditor::addStatement(stc);
    PD_RDFStatement st = stc.uriToPrefixed( getModel() );

    AbiRdfTripleRow *row = abi_rdf_triple_row_new(
        st.getSubject().  toString().c_str(),
        st.getPredicate().toString().c_str(),
        st.getObject().   toString().c_str() );
    g_list_store_append( m_resultsStore, row );
    g_object_unref( row );
}

PD_RDFStatement
AP_UnixDialog_RDFEditor::rowToStatement( AbiRdfTripleRow* row )
{
    return PD_RDFStatement( getModel(),
                            PD_URI( row->subj ),
                            PD_URI( row->pred ),
                            PD_Object( row->obj ) );
}

/* position of the row whose statement equals st inside the view's
 * (possibly sorted) model, or GTK_INVALID_LIST_POSITION */
guint
AP_UnixDialog_RDFEditor::findRowPos( const PD_RDFStatement& st )
{
    GtkSelectionModel* sel = gtk_column_view_get_model( m_resultsView );
    GListModel* model = G_LIST_MODEL( sel );
    guint n = g_list_model_get_n_items( model );
    for( guint i = 0; i < n; ++i )
    {
        AbiRdfTripleRow* row =
            ABI_RDF_TRIPLE_ROW( g_list_model_get_item( model, i ) );
        bool match = row && (rowToStatement( row ) == st);
        if( row )
            g_object_unref( row );
        if( match )
            return i;
    }
    return GTK_INVALID_LIST_POSITION;
}

guint
AP_UnixDialog_RDFEditor::rowPosition( AbiRdfTripleRow* target )
{
    GtkSelectionModel* sel = gtk_column_view_get_model( m_resultsView );
    GListModel* model = G_LIST_MODEL( sel );
    guint n = g_list_model_get_n_items( model );
    for( guint i = 0; i < n; ++i )
    {
        gpointer item = g_list_model_get_item( model, i );
        bool match = (item == target);
        if( item )
            g_object_unref( item );
        if( match )
            return i;
    }
    return GTK_INVALID_LIST_POSITION;
}


void
AP_UnixDialog_RDFEditor::setSelection( const std::list< PD_RDFStatement >& l )
{
    GtkSelectionModel* sel = gtk_column_view_get_model( m_resultsView );
    for( std::list< PD_RDFStatement >::const_iterator iter = l.begin();
         iter != l.end(); ++iter )
    {
        guint pos = findRowPos( *iter );
        if( pos != GTK_INVALID_LIST_POSITION )
            gtk_selection_model_select_item( sel, pos, FALSE );
    }

    if( !l.empty() )
    {
        guint pos = findRowPos( l.front() );
        if( pos != GTK_INVALID_LIST_POSITION )
            gtk_column_view_scroll_to( m_resultsView, pos, nullptr,
                                       static_cast<GtkListScrollFlags>(
                                           GTK_LIST_SCROLL_FOCUS ), nullptr );
    }
}


void
AP_UnixDialog_RDFEditor::hideRestrictionXMLID( bool v )
{
    AP_Dialog_RDFEditor::hideRestrictionXMLID( v );

	// check that the UI is actually loaded.
	if(!m_wDialog) {
		return;
	}

    if( v )
    {
        UT_DEBUGMSG(("AP_UnixDialog_RDFEditor, no restriction HIDING! w:%p\n", m_restrictxmlidhidew ));
        gtk_widget_set_visible( m_restrictxmlidhidew , FALSE);
        gtk_widget_set_visible( GTK_WIDGET(m_selectedxmlid) , FALSE);
    }
    else
    {
        PD_RDFModelHandle model;
        std::set< std::string > xmlids;
        getRDF()->addRelevantIDsForPosition( xmlids, getView()->getPoint() );
        UT_DEBUGMSG(("AP_UnixDialog_RDFEditor, have restricted xmlids size:%lu\n", static_cast<long unsigned>(xmlids.size() )));
        
		/// FIXME...
		setRestrictedModel( model );
    }
    
}





void
AP_UnixDialog_RDFEditor::onShowAllClicked()
{
    UT_DEBUGMSG(("onShowAllClicked()\n" ));
    showAllRDF();
}

PD_RDFStatement
AP_UnixDialog_RDFEditor::next( const PD_RDFStatement& st )
{
    guint pos = findRowPos( st );
    GtkSelectionModel* sel = gtk_column_view_get_model( m_resultsView );
    GListModel* model = G_LIST_MODEL( sel );
    if( pos != GTK_INVALID_LIST_POSITION &&
        pos + 1 < g_list_model_get_n_items( model ) )
    {
        AbiRdfTripleRow* row =
            ABI_RDF_TRIPLE_ROW( g_list_model_get_item( model, pos + 1 ) );
        PD_RDFStatement ret = rowToStatement( row );
        g_object_unref( row );
        return ret;
    }

    // no good old chum
    PD_RDFStatement ret;
    return ret;
}

void
AP_UnixDialog_RDFEditor::onDelClicked()
{
    UT_DEBUGMSG(("onDelClicked()\n" ));
    std::list< PD_RDFStatement > l = getSelection();
    if( l.empty() )
        return;

    PD_RDFStatement n;
    if( l.size() == 1 )
    {
        n = next( l.front() );
    }
    
    PD_DocumentRDFMutationHandle m = getModel()->createMutation();
    for( std::list< PD_RDFStatement >::iterator iter = l.begin(); iter != l.end(); ++iter )
    {
        const PD_RDFStatement& st = *iter;
        xxx_UT_DEBUGMSG(("onDelClicked() removing statement: %s\n", st.toString().utf8_str()));
        m->remove( st );
        removeStatement( st );
        m_count--;
    }
    m->commit();
//    showAllRDF();

    if( n.isValid() )
    {
        std::list< PD_RDFStatement > zz;
        zz.push_back( n );
        setSelection( zz );
    }

    statusIsTripleCount();
}


void
AP_UnixDialog_RDFEditor::commitCellEdit( AbiRdfTripleRow *row,
                                         const char *new_text,
                                         int cidx )
{
    xxx_UT_DEBUGMSG(("commitCellEdit() nt: %s\n", new_text));
    if( !row )
        return;
    const char* old_text = s_row_field( row, cidx );
    if( old_text && !strcmp( old_text, new_text ) )
        return;

    PD_URI n( new_text );
    n = n.prefixedToURI( getModel() );

    PD_RDFStatement oldst = rowToStatement( row );
    PD_RDFStatement newst;
    switch( cidx )
    {
        case C_SUBJ_COLUMN:
            newst = PD_RDFStatement( n, oldst.getPredicate(), oldst.getObject() );
            break;
        case C_PRED_COLUMN:
            newst = PD_RDFStatement( oldst.getSubject(), n, oldst.getObject() );
            break;
        case C_OBJ_COLUMN:
            newst = PD_RDFStatement( oldst.getSubject(), oldst.getPredicate(), PD_Object( n.toString() ) );
            break;
        default:
            UT_ASSERT_NOT_REACHED();
    }

    PD_DocumentRDFMutationHandle m = getModel()->createMutation();
    if( m->add( newst ) )
    {
        m->remove( oldst );
        m->commit();
        s_row_set_field( row, cidx, new_text );
    }
}

void
AP_UnixDialog_RDFEditor::selectRowForCellClick( AbiRdfTripleRow *row,
                                                guint modifiers )
{
    if( !row || !m_resultsView )
        return;
    guint pos = rowPosition( row );
    if( pos == GTK_INVALID_LIST_POSITION )
        return;

    GtkSelectionModel* sel = gtk_column_view_get_model( m_resultsView );
    if( modifiers & GDK_CONTROL_MASK )
    {
        GtkBitset* selected = gtk_selection_model_get_selection( sel );
        bool wasSelected = gtk_bitset_contains( selected, pos );
        gtk_bitset_unref( selected );
        if( wasSelected )
            gtk_selection_model_unselect_item( sel, pos );
        else
            gtk_selection_model_select_item( sel, pos, FALSE );
    }
    else if( modifiers & GDK_SHIFT_MASK )
    {
        GtkBitset* selected = gtk_selection_model_get_selection( sel );
        if( gtk_bitset_is_empty( selected ) )
        {
            gtk_bitset_unref( selected );
            gtk_selection_model_select_item( sel, pos, TRUE );
        }
        else
        {
            guint anchor = gtk_bitset_get_minimum( selected );
            gtk_bitset_unref( selected );
            guint lo = MIN( anchor, pos );
            guint hi = MAX( anchor, pos );
            gtk_selection_model_select_range( sel, lo, hi - lo + 1, TRUE );
        }
    }
    else
    {
        gtk_selection_model_select_item( sel, pos, TRUE );
    }
    onCursorChanged();
}

static std::string tostr( GsfInput* gsf )
{
    gsf_off_t sz = gsf_input_size( gsf );
    guint8 const * d = gsf_input_read(gsf, sz, nullptr);
    std::string ret = std::string(const_cast<char*>(reinterpret_cast<const char*>(d)));
    return ret;
}


void
AP_UnixDialog_RDFEditor::onImportRDFXML()
{
    xxx_UT_DEBUGMSG(("onImportRDFXML()\n"));

    UT_runDialog_AskForPathname afp( XAP_DIALOG_ID_FILE_IMPORT );
    afp.appendFiletype( "RDF/XML Triple File", "rdf" );

    if( afp.run( getActiveFrame() ) )
    {
        xxx_UT_DEBUGMSG(("onImportRDFXML() path: %s", afp.getPath().utf8_str()));
        GError* err = nullptr;
        GsfInput* gsf = UT_go_file_open( afp.getPath().c_str(), &err );
        std::string rdfxml = tostr( gsf );
        g_object_unref (G_OBJECT (gsf));

        xxx_UT_DEBUGMSG(("rdfxml: %s\n", rdfxml.c_str()));
        PD_DocumentRDFMutationHandle m = getModel()->createMutation();
		// FIXME check the error code
        /* UT_Error e =*/ loadRDFXML( m, rdfxml );
        m->commit();

        xxx_UT_DEBUGMSG(("count of triples: %ld\n", getModel()->size()));
        showAllRDF();
    }
    gtk_window_present( GTK_WINDOW( m_wDialog ));
}

void
AP_UnixDialog_RDFEditor::onExportRDFXML()
{
    xxx_UT_DEBUGMSG(("onExportRDFXML()\n"));

    UT_runDialog_AskForPathname afp( XAP_DIALOG_ID_FILE_EXPORT );
    afp.appendFiletype( "RDF/XML Triple File", "rdf" );
    afp.setDefaultFiletype( "RDF/XML Triple File" );

    if( afp.run( getActiveFrame() ) )
    {
        xxx_UT_DEBUGMSG(("onExportRDFXML() path: %s\n", afp.getPath().utf8_str()));
        std::string rdfxml = toRDFXML( getModel() );
        GError* err = nullptr;
        GsfOutput* gsf = UT_go_file_create( afp.getPath().c_str(), &err );
        gsf_output_write( gsf, rdfxml.size(), reinterpret_cast<const guint8*>(rdfxml.data() ));
        gsf_output_close( gsf );
    }
    gtk_window_present( GTK_WINDOW( m_wDialog ));
}



void
AP_UnixDialog_RDFEditor::setStatus( const std::string& msg )
{
    gtk_label_set_text( GTK_LABEL(m_status), msg.c_str() );
}



/*!
* Build dialog.
*/
void 
AP_UnixDialog_RDFEditor::_constructWindow (XAP_Frame * /*pFrame*/) 
{
	UT_DEBUGMSG (("MIQ: _constructWindow ()\n"));		

	const XAP_StringSet *pSS = m_pApp->getStringSet();
	std::string text;

	// load the dialog from the UI file
	GtkBuilder* builder = newDialogBuilderFromResource("ap_UnixDialog_RDFEditor.ui");

	m_wDialog = GTK_WIDGET(gtk_builder_get_object(builder, "ap_UnixDialog_RDFEditor"));
	m_btClose = GTK_WIDGET(gtk_builder_get_object(builder, "btClose"));
    m_btShowAll = GTK_WIDGET(gtk_builder_get_object(builder, "btShowAll"));
	m_resultsView   = GTK_COLUMN_VIEW(gtk_builder_get_object(builder, "resultsView"));
    m_status        = GTK_WIDGET(gtk_builder_get_object(builder, "status"));
    m_selectedxmlid = GTK_DROP_DOWN(gtk_builder_get_object(builder, "selectedxmlid"));
    m_restrictxmlidhidew = GTK_WIDGET(gtk_builder_get_object(builder, "restrictxmlidhidew"));

    // Create actions
    GSimpleActionGroup* action_group = g_simple_action_group_new();
    gtk_widget_insert_action_group(m_wDialog, "rdf", G_ACTION_GROUP(action_group));
    g_object_unref(action_group);
    m_anewtriple = g_simple_action_new("newtriple", nullptr);
    g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(m_anewtriple));
    m_acopytriple = g_simple_action_new("copytriple", nullptr);
    g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(m_acopytriple));
    m_adeletetriple = g_simple_action_new("deletetriple", nullptr);
    g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(m_adeletetriple));
    m_aimportrdfxml = g_simple_action_new("importrdfxml", nullptr);
    g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(m_aimportrdfxml));
    m_aexportrdfxml = g_simple_action_new("exportrdfxml", nullptr);
    g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(m_aexportrdfxml));

    // localization: the menubar is a GMenu model (GTK4 removed
    // GtkMenuItem), so localize the submenu labels on the model itself
    {
        GMenu *menu = G_MENU(gtk_builder_get_object(builder, "rdfmenubar"));
        if (menu)
        {
            auto localizeSubmenu = [&](int pos, XAP_String_Id id) {
                std::string s;
                if (!pSS->getValueUTF8(id, s))
                    return;
                GMenuItem *it = g_menu_item_new_from_model(G_MENU_MODEL(menu), pos);
                if (!it)
                    return;
                /* GMenu labels show literal text; convert the string
                 * table's '&' mnemonic marker to the '_' form the menu
                 * items interpret, like _ev_convert for the main menus */
                convertMnemonics(s);
                g_menu_item_set_label(it, s.c_str());
                g_menu_remove(menu, pos);
                g_menu_insert_item(menu, pos, it);
                g_object_unref(it);
            };
            localizeSubmenu(0, AP_STRING_ID_DLG_RDF_Editor_Menu_File);
            localizeSubmenu(1, AP_STRING_ID_DLG_RDF_Editor_Menu_Triple);
        }
    }
    localizeButton(m_btShowAll, pSS, AP_STRING_ID_DLG_RDF_Editor_ShowAll);
    localizeLabel(GTK_WIDGET(gtk_builder_get_object(builder, "lbRestrict")), pSS, AP_STRING_ID_DLG_RDF_Editor_Restrict);

    /* the GTK4 model-wrapper ctors are transfer-full: ref what we keep */
    m_resultsStore = g_list_store_new( ABI_TYPE_RDF_TRIPLE_ROW );
    m_sortModel = gtk_sort_list_model_new(
        G_LIST_MODEL( g_object_ref( m_resultsStore ) ), nullptr );
    GtkMultiSelection* selection =
        gtk_multi_selection_new( G_LIST_MODEL( g_object_ref( m_sortModel ) ) );
    gtk_column_view_set_model( m_resultsView, GTK_SELECTION_MODEL( selection ) );
    g_object_unref( selection );
    /* header clicks drive the per-column sorters */
    gtk_sort_list_model_set_sorter( m_sortModel,
                                    gtk_column_view_get_sorter( m_resultsView ) );

    int colid = 0;
    const XAP_String_Id titles[ C_COLUMN_COUNT ] = {
        AP_STRING_ID_DLG_RDF_Query_Column_Subject,
        AP_STRING_ID_DLG_RDF_Query_Column_Predicate,
        AP_STRING_ID_DLG_RDF_Query_Column_Object
    };
    for( colid = 0; colid < C_COLUMN_COUNT; ++colid )
    {
        pSS->getValueUTF8( titles[ colid ], text );
        GtkColumnViewColumn* col = gtk_column_view_column_new(
            text.c_str(), s_rdf_cell_factory( this, colid ) );
        GtkSorter* sorter = GTK_SORTER( gtk_custom_sorter_new(
            s_sort_triples, GINT_TO_POINTER( colid ), nullptr ) );
        gtk_column_view_column_set_sorter( col, sorter );
        g_object_unref( sorter );
        gtk_column_view_column_set_resizable( col, TRUE );
        gtk_column_view_column_set_expand( col, TRUE );
        gtk_column_view_append_column( m_resultsView, col );
        g_object_unref( col );
    }


    if( m_hideRestrictionXMLID )
    {
        UT_DEBUGMSG(("AP_UnixDialog_RDFEditor, no restriction HIDING! w:%p\n", m_restrictxmlidhidew ));
        if( GtkWidget* w = GTK_WIDGET(gtk_builder_get_object(builder, "topvbox")))
        {
            xap_gtk_container_remove (w,  m_restrictxmlidhidew );
        }
//        gtk_widget_set_visible( m_restrictxmlidhidew , FALSE);
//        gtk_widget_set_visible( GTK_WIDGET(m_selectedxmlid) , FALSE);
        setRestrictedXMLID( "" );
    }
    else
    {
        XAP_makeGtkDropDown( m_selectedxmlid );

        PT_DocPosition point = getView()->getPoint();
        if( PD_DocumentRDFHandle rdf = getRDF() )
        {
            std::set< std::string > xmlids;
            rdf->addRelevantIDsForPosition( xmlids, point );

            bool combined = false; 
            std::stringstream combinedxmlidss;
            for( std::set< std::string >::const_iterator iter = xmlids.begin();
                 iter != xmlids.end(); ++iter )
            {
                if( iter != xmlids.begin() )
                {
                    combinedxmlidss << ",";
                    combined = true;
                }
                combinedxmlidss << *iter;
            }
            XAP_appendDropDownTextAndInt( m_selectedxmlid, combinedxmlidss.str().c_str(), 0 );
            setRestrictedXMLID( combinedxmlidss.str() );
            
            if (combined)
            {
                int idx = 1;
                for( std::set< std::string >::const_iterator iter = xmlids.begin();
                     iter != xmlids.end(); ++iter, ++idx )
                {
                    XAP_appendDropDownTextAndInt( m_selectedxmlid, iter->c_str(), idx );
                }

                gtk_drop_down_set_selected( m_selectedxmlid, 0 );
        
                // std::list< std::string > xmlids;
                // getRDF()->addRelevantIDsForPosition( xmlids, getView()->getPoint() );
                // UT_DEBUGMSG(("AP_UnixDialog_RDFEditor, have restricted xmlids size:%d\n", xmlids.size() ));
                // if( !xmlids.empty() )
                // {
                //     setRestrictedXMLID( xmlids.front() );
                // }
                
                g_signal_connect(G_OBJECT(m_selectedxmlid),
                                 "notify::selected",
                                 G_CALLBACK(s_OnXMLIDChanged),
                                 static_cast<gpointer>( this));
            }
            else xap_gtk_container_remove (GTK_WIDGET(gtk_builder_get_object(builder, "topvbox")),  m_restrictxmlidhidew);
        }
    }
    
    
    /////////////
	/// Signals
    ///
	g_signal_connect (GTK_BUTTON (m_btShowAll), "clicked", 
					  G_CALLBACK (AP_UnixDialog_RDFEditor__onShowAllClicked), static_cast <gpointer>(this));
	g_signal_connect (m_anewtriple, "activate", 
					  G_CALLBACK (AP_UnixDialog_RDFEditor__onActionNew), static_cast <gpointer>(this));
	g_signal_connect (m_acopytriple, "activate", 
					  G_CALLBACK (AP_UnixDialog_RDFEditor__onActionCopy), static_cast <gpointer>(this));
	g_signal_connect (m_adeletetriple, "activate", 
					  G_CALLBACK (AP_UnixDialog_RDFEditor__onActionDelete), static_cast <gpointer>(this));
	g_signal_connect (m_aimportrdfxml, "activate", 
					  G_CALLBACK (AP_UnixDialog_RDFEditor__onActionImportRDFXML), static_cast <gpointer>(this));
	g_signal_connect (m_aexportrdfxml, "activate", 
					  G_CALLBACK (AP_UnixDialog_RDFEditor__onActionExportRDFXML), static_cast <gpointer>(this));
    g_signal_connect (GTK_DIALOG (m_wDialog), "response",
					  G_CALLBACK (AP_UnixDialog_RDFEditor__onDialogResponse), static_cast <gpointer>(this));
	g_signal_connect (m_wDialog, "close-request",
					  G_CALLBACK (AP_UnixDialog_RDFEditor__onDeleteWindow), static_cast <gpointer>(this));
	g_signal_connect (gtk_column_view_get_model( m_resultsView ), "selection-changed",
					  G_CALLBACK (AP_UnixDialog_RDFEditor__onCursorChanged), static_cast <gpointer>(this));

#ifndef WITH_REDLAND
	g_simple_action_set_enabled(m_aimportrdfxml, FALSE);
	g_simple_action_set_enabled(m_aexportrdfxml, FALSE);
#endif

	g_object_unref(G_OBJECT(builder));
}

/*!
* Update dialog's data.
*/
void 
AP_UnixDialog_RDFEditor::_updateWindow ()
{
    UT_DEBUGMSG(("RDFEditor::_updateWindow()\n"));
	ConstructWindowName ();
	gtk_window_set_title (GTK_WINDOW (m_wDialog), m_WindowName.c_str() );
}

void 
AP_UnixDialog_RDFEditor::runModeless (XAP_Frame * pFrame)
{
	UT_DEBUGMSG (("MIQ: runModeless ()\n"));
	_constructWindow (pFrame);
	UT_ASSERT (m_wDialog);
	_updateWindow ();

	abiSetupModelessDialog (GTK_DIALOG (m_wDialog), pFrame, this, GTK_RESPONSE_CLOSE);
	showAllRDF();
	gtk_widget_set_visible(m_wDialog, TRUE);
	gtk_window_present (GTK_WINDOW (m_wDialog));
}

void 
AP_UnixDialog_RDFEditor::notifyActiveFrame (XAP_Frame * /*pFrame*/)
{
	UT_DEBUGMSG (("MIQ: notifyActiveFrame ()\n"));
	UT_ASSERT (m_wDialog);
	_updateWindow ();
}

void 
AP_UnixDialog_RDFEditor::activate (void)
{
	UT_ASSERT (m_wDialog);
	UT_DEBUGMSG (("MIQ: AP_UnixDialog_RDFEditor::activate ()\n"));
	_updateWindow ();
	gtk_window_present (GTK_WINDOW (m_wDialog));
}

void 
AP_UnixDialog_RDFEditor::destroy ()
{
	UT_DEBUGMSG (("MIQ: AP_UnixDialog_RDFEditor::destroy ()\n"));
	modeless_cleanup ();
	if (m_wDialog) {
		abiDestroyWidget(m_wDialog); // TOPLEVEL
		m_wDialog = nullptr;
	}
}

void
AP_UnixDialog_RDFEditor::removeStatement( const PD_RDFStatement& st )
{
    guint pos = findRowPos( st );
    if( pos == GTK_INVALID_LIST_POSITION )
        return;
    GtkSelectionModel* sel = gtk_column_view_get_model( m_resultsView );
    AbiRdfTripleRow* row = ABI_RDF_TRIPLE_ROW(
        g_list_model_get_item( G_LIST_MODEL( sel ), pos ) );
    guint storeidx = 0;
    if( row && g_list_store_find( m_resultsStore, row, &storeidx ) )
        g_list_store_remove( m_resultsStore, storeidx );
    if( row )
        g_object_unref( row );
}

std::list< PD_RDFStatement >
AP_UnixDialog_RDFEditor::getSelection()
{
    std::list< PD_RDFStatement > ret;
    if( !m_resultsView )
        return ret;
    GtkSelectionModel* sel = gtk_column_view_get_model( m_resultsView );
    GtkBitset* bitset = gtk_selection_model_get_selection( sel );
    guint nsel = gtk_bitset_get_size( bitset );
    for( guint i = 0; i < nsel; ++i )
    {
        guint pos = gtk_bitset_get_nth( bitset, i );
        AbiRdfTripleRow* row = ABI_RDF_TRIPLE_ROW(
            g_list_model_get_item( G_LIST_MODEL( sel ), pos ) );
        PD_RDFStatement st = rowToStatement( row );
        g_object_unref( row );
        ret.push_back( st );
        xxx_UT_DEBUGMSG(("getSelection() st: %s\n", st.toString().utf8_str()));
    }
    gtk_bitset_unref( bitset );

    return ret;
}

void
AP_UnixDialog_RDFEditor::onCursorChanged()
{
    xxx_UT_DEBUGMSG(("onCursorChanged()\n"));
    PD_URI pkg_idref("http://docs.oasis-open.org/opendocument/meta/package/common#idref");
    PD_DocumentRDFHandle rdf = getRDF();
    PD_RDFModelHandle  model = getModel();
    
    std::list< PD_RDFStatement > sl = getSelection();
    if( !sl.empty() )
    {
        for( std::list< PD_RDFStatement >::iterator siter = sl.begin();
             siter != sl.end(); ++siter )
        {
            xxx_UT_DEBUGMSG((" subj: %s\n", siter->getSubject().toString().utf8_string()));
            PD_ObjectList ul = model->getObjects( siter->getSubject(), pkg_idref );
            for( PD_ObjectList::iterator uiter = ul.begin(); uiter != ul.end(); ++uiter )
            {
                std::string xmlid = uiter->toString();
                
                xxx_UT_DEBUGMSG((" xmlid: %s\n", xmlid.c_str()));
                std::pair< PT_DocPosition, PT_DocPosition > range = rdf->getIDRange( xmlid );
                xxx_UT_DEBUGMSG((" start: %d end: %d\n", range.first, range.second));
                getView()->cmdSelect( range );
                
            }
        }
    }
}

