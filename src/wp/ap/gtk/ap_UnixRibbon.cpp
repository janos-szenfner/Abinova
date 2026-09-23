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
	{ "text",        "Text" },
	{ "symbols",     "Symbols" },
	{ "fields",      "Fields" },
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
	{ "revisions",   "Revisions" },
	{ "annotations", "Annotations" },
	{ "show",        "Show" },
	{ "window",      "Window" },
	{ "insert",      "Insert" },
	{ "delete",      "Delete" },
	{ "select",      "Select" },
	{ "format",      "Format" },
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

		if (!strcmp(tab->szTabKey, "home"))
			gtk_notebook_set_current_page(GTK_NOTEBOOK(m_wNotebook),
										  gtk_notebook_get_n_pages(
											  GTK_NOTEBOOK(m_wNotebook)) - 1);

		if (tab->bContextual)
		{
			m_vecContextualPages.addItem(page);
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
		GtkWidget * wLabel = gtk_label_new(label);
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
			gtk_label_set_max_width_chars(GTK_LABEL(wLabel), 14);
		}
		else if ((szIcon && *szIcon) || bDrawnIcon)
		{
			/* small icon+label buttons wrap onto a second line like
			 * Word's compact ribbon entries instead of ellipsizing */
			gtk_label_set_wrap(GTK_LABEL(wLabel), TRUE);
			gtk_label_set_wrap_mode(GTK_LABEL(wLabel), PANGO_WRAP_WORD_CHAR);
			gtk_label_set_lines(GTK_LABEL(wLabel), 2);
			gtk_label_set_justify(GTK_LABEL(wLabel), GTK_JUSTIFY_LEFT);
			gtk_label_set_max_width_chars(GTK_LABEL(wLabel), 12);
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

	GtkWidget * popover = gtk_popover_new();
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
	GtkWidget * popover = gtk_popover_new();
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
	GtkWidget * popover = gtk_popover_new();
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
	GtkWidget * popover = gtk_popover_new();
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
	GtkWidget * popover = gtk_popover_new();
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
	GtkWidget * popover = gtk_popover_new();
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
	default:
		break;
	}
	if (!popover)
		return nullptr;

	if (flags & AP_RIBBON_FLAG_LARGE)
		return _makeLargeMenuButton(id, popover, flags);

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
		/* Word wraps the small dropdown captions onto two lines
		 * ("Manage Sources", "Next Footnote") instead of ellipsizing */
		gtk_label_set_wrap(GTK_LABEL(wl), TRUE);
		gtk_label_set_wrap_mode(GTK_LABEL(wl), PANGO_WRAP_WORD_CHAR);
		gtk_label_set_lines(GTK_LABEL(wl), 2);
		gtk_label_set_justify(GTK_LABEL(wl), GTK_JUSTIFY_LEFT);
		gtk_label_set_max_width_chars(GTK_LABEL(wl), 12);
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
	bool	bullets;		/* text lines get a bullet dot (Bibliography) */
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
	double indent = s.bullets ? 2.4 : 0.0;
	cairo_set_source_rgb(cr, 0.55, 0.58, 0.65);
	cairo_set_line_width(cr, 0.9);
	for (int c = 0; c < cols; ++c)
	{
		double lx = mx0 + c * (cw + gap);
		for (double ly = my0 + 2.0; ly < my1 - 1.0; ly += 3.2)
		{
			cairo_move_to(cr, lx + indent, ly);
			cairo_line_to(cr, lx + cw, ly);
		}
	}
	cairo_stroke(cr);

	if (s.bullets)
	{
		/* blue bullet dot at the start of each text line */
		cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
		for (int c = 0; c < cols; ++c)
		{
			double lx = mx0 + c * (cw + gap);
			for (double ly = my0 + 2.0; ly < my1 - 1.0; ly += 3.2)
			{
				cairo_arc(cr, lx + 0.8, ly, 0.75, 0, 2 * G_PI);
				cairo_fill(cr);
			}
		}
	}

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

/* "ab" mark for Insert Citation */
static void _overlay_citation(cairo_t * cr, double w, double h)
{
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	_badge_text(cr, "ab", w - 7.0, h - 7.0, 8.5);
}

/* stacked source books for Manage Sources */
static void _overlay_sources(cairo_t * cr, double w, double h)
{
	/* each book: coloured cover with a pale "pages" edge on the right,
	 * the top book offset left like a real stack */
	cairo_set_source_rgb(cr, 0.55, 0.65, 0.35);
	cairo_rectangle(cr, w - 11.0, h - 6.6, 9.0, 3.0);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, 0.93, 0.94, 0.86);
	cairo_rectangle(cr, w - 4.2, h - 6.6, 1.8, 3.0);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, 0.2, 0.45, 0.9);
	cairo_rectangle(cr, w - 12.6, h - 10.2, 9.0, 3.0);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, 0.85, 0.9, 0.97);
	cairo_rectangle(cr, w - 5.8, h - 10.2, 1.8, 3.0);
	cairo_fill(cr);
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
		extra = _overlay_sources;
		break;
	case (XAP_Menu_Id)AP_MENU_ID_REF_BIBLIOGRAPHY:
		spec.bullets = true;
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
	GtkWidget * wLabel = gtk_label_new(label);
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

/* a popover row: [icon] name\n detail  - clicked runs an edit method */
GtkWidget * AP_UnixRibbon::_presetRow(const char * szName,
									  const char * szDetail,
									  GtkWidget * icon,
									  const char * szMethod,
									  const char * szData,
									  bool bSensitive)
{
	GtkWidget * btn = gtk_button_new();
	GtkWidget * row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

	if (icon)
	{
		gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(row), icon);
	}

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
	GtkWidget * popover = gtk_popover_new();
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
	GtkWidget * popover = gtk_popover_new();
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
	GtkWidget * popover = gtk_popover_new();
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
	GtkWidget * popover = gtk_popover_new();
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
	GtkWidget * popover = gtk_popover_new();
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
	GtkWidget * popover = gtk_popover_new();
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
	AV_View * view = m_pFrame ? m_pFrame->getCurrentView() : nullptr;
	if (view)
		m_pMenu->refreshMenu(view);
	_refreshContextualTabs();
	_populateStyleTiles();   /* lazy: view/doc may not exist at build time */
	_refreshToolbarItems();
	_refreshSpinFields();
}

void AP_UnixRibbon::_refreshContextualTabs()
{
	FV_View * view = static_cast<FV_View *>(
		m_pFrame ? m_pFrame->getCurrentView() : nullptr);
	bool bInTable = view && view->isInTable();

	UT_sint32 count = m_vecContextualPages.getItemCount();
	for (UT_sint32 i = 0; i < count; ++i)
		gtk_widget_set_visible(m_vecContextualPages.getNthItem(i), bInTable);
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
