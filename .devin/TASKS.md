# Devin autonomous task list — DOCX/DrawingML rendering fidelity

This file is the task queue for the Devin CLI task loop
(`.devin/taskloop.sh`). Each run of the loop picks the FIRST row whose
Status is `pending` or `in_progress` (crash-restart), works it to
completion, updates the row, commits, and stops.

Status values: `pending` | `in_progress` | `done` | `blocked`

Rules for the agent working this file:
- Mark the row `in_progress` before starting work, `done` only after
  building and verifying per the task's own verification hint.
- `blocked:<reason>` in Notes when genuinely stuck — never fake done.
- One task per run: after committing, STOP.
- Never revert unrelated working-tree changes.
- Append a one-line summary to `.devin/WORKLOG.md` when a task completes.

Context: repo is a GTK4 AbiWord fork (Abinova 4.0.0). The OOXML
importer lives in `src/wp/impexp/openxml/`, frame layout/render in
`src/text/fmt/xp/fl_FrameLayout.cpp` + `src/text/fmt/xp/fp_FrameContainer.cpp`,
text runs in `fp_TextRun*` + `gr_CairoGraphics.cpp` (Pango), the abwn
writer/reader in `src/wp/impexp/xp/ie_{exp,imp}_Abinova_1.cpp`, props in
`src/text/ptbl/xp/pp_Property.cpp`. Build: `cd src && make -j2`.
Headless render check: `src/abinova --to=pdf --to-name=out.pdf file.abwn`
then `pdftoppm -png -r 72 out.pdf out`. Reference docx files live in
`~/Documents/*.docx` (Austin, Badge, banded, Crop, facet, feathered,
filgree, headiness, integral, ion-dark, ion-light, retrospect, semaphore,
slice-dark, slice-light, viewmaster, whip); generated cover fragments in
`src/wp/covers/*.xml` (regenerate with `tools/mkcovers.py`).

