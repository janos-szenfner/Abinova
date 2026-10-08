/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */

/* Abinova
 * Copyright (C) 1998-2000 AbiSource, Inc.
 * Copyright (c) 2016 Hubert Figuiere
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


#include <string.h>
#include <stdlib.h>

#include <cstdint>
#include <string>
#include <unordered_map>

#include "ut_types.h"
#include "ut_assert.h"
#include "ut_string.h"
#include "ut_hash.h"
#include "ut_debugmsg.h"
#include "pp_Property.h"
#include "pp_AttrProp.h"
#include "pd_Document.h"
#include "pd_Style.h"


/*****************************************************************/

/*
  TODO do we want this list of last-resort default settings to be here?
  It seems out of place... --EWS
*/
/*
	Response: I agree that it seems out of place.  It's inconsistent with
				the per-class definition and organization of the properties.
				IE, the pd_Document use of dom-dir.  We don't even have a
				PP_LEVEL_DOC represented here.		-MG
*/

/*
	We need to be able to change the BiDi relevant dafault properties at runtime
	in response to the user changing the default direction in preferences.
	Therefore cannot use constants for these three, since those are stored in
	read-only segment.
*/
	gchar def_dom_dir[]="ltr";
	gchar default_direction[]="ltr";
	gchar text_align[]="left\0";		//the '\0' is needed so that we can copy
										//the word 'right' here

// KEEP THIS ALPHABETICALLY ORDERED UNDER PENALTY OF DEATH!


/*!
 * Definitions are: Property Nme: Initial Value: Can Inherit: Pointer
 * to class : tPropLevel
 * tPropLevel should be set by or-ing the values defined in PP_Property.h
 */
