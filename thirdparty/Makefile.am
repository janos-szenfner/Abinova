## Vendored third-party libraries, built as noinst convenience
## libraries so the plugins that need them link them statically
## (PIC via libtool). Users need no external downloads.

if WITH_BUNDLED_LINKGRAMMAR
LG_SUBDIR = link-grammar-5.12.5
endif
SUBDIRS = $(LG_SUBDIR)
DIST_SUBDIRS = link-grammar-5.12.5

noinst_LTLIBRARIES = \
	librevenge.la \
	libwpd.la \
	libwpg.la \
	libwps.la

AM_CXXFLAGS = -fPIC

librevenge_la_SOURCES = \
	librevenge-0.0.6/src/lib/RVNGBinaryData.cpp \
	librevenge-0.0.6/src/lib/RVNGCSVSpreadsheetGenerator.cpp \
	librevenge-0.0.6/src/lib/RVNGDirectoryStream.cpp \
	librevenge-0.0.6/src/lib/RVNGHTMLTextGenerator.cpp \
	librevenge-0.0.6/src/lib/RVNGHTMLTextTableStyle.cpp \
	librevenge-0.0.6/src/lib/RVNGHTMLTextTextStyle.cpp \
	librevenge-0.0.6/src/lib/RVNGMemoryStream.cpp \
	librevenge-0.0.6/src/lib/RVNGOLEStream.cpp \
	librevenge-0.0.6/src/lib/RVNGProperty.cpp \
	librevenge-0.0.6/src/lib/RVNGPropertyList.cpp \
	librevenge-0.0.6/src/lib/RVNGPropertyListVector.cpp \
	librevenge-0.0.6/src/lib/RVNGRawDrawingGenerator.cpp \
	librevenge-0.0.6/src/lib/RVNGRawGeneratorBase.cpp \
	librevenge-0.0.6/src/lib/RVNGRawPresentationGenerator.cpp \
	librevenge-0.0.6/src/lib/RVNGRawSpreadsheetGenerator.cpp \
	librevenge-0.0.6/src/lib/RVNGRawTextGenerator.cpp \
	librevenge-0.0.6/src/lib/RVNGSVGDrawingGenerator.cpp \
	librevenge-0.0.6/src/lib/RVNGSVGPresentationGenerator.cpp \
	librevenge-0.0.6/src/lib/RVNGStreamImplementation.cpp \
	librevenge-0.0.6/src/lib/RVNGString.cpp \
	librevenge-0.0.6/src/lib/RVNGStringVector.cpp \
	librevenge-0.0.6/src/lib/RVNGTextDrawingGenerator.cpp \
	librevenge-0.0.6/src/lib/RVNGTextPresentationGenerator.cpp \
	librevenge-0.0.6/src/lib/RVNGTextSpreadsheetGenerator.cpp \
	librevenge-0.0.6/src/lib/RVNGTextTextGenerator.cpp \
	librevenge-0.0.6/src/lib/RVNGZipStream.cpp

librevenge_la_CPPFLAGS = -I$(srcdir)/librevenge-0.0.6/inc -I$(srcdir)/librevenge-0.0.6/src/lib -DLIBREVENGE_BUILD

