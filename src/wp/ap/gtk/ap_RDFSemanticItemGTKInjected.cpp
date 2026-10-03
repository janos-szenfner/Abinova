/* Abinova
 * Copyright (C) Ben Martin 2012.
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

#include "GTKCommon.h"
#include "ap_RDFSemanticItemGTKInjected.h"
#include "pd_Document.h"
#include "fv_View.h"
#include "xap_UnixDialogHelper.h"
#include "ap_Strings.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"
#include "xap_GtkComboBoxHelpers.h"

static gchar *s_id;

struct ssList_t
{
    const XAP_String_Id translation_id;  
    const char *stylesheet;  
};

struct combo_box_t
{
    const char *itemClass;
    const char *defaultStylesheet;
    const ssList_t *ssList;
    GtkWidget *combo_box; 
    int index; 
};

static const ssList_t ssListContact[] =
{
    {AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_CONTACT_NAME, RDF_SEMANTIC_STYLESHEET_CONTACT_NAME},
    {AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_CONTACT_NICK, RDF_SEMANTIC_STYLESHEET_CONTACT_NICK},    
    {AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_CONTACT_NAME_PHONE, RDF_SEMANTIC_STYLESHEET_CONTACT_NAME_PHONE},   
    {AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_CONTACT_NICK_PHONE, RDF_SEMANTIC_STYLESHEET_CONTACT_NICK_PHONE},   
    {AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_CONTACT_NAME_HOMEPAGE_PHONE, RDF_SEMANTIC_STYLESHEET_CONTACT_NAME_HOMEPAGE_PHONE},   
    {0, nullptr}
};

static const ssList_t ssListEvent[] =
{
    {AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_EVENT_NAME, RDF_SEMANTIC_STYLESHEET_EVENT_NAME},
    {AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_EVENT_SUMMARY, RDF_SEMANTIC_STYLESHEET_EVENT_SUMMARY},
    {AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_EVENT_SUMMARY_LOCATION, RDF_SEMANTIC_STYLESHEET_EVENT_SUMMARY_LOCATION},
    {AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_EVENT_SUMMARY_LOCATION_TIMES, RDF_SEMANTIC_STYLESHEET_EVENT_SUMMARY_LOCATION_TIMES},
    {AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_EVENT_SUMMARY_TIMES, RDF_SEMANTIC_STYLESHEET_EVENT_SUMMARY_TIMES},
    {0, nullptr}
};

static const ssList_t ssListLocation[] =
{
    {AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_LOCATION_NAME, RDF_SEMANTIC_STYLESHEET_LOCATION_NAME},
    {AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_LOCATION_NAME_LATLONG, RDF_SEMANTIC_STYLESHEET_LOCATION_NAME_LATLONG},
    {0, nullptr}
};

static combo_box_t combo_box_data[] =
{
    {"Contact", RDF_SEMANTIC_STYLESHEET_CONTACT_NAME, ssListContact, nullptr, 0},
    {"Event", RDF_SEMANTIC_STYLESHEET_EVENT_NAME, ssListEvent, nullptr, 0},
    {"Location", RDF_SEMANTIC_STYLESHEET_LOCATION_NAME, ssListLocation, nullptr, 0},
    {nullptr, nullptr, nullptr, nullptr, 0}
};

static const char *getStylesheetName( const ssList_t *ssList, const gchar *translation )
{
    const XAP_StringSet *pSS = XAP_App::getApp()->getStringSet();
    std::string text;
    int i;

    if (!translation) return nullptr;

    for (i = 0; ssList[i].stylesheet; i++)
    {
        pSS->getValueUTF8(ssList[i].translation_id, text);

        if (strcmp(translation, text.c_str()) == 0) break;
    }

    UT_DEBUGMSG(("getStylesheetName: in=\"%s\", out=\"%s\"\n", translation, ssList[i].stylesheet));

    return ssList[i].stylesheet;
}

void GDestroyNotify_GObjectSemItem(gpointer data)
{
    ap_GObjectSemItem* obj = static_cast<ap_GObjectSemItem*>(data);
    delete obj;
}

PD_RDFSemanticItemHandle getHandle(GtkDialog* d)
{
    ap_GObjectSemItem* data = static_cast<ap_GObjectSemItem*>(
        g_object_get_data( G_OBJECT(d), G_OBJECT_SEMITEM ));
    return data->h;
}

void OnSemItemEdited ( GtkDialog* d, gint /*response_id*/, 
					   gpointer /*user_data*/)
{
    UT_DEBUGMSG(("OnSemItemEdited()\n"));
    PD_RDFSemanticItemHandle h = getHandle( d );
    h->updateFromEditorData();
    abiDestroyWidget(GTK_WIDGET(d)); // TOPLEVEL
}