static PP_Property _props[] =
{
	{ "adjust-right-ind",      "1",               false, PP_LEVEL_BLOCK}, // OOXML w:adjustRightInd
	{ "altchunk-format",       "",                false, PP_LEVEL_BLOCK}, // OOXML w:altChunk part extension
	{ "altchunk-path",         "",                false, PP_LEVEL_BLOCK}, // OOXML w:altChunk resolved part path
	{ "altcontent-kind",       "",                false, PP_LEVEL_IMG | PP_LEVEL_FRAME | PP_LEVEL_CHAR}, // OOXML a:graphicData@uri kind of an unsupported drawing payload
	{ "altcontent-part",       "",                false, PP_LEVEL_IMG | PP_LEVEL_FRAME | PP_LEVEL_CHAR}, // resolved in-package part path of the object's first rel
	{ "altcontent-rels",       "",                false, PP_LEVEL_IMG | PP_LEVEL_FRAME | PP_LEVEL_CHAR}, // "attr=rid" pairs of the object's relationship refs
	{ "altcontent-uri",        "",                false, PP_LEVEL_IMG | PP_LEVEL_FRAME | PP_LEVEL_CHAR}, // raw a:graphicData@uri of the unsupported payload
	{ "auto-space-de",         "1",               false, PP_LEVEL_BLOCK}, // OOXML w:autoSpaceDE
	{ "auto-space-dn",         "1",               false, PP_LEVEL_BLOCK}, // OOXML w:autoSpaceDN

	{ "background-color",      "transparent",     false, PP_LEVEL_SECT},
	{ "baseline-align",        "auto",            false, PP_LEVEL_BLOCK}, // OOXML w:textAlignment
	{ "bgcolor",               "transparent",     true,  PP_LEVEL_CHAR},
	{"border-merge",           "0",               true,  PP_LEVEL_BLOCK},
	{"border-shadow-merge",    "0",               true,  PP_LEVEL_BLOCK},
	{ "bot-attach",            "",               false,  PP_LEVEL_TABLE},
	{ "bot-color",             "000000",          false, PP_LEVEL_TABLE},
	{ "bot-shadow",            "0",               false, PP_LEVEL_BLOCK},
	{ "bot-shadow-color",      "grey",            false, PP_LEVEL_BLOCK},
	{ "bot-space",             "0.02in",          false, PP_LEVEL_BLOCK},
	{ "bot-style",             "1",           false, PP_LEVEL_TABLE},
	{ "bot-thickness",         "1px",             false, PP_LEVEL_TABLE},

	{ "bounding-space",        "0.05in",          false, PP_LEVEL_FRAME},

	{ "cell-fit-text",         "0",              false,  PP_LEVEL_TABLE}, // OOXML w:tcFitText
	{ "cell-hide-mark",        "0",              false,  PP_LEVEL_TABLE}, // OOXML w:hideMark
	{ "cell-margin-bottom",   "0.002in",         false,  PP_LEVEL_TABLE},
	{ "cell-margin-left",     "0.002in",         false,  PP_LEVEL_TABLE},
	{ "cell-margin-right",    "0.002in",         false,  PP_LEVEL_TABLE},
	{ "cell-margin-top",      "0.002in",         false,  PP_LEVEL_TABLE},
	{ "cell-no-wrap",          "0",              false,  PP_LEVEL_TABLE}, // OOXML w:noWrap
	{ "cell-text-direction",   "",               false,  PP_LEVEL_TABLE}, // OOXML w:textDirection

	{ "char-emphasis",         "",               true,   PP_LEVEL_CHAR}, // OOXML w:em
	{ "char-kern",             "0pt",            true,   PP_LEVEL_CHAR}, // OOXML w:kern (half-point threshold)
	{ "char-spacing",          "0pt",            true,   PP_LEVEL_CHAR}, // OOXML w:spacing
	{ "char-width",            "100",            true,   PP_LEVEL_CHAR}, // OOXML w:w (percent)

	{ "color",                 "000000",          true,  PP_LEVEL_CHAR},
	{ "column-gap",	           "0.25in",          false, PP_LEVEL_SECT},
	{ "column-line",           "off",	          false, PP_LEVEL_SECT},
	{ "columns",               "1",               false, PP_LEVEL_SECT},
	{ "contextual-spacing",    "0",               false, PP_LEVEL_BLOCK}, // OOXML w:contextualSpacing - no gap between same-style paras

	{ "default-tab-interval",  "0.5in",           false, PP_LEVEL_BLOCK},
	{ "dir-override",          nullptr,              true,  PP_LEVEL_CHAR},
	{ "display",               "inline",          true,  PP_LEVEL_CHAR},

	{ "document-auto-hyphenation",      "0",      false, PP_LEVEL_DOC},  // OOXML w:autoHyphenation
	{ "document-book-fold-printing",    "0",      false, PP_LEVEL_DOC},  // OOXML w:bookFoldPrinting
	{ "document-book-fold-rev",         "0",      false, PP_LEVEL_DOC},  // OOXML w:bookFoldRevPrinting
	{ "document-clr-scheme-mapping",    "",       false, PP_LEVEL_DOC},  // OOXML w:clrSchemeMapping
	{ "document-consecutive-hyphen-limit","",     false, PP_LEVEL_DOC},  // OOXML w:consecutiveHyphenLimit
	{ "document-decimal-symbol",        "",       false, PP_LEVEL_DOC},  // OOXML w:decimalSymbol
	{ "document-default-tab-stop",      "",       false, PP_LEVEL_DOC},  // OOXML w:defaultTabStop (twips)
	{ "document-do-not-track-formatting","0",     false, PP_LEVEL_DOC},  // OOXML w:doNotTrackFormatting
	{ "document-do-not-track-moves",    "0",      false, PP_LEVEL_DOC},  // OOXML w:doNotTrackMoves
	{ "document-even-odd-headers",      "0",      false, PP_LEVEL_DOC},  // OOXML w:evenAndOddHeaders
	{ "document-gutter-at-top",         "0",      false, PP_LEVEL_DOC},  // OOXML w:gutterAtTop
	{ "document-hyphenation-zone",      "",       false, PP_LEVEL_DOC},  // OOXML w:hyphenationZone
	{ "document-list-separator",        "",       false, PP_LEVEL_DOC},  // OOXML w:listSeparator
	{ "document-mirror-margins",        "0",      false, PP_LEVEL_DOC},  // OOXML w:mirrorMargins
	{ "document-protected",             "0",      false, PP_LEVEL_DOC},  // OOXML w:documentProtection
	{ "document-protection-mode",       "",       false, PP_LEVEL_DOC},  // OOXML w:documentProtection@edit
	{ "document-remove-date-info",      "0",      false, PP_LEVEL_DOC},  // OOXML w:removeDateAndTime
	{ "document-remove-personal-info",  "0",      false, PP_LEVEL_DOC},  // OOXML w:removePersonalInformation
	{ "document-track-changes",         "0",      false, PP_LEVEL_DOC},  // OOXML w:trackChanges
	{ "document-zoom",                  "",       false, PP_LEVEL_DOC},  // OOXML w:zoom

	{ "dom-dir",               def_dom_dir,       true,  PP_LEVEL_BLOCK | PP_LEVEL_SECT},

	{ "field-color",           "dcdcdc",          true,  PP_LEVEL_FIELD},
	{ "field-font",	           "NULL",	          true,  PP_LEVEL_FIELD},
	{ "fill-alpha",            "1.0",             false, PP_LEVEL_FRAME}, // OOXML a:alpha on fill
	{ "fill-gradient",         "",                false, PP_LEVEL_FRAME}, // OOXML a:gradFill descriptor
	{ "font-family",           "Carlito",         true,  PP_LEVEL_CHAR},
	{ "font-size",	           "12pt",	          true,  PP_LEVEL_CHAR},	// MS word defaults to 10pt, but it just seems too small
	{ "font-stretch",          "normal",          true,  PP_LEVEL_CHAR},
	{ "font-style",	           "normal",          true,  PP_LEVEL_CHAR},
	{ "font-variant",          "normal",          true,  PP_LEVEL_CHAR},
	{ "font-weight",           "normal",          true,  PP_LEVEL_CHAR},
	{ "footer",                "",                false, PP_LEVEL_SECT},
	{ "footer-even",           "",                false, PP_LEVEL_SECT},
	{ "footer-first",          "",                false, PP_LEVEL_SECT},
	{ "footer-last",           "",                false, PP_LEVEL_SECT},
	{ "format",                "%*%d.",           true,  PP_LEVEL_BLOCK},

	{"frame-col-xpos",         "0.0in",           false, PP_LEVEL_FRAME},
	{"frame-col-ypos",         "0.0in",           false, PP_LEVEL_FRAME},
	{"frame-expand-height",    "0.0in",           false, PP_LEVEL_FRAME},
	{"frame-flip-horiz",       "0",               false, PP_LEVEL_FRAME},
	{"frame-flip-vert",        "0",               false, PP_LEVEL_FRAME},
	{"frame-font-scale",       "1",               false, PP_LEVEL_FRAME}, // OOXML a:normAutofit@fontScale as fraction
	{"frame-group",            "",                false, PP_LEVEL_FRAME},
	{"frame-height",           "0.0in",           false, PP_LEVEL_FRAME},
	{"frame-hidden",           "0",               false, PP_LEVEL_FRAME},
	{"frame-horiz-align",      "left",            false, PP_LEVEL_FRAME},
	{"frame-linesp-reduction", "0",               false, PP_LEVEL_FRAME}, // OOXML a:normAutofit@lnSpcReduction as fraction
	{"frame-min-height",       "0.0in",           false, PP_LEVEL_FRAME},
	{"frame-name",             "",                false, PP_LEVEL_FRAME},
	{"frame-page-xpos",        "0.0in",           false, PP_LEVEL_FRAME},
	{"frame-page-ypos",        "0.0in",           false, PP_LEVEL_FRAME},
	{"frame-pref-column",      "0",               false, PP_LEVEL_FRAME},
	{"frame-pref-page",        "0",               false, PP_LEVEL_FRAME},
	{"frame-rel-width",        "0.5",             false, PP_LEVEL_FRAME},
	{"frame-rotation",         "0",               false, PP_LEVEL_FRAME},
	{"frame-shadow",           "none",            false, PP_LEVEL_FRAME}, // OOXML a:outerShdw presence
	{"frame-shadow-alpha",     "0.5",             false, PP_LEVEL_FRAME}, // OOXML a:outerShdw color a:alpha (0..1)
	{"frame-shadow-blur",      "0pt",             false, PP_LEVEL_FRAME}, // OOXML a:outerShdw@blurRad
	{"frame-shadow-color",     "000000",          false, PP_LEVEL_FRAME}, // OOXML a:outerShdw color
	{"frame-shadow-dir",       "0",               false, PP_LEVEL_FRAME}, // OOXML a:outerShdw@dir (60000ths of a degree)
	{"frame-shadow-offset",    "0pt",             false, PP_LEVEL_FRAME}, // OOXML a:outerShdw@dist
	{"frame-shadow-rot",       "1",               false, PP_LEVEL_FRAME}, // OOXML a:outerShdw@rotWithShape
	{"frame-stack-order",      "0",               false, PP_LEVEL_FRAME},
	{"frame-text-direction",   "",                false, PP_LEVEL_FRAME}, // OOXML wps:bodyPr@vert
	{"frame-type",             "textbox",         false, PP_LEVEL_FRAME},
	{"frame-valign",           "top",             false, PP_LEVEL_FRAME}, // OOXML wps:bodyPr@anchor
	{"frame-width",            "0.0in",           false, PP_LEVEL_FRAME},

	{ "header",                "",                false, PP_LEVEL_SECT},
	{ "header-even",           "",                false, PP_LEVEL_SECT},
	{ "header-first",          "",                false, PP_LEVEL_SECT},
	{ "header-last",           "",                false, PP_LEVEL_SECT},
	{ "header-row",            "",                false, PP_LEVEL_TABLE},
	{ "height",                "0in",             false, PP_LEVEL_CHAR},
	{ "homogeneous",           "1",               false, PP_LEVEL_CHAR},

	{ "image-alpha-mod",       "",                false, PP_LEVEL_IMG | PP_LEVEL_FRAME}, // OOXML a:alphaModFix@amt alpha multiplier 0..1
	{ "image-duotone",         "",                false, PP_LEVEL_IMG | PP_LEVEL_FRAME}, // OOXML a:duotone: "loRRGGBB hiRRGGBB"
	{ "image-fill-rect",       "",                false, PP_LEVEL_IMG | PP_LEVEL_FRAME}, // OOXML a:stretch/a:fillRect destination "l t r b" in 1000ths of percent
	{ "image-grayscale",       "0",               false, PP_LEVEL_IMG | PP_LEVEL_FRAME}, // OOXML a:grayscl
	{ "image-lum",             "",                false, PP_LEVEL_IMG | PP_LEVEL_FRAME}, // OOXML a:lum: "bright contrast" fractions
	{ "image-src-rect",        "",                false, PP_LEVEL_FRAME}, // OOXML a:srcRect crop: "l t r b" in 1000ths of percent
	{ "image-tile",            "",                false, PP_LEVEL_IMG | PP_LEVEL_FRAME}, // OOXML a:tile: "tx ty sx sy flip algn" (EMU, 1000ths pct, tokens)

	{ "keep-together",         "no",              false, PP_LEVEL_BLOCK},
	{ "keep-with-next",        "no",              false, PP_LEVEL_BLOCK},
	{ "kinsoku",               "1",               false, PP_LEVEL_BLOCK}, // OOXML w:kinsoku

	{ "lang",                  "en-US",           true,  PP_LEVEL_CHAR},
	{ "left-attach",           "",               false,  PP_LEVEL_TABLE},
	{ "left-color",            "000000",          false, PP_LEVEL_TABLE},
	{ "left-shadow",           "0",               false, PP_LEVEL_BLOCK},
	{ "left-shadow-color",     "grey",            false, PP_LEVEL_BLOCK},
	{ "left-space",            "0.02in",          false, PP_LEVEL_BLOCK},
	{ "left-style",            "1",           false, PP_LEVEL_TABLE},
	{ "left-thickness",        "1px",             false, PP_LEVEL_TABLE},

	{ "line-align",            "ctr",             false, PP_LEVEL_FRAME}, // OOXML a:ln@algn
	{ "line-cap",              "flat",            false, PP_LEVEL_FRAME}, // OOXML a:ln@cap
	{ "line-compound",         "sng",             false, PP_LEVEL_FRAME}, // OOXML a:ln@cmpd
	{ "line-custom-dash",      "",                false, PP_LEVEL_FRAME}, // OOXML a:custDash: "d sp" pairs, fractions of line width
	{ "line-dash",             "solid",           false, PP_LEVEL_FRAME}, // OOXML a:prstDash@val for prstGeom=line bars
	{ "line-end-arrow",        "none",            false, PP_LEVEL_FRAME}, // OOXML a:tailEnd@type
	{ "line-end-arrow-len",    "med",             false, PP_LEVEL_FRAME}, // OOXML a:tailEnd@len
	{ "line-end-arrow-w",      "med",             false, PP_LEVEL_FRAME}, // OOXML a:tailEnd@w
	{ "line-height",           "1.0",             false, PP_LEVEL_BLOCK},
	{ "line-join",             "miter",           false, PP_LEVEL_FRAME}, // OOXML a:ln join: round/bevel/miter
	{ "line-miter-limit",      "8",               false, PP_LEVEL_FRAME}, // OOXML a:miter@lim as a ratio
	{ "line-number-count-by",  "1",               false, PP_LEVEL_SECT},
	{ "line-number-distance",  "0in",             false, PP_LEVEL_SECT},
	{ "line-number-start",     "1",               false, PP_LEVEL_SECT},
	{ "line-numbering",        "none",            false, PP_LEVEL_SECT},
	{ "line-start-arrow",      "none",            false, PP_LEVEL_FRAME}, // OOXML a:headEnd@type
	{ "line-start-arrow-len",  "med",             false, PP_LEVEL_FRAME}, // OOXML a:headEnd@len
	{ "line-start-arrow-w",    "med",             false, PP_LEVEL_FRAME}, // OOXML a:headEnd@w
	{ "list-decimal",          ".",               true,  PP_LEVEL_BLOCK},
	{ "list-delim",            "%L",              true,  PP_LEVEL_BLOCK},
	{ "list-style",            "None",            true,  PP_LEVEL_CHAR},
	{ "list-tag",              "0",               false, PP_LEVEL_BLOCK},

	{ "margin-bottom",         "0in",             false, PP_LEVEL_BLOCK},
	{ "margin-left",           "0in",	          false, PP_LEVEL_BLOCK},
	{ "margin-right",          "0in",             false, PP_LEVEL_BLOCK},
	{ "margin-top",	           "0in",             false, PP_LEVEL_BLOCK}, // zero to be consistent with other WPs
	{ "mirror-indents",        "0",               false, PP_LEVEL_BLOCK}, // OOXML w:mirrorIndents
	{ "no-proof",              "0",               true,  PP_LEVEL_CHAR}, // OOXML w:noProof
	{ "ole-prog-id",           "",                false, PP_LEVEL_IMG | PP_LEVEL_FRAME | PP_LEVEL_CHAR}, // OOXML o:OLEObject@ProgID
	{ "ole-type",              "",                false, PP_LEVEL_IMG | PP_LEVEL_FRAME | PP_LEVEL_CHAR}, // OOXML o:OLEObject@Type (Embed/Link)

	{ "orphans",               "2",               false, PP_LEVEL_BLOCK}, // 2 to be consistent with widows & CSS
	{ "outline-gradient",      "",                false, PP_LEVEL_FRAME}, // OOXML a:ln/a:gradFill serialized gradient
	{ "outline-level",         "",                false, PP_LEVEL_BLOCK}, // OOXML w:outlineLvl
	{ "overflow-punct",        "1",               false, PP_LEVEL_BLOCK}, // OOXML w:overflowPunct

	{ "page-border-art",       "",                false, PP_LEVEL_SECT},  // OOXML w:pgBorders art names
	{ "page-border-bottom",    "none",            false, PP_LEVEL_SECT},
	{ "page-border-bottom-art","",                false, PP_LEVEL_SECT},
	{ "page-border-bottom-color","auto",          false, PP_LEVEL_SECT},
	{ "page-border-bottom-shadow","0",            false, PP_LEVEL_SECT},
	{ "page-border-bottom-space","0pt",           false, PP_LEVEL_SECT},
	{ "page-border-bottom-thickness","0pt",       false, PP_LEVEL_SECT},
	{ "page-border-display",   "all",             false, PP_LEVEL_SECT},  // OOXML w:pgBorders@display
	{ "page-border-left",      "none",            false, PP_LEVEL_SECT},
	{ "page-border-left-art",  "",                false, PP_LEVEL_SECT},
	{ "page-border-left-color","auto",            false, PP_LEVEL_SECT},
	{ "page-border-left-shadow","0",              false, PP_LEVEL_SECT},
	{ "page-border-left-space","0pt",             false, PP_LEVEL_SECT},
	{ "page-border-left-thickness","0pt",         false, PP_LEVEL_SECT},
	{ "page-border-offset",    "page",            false, PP_LEVEL_SECT},  // OOXML w:pgBorders@offsetFrom
	{ "page-border-right",     "none",            false, PP_LEVEL_SECT},
	{ "page-border-right-art", "",                false, PP_LEVEL_SECT},
	{ "page-border-right-color","auto",           false, PP_LEVEL_SECT},
	{ "page-border-right-shadow","0",             false, PP_LEVEL_SECT},
	{ "page-border-right-space","0pt",            false, PP_LEVEL_SECT},
	{ "page-border-right-thickness","0pt",        false, PP_LEVEL_SECT},
	{ "page-border-shadow",    "0",               false, PP_LEVEL_SECT},
	{ "page-border-top",       "none",            false, PP_LEVEL_SECT},
	{ "page-border-top-art",   "",                false, PP_LEVEL_SECT},
	{ "page-border-top-color", "auto",            false, PP_LEVEL_SECT},
	{ "page-border-top-shadow","0",               false, PP_LEVEL_SECT},
	{ "page-border-top-space", "0pt",             false, PP_LEVEL_SECT},
	{ "page-border-top-thickness","0pt",          false, PP_LEVEL_SECT},
	{ "page-margin-bottom",	   "1in",             false, PP_LEVEL_SECT},
	{ "page-margin-footer",    "0.0in",           false, PP_LEVEL_SECT},
	{ "page-margin-header",    "0.0in",           false, PP_LEVEL_SECT},
	{ "page-margin-left",	   "1in",             false, PP_LEVEL_SECT},
	{ "page-margin-right",     "1in",             false, PP_LEVEL_SECT},
	{ "page-margin-top",       "1in",             false, PP_LEVEL_SECT},
	{ "para-mark-rev",         "",                false, PP_LEVEL_BLOCK}, // tracked w:ins/w:del of the paragraph mark ("+id"/"-id" token, recorded-only)
	{ "position-to",           "block-above-text",false, PP_LEVEL_FRAME},
	{ "right-attach",          "",                false, PP_LEVEL_TABLE},
	{ "right-color",           "000000",          false, PP_LEVEL_TABLE},
	{ "right-shadow",          "0",               false, PP_LEVEL_BLOCK},
	{ "right-shadow-color",    "grey",            false, PP_LEVEL_BLOCK},
	{ "right-space",           "0.02in",          false, PP_LEVEL_BLOCK},
	{ "right-style",           "1",           false, PP_LEVEL_TABLE},
	{ "right-thickness",       "1px",             false, PP_LEVEL_TABLE},

	{ "section-doc-grid",      "",                false, PP_LEVEL_SECT}, // OOXML w:docGrid@type
	{ "section-doc-grid-char-space","",           false, PP_LEVEL_SECT}, // OOXML w:docGrid@charSpace
	{ "section-doc-grid-line-pitch","",           false, PP_LEVEL_SECT}, // OOXML w:docGrid@linePitch
	{ "section-endnote-suppress","0",             false, PP_LEVEL_SECT}, // OOXML w:noEndnote
	{ "section-footnote-line-thickness","0.005in",false, PP_LEVEL_SECT},
	{ "section-footnote-yoff", "0.01in",          false, PP_LEVEL_SECT},
	{ "section-form-protected","0",               false, PP_LEVEL_SECT}, // OOXML w:formProt
	{ "section-ln-count-by",   "",                false, PP_LEVEL_SECT}, // OOXML w:lnNumType@countBy
	{ "section-ln-distance",   "",                false, PP_LEVEL_SECT}, // OOXML w:lnNumType@distance
	{ "section-ln-restart",    "",                false, PP_LEVEL_SECT}, // OOXML w:lnNumType@restart
	{ "section-ln-start",      "",                false, PP_LEVEL_SECT}, // OOXML w:lnNumType@start
	{ "section-max-column-height", "0in",         false, PP_LEVEL_SECT},
	{ "section-paper-src-first","",               false, PP_LEVEL_SECT}, // OOXML w:paperSrc@first
	{ "section-paper-src-other","",               false, PP_LEVEL_SECT}, // OOXML w:paperSrc@other
	{ "section-restart",       "",                false, PP_LEVEL_SECT},
	{ "section-restart-value", "",                false, PP_LEVEL_SECT},
	{ "section-rtl-gutter",    "0",               false, PP_LEVEL_SECT}, // OOXML w:rtlGutter
	{ "section-space-after",   "0.25in",          false, PP_LEVEL_SECT},
	{ "section-text-direction","",                false, PP_LEVEL_SECT}, // OOXML w:textDirection
	{ "section-y-align",       "top",             false, PP_LEVEL_SECT}, // OOXML w:vAlign
	{"shading-background-color", "white",         false, PP_LEVEL_BLOCK},
	{"shading-foreground-color", "white",         false, PP_LEVEL_BLOCK},
	{"shading-pattern",          "0",             false, PP_LEVEL_BLOCK},
	{ "shape-path",            "",                false, PP_LEVEL_FRAME}, // OOXML a:custGeom normalized path
	{ "signature-allow-comments","1",             false, PP_LEVEL_FRAME}, // signature line: o:signatureline@allowcomments
	{ "signature-email",       "",                false, PP_LEVEL_FRAME}, // signature line: suggested signer e-mail
	{ "signature-host",        "",                false, PP_LEVEL_BLOCK}, // signature line: spacer paragraph under the floating frame
	{ "signature-id",          "",                false, PP_LEVEL_FRAME}, // signature line: unique object GUID
	{ "signature-instructions","",                false, PP_LEVEL_FRAME}, // signature line: instructions to the signer
	{ "signature-line",        "",                false, PP_LEVEL_FRAME}, // marks a signature-line textbox frame
	{ "signature-name",        "",                false, PP_LEVEL_FRAME}, // signature line: suggested signer name
	{ "signature-show-date",   "1",               false, PP_LEVEL_FRAME}, // signature line: o:signatureline@showsigndate
	{ "signature-title",       "",                false, PP_LEVEL_FRAME}, // signature line: suggested signer title
	{ "snap-to-grid",          "1",               false, PP_LEVEL_BLOCK}, // OOXML w:snapToGrid
	{ "start-value",           "1",               true,  PP_LEVEL_BLOCK},
	{ "suppress-auto-hyphens", "0",               false, PP_LEVEL_BLOCK}, // OOXML w:suppressAutoHyphens
	{ "suppress-line-numbers", "0",               false, PP_LEVEL_BLOCK}, // OOXML w:suppressLineNumbers

	{ "table-bidi-visual",     "0",               false, PP_LEVEL_TABLE}, // OOXML w:bidiVisual
	{ "table-border",          "0.1in",           false, PP_LEVEL_TABLE},
	{ "table-caption",         "",                false, PP_LEVEL_TABLE}, // OOXML w:tblCaption
	{ "table-col-spacing",     "0.03in",          false, PP_LEVEL_TABLE},
	{ "table-column-leftpos",  "0.0in",           false, PP_LEVEL_TABLE},
	{ "table-column-props",    "",                false, PP_LEVEL_TABLE},
	{ "table-description",     "",                false, PP_LEVEL_TABLE}, // OOXML w:tblDescription
	{ "table-float-halign",    "",                false, PP_LEVEL_TABLE}, // OOXML w:tblpPr@tblpXSpec
	{ "table-float-hanchor",   "",                false, PP_LEVEL_TABLE}, // OOXML w:tblpPr@horzAnchor
	{ "table-float-margin-bottom","",             false, PP_LEVEL_TABLE}, // OOXML w:tblpPr@bottomFromText
	{ "table-float-margin-left","",               false, PP_LEVEL_TABLE}, // OOXML w:tblpPr@leftFromText
	{ "table-float-margin-right","",              false, PP_LEVEL_TABLE}, // OOXML w:tblpPr@rightFromText
	{ "table-float-margin-top","",                false, PP_LEVEL_TABLE}, // OOXML w:tblpPr@topFromText
	{ "table-float-valign",    "",                false, PP_LEVEL_TABLE}, // OOXML w:tblpPr@tblpYSpec
	{ "table-float-vanchor",   "",                false, PP_LEVEL_TABLE}, // OOXML w:tblpPr@vertAnchor
	{ "table-float-x",         "",                false, PP_LEVEL_TABLE}, // OOXML w:tblpPr@tblpX
	{ "table-float-y",         "",                false, PP_LEVEL_TABLE}, // OOXML w:tblpPr@tblpY
	{ "table-line-thickness",  "0.8pt",           false, PP_LEVEL_TABLE},
	{ "table-line-type",       "1",               false, PP_LEVEL_TABLE},
	{ "table-look",            "",                false, PP_LEVEL_TABLE}, // OOXML w:tblLook bitmask
	{ "table-margin-bottom",   "0.01in",          false, PP_LEVEL_TABLE},
 	{ "table-margin-left",     "0.005in",         false, PP_LEVEL_TABLE},
	{ "table-margin-right",    "0.005in",         false, PP_LEVEL_TABLE},
	{ "table-margin-top",      "0.01in",          false, PP_LEVEL_TABLE},
	{ "table-max-extra-margin","0.05",            false, PP_LEVEL_TABLE},
	{ "table-position",        "left",            false, PP_LEVEL_TABLE}, // OOXML w:tblPr/w:jc
	{ "table-row-props",       "",                false, PP_LEVEL_TABLE},
	{ "table-row-spacing",     "0.01in",          false, PP_LEVEL_TABLE},
	{ "tabstops",              "",                false, PP_LEVEL_BLOCK},
	{ "text-align",            text_align,	      true,  PP_LEVEL_BLOCK},
	{ "text-decoration",       "none",            true,  PP_LEVEL_CHAR},
	{ "text-folded",           "0",               false, PP_LEVEL_BLOCK},
	{ "text-folded-id",        "0",               false, PP_LEVEL_BLOCK},
	{ "text-indent",           "0in",             false, PP_LEVEL_BLOCK},
	{ "text-position",         "normal",          true,  PP_LEVEL_CHAR},
	{ "text-transform",         "none",          true,  PP_LEVEL_CHAR},
	{ "text-warp",             "none",           false, PP_LEVEL_FRAME}, // OOXML a:prstTxWarp@prst
	{ "toc-dest-style1",      "Contents 1"   ,   false, PP_LEVEL_BLOCK},
	{ "toc-dest-style2",      "Contents 2",      false, PP_LEVEL_BLOCK},
	{ "toc-dest-style3",      "Contents 3",      false, PP_LEVEL_BLOCK},
	{ "toc-dest-style4",      "Contents 4",      false, PP_LEVEL_BLOCK},
	{ "toc-has-heading",       "1",               false, PP_LEVEL_BLOCK},
	{ "toc-has-label1",       "1",                false, PP_LEVEL_BLOCK},
	{ "toc-has-label2",       "1",                false, PP_LEVEL_BLOCK},
	{ "toc-has-label3",       "1",                false, PP_LEVEL_BLOCK},
	{ "toc-has-label4",       "1",                false, PP_LEVEL_BLOCK},
	{ "toc-heading",       "Contents",   false, PP_LEVEL_BLOCK},
	{ "toc-heading-style",  "Contents Header",    false, PP_LEVEL_BLOCK},
    { "toc-id",                "0",               false, PP_LEVEL_SECT},
	{ "toc-indent1",           "0.5in",           false, PP_LEVEL_BLOCK},
	{ "toc-indent2",           "0.5in",           false, PP_LEVEL_BLOCK},
	{ "toc-indent3",           "0.5in",           false, PP_LEVEL_BLOCK},
	{ "toc-indent4",           "0.5in",           false, PP_LEVEL_BLOCK},
	{ "toc-label-after1",       "",               false, PP_LEVEL_BLOCK},
	{ "toc-label-after2",       "",               false, PP_LEVEL_BLOCK},
	{ "toc-label-after3",       "",               false, PP_LEVEL_BLOCK},
	{ "toc-label-after4",       "",               false, PP_LEVEL_BLOCK},
	{ "toc-label-before1",       "",              false, PP_LEVEL_BLOCK},
	{ "toc-label-before2",       "",              false, PP_LEVEL_BLOCK},
	{ "toc-label-before3",       "",              false, PP_LEVEL_BLOCK},
	{ "toc-label-before4",       "",              false, PP_LEVEL_BLOCK},
	{ "toc-label-inherits1",       "1",           false, PP_LEVEL_BLOCK},
	{ "toc-label-inherits2",       "1",           false, PP_LEVEL_BLOCK},
	{ "toc-label-inherits3",       "1",           false, PP_LEVEL_BLOCK},
	{ "toc-label-inherits4",       "1",           false, PP_LEVEL_BLOCK},
	{ "toc-label-start1",       "1",              false, PP_LEVEL_BLOCK},
	{ "toc-label-start2",       "1",              false, PP_LEVEL_BLOCK},
	{ "toc-label-start3",       "1",              false, PP_LEVEL_BLOCK},
	{ "toc-label-start4",       "1",              false, PP_LEVEL_BLOCK},
	{ "toc-label-type1",       "numeric",         false, PP_LEVEL_BLOCK},
	{ "toc-label-type2",       "numeric",         false, PP_LEVEL_BLOCK},
	{ "toc-label-type3",       "numeric",         false, PP_LEVEL_BLOCK},
	{ "toc-label-type4",       "numeric",         false, PP_LEVEL_BLOCK},
	{ "toc-level",             "",                false, PP_LEVEL_BLOCK},
	{ "toc-page-type1",        "numeric",         false, PP_LEVEL_BLOCK},
	{ "toc-page-type2",        "numeric",         false, PP_LEVEL_BLOCK},
	{ "toc-page-type3",        "numeric",         false, PP_LEVEL_BLOCK},
	{ "toc-page-type4",        "numeric",         false, PP_LEVEL_BLOCK},
	{ "toc-range-bookmark",    "",                false, PP_LEVEL_BLOCK},
	{ "toc-source-style1",     "Heading 1",       false, PP_LEVEL_BLOCK},
	{ "toc-source-style2",     "Heading 2",       false, PP_LEVEL_BLOCK},
	{ "toc-source-style3",     "Heading 3",       false, PP_LEVEL_BLOCK},
	{ "toc-source-style4",     "Heading 4",       false, PP_LEVEL_BLOCK},
	{ "toc-tab-leader1",       "dot",             false, PP_LEVEL_BLOCK},
	{ "toc-tab-leader2",       "dot",             false, PP_LEVEL_BLOCK},
	{ "toc-tab-leader3",       "dot",             false, PP_LEVEL_BLOCK},
	{ "toc-tab-leader4",       "dot",             false, PP_LEVEL_BLOCK},

	{ "top-attach",             "",               false, PP_LEVEL_TABLE},
	{ "top-color",             "000000",          false, PP_LEVEL_TABLE},
	{ "top-line-punct",        "0",               false, PP_LEVEL_BLOCK}, // OOXML w:topLinePunct
	{ "top-shadow",            "0",               false, PP_LEVEL_BLOCK},
	{ "top-shadow-color",      "grey",            false, PP_LEVEL_BLOCK},
	{ "top-space",             "0.02in",          false, PP_LEVEL_BLOCK},
	{ "top-style",             "1",               false, PP_LEVEL_TABLE},
	{ "top-thickness",         "1px",             false, PP_LEVEL_TABLE},


	{ "vert-align",            "0",               false, PP_LEVEL_TABLE},
	{ "vert-position",         "0pt",             true,  PP_LEVEL_CHAR}, // OOXML w:position (raise/lower)

	{ "widows",                "2",               false, PP_LEVEL_BLOCK},
	{ "width",                 "0in",             false, PP_LEVEL_CHAR},
	{ "word-wrap",             "1",               false, PP_LEVEL_BLOCK}, // OOXML w:wordWrap
	{ "wrap-mode",             "above-text",      false, PP_LEVEL_FRAME},
	{ "xpad",                  "0.03in",          false, PP_LEVEL_FRAME},
	{ "xpad-left",             "",                false, PP_LEVEL_FRAME}, // OOXML lIns; empty = use xpad
	{ "xpad-right",            "",                false, PP_LEVEL_FRAME}, // OOXML rIns; empty = use xpad
	{ "xpos",                  "0.0in",           false, PP_LEVEL_FRAME},
	{ "ypad",                  "0.03in",          false, PP_LEVEL_FRAME},
	{ "ypad-bottom",           "",                false, PP_LEVEL_FRAME}, // OOXML bIns; empty = use ypad
	{ "ypad-top",              "",                false, PP_LEVEL_FRAME}, // OOXML tIns; empty = use ypad
	{ "ypos",                  "0.0in",           false, PP_LEVEL_FRAME}
};