libwpd_la_SOURCES = \
	libwpd-0.10.3/src/lib/WP1BottomMarginGroup.cpp \
	libwpd-0.10.3/src/lib/WP1CenterTextGroup.cpp \
	libwpd-0.10.3/src/lib/WP1ContentListener.cpp \
	libwpd-0.10.3/src/lib/WP1ExtendedCharacterGroup.cpp \
	libwpd-0.10.3/src/lib/WP1FileStructure.cpp \
	libwpd-0.10.3/src/lib/WP1FixedLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WP1FlushRightGroup.cpp \
	libwpd-0.10.3/src/lib/WP1FontIdGroup.cpp \
	libwpd-0.10.3/src/lib/WP1FootnoteEndnoteGroup.cpp \
	libwpd-0.10.3/src/lib/WP1HeaderFooterGroup.cpp \
	libwpd-0.10.3/src/lib/WP1Heuristics.cpp \
	libwpd-0.10.3/src/lib/WP1JustificationGroup.cpp \
	libwpd-0.10.3/src/lib/WP1LeftIndentGroup.cpp \
	libwpd-0.10.3/src/lib/WP1LeftRightIndentGroup.cpp \
	libwpd-0.10.3/src/lib/WP1Listener.cpp \
	libwpd-0.10.3/src/lib/WP1MarginReleaseGroup.cpp \
	libwpd-0.10.3/src/lib/WP1MarginResetGroup.cpp \
	libwpd-0.10.3/src/lib/WP1Parser.cpp \
	libwpd-0.10.3/src/lib/WP1Part.cpp \
	libwpd-0.10.3/src/lib/WP1PictureGroup.cpp \
	libwpd-0.10.3/src/lib/WP1PointSizeGroup.cpp \
	libwpd-0.10.3/src/lib/WP1SetTabsGroup.cpp \
	libwpd-0.10.3/src/lib/WP1SpacingResetGroup.cpp \
	libwpd-0.10.3/src/lib/WP1StylesListener.cpp \
	libwpd-0.10.3/src/lib/WP1SubDocument.cpp \
	libwpd-0.10.3/src/lib/WP1SuppressPageCharacteristicsGroup.cpp \
	libwpd-0.10.3/src/lib/WP1TopMarginGroup.cpp \
	libwpd-0.10.3/src/lib/WP1UnsupportedFixedLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WP1UnsupportedVariableLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WP1VariableLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WP3AttributeGroup.cpp \
	libwpd-0.10.3/src/lib/WP3ContentListener.cpp \
	libwpd-0.10.3/src/lib/WP3DefinitionGroup.cpp \
	libwpd-0.10.3/src/lib/WP3DisplayGroup.cpp \
	libwpd-0.10.3/src/lib/WP3DoubleByteScriptCharacterGroup.cpp \
	libwpd-0.10.3/src/lib/WP3EndOfLinePageGroup.cpp \
	libwpd-0.10.3/src/lib/WP3ExtendedCharacterGroup.cpp \
	libwpd-0.10.3/src/lib/WP3FileStructure.cpp \
	libwpd-0.10.3/src/lib/WP3FixedLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WP3FontGroup.cpp \
	libwpd-0.10.3/src/lib/WP3FootnoteEndnoteGroup.cpp \
	libwpd-0.10.3/src/lib/WP3Header.cpp \
	libwpd-0.10.3/src/lib/WP3HeaderFooterGroup.cpp \
	libwpd-0.10.3/src/lib/WP3IndentGroup.cpp \
	libwpd-0.10.3/src/lib/WP3Listener.cpp \
	libwpd-0.10.3/src/lib/WP3MiscellaneousGroup.cpp \
	libwpd-0.10.3/src/lib/WP3PageFormatGroup.cpp \
	libwpd-0.10.3/src/lib/WP3Parser.cpp \
	libwpd-0.10.3/src/lib/WP3Part.cpp \
	libwpd-0.10.3/src/lib/WP3Resource.cpp \
	libwpd-0.10.3/src/lib/WP3ResourceFork.cpp \
	libwpd-0.10.3/src/lib/WP3SingleByteFunction.cpp \
	libwpd-0.10.3/src/lib/WP3StylesListener.cpp \
	libwpd-0.10.3/src/lib/WP3SubDocument.cpp \
	libwpd-0.10.3/src/lib/WP3TabGroup.cpp \
	libwpd-0.10.3/src/lib/WP3TablesGroup.cpp \
	libwpd-0.10.3/src/lib/WP3UndoGroup.cpp \
	libwpd-0.10.3/src/lib/WP3UnsupportedFixedLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WP3UnsupportedVariableLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WP3VariableLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WP3WindowGroup.cpp \
	libwpd-0.10.3/src/lib/WP42ContentListener.cpp \
	libwpd-0.10.3/src/lib/WP42DefineColumnsGroup.cpp \
	libwpd-0.10.3/src/lib/WP42ExtendedCharacterGroup.cpp \
	libwpd-0.10.3/src/lib/WP42FileStructure.cpp \
	libwpd-0.10.3/src/lib/WP42HeaderFooterGroup.cpp \
	libwpd-0.10.3/src/lib/WP42Heuristics.cpp \
	libwpd-0.10.3/src/lib/WP42Listener.cpp \
	libwpd-0.10.3/src/lib/WP42MarginResetGroup.cpp \
	libwpd-0.10.3/src/lib/WP42MultiByteFunctionGroup.cpp \
	libwpd-0.10.3/src/lib/WP42Parser.cpp \
	libwpd-0.10.3/src/lib/WP42Part.cpp \
	libwpd-0.10.3/src/lib/WP42StylesListener.cpp \
	libwpd-0.10.3/src/lib/WP42SubDocument.cpp \
	libwpd-0.10.3/src/lib/WP42SuppressPageCharacteristicsGroup.cpp \
	libwpd-0.10.3/src/lib/WP42UnsupportedMultiByteFunctionGroup.cpp \
	libwpd-0.10.3/src/lib/WP5AttributeGroup.cpp \
	libwpd-0.10.3/src/lib/WP5BoxGroup.cpp \
	libwpd-0.10.3/src/lib/WP5ContentListener.cpp \
	libwpd-0.10.3/src/lib/WP5DefinitionGroup.cpp \
	libwpd-0.10.3/src/lib/WP5ExtendedCharacterGroup.cpp \
	libwpd-0.10.3/src/lib/WP5FileStructure.cpp \
	libwpd-0.10.3/src/lib/WP5FixedLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WP5FontGroup.cpp \
	libwpd-0.10.3/src/lib/WP5FontNameStringPoolPacket.cpp \
	libwpd-0.10.3/src/lib/WP5FootnoteEndnoteGroup.cpp \
	libwpd-0.10.3/src/lib/WP5GeneralPacketData.cpp \
	libwpd-0.10.3/src/lib/WP5GeneralPacketIndex.cpp \
	libwpd-0.10.3/src/lib/WP5GraphicsInformationPacket.cpp \
	libwpd-0.10.3/src/lib/WP5Header.cpp \
	libwpd-0.10.3/src/lib/WP5HeaderFooterGroup.cpp \
	libwpd-0.10.3/src/lib/WP5IndentGroup.cpp \
	libwpd-0.10.3/src/lib/WP5ListFontsUsedPacket.cpp \
	libwpd-0.10.3/src/lib/WP5Listener.cpp \
	libwpd-0.10.3/src/lib/WP5PageFormatGroup.cpp \
	libwpd-0.10.3/src/lib/WP5Parser.cpp \
	libwpd-0.10.3/src/lib/WP5Part.cpp \
	libwpd-0.10.3/src/lib/WP5PrefixData.cpp \
	libwpd-0.10.3/src/lib/WP5SingleByteFunction.cpp \
	libwpd-0.10.3/src/lib/WP5SpecialHeaderIndex.cpp \
	libwpd-0.10.3/src/lib/WP5StylesListener.cpp \
	libwpd-0.10.3/src/lib/WP5SubDocument.cpp \
	libwpd-0.10.3/src/lib/WP5TabGroup.cpp \
	libwpd-0.10.3/src/lib/WP5TableEOLGroup.cpp \
	libwpd-0.10.3/src/lib/WP5TableEOPGroup.cpp \
	libwpd-0.10.3/src/lib/WP5UnsupportedFixedLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WP5UnsupportedVariableLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WP5VariableLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WP60Header.cpp \
	libwpd-0.10.3/src/lib/WP61Header.cpp \
	libwpd-0.10.3/src/lib/WP6AttributeGroup.cpp \
	libwpd-0.10.3/src/lib/WP6BoxGroup.cpp \
	libwpd-0.10.3/src/lib/WP6CharacterGroup.cpp \
	libwpd-0.10.3/src/lib/WP6ColumnGroup.cpp \
	libwpd-0.10.3/src/lib/WP6CommentAnnotationPacket.cpp \
	libwpd-0.10.3/src/lib/WP6ContentListener.cpp \
	libwpd-0.10.3/src/lib/WP6DefaultInitialFontPacket.cpp \
	libwpd-0.10.3/src/lib/WP6DisplayNumberReferenceGroup.cpp \
	libwpd-0.10.3/src/lib/WP6EOLGroup.cpp \
	libwpd-0.10.3/src/lib/WP6ExtendedCharacterGroup.cpp \
	libwpd-0.10.3/src/lib/WP6ExtendedDocumentSummaryPacket.cpp \
	libwpd-0.10.3/src/lib/WP6FileStructure.cpp \
	libwpd-0.10.3/src/lib/WP6FillStylePacket.cpp \
	libwpd-0.10.3/src/lib/WP6FixedLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WP6FontDescriptorPacket.cpp \
	libwpd-0.10.3/src/lib/WP6FootnoteEndnoteGroup.cpp \
	libwpd-0.10.3/src/lib/WP6GeneralTextPacket.cpp \
	libwpd-0.10.3/src/lib/WP6GraphicsBoxStylePacket.cpp \
	libwpd-0.10.3/src/lib/WP6GraphicsCachedFileDataPacket.cpp \
	libwpd-0.10.3/src/lib/WP6GraphicsFilenamePacket.cpp \
	libwpd-0.10.3/src/lib/WP6Header.cpp \
	libwpd-0.10.3/src/lib/WP6HeaderFooterGroup.cpp \
	libwpd-0.10.3/src/lib/WP6HighlightGroup.cpp \
	libwpd-0.10.3/src/lib/WP6HyperlinkPacket.cpp \
	libwpd-0.10.3/src/lib/WP6Listener.cpp \
	libwpd-0.10.3/src/lib/WP6NumberingMethodGroup.cpp \
	libwpd-0.10.3/src/lib/WP6OutlineStylePacket.cpp \
	libwpd-0.10.3/src/lib/WP6PageGroup.cpp \
	libwpd-0.10.3/src/lib/WP6ParagraphGroup.cpp \
	libwpd-0.10.3/src/lib/WP6Parser.cpp \
	libwpd-0.10.3/src/lib/WP6Part.cpp \
	libwpd-0.10.3/src/lib/WP6PrefixData.cpp \
	libwpd-0.10.3/src/lib/WP6PrefixDataPacket.cpp \
	libwpd-0.10.3/src/lib/WP6PrefixIndice.cpp \
	libwpd-0.10.3/src/lib/WP6SetNumberGroup.cpp \
	libwpd-0.10.3/src/lib/WP6SingleByteFunction.cpp \
	libwpd-0.10.3/src/lib/WP6StyleGroup.cpp \
	libwpd-0.10.3/src/lib/WP6StylesListener.cpp \
	libwpd-0.10.3/src/lib/WP6SubDocument.cpp \
	libwpd-0.10.3/src/lib/WP6TabGroup.cpp \
	libwpd-0.10.3/src/lib/WP6TableStylePacket.cpp \
	libwpd-0.10.3/src/lib/WP6UndoGroup.cpp \
	libwpd-0.10.3/src/lib/WP6UnsupportedFixedLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WP6UnsupportedVariableLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WP6VariableLengthGroup.cpp \
	libwpd-0.10.3/src/lib/WPDocument.cpp \
	libwpd-0.10.3/src/lib/WPXContentListener.cpp \
	libwpd-0.10.3/src/lib/WPXEncryption.cpp \
	libwpd-0.10.3/src/lib/WPXHeader.cpp \
	libwpd-0.10.3/src/lib/WPXHeaderFooter.cpp \
	libwpd-0.10.3/src/lib/WPXListener.cpp \
	libwpd-0.10.3/src/lib/WPXMemoryStream.cpp \
	libwpd-0.10.3/src/lib/WPXPageSpan.cpp \
	libwpd-0.10.3/src/lib/WPXParser.cpp \
	libwpd-0.10.3/src/lib/WPXStylesListener.cpp \
	libwpd-0.10.3/src/lib/WPXSubDocument.cpp \
	libwpd-0.10.3/src/lib/WPXTable.cpp \
	libwpd-0.10.3/src/lib/WPXTableList.cpp \
	libwpd-0.10.3/src/lib/libwpd_internal.cpp \
	libwpd-0.10.3/src/lib/libwpd_math.cpp

