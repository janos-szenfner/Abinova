/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:t -*- */
/* Abinova
 * Copyright (C) 2003 Dom Lachowicz
 * Copyright (C) 2004 Martin Sevior
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

#include "ut_string.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "pt_PieceTable.h"

#include "xap_UnixDialogHelper.h"

#include "xap_App.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"

#include "ap_Strings.h"
#include "ap_Dialog_Id.h"
#include "ap_UnixDialog_Stylist.h"

#include <algorithm>
#include <string>
#include <vector>

/* Item objects for the style list model: depth-0 items are the style
 * categories ("Heading styles" etc.) and carry their child styles in a
 * GListModel; depth-1 items are the styles themselves and store the XP
 * (row, col) coordinates that styleClicked expects. */
#define ABI_TYPE_STYLE_ITEM (abi_style_item_get_type ())
G_DECLARE_FINAL_TYPE (AbiStyleItem, abi_style_item, ABI, STYLE_ITEM, GObject)

struct _AbiStyleItem
{
	GObject parent_instance;
	gchar *name;            /* localized display name */
	gint row;
	gint col;
	GListModel *children;   /* GListStore of AbiStyleItem, or NULL */
};

G_DEFINE_TYPE (AbiStyleItem, abi_style_item, G_TYPE_OBJECT)

static void
abi_style_item_init (AbiStyleItem * /*self*/)
{
}

static void
abi_style_item_finalize (GObject *object)
{
	AbiStyleItem *item = ABI_STYLE_ITEM (object);
	g_free (item->name);
	g_clear_object (&item->children);
	G_OBJECT_CLASS (abi_style_item_parent_class)->finalize (object);
}

static void
abi_style_item_class_init (AbiStyleItemClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = abi_style_item_finalize;
}

/* takes over the children reference */
static AbiStyleItem *
abi_style_item_new (const gchar *name, gint row, gint col,
					GListModel *children)
{
	AbiStyleItem *item =
		ABI_STYLE_ITEM (g_object_new (ABI_TYPE_STYLE_ITEM, nullptr));
	item->name = g_strdup (name);
	item->row = row;
	item->col = col;
	item->children = children;
	return item;
}

static GListModel *
s_style_create_model (gpointer item, gpointer /*data*/)
{
	AbiStyleItem *it = ABI_STYLE_ITEM (item);
	return it->children ? G_LIST_MODEL (g_object_ref (it->children))
						: nullptr;
}