static int s_compare (const void * a, const void * b)
{
  const PP_Property * prop;
  const char * name;

  name = static_cast<const char *>(a);
  prop = static_cast<const PP_Property *>(b);

  return strcmp (name, prop->getName());
}

/*****************************************************************/

/*
 * Memoization cache for PP_evalProperty.
 *
 * Evaluating a property walks up to three attribute/property lists, the
 * style(s) named by their attributes (following the basedOn chain), the
 * document AP and finally the static initial value -- for every run of
 * every format pass.  On large documents this made PP_evalProperty the
 * single hottest function in the profile.
 *
 * The result is fully determined by (property, span AP, block AP, section
 * AP, document, expandStyles) because:
 *  - an AP marked read-only is immutable and owned by a container that
 *    lives as long as the document (the piece-table varset or a
 *    pp_TableAttrProp table), so its address cannot be recycled while
 *    the document lives;
 *  - style contents change only by replacing PD_Style::m_indexAP, and the
 *    style table changes only via pt_PieceTable::appendStyle /
 *    removeStyle / _createBuiltinStyle;
 *  - document properties change only via PD_Document::setAttrProp /
 *    setAttributes / setProperties;
 *  - the static initial values change only via PP_resetInitialBiDiValues /
 *    PP_setDefaultFontFamily;
 * and all of those call PP_invalidateEvalPropertyCache().  Document
 * destruction also invalidates, so recycled heap addresses cannot alias
 * stale keys.
 *
 * Callers may also pass short-lived, never-registered APs (e.g. the
 * scaled-font clone fp_Run::lookupProperties builds for normAutofit
 * frames): those are mutable and their addresses can be recycled once
 * freed, so evaluations involving a non-read-only AP are never cached.
 *
 * Cached values point into AP-owned storage or the static initial
 * values, which outlive the cache, so pointers returned to callers stay
 * valid across invalidations just as they did before caching existed.
 */
