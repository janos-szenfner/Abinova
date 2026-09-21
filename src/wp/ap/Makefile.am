## Copyright (C) 2023 Hubert Figuière
##
## This program is free software; you can redistribute it and/or
## modify it under the terms of the GNU General Public License
## as published by the Free Software Foundation; either version 2
## of the License, or (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
## 02110-1301 USA.

CDUMPAPP = $(top_srcdir)/tools/cdump/xp/cdump.pl

noinst_LTLIBRARIES = libap.la

AM_CPPFLAGS = \
	$(WP_CPPFLAGS) \
	-I$(srcdir)/xp/ToolbarIcons

libap_la_SOURCES = \
	xp/ap_App.cpp \
	xp/ap_App.h \
	xp/ap_Args.cpp \
	xp/ap_Args.h \
	xp/ap_Clipboard.h \
	xp/ap_Convert.cpp \
	xp/ap_Convert.h \
	xp/ap_Dialog_Modeless.h \
	xp/ap_Dialog_Modeless.cpp \
	xp/ap_Dialog_Modal.h \
	xp/ap_Dialog_Modal.cpp \
	xp/ap_Dialog_Annotation.h \
	xp/ap_Dialog_Background.cpp \
	xp/ap_Dialog_Background.h \
	xp/ap_Dialog_Border_Shading.cpp \
	xp/ap_Dialog_Border_Shading.h \
	xp/ap_Dialog_Break.cpp \
	xp/ap_Dialog_Break.h \
	xp/ap_Dialog_Columns.cpp \
	xp/ap_Dialog_Columns.h \
	xp/ap_Dialog_Field.cpp \
	xp/ap_Dialog_Field.h \
	xp/ap_Dialog_FormatFootnotes.cpp \
	xp/ap_Dialog_FormatFootnotes.h \
	xp/ap_Dialog_FormatFrame.cpp \
	xp/ap_Dialog_FormatFrame.h \
	xp/ap_Dialog_FormatTable.cpp \
	xp/ap_Dialog_FormatTable.h \
	xp/ap_Dialog_FormatTOC.cpp \
	xp/ap_Dialog_FormatTOC.h \
	xp/ap_Dialog_Goto.cpp \
	xp/ap_Dialog_Goto.h \
	xp/ap_Dialog_HdrFtr.cpp \
	xp/ap_Dialog_HdrFtr.h \
	xp/ap_Dialog_Id.h \
	xp/ap_Dialog_GetStringCommon.cpp \
	xp/ap_Dialog_GetStringCommon.h \
	xp/ap_Dialog_InsertBookmark.cpp \
	xp/ap_Dialog_InsertBookmark.h \
	xp/ap_Dialog_InsertXMLID.cpp \
	xp/ap_Dialog_InsertXMLID.h \
	xp/ap_Dialog_Insert_DateTime.cpp \
	xp/ap_Dialog_Insert_DateTime.h \
	xp/ap_Dialog_InsertHyperlink.cpp \
	xp/ap_Dialog_InsertHyperlink.h \
	xp/ap_Dialog_InsertTable.cpp \
	xp/ap_Dialog_InsertTable.h \
	xp/ap_Dialog_Latex.cpp \
	xp/ap_Dialog_Latex.h \
	xp/ap_Dialog_ListRevisions.cpp \
	xp/ap_Dialog_ListRevisions.h \
	xp/ap_Dialog_Lists.cpp \
	xp/ap_Dialog_Lists.h \
	xp/ap_Dialog_MailMerge.cpp \
	xp/ap_Dialog_MailMerge.h \
	xp/ap_Dialog_MarkRevisions.cpp \
	xp/ap_Dialog_MarkRevisions.h \
	xp/ap_Dialog_MergeCells.cpp \
	xp/ap_Dialog_MergeCells.h \
	xp/ap_Dialog_MetaData.h \
	xp/ap_Dialog_New.cpp \
	xp/ap_Dialog_New.h \
	xp/ap_Dialog_Options.cpp \
	xp/ap_Dialog_Options.h \
	xp/ap_Dialog_PageNumbers.cpp \
	xp/ap_Dialog_PageNumbers.h \
	xp/ap_Dialog_PageSetup.cpp \
	xp/ap_Dialog_PageSetup.h \
	xp/ap_Dialog_Paragraph.cpp \
	xp/ap_Dialog_Paragraph.h \
	xp/ap_Dialog_Replace.cpp \
	xp/ap_Dialog_Replace.h \
	xp/ap_Dialog_Spell.h \
	xp/ap_Dialog_SplitCells.cpp \
	xp/ap_Dialog_SplitCells.h \
	xp/ap_Dialog_Stub.h \
	xp/ap_Dialog_Styles.cpp \
	xp/ap_Dialog_Styles.h \
	xp/ap_Dialog_Stylist.cpp \
	xp/ap_Dialog_Stylist.h \
	xp/ap_Dialog_Tab.cpp \
	xp/ap_Dialog_Tab.h \
	xp/ap_Dialog_ToggleCase.cpp \
	xp/ap_Dialog_ToggleCase.h \
	xp/ap_Dialog_WordCount.cpp \
	xp/ap_Dialog_WordCount.h \
	xp/ap_Dialog_RDFQuery.cpp \
	xp/ap_Dialog_RDFQuery.h \
	xp/ap_Dialog_RDFEditor.cpp \
	xp/ap_Dialog_RDFEditor.h \
	xp/ap_EditMethods.cpp \
	xp/ap_EditMethods.h \
	xp/ap_EmbeddedFeatures.h \
	xp/ap_Features.h \
	xp/ap_Frame.cpp \
	xp/ap_FrameData.cpp \
	xp/ap_FrameData.h \
	xp/ap_Frame.h \
	xp/ap_FrameListener.h \
	xp/ap_LB_DeadAbovedot.cpp \
	xp/ap_LB_DeadAbovedot.h \
	xp/ap_LB_DeadAcute.cpp \
	xp/ap_LB_DeadAcute.h \
	xp/ap_LB_DeadBreve.cpp \
	xp/ap_LB_DeadBreve.h \
	xp/ap_LB_DeadCaron.cpp \
	xp/ap_LB_DeadCaron.h \
	xp/ap_LB_DeadCedilla.cpp \
	xp/ap_LB_DeadCedilla.h \
	xp/ap_LB_DeadCircumflex.cpp \
	xp/ap_LB_DeadCircumflex.h \
	xp/ap_LB_DeadDiaeresis.cpp \
	xp/ap_LB_DeadDiaeresis.h \
	xp/ap_LB_DeadDoubleacute.cpp \
	xp/ap_LB_DeadDoubleacute.h \
	xp/ap_LB_DeadGrave.cpp \
	xp/ap_LB_DeadGrave.h \
	xp/ap_LB_DeadMacron.cpp \
	xp/ap_LB_DeadMacron.h \
	xp/ap_LB_DeadOgonek.cpp \
	xp/ap_LB_DeadOgonek.h \
	xp/ap_LB_DeadTilde.cpp \
	xp/ap_LB_DeadTilde.h \
	xp/ap_LB_Default.cpp \
	xp/ap_LB_Default.h \
	xp/ap_LeftRuler.cpp \
	xp/ap_LeftRuler.h \
	xp/ap_LoadBindings.cpp \
	xp/ap_LoadBindings.h \
	xp/ap_Menu_ActionSet.cpp \
	xp/ap_Menu_Functions.cpp \
	xp/ap_Menu_Functions.h \
	xp/ap_Menu_Id.h \
	xp/ap_Menu_Id_List.h \
	xp/ap_Menu_Layouts_All.h \
	xp/ap_Menu_Layouts.cpp \
	xp/ap_Menu_Layouts_Embedded.h \
	xp/ap_Menu_Layouts_MainMenu.h \
	xp/ap_ML_ContextEmbed.h \
	xp/ap_ML_ContextEquation.h \
	xp/ap_ML_ContextFrame.h \
	xp/ap_ML_ContextHyperlink.h \
	xp/ap_ML_ContextImage.h \
	xp/ap_ML_ContextPosObject.h \
	xp/ap_ML_ContextRevision.h \
	xp/ap_ML_ContextSquiggle.h \
	xp/ap_ML_ContextText.h \
	xp/ap_ML_ContextTOC.h \
	xp/ap_Prefs.cpp \
	xp/ap_Prefs.h \
	xp/ap_Prefs_SchemeIds.h \
	xp/ap_Preview_Abi.cpp \
	xp/ap_Preview_Abi.h \
	xp/ap_Preview_Annotation.cpp \
	xp/ap_Preview_Annotation.h \
	xp/ap_Preview_Paragraph.cpp \
	xp/ap_Preview_Paragraph.h \
	xp/ap_Ruler.cpp \
	xp/ap_Ruler.h \
	xp/ap_StatusBar.h \
	xp/ap_String_Id.h \
	xp/ap_Strings.cpp \
	xp/ap_Strings.h \
	xp/ap_TB_Layouts_Embedded.h \
	xp/ap_TB_Layouts_ExtraOps.h \
	xp/ap_TB_Layouts_FileEditOps.h \
	xp/ap_TB_Layouts_FormatOps.h \
	xp/ap_TB_Layouts_SimpleOps.h \
	xp/ap_TB_Layouts_TableOps.h \
	xp/ap_Toolbar_ActionSet.cpp \
	xp/ap_Toolbar_Functions.cpp \
	xp/ap_Toolbar_Functions.h \
	xp/ap_Toolbar_Iconmap.h \
	xp/ap_Toolbar_Icons_All.h \
	xp/ap_Toolbar_Icons.cpp \
	xp/ap_Toolbar_Id.h \
	xp/ap_Toolbar_Id_List.h \
	xp/ap_Toolbar_LabelSet.cpp \
	xp/ap_Toolbar_Layouts_All.h \
	xp/ap_Toolbar_Layouts.cpp \
	xp/ap_TopRuler.cpp \
	xp/ap_TopRuler.h \
	xp/ap_RDFContact.h \
	xp/ap_RDFContact.cpp \
	xp/ap_RDFEvent.h \
	xp/ap_RDFEvent.cpp \
	xp/ap_RDFLocation.h \
	xp/ap_RDFLocation.cpp \
	ap_wp_sidebar.cpp