libwpd_la_CPPFLAGS = -I$(srcdir)/libwpd-0.10.3/inc -I$(srcdir)/librevenge-0.0.6/inc -I$(srcdir)/libwpd-0.10.3/src/lib -DLIBWPD_BUILD

libwpg_la_SOURCES = \
	libwpg-0.3.4/src/lib/WPG1Parser.cpp \
	libwpg-0.3.4/src/lib/WPG2Parser.cpp \
	libwpg-0.3.4/src/lib/WPGBitmap.cpp \
	libwpg-0.3.4/src/lib/WPGColor.cpp \
	libwpg-0.3.4/src/lib/WPGDashArray.cpp \
	libwpg-0.3.4/src/lib/WPGHeader.cpp \
	libwpg-0.3.4/src/lib/WPGTextDataHandler.cpp \
	libwpg-0.3.4/src/lib/WPGXParser.cpp \
	libwpg-0.3.4/src/lib/WPGraphics.cpp \
	libwpg-0.3.4/src/lib/libwpg_utils.cpp

libwpg_la_CPPFLAGS = -I$(srcdir)/libwpg-0.3.4/inc -I$(srcdir)/librevenge-0.0.6/inc -I$(srcdir)/libwpd-0.10.3/inc -I$(srcdir)/libwpg-0.3.4/src/lib -DLIBWPG_BUILD

