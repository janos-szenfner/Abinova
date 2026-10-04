/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Application Framework
 * Copyright (C) 1998-2000 AbiSource, Inc.
 * Copyright (C) 2009-2025 Hubert Figuiere
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

// 9/4/04 Updated to use GtkTreeView , Tim O'Brien (obrientimo@vuw.ac.nz)

#include "ut_compiler.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <set>
#include <string>

#include <gtk/gtk.h>

#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ut_string.h"
#include "ut_misc.h"
#include "ut_units.h"
#include "xap_UnixDialogHelper.h"
#include "xap_GtkListHelpers.h"
#include "xap_UnixDlg_FontChooser.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"
#include "xap_EncodingManager.h"
#include "xav_View.h"
#include "gr_UnixCairoGraphics.h"

#define PREVIEW_BOX_BORDER_WIDTH_PIXELS 8
#define PREVIEW_BOX_HEIGHT_PIXELS	80

// your typographer's standard nonsense latin font phrase
#define PREVIEW_ENTRY_DEFAULT_STRING	"Lorem ipsum dolor sit amet, consectetaur adipisicing..."

//
// For Screen color picker
	enum: uint8_t {
		RED,
		GREEN,
		BLUE,
		OPACITY
	};

static gint searchListModel(GListModel* model, const char * compareText)
{
       UT_ASSERT(model);

       // if text is null, it's not found
       if (!model || !compareText)
               return -1;

       guint n = g_list_model_get_n_items(model);
       for (guint i = 0; i < n; i++)
       {
           XAPDropDownItem *row =
               XAP_DROP_DOWN_ITEM(g_list_model_get_item(model, i));
           const char *text =
               row ? xap_drop_down_item_get_text(row) : nullptr;
           bool match = text && !g_ascii_strcasecmp(text, compareText);
           if (row)
               g_object_unref(row);
           if (match)
               return i;
       }

       return -1;
}

// font-list cell: the family name drawn in its own font
static void s_font_item_setup(GtkSignalListItemFactory * /*factory*/,
							  GtkListItem * item, gpointer /*data*/)
{
	GtkWidget *label = gtk_label_new(nullptr);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_list_item_set_child(item, label);
}

static void s_font_item_bind(GtkSignalListItemFactory * /*factory*/,
							 GtkListItem * item, gpointer /*data*/)
{
	GtkLabel *label = GTK_LABEL(gtk_list_item_get_child(item));
	gpointer row = gtk_list_item_get_item(item);
	const char *family =
		row ? xap_drop_down_item_get_text(XAP_DROP_DOWN_ITEM(row))
			: nullptr;
	gtk_label_set_text(label, family ? family : "");
	if (family && *family) {
		PangoAttrList *attrs = pango_attr_list_new();
		pango_attr_list_insert(attrs, pango_attr_family_new(family));
		gtk_label_set_attributes(label, attrs);
		pango_attr_list_unref(attrs);
	} else {
		gtk_label_set_attributes(label, nullptr);
	}
}

//
// Create GtkListView that is similar to a CList
// ie single text column, with no header
static GtkWidget* createFontTabListView(bool fontPreview,
										GtkSingleSelection **selOut)
{
	GListStore* listStore = XAP_list_store_new();
	GtkSingleSelection *sel = gtk_single_selection_new(nullptr);
	gtk_single_selection_set_autoselect(sel, FALSE);
	gtk_single_selection_set_can_unselect(sel, TRUE);
	gtk_single_selection_set_model(sel, G_LIST_MODEL(listStore));
	g_object_unref(G_OBJECT(listStore));

	GtkListItemFactory *factory;
	if (fontPreview) {
		factory = gtk_signal_list_item_factory_new();
		g_signal_connect(factory, "setup", G_CALLBACK(s_font_item_setup),
						 nullptr);
		g_signal_connect(factory, "bind", G_CALLBACK(s_font_item_bind),
						 nullptr);
	} else {
		factory = XAP_list_item_text_factory();
	}

	/* Build an empty view and attach model/factory via the setters:
	 * passing a late-bound GtkSingleSelection into gtk_list_view_new()
	 * corrupts the view's factory property on GTK 4.14. */
	GtkWidget* view = gtk_list_view_new(nullptr, nullptr);
	gtk_list_view_set_model(GTK_LIST_VIEW(view), GTK_SELECTION_MODEL(sel));
	gtk_list_view_set_factory(GTK_LIST_VIEW(view), factory);
	g_object_unref(G_OBJECT(sel));
	g_object_unref(G_OBJECT(factory));

	if (selOut)
		*selOut = GTK_SINGLE_SELECTION(
			gtk_list_view_get_model(GTK_LIST_VIEW(view)));
	return view;
}


/*****************************************************************/
XAP_Dialog * XAP_UnixDialog_FontChooser::static_constructor(XAP_DialogFactory * pFactory,
														 XAP_Dialog_Id id)
{
	XAP_UnixDialog_FontChooser * p = new XAP_UnixDialog_FontChooser(pFactory,id);
	return p;
}

