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
	GtkWidget *		_makeBordersPopover();
	GtkWidget *		_borderRow(int edges, const char * szLabel,
							   const char * szMethod,
							   const char * szData,
							   bool bSensitive = true);
	GtkWidget *		_makeBulletLibraryPopover();
	GtkWidget *		_makeNumberingLibraryPopover();
	GtkWidget *		_makeMultilevelLibraryPopover();
	GtkWidget *		_listTile(const char * szMarkup,
							  const char * szData,
							  int iWidth, int iHeight);
	GtkWidget *		_makeChangeCasePopover();
	GtkWidget *		_makeMenuPopButton(XAP_Menu_Id id, uint8_t flags);
	GtkWidget *		_makeLargeMenuButton(XAP_Menu_Id id,
										 GtkWidget * popover,
										 uint8_t flags);
	GtkWidget *		_makeMenuPopTbButton(XAP_Toolbar_Id id, uint8_t flags);
	GtkWidget *		_makeLineSpacingPopover();
	GtkWidget *		_makeParaSpacingPopover();
	GtkWidget *		_makeSortParaPopover();
	/* Layout tab */
	GtkWidget *		_makeMarginsPopover();
	GtkWidget *		_makeOrientationPopover();
	GtkWidget *		_makeSizePopover();
	GtkWidget *		_makeColumnsPopover();
	GtkWidget *		_makeBreaksPopover();
	GtkWidget *		_makeLineNumbersPopover();
	GtkWidget *		_makeHyphenationPopover();
	GtkWidget *		_makeWrapPopover();
	GtkWidget *		_makePositionPopover();
	GtkWidget *		_makeAlignObjPopover();
	GtkWidget *		_makeSpinField(int spinId);
	GtkWidget *		_presetRow(const char * szName, const char * szDetail,
							   GtkWidget * icon, const char * szMethod,
							   const char * szData, bool bSensitive = true);
	GtkWidget *		_makeZOrderPopover(bool bForward);
	GtkWidget *		_makeRotatePopover();
	GtkWidget *		_makeGroupPopover();
	/* Insert tab */
	GtkWidget *		_makeCoverPagePopover();
	GtkWidget *		_makePicturesPopover();
	GtkWidget *		_makeShapesPopover();
	GtkWidget *		_make3DModelsPopover();
	GtkWidget *		_makeMediaPopover();
	GtkWidget *		_makeWordArtPopover();
	GtkWidget *		_makeEquationPopover();
	GtkWidget *		_makeEquationPalette(bool bStructures);
	GtkWidget *		_equationPreview(const char * szLatex, int w, int h);
	GtkWidget *		_makeTextBoxPopover();
	GtkWidget *		_makeObjectPopover();
	GtkWidget *		_makeHdrFtrPopover(bool bFooter);
	GtkWidget *		_makePageNumberPopover();
	GtkWidget *		_makeDropCapPopover();
	/* Review tab */
	GtkWidget *		_makeCommentDeletePopover();
	GtkWidget *		_makeCommentShowPopover();
	void			_showOnlinePictureDialog();
	void			_addGalleryDir(GtkWidget * parent,
								   const char * szMethod,
								   const char * szPrefix,
								   const char * szDataPrefix,
								   const char * szTitle, int iconSize,
								   bool bRowLabel);
	static void		_s_online_pic_clicked(GtkWidget * w, gpointer data);
	static void		_s_online_pic_insert(GtkWidget * w, gpointer data);
	/* References tab */
	GtkWidget *		_makeTOCGalleryPopover();
	GtkWidget *		_makeAddTextPopover();
	GtkWidget *		_makeNextNotePopover();
	GtkWidget *		_makeCaptionPopover();
	GtkWidget *		_makeTOFPopover();
	GtkWidget *		_makeXRefPopover();
	GtkWidget *		_makeMarkEntryPopover();
	GtkWidget *		_makeMarkCitPopover();
	GtkWidget *		_makeCitationPopover();
	GtkWidget *		_makeBibliographyPopover();
	GtkWidget *		_makeSourcesPopover();
	GtkWidget *		_makeDeadButton(uint16_t id);
	static void		_s_cover_gallery_map(GtkWidget * popover,
										 gpointer data);
	static void		_s_toc_gallery_map(GtkWidget * popover,
									   gpointer data);
	static void		_s_caption_apply(GtkWidget * w, gpointer data);
	static void		_s_tof_apply(GtkWidget * w, gpointer data);
	static void		_s_xref_map(GtkWidget * popover, gpointer data);
	static void		_s_xref_apply(GtkWidget * w, gpointer data);
	static void		_s_markentry_map(GtkWidget * popover, gpointer data);
	static void		_s_markentry_apply(GtkWidget * w, gpointer data);
	static void		_s_markcit_apply(GtkWidget * w, gpointer data);
	static void		_s_citation_apply(GtkWidget * w, gpointer data);
	static void		_s_biblio_map(GtkWidget * popover, gpointer data);
	static void		_s_sources_map(GtkWidget * popover, gpointer data);
	static void		_s_rotate_to_clicked(GtkWidget * w, gpointer data);
	static void		_s_arrange_popover_map(GtkWidget * popover,
										   gpointer data);
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
	std::string		_refSelectionText() const;
	void			_showPasteSpecialDialog();
	void			_refreshContextualTabs();
	void			_refreshToolbarItems();
	void			_populateStyleTiles();
	void			_refreshStyleTiles(const char * szCurrentStyle);
	void			_buildIconMap();

	static void		_s_style_tile_clicked(GtkWidget * w, gpointer data);
	static void		_s_style_scroll_clicked(GtkWidget * w, gpointer data);
	static void		_s_styles_pane_clicked(GtkWidget * w, gpointer data);
	static void		_updateStyleScrollButtons(AP_UnixRibbon * self);

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
	static void			_s_spin_changed(GtkSpinButton * spin, gpointer data);
	static gboolean		_s_spin_apply(gpointer data);
	static void			_s_linedlg_clicked(GtkWidget * w, gpointer data);
	static void			_s_hyphdlg_clicked(GtkWidget * w, gpointer data);
	void				_refreshSpinFields();
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
	GtkWidget *			m_wStylePrev;
	GtkWidget *			m_wStyleNext;

	XAP_Frame *			m_pFrame;
	EV_UnixMenuBar *	m_pMenu;
	GtkWidget *			m_wNotebook;
	bool				m_bRefreshing = false;
	bool				m_bRefreshAgain = false;
	EV_Toolbar_LabelSet *	m_pTBLabels;
	UT_GenericVector<GtkWidget*>	m_vecContextualPages;
	GHashTable *		m_pIconMap; /* edit-method name -> icon name */

	/* Layout indent/spacing spin fields: prop name -> widget, synced
	 * by _refreshSpinFields() */
	struct _SpinField
	{
		GtkWidget *	spin;
		const char * prop;	/* static block property name */
	};
	UT_GenericVector<_SpinField*>	m_vecSpins;
	bool				m_bSpinUpdating;
};

#endif /* AP_UNIXRIBBON_H */