namespace {

struct PP_EvalCacheKey
{
	const PP_Property * prop;
	const PP_AttrProp * spanAP;
	const PP_AttrProp * blockAP;
	const PP_AttrProp * sectionAP;
	const PD_Document * doc;
	bool                expandStyles;

	bool operator==(const PP_EvalCacheKey & o) const
	{
		return prop == o.prop && spanAP == o.spanAP && blockAP == o.blockAP
			&& sectionAP == o.sectionAP && doc == o.doc
			&& expandStyles == o.expandStyles;
	}
};

struct PP_EvalCacheKeyHash
{
	std::size_t operator()(const PP_EvalCacheKey & k) const
	{
		const std::size_t mix = 0x9e3779b97f4a7c15ULL;
		std::size_t h = reinterpret_cast<std::uintptr_t>(k.prop) >> 4;
		h = h * mix + (reinterpret_cast<std::uintptr_t>(k.spanAP) >> 4);
		h = h * mix + (reinterpret_cast<std::uintptr_t>(k.blockAP) >> 4);
		h = h * mix + (reinterpret_cast<std::uintptr_t>(k.sectionAP) >> 4);
		h = h * mix + (reinterpret_cast<std::uintptr_t>(k.doc) >> 4);
		return h ^ (k.expandStyles ? mix : 0);
	}
};

// Bound the map on pathological documents; overflowing simply clears and
// refills.
constexpr std::size_t PP_EVAL_CACHE_MAX = 262144;

// nullptr values are cached too: they are genuine results (e.g. a
// property whose initial value is nullptr, like "dir-override").
std::unordered_map<PP_EvalCacheKey, const gchar *, PP_EvalCacheKeyHash> s_evalCache;

// Escape hatch for perf a/b measurement and stale-cache debugging:
// ABINOVA_EVAL_CACHE=off disables memoization entirely.
const bool s_bEvalCacheEnabled = [] {
	const char * v = getenv("ABINOVA_EVAL_CACHE");
	return !(v && (strcmp(v, "off") == 0 || strcmp(v, "0") == 0));
}();

// Bumped on every invalidation so pointer-keyed callers (e.g. the
// fp_Run lookupProperties memo) can tell when AP contents may have
// mutated in place under stable AP pointers.
UT_uint32 s_evalGeneration = 1;

}