libwps_la_SOURCES = \
	libwps-0.4.14/src/lib/libwps_internal.cpp \
	libwps-0.4.14/src/lib/libwps_tools_win.cpp \
	libwps-0.4.14/src/lib/DosWord.cpp \
	libwps-0.4.14/src/lib/Lotus.cpp \
	libwps-0.4.14/src/lib/LotusChart.cpp \
	libwps-0.4.14/src/lib/LotusGraph.cpp \
	libwps-0.4.14/src/lib/LotusSpreadsheet.cpp \
	libwps-0.4.14/src/lib/LotusStyleManager.cpp \
	libwps-0.4.14/src/lib/MSWrite.cpp \
	libwps-0.4.14/src/lib/Multiplan.cpp \
	libwps-0.4.14/src/lib/PocketWord.cpp \
	libwps-0.4.14/src/lib/Quattro.cpp \
	libwps-0.4.14/src/lib/QuattroFormula.cpp \
	libwps-0.4.14/src/lib/QuattroGraph.cpp \
	libwps-0.4.14/src/lib/QuattroSpreadsheet.cpp \
	libwps-0.4.14/src/lib/QuattroDos.cpp \
	libwps-0.4.14/src/lib/QuattroDosChart.cpp \
	libwps-0.4.14/src/lib/QuattroDosSpreadsheet.cpp \
	libwps-0.4.14/src/lib/Quattro9.cpp \
	libwps-0.4.14/src/lib/Quattro9Graph.cpp \
	libwps-0.4.14/src/lib/Quattro9Spreadsheet.cpp \
	libwps-0.4.14/src/lib/WKS4.cpp \
	libwps-0.4.14/src/lib/WKS4Chart.cpp \
	libwps-0.4.14/src/lib/WKS4Format.cpp \
	libwps-0.4.14/src/lib/WKS4Spreadsheet.cpp \
	libwps-0.4.14/src/lib/WKSChart.cpp \
	libwps-0.4.14/src/lib/WKSContentListener.cpp \
	libwps-0.4.14/src/lib/WKSParser.cpp \
	libwps-0.4.14/src/lib/WKSSubDocument.cpp \
	libwps-0.4.14/src/lib/WPS4.cpp \
	libwps-0.4.14/src/lib/WPS4Graph.cpp \
	libwps-0.4.14/src/lib/WPS4Text.cpp \
	libwps-0.4.14/src/lib/WPS8.cpp \
	libwps-0.4.14/src/lib/WPS8Graph.cpp \
	libwps-0.4.14/src/lib/WPS8Struct.cpp \
	libwps-0.4.14/src/lib/WPS8Table.cpp \
	libwps-0.4.14/src/lib/WPS8Text.cpp \
	libwps-0.4.14/src/lib/WPS8TextStyle.cpp \
	libwps-0.4.14/src/lib/WPSCell.cpp \
	libwps-0.4.14/src/lib/WPSContentListener.cpp \
	libwps-0.4.14/src/lib/WPSDebug.cpp \
	libwps-0.4.14/src/lib/WPSDocument.cpp \
	libwps-0.4.14/src/lib/WPSEntry.cpp \
	libwps-0.4.14/src/lib/WPSFont.cpp \
	libwps-0.4.14/src/lib/WPSGraphicShape.cpp \
	libwps-0.4.14/src/lib/WPSGraphicStyle.cpp \
	libwps-0.4.14/src/lib/WPSHeader.cpp \
	libwps-0.4.14/src/lib/WPSList.cpp \
	libwps-0.4.14/src/lib/WPSListener.cpp \
	libwps-0.4.14/src/lib/WPSOLE1Parser.cpp \
	libwps-0.4.14/src/lib/WPSOLEObject.cpp \
	libwps-0.4.14/src/lib/WPSOLEParser.cpp \
	libwps-0.4.14/src/lib/WPSOLEStream.cpp \
	libwps-0.4.14/src/lib/WPSPageSpan.cpp \
	libwps-0.4.14/src/lib/WPSParagraph.cpp \
	libwps-0.4.14/src/lib/WPSParser.cpp \
	libwps-0.4.14/src/lib/WPSStream.cpp \
	libwps-0.4.14/src/lib/WPSStringStream.cpp \
	libwps-0.4.14/src/lib/WPSSubDocument.cpp \
	libwps-0.4.14/src/lib/WPSTable.cpp \
	libwps-0.4.14/src/lib/WPSTextParser.cpp \
	libwps-0.4.14/src/lib/WPSTextSubDocument.cpp \
	libwps-0.4.14/src/lib/XYWrite.cpp

