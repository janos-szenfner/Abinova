/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:t -*- */
/* Abinova
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

#ifndef AP_UNIXRIBBON_H
#define AP_UNIXRIBBON_H

#include <gtk/gtk.h>

#include <set>
#include <string>
#include <vector>
#include "ut_types.h"
#include "ut_vector.h"
#include "xap_Types.h"
#include "ap_Toolbar_Id.h"

class XAP_Frame;
class XAP_Prefs;
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

	/* opens the Paste Special dialog; also used by the paste-options
	 * smart tag in the document overlay */
	void			showPasteSpecialDialog() { _showPasteSpecialDialog(); }

private:
	GtkWidget *		_makeButton(XAP_Menu_Id id, uint8_t flags);
	GtkWidget *		_makeToolbarWidget(XAP_Toolbar_Id id, uint8_t flags);
	GtkWidget *		_makeStyleGallery();
	GtkWidget *		_makeStyleSetGallery();
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
	GtkWidget *		_makeThemesPopover();
	GtkWidget *		_makeStyleSetsPopover();
	GtkWidget *		_makeDesignParaSpacingPopover();
	GtkWidget *		_makeThemeColorsPopover();
	GtkWidget *		_makeThemeFontsPopover();
	GtkWidget *		_makeWatermarkPopover();
	GtkWidget *		_makePageColorPopover();
	GtkWidget *		_makePageBordersPopover();
	void			_showCustomWatermarkDialog();
	static void		_s_watermark_custom_clicked(GtkWidget * w,
													gpointer data);
	static void		_s_custom_watermark_response(GtkDialog * dlg,
												 gint resp, gpointer data);
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
	GtkWidget *		_makeTableGridPopover();
	static void			_tablegrid_commit(struct _TableGridPick * pk);
	static void			_s_tablegrid_click(GtkGestureClick * g, int n,
										   double x, double y, gpointer data);
	static gboolean		_s_tablegrid_event(GtkEventControllerLegacy * c,
										   GdkEvent * event, gpointer data);
	static gboolean		_s_tablegrid_commit_idle(gpointer data);
	GtkWidget *		_makeCoverPagePopover();
	GtkWidget *		_makePicturesPopover();
	GtkWidget *		_makeShapesPopover();
	GtkWidget *		_make3DModelsPopover();
	GtkWidget *		_makeMediaPopover();
	GtkWidget *		_makeWordArtPopover();
	GtkWidget *		_makeEquationPopover();
	GtkWidget *		_makeEquationPalette(bool bStructures);
	GtkWidget *		_makeEquationStructures();
	GtkWidget *		_eqStructDrop(const char * szIconLatex,
								  const char * szCaption,
								  const void * items, unsigned n);
	GtkWidget *		_equationPreview(const char * szLatex, int w, int h);
	int				_uiScale() const;
	GtkWidget *		_makeTextBoxPopover();
	GtkWidget *		_makeObjectPopover();
	GtkWidget *		_makeHdrFtrPopover(bool bFooter);
	GtkWidget *		_makePageNumberPopover();
	GtkWidget *		_makeDropCapPopover();
	/* Developer tab: content controls (w:sdt) */
	GtkWidget *		_makeContentControlPopover();
	GtkWidget *		_makeCtlPropsPopover();
	GtkWidget *		_ctlPropRow(const char * szLabel,
								const char * szValue);
	static void		_s_ctlprops_apply(GtkWidget * w, gpointer data);
	/* Table Design tab */
	GtkWidget *		_makeTableStyleOptions();
	GtkWidget *		_makeTableStyleGallery();
	GtkWidget *		_makeTableStyleGalleryPopover();
	GtkWidget *		_tblStyleTile(const char * szStyleId, int iW, int iH,
								  bool bInPopover);
	GtkWidget *		_makeTblPopButton(int popId);
	GtkWidget *		_makeTblBordersPopover();
	GtkWidget *		_tblBorderRow(int edges, const char * szLabel,
								  const char * szData);
	GtkWidget *		_makeTblPenStylePopover();
	GtkWidget *		_makeTblLineStylePopover();
	GtkWidget *		_makeTblPenThickPopover();
	GtkWidget *		_tblPenRow(const char * szStyle, const char * szThickness,
							   const char * szLabel);
	GtkWidget *		_tblThemePenTile(const char * szThickness,
									 const char * szHex);
	static void		_s_tbl_painter_toggled(GtkToggleButton * tb,
										   gpointer data);
	static void		_s_tbl_theme_tile_clicked(GtkWidget * w,
											  gpointer data);
	static void		_s_tbl_sampler_clicked(GtkWidget * w,
										   gpointer data);
	GtkWidget *		_makeTblColorPopover(const char * szMethod,
										 const char * szAutomaticLabel);
	void			_refreshTableStyleOptions();
	void			_tblPreviewTile(GtkWidget * tile, bool bBegin);
	static void		_s_tbl_tile_clicked(GtkWidget * w, gpointer data);
	static void		_s_tbl_tile_motion_enter(GtkEventControllerMotion * ctrl,
											 gdouble x, gdouble y,
											 gpointer data);
	static void		_s_tbl_tile_motion_leave(GtkEventControllerMotion * ctrl,
											 gpointer data);
	static void		_s_tbl_opt_toggled(GtkCheckButton * cb, gpointer data);
	static void		_s_tblcolor_swatch_clicked(GtkWidget * w, gpointer data);
	static void		_s_tblcolor_auto_clicked(GtkWidget * w, gpointer data);
	static void		_s_tblcolor_custom_clicked(GtkWidget * w, gpointer data);
	static void		_s_tblcolor_custom_response(GtkDialog * dlg, gint resp,
												gpointer data);

	/* Review tab */
	GtkWidget *		_makeCommentDeletePopover();
	GtkWidget *		_makeCommentShowPopover();
	GtkWidget *		_makeSpellingPopover();
	GtkWidget *		_makeTrackChangesPopover();
	GtkWidget *		_makeMarkupPopover();
	GtkWidget *		_makeAcceptPopover();
	GtkWidget *		_makeRejectPopover();
	GtkWidget *		_makeComparePopover();
	GtkWidget *		_checkRow(const char * szLabel,
							  const char * szDetail,
							  const char * szMethod,
							  const char * szData,
							  const char * szKind,
							  GtkWidget * icon = nullptr);
	bool			_evalCheckKind(const char * szKind) const;
	const char *	_markupModeName() const;
	void			_refreshCheckRows(GtkWidget * popover);
	static void		_s_popover_check_show(GtkPopover * w, gpointer data);
	/* File tab */
	GtkWidget *		_makeRecentFilesPopover();
	void			_populateRecentList(GtkWidget * box);
	static void		_s_popover_recent_show(GtkPopover * w, gpointer data);
	static void		_s_recent_clear_clicked(GtkWidget * w, gpointer data);
	/* QAT undo/redo history dropdown: Word's split button lists
	 * recent ops; entry N undoes/redoes N steps */
	GtkWidget *		_makeHistoryDropButton(bool bUndo);
	GtkWidget *		_makeHistoryPopover(bool bUndo);
	void			_populateHistoryList(GtkWidget * box, bool bUndo);
	static void		_s_popover_history_show(GtkPopover * w, gpointer data);
	/* View tab */
	GtkWidget *		_makeZoomPopover();
	GtkWidget *		_makeWindowPopover();
	void			_populateWindowList(GtkWidget * box);
	static void		_s_popover_windows_show(GtkPopover * w, gpointer data);
	/* Table Layout tab */
	GtkWidget *		_makeTableSelectPopover();
	GtkWidget *		_makeTableDeletePopover();
	GtkWidget *		_makeTableAutoFitPopover();
	GtkWidget *		_makeTableSortPopover();
	GtkWidget *		_makeTableToTextPopover();
	GtkWidget *		_makeTableTextDirPopover();
	GtkWidget *		_makeTableMergePopover();
	GtkWidget *		_makeTableSplitPopover();
	GtkWidget *		_makeCellMarginsPopover();
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
	static void		_s_setgal_tile_clicked(GtkWidget * w, gpointer data);
	static void		_s_style_scroll_clicked(GtkWidget * w, gpointer data);
	static void		_s_styles_pane_clicked(GtkWidget * w, gpointer data);
	static void		_updateStyleScrollButtons(AP_UnixRibbon * self);

	static void		_s_switch_page(GtkNotebook * book, GtkWidget * page,
								   guint page_num, gpointer data);
	static void		_s_motion_enter(GtkEventControllerMotion * ctrl,
									gdouble x, gdouble y, gpointer data);

	/* tab overflow (» menu) + collapse (double-click tab) */
	void			_maybeReturnFocus();	/* Word: hand focus back
											 * to the doc after a
											 * ribbon command */

	/* persisted ribbon customization (Word's RibbonLayout
	 * equivalent): hidden/ordered tabs + Quick Access Toolbar */
	struct _RibbonLayout
	{
		std::vector<std::string>	hidden;		/* tab keys */
		std::vector<std::string>	order;		/* tab keys */
		std::vector<UT_uint32>		qat;		/* XAP_Menu_Id */
	};
	_RibbonLayout			m_layout;
	GtkWidget *				m_wQat = nullptr;
	GtkWidget *				m_wAutoSave = nullptr;

	/* AutoSave switch in the QAT lane: flips the AutoSaveFile pref;
	 * the pref listeners start/stop the frame's save timer */
	GtkWidget *		_makeAutoSaveToggle();
	void			_syncAutoSaveToggle();
	static gboolean	_s_autosave_toggled(GtkSwitch * w, gboolean state,
										gpointer data);
	static void		_s_autosave_pref(class XAP_Prefs *,
								 const std::set<std::string> * phChanges,
								 void * data);

	void			_loadLayout();
	void			_saveLayout() const;
	void			_applyLayout();
	void			_rebuildQat();
	void			_qatToggle(XAP_Menu_Id id);
	bool			_qatHas(XAP_Menu_Id id) const;
	bool			_tabHidden(const char * key) const;
	static void		_s_nb_rclick(GtkGestureClick * g, int n_press,
								 double x, double y, gpointer data);
	void			_tabContext(GtkWidget * lab, const char * key);
	static void		_s_qat_pop_clicked(GtkWidget * w, gpointer data);
	static void		_s_hide_tab_clicked(GtkWidget * w, gpointer data);
	static void		_s_reset_layout_clicked(GtkWidget * w, gpointer data);

	void			_reflowTabs();
	void			_setCollapsed(bool bCollapsed);
	void			_applyCollapsedPage();
	void			_foldBody();

	/* keytips: Alt shows letter badges over the tabs, a letter
	 * switches tab and shows badges over its commands */
	void			_ktShowTabs();
	void			_ktShowCommands();
	void			_ktDismiss();
	void			_ktClear();
	static gboolean	_s_kt_key(GtkEventControllerKey * c, guint keyval,
							  guint keycode, GdkModifierType state,
							  gpointer data);
	static void		_s_kt_mapped(GtkWidget * w, gpointer data);
	static gboolean	_s_reflow_tick(GtkWidget * w, GdkFrameClock * clock,
								   gpointer data);
	static void		_s_tab_pressed(GtkGestureClick * g, gint n_press,
								   gdouble x, gdouble y, gpointer data);
	static void		_s_tab_released(GtkGestureClick * g, gint n_press,
									gdouble x, gdouble y, gpointer data);
	static void		_s_overflow_tab_clicked(GtkWidget * w, gpointer data);
	static gboolean	_s_collapse_idle(gpointer data);

	/* per-toolbar-item callback context; owned by m_vecTbCtx */
	struct _TbCtx;

	static GtkWidget *	_tb_make_combo(_TbCtx * ctx);
	static GtkWidget *	_tb_color_button_new(const gchar * icon_name,
											 const gchar * automatic_label,
											 _TbCtx * ctx,
											 const gchar * szMarkup = nullptr);
	static gchar *		_tb_combo_get_text(GtkWidget * combo);
	static void			_tb_combo_apply(GtkWidget * combo, _TbCtx * ctx);
	static void			_tb_combo_set_text(GtkWidget * combo,
										   const char * text, _TbCtx * ctx);

	static void			_s_tb_clicked(GtkWidget * w, gpointer data);
	static void			_s_tb_combo_changed(GtkWidget * combo, gpointer data);
	static void			_s_tb_dropdown_changed(GtkWidget * dd, GParamSpec * pspec,
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
	static void			_s_spin_text_changed(GtkEditable * ed, gpointer data);
	static gboolean		_s_spin_apply(gpointer data);
	static void			_s_linedlg_clicked(GtkWidget * w, gpointer data);
	static void			_s_hyphdlg_clicked(GtkWidget * w, gpointer data);
	void				_refreshSpinFields();
	static void			_s_paste_special_clicked(GtkWidget * w, gpointer data);
	static void			_s_paste_special_response(GtkDialog * dlg,
												  gint resp, gpointer data);

	std::vector<_TbCtx*>	m_vecTbCtx;

	/* style-gallery tiles: widget -> unlocalised style name it applies */
	struct _StyleTile
	{
		GtkWidget *	widget;
		char *		styleName;
	};
	std::vector<_StyleTile*>	m_vecStyleTiles;
	GtkWidget *			m_wStyleBox;
	GtkWidget *			m_wStyleScroll;
	GtkWidget *			m_wStylePrev;
	GtkWidget *			m_wStyleNext;
	guint				m_iStyleBtnIdle = 0;	/* deferred arrow
											 * visibility update */

	XAP_Frame *			m_pFrame;
	EV_UnixMenuBar *	m_pMenu;
	GtkWidget *			m_wNotebook;
	GtkWidget *			m_wMore;		/* » tab-overflow menu button */
	GtkWidget *			m_wMoreBox;		/* its popover content */
	bool				m_bRefreshing = false;
	bool				m_bRefreshAgain = false;
	bool				m_bCollapsed = false;
	int					m_iReflowWidth = -1;
	guint				m_iCollapseIdle = 0;
	int					m_iTabPressPage = -1;	/* current page at press
												 * time while collapsed */

	/* keytip state: 0 off, 1 tab badges, 2 command badges */
	int					m_iKtMode = 0;
	GtkWidget *			m_wOverlay = nullptr;	/* GtkOverlay root the
												 * badges sit on */
	struct _KtEntry
	{
		std::string		key;		/* badge letters, e.g. "H", "JT" */
		GtkWidget *		target;		/* command widget (commands) */
		GtkWidget *		badge;		/* the overlay badge label */
		int				page;		/* notebook page (tabs) or -1 */
	};
	std::vector<_KtEntry>	m_vecKt;
	std::string				m_strKtPrefix;	/* letters typed so far */
	EV_Toolbar_LabelSet *	m_pTBLabels;
	std::vector<GtkWidget*>	m_vecContextualPages;
	std::vector<GtkWidget*>	m_vecOverflowPages;	/* pages hidden to fit tabs */

	/* Table Design tab widgets */
	GtkWidget *			m_wTblOptChecks[6] = { nullptr, nullptr, nullptr,
											   nullptr, nullptr, nullptr };
	bool				m_bTblOptSync = false;	/* suppress toggled-
												 * handlers while the
												 * checks are re-synced */
	GtkWidget *			m_wTblLineStyleIcon = nullptr;	/* preview in the
												 * compact style combo */
	GtkWidget *			m_wTblPenThickIcon = nullptr;	/* preview in the
												 * thickness combo */
	GtkWidget *			m_wTblPenThickLabel = nullptr;	/* "½ pt" label */
	GtkWidget *			m_wTblPenColorIcon = nullptr;	/* colour bar of
												 * the Pen Colour icon */
	GtkWidget *			m_wTblPainter = nullptr;	/* Border Painter
												 * toggle button */

	GtkWidget *			m_pMarkupLabel; /* caption of the Display-for-
										 * Review dropdown, shows the
										 * active markup mode */

	/* Layout indent/spacing + Table cell-size spin fields, synced
	 * by _refreshSpinFields() */
	struct _SpinField
	{
		GtkWidget *	spin;
		const char * prop;	/* static block property name, nullptr for
							 * the table cell-size spins */
		int			spinId;	/* AP_RIBBON_SPIN_* */
	};
	std::vector<_SpinField*>	m_vecSpins;
	bool				m_bSpinUpdating;
};

#endif /* AP_UNIXRIBBON_H */