/*!
 * Drop all memoized PP_evalProperty results.  Must be called whenever
 * anything the evaluation depends on changes: style contents or the
 * style table, the document AP, the static initial values, or the
 * document itself going away.
 */
void PP_invalidateEvalPropertyCache()
{
	s_evalCache.clear();
	++s_evalGeneration;
}

UT_uint32 PP_evalPropertyGeneration()
{
	return s_evalGeneration;
}

bool PP_evalPropertyCacheEnabled()
{
	return s_bEvalCacheEnabled;
}

const PP_Property * PP_lookupProperty(const gchar * name)
{
	PP_Property * prop = nullptr;

	prop = static_cast<PP_Property *>(bsearch (name, _props, G_N_ELEMENTS(_props), sizeof (_props[0]), s_compare));

	return prop;
}

//allows us to reset the default value for the direction settings;
void PP_resetInitialBiDiValues(const gchar * pszValue)
{
	int i;
	int count = G_N_ELEMENTS(_props);

	for (i=0; i<count; i++)
	{
		if (/*(0 == strcmp(_props[i].m_pszName, "dir"))
		  ||*/(0 == strcmp(_props[i].m_pszName, "dom-dir"))
		  /*||(0 == strcmp(_props[i].m_pszName, "column-order"))*/)
		  //this last one is not necessary since dom-dir and column-order
		  //share the same physical string
		{
			_props[i].m_pszInitial = pszValue;
		}
		else if ((0 == strcmp(_props[i].m_pszName, "text-align")))
		{
			UT_DEBUGMSG(("reseting text-align (%s)\n", pszValue));
			if(pszValue[0] == static_cast<gchar>('r')) {
				_props[i].m_pszInitial = "right";
			}
			else {
				_props[i].m_pszInitial = "left";
			}
			break; //since the list is alphabetical, this is always the last one
		}
	}
	PP_invalidateEvalPropertyCache();
}

