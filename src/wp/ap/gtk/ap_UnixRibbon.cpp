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
#include "ap_Ribbon_Layouts.h"
#include "gr_CairoGraphics.h"
#include "pt_PieceTable.h"
#include "pd_Document.h"
#include "pd_Style.h"
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
	{ "views",       "Views" },
	{ "page",        "Page Setup" },
	{ "columns",     "Page Columns" },
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

AP_UnixRibbon::AP_UnixRibbon(XAP_Frame * pFrame, EV_UnixMenuBar * pMenu)
	: m_pFrame(pFrame)
	, m_pMenu(pMenu)
	, m_wNotebook(nullptr)
	, m_pTBLabels(nullptr)
	, m_wStyleBox(nullptr)
	, m_wStyleScroll(nullptr)
	, m_pIconMap(nullptr)
{
}

AP_UnixRibbon::~AP_UnixRibbon()
{
	// m_wNotebook is owned by the widget tree; nothing to unref here.
	g_clear_pointer(&m_pIconMap, g_hash_table_unref);
	DELETEP(m_pTBLabels);
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
		"  border-right: 1px solid @borders;"
		"}"
		".abiword-ribbon .ribbon-group-title {"
		"  font-size: 0.78em; margin-top: 1px; padding: 0 4px 2px 4px;"
		"  color: alpha(@theme_fg_color, 0.75);"
		"}"
		".abiword-ribbon combobox, .abiword-ribbon dropdown { margin: 1px 2px; }"
		".abiword-ribbon notebook > header { margin-bottom: 0; }"
		/* Word-style style gallery tiles */
		".abiword-ribbon scrolledwindow { min-height: 0; }"
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
			for (const AP_RibbonItem * it = group->items;
				 !(it->kind == AP_RIBBON_ITEM_MENU &&
				   it->id == (uint16_t)AP_MENU_ID__BOGUS1__); ++it)
			{
				if (it->kind == AP_RIBBON_ITEM_ROWEND)
				{
					bRowMajor = true;
					break;
				}
			}
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
				else if (item->flags & AP_RIBBON_FLAG_MENUPOP)
					w = _makeMenuPopButton((XAP_Menu_Id)item->id,
										   item->flags);
				else if (item->kind == AP_RIBBON_ITEM_TOOLBAR)
					w = _makeToolbarWidget((XAP_Toolbar_Id)item->id,
										   item->flags);
				else
					w = _makeButton((XAP_Menu_Id)item->id, item->flags);
				if (!w)
					continue;
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
						popover = _makeListPopover();
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
			gtk_box_append(GTK_BOX(page), frame);
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
			" min-width: 0px; }");
