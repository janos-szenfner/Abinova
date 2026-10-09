/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
 * Copyright (C) 2026 Abinova contributors
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

/* COVD03 wave-5 -- OOXML importer element-validation coverage.
 *
 * OXMLi_ListenerState_Valid dispatches every element name through a
 * 577-entry keyword table and marks the request valid when the name
 * is registered (the contextMatches() arms are only reachable for
 * names sharing a keyword -- this suite pins the effective contract:
 * registered name => valid, unregistered name => invalid).
 *
 * Mains:
 *   sweep       every registered name validates
 *   misroutes   the wave-5 fixes: names that used to land on the
 *               wrong case arm (or no arm) must validate even under
 *               an unrelated context -- name-match semantics
 *   unknowns    unregistered names are rejected, never handled
 *   ac          mc:AlternateContent bookkeeping: supported Choice is
 *               tracked, a sibling Fallback's payload is swallowed,
 *               an unsupported Choice's subtree is swallowed while
 *               its part references are captured for the fallback
 *   endelement  endElement + charData paths incl. bookkeeping cleanup
 *   fixture     test/wp/oxcov/cov.docx exercises the uncovered
 *               settings/webSettings/ffData/sdt/change-tracking
 *               clusters through the real importer
 *
 * s_kwNames mirrors OXMLi_ListenerState_Valid::populateKeywordTable --
 * keep it in sync when the table changes.
 */

#include "tf_test.h"

#include "OXMLi_ListenerState_Valid.h"
#include "OXML_Document.h"
#include "pd_Document.h"
#include "ie_exp.h"
#include "ie_types.h"

#include <gsf/gsf-output-stdio.h>
#include <glib/gstdio.h>

#include <cstring>
#include <string>
#include <vector>

#define TFSUITE "core.wp.impexp.oxmlvalid"

namespace {

/* minimal request plumbing: the validator reads pName, ppAtts and the
 * context vector; the element/section stacks are only touched on
 * paths that handle a request (which Valid never does outside the
 * mc:AlternateContent machinery) */
struct VRq
{
	OXMLi_ElementStack  stck;
	OXMLi_SectionStack  sect;
	OXMLi_ContextVector ctx;
	std::map<std::string, std::string> atts;

	VRq() : ctx{"W:document", "W:body"} {}

	OXMLi_StartElementRequest * start(const char * name)
	{
		m_start.pName = name;
		m_start.ppAtts = &atts;
		m_start.stck = &stck;
		m_start.sect_stck = &sect;
		m_start.context = &ctx;
		m_start.handled = false;
		m_start.valid = false;
		return &m_start;
	}

	OXMLi_EndElementRequest * end(const char * name)
	{
		m_end.pName = name;
		m_end.stck = &stck;
		m_end.sect_stck = &sect;
		m_end.context = &ctx;
		m_end.handled = false;
		m_end.valid = false;
		return &m_end;
	}