void PP_setDefaultFontFamily(const char* pszFamily)
{
	static std::string family(pszFamily ? pszFamily : "");
	PP_Property* prop = static_cast<PP_Property*>(bsearch ("font-family", _props, G_N_ELEMENTS(_props), sizeof(_props[0]), s_compare));
	UT_nonnull_or_return(prop, );
	prop->m_pszInitial = family.c_str();
	PP_invalidateEvalPropertyCache();
}

static PD_Style * _getStyle(const PP_AttrProp * pAttrProp, const PD_Document * pDoc)
{
	PD_Style * pStyle = nullptr;

	const gchar * szValue = nullptr;
//
// SHIT. This is where the style/name split gets really hairy. This index AP MIGHT be
// from a style definition in which case the name of the style is PT_NAME_ATTRIBUTE_NAME
// or it might be from the document in which case the attribute is
// PT_STYLE_ATTRIBUTE_NAME. Fuck it, try both. - MES.
//
	if (pAttrProp->getAttribute(PT_NAME_ATTRIBUTE_NAME, szValue))
	{
		UT_return_val_if_fail (szValue && szValue[0], nullptr);
		if (pDoc)
			pDoc->getStyle(reinterpret_cast<const char*>(szValue), &pStyle);

		// NOTE: we silently fail if style is referenced, but not defined
	}
    else if(pAttrProp->getAttribute(PT_STYLE_ATTRIBUTE_NAME, szValue))
	{
		UT_return_val_if_fail (szValue && szValue[0], nullptr);
		if (pDoc)
			pDoc->getStyle(reinterpret_cast<const char*>(szValue), &pStyle);

		// NOTE: we silently fail if style is referenced, but not defined
	}

	return pStyle;
}

