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

noinst_LTLIBRARIES = libimpexp.la

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(srcdir)/odf/common/xp \
	-I$(srcdir)/odf/common/xp/crypto \
	-I$(srcdir)/odf/common/xp/crypto/blowfish \
	$(WP_CPPFLAGS)

libimpexp_la_SOURCES = \
	xp/ie_exp_AbiWord_1.cpp \
	xp/ie_exp_AbiWord_1.h \
	xp/ie_exp_AWT.cpp \
	xp/ie_exp_AWT.h \
	xp/ie_exp.cpp \
	xp/ie_exp.h \
	xp/ie_exp_HTML.cpp \
	xp/ie_exp_HTML.h \
	xp/ie_exp_HTML_DocumentWriter.cpp \
	xp/ie_exp_HTML_DocumentWriter.h \
	xp/ie_exp_HTML_StyleTree.cpp \
	xp/ie_exp_HTML_StyleTree.h \
	xp/ie_exp_HTML_util.cpp \
	xp/ie_exp_HTML_util.h \
	xp/ie_exp_HTML_UtilListeners.cpp \
	xp/ie_exp_HTML_UtilListeners.h \
	xp/ie_exp_HTML_Listener.cpp \
	xp/ie_exp_HTML_Listener.h \
	xp/ie_exp_HTML_NavigationHelper.cpp \
	xp/ie_exp_HTML_NavigationHelper.h \
	xp/ie_exp_RTF_AttrProp.cpp \
	xp/ie_exp_RTF_AttrProp.h \
	xp/ie_exp_RTF.cpp \
	xp/ie_exp_RTF.h \
	xp/ie_exp_RTF_listenerGetProps.cpp \
	xp/ie_exp_RTF_listenerGetProps.h \
	xp/ie_exp_RTF_listenerWriteDoc.cpp \
	xp/ie_exp_RTF_listenerWriteDoc.h \
	xp/ie_exp_Markdown.cpp \
	xp/ie_exp_Markdown.h \
	xp/ie_exp_Text.cpp \
	xp/ie_exp_Text.h \
	xp/ie_exp_XML.cpp \
	xp/ie_exp_XML.h \
	xp/ie_FileInfo.cpp \
	xp/ie_FileInfo.h \
	xp/ie_imp_AbiWord_1.cpp \
	xp/ie_imp_AbiWord_1.h \
	xp/ie_imp.cpp \
	xp/ie_impexp_AbiWord_1.h \
	xp/ie_impexp_HTML.h \
	xp/ie_impexp_MsWord_97.h \
	xp/ie_impexp_Register.cpp \
	xp/ie_impexp_Register.h \
	xp/ie_impexp_RTF.h \
	xp/ie_impexp_Text.h \
	xp/ie_imp_GraphicAsDocument.cpp \
	xp/ie_imp_GraphicAsDocument.h \
	xp/ie_impGraphic.cpp \
	xp/ie_impGraphic.h \
	xp/ie_impGraphic_PNG.cpp \
	xp/ie_impGraphic_PNG.h \
	xp/ie_impGraphic_SVG.cpp \
	xp/ie_impGraphic_SVG.h \
	xp/ie_imp.h \
	xp/ie_imp_MsWord_97.cpp \
	xp/ie_imp_MsWord_97.h \
	xp/ie_imp_MsWord_dump.h \
	xp/ie_imp_PasteListener.cpp \
	xp/ie_imp_PasteListener.h \
	xp/ie_imp_RTF.cpp \
	xp/ie_imp_RTF.h \
	xp/ie_imp_RTFKeywordIDs.h \
	xp/ie_imp_RTFKeywords.h \
	xp/ie_imp_RTFObjectsAndPicts.cpp \
	xp/ie_imp_RTFParse.cpp \
	xp/ie_imp_RTFParse.h \
	xp/ie_imp_Markdown.cpp \
	xp/ie_imp_Markdown.h \
	xp/ie_imp_Text.cpp \
	xp/ie_imp_Text.h \
	xp/ie_imp_RDF.cpp \
	xp/ie_imp_RDF.h \
	xp/ie_imp_XHTML.cpp \
	xp/ie_imp_XHTML.h \
	xp/ie_imp_XML.cpp \
	xp/ie_imp_XML.h \
	xp/ie_mailmerge.cpp \
	xp/ie_mailmerge.h \
	xp/ie_math_convert.cpp \
	xp/ie_math_convert.h \
	xp/ie_Table.cpp \
	xp/ie_Table.h \
	xp/ie_TOC.cpp \
	xp/ie_TOC.h \
	xp/ie_types.h \
	xp/MathSVGScript.h \
	xp/ie_exp_DocRangeListener.cpp \
        xp/ie_exp_DocRangeListener.h