	OXMLi_CharDataRequest * chars(const gchar * buf, int len)
	{
		m_chars.buffer = buf;
		m_chars.length = len;
		m_chars.stck = &stck;
		m_chars.context = &ctx;
		m_chars.handled = false;
		m_chars.valid = false;
		return &m_chars;
	}

private:
	OXMLi_StartElementRequest m_start;
	OXMLi_EndElementRequest   m_end;
	OXMLi_CharDataRequest     m_chars;
};

static const char * const s_kwNames[] = {
	"PIC:blipFill", "PIC:cNvPicPr", "PIC:cNvPr", "PIC:nvPicPr",
	"PIC:pic", "PIC:spPr", "VE:AlternateContent", "VE:Choice",
	"VE:Fallback", "W:abstractNum", "W:abstractNumId", "W:activeWritingStyle",
	"W:adjustLineHeightInTable", "W:adjustRightInd", "W:alias", "W:aliases",
	"W:alignBordersAndEdges", "W:alignTablesRowByRow", "W:allowPNG", "W:allowSpaceOfSameStyleInTable",
	"W:altChunk", "W:altChunkPr", "W:altName", "W:alwaysMergeEmptyNamespace",
	"W:alwaysShowPlaceholderText", "W:annotationRef", "W:applyBreakingRules", "W:attachedSchema",
	"W:attachedTemplate", "W:attr", "W:autoCaption", "W:autoCaptions",
	"W:autofitToFirstFixedWidthCell", "W:autoFormatOverride", "W:autoHyphenation", "W:autoRedefine",
	"W:autoSpaceDE", "W:autoSpaceDN", "W:autoSpaceLikeWord95", "W:b",
	"W:background", "W:balanceSingleByteDoubleByteWidth", "W:bar", "W:basedOn",
	"W:bCs", "W:bdr", "W:behavior", "W:behaviors",
	"W:between", "W:bibliography", "W:bidi", "W:bidiVisual",
	"W:blockQuote", "W:body", "W:bodyDiv", "W:bookFoldPrinting",
	"W:bookFoldPrintingSheets", "W:bookFoldRevPrinting", "W:bookmarkEnd", "W:bookmarkStart",
	"W:bordersDoNotSurroundFooter", "W:bordersDoNotSurroundHeader", "W:bottom", "W:break",
	"W:cachedColBalance", "W:calcOnExit", "W:calendar", "W:cantSplit",
	"W:caps", "W:caption", "W:captions", "W:category",
	"W:cellDel", "W:cellIns", "W:cellMerge", "W:characterSpacingControl",
	"W:charset", "W:checkBox", "W:checked", "W:citation",
	"W:clickAndTypeStyle", "W:clrSchemeMapping", "W:cnfStyle", "W:col",
	"W:color", "W:cols", "W:comboBox", "W:comment",
	"W:commentRangeEnd", "W:commentRangeStart", "W:commentReference", "W:comments",
	"W:compat", "W:consecutiveHyphenLimit", "W:contextualSpacing", "W:continuationSeparator",
	"W:control", "W:cr", "W:cs", "W:customXml",
	"W:customXmlDelRangeEnd", "W:customXmlDelRangeStart", "W:customXmlInsRangeEnd", "W:customXmlInsRangeStart",
	"W:customXmlMoveFromRangeEnd", "W:customXmlMoveFromRangeStart", "W:customXmlMoveToRangeEnd", "W:customXmlMoveToRangeStart",
	"W:customXmlPr", "W:dataBinding", "W:date", "W:dateFormat",
	"W:dayLong", "W:dayShort", "W:ddList", "W:decimalSymbol",
	"W:default", "W:defaultTableStyle", "W:defaultTabStop", "W:del",
	"W:delInstrText", "W:delText", "W:description", "W:dirty",
	"W:displayBackgroundShape", "W:displayHangulFixedWidth", "W:displayHorizontalDrawingGridEvery", "W:displayVerticalDrawingGridEvery",
	"W:div", "W:divBdr", "W:divId", "W:divs",
	"W:divsChild", "W:docDefaults", "W:docGrid", "W:docPart",
	"W:docPartBody", "W:docPartCategory", "W:docPartGallery", "W:docPartList",
	"W:docPartObj", "W:docPartPr", "W:docParts", "W:docPartUnique",
	"W:document", "W:documentProtection", "W:documentType", "W:docVar",
	"W:docVars", "W:doNotAutoCompressPictures", "W:doNotAutofitConstrainedTables", "W:doNotBreakConstrainedForcedTable",
	"W:doNotBreakWrappedTables", "W:doNotDemarcateInvalidXml", "W:doNotDisplayPageBoundries", "W:doNotEmbedSmartTags",
	"W:doNotExpandShiftReturn", "W:doNotHyphenateCaps", "W:doNotIncludeSubdocsInStats", "W:doNotLeaveBackslashAlone",
	"W:doNotOrganizeInFolder", "W:doNotRelyOnCSS", "W:doNotSaveAsSingleFile", "W:doNotShadeFormData",
	"W:doNotSnapToGridInCell", "W:doNotSuppressIndentation", "W:doNotSuppressParagraphBorders", "W:doNotTrackFormatting",
	"W:doNotTrackMoves", "W:doNotUseEastAsianBreakRules", "W:doNotUseHTMLParagraphAutoSpacing", "W:doNotUseIndentAsNumberingTabStop",
	"W:doNotUseLongFileNames", "W:doNotUseMarginsForDrawingGridOrigin", "W:doNotValidateAgainstSchema", "W:doNotVertAlignCellWithSp",
	"W:doNotVertAlignInTxbx", "W:doNotWrapTextWithPunct", "W:drawing", "W:drawingGridHorizontalOrigin",
	"W:drawingGridHorizontalSpacing", "W:drawingGridVerticalOrigin", "W:drawingGridVerticalSpacing", "W:dropDownList",
	"W:dstrike", "W:eastAsianLayout", "W:effect", "W:em",
	"W:embedBold", "W:embedBoldItalic", "W:embedItalic", "W:embedRegular",
	"W:embedSystemFonts", "W:embedTrueTypeFonts", "W:emboss", "W:enabled",
	"W:encoding", "W:endnote", "W:endnotePr", "W:endnoteRef",
	"W:endnoteReference", "W:endnotes", "W:entryMacro", "W:equation",
	"W:evenAndOddHeaders", "W:exitMacro", "W:family", "W:ffData",
	"W:fitText", "W:flatBorders", "W:fldChar", "W:fldData",
	"W:fldSimple", "W:font", "W:fonts", "W:footerReference",
	"W:footnote", "W:footnoteLayoutLikeWW8", "W:footnotePr", "W:footnoteRef",
	"W:footnoteReference", "W:footnotes", "W:forceUpgrade", "W:forgetLastTabAlignment",
	"W:format", "W:formProt", "W:formsDesign", "W:frame",
	"W:frameLayout", "W:framePr", "W:frameset", "W:framesetSplitbar",
	"W:ftr", "W:gallery", "W:glossaryDocument", "W:gridAfter",
	"W:gridBefore", "W:gridCol", "W:gridSpan", "W:group",
	"W:growAutofit", "W:guid", "W:gutterAtTop", "W:hdr",
	"W:hdrShapeDefaults", "W:headerReference", "W:helpText", "W:hidden",
	"W:hideGrammaticalErrors", "W:hideMark", "W:hideSpellingErrors", "W:highlight",
	"W:hMerge", "W:hps", "W:hpsBaseText", "W:hpsRaise",
	"W:hyperlink", "W:hyphenationZone", "W:i", "W:iCs",
	"W:id", "W:ignoreMixedContent", "W:ilvl", "W:imprint",
	"W:ind", "W:ins", "W:insideH", "W:insideV",
	"W:instrText", "W:isLgl", "W:jc", "W:keepLines",
	"W:keepNext", "W:kern", "W:kinsoku", "W:lang",
	"W:lastRenderedPageBreak", "W:latentStyles", "W:layoutRawTableWidth", "W:layoutTableRowsApart",
	"W:left", "W:legacy", "W:lid", "W:lineWrapLikeWord6",
	"W:link", "W:linkedToFile", "W:linkStyles", "W:listEntry",
	"W:listItem", "W:listSeparator", "W:lnNumType", "W:lock",
	"W:locked", "W:lsdException", "W:lvl", "W:lvlJc",
	"W:lvlOverride", "W:lvlPicBulletId", "W:lvlRestart", "W:lvlText",
	"W:marBottom", "W:marH", "W:marLeft", "W:marRight",
	"W:marTop", "W:marW", "W:matchSrc", "W:maxLength",
	"W:mirrorIndents", "W:mirrorMargins", "W:monthLong", "W:monthShort",
	"W:moveFrom", "W:moveFromRangeEnd", "W:moveFromRangeStart", "W:moveTo",
	"W:moveToRangeEnd", "W:moveToRangeStart", "W:movie", "W:multiLevelType",
	"W:mwSmallCaps", "W:name", "W:next", "W:noBorder",
	"W:noBreakHyphen", "W:noColumnBalance", "W:noEndnote", "W:noExtraLineSpacing",
	"W:noLeading", "W:noLineBreaksAfter", "W:noLineBreaksBefore", "W:noProof",
	"W:noPunctuationKerning", "W:noResizeAllowed", "W:noSpaceRaiseLower", "W:noTabHangInd",
	"W:notTrueType", "W:noWrap", "W:nsid", "W:num",
	"W:numbering", "W:numberingChange", "W:numFmt", "W:numId",
	"W:numIdMacAtCleanup", "W:numPicBullet", "W:numPr", "W:numRestart",
	"W:numStart", "W:numStyleLink", "W:object", "W:oMath",
	"W:optimizeForBrowser", "W:outline", "W:outlineLvl", "W:overflowPunct",
	"W:p", "W:pageBreakBefore", "W:panose1", "W:paperSrc",
	"W:pBdr", "W:permEnd", "W:permStart", "W:personal",
	"W:personalCompose", "W:personalReply", "W:pgBorders", "W:pgMar",
	"W:pgNum", "W:pgNumType", "W:pgSz", "W:pict",
	"W:picture", "W:pitch", "W:pixelsPerInch", "W:placeholder",
	"W:pos", "W:position", "W:pPr", "W:pPrChange",
	"W:pPrDefault", "W:printBodyTextBeforeHeader", "W:printColBlack", "W:printerSettings",
	"W:printFormsData", "W:printFractionalCharacterWidth", "W:printPostScriptOverText", "W:printTwoOnOne",
	"W:proofErr", "W:proofState", "W:pStyle", "W:ptab",
	"W:qFormat", "W:r", "W:readModeInkLockDown", "W:relyOnVML",
	"W:removeDateAndTime", "W:removePersonalInformation", "W:result", "W:revisionView",
	"W:rFonts", "W:richText", "W:right", "W:rPr",
	"W:rPrChange", "W:rPrDefault", "W:rsid", "W:rsidRoot",
	"W:rsids", "W:rStyle", "W:rt", "W:rtl",
	"W:rtlGutter", "W:ruby", "W:rubyAlign", "W:rubyBase",
	"W:rubyPr", "W:saveFormsData", "W:saveInvalidXml", "W:savePreviewPicture",
	"W:saveSmartTagsAsXml", "W:saveSubsetFonts", "W:saveThroughXslt", "W:saveXmlDataOnly",
	"W:scrollbar", "W:sdt", "W:sdtContent", "W:sdtEndPr",
	"W:sdtPr", "W:sectPr", "W:sectPrChange", "W:selectFldWithFirstOrLastChar",
	"W:semiHidden", "W:separator", "W:settings", "W:shadow",
	"W:shapeDefaults", "W:shapeLayoutLikeWW8", "W:shd", "W:showBreaksInFrames",
	"W:showEnvelope", "W:showingPlcHdr", "W:showXMLTags", "W:sig",
	"W:size", "W:sizeAuto", "W:smallCaps", "W:smartTag",
	"W:smartTagPr", "W:smartTagType", "W:snapToGrid", "W:softHyphen",
	"W:sourceFileName", "W:spaceForUL", "W:spacing", "W:spacingInWholePoints",
	"W:specVanish", "W:splitPgBreakAndParaMark", "W:start", "W:startOverride",
	"W:statusText", "W:storeMappedDataAs", "W:strictFirstAndLastChars", "W:strike",
	"W:style", "W:styleLink", "W:styleLockQFset", "W:styleLockTheme",
	"W:stylePaneFormatFilter", "W:stylePaneSortMethod", "W:styles", "W:subDoc",
	"W:subFontBySize", "W:suff", "W:summaryLength", "W:suppressAutoHypens",
	"W:suppressBottomSpacing", "W:suppressLineNumbers", "W:suppressOverlap", "W:suppressSpacingAtTopOfPage",
	"W:suppressSpBfAfterPgBrk", "W:suppressTopSpacing", "W:suppressTopSpacingWP", "W:swapBordersFacingPages",
	"W:sym", "W:sz", "W:szCs", "W:t",
	"W:tab", "W:tabs", "W:tag", "W:targetScreenSz",
	"W:tbl", "W:tblBorders", "W:tblCellMar", "W:tblCellSpacing",
	"W:tblGrid", "W:tblGridChange", "W:tblHeader", "W:tblInd",
	"W:tblLayout", "W:tblLook", "W:tblOverlap", "W:tblpPr",
	"W:tblPr", "W:tblPrChange", "W:tblPrEx", "W:tblPrExChange",
	"W:tblStyle", "W:tblStyleColBandSize", "W:tblStylePr", "W:tblStyleRowBandSize",
	"W:tblW", "W:tc", "W:tcBorders", "W:tcFitText",
	"W:tcMar", "W:tcPr", "W:tcPrChange", "W:tcW",
	"W:temporary", "W:text", "W:textAlignment", "W:textboxTightWrap",
	"W:textDirection", "W:textInput", "W:themeFontLang", "W:titlePg",
	"W:tl2br", "W:tmpl", "W:top", "W:topLinePunct",
	"W:tr", "W:tr2bl", "W:trackRevisions", "W:trHeight",
	"W:trPr", "W:trPrChange", "W:truncateFontHeightsLikeWP6", "W:txbxContent",
	"W:type", "W:types", "W:u", "W:uiCompat97To2003",
	"W:uiPriority", "W:ulTrailSpace", "W:underlineTabInNumList", "W:unhideWhenUsed",
	"W:updateFields", "W:useAltKinsokuLineBreakRules", "W:useAnsiKerningPairs", "W:useFELayout",
	"W:useNormalStyleForList", "W:usePrinterMetrics", "W:useSingleBorderforContiguousCells", "W:useWord2002TableStyleRules",
	"W:useWord97LineBreakRules", "W:useXSLTWhenSaving", "W:vAlign", "W:vanish",
	"W:vertAlign", "W:view", "W:vMerge", "W:w",
	"W:wAfter", "W:wBefore", "W:webHidden", "W:webSettings",
	"W:widowControl", "W:wordWrap", "W:wpJustification", "W:wpSpaceWidth",
	"W:wrapTrailSpaces", "W:writeProtection", "W:yearLong", "W:yearShort",
	"W:zoom",
};


} // anonymous namespace

