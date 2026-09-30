# Abinova work log — crash-resume checkpoint

> **If Devin Desktop crashed and this is a new session:** read this file
> fully, then say "continue" — resume from the *Next steps* section.
> Update this file after every completed chunk (keep it small).

## Project

- Repo: `/home/szefi/Documents/exp-abi` (GTK4 AbiWord fork → Abinova 4.0.0)
- Build: `cd src && make -j2`  (build ONLY from `src/` — `make -C wp`
  rebuilds the static lib but does NOT relink the `abinova` binary!)
- Binary: `src/abinova` (libtool wrapper) / `src/.libs/abinova`
- Import-to-file test: `src/abinova --to=abwn --to-name=OUT.abwn IN.docx`
- PDF render check: `--to=pdf`
- Corpus: `/home/szefi/Documents/*.docx` (25 files incl. Austin, Badge,
  viewmaster, slice-dark, feathered, headiness, integral, crop-header…);
  extracted XML under `/tmp/allx/<name>/word/document.xml`
- Spec: `/home/szefi/Documents/[MS-DOCX]-260818.docx`
- Debug log: `~/abinova_dbg.log`; watchdog: `~/abi-work/devin_watchdog.sh`
  (log `~/devin-watchdog.log`)
- App launch (2 GB RSS cap, NOT ulimit -v — that kills GTK threads):
  `systemd-run --user --scope -p MemoryMax=2G --unit=abinova-app ...`

## Long-term goal

Ribbon-only UI (classic menus removed), obsolete plugins removed,
DOCX/ODT/EPUB/Markdown/LaTeX integrated in core, harden GTK4 port,
Fluent-style artwork. Version 4.0.0.

## Current task (2026-09-29)

**DOCX/OOXML importer hardening per [MS-DOCX].** Cover pages still render
wrong — visual fidelity work is DEFERRED until the schema audit is done.

### Done this session (all built + 25-file sweep OK)

- strict OOXML (purl.oclc.org) namespaces + custom .rels fallback parser
- `mc:Choice @Requires` evaluation → proper Choice/Fallback selection
- `wp:anchor@relativeHeight` → `frame-stack-order` z-order
- `wp:align` → page position; `wp14:pctPos*/pctW/pctH` percent sizing
- `wpg:wgp` group transforms (`a:off/ext/chOff/chExt`), `pic:pic` children
- `wps:wsp` shapes → frames; solidFill/gradFill(first stop)/fillRef/grpFill/
  blipFill; `a:ln` outlines (color/dash/width); `prstGeom="line"` → bars;
  `wps:bodyPr` insets → xpad/ypad; theme colors + lumMod/lumOff/tint/shade/alpha