XAP_UnixDialog_FontChooser::XAP_UnixDialog_FontChooser(XAP_DialogFactory * pDlgFactory,
												   XAP_Dialog_Id id)
	: XAP_Dialog_FontChooser(pDlgFactory,id)
{
	m_fontList = nullptr;
	m_styleList = nullptr;
	m_sizeList = nullptr;
	m_selFonts = nullptr;
	m_selStyles = nullptr;
	m_selSizes = nullptr;
	m_checkStrikeOut = nullptr;
	m_checkUnderline = nullptr;
	m_checkOverline = nullptr;
	m_checkHidden = nullptr;
	m_checkTransparency = nullptr;
	m_checkSubScript = nullptr;
	m_iSubScriptId = 0;
	m_checkSuperScript = nullptr;
	m_iSuperScriptId = 0;
	m_colorSelector = nullptr;
	m_bgcolorSelector = nullptr;
	m_preview = nullptr;

	m_gc = nullptr;
	m_pFrame = nullptr;
	m_doneFirstFont = false;

	memset(&m_currentFGColor, 0, sizeof(m_currentFGColor));
	memset(&m_currentBGColor, 0, sizeof(m_currentBGColor));
	m_currentBGColorTransparent = false;
	memset(&m_funkyColor, 0, sizeof(m_funkyColor));
}

XAP_UnixDialog_FontChooser::~XAP_UnixDialog_FontChooser(void)
{
	DELETEP(m_gc);
}


/*****************************************************************/

static gint s_color_update(GtkWidget * /* widget */,
                           GdkRGBA * /* color */,
                           XAP_UnixDialog_FontChooser * dlg)
{
	UT_return_val_if_fail(dlg,FALSE);
	dlg->fgColorChanged();
	return FALSE;
}

static gint s_bgcolor_update(GtkWidget * /* widget */,
                           GdkRGBA * /* color */,
						   XAP_UnixDialog_FontChooser * dlg)
{
	UT_return_val_if_fail(dlg,FALSE);
	dlg->bgColorChanged();
	return FALSE;
}

static void s_select_row_font(GtkSelectionModel * /* model */,
							  guint /* position */, guint /* n_items */,
							  XAP_UnixDialog_FontChooser * dlg)
{
	UT_return_if_fail(dlg);
    // update the row number and show the changed preview
	// redisplay the preview text
	dlg->fontRowChanged();
}


static void s_select_row_style(GtkSelectionModel * /* model */,
							   guint /* position */, guint /* n_items */,
							   XAP_UnixDialog_FontChooser * dlg)
{
	UT_return_if_fail(dlg);

	// redisplay the preview text
	dlg->styleRowChanged();
}

static void s_select_row_size(GtkSelectionModel * /* model */,
							  guint /* position */, guint /* n_items */,
							  XAP_UnixDialog_FontChooser * dlg)
{
	UT_return_if_fail(dlg);

	// redisplay the preview text
	dlg->sizeRowChanged();
}

static void s_drawing_area_draw(GtkDrawingArea * /* area */,
								cairo_t * /* cr */,
								int /* width */,
								int /* height */,
								gpointer data)
{
	XAP_UnixDialog_FontChooser * dlg =
		static_cast<XAP_UnixDialog_FontChooser *>(data);
	dlg->event_previewDrawImmediate();
}

static void s_underline_toggled(GtkWidget * ,  XAP_UnixDialog_FontChooser * dlg)
{
	dlg->underlineChanged();
}


static void s_overline_toggled(GtkWidget * ,  XAP_UnixDialog_FontChooser * dlg)
{
	dlg->overlineChanged();
}


static void s_subscript_toggled(GtkWidget * ,  XAP_UnixDialog_FontChooser * dlg) 
{ 
    dlg->subscriptChanged(); 
} 
 
 
static void s_superscript_toggled(GtkWidget * ,  XAP_UnixDialog_FontChooser * dlg) 
{ 
    dlg->superscriptChanged(); 
} 
 
 
static void s_strikeout_toggled(GtkWidget * ,  XAP_UnixDialog_FontChooser * dlg)
{
	dlg->strikeoutChanged();
}

static void s_hidden_toggled(GtkWidget * ,  XAP_UnixDialog_FontChooser * dlg)
{
	dlg->hiddenChanged();
}


static void s_transparency_toggled(GtkWidget * ,  XAP_UnixDialog_FontChooser * dlg)
{
	dlg->transparencyChanged();
}

/*****************************************************************/

void XAP_UnixDialog_FontChooser::underlineChanged(void)
{
	m_bUnderline = gtk_check_button_get_active(GTK_CHECK_BUTTON(m_checkUnderline));
	m_bChangedUnderline = !m_bChangedUnderline;
	setFontDecoration(m_bUnderline,m_bOverline,m_bStrikeout,m_bTopline,m_bBottomline);
	updatePreview();
}


void XAP_UnixDialog_FontChooser::strikeoutChanged(void)
{
	m_bStrikeout = gtk_check_button_get_active(GTK_CHECK_BUTTON(m_checkStrikeOut));
	m_bChangedStrikeOut = !m_bChangedStrikeOut;
	setFontDecoration(m_bUnderline,m_bOverline,m_bStrikeout,m_bTopline,m_bBottomline);
	updatePreview();
}


void XAP_UnixDialog_FontChooser::overlineChanged(void)
{
	m_bOverline = gtk_check_button_get_active(GTK_CHECK_BUTTON(m_checkOverline));
	m_bChangedOverline = !m_bChangedOverline;
	setFontDecoration(m_bUnderline,m_bOverline,m_bStrikeout,m_bTopline,m_bBottomline);
	updatePreview();
}

 
void XAP_UnixDialog_FontChooser::subscriptChanged(void) 
{ 
    m_bSubScript = gtk_check_button_get_active(GTK_CHECK_BUTTON(m_checkSubScript)); 
    m_bChangedSubScript = !m_bChangedSubScript; 
    if (m_bSubScript)
	{
		if (gtk_check_button_get_active(GTK_CHECK_BUTTON(m_checkSuperScript)))
		{
			g_signal_handler_block(G_OBJECT(m_checkSuperScript), m_iSuperScriptId);
			gtk_check_button_set_active(GTK_CHECK_BUTTON(m_checkSuperScript), false);
			g_signal_handler_unblock(G_OBJECT(m_checkSuperScript), m_iSuperScriptId);
			m_bChangedSuperScript = !m_bChangedSuperScript;
			setSuperScript(false);
		}
	}
    setSubScript(m_bSubScript); 
    updatePreview(); 
} 
 