if ENABLE_SPELL
libap_la_SOURCES += \
	xp/ap_Dialog_Spell.cpp

# grammar checking, bundled hunspell backend (formerly the grammar plugin)
AM_CPPFLAGS += \
	-I$(top_srcdir)/thirdparty/hunspell-1.7.0/src/hunspell \
	-DHUNSPELL_STATIC

libap_la_SOURCES += \
	grammar/AbiGrammar.cpp \
	grammar/AbiGrammar.h \
	grammar/AbiGrammarCheck.cpp \
	grammar/AbiGrammarCheck.h \
	grammar/AbiGrammarUtil.cpp \
	grammar/AbiGrammarUtil.h \
	grammar/HunspellWrap.cpp \
	grammar/HunspellWrap.h
endif

if ENABLE_STATUSBAR
libap_la_SOURCES += \
	xp/ap_StatusBar.cpp
endif

if ENABLE_EMACS_KEYBINDING
libap_la_SOURCES += \
	xp/ap_LB_Emacs.cpp \
	xp/ap_LB_EmacsCtrlX.cpp \
	xp/ap_LB_EmacsCtrlX.h \
	xp/ap_LB_Emacs.h
endif

if ENABLE_VI_KEYBINDING
libap_la_SOURCES += \
	xp/ap_LB_viEdit_c.cpp \
	xp/ap_LB_viEdit_c.h \
	xp/ap_LB_viEdit_colon.cpp \
	xp/ap_LB_viEdit_colon.h \
	xp/ap_LB_viEdit.cpp \
	xp/ap_LB_viEdit_d.cpp \
	xp/ap_LB_viEdit_d.h \
	xp/ap_LB_viEdit.h \
	xp/ap_LB_viEdit_r.cpp \
	xp/ap_LB_viEdit_r.h \
	xp/ap_LB_viEdit_y.cpp \
	xp/ap_LB_viEdit_y.h \
	xp/ap_LB_viInput.cpp \
	xp/ap_LB_viInput.h
