/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
/* Abinova
 * Copyright (C) 1998-2000 AbiSource, Inc.
 * Copyright (C) 2001 Tomas Frydrych
 * Copyright (C) 2004-2025 Hubert Figuière
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// this ansi header is not available on Windows.
// needed for close()
#include <unistd.h>
#include <signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>

#include "xap_Features.h"
#include "ap_Features.h"
#include "ap_EditMethods.h"

#include "ut_locale.h"
#include "ut_debugmsg.h"
#include "ut_string.h"
#include "ut_std_string.h"
#include "ut_bytebuf.h"
#include "ut_Language.h"
#include "ev_EditMethod.h"
#include "xav_View.h"
#include "fv_View.h"
#include "fl_DocLayout.h"
#include "fl_AutoLists.h"
#include "fp_AnnotationRun.h"
#include "fp_RDFAnchorRun.h"
#include "fp_Page.h"
#include "fp_Line.h"
#include "fg_Graphic.h"
#include "pd_Document.h"
#include "pd_Iterator.h"
#include "pf_Frag.h"
#include "pf_Frag_Strux.h"
#include "pf_Frag_Object.h"
#include "pp_Revision.h"
#include "gr_Graphics.h"
#include "gr_DrawArgs.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "xap_FrameImpl.h"
#include "xap_EditMethods.h"
#include "xap_Menu_Layouts.h"
#include "xap_Prefs.h"
#include "ap_Strings.h"
#include "ap_LoadBindings.h"
#include "ap_FrameData.h"
#include "ap_LeftRuler.h"
#include "ap_TopRuler.h"
#include "ap_Prefs.h"
#include "ut_string_class.h"

#include "ap_Dialog_Id.h"
#include "ap_Dialog_Replace.h"
#include "ap_Dialog_Goto.h"
#include "ap_Dialog_Break.h"
#include "ap_Dialog_InsertTable.h"
#include "ap_Dialog_Paragraph.h"
#include "ap_Dialog_PageNumbers.h"
#include "ap_Dialog_PageSetup.h"
#include "ap_Dialog_Lists.h"
#include "ap_Dialog_Options.h"
#include "ap_Dialog_RDFQuery.h"
#include "ap_Dialog_RDFEditor.h"

#ifdef ENABLE_SPELL
#include "ap_Dialog_Spell.h"
#endif

#include "ap_Dialog_Styles.h"
#include "ap_Dialog_Stylist.h"
#include "ap_Dialog_Insert_DateTime.h"
#include "ap_Dialog_Field.h"
#include "ap_Dialog_WordCount.h"
#include "ap_Dialog_Columns.h"
#include "ap_Dialog_ToggleCase.h"
#include "ap_Dialog_Background.h"
#include "ap_Dialog_New.h"
#include "ap_Dialog_HdrFtr.h"
#include "ap_Dialog_InsertBookmark.h"
#include "ap_Dialog_InsertHyperlink.h"
#include "ap_Dialog_InsertXMLID.h"
#include "ap_Dialog_MetaData.h"
#include "ap_Dialog_MarkRevisions.h"
#include "ap_Dialog_ListRevisions.h"
#include "ap_Dialog_MergeCells.h"
#include "ap_Dialog_SplitCells.h"
#include "ap_Dialog_FormatTable.h"
//	Maleesh 6/8/2010 - 
#include "ap_Dialog_Border_Shading.h"
#include "ap_Dialog_FormatFootnotes.h"
#include "ap_Dialog_FormatTOC.h"
#include "ap_Dialog_Latex.h"
#include "ap_Dialog_Document.h"
#include "fv_FrameEdit.h"
#include "fl_FootnoteLayout.h"
#include "gr_EmbedManager.h"
#include "fp_MathRun.h"
#include "ut_mbtowc.h"
#include "fp_EmbedRun.h"
#include "fp_FrameContainer.h"
#include "ap_Frame.h"

#include "xad_Document.h"
#include "xap_App.h"
#include "xap_DialogFactory.h"
#include "xap_Dlg_About.h"
#include "xap_Dlg_ClipArt.h"
#include "xap_Dlg_MessageBox.h"
#include "xap_Dlg_FileOpenSaveAs.h"
#include "xap_Dlg_FontChooser.h"
#include "xap_Dlg_Print.h"
#include "xap_Dlg_PrintPreview.h"
#include "xap_Dlg_WindowMore.h"
#include "xap_Dlg_Zoom.h"
#include "xap_Dlg_Insert_Symbol.h"
#include "xap_Dlg_Language.h"
#include "xap_Dlg_Image.h"
#include "xap_Dlg_ListDocuments.h"

#include "ie_imp.h"
#include "ie_impGraphic.h"
#include "ie_exp.h"
#include "ie_types.h"

#include "ut_timer.h"
#include "ut_Script.h"
#include "ut_path.h"
#include "ie_mailmerge.h"
#include "gr_Painter.h"
#include "fp_FootnoteContainer.h"


#include "ap_Dialog_Annotation.h"
#include "ap_Preview_Annotation.h"

#include "pd_DocumentRDF.h"

#include <sstream>
#include <iterator>

/*****************************************************************/
/*****************************************************************/

/* abbreviations:
**	 BOL	beginning of line
**	 EOL	end of line
**	 BOW	beginning of word
**	 EOW	end of word
**	 BOS	beginning of sentence
**	 EOS	end of sentence
**	 BOB	beginning of block
**	 EOB	end of block
**	 BOD	beginning of document
**	 EOD	end of document
**
**	 warpInsPt	  warp insertion point
**	 extSel 	  extend selection
**	 del		  delete
**	 bck		  backwards
**	 fwd		  forwards
**/

class ABI_EXPORT ap_EditMethods
{
public:
	static EV_EditMethod_Fn scrollPageDown;
	static EV_EditMethod_Fn scrollPageUp;
	static EV_EditMethod_Fn scrollPageLeft;
	static EV_EditMethod_Fn scrollPageRight;
	static EV_EditMethod_Fn scrollLineDown;
	static EV_EditMethod_Fn scrollLineUp;
	static EV_EditMethod_Fn scrollLineLeft;
	static EV_EditMethod_Fn scrollLineRight;
	static EV_EditMethod_Fn scrollToTop;
	static EV_EditMethod_Fn scrollToBottom;
	static EV_EditMethod_Fn scrollWheelMouseDown;
	static EV_EditMethod_Fn scrollWheelMouseUp;

	static EV_EditMethod_Fn warpInsPtToXY;
	static EV_EditMethod_Fn warpInsPtLeft;
	static EV_EditMethod_Fn warpInsPtRight;
	static EV_EditMethod_Fn warpInsPtBOP;
	static EV_EditMethod_Fn warpInsPtEOP;
	static EV_EditMethod_Fn warpInsPtBOL;
	static EV_EditMethod_Fn warpInsPtEOL;
	static EV_EditMethod_Fn warpInsPtBOW;
	static EV_EditMethod_Fn warpInsPtEOW;
	static EV_EditMethod_Fn warpInsPtBOS;
	static EV_EditMethod_Fn warpInsPtEOS;
	static EV_EditMethod_Fn warpInsPtBOB;
	static EV_EditMethod_Fn warpInsPtEOB;
	static EV_EditMethod_Fn warpInsPtBOD;
	static EV_EditMethod_Fn warpInsPtEOD;
	static EV_EditMethod_Fn warpInsPtPrevPage;
	static EV_EditMethod_Fn warpInsPtNextPage;
	static EV_EditMethod_Fn warpInsPtPrevScreen;
	static EV_EditMethod_Fn warpInsPtNextScreen;
	static EV_EditMethod_Fn warpInsPtPrevLine;
	static EV_EditMethod_Fn warpInsPtNextLine;

	static EV_EditMethod_Fn cairoPrint;
	static EV_EditMethod_Fn cairoPrintDirectly;
	static EV_EditMethod_Fn cairoPrintPreview;

	static EV_EditMethod_Fn coverPageInsert;
	static EV_EditMethod_Fn coverPageRemove;
	static EV_EditMethod_Fn cursorDefault;
	static EV_EditMethod_Fn cursorIBeam;
	static EV_EditMethod_Fn cursorRightArrow;
	static EV_EditMethod_Fn cursorTopCell;
	static EV_EditMethod_Fn cursorVline;
	static EV_EditMethod_Fn cursorHline;
	static EV_EditMethod_Fn cursorLeftArrow;
	static EV_EditMethod_Fn cursorImage;
	static EV_EditMethod_Fn cursorImageSize;
	static EV_EditMethod_Fn cursorTOC;

	static EV_EditMethod_Fn contextPosObject;
	static EV_EditMethod_Fn contextImage;
	static EV_EditMethod_Fn contextHyperlink;
	static EV_EditMethod_Fn contextMath;
	static EV_EditMethod_Fn contextMenu;
	static EV_EditMethod_Fn contextRevision;
	static EV_EditMethod_Fn contextTOC;
	static EV_EditMethod_Fn contextText;
#ifdef ENABLE_SPELL
	static EV_EditMethod_Fn contextMisspellText;
#endif
	static EV_EditMethod_Fn contextEmbedLayout;

#ifdef ENABLE_SPELL
	static EV_EditMethod_Fn spellSuggest_1;
	static EV_EditMethod_Fn spellSuggest_2;
	static EV_EditMethod_Fn spellSuggest_3;
	static EV_EditMethod_Fn spellSuggest_4;
	static EV_EditMethod_Fn spellSuggest_5;
	static EV_EditMethod_Fn spellSuggest_6;
	static EV_EditMethod_Fn spellSuggest_7;
	static EV_EditMethod_Fn spellSuggest_8;
	static EV_EditMethod_Fn spellSuggest_9;

	static EV_EditMethod_Fn spellIgnoreAll;
	static EV_EditMethod_Fn spellAdd;
#endif
	
	static EV_EditMethod_Fn dragToXY;
	static EV_EditMethod_Fn dragToXYword;
	static EV_EditMethod_Fn endDrag;

	static EV_EditMethod_Fn editLatexAtPos;
	static EV_EditMethod_Fn editLatexEquation;
	static EV_EditMethod_Fn editEmbed;
	static EV_EditMethod_Fn equationInsertSymbol;
	static EV_EditMethod_Fn toggleEquationDisplay;

	static EV_EditMethod_Fn extSelToXY;
	static EV_EditMethod_Fn extSelLeft;
	static EV_EditMethod_Fn extSelRight;
	static EV_EditMethod_Fn extSelBOL;
	static EV_EditMethod_Fn extSelEOL;
	static EV_EditMethod_Fn extSelBOW;
	static EV_EditMethod_Fn extSelEOW;
	static EV_EditMethod_Fn extSelBOS;
	static EV_EditMethod_Fn extSelEOS;
	static EV_EditMethod_Fn extSelBOB;
	static EV_EditMethod_Fn extSelEOB;
	static EV_EditMethod_Fn extSelBOD;
	static EV_EditMethod_Fn extSelEOD;
	static EV_EditMethod_Fn extSelPrevLine;
	static EV_EditMethod_Fn extSelNextLine;
	static EV_EditMethod_Fn extSelPageDown;
	static EV_EditMethod_Fn extSelPageUp;
	static EV_EditMethod_Fn extSelScreenUp;
	static EV_EditMethod_Fn extSelScreenDown;
	static EV_EditMethod_Fn saveImmediate;
	static EV_EditMethod_Fn selectAll;
	static EV_EditMethod_Fn selectWord;
	static EV_EditMethod_Fn selectLine;
	static EV_EditMethod_Fn selectBlock;
	static EV_EditMethod_Fn selectObject;
	static EV_EditMethod_Fn selectTable;
	static EV_EditMethod_Fn selectRow;
	static EV_EditMethod_Fn selectCell;
	static EV_EditMethod_Fn selectColumn;
	static EV_EditMethod_Fn selectColumnClick;
	static EV_EditMethod_Fn selectMath;
	static EV_EditMethod_Fn selPane;
	static EV_EditMethod_Fn selectTOC;

	static EV_EditMethod_Fn delLeft;
	static EV_EditMethod_Fn delRight;
	static EV_EditMethod_Fn delBOL;
	static EV_EditMethod_Fn delEOL;
	static EV_EditMethod_Fn delBOW;
	static EV_EditMethod_Fn delEOW;
	static EV_EditMethod_Fn delBOS;
	static EV_EditMethod_Fn delEOS;
	static EV_EditMethod_Fn delBOB;
	static EV_EditMethod_Fn delEOB;
	static EV_EditMethod_Fn delBOD;
	static EV_EditMethod_Fn delEOD;
	static EV_EditMethod_Fn delAnnotation;
	static EV_EditMethod_Fn delAllAnnotations;
	static EV_EditMethod_Fn deleteBookmark;
	static EV_EditMethod_Fn deleteXMLID;
	static EV_EditMethod_Fn deleteColumns;
	static EV_EditMethod_Fn deleteCell;
	static EV_EditMethod_Fn deleteHyperlink;
	static EV_EditMethod_Fn deleteRows;
	static EV_EditMethod_Fn deleteTable;
	static EV_EditMethod_Fn doEscape;


	static EV_EditMethod_Fn iconsPane;
	static EV_EditMethod_Fn insert3DModel;
	static EV_EditMethod_Fn insertBlankPage;
	static EV_EditMethod_Fn insertIcon;
	static EV_EditMethod_Fn insertShape;
	static EV_EditMethod_Fn insertSignatureLine;
	static EV_EditMethod_Fn insertWordArt;
	static EV_EditMethod_Fn insertBookmark;
	static EV_EditMethod_Fn insertXMLID;
	static EV_EditMethod_Fn insertFooterPreset;
	static EV_EditMethod_Fn insertHeaderPreset;
	static EV_EditMethod_Fn insertHyperlink;
	static EV_EditMethod_Fn insertColsAfter;
	static EV_EditMethod_Fn insertColsBefore;
	static EV_EditMethod_Fn insertColumnBreak;
	static EV_EditMethod_Fn insertData;
	static EV_EditMethod_Fn insertLineBreak;
	static EV_EditMethod_Fn insertParagraphBreak;
	static EV_EditMethod_Fn insertPageBreak;
	static EV_EditMethod_Fn insertRowsAfter;
	static EV_EditMethod_Fn insertRowsBefore;
	static EV_EditMethod_Fn insertSectionBreak;
	static EV_EditMethod_Fn insertSoftBreak;
	static EV_EditMethod_Fn insertSumRows;
	static EV_EditMethod_Fn insertSumCols;
	static EV_EditMethod_Fn insertTab;
	static EV_EditMethod_Fn insertTabCTL;
	static EV_EditMethod_Fn insertTabShift;

	static EV_EditMethod_Fn insertSpace;
	static EV_EditMethod_Fn insertNBSpace;
	static EV_EditMethod_Fn insertNBHyphen;
	static EV_EditMethod_Fn insertSoftHyphen;
	static EV_EditMethod_Fn insertEmDash;
	static EV_EditMethod_Fn insertEnDash;
	static EV_EditMethod_Fn insertCopyright;
	static EV_EditMethod_Fn insertRegistered;
	static EV_EditMethod_Fn insertTrademark;
	static EV_EditMethod_Fn insertNBZWSpace;
	static EV_EditMethod_Fn insertZWJoiner;
	static EV_EditMethod_Fn insertLRM;
	static EV_EditMethod_Fn insertRLM;
	static EV_EditMethod_Fn insertClosingParenthesis;
	static EV_EditMethod_Fn insertOpeningParenthesis;

	static EV_EditMethod_Fn insertGraveData; // for certain european keys
	static EV_EditMethod_Fn insertAcuteData;
	static EV_EditMethod_Fn insertCircumflexData;
	static EV_EditMethod_Fn insertTildeData;
	static EV_EditMethod_Fn insertEquation;
	static EV_EditMethod_Fn insertLatexEquation;
	static EV_EditMethod_Fn insertMacronData;
	static EV_EditMethod_Fn insertBreveData;
	static EV_EditMethod_Fn insertAbovedotData;
	static EV_EditMethod_Fn insertDiaeresisData;
	static EV_EditMethod_Fn insertDoubleacuteData;
	static EV_EditMethod_Fn insertCaronData;
	static EV_EditMethod_Fn insertCedillaData;
	static EV_EditMethod_Fn insertOgonekData;

	static EV_EditMethod_Fn mergeCells;
	static EV_EditMethod_Fn mergeCellsDir;
	static EV_EditMethod_Fn splitCells;
	static EV_EditMethod_Fn splitCellsDir;
	static EV_EditMethod_Fn formatTable;
	static EV_EditMethod_Fn autoFitTable;
	static EV_EditMethod_Fn tableColWider;
	static EV_EditMethod_Fn tableColNarrower;
	static EV_EditMethod_Fn tableRowTaller;
	static EV_EditMethod_Fn tableRowShorter;
	static EV_EditMethod_Fn splitTable;
	static EV_EditMethod_Fn cellAlignTopLeft;
	static EV_EditMethod_Fn cellAlignTopCenter;
	static EV_EditMethod_Fn cellAlignTopRight;
	static EV_EditMethod_Fn cellTextDirection;
	static EV_EditMethod_Fn cellAlignCenterLeft;
	static EV_EditMethod_Fn cellAlignCenter;
	static EV_EditMethod_Fn cellAlignCenterRight;
	static EV_EditMethod_Fn cellAlignBottomLeft;
	static EV_EditMethod_Fn cellAlignBottomCenter;
	static EV_EditMethod_Fn cellAlignBottomRight;
	static EV_EditMethod_Fn autoFitTableWindow;
	static EV_EditMethod_Fn autoFitTableFixed;
	static EV_EditMethod_Fn distributeTableCols;
	static EV_EditMethod_Fn distributeTableRows;
	static EV_EditMethod_Fn tableCellWidth;
	static EV_EditMethod_Fn tableCellHeight;
	static EV_EditMethod_Fn sortTable;
	static EV_EditMethod_Fn repeatHeaderRows;
	static EV_EditMethod_Fn tableToTextParas;
	static EV_EditMethod_Fn toggleTableGridlines;
	static EV_EditMethod_Fn toggleDrawTable;
	static EV_EditMethod_Fn toggleTableEraser;
	static EV_EditMethod_Fn borderPaintAt;
	static EV_EditMethod_Fn borderSampleAt;
	static EV_EditMethod_Fn cursorBorderPaint;
	static EV_EditMethod_Fn cursorBorderSampler;
	static EV_EditMethod_Fn beginTableDraw;
	static EV_EditMethod_Fn dragTableDraw;
	static EV_EditMethod_Fn endTableDraw;
	static EV_EditMethod_Fn eraseTableBorder;
	static EV_EditMethod_Fn cursorTableDraw;
	static EV_EditMethod_Fn cursorTableEraser;

        static EV_EditMethod_Fn repeatThisRow;
        static EV_EditMethod_Fn removeThisRowRepeat;
        static EV_EditMethod_Fn tableToTextCommas;
        static EV_EditMethod_Fn tableToTextTabs;
        static EV_EditMethod_Fn tableToTextCommasTabs;

	static EV_EditMethod_Fn tableStyle;
	static EV_EditMethod_Fn tableStyleClear;
	static EV_EditMethod_Fn tableStyleOpt;
	static EV_EditMethod_Fn tableBorder;
	static EV_EditMethod_Fn tableShading;
	static EV_EditMethod_Fn tablePen;

	static EV_EditMethod_Fn replaceChar;

	static EV_EditMethod_Fn cutVisualText;
	static EV_EditMethod_Fn copyVisualText;
	static EV_EditMethod_Fn dragVisualText;
	static EV_EditMethod_Fn pasteVisualText;
	static EV_EditMethod_Fn btn0VisualText;

	static EV_EditMethod_Fn btn1InlineImage;
	static EV_EditMethod_Fn btn0InlineImage;
	static EV_EditMethod_Fn copyInlineImage;
	static EV_EditMethod_Fn dragInlineImage;
	static EV_EditMethod_Fn releaseInlineImage;

	static EV_EditMethod_Fn btn1Frame;
	static EV_EditMethod_Fn btn0Frame;
	static EV_EditMethod_Fn dragFrame;
	static EV_EditMethod_Fn releaseFrame;
	static EV_EditMethod_Fn contextFrame;
	static EV_EditMethod_Fn deleteFrame;
	static EV_EditMethod_Fn frameBehindText;
	static EV_EditMethod_Fn frameBringForward;
	static EV_EditMethod_Fn frameBringToFront;
	static EV_EditMethod_Fn frameFlipHoriz;
	static EV_EditMethod_Fn frameFlipVert;
	static EV_EditMethod_Fn frameGroup;
	static EV_EditMethod_Fn frameInFrontOfText;
	static EV_EditMethod_Fn frameRotateLeft;
	static EV_EditMethod_Fn frameRotateRight;
	static EV_EditMethod_Fn frameRotateTo;
	static EV_EditMethod_Fn frameSendBackward;
	static EV_EditMethod_Fn frameSendToBack;
	static EV_EditMethod_Fn frameUngroup;
	static EV_EditMethod_Fn cutFrame;
	static EV_EditMethod_Fn copyFrame;
	static EV_EditMethod_Fn selectFrame;

	static EV_EditMethod_Fn beginVDrag;
	static EV_EditMethod_Fn clearSetCols;
	static EV_EditMethod_Fn dragVline;
	static EV_EditMethod_Fn dropCap;
	static EV_EditMethod_Fn endDragVline;

	static EV_EditMethod_Fn beginHDrag;
	static EV_EditMethod_Fn clearSetRows;
	static EV_EditMethod_Fn dragHline;
	static EV_EditMethod_Fn endDragHline;


	// TODO add functions for all of the standard menu commands.
	// TODO here are a few that i started.

	static EV_EditMethod_Fn fileNew;
	static EV_EditMethod_Fn fileNewUsingTemplate;
    static EV_EditMethod_Fn fileRevert;
	static EV_EditMethod_Fn fileOpen;
	static EV_EditMethod_Fn fileSave;
	static EV_EditMethod_Fn fileSaveAs;
	static EV_EditMethod_Fn fileSaveImage;
	static EV_EditMethod_Fn fileSaveEmbed;
	static EV_EditMethod_Fn fileExport;
	static EV_EditMethod_Fn fileImport;
	static EV_EditMethod_Fn importStyles;
	static EV_EditMethod_Fn formatPainter;
	static EV_EditMethod_Fn pageSetup;
	static EV_EditMethod_Fn docSettings;
	static EV_EditMethod_Fn pageMargins;
	static EV_EditMethod_Fn pageNumber;
	static EV_EditMethod_Fn pageNumberRemove;
	static EV_EditMethod_Fn pageOrientation;
	static EV_EditMethod_Fn pageSize;
	static EV_EditMethod_Fn pageColumns;
	static EV_EditMethod_Fn insColumnBreak;
	static EV_EditMethod_Fn insSectionBreak;
	static EV_EditMethod_Fn paraProp;
	static EV_EditMethod_Fn sectProps;
	static EV_EditMethod_Fn docProps;
	static EV_EditMethod_Fn arrangePosition;
	static EV_EditMethod_Fn wrapObject;
	static EV_EditMethod_Fn prevComment;
	static EV_EditMethod_Fn print;
	static EV_EditMethod_Fn printTB;
	static EV_EditMethod_Fn printPreview;
	static EV_EditMethod_Fn printDirectly;
	static EV_EditMethod_Fn fileInsertGraphic;
	static EV_EditMethod_Fn fileInsertPositionedGraphic;
	static EV_EditMethod_Fn fileInsertPageBackgroundGraphic;
	static EV_EditMethod_Fn insertClipart;
	static EV_EditMethod_Fn fileSaveAsWeb;
    static EV_EditMethod_Fn fileSaveTemplate;
	static EV_EditMethod_Fn openTemplate;

	static EV_EditMethod_Fn undo;
	static EV_EditMethod_Fn redo;
	static EV_EditMethod_Fn cut;
	static EV_EditMethod_Fn copy;
	static EV_EditMethod_Fn paraSortAscend;
	static EV_EditMethod_Fn paraSortDescend;
	static EV_EditMethod_Fn paste;
	static EV_EditMethod_Fn pasteSelection;
	static EV_EditMethod_Fn pasteSpecial;
	static EV_EditMethod_Fn find;
	static EV_EditMethod_Fn findAgain;
	static EV_EditMethod_Fn go;
	static EV_EditMethod_Fn replace;
	static EV_EditMethod_Fn editHeader;
	static EV_EditMethod_Fn editFooter;
	static EV_EditMethod_Fn removeHeader;
	static EV_EditMethod_Fn removeFooter;

	static EV_EditMethod_Fn revisionNew;
	static EV_EditMethod_Fn revisionSelect;
	static EV_EditMethod_Fn resolveAnnotation;

	static EV_EditMethod_Fn refCaption;
	static EV_EditMethod_Fn refDeleteSource;
	static EV_EditMethod_Fn refInsertBibliography;
	static EV_EditMethod_Fn refInsertCitation;
	static EV_EditMethod_Fn refInsertIndex;
	static EV_EditMethod_Fn refInsertTOA;
	static EV_EditMethod_Fn refInsertTOF;
	static EV_EditMethod_Fn refMarkCitation;
	static EV_EditMethod_Fn refMarkEntry;
	static EV_EditMethod_Fn refRemoveBibliography;
	static EV_EditMethod_Fn refRemoveIndex;
	static EV_EditMethod_Fn refRemoveTOA;
	static EV_EditMethod_Fn refXRef;

	static EV_EditMethod_Fn viewStd;
	static EV_EditMethod_Fn viewFormat;
	static EV_EditMethod_Fn viewExtra;
	static EV_EditMethod_Fn viewTable;
	static EV_EditMethod_Fn viewTB1;
	static EV_EditMethod_Fn viewTB2;
	static EV_EditMethod_Fn viewTB3;
	static EV_EditMethod_Fn viewTB4;
	static EV_EditMethod_Fn lockToolbarLayout;
	static EV_EditMethod_Fn defaultToolbarLayout;
	static EV_EditMethod_Fn viewRuler;
	static EV_EditMethod_Fn viewStatus;
	static EV_EditMethod_Fn viewPara;
	static EV_EditMethod_Fn viewLockStyles;
	static EV_EditMethod_Fn viewHeadFoot;
	static EV_EditMethod_Fn zoom;
	static EV_EditMethod_Fn dlgZoom;
	static EV_EditMethod_Fn viewFullScreen;
	static EV_EditMethod_Fn viewGridlines;
	static EV_EditMethod_Fn viewNavPane;
	static EV_EditMethod_Fn viewSplit;
	static EV_EditMethod_Fn arrangeAll;

	static EV_EditMethod_Fn zoom100;
	static EV_EditMethod_Fn zoom200;
	static EV_EditMethod_Fn zoom75;
	static EV_EditMethod_Fn zoom50;
	static EV_EditMethod_Fn zoomWidth;
	static EV_EditMethod_Fn zoomWhole;
	static EV_EditMethod_Fn zoomIn;
	static EV_EditMethod_Fn zoomOut;

	static EV_EditMethod_Fn insBreak;
	static EV_EditMethod_Fn insPageNo;
	static EV_EditMethod_Fn insMediaFile;
	static EV_EditMethod_Fn insScreenshot;
	static EV_EditMethod_Fn insVerticalTextBox;
	static EV_EditMethod_Fn insDateTime;
	static EV_EditMethod_Fn insField;
	static EV_EditMethod_Fn insTextBox;
	static EV_EditMethod_Fn insSymbol;
	static EV_EditMethod_Fn insFile;
	static EV_EditMethod_Fn insTOC;
	static EV_EditMethod_Fn insFootnote;
	static EV_EditMethod_Fn insEndnote;

#ifdef ENABLE_SPELL
	static EV_EditMethod_Fn dlgSpell;
	static EV_EditMethod_Fn dlgSpellPrefs;
#endif
	
	static EV_EditMethod_Fn dlgWordCount;
	static EV_EditMethod_Fn dlgOptions;
    static EV_EditMethod_Fn dlgMetaData;

	static EV_EditMethod_Fn dlgFont;
	static EV_EditMethod_Fn dlgParagraph;
	static EV_EditMethod_Fn dlgBullets;
	static EV_EditMethod_Fn dlgBorders;
	static EV_EditMethod_Fn dlgColumns;
	static EV_EditMethod_Fn dlgFmtPosImage;
	static EV_EditMethod_Fn setPosImage;
	static EV_EditMethod_Fn dlgHdrFtr;
	static EV_EditMethod_Fn style;
	static EV_EditMethod_Fn dlgBackground;
	static EV_EditMethod_Fn dlgStyle;
	static EV_EditMethod_Fn formatTOC;
	static EV_EditMethod_Fn formatFootnotes;
	static EV_EditMethod_Fn dlgToggleCase;
	static EV_EditMethod_Fn rotateCase;
	static EV_EditMethod_Fn dlgLanguage;
	static EV_EditMethod_Fn dlgColorPickerFore;
	static EV_EditMethod_Fn dlgColorPickerBack;
	static EV_EditMethod_Fn language;
	static EV_EditMethod_Fn fontFamily;
	static EV_EditMethod_Fn fontSize;
	static EV_EditMethod_Fn fontSizeIncrease;
	static EV_EditMethod_Fn fontSizeDecrease;
	static EV_EditMethod_Fn caseLower;
	static EV_EditMethod_Fn caseSentence;
	static EV_EditMethod_Fn caseTitle;
	static EV_EditMethod_Fn caseToggle;
	static EV_EditMethod_Fn caseUpper;
	static EV_EditMethod_Fn clearFormatting;
	static EV_EditMethod_Fn clearParaFormatting;
	static EV_EditMethod_Fn toggleBold;
	static EV_EditMethod_Fn toggleDisplayAnnotations;
	static EV_EditMethod_Fn toggleAutoGrammar;
	static EV_EditMethod_Fn toggleHidden;
	static EV_EditMethod_Fn toggleItalic;
	static EV_EditMethod_Fn toggleUline;
	static EV_EditMethod_Fn toggleOline;
	static EV_EditMethod_Fn toggleStrike;
	static EV_EditMethod_Fn toggleTopline;
	static EV_EditMethod_Fn toggleBottomline;
	static EV_EditMethod_Fn toggleSuper;
	static EV_EditMethod_Fn toggleSub;
	static EV_EditMethod_Fn togglePlain;
	static EV_EditMethod_Fn toggleDirOverrideLTR;
	static EV_EditMethod_Fn toggleDirOverrideRTL;
	static EV_EditMethod_Fn toggleRDFAnchorHighlight;

	static EV_EditMethod_Fn doBullets;
	static EV_EditMethod_Fn doNumbers;
	static EV_EditMethod_Fn doDashedList;
	static EV_EditMethod_Fn doListType;
	static EV_EditMethod_Fn paraBorder;

	static EV_EditMethod_Fn colorForeTB;
	static EV_EditMethod_Fn colorBackTB;

	static EV_EditMethod_Fn toggleIndent;
	static EV_EditMethod_Fn toggleUnIndent;

	static EV_EditMethod_Fn alignLeft;
	static EV_EditMethod_Fn alignCenter;
	static EV_EditMethod_Fn alignRight;
	static EV_EditMethod_Fn alignJustify;

	static EV_EditMethod_Fn setStyleHeading1;
	static EV_EditMethod_Fn setStyleHeading2;
	static EV_EditMethod_Fn setStyleHeading3;
	static EV_EditMethod_Fn setStyleNormal;

	static EV_EditMethod_Fn paraBefore0;
	static EV_EditMethod_Fn paraBefore12;
	static EV_EditMethod_Fn toggleParaBefore;

	static EV_EditMethod_Fn sectColumns1;
	static EV_EditMethod_Fn sectColumns2;
	static EV_EditMethod_Fn sectColumns3;

	static EV_EditMethod_Fn singleSpace;
	static EV_EditMethod_Fn middleSpace;
	static EV_EditMethod_Fn doubleSpace;


	static EV_EditMethod_Fn activateWindow_1;
	static EV_EditMethod_Fn activateWindow_2;
	static EV_EditMethod_Fn activateWindow_3;
	static EV_EditMethod_Fn activateWindow_4;
	static EV_EditMethod_Fn activateWindow_5;
	static EV_EditMethod_Fn activateWindow_6;
	static EV_EditMethod_Fn activateWindow_7;
	static EV_EditMethod_Fn activateWindow_8;
	static EV_EditMethod_Fn activateWindow_9;
	static EV_EditMethod_Fn dlgMoreWindows;

	static EV_EditMethod_Fn dlgAbout;
	static EV_EditMethod_Fn helpChangelog;
	static EV_EditMethod_Fn helpContents;
	static EV_EditMethod_Fn helpIntro;
	static EV_EditMethod_Fn helpSearch;
	static EV_EditMethod_Fn helpCheckVer;
	static EV_EditMethod_Fn helpReportBug;

	static EV_EditMethod_Fn newWindow;
	static EV_EditMethod_Fn nextComment;
	static EV_EditMethod_Fn notImplemented;
	static EV_EditMethod_Fn cycleWindows;
	static EV_EditMethod_Fn cycleWindowsBck;
	static EV_EditMethod_Fn closeWindow;
	static EV_EditMethod_Fn closeWindowX;
	static EV_EditMethod_Fn querySaveAndExit;

	static EV_EditMethod_Fn setEditVI;
	static EV_EditMethod_Fn setInputVI;
	static EV_EditMethod_Fn cycleInputMode;
	static EV_EditMethod_Fn toggleInsertMode;

	static EV_EditMethod_Fn	viCmd_5e;
	static EV_EditMethod_Fn viCmd_A;
	static EV_EditMethod_Fn viCmd_C;
	static EV_EditMethod_Fn viCmd_I;
	static EV_EditMethod_Fn viCmd_J;
	static EV_EditMethod_Fn viCmd_O;
	static EV_EditMethod_Fn viCmd_P;
	static EV_EditMethod_Fn viCmd_a;
	static EV_EditMethod_Fn viCmd_c24;
	static EV_EditMethod_Fn viCmd_c28;
	static EV_EditMethod_Fn viCmd_c29;
	static EV_EditMethod_Fn viCmd_c5b;
	static EV_EditMethod_Fn viCmd_c5d;
	static EV_EditMethod_Fn viCmd_c5e;
	static EV_EditMethod_Fn viCmd_cb;
	static EV_EditMethod_Fn viCmd_cw;
	static EV_EditMethod_Fn viCmd_d24;
	static EV_EditMethod_Fn viCmd_d28;
	static EV_EditMethod_Fn viCmd_d29;
	static EV_EditMethod_Fn viCmd_d5b;
	static EV_EditMethod_Fn viCmd_d5d;
	static EV_EditMethod_Fn viCmd_d5e;
	static EV_EditMethod_Fn viCmd_db;
	static EV_EditMethod_Fn viCmd_dd;
	static EV_EditMethod_Fn viCmd_dw;
	static EV_EditMethod_Fn viCmd_o;
	static EV_EditMethod_Fn viCmd_y24;
	static EV_EditMethod_Fn viCmd_y28;
	static EV_EditMethod_Fn viCmd_y29;
	static EV_EditMethod_Fn viCmd_y5b;
	static EV_EditMethod_Fn viCmd_y5d;
	static EV_EditMethod_Fn viCmd_y5e;
	static EV_EditMethod_Fn viCmd_yb;
	static EV_EditMethod_Fn viCmd_yw;
	static EV_EditMethod_Fn viCmd_yy;

	static EV_EditMethod_Fn viewNormalLayout;
	static EV_EditMethod_Fn viewPrintLayout;
	static EV_EditMethod_Fn viewWebLayout;

#ifdef ENABLE_SPELL
	static EV_EditMethod_Fn toggleAutoSpell;
#endif
	
	static EV_EditMethod_Fn executeScript;

        static EV_EditMethod_Fn mailMerge;

	static EV_EditMethod_Fn hyperlinkCopyLocation;
	static EV_EditMethod_Fn hyperlinkJump;
	static EV_EditMethod_Fn hyperlinkJumpPos;
	static EV_EditMethod_Fn hyperlinkStatusBar;
	static EV_EditMethod_Fn rdfAnchorEditTriples;
	static EV_EditMethod_Fn rdfAnchorQuery;
	static EV_EditMethod_Fn rdfAnchorEditSemanticItem;
	static EV_EditMethod_Fn rdfAnchorExportSemanticItem;
	static EV_EditMethod_Fn rdfAnchorSelectThisReferenceToSemanticItem;
	static EV_EditMethod_Fn rdfAnchorSelectNextReferenceToSemanticItem;
	static EV_EditMethod_Fn rdfAnchorSelectPrevReferenceToSemanticItem;
	static EV_EditMethod_Fn rdfApplyStylesheetContactName;
	static EV_EditMethod_Fn rdfApplyStylesheetContactNick;
	static EV_EditMethod_Fn rdfApplyStylesheetContactNamePhone;
	static EV_EditMethod_Fn rdfApplyStylesheetContactNickPhone;
	static EV_EditMethod_Fn rdfApplyStylesheetContactNameHomepagePhone;
	static EV_EditMethod_Fn rdfApplyStylesheetEventName;
	static EV_EditMethod_Fn rdfApplyStylesheetEventSummary;
	static EV_EditMethod_Fn rdfApplyStylesheetEventSummaryLocation;
	static EV_EditMethod_Fn rdfApplyStylesheetEventSummaryLocationTimes;
	static EV_EditMethod_Fn rdfApplyStylesheetEventSummaryTimes;
	static EV_EditMethod_Fn rdfApplyStylesheetLocationName;
	static EV_EditMethod_Fn rdfApplyStylesheetLocationLatLong;
	static EV_EditMethod_Fn rdfApplyCurrentStyleSheet;
	static EV_EditMethod_Fn rdfStylesheetSettings;
	static EV_EditMethod_Fn rdfDisassocateCurrentStyleSheet;
	static EV_EditMethod_Fn rdfSemitemSetAsSource;
	static EV_EditMethod_Fn rdfSemitemRelatedToSourceFoafKnows;
	static EV_EditMethod_Fn rdfSemitemFindRelatedFoafKnows;
	static EV_EditMethod_Fn tocAddText;
	static EV_EditMethod_Fn tocInsert;
	static EV_EditMethod_Fn tocRemove;
	static EV_EditMethod_Fn tocUpdate;
	static EV_EditMethod_Fn footnoteNext;
	static EV_EditMethod_Fn footnotePrev;
	static EV_EditMethod_Fn endnoteNext;
	static EV_EditMethod_Fn endnotePrev;
	static EV_EditMethod_Fn showNotes;
	static EV_EditMethod_Fn footnoteToEndnote;
	static EV_EditMethod_Fn endnoteToFootnote;
	static EV_EditMethod_Fn noteSwap;
	static EV_EditMethod_Fn toggleMarkRevisions;
	static EV_EditMethod_Fn toggleAutoRevision;
	static EV_EditMethod_Fn revisionAccept;
	static EV_EditMethod_Fn revisionAcceptAll;
	static EV_EditMethod_Fn revisionAcceptAllShown;
	static EV_EditMethod_Fn revisionAcceptAllStopTracking;
	static EV_EditMethod_Fn revisionAcceptNext;
	static EV_EditMethod_Fn revisionReject;
	static EV_EditMethod_Fn revisionRejectAll;
	static EV_EditMethod_Fn revisionRejectAllShown;
	static EV_EditMethod_Fn revisionRejectAllStopTracking;
	static EV_EditMethod_Fn revisionRejectNext;
	static EV_EditMethod_Fn revisionDisplayMode;
	static EV_EditMethod_Fn revisionFindNext;
	static EV_EditMethod_Fn revisionFindPrev;
	static EV_EditMethod_Fn revisionSetViewLevel;
	static EV_EditMethod_Fn revisionCombineDocuments;
	static EV_EditMethod_Fn toggleShowRevisions;
	static EV_EditMethod_Fn toggleShowRevisionsBefore;
	static EV_EditMethod_Fn toggleShowRevisionsAfter;
	static EV_EditMethod_Fn toggleShowRevisionsAfterPrevious;
	static EV_EditMethod_Fn revisionCompareDocuments;
	
    static EV_EditMethod_Fn insAnnotation;
    static EV_EditMethod_Fn insAnnotationFromSel;
    static EV_EditMethod_Fn commentsPane;
    static EV_EditMethod_Fn editAnnotation;
	
	static EV_EditMethod_Fn sortColsAscend;
	static EV_EditMethod_Fn sortColsDescend;
	static EV_EditMethod_Fn sortRowsAscend;
	static EV_EditMethod_Fn sortRowsDescend;


	
	static EV_EditMethod_Fn insertTable;

#ifdef DEBUG
	static EV_EditMethod_Fn dumpRDFForPoint;
	static EV_EditMethod_Fn dumpRDFObjects;
	static EV_EditMethod_Fn rdfTest;
	static EV_EditMethod_Fn rdfPlay;
#endif
	static EV_EditMethod_Fn rdfQuery;
	static EV_EditMethod_Fn rdfEditor;
	static EV_EditMethod_Fn rdfQueryXMLIDs;
 	static EV_EditMethod_Fn rdfInsertRef;
	static EV_EditMethod_Fn rdfInsertNewContact;
	static EV_EditMethod_Fn rdfInsertNewContactFromFile;
	
	static EV_EditMethod_Fn noop;

	// Test routines

#if defined(PT_TEST) || defined(FMT_TEST) || defined(UT_TEST)
	static EV_EditMethod_Fn Test_Dump;
	static EV_EditMethod_Fn Test_Ftr;
#endif

};

/*****************************************************************/
/*****************************************************************/

#define _D_ 			EV_EMT_REQUIREDATA
#define _A_				EV_EMT_APP_METHOD

#define F(fn)			ap_EditMethods::fn
#define N(fn)			#fn
#define NF(fn)			N(fn), F(fn)

// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!!!!!!!!!!!!!!!
//
// keep this array alphabetically (strcmp) ordered under
// penalty of being forced to port Abi to the PalmOS
//
// YOUR NEW METHOD WON'T BE FOUND AND YOU'LL SCREW UP ALL THE OTHER METHODS
// IF YOU DON'T DO THIS
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
static EV_EditMethod s_arrayEditMethods[] =
{
#if defined(PT_TEST) || defined(FMT_TEST) || defined(UT_TEST)
	EV_EditMethod(NF(Test_Dump),			0,	""),
	EV_EditMethod(NF(Test_Ftr), 		0,	""),
#endif

	// a
	EV_EditMethod(NF(activateWindow_1), 	0,		""),
	EV_EditMethod(NF(activateWindow_2), 	0,		""),
	EV_EditMethod(NF(activateWindow_3), 	0,		""),
	EV_EditMethod(NF(activateWindow_4), 	0,		""),
	EV_EditMethod(NF(activateWindow_5), 	0,		""),
	EV_EditMethod(NF(activateWindow_6), 	0,		""),
	EV_EditMethod(NF(activateWindow_7), 	0,		""),
	EV_EditMethod(NF(activateWindow_8), 	0,		""),
	EV_EditMethod(NF(activateWindow_9), 	0,		""),
	EV_EditMethod(NF(alignCenter),			0,		""),
	EV_EditMethod(NF(alignJustify), 		0,		""),
	EV_EditMethod(NF(alignLeft),			0,		""),
	EV_EditMethod(NF(alignRight),			0,		""),
	EV_EditMethod(NF(arrangeAll), 0, ""),
	EV_EditMethod(NF(arrangePosition),		0,	""),
	EV_EditMethod(NF(autoFitTable),         0,      ""),
	EV_EditMethod(NF(autoFitTableFixed),    0,      ""),
	EV_EditMethod(NF(autoFitTableWindow),   0,      ""),

	// b
	EV_EditMethod(NF(beginHDrag), 0, ""),
	EV_EditMethod(NF(beginTableDraw), 0, ""),
	EV_EditMethod(NF(beginVDrag), 0, ""),
	EV_EditMethod(NF(borderPaintAt),		0,	""),
	EV_EditMethod(NF(borderSampleAt),		0,	""),
	EV_EditMethod(NF(btn0Frame), 0, ""),
	EV_EditMethod(NF(btn0InlineImage), 0, ""),
	EV_EditMethod(NF(btn0VisualText), 0, ""),
	EV_EditMethod(NF(btn1Frame), 0, ""),
	EV_EditMethod(NF(btn1InlineImage), 0, ""),

	// c
	EV_EditMethod(NF(cairoPrint), 0, ""),
	EV_EditMethod(NF(cairoPrintDirectly), 0, ""),
	EV_EditMethod(NF(cairoPrintPreview), 0, ""),
	EV_EditMethod(NF(caseLower),			0,	""),
	EV_EditMethod(NF(caseSentence),			0,	""),
	EV_EditMethod(NF(caseTitle),			0,	""),
	EV_EditMethod(NF(caseToggle),			0,	""),
	EV_EditMethod(NF(caseUpper),			0,	""),
	EV_EditMethod(NF(cellAlignBottomCenter),0,	""),
	EV_EditMethod(NF(cellAlignBottomLeft),	0,	""),
	EV_EditMethod(NF(cellAlignBottomRight),	0,	""),
	EV_EditMethod(NF(cellAlignCenter),		0,	""),
	EV_EditMethod(NF(cellAlignCenterLeft),	0,	""),
	EV_EditMethod(NF(cellAlignCenterRight),	0,	""),
	EV_EditMethod(NF(cellAlignTopCenter),	0,	""),
	EV_EditMethod(NF(cellAlignTopLeft),		0,	""),
	EV_EditMethod(NF(cellAlignTopRight),	0,	""),
	EV_EditMethod(NF(cellTextDirection),	_D_,""),
	EV_EditMethod(NF(clearFormatting),		0,	""),
	EV_EditMethod(NF(clearParaFormatting),	0,	""),
	EV_EditMethod(NF(clearSetCols), 0, ""),
	EV_EditMethod(NF(clearSetRows), 0, ""),
	EV_EditMethod(NF(closeWindow),			0,	""),
	EV_EditMethod(NF(closeWindowX), 0, ""),
	EV_EditMethod(NF(colorBackTB), _D_, ""),
	EV_EditMethod(NF(colorForeTB), _D_, ""),
	EV_EditMethod(NF(commentsPane),			0,		""),
	EV_EditMethod(NF(contextEmbedLayout), 		0,	""),
	EV_EditMethod(NF(contextFrame), 		0,	""),
	EV_EditMethod(NF(contextHyperlink), 		0,	""),
	EV_EditMethod(NF(contextImage), 0, ""),
	EV_EditMethod(NF(contextMath),			0,	""),
	EV_EditMethod(NF(contextMenu),			0,	""),
#ifdef ENABLE_SPELL
	EV_EditMethod(NF(contextMisspellText),	0,	""),
#endif
	EV_EditMethod(NF(contextPosObject), 0, ""),
	EV_EditMethod(NF(contextRevision),	    0,	""),
	EV_EditMethod(NF(contextTOC),			0,	""),
	EV_EditMethod(NF(contextText),			0,	""),
	EV_EditMethod(NF(copy), 				0,	""),
	EV_EditMethod(NF(copyFrame), 				0,	""),
	EV_EditMethod(NF(copyInlineImage), 				0,	""),
	EV_EditMethod(NF(copyVisualText),		0,	""),
	EV_EditMethod(NF(coverPageInsert),		0,	""),
	EV_EditMethod(NF(coverPageRemove),		0,	""),
	EV_EditMethod(NF(cursorBorderPaint),	0,	""),
	EV_EditMethod(NF(cursorBorderSampler),	0,	""),
	EV_EditMethod(NF(cursorDefault),		0,	""),
	EV_EditMethod(NF(cursorHline),      	0,	""),
	EV_EditMethod(NF(cursorIBeam),			0,	""),
	EV_EditMethod(NF(cursorImage),			0,	""),
	EV_EditMethod(NF(cursorImageSize),		0,	""),
	EV_EditMethod(NF(cursorLeftArrow),		0,	""),
	EV_EditMethod(NF(cursorRightArrow), 	0,	""),
	EV_EditMethod(NF(cursorTOC), 	0,	""),
	EV_EditMethod(NF(cursorTableDraw), 	0,	""),
	EV_EditMethod(NF(cursorTableEraser), 	0,	""),
	EV_EditMethod(NF(cursorTopCell), 	0,	""),
	EV_EditMethod(NF(cursorVline), 	        0,	""),
	EV_EditMethod(NF(cut),					0,	""),
	EV_EditMethod(NF(cutFrame),					0,	""),
	EV_EditMethod(NF(cutVisualText),		0,	""),
	EV_EditMethod(NF(cycleInputMode),		0,	""),
	EV_EditMethod(NF(cycleWindows), 		0,	""),
	EV_EditMethod(NF(cycleWindowsBck),		0,	""),

	// d
	EV_EditMethod(NF(defaultToolbarLayout),			0,	""),
	EV_EditMethod(NF(delAllAnnotations),	0,	""),
	EV_EditMethod(NF(delAnnotation),		0,	""),
	EV_EditMethod(NF(delBOB),				0,	""),
	EV_EditMethod(NF(delBOD),				0,	""),
	EV_EditMethod(NF(delBOL),				0,	""),
	EV_EditMethod(NF(delBOS),				0,	""),
	EV_EditMethod(NF(delBOW),				0,	""),
	EV_EditMethod(NF(delEOB),				0,	""),
	EV_EditMethod(NF(delEOD),				0,	""),
	EV_EditMethod(NF(delEOL),				0,	""),
	EV_EditMethod(NF(delEOS),				0,	""),
	EV_EditMethod(NF(delEOW),				0,	""),
	EV_EditMethod(NF(delLeft),				0,	""),
	EV_EditMethod(NF(delRight), 			0,	""),
	EV_EditMethod(NF(deleteBookmark),		0,	""),
	EV_EditMethod(NF(deleteCell),   		0,	""),
	EV_EditMethod(NF(deleteColumns),   		0,	""),
	EV_EditMethod(NF(deleteFrame),   		0,	""),
	EV_EditMethod(NF(deleteHyperlink),		0,	""),
	EV_EditMethod(NF(deleteRows),   		0,	""),
	EV_EditMethod(NF(deleteTable),   		0,	""),
	EV_EditMethod(NF(deleteXMLID),  		0,	""),
	EV_EditMethod(NF(distributeTableCols),	0,	""),
	EV_EditMethod(NF(distributeTableRows),	0,	""),
	EV_EditMethod(NF(dlgAbout), 			_A_, ""),
	EV_EditMethod(NF(dlgBackground),		0,	""),
	EV_EditMethod(NF(dlgBorders),			0,	""),
	EV_EditMethod(NF(dlgBullets),			0,	""),
	EV_EditMethod(NF(dlgColorPickerBack),	0,	""),
	EV_EditMethod(NF(dlgColorPickerFore),	0,	""),
	EV_EditMethod(NF(dlgColumns),			0,	""),
	EV_EditMethod(NF(dlgFmtPosImage), 		0, ""),
	EV_EditMethod(NF(dlgFont),				0,	""),
	EV_EditMethod(NF(dlgHdrFtr),			0,	""),
	EV_EditMethod(NF(dlgLanguage),			0,	""),
	EV_EditMethod(NF(dlgMetaData), 			0, ""),
	EV_EditMethod(NF(dlgMoreWindows),		0,	""),
	EV_EditMethod(NF(dlgOptions),			0,	""),
	EV_EditMethod(NF(dlgParagraph), 		0,	""),
#ifdef ENABLE_SPELL
	EV_EditMethod(NF(dlgSpell), 			0,	""),
	EV_EditMethod(NF(dlgSpellPrefs), 		0,	""),
#endif
	EV_EditMethod(NF(dlgStyle), 			0,	""),
	EV_EditMethod(NF(dlgToggleCase),		0,	""),
	EV_EditMethod(NF(dlgWordCount), 		0,	""),
	EV_EditMethod(NF(dlgZoom),				0,	""),
	EV_EditMethod(NF(doBullets),			0,	""),
	EV_EditMethod(NF(doDashedList),		0,	""),
	EV_EditMethod(NF(doEscape),				0,	""),
	EV_EditMethod(NF(doListType),			0,	""),
	EV_EditMethod(NF(doNumbers),			0,	""),
	EV_EditMethod(NF(docProps),				0,	""),
	EV_EditMethod(NF(docSettings),			0,	""),
	EV_EditMethod(NF(doubleSpace),			0,	""),
	EV_EditMethod(NF(dragFrame), 			0,	""),
	EV_EditMethod(NF(dragHline), 			0,	""),
	EV_EditMethod(NF(dragInlineImage),		0,	""),
	EV_EditMethod(NF(dragTableDraw), 		0,	""),
	EV_EditMethod(NF(dragToXY), 			0,	""),
	EV_EditMethod(NF(dragToXYword), 		0,	""),
	EV_EditMethod(NF(dragVisualText),       0, ""),
	EV_EditMethod(NF(dragVline), 			0,	""),
	EV_EditMethod(NF(dropCap), 				0,	""),
#ifdef DEBUG
	EV_EditMethod(NF(dumpRDFForPoint),		0,	""),
	EV_EditMethod(NF(dumpRDFObjects),		0,	""),
#endif

	
	// e

	EV_EditMethod(NF(editAnnotation),		0,	""),
	EV_EditMethod(NF(editEmbed),			0,	""),
	EV_EditMethod(NF(editFooter),			0,	""),
	EV_EditMethod(NF(editHeader),			0,	""),
	EV_EditMethod(NF(editLatexAtPos),		0,	""),
	EV_EditMethod(NF(editLatexEquation),	0,	""),
	EV_EditMethod(NF(endDrag),				0,	""),
	EV_EditMethod(NF(endDragHline),			0,	""),
	EV_EditMethod(NF(endDragVline),			0,	""),
	EV_EditMethod(NF(endTableDraw),			0,	""),
	EV_EditMethod(NF(endnoteNext),			0,	""),
	EV_EditMethod(NF(endnotePrev),			0,	""),
	EV_EditMethod(NF(endnoteToFootnote),	0,	""),
	EV_EditMethod(NF(equationInsertSymbol),	0,	""),
	EV_EditMethod(NF(eraseTableBorder),		0,	""),
	EV_EditMethod(NF(executeScript),		EV_EMT_REQUIRE_SCRIPT_NAME, ""),
	EV_EditMethod(NF(extSelBOB),			0,	""),
	EV_EditMethod(NF(extSelBOD),			0,	""),
	EV_EditMethod(NF(extSelBOL),			0,	""),
	EV_EditMethod(NF(extSelBOS),			0,	""),
	EV_EditMethod(NF(extSelBOW),			0,	""),
	EV_EditMethod(NF(extSelEOB),			0,	""),
	EV_EditMethod(NF(extSelEOD),			0,	""),
	EV_EditMethod(NF(extSelEOL),			0,	""),
	EV_EditMethod(NF(extSelEOS),			0,	""),
	EV_EditMethod(NF(extSelEOW),			0,	""),
	EV_EditMethod(NF(extSelLeft),			0,	""),
	EV_EditMethod(NF(extSelNextLine),		0,	""),
	EV_EditMethod(NF(extSelPageDown),		0,	""),
	EV_EditMethod(NF(extSelPageUp), 		0,	""),
	EV_EditMethod(NF(extSelPrevLine),		0,	""),
	EV_EditMethod(NF(extSelRight),			0,	""),
	EV_EditMethod(NF(extSelScreenDown),		0,	""),
	EV_EditMethod(NF(extSelScreenUp),		0,	""),
	EV_EditMethod(NF(extSelToXY),			0,	""),

	// f
	EV_EditMethod(NF(fileExport), 0, ""),
	EV_EditMethod(NF(fileImport), 0, ""),
	EV_EditMethod(NF(fileInsertGraphic),	0,	""),
	EV_EditMethod(NF(fileInsertPageBackgroundGraphic),	0,	""),
	EV_EditMethod(NF(fileInsertPositionedGraphic),	0,	""),
	EV_EditMethod(NF(fileNew),				_A_,	""),
	EV_EditMethod(NF(fileNewUsingTemplate),				_A_,	""),
	EV_EditMethod(NF(fileOpen), 			_A_,	""),
	EV_EditMethod(NF(fileRevert), 0, ""),
	EV_EditMethod(NF(fileSave), 			0,	""),
	EV_EditMethod(NF(fileSaveAs),			0,	""),
	EV_EditMethod(NF(fileSaveAsWeb),				0, ""),
	EV_EditMethod(NF(fileSaveEmbed),		0,	""),
	EV_EditMethod(NF(fileSaveImage),		0,	""),
	EV_EditMethod(NF(fileSaveTemplate), 0, ""),
	EV_EditMethod(NF(find), 				0,	""),
	EV_EditMethod(NF(findAgain),			0,	""),
	EV_EditMethod(NF(fontFamily),			_D_,	""),
	EV_EditMethod(NF(fontSize), 			_D_,	""),
	EV_EditMethod(NF(fontSizeDecrease),		0,	""),
	EV_EditMethod(NF(fontSizeIncrease),		0,	""),
	EV_EditMethod(NF(footnoteNext),			0,		""),
	EV_EditMethod(NF(footnotePrev),			0,		""),
	EV_EditMethod(NF(footnoteToEndnote),	0,		""),
	EV_EditMethod(NF(formatFootnotes),        0,  ""),
	EV_EditMethod(NF(formatPainter),		0,	""),
	EV_EditMethod(NF(formatTOC),			0,		""),
	EV_EditMethod(NF(formatTable),			0,		""),
	EV_EditMethod(NF(frameBehindText),		0,		""),
	EV_EditMethod(NF(frameBringForward),	0,		""),
	EV_EditMethod(NF(frameBringToFront),	0,		""),
	EV_EditMethod(NF(frameFlipHoriz),		0,		""),
	EV_EditMethod(NF(frameFlipVert),		0,		""),
	EV_EditMethod(NF(frameGroup),			0,		""),
	EV_EditMethod(NF(frameInFrontOfText),	0,		""),
	EV_EditMethod(NF(frameRotateLeft),		0,		""),
	EV_EditMethod(NF(frameRotateRight),		0,		""),
	EV_EditMethod(NF(frameRotateTo),		0,		""),
	EV_EditMethod(NF(frameSendBackward),	0,		""),
	EV_EditMethod(NF(frameSendToBack),		0,		""),
	EV_EditMethod(NF(frameUngroup),			0,		""),

	// g
	EV_EditMethod(NF(go),					0,	""),

	// h
	EV_EditMethod(NF(helpChangelog),		_A_,		""),
	EV_EditMethod(NF(helpCheckVer), 		_A_,		""),
	EV_EditMethod(NF(helpContents), 		_A_,		""),
	EV_EditMethod(NF(helpIntro),			_A_,		""),
	EV_EditMethod(NF(helpReportBug), _A_, ""),
	EV_EditMethod(NF(helpSearch),			_A_,		""),
	EV_EditMethod(NF(hyperlinkCopyLocation), 0, ""),
	EV_EditMethod(NF(hyperlinkJump),		0,		""),
	EV_EditMethod(NF(hyperlinkJumpPos),     0,      ""),
	EV_EditMethod(NF(hyperlinkStatusBar),	0,		""),
	// i
	EV_EditMethod(NF(iconsPane),			0,	""),
	EV_EditMethod(NF(importStyles),			0,		""),
	EV_EditMethod(NF(insAnnotation),		0,		""),
	EV_EditMethod(NF(insAnnotationFromSel),	0,		""),
	EV_EditMethod(NF(insBreak),				0,		""),
	EV_EditMethod(NF(insColumnBreak),		0,	""),
	EV_EditMethod(NF(insDateTime),			0,		""),
	EV_EditMethod(NF(insEndnote),			0,		""),
	EV_EditMethod(NF(insField),				0,		""),
	EV_EditMethod(NF(insFile),				0,		""),
	EV_EditMethod(NF(insFootnote),			0,		""),
	EV_EditMethod(NF(insMediaFile),		0,		""),
	EV_EditMethod(NF(insPageNo),			0,		""),
	EV_EditMethod(NF(insScreenshot),		0,		""),
	EV_EditMethod(NF(insSectionBreak),		0,	""),
	EV_EditMethod(NF(insSymbol),			0,		""),
	EV_EditMethod(NF(insTOC),			0,		""),
	EV_EditMethod(NF(insTextBox),			0,		""),
	EV_EditMethod(NF(insVerticalTextBox),	0,		""),
	EV_EditMethod(NF(insert3DModel),		0,	""),
	EV_EditMethod(NF(insertAbovedotData),	_D_,	""),
	EV_EditMethod(NF(insertAcuteData),		_D_,	""),
	EV_EditMethod(NF(insertBlankPage),		0,	""),
	EV_EditMethod(NF(insertBookmark),		0,	""),
	EV_EditMethod(NF(insertBreveData),		_D_,	""),
	EV_EditMethod(NF(insertCaronData),		_D_,	""),
	EV_EditMethod(NF(insertCedillaData),	_D_,	""),
	EV_EditMethod(NF(insertCircumflexData), _D_,	""),
	EV_EditMethod(NF(insertClipart), 0, ""),
	EV_EditMethod(NF(insertClosingParenthesis),	_D_,""),
	EV_EditMethod(NF(insertColsAfter),	0,	""),
	EV_EditMethod(NF(insertColsBefore),	0,	""),
	EV_EditMethod(NF(insertColumnBreak),	0,	""),
	EV_EditMethod(NF(insertCopyright),		0,	""),
	EV_EditMethod(NF(insertData),			_D_,	""),
	EV_EditMethod(NF(insertDiaeresisData),	_D_,	""),
	EV_EditMethod(NF(insertDoubleacuteData),_D_,	""),
	EV_EditMethod(NF(insertEmDash),			0,	""),
	EV_EditMethod(NF(insertEnDash),			0,	""),
	EV_EditMethod(NF(insertEquation),		0,	""),
	EV_EditMethod(NF(insertFooterPreset),	0,	""),
	EV_EditMethod(NF(insertGraveData),		_D_,	""),
	EV_EditMethod(NF(insertHeaderPreset),	0,	""),
	EV_EditMethod(NF(insertHyperlink),		0,	""),
	EV_EditMethod(NF(insertIcon),			0,	""),
	EV_EditMethod(NF(insertLRM),		0,	""),
	EV_EditMethod(NF(insertLatexEquation),	0,	""),
	EV_EditMethod(NF(insertLineBreak),		0,	""),
	EV_EditMethod(NF(insertMacronData), 	_D_,	""),
	EV_EditMethod(NF(insertNBHyphen),		0,	""),
	EV_EditMethod(NF(insertNBSpace),		0,	""),
	EV_EditMethod(NF(insertNBZWSpace),		0,	""),
	EV_EditMethod(NF(insertOgonekData), 	_D_,	""),
	EV_EditMethod(NF(insertOpeningParenthesis),	_D_,""),
	EV_EditMethod(NF(insertPageBreak),		0,	""),
	EV_EditMethod(NF(insertParagraphBreak), 0,	""),
	EV_EditMethod(NF(insertRLM),		0,	""),
	EV_EditMethod(NF(insertRegistered),		0,	""),
	EV_EditMethod(NF(insertRowsAfter),	0,	""),
	EV_EditMethod(NF(insertRowsBefore),	0,	""),
	EV_EditMethod(NF(insertSectionBreak),	0,	""),
	EV_EditMethod(NF(insertShape),			0,	""),
	EV_EditMethod(NF(insertSignatureLine),	0,	""),
	EV_EditMethod(NF(insertSoftBreak),		0,	""),
	EV_EditMethod(NF(insertSoftHyphen),		0,	""),
	EV_EditMethod(NF(insertSpace),			0,	""),
	EV_EditMethod(NF(insertSumCols),			0,	""),
	EV_EditMethod(NF(insertSumRows),			0,	""),
	EV_EditMethod(NF(insertTab),			0,	""),
	EV_EditMethod(NF(insertTabCTL),			0,	""),
	EV_EditMethod(NF(insertTabShift),			0,	""),
	EV_EditMethod(NF(insertTable),          0,  ""),
	EV_EditMethod(NF(insertTildeData),		_D_,	""),
	EV_EditMethod(NF(insertTrademark),		0,	""),
	EV_EditMethod(NF(insertWordArt),		0,	""),
	EV_EditMethod(NF(insertXMLID),    		0,	""),
	EV_EditMethod(NF(insertZWJoiner),		0,	""),

	// j

	// k

	// l
	EV_EditMethod(NF(language), 		0,	""),
	EV_EditMethod(NF(lockToolbarLayout),	0,	""),

	// m
	EV_EditMethod(NF(mergeCells),			0,		""),
	EV_EditMethod(NF(mergeCellsDir),		0,		""),
	EV_EditMethod(NF(middleSpace),			0,		""),

	// n
	EV_EditMethod(NF(newWindow),			0,	""),
	EV_EditMethod(NF(nextComment),			0,	""),
	EV_EditMethod(NF(noop), 				0,	""),
	EV_EditMethod(NF(notImplemented),		0,	""),
	EV_EditMethod(NF(noteSwap),				0,	""),

	// o
	EV_EditMethod(NF(openTemplate), 0, ""),

	// p
#ifdef ENABLE_PRINT
	EV_EditMethod(NF(pageColumns),			0,	""),
	EV_EditMethod(NF(pageMargins),			0,	""),
	EV_EditMethod(NF(pageNumber),			0,	""),
	EV_EditMethod(NF(pageNumberRemove),		0,	""),
	EV_EditMethod(NF(pageOrientation),		0,	""),
	EV_EditMethod(NF(pageSetup),			0,	""),
	EV_EditMethod(NF(pageSize),				0,	""),
#endif
	EV_EditMethod(NF(paraBefore0),			0,		""),
	EV_EditMethod(NF(paraBefore12), 		0,		""),
	EV_EditMethod(NF(paraBorder),			0,		""),
	EV_EditMethod(NF(paraProp),				0,	""),
	EV_EditMethod(NF(paraSortAscend),		0,		""),
	EV_EditMethod(NF(paraSortDescend),		0,		""),
		// intended for ^V and Menu[Edit/Paste]
	EV_EditMethod(NF(paste),				0,	""),
			// intended for X11 middle mouse
	EV_EditMethod(NF(pasteSelection),		0,	""),
	EV_EditMethod(NF(pasteSpecial), 		0,	""),
	EV_EditMethod(NF(pasteVisualText), 		0,	""),
	EV_EditMethod(NF(prevComment),			0,	""),
	EV_EditMethod(NF(print),				0,	""),
#ifdef ENABLE_PRINT
	EV_EditMethod(NF(printDirectly),		0,	""),
	EV_EditMethod(NF(printPreview),			0,	""),
	EV_EditMethod(NF(printTB),				0,	""),
#endif

	// q
	EV_EditMethod(NF(querySaveAndExit), 	_A_,	""),

	// r
	EV_EditMethod(NF(rdfAnchorEditSemanticItem) , 0,  ""),
	EV_EditMethod(NF(rdfAnchorEditTriples), 0,  ""),
	EV_EditMethod(NF(rdfAnchorExportSemanticItem) , 0,  ""),
	EV_EditMethod(NF(rdfAnchorQuery) ,     0,  ""),
	EV_EditMethod(NF(rdfAnchorSelectNextReferenceToSemanticItem) , 0,  ""),
	EV_EditMethod(NF(rdfAnchorSelectPrevReferenceToSemanticItem) , 0,  ""),
	EV_EditMethod(NF(rdfAnchorSelectThisReferenceToSemanticItem) , 0,  ""),
	EV_EditMethod(NF(rdfApplyCurrentStyleSheet),  0,  ""),
	EV_EditMethod(NF(rdfApplyStylesheetContactName) ,  0,  ""),
	EV_EditMethod(NF(rdfApplyStylesheetContactNameHomepagePhone) ,  0,  ""),
	EV_EditMethod(NF(rdfApplyStylesheetContactNamePhone) ,  0,  ""),
	EV_EditMethod(NF(rdfApplyStylesheetContactNick) ,  0,  ""),
	EV_EditMethod(NF(rdfApplyStylesheetContactNickPhone) ,  0,  ""),
	EV_EditMethod(NF(rdfApplyStylesheetEventName) ,  0,  ""),
	EV_EditMethod(NF(rdfApplyStylesheetEventSummary) ,  0,  ""),
	EV_EditMethod(NF(rdfApplyStylesheetEventSummaryLocation) ,  0,  ""),
	EV_EditMethod(NF(rdfApplyStylesheetEventSummaryLocationTimes) ,  0,  ""),
	EV_EditMethod(NF(rdfApplyStylesheetEventSummaryTimes) ,  0,  ""),
	EV_EditMethod(NF(rdfApplyStylesheetLocationLatLong) ,  0,  ""),
	EV_EditMethod(NF(rdfApplyStylesheetLocationName) ,  0,  ""),
	EV_EditMethod(NF(rdfDisassocateCurrentStyleSheet),  0,  ""),
	EV_EditMethod(NF(rdfEditor),            0,	""),
	EV_EditMethod(NF(rdfInsertNewContact),  0,	""),
	EV_EditMethod(NF(rdfInsertNewContactFromFile),  0,	""),
	EV_EditMethod(NF(rdfInsertRef),         0,	""),
#ifdef DEBUG
	EV_EditMethod(NF(rdfPlay), 				0,	""),
#endif
	EV_EditMethod(NF(rdfQuery),             0,	""),
	EV_EditMethod(NF(rdfQueryXMLIDs),       0,	""),
	EV_EditMethod(NF(rdfSemitemFindRelatedFoafKnows),  0,  ""),
	EV_EditMethod(NF(rdfSemitemRelatedToSourceFoafKnows),  0,  ""),
	EV_EditMethod(NF(rdfSemitemSetAsSource),  0,  ""),
	EV_EditMethod(NF(rdfStylesheetSettings),  0,  ""),
#ifdef DEBUG
	EV_EditMethod(NF(rdfTest), 				0,	""),
#endif
	EV_EditMethod(NF(redo), 				0,	""),
	EV_EditMethod(NF(refCaption),			0,	""),
	EV_EditMethod(NF(refDeleteSource),		0,	""),
	EV_EditMethod(NF(refInsertBibliography), 0,	""),
	EV_EditMethod(NF(refInsertCitation),	0,	""),
	EV_EditMethod(NF(refInsertIndex),		0,	""),
	EV_EditMethod(NF(refInsertTOA),			0,	""),
	EV_EditMethod(NF(refInsertTOF),			0,	""),
	EV_EditMethod(NF(refMarkCitation),		0,	""),
	EV_EditMethod(NF(refMarkEntry),			0,	""),
	EV_EditMethod(NF(refRemoveBibliography), 0,	""),
	EV_EditMethod(NF(refRemoveIndex),		0,	""),
	EV_EditMethod(NF(refRemoveTOA),			0,	""),
	EV_EditMethod(NF(refXRef),				0,	""),
	EV_EditMethod(NF(releaseFrame), 		0,	""),
	EV_EditMethod(NF(releaseInlineImage), 		0,	""),
	EV_EditMethod(NF(removeFooter), 		0,	""),
	EV_EditMethod(NF(removeHeader), 		0,	""),
	EV_EditMethod(NF(removeThisRowRepeat), 		0,	""),
	EV_EditMethod(NF(repeatHeaderRows),		0,	""),
	EV_EditMethod(NF(repeatThisRow),		0,	""),
	EV_EditMethod(NF(replace),				0,	""),
	EV_EditMethod(NF(replaceChar),			_D_,""),
	EV_EditMethod(NF(resolveAnnotation),	0,	""),
	EV_EditMethod(NF(revisionAccept),		0,  ""),
	EV_EditMethod(NF(revisionAcceptAll),	0,  ""),
	EV_EditMethod(NF(revisionAcceptAllShown),	0,  ""),
	EV_EditMethod(NF(revisionAcceptAllStopTracking),	0,  ""),
	EV_EditMethod(NF(revisionAcceptNext),	0,  ""),
	EV_EditMethod(NF(revisionCombineDocuments),	0,  ""),
	EV_EditMethod(NF(revisionCompareDocuments),	0,  ""),
	EV_EditMethod(NF(revisionDisplayMode),	_D_,""),
	EV_EditMethod(NF(revisionFindNext),		0,  ""),
	EV_EditMethod(NF(revisionFindPrev),		0,  ""),
	EV_EditMethod(NF(revisionNew),   		0,	""),
	EV_EditMethod(NF(revisionReject),		0,  ""),
	EV_EditMethod(NF(revisionRejectAll),	0,  ""),
	EV_EditMethod(NF(revisionRejectAllShown),	0,  ""),
	EV_EditMethod(NF(revisionRejectAllStopTracking),	0,  ""),
	EV_EditMethod(NF(revisionRejectNext),	0,  ""),
	EV_EditMethod(NF(revisionSelect),       0,	""),
	EV_EditMethod(NF(revisionSetViewLevel),	0,  ""),
	EV_EditMethod(NF(rotateCase),			0,	""),

	// s

	EV_EditMethod(NF(saveImmediate),			0,	""),
	EV_EditMethod(NF(scrollLineDown),		0,	""),
	EV_EditMethod(NF(scrollLineLeft),		0,	""),
	EV_EditMethod(NF(scrollLineRight),		0,	""),
	EV_EditMethod(NF(scrollLineUp), 		0,	""),
	EV_EditMethod(NF(scrollPageDown),		0,	""),
	EV_EditMethod(NF(scrollPageLeft),		0,	""),
	EV_EditMethod(NF(scrollPageRight),		0,	""),
	EV_EditMethod(NF(scrollPageUp), 		0,	""),
	EV_EditMethod(NF(scrollToBottom),		0,	""),
	EV_EditMethod(NF(scrollToTop),			0,	""),
	EV_EditMethod(NF(scrollWheelMouseDown), 		0,	""),
	EV_EditMethod(NF(scrollWheelMouseUp),			0,	""),
	EV_EditMethod(NF(sectColumns1), 		0,		""),
	EV_EditMethod(NF(sectColumns2), 		0,		""),
	EV_EditMethod(NF(sectColumns3), 		0,		""),
	EV_EditMethod(NF(sectProps),			0,	""),
	EV_EditMethod(NF(selPane),				0,	""),
	EV_EditMethod(NF(selectAll),			0,	""),
	EV_EditMethod(NF(selectBlock),			0,	""),
	EV_EditMethod(NF(selectCell),			0,	""),
	EV_EditMethod(NF(selectColumn),			0,	""),
	EV_EditMethod(NF(selectColumnClick),			0,	""),
	EV_EditMethod(NF(selectFrame),			0,	""),
	EV_EditMethod(NF(selectLine),			0,	""),
	EV_EditMethod(NF(selectMath),			0,	""),
	EV_EditMethod(NF(selectObject), 		0,	""),
	EV_EditMethod(NF(selectRow),			0,	""),
	EV_EditMethod(NF(selectTOC),			0,	""),
	EV_EditMethod(NF(selectTable),			0,	""),
	EV_EditMethod(NF(selectWord),			0,	""),
	EV_EditMethod(NF(setEditVI),			0,	""),
	EV_EditMethod(NF(setInputVI),			0,	""),
	EV_EditMethod(NF(setPosImage), 	0,		""),
	EV_EditMethod(NF(setStyleHeading1), 	0,		""),
	EV_EditMethod(NF(setStyleHeading2), 	0,		""),
	EV_EditMethod(NF(setStyleHeading3), 	0,		""),
	EV_EditMethod(NF(setStyleNormal), 		0,		""),
	EV_EditMethod(NF(showNotes),			0,		""),
	EV_EditMethod(NF(singleSpace),			0,		""),
	EV_EditMethod(NF(sortColsAscend),       0,  ""),
	EV_EditMethod(NF(sortColsDescend),      0,  ""),
	EV_EditMethod(NF(sortRowsAscend),       0,  ""),
	EV_EditMethod(NF(sortRowsDescend),      0,  ""),
	EV_EditMethod(NF(sortTable),            0,  ""),
#ifdef ENABLE_SPELL
	EV_EditMethod(NF(spellAdd), 			0,	""),
	EV_EditMethod(NF(spellIgnoreAll),		0,	""),
	EV_EditMethod(NF(spellSuggest_1),		0,	""),
	EV_EditMethod(NF(spellSuggest_2),		0,	""),
	EV_EditMethod(NF(spellSuggest_3),		0,	""),
	EV_EditMethod(NF(spellSuggest_4),		0,	""),
	EV_EditMethod(NF(spellSuggest_5),		0,	""),
	EV_EditMethod(NF(spellSuggest_6),		0,	""),
	EV_EditMethod(NF(spellSuggest_7),		0,	""),
	EV_EditMethod(NF(spellSuggest_8),		0,	""),
	EV_EditMethod(NF(spellSuggest_9),		0,	""),
#endif
	EV_EditMethod(NF(splitCells),           0,  ""),
	EV_EditMethod(NF(splitCellsDir),        0,  ""),
	EV_EditMethod(NF(splitTable),           0,  ""),
	EV_EditMethod(NF(style),				_D_,""),

	// t
	EV_EditMethod(NF(tableBorder),			0,		""),
	EV_EditMethod(NF(tableCellHeight),		_D_,	""),
	EV_EditMethod(NF(tableCellWidth),		_D_,	""),
	EV_EditMethod(NF(tableColNarrower),		0,		""),
	EV_EditMethod(NF(tableColWider),		0,		""),
	EV_EditMethod(NF(tablePen),				0,		""),
	EV_EditMethod(NF(tableRowShorter),		0,		""),
	EV_EditMethod(NF(tableRowTaller),		0,		""),
	EV_EditMethod(NF(tableShading),			0,		""),
	EV_EditMethod(NF(tableStyle),			0,		""),
	EV_EditMethod(NF(tableStyleClear),		0,		""),
	EV_EditMethod(NF(tableStyleOpt),		0,		""),
	EV_EditMethod(NF(tableToTextCommas),	0,		""),
	EV_EditMethod(NF(tableToTextCommasTabs),    0,		""),
	EV_EditMethod(NF(tableToTextParas),    0,		""),
	EV_EditMethod(NF(tableToTextTabs),    0,		""),
	EV_EditMethod(NF(tocAddText),			0,		""),
	EV_EditMethod(NF(tocInsert),			0,		""),
	EV_EditMethod(NF(tocRemove),			0,		""),
	EV_EditMethod(NF(tocUpdate),			0,		""),
	EV_EditMethod(NF(toggleAutoGrammar),	0,	""),
	EV_EditMethod(NF(toggleAutoRevision),  0,  ""),
#ifdef ENABLE_SPELL
	EV_EditMethod(NF(toggleAutoSpell),      0,  ""),
#endif
	EV_EditMethod(NF(toggleBold),			0,	""),
	EV_EditMethod(NF(toggleBottomline), 	0,	""),
	EV_EditMethod(NF(toggleDirOverrideLTR), 0,	""),
	EV_EditMethod(NF(toggleDirOverrideRTL), 0,	""),
	EV_EditMethod(NF(toggleDisplayAnnotations), 0,	""),
	EV_EditMethod(NF(toggleDrawTable),	0,	""),
	EV_EditMethod(NF(toggleEquationDisplay),	0,	""),
	EV_EditMethod(NF(toggleHidden),			0,	""),
	EV_EditMethod(NF(toggleIndent),         0,  ""),
	EV_EditMethod(NF(toggleInsertMode), 	0,  ""),
	EV_EditMethod(NF(toggleItalic), 		0,	""),
	EV_EditMethod(NF(toggleMarkRevisions),  0,  ""),
	EV_EditMethod(NF(toggleOline),			0,  ""),
	EV_EditMethod(NF(toggleParaBefore),		0,	""),
	EV_EditMethod(NF(togglePlain),			0,	""),
	EV_EditMethod(NF(toggleRDFAnchorHighlight), 0,	""),
	EV_EditMethod(NF(toggleShowRevisions),  0,  ""),
	EV_EditMethod(NF(toggleShowRevisionsAfter),  0,  ""),
	EV_EditMethod(NF(toggleShowRevisionsAfterPrevious),  0,  ""),
	EV_EditMethod(NF(toggleShowRevisionsBefore),  0,  ""),
	EV_EditMethod(NF(toggleStrike), 		0,	""),
	EV_EditMethod(NF(toggleSub),			0,	""),
	EV_EditMethod(NF(toggleSuper),			0,	""),
	EV_EditMethod(NF(toggleTableEraser),	0,	""),
	EV_EditMethod(NF(toggleTableGridlines),	0,	""),
	EV_EditMethod(NF(toggleTopline),		0,	""),
	EV_EditMethod(NF(toggleUline),			0,	""),
	EV_EditMethod(NF(toggleUnIndent),       0,  ""),

	// u
	EV_EditMethod(NF(undo), 				0,	""),

	// v
	EV_EditMethod(NF(viCmd_5e),		0,	""), //^ 
	EV_EditMethod(NF(viCmd_A),		0,	""),
	EV_EditMethod(NF(viCmd_C),		0,	""),
	EV_EditMethod(NF(viCmd_I),		0,	""),
	EV_EditMethod(NF(viCmd_J),		0,	""),
	EV_EditMethod(NF(viCmd_O),		0,	""),
	EV_EditMethod(NF(viCmd_P),		0,	""),
	EV_EditMethod(NF(viCmd_a),		0,	""),
	EV_EditMethod(NF(viCmd_c24),	0,	""),
	EV_EditMethod(NF(viCmd_c28),	0,	""),
	EV_EditMethod(NF(viCmd_c29),	0,	""),
	EV_EditMethod(NF(viCmd_c5b),	0,	""),
	EV_EditMethod(NF(viCmd_c5d),	0,	""),
	EV_EditMethod(NF(viCmd_c5e),	0,	""),
	EV_EditMethod(NF(viCmd_cb), 	0,	""),
	EV_EditMethod(NF(viCmd_cw), 	0,	""),
	EV_EditMethod(NF(viCmd_d24),		0,	""),
	EV_EditMethod(NF(viCmd_d28),		0,	""),
	EV_EditMethod(NF(viCmd_d29),		0,	""),
	EV_EditMethod(NF(viCmd_d5b),		0,	""),
	EV_EditMethod(NF(viCmd_d5d),		0,	""),
	EV_EditMethod(NF(viCmd_d5e),		0,	""),
	EV_EditMethod(NF(viCmd_db), 	0,	""),
	EV_EditMethod(NF(viCmd_dd), 	0,	""),
	EV_EditMethod(NF(viCmd_dw), 	0,	""),
	EV_EditMethod(NF(viCmd_o),		0,	""),
	EV_EditMethod(NF(viCmd_y24),	0,	""),
	EV_EditMethod(NF(viCmd_y28),	0,	""),
	EV_EditMethod(NF(viCmd_y29),	0,	""),
	EV_EditMethod(NF(viCmd_y5b),	0,	""),
	EV_EditMethod(NF(viCmd_y5d),	0,	""),
	EV_EditMethod(NF(viCmd_y5e),	0,	""),
	EV_EditMethod(NF(viCmd_yb), 	0,	""),
	EV_EditMethod(NF(viCmd_yw), 	0,	""),
	EV_EditMethod(NF(viCmd_yy), 	0,	""),
#if !XAP_SIMPLE_TOOLBAR
	EV_EditMethod(NF(viewExtra),			0,		""),
	EV_EditMethod(NF(viewFormat),			0,		""),
#endif
	EV_EditMethod(NF(viewFullScreen), 0, ""),
	EV_EditMethod(NF(viewGridlines), 0, ""),
	EV_EditMethod(NF(viewHeadFoot), 		0,		""),
	EV_EditMethod(NF(viewLockStyles),   0,		""),
	EV_EditMethod(NF(viewNavPane), 0, ""),
	EV_EditMethod(NF(viewNormalLayout), 0, ""),
	EV_EditMethod(NF(viewPara), 		0,		""),
	EV_EditMethod(NF(viewPrintLayout), 0, ""),
	EV_EditMethod(NF(viewRuler),			0,		""),
	EV_EditMethod(NF(viewSplit), 0, ""),
	EV_EditMethod(NF(viewStatus),			0,		""),
#if !XAP_SIMPLE_TOOLBAR
	EV_EditMethod(NF(viewStd),			0,		""),
#endif
	// capitals before lowercase ...
	EV_EditMethod(NF(viewTB1),			0,		""),
	EV_EditMethod(NF(viewTB2),			0,		""),
	EV_EditMethod(NF(viewTB3),			0,		""),
	EV_EditMethod(NF(viewTB4),			0,		""),
#if !XAP_SIMPLE_TOOLBAR
	EV_EditMethod(NF(viewTable),			0,		""),	
#endif
	EV_EditMethod(NF(viewWebLayout), 0, ""),

	// w
	EV_EditMethod(NF(warpInsPtBOB), 		0,	""),
	EV_EditMethod(NF(warpInsPtBOD), 		0,	""),
	EV_EditMethod(NF(warpInsPtBOL), 		0,	""),
	EV_EditMethod(NF(warpInsPtBOP), 		0,	""),
	EV_EditMethod(NF(warpInsPtBOS), 		0,	""),
	EV_EditMethod(NF(warpInsPtBOW), 		0,	""),
	EV_EditMethod(NF(warpInsPtEOB), 		0,	""),
	EV_EditMethod(NF(warpInsPtEOD), 		0,	""),
	EV_EditMethod(NF(warpInsPtEOL), 		0,	""),
	EV_EditMethod(NF(warpInsPtEOP), 		0,	""),
	EV_EditMethod(NF(warpInsPtEOS), 		0,	""),
	EV_EditMethod(NF(warpInsPtEOW), 		0,	""),
	EV_EditMethod(NF(warpInsPtLeft),		0,	""),
	EV_EditMethod(NF(warpInsPtNextLine),	0,	""),
	EV_EditMethod(NF(warpInsPtNextPage),	0,	""),
	EV_EditMethod(NF(warpInsPtNextScreen),	0,	""),
	EV_EditMethod(NF(warpInsPtPrevLine),	0,	""),
	EV_EditMethod(NF(warpInsPtPrevPage),	0,	""),
	EV_EditMethod(NF(warpInsPtPrevScreen),	0,	""),
	EV_EditMethod(NF(warpInsPtRight),		0,	""),
	EV_EditMethod(NF(warpInsPtToXY),		0,	""),
	EV_EditMethod(NF(wrapObject),			0,	""),

	// x

	// y

	// z
	EV_EditMethod(NF(zoom), 				0,		""),
	EV_EditMethod(NF(zoom100), 0, ""),
	EV_EditMethod(NF(zoom200), 0, ""),
	EV_EditMethod(NF(zoom50), 0, ""),
	EV_EditMethod(NF(zoom75), 0, ""),
	EV_EditMethod(NF(zoomIn), 0, ""),
	EV_EditMethod(NF(zoomOut), 0, ""),
	EV_EditMethod(NF(zoomWhole), 0, ""),
	EV_EditMethod(NF(zoomWidth), 0, "")
};



EV_EditMethodContainer * AP_GetEditMethods(void)
{
	// Construct a container for all of the methods this application
	// knows about.

	return new EV_EditMethodContainer(G_N_ELEMENTS(s_arrayEditMethods),s_arrayEditMethods);
}

#undef _D_
#undef _A_
#undef F
#undef N
#undef NF

/*****************************************************************/
/*****************************************************************/

#define F(fn)		ap_EditMethods::fn
#define Defun(fn)	bool F(fn)(AV_View*   pAV_View,   EV_EditMethodCallData *	pCallData  )
#define Defun0(fn)	bool F(fn)(AV_View* /*pAV_View*/, EV_EditMethodCallData * /*pCallData*/)
#define Defun1(fn)	bool F(fn)(AV_View*   pAV_View,   EV_EditMethodCallData * /*pCallData*/)
#define EX(fn)		F(fn)(pAV_View, pCallData)

// forward declaration
static bool _openURL(const char* url);

static UT_Timer * s_pToUpdateCursor = nullptr;
static UT_Worker * s_pFrequentRepeat = nullptr;
static XAP_Frame * s_pLoadingFrame = nullptr;
static AD_Document * s_pLoadingDoc = nullptr;
static bool s_LockOutGUI = false;

class ABI_EXPORT _Freq
{
public:
	_Freq(AV_View * pView,EV_EditMethodCallData * pData, void(* exe)(AV_View * pView,EV_EditMethodCallData * pData)):
		m_pView (pView),
		m_pData(pData),
		m_pExe(exe)
	{ xxx_UT_DEBUGMSG(("_Freq created %x ",this));};
	AV_View * m_pView;
	EV_EditMethodCallData * m_pData;
	void(* m_pExe)(AV_View * ,EV_EditMethodCallData *) ;
};

/*!
This little macro locks out loading frames from any activity thus preventing
segfaults.
*
* Also used to lock out operations during a frequently repeated event 
* (like holding down an arrow key)
*
*/

static bool s_EditMethods_check_frame(void)
{
	bool result = false;
	if(s_LockOutGUI)
	{
		return true;
	}
	if(s_pFrequentRepeat != nullptr)
	{
		xxx_UT_DEBUGMSG(("Dropping frequent event!!!! \n"));
		return true;
	}
	XAP_Frame * pFrame = XAP_App::getApp()->getLastFocussedFrame();
	AV_View * pView = nullptr;
	if(pFrame)
	{
		pView = pFrame->getCurrentView();
	}
	if(s_pLoadingFrame && (pFrame == s_pLoadingFrame))
	{
		result = true;
	}
	else if(pFrame && (s_pLoadingDoc != nullptr) && (pFrame->getCurrentDoc() == s_pLoadingDoc))
	{
	        result = true;
	}
	else if(pView && ((pView->getPoint() == 0) || pView->isLayoutFilling()))
	{
		result = true;
	}
	return result;
}

/*!
 * Call this if you want to prevent GUI operations on Abinova.
 */
static bool lockGUI(void)
{
	s_LockOutGUI = true;
	return true;
}


/*!
 * Call this to allow GUI operations on Abinova.
 */
static bool unlockGUI(void)
{
	s_LockOutGUI = false;
	return true;
}

#define CHECK_FRAME if(s_EditMethods_check_frame()) return true;

/*!
 * use this code to execute a one-off operation in an idle loop.
 * This allows us to drop frequent events like those that come from arrow keys
 * so we never get ahead of ourselves.
 */
static void _sFrequentRepeat(UT_Worker * pWorker)
{
	// I have experienced a situation in which the the worker fired recursively while
	// inside of the m_pExe function, creating an endless loop; this prevents that from hapening
	// (the problem was caused by an endless loop elsewhere, but the recursive firing made
	// it hard to diagnose; in any case when we use a timer rather than idle, this could
	// happen if m_pExe is taking longer to execute than the timer interval)
	
	static bool bRunning = false;

	if(bRunning)
		return;
	
	bRunning = true;
//
// Once run then delete, stop and set to nullptr
//
	
	_Freq * pFreq = static_cast<_Freq *>(pWorker->getInstanceData());
	xxx_UT_DEBUGMSG((" _sFrequentRepeat: pWorker %x pFeq %x \n",pWorker,pFreq));
	s_pFrequentRepeat->stop();
	UT_Worker * pTmp =  s_pFrequentRepeat;
	//
	// Set s_pFrequentRepeat to nullptr before we execute the method
	// so that the call itself doesn't generate a new event to process
	//
	s_pFrequentRepeat = nullptr;

	pFreq->m_pExe(pFreq->m_pView,pFreq->m_pData);
	DELETEP(pFreq->m_pData);
	delete pFreq;
	delete pTmp;

	
	bRunning = false;
}

#ifdef ENABLE_SPELL
Defun1(toggleAutoSpell)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail (pFrame, false);

	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail (pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail (pPrefs, false);

	XAP_PrefsScheme *pPrefsScheme = pPrefs->getCurrentScheme();
	UT_return_val_if_fail (pPrefsScheme, false);

	bool b = false;

	pPrefs->getPrefsValueBool(AP_PREF_KEY_AutoSpellCheck, b);
	pPrefsScheme->setValueBool(AP_PREF_KEY_AutoSpellCheck, !b);
	return true;
}
#endif

Defun1(scrollPageDown)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	pAV_View->cmdScroll(AV_SCROLLCMD_PAGEDOWN);

	return true;
}

Defun1(scrollPageUp)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	pAV_View->cmdScroll(AV_SCROLLCMD_PAGEUP);

	return true;
}

Defun1(scrollPageLeft)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	pAV_View->cmdScroll(AV_SCROLLCMD_PAGELEFT);

	return true;
}

Defun1(scrollPageRight)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	pAV_View->cmdScroll(AV_SCROLLCMD_PAGERIGHT);

	return true;
}

Defun1(scrollLineDown)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	pAV_View->cmdScroll(AV_SCROLLCMD_LINEDOWN);

	return true;
}

Defun1(scrollLineUp)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	pAV_View->cmdScroll(AV_SCROLLCMD_LINEUP);

	return true;
}

Defun1(scrollWheelMouseDown)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	xxx_UT_DEBUGMSG(("Wheel Mouse Down \n"));
	pAV_View->cmdScroll(AV_SCROLLCMD_LINEDOWN, pAV_View->getGraphics()->tlu(36));

	return true;
}

Defun1(scrollWheelMouseUp)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	xxx_UT_DEBUGMSG(("Wheel Mouse Up \n"));
	pAV_View->cmdScroll(AV_SCROLLCMD_LINEUP, pAV_View->getGraphics()->tlu (36));

	return true;
}

Defun1(scrollLineLeft)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	pAV_View->cmdScroll(AV_SCROLLCMD_LINELEFT);

	return true;
}

Defun1(scrollLineRight)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	pAV_View->cmdScroll(AV_SCROLLCMD_LINERIGHT);

	return true;
}

Defun1(scrollToTop)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	pAV_View->cmdScroll(AV_SCROLLCMD_TOTOP);

	return true;
}

Defun1(scrollToBottom)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	pAV_View->cmdScroll(AV_SCROLLCMD_TOBOTTOM);

	return true;
}

Defun0(fileNew)
{
	CHECK_FRAME;
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail (pApp, false);
	
#if 0 //def HAVE_HILDON	
	
	XAP_Frame * pNewFrame;
	if (pApp->getFrameCount() == 0)
		pNewFrame = pApp->newFrame();
	else
	{
		//fileSave(nullptr, nullptr);
		pNewFrame = pApp->getFrame(0);
		if (pNewFrame->isDirty())
		{
			if(!fileSave(pAV_View, nullptr))
			{
				// we cannot just close the dirty file when the user clicked cancel -- if
				// she really want to loose unsaved changes, let her close it manually
				return false;
			}
		}
	}
#else
	XAP_Frame * pNewFrame = pApp->newFrame();
#endif

	// the IEFileType here doesn't really matter, since the name is nullptr
	UT_Error error = pNewFrame->loadDocument((const char *)nullptr, IEFT_Unknown);

	if (pNewFrame)
	{
		pNewFrame->show();
	}
	return E2B(error);
}

/*****************************************************************/
/*****************************************************************/

// TODO i've pulled the code to compose a question in a message
// TODO box into these little s_Ask*() functions.  part of this
// TODO is to isolate the question asking from the code which
// TODO decides what to do with the answer.  but also to see if
// TODO we want to abstract things further and make us think about
// TODO localization of the question strings....

/*!
 * Callback function to implement the updating loader. This enables the user
 * to see the document as soon as possible and updates the size of the scroll
 * bars as the document is loaded.
 */
static bool s_bFirstDrawDone = false;
static UT_sint32 s_iLastYScrollOffset = -1;
static UT_sint32 s_iLastXScrollOffset = -1;
static bool      s_bFreshDraw = false;

static void s_LoadingCursorCallback(UT_Worker * pTimer )
{
	UT_return_if_fail (pTimer);
	xxx_UT_DEBUGMSG(("Update Screen on load Frame %x \n",s_pLoadingFrame));
	XAP_Frame * pFrame = s_pLoadingFrame;
	UT_uint32 iPageCount = 0;
	
	if(pFrame == nullptr)
	{
		s_bFirstDrawDone = false;
		return;
	}
	pFrame->setCursor(GR_Graphics::GR_CURSOR_WAIT);
	FV_View * pView = static_cast<FV_View *>(pFrame->getCurrentView());
	if(pView)
	{
		GR_Graphics * pG = pView->getGraphics();
		if(pG)
		{
			pG->setCursor(GR_Graphics::GR_CURSOR_WAIT);
		}
		FL_DocLayout * pLayout = pView->getLayout();
		if(pView->getPoint() > 0)
		{
			pLayout->updateLayout();
			iPageCount = pLayout->countPages();

			if(!s_bFirstDrawDone && (iPageCount > 1))
			{
				pView->queueDraw();
				s_bFirstDrawDone = true;
			}
			else
			{
				// we only want to draw if we need to:
				//   (1) if the scroller position has changed
				//   (2) if the previous draw was due to a scroll change
				//
				// This way each change of scroller position will
				// result in two draws, the second of which will
				// ensure that anything from the current vieport that
				// was not yet laid out when the first draw was made
				// is drawn
				if(iPageCount > 1)
				{
					pView->notifyListeners(AV_CHG_PAGECOUNT | AV_CHG_WINDOWSIZE);
					if(pView->getYScrollOffset() != s_iLastYScrollOffset ||
					   pView->getXScrollOffset() != s_iLastXScrollOffset)
					{
						pView->updateScreen(true);
						s_iLastYScrollOffset = pView->getYScrollOffset();
						s_iLastXScrollOffset = pView->getXScrollOffset();
						s_bFreshDraw = true;
						xxx_UT_DEBUGMSG(("Incr. loader: primary draw\n"));
					}
					else if(s_bFreshDraw)
					{
					    pView->updateScreen(true);
						s_bFreshDraw = false;
						xxx_UT_DEBUGMSG(("Incr. loader: secondary draw\n"));
					}
					else
					{
						xxx_UT_DEBUGMSG(("Incr. loader: draw not needed\n"));
					}
				}
			}
		}
	}
	else
	{
		s_bFirstDrawDone = false;
	}
}

/*!
 * Control Method for the updating loader.
\param bool bStartStop true to start the updating loader, flase to stop it
             after the document has loaded.
\param XAP_Frame * pFrame Pointer to the new frame being loaded.
*/
static void s_StartStopLoadingCursor( bool bStartStop, XAP_Frame * pFrame)
{
	// Now construct the timer for auto-updating
	if(bStartStop)
	{
//
// Can't have multiple loading document yet. Need Vectors of loading frames
// and auto-updaters. Do this later.
//
		if(s_pLoadingFrame != nullptr)
		{
			return;
		}
		s_pLoadingFrame = pFrame;
		s_pLoadingDoc = pFrame->getCurrentDoc();
		if(s_pToUpdateCursor == nullptr)
		{
			s_pToUpdateCursor = UT_Timer::static_constructor(s_LoadingCursorCallback,nullptr);
		}
		s_bFirstDrawDone = false;
		const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
		UT_String msg (pSS->getValue(XAP_STRING_ID_MSG_ImportingDoc));
		pFrame->setStatusMessage ( static_cast<const gchar *>(msg.c_str()) );
		s_pToUpdateCursor->set(1000);
		s_pToUpdateCursor->start();
//		s_pLoadingFrame = XAP_App::getApp()->getLastFocussedFrame();
	}
	else
	{
		if(s_pToUpdateCursor != nullptr)
		{
			s_pToUpdateCursor->stop();
			DELETEP(s_pToUpdateCursor);
			s_pToUpdateCursor = nullptr;
			if(s_pLoadingFrame != nullptr)
			{
				s_pLoadingFrame->setCursor(GR_Graphics::GR_CURSOR_DEFAULT);
				FV_View * pView = static_cast<FV_View *>(s_pLoadingFrame->getCurrentView());
				if(pView)
				{
					pView->setCursorToContext();
					pView->focusChange(AV_FOCUS_HERE);
				}
			}
			s_pLoadingFrame = nullptr;
		}
		s_pLoadingDoc = nullptr;
	}
}


static void s_TellSaveFailed(XAP_Frame * pFrame, const char * fileName, UT_Error errorCode)
{
	XAP_String_Id String_id;

    switch(errorCode) {
    case UT_SAVE_CANCELLED: // We actually don't have a write error
        return;

    case UT_SAVE_WRITEERROR: // We have a write error
        String_id = AP_STRING_ID_MSG_SaveFailedWrite;
        break;

	case UT_SAVE_NAMEERROR: // We have a name error
        String_id = AP_STRING_ID_MSG_SaveFailedName;
        break;

	case UT_SAVE_EXPORTERROR: // We have an export error
        String_id = AP_STRING_ID_MSG_SaveFailedExport;
        break;

	default: // The generic case - should be eliminated eventually
        String_id = AP_STRING_ID_MSG_SaveFailed;
        break;
    }
	pFrame->showMessageBox(String_id,
			       XAP_Dialog_MessageBox::b_O,
			       XAP_Dialog_MessageBox::a_OK,
			       fileName);
}

#ifdef ENABLE_SPELL
static void s_TellSpellDone(XAP_Frame * pFrame, bool bIsSelection)
{
	pFrame->showMessageBox(bIsSelection ? AP_STRING_ID_MSG_SpellSelectionDone : AP_STRING_ID_MSG_SpellDone,
			       XAP_Dialog_MessageBox::b_O,
			       XAP_Dialog_MessageBox::a_OK);
}
#endif

static void s_TellNotImplemented(XAP_Frame * pFrame, const char * szWhat, int iLine)
{
	XAP_Dialog_MessageBox * message = 
		pFrame->createMessageBox(AP_STRING_ID_MSG_DlgNotImp,
					 XAP_Dialog_MessageBox::b_O,
					 XAP_Dialog_MessageBox::a_OK,
					 szWhat, __FILE__, iLine);
	pFrame->showMessageBox(message);

}

static bool s_AskRevertFile(XAP_Frame * pFrame)
{
	// return true if we should revert the file (back to the saved copy).

	char *pFilename = UT_go_filename_from_uri(pFrame->getFilename());

	bool b = (pFrame->showMessageBox(AP_STRING_ID_MSG_RevertBuffer,
										XAP_Dialog_MessageBox::b_YN,
										XAP_Dialog_MessageBox::a_YES,
										pFilename)
						== XAP_Dialog_MessageBox::a_YES);

	FREEP(pFilename);
	return b;
}

#if XAP_DONT_CONFIRM_QUIT
#else
static bool s_AskCloseAllAndExit(XAP_Frame * pFrame)
{
	// return true if we should quit.
	return (pFrame->showMessageBox(AP_STRING_ID_MSG_QueryExit,
				       XAP_Dialog_MessageBox::b_YN,
				       XAP_Dialog_MessageBox::a_NO)
		== XAP_Dialog_MessageBox::a_YES);

}
#endif

static XAP_Dialog_MessageBox::tAnswer s_AskSaveFile(XAP_Frame * pFrame)
{
	XAP_Dialog_MessageBox * message = 
		pFrame->createMessageBox(AP_STRING_ID_MSG_ConfirmSave,
					 XAP_Dialog_MessageBox::b_YNC,
					 XAP_Dialog_MessageBox::a_YES,
					 pFrame->getNonDecoratedTitle());
	message->setSecondaryMessage(AP_STRING_ID_MSG_ConfirmSaveSecondary);
	return pFrame->showMessageBox(message);
}

static bool s_AskForPathname(XAP_Frame * pFrame,
				 bool bSaveAs,
				 XAP_Dialog_Id id,
				 const char * pSuggestedName,
				 char ** ppPathname,
				 IEFileType * ieft)
{
	// raise the file-open or file-save-as dialog.
	// return a_OK or a_CANCEL depending on which button
	// the user hits.
	// return a pointer a g_strdup()'d string containing the
	// pathname the user entered -- ownership of this goes
	// to the caller (so g_free it when you're done with it).

	UT_DEBUGMSG(("s_AskForPathname: frame %p, bSaveAs %d, suggest=[%s]\n",
				 (void*)pFrame, bSaveAs, ((pSuggestedName) ? pSuggestedName : "")));

	UT_return_val_if_fail (ppPathname, false);
	*ppPathname = nullptr;

	if (pFrame) {
		pFrame->raise();
	}
	
	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());

	XAP_Dialog_FileOpenSaveAs * pDialog
		= static_cast<XAP_Dialog_FileOpenSaveAs *>(pDialogFactory->requestDialog(id));
	UT_return_val_if_fail (pDialog, false);

	if (pSuggestedName && *pSuggestedName)
	{
		// if caller wants to suggest a name, use it and seed the
		// dialog in that directory and set the filename.
		pDialog->setCurrentPathname(pSuggestedName);
		pDialog->setSuggestFilename(true);
	}
	else if (pFrame)
	{
		// if caller does not want to suggest a name, seed the dialog
		// to the directory containing this document (if it has a
		// name), but don't put anything in the filename portion.
		PD_Document * pDoc = static_cast<PD_Document*>(pFrame->getCurrentDoc());
		std::string title;

		if (pDoc->getMetaDataProp (PD_META_KEY_TITLE, title) && !title.empty())
		{
#if 0
			// the metadata is returned to us in utf8; we have to convert it to whatever
			// the c-lib library uses
			const char * encoding;
			bool bSet = false;
			
			if(g_ascii_strcasecmp(l.getEncoding(), "UTF-8") != 0)
			{
				UT_iconv_t  cd = UT_iconv_open(l.getEncoding(), "UTF-8");

				if(UT_iconv_isValid(cd));
				{
					const char * pTitle = title.c_str();
					int bytes = title.size();
					int left;
					char out[500];
					char *out_ptr = out;
					int res = UT_iconv(cd, &pTitle, &bytes, &out,&left);
					if (res != (size_t) -1 && bytes == 0)
					{
						out[500 - outbytes] = '\0';
						pDialog->setCurrentPathname(out);
						bSet = true;
					}
				}
			}

			if(!bSet)
				pDialog->setCurrentPathname(title);
#else
			UT_legalizeFileName(title);
			pDialog->setCurrentPathname(title);
#endif
			pDialog->setSuggestFilename(true);
		} else {
			pDialog->setCurrentPathname(pFrame->getFilename());
			pDialog->setSuggestFilename(false);
		}
	}
	else {
		// we don't have a frame. This is likely that we are going to open
		// so don't need to suggest a name.
		pDialog->setSuggestFilename(false);
	}

	// to fill the file types popup list, we need to convert
	// AP-level Imp/Exp descriptions, suffixes, and types into
	// strings.

	UT_uint32 filterCount = 0;

	if (bSaveAs)
		filterCount = IE_Exp::getExporterCount();
	else
		filterCount = IE_Imp::getImporterCount();

	const char ** szDescList = static_cast<const char **>(UT_calloc(filterCount + 1, sizeof(char *)));
	UT_return_val_if_fail(szDescList, false);

	const char ** szSuffixList = static_cast<const char **>(UT_calloc(filterCount + 1, sizeof(char *)));
	if(!szSuffixList)
	{
		UT_ASSERT_HARMLESS(szSuffixList);
		FREEP(szDescList);
		return false;
	}

	IEFileType * nTypeList = static_cast<IEFileType *>(UT_calloc(filterCount + 1, sizeof(IEFileType)));
	if(!nTypeList)
	{
		UT_ASSERT_HARMLESS(nTypeList);
		FREEP(szDescList);
		FREEP(szSuffixList);
		return false;
	}

	UT_uint32 k = 0;

	if (bSaveAs)
		while (IE_Exp::enumerateDlgLabels(k, &szDescList[k], &szSuffixList[k], &nTypeList[k]))
			k++;
	else
		while (IE_Imp::enumerateDlgLabels(k, &szDescList[k], &szSuffixList[k], &nTypeList[k]))
			k++;

	pDialog->setFileTypeList(szDescList, szSuffixList, static_cast<const UT_sint32 *>(nTypeList));

	// Abinova uses IEFT_Abinova_1 as the default

	// try to remember the previous file type
	static IEFileType dflFileType = IEFT_Bogus;

	// if a file format was given to us, then use that
	if (ieft != nullptr && *ieft != IEFT_Bogus)
	  {
		// have a pre-existing file format, try to default to that
		UT_DEBUGMSG(("DOM: using given filetype %d\n", *ieft));
		dflFileType = *ieft;
	  }
	else if (bSaveAs)
	  {
		XAP_App * pApp = XAP_App::getApp();
		if(!pApp)
		{
			UT_ASSERT_HARMLESS(pApp);
			FREEP(szDescList);
			FREEP(szSuffixList);
			FREEP(nTypeList);
			return false;
		}

		XAP_Prefs * pPrefs = pApp->getPrefs();
		if(!pPrefs)
		{
			UT_ASSERT_HARMLESS(pPrefs);
			FREEP(szDescList);
			FREEP(szSuffixList);
			FREEP(nTypeList);
			return false;
		}

		std::string ftype;

		// fetch the default save format
		pPrefs->getPrefsValue(AP_PREF_KEY_DefaultSaveFormat, ftype, true);
		if (!ftype.empty()) {
			// load the default file format
			dflFileType = IE_Exp::fileTypeForSuffix(ftype.c_str());
			UT_DEBUGMSG(("DOM: reverting to default file type: %s (%d)\n", ftype.c_str(), dflFileType));
		} else {
			UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		}
	  }
	else
	  {
		// try to load ABW by default
		dflFileType = IE_Imp::fileTypeForSuffix (".abwn");
	  }

	pDialog->setDefaultFileType(dflFileType);
	UT_DEBUGMSG(("About to runModal on FileOpen \n"));
	pDialog->runModal(pFrame);

	XAP_Dialog_FileOpenSaveAs::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == XAP_Dialog_FileOpenSaveAs::a_OK);

	if (bOK)
	{
		const std::string & resultPathname = pDialog->getPathname();
		if (!resultPathname.empty()) {
			*ppPathname = g_strdup(resultPathname.c_str());
		}

		UT_sint32 type = pDialog->getFileType();
		dflFileType = type;

		// If the number is negative, it's a special type.
		// Some operating systems which depend solely on filename
		// suffixes to identify type (like Windows) will always
		// want auto-detection.
		if (type < 0)
			switch (type)
			{
			case XAP_DIALOG_FILEOPENSAVEAS_FILE_TYPE_AUTO:
				// do some automagical detecting
				*ieft = IEFT_Unknown;
				break;
			default:
				// it returned a type we don't know how to handle
				UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
			}
		else
			*ieft = static_cast<IEFileType>(pDialog->getFileType());

		// If the user asked for password protection in the save dialog,
		// stash it on the document for exporters that support it.
		if (bSaveAs && pFrame)
		{
			PD_Document * pDoc = static_cast<PD_Document*>(pFrame->getCurrentDoc());
			if (pDoc)
				pDoc->setSavePassword(pDialog->getEncryptionPassword());
		}
	}

	FREEP(szDescList);
	FREEP(szSuffixList);
	FREEP(nTypeList);

	pDialogFactory->releaseDialog(pDialog);
	return bOK;
}

static bool s_AskForGraphicPathname(XAP_Frame * pFrame,
					   char ** ppPathname,
					   IEGraphicFileType * iegft)
{
	// raise the file-open dialog for inserting an image.
	// return a_OK or a_CANCEL depending on which button
	// the user hits.
	// return a pointer a g_strdup()'d string containing the
	// pathname the user entered -- ownership of this goes
	// to the caller (so g_free it when you're done with it).

	UT_DEBUGMSG(("s_AskForGraphicPathname: frame %p\n",
				 (void*)pFrame));

	UT_return_val_if_fail (ppPathname, false);
	*ppPathname = nullptr;

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_FileOpenSaveAs * pDialog
		= static_cast<XAP_Dialog_FileOpenSaveAs *>(pDialogFactory->requestDialog(XAP_DIALOG_ID_INSERT_PICTURE));
	UT_return_val_if_fail (pDialog, false);

	pDialog->setCurrentPathname("");
	pDialog->setSuggestFilename(false);

	// to fill the file types popup list, we need to convert AP-level
	// ImpGraphic descriptions, suffixes, and types into strings.

	UT_uint32 filterCount = IE_ImpGraphic::getImporterCount();

	const char ** szDescList = static_cast<const char **>(UT_calloc(filterCount + 1, sizeof(char *)));
	UT_return_val_if_fail(szDescList, false);

	const char ** szSuffixList = static_cast<const char **>(UT_calloc(filterCount + 1, sizeof(char *)));
	if(!szSuffixList)
	{
		UT_ASSERT_HARMLESS(szSuffixList);
		FREEP(szDescList);
		return false;
	}

	IEGraphicFileType * nTypeList = (IEGraphicFileType *)
		 UT_calloc(filterCount + 1,	sizeof(IEGraphicFileType));
	if(!nTypeList)
	{
		UT_ASSERT_HARMLESS(nTypeList);
		FREEP(szDescList);
		FREEP(szSuffixList);
		return false;
	}

	UT_uint32 k = 0;

	while (IE_ImpGraphic::enumerateDlgLabels(k, &szDescList[k], &szSuffixList[k], &nTypeList[k]))
		k++;

	pDialog->setFileTypeList(szDescList, szSuffixList, static_cast<const UT_sint32 *>(nTypeList));
	if (iegft != nullptr)
	  pDialog->setDefaultFileType(*iegft);
	pDialog->runModal(pFrame);

	XAP_Dialog_FileOpenSaveAs::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == XAP_Dialog_FileOpenSaveAs::a_OK);

	if (bOK)
	{
		const std::string & resultPathname = pDialog->getPathname();
		if (!resultPathname.empty()) {
			*ppPathname = g_strdup(resultPathname.c_str());
		}

		UT_sint32 type = pDialog->getFileType();

		// If the number is negative, it's a special type.
		// Some operating systems which depend solely on filename
		// suffixes to identify type (like Windows) will always
		// want auto-detection.
		if (type < 0)
			switch (type)
			{
			case XAP_DIALOG_FILEOPENSAVEAS_FILE_TYPE_AUTO:
				// do some automagical detecting
				*iegft = IEGFT_Unknown;
				break;
			default:
				// it returned a type we don't know how to handle
				UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
			}
		else
			*iegft = static_cast<IEGraphicFileType>(pDialog->getFileType());
	}

	FREEP(szDescList);
	FREEP(szSuffixList);
	FREEP(nTypeList);

	pDialogFactory->releaseDialog(pDialog);

	return bOK;
}

/*****************************************************************/
/*****************************************************************/

XAP_Dialog_MessageBox::tAnswer s_CouldNotLoadFileMessage(XAP_Frame * pFrame, const char * pNewFile, UT_Error errorCode)
{
	XAP_String_Id String_id;

	switch (errorCode)
	  {
	  case UT_IE_FILENOTFOUND:
		String_id = AP_STRING_ID_MSG_IE_FileNotFound;
		break;

	  case UT_IE_NOMEMORY:
		String_id = AP_STRING_ID_MSG_IE_NoMemory;
		break;

	  case UT_IE_UNKNOWNTYPE:
		String_id = AP_STRING_ID_MSG_IE_UnsupportedType;
		//AP_STRING_ID_MSG_IE_UnknownType;
		break;

	  case UT_IE_BOGUSDOCUMENT:
		String_id = AP_STRING_ID_MSG_IE_BogusDocument;
		break;

	  case UT_IE_COULDNOTOPEN:
		String_id = AP_STRING_ID_MSG_IE_CouldNotOpen;
		break;

	  case UT_IE_COULDNOTWRITE:
		String_id = AP_STRING_ID_MSG_IE_CouldNotWrite;
		break;

	  case UT_IE_FAKETYPE:
		String_id = AP_STRING_ID_MSG_IE_FakeType;
		break;

	  case UT_IE_UNSUPTYPE:
		String_id = AP_STRING_ID_MSG_IE_UnsupportedType;
		break;

      case UT_IE_TRY_RECOVER:
		String_id = AP_STRING_ID_MSG_OpenRecovered;
		break;        

	  default:
		String_id = AP_STRING_ID_MSG_ImportError;
	  }

	return pFrame->showMessageBox(String_id,
									XAP_Dialog_MessageBox::b_O,
									XAP_Dialog_MessageBox::a_OK,
									pNewFile);
}

UT_Error fileOpen(XAP_Frame * pFrame, const char * pNewFile, IEFileType ieft)
{
	UT_DEBUGMSG(("fileOpen: loading [%s]\n",pNewFile));
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail (pApp, UT_ERROR);

	XAP_Frame * pNewFrame = nullptr;
	// not needed bool bRes = false;
	UT_Error errorCode = UT_IE_IMPORTERROR;

	// see if requested file is already open in another frame
	UT_sint32 ndx = pApp->findFrame(pNewFile);
	if (ndx >= 0)
	{
		// yep, reuse it
		pNewFrame = pApp->getFrame(ndx);
		UT_return_val_if_fail (pNewFrame, UT_ERROR);

		if (s_AskRevertFile(pNewFrame))
		{
			// re-load the document in pNewFrame
			s_StartStopLoadingCursor( true,pNewFrame);
			errorCode = pNewFrame->loadDocument(pNewFile, ieft);
			if (UT_IS_IE_SUCCESS(errorCode))
			{
				pNewFrame->show();
			}
			// even UT_IE_TRY_RECORVER
			if (errorCode)
			{
				s_CouldNotLoadFileMessage(pNewFrame,pNewFile, errorCode);
			}
		}
		else
		{
			// cancel the FileOpen.
			errorCode = UT_OK;		// don't remove from recent list
		}
		s_StartStopLoadingCursor( false,nullptr);
		return errorCode;
	}

	// For widgetized Abinova, if there is a prexisting document in the 
	// Frame, we save it then open a the new document in the same frame

	if(pFrame)
	{
		AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());		
		if(pFrameData && pFrameData->m_bIsWidget)
		 {
			 if(pFrame->isDirty())
			 {
				 AV_View * pAV_View = pFrame->getCurrentView();
				 EV_EditMethodCallData * pCallData = nullptr;
				 EX(saveImmediate);
			 }


			 s_StartStopLoadingCursor( true,pFrame);
			 errorCode = pFrame->loadDocument(pNewFile, ieft);
			 if (UT_IS_IE_SUCCESS(errorCode))
			 {
				 pFrame->updateZoom();
				 pFrame->show();
			 }
			 if (errorCode)
			 {
				 s_CouldNotLoadFileMessage(pFrame,pNewFile, errorCode);
			 }
			 s_StartStopLoadingCursor( false,nullptr);
			 return errorCode;
		 } 
	}

	// We generally open documents in a new frame, which keeps the
	// contents of the current frame available.
	// However, as a convenience we do replace the contents of the
	// current frame if it's the only top-level view on an empty,
	// untitled document.

	if ((pFrame == nullptr) || pFrame->isDirty() || (pFrame->getFilename() && *pFrame->getFilename()) || (pFrame->getViewNumber() > 0))
	{
		// open new document in a new frame.  if we fail,
		// put up an error dialog on current frame (our
		// new one is not completely instantiated) and
		// return.	we do not create a new untitled document
		// in this case.
		pNewFrame = pApp->newFrame();
		if (!pNewFrame)
		{
			s_StartStopLoadingCursor( false,nullptr);
			return false;
		}

// Open a complete but blank frame, then load the document into it

		errorCode = pNewFrame->loadDocument((const char *)nullptr, IEFT_Unknown);
		if (UT_IS_IE_SUCCESS(errorCode))
		{
			pNewFrame->show();
		}
	    else
		{
			return false;
		}


		s_StartStopLoadingCursor( true,pNewFrame);
		errorCode = pNewFrame->loadDocument(pNewFile, ieft);
		if (UT_IS_IE_SUCCESS(errorCode))
		{
			pNewFrame->show();
		}
#if 0
		else
		{
			// TODO there is a problem with the way we create a
			// TODO new frame and then load a documentent into
			// TODO it.  if we try to load pNewFile and fail,
			// TODO and then destroy the window, and raise a
			// TODO message box (on the original window) we get
			// TODO nasty race on UNIX.  raising the dialog and
			// TODO waiting for input flushes out the show-windows
			// TODO on the new (and not yet completely instantiated)
			// TODO window.  this causes a view-less top-level
			// TODO window to appear -- which causes lots of
			// TODO expose-related problems... and then other
			// TODO problems which appear to be related to having
			// TODO multiple gtk_main()'s on the stack....
			// TODO
			// TODO for now, we force a new untitled document into
			// TODO the new window and then raise the message on
			// TODO this new window.
			// TODO
			// TODO long term, we may want to modified pApp->newFrame()
			// TODO to take an 'bool bShowWindow' argument....

			// the IEFileType here doesn't really matter since the file name is nullptr
			errorCode = pNewFrame->loadDocument((const char *)nullptr, IEFT_Unknown);
			if (UT_IS_IE_SUCCESS(errorCode)) {
				pNewFrame->updateZoom();
				pNewFrame->show();
			}
			s_CouldNotLoadFileMessage(pNewFrame,pNewFile, errorCode);
		}
#endif
		s_StartStopLoadingCursor( false,nullptr);
		return errorCode;
	}

	// we are replacing the single-view, unmodified, untitled document.
	// if we fail, put up an error message on the current frame
	// and return -- we do not replace this untitled document with a
	// new untitled document.
	s_StartStopLoadingCursor( true,pFrame);
	errorCode = pFrame->loadDocument(pNewFile, ieft);
	if (UT_IS_IE_SUCCESS(errorCode))
	{
		pFrame->updateZoom();
		pFrame->show();
	}
	if (errorCode)
	{
		s_CouldNotLoadFileMessage(pFrame,pNewFile, errorCode);
	}
	s_StartStopLoadingCursor( false,nullptr);
	return errorCode;
}

#define ABIWORD_VIEW	FV_View * pView = static_cast<FV_View *>(pAV_View);

Defun1(importStyles)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pAV_View->getParentData());
	UT_return_val_if_fail(pFrame,false);

	UT_Error error = UT_IE_IMPORTERROR;
	char * pFile = nullptr;
	IEFileType ieft = IEFT_Unknown;
	bool bOK = s_AskForPathname(pFrame,false, XAP_DIALOG_ID_FILE_OPEN, nullptr,&pFile,&ieft);

	if (!bOK || !pFile)
	  return false;

	PD_Document * pDoc = static_cast<PD_Document *>(pFrame->getCurrentDoc());

	UT_return_val_if_fail(pDoc,false);

	error = pDoc->importStyles(pFile,ieft);

	return E2B(error);
}


Defun1(fileOpen)
{
	CHECK_FRAME;
	XAP_Frame * pFrame = nullptr;
	IEFileType ieft = IEFT_Unknown;
	if (pAV_View) {
		pFrame = static_cast<XAP_Frame *> (pAV_View->getParentData());
		UT_return_val_if_fail (pFrame, false);
		ieft = static_cast<PD_Document *>(pFrame->getCurrentDoc())->getLastOpenedType();
	}
	char * pNewFile = nullptr;
	bool bOK = s_AskForPathname(pFrame,false, XAP_DIALOG_ID_FILE_OPEN, nullptr,&pNewFile,&ieft);

	if (!bOK || !pNewFile)
	  return false;

	// we own storage for pNewFile and must g_free it.

	UT_Error error = ::fileOpen(pFrame, pNewFile, ieft);

	g_free(pNewFile);
	return E2B(error);
}

static UT_Error
s_importFile (XAP_Frame * pFrame, const char * pNewFile, IEFileType ieft)
{
	UT_DEBUGMSG(("fileOpen: loading [%s]\n",pNewFile));
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail (pApp, UT_ERROR);
	XAP_Frame * pNewFrame = nullptr;
	// not needed bool bRes = false;
	UT_Error errorCode = UT_IE_IMPORTERROR;

	// We open documents in a new frame, which keeps the
	// contents of the current frame available.
	// However, as a convenience we do replace the contents of the
	// current frame if it's the only top-level view on an empty,
	// untitled document.

	if ((pFrame == nullptr) || pFrame->isDirty() || pFrame->getFilename() || (pFrame->getViewNumber() > 0))
	{
		// open new document in a new frame.  if we fail,
		// put up an error dialog on current frame (our
		// new one is not completely instantiated) and
		// return.	we do not create a new untitled document
		// in this case.

		pNewFrame = pApp->newFrame();
		if (!pNewFrame)
		{
			s_StartStopLoadingCursor( false,nullptr);
			return false;
		}

		// treat import as creating a new, dirty document that
		// must be saved to be made 'clean'
		s_StartStopLoadingCursor( true,pNewFrame);
		errorCode = pNewFrame->importDocument(pNewFile, ieft, false);
		if (!errorCode)
		{
			pNewFrame->show(); // don't add to the MRU
		}
		else
		{
			// see problem documented in ::fileOpen()
			errorCode = pNewFrame->loadDocument((const char *)nullptr, IEFT_Unknown);
			if (!errorCode)
				pNewFrame->show();
			s_CouldNotLoadFileMessage(pNewFrame,pNewFile, errorCode);
		}
		s_StartStopLoadingCursor( false,nullptr);
		return errorCode;
	}

	// we are replacing the single-view, unmodified, untitled document.
	// if we fail, put up an error message on the current frame
	// and return -- we do not replace this untitled document with a
	// new untitled document.
	s_StartStopLoadingCursor( true,pFrame);
	errorCode = pFrame->importDocument(pNewFile, ieft);
	if (UT_IS_IE_SUCCESS(errorCode))
	{
		pFrame->show(); // don't add to the MRU
	}
	if (errorCode)
	{
		s_CouldNotLoadFileMessage(pFrame,pNewFile, errorCode);
	}
	s_StartStopLoadingCursor( false,nullptr);
	return errorCode;
}

Defun1(openTemplate)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail (pFrame, false);

	char * pNewFile = nullptr;
	IEFileType ieft = static_cast<PD_Document *>(pFrame->getCurrentDoc())->getLastOpenedType();
	bool bOK = s_AskForPathname(pFrame,false, XAP_DIALOG_ID_FILE_IMPORT, nullptr,&pNewFile,&ieft);

	if (!bOK || !pNewFile)
	  return false;

	// we own storage for pNewFile and must g_free it.

	UT_Error error = s_importFile(pFrame, pNewFile, ieft);

	g_free(pNewFile);
	return E2B(error);
}

Defun(saveImmediate)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pAV_View->getParentData());
	UT_return_val_if_fail (pFrame, false);
	//
	// If we're connected let the remote document know.
	// We do this with the savedoc signal
	//
	FV_View * pView = static_cast<FV_View *>(pFrame->getCurrentView());
	if(pView)
	{
		PD_Document * pDoc = pView->getDocument();
		if(pDoc && pDoc->isConnected())
		{
			pDoc->signalListeners(PD_SIGNAL_SAVEDOC);
			if (pFrame->getViewNumber() > 0)
			{
				XAP_App * pApp = XAP_App::getApp();
				UT_return_val_if_fail (pApp, false);

				pApp->updateClones(pFrame);
			}
			if(!pDoc->isDirty())
				return true;
		}
	}
	// can only save without prompting if filename already known

	if (!pFrame->getFilename())
   		return EX(fileSaveAs);

	UT_Error errSaved;
	errSaved = pAV_View->cmdSave();
	
	// if it has a problematic extension save as instead
	//	if (errSaved == UT_EXTENSIONERROR)
	//  return EX(fileSaveAs);

	if (errSaved)
	{
		// throw up a dialog
		s_TellSaveFailed(pFrame, pFrame->getFilename(), errSaved);
		return false;
	}

	if (pFrame->getViewNumber() > 0)
	{
		XAP_App * pApp = XAP_App::getApp();
		UT_return_val_if_fail (pApp, false);

		pApp->updateClones(pFrame);
	}

	return true;
}

Defun(fileSave)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pAV_View->getParentData());
	UT_return_val_if_fail (pFrame, false);
	//
	// If we're connected let the remote document know
	// We do this with the savedoc signal
	//
	FV_View * pView = static_cast<FV_View *>(pFrame->getCurrentView());
	if(pView)
	{
		PD_Document * pDoc = pView->getDocument();
		if(pDoc && pDoc->isConnected())
		{
			pDoc->signalListeners(PD_SIGNAL_SAVEDOC);
			if (pFrame->getViewNumber() > 0)
			{
				XAP_App * pApp = XAP_App::getApp();
				UT_return_val_if_fail (pApp, false);

				pApp->updateClones(pFrame);
			}
			if(!pDoc->isDirty())
				return true;
		}
	}
	// can only save without prompting if filename already known

	auto filename = pFrame->getFilename();
	if (!filename || !*filename)
   		return EX(fileSaveAs);

	UT_Error errSaved;
	errSaved = pAV_View->cmdSave();
	
	// if it has a problematic extension save as instead
	if (errSaved == UT_EXTENSIONERROR)
		return EX(fileSaveAs);

	if (errSaved)
	{
		// throw up a dialog
		s_TellSaveFailed(pFrame, pFrame->getFilename(), errSaved);
		return false;
	}

	if (pFrame->getViewNumber() > 0)
	{
		XAP_App * pApp = XAP_App::getApp();
		UT_return_val_if_fail (pApp, false);

		pApp->updateClones(pFrame);
	}

	return true;
}

static bool
s_actuallySaveAs(AV_View * pAV_View, bool overwriteName)
{
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail (pFrame, false);

	IEFileType ieft = IEFT_Bogus; // IEFT_Bogus will let the file dialog fall back to the default format

	//ieft = static_cast<PD_Document *>(pFrame->getCurrentDoc())->getLastSavedAsType();

	char * pNewFile = nullptr;
	XAP_Dialog_Id id = XAP_DIALOG_ID_FILE_SAVEAS;

	if ( !overwriteName )
	  id = XAP_DIALOG_ID_FILE_EXPORT;

	// the dialog stashes any requested encryption password on the
	// document before the save happens; remember the old state so a
	// failed save does not silently change it
	PD_Document * pDocForPw = static_cast<PD_Document*>(pFrame->getCurrentDoc());
	const std::string oldPassword = pDocForPw ? pDocForPw->getSavePassword() : "";

	bool bOK = s_AskForPathname(pFrame,true, id, pFrame->getFilename(),&pNewFile,&ieft);

	if (!bOK || !pNewFile)
	{
		if (pDocForPw)
			pDocForPw->setSavePassword(oldPassword);
		return false;
	}

	UT_DEBUGMSG(("fileSaveAs: saving as [%s]\n",pNewFile));

	UT_Error errSaved;
	errSaved = pAV_View->cmdSaveAs(pNewFile, static_cast<int>(ieft), overwriteName);
	if (errSaved)
	{
		if (pDocForPw)
			pDocForPw->setSavePassword(oldPassword);
		// throw up a dialog
		s_TellSaveFailed(pFrame, pNewFile, errSaved);
		g_free(pNewFile);
		return false;
	}

	g_free(pNewFile);

	// ignore all of this stuff
	if (!overwriteName)
		return bOK;

	// update the MRU list
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail (pApp, false);

	if (pFrame->getViewNumber() > 0)
	{
		// renumber clones
		pApp->updateClones(pFrame);
	}

	return true;
}

Defun1(fileExport)
{
	CHECK_FRAME;
	return s_actuallySaveAs(pAV_View, false);
}

Defun1(fileImport)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail (pFrame, false);

	char * pNewFile = nullptr;
	IEFileType ieft = static_cast<PD_Document *>(pFrame->getCurrentDoc())->getLastOpenedType();
	bool bOK = s_AskForPathname(pFrame,false, XAP_DIALOG_ID_FILE_IMPORT, nullptr,&pNewFile,&ieft);

	if (!bOK || !pNewFile)
	  return false;

	// we own storage for pNewFile and must g_free it.

	UT_Error error = s_importFile(pFrame, pNewFile, ieft);

	g_free(pNewFile);
	return E2B(error);
}

Defun1(fileSaveAs)
{
	CHECK_FRAME;
	return s_actuallySaveAs(pAV_View, true);
}

Defun1(fileSaveTemplate)
{
  CHECK_FRAME;

  UT_return_val_if_fail (pAV_View, false);
  XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
  UT_return_val_if_fail (pFrame, false);

  IEFileType ieft = IE_Exp::fileTypeForSuffix ( ".awt" ) ;
  char * pNewFile = nullptr;
  XAP_Dialog_Id id = XAP_DIALOG_ID_FILE_SAVEAS;

  UT_String suggestedName (XAP_App::getApp()->getUserPrivateDirectory());
  suggestedName += "/templates/" ;

  bool bOK = s_AskForPathname(pFrame,true, id, suggestedName.c_str(),&pNewFile,&ieft);

  if (!bOK || !pNewFile)
    return false;

  UT_DEBUGMSG(("fileSaveTemplate: saving as [%s]\n",pNewFile));

  UT_Error errSaved;
  errSaved = pAV_View->cmdSaveAs(pNewFile, static_cast<int>(ieft), false);
  if (errSaved)
    {
      // throw up a dialog
      s_TellSaveFailed(pFrame, pNewFile, errSaved);
      g_free(pNewFile);
      return false;
    }

  return bOK;
}

Defun1(fileSaveAsWeb)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pAV_View->getParentData());
  IEFileType ieft = IE_Exp::fileTypeForSuffix (".xhtml");
  char * pNewFile = nullptr;
  bool bOK = s_AskForPathname(pFrame,true, XAP_DIALOG_ID_FILE_SAVEAS, pFrame->getFilename(),&pNewFile,&ieft);

  if (!bOK || !pNewFile)
	return false;

  UT_Error errSaved;
  errSaved = pAV_View->cmdSaveAs(pNewFile, ieft);
  if (errSaved)
	{
	  // throw up a dialog
	  s_TellSaveFailed(pFrame, pNewFile, errSaved);
	  g_free(pNewFile);
	  return false;
	}

  return true;
}

Defun1(fileSaveImage)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_FileOpenSaveAs * pDialog
		= static_cast<XAP_Dialog_FileOpenSaveAs *>(pDialogFactory->requestDialog(XAP_DIALOG_ID_FILE_SAVE_IMAGE));
	UT_return_val_if_fail (pDialog, false);

	UT_uint32 filterCount = 1;
	const char ** szDescList = static_cast<const char **>(UT_calloc(filterCount + 1, sizeof(char *)));
	UT_return_val_if_fail(szDescList, false);

	const char ** szSuffixList = static_cast<const char **>(UT_calloc(filterCount + 1, sizeof(char *)));
	if(!szSuffixList)
	{
		UT_ASSERT_HARMLESS(szSuffixList);
		FREEP(szDescList);
		return false;
	}

	IEFileType * nTypeList = static_cast<IEFileType *>(UT_calloc(filterCount + 1, sizeof(IEFileType)));
	if(!nTypeList)
	{
		UT_ASSERT_HARMLESS(nTypeList);
		FREEP(szDescList);
		FREEP(szSuffixList);
		return false;
	}

	// we only support saving images in png format for now
	szDescList[0] = "Portable Network Graphics (.png)";
	szSuffixList[0] = "*.png";
	nTypeList[0] = static_cast<IEFileType>(1);

	pDialog->setFileTypeList(szDescList, szSuffixList,
							 static_cast<const UT_sint32 *>(nTypeList));

	pDialog->setDefaultFileType(static_cast<IEFileType>(1));

	pDialog->runModal(pFrame);

	XAP_Dialog_FileOpenSaveAs::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == XAP_Dialog_FileOpenSaveAs::a_OK);

	if (bOK)
	{
		const std::string resultPathname = pDialog->getPathname();
		if (!resultPathname.empty()) {
			FV_View * pView = static_cast<FV_View *>(pAV_View);
			pView->saveSelectedImage (resultPathname.c_str());
		}
	}

	FREEP(szDescList);
	FREEP(szSuffixList);
	FREEP(nTypeList);

	pDialogFactory->releaseDialog(pDialog);

	return true;
}


Defun1(fileSaveEmbed)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	FV_View * pView = static_cast<FV_View *>(pAV_View);
	fp_EmbedRun *pRun = dynamic_cast <fp_EmbedRun*> (pView->getSelectedObject ());
	UT_return_val_if_fail(pRun, false);

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_FileOpenSaveAs * pDialog
		= static_cast<XAP_Dialog_FileOpenSaveAs *>(pDialogFactory->requestDialog(XAP_DIALOG_ID_FILE_SAVEAS));
	UT_return_val_if_fail (pDialog, false);

	UT_uint32 filterCount = 1;
	const char ** szDescList = static_cast<const char **>(UT_calloc(filterCount + 1, sizeof(char *)));
	UT_return_val_if_fail(szDescList, false);

	const char ** szSuffixList = static_cast<const char **>(UT_calloc(filterCount + 1, sizeof(char *)));
	if(!szSuffixList)
	{
		UT_ASSERT_HARMLESS(szSuffixList);
		FREEP(szDescList);
		return false;
	}

	IEFileType * nTypeList = static_cast<IEFileType *>(UT_calloc(filterCount + 1, sizeof(IEFileType)));
	if(!nTypeList)
	{
		UT_ASSERT_HARMLESS(nTypeList);
		FREEP(szDescList);
		FREEP(szSuffixList);
		return false;
	}

	// we only support saving objects to their default format
	szDescList[0] =  pRun->getEmbedManager()->getMimeTypeDescription();
	szSuffixList[0] = pRun->getEmbedManager()->getMimeTypeSuffix();
	nTypeList[0] = static_cast<IEFileType>(1);

	pDialog->setFileTypeList(szDescList, szSuffixList,
							 static_cast<const UT_sint32 *>(nTypeList));

	pDialog->setDefaultFileType(static_cast<IEFileType>(1));

	pDialog->runModal(pFrame);

	XAP_Dialog_FileOpenSaveAs::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == XAP_Dialog_FileOpenSaveAs::a_OK);

	if (bOK)
	{
		const std::string & resultPathname = pDialog->getPathname();
		if (!resultPathname.empty()) {
			UT_ConstByteBufPtr Buf;
			pView->getDocument()->getDataItemDataByName(pRun->getDataID(), Buf, nullptr, nullptr);
			if (Buf)
				Buf->writeToURI(resultPathname.c_str());
		}
	}

	FREEP(szDescList);
	FREEP(szSuffixList);
	FREEP(nTypeList);

	pDialogFactory->releaseDialog(pDialog);

	return true;
}


Defun1(undo)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	pAV_View->cmdUndo(1);
	return true;
}

Defun1(redo)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	pAV_View->cmdRedo(1);
	return true;
}

Defun1(newWindow)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_Frame * pClone = pFrame->cloneFrame();
	if(!pClone)
	{
		return false;
	}
	s_StartStopLoadingCursor(true,pClone);
	pClone = pFrame->buildFrame(pClone);
	s_StartStopLoadingCursor(false,pClone);
	return (pClone ? true : false);
}



static bool _activateWindow(AV_View* pAV_View, UT_sint32 ndx)
{
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail (pApp, false);

	UT_return_val_if_fail (ndx > 0 && ndx <= pApp->getFrameCount(), false);


	XAP_Frame * pSelFrame = pApp->getFrame(ndx - 1);

	if (pSelFrame)
		pSelFrame->raise();

	return true;
}

Defun1(activateWindow_1)
{
	CHECK_FRAME;
	return _activateWindow(pAV_View, 1);
}
Defun1(activateWindow_2)
{
	CHECK_FRAME;
	return _activateWindow(pAV_View, 2);
}
Defun1(activateWindow_3)
{
	CHECK_FRAME;
	return _activateWindow(pAV_View, 3);
}
Defun1(activateWindow_4)
{
	CHECK_FRAME;
	return _activateWindow(pAV_View, 4);
}
Defun1(activateWindow_5)
{
	CHECK_FRAME;
	return _activateWindow(pAV_View, 5);
}
Defun1(activateWindow_6)
{
	CHECK_FRAME;
	return _activateWindow(pAV_View, 6);
}
Defun1(activateWindow_7)
{
	CHECK_FRAME;
	return _activateWindow(pAV_View, 7);
}
Defun1(activateWindow_8)
{
	CHECK_FRAME;
	return _activateWindow(pAV_View, 8);
}
Defun1(activateWindow_9)
{
	CHECK_FRAME;
	return _activateWindow(pAV_View, 9);
}

static bool s_doMoreWindowsDlg(XAP_Frame* pFrame, XAP_Dialog_Id id)
{
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_WindowMore * pDialog
		= static_cast<XAP_Dialog_WindowMore *>(pDialogFactory->requestDialog(id));
	UT_return_val_if_fail (pDialog, false);

	// run the dialog
	pDialog->runModal(pFrame);

	XAP_Frame * pSelFrame = nullptr;
	bool bOK = (pDialog->getAnswer() == XAP_Dialog_WindowMore::a_OK);

	if (bOK)
		pSelFrame = pDialog->getSelFrame();

	pDialogFactory->releaseDialog(pDialog);

	// now do it
	if (pSelFrame)
		pSelFrame->raise();

	return bOK;
}

Defun1(dlgMoreWindows)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	s_doMoreWindowsDlg(pFrame, XAP_DIALOG_ID_WINDOWMORE);
	return true;
}

static bool s_doAboutDlg(XAP_Frame* pFrame, XAP_Dialog_Id id)
{
	if (pFrame) {
		pFrame->raise();
	}
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail (pApp, false);

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pApp->getDialogFactory());

	XAP_Dialog_About * pDialog
		= static_cast<XAP_Dialog_About *>(pDialogFactory->requestDialog(id));
	UT_return_val_if_fail (pDialog, false);

	// run the dialog (it should really be modeless if anyone
	// gets the urge to make it safe that way)
	pDialog->runModal(pFrame);

	bool bOK = true;

	pDialogFactory->releaseDialog(pDialog);

	return bOK;
}

static bool s_doToggleCase(XAP_Frame * pFrame, FV_View * pView, XAP_Dialog_Id id)
{
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

#if 0
	// we do not need selection (if there is none, we will try to apply
	// the case to the word at editing position)
	if (pView->isSelectionEmpty())
	  {
		pFrame->showMessageBox(AP_STRING_ID_MSG_EmptySelection,
				   XAP_Dialog_MessageBox::b_O,
				   XAP_Dialog_MessageBox::a_OK);
		return false;
	  }
#endif

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_ToggleCase * pDialog
		= static_cast<AP_Dialog_ToggleCase *>(pDialogFactory->requestDialog(id));
	UT_return_val_if_fail (pDialog, false);

	// run the dialog (it should really be modeless if anyone
	// gets the urge to make it safe that way)
	pDialog->runModal(pFrame);
	bool bOK = (pDialog->getAnswer() == AP_Dialog_ToggleCase::a_OK);

	if (bOK)
	  pView->toggleCase(pDialog->getCase());

	pDialogFactory->releaseDialog(pDialog);

		return bOK;
}

Defun1(dlgToggleCase)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	return s_doToggleCase(pFrame, static_cast<FV_View *>(pAV_View), (XAP_Dialog_Id)AP_DIALOG_ID_TOGGLECASE);
}

Defun1(rotateCase)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	FV_View * pView = static_cast<FV_View *>(pAV_View);
	pView->toggleCase(CASE_ROTATE);

	return true;
}

/* direct Change Case entries for the ribbon "Aa" dropdown,
 * LibreOffice-style */
Defun1(caseSentence)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	FV_View * pView = static_cast<FV_View *>(pAV_View);
	pView->toggleCase(CASE_SENTENCE);

	return true;
}

Defun1(caseLower)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	FV_View * pView = static_cast<FV_View *>(pAV_View);
	pView->toggleCase(CASE_LOWER);

	return true;
}

Defun1(caseUpper)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	FV_View * pView = static_cast<FV_View *>(pAV_View);
	pView->toggleCase(CASE_UPPER);

	return true;
}

Defun1(caseTitle)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	FV_View * pView = static_cast<FV_View *>(pAV_View);
	pView->toggleCase(CASE_TITLE);

	return true;
}

Defun1(caseToggle)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	FV_View * pView = static_cast<FV_View *>(pAV_View);
	pView->toggleCase(CASE_TOGGLE);

	return true;
}

Defun1(dlgAbout)
{
	CHECK_FRAME;
	XAP_Frame * pFrame = nullptr;
	
	if (pAV_View) {
		pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
		UT_return_val_if_fail(pFrame, false);
	}
	
	s_doAboutDlg(pFrame, XAP_DIALOG_ID_ABOUT);

	return true;
}

Defun1(dlgMetaData)
{
  CHECK_FRAME;
  UT_return_val_if_fail (pAV_View, false);
  FV_View * pView = static_cast<FV_View *>(pAV_View);

  XAP_Frame * pFrame = static_cast<XAP_Frame *>(pView->getParentData());
  UT_return_val_if_fail(pFrame, false);

  XAP_App * pApp = XAP_App::getApp();
  UT_return_val_if_fail (pApp, false);

  pFrame->raise();

  XAP_DialogFactory * pDialogFactory
    = static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

  AP_Dialog_MetaData * pDialog
    = static_cast<AP_Dialog_MetaData *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_METADATA));
  UT_return_val_if_fail (pDialog, false);

  // get the properties

  PD_Document * pDocument = pView->getDocument();

  std::string prop;

  if ( pDocument->getMetaDataProp ( PD_META_KEY_TITLE, prop ) )
    pDialog->setTitle ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_SUBJECT, prop ) )
    pDialog->setSubject ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_CREATOR, prop ) )
    pDialog->setAuthor ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_PUBLISHER, prop ) )
    pDialog->setPublisher ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_CONTRIBUTOR, prop ) )
    pDialog->setCoAuthor ( prop ) ;
  // Category is its own property (Word's cp:category); documents saved
  // by older Abinova versions kept it under dc.type, so fall back
  if ( pDocument->getMetaDataProp ( PD_META_KEY_CATEGORY, prop ) ||
       pDocument->getMetaDataProp ( PD_META_KEY_TYPE, prop ) )
    pDialog->setCategory ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_KEYWORDS, prop ) )
    pDialog->setKeywords ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_LANGUAGE, prop ) )
    pDialog->setLanguages ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_SOURCE, prop ) )
    pDialog->setSource ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_RELATION, prop ) )
    pDialog->setRelation ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_COVERAGE, prop ) )
    pDialog->setCoverage ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_RIGHTS, prop ) )
    pDialog->setRights ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_DESCRIPTION, prop ) )
    pDialog->setDescription ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_LASTMODIFIEDBY, prop ) )
    pDialog->setLastSavedBy ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_MANAGER, prop ) )
    pDialog->setManager ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_COMPANY, prop ) )
    pDialog->setCompany ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_TEMPLATE, prop ) )
    pDialog->setTemplate ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_CONTENTSTATUS, prop ) )
    pDialog->setStatus ( prop ) ;

  // statistics: stored document dates plus live counts, Word-style
  if ( pDocument->getMetaDataProp ( PD_META_KEY_DATE, prop ) )
    pDialog->setStatCreated ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_DATE_LAST_CHANGED, prop ) )
    pDialog->setStatModified ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_LASTPRINTED, prop ) )
    pDialog->setStatPrinted ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_REVISION, prop ) )
    pDialog->setStatRevision ( prop ) ;
  if ( pDocument->getMetaDataProp ( PD_META_KEY_EDITING_DURATION, prop ) )
    pDialog->setStatEditingTime ( prop + " min" ) ;

  {
    FV_DocCount cnt = pView->countWords(true);
    pDialog->setStatPages ( std::to_string(cnt.page) ) ;
    pDialog->setStatParas ( std::to_string(cnt.para) ) ;
    pDialog->setStatLines ( std::to_string(cnt.line) ) ;
    pDialog->setStatWords ( std::to_string(cnt.word) ) ;
    pDialog->setStatChars ( std::to_string(cnt.ch_sp) ) ;
  }

  // run the dialog

  pDialog->runModal(pFrame);
  bool bOK = (pDialog->getAnswer() == AP_Dialog_MetaData::a_OK);

  if (bOK)
    {
      // reset the props
      pDocument->setMetaDataProp ( PD_META_KEY_TITLE, pDialog->getTitle() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_SUBJECT, pDialog->getSubject() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_CREATOR, pDialog->getAuthor() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_PUBLISHER, pDialog->getPublisher() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_CONTRIBUTOR, pDialog->getCoAuthor() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_CATEGORY, pDialog->getCategory() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_KEYWORDS, pDialog->getKeywords() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_LANGUAGE, pDialog->getLanguages() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_SOURCE, pDialog->getSource() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_RELATION, pDialog->getRelation() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_COVERAGE, pDialog->getCoverage() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_RIGHTS, pDialog->getRights() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_DESCRIPTION, pDialog->getDescription() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_LASTMODIFIEDBY, pDialog->getLastSavedBy() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_MANAGER, pDialog->getManager() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_COMPANY, pDialog->getCompany() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_TEMPLATE, pDialog->getTemplate() ) ;
      pDocument->setMetaDataProp ( PD_META_KEY_CONTENTSTATUS, pDialog->getStatus() ) ;

	  for(UT_sint32 i = 0;i < pApp->getFrameCount();++i)
	  {
		  pApp->getFrame(i)->updateTitle ();
	  }	  

      pDocument->forceDirty();
    }

  // release the dialog

  pDialogFactory->releaseDialog(pDialog);

  return true ;
}

Defun1(fileNewUsingTemplate)
{
	CHECK_FRAME;
	XAP_Frame * pFrame = nullptr;
	if (pAV_View) {
		FV_View * pView = static_cast<FV_View *>(pAV_View);
	
		pFrame = static_cast<XAP_Frame *>(pView->getParentData());
		UT_return_val_if_fail(pFrame, false);
	
	
		pFrame->raise();
	}
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail (pApp, false);

 	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pApp->getDialogFactory());

	AP_Dialog_New * pDialog
		= static_cast<AP_Dialog_New *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_FILE_NEW));
	UT_return_val_if_fail (pDialog, false);

	pDialog->runModal(pFrame);
	bool bOK = (pDialog->getAnswer() == AP_Dialog_New::a_OK);

	if (bOK)
	{
		UT_String str;
		const char * szStr;

		switch(pDialog->getOpenType())
		{
			// this will just open up a blank document
		case AP_Dialog_New::open_New :
			break;

			// these two will open things as templates
		case AP_Dialog_New::open_Existing :
		case AP_Dialog_New::open_Template :
			szStr = pDialog->getFileName();
			if (szStr)
				str += szStr;
			break;
		}

		if (str.size())
		{
			// we want to create from a template
		    bOK = s_importFile (pFrame, str.c_str(), IEFT_Unknown) == UT_OK;
		}
		else
		{
			// we want a new blank doc
			XAP_Frame * pNewFrame = pApp->newFrame();

			if (pNewFrame)
				pFrame = pNewFrame;

			bOK = pFrame->loadDocument((const char *)nullptr, IEFT_Unknown) == UT_OK;

			if (pNewFrame)
			{
				pNewFrame->show();
			}
		}
	}

	pDialogFactory->releaseDialog(pDialog);
	return bOK;
}

static bool _helpOpenURL(const char* helpURL)
{
	return XAP_App::getApp()->openHelpURL(helpURL);
}

static bool _openURL(const char* url)
{
	return XAP_App::getApp()->openURL(url);
}
	
bool helpLocalizeAndOpenURL(const char* pathBeforeLang, const char* pathAfterLang, const char *remoteURLbase)
{
	UT_String url (XAP_App::getApp()->localizeHelpUrl (pathBeforeLang, pathAfterLang, remoteURLbase));
	return _helpOpenURL(url.c_str());
}

static bool _openHelpWindow(AV_View * pAV_View, const char * page,
							bool bFocusSearch)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail (pApp, false);

	pApp->openHelpWindow(pFrame, page, bFocusSearch);
	return true;
}

Defun1(helpContents)
{
	return _openHelpWindow(pAV_View, "index.html", false);
}

Defun1(helpChangelog)
{
	return _openHelpWindow(pAV_View, "changelog.html", false);
}

Defun1(helpIntro)
{
	return _openHelpWindow(pAV_View, "index.html", false);
}

Defun1(helpCheckVer)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail (pApp, false);

	pApp->checkForUpdates(pFrame);
	return true;
}

Defun0(helpReportBug)
{
	UT_String bugURL ("https://github.com/janos-szenfner/Abinova/issues/new");

  return _openURL(bugURL.c_str());
}

Defun1(helpSearch)
{
	return _openHelpWindow(pAV_View, nullptr, true);
}

Defun1(cycleWindows)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail (pApp, false);

	UT_sint32 ndx = pApp->findFrame(pFrame);
	UT_return_val_if_fail (ndx >= 0, false);

	if (ndx < static_cast<UT_sint32>(pApp->getFrameCount()) - 1)
		ndx++;
	else
		ndx = 0;

	XAP_Frame * pSelFrame = pApp->getFrame(ndx);

	if (pSelFrame)
		pSelFrame->raise();

	return true;
}

Defun1(cycleWindowsBck)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail (pApp, false);

	UT_sint32 ndx = pApp->findFrame(pFrame);
	UT_return_val_if_fail (ndx >= 0, false);

	if (ndx > 0)
		ndx--;
	else
		ndx = pApp->getFrameCount() - 1;

	XAP_Frame * pSelFrame = pApp->getFrame(ndx);

	if (pSelFrame)
		pSelFrame->raise();

	return true;
}

static bool
s_closeWindow (AV_View * pAV_View, EV_EditMethodCallData * pCallData,
		   bool bCanExit)
{
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);

	if(pFrame == pApp->getLastFocussedFrame())
	{

	  // This probabally not necessary given the code that's in xap_App
		// but I hate seg faults.

		pApp->clearLastFocussedFrame();
	}
	if (1 >= pApp->getFrameCount())
	{
		// Delete all the open modeless dialogs

		pApp->closeModelessDlgs();
	}

	// is this the last view on a dirty document?
	bool bRemoteSave = false;
	bool bRet = true;
	if ((pFrame->getViewNumber() == 0) &&
		(pFrame->isDirty()))
	{
		
		XAP_Dialog_MessageBox::tAnswer ans;

		ans = s_AskSaveFile(pFrame);

		switch (ans)
		{
		case XAP_Dialog_MessageBox::a_YES:				// save it first
		{
			//
			// If we're connected let the remote document know.
			// We do this with the savedoc signal
			//
			FV_View * pView = static_cast<FV_View *>(pFrame->getCurrentView());
			if(pView)
			{
				PD_Document * pDoc = pView->getDocument();
				if(pDoc && pDoc->isConnected())
				{
					pDoc->signalListeners(PD_SIGNAL_SAVEDOC);
				}
				bRemoteSave = pDoc->isDirty();
				UT_DEBUGMSG(("remote save %d\n", bRemoteSave));
			}
			if(bRemoteSave)
				bRet = EX(fileSave);
			if (!bRet)								// didn't successfully save,
				return false;					//	  so don't close
		}
		break;

		case XAP_Dialog_MessageBox::a_NO:				// just close it
			break;
			
		case XAP_Dialog_MessageBox::a_CANCEL:			// don't close it
			return false;
			
		default:
			UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
			return false;
			
		}
	}

	// are we the last window?
	if (1 >= pApp->getFrameCount())
	{
		// Delete all the open modeless dialogs

		pApp->closeModelessDlgs();

		// in single XAPAPP mode we can't close the app when closing the last frame
		// or reopen a new one.
#if XAP_SINGLE_XAPAPP
        UT_UNUSED(bCanExit);
#else
		if (bCanExit)
		{
			pApp->reallyExit();
		}
		else
		{
			// keep the app open with an empty document (in this frame)
			pFrame->loadDocument((const char *)nullptr, IEFT_Unknown);
			pFrame->updateZoom();
			pFrame->show();
			return true;
		}
#endif
	}

	// nuke the window

	pApp->forgetFrame(pFrame);
	pFrame->close();
	delete pFrame;

	return true;
}

Defun(closeWindow)
{
	CHECK_FRAME;
	// must, to comply with the HIG
	return s_closeWindow (pAV_View, pCallData, true);
}

Defun(closeWindowX)
{
	CHECK_FRAME;
	return s_closeWindow (pAV_View, pCallData, true);
}

Defun(querySaveAndExit)
{
	CHECK_FRAME;
		
	XAP_Frame * pFrame = nullptr;
	bool bRet = true;

	if (pAV_View) {
		pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
		UT_return_val_if_fail(pFrame, false);
	}
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);

	if (pFrame) {

#if XAP_DONT_CONFIRM_QUIT
#else
		if (1 < pApp->getFrameCount())
		{
			if (!s_AskCloseAllAndExit(pFrame))
			{
				// never mind
				return false;
			}
		}
#endif
	}
	if (pApp->getFrameCount()) {
		UT_uint32 ndx = pApp->getFrameCount();

		// loop over windows, but stop if one can't close
		while (bRet && ndx > 0)
		{
			XAP_Frame * f = pApp->getFrame(ndx - 1);
			UT_return_val_if_fail (f, false);
			pAV_View = f->getCurrentView();
			UT_return_val_if_fail (pAV_View, false);

			bRet = s_closeWindow (pAV_View, pCallData, true);

			ndx--;
		}
	}
	
	if (bRet)
	{
		//	delete all open modeless dialogs
		pApp->closeModelessDlgs();

		// TODO: this shouldn't be necessary, but just in case
		pApp->reallyExit();
	}

	return bRet;
}

/*****************************************************************/
/*****************************************************************/

/*
	NOTE: This file should really be split in two:

		1.	XAP methods (above)
		2.	Abinova-specific methods (below)

	Until we do the necessary architectural work, we just segregate
	the methods within the same file.
*/

Defun1(fileRevert)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

  UT_return_val_if_fail (pAV_View, false);
  XAP_Frame * pFrame = static_cast<XAP_Frame *> (pAV_View->getParentData());

  if (XAP_Dialog_MessageBox::a_YES == pFrame->showMessageBox(AP_STRING_ID_MSG_RevertFile,
								 XAP_Dialog_MessageBox::b_YN,
								 XAP_Dialog_MessageBox::a_NO))
	  pView->cmdUndo ( pView->undoCount(true) - pView->undoCount(false) );

  return true;
}

Defun1(insertClipart)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_DEBUGMSG(("DOM: insert clipart\n"));

	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_ClipArt * pDialog
		= static_cast<XAP_Dialog_ClipArt *>(pDialogFactory->requestDialog(XAP_DIALOG_ID_CLIPART));
UT_return_val_if_fail(pDialog, false);
	// set the initial directory
	UT_String dir(pApp->getAbiSuiteLibDir());
	dir += "/clipart/";

	pDialog->setInitialDir (dir.c_str());

	pDialog->runModal(pFrame);
	bool bOK = (pDialog->getAnswer() == XAP_Dialog_ClipArt::a_OK);
	const char * pNewFile = pDialog->getGraphicName ();

	bool ret = false;

	if (bOK && pNewFile)
	{
		IEGraphicFileType iegft = IEGFT_Unknown;
		FG_ConstGraphicPtr pFG;

		UT_Error errorCode;

		errorCode = IE_ImpGraphic::loadGraphic(pNewFile, iegft, pFG);
		if(errorCode)
		{
			s_CouldNotLoadFileMessage(pFrame, pNewFile, errorCode);
			goto Cleanup;
		}

		errorCode = pView->cmdInsertGraphic(pFG);
		if (errorCode)
		{
			s_CouldNotLoadFileMessage(pFrame, pNewFile, errorCode);
			goto Cleanup;
		}

		ret = true; // goes to Cleanup
	}

 Cleanup:

	pDialogFactory->releaseDialog(pDialog);
	return ret;
}


Defun1(fileInsertGraphic)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	char* pNewFile = nullptr;


	IEGraphicFileType iegft = IEGFT_Unknown;
	bool bOK = s_AskForGraphicPathname(pFrame,&pNewFile,&iegft);

	if (!bOK || !pNewFile)
		return false;

	// we own storage for pNewFile and must g_free it.
	UT_DEBUGMSG(("fileInsertGraphic: loading [%s]\n",pNewFile));

	FG_ConstGraphicPtr pFG;

	UT_Error errorCode;

	errorCode = IE_ImpGraphic::loadGraphic(pNewFile, iegft, pFG);
	if(errorCode != UT_OK || !pFG)
	  {
		s_CouldNotLoadFileMessage(pFrame, pNewFile, errorCode);
		FREEP(pNewFile);
		return false;
	  }

	ABIWORD_VIEW;

	errorCode = pView->cmdInsertGraphic(pFG);
	if (errorCode != UT_OK)
	{
		s_CouldNotLoadFileMessage(pFrame, pNewFile, errorCode);

		FREEP(pNewFile);
		return false;
	}

	FREEP(pNewFile);

	return true;
}

Defun1(fileInsertPositionedGraphic)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	char* pNewFile = nullptr;


	IEGraphicFileType iegft = IEGFT_Unknown;
	bool bOK = s_AskForGraphicPathname(pFrame,&pNewFile,&iegft);

	if (!bOK || !pNewFile)
		return false;

	// we own storage for pNewFile and must g_free it.
	UT_DEBUGMSG(("fileInsertGraphic: loading [%s]\n",pNewFile));

	FG_ConstGraphicPtr pFG;

	UT_Error errorCode;

	errorCode = IE_ImpGraphic::loadGraphic(pNewFile, iegft, pFG);
	if(errorCode != UT_OK || !pFG)
	  {
		s_CouldNotLoadFileMessage(pFrame, pNewFile, errorCode);
		FREEP(pNewFile);
		return false;
	  }

	ABIWORD_VIEW;
	errorCode = pView->cmdInsertPositionedGraphic(pFG);
	if (errorCode != UT_OK)
	{
		s_CouldNotLoadFileMessage(pFrame, pNewFile, errorCode);

		FREEP(pNewFile);
		return false;
	}

	FREEP(pNewFile);

	return true;
}


Defun1(fileInsertPageBackgroundGraphic)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	char* pNewFile = nullptr;


	IEGraphicFileType iegft = IEGFT_Unknown;
	bool bOK = s_AskForGraphicPathname(pFrame,&pNewFile,&iegft);

	if (!bOK || !pNewFile)
		return false;

	// we own storage for pNewFile and must g_free it.
	UT_DEBUGMSG(("fileInsertBackgroundGraphic: loading [%s]\n",pNewFile));

	FG_ConstGraphicPtr pFG;

	UT_Error errorCode;

	errorCode = IE_ImpGraphic::loadGraphic(pNewFile, iegft, pFG);
	if(errorCode != UT_OK || !pFG)
	  {
		s_CouldNotLoadFileMessage(pFrame, pNewFile, errorCode);
		FREEP(pNewFile);
		return false;
	  }

	ABIWORD_VIEW;
	fl_BlockLayout * pBlock = pView->getCurrentBlock();
	UT_return_val_if_fail( pBlock, false );
	fl_DocSectionLayout * pDSL = pBlock->getDocSectionLayout();
	UT_return_val_if_fail( pDSL, false );
	PT_DocPosition iPos = pDSL->getPosition();
	errorCode = pView->cmdInsertGraphicAtStrux(pFG, iPos, PTX_Section);

	if (errorCode != UT_OK)
	{
		s_CouldNotLoadFileMessage(pFrame, pNewFile, errorCode);

		FREEP(pNewFile);
		return false;
	}

	FREEP(pNewFile);

	return true;
}

Defun(selectObject)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	
	UT_return_val_if_fail (pView, false);
	// check if the run to select is a fp_ImageRun. If, so, don't move the view
	PT_DocPosition pos = pView->getDocPositionFromXY(pCallData->m_xPos, pCallData->m_yPos);
	fl_BlockLayout * pBlock = pView->getBlockAtPosition(pos);
	if(pBlock)
	{
		UT_sint32 x1,x2,y1,y2,iHeight;
		bool bEOL = false;
		bool bDir = false;
		
		fp_Run * pRun = nullptr;
		
		pRun = pBlock->findPointCoords(pos,bEOL,x1,y1,x2,y2,iHeight,bDir);
		while(pRun && ((pRun->getType() != FPRUN_IMAGE) && (pRun->getType() != FPRUN_EMBED)))
		{
			pRun = pRun->getNextRun();
		}
		if(pRun && ((pRun->getType() == FPRUN_IMAGE) || ((pRun->getType() == FPRUN_EMBED))))
		{
			// we've found an image: do not move the view, just select the image and exit
			pView->cmdSelect(pos,pos+1);
			// Set the cursor context to image selected.
			pView->getMouseContext(pCallData->m_xPos, pCallData->m_yPos);
			return true;
		}
		else
		{
			// do nothing...
		}
	}
	
	// no, the run is something else (ie. not a fp_ImageRun)
	pView->warpInsPtToXY(pCallData->m_xPos, pCallData->m_yPos, true);
	pView->extSelHorizontal(true, 1); // move point forward one
	
	return true;
}

Defun(warpInsPtToXY)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->warpInsPtToXY(pCallData->m_xPos, pCallData->m_yPos, true);

	return true;
}

static void sActualMoveLeft(AV_View *  pAV_View, EV_EditMethodCallData * /*pCallData*/)
{
	ABIWORD_VIEW;
	UT_return_if_fail (pView);
	bool bRTL = false;
	fl_BlockLayout * pBL = pView->getCurrentBlock();
	if(pBL)
		bRTL = pBL->getDominantDirection() == UT_BIDI_RTL;
	
	pView->cmdCharMotion(bRTL,1);
}

Defun1(warpInsPtLeft)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
//
// Do this operation in an idle loop so when can reject queued events
//
//
// This code sets things up to handle the warp right in an idle loop.
//
	UT_return_val_if_fail (pView, false);
	int inMode = UT_WorkerFactory::IDLE | UT_WorkerFactory::TIMER;
	UT_WorkerFactory::ConstructMode outMode = UT_WorkerFactory::NONE;
	_Freq * pFreq = new _Freq(pView,nullptr,sActualMoveLeft);
	s_pFrequentRepeat = UT_WorkerFactory::static_constructor (_sFrequentRepeat,pFreq, inMode, outMode);

	UT_ASSERT(s_pFrequentRepeat);
	UT_ASSERT(outMode != UT_WorkerFactory::NONE);

	// If the worker is working on a timer instead of in the idle
	// time, set the frequency of the checks.
	if ( UT_WorkerFactory::TIMER == outMode )
	{
		// this is really a timer, so it's safe to static_cast it
		static_cast<UT_Timer*>(s_pFrequentRepeat)->set(50);
	}
	s_pFrequentRepeat->start();
	return true;
}

static void sActualMoveRight(AV_View *  pAV_View, EV_EditMethodCallData * /*pCallData*/)
{
	ABIWORD_VIEW;
	UT_return_if_fail (pView);
	bool bRTL = false;
	fl_BlockLayout * pBL = pView->getCurrentBlock();
	if(pBL)
		bRTL = pBL->getDominantDirection() == UT_BIDI_RTL;
	
	pView->cmdCharMotion(!bRTL,1);
}

Defun1(warpInsPtRight)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
//
// Do this operation in an idle loop so when can reject queued events
//
//
// This code sets things up to handle the warp right in an idle loop.
//
	UT_return_val_if_fail (pView, false);
	int inMode = UT_WorkerFactory::IDLE | UT_WorkerFactory::TIMER;
	UT_WorkerFactory::ConstructMode outMode = UT_WorkerFactory::NONE;
	_Freq * pFreq = new _Freq(pView,nullptr,sActualMoveRight);
	s_pFrequentRepeat = UT_WorkerFactory::static_constructor (_sFrequentRepeat,pFreq, inMode, outMode);

	UT_ASSERT(s_pFrequentRepeat);
	UT_ASSERT(outMode != UT_WorkerFactory::NONE);

	// If the worker is working on a timer instead of in the idle
	// time, set the frequency of the checks.
	if ( UT_WorkerFactory::TIMER == outMode )
	{
		// this is really a timer, so it's safe to static_cast it
		static_cast<UT_Timer*>(s_pFrequentRepeat)->set(50);
	}
	s_pFrequentRepeat->start();
	return true;
}

Defun1(warpInsPtBOP)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->moveInsPtTo(FV_DOCPOS_BOP);
	return true;
}

Defun1(warpInsPtEOP)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->moveInsPtTo(FV_DOCPOS_EOP);
	return true;
}

Defun1(warpInsPtBOL)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->moveInsPtTo(FV_DOCPOS_BOL);
	return true;
}

Defun1(warpInsPtEOL)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->moveInsPtTo(FV_DOCPOS_EOL);
	return true;
}

Defun1(warpInsPtBOW)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	bool bRTL = false;
	fl_BlockLayout * pBL = pView->getCurrentBlock();
	if(pBL)
		bRTL = pBL->getDominantDirection() == UT_BIDI_RTL;
	

	if(bRTL)
		pView->moveInsPtTo(FV_DOCPOS_EOW_MOVE);
	else
		pView->moveInsPtTo(FV_DOCPOS_BOW);

	return true;
}

Defun1(warpInsPtEOW)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	bool bRTL = false;
	fl_BlockLayout * pBL = pView->getCurrentBlock();
	if(pBL)
		bRTL = pBL->getDominantDirection() == UT_BIDI_RTL;
	

	if(bRTL)
		pView->moveInsPtTo(FV_DOCPOS_BOW);
	else
		pView->moveInsPtTo(FV_DOCPOS_EOW_MOVE);

	return true;
}

Defun0(warpInsPtBOS)
{
	CHECK_FRAME;
	return true;
}

Defun0(warpInsPtEOS)
{
	CHECK_FRAME;
	return true;
}

Defun1(warpInsPtBOB)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->moveInsPtTo(FV_DOCPOS_BOB);
	return true;
}

Defun1(warpInsPtEOB)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->moveInsPtTo(FV_DOCPOS_EOB);
	return true;
}

Defun1(warpInsPtBOD)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->moveInsPtTo(FV_DOCPOS_BOD);
	return true;
}

Defun1(warpInsPtEOD)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
//
// This is called on cntrl-End. If called from within a footnote/endnote
// jump back to just after the insertion point
//
	UT_return_val_if_fail (pView, false);
	if(pView->isInFootnote())
	{
		fl_FootnoteLayout * pFL = pView->getClosestFootnote(pView->getPoint());
		PT_DocPosition pos = pFL->getDocPosition() + pFL->getLength();
		pView->setPoint(pos);
		pView->ensureInsertionPointOnScreen();
		return true;
	}
	if(pView->isInEndnote())
	{
		fl_EndnoteLayout * pEL = pView->getClosestEndnote(pView->getPoint());
		PT_DocPosition pos = pEL->getDocPosition() + pEL->getLength();
		pView->setPoint(pos);
		pView->ensureInsertionPointOnScreen();
		return true;
	}

	pView->moveInsPtTo(FV_DOCPOS_EOD);
	return true;
}

Defun1(warpInsPtPrevPage)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->warpInsPtNextPrevPage(false);
	return true;
}

Defun1(warpInsPtNextPage)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->warpInsPtNextPrevPage(true);
	return true;
}

Defun1(warpInsPtPrevScreen)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->warpInsPtNextPrevScreen(false);
	return true;
}

Defun1(warpInsPtNextScreen)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->warpInsPtNextPrevScreen(true);
	return true;
}

Defun1(warpInsPtPrevLine)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
//
// Finish handling current expose before doing the next movement
//
	UT_return_val_if_fail (pView, false);
	pView->warpInsPtNextPrevLine(false);

	return true;
}

Defun1(warpInsPtNextLine)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
//
// Finish handling current expose before doing the next movement
//
	UT_return_val_if_fail (pView, false);
	pView->warpInsPtNextPrevLine(true);

	return true;
}

/*****************************************************************/

Defun1(cursorDefault)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// clear status bar of any lingering messages
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	pFrame->setStatusMessage(nullptr);

	GR_Graphics * pG = pView->getGraphics();
	if (pG)
	{
		pG->setCursor(GR_Graphics::GR_CURSOR_DEFAULT);
	}
	return true;
}

Defun1(cursorIBeam)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	xxx_UT_DEBUGMSG((" CursorIBeam \n"));
	// clear status bar of any lingering messages
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	pFrame->setStatusMessage(nullptr);

	GR_Graphics * pG = pView->getGraphics();
	if (pG)
	{
		pG->setCursor(GR_Graphics::GR_CURSOR_IBEAM);
	}
	static_cast<AV_View *>(pView)->notifyListeners(AV_CHG_MOUSEPOS);
	return true;
}

Defun1(cursorTOC)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// clear status bar of any lingering messages
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	pFrame->setStatusMessage(nullptr);

	GR_Graphics * pG = pView->getGraphics();
	if (pG)
	{
		pG->setCursor(GR_Graphics::GR_CURSOR_LINK);
	}
	return true;
}

Defun1(cursorRightArrow)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// clear status bar of any lingering messages
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	pFrame->setStatusMessage(nullptr);

	GR_Graphics * pG = pView->getGraphics();
	if (pG)
	{
		pG->setCursor(GR_Graphics::GR_CURSOR_RIGHTARROW);
	}
	return true;
}


Defun1(cursorVline)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// clear status bar of any lingering messages
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	pFrame->setStatusMessage(nullptr);

	GR_Graphics * pG = pView->getGraphics();
	if (pG)
	{
		pG->setCursor(GR_Graphics::GR_CURSOR_VLINE_DRAG);
	}
	return true;
}


Defun1(cursorTopCell)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// clear status bar of any lingering messages
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	pFrame->setStatusMessage(nullptr);

	GR_Graphics * pG = pView->getGraphics();
	if (pG)
	{
		pG->setCursor(GR_Graphics::GR_CURSOR_DOWNARROW);
	}
	return true;
}


Defun1(cursorHline)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// clear status bar of any lingering messages
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	pFrame->setStatusMessage(nullptr);

	GR_Graphics * pG = pView->getGraphics();
	if (pG)
	{
		pG->setCursor(GR_Graphics::GR_CURSOR_HLINE_DRAG);
	}
	return true;
}

Defun1(cursorLeftArrow)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// clear status bar of any lingering messages
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	pFrame->setStatusMessage(nullptr);

	GR_Graphics * pG = pView->getGraphics();
	if (pG)
	{
		pG->setCursor(GR_Graphics::GR_CURSOR_LEFTARROW);
	}
	return true;
}

Defun1(cursorImage)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// clear status bar of any lingering messages
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	pFrame->setStatusMessage(nullptr);

	GR_Graphics * pG = pView->getGraphics();
	if (pG)
	{
		pG->setCursor(GR_Graphics::GR_CURSOR_IMAGE);
	}
	return true;
}

Defun1(cursorImageSize)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// clear status bar of any lingering messages
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	pFrame->setStatusMessage(nullptr);

	GR_Graphics * pG = pView->getGraphics();
	if (pG)
	{
		// set the mouse cursor to the appropriate shape
		pG->setCursor( pView->getImageSelCursor() );
	}
	return true;
}

static bool dlgEditLatexEquation(AV_View *pAV_View, EV_EditMethodCallData * /*pCallData*/, bool bStartDlg, PT_DocPosition pos)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	FL_DocLayout * pLayout = pView->getLayout();
	GR_EmbedManager * pMath = pLayout->getEmbedManager("mathml");
	if(pMath->isDefault())
	{
	  //
	  // No MathML plugin. We can't edit this.
	  //
	  UT_DEBUGMSG(("No Math Plugin! \n"));
	     return false;
	}
	if(pos == 0)
	{
	    pos = pView->getPoint()-1;
	}
	fl_BlockLayout * pBlock = pView->getCurrentBlock();
	fp_Run * pRun = nullptr;
	fp_MathRun * pMathRun = nullptr;
	UT_sint32 x1,y1,x2,y2,height;
	bool bEOL = false;
	bool bDir = false;
	pRun = pBlock->findPointCoords(pos,bEOL,x1,y1,x2,y2,height,bDir);
	while(pRun && pRun->getLength() == 0)
	{
		pRun = pRun->getNextRun();
	}
	if(pRun == nullptr)
	{
		return false;
	}
	if(pRun->getType() != FPRUN_MATH)
    {
		return false;
	}
	pMathRun = static_cast<fp_MathRun *>(pRun);
	const PP_AttrProp * pSpanAP = pMathRun->getSpanAP();
	const gchar * pszLatexID = nullptr, *pszDisplayMode = nullptr;
	pSpanAP->getAttribute("latexid",pszLatexID);
	pSpanAP->getProperty("display",pszDisplayMode);
	if(pszLatexID == nullptr || *pszLatexID == 0)
	{
		return false;
	}
	UT_ConstByteBufPtr pByteBuf;
	UT_UTF8String sLatex;
	PD_Document * pDoc= pView->getDocument();
	bool bFoundLatexID = pDoc->getDataItemDataByName(pszLatexID, 
													 pByteBuf,
													 nullptr, nullptr);

	if(!bFoundLatexID)
	{
		return true;
	}
	UT_UCS4_mbtowc myWC;
	sLatex.appendBuf(pByteBuf, myWC);
	UT_DEBUGMSG(("Loaded Latex %s from PT \n",sLatex.utf8_str()));
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());

	AP_Dialog_Latex * pDialog
		= static_cast<AP_Dialog_Latex *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_LATEX));
	UT_return_val_if_fail(pDialog, false);
	if(pDialog->isRunning())
	{
		pDialog->activate();
		pDialog->setDisplayMode((pszDisplayMode && !strcmp(pszDisplayMode, "inline"))?
		                        ABI_DISPLAY_INLINE: ABI_DISPLAY_BLOCK);
		pDialog->fillLatex(sLatex);
	}
	else if(bStartDlg)
	{
		pDialog->runModeless(pFrame);
		pDialog->setDisplayMode((pszDisplayMode && !strcmp(pszDisplayMode, "inline"))?
		                        ABI_DISPLAY_INLINE: ABI_DISPLAY_BLOCK);
		pDialog->fillLatex(sLatex);
	}
	else
	{
		pDialogFactory->releaseDialog(pDialog);
	}
	return true;

}

/*****************************************************************/

Defun1(contextMenu)
{
	CHECK_FRAME;
// raise context menu over whatever we are over.  this is
	// intended for use by the keyboard accelerator rather than
	// the other "targeted" context{...} methods which are bound
	// to the mouse.

	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	UT_sint32 xPos, yPos;
	EV_EditMouseContext emc = pView->getInsertionPointContext(&xPos,&yPos);

	const char * szContextMenuName = XAP_App::getApp()->getMenuFactory()->FindContextMenu(emc);
	if (!szContextMenuName)
		return false;
	bool res =	pFrame->runModalContextMenu(pView,szContextMenuName,xPos,yPos);
	return res;
}

static bool s_doContextMenu_no_move( EV_EditMouseContext emc,
											UT_sint32 xPos,
											UT_sint32 yPos,
											FV_View * pView,
											XAP_Frame * pFrame)
{
	const char * szContextMenuName =  XAP_App::getApp()->getMenuFactory()->FindContextMenu(emc);
	UT_DEBUGMSG(("Context Menu Name is........ %s \n",szContextMenuName));
	if (!szContextMenuName)
		return false;
	bool res =	pFrame->runModalContextMenu(pView,szContextMenuName,
											xPos,yPos);
	return res;
}

bool static s_doContextMenu(EV_EditMouseContext emc,
							UT_sint32 xPos,
							UT_sint32 yPos,
							FV_View * pView,
							XAP_Frame * pFrame)
{
	// move the IP so actions have the right context
	if (!pView->isXYSelected(xPos, yPos))
		pView->warpInsPtToXY(xPos,yPos,true);

	return s_doContextMenu_no_move(emc,xPos,yPos,pView,pFrame);
}

Defun(contextText)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	//
	// Look if we've right clicked on a mathrun
	//
	PT_DocPosition pos = 0;
	if(pView->isMathLoaded() && pView->isMathSelected(pCallData->m_xPos, pCallData->m_yPos,pos))
	{
	  return s_doContextMenu(EV_EMC_MATH,pCallData->m_xPos, pCallData->m_yPos,pView,pFrame);
	}
	return s_doContextMenu(EV_EMC_TEXT,pCallData->m_xPos, pCallData->m_yPos,pView,pFrame);
}


Defun(contextFrame)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	// no frame context menu in normal view ...
	if(pView->getViewMode() == VIEW_NORMAL)
		return true;
	
	return s_doContextMenu(EV_EMC_FRAME,pCallData->m_xPos, pCallData->m_yPos,pView,pFrame);
}

Defun(contextRevision)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	return s_doContextMenu(EV_EMC_REVISION,pCallData->m_xPos, pCallData->m_yPos,pView,pFrame);
}

Defun(contextTOC)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	return s_doContextMenu_no_move(EV_EMC_TOC,pCallData->m_xPos, pCallData->m_yPos,pView,pFrame);
}


Defun(contextMath)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	bool b = false;
	if(pView->isMathLoaded())
	{
	    b = s_doContextMenu_no_move( EV_EMC_MATH,pCallData->m_xPos, pCallData->m_yPos,pView,pFrame);
	}
	else
	{
	    b = s_doContextMenu_no_move( EV_EMC_TEXT,pCallData->m_xPos, pCallData->m_yPos,pView,pFrame);

	}
	return b;
}

#ifdef ENABLE_SPELL
Defun(contextMisspellText)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	UT_DEBUGMSG(("Doing Misspelt text \n"));
	return s_doContextMenu(EV_EMC_MISSPELLEDTEXT,pCallData->m_xPos, pCallData->m_yPos,pView,pFrame);
}
#endif

Defun(contextImage)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	fp_Run *pRun = nullptr;

	if ( pView->isSelectionEmpty () )
	  {
		// select the object if it isn't already
		UT_DEBUGMSG(("Selecting objec: %d\n", pCallData->m_xPos));
		pView->warpInsPtToXY(pCallData->m_xPos, pCallData->m_yPos, true);
		pView->extSelHorizontal (true, 1);
	  }
	PT_DocPosition pos = pView->getDocPositionFromXY(pCallData->m_xPos, pCallData->m_yPos);
	fl_BlockLayout * pBlock = pView->getBlockAtPosition(pos);
	bool bDoEmbed = false;
	if(pBlock)
	{
		UT_sint32 x1,x2,y1,y2,iHeight;
		bool bEOL = false;
		bool bDir = false;
		
		pRun = pBlock->findPointCoords(pos,bEOL,x1,y1,x2,y2,iHeight,bDir);
		while(pRun && ((pRun->getType() != FPRUN_IMAGE) && (pRun->getType() != FPRUN_EMBED)))
		{
			pRun = pRun->getNextRun();
		}
		if(pRun && ((pRun->getType() == FPRUN_IMAGE) || ((pRun->getType() == FPRUN_EMBED))))
		{
			// Set the cursor context to image selected.
			if(pRun->getType() == FPRUN_EMBED)
			{
			      bDoEmbed = true;
			}
		}
		else
		{
			// do nothing...
		}
	}
	if(!bDoEmbed)
	{
	     return s_doContextMenu(EV_EMC_IMAGE,pCallData->m_xPos, pCallData->m_yPos,pView,pFrame);
	}
	// get the menu from pRun
	fp_EmbedRun *pEmbedRun = dynamic_cast<fp_EmbedRun*>(pRun);
	return s_doContextMenu((pRun)? pEmbedRun->getContextualMenu(): EV_EMC_EMBED, pCallData->m_xPos, pCallData->m_yPos, pView, pFrame);
}


Defun(contextPosObject)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	return s_doContextMenu_no_move(EV_EMC_POSOBJECT,pCallData->m_xPos, pCallData->m_yPos,pView,pFrame);
}


Defun(contextEmbedLayout)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	fp_Run *pRun = nullptr;

	if ( pView->isSelectionEmpty () )
	  {
		// select the image if it isn't already
		UT_DEBUGMSG(("Selecting image: %d\n", pCallData->m_xPos));
		pView->warpInsPtToXY(pCallData->m_xPos, pCallData->m_yPos, true);
		pView->extSelHorizontal (true, 1);
	  }
	PT_DocPosition pos = pView->getDocPositionFromXY(pCallData->m_xPos, pCallData->m_yPos);
	fl_BlockLayout * pBlock = pView->getBlockAtPosition(pos);
	if(pBlock)
	{
		UT_sint32 x1,x2,y1,y2,iHeight;
		bool bEOL = false;
		bool bDir = false;
		
		pRun = pBlock->findPointCoords(pos,bEOL,x1,y1,x2,y2,iHeight,bDir);
		while(pRun && ((pRun->getType() != FPRUN_IMAGE) && (pRun->getType() != FPRUN_EMBED)))
		{
			pRun = pRun->getNextRun();
		}
	}
	// get the menu from pRun
	fp_EmbedRun *pEmbedRun = dynamic_cast<fp_EmbedRun*>(pRun);
	return s_doContextMenu((pRun)? pEmbedRun->getContextualMenu(): EV_EMC_EMBED, pCallData->m_xPos, pCallData->m_yPos, pView, pFrame);
}

Defun(contextHyperlink)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	// move the IP so actions have the right context
	if (!pView->isXYSelected(pCallData->m_xPos, pCallData->m_yPos))
		EX(warpInsPtToXY);
	
	fp_Run * pRun = pView->getHyperLinkRun(pView->getPoint());
	UT_return_val_if_fail(pRun, false);
	fp_HyperlinkRun * pHRun = pRun->getHyperlink();
	
	if(pHRun && pHRun->getHyperlinkType() == HYPERLINK_NORMAL) // normal hyperlinks
	{
#ifdef ENABLE_SPELL
		if(pView->isTextMisspelled())
			return s_doContextMenu_no_move(EV_EMC_HYPERLINKMISSPELLED,pCallData->m_xPos, pCallData->m_yPos,pView,pFrame);
		else
#endif
			return s_doContextMenu_no_move(EV_EMC_HYPERLINKTEXT,pCallData->m_xPos, pCallData->m_yPos,pView,pFrame);
	}

	if(pHRun && pHRun->getHyperlinkType() == HYPERLINK_ANNOTATION) // annotations
	{
#ifdef ENABLE_SPELL
		if(pView->isTextMisspelled())
			return s_doContextMenu_no_move(EV_EMC_ANNOTATIONMISSPELLED,pCallData->m_xPos, pCallData->m_yPos,pView,pFrame);
		else
#endif
			return s_doContextMenu_no_move(EV_EMC_ANNOTATIONTEXT,pCallData->m_xPos, pCallData->m_yPos,pView,pFrame);
	}


	if(pHRun && pHRun->getHyperlinkType() == HYPERLINK_RDFANCHOR)
	{
		UT_DEBUGMSG(("open rdf context menu\n"));
		return s_doContextMenu_no_move(EV_EMC_RDFANCHORTEXT,pCallData->m_xPos, pCallData->m_yPos,pView,pFrame);
	}
	return false; // to avoid compilation warnings (should never be reached)
}

#ifdef ENABLE_SPELL
static bool _spellSuggest(AV_View* pAV_View, UT_uint32 ndx)
{
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->cmdContextSuggest(ndx);
	return true;
}

Defun1(spellSuggest_1)
{
	CHECK_FRAME;
	return _spellSuggest(pAV_View, 1);
}
Defun1(spellSuggest_2)
{
	CHECK_FRAME;
	return _spellSuggest(pAV_View, 2);
}
Defun1(spellSuggest_3)
{
	CHECK_FRAME;
	return _spellSuggest(pAV_View, 3);
}
Defun1(spellSuggest_4)
{
	CHECK_FRAME;
	return _spellSuggest(pAV_View, 4);
}
Defun1(spellSuggest_5)
{
	CHECK_FRAME;
	return _spellSuggest(pAV_View, 5);
}
Defun1(spellSuggest_6)
{
	CHECK_FRAME;
	return _spellSuggest(pAV_View, 6);
}
Defun1(spellSuggest_7)
{
	CHECK_FRAME;
	return _spellSuggest(pAV_View, 7);
}
Defun1(spellSuggest_8)
{
	CHECK_FRAME;
	return _spellSuggest(pAV_View, 8);
}
Defun1(spellSuggest_9)
{
	CHECK_FRAME;
	return _spellSuggest(pAV_View, 9);
}

Defun1(spellIgnoreAll)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail (pView, false);
	pView->cmdContextIgnoreAll();
	return true;
}

Defun1(spellAdd)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail (pView, false);
	pView->cmdContextAdd();
	return true;
}
#endif

/*****************************************************************/


static void sActualDragToXY(AV_View *  pAV_View, EV_EditMethodCallData * pCallData)
{
	ABIWORD_VIEW;
	UT_return_if_fail (pView);
	AP_Frame *pFrame = static_cast<AP_Frame *>(pAV_View->getParentData());
	if(pFrame->getDoWordSelections())
	{
		pView->extSelToXYword(pCallData->m_xPos, pCallData->m_yPos, true);
		return;
	}
	pView->extSelToXY(pCallData->m_xPos, pCallData->m_yPos, true);
	return;
}

Defun(dragToXY)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
//
// Do this operation in an idle loop so when can reject queued events
//
//
// This code sets things up to handle the warp right in an idle loop.
//
	UT_return_val_if_fail (pView, false);
	int inMode = UT_WorkerFactory::IDLE | UT_WorkerFactory::TIMER;
	UT_WorkerFactory::ConstructMode outMode = UT_WorkerFactory::NONE;
	EV_EditMethodCallData * pNewData = new  EV_EditMethodCallData(pCallData->m_pData,pCallData->m_dataLength);
	pNewData->m_xPos = pCallData->m_xPos;
	pNewData->m_yPos = pCallData->m_yPos;
	_Freq * pFreq = new _Freq(pView,pNewData,sActualDragToXY);
	s_pFrequentRepeat = UT_WorkerFactory::static_constructor (_sFrequentRepeat,pFreq, inMode, outMode);

	UT_ASSERT(s_pFrequentRepeat);
	UT_ASSERT(outMode != UT_WorkerFactory::NONE);

	// If the worker is working on a timer instead of in the idle
	// time, set the frequency of the checks.
	if ( UT_WorkerFactory::TIMER == outMode )
	{
		// this is really a timer, so it's safe to static_cast it
		static_cast<UT_Timer*>(s_pFrequentRepeat)->set(50);
	}
	s_pFrequentRepeat->start();
	return true;
}

Defun(dragToXYword)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->extSelToXYword(pCallData->m_xPos, pCallData->m_yPos, true);
	return true;
}

Defun(endDrag)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->endDrag(pCallData->m_xPos, pCallData->m_yPos);
	return true;
}

Defun(extSelToXY)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->extSelToXY(pCallData->m_xPos, pCallData->m_yPos, false);
	return true;
}

Defun1(extSelLeft)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	bool bRTL = false;
	fl_BlockLayout * pBL = pView->getCurrentBlock();
	if(pBL)
		bRTL = pBL->getDominantDirection() == UT_BIDI_RTL;
	

	pView->extSelHorizontal(bRTL,1);

	return true;
}

Defun1(extSelRight)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	bool bRTL = false;
	fl_BlockLayout * pBL = pView->getCurrentBlock();
	if(pBL)
		bRTL = pBL->getDominantDirection() == UT_BIDI_RTL;
	

	pView->extSelHorizontal(!bRTL,1);

	return true;
}

Defun1(extSelBOL)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->extSelTo(FV_DOCPOS_BOL);
	return true;
}

Defun1(extSelEOL)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->extSelTo(FV_DOCPOS_EOL);
	return true;
}

Defun1(extSelBOW)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	bool bRTL = false;
	fl_BlockLayout * pBL = pView->getCurrentBlock();
	if(pBL)
		bRTL = pBL->getDominantDirection() == UT_BIDI_RTL;
	

	if(bRTL)
		pView->extSelTo(FV_DOCPOS_EOW_MOVE);
	else
		pView->extSelTo(FV_DOCPOS_BOW);

	return true;
}

Defun1(extSelEOW)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	bool bRTL = false;
	fl_BlockLayout * pBL = pView->getCurrentBlock();
	if(pBL)
		bRTL = pBL->getDominantDirection() == UT_BIDI_RTL;
	

	if(bRTL)
		pView->extSelTo(FV_DOCPOS_BOW);
	else
		pView->extSelTo(FV_DOCPOS_EOW_MOVE);

	return true;
}

Defun0(extSelBOS)
{
	CHECK_FRAME;
	return true;
}

Defun0(extSelEOS)
{
	CHECK_FRAME;
	return true;
}

Defun1(extSelBOB)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->extSelTo(FV_DOCPOS_BOB);
	return true;
}

Defun1(extSelEOB)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->extSelTo(FV_DOCPOS_EOB);
	return true;
}

Defun1(extSelBOD)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->extSelTo(FV_DOCPOS_BOD);
	return true;
}

Defun1(extSelEOD)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->extSelTo(FV_DOCPOS_EOD);
	return true;
}

Defun1(extSelPrevLine)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->extSelNextPrevLine(false);
	return true;
}

Defun1(extSelNextLine)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->extSelNextPrevLine(true);
	return true;
}

Defun1(extSelPageDown)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->extSelNextPrevPage(true);
	return true;
}

Defun1(extSelPageUp)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->extSelNextPrevPage(false);
	return true;
}

Defun1(extSelScreenDown)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->extSelNextPrevScreen(true);
	return true;
}

Defun1(extSelScreenUp)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->extSelNextPrevScreen(false);
	return true;
}

Defun(selectAll)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->cmdSelect(pCallData->m_xPos, pCallData->m_yPos, FV_DOCPOS_BOD, FV_DOCPOS_EOD);
	return true;
}

Defun(selectWord)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->cmdSelect(pCallData->m_xPos, pCallData->m_yPos, FV_DOCPOS_BOW, FV_DOCPOS_EOW_SELECT);
	return true;
}

Defun(selectLine)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	if(pView->getMouseContext(pCallData->m_xPos, pCallData->m_yPos) == EV_EMC_LEFTOFTEXT)
	{
		AP_Frame *pFrame = static_cast<AP_Frame *>(pAV_View->getParentData());
		if(pFrame->getDoWordSelections())
		{
			pView->cmdSelect(pCallData->m_xPos, pCallData->m_yPos, FV_DOCPOS_BOB, FV_DOCPOS_EOB);
			return true;
		}
	}
	pView->cmdSelect(pCallData->m_xPos, pCallData->m_yPos, FV_DOCPOS_BOL, FV_DOCPOS_EOL);
	return true;
}

Defun(selectBlock)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->cmdSelect(pCallData->m_xPos, pCallData->m_yPos, FV_DOCPOS_BOB, FV_DOCPOS_EOB);
	return true;
}

Defun1(selectTable)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	PT_DocPosition posStartTab,posEndTab;
	const pf_Frag_Strux *tableSDH, *endTableSDH;
	PD_Document * pDoc = pView->getDocument();
	bool bRes = pDoc->getStruxOfTypeFromPosition(pView->getPoint(),PTX_SectionTable,&tableSDH);
	if(!bRes)
	{
		UT_DEBUGMSG(("No Table Strux found!! \n"));
		return false;
	}
	posStartTab = pDoc->getStruxPosition(tableSDH); //was -1
	UT_DEBUGMSG(("PosStart %d TableSDH %p \n", posStartTab, (void*)tableSDH));
	bRes = pDoc->getNextStruxOfType(tableSDH,PTX_EndTable,&endTableSDH);
	if(!bRes)
	{
		UT_DEBUGMSG(("No End Table Strux found!! \n"));
		return false;
	}
	posEndTab = pDoc->getStruxPosition(endTableSDH)+1; //was +1
	UT_DEBUGMSG(("PosEndTab %d endTableSDH %p \n", posEndTab, (void*)endTableSDH));
	pView->cmdSelect(posStartTab,posEndTab);
	return true;
}


Defun(selectTOC)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Select TOC \n"));
	UT_return_val_if_fail (pView, false);
	pView->cmdSelectTOC(pCallData->m_xPos, pCallData->m_yPos);
	return true;
}


static PT_DocPosition s_mathObjectAt(FV_View * pView, PT_DocPosition pos,
                                     fp_MathRun ** ppRun);

Defun(editLatexAtPos)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Edit Math at Pos\n"));
	UT_return_val_if_fail (pView, false);
	/* prefer the math object under the last click (context menu);
	 * ribbon/menu invocations fall back to the equation at the caret */
	PT_DocPosition pos = s_mathObjectAt(pView,
		pView->getDocPositionFromLastXY(), nullptr);
	if (!pos)
		pos = s_mathObjectAt(pView, pView->getPoint(), nullptr);
	return dlgEditLatexEquation(pAV_View, pCallData, true, pos);
}


Defun(editLatexEquation)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Select and Edit Math \n"));
	UT_return_val_if_fail (pView, false);
        PT_DocPosition posL = pView->getDocPositionFromXY(pCallData->m_xPos, pCallData->m_yPos);
	PT_DocPosition posH = posL+1;
	pView->cmdSelect(posL,posH);
        return dlgEditLatexEquation(pAV_View, pCallData, true,0);
}


Defun1(editEmbed)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Select and Edit an Embedded Object \n"));
	UT_return_val_if_fail (pView, false);
        PT_DocPosition posL = pView->getPoint();
	PT_DocPosition posH = pView->getSelectionAnchor();
	PT_DocPosition posTemp = 0;
	if(posH < posL)
	{
	     posTemp = posL;
	     posL = posH;
	     posH = posTemp;
	}
	if(posL == posH)
	{
	     posH = posL+1;
	     pView->cmdSelect(posL,posH);
	}
	fl_BlockLayout * pBlock = pView->getBlockAtPosition(posL);
	if(pBlock)
	{
		UT_sint32 x1,x2,y1,y2,iHeight;
		bool bEOL = false;
		bool bDir = false;
		
		fp_Run * pRun = nullptr;
		
		pRun = pBlock->findPointCoords(posL,bEOL,x1,y1,x2,y2,iHeight,bDir);
		while(pRun && ((pRun->getType() != FPRUN_IMAGE) && (pRun->getType() != FPRUN_EMBED)))
		{
			pRun = pRun->getNextRun();
		}
		if(pRun && ((pRun->getType() == FPRUN_IMAGE) || ((pRun->getType() == FPRUN_EMBED))))
		{
			// Set the cursor context to image selected.
		  UT_DEBUGMSG(("Found and embedded Object \n"));
			if(pRun->getType() == FPRUN_EMBED)
			{
			      fp_EmbedRun * pEmbedRun = static_cast<fp_EmbedRun *>(pRun);
			      UT_DEBUGMSG(("About to edit the object \n"));
			      GR_EmbedManager * pEmbed = pEmbedRun->getEmbedManager();
			      UT_sint32 uid = pEmbedRun->getUID();
			      pEmbed->modify(uid);
			}
		}
	}

	return true;
}

Defun(selectMath)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Select Math \n"));
	UT_return_val_if_fail (pView, false);
        PT_DocPosition posL = pView->getDocPositionFromXY(pCallData->m_xPos, pCallData->m_yPos);
	PT_DocPosition posH = posL+1;
	pView->cmdSelect(posL,posH);
	dlgEditLatexEquation(pAV_View, pCallData, false,0);
	return true;
}

/*! find the fp_MathRun whose object sits at (or immediately before)
 * pos; falls back to the last math run in the paragraph.
 * Returns the object's document position or 0. */
static PT_DocPosition s_mathObjectAt(FV_View * pView, PT_DocPosition pos,
                                     fp_MathRun ** ppRun)
{
	UT_return_val_if_fail(pView, 0);
	if (pos < 3)
		return 0;
	fl_BlockLayout * pBlock = pView->getBlockAtPosition(pos);
	if (!pBlock)
		return 0;
	PT_DocPosition blockPos = pBlock->getPosition();
	fp_MathRun * pMathRun = nullptr;
	PT_DocPosition objPos = 0;
	for (fp_Run * pRun = pBlock->getFirstRun(); pRun;
	     pRun = pRun->getNextRun())
	{
		if (pRun->getType() != FPRUN_MATH)
			continue;
		fp_MathRun * mr = static_cast<fp_MathRun *>(pRun);
		PT_DocPosition rp = blockPos + pRun->getBlockOffset();
		if (rp == pos - 1 || rp == pos) {
			pMathRun = mr;
			objPos = rp;
			break;
		}
		pMathRun = mr;
		objPos = rp;
	}
	if (!pMathRun)
		return 0;
	if (ppRun) *ppRun = pMathRun;
	return objPos;
}

/*! convert LaTeX source to MathML through the built-in manager */
static bool s_latexToMathML(FV_View * pView, const UT_UTF8String & sLatex,
                            bool compact, UT_UTF8String & sMathML)
{
	GR_EmbedManager * pEmbed = pView->getLayout()->getEmbedManager("mathml");
	UT_return_val_if_fail(pEmbed && !pEmbed->isDefault(), false);
	UT_ByteBufPtr From(new UT_ByteBuf);
	UT_ByteBufPtr To(new UT_ByteBuf);
	From->ins(0, reinterpret_cast<const UT_Byte *>(sLatex.utf8_str()),
	          static_cast<UT_uint32>(sLatex.size()));
	if (!pEmbed->convert(compact ? 1 : 0, From, To))
		return false;
	UT_UCS4_mbtowc myWC;
	sMathML.appendBuf(To, myWC);
	return sMathML.size() > 0;
}

/*! read the LaTeX source of the math object owned by pMathRun */
static bool s_mathLatexOf(FV_View * pView, fp_MathRun * pMathRun,
                          UT_UTF8String & sLatex, bool & bInline)
{
	const PP_AttrProp * pSpanAP = pMathRun->getSpanAP();
	const gchar * pszLatexID = nullptr, *pszDisplayMode = nullptr;
	pSpanAP->getAttribute("latexid", pszLatexID);
	pSpanAP->getProperty("display", pszDisplayMode);
	bInline = pszDisplayMode && !strcmp(pszDisplayMode, "inline");
	if (!pszLatexID || !*pszLatexID)
		return false;
	UT_ConstByteBufPtr pByteBuf;
	if (!pView->getDocument()->getDataItemDataByName(pszLatexID, pByteBuf,
	                                               nullptr, nullptr))
		return false;
	UT_UCS4_mbtowc myWC;
	sLatex.appendBuf(pByteBuf, myWC);
	return sLatex.size() > 0;
}

/*! insertEquation — ribbon gallery presets.
 *  callData: "display:<latex>" or "inline:<latex>" (inline is default) */
Defun(insertEquation)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_return_val_if_fail(pCallData && pCallData->m_pData, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	std::string d(s.utf8_str());
	bool compact = true;
	if (d.compare(0, 8, "display:") == 0) {
		compact = false;
		d = d.substr(8);
	} else if (d.compare(0, 7, "inline:") == 0) {
		d = d.substr(7);
	}
	UT_UTF8String sLatex(d.c_str()), sMathML;
	if (!s_latexToMathML(pView, sLatex, compact, sMathML))
		return false;
	return pView->cmdInsertLatexMath(sLatex, sMathML, compact);
}

/*! equationInsertSymbol — contextual-tab symbol/structure buttons.
 *  If the caret is on an equation, the snippet is appended to its
 *  LaTeX source and the object is re-created; otherwise a new inline
 *  equation holding the snippet is inserted. */
Defun(equationInsertSymbol)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_return_val_if_fail(pCallData && pCallData->m_pData, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	UT_UTF8String sSnippet(s.utf8_str());

	fp_MathRun * pMathRun = nullptr;
	PT_DocPosition posObj = s_mathObjectAt(pView, pView->getPoint(),
	                                       &pMathRun);
	if (pMathRun)
	{
		UT_UTF8String sLatex;
		bool bInline = false;
		if (s_mathLatexOf(pView, pMathRun, sLatex, bInline))
		{
			UT_UTF8String sNew = sLatex;
			sNew += " ";
			sNew += sSnippet;
			UT_UTF8String sMathML;
			if (s_latexToMathML(pView, sNew, bInline, sMathML))
			{
				pView->cmdSelect(posObj, posObj + 1);
				return pView->cmdInsertLatexMath(sNew, sMathML, bInline);
			}
		}
	}
	UT_UTF8String sMathML;
	if (!s_latexToMathML(pView, sSnippet, true, sMathML))
		return false;
	return pView->cmdInsertLatexMath(sSnippet, sMathML, true);
}

/*! insertLatexEquation — open the LaTeX dialog for a new equation.
 *  Unlike editLatexAtPos this does not require an existing math run. */
Defun1(insertLatexEquation)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	FL_DocLayout * pLayout = pView->getLayout();
	GR_EmbedManager * pMath = pLayout->getEmbedManager("mathml");
	UT_return_val_if_fail(pMath && !pMath->isDefault(), false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	pFrame->raise();
	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());
	AP_Dialog_Latex * pDialog
		= static_cast<AP_Dialog_Latex *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_LATEX));
	UT_return_val_if_fail(pDialog, false);
	if (pDialog->isRunning())
		pDialog->activate();
	else
		pDialog->runModeless(pFrame);
	return true;
}

/*! toggleEquationDisplay — switch the equation at the caret between
 *  inline and block display. */
Defun1(toggleEquationDisplay)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	fp_MathRun * pMathRun = nullptr;
	PT_DocPosition posObj = s_mathObjectAt(pView, pView->getPoint(),
	                                       &pMathRun);
	UT_return_val_if_fail(pMathRun, false);
	UT_UTF8String sLatex;
	bool bInline = false;
	if (!s_mathLatexOf(pView, pMathRun, sLatex, bInline))
		return false;
	UT_UTF8String sMathML;
	if (!s_latexToMathML(pView, sLatex, !bInline, sMathML))
		return false;
	pView->cmdSelect(posObj, posObj + 1);
	return pView->cmdInsertLatexMath(sLatex, sMathML, !bInline);
}


Defun1(selectRow)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	PT_DocPosition posStartRow,posEndRow;
	const pf_Frag_Strux *rowSDH, *endRowSDH, *tableSDH;
	UT_sint32 iLeft,iRight,iTop,iBot;

	PD_Document * pDoc = pView->getDocument();
	pView->getCellParams(pView->getPoint(), &iLeft, &iRight,&iTop,&iBot);
	
	bool bRes = pDoc->getStruxOfTypeFromPosition(pView->getPoint(),PTX_SectionTable,&tableSDH);
	if(!bRes)
	{
		return false;
	}
  
	//
	// Now find the number of rows and columns inthis table.
    //
	UT_sint32 numRows,numCols;
	bRes = pDoc->getRowsColsFromTableStrux(tableSDH, pView->isShowRevisions(), pView->getRevisionLevel(), &numRows, &numCols);
	if(!bRes)
	{
		return false;
	}
	rowSDH = pDoc->getCellStruxFromRowCol(tableSDH, pView->isShowRevisions(), pView->getRevisionLevel(), iTop, 0);
	posStartRow = pDoc->getStruxPosition(rowSDH) - 1;
	endRowSDH = pDoc->getCellStruxFromRowCol(tableSDH, pView->isShowRevisions(), pView->getRevisionLevel(), iTop, numCols -1);
	posEndRow = pDoc->getStruxPosition(endRowSDH);
	bRes = pDoc->getNextStruxOfType(endRowSDH,PTX_EndCell,&endRowSDH);
	if(!bRes)
	{
		return false;
	}
	posEndRow = pDoc->getStruxPosition(endRowSDH)+1;
	pView->cmdSelect(posStartRow,posEndRow);
	pView->setSelectionMode(FV_SelectionMode_TableRow);
	return true;
}


Defun1(selectCell)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	PT_DocPosition posStartCell,posEndCell;
	const pf_Frag_Strux *cellSDH, *endCellSDH;
	PD_Document * pDoc = pView->getDocument();
	bool bRes = pDoc->getStruxOfTypeFromPosition(pView->getPoint(),PTX_SectionCell,&cellSDH);
	if(!bRes)
	{
		return false;
	}
	posStartCell = pDoc->getStruxPosition(cellSDH) - 1;
	bRes = pDoc->getNextStruxOfType(cellSDH,PTX_EndCell,&endCellSDH);
	if(!bRes)
	{
		return false;
	}
	posEndCell = pDoc->getStruxPosition(endCellSDH)+1;
	pView->cmdSelect(posStartCell,posEndCell);
	return true;
}


Defun1(selectColumn)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	if(!pView->isInTable())
	{
		return false;
	}
	pView->cmdSelectColumn(pView->getPoint());
	return true;
}


Defun(selectColumnClick)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	UT_sint32 y = pCallData->m_yPos;
	UT_sint32 x = pCallData->m_xPos;
	PT_DocPosition pos = pView->getDocPositionFromXY(x,y);
	if(!pView->isInTable(pos))
	{
		return false;
	}
	pView->cmdSelectColumn(pos);
	return true;
}


static void sActualDelLeft(AV_View *  pAV_View, EV_EditMethodCallData * /*pCallData*/)
{
	ABIWORD_VIEW;
	UT_return_if_fail (pView);
	pView->cmdCharDelete(false,1);
}

Defun1(delLeft)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
//
// Do this operation in an idle loop so when can reject queued events
//
//
// This code sets things up to handle the warp right in an idle loop.
//
	UT_return_val_if_fail (pView, false);
	int inMode = UT_WorkerFactory::IDLE | UT_WorkerFactory::TIMER;
	UT_WorkerFactory::ConstructMode outMode = UT_WorkerFactory::NONE;
	_Freq * pFreq = new _Freq(pView,nullptr,sActualDelLeft);
	s_pFrequentRepeat = UT_WorkerFactory::static_constructor (_sFrequentRepeat,pFreq, inMode, outMode);

	UT_ASSERT(s_pFrequentRepeat);
	UT_ASSERT(outMode != UT_WorkerFactory::NONE);

	// If the worker is working on a timer instead of in the idle
	// time, set the frequency of the checks.
	if ( UT_WorkerFactory::TIMER == outMode )
	{
		// this is really a timer, so it's safe to static_cast it
		static_cast<UT_Timer*>(s_pFrequentRepeat)->set(50);
	}
	s_pFrequentRepeat->start();
	return true;
}


static void sActualDelRight(AV_View *  pAV_View, EV_EditMethodCallData * /*pCallData*/)
{
	ABIWORD_VIEW;
	UT_return_if_fail (pView);
	pView->cmdCharDelete(true,1);
}

Defun1(delRight)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
//
// Do this operation in an idle loop so when can reject queued events
//
//
// This code sets things up to handle the warp right in an idle loop.
//
	UT_return_val_if_fail (pView, false);
	int inMode = UT_WorkerFactory::IDLE | UT_WorkerFactory::TIMER;
	UT_WorkerFactory::ConstructMode outMode = UT_WorkerFactory::NONE;
	_Freq * pFreq = new _Freq(pView,nullptr,sActualDelRight);
	s_pFrequentRepeat = UT_WorkerFactory::static_constructor (_sFrequentRepeat,pFreq, inMode, outMode);

	UT_ASSERT(s_pFrequentRepeat);
	UT_ASSERT(outMode != UT_WorkerFactory::NONE);

	// If the worker is working on a timer instead of in the idle
	// time, set the frequency of the checks.
	if ( UT_WorkerFactory::TIMER == outMode )
	{
		// this is really a timer, so it's safe to static_cast it
		static_cast<UT_Timer*>(s_pFrequentRepeat)->set(50);
	}
	s_pFrequentRepeat->start();
	return true;
}

Defun1(delBOL)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->delTo(FV_DOCPOS_BOL);
	return true;
}

Defun1(delEOL)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->delTo(FV_DOCPOS_EOL);
	return true;
}

Defun1(delBOW)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->delTo(FV_DOCPOS_BOW);
	return true;
}

Defun1(delEOW)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->delTo(FV_DOCPOS_EOW_MOVE);
	return true;
}

Defun0(delBOS)
{
	CHECK_FRAME;
	return true;
}

Defun0(delEOS)
{
	CHECK_FRAME;
	return true;
}

Defun1(delBOB)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->delTo(FV_DOCPOS_BOB);
	return true;
}

Defun1(delEOB)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->delTo(FV_DOCPOS_EOB);
	return true;
}

Defun1(delBOD)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->delTo(FV_DOCPOS_BOD);
	return true;
}

Defun1(delEOD)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->delTo(FV_DOCPOS_EOD);
	return true;
}

#if 0
static bool pView->cmdCharInsert(const UT_UCS4Char * pText, UT_uint32 iLen,
						XAP_Frame * pFrame, FV_View * pView,
						bool bForce = false)
{
	// handle automatic language formatting
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);

	bool b = false;

	pPrefs->getPrefsValueBool(static_cast<gchar*>(XAP_PREF_KEY_ChangeLanguageWithKeyboard),
							  &b);
	if(b)
	{
		const UT_LangRecord * pLR = pApp->getKbdLanguage();

		if (pLR)
		{
			const gchar * props_out[] = {"lang", nullptr, nullptr};
			props_out[1] = pLR->m_szLangCode;
			pView->setCharFormat(props_out);
		}
	}
	
	pView->cmdCharInsert(pText, iLen, bForce);
	return true;
}
#endif

#if 0 // disabled because of conditionnal below
static void sActualInsertData(AV_View *  pAV_View, EV_EditMethodCallData * pCallData)
{
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->cmdCharInsert(pCallData->m_pData, pCallData->m_dataLength);
	return;
}
#endif
Defun(insertData)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail (pView, false);
	pView->cmdCharInsert(pCallData->m_pData, pCallData->m_dataLength);
#if 0
//
// Do this operation in an idle loop so when can reject queued events
//
//
// This code sets things up to handle the warp right in an idle loop.
//
	int inMode = UT_WorkerFactory::IDLE | UT_WorkerFactory::TIMER;
	UT_WorkerFactory::ConstructMode outMode = UT_WorkerFactory::NONE;
	GR_Graphics * pG = pView->getGraphics();
	EV_EditMethodCallData * pNewData = new  EV_EditMethodCallData(pCallData->m_pData,pCallData->m_dataLength);
	_Freq * pFreq = new _Freq(pView,pNewData,sActualInsertData);
	s_pFrequentRepeat = UT_WorkerFactory::static_constructor (_sFrequentRepeat,pFreq, inMode, outMode, pG);

	UT_ASSERT(s_pFrequentRepeat);
	UT_ASSERT(outMode != UT_WorkerFactory::NONE);

	// If the worker is working on a timer instead of in the idle
	// time, set the frequency of the checks.
	if ( UT_WorkerFactory::TIMER == outMode )
	{
		// this is really a timer, so it's safe to static_cast it
		static_cast<UT_Timer*>(s_pFrequentRepeat)->set(50);
	}
	s_pFrequentRepeat->start();
#endif
	return true;
}


Defun(insertClosingParenthesis)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	ABIWORD_VIEW;

	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);

	bool bLang = false, bMarker = false;

	pPrefs->getPrefsValueBool(XAP_PREF_KEY_ChangeLanguageWithKeyboard, bLang);

	const UT_LangRecord * pLR = nullptr;
	
	if(bLang)
	{
		pLR = pApp->getKbdLanguage();
		
		pPrefs->getPrefsValueBool(XAP_PREF_KEY_DirMarkerAfterClosingParenthesis, bMarker);
	}

	if(bMarker && pLR)
	{
		UT_return_val_if_fail(pCallData->m_dataLength == 1, false);
		UT_UCS4Char data[2];
		data[0] = (UT_UCS4Char) *(pCallData->m_pData);
		
		if(pLR->m_eDir == UTLANG_RTL)
		{
			data[1] = UCS_RLM;
		}
		else if(pLR->m_eDir == UTLANG_LTR)
		{
			data[1] = UCS_LRM;
		}
		else
		{
			goto normal_insert;
		}

		pView->cmdCharInsert(&data[0],2);
		return true;
	}

 normal_insert:	
	pView->cmdCharInsert(pCallData->m_pData, pCallData->m_dataLength);
	return true;
}

Defun(insertOpeningParenthesis)
{
	CHECK_FRAME;
	UT_return_val_if_fail (pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	ABIWORD_VIEW;

	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);

	bool bLang = false, bMarker = false;

	pPrefs->getPrefsValueBool(XAP_PREF_KEY_ChangeLanguageWithKeyboard, bLang);

	const UT_LangRecord * pLR = nullptr;

	if(bLang)
	{
		pLR = pApp->getKbdLanguage();

		pPrefs->getPrefsValueBool(XAP_PREF_KEY_DirMarkerAfterClosingParenthesis, bMarker);
	}

	if(bMarker && pLR)
	{
		UT_return_val_if_fail(pCallData->m_dataLength == 1, false);
		UT_UCS4Char data[2];
		data[1] = (UT_UCS4Char) *(pCallData->m_pData);

		if(pLR->m_eDir == UTLANG_RTL)
		{
			data[0] = UCS_RLM;
		}
		else if(pLR->m_eDir == UTLANG_LTR)
		{
			data[0] = UCS_LRM;
		}
		else
		{
			goto normal_insert;
		}

		pView->cmdCharInsert(&data[0], 2);
		return true;
	}

 normal_insert:	
	pView->cmdCharInsert(pCallData->m_pData, pCallData->m_dataLength);
	return true;
}

Defun1(insertLRM)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	
	UT_return_val_if_fail (pView, false);
	UT_UCS4Char cM = UCS_LRM;
	pView->cmdCharInsert(&cM, 1);
	return true;
}

Defun1(insertRLM)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail (pView, false);
	UT_UCS4Char cM = UCS_RLM;
	pView->cmdCharInsert(&cM, 1);
	return true;
}

/*****************************************************************/
// TODO the bInsert parameter is currently not used; it would require
// changes to the dialogues so that if bInsert == false the dialogue
// would be labeled "Delete bookmark"

static bool s_doBookmarkDlg(FV_View * pView, bool /*bInsert*/)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_InsertBookmark * pDialog
		= static_cast<AP_Dialog_InsertBookmark *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_INSERTBOOKMARK));
UT_return_val_if_fail(pDialog, false);
	if (!pView->isSelectionEmpty())
	{
		UT_UCS4Char * buffer;
		pView->getSelectionText(buffer);
		pDialog->setSuggestedBM(buffer);
		FREEP(buffer);
	}

	pDialog->setDoc(pView);
	pDialog->runModal(pFrame);

	AP_Dialog_InsertBookmark::tAnswer ans = pDialog->getAnswer();

	if (ans == AP_Dialog_InsertBookmark::a_OK)
	{
			pView->cmdInsertBookmark(pDialog->getBookmark());
	}
	else if(ans == AP_Dialog_InsertBookmark::a_DELETE)
	{
			pView->cmdDeleteBookmark(pDialog->getBookmark());
	}
	pDialogFactory->releaseDialog(pDialog);

	return (ans == AP_Dialog_InsertBookmark::a_DELETE || ans == AP_Dialog_InsertBookmark::a_OK);
}

Defun1(insertBookmark)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	s_doBookmarkDlg(pView, true);
	return true;
}

Defun1(deleteBookmark)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	s_doBookmarkDlg(pView, false);
	return true;
}


static bool s_xmlidDlg(FV_View * pView, bool /*bInsert*/)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_InsertXMLID * pDialog
		= static_cast<AP_Dialog_InsertXMLID *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_INSERTXMLID));
	UT_return_val_if_fail(pDialog, false);

	pDialog->setDoc(pView);
	pDialog->runModal(pFrame);

	AP_Dialog_GetStringCommon::tAnswer ans = pDialog->getAnswer();

	if (ans == AP_Dialog_GetStringCommon::a_OK)
	{
			pView->cmdInsertXMLID(pDialog->getString());
	}
	else if(ans == AP_Dialog_GetStringCommon::a_DELETE)
	{
			pView->cmdDeleteXMLID(pDialog->getString());
	}
	pDialogFactory->releaseDialog(pDialog);

	return (ans == AP_Dialog_GetStringCommon::a_DELETE || ans == AP_Dialog_GetStringCommon::a_OK);
}

Defun1(insertXMLID)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("insertXMLID()\n"));
	s_xmlidDlg(pView, true);
	return true;
}

Defun1(deleteXMLID)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	s_xmlidDlg(pView, false);
	return true;
}

/*****************************************************************/
static bool s_doHyperlinkDlg(FV_View * pView)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_InsertHyperlink * pDialog
		= static_cast<AP_Dialog_InsertHyperlink *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_INSERTHYPERLINK));
	UT_return_val_if_fail(pDialog, false);
	std::string sTarget;
	std::string sTitle;
	bool bEdit = false;
	PT_DocPosition pos1 = 0;
	PT_DocPosition pos2 = 0;
	PT_DocPosition posOrig = pView->getPoint();
	pDialog->setDoc(pView);
	if(pView->isSelectionEmpty())
	{
		const char *buf;
		fp_HyperlinkRun * pHRun = static_cast<fp_HyperlinkRun *>(pView->getHyperLinkRun(pView->getPoint()));
		if(pHRun == nullptr)
		{
			/* no selection and not on a link: Word still opens the
			 * dialog - the typed text becomes the new link */
			pDialog->runModal(pFrame);
			AP_Dialog_InsertHyperlink::tAnswer ans =
				pDialog->getAnswer();
			bool bOK = (ans == AP_Dialog_InsertHyperlink::a_OK);
			if (bOK)
			{
				const char * szLink  = pDialog->getHyperlink()
					? pDialog->getHyperlink() : "";
				const char * szTitle = pDialog->getHyperlinkTitle()
					? pDialog->getHyperlinkTitle() : "";
				const char * szText  = pDialog->getDisplayText()
					? pDialog->getDisplayText() : "";
				if (!*szText)
					szText = szLink;
				if (*szText && *szLink)
				{
					UT_UCS4String s(szText);
					pView->cmdCharInsert(s.ucs4_str(), s.length());
					PT_DocPosition end = pView->getPoint();
					pView->cmdSelect(end - s.length(), end);
					pView->cmdInsertHyperlink(szLink, szTitle);
					pView->cmdUnselectSelection();
					pView->setPoint(end);
				}
			}
			pDialogFactory->releaseDialog(pDialog);
			return bOK;
		}
		bEdit = true;
		buf = pHRun->getTarget();
		if (buf != nullptr)
			sTarget = buf;
		buf = pHRun->getTitle();
		if (buf != nullptr)
			sTitle = buf;
		fl_BlockLayout * pBL = pHRun->getBlock();
		fp_Run * pRun = nullptr;
		if(pHRun->isStartOfHyperlink())
		{
			pos1 = pBL->getPosition(true) + pHRun->getBlockOffset()+1;
			pRun = pHRun->getNextRun();
			pos2 = pBL->getPosition(true) + pHRun->getBlockOffset() + 1;
			while(pRun && (pRun->getType() != FPRUN_HYPERLINK))
			{
			        pos2 += pRun->getLength();
				pRun = pRun->getNextRun();
			}
		}
		else
		{
			pos2 = pBL->getPosition(true) + pHRun->getBlockOffset();
			pRun = pHRun->getPrevRun();
			pos1 = pBL->getPosition(true) + pHRun->getBlockOffset();
			while(pRun && pRun->getHyperlink())
			{
				pos1 = pBL->getPosition(true) + pRun->getBlockOffset();
				pRun = pRun->getPrevRun();
			}
		}
//
// Select our range
//
//		pView->cmdSelect(pos1,pos2);
//
// Set the target
//
		pDialog->setHyperlink(sTarget.c_str());
		pDialog->setHyperlinkTitle(sTitle.c_str());
	}
	pDialog->runModal(pFrame);

	AP_Dialog_InsertHyperlink::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == AP_Dialog_InsertHyperlink::a_OK);

	if (bOK)
	{
		// the dialog getters can return NULL (e.g. fields left empty)
		const char * szLink  = pDialog->getHyperlink()      ? pDialog->getHyperlink()      : "";
		const char * szTitle = pDialog->getHyperlinkTitle() ? pDialog->getHyperlinkTitle() : "";
		if(bEdit)
		{
//
// Delete the old one.
//
			pView->cmdDeleteHyperlink();
			if(!pView->isSelectionEmpty())
			{
				pView->cmdUnselectSelection();
			}
//
// Select our range
//
			pView->cmdSelect(pos1,pos2);
			pView->cmdInsertHyperlink(szLink, szTitle);
			pView->cmdUnselectSelection();
			pView->setPoint(posOrig);
		}
		else
		{
			pView->cmdInsertHyperlink(szLink, szTitle);
		}
	}
	else
	{
		if(bEdit)
		{
			pView->cmdUnselectSelection();
			pView->setPoint(posOrig);
		}
	}
	pDialogFactory->releaseDialog(pDialog);

	return bOK;
}

Defun1(insertHyperlink)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	/* Word's Link button always opens the dialog; with no
	 * selection the "Text to display" field creates a new link */
	s_doHyperlinkDlg(pView);
	return true;
}

Defun(replaceChar)
{
	CHECK_FRAME;
//ABIWORD_VIEW;
	return ( EX(delRight) && EX(insertData) && EX(setEditVI) );
}

Defun0(insertSoftBreak)
{
	CHECK_FRAME;
	return true;
}

Defun1(insertParagraphBreak)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->insertParagraphBreak();
	return true;
}

Defun1(insertSectionBreak)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView, false);
//
// No section breaks in header/Footers
//
	if(pView->isHdrFtrEdit())
		return true;
	if(pView->isInTable())
	{
		XAP_Frame * pFrame = static_cast<XAP_Frame *> (pAV_View->getParentData());
		pFrame->showMessageBox(AP_STRING_ID_MSG_NoBreakInsideTable,
							   XAP_Dialog_MessageBox::b_O,
							   XAP_Dialog_MessageBox::a_OK);
		return true;
	}
	if(pView->isInFrame(pView->getPoint()))
	{
		XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
		UT_return_val_if_fail(pFrame, false);
		pFrame->showMessageBox(AP_STRING_ID_MSG_NoBreakInsideFrame,
							   XAP_Dialog_MessageBox::b_O,
							   XAP_Dialog_MessageBox::a_OK);
		return true;
	}

	pView->insertSectionBreak();
	return true;
}

/*
  Note that within the piece table, we use the following
  representations:
	char code					meaning
	UCS_TAB  (tab)				tab
	UCS_LF	 (line feed)		forced line break
	UCS_VTAB (vertical tab) 	forced column break
	UCS_FF	 (form feed)		forced page break
*/

Defun1(insertTab)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_UCS4Char c = UCS_TAB;
	if(!pView->isInTable())
	{
		pView->cmdCharInsert(&c,1);
	}
	else
	{
		pView->cmdAdvanceNextPrevCell(true);
	}
	return true;
}


Defun1(insertTabCTL)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_UCS4Char c = UCS_TAB;
	pView->cmdCharInsert(&c,1);
	return true;
}


Defun1(insertTabShift)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	if(!pView->isInTable())
	{
		return true;
	}
	else
	{
		pView->cmdAdvanceNextPrevCell(false);
	}
	return true;
}


Defun1(insertLineBreak)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_UCS4Char c = UCS_LF;
	pView->cmdCharInsert(&c,1);
	return true;
}

Defun1(insertColumnBreak)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
//
// No column breaks in header/Footers
//
	if(pView->isHdrFtrEdit())
		return true;
	if(pView->isInTable())
	{
		XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
		UT_return_val_if_fail(pFrame, false);
		pFrame->showMessageBox(AP_STRING_ID_MSG_NoBreakInsideTable,
							   XAP_Dialog_MessageBox::b_O,
							   XAP_Dialog_MessageBox::a_OK);
		return true;
	}
	if(pView->isInFrame(pView->getPoint()))
	{
		XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
		UT_return_val_if_fail(pFrame, false);
		pFrame->showMessageBox(AP_STRING_ID_MSG_NoBreakInsideFrame,
							   XAP_Dialog_MessageBox::b_O,
							   XAP_Dialog_MessageBox::a_OK);
		return true;
	}

	UT_UCS4Char c = UCS_VTAB;
	pView->cmdCharInsert(&c,1);
	return true;
}

Defun1(insertColsBefore)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PT_DocPosition insPoint;
	PT_DocPosition insAnchor;
	if(pView->isSelectionEmpty())
	{
		insPoint = pView->getPoint();
	}
	else
	{
		insPoint = pView->getPoint();
		insAnchor = pView->getSelectionAnchor();
		if(insAnchor < insPoint)
		{
			insPoint = insAnchor;
		}
	}

	pView->cmdInsertCol(insPoint,true); // is before
	return true;
}


Defun1(insertColsAfter)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PT_DocPosition insPoint;
	PT_DocPosition insAnchor;
	if(pView->isSelectionEmpty())
	{
		insPoint = pView->getPoint();
	}
	else
	{
		insPoint = pView->getPoint();
		insAnchor = pView->getSelectionAnchor();
		if(insAnchor < insPoint)
		{
			insPoint = insAnchor;
		}
	}

	pView->cmdInsertCol(insPoint,false); // is After
	return true;
}


Defun1(insertRowsBefore)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PT_DocPosition insPoint;
	PT_DocPosition insAnchor;
	if(pView->isSelectionEmpty())
	{
		insPoint = pView->getPoint();
	}
	else
	{
		insPoint = pView->getPoint();
		insAnchor = pView->getSelectionAnchor();
		if(insAnchor < insPoint)
		{
			insPoint = insAnchor;
		}
	}
	pView->cmdInsertRow(insPoint,true); // is before
	return true;
}


Defun1(insertRowsAfter)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PT_DocPosition insPoint;
	PT_DocPosition insAnchor;
	if(pView->isSelectionEmpty())
	{
		insPoint = pView->getPoint();
	}
	else
	{
		insPoint = pView->getPoint();
		insAnchor = pView->getSelectionAnchor();
		if(insAnchor > insPoint)
		{
			insPoint = insAnchor;
		}
	}
	pView->cmdInsertRow(insPoint,false); // is After
	return true;
}

/*********************************************************************************/

static bool s_doMergeCellsDlg(FV_View * pView)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());

	AP_Dialog_MergeCells * pDialog
		= static_cast<AP_Dialog_MergeCells *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_MERGE_CELLS));
UT_return_val_if_fail(pDialog, false);
	if(pDialog->isRunning() == true)
	{
		pDialog->activate();
	}
	else
	{
		pDialog->runModeless(pFrame);
	}
	return true;
}


Defun1(mergeCells)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	s_doMergeCellsDlg(pView);
	return true;
}


static bool s_doSplitCellsDlg(FV_View * pView)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());

	AP_Dialog_SplitCells * pDialog
		= static_cast<AP_Dialog_SplitCells *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_SPLIT_CELLS));
UT_return_val_if_fail(pDialog, false);
	if(pDialog->isRunning() == true)
	{
		pDialog->activate();
	}
	else
	{
		pDialog->runModeless(pFrame);
	}
	return true;
}


Defun1(splitCells)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	s_doSplitCellsDlg(pView);
	return true;
}

/***********************************************************************************/

static bool s_doFormatTableDlg(FV_View * pView)
{
	UT_return_val_if_fail(pView, false);
	if(!pView->isInTable(pView->getPoint()))
	{
		pView->swapSelectionOrientation();
		UT_ASSERT(pView->isInTable(pView->getPoint()));
	}

	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();
	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());

	//	Maleesh 6/8/2010 - TEMP

	AP_Dialog_FormatTable * pDialog
		= static_cast<AP_Dialog_FormatTable *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_FORMAT_TABLE));
	UT_return_val_if_fail(pDialog, false);
	if(pDialog->isRunning() == true)
	{
		pDialog->activate();
	}
	else
	{
		pDialog->runModeless(pFrame);
	}
	return true;
}


Defun1(formatTable)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	s_doFormatTableDlg(pView);
	return true;
}


Defun1(formatTOC)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());

	AP_Dialog_FormatTOC * pDialog
		= static_cast<AP_Dialog_FormatTOC *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_FORMAT_TOC));
UT_return_val_if_fail(pDialog, false);
	if(pDialog->isRunning() == true)
	{
		pDialog->activate();
	}
	else
	{
		pDialog->runModeless(pFrame);
	}
	return true;
}

/*
 * References ribbon: insert a TOC using one of the gallery presets
 * ("classic", "contemporary", "formal", "modern", "simple") or a
 * static manual placeholder table ("manual" or "manual-<preset>" to
 * style it like that preset).
 */
Defun(tocInsert)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_return_val_if_fail(pCallData && pCallData->m_pData, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	UT_UTF8String sPreset(s.utf8_str());
	if(0 == strcmp(sPreset.utf8_str(), "manual"))
	{
		return (UT_OK == pView->cmdInsertTOCManual());
	}
	if(0 == strncmp(sPreset.utf8_str(), "manual-", 7))
	{
		return (UT_OK == pView->cmdInsertTOCManual(sPreset.utf8_str() + 7));
	}
	return (UT_OK == pView->cmdInsertTOCStyled(sPreset.utf8_str()));
}

Defun1(tocUpdate)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->cmdUpdateTOC();
}

Defun1(tocRemove)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->cmdRemoveTOC();
}

/*
 * References ribbon "Add Text": mark the paragraph(s) under the
 * selection for a given TOC level. callData is "0" (do not show) or
 * "1"-"4" for the TOC level.
 */
Defun(tocAddText)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_return_val_if_fail(pCallData && pCallData->m_pData, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	UT_sint32 iLevel = atoi(s.utf8_str());
	pView->setTocLevel(iLevel);
	return true;
}

Defun1(footnoteNext)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->nextNote(true, true);
}

Defun1(footnotePrev)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->nextNote(true, false);
}

Defun1(endnoteNext)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->nextNote(false, true);
}

Defun1(endnotePrev)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->nextNote(false, false);
}

Defun1(showNotes)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->cmdShowNotes();
	return true;
}

Defun1(footnoteToEndnote)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->convertNotes(true);
}

Defun1(endnoteToFootnote)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->convertNotes(false);
}

Defun1(noteSwap)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->swapNotes();
}

/*
 * Insert tab "Cover Page" gallery. callData is the preset id
 * ("austin", "banded", "facet", "filigree", "integral", "whisp");
 * no data inserts the default preset.
 */
Defun(coverPageInsert)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	const char * szPreset = "austin";
	if(pCallData && pCallData->m_pData && pCallData->m_dataLength)
	{
		UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
		return (UT_OK == pView->cmdInsertCoverPage(s.utf8_str()));
	}
	return (UT_OK == pView->cmdInsertCoverPage(szPreset));
}

Defun1(coverPageRemove)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->cmdRemoveCoverPage();
}

/* Word's Insert > Blank Page: break to a fresh page and leave a
 * completely blank page between, cursor on it */
Defun1(insertBlankPage)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	if(pView->isInTable() || pView->isInFrame(pView->getPoint()) ||
	   pView->isHdrFtrEdit())
	{
		return true;
	}
	UT_UCS4Char c = UCS_FF;
	pView->getDocument()->beginUserAtomicGlob();
	pView->cmdCharInsert(&c, 1);
	pView->cmdCharInsert(&c, 1);
	pView->cmdCharMotion(false, 1);
	pView->getDocument()->endUserAtomicGlob();
	return true;
}

/*
 * Bundled artwork galleries for the Insert tab.  Three data sets
 * ship in <AbiSuiteLibDir>/artwork:
 *   shapes/<category>/*.svg - the LibreOffice preset-shape set
 *                           (Yaru icon theme, MPL-2.0/LGPL-3+)
 *   icons/*.svg             - Lucide icons (ISC)
 *   3d/*.png                - Fluent UI 3D emoji (MIT)
 * szFile is the path relative to the artwork dir.  The artwork is
 * loaded, lightly recoloured toward the app's accent palette and
 * inserted at the point as an SVG (shapes/icons) or PNG (3d)
 * graphic.
 */
static void s_artworkMissing(XAP_Frame * pFrame)
{
	pFrame->showMessageBox(
		"The bundled artwork gallery could not be found. It is "
		"installed under the Abinova data directory as \"artwork\".",
		XAP_Dialog_MessageBox::b_O, XAP_Dialog_MessageBox::a_OK);
}

static void s_replace_all(std::string & s, const char * from,
						  const char * to)
{
	std::string::size_type pos = 0, flen = strlen(from);
	while ((pos = s.find(from, pos)) != std::string::npos)
	{
		s.replace(pos, flen, to);
		pos += strlen(to);
	}
}

static bool s_insertArtworkFile(FV_View * pView, XAP_Frame * pFrame,
								const char * szFile)
{
	UT_return_val_if_fail(pView && pFrame && szFile, false);

	std::string sub("artwork");
	std::string name(szFile);
	std::string::size_type slash = name.rfind('/');
	if (slash != std::string::npos)
	{
		sub += '/' + name.substr(0, slash);
		name = name.substr(slash + 1);
	}

	std::string path;
	if (!XAP_App::getApp()->findAbiSuiteLibFile(path, name.c_str(),
												sub.c_str()))
	{
		s_artworkMissing(pFrame);
		return false;
	}

	gchar * contents = nullptr;
	gsize len = 0;
	if (!g_file_get_contents(path.c_str(), &contents, &len, nullptr))
	{
		s_CouldNotLoadFileMessage(pFrame, path.c_str(),
								  UT_IE_COULDNOTOPEN);
		return false;
	}

	const bool bPng = g_str_has_suffix(path.c_str(), ".png");
	UT_ByteBufPtr pBB(new UT_ByteBuf);
	if (bPng)
	{
		pBB->append(reinterpret_cast<const UT_Byte *>(contents),
					static_cast<UT_uint32>(len));
	}
	else
	{
		std::string svg(contents, len);
		if (strncmp(szFile, "icons/", 6) == 0)
		{
			/* Lucide strokes follow the current text colour; pin a
			 * neutral dark so the icon stays visible in any style */
			s_replace_all(svg, "stroke=\"currentColor\"",
						  "stroke=\"#232629\"");
		}
		else
		{
			/* Yaru shapes are dark-outlined silhouettes over a white
			 * face; recolor the face accent-blue and keep the
			 * outline a darker shade, Word-style */
			s_replace_all(svg, "fill:#fff", "fill:#4472C4");
			s_replace_all(svg, "stroke:#000", "stroke:#2F5597");
			s_replace_all(svg, "fill:#000", "fill:#2F5597");
			s_replace_all(svg, "fill=\"#232629\"", "fill=\"#2F5597\"");
		}
		pBB->append(reinterpret_cast<const UT_Byte *>(svg.data()),
					static_cast<UT_uint32>(svg.size()));
	}
	g_free(contents);

	FG_ConstGraphicPtr pFG;
	UT_Error errorCode = IE_ImpGraphic::loadGraphic(
		pBB, bPng ? IEGFT_PNG : IEGFT_SVG, pFG);
	if (errorCode != UT_OK || !pFG)
	{
		s_CouldNotLoadFileMessage(pFrame, path.c_str(), errorCode);
		return false;
	}
	errorCode = pView->cmdInsertGraphic(pFG);
	if (errorCode != UT_OK)
	{
		s_CouldNotLoadFileMessage(pFrame, path.c_str(), errorCode);
		return false;
	}
	return true;
}

/* callData: "<category>/<icon name>" under artwork/shapes */
Defun(insertShape)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	if (!pCallData || !pCallData->m_pData || !pCallData->m_dataLength)
		return false;
	XAP_Frame * pFrame =
		static_cast<XAP_Frame *>(pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	std::string f("shapes/");
	f += s.utf8_str();
	f += ".svg";
	return s_insertArtworkFile(pView, pFrame, f.c_str());
}

/* callData: the Lucide icon name under artwork/icons */
Defun(insertIcon)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	if (!pCallData || !pCallData->m_pData || !pCallData->m_dataLength)
		return false;
	XAP_Frame * pFrame =
		static_cast<XAP_Frame *>(pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	std::string f("icons/");
	f += s.utf8_str();
	f += ".svg";
	return s_insertArtworkFile(pView, pFrame, f.c_str());
}

/* callData: the asset name under artwork/3d */
Defun(insert3DModel)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	if (!pCallData || !pCallData->m_pData || !pCallData->m_dataLength)
		return false;
	XAP_Frame * pFrame =
		static_cast<XAP_Frame *>(pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	std::string f("3d/");
	f += s.utf8_str();
	f += ".png";
	return s_insertArtworkFile(pView, pFrame, f.c_str());
}

/* Word's WordArt gallery: callData is a preset key
 * ("fill-<hex>" or "outline-<hex>") applied to a styled sample
 * text run; the user edits the text afterwards */
Defun(insertWordArt)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	if (pView->isInTable() || pView->isInFrame(pView->getPoint()) ||
		pView->isHdrFtrEdit())
		return true;

	/* callData is a "key=value;key=value" style spec:
	 *   font=<family>  size=<pt>  weight=bold  italic=1
	 *   color=RRGGBB                    flat fill colour
	 *   outline=RRGGBB[:widthpt]        glyph outline
	 *   gradient=RRGGBB-RRGGBB[:h]      fill gradient (v default)
	 *   shadow=RRGGBB[:dx,dy]           drop shadow (pt)
	 *   reflect=1                       baseline reflection
	 * Legacy "fill-RRGGBB" / "outline-RRGGBB" specs still work. */
	PP_PropertyVector atts = {
		"font-family", "Georgia",
		"font-size", "36pt",
		"font-weight", "bold",
		"font-style", "normal",
		"color", "4472C4"
	};
	if (pCallData && pCallData->m_pData && pCallData->m_dataLength)
	{
		UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
		std::string d(s.utf8_str());
		if (d.compare(0, 8, "outline-") == 0)
		{
			atts[7] = "italic";
			atts[9] = d.c_str() + 8;
		}
		else if (d.compare(0, 5, "fill-") == 0)
		{
			atts[9] = d.c_str() + 5;
		}
		else
		{
			std::string sOutline, sGradient, sShadow;
			bool bReflect = false;
			size_t p = 0;
			while (p < d.size())
			{
				size_t e = d.find(';', p);
				std::string kv = d.substr(
					p, e == std::string::npos ? e : e - p);
				size_t eq = kv.find('=');
				if (eq != std::string::npos)
				{
					std::string k = kv.substr(0, eq);
					std::string v = kv.substr(eq + 1);
					if (k == "font")
						atts[1] = v;
					else if (k == "size")
						atts[3] = v;
					else if (k == "weight")
						atts[5] = v;
					else if (k == "italic" && v == "1")
						atts[7] = "italic";
					else if (k == "color")
						atts[9] = v;
					else if (k == "outline")
						sOutline = v;
					else if (k == "gradient")
						sGradient = v;
					else if (k == "shadow")
						sShadow = v;
					else if (k == "reflect" && v == "1")
						bReflect = true;
				}
				if (e == std::string::npos)
					break;
				p = e + 1;
			}
			/* apply the preset exactly: effects the spec does not
			 * mention are cleared so a re-styled WordArt does not
			 * keep stale effects from the surrounding format */
			atts.push_back("text-outline");
			atts.push_back(sOutline);
			atts.push_back("text-gradient");
			atts.push_back(sGradient);
			atts.push_back("text-shadow");
			atts.push_back(sShadow);
			atts.push_back("text-reflection");
			atts.push_back(bReflect ? "1" : "");
		}
	}
	pView->getDocument()->beginUserAtomicGlob();
	pView->setCharFormat(atts);
	UT_UCS4String s("Your text here");
	pView->cmdCharInsert(s.ucs4_str(), s.length());
	pView->getDocument()->endUserAtomicGlob();
	return true;
}

/* Insert tab Header/Footer galleries: callData is the built-in
 * preset id ("austin", "iondark", ...) applied to the header or
 * footer shadow */
Defun(insertHeaderPreset)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_return_val_if_fail(pCallData && pCallData->m_pData &&
						  pCallData->m_dataLength, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	return (UT_OK == pView->cmdInsertHeaderPreset(s.utf8_str(),
												FL_HDRFTR_HEADER));
}

Defun(insertFooterPreset)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_return_val_if_fail(pCallData && pCallData->m_pData &&
						  pCallData->m_dataLength, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	return (UT_OK == pView->cmdInsertHeaderPreset(s.utf8_str(),
												FL_HDRFTR_FOOTER));
}

/* Page Number popover: "header:left|center|right" or
 * "footer:left|center|right" places the number without a dialog */
Defun(pageNumber)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_return_val_if_fail(pCallData && pCallData->m_pData &&
						  pCallData->m_dataLength, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	std::string spec = s.utf8_str();
	std::string::size_type colon = spec.find(':');
	if (colon == std::string::npos)
		return false;
	const bool bFooter = spec.compare(0, colon, "footer") == 0;
	if (!bFooter && spec.compare(0, colon, "header") != 0)
		return false;
	std::string align = spec.substr(colon + 1);
	if (align != "left" && align != "center" && align != "right")
		return false;
	PP_PropertyVector atts = {
		"text-align", align
	};
	return pView->processPageNumber(
		bFooter ? FL_HDRFTR_FOOTER : FL_HDRFTR_HEADER, atts);
}

Defun1(pageNumberRemove)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->removePageNumbers();
}

/* Drop Cap popover: "none" removes it, "dropped[:N]" and
 * "margin[:N]" apply it with N lines to drop (default 3) */
Defun(dropCap)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	std::string spec = "dropped";
	if (pCallData && pCallData->m_pData && pCallData->m_dataLength)
	{
		UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
		spec = s.utf8_str();
	}
	if (spec == "none")
		return pView->removeDropCap();
	bool bMargin = false;
	if (spec.compare(0, 6, "margin") == 0)
		bMargin = true;
	else if (spec.compare(0, 7, "dropped") != 0)
		return false;
	UT_sint32 iLines = 3;
	std::string::size_type colon = spec.find(':');
	if (colon != std::string::npos)
	{
		iLines = atoi(spec.c_str() + colon + 1);
		if (iLines < 1 || iLines > 10)
			return false;
	}
	return pView->insertDropCap(iLines, "", 0.0, bMargin);
}

/* Word's Signature Line: a sign-here rule with Name/Title
 * placeholders under it */
Defun1(insertSignatureLine)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	if (pView->isInTable() || pView->isInFrame(pView->getPoint()) ||
		pView->isHdrFtrEdit())
		return true;

	pView->getDocument()->beginUserAtomicGlob();
	static const PP_PropertyVector s_block[] = {
		{ "margin-top", "0.75in", "margin-left", "1.5in",
		  "margin-right", "1.5in", "bot-style", "solid",
		  "bot-color", "000000", "bot-thickness", "0.5pt",
		  "text-align", "center" },
		{ "margin-left", "1.5in", "margin-right", "1.5in",
		  "text-align", "center" },
		{ "margin-left", "1.5in", "margin-right", "1.5in",
		  "text-align", "center", "margin-bottom", "0.3in" }
	};
	static const char * s_text[] = { " ", "Name", "Title" };
	for (int i = 0; i < 3; i++)
	{
		pView->setBlockFormat(s_block[i]);
		UT_UCS4String s(s_text[i]);
		pView->cmdCharInsert(s.ucs4_str(), s.length());
		if (i < 2)
			pView->insertParagraphBreak();
	}
	pView->getDocument()->endUserAtomicGlob();
	return true;
}

/* Insert tab "Icons": toggles the docked icon gallery */
Defun1(iconsPane)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame =
		static_cast<XAP_Frame *>(pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_FrameImpl * pImpl = pFrame->getFrameImpl();
	UT_return_val_if_fail(pImpl, false);
	pImpl->toggleIconsPane();
	return true;
}

/* ribbon placeholder for features the engine cannot provide yet */
Defun(notImplemented)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame =
		static_cast<XAP_Frame *>(pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	const char * szWhat = "This feature";
	if (pCallData && pCallData->m_pData && pCallData->m_dataLength)
	{
		UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
		s_TellNotImplemented(pFrame, s.utf8_str(), __LINE__);
		return true;
	}
	s_TellNotImplemented(pFrame, szWhat, __LINE__);
	return true;
}

/*
 * References ribbon "Insert Caption". callData is
 * "<label>|<position>", e.g. "Figure|below"; position may also be
 * "above".
 */
Defun(refCaption)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_return_val_if_fail(pCallData && pCallData->m_pData, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	const std::string sData(s.utf8_str());
	const size_t iBar = sData.find('|');
	const std::string sLabel = sData.substr(0, iBar);
	const bool bAbove = (iBar != std::string::npos &&
						 sData.substr(iBar + 1) == "above");
	return (UT_OK == pView->cmdInsertCaption(sLabel.c_str(), bAbove));
}

/*
 * References ribbon "Insert Table of Figures". callData is the
 * caption label ("Figure", "Table", "Equation" or a custom label).
 */
Defun(refInsertTOF)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_return_val_if_fail(pCallData && pCallData->m_pData, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	return (UT_OK == pView->cmdInsertTableOfFigures(s.utf8_str()));
}

/*
 * References ribbon "Cross-reference". callData is
 * "<bookmark>|page" or "<bookmark>|text".
 */
Defun(refXRef)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_return_val_if_fail(pCallData && pCallData->m_pData, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	const std::string sData(s.utf8_str());
	const size_t iBar = sData.find('|');
	const std::string sBookmark = sData.substr(0, iBar);
	const bool bPage = (iBar != std::string::npos &&
						sData.substr(iBar + 1) == "page");
	return (UT_OK == pView->cmdInsertCrossReference(sBookmark.c_str(), bPage));
}

/*
 * References ribbon "Mark Entry". callData is the entry text used
 * when nothing is selected; empty means "mark the selection".
 */
Defun(refMarkEntry)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	const char * szEntry = "";
	if(pCallData && pCallData->m_pData)
	{
		static UT_UTF8String sEntry;
		UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
		sEntry = s.utf8_str();
		szEntry = sEntry.utf8_str();
	}
	return (UT_OK == pView->cmdMarkIndexEntry(szEntry));
}

/*
 * References ribbon "Mark Citation". callData is
 * "<category>|<citation>"; the citation text is used when nothing is
 * selected.
 */
Defun(refMarkCitation)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_return_val_if_fail(pCallData && pCallData->m_pData, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	const std::string sData(s.utf8_str());
	const size_t iBar = sData.find('|');
	const std::string sCat = sData.substr(0, iBar);
	const std::string sCit = (iBar != std::string::npos)
		? sData.substr(iBar + 1) : "";
	return (UT_OK == pView->cmdMarkCitation(sCat.c_str(), sCit.c_str()));
}

Defun1(refInsertIndex)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return (UT_OK == pView->cmdInsertIndex());
}

Defun1(refRemoveIndex)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->cmdRemoveRefSection("_genidx");
}

Defun1(refInsertTOA)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return (UT_OK == pView->cmdInsertTOA());
}

Defun1(refRemoveTOA)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->cmdRemoveRefSection("_gentoa");
}

/*
 * References ribbon "Insert Citation". callData is the pipe-separated
 * source record "author|year|title|publisher|type".
 */
Defun(refInsertCitation)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_return_val_if_fail(pCallData && pCallData->m_pData, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	return (UT_OK == pView->cmdInsertCitation(s.utf8_str()));
}

/*
 * References ribbon "Bibliography". callData is the citation style:
 * "apa", "mla", "chicago" or "ieee".
 */
Defun(refInsertBibliography)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_return_val_if_fail(pCallData && pCallData->m_pData, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	return (UT_OK == pView->cmdInsertBibliography(s.utf8_str()));
}

Defun1(refRemoveBibliography)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->cmdRemoveRefSection("_genbib");
}

/*
 * Manage Sources popover delete action; callData is the source number.
 */
Defun(refDeleteSource)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_return_val_if_fail(pCallData && pCallData->m_pData, false);
	UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
	pView->cmdDeleteBibSource(atoi(s.utf8_str()));
	return true;
}

/***********************************************************************************/

Defun1(deleteCell)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView, false);
	pView->cmdDeleteCell(pView->getPoint());
	return true;
}


Defun1(deleteColumns)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView, false);
	pView->cmdDeleteCol(pView->getPoint());
	return true;
}


Defun1(deleteRows)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PT_DocPosition pos = pView->getPoint();
	if(pos > pView->getSelectionAnchor())
	{
		pos = pView->getSelectionAnchor();
	}
	pView->cmdDeleteRow(pos);
	return true;
}

Defun1(deleteTable)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PT_DocPosition pos = pView->getPoint();
	if(!pView->isInTable(pos))
	{
	  if(pos > pView->getSelectionAnchor())
	  {
	    pos--;
	  }
	  else
	  {
	    pos++;
	  }
	}
	pView->cmdDeleteTable(pos);
	return true;
}

Defun1(insertPageBreak)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView, false);

	UT_UCS4Char c = UCS_FF;
//
// No page breaks in header/Footers
//
	if(pView->isHdrFtrEdit())
		return true;
	if(pView->isInTable())
	{
		XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
		UT_return_val_if_fail(pFrame, false);
		pFrame->showMessageBox(AP_STRING_ID_MSG_NoBreakInsideTable,
							   XAP_Dialog_MessageBox::b_O,
							   XAP_Dialog_MessageBox::a_OK);
		return true;
	}
	if(pView->isInFrame(pView->getPoint()))
	{
		XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
		UT_return_val_if_fail(pFrame, false);
		pFrame->showMessageBox(AP_STRING_ID_MSG_NoBreakInsideFrame,
							   XAP_Dialog_MessageBox::b_O,
							   XAP_Dialog_MessageBox::a_OK);
		return true;
	}
	pView->cmdCharInsert(&c,1);
	return true;
}

Defun1(insertSpace)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_UCS4Char c = UCS_SPACE;
	pView->cmdCharInsert(&c,1);
	return true;
}

Defun1(insertNBSpace)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_UCS4Char c = UCS_NBSP;			// decimal 160 is NBS
	pView->cmdCharInsert(&c,1);
	return true;
}

// non-breaking hyphen U+2011 (Word: Ctrl+Shift+-)
Defun1(insertNBHyphen)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_UCS4Char c = 0x2011;
	pView->cmdCharInsert(&c,1);
	return true;
}

// optional (soft) hyphen U+00AD (Word: Ctrl+-)
Defun1(insertSoftHyphen)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_UCS4Char c = 0x00AD;
	pView->cmdCharInsert(&c,1);
	return true;
}

// em dash U+2014 (Word: Ctrl+Alt+Num-)
Defun1(insertEmDash)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_UCS4Char c = 0x2014;
	pView->cmdCharInsert(&c,1);
	return true;
}

// en dash U+2013 (Word: Ctrl+Num-)
Defun1(insertEnDash)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_UCS4Char c = 0x2013;
	pView->cmdCharInsert(&c,1);
	return true;
}

// copyright sign U+00A9 (Word: Ctrl+Alt+C)
Defun1(insertCopyright)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_UCS4Char c = 0x00A9;
	pView->cmdCharInsert(&c,1);
	return true;
}

// registered sign U+00AE (Word: Ctrl+Alt+R)
Defun1(insertRegistered)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_UCS4Char c = 0x00AE;
	pView->cmdCharInsert(&c,1);
	return true;
}

// trademark sign U+2122 (Word: Ctrl+Alt+T)
Defun1(insertTrademark)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_UCS4Char c = 0x2122;
	pView->cmdCharInsert(&c,1);
	return true;
}

// non-breaking, zerrow width
Defun1(insertNBZWSpace)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_UCS4Char c = 0xFEFF;
	pView->cmdCharInsert(&c,1);
	return true;
}

// zero width joiner
Defun1(insertZWJoiner)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_UCS4Char c = 0x200D;
	pView->cmdCharInsert(&c,1);
	return true;
}

/*****************************************************************/

Defun(insertGraveData)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// This function provides an interlude.  All of the keys
	// on the Dead_Grave map are mapped here.  The desired
	// character is in the argument (just like in insertData()).
	// We do the character mapping here.
	//
	// This keeps us from having to define 10 EditMethod
	// functions (one for each grave character).
	//
	// It would be nice if the key-binding mechanism (in
	// ap_LoadBindings_*.cpp) were extended to allow a constant
	// to be specified along with the function binding, so that
	// we could have bound 'A' on the DeadGrave map to
	// "insertData(0x00c0)", for example.

	UT_return_val_if_fail (pCallData->m_dataLength==1, false);
	UT_UCS4Char graveChar = 0x0000;
	switch (pCallData->m_pData[0])
	{
	case 0x41:		graveChar=0x00c0;	break;	// Agrave
	case 0x45:		graveChar=0x00c8;	break;	// Egrave
	case 0x49:		graveChar=0x00cc;	break;	// Igrave
	case 0x4f:		graveChar=0x00d2;	break;	// Ograve
	case 0x55:		graveChar=0x00d9;	break;	// Ugrave

	case 0x61:		graveChar=0x00e0;	break;	// agrave
	case 0x65:		graveChar=0x00e8;	break;	// egrave
	case 0x69:		graveChar=0x00ec;	break;	// igrave
	case 0x6f:		graveChar=0x00f2;	break;	// ograve
	case 0x75:		graveChar=0x00f9;	break;	// ugrave
	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		return false;
	}

	pView->cmdCharInsert(&graveChar, 1);
	return true;
}

Defun(insertAcuteData)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// This function provides an interlude.  All of the keys
	// on the Dead_Acute map are mapped here.  The desired
	// character is in the argument (just like in insertData()).
	// We do the character mapping here.
	//
	// See note in Defun(insertGraveData)

	UT_return_val_if_fail (pCallData->m_dataLength==1, false);
	UT_UCS4Char acuteChar = 0x0000;
	switch (pCallData->m_pData[0])
	{
	case 0x41:		acuteChar=0x00c1;	break;	// Aacute
	case 0x45:		acuteChar=0x00c9;	break;	// Eacute
	case 0x49:		acuteChar=0x00cd;	break;	// Iacute
	case 0x4f:		acuteChar=0x00d3;	break;	// Oacute
	case 0x55:		acuteChar=0x00da;	break;	// Uacute
	case 0x59:		acuteChar=0x00dd;	break;	// Yacute

	case 0x61:		acuteChar=0x00e1;	break;	// aacute
	case 0x65:		acuteChar=0x00e9;	break;	// eacute
	case 0x69:		acuteChar=0x00ed;	break;	// iacute
	case 0x6f:		acuteChar=0x00f3;	break;	// oacute
	case 0x75:		acuteChar=0x00fa;	break;	// uacute
	case 0x79:		acuteChar=0x00fd;	break;	// yacute

	// Latin-2 characters
	case 0x53:		acuteChar=0x01a6;	break;	// Sacute
	case 0x5a:		acuteChar=0x01ac;	break;	// Zacute
	case 0x52:		acuteChar=0x01c0;	break;	// Racute
	case 0x4c:		acuteChar=0x01c5;	break;	// Lacute
	case 0x43:		acuteChar=0x01c6;	break;	// Cacute
	case 0x4e:		acuteChar=0x01d1;	break;	// Nacute

	case 0x73:		acuteChar=0x01b6;	break;	// sacute
	case 0x7a:		acuteChar=0x01bc;	break;	// zacute
	case 0x72:		acuteChar=0x01e0;	break;	// racute
	case 0x6c:		acuteChar=0x01e5;	break;	// lacute
	case 0x63:		acuteChar=0x01e6;	break;	// cacute
	case 0x6e:		acuteChar=0x01f1;	break;	// nacute

	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		return false;
	}

	pView->cmdCharInsert(&acuteChar, 1);
	return true;
}

Defun(insertCircumflexData)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// This function provides an interlude.  All of the keys
	// on the Dead_Circumflex map are mapped here.	The desired
	// character is in the argument (just like in insertData()).
	// We do the character mapping here.
	//
	// See note in Defun(insertGraveData)

	UT_return_val_if_fail (pCallData->m_dataLength==1, false);
	UT_UCS4Char circumflexChar = 0x0000;
	switch (pCallData->m_pData[0])
	{
	case 0x41:		circumflexChar=0x00c2;	break;	// Acircumflex
	case 0x45:		circumflexChar=0x00ca;	break;	// Ecircumflex
	case 0x49:		circumflexChar=0x00ce;	break;	// Icircumflex
	case 0x4f:		circumflexChar=0x00d4;	break;	// Ocircumflex
	case 0x55:		circumflexChar=0x00db;	break;	// Ucircumflex

	case 0x61:		circumflexChar=0x00e2;	break;	// acircumflex
	case 0x65:		circumflexChar=0x00ea;	break;	// ecircumflex
	case 0x69:		circumflexChar=0x00ee;	break;	// icircumflex
	case 0x6f:		circumflexChar=0x00f4;	break;	// ocircumflex
	case 0x75:		circumflexChar=0x00fb;	break;	// ucircumflex

	// Latin-3 characters
	case 0x48:		circumflexChar=0x02a6;	break;	// Hcircumflex
	case 0x4a:		circumflexChar=0x02ac;	break;	// Jcircumflex
	case 0x43:		circumflexChar=0x02c6;	break;	// Ccircumflex
	case 0x47:		circumflexChar=0x02d8;	break;	// Gcircumflex
	case 0x53:		circumflexChar=0x02de;	break;	// Scircumflex

	case 0x68:		circumflexChar=0x02b6;	break;	// hcircumflex
	case 0x6a:		circumflexChar=0x02bc;	break;	// jcircumflex
	case 0x63:		circumflexChar=0x02e6;	break;	// ccircumflex
	case 0x67:		circumflexChar=0x02f8;	break;	// gcircumflex
	case 0x73:		circumflexChar=0x02fe;	break;	// scircumflex

	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		return false;
	}

	pView->cmdCharInsert(&circumflexChar, 1);
	return true;
}

Defun(insertTildeData)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// This function provides an interlude.  All of the keys
	// on the Dead_Tilde map are mapped here.  The desired
	// character is in the argument (just like in insertData()).
	// We do the character mapping here.
	//
	// See note in Defun(insertGraveData)

	UT_return_val_if_fail (pCallData->m_dataLength==1, false);
	UT_UCS4Char tildeChar = 0x0000;
	switch (pCallData->m_pData[0])
	{
	case 0x41:		tildeChar=0x00c3;	break;	// Atilde
	case 0x4e:		tildeChar=0x00d1;	break;	// Ntilde
	case 0x4f:		tildeChar=0x00d5;	break;	// Otilde

	case 0x61:		tildeChar=0x00e3;	break;	// atilde
	case 0x6e:		tildeChar=0x00f1;	break;	// ntilde
	case 0x6f:		tildeChar=0x00f5;	break;	// otilde

	// Latin-4 characters
	case 0x49:		tildeChar=0x03a5;	break;	// Itilde
	case 0x55:		tildeChar=0x03dd;	break;	// Utilde

	case 0x69:		tildeChar=0x03b5;	break;	// itilde
	case 0x75:		tildeChar=0x03fd;	break;	// utilde

	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		return false;
	}

	pView->cmdCharInsert(&tildeChar, 1);
	return true;
}

Defun(insertMacronData)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// This function provides an interlude.  All of the keys
	// on the Dead_Macron map are mapped here.	The desired
	// character is in the argument (just like in insertData()).
	// We do the character mapping here.
	//
	// See note in Defun(insertGraveData)

	UT_return_val_if_fail (pCallData->m_dataLength==1, false);
	UT_UCS4Char macronChar = 0x0000;

	switch (pCallData->m_pData[0])
	{
// Latin-4 characters
	case 0x45:		macronChar=0x03aa;	break;	// Emacron
	case 0x41:		macronChar=0x03c0;	break;	// Amacron
	case 0x49:		macronChar=0x03cf;	break;	// Imacron
	case 0x4f:		macronChar=0x03d2;	break;	// Omacron
	case 0x55:		macronChar=0x03de;	break;	// Umacron

	case 0x65:		macronChar=0x03ba;	break;	// emacron
	case 0x61:		macronChar=0x03e0;	break;	// amacron
	case 0x69:		macronChar=0x03ef;	break;	// imacron
	case 0x6f:		macronChar=0x03f2;	break;	// omacron
	case 0x75:		macronChar=0x03fe;	break;	// umacron

	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		return false;
	}

	pView->cmdCharInsert(&macronChar, 1);
	return true;
}

Defun(insertBreveData)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// This function provides an interlude.  All of the keys
	// on the Dead_Breve map are mapped here.  The desired
	// character is in the argument (just like in insertData()).
	// We do the character mapping here.
	//
	// See note in Defun(insertGraveData)

	UT_return_val_if_fail(pCallData->m_dataLength==1, false);
	UT_UCS4Char breveChar = 0x0000;

	switch (pCallData->m_pData[0])
	{
// Latin-[23] characters
	case 0x41:		breveChar=0x01c3;	break;	// Abreve
	case 0x47:		breveChar=0x02ab;	break;	// Gbreve
	case 0x55:		breveChar=0x02dd;	break;	// Ubreve

	case 0x61:		breveChar=0x01e3;	break;	// abreve
	case 0x67:		breveChar=0x02bb;	break;	// gbreve
	case 0x75:		breveChar=0x02fd;	break;	// ubreve

	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		return false;
	}

	pView->cmdCharInsert(&breveChar, 1);
	return true;
}

Defun(insertAbovedotData)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// This function provides an interlude.  All of the keys
	// on the Dead_Abovedot map are mapped here.  The desired
	// character is in the argument (just like in insertData()).
	// We do the character mapping here.
	//
	// See note in Defun(insertGraveData)

	UT_return_val_if_fail(pCallData->m_dataLength==1, false);
	UT_UCS4Char abovedotChar = 0x0000;

	switch (pCallData->m_pData[0])
	{
// Latin-[234] characters
	case 0x5a:		abovedotChar=0x01af;	break;	// Zabovedot
	case 0x49:		abovedotChar=0x02a9;	break;	// Iabovedot
	case 0x43:		abovedotChar=0x02c5;	break;	// Cabovedot
	case 0x47:		abovedotChar=0x02d5;	break;	// Gabovedot
	case 0x45:		abovedotChar=0x03cc;	break;	// Eabovedot

	case 0x7a:		abovedotChar=0x01bf;	break;	// zabovedot
	//case 0x69: TODO no corresponding 'iabovedot', is this supposed to be 'idotless' ??
	case 0x63:		abovedotChar=0x02e5;	break;	// cabovedot
	case 0x67:		abovedotChar=0x02f5;	break;	// gabovedot
	case 0x65:		abovedotChar=0x03ec;	break;	// eabovedot

	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		return false;
	}

	pView->cmdCharInsert(&abovedotChar, 1);
	return true;
}

Defun(insertDiaeresisData)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// This function provides an interlude.  All of the keys
	// on the Dead_Diaeresis map are mapped here.  The desired
	// character is in the argument (just like in insertData()).
	// We do the character mapping here.
	//
	// See note in Defun(insertGraveData)

	UT_return_val_if_fail(pCallData->m_dataLength==1, false);
	UT_UCS4Char diaeresisChar = 0x0000;
	switch (pCallData->m_pData[0])
	{
	case 0x41:		diaeresisChar=0x00c4;	break;	// Adiaeresis
	case 0x45:		diaeresisChar=0x00cb;	break;	// Ediaeresis
	case 0x49:		diaeresisChar=0x00cf;	break;	// Idiaeresis
	case 0x4f:		diaeresisChar=0x00d6;	break;	// Odiaeresis
	case 0x55:		diaeresisChar=0x00dc;	break;	// Udiaeresis
	// TODO no Ydiaeresis ??

	case 0x61:		diaeresisChar=0x00e4;	break;	// adiaeresis
	case 0x65:		diaeresisChar=0x00eb;	break;	// ediaeresis
	case 0x69:		diaeresisChar=0x00ef;	break;	// idiaeresis
	case 0x6f:		diaeresisChar=0x00f6;	break;	// odiaeresis
	case 0x75:		diaeresisChar=0x00fc;	break;	// udiaeresis
	case 0x79:		diaeresisChar=0x00ff;	break;	// ydiaeresis
	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		return false;
	}

	pView->cmdCharInsert(&diaeresisChar, 1);
	return true;
}

Defun(insertDoubleacuteData)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// This function provides an interlude.  All of the keys
	// on the Dead_Doubleacute map are mapped here.  The desired
	// character is in the argument (just like in insertData()).
	// We do the character mapping here.
	//
	// See note in Defun(insertGraveData)

	UT_return_val_if_fail(pCallData->m_dataLength==1, false);
	UT_UCS4Char doubleacuteChar = 0x0000;

	switch (pCallData->m_pData[0])
	{
// Latin-2 characters
	case 0x4f:		doubleacuteChar=0x01d5; break;	// Odoubleacute
	case 0x55:		doubleacuteChar=0x01db; break;	// Udoubleacute

	case 0x6f:		doubleacuteChar=0x01f5; break;	// odoubleacute
	case 0x75:		doubleacuteChar=0x01fb; break;	// udoubleacute

	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		return false;
	}

	pView->cmdCharInsert(&doubleacuteChar, 1);
	return true;
}

Defun(insertCaronData)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// This function provides an interlude.  All of the keys
	// on the Dead_Caron map are mapped here.  The desired
	// character is in the argument (just like in insertData()).
	// We do the character mapping here.
	//
	// See note in Defun(insertGraveData)

	UT_return_val_if_fail(pCallData->m_dataLength==1, false);
	UT_UCS4Char caronChar = 0x0000;

	switch (pCallData->m_pData[0])
	{
// Latin-2 characters
	case 0x4c:		caronChar=0x013d;	break;	// Lcaron
	case 0x53:		caronChar=0x0160;	break;	// Scaron
	case 0x54:		caronChar=0x0164;	break;	// Tcaron
	case 0x5a:		caronChar=0x017d;	break;	// Zcaron
	case 0x43:		caronChar=0x010c;	break;	// Ccaron
	case 0x45:		caronChar=0x011a;	break;	// Ecaron
	case 0x44:		caronChar=0x010e;	break;	// Dcaron
	case 0x4e:		caronChar=0x0147;	break;	// Ncaron
	case 0x52:		caronChar=0x0158;	break;	// Rcaron

	case 0x6c:		caronChar=0x013e;	break;	// lcaron
	case 0x73:		caronChar=0x0161;	break;	// scaron
	case 0x74:		caronChar=0x0165;	break;	// tcaron
	case 0x7a:		caronChar=0x017e;	break;	// zcaron
	case 0x63:		caronChar=0x010d;	break;	// ccaron
	case 0x65:		caronChar=0x011b;	break;	// ecaron
	case 0x64:		caronChar=0x010f;	break;	// dcaron
	case 0x6e:		caronChar=0x0148;	break;	// ncaron
	case 0x72:		caronChar=0x0159;	break;	// rcaron

	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		return false;
	}

	pView->cmdCharInsert(&caronChar, 1);
	return true;
}

Defun(insertCedillaData)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// This function provides an interlude.  All of the keys
	// on the Dead_Cedilla map are mapped here.  The desired
	// character is in the argument (just like in insertData()).
	// We do the character mapping here.
	//
	// See note in Defun(insertGraveData)

	UT_return_val_if_fail(pCallData->m_dataLength==1, false);
	UT_UCS4Char cedillaChar = 0x0000;
	switch (pCallData->m_pData[0])
	{
	case 0x43:		cedillaChar=0x00c7; break;	// Ccedilla
	case 0x63:		cedillaChar=0x00e7; break;	// ccedilla

	// Latin-[24] characters
	case 0x53:		cedillaChar=0x01aa; break;	// Scedilla
	case 0x54:		cedillaChar=0x01de; break;	// Tcedilla
	case 0x52:		cedillaChar=0x03a3; break;	// Rcedilla
	case 0x4c:		cedillaChar=0x03a6; break;	// Lcedilla
	case 0x47:		cedillaChar=0x03ab; break;	// Gcedilla
	case 0x4e:		cedillaChar=0x03d1; break;	// Ncedilla
	case 0x4b:		cedillaChar=0x03d3; break;	// Kcedilla

	case 0x73:		cedillaChar=0x01ba; break;	// scedilla
	case 0x74:		cedillaChar=0x01fe; break;	// tcedilla
	case 0x72:		cedillaChar=0x03b3; break;	// rcedilla
	case 0x6c:		cedillaChar=0x03b6; break;	// lcedilla
	case 0x67:		cedillaChar=0x03bb; break;	// gcedilla
	case 0x6e:		cedillaChar=0x03f1; break;	// ncedilla
	case 0x6b:		cedillaChar=0x03f3; break;	// kcedilla

	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		return false;
	}

	pView->cmdCharInsert(&cedillaChar, 1);
	return true;
}

Defun(insertOgonekData)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	// This function provides an interlude.  All of the keys
	// on the Dead_Ogonek map are mapped here.	The desired
	// character is in the argument (just like in insertData()).
	// We do the character mapping here.
	//
	// See note in Defun(insertGraveData)

	UT_return_val_if_fail(pCallData->m_dataLength==1, false);
	UT_UCS4Char ogonekChar = 0x0000;

	switch (pCallData->m_pData[0])
	{
// Latin-[24] characters
	case 0x41:		ogonekChar=0x01a1;	break;	// Aogonek
	case 0x45:		ogonekChar=0x01ca;	break;	// Eogonek
	case 0x49:		ogonekChar=0x03c7;	break;	// Iogonek
	case 0x55:		ogonekChar=0x03d9;	break;	// Uogonek

	case 0x65:		ogonekChar=0x01b1;	break;	// eogonek
	case 0x61:		ogonekChar=0x01ea;	break;	// aogonek
	case 0x69:		ogonekChar=0x03e7;	break;	// iogonek
	case 0x75:		ogonekChar=0x03f9;	break;	// uogonek

	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		return false;
	}

	pView->cmdCharInsert(&ogonekChar, 1);
	return true;
}

/*****************************************************************/

Defun1(cut)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	if (pView->isFrameSelected())
	{
		pView->copyFrame(false);
	}
	else
	{
		pView->cmdCut();
	}

	return true;
}

Defun1(copy)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	if (pView->isFrameSelected())
	{
		pView->copyFrame(true);
	}
	else
	{
		pView->cmdCopy();
	}

	return true;
}

static void sActualPaste(AV_View *  pAV_View, EV_EditMethodCallData * /*pCallData*/)
{
	ABIWORD_VIEW;
	pView->cmdPaste();
	return;
}

Defun1(paste)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
//
// Do this operation in an idle loop so when can reject queued events
//
//
// This code sets things up to handle the warp right in an idle loop.
//
	UT_return_val_if_fail(pView, false);
	int inMode = UT_WorkerFactory::IDLE | UT_WorkerFactory::TIMER;
	UT_WorkerFactory::ConstructMode outMode = UT_WorkerFactory::NONE;
	_Freq * pFreq = new _Freq(pView,nullptr,sActualPaste);
	s_pFrequentRepeat = UT_WorkerFactory::static_constructor (_sFrequentRepeat,pFreq, inMode, outMode);

	UT_ASSERT(s_pFrequentRepeat);
	UT_ASSERT(outMode != UT_WorkerFactory::NONE);

	// If the worker is working on a timer instead of in the idle
	// time, set the frequency of the checks.
	if ( UT_WorkerFactory::TIMER == outMode )
	{
		// this is really a timer, so it's safe to static_cast it
		static_cast<UT_Timer*>(s_pFrequentRepeat)->set(50);
	}
	s_pFrequentRepeat->start();

	return true;
}

Defun(pasteSelection)
{
	CHECK_FRAME;
// this is intended for the X11 middle mouse thing.
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->cmdPasteSelectionAt(pCallData->m_xPos, pCallData->m_yPos);

	return true;
}

Defun1(pasteSpecial)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->cmdPaste(false);

	return true;
}

static bool checkViewModeIsPrint(FV_View * pView)
{
	UT_return_val_if_fail(pView, false);
	if(pView->getViewMode() != VIEW_PRINT)
	{
		XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
		UT_return_val_if_fail(pFrame, false);
		XAP_Dialog_MessageBox::tAnswer res = pFrame->showMessageBox(AP_STRING_ID_MSG_CHECK_PRINT_MODE,
				   XAP_Dialog_MessageBox::b_YN,
				   XAP_Dialog_MessageBox::a_NO);
		if(res == XAP_Dialog_MessageBox::a_NO)
		{
			return false;
		}
		else
		{

			AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
			UT_return_val_if_fail (pFrameData, false);

			pFrameData->m_pViewMode = VIEW_PRINT;
			pFrame->toggleLeftRuler (true && (pFrameData->m_bShowRuler) &&
									 (!pFrameData->m_bIsFullScreen));


			pView->setViewMode (VIEW_PRINT);

			// POLICY: make this the default for new frames, too
			XAP_App * pApp = XAP_App::getApp();
			UT_return_val_if_fail(pApp, false);
			XAP_Prefs * pPrefs = pApp->getPrefs();
			UT_return_val_if_fail(pPrefs, false);
			XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
			UT_return_val_if_fail (pScheme, false);

			pScheme->setValue(AP_PREF_KEY_LayoutMode, "1");

			pView->updateScreen(false);
			pView->notifyListeners(AV_CHG_ALL);
		}
	}
	return true;
}

Defun1(editFooter)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	if(checkViewModeIsPrint(pView))
	{
		pView->cmdEditFooter();
	}
	return true;
}

Defun1(removeHeader)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	if(checkViewModeIsPrint(pView))
	{
		pView->cmdRemoveHdrFtr(true);
	}
	return true;
}

Defun1(removeFooter)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	if(checkViewModeIsPrint(pView))
	{
		pView->cmdRemoveHdrFtr(false);
	}
	return true;
}

Defun1(editHeader)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView, false);
	if(checkViewModeIsPrint(pView))
	{
		pView->cmdEditHeader();
	}
	return true;
}

/*****************************************************************/

static bool s_doGotoDlg(FV_View * pView, XAP_Dialog_Id id)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());

	AP_Dialog_Goto * pDialog
		= static_cast<AP_Dialog_Goto *>(pDialogFactory->requestDialog(id));
UT_return_val_if_fail(pDialog, false);
	if(pDialog->isRunning() == true)
	{
		pDialog->activate();
	}
	else
	{
		pDialog->setView(pView);
		pDialog->runModeless(pFrame);
	}
	return true;
}

Defun1(go)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	XAP_Dialog_Id id = (XAP_Dialog_Id)AP_DIALOG_ID_GOTO;

	return s_doGotoDlg(pView, id);
}


/*****************************************************************/

#ifdef ENABLE_SPELL
static bool s_doSpellDlg(FV_View * pView, XAP_Dialog_Id id)
{
   UT_return_val_if_fail(pView,false);
   XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
   UT_return_val_if_fail(pFrame, false);

   pFrame->raise();

   XAP_DialogFactory * pDialogFactory
	 = static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

   AP_Dialog_Spell * pDialog
	 = static_cast<AP_Dialog_Spell *>(pDialogFactory->requestDialog(id));
   UT_return_val_if_fail (pDialog, false);

   // run the dialog (it probably should be modeless if anyone
   // gets the urge to make it safe that way)
   pDialog->runModal(pFrame);
   bool bOK = pDialog->isComplete();

   if (bOK)
	   s_TellSpellDone(pFrame, pDialog->isSelection());

   pDialogFactory->releaseDialog(pDialog);

   return bOK;
}


Defun1(dlgSpell)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	XAP_Dialog_Id id = (XAP_Dialog_Id)AP_DIALOG_ID_SPELL;

   return s_doSpellDlg(pView,id);
}
#endif

/*****************************************************************/

static bool s_doFindOrFindReplaceDlg(FV_View * pView, XAP_Dialog_Id id)
{
	UT_return_val_if_fail(pView,false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());

	AP_Dialog_Replace * pDialog
		= static_cast<AP_Dialog_Replace *>(pDialogFactory->requestDialog(id));
UT_return_val_if_fail(pDialog, false);
	// don't match case by default
	pDialog->setMatchCase(false);

	// prime the dialog with a "find" string if there's a
	// current selection.
	if (!pView->isSelectionEmpty())
	{
		UT_UCS4Char * buffer;
		pView->getSelectionText(buffer);
		if(buffer != nullptr)
		{
			pDialog->setFindString(buffer);
			FREEP(buffer);
		}
		else
		{
			pView->setPoint(pView->getPoint());
		}
	}

	// run the dialog (it should really be modeless if anyone
	// gets the urge to make it safe that way)
	// OK I Will
	if(pDialog->isRunning() == true)
	{
		   pDialog->activate();
	}
		else
	{
		   pDialog->runModeless(pFrame);
	}
	bool bOK = true;
	return bOK;
}


Defun1(find)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	XAP_Dialog_Id id = (XAP_Dialog_Id)AP_DIALOG_ID_FIND;

	return s_doFindOrFindReplaceDlg(pView,id);
}

Defun1(findAgain)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView, false);
	return pView->findAgain();
}

Defun1(replace)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	XAP_Dialog_Id id = (XAP_Dialog_Id)AP_DIALOG_ID_REPLACE;

	return s_doFindOrFindReplaceDlg(pView,id);
}

/*****************************************************************/

static bool s_doLangDlg(FV_View * pView)
{
	UT_return_val_if_fail(pView,false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_Dialog_Id id = XAP_DIALOG_ID_LANGUAGE;

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_Language * pDialog
		= static_cast<XAP_Dialog_Language *>(pDialogFactory->requestDialog(id));
	UT_return_val_if_fail(pDialog, false);

	PP_PropertyVector props_in;
	if (pView->getCharFormat(props_in))
	{
		pDialog->setLanguageProperty(PP_getAttribute("lang", props_in).c_str());
	}

	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail( pDoc, false );

	const PP_AttrProp *  pAP = pDoc->getAttrProp();
	UT_return_val_if_fail( pAP, false );

	const gchar * pLang = nullptr;
	bool bRet = pAP->getProperty("lang", pLang);

	if(bRet)
	{
		pDialog->setDocumentLanguage(pLang);
	}
	else
	{
		UT_ASSERT_HARMLESS( UT_SHOULD_NOT_HAPPEN );
	}

	// sample text for the "Detect language automatically" checkbox:
	// the selection, or the text around the caret
	{
		UT_UCS4Char * pSample = nullptr;
		if (!pView->isSelectionEmpty())
		{
			pView->getSelectionText(pSample);
		}
		else
		{
			PT_DocPosition posBOD, posEOD;
			pView->getEditableBounds(false, posBOD);
			pView->getEditableBounds(true, posEOD);
			PT_DocPosition pos = pView->getPoint();
			PT_DocPosition lo = (pos > posBOD + 500) ? pos - 500 : posBOD;
			PT_DocPosition hi = UT_MIN(pos + 1500, posEOD);
			if (hi > lo)
				pSample = pView->getTextBetweenPos(lo, hi);
		}
		if (pSample)
		{
			UT_UCS4String sSample(pSample);
			pDialog->setSampleText(sSample.utf8_str());
			delete [] pSample;
		}
	}

	// run the dialog

	pDialog->runModal(pFrame);

	// extract what they did

	bool bOK = (pDialog->getAnswer() == XAP_Dialog_Language::a_OK);

	if (bOK)
	{
		//UT_DEBUGMSG(("pressed OK\n"));
		PP_PropertyVector props_out;
		const gchar * s = nullptr;

		bool bChange = pDialog->getChangedLangProperty(&s);
		if (s)
		{
			props_out.push_back("lang");
			props_out.push_back(s);
		}

		if(bChange && !props_out.empty())					// if something changed
			pView->setCharFormat(props_out);

		if(!props_out.empty() && pDialog->isMakeDocumentDefault()
		   && (!pLang || strcmp(pLang, s)))
		{
#ifdef ENABLE_SPELL
			FL_DocLayout* pLayout = pView->getLayout();
			
			if(pLayout)
				pLayout->queueAll(FL_DocLayout::bgcrSpelling | FL_DocLayout::bgcrGrammar);
#endif
			pDoc->setProperties(props_out);
		}
		
	}

	pDialogFactory->releaseDialog(pDialog);

	return bOK;
}

/*****************************************************************/

static bool s_doFontDlg(FV_View * pView)
{
	UT_return_val_if_fail(pView,false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_Dialog_Id id = XAP_DIALOG_ID_FONT;

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_FontChooser * pDialog
		= static_cast<XAP_Dialog_FontChooser *>(pDialogFactory->requestDialog(id));
UT_return_val_if_fail(pDialog, false);
	// stuff the GR_Graphics into the dialog so that it
	// can query the system for font info relative to our
	// context.

	pDialog->setGraphicsContext(pView->getLayout()->getGraphics());

	// get current font info from pView

	PP_PropertyVector props_in;
	if (pView->getCharFormat(props_in))
	{
		// stuff font properties into the dialog.
		// for a/p which are constant across the selection (always
		// present) we will set the field in the dialog.  for things
		// which change across the selection, we ask the dialog not
		// to set the field (by passing "").

		const std::string & sFontFamily = PP_getAttribute("font-family", props_in);
		const std::string & sTextTransform = PP_getAttribute("text-transform", props_in);
		const std::string & sFontSize = PP_getAttribute("font-size", props_in);
		const std::string & sFontWeight = PP_getAttribute("font-weight", props_in);
		const std::string & sFontStyle = PP_getAttribute("font-style", props_in);
		const std::string & sColor = PP_getAttribute("color", props_in);
		const std::string & sBGColor = PP_getAttribute("bgcolor", props_in);

		pDialog->setFontFamily(sFontFamily);
		pDialog->setTextTransform(sTextTransform);
		pDialog->setFontSize(sFontSize);
		pDialog->setFontWeight(sFontWeight);
		pDialog->setFontStyle(sFontStyle);
		pDialog->setColor(sColor);
		pDialog->setBGColor(sBGColor);

//
// Set the background color for the preview
//
		gchar  background[8];
		const UT_RGBColor * bgCol = pView->getCurrentPage()->getFillType().getColor();
		snprintf(background, 8, "%02x%02x%02x", bgCol->m_red,
				bgCol->m_grn, bgCol->m_blu);
		pDialog->setBackGroundColor( static_cast<const gchar *>(background));

		// these behave a little differently since they are
		// probably just check boxes and we don't have to
		// worry about initializing a combo box with a choice
		// (and because they are all stuck under one CSS attribute).

		bool bUnderline = false;
		bool bOverline = false;
		bool bStrikeOut = false;
		bool bTopLine = false;
		bool bBottomLine = false;

		const std::string & s = PP_getAttribute("text-decoration", props_in);
		if (!s.empty())
		{
			bUnderline = (s.find("underline") != std::string::npos);
			bOverline = (s.find("overline") != std::string::npos);
			bStrikeOut = (s.find("line-through") != std::string::npos);
			bTopLine = (s.find("topline") != std::string::npos);
			bBottomLine = (s.find("bottomline") != std::string::npos);
		}
		pDialog->setFontDecoration(bUnderline,bOverline,bStrikeOut,bTopLine,bBottomLine);

		bool bHidden = false;
		const std::string & disp = PP_getAttribute("display", props_in);
		if(!disp.empty())
		{
			bHidden = (disp.find("none") != std::string::npos);
		}
		pDialog->setHidden(bHidden);

		bool bSuperScript = false;
		bool bSubScript = false;
		const std::string & textPos = PP_getAttribute("text-position", props_in);
		if(!textPos.empty())
		{
			bSuperScript = (textPos.find("superscript") != std::string::npos);
			bSubScript = (textPos.find("subscript") != std::string::npos);
		}
		pDialog->setSuperScript(bSuperScript);
		pDialog->setSubScript(bSubScript);
	}

	if(!pView->isSelectionEmpty())
	{
	    // set the drawable string to the selection text
		// the pointer return by getSelectionText() must be freed
		UT_UCS4Char* text = nullptr;
		pView->getSelectionText(text);
		if(text)
		{
			pDialog->setDrawString(text);
			FREEP(text);
		}
	}

	// run the dialog

	pDialog->runModal(pFrame);

	// extract what they did

	bool bOK = (pDialog->getAnswer() == XAP_Dialog_FontChooser::a_OK);

	if (bOK)
	{
		PP_PropertyVector props_out;
		std::string s;

		if (pDialog->getChangedFontFamily(s))
		{
			props_out.push_back("font-family");
			props_out.push_back(s);
		}

		if (pDialog->getChangedTextTransform(s))
		{
			props_out.push_back("text-transform");
			props_out.push_back(s);
		}

		if (pDialog->getChangedFontSize(s))
		{
			props_out.push_back("font-size");
			props_out.push_back(s);
		}

		if (pDialog->getChangedFontWeight(s))
		{
			props_out.push_back("font-weight");
			props_out.push_back(s);
		}

		if (pDialog->getChangedFontStyle(s))
		{
			props_out.push_back("font-style");
			props_out.push_back(s);
		}

		if (pDialog->getChangedColor(s))
		{
			props_out.push_back("color");
			props_out.push_back(s);
		}

		if (pDialog->getChangedBGColor(s))
		{
			props_out.push_back("bgcolor");
			props_out.push_back(s);
		}

		bool bUnderline = false;
		bool bChangedUnderline = pDialog->getChangedUnderline(&bUnderline);
		bool bOverline = false;
		bool bChangedOverline = pDialog->getChangedOverline(&bOverline);
		bool bStrikeOut = false;
		bool bChangedStrikeOut = pDialog->getChangedStrikeOut(&bStrikeOut);
		bool bTopline = false;
		bool bChangedTopline = pDialog->getChangedTopline(&bTopline);
		bool bBottomline = false;
		bool bChangedBottomline = pDialog->getChangedBottomline(&bBottomline);
		std::string decors;
		if (bChangedUnderline || bChangedStrikeOut || bChangedOverline || bChangedTopline || bChangedBottomline)
		{
			if(bUnderline)
				decors += "underline ";
			if(bStrikeOut)
				decors += "line-through ";
			if(bOverline)
				decors += "overline ";
			if(bTopline)
				decors += "topline ";
			if(bBottomline)
				decors += "bottomline ";
			if(!bUnderline && !bStrikeOut && !bOverline && !bTopline && !bBottomline)
				decors = "none";
			props_out.push_back("text-decoration");
			props_out.push_back(decors);
		}

		bool bHidden = false;
		bool bChangedHidden = pDialog->getChangedHidden(&bHidden);

		if (bChangedHidden)
		{
			if(bHidden)
			{
				props_out.push_back("display");
				props_out.push_back("none");
			}
			else
			{
				props_out.push_back("display");
				props_out.push_back("inline");
			}
		}

		bool bSuperScript = false;
		bool bChangedSuperScript = pDialog->getChangedSuperScript(&bSuperScript);

		if (bChangedSuperScript)
		{
			if(bSuperScript)
			{
				props_out.push_back("text-position");
				props_out.push_back("superscript");
			}
			else
			{
				props_out.push_back("text-position");
				props_out.push_back("");
			}
		}

		bool bSubScript = false;
		bool bChangedSubScript = pDialog->getChangedSubScript(&bSubScript);

		if (!(bChangedSuperScript && bSuperScript)) /* skip setting subscript if we just enabled superscript */
		{
			if (bChangedSubScript)
			{
				if(bSubScript)
				{
					props_out.push_back("text-position");
					props_out.push_back("subscript");
				}
				else
				{
					props_out.push_back("text-position");
					props_out.push_back("");
				}
			}
		}

		if (!props_out.empty()) {				// if something changed
			pView->setCharFormat(props_out);
		}
	}

	pDialogFactory->releaseDialog(pDialog);

	return bOK;
}

static bool s_doParagraphDlg(FV_View * pView)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_Paragraph * pDialog
		= static_cast<AP_Dialog_Paragraph *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_PARAGRAPH));
	UT_return_val_if_fail(pDialog, false);
	PP_PropertyVector props;

	if (!pView->getBlockFormat(props))
		return false;

	if (!pDialog->setDialogData(props))
		return false;

	// let's steal the width from getTopRulerInfo.
	AP_TopRulerInfo info;
	pView->getTopRulerInfo(&info);

	// TODO tables
	pDialog->setMaxWidth (info.u.c.m_xColumnWidth);

	// run the dialog
	pDialog->runModal(pFrame);

	// get the dialog answer
	AP_Dialog_Paragraph::tAnswer answer = pDialog->getAnswer();

	switch (answer)
	{
	case AP_Dialog_Paragraph::a_OK:

		pDialog->getDialogData(props);

		// set properties back to document
		if (!props.empty())
			pView->setBlockFormat(props);

		break;

	case AP_Dialog_Paragraph::a_CANCEL:
		// do nothing
		break;
	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
	}

	pDialogFactory->releaseDialog(pDialog);

	return true;
}


static bool s_doOptionsDlg(FV_View * pView, int which = -1)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());
	XAP_TabbedDialog_NonPersistent * pDialog
		= static_cast<XAP_TabbedDialog_NonPersistent *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_OPTIONS));
	UT_return_val_if_fail(pDialog, false);

	if ( which != -1 )
	  pDialog->setInitialPageNum(which);
	else
	  pDialog->setInitialPageNum(0);

	// run the dialog
	pDialog->runModal(pFrame);

	pDialogFactory->releaseDialog(pDialog);

	return true;
}

/****************************************************************/

Defun1(dlgLanguage)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	return s_doLangDlg(pView);
}

Defun(language)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);

	char lang[10];
	UT_return_val_if_fail(pCallData->m_dataLength < sizeof(lang),false);

	UT_uint32 i = 0;
	for(i = 0; i < pCallData->m_dataLength; i++)
		lang[i] = static_cast<char>(pCallData->m_pData[i]);
	lang[i] = 0;

	PP_PropertyVector properties = {
		"lang", lang
	};
	pView->setCharFormat(properties);
	return true;
}

/*****************************************************************/

Defun1(dlgFont)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView, false);
	if (pView->getDocument()->areStylesLocked())
		return true;

	return s_doFontDlg(pView);
}

Defun(fontFamily)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView, false);
	UT_UTF8String utf8(pCallData->m_pData, pCallData->m_dataLength);
	const PP_PropertyVector properties = {
		"font-family", utf8.utf8_str()
	};
	pView->setCharFormat(properties);

	return true;
}

Defun(fontSize)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView, false);
	UT_UTF8String utf8(pCallData->m_pData, pCallData->m_dataLength);
	const gchar * sz = utf8.utf8_str();

	if (sz && *sz)
	{
		std::string buf(sz);
		buf += "pt";

		PP_PropertyVector properties = {
			"font-size", buf
		};
		pView->setCharFormat(properties);
	}
	return true;
}

static bool _fontSizeChange(FV_View * pView, bool bIncrease)
{
	UT_return_val_if_fail(pView, false);
	PP_PropertyVector span_props;

	pView->getCharFormat(span_props);
	UT_return_val_if_fail(!span_props.empty(), false);

	const std::string & s = PP_getAttribute("font-size", span_props);

	if(s.empty())
		return false;

	double dPoints = UT_convertToPoints(s.c_str());

#define PT_INC_SMALL  1.0
#define PT_INC_MEDIUM 2.0
#define PT_INC_LARGE  4.0
	
	if(bIncrease)
	{
		if(dPoints >= 26.0)
			dPoints += PT_INC_LARGE;
		else if(dPoints >= 8.0)
			dPoints += PT_INC_MEDIUM;
		else
			dPoints += PT_INC_SMALL;
	}
	else
	{
		if(dPoints > 26.0)
			dPoints -= PT_INC_LARGE;
		else if(dPoints > 8.0)
			dPoints -= PT_INC_MEDIUM;
		else
			dPoints -= PT_INC_SMALL;
		
	}

#undef PT_INC_SMALL
#undef PT_INC_MEDIUM
#undef PT_INC_LARGE

	// make sure that we do not decrease fonts too far ...
	if(dPoints < 2.0)
		return false;
	
	const gchar * sz = UT_formatDimensionString(DIM_PT, dPoints);

	if(!sz || !*sz)
		return false;

	PP_PropertyVector properties = {
		"font-size", sz
	};
	pView->setCharFormat(properties);

	return true;
}

Defun1(fontSizeIncrease)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	
	return _fontSizeChange(pView, true);
}

/*! Clear direct character formatting from the selection, keeping
 *  language ("Clear formatting" / LibreOffice Ctrl+M equivalent). */
Defun1(clearFormatting)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	return pView->resetCharFormat(false);
}

// Ctrl+Q (Word): remove direct paragraph formatting, keep the style
Defun1(clearParaFormatting)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	if (pView->getDocument()->areStylesLocked())
		return true;

	return pView->resetBlockFormat();
}

Defun1(fontSizeDecrease)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	
	return _fontSizeChange(pView, false);
}

Defun1(cairoPrint)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Cairo Print \n"));
	UT_return_val_if_fail (pView, false);

	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_Print * pDialog
		= static_cast<XAP_Dialog_Print *>(pDialogFactory->requestDialog(XAP_DIALOG_ID_PRINT));
	pView->setCursorWait();
	pDialog->setPreview(false);
	pDialog->runModal(pFrame);
	GR_Graphics * pGraphics = pDialog->getPrinterGraphicsContext();
	pDialog->releasePrinterGraphicsContext(pGraphics);
	pView->clearCursorWait();
	s_pLoadingFrame = nullptr;
	pView->setPoint(pView->getPoint());
	pView->updateScreen(false);
	pDialogFactory->releaseDialog(pDialog);
	return true;
}


Defun1(cairoPrintPreview)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Cairo Print Preview\n"));
	UT_return_val_if_fail (pView, false);

	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_Print * pDialog
		= static_cast<XAP_Dialog_Print *>(pDialogFactory->requestDialog(XAP_DIALOG_ID_PRINT));
	pView->setCursorWait();
	pDialog->setPreview(true);
	pDialog->runModal(pFrame);
	GR_Graphics * pGraphics = pDialog->getPrinterGraphicsContext();
	pDialog->releasePrinterGraphicsContext(pGraphics);
	pView->clearCursorWait();
	s_pLoadingFrame = nullptr;
	pView->setPoint(pView->getPoint());
	pView->updateScreen(false);
	pDialogFactory->releaseDialog(pDialog);
	return true;
}


Defun1(cairoPrintDirectly)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Cairo Print Directly\n"));
	UT_return_val_if_fail (pView, false);

	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_Print * pDialog
		= static_cast<XAP_Dialog_Print *>(pDialogFactory->requestDialog(XAP_DIALOG_ID_PRINT));
	pView->setCursorWait();
	pDialog->setPreview(false);
	//
	// DOM you can use this for your command line printing
	//
	pDialog->PrintDirectly(pFrame,/*filename*/ nullptr, /*printer name */nullptr);
	GR_Graphics * pGraphics = pDialog->getPrinterGraphicsContext();
	pDialog->releasePrinterGraphicsContext(pGraphics);
	pView->clearCursorWait();
	s_pLoadingFrame = nullptr;
	pView->updateScreen(false);
	pDialogFactory->releaseDialog(pDialog);
	return true;
	return true;
}

Defun1(formatPainter)
{
  CHECK_FRAME;
  ABIWORD_VIEW;

  // prereqs: !pView->isSelectionEmpty() && XAP_App::getApp()->canPasteFromClipboard()
  // taken care of in ap_Toolbar_Functions.cpp::ap_ToolbarGetState_Clipboard

  UT_return_val_if_fail(pView, false);
  PP_PropertyVector block_properties;
  PP_PropertyVector span_properties;

  // get the current document's selected range
  PD_DocumentRange range;
  pView->getDocumentRangeOfCurrentSelection (&range);

  // now create a new (invisible) view to paste our clipboard contents into
  PD_Document * pNewDoc = new PD_Document();
  pNewDoc->newDocument();

  FL_DocLayout *pDocLayout = new FL_DocLayout(pNewDoc, pView->getGraphics());
  FV_View pPasteView(XAP_App::getApp(), nullptr, pDocLayout);
  pDocLayout->setView (&pPasteView);
  pDocLayout->fillLayouts();
  pDocLayout->formatAll();

  // paste contents
  pPasteView.cmdPaste (true);

  // select all so that we can get the block & span properties
  pPasteView.cmdSelect(0, 0, FV_DOCPOS_BOD, FV_DOCPOS_EOD);

  // get the paragraph and span/character formatting properties of
  // the clipboard selection
  if (!pPasteView.getBlockFormat(block_properties))
    {
      UT_DEBUGMSG(("DOM: No block attributes in the new paragraph!\n"));
    }

  if (!pPasteView.getCharFormat(span_properties))
    {
      UT_DEBUGMSG(("DOM: No span attributes in the new paragraph!\n"));
    }

  // reset what was selected before setting the block and char formatting
  pView->cmdSelect (range.m_pos1, range.m_pos2) ;

  // set the current selection's properties to what's on the clipboard
  if (!block_properties.empty())
    pView->setBlockFormat (block_properties);
  if (!span_properties.empty())
    pView->setCharFormat (span_properties);

  DELETEP(pDocLayout);
  UNREFP(pNewDoc);

  return true;
}

/*****************************************************************/

static bool _toggleSpanOrBlock(FV_View * pView,
				  const gchar * prop,
				  const gchar * vOn,
				  const gchar * vOff,
				  bool bMultiple,
				  bool isSpan)
{
	UT_return_val_if_fail(pView, false);
	if (pView->getDocument()->areStylesLocked())
		return true;


	// get current font info from pView
	PP_PropertyVector props_in;

	if (isSpan)
	{
		if (!pView->getCharFormat(props_in))
		return false;
	}
	else // isBlock
	{
		if (!pView->getBlockFormat(props_in))
		return false;
	}

	PP_PropertyVector props_out = {
		prop, vOn
	};

	const std::string & s = PP_getAttribute(prop, props_in);
	if (!s.empty())
	{
		if (bMultiple)
		{
			// some properties have multiple values
			std::size_t	n = s.find(vOn);

			if (n != std::string::npos)
			{
				// yep...
				if (s.find(vOff) != std::string::npos)
				{
					UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
				}

				// ... take it out
				std::string buf(s.cbegin(), s.cbegin() + n);
				buf += s.substr(n + strlen(vOn));

				if (buf.find(' ') != std::string::npos)
					props_out[1] = buf; 	// yep, use it
				else
					props_out[1] = vOff;	// nope, clear it
			}
			else
			{
				// nope...
				if (g_ascii_strcasecmp(s.c_str(), vOff))
				{
					props_out[1] = s + " " + vOn;
				}
			}
		}
		else
		{
			if (0 == g_ascii_strcasecmp(s.c_str(), vOn))
				props_out[1] = vOff;
		}
	}


	// set it either way

	if (isSpan)
	  pView->setCharFormat(props_out);
	else // isBlock
	  pView->setBlockFormat(props_out);

	return true;
}

static bool _toggleSpan(FV_View * pView,
			   const gchar * prop,
			   const gchar * vOn,
			   const gchar * vOff,
			   bool bMultiple=false)
{
  return _toggleSpanOrBlock (pView, prop, vOn, vOff, bMultiple, true);
}


/*****************************************************************/
/*****************************************************************/

bool s_actuallyPrint(PD_Document *doc,  GR_Graphics *pGraphics,
		     FV_View * pPrintView, const char *pDocName,
		     UT_uint32 nCopies, bool bCollate,
		     UT_sint32 iWidth,  UT_sint32 iHeight,
		     UT_sint32 nToPage, UT_sint32 nFromPage)
{
	std::set<UT_sint32> pages;
	for (UT_sint32 i = nFromPage; i <= nToPage; i++)
		{
			pages.insert(i);
		}

	return s_actuallyPrint(doc, pGraphics, pPrintView, pDocName, 
						   nCopies, bCollate, iWidth, iHeight, pages);
}

bool s_actuallyPrint(PD_Document *doc,  GR_Graphics *pGraphics,
		     FV_View * pPrintView, const char *pDocName,
		     UT_uint32 nCopies, bool bCollate,
		     UT_sint32 iWidth,  UT_sint32 iHeight,
		     const std::set<UT_sint32>& pages)
{
	UT_uint32 i,j,k;

	//
	// Lock out operations on this document
	//
	s_pLoadingDoc = static_cast<AD_Document *>(doc);

	if(pGraphics->startPrint())
	{
	  fp_PageSize ps = pPrintView->getPageSize();
	  bool orient = ps.isPortrait ();
	  pGraphics->setPortrait (orient);

	  const XAP_StringSet *pSS = XAP_App::getApp()->getStringSet ();
	  const gchar * msgTmpl = pSS->getValue (AP_STRING_ID_MSG_PrintStatus);

	  gchar msgBuf [1024];

	  dg_DrawArgs da;
	  da.pG = pGraphics;

	  XAP_Frame * pFrame = XAP_App::getApp()->getLastFocussedFrame ();

		if (bCollate)
		{
			for (j=1; (j <= nCopies); j++)
				{
					i = 0;
					for (std::set<UT_sint32>::const_iterator page = pages.begin();
						 page != pages.end();
						 page++)
						{
							i++;
							k = *page;
							snprintf (msgBuf, 1024, msgTmpl, i, pages.size());

							if(pFrame) {
								pFrame->setStatusMessage ( msgBuf );
								pFrame->nullUpdate();
							}

							// NB we will need a better way to calc
							// pGraphics->m_iRasterPosition when
							// iHeight is allowed to vary page to page
							pGraphics->m_iRasterPosition = (k-1)*iHeight;
							pGraphics->startPage(pDocName, k, orient, iWidth, iHeight);
							pPrintView->drawPage(k-1, &da);
						}
				}
		}
		else
		{
			i = 0;
			for (std::set<UT_sint32>::const_iterator page = pages.begin();
				 page != pages.end();
				 page++)
				{
					k = *page;
					i++;

					for (j=1; (j <= nCopies); j++)
						{
							snprintf (msgBuf, 1024, msgTmpl, i, pages.size());

							if(pFrame) {
								pFrame->setStatusMessage ( msgBuf );
								pFrame->nullUpdate();
							}

							// NB we will need a better way to calc
							// pGraphics->m_iRasterPosition when
							// iHeight is allowed to vary page to page
							pGraphics->m_iRasterPosition = (k-1)*iHeight;
							pGraphics->startPage(pDocName, k, orient, iWidth, iHeight);
							pPrintView->drawPage(k-1, &da);
						}
				}
		}
		pGraphics->endPrint();

		if(pFrame)
		  pFrame->setStatusMessage (""); // reset/0 out status bar
	}
	s_pLoadingDoc = nullptr;

	return true;
}

#ifdef ENABLE_PRINT
static bool s_doPrint(FV_View * pView, bool bTryToSuppressDialog,bool bPrintDirectly)
{
	UT_return_val_if_fail (pView, false);

	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
	UT_return_val_if_fail(pFrameData, false);

	if (pView->getViewMode() != VIEW_PRINT)
	{
		/* if the current view is in normal mode, switch to print layout
		 * first, this ensure that the printed layout is as it should be.
		 * We force screen update here, otherwise the screen is messed up
		 * under the print dialog
		 */
		pFrameData->m_pViewMode = VIEW_PRINT;
		pView->setViewMode (VIEW_PRINT);
		pView->updateScreen (false);
	}
	
	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_Print * pDialog
		= static_cast<XAP_Dialog_Print *>(pDialogFactory->requestDialog(bPrintDirectly? XAP_DIALOG_ID_PRINT_DIRECTLY: XAP_DIALOG_ID_PRINT));
UT_return_val_if_fail(pDialog, false);
	FL_DocLayout* pLayout = pView->getLayout();
	PD_Document * doc = pLayout->getDocument();

	pDialog->setPaperSize (pView->getPageSize().getPredefinedName());
	pDialog->setDocumentTitle(pFrame->getNonDecoratedTitle());
	pDialog->setDocumentPathname((!doc->getFilename().empty())
								 ? doc->getFilename().c_str()
								 : pFrame->getNonDecoratedTitle());
	pDialog->setEnablePageRangeButton(true,1,pLayout->countPages());
	pDialog->setEnablePrintSelection(false);	// TODO change this when we know how to do it.
	pDialog->setEnablePrintToFile(true);
	pDialog->setTryToBypassActualDialog(bTryToSuppressDialog);

	pDialog->runModal(pFrame);

	XAP_Dialog_Print::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == XAP_Dialog_Print::a_OK);
	bool bHideFmtMarks = false;

	if (bOK)
	{

//
// Turn on Wait cursor
//
		pView->setCursorWait();
		const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
		UT_String msg (pSS->getValue(AP_STRING_ID_MSG_PrintingDoc));

		pFrame->setStatusMessage ( static_cast<const gchar *>(msg.c_str()) );

		GR_Graphics * pGraphics = pDialog->getPrinterGraphicsContext();

		if (!pGraphics)
		{
			pFrame->showMessageBox(AP_STRING_ID_PRINT_CANNOTSTARTPRINTJOB,
				   XAP_Dialog_MessageBox::b_O,
				   XAP_Dialog_MessageBox::a_OK);

		   return false;
		}

		UT_return_val_if_fail (pGraphics->queryProperties(GR_Graphics::DGP_PAPER), false);

		/*
		We need to re-layout the document for now, so the UnixPSGraphics class will
		get it's font list filled. When we find a better way to fill the UnixPSGraphics
		font list, we can remove the 4 lines below. - MARCM
		*/
		//
		FL_DocLayout * pDocLayout = nullptr;
		FV_View * pPrintView = nullptr;
		bool canQuickPrint = pGraphics->canQuickPrint();
		if(!canQuickPrint)
		{
				pDocLayout = new FL_DocLayout(doc,pGraphics);
				pPrintView = new FV_View(XAP_App::getApp(), nullptr, pDocLayout);
				pPrintView->getLayout()->fillLayouts();
				pPrintView->getLayout()->formatAll();
				pPrintView->getLayout()->recalculateTOCFields();
		}
		else
		{
				pDocLayout = pLayout;
				pPrintView = pView;
				pDocLayout->setQuickPrint(pGraphics);
				if(pFrameData->m_bShowPara)
				{
					pPrintView->setShowPara(false);
					bHideFmtMarks = true;
				}
		}

		UT_sint32 nFromPage, nToPage;
		static_cast<void>(pDialog->getDoPrintRange(&nFromPage,&nToPage));

		// must use the layout of the print view here !!!
		if (nToPage > pPrintView->getLayout()->countPages())
		  nToPage = pPrintView->getLayout()->countPages();

		// TODO add code to handle getDoPrintSelection()

		UT_uint32 nCopies = pDialog->getNrCopies();
		bool bCollate = pDialog->getCollate();

		// TODO these are here temporarily to make printing work.  We'll fix the hack later.
		// BUGBUG assumes all pages are same size and orientation
		// Must use the layout create with printer graphics here, because the screen
		// layout adds screen margins to the width and height
		UT_sint32 iWidth = pDocLayout->getWidth();
		UT_sint32 iHeight = pDocLayout->getHeight() / pDocLayout->countPages();

		const char *pDocName = ((!doc->getFilename().empty()) ? doc->getFilename().c_str() : pFrame->getNonDecoratedTitle());
		s_actuallyPrint(doc, pGraphics, pPrintView, pDocName, nCopies, bCollate,
				iWidth,  iHeight, nToPage, nFromPage);

		if(!canQuickPrint)
		{
			delete pDocLayout;
			delete pPrintView;
		}
		else
		{
			if(bHideFmtMarks)
				pPrintView->setShowPara(true);

			pDocLayout->setQuickPrint(nullptr);
		}
		pDialog->releasePrinterGraphicsContext(pGraphics);

//
// Turn off wait cursor
//
		pView->clearCursorWait();
		s_pLoadingFrame = nullptr;
		pView->updateScreen(false);
	}

	pDialogFactory->releaseDialog(pDialog);

	return bOK;
}
#endif

#ifdef ENABLE_PRINT
static bool s_doPrintPreview(FV_View * pView)
{
	UT_return_val_if_fail(pView, false);

	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
	UT_return_val_if_fail(pFrameData, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_PrintPreview * pDialog
		= static_cast<XAP_Dialog_PrintPreview *>(pDialogFactory->requestDialog(XAP_DIALOG_ID_PRINTPREVIEW));
	UT_return_val_if_fail(pDialog, false);
	FL_DocLayout* pLayout = pView->getLayout();
	PD_Document * doc = pLayout->getDocument();

    // Turn on Wait cursor
	pView->setCursorWait();

	pDialog->setPaperSize (pView->getPageSize().getPredefinedName());
	pDialog->setDocumentTitle(pFrame->getNonDecoratedTitle());
	pDialog->setDocumentPathname((!doc->getFilename().empty())
								 ? doc->getFilename().c_str()
								 : pFrame->getNonDecoratedTitle());

	pDialog->runModal(pFrame);

	GR_Graphics * pGraphics = pDialog->getPrinterGraphicsContext();
	if (!(pGraphics && pGraphics->queryProperties(GR_Graphics::DGP_PAPER)))
		{
			UT_ASSERT_HARMLESS(pGraphics);
			UT_ASSERT_HARMLESS(pGraphics->queryProperties(GR_Graphics::DGP_PAPER));
			
			pDialogFactory->releaseDialog(pDialog);
			
			// Turn off wait cursor
			pView->clearCursorWait();

			return false;
		}

	/*
	We need to re-layout the document for now, so the UnixPSGraphics class will
	get it's font list filled. When we find a better way to fill the UnixPSGraphics
	font list, we can remove the 4 lines below. - MARCM
	*/
	FL_DocLayout * pDocLayout = nullptr;
	FV_View * pPrintView = nullptr;
	bool bHideFmtMarks = false;
	bool bDidQuickPrint = false;
	if(!pGraphics->canQuickPrint() || (pView->getViewMode() != VIEW_PRINT))
	{
			pDocLayout = new FL_DocLayout(doc,pGraphics);
			pPrintView = new FV_View(XAP_App::getApp(), nullptr, pDocLayout);
			pPrintView->setViewMode(VIEW_PRINT);
			pPrintView->getLayout()->fillLayouts();
			pPrintView->getLayout()->formatAll();
			pPrintView->getLayout()->recalculateTOCFields();
	}
	else
	{
			pDocLayout = pLayout;
			pPrintView = pView;
			pDocLayout->setQuickPrint(pGraphics);
			bDidQuickPrint = true;
			if(pFrameData->m_bShowPara)
			{
				pPrintView->setShowPara(false);
				bHideFmtMarks = true;
			}
	}
	
	UT_uint32 nFromPage = 1, nToPage = pLayout->countPages(), nCopies = 1;
	bool bCollate  = false;

	// TODO these are here temporarily to make printing work.  We'll fix the hack later.
	// BUGBUG assumes all pages are same size and orientation
	UT_sint32 iWidth = pDocLayout->getWidth();
	UT_sint32 iHeight = pDocLayout->getHeight() / pDocLayout->countPages();

	const char *pDocName = ((!doc->getFilename().empty()) ? doc->getFilename().c_str() : pFrame->getNonDecoratedTitle());

	s_actuallyPrint(doc, pGraphics, pPrintView, pDocName, nCopies, bCollate,
					iWidth,  iHeight, nToPage, nFromPage);

	if(!bDidQuickPrint)
	{
			delete pDocLayout;
			delete pPrintView;
	}
	else
	{
		if(bHideFmtMarks)
			pPrintView->setShowPara(true);

		pDocLayout->setQuickPrint(nullptr);
	}
	pDialog->releasePrinterGraphicsContext(pGraphics);

	pDialogFactory->releaseDialog(pDialog);

    // Turn off wait cursor
	pView->clearCursorWait();
	
	return true;
}
#endif

static bool s_doZoomDlg(FV_View * pView)
{
	UT_return_val_if_fail(pView, false);
	UT_String tmp;
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme *pPrefsScheme = pPrefs->getCurrentScheme();
	UT_return_val_if_fail (pPrefsScheme, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_Zoom * pDialog
		= static_cast<XAP_Dialog_Zoom *>(pDialogFactory->requestDialog(XAP_DIALOG_ID_ZOOM));
	UT_return_val_if_fail(pDialog, false);

	pDialog->setZoomPercent(pFrame->getZoomPercentage());
	pDialog->setZoomType(pFrame->getZoomType());

	pDialog->runModal(pFrame);

	switch (pDialog->getZoomType())
	{
	case XAP_Frame::z_PAGEWIDTH:
		pPrefsScheme->setValue(static_cast<const gchar*>(XAP_PREF_KEY_ZoomType),
				       static_cast<const gchar*>("Width"));
		break;
	case XAP_Frame::z_WHOLEPAGE:
		pPrefsScheme->setValue(static_cast<const gchar*>(XAP_PREF_KEY_ZoomType),
				       static_cast<const gchar*>("Page"));
		break;
	default:
		{
			UT_UTF8String percent = UT_UTF8String_sprintf("%lu", static_cast<unsigned long>(pDialog->getZoomPercent()));
			pPrefsScheme->setValue(static_cast<const gchar*>(XAP_PREF_KEY_ZoomType),
					       static_cast<const gchar*>(percent.utf8_str()));
		}
		break;
	}
	pFrame->setZoomType(pDialog->getZoomType());
	pFrame->quickZoom(pDialog->getZoomPercent());

	// Zoom is instant-apply, no need to worry about processing the
	// OK/cancel state of the dialog.  Just release it.
	pDialogFactory->releaseDialog(pDialog);
	return true;
}

Defun1(zoom100)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme *pPrefsScheme = pPrefs->getCurrentScheme();
	UT_return_val_if_fail (pPrefsScheme, false);
	pPrefsScheme->setValue(static_cast<const gchar*>(XAP_PREF_KEY_ZoomType),
						   static_cast<const gchar*>("100"));

  pFrame->raise();

  UT_uint32 newZoom = 100;
  pFrame->setZoomType( XAP_Frame::z_100 );
  pFrame->quickZoom(newZoom);
  return true;
}

Defun1(zoom200)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
  XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
  UT_return_val_if_fail(pFrame, false);
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme *pPrefsScheme = pPrefs->getCurrentScheme();
	UT_return_val_if_fail (pPrefsScheme, false);
	pPrefsScheme->setValue(static_cast<const gchar*>(XAP_PREF_KEY_ZoomType),
						   static_cast<const gchar*>("200"));

  pFrame->raise();

  UT_uint32 newZoom = 200;
  pFrame->setZoomType( XAP_Frame::z_200 );
  pFrame->quickZoom(newZoom);

  return true;
}

Defun1(zoom50)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
  XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
  UT_return_val_if_fail(pFrame, false);
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme *pPrefsScheme = pPrefs->getCurrentScheme();
UT_return_val_if_fail(pPrefsScheme, false);	pPrefsScheme->setValue(static_cast<const gchar*>(XAP_PREF_KEY_ZoomType),
						   static_cast<const gchar*>("50"));

  pFrame->raise();

  UT_uint32 newZoom = 50;
  pFrame->setZoomType( XAP_Frame::z_PERCENT );
  pFrame->quickZoom(newZoom);

  return true;
}

Defun1(zoom75)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
  XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
  UT_return_val_if_fail(pFrame, false);
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme *pPrefsScheme = pPrefs->getCurrentScheme();
UT_return_val_if_fail(pPrefsScheme, false);	pPrefsScheme->setValue(static_cast<const gchar*>(XAP_PREF_KEY_ZoomType),
						   static_cast<const gchar*>("75"));

  pFrame->raise();

  UT_uint32 newZoom = 75;
  pFrame->setZoomType(	XAP_Frame::z_75 );
  pFrame->quickZoom(newZoom);

  return true;
}

Defun1(zoomWidth)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
  XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
  UT_return_val_if_fail(pFrame, false);
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme *pPrefsScheme = pPrefs->getCurrentScheme();
UT_return_val_if_fail(pPrefsScheme, false);	pPrefsScheme->setValue(static_cast<const gchar*>(XAP_PREF_KEY_ZoomType),
						   static_cast<const gchar*>("Width"));

  pFrame->raise();

  pFrame->setZoomType( XAP_Frame::z_PAGEWIDTH );

  UT_uint32 newZoom = pView->calculateZoomPercentForPageWidth();
  pFrame->quickZoom(newZoom);


  return true;
}

Defun1(zoomWhole)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme *pPrefsScheme = pPrefs->getCurrentScheme();
UT_return_val_if_fail(pPrefsScheme, false);	pPrefsScheme->setValue(static_cast<const gchar*>(XAP_PREF_KEY_ZoomType),
						   static_cast<const gchar*>("Page"));


  pFrame->raise();

  pFrame->setZoomType( XAP_Frame::z_WHOLEPAGE );

  UT_uint32 newZoom = pView->calculateZoomPercentForWholePage();
  pFrame->quickZoom(newZoom);

  return true;
}

Defun1(zoomIn)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	
	pFrame->raise();
	UT_uint32 newZoom = UT_MIN(pFrame->getZoomPercentage() + 10, XAP_DLG_ZOOM_MAXIMUM_ZOOM);
	UT_String tmp (UT_String_sprintf("%d",newZoom));
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme *pPrefsScheme = pPrefs->getCurrentScheme();
UT_return_val_if_fail(pPrefsScheme, false);	pPrefsScheme->setValue(static_cast<const gchar*>(XAP_PREF_KEY_ZoomType),
						 static_cast<const gchar*>(tmp.c_str()));
	
	pFrame->setZoomType( XAP_Frame::z_PERCENT );
	pFrame->quickZoom(newZoom);

	return true;
}

Defun1(zoomOut)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	
	pFrame->raise();
	
	UT_uint32 newZoom = UT_MAX(pFrame->getZoomPercentage() - 10, XAP_DLG_ZOOM_MINIMUM_ZOOM);
	UT_String tmp (UT_String_sprintf("%d",newZoom));
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme *pPrefsScheme = pPrefs->getCurrentScheme();
UT_return_val_if_fail(pPrefsScheme, false);	pPrefsScheme->setValue(static_cast<const gchar*>(XAP_PREF_KEY_ZoomType),
						 static_cast<const gchar*>(tmp.c_str()));
	pFrame->setZoomType( XAP_Frame::z_PERCENT );
	pFrame->quickZoom(newZoom);

	
	return true;
}

static bool s_doBreakDlg(FV_View * pView)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	if(pView->isHdrFtrEdit())
		return false;

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_Break * pDialog
		= static_cast<AP_Dialog_Break *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_BREAK));
	UT_return_val_if_fail(pDialog, false);
	pDialog->runModal(pFrame);

	AP_Dialog_Break::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == AP_Dialog_Break::a_OK);

	if (bOK)
	{
		UT_UCS4Char c;
		switch(pDialog->getBreakType())
		{
		// special cases
		case AP_Dialog_Break::b_PAGE:
			c = UCS_FF;
			pView->cmdCharInsert(&c,1);
			break;
		case AP_Dialog_Break::b_COLUMN:
				c = UCS_VTAB;
			pView->cmdCharInsert(&c,1);
			break;
		case AP_Dialog_Break::b_NEXTPAGE:
				pView->insertSectionBreak(BreakSectionNextPage);
			break;
		case AP_Dialog_Break::b_CONTINUOUS:
				pView->insertSectionBreak(BreakSectionContinuous);
			break;
		case AP_Dialog_Break::b_EVENPAGE:
				pView->insertSectionBreak(BreakSectionEvenPage);
			break;
		case AP_Dialog_Break::b_ODDPAGE:
				pView->insertSectionBreak(BreakSectionOddPage);
			break;
		default:
			UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		}
	}

	pDialogFactory->releaseDialog(pDialog);

	return bOK;
}

#ifdef ENABLE_PRINT
static bool s_doPageSetupDlg (FV_View * pView)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);

	pFrame->raise();
	XAP_DialogFactory * pDialogFactory
	  = static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_PageSetup * pDialog =
	  static_cast<AP_Dialog_PageSetup *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_FILE_PAGESETUP));

	UT_return_val_if_fail(pDialog, false);
	PD_Document * pDoc = pView->getLayout()->getDocument();
	//
	// Need this for the conversion methods
	//
	fp_PageSize::Predefined orig_def,final_def;
	double orig_wid = -1, orig_ht = -1, final_wid = -1, final_ht = -1;
	UT_Dimension orig_ut = DIM_IN, final_ut = DIM_IN;
	fp_PageSize pSize(pDoc->getPageSize()->getPredefinedName());
	orig_def = pSize.NameToPredefined(pSize.getPredefinedName());
	//
	// Set first page of the dialog properties.
	//
	AP_Dialog_PageSetup::Orientation orig_ori,final_ori;
	orig_ori =	AP_Dialog_PageSetup::PORTRAIT;
	if(pDoc->getPageSize()->isPortrait() == false)
	{
		orig_ori = AP_Dialog_PageSetup::LANDSCAPE;
	}
	if (orig_def == fp_PageSize::psCustom)
	{
		orig_ut = pDoc->getPageSize()->getDims();
		orig_wid = pDoc->getPageSize()->Width(orig_ut);
		orig_ht = pDoc->getPageSize()->Height(orig_ut);
		if(orig_ori == AP_Dialog_PageSetup::LANDSCAPE)
		{
			pSize.Set(orig_ht, orig_wid, orig_ut);
		}
		else
		{
			pSize.Set(orig_wid, orig_ht, orig_ut);
		}
	}
	pDialog->setPageSize(pSize);
	pDialog->setPageOrientation(orig_ori);
	UT_Dimension orig_margu,final_margu;
	double orig_scale,final_scale;
	orig_scale = pDoc->getPageSize()->getScale();

	// respect units set in the dialogue constructer from prefs
	UT_Dimension orig_uprefs = DIM_IN;
	std::string rulerUnits;
	if (pApp->getPrefsValue(AP_PREF_KEY_RulerUnits, rulerUnits)) {
		// we only allow in, cm, mm in the dlg
		UT_Dimension units = UT_determineDimension(rulerUnits.c_str());
		if(units == DIM_CM || units == DIM_MM || units == DIM_IN)
		{
			orig_uprefs = units;
		}
	}

	// make sure that the units in the dlg are the same as in the prefs
	pDialog->setPageUnits(orig_uprefs);
	pDialog->setMarginUnits(orig_uprefs);
	
	pDialog->setPageScale(static_cast<int>(100.0*orig_scale));

	//
	// Set the second page of info
	// All the page and header/footer margins
	//
	PP_PropertyVector props_in;
	std::string pszLeftMargin;
	std::string pszTopMargin;
	std::string pszRightMargin;
	std::string pszBottomMargin;
	std::string pszFooterMargin;
	std::string pszHeaderMargin;
	double dLeftMargin = 1.0;
	double dRightMargin=1.0;
	double dTopMargin = 1.0;
	double dBottomMargin = 1.0;
	double dFooterMargin= 0.0;
	double dHeaderMargin = 0.0;

	bool bResult = pView->getSectionFormat(props_in);
	if (!bResult)
	{
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
	}
	if(!props_in.empty())
	{
		pszLeftMargin = PP_getAttribute("page-margin-left", props_in);
		if(!pszLeftMargin.empty())
		{
			dLeftMargin = UT_convertToInches(pszLeftMargin.c_str());
		}

		pszRightMargin = PP_getAttribute("page-margin-right", props_in);
		if(!pszRightMargin.empty())
		{
			dRightMargin = UT_convertToInches(pszRightMargin.c_str());
		}

		pszTopMargin = PP_getAttribute("page-margin-top", props_in);
		if(!pszTopMargin.empty())
		{
			dTopMargin = UT_convertToInches(pszTopMargin.c_str());
		}

		pszBottomMargin = PP_getAttribute("page-margin-bottom", props_in);
		if(!pszBottomMargin.empty())
		{
			dBottomMargin = UT_convertToInches(pszBottomMargin.c_str());
		}

		pszFooterMargin = PP_getAttribute("page-margin-footer", props_in);
		if(!pszFooterMargin.empty())
			dFooterMargin = UT_convertToInches(pszFooterMargin.c_str());

		pszHeaderMargin = PP_getAttribute("page-margin-header", props_in);
		if(!pszHeaderMargin.empty())
			dHeaderMargin = UT_convertToInches(pszHeaderMargin.c_str());
	}

	orig_margu = pDialog->getMarginUnits();
	if(orig_margu == DIM_MM)
	{
		dLeftMargin = dLeftMargin * 25.4;
		dRightMargin = dRightMargin * 25.4;
		dTopMargin = dTopMargin * 25.4;
		dBottomMargin = dBottomMargin * 25.4;
		dFooterMargin = dFooterMargin * 25.4;
		dHeaderMargin = dHeaderMargin * 25.4;
	}
	else if(orig_margu == DIM_CM)
	{
		dLeftMargin = dLeftMargin * 2.54;
		dRightMargin = dRightMargin * 2.54;
		dTopMargin = dTopMargin * 2.54;
		dBottomMargin = dBottomMargin * 2.54;
		dFooterMargin = dFooterMargin * 2.54;
		dHeaderMargin = dHeaderMargin * 2.54;
	}

	//
	// OK set all page two stuff
	//
	// do not set units -- they have not changed
	// pDialog->setMarginUnits(orig_margu);
	pDialog->setMarginTop(static_cast<float>(dTopMargin));
	pDialog->setMarginBottom(static_cast<float>(dBottomMargin));
	pDialog->setMarginLeft(static_cast<float>(dLeftMargin));
	pDialog->setMarginRight(static_cast<float>(dRightMargin));
	pDialog->setMarginHeader(static_cast<float>(dHeaderMargin));
	pDialog->setMarginFooter(static_cast<float>(dFooterMargin));

	pDialog->runModal (pFrame);

	AP_Dialog_PageSetup::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == AP_Dialog_PageSetup::a_OK);

	if(bOK == false)
	{
		delete pDialog;
		return true;
	}

	final_def = pSize.NameToPredefined(pDialog->getPageSize().getPredefinedName());
	final_ori = pDialog->getPageOrientation();
	final_scale = pDialog->getPageScale()/100.0;
	pSize.Set(final_def);

	if (final_def == fp_PageSize::psCustom)
	{
		final_ut = pDialog->getPageSize().getDims();
		final_wid = pDialog->getPageSize().Width(final_ut);
		final_ht = pDialog->getPageSize().Height(final_ut);
	}

	if((final_def != orig_def) || (final_ori != orig_ori) || ((final_scale-orig_scale) > 0.001) || ((final_scale-orig_scale) < -0.001) || (orig_ht != final_ht) || (orig_wid != final_wid) || (orig_ut != final_ut) )
	{
		final_wid = pDialog->getPageSize().Width(final_ut);
		final_ht = pDialog->getPageSize().Height(final_ut);
		//
		// Set the new Page Stuff
		//
		UT_UTF8String sType,sOri,sWidth,sHeight,sUnits,sScale;
		sType = pSize.getPredefinedName();
		sUnits = UT_dimensionName(final_ut);
		sWidth = UT_formatDimensionString(final_ut,final_wid);
		sHeight = UT_formatDimensionString(final_ut,final_ht);
		sScale = UT_formatDimensionString(DIM_none,final_scale);
		const PP_PropertyVector attr = {
			"pagetype", sType.utf8_str(),
			"orientation", (final_ori == AP_Dialog_PageSetup::PORTRAIT) ?
			"portrait" : "landscape",
			"width", sWidth.utf8_str(),
			"height", sHeight.utf8_str(),
			"units", sUnits.utf8_str(),
			"page-scale", sScale.utf8_str()
		};
#if 0 //def DEBUG
		for (const gchar ** a = szAttr; (*a); a++)
			{
				UT_DEBUGMSG(("apEditMethods attrib %s value %s \n",a[0],a[1]));
				a++;
			}
#endif
		pDoc->setPageSizeFromFile(attr);
	}

	// I am not entirely sure about this; perhaps the units should only be modifiable
	// through the prefs dialogue, not through the page setup
	XAP_Prefs * pPrefs = pApp->getPrefs();
	if(!pPrefs)
	{
		UT_ASSERT_HARMLESS(pPrefs);
		DELETEP(pDialog);
		return false;
	}

	XAP_PrefsScheme *pPrefsScheme = pPrefs->getCurrentScheme();
	if(!pPrefsScheme)
	{
		UT_ASSERT_HARMLESS(pPrefsScheme);
		DELETEP(pDialog);
		return false;
	}

	//
	// Recover ppView
	//
	FV_View * ppView = static_cast<FV_View *>(pFrame->getCurrentView());
	//
	// Now gather all the margin properties...
	//

	UT_String szLeftMargin;
	UT_String szTopMargin;
	UT_String szRightMargin;
	UT_String szBottomMargin;
	UT_String szFooterMargin;
	UT_String szHeaderMargin;

	final_margu = pDialog->getMarginUnits();

	pPrefsScheme->setValue(static_cast<const gchar *>(AP_PREF_KEY_RulerUnits),
						   static_cast<const gchar *>(UT_dimensionName(final_margu)));

	dTopMargin = static_cast<double>(pDialog->getMarginTop());
	dBottomMargin = static_cast<double>(pDialog->getMarginBottom());
	dLeftMargin = static_cast<double>(pDialog->getMarginLeft());
	dRightMargin = static_cast<double>(pDialog->getMarginRight());
	dHeaderMargin = static_cast<double>(pDialog->getMarginHeader());
	dFooterMargin = static_cast<double>(pDialog->getMarginFooter());

	//
	// Convert them into const char strings and change the section format
	//
	PP_PropertyVector props;
	//szLeftMargin = UT_convertInchesToDimensionString(docMargUnits,dLeftMargin);
	szLeftMargin = UT_formatDimensionString(final_margu,dLeftMargin);
	props.push_back("page-margin-left");
	props.push_back(szLeftMargin.c_str());

	//szRightMargin = UT_convertInchesToDimensionString(docMargUnits,dRightMargin);
	szRightMargin = UT_formatDimensionString(final_margu,dRightMargin);
	props.push_back("page-margin-right");
	props.push_back(szRightMargin.c_str());

	//szTopMargin = UT_convertInchesToDimensionString(docMargUnits,dTopMargin);
	szTopMargin = UT_formatDimensionString(final_margu,dTopMargin);
	props.push_back("page-margin-top");
	props.push_back(szTopMargin.c_str());

	//szBottomMargin = UT_convertInchesToDimensionString(docMargUnits,dBottomMargin);
	szBottomMargin = UT_formatDimensionString(final_margu,dBottomMargin);
	props.push_back("page-margin-bottom");
	props.push_back(szBottomMargin.c_str());

	//szFooterMargin = UT_convertInchesToDimensionString(docMargUnits,dFooterMargin);
	szFooterMargin = UT_formatDimensionString(final_margu,dFooterMargin);
	props.push_back("page-margin-footer");
	props.push_back(szFooterMargin.c_str());

	//szHeaderMargin = UT_convertInchesToDimensionString(docMargUnits,dHeaderMargin);
	szHeaderMargin = UT_formatDimensionString(final_margu,dHeaderMargin);
	props.push_back("page-margin-header");
	props.push_back(szHeaderMargin.c_str());

	if(ppView->isHdrFtrEdit())
	{
		ppView->clearHdrFtrEdit();
		ppView->warpInsPtToXY(0,0,false);
	}

	//
	// Finally we've got it all in place, Make the change!
	//

	ppView->setSectionFormat(props);
	delete pDialog;
	return true;
}
#endif

/* -------------------------------------------------------------------
 * Layout ribbon commands - direct page-setup operations and the
 * Word-style Document dialog
 * ------------------------------------------------------------------- */

/*!
 * Apply margin values (inches) to every section in the document.
 */
static bool s_applyMarginsAll(FV_View * pView,
							  double dTop, double dBottom,
							  double dLeft, double dRight)
{
	UT_UTF8String sTop = UT_formatDimensionString(DIM_IN, dTop);
	UT_UTF8String sBot = UT_formatDimensionString(DIM_IN, dBottom);
	UT_UTF8String sLeft = UT_formatDimensionString(DIM_IN, dLeft);
	UT_UTF8String sRight = UT_formatDimensionString(DIM_IN, dRight);
	const PP_PropertyVector props = {
		"page-margin-top",    sTop.utf8_str(),
		"page-margin-bottom", sBot.utf8_str(),
		"page-margin-left",   sLeft.utf8_str(),
		"page-margin-right",  sRight.utf8_str()
	};
	return pView->setDocWideSectionFormat(props);
}

/*!
 * Margin presets, Word-style: pCallData carries
 * normal|narrow|moderate|wide|mirrored (values in inches).
 */
Defun(pageMargins)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);

	UT_UTF8String arg(pCallData->m_pData, pCallData->m_dataLength);
	double t = 1.0, b = 1.0, l = 1.0, r = 1.0;
	if (arg == "narrow")            { t = b = l = r = 0.5; }
	else if (arg == "moderate")     { l = r = 0.75; }
	else if (arg == "wide")         { l = r = 2.0; }
	else if (arg == "mirrored")     { l = 1.25; r = 1.0; }
	else if (arg != "normal")       return false;

	return s_applyMarginsAll(pView, t, b, l, r);
}

/*!
 * Push a new paper size / orientation to the document, preserving the
 * scale and (for custom sizes) the dimensions.
 */
static bool s_applyPageSize(FV_View * pView, const char * szPredefined,
							bool bLandscape)
{
	PD_Document * pDoc = pView->getLayout()->getDocument();
	UT_return_val_if_fail(pDoc, false);

	const fp_PageSize * cur = pDoc->getPageSize();
	fp_PageSize::Predefined curDef =
		fp_PageSize::NameToPredefined(cur->getPredefinedName());
	fp_PageSize pSize(szPredefined);
	fp_PageSize::Predefined newDef = pSize.NameToPredefined(
		pSize.getPredefinedName());

	UT_Dimension ut = DIM_IN;
	double wid = -1, ht = -1;
	if (newDef == fp_PageSize::psCustom)
	{
		/* keep the current custom dims, normalised to portrait */
		ut = cur->getDims();
		wid = cur->Width(ut);
		ht = cur->Height(ut);
		if (!cur->isPortrait())
		{
			double tmp = wid;
			wid = ht;
			ht = tmp;
		}
		pSize.Set(wid, ht, ut);
	}
	pSize.setScale(cur->getScale());
	if (bLandscape)
		pSize.setLandscape();

	if (curDef == newDef && (cur->isPortrait() != bLandscape) &&
		newDef != fp_PageSize::psCustom)
		return true; /* already this size + orientation */

	UT_UTF8String sType = pSize.getPredefinedName();
	UT_UTF8String sUnits = UT_dimensionName(ut);
	UT_UTF8String sWidth = UT_formatDimensionString(ut,
		newDef == fp_PageSize::psCustom ? wid : pSize.Width(ut));
	UT_UTF8String sHeight = UT_formatDimensionString(ut,
		newDef == fp_PageSize::psCustom ? ht : pSize.Height(ut));
	UT_UTF8String sScale = UT_formatDimensionString(DIM_none,
													pSize.getScale());
	const PP_PropertyVector attr = {
		"pagetype", sType.utf8_str(),
		"orientation", bLandscape ? "landscape" : "portrait",
		"width", sWidth.utf8_str(),
		"height", sHeight.utf8_str(),
		"units", sUnits.utf8_str(),
		"page-scale", sScale.utf8_str()
	};
	return pDoc->setPageSizeFromFile(attr);
}

/*!
 * pCallData: "portrait" | "landscape"
 */
Defun(pageOrientation)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);

	UT_UTF8String arg(pCallData->m_pData, pCallData->m_dataLength);
	bool bLandscape = (arg == "landscape");
	PD_Document * pDoc = pView->getLayout()->getDocument();
	UT_return_val_if_fail(pDoc, false);
	if (bLandscape == !pDoc->getPageSize()->isPortrait())
		return true; /* already that orientation */
	return s_applyPageSize(pView, pDoc->getPageSize()->getPredefinedName(),
						   bLandscape);
}

/*!
 * pCallData: a predefined fp_PageSize name ("A4", "Letter", ...),
 * optionally followed by "|landscape" for the long-edge variant.
 */
Defun(pageSize)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);

	UT_UTF8String arg(pCallData->m_pData, pCallData->m_dataLength);
	bool bLandscape = false;
	std::string name = arg.utf8_str();
	size_t bar = name.find('|');
	if (bar != std::string::npos)
	{
		bLandscape = (name.substr(bar + 1) == "landscape");
		name.erase(bar);
	}
	else
	{
		PD_Document * pDoc = pView->getLayout()->getDocument();
		UT_return_val_if_fail(pDoc, false);
		bLandscape = !pDoc->getPageSize()->isPortrait();
	}
	if (!fp_PageSize::IsPredefinedName(name.c_str()))
		return false;
	return s_applyPageSize(pView, name.c_str(), bLandscape);
}

/*!
 * pCallData: column count "1".."N" - applies to the current section.
 */
Defun(pageColumns)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);

	UT_UTF8String arg(pCallData->m_pData, pCallData->m_dataLength);
	int n = atoi(arg.utf8_str());
	if (n < 1 || n > 13)
		return false;
	UT_UTF8String sCols = UT_UTF8String_sprintf("%d", n);
	const PP_PropertyVector props = {
		"columns", sCols.utf8_str()
	};
	return pView->setSectionFormat(props);
}

Defun1(insColumnBreak)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_UCS4Char c = UCS_VTAB;
	return pView->cmdCharInsert(&c, 1);
}

/*!
 * pCallData: "next"|"continuous"|"even"|"odd"
 */
Defun(insSectionBreak)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);

	UT_UTF8String arg(pCallData->m_pData, pCallData->m_dataLength);
	BreakSectionType type = BreakSectionNextPage;
	if (arg == "continuous")      type = BreakSectionContinuous;
	else if (arg == "even")       type = BreakSectionEvenPage;
	else if (arg == "odd")        type = BreakSectionOddPage;
	else if (arg != "next")       return false;
	pView->insertSectionBreak(type);
	return true;
}

/*!
 * Apply one block property to the paragraphs under the caret /
 * selection.  pCallData: "prop:value" - used by the Layout ribbon's
 * Indent and Spacing spin fields.
 */
Defun(paraProp)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);

	UT_UTF8String arg(pCallData->m_pData, pCallData->m_dataLength);
	const char * colon = strchr(arg.utf8_str(), ':');
	if (!colon || colon == arg.utf8_str())
		return false;
	std::string prop(arg.utf8_str(), colon - arg.utf8_str());
	std::string val(colon + 1);
	if (val.empty())
		return false;
	const PP_PropertyVector props = {
		prop.c_str(), val.c_str()
	};
	pView->setBlockFormat(props);
	return true;
}

/*!
 * Position / Wrap Text / Align entry point for the Arrange group:
 * opens the positioned-object dialog for the selected image or frame.
 */
Defun(arrangePosition)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	dlgFmtPosImage(pAV_View, pCallData);
	return true;
}

/*!
 * pCallData: wrap-mode value for the selected frame/image
 * (wrapped-both|wrapped-topbot|above-text|below-text)
 */
Defun(wrapObject)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);

	UT_UTF8String arg(pCallData->m_pData, pCallData->m_dataLength);
	const char * mode = arg.utf8_str();
	if (strcmp(mode, "wrapped-both") && strcmp(mode, "wrapped-topbot") &&
		strcmp(mode, "above-text") && strcmp(mode, "below-text"))
		return false;
	const PP_PropertyVector props = {
		"wrap-mode", mode
	};
	pView->setFrameFormat(props);
	return true;
}

/*!
 * Apply "name:value;name:value" section properties to the current
 * section - used by the Layout ribbon's line-numbering choices.
 */
Defun(sectProps)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);

	UT_UTF8String arg(pCallData->m_pData, pCallData->m_dataLength);
	PP_PropertyVector props;
	std::string rest = arg.utf8_str();
	size_t pos = 0;
	while (pos <= rest.size())
	{
		size_t semi = rest.find(';', pos);
		std::string kv = rest.substr(pos, semi == std::string::npos
									 ? std::string::npos : semi - pos);
		size_t colon = kv.find(':');
		if (colon > 0)
		{
			props.push_back(kv.substr(0, colon));
			props.push_back(kv.substr(colon + 1));
		}
		if (semi == std::string::npos)
			break;
		pos = semi + 1;
	}
	if (props.empty())
		return false;
	return pView->setSectionFormat(props);
}

/*!
 * Apply "name:value;name:value" attributes to the document strux -
 * used by the Layout ribbon's hyphenation settings.
 */
Defun(docProps)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);

	PD_Document * pDoc = pView->getLayout()->getDocument();
	UT_return_val_if_fail(pDoc, false);

	UT_UTF8String arg(pCallData->m_pData, pCallData->m_dataLength);
	PP_PropertyVector props;
	std::string rest = arg.utf8_str();
	size_t pos = 0;
	while (pos <= rest.size())
	{
		size_t semi = rest.find(';', pos);
		std::string kv = rest.substr(pos, semi == std::string::npos
									 ? std::string::npos : semi - pos);
		size_t colon = kv.find(':');
		if (colon > 0)
		{
			props.push_back(kv.substr(0, colon));
			props.push_back(kv.substr(colon + 1));
		}
		if (semi == std::string::npos)
			break;
		pos = semi + 1;
	}
	if (props.empty())
		return false;
	pDoc->setAttrProp(props);
	return true;
}

/*!
 * The Word-style Document dialog (Margins / Layout pages).
 */
Defun1(docSettings)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);

	pFrame->raise();
	XAP_DialogFactory * pDialogFactory =
		static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());
	AP_Dialog_Document * pDialog =
		static_cast<AP_Dialog_Document *>(pDialogFactory->requestDialog(
			(XAP_Dialog_Id)AP_DIALOG_ID_DOCUMENT));
	UT_return_val_if_fail(pDialog, false);

	PD_Document * pDoc = pView->getLayout()->getDocument();
	UT_return_val_if_fail(pDoc, false);

	/* units follow the ruler preference (in|cm|mm) */
	UT_Dimension units = DIM_IN;
	std::string rulerUnits;
	if (pApp->getPrefsValue(AP_PREF_KEY_RulerUnits, rulerUnits))
	{
		UT_Dimension u = UT_determineDimension(rulerUnits.c_str());
		if (u == DIM_CM || u == DIM_MM || u == DIM_IN)
			units = u;
	}
	pDialog->setMarginUnits(units);
	pDialog->setPageUnits(units);

	/* current page size / orientation / scale */
	fp_PageSize pSize(pDoc->getPageSize()->getPredefinedName());
	fp_PageSize::Predefined def =
		pSize.NameToPredefined(pSize.getPredefinedName());
	if (def == fp_PageSize::psCustom)
	{
		UT_Dimension u0 = pDoc->getPageSize()->getDims();
		double w = pDoc->getPageSize()->Width(u0);
		double h = pDoc->getPageSize()->Height(u0);
		if (!pDoc->getPageSize()->isPortrait())
			std::swap(w, h);
		pSize.Set(w, h, u0);
	}
	pDialog->setPageSize(pSize);
	pDialog->setPageScale(static_cast<int>(100.0 * pDoc->getPageSize()->getScale()));

	/* current section margins */
	PP_PropertyVector props_in;
	pView->getSectionFormat(props_in);
	double top = 1.0, bot = 1.0, lft = 1.0, rgt = 1.0;
	double hdr = 0.0, ftr = 0.0, gut = 0.0;
	const std::string & sTop = PP_getAttribute("page-margin-top", props_in);
	const std::string & sBot = PP_getAttribute("page-margin-bottom", props_in);
	const std::string & sL = PP_getAttribute("page-margin-left", props_in);
	const std::string & sR = PP_getAttribute("page-margin-right", props_in);
	const std::string & sH = PP_getAttribute("page-margin-header", props_in);
	const std::string & sF = PP_getAttribute("page-margin-footer", props_in);
	const std::string & sG = PP_getAttribute("page-margin-gutter", props_in);
	if (!sTop.empty()) top = UT_convertToInches(sTop.c_str());
	if (!sBot.empty()) bot = UT_convertToInches(sBot.c_str());
	if (!sL.empty())   lft = UT_convertToInches(sL.c_str());
	if (!sR.empty())   rgt = UT_convertToInches(sR.c_str());
	if (!sH.empty())   hdr = UT_convertToInches(sH.c_str());
	if (!sF.empty())   ftr = UT_convertToInches(sF.c_str());
	if (!sG.empty())   gut = UT_convertToInches(sG.c_str());
	double conv = (units == DIM_CM) ? 2.54 : (units == DIM_MM) ? 25.4 : 1.0;
	pDialog->setMarginTop(static_cast<float>(top * conv));
	pDialog->setMarginBottom(static_cast<float>(bot * conv));
	pDialog->setMarginLeft(static_cast<float>(lft * conv));
	pDialog->setMarginRight(static_cast<float>(rgt * conv));
	pDialog->setMarginHeader(static_cast<float>(hdr * conv));
	pDialog->setMarginFooter(static_cast<float>(ftr * conv));
	pDialog->setMarginGutter(static_cast<float>(gut * conv));

	/* headers/footers present in the current section */
	const std::string & sHE = PP_getAttribute("header-even", props_in);
	const std::string & sHF = PP_getAttribute("header-first", props_in);
	pDialog->setDifferentOddEven(!sHE.empty());
	pDialog->setDifferentFirstPage(!sHF.empty());

	pDialog->runModal(pFrame);

	if (pDialog->getAnswer() != AP_Dialog_Document::a_OK)
	{
		delete pDialog;
		return true;
	}

	/* paper size / orientation / scale changed via Page Setup... */
	if (pDialog->getPageSetupChanged())
	{
		const fp_PageSize & fpSz = pDialog->getPageSize();
		UT_Dimension fu = fpSz.getDims();
		double fw = fpSz.Width(fu);
		double fh = fpSz.Height(fu);
		if (!fpSz.isPortrait())
			std::swap(fw, fh);
		UT_UTF8String sT = fpSz.getPredefinedName();
		UT_UTF8String sU = UT_dimensionName(fu);
		UT_UTF8String sW = UT_formatDimensionString(fu, fw);
		UT_UTF8String sH2 = UT_formatDimensionString(fu, fh);
		UT_UTF8String sS = UT_formatDimensionString(DIM_none,
											pDialog->getPageScale() / 100.0);
		const PP_PropertyVector attr = {
			"pagetype", sT.utf8_str(),
			"orientation", fpSz.isPortrait() ? "portrait" : "landscape",
			"width", sW.utf8_str(),
			"height", sH2.utf8_str(),
			"units", sU.utf8_str(),
			"page-scale", sS.utf8_str()
		};
		pDoc->setPageSizeFromFile(attr);
	}

	/* margins + header/footer edge offsets */
	UT_Dimension mu = pDialog->getMarginUnits();
	UT_UTF8String sTop2 = UT_formatDimensionString(mu, pDialog->getMarginTop());
	UT_UTF8String sBot2 = UT_formatDimensionString(mu, pDialog->getMarginBottom());
	UT_UTF8String sL2 = UT_formatDimensionString(mu, pDialog->getMarginLeft());
	UT_UTF8String sR2 = UT_formatDimensionString(mu, pDialog->getMarginRight());
	UT_UTF8String sH2 = UT_formatDimensionString(mu, pDialog->getMarginHeader());
	UT_UTF8String sF2 = UT_formatDimensionString(mu, pDialog->getMarginFooter());
	UT_UTF8String sG2 = UT_formatDimensionString(mu, pDialog->getMarginGutter());
	PP_PropertyVector props = {
		"page-margin-top",    sTop2.utf8_str(),
		"page-margin-bottom", sBot2.utf8_str(),
		"page-margin-left",   sL2.utf8_str(),
		"page-margin-right",  sR2.utf8_str(),
		"page-margin-header", sH2.utf8_str(),
		"page-margin-footer", sF2.utf8_str()
	};
	if (pDialog->getMarginGutter() > 0.0f)
	{
		props.push_back("page-margin-gutter");
		props.push_back(sG2.utf8_str());
		props.push_back("page-gutter-position");
		props.push_back(pDialog->getGutterPosition() ==
						AP_Dialog_Document::GUTTER_TOP ? "top" : "left");
	}
	if (pDialog->getMultiplePages() != AP_Dialog_Document::MULTI_NORMAL)
	{
		static const char * multi[] = { "normal", "mirror-margins",
										"two-per-sheet", "book-fold" };
		props.push_back("section-multiple-pages");
		props.push_back(multi[pDialog->getMultiplePages()]);
	}
	{
		static const char * start[] = { "continuous", "new-page",
										"even-page", "odd-page" };
		props.push_back("section-start");
		props.push_back(start[pDialog->getSectionStart()]);
	}
	{
		static const char * va[] = { "top", "center", "justified", "bottom" };
		props.push_back("section-vertical-align");
		props.push_back(va[pDialog->getVerticalAlign()]);
	}

	if (pDialog->getApplyTo() == AP_Dialog_Document::APPLY_POINT_FORWARD)
	{
		pView->insertSectionBreak(BreakSectionContinuous);
		pView->setSectionFormat(props);
	}
	else if (pDialog->getApplyTo() == AP_Dialog_Document::APPLY_THIS_SECTION)
		pView->setSectionFormat(props);
	else
		pView->setDocWideSectionFormat(props);

	/* header/footer type changes */
	if (pDialog->getDifferentOddEven() != !sHE.empty())
	{
		if (pDialog->getDifferentOddEven())
		{
			pView->createThisHdrFtr(FL_HDRFTR_HEADER_EVEN);
			pView->createThisHdrFtr(FL_HDRFTR_FOOTER_EVEN);
		}
		else
		{
			pView->removeThisHdrFtr(FL_HDRFTR_HEADER_EVEN);
			pView->removeThisHdrFtr(FL_HDRFTR_FOOTER_EVEN);
		}
	}
	if (pDialog->getDifferentFirstPage() != !sHF.empty())
	{
		if (pDialog->getDifferentFirstPage())
		{
			pView->createThisHdrFtr(FL_HDRFTR_HEADER_FIRST);
			pView->createThisHdrFtr(FL_HDRFTR_FOOTER_FIRST);
		}
		else
		{
			pView->removeThisHdrFtr(FL_HDRFTR_HEADER_FIRST);
			pView->removeThisHdrFtr(FL_HDRFTR_FOOTER_FIRST);
		}
	}

	delete pDialog;
	return true;
}

class ABI_EXPORT FV_View_Insert_symbol_listener : public XAP_Insert_symbol_listener
	{
	public:

		virtual void setView( AV_View * pJustFocussedView) override
			{
			p_view = static_cast<FV_View *>(pJustFocussedView) ;
			}
		virtual bool insertSymbol(UT_UCS4Char Char, const char *p_font_name) override
		{
			UT_return_val_if_fail (p_view != nullptr, false);

			p_view->insertSymbol(Char, p_font_name);

			return true;
		}

	private:
		FV_View *p_view;
	};


static	FV_View_Insert_symbol_listener symbol_Listener;

static bool s_InsertSymbolDlg(FV_View * pView, XAP_Dialog_Id id  )
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();
	XAP_DialogFactory * pDialogFactory
	  = static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());

	XAP_Dialog_Insert_Symbol * pDialog
		= static_cast<XAP_Dialog_Insert_Symbol *>(pDialogFactory->requestDialog(id));
UT_return_val_if_fail(pDialog, false);
	if(pDialog->isRunning() == true)
	{
		   pDialog->activate();
	}
		else
	{
		   pDialog->setListener(&symbol_Listener);
		   pDialog->runModeless(pFrame);

	}
	return true;
}

/*****************************************************************/
/*****************************************************************/

Defun1(print)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
#ifdef ENABLE_PRINT
	return s_doPrint(pView,false,false);
#else
    UT_UNUSED(pView);
    return false;
#endif
}


#ifdef ENABLE_PRINT
Defun1(printDirectly)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_doPrint(pView,false,true);
}
#endif


#ifdef ENABLE_PRINT
Defun1(printTB)
{
	CHECK_FRAME;
// print (intended to be from the tool-bar (where we'd like to
	// suppress the dialog if possible))

	ABIWORD_VIEW;
	return s_doPrint(pView,true,false);
}
#endif


#ifdef ENABLE_PRINT
Defun1(printPreview)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_doPrintPreview(pView);
}

Defun1(pageSetup)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_doPageSetupDlg(pView);
}
#endif

Defun1(dlgOptions)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	return s_doOptionsDlg(pView);
}

#ifdef ENABLE_SPELL
Defun1(dlgSpellPrefs)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	

    // spelling tab in Windows in the tab num 2
    // becuase 1, is language selection. For UNIX, it's
    // tab 2 as well. We should use an enumerator instead
    // of fixed values. Jordi,
	return s_doOptionsDlg(pView, 2);
}
#endif

/*****************************************************************/
/*****************************************************************/

/* the array below is a HACK. FIXME */
static const gchar* s_TBPrefsKeys [] = {
#if XAP_SIMPLE_TOOLBAR
	AP_PREF_KEY_SimpleBarVisible,
#else	
	AP_PREF_KEY_StandardBarVisible,
	AP_PREF_KEY_FormatBarVisible,
	AP_PREF_KEY_TableBarVisible,
	AP_PREF_KEY_ExtraBarVisible
#endif		
};

static bool
_viewTBx(AV_View* pAV_View, int num) 
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	AP_FrameData *pFrameData = static_cast<AP_FrameData *> (pFrame->getFrameData());
	UT_return_val_if_fail (pFrameData, false);

	// don't do anything if fullscreen
	if (pFrameData->m_bIsFullScreen)
	  return false;

	// toggle the ruler bit
	pFrameData->m_bShowBar[num] = ! pFrameData->m_bShowBar[num];

	// actually do the dirty work
	pFrame->toggleBar(num, pFrameData->m_bShowBar[num] );

	// POLICY: make this the default for new frames, too
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
	UT_return_val_if_fail (pScheme, false);

	pScheme->setValueBool(s_TBPrefsKeys[num], pFrameData->m_bShowBar[num]);

	//	FV_View * pView = static_cast<FV_View *>(pAV_View);
	//	pView->draw(nullptr);
	return true;
}


Defun1(viewTB1)
{
	CHECK_FRAME;
	return _viewTBx(pAV_View, 0);
}

Defun1(viewTB2)
{
	CHECK_FRAME;
	return _viewTBx(pAV_View, 1);
}

Defun1(viewTB3)
{
	CHECK_FRAME;
	return _viewTBx(pAV_View, 2);
}

Defun1(viewTB4)
{
	CHECK_FRAME;
	return _viewTBx(pAV_View, 3);
}


#if !XAP_SIMPLE_TOOLBAR
Defun1(viewStd)
{
	CHECK_FRAME;
// TODO: Share this function with viewFormat & viewExtra
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
	UT_return_val_if_fail (pFrameData, false);

	// don't do anything if fullscreen
	if (pFrameData->m_bIsFullScreen)
		return false;

	// toggle the ruler bit
	pFrameData->m_bShowBar[0] = ! pFrameData->m_bShowBar[0];

	// actually do the dirty work
	pFrame->toggleBar( 0, pFrameData->m_bShowBar[0] );

	// POLICY: make this the default for new frames, too
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
	UT_return_val_if_fail (pScheme, false);

	pScheme->setValueBool(static_cast<const gchar *>(AP_PREF_KEY_StandardBarVisible), pFrameData->m_bShowBar[0]);
	return true;
}
#endif

#if !XAP_SIMPLE_TOOLBAR
Defun1(viewFormat)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
	UT_return_val_if_fail (pFrameData, false);

	// don't do anything if fullscreen
	if (pFrameData->m_bIsFullScreen)
	  return false;

	// toggle the ruler bit
	pFrameData->m_bShowBar[1] = ! pFrameData->m_bShowBar[1];

	// actually do the dirty work
	pFrame->toggleBar( 1, pFrameData->m_bShowBar[1] );

	// POLICY: make this the default for new frames, too
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
	UT_return_val_if_fail (pScheme, false);

	pScheme->setValueBool(static_cast<const gchar *>(AP_PREF_KEY_FormatBarVisible), pFrameData->m_bShowBar[1]);
	return true;
}
#endif


#if !XAP_SIMPLE_TOOLBAR
Defun1(viewTable)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	AP_FrameData *pFrameData = static_cast<AP_FrameData *> (pFrame->getFrameData());
UT_return_val_if_fail(pFrameData, false);
	// don't do anything if fullscreen
	if (pFrameData->m_bIsFullScreen)
	  return false;

	// toggle the ruler bit
	pFrameData->m_bShowBar[2] = ! pFrameData->m_bShowBar[2];

	// actually do the dirty work
	pFrame->toggleBar( 2, pFrameData->m_bShowBar[2] );

	// POLICY: make this the default for new frames, too
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
	UT_return_val_if_fail (pScheme, false);

	pScheme->setValueBool(static_cast<const gchar *>(AP_PREF_KEY_TableBarVisible), pFrameData->m_bShowBar[2]);
	return true;
}
#endif


#if !XAP_SIMPLE_TOOLBAR
Defun1(viewExtra)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	AP_FrameData *pFrameData = static_cast<AP_FrameData *> (pFrame->getFrameData());
UT_return_val_if_fail(pFrameData, false);
	// don't do anything if fullscreen
	if (pFrameData->m_bIsFullScreen)
	  return false;

	// toggle the ruler bit
	pFrameData->m_bShowBar[3] = ! pFrameData->m_bShowBar[3];

	// actually do the dirty work
	pFrame->toggleBar( 3, pFrameData->m_bShowBar[3] );

	// POLICY: make this the default for new frames, too
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
UT_return_val_if_fail(pScheme, false);

	pScheme->setValueBool(static_cast<const gchar *>(AP_PREF_KEY_ExtraBarVisible), pFrameData->m_bShowBar[3]);
	
	return true;
}
#endif

Defun1(lockToolbarLayout)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);

	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);

	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
UT_return_val_if_fail(pScheme, false);

	return true;
}

Defun1(defaultToolbarLayout)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);

	AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
UT_return_val_if_fail(pFrameData, false);
	// don't do anything if fullscreen
	if (pFrameData->m_bIsFullScreen)
	  return false;

	// we don't want to change their visibility, just the layout
	pFrame->toggleBar(0, pFrameData->m_bShowBar[0]);
	pFrame->toggleBar(1, pFrameData->m_bShowBar[1]);
	pFrame->toggleBar(2, pFrameData->m_bShowBar[2]);
	pFrame->toggleBar(3, pFrameData->m_bShowBar[3]);

	return true;
}

Defun1(viewNormalLayout)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	FV_View * pView = static_cast<FV_View *>(pAV_View);
	if(pView->isHdrFtrEdit())
	{
		pView->clearHdrFtrEdit();
		pView->warpInsPtToXY(0,0,false);
	}

	AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
UT_return_val_if_fail(pFrameData, false);
	pFrameData->m_pViewMode = VIEW_NORMAL;
	pFrame->toggleLeftRuler (false);
	if(!pFrameData->m_bIsFullScreen)
		pFrame->toggleTopRuler (true);

	pView->setViewMode (VIEW_NORMAL);

	// POLICY: make this the default for new frames, too
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
UT_return_val_if_fail(pScheme, false);
	pScheme->setValue(AP_PREF_KEY_LayoutMode, "2");

	pView->updateScreen(false);
	//pView->notifyListeners(AV_CHG_ALL);

	if (pFrame->getZoomType() == pFrame->z_PAGEWIDTH || pFrame->getZoomType() == pFrame->z_WHOLEPAGE)
		pFrame->updateZoom();

	return true;
}


Defun1(viewWebLayout)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
UT_return_val_if_fail(pFrameData, false);
	pFrameData->m_pViewMode = VIEW_WEB;
	pFrame->toggleLeftRuler (false);
	pFrame->toggleTopRuler (false);
	//
	// This about this. we need to work out a page width for 100% zoom
	//
	//pFrame->setZoomType(XAP_Frame::z_PAGEWIDTH);

	FV_View * pView = static_cast<FV_View *>(pAV_View);
	pView->setViewMode (VIEW_WEB);

	// POLICY: make this the default for new frames, too
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
UT_return_val_if_fail(pScheme, false);
	pScheme->setValue(AP_PREF_KEY_LayoutMode, "3");

	pView->updateScreen(false);
	//pView->notifyListeners(AV_CHG_ALL);

	if (pFrame->getZoomType() == pFrame->z_PAGEWIDTH || pFrame->getZoomType() == pFrame->z_WHOLEPAGE)
		pFrame->updateZoom();

	return true;
}

Defun1(viewPrintLayout)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
UT_return_val_if_fail(pFrameData, false);
	pFrameData->m_pViewMode = VIEW_PRINT;
	pFrame->toggleLeftRuler (true && (pFrameData->m_bShowRuler) &&
				 (!pFrameData->m_bIsFullScreen));
	if(!pFrameData->m_bIsFullScreen)
		pFrame->toggleTopRuler (true);

	FV_View * pView = static_cast<FV_View *>(pAV_View);
	UT_DEBUGMSG(("Set mode VIEW PRINT \n"));
	pView->setViewMode (VIEW_PRINT);

	// POLICY: make this the default for new frames, too
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
UT_return_val_if_fail(pScheme, false);
	pScheme->setValue(AP_PREF_KEY_LayoutMode, "1");

	//pView->notifyListeners(AV_CHG_ALL);

	if (pFrame->getZoomType() == pFrame->z_PAGEWIDTH || pFrame->getZoomType() == pFrame->z_WHOLEPAGE)
		pFrame->updateZoom();
	pView->updateScreen(false);

	return true;
}

Defun1(viewStatus)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	AP_FrameData *pFrameData = static_cast<AP_FrameData *> (pFrame->getFrameData());
UT_return_val_if_fail(pFrameData, false);	// don't do anything if fullscreen
	if (pFrameData->m_bIsFullScreen)
	  return false;


	// toggle the view status bit
	pFrameData->m_bShowStatusBar = ! pFrameData->m_bShowStatusBar;

	// actually do the dirty work
	pFrame->toggleStatusBar(pFrameData->m_bShowStatusBar);

	// POLICY: make this the default for new frames, too
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
UT_return_val_if_fail(pScheme, false);
	pScheme->setValueBool(static_cast<const gchar *>(AP_PREF_KEY_StatusBarVisible), pFrameData->m_bShowStatusBar);
	return true;
}

Defun1(viewRuler)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
	UT_return_val_if_fail(pFrameData, false);
	// don't do anything if fullscreen
	if (pFrameData->m_bIsFullScreen)
	  return false;

	// toggle the ruler bit
	pFrameData->m_bShowRuler = ! pFrameData->m_bShowRuler;

	// actually do the dirty work
	pFrame->toggleRuler(pFrameData->m_bShowRuler);

	// POLICY: make this the default for new frames, too
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
UT_return_val_if_fail(pScheme, false);	pScheme->setValueBool(static_cast<const gchar *>(AP_PREF_KEY_RulerVisible), pFrameData->m_bShowRuler);

	return true;
}

Defun1(viewFullScreen)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
	UT_return_val_if_fail(pFrameData, false);


	if(!pFrameData->m_bIsFullScreen) // we're hiding stuff
	{
		pFrameData->m_bIsFullScreen = true;
		for (UT_uint32 i = 0; i < 20; i++) // should never have more than 20 toolbars
		{
			if (!pFrame->getToolbar(i))
				break;

			if (pFrameData->m_bShowBar[i])
				pFrame->toggleBar(i, false);
		}
		if (pFrameData->m_bShowStatusBar)
			pFrame->toggleStatusBar(false);
		if (pFrameData->m_bShowRuler)
			pFrame->toggleRuler(false);
		pFrame->setFullScreen(true);
	}
	else // we're (possibly) unhiding stuff
	{
		if (pFrameData->m_bShowRuler)
			pFrame->toggleRuler(pFrameData->m_bShowRuler);
		if (pFrameData->m_bShowStatusBar)
			pFrame->toggleStatusBar(pFrameData->m_bShowStatusBar);
		for (UT_uint32 i = 0; i < 4; i++)
		{
			if (!pFrame->getToolbar(i))
				break;

			if (pFrameData->m_bShowBar[i])
				pFrame->toggleBar(i, true);
		}
		pFrameData->m_bIsFullScreen = false;
		pFrame->setFullScreen(false);
	}

	// Recalculate the layout after entering/leaving fullscreen
	pFrame->queue_resize();
	return true;
}

Defun1(viewGridlines)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_FrameImpl * pImpl = pFrame->getFrameImpl();
	UT_return_val_if_fail(pImpl, false);
	pImpl->toggleGridlines();
	return true;
}

Defun1(viewNavPane)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_FrameImpl * pImpl = pFrame->getFrameImpl();
	UT_return_val_if_fail(pImpl, false);
	pImpl->toggleNavPane();
	return true;
}

Defun1(viewSplit)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_FrameImpl * pImpl = pFrame->getFrameImpl();
	UT_return_val_if_fail(pImpl, false);
	pImpl->toggleSplitView();
	return true;
}

Defun1(arrangeAll)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_FrameImpl * pImpl = pFrame->getFrameImpl();
	UT_return_val_if_fail(pImpl, false);
	if (!pImpl->arrangeAllWindows())
	{
		pFrame->showMessageBox(AP_STRING_ID_MSG_ArrangeUnsupported,
							   XAP_Dialog_MessageBox::b_O,
							   XAP_Dialog_MessageBox::a_OK);
	}
	return true;
}

Defun1(viewPara)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
UT_return_val_if_fail(pFrameData, false);
	pFrameData->m_bShowPara = !pFrameData->m_bShowPara;

	ABIWORD_VIEW;
	pView->setShowPara(pFrameData->m_bShowPara);

	// POLICY: make this the default for new frames, too
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
UT_return_val_if_fail(pScheme, false);
	pScheme->setValueBool(AP_PREF_KEY_ParaVisible, pFrameData->m_bShowPara);
	pView->notifyListeners(AV_CHG_ALL);

	return true;
}

Defun1(viewHeadFoot)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	// TODO: synch this implementation with ap_GetState_View
	s_TellNotImplemented(pFrame, "View Headers and Footers", __LINE__);
	return true;
}

Defun1(viewLockStyles)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->getDocument()->lockStyles( !pView->getDocument()->areStylesLocked() );
	pView->notifyListeners(AV_CHG_ALL);
 	return true;
}

Defun(zoom)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme *pPrefsScheme = pPrefs->getCurrentScheme();
UT_return_val_if_fail(pPrefsScheme, false);
	UT_uint32 iZoom = 0;
	
	UT_UTF8String utf8(pCallData->m_pData, pCallData->m_dataLength);
	const gchar *p_zoom = reinterpret_cast<const gchar *>(utf8.utf8_str());

	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();

	std::string sPageWidth;
	pSS->getValueUTF8(XAP_STRING_ID_TB_Zoom_PageWidth,sPageWidth);
	
	std::string sWholePage;
	pSS->getValueUTF8(XAP_STRING_ID_TB_Zoom_WholePage,sWholePage);
	
	std::string sPercent;
	pSS->getValueUTF8(XAP_STRING_ID_TB_Zoom_Percent,sPercent);
	
	if(strcmp(p_zoom, sPageWidth.c_str()) == 0)
	{
		pPrefsScheme->setValue(static_cast<const gchar*>(XAP_PREF_KEY_ZoomType),
						 static_cast<const gchar*>("Width"));
		pFrame->setZoomType(XAP_Frame::z_PAGEWIDTH);
		iZoom = pView->calculateZoomPercentForPageWidth();
	}
	else if(strcmp(p_zoom, sWholePage.c_str()) == 0)
	{
		pFrame->setZoomType(XAP_Frame::z_WHOLEPAGE);
		pPrefsScheme->setValue(static_cast<const gchar*>(XAP_PREF_KEY_ZoomType),
						 static_cast<const gchar*>("Page"));
		iZoom = pView->calculateZoomPercentForWholePage();
	}
	else if(strcmp(p_zoom, sPercent.c_str()) == 0)
	{
		// invoke the zoom dialog instead for some custom value
		return EX(dlgZoom);
	}
	else
	{
		// we've gotten back a number - turn it into a zoom percentage
		//UT_UTF8String tmp (UT_UTF8String_sprintf("%d",p_zoom))
		pPrefsScheme->setValue(static_cast<const gchar*>(XAP_PREF_KEY_ZoomType),
						 static_cast<const gchar*>(utf8.utf8_str()));		
		pFrame->setZoomType(XAP_Frame::z_PERCENT);
		iZoom = atoi(p_zoom);
	}
	  
	UT_return_val_if_fail (iZoom > 0, false);
	pFrame->quickZoom(iZoom);

//
// Make damn sure the cursor is ON!!
//
	FV_View * pAbiView = static_cast<FV_View *>(pFrame->getCurrentView());
	pAbiView->focusChange(AV_FOCUS_HERE);

	return true;
}

Defun1(dlgZoom)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_doZoomDlg(pView);
}

static bool s_doInsertDateTime(FV_View * pView)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_Insert_DateTime * pDialog
		= static_cast<AP_Dialog_Insert_DateTime *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_INSERT_DATETIME));
UT_return_val_if_fail(pDialog, false);
	pDialog->runModal(pFrame);

	if (pDialog->getAnswer() == AP_Dialog_Insert_DateTime::a_OK)
	{
		time_t	tim = time(nullptr);
		struct tm *pTime = localtime(&tim);
		UT_UCS4Char *CurrentDateTime = nullptr;
		char szCurrentDateTime[CURRENT_DATE_TIME_SIZE];

		strftime(szCurrentDateTime,CURRENT_DATE_TIME_SIZE,pDialog->GetDateTimeFormat(),pTime);
		UT_UCS4_cloneString_char(&CurrentDateTime,szCurrentDateTime);
		pView->cmdCharInsert(CurrentDateTime,UT_UCS4_strlen(CurrentDateTime), true);
		FREEP(CurrentDateTime);
	}

	pDialogFactory->releaseDialog(pDialog);

	return true;
}

Defun1(insDateTime)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_doInsertDateTime(pView);
}

/*****************************************************************/
/*****************************************************************/

Defun1(insBreak)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	if(pView->isInTable(pView->getPoint()-1) && pView->isInTable())
	{
		XAP_Frame * pFrame = static_cast<XAP_Frame *> (pAV_View->getParentData());
		pFrame->showMessageBox(AP_STRING_ID_MSG_NoBreakInsideTable,
							   XAP_Dialog_MessageBox::b_O,
							   XAP_Dialog_MessageBox::a_OK);
		return true;
	}
	return s_doBreakDlg(pView);
}

static bool s_doInsertPageNumbers(FV_View * pView)
{
	UT_return_val_if_fail(pView,false);

	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_PageNumbers * pDialog
		= static_cast<AP_Dialog_PageNumbers *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_PAGE_NUMBERS));
UT_return_val_if_fail(pDialog, false);
	pDialog->runModal(pFrame);

	if (pDialog->getAnswer() != AP_Dialog_PageNumbers::a_OK)
	{
		pDialogFactory->releaseDialog(pDialog);
		return true;
	}
	PP_PropertyVector atts = {
		"text-align", ""
	};
	switch (pDialog->getAlignment())
	{
		case AP_Dialog_PageNumbers::id_RALIGN :
			atts[1] = "right";
			break;
		case AP_Dialog_PageNumbers::id_LALIGN :
			atts[1] = "left";
			break;
		case AP_Dialog_PageNumbers::id_CALIGN :
			atts[1] = "center";
			break;
		default:
			UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
			break;
	}
	pView->processPageNumber(pDialog->isFooter() ?
								  FL_HDRFTR_FOOTER : FL_HDRFTR_HEADER,
							 atts);
	pDialogFactory->releaseDialog(pDialog);
	return true;
}

Defun1(insPageNo)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_doInsertPageNumbers(pView);
}

static bool s_doField(FV_View * pView)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_Field * pDialog
		= static_cast<AP_Dialog_Field *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_FIELD));
UT_return_val_if_fail(pDialog, false);
	pDialog->runModal(pFrame);

	if (pDialog->getAnswer() == AP_Dialog_Field::a_OK)
	{
		const gchar * pParam = pDialog->getParameter();

		if(pParam) {
			PP_PropertyVector pAttr = {
				"param", pParam
			};
			pView->cmdInsertField(pDialog->GetFieldFormat(), pAttr);
		}
		else
			pView->cmdInsertField(pDialog->GetFieldFormat());
	}

	pDialogFactory->releaseDialog(pDialog);

	return true;
}

Defun1(insField)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_doField(pView);
}


/* shared body of insFile: load the chosen file into a
 * throwaway document and copy its whole contents into the current
 * view at the point, honoring formatting */
static bool s_insertFileIntoView(FV_View * pView, XAP_Frame * pFrame,
								 const char * pathName)
{
	UT_DEBUGMSG(("DOM: insertFile %s\n", pathName));

	PD_Document * newDoc = new PD_Document();
	UT_Error err = newDoc->readFromFile(pathName, IEFT_Unknown);

	if (!UT_IS_IE_SUCCESS(err))
	{
		UNREFP(newDoc);
		s_CouldNotLoadFileMessage(pFrame, pathName, err);
		return false;
	}
	if ( err == UT_IE_TRY_RECOVER )
	{
		s_CouldNotLoadFileMessage(pFrame, pathName, err);
	}

	// we'll share the same graphics context, which won't matter because
	// we only use it to get font metrics and stuff and not actually draw
	GR_Graphics *pGraphics = pView->getGraphics();

	// create a new layout and view object for the doc
	FL_DocLayout *pDocLayout = new FL_DocLayout(newDoc,pGraphics);
	FV_View copyView(XAP_App::getApp(), nullptr, pDocLayout);

	pDocLayout->setView (&copyView);
	pDocLayout->fillLayouts();

	copyView.cmdSelect(0, 0, FV_DOCPOS_BOD, FV_DOCPOS_EOD); // select all the contents of the new doc
	copyView.cmdCopy(); // copy the contents of the new document
	pView->cmdPaste ( true ); // paste the contents into the existing document honoring the formatting

	DELETEP(pDocLayout);
	UNREFP(newDoc);
	return true;
}

Defun1(insFile)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	IEFileType fType = IEFT_Unknown;
	char *pathName = nullptr;

	if (s_AskForPathname (pFrame, false, XAP_DIALOG_ID_INSERT_FILE,
			      nullptr, &pathName, &fType))
	{
		return s_insertFileIntoView(pView, pFrame, pathName);
	}

	return false;
}

/* Insert tab "Screenshot": captures an area of the screen with
 * gnome-screenshot and inserts the PNG at the point */
Defun1(insScreenshot)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	gchar * shot = g_find_program_in_path("gnome-screenshot");
	if (!shot)
	{
		pFrame->showMessageBox(
			"Screenshot capture needs the gnome-screenshot tool, "
			"which was not found on this system.",
			XAP_Dialog_MessageBox::b_O, XAP_Dialog_MessageBox::a_OK);
		return false;
	}

	gchar * tmp = g_build_filename(g_get_tmp_dir(),
								   "abinova-screenshot.png", nullptr);
	gchar * cmd = g_strdup_printf("%s -a -f \"%s\"", shot, tmp);
	gint status = 0;
	gboolean ok = g_spawn_command_line_sync(cmd, nullptr, nullptr,
											&status, nullptr);
	g_free(cmd);
	g_free(shot);

	bool bOK = ok && status == 0 &&
		g_file_test(tmp, G_FILE_TEST_IS_REGULAR);
	if (bOK)
	{
		FG_ConstGraphicPtr pFG;
		UT_Error errorCode = IE_ImpGraphic::loadGraphic(tmp, IEGFT_PNG, pFG);
		if (errorCode == UT_OK && pFG)
			errorCode = pView->cmdInsertGraphic(pFG);
		if (errorCode != UT_OK)
		{
			s_CouldNotLoadFileMessage(pFrame, tmp, errorCode);
			bOK = false;
		}
	}
	remove(tmp);
	g_free(tmp);
	return bOK;
}

Defun1(insSymbol)
{
	CHECK_FRAME;

	ABIWORD_VIEW;
	XAP_Dialog_Id id = XAP_DIALOG_ID_INSERT_SYMBOL;

	return s_InsertSymbolDlg(pView,id);
}

Defun1(insTextBox)
{
	CHECK_FRAME;

	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	static_cast<FV_View *>(pView)->getFrameEdit()->setMode(FV_FrameEdit_WAIT_FOR_FIRST_CLICK_INSERT);
	static_cast<FV_View *>(pView)->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_CROSSHAIR);
	return true;
}

/* Word's "Draw Vertical Text Box": same drag-to-draw insert but the
 * frame is created rotated 90 degrees */
Defun1(insVerticalTextBox)
{
	CHECK_FRAME;

	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	FV_View * pFV = static_cast<FV_View *>(pView);
	pFV->getFrameEdit()->setVerticalTextBox(true);
	pFV->getFrameEdit()->setMode(FV_FrameEdit_WAIT_FOR_FIRST_CLICK_INSERT);
	pFV->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_CROSSHAIR);
	return true;
}

/* Media popover "Video/Audio from File": Abinova cannot embed a
 * playable media object, so the file is linked like Word's
 * "Insert > Link to File" - clicking the link opens it in the
 * system's media player */
Defun1(insMediaFile)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	IEFileType fType = IEFT_Unknown;
	char *pathName = nullptr;
	if (!s_AskForPathname(pFrame, false, XAP_DIALOG_ID_INSERT_FILE,
						  nullptr, &pathName, &fType) || !pathName)
		return false;

	UT_String url("file://");
	url += pathName;
	const char * base = strrchr(pathName, '/');
	base = base ? base + 1 : pathName;
	/* insert the filename as linked text */
	UT_UCS4String s(base);
	pView->cmdCharInsert(s.ucs4_str(), s.length());
	PT_DocPosition end = pView->getPoint();
	PT_DocPosition start = end - s.length();
	pView->cmdSelect(start, end);
	pView->cmdInsertHyperlink(url.c_str(), base);
	pView->cmdUnselectSelection();
	pView->setPoint(end);
	FREEP(pathName);
	return true;
}

Defun1(insFootnote)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->insertFootnote(true);
}


//
// Word-style "New comment": insert an empty comment anchored at the
// selection (or caret) and move the caret inside it so the user can
// type immediately. No dialog.
//
Defun1(insAnnotation)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);

	UT_DEBUGMSG(("insAnnotation: inserting comment\n"));
	if (!pView->cmdInsertComment())
		return false;

	// like Word: the reviewing pane opens as soon as a comment exists
	XAP_Frame * pFrame =
		static_cast<XAP_Frame *>(pAV_View->getParentData());
	if (pFrame && pFrame->getFrameImpl())
		pFrame->getFrameImpl()->setCommentsPaneVisible(true);
	return true;
}


Defun1(insAnnotationFromSel)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);

	UT_DEBUGMSG(("insAnnotationFromSel: inserting comment\n"));
	if (!pView->cmdInsertComment())
		return false;

	XAP_Frame * pFrame =
		static_cast<XAP_Frame *>(pAV_View->getParentData());
	if (pFrame && pFrame->getFrameImpl())
		pFrame->getFrameImpl()->setCommentsPaneVisible(true);
	return true;
}


// Reviewing pane (Word: Review > Reviewing Pane)
Defun1(commentsPane)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame =
		static_cast<XAP_Frame *>(pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_FrameImpl * pImpl = pFrame->getFrameImpl();
	UT_return_val_if_fail(pImpl, false);
	pImpl->toggleCommentsPane();
	return true;
}


Defun1(nextComment)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->nextComment(true);
}


Defun1(prevComment)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->nextComment(false);
}


Defun1(delAnnotation)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->delAnnotation();
}


Defun1(delAllAnnotations)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->delAllAnnotations();
}


Defun1(resolveAnnotation)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->resolveAnnotation();
}

Defun1(toggleDisplayAnnotations)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	
	//
	// Set the preference to enable annotations display
	//
	XAP_Prefs * pPrefs = XAP_App::getApp()->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
	UT_return_val_if_fail(pScheme, false);
	bool b = false;
	pScheme->getValueBool(AP_PREF_KEY_DisplayAnnotations, b);
	b = !b;
	UT_DEBUGMSG(("toggleDisplayAnnotations: Changing annotation display to %s\n",(b ? "true" : "false")));
	gchar szBuffer[2] = {0,0};
	szBuffer[0] = ((b)==true ? '1' : '0');
	pScheme->setValue(AP_PREF_KEY_DisplayAnnotations, szBuffer);

	// only the in-document (contextual) display is toggled here; the
	// reviewing pane has its own control (commentsPane), matching the
	// Show Comments split in Word's Review tab.  The layout polls the
	// pref and reformats on its own.
	return true ;
}

/*!
    Toggle the "AutoGrammarCheck" preference; the layout polls the
    pref and starts/stops the background grammar check (and clears
    the squiggles) on its own.
*/
Defun1(toggleAutoGrammar)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);

	XAP_Prefs * pPrefs = XAP_App::getApp()->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
	UT_return_val_if_fail(pScheme, false);
	bool b = false;
	pScheme->getValueBool(AP_PREF_KEY_AutoGrammarCheck, b);
	b = !b;
	gchar szBuffer[2] = {0,0};
	szBuffer[0] = ((b)==true ? '1' : '0');
	pScheme->setValue(AP_PREF_KEY_AutoGrammarCheck, szBuffer);
	return true ;
}

Defun1(editAnnotation)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	UT_DEBUGMSG(("editAnnotation\n"));

	fp_AnnotationRun * pA = static_cast<fp_AnnotationRun *>(pView->getHyperLinkRun(pView->getPoint()));
	UT_ASSERT(pA);
	
	pView->cmdEditAnnotationWithDialog(pA->getPID());
	return true;
}

Defun1(insTOC)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->cmdInsertTOC();
	return true;
}


Defun1(insEndnote)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->insertFootnote(false);
	return true;
}

Defun1(toggleRDFAnchorHighlight)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	
	//
	// Set the preference to enable annotations display
	//
	XAP_Prefs * pPrefs = XAP_App::getApp()->getPrefs();
	UT_return_val_if_fail(pPrefs, false);
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
	UT_return_val_if_fail(pScheme, false);
	bool b = false;
	pScheme->getValueBool(AP_PREF_KEY_DisplayRDFAnchors, b);
	b = !b;
	UT_DEBUGMSG(("toggleRDFAnchorHighlight: Changing annotation display to %s\n",(b ? "true" : "false")));
	gchar szBuffer[2] = {0,0};
	szBuffer[0] = ((b)==true ? '1' : '0');
	pScheme->setValue(AP_PREF_KEY_DisplayRDFAnchors, szBuffer);
	return true ;
}


static bool s_doRDFQueryDlg( FV_View * pView, XAP_Dialog_Id id, AP_Dialog_RDFQuery*& dialogret )
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	// kill the annotation preview popup if needed
	if(pView->isAnnotationPreviewActive())
		pView->killAnnotationPreview();
	
	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());

	AP_Dialog_RDFQuery * pDialog
		= static_cast<AP_Dialog_RDFQuery *>(pDialogFactory->requestDialog(id));
	UT_return_val_if_fail(pDialog, false);
	dialogret = pDialog;

	if(pDialog->isRunning() == true)
	{
		pDialog->activate();
	}
	else
	{
		pDialog->setView(pView);
		pDialog->runModeless(pFrame);
	}
	return true;
}

Defun1(rdfQuery)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	XAP_Dialog_Id id = (XAP_Dialog_Id)AP_DIALOG_ID_RDF_QUERY;
	AP_Dialog_RDFQuery* dialog = nullptr;
	return s_doRDFQueryDlg( pView, id, dialog );
}


static bool s_doRDFEditorDlg( FV_View * pView, XAP_Dialog_Id id, AP_Dialog_RDFEditor*& dialogret, bool rstrct )
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	// kill the annotation preview popup if needed
	if(pView->isAnnotationPreviewActive())
		pView->killAnnotationPreview();
	
	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());

	AP_Dialog_RDFEditor * pDialog
		= static_cast<AP_Dialog_RDFEditor *>(pDialogFactory->requestDialog(id));
	UT_return_val_if_fail(pDialog, false);
	dialogret = pDialog;

	pDialog->hideRestrictionXMLID( !rstrct );
	
	
	if(pDialog->isRunning() == true)
	{
		pDialog->activate();
	}
	else
	{
		pDialog->setView(pView);
		pDialog->runModeless(pFrame);
	}
	return true;
}


Defun1(rdfEditor)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	XAP_Dialog_Id id = (XAP_Dialog_Id)AP_DIALOG_ID_RDF_EDITOR;
	AP_Dialog_RDFEditor* dialog = nullptr;
	return s_doRDFEditorDlg( pView, id, dialog, false );
}

Defun1(rdfInsertNewContact)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	if( PD_Document * pDoc = pView->getDocument() )
	{
		if( PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF() )
		{
			std::string objname;
			const XAP_StringSet *pSS = XAP_App::getApp()->getStringSet();
			pSS->getValueUTF8(AP_STRING_ID_DLG_RDF_Insert_NewContact, objname);
			PD_RDFSemanticItemHandle obj = PD_RDFSemanticItem::createSemanticItem( rdf, "Contact" );
			obj->setName( objname );
			/*std::pair< PT_DocPosition, PT_DocPosition > range =*/
			obj->insert( pView );
			obj->showEditorWindow( obj );
		}
	}
	return 0;
}

Defun1(rdfInsertNewContactFromFile)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	if( PD_Document * pDoc = pView->getDocument() )
	{
		if( PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF() )
		{
			std::string objname;
			const XAP_StringSet *pSS = XAP_App::getApp()->getStringSet();
			pSS->getValueUTF8(AP_STRING_ID_DLG_RDF_Insert_NewContact, objname);
			PD_RDFSemanticItemHandle obj = PD_RDFSemanticItem::createSemanticItem( rdf, "Contact" );
			obj->setName( objname );
			obj->importFromFile();
		}
	}
	return 0;
}

Defun1(rdfInsertRef)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	if( PD_Document * pDoc = pView->getDocument() )
	{
		if( PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF() )
		{
			runInsertReferenceDialog( pView );
		}
	}
	return 0;
}



Defun1(rdfQueryXMLIDs)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	AP_Dialog_RDFQuery* dialog = nullptr;
	XAP_Dialog_Id id = (XAP_Dialog_Id)AP_DIALOG_ID_RDF_QUERY;

	bool rc = s_doRDFQueryDlg( pView, id, dialog );
	if( dialog )
	{
		std::string sparql;
		PT_DocPosition point = pView->getPoint();
		UT_DEBUGMSG(("point is at:%d\n", point ));

		if( PD_Document * pDoc = pView->getDocument() )
		{
			if( PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF() )
			{
				std::set< std::string > xmlids;
				rdf->addRelevantIDsForPosition( xmlids, point );
				UT_DEBUGMSG(("xmlids.sz:%lu\n", (long unsigned)xmlids.size() ));

				sparql = PD_DocumentRDF::getSPARQL_LimitedToXMLIDList( xmlids );
			}
		}
		
		dialog->executeQuery( sparql );
	}
	return rc;
}

/*****************************************************************/
/*****************************************************************/

Defun1(dlgParagraph)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView, false);
	if (pView->getDocument()->areStylesLocked())
		return true;

	return s_doParagraphDlg(pView);
}

static bool s_doBullets(FV_View *pView)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());
	AP_Dialog_Lists * pDialog
		= static_cast<AP_Dialog_Lists *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_LISTS));
UT_return_val_if_fail(pDialog, false);
	if(pDialog->isRunning() == true)
	{
		pDialog->activate();
	}
	else
	{
		pDialog->runModeless(pFrame);
	}
	return true;
}


Defun1(dlgBullets)
{
	CHECK_FRAME;
//
  // Dialog for Bullets and Lists
  //
#if defined(TARGET_OS_MAC)
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	s_TellNotImplemented(pFrame, "Lists dialog", __LINE__);
	return true;
#else // enable for GTK+ & Gnome builds only
	ABIWORD_VIEW;
	return s_doBullets(pView);
#endif
}

/***********************************************************************************/

static bool s_doBorderShadingDlg(FV_View * pView)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());

	AP_Dialog_Border_Shading * pDialog
		= static_cast<AP_Dialog_Border_Shading *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_BORDER_SHADING));
	UT_return_val_if_fail(pDialog, false);
	if(!pView->isInTable(pView->getPoint()))
	{
		pView->setPoint(pView->getSelectionAnchor());
	}
	if(pDialog->isRunning() == true)
	{
		pDialog->activate();
	}
	else
	{
		pDialog->runModeless(pFrame);
	}
	return true;
}

Defun1(dlgBorders)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	s_doBorderShadingDlg(pView);

	return true;
}

Defun1(setPosImage)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView, false);
	PT_DocPosition pos = pView->getDocPositionFromLastXY();

	fl_BlockLayout * pBlock = pView->getBlockAtPosition(pos);
	fp_Run *  pRun = nullptr;
	fp_Line * pLine = nullptr;
	UT_sint32 x1,x2,y1,y2,iHeight;
	bool bEOL = false;
	bool bDir = false;
	if(pBlock)
	{
		pRun = pBlock->findPointCoords(pos,bEOL,x1,y1,x2,y2,iHeight,bDir);
		while(pRun && pRun->getType() != FPRUN_IMAGE)
		{
			pRun = pRun->getNextRun();
		}
		if(pRun && pRun->getType() == FPRUN_IMAGE)
		{
			UT_DEBUGMSG(("SEVIOR: Image run on pos \n"));
		}
		else
		{
			UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
			return false;
		}
	}
	UT_nonnull_or_return(pRun, false);
	pLine = pRun->getLine();
	if(pLine == nullptr)
	{
	        return false;
	}
	pView->cmdSelect(pos,pos+1);
	fp_ImageRun * pImageRun = static_cast<fp_ImageRun *>(pRun);
	std::string sWidth;
	std::string sHeight;
	double d = static_cast<double>(pRun->getWidth())/static_cast<double>(UT_LAYOUT_RESOLUTION);
	sWidth =  UT_formatDimensionedValue(d,"in", nullptr);
	d = static_cast<double>(pRun->getHeight())/static_cast<double>(UT_LAYOUT_RESOLUTION);
	sHeight =  UT_formatDimensionedValue(d,"in", nullptr);
//
// Get the dataID of the image.

	const char * dataID = pImageRun->getDataId();
	const PP_AttrProp * pImageAP = pImageRun->getSpanAP();
	std::string sFrameProps;
	std::string sProp;
	std::string sVal;
	sProp = "frame-type";
	sVal = "image";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
//
// Turn off the borders.
//
	sProp = "top-style";
	sVal = "none";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	sProp = "right-style";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	sProp = "left-style";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	sProp = "bot-style";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
//
// Set width/Height
//
	sProp = "frame-width";
	sVal = sWidth;
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	sProp = "frame-height";
	sVal = sHeight;
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	double xpos = 0.0;
	double ypos= 0.0;

	sProp = "position-to";
	sVal = "page-above-text";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	if(pView->isHdrFtrEdit() || pView->isInHdrFtr(pos))
	{
		pView->clearHdrFtrEdit();
		pView->warpInsPtToXY(0,0,false);
		pos = pView->getPoint();
	}

//
// Now calculate the Y offset to the Column
//
	UT_sint32 yLine = pLine->getY() + pLine->getColumn()->getY();
	ypos = static_cast<double>(yLine)/static_cast<double>(UT_LAYOUT_RESOLUTION);
	sProp = "frame-page-ypos";
	sVal = UT_formatDimensionedValue(ypos,"in", nullptr);
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	UT_sint32 ix = pRun->getX() + pLine->getColumn()->getX() + pLine->getX();
	xpos =  static_cast<double>(ix)/static_cast<double>(UT_LAYOUT_RESOLUTION);
	sProp = "frame-page-xpos";
	sVal = UT_formatDimensionedValue(xpos,"in", nullptr);
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	sVal = UT_std_string_sprintf("%d", pLine->getPage()->getPageNumber());
	sProp = "frame-pref-page";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
//
// Wrapped Mode
//
	sProp = "wrap-mode";
	sVal = "wrapped-both";
	UT_std_string_setProperty(sFrameProps, sProp, sVal);
	//
	// Now the alt and title
	//
	const char * szTitle = nullptr;
	const char * szDescription = nullptr;
	bool bFound = pImageAP->getAttribute("title",szTitle);
	if(!bFound)
	{
			szTitle = "";
	}
	bFound = pImageAP->getAttribute("alt",szDescription);
	if(!bFound)
	{
			szDescription = "";
	}
//
// Now define the Frame attributes strux
//
	const PP_PropertyVector attributes = {
		PT_STRUX_IMAGE_DATAID, dataID,
		PT_PROPS_ATTRIBUTE_NAME, sFrameProps.c_str(),
		PT_IMAGE_TITLE, szTitle,
		PT_IMAGE_DESCRIPTION, szDescription
	};
//
// This deletes the inline image and places a positioned image in it's place
// It deals with the undo/general update issues.
//
	pView->convertInLineToPositioned(pos,attributes);
//
// Done! Now have a positioned image!
//
	return true;
}

Defun1(dlgFmtPosImage)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_Image * pDialog
		= static_cast<XAP_Dialog_Image *>(pDialogFactory->requestDialog(XAP_DIALOG_ID_IMAGE));
	UT_return_val_if_fail(pDialog, false);
	fl_FrameLayout * pPosObj = pView->getFrameLayout();
	if(pPosObj == nullptr)
	{
		// try to select frame
		pView->activateFrame();
		pPosObj = pView->getFrameLayout();
		if (pPosObj == nullptr)
		{
			return true;
		}
	}
	if(pPosObj-> getFrameType() < FL_FRAME_WRAPPER_IMAGE)
	{
	  return true;
	}

	const PP_AttrProp* pAP = nullptr;
	pPosObj->getAP(pAP);
	const gchar* szTitle = nullptr;
	const gchar* szDescription = nullptr;
	pDialog->setInHdrFtr(false);
	std::string rulerUnits;
	UT_Dimension dim = DIM_IN;
	if (XAP_App::getApp()->getPrefsValue(AP_PREF_KEY_RulerUnits, rulerUnits)) {
		dim = UT_determineDimension(rulerUnits.c_str());
	}
	pDialog->setPreferedUnits(dim);

	fl_BlockLayout * pBL = pView->getCurrentBlock();
	// an approximate... TODO: make me more accurate
	fl_DocSectionLayout * pDSL = pBL->getDocSectionLayout();
	UT_sint32 iColWidth = pDSL->getActualColumnWidth();
	UT_sint32 iColHeight = pDSL->getActualColumnHeight();
	double max_width  = iColWidth*72.0/UT_LAYOUT_RESOLUTION; // units are 1/72 of an inch
	double max_height = iColHeight*72.0/UT_LAYOUT_RESOLUTION;

	pDialog->setMaxWidth (max_width);
	pDialog->setMaxHeight (max_height);

	if (pAP) 
	{
	  pAP->getAttribute ("title", szTitle);
	  pAP->getAttribute ("alt", szDescription);
	}

	if (szTitle) 
	{
	  pDialog->setTitle (szTitle);
	}
	if (szDescription) 
	{
	  pDialog->setDescription (szDescription);
	}
	const gchar * pszWidth = nullptr;
	const gchar * pszHeight = nullptr;
	if(!pAP || !pAP->getProperty("frame-width",pszWidth))
	{
	  pszWidth = "1.0in";
	}
	if(!pAP || !pAP->getProperty("frame-height",pszHeight))
	{
	  pszHeight = "1.0in";
	}
	pDialog->setWidth( UT_reformatDimensionString(dim,pszWidth));
	pDialog->setHeight( UT_reformatDimensionString(dim,pszHeight));

	UT_DEBUGMSG(("Width %s Height %s \n",pszWidth,pszHeight));
	WRAPPING_TYPE iWrap = WRAP_NONE;
	if(pPosObj->getFrameWrapMode() == FL_FRAME_WRAPPED_TO_LEFT  )
	{
	  iWrap = WRAP_TEXTLEFT;
	}
	if(pPosObj->getFrameWrapMode() == FL_FRAME_WRAPPED_TO_RIGHT  )
	{
	  iWrap = WRAP_TEXTRIGHT;
	}
	else if(pPosObj->getFrameWrapMode() == FL_FRAME_WRAPPED_BOTH_SIDES)
	{
	  iWrap = WRAP_TEXTBOTH;
	} 
	else if(pPosObj->getFrameWrapMode() == FL_FRAME_ABOVE_TEXT)
	{
	  iWrap = WRAP_NONE;
	}
	else if(pPosObj->getFrameWrapMode() == FL_FRAME_BELOW_TEXT)
	{
	  iWrap = WRAP_NONE;
	}
	POSITION_TO iPos = POSITION_TO_PARAGRAPH;
	if(pPosObj->getFramePositionTo() == FL_FRAME_POSITIONED_TO_COLUMN)
	{
	  iPos = POSITION_TO_COLUMN;
	}
	else if(pPosObj->getFramePositionTo() == FL_FRAME_POSITIONED_TO_PAGE)
	{
	  iPos = POSITION_TO_PAGE;
	}
	pDialog->setWrapping( iWrap);
	pDialog->setPositionTo( iPos);
	if(pPosObj->isTightWrap())
	{
	  pDialog->setTightWrap(true);
	}
	else
	{
	  pDialog->setTightWrap(false);
	}
	pDialog->runModal(pFrame);
	XAP_Dialog_Image::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == XAP_Dialog_Image::a_OK);
	if(!bOK)
	{
	  return true;
	}


	UT_String sWidth;
	UT_String sHeight;

	sWidth = pDialog->getWidthString();
	sHeight = pDialog->getHeightString();
	UT_DEBUGMSG(("Width %s Height %s \n",sWidth.c_str(),sHeight.c_str()));
	PP_PropertyVector attribs = {
		"title", pDialog->getTitle().utf8_str(),
		"alt", pDialog->getDescription().utf8_str()
	};

	if(pDialog->getWrapping() == WRAP_INLINE)
	{
		const PP_PropertyVector properties = {
			"width", sWidth.c_str(),
			"height", sHeight.c_str()
		};

		pView->convertPositionedToInLine(pPosObj);
		pView->setCharFormat(properties, attribs);
		pView->updateScreen(true);
		return true;
	}
	else
	{
	  POSITION_TO newFormatMode = pDialog->getPositionTo(); 
	  WRAPPING_TYPE newWrapMode = pDialog->getWrapping();
	  PP_PropertyVector properties = {
		  "frame-width", sWidth.c_str(),
		  "frame-height", sHeight.c_str(),
		  "wrap-mode", "",
		  "position-to", "",
		  "tight-wrap", pDialog->isTightWrap() ? "1" : "0",
	  };

	  if(newWrapMode == WRAP_TEXTRIGHT)
	  {
	    properties[5] = "wrapped-to-right";
	  }
	  else if(newWrapMode == WRAP_TEXTLEFT)
	  {
	    properties[5] = "wrapped-to-left";
	  }
	  else if(newWrapMode == WRAP_TEXTBOTH)
	  {
	    properties[5] = "wrapped-both";
	  }
	  else if(newWrapMode == WRAP_NONE)
	  {
	    properties[5] = "above-text";
	  }

	  if(newFormatMode == POSITION_TO_PARAGRAPH)
	  {
	    properties[7] = "block-above-text";
	  }
	  else if(newFormatMode == POSITION_TO_COLUMN)
	  {
	    properties[7] = "column-above-text";
	  }
	  else if(newFormatMode == POSITION_TO_PAGE)
	  {
	    properties[7] = "page-above-text";
	  }

	  fp_FrameContainer * pFrameC = static_cast<fp_FrameContainer *>(pPosObj->getFirstContainer());
	  fv_FrameStrings FrameStrings;
	  fl_BlockLayout * pCloseBL = nullptr;
	  fp_Page * pPage = nullptr;

	  if (pFrameC && (newFormatMode != iPos))
	  {
		  UT_sint32 iXposPage = pFrameC->getX() - pFrameC->getXPad();
		  UT_sint32 iYposPage = pFrameC->getY() - pFrameC->getYPad();
		  UT_sint32 xp = 0;
		  UT_sint32 yp = 0;
		  pPage = pFrameC->getColumn()->getPage();
		  pView->getPageScreenOffsets(pPage,xp,yp);
		  pView->getFrameStrings_view(iXposPage+xp,iYposPage+yp,
									  FrameStrings,&pCloseBL,&pPage);

		  UT_DEBUGMSG(("Position of frame: X %d\t Y %d\n",iXposPage,iYposPage));
		  if (newFormatMode == POSITION_TO_PARAGRAPH)
		  {
			  properties.push_back("xpos");
			  properties.push_back(FrameStrings.sXpos.c_str());
			  properties.push_back("ypos");
			  properties.push_back(FrameStrings.sYpos.c_str());
		  }
		  else if (newFormatMode == POSITION_TO_COLUMN)
		  {
			  properties.push_back("frame-col-xpos");
			  properties.push_back(FrameStrings.sColXpos.c_str());
			  properties.push_back("frame-col-ypos");
			  properties.push_back(FrameStrings.sColYpos.c_str());
			  properties.push_back("frame-pref-column");
			  properties.push_back(FrameStrings.sPrefColumn.c_str());
		  }
		  else if (newFormatMode == POSITION_TO_PAGE)
		  {
			  properties.push_back("frame-page-xpos");
			  properties.push_back(FrameStrings.sPageXpos.c_str());
			  properties.push_back("frame-page-ypos");
			  properties.push_back(FrameStrings.sPageYpos.c_str());
		  }
	  }

	  //
	  // Change the frame!
	  //
	  pView->setFrameFormat(attribs,properties,pCloseBL);
	}
	return true;
}








Defun(dlgColumns)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_Columns * pDialog
		= static_cast<AP_Dialog_Columns *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_COLUMNS));
UT_return_val_if_fail(pDialog, false);
	UT_uint32 iColumns = 1;
	bool bLineBetween = false;
	bool bSpaceAfter = false;
	bool bMaxHeight = false;

	PP_PropertyVector props_in;

	bool bResult = pView->getSectionFormat(props_in);

	if (!bResult)
	{
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
	}

	// NB: maybe *no* properties are consistent across the selection
	const std::string & sz = PP_getAttribute("columns", props_in);
	if (!sz.empty())
	{
		iColumns = atoi(sz.c_str());
	}

	if ( iColumns > 1 )
	{
		EX(viewPrintLayout);
	}

	bLineBetween = (PP_getAttribute("column-line", props_in) == "on");

	UT_uint32 iOrder = 0;
	iOrder = PP_getAttribute("dom-dir", props_in) != "ltr" ? 1 : 0;

	pDialog->setColumnOrder(iOrder);

	bSpaceAfter = !PP_getAttribute("section-space-after", props_in).empty();
	bMaxHeight = !PP_getAttribute("section-max-column-height", props_in).empty();

	pDialog->setColumns(iColumns);
	pDialog->setLineBetween(bLineBetween);
	pDialog->runModal(pFrame);

	AP_Dialog_Columns::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == AP_Dialog_Columns::a_OK);

	if (bOK)
	{
		// Set the columns property.

		bMaxHeight = bMaxHeight || pDialog->isMaxHeightChanged();
		bSpaceAfter = bSpaceAfter || pDialog->isSpaceAfterChanged();

		const char *buf3;
		const char *buf4;
		if(pDialog->getColumnOrder())
		{
			buf3 = "rtl";
			buf4 = "right";
		}
		else
		{
			buf3 = "ltr";
			buf4 = "left";
		}
		PP_PropertyVector props = {
			"columns", UT_std_string_sprintf("%i", pDialog->getColumns()),
			"column-line", pDialog->getLineBetween() ? "on" : "off",
			"dom-dir", buf3,
			"text-align", buf4
		};

		if(bSpaceAfter)
		{
			props.push_back("section-space-after");
			props.push_back(pDialog->getSpaceAfterString());
		}
		if(bMaxHeight)
		{
			props.push_back("section-max-column-height");
			props.push_back(pDialog->getHeightString());
		}
		pView->setSectionFormat(props);
	}

	pDialogFactory->releaseDialog(pDialog);

	return bOK;
}

Defun(style)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	
	UT_return_val_if_fail(pView, false);
	UT_UTF8String utf8(pCallData->m_pData, pCallData->m_dataLength);
	const gchar * style = reinterpret_cast<const gchar *>(utf8.utf8_str());
	pView->setStyle(style,false);
	pView->notifyListeners(AV_CHG_MOTION  | AV_CHG_HDRFTR);
	
	return true;
}

static bool s_doStylesDlg(FV_View * pView)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_Styles * pDialog
		= static_cast<AP_Dialog_Styles *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_STYLES));
UT_return_val_if_fail(pDialog, false);	if(pView->isHdrFtrEdit())
	{
		pView->clearHdrFtrEdit();
		pView->warpInsPtToXY(0,0,false);
	}

	pDialog->runModal(pFrame);

//	AP_Dialog_Styles::tAnswer ans = pDialog->getAnswer();
	bool bOK = true;
//
// update the combo box with the new styles.
//
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	//
	// Get all clones of this frame and set styles combo box
	//
	UT_GenericVector<XAP_Frame*> vClones;
	if(pFrame->getViewNumber() > 0)
	{
		pApp->getClones(&vClones,pFrame);
		for (UT_sint32 i = 0; i < vClones.getItemCount(); i++)
		{
			XAP_Frame * f = vClones.getNthItem(i);
			f->repopulateCombos();
		}
	}
	else
	{
		pFrame->repopulateCombos();
	}
//
// Now update all views on the document. Do this always to be safe.
//
	{
		PD_Document * pDoc = pView->getLayout()->getDocument();
		pDoc->signalListeners(PD_SIGNAL_UPDATE_LAYOUT);
	}

	pDialogFactory->releaseDialog(pDialog);
	return bOK;
}

Defun1(formatFootnotes)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_FormatFootnotes * pDialog
		= static_cast<AP_Dialog_FormatFootnotes *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_FORMAT_FOOTNOTES));
	UT_return_val_if_fail(pDialog, false);	
	pDialog->runModal(pFrame);
	AP_Dialog_FormatFootnotes::tAnswer ans = pDialog->getAnswer();
	if(ans == AP_Dialog_FormatFootnotes::a_OK)
	{
//
// update all the layouts.
//
// Clear out pending redraws...
//
		lockGUI();
		pFrame->nullUpdate();
		pDialog->updateDocWithValues();
		pView->updateScreen(false);
		unlockGUI();
	}
	pDialogFactory->releaseDialog(pDialog);
	return true;
}

Defun1(dlgStyle)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	ABIWORD_VIEW;

	return s_doStylesDlg(pView);
}




Defun0(noop)
{
	CHECK_FRAME;
// this is a no-op, so unbound menus don't assert at trade shows
	return true;
}

static bool s_doWordCountDlg(FV_View * pView)
{
	UT_return_val_if_fail(pView,false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(XAP_App::getApp()->getDialogFactory());

	AP_Dialog_WordCount * pDialog
		= static_cast<AP_Dialog_WordCount *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_WORDCOUNT));
UT_return_val_if_fail(pDialog, false);
	if(pDialog->isRunning())
	{
		pDialog->activate();
	}
	else
	{
		pDialog->setCount(pView->countWords(true));
		pDialog->runModeless(pFrame);
	}
	bool bOK = true;
	return bOK;
}


Defun1(dlgWordCount)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	return s_doWordCountDlg(pView);
}

/****************************************************************/
/****************************************************************/

static bool s_doInsertTableDlg(FV_View * pView)
{
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_InsertTable * pDialog
		= static_cast<AP_Dialog_InsertTable *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_INSERT_TABLE));
UT_return_val_if_fail(pDialog, false);
	pDialog->runModal(pFrame);

	AP_Dialog_InsertTable::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == AP_Dialog_InsertTable::a_OK);
//
// Should be able to put a table in Headers/footers eventually
//
//	if(pView->isHdrFtrEdit())
//		return false;
//
	if (bOK)
	{
		if (pDialog->getColumnType() == AP_Dialog_InsertTable::b_FIXEDSIZE)
		{
			std::string propBuffer;
			UT_LocaleTransactor t(LC_NUMERIC, "C");
			for (UT_uint32 i = 0; i < pDialog->getNumCols(); i++)	{
				propBuffer += UT_std_string_sprintf("%fin/",
													pDialog->getColumnWidth());
			}
			const PP_PropertyVector propsArray = {
				"table-column-props", propBuffer,
			};
			pView->cmdInsertTable(pDialog->getNumRows(), pDialog->getNumCols(), propsArray);
		} else
		{
			pView->cmdInsertTable(pDialog->getNumRows(), pDialog->getNumCols(), PP_NOPROPS);
		}
	}

	pDialogFactory->releaseDialog(pDialog);

	return bOK;
}

Defun1(sortColsAscend)
{
	UT_UNUSED(pAV_View);
	CHECK_FRAME;
	//ABIWORD_VIEW;
	return true;
}

Defun1(sortColsDescend)
{
	UT_UNUSED(pAV_View);
	CHECK_FRAME;
	//ABIWORD_VIEW;
	return true;
}

Defun1(sortRowsAscend)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->cmdSortTableRows(true, -1, false);
}

Defun1(sortRowsDescend)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->cmdSortTableRows(false, -1, false);
}

Defun1(paraSortAscend)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	pView->cmdSortParagraphs(true);
	return true;
}

Defun1(paraSortDescend)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	pView->cmdSortParagraphs(false);
	return true;
}





Defun1(insertSumRows)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	PP_PropertyVector atts = {
		"param", ""
	};
	pView->cmdInsertField("sum_rows", atts, PP_NOPROPS);
	return true;
}

Defun1(insertSumCols)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	PP_PropertyVector atts = {
		"param", ""
	};
	pView->cmdInsertField("sum_cols", atts, PP_NOPROPS);
	return true;
}

Defun1(insertTable)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_doInsertTableDlg(pView);
}


/****************************************************************/
/****************************************************************/
Defun1(toggleHidden)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return _toggleSpan(pView, "display", "none", "");
}

Defun1(toggleBold)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return _toggleSpan(pView, "font-weight", "bold", "normal");
}

Defun1(toggleItalic)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return _toggleSpan(pView, "font-style", "italic", "normal");
}

Defun1(toggleUline)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return _toggleSpan(pView, "text-decoration", "underline", "none", true);
}
Defun1(toggleOline)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return _toggleSpan(pView, "text-decoration", "overline", "none", true);
}

Defun1(toggleStrike)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return _toggleSpan(pView, "text-decoration", "line-through", "none", true);
}


Defun1(toggleTopline)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return _toggleSpan(pView, "text-decoration", "topline", "none", true);
}


Defun1(toggleBottomline)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return _toggleSpan(pView, "text-decoration", "bottomline", "none", true);
}

// non-static so that ap_Toolbar_Functions.cpp can use it
void s_getPageMargins(FV_View * pView,
					  double &margin_left,
					  double &margin_right,
					  double &page_margin_left,
					  double &page_margin_right,
					  double &page_margin_top,
					  double &page_margin_bottom)
{
	UT_return_if_fail(pView);
	// get current char properties from pView
	const gchar * prop = nullptr;
	std::string sz;

	PP_PropertyVector props_in;
	pView->getBlockFormat(props_in);

	prop = "margin-left";
	sz = PP_getAttribute(prop, props_in);
	margin_left = UT_convertToInches(sz.c_str());

	prop = "margin-right";
	sz = PP_getAttribute(prop, props_in);
	margin_right = UT_convertToInches(sz.c_str());

	prop = "page-margin-left";
	sz = PP_getAttribute(prop, props_in);
	page_margin_left = UT_convertToInches(sz.c_str());

	prop = "page-margin-right";
	sz = PP_getAttribute(prop, props_in);
	page_margin_right = UT_convertToInches(sz.c_str());

	prop = "page-margin-top";
	sz = PP_getAttribute(prop, props_in);
	page_margin_top	 = UT_convertToInches(sz.c_str());

	prop = "page-margin-bottom";
	sz = PP_getAttribute(prop, props_in);
	page_margin_bottom = UT_convertToInches(sz.c_str());
}

// MSWord defines this to 1/2 an inch, so we do too
#define TOGGLE_INDENT_AMT 0.5

Defun1(toggleIndent)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
  bool doLists = true;
  double page_size = pView->getPageSize().Width (DIM_IN);

  double margin_left = 0., margin_right = 0., allowed = 0.,
	  page_margin_left = 0., page_margin_right = 0.,
	  page_margin_top = 0., page_margin_bottom = 0.;

  s_getPageMargins (pView, margin_left, margin_right,
					page_margin_left, page_margin_right,
					page_margin_top, page_margin_bottom);

  allowed = page_size - page_margin_left - page_margin_right;
  if (margin_left >= allowed)
	  return true;

  fl_BlockLayout * pBL = pView->getCurrentBlock();
  if(pBL && (!pBL->isListItem() || !pView->isSelectionEmpty()) )
  {
	  doLists = false;
  }
  return  pView->setBlockIndents(doLists, static_cast<double>(TOGGLE_INDENT_AMT) ,page_size);
}

Defun1(toggleUnIndent)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
  bool ret;
  double page_size = pView->getPageSize().Width (DIM_IN);
  bool doLists = true;

  double margin_left = 0., margin_right = 0., allowed = 0.,
	  page_margin_left = 0., page_margin_right = 0.,
	  page_margin_top = 0., page_margin_bottom = 0.;

  s_getPageMargins (pView, margin_left, margin_right,
					page_margin_left, page_margin_right,
					page_margin_top, page_margin_bottom);

  fl_BlockLayout * pBL = pView->getCurrentBlock();
  UT_BidiCharType iBlockDir = UT_BIDI_LTR;

  if(pBL)
	  iBlockDir = pBL->getDominantDirection();
  
  allowed = iBlockDir == UT_BIDI_LTR ? margin_left : margin_right;
  if ( allowed <= 0. )
	  return true ;

  if(pBL && (!pBL->isListItem() || !pView->isSelectionEmpty()) )
  {
	 doLists = false;
  }
  ret = pView->setBlockIndents(doLists, (double) -TOGGLE_INDENT_AMT ,page_size);
  return ret;
}

#undef TOGGLE_INDENT_AMT

Defun1(toggleSuper)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return _toggleSpan(pView, "text-position", "superscript", "normal");
}

Defun1(toggleSub)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return _toggleSpan(pView, "text-position", "subscript", "normal");
}

Defun1(toggleDirOverrideLTR)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return _toggleSpan(pView, "dir-override", "ltr", "");
}

Defun1(toggleDirOverrideRTL)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return _toggleSpan(pView, "dir-override", "rtl", "");
}





Defun1(doBullets)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	pView->processSelectedBlocks(BULLETED_LIST);
	return true;
}

Defun1(doNumbers)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	pView->processSelectedBlocks(NUMBERED_LIST);
	return true;
}

Defun1(doDashedList)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	pView->processSelectedBlocks(DASHED_LIST);
	return true;
}

/*!
 * Bullet/numbering library pick.
 * pCallData->m_pData: "TYPE[:DECIMAL[:DELIM]]" where TYPE is one of
 * NONE, BULLETED, DASHED, SQUARE, TRIANGLE, DIAMOND, STAR, IMPLIES,
 * TICK, BOX, HAND, HEART, ARROWHEAD, NUMBERED, LOWERCASE, UPPERCASE,
 * LOWERROMAN, UPPERROMAN, HEBREW, ARABICNUM.  DECIMAL overrides the
 * numbering format (e.g. "%*%d" for 1.1.1) and DELIM the suffix
 * (e.g. "%L)" for "1)").
 */
Defun(doListType)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);

	UT_UTF8String u8arg(pCallData->m_pData, pCallData->m_dataLength);
	std::string arg(u8arg.utf8_str());
	std::string sType = arg.substr(0, arg.find(':'));
	std::string sDecimal, sDelim;
	size_t p1 = arg.find(':');
	if (p1 != std::string::npos)
	{
		size_t p2 = arg.find(':', p1 + 1);
		sDecimal = arg.substr(p1 + 1,
							  p2 == std::string::npos ? p2 : p2 - p1 - 1);
		if (p2 != std::string::npos)
			sDelim = arg.substr(p2 + 1);
	}

	FL_ListType lType = NOT_A_LIST;
	if      (sType == "BULLETED")    lType = BULLETED_LIST;
	else if (sType == "DASHED")      lType = DASHED_LIST;
	else if (sType == "SQUARE")      lType = SQUARE_LIST;
	else if (sType == "TRIANGLE")    lType = TRIANGLE_LIST;
	else if (sType == "DIAMOND")     lType = DIAMOND_LIST;
	else if (sType == "STAR")        lType = STAR_LIST;
	else if (sType == "IMPLIES")     lType = IMPLIES_LIST;
	else if (sType == "TICK")        lType = TICK_LIST;
	else if (sType == "BOX")         lType = BOX_LIST;
	else if (sType == "HAND")        lType = HAND_LIST;
	else if (sType == "HEART")       lType = HEART_LIST;
	else if (sType == "ARROWHEAD")   lType = ARROWHEAD_LIST;
	else if (sType == "NUMBERED")    lType = NUMBERED_LIST;
	else if (sType == "LOWERCASE")   lType = LOWERCASE_LIST;
	else if (sType == "UPPERCASE")   lType = UPPERCASE_LIST;
	else if (sType == "LOWERROMAN")  lType = LOWERROMAN_LIST;
	else if (sType == "UPPERROMAN")  lType = UPPERROMAN_LIST;
	else if (sType == "HEBREW")      lType = HEBREW_LIST;
	else if (sType == "ARABICNUM")   lType = ARABICNUMBERED_LIST;
	else if (sType == "NONE")
		return pView->cmdRemoveListFormat();
	else
		return false;

	return pView->cmdApplyListType(
		lType,
		sDecimal.empty() ? nullptr : sDecimal.c_str(),
		sDelim.empty()   ? nullptr : sDelim.c_str());
}

/*!
 * Paragraph border preset for the ribbon Borders menu.
 * pCallData->m_pData: bottom/top/left/right/none/all/outside/
 * inside/insideh/hline.
 */
Defun(paraBorder)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData,
						  false);
	UT_UTF8String u8arg(pCallData->m_pData, pCallData->m_dataLength);
	return pView->cmdParaBorder(u8arg.utf8_str());
}

/* ---- Table Design tab (fl_TableStyles recipes) ----
 * data strings are small and unlocalised:
 *   tableStyle     "<style-id>"
 *   tableStyleOpt  "<option-index>:<0|1>"
 *   tableBorder    "<preset-name>"
 *   tableShading   "<hex-colour>"
 *   tablePen       "<style>|<thickness>|<colour>" (empty field keeps old)
 */
Defun(tableStyle)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData,
						  false);
	UT_UTF8String u8arg(pCallData->m_pData, pCallData->m_dataLength);
	return pView->cmdTableSetStyle(u8arg.utf8_str());
}

Defun1(tableStyleClear)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->cmdTableClearStyle();
}

Defun(tableStyleOpt)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData,
						  false);
	UT_UTF8String u8arg(pCallData->m_pData, pCallData->m_dataLength);
	const char * s = u8arg.utf8_str();
	const char * colon = s ? strchr(s, ':') : nullptr;
	UT_return_val_if_fail(s && colon, false);
	return pView->cmdTableSetStyleOption(atoi(s), colon[1] == '1');
}

Defun(tableBorder)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData,
						  false);
	UT_UTF8String u8arg(pCallData->m_pData, pCallData->m_dataLength);
	const char * s = u8arg.utf8_str();
	struct { const char * name; UT_sint32 preset; } presets[] = {
		{ "none",    0 }, { "all",     1 }, { "outside", 2 },
		{ "inside",  3 }, { "insideh", 4 }, { "insidev", 5 },
		{ "top",     6 }, { "bot",     7 }, { "left",    8 },
		{ "right",   9 }
	};
	for (const auto & p : presets)
		if (s && !strcmp(s, p.name))
			return pView->cmdTableBorderPreset(p.preset);
	return false;
}

Defun(tableShading)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData,
						  false);
	UT_UTF8String u8arg(pCallData->m_pData, pCallData->m_dataLength);
	return pView->cmdTableCellShading(u8arg.utf8_str());
}

Defun(tablePen)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData,
						  false);
	UT_UTF8String u8arg(pCallData->m_pData, pCallData->m_dataLength);
	UT_UTF8String arg(u8arg);
	/* "<style>|<thickness>|<colour>" - any field may be empty */
	const char * s = arg.utf8_str();
	std::string f[3];
	int i = 0;
	for (const char * p = s; p && i < 3; ++i)
	{
		const char * bar = strchr(p, '|');
		f[i] = bar ? std::string(p, bar - p) : std::string(p);
		p = bar ? bar + 1 : nullptr;
	}
	pView->setTablePen(f[0].empty() ? nullptr : f[0].c_str(),
					   f[1].empty() ? nullptr : f[1].c_str(),
					   f[2].empty() ? nullptr : f[2].c_str());
	return true;
}

Defun(colorForeTB)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	UT_UTF8String utf8(pCallData->m_pData, pCallData->m_dataLength);

	const PP_PropertyVector properties = {
		"color", utf8.utf8_str()
	};
	pView->setCharFormat(properties);

	return true;
}

Defun(colorBackTB)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	UT_UTF8String utf8(pCallData->m_pData, pCallData->m_dataLength);

	const PP_PropertyVector properties = {
		"bgcolor", utf8.utf8_str()
	};
	pView->setCharFormat(properties);

	return true;
}

/*! removes the "props" attribute, i.e., all non-style based formatting
 */
Defun1(togglePlain)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	if (pView->getDocument()->areStylesLocked())
		return true;

	pView->resetCharFormat(false);
		
	return true;
}

Defun1(alignLeft)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	if (pView->getDocument()->areStylesLocked())
		return true;

	const PP_PropertyVector properties = {
		"text-align", "left"
	};
	pView->setBlockFormat(properties);
	return true;
}

Defun1(alignCenter)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	if (pView->getDocument()->areStylesLocked())
		return true;

	const PP_PropertyVector properties = {
		"text-align", "center"
	};
	pView->setBlockFormat(properties);
	return true;
}

Defun1(alignRight)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	if (pView->getDocument()->areStylesLocked())
		return true;

	const PP_PropertyVector properties = {
		"text-align", "right"
	};
	pView->setBlockFormat(properties);
	return true;
}

Defun1(alignJustify)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	if (pView->getDocument()->areStylesLocked())
		return true;

	const PP_PropertyVector properties = {
		"text-align", "justify"
	};
	pView->setBlockFormat(properties);
	return true;
}

Defun1(setStyleHeading1)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	const gchar * style = "Heading 1";
	pView->setStyle(style,false);
	pView->notifyListeners(AV_CHG_MOTION | AV_CHG_HDRFTR);
	return true;
}


Defun1(setStyleHeading2)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	const gchar * style = "Heading 2";
	pView->setStyle(style,false);
	pView->notifyListeners(AV_CHG_MOTION | AV_CHG_HDRFTR);
	return true;
}

Defun1(setStyleHeading3)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	const gchar * style = "Heading 3";
	pView->setStyle(style,false);
	pView->notifyListeners(AV_CHG_MOTION | AV_CHG_HDRFTR);
	return true;
}

// Ctrl+Shift+N (Word): apply the Normal paragraph style
Defun1(setStyleNormal)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	const gchar * style = "Normal";
	pView->setStyle(style,false);
	pView->notifyListeners(AV_CHG_MOTION | AV_CHG_HDRFTR);
	return true;
}

Defun1(sectColumns1)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	if(pView->isHdrFtrEdit())
		return false;

	const PP_PropertyVector properties = {
		"columns", "1"
	};
	pView->setSectionFormat(properties);
	return true;
}

Defun1(sectColumns2)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	if(pView->isHdrFtrEdit())
		return false;

	const PP_PropertyVector properties = {
		"columns", "2"
	};
	pView->setSectionFormat(properties);
	return true;
}

Defun1(sectColumns3)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	if(pView->isHdrFtrEdit())
		return false;
	const PP_PropertyVector properties = {
		"columns", "3"
	};
	pView->setSectionFormat(properties);
	return true;
}

Defun1(paraBefore0)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	if (pView->getDocument()->areStylesLocked())
		return true;

	const PP_PropertyVector properties = {
		"margin-top", "0pt"
	};
	pView->setBlockFormat(properties);
	return true;
}

Defun1(paraBefore12)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	if (pView->getDocument()->areStylesLocked())
		return true;

	const PP_PropertyVector properties = {
		"margin-top", "12pt"
	};
	pView->setBlockFormat(properties);
	return true;
}

// Ctrl+0 (Word): add/remove the 12pt space before the paragraph
Defun1(toggleParaBefore)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	if (pView->getDocument()->areStylesLocked())
		return true;

	PP_PropertyVector props_in;
	pView->getBlockFormat(props_in);
	const std::string & sBefore = PP_getAttribute("margin-top", props_in);
	const bool bHasSpace = (!sBefore.empty() && sBefore != "0pt");

	const PP_PropertyVector properties = {
		"margin-top", bHasSpace ? "0pt" : "12pt"
	};
	pView->setBlockFormat(properties);
	return true;
}

Defun1(singleSpace)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	if (pView->getDocument()->areStylesLocked())
		return true;

	const PP_PropertyVector properties = {
		"line-height", "1.0"
	};
	pView->setBlockFormat(properties);
	return true;
}

Defun1(middleSpace)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	if (pView->getDocument()->areStylesLocked())
		return true;

	const PP_PropertyVector properties = {
		"line-height", "1.5"
	};
	pView->setBlockFormat(properties);
	return true;
}

Defun1(doubleSpace)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	if (pView->getDocument()->areStylesLocked())
		return true;

	const PP_PropertyVector properties = {
		"line-height", "2.0"
	};
	pView->setBlockFormat(properties);
	return true;
}

#if defined(PT_TEST) || defined(FMT_TEST) || defined(UT_TEST)
Defun1(Test_Dump)
{
	CHECK_FRAME;
//	ABIWORD_VIEW;
//	UT_return_val_if_fail(pView,false);
//	pView->Test_Dump();
	return true;
}

Defun1(Test_Ftr)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	pView->insertPageNum(nullptr, FL_HDRFTR_FOOTER);
	return true;
}
#endif

Defun1(setEditVI)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	// enter "VI Edit Mode" (only valid when VI keys are loaded)
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	// When exiting input mode, vi goes to previous character
	pView->cmdCharMotion(false,1);

	bool bResult = (XAP_App::getApp()->setInputMode("viEdit") != 0);
	return bResult;
}

Defun1(setInputVI)
{
	CHECK_FRAME;
// enter "VI Input Mode" (only valid when VI keys are loaded)
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);

	bool bResult = (XAP_App::getApp()->setInputMode("viInput") != 0);
	return bResult;
}

Defun1(cycleInputMode)
{
	CHECK_FRAME;
// switch to the next input mode { default, emacs, vi, ... }
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);

	// this edit method may get ignored entirely
	bool b;
	if (pPrefs->getPrefsValueBool(AP_PREF_KEY_KeyBindingsCycle, b) && !b) {
		return false;
	}

	const char * szCurrentInputMode = pApp->getInputMode();
	UT_return_val_if_fail (szCurrentInputMode, false);
	AP_BindingSet * pBSet = static_cast<AP_BindingSet *>(pApp->getBindingSet());
	const char * szNextInputMode = pBSet->getNextInCycle(szCurrentInputMode);
	if (!szNextInputMode)				// probably an error....
		return false;

	bool bResult = (pApp->setInputMode(szNextInputMode) != 0);

	// POLICY: make this the default for new frames, too
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
UT_return_val_if_fail(pScheme, false);
	pScheme->setValue(static_cast<const gchar *>(AP_PREF_KEY_KeyBindings),
					  szNextInputMode);

	return bResult;
}

Defun1(toggleInsertMode)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp, false);
	XAP_Prefs * pPrefs = pApp->getPrefs();
	UT_return_val_if_fail(pPrefs, false);

	AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
    UT_return_val_if_fail(pFrameData, false);

	// this edit method may get ignored entirely
	bool b;
	if (pPrefs->getPrefsValueBool(AP_PREF_KEY_InsertModeToggle, b) && !b) {
        // if we are in insert mode, just return, otherwise give a chance
        // to toggle it, or one might get stick to overwrite.
        if(pFrameData->m_bInsertMode) {
            return false;
        }
    }

	// toggle the insert mode
	pFrameData->m_bInsertMode = ! pFrameData->m_bInsertMode;

	// the view actually does the dirty work
	pAV_View->setInsertMode(pFrameData->m_bInsertMode);

	if (pFrameData->m_pStatusBar)
	  pFrameData->m_pStatusBar->notify(pAV_View, AV_CHG_ALL);

	// POLICY: make this the default for new frames, too
	XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
UT_return_val_if_fail(pScheme, false);
	pScheme->setValueBool(AP_PREF_KEY_InsertMode, pFrameData->m_bInsertMode);

	return true;
}


//////////////////////////////////////////////////////////////////
// The following commands are suggested for the various VI keybindings.
// It may be possible to use our exisiting methods for them, but I
// didn't know all of the little (subtle) side-effects that make VI
// special.
//////////////////////////////////////////////////////////////////

Defun(viCmd_5e)
{
	CHECK_FRAME;
	//Move to first non space char on current line 
	//TODO: BOL seems to count as a BOW, how to move to first non space? 
	return ( EX(warpInsPtBOL));
}

Defun(viCmd_A)
{
	CHECK_FRAME;
// insert after the end of the current line
	return ( EX(warpInsPtEOL) && EX(setInputVI) );
}

Defun(viCmd_C)
{
	CHECK_FRAME;
// Select to the end of the line for modification
	return ( EX(extSelEOL) && EX(setInputVI) );
}

Defun(viCmd_I)
{
	CHECK_FRAME;
// insert before the beginning of current line
	return ( EX(warpInsPtBOL) && EX(setInputVI) );
}

Defun(viCmd_J)
{
	CHECK_FRAME;
// Join current and next line.
	return ( EX(warpInsPtEOL) && EX(delRight) && EX(insertSpace) );
}

Defun(viCmd_O)
{
	CHECK_FRAME;
// insert new line before current line, go into input mode
	return ( EX(warpInsPtBOL) && EX(insertLineBreak) && EX(warpInsPtLeft) \
		&& EX(setInputVI) );
}

Defun(viCmd_P)
{
	CHECK_FRAME;
// paste text before cursor
	return ( EX(warpInsPtLeft) && EX(paste) );
}

Defun(viCmd_a)
{
	CHECK_FRAME;
// insert after the current position
	return ( EX(warpInsPtRight) && EX(setInputVI) );
}

Defun(viCmd_o)
{
	CHECK_FRAME;
// insert new line after current line, go into input mode
	return ( EX(warpInsPtEOL) && EX(insertLineBreak) && EX(setInputVI) );
}

/* c$ */
Defun(viCmd_c24)
{
	CHECK_FRAME;
//change to end of current line
	return ( EX(delEOL) && EX(setInputVI) );
}

/* c( */
Defun(viCmd_c28)
{
	CHECK_FRAME;
//change to start of current sentence
	return ( EX(delBOS) && EX(setInputVI) );
}

/* c) */
Defun(viCmd_c29)
{
	CHECK_FRAME;
//change to end of current sentence
	return ( EX(delEOS) && EX(setInputVI) );
}

/* c[ */
Defun(viCmd_c5b)
{
	CHECK_FRAME;
//change to beginning of current block
	return ( EX(delBOB) && EX(setInputVI) );
}

/* c] */
Defun(viCmd_c5d)
{
	CHECK_FRAME;
//change to end of current block
	return ( EX(delEOB) && EX(setInputVI) );
}

/* c^ */
Defun(viCmd_c5e)
{
	CHECK_FRAME;
//change to beginning of current line
	return ( EX(delBOL) && EX(setInputVI) );
}

Defun(viCmd_cb)
{
	CHECK_FRAME;
//change to beginning of current word
	return ( EX(delBOW) && EX(setInputVI) );
}

Defun(viCmd_cw)
{
	CHECK_FRAME;
// delete to the end of current word, start input mode
	return ( EX(delEOW) && EX(setInputVI) );
}

/* d$ */
Defun(viCmd_d24)
{
	CHECK_FRAME;
//delete to end of line
	return ( EX(delEOL) );
}

/* d( */
Defun(viCmd_d28)
{
	CHECK_FRAME;
//delete to start of sentence
	return ( EX(delBOS) );
}

/* d) */
Defun(viCmd_d29)
{
	CHECK_FRAME;
//delete to end of sentence
	return ( EX(delEOS) );
}

/* d[ */
Defun(viCmd_d5b)
{
	CHECK_FRAME;
//delete to beginning of block
	return ( EX(delBOB) );
}

/* d] */
Defun(viCmd_d5d)
{
	CHECK_FRAME;
//delete to end of block
	return ( EX(delEOB) );
}

/* d^ */
Defun(viCmd_d5e)
{
	CHECK_FRAME;
//delete to beginning of line
	return ( EX(delBOL) );
}

Defun(viCmd_db)
{
	CHECK_FRAME;
//delete to beginning of word
	return ( EX(delBOW) );
}

Defun(viCmd_dd)
{
	CHECK_FRAME;
// delete the current line
	return ( EX(warpInsPtBOL) && EX(delEOL) && EX(delLeft) && EX(warpInsPtBOL) );
}

Defun(viCmd_dw)
{
	CHECK_FRAME;
//delete to end of word
	return ( EX(delEOW) );
}

/* y$ */
Defun(viCmd_y24)
{
	CHECK_FRAME;
//copy to end of current line
	return ( EX(extSelEOL) && EX(copy) );
}

/* y( */
Defun(viCmd_y28)
{
	CHECK_FRAME;
//copy to beginning of current sentence
	return ( EX(extSelBOS) && EX(copy) );
}

/* y) */
Defun(viCmd_y29)
{
	CHECK_FRAME;
//copy to end of current sentence
	return ( EX(extSelEOS) && EX(copy) );
}

/* y[ */
Defun(viCmd_y5b)
{
	CHECK_FRAME;
//copy to beginning of current block
	return ( EX(extSelBOB) && EX(copy) );
}

/* y] */
Defun(viCmd_y5d)
{
	CHECK_FRAME;
//copy to end of current block
	return ( EX(extSelEOB) && EX(copy) );
}

/* y^ */
Defun(viCmd_y5e)
{
	CHECK_FRAME;
//copy to beginning of current line
	return ( EX(extSelBOL) && EX(copy) );
}

Defun(viCmd_yb)
{
	CHECK_FRAME;
//copy to beginning of current word
	return ( EX(extSelBOW) && EX(copy) );
}

Defun(viCmd_yw)
{
	CHECK_FRAME;
//copy to end of current word
	return ( EX(extSelEOW) && EX(copy) );
}

Defun(viCmd_yy)
{
	CHECK_FRAME;
//copy current line
	return ( EX(warpInsPtBOL) && EX(extSelEOL) && EX(copy) );
}

static bool s_AskForScriptName(XAP_Frame * pFrame,
							   UT_String& stPathname,
							   UT_ScriptIdType * ieft)
{
	UT_return_val_if_fail (ieft, false);

	stPathname.clear();

	pFrame->raise();

	XAP_Dialog_Id id = XAP_DIALOG_ID_FILE_OPEN;

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_FileOpenSaveAs * pDialog
		= static_cast<XAP_Dialog_FileOpenSaveAs *>(pDialogFactory->requestDialog(id));
	UT_return_val_if_fail(pDialog, false);

	UT_ScriptLibrary * instance = UT_ScriptLibrary::instance ();

	UT_uint32 filterCount = instance->getNumScripts ();

	const char ** szDescList = static_cast<const char **>(UT_calloc(filterCount + 1, sizeof(char *)));
	UT_return_val_if_fail(szDescList, false);

	const char ** szSuffixList = static_cast<const char **>(UT_calloc(filterCount + 1, sizeof(char *)));
	if(!szSuffixList)
	{
		UT_ASSERT_HARMLESS(szSuffixList);
		FREEP(szDescList);
		return false;
	}

	UT_ScriptIdType * nTypeList = static_cast<UT_ScriptIdType *>(UT_calloc(filterCount + 1, sizeof(UT_ScriptIdType)));
	if(!nTypeList)
	{
		UT_ASSERT_HARMLESS(nTypeList);
		FREEP(szDescList);
		FREEP(szSuffixList);
		return false;
	}

	UT_uint32 k = 0;

	while (instance->enumerateDlgLabels(k, &szDescList[k],
					   &szSuffixList[k], &nTypeList[k]))
		k++;

	pDialog->setFileTypeList(szDescList, szSuffixList,
							 static_cast<const UT_sint32 *>(nTypeList));

	UT_ScriptIdType dflFileType = -1;
	pDialog->setDefaultFileType(dflFileType);

	pDialog->runModal(pFrame);

	XAP_Dialog_FileOpenSaveAs::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == XAP_Dialog_FileOpenSaveAs::a_OK);

	if (bOK)
	{
		const std::string & resultPathname = pDialog->getPathname();

		if (!resultPathname.empty()) {
			stPathname += resultPathname;
		}

		UT_sint32 type = pDialog->getFileType();
		dflFileType = type;

		// If the number is negative, it's a special type.
		// Some operating systems which depend solely on filename
		// suffixes to indentify type (like Windows) will always
		// want auto-detection.
		if (type < 0)
			switch (type)
			{
			case XAP_DIALOG_FILEOPENSAVEAS_FILE_TYPE_AUTO:
				// do some automagical detecting
				*ieft = -1;
				break;
			default:
				// it returned a type we don't know how to handle
				UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
			}
		else
			*ieft = static_cast<UT_ScriptIdType>(pDialog->getFileType());
	}

	FREEP(szDescList);
	FREEP(szSuffixList);
	FREEP(nTypeList);

	pDialogFactory->releaseDialog(pDialog);

	return bOK;
}

class ABI_EXPORT OneShot_MailMerge_Listener : public IE_MailMerge::IE_MailMerge_Listener
{
public:

	explicit OneShot_MailMerge_Listener (PD_Document * pd)
		: IE_MailMerge::IE_MailMerge_Listener (), m_doc (pd)
		{

		}

	virtual ~OneShot_MailMerge_Listener ()
		{
		}
		
	virtual PD_Document* getMergeDocument() const  override
		{
			return m_doc;
		}
	
	virtual bool fireUpdate() override
		{
			// don't process any more data
			return false;
		}
	
private:
	PD_Document *m_doc;
};



Defun(executeScript)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame* pFrame = static_cast<XAP_Frame *> (pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	UT_DEBUGMSG(("executeScript (trying to execute [%s])\n", pCallData->getScriptName().c_str()));

	UT_ScriptLibrary * instance = UT_ScriptLibrary::instance ();

	// we have no expectations of executing a remote program
	char * scriptName = UT_go_filename_from_uri (pCallData->getScriptName().c_str());
	UT_return_val_if_fail (scriptName != nullptr, false);

#ifdef _WIN32
	// we need to add quotes to the script name _after_ the UT_go_filename_from_uri() call above;
	// if not, it will return nullptr and the script won't execute.

	UT_UTF8String script = "\"";
	script += scriptName;
	script += "\"";
	g_free(scriptName);
	scriptName = g_strdup(script.utf8_str());
#endif

	if (UT_OK != instance->execute(scriptName))
	{
		if (instance->errmsg().size() > 0)
			pFrame->showMessageBox(instance->errmsg().c_str(),
								   XAP_Dialog_MessageBox::b_O,
								   XAP_Dialog_MessageBox::a_OK);

		else
			pFrame->showMessageBox(AP_STRING_ID_SCRIPT_CANTRUN,
								   XAP_Dialog_MessageBox::b_O,
								   XAP_Dialog_MessageBox::a_OK,
								   scriptName);
	}

	g_free (scriptName);

	return true;
}


Defun1(dlgColorPickerFore)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	FV_View * pView = static_cast<FV_View *>(pAV_View);
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_Background * pDialog
		= static_cast<AP_Dialog_Background *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_BACKGROUND));
UT_return_val_if_fail(pDialog, false);//
// Set the color in the dialog to the current Color
//
	PP_PropertyVector propsChar;
	pView->getCharFormat(propsChar);
	const std::string & pszChar = PP_getAttribute("color",propsChar);
	pDialog->setColor(pszChar.c_str());
//
// Set the dialog to Foreground Color Mode.
//
	pDialog->setForeground();

	pDialog->runModal (pFrame);

	AP_Dialog_Background::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == AP_Dialog_Background::a_OK);

	if (bOK)
	{
		const gchar * clr = pDialog->getColor();
		const PP_PropertyVector properties = {
			"color", clr ? clr : ""
		};
		pView->setCharFormat(properties);
	}
	pDialogFactory->releaseDialog(pDialog);
	return bOK;
}


Defun1(dlgColorPickerBack)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	FV_View * pView = static_cast<FV_View *>(pAV_View);

	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_Background * pDialog
		= static_cast<AP_Dialog_Background *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_BACKGROUND));
UT_return_val_if_fail(pDialog, false);//
// Set the color in the dialog to the current Color
//
	PP_PropertyVector propsChar;
	pView->getCharFormat(propsChar);
	const std::string & pszChar = PP_getAttribute("bgcolor", propsChar);
	pDialog->setColor(pszChar.c_str());
//
// Set the dialog to Highlight Color Mode.
//
	pDialog->setHighlight();

	pDialog->runModal (pFrame);

	AP_Dialog_Background::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == AP_Dialog_Background::a_OK);

	if (bOK)
	{
		const gchar * clr = pDialog->getColor();
		const PP_PropertyVector properties = {
			"bgcolor", clr ? clr : ""
		};
		pView->setCharFormat(properties);
	}
	pDialogFactory->releaseDialog(pDialog);
	return bOK;
}

Defun1(dlgBackground)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	FV_View * pView = static_cast<FV_View *>(pAV_View);

	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_Background * pDialog
		= static_cast<AP_Dialog_Background *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_BACKGROUND));
UT_return_val_if_fail(pDialog, false);
//
// Get Current background color
//
	PP_PropertyVector propsSection;
	pView->getSectionFormat(propsSection);
	const std::string & background = PP_getAttribute("background-color", propsSection);
	pDialog->setColor(background.c_str());

	pDialog->runModal (pFrame);

	AP_Dialog_Background::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == AP_Dialog_Background::a_OK);

	if (bOK)
	{
		// let the view set the proper value in the
		// document and refresh/redraw itself
		const gchar * clr = pDialog->getColor();
		pView->setPaperColor (clr);
	}

	pDialogFactory->releaseDialog(pDialog);
	return bOK;
}


Defun1(dlgHdrFtr)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	FV_View * pView = static_cast<FV_View *>(pAV_View);

	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pView->getParentData());
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_HdrFtr * pDialog = static_cast<AP_Dialog_HdrFtr *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_HDRFTR));
UT_return_val_if_fail(pDialog, false);//
// Get stuff we need from the view
//
	if(pView->isHdrFtrEdit())
	{	
		pView->clearHdrFtrEdit();
		pView->warpInsPtToXY(0,0,false);
	}

	fl_BlockLayout *pBL = pView->getCurrentBlock();
	UT_return_val_if_fail( pBL, false );
	fl_DocSectionLayout * pDSL = static_cast<fl_DocSectionLayout *>(pBL->getDocSectionLayout());
	UT_ASSERT(pDSL->getContainerType() == FL_CONTAINER_DOCSECTION);
	bool bOldHdr = false;
	bool bOldHdrEven = false;
	bool bOldHdrFirst = false;
	bool bOldHdrLast = false;
	bool bOldFtr = false;
	bool bOldFtrEven = false;
	bool bOldFtrFirst = false;
	bool bOldFtrLast = false;
	bool bOldBools[6];
	UT_sint32 i = 0;
	for(i=0; i<6; i++)
	{
		bOldBools[i] = false;
	}
	if(nullptr != pDSL->getHeader())
	{
		bOldHdr = true;
	}
	if(nullptr != pDSL->getHeaderEven())
	{
		bOldHdrEven = true;
		bOldBools[AP_Dialog_HdrFtr::HdrEven] = true;
	}
	if(nullptr != pDSL->getHeaderFirst())
	{
		bOldHdrFirst = true;
		bOldBools[AP_Dialog_HdrFtr::HdrFirst] = true;
	}
	if(nullptr != pDSL->getHeaderLast())
	{
		bOldHdrLast = true;
		bOldBools[AP_Dialog_HdrFtr::HdrLast] = true;
	}
	if(nullptr != pDSL->getFooter())
	{
		bOldFtr = true;
	}
	if(nullptr != pDSL->getFooterEven())
	{
		bOldFtrEven = true;
		bOldBools[AP_Dialog_HdrFtr::FtrEven] = true;
	}
	if(nullptr != pDSL->getFooterFirst())
	{
		bOldFtrFirst = true;
		bOldBools[AP_Dialog_HdrFtr::FtrFirst] = true;
	}
	if(nullptr != pDSL->getFooterLast())
	{
		bOldFtrLast = true;
		bOldBools[AP_Dialog_HdrFtr::FtrLast] = true;
	}
	for(i =0; i < 6; i++)
	{
		pDialog->setValue((AP_Dialog_HdrFtr::HdrFtr_Control) i,
						  bOldBools[i], false);
	}
	PP_PropertyVector propsSectionIn;
	pView->getSectionFormat(propsSectionIn);
	const std::string & restart = PP_getAttribute("section-restart", propsSectionIn);
	const std::string & restartValue1 =
		PP_getAttribute("section-restart-value", propsSectionIn);
	bool bRestart = restart == "1";
	UT_sint32 restartValue = 1;
	if(!restartValue1.empty())
	{
		restartValue = atoi(restartValue1.c_str());
	}
	pDialog->setRestart(bRestart, restartValue, false);

	pDialog->runModal (pFrame);

	AP_Dialog_HdrFtr::tAnswer ans = pDialog->getAnswer();
	bool bOK = (ans == AP_Dialog_HdrFtr::a_OK);

	if (bOK)
	{
		// let the view set the proper value in the
		// document and refresh/redraw itself
//
// Read back hdr/ftr type changes
//
		bool bNewHdrEven = pDialog->getValue(AP_Dialog_HdrFtr::HdrEven);
		bool bNewHdrFirst = pDialog->getValue(AP_Dialog_HdrFtr::HdrFirst);
		bool bNewHdrLast = pDialog->getValue(AP_Dialog_HdrFtr::HdrLast);
		bool bNewFtrEven = pDialog->getValue(AP_Dialog_HdrFtr::FtrEven);
		bool bNewFtrFirst = pDialog->getValue(AP_Dialog_HdrFtr::FtrFirst);
		bool bNewFtrLast = pDialog->getValue(AP_Dialog_HdrFtr::FtrLast);
//
// Save everything from the PieceTable we need.
//
		pView->SetupSavePieceTableState();
//
// Now delete the header/footers that need to be deleted.
//

		if(bOldHdrEven && !bNewHdrEven)
		{
			pView->removeThisHdrFtr(FL_HDRFTR_HEADER_EVEN);
		}
		if(bOldHdrFirst && !bNewHdrFirst)
		{
			pView->removeThisHdrFtr(FL_HDRFTR_HEADER_FIRST);
		}
		if(bOldHdrLast && !bNewHdrLast)
		{
			pView->removeThisHdrFtr(FL_HDRFTR_HEADER_LAST);
		}
		if(bOldFtrEven && !bNewFtrEven)
		{
			pView->removeThisHdrFtr(FL_HDRFTR_FOOTER_EVEN);
		}
		if(bOldHdrFirst && !bNewHdrFirst)
		{
			pView->removeThisHdrFtr(FL_HDRFTR_FOOTER_FIRST);
		}
		if(bOldHdrLast && !bNewHdrLast)
		{
			pView->removeThisHdrFtr(FL_HDRFTR_FOOTER_LAST);
		}
//
// Now create odd header/footers if there are none and any other Header/Footer
// types are asked for
//
		if(!bOldHdr && (bNewHdrEven || bNewHdrFirst || bNewHdrLast))
		{
			pView->createThisHdrFtr(FL_HDRFTR_HEADER);
		}
		if(!bOldFtr && (bNewFtrEven || bNewFtrFirst || bNewFtrLast))
		{
			pView->createThisHdrFtr(FL_HDRFTR_FOOTER);
		}
//
// OK now create and populate the  requested header/footer types
//
		if(bNewHdrEven && !bOldHdrEven)
		{
			pView->createThisHdrFtr(FL_HDRFTR_HEADER_EVEN);
			pView->populateThisHdrFtr(FL_HDRFTR_HEADER_EVEN);
		}
		if(bNewHdrFirst && !bOldHdrFirst)
		{
			pView->createThisHdrFtr(FL_HDRFTR_HEADER_FIRST);
			pView->populateThisHdrFtr(FL_HDRFTR_HEADER_FIRST);
		}
		if(bNewHdrLast && !bOldHdrLast)
		{
			pView->createThisHdrFtr(FL_HDRFTR_HEADER_LAST);
			pView->populateThisHdrFtr(FL_HDRFTR_HEADER_LAST);
		}
		if(bNewFtrEven && !bOldFtrEven)
		{
			pView->createThisHdrFtr(FL_HDRFTR_FOOTER_EVEN);
			pView->populateThisHdrFtr(FL_HDRFTR_FOOTER_EVEN);
		}
		if(bNewFtrFirst && !bOldFtrFirst)
		{
			pView->createThisHdrFtr(FL_HDRFTR_FOOTER_FIRST);
			pView->populateThisHdrFtr(FL_HDRFTR_FOOTER_FIRST);
		}
		if(bNewFtrLast && !bOldFtrLast)
		{
			pView->createThisHdrFtr(FL_HDRFTR_FOOTER_LAST);
			pView->populateThisHdrFtr(FL_HDRFTR_FOOTER_LAST);
		}


		pView->RestoreSavedPieceTableState();
		if(pDialog->isRestartChanged())
		{
			PP_PropertyVector props_out = {
					"section-restart", "",
					"section-restart-value", ""
			};
			if(pDialog->isRestart())
			{
				props_out[1] = "1";
				props_out[3] = UT_std_string_sprintf("%i", pDialog->getRestartValue());
			}
			else
			{
				props_out[1] = "0";
				props_out[3] = "";
			}
			pView->setSectionFormat(props_out);
		}
		pView->notifyListeners(AV_CHG_ALL);
	}

	pDialogFactory->releaseDialog(pDialog);
	return bOK;
}

Defun1(hyperlinkCopyLocation)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	pView->cmdHyperlinkCopyLocation(pView->getPoint());
	return true;
}

Defun(hyperlinkJump)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	
	fp_Run * pRun = pView->getHyperLinkRun(pView->getPoint());
	fp_HyperlinkRun * pHRun = nullptr;
	if(pRun)
		pHRun = pRun->getHyperlink();
	
	if(pHRun && pHRun->getHyperlinkType() == HYPERLINK_NORMAL)
	{
		UT_DEBUGMSG(("hyperlinkJump: Normal hyperlink jump\n"));
		pView->cmdHyperlinkJump(pCallData->m_xPos, pCallData->m_yPos);
	}
	
	if(pHRun && pHRun->getHyperlinkType() == HYPERLINK_ANNOTATION)
	{
		// This is the behaveour when double clicking an annotation hypermark
		UT_DEBUGMSG(("hyperlinkJump: Hyperlink annotation (no jump) edit dialog\n"));
		fp_AnnotationRun * pARun = static_cast<fp_AnnotationRun *>(pHRun);
		pView->cmdEditAnnotationWithDialog(pARun->getPID());
	}
	
	return true;
}


Defun1(hyperlinkJumpPos)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	UT_DEBUGMSG(("hyperlinkJumpPos\n"));
	pView->cmdHyperlinkJump(pView->getPoint());
	return true;
}

Defun1(rdfAnchorEditTriples)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	XAP_Dialog_Id id = (XAP_Dialog_Id)AP_DIALOG_ID_RDF_EDITOR;
	AP_Dialog_RDFEditor* dialog = nullptr;
	return s_doRDFEditorDlg( pView, id, dialog, true );
}
Defun1(rdfAnchorQuery)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	return rdfQueryXMLIDs(pView, nullptr);
}

Defun1(rdfAnchorEditSemanticItem)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	if( PD_Document * pDoc = pView->getDocument() )
	{
		if( PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF() )
		{
			std::set< std::string > xmlids;
			rdf->addRelevantIDsForPosition( xmlids, pView->getPoint() );

			PD_RDFSemanticItems sl = rdf->getSemanticObjects( xmlids );
			rdf->showEditorWindow( sl );
			
			
			// PD_RDFContacts contacts = rdf->getContacts();
			// for( PD_RDFContacts::iterator ci = contacts.begin();
			// 	 ci != contacts.end(); ++ci )
			// {
			// 	PD_RDFContactHandle c = *ci;
			// 	std::set< std::string > clist = c->getXMLIDs();
			// 	std::set< std::string > tmp;
			// 	std::set_intersection( xmlids.begin(), xmlids.end(),
			// 						   clist.begin(), clist.end(),
			// 						   inserter( tmp, tmp.end() ));
			// 	if( !tmp.empty() )
			// 		c->showEditorWindow(c);
			// }

			// PD_RDFEvents events = rdf->getEvents();
			// for( PD_RDFEvents::iterator ci = events.begin();
			// 	 ci != events.end(); ++ci )
			// {
			// 	PD_RDFEventHandle c = *ci;
			// 	std::set< std::string > clist = c->getXMLIDs();
			// 	std::set< std::string > tmp;
			// 	std::set_intersection( xmlids.begin(), xmlids.end(),
			// 						   clist.begin(), clist.end(),
			// 						   inserter( tmp, tmp.end() ));
			// 	if( !tmp.empty() )
			// 		c->showEditorWindow(c);
			// }


			// PD_RDFLocations locations = rdf->getLocations();
			// for( PD_RDFLocations::iterator ci = locations.begin();
			// 	 ci != locations.end(); ++ci )
			// {
			// 	PD_RDFLocationHandle c = *ci;
			// 	std::set< std::string > clist = c->getXMLIDs();
			// 	std::set< std::string > tmp;
			// 	std::set_intersection( xmlids.begin(), xmlids.end(),
			// 						   clist.begin(), clist.end(),
			// 						   inserter( tmp, tmp.end() ));
			// 	UT_DEBUGMSG(("location name:%s linksubj:%s c->xmlids.sz:%d tmp.sz:%d\n", c->name().c_str(), c->linkingSubject().toString().c_str(), c->getXMLIDs().size(), tmp.size() ));
			// 	if( !tmp.empty() )
			// 	{
			// 		c->showEditorWindow(c);
			// 	}
			// }
		}
	}
	return 0;
}

Defun1(rdfAnchorExportSemanticItem)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	if( PD_Document * pDoc = pView->getDocument() )
	{
		if( PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF() )
		{
			std::set< std::string > xmlids;
			rdf->addRelevantIDsForPosition( xmlids, pView->getPoint() );

			if( xmlids.empty() )
				return 0;

			std::string filename = "";
			PD_RDFSemanticItems sl = rdf->getSemanticObjects( xmlids );
			for( PD_RDFSemanticItems::iterator ci = sl.begin();
				 ci != sl.end(); ++ci )
			{
				PD_RDFSemanticItemHandle h = *ci;
				std::set< std::string > clist = h->getXMLIDs();
				std::set< std::string > tmp;
				std::set_intersection( xmlids.begin(), xmlids.end(),
									   clist.begin(), clist.end(),
									   std::inserter( tmp, tmp.end() ));
				if( !tmp.empty() )
				{
					h->exportToFile();
				}
				
			}
			
			
			// rdf->addRelevantIDsForPosition( xmlids, pView->getPoint() );
			// PD_RDFContacts contacts = rdf->getContacts();
			// for( PD_RDFContacts::iterator ci = contacts.begin();
			// 	 ci != contacts.end(); ++ci )
			// {
			// 	PD_RDFContactHandle c = *ci;
			// 	std::set< std::string > clist = c->getXMLIDs();
			// 	std::set< std::string > tmp;
			// 	std::set_intersection( xmlids.begin(), xmlids.end(),
			// 						   clist.begin(), clist.end(),
			// 						   inserter( tmp, tmp.end() ));
			// 	if( !tmp.empty() )
			// 		c->exportToFile();
			// }
			// PD_RDFEvents events = rdf->getEvents();
			// for( PD_RDFEvents::iterator ci = events.begin();
			// 	 ci != events.end(); ++ci )
			// {
			// 	PD_RDFEventHandle c = *ci;
			// 	std::set< std::string > clist = c->getXMLIDs();
			// 	std::set< std::string > tmp;
			// 	std::set_intersection( xmlids.begin(), xmlids.end(),
			// 						   clist.begin(), clist.end(),
			// 						   inserter( tmp, tmp.end() ));
			// 	if( !tmp.empty() )
			// 		c->exportToFile();
			// }
			// PD_RDFLocations locations = rdf->getLocations();
			// for( PD_RDFLocations::iterator ci = locations.begin();
			// 	 ci != locations.end(); ++ci )
			// {
			// 	PD_RDFLocationHandle c = *ci;
			// 	std::set< std::string > clist = c->getXMLIDs();
			// 	std::set< std::string > tmp;
			// 	std::set_intersection( xmlids.begin(), xmlids.end(),
			// 						   clist.begin(), clist.end(),
			// 						   inserter( tmp, tmp.end() ));
			// 	if( !tmp.empty() )
			// 		c->exportToFile();
			// }
		}
	}
	return 0;
}



    struct selectReferenceToSemanticItemRing
	{
		PD_RDFSemanticItemHandle h;
		std::set< std::string > xmlids;
		std::set< std::string >::iterator iter;
	};

	static selectReferenceToSemanticItemRing& getSelectReferenceToSemanticItemRing()
	{
		static selectReferenceToSemanticItemRing ring;
		return ring;
	}

static void setSemanticItemRing( PD_DocumentRDFHandle rdf,
								 PD_RDFSemanticItemHandle h,
								 const std::set< std::string >& xmlids,
								 const std::string& xmlid )
{
	selectReferenceToSemanticItemRing& ring = getSelectReferenceToSemanticItemRing();

	ring.h = h;
	ring.xmlids = xmlids;
	for( std::set< std::string >::iterator ri = ring.xmlids.begin();
		 ri != ring.xmlids.end(); )
	{
		std::set< std::string >::iterator t = ri;
		++ri;
		std::pair< PT_DocPosition, PT_DocPosition > range = rdf->getIDRange( *t );
		if( !range.first || range.second <= range.first )
			ring.xmlids.erase( t );
	}
	ring.iter   = ring.xmlids.find( xmlid );	
}


static void rdfAnchorSelectPos( FV_View* pView,
								PD_DocumentRDFHandle rdf,
								PT_DocPosition pos,
								bool selectit = true )
{
	UT_DEBUGMSG(("rdfAnchorSelectPos() pos:%ld\n", (long)pos ));
	selectReferenceToSemanticItemRing& ring = getSelectReferenceToSemanticItemRing();
	ring.h.reset();
	ring.xmlids.clear();
	ring.iter = ring.xmlids.end();

	std::set< std::string > xmlids;
	rdf->addRelevantIDsForPosition( xmlids, pos );
	PD_RDFSemanticItems semitems = rdf->getSemanticObjects( xmlids );
	for( PD_RDFSemanticItems::iterator si = semitems.begin(); si != semitems.end(); ++si )
	{
		PD_RDFSemanticItemHandle c = *si;
		std::set< std::string > clist = c->getXMLIDs();
		for( std::set< std::string >::iterator clistiter = clist.begin();
			 clistiter != clist.end(); ++clistiter )
		{
			std::string xmlid = *clistiter;
			UT_DEBUGMSG(("rdfAnchorSelectPos() xmlid:%s\n", xmlid.c_str() ));
						
			std::pair< PT_DocPosition, PT_DocPosition > range = rdf->getIDRange( xmlid );
			if( range.first && range.second > range.first )
			{
				UT_DEBUGMSG(("rdfAnchorSelectPos() has range...\n" ));
				if( range.first <= pos && pos <= range.second )
				{
					UT_DEBUGMSG(("rdfAnchorSelectPos() contains point...\n" ));
					setSemanticItemRing( rdf, c, clist, xmlid );
					
					// ring.h = c;
					// ring.xmlids = clist;
					// for( std::set< std::string >::iterator ri = ring.xmlids.begin();
					// 	 ri != ring.xmlids.end(); )
					// {
					// 	std::set< std::string >::iterator t = ri;
					// 	++ri;
					// 	std::pair< PT_DocPosition, PT_DocPosition > range = rdf->getIDRange( *t );
					// 	if( !range.first || range.second <= range.first )
					// 		ring.xmlids.erase( t );
					// }
					// ring.iter   = ring.xmlids.find( xmlid );

					if( selectit )
						pView->selectRange( range );
					return;
				}
			}
		}
	}
	UT_DEBUGMSG(("rdfAnchorSelectPos() FAILED...\n" ));
}

//
// If they have started at another sem item, adapt to their choice
// Note that this can alter the ring.iter
//
static bool rdfAnchorContainsPoint( FV_View* pView,
									PD_DocumentRDFHandle rdf,
									PT_DocPosition pos )
{
	selectReferenceToSemanticItemRing& ring = getSelectReferenceToSemanticItemRing();

	std::set< std::string > xmlids;
	rdf->addRelevantIDsForPosition( xmlids, pos );
	std::set< std::string > tmp;
	std::set_intersection( xmlids.begin(), xmlids.end(),
						   ring.xmlids.begin(), ring.xmlids.end(),
						   std::inserter( tmp, tmp.end() ));
	UT_DEBUGMSG(("rdfAnchorContainsPoint() pos:%ld xmlids.sz:%ld tmp.sz:%ld\n",
				 (long)pos, (long)xmlids.size(), (long)tmp.size() ));
	if( tmp.empty() )
	{
		//
		// nothing for the cursor position is in the cached ring state!
		//
		UT_DEBUGMSG(("rdfAnchorContainsPoint() resyncing\n"));
		rdfAnchorSelectPos( pView, rdf, pos, false );
		return false;
	}
	return true;
}

	
Defun1(rdfAnchorSelectThisReferenceToSemanticItem)
{
	selectReferenceToSemanticItemRing& ring = getSelectReferenceToSemanticItemRing();
	ring.h.reset();
	ring.xmlids.clear();
	ring.iter = ring.xmlids.end();
	
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	if( PD_Document * pDoc = pView->getDocument() )
	{
		if( PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF() )
		{
			rdfAnchorSelectPos( pView, rdf, pView->getPoint(), true );
		}
	}
	return 0;
}

Defun1(rdfAnchorSelectNextReferenceToSemanticItem)
{
	selectReferenceToSemanticItemRing& ring = getSelectReferenceToSemanticItemRing();
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	if( PD_Document * pDoc = pView->getDocument() )
	{
		if( PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF() )
		{
			// If they have started at another sem item, adapt to their choice
			bool wasContained = rdfAnchorContainsPoint( pView, rdf, pView->getPoint()-1 );

			if( ring.iter == ring.xmlids.end() )
			{
				UT_DEBUGMSG((" selectNext(1) iter == end\n" ));
				return 0;
			}
			ring.iter++;

			UT_DEBUGMSG((" selectNext(2) iter == end:%d wasc:%d\n", ring.iter == ring.xmlids.end(), wasContained));
			// the iter was resyned and there is no next.
			if( ring.iter == ring.xmlids.end() && !wasContained )
				ring.iter--;

			if( ring.iter != ring.xmlids.end() )
			{
				std::string xmlid = *ring.iter;
				UT_DEBUGMSG((" selectNext() xmlid:%s\n", xmlid.c_str() ));
				std::pair< PT_DocPosition, PT_DocPosition > range = rdf->getIDRange( xmlid );
				if( range.first && range.second > range.first )
					pView->selectRange( range );
			}
		}
	}
	
	return 0;
}

Defun1(rdfAnchorSelectPrevReferenceToSemanticItem)
{
	selectReferenceToSemanticItemRing& ring = getSelectReferenceToSemanticItemRing();
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	if( PD_Document * pDoc = pView->getDocument() )
	{
		if( PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF() )
		{
			bool wasContained = rdfAnchorContainsPoint( pView, rdf, pView->getPoint()-1 );
	
			if( ring.iter == ring.xmlids.begin() )
			{
				UT_DEBUGMSG((" selectPrev() resetting iter to end()\n" ));
				ring.iter = ring.xmlids.end();
			}
			if( ring.iter == ring.xmlids.end() )
			{
				UT_DEBUGMSG((" selectPrev() iter IS end()\n" ));
				if( wasContained )
					return 0;
				
				// if we resynced, and there is no prev, then select the first one.
				UT_DEBUGMSG((" selectPrev() set iter to the first item due to resync...\n" ));
				ring.iter = ring.xmlids.begin();
				ring.iter++;
			}
	
			ring.iter--;
	
			std::string xmlid = *ring.iter;
			UT_DEBUGMSG((" selectPrev() xmlid:%s\n", xmlid.c_str() ));
			
			std::pair< PT_DocPosition, PT_DocPosition > range = rdf->getIDRange( xmlid );
			if( range.first && range.second > range.first )
				pView->selectRange( range );
		}
	}
	
	return 0;
}


static PD_RDFSemanticItemHandle& getrdfSemitemSource()
{
	static PD_RDFSemanticItemHandle ret;
	return ret;
}

Defun1(rdfSemitemSetAsSource)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PD_Document * pDoc = pView->getDocument();
	PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF();
	std::set< std::string > xmlids;
	rdf->addRelevantIDsForPosition( xmlids, pView->getPoint() );
    PD_RDFSemanticItems sl = rdf->getSemanticObjects( xmlids );
	if( sl.empty() )
		return false;
	
	PD_RDFSemanticItemHandle si = *(sl.begin());
	getrdfSemitemSource() = si;
	return true;
}

Defun1(rdfSemitemFindRelatedFoafKnows)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PD_Document * pDoc = pView->getDocument();
	PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF();

	std::set< std::string > xmlids;
	rdf->addRelevantIDsForPosition( xmlids, pView->getPoint() );
	UT_DEBUGMSG(("rdfSemitemFindRelatedFoafKnows(a) point->xmlids.sz:%ld\n", (long)xmlids.size() ));
	if( xmlids.empty() )
		rdf->addRelevantIDsForPosition( xmlids, pView->getPoint()-1 );
		
    PD_RDFSemanticItems sl = rdf->getSemanticObjects( xmlids );
	if( sl.empty() )
		return false;
	PD_RDFSemanticItemHandle src = *(sl.begin());
	UT_DEBUGMSG(("rdfSemitemFindRelatedFoafKnows(b) point->xmlids.sz:%ld\n", (long)xmlids.size() ));
	UT_DEBUGMSG(("rdfSemitemFindRelatedFoafKnows() point->sl.sz:%ld\n", (long)sl.size() ));
	for( PD_RDFSemanticItems::iterator iter = sl.begin(); iter != sl.end(); ++iter )
	{
		PD_RDFSemanticItemHandle si = *iter;
		UT_DEBUGMSG(("rdfSemitemFindRelatedFoafKnows() point->si:%s\n", si->name().c_str() ));
	}
	
	PD_RDFSemanticItems related = src->relationFind( PD_RDFSemanticItem::RELATION_FOAF_KNOWS );
	for( PD_RDFSemanticItems::iterator iter = related.begin(); iter != related.end(); ++iter )
	{
		PD_RDFSemanticItemHandle si = *iter;
		xmlids = si->getXMLIDs();
		for( std::set< std::string >::iterator xi = xmlids.begin(); xi != xmlids.end(); ++xi )
		{
			std::string xmlid = *xi;
			std::pair< PT_DocPosition, PT_DocPosition > p = rdf->getIDRange( xmlid );
			if( p.first && p.first != p.second )
			{
				setSemanticItemRing( rdf, si, xmlids, xmlid );
				PD_RDFSemanticItemViewSite vs( si, xmlid );
				vs.select( pView );
				return true;
			}
		}
	}
	
	return true;
}

Defun1(rdfSemitemRelatedToSourceFoafKnows)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PD_Document * pDoc = pView->getDocument();
	PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF();
	PD_RDFSemanticItemHandle src = getrdfSemitemSource();
	std::set< std::string > xmlids;
	rdf->addRelevantIDsForPosition( xmlids, pView->getPoint() );
    PD_RDFSemanticItems sl = rdf->getSemanticObjects( xmlids );
	if( sl.empty() )
		return false;

	for( PD_RDFSemanticItems::iterator iter = sl.begin(); iter != sl.end(); ++iter )
	{
		PD_RDFSemanticItemHandle si = *iter;
		src->relationAdd( si, PD_RDFSemanticItem::RELATION_FOAF_KNOWS );
	}
	
	return true;
}

Defun1(rdfApplyCurrentStyleSheet)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PD_Document * pDoc = pView->getDocument();
	PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF();

	std::set< std::string > xmlids;
	rdf->addRelevantIDsForPosition( xmlids, pView->getPoint() );
    PD_RDFSemanticItems sl = rdf->getSemanticObjects( xmlids );
	for( PD_RDFSemanticItems::iterator iter = sl.begin(); iter != sl.end(); ++iter )
	{
		PD_RDFSemanticItemHandle si = *iter;
		PD_RDFSemanticItemViewSite vs( si, pView->getPoint() );
		vs.reflowUsingCurrentStylesheet( pView );
	}	
	return true;
}

Defun1(rdfStylesheetSettings)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PD_Document * pDoc = pView->getDocument();
	PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF();

	runSemanticStylesheetsDialog( pView );
	
	return true;
}

Defun1(rdfDisassocateCurrentStyleSheet)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PD_Document * pDoc = pView->getDocument();
	PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF();

	std::set< std::string > xmlids;
	rdf->addRelevantIDsForPosition( xmlids, pView->getPoint() );
    PD_RDFSemanticItems sl = rdf->getSemanticObjects( xmlids );
	for( PD_RDFSemanticItems::iterator iter = sl.begin(); iter != sl.end(); ++iter )
	{
		PD_RDFSemanticItemHandle si = *iter;
		PD_RDFSemanticItemViewSite vs( si, pView->getPoint() );
		vs.disassociateStylesheet();
		vs.reflowUsingCurrentStylesheet( pView );
	}	
	return true;
}

static void _rdfApplyStylesheet( FV_View* pView, std::string stylesheetName, PT_DocPosition pos )
{
	PD_Document * pDoc = pView->getDocument();
	PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF();

	std::set< std::string > xmlids;
	rdf->addRelevantIDsForPosition( xmlids, pos );
    PD_RDFSemanticItems sl = rdf->getSemanticObjects( xmlids );
	if( sl.empty() )
		return;

	for( PD_RDFSemanticItems::iterator iter = sl.begin(); iter != sl.end(); ++iter )
	{
		PD_RDFSemanticItemHandle si = *iter;
		PD_RDFSemanticStylesheetHandle ss =
			si->findStylesheetByName( PD_RDFSemanticStylesheet::stylesheetTypeSystem(),
									  stylesheetName );
		if( !ss )
			continue;
		
		PD_RDFSemanticItemViewSite vs( si, pos );
		vs.applyStylesheet( pView, ss );
		return;
	}	
}


Defun1(rdfApplyStylesheetContactName)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	_rdfApplyStylesheet( pView, RDF_SEMANTIC_STYLESHEET_CONTACT_NAME, pView->getPoint() );
	return true;
}

Defun1(rdfApplyStylesheetContactNameHomepagePhone)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	_rdfApplyStylesheet( pView, RDF_SEMANTIC_STYLESHEET_CONTACT_NAME_HOMEPAGE_PHONE, pView->getPoint() );
	return true;
}

Defun1(rdfApplyStylesheetContactNamePhone)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	_rdfApplyStylesheet( pView, RDF_SEMANTIC_STYLESHEET_CONTACT_NAME_PHONE, pView->getPoint() );
	return true;
}

Defun1(rdfApplyStylesheetContactNick)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	_rdfApplyStylesheet( pView, RDF_SEMANTIC_STYLESHEET_CONTACT_NICK, pView->getPoint() );
	return true;
}

Defun1(rdfApplyStylesheetContactNickPhone)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	_rdfApplyStylesheet( pView, RDF_SEMANTIC_STYLESHEET_CONTACT_NICK_PHONE, pView->getPoint() );
	return true;
}

Defun1(rdfApplyStylesheetEventName)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	_rdfApplyStylesheet( pView, RDF_SEMANTIC_STYLESHEET_EVENT_NAME, pView->getPoint() );
	return true;
}

Defun1(rdfApplyStylesheetEventSummary)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	_rdfApplyStylesheet( pView, RDF_SEMANTIC_STYLESHEET_EVENT_SUMMARY, pView->getPoint() );
	return true;
}

Defun1(rdfApplyStylesheetEventSummaryLocation)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	_rdfApplyStylesheet( pView, RDF_SEMANTIC_STYLESHEET_EVENT_SUMMARY_LOCATION, pView->getPoint() );
	return true;
}

Defun1(rdfApplyStylesheetEventSummaryLocationTimes)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	_rdfApplyStylesheet( pView, RDF_SEMANTIC_STYLESHEET_EVENT_SUMMARY_LOCATION_TIMES, pView->getPoint() );
	return true;
}

Defun1(rdfApplyStylesheetEventSummaryTimes)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	_rdfApplyStylesheet( pView, RDF_SEMANTIC_STYLESHEET_EVENT_SUMMARY_TIMES, pView->getPoint() );
	return true;
}

Defun1(rdfApplyStylesheetLocationLatLong)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	_rdfApplyStylesheet( pView, RDF_SEMANTIC_STYLESHEET_LOCATION_NAME_LATLONG, pView->getPoint() );
	return true;
}

Defun1(rdfApplyStylesheetLocationName)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	_rdfApplyStylesheet( pView, RDF_SEMANTIC_STYLESHEET_LOCATION_NAME, pView->getPoint() );
	return true;
}

Defun1(deleteHyperlink)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	pView->cmdDeleteHyperlink();
	return true;
}

Defun(hyperlinkStatusBar)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);

	if( pView->bubblesAreBlocked() )
	{
		UT_DEBUGMSG(("hyperlinkStatusBar() bubbles are blocked, not opening one right now\n" ));
		return true;	
	}
	
	GR_Graphics * pG = pView->getGraphics();
	if (pG)
		pG->setCursor(GR_Graphics::GR_CURSOR_LINK);

	UT_sint32 xpos = pCallData->m_xPos;
	UT_sint32 ypos = pCallData->m_yPos;
	PT_DocPosition  pos = pView->getDocPositionFromXY(xpos,ypos);
	fp_HyperlinkRun * pHRun = static_cast<fp_HyperlinkRun *>(pView->getHyperLinkRun(pos));
	if(!pHRun)
		return false;
	UT_DEBUGMSG(("hyperlinkStatusBar() pHRun:%p\n", (void*)pHRun));
	UT_DEBUGMSG(("hyperlinkStatusBar()  type:%d\n", (int)pHRun->getHyperlinkType()));
	if(pHRun->getHyperlinkType() == HYPERLINK_NORMAL)
	{
			pView->cmdHyperlinkStatusBar(xpos, ypos);
			return true;
	}

	UT_uint32 pid = 0;
	std::string sText;
	if( fp_AnnotationRun * pAnn = dynamic_cast<fp_AnnotationRun *>(pHRun) )
	{
		UT_DEBUGMSG(("hyperlinkStatusBar() have annotation...\n" ));
		pid = pAnn->getPID();
		UT_DebugOnly<bool> b = pView->getAnnotationText( pid, sText );
		UT_ASSERT(b);
	}
	else if( fp_RDFAnchorRun * pAnchorRun = dynamic_cast<fp_RDFAnchorRun *>(pHRun) )
	{
		UT_DEBUGMSG(("hyperlinkStatusBar() have RDF anchor!\n" ));
		pid = pAnchorRun->getPID();
		std::string xmlid = pAnchorRun->getXMLID();
		std::stringstream ss;
		ss << "xmlid:" << xmlid;
		if( PD_Document * pDoc = pView->getDocument() )
		{
			if( PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF() )
			{
				PD_RDFModelHandle m = rdf->getRDFForID( xmlid );
				ss << " triple count:" << m->getTripleCount();
#if DEBUG
				std::pair< PT_DocPosition, PT_DocPosition > range = rdf->getIDRange( xmlid );
				ss << " start:" << range.first << " end:" << range.second;
#endif
			}
		}
		ss << " ";
		sText = ss.str();
	}
	
	// avoid unneeded redrawings
	// check BOTH if we are already previewing an annotation, and that it is indeed the annotation we want
	if((pView->isAnnotationPreviewActive()) &&
	   (pView->getActivePreviewAnnotationID() == pid ))
	{
		xxx_UT_DEBUGMSG(("hyperlinkStatusBar: nothing to draw, annotation already previewed\n"));
		return true; // should be false? think not
	}
	
	// kill previous preview if needed (it is not the same annotation as it would have been detected above)
	if (pView->isAnnotationPreviewActive())
	{
		UT_DEBUGMSG(("hyperlinkStatusBar: Deleting previous annotation preview...\n"));
		pView->killAnnotationPreview();
	}
	
	std::string sTitle;
	std::string sAuthor;
	if(pHRun->getHyperlinkType() == HYPERLINK_ANNOTATION && sText.empty() )
	{
		UT_DEBUGMSG(("hyperlinkStatusBar: exiting because we have no annotation text for pid:%d\n", pid));
		return false;
	}
	
	// Optional fields
	pView->getAnnotationTitle( pid, sTitle );
	pView->getAnnotationAuthor( pid, sAuthor );
	
	// preview annotation

	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pView->getParentData());
	UT_return_val_if_fail(pFrame, false);
	
	// PLEASE DOOOON'T UNCOMMENT THIS EVIL LINE (unexpectedly will hide the pop-up)
	//pFrame->raise();

	// Annotation Preview windows are per frame!

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Preview_Annotation * pAnnPview
		= static_cast<AP_Preview_Annotation *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_ANNOTATION_PREVIEW));

	if(!pAnnPview)
		return false;
		
	UT_DEBUGMSG(("hyperlinkStatusBar: Previewing annotation text %s \n",sText.c_str()));
	
	// flags
	pView->setAnnotationPreviewActive(true);
	// this call is also needed to decide when to redraw the preview
	pView->setActivePreviewAnnotationID( pid ); 
	
	// Fields
	pAnnPview->setDescription(sText);
	
	// Optional fields
	// if those fields are to be hidden it should be at the GUI level (inside AP_Preview_Annotation)
	pAnnPview->setTitle(sTitle);	
	pAnnPview->setAuthor(sAuthor);
	
	fp_Line * pLine = pHRun->getLine();
	if(pLine)
	{
		UT_Rect pRect = pLine->getScreenRect().value();
		UT_sint32 ioff = pRect.top;
		pAnnPview->setOffset(pG->tdu(ypos - ioff));
	}
	pAnnPview->setXY(pG->tdu(xpos),pG->tdu(ypos));
	pAnnPview->runModeless(pFrame);
	
	//UT_sint32 xoff = 0, yoff = 0;
	//fp_Run * pRun = pView->getHyperLinkRun(pos);
	//pHRun->getLine()->getOffsets(pHRun, xoff, yoff); //TODO try getting container's screen offset... ->getContainer()
	// Sevior's infamous + 1....
	//yoff += pHRun->getLine()->getAscent() - pHRun->getAscent() + 1;
	UT_DEBUGMSG(("hyperlinkStatusBar: xypos %d %d\n",xpos,ypos));
	UT_DEBUGMSG(("hyperlinkStatusBar: setXY %d %d\n",pG->tdu(xpos),pG->tdu(ypos)));
	//UT_DEBUGMSG(("hyperlinkStatusBar: pRungetxy %d %d\n",pHRun->getX(),pHRun->getY()));
	//UT_DEBUGMSG(("hyperlinkStatusBar: getScreenOffsets %d %d\n",xoff,yoff));
	
	pAnnPview->queueDraw();
	
	return true;	
}

static bool s_doMarkRevisions(XAP_Frame * pFrame, PD_Document * pDoc, FV_View * pView,
							  bool bToggleMark, bool bForceNew)
{
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_MarkRevisions * pDialog
		= static_cast<AP_Dialog_MarkRevisions *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_MARK_REVISIONS));
UT_return_val_if_fail(pDialog, false);
	pDialog->setDocument(pDoc);

	if(bForceNew)
		pDialog->forceNew();
	
	pDialog->runModal(pFrame);
	bool bOK = (pDialog->getAnswer() == AP_Dialog_MarkRevisions::a_OK);

	if (!bOK && bToggleMark)
	{
		// we have already turned this on, so turn it off again
		pView->toggleMarkRevisions();
	}
	else if(bOK)
	{
		pDialog->addRevision();
#if 0
		// cannot remember at all why I thought this was needed and it has been marked as
		// bug 7700, so I am going to disable this. Tomas, May 10, 2005
		
		// we also want to have paragraph marks and etc visible
		AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
		UT_return_val_if_fail(pFrameData, false);
		if(!pFrameData->m_bShowPara)
		{
			pFrameData->m_bShowPara = true;
			pView->setShowPara(true);
			pView->notifyListeners(AV_CHG_FRAMEDATA);	// to update toolbar
		}
#endif
	}


	pDialogFactory->releaseDialog(pDialog);

	return bOK;
}


Defun1(toggleAutoRevision)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail(pDoc,false);
	
	bool bAuto = !pDoc->isAutoRevisioning();
	bool bDoIT = true;
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame,false);
	if(!bAuto)
	{
		// the user asked to turn revisioning off; this would disrupt
		// the record of changes, making it impossible to revert
		// reliably to any earlier versions of document history
		// we issue worning
		
		bDoIT = (XAP_Dialog_MessageBox::a_YES ==
				        pFrame->showMessageBox(AP_STRING_ID_MSG_AutoRevisionOffWarning, 
											   XAP_Dialog_MessageBox::b_YN, 
											   XAP_Dialog_MessageBox::a_NO));
	
	}
	if(bDoIT)
	{
//
// Get rid of the warning box before the redraw
//
		UT_sint32 i =0;
		for(i=0; i< 5;i++)
		{
			pFrame->nullUpdate();
		}
		pDoc->setAutoRevisioning(bAuto);
		pView->focusChange(AV_FOCUS_HERE);
	}
	return true;
}

Defun1(toggleMarkRevisions)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);

	if(!pView->isMarkRevisions())
	{
		// set view level to all
		pView->setRevisionLevel(0);
	}
	
	if(!pView->isMarkRevisions())
	{
		PD_Document * pDoc = pView->getDocument();
		XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
		UT_return_val_if_fail( pFrame && pDoc, false );
		
		if(s_doMarkRevisions(pFrame, pDoc, pView, false, false))
			pView->toggleMarkRevisions();
	}
	else
	{
		pView->toggleMarkRevisions();
	}
	


	
	return true;
}


Defun1(toggleShowRevisions)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	pView->toggleShowRevisions();
	return true;
}

Defun1(toggleShowRevisionsBefore)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	bool bShow = pView->isShowRevisions();
	UT_uint32 iLevel = pView->getRevisionLevel();
	
	if(bShow)
	{
		//we are asked to hide revisions, first set view level to 0
		pView->setRevisionLevel(0);
		pView->toggleShowRevisions();
	}
	else if(iLevel != 0)
	{
		// we are asked to change view level
		pView->cmdSetRevisionLevel(0);
	}
	
	return true;
}

Defun1(toggleShowRevisionsAfter)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	bool bShow = pView->isShowRevisions();
	bool bMark = pView->isMarkRevisions();
	UT_uint32 iLevel = pView->getRevisionLevel();

	if(bMark)
	{
		if(iLevel != PD_MAX_REVISION)
		{
			pView->cmdSetRevisionLevel(PD_MAX_REVISION);
		}
		else
		{
			pView->cmdSetRevisionLevel(0);
		}
	}
	else if(bShow)
	{
		//we are asked to hide revisions, first set view level to max
		pView->setRevisionLevel(PD_MAX_REVISION);
		pView->toggleShowRevisions();
	}
	else if(iLevel != PD_MAX_REVISION)
	{
		// we are asked to change view level
		pView->cmdSetRevisionLevel(PD_MAX_REVISION);
	}
	
	return true;
}

Defun1(toggleShowRevisionsAfterPrevious)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView,false);
	UT_uint32 iLevel = pView->getRevisionLevel();
	UT_uint32 iDocLevel = pView->getDocument()->getHighestRevisionId();

	if(iDocLevel == 0)
		return false;
	
	if(iLevel != iDocLevel - 1)
	{
		// we are in Mark mode and are asked to treat all revisions
		// but the present as accepted
		pView->cmdSetRevisionLevel(iDocLevel-1);
	}
	else
	{
		pView->cmdSetRevisionLevel(0);
	}
	return true;
}

Defun(revisionAccept)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	pView->cmdAcceptRejectRevision(false, pCallData->m_xPos, pCallData->m_yPos);
	return true;
}

Defun(revisionReject)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	pView->cmdAcceptRejectRevision(true, pCallData->m_xPos, pCallData->m_yPos);
	return true;
}

Defun(revisionFindNext)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	pView->cmdFindRevision(true, pCallData->m_xPos, pCallData->m_yPos);
	return true;
}

Defun(revisionFindPrev)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	pView->cmdFindRevision(false, pCallData->m_xPos, pCallData->m_yPos);
	return true;
}

static bool s_doListRevisions(XAP_Frame * pFrame, PD_Document * pDoc, FV_View * pView)
{
	UT_return_val_if_fail(pFrame, false);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	AP_Dialog_ListRevisions * pDialog
		= static_cast<AP_Dialog_ListRevisions *>(pDialogFactory->requestDialog((XAP_Dialog_Id)AP_DIALOG_ID_LIST_REVISIONS));
UT_return_val_if_fail(pDialog, false);
	pDialog->setDocument(pDoc);
	pDialog->runModal(pFrame);
	bool bOK = (pDialog->getAnswer() == AP_Dialog_ListRevisions::a_OK);

	if (bOK)
	{
		pView->cmdSetRevisionLevel(pDialog->getSelectedId());
	}


	pDialogFactory->releaseDialog(pDialog);

	return bOK;
}

Defun1(revisionSetViewLevel)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail(pDoc,false);

	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame,false);

	s_doListRevisions(pFrame, pDoc, pView);

	return true;
}

Defun(revisionNew)
{
	UT_UNUSED(pCallData);
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);

	PD_Document * pDoc = pView->getDocument();
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail( pDoc && pFrame, false );

	s_doMarkRevisions(pFrame, pDoc, pView, false, true);
	pDoc->setMarkRevisions( true );
	
	return true;
}

Defun(revisionSelect)
{
	UT_UNUSED(pCallData);
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);

	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail(pDoc,false);

	pDoc->setMarkRevisions( false );
	pView->setShowRevisions( true );
	
	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame,false);

	s_doListRevisions(pFrame, pDoc, pView);

	return true;
}


/*!
    This function can be used to raise one of the ListDocuments dialogues
    \param pFrame: the active frame
    \param bExcludeCurrent: true if current document is to be excluded
                            from the list
    \param iId: the dialogue iId determining which of the
                ListDocuments variants to raise

    \return: returns pointer to the document user selected or nullptr
*/
static PD_Document * s_doListDocuments(XAP_Frame * pFrame, bool bExcludeCurrent, XAP_Dialog_Id iId)
{
	UT_return_val_if_fail(pFrame, nullptr);

	pFrame->raise();

	XAP_DialogFactory * pDialogFactory
		= static_cast<XAP_DialogFactory *>(pFrame->getDialogFactory());

	XAP_Dialog_ListDocuments * pDialog
		= static_cast<XAP_Dialog_ListDocuments *>(pDialogFactory->requestDialog(iId));
	
	UT_return_val_if_fail(pDialog, nullptr);

	// the dialgue excludes current document by default, if we are to
	// include it, we need to tell it ...
	if(!bExcludeCurrent)
		pDialog->setIncludeActiveDoc(true);
	
	pDialog->runModal(pFrame);
	bool bOK = (pDialog->getAnswer() == XAP_Dialog_ListDocuments::a_OK);

	PD_Document *pD = nullptr;
	
	if (bOK)
	{
		pD = (PD_Document *)pDialog->getDocument();
#if DEBUG
		if(!pD)
			UT_DEBUGMSG(("DIALOG LIST DOCUMENTS: no document\n"));
		else
			UT_DEBUGMSG(("DIALOG LIST DOCUMENTS: %s\n",
						 pD->getFilename().c_str()));
#endif
	}

	pDialogFactory->releaseDialog(pDialog);

	return pD;
}

/* ------------------------------------------------------------------
   Document compare (legal blackline): the current document is the
   original, a second open document is the revised version.  A
   word-level diff is computed and emitted into a NEW document where
   every difference is marked as a revision, so the standard
   Accept/Reject tools work on the result.  Both sources are left
   untouched; the result must be saved explicitly.
   ------------------------------------------------------------------ */

struct CmpTok
{
	bool           para;
	UT_UCS4String  text;
};

static void s_flushParaTokens(const UT_UCS4String & para,
							  std::vector<CmpTok> & toks)
{
	size_t i = 0;
	const size_t n = para.size();
	while(i < n)
	{
		while(i < n && UT_UCS4_isspace(para[i]))
			++i;
		size_t j = i;
		while(j < n && !UT_UCS4_isspace(para[j]))
			++j;
		if(j > i)
		{
			CmpTok tk = {false, para.substr(i, j - i)};
			toks.push_back(tk);
		}
		i = j;
	}
	CmpTok pb = {true, UT_UCS4String()};
	toks.push_back(pb);
}

static void s_docTokens(PD_Document * pDoc, std::vector<CmpTok> & toks)
{
	UT_UCS4String para;
	PD_DocIterator t(*pDoc);
	while(t.getStatus() == UTIter_OK)
	{
		pf_Frag * pf = t.getFrag();
		if(pf && pf->getType() == pf_Frag::PFT_Strux &&
		   static_cast<pf_Frag_Strux *>(pf)->getStruxType() == PTX_Block)
		{
			s_flushParaTokens(para, toks);
			para.clear();
		}
		else if(pf && pf->getType() == pf_Frag::PFT_Text)
		{
			UT_UCS4Char c = t.getChar();
			if(c)
				para += c;
		}
		++t;
	}
	s_flushParaTokens(para, toks);
}

enum CmpOp { CMP_KEEP = 0, CMP_DEL = 1, CMP_INS = 2 };

static bool s_tokEq(const CmpTok & a, const CmpTok & b)
{
	return a.para == b.para && (a.para || a.text == b.text);
}

// limits keep the Myers trace arrays bounded on pathological inputs
#define CMP_MAX_TOKENS  6000
#define CMP_MAX_EDITS   1500

/*  Myers O(ND) diff.  On success ops holds a merged edit script:
    KEEP consumes one token of a and one of b, DEL one of a, INS one
    of b.  Returns false when the documents are too large or too
    different. */
static bool s_wordDiff(const std::vector<CmpTok> & a,
					   const std::vector<CmpTok> & b,
					   std::vector<CmpOp> & ops)
{
	const int N = (int)a.size();
	const int M = (int)b.size();
	const int max = N + M;
	if(max == 0)
		return true;
	if(max > CMP_MAX_TOKENS)
		return false;

	const int off = max;
	const int width = 2 * max + 1;
	std::vector<int> v(width, 0);
	std::vector<std::vector<int>> trace;
	int dFinal = -1;

	for(int d = 0; d <= max; ++d)
	{
		for(int k = -d; k <= d; k += 2)
		{
			int x;
			if(k == -d || (k != d && v[off + k - 1] < v[off + k + 1]))
				x = v[off + k + 1];
			else
				x = v[off + k - 1] + 1;
			int y = x - k;
			while(x < N && y < M && s_tokEq(a[x], b[y]))
			{
				++x;
				++y;
			}
			v[off + k] = x;
			if(x >= N && y >= M)
			{
				dFinal = d;
				break;
			}
		}
		trace.push_back(v);
		if(dFinal >= 0)
			break;
		if(d + 1 > CMP_MAX_EDITS)
			return false;
	}

	int x = N, y = M;
	for(int d = dFinal; d > 0; --d)
	{
		const std::vector<int> & vv = trace[d - 1];
		const int k = x - y;
		const int pk = (k == -d || (k != d && vv[off + k - 1] < vv[off + k + 1]))
				? k + 1 : k - 1;
		const int px = vv[off + pk];
		const int py = px - pk;
		while(x > px && y > py)
		{
			ops.push_back(CMP_KEEP);
			--x;
			--y;
		}
		if(x == px)
		{
			ops.push_back(CMP_INS);
			--y;
		}
		else
		{
			ops.push_back(CMP_DEL);
			--x;
		}
	}
	while(x > 0 && y > 0)
	{
		ops.push_back(CMP_KEEP);
		--x;
		--y;
	}
	std::reverse(ops.begin(), ops.end());
	return true;
}

Defun1(revisionCompareDocuments)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail(pDoc,false);

	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame,false);

	PD_Document * pDoc2 = s_doListDocuments(pFrame, true, XAP_DIALOG_ID_COMPAREDOCUMENTS);
	if(!pDoc2)
		return true;

	pFrame->raise();

	std::vector<CmpTok> toks1, toks2;
	s_docTokens(pDoc, toks1);
	s_docTokens(pDoc2, toks2);

	std::vector<CmpOp> ops;
	if(!s_wordDiff(toks1, toks2, ops))
	{
		pFrame->showMessageBox(AP_STRING_ID_MSG_CompareTooLarge,
							   XAP_Dialog_MessageBox::b_O,
							   XAP_Dialog_MessageBox::a_OK);
		return true;
	}

	bool bAnyChange = false;
	for(CmpOp op : ops)
	{
		if(op != CMP_KEEP)
		{
			bAnyChange = true;
			break;
		}
	}
	if(!bAnyChange)
	{
		pFrame->showMessageBox(AP_STRING_ID_MSG_CompareIdentical,
							   XAP_Dialog_MessageBox::b_O,
							   XAP_Dialog_MessageBox::a_OK);
		return true;
	}

	// build the merged result in a fresh document
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp,false);

	XAP_Frame * pNewFrame = pApp->newFrame();
	UT_return_val_if_fail(pNewFrame,false);
	pNewFrame->loadDocument((const char *)nullptr, IEFT_Unknown);
	pNewFrame->show();

	FV_View * pNewView = static_cast<FV_View *>(pNewFrame->getCurrentView());
	UT_return_val_if_fail(pNewView,false);
	PD_Document * pNewDoc = pNewView->getDocument();
	UT_return_val_if_fail(pNewDoc,false);

	pNewDoc->beginUserAtomicGlob();

	size_t i = 0, j = 0;
	bool bEmitted = false;
	UT_UCS4String run;
	CmpOp runOp = CMP_KEEP;

	auto flush = [&](CmpOp op)
	{
		if(run.empty())
			return;
		if(op == CMP_DEL)
		{
			// deletion marks are made by inserting the text plainly
			// and then deleting it while tracking is on
			pNewDoc->setMarkRevisions(false);
			PT_DocPosition p1 = 0;
			pNewDoc->getBounds(true, p1);
			--p1;
			pNewView->moveInsPtTo(FV_DOCPOS_EOD);
			pNewView->cmdCharInsert(run.ucs4_str(), run.length());
			PT_DocPosition p2 = 0;
			pNewDoc->getBounds(true, p2);
			--p2;
			pNewDoc->setMarkRevisions(true);
			UT_uint32 cnt = 0;
			pNewDoc->deleteSpan(p1, p2, nullptr, cnt);
		}
		else
		{
			pNewDoc->setMarkRevisions(op == CMP_INS);
			pNewView->moveInsPtTo(FV_DOCPOS_EOD);
			pNewView->cmdCharInsert(run.ucs4_str(), run.length());
		}
		run.clear();
		bEmitted = true;
	};

	for(CmpOp op : ops)
	{
		CmpTok tk;
		if(op == CMP_INS)
			tk = toks2[j++];
		else
		{
			tk = toks1[i++];
			if(op == CMP_KEEP)
				++j;
		}

		if(tk.para)
		{
			flush(runOp);
			runOp = CMP_KEEP;
			if(bEmitted)
			{
				pNewDoc->setMarkRevisions(false);
				pNewView->moveInsPtTo(FV_DOCPOS_EOD);
				pNewView->insertParagraphBreak();
			}
			continue;
		}
		if(op != runOp)
		{
			flush(runOp);
			runOp = op;
		}
		run += tk.text;
		run += ' ';
	}
	flush(runOp);

	pNewDoc->setMarkRevisions(false);
	pNewDoc->endUserAtomicGlob();

	return true;
}

/*!
    Accept every revision mark in the document, keeping the
    revision table (unlike purgeAllRevisions, which also drops
    the recorded history and prompts for confirmation).
*/
Defun1(revisionAcceptAll)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail(pDoc,false);

	return pDoc->acceptAllRevisions();
}

/*!
    Reject every revision mark in the document.  Uses the
    document-level iterator (like acceptAllRevisions) rather than
    cmdFindRevision, which skips hidden runs and would silently do
    nothing in Simple/No Markup modes.
*/
Defun1(revisionRejectAll)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail(pDoc,false);

	// revision ids start at 1, so "higher than 0" means all of them
	return pDoc->rejectAllHigherRevisions(0);
}

/*!
    Accept the revision at the caret and move to the next one,
    Word's "Accept and Move to Next".
*/
Defun1(revisionAcceptNext)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	pView->cmdAcceptRejectRevision(false, 0, 0);
	pView->cmdFindRevision(true, 0, 0);
	return true;
}

/*!
    Reject the revision at the caret and move to the next one.
*/
Defun1(revisionRejectNext)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	pView->cmdAcceptRejectRevision(true, 0, 0);
	pView->cmdFindRevision(true, 0, 0);
	return true;
}

/*!
    Accept every revision currently shown in the view
    ("Accept All Changes Shown").
*/
Defun1(revisionAcceptAllShown)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail(pDoc,false);

	return pDoc->acceptAllRevisionsUpTo(pView->getRevisionLevel());
}

/*!
    Reject every revision currently shown in the view
    ("Reject All Changes Shown").
*/
Defun1(revisionRejectAllShown)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail(pDoc,false);

	return pDoc->rejectAllRevisionsUpTo(pView->getRevisionLevel());
}

/*!
    Accept all revisions and stop tracking changes.
*/
Defun1(revisionAcceptAllStopTracking)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail(pDoc,false);

	bool b = pDoc->acceptAllRevisions();
	pDoc->setMarkRevisions(false);
	return b;
}

/*!
    Reject all revisions and stop tracking changes.
*/
Defun1(revisionRejectAllStopTracking)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail(pDoc,false);

	bool b = pDoc->rejectAllHigherRevisions(0);
	pDoc->setMarkRevisions(false);
	return b;
}

/*!
    Combine the contents of another open document into this one:
    the other document's paragraphs are appended at the end as
    tracked insertions, skipping spans that are marked as deleted
    revisions in that document.  Both source documents are left
    unmodified apart from the appended block, which can be rejected
    like any other revision.
*/
Defun1(revisionCombineDocuments)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail(pDoc,false);

	XAP_Frame * pFrame = static_cast<XAP_Frame *> ( pAV_View->getParentData());
	UT_return_val_if_fail(pFrame,false);

	PD_Document * pDoc2 = s_doListDocuments(pFrame, true, XAP_DIALOG_ID_MERGEDOCUMENTS);
	if(!pDoc2)
		return true;

	pFrame->raise();

	// collect the other document's text paragraph by paragraph,
	// skipping revision-deleted spans
	std::vector<UT_UCS4String> paras;
	UT_UCS4String para;

	PD_DocIterator t(*pDoc2);
	while(t.getStatus() == UTIter_OK)
	{
		pf_Frag * pf = t.getFrag();
		if(pf && pf->getType() == pf_Frag::PFT_Strux &&
		   static_cast<pf_Frag_Strux *>(pf)->getStruxType() == PTX_Block)
		{
			paras.push_back(para);
			para.clear();
		}
		else if(pf && pf->getType() == pf_Frag::PFT_Text)
		{
			const PP_AttrProp * pAP = nullptr;
			pDoc2->getPieceTable()->getAttrProp(pf->getIndexAP(), &pAP);
			const gchar * pszRevision = nullptr;
			if(pAP)
				pAP->getAttribute("revision", pszRevision);

			bool bDeleted = false;
			if(pszRevision)
			{
				PP_RevisionAttr RevAttr(pszRevision);
				bDeleted = (RevAttr.getType() == PP_REVISION_DELETION);
			}

			if(!bDeleted)
			{
				UT_UCS4Char c = t.getChar();
				if(c)
					para += c;
			}
		}
		++t;
	}
	paras.push_back(para);

	// append the paragraphs at the end of this document as tracked
	// insertions
	bool bMarkWasOn = pDoc->isMarkRevisions();
	if(!bMarkWasOn)
		pDoc->setMarkRevisions(true);

	pView->moveInsPtTo(FV_DOCPOS_EOD);

	for(const UT_UCS4String & s : paras)
	{
		if(s.empty())
			continue;
		pView->insertParagraphBreak();
		pView->cmdCharInsert(s.ucs4_str(), s.length());
	}

	if(!bMarkWasOn)
		pDoc->setMarkRevisions(false);

	return true;
}

/*!
    Set the revision display mode, Word's "Display for Review":
      simple   - final text, a bar in the left margin marks changed lines
      all      - final text with insertions/deletions marked inline
      none     - final text, no revision display at all
      original - the document as it was before any revisions
*/
Defun(revisionDisplayMode)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView,false);
	UT_return_val_if_fail(pCallData && pCallData->m_pData,false);

	std::string sMode(
		reinterpret_cast<const char *>(pCallData->m_pData),
		pCallData->m_dataLength);

	if (sMode == "all")
	{
		pView->setShowRevBars(false);
		pView->setShowRevisions(true);
		pView->cmdSetRevisionLevel(PD_MAX_REVISION);
	}
	else if (sMode == "none")
	{
		pView->setShowRevBars(false);
		pView->setShowRevisions(false);
		pView->cmdSetRevisionLevel(PD_MAX_REVISION);
	}
	else if (sMode == "original")
	{
		pView->setShowRevBars(false);
		pView->setShowRevisions(false);
		pView->cmdSetRevisionLevel(0);
	}
	else /* "simple" */
	{
		pView->setShowRevisions(false);
		pView->cmdSetRevisionLevel(PD_MAX_REVISION);
		pView->setShowRevBars(true);
	}
	return true;
}


static UT_sint32 sTopRulerHeight =0;
static UT_sint32 sLeftRulerPos =0;
static UT_sint32 siFixed =0;

Defun(beginVDrag)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView, false);
	AP_TopRuler * pTopRuler = pView->getTopRuler();

	if(pTopRuler == nullptr)
	{
		XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());
		UT_return_val_if_fail( pFrame, true );
		
		pTopRuler = new AP_TopRuler(pFrame);
		AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
		pFrameData->m_pTopRuler = pTopRuler;
		pView->setTopRuler(pTopRuler);
		pTopRuler->setViewHidden(pView);
	}
	if(pTopRuler->getView() == nullptr)
	{
		return true;
	}

	pView->setDragTableLine(true);
	UT_sint32 x = pCallData->m_xPos;
	UT_sint32 y = pCallData->m_yPos;
	PT_DocPosition pos = pView->getDocPositionFromXY(x, y);
	xxx_UT_DEBUGMSG(("ap_EditMethods.cpp:: VDrag begin \n"));
	
	sTopRulerHeight = pTopRuler ? pTopRuler->setTableLineDrag(pos,x,siFixed) : 0;
	pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_GRAB);
	return true;
}

Defun(beginHDrag)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	AP_LeftRuler * pLeftRuler = pView->getLeftRuler();

	if(pLeftRuler == nullptr)
	{
		XAP_Frame * pFrame = static_cast<XAP_Frame *> (pView->getParentData());

		pLeftRuler = new AP_LeftRuler(pFrame);
		AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
		pFrameData->m_pLeftRuler = pLeftRuler;
		pView->setLeftRuler(pLeftRuler);
		pLeftRuler->setViewHidden(pView);
	}

	pView->setDragTableLine(true);
	UT_sint32 x = pCallData->m_xPos;
	UT_sint32 y = pCallData->m_yPos;
	PT_DocPosition pos = pView->getDocPositionFromXY(x, y);
	sLeftRulerPos = pLeftRuler->setTableLineDrag(pos,siFixed,y);
	pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_GRAB);

	return true;
}

Defun1(clearSetCols)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	bool bres = pView->cmdAutoSizeCols();
	pView->setDragTableLine(false);
	return bres;
}


Defun1(autoFitTable)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	bool bres = pView->cmdAutoFitTable();
	return bres;
}

Defun1(autoFitTableWindow)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->cmdAutoFitWindow();
}

Defun1(autoFitTableFixed)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->cmdFixColumnWidths();
}

Defun1(distributeTableCols)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->cmdDistributeCols();
}

Defun1(distributeTableRows)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->cmdDistributeRows();
}

Defun1(splitTable)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->cmdSplitTable();
}

static bool s_cellAlign(FV_View * pView, UT_sint32 iVert,
						const char * szAlign)
{
	UT_return_val_if_fail(pView, false);
	return pView->cmdTableCellAlign(iVert, szAlign);
}

Defun1(cellAlignTopLeft)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_cellAlign(pView, 0, "left");
}

Defun1(cellAlignTopCenter)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_cellAlign(pView, 0, "center");
}

Defun1(cellAlignTopRight)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_cellAlign(pView, 0, "right");
}

Defun1(cellAlignCenterLeft)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_cellAlign(pView, 50, "left");
}

Defun1(cellAlignCenter)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_cellAlign(pView, 50, "center");
}

Defun1(cellAlignCenterRight)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_cellAlign(pView, 50, "right");
}

Defun1(cellAlignBottomLeft)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_cellAlign(pView, 100, "left");
}

Defun1(cellAlignBottomCenter)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_cellAlign(pView, 100, "center");
}

Defun1(cellAlignBottomRight)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	return s_cellAlign(pView, 100, "right");
}

/*!
 * Text Direction popover entries; pCallData is "ltr" or "rtl".
 */
Defun(cellTextDirection)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);
	UT_UTF8String arg(pCallData->m_pData, pCallData->m_dataLength);
	return pView->cmdCellTextDirection(arg.utf8_str());
}

/*!
 * Exact cell sizes from the Cell Size spin fields; pCallData carries a
 * dimension string like "2.5cm".
 */
Defun(tableCellWidth)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);
	UT_UTF8String arg(pCallData->m_pData, pCallData->m_dataLength);
	return pView->cmdTableColWidth(arg.utf8_str());
}

Defun(tableCellHeight)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);
	UT_UTF8String arg(pCallData->m_pData, pCallData->m_dataLength);
	return pView->cmdTableRowHeight(arg.utf8_str());
}

/*!
 * Table sort from the Sort popover; pCallData is
 * "asc"/"desc", optionally with ":h" to keep a header row in place.
 */
Defun(sortTable)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);
	UT_UTF8String arg(pCallData->m_pData, pCallData->m_dataLength);
	const char * sz = arg.utf8_str();
	bool bAsc = strncmp(sz, "desc", 4) != 0;
	bool bHeader = strstr(sz, ":h") != nullptr;
	return pView->cmdSortTableRows(bAsc, -1, bHeader);
}

/* directional merge for the Table Layout Merge Cells popover:
 * data is "left" | "right" | "above" | "below" */
Defun(mergeCellsDir)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);
	UT_UTF8String arg(pCallData->m_pData, pCallData->m_dataLength);
	const char * sz = arg.utf8_str();
	UT_sint32 iDir = -1;
	if (!strcmp(sz, "left"))       iDir = 0;
	else if (!strcmp(sz, "right")) iDir = 1;
	else if (!strcmp(sz, "above")) iDir = 2;
	else if (!strcmp(sz, "below")) iDir = 3;
	UT_return_val_if_fail(iDir >= 0, false);
	return pView->cmdMergeCellsDir(iDir);
}

/* directional split for the Table Layout Split Cells popover:
 * data is "hleft" | "hmid" | "hright" | "vabove" | "vmid" | "vbelow" */
Defun(splitCellsDir)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView && pCallData && pCallData->m_pData, false);
	UT_UTF8String arg(pCallData->m_pData, pCallData->m_dataLength);
	const char * sz = arg.utf8_str();
	AP_CellSplitType eType;
	if (!strcmp(sz, "hleft"))      eType = hori_left;
	else if (!strcmp(sz, "hmid"))  eType = hori_mid;
	else if (!strcmp(sz, "hright"))eType = hori_right;
	else if (!strcmp(sz, "vabove"))eType = vert_above;
	else if (!strcmp(sz, "vmid"))  eType = vert_mid;
	else if (!strcmp(sz, "vbelow"))eType = vert_below;
	else return false;
	return pView->cmdSplitCells(eType);
}

Defun1(repeatHeaderRows)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->cmdToggleRepeatHeader();
}

Defun1(tableToTextParas)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->cmdTableToText(pView->getPoint(), 3);
	return true;
}

Defun1(toggleTableGridlines)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->setShowTableGridlines(!pView->getShowTableGridlines());
	return true;
}

Defun1(toggleDrawTable)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->setDrawTableMode(!pView->getDrawTableMode());
	return true;
}

Defun1(toggleTableEraser)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->setEraserMode(!pView->getEraserMode());
	return true;
}

/* Draw Table pointer mode: drag out a rectangle, get a table on release */
Defun(beginTableDraw)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->beginTableDraw(pCallData->m_xPos, pCallData->m_yPos);
	pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_CROSSHAIR);
	return true;
}

Defun(dragTableDraw)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->dragTableDraw(pCallData->m_xPos, pCallData->m_yPos);
	return true;
}

Defun(endTableDraw)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->endTableDraw(pCallData->m_xPos, pCallData->m_yPos);
	return true;
}

/* Eraser mode: click near a cell border merges the cells across it */
Defun(eraseTableBorder)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->cmdEraseTableBorder(pCallData->m_xPos, pCallData->m_yPos);
	return true;
}

Defun1(cursorTableDraw)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	GR_Graphics * pG = pView->getGraphics();
	if (pG)
	{
		pG->setCursor(GR_Graphics::GR_CURSOR_CROSSHAIR);
	}
	return true;
}

Defun1(cursorTableEraser)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	GR_Graphics * pG = pView->getGraphics();
	if (pG)
	{
		pG->setCursor(GR_Graphics::GR_CURSOR_GRAB);
	}
	return true;
}

/* Border Painter mode (Table Design ribbon): click/drag near a cell
 * border stamps the current table pen onto that edge */
Defun(borderPaintAt)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->cmdBorderPaintAt(pCallData->m_xPos, pCallData->m_yPos);
	return true;
}

/* Border Sampler mode: click near a cell border copies that edge's
 * pen into the table pen and arms the painter */
Defun(borderSampleAt)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->cmdBorderSampleAt(pCallData->m_xPos, pCallData->m_yPos);
	return true;
}

Defun1(cursorBorderPaint)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	GR_Graphics * pG = pView->getGraphics();
	if (pG)
	{
		pG->setCursor(GR_Graphics::GR_CURSOR_DRAGTEXT);
	}
	return true;
}

Defun1(cursorBorderSampler)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	GR_Graphics * pG = pView->getGraphics();
	if (pG)
	{
		pG->setCursor(GR_Graphics::GR_CURSOR_CROSSHAIR);
	}
	return true;
}

Defun1(tableColWider)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	bool bres = pView->cmdTableColResize(true);
	return bres;
}

Defun1(tableColNarrower)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	bool bres = pView->cmdTableColResize(false);
	return bres;
}

Defun1(tableRowTaller)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	bool bres = pView->cmdTableRowResize(true);
	return bres;
}

Defun1(tableRowShorter)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	bool bres = pView->cmdTableRowResize(false);
	return bres;
}

Defun1(clearSetRows)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	bool bres = pView->cmdAutoSizeRows();
	pView->setDragTableLine(false);
	return bres;
}

Defun(dragVline)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Doing Vertical Line drag \n"));

	UT_return_val_if_fail(pView, false);
	AP_TopRuler * pTopRuler = pView->getTopRuler();
	if(pTopRuler == nullptr)
	{
		return true;
	}
	if(pTopRuler->getView() == nullptr)
	{
		pTopRuler->setViewHidden(pView);
	}
	UT_sint32 x = pCallData->m_xPos + siFixed;
	pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_GRAB);
	EV_EditModifierState ems = 0; 
	xxx_UT_DEBUGMSG(("ap_EditMethods.cpp:: DRagging VLine \n"));
	pTopRuler->mouseMotion(ems, x, sTopRulerHeight);
	return true;
}

Defun(dragHline)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Doing Hline Line drag \n"));
	UT_return_val_if_fail(pView, false);
	AP_LeftRuler * pLeftRuler = pView->getLeftRuler();
	if(!pLeftRuler)
	{
		return true;
	}
	if(pLeftRuler->getView() == nullptr)
	{
		pLeftRuler->setViewHidden(pView);
	}
	UT_sint32 y = pCallData->m_yPos;
	pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_GRAB);
	EV_EditModifierState ems = 0; 
	pLeftRuler->mouseMotion(ems, sLeftRulerPos,y);
	return true;
}

Defun(endDragVline)
{
	CHECK_FRAME;
	ABIWORD_VIEW;

	UT_return_val_if_fail(pView, false);
	AP_TopRuler * pTopRuler = pView->getTopRuler();
	if(!pTopRuler)
	{
		return true;
	}
	if(pTopRuler->getView() == nullptr)
	{
		pTopRuler->setView(pView);
	}
	UT_sint32 x = pCallData->m_xPos;
	EV_EditModifierState ems = 0; 
	EV_EditMouseButton emb = EV_EMB_BUTTON1;
	pTopRuler->mouseRelease(ems,emb, x, sTopRulerHeight);
	pView->setDragTableLine(false);
	pView->setCursorToContext();
	return true;
}

Defun(endDragHline)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	AP_LeftRuler * pLeftRuler = pView->getLeftRuler();
	if(!pLeftRuler)
	{
		return true;
	}
	UT_sint32 y = pCallData->m_yPos;
	EV_EditModifierState ems = 0; 
	EV_EditMouseButton emb = EV_EMB_BUTTON1;
	pLeftRuler->mouseRelease(ems,emb,sLeftRulerPos,y);
	pView->setDragTableLine(false);
	pView->setCursorToContext();
	return true;
}


Defun(btn0InlineImage)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	xxx_UT_DEBUGMSG(("Hover on Inline Image \n"));
	UT_return_val_if_fail(pView, false);
	UT_sint32 y = pCallData->m_yPos;
	UT_sint32 x = pCallData->m_xPos;
	pView->btn0InlineImage(x,y);
	return true;
}


Defun(btn1InlineImage)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_DEBUGMSG(("Click on InlineImage \n"));
	UT_sint32 y = pCallData->m_yPos;
	UT_sint32 x = pCallData->m_xPos;
	pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_GRAB);
	if(pView->getMouseContext(x,y) == EV_EMC_IMAGESIZE)
	{
	     PT_DocPosition pos = pView->getDocPositionFromXY(pCallData->m_xPos, pCallData->m_yPos);
	     fl_BlockLayout * pBlock = pView->getBlockAtPosition(pos);
	     if(pBlock)
	     {
		  UT_sint32 x1,x2,y1,y2,iHeight;
		  bool bEOL = false;
		  bool bDir = false;
		
		  fp_Run * pRun = nullptr;
		
		  pRun = pBlock->findPointCoords(pos,bEOL,x1,y1,x2,y2,iHeight,bDir);
		  while(pRun && ((pRun->getType() != FPRUN_IMAGE) && (pRun->getType() != FPRUN_EMBED)))
		  {
			pRun = pRun->getNextRun();
		  }
		  if(pRun && (pRun->getType() == FPRUN_EMBED))
		  {
			// we've found an embed object: do not move the view, just select the image and exit
		        pView->cmdSelect(pos,pos+1);
		  }
	     }
	}
	pView->btn1InlineImage(x,y);
	return true;
}


Defun(copyInlineImage)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Copy InlineImage \n"));
	UT_return_val_if_fail(pView, false);
	UT_sint32 y = pCallData->m_yPos;
	UT_sint32 x = pCallData->m_xPos;
	pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_GRAB);
	pView->btn1CopyImage(x,y);
	return true;
}

static bool sReleaseInlineImage = false;

static void sActualDragInlineImage(AV_View *  pAV_View, EV_EditMethodCallData * pCallData)
{
	ABIWORD_VIEW;
	UT_return_if_fail(pView);
	UT_sint32 y = pCallData->m_yPos;
	UT_sint32 x = pCallData->m_xPos;
	//
	// This boolean is true if we had to drop aa release event because
	// a drag was pending so instead of doing a dragInlineImage we now
	// do the release inline image and return.
	//
	if(sReleaseInlineImage)
	{
		UT_DEBUGMSG(("Nested Release InlineImage call \n"));
		sReleaseInlineImage = false;
		pView->releaseInlineImage(x,y);
		return;
	}
	pView->dragInlineImage(x,y);

}

Defun(dragInlineImage)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	xxx_UT_DEBUGMSG(("Drag Inline Image \n"));
//
// Do this operation in an idle loop so when can reject queued events
//
//
// This code sets things up to handle the warp right in an idle loop.
//
	UT_return_val_if_fail(pView, false);
	int inMode = UT_WorkerFactory::IDLE | UT_WorkerFactory::TIMER;
	//int inMode = UT_WorkerFactory::TIMER;
	UT_WorkerFactory::ConstructMode outMode = UT_WorkerFactory::NONE;
	EV_EditMethodCallData * pNewData = new  EV_EditMethodCallData(pCallData->m_pData,pCallData->m_dataLength);
	pNewData->m_xPos = pCallData->m_xPos;
	pNewData->m_yPos = pCallData->m_yPos;
	_Freq * pFreq = new _Freq(pView,pNewData,sActualDragInlineImage);
	s_pFrequentRepeat = UT_WorkerFactory::static_constructor (_sFrequentRepeat,pFreq, inMode, outMode);

	UT_ASSERT(s_pFrequentRepeat);
	UT_ASSERT(outMode != UT_WorkerFactory::NONE);

	// If the worker is working on a timer instead of in the idle
	// time, set the frequency of the checks.
	if ( UT_WorkerFactory::TIMER == outMode )
	{
		// this is really a timer, so it's safe to static_cast it
		static_cast<UT_Timer*>(s_pFrequentRepeat)->set(50);
	}
	s_pFrequentRepeat->start();
	return true;
}


Defun(releaseInlineImage)
{
	sReleaseInlineImage = true;
	//
	// If this release event occurs while the current image is had a drag
	// event pending process then we can up with a duplicated image.
	// The CHECK_FRAME below will return true if there is a pending
	// drag being processed. The flag above will be set true to handle
	// this case.
	//
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Release Inline Image \n"));
	UT_return_val_if_fail(pView, false);
	UT_sint32 y = pCallData->m_yPos;
	UT_sint32 x = pCallData->m_xPos;
	sReleaseInlineImage = false;
	pView->releaseInlineImage(x,y);
	return true;
}


Defun(btn0Frame)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	xxx_UT_DEBUGMSG(("Hover on Frame \n"));
	UT_return_val_if_fail(pView, false);
	UT_sint32 y = pCallData->m_yPos;
	UT_sint32 x = pCallData->m_xPos;
	pView->btn0Frame(x,y);
	return true;
}


Defun(btn1Frame)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Click on Frame \n"));
	UT_return_val_if_fail(pView, false);
	UT_sint32 y = pCallData->m_yPos;
	UT_sint32 x = pCallData->m_xPos;
	pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_GRAB);
	pView->btn1Frame(x,y);
	return true;
}

static bool sReleaseFrame = false;

static void sActualDragFrame(AV_View *  pAV_View, EV_EditMethodCallData * pCallData)
{
	ABIWORD_VIEW;
	UT_return_if_fail(pView);
	UT_sint32 y = pCallData->m_yPos;
	UT_sint32 x = pCallData->m_xPos;
	if(sReleaseFrame)
	{
		sReleaseFrame = false;
		pView->releaseFrame(x,y);
		return;
	}
	pView->dragFrame(x,y);

}

Defun(dragFrame)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Doing Drag Frame \n"));
//
// Do this operation in an idle loop so when can reject queued events
//
//
// This code sets things up to handle the warp right in an idle loop.
//
	UT_return_val_if_fail(pView, false);
	int inMode = UT_WorkerFactory::IDLE | UT_WorkerFactory::TIMER;
	UT_WorkerFactory::ConstructMode outMode = UT_WorkerFactory::NONE;
	EV_EditMethodCallData * pNewData = new  EV_EditMethodCallData(pCallData->m_pData,pCallData->m_dataLength);
	pNewData->m_xPos = pCallData->m_xPos;
	pNewData->m_yPos = pCallData->m_yPos;
	_Freq * pFreq = new _Freq(pView,pNewData,sActualDragFrame);
	s_pFrequentRepeat = UT_WorkerFactory::static_constructor (_sFrequentRepeat,pFreq, inMode, outMode);

	UT_ASSERT(s_pFrequentRepeat);
	UT_ASSERT(outMode != UT_WorkerFactory::NONE);

	// If the worker is working on a timer instead of in the idle
	// time, set the frequency of the checks.
	if ( UT_WorkerFactory::TIMER == outMode )
	{
		// this is really a timer, so it's safe to static_cast it
		UT_DEBUGMSG(("Set timer to 50 ms \n"));
		static_cast<UT_Timer*>(s_pFrequentRepeat)->set(50);
	}
	s_pFrequentRepeat->start();
	return true;
}


Defun(releaseFrame)
{
	sReleaseFrame = true;
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Release Frame \n"));
	UT_return_val_if_fail(pView, false);
	UT_sint32 y = pCallData->m_yPos;
	UT_sint32 x = pCallData->m_xPos;
	sReleaseFrame = false;
	pView->releaseFrame(x,y);
	return true;
}


Defun1(deleteFrame)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Delete Frame \n"));
	UT_return_val_if_fail(pView, false);
	pView->deleteFrame();
	return true;
}

Defun1(cutFrame)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->copyFrame(false);
	return true;
}


Defun1(copyFrame)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Copy Frame \n"));
	UT_return_val_if_fail(pView, false);
	pView->copyFrame(true);
	return true;
}


Defun1(selectFrame)
{
	CHECK_FRAME;
	UT_DEBUGMSG(("Select Frame \n"));
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->selectFrame();
	return true;
}


Defun1(frameBringForward)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->restackFrame(1);
}

Defun1(frameBringToFront)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->restackFrame(2);
}

Defun1(frameSendBackward)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->restackFrame(-1);
}

Defun1(frameSendToBack)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->restackFrame(-2);
}

Defun1(frameInFrontOfText)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->frameSetTextLayer(true);
}

Defun1(frameBehindText)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->frameSetTextLayer(false);
}

Defun1(frameRotateRight)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->rotateFrame(pView->getFrameLayout(), 90.0);
}

Defun1(frameRotateLeft)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->rotateFrame(pView->getFrameLayout(), -90.0);
}

Defun1(frameFlipHoriz)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->flipFrame(pView->getFrameLayout(), true);
}

Defun1(frameFlipVert)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	return pView->flipFrame(pView->getFrameLayout(), false);
}

/* absolute rotation angle, call data is the degrees string */
Defun(frameRotateTo)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	double deg = 0.0;
	if (pCallData && pCallData->m_pData && pCallData->m_dataLength)
	{
		UT_UCS4String s(pCallData->m_pData, pCallData->m_dataLength);
		deg = g_ascii_strtod(s.utf8_str(), nullptr);
	}
	return pView->setFrameRotation(pView->getFrameLayout(), deg);
}

Defun1(frameGroup)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_GenericVector<fl_FrameLayout *> sel;
	pView->getGroupSel(sel);
	if (sel.getItemCount() < 2)
	{
		XAP_Frame * pFrame =
			static_cast<XAP_Frame *>(pAV_View->getParentData());
		if (pFrame)
		{
			pFrame->setStatusMessage(
				"Tick two or more objects in the Selection Pane first");
		}
		return false;
	}
	bool bOK = pView->groupFrames(sel);
	if (bOK)
		pView->clearGroupSel();
	return bOK;
}

Defun1(frameUngroup)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	UT_GenericVector<fl_FrameLayout *> sel;
	pView->getGroupSel(sel);
	fl_FrameLayout * pCur = pView->getFrameLayout();
	if (pCur && sel.findItem(pCur) < 0)
		sel.addItem(pCur);
	return pView->ungroupFrames(sel);
}

Defun1(selPane)
{
	CHECK_FRAME;
	UT_return_val_if_fail(pAV_View, false);
	XAP_Frame * pFrame =
		static_cast<XAP_Frame *>(pAV_View->getParentData());
	UT_return_val_if_fail(pFrame, false);
	XAP_FrameImpl * pImpl = pFrame->getFrameImpl();
	UT_return_val_if_fail(pImpl, false);
	pImpl->toggleSelPane();
	return true;
}

Defun(cutVisualText)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pView->getParentData()); 
	UT_DEBUGMSG(("Cut on Selection \n"));
	UT_sint32 y = pCallData->m_yPos;
	UT_sint32 x = pCallData->m_xPos;
	pView->cutVisualText(x,y);
	if(pView->getVisualText()->isNotdraggingImage())
	{
	  pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_DRAGTEXT);
	  pFrame->setCursor(GR_Graphics::GR_CURSOR_DRAGTEXT);
	  if(	pView->getVisualText()->isDoingCopy())
	  {
	    pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_COPYTEXT);
	    pFrame->setCursor(GR_Graphics::GR_CURSOR_COPYTEXT);
	  }
	}
	else
	{
	  pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_IMAGE);
	}
	return true;
}


Defun(copyVisualText)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pView->getParentData()); 
	xxx_UT_DEBUGMSG(("Copy on Selection \n"));
	UT_sint32 y = pCallData->m_yPos;
	UT_sint32 x = pCallData->m_xPos;
	pView->copyVisualText(x,y);
	if(pView->getVisualText()->isNotdraggingImage())
	{
	  pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_DRAGTEXT);
	  pFrame->setCursor(GR_Graphics::GR_CURSOR_DRAGTEXT);
	  if(	pView->getVisualText()->isDoingCopy())
	  {
	    pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_COPYTEXT);
	    pFrame->setCursor(GR_Graphics::GR_CURSOR_COPYTEXT);
	  }
	}
	else
	{
	  pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_IMAGE);
	}
	return true;
}

static bool sEndVisualDrag = false;

static void sActualVisualDrag(AV_View *  pAV_View, EV_EditMethodCallData * pCallData)
{
	ABIWORD_VIEW;
	UT_return_if_fail(pView);
	XAP_Frame * pFrame = static_cast<XAP_Frame *>(pView->getParentData()); 
	UT_sint32 y = pCallData->m_yPos;
	UT_sint32 x = pCallData->m_xPos;
	if(sEndVisualDrag)
	{
		sEndVisualDrag = false;
		pView->pasteVisualText(x,y);
		return;
	}
	if(pView->getVisualText()->isNotdraggingImage())
	{
	  pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_DRAGTEXT);
	  pFrame->setCursor(GR_Graphics::GR_CURSOR_DRAGTEXT);
	  if(	pView->getVisualText()->isDoingCopy())
	  {
	    pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_COPYTEXT);
	    pFrame->setCursor(GR_Graphics::GR_CURSOR_COPYTEXT);
	  }
	}
	else
	{
	  pView->getGraphics()->setCursor(GR_Graphics::GR_CURSOR_IMAGE);
	}
	pView->dragVisualText(x,y);
}

Defun(dragVisualText)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	sEndVisualDrag = false;
	xxx_UT_DEBUGMSG(("Drag Visual Text \n"));
	UT_return_val_if_fail(pView, false);
	PT_DocPosition posLow = pView->getSelectionAnchor();
	PT_DocPosition posHigh = pView->getPoint();
	if(posLow > posHigh)
	{
	     PT_DocPosition pos = posLow;
	     posLow = posHigh;
	     posHigh = pos;
	}
	if((posLow + 1) == posHigh)
	{
	     fl_BlockLayout * pBL = pView->getCurrentBlock();
	     if((pBL->getPosition() >= posLow) && ((pBL->getPosition() + pBL->getLength()) > posHigh))
	     {
	       UT_sint32 x1,x2,y1,y2,height;
	       bool bEOL,bDir;
	       bEOL = false;
	       fp_Run * pRun = pBL->findPointCoords(posHigh,bEOL,x1,x2,y1,y2,height,bDir);
	       if(pRun->getType() == FPRUN_IMAGE)
	       {
			   FV_VisualDragText * pVis = pView->getVisualText();
			   pVis->abortDrag();
	       }
	     }
	}
//
// Do this operation in an idle loop so when can reject queued events
//
//
// This code sets things up to handle the warp right in an idle loop.
//
	int inMode = UT_WorkerFactory::IDLE | UT_WorkerFactory::TIMER;
	UT_WorkerFactory::ConstructMode outMode = UT_WorkerFactory::NONE;
	EV_EditMethodCallData * pNewData = new  EV_EditMethodCallData(pCallData->m_pData,pCallData->m_dataLength);
	pNewData->m_xPos = pCallData->m_xPos;
	pNewData->m_yPos = pCallData->m_yPos;
	_Freq * pFreq = new _Freq(pView,pNewData,sActualVisualDrag);
	s_pFrequentRepeat = UT_WorkerFactory::static_constructor (_sFrequentRepeat,pFreq, inMode, outMode);

	UT_ASSERT(s_pFrequentRepeat);
	UT_ASSERT(outMode != UT_WorkerFactory::NONE);

	// If the worker is working on a timer instead of in the idle
	// time, set the frequency of the checks.
	if ( UT_WorkerFactory::TIMER == outMode )
	{
		// this is really a timer, so it's safe to static_cast it
		static_cast<UT_Timer*>(s_pFrequentRepeat)->set(50);
	}
	s_pFrequentRepeat->start();
	return true;
}


Defun(pasteVisualText)
{
    sEndVisualDrag = true;
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Drop Visual Text \n"));
	UT_return_val_if_fail(pView, false);
	UT_sint32 y = pCallData->m_yPos;
	UT_sint32 x = pCallData->m_xPos;
    sEndVisualDrag = false;
	pView->pasteVisualText(x,y);
	return true;
}



Defun(btn0VisualText)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	xxx_UT_DEBUGMSG(("In Visual Text \n"));
	UT_return_val_if_fail(pView, false);
	pView->btn0VisualDrag(pCallData->m_xPos,pCallData->m_yPos);
	static_cast<AV_View *>(pView)->notifyListeners(AV_CHG_MOUSEPOS);
	return true;
}

Defun1(repeatThisRow)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	if (!pView->isRepeatHeaderOn())
	{
		return pView->cmdToggleRepeatHeader();
	}
	return true;
}

Defun1(removeThisRowRepeat)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	if (pView->isRepeatHeaderOn())
	{
		return pView->cmdToggleRepeatHeader();
	}
	return true;
}

Defun1(tableToTextCommas)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->cmdTableToText(pView->getPoint(),0);
	return true;
}


Defun1(tableToTextTabs)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->cmdTableToText(pView->getPoint(),1);
	return true;
}

Defun1(tableToTextCommasTabs)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	pView->cmdTableToText(pView->getPoint(),2);
	return true;
}

Defun1(doEscape)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_DEBUGMSG(("Escape Pressed. \n"));
	UT_return_val_if_fail(pView, false);
	if (pView->getDrawTableMode() || pView->getEraserMode() ||
		pView->isBorderPainterMode() || pView->isBorderSamplerMode())
	{
		pView->setDrawTableMode(false);
		pView->setEraserMode(false);
		pView->setBorderPainterMode(false);
		pView->setBorderSamplerMode(false);
		return true;
	}
	FV_VisualDragText * pVis = pView->getVisualText();
	if(pVis->isActive())
	{
	    pVis->abortDrag();
	    sEndVisualDrag = false;
	    return true;
	}
	return true;
}

#ifdef DEBUG
Defun1(dumpRDFForPoint)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail(pDoc, false);

    UT_DEBUGMSG(("dumpRDFForPoint...\n"));
    if( pView )
    {
        PT_DocPosition curr = pView->getPoint();
        UT_DEBUGMSG(("dumpRDFForPoint...current position:%d\n", curr));
        PD_RDFModelHandle h = pDoc->getDocumentRDF()->getRDFAtPosition( curr );
		
    }
    
    return true;
}

Defun1(dumpRDFObjects)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail(pDoc, false);

    UT_DEBUGMSG(("dumpRDFObjects...\n"));
    pDoc->getDocumentRDF()->dumpObjectMarkersFromDocument();
    return true;
}

Defun1(rdfTest)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail(pDoc, false);

    UT_DEBUGMSG(("RDFTest... running ml2 test\n"));
    pDoc->getDocumentRDF()->runMilestone2Test();
    return true;
}

Defun1(rdfPlay)
{
	CHECK_FRAME;
	ABIWORD_VIEW;
	UT_return_val_if_fail(pView, false);
	PD_Document * pDoc = pView->getDocument();
	UT_return_val_if_fail(pDoc, false);

    UT_DEBUGMSG(("RDFTest... running RDF play\n"));
    pDoc->getDocumentRDF()->runPlay();
    return true;
}
#endif