static const gchar * s_evalProperty (const PP_Property * pProp,
										const PP_AttrProp * pAttrProp,
										const PD_Document * pDoc,
										bool bExpandStyles)
{
	const gchar * szValue = nullptr;

	if (pAttrProp->getProperty (pProp->getName(), szValue))
		{
			return szValue;
		}
	if (!bExpandStyles) {
		return nullptr;
	}

	PD_Style * pStyle = _getStyle (pAttrProp, pDoc);

	int i = 0;
	while (pStyle && (i < pp_BASEDON_DEPTH_LIMIT))
		{
			if (pStyle->getProperty (pProp->getName (), szValue))
				{
					return szValue;
				}
			pStyle = pStyle->getBasedOn ();
			i++;
		}
	return nullptr;
}

const gchar * PP_evalProperty (const gchar *  pszName,
								  const PP_AttrProp * pSpanAttrProp,
								  const PP_AttrProp * pBlockAttrProp,
								  const PP_AttrProp * pSectionAttrProp,
								  const PD_Document * pDoc,
								  bool bExpandStyles)
{
	// find the value for the given property
	// by evaluating it in the contexts given.
	// use the CSS inheritance as necessary.

	if (!pszName || !*pszName)
	{
		UT_DEBUGMSG(("PP_evalProperty: null property given\n"));
		return nullptr;
	}

	if (pDoc == nullptr)
		bExpandStyles = false;

	const PP_Property * pProp = PP_lookupProperty(pszName);
	if (!pProp)
	{
		UT_DEBUGMSG(("PP_evalProperty: unknown property \'%s\'\n",pszName));
		return nullptr;
	}

	// Only APs adopted by a document-lifetime container (varset /
	// pp_TableAttrProp) are marked read-only; a transient AP may be
	// mutated or freed and its address recycled, so it is never a valid
	// cache key component.
	const bool bCacheable = s_bEvalCacheEnabled
		&& (!pSpanAttrProp || pSpanAttrProp->isReadOnly())
		&& (!pBlockAttrProp || pBlockAttrProp->isReadOnly())
		&& (!pSectionAttrProp || pSectionAttrProp->isReadOnly());

	const PP_EvalCacheKey cacheKey = { pProp, pSpanAttrProp, pBlockAttrProp,
									   pSectionAttrProp, pDoc, bExpandStyles };
	if (bCacheable)
	{
		auto it = s_evalCache.find(cacheKey);
		if (it != s_evalCache.end())
			return it->second;
	}

	/* Not all properties can have a value of inherit, but we're not validating here.
	 * This is not to be confused with automatic inheritance - the difference is whether
	 * to take the default value (for when no value is specified).
	 */
	bool bInherit = false;

	// see if the property is on the Span item.

	const gchar * szValue = nullptr;

	// TODO: ?? make lookup more efficient by tagging each property with scope (block, char, section)

	if (pSpanAttrProp)
	{
		szValue = s_evalProperty (pProp, pSpanAttrProp, pDoc, bExpandStyles);

		if (szValue)
			if (strcmp (szValue, "inherit") == 0)
			{
				szValue = nullptr;
				bInherit = true;
			}
		if ((szValue == nullptr) && (bInherit || pProp->canInherit ()))
		{
			bInherit = false;

			if (pBlockAttrProp)
			{
				szValue = s_evalProperty (pProp, pBlockAttrProp, pDoc, bExpandStyles);

				if (szValue)
					if (strcmp (szValue, "inherit") == 0)
					{
						szValue = nullptr;
						bInherit = true;
					}
				if ((szValue == nullptr) && (bInherit || pProp->canInherit ()))
				{
					bInherit = false;

					if (pSectionAttrProp)
					{
						szValue = s_evalProperty (pProp, pSectionAttrProp, pDoc, bExpandStyles);

						if (szValue)
							if (strcmp (szValue, "inherit") == 0)
							{
								szValue = nullptr;
								bInherit = true;
							}
						if ((szValue == nullptr) && (bInherit || pProp->canInherit ()))
						{
							const PP_AttrProp * pDocAP = pDoc->getAttrProp ();
							if (pDocAP)
								pDocAP->getProperty (pszName, szValue);
						}
					}
				}
			}
		}
	}
	else if (pBlockAttrProp)
	{
		szValue = s_evalProperty (pProp, pBlockAttrProp, pDoc, bExpandStyles);

		if (szValue)
			if (strcmp (szValue, "inherit") == 0)
			{
				szValue = nullptr;
				bInherit = true;
			}
		if ((szValue == nullptr) && (bInherit || pProp->canInherit ()))
		{
			bInherit = false;

			if (pSectionAttrProp)
			{
				szValue = s_evalProperty (pProp, pSectionAttrProp, pDoc, bExpandStyles);

				if (szValue)
					if (strcmp (szValue, "inherit") == 0)
					{
						szValue = nullptr;
						bInherit = true;
					}
				if ((szValue == nullptr) && (bInherit || pProp->canInherit ()))
				{
					const PP_AttrProp * pDocAP = pDoc->getAttrProp ();
					if (pDocAP)
						pDocAP->getProperty (pszName, szValue);
				}
			}
		}
	}
	else if (pSectionAttrProp)
	{
		szValue = s_evalProperty (pProp, pSectionAttrProp, pDoc, bExpandStyles);

		if (szValue)
			if (strcmp (szValue, "inherit") == 0)
			{
				szValue = nullptr;
				bInherit = true;
			}
		if ((szValue == nullptr) && (bInherit || pProp->canInherit ()))
		{
			const PP_AttrProp * pDocAP = pDoc->getAttrProp ();
			if (pDocAP)
				pDocAP->getProperty (pszName, szValue);
		}
	}
	else
	{
		const PP_AttrProp * pDocAP = pDoc->getAttrProp ();
		if (pDocAP)
		{
			pDocAP->getProperty (pszName, szValue);

			// dom-dir requires special treatment at document level
			if(szValue && strcmp(pszName, "dom-dir") == 0)
			{
				if(   strcmp(szValue, "logical-ltr") == 0
				   || strcmp(szValue, "logical-rtl") == 0)
					szValue += 8;
			}
		}
	}
	if (szValue)
		if (strcmp (szValue, "inherit") == 0) // shouldn't happen, but doesn't hurt to check
			szValue = nullptr;

	if (szValue == nullptr)
		if (bExpandStyles)
		{
			PD_Style * pStyle = nullptr;

			if (pDoc->getStyle ("Normal", &pStyle))
			{
				/* next to last resort -- check for this property in the Normal style
				 */
				pStyle->getProperty (pszName, szValue);

				if (szValue)
					if (strcmp (szValue, "inherit") == 0)
						szValue = nullptr;
			}
		}

	if(szValue == nullptr && pDoc && (bInherit || pProp->canInherit ()))
	{
		// see if the doc has a value for this prop
		const PP_AttrProp *  pAP = pDoc->getAttrProp();
		if(pAP)
		{
			pAP->getProperty(pszName, szValue);
		}
	}
	
	if (szValue == nullptr)
		szValue = pProp->getInitial (); // which may itself be nullptr, but that is a bad thing - FIXME!!

	if (bCacheable)
	{
		if (s_evalCache.size() >= PP_EVAL_CACHE_MAX)
			s_evalCache.clear();
		s_evalCache.emplace(cacheKey, szValue);
	}

	return szValue;
}