libwps_la_CPPFLAGS = -I$(srcdir)/libwps-0.4.14/inc -I$(srcdir)/librevenge-0.0.6/inc -I$(srcdir)/libwps-0.4.14/src/lib -DLIBWPS_BUILD



EXTRA_DIST = README


# vendored, security-patched wv (libwv) for .doc import --
# no system wv-1.0 required
noinst_LTLIBRARIES += libwv.la

libwv_la_SOURCES = \
	wv-1.2.9/anld.c \
	wv-1.2.9/anlv.c \
	wv-1.2.9/asumy.c \
	wv-1.2.9/asumyi.c \
	wv-1.2.9/atrd.c \
	wv-1.2.9/basename.c \
	wv-1.2.9/bintree.c \
	wv-1.2.9/bkd.c \
	wv-1.2.9/bkf.c \
	wv-1.2.9/bkl.c \
	wv-1.2.9/blip.c \
	wv-1.2.9/brc.c \
	wv-1.2.9/bte.c \
	wv-1.2.9/bx.c \
	wv-1.2.9/chp.c \
	wv-1.2.9/clx.c \
	wv-1.2.9/crc32.c \
	wv-1.2.9/dcs.c \
	wv-1.2.9/decode_complex.c \
	wv-1.2.9/decode_simple.c \
	wv-1.2.9/decompresswmf.c \
	wv-1.2.9/decrypt95.c \
	wv-1.2.9/decrypt97.c \
	wv-1.2.9/dogrid.c \
	wv-1.2.9/dop.c \
	wv-1.2.9/doptypography.c \
	wv-1.2.9/dttm.c \
	wv-1.2.9/error.c \
	wv-1.2.9/escher.c \
	wv-1.2.9/fbse.c \
	wv-1.2.9/fdoa.c \
	wv-1.2.9/ffn.c \
	wv-1.2.9/fib.c \
	wv-1.2.9/field.c \
	wv-1.2.9/filetime.c \
	wv-1.2.9/fkp.c \
	wv-1.2.9/fld.c \
	wv-1.2.9/font.c \
	wv-1.2.9/fopt.c \
	wv-1.2.9/frd.c \
	wv-1.2.9/fspa.c \
	wv-1.2.9/ftxbxs.c \
	wv-1.2.9/generic.c \
	wv-1.2.9/isbidi.c \
	wv-1.2.9/laolareplace.c \
	wv-1.2.9/lfo.c \
	wv-1.2.9/list.c \
	wv-1.2.9/lspd.c \
	wv-1.2.9/lst.c \
	wv-1.2.9/lvl.c \
	wv-1.2.9/md5.c \
	wv-1.2.9/mtextra.c \
	wv-1.2.9/numrm.c \
	wv-1.2.9/olst.c \
	wv-1.2.9/pap.c \
	wv-1.2.9/pcd.c \
	wv-1.2.9/pgd.c \
	wv-1.2.9/phe.c \
	wv-1.2.9/picf.c \
	wv-1.2.9/plcf.c \
	wv-1.2.9/prm.c \
	wv-1.2.9/rc4.c \
	wv-1.2.9/reasons.c \
	wv-1.2.9/roman.c \
	wv-1.2.9/rr.c \
	wv-1.2.9/rs.c \
	wv-1.2.9/sed.c \
	wv-1.2.9/sep.c \
	wv-1.2.9/shd.c \
	wv-1.2.9/sprm.c \
	wv-1.2.9/strcasecmp.c \
	wv-1.2.9/sttbf.c \
	wv-1.2.9/stylesheet.c \
	wv-1.2.9/support.c \
	wv-1.2.9/symbol.c \
	wv-1.2.9/table.c \
	wv-1.2.9/tap.c \
	wv-1.2.9/tbd.c \
	wv-1.2.9/tc.c \
	wv-1.2.9/text.c \
	wv-1.2.9/tlp.c \
	wv-1.2.9/twips.c \
	wv-1.2.9/unicode.c \
	wv-1.2.9/utf.c \
	wv-1.2.9/version.c \
	wv-1.2.9/winmmap.c \
	wv-1.2.9/wkb.c \
	wv-1.2.9/wvConfig.c \
	wv-1.2.9/wvHtmlEngine.c \
	wv-1.2.9/wvTextEngine.c \
	wv-1.2.9/wvparse.c \
	wv-1.2.9/xst.c

libwv_la_CPPFLAGS = -I$(srcdir)/wv-1.2.9 $(DEPS_CFLAGS)
libwv_la_CFLAGS = -fPIC -DHAVE_CONFIG_H

EXTRA_DIST += wv-1.2.9/config.h
