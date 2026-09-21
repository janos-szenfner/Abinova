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

#ifndef AP_UNIXRIBBON_H
#define AP_UNIXRIBBON_H

#include <gtk/gtk.h>

#include "ut_types.h"
#include "ut_vector.h"
#include "xap_Types.h"
#include "ap_Toolbar_Id.h"

class XAP_Frame;
class AV_View;
class EV_UnixMenuBar;
class EV_Toolbar_LabelSet;

/*****************************************************************/
/* Ribbon modelled on the LibreOffice Writer NotebookBar
 * (sw/uiconfig/swriter/ui/notebookbar.ui): a GtkNotebook whose
 * pages are horizontal strips of labelled groups containing
 * buttons, combo boxes and color pickers.
 *
 * Menu-backed items bind the same "menu.*" GActions the classic
 * menubar uses; toolbar-backed items (font/size/style/zoom combos,
 * text/highlight colors, format painter, list/indent/spacing
 * buttons, column presets) dispatch through the same edit methods
 * the classic toolbars invoke.  Enablement and toggle/combo state
 * stay in sync through refresh(), driven by the normal
 * EV_UnixMenu refresh path.
 *
 * The tab/group layout lives in ap_Ribbon_Layouts.h.
 */

class AP_UnixRibbon
{
public:
	AP_UnixRibbon(XAP_Frame * pFrame, EV_UnixMenuBar * pMenu);
	~AP_UnixRibbon();

	/* builds the widget; caller packs it */
	GtkWidget *		createWidget();
	GtkWidget *		getWidget() const { return m_wNotebook; }

	/* re-sync action states and contextual tab visibility */
	void			refresh();

private:
	GtkWidget *		_makeButton(XAP_Menu_Id id, uint8_t flags);
	GtkWidget *		_makeToolbarWidget(XAP_Toolbar_Id id, uint8_t flags);
	GtkWidget *		_makeStyleGallery();
	GtkWidget *		_wrapSplit(GtkWidget * w, GtkWidget * popover,
							   bool bVertical);
	GtkWidget *		_makePastePopover();
	GtkWidget *		_makeListPopover(XAP_Toolbar_Id id);
	GtkWidget *		_makeBulletLibraryPopover();
	GtkWidget *		_makeNumberingLibraryPopover();
	GtkWidget *		_makeMultilevelLibraryPopover();
	GtkWidget *		_listTile(const char * szMarkup,
							  const char * szData,
							  int iWidth, int iHeight);
	GtkWidget *		_makeChangeCasePopover();
	GtkWidget *		_makeMenuPopButton(XAP_Menu_Id id, uint8_t flags);
	GtkWidget *		_makeMenuPopTbButton(XAP_Toolbar_Id id, uint8_t flags);
	GtkWidget *		_makeLineSpacingPopover();
	GtkWidget *		_makeParaSpacingPopover();
	GtkWidget *		_makeSortParaPopover();
	GtkWidget *		_popoverMenuButton(XAP_Menu_Id id);
	GtkWidget *		_popoverTbButton(XAP_Toolbar_Id id,
									 const char * szLabel);
	GtkWidget *		_popoverEmButton(const char * szLabel,
									 const char * szIcon,
									 const char * szMethod,
									 const char * szData = nullptr);
	void			_invokeToolbarItem(XAP_Toolbar_Id id,
									   const UT_UCS4Char * pData = nullptr,
									   UT_uint32 dataLength = 0);
	void			_invokeEditMethod(const char * szMethod,
									  const char * szData = nullptr);
	void			_showPasteSpecialDialog();
	void			_refreshContextualTabs();
	void			_refreshToolbarItems();
	void			_populateStyleTiles();
	void			_refreshStyleTiles(const char * szCurrentStyle);
	void			_buildIconMap();

	static void		_s_style_tile_clicked(GtkWidget * w, gpointer data);

	static void		_s_switch_page(GtkNotebook * book, GtkWidget * page,
								   guint page_num, gpointer data);
	static void		_s_motion_enter(GtkEventControllerMotion * ctrl,
									gdouble x, gdouble y, gpointer data);

	/* per-toolbar-item callback context; owned by m_vecTbCtx */
	struct _TbCtx;

	static GtkWidget *	_tb_make_combo(_TbCtx * ctx);
	static GtkWidget *	_tb_color_button_new(const gchar * icon_name,
											 const gchar * automatic_label,
											 _TbCtx * ctx,
											 const gchar * szMarkup = nullptr);
	static gchar *		_tb_combo_get_text(GtkComboBox * combo);
	static void			_tb_combo_apply(GtkComboBox * combo, _TbCtx * ctx);
	static void			_tb_combo_set_text(GtkComboBox * combo,
										   const char * text, _TbCtx * ctx);

	static void			_s_tb_clicked(GtkWidget * w, gpointer data);
	static void			_s_tb_combo_changed(GtkComboBox * combo, gpointer data);
	static gboolean		_s_tb_size_key(GtkEventControllerKey * ctrl,
									   guint keyval, guint keycode,
									   GdkModifierType state, gpointer data);
	static void			_s_tb_size_focus_out(GtkEventControllerFocus * ctrl,
											 gpointer data);
	static void			_s_tb_size_insert_text(GtkEditable * editable,
											   gchar * new_text,
											   gint new_text_length,
											   gint * position,
											   gpointer data);
	static void			_s_tb_color_activated(GtkColorChooser * cc,
											  GdkRGBA * color, gpointer data);
	static void			_s_tb_color_automatic(GtkWidget * w, gpointer data);
	static void			_s_tb_color_swatch_clicked(GtkWidget * w,
												   gpointer data);
	static void			_s_tb_color_custom_clicked(GtkWidget * w,
												   gpointer data);
	static void			_s_tb_color_custom_response(GtkDialog * dlg,
													gint resp, gpointer data);
	static GtkWidget *	_tb_color_swatch(const gchar * hex, _TbCtx * ctx);
	static void			_s_popover_tb_clicked(GtkWidget * w, gpointer data);
	static void			_s_popover_menu_clicked(GtkWidget * w, gpointer data);
	static void			_s_popover_em_clicked(GtkWidget * w, gpointer data);
	static void			_s_paste_special_clicked(GtkWidget * w, gpointer data);
	static void			_s_paste_special_response(GtkDialog * dlg,
												  gint resp, gpointer data);

	UT_GenericVector<_TbCtx*>	m_vecTbCtx;

	/* style-gallery tiles: widget -> unlocalised style name it applies */
	struct _StyleTile
	{
		GtkWidget *	widget;
		char *		styleName;
	};
	UT_GenericVector<_StyleTile*>	m_vecStyleTiles;
	GtkWidget *			m_wStyleBox;
	GtkWidget *			m_wStyleScroll;

	XAP_Frame *			m_pFrame;
	EV_UnixMenuBar *	m_pMenu;
	GtkWidget *			m_wNotebook;
	EV_Toolbar_LabelSet *	m_pTBLabels;
	UT_GenericVector<GtkWidget*>	m_vecContextualPages;
	GHashTable *		m_pIconMap; /* edit-method name -> icon name */
};

#endif /* AP_UNIXRIBBON_H */