G_GNUC_END_IGNORE_DEPRECATIONS
	}
	return p;
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

	GAction * action = m_pMenu->lookupAction(id);
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
		szIcon = nullptr;	/* theme lacks the icon - use the text label */

	if (szIcon && *szIcon && (flags & AP_RIBBON_FLAG_ICONONLY))
	{
		/* Word-style compact glyph button: icon only, the name lives
		 * in the tooltip */
		GtkWidget * image = gtk_image_new_from_icon_name(szIcon);
		gtk_widget_set_valign(image, GTK_ALIGN_CENTER);
		gtk_button_set_child(GTK_BUTTON(btn), image);
	}
	else if (szIcon && *szIcon && (flags & AP_RIBBON_FLAG_LARGE))
	{
		/* Word-style large button: icon on top, caption underneath */
		GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		GtkWidget * image = gtk_image_new_from_icon_name(szIcon);
		gtk_image_set_pixel_size(GTK_IMAGE(image), 24);
		gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
		GtkWidget * wLabel = gtk_label_new(label);
		gtk_label_set_ellipsize(GTK_LABEL(wLabel), PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars(GTK_LABEL(wLabel), 14);
		gtk_box_append(GTK_BOX(box), image);
		gtk_box_append(GTK_BOX(box), wLabel);
		gtk_button_set_child(GTK_BUTTON(btn), box);
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
		else
		{
			gtk_label_set_ellipsize(GTK_LABEL(wLabel), PANGO_ELLIPSIZE_END);
			gtk_label_set_max_width_chars(GTK_LABEL(wLabel), 18);
		}

		if (szIcon && *szIcon)
		{
			GtkWidget * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
			GtkWidget * image = gtk_image_new_from_icon_name(szIcon);
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
											const char * szMethod)
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
	UT_return_if_fail(self && szMethod);
	_tb_popdown_popover(w);
	self->_invokeEditMethod(szMethod);
}

void AP_UnixRibbon::_s_paste_special_clicked(GtkWidget * w, gpointer data)
{
	AP_UnixRibbon * self = static_cast<AP_UnixRibbon *>(data);
	UT_return_if_fail(self);
	_tb_popdown_popover(w);
	self->_showPasteSpecialDialog();
}

void AP_UnixRibbon::_invokeEditMethod(const char * szMethod)
{
	const EV_EditMethodContainer * pEMC =
		XAP_App::getApp()->getEditMethodContainer();
	UT_return_if_fail(pEMC);

	EV_EditMethod * pEM = pEMC->findEditMethodByName(szMethod);
	UT_return_if_fail(pEM);

	AV_View * pView = m_pFrame ? m_pFrame->getCurrentView() : nullptr;
	EV_EditMethodCallData emcd;
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

/* List options: pick the type, change the level, or open the dialog */
GtkWidget * AP_UnixRibbon::_makeListPopover()
{
	GtkWidget * popover = gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);
	gtk_widget_set_margin_start(box, 4);
	gtk_widget_set_margin_end(box, 4);

	GtkWidget * w = _popoverTbButton((XAP_Toolbar_Id)AP_TOOLBAR_ID_LISTS_BULLETS,
									 "Bulleted List");
	if (w) gtk_box_append(GTK_BOX(box), w);
	w = _popoverTbButton((XAP_Toolbar_Id)AP_TOOLBAR_ID_LISTS_NUMBERS, "Numbered List");
	if (w) gtk_box_append(GTK_BOX(box), w);
	w = _popoverTbButton((XAP_Toolbar_Id)AP_TOOLBAR_ID_LISTS_DASHED, "Dashed List");
	if (w) gtk_box_append(GTK_BOX(box), w);

	gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	w = _popoverTbButton((XAP_Toolbar_Id)AP_TOOLBAR_ID_INDENT, "Increase Level");
	if (w) gtk_box_append(GTK_BOX(box), w);
	w = _popoverTbButton((XAP_Toolbar_Id)AP_TOOLBAR_ID_UNINDENT, "Decrease Level");
	if (w) gtk_box_append(GTK_BOX(box), w);

	gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	w = _popoverMenuButton((XAP_Menu_Id)AP_MENU_ID_FMT_BULLETS);
	if (w) gtk_box_append(GTK_BOX(box), w);

	gtk_popover_set_child(GTK_POPOVER(popover), box);
	return popover;
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
 * the popover - no separate arrow widget, no action on the button */
GtkWidget * AP_UnixRibbon::_makeMenuPopButton(XAP_Menu_Id id,
											 uint8_t flags)
{
	GtkWidget * popover = (id == (XAP_Menu_Id)AP_MENU_ID_FMT_TOGGLECASE)
		? _makeChangeCasePopover() : nullptr;
	if (!popover)
		return nullptr;

	GtkWidget * mb = gtk_menu_button_new();
	GtkWidget * gl = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(gl),
						 id == (XAP_Menu_Id)AP_MENU_ID_FMT_TOGGLECASE
						 ? "Aa" : "?");
	gtk_menu_button_set_child(GTK_MENU_BUTTON(mb), gl);
	if (flags & AP_RIBBON_FLAG_SLIM)
	{
		gtk_style_context_add_provider(
			gtk_widget_get_style_context(mb),
			GTK_STYLE_PROVIDER(_slimButtonCss()),
			GTK_STYLE_PROVIDER_PRIORITY_USER);
		/* the menubutton's inner toggle button does not inherit
		 * context providers - slim it directly too */
		GtkWidget * inner = gtk_widget_get_first_child(mb);
		if (inner)
			gtk_style_context_add_provider(
				gtk_widget_get_style_context(inner),
				GTK_STYLE_PROVIDER(_slimButtonCss()),
				GTK_STYLE_PROVIDER_PRIORITY_USER);
	}
	gtk_menu_button_set_direction(GTK_MENU_BUTTON(mb), GTK_ARROW_NONE);
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

/* attach a small drop-arrow menu button beside (or below) a button */
GtkWidget * AP_UnixRibbon::_wrapSplit(GtkWidget * w, GtkWidget * popover,
									  bool bVertical)
{
	GtkWidget * arrow = gtk_menu_button_new();
	gtk_menu_button_set_direction(GTK_MENU_BUTTON(arrow),
								  GTK_ARROW_DOWN);
	gtk_menu_button_set_has_frame(GTK_MENU_BUTTON(arrow), FALSE);
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(arrow), popover);

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
		gtk_editable_set_width_chars(GTK_EDITABLE(entry), 3);
		gtk_editable_set_max_width_chars(GTK_EDITABLE(entry), 5);
		/* slim the combo's dropdown button to match the glyph
		 * buttons beside it */
		gtk_style_context_add_provider(
			gtk_widget_get_style_context(combo),
			GTK_STYLE_PROVIDER(_slimButtonCss()),
			GTK_STYLE_PROVIDER_PRIORITY_USER);
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
		GTK_SCROLLED_WINDOW(scroll), 560);
	m_wStyleBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_widget_set_valign(m_wStyleBox, GTK_ALIGN_CENTER);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
								  m_wStyleBox);
	m_wStyleScroll = scroll;
	gtk_widget_set_visible(scroll, FALSE);
	_populateStyleTiles();
	return scroll;
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
		const UT_sint32 MAX_TILES = 10;
		for (UT_uint32 k = 0; nTiles < MAX_TILES; ++k)
		{
			const char * szName = nullptr;
			const PD_Style * pStyle = nullptr;
			if (!pdoc->enumStyles(k, &szName, &pStyle))
				break;
			if (!pStyle || !szName || !*szName || !pStyle->isDisplayed() ||
				pStyle->isCharStyle())
				continue;

			std::string markup;
			{
				const gchar * szVal = nullptr;
				gchar * esc = g_markup_escape_text(szName, -1);
				std::string open, close;
				if (pStyle->getPropertyExpand("font-family", szVal) && szVal)
				{
					gchar * ef = g_markup_escape_text(szVal, -1);
					open += "<span font_family='";
					open += ef;
					open += "'>";
					close = "</span>" + close;
					g_free(ef);
				}
				if (pStyle->getPropertyExpand("font-weight", szVal) &&
					szVal && strcmp(szVal, "bold") == 0)
				{ open += "<b>"; close = "</b>" + close; }
				if (pStyle->getPropertyExpand("font-style", szVal) &&
					szVal && strcmp(szVal, "italic") == 0)
				{ open += "<i>"; close = "</i>" + close; }
				if (pStyle->getPropertyExpand("text-decoration", szVal) &&
					szVal && strstr(szVal, "underline"))
				{ open += "<u>"; close = "</u>" + close; }
				if (pStyle->getPropertyExpand("font-size", szVal) && szVal)
				{
					/* render at roughly the style size, clamped so the
					 * tile stays inside the ribbon band */
					double pt = g_ascii_strtod(szVal, nullptr);
					if (pt > 0)
					{
						pt = CLAMP(pt, 8.0, 18.0);
						char buf[64];
						g_snprintf(buf, sizeof(buf), "<span size='%d'>",
								   static_cast<int>(pt * PANGO_SCALE));
						open += buf;
						close = "</span>" + close;
					}
				}
				if (pStyle->getPropertyExpand("color", szVal) && szVal &&
					strlen(szVal) == 6)
				{
					open += "<span foreground='#";
					open += szVal;
					open += "'>";
					close = "</span>" + close;
				}
				markup = open + esc + close;
				g_free(esc);
			}

			GtkWidget * tile = gtk_button_new();
			GtkWidget * label = gtk_label_new(nullptr);
			std::string sLoc;
			pt_PieceTable::s_getLocalisedStyleName(szName, sLoc);
			/* swap the raw name for the localized one inside the markup */
			gchar * escName = g_markup_escape_text(szName, -1);
			gchar * escLoc = g_markup_escape_text(sLoc.c_str(), -1);
			size_t pos = markup.find(escName);
			if (pos != std::string::npos)
				markup.replace(pos, strlen(escName), escLoc);
			gtk_label_set_markup(GTK_LABEL(label), markup.c_str());
			g_free(escName);
			g_free(escLoc);
			gtk_button_set_child(GTK_BUTTON(tile), label);
			gtk_widget_add_css_class(tile, "abiword-style-tile");
			gtk_widget_set_tooltip_text(tile, sLoc.c_str());

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

	/* not in the list - append it so the combo shows the real value
	 * (custom zoom levels, document styles not in the seed list) */
	if (GTK_IS_COMBO_BOX_TEXT(combo))
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), text);
		_tb_combo_set_text(combo, text, ctx);	/* re-run to select it */
		return;
	}

	GtkWidget * child = gtk_combo_box_get_child(combo);
	if (child && GTK_IS_EDITABLE(child))
		gtk_editable_set_text(GTK_EDITABLE(child), text);
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
		gtk_widget_set_visible(ctx->widget, !EV_TIS_ShouldBeHidden(tis));

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
	AV_View * view = m_pFrame ? m_pFrame->getCurrentView() : nullptr;
	if (view)
		m_pMenu->refreshMenu(view);
	_refreshContextualTabs();
	_populateStyleTiles();   /* lazy: view/doc may not exist at build time */
	_refreshToolbarItems();
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
