/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:t -*- */
/* AbiWord
 * Copyright (C) 2026 AbiSource, Inc.
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

#include <climits>
#endif

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "ap_UnixRibbon.h"

#include "ut_vector.h"
#include "ut_debugmsg.h"
#include "ut_string.h"
#include "ut_string_class.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"
#include "xap_Strings.h"
#include "xap_EncodingManager.h"
#include "xap_GtkUtils.h"
#include "ev_UnixMenuBar.h"
#include "ev_UnixFontCombo.h"
#include "ev_Menu_Labels.h"
#include "ev_Menu_Actions.h"
#include "ev_EditMethod.h"
#include "ev_Toolbar_Actions.h"
#include "ev_Toolbar_Labels.h"
#include "xap_Toolbar_LabelSet.h"
#include "ap_Toolbar_Id.h"
#include "ap_Prefs_SchemeIds.h"
#include "ap_UnixStockIcons.h"
#include "ap_UnixStylesPane.h"
#include "ap_UnixFrameImpl.h"
#include "ap_Ribbon_Layouts.h"
#include "gr_CairoGraphics.h"
#include "gr_MathTypesetter.h"
#include "pt_PieceTable.h"
#include "pd_Document.h"
#include "pd_Style.h"
#include "pp_Property.h"
#include "pp_AttrProp.h"
#include "fp_PageSize.h"
#include "ut_units.h"
#include "fl_DocLayout.h"
#include "ap_UnixDialog_Document.h"
#include "fv_View.h"
#include "ie_impGraphic.h"
#include "fg_Graphic.h"

/*
 * Tab and group titles.  The layout table stores untranslated keys;
 * these English titles are the ribbon's labels.  (Localizing them is
 * future work - they are not in the .strings machinery yet.)
 */
struct _ribbon_kv { const char * szKey; const char * szLabel; };

static const _ribbon_kv s_ribbon_tab_labels[] =
{
	{ "file",       "File" },
	{ "home",       "Home" },
	{ "insert",     "Insert" },
	{ "references", "References" },
	{ "layout",     "Layout" },
	{ "review",     "Review" },
	{ "view",       "View" },
	{ "table",      "Table" },
	{ "equation",   "Equation" },
	{ "help",       "Help" },
	{ nullptr,       nullptr }
};

static const _ribbon_kv s_ribbon_group_labels[] =
{
	{ "document",    "Document" },
	{ "print",       "Print" },
	{ "clipboard",   "Clipboard" },
	{ "font",        "Font" },
	{ "paragraph",   "Paragraph" },
	{ "lists",       "Lists" },
	{ "styles",      "Styles" },
	{ "editing",     "Editing" },
	{ "pages",       "Pages" },
	{ "tables",      "Tables" },
	{ "illustrations","Illustrations" },
	{ "links",       "Links" },
	{ "comments",    "Comments" },
	{ "headerfooter","Header & Footer" },
	{ "text",        "Text" },
	{ "symbols",     "Symbols" },
	{ "fields",      "Fields" },
	{ "media",       "Media" },
	{ "toc",         "Table of Contents" },
	{ "notes",       "Footnotes" },
	{ "citations",   "Citations & Bibliography" },
	{ "captions",    "Captions" },
	{ "index",       "Index" },
	{ "authorities", "Table of Authorities" },
	{ "views",       "Views" },
	{ "page",        "Page Setup" },
	{ "columns",     "Page Columns" },
	{ "indent",      "Indent" },
	{ "spacing",     "Spacing" },
	{ "arrange",     "Arrange" },
	{ "background",  "Page Background" },
	{ "zoom",        "Zoom" },
	{ "proofing",    "Proofing" },
	{ "language",    "Language" },
	{ "tracking",    "Tracking" },
	{ "compare",     "Compare" },
	{ "ink",         "Ink" },
	{ "revisions",   "Revisions" },
	{ "annotations", "Annotations" },
	{ "show",        "Show" },
	{ "window",      "Window" },
	{ "insert",      "Insert" },
	{ "delete",      "Delete" },
	{ "select",      "Select" },
	{ "format",      "Format" },
	{ "equation",    "Equation" },
	{ "structures",  "Structures" },
	{ "help",        "Help" },
	{ "interface",   "Interface" },
	{ nullptr,        nullptr }
};

static const char * _ribbon_label(const char * szKey,
								  const _ribbon_kv * table)
{
	for (; table->szKey; ++table)
		if (strcmp(table->szKey, szKey) == 0)
			return table->szLabel;
	return szKey;
}

/* strip mnemonic markers (& and _) from a menu label */
static void _ribbon_strip_mnemonic(const char * szIn, char * szOut, size_t outSize)
{
	size_t o = 0;
	for (const char * p = szIn; *p && o + 1 < outSize; ++p)
	{
		if ((*p == '&' || *p == '_') && p[1])
			continue;
		szOut[o++] = *p;
	}
	szOut[o] = '\0';
}

/*
 * Per-toolbar-item bookkeeping: the widget, the toolbar id, and a
 * flag that suppresses signal handling while refresh() pokes the
 * widget's state (same role as _wd::m_blockSignal in ev_UnixToolbar).
 */
struct AP_UnixRibbon::_TbCtx
{
	AP_UnixRibbon *	self;
	XAP_Toolbar_Id	id;
	GtkWidget *		widget;
	bool			blockSignal;
	gulong			handlerId;
};

/* forward decl: drawn page-glyph icons for the Layout popovers */
static GtkWidget * _layout_icon(XAP_Menu_Id id, int w, int h);
static bool _has_drawn_icon(XAP_Menu_Id id);

AP_UnixRibbon::AP_UnixRibbon(XAP_Frame * pFrame, EV_UnixMenuBar * pMenu)
	: m_pFrame(pFrame)
	, m_pMenu(pMenu)
	, m_wNotebook(nullptr)
	, m_pTBLabels(nullptr)
	, m_wStyleBox(nullptr)
	, m_wStyleScroll(nullptr)
	, m_wStylePrev(nullptr)
	, m_wStyleNext(nullptr)
	, m_pIconMap(nullptr)
	, m_pMarkupLabel(nullptr)
	, m_bSpinUpdating(false)
{
}

AP_UnixRibbon::~AP_UnixRibbon()
{
	// m_wNotebook is owned by the widget tree; nothing to unref here.
	g_clear_pointer(&m_pIconMap, g_hash_table_unref);
	DELETEP(m_pTBLabels);
	UT_VECTOR_PURGEALL(_SpinField *, m_vecSpins);
	UT_VECTOR_PURGEALL(_TbCtx *, m_vecTbCtx);
	for (UT_sint32 i = 0; i < m_vecStyleTiles.getItemCount(); ++i)
	{
		_StyleTile * t = m_vecStyleTiles.getNthItem(i);
		g_free(t->styleName);
		delete t;
	}
}

/*
 * Toolbar icons are keyed by toolbar id, while the ribbon is built
 * from menu ids.  Both action sets name the same edit methods, so we
 * bridge them: for every toolbar id, map its edit-method name to the
 * icon the classic toolbar would show.  Ribbon buttons then look up
 * their icon by the menu action's method name.  The toolbar label set
 * is also kept for toolbar-backed items (labels, tooltips, icons).
 */
void AP_UnixRibbon::_buildIconMap()
{
	if (m_pIconMap)
		return;
	m_pIconMap = g_hash_table_new_full(g_str_hash, g_str_equal,
									   g_free, g_free);

	const EV_Toolbar_ActionSet * pTBActions =
		XAP_App::getApp()->getToolbarActionSet();
	if (!pTBActions)
		return;

	/* the label-set language follows the StringSet pref, the same way
	 * xap_Frame::initialize picks m_szToolbarLabelSetName */
	std::string lang;
	if (!XAP_App::getApp()->getPrefsValue(AP_PREF_KEY_StringSet, lang) ||
		lang.empty())
		lang = AP_PREF_DEFAULT_StringSet;
	m_pTBLabels = AP_CreateToolbarLabelSet(lang.c_str());
	if (!m_pTBLabels)
		return;

	for (UT_uint32 tid = 1; tid < (UT_uint32)AP_TOOLBAR_ID__BOGUS2__; ++tid)
	{
		EV_Toolbar_Action * pTBAction =
			pTBActions->getAction((XAP_Toolbar_Id)tid);
		EV_Toolbar_Label * pTBLabel =
			m_pTBLabels->getLabel((XAP_Toolbar_Id)tid);
		if (!pTBAction || !pTBLabel)
			continue;
		const char * szMethod = pTBAction->getMethodName();
		const char * szIcon = pTBLabel->getIconName();
		if (!szMethod || !*szMethod || !szIcon ||
			g_ascii_strcasecmp(szIcon, "NoIcon") == 0)
			continue;
		if (!g_hash_table_contains(m_pIconMap, szMethod))
			g_hash_table_insert(m_pIconMap, g_strdup(szMethod),
								abi_stock_from_toolbar_id(szIcon));
	}
}

GtkWidget * AP_UnixRibbon::createWidget()
{
	m_wNotebook = gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(m_wNotebook), TRUE);
	gtk_widget_add_css_class(m_wNotebook, "abiword-ribbon");
	gtk_widget_set_vexpand(m_wNotebook, FALSE);
	gtk_widget_set_hexpand(m_wNotebook, TRUE);
	gtk_widget_set_valign(m_wNotebook, GTK_ALIGN_START);

	/* keep the band compact: small flat-ish buttons, tight frames */
	static const char ribbon_css[] =
		".abiword-ribbon button { min-height: 0; padding: 3px 10px; }"
		".abiword-ribbon flowboxchild { padding: 0; }"
		".abiword-ribbon flowbox { padding: 2px; }"
		/* Word-style group: subtle box, title centered at the bottom */
		".abiword-ribbon .ribbon-group {"
		"  margin: 2px 3px; padding: 2px 4px 0 4px;"
		"}"
		".abiword-ribbon separator { margin: 6px 0; }"
		/* visible group separator - a real 1px line, not theme-drawn */
		".abiword-ribbon separator.ribbon-group-sep {"
		"  min-width: 0; min-height: 0; margin: 8px 2px;"
		"  border-left: 1px solid alpha(@theme_fg_color, 0.22);"
		"}"
		/* File ▸ Close - red glyph like Word's destructive controls;
		 * only the symbolic icon picks up the colour */
		".abiword-ribbon image.ribbon-close {"
		"  color: @error_color;"
		"}"
		".abiword-ribbon .ribbon-group-title {"
		"  font-size: 0.78em; margin-top: 1px; padding: 0 4px 2px 4px;"
		"  color: alpha(@theme_fg_color, 0.75);"
		"}"
		/* Word keeps ribbon captions a notch below the document font;
		 * smaller button labels keep wide tabs (References) inside
		 * the window instead of being squeezed to their minimum */
		".abiword-ribbon .ribbon-group button label,"
		".abiword-ribbon .ribbon-group menubutton label {"
		"  font-size: 0.88em;"
		"}"
		/* compact +/- on the Layout tab's indent/spacing spins */
		".abiword-ribbon spinbutton.ribbon-spin button {"
		"  min-width: 0; min-height: 0; padding: 0 3px; margin: 0;"
		"}"
		".abiword-ribbon combobox, .abiword-ribbon dropdown { margin: 1px 2px; }"
		".abiword-ribbon notebook > header { margin-bottom: 0; }"
		/* Word-style style gallery tiles */
		".abiword-ribbon scrolledwindow { min-height: 0; }"
		/* style-gallery nav arrows: zero horizontal padding so an
		 * under-allocated button can never push its icon into a
		 * negative allocation when the ribbon is squeezed */
		".abiword-ribbon button.ribbon-nav {"
		"  min-width: 0; min-height: 0; padding: 3px 0;"
		"}"
		".abiword-ribbon .abiword-style-tile {"
		"  min-height: 26px; padding: 4px 12px; margin: 1px;"
		"}"
		".abiword-ribbon .abiword-style-active {"
		"  border: 2px solid @theme_selected_bg_color;"
		"  border-radius: 4px;"
		"  padding: 1px 8px;"
		"}";
	GtkCssProvider * css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(css, ribbon_css, -1);
	gtk_style_context_add_provider_for_display(
		gtk_widget_get_display(m_wNotebook),
		GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(css);

	/* bind the same per-window action group the menubar uses, so
	 * ribbon buttons resolve "menu.<name>" identically */
	gtk_widget_insert_action_group(m_wNotebook, "menu",
								   m_pMenu->getActionGroup());

	_buildIconMap();

	for (const AP_RibbonTab * tab = s_ribbon_tabs; tab->szTabKey; ++tab)
	{
		GtkWidget * page = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_widget_set_valign(page, GTK_ALIGN_START);

		for (const AP_RibbonGroup * group = tab->groups; group->szGroupKey; ++group)
		{
			/* Word/NotebookBar group: borderless box with the group
			 * title centered at the bottom */
			GtkWidget * frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
			gtk_widget_add_css_class(frame, "ribbon-group");
			/* LibreOffice-style columns of 3 rows: group height stays
			 * constant, extra items wrap into more columns */
			GtkWidget * grid = gtk_grid_new();
			gtk_grid_set_column_spacing(GTK_GRID(grid), 2);
			gtk_widget_set_valign(grid, GTK_ALIGN_START);
			gtk_widget_set_vexpand(grid, TRUE);
			/* row-major groups (Font) use a vertical box of horizontal
			 * rows instead - shared grid columns would stretch narrow
			 * glyph buttons to the font combo's width */
			GtkWidget * rowBox = nullptr;
			GtkWidget * curRow = nullptr;
			GtkWidget * evenBox = nullptr;
			bool bEmpty = true;
			/* a group containing ROWEND markers packs row-major
			 * (left to right, wrapping at each ROWEND) instead of
			 * the default 3-row column packing */
			bool bRowMajor = false;
			/* groups whose items are all LARGE (Word's Footnotes /
			 * Index / Table of Authorities rows) get homogeneous grid
			 * columns so every button comes out the same size */
			bool bAllLarge = true;
			for (const AP_RibbonItem * it = group->items;
				 !(it->kind == AP_RIBBON_ITEM_MENU &&
				   it->id == (uint16_t)AP_MENU_ID__BOGUS1__); ++it)
			{
				if (it->kind == AP_RIBBON_ITEM_ROWEND)
				{
					bRowMajor = true;
					break;
				}
				if (!(it->flags & AP_RIBBON_FLAG_LARGE))
					bAllLarge = false;
			}
			if (!bRowMajor && bAllLarge)
				gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
			if (bRowMajor)
			{
				rowBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
				gtk_widget_set_valign(rowBox, GTK_ALIGN_CENTER);
				gtk_widget_set_vexpand(rowBox, TRUE);
			}

			int nCol = 0, nRow = 0;
			for (const AP_RibbonItem * item = group->items;
				 !(item->kind == AP_RIBBON_ITEM_MENU &&
				   item->id == (uint16_t)AP_MENU_ID__BOGUS1__); ++item)
			{
				if (item->kind == AP_RIBBON_ITEM_ROWEND)
				{
					curRow = nullptr;
					evenBox = nullptr;
					continue;
				}
				GtkWidget * w = nullptr;
				if (item->kind == AP_RIBBON_ITEM_STYLEGAL)
					w = _makeStyleGallery();
				else if (item->kind == AP_RIBBON_ITEM_EQSYMBOLS)
					w = _makeEquationPalette(false);
				else if (item->kind == AP_RIBBON_ITEM_EQSTRUCT)
					w = _makeEquationStructures();
				else if (item->kind == AP_RIBBON_ITEM_SPIN)
					w = _makeSpinField(item->id);
				else if (item->kind == AP_RIBBON_ITEM_DEAD)
					w = _makeDeadButton(item->id);
				else if (item->flags & AP_RIBBON_FLAG_MENUPOP)
				{
					if (item->kind == AP_RIBBON_ITEM_TOOLBAR)
						w = _makeMenuPopTbButton((XAP_Toolbar_Id)item->id,
												 item->flags);
					else
						w = _makeMenuPopButton((XAP_Menu_Id)item->id,
											   item->flags);
				}
				else if (item->kind == AP_RIBBON_ITEM_TOOLBAR)
					w = _makeToolbarWidget((XAP_Toolbar_Id)item->id,
										   item->flags);
				else
					w = _makeButton((XAP_Menu_Id)item->id, item->flags);
				if (!w)
					continue;
				/* FMT_STYLE stays registered for toolbar-state updates
				 * (gallery highlight + Styles pane) but is not shown */
				if (item->kind == AP_RIBBON_ITEM_TOOLBAR &&
					item->id == (uint16_t)AP_TOOLBAR_ID_FMT_STYLE)
					gtk_widget_set_visible(w, FALSE);
				bEmpty = false;

				/* SPLIT items gain a small drop-arrow menu button
				 * opening a popover with the related choices */
				if (item->flags & AP_RIBBON_FLAG_SPLIT)
				{
					GtkWidget * popover;
					if (item->kind == AP_RIBBON_ITEM_MENU &&
						item->id == (uint16_t)AP_MENU_ID_EDIT_PASTE)
						popover = _makePastePopover();
					else if (item->kind == AP_RIBBON_ITEM_MENU &&
							 item->id == (uint16_t)AP_MENU_ID_FMT_TOGGLECASE)
						popover = _makeChangeCasePopover();
					else
						popover = _makeListPopover(
							(XAP_Toolbar_Id)item->id);
					w = _wrapSplit(w, popover,
								   (item->flags & AP_RIBBON_FLAG_LARGE) != 0);
				}

				if (bRowMajor)
				{
					if (!curRow)
					{
						curRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
						gtk_widget_set_valign(curRow, GTK_ALIGN_CENTER);
						gtk_box_append(GTK_BOX(rowBox), curRow);
						evenBox = nullptr;
					}
					/* EVEN items share a homogeneous box so they all
					 * get the same width (grow/shrink font, Change Case) */
					if (item->flags & AP_RIBBON_FLAG_EVEN)
					{
						if (!evenBox)
						{
							evenBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
							gtk_box_set_homogeneous(GTK_BOX(evenBox), TRUE);
							gtk_box_append(GTK_BOX(curRow), evenBox);
						}
						gtk_box_append(GTK_BOX(evenBox), w);
					}
					else
					{
						evenBox = nullptr;
						gtk_box_append(GTK_BOX(curRow), w);
					}
					continue;
				}

				/* combos, large buttons and the style gallery get a
				 * full-height column to themselves; plain buttons pack
				 * 3 rows per column, LibreOffice-style */
				bool bTall = (item->flags & AP_RIBBON_FLAG_LARGE) ||
					(item->kind == AP_RIBBON_ITEM_STYLEGAL) ||
					(item->kind == AP_RIBBON_ITEM_EQSTRUCT) ||
					((item->kind == AP_RIBBON_ITEM_TOOLBAR) &&
					 (item->id == AP_TOOLBAR_ID_FMT_FONT ||
					  item->id == AP_TOOLBAR_ID_FMT_SIZE ||
					  item->id == AP_TOOLBAR_ID_FMT_STYLE ||
					  item->id == AP_TOOLBAR_ID_ZOOM));
				if (bTall)
				{
					if (nRow != 0) { ++nCol; nRow = 0; }
					gtk_grid_attach(GTK_GRID(grid), w, nCol, 0, 1, 3);
					++nCol;
				}
				else
				{
					gtk_grid_attach(GTK_GRID(grid), w, nCol, nRow, 1, 1);
					if (++nRow == 3) { nRow = 0; ++nCol; }
				}
			}

			if (bEmpty)
			{
				gtk_widget_set_visible(frame, FALSE);
			}
			else
			{
				gtk_box_append(GTK_BOX(frame), rowBox ? rowBox : grid);
				GtkWidget * gtitle = gtk_label_new(
					_ribbon_label(group->szGroupKey,
								  s_ribbon_group_labels));
				gtk_widget_add_css_class(gtitle, "ribbon-group-title");
				gtk_widget_set_halign(gtitle, GTK_ALIGN_CENTER);
				gtk_box_append(GTK_BOX(frame), gtitle);
			}
			if (!bEmpty)
			{
				gtk_box_append(GTK_BOX(page), frame);
				/* LibreOffice-style vertical separator between groups;
				 * the last group gets none */
				if (group[1].szGroupKey)
				{
					GtkWidget * sep = gtk_separator_new(
						GTK_ORIENTATION_VERTICAL);
					gtk_widget_add_css_class(sep, "ribbon-group-sep");
					gtk_box_append(GTK_BOX(page), sep);
				}
			}
		}

		GtkWidget * tabLabel = gtk_label_new(_ribbon_label(tab->szTabKey,
														 s_ribbon_tab_labels));
		gtk_notebook_append_page(GTK_NOTEBOOK(m_wNotebook), page, tabLabel);
		g_object_set_data(G_OBJECT(page), "abi-tab-key",
						  (gpointer)tab->szTabKey);

		if (!strcmp(tab->szTabKey, "home"))
			gtk_notebook_set_current_page(GTK_NOTEBOOK(m_wNotebook),
										  gtk_notebook_get_n_pages(
											  GTK_NOTEBOOK(m_wNotebook)) - 1);

		if (tab->bContextual)
		{
			m_vecContextualPages.addItem(page);
			g_object_set_data(G_OBJECT(page), "abi-ctx-key",
							  (gpointer)tab->szTabKey);
			gtk_widget_set_visible(page, FALSE);
		}
	}

	g_signal_connect(m_wNotebook, "switch-page",
					 G_CALLBACK(_s_switch_page), this);

	GtkEventController * motion = gtk_event_controller_motion_new();
	g_signal_connect(motion, "enter", G_CALLBACK(_s_motion_enter), this);
	gtk_widget_add_controller(m_wNotebook, motion);

	return m_wNotebook;
}

/* shared CSS removing the theme's wide default button padding so
 * SLIM glyph buttons pack tighter */
static GtkCssProvider * _slimButtonCss()
{
	static GtkCssProvider * p = nullptr;
	if (!p)
	{
		p = gtk_css_provider_new();
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		gtk_css_provider_load_from_string(p,
			"button { padding-left: 4px; padding-right: 4px;"
			" min-width: 0px; }"
			"entry { padding-left: 2px; padding-right: 2px; }"
			"combobox button { padding-left: 0px;"
			" padding-right: 0px; }");
G_GNUC_END_IGNORE_DEPRECATIONS
	}
	return p;
}

/* context providers do not reach a widget's internal children (the
 * arrow button inside a GtkComboBox, the toggle inside a
 * GtkMenuButton) - walk the tree and slim buttons/entries directly */
static void _slim_widget_tree(GtkWidget * w)
{
	gtk_style_context_add_provider(
		gtk_widget_get_style_context(w),
		GTK_STYLE_PROVIDER(_slimButtonCss()),
		GTK_STYLE_PROVIDER_PRIORITY_USER);
	for (GtkWidget * c = gtk_widget_get_first_child(w); c;
		 c = gtk_widget_get_next_sibling(c))
		_slim_widget_tree(c);
}

GtkWidget * AP_UnixRibbon::_makeButton(XAP_Menu_Id id, uint8_t flags)
{
	const EV_Menu_ActionSet * pActionSet =
		XAP_App::getApp()->getMenuActionSet();
	UT_return_val_if_fail(pActionSet, nullptr);

	const EV_Menu_Action * pAction = pActionSet->getAction(id);
	const EV_Menu_Label * pLabel = m_pMenu->getLabelSet()->getLabel(id);
	if (!pAction || !pLabel)
		return nullptr;

	const char * szLabel = pAction->hasDynamicLabel()
		? pAction->getDynamicLabel(pLabel)
		: pLabel->getMenuLabel();
	if (!szLabel || !*szLabel)
		return nullptr;

	/* ribbon-only items (Selection Pane, ...) are not part of the
	 * classic menubar layout, so their action does not exist yet -
	 * create it on demand */
	GAction * action = m_pMenu->lookupAction(id);
	if (!action)
		action = m_pMenu->ensureAction(id);
	if (!action)
		return nullptr;

	char detailed[64];
	g_snprintf(detailed, sizeof(detailed), "menu.%s", g_action_get_name(action));

	char label[256];
	_ribbon_strip_mnemonic(szLabel, label, sizeof(label));

	GtkWidget * btn;
	if (pAction->isCheckable() || pAction->isRadio())
		btn = gtk_toggle_button_new();
	else
		btn = gtk_button_new();



	/* reuse the classic toolbar's icon for this edit method, if any;
	 * fall back to the menu action's own stock-icon mapping */
	const char * szMethod = pAction->getMethodName();
	const char * szIcon = (szMethod && m_pIconMap)
		? static_cast<const char *>(g_hash_table_lookup(m_pIconMap,
														szMethod))
		: nullptr;
	if (!szIcon || !*szIcon)
		szIcon = abi_stock_from_menu_id(id);
	/* ids with a drawn ribbon glyph (References tab) still get an
	 * icon when the theme provides none */
	bool bDrawnIcon = (!szIcon || !*szIcon) && _has_drawn_icon(id);

	/* GLYPH buttons draw the LibreOffice-style text glyph
	 * (bold B, italic I, underlined U, x², A⁺) instead of an icon */
	if (flags & AP_RIBBON_FLAG_GLYPH)
	{
		const char * markup = nullptr;
		switch (id)
		{
		case AP_MENU_ID_FMT_BOLD:       markup = "<b>B</b>"; break;
		case AP_MENU_ID_FMT_ITALIC:     markup = "<i>I</i>"; break;
		case AP_MENU_ID_FMT_UNDERLINE:  markup = "<u>U</u>"; break;
		case AP_MENU_ID_FMT_STRIKE:     markup = "<s>S</s>"; break;
		case AP_MENU_ID_FMT_OVERLINE:   markup = "<span overline='single'>O</span>"; break;
		case AP_MENU_ID_FMT_SUPERSCRIPT:markup = "x<sup>2</sup>"; break;
		case AP_MENU_ID_FMT_SUBSCRIPT:  markup = "x<sub>2</sub>"; break;
		case AP_MENU_ID_FMT_GROWFONT:   markup = "A<sup>+</sup>"; break;
		case AP_MENU_ID_FMT_SHRINKFONT: markup = "A<sup>&#x2212;</sup>"; break;
		case AP_MENU_ID_FMT_TOGGLECASE: markup = "Aa"; break;
		default: break;
		}
		if (markup)
		{
			GtkWidget * gl = gtk_label_new(nullptr);
			gtk_label_set_markup(GTK_LABEL(gl), markup);
			gtk_widget_set_valign(gl, GTK_ALIGN_CENTER);
			gtk_button_set_child(GTK_BUTTON(btn), gl);
			/* SLIM glyph buttons lose the theme's wide default
			 * padding so they pack tighter (grow/shrink font) */
			if (flags & AP_RIBBON_FLAG_SLIM)
				gtk_style_context_add_provider(
					gtk_widget_get_style_context(btn),
					GTK_STYLE_PROVIDER(_slimButtonCss()),
					GTK_STYLE_PROVIDER_PRIORITY_USER);
			gtk_actionable_set_action_name(GTK_ACTIONABLE(btn), detailed);
			if (pAction->isRadio())
			{
				char target[32];
				g_snprintf(target, sizeof(target), "%u",
						   static_cast<unsigned>(id));
				gtk_actionable_set_action_target_value(
					GTK_ACTIONABLE(btn),
					g_variant_new_string(target));
			}
			const char * szStatus2 = pLabel->getMenuStatusMessage();
			if (szStatus2 && *szStatus2 && strcmp(szStatus2, " ") != 0)
				gtk_widget_set_tooltip_text(btn, szStatus2);
			return btn;
		}
		/* unknown glyph id - fall through to the icon/label path */
	}

	if (szIcon && *szIcon && (flags & AP_RIBBON_FLAG_ICONONLY) &&
		!gtk_icon_theme_has_icon(
			gtk_icon_theme_get_for_display(gdk_display_get_default()),
			szIcon))
	{
		szIcon = nullptr;	/* theme lacks the icon - use the text label */
		bDrawnIcon = _has_drawn_icon(id);
	}

	if (((szIcon && *szIcon) || bDrawnIcon) &&
		(flags & AP_RIBBON_FLAG_ICONONLY))
	{
		/* Word-style compact glyph button: icon only, the name lives
		 * in the tooltip */
		GtkWidget * image = bDrawnIcon
			? _layout_icon(id, 18, 18)
			: gtk_image_new_from_icon_name(szIcon);
		gtk_widget_set_valign(image, GTK_ALIGN_CENTER);
		gtk_button_set_child(GTK_BUTTON(btn), image);
	}
	else if (((szIcon && *szIcon) || bDrawnIcon) &&
			 (flags & AP_RIBBON_FLAG_LARGE))
	{
		/* Word-style large button: icon on top, caption underneath;
		 * the background and references buttons use drawn
		 * page-glyphs, the rest a stock/theme icon */
		GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		GtkWidget * image;
		if (bDrawnIcon ||
			id == static_cast<XAP_Menu_Id>(AP_MENU_ID_FMT_BACKGROUND_PAGE_COLOR) ||
			id == static_cast<XAP_Menu_Id>(AP_MENU_ID_FMT_BACKGROUND_PAGE_IMAGE))
		{
			image = _layout_icon(id, 24, 24);
		}
		else
		{
			image = gtk_image_new_from_icon_name(szIcon);
			gtk_image_set_pixel_size(GTK_IMAGE(image), 24);
		}
		gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
		/* Close: red icon only, button face and label stay normal */
		if (id == static_cast<XAP_Menu_Id>(AP_MENU_ID_FILE_CLOSE))
			gtk_widget_add_css_class(image, "ribbon-close");
		/* Word breaks large-button captions onto two lines
		 * ("Cover\nPage", "Blank\nPage") */
		char caption[256];
		strncpy(caption, label, sizeof(caption) - 1);
		caption[sizeof(caption) - 1] = 0;
		char * sp = strchr(caption, ' ');
		if (sp)
			*sp = '\n';
		GtkWidget * wLabel = gtk_label_new(caption);
		/* Word wraps long captions onto a second line rather than
		 * ellipsizing ("Document Properties", "New using Template") */
		gtk_label_set_wrap(GTK_LABEL(wLabel), TRUE);
		gtk_label_set_wrap_mode(GTK_LABEL(wLabel), PANGO_WRAP_WORD);
		gtk_label_set_justify(GTK_LABEL(wLabel), GTK_JUSTIFY_CENTER);
		gtk_label_set_lines(GTK_LABEL(wLabel), 2);
		gtk_label_set_max_width_chars(GTK_LABEL(wLabel),
			(flags & AP_RIBBON_FLAG_SLIM) ? 10 : 12);
		gtk_box_append(GTK_BOX(box), image);
		gtk_box_append(GTK_BOX(box), wLabel);
		gtk_button_set_child(GTK_BUTTON(btn), box);
		if (flags & AP_RIBBON_FLAG_SLIM)
			gtk_style_context_add_provider(
				gtk_widget_get_style_context(btn),
				GTK_STYLE_PROVIDER(_slimButtonCss()),
				GTK_STYLE_PROVIDER_PRIORITY_USER);
	}
	else
	{
		GtkWidget * wLabel = gtk_label_new(label);
		/* LARGE items without a stock icon still fill the group
		 * height - let the caption wrap instead of ellipsizing */
		if (flags & AP_RIBBON_FLAG_LARGE)
		{
			gtk_label_set_wrap(GTK_LABEL(wLabel), TRUE);
			gtk_label_set_wrap_mode(GTK_LABEL(wLabel), PANGO_WRAP_WORD);
			gtk_label_set_justify(GTK_LABEL(wLabel), GTK_JUSTIFY_CENTER);
			gtk_label_set_max_width_chars(GTK_LABEL(wLabel), 16);
		}
		else if ((szIcon && *szIcon) || bDrawnIcon)
		{
			/* small icon+label buttons are single-line like Word's
			 * compact ribbon rows; ellipsize rather than wrap so a
			 * squeezed group never collapses into one-char columns */
			gtk_label_set_ellipsize(GTK_LABEL(wLabel),
									PANGO_ELLIPSIZE_END);
			gtk_label_set_max_width_chars(GTK_LABEL(wLabel), 16);
		}
		else
		{
			gtk_label_set_ellipsize(GTK_LABEL(wLabel), PANGO_ELLIPSIZE_END);
			gtk_label_set_max_width_chars(GTK_LABEL(wLabel), 18);
		}

		if ((szIcon && *szIcon) || bDrawnIcon)
		{
			GtkWidget * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
			GtkWidget * image = bDrawnIcon
				? _layout_icon(id, 16, 16)
				: gtk_image_new_from_icon_name(szIcon);
			gtk_widget_set_valign(image, GTK_ALIGN_CENTER);
			gtk_box_append(GTK_BOX(box), image);
			gtk_box_append(GTK_BOX(box), wLabel);
			gtk_button_set_child(GTK_BUTTON(btn), box);
		}
		else
		{
			gtk_widget_set_valign(wLabel, GTK_ALIGN_CENTER);
			gtk_button_set_child(GTK_BUTTON(btn), wLabel);
		}
	}

	gtk_actionable_set_action_name(GTK_ACTIONABLE(btn), detailed);
	if (pAction->isRadio())
	{
		char target[32];
		g_snprintf(target, sizeof(target), "%u", static_cast<unsigned>(id));
		gtk_actionable_set_action_target_value(GTK_ACTIONABLE(btn),
											   g_variant_new_string(target));
	}

	const char * szStatus = pLabel->getMenuStatusMessage();
	if (szStatus && *szStatus && strcmp(szStatus, " ") != 0)
		gtk_widget_set_tooltip_text(btn, szStatus);

	return btn;
}

/*****************************************************************/
/* Toolbar-backed items: combos, color pickers and plain buttons
 * that dispatch through the toolbar action set's edit methods,
 * exactly like the classic toolbar does.
 */

void AP_UnixRibbon::_invokeToolbarItem(XAP_Toolbar_Id id,
									   const UT_UCS4Char * pData,
									   UT_uint32 dataLength)
{
	const EV_Toolbar_ActionSet * pTBActions =
		XAP_App::getApp()->getToolbarActionSet();
	UT_return_if_fail(pTBActions);

	EV_Toolbar_Action * pAction = pTBActions->getAction(id);
	UT_return_if_fail(pAction);

	AV_View * pView = m_pFrame ? m_pFrame->getCurrentView() : nullptr;

	/* ignore presses on "down" group buttons (same rule as
	 * EV_UnixToolbar::toolbarEvent) */
	if (pAction->getItemType() == EV_TBIT_GroupButton && pView)
	{
		const char * szState = nullptr;
		EV_Toolbar_ItemState tis = pAction->getToolbarItemState(pView, &szState);
		if (EV_TIS_ShouldBeToggled(tis))
			return;
	}

	const char * szMethodName = pAction->getMethodName();
	UT_return_if_fail(szMethodName);

	const EV_EditMethodContainer * pEMC =
		XAP_App::getApp()->getEditMethodContainer();
	UT_return_if_fail(pEMC);

	EV_EditMethod * pEM = pEMC->findEditMethodByName(szMethodName);
	UT_return_if_fail(pEM);

	EV_EditMethodType t = pEM->getType();
	if (((t & EV_EMT_REQUIREDATA) != 0) && (!pData || !dataLength))
		return;

	EV_EditMethodCallData emcd(pData, dataLength);
	pEM->Fn(pView, &emcd);
}

void AP_UnixRibbon::_s_tb_clicked(GtkWidget * w, gpointer data)
{
	_TbCtx * ctx =
		static_cast<_TbCtx *>(data);
	UT_return_if_fail(ctx && ctx->self);
	if (ctx->blockSignal)
		return;

	/* a "down" group button cannot be un-pressed by clicking it;
	 * throw the widget's state back and ignore the event (same rule
	 * as EV_UnixToolbar::toolbarEvent) */
	const EV_Toolbar_ActionSet * pTBActions =
		XAP_App::getApp()->getToolbarActionSet();
	EV_Toolbar_Action * pAction =
		pTBActions ? pTBActions->getAction(ctx->id) : nullptr;
	if (pAction && pAction->getItemType() == EV_TBIT_GroupButton &&
		GTK_IS_TOGGLE_BUTTON(w))
	{
		AV_View * pView = ctx->self->m_pFrame
			? ctx->self->m_pFrame->getCurrentView() : nullptr;
		const char * szState = nullptr;
		EV_Toolbar_ItemState tis = pView
			? pAction->getToolbarItemState(pView, &szState)
			: EV_TIS_ZERO;
		if (EV_TIS_ShouldBeToggled(tis))
		{
			bool wasBlocked = ctx->blockSignal;
			ctx->blockSignal = true;
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),
				!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));
			ctx->blockSignal = wasBlocked;
			return;
		}
	}
	ctx->self->_invokeToolbarItem(ctx->id);
}


/* get the combo's active/typed text, like s_combo_apply_changes */
gchar * AP_UnixRibbon::_tb_combo_get_text(GtkComboBox * combo)
{
	if (ABI_IS_FONT_COMBO(combo))
		return abi_font_combo_get_active_text(ABI_FONT_COMBO(combo));

	if (GTK_IS_COMBO_BOX_TEXT(combo))
	{
		gchar * buffer =
			gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
		/* combos with an entry (font size) may hold a value that is
		 * not in the list; the typed value must not be lost */
		if (!buffer)
		{
			GtkWidget * child = gtk_combo_box_get_child(combo);
			if (child && GTK_IS_EDITABLE(child))
			{
				const char * entryText =
					gtk_editable_get_text(GTK_EDITABLE(child));
				if (entryText && *entryText)
					buffer = g_strdup(entryText);
			}
		}
		return buffer;
	}
	return nullptr;
}

void AP_UnixRibbon::_tb_combo_apply(GtkComboBox * combo, _TbCtx * ctx)
{
	UT_return_if_fail(ctx && ctx->self);

	gchar * buffer = _tb_combo_get_text(combo);
	if (!buffer)
		return;

	const char * text = buffer;
	if (ctx->id == (XAP_Toolbar_Id)AP_TOOLBAR_ID_FMT_FONT)
	{
		/* translate the localized size entry back, if it is one */
		const gchar * font =
			XAP_EncodingManager::fontsizes_mapping.lookupByTarget(buffer);
		if (font)
		{
			g_free(buffer);
			buffer = g_strdup(font);
			text = buffer;
		}
	}
	else if (ctx->id == (XAP_Toolbar_Id)AP_TOOLBAR_ID_FMT_STYLE)
	{
		text = pt_PieceTable::s_getUnlocalisedStyleName(buffer);
	}

	if (text)
	{
		UT_UCS4String ucsText(text);
		ctx->self->_invokeToolbarItem(ctx->id, ucsText.ucs4_str(),
									  ucsText.length());
	}
	g_free(buffer);
}

void AP_UnixRibbon::_s_tb_combo_changed(GtkComboBox * combo, gpointer data)
{
	_TbCtx * ctx =
		static_cast<_TbCtx *>(data);
	UT_return_if_fail(ctx);
	if (ctx->blockSignal || !ctx->widget)
		return;

	if (ctx->id == (XAP_Toolbar_Id)AP_TOOLBAR_ID_FMT_SIZE)
	{
		/* no updates of the font size while the entry is being edited */
		GtkWidget * entry = gtk_combo_box_get_child(combo);
		if (entry && gtk_widget_has_focus(entry))
			return;
	}
	_tb_combo_apply(combo, ctx);
}

gboolean AP_UnixRibbon::_s_tb_size_key(GtkEventControllerKey * controller,
							   guint keyval, guint /*keycode*/,
							   GdkModifierType /*state*/, gpointer data)
{
	if (keyval == GDK_KEY_Return)
	{
		GtkWidget * widget =
			gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
		GtkComboBox * combo = GTK_COMBO_BOX(
			gtk_widget_get_ancestor(widget, GTK_TYPE_COMBO_BOX));
		_tb_combo_apply(combo,
						static_cast<_TbCtx *>(data));
	}
	return FALSE;
}

void AP_UnixRibbon::_s_tb_size_focus_out(GtkEventControllerFocus * controller,
								 gpointer data)
{
	GtkWidget * widget =
		gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
	GtkComboBox * combo = GTK_COMBO_BOX(
		gtk_widget_get_ancestor(widget, GTK_TYPE_COMBO_BOX));
	_tb_combo_apply(combo, static_cast<_TbCtx *>(data));
}

/* only accept numeric input in the font size combo */
void AP_UnixRibbon::_s_tb_size_insert_text(GtkEditable * editable,
								   gchar * new_text, gint new_text_length,
								   gint * /*position*/, gpointer /*data*/)
{
	gchar * iter = new_text;
	while (iter < (new_text + new_text_length))
	{
		if (!g_unichar_isdigit(g_utf8_get_char(iter)))
		{
			g_signal_stop_emission_by_name(G_OBJECT(editable), "insert-text");
			return;
		}
		iter = g_utf8_next_char(iter);
	}
}

static void _tb_popdown_popover(GtkWidget * widget)
{
	GtkWidget * popover =
		gtk_widget_get_ancestor(widget, GTK_TYPE_POPOVER);
	if (popover)
		gtk_popover_popdown(GTK_POPOVER(popover));
}

void AP_UnixRibbon::_s_tb_color_activated(GtkColorChooser * cc, GdkRGBA * color,
								  gpointer data)
{
	_TbCtx * ctx =
		static_cast<_TbCtx *>(data);
	UT_return_if_fail(ctx && ctx->self && color);

	UT_UTF8String str = UT_UTF8String_sprintf("%02x%02x%02x",
		static_cast<int>(color->red   * 255),
		static_cast<int>(color->green * 255),
		static_cast<int>(color->blue  * 255));
	_tb_popdown_popover(GTK_WIDGET(cc));
	ctx->self->_invokeToolbarItem(ctx->id,
								  str.ucs4_str().ucs4_str(), str.size());
}

/* one click on a palette swatch applies the colour and closes the
 * popover, like LibreOffice's colour picker */
void AP_UnixRibbon::_s_tb_color_swatch_clicked(GtkWidget * w, gpointer data)
{
	_TbCtx * ctx = static_cast<_TbCtx *>(data);
	const gchar * hex = static_cast<const gchar *>(
		g_object_get_data(G_OBJECT(w), "abi-color-hex"));
	UT_return_if_fail(ctx && ctx->self && hex);

	UT_UTF8String str = hex;
	_tb_popdown_popover(w);
	ctx->self->_invokeToolbarItem(ctx->id,
								  str.ucs4_str().ucs4_str(), str.size());
}

/* "Custom Color…" opens a real dialog with Select/Cancel so the user
 * can always get back; the chosen colour applies on Select */
void AP_UnixRibbon::_s_tb_color_custom_response(GtkDialog * dlg,
												gint response,
												gpointer data)
{
	_TbCtx * ctx = static_cast<_TbCtx *>(data);
	if (response == GTK_RESPONSE_OK && ctx && ctx->self)
	{
		GdkRGBA color;
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dlg), &color);
		UT_UTF8String str = UT_UTF8String_sprintf("%02x%02x%02x",
			static_cast<int>(color.red   * 255),
			static_cast<int>(color.green * 255),
			static_cast<int>(color.blue  * 255));
		ctx->self->_invokeToolbarItem(ctx->id,
									  str.ucs4_str().ucs4_str(), str.size());
	}
	gtk_window_destroy(GTK_WINDOW(dlg));
}

void AP_UnixRibbon::_s_tb_color_custom_clicked(GtkWidget * w, gpointer data)
{
	_TbCtx * ctx = static_cast<_TbCtx *>(data);
	UT_return_if_fail(ctx && ctx->self);
	_tb_popdown_popover(w);

	GtkWidget * toplevel =
		GTK_WIDGET(gtk_widget_get_root(w));
	GtkWidget * dlg = gtk_color_chooser_dialog_new(
		"Custom Color",
		toplevel ? GTK_WINDOW(toplevel) : nullptr);

	/* default to black for font colour, yellow for highlight */
	GdkRGBA initial = { 0.0, 0.0, 0.0, 1.0 };
	if (ctx->id == (XAP_Toolbar_Id)AP_TOOLBAR_ID_COLOR_BACK)
	{
		initial.red = 1.0;
		initial.green = 1.0;
	}
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dlg), &initial);
	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(dlg), FALSE);
	g_signal_connect(dlg, "response",
					 G_CALLBACK(_s_tb_color_custom_response), ctx);
	gtk_window_present(GTK_WINDOW(dlg));
}

GtkWidget * AP_UnixRibbon::_tb_color_swatch(const gchar * hex, _TbCtx * ctx)
{
	GtkWidget * sw = gtk_button_new();
	gtk_widget_set_size_request(sw, 20, 20);
	gtk_widget_set_tooltip_text(sw, hex);

	char css[160];
	g_snprintf(css, sizeof(css),
			   "button { background-image: none; background-color: #%s;"
			   " min-width: 20px; min-height: 20px; padding: 0; }",
			   hex);
	GtkCssProvider * cssProv = gtk_css_provider_new();
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_css_provider_load_from_string(cssProv, css);
G_GNUC_END_IGNORE_DEPRECATIONS
	gtk_style_context_add_provider(gtk_widget_get_style_context(sw),
								   GTK_STYLE_PROVIDER(cssProv),
								   GTK_STYLE_PROVIDER_PRIORITY_USER);
	g_object_unref(cssProv);

	g_object_set_data_full(G_OBJECT(sw), "abi-color-hex",
						   g_strdup(hex), g_free);
	g_signal_connect(sw, "clicked",
					 G_CALLBACK(_s_tb_color_swatch_clicked), ctx);
	return sw;
}

/* compact standard palette, LibreOffice-style: neutrals, vivid,
 * light and dark rows */
static const char * const _s_color_palette[] =
{
	"000000", "444444", "666666", "999999", "bbbbbb",
	"cccccc", "dddddd", "eeeeee", "f7f7f7", "ffffff",
	"ff0000", "ff9900", "ffff00", "00ff00", "00ffff",
	"0000ff", "9900ff", "ff00ff", "e69138", "a67c52",
	"ffcccc", "ffe5cc", "ffffcc", "ccffcc", "ccffff",
	"ccccff", "e5ccff", "ffccff", "f4cccc", "d9d2e9",
	"990000", "b45f06", "bf9000", "38761d", "0c8577",
	"000099", "674ea7", "a64d79", "783f04", "1155cc"
};

void AP_UnixRibbon::_s_tb_color_automatic(GtkWidget * widget, gpointer data)
{
	_TbCtx * ctx =
		static_cast<_TbCtx *>(data);
	UT_return_if_fail(ctx && ctx->self);
	_tb_popdown_popover(widget);

	if (ctx->id == (XAP_Toolbar_Id)AP_TOOLBAR_ID_COLOR_BACK)
	{
		const UT_UCS4Char transparent[] =
			{'t','r','a','n','s','p','a','r','e','n','t',0};
		ctx->self->_invokeToolbarItem(ctx->id, transparent, 10);
	}
	else
	{
		const UT_UCS4Char black[] = {'0','0','0','0','0','0',0};
		ctx->self->_invokeToolbarItem(ctx->id, black, 6);
	}
}

/* menu button + popover color chooser, mirroring abi_color_button_new.
 * szMarkup, when given, is a Pango markup glyph used as the button
 * child instead of an icon - the LibreOffice-style "A with colour
 * bar" / "ab on highlight" look. */
GtkWidget * AP_UnixRibbon::_tb_color_button_new(const gchar * icon_name,
										const gchar * automatic_label,
										_TbCtx * ctx,
										const gchar * szMarkup)
{
	GtkWidget * button = gtk_menu_button_new();
	if (szMarkup && *szMarkup)
	{
		GtkWidget * gl = gtk_label_new(nullptr);
		gtk_label_set_markup(GTK_LABEL(gl), szMarkup);
		gtk_menu_button_set_child(GTK_MENU_BUTTON(button), gl);
	}
	else
		gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(button), icon_name);
	gtk_menu_button_set_direction(GTK_MENU_BUTTON(button), GTK_ARROW_DOWN);
	gtk_menu_button_set_has_frame(GTK_MENU_BUTTON(button), FALSE);

	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_top(box, 6);
	gtk_widget_set_margin_bottom(box, 6);
	gtk_widget_set_margin_start(box, 6);
	gtk_widget_set_margin_end(box, 6);

	if (automatic_label && *automatic_label)
	{
		GtkWidget * automatic = gtk_button_new_with_label(automatic_label);
		g_signal_connect(G_OBJECT(automatic), "clicked",
						 G_CALLBACK(_s_tb_color_automatic), ctx);
		gtk_box_append(GTK_BOX(box), automatic);
	}

	GtkWidget * grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 2);
	for (guint i = 0; i < G_N_ELEMENTS(_s_color_palette); ++i)
	{
		GtkWidget * sw = _tb_color_swatch(_s_color_palette[i], ctx);
		gtk_grid_attach(GTK_GRID(grid), sw, i % 10, i / 10, 1, 1);
	}
	gtk_box_append(GTK_BOX(box), grid);

	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
	GtkWidget * custom = gtk_button_new_with_label(
		(pSS && pSS->getValue(XAP_STRING_ID_TB_CustomColor))
			? pSS->getValue(XAP_STRING_ID_TB_CustomColor)
			: "Custom Color...");
	g_signal_connect(G_OBJECT(custom), "clicked",
					 G_CALLBACK(_s_tb_color_custom_clicked), ctx);
	gtk_box_append(GTK_BOX(box), custom);

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
	return button;
}

/* ------------------------------------------------------------------ */
/* split-button popovers (Word-style drop arrows on Paste / list types) */

struct _PopTbCtx
{
	AP_UnixRibbon *	self;
	XAP_Toolbar_Id	id;
};

void AP_UnixRibbon::_s_popover_tb_clicked(GtkWidget * w, gpointer data)
{
	_PopTbCtx * ctx = static_cast<_PopTbCtx *>(data);
	UT_return_if_fail(ctx && ctx->self);
	_tb_popdown_popover(w);
	ctx->self->_invokeToolbarItem(ctx->id);
}

void AP_UnixRibbon::_s_popover_menu_clicked(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	std::string * pAction = static_cast<std::string *>(
		g_object_get_data(G_OBJECT(w), "abi-menu-action"));
	UT_return_if_fail(self && pAction);
	_tb_popdown_popover(w);
	GActionGroup * group =
		self->m_pMenu ? self->m_pMenu->getActionGroup() : nullptr;
	if (group)
		g_action_group_activate_action(group, pAction->c_str(), nullptr);
}

/* flat labelled button inside a split popover, wired to a menu action */
GtkWidget * AP_UnixRibbon::_popoverMenuButton(XAP_Menu_Id id)
{
	const EV_Menu_ActionSet * pActionSet =
		XAP_App::getApp()->getMenuActionSet();
	const EV_Menu_Action * pAction =
		pActionSet ? pActionSet->getAction(id) : nullptr;
	const EV_Menu_Label * pLabel =
		m_pMenu ? m_pMenu->getLabelSet()->getLabel(id) : nullptr;
	GAction * action = m_pMenu ? m_pMenu->lookupAction(id) : nullptr;
	if (!pAction || !pLabel || !action)
		return nullptr;

	const char * szLabel = pAction->hasDynamicLabel()
		? pAction->getDynamicLabel(pLabel) : pLabel->getMenuLabel();
	if (!szLabel || !*szLabel)
		return nullptr;

	char label[256];
	_ribbon_strip_mnemonic(szLabel, label, sizeof(label));

	GtkWidget * btn = gtk_button_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	const char * szIcon = abi_stock_from_menu_id(id);
	if (szIcon)
		gtk_box_append(GTK_BOX(box), gtk_image_new_from_icon_name(szIcon));
	GtkWidget * wLabel = gtk_label_new(label);
	gtk_widget_set_halign(wLabel, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(box), wLabel);
	gtk_button_set_child(GTK_BUTTON(btn), box);

	g_object_set_data_full(G_OBJECT(btn), "abi-menu-action",
						   new std::string(g_action_get_name(action)),
						   [](gpointer p){ delete static_cast<std::string*>(p); });
	g_signal_connect(btn, "clicked",
					 G_CALLBACK(_s_popover_menu_clicked), this);
	gtk_widget_add_css_class(btn, "flat");
	return btn;
}

/* flat labelled button inside a split popover, wired to a toolbar action */
GtkWidget * AP_UnixRibbon::_popoverTbButton(XAP_Toolbar_Id id,
											const char * szLabel)
{
	const EV_Toolbar_ActionSet * pTBActions =
		XAP_App::getApp()->getToolbarActionSet();
	EV_Toolbar_Action * pAction =
		pTBActions ? pTBActions->getAction(id) : nullptr;
	EV_Toolbar_Label * pLabel =
		m_pTBLabels ? m_pTBLabels->getLabel(id) : nullptr;
	if (!pAction || !pLabel)
		return nullptr;

	GtkWidget * btn = gtk_button_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	const char * szIcon = pLabel->getIconName();
	if (szIcon && g_ascii_strcasecmp(szIcon, "NoIcon") != 0)
	{
		gchar * szTheme = abi_stock_from_toolbar_id(szIcon);
		gtk_box_append(GTK_BOX(box),
					   gtk_image_new_from_icon_name(szTheme));
		g_free(szTheme);
	}
	GtkWidget * wLabel = gtk_label_new(szLabel);
	gtk_widget_set_halign(wLabel, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(box), wLabel);
	gtk_button_set_child(GTK_BUTTON(btn), box);

	const char * szTip = pLabel->getToolTip();
	if (szTip && *szTip)
		gtk_widget_set_tooltip_text(btn, szTip);

	_PopTbCtx * ctx = g_new0(_PopTbCtx, 1);
	ctx->self = this;
	ctx->id = id;
	g_object_set_data_full(G_OBJECT(btn), "abi-tb-ctx", ctx, g_free);
	g_signal_connect(btn, "clicked",
					 G_CALLBACK(_s_popover_tb_clicked), ctx);
	gtk_widget_add_css_class(btn, "flat");
	return btn;
}

/* flat labelled button inside a split popover, wired directly to an
 * edit method by name (for actions that have no menu action id) */
GtkWidget * AP_UnixRibbon::_popoverEmButton(const char * szLabel,
											const char * szIcon,
											const char * szMethod,
											const char * szData)
{
	GtkWidget * btn = gtk_button_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	if (szIcon && *szIcon)
		gtk_box_append(GTK_BOX(box),
					   gtk_image_new_from_icon_name(szIcon));
	GtkWidget * wLabel = gtk_label_new(szLabel);
	gtk_widget_set_halign(wLabel, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(box), wLabel);
	gtk_button_set_child(GTK_BUTTON(btn), box);

	g_object_set_data_full(G_OBJECT(btn), "abi-em-method",
						   g_strdup(szMethod), g_free);
	if (szData)
		g_object_set_data_full(G_OBJECT(btn), "abi-em-data",
							   g_strdup(szData), g_free);
	g_signal_connect(btn, "clicked",
					 G_CALLBACK(_s_popover_em_clicked), this);
	gtk_widget_add_css_class(btn, "flat");
	return btn;
}

void AP_UnixRibbon::_s_popover_em_clicked(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	const char * szMethod = static_cast<const char *>(
		g_object_get_data(G_OBJECT(w), "abi-em-method"));
	const char * szData = static_cast<const char *>(
		g_object_get_data(G_OBJECT(w), "abi-em-data"));
	UT_return_if_fail(self && szMethod);
	_tb_popdown_popover(w);
	self->_invokeEditMethod(szMethod, szData);
}

/* Layout tab: Line Numbering Options… / Hyphenation Options… */
void AP_UnixRibbon::_s_linedlg_clicked(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	_tb_popdown_popover(w);
	GtkWindow * win = nullptr;
	if (self->m_pFrame && self->m_pFrame->getFrameImpl())
	{
		XAP_UnixFrameImpl * impl =
			static_cast<XAP_UnixFrameImpl *>(self->m_pFrame->getFrameImpl());
		win = GTK_WINDOW(impl->getTopLevelWindow());
	}
	ap_showLineNumbersDialog(win, static_cast<FV_View *>(
		self->m_pFrame ? self->m_pFrame->getCurrentView() : nullptr));
}

void AP_UnixRibbon::_s_hyphdlg_clicked(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	_tb_popdown_popover(w);
	GtkWindow * win = nullptr;
	if (self->m_pFrame && self->m_pFrame->getFrameImpl())
	{
		XAP_UnixFrameImpl * impl =
			static_cast<XAP_UnixFrameImpl *>(self->m_pFrame->getFrameImpl());
		win = GTK_WINDOW(impl->getTopLevelWindow());
	}
	ap_showHyphenationDialog(win, static_cast<FV_View *>(
		self->m_pFrame ? self->m_pFrame->getCurrentView() : nullptr));
}

void AP_UnixRibbon::_s_paste_special_clicked(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	_tb_popdown_popover(w);
	self->_showPasteSpecialDialog();
}

void AP_UnixRibbon::_invokeEditMethod(const char * szMethod,
										const char * szData)
{
	const EV_EditMethodContainer * pEMC =
		XAP_App::getApp()->getEditMethodContainer();
	UT_return_if_fail(pEMC);

	EV_EditMethod * pEM = pEMC->findEditMethodByName(szMethod);
	UT_return_if_fail(pEM);

	AV_View * pView = m_pFrame ? m_pFrame->getCurrentView() : nullptr;
	EV_EditMethodCallData emcd(szData ? szData : "",
							   szData ? strlen(szData) : 0);
	pEM->Fn(pView, &emcd);
}

/* Paste Special: pick the clipboard format to import */
static const char * _paste_format_label(const char * szMime)
{
	if (!szMime)
		return "";
	if (!strcmp(szMime, "text/plain") || !strcmp(szMime, "UTF8_STRING") ||
		!strcmp(szMime, "TEXT") || !strcmp(szMime, "STRING") ||
		!strcmp(szMime, "COMPOUND_TEXT"))
		return "Unformatted Text";
	if (!strcmp(szMime, "text/rtf") || !strcmp(szMime, "application/rtf"))
		return "Rich Text Format (RTF)";
	if (!strcmp(szMime, "text/html") ||
		!strcmp(szMime, "application/xhtml+xml"))
		return "HTML";
	if (!strcmp(szMime, "application/vnd.oasis.opendocument.text"))
		return "ODF Text";
	if (!strcmp(szMime, "text/uri-list"))
		return "Files";
	if (!strncmp(szMime, "image/", 6))
	{
		static char buf[128];
		const char * ext = szMime + 6;
		g_snprintf(buf, sizeof(buf), "Picture (%s)", ext);
		for (char * p = buf; *p; ++p)
			*p = g_ascii_toupper(*p);
		return buf;
	}
	return szMime;
}

void AP_UnixRibbon::_showPasteSpecialDialog()
{
	AV_View * pView = m_pFrame ? m_pFrame->getCurrentView() : nullptr;
	FV_View * pFV = pView ? static_cast<FV_View *>(pView) : nullptr;
	UT_return_if_fail(pFV);

	GtkWidget * toplevel = m_pFrame && m_pFrame->getFrameImpl()
		? static_cast<XAP_UnixFrameImpl *>(
			  m_pFrame->getFrameImpl())->getTopLevelWindow()
		: nullptr;

	GdkClipboard * clip = gdk_display_get_clipboard(
		toplevel ? gtk_widget_get_display(toplevel)
				 : gdk_display_get_default());
	GdkContentFormats * fmts =
		clip ? gdk_clipboard_get_formats(clip) : nullptr;
	gsize nMimes = 0;
	const char * const * mimes =
		fmts ? gdk_content_formats_get_mime_types(fmts, &nMimes) : nullptr;
	UT_return_if_fail(mimes && nMimes);

	GtkWidget * dlg = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dlg), "Paste Special");
	gtk_window_set_modal(GTK_WINDOW(dlg), true);
	if (toplevel)
		gtk_window_set_transient_for(GTK_WINDOW(dlg),
									 GTK_WINDOW(toplevel));
	gtk_dialog_add_button(GTK_DIALOG(dlg), "_Cancel", GTK_RESPONSE_CANCEL);
	GtkWidget * ok = gtk_dialog_add_button(GTK_DIALOG(dlg), "OK",
										   GTK_RESPONSE_OK);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

	GtkWidget * content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
	GtkWidget * label = gtk_label_new("Source:");
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_widget_set_margin_top(label, 8);
	gtk_widget_set_margin_start(label, 8);
	gtk_box_append(GTK_BOX(content), label);

	GtkWidget * scroll = gtk_scrolled_window_new();
	gtk_widget_set_size_request(scroll, 360, 220);
	gtk_scrolled_window_set_min_content_height(
		GTK_SCROLLED_WINDOW(scroll), 180);
	GtkWidget * list = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(list),
									GTK_SELECTION_SINGLE);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list);
	gtk_widget_set_margin_start(scroll, 8);
	gtk_widget_set_margin_end(scroll, 8);
	gtk_widget_set_margin_bottom(scroll, 8);
	gtk_box_append(GTK_BOX(content), scroll);

	/* image formats collapse into a single "Picture" entry that pastes
	 * the best available flavour */
	static const char * s_image_pref[] = {
		"image/png", "image/svg+xml", "image/svg",
		"image/jpeg", "image/gif", "image/bmp",
		"image/tiff", "image/x-xpixmap", "image/x-xbitmap",
		"image/x-portable-anymap", "image/x-portable-pixmap",
		"image/x-portable-graymap", "image/x-cmu-raster",
		"image/vnd.wap.wbmp", "image/x-wmf", nullptr
	};
	const char * szBestImage = nullptr;
	for (gsize i = 0; i < nMimes; ++i)
	{
		if (strncmp(mimes[i], "image/", 6))
			continue;
		int best = INT_MAX, cur = INT_MAX;
		if (szBestImage)
			for (int k = 0; s_image_pref[k]; ++k)
				if (!strcmp(s_image_pref[k], szBestImage)) { best = k; break; }
		for (int k = 0; s_image_pref[k]; ++k)
			if (!strcmp(s_image_pref[k], mimes[i])) { cur = k; break; }
		if (!szBestImage || cur < best)
			szBestImage = mimes[i];
	}

	GtkListBoxRow * first = nullptr;
	GHashTable * seen = g_hash_table_new(g_str_hash, g_str_equal);
	for (gsize i = 0; i < nMimes; ++i)
	{
		const char * szMime = mimes[i];
		const char * szFriendly;
		if (!strncmp(szMime, "image/", 6))
		{
			szMime = szBestImage;
			szFriendly = "Picture";
		}
		else
			szFriendly = _paste_format_label(szMime);

		/* collapse clipboard aliases that paste identically
		 * (text/plain vs UTF8_STRING, text/rtf vs application/rtf,
		 * the image flavours) into a single row */
		const char * szKey = *szFriendly ? szFriendly : szMime;
		if (g_hash_table_contains(seen, szKey))
			continue;
		g_hash_table_add(seen, const_cast<char *>(szKey));

		GtkWidget * row = gtk_list_box_row_new();
		GtkWidget * rLabel = gtk_label_new(
			*szFriendly ? szFriendly : szMime);
		gtk_widget_set_halign(rLabel, GTK_ALIGN_START);
		gtk_widget_set_margin_top(rLabel, 4);
		gtk_widget_set_margin_bottom(rLabel, 4);
		gtk_widget_set_margin_start(rLabel, 8);
		gtk_widget_set_margin_end(rLabel, 8);
		gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), rLabel);
		g_object_set_data_full(G_OBJECT(row), "abi-mime",
							   g_strdup(szMime), g_free);
		gtk_list_box_append(GTK_LIST_BOX(list), row);
		if (!first)
			first = GTK_LIST_BOX_ROW(row);
	}
	g_hash_table_destroy(seen);
	if (first)
		gtk_list_box_select_row(GTK_LIST_BOX(list), first);
	gtk_widget_set_sensitive(ok, first != nullptr);

	g_object_set_data(G_OBJECT(dlg), "abi-list", list);
	g_object_set_data(G_OBJECT(dlg), "abi-view", pFV);
	g_signal_connect(dlg, "response",
					 G_CALLBACK(_s_paste_special_response), nullptr);
	gtk_window_present(GTK_WINDOW(dlg));
}

struct _PasteAsCtx
{
	FV_View * view;
	char    * mime;
};

static gboolean _paste_as_idle(gpointer data)
{
	_PasteAsCtx * ctx = static_cast<_PasteAsCtx *>(data);
	if (ctx->view && ctx->mime)
		ctx->view->cmdPasteAs(ctx->mime);
	g_free(ctx->mime);
	g_free(ctx);
	return G_SOURCE_REMOVE;
}

void AP_UnixRibbon::_s_paste_special_response(GtkDialog * dlg, gint resp,
											  gpointer /*data*/)
{
	_PasteAsCtx * ctx = nullptr;
	if (resp == GTK_RESPONSE_OK)
	{
		GtkWidget * list = GTK_WIDGET(
			g_object_get_data(G_OBJECT(dlg), "abi-list"));
		GtkListBoxRow * row =
			gtk_list_box_get_selected_row(GTK_LIST_BOX(list));
		const char * mime = row
			? static_cast<const char *>(
				  g_object_get_data(G_OBJECT(row), "abi-mime"))
			: nullptr;
		FV_View * pView = static_cast<FV_View *>(
			g_object_get_data(G_OBJECT(dlg), "abi-view"));
		if (pView && mime)
		{
			ctx = g_new0(_PasteAsCtx, 1);
			ctx->view = pView;
			ctx->mime = g_strdup(mime);
		}
	}
	gtk_window_destroy(GTK_WINDOW(dlg));
	/* run the paste once the dialog is gone - the clipboard read pumps
	 * a nested main loop which must not run inside the response
	 * handler of a still-modal dialog */
	if (ctx)
		g_idle_add(_paste_as_idle, ctx);
}

/* Paste options: Paste / Paste Special… */
GtkWidget * AP_UnixRibbon::_makePastePopover()
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);
	gtk_widget_set_margin_start(box, 4);
	gtk_widget_set_margin_end(box, 4);

	GtkWidget * header = gtk_label_new("Paste Options:");
	gtk_widget_set_halign(header, GTK_ALIGN_START);
	gtk_widget_set_margin_start(header, 8);
	gtk_widget_set_margin_end(header, 8);
	gtk_widget_set_margin_bottom(header, 2);
	gtk_widget_add_css_class(header, "heading");
	gtk_box_append(GTK_BOX(box), header);

	GtkWidget * w = _popoverEmButton("Keep Text Only",
								   "edit-copy",
								   "pasteSpecial");
	if (w) gtk_box_append(GTK_BOX(box), w);

	GtkWidget * btn = gtk_button_new();
	GtkWidget * row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_append(GTK_BOX(row),
				   gtk_image_new_from_icon_name("edit-paste"));
	GtkWidget * wLabel = gtk_label_new("Paste Special…");
	gtk_widget_set_halign(wLabel, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(row), wLabel);
	gtk_button_set_child(GTK_BUTTON(btn), row);
	g_signal_connect(btn, "clicked",
					 G_CALLBACK(_s_paste_special_clicked), this);
	gtk_widget_add_css_class(btn, "flat");
	gtk_box_append(GTK_BOX(box), btn);

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* one library tile: a preview button wired to doListType with the
 * given "TYPE[:DECIMAL[:DELIM]]" argument */
GtkWidget * AP_UnixRibbon::_listTile(const char * szMarkup,
									 const char * szData,
									 int iWidth,
									 int iHeight)
{
	GtkWidget * btn = gtk_button_new();
	GtkWidget * lbl = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(lbl), szMarkup);
	gtk_label_set_justify(GTK_LABEL(lbl), GTK_JUSTIFY_LEFT);
	gtk_button_set_child(GTK_BUTTON(btn), lbl);
	if (iWidth > 0 && iHeight > 0)
		gtk_widget_set_size_request(btn, iWidth, iHeight);
	g_object_set_data_full(G_OBJECT(btn), "abi-em-method",
						   g_strdup("doListType"), g_free);
	g_object_set_data_full(G_OBJECT(btn), "abi-em-data",
						   g_strdup(szData), g_free);
	g_signal_connect(btn, "clicked",
					 G_CALLBACK(_s_popover_em_clicked), this);
	return btn;
}

/* Bullet Library (LibreOffice style): a grid of bullet-glyph
 * preview tiles + a "Define New Bulletpoint" entry that opens the
 * full Bullets & Numbering dialog */
GtkWidget * AP_UnixRibbon::_makeBulletLibraryPopover()
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);
	gtk_widget_set_margin_start(box, 4);
	gtk_widget_set_margin_end(box, 4);

	GtkWidget * title = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(title), "<b>Bullet Library</b>");
	gtk_widget_set_halign(title, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(box), title);

	GtkWidget * grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
	gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);

	struct { const char * glyph; const char * data; } tiles[] = {
		{ "<span size='large'>None</span>",	"NONE" },
		{ "<span size='x-large'>\xE2\x80\xA2</span>",	"BULLETED" },
		{ "<span size='x-large'>\xE2\x9D\x92</span>",	"BOX" },
		{ "<span size='x-large'>\xE2\x96\xAA</span>",	"SQUARE" },
		{ "<span size='x-large'>\xE2\x96\xB2</span>",	"TRIANGLE" },
		{ "<span size='x-large'>\xE2\x97\x86</span>",	"DIAMOND" },
		{ "<span size='x-large'>\xE2\x9C\xB3</span>",	"STAR" },
		{ "<span size='x-large'>\xE2\x87\x92</span>",	"IMPLIES" },
		{ "<span size='x-large'>\xE2\x9C\x93</span>",	"TICK" },
		{ "<span size='x-large'>\xE2\x98\x9E</span>",	"HAND" },
		{ "<span size='x-large'>\xE2\x99\xA5</span>",	"HEART" },
		{ "<span size='x-large'>\xE2\x9E\xA3</span>",	"ARROWHEAD" },
		{ "<span size='x-large'>-</span>",	"DASHED" },
	};
	const int nTiles = sizeof(tiles) / sizeof(tiles[0]);
	for (int i = 0; i < nTiles; ++i)
	{
		GtkWidget * t = _listTile(tiles[i].glyph, tiles[i].data, 64, 48);
		gtk_grid_attach(GTK_GRID(grid), t, i % 4, i / 4, 1, 1);
	}
	gtk_box_append(GTK_BOX(box), grid);

	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget * w = _popoverEmButton("Define New Bulletpoint\xE2\x80\xA6",
								   nullptr, "dlgBullets");
	if (w) gtk_box_append(GTK_BOX(box), w);

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* Numbering Library (LibreOffice style): preview tiles for each
 * numbering style + "Define New Number Format" entry */
GtkWidget * AP_UnixRibbon::_makeNumberingLibraryPopover()
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);
	gtk_widget_set_margin_start(box, 4);
	gtk_widget_set_margin_end(box, 4);

	GtkWidget * title = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(title), "<b>Numbering Library</b>");
	gtk_widget_set_halign(title, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(box), title);

	GtkWidget * grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
	gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);

	struct { const char * prev; const char * data; } tiles[] = {
		{ "<span size='large'>None</span>",
		  "NONE" },
		{ "1.  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\n2.  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\n3.  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80",
		  "NUMBERED" },
		{ "1)  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\n2)  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\n3)  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80",
		  "NUMBERED::%L)" },
		{ "I.   \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\nII.  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\nIII. \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80",
		  "UPPERROMAN" },
		{ "A.  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\nB.  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\nC.  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80",
		  "UPPERCASE" },
		{ "a)  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\nb)  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\nc)  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80",
		  "LOWERCASE::%L)" },
		{ "a.  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\nb.  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\nc.  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80",
		  "LOWERCASE" },
		{ "i.   \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\nii.  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\niii. \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80",
		  "LOWERROMAN" },
	};
	const int nTiles = sizeof(tiles) / sizeof(tiles[0]);
	for (int i = 0; i < nTiles; ++i)
	{
		GtkWidget * t = _listTile(tiles[i].prev, tiles[i].data, 132, 68);
		gtk_grid_attach(GTK_GRID(grid), t, i % 3, i / 3, 1, 1);
	}
	gtk_box_append(GTK_BOX(box), grid);

	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget * w = _popoverEmButton("Define New Number Format\xE2\x80\xA6",
								   nullptr, "dlgBullets");
	if (w) gtk_box_append(GTK_BOX(box), w);

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* List Library (LibreOffice style): current multi-level list plus a
 * grid of multi-level presets, with "Define New" entries opening
 * the full Bullets & Numbering dialog */
GtkWidget * AP_UnixRibbon::_makeMultilevelLibraryPopover()
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);
	gtk_widget_set_margin_start(box, 4);
	gtk_widget_set_margin_end(box, 4);

	GtkWidget * title = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(title), "<b>Current List</b>");
	gtk_widget_set_halign(title, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(box), title);

	GtkWidget * cur = gtk_frame_new(nullptr);
	GtkWidget * curLbl = gtk_label_new(
		"1. \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\n"
		"    a. \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\n"
		"        i. \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80");
	gtk_label_set_justify(GTK_LABEL(curLbl), GTK_JUSTIFY_LEFT);
	gtk_widget_set_margin_start(curLbl, 12);
	gtk_widget_set_margin_top(curLbl, 8);
	gtk_widget_set_margin_bottom(curLbl, 8);
	gtk_widget_set_halign(curLbl, GTK_ALIGN_START);
	gtk_frame_set_child(GTK_FRAME(cur), curLbl);
	gtk_box_append(GTK_BOX(box), cur);

	title = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(title), "<b>List Library</b>");
	gtk_widget_set_halign(title, GTK_ALIGN_START);
	gtk_widget_set_margin_top(title, 6);
	gtk_box_append(GTK_BOX(box), title);

	GtkWidget * grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
	gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);

	struct { const char * prev; const char * data; } tiles[] = {
		{ "<span size='large'>None</span>",
		  "NONE" },
		{ "1) \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\na) \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\ni) \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80",
		  "NUMBERED::%L)" },
		{ "1. \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\n1.1. \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\n1.1.1. \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80",
		  "NUMBERED:%*%d:%L." },
		{ "\xE2\x9D\x96 \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\n\xE2\x9E\xA2 \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\n\xE2\x96\xAA \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80",
		  "BULLETED" },
		{ "<b>Article I.</b> <span size='x-small'>Heading 1</span>\n"
		  "<b>Section 1.01</b> <span size='x-small'>Heading</span>\n"
		  "<b>(a)</b> <span size='x-small'>Heading 3</span>",
		  "UPPERROMAN" },
		{ "<b>1</b> <span size='x-small'>Heading 1</span> \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\n"
		  "<b>1.1</b> <span size='x-small'>Heading 2</span> \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\n"
		  "<b>1.1.1</b> <span size='x-small'>Heading 3</span>",
		  "NUMBERED:%*%d:%L" },
		{ "I. <span size='x-small'>Heading 1</span> \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\n"
		  "A. <span size='x-small'>Heading 2</span> \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\n"
		  "1. <span size='x-small'>Heading 3</span>",
		  "UPPERROMAN" },
		{ "<b>Chapter 1</b> <span size='x-small'>Heading</span>\n"
		  "<span size='x-small'>Heading 2</span> \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\n"
		  "<span size='x-small'>Heading 3</span> \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80",
		  "NUMBERED" },
	};
	const int nTiles = sizeof(tiles) / sizeof(tiles[0]);
	for (int i = 0; i < nTiles; ++i)
	{
		GtkWidget * t = _listTile(tiles[i].prev, tiles[i].data, 160, 68);
		gtk_grid_attach(GTK_GRID(grid), t, i % 3, i / 3, 1, 1);
	}
	gtk_box_append(GTK_BOX(box), grid);

	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget * w = _popoverEmButton("Define New Multi-level List\xE2\x80\xA6",
								   nullptr, "dlgBullets");
	if (w) gtk_box_append(GTK_BOX(box), w);
	w = _popoverEmButton("Define New List Style\xE2\x80\xA6",
						 nullptr, "dlgBullets");
	if (w) gtk_box_append(GTK_BOX(box), w);

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* list dropdown for the three list split-buttons: the Bullet,
 * Numbering and List libraries */
GtkWidget * AP_UnixRibbon::_makeListPopover(XAP_Toolbar_Id id)
{
	switch (id)
	{
	case (XAP_Toolbar_Id)AP_TOOLBAR_ID_LISTS_BULLETS:
		return _makeBulletLibraryPopover();
	case (XAP_Toolbar_Id)AP_TOOLBAR_ID_LISTS_NUMBERS:
		return _makeNumberingLibraryPopover();
	case (XAP_Toolbar_Id)AP_TOOLBAR_ID_LISTS_DASHED:
		return _makeMultilevelLibraryPopover();
	default:
		break;
	}
	return _makeBulletLibraryPopover();
}

/* Change Case: the LibreOffice "Aa" dropdown - five direct case
 * conversions wired straight to the edit methods */
GtkWidget * AP_UnixRibbon::_makeChangeCasePopover()
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);
	gtk_widget_set_margin_start(box, 4);
	gtk_widget_set_margin_end(box, 4);

	GtkWidget * w = _popoverEmButton("Sentence case", nullptr, "caseSentence");
	if (w) gtk_box_append(GTK_BOX(box), w);
	w = _popoverEmButton("lowercase", nullptr, "caseLower");
	if (w) gtk_box_append(GTK_BOX(box), w);
	w = _popoverEmButton("UPPERCASE", nullptr, "caseUpper");
	if (w) gtk_box_append(GTK_BOX(box), w);
	w = _popoverEmButton("Capitalize Every Word", nullptr, "caseTitle");
	if (w) gtk_box_append(GTK_BOX(box), w);
	w = _popoverEmButton("tOGGLE cASE", nullptr, "caseToggle");
	if (w) gtk_box_append(GTK_BOX(box), w);

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* single-button dropdown (LibreOffice "Aa"): the button itself opens
 * the popover - no separate arrow widget, no action on the button.
 * With AP_RIBBON_FLAG_LARGE the button is a Word-style icon-over-
 * caption button with a down arrow (Layout page-setup group). */
GtkWidget * AP_UnixRibbon::_makeMenuPopButton(XAP_Menu_Id id,
											 uint8_t flags)
{
	GtkWidget * popover = nullptr;
	switch (id)
	{
	case (XAP_Menu_Id)AP_MENU_ID_FMT_TOGGLECASE:
		popover = _makeChangeCasePopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_FMT_BORDERS:
		popover = _makeBordersPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_MARGINS:
		popover = _makeMarginsPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_ORIENTATION:
		popover = _makeOrientationPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_SIZE:
		popover = _makeSizePopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_FMT_COLUMNS:
		popover = _makeColumnsPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_BREAKS:
		popover = _makeBreaksPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_LINENUMBERS:
		popover = _makeLineNumbersPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_HYPHENATION:
		popover = _makeHyphenationPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_POSITION:
		popover = _makePositionPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_WRAP:
		popover = _makeWrapPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_ALIGNOBJECTS:
		popover = _makeAlignObjPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_BRINGFORWARD:
		popover = _makeZOrderPopover(true);
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_SENDBACKWARD:
		popover = _makeZOrderPopover(false);
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_GROUPOBJECTS:
		popover = _makeGroupPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_ROTATE:
		popover = _makeRotatePopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_COVERPAGE:
		popover = _makeCoverPagePopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_PICTURES:
		popover = _makePicturesPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_SHAPES:
		popover = _makeShapesPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_3DMODELS:
		popover = _make3DModelsPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_MEDIA:
		popover = _makeMediaPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_WORDART:
		popover = _makeWordArtPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_EQUATION:
	case (XAP_Menu_Id)AP_MENU_ID_EDIT_LATEXEQUATION:
		popover = _makeEquationPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_TEXTBOX:
		popover = _makeTextBoxPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_OBJECT:
		popover = _makeObjectPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_HEADER:
		popover = _makeHdrFtrPopover(false);
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_FOOTER:
		popover = _makeHdrFtrPopover(true);
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_PAGENO:
		popover = _makePageNumberPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_DROPCAP:
		popover = _makeDropCapPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_TOCPOP:
		popover = _makeTOCGalleryPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_ADDTEXT:
		popover = _makeAddTextPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_NEXTFN:
		popover = _makeNextNotePopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_CITATION:
		popover = _makeCitationPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_BIBLIOGRAPHY:
		popover = _makeBibliographyPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_SOURCES:
		popover = _makeSourcesPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_CAPTION:
		popover = _makeCaptionPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_TOF:
		popover = _makeTOFPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_XREF:
		popover = _makeXRefPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_MARKENTRY:
		popover = _makeMarkEntryPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_MARKCIT:
		popover = _makeMarkCitPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_MENUPOP_DELETE:
		popover = _makeCommentDeletePopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_MENUPOP_SHOW:
		popover = _makeCommentShowPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_SPELLING_MENUPOP:
		popover = _makeSpellingPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_TRACK:
		popover = _makeTrackChangesPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_DISPLAY:
		popover = _makeMarkupPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_ACCEPT:
		popover = _makeAcceptPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_REJECT:
		popover = _makeRejectPopover();
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_COMPARE:
		popover = _makeComparePopover();
		break;
	default:
		break;
	}
	if (!popover)
		return nullptr;

	if (flags & AP_RIBBON_FLAG_LARGE)
	{
		GtkWidget * mb = _makeLargeMenuButton(id, popover, flags);
		/* the Display-for-Review button shows the active markup mode
		 * as its caption, like Word's "All Markup" dropdown */
		if (id == (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_DISPLAY)
		{
			GtkWidget * box = gtk_menu_button_get_child(
				GTK_MENU_BUTTON(mb));
			if (box)
			{
				GtkWidget * icon = gtk_widget_get_first_child(box);
				m_pMarkupLabel = icon
					? gtk_widget_get_next_sibling(icon) : nullptr;
				if (m_pMarkupLabel)
					gtk_label_set_text(GTK_LABEL(m_pMarkupLabel),
									   _markupModeName());
			}
		}
		return mb;
	}

	GtkWidget * mb = gtk_menu_button_new();
	if (id == (XAP_Menu_Id)AP_MENU_ID_FMT_BORDERS)
	{
		const gchar * szIcon = abi_stock_from_menu_id(id);
		if (szIcon && *szIcon)
			gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(mb), szIcon);
		gtk_menu_button_set_direction(GTK_MENU_BUTTON(mb),
									  GTK_ARROW_DOWN);
	}
	else if (id == (XAP_Menu_Id)AP_MENU_ID_FMT_TOGGLECASE)
	{
		GtkWidget * gl = gtk_label_new(nullptr);
		gtk_label_set_markup(GTK_LABEL(gl), "Aa");
		gtk_menu_button_set_child(GTK_MENU_BUTTON(mb), gl);
		gtk_menu_button_set_direction(GTK_MENU_BUTTON(mb),
									  GTK_ARROW_NONE);
	}
	else if (flags & AP_RIBBON_FLAG_ICONONLY)
	{
		/* drawn page glyph for the Layout popovers */
		gtk_menu_button_set_child(GTK_MENU_BUTTON(mb),
								  _layout_icon(id, 18, 18));
		gtk_menu_button_set_direction(GTK_MENU_BUTTON(mb),
									  GTK_ARROW_DOWN);
	}
	else
	{
		/* small labelled dropdown (Word's Add Text / Bibliography):
		 * drawn glyph + caption, arrow supplied by the menu button */
		const EV_Menu_Label * pFaceLabel =
			m_pMenu ? m_pMenu->getLabelSet()->getLabel(id) : nullptr;
		char face[64];
		_ribbon_strip_mnemonic(
			(pFaceLabel && pFaceLabel->getMenuLabel())
				? pFaceLabel->getMenuLabel() : "",
			face, sizeof(face));
		GtkWidget * hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
		gtk_box_append(GTK_BOX(hb), _layout_icon(id, 16, 16));
		GtkWidget * wl = gtk_label_new(face);
		/* small dropdown captions stay single-line like Word's
		 * compact rows; ellipsize instead of wrapping so squeezed
		 * groups never collapse into one-char columns */
		gtk_label_set_ellipsize(GTK_LABEL(wl), PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars(GTK_LABEL(wl), 16);
		gtk_box_append(GTK_BOX(hb), wl);
		gtk_menu_button_set_child(GTK_MENU_BUTTON(mb), hb);
		gtk_menu_button_set_direction(GTK_MENU_BUTTON(mb),
									  GTK_ARROW_DOWN);
	}
	if (flags & AP_RIBBON_FLAG_SLIM)
		_slim_widget_tree(mb);
	gtk_menu_button_set_has_frame(GTK_MENU_BUTTON(mb), FALSE);
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(mb), popover);

	const EV_Menu_Label * pLabel =
		m_pMenu ? m_pMenu->getLabelSet()->getLabel(id) : nullptr;
	if (pLabel)
	{
		const char * szStatus = pLabel->getMenuStatusMessage();
		if (szStatus && *szStatus && strcmp(szStatus, " ") != 0)
			gtk_widget_set_tooltip_text(mb, szStatus);
	}
	return mb;
}

/* ================= Layout tab: page-setup popovers =================
 *
 * Word-style dropdowns for Margins, Orientation, Size, Columns,
 * Breaks, Line Numbers and Hyphenation.  The row icons are tiny
 * cairo-drawn page glyphs so the look matches the reference UI on
 * every icon theme.
 */

struct _PageSpec
{
	double mt, mb, ml, mr;	/* margins as fractions of the page */
	int		cols;			/* text columns */
	bool	landscape;		/* page drawn wider than tall */
	bool	linenum;		/* tiny line-number digits */
	int		fold;			/* folded corner / break marker */
	bool	bare;			/* skip the page, draw only the overlay */
};

/* paint a mini page: outline, margin frame, text lines */
static void _draw_page_glyph(cairo_t * cr, double w, double h,
							 const _PageSpec & s)
{
	double pw = s.landscape ? w : w * 0.78;
	double ph = s.landscape ? h * 0.62 : h;
	double px = (w - pw) / 2.0;
	double py = (h - ph) / 2.0;

	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_rectangle(cr, px + 0.5, py + 0.5, pw - 1, ph - 1);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	double mx0 = px + s.ml * pw, mx1 = px + pw - s.mr * pw;
	double my0 = py + s.mt * ph, my1 = py + ph - s.mb * ph;

	/* margin frame */
	cairo_set_source_rgba(cr, 0.35, 0.55, 0.9, 0.85);
	cairo_set_line_width(cr, 0.9);
	cairo_rectangle(cr, mx0, my0, mx1 - mx0, my1 - my0);
	cairo_stroke(cr);

	/* text lines, split into column stripes */
	int cols = s.cols < 1 ? 1 : s.cols;
	double gap = 2.5;
	double cw = (mx1 - mx0 - gap * (cols - 1)) / cols;
	cairo_set_source_rgb(cr, 0.55, 0.58, 0.65);
	cairo_set_line_width(cr, 0.9);
	for (int c = 0; c < cols; ++c)
	{
		double lx = mx0 + c * (cw + gap);
		for (double ly = my0 + 2.0; ly < my1 - 1.0; ly += 3.2)
		{
			cairo_move_to(cr, lx, ly);
			cairo_line_to(cr, lx + cw, ly);
		}
	}
	cairo_stroke(cr);

	if (s.linenum)
	{
		cairo_set_source_rgb(cr, 0.25, 0.45, 0.85);
		cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
							   CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, MAX(5.0, ph * 0.16));
		cairo_move_to(cr, px + 1.0, my0 + 4.5);
		cairo_show_text(cr, "1");
		cairo_move_to(cr, px + 1.0, my0 + ph * 0.28 + 4.5);
		cairo_show_text(cr, "2");
	}

	if (s.fold)
	{
		/* folded top-right corner */
		double f = pw * 0.18;
		cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);
		cairo_move_to(cr, px + pw - f, py);
		cairo_line_to(cr, px + pw, py + f);
		cairo_line_to(cr, px + pw - f, py + f);
		cairo_close_path(cr);
		cairo_fill(cr);
	}
}

struct _GlyphCtx
{
	_PageSpec spec;
	void (*extra)(cairo_t *, double, double); /* optional overlay */
};

static void _s_glyph_draw(GtkDrawingArea * /*area*/, cairo_t * cr,
						  int w, int h, gpointer data)
{
	_GlyphCtx * c = static_cast<_GlyphCtx *>(data);
	UT_return_if_fail(c);
	if (!c->spec.bare)
		_draw_page_glyph(cr, w, h, c->spec);
	if (c->extra)
		c->extra(cr, w, h);
}

/* a drawn page-glyph widget for popover rows and ribbon buttons */
static GtkWidget * _glyph_widget(const _PageSpec & spec, int w, int h,
								 void (*extra)(cairo_t *, double, double) = nullptr)
{
	_GlyphCtx * c = new _GlyphCtx;
	c->spec = spec;
	c->extra = extra;
	GtkWidget * area = gtk_drawing_area_new();
	gtk_widget_set_size_request(area, w, h);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), _s_glyph_draw,
								   c, [](gpointer d) {
		delete static_cast<_GlyphCtx *>(d);
	});
	return area;
}

/* -------- button-face glyphs for the Page Setup group -------- */

static void _overlay_margin_corners(cairo_t * cr, double w, double h)
{
	/* blue L-brackets at the margin corners */
	double pw = w * 0.78, ph = h;
	double px = (w - pw) / 2.0, py = 0.0;
	double mx = px + 0.18 * pw, my = py + 0.18 * ph;
	double mx2 = px + pw - 0.18 * pw, my2 = py + ph - 0.18 * ph;
	double l = 3.5;
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 1.2);
	cairo_move_to(cr, mx + l, my); cairo_line_to(cr, mx, my); cairo_line_to(cr, mx, my + l);
	cairo_move_to(cr, mx2 - l, my); cairo_line_to(cr, mx2, my); cairo_line_to(cr, mx2, my + l);
	cairo_move_to(cr, mx + l, my2); cairo_line_to(cr, mx, my2); cairo_line_to(cr, mx, my2 - l);
	cairo_move_to(cr, mx2 - l, my2); cairo_line_to(cr, mx2, my2); cairo_line_to(cr, mx2, my2 - l);
	cairo_stroke(cr);
}

static void _overlay_dir_arrow(cairo_t * cr, double w, double h,
							   bool bRTL)
{
	/* bottom edge arrow showing the mark's writing direction */
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 1.4);
	double y = h - 4.5, x0 = 4.0, x1 = w - 5.0;
	double ax = bRTL ? x0 : x1;
	cairo_move_to(cr, bRTL ? x1 : x0, y);
	cairo_line_to(cr, ax, y);
	cairo_move_to(cr, ax + (bRTL ? 3.2 : -3.2), y - 2.4);
	cairo_line_to(cr, ax, y);
	cairo_line_to(cr, ax + (bRTL ? 3.2 : -3.2), y + 2.4);
	cairo_stroke(cr);
}

/* ---- Insert tab glyph overlays ------------------------------- */

static void _overlay_coverband(cairo_t * cr, double w, double h)
{
	/* accent band across the lower third of the page */
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_rectangle(cr, 0, h * 0.62, w, h * 0.38);
	cairo_fill(cr);
}

static void _overlay_pagebreak_arrow(cairo_t * cr, double w, double h)
{
	/* horizontal arrow pointing right off the page's mid-line */
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 1.4);
	double y = h * 0.5;
	cairo_move_to(cr, 2.5, y);
	cairo_line_to(cr, w - 4.5, y);
	cairo_move_to(cr, w - 7.5, y - 2.8);
	cairo_line_to(cr, w - 4.5, y);
	cairo_line_to(cr, w - 7.5, y + 2.8);
	cairo_stroke(cr);
}

static void _overlay_blankpage(cairo_t * cr, double w, double h)
{
	/* plus badge at the page's lower right = a fresh empty page */
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 1.6);
	double cx = w * 0.72, cy = h * 0.68, r = h * 0.14;
	cairo_move_to(cr, cx - r, cy);
	cairo_line_to(cr, cx + r, cy);
	cairo_move_to(cr, cx, cy - r);
	cairo_line_to(cr, cx, cy + r);
	cairo_stroke(cr);
}

static void _glyph_table(cairo_t * cr, double w, double h)
{
	/* 3x3 table grid */
	cairo_set_source_rgb(cr, 0.35, 0.5, 0.75);
	cairo_set_line_width(cr, 1.1);
	double m = 2.5, cw = (w - 2 * m) / 3.0, ch = (h - 2 * m) / 3.0;
	cairo_rectangle(cr, m, m, w - 2 * m, h - 2 * m);
	for (int i = 1; i < 3; i++)
	{
		cairo_move_to(cr, m + i * cw, m);
		cairo_line_to(cr, m + i * cw, h - m);
		cairo_move_to(cr, m, m + i * ch);
		cairo_line_to(cr, w - m, m + i * ch);
	}
	cairo_stroke(cr);
}

static void _glyph_picture(cairo_t * cr, double w, double h)
{
	/* framed landscape: sun + two mountains */
	cairo_set_source_rgb(cr, 0.35, 0.5, 0.75);
	cairo_set_line_width(cr, 1.1);
	cairo_rectangle(cr, 1.5, 3.0, w - 3.0, h - 6.0);
	cairo_stroke(cr);
	cairo_arc(cr, w * 0.30, h * 0.36, 1.8, 0, 2 * M_PI);
	cairo_set_source_rgb(cr, 0.9, 0.65, 0.15);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, 0.25, 0.55, 0.35);
	cairo_move_to(cr, 3, h - 4.5);
	cairo_line_to(cr, w * 0.42, h * 0.55);
	cairo_line_to(cr, w * 0.62, h - 4.5);
	cairo_close_path(cr);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.75);
	cairo_move_to(cr, w * 0.42, h - 4.5);
	cairo_line_to(cr, w * 0.68, h * 0.50);
	cairo_line_to(cr, w - 3, h - 4.5);
	cairo_close_path(cr);
	cairo_fill(cr);
}

static void _glyph_shapes(cairo_t * cr, double w, double h)
{
	/* overlapping square + circle + triangle */
	cairo_set_line_width(cr, 1.1);
	cairo_set_source_rgba(cr, 0.2, 0.45, 0.9, 0.85);
	cairo_rectangle(cr, 2.0, h * 0.30, w * 0.42, w * 0.42);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.15, 0.32, 0.65);
	cairo_stroke(cr);
	cairo_set_source_rgba(cr, 0.9, 0.45, 0.2, 0.85);
	cairo_arc(cr, w * 0.62, h * 0.60, w * 0.22, 0, 2 * M_PI);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.65, 0.3, 0.1);
	cairo_stroke(cr);
	cairo_set_source_rgba(cr, 0.25, 0.6, 0.4, 0.85);
	cairo_move_to(cr, w * 0.60, 2.0);
	cairo_line_to(cr, w * 0.82, h * 0.34);
	cairo_line_to(cr, w * 0.38, h * 0.34);
	cairo_close_path(cr);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.15, 0.4, 0.25);
	cairo_stroke(cr);
}

static void _glyph_smiley(cairo_t * cr, double w, double h)
{
	/* icon smiley: circle face, two eyes, smile */
	double cx = w / 2.0, cy = h / 2.0, r = w * 0.40;
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 1.2);
	cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
	cairo_stroke(cr);
	cairo_arc(cr, cx - r * 0.35, cy - r * 0.25, 0.9, 0, 2 * M_PI);
	cairo_arc(cr, cx + r * 0.35, cy - r * 0.25, 0.9, 0, 2 * M_PI);
	cairo_fill(cr);
	cairo_arc(cr, cx, cy + r * 0.1, r * 0.5, 0.35, M_PI - 0.35);
	cairo_stroke(cr);
}

static void _glyph_cube(cairo_t * cr, double w, double h)
{
	/* isometric cube */
	double cx = w / 2.0, top = h * 0.16, mid = h * 0.44,
		bot = h * 0.86, half = w * 0.34;
	cairo_set_source_rgba(cr, 0.25, 0.5, 0.9, 0.9);
	cairo_move_to(cr, cx, top);
	cairo_line_to(cr, cx + half, mid - 2);
	cairo_line_to(cr, cx, mid + 4);
	cairo_line_to(cr, cx - half, mid - 2);
	cairo_close_path(cr);
	cairo_fill(cr);
	cairo_set_source_rgba(cr, 0.2, 0.4, 0.8, 0.9);
	cairo_move_to(cr, cx - half, mid - 2);
	cairo_line_to(cr, cx, mid + 4);
	cairo_line_to(cr, cx, bot);
	cairo_line_to(cr, cx - half, bot - 4);
	cairo_close_path(cr);
	cairo_fill(cr);
	cairo_set_source_rgba(cr, 0.55, 0.7, 0.95, 0.9);
	cairo_move_to(cr, cx + half, mid - 2);
	cairo_line_to(cr, cx, mid + 4);
	cairo_line_to(cr, cx, bot);
	cairo_line_to(cr, cx + half, bot - 4);
	cairo_close_path(cr);
	cairo_fill(cr);
}

static void _glyph_camera(cairo_t * cr, double w, double h)
{
	/* camera body + lens */
	cairo_set_source_rgb(cr, 0.35, 0.5, 0.75);
	cairo_set_line_width(cr, 1.1);
	cairo_rectangle(cr, 1.5, h * 0.32, w - 3.0, h * 0.55);
	cairo_stroke(cr);
	cairo_rectangle(cr, w * 0.38, h * 0.22, w * 0.24, h * 0.12);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_arc(cr, w * 0.5, h * 0.60, w * 0.16, 0, 2 * M_PI);
	cairo_stroke(cr);
	cairo_arc(cr, w * 0.5, h * 0.60, w * 0.08, 0, 2 * M_PI);
	cairo_fill(cr);
}

static void _glyph_media(cairo_t * cr, double w, double h)
{
	/* film strip with play triangle */
	cairo_set_source_rgb(cr, 0.35, 0.5, 0.75);
	cairo_set_line_width(cr, 1.1);
	cairo_rectangle(cr, 1.5, 3.5, w - 3.0, h - 7.0);
	cairo_stroke(cr);
	for (int i = 0; i < 3; i++)
	{
		cairo_rectangle(cr, 3.0, 5.5 + i * 4.5, 2.2, 2.8);
		cairo_rectangle(cr, w - 5.2, 5.5 + i * 4.5, 2.2, 2.8);
	}
	cairo_fill(cr);
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_move_to(cr, w * 0.40, h * 0.38);
	cairo_line_to(cr, w * 0.40, h * 0.66);
	cairo_line_to(cr, w * 0.66, h * 0.52);
	cairo_close_path(cr);
	cairo_fill(cr);
}

static void _glyph_link(cairo_t * cr, double w, double h)
{
	/* two interlocked chain links */
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 1.6);
	cairo_arc(cr, w * 0.38, h * 0.42, w * 0.18, 0, 2 * M_PI);
	cairo_stroke(cr);
	cairo_arc(cr, w * 0.62, h * 0.62, w * 0.18, 0, 2 * M_PI);
	cairo_stroke(cr);
	cairo_set_line_width(cr, 1.3);
	cairo_move_to(cr, w * 0.47, h * 0.52);
	cairo_line_to(cr, w * 0.53, h * 0.52);
	cairo_stroke(cr);
}

static void _glyph_bookmark(cairo_t * cr, double w, double h)
{
	/* ribbon bookmark shape */
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	double x0 = w * 0.30, x1 = w * 0.70, y0 = 2.0, y1 = h - 2.0;
	cairo_move_to(cr, x0, y0);
	cairo_line_to(cr, x1, y0);
	cairo_line_to(cr, x1, y1);
	cairo_line_to(cr, w * 0.5, y1 - h * 0.22);
	cairo_line_to(cr, x0, y1);
	cairo_close_path(cr);
	cairo_fill(cr);
}

static void _glyph_comment(cairo_t * cr, double w, double h)
{
	/* speech bubble with two lines */
	cairo_set_source_rgb(cr, 0.35, 0.5, 0.75);
	cairo_set_line_width(cr, 1.1);
	double r = 3.0;
	cairo_new_sub_path(cr);
	cairo_arc(cr, 2 + r, 2 + r, r, M_PI, 1.5 * M_PI);
	cairo_arc(cr, w - 2 - r, 2 + r, r, 1.5 * M_PI, 0);
	cairo_arc(cr, w - 2 - r, h * 0.62 - r, r, 0, 0.5 * M_PI);
	cairo_arc(cr, 6 + r, h * 0.62 - r, r, 0.5 * M_PI, M_PI);
	cairo_close_path(cr);
	cairo_line_to(cr, 4.5, h - 2.0);
	cairo_line_to(cr, 4.5, h * 0.62);
	cairo_stroke(cr);
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_move_to(cr, 5, h * 0.24);
	cairo_line_to(cr, w - 5, h * 0.24);
	cairo_move_to(cr, 5, h * 0.42);
	cairo_line_to(cr, w - 8, h * 0.42);
	cairo_stroke(cr);
}

static void _glyph_comment_badge(cairo_t * cr, double w, double h,
								 double r, double g, double b, char sym)
{
	_glyph_comment(cr, w, h);
	/* badge circle top-left, like Word's comment action icons */
	double br = w * 0.22;
	double cx = br + 1.5, cy = br + 1.5;
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_arc(cr, cx, cy, br + 1.2, 0, 2 * M_PI);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, r, g, b);
	cairo_arc(cr, cx, cy, br, 0, 2 * M_PI);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_set_line_width(cr, 1.6);
	double s = br * 0.55;
	switch (sym)
	{
	case '+':
		cairo_move_to(cr, cx - s, cy); cairo_line_to(cr, cx + s, cy);
		cairo_move_to(cr, cx, cy - s); cairo_line_to(cr, cx, cy + s);
		break;
	case 'x':
		cairo_move_to(cr, cx - s, cy - s); cairo_line_to(cr, cx + s, cy + s);
		cairo_move_to(cr, cx + s, cy - s); cairo_line_to(cr, cx - s, cy + s);
		break;
	case 'v':
		cairo_move_to(cr, cx - s, cy); cairo_line_to(cr, cx - s * 0.2, cy + s * 0.8);
		cairo_line_to(cr, cx + s, cy - s * 0.7);
		break;
	case '<':
		cairo_move_to(cr, cx + s * 0.6, cy - s); cairo_line_to(cr, cx - s * 0.6, cy);
		cairo_line_to(cr, cx + s * 0.6, cy + s);
		break;
	default: /* '>' */
		cairo_move_to(cr, cx - s * 0.6, cy - s); cairo_line_to(cr, cx + s * 0.6, cy);
		cairo_line_to(cr, cx - s * 0.6, cy + s);
		break;
	}
	cairo_stroke(cr);
}

static void _glyph_comment_new(cairo_t * cr, double w, double h)
	{ _glyph_comment_badge(cr, w, h, 0.15, 0.65, 0.30, '+'); }
static void _glyph_comment_del(cairo_t * cr, double w, double h)
	{ _glyph_comment_badge(cr, w, h, 0.80, 0.20, 0.20, 'x'); }
static void _glyph_comment_resolve(cairo_t * cr, double w, double h)
	{ _glyph_comment_badge(cr, w, h, 0.15, 0.65, 0.30, 'v'); }
static void _glyph_comment_prev(cairo_t * cr, double w, double h)
	{ _glyph_comment_badge(cr, w, h, 0.20, 0.45, 0.90, '<'); }
static void _glyph_comment_next(cairo_t * cr, double w, double h)
	{ _glyph_comment_badge(cr, w, h, 0.20, 0.45, 0.90, '>'); }

/* ---- Review tab glyphs ---------------------------------------- */

static void _glyph_spell(cairo_t * cr, double w, double h)
{
	/* "ABC" with a check mark - Word's Spelling & Grammar icon */
	cairo_set_source_rgb(cr, 0.35, 0.5, 0.75);
	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
						   CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, h * 0.42);
	cairo_move_to(cr, 1.5, h * 0.52);
	cairo_show_text(cr, "ABC");
	/* green check at the lower right */
	cairo_set_source_rgb(cr, 0.15, 0.65, 0.30);
	cairo_set_line_width(cr, 2.2);
	double s = w * 0.16;
	double cx = w * 0.62, cy = h * 0.78;
	cairo_move_to(cr, cx - s, cy);
	cairo_line_to(cr, cx - s * 0.2, cy + s * 0.8);
	cairo_line_to(cr, cx + s * 1.3, cy - s * 0.8);
	cairo_stroke(cr);
}

static void _glyph_wordcount(cairo_t * cr, double w, double h)
{
	/* text lines with "123" over them - word count */
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, w * 0.10, h * 0.16);
	cairo_line_to(cr, w * 0.90, h * 0.16);
	cairo_move_to(cr, w * 0.10, h * 0.34);
	cairo_line_to(cr, w * 0.90, h * 0.34);
	cairo_stroke(cr);
	cairo_set_source_rgb(cr, 0.20, 0.45, 0.90);
	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
						   CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, h * 0.38);
	cairo_move_to(cr, w * 0.10, h * 0.80);
	cairo_show_text(cr, "123");
}

static void _glyph_track(cairo_t * cr, double w, double h)
{
	/* text lines with a struck-through deletion and an inserted
	 * underlined line - tracked-changes markup */
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, w * 0.14, h * 0.18);
	cairo_line_to(cr, w * 0.86, h * 0.18);
	cairo_move_to(cr, w * 0.14, h * 0.82);
	cairo_line_to(cr, w * 0.86, h * 0.82);
	cairo_stroke(cr);
	/* struck line */
	cairo_set_source_rgb(cr, 0.80, 0.20, 0.20);
	cairo_move_to(cr, w * 0.14, h * 0.40);
	cairo_line_to(cr, w * 0.72, h * 0.40);
	cairo_stroke(cr);
	cairo_move_to(cr, w * 0.14, h * 0.40 + 1.6);
	cairo_line_to(cr, w * 0.72, h * 0.40 + 1.6);
	cairo_stroke(cr);
	/* inserted underlined line */
	cairo_move_to(cr, w * 0.14, h * 0.61);
	cairo_line_to(cr, w * 0.62, h * 0.61);
	cairo_stroke(cr);
	cairo_move_to(cr, w * 0.14, h * 0.61 + 2.2);
	cairo_line_to(cr, w * 0.62, h * 0.61 + 2.2);
	cairo_stroke(cr);
}

static void _glyph_check(cairo_t * cr, double w, double h)
{
	/* plain tick used in the check-row slot - same footprint as a
	 * row icon so checked and unchecked rows keep the same width */
	cairo_set_source_rgb(cr, 0.25, 0.30, 0.36);
	cairo_set_line_width(cr, 2.0);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
	cairo_move_to(cr, w * 0.18, h * 0.55);
	cairo_line_to(cr, w * 0.42, h * 0.78);
	cairo_line_to(cr, w * 0.82, h * 0.28);
	cairo_stroke(cr);
}

static void _glyph_revauto(cairo_t * cr, double w, double h)
{
	/* text lines with a circular arrow - save a revision each save */
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_set_line_width(cr, 1.0);
	for (int i = 0; i < 3; ++i)
	{
		cairo_move_to(cr, w * 0.10, h * (0.16 + i * 0.20));
		cairo_line_to(cr, w * 0.62, h * (0.16 + i * 0.20));
	}
	cairo_stroke(cr);
	cairo_set_source_rgb(cr, 0.20, 0.45, 0.90);
	cairo_set_line_width(cr, 1.6);
	double cx = w * 0.72, cy = h * 0.62, r = w * 0.20;
	cairo_arc(cr, cx, cy, r, 0.6, 4.9);
	cairo_stroke(cr);
	/* arrow head */
	double ax = cx + r * cos(4.9), ay = cy + r * sin(4.9);
	cairo_move_to(cr, ax, ay);
	cairo_line_to(cr, ax - w * 0.10, ay - h * 0.02);
	cairo_move_to(cr, ax, ay);
	cairo_line_to(cr, ax + w * 0.02, ay - h * 0.10);
	cairo_stroke(cr);
}

static void _glyph_revnew(cairo_t * cr, double w, double h)
{
	/* revision lines plus a green plus - start a new revision level */
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_set_line_width(cr, 1.0);
	for (int i = 0; i < 3; ++i)
	{
		cairo_move_to(cr, w * 0.10, h * (0.16 + i * 0.20));
		cairo_line_to(cr, w * 0.62, h * (0.16 + i * 0.20));
	}
	cairo_stroke(cr);
	cairo_set_source_rgb(cr, 0.15, 0.65, 0.30);
	cairo_set_line_width(cr, 2.0);
	double cx = w * 0.72, cy = h * 0.60, s = w * 0.16;
	cairo_move_to(cr, cx - s, cy);
	cairo_line_to(cr, cx + s, cy);
	cairo_move_to(cr, cx, cy - s);
	cairo_line_to(cr, cx, cy + s);
	cairo_stroke(cr);
}

static void _glyph_revpurge(cairo_t * cr, double w, double h)
{
	/* revision lines crossed by a red X - purge the history */
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_set_line_width(cr, 1.0);
	for (int i = 0; i < 3; ++i)
	{
		cairo_move_to(cr, w * 0.10, h * (0.16 + i * 0.20));
		cairo_line_to(cr, w * 0.62, h * (0.16 + i * 0.20));
	}
	cairo_stroke(cr);
	cairo_set_source_rgb(cr, 0.80, 0.20, 0.20);
	cairo_set_line_width(cr, 2.0);
	double cx = w * 0.72, cy = h * 0.60, s = w * 0.15;
	cairo_move_to(cr, cx - s, cy - s);
	cairo_line_to(cr, cx + s, cy + s);
	cairo_move_to(cr, cx + s, cy - s);
	cairo_line_to(cr, cx - s, cy + s);
	cairo_stroke(cr);
}

static void _glyph_markup(cairo_t * cr, double w, double h)
{
	/* page lines plus the left-margin change bar of Simple Markup */
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_set_line_width(cr, 1.0);
	for (int i = 0; i < 4; ++i)
	{
		cairo_move_to(cr, w * 0.26, h * (0.18 + i * 0.19));
		cairo_line_to(cr, w * 0.88, h * (0.18 + i * 0.19));
	}
	cairo_stroke(cr);
	cairo_set_source_rgb(cr, 0.20, 0.45, 0.90);
	cairo_set_line_width(cr, 2.0);
	cairo_move_to(cr, w * 0.16, h * 0.12);
	cairo_line_to(cr, w * 0.16, h * 0.72);
	cairo_stroke(cr);
}

static void _glyph_accept(cairo_t * cr, double w, double h)
{
	/* green check mark */
	cairo_set_source_rgb(cr, 0.15, 0.65, 0.30);
	cairo_set_line_width(cr, w * 0.12);
	cairo_move_to(cr, w * 0.16, h * 0.55);
	cairo_line_to(cr, w * 0.40, h * 0.80);
	cairo_line_to(cr, w * 0.86, h * 0.20);
	cairo_stroke(cr);
}

static void _glyph_reject(cairo_t * cr, double w, double h)
{
	/* red cross */
	cairo_set_source_rgb(cr, 0.80, 0.20, 0.20);
	cairo_set_line_width(cr, w * 0.12);
	cairo_move_to(cr, w * 0.24, h * 0.24);
	cairo_line_to(cr, w * 0.76, h * 0.76);
	cairo_move_to(cr, w * 0.76, h * 0.24);
	cairo_line_to(cr, w * 0.24, h * 0.76);
	cairo_stroke(cr);
}

static void _glyph_pane(cairo_t * cr, double w, double h)
{
	/* document column beside a comments list - the reviewing pane */
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, w * 0.08, h * 0.25);
	cairo_line_to(cr, w * 0.50, h * 0.25);
	cairo_move_to(cr, w * 0.08, h * 0.45);
	cairo_line_to(cr, w * 0.50, h * 0.45);
	cairo_move_to(cr, w * 0.08, h * 0.65);
	cairo_line_to(cr, w * 0.40, h * 0.65);
	cairo_stroke(cr);
	/* the side pane */
	cairo_set_source_rgb(cr, 0.35, 0.5, 0.75);
	cairo_rectangle(cr, w * 0.58, h * 0.12, w * 0.34, h * 0.76);
	cairo_stroke(cr);
	cairo_move_to(cr, w * 0.62, h * 0.30);
	cairo_line_to(cr, w * 0.88, h * 0.30);
	cairo_move_to(cr, w * 0.62, h * 0.50);
	cairo_line_to(cr, w * 0.88, h * 0.50);
	cairo_move_to(cr, w * 0.62, h * 0.70);
	cairo_line_to(cr, w * 0.82, h * 0.70);
	cairo_stroke(cr);
}

static void _glyph_language(cairo_t * cr, double w, double h)
{
	/* globe with meridians */
	cairo_set_source_rgb(cr, 0.20, 0.45, 0.90);
	cairo_set_line_width(cr, 1.2);
	double r = w * 0.36;
	cairo_arc(cr, w * 0.5, h * 0.5, r, 0, 2 * M_PI);
	cairo_stroke(cr);
	/* equator */
	cairo_move_to(cr, w * 0.5 - r, h * 0.5);
	cairo_line_to(cr, w * 0.5 + r, h * 0.5);
	cairo_stroke(cr);
	/* vertical meridian ellipse */
	cairo_save(cr);
	cairo_translate(cr, w * 0.5, h * 0.5);
	cairo_scale(cr, 0.45, 1.0);
	cairo_arc(cr, 0, 0, r, 0, 2 * M_PI);
	cairo_restore(cr);
	cairo_stroke(cr);
	/* latitude curves, sagging toward the equator */
	cairo_move_to(cr, w * 0.5 - r * 0.82, h * 0.5 - r * 0.30);
	cairo_curve_to(cr, w * 0.5 - r * 0.40, h * 0.5 - r * 0.06,
				   w * 0.5 + r * 0.40, h * 0.5 - r * 0.06,
				   w * 0.5 + r * 0.82, h * 0.5 - r * 0.30);
	cairo_stroke(cr);
	cairo_move_to(cr, w * 0.5 - r * 0.82, h * 0.5 + r * 0.30);
	cairo_curve_to(cr, w * 0.5 - r * 0.40, h * 0.5 + r * 0.06,
				   w * 0.5 + r * 0.40, h * 0.5 + r * 0.06,
				   w * 0.5 + r * 0.82, h * 0.5 + r * 0.30);
	cairo_stroke(cr);
}

static void _glyph_compare(cairo_t * cr, double w, double h)
{
	/* two overlapping pages with text lines - Compare Documents */
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_set_line_width(cr, 1.0);
	/* back page */
	cairo_rectangle(cr, w * 0.34, h * 0.10, w * 0.48, h * 0.62);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_stroke(cr);
	/* front page */
	cairo_rectangle(cr, w * 0.14, h * 0.30, w * 0.48, h * 0.62);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_stroke(cr);
	for (int i = 0; i < 3; ++i)
	{
		cairo_move_to(cr, w * 0.20, h * (0.42 + i * 0.15));
		cairo_line_to(cr, w * 0.56, h * (0.42 + i * 0.15));
	}
	cairo_stroke(cr);
}

static void _glyph_combine(cairo_t * cr, double w, double h)
{
	/* two pages with a right-pointing merge arrow - Combine Documents */
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_set_line_width(cr, 1.0);
	/* left page */
	cairo_rectangle(cr, w * 0.08, h * 0.20, w * 0.30, h * 0.58);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_stroke(cr);
	/* right page */
	cairo_rectangle(cr, w * 0.62, h * 0.20, w * 0.30, h * 0.58);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_stroke(cr);
	for (int i = 0; i < 2; ++i)
	{
		cairo_move_to(cr, w * 0.12, h * (0.34 + i * 0.18));
		cairo_line_to(cr, w * 0.34, h * (0.34 + i * 0.18));
		cairo_move_to(cr, w * 0.66, h * (0.34 + i * 0.18));
		cairo_line_to(cr, w * 0.88, h * (0.34 + i * 0.18));
	}
	cairo_stroke(cr);
	/* merge arrow between the pages */
	cairo_set_source_rgb(cr, 0.20, 0.45, 0.90);
	cairo_set_line_width(cr, 1.8);
	cairo_move_to(cr, w * 0.40, h * 0.49);
	cairo_line_to(cr, w * 0.58, h * 0.49);
	cairo_stroke(cr);
	cairo_move_to(cr, w * 0.50, h * 0.40);
	cairo_line_to(cr, w * 0.60, h * 0.49);
	cairo_line_to(cr, w * 0.50, h * 0.58);
	cairo_stroke(cr);
}

static void _glyph_revfind(cairo_t * cr, double w, double h,
						   bool bNext)
{
	/* text lines with a small prev/next arrow over them */
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, w * 0.12, h * 0.30);
	cairo_line_to(cr, w * 0.88, h * 0.30);
	cairo_move_to(cr, w * 0.12, h * 0.55);
	cairo_line_to(cr, w * 0.88, h * 0.55);
	cairo_move_to(cr, w * 0.12, h * 0.80);
	cairo_line_to(cr, w * 0.70, h * 0.80);
	cairo_stroke(cr);
	cairo_set_source_rgb(cr, 0.20, 0.45, 0.90);
	cairo_set_line_width(cr, 1.8);
	double s = w * 0.16;
	double cy = h * 0.425;
	double cx = bNext ? w * 0.66 : w * 0.34;
	cairo_move_to(cr, cx + (bNext ? -s : s), cy - s);
	cairo_line_to(cr, cx, cy);
	cairo_line_to(cr, cx + (bNext ? -s : s), cy + s);
	cairo_stroke(cr);
}

static void _glyph_revprev(cairo_t * cr, double w, double h)
	{ _glyph_revfind(cr, w, h, false); }
static void _glyph_revnext(cairo_t * cr, double w, double h)
	{ _glyph_revfind(cr, w, h, true); }

static void _overlay_band_top(cairo_t * cr, double w, double /*h*/)
{
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_rectangle(cr, 0, 0, w, 4.5);
	cairo_fill(cr);
}

static void _overlay_band_bot(cairo_t * cr, double w, double h)
{
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_rectangle(cr, 0, h - 4.5, w, 4.5);
	cairo_fill(cr);
}

static void _overlay_pageno(cairo_t * cr, double w, double h)
{
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
						   CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, h * 0.5);
	cairo_move_to(cr, w * 0.30, h * 0.70);
	cairo_show_text(cr, "#");
}

static void _glyph_textbox(cairo_t * cr, double w, double h)
{
	/* dashed box with an A and a text line */
	cairo_set_source_rgb(cr, 0.35, 0.5, 0.75);
	cairo_set_line_width(cr, 1.0);
	const double dash[] = { 2.0, 1.6 };
	cairo_set_dash(cr, dash, 2, 0);
	cairo_rectangle(cr, 1.5, 3.5, w - 3.0, h - 7.0);
	cairo_stroke(cr);
	cairo_set_dash(cr, nullptr, 0, 0);
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
						   CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, h * 0.42);
	cairo_move_to(cr, 4.0, h * 0.62);
	cairo_show_text(cr, "A");
	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, w * 0.55, h * 0.42);
	cairo_line_to(cr, w - 4, h * 0.42);
	cairo_move_to(cr, w * 0.55, h * 0.62);
	cairo_line_to(cr, w - 4, h * 0.62);
	cairo_stroke(cr);
}

static void _glyph_wordart(cairo_t * cr, double w, double h)
{
	/* stylized A with accent */
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_ITALIC,
						   CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, h * 0.66);
	cairo_move_to(cr, w * 0.24, h * 0.72);
	cairo_show_text(cr, "A");
	cairo_set_line_width(cr, 1.4);
	cairo_move_to(cr, 3.0, h - 2.5);
	cairo_line_to(cr, w - 3.0, h - 2.5);
	cairo_stroke(cr);
}

static void _glyph_dropcap(cairo_t * cr, double w, double h)
{
	/* big D with wrapped text lines beside it */
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_select_font_face(cr, "Serif", CAIRO_FONT_SLANT_NORMAL,
						   CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, h * 0.72);
	cairo_move_to(cr, 1.5, h * 0.76);
	cairo_show_text(cr, "D");
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_set_line_width(cr, 1.0);
	for (int i = 0; i < 4; i++)
	{
		cairo_move_to(cr, w * 0.48, 4.0 + i * 4.2);
		cairo_line_to(cr, w - 2.0, 4.0 + i * 4.2);
	}
	cairo_stroke(cr);
}

static void _glyph_signature(cairo_t * cr, double w, double h)
{
	/* squiggle signature over a rule */
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 1.3);
	cairo_move_to(cr, 2.0, h * 0.55);
	cairo_curve_to(cr, w * 0.25, h * 0.15, w * 0.35, h * 0.75,
				   w * 0.5, h * 0.45);
	cairo_curve_to(cr, w * 0.6, h * 0.25, w * 0.7, h * 0.55,
				   w - 3.0, h * 0.35);
	cairo_stroke(cr);
	cairo_set_source_rgb(cr, 0.55, 0.6, 0.7);
	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, 2.0, h - 3.0);
	cairo_line_to(cr, w - 2.0, h - 3.0);
	cairo_stroke(cr);
}

static void _glyph_datetime(cairo_t * cr, double w, double h)
{
	/* calendar page + clock */
	cairo_set_source_rgb(cr, 0.35, 0.5, 0.75);
	cairo_set_line_width(cr, 1.1);
	cairo_rectangle(cr, 1.5, 3.5, w * 0.62, h - 6.0);
	cairo_stroke(cr);
	cairo_move_to(cr, 1.5, 7.5);
	cairo_line_to(cr, 1.5 + w * 0.62, 7.5);
	cairo_stroke(cr);
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	double cx = w * 0.70, cy = h * 0.62, r = w * 0.22;
	cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_set_line_width(cr, 1.2);
	cairo_move_to(cr, cx, cy);
	cairo_line_to(cr, cx, cy - r * 0.55);
	cairo_move_to(cr, cx, cy);
	cairo_line_to(cr, cx + r * 0.45, cy);
	cairo_stroke(cr);
}

static void _glyph_field(cairo_t * cr, double w, double h)
{
	/* grey field box with chevrons */
	cairo_set_source_rgba(cr, 0.6, 0.65, 0.75, 0.35);
	cairo_rectangle(cr, 1.5, 4.0, w - 3.0, h - 8.0);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, 0.35, 0.5, 0.75);
	cairo_set_line_width(cr, 1.1);
	cairo_rectangle(cr, 1.5, 4.0, w - 3.0, h - 8.0);
	cairo_stroke(cr);
	cairo_set_line_width(cr, 1.4);
	cairo_move_to(cr, w * 0.32, h * 0.60);
	cairo_line_to(cr, w * 0.50, h * 0.40);
	cairo_line_to(cr, w * 0.68, h * 0.60);
	cairo_stroke(cr);
}

static void _glyph_object(cairo_t * cr, double w, double h)
{
	/* small window/object box with title bar */
	cairo_set_source_rgb(cr, 0.35, 0.5, 0.75);
	cairo_set_line_width(cr, 1.1);
	cairo_rectangle(cr, 1.5, 4.0, w - 3.0, h - 7.0);
	cairo_stroke(cr);
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_rectangle(cr, 1.5, 4.0, w - 3.0, 4.5);
	cairo_fill(cr);
	cairo_rectangle(cr, w * 0.30, h * 0.55, w * 0.40, h * 0.22);
	cairo_stroke(cr);
}

static void _glyph_vtextbox(cairo_t * cr, double w, double h)
{
	/* tall dashed box with a sideways A - vertical text box */
	cairo_set_source_rgb(cr, 0.35, 0.5, 0.75);
	cairo_set_line_width(cr, 1.0);
	const double dash[] = { 2.0, 1.6 };
	cairo_set_dash(cr, dash, 2, 0);
	cairo_rectangle(cr, w * 0.28, 1.5, w * 0.44, h - 3.0);
	cairo_stroke(cr);
	cairo_set_dash(cr, nullptr, 0, 0);
	cairo_save(cr);
	cairo_translate(cr, w * 0.50, h * 0.30);
	cairo_rotate(cr, G_PI / 2.0);
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
						   CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, h * 0.34);
	cairo_move_to(cr, 0, 0);
	cairo_show_text(cr, "A");
	cairo_restore(cr);
}

static void _glyph_equation(cairo_t * cr, double w, double h)
{
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_select_font_face(cr, "Serif", CAIRO_FONT_SLANT_ITALIC,
						   CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, h * 0.66);
	cairo_move_to(cr, w * 0.24, h * 0.72);
	cairo_show_text(cr, "\xcf\x80");	/* π */
}

static void _glyph_omega(cairo_t * cr, double w, double h)
{
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_select_font_face(cr, "Serif", CAIRO_FONT_SLANT_NORMAL,
						   CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, h * 0.66);
	cairo_move_to(cr, w * 0.20, h * 0.74);
	cairo_show_text(cr, "\xce\xa9");	/* Ω */
}

static void _overlay_lrm(cairo_t * cr, double w, double h)
{
	_overlay_dir_arrow(cr, w, h, false);
}

static void _overlay_rlm(cairo_t * cr, double w, double h)
{
	_overlay_dir_arrow(cr, w, h, true);
}

static void _overlay_orient_arrow(cairo_t * cr, double w, double h)
{
	/* circular arrow at the bottom-right */
	double cx = w - 7, cy = h - 7, r = 5;
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 1.3);
	cairo_arc(cr, cx, cy, r, -0.4, 4.4);
	cairo_stroke(cr);
	cairo_move_to(cr, cx + r + 1.5, cy - 2.5);
	cairo_line_to(cr, cx + r + 1.5, cy + 1.5);
	cairo_line_to(cr, cx + r - 1.5, cy - 0.5);
	cairo_close_path(cr);
	cairo_fill(cr);
}

static void _overlay_size_arrows(cairo_t * cr, double w, double h)
{
	/* vertical double-arrow left of the page */
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 1.1);
	double x = 2.0, y0 = 3.0, y1 = h - 3.0;
	cairo_move_to(cr, x, y0 + 2); cairo_line_to(cr, x, y1 - 2);
	cairo_move_to(cr, x - 2, y0 + 3); cairo_line_to(cr, x, y0);
	cairo_line_to(cr, x + 2, y0 + 3);
	cairo_move_to(cr, x - 2, y1 - 3); cairo_line_to(cr, x, y1);
	cairo_line_to(cr, x + 2, y1 - 3);
	cairo_stroke(cr);
}

static void _overlay_break_dash(cairo_t * cr, double w, double h)
{
	/* dashed mid-line + small arrow - a page/section break */
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 1.0);
	const double dash[] = { 2.5, 2.0 };
	cairo_set_dash(cr, dash, 2, 0);
	double y = h / 2.0;
	cairo_move_to(cr, 3, y);
	cairo_line_to(cr, w - 6, y);
	cairo_stroke(cr);
	cairo_set_dash(cr, nullptr, 0, 0);
	cairo_move_to(cr, w - 8, y - 3);
	cairo_line_to(cr, w - 4, y);
	cairo_line_to(cr, w - 8, y + 3);
	cairo_close_path(cr);
	cairo_fill(cr);
}

static void _overlay_hyphen(cairo_t * cr, double w, double h)
{
	/* "a-" over "bc" letterforms */
	cairo_set_source_rgb(cr, 0.35, 0.35, 0.4);
	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
						   CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, h * 0.42);
	cairo_move_to(cr, 2, h * 0.42);
	cairo_show_text(cr, "a-");
	cairo_move_to(cr, 2, h * 0.9);
	cairo_show_text(cr, "bc");
}

/* Z-order icons: stack of squares rising to the right, blue top
 * square, and a bold arrow - up for Bring Forward, down for Send
 * Backward.  Drawn bare (no page) so they read differently from
 * the page-setup glyphs. */
static void _overlay_zorder_stack(cairo_t * cr, double w, double h,
								  bool bUp)
{
	double s = w * 0.30;				/* square size */
	double step = s * 0.38;				/* stair offset */
	double x0 = w * 0.06, y0 = h - s - 1.5;
	cairo_set_line_width(cr, 1.0);
	for (int i = 0; i < 3; ++i)
	{
		double x = x0 + i * step;
		double y = y0 - i * step;
		if (i == 2)
			cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);	/* front = blue */
		else
			cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
		cairo_rectangle(cr, x, y, s, s);
		cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, 0.45, 0.47, 0.55);
		cairo_stroke(cr);
	}
	/* bold vertical arrow on the right */
	double ax = w * 0.82;
	double top = h * 0.12, bot = h * 0.88;
	cairo_set_source_rgb(cr, 0.15, 0.35, 0.8);
	cairo_set_line_width(cr, 1.7);
	cairo_move_to(cr, ax, bUp ? bot : top);
	cairo_line_to(cr, ax, bUp ? top + 3.0 : bot - 3.0);
	cairo_stroke(cr);
	double hy = bUp ? top : bot;
	double dir = bUp ? 1.0 : -1.0;
	cairo_move_to(cr, ax - 3.2, hy + dir * 4.5);
	cairo_line_to(cr, ax, hy);
	cairo_line_to(cr, ax + 3.2, hy + dir * 4.5);
	cairo_close_path(cr);
	cairo_fill(cr);
}

static void _overlay_bring_forward(cairo_t * cr, double w, double h)
{
	_overlay_zorder_stack(cr, w, h, true);
}

static void _overlay_send_backward(cairo_t * cr, double w, double h)
{
	_overlay_zorder_stack(cr, w, h, false);
}

/* paint-drop badge for Page Color */
static void _overlay_color_drop(cairo_t * cr, double w, double h)
{
	double cx = w - 6.0, cy = h - 7.0, r = 4.0;
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_arc(cr, cx, cy + 1.0, r, 0, 2 * M_PI);
	cairo_move_to(cr, cx - r * 0.8, cy - 0.5);
	cairo_line_to(cr, cx, cy - r - 2.0);
	cairo_line_to(cr, cx + r * 0.8, cy - 0.5);
	cairo_close_path(cr);
	cairo_fill(cr);
}

/* picture badge for Page Image: frame + mountains + sun */
static void _overlay_page_image(cairo_t * cr, double w, double h)
{
	double pw = w * 0.42, ph = h * 0.30;
	double x = w - pw - 1.0, y = h - ph - 1.0;
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_rectangle(cr, x, y, pw, ph);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 0.9);
	cairo_stroke(cr);
	/* sun */
	cairo_arc(cr, x + pw * 0.72, y + ph * 0.3, ph * 0.13, 0, 2 * M_PI);
	cairo_fill(cr);
	/* mountains */
	cairo_move_to(cr, x + 1.0, y + ph - 1.0);
	cairo_line_to(cr, x + pw * 0.38, y + ph * 0.35);
	cairo_line_to(cr, x + pw * 0.62, y + ph - 1.0);
	cairo_close_path(cr);
	cairo_move_to(cr, x + pw * 0.45, y + ph - 1.0);
	cairo_line_to(cr, x + pw * 0.72, y + ph * 0.5);
	cairo_line_to(cr, x + pw - 1.0, y + ph - 1.0);
	cairo_close_path(cr);
	cairo_set_source_rgb(cr, 0.55, 0.58, 0.65);
	cairo_fill(cr);
}

/* overlapping-shapes badge for Group Objects */
static void _overlay_group(cairo_t * cr, double w, double h)
{
	double x = w - 12.0, y = h - 11.0;
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_rectangle(cr, x, y + 3.5, 7.0, 7.0);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_rectangle(cr, x + 4.5, y, 7.0, 7.0);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 0.9);
	cairo_stroke(cr);
}

/* circular-arrow badge for Rotate */
static void _overlay_rotate(cairo_t * cr, double w, double h)
{
	double cx = w - 7.0, cy = h - 7.0, r = 4.6;
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 1.3);
	cairo_arc(cr, cx, cy, r, -0.4, 4.0);
	cairo_stroke(cr);
	/* arrowhead at the arc end, pointing along the sweep */
	double ax = cx + r * cos(4.0), ay = cy + r * sin(4.0);
	cairo_move_to(cr, ax + 2.6, ay - 2.2);
	cairo_line_to(cr, ax - 2.6, ay + 0.4);
	cairo_line_to(cr, ax + 0.4, ay + 3.0);
	cairo_close_path(cr);
	cairo_fill(cr);
}

/* leader-dot column for the Table of Contents button */
static void _overlay_toc(cairo_t * cr, double w, double h)
{
	cairo_set_source_rgb(cr, 0.45, 0.45, 0.45);
	double x0 = w * 0.78 - 8.0;
	for (int i = 0; i < 3; i++)
	{
		double y = h * 0.30 + i * h * 0.22;
		for (int d = 0; d < 3; d++)
		{
			cairo_arc(cr, x0 + d * 3.0, y, 0.7, 0, 2 * G_PI);
			cairo_fill(cr);
		}
	}
}

/* down-caret badge for Add Text */
static void _overlay_addtext(cairo_t * cr, double w, double h)
{
	double x = w - 9.0, y = h - 9.0;
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_move_to(cr, x, y);
	cairo_line_to(cr, x + 7.0, y);
	cairo_line_to(cr, x + 3.5, y + 4.5);
	cairo_close_path(cr);
	cairo_fill(cr);
}

/* separator + note lines for footnote buttons */
static void _overlay_footnote(cairo_t * cr, double w, double h)
{
	double pw = w * 0.78;
	double px = (w - pw) / 2.0;
	cairo_set_source_rgb(cr, 0.55, 0.55, 0.55);
	cairo_set_line_width(cr, 0.8);
	cairo_move_to(cr, px + 3.0, h - 8.0);
	cairo_line_to(cr, px + pw * 0.55, h - 8.0);
	cairo_stroke(cr);
	cairo_set_line_width(cr, 0.9);
	cairo_move_to(cr, px + 3.0, h - 5.0);
	cairo_line_to(cr, px + pw * 0.45, h - 5.0);
	cairo_stroke(cr);
	/* superscript ref mark */
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_rectangle(cr, px + pw * 0.60, h * 0.24, 2.6, 2.6);
	cairo_fill(cr);
}

/* down-arrow badge for Show Notes */
static void _overlay_shownotes(cairo_t * cr, double w, double h)
{
	double x = w - 10.0, y = h - 11.0;
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 1.2);
	cairo_move_to(cr, x + 3.0, y);
	cairo_line_to(cr, x + 3.0, y + 5.5);
	cairo_stroke(cr);
	cairo_move_to(cr, x, y + 3.5);
	cairo_line_to(cr, x + 3.0, y + 7.5);
	cairo_line_to(cr, x + 6.0, y + 3.5);
	cairo_close_path(cr);
	cairo_fill(cr);
}

/* centered text badge used by several References glyphs */
static void _badge_text(cairo_t * cr, const char * s,
						double cx, double cy, double size)
{
	cairo_select_font_face(cr, "sans",
						   CAIRO_FONT_SLANT_NORMAL,
						   CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, size);
	cairo_text_extents_t ext;
	cairo_text_extents(cr, s, &ext);
	cairo_move_to(cr, cx - ext.width / 2.0, cy + ext.height / 2.0);
	cairo_show_text(cr, s);
}

/* open-quote mark + cite line for Insert Citation */
static void _overlay_citation(cairo_t * cr, double w, double h)
{
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	_badge_text(cr, "\xE2\x80\x9C", w * 0.5, h * 0.42, h * 0.7);
	cairo_set_line_width(cr, 1.1);
	cairo_move_to(cr, w * 0.30, h * 0.78);
	cairo_line_to(cr, w * 0.66, h * 0.78);
	cairo_stroke(cr);
}

/* bookshelf for Manage Sources */
static void _overlay_sources(cairo_t * cr, double w, double h)
{
	static const double hues[3][3] = {
		{ 0.2, 0.45, 0.9 }, { 0.55, 0.65, 0.35 }, { 0.8, 0.45, 0.3 }
	};
	static const double hts[3] = { 0.62, 0.50, 0.58 };
	double bw = w / 5.0, gap = 1.3;
	double x0 = (w - 3.0 * bw - 2.0 * gap) / 2.0;
	double base = h * 0.82;
	for (int i = 0; i < 3; ++i)
	{
		double bh = h * hts[i];
		double x = x0 + i * (bw + gap);
		cairo_set_source_rgb(cr, hues[i][0], hues[i][1], hues[i][2]);
		cairo_rectangle(cr, x, base - bh, bw, bh);
		cairo_fill(cr);
		cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.75);
		cairo_rectangle(cr, x + 0.8, base - bh + 1.6, bw - 1.6, 1.3);
		cairo_fill(cr);
	}
	cairo_set_source_rgb(cr, 0.45, 0.45, 0.45);
	cairo_set_line_width(cr, 1.2);
	cairo_move_to(cr, x0 - 1.5, base + 0.8);
	cairo_line_to(cr, x0 + 3.0 * bw + 2.0 * gap + 1.5, base + 0.8);
	cairo_stroke(cr);
}

/* bulleted list for Bibliography */
static void _overlay_bibliography(cairo_t * cr, double w, double h)
{
	double x0 = w * 0.14, x1 = w * 0.88;
	double y0 = h * 0.18, dy = h * 0.22;
	for (int i = 0; i < 3; ++i)
	{
		double y = y0 + i * dy;
		cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
		cairo_arc(cr, x0 + 1.7, y + dy * 0.42, 1.8, 0, 2 * G_PI);
		cairo_fill(cr);
		cairo_set_source_rgb(cr, 0.5, 0.55, 0.65);
		double ln = (x1 - x0 - 6.0) * (i == 2 ? 0.72 : 1.0);
		cairo_rectangle(cr, x0 + 5.5, y + dy * 0.16, ln, dy * 0.5);
		cairo_fill(cr);
	}
}

/* framed figure over a caption line for Insert Caption */
static void _overlay_caption(cairo_t * cr, double w, double h)
{
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 0.9);
	cairo_rectangle(cr, w - 11.5, h - 11.0, 7.0, 4.5);
	cairo_stroke(cr);
	cairo_move_to(cr, w - 11.5, h - 5.2);
	cairo_line_to(cr, w - 4.5, h - 5.2);
	cairo_stroke(cr);
}

/* labelled list rows for Insert Table of Figures */
static void _overlay_tof(cairo_t * cr, double w, double h)
{
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	double x = w - 12.0, y = h - 11.0;
	for (int i = 0; i < 3; i++)
	{
		cairo_rectangle(cr, x, y + i * 3.4, 2.2, 2.2);
		cairo_fill(cr);
		cairo_set_line_width(cr, 0.9);
		cairo_move_to(cr, x + 3.4, y + i * 3.4 + 1.1);
		cairo_line_to(cr, x + 10.0, y + i * 3.4 + 1.1);
		cairo_stroke(cr);
	}
}

/* interlinked rings for Cross-reference */
static void _overlay_xref(cairo_t * cr, double w, double h)
{
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 1.1);
	cairo_arc(cr, w - 8.6, h - 8.4, 2.5, 0, 2 * G_PI);
	cairo_stroke(cr);
	cairo_arc(cr, w - 4.8, h - 6.6, 2.5, 0, 2 * G_PI);
	cairo_stroke(cr);
}

/* price-tag mark for Mark Entry */
static void _overlay_markentry(cairo_t * cr, double w, double h)
{
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_move_to(cr, w - 12.0, h - 10.5);
	cairo_line_to(cr, w - 5.5, h - 10.5);
	cairo_line_to(cr, w - 3.2, h - 8.0);
	cairo_line_to(cr, w - 5.5, h - 5.5);
	cairo_line_to(cr, w - 12.0, h - 5.5);
	cairo_close_path(cr);
	cairo_fill(cr);
}

/* nested index lines for Insert Index */
static void _overlay_index(cairo_t * cr, double w, double h)
{
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 0.9);
	double x = w - 12.0, y = h - 10.5;
	for (int i = 0; i < 3; i++)
	{
		double xi = x + (i == 1 ? 2.4 : 0.0);
		cairo_move_to(cr, xi, y + i * 3.4);
		cairo_line_to(cr, x + 10.0, y + i * 3.4);
		cairo_stroke(cr);
	}
}

/* section sign for Mark Citation */
static void _overlay_markcit(cairo_t * cr, double w, double h)
{
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	_badge_text(cr, "\xC2\xA7", w - 7.0, h - 7.5, 9.5);	/* § */
}

/* two-column entries for Insert Table of Authorities */
static void _overlay_toa(cairo_t * cr, double w, double h)
{
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 0.9);
	double x = w - 13.0, y = h - 10.5;
	for (int i = 0; i < 3; i++)
	{
		cairo_move_to(cr, x, y + i * 3.4);
		cairo_line_to(cr, x + 6.0, y + i * 3.4);
		cairo_stroke(cr);
		cairo_move_to(cr, x + 7.6, y + i * 3.4);
		cairo_line_to(cr, x + 11.0, y + i * 3.4);
		cairo_stroke(cr);
	}
}

/* ids that have a drawn ribbon glyph even though they are plain
 * menu buttons (References tab items have no stock icon) */
static bool _has_drawn_icon(XAP_Menu_Id id)
{
	switch (id)
	{
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_FOOTNOTE:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_ENDNOTE:
	case (XAP_Menu_Id)AP_MENU_ID_FMT_FOOTNOTES:
	case (XAP_Menu_Id)AP_MENU_ID_REF_UPDATETOC:
	case (XAP_Menu_Id)AP_MENU_ID_REF_SHOWNOTES:
	case (XAP_Menu_Id)AP_MENU_ID_REF_UPDATEINDEX:
	case (XAP_Menu_Id)AP_MENU_ID_REF_UPDATETOA:
	case (XAP_Menu_Id)AP_MENU_ID_REF_INSERTINDEX:
	case (XAP_Menu_Id)AP_MENU_ID_REF_INSERTTOA:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_DIRECTIONMARKER_LRM:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_DIRECTIONMARKER_RLM:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_COVERPAGE:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_BLANKPAGE:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_PAGEBREAK:
	case (XAP_Menu_Id)AP_MENU_ID_TABLE_INSERT_TABLE:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_PICTURES:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_SHAPES:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_ICONS:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_3DMODELS:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_SCREENSHOT:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_MEDIA:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_HYPERLINK:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_BOOKMARK:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_INSERT:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_DELETE:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_MENUPOP_DELETE:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_RESOLVE:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_PREV:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_NEXT:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_MENUPOP_SHOW:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_TOGGLE_DISPLAY:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_SPELLING_MENUPOP:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_SPELL:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_WORDCOUNT:
	case (XAP_Menu_Id)AP_MENU_ID_FMT_LANGUAGE:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MARK:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_AUTO:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_NEW_REVISION:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_PURGE:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_SHOW:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_SET_VIEW_LEVEL:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_ACCEPT_REVISION:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_REJECT_REVISION:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_TRACK:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_DISPLAY:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_ACCEPT:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_REJECT:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_PANE:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_COMPARE_DOCUMENTS:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_COMPARE:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_COMBINE_DOCUMENTS:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_FIND_PREV:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_FIND_NEXT:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_HEADER:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_FOOTER:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_PAGENO:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_TEXTBOX:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_WORDART:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_DROPCAP:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_SIGNATURE:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_DATETIME:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_FIELD:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_OBJECT:
	case (XAP_Menu_Id)AP_MENU_ID_EDIT_LATEXEQUATION:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_SYMBOL:
		return true;
	default:
		return false;
	}
}

/* dispatch a drawn glyph for the Layout menu ids */
static GtkWidget * _layout_icon(XAP_Menu_Id id, int w, int h)
{
	_PageSpec spec = { 0.12, 0.12, 0.15, 0.15, 1, false, false, 0, false };
	void (*extra)(cairo_t *, double, double) = nullptr;

	switch (id)
	{
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_MARGINS:
		extra = _overlay_margin_corners;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_ORIENTATION:
		extra = _overlay_orient_arrow;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_SIZE:
		extra = _overlay_size_arrows;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_FMT_COLUMNS:
		spec.cols = 2;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_BREAKS:
		spec.fold = 1;
		extra = _overlay_break_dash;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_LINENUMBERS:
		spec.linenum = true;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_HYPHENATION:
		extra = _overlay_hyphen;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_POSITION:
		spec.fold = 1;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_WRAP:
		spec.cols = 2;
		extra = _overlay_break_dash;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_ALIGNOBJECTS:
		spec.mr = 0.45;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_BRINGFORWARD:
		spec.bare = true;
		extra = _overlay_bring_forward;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_SENDBACKWARD:
		spec.bare = true;
		extra = _overlay_send_backward;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_GROUPOBJECTS:
		extra = _overlay_group;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_LAYOUT_ROTATE:
		extra = _overlay_rotate;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_FMT_BACKGROUND_PAGE_COLOR:
		extra = _overlay_color_drop;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_FMT_BACKGROUND_PAGE_IMAGE:
		extra = _overlay_page_image;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_TOCPOP:
		extra = _overlay_toc;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_ADDTEXT:
		extra = _overlay_addtext;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_NEXTFN:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_FOOTNOTE:
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_ENDNOTE:
	case (XAP_Menu_Id)AP_MENU_ID_FMT_FOOTNOTES:
		extra = _overlay_footnote;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_SHOWNOTES:
		extra = _overlay_shownotes;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_UPDATETOC:
	case (XAP_Menu_Id)AP_MENU_ID_REF_UPDATEINDEX:
	case (XAP_Menu_Id)AP_MENU_ID_REF_UPDATETOA:
		extra = _overlay_rotate;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_CITATION:
		extra = _overlay_citation;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_SOURCES:
		spec.bare = true;
		extra = _overlay_sources;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_BIBLIOGRAPHY:
		spec.bare = true;
		extra = _overlay_bibliography;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_CAPTION:
		extra = _overlay_caption;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_TOF:
		extra = _overlay_tof;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_XREF:
		extra = _overlay_xref;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_MARKENTRY:
		extra = _overlay_markentry;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_INSERTINDEX:
		extra = _overlay_index;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_MARKCIT:
		extra = _overlay_markcit;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_INSERTTOA:
		extra = _overlay_toa;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_DIRECTIONMARKER_LRM:
		extra = _overlay_lrm;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_DIRECTIONMARKER_RLM:
		extra = _overlay_rlm;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_COVERPAGE:
		extra = _overlay_coverband;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_BLANKPAGE:
		extra = _overlay_blankpage;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_PAGEBREAK:
		extra = _overlay_pagebreak_arrow;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TABLE_INSERT_TABLE:
		spec.bare = true;
		extra = _glyph_table;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_PICTURES:
		spec.bare = true;
		extra = _glyph_picture;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_SHAPES:
		spec.bare = true;
		extra = _glyph_shapes;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_ICONS:
		spec.bare = true;
		extra = _glyph_smiley;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_3DMODELS:
		spec.bare = true;
		extra = _glyph_cube;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_SCREENSHOT:
		spec.bare = true;
		extra = _glyph_camera;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_MEDIA:
		spec.bare = true;
		extra = _glyph_media;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_HYPERLINK:
		spec.bare = true;
		extra = _glyph_link;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_BOOKMARK:
		spec.bare = true;
		extra = _glyph_bookmark;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_INSERT:
		spec.bare = true;
		extra = _glyph_comment_new;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_DELETE:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_MENUPOP_DELETE:
		spec.bare = true;
		extra = _glyph_comment_del;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_RESOLVE:
		spec.bare = true;
		extra = _glyph_comment_resolve;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_PREV:
		spec.bare = true;
		extra = _glyph_comment_prev;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_NEXT:
		spec.bare = true;
		extra = _glyph_comment_next;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_MENUPOP_SHOW:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_TOGGLE_DISPLAY:
		spec.bare = true;
		extra = _glyph_comment;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_SPELLING_MENUPOP:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_SPELL:
		spec.bare = true;
		extra = _glyph_spell;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_WORDCOUNT:
		spec.bare = true;
		extra = _glyph_wordcount;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_FMT_LANGUAGE:
		spec.bare = true;
		extra = _glyph_language;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_TRACK:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MARK:
		spec.bare = true;
		extra = _glyph_track;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_AUTO:
		spec.bare = true;
		extra = _glyph_revauto;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_NEW_REVISION:
		spec.bare = true;
		extra = _glyph_revnew;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_PURGE:
		spec.bare = true;
		extra = _glyph_revpurge;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_SHOW:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_SET_VIEW_LEVEL:
		spec.bare = true;
		extra = _glyph_markup;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_ACCEPT_REVISION:
		spec.bare = true;
		extra = _glyph_accept;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_REJECT_REVISION:
		spec.bare = true;
		extra = _glyph_reject;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_DISPLAY:
		spec.bare = true;
		extra = _glyph_markup;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_ACCEPT:
		spec.bare = true;
		extra = _glyph_accept;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_REJECT:
		spec.bare = true;
		extra = _glyph_reject;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_PANE:
		spec.bare = true;
		extra = _glyph_pane;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_COMPARE_DOCUMENTS:
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_COMPARE:
		spec.bare = true;
		extra = _glyph_compare;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_COMBINE_DOCUMENTS:
		spec.bare = true;
		extra = _glyph_combine;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_FIND_PREV:
		spec.bare = true;
		extra = _glyph_revprev;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_FIND_NEXT:
		spec.bare = true;
		extra = _glyph_revnext;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_HEADER:
		extra = _overlay_band_top;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_FOOTER:
		extra = _overlay_band_bot;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_PAGENO:
		extra = _overlay_pageno;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_TEXTBOX:
		spec.bare = true;
		extra = _glyph_textbox;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_WORDART:
		spec.bare = true;
		extra = _glyph_wordart;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_DROPCAP:
		spec.bare = true;
		extra = _glyph_dropcap;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_SIGNATURE:
		spec.bare = true;
		extra = _glyph_signature;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_DATETIME:
		spec.bare = true;
		extra = _glyph_datetime;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_FIELD:
		spec.bare = true;
		extra = _glyph_field;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_OBJECT:
		spec.bare = true;
		extra = _glyph_object;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_EDIT_LATEXEQUATION:
		spec.bare = true;
		extra = _glyph_equation;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_INSERT_SYMBOL:
		spec.bare = true;
		extra = _glyph_omega;
		break;
	default:
		break;
	}
	return _glyph_widget(spec, w, h, extra);
}

/* labels for AP_RIBBON_ITEM_DEAD placeholders - these Word groups have
 * no engine support yet, so the buttons render insensitive */
static const _ribbon_kv s_ribbon_dead_labels[] =
{
	{ "citation",    "Insert Citation" },
	{ "sources",     "Manage Sources" },
	{ "bibliography","Bibliography" },
	{ "caption",     "Insert Caption" },
	{ "figures",     "Insert Table of Figures" },
	{ "xref",        "Cross-reference" },
	{ "index",       "Insert Index" },
	{ "markentry",   "Mark Entry" },
	{ "updateindex", "Update Index" },
	{ "toa",         "Insert Table of Authorities" },
	{ "markcitation","Mark Citation" },
	{ "updatetoa",   "Update Table" },
	{ nullptr,        nullptr }
};

static const char * s_ribbon_dead_keys[] =
{
	"citation", "sources", "bibliography", "caption", "figures",
	"xref", "index", "markentry", "updateindex", "toa",
	"markcitation", "updatetoa"
};

GtkWidget * AP_UnixRibbon::_makeDeadButton(uint16_t id)
{
	const char * szLabel = "Unsupported";
	if (id < G_N_ELEMENTS(s_ribbon_dead_keys))
		szLabel = _ribbon_label(s_ribbon_dead_keys[id],
								s_ribbon_dead_labels);

	GtkWidget * btn = gtk_button_new_with_label(szLabel);
	GtkWidget * wLabel = gtk_button_get_child(GTK_BUTTON(btn));
	gtk_label_set_ellipsize(GTK_LABEL(wLabel), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars(GTK_LABEL(wLabel), 18);
	gtk_widget_set_sensitive(btn, FALSE);
	gtk_widget_set_tooltip_text(btn,
							  "Not supported by this build");
	return btn;
}

/* Word-style large dropdown button: icon over caption + down arrow */
GtkWidget * AP_UnixRibbon::_makeLargeMenuButton(XAP_Menu_Id id,
											  GtkWidget * popover,
											  uint8_t flags)
{
	const EV_Menu_Label * pLabel =
		m_pMenu ? m_pMenu->getLabelSet()->getLabel(id) : nullptr;
	const char * szLabel = pLabel ? pLabel->getMenuLabel() : nullptr;

	char label[64];
	_ribbon_strip_mnemonic(szLabel ? szLabel : "", label, sizeof(label));
	GtkWidget * mb = gtk_menu_button_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	GtkWidget * icon = _layout_icon(id, 24, 24);
	gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(box), icon);
	/* Word breaks large-button captions onto two lines */
	char caption[64];
	strncpy(caption, label, sizeof(caption) - 1);
	caption[sizeof(caption) - 1] = 0;
	char * sp = strchr(caption, ' ');
	if (sp)
		*sp = '\n';
	GtkWidget * wLabel = gtk_label_new(caption);
	gtk_label_set_wrap(GTK_LABEL(wLabel), TRUE);
	gtk_label_set_wrap_mode(GTK_LABEL(wLabel), PANGO_WRAP_WORD);
	gtk_label_set_justify(GTK_LABEL(wLabel), GTK_JUSTIFY_CENTER);
	gtk_label_set_lines(GTK_LABEL(wLabel), 2);
	gtk_label_set_max_width_chars(GTK_LABEL(wLabel),
		(flags & AP_RIBBON_FLAG_SLIM) ? 10 : 12);
	gtk_box_append(GTK_BOX(box), wLabel);
	gtk_menu_button_set_child(GTK_MENU_BUTTON(mb), box);
	gtk_menu_button_set_direction(GTK_MENU_BUTTON(mb), GTK_ARROW_DOWN);
	gtk_menu_button_set_has_frame(GTK_MENU_BUTTON(mb), FALSE);
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(mb), popover);
	if (flags & AP_RIBBON_FLAG_SLIM)
		_slim_widget_tree(mb);

	const char * szStatus = pLabel ? pLabel->getMenuStatusMessage() : nullptr;
	if (szStatus && *szStatus && strcmp(szStatus, " ") != 0)
		gtk_widget_set_tooltip_text(mb, szStatus);
	return mb;
}

/* a popover row: [slot] name\n detail  - clicked runs an edit method.
 * The leading slot is a fixed-width column so rows line up whether
 * they carry an icon, a check mark, or nothing. */
GtkWidget * AP_UnixRibbon::_presetRow(const char * szName,
									  const char * szDetail,
									  GtkWidget * icon,
									  const char * szMethod,
									  const char * szData,
									  bool bSensitive)
{
	GtkWidget * btn = gtk_button_new();
	GtkWidget * row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

	GtkWidget * slot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_size_request(slot, 20, -1);
	if (icon)
	{
		/* keep the slot width fixed: if the child expands, GTK
		 * splits the row's spare width between the slot and the
		 * text column and rows with shorter captions drift right */
		gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
		gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(slot), icon);
	}
	gtk_box_append(GTK_BOX(row), slot);

	GtkWidget * texts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
	gtk_widget_set_valign(texts, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand(texts, TRUE);
	GtkWidget * wName = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(wName), szName);
	gtk_label_set_wrap(GTK_LABEL(wName), TRUE);
	gtk_widget_set_halign(wName, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(texts), wName);
	if (szDetail && *szDetail)
	{
		GtkWidget * wDet = gtk_label_new(nullptr);
		char * mk = g_markup_printf_escaped(
			"<span size='small' alpha='65%%'>%s</span>", szDetail);
		gtk_label_set_markup(GTK_LABEL(wDet), mk);
		g_free(mk);
		gtk_label_set_wrap(GTK_LABEL(wDet), TRUE);
		gtk_label_set_max_width_chars(GTK_LABEL(wDet), 34);
		gtk_widget_set_halign(wDet, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(texts), wDet);
	}
	gtk_box_append(GTK_BOX(row), texts);
	gtk_button_set_child(GTK_BUTTON(btn), row);

	gtk_widget_add_css_class(btn, "flat");
	gtk_widget_set_sensitive(btn, bSensitive);
	if (!bSensitive)
		gtk_widget_set_tooltip_text(btn,
									"Not supported by the layout engine yet");

	if (szMethod && *szMethod)
	{
		g_object_set_data_full(G_OBJECT(btn), "abi-em-method",
							   g_strdup(szMethod), g_free);
		if (szData)
			g_object_set_data_full(G_OBJECT(btn), "abi-em-data",
								   g_strdup(szData), g_free);
		g_signal_connect(btn, "clicked",
						 G_CALLBACK(_s_popover_em_clicked), this);
	}
	return btn;
}

static GtkWidget * _popover_section_label(const char * szText)
{
	GtkWidget * l = gtk_label_new(nullptr);
	char * mk = g_markup_printf_escaped(
		"<span weight='bold' alpha='75%%'>%s</span>", szText);
	gtk_label_set_markup(GTK_LABEL(l), mk);
	g_free(mk);
	gtk_widget_set_halign(l, GTK_ALIGN_START);
	gtk_widget_set_margin_top(l, 4);
	gtk_widget_set_margin_bottom(l, 2);
	return l;
}

static GtkWidget * _popover_new_box(GtkWidget ** box)
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_top(b, 4);
	gtk_widget_set_margin_bottom(b, 4);
	gtk_widget_set_margin_start(b, 4);
	gtk_widget_set_margin_end(b, 4);
	gtk_popover_set_child(GTK_POPOVER(popover), b);
	*box = b;
	return popover;
}

/* ruler units (in|cm|mm) and a "x cm"-style formatted margin */
static UT_Dimension _ruler_units()
{
	UT_Dimension u = DIM_IN;
	std::string ru;
	if (XAP_App::getApp()->getPrefsValue(AP_PREF_KEY_RulerUnits, ru))
	{
		UT_Dimension d = UT_determineDimension(ru.c_str());
		if (d == DIM_CM || d == DIM_MM || d == DIM_IN)
			u = d;
	}
	return u;
}

static std::string _fmt_dim(double inches, UT_Dimension u)
{
	double v = UT_convertInchesToDimension(inches, u);
	char buf[128];
	snprintf(buf, sizeof(buf), "%.2f %s", v, UT_dimensionName(u));
	return buf;
}

/* Margins dropdown: Word-style preset gallery + Custom Margins… */
GtkWidget * AP_UnixRibbon::_makeMarginsPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);
	UT_Dimension u = _ruler_units();

	/* current margins, inches */
	double ct = 1.0, cb = 1.0, cl = 1.0, cr = 1.0;
	FV_View * pView = static_cast<FV_View *>(
		m_pFrame ? m_pFrame->getCurrentView() : nullptr);
	if (pView)
	{
		PP_PropertyVector props;
		if (pView->getSectionFormat(props))
		{
			const std::string & s = PP_getAttribute("page-margin-top", props);
			if (!s.empty()) ct = UT_convertToInches(s.c_str());
			const std::string & s2 = PP_getAttribute("page-margin-bottom", props);
			if (!s2.empty()) cb = UT_convertToInches(s2.c_str());
			const std::string & s3 = PP_getAttribute("page-margin-left", props);
			if (!s3.empty()) cl = UT_convertToInches(s3.c_str());
			const std::string & s4 = PP_getAttribute("page-margin-right", props);
			if (!s4.empty()) cr = UT_convertToInches(s4.c_str());
		}
	}

	struct _mp { const char * name; const char * data;
				 double t, b, l, r; };
	static const _mp presets[] = {
		{ "Normal",   "normal",   1.0, 1.0, 1.0,  1.0  },
		{ "Narrow",   "narrow",   0.5, 0.5, 0.5,  0.5  },
		{ "Moderate", "moderate", 1.0, 1.0, 0.75, 0.75 },
		{ "Wide",     "wide",     1.0, 1.0, 2.0,  2.0  },
		{ "Mirrored", "mirrored", 1.0, 1.0, 1.25, 1.0  },
	};

	auto detail = [&](double t, double b, double l, double r) {
		return g_strdup_printf("Top: %s   Bottom: %s\nLeft: %s   Right: %s",
							   _fmt_dim(t, u).c_str(), _fmt_dim(b, u).c_str(),
							   _fmt_dim(l, u).c_str(), _fmt_dim(r, u).c_str());
	};
	auto close = [](double a, double b) { return fabs(a - b) < 0.01; };

	/* "Last Custom Setting" heads the list when the current margins
	 * match none of the presets (Word behaviour) */
	bool matched = false;
	for (const _mp & p : presets)
		matched = matched || (close(ct, p.t) && close(cb, p.b) &&
							  close(cl, p.l) && close(cr, p.r));
	if (!matched && pView)
	{
		_PageSpec spec = { 0.12, 0.12, 0.15, 0.15, 1, false, false, 0, false };
		gchar * det = detail(ct, cb, cl, cr);
		GtkWidget * row = _presetRow(
			"<b>Last Custom Setting</b>  \xE2\x98\x85", det,
			_glyph_widget(spec, 26, 34), nullptr, nullptr);
		gtk_widget_set_sensitive(row, FALSE);
		g_free(det);
		gtk_box_append(GTK_BOX(box), row);
	}

	for (const _mp & p : presets)
	{
		_PageSpec spec = { p.t * 0.13, p.b * 0.13,
						   p.l * 0.18, p.r * 0.18, 1, false, false, 0, false };
		gchar * det = detail(p.t, p.b, p.l, p.r);
		std::string nm = p.name;
		bool cur = close(ct, p.t) && close(cb, p.b) &&
				   close(cl, p.l) && close(cr, p.r);
		if (cur)
			nm = std::string("\xE2\x9C\x93 <b>") + p.name + "</b>";
		GtkWidget * row = _presetRow(nm.c_str(), det,
									 _glyph_widget(spec, 26, 34),
									 "pageMargins", p.data);
		g_free(det);
		gtk_box_append(GTK_BOX(box), row);
	}

	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("<b>Custom Margins\xE2\x80\xA6</b>", nullptr,
							  nullptr, "docSettings", nullptr));
	return popover;
}

/* Orientation dropdown: Portrait / Landscape */
GtkWidget * AP_UnixRibbon::_makeOrientationPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	FV_View * pView = static_cast<FV_View *>(
		m_pFrame ? m_pFrame->getCurrentView() : nullptr);
	bool bPortrait = true;
	if (pView && pView->getLayout() && pView->getLayout()->getDocument())
		bPortrait = pView->getLayout()->getDocument()->getPageSize()->isPortrait();

	_PageSpec ps = { 0.12, 0.12, 0.15, 0.15, 1, false, false, 0, false };
	_PageSpec ls = ps;
	ls.landscape = true;

	GtkWidget * r1 = _presetRow(bPortrait ? "\xE2\x9C\x93 <b>Portrait</b>"
										  : "Portrait",
								nullptr, _glyph_widget(ps, 26, 34),
								"pageOrientation", "portrait");
	GtkWidget * r2 = _presetRow(!bPortrait ? "\xE2\x9C\x93 <b>Landscape</b>"
										   : "Landscape",
								nullptr, _glyph_widget(ls, 26, 34),
								"pageOrientation", "landscape");
	gtk_box_append(GTK_BOX(box), r1);
	gtk_box_append(GTK_BOX(box), r2);
	return popover;
}

/* Size dropdown: paper-size gallery, current first, + More Sizes… */
GtkWidget * AP_UnixRibbon::_makeSizePopover()
{
	GtkWidget * outer;
	GtkWidget * popover = _popover_new_box(&outer);

	FV_View * pView = static_cast<FV_View *>(
		m_pFrame ? m_pFrame->getCurrentView() : nullptr);
	const fp_PageSize * cur = (pView && pView->getLayout())
		? pView->getLayout()->getDocument()->getPageSize() : nullptr;
	std::string curName = cur ? cur->getPredefinedName() : "";
	bool curLandscape = cur ? !cur->isPortrait() : false;
	UT_Dimension u = _ruler_units();

	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();

	/* scrolled list - the full predefined set is long */
	GtkWidget * scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
								   GTK_POLICY_NEVER,
								   GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(
		GTK_SCROLLED_WINDOW(scroll), TRUE);
	gtk_scrolled_window_set_propagate_natural_width(
		GTK_SCROLLED_WINDOW(scroll), TRUE);
	gtk_scrolled_window_set_max_content_height(
		GTK_SCROLLED_WINDOW(scroll), 340);
	gtk_widget_set_vexpand(scroll, TRUE);
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
	gtk_box_append(GTK_BOX(outer), scroll);

	auto sizeRow = [&](fp_PageSize::Predefined pd, bool landscape) {
		const char * szName = fp_PageSize::PredefinedToName(pd);
		std::string disp = szName;
		int sid = fp_PageSize::PredefinedToLocalName(pd);
		std::string loc;
		if (sid && pSS->getValueUTF8((XAP_String_Id)sid, loc) && !loc.empty())
			disp = loc;
		if (landscape)
			disp += " (Long Edge)";
		fp_PageSize sz(pd);
		std::string det = _fmt_dim(
			sz.Width(DIM_IN), u) + " \xC3\x97 " + _fmt_dim(sz.Height(DIM_IN), u);
		std::string data = szName;
		if (landscape)
			data += "|landscape";
		bool isCur = (curName == szName) && (curLandscape == landscape);
		std::string nm = isCur
			? std::string("\xE2\x9C\x93 <b>") + disp + "</b>" : disp;
		_PageSpec spec = { 0.10, 0.10, 0.13, 0.13, 1, landscape,
						   false, 0, false };
		GtkWidget * row = _presetRow(nm.c_str(), det.c_str(),
									 _glyph_widget(spec, 22, 28),
									 "pageSize", data.c_str());
		gtk_box_append(GTK_BOX(box), row);
	};

	/* Word's order: the current size first, then the common set */
	static const fp_PageSize::Predefined common[] = {
		fp_PageSize::psA4, fp_PageSize::psLetter, fp_PageSize::psLegal,
		fp_PageSize::psExecutive, fp_PageSize::psA5, fp_PageSize::psB5,
		fp_PageSize::ps8_5x13, fp_PageSize::psFolio,
		fp_PageSize::psEnvelope_DL, fp_PageSize::psEnvelope_no10
	};

	/* current first */
	fp_PageSize::Predefined curPd = cur
		? fp_PageSize::NameToPredefined(curName.c_str())
		: fp_PageSize::psCustom;
	if (curPd != fp_PageSize::psCustom)
		sizeRow(curPd, curLandscape);

	bool seen[fp_PageSize::_last_predefined_pagesize_dont_use_] = {};
	if (curPd != fp_PageSize::psCustom)
		seen[curPd] = true;
	for (fp_PageSize::Predefined pd : common)
	{
		if (pd == curPd || seen[pd])
			continue;
		seen[pd] = true;
		sizeRow(pd, false);
	}
	/* long-edge variants of the common sizes, like the reference UI */
	sizeRow(fp_PageSize::psA5, true);
	sizeRow(fp_PageSize::psA4, true);
	sizeRow(fp_PageSize::psLetter, true);

	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	/* the rest */
	for (int i = fp_PageSize::_first_predefined_pagesize_;
		 i < fp_PageSize::_last_predefined_pagesize_dont_use_; ++i)
	{
		if (i == fp_PageSize::psCustom || seen[i])
			continue;
		seen[i] = true;
		sizeRow(static_cast<fp_PageSize::Predefined>(i), false);
	}

	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(outer),
				   _presetRow("<b>More Paper Sizes\xE2\x80\xA6</b>",
							  nullptr, nullptr, "docSettings", nullptr));
	return popover;
}

/* Columns dropdown: One/Two/Three + More Columns… */
GtkWidget * AP_UnixRibbon::_makeColumnsPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	FV_View * pView = static_cast<FV_View *>(
		m_pFrame ? m_pFrame->getCurrentView() : nullptr);
	int cur = 1;
	if (pView)
	{
		PP_PropertyVector props;
		if (pView->getSectionFormat(props))
		{
			const std::string & s = PP_getAttribute("columns", props);
			if (!s.empty())
				cur = atoi(s.c_str());
		}
	}

	static const char * names[] = { "One", "Two", "Three" };
	for (int i = 0; i < 3; ++i)
	{
		_PageSpec spec = { 0.12, 0.12, 0.15, 0.15, i + 1, false,
						   false, 0, false };
		std::string nm = names[i];
		if (cur == i + 1)
			nm = std::string("\xE2\x9C\x93 <b>") + nm + "</b>";
		char data[8];
		g_snprintf(data, sizeof(data), "%d", i + 1);
		GtkWidget * row = _presetRow(nm.c_str(), nullptr,
									 _glyph_widget(spec, 24, 32),
									 "pageColumns", data);
		gtk_box_append(GTK_BOX(box), row);
	}

	/* uneven columns are not supported by the layout engine */
	_PageSpec spec = { 0.12, 0.12, 0.15, 0.15, 2, false, false, 0, false };
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Left", nullptr,
							  _glyph_widget(spec, 24, 32), nullptr, nullptr,
							  false));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Right", nullptr,
							  _glyph_widget(spec, 24, 32), nullptr, nullptr,
							  false));

	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("<b>More Columns\xE2\x80\xA6</b>", nullptr,
							  nullptr, "dlgColumns", nullptr));
	return popover;
}

/* Breaks dropdown: page/column breaks + section breaks, each with a
 * description like the reference UI */
GtkWidget * AP_UnixRibbon::_makeBreaksPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	_PageSpec spec = { 0.12, 0.12, 0.15, 0.15, 1, false, false, 1, false };

	gtk_box_append(GTK_BOX(box), _popover_section_label("Page Breaks"));
	gtk_box_append(GTK_BOX(box),
		_presetRow("<b>Page</b>",
				   "Mark the point at which one page ends and the "
				   "next page begins.",
				   _glyph_widget(spec, 26, 34), "insertPageBreak", nullptr));
	gtk_box_append(GTK_BOX(box),
		_presetRow("<b>Column</b>",
				   "Indicate that the text following the column break "
				   "will begin in the next column.",
				   _glyph_widget(spec, 26, 34), "insColumnBreak", nullptr));
	gtk_box_append(GTK_BOX(box),
		_presetRow("<b>Text Wrapping</b>",
				   "Separate text around objects on web pages, "
				   "such as caption text.",
				   _glyph_widget(spec, 26, 34), nullptr, nullptr, false));

	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box), _popover_section_label("Section Breaks"));
	gtk_box_append(GTK_BOX(box),
		_presetRow("<b>Next Page</b>",
				   "Insert a section break and start the new section "
				   "on the next page.",
				   _glyph_widget(spec, 26, 34), "insSectionBreak", "next"));
	gtk_box_append(GTK_BOX(box),
		_presetRow("<b>Continuous</b>",
				   "Insert a section break and start the new section "
				   "on the same page.",
				   _glyph_widget(spec, 26, 34), "insSectionBreak",
				   "continuous"));
	gtk_box_append(GTK_BOX(box),
		_presetRow("<b>Even Page</b>",
				   "Insert a section break and start the new section "
				   "on the next even-numbered page.",
				   _glyph_widget(spec, 26, 34), "insSectionBreak", "even"));
	gtk_box_append(GTK_BOX(box),
		_presetRow("<b>Odd Page</b>",
				   "Insert a section break and start the new section "
				   "on the next odd-numbered page.",
				   _glyph_widget(spec, 26, 34), "insSectionBreak", "odd"));
	return popover;
}

/* Line Numbers dropdown - settings are stored on the section and
 * round-trip in the document; the layout engine does not render
 * numbers yet */
GtkWidget * AP_UnixRibbon::_makeLineNumbersPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	FV_View * pView = static_cast<FV_View *>(
		m_pFrame ? m_pFrame->getCurrentView() : nullptr);
	std::string cur = "none";
	if (pView)
	{
		PP_PropertyVector props;
		if (pView->getSectionFormat(props))
		{
			const std::string & s = PP_getAttribute("line-numbering", props);
			if (!s.empty())
				cur = s;
		}
	}

	static const struct { const char * name; const char * data; } modes[] = {
		{ "None",                          "line-numbering:none" },
		{ "Continuous",                    "line-numbering:continuous" },
		{ "Restart Each Page",             "line-numbering:page" },
		{ "Restart Each Section",          "line-numbering:section" },
	};
	for (const auto & m : modes)
	{
		std::string nm = (cur == m.data + 15)
			? std::string("\xE2\x9C\x93 ") + m.name : m.name;
		GtkWidget * row = _presetRow(nm.c_str(), nullptr, nullptr,
									 "sectProps", m.data);
		gtk_widget_set_tooltip_text(row,
			"Line numbering is stored but not rendered yet");
		gtk_box_append(GTK_BOX(box), row);
	}
	/* paragraph-level suppression */
	gtk_box_append(GTK_BOX(box),
		_presetRow("Suppress for Current Paragraph", nullptr, nullptr,
				   "paraProp", "suppress-line-numbers:1"));

	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget * opt = _presetRow("<b>Line Numbering Options\xE2\x80\xA6</b>",
								 nullptr, nullptr, nullptr, nullptr);
	g_signal_connect(opt, "clicked",
					 G_CALLBACK(_s_linedlg_clicked), this);
	gtk_box_append(GTK_BOX(box), opt);
	return popover;
}

/* Hyphenation dropdown - stored as a document attribute */
GtkWidget * AP_UnixRibbon::_makeHyphenationPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	static const struct { const char * name; const char * data; } modes[] = {
		{ "None",      "hyphenation:none" },
		{ "Automatic", "hyphenation:auto" },
		{ "Manual",    "hyphenation:manual" },
	};
	for (const auto & m : modes)
	{
		GtkWidget * row = _presetRow(m.name, nullptr, nullptr,
									 "docProps", m.data);
		gtk_widget_set_tooltip_text(row,
			"Hyphenation is stored but not rendered yet");
		gtk_box_append(GTK_BOX(box), row);
	}

	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget * opt = _presetRow("<b>Hyphenation Options\xE2\x80\xA6</b>",
								 nullptr, nullptr, nullptr, nullptr);
	g_signal_connect(opt, "clicked",
					 G_CALLBACK(_s_hyphdlg_clicked), this);
	gtk_box_append(GTK_BOX(box), opt);
	return popover;
}

/* Wrap Text popover: frame wrap modes the engine supports */
GtkWidget * AP_UnixRibbon::_makeWrapPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	static const struct { const char * name; const char * data; } modes[] = {
		{ "Square",            "wrapped-both" },
		{ "Top and Bottom",    "wrapped-topbot" },
		{ "Behind Text",       "below-text" },
		{ "In Front of Text",  "above-text" },
	};
	for (const auto & m : modes)
		gtk_box_append(GTK_BOX(box),
					   _presetRow(m.name, nullptr, nullptr,
								  "wrapObject", m.data));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("In Line with Text", nullptr, nullptr,
							  nullptr, nullptr, false));
	return popover;
}

/* Position popover: frame position-to choices + the full dialog */
GtkWidget * AP_UnixRibbon::_makePositionPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	static const struct { const char * name; const char * data; } modes[] = {
		{ "Position in Top Left with Square Text Wrapping",
		  "frame-page-xpos:0in;frame-page-ypos:0in;wrap-mode:wrapped-both;position-to:page-above-text" },
		{ "Position in Top Center with Square Text Wrapping",
		  "frame-page-xpos:50%;frame-page-ypos:0in;wrap-mode:wrapped-both;position-to:page-above-text;frame-horiz-align:center" },
		{ "Position in Top Right with Square Text Wrapping",
		  "frame-page-xpos:100%;frame-page-ypos:0in;wrap-mode:wrapped-both;position-to:page-above-text;frame-horiz-align:right" },
	};
	for (const auto & m : modes)
		gtk_box_append(GTK_BOX(box),
					   _presetRow(m.name, nullptr, nullptr,
								  "sectProps", m.data));
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("<b>More Layout Options\xE2\x80\xA6</b>",
							  nullptr, nullptr, "arrangePosition", nullptr));
	return popover;
}

/* Align popover: horizontal alignment of the selected frame/image */
GtkWidget * AP_UnixRibbon::_makeAlignObjPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	static const struct { const char * name; const char * data; } modes[] = {
		{ "Align Left",   "frame-horiz-align:left" },
		{ "Align Center", "frame-horiz-align:center" },
		{ "Align Right",  "frame-horiz-align:right" },
	};
	for (const auto & m : modes)
		gtk_box_append(GTK_BOX(box),
					   _presetRow(m.name, nullptr, nullptr,
								  "sectProps", m.data));
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("<b>More Layout Options\xE2\x80\xA6</b>",
							  nullptr, nullptr, "arrangePosition", nullptr));
	return popover;
}

/* Z-order popover: Word's Bring Forward / Send Backward menus.
 * bForward selects which of the two menus is built. */
GtkWidget * AP_UnixRibbon::_makeZOrderPopover(bool bForward)
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	if (bForward)
	{
		gtk_box_append(GTK_BOX(box),
					   _presetRow("Bring Forward", nullptr, nullptr,
								  "frameBringForward", nullptr));
		gtk_box_append(GTK_BOX(box),
					   _presetRow("Bring to Front", nullptr, nullptr,
								  "frameBringToFront", nullptr));
		gtk_box_append(GTK_BOX(box),
					   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
		gtk_box_append(GTK_BOX(box),
					   _presetRow("Bring in Front of Text", nullptr, nullptr,
								  "frameInFrontOfText", nullptr));
	}
	else
	{
		gtk_box_append(GTK_BOX(box),
					   _presetRow("Send Backward", nullptr, nullptr,
								  "frameSendBackward", nullptr));
		gtk_box_append(GTK_BOX(box),
					   _presetRow("Send to Back", nullptr, nullptr,
								  "frameSendToBack", nullptr));
		gtk_box_append(GTK_BOX(box),
					   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
		gtk_box_append(GTK_BOX(box),
					   _presetRow("Send Behind Text", nullptr, nullptr,
								  "frameBehindText", nullptr));
	}
	return popover;
}

/* Arrange popovers are built once at startup; row sensitivity is
 * re-evaluated every time the popover opens (Word greys Group until
 * two objects are ticked in the Selection pane, Rotate until an
 * object is selected) */
void AP_UnixRibbon::_s_arrange_popover_map(GtkWidget * popover,
										   gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	FV_View * pView = static_cast<FV_View *>(
		self->m_pFrame ? self->m_pFrame->getCurrentView() : nullptr);
	bool bObj = pView && pView->getFrameLayout();
	UT_sint32 nTicked = pView ? pView->groupSelCount() : 0;
	bool bUngroup = nTicked > 0;
	if (!bUngroup && bObj)
	{
		const PP_AttrProp * pAP = nullptr;
		pView->getFrameLayout()->getAP(pAP);
		const gchar * sz = nullptr;
		bUngroup = pAP && pAP->getProperty("frame-group", sz)
			&& sz && *sz;
	}

	GtkWidget * box = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(popover), "abi-box"));
	UT_return_if_fail(box);
	for (GtkWidget * c = gtk_widget_get_first_child(box); c;
		 c = gtk_widget_get_next_sibling(c))
	{
		if (g_object_get_data(G_OBJECT(c), "abi-spin-row"))
		{
			for (GtkWidget * k = gtk_widget_get_first_child(c); k;
				 k = gtk_widget_get_next_sibling(k))
				if (GTK_IS_SPIN_BUTTON(k) || GTK_IS_BUTTON(k))
					gtk_widget_set_sensitive(k, bObj);
			continue;
		}
		const char * szMethod = static_cast<const char *>(
			g_object_get_data(G_OBJECT(c), "abi-em-method"));
		if (!szMethod)
			continue;
		bool bOn;
		if (!strcmp(szMethod, "frameGroup"))
			bOn = nTicked >= 2;
		else if (!strcmp(szMethod, "frameUngroup"))
			bOn = bUngroup;
		else
			bOn = bObj;
		gtk_widget_set_sensitive(c, bOn);
	}
}

/* "Set" in the Rotate popover's exact-angle row */
void AP_UnixRibbon::_s_rotate_to_clicked(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	GtkWidget * spin = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(w), "abi-spin"));
	UT_return_if_fail(self && spin);
	double deg = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
	char buf[32];
	snprintf(buf, sizeof(buf), "%.6g", deg);
	_tb_popdown_popover(w);
	self->_invokeEditMethod("frameRotateTo", buf);
}

/* Word's Rotate menu: fixed 90° turns, flips and an exact angle */
GtkWidget * AP_UnixRibbon::_makeRotatePopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);
	g_object_set_data(G_OBJECT(popover), "abi-box", box);
	g_signal_connect(popover, "map",
					 G_CALLBACK(_s_arrange_popover_map), this);

	gtk_box_append(GTK_BOX(box),
				   _presetRow("Rotate Right 90\xC2\xB0", nullptr, nullptr,
							  "frameRotateRight", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Rotate Left 90\xC2\xB0", nullptr, nullptr,
							  "frameRotateLeft", nullptr));
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Flip Vertical", nullptr, nullptr,
							  "frameFlipVert", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Flip Horizontal", nullptr, nullptr,
							  "frameFlipHoriz", nullptr));
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	/* exact-angle row: spin + Set -> frameRotateTo */
	GtkWidget * row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	g_object_set_data(G_OBJECT(row), "abi-spin-row",
					  GINT_TO_POINTER(1));
	GtkWidget * lbl = gtk_label_new("Angle:");
	gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(row), lbl);
	GtkWidget * spin = gtk_spin_button_new_with_range(0.0, 359.9, 5.0);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
	gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_widget_set_hexpand(spin, TRUE);
	gtk_box_append(GTK_BOX(row), spin);
	GtkWidget * set = gtk_button_new_with_label("Set");
	g_object_set_data(G_OBJECT(set), "abi-spin", spin);
	g_signal_connect(set, "clicked",
					 G_CALLBACK(_s_rotate_to_clicked), this);
	gtk_box_append(GTK_BOX(row), set);
	gtk_box_append(GTK_BOX(box), row);
	return popover;
}

/* Word's Group menu: grouping runs on the Selection pane's ticked
 * objects since the canvas only selects one frame at a time */
GtkWidget * AP_UnixRibbon::_makeGroupPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);
	g_object_set_data(G_OBJECT(popover), "abi-box", box);
	g_signal_connect(popover, "map",
					 G_CALLBACK(_s_arrange_popover_map), this);

	gtk_box_append(GTK_BOX(box),
				   _presetRow("Group", "Combine the ticked objects",
							  nullptr, "frameGroup", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Ungroup", "Split the group into objects",
							  nullptr, "frameUngroup", nullptr));
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget * hint = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(hint),
		"<span size='small' alpha='65%'>Tick objects in the Selection "
		"Pane to choose what gets grouped</span>");
	gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
	gtk_label_set_max_width_chars(GTK_LABEL(hint), 30);
	gtk_widget_set_halign(hint, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(box), hint);
	return popover;
}

/* -------- References tab popovers -------- */

/* mini TOC preview glyph for the gallery: three entry lines with dot
 * leaders, drawn in the preset's look */
/* draw a Word-style TOC preview card: a white card with a "Table of
 * Contents" heading and four sample entries rendered with the
 * preset's level styles (weight, slant, case), tab leaders and page
 * numbers. szPreset is a s_TOCPresets id or "manual". */
static void _toc_card_draw(GtkDrawingArea *, cairo_t * cr,
						   int w, int h, gpointer data)
{
	const char * szPreset = static_cast<const char *>(data);
	/* manual cards carry "manual-<preset>" so they get the preset's
	 * look with the placeholder text Word shows for Manual Table */
	bool bManual   = !strncmp(szPreset, "manual-", 7);
	if (bManual)
		szPreset += 7;
	bool bCaps     = !strcmp(szPreset, "contemporary");
	bool bLine     = !strcmp(szPreset, "contemporary");
	/* spec: modern has no leaders, every other type uses dots */
	bool bDots     = !bLine && strcmp(szPreset, "modern");
	/* spec: level-1 entries bold for classic/formal/modern/
	 * contemporary; formal also bolds level 2 */
	bool bBold1    = !strcmp(szPreset, "classic") ||
					 !strcmp(szPreset, "contemporary") ||
					 !strcmp(szPreset, "formal") ||
					 !strcmp(szPreset, "modern");
	bool bBold2    = !strcmp(szPreset, "formal");
	bool bCenter   = !strcmp(szPreset, "classic") ||
					 !strcmp(szPreset, "formal");

	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_rectangle(cr, 0.5, 0.5, w - 1, h - 1);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.78, 0.78, 0.78);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	/* Word's Automatic Table 1 titles the TOC "Contents"; every other
	 * automatic type and all manual tables use "Table of Contents" */
	const char * szHeading =
		(!bManual && !strcmp(szPreset, "classic"))
			? "Contents" : "Table of Contents";

	cairo_set_source_rgb(cr, 0.27, 0.45, 0.77);
	cairo_select_font_face(cr, "sans",
						   CAIRO_FONT_SLANT_NORMAL,
						   CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 11);
	if (bCenter)
	{
		cairo_text_extents_t hext;
		cairo_text_extents(cr, szHeading, &hext);
		cairo_move_to(cr, (w - hext.width) / 2, 18);
	}
	else
		cairo_move_to(cr, 10, 18);
	cairo_show_text(cr, szHeading);

	double y = 40;
	for (int lvl = 1; lvl <= 4; lvl++)
	{
		double x = 10 + (lvl - 1) * 13;
		cairo_font_weight_t weight =
			((lvl == 1 && bBold1) || (lvl == 2 && bBold2))
				? CAIRO_FONT_WEIGHT_BOLD
				: CAIRO_FONT_WEIGHT_NORMAL;
		cairo_set_source_rgb(cr, 0.30, 0.30, 0.30);
		cairo_select_font_face(cr, "sans",
							   CAIRO_FONT_SLANT_NORMAL, weight);
		cairo_set_font_size(cr, 8);
		char buf[64];
		snprintf(buf, sizeof(buf), "Type chapter %s (level %d)",
				 bManual ? "title" : "level", lvl);
		if (bCaps)
		{
			for (char * p = buf; *p; p++)
				*p = g_ascii_toupper(*p);
		}
		cairo_move_to(cr, x, y);
		cairo_show_text(cr, buf);

		cairo_text_extents_t ext;
		cairo_text_extents(cr, buf, &ext);
		double lx0 = x + ext.width + 5;
		double lx1 = w - 22;
		cairo_set_source_rgb(cr, 0.45, 0.45, 0.45);
		if (bLine)
		{
			cairo_set_line_width(cr, 0.7);
			cairo_move_to(cr, lx0, y - 2.2);
			cairo_line_to(cr, lx1, y - 2.2);
			cairo_stroke(cr);
		}
		else if (bDots)
		{
			for (double dx = lx0; dx < lx1; dx += 3.4)
			{
				cairo_arc(cr, dx, y - 2.4, 0.65, 0, 2 * G_PI);
				cairo_fill(cr);
			}
		}
		cairo_set_source_rgb(cr, 0.30, 0.30, 0.30);
		cairo_move_to(cr, lx1 + 4, y);
		char num[8];
		snprintf(num, sizeof(num), "%d", lvl);
		cairo_show_text(cr, num);
		y += 18;
	}
}

/* refresh the TOC gallery rows that depend on the cursor position
 * ("Remove Table of Contents" is only live inside a TOC) */
void AP_UnixRibbon::_s_toc_gallery_map(GtkWidget * popover,
									   gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	GtkWidget * btn = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(popover), "abi-toc-remove"));
	if (!btn)
		return;
	FV_View * pView = static_cast<FV_View *>(
		self->m_pFrame ? self->m_pFrame->getCurrentView() : nullptr);
	gtk_widget_set_sensitive(btn, pView && pView->hasTOC());
}

/* Word's Table of Contents dropdown: a scrolling column of preview
 * cards — the manual table first, then the built-in presets —
 * followed by Custom Table of Contents… and Remove Table of Contents */
GtkWidget * AP_UnixRibbon::_makeTOCGalleryPopover()
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);
	gtk_widget_set_margin_start(box, 4);
	gtk_widget_set_margin_end(box, 4);

	GtkWidget * sw = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(
		GTK_SCROLLED_WINDOW(sw), TRUE);
	gtk_scrolled_window_set_max_content_height(
		GTK_SCROLLED_WINDOW(sw), 430);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), box);
	gtk_popover_set_child(GTK_POPOVER(popover), sw);

	auto cardBtn = [this](const char * szName,
						  const char * szPreset) -> GtkWidget *
	{
		GtkWidget * v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
		GtkWidget * l = gtk_label_new(nullptr);
		char * mk = g_markup_printf_escaped(
			"<span alpha='70%%'>%s</span>", szName);
		gtk_label_set_markup(GTK_LABEL(l), mk);
		g_free(mk);
		gtk_box_append(GTK_BOX(v), l);

		GtkWidget * da = gtk_drawing_area_new();
		gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(da), 220);
		gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(da), 104);
		gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(da),
									   _toc_card_draw,
									   g_strdup(szPreset), g_free);
		gtk_widget_set_margin_start(da, 6);
		gtk_widget_set_margin_end(da, 6);
		gtk_box_append(GTK_BOX(v), da);

		GtkWidget * btn = gtk_button_new();
		gtk_button_set_child(GTK_BUTTON(btn), v);
		gtk_button_set_has_frame(GTK_BUTTON(btn), FALSE);
		g_object_set_data_full(G_OBJECT(btn), "abi-em-method",
							   g_strdup("tocInsert"), g_free);
		g_object_set_data_full(G_OBJECT(btn), "abi-em-data",
							   g_strdup(szPreset), g_free);
		g_signal_connect(btn, "clicked",
						 G_CALLBACK(_s_popover_em_clicked), this);
		return btn;
	};

	auto sectionLabel = [](const char * szText) -> GtkWidget *
	{
		GtkWidget * l = gtk_label_new(nullptr);
		char * mk = g_markup_printf_escaped(
			"<span size='small' weight='bold' alpha='60%%'>%s</span>",
			szText);
		gtk_label_set_markup(GTK_LABEL(l), mk);
		g_free(mk);
		gtk_widget_set_halign(l, GTK_ALIGN_START);
		gtk_widget_set_margin_start(l, 6);
		return l;
	};

	/* Word's gallery: automatic (field-built) tables under "Automatic",
	 * the hand-edited placeholder tables under "Manual" — both offer
	 * the five built-in types */
	static const struct { const char * szName; const char * szId; }
	s_tocTypes[] =
	{
		{ "Classic",		"classic" },
		{ "Contemporary",	"contemporary" },
		{ "Modern",			"modern" },
		{ "Formal",			"formal" },
		{ "Simple",			"simple" },
	};

	gtk_box_append(GTK_BOX(box), sectionLabel("Automatic"));
	for (const auto & t : s_tocTypes)
		gtk_box_append(GTK_BOX(box), cardBtn(t.szName, t.szId));

	gtk_box_append(GTK_BOX(box), sectionLabel("Manual"));
	for (const auto & t : s_tocTypes)
	{
		char szManual[32];
		g_snprintf(szManual, sizeof(szManual), "manual-%s", t.szId);
		gtk_box_append(GTK_BOX(box), cardBtn(t.szName, szManual));
	}
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Custom Table of Contents…",
							  "Set levels, styles, leaders and page numbers",
							  nullptr, "formatTOC", nullptr));
	GtkWidget * remove = _presetRow("Remove Table of Contents",
								  "Delete the table of contents",
								  nullptr, "tocRemove", nullptr);
	gtk_box_append(GTK_BOX(box), remove);
	g_object_set_data(G_OBJECT(popover), "abi-toc-remove", remove);
	g_signal_connect(popover, "map",
					 G_CALLBACK(_s_toc_gallery_map), this);
	return popover;
}

/* Insert tab Cover Page gallery: mini previews of the generated
 * presets, sketching the same bands/rules the real presets use */
static void _cover_card_draw(GtkDrawingArea *, cairo_t * cr,
							 int w, int h, gpointer data)
{
	const char * szPreset = static_cast<const char *>(data);
	/* draw in a fixed 132x187 A4 space, scaled to the widget */
	cairo_scale(cr, w / 132.0, h / 187.0);
	double pw = 131, ph = 186;

	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_rectangle(cr, 0.5, 0.5, pw, ph);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.78, 0.78, 0.78);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	auto band = [&](double y, double hh, double r, double g, double b)
	{
		cairo_set_source_rgb(cr, r, g, b);
		cairo_rectangle(cr, 12, y, pw - 24, hh);
		cairo_fill(cr);
	};
	auto tbar = [&](double x, double y, double ww, double hh,
					double r, double g, double b)
	{
		cairo_set_source_rgb(cr, r, g, b);
		cairo_rectangle(cr, x, y, ww, hh);
		cairo_fill(cr);
	};
	double cx = pw / 2;

	if (!strcmp(szPreset, "austin"))
	{
		band(18, 26, 0.12, 0.22, 0.39);               /* navy band */
		tbar(cx - 32, 88, 64, 10, 0.12, 0.22, 0.39);  /* title */
		tbar(cx - 22, 104, 44, 5, 0.60, 0.60, 0.60);  /* subtitle */
		tbar(cx - 36, 126, 72, 1.6, 0.18, 0.45, 0.71);/* rule */
		tbar(cx - 17, 138, 34, 5, 0.35, 0.35, 0.35);  /* author */
		tbar(cx - 13, 148, 26, 4, 0.65, 0.65, 0.65);  /* date */
	}
	else if (!strcmp(szPreset, "banded"))
	{
		band(10, 30, 0.18, 0.45, 0.71);               /* top band */
		tbar(24, 74, 70, 10, 0.15, 0.15, 0.15);       /* title */
		tbar(24, 90, 50, 5, 0.60, 0.60, 0.60);        /* subtitle */
		tbar(24, 138, 34, 5, 0.35, 0.35, 0.35);       /* author */
		tbar(24, 148, 26, 4, 0.65, 0.65, 0.65);       /* date */
		band(ph - 22, 14, 0.18, 0.45, 0.71);          /* bottom band */
	}
	else if (!strcmp(szPreset, "facet"))
	{
		band(14, 5, 0.75, 0.0, 0.0);                  /* thin top bar */
		tbar(22, 72, 6, 30, 0.75, 0.0, 0.0);          /* red sidebar */
		tbar(34, 74, 66, 10, 0.15, 0.15, 0.15);       /* title */
		tbar(34, 90, 48, 5, 0.60, 0.60, 0.60);        /* subtitle */
		tbar(26, 140, 34, 5, 0.35, 0.35, 0.35);       /* author */
		tbar(26, 150, 26, 4, 0.65, 0.65, 0.65);       /* date */
	}
	else if (!strcmp(szPreset, "filigree"))
	{
		tbar(cx - 42, 56, 84, 1.2, 0.50, 0.39, 0.64); /* top rule */
		tbar(cx - 32, 76, 64, 10, 0.25, 0.19, 0.31);  /* title */
		tbar(cx - 42, 98, 84, 1.2, 0.50, 0.39, 0.64); /* bottom rule */
		tbar(cx - 22, 110, 44, 5, 0.60, 0.60, 0.60);  /* subtitle */
		tbar(cx - 17, 148, 34, 5, 0.35, 0.35, 0.35);  /* author */
		tbar(cx - 13, 158, 26, 4, 0.65, 0.65, 0.65);  /* date */
	}
	else if (!strcmp(szPreset, "integral"))
	{
		band(52, 62, 0.06, 0.42, 0.42);               /* teal block */
		tbar(cx - 32, 70, 64, 10, 1.0, 1.0, 1.0);     /* title */
		tbar(cx - 22, 88, 44, 5, 0.85, 0.95, 0.95);   /* subtitle */
		tbar(cx - 17, 146, 34, 5, 0.06, 0.42, 0.42);  /* author */
		tbar(cx - 13, 156, 26, 4, 0.65, 0.65, 0.65);  /* date */
	}
	else if (!strcmp(szPreset, "crop"))
	{
		/* L brackets: top-left and bottom-right */
		cairo_set_source_rgb(cr, 0.27, 0.45, 0.77);
		cairo_set_line_width(cr, 2.0);
		cairo_move_to(cr, 16, 34); cairo_line_to(cr, 16, 16);
		cairo_line_to(cr, 44, 16); cairo_stroke(cr);
		cairo_move_to(cr, pw - 16, ph - 34);
		cairo_line_to(cr, pw - 16, ph - 16);
		cairo_line_to(cr, pw - 44, ph - 16); cairo_stroke(cr);
		tbar(34, 84, 68, 10, 0.15, 0.27, 0.47);       /* title */
		tbar(34, 100, 46, 5, 0.60, 0.60, 0.60);       /* subtitle */
		tbar(pw - 66, 138, 32, 5, 0.15, 0.27, 0.47);  /* author */
		tbar(pw - 58, 148, 24, 4, 0.65, 0.65, 0.65);  /* date */
	}
	else if (!strcmp(szPreset, "sideline"))
	{
		tbar(10, 8, 12, ph - 16, 0.27, 0.45, 0.77);   /* left stripe */
		tbar(32, 66, 66, 10, 0.15, 0.27, 0.47);       /* title */
		tbar(32, 82, 46, 5, 0.60, 0.60, 0.60);        /* subtitle */
		tbar(32, 140, 32, 5, 0.15, 0.27, 0.47);       /* author */
		tbar(32, 150, 24, 4, 0.65, 0.65, 0.65);       /* date */
	}
	else if (!strcmp(szPreset, "retrospect"))
	{
		cairo_set_source_rgb(cr, 0.18, 0.45, 0.71);
		cairo_set_line_width(cr, 1.6);
		cairo_rectangle(cr, 26, 68, pw - 52, 34);     /* title box */
		cairo_stroke(cr);
		tbar(cx - 30, 80, 60, 9, 0.12, 0.31, 0.47);   /* title */
		tbar(cx - 22, 110, 44, 5, 0.60, 0.60, 0.60);  /* subtitle */
		tbar(cx - 17, 150, 34, 5, 0.35, 0.35, 0.35);  /* author */
		tbar(cx - 13, 160, 26, 4, 0.65, 0.65, 0.65);  /* date */
	}
	else if (!strcmp(szPreset, "yearly"))
	{
		tbar(cx - 40, 30, 80, 22, 0.85, 0.89, 0.95);  /* pale year */
		tbar(cx - 32, 66, 64, 9, 0.12, 0.31, 0.47);   /* title */
		tbar(cx - 22, 80, 44, 5, 0.60, 0.60, 0.60);   /* subtitle */
		tbar(cx - 17, 138, 34, 5, 0.35, 0.35, 0.35);  /* author */
		band(ph - 20, 12, 0.18, 0.45, 0.71);          /* bottom band */
	}
	else if (!strcmp(szPreset, "motion"))
	{
		cairo_set_source_rgb(cr, 0.18, 0.45, 0.71);
		cairo_rectangle(cr, pw / 2, 24, pw / 2 - 14, 10);
		cairo_fill(cr);                               /* right band */
		cairo_set_source_rgb(cr, 0.71, 0.78, 0.91);
		cairo_rectangle(cr, 14, 38, pw / 2 - 14, 10);
		cairo_fill(cr);                               /* left band */
		tbar(20, 84, 70, 9, 0.15, 0.15, 0.15);        /* title */
		tbar(20, 98, 50, 5, 0.60, 0.60, 0.60);        /* subtitle */
		tbar(20, 148, 32, 5, 0.35, 0.35, 0.35);       /* author */
		tbar(20, 158, 24, 4, 0.65, 0.65, 0.65);       /* date */
	}
	else if (!strcmp(szPreset, "frame"))
	{
		cairo_set_source_rgb(cr, 0.85, 0.89, 0.95);
		cairo_rectangle(cr, 22, 22, pw - 44, 56);     /* picture fill */
		cairo_fill(cr);
		cairo_set_source_rgb(cr, 0.18, 0.45, 0.71);
		cairo_set_line_width(cr, 1.4);
		cairo_rectangle(cr, 22, 22, pw - 44, 56);
		cairo_stroke(cr);
		tbar(cx - 32, 92, 64, 9, 0.12, 0.31, 0.47);   /* title */
		tbar(cx - 22, 106, 44, 5, 0.60, 0.60, 0.60);  /* subtitle */
		tbar(cx - 17, 150, 34, 5, 0.35, 0.35, 0.35);  /* author */
		tbar(cx - 13, 160, 26, 4, 0.65, 0.65, 0.65);  /* date */
	}
	else /* whisp */
	{
		tbar(16, 16, 64, 2.4, 0.75, 0.56, 0.0);       /* amber rule */
		tbar(20, 80, 74, 10, 0.25, 0.25, 0.25);       /* title */
		tbar(20, 96, 52, 5, 0.55, 0.55, 0.55);        /* subtitle */
		tbar(20, 152, 34, 5, 0.35, 0.35, 0.35);       /* author */
		tbar(20, 162, 26, 4, 0.65, 0.65, 0.65);       /* date */
	}
}

/* "Remove Current Cover" is only live while a generated cover exists */
void AP_UnixRibbon::_s_cover_gallery_map(GtkWidget * popover,
										 gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	GtkWidget * btn = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(popover), "abi-cover-remove"));
	if (!btn)
		return;
	FV_View * pView = static_cast<FV_View *>(
		self->m_pFrame ? self->m_pFrame->getCurrentView() : nullptr);
	gtk_widget_set_sensitive(btn, pView && pView->hasCoverPage());
}

/* Word's Cover Page dropdown: a scrolling column of preview cards for
 * the code-generated designs, then "Remove Current Cover" */
GtkWidget * AP_UnixRibbon::_makeCoverPagePopover()
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);
	gtk_widget_set_margin_start(box, 4);
	gtk_widget_set_margin_end(box, 4);

	/* Word's gallery: a scrolling 3-column grid of A4 portrait
	 * thumbnails with the design name under each card */
	GtkWidget * grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_grid_set_row_homogeneous(GTK_GRID(grid), FALSE);
	gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);

	GtkWidget * sw = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(
		GTK_SCROLLED_WINDOW(sw), TRUE);
	gtk_scrolled_window_set_max_content_height(
		GTK_SCROLLED_WINDOW(sw), 480);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), grid);
	gtk_box_append(GTK_BOX(box), sw);

	auto cardBtn = [this](const char * szName,
						  const char * szPreset) -> GtkWidget *
	{
		GtkWidget * v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);

		/* A4 portrait thumbnail, like Word's cover gallery */
		GtkWidget * da = gtk_drawing_area_new();
		gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(da), 95);
		gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(da), 134);
		gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(da),
									   _cover_card_draw,
									   g_strdup(szPreset), g_free);
		gtk_widget_set_halign(da, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(v), da);

		GtkWidget * l = gtk_label_new(nullptr);
		char * mk = g_markup_printf_escaped(
			"<span alpha='70%%'>%s</span>", szName);
		gtk_label_set_markup(GTK_LABEL(l), mk);
		g_free(mk);
		gtk_box_append(GTK_BOX(v), l);

		GtkWidget * btn = gtk_button_new();
		gtk_button_set_child(GTK_BUTTON(btn), v);
		gtk_button_set_has_frame(GTK_BUTTON(btn), FALSE);
		g_object_set_data_full(G_OBJECT(btn), "abi-em-method",
							   g_strdup("coverPageInsert"), g_free);
		g_object_set_data_full(G_OBJECT(btn), "abi-em-data",
							   g_strdup(szPreset), g_free);
		g_signal_connect(btn, "clicked",
						 G_CALLBACK(_s_popover_em_clicked), this);
		return btn;
	};

	static const struct { const char * szName; const char * szId; }
	s_coverTypes[] =
	{
		{ "Austin",		"austin" },
		{ "Banded",		"banded" },
		{ "Crop",		"crop" },
		{ "Facet",		"facet" },
		{ "Filigree",	"filigree" },
		{ "Frame",		"frame" },
		{ "Integral",	"integral" },
		{ "Motion",		"motion" },
		{ "Retrospect",	"retrospect" },
		{ "Sideline",	"sideline" },
		{ "Whisp",		"whisp" },
		{ "Yearly",		"yearly" },
	};

	for (unsigned i = 0; i < G_N_ELEMENTS(s_coverTypes); i++)
		gtk_grid_attach(GTK_GRID(grid),
						cardBtn(s_coverTypes[i].szName,
								s_coverTypes[i].szId),
						i % 3, i / 3, 1, 1);
	gtk_popover_set_child(GTK_POPOVER(popover), box);

	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget * remove = _presetRow("Remove Current Cover",
								  "Delete the generated cover page",
								  nullptr, "coverPageRemove", nullptr);
	gtk_box_append(GTK_BOX(box), remove);
	g_object_set_data(G_OBJECT(popover), "abi-cover-remove", remove);
	g_signal_connect(popover, "map",
					 G_CALLBACK(_s_cover_gallery_map), this);
	return popover;
}

/* Online Pictures dialog: download the image URL to a temp file
 * and insert it like a device picture */
void AP_UnixRibbon::_s_online_pic_clicked(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	_tb_popdown_popover(w);
	self->_showOnlinePictureDialog();
}

GtkWidget * AP_UnixRibbon::_makePicturesPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _presetRow("This Device…",
							  "Insert a picture from your computer",
							  _layout_icon((XAP_Menu_Id)AP_MENU_ID_INSERT_PICTURES, 16, 16),
							  "fileInsertGraphic", nullptr));
	GtkWidget * online = _presetRow("Online Pictures…",
								  "Insert a picture from a web address",
								  nullptr, nullptr, nullptr);
	g_signal_connect(online, "clicked",
					 G_CALLBACK(_s_online_pic_clicked), this);
	gtk_box_append(GTK_BOX(box), online);
	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* Online Pictures: a small URL dialog that downloads via GIO
 * (gvfs http backend) into a temp file and inserts it */
struct _OnlinePicCtx
{
	AP_UnixRibbon * self;
	GtkWidget * entry;
	GtkWidget * win;
};

void AP_UnixRibbon::_s_online_pic_insert(GtkWidget * /*w*/,
										 gpointer data)
{
	_OnlinePicCtx * ctx = static_cast<_OnlinePicCtx *>(data);
	const char * url = gtk_editable_get_text(GTK_EDITABLE(ctx->entry));
	XAP_Frame * pFrame = ctx->self->m_pFrame;
	FV_View * pView = pFrame
		? static_cast<FV_View *>(pFrame->getCurrentView()) : nullptr;

	GFile * gf = url ? g_file_new_for_uri(url) : nullptr;
	gchar * contents = nullptr;
	gsize len = 0;
	GError * err = nullptr;
	bool bOK = gf && pView &&
		g_file_load_contents(gf, nullptr, &contents, &len, nullptr,
							 &err);
	if (gf)
		g_object_unref(gf);

	if (bOK)
	{
		gchar * tmp = g_build_filename(g_get_tmp_dir(),
									   "abiword-online-pic", nullptr);
		bOK = g_file_set_contents(tmp, contents, len, nullptr);
		if (bOK)
		{
			FG_ConstGraphicPtr pFG;
			if (IE_ImpGraphic::loadGraphic(tmp, IEGFT_Unknown, pFG)
				== UT_OK && pFG)
				pView->cmdInsertGraphic(pFG);
			else
				bOK = false;
		}
		remove(tmp);
		g_free(tmp);
	}
	g_free(contents);

	if (!bOK && pFrame)
	{
		std::string msg = "The picture could not be downloaded";
		if (err && err->message)
			msg += std::string(": ") + err->message;
		pFrame->showMessageBox(msg, XAP_Dialog_MessageBox::b_O,
							   XAP_Dialog_MessageBox::a_OK);
	}
	if (err)
		g_error_free(err);
	gtk_window_destroy(GTK_WINDOW(ctx->win));
	delete ctx;
}

void AP_UnixRibbon::_showOnlinePictureDialog()
{
	GtkWidget * win = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(win), "Online Pictures");
	gtk_window_set_modal(GTK_WINDOW(win), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
	if (m_pFrame && m_pFrame->getFrameImpl())
		gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(
			static_cast<XAP_UnixFrameImpl *>(
				m_pFrame->getFrameImpl())->getTopLevelWindow()));

	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_top(box, 12);
	gtk_widget_set_margin_bottom(box, 12);
	gtk_widget_set_margin_start(box, 12);
	gtk_widget_set_margin_end(box, 12);
	gtk_window_set_child(GTK_WINDOW(win), box);

	GtkWidget * lbl = gtk_label_new(
		"Enter the address of the picture:");
	gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
	gtk_box_append(GTK_BOX(box), lbl);

	GtkWidget * entry = gtk_entry_new();
	gtk_entry_set_placeholder_text(GTK_ENTRY(entry),
								   "https://example.com/image.png");
	gtk_widget_set_size_request(entry, 320, -1);
	gtk_box_append(GTK_BOX(box), entry);

	_OnlinePicCtx * ctx = new _OnlinePicCtx { this, entry, win };
	GtkWidget * row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_halign(row, GTK_ALIGN_END);
	GtkWidget * cancel = gtk_button_new_with_label("Cancel");
	g_signal_connect_swapped(cancel, "clicked",
							 G_CALLBACK(gtk_window_destroy), win);
	gtk_box_append(GTK_BOX(row), cancel);
	GtkWidget * ok = gtk_button_new_with_label("Insert");
	gtk_widget_add_css_class(ok, "suggested-action");
	g_signal_connect(ok, "clicked",
					 G_CALLBACK(_s_online_pic_insert), ctx);
	gtk_box_append(GTK_BOX(row), ok);
	gtk_box_append(GTK_BOX(box), row);
	g_signal_connect(entry, "activate",
					 G_CALLBACK(_s_online_pic_insert), ctx);

	gtk_window_present(GTK_WINDOW(win));
	gtk_widget_grab_focus(entry);
}

/* Word's Shapes dropdown: the bundled LibreOffice preset-shape
 * gallery (artwork/shapes), grouped by shape category */
static void _s_shape_name_tooltip(std::string & tip)
{
	/* "basicshapes.round-quadrat" -> "Round Quadrat" */
	static const char * prefixes[] = {
		"basicshapes.", "arrowshapes.", "symbolshapes.",
		"starshapes.", "calloutshapes.", "flowchartshapes.flowchart-",
		"flowchartshapes.", nullptr
	};
	for (int i = 0; prefixes[i]; i++)
	{
		std::string::size_type p = tip.find(prefixes[i]);
		if (p != std::string::npos)
		{
			tip.erase(0, p + strlen(prefixes[i]));
			break;
		}
	}
	for (auto & c : tip)
		if (c == '-' || c == '.')
			c = ' ';
	bool bCap = true;
	for (auto & c : tip)
	{
		if (bCap && g_ascii_isalpha(c))
		{
			c = g_ascii_toupper(c);
			bCap = false;
		}
		else if (c == ' ')
			bCap = true;
	}
}

void AP_UnixRibbon::_addGalleryDir(GtkWidget * parent,
								   const char * szMethod,
								   const char * szPrefix,
								   const char * szDataPrefix,
								   const char * szTitle, int iconSize,
								   bool bRowLabel)
{
	std::string dirPath = XAP_App::getApp()->getAbiSuiteLibDir();
	dirPath += "/";
	dirPath += szPrefix;

	GDir * dir = g_dir_open(dirPath.c_str(), 0, nullptr);
	if (!dir)
		return;
	std::vector<std::string> files;
	for (const gchar * n = g_dir_read_name(dir); n;
		 n = g_dir_read_name(dir))
		files.push_back(n);
	g_dir_close(dir);
	std::sort(files.begin(), files.end());
	if (files.empty())
		return;

	if (szTitle)
		gtk_box_append(GTK_BOX(parent),
					   _popover_section_label(szTitle));

	GtkWidget * flow = gtk_flow_box_new();
	gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow),
									GTK_SELECTION_NONE);
	gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow),
										   bRowLabel ? 4 : 8);
	gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow),
										   bRowLabel ? 4 : 8);
	gtk_widget_set_margin_start(flow, 6);
	gtk_widget_set_margin_end(flow, 6);
	gtk_box_append(GTK_BOX(parent), flow);

	for (const auto & file : files)
	{
		bool bSvg = g_str_has_suffix(file.c_str(), ".svg");
		bool bPng = g_str_has_suffix(file.c_str(), ".png");
		if (!bSvg && !bPng)
			continue;
		std::string stem = file.substr(0, file.size() - 4);

		GtkWidget * btn = gtk_button_new();
		GtkWidget * v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		GtkWidget * pic = gtk_picture_new_for_filename(
			(dirPath + "/" + file).c_str());
		gtk_widget_set_size_request(pic, iconSize, iconSize);
		gtk_widget_set_halign(pic, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(v), pic);
		if (bRowLabel)
		{
			std::string tip = stem;
			_s_shape_name_tooltip(tip);
			GtkWidget * l = gtk_label_new(nullptr);
			char * mk = g_markup_printf_escaped(
				"<span size='x-small' alpha='70%%'>%s</span>",
				tip.c_str());
			gtk_label_set_markup(GTK_LABEL(l), mk);
			g_free(mk);
			gtk_label_set_max_width_chars(GTK_LABEL(l), 12);
			gtk_label_set_ellipsize(GTK_LABEL(l),
									PANGO_ELLIPSIZE_END);
			gtk_box_append(GTK_BOX(v), l);
			gtk_widget_set_tooltip_text(btn, tip.c_str());
		}
		gtk_button_set_child(GTK_BUTTON(btn), v);
		gtk_widget_add_css_class(btn, "flat");
		std::string data = szDataPrefix ? szDataPrefix : "";
		data += stem;
		g_object_set_data_full(G_OBJECT(btn), "abi-em-method",
							   g_strdup(szMethod), g_free);
		g_object_set_data_full(G_OBJECT(btn), "abi-em-data",
							   g_strdup(data.c_str()), g_free);
		g_signal_connect(btn, "clicked",
						 G_CALLBACK(_s_popover_em_clicked), this);
		gtk_flow_box_append(GTK_FLOW_BOX(flow), btn);
	}
}

GtkWidget * AP_UnixRibbon::_makeShapesPopover()
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);

	GtkWidget * sw = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(
		GTK_SCROLLED_WINDOW(sw), TRUE);
	gtk_scrolled_window_set_max_content_height(
		GTK_SCROLLED_WINDOW(sw), 460);
	gtk_scrolled_window_set_min_content_width(
		GTK_SCROLLED_WINDOW(sw), 400);
	GtkWidget * inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), inner);
	gtk_box_append(GTK_BOX(box), sw);

	static const struct { const char * title; const char * dir;
						  const char * data; } s_cats[] =
	{
		{ "Basic Shapes",	"artwork/shapes/basic",		"basic/" },
		{ "Block Arrows",	"artwork/shapes/arrows",	"arrows/" },
		{ "Symbol Shapes",	"artwork/shapes/symbols",	"symbols/" },
		{ "Stars and Banners","artwork/shapes/stars",	"stars/" },
		{ "Callouts",		"artwork/shapes/callouts",	"callouts/" },
		{ "Flowchart",		"artwork/shapes/flowchart",	"flowchart/" },
	};
	for (unsigned i = 0; i < G_N_ELEMENTS(s_cats); i++)
		_addGalleryDir(inner, "insertShape",
					   s_cats[i].dir, s_cats[i].data, s_cats[i].title,
					   26, false);

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* 3D Models: the bundled Fluent UI 3D emoji set (artwork/3d) */
GtkWidget * AP_UnixRibbon::_make3DModelsPopover()
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);

	GtkWidget * sw = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(
		GTK_SCROLLED_WINDOW(sw), TRUE);
	gtk_scrolled_window_set_max_content_height(
		GTK_SCROLLED_WINDOW(sw), 440);
	gtk_scrolled_window_set_min_content_width(
		GTK_SCROLLED_WINDOW(sw), 400);
	GtkWidget * inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), inner);
	gtk_box_append(GTK_BOX(box), sw);

	_addGalleryDir(inner, "insert3DModel", "artwork/3d", "",
				   "3D Illustrations", 56, true);

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* Word's Media dropdown: media objects cannot be embedded, so the
 * "from File" rows insert a file:// link that opens in the system
 * player; the browser rows report unsupported */
GtkWidget * AP_UnixRibbon::_makeMediaPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _presetRow("Video Browser…", nullptr, nullptr,
							  "notImplemented", "Video Browser"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Video from File…",
							  "Insert a link to a video file",
							  nullptr, "insMediaFile", "video"));
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Audio Browser…", nullptr, nullptr,
							  "notImplemented", "Audio Browser"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Audio from File…",
							  "Insert a link to an audio file",
							  nullptr, "insMediaFile", "audio"));
	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* Word's WordArt gallery: a grid of styled "A" tiles; clicking one
 * inserts sample text with that character styling */
/* WordArt preview: parse a "key=value;…" preset spec into draw
 * state, then render a sample glyph with real cairo effects —
 * the same visual pipeline as GR_TextEffects in drawChars. */
struct WPArtSpec
{
	GR_TextEffects fx;
	UT_RGBColor    fill;
	std::string    font;
	double         sizePt;
	bool           bold;
	bool           italic;
	WPArtSpec() : fill(68, 114, 196), font("Georgia"),
				  sizePt(30), bold(true), italic(false) {}
};

static void s_wp_color(const std::string & s, size_t pos,
					   UT_RGBColor & col)
{
	std::string hex;
	while (pos < s.size() && isxdigit((unsigned char)s[pos]) &&
		   hex.size() < 6)
		hex += s[pos++];
	if (hex.size() == 6)
		col.setColor(hex.c_str());
}

static void s_wp_parse(const std::string & d, WPArtSpec & a)
{
	size_t p = 0;
	while (p < d.size())
	{
		size_t e = d.find(';', p);
		std::string kv = d.substr(p, e == std::string::npos ? e : e - p);
		size_t eq = kv.find('=');
		if (eq != std::string::npos)
		{
			std::string k = kv.substr(0, eq);
			std::string v = kv.substr(eq + 1);
			size_t c = v[0] == '#' ? 1 : 0;
			if (k == "font")
				a.font = v;
			else if (k == "size")
				a.sizePt = g_ascii_strtod(v.c_str(), nullptr);
			else if (k == "italic" && v == "1")
				a.italic = true;
			else if (k == "weight")
				a.bold = (v != "normal");
			else if (k == "color")
				s_wp_color(v, c, a.fill);
			else if (k == "outline")
			{
				a.fx.m_bOutline = true;
				s_wp_color(v, c, a.fx.m_colOutline);
				size_t colon = v.find(':');
				if (colon != std::string::npos)
				{
					double w = g_ascii_strtod(v.c_str() + colon + 1,
											  nullptr);
					if (w > 0.05 && w < 20)
						a.fx.m_outlineWidthPt = w;
				}
			}
			else if (k == "gradient")
			{
				size_t s2 = c + 7;
				if (s2 < v.size() && v[s2] == '#')
					++s2;
				a.fx.m_bGradient = true;
				s_wp_color(v, c, a.fx.m_colGradFrom);
				s_wp_color(v, s2, a.fx.m_colGradTo);
				size_t colon = v.find(':', s2);
				if (colon != std::string::npos && v[colon + 1] == 'h')
					a.fx.m_bGradVertical = false;
			}
			else if (k == "shadow")
			{
				a.fx.m_bShadow = true;
				s_wp_color(v, c, a.fx.m_colShadow);
				size_t colon = v.find(':');
				if (colon != std::string::npos)
				{
					a.fx.m_shadowDXpt = g_ascii_strtod(
						v.c_str() + colon + 1, nullptr);
					const char * comma = strchr(v.c_str() + colon + 1,
												',');
					a.fx.m_shadowDYpt = comma
						? g_ascii_strtod(comma + 1, nullptr)
						: a.fx.m_shadowDXpt;
				}
			}
			else if (k == "reflect")
				a.fx.m_bReflection = (v == "1" || v == "true");
		}
		if (e == std::string::npos)
			break;
		p = e + 1;
	}
}

static void s_wp_set_rgb(cairo_t * cr, const UT_RGBColor & c)
{
	cairo_set_source_rgb(cr, c.m_red / 255.0, c.m_grn / 255.0,
						 c.m_blu / 255.0);
}

static GtkWidget * s_wp_preview(const std::string & spec, int w, int h)
{
	WPArtSpec a;
	s_wp_parse(spec, a);
	const GR_TextEffects & fx = a.fx;

	cairo_surface_t * sf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, w, h);
	cairo_t * cr = cairo_create(sf);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	PangoLayout * lo = pango_cairo_create_layout(cr);
	PangoFontDescription * d = pango_font_description_new();
	pango_font_description_set_family(d, a.font.c_str());
	pango_font_description_set_size(d,
		(gint)(a.sizePt * PANGO_SCALE * 0.85));
	pango_font_description_set_weight(d,
		a.bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
	pango_font_description_set_style(d,
		a.italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
	pango_layout_set_font_description(lo, d);
	pango_font_description_free(d);
	pango_layout_set_text(lo, "A", -1);

	int tw = 0, th = 0;
	pango_layout_get_pixel_size(lo, &tw, &th);
	double refl = fx.m_bReflection ? th * 0.45 : 0;
	double ox = (w - tw) / 2.0, oy = (h - th - refl) / 2.0;
	const double pts = 96.0 / 72.0;

	/* shadow */
	if (fx.m_bShadow)
	{
		cairo_save(cr);
		cairo_translate(cr, ox + fx.m_shadowDXpt * pts,
						oy + fx.m_shadowDYpt * pts);
		pango_cairo_layout_path(cr, lo);
		s_wp_set_rgb(cr, fx.m_colShadow);
		cairo_fill(cr);
		cairo_restore(cr);
	}
	/* outline under fill */
	if (fx.m_bOutline)
	{
		cairo_save(cr);
		cairo_translate(cr, ox, oy);
		pango_cairo_layout_path(cr, lo);
		cairo_set_line_width(cr, fx.m_outlineWidthPt * pts);
		cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
		s_wp_set_rgb(cr, fx.m_colOutline);
		cairo_stroke(cr);
		cairo_restore(cr);
	}
	/* fill */
	cairo_save(cr);
	cairo_translate(cr, ox, oy);
	if (fx.m_bGradient)
	{
		pango_cairo_layout_path(cr, lo);
		cairo_pattern_t * pat = fx.m_bGradVertical
			? cairo_pattern_create_linear(0, 0, 0, th)
			: cairo_pattern_create_linear(0, 0, tw, 0);
		cairo_pattern_add_color_stop_rgb(pat, 0,
			a.fx.m_colGradFrom.m_red / 255.0,
			a.fx.m_colGradFrom.m_grn / 255.0,
			a.fx.m_colGradFrom.m_blu / 255.0);
		cairo_pattern_add_color_stop_rgb(pat, 1,
			a.fx.m_colGradTo.m_red / 255.0,
			a.fx.m_colGradTo.m_grn / 255.0,
			a.fx.m_colGradTo.m_blu / 255.0);
		cairo_set_source(cr, pat);
		cairo_fill(cr);
		cairo_pattern_destroy(pat);
	}
	else
	{
		s_wp_set_rgb(cr, a.fill);
		pango_cairo_show_layout(cr, lo);
	}
	cairo_restore(cr);
	/* reflection — flipped copy, alpha-faded */
	if (fx.m_bReflection)
	{
		cairo_save(cr);
		cairo_translate(cr, ox, oy + th + 1);
		cairo_push_group(cr);
		cairo_scale(cr, 1.0, -1.0);
		pango_cairo_layout_path(cr, lo);
		if (fx.m_bGradient)
		{
			cairo_pattern_t * pat = fx.m_bGradVertical
				? cairo_pattern_create_linear(0, 0, 0, -th)
				: cairo_pattern_create_linear(0, 0, tw, 0);
			cairo_pattern_add_color_stop_rgb(pat, 0,
				a.fx.m_colGradFrom.m_red / 255.0,
				a.fx.m_colGradFrom.m_grn / 255.0,
				a.fx.m_colGradFrom.m_blu / 255.0);
			cairo_pattern_add_color_stop_rgb(pat, 1,
				a.fx.m_colGradTo.m_red / 255.0,
				a.fx.m_colGradTo.m_grn / 255.0,
				a.fx.m_colGradTo.m_blu / 255.0);
			cairo_set_source(cr, pat);
			cairo_fill(cr);
			cairo_pattern_destroy(pat);
		}
		else
		{
			s_wp_set_rgb(cr, a.fill);
			cairo_fill(cr);
		}
		cairo_pattern_t * txt = cairo_pop_group(cr);
		cairo_restore(cr);
		cairo_save(cr);
		cairo_set_source(cr, txt);
		cairo_pattern_t * mask = cairo_pattern_create_linear(
			0, oy + th + 1, 0, oy + th + 1 + refl);
		cairo_pattern_add_color_stop_rgba(mask, 0, 0, 0, 0, 0.5);
		cairo_pattern_add_color_stop_rgba(mask, 1, 0, 0, 0, 0.0);
		cairo_mask(cr, mask);
		cairo_pattern_destroy(mask);
		cairo_pattern_destroy(txt);
		cairo_restore(cr);
	}
	g_object_unref(lo);
	cairo_destroy(cr);

	cairo_surface_flush(sf);
	GBytes * bytes = g_bytes_new_with_free_func(
		cairo_image_surface_get_data(sf),
		cairo_image_surface_get_height(sf) *
			cairo_image_surface_get_stride(sf),
		(GDestroyNotify)cairo_surface_destroy, sf);
	GdkTexture * tex = gdk_memory_texture_new(
		w, h,
#ifdef G_LITTLE_ENDIAN
		GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
#else
		GDK_MEMORY_A8R8G8B8_PREMULTIPLIED,
#endif
		bytes, cairo_image_surface_get_stride(sf));
	g_bytes_unref(bytes);
	GtkWidget * pic = gtk_picture_new_for_paintable(GDK_PAINTABLE(tex));
	g_object_unref(tex);
	gtk_picture_set_content_fit(GTK_PICTURE(pic), GTK_CONTENT_FIT_CONTAIN);
	return pic;
}

GtkWidget * AP_UnixRibbon::_makeWordArtPopover()
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
	gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
	gtk_widget_set_margin_top(grid, 6);
	gtk_widget_set_margin_bottom(grid, 6);
	gtk_widget_set_margin_start(grid, 6);
	gtk_widget_set_margin_end(grid, 6);

	/* preset spec strings consumed by insertWordArt (and by the
	 * preview painter above) — modelled on Word's WordArt gallery */
	static const char * s_presets[] =
	{
		"color=000000",
		"color=4472C4",
		"color=FFFFFF;outline=ED7D31:1.0",
		"color=FFC000;shadow=595959:1.5,1.5",
		"color=4472C4;gradient=5B9BD5-2E74B5",
		"color=ED7D31;gradient=FFD966-C55A11;outline=843C0C:0.75",
		"color=70AD47;reflect=1",
		"color=FFFFFF;outline=2E74B5:1.25",
		"color=7030A0;gradient=A64DFF-3B1D5E",
		"color=C00000;shadow=595959:2,2",
		"color=595959;gradient=D9D9D9-404040",
		"color=FFFFFF;outline=BF9000:1.0;shadow=808080:1.5,1.5",
		"color=4472C4;reflect=1",
		"color=2E9396;gradient=40C4C8-1D6B6E;reflect=1",
		"color=FFFFFF;outline=000000:1.5",
	};
	for (unsigned i = 0; i < G_N_ELEMENTS(s_presets); i++)
	{
		GtkWidget * btn = gtk_button_new();
		GtkWidget * pic = s_wp_preview(s_presets[i], 64, 52);
		gtk_button_set_child(GTK_BUTTON(btn), pic);
		gtk_widget_add_css_class(btn, "flat");

		g_object_set_data_full(G_OBJECT(btn), "abi-em-method",
							   g_strdup("insertWordArt"), g_free);
		g_object_set_data_full(G_OBJECT(btn), "abi-em-data",
							   g_strdup(s_presets[i]), g_free);
		g_signal_connect(btn, "clicked",
						 G_CALLBACK(_s_popover_em_clicked), this);
		gtk_grid_attach(GTK_GRID(grid), btn, i % 5, i / 5, 1, 1);
	}
	gtk_popover_set_child(GTK_POPOVER(popover), grid);
	return popover;
}

GtkWidget * AP_UnixRibbon::_makeCommentDeletePopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _presetRow("Delete Comment",
							  "Delete the comment at the insertion point",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_DELETE,
								  16, 16),
							  "delAnnotation", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Delete All Comments",
							  "Delete every comment in the document",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_DELETE,
								  16, 16),
							  "delAllAnnotations", nullptr));
	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* a popover row with a check-mark slot, like Word's toggling
 * menu entries; szKind names the state _evalCheckKind() reads and
 * _refreshCheckRows() re-reads whenever the popover is shown */
/* a popover row whose leading slot swaps between the row icon and a
 * check mark - Word-style: the icon shows unchecked, the tick
 * replaces it when the option is on */
GtkWidget * AP_UnixRibbon::_checkRow(const char * szLabel,
									 const char * szDetail,
									 const char * szMethod,
									 const char * szData,
									 const char * szKind,
									 GtkWidget * icon)
{
	GtkWidget * stack = gtk_stack_new();
	gtk_stack_set_transition_type(GTK_STACK(stack),
								  GTK_STACK_TRANSITION_TYPE_NONE);
	if (!icon)
	{
		_PageSpec blank = { 0, 0, 0, 0, 1, false, false, 0, true };
		icon = _glyph_widget(blank, 16, 16);
	}
	gtk_stack_add_named(GTK_STACK(stack), icon, "icon");
	_PageSpec chkSpec = { 0, 0, 0, 0, 1, false, false, 0, true };
	gtk_stack_add_named(GTK_STACK(stack),
						_glyph_widget(chkSpec, 16, 16, _glyph_check),
						"check");
	gtk_stack_set_visible_child_name(GTK_STACK(stack),
						_evalCheckKind(szKind) ? "check" : "icon");
	GtkWidget * btn = _presetRow(szLabel, szDetail, stack,
							   szMethod, szData);
	g_object_set_data_full(G_OBJECT(btn), "abi-check-kind",
						   g_strdup(szKind), g_free);
	g_object_set_data(G_OBJECT(btn), "abi-check-img", stack);
	return btn;
}

/* name of the active Display-for-Review mode */
const char * AP_UnixRibbon::_markupModeName() const
{
	FV_View * pView = static_cast<FV_View *>(
		m_pFrame ? m_pFrame->getCurrentView() : nullptr);
	if (!pView)
		return "All Markup";
	if (pView->isShowRevBars())
		return "Simple Markup";
	if (pView->isShowRevisions())
		return "All Markup";
	if (pView->getRevisionLevel() == 0)
		return "Original";
	return "No Markup";
}

/* evaluate a check-row kind against the live view/frame state */
bool AP_UnixRibbon::_evalCheckKind(const char * szKind) const
{
	if (!szKind)
		return false;

	if (!strcmp(szKind, "ann-contextual"))
	{
		bool b = true;
		XAP_Prefs * pPrefs = XAP_App::getApp()->getPrefs();
		if (pPrefs)
			pPrefs->getPrefsValueBool(AP_PREF_KEY_DisplayAnnotations, b);
		return b;
	}
	if (!strcmp(szKind, "ann-pane"))
	{
		return (m_pFrame && m_pFrame->getFrameImpl())
			? m_pFrame->getFrameImpl()->isCommentsPaneVisible()
			: false;
	}
	if (!strcmp(szKind, "grammar"))
	{
		bool b = false;
		XAP_Prefs * pPrefs = XAP_App::getApp()->getPrefs();
		if (pPrefs)
			pPrefs->getPrefsValueBool(AP_PREF_KEY_AutoGrammarCheck, b);
		return b;
	}

	FV_View * pView = static_cast<FV_View *>(
		m_pFrame ? m_pFrame->getCurrentView() : nullptr);
	if (!pView)
		return false;

	if (!strcmp(szKind, "track"))
		return pView->isMarkRevisions();
	if (!strcmp(szKind, "revauto"))
		return pView->getDocument()
			&& pView->getDocument()->isAutoRevisioning();
	if (!strncmp(szKind, "mode:", 5))
	{
		const char * key = "none";
		if (pView->isShowRevisions())
			key = "all";
		else if (pView->isShowRevBars())
			key = "simple";
		else if (pView->getRevisionLevel() == 0)
			key = "original";
		return !strcmp(szKind + 5, key);
	}
	return false;
}

/* walk a popover's rows and re-paint every check-mark slot */
void AP_UnixRibbon::_refreshCheckRows(GtkWidget * popover)
{
	GtkWidget * box = gtk_popover_get_child(GTK_POPOVER(popover));
	for (GtkWidget * w = box ? gtk_widget_get_first_child(box) : nullptr;
		 w; w = gtk_widget_get_next_sibling(w))
	{
		const char * kind = static_cast<const char *>(
			g_object_get_data(G_OBJECT(w), "abi-check-kind"));
		GtkWidget * img = static_cast<GtkWidget *>(
			g_object_get_data(G_OBJECT(w), "abi-check-img"));
		if (kind && img && GTK_IS_STACK(img))
			gtk_stack_set_visible_child_name(GTK_STACK(img),
				_evalCheckKind(kind) ? "check" : "icon");
	}
}

void AP_UnixRibbon::_s_popover_check_show(GtkPopover * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	if (self)
		self->_refreshCheckRows(GTK_WIDGET(w));
}

GtkWidget * AP_UnixRibbon::_makeCommentShowPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _checkRow("Contextual",
							 "Show comments in the document",
							 "toggleDisplayAnnotations", nullptr,
							 "ann-contextual",
							 _layout_icon(
								 (XAP_Menu_Id)AP_MENU_ID_TOOLS_ANNOTATIONS_TOGGLE_DISPLAY,
								 16, 16)));
	gtk_box_append(GTK_BOX(box),
				   _checkRow("List",
							 "Show comments in the reviewing pane",
							 "commentsPane", nullptr,
							 "ann-pane",
							 _layout_icon(
								 (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_PANE,
								 16, 16)));
	g_signal_connect(popover, "show",
					 G_CALLBACK(_s_popover_check_show), this);
	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

GtkWidget * AP_UnixRibbon::_makeSpellingPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _presetRow("Spelling\xE2\x80\xA6",
							  "Check the spelling of the document",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_SPELL,
								  16, 16),
							  "dlgSpell", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _checkRow("Check Grammar",
							 "Check grammar as you type",
							 "toggleAutoGrammar", nullptr,
							 "grammar",
							 _layout_icon(
								 (XAP_Menu_Id)AP_MENU_ID_TOOLS_SPELL,
								 16, 16)));
	g_signal_connect(popover, "show",
					 G_CALLBACK(_s_popover_check_show), this);
	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

GtkWidget * AP_UnixRibbon::_makeTrackChangesPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _checkRow("Track Changes",
							 "Track every edit you make",
							 "toggleMarkRevisions", nullptr,
							 "track",
							 _layout_icon(
								 (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MARK,
								 16, 16)));
	gtk_box_append(GTK_BOX(box),
				   _checkRow("Auto Revision",
							 "Start a new revision on every save",
							 "toggleAutoRevision", nullptr,
							 "revauto",
							 _layout_icon(
								 (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_AUTO,
								 16, 16)));
	gtk_box_append(GTK_BOX(box), gtk_separator_new(
								   GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Start New Revision",
							  "Begin a new revision level",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_NEW_REVISION,
								  16, 16),
							  "startNewRevision", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Purge All Revisions",
							  "Delete the revision history",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_PURGE,
								  16, 16),
							  "purgeAllRevisions", nullptr));
	g_signal_connect(popover, "show",
					 G_CALLBACK(_s_popover_check_show), this);
	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

GtkWidget * AP_UnixRibbon::_makeMarkupPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _popover_section_label("Display for Review"));
	gtk_box_append(GTK_BOX(box),
				   _checkRow("Simple Markup",
							 "A red bar in the margin marks changed lines",
							 "revisionDisplayMode", "simple",
							 "mode:simple",
							 _layout_icon(
								 (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_DISPLAY,
								 16, 16)));
	gtk_box_append(GTK_BOX(box),
				   _checkRow("All Markup",
							 "Show insertions and deletions inline",
							 "revisionDisplayMode", "all",
							 "mode:all",
							 _layout_icon(
								 (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_DISPLAY,
								 16, 16)));
	gtk_box_append(GTK_BOX(box),
				   _checkRow("No Markup",
							 "Show the document with all changes applied",
							 "revisionDisplayMode", "none",
							 "mode:none",
							 _layout_icon(
								 (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_DISPLAY,
								 16, 16)));
	gtk_box_append(GTK_BOX(box),
				   _checkRow("Original",
							 "Show the document before any changes",
							 "revisionDisplayMode", "original",
							 "mode:original",
							 _layout_icon(
								 (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_MENUPOP_DISPLAY,
								 16, 16)));
	gtk_box_append(GTK_BOX(box), gtk_separator_new(
								   GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Show Revisions",
							  "Toggle the inline revision display",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_SHOW,
								  16, 16),
							  "toggleShowRevisions", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Compare Revisions\xE2\x80\xA6",
							  "Pick the revision level shown",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_SET_VIEW_LEVEL,
								  16, 16),
							  "revisionSetViewLevel", nullptr));
	g_signal_connect(popover, "show",
					 G_CALLBACK(_s_popover_check_show), this);
	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

GtkWidget * AP_UnixRibbon::_makeAcceptPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _presetRow("Accept and Move to Next",
							  "Accept the revision at the caret and move "
							  "to the next",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_ACCEPT_REVISION,
								  16, 16),
							  "revisionAcceptNext", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Accept This Change",
							  "Accept the revision at the caret",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_ACCEPT_REVISION,
								  16, 16),
							  "revisionAccept", nullptr));
	gtk_box_append(GTK_BOX(box), gtk_separator_new(
								   GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Accept All Changes Shown",
							  "Accept every revision currently shown",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_ACCEPT_REVISION,
								  16, 16),
							  "revisionAcceptAllShown", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Accept All Changes",
							  "Accept every revision in the document",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_ACCEPT_REVISION,
								  16, 16),
							  "revisionAcceptAll", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Accept All Changes and Stop Tracking",
							  "Accept every revision and stop "
							  "tracking changes",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_ACCEPT_REVISION,
								  16, 16),
							  "revisionAcceptAllStopTracking",
							  nullptr));
	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

GtkWidget * AP_UnixRibbon::_makeRejectPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _presetRow("Reject and Move to Next",
							  "Reject the revision at the caret and move "
							  "to the next",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_REJECT_REVISION,
								  16, 16),
							  "revisionRejectNext", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Reject This Change",
							  "Reject the revision at the caret",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_REJECT_REVISION,
								  16, 16),
							  "revisionReject", nullptr));
	gtk_box_append(GTK_BOX(box), gtk_separator_new(
								   GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Reject All Changes Shown",
							  "Reject every revision currently shown",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_REJECT_REVISION,
								  16, 16),
							  "revisionRejectAllShown", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Reject All Changes",
							  "Reject every revision in the document",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_REJECT_REVISION,
								  16, 16),
							  "revisionRejectAll", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Reject All Changes and Stop Tracking",
							  "Reject every revision and stop "
							  "tracking changes",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_REJECT_REVISION,
								  16, 16),
							  "revisionRejectAllStopTracking",
							  nullptr));
	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

GtkWidget * AP_UnixRibbon::_makeComparePopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _presetRow("Compare Documents\xE2\x80\xA6",
							  "Compare two versions of a document",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_COMPARE_DOCUMENTS,
								  16, 16),
							  "revisionCompareDocuments", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Combine Documents\xE2\x80\xA6",
							  "Combine revisions from another open "
							  "document into this one",
							  _layout_icon(
								  (XAP_Menu_Id)AP_MENU_ID_TOOLS_REVISIONS_COMBINE_DOCUMENTS,
								  16, 16),
							  "revisionCombineDocuments", nullptr));
	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* render a LaTeX fragment to a GtkPicture preview tile */
GtkWidget * AP_UnixRibbon::_equationPreview(const char * szLatex,
											int w, int h)
{
	GR_MathTypesetter ts;
	ts.parseLaTeX(szLatex);
	cairo_surface_t * ms = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
													  1, 1);
	cairo_t * mc = cairo_create(ms);
	ts.layout(mc, "DejaVu Serif", 11, true);
	cairo_destroy(mc);
	cairo_surface_destroy(ms);

	double ew = ts.width(), eh = ts.ascent() + ts.descent();
	double scale = 1.0;
	if (ew > w - 8) scale = (w - 8) / ew;
	if (eh * scale > h - 4) scale = (h - 4) / eh;

	cairo_surface_t * sf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, w, h);
	cairo_t * cr = cairo_create(sf);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgb(cr, 0.13, 0.13, 0.13);
	cairo_translate(cr, (w - ew * scale) / 2, (h - eh * scale) / 2);
	cairo_scale(cr, scale, scale);
	ts.render(cr);
	cairo_destroy(cr);

	cairo_surface_flush(sf);
	GBytes * bytes = g_bytes_new_with_free_func(
		cairo_image_surface_get_data(sf),
		cairo_image_surface_get_height(sf) *
			cairo_image_surface_get_stride(sf),
		(GDestroyNotify)cairo_surface_destroy, sf);
	GdkTexture * tex = gdk_memory_texture_new(
		w, h,
#ifdef G_LITTLE_ENDIAN
		GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
#else
		GDK_MEMORY_A8R8G8B8_PREMULTIPLIED,
#endif
		bytes, cairo_image_surface_get_stride(sf));
	g_bytes_unref(bytes);
	GtkWidget * pic = gtk_picture_new_for_paintable(GDK_PAINTABLE(tex));
	g_object_unref(tex);
	gtk_picture_set_content_fit(GTK_PICTURE(pic), GTK_CONTENT_FIT_CONTAIN);
	return pic;
}

/* Word's Equation gallery: built-in presets rendered as live previews,
 * plus an "Insert New Equation" row that opens the LaTeX dialog */
GtkWidget * AP_UnixRibbon::_makeEquationPopover()
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_set_margin_top(box, 6);
	gtk_widget_set_margin_bottom(box, 6);
	gtk_widget_set_margin_start(box, 6);
	gtk_widget_set_margin_end(box, 6);

	GtkWidget * cap = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(cap), "<b>Built-In</b>");
	gtk_widget_set_halign(cap, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(box), cap);

	GtkWidget * grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
	gtk_box_append(GTK_BOX(box), grid);

	static const struct { const char * name; const char * latex; } s_eq[] =
	{
		{ "Area of Circle",
		  "A = \\pi r^2" },
		{ "Binomial Theorem",
		  "(x+a)^n = \\sum_{k=0}^{n} \\binom{n}{k} x^k a^{n-k}" },
		{ "Expansion of a Sum",
		  "(1+x)^n = 1 + \\frac{nx}{1!} + \\frac{n(n-1)x^2}{2!} + \\cdots" },
		{ "Fourier Series",
		  "f(x) = a_0 + \\sum_{n=1}^{\\infty} \\left( a_n \\cos \\frac{n\\pi x}{L} + b_n \\sin \\frac{n\\pi x}{L} \\right)" },
		{ "Pythagorean Theorem",
		  "a^2 + b^2 = c^2" },
		{ "Quadratic Formula",
		  "x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}" },
		{ "Taylor Expansion",
		  "e^x = \\sum_{n=0}^{\\infty} \\frac{x^n}{n!}" },
		{ "Trig Identity",
		  "\\sin \\alpha \\pm \\sin \\beta = 2 \\sin \\frac{\\alpha \\pm \\beta}{2} \\cos \\frac{\\alpha \\mp \\beta}{2}" },
		{ "Gaussian Integral",
		  "\\int_0^{\\infty} e^{-x^2} dx = \\frac{\\sqrt{\\pi}}{2}" },
		{ "Euler's Identity",
		  "e^{i\\pi} + 1 = 0" },
	};
	for (unsigned i = 0; i < G_N_ELEMENTS(s_eq); ++i)
	{
		GtkWidget * btn = gtk_button_new();
		GtkWidget * tile = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		gtk_box_append(GTK_BOX(tile), _equationPreview(s_eq[i].latex,
													   150, 46));
		GtkWidget * l = gtk_label_new(s_eq[i].name);
		gtk_widget_add_css_class(l, "caption");
		gtk_box_append(GTK_BOX(tile), l);
		gtk_button_set_child(GTK_BUTTON(btn), tile);
		gtk_widget_add_css_class(btn, "flat");
		gtk_widget_set_tooltip_text(btn, s_eq[i].name);

		std::string data = std::string("display:") + s_eq[i].latex;
		g_object_set_data_full(G_OBJECT(btn), "abi-em-method",
							   g_strdup("insertEquation"), g_free);
		g_object_set_data_full(G_OBJECT(btn), "abi-em-data",
							   g_strdup(data.c_str()), g_free);
		g_signal_connect(btn, "clicked",
						 G_CALLBACK(_s_popover_em_clicked), this);
		gtk_grid_attach(GTK_GRID(grid), btn, i % 2, i / 2, 1, 1);
	}

	gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("<b>Insert New Equation</b>",
							  "Type a LaTeX equation",
							  nullptr, "insertLatexEquation", nullptr));
	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* symbol palette for the contextual Equation ribbon tab */
GtkWidget * AP_UnixRibbon::_makeEquationPalette(bool /*bStructures*/)
{
	GtkWidget * grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 1);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 1);

	static const struct { const char * glyph; const char * latex;
						  const char * tip; } s_sym[] =
	{
		{ "\xc2\xb1", "\\pm", "Plus-minus" }, { "\xc3\x97", "\\times", "Times" },
		{ "\xc3\xb7", "\\div", "Divide" }, { "=", "=", "Equals" },
		{ "\xe2\x89\xa0", "\\neq", "Not equal" },
		{ "\xe2\x89\xa4", "\\leq", "Less or equal" },
		{ "\xe2\x89\xa5", "\\geq", "Greater or equal" },
		{ "\xe2\x89\x88", "\\approx", "Approximately" },
		{ "\xe2\x88\x9e", "\\infty", "Infinity" },
		{ "\xe2\x88\x9d", "\\propto", "Proportional" },
		{ "\xce\xb1", "\\alpha", "Alpha" }, { "\xce\xb2", "\\beta", "Beta" },
		{ "\xce\xb3", "\\gamma", "Gamma" }, { "\xce\xb4", "\\delta", "Delta" },
		{ "\xce\xb8", "\\theta", "Theta" }, { "\xce\xbb", "\\lambda", "Lambda" },
		{ "\xce\xbc", "\\mu", "Mu" }, { "\xcf\x80", "\\pi", "Pi" },
		{ "\xcf\x83", "\\sigma", "Sigma" }, { "\xcf\x86", "\\phi", "Phi" },
		{ "\xcf\x89", "\\omega", "Omega" },
		{ "\xce\x94", "\\Delta", "Delta" },
		{ "\xce\xa3", "\\Sigma", "Sigma" },
		{ "\xce\xa9", "\\Omega", "Omega" },
		{ "\xe2\x88\x82", "\\partial", "Partial" },
		{ "\xe2\x88\x87", "\\nabla", "Nabla" },
		{ "\xe2\x88\x88", "\\in", "Element of" },
		{ "\xe2\x88\x89", "\\notin", "Not element of" },
		{ "\xe2\x8a\x82", "\\subset", "Subset" },
		{ "\xe2\x88\xaa", "\\cup", "Union" },
		{ "\xe2\x88\xa9", "\\cap", "Intersection" },
		{ "\xe2\x88\x80", "\\forall", "For all" },
		{ "\xe2\x88\x83", "\\exists", "Exists" },
		{ "\xe2\x86\x92", "\\rightarrow", "Right arrow" },
		{ "\xe2\x86\x90", "\\leftarrow", "Left arrow" },
		{ "\xe2\x87\x92", "\\Rightarrow", "Double arrow" },
		{ "\xe2\x86\x94", "\\leftrightarrow", "Both ways" },
		{ "\xe2\x88\x85", "\\emptyset", "Empty set" },
		{ "\xe2\x84\x9d", "\\mathbb{R}", "Reals" },
		{ "\xe2\x84\xa4", "\\mathbb{Z}", "Integers" },
		{ "\xe2\x84\x95", "\\mathbb{N}", "Naturals" },
	};
	/* Word's symbols strip: two rows of larger glyphs that scroll
	 * horizontally when they overflow the ribbon width */
	for (unsigned i = 0; i < G_N_ELEMENTS(s_sym); ++i)
	{
		const char * glyph = s_sym[i].glyph;
		const char * latex = s_sym[i].latex;
		const char * tip   = s_sym[i].tip;
		GtkWidget * btn = gtk_button_new();
		GtkWidget * l = gtk_label_new(nullptr);
		char * markup = g_markup_printf_escaped(
			"<span size='larger'>%s</span>", glyph);
		gtk_label_set_markup(GTK_LABEL(l), markup);
		g_free(markup);
		gtk_widget_set_size_request(l, 30, 30);
		gtk_button_set_child(GTK_BUTTON(btn), l);
		gtk_widget_add_css_class(btn, "flat");
		gtk_widget_set_tooltip_text(btn, tip);
		g_object_set_data_full(G_OBJECT(btn), "abi-em-method",
							   g_strdup("equationInsertSymbol"), g_free);
		g_object_set_data_full(G_OBJECT(btn), "abi-em-data",
							   g_strdup(latex), g_free);
		g_signal_connect(btn, "clicked",
						 G_CALLBACK(_s_popover_em_clicked), this);
		gtk_grid_attach(GTK_GRID(grid), btn, i / 2, i % 2, 1, 1);
	}

	GtkWidget * sw = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_ALWAYS, GTK_POLICY_NEVER);
	gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(sw), TRUE);
	/* real scrollbar trough instead of GTK's auto-hiding overlay */
	gtk_scrolled_window_set_overlay_scrolling(
		GTK_SCROLLED_WINDOW(sw), FALSE);
	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sw), 68);
	/* cap the natural width so the Structures group stays on-screen;
	 * the strip scrolls horizontally for the rest */
	gtk_scrolled_window_set_propagate_natural_width(
		GTK_SCROLLED_WINDOW(sw), TRUE);
	gtk_scrolled_window_set_max_content_width(
		GTK_SCROLLED_WINDOW(sw), 470);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), grid);
	gtk_widget_set_hexpand(sw, TRUE);
	gtk_widget_set_vexpand(sw, TRUE);
	return sw;
}

/* ---- Equation tab: Structures group ------------------------------
 * Word's Equation → Structures: large icon-over-caption dropdown
 * buttons (Fraction, Script, Radical, Integral, Large Operator,
 * Bracket, Function, Accent, Limit and Log, Operator, Matrix), each
 * opening a gallery of typeset templates.  A template click appends
 * its LaTeX snippet to the current equation through
 * equationInsertSymbol. */

struct _EqStructItem { const char * latex; const char * tip; };

static const _EqStructItem s_eq_frac[] = {
	{ "\\frac{a}{b}", "Stacked Fraction" },
	{ "\\dfrac{a}{b}", "Large Fraction" },
	{ "\\frac{dy}{dx}", "dy/dx" },
	{ "\\frac{\\Delta y}{\\Delta x}", "\xce\x94y/\xce\x94x" },
	{ "\\frac{\\partial y}{\\partial x}", "Partial derivative" },
	{ "\\frac{\\pi}{2}", "\xcf\x80/2" },
};

static const _EqStructItem s_eq_script[] = {
	{ "x^{a}", "Superscript" },
	{ "x_{a}", "Subscript" },
	{ "x_{a}^{b}", "Sub and superscript" },
	{ "{}_{a}^{b}x", "Prescript" },
	{ "e^{x}", "e to the x" },
	{ "x^{2}", "x squared" },
	{ "e^{-i\\pi}", "e^{-i\xcf\x80}" },
};

static const _EqStructItem s_eq_rad[] = {
	{ "\\sqrt{x}", "Square root" },
	{ "\\sqrt[n]{x}", "Nth root" },
	{ "\\sqrt[3]{x}", "Cube root" },
	{ "\\sqrt{a+b}", "Square root (a+b)" },
	{ "\\sqrt{\\frac{a}{b}}", "Square root (a/b)" },
	{ "\\sqrt{x^{2}+y^{2}}", "Square root (x\xb2+y\xb2)" },
};

static const _EqStructItem s_eq_int[] = {
	{ "\\int", "Integral" },
	{ "\\int_{a}^{b}", "Integral with limits" },
	{ "\\iint", "Double integral" },
	{ "\\iiint", "Triple integral" },
	{ "\\oint", "Contour integral" },
	{ "\\int_{a}^{b} f(x) \\, dx", "Integral of f(x)" },
};

static const _EqStructItem s_eq_bigop[] = {
	{ "\\sum", "Sum" },
	{ "\\sum_{i=1}^{n}", "Sum with limits" },
	{ "\\prod", "Product" },
	{ "\\prod_{i=1}^{n}", "Product with limits" },
	{ "\\coprod", "Coproduct" },
	{ "\\bigcup", "Union" },
	{ "\\bigcap", "Intersection" },
	{ "\\bigoplus", "Direct sum" },
	{ "\\bigotimes", "Tensor product" },
	{ "\\bigodot", "Circle dot" },
	{ "\\bigvee", "Logical or" },
	{ "\\bigwedge", "Logical and" },
};

static const _EqStructItem s_eq_bracket[] = {
	{ "\\left( x \\right)", "Parentheses" },
	{ "\\left[ x \\right]", "Square brackets" },
	{ "\\left\\{ x \\right\\}", "Braces" },
	{ "\\left| x \\right|", "Absolute value" },
	{ "\\left\\| x \\right\\|", "Norm" },
	{ "\\left\\lfloor x \\right\\rfloor", "Floor" },
	{ "\\left\\lceil x \\right\\rceil", "Ceiling" },
	{ "\\left\\langle x \\right\\rangle", "Angle brackets" },
	{ "\\begin{cases} a & p \\\\ b & q \\end{cases}", "Cases" },
};

static const _EqStructItem s_eq_func[] = {
	{ "\\sin x", "Sine" }, { "\\cos x", "Cosine" },
	{ "\\tan x", "Tangent" }, { "\\sec x", "Secant" },
	{ "\\csc x", "Cosecant" }, { "\\cot x", "Cotangent" },
	{ "\\sin^{-1} x", "Inverse sine" }, { "\\arctan x", "Arctangent" },
	{ "\\sinh x", "Sinh" }, { "\\cosh x", "Cosh" },
	{ "\\ln x", "Natural log" }, { "\\log_{2} x", "Log base 2" },
};

static const _EqStructItem s_eq_accent[] = {
	{ "\\hat{x}", "Hat" }, { "\\check{x}", "Check" },
	{ "\\tilde{x}", "Tilde" }, { "\\acute{x}", "Acute" },
	{ "\\grave{x}", "Grave" }, { "\\dot{x}", "Dot" },
	{ "\\ddot{x}", "Double dot" }, { "\\breve{x}", "Breve" },
	{ "\\bar{x}", "Bar" }, { "\\vec{x}", "Vector" },
	{ "\\overline{AB}", "Overline" }, { "\\underline{x}", "Underline" },
};

static const _EqStructItem s_eq_limlog[] = {
	{ "\\lim_{x \\to 0}", "Limit" },
	{ "\\lim_{x \\to \\infty}", "Limit to infinity" },
	{ "\\liminf", "Limit inferior" }, { "\\limsup", "Limit superior" },
	{ "\\min", "Minimum" }, { "\\max", "Maximum" },
	{ "\\log x", "Log" }, { "\\ln x", "Natural log" },
	{ "\\log_{b} x", "Log base b" },
};

static const _EqStructItem s_eq_oper[] = {
	{ "\\det", "Determinant" }, { "\\gcd", "Greatest common divisor" },
	{ "\\ker", "Kernel" }, { "\\arg", "Argument" },
	{ "\\hom", "Homomorphism" }, { "\\dim", "Dimension" },
	{ "\\deg", "Degree" }, { "\\Pr", "Probability" },
	{ "\\inf", "Infimum" }, { "\\sup", "Supremum" },
};

static const _EqStructItem s_eq_matrix[] = {
	{ "\\begin{matrix} a & b \\\\ c & d \\end{matrix}", "Empty matrix" },
	{ "\\begin{pmatrix} a & b \\\\ c & d \\end{pmatrix}", "Parentheses matrix" },
	{ "\\begin{bmatrix} a & b \\\\ c & d \\end{bmatrix}", "Square-bracket matrix" },
	{ "\\begin{Bmatrix} a & b \\\\ c & d \\end{Bmatrix}", "Brace matrix" },
	{ "\\begin{vmatrix} a & b \\\\ c & d \\end{vmatrix}", "Determinant" },
	{ "\\begin{Vmatrix} a & b \\\\ c & d \\end{Vmatrix}", "Double-bar matrix" },
	{ "\\begin{pmatrix} 1 & 0 & 0 \\\\ 0 & 1 & 0 \\\\ 0 & 0 & 1 \\end{pmatrix}",
	  "Identity 3\xd7""3" },
	{ "\\begin{cases} a & p \\\\ b & q \\end{cases}", "Cases" },
};

/* one large dropdown: typeset icon over caption + gallery popover */
GtkWidget * AP_UnixRibbon::_eqStructDrop(const char * szIconLatex,
										 const char * szCaption,
										 const void * itemsV, unsigned n)
{
	const _EqStructItem * items =
		static_cast<const _EqStructItem *>(itemsV);

	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_set_margin_top(box, 6);
	gtk_widget_set_margin_bottom(box, 6);
	gtk_widget_set_margin_start(box, 6);
	gtk_widget_set_margin_end(box, 6);

	GtkWidget * cap = gtk_label_new(nullptr);
	std::string m = std::string("<b>") + szCaption + "</b>";
	gtk_label_set_markup(GTK_LABEL(cap), m.c_str());
	gtk_widget_set_halign(cap, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(box), cap);

	GtkWidget * grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
	gtk_box_append(GTK_BOX(box), grid);
	for (unsigned i = 0; i < n; ++i)
	{
		GtkWidget * btn = gtk_button_new();
		gtk_button_set_child(GTK_BUTTON(btn),
							 _equationPreview(items[i].latex, 52, 34));
		gtk_widget_add_css_class(btn, "flat");
		gtk_widget_set_tooltip_text(btn, items[i].tip);
		g_object_set_data_full(G_OBJECT(btn), "abi-em-method",
							   g_strdup("equationInsertSymbol"), g_free);
		g_object_set_data_full(G_OBJECT(btn), "abi-em-data",
							   g_strdup(items[i].latex), g_free);
		g_signal_connect(btn, "clicked",
						 G_CALLBACK(_s_popover_em_clicked), this);
		gtk_grid_attach(GTK_GRID(grid), btn, i % 4, i / 4, 1, 1);
	}
	gtk_popover_set_child(GTK_POPOVER(popover), box);

	GtkWidget * mb = gtk_menu_button_new();
	GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
	GtkWidget * icon = _equationPreview(szIconLatex, 22, 16);
	gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(vbox), icon);
	GtkWidget * wl = gtk_label_new(szCaption);
	gtk_label_set_justify(GTK_LABEL(wl), GTK_JUSTIFY_CENTER);
	gtk_label_set_lines(GTK_LABEL(wl), 2);
	gtk_label_set_max_width_chars(GTK_LABEL(wl), 8);
	gtk_box_append(GTK_BOX(vbox), wl);
	gtk_menu_button_set_child(GTK_MENU_BUTTON(mb), vbox);
	gtk_menu_button_set_direction(GTK_MENU_BUTTON(mb), GTK_ARROW_DOWN);
	gtk_menu_button_set_has_frame(GTK_MENU_BUTTON(mb), FALSE);
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(mb), popover);
	_slim_widget_tree(mb);
	return mb;
}

GtkWidget * AP_UnixRibbon::_makeEquationStructures()
{
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	static const struct { const char * icon; const char * caption;
						  const _EqStructItem * items; unsigned n; } s_cat[] =
	{
		{ "\\frac{a}{b}", "Fraction",
		  s_eq_frac, G_N_ELEMENTS(s_eq_frac) },
		{ "e^{x}", "Script",
		  s_eq_script, G_N_ELEMENTS(s_eq_script) },
		{ "\\sqrt{x}", "Radical",
		  s_eq_rad, G_N_ELEMENTS(s_eq_rad) },
		{ "\\int_{a}^{b}", "Integral",
		  s_eq_int, G_N_ELEMENTS(s_eq_int) },
		{ "\\sum_{i=0}^{n}", "Large\nOperator",
		  s_eq_bigop, G_N_ELEMENTS(s_eq_bigop) },
		{ "\\left( x \\right)", "Bracket",
		  s_eq_bracket, G_N_ELEMENTS(s_eq_bracket) },
		{ "\\sin \\theta", "Function",
		  s_eq_func, G_N_ELEMENTS(s_eq_func) },
		{ "\\hat{x}", "Accent",
		  s_eq_accent, G_N_ELEMENTS(s_eq_accent) },
		{ "\\lim_{x \\to a}", "Limit and\nLog",
		  s_eq_limlog, G_N_ELEMENTS(s_eq_limlog) },
		{ "\\det", "Operator",
		  s_eq_oper, G_N_ELEMENTS(s_eq_oper) },
		{ "\\begin{pmatrix} a & b \\\\ c & d \\end{pmatrix}", "Matrix",
		  s_eq_matrix, G_N_ELEMENTS(s_eq_matrix) },
	};
	for (unsigned i = 0; i < G_N_ELEMENTS(s_cat); ++i)
		gtk_box_append(GTK_BOX(box),
					   _eqStructDrop(s_cat[i].icon, s_cat[i].caption,
									 s_cat[i].items, s_cat[i].n));
	return box;
}

/* Word's Text Box dropdown */
GtkWidget * AP_UnixRibbon::_makeTextBoxPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _presetRow("Draw Text Box",
							  "Draw a horizontal text box",
							  _layout_icon((XAP_Menu_Id)AP_MENU_ID_INSERT_TEXTBOX, 16, 16),
							  "insTextBox", nullptr));
	{
		_PageSpec spec = { 0, 0, 0, 0, 1, false, false, 0, true };
		gtk_box_append(GTK_BOX(box),
					   _presetRow("Draw Vertical Text Box",
								  "Draw a text box rotated 90 degrees",
								  _glyph_widget(spec, 16, 16, _glyph_vtextbox),
								  "insVerticalTextBox", nullptr));
	}
	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* Word's Object dropdown */
GtkWidget * AP_UnixRibbon::_makeObjectPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _presetRow("Object…", nullptr, nullptr,
							  "notImplemented", "Embedded Object"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Text from File…",
							  "Insert the contents of a document file",
							  nullptr, "insFile", nullptr));
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("RDF Link",
							  "Insert an RDF semantic link",
							  nullptr, "insertXMLID", nullptr));
	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* ---- Header/Footer built-in gallery ------------------------------
 * Word's Header and Footer dropdowns: a scrolling list of preview
 * cards (name over a mini header sketch) for the generated presets
 * in fv_View_cmd.cpp, followed by Edit and Remove rows. */

struct _HdrCardData { const char * id; bool footer; };

static void _hdrftr_card_draw(GtkDrawingArea *, cairo_t * cr,
							  int w, int h, gpointer data)
{
	const _HdrCardData * cd = static_cast<const _HdrCardData *>(data);
	const char * id = cd->id;
	const bool bFooter = cd->footer;

	/* white card + thin frame = the header band on a page */
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_rectangle(cr, 0.5, 0.5, w - 1, h - 1);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.72, 0.72, 0.72);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	auto text = [cr](const char * s, double x, double y,
					 double r, double g, double b, bool bold,
					 bool center, double areaW)
	{
		cairo_set_source_rgb(cr, r, g, b);
		cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL,
							   bold ? CAIRO_FONT_WEIGHT_BOLD
									: CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, 8.5);
		if (center)
		{
			cairo_text_extents_t ext;
			cairo_text_extents(cr, s, &ext);
			cairo_move_to(cr, x + (areaW - ext.width) / 2, y);
		}
		else
			cairo_move_to(cr, x, y);
		cairo_show_text(cr, s);
	};
	auto itext = [cr](const char * s, double x, double y,
					  double r, double g, double b)
	{
		cairo_set_source_rgb(cr, r, g, b);
		cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_ITALIC,
							   CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, 8.5);
		cairo_move_to(cr, x, y);
		cairo_show_text(cr, s);
	};
	auto band = [cr](double x, double y, double bw, double bh,
					 double r, double g, double b)
	{
		cairo_set_source_rgb(cr, r, g, b);
		cairo_rectangle(cr, x, y, bw, bh);
		cairo_fill(cr);
	};
	auto hline = [cr](double x1, double y, double x2,
					  double r, double g, double b, double lw)
	{
		cairo_set_source_rgb(cr, r, g, b);
		cairo_set_line_width(cr, lw);
		cairo_move_to(cr, x1, y);
		cairo_line_to(cr, x2, y);
		cairo_stroke(cr);
	};
	const double m = 10.0;	/* card inner margin */

	if (bFooter)
	{
		/* ---- footer previews (Word's Insert > Footer set) ---- */
		if (!strcmp(id, "blank"))
			text("[Type here]", m, h * 0.5 + 3, 0.5, 0.5, 0.55,
				 false, false, 0);
		else if (!strcmp(id, "blank3"))
		{
			text("[Type here]", m, h * 0.5 + 3, 0.5, 0.5, 0.55,
				 false, false, 0);
			text("[Type here]", 0, h * 0.5 + 3, 0.5, 0.5, 0.55,
				 false, true, w);
			cairo_text_extents_t ext;
			cairo_select_font_face(cr, "sans",
								   CAIRO_FONT_SLANT_NORMAL,
								   CAIRO_FONT_WEIGHT_NORMAL);
			cairo_set_font_size(cr, 8.5);
			cairo_text_extents(cr, "[Type here]", &ext);
			text("[Type here]", w - m - ext.width, h * 0.5 + 3,
				 0.5, 0.5, 0.55, false, false, 0);
		}
		else if (!strcmp(id, "austin"))
		{
			hline(m, h * 0.2, w - m, 0.3, 0.3, 0.3, 0.8);
			hline(m, h * 0.85, w - m, 0.3, 0.3, 0.3, 0.8);
			text("pg. 1", m + 6, h * 0.82, 0.27, 0.45, 0.77,
				 false, false, 0);
		}
		else if (!strcmp(id, "badge"))
		{
			cairo_set_source_rgb(cr, 0.27, 0.45, 0.77);
			cairo_arc(cr, w / 2.0, h * 0.5, h * 0.3, 0, 6.2832);
			cairo_fill(cr);
			text("1", 0, h * 0.5 + 4, 1, 1, 1, true, true, w);
		}
		else if (!strcmp(id, "banded"))
			text("1", 0, h * 0.5 + 3, 0.27, 0.45, 0.77,
				 false, true, w);
		else if (!strcmp(id, "crop"))
			text("[Document Title]", 0, h * 0.5 + 3, 0.5, 0.5, 0.5,
				 false, true, w);
		else if (!strcmp(id, "faceteven"))
			text("[Author name] | [SCHOOL]", m, h * 0.5 + 3,
				 0.5, 0.5, 0.5, false, false, 0);
		else if (!strcmp(id, "facetodd"))
		{
			cairo_text_extents_t ext;
			cairo_select_font_face(cr, "sans",
								   CAIRO_FONT_SLANT_NORMAL,
								   CAIRO_FONT_WEIGHT_NORMAL);
			cairo_set_font_size(cr, 8.5);
			cairo_text_extents(cr, "[DOCUMENT TITLE] | [Document subtitle]",
							   &ext);
			text("[DOCUMENT TITLE] | [Document subtitle]",
				 w - m - ext.width, h * 0.5 + 3, 0.27, 0.45, 0.77,
				 false, false, 0);
		}
		else if (!strcmp(id, "feathered"))
		{
			cairo_set_source_rgb(cr, 0.27, 0.45, 0.77);
			cairo_select_font_face(cr, "sans",
								   CAIRO_FONT_SLANT_NORMAL,
								   CAIRO_FONT_WEIGHT_BOLD);
			cairo_set_font_size(cr, 26);
			cairo_move_to(cr, m, h * 0.75);
			cairo_show_text(cr, "1");
		}
		else if (!strcmp(id, "filigree"))
		{
			hline(m + w * 0.15, h * 0.30, w - m - w * 0.15,
				  0.27, 0.45, 0.77, 0.8);
			text("\xe2\x9d\xa7", 0, h * 0.62, 0.27, 0.45, 0.77,
				 false, true, w);
		}
		else if (!strcmp(id, "headlines"))
		{
			itext("[Document Title]", m, h * 0.40,
				  0.3, 0.3, 0.3);
			hline(m, h * 0.68, w - m, 0.3, 0.3, 0.3, 1.0);
		}
		else if (!strcmp(id, "integral"))
		{
			cairo_text_extents_t ext;
			cairo_select_font_face(cr, "sans",
								   CAIRO_FONT_SLANT_NORMAL,
								   CAIRO_FONT_WEIGHT_NORMAL);
			cairo_set_font_size(cr, 8.5);
			cairo_text_extents(cr, "[AUTHOR NAME]", &ext);
			text("[AUTHOR NAME]", w - m - 30 - ext.width - 6,
				 h * 0.5 + 3, 0.2, 0.2, 0.2, false, false, 0);
			band(w - m - 30, h * 0.30, 30, h * 0.5,
				 0.93, 0.49, 0.19);
			text("1", w - m - 19, h * 0.62, 1, 1, 1, true, false, 0);
		}
		else if (!strcmp(id, "iondark"))
		{
			band(m, h * 0.30, w - 2 * m, h * 0.42,
				 0.27, 0.45, 0.77);
			text("[DOCUMENT TITLE]", m + 8, h * 0.55, 1, 1, 1,
				 false, false, 0);
			cairo_text_extents_t ext;
			cairo_select_font_face(cr, "sans",
								   CAIRO_FONT_SLANT_NORMAL,
								   CAIRO_FONT_WEIGHT_NORMAL);
			cairo_set_font_size(cr, 8.5);
			cairo_text_extents(cr, "[AUTHOR NAME]", &ext);
			text("[AUTHOR NAME]", w - m - 8 - ext.width, h * 0.55,
				 1, 1, 1, false, false, 0);
		}
		else if (!strcmp(id, "ionlight"))
		{
			text("[DOCUMENT TITLE]", m, h * 0.55, 0.27, 0.45, 0.77,
				 false, false, 0);
			cairo_text_extents_t ext;
			cairo_select_font_face(cr, "sans",
								   CAIRO_FONT_SLANT_NORMAL,
								   CAIRO_FONT_WEIGHT_NORMAL);
			cairo_set_font_size(cr, 8.5);
			cairo_text_extents(cr, "[AUTHOR NAME]", &ext);
			text("[AUTHOR NAME]", w - m - ext.width, h * 0.55,
				 0.27, 0.45, 0.77, false, false, 0);
		}
		else if (!strcmp(id, "retrospect"))
		{
			hline(m, h * 0.24, w - m, 0.27, 0.45, 0.77, 1.4);
			text("[AUTHOR]", m, h * 0.52, 0.5, 0.5, 0.5,
				 false, false, 0);
			text("1", w - m - 8, h * 0.52, 0.5, 0.5, 0.5,
				 false, false, 0);
		}
		else if (!strcmp(id, "semaphore"))
			text("Page 1 of 1", 0, h * 0.5 + 3, 0.27, 0.45, 0.77,
				 false, true, w);
		else if (!strcmp(id, "slice"))
		{
			cairo_text_extents_t ext;
			cairo_select_font_face(cr, "sans",
								   CAIRO_FONT_SLANT_NORMAL,
								   CAIRO_FONT_WEIGHT_NORMAL);
			cairo_set_font_size(cr, 8.5);
			cairo_text_extents(cr, "[Author]", &ext);
			text("[Author]", w - m - ext.width, h * 0.5 + 3,
				 0.5, 0.5, 0.5, false, false, 0);
		}
		else if (!strcmp(id, "viewmasterh"))
		{
			hline(m + w * 0.1, h * 0.34, w - m, 0.2, 0.2, 0.2, 1.0);
			cairo_text_extents_t ext;
			cairo_select_font_face(cr, "sans",
								   CAIRO_FONT_SLANT_NORMAL,
								   CAIRO_FONT_WEIGHT_NORMAL);
			cairo_set_font_size(cr, 8.5);
			cairo_text_extents(cr, "[Date]", &ext);
			text("[Date]", w - m - 26 - ext.width - 4, h * 0.62,
				 0.5, 0.5, 0.5, false, false, 0);
			band(w - m - 26, h * 0.42, 26, h * 0.36, 0.1, 0.1, 0.1);
			text("1", w - m - 16, h * 0.66, 1, 1, 1, true, false, 0);
		}
		else if (!strcmp(id, "viewmasterv"))
		{
			cairo_text_extents_t ext;
			cairo_select_font_face(cr, "sans",
								   CAIRO_FONT_SLANT_NORMAL,
								   CAIRO_FONT_WEIGHT_NORMAL);
			cairo_set_font_size(cr, 7.5);
			cairo_text_extents(cr, "[Date]", &ext);
			text("[Date]", w - m - ext.width, h * 0.34,
				 0.5, 0.5, 0.5, false, false, 0);
			band(w - m - 26, h * 0.46, 26, h * 0.34, 0.1, 0.1, 0.1);
			text("1", w - m - 16, h * 0.68, 1, 1, 1, true, false, 0);
		}
		else /* whisp */
			text("Page 1", 0, h * 0.5 + 3, 0.5, 0.5, 0.5,
				 false, true, w);
		return;
	}

	if (!strcmp(id, "blank"))
		text("[Type here]", m, h * 0.5 + 3, 0.5, 0.5, 0.55,
			 false, false, 0);
	else if (!strcmp(id, "blank3"))
	{
		text("[Type here]", m, h * 0.5 + 3, 0.5, 0.5, 0.55,
			 false, false, 0);
		text("[Type here]", 0, h * 0.5 + 3, 0.5, 0.5, 0.55,
			 false, true, w);
		cairo_text_extents_t ext;
		cairo_text_extents(cr, "[Type here]", &ext);
		text("[Type here]", w - m - ext.width, h * 0.5 + 3,
			 0.5, 0.5, 0.55, false, false, 0);
	}
	else if (!strcmp(id, "austin"))
	{
		cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
		cairo_rectangle(cr, m + 8, h * 0.22, w - 2 * m - 16,
						h * 0.56);
		cairo_set_line_width(cr, 0.9);
		cairo_stroke(cr);
		text("[Document title]", m + 16, h * 0.5 + 3,
			 0.27, 0.45, 0.77, false, false, 0);
	}
	else if (!strcmp(id, "badge"))
	{
		band(m, h * 0.20, w - 2 * m, h * 0.60,
			 0.27, 0.33, 0.42);
		text("[DOCUMENT TITLE]", 0, h * 0.5 + 3, 1, 1, 1,
			 true, true, w);
	}
	else if (!strcmp(id, "banded"))
	{
		band(m + w * 0.12, h * 0.22, (w - 2 * m) * 0.76, h * 0.56,
			 0.27, 0.45, 0.77);
		text("[DOCUMENT TITLE]", 0, h * 0.5 + 3, 1, 1, 1,
			 false, true, w);
	}
	else if (!strcmp(id, "crop"))
	{
		band(m, h * 0.18, w * 0.16, 7, 0.27, 0.33, 0.42);
		band(m, h * 0.18, 7, h * 0.5, 0.27, 0.33, 0.42);
		text("1", w - m - 6, h * 0.5, 0.5, 0.5, 0.55,
			 false, false, 0);
	}
	else if (!strcmp(id, "faceteven") || !strcmp(id, "facetodd"))
	{
		bool odd = !strcmp(id, "facetodd");
		band(odd ? w - m - 42 : m, h * 0.18, 42, h * 0.6,
			 0.27, 0.45, 0.77);
		text("1", odd ? w - m - 24 : m + 18, h * 0.52, 1, 1, 1,
			 true, false, 0);
	}
	else if (!strcmp(id, "feathered"))
		text("[Document Title]", m, h * 0.5 + 3, 0.27, 0.45, 0.77,
			 false, false, 0);
	else if (!strcmp(id, "feathered2"))
	{
		text("[Document Title]", m, h * 0.44, 0.27, 0.45, 0.77,
			 false, false, 0);
		cairo_set_source_rgb(cr, 0.65, 0.65, 0.65);
		cairo_set_line_width(cr, 0.8);
		cairo_move_to(cr, m, h * 0.72);
		cairo_line_to(cr, w - m, h * 0.72);
		cairo_stroke(cr);
	}
	else if (!strcmp(id, "filigree"))
	{
		cairo_text_extents_t ext;
		cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL,
							   CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, 8.5);
		cairo_text_extents(cr, "[Document title] | [Author name]",
						   &ext);
		text("[Document title] | [Author name]",
			 w - m - ext.width, h * 0.5 + 3, 0.27, 0.45, 0.77,
			 false, false, 0);
	}
	else if (!strcmp(id, "headlines"))
	{
		band(w - m - 46, h * 0.16, 46, h * 0.64,
			 0.15, 0.15, 0.15);
		text("1", w - m - 26, h * 0.52, 1, 1, 1, true, false, 0);
	}
	else if (!strcmp(id, "integral"))
	{
		band(m + w * 0.10, h * 0.22, (w - 2 * m) * 0.80, h * 0.56,
			 0.93, 0.49, 0.19);
		cairo_text_extents_t ext;
		cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL,
							   CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 8.5);
		cairo_text_extents(cr, "[DOCUMENT TITLE]", &ext);
		text("[DOCUMENT TITLE]", w - m - w * 0.10 - ext.width - 8,
			 h * 0.5 + 3, 1, 1, 1, true, false, 0);
	}
	else if (!strcmp(id, "iondark"))
	{
		band(w - m - 34, h * 0.14, 34, h * 0.66,
			 0.27, 0.45, 0.77);
		text("1", w - m - 20, h * 0.52, 1, 1, 1, true, false, 0);
	}
	else if (!strcmp(id, "ionlight"))
		text("1", w - m - 8, h * 0.5 + 3, 0.27, 0.45, 0.77,
			 true, false, 0);
	else if (!strcmp(id, "retrospect"))
	{
		text("[Document title]", 0, h * 0.44, 0.5, 0.5, 0.5,
			 false, true, w);
		cairo_set_source_rgb(cr, 0.65, 0.65, 0.65);
		cairo_set_line_width(cr, 0.8);
		cairo_move_to(cr, m, h * 0.74);
		cairo_line_to(cr, w - m, h * 0.74);
		cairo_stroke(cr);
	}
	else if (!strcmp(id, "semaphore"))
	{
		text("[Author name]", 0, h * 0.34, 0.27, 0.45, 0.77,
			 false, true, w);
		text("[DOCUMENT TITLE]", 0, h * 0.68, 0.27, 0.45, 0.77,
			 true, true, w);
	}
	else if (!strcmp(id, "slice1"))
	{
		cairo_text_extents_t ext;
		cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL,
							   CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, 8.5);
		cairo_text_extents(cr, "Page 1", &ext);
		text("Page 1", w - m - ext.width, h * 0.5 + 3,
			 0.5, 0.5, 0.5, false, false, 0);
	}
	else if (!strcmp(id, "slice2"))
	{
		cairo_set_source_rgb(cr, 0.52, 0.59, 0.69);
		cairo_set_line_width(cr, 0.8);
		cairo_move_to(cr, w - m - 40, h * 0.36);
		cairo_line_to(cr, w - m, h * 0.36);
		cairo_stroke(cr);
		text("1", w - m - 24, h * 0.62, 0.27, 0.45, 0.77,
			 false, false, 0);
	}
	else if (!strcmp(id, "viewmaster"))
	{
		cairo_text_extents_t ext;
		cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL,
							   CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, 8.5);
		cairo_text_extents(cr, "[Document title]", &ext);
		text("[Document title]", w - m - ext.width, h * 0.5 + 3,
			 0.5, 0.5, 0.5, false, false, 0);
	}
	else /* whisp */
	{
		cairo_text_extents_t ext;
		cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL,
							   CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, 7.5);
		cairo_text_extents(cr, "[Author name]", &ext);
		text("[Author name]", w - m - ext.width, h * 0.30,
			 0.5, 0.5, 0.5, false, false, 0);
		cairo_text_extents(cr, "[Date]", &ext);
		text("[Date]", w - m - ext.width, h * 0.50,
			 0.5, 0.5, 0.5, false, false, 0);
		text("[Document title]", 0, h * 0.82, 0.5, 0.5, 0.5,
			 false, true, w);
	}
}

GtkWidget * AP_UnixRibbon::_makeHdrFtrPopover(bool bFooter)
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);
	gtk_widget_set_margin_start(box, 4);
	gtk_widget_set_margin_end(box, 4);

	GtkWidget * sw = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(
		GTK_SCROLLED_WINDOW(sw), TRUE);
	gtk_scrolled_window_set_max_content_height(
		GTK_SCROLLED_WINDOW(sw), 430);
	GtkWidget * inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), inner);
	gtk_box_append(GTK_BOX(box), sw);
	gtk_popover_set_child(GTK_POPOVER(popover), box);

	gtk_box_append(GTK_BOX(inner), _popover_section_label("Built-in"));

	static const struct { const char * id; const char * name; }
	s_hdr_list[] =
	{
		{ "blank",		"Blank" },
		{ "blank3",		"Blank (Three Columns)" },
		{ "austin",		"Austin" },
		{ "badge",		"Badge" },
		{ "banded",		"Banded" },
		{ "crop",		"Crop" },
		{ "faceteven",	"Facet (Even Page)" },
		{ "facetodd",	"Facet (Odd Page)" },
		{ "feathered",	"Feathered" },
		{ "feathered2",	"Feathered 2" },
		{ "filigree",	"Filigree" },
		{ "headlines",	"Headlines" },
		{ "integral",	"Integral" },
		{ "iondark",	"Ion (Dark)" },
		{ "ionlight",	"Ion (Light)" },
		{ "retrospect",	"Retrospect" },
		{ "semaphore",	"Semaphore" },
		{ "slice1",		"Slice 1" },
		{ "slice2",		"Slice 2" },
		{ "viewmaster",	"ViewMaster" },
		{ "whisp",		"Whisp" },
	},
	s_ftr_list[] =
	{
		{ "blank",		"Blank" },
		{ "blank3",		"Blank (Three Columns)" },
		{ "austin",		"Austin" },
		{ "badge",		"Badge" },
		{ "banded",		"Banded" },
		{ "crop",		"Crop" },
		{ "faceteven",	"Facet (Even Page)" },
		{ "facetodd",	"Facet (Odd Page)" },
		{ "feathered",	"Feathered" },
		{ "filigree",	"Filigree" },
		{ "headlines",	"Headlines" },
		{ "integral",	"Integral" },
		{ "iondark",	"Ion (Dark)" },
		{ "ionlight",	"Ion (Light)" },
		{ "retrospect",	"Retrospect" },
		{ "semaphore",	"Semaphore" },
		{ "slice",		"Slice" },
		{ "viewmasterh","ViewMaster (Horizontal)" },
		{ "viewmasterv","ViewMaster (Vertical)" },
		{ "whisp",		"Whisp" },
	};
	const auto * pList = bFooter ? s_ftr_list : s_hdr_list;
	const unsigned nList = bFooter ? G_N_ELEMENTS(s_ftr_list)
		: G_N_ELEMENTS(s_hdr_list);
	const char * szMethod = bFooter ? "insertFooterPreset"
		: "insertHeaderPreset";
	for (unsigned i = 0; i < nList; i++)
	{
		GtkWidget * v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		GtkWidget * l = gtk_label_new(pList[i].name);
		gtk_label_set_xalign(GTK_LABEL(l), 0.5);
		gtk_box_append(GTK_BOX(v), l);
		GtkWidget * da = gtk_drawing_area_new();
		gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(da), 236);
		gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(da), 44);
		_HdrCardData * cd = g_new0(_HdrCardData, 1);
		cd->id = pList[i].id;
		cd->footer = bFooter;
		gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(da),
									   _hdrftr_card_draw, cd, g_free);
		gtk_widget_set_margin_start(da, 8);
		gtk_widget_set_margin_end(da, 8);
		gtk_box_append(GTK_BOX(v), da);
		GtkWidget * btn = gtk_button_new();
		gtk_button_set_child(GTK_BUTTON(btn), v);
		gtk_widget_add_css_class(btn, "flat");
		g_object_set_data_full(G_OBJECT(btn), "abi-em-method",
							   g_strdup(szMethod), g_free);
		g_object_set_data_full(G_OBJECT(btn), "abi-em-data",
							   g_strdup(pList[i].id), g_free);
		g_signal_connect(btn, "clicked",
						 G_CALLBACK(_s_popover_em_clicked), this);
		gtk_box_append(GTK_BOX(inner), btn);
	}

	gtk_box_append(GTK_BOX(inner),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(inner),
				   _presetRow(bFooter ? "Edit Footer" : "Edit Header",
							  nullptr,
							  _layout_icon(bFooter
								   ? (XAP_Menu_Id)AP_MENU_ID_INSERT_FOOTER
								   : (XAP_Menu_Id)AP_MENU_ID_INSERT_HEADER,
								   16, 16),
							  bFooter ? "editFooter" : "editHeader",
							  nullptr));
	gtk_box_append(GTK_BOX(inner),
				   _presetRow(bFooter ? "Remove Footer" : "Remove Header",
							  nullptr, nullptr,
							  bFooter ? "removeFooter" : "removeHeader",
							  nullptr));
	return popover;
}

/* Word's Page Number dropdown: position + alignment option rows,
 * a format/options dialog shortcut and a remove row */
GtkWidget * AP_UnixRibbon::_makePageNumberPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box), _popover_section_label("Top of Page"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Left", nullptr, nullptr,
							  "pageNumber", "header:left"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Center", nullptr, nullptr,
							  "pageNumber", "header:center"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Right", nullptr, nullptr,
							  "pageNumber", "header:right"));
	gtk_box_append(GTK_BOX(box), _popover_section_label("Bottom of Page"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Left", nullptr, nullptr,
							  "pageNumber", "footer:left"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Center", nullptr, nullptr,
							  "pageNumber", "footer:center"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Right", nullptr, nullptr,
							  "pageNumber", "footer:right"));
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Page Number Options\xe2\x80\xa6", nullptr,
							  nullptr, "insPageNo", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Remove Page Numbers", nullptr, nullptr,
							  "pageNumberRemove", nullptr));
	return popover;
}

/* Word's Drop Cap dropdown: None / Dropped / In margin plus a
 * lines-to-drop option group */
GtkWidget * AP_UnixRibbon::_makeDropCapPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _presetRow("None", nullptr, nullptr,
							  "dropCap", "none"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Dropped", nullptr, nullptr,
							  "dropCap", "dropped:3"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("In margin", nullptr, nullptr,
							  "dropCap", "margin:3"));
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _popover_section_label("Lines to drop"));
	static const char * const s_lines[] =
		{ "2", "3", "4", "5" };
	for (unsigned i = 0; i < G_N_ELEMENTS(s_lines); i++)
	{
		std::string label = std::string(s_lines[i]) + " lines";
		std::string data = std::string("dropped:") + s_lines[i];
		gtk_box_append(GTK_BOX(box),
					   _presetRow(label.c_str(), nullptr, nullptr,
								  "dropCap", data.c_str()));
	}
	return popover;
}

/* Word's Add Text dropdown: mark the paragraph's TOC level */
GtkWidget * AP_UnixRibbon::_makeAddTextPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _presetRow("Do Not Show in Table of Contents",
							  nullptr, nullptr, "tocAddText", "0"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Level 1", nullptr, nullptr,
							  "tocAddText", "1"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Level 2", nullptr, nullptr,
							  "tocAddText", "2"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Level 3", nullptr, nullptr,
							  "tocAddText", "3"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Level 4", nullptr, nullptr,
							  "tocAddText", "4"));
	return popover;
}

/* Word's Next Footnote dropdown */
GtkWidget * AP_UnixRibbon::_makeNextNotePopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _presetRow("Next Footnote", nullptr, nullptr,
							  "footnoteNext", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Previous Footnote", nullptr, nullptr,
							  "footnotePrev", nullptr));
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Next Endnote", nullptr, nullptr,
							  "endnoteNext", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Previous Endnote", nullptr, nullptr,
							  "endnotePrev", nullptr));
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Convert All Footnotes to Endnotes", nullptr,
							  nullptr, "footnoteToEndnote", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Convert All Endnotes to Footnotes", nullptr,
							  nullptr, "endnoteToFootnote", nullptr));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Swap Footnotes and Endnotes", nullptr,
							  nullptr, "noteSwap", nullptr));
	return popover;
}

/* ---------------- References popovers with input fields ----------------
 *
 * Word's References features use dialogs; the ribbon keeps the same
 * fields in popovers so the workflow stays inside the tab.
 */

/* a labelled GtkEntry row; the entry widget is stored on the row as
 * "entry" for the apply handler */
static GtkWidget * _ref_entry_row(const char * szLabel, const char * szText)
{
	GtkWidget * row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget * l = gtk_label_new(szLabel);
	gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
	gtk_widget_set_size_request(l, 76, -1);
	GtkWidget * e = gtk_entry_new();
	if (szText && *szText)
		gtk_editable_set_text(GTK_EDITABLE(e), szText);
	gtk_widget_set_hexpand(e, TRUE);
	gtk_box_append(GTK_BOX(row), l);
	gtk_box_append(GTK_BOX(row), e);
	g_object_set_data(G_OBJECT(row), "entry", e);
	return row;
}

/* a labelled GtkDropDown row; the dropdown widget is stored on the row
 * as "dd" */
static GtkWidget * _ref_dropdown_row(const char * szLabel,
									 const char * const * items,
									 UT_uint32 nItems)
{
	GtkWidget * row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget * l = gtk_label_new(szLabel);
	gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
	gtk_widget_set_size_request(l, 76, -1);
	GtkStringList * list = gtk_string_list_new(nullptr);
	for (UT_uint32 i = 0; i < nItems; i++)
		gtk_string_list_append(list, items[i]);
	GtkWidget * dd = gtk_drop_down_new(G_LIST_MODEL(list), nullptr);
	gtk_widget_set_hexpand(dd, TRUE);
	gtk_box_append(GTK_BOX(row), l);
	gtk_box_append(GTK_BOX(row), dd);
	g_object_set_data(G_OBJECT(row), "dd", dd);
	return row;
}

static const char * _dropdown_text(GtkWidget * dd)
{
	GObject * item = G_OBJECT(gtk_drop_down_get_selected_item(
		GTK_DROP_DOWN(dd)));
	if (!item)
		return "";
	return gtk_string_object_get_string(GTK_STRING_OBJECT(item));
}

/* current selection text, for prefilling the Mark Entry popover */
std::string AP_UnixRibbon::_refSelectionText() const
{
	FV_View * pView = static_cast<FV_View *>(
		m_pFrame ? m_pFrame->getCurrentView() : nullptr);
	if (!pView || pView->isSelectionEmpty())
		return "";
	PT_DocPosition a = pView->getPoint();
	PT_DocPosition b = pView->getSelectionAnchor();
	if (a > b)
		std::swap(a, b);
	UT_UCS4Char * pText = pView->getTextBetweenPos(a, b);
	if (!pText)
		return "";
	const std::string s = UT_UCS4String(pText).utf8_str();
	FREEP(pText);
	return s;
}

void AP_UnixRibbon::_s_caption_apply(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	GtkWidget * ddLabel = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(w), "dd-label"));
	GtkWidget * ddPos = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(w), "dd-pos"));
	GtkWidget * eCustom = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(w), "entry-custom"));

	const char * szCustom = gtk_editable_get_text(GTK_EDITABLE(eCustom));
	std::string sLabel = (szCustom && *szCustom) ? szCustom
		: _dropdown_text(ddLabel);
	const std::string sData =
		sLabel + (gtk_drop_down_get_selected(GTK_DROP_DOWN(ddPos)) == 0
				  ? "|below" : "|above");
	_tb_popdown_popover(w);
	self->_invokeEditMethod("refCaption", sData.c_str());
}

/* Word's Insert Caption: label, optional custom label, position */
GtkWidget * AP_UnixRibbon::_makeCaptionPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	static const char * s_Labels[] = { "Figure", "Table", "Equation" };
	GtkWidget * ddLabel = _ref_dropdown_row("Label:", s_Labels, 3);
	gtk_box_append(GTK_BOX(box), ddLabel);
	GtkWidget * eCustom = _ref_entry_row("Custom:", nullptr);
	gtk_box_append(GTK_BOX(box), eCustom);
	static const char * s_Pos[] =
		{ "Below selected item", "Above selected item" };
	GtkWidget * ddPos = _ref_dropdown_row("Position:", s_Pos, 2);
	gtk_box_append(GTK_BOX(box), ddPos);
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	GtkWidget * btn = gtk_button_new_with_label("Insert Caption");
	gtk_widget_add_css_class(btn, "flat");
	g_object_set_data(G_OBJECT(btn), "dd-label",
					  g_object_get_data(G_OBJECT(ddLabel), "dd"));
	g_object_set_data(G_OBJECT(btn), "dd-pos",
					  g_object_get_data(G_OBJECT(ddPos), "dd"));
	g_object_set_data(G_OBJECT(btn), "entry-custom",
					  g_object_get_data(G_OBJECT(eCustom), "entry"));
	g_signal_connect(btn, "clicked",
					 G_CALLBACK(_s_caption_apply), this);
	gtk_box_append(GTK_BOX(box), btn);
	return popover;
}

void AP_UnixRibbon::_s_tof_apply(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	GtkWidget * ddLabel = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(w), "dd-label"));
	GtkWidget * eCustom = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(w), "entry-custom"));
	const char * szCustom = gtk_editable_get_text(GTK_EDITABLE(eCustom));
	const std::string sLabel = (szCustom && *szCustom) ? szCustom
		: _dropdown_text(ddLabel);
	_tb_popdown_popover(w);
	self->_invokeEditMethod("refInsertTOF", sLabel.c_str());
}

/* Word's Insert Table of Figures: pick the caption label to list */
GtkWidget * AP_UnixRibbon::_makeTOFPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	static const char * s_Labels[] = { "Figure", "Table", "Equation" };
	GtkWidget * ddLabel = _ref_dropdown_row("Caption label:", s_Labels, 3);
	gtk_box_append(GTK_BOX(box), ddLabel);
	GtkWidget * eCustom = _ref_entry_row("Custom:", nullptr);
	gtk_box_append(GTK_BOX(box), eCustom);
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	GtkWidget * btn = gtk_button_new_with_label("Insert Table of Figures");
	gtk_widget_add_css_class(btn, "flat");
	g_object_set_data(G_OBJECT(btn), "dd-label",
					  g_object_get_data(G_OBJECT(ddLabel), "dd"));
	g_object_set_data(G_OBJECT(btn), "entry-custom",
					  g_object_get_data(G_OBJECT(eCustom), "entry"));
	g_signal_connect(btn, "clicked", G_CALLBACK(_s_tof_apply), this);
	gtk_box_append(GTK_BOX(box), btn);
	return popover;
}

void AP_UnixRibbon::_s_xref_map(GtkWidget * popover, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	GtkWidget * listbox = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(popover), "xref-list"));
	UT_return_if_fail(listbox);

	for (GtkWidget * child = gtk_widget_get_first_child(listbox);
		 child;)
	{
		GtkWidget * next = gtk_widget_get_next_sibling(child);
		gtk_list_box_remove(GTK_LIST_BOX(listbox), child);
		child = next;
	}

	FV_View * pView = static_cast<FV_View *>(
		self->m_pFrame ? self->m_pFrame->getCurrentView() : nullptr);
	std::vector<std::string> names;
	if (pView)
		pView->getXRefBookmarks(names);
	for (const std::string & s : names)
	{
		GtkWidget * row = gtk_list_box_row_new();
		GtkWidget * l = gtk_label_new(s.c_str());
		gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
		gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), l);
		g_object_set_data_full(G_OBJECT(row), "bookmark",
							   g_strdup(s.c_str()), g_free);
		gtk_list_box_append(GTK_LIST_BOX(listbox), row);
	}
	if (names.empty())
	{
		GtkWidget * row = gtk_list_box_row_new();
		GtkWidget * l = gtk_label_new("(no bookmarks in document)");
		gtk_widget_set_sensitive(row, FALSE);
		gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), l);
		gtk_list_box_append(GTK_LIST_BOX(listbox), row);
	}
}

void AP_UnixRibbon::_s_xref_apply(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	GtkWidget * listbox = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(w), "xref-list"));
	GtkWidget * ddType = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(w), "dd-type"));
	GtkListBoxRow * row = gtk_list_box_get_selected_row(
		GTK_LIST_BOX(listbox));
	UT_return_if_fail(row);
	const char * szBookmark = static_cast<const char *>(
		g_object_get_data(G_OBJECT(row), "bookmark"));
	UT_return_if_fail(szBookmark);
	const std::string sData =
		std::string(szBookmark) +
		(gtk_drop_down_get_selected(GTK_DROP_DOWN(ddType)) == 0
		 ? "|text" : "|page");
	_tb_popdown_popover(w);
	self->_invokeEditMethod("refXRef", sData.c_str());
}

/* Word's Cross-reference: pick a bookmark, insert its text as a link
 * or its page number as a field */
GtkWidget * AP_UnixRibbon::_makeXRefPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box), _popover_section_label("Reference target"));
	GtkWidget * sw = gtk_scrolled_window_new();
	gtk_widget_set_size_request(sw, 240, 140);
	GtkWidget * listbox = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(listbox),
									GTK_SELECTION_SINGLE);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), listbox);
	gtk_box_append(GTK_BOX(box), sw);

	static const char * s_Types[] =
		{ "Bookmark text (link)", "Page number" };
	GtkWidget * ddType = _ref_dropdown_row("Insert:", s_Types, 2);
	gtk_box_append(GTK_BOX(box), ddType);
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	GtkWidget * btn = gtk_button_new_with_label("Insert Reference");
	gtk_widget_add_css_class(btn, "flat");
	g_object_set_data(G_OBJECT(btn), "xref-list", listbox);
	g_object_set_data(G_OBJECT(btn), "dd-type",
					  g_object_get_data(G_OBJECT(ddType), "dd"));
	g_signal_connect(btn, "clicked", G_CALLBACK(_s_xref_apply), this);
	gtk_box_append(GTK_BOX(box), btn);

	g_object_set_data(G_OBJECT(popover), "xref-list", listbox);
	g_signal_connect(popover, "map", G_CALLBACK(_s_xref_map), this);
	return popover;
}

void AP_UnixRibbon::_s_markentry_map(GtkWidget * popover, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	GtkWidget * e = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(popover), "markentry-entry"));
	UT_return_if_fail(e);
	const std::string s = self->_refSelectionText();
	gtk_editable_set_text(GTK_EDITABLE(e), s.c_str());
	gtk_widget_set_sensitive(e, s.empty());
}

void AP_UnixRibbon::_s_markentry_apply(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	GtkWidget * e = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(w), "markentry-entry"));
	const char * szText = gtk_editable_get_text(GTK_EDITABLE(e));
	_tb_popdown_popover(w);
	self->_invokeEditMethod("refMarkEntry", szText);
}

/* Word's Mark Entry: the selected text (or a typed entry) is flagged
 * for the index */
GtkWidget * AP_UnixRibbon::_makeMarkEntryPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _popover_section_label("Main entry"));
	GtkWidget * eRow = _ref_entry_row("Entry:", nullptr);
	GtkWidget * e = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(eRow), "entry"));
	gtk_box_append(GTK_BOX(box), eRow);
	GtkWidget * hint = gtk_label_new(
		"Select text first, or type the entry. Use \"Main:Sub\" for "
		"a subentry.");
	gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
	gtk_label_set_max_width_chars(GTK_LABEL(hint), 34);
	gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
	gtk_box_append(GTK_BOX(box), hint);
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	GtkWidget * btn = gtk_button_new_with_label("Mark");
	gtk_widget_add_css_class(btn, "flat");
	g_object_set_data(G_OBJECT(btn), "markentry-entry", e);
	g_signal_connect(btn, "clicked",
					 G_CALLBACK(_s_markentry_apply), this);
	gtk_box_append(GTK_BOX(box), btn);

	g_object_set_data(G_OBJECT(popover), "markentry-entry", e);
	g_signal_connect(popover, "map", G_CALLBACK(_s_markentry_map), this);
	return popover;
}

void AP_UnixRibbon::_s_markcit_apply(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	GtkWidget * ddCat = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(w), "dd-cat"));
	GtkWidget * e = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(w), "markcit-entry"));
	const char * szCat = _dropdown_text(ddCat);
	const char * szCit = gtk_editable_get_text(GTK_EDITABLE(e));
	const std::string sData =
		std::string(szCat) + "|" + (szCit ? szCit : "");
	_tb_popdown_popover(w);
	self->_invokeEditMethod("refMarkCitation", sData.c_str());
}

/* Word's Mark Citation: category plus the citation text */
GtkWidget * AP_UnixRibbon::_makeMarkCitPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	static const char * s_Cats[] =
		{ "cases", "statutes", "regulations", "other" };
	GtkWidget * ddCat = _ref_dropdown_row("Category:", s_Cats, 4);
	gtk_box_append(GTK_BOX(box), ddCat);
	GtkWidget * eRow = _ref_entry_row("Citation:", nullptr);
	gtk_box_append(GTK_BOX(box), eRow);
	GtkWidget * hint = gtk_label_new(
		"Select the citation text first, or type it here.");
	gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
	gtk_label_set_max_width_chars(GTK_LABEL(hint), 34);
	gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
	gtk_box_append(GTK_BOX(box), hint);
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	GtkWidget * btn = gtk_button_new_with_label("Mark");
	gtk_widget_add_css_class(btn, "flat");
	g_object_set_data(G_OBJECT(btn), "dd-cat",
					  g_object_get_data(G_OBJECT(ddCat), "dd"));
	g_object_set_data(G_OBJECT(btn), "markcit-entry",
					  g_object_get_data(G_OBJECT(eRow), "entry"));
	g_signal_connect(btn, "clicked",
					 G_CALLBACK(_s_markcit_apply), this);
	gtk_box_append(GTK_BOX(box), btn);
	return popover;
}

void AP_UnixRibbon::_s_citation_apply(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	std::string sData;
	static const char * s_Keys[] =
		{ "entry-author", "entry-year", "entry-title",
		  "entry-publisher" };
	for (UT_uint32 i = 0; i < G_N_ELEMENTS(s_Keys); i++)
	{
		GtkWidget * e = static_cast<GtkWidget *>(
			g_object_get_data(G_OBJECT(w), s_Keys[i]));
		const char * sz = gtk_editable_get_text(GTK_EDITABLE(e));
		if (i)
			sData += '|';
		if (sz)
			sData += sz;
	}
	GtkWidget * ddType = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(w), "dd-type"));
	sData += '|';
	sData += _dropdown_text(ddType);
	_tb_popdown_popover(w);
	self->_invokeEditMethod("refInsertCitation", sData.c_str());
}

/* Word's Insert Citation / Add New Source fields */
GtkWidget * AP_UnixRibbon::_makeCitationPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box),
				   _popover_section_label("New source"));
	GtkWidget * btn = gtk_button_new_with_label("Insert Citation");
	gtk_widget_add_css_class(btn, "flat");

	static const char * s_Fields[] =
		{ "Author:", "Year:", "Title:", "Publisher:" };
	static const char * s_Keys[] =
		{ "entry-author", "entry-year", "entry-title",
		  "entry-publisher" };
	for (UT_uint32 i = 0; i < G_N_ELEMENTS(s_Fields); i++)
	{
		GtkWidget * row = _ref_entry_row(s_Fields[i], nullptr);
		g_object_set_data(G_OBJECT(btn), s_Keys[i],
						  g_object_get_data(G_OBJECT(row), "entry"));
		gtk_box_append(GTK_BOX(box), row);
	}
	static const char * s_Types[] =
		{ "book", "journal", "article", "website" };
	GtkWidget * ddType = _ref_dropdown_row("Type:", s_Types, 4);
	g_object_set_data(G_OBJECT(btn), "dd-type",
					  g_object_get_data(G_OBJECT(ddType), "dd"));
	gtk_box_append(GTK_BOX(box), ddType);
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	g_signal_connect(btn, "clicked",
					 G_CALLBACK(_s_citation_apply), this);
	gtk_box_append(GTK_BOX(box), btn);
	return popover;
}

/* Word's Bibliography gallery: pick the citation style */
GtkWidget * AP_UnixRibbon::_makeBibliographyPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box), _popover_section_label("Built-In"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("APA",
							  "Author (Year). Title. Publisher.",
							  nullptr, "refInsertBibliography", "apa"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("MLA",
							  "Author. Title. Publisher, Year.",
							  nullptr, "refInsertBibliography", "mla"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("Chicago",
							  "Author. Year. Title. Publisher.",
							  nullptr, "refInsertBibliography", "chicago"));
	gtk_box_append(GTK_BOX(box),
				   _presetRow("IEEE",
							  "Author, \"Title,\" Publisher, Year.",
							  nullptr, "refInsertBibliography", "ieee"));
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget * remove = _presetRow("Remove Bibliography",
									"Delete the bibliography",
									nullptr, "refRemoveBibliography", nullptr);
	gtk_box_append(GTK_BOX(box), remove);
	g_object_set_data(G_OBJECT(popover), "abi-bib-remove", remove);
	g_signal_connect(popover, "map",
					 G_CALLBACK(_s_biblio_map), this);
	return popover;
}

void AP_UnixRibbon::_s_biblio_map(GtkWidget * popover, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	GtkWidget * btn = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(popover), "abi-bib-remove"));
	if (!btn)
		return;
	FV_View * pView = static_cast<FV_View *>(
		self->m_pFrame ? self->m_pFrame->getCurrentView() : nullptr);
	gtk_widget_set_sensitive(btn,
							 pView && pView->hasRefSection("_genbib"));
}

/* Word's Manage Sources: the stored source list with per-source
 * delete, rebuilt each time the popover opens */
void AP_UnixRibbon::_s_sources_map(GtkWidget * popover, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	GtkWidget * listbox = static_cast<GtkWidget *>(
		g_object_get_data(G_OBJECT(popover), "sources-list"));
	UT_return_if_fail(listbox);

	for (GtkWidget * child = gtk_widget_get_first_child(listbox);
		 child;)
	{
		GtkWidget * next = gtk_widget_get_next_sibling(child);
		gtk_list_box_remove(GTK_LIST_BOX(listbox), child);
		child = next;
	}

	FV_View * pView = static_cast<FV_View *>(
		self->m_pFrame ? self->m_pFrame->getCurrentView() : nullptr);
	std::vector<FV_BibSource> sources;
	if (pView)
		pView->getBibSources(sources);
	for (const FV_BibSource & s : sources)
	{
		GtkWidget * row = gtk_list_box_row_new();
		GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		const std::string sLabel = UT_UTF8String_sprintf(
			"%s (%s). %s", s.author.c_str(), s.year.c_str(),
			s.title.c_str()).utf8_str();
		GtkWidget * l = gtk_label_new(sLabel.c_str());
		gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
		gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars(GTK_LABEL(l), 30);
		gtk_widget_set_hexpand(l, TRUE);
		gtk_box_append(GTK_BOX(hbox), l);
		GtkWidget * del = gtk_button_new_with_label("Delete");
		gtk_widget_add_css_class(del, "flat");
		g_object_set_data_full(G_OBJECT(del), "abi-em-method",
							   g_strdup("refDeleteSource"), g_free);
		char buf[16];
		snprintf(buf, sizeof(buf), "%u", s.n);
		g_object_set_data_full(G_OBJECT(del), "abi-em-data",
							   g_strdup(buf), g_free);
		g_signal_connect(del, "clicked",
						 G_CALLBACK(_s_popover_em_clicked), self);
		gtk_box_append(GTK_BOX(hbox), del);
		gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);
		gtk_list_box_append(GTK_LIST_BOX(listbox), row);
	}
	if (sources.empty())
	{
		GtkWidget * row = gtk_list_box_row_new();
		GtkWidget * l = gtk_label_new("(no sources yet)");
		gtk_widget_set_sensitive(row, FALSE);
		gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), l);
		gtk_list_box_append(GTK_LIST_BOX(listbox), row);
	}
}

GtkWidget * AP_UnixRibbon::_makeSourcesPopover()
{
	GtkWidget * box;
	GtkWidget * popover = _popover_new_box(&box);

	gtk_box_append(GTK_BOX(box), _popover_section_label("Sources"));
	GtkWidget * sw = gtk_scrolled_window_new();
	gtk_widget_set_size_request(sw, 280, 160);
	GtkWidget * listbox = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(listbox),
									GTK_SELECTION_NONE);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), listbox);
	gtk_box_append(GTK_BOX(box), sw);

	g_object_set_data(G_OBJECT(popover), "sources-list", listbox);
	g_signal_connect(popover, "map", G_CALLBACK(_s_sources_map), this);
	return popover;
}

/* -------- Indent / Spacing spin fields -------- */

struct _SpinCtx
{
	AP_UnixRibbon * self;
	const char *  prop;	/* block property name (static storage) */
	UT_Dimension  unit;	/* display unit */
	guint		  idleId;
};

static void _spin_ctx_free(gpointer p)
{
	_SpinCtx * c = static_cast<_SpinCtx *>(p);
	if (c->idleId)
		g_source_remove(c->idleId);
	delete c;
}

gboolean AP_UnixRibbon::_s_spin_apply(gpointer data)
{
	GtkWidget * spin = GTK_WIDGET(data);
	_SpinCtx * c = static_cast<_SpinCtx *>(
		g_object_get_data(G_OBJECT(spin), "spin-ctx"));
	UT_return_val_if_fail(c, G_SOURCE_REMOVE);
	c->idleId = 0;
	if (c->self->m_bSpinUpdating)
		return G_SOURCE_REMOVE;

	double v = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
	char buf[128];
	snprintf(buf, sizeof(buf), "%s:%.2f%s", c->prop, v,
			 UT_dimensionName(c->unit));
	c->self->_invokeEditMethod("paraProp", buf);
	return G_SOURCE_REMOVE;
}

void AP_UnixRibbon::_s_spin_changed(GtkSpinButton * spin, gpointer /*data*/)
{
	_SpinCtx * c = static_cast<_SpinCtx *>(
		g_object_get_data(G_OBJECT(spin), "spin-ctx"));
	UT_return_if_fail(c);
	if (c->self->m_bSpinUpdating)
		return;
	/* debounce so typing "12.5" doesn't reformat per keystroke */
	if (c->idleId)
		g_source_remove(c->idleId);
	c->idleId = g_timeout_add(350, _s_spin_apply, spin);
}

/* spacing glyph for the Before/After spin rows: three text lines
 * with a small arrow on the padded edge (up = Before, down = After) */
static void _s_spacing_icon_draw(GtkDrawingArea * /*area*/, cairo_t * cr,
								 int w, int h, gpointer data)
{
	bool bUp = GPOINTER_TO_INT(data) != 0;
	double top = bUp ? 5.0 : 1.0;
	cairo_set_source_rgb(cr, 0.35, 0.35, 0.35);
	cairo_set_line_width(cr, 1.2);
	for (int i = 0; i < 3; ++i)
	{
		double y = top + 1.0 + i * 3.6;
		cairo_move_to(cr, 1.5, y);
		cairo_line_to(cr, w - 1.5, y);
	}
	cairo_stroke(cr);
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_set_line_width(cr, 1.2);
	double stem0 = bUp ? 4.6 : h - 4.6;
	double tip  = bUp ? 0.8 : h - 0.8;
	cairo_move_to(cr, w * 0.5, stem0);
	cairo_line_to(cr, w * 0.5, tip);
	cairo_stroke(cr);
	cairo_move_to(cr, w * 0.5, tip);
	cairo_line_to(cr, w * 0.5 - 2.2, tip + (bUp ? 3.0 : -3.0));
	cairo_line_to(cr, w * 0.5 + 2.2, tip + (bUp ? 3.0 : -3.0));
	cairo_close_path(cr);
	cairo_fill(cr);
}

GtkWidget * AP_UnixRibbon::_makeSpinField(int spinId)
{
	const char * prop;
	const char * label;
	UT_Dimension unit;
	switch (spinId)
	{
	case AP_RIBBON_SPIN_INDENT_LEFT:
		prop = "margin-left";  label = "Left:";   unit = _ruler_units();
		break;
	case AP_RIBBON_SPIN_INDENT_RIGHT:
		prop = "margin-right"; label = "Right:";  unit = _ruler_units();
		break;
	case AP_RIBBON_SPIN_BEFORE:
		prop = "margin-top";    label = "Before:"; unit = DIM_PT;
		break;
	case AP_RIBBON_SPIN_AFTER:
		prop = "margin-bottom"; label = "After:";  unit = DIM_PT;
		break;
	default:
		return nullptr;
	}

	_SpinCtx * c = new _SpinCtx{ this, prop, unit, 0 };

	GtkWidget * row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_valign(row, GTK_ALIGN_CENTER);
	GtkWidget * icon;
	if (spinId == AP_RIBBON_SPIN_BEFORE || spinId == AP_RIBBON_SPIN_AFTER)
	{
		icon = gtk_drawing_area_new();
		gtk_widget_set_size_request(icon, 16, 16);
		gtk_drawing_area_set_draw_func(
			GTK_DRAWING_AREA(icon), _s_spacing_icon_draw,
			GINT_TO_POINTER(spinId == AP_RIBBON_SPIN_BEFORE), nullptr);
	}
	else
	{
		icon = gtk_image_new_from_icon_name(
			spinId == AP_RIBBON_SPIN_INDENT_LEFT
				? "format-indent-more-symbolic"
				: "format-indent-less-symbolic");
	}
	gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(row), icon);
	/* fixed label width so the spin entries line up in a column
	 * ("Before:" is the widest label) */
	GtkWidget * lbl = gtk_label_new(label);
	gtk_label_set_width_chars(GTK_LABEL(lbl), 7);
	gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
	gtk_box_append(GTK_BOX(row), lbl);

	double max = (unit == DIM_PT) ? 1584.0 : 30.0; /* 22in in pt / 30cm|in */
	GtkWidget * spin = gtk_spin_button_new_with_range(-100.0, max,
													  unit == DIM_PT ? 1.0 : 0.05);
	gtk_widget_add_css_class(spin, "ribbon-spin");
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin),
							   unit == DIM_PT ? 0 : 2);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), FALSE); /* locale ',' ok */
	gtk_editable_set_width_chars(GTK_EDITABLE(spin), 5);
	g_object_set_data_full(G_OBJECT(spin), "spin-ctx", c, _spin_ctx_free);
	g_signal_connect(spin, "value-changed",
					 G_CALLBACK(_s_spin_changed), this);
	gtk_box_append(GTK_BOX(row), spin);
	gtk_box_append(GTK_BOX(row),
				   gtk_label_new(unit == DIM_PT ? "pt" : UT_dimensionName(unit)));

	_SpinField * f = new _SpinField{ spin, prop };
	m_vecSpins.addItem(f);
	return row;
}

/* sync the indent/spacing spins with the block under the caret */
void AP_UnixRibbon::_refreshSpinFields()
{
	if (m_vecSpins.getItemCount() == 0)
		return;
	FV_View * pView = static_cast<FV_View *>(
		m_pFrame ? m_pFrame->getCurrentView() : nullptr);

	PP_PropertyVector props;
	bool ok = pView && pView->getBlockFormat(props);

	m_bSpinUpdating = true;
	for (UT_sint32 i = 0; i < m_vecSpins.getItemCount(); ++i)
	{
		_SpinField * f = m_vecSpins.getNthItem(i);
		_SpinCtx * c = static_cast<_SpinCtx *>(
			g_object_get_data(G_OBJECT(f->spin), "spin-ctx"));
		double v = 0.0;
		if (ok)
		{
			const std::string & s = PP_getAttribute(f->prop, props);
			if (!s.empty())
				v = UT_convertToDimension(s.c_str(), c->unit);
		}
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(f->spin), v);
		gtk_widget_set_sensitive(f->spin, ok);
	}
	m_bSpinUpdating = false;
}

/* line-spacing dropdown: single/1.5/double spacing */
GtkWidget * AP_UnixRibbon::_makeLineSpacingPopover()
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);
	gtk_widget_set_margin_start(box, 4);
	gtk_widget_set_margin_end(box, 4);

	GtkWidget * w = _popoverTbButton(
		(XAP_Toolbar_Id)AP_TOOLBAR_ID_SINGLE_SPACE, "Single");
	if (w) gtk_box_append(GTK_BOX(box), w);
	w = _popoverTbButton(
		(XAP_Toolbar_Id)AP_TOOLBAR_ID_MIDDLE_SPACE, "1.5 Lines");
	if (w) gtk_box_append(GTK_BOX(box), w);
	w = _popoverTbButton(
		(XAP_Toolbar_Id)AP_TOOLBAR_ID_DOUBLE_SPACE, "Double");
	if (w) gtk_box_append(GTK_BOX(box), w);

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* paragraph-spacing dropdown: space before paragraph */
GtkWidget * AP_UnixRibbon::_makeParaSpacingPopover()
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);
	gtk_widget_set_margin_start(box, 4);
	gtk_widget_set_margin_end(box, 4);

	GtkWidget * w = _popoverTbButton(
		(XAP_Toolbar_Id)AP_TOOLBAR_ID_PARA_0BEFORE,
		"No Spacing Above Paragraph");
	if (w) gtk_box_append(GTK_BOX(box), w);
	w = _popoverTbButton(
		(XAP_Toolbar_Id)AP_TOOLBAR_ID_PARA_12BEFORE,
		"12pt Spacing Above Paragraph");
	if (w) gtk_box_append(GTK_BOX(box), w);

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* paragraph sort dropdown: ascending/descending */
GtkWidget * AP_UnixRibbon::_makeSortParaPopover()
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);
	gtk_widget_set_margin_start(box, 4);
	gtk_widget_set_margin_end(box, 4);

	GtkWidget * w = _popoverEmButton("Sort Ascending (A-Z)", nullptr,
								   "paraSortAscend");
	if (w) gtk_box_append(GTK_BOX(box), w);
	w = _popoverEmButton("Sort Descending (Z-A)", nullptr,
					   "paraSortDescend");
	if (w) gtk_box_append(GTK_BOX(box), w);

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* miniature border-diagram icon for the Borders menu rows.
 * data bitmask: 1 top, 2 bottom, 4 left, 8 right edges (solid lines
 * over a dashed frame); 16 inside-horizontal, 32 inside-vertical,
 * 64 diagonal-down, 128 diagonal-up midlines. */
static void _s_border_icon_draw(GtkDrawingArea * /*area*/, cairo_t * cr,
								int width, int height, gpointer data)
{
	int edges = GPOINTER_TO_INT(data);
	double x0 = 2.5, y0 = 2.5, x1 = width - 2.5, y1 = height - 2.5;

	cairo_set_source_rgba(cr, 0, 0, 0, 0.30);
	cairo_set_line_width(cr, 1.0);
	const double dashes[2] = {2.0, 2.0};
	cairo_set_dash(cr, dashes, 2, 0);
	cairo_rectangle(cr, x0, y0, x1 - x0, y1 - y0);
	cairo_stroke(cr);

	cairo_set_dash(cr, nullptr, 0, 0);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_set_line_width(cr, 2.0);
	if (edges & 1)	/* top */
	{
		cairo_move_to(cr, x0 - 1, y0);
		cairo_line_to(cr, x1 + 1, y0);
	}
	if (edges & 2)	/* bottom */
	{
		cairo_move_to(cr, x0 - 1, y1);
		cairo_line_to(cr, x1 + 1, y1);
	}
	if (edges & 4)	/* left */
	{
		cairo_move_to(cr, x0, y0 - 1);
		cairo_line_to(cr, x0, y1 + 1);
	}
	if (edges & 8)	/* right */
	{
		cairo_move_to(cr, x1, y0 - 1);
		cairo_line_to(cr, x1, y1 + 1);
	}
	cairo_set_line_width(cr, 1.5);
	if (edges & 16)	/* inside horizontal */
	{
		cairo_move_to(cr, x0, (y0 + y1) / 2);
		cairo_line_to(cr, x1, (y0 + y1) / 2);
	}
	if (edges & 32)	/* inside vertical */
	{
		cairo_move_to(cr, (x0 + x1) / 2, y0);
		cairo_line_to(cr, (x0 + x1) / 2, y1);
	}
	if (edges & 64)	/* diagonal down */
	{
		cairo_move_to(cr, x0, y0);
		cairo_line_to(cr, x1, y1);
	}
	if (edges & 128)	/* diagonal up */
	{
		cairo_move_to(cr, x0, y1);
		cairo_line_to(cr, x1, y0);
	}
	cairo_stroke(cr);
}

static GtkWidget * _border_icon(int edges)
{
	GtkWidget * da = gtk_drawing_area_new();
	gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(da), 16);
	gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(da), 16);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(da),
								   _s_border_icon_draw,
								   GINT_TO_POINTER(edges), nullptr);
	return da;
}

/* one row of the Borders dropdown: drawn edge-diagram + label,
 * wired to an edit method (with optional data argument) */
GtkWidget * AP_UnixRibbon::_borderRow(int edges, const char * szLabel,
									  const char * szMethod,
									  const char * szData,
									  bool bSensitive)
{
	GtkWidget * btn = gtk_button_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_append(GTK_BOX(box), _border_icon(edges));
	GtkWidget * wLabel = gtk_label_new(szLabel);
	gtk_widget_set_halign(wLabel, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(box), wLabel);
	gtk_button_set_child(GTK_BUTTON(btn), box);
	gtk_widget_add_css_class(btn, "flat");
	if (!bSensitive)
	{
		gtk_widget_set_sensitive(btn, FALSE);
		return btn;
	}
	g_object_set_data_full(G_OBJECT(btn), "abi-em-method",
						   g_strdup(szMethod), g_free);
	if (szData)
		g_object_set_data_full(G_OBJECT(btn), "abi-em-data",
							   g_strdup(szData), g_free);
	g_signal_connect(btn, "clicked",
					 G_CALLBACK(_s_popover_em_clicked), this);
	return btn;
}

/* Word-style Borders dropdown: quick edge presets plus the
 * Borders and Shading dialog entry */
GtkWidget * AP_UnixRibbon::_makeBordersPopover()
{
	GtkWidget * popover = xap_gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);
	gtk_widget_set_margin_start(box, 4);
	gtk_widget_set_margin_end(box, 4);

	auto add = [&](GtkWidget * w) {
		if (w) gtk_box_append(GTK_BOX(box), w);
	};
	auto sep = [&]() {
		add(gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	};

	add(_borderRow(2, "Bottom Border",  "paraBorder", "bottom"));
	add(_borderRow(1, "Top Border",     "paraBorder", "top"));
	add(_borderRow(4, "Left Border",    "paraBorder", "left"));
	add(_borderRow(8, "Right Border",   "paraBorder", "right"));
	sep();
	add(_borderRow(0,   "No Border",       "paraBorder", "none"));
	add(_borderRow(15,  "All Borders",     "paraBorder", "all"));
	add(_borderRow(15,  "Outside Borders", "paraBorder", "outside"));
	add(_borderRow(16,  "Inside Borders",  "paraBorder", "inside"));
	sep();
	add(_borderRow(16, "Inside Horizontal Border", "paraBorder",
				   "insideh"));
	add(_borderRow(32, "Inside Vertical Border", nullptr, nullptr,
				   false));
	sep();
	add(_borderRow(64,  "Diagonal Down Border", nullptr, nullptr, false));
	add(_borderRow(128, "Diagonal Up Border",   nullptr, nullptr, false));
	sep();
	add(_borderRow(2, "Horizontal Line", "paraBorder", "hline"));
	add(_borderRow(0, "Draw Table", "insertTable", nullptr));
	add(_borderRow(0, "View Gridlines", nullptr, nullptr, false));
	sep();
	add(_borderRow(15, "Borders and Shading\xE2\x80\xA6",
				   "dlgBorders", nullptr));

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
}

/* icon-only toolbar menu-button: one click opens the matching
 * dropdown popover (line spacing, paragraph spacing, sort) */
GtkWidget * AP_UnixRibbon::_makeMenuPopTbButton(XAP_Toolbar_Id id,
												uint8_t flags)
{
	GtkWidget * popover = nullptr;
	switch (id)
	{
	case (XAP_Toolbar_Id)AP_TOOLBAR_ID_SINGLE_SPACE:
		popover = _makeLineSpacingPopover();
		break;
	case (XAP_Toolbar_Id)AP_TOOLBAR_ID_PARA_0BEFORE:
		popover = _makeParaSpacingPopover();
		break;
	case (XAP_Toolbar_Id)AP_TOOLBAR_ID_SORT_PARA:
		popover = _makeSortParaPopover();
		break;
	default:
		break;
	}
	if (!popover)
		return nullptr;

	EV_Toolbar_Label * pLabel =
		m_pTBLabels ? m_pTBLabels->getLabel(id) : nullptr;

	GtkWidget * mb = gtk_menu_button_new();
	const char * szIcon = pLabel ? pLabel->getIconName() : nullptr;
	if (szIcon && g_ascii_strcasecmp(szIcon, "NoIcon") != 0)
	{
		gchar * szTheme = abi_stock_from_toolbar_id(szIcon);
		gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(mb), szTheme);
		g_free(szTheme);
	}
	if (flags & AP_RIBBON_FLAG_SLIM)
		_slim_widget_tree(mb);
	gtk_menu_button_set_direction(GTK_MENU_BUTTON(mb), GTK_ARROW_NONE);
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(mb), popover);

	const char * szTip = pLabel ? pLabel->getToolTip() : nullptr;
	if (szTip && *szTip)
		gtk_widget_set_tooltip_text(mb, szTip);
	return mb;
}

/* attach a small drop-arrow menu button beside (or below) a button */
GtkWidget * AP_UnixRibbon::_wrapSplit(GtkWidget * w, GtkWidget * popover,
									  bool bVertical)
{
	GtkWidget * arrow = gtk_menu_button_new();
	gtk_menu_button_set_direction(GTK_MENU_BUTTON(arrow),
								  GTK_ARROW_DOWN);
	gtk_menu_button_set_has_frame(GTK_MENU_BUTTON(arrow), FALSE);
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(arrow), popover);
	/* slim the drop-arrow: zero padding on the button and its
	 * internal children so it is just a narrow wedge */
	_slim_widget_tree(arrow);
	gtk_widget_set_size_request(arrow, 12, -1);

	GtkWidget * box = gtk_box_new(
		bVertical ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(box, "linked");
	if (bVertical)
	{
		gtk_widget_set_vexpand(w, TRUE);
		gtk_widget_set_halign(arrow, GTK_ALIGN_FILL);
	}
	gtk_box_append(GTK_BOX(box), w);
	gtk_box_append(GTK_BOX(box), arrow);
	return box;
}

GtkWidget * AP_UnixRibbon::_tb_make_combo(_TbCtx * ctx)
{
	GtkWidget * combo = nullptr;
	switch (ctx->id)
	{
	case AP_TOOLBAR_ID_FMT_FONT:
	{
		combo = abi_font_combo_new();
		gtk_widget_set_name(combo, "AbiFontCombo");
		const std::vector<std::string> & fonts =
			GR_CairoGraphics::getAllFontNames();
		const gchar ** list = g_new0(const gchar *, fonts.size() + 1);
		for (size_t i = 0; i < fonts.size(); ++i)
			list[i] = fonts[i].c_str();
		abi_font_combo_set_fonts(ABI_FONT_COMBO(combo), list);
		g_free(list);
		break;
	}
	case AP_TOOLBAR_ID_FMT_SIZE:
	{
		combo = gtk_combo_box_text_new_with_entry();
		GtkEntry * entry =
			GTK_ENTRY(gtk_combo_box_get_child(GTK_COMBO_BOX(combo)));
		gtk_widget_set_can_focus(GTK_WIDGET(entry), TRUE);
		gtk_editable_set_width_chars(GTK_EDITABLE(entry), 2);
		gtk_editable_set_max_width_chars(GTK_EDITABLE(entry), 5);
		gtk_editable_set_alignment(GTK_EDITABLE(entry), 0.5f);
		/* slim the combo's entry and internal dropdown button to
		 * match the glyph buttons beside it */
		_slim_widget_tree(combo);
		g_signal_connect(G_OBJECT(entry), "insert-text",
						 G_CALLBACK(_s_tb_size_insert_text), nullptr);
		GtkEventController * focus = gtk_event_controller_focus_new();
		g_signal_connect(G_OBJECT(focus), "leave",
						 G_CALLBACK(_s_tb_size_focus_out), ctx);
		gtk_widget_add_controller(GTK_WIDGET(entry), focus);
		GtkEventController * key = gtk_event_controller_key_new();
		g_signal_connect(G_OBJECT(key), "key-pressed",
						 G_CALLBACK(_s_tb_size_key), ctx);
		gtk_widget_add_controller(GTK_WIDGET(entry), key);

		int n = XAP_EncodingManager::fontsizes_mapping.size();
		for (int i = 0; i < n; ++i)
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
				XAP_EncodingManager::fontsizes_mapping.nth2(i));
		break;
	}
	case AP_TOOLBAR_ID_FMT_STYLE:
	{
		combo = gtk_combo_box_text_new();
		gtk_widget_set_name(combo, "AbiStyleCombo");
		/* same hardwired list the classic toolbar seeds with; the
		 * refresh path appends the caret's actual style if missing */
		static const char * styles[] =
		{
			"Normal", "Heading 1", "Heading 2", "Heading 3",
			"Plain Text", "Block Text"
		};
		for (size_t i = 0; i < G_N_ELEMENTS(styles); ++i)
		{
			std::string sLoc;
			pt_PieceTable::s_getLocalisedStyleName(styles[i], sLoc);
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
										   sLoc.c_str());
		}
		break;
	}
	case AP_TOOLBAR_ID_ZOOM:
	{
		combo = gtk_combo_box_text_new();
		gtk_widget_set_name(combo, "AbiZoomCombo");
		static const char * zooms[] =
			{ "200%", "150%", "100%", "75%", "50%", "25%" };
		for (size_t i = 0; i < G_N_ELEMENTS(zooms); ++i)
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
										   zooms[i]);
		const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
		if (pSS)
		{
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
				pSS->getValue(XAP_STRING_ID_TB_Zoom_PageWidth));
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
				pSS->getValue(XAP_STRING_ID_TB_Zoom_WholePage));
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
				pSS->getValue(XAP_STRING_ID_TB_Zoom_Percent));
		}
		break;
	}
	default:
		break;
	}
	return combo;
}

GtkWidget * AP_UnixRibbon::_makeToolbarWidget(XAP_Toolbar_Id id,
											  uint8_t flags)
{
	const EV_Toolbar_ActionSet * pTBActions =
		XAP_App::getApp()->getToolbarActionSet();
	UT_return_val_if_fail(pTBActions, nullptr);

	EV_Toolbar_Action * pAction = pTBActions->getAction(id);
	EV_Toolbar_Label * pLabel =
		m_pTBLabels ? m_pTBLabels->getLabel(id) : nullptr;
	if (!pAction || !pLabel)
		return nullptr;

	_TbCtx * ctx = new _TbCtx;
	ctx->self = this;
	ctx->id = id;
	ctx->widget = nullptr;
	ctx->blockSignal = false;
	ctx->handlerId = 0;

	GtkWidget * w = nullptr;
	const char * szIcon = pLabel->getIconName();
	gchar * szThemeIcon =
		(szIcon && g_ascii_strcasecmp(szIcon, "NoIcon") != 0)
		? abi_stock_from_toolbar_id(szIcon) : nullptr;

	switch (pAction->getItemType())
	{
	case EV_TBIT_PushButton:
	case EV_TBIT_ToggleButton:
	case EV_TBIT_GroupButton:
	{
		bool bToggle = (pAction->getItemType() != EV_TBIT_PushButton);
		w = bToggle ? gtk_toggle_button_new() : gtk_button_new();
		const char * szTip = pLabel->getToolTip();
		if (szThemeIcon && (flags & AP_RIBBON_FLAG_LARGE) &&
			szTip && *szTip)
		{
			/* Word-style large button: big icon, caption beneath it */
			GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
			GtkWidget * image = gtk_image_new_from_icon_name(szThemeIcon);
			gtk_image_set_icon_size(GTK_IMAGE(image), GTK_ICON_SIZE_LARGE);
			GtkWidget * caption = gtk_label_new(szTip);
			gtk_label_set_ellipsize(GTK_LABEL(caption), PANGO_ELLIPSIZE_END);
			gtk_label_set_max_width_chars(GTK_LABEL(caption), 14);
			gtk_box_append(GTK_BOX(vbox), image);
			gtk_box_append(GTK_BOX(vbox), caption);
			gtk_button_set_child(GTK_BUTTON(w), vbox);
		}
		else if (szThemeIcon)
		{
			if (bToggle)
			{
				GtkWidget * image =
					gtk_image_new_from_icon_name(szThemeIcon);
				gtk_button_set_child(GTK_BUTTON(w), image);
			}
			else
				gtk_button_set_icon_name(GTK_BUTTON(w), szThemeIcon);
		}
		ctx->handlerId = g_signal_connect(w, "clicked",
										  G_CALLBACK(_s_tb_clicked), ctx);
		break;
	}

	case EV_TBIT_ComboBox:
		w = _tb_make_combo(ctx);
		if (w)
			ctx->handlerId = g_signal_connect(w, "changed",
											  G_CALLBACK(_s_tb_combo_changed),
											  ctx);
		break;

	case EV_TBIT_ColorFore:
	case EV_TBIT_ColorBack:
	{
		const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
		std::string sClear;
		if (pSS)
			pSS->getValueUTF8(
				pAction->getItemType() == EV_TBIT_ColorFore
					? XAP_STRING_ID_TB_ClearForeground
					: XAP_STRING_ID_TB_ClearBackground, sClear);
		/* LO-style glyph: font colour is a bold "A" with a colour
		 * bar, highlight is "ab" on a yellow swatch */
		const char * szGlyph =
			(pAction->getItemType() == EV_TBIT_ColorFore)
			? "<span font_weight='bold' underline='single' "
			  "underline_color='#d01c11'>A</span>"
			: "<span font_weight='bold' "
			  "bgcolor='#fff59d'>ab</span>";
		w = _tb_color_button_new(
			szThemeIcon ? szThemeIcon : "preferences-color-symbolic",
			sClear.c_str(), ctx, szGlyph);
		break;
	}

	default:
		break;
	}

	if (!w)
	{
		delete ctx;
		return nullptr;
	}

	const char * szToolTip = pLabel->getToolTip();
	if (szToolTip && *szToolTip)
		gtk_widget_set_tooltip_text(w, szToolTip);
	g_free(szThemeIcon);

	gtk_widget_set_hexpand(w, FALSE);
	gtk_widget_set_valign(w, GTK_ALIGN_CENTER);
	gtk_widget_set_visible(w, TRUE);

	ctx->widget = w;
	m_vecTbCtx.addItem(ctx);
	return w;
}

/*
 * Word-style Styles gallery: a horizontal strip of preview tiles, one
 * per paragraph style in the document.  Each tile's caption is the
 * (localized) style name rendered in an approximation of the style
 * itself - family, weight, slant, underline and color come from the
 * resolved style properties.  Clicking a tile applies the style
 * through the same edit method the style combo uses.
 */
GtkWidget * AP_UnixRibbon::_makeStyleGallery()
{
	/* a menu rebuild recreates this widget; drop the stale tile
	 * records, whose widgets belong to the previous instance */
	for (UT_sint32 i = 0; i < m_vecStyleTiles.getItemCount(); ++i)
	{
		_StyleTile * t = m_vecStyleTiles.getNthItem(i);
		g_free(t->styleName);
		delete t;
	}
	m_vecStyleTiles.clear();

	/* LibreOffice-style strip: [<] tiles [>] [Styles Pane] */
	GtkWidget * outer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	GtkWidget * scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
								   GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	/* show tiles at natural width, scroll when the doc has more
	 * styles than fit beside the other Home groups */
	gtk_scrolled_window_set_propagate_natural_width(
		GTK_SCROLLED_WINDOW(scroll), TRUE);
	gtk_scrolled_window_set_min_content_width(
		GTK_SCROLLED_WINDOW(scroll), 240);
	gtk_scrolled_window_set_max_content_width(
		GTK_SCROLLED_WINDOW(scroll), 640);
	m_wStyleBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_widget_set_valign(m_wStyleBox, GTK_ALIGN_FILL);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
								  m_wStyleBox);
	m_wStyleScroll = scroll;

	m_wStylePrev = gtk_button_new_from_icon_name(
		"go-previous-symbolic");
	gtk_widget_add_css_class(m_wStylePrev, "flat");
	gtk_widget_add_css_class(m_wStylePrev, "ribbon-nav");
	gtk_widget_set_size_request(m_wStylePrev, 16, -1);
	gtk_widget_set_valign(m_wStylePrev, GTK_ALIGN_FILL);
	g_signal_connect(m_wStylePrev, "clicked",
					 G_CALLBACK(_s_style_scroll_clicked), this);
	g_object_set_data(G_OBJECT(m_wStylePrev), "abi-dir",
					  GINT_TO_POINTER(-1));

	m_wStyleNext = gtk_button_new_from_icon_name(
		"go-next-symbolic");
	gtk_widget_add_css_class(m_wStyleNext, "flat");
	gtk_widget_add_css_class(m_wStyleNext, "ribbon-nav");
	gtk_widget_set_size_request(m_wStyleNext, 16, -1);
	gtk_widget_set_valign(m_wStyleNext, GTK_ALIGN_FILL);
	g_signal_connect(m_wStyleNext, "clicked",
					 G_CALLBACK(_s_style_scroll_clicked), this);
	g_object_set_data(G_OBJECT(m_wStyleNext), "abi-dir",
					  GINT_TO_POINTER(1));

	GtkAdjustment * hadj = gtk_scrolled_window_get_hadjustment(
		GTK_SCROLLED_WINDOW(scroll));
	g_signal_connect_swapped(hadj, "changed",
							 G_CALLBACK(_updateStyleScrollButtons),
							 this);
	g_signal_connect_swapped(hadj, "value-changed",
							 G_CALLBACK(_updateStyleScrollButtons),
							 this);

	/* Styles Pane button: pencil icon over two-line label */
	GtkWidget * paneBtn = gtk_button_new();
	GtkWidget * pbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_valign(pbox, GTK_ALIGN_CENTER);
	GtkWidget * picon = gtk_image_new_from_icon_name(
		"accessories-text-editor-symbolic");
	gtk_image_set_pixel_size(GTK_IMAGE(picon), 20);
	gtk_box_append(GTK_BOX(pbox), picon);
	GtkWidget * plbl = gtk_label_new("Styles\nPane");
	gtk_label_set_justify(GTK_LABEL(plbl), GTK_JUSTIFY_CENTER);
	gtk_box_append(GTK_BOX(pbox), plbl);
	gtk_button_set_child(GTK_BUTTON(paneBtn), pbox);
	gtk_widget_set_tooltip_text(paneBtn,
								"Show the Styles deck");
	g_signal_connect(paneBtn, "clicked",
					 G_CALLBACK(_s_styles_pane_clicked), this);

	gtk_box_append(GTK_BOX(outer), m_wStylePrev);
	gtk_box_append(GTK_BOX(outer), scroll);
	gtk_box_append(GTK_BOX(outer), m_wStyleNext);
	gtk_box_append(GTK_BOX(outer), gtk_separator_new(
		GTK_ORIENTATION_VERTICAL));
	gtk_box_append(GTK_BOX(outer), paneBtn);

	gtk_widget_set_visible(scroll, FALSE);
	gtk_widget_set_visible(m_wStylePrev, FALSE);
	gtk_widget_set_visible(m_wStyleNext, FALSE);
	_populateStyleTiles();
	_updateStyleScrollButtons(this);
	return outer;
}

void AP_UnixRibbon::_s_style_scroll_clicked(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self && self->m_wStyleScroll);
	GtkAdjustment * hadj = gtk_scrolled_window_get_hadjustment(
		GTK_SCROLLED_WINDOW(self->m_wStyleScroll));
	int dir = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "abi-dir"));
	double step = gtk_adjustment_get_page_size(hadj);
	if (step <= 0)
		step = 240;
	gtk_adjustment_set_value(hadj,
		gtk_adjustment_get_value(hadj) + dir * step);
}

void AP_UnixRibbon::_updateStyleScrollButtons(AP_UnixRibbon * self)
{
	if (!self || !self->m_wStyleScroll)
		return;
	GtkAdjustment * hadj = gtk_scrolled_window_get_hadjustment(
		GTK_SCROLLED_WINDOW(self->m_wStyleScroll));
	double value = gtk_adjustment_get_value(hadj);
	double lower = gtk_adjustment_get_lower(hadj);
	double upper = gtk_adjustment_get_upper(hadj);
	double page = gtk_adjustment_get_page_size(hadj);
	bool bTiles = self->m_vecStyleTiles.getItemCount() > 0;
	gtk_widget_set_visible(self->m_wStylePrev,
						   bTiles && value > lower + 0.5);
	gtk_widget_set_visible(self->m_wStyleNext,
						   bTiles && value < upper - page - 0.5);
}

void AP_UnixRibbon::_s_styles_pane_clicked(GtkWidget * /*w*/,
										   gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self && self->m_pFrame);
	AP_UnixFrameImpl * pImpl = static_cast<AP_UnixFrameImpl *>(
		self->m_pFrame->getFrameImpl());
	if (pImpl)
		pImpl->setStylesPaneVisible(!pImpl->isStylesPaneVisible());
}

/* (re)build the preview tiles - called lazily because the ribbon is
 * constructed before the frame's view/document exist */
void AP_UnixRibbon::_populateStyleTiles()
{
	if (m_vecStyleTiles.getItemCount() > 0 || !m_wStyleBox)
		return;

	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	PD_Document * pdoc = pView ? pView->getDocument() : nullptr;
	if (!pdoc)
		return;

	GtkWidget * box = m_wStyleBox;
	UT_sint32 nTiles = 0;
	{
		/* Word-style gallery order - listed styles first, in this
		 * sequence, then any other displayed style alphabetically */
		static const char * s_galleryOrder[] = {
			"Normal", "No Spacing",
			"Heading 1", "Heading 2", "Heading 3",
			"Title", "Subtitle",
			"Subtle Emphasis", "Emphasis",
			"Intense Emphasis", "Strong",
			"Quote", "Intense Quote",
			"Subtle Reference", "Intense Reference",
			"Book Title", "List Paragraph",
		};

		std::vector<std::pair<std::string, const PD_Style*>> styles;
		for (UT_uint32 k = 0;; ++k)
		{
			const char * szName = nullptr;
			const PD_Style * pStyle = nullptr;
			if (!pdoc->enumStyles(k, &szName, &pStyle))
				break;
			if (!pStyle || !szName || !*szName || !pStyle->isDisplayed() ||
				AP_UnixStylesPane::isListPseudoStyle(szName))
				continue;
			styles.push_back({szName, pStyle});
		}

		std::sort(styles.begin(), styles.end(),
				  [](const auto& a, const auto& b)
		{
			auto orderOf = [](const std::string& s) -> int
			{
				for (size_t i = 0; i < G_N_ELEMENTS(s_galleryOrder); ++i)
					if (s == s_galleryOrder[i])
						return (int)i;
				return 1000;
			};
			int oa = orderOf(a.first), ob = orderOf(b.first);
			if (oa != ob)
				return oa < ob;
			return a.first < b.first;
		});

		const UT_sint32 MAX_TILES = 24;
		for (const auto& pr : styles)
		{
			if (nTiles >= MAX_TILES)
				break;
			const char * szName = pr.first.c_str();
			const PD_Style * pStyle = pr.second;

			std::string sLoc;
			pt_PieceTable::s_getLocalisedStyleName(szName, sLoc);
			const char * szDisp = sLoc.empty() ? szName : sLoc.c_str();

			/* two-line tile: styled sample text over the plain style
			 * name, like the LibreOffice/Word gallery */
			GtkWidget * tile = gtk_button_new();
			GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

			GtkWidget * sample = gtk_label_new(nullptr);
			gtk_label_set_markup(GTK_LABEL(sample),
				AP_UnixStylesPane::styleMarkup(
					pStyle, "AaBbCcDdEe", 8.0, 15.0).c_str());
			gtk_label_set_ellipsize(GTK_LABEL(sample),
									PANGO_ELLIPSIZE_END);
			gtk_label_set_max_width_chars(GTK_LABEL(sample), 12);
			gtk_box_append(GTK_BOX(vbox), sample);

			GtkWidget * name = gtk_label_new(nullptr);
			gchar * escLoc = g_markup_escape_text(szDisp, -1);
			std::string nameMarkup = "<span size='small'>";
			nameMarkup += escLoc;
			nameMarkup += "</span>";
			gtk_label_set_markup(GTK_LABEL(name), nameMarkup.c_str());
			g_free(escLoc);
			gtk_label_set_ellipsize(GTK_LABEL(name),
									PANGO_ELLIPSIZE_END);
			gtk_label_set_max_width_chars(GTK_LABEL(name), 14);
			gtk_box_append(GTK_BOX(vbox), name);

			gtk_button_set_child(GTK_BUTTON(tile), vbox);
			gtk_widget_add_css_class(tile, "abiword-style-tile");
			gtk_widget_set_tooltip_text(tile, szDisp);
			gtk_widget_set_valign(tile, GTK_ALIGN_FILL);
			gtk_widget_set_size_request(tile, 104, -1);

			_StyleTile * t = new _StyleTile;
			t->widget = tile;
			t->styleName = g_strdup(szName);
			m_vecStyleTiles.addItem(t);

			g_signal_connect(tile, "clicked",
							 G_CALLBACK(_s_style_tile_clicked), this);
			gtk_box_append(GTK_BOX(box), tile);
			++nTiles;
		}
	}

	/* reveal the strip once it actually holds tiles */
	if (nTiles > 0 && m_wStyleScroll)
		gtk_widget_set_visible(m_wStyleScroll, TRUE);
	_updateStyleScrollButtons(this);
}

void AP_UnixRibbon::_s_style_tile_clicked(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);

	for (UT_sint32 i = 0; i < self->m_vecStyleTiles.getItemCount(); ++i)
	{
		_StyleTile * t = self->m_vecStyleTiles.getNthItem(i);
		if (t->widget == w)
		{
			UT_UCS4String ucsName(t->styleName);
			self->_invokeToolbarItem(
				(XAP_Toolbar_Id)AP_TOOLBAR_ID_FMT_STYLE,
				ucsName.ucs4_str(), ucsName.length());
			return;
		}
	}
}

/* mark the gallery tile that matches the caret's current style */
void AP_UnixRibbon::_refreshStyleTiles(const char * szCurrentStyle)
{
	for (UT_sint32 i = 0; i < m_vecStyleTiles.getItemCount(); ++i)
	{
		_StyleTile * t = m_vecStyleTiles.getNthItem(i);
		bool bCur = szCurrentStyle && t->styleName &&
			strcmp(t->styleName, szCurrentStyle) == 0;
		if (bCur)
			gtk_widget_add_css_class(t->widget, "abiword-style-active");
		else
			gtk_widget_remove_css_class(t->widget, "abiword-style-active");
	}

	/* keep the docked Styles pane's current-style readout in sync */
	if (m_pFrame)
	{
		AP_UnixFrameImpl * pImpl = static_cast<AP_UnixFrameImpl *>(
			m_pFrame->getFrameImpl());
		if (pImpl)
			pImpl->refreshStylesPane(szCurrentStyle);
	}
}

/*
 * Set the combo's displayed text without firing "changed"
 * (combo_box_set_active_text equivalent for ribbon combos).
 */
void AP_UnixRibbon::_tb_combo_set_text(GtkComboBox * combo, const char * text,
							   _TbCtx * ctx)
{
	if (ABI_IS_FONT_COMBO(combo))
	{
		if (!abi_font_combo_select_text(ABI_FONT_COMBO(combo), text))
			abi_font_combo_insert_font(ABI_FONT_COMBO(combo), text, TRUE);
		return;
	}

	GtkTreeModel * model = gtk_combo_box_get_model(combo);
	if (!model)
		return;
	GtkTreeIter iter;
	gboolean next = gtk_tree_model_get_iter_first(model, &iter);
	while (next)
	{
		gchar * value = nullptr;
		gtk_tree_model_get(model, &iter, 0, &value, -1);
		bool bMatch = value && strcmp(text, value) == 0;
		g_free(value);
		if (bMatch)
		{
			gtk_combo_box_set_active_iter(combo, &iter);
			return;
		}
		next = gtk_tree_model_iter_next(model, &iter);
	}

	/* combos with an entry show the real value straight in the
	 * entry - appending it would leak odd document sizes (e.g. a
	 * stray 3pt run) into the dropdown list permanently */
	GtkWidget * child = gtk_combo_box_get_child(combo);
	if (child && GTK_IS_EDITABLE(child))
	{
		gtk_editable_set_text(GTK_EDITABLE(child), text);
		return;
	}

	/* not in the list - append it so the combo shows the real value
	 * (document styles not in the seed list) */
	if (GTK_IS_COMBO_BOX_TEXT(combo))
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), text);
		_tb_combo_set_text(combo, text, ctx);	/* re-run to select it */
	}
}

void AP_UnixRibbon::_refreshToolbarItems()
{
	const EV_Toolbar_ActionSet * pTBActions =
		XAP_App::getApp()->getToolbarActionSet();
	if (!pTBActions)
		return;

	AV_View * pView = m_pFrame ? m_pFrame->getCurrentView() : nullptr;

	UT_sint32 count = m_vecTbCtx.getItemCount();
	for (UT_sint32 i = 0; i < count; ++i)
	{
		_TbCtx * ctx = m_vecTbCtx.getNthItem(i);
		EV_Toolbar_Action * pAction = pTBActions->getAction(ctx->id);
		if (!pAction || !ctx->widget)
			continue;

		const char * szState = nullptr;
		EV_Toolbar_ItemState tis = pView
			? pAction->getToolbarItemState(pView, &szState)
			: EV_TIS_Gray;
		if (tis & EV_TIS_Hidden)
			tis = (EV_Toolbar_ItemState)(tis | EV_TIS_Gray);

		bool bGrayed = EV_TIS_ShouldBeGray(tis);
		gtk_widget_set_sensitive(ctx->widget, !bGrayed);
		/* FMT_STYLE stays hidden permanently (state-only ctx) */
		if (ctx->id != (XAP_Toolbar_Id)AP_TOOLBAR_ID_FMT_STYLE)
			gtk_widget_set_visible(ctx->widget,
								   !EV_TIS_ShouldBeHidden(tis));

		bool wasBlocked = ctx->blockSignal;
		ctx->blockSignal = true;

		switch (pAction->getItemType())
		{
		case EV_TBIT_ToggleButton:
		case EV_TBIT_GroupButton:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctx->widget),
										 EV_TIS_ShouldBeToggled(tis));
			break;

		case EV_TBIT_ComboBox:
			if (!szState)
			{
				if (ABI_IS_FONT_COMBO(ctx->widget))
					abi_font_combo_unselect(ABI_FONT_COMBO(ctx->widget));
				else
					gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->widget), -1);
				if (ctx->id == (XAP_Toolbar_Id)AP_TOOLBAR_ID_FMT_STYLE)
					_refreshStyleTiles(nullptr);
			}
			else if (ctx->id == (XAP_Toolbar_Id)AP_TOOLBAR_ID_FMT_SIZE)
			{
				const char * fsz =
					XAP_EncodingManager::fontsizes_mapping.lookupBySource(szState);
				_tb_combo_set_text(GTK_COMBO_BOX(ctx->widget),
								   fsz ? fsz : szState, ctx);
			}
			else if (ctx->id == (XAP_Toolbar_Id)AP_TOOLBAR_ID_FMT_STYLE)
			{
				std::string sLoc;
				pt_PieceTable::s_getLocalisedStyleName(szState, sLoc);
				_tb_combo_set_text(GTK_COMBO_BOX(ctx->widget),
								   sLoc.c_str(), ctx);
				_refreshStyleTiles(szState);
			}
			else if (ABI_IS_FONT_COMBO(ctx->widget))
			{
				if (!abi_font_combo_select_text(ABI_FONT_COMBO(ctx->widget),
												szState))
					abi_font_combo_insert_font(ABI_FONT_COMBO(ctx->widget),
											   szState, TRUE);
			}
			else
			{
				_tb_combo_set_text(GTK_COMBO_BOX(ctx->widget), szState, ctx);
			}
			break;

		default:
			break;
		}

		ctx->blockSignal = wasBlocked;
	}
}

void AP_UnixRibbon::refresh()
{
	/* GtkNotebook emits "switch-page" while it is being disposed
	 * during window teardown; the frame is already half gone there,
	 * so getCurrentView() would dereference a dead view list */
	if (m_wNotebook && gtk_widget_in_destruction(m_wNotebook))
		return;
	/* _refreshContextualTabs() can emit "switch-page" (hiding the
	 * current contextual tab, or steering it back to Insert), which
	 * re-enters refresh(); refreshMenu is not re-entrant, so defer
	 * nested calls until the outer one finishes */
	if (m_bRefreshing)
	{
		m_bRefreshAgain = true;
		return;
	}
	m_bRefreshing = true;
	do
	{
		m_bRefreshAgain = false;
		AV_View * view = m_pFrame ? m_pFrame->getCurrentView() : nullptr;
		if (view)
			m_pMenu->refreshMenu(view);
		_refreshContextualTabs();
		_populateStyleTiles();   /* lazy: view/doc may not exist at build time */
		_refreshToolbarItems();
		_refreshSpinFields();
		/* keep the Display-for-Review caption in sync with the
		 * active markup mode */
		if (m_pMarkupLabel)
			gtk_label_set_text(GTK_LABEL(m_pMarkupLabel),
							   _markupModeName());
	} while (m_bRefreshAgain);
	m_bRefreshing = false;
}

void AP_UnixRibbon::_refreshContextualTabs()
{
	FV_View * view = static_cast<FV_View *>(
		m_pFrame ? m_pFrame->getCurrentView() : nullptr);
	bool bInTable = view && view->isInTable();
	bool bInMath = view && view->isInMath();

	UT_sint32 count = m_vecContextualPages.getItemCount();
	for (UT_sint32 i = 0; i < count; ++i)
	{
		GtkWidget * page = m_vecContextualPages.getNthItem(i);
		const char * key = static_cast<const char *>(
			g_object_get_data(G_OBJECT(page), "abi-ctx-key"));
		bool vis = bInTable;
		if (key && !strcmp(key, "equation"))
			vis = bInMath;

		/* when the current tab is about to hide, GTK would flip to
		 * an adjacent page (usually the last one — Help); land back
		 * on the Insert tab instead, where the object came from */
		GtkWidget * cur = gtk_notebook_get_nth_page(
			GTK_NOTEBOOK(m_wNotebook),
			gtk_notebook_get_current_page(GTK_NOTEBOOK(m_wNotebook)));
		const bool wasCurrent = (cur == page);

		gtk_widget_set_visible(page, vis);

		if (!vis && wasCurrent)
		{
			int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(m_wNotebook));
			for (int j = 0; j < n; ++j)
			{
				GtkWidget * p = gtk_notebook_get_nth_page(
					GTK_NOTEBOOK(m_wNotebook), j);
				const char * tk = static_cast<const char *>(
					g_object_get_data(G_OBJECT(p), "abi-tab-key"));
				if (tk && !strcmp(tk, "insert"))
				{
					gtk_notebook_set_current_page(
						GTK_NOTEBOOK(m_wNotebook), j);
					break;
				}
			}
		}
	}
}

void AP_UnixRibbon::_s_switch_page(GtkNotebook * /*book*/,
								   GtkWidget * /*page*/,
								   guint /*page_num*/,
								   gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	if (self)
		self->refresh();
}

void AP_UnixRibbon::_s_motion_enter(GtkEventControllerMotion * /*ctrl*/,
									gdouble /*x*/, gdouble /*y*/,
									gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	if (self)
		self->refresh();
}