std::unique_ptr<PP_PropertyType> PP_evalPropertyType(const gchar *  pszName,
								 const PP_AttrProp * pSpanAttrProp,
								 const PP_AttrProp * pBlockAttrProp,
								 const PP_AttrProp * pSectionAttrProp,
								 tProperty_type Type,
								 const PD_Document * pDoc,
								 bool bExpandStyles)
{
	// find the value for the given property
	// by evaluating it in the contexts given.
	// use the CSS inheritance as necessary.

	if (!pszName || !*pszName)
	{
		UT_DEBUGMSG(("PP_evalProperty: null property given\n"));
		return nullptr;
	}

	std::unique_ptr<PP_PropertyType> p_property;
	const PP_Property * pProp = PP_lookupProperty(pszName);
	if (!pProp)
	{
		UT_DEBUGMSG(("PP_evalProperty: unknown property \'%s\'\n",pszName));
		return nullptr;
	}

	PD_Style * pStyle = nullptr;

	// TODO: make lookup more efficient by tagging each property with scope (block, char, section)

	// see if the property is on the Span item.

	if (pSpanAttrProp)
	{
		p_property = pSpanAttrProp->getPropertyType(pProp->getName(), Type);
		if(p_property)
			return p_property;

		if (bExpandStyles)
		{
			pStyle = _getStyle(pSpanAttrProp, pDoc);

			int i = 0;
			while (pStyle && (i < pp_BASEDON_DEPTH_LIMIT))
			{
				p_property = pStyle->getPropertyType(pProp->getName(), Type);
				if(p_property)
					return p_property;

				pStyle = pStyle->getBasedOn();
				i++;
			}
		}
	}

	// otherwise, see if we can inherit it from the containing block or the section.

	if (!pSpanAttrProp || pProp->canInherit())
	{
		if (pBlockAttrProp)
		{
			p_property = pBlockAttrProp->getPropertyType(pProp->getName(), Type);
			if(p_property)
				return p_property;

			if (bExpandStyles)
			{
				pStyle = _getStyle(pBlockAttrProp, pDoc);

				int i = 0;
				while (pStyle && (i < pp_BASEDON_DEPTH_LIMIT))
				{
					p_property = pStyle->getPropertyType(pProp->getName(),  Type);
					if(p_property)
						return p_property;

					pStyle = pStyle->getBasedOn();
					i++;
				}
			}
		}

		if (!pBlockAttrProp || pProp->canInherit())
		{
			if (pSectionAttrProp)
			{
				p_property =  pSectionAttrProp->getPropertyType(pProp->getName(), Type);
				if(p_property)
					return p_property;
			}
		}
	}

	if (pDoc->getStyle("Normal", &pStyle))
	{
		// next to last resort -- check for this property in the Normal style
		p_property = pStyle->getPropertyType(pProp->getName(),  Type);
		if(p_property)
			return p_property;
	}

	// if no inheritance allowed for it or there is no
	// value set in containing block or section, we return
	// the default value for this property.

	return pProp->getInitialType(Type);
}

UT_uint32        PP_getPropertyCount()
{
	return (sizeof(_props)/sizeof(PP_Property));
}

const gchar * PP_getNthPropertyName(UT_uint32 n)
{
	return _props[n].getName();
}

tPropLevel   PP_getNthPropertyLevel(UT_uint32 n)
{
	return _props[n].getLevel();
}



PP_Property::~PP_Property()
{
}


std::unique_ptr<PP_PropertyType> PP_Property::getInitialType(tProperty_type Type) const
{
	// XXX this used to be cached....
	return PP_PropertyType::createPropertyType(Type, m_pszInitial);
}

std::unique_ptr<PP_PropertyType> PP_PropertyType::createPropertyType(tProperty_type Type, const gchar *p_init)
{
	switch(Type)
	{
	case Property_type_color:
		return std::unique_ptr<PP_PropertyType>(new PP_PropertyTypeColor(p_init));
		break;

	case Property_type_bool:
		return std::unique_ptr<PP_PropertyType>(new PP_PropertyTypeBool(p_init));
		break;

	case Property_type_int:
		return std::unique_ptr<PP_PropertyType>(new PP_PropertyTypeInt(p_init));
		break;

	case Property_type_size:
		return std::unique_ptr<PP_PropertyType>(new PP_PropertyTypeSize(p_init));
		break;

	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		break;
	}

	return std::unique_ptr<PP_PropertyType>();
}

PP_PropertyTypeColor::PP_PropertyTypeColor(const gchar *p_init)
{
	UT_parseColor(p_init, Color);
}

PP_PropertyTypeBool::PP_PropertyTypeBool(const gchar *p_init)
{
	State = (strcmp("yes", p_init) != 0);
}

PP_PropertyTypeInt::PP_PropertyTypeInt(const gchar *p_init)
{
	Value = atoi(p_init);
}

PP_PropertyTypeSize::PP_PropertyTypeSize(const gchar *p_init)
{
	Value = UT_convertDimensionless(p_init);
	Dim = UT_determineDimension(p_init);
}