void GDestroyNotify_GObjectSemItem_List(gpointer data)
{
    ap_GObjectSemItem_List* obj = static_cast<ap_GObjectSemItem_List*>(data);
    delete obj;
}
PD_RDFSemanticItems getSemItemListHandle(GtkDialog* d)
{
    ap_GObjectSemItem_List* data = static_cast<ap_GObjectSemItem_List*>(
        g_object_get_data( G_OBJECT(d), G_OBJECT_SEMITEM_LIST ));
    return data->cl;
}
void OnSemItemListEdited ( GtkDialog* d, gint response_id, 
						   gpointer /*user_data*/)
{
    UT_DEBUGMSG(("OnSemItemListEdited() response_id:%d\n", response_id ));
    if( response_id != GTK_RESPONSE_DELETE_EVENT )
    {
        PD_RDFSemanticItems cl = getSemItemListHandle( d );
        for( PD_RDFSemanticItems::iterator ci = cl.begin(); ci != cl.end(); ++ci )
        {
            PD_RDFSemanticItemHandle c = *ci;
            c->updateFromEditorData();
        }
    }
    abiDestroyWidget(GTK_WIDGET(d)); // TOPLEVEL
}


/********************************************************************************/
/********************************************************************************/
/********************************************************************************/


static void
OnSemanticStylesheetsDialogResponse( GtkWidget* dialog,
                                     GtkListView* /*tree*/,
                                     FV_View* /*pView*/)
{
    abiDestroyWidget(dialog); // TOPLEVEL
}


static void
ApplySemanticStylesheets( const std::string& semItemClassRestriction,
                          const std::string& ssName, bool reflow )
{
    // set the RDF linking to the stylesheets
    std::list< AD_Document* > dl = XAP_App::getApp()->getDocuments();
    for( std::list< AD_Document* >::iterator diter = dl.begin(); diter != dl.end(); ++diter )
    {
        PD_Document* pDoc = dynamic_cast<PD_Document*>(*diter);
        pDoc->beginUserAtomicGlob();
        
        PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF();
        PD_RDFSemanticItems   sl = rdf->getAllSemanticObjects( semItemClassRestriction );

        for( PD_RDFSemanticItems::iterator siter = sl.begin(); siter != sl.end(); ++siter )
        {
            PD_RDFSemanticItemHandle si = *siter;
            PD_RDFSemanticStylesheetHandle ss = si->findStylesheetByName(
                PD_RDFSemanticStylesheet::stylesheetTypeSystem(), ssName );

            std::set< std::string > xmlids = si->getXMLIDs();
            for( std::set< std::string >::iterator xiter = xmlids.begin(); xiter != xmlids.end(); ++xiter )
            {
                std::string xmlid = *xiter;
                PD_RDFSemanticItemViewSite vs( si, xmlid );
                vs.setStylesheetWithoutReflow( ss );
            }
        }
        pDoc->endUserAtomicGlob();
    }

    if (reflow)
    {
        UT_DEBUGMSG(("ApplySemanticStylesheets(reflowing)\n" ));

        // reflow all the viewsites
        for( std::list< AD_Document* >::iterator diter = dl.begin(); diter != dl.end(); ++diter )
        {
            PD_Document* pDoc = dynamic_cast<PD_Document*>(*diter);
            pDoc->beginUserAtomicGlob();
            pDoc->notifyPieceTableChangeStart();
            pDoc->setDontImmediatelyLayout(true);
            
            PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF();
            PD_RDFSemanticItems   sl = rdf->getAllSemanticObjects( semItemClassRestriction );

            std::list<AV_View*> vl = pDoc->getAllViews();
            for( std::list<AV_View*>::iterator viter = vl.begin(); viter != vl.end(); ++viter )
            {
                FV_View* pView = dynamic_cast<FV_View*>(*viter);

                for( PD_RDFSemanticItems::iterator siter = sl.begin(); siter != sl.end(); ++siter )
                {
                    PD_RDFSemanticItemHandle si = *siter;
                    std::set< std::string > xmlids = si->getXMLIDs();
                    for( std::set< std::string >::iterator xiter = xmlids.begin(); xiter != xmlids.end(); ++xiter )
                    {
                        std::string xmlid = *xiter;
                        PD_RDFSemanticItemViewSite vs( si, xmlid );
                        vs.reflowUsingCurrentStylesheet( pView );
                    }
                }
                break;
            }
            
            pDoc->setDontImmediatelyLayout(false);
            pDoc->notifyPieceTableChangeEnd();
            pDoc->endUserAtomicGlob();
        }
    }

    UT_DEBUGMSG(("ApplySemanticStylesheets(done)\n" ));
}