# OpenDocument (ODF) importer/exporter, formerly a plugin
libimpexp_la_SOURCES += \
	odf/common/xp/crypto/blowfish/bf_cfb64.c \
	odf/common/xp/crypto/blowfish/bf_enc.c \
	odf/common/xp/crypto/blowfish/bf_local.h \
	odf/common/xp/crypto/blowfish/bf_pi.h \
	odf/common/xp/crypto/blowfish/bf_skey.c \
	odf/common/xp/crypto/blowfish/blowfish.h \
	odf/common/xp/crypto/blowfish/LICENSE.txt \
	odf/common/xp/crypto/blowfish/README \
	odf/common/xp/crypto/gc-pbkdf2-sha1.c \
	odf/common/xp/crypto/gc-pbkdf2-sha1.h \
	odf/common/xp/crypto/hmac.h \
	odf/common/xp/crypto/hmac-sha1.c \
	odf/common/xp/crypto/memxor.c \
	odf/common/xp/crypto/memxor.h \
	odf/common/xp/crypto/sha1.c \
	odf/common/xp/crypto/sha1.h \
	odf/common/xp/ODc_Crypto.cpp \
	odf/common/xp/ODc_Crypto.h \
	odf/common/xp/ODc_util.cpp \
	odf/common/xp/ODc_util.h \
	odf/exp/xp/ie_exp_OpenDocument.cpp \
	odf/exp/xp/ie_exp_OpenDocument.h \
	odf/exp/xp/ie_exp_OpenDocument_Sniffer.cpp \
	odf/exp/xp/ie_exp_OpenDocument_Sniffer.h \
	odf/exp/xp/ODe_AbiDocListener.cpp \
	odf/exp/xp/ODe_AbiDocListener.h \
	odf/exp/xp/ODe_AbiDocListenerImpl.cpp \
	odf/exp/xp/ODe_AbiDocListenerImpl.h \
	odf/exp/xp/ODe_AutomaticStyles.cpp \
	odf/exp/xp/ODe_AutomaticStyles.h \
	odf/exp/xp/ODe_AuxiliaryData.cpp \
	odf/exp/xp/ODe_AuxiliaryData.h \
	odf/exp/xp/ODe_Common.cpp \
	odf/exp/xp/ODe_Common.h \
	odf/exp/xp/ODe_DefaultStyles.cpp \
	odf/exp/xp/ODe_DefaultStyles.h \
	odf/exp/xp/ODe_DocumentData.cpp \
	odf/exp/xp/ODe_DocumentData.h \
	odf/exp/xp/ODe_FontFaceDecls.cpp \
	odf/exp/xp/ODe_FontFaceDecls.h \
	odf/exp/xp/ODe_Frame_Listener.cpp \
	odf/exp/xp/ODe_Frame_Listener.h \
	odf/exp/xp/ODe_HeadingSearcher_Listener.cpp \
	odf/exp/xp/ODe_HeadingSearcher_Listener.h \
	odf/exp/xp/ODe_ListenerAction.h \
	odf/exp/xp/ODe_ListLevelStyle.cpp \
	odf/exp/xp/ODe_ListLevelStyle.h \
	odf/exp/xp/ODe_Main_Listener.cpp \
	odf/exp/xp/ODe_Main_Listener.h \
	odf/exp/xp/ODe_ManifestWriter.cpp \
	odf/exp/xp/ODe_ManifestWriter.h \
	odf/exp/xp/ODe_MetaDataWriter.cpp \
	odf/exp/xp/ODe_MetaDataWriter.h \
	odf/exp/xp/ODe_Note_Listener.cpp \
	odf/exp/xp/ODe_Note_Listener.h \
	odf/exp/xp/ODe_PicturesWriter.cpp \
	odf/exp/xp/ODe_PicturesWriter.h \
	odf/exp/xp/ODe_RDFWriter.cpp \
	odf/exp/xp/ODe_RDFWriter.h \
	odf/exp/xp/ODe_SettingsWriter.cpp \
	odf/exp/xp/ODe_SettingsWriter.h \
	odf/exp/xp/ODe_Style_List.cpp \
	odf/exp/xp/ODe_Style_List.h \
	odf/exp/xp/ODe_Style_MasterPage.cpp \
	odf/exp/xp/ODe_Style_MasterPage.h \
	odf/exp/xp/ODe_Style_PageLayout.cpp \
	odf/exp/xp/ODe_Style_PageLayout.h \
	odf/exp/xp/ODe_Styles.cpp \
	odf/exp/xp/ODe_Styles.h \
	odf/exp/xp/ODe_Style_Style.cpp \
	odf/exp/xp/ODe_Style_Style.h \
	odf/exp/xp/ODe_Table_Listener.cpp \
	odf/exp/xp/ODe_Table_Listener.h \
	odf/exp/xp/ODe_Text_Listener.cpp \
	odf/exp/xp/ODe_Text_Listener.h \
	odf/exp/xp/ODe_ThumbnailsWriter.cpp \
	odf/exp/xp/ODe_ThumbnailsWriter.h \
	odf/exp/xp/ODe_TOC_Listener.cpp \
	odf/exp/xp/ODe_TOC_Listener.h \
	odf/imp/xp/ie_imp_OpenDocument.cpp \
	odf/imp/xp/ie_imp_OpenDocument.h \
	odf/imp/xp/ie_imp_OpenDocument_Sniffer.cpp \
	odf/imp/xp/ie_imp_OpenDocument_Sniffer.h \
	odf/imp/xp/ODi_Abi_Data.cpp \
	odf/imp/xp/ODi_Abi_Data.h \
	odf/imp/xp/ODi_ContentStreamAnnotationMatcher_ListenerState.cpp \
	odf/imp/xp/ODi_ContentStreamAnnotationMatcher_ListenerState.h \
	odf/imp/xp/ODi_ContentStream_ListenerState.cpp \
	odf/imp/xp/ODi_ContentStream_ListenerState.h \
	odf/imp/xp/ODi_ElementStack.cpp \
	odf/imp/xp/ODi_ElementStack.h \
	odf/imp/xp/ODi_FontFaceDecls.cpp \
	odf/imp/xp/ODi_FontFaceDecls.h \
	odf/imp/xp/ODi_Frame_ListenerState.cpp \
	odf/imp/xp/ODi_Frame_ListenerState.h \
	odf/imp/xp/ODi_ListenerStateAction.cpp \
	odf/imp/xp/ODi_ListenerStateAction.h \
	odf/imp/xp/ODi_ListenerState.h \
	odf/imp/xp/ODi_ListLevelStyle.cpp \
	odf/imp/xp/ODi_ListLevelStyleFormats.h \
	odf/imp/xp/ODi_ListLevelStyle.h \
	odf/imp/xp/ODi_ManifestStream_ListenerState.cpp \
	odf/imp/xp/ODi_ManifestStream_ListenerState.h \
	odf/imp/xp/ODi_MetaStream_ListenerState.cpp \
	odf/imp/xp/ODi_MetaStream_ListenerState.h \
	odf/imp/xp/ODi_NotesConfiguration.cpp \
	odf/imp/xp/ODi_NotesConfiguration.h \
	odf/imp/xp/ODi_Office_Styles.cpp \
	odf/imp/xp/ODi_Office_Styles.h \
	odf/imp/xp/ODi_Postpone_ListenerState.cpp \
	odf/imp/xp/ODi_Postpone_ListenerState.h \
	odf/imp/xp/ODi_RDFParser.cpp \
	odf/imp/xp/ODi_RDFParser.h \
	odf/imp/xp/ODi_SettingsStream_ListenerState.h \
	odf/imp/xp/ODi_StartTag.cpp \
	odf/imp/xp/ODi_StartTag.h \
	odf/imp/xp/ODi_StreamListener.cpp \
	odf/imp/xp/ODi_StreamListener.h \
	odf/imp/xp/ODi_Style_List.cpp \
	odf/imp/xp/ODi_Style_List.h \
	odf/imp/xp/ODi_Style_MasterPage.cpp \
	odf/imp/xp/ODi_Style_MasterPage.h \
	odf/imp/xp/ODi_Style_PageLayout.cpp \
	odf/imp/xp/ODi_Style_PageLayout.h \
	odf/imp/xp/ODi_StylesStream_ListenerState.cpp \
	odf/imp/xp/ODi_StylesStream_ListenerState.h \
	odf/imp/xp/ODi_Style_Style.cpp \
	odf/imp/xp/ODi_Style_Style_Family.cpp \
	odf/imp/xp/ODi_Style_Style_Family.h \
	odf/imp/xp/ODi_Style_Style.h \
	odf/imp/xp/ODi_Table_ListenerState.cpp \
	odf/imp/xp/ODi_Table_ListenerState.h \
	odf/imp/xp/ODi_TableOfContent_ListenerState.cpp \
	odf/imp/xp/ODi_TableOfContent_ListenerState.h \
	odf/imp/xp/ODi_TextContent_ListenerState.cpp \
	odf/imp/xp/ODi_TextContent_ListenerState.h \
	odf/imp/xp/ODi_XMLRecorder.cpp \
	odf/imp/xp/ODi_XMLRecorder.h

libimpexp_la_SOURCES += \
	gtk/ie_exp_PDF.cpp \
	gtk/ie_exp_PDF.h \
	gtk/ie_impexp_RegisterUnix.cpp \
	gtk/ie_impGraphic_GdkPixbuf.cpp \
	gtk/ie_impGraphic_GdkPixbuf.h