| ID | Task | Status | Attempts | Notes |
|----|------|--------|----------|-------|
| D01 | char-spacing (w:spacing/letter-spacing): verify docx import sets it, ensure abwn serialize/deserialize, render via Pango letter-spacing attribute in gr_CairoGraphics/fp_TextRun. Verify: Badge.docx title shows 8pt tracking (letters visibly spaced). | done | 1 | rPr w:spacing was swallowed by paragraph-spacing handler — fixed by parent-element dispatch; spacing applied as cluster-advance widening in GR_CairoGraphics::shape; Badge.docx PDF shows tracked title, abwn round-trip verified |
| D02 | Font substitution: verify fonts/abinova-fonts.conf loads at runtime from build tree (xap_UnixApp.cpp loads <libDir>/fonts only if dir exists). Calibri Light must resolve to Carlito. Verify: Badge.docx title no longer overflows the seal; fc-list inside app config shows the match. | done | 1 | build-tree load worked via artwork/ walk-up (strace-confirmed conf parse); real bug was install path — stale @ABIWORD_DATADIR@/$(ABIWORD_*) vars in Makefile.am files made fonts land in literal '@ABIWORD_DATADIR@' dir; renamed all to ABINOVA_*; staged DESTDIR install puts fonts+conf at <datadir>/abinova-4.0/fonts and app loads them; fc-match 'Calibri Light'->Carlito under app config, Badge.pdf embeds Carlito |
| D03 | Line arrowheads: line-start-arrow/line-end-arrow (+ -w/-len) props are imported onto frame elements but never painted. Paint arrowheads in fp_FrameContainer (triangle/arrow/stealth/diamond/oval types, w/len sm/med/lg relative to line width). Verify: slice-light.docx arrows render at bar ends. | done | 1 | s_paintFrameArrows paints markers on bar-w/bar-h frames (sm/med/lg=2/3/5x line width, open arrow 2.5/3.5/5.5, LO marker geometry); headEnd/tailEnd gated on m_bInOutline so a14:hiddenLine copies skipped; found+fixed: top-level a:xfrm ext never captured (context off-by-one + dead gate) so vertical lines collapsed to slivers — bar-w now works; regenerated headline.xml (vertical accent bar back) + feathered.xml; synthetic docx renders all 5 types on horiz+vert bars, abwn round-trip OK; note: corpus has no real typed arrowheads (slice-light's are empty a14 ends) |
| D04 | frame-shadow: frame-shadow/frame-shadow-offset/frame-shadow-dir/frame-shadow-blur props parsed from outerShdw but not painted. Render drop shadow in fp_FrameContainer::draw before fill (cairo offset+blur or layered offset fill). Verify: any docx with outerShdw shows shadow. | done | 1 | s_paintFrameShadow rasterizes the shape silhouette (custGeom path or rect), 3-pass box-blurs alpha by blurRad, masks it in shadow color at dist/dir; rotWithShape=0 un-transforms the offset; importer now also captures shadow color + a:alpha + rotWithShape (new frame-shadow-alpha/-rot props, registered); custGeom path builder extracted to s_frameShapePath shared with s_paintFrameShape; synthetic docx->pdf shows blurred/colored/dir-offset shadows on rect+custGeom shapes, abwn round-trip verified |
| D05 | effectRef (a:effectRef idx>N inside wps:style): theme effectStyleLst gives shadow/glow per style index; resolve idx>1 to a shadow prop on the shape. Also handle wps:style lnRef/fontRef theme defaults if missing. Verify: covers' theme-styled shapes get correct shadow/color defaults. | done | 1 | theme listener now parses effectStyleLst outerShdw + lnStyleLst lines (positional, phClr placeholder); Textbox resolves lnRef->outline default, effectRef->frame-shadow-*, fontRef->default run font/color (skipping styled/colorized runs, nested frames excluded); explicit a:ln/a:effectLst in spPr still suppress refs; synthetic Austin.docx (stripped ln/effectLst, effectRef=3, lnRef=2) renders theme shadow+outline, explicit-effectLst shape unaffected, fontRef srgbClr recolors colorless run; abwn round-trip keeps frame-shadow-*; 5-doc corpus regression clean |
| D06 | Blip effects: parse a:duotone (two-color remap), a:grayscl, a:lum (bright/contrast), a:alphaModFix children of a:blip in blipFill; store as image-effect props on the element; apply at image load/paint (pixel transform or cairo ops). Verify: filgree.docx flower images render tinted per duotone colors. | done | 1 | a:blip children captured in Image listener (swallowed before Textbox sees them); duotone colors resolve schemeClr/prstClr/srgbClr/sysClr/scrgbClr/hslClr + HSL transforms; props image-duotone/-grayscale/-lum/-alpha-mod (IMG+FRAME level) applied as GdkPixbuf pixel pass in GR_UnixImage::applyBlipEffects via FG_GraphicRaster::generateImage — covers inline+frame images; fixed locale bug (sscanf %lf fails under comma-decimal locale → UT_convertDimensionless); filgree.pdf flowers render (160,172,193) matching duotone lerp(#123065->white); synthetic docx verified lum (154->100) + alphaModFix fade; Austin.docx regression clean; abw export headless broken pre-existing (fails even for .txt) |
| D07 | normAutofit: <a:normAutofit fontScale="N" lnSpcReduction="M"/> in wps:bodyPr — multiply font sizes (and line spacing) of runs in that textbox by N/100000 (and apply lnSpcReduction to line-height). Implement by propagating a scale factor into run/para prop application while inside the textbox, or a frame prop applied at layout. Verify: text in autofit boxes matches Word's scaled size. | done | 1 | importer stores frame-font-scale/-linesp-reduction fractions on the shape; fp_Run::lookupProperties pins scaled resolved font-size on a cloned span AP (style-inherited sizes scale too), fp_Line::recalcHeight shrinks line box+descent by the reduction; synthetic docx (fontScale=50000, lnSpcReduction=20000): PDF bbox shows 24.4pt->12.2pt glyphs and 28pt->11pt line pitch; abwn round-trip byte-identical layout; 5-doc corpus regression clean |
| D08 | frame-text-direction: bodyPr vert (vert270, vert, wordArtVert, eaVert) already imported to prop but never rendered. In fl_FrameLayout/fp_FrameContainer rotate text layout 90/270deg inside the box (layout width/height swap + cairo transform at paint). Verify: synthetic docx with vert270 textbox shows vertical text. | done | 1 | fp_FrameContainer::getTextRotation maps vert/eaVert/mongolianVert/wordArtVert*->90deg, vert270->270deg; content getWidth/getHeight swap so lines wrap in the rotated logical space (wrap/align/valign/overflow all follow); draw() wraps child draws in cairo translate+rotate around inner-box origin and inverse-rotates the lazy clip rect so it lands on the physical box; spAutoFit grows width; mapXYToPosition un-rotates hits — synthetic docx->pdf shows vert270 bottom-to-top L->R + vert top-to-bottom R->L boxes, wordArtVert approximates as 90deg; abwn round-trip keeps all 4 tokens; Austin/Badge/filgree/retrospect regression clean |
| D09 | a:ln completeness: cmpd (compound double lines), round/bevel/miter joins, a:gradFill outlines, and custDash. Extend outline capture + border painting. Verify: compound outlines render as double lines. | done | 1 | a:ln@cmpd/@cap/@algn, round/bevel/miter(+lim), custDash a:ds pairs and a:ln gradFill captured as frame props; cairo outline painter strokes compound strands, joins, caps, custom/preset dashes and gradient strokes on rect borders, custGeom paths and prstGeom=line bars; frame _drawLine gains double/triple/wave — synthetic docx PDF shows dbl/thickThin/tri strands, custDash dashes and green->blue gradient outline; feathered/Austin/retrospect/slice-dark regression clean |
| D10 | a:blipFill tiling (a:tile) + stretch/fillRect edge cases: tile image fill instead of stretch when a:tile present. Verify: tiled blipFill repeats across the shape. | done | 1 | a:tile/a:fillRect imported as image-tile/image-fill-rect props in both Image+Textbox listeners; fp_FrameContainer gains a cairo blipFill painter: tile grid (natural px size x sx/sy, algn anchor, tx/ty EMU offset, flip alternation via 2x supercell), stretch into fillRect subrect, srcRect crop applied once — synthetic docx PDF shows 32px motif tiled at 24pt cells with x-mirror + 0.17in/0.08in offsets; srcRect+flip=xy variant crops+mirrors correctly; feathered/Crop/banded regression clean |
| D11 | abwn round-trip for new props: ensure every new prop added above serializes in abwn export and survives re-import (they're all plain props so should be automatic — verify). Also add any missing prop registrations to pp_Property.cpp and docs (abwn.dtd + docs/ABWN-FORMAT.md). | done | 1 | all D01-D10 props round-trip loss-free (generic props serialize/reimport); char-spacing gap was already fixed in D01; registered 8 missing a:ln props (line-align/cap/compound/custom-dash/dash/join/miter-limit, outline-gradient) at PP_LEVEL_FRAME; docx->abwn->abwn multiset diff on 17-doc corpus + 9 synthetic docs: zero loss; ABWN-FORMAT.md gained §4.4 line-* table + image-tile/-fill-rect + §6 defaults rows (51->67), abwn.dtd frame comment updated; round-tripped shadow doc PDF renders shadows |
| D12 | Corpus regression + cover sweep: convert all 17 cover docx -> pdf, render each covers/*.xml fragment wrapped, eyeball-compare, fix remaining visible diffs. Update WORKLOG + CHANGELOG. | pending | 0 | run after D01-D11 |

## .doc binary import (MS-DOC spec) tasks

The .doc importer is `src/wp/impexp/xp/ie_imp_MsWord_97.cpp` (~7150 lines)
driving the vendored wvWare parser in `thirdparty/wv-1.2.9/` (wv.h,
wvparse.c, sprm.c, fib.c, etc.). Base all semantics on the official
MS-DOC spec (downloaded reference: /tmp/ms-doc.docx, spec name
[MS-DOC] v260217 — sections 2.1 streams/storages, 2.2 CP/PLC/sprm,
2.4 text/tables/properties, 2.5 FIB + FibRgFcLcb*, 2.8 stylesheet,
lists, OfficeArt blips).

| ID | Task | Status | Attempts | Notes |
|----|------|--------|----------|-------|
| DOC01 | Text retrieval + piece table: audit CLX/PlcfPcd handling in wv (wvparse.c/piecetable) — fcCompressed CP1252 vs UTF-16 pieces, far-east codepages via wvLIDToCodePageConverter, fComplex vs fast-saved docs. Verify: mixed-encoding .doc imports correct text, no dropped/reordered pieces. | pending | 0 | MS-DOC 2.4.1/2.4.2; s_convert_to_utf8 workaround at ie_imp_MsWord_97.cpp:284 |
| DOC02 | FIB validation: confirm all FibRgFcLcb97/2000/2002 offsets consumed by wv are read per spec; reject truncated/malformed FIBs gracefully (no OOB reads). Verify: fuzzed FIB variants don't crash; sane docs still open. | pending | 0 | MS-DOC 2.5; fib.c |
| DOC03 | PAP sprm audit: walk the spec's paragraph-property sprm table vs wv sprm.c handlers — spacing, indents, tabs, paragraph borders/shading, widow control, keep-with-next. Implement missing ones mapped to Abi props. Verify: .doc with tabs/borders/spacing imports with correct paragraph props. | pending | 0 | MS-DOC 2.4.6.1; sprm.c |
| DOC04 | CHP sprm audit: character props — fonts, sizes, colors, underline variants, effects (caps/smallcaps/strike), letter-spacing (sprmCDxaSpace -> char-spacing, ties to D01), highlight/shading. Verify: richly-formatted .doc renders matching char props. | pending | 0 | MS-DOC 2.4.6.2; sprm.c |
| DOC05 | Lists: LVLF/LSTF completeness — multilevel numbering, all WLNF formats, bullet char/fonts (s_fieldFontForListStyle), level linking to styles. Verify: multilevel numbered/bulleted .doc imports correct list structure. | pending | 0 | _mapDocToAbiList* ~line 500-650 |
| DOC06 | Tables via TAP: cell merge (vmerge/hmerge), cell borders/shading, row heights, nested tables; fix table-at-doc-start and table-in-frame edge cases. Verify: complex .doc tables import structurally correct. | pending | 0 | MS-DOC 2.4.3-2.4.5 |
| DOC07 | Pictures: OfficeArtBlip extraction (PNG/JPEG/WMF from Data stream + ObjectPool), PICF legacy (SUPPORTS_OLD_IMAGES), image size/crop/position props. Verify: .doc with embedded images imports them at right size. | pending | 0 | ie_imp_MsWord_97.cpp image path + wv OfficeArt code |
| DOC08 | Headers/footers + notes: plcfhdd story text, footnote/endnote anchors (plcffndTxtBte/plcfendTxtBte) -> Abi footnote/endnote struxes. Verify: .doc with footnotes+headers imports them to the right places. | pending | 0 | MS-DOC 2.3.2-2.3.5 |
| DOC09 | Fields + hyperlinks: fldChar begin/separate/end handling, VtHyperlink app data (MS-DOC 2.4.7), bookmarks -> Abi bookmarks. Verify: .doc hyperlinks/TOC fields import as Abi hyperlinks/fields. | pending | 0 | field map at ie_imp_MsWord_97.cpp:373-467 |
| DOC10 | Annotations: plcfand comments -> Abi annotation struxes with author/date from atrf* structs. Verify: commented .doc shows comments in Abi. | pending | 0 | MS-DOC 2.3.4 |
| DOC11 | Textboxes/OfficeArt: anchored drawings, FSPA geometry, escher shape props -> Abi frames where feasible. Verify: .doc with text boxes imports them as positioned frames. | pending | 0 | m_vecTextboxPos ~line 855 |
| DOC12 | Sections/page setup: SED/SEP sprms -> page size, margins, columns, orientation, section breaks. Verify: multi-section .doc keeps per-section geometry. | pending | 0 | MS-DOC SEP section |
| DOC13 | Encrypted docs: detect fibBase.fEncrypted (+ XOR/RC4 variants per MS-DOC 2.2.6) and fail with a clear error instead of producing garbage or crashing. Verify: encrypted .doc shows clean error dialog. | pending | 0 | password path exists via XAP_Dlg_Password — wire detection |
| DOC14 | .doc hardening pass: run a corpus of real .doc files through the importer under ASan or valgrind; fix memory leaks + out-of-bounds + crashes in both ie_imp_MsWord_97.cpp and thirdparty/wv-1.2.9. Verify: clean ASan/valgrind report on corpus. | pending | 0 | 158 TODO/XXX/debug sites already in importer |

## EPUB import/export modernization

EPUB lives in core at `src/wp/impexp/epub/` — importer `imp/xp/ie_imp_EPUB.cpp`
(~450 lines, old plugin-era code) and exporter `exp/xp/ie_exp_EPUB.cpp` +
`ie_exp_EPUB_EPUB3Writer.cpp` (~170 lines). EPUB spec: OCF container ->
META-INF/container.xml -> OPF package doc (metadata/manifest/spine/guide) ->
XHTML content docs + NCX (EPUB2) or nav.xhtml (EPUB3).

| ID | Task | Status | Attempts | Notes |
|----|------|--------|----------|-------|
| E01 | EPUB import audit: verify container.xml -> OPF -> spine walk; handle OPF 2.0 + 3.0 manifests, URL-decoded/relative hrefs, missing/duplicate ids, malformed zips without crashing. Verify: assorted real .epub files import text in spine order. | pending | 0 | ie_imp_EPUB.cpp |
| E02 | EPUB import fidelity: images -> Abi data items with media types, basic CSS (font-size/weight/style, alignment, margins), metadata -> doc props (dc:title/creator/language/date). Verify: epub with images+styles keeps them. | pending | 0 | check goffice libgsf zip usage |
| E03 | EPUB export correctness: OPF3 package doc (manifest/spine), nav.xhtml AND legacy NCX for compat, dc: metadata from doc props, unique identifiers, mimetype-first-stored zip layout per OCF. Verify: output passes epubcheck. | pending | 0 | ie_exp_EPUB_EPUB3Writer.cpp |
| E04 | EPUB export fidelity: embed images, emit a real CSS stylesheet from doc styles, chapter splitting policy (heading-level), cover image markup, epub:type semantic inflection for notes. Verify: exported epub renders correctly in a reader (e.g. Foliate/ebook-viewer). | pending | 0 | |
| E05 | EPUB roundtrip + validation: doc -> epub -> import back smoke test; run epubcheck if available (else manual OPF/XML validation). Add an automated-ish check in the worklog. Verify: roundtrip preserves text/images/structure. | pending | 0 | |

<!-- Loop-driver footer: untouched rows below stay pending -->