static void
OnSemanticStylesheetsSet_cb (GtkWidget *, combo_box_t *box)
{
    std::string sel = XAP_dropDownGetSelectedText(GTK_DROP_DOWN(box->combo_box));
    const char *t = getStylesheetName(box->ssList, sel.empty() ? nullptr : sel.c_str());
    std::string ssName = t ? t : box->defaultStylesheet;

    UT_DEBUGMSG(("OnSemanticStylesheetsSet_cb() combo:%p\n", box->combo_box));
    UT_DEBUGMSG(("OnSemanticStylesheetsSet_cb() t:%s\n", t));
    UT_DEBUGMSG(("OnSemanticStylesheetsSet_cb() ssName:%s\n", ssName.c_str()));

    ApplySemanticStylesheets(box->itemClass, ssName, true);
}

static void
OnSemanticStylesheetsOk_cb (GtkWidget *widget, combo_box_t *box)
{
    UT_UNUSED(widget);

    for (int i = 0; box[i].itemClass; i++)
    {
        const char *t;
        std::string ssName;

        box[i].index = static_cast<int>(gtk_drop_down_get_selected(GTK_DROP_DOWN(box[i].combo_box)));

        std::string sel = XAP_dropDownGetSelectedText(GTK_DROP_DOWN(box[i].combo_box));
        t = getStylesheetName(box[i].ssList, sel.empty() ? nullptr : sel.c_str());
        ssName = t ? t : box[i].defaultStylesheet;

        UT_DEBUGMSG(("OnSemanticStylesheetsOk_cb() combo:%p\n", box[i].combo_box));
        UT_DEBUGMSG(("OnSemanticStylesheetsOk_cb() t:%s\n", t));
        UT_DEBUGMSG(("OnSemanticStylesheetsOk_cb() ssName:%s\n", ssName.c_str()));

        ApplySemanticStylesheets(box[i].itemClass, ssName, false);
    }
}

/******************************/
/******************************/
/******************************/

/* Row object for the insert-reference GtkTreeListModel: a name plus an
 * optional child model (only the "Contacts" heading has children). */
#define ABI_TYPE_RDF_REF_ITEM (abi_rdf_ref_item_get_type())
G_DECLARE_FINAL_TYPE (AbiRdfRefItem, abi_rdf_ref_item, ABI, RDF_REF_ITEM, GObject)

struct _AbiRdfRefItem
{
	GObject parent_instance;
	gchar *name;
	GListModel *children;
};

G_DEFINE_TYPE (AbiRdfRefItem, abi_rdf_ref_item, G_TYPE_OBJECT)

static void
abi_rdf_ref_item_init (AbiRdfRefItem * /*self*/)
{
}

static void
abi_rdf_ref_item_finalize (GObject *object)
{
	AbiRdfRefItem *item = ABI_RDF_REF_ITEM (object);
	g_free (item->name);
	g_clear_object (&item->children);
	G_OBJECT_CLASS (abi_rdf_ref_item_parent_class)->finalize (object);
}

static void
abi_rdf_ref_item_class_init (AbiRdfRefItemClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = abi_rdf_ref_item_finalize;
}