TFTEST_MAIN("validator: every registered element name is valid")
{
	OXMLi_ListenerState_Valid v;
	VRq q;
	for (size_t i = 0; i < G_N_ELEMENTS(s_kwNames); ++i)
	{
		OXMLi_StartElementRequest * r = q.start(s_kwNames[i]);
		v.startElement(r);
		TFPASS(r->valid);
		/* KEYWORD_Fallback marks valid requests handled by design
		 * (consume the fallback subtree silently); nothing else does */
		TFPASS(r->handled == (0 == strcmp(s_kwNames[i], "VE:Fallback")));
	}
}

TFTEST_MAIN("validator: misrouted names validate by name alone")
{
	/* wave-5 fixes -- these names used to hit the wrong case arm or
	 * no arm at all; under an unrelated context the name match is
	 * what must carry them */
	OXMLi_ListenerState_Valid v;
	static const char * const fixed[] = {
		"W:drawingGridVerticalOrigin",
		"W:drawingGridVerticalSpacing",
		"W:matchSrc",
		"W:activeWritingStyle",
		"W:doNotHyphenateCaps",
		"W:checkBox",
	};
	for (size_t i = 0; i < G_N_ELEMENTS(fixed); ++i)
	{
		VRq q;
		q.ctx = {"W:document", "W:body", "W:p"};  // not "W:settings"
		OXMLi_StartElementRequest * r = q.start(fixed[i]);
		v.startElement(r);
		TFPASS(r->valid);
	}
}