void XAP_UnixDialog_FontChooser::superscriptChanged(void) 
{ 
    m_bSuperScript = gtk_check_button_get_active(GTK_CHECK_BUTTON(m_checkSuperScript)); 
    m_bChangedSuperScript = !m_bChangedSuperScript; 
    if (m_bSuperScript)
	{
		if (gtk_check_button_get_active(GTK_CHECK_BUTTON(m_checkSubScript)))
		{
			g_signal_handler_block(G_OBJECT(m_checkSubScript), m_iSubScriptId);
    		gtk_check_button_set_active(GTK_CHECK_BUTTON(m_checkSubScript), false);
			g_signal_handler_unblock(G_OBJECT(m_checkSubScript), m_iSubScriptId);
			m_bChangedSubScript = !m_bChangedSubScript;
			setSubScript(false);
		}
	}
    setSuperScript(m_bSuperScript); 
    updatePreview(); 
} 
 
 
void XAP_UnixDialog_FontChooser::hiddenChanged(void)
{
	m_bHidden = gtk_check_button_get_active(GTK_CHECK_BUTTON(m_checkHidden));
	m_bChangedHidden = !m_bChangedHidden;
}

void XAP_UnixDialog_FontChooser::transparencyChanged(void)
{
	bool bTrans = gtk_check_button_get_active(GTK_CHECK_BUTTON(m_checkTransparency));
	if(bTrans)
	{
		addOrReplaceVecProp("bgcolor","transparent");
		m_currentBGColorTransparent = true;
	}
	updatePreview();
}

void XAP_UnixDialog_FontChooser::textTransformChanged(void)
{
	updatePreview();
}

void XAP_UnixDialog_FontChooser::fontRowChanged(void)
{
	static char szFontFamily[60];

	const char *text = XAP_single_selection_get_text(m_selFonts);
	if (text)
	{
		g_snprintf(szFontFamily, 50, "%s", text);
		addOrReplaceVecProp("font-family",static_cast<gchar*>(szFontFamily));
	}

	updatePreview();
}

void XAP_UnixDialog_FontChooser::styleRowChanged(void)
{
	guint pos = gtk_single_selection_get_selected(m_selStyles);
	if (pos != GTK_INVALID_LIST_POSITION)
	{
		gint rowNumber = static_cast<gint>(pos);

		// perhaps these attributes really should be smashed
		// into bitfields.  :)
		if (rowNumber == LIST_STYLE_NORMAL)
		{
			addOrReplaceVecProp("font-style","normal");
			addOrReplaceVecProp("font-weight","normal");
		}
		else if (rowNumber == LIST_STYLE_BOLD)
		{
			addOrReplaceVecProp("font-style","normal");
			addOrReplaceVecProp("font-weight","bold");
		}
		else if (rowNumber == LIST_STYLE_ITALIC)
		{
			addOrReplaceVecProp("font-style","italic");
			addOrReplaceVecProp("font-weight","normal");
		}
		else if (rowNumber == LIST_STYLE_BOLD_ITALIC)
		{
			addOrReplaceVecProp("font-style","italic");
			addOrReplaceVecProp("font-weight","bold");
		}
		else
		{
			UT_ASSERT_HARMLESS(0);
		}
	}
	updatePreview();
}


void XAP_UnixDialog_FontChooser::sizeRowChanged(void)
{
	// used similarly to convert between text and numeric arguments
	static char szFontSize[50];

	const char *text = XAP_single_selection_get_text(m_selSizes);
	if (text)
	{
		g_snprintf(szFontSize, 50, "%spt",
				   static_cast<const gchar *>(XAP_EncodingManager::fontsizes_mapping.lookupByTarget(text)));
		addOrReplaceVecProp("font-size",static_cast<gchar *>(szFontSize));
	}
	updatePreview();
}

void XAP_UnixDialog_FontChooser::fgColorChanged(void)
{
	gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER(m_colorSelector),
							   &m_currentFGColor);
	UT_RGBColor * rgbcolor = UT_UnixGdkRGBAToRGBColor(m_currentFGColor);
	UT_HashColor hash_color;
	const char * c = hash_color.setColor(*rgbcolor);
	addOrReplaceVecProp("color",  c + 1);
	delete rgbcolor;
	updatePreview();
}


void XAP_UnixDialog_FontChooser::bgColorChanged(void)
{
	gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER(m_bgcolorSelector),
								&m_currentBGColor);
	UT_RGBColor * rgbcolor = UT_UnixGdkRGBAToRGBColor(m_currentBGColor);
	UT_HashColor hash_color;
	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_checkTransparency), FALSE);
	m_currentBGColorTransparent = false;
	// test for funkyColor-has-been-changed-to-sane-color case
	addOrReplaceVecProp("bgcolor", hash_color.setColor(*rgbcolor) + 1);
	delete rgbcolor;
	updatePreview();
}