/* takes over the children reference */
static AbiRdfRefItem *
abi_rdf_ref_item_new (const gchar *name, GListModel *children)
{
	AbiRdfRefItem *item =
		ABI_RDF_REF_ITEM (g_object_new (ABI_TYPE_RDF_REF_ITEM, nullptr));
	item->name = g_strdup (name);
	item->children = children;
	return item;
}

static GListModel *
s_ref_create_model (gpointer item, gpointer /*data*/)
{
	AbiRdfRefItem *it = ABI_RDF_REF_ITEM (item);
	return it->children ? G_LIST_MODEL (g_object_ref (it->children))
						: nullptr;
}

static void
s_ref_setup (GtkSignalListItemFactory * /*factory*/,
			 GtkListItem *item,
			 gpointer /*data*/)
{
	GtkWidget *expander = gtk_tree_expander_new ();
	GtkWidget *label = gtk_label_new (nullptr);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
	gtk_tree_expander_set_child (GTK_TREE_EXPANDER (expander), label);
	gtk_list_item_set_child (item, expander);
}

static void
s_ref_bind (GtkSignalListItemFactory * /*factory*/,
			GtkListItem *item,
			gpointer /*data*/)
{
	GtkTreeListRow *row =
		GTK_TREE_LIST_ROW (gtk_list_item_get_item (item));
	GtkTreeExpander *expander =
		GTK_TREE_EXPANDER (gtk_list_item_get_child (item));
	gtk_tree_expander_set_list_row (expander, row);
	AbiRdfRefItem *it =
		ABI_RDF_REF_ITEM (gtk_tree_list_row_get_item (row));
	gtk_label_set_text (GTK_LABEL (gtk_tree_expander_get_child (expander)),
						it->name);
	/* the "Contacts" heading is display-only */
	gboolean isRef = gtk_tree_list_row_get_depth (row) > 0;
	gtk_list_item_set_selectable (item, isRef);
	gtk_list_item_set_activatable (item, isRef);
	g_object_unref (it);
}

/* name of the AbiRdfRefItem at flat position pos, or "" */
static std::string
s_ref_name_at (GtkListView* tv, guint pos)
{
	GtkSelectionModel *sel = gtk_list_view_get_model (tv);
	GListModel *model =
		gtk_single_selection_get_model (GTK_SINGLE_SELECTION (sel));
	GtkTreeListRow *row =
		GTK_TREE_LIST_ROW (g_list_model_get_item (model, pos));
	std::string n;
	if (row)
	{
		AbiRdfRefItem *it =
			ABI_RDF_REF_ITEM (gtk_tree_list_row_get_item (row));
		if (it)
		{
			if (gtk_tree_list_row_get_depth (row) > 0 && it->name)
				n = it->name;
			g_object_unref (it);
		}
		g_object_unref (row);
	}
	return n;
}

/* name of the selected AbiRdfRefItem, or "" */
static std::string
s_ref_selected_name (GtkListView* tv)
{
	GtkSelectionModel *sel = gtk_list_view_get_model (tv);
	gpointer item = gtk_single_selection_get_selected_item (
		GTK_SINGLE_SELECTION (sel));
	std::string n;
	if (item && GTK_IS_TREE_LIST_ROW (item))
	{
		AbiRdfRefItem *it = ABI_RDF_REF_ITEM (
			gtk_tree_list_row_get_item (GTK_TREE_LIST_ROW (item)));
		if (it)
		{
			if (gtk_tree_list_row_get_depth (GTK_TREE_LIST_ROW (item)) > 0 &&
				it->name)
				n = it->name;
			g_object_unref (it);
		}
	}
	return n;
}

static void
OnInsertReferenceName( GtkWidget* dialog,
					   const std::string& n,
					   FV_View* pView )
{
    PD_Document* pDoc = pView->getDocument();
    PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF();

    UT_DEBUGMSG(("clicked on: %s\n", n.c_str() ));

    bool found = false;
    PD_RDFContacts clist = rdf->getContacts();
    for( PD_RDFContacts::iterator ci = clist.begin(); ci != clist.end(); ++ci )
    {
        PD_RDFContactHandle obj = *ci;
        if( obj->name() == n )
        {
            obj->insert( pView );
            found = true;
            break;
        }
    }
    if( found )
        abiDestroyWidget(dialog); // TOPLEVEL

}