- header/footer textboxes flatten inline (frames don't lay out in hdrftr)
- `w:framePr` → paragraph wrapped in frame strux (x/y/w/h/hAnchor/vAnchor/wrap)
- `w:keepNext`/`keepLines`/`widowControl`/`bidi` paragraph props
- `w:caps`/`smallCaps`/`w`/`rtl`/`specVanish`/`webHidden` run props
- `w:tab`/`noBreakHyphen`/`softHyphen`/`sym` (Symbol charset table)/
  `lastRenderedPageBreak`
- `wp:wrapTight`/`wrapThrough`/`wrapTopAndBottom`/`wrapNone`
- `w:tblW`/`tblInd`/`tblCellMar`/`tcMar`/`tblCellSpacing`/`vAlign`(cell)/
  `tblHeader`→header-row
- `w:titlePg` gate on first-page hdrftr refs; `w:cols@space` → column-gap
- `w:lvl>w:rPr` bullet formatting gate fix; `w:contextualSpacing` `&&`→`||` fix
- VML `v:rect`/`roundrect`/`oval`/`line`/`polyline` fallback shapes → frames
- null-checks in PackageManager rels; unknown-namespaced attributes preserved

### Done this session (cont.)

- `w:footnotePr`/`w:endnotePr` in settings.xml: `numFmt`→`document-*-type`
  (decimal/lowerLetter/upperLetter/lowerRoman/upperRoman→numeric/lower/
  upper/lower-roman/upper-roman; chicago→numeric), `numStart`→`*-initial`,
  `numRestart` `eachSect`/`eachPage`→`*-restart-section`/`-restart-page`,
  `pos` `sectEnd`/`docEnd`→`document-endnote-place-*`. Store = new
  `OXML_Document::m_docProps` map + `setDocProperty()`, applied via
  `pDocument->setProperties()` at end of `addToPT`. Verified: injected
  lowerRoman/start=3/eachPage + upperLetter/sectEnd all land in .abwn.
  Files: `OXML_Document.{h,cpp}`, `OXMLi_ListenerState_DocSettings.{h,cpp}`.
- `w:pgNumType@start` → `section-restart`/`section-restart-value`
  (Common listener, sectPr block).
- `a:spAutoFit` → `frame-expand-height` (Textbox listener; `noAutofit`
  consumed). `wps:bodyPr` now marked handled in endElement.
- `wp:simplePos` x/y honored when `wp:anchor@simplePos="1"`
  (`m_bSimplePos` flag in Image listener; corpus uses simplePos="0").
- Corpus element audit: every `w:` element in all 25 document.xml parts
  is now claimed; remaining unhandled = VML type defs (`v:shapetype`/
  `v:formulas`/`v:handles`/`v:path`/`v:stroke`), `w14:ligatures`,
  `w15:appearance`, math settings, locks — all ignorable.
- 28-file sweep (incl. strict Austin + footnote test) OK=28 FAIL=0.
- Watchdog `~/abi-work/devin_watchdog.sh` runs as persistent systemd user
  service `devin-watchdog.service` (enabled, Restart=always; log
  `~/devin-watchdog.log`). Three crash-detection layers: process exit,
  `renderer process gone (reason: crashed` in newest
  `~/.config/Devin/logs/*/main.log` (the code-4 dialog case where the
  process stays alive), and fresh Crashpad `.dmp` in
  `~/.config/Devin/Crashpad`. 20s grace for the in-app Restart button,
  10 restarts/h cap. GNOME Shell Eval is disabled and Devin is
  ozone-platform=wayland, so dialog button-clicks aren't scriptable —
  watchdog kills the tree and respawns instead.
- Last commit pushed: `24df7db` (schema coverage batch).
- pp_Property.cpp: +~70 new props (page-border-*, section-y-align/doc-grid/
  ln-*/paper-src/text-direction/rtl-gutter/endnote-suppress/form-protected,
  table-position/float-*/look/caption/description/bidi-visual,
  cell-text-direction/no-wrap/fit-text/hide-mark, char-emphasis/kern/
  spacing/width, vert-position, no-proof, outline-level, kinsoku/
  snap-to-grid/word-wrap/etc, frame-valign/text-direction/shadow-*,
  line-*-arrow*, image-src-rect, shape-path, text-warp, fill-alpha/
  fill-gradient, document-* settings bag). Table still sorted (verified).
- COMMENTS implemented end-to-end (word/comments.xml):
  new `OXMLi_ListenerState_Comments` (w:comment → OXML_Section with
  annotation-author/-date/-initials props), `OXML_Document::m_annotations`
  + add/get/clear, `OXML_Section::addToPTAsAnnotation` (packs props into
  strux "props" attr), new `OXML_Element_Annotation` (ANNOT_TAG/ANNOT:
  commentRangeStart emits PTO_Annotation start + inline comment shadow,
  commentRangeEnd emits anonymous end object; commentReference/annotationRef
  consumed silently), `parseDocumentComments` + COMMENTS_PART setupStates
  + importer call. Verified: synthetic docx → `<ann annotation-id>` +
  `<annotate props="annotation-author...">` in .abwn. Sweep OK=25 FAIL=0.
- DocSettings batch: clrSchemeMapping→document-clr-scheme-mapping,
  documentProtection/writeProtection→document-protected+protection-mode,
  zoom→document-zoom, defaultTabStop/hyphenationZone→document-* (pt),
  decimalSymbol/listSeparator/consecutiveHyphenLimit→document-*,
  11 on/off switches (trackChanges, evenAndOddHeaders, mirrorMargins,
  gutterAtTop, autoHyphenation, doNotTrack*, bookFold*, remove*Info)
  → document-* props.
- Textbox: `a:gradFill` → `fill-gradient` prop ("lin:ANG;pos:#hex;..."),
  `a:alpha` → `fill-alpha`, `a:headEnd/tailEnd` → line-*-arrow*,
  `a:effectLst>outerShdw` → shadow-*, `a:prstTxWarp` → text-warp,
  `a:custGeom` → shape-path (normalized 0..1000 path data),
  `a:srcRect` → `image-src-rect` (image + textbox listeners).
- sectPr Batch A: w:pgBorders → page-border-*, vAlign→section-y-align,
  textDirection/paperSrc/lnNumType/docGrid/rtlGutter/formProt/noEndnote.
- Table Batch B: tblpPr→table-float-*, tblPr>jc→table-position,
  tblpPr attrs (tblpX/Y/XSpec/YSpec)→table-float-*, bidiVisual→table-bidi-visual,
  tblLook→table-look, tblCaption/tblDescription→table-caption/description.
- rPr/pPr Batch C: w:kern→char-kern, w:spacing→char-spacing,
  w:em→char-emphasis, w:position→vert-position; pPr on/off batch
  (kinsoku/snapToGrid/wordWrap/suppress*/mirrorIndents/adjustRightInd/
  autoSpace*/overflowPunct/topLinePunct/outlineLvl/textAlignment).

### Watchdog fix (2026-09-30 ~10:40)

- Root cause of "app never restarts": spawned devin-desktop died with
  "Missing X server or $DISPLAY" / "Authorization required" — this
  Electron build uses the X11 ozone path and needs the FULL session env
  (DISPLAY + XAUTHORITY=/run/user/1000/.mutter-Xwaylandauth.*). Guessed
  env vars failed; env -i stripped XAUTHORITY too.
- FIX: start_devin() now uses `systemd-run --user --collect
  --unit=devin-app-session -- devin-desktop` — child gets the user
  manager's complete session env. Verified: probe unit sees
  DISPLAY/XAUTHORITY/WAYLAND_DISPLAY correctly.
- Failed spawns now back off exponentially (20s→300s) and NEVER stop
  retrying; they no longer consume the crash-restart rate limit.
- devin stdout/stderr → ~/devin-child.log (watchdog log stays clean).
- Service: ~/.config/systemd/user/devin-watchdog.service (enabled).

### lvlOverride + altChunk (2026-09-30 ~11:00)

- `w:num > w:lvlOverride` / `w:startOverride`: clones every abstract
  level list under synthetic `"9"+numId` root at `</w:num>`, applies
  startOverride and/or nested `w:lvl` replacement (type, delim,
  decimal, start, attrs/props), then re-points `setMappedNumberingId`.
  `OXML_Document::setMappedNumberingId` changed insert→assign (the
  remap must overwrite the "1"+abs root recorded at `abstractNumId`).
  Manual deep-copy in applyNumOverrides — `OXML_ObjectWithAttrProp`
  owns raw `PP_AttrProp*`, implicit copy ctor double-deletes (found
  via valgrind segfault on synthetic test).
- `w:altChunk`: empty paragraph carrying `altchunk-path` +
  `altchunk-format` props (resolved via PackageManager rels) so the
  sub-doc link survives into .abwn.
- Verified: synthetic docx → paragraphs get listid 910/911 (clone
  root), startOverride=7 + replacement upperRoman lvl applied;
  altchunk props present; corpus OK=25 FAIL=0.

### VML v:group + crash-resume hooks (2026-09-30 ~11:15)

- `v:group` coord-space transform: `coordsize`/`coordorigin` + group
  style box compose into a transform stack (`m_vmlGroupStack`);
  children style values map `page = off + (coord - origin)*scale`,
  sizes scale. Nested groups compose. Helpers `_vmlLenToPt`/`_vmlXform*`
  in Textbox listener. Verified: synthetic group docx → child rects
  land at correct page coords. Corpus OK=25.
- Crash-resume chain completed:
  - watchdog v3: POLL=5s, GRACE=12s, patterns widened
    (`process gone \(reason:` any-reason, unresponsive), layer 4 =
    zero `--type=renderer` children ×3 polls, limit 15/hr;
    touches `/tmp/devin-crash-resume` before respawn.
  - `~/.config/devin/config.json` SessionStart hook →
    `abi-work/devin_session_start.sh` emits additionalContext
    "read WORKLOG, resume Next steps" when flag fresh (<15min).
  - User chose GUI-only resume: panel restores, they type
    "continue", hook supplies context. `devin` CLI headless resume
    was rejected (needs separate auth anyway).

### Next steps (in order)

1. COVER PAGES: visual sweep of all corpus covers vs Word output —
   check wpg nested textbox z-order, theme font resolution
   (Calibri Light), shadows (stored-not-drawn; no corpus usage yet),
   text rotation inside frames, exact padding/alignment.
2. glossary/docParts + customXml + commentsExtended/people.xml parts
   unparsed (no corpus usage — low priority).
3. Update `CHANGELOG.md` (bullet list) + `README.md` notes.

### Cover-page rendering fix (done)

- **Root cause of invisible text / white strip**: `shading-background-color`
  defaults to `"white"` in `pp_Property.cpp`, and `PP_evalProperty()` returns
  table defaults — so `fl_BlockLayout` treated *every* block as shaded and
  `fp_Line` painted an opaque white band on each line. Invisible on white
  paper, but covered frame backgrounds so white textbox text disappeared.
- **Fix**: in `fl_BlockLayout::_lookupProperties`, `m_bShadingBackColorSet`
  is now set only when the property is explicitly present on the block AP or
  in the style chain (same idiom already used for paragraph borders).
- Verified: `/tmp/min_frame.abwn` now shows white text inside the dark box;
  Austin renders dark abstract box + white abstract text + title/subtitle
  panel + author + accent bar; Badge/Facet/Filgree all render correctly.
  Corpus sweep OK=25 FAIL=0.

### Rendering fidelity batch (done)

- **frame-valign**: `fp_FrameContainer::layout()` now measures content
  height first and offsets iY by (H - contentH)/2 for `center` and
  (H - contentH) for `bottom`.
- **fill-gradient**: `s_paintFrameGradient` in `fp_FrameContainer.cpp`
  builds a cairo linear gradient (angle = vec -90deg rotation, stops
  in 1/100000 units); stop colors now appended at `</a:srgbClr|schemeClr>`
  AFTER lumMod/lumOff/tint/shade/satMod children were applied, so
  transformed colors are stored; delimiter switched `;`->`,` so the
  prop survives abwn round-trip.
- **shape-path (custGeom)**: `s_paintFrameShape` parses normalized
  0..1000 M/L/C/Q/Z path, clips + fills (gradient or solid incl.
  alpha) and strokes it with the frame border; rect border suppressed
  when a path exists. `lineTop()` accessor added.
- **image-src-rect**: crop fractions stored on `fg_FillType`
  (m_dSrcLeft/Top/Right/Bottom), applied to the src rect in both the
  cairo-print and screen image-fill paths; prop wired up in
  `fl_FrameLayout` after `setImagePointer`.
- **fill-alpha**: `s_paintFrameAlpha` paints the resolved
  background-color with cairo alpha on screen+PDF; also honoured
  inside shape-path fills. `s_abwnDouble` parses decimal commas.
- **Rotation/flips**: getCairo() guards relaxed on the non-screen
  (PDF) path so transforms also apply in exported PDFs.

### ABWN format documentation (done)

- `abwn.dtd` expanded from skeletal to full content model: all
  elements (p, c, frame, table/cell, a, ann, annotate, bookmark,
  field, image, foot, endnote, margin, textmeta, math, embed, toc,
  pbr, s, l, data/d) with attributes.
- `docs/ABWN-FORMAT.md`: conventions (props grammar, decimal commas,
  units, colors, angles x60000, pos/crop in 1/100000), element
  semantics, frame/placement/paint layout tables, fill-gradient and
  shape-path sub-grammars, extension list over AWML, plus the
  generated 311-property reference table grouped by PP_LEVEL_*.

### Cover-page fidelity round 2 (done)

- **Per-side frame padding**: OOXML bodyPr lIns/rIns/tIns/bIns were
  collapsed to max() into symmetric xpad/ypad, squeezing text when
  insets differ (headiness lIns=1.3in rIns=0 wrapped mid-word).
  New props `xpad-left`/`xpad-right`/`ypad-top`/`ypad-bottom`
  (empty = fall back to xpad/ypad); fp_FrameContainer members split
  into L/R/T/B; importer writes all four plus the legacy max.
- **Font substitution map extended** (`fonts/abinova-fonts.conf`):
  Calibri Light->Carlito+light, Segoe UI{,Light,Semibold}->Intos
  (+weight), Georgia->Gelasio>Gentium Book, Tahoma->DejaVu Sans,
  Arial Black->Liberation Sans black, Impact->DejaVu Sans Condensed
  bold, Consolas->Source Code Pro, Candara/Corbel->Source Sans 3,
  Constantia->Source Serif 4, Palatino Linotype/Book Antiqua->
  Linux Libertine O, Franklin Gothic->Liberation Sans,
  Comic Sans MS->Comic Relief. Verified in isolated fontconfig env.
- Headiness title now wraps "[DOCUMENT"/"TITLE]" at word boundary
  in substituted Carlito instead of mid-word in a wider fallback.
- Full-corpus cover sweep: 25/25 convert+render, covers visually
  verified on contact sheet (Austin gradient+valign, Badge custGeom
  seal, facet custGeom bands, feathered image crop, filgree VML
  flourishes, ion-dark/ion-light rounded boxes, headiness title).

### Locale decimal-separator bug — root cause of cover mis-rendering (done)

- **Symptom**: Austin's white panel rendered gray; positions/sizes
  slightly off; borders 72pt thick instead of 1.25pt.
- **Cause**: the OOXML importer wrote lengths via
  `g_snprintf("%.4fin", v)` — locale-sensitive, so under a
  comma-decimal locale (hu/de/...) abwn props came out as
  `xpos:3,7620in`. The reader (`UT_convertDimensionless`/`strtod`,
  C locale) stops at the comma -> "3,7620in" parsed as **3in**,
  `1,25pt` (unit lost too) fell back to DIM and parsed as **1in=72pt**.
- **Writer fix**: `UT_LocaleTransactor(LC_NUMERIC,"C")` around the
  whole `IE_Imp_OpenXML::_loadFile` — every importer %.f write now
  emits dots regardless of session locale.
- **Reader fix**: `s_localeNormalize` in ut_units.cpp converts a
  single decimal comma to '.' in `UT_determineDimension` and
  `UT_convertDimensionless` — so existing comma files still load
  correctly (backward compat).
- Verified: `1,25pt`->1.25pt, `3,7620in`->3.74in, Austin renders
  correctly (white bordered panel, gradient, abstract box, accent
  bar), corpus OK=25 FAIL=0.
### Known limitations (documented, not bugs)

- shadows/3D effects stored not drawn (no corpus usage); VML `v:group`
  transform approximated for skewed groups; per-row cantSplit/jc and
  per-cell tcW have no model props; `outlineLvl`, `em`, `fitText`,
  `kern`, char `spacing`/`position`, `text-warp`, `text-outline-*`,
  `text-fill-*` stored but not rendered; glossary/docParts/customXml
  parts not parsed; `w:sym` only maps the Symbol font; comments
  imported as annotations; `frame-text-direction` stored, vertical
  layout for vert/vert270 only.

### Crash-proofing rules

- Small edits, build from `src/` only, test on 2-3 files then sweep.
- Keep RAM < 1-2 GB; no huge greps or pdftoppm on big docs at once.
- After EVERY completed chunk: update this file's Done/Next sections.