GtkWidget * XAP_UnixDialog_FontChooser::constructWindow(void)
{
	const XAP_StringSet * pSS = m_pApp->getStringSet();
	GtkWidget *windowFontSelection;
	GtkWidget *vboxMain;
	GtkWidget *vboxOuter;

	std::string s;
	pSS->getValueUTF8(XAP_STRING_ID_DLG_UFS_FontTitle,s);
	windowFontSelection = abiDialogNew ( "font dialog", TRUE, s.c_str() ) ;
	vboxOuter = gtk_dialog_get_content_area(GTK_DIALOG(windowFontSelection));

	vboxMain = constructWindowContents(vboxOuter);
	gtk_box_append(GTK_BOX(vboxOuter), vboxMain);
			gtk_widget_set_hexpand(vboxMain, TRUE);
			gtk_widget_set_vexpand(vboxMain, TRUE);

	pSS->getValueUTF8(XAP_STRING_ID_DLG_Cancel, s);
	abiAddButton ( GTK_DIALOG(windowFontSelection), s, BUTTON_CANCEL ) ;
	pSS->getValueUTF8(XAP_STRING_ID_DLG_OK, s);
	abiAddButton ( GTK_DIALOG(windowFontSelection), s, BUTTON_OK ) ;

	return windowFontSelection;
}