static void OnInsertReference( GtkDialog* d, gint /*response_id*/, gpointer user_data)
{
    UT_DEBUGMSG(("OnInsertReference()\n"));
    FV_View* pView = static_cast<FV_View*>(user_data);

    GtkListView* tv = GTK_LIST_VIEW( g_object_get_data( G_OBJECT(d), G_OBJECT_TREEVIEW ));
    OnInsertReferenceName( GTK_WIDGET(d), s_ref_selected_name( tv ), pView );
}

static void
OnInsertReferenceDblClicked( GtkListView      * tree,
                             guint              pos,
                             gpointer		    user_data )
{
    FV_View* pView = static_cast<FV_View*>(user_data);

    GtkWidget* d = GTK_WIDGET(g_object_get_data( G_OBJECT(tree), G_OBJECT_WINDOW ));
    OnInsertReferenceName( d, s_ref_name_at( tree, pos ), pView );
}


/******************************/
/******************************/
/******************************/

class PD_RDFDialogsGTK : public PD_RDFDialogs
{
private:
    void _setIcon (GtkWidget *window)
    {
        XAP_Frame *lff = XAP_App::getApp()->getLastFocussedFrame();
        XAP_UnixFrameImpl *pUnixFrameImpl = static_cast<XAP_UnixFrameImpl *>(lff->getFrameImpl());
        GtkWidget *top = pUnixFrameImpl->getTopLevelWindow();
        const char *icon = gtk_window_get_icon_name(GTK_WINDOW(top));
        if (icon) gtk_window_set_icon_name(GTK_WINDOW(window), icon);
    }
public:
    PD_RDFDialogsGTK()
    {
        PD_DocumentRDF::setRDFDialogs( this );
    }
    ~PD_RDFDialogsGTK()
    {
        g_free(s_id);
    }
    virtual void runSemanticStylesheetsDialog(FV_View* pView) override
    {
        const XAP_StringSet *pSS = XAP_App::getApp()->getStringSet();
        std::string text;

        GtkBuilder* builder   = newDialogBuilderFromResource("ap_UnixDialog_SemanticStylesheets.ui");
        GtkWidget*  window    = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
        GtkWidget*  lbExplanation = GTK_WIDGET(gtk_builder_get_object(builder, "lbExplanation"));         
        combo_box_data[0].combo_box = GTK_WIDGET(gtk_builder_get_object(builder, "contacts"));
        combo_box_data[1].combo_box = GTK_WIDGET(gtk_builder_get_object(builder, "events"));
        combo_box_data[2].combo_box = GTK_WIDGET(gtk_builder_get_object(builder, "locations"));
        XAP_makeGtkDropDown(GTK_DROP_DOWN(combo_box_data[0].combo_box));
        XAP_makeGtkDropDown(GTK_DROP_DOWN(combo_box_data[1].combo_box));
        XAP_makeGtkDropDown(GTK_DROP_DOWN(combo_box_data[2].combo_box));
        GtkWidget*  setContacts  = GTK_WIDGET(gtk_builder_get_object(builder, "setContacts"));
        GtkWidget*  setEvents    = GTK_WIDGET(gtk_builder_get_object(builder, "setEvents"));
        GtkWidget*  setLocations = GTK_WIDGET(gtk_builder_get_object(builder, "setLocations"));
        GtkWidget*  setAll       = GTK_WIDGET(gtk_builder_get_object(builder, "setAll"));

        // localization

        pSS->getValueUTF8(AP_STRING_ID_DLG_RDF_SemanticStylesheets_Explanation, text);
        text += "\xe2\x80\xa9";     // paragraph separator 
        gtk_label_set_text(GTK_LABEL(lbExplanation), text.c_str());
        localizeLabel(GTK_WIDGET(gtk_builder_get_object(builder, "lbContacts")), pSS, AP_STRING_ID_DLG_RDF_SemanticStylesheets_Contacts);
        localizeLabel(GTK_WIDGET(gtk_builder_get_object(builder, "lbEvents")), pSS, AP_STRING_ID_DLG_RDF_SemanticStylesheets_Events);
        localizeLabel(GTK_WIDGET(gtk_builder_get_object(builder, "lbLocations")), pSS, AP_STRING_ID_DLG_RDF_SemanticStylesheets_Locations);
        localizeButton(setContacts, pSS, AP_STRING_ID_DLG_RDF_SemanticStylesheets_Set);        
        localizeButton(setEvents, pSS, AP_STRING_ID_DLG_RDF_SemanticStylesheets_Set);        
        localizeButton(setLocations, pSS, AP_STRING_ID_DLG_RDF_SemanticStylesheets_Set);        
        localizeButton(setAll, pSS, AP_STRING_ID_DLG_RDF_SemanticStylesheets_Set);        
        // drop-downs
        for (int i = 0; ssListContact[i].stylesheet; i++)
        {
            pSS->getValueUTF8(ssListContact[i].translation_id, text);
            XAP_appendDropDownText(GTK_DROP_DOWN(combo_box_data[0].combo_box), text.c_str());
        }
        for (int i = 0; ssListEvent[i].stylesheet; i++)
        {
            pSS->getValueUTF8(ssListEvent[i].translation_id, text);
            XAP_appendDropDownText(GTK_DROP_DOWN(combo_box_data[1].combo_box), text.c_str());
        }
        for (int i = 0; ssListLocation[i].stylesheet; i++)
        {
            pSS->getValueUTF8(ssListLocation[i].translation_id, text);
            XAP_appendDropDownText(GTK_DROP_DOWN(combo_box_data[2].combo_box), text.c_str());
        }
        gtk_drop_down_set_selected(GTK_DROP_DOWN(combo_box_data[0].combo_box), combo_box_data[0].index);
        gtk_drop_down_set_selected(GTK_DROP_DOWN(combo_box_data[1].combo_box), combo_box_data[1].index);
        gtk_drop_down_set_selected(GTK_DROP_DOWN(combo_box_data[2].combo_box), combo_box_data[2].index);

        // set max. text width for explanation
        GtkRequisition requisition;
        gtk_widget_get_preferred_size(gtk_widget_get_parent(lbExplanation), &requisition, nullptr);
        gtk_widget_set_size_request(lbExplanation, requisition.width, -1);

        // window title and icon
        pSS->getValueUTF8(AP_STRING_ID_DLG_RDF_SemanticStylesheets_Title, text);
        gtk_window_set_title(GTK_WINDOW(window), text.c_str());
        _setIcon(window);

        g_signal_connect (setContacts,  "clicked", G_CALLBACK (OnSemanticStylesheetsSet_cb),  &combo_box_data[0] );
        g_signal_connect (setEvents,    "clicked", G_CALLBACK (OnSemanticStylesheetsSet_cb),    &combo_box_data[1] );
        g_signal_connect (setLocations, "clicked", G_CALLBACK (OnSemanticStylesheetsSet_cb), &combo_box_data[2] );

        g_signal_connect (setAll, "clicked", G_CALLBACK (OnSemanticStylesheetsSet_cb),  &combo_box_data[0] );
        g_signal_connect (setAll, "clicked", G_CALLBACK (OnSemanticStylesheetsSet_cb),    &combo_box_data[1] );
        g_signal_connect (setAll, "clicked", G_CALLBACK (OnSemanticStylesheetsSet_cb), &combo_box_data[2] );

        g_signal_connect(GTK_WIDGET(gtk_builder_get_object(builder, "OK")), "clicked", G_CALLBACK(OnSemanticStylesheetsOk_cb), combo_box_data);

        g_signal_connect (G_OBJECT(window), "response",  G_CALLBACK(OnSemanticStylesheetsDialogResponse), pView );
        gtk_widget_set_visible(window, TRUE);
        g_object_unref (builder);

    }
    std::pair<PT_DocPosition, PT_DocPosition> runInsertReferenceDialog(FV_View* pView) override
    {
        const XAP_StringSet *pSS = XAP_App::getApp()->getStringSet();
        std::string text;

        GtkBuilder* builder = newDialogBuilderFromResource("pd_RDFInsertReference.ui");
        GtkWidget*  window  = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
        GtkWidget*  tv      = GTK_WIDGET(gtk_builder_get_object(builder, "tv"));

        // localization
        GtkWidget *ok = GTK_WIDGET(gtk_builder_get_object(builder, "ok"));
        localizeButton(ok, pSS, AP_STRING_ID_DLG_RDF_SemanticItemInsert_Ok);

        // window title and icon
        pSS->getValueUTF8(AP_STRING_ID_DLG_RDF_SemanticItemInsert_Title, text);
        gtk_window_set_title(GTK_WINDOW(window), text.c_str());
        _setIcon(window);

        PD_Document* pDoc = pView->getDocument();
        PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF();

        PD_RDFContacts l = rdf->getContacts();

        /* root holds a single "Contacts" heading whose child model
         * contains one row per contact */
        GListStore *root = g_list_store_new (ABI_TYPE_RDF_REF_ITEM);
        GListStore *contacts = g_list_store_new (ABI_TYPE_RDF_REF_ITEM);
        for( PD_RDFContacts::iterator iter = l.begin(); iter != l.end(); ++iter )
        {
            PD_RDFContactHandle c = *iter;
            AbiRdfRefItem *it = abi_rdf_ref_item_new (c->name().c_str(),
                                                    nullptr);
            g_list_store_append (contacts, it);
            g_object_unref (it);
        }
        if (l.begin() != l.end())
        {
            pSS->getValueUTF8(AP_STRING_ID_DLG_RDF_SemanticItemInsert_Column_Refdlg, text);
            AbiRdfRefItem *heading = abi_rdf_ref_item_new (
                text.c_str(), G_LIST_MODEL (contacts));
            g_list_store_append (root, heading);
            g_object_unref (heading);
        }
        else
        {
            g_object_unref (contacts);
        }

        /* the GTK4 model-wrapper ctors are transfer-full: ref what we
         * still use below */
        GtkTreeListModel *treemodel =
            gtk_tree_list_model_new (G_LIST_MODEL (g_object_ref (root)),
                                     FALSE, FALSE,
                                     s_ref_create_model, nullptr, nullptr);
        GtkSingleSelection *sel = gtk_single_selection_new (
            G_LIST_MODEL (g_object_ref (treemodel)));
        gtk_single_selection_set_autoselect (sel, FALSE);
        gtk_single_selection_set_can_unselect (sel, FALSE);

        GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
        g_signal_connect (factory, "setup", G_CALLBACK (s_ref_setup), nullptr);
        g_signal_connect (factory, "bind", G_CALLBACK (s_ref_bind), nullptr);

        gtk_list_view_set_model (GTK_LIST_VIEW (tv), GTK_SELECTION_MODEL (sel));
        gtk_list_view_set_factory (GTK_LIST_VIEW (tv), factory);
        g_object_unref (factory);
        g_object_unref (sel);

        /* expand the Contacts heading, like the old expand_all */
        guint n = g_list_model_get_n_items (G_LIST_MODEL (treemodel));
        for( guint i = 0; i < n; ++i )
        {
            GtkTreeListRow *row = GTK_TREE_LIST_ROW (
                g_list_model_get_item (G_LIST_MODEL (treemodel), i));
            if( row && gtk_tree_list_row_get_children (row) )
                gtk_tree_list_row_set_expanded (row, TRUE);
            if( row )
                g_object_unref (row);
        }
        g_object_unref (treemodel);
        g_object_unref (root);

        g_object_set_data( G_OBJECT(tv),     G_OBJECT_WINDOW,   window );
        g_object_set_data( G_OBJECT(window), G_OBJECT_TREEVIEW, tv );

        g_signal_connect (GTK_LIST_VIEW (tv), "activate",
                          G_CALLBACK (OnInsertReferenceDblClicked), static_cast <gpointer>(pView));
        g_signal_connect (G_OBJECT(window), "response",  G_CALLBACK(OnInsertReference), pView );
        gtk_widget_set_visible(window, TRUE);
        g_object_unref (builder);

        std::pair< PT_DocPosition, PT_DocPosition > ret;
        return ret;
    }
    
};

namespace 
{
    PD_RDFDialogsGTK __obj;
};