TFTEST_MAIN("validator: unregistered names are rejected, not handled")
{
	OXMLi_ListenerState_Valid v;
	static const char * const bogus[] = {
		"W:notARealElement", "Q:bogus", "w:lowercase", "", "W",
	};
	for (size_t i = 0; i < G_N_ELEMENTS(bogus); ++i)
	{
		VRq q;
		OXMLi_StartElementRequest * r = q.start(bogus[i]);
		v.startElement(r);
		TFPASS(!r->valid);
		TFPASS(!r->handled);
	}
}

TFTEST_MAIN("mc:AlternateContent accepted-choice and fallback swallow")
{
	OXMLi_ListenerState_Valid v;
	VRq q;
	q.ctx = {"W:document", "W:body", "VE:AlternateContent"};

	/* a Choice with no Requires is supported -> the AC is taken */
	OXMLi_StartElementRequest * r = q.start("VE:Choice");
	v.startElement(r);
	TFPASS(r->valid);
	TFPASS(!r->handled);

	q.ctx.push_back("VE:Choice");
	r = q.start("W:drawing");
	v.startElement(r);
	TFPASS(r->valid);
	TFPASS(!r->handled);

	/* Choice closes (context already popped, like the real listener) */
	q.ctx.pop_back();
	OXMLi_EndElementRequest * e = q.end("VE:Choice");
	v.endElement(e);
	TFPASS(e->valid);

	/* with a taken AC the Fallback element itself validates and is
	 * marked handled (the case consumes the wrapper silently), and
	 * everything INSIDE it is swallowed */
	r = q.start("VE:Fallback");
	v.startElement(r);
	TFPASS(r->valid);
	TFPASS(r->handled);
	q.ctx.push_back("VE:Fallback");
	r = q.start("W:pict");
	v.startElement(r);
	TFPASS(r->valid);
	TFPASS(r->handled);
	OXMLi_CharDataRequest * c = q.chars("x", 1);
	v.charData(c);
	TFPASS(c->valid);
	TFPASS(c->handled);
	q.ctx.pop_back();

	/* closing the AlternateContent drops the bookkeeping: a later,
	 * unrelated subtree is no longer swallowed */
	q.ctx.pop_back(); // back to {W:document, W:body}
	e = q.end("VE:AlternateContent");
	v.endElement(e);
	TFPASS(e->valid);
	q.ctx.push_back("VE:AlternateContent");
	q.ctx.push_back("VE:Fallback");
	r = q.start("W:pict");
	v.startElement(r);
	TFPASS(r->valid);
	TFPASS(!r->handled);
}