// GtkBuilder generated dialog, using fixed widgets to closely match
// the Windows layout, with some changes for color selector
GtkWidget * XAP_UnixDialog_FontChooser::constructWindowContents(GtkWidget *)
{
	GtkWidget *vboxMain;
	GtkWidget *notebookMain;
	GtkWidget *labelFont;
	GtkWidget *labelStyle;
	GtkWidget *listFonts;
	GtkWidget *labelSize;
	GtkWidget *lblEffects;
	GtkWidget *grEffectRows;
	GtkWidget *checkbuttonStrikeout;
	GtkWidget *checkbuttonUnderline;
	GtkWidget *checkbuttonOverline;
	GtkWidget *checkbuttonHidden;
	GtkWidget *checkbuttonSubscript;
	GtkWidget *checkbuttonSuperscript;
 	GtkWidget *listStyles;
	GtkWidget *listSizes;
	GtkWidget *hbox1;
	GtkWidget *colorSelector;
	GtkWidget *colorBGSelector;
	GtkWidget *labelTabFont;
	GtkWidget *labelTabColor;
	GtkWidget *labelTabBGColor;
	GtkWidget *frame4;

	// the entry is a special drawing area full of one
	// of our graphics contexts
	GtkWidget *entryArea;

	const XAP_StringSet * pSS = m_pApp->getStringSet();

	vboxMain = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_visible(vboxMain, TRUE);

	notebookMain = gtk_notebook_new ();
	gtk_widget_set_visible(notebookMain, TRUE);
	gtk_box_append(GTK_BOX(vboxMain), notebookMain);
	XAP_gtk_widget_set_margin(notebookMain, 8);

	GtkWidget *grid1;
	GtkWidget *scrolledwindow1;
	GtkWidget *scrolledwindow2;
	GtkWidget *scrolledwindow3;
//  	GtkWidget *hboxForEncoding;
	grid1 = gtk_grid_new();
	g_object_set(G_OBJECT(grid1),
	             "row-spacing", 6,
	             "column-spacing", 12,
	             "margin-top", 12,
	             "margin-bottom", 12,
	             "margin-start", 12,
	             "margin-end", 12,
	             nullptr);
	gtk_widget_set_visible(grid1, TRUE);

	std::string s;
	// Label for first page of the notebook
	pSS->getValueUTF8(XAP_STRING_ID_DLG_UFS_FontTab,s);
	labelTabFont = gtk_label_new (s.c_str());
	gtk_widget_set_visible(labelTabFont, TRUE);
//
// Make first page of the notebook
//
	gtk_notebook_append_page(GTK_NOTEBOOK(notebookMain), grid1, labelTabFont);

	pSS->getValueUTF8(XAP_STRING_ID_DLG_UFS_FontLabel,s);
	labelFont = gtk_label_new (s.c_str());
	gtk_widget_set_halign(labelFont, GTK_ALIGN_CENTER);
	gtk_widget_set_visible(labelFont, TRUE);
	gtk_grid_attach(GTK_GRID(grid1), labelFont, 0, 0, 1, 1);

	scrolledwindow1 = gtk_scrolled_window_new();
	gtk_widget_set_visible(scrolledwindow1, TRUE);
	gtk_widget_set_hexpand(scrolledwindow1, TRUE);
	gtk_widget_set_vexpand(scrolledwindow1, TRUE);
	gtk_grid_attach(GTK_GRID(grid1), scrolledwindow1, 0, 1, 1, 3);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	listFonts = createFontTabListView(true, &m_selFonts);
	/* GtkScrolledWindow ignores min-content-width under
	 * GTK_POLICY_NEVER, and a bare GtkListView requests ~46px —
	 * far too narrow for family names. */
	gtk_widget_set_size_request(listFonts, 160, -1);
	gtk_widget_set_visible(listFonts, TRUE);
	xap_gtk_container_add (scrolledwindow1, listFonts);

	pSS->getValueUTF8(XAP_STRING_ID_DLG_UFS_StyleLabel,s);
	labelStyle = gtk_label_new (s.c_str());
	gtk_widget_set_halign(labelStyle, GTK_ALIGN_CENTER);
	gtk_widget_set_visible(labelStyle, TRUE);
	gtk_grid_attach(GTK_GRID(grid1), labelStyle, 1, 0, 1, 1);

	scrolledwindow2 = gtk_scrolled_window_new();
	gtk_widget_set_visible(scrolledwindow2, TRUE);
	gtk_widget_set_hexpand(scrolledwindow2, TRUE);
	gtk_widget_set_vexpand(scrolledwindow2, TRUE);
	gtk_grid_attach(GTK_GRID(grid1), scrolledwindow2, 1, 1, 1, 1);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow2), GTK_POLICY_NEVER, GTK_POLICY_NEVER);

	listStyles = createFontTabListView(false, &m_selStyles);
	gtk_widget_set_name (listStyles, "listStyles");
	gtk_widget_set_visible(listStyles, TRUE);
	xap_gtk_container_add (scrolledwindow2, listStyles);

	pSS->getValueUTF8(XAP_STRING_ID_DLG_UFS_SizeLabel,s);
	labelSize = gtk_label_new (s.c_str());
	gtk_widget_set_halign(labelSize, GTK_ALIGN_CENTER);
	gtk_widget_set_visible(labelSize, TRUE);
	gtk_grid_attach(GTK_GRID(grid1), labelSize, 2, 0, 1, 1);

	scrolledwindow3 = gtk_scrolled_window_new();
	gtk_widget_set_visible(scrolledwindow3, TRUE);
	gtk_widget_set_hexpand(scrolledwindow3, TRUE);
	gtk_widget_set_vexpand(scrolledwindow3, TRUE);
	gtk_grid_attach(GTK_GRID(grid1), scrolledwindow3, 2, 1, 1, 1);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow3), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	listSizes = createFontTabListView(false, &m_selSizes);
	gtk_widget_set_visible(listSizes, TRUE);
	xap_gtk_container_add (scrolledwindow3, listSizes);

	grEffectRows = gtk_grid_new();
	g_object_set(G_OBJECT(grEffectRows),
	             "row-spacing", 6,
	             "column-spacing", 12,
	             "margin-top", 12,
	             nullptr);
	gtk_widget_set_visible(grEffectRows, TRUE);

	gtk_grid_attach(GTK_GRID(grid1), grEffectRows, 1, 2, 2, 1);
	pSS->getValueUTF8(XAP_STRING_ID_DLG_UFS_EffectsFrameLabel,s);
	s = std::string("<b>") + s + "</b>";
	lblEffects = gtk_label_new (s.c_str());
	g_object_set(lblEffects, "use-markup", true, "xalign", 0., nullptr);
	gtk_widget_set_visible(lblEffects, TRUE);
	gtk_grid_attach(GTK_GRID(grEffectRows), lblEffects, 0, 0, 4, 1);

	pSS->getValueUTF8(XAP_STRING_ID_DLG_UFS_StrikeoutCheck,s);
	checkbuttonStrikeout = gtk_check_button_new_with_label (s.c_str());
	gtk_widget_set_margin_start(checkbuttonStrikeout, 18);
	gtk_widget_set_visible(checkbuttonStrikeout, TRUE);
	gtk_grid_attach(GTK_GRID(grEffectRows), checkbuttonStrikeout, 0, 1, 1, 1);

	pSS->getValueUTF8(XAP_STRING_ID_DLG_UFS_UnderlineCheck,s);
	checkbuttonUnderline = gtk_check_button_new_with_label (s.c_str());
	gtk_widget_set_visible(checkbuttonUnderline, TRUE);
	gtk_grid_attach(GTK_GRID(grEffectRows), checkbuttonUnderline, 1, 1, 1, 1);

	pSS->getValueUTF8(XAP_STRING_ID_DLG_UFS_OverlineCheck,s);
	checkbuttonOverline = gtk_check_button_new_with_label (s.c_str());
	gtk_widget_set_visible(checkbuttonOverline, TRUE);
	gtk_grid_attach(GTK_GRID(grEffectRows), checkbuttonOverline, 2, 1, 1, 1);

	pSS->getValueUTF8(XAP_STRING_ID_DLG_UFS_HiddenCheck,s);
	checkbuttonHidden = gtk_check_button_new_with_label (s.c_str());
	gtk_widget_set_visible(checkbuttonHidden, TRUE);
	gtk_grid_attach(GTK_GRID(grEffectRows), checkbuttonHidden, 3, 1, 1, 1);

	/* subscript/superscript */

	pSS->getValueUTF8(XAP_STRING_ID_DLG_UFS_SubScript,s);
	checkbuttonSubscript = gtk_check_button_new_with_label (s.c_str());
	gtk_widget_set_margin_start(checkbuttonSubscript, 18);
	gtk_widget_set_visible(checkbuttonSubscript, TRUE);
	gtk_grid_attach(GTK_GRID(grEffectRows), checkbuttonSubscript, 0, 2, 1, 1);

	pSS->getValueUTF8(XAP_STRING_ID_DLG_UFS_SuperScript,s);
	checkbuttonSuperscript = gtk_check_button_new_with_label (s.c_str());
	gtk_widget_set_visible(checkbuttonSuperscript, TRUE);
	gtk_grid_attach(GTK_GRID(grEffectRows), checkbuttonSuperscript, 1, 2, 1, 1);

	/* Notebook page for ForeGround Color Selector */

	hbox1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_visible(hbox1, TRUE);

    // Label for second page of the notebook

	pSS->getValueUTF8(XAP_STRING_ID_DLG_UFS_ColorTab,s);
	labelTabColor = gtk_label_new (s.c_str());
	gtk_widget_set_visible(labelTabColor, TRUE);