endif

AM_CPPFLAGS += 						\
	$(WP_CPPFLAGS) 						\
	-DABIWORD_PLUGINSDIR="\"$(ABIWORD_PLUGINSDIR)"\"	\
	-DICONDIR="\"$(ABIWORD_ICONDIR)\"" \
        $(CHAMPLAIN_CFLAGS)

libap_la_SOURCES += \
	gtk/GTKCommon.h \
	gtk/GTKCommon.cpp \
	gtk/ap_DocView.cpp \
	gtk/ap_DocView.h \
	gtk/ap_UnixApp.cpp \
	gtk/ap_UnixApp.h \
	gtk/ap_UnixAppImpl.cpp \
	gtk/ap_UnixAppImpl.h \
	gtk/ap_UnixClipboard.cpp \
	gtk/ap_UnixClipboard.h \
	gtk/ap_UnixDialog_All.h \
	gtk/ap_UnixDialog_Annotation.cpp \
	gtk/ap_UnixDialog_Annotation.h \
	gtk/ap_UnixDialog_Background.cpp \
	gtk/ap_UnixDialog_Background.h \
	gtk/ap_UnixDialog_Border_Shading.cpp \
	gtk/ap_UnixDialog_Border_Shading.h \
	gtk/ap_UnixDialog_Break.cpp \
	gtk/ap_UnixDialog_Break.h \
	gtk/ap_UnixDialog_Columns.cpp \
	gtk/ap_UnixDialog_Columns.h \
	gtk/ap_UnixDialogFactory.cpp \
	gtk/ap_UnixDialog_Field.cpp \
	gtk/ap_UnixDialog_Field.h \
	gtk/ap_UnixDialog_FormatFootnotes.cpp \
	gtk/ap_UnixDialog_FormatFootnotes.h \
	gtk/ap_UnixDialog_FormatFrame.cpp \
	gtk/ap_UnixDialog_FormatFrame.h \
	gtk/ap_UnixDialog_FormatTable.cpp \
	gtk/ap_UnixDialog_FormatTable.h \
	gtk/ap_UnixDialog_FormatTOC.cpp \
	gtk/ap_UnixDialog_FormatTOC.h \
	gtk/ap_UnixDialog_Goto.cpp \
	gtk/ap_UnixDialog_Goto.h \
	gtk/ap_UnixDialog_HdrFtr.cpp \
	gtk/ap_UnixDialog_HdrFtr.h \
	gtk/ap_UnixDialog_InsertBookmark.cpp \
	gtk/ap_UnixDialog_InsertBookmark.h \
	gtk/ap_UnixDialog_InsertXMLID.cpp \
	gtk/ap_UnixDialog_InsertXMLID.h \
	gtk/ap_UnixDialog_Insert_DateTime.cpp \
	gtk/ap_UnixDialog_Insert_DateTime.h \
	gtk/ap_UnixDialog_InsertHyperlink.cpp \
	gtk/ap_UnixDialog_InsertHyperlink.h \
	gtk/ap_UnixDialog_InsertTable.cpp \
	gtk/ap_UnixDialog_InsertTable.h \
	gtk/ap_UnixDialog_Latex.cpp \
	gtk/ap_UnixDialog_Latex.h \
	gtk/ap_UnixDialog_ListRevisions.cpp \
	gtk/ap_UnixDialog_ListRevisions.h \
	gtk/ap_UnixDialog_Lists.cpp \
	gtk/ap_UnixDialog_Lists.h \
	gtk/ap_UnixDialog_MailMerge.cpp \
	gtk/ap_UnixDialog_MailMerge.h \
	gtk/ap_UnixDialog_MarkRevisions.cpp \
	gtk/ap_UnixDialog_MarkRevisions.h \
	gtk/ap_UnixDialog_MergeCells.cpp \
	gtk/ap_UnixDialog_MergeCells.h \
	gtk/ap_UnixDialog_MetaData.cpp \
	gtk/ap_UnixDialog_MetaData.h \
	gtk/ap_UnixDialog_New.cpp \
	gtk/ap_UnixDialog_New.h \
	gtk/ap_UnixDialog_Options.cpp \
	gtk/ap_UnixDialog_Options.h \
	gtk/ap_UnixDialog_PageNumbers.cpp \
	gtk/ap_UnixDialog_PageNumbers.h \
	gtk/ap_UnixDialog_PageSetup.cpp \
	gtk/ap_UnixDialog_PageSetup.h \
	gtk/ap_UnixDialog_Paragraph.cpp \
	gtk/ap_UnixDialog_Paragraph.h \
	gtk/ap_UnixDialog_Replace.cpp \
	gtk/ap_UnixDialog_Replace.h \
	gtk/ap_UnixDialog_Spell.h \
	gtk/ap_UnixDialog_SplitCells.cpp \
	gtk/ap_UnixDialog_SplitCells.h \
	gtk/ap_UnixDialog_Styles.cpp \
	gtk/ap_UnixDialog_Styles.h \
	gtk/ap_UnixDialog_Stylist.cpp \
	gtk/ap_UnixDialog_Stylist.h \
	gtk/ap_UnixDialog_Tab.cpp \
	gtk/ap_UnixDialog_Tab.h \
	gtk/ap_UnixDialog_ToggleCase.cpp \
	gtk/ap_UnixDialog_ToggleCase.h \
	gtk/ap_UnixDialog_WordCount.cpp \
	gtk/ap_UnixDialog_WordCount.h \
	gtk/ap_UnixDialog_RDFQuery.cpp \
	gtk/ap_UnixDialog_RDFQuery.h \
	gtk/ap_UnixDialog_RDFEditor.cpp \
	gtk/ap_UnixDialog_RDFEditor.h \
	gtk/ap_UnixFrame.cpp \
	gtk/ap_UnixFrame.h \
	gtk/ap_UnixFrameImpl.cpp \
	gtk/ap_UnixFrameImpl.h \
	gtk/ap_UnixLeftRuler.cpp \
	gtk/ap_UnixLeftRuler.h \
	gtk/ap_UnixPrefs.cpp \
	gtk/ap_UnixPrefs.h \
	gtk/ap_UnixPreview_Annotation.cpp \
	gtk/ap_UnixPreview_Annotation.h \
	gtk/ap_UnixRibbon.cpp \
	gtk/ap_UnixRibbon.h \
	gtk/ap_UnixRuler.h \
	gtk/ap_UnixRuler.cpp \
	gtk/ap_UnixStatusBar.h \
	gtk/ap_UnixStockIcons.cpp \
	gtk/ap_UnixStockIcons.h \
	gtk/ap_UnixStylesPane.cpp \
	gtk/ap_UnixStylesPane.h \
	gtk/ap_Unix_TB_CFactory.cpp \
	gtk/ap_UnixToolbar_Control_All.h \
	gtk/ap_UnixToolbar_FontCombo.cpp \
	gtk/ap_UnixToolbar_FontCombo.h \
	gtk/ap_UnixToolbar_SizeCombo.cpp \
	gtk/ap_UnixToolbar_SizeCombo.h \
	gtk/ap_UnixToolbar_StyleCombo.cpp \
	gtk/ap_UnixToolbar_StyleCombo.h \
	gtk/ap_UnixToolbar_ZoomCombo.cpp \
	gtk/ap_UnixToolbar_ZoomCombo.h \
	gtk/ap_UnixTopRuler.cpp \
	gtk/ap_UnixTopRuler.h \
	gtk/ap_UnixViewListener.cpp \
	gtk/ap_UnixViewListener.h \
	gtk/ap_RDFContactGTK.h \
	gtk/ap_RDFContactGTK.cpp \
	gtk/ap_RDFEventGTK.h \
	gtk/ap_RDFEventGTK.cpp \
	gtk/ap_RDFLocationGTK.h \
	gtk/ap_RDFLocationGTK.cpp \
	gtk/ap_RDFSemanticItemFactoryGTK.h \
	gtk/ap_RDFSemanticItemFactoryGTK.cpp \
	gtk/ap_RDFSemanticItemGTKInjected.h \
	gtk/ap_RDFSemanticItemGTKInjected.cpp \
	gtk/abiwidget.cpp \
	gtk/abiwidget.h