TFTEST_MAIN("mc:AlternateContent rejected choice records rels")
{
	OXML_Document::getNewInstance();
	OXMLi_ListenerState_Valid v;
	VRq q;
	q.ctx = {"W:document", "W:body", "VE:AlternateContent"};
	q.atts["VE:Requires"] = "bogusns"; /* no listener -> unsupported */

	OXMLi_StartElementRequest * r = q.start("VE:Choice");
	v.startElement(r);
	TFPASS(r->valid);
	TFPASS(!r->handled);

	/* the rejected subtree is swallowed but its part references are
	 * recorded on the document for the fallback representation */
	q.ctx.push_back("VE:Choice");
	q.atts.clear();
	q.atts["A:uri"] = "urn:chart";
	r = q.start("A:graphicData");
	v.startElement(r);
	TFPASS(r->valid);
	TFPASS(r->handled);
	q.atts.clear();
	q.atts["R:embed"] = "rId9";
	r = q.start("A:blip");
	v.startElement(r);
	TFPASS(r->valid);
	TFPASS(r->handled);

	OXML_Document * doc = OXML_Document::getInstance();
	TFPASS(doc != nullptr);
	TFPASS(doc->takeDroppedObjectUri() == "urn:chart");
	std::vector<std::string> rels = doc->takeDroppedObjectRels();
	TFPASS(rels.size() == 1 && rels[0] == "embed=rId9");

	/* closing the Choice drains the rejection */
	q.ctx.pop_back();
	OXMLi_EndElementRequest * e = q.end("VE:Choice");
	v.endElement(e);
	TFPASS(e->valid);
	q.ctx.push_back("VE:Choice");
	r = q.start("W:pict");
	v.startElement(r);
	TFPASS(r->valid);
	TFPASS(!r->handled);

	OXML_Document::destroyInstance();
}