//
// Make second page of the notebook
//
    gtk_notebook_append_page(GTK_NOTEBOOK(notebookMain), hbox1,labelTabColor);

	colorSelector = gtk_color_chooser_widget_new ();
	XAP_gtk_widget_set_margin(colorSelector, 6);
	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(colorSelector), FALSE);
	gtk_widget_set_visible(colorSelector, TRUE);
	gtk_box_append(GTK_BOX(hbox1), colorSelector);
			gtk_widget_set_hexpand(colorSelector, TRUE);
			gtk_widget_set_vexpand(colorSelector, TRUE);

	/*Notebook page for Background Color Selector*/

	GtkWidget * vboxBG = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_visible(vboxBG, TRUE);

    // Label for third page of the notebook

	pSS->getValueUTF8(XAP_STRING_ID_DLG_UFS_BGColorTab,s);
	labelTabBGColor = gtk_label_new (s.c_str());
	gtk_widget_set_visible(labelTabBGColor, TRUE);
//
// Make third page of the notebook
//
    gtk_notebook_append_page(GTK_NOTEBOOK(notebookMain), vboxBG,labelTabBGColor);

	colorBGSelector = gtk_color_chooser_widget_new ();
	XAP_gtk_widget_set_margin(colorBGSelector, 6);
	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(colorBGSelector), FALSE);
	gtk_widget_set_visible(colorBGSelector, TRUE);
	gtk_box_append(GTK_BOX(vboxBG), colorBGSelector);
			gtk_widget_set_hexpand(colorBGSelector, TRUE);
			gtk_widget_set_vexpand(colorBGSelector, TRUE);

//
// Make a toggle button to set hightlight color transparent
//
	pSS->getValueUTF8(XAP_STRING_ID_DLG_UFS_TransparencyCheck,s);
	GtkWidget * checkbuttonTrans = gtk_check_button_new_with_label (s.c_str());
	XAP_gtk_widget_set_margin(checkbuttonTrans, 6);
	gtk_widget_set_visible(checkbuttonTrans, TRUE);
	gtk_box_append(GTK_BOX(vboxBG), checkbuttonTrans);
			gtk_widget_set_hexpand(checkbuttonTrans, TRUE);
			gtk_widget_set_vexpand(checkbuttonTrans, TRUE);

	/* frame with preview */

	frame4 = gtk_frame_new (nullptr);
	gtk_widget_set_visible(frame4, TRUE);
	gtk_box_append(GTK_BOX(vboxMain), frame4);
	// setting the height takes into account the border applied on all
	// sides, so we need to double the single border width
	gtk_widget_set_size_request (frame4, -1, PREVIEW_BOX_HEIGHT_PIXELS + (PREVIEW_BOX_BORDER_WIDTH_PIXELS * 2));
	XAP_gtk_widget_set_margin(frame4, PREVIEW_BOX_BORDER_WIDTH_PIXELS);

	entryArea = gtk_drawing_area_new();
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(entryArea),
								   s_drawing_area_draw, this, nullptr);
	gtk_widget_set_size_request (entryArea, -1, PREVIEW_BOX_HEIGHT_PIXELS);
	gtk_widget_set_visible(entryArea, TRUE);
	xap_gtk_container_add (frame4, entryArea);


	// save out to members for callback and class access
	m_fontList = listFonts;
	m_styleList = listStyles;
	m_sizeList = listSizes;
	m_colorSelector = colorSelector;
	m_bgcolorSelector = colorBGSelector;
	m_preview = entryArea;
	m_checkStrikeOut = checkbuttonStrikeout;
	m_checkUnderline = checkbuttonUnderline;
	m_checkOverline = checkbuttonOverline;
	m_checkSubScript = checkbuttonSubscript;
	m_checkSuperScript = checkbuttonSuperscript;
	m_checkHidden = checkbuttonHidden;
	m_checkTransparency = checkbuttonTrans;

	// bind signals to things
	g_signal_connect(G_OBJECT(m_checkUnderline),
					   "toggled",
					   G_CALLBACK(s_underline_toggled),
					   static_cast<gpointer>(this));

	g_signal_connect(G_OBJECT(m_checkOverline),
					   "toggled",
					   G_CALLBACK(s_overline_toggled),
					   static_cast<gpointer>(this));

	g_signal_connect(G_OBJECT(m_checkStrikeOut),
					   "toggled",
					   G_CALLBACK(s_strikeout_toggled),
					   static_cast<gpointer>(this));

	g_signal_connect(G_OBJECT(m_checkHidden),
					   "toggled",
					   G_CALLBACK(s_hidden_toggled),
					   static_cast<gpointer>(this));

	m_iSubScriptId = g_signal_connect(G_OBJECT(m_checkSubScript),
					   "toggled",
					   G_CALLBACK(s_subscript_toggled),
					   static_cast<gpointer>(this));

	m_iSuperScriptId = g_signal_connect(G_OBJECT(m_checkSuperScript),
					   "toggled",
					   G_CALLBACK(s_superscript_toggled),
					   static_cast<gpointer>(this));

	g_signal_connect(G_OBJECT(m_checkTransparency),
					   "toggled",
					   G_CALLBACK(s_transparency_toggled),
					   static_cast<gpointer>(this));

	g_signal_connect(G_OBJECT(m_selFonts),
					   "selection-changed",
					   G_CALLBACK(s_select_row_font),
					   static_cast<gpointer>(this));

	g_signal_connect(G_OBJECT(m_selStyles),
					   "selection-changed",
					   G_CALLBACK(s_select_row_style),
					   static_cast<gpointer>(this));

	g_signal_connect(G_OBJECT(m_selSizes),
					   "selection-changed",
					   G_CALLBACK(s_select_row_size),
					   static_cast<gpointer>(this));

	// This is a catch-all color selector callback which catches any
	// real-time updating of the color so we can refresh our preview
	// text
	g_signal_connect(G_OBJECT(colorSelector),
			 "color-activated",
			 G_CALLBACK(s_color_update),
			 static_cast<gpointer>(this));

	g_signal_connect(G_OBJECT(colorBGSelector),
			 "color-activated",
			 G_CALLBACK(s_bgcolor_update),
			 static_cast<gpointer>(this));

	gtk_widget_set_can_focus(listFonts, true);
	gtk_widget_set_can_focus(listStyles, true);
	gtk_widget_set_can_focus(listSizes, true);

	gtk_widget_grab_focus(scrolledwindow1);

	
	const gchar * text;

	// update the styles list
	GListStore *styleStore =
		G_LIST_STORE(gtk_single_selection_get_model(m_selStyles));
	g_list_store_remove_all(styleStore);

	text = pSS->getValue(XAP_STRING_ID_DLG_UFS_StyleRegular);
	XAP_list_store_append_text(styleStore, text);
	text = pSS->getValue(XAP_STRING_ID_DLG_UFS_StyleItalic);
	XAP_list_store_append_text(styleStore, text);
	text = pSS->getValue(XAP_STRING_ID_DLG_UFS_StyleBold);
	XAP_list_store_append_text(styleStore, text);
	text = pSS->getValue(XAP_STRING_ID_DLG_UFS_StyleBoldItalic);
	XAP_list_store_append_text(styleStore, text);



	GListStore *sizeStore =
		G_LIST_STORE(gtk_single_selection_get_model(m_selSizes));
	g_list_store_remove_all(sizeStore);
	// TODO perhaps populate the list based on the selected font/style?
	{
		int sz = XAP_EncodingManager::fontsizes_mapping.size();
		for (int i = 0; i < sz; ++i)
		{
			text = XAP_EncodingManager::fontsizes_mapping.nth2(i);
			XAP_list_store_append_text(sizeStore, text);
	    }
	}

	return vboxMain;
}