if ENABLE_SPELL
libap_la_SOURCES += gtk/ap_UnixDialog_Spell.cpp
endif

if ENABLE_STATUSBAR
libap_la_SOURCES += gtk/ap_UnixStatusBar.cpp
endif

if ENABLE_SPELL
endif



EXTRA_DIST = \
	xp/margin.xpm \
	xp/orient-horizontal.xpm \
	xp/orient-vertical.xpm \
	xp/sidebar.png \
	xp/ap_wp_sidebar_static.cpp \
	gtk/ap_UnixDialog_Annotation.ui \
	gtk/ap_UnixDialog_Border_Shading.ui \
	gtk/ap_UnixDialog_Break.ui \
	gtk/ap_UnixDialog_Field.ui \
	gtk/ap_UnixDialog_FormatTOC.ui \
	gtk/ap_UnixDialog_FormatFootnotes.ui \
	gtk/ap_UnixDialog_FormatTable.ui \
	gtk/ap_UnixDialog_FormatFrame.ui \
	gtk/ap_UnixDialog_Goto.ui \
	gtk/ap_UnixDialog_HdrFtr.ui \
	gtk/ap_UnixDialog_InsertTable.ui \
	gtk/ap_UnixDialog_Insert_DateTime.ui \
	gtk/ap_UnixDialog_MailMerge.ui \
	gtk/ap_UnixDialog_MetaData.ui \
	gtk/ap_UnixDialog_Options.ui \
	gtk/ap_UnixDialog_Options_ColorSel.ui \
	gtk/ap_UnixDialog_PageNumbers.ui	\
	gtk/ap_UnixDialog_PageSetup.ui \
	gtk/ap_UnixDialog_Replace.ui \
	gtk/ap_UnixDialog_New.ui \
	gtk/ap_UnixDialog_Styles.ui \
	gtk/ap_UnixDialog_Stylist.ui \
	gtk/ap_UnixDialog_Tab.ui	\
	gtk/ap_UnixDialog_WordCount.ui \
	gtk/ap_UnixDialog_Latex.ui \
	gtk/ap_UnixDialog_RDFQuery.ui \
	gtk/ap_UnixDialog_RDFEditor.ui \
	gtk/pd_RDFContact.ui \
	gtk/pd_RDFInsertReference.ui \
	gtk/pd_RDFEvent.ui \
	gtk/pd_RDFLocation.ui \
	gtk/ap_UnixDialog_SemanticStylesheets.ui \
	gtk/ap_UnixDialog_Spell.ui \
	xp/ToolbarIcons/menu_about.xpm \
	xp/ToolbarIcons/menu_add_column.xpm \
	xp/ToolbarIcons/menu_add_row.xpm \
	xp/ToolbarIcons/menu_book.xpm \
	xp/ToolbarIcons/menu_close.xpm \
	xp/ToolbarIcons/menu_copy.xpm \
	xp/ToolbarIcons/menu_credits.xpm \
	xp/ToolbarIcons/menu_cut.xpm \
	xp/ToolbarIcons/menu_delete_column.xpm \
	xp/ToolbarIcons/menu_delete_row.xpm \
	xp/ToolbarIcons/menu_delete_table.xpm \
	xp/ToolbarIcons/menu_delete.xpm \
	xp/ToolbarIcons/menu_exec.xpm \
	xp/ToolbarIcons/menu_exit.xpm \
	xp/ToolbarIcons/menu_export.xpm \
	xp/ToolbarIcons/menu_file-properties.xpm \
	xp/ToolbarIcons/menu_font.xpm \
	xp/ToolbarIcons/menu_help-bubble.xpm \
	xp/ToolbarIcons/menu_help.xpm \
	xp/ToolbarIcons/menu_hyperlink.xpm \
	xp/ToolbarIcons/menu_import.xpm \
	xp/ToolbarIcons/menu_insert_bookmark.xpm \
	xp/ToolbarIcons/menu_insert_graphic.xpm \
	xp/ToolbarIcons/menu_insert_hyperlink.xpm \
	xp/ToolbarIcons/menu_insert_image.xpm \
	xp/ToolbarIcons/menu_insert_symbol.xpm \
	xp/ToolbarIcons/menu_insert_table.xpm \
	xp/ToolbarIcons/menu_jump_to.xpm \
	xp/ToolbarIcons/menu_merge_cells.xpm \
	xp/ToolbarIcons/menu_new_window.xpm \
	xp/ToolbarIcons/menu_new.xpm \
	xp/ToolbarIcons/menu_open.xpm \
	xp/ToolbarIcons/menu_paste.xpm \
	xp/ToolbarIcons/menu_preferences.xpm \
	xp/ToolbarIcons/menu_print_preview.xpm \
	xp/ToolbarIcons/menu_print-setup.xpm \
	xp/ToolbarIcons/menu_print.xpm \
	xp/ToolbarIcons/menu_redo.xpm \
	xp/ToolbarIcons/menu_revert.xpm \
	xp/ToolbarIcons/menu_save_as.xpm \
	xp/ToolbarIcons/menu_save.xpm \
	xp/ToolbarIcons/menu_search_replace.xpm \
	xp/ToolbarIcons/menu_search.xpm \
	xp/ToolbarIcons/menu_spellcheck.xpm \
	xp/ToolbarIcons/menu_split_cells.xpm \
	xp/ToolbarIcons/menu_subscript.xpm \
	xp/ToolbarIcons/menu_superscript.xpm \
	xp/ToolbarIcons/menu_text_bold.xpm \
	xp/ToolbarIcons/menu_text_bottomline.xpm \
	xp/ToolbarIcons/menu_text_center.xpm \
	xp/ToolbarIcons/menu_text_italic.xpm \
	xp/ToolbarIcons/menu_text_justify.xpm \
	xp/ToolbarIcons/menu_text_left.xpm \
	xp/ToolbarIcons/menu_text_overline.xpm \
	xp/ToolbarIcons/menu_text_right.xpm \
	xp/ToolbarIcons/menu_text_strikethrough.xpm \
	xp/ToolbarIcons/menu_text_topline.xpm \
	xp/ToolbarIcons/menu_text_underline.xpm \
	xp/ToolbarIcons/menu_undo.xpm \
	xp/ToolbarIcons/menu_view_showpara.xpm \
	xp/ToolbarIcons/stock_new-text.xpm \
	xp/ToolbarIcons/tb_1column.xpm \
	xp/ToolbarIcons/tb_2column.xpm \
	xp/ToolbarIcons/tb_3column.xpm \
	xp/ToolbarIcons/tb_add_column.xpm \
	xp/ToolbarIcons/tb_add_row.xpm \
	xp/ToolbarIcons/tb_anchor.xpm \
	xp/ToolbarIcons/tb_copy.xpm \
	xp/ToolbarIcons/tb_cut.xpm \
	xp/ToolbarIcons/tb_delete_column.xpm \
	xp/ToolbarIcons/tb_delete_row.xpm \
	xp/ToolbarIcons/tb_edit_editfooter.xpm \
	xp/ToolbarIcons/tb_edit_editheader.xpm \
	xp/ToolbarIcons/tb_edit_removefooter.xpm \
	xp/ToolbarIcons/tb_edit_removeheader.xpm \
	xp/ToolbarIcons/tb_edit.xpm \
	xp/ToolbarIcons/tb_help.xpm \
	xp/ToolbarIcons/tb_hyperlink.xpm \
	xp/ToolbarIcons/tb_insert_graphic.xpm \
	xp/ToolbarIcons/tb_insert_table.xpm \
	xp/ToolbarIcons/tb_LineAll.xpm \
	xp/ToolbarIcons/tb_LineBottom.xpm \
	xp/ToolbarIcons/tb_line_double_space.xpm \
	xp/ToolbarIcons/tb_LineLeft.xpm \
	xp/ToolbarIcons/tb_line_middle_space.xpm \
	xp/ToolbarIcons/tb_LineNone.xpm \
	xp/ToolbarIcons/tb_LineRight.xpm \
	xp/ToolbarIcons/tb_line_single_space.xpm \
	xp/ToolbarIcons/tb_LineTop.xpm \
	xp/ToolbarIcons/tb_lists_bullets.xpm \
	xp/ToolbarIcons/tb_lists_dashed.xpm \
	xp/ToolbarIcons/tb_lists_numbers.xpm \
	xp/ToolbarIcons/tb_lists.xpm \
	xp/ToolbarIcons/tb_MergeAbove.xpm \
	xp/ToolbarIcons/tb_MergeBelow.xpm \
	xp/ToolbarIcons/tb_merge_cells.xpm \
	xp/ToolbarIcons/tb_MergeLeft.xpm \
	xp/ToolbarIcons/tb_MergeRight.xpm \
	xp/ToolbarIcons/tb_new.xpm \
	xp/ToolbarIcons/tb_open.xpm \
	xp/ToolbarIcons/tb_para_0before.xpm \
	xp/ToolbarIcons/tb_para_12before.xpm \
	xp/ToolbarIcons/tb_paste.xpm \
	xp/ToolbarIcons/tb_print_preview.xpm \
	xp/ToolbarIcons/tb_print.xpm \
	xp/ToolbarIcons/tb_redo.xpm \
	xp/ToolbarIcons/tb_remove_graphic.xpm \
	xp/ToolbarIcons/tb_save_as.xpm \
	xp/ToolbarIcons/tb_save.xpm \
	xp/ToolbarIcons/tb_script_play.xpm \
	xp/ToolbarIcons/tb_search_replace.xpm \
	xp/ToolbarIcons/tb_search.xpm \
	xp/ToolbarIcons/tb_smiley.xpm \
	xp/ToolbarIcons/tb_spellcheck.xpm \
	xp/ToolbarIcons/tb_SplitAbove.xpm \
	xp/ToolbarIcons/tb_SplitBelow.xpm \
	xp/ToolbarIcons/tb_split_cells.xpm \
	xp/ToolbarIcons/tb_SplitHoriMid.xpm \
	xp/ToolbarIcons/tb_SplitLeft.xpm \
	xp/ToolbarIcons/tb_SplitRight.xpm \
	xp/ToolbarIcons/tb_SplitVertMid.xpm \
	xp/ToolbarIcons/tb_stock_font.xpm \
	xp/ToolbarIcons/tb_stock_paint.xpm \
	xp/ToolbarIcons/tb_symbol.xpm \
	xp/ToolbarIcons/tb_text_align_left.xpm \
	xp/ToolbarIcons/tb_text_align_right.xpm \
	xp/ToolbarIcons/tb_text_bgcolor.xpm \
	xp/ToolbarIcons/tb_text_bold.xpm \
	xp/ToolbarIcons/tb_text_bottomline.xpm \
	xp/ToolbarIcons/tb_text_center.xpm \
	xp/ToolbarIcons/tb_text_direction_ltr.xpm \
	xp/ToolbarIcons/tb_text_direction_rtl.xpm \
	xp/ToolbarIcons/tb_text_dom_direction_rtl.xpm \
	xp/ToolbarIcons/tb_text_fgcolor.xpm \
	xp/ToolbarIcons/tb_text_font.xpm \
	xp/ToolbarIcons/tb_text_indent.xpm \
	xp/ToolbarIcons/tb_text_italic.xpm \
	xp/ToolbarIcons/tb_text_justify.xpm \
	xp/ToolbarIcons/tb_text_overline.xpm \
	xp/ToolbarIcons/tb_text_strikeout.xpm \
	xp/ToolbarIcons/tb_text_subscript.xpm \
	xp/ToolbarIcons/tb_text_superscript.xpm \
	xp/ToolbarIcons/tb_text_topline.xpm \
	xp/ToolbarIcons/tb_text_underline.xpm \
	xp/ToolbarIcons/tb_text_unindent.xpm \
	xp/ToolbarIcons/tb_todo.xpm \
	xp/ToolbarIcons/tb_transparent.xpm \
	xp/ToolbarIcons/tb_undo.xpm \
	xp/ToolbarIcons/tb_view_full_screen.xpm \
	xp/ToolbarIcons/tb_view_showpara.xpm \
	xp/ToolbarIcons/tb_WrapButton.xpm \
	xp/ToolbarIcons/tb_revision_new.xpm \
	xp/ToolbarIcons/tb_revision_select.xpm \
	xp/ToolbarIcons/tb_revision_show_final.xpm \
	xp/ToolbarIcons/tb_revision_find_prev.xpm \
	xp/ToolbarIcons/tb_revision_find_next.xpm \
	xp/ToolbarIcons/tb_semitem_this.xpm \
	xp/ToolbarIcons/tb_semitem_next.xpm \
	xp/ToolbarIcons/tb_semitem_prev.xpm \
	xp/ToolbarIcons/tb_semitem_edit.xpm \
	xp/ToolbarIcons/tb_semitem_stylesheet_apply.xpm

ap_wp_sidebar.cpp: $(srcdir)/xp/sidebar.png
	$(CDUMPAPP) $(srcdir)/xp/sidebar.png g_pngSidebar > ap_wp_sidebar.cpp

BUILT_SOURCES = ap_wp_sidebar.cpp \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES)