TFTEST_MAIN("validator: endElement and charData paths")
{
	OXMLi_ListenerState_Valid v;
	VRq q;

	OXMLi_EndElementRequest * e = q.end("W:p");
	v.endElement(e);
	TFPASS(e->valid);
	TFPASS(!e->handled);

	e = q.end("W:notARealElement");
	v.endElement(e);
	TFPASS(!e->valid);

	OXMLi_CharDataRequest * c = q.chars("text", 4);
	v.charData(c);
	TFPASS(c->valid);
	TFPASS(!c->handled);

	c = q.chars(nullptr, 0);
	v.charData(c);
	TFPASS(!c->valid);

	/* endElement inside a rejected choice is swallowed */
	VRq r2;
	r2.ctx = {"W:document", "W:body", "VE:AlternateContent"};
	r2.atts["VE:Requires"] = "bogusns";
	OXMLi_StartElementRequest * s = r2.start("VE:Choice");
	v.startElement(s);
	r2.ctx.push_back("VE:Choice");
	e = r2.end("W:p");
	v.endElement(e);
	TFPASS(e->valid);
	TFPASS(e->handled);
}

/* export to .abwn, returning the bytes as a string */
static bool ox_export_abwn(PD_Document * doc, std::string & out)
{
	std::string tmp = std::string("/tmp/ie_oxmlvalid_") +
		std::to_string(::getpid()) + ".abwn";
	GError * err = nullptr;
	GsfOutput * file = gsf_output_stdio_new(tmp.c_str(), &err);
	bool ok = file &&
		doc->saveAs(file, static_cast<int>(IE_Exp::fileTypeForSuffix(".abwn")),
					false, nullptr) == UT_OK;
	if (file)
		g_object_unref(file);
	if (ok) {
		FILE * fp = fopen(tmp.c_str(), "rb");
		if (!fp) { ok = false; }
		else {
			fseek(fp, 0, SEEK_END);
			long sz = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			std::vector<char> buf(sz);
			ok = fread(buf.data(), 1, sz, fp) == static_cast<size_t>(sz);
			if (ok)
				out.assign(buf.data(), sz);
			fclose(fp);
		}
	}
	g_unlink(tmp.c_str());
	return ok;
}

TFTEST_MAIN("oxcov docx: uncovered element clusters import")
{
	std::string path;
	TFPASS(TF_Test::ensure_test_data("test/wp/oxcov/cov.docx", path));

	PD_Document * doc = new PD_Document;
	if (!TFPASS(doc->readFromFile(path.c_str(), IEFT_Unknown, nullptr)
			  == UT_OK)) {
		doc->unref();
		return;
	}

	std::string abwn;
	if (TFPASS(ox_export_abwn(doc, abwn))) {
		TFPASS(abwn.find("oxcov") != std::string::npos);
		TFPASS(abwn.find("cellA") != std::string::npos);   // table
		TFPASS(abwn.find("customxml") != std::string::npos);
		TFPASS(abwn.find("after-break") != std::string::npos);
		/* altChunk graft: the chunk.html part text lands in the doc */
		TFPASS(abwn.find("chunked") != std::string::npos);
	}
	doc->unref();
}