void XAP_UnixDialog_FontChooser::runModal(XAP_Frame * pFrame)
{
	m_pFrame = static_cast<XAP_Frame *>(pFrame);

	// used similarly to convert between text and numeric arguments
	static char sizeString[50];

	// build the dialog
	GtkWidget * cf = constructWindow();

	// freeze updates of the preview
	m_blockUpdate = true;

	// to sort out dupes
    std::set<std::string> fontSet;

	GListStore *fontStore =
		G_LIST_STORE(gtk_single_selection_get_model(m_selFonts));
	g_list_store_remove_all(fontStore);

	GR_GraphicsFactory * pGF = XAP_App::getApp()->getGraphicsFactory();
	if(!pGF)
	{
		return;
	}

	const std::vector<std::string> & names = GR_CairoGraphics::getAllFontNames();

	for (std::vector<std::string>::const_iterator  i = names.begin();
		 i != names.end(); ++i)
	{
		const std::string & fName = *i;

		if (fontSet.find(fName) == fontSet.end())
		{
            fontSet.insert(fName);

		    XAP_list_store_append_text(fontStore, fName.c_str());
		  }
	}

	// Set the defaults in the list boxes according to dialog data
	gint foundAt = 0;

	const std::string sFontFamily = getVal("font-family");
	foundAt = searchListModel(G_LIST_MODEL(fontStore), sFontFamily.c_str());

	// select and scroll to font name
	if (foundAt >= 0) {
		gtk_list_view_scroll_to(GTK_LIST_VIEW(m_fontList),
								static_cast<guint>(foundAt),
								static_cast<GtkListScrollFlags>(
									GTK_LIST_SCROLL_SELECT |
									GTK_LIST_SCROLL_FOCUS),
								nullptr);
	}

	// this is pretty messy
	listStyle st = LIST_STYLE_NORMAL;
	const std::string sWeight = getVal("font-weight");
	const std::string sStyle = getVal("font-style");
	if (sStyle.empty() || sWeight.empty())
		st = LIST_STYLE_NONE;
	else {
		bool isBold = !g_ascii_strcasecmp(sWeight.c_str(), "bold");
		bool isItalic = !g_ascii_strcasecmp(sStyle.c_str(), "italic");
		if (!isBold && !isItalic) {
			st = LIST_STYLE_NORMAL;
		}
		else if (!isItalic && isBold) {
			st = LIST_STYLE_BOLD;
		}
		else if (isItalic && !isBold) {
			st = LIST_STYLE_ITALIC;
		}
		else if (isItalic && isBold) {
			st = LIST_STYLE_BOLD_ITALIC;
		}
		else {
			UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		}
	}

	// select and scroll to style name
	if (st != LIST_STYLE_NONE) {
		gtk_list_view_scroll_to(GTK_LIST_VIEW(m_styleList),
								static_cast<guint>(st),
								static_cast<GtkListScrollFlags>(
									GTK_LIST_SCROLL_SELECT |
									GTK_LIST_SCROLL_FOCUS),
								nullptr);
	}

	g_snprintf(sizeString, sizeof(sizeString), "%s", std_size_string(UT_convertToPoints(getVal("font-size").c_str())));
	foundAt = searchListModel(
		gtk_single_selection_get_model(m_selSizes),
		XAP_EncodingManager::fontsizes_mapping.lookupBySource(sizeString));

	// select and scroll to size name
	if (foundAt >= 0) {
		gtk_list_view_scroll_to(GTK_LIST_VIEW(m_sizeList),
								static_cast<guint>(foundAt),
								static_cast<GtkListScrollFlags>(
									GTK_LIST_SCROLL_SELECT |
									GTK_LIST_SCROLL_FOCUS),
								nullptr);
	}

	// Set color in the color selector
	const std::string sColor = getVal("color");
	if (!sColor.empty())
	{
		UT_RGBColor c;
		UT_parseColor(sColor.c_str(), c);

		GdkRGBA *color = UT_UnixRGBColorToGdkRGBA(c);
		m_currentFGColor = *color;
		gdk_rgba_free(color);
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(m_colorSelector), &m_currentFGColor);
	}
	else
	{
		// if we have no color, use a placeholder of funky values
		// the user can't pick interactively.  This catches ALL
		// the cases except where the user specifically enters -1 for
		// all Red, Green and Blue attributes manually.  This user
		// should expect it not to touch the color.  :)
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(m_colorSelector), &m_funkyColor);
	}

	// Set color in the color selector
	const std::string sBGCol = getVal("bgcolor");
	if (!sBGCol.empty() && strcmp(sBGCol.c_str(),"transparent") != 0)
	{
		UT_RGBColor c;
		UT_parseColor(sBGCol.c_str(), c);

		GdkRGBA *color = UT_UnixRGBColorToGdkRGBA(c);
		m_currentBGColor = *color;
		gdk_rgba_free(color);
		gtk_check_button_set_active(GTK_CHECK_BUTTON(m_checkTransparency), FALSE);
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(m_bgcolorSelector), &m_currentBGColor);
	}
	else
		gtk_check_button_set_active(GTK_CHECK_BUTTON(m_checkTransparency), TRUE);

	// fix for GTK's questionable gtk_toggle_set_active behaviour (emits when setting TRUE)
	m_bChangedStrikeOut = m_bStrikeout;
	m_bChangedUnderline = m_bUnderline;
	m_bChangedOverline = m_bOverline;
	m_bChangedHidden = m_bHidden;
	m_bChangedSubScript = m_bSubScript;
	m_bChangedSuperScript = m_bSuperScript;

	// set the strikeout, underline, overline, and hidden check buttons
	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_checkStrikeOut), m_bStrikeout);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_checkUnderline), m_bUnderline);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_checkOverline), m_bOverline);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_checkHidden), m_bHidden);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_checkSubScript), m_bSubScript);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_checkSuperScript), m_bSuperScript);

	m_doneFirstFont = true;

	// transient before the early show: GTK4 maps a GtkWindow the
	// moment it becomes visible, and warns about windows mapped
	// without a transient parent
	{
		XAP_UnixFrameImpl * pImpl =
			pFrame ? static_cast<XAP_UnixFrameImpl *>(pFrame->getFrameImpl())
				   : nullptr;
		GtkWidget * parentWindow = pImpl ? pImpl->getTopLevelWindow() : nullptr;
		if (GTK_IS_WINDOW(parentWindow))
			gtk_window_set_transient_for(GTK_WINDOW(cf), GTK_WINDOW(parentWindow));
	}

	// attach a new graphics context
	gtk_widget_set_visible(cf , TRUE);
	
	GR_UnixCairoAllocInfo ai(m_preview);
	m_gc = static_cast<GR_CairoGraphics*>( XAP_App::getApp()->newGraphics(ai));
	GtkAllocation alloc;

	gtk_widget_get_allocation(m_preview, &alloc);
	_createFontPreviewFromGC(m_gc,alloc.width,alloc.height);