static void
s_style_setup (GtkSignalListItemFactory * /*factory*/,
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
s_style_bind (GtkSignalListItemFactory * /*factory*/,
			  GtkListItem *item,
			  gpointer /*data*/)
{
	GtkTreeListRow *row =
		GTK_TREE_LIST_ROW (gtk_list_item_get_item (item));
	GtkTreeExpander *expander =
		GTK_TREE_EXPANDER (gtk_list_item_get_child (item));
	gtk_tree_expander_set_list_row (expander, row);
	AbiStyleItem *it =
		ABI_STYLE_ITEM (gtk_tree_list_row_get_item (row));
	gtk_label_set_text (GTK_LABEL (gtk_tree_expander_get_child (expander)),
						it->name);
	/* category headings are display-only, like the old select filter */
	gboolean isStyle = gtk_tree_list_row_get_depth (row) > 0;
	gtk_list_item_set_selectable (item, isStyle);
	gtk_list_item_set_activatable (item, isStyle);
	g_object_unref (it);
}

static void
s_types_clicked (GtkSingleSelection *selection,
				 GParamSpec * /*pspec*/,
				 AP_UnixDialog_Stylist *dlg)
{
	UT_ASSERT (selection && dlg);

	gpointer item = gtk_single_selection_get_selected_item (selection);
	if (!item || !GTK_IS_TREE_LIST_ROW (item))
		return;

	AbiStyleItem *it = ABI_STYLE_ITEM (
		gtk_tree_list_row_get_item (GTK_TREE_LIST_ROW (item)));
	if (it)
	{
		dlg->styleClicked (it->row, it->col);
		g_object_unref (it);
	}
}

static void s_types_dblclicked(GtkListView *listview,
							   guint pos,
							   AP_UnixDialog_Stylist * me)
{
	GtkSelectionModel *selection = gtk_list_view_get_model (listview);
	GListModel *model =
		gtk_single_selection_get_model (GTK_SINGLE_SELECTION (selection));
	GtkTreeListRow *row =
		GTK_TREE_LIST_ROW (g_list_model_get_item (model, pos));
	AbiStyleItem *it =
		ABI_STYLE_ITEM (gtk_tree_list_row_get_item (row));

	// simulate the effects of a single click
	if (it)
	{
		me->styleClicked (it->row, it->col);
		g_object_unref (it);
		me->event_Apply ();
	}
	g_object_unref (row);
}

static gboolean s_destroy_clicked (GtkWidget * /*wid*/, AP_UnixDialog_Stylist * me )
{
   me->event_Close();
	return TRUE;
}

static void s_response_triggered(GtkWidget * widget, gint resp, AP_UnixDialog_Stylist * dlg)
{
	UT_return_if_fail(widget && dlg);
	
	if ( resp == GTK_RESPONSE_APPLY )
	  dlg->event_Apply();
	else if ( resp == GTK_RESPONSE_CLOSE ) {
	  if (dlg->isRunning())
	    dlg->event_Close(); // modeless: full destroy() cleanup
	  else
	    abiDestroyWidget(widget);
	}
}

XAP_Dialog * AP_UnixDialog_Stylist::static_constructor(XAP_DialogFactory * pFactory,
														  XAP_Dialog_Id id)
{
	return new AP_UnixDialog_Stylist(pFactory,id);
}

AP_UnixDialog_Stylist::AP_UnixDialog_Stylist(XAP_DialogFactory * pDlgFactory,
												   XAP_Dialog_Id id)
	: AP_Dialog_Stylist(pDlgFactory,id),
	  m_wStyleList(nullptr),
	  m_wStyleListContainer(nullptr)
{
}

AP_UnixDialog_Stylist::~AP_UnixDialog_Stylist(void)
{
}

void AP_UnixDialog_Stylist::event_Close(void)
{
	destroy();
}

void AP_UnixDialog_Stylist::setStyleInGUI(void)
{
	std::string sLocCurStyle;
	std::string sCurStyle = getCurStyle();

	if((getStyleTree() == nullptr) || (sCurStyle.size() == 0))
		updateDialog();

	if(m_wStyleList == nullptr)
		return;

	if(isStyleTreeChanged())
		_fillTree();

	pt_PieceTable::s_getLocalisedStyleName(sCurStyle.c_str(), sLocCurStyle);

	GtkSelectionModel *selection =
		gtk_list_view_get_model(GTK_LIST_VIEW(m_wStyleList));
	GListModel *model =
		gtk_single_selection_get_model(GTK_SINGLE_SELECTION(selection));

	// find the matching style item inside its (still collapsed)
	// category's child model
	GtkTreeListRow *parentRow = nullptr;
	AbiStyleItem *target = nullptr;
	guint n = g_list_model_get_n_items(model);
	for (guint i = 0; i < n && !target; i++)
	{
		GtkTreeListRow *row =
			GTK_TREE_LIST_ROW(g_list_model_get_item(model, i));
		if (!row)
			break;
		if (gtk_tree_list_row_get_depth(row) == 0)
		{
			GListModel *children = gtk_tree_list_row_get_children(row);
			if (children)
			{
				guint cn = g_list_model_get_n_items(children);
				for (guint j = 0; j < cn; j++)
				{
					AbiStyleItem *it = ABI_STYLE_ITEM(
						g_list_model_get_item(children, j));
					bool match = it && it->name &&
						sLocCurStyle == it->name;
					if (match)
					{
						target = it; // keep the ref
						parentRow = row;
						break;
					}
					if (it)
						g_object_unref(it);
				}
			}
		}
		if (row != parentRow)
			g_object_unref(row);
	}

	if (target)
	{
		gtk_tree_list_row_set_expanded(parentRow, TRUE);
		g_object_unref(parentRow);

		// the children slot in right after their parent row; find the
		// flat position of the matching item
		guint pos = GTK_INVALID_LIST_POSITION;
		guint m = g_list_model_get_n_items(model);
		for (guint k = 0; k < m; k++)
		{
			GtkTreeListRow *row =
				GTK_TREE_LIST_ROW(g_list_model_get_item(model, k));
			if (!row)
				break;
			gpointer item = gtk_tree_list_row_get_item(row);
			bool found = (item == target);
			if (item)
				g_object_unref(item);
			g_object_unref(row);
			if (found)
			{
				pos = k;
				break;
			}
		}
		g_object_unref(target);

		if (pos != GTK_INVALID_LIST_POSITION)
		{
			gtk_list_view_scroll_to(GTK_LIST_VIEW(m_wStyleList), pos,
									static_cast<GtkListScrollFlags>(
										GTK_LIST_SCROLL_FOCUS |
										GTK_LIST_SCROLL_SELECT),
									nullptr);
		}
	}
	setStyleChanged(false);
}

void AP_UnixDialog_Stylist::destroy(void)
{
	finalize();
	abiDestroyWidget(m_windowMain); // TOPLEVEL
	m_windowMain = nullptr;
	m_wStyleList = nullptr;
}

void AP_UnixDialog_Stylist::activate(void)
{
	UT_ASSERT (m_windowMain);
	XAP_gtk_window_raise(m_windowMain);
}

void AP_UnixDialog_Stylist::notifyActiveFrame(XAP_Frame * /*pFrame*/)
{
    UT_ASSERT(m_windowMain);
}

/*!
 * Set the style in the XP layer from the selection in the GUI.
 */
void AP_UnixDialog_Stylist::styleClicked(UT_sint32 row, UT_sint32 col)
{
	std::string sStyle;
	UT_DEBUGMSG(("row %d col %d clicked \n",row,col));

	if((col == 0) && (getStyleTree()->getNumCols(row) == 1))
		return;
	else if(col == 0)
		getStyleTree()->getStyleAtRowCol(sStyle,row,col);
	else
		getStyleTree()->getStyleAtRowCol(sStyle,row,col-1);

	UT_DEBUGMSG(("StyleClicked row %d col %d style %s \n",row,col,sStyle.c_str()));
	setCurStyle(sStyle);
}

void AP_UnixDialog_Stylist::runModeless(XAP_Frame * pFrame)
{
	// Build the window's widgets and arrange them
	GtkWidget * mainWindow = _constructWindow();
	UT_return_if_fail(mainWindow);

	// Populate the window's data items
	_populateWindowData();
	_connectSignals();
	abiSetupModelessDialog(GTK_DIALOG(mainWindow),pFrame,this,GTK_RESPONSE_CLOSE);
	startUpdater();
}


void AP_UnixDialog_Stylist::runModal(XAP_Frame * pFrame)
{
	// Build the window's widgets and arrange them
	m_bIsModal = true;
	GtkWidget * mainWindow = _constructWindow();
	UT_return_if_fail(mainWindow);

	// Populate the window's data items
	_populateWindowData();
	_connectSignals();

	switch (abiRunModalDialog ( GTK_DIALOG(mainWindow), pFrame, this, GTK_RESPONSE_CLOSE,false ))
	{
	case GTK_RESPONSE_CLOSE:
		setStyleValid(false);
		break;
	case GTK_RESPONSE_OK:
		setStyleValid(true);
		break;
	default:
		setStyleValid(false);
		break;
	}
	abiDestroyWidget(mainWindow);
}


GtkWidget * AP_UnixDialog_Stylist::_constructWindow(void)
{
	// load the dialog from the UI file
	GtkBuilder* builder = newDialogBuilderFromResource("ap_UnixDialog_Stylist.ui");

	const XAP_StringSet * pSS = m_pApp->getStringSet ();

	m_windowMain   = GTK_WIDGET(gtk_builder_get_object(builder, "ap_UnixDialog_Stylist"));
	m_wStyleListContainer  = GTK_WIDGET(gtk_builder_get_object(builder, "TreeViewContainer"));

	if(m_bIsModal)
	{
		std::string label;
		pSS->getValueUTF8(XAP_STRING_ID_DLG_OK, label);
		/*button =*/ abiAddButton(GTK_DIALOG(m_windowMain), label, GTK_RESPONSE_OK);
	}
	else
	{
		std::string label;
		pSS->getValueUTF8(AP_STRING_ID_DLG_ApplyButton, label);
		/*button =*/ abiAddButton(GTK_DIALOG(m_windowMain), label, GTK_RESPONSE_APPLY);
	}

	// set the dialog title
	std::string s;
	pSS->getValueUTF8(AP_STRING_ID_DLG_Stylist_Title,s);
	abiDialogSetTitle(m_windowMain, "%s", s.c_str());

	g_object_unref(G_OBJECT(builder));
	
	return m_windowMain;
}

void  AP_UnixDialog_Stylist::event_Apply(void)
{
	Apply();
}

/*!
 * Fill the GUI tree with the styles as defined in the XP tree.
 */
void  AP_UnixDialog_Stylist::_fillTree(void)
{
	Stylist_tree * pStyleTree = getStyleTree();
	if (pStyleTree == nullptr)
	{
		updateDialog();
		pStyleTree = getStyleTree();
	}
	if (pStyleTree->getNumRows() == 0)
	{
		updateDialog();
		pStyleTree = getStyleTree();
	}
	UT_DEBUGMSG(("Number of rows of styles in document %d \n", pStyleTree->getNumRows()));

	GListStore *root = g_list_store_new (ABI_TYPE_STYLE_ITEM);
	UT_sint32 row, col;

	std::string sTmp, sLoc;
	for (row = 0; row < pStyleTree->getNumRows(); row++)
	{
		if (!pStyleTree->getNameOfRow(sTmp, row))
		{
			UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
			break;
		}
		if (pStyleTree->getNumCols(row) > 0)
		{
			xxx_UT_DEBUGMSG(("Adding Heading %s at row %d \n", sTmp.c_str(), row));

			/* children are shown sorted by their localized name, like
			 * the old tree-sortable collation; categories keep their
			 * insertion order */
			struct StyleEntry { std::string label; gint col; };
			std::vector<StyleEntry> styles;
			for (col = 0; col < pStyleTree->getNumCols(row); col++)
			{
				std::string style;
				if (!pStyleTree->getStyleAtRowCol(style, row, col))
				{
					UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
					break;
				}
				pt_PieceTable::s_getLocalisedStyleName(style.c_str(), sLoc);
				xxx_UT_DEBUGMSG(("Adding style %s at row %d col %d \n", sLoc.c_str(), row, col + 1));
				styles.push_back({sLoc, col + 1});
			}
			std::sort(styles.begin(), styles.end(),
					  [](const StyleEntry &a, const StyleEntry &b) {
						  return g_utf8_collate(a.label.c_str(),
												b.label.c_str()) < 0;
					  });
			GListStore *children = g_list_store_new (ABI_TYPE_STYLE_ITEM);
			for (const StyleEntry &st : styles)
			{
				AbiStyleItem *it = abi_style_item_new (st.label.c_str(),
													 row, st.col, nullptr);
				g_list_store_append (children, it);
				g_object_unref (it);
			}
			AbiStyleItem *it = abi_style_item_new (sTmp.c_str(), row, 0,
												 G_LIST_MODEL (children));
			g_list_store_append (root, it);
			g_object_unref (it);
		}
		else
		{
			pt_PieceTable::s_getLocalisedStyleName(sTmp.c_str(), sLoc);
			xxx_UT_DEBUGMSG(("Adding style %s at row %d \n", sLoc.c_str(), row));
			AbiStyleItem *it = abi_style_item_new (sLoc.c_str(), row, 0,
												 nullptr);
			g_list_store_append (root, it);
			g_object_unref (it);
		}
	}



	/* the flat model consumed by the GtkListView; rows start collapsed */
	GtkTreeListModel *treemodel =
		gtk_tree_list_model_new (G_LIST_MODEL (root), FALSE, FALSE,
								 s_style_create_model, nullptr, nullptr);

	if (m_wStyleList == nullptr)
	{
		GtkSingleSelection *sel = gtk_single_selection_new (
			G_LIST_MODEL (g_object_ref (treemodel)));
		gtk_single_selection_set_autoselect (sel, FALSE);
		gtk_single_selection_set_can_unselect (sel, FALSE);

		GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
		g_signal_connect (factory, "setup", G_CALLBACK (s_style_setup),
						  nullptr);
		g_signal_connect (factory, "bind", G_CALLBACK (s_style_bind),
						  nullptr);

		m_wStyleList = gtk_list_view_new (GTK_SELECTION_MODEL (sel),
										factory);
		xap_gtk_container_add (m_wStyleListContainer, m_wStyleList);

		g_signal_connect (G_OBJECT (sel),
						  "notify::selected-item",
						  G_CALLBACK (s_types_clicked),
						  static_cast<gpointer> (this));

		g_signal_connect (G_OBJECT (m_wStyleList),
						  "activate",
						  G_CALLBACK (s_types_dblclicked),
						  static_cast<gpointer> (this));
		gtk_widget_set_visible (m_wStyleList, TRUE);
	}
	else
	{
		gtk_single_selection_set_model (
			GTK_SINGLE_SELECTION (
				gtk_list_view_get_model (GTK_LIST_VIEW (m_wStyleList))),
			G_LIST_MODEL (treemodel));
	}
	g_object_unref (treemodel);

	setStyleTreeChanged(false);
}

void  AP_UnixDialog_Stylist::_populateWindowData(void)
{
	_fillTree();
	setStyleInGUI();
}

void  AP_UnixDialog_Stylist::_connectSignals(void)
{
	connectBasicSignals();

	g_signal_connect(G_OBJECT(m_windowMain), "response", 
					 G_CALLBACK(s_response_triggered), this);
	// the catch-alls
	// Dont use gtk_signal_connect_after for modeless dialogs
	g_signal_connect(G_OBJECT(m_windowMain),
			   "close-request",
			   G_CALLBACK(s_destroy_clicked),
			   static_cast<gpointer>( this));
}