//
// This enables callbacks on the preview area with a widget pointer to
// access this dialog.
//
	g_object_set_data(G_OBJECT(m_preview), "user-data", this);

	// unfreeze updates of the preview
	m_blockUpdate = false;
	// manually trigger an update
	updatePreview();


	switch ( abiRunModalDialog ( GTK_DIALOG(cf), pFrame, this, BUTTON_CANCEL, true ) )
	  {
	  case BUTTON_OK:
	    {
	      m_answer = a_OK;
	      break ;
	    }
	  default:
	    {
	      m_answer = a_CANCEL;
	      break;
	    }
	  }

	// these dialogs are cached around through the dialog framework,
	// and this variable needs to get set back
	m_doneFirstFont = false;

	UT_DEBUGMSG(("FontChooserEnd: Family[%s%s] Size[%s%s] Weight[%s%s] Style[%s%s] Color[%s%s] Underline[%d%s] StrikeOut[%d%s] SubScript[%d%s] SuperScript[%d%s]\n",
				 getVal("font-family").c_str(),			((m_bChangedFontFamily) ? "(chg)" : ""),
				 getVal("font-size").c_str(),			((m_bChangedFontSize) ? "(chg)" : ""),
				 getVal("font-weight").c_str(),			((m_bChangedFontWeight) ? "(chg)" : ""),
				 getVal("font-style").c_str(),			((m_bChangedFontStyle) ? "(chg)" : ""),
				 getVal("color").c_str(),				((m_bChangedColor) ? "(chg)" : ""),
				 m_bUnderline,							((m_bChangedUnderline) ? "(chg)" : ""),
				 m_bStrikeout,							((m_bChangedStrikeOut) ? "(chg)" : ""),
				 m_bSubScript,							((m_bChangedSubScript) ? "(chg)" : ""),
				 m_bSuperScript,						((m_bChangedSuperScript) ? "(chg)" : "")
	            ));

	// answer should be set by the appropriate callback
	// the caller can get the answer from getAnswer().

	m_pFrame = nullptr;
}

void XAP_UnixDialog_FontChooser::updatePreview(void)
{
	// if we don't have anything yet, just ignore this request
	if (!m_gc)
		return;
	// if a font has been set since this dialog was launched, draw things with it
	if (m_doneFirstFont)
	{
		const UT_UCS4Char * entryString = getDrawString ();

		if (!entryString) {
			return;
        }

		event_previewInvalidate(entryString);
	} else {
		event_previewClear();
	}
}
