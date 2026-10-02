# Changelog

All notable changes in this experimental Abinova GTK4 fork, grouped by
category. Based on upstream Abiword 3.1.90 (`5e3e1cc` import).

This project is experimental and supplied **without any warranty or
responsibility** — see `README.md`.

## [4.0.0] — Upcoming (not yet released)

The next release of this fork will be versioned **4.0.0**. The items
below are on `main` but the release has not been cut yet.

### Built-in file-format support (formerly plugins)

- **DOCX (Office Open XML) moved into the core** — the `openxml`
  plugin sources moved to `src/wp/impexp/openxml/` and compile into
  `libimpexp`; `.docx`/`.dotx`/`.docm` open, save, save-as and edit are
  native functionality, no plugin `.so` required.
- **EPUB moved into the core** — the `epub` plugin sources moved to
  `src/wp/impexp/epub/`; EPUB 3.3 import/export and the export-options
  dialog are built in.
- **Grammar checking moved into the core** — the `grammar` plugin
  sources moved to `src/wp/ap/grammar/`; the checker registers an
  `AV_ListenerExtra` app listener at startup (fired by the existing
  `AV_CHG_BLOCKCHECK` background-check path, gated by the
  `AutoGrammarCheck` preference) instead of loading as a module.
- **ODT (OpenDocument) moved into the core** — `src/wp/impexp/odf/`;
  `.odt`/`.ott` open, save and save-as natively with ODF 1.4
  declarations (`office:version="1.4"`, `manifest:version="1.4"`).
- **Built-in flat-XML ODF import** (`.fodt`), including
  `office:binary-data` embedded images.
- **Built-in `.abwn` password encryption** — the native format gains
  an `ABWNCRP1` versioned binary envelope: PBKDF2-HMAC-SHA-256
  (600k iterations) derives a key for AES-256-GCM with the full
  header bound as AAD (`src/wp/impexp/xp/ut_abwncrypt.cpp`). The
  same "Encrypt with password" save-dialog UI as ODF, the same
  password prompt on open (three attempts), `ABINOVA_PASSWORD`
  headless fallback, and a decrypted document keeps its password so
  plain Ctrl+S re-encrypts. AES comes from the system `libcrypto`
  via dlopen — no new build dependency. Autosave backups of
  protected documents are encrypted as well.
- **Built-in ODF encryption both ways** — decrypt on open (GTK
  password dialog / `ABINOVA_PASSWORD` env var); encrypt on save via
  "Encrypt with password" in the ODF save dialog. PBKDF2-SHA1 +
  Blowfish CFB64 (vendored from OpenSSL 4.0.2, Apache-2.0), no
  libgcrypt dependency.
- **Built-in RDF metadata** — SAX-based `ODi_RDFParser` importer and
  `toRDFXML` serializer; `manifest.rdf` round-trips, no libredland.
- **Equation LaTeX source round-trips through ODF** — the ODF
  exporter now writes each equation's LaTeX source and
  `display:inline|block` mode as foreign-namespaced
  `abiword:latex-source`/`abiword:display` attributes on
  `<draw:object>` (self-declared `xmlns:abiword`); the importer
  restores them verbatim, so the original source survives
  `.abw`→`.odt`→`.abw` instead of being re-derived from MathML.
- **Reserved `.abw` schema sections** — `<changes>`
  (change-tracking metadata), `<masterpages>` (page-layout
  templates) and `<notes>` (presentation notes) are now part of the
  file format as placeholders for planned features: the importer
  stores each child element verbatim (name, attributes, text) and
  the exporter re-emits it, so the data survives a load/save round
  trip even before the features exist.
- **Built-in Markdown import/export** — `.md`/`.markdown`/`.mdown`/
  `.mkd`/`.mkdn`, CommonMark + Zettlr compendium: ATX/setext headings,
  emphasis/strong/strike/code, links, autolinks, images, nested
  bullet/ordered/task lists, blockquotes, fenced+indented code,
  horizontal rules, GFM pipe tables with alignment, hard breaks;
  documents round-trip through Markdown.
- **Markdown importer extended** — YAML frontmatter parses into
  document metadata (`dc.title`/`dc.creator`/`dc.date`/`dc.subject`/
  `abiword.keywords`); reference links/images resolve from `[id]: url`
  definitions; `[^id]` footnotes become real Abinova footnote objects;
  inline `$…$` and fenced `$$…$$`/`math` blocks import as styled math
  text; `<!-- -->` comments are dropped; `:emoji:` shortcodes convert
  to Unicode.
- **Mermaid diagrams render inline** — fenced `mermaid` blocks are
  drawn to a PNG with a built-in Cairo renderer
  (`src/wp/impexp/xp/ut_mermaid.cpp`) and embedded as images: flowcharts,
  sequence diagrams, Gantt charts, class diagrams and pie charts are
  supported (subgraph/cluster labels, dashed/dotted edges, message
  arrows, inheritance, legend percentages). Unsupported diagram types
  keep their source as code text.
- **Markdown formatting test document** — `test/wp/markdown-formatting.md`
  exercises every supported construct (frontmatter, reference links,
  footnotes, tables, task lists, math, comments, emoji) and was used
  to verify the importer coverage above.
- **Built-in LaTeX import/export** — `.tex`/`.latex`/`.ltx`: the old
  `latex` plugin exporter moved to `src/wp/impexp/xp/ie_exp_LaTeX.cpp`
  (registered centrally, no module load), and a new importer
  (`ie_imp_LaTeX.cpp`) covers the common document subset —
  preamble, `\maketitle`, sectioning, `\text*`/`{\bf ...}`
  formatting, itemize/enumerate/description lists (with nesting),
  quote/verse, verbatim/lstlisting, center/flushleft/flushright,
  tabular/array/longtable tables, `\includegraphics`, `\footnote`
  (real footnote objects), inline and display math (styled text),
  comments, escapes, accents, ligatures, `\hrule` and page breaks.
- **Self-contained MHTML importer** — the `mht` plugin was rewritten
  around an internal `UT_MHTStream` MIME parser (folded headers,
  multipart boundary, quoted-printable/base64 parts, `cid:` images);
  `.mht`/`.mhtm`/`.mhtml` plus `application/x-mimearchive` and
  `message/rfc822` registered.
- **WordPerfect moved into the core** — the `wordperfect` plugin
  sources moved to `src/wp/impexp/wordperfect/`; `.wpd`/`.wp` and
  MS Works (`.wps`) import via the vendored `libwpd`/`libwps`/
  `librevenge` convenience libraries, no plugin `.so`.
- **WPG graphics moved into the core** — the `wpg` plugin sources
  moved to `src/wp/impexp/wpg/`; `.wpg` images import through the
  vendored `libwpg` renderer as core functionality.
- **MHT moved into the core** — the `mht` plugin sources moved to
  `src/wp/impexp/mht/`; optional libtidy HTML cleanup behind
  `-DXHTML_HTML_TIDY_SUPPORTED`, with the libxml2 HTML parser as the
  always-available fallback (`-DXHTML_HTML_XML2_SUPPORTED`); the
  importer's `s_strnstr` off-by-one on the final match position was
  fixed along the way.
- **WMF moved into the core** — the `wmf` plugin source moved to
  `src/wp/impexp/wmf/` behind a `HAVE_LIBWMF` configure check
  (`libwmf-config` ≥ 0.2.8); `.wmf`/`.apm` images import via
  system libwmf when present.
- **`rsvg` plugin removed** — redundant: the core SVG importer and
  the GdkPixbuf loader already cover `.svg` natively.
- **No loadable plugins remain** — every importer/exporter is
  registered centrally in `ie_impexp_Register.cpp`; the plugin
  configure list is empty.
- **abiword.keys** registers ODF, DOCX and EPUB mimetypes.

### MS Word compatibility

- **DOCX document properties round-trip** — `docProps/core.xml` and
  `docProps/app.xml` are parsed on import and written on export
  (dc/cp/dcterms namespaces, typed `W3CDTF` dates), plus their
  `[Content_Types].xml` overrides and `_rels/.rels` relationships.
- **New document metadata keys** — `category`, `last_modified_by`,
  `revision`, `last_printed`, `company`, `manager`, `template`,
  `editing_duration`, `content_status`.
- **DOC import metadata map fixed** — category, last-saved-by,
  revision, last-printed, template and editing duration now imported;
  Manager/Company stored under their own keys (was contributor/
  publisher).
- **Document Properties dialog extended** — editable Last saved by /
  Manager / Company / Template / Status fields and a read-only
  Statistics tab (created/modified/printed dates, revision, total
  editing time, live page/paragraph/line/word/character counts).
- **Category property fixed** — stored under its own key (was
  `dc.type`, which never round-tripped to `cp:category`); legacy
  `dc.type` values still read as fallback.
- **DOCX `mc:AlternateContent` handled per spec** — the `mc:Choice`
  branch is consumed and the whole `mc:Fallback` subtree suppressed;
  Word 2010+/LibreOffice drawings and textboxes no longer duplicate.
- **DOCX builtin style names mapped** — lowercase `w:name` values
  (`heading 1`, `list bullet`, …) resolve to Abinova builtins instead
  of duplicate custom styles.
- **DOCX headers/footers** — tables, images, math and textboxes inside
  header/footer parts now import (was Common+Field states only);
  math listener unwinds cleanly on conversion failure.
- **Vendored `wv-1.2.9`** for legacy `.doc`, patched for the buffer
  overflows Debian reported in libwv-1.2 (bounded sprm walks,
  page-bound memcpys, FKP record clamps, `vsnprintf`); verified with a
  600-case byte-mutation ASan fuzz run.
- **Legacy `.doc` headers/footers inherited across sections fixed** —
  a section that shares its previous section's header/footer (empty
  `Plcfhdd` story) now renders the inherited content instead of
  swallowing the source section's text; the copied header/footer no
  longer gets a duplicate strux that ate the following story's text.
- **Legacy `.doc` custom footnote/endnote marks** — manual reference
  marks no longer leak `footnote-id`/`endnote-id` attributes onto the
  following body text.
- **Legacy `.doc` fields, hyperlinks and bookmarks hardened** — stray
  field separator/end characters can no longer corrupt the field
  stack, oversized field instructions are truncated instead of
  overflowing, and `HYPERLINK` fields resolve their target from the
  instruction (`\l` bookmark links included) or from the document's
  `_PID_HLINKS` hyperlink table (MS-DOC 2.4.7) instead of emitting
  empty links; bookmarks anchored inside skipped field content are no
  longer dropped.
- **Legacy `.doc` comments/annotations imported** — Word comments
  (`PlcfandRef`/`PlcfandTxt` + `ATRD` records per MS-DOC 2.3.4) now
  arrive as real Abinova annotations: the anchored text range is
  resolved through the comment's `SttbfAtnBkmk` bookmark, comment
  bodies land in the annotation shadow, and author (from
  `GrpXstAtnOwners`), initials and date (Word 2000+ `AtrdExtra`
  DTTM) are preserved. Point comments and nested/overlapping
  comment ranges are handled.
- **Legacy `.doc` text boxes and anchored shapes imported** — Word 97+
  drawing objects (`FSPA` anchors + Escher/`OfficeArt` shape records)
  now arrive as positioned Abinova frames instead of being dropped:
  text-box story text (`PlcftxbxTxt`/`FTXBXS`) is routed into the
  frame matching its shape `spid`, FSPA geometry maps to
  margin/page/paragraph-relative frame positions and Word wrap modes
  (square, top/bottom, behind/in-front, tight), and shape properties
  (fill color/opacity, line color/width/dash, rotation, flips, text
  insets, vertical alignment, vertical text flow) are applied.
  Word-style defaults are honoured (white fill + 0.75 pt border on
  text boxes, no border on floating images); deleted and
  background-part shapes are skipped.
- **Legacy `.doc` sections/page setup audited against MS-DOC 2.6.6** —
  the SED/SEPX sprm table in vendored wv now covers the Word 2000+
  opcodes (`sprmSBrcTop/Left/Bottom/Right` BrcOperand page borders,
  `sprmSPgnStart`, `sprmSWall`, `sprmSRsid`, footnote/endnote
  numbering sprms) and decodes `sprmSDxaColWidth`/`ColSpacing`,
  `sprmSClm`, `sprmSFRTLGutter` and `sprmSPgbProp` bitfields;
  long-standing misparse bugs fixed (`sprmSDmBinOther` overwrote
  `dmBinFirst`, `sprmSVjc` overwrote `fLBetween`, `sprmSBOrientation`
  was inverted so landscape sections imported as portrait).
  The importer now emits per-section `section-y-align`,
  `section-text-direction`, `section-doc-grid*` (document grid),
  `section-ln-*` (line numbering), `section-paper-src-*`,
  `section-endnote-suppress`, `section-rtl-gutter`,
  `section-form-protected` and `page-border-*` properties matching
  the OOXML importer's vocabulary; `section-restart-value` is only
  emitted when page-number restart is enabled, and a "new column"
  break in a column-less section degrades to a page break like Word.
  Note: Abinova's layout model has a single page size per document,
  so per-section page sizes collapse to the first section's.
- **Password-protected legacy `.doc` files open** — the importer now
  detects `fEncrypted` in the FIB (the WordDocument stream's 68-byte
  cleartext prefix is parsed, the rest left as ciphertext until a
  password is supplied) and decrypts both Word 97+ schemes:
  XOR obfuscation (`fObfuscated`, MS-OFFCRYPTO 2.3.7) and the
  RC4 `EncryptionVersionInfo` 1.1 scheme with per-512-byte-block
  rekeying; the RC4 CryptoAPI/AES variants are rejected cleanly.
  Word 95 and earlier protected files, and unsupported cipher
  variants, report "The file is password-protected" instead of
  importing garbage.  Headless conversions take the password from
  `ABINOVA_PASSWORD` like the ODF/.abwn paths.  (Also fixes the
  vendored MD5 `UINT4` typedef being 64 bits on LP64, which had
  silently broken the RC4 password verifier.)
- **Legacy `.doc` importer memory-safety pass** — fixed a class of
  heap-corruption bugs in vendored wv where the `wvGet*_PLCF` table
  readers (`atrd`, `bte`, `pcd`, `fld`, `lst`, `ftxbxs`, `bkd`, `fdoa`,
  `frd`, `ffn`, `fspa`) freed the caller's stack pointer instead of the
  positions array on allocation failure (`wvFree(pos)` → `wvFree(*pos)`).
  The PAPX/CHPX formatted-disk-page caches now deep-copy their arrays
  (previously the cache aliased the caller's buffers, forcing
  `wvReleasePAPX_FKP`/`wvReleaseCHPX_FKP` into no-op stubs — every FKP
  page read leaked); both release functions now actually free, and the
  complex-decode path finally releases the bookmark string table
  (`Sttbfbkmk`).  The importer no longer strands a loaded footnote/
  endnote PLCF when its zero-length sibling loads as NULL, and a
  missing reference table leaves no stale note count.
- **Legacy `.doc` importer uninitialized-read fixes** — vendored wv
  now fully initializes the OfficeArt drawing structs: a `.doc` whose
  escher stream lacks a `DgContainer`/`FSP` record no longer leaves
  shape counts, pointers and geometry read from uninitialized memory
  (bogus frees / garbage anchors).  `wvStream_read` also zero-fills
  the unread tail on short reads at end-of-stream on both the GSF and
  `FILE*` paths instead of leaving caller buffers uninitialized —
  matching the memory-stream behaviour.
- **Legacy `.doc` importer arithmetic hardened** — a signed-overflow /
  shift audit of the vendored wv parser closed off remaining
  undefined behaviour on corrupt length and offset fields: BMP
  `biClrUsed` assembly no longer shifts a byte into the sign bit and
  corrupt palette sizes are clamped to `1<<bpp`, `sprmCHpsMul`
  font-size scaling uses 32-bit unsigned math instead of overflowing
  `int`, `sprmPChgTabs`/`sprmPChgTabsPapx` tab-stop merges can no
  longer write past the 64-entry `rgdxaTab`/`rgtbd` arrays, the
  `cch == 255` long-operand form of `sprmPChgTabs` now returns its
  real (int) length instead of a wrapped `U8`, grpprl sprm walkers
  in the complex-decode path (SEP/PAP/CHP and `sprmPHugePapx`)
  validate each operand against the declared grpprl length before
  applying it, TDefTable tail-eaters stop at the operand end instead
  of looping forever on overshot positions, FILETIME-to-`time_t`
  conversion computes in unsigned 64-bit so extreme dates can't
  shift into the sign bit, `PlcfandRef` rejects `lcb < 4`, and piece-
  table CP deltas clamp instead of wrapping on non-monotonic tables.
- **Legacy `.doc` importer out-of-bounds access fixed** — an index/
  length audit of the vendored wv parser closed the remaining
  file-controlled overruns: PAPX/CHPX FKP pages no longer read BX+PHE
  records past the 512-byte page when `crun` leaves a truncated tail,
  `sprmPIstdPermute`/`sprmCIstdPermute` bound the `rgistd` index to the
  styles actually supplied (and no longer dereference a NULL table),
  corrupt ANLD `cxchTextBefore/After` lengths and `nLvlAnm` levels are
  clamped to the real `rgxch`/`lvl[]` capacities, password handling
  reserves room for the terminator instead of writing past
  `password[15]` and no longer loops on failed multibyte conversion,
  `expandpw` bounds its scan, `FFN`/`ATRD`/`xstDispFldRMark` strings
  are force-terminated so `wvWideStrToMB` can't scan off fixed arrays,
  field instruction/argument writes stop at the 40000-entry arrays,
  `msofbtClientTextbox` records shorter than a `U32` no longer
  under-allocate, and font-table indexing validates `ftc`/`ftcSym`
  against `nostrings`.  The RTF, HTML and GdkPixbuf importers no
  longer index `len-1` on empty strings, and an overlapping `strcpy`
  in the GTK dialog helper became `memmove`.  Verified with an ASan
  harness replaying each malformed-input path (all reports on the old
  code, clean after the fix) plus PDF conversions of the `.doc` test
  corpus.
- **Container-invalidation audit (iterators/references across
  mutation)** — swept erase-while-iterate loops, element references
  held across `push_back`/`erase`, and stale index/iterator reuse
  across `UT_Vector`, `std::vector`, maps and stacks throughout the
  importers, layout engine and piece table.  Fixed the one real bug
  found: the "previous reference to semantic item" command
  (`rdfAnchorSelectPrevReferenceToSemanticItem`) incremented the
  `begin()`/`end()` iterator of an empty `std::set` after a resync —
  undefined behaviour and a potential crash; it now returns early when
  the set is empty.
- **Expression-sequencing audit** — swept the tree for unsequenced
  read+modify of the same object (`x = x++`, `f(i++, i)`,
  `*p++ = *p` forms), side effects inside function-call arguments that
  depend on evaluation order, and side effects hidden inside
  assert/debug macros that vanish in release builds.  No undefined
  behaviour found; the one fragile construct — the tab-type cycler in
  the top ruler assigning and pre-incrementing the same variable in a
  single ternary — was rewritten as explicit branches.
- **Legacy `.doc` exporter removed** (`ie_exp_MsWord_97` was dead code);
  DOC export continues via the RTF-as-DOC hack sniffer.
- **Column balancing for short multi-column sections** — the last
  column row of a section now redistributes its content evenly across
  the configured columns instead of letting the first column fill to
  the full page height.  This matches Word/LibreOffice "continuous"
  section behaviour, so e.g. a two-column header block renders its
  left/right content side by side.  Balancing is skipped when a
  forced column or page break is present, and multi-page sections
  only rebalance their final row.
- **DOCX pagination fidelity** — fixed a chain of importer bugs that
  made a real-world CV occupy ~3 pages instead of Word's 2:
  - `w:docDefaults` no longer hijacks `Normal`: the document's real
    `Normal` imports as `_Normal` and unstyled paragraphs resolve to
    it, so stray docDefault spacing (`w:after`/`w:line`/`w:sz`) stops
    inflating every paragraph.
  - Theme fonts resolve correctly: `w:themeFontLang` maps ranges to
    languages (`en-US` → `Latn`), but theme `<a:font>` entries are
    keyed by ISO-15924 script and the Latin face lives under
    `latin` — `OXML_FontManager` now falls back to the range's
    default script, so `asciiTheme="minorHAnsi"` yields Calibri
    instead of silently degrading to Times New Roman.
  - `w:rFonts` Latin resolution only considers `ascii`/`asciiTheme`/
    `hAnsi`/`hAnsiTheme`; a style setting only `w:eastAsia`/`w:cs`
    (e.g. Verdana for CJK) no longer leaks that face onto Latin text.
  - `w:contextualSpacing` is honoured via a new
    `contextual-spacing` block property — consecutive same-style
    paragraphs (e.g. list items) collapse their inter-paragraph
    margins.
  - `w:pgMar` applies to the section whose `w:sectPr` carries it,
    instead of globally overwriting every section with the last
    sectPr's margins.
  - Paragraph-mark run properties (`w:pPr/w:rPr`) only contribute
    `w:sz` (empty-paragraph height) — `w:highlight`/`w:shd` no longer
    paint the whole paragraph.
  - Paragraphs carrying a `w:sectPr` break mark get a
    `section-break` paragraph property in `.abw`; layout suppresses
    their borders, matching Word's paragraph-border merging.
- **DOCX schema coverage broadened** — `w:lvlOverride`/`startOverride`
  (per-numbering-instance list clones), `w:altChunk` (references kept
  as `altchunk-path`/`altchunk-format` links), `w:comment*` imported
  as real annotations (comment range anchors + comment bodies with
  author/date/initials), `v:group` coordinate-space transforms for
  VML groups, `a:arrowhead`, `a:effectRef`, `a:prstTxWarp`, `a:srcRect`
  and `a:gradFill` captured into frame properties.
- **DOCX positioned objects rendered properly** —
  - `behindDoc` anchors no longer lose `wrap-mode:below-text` when a
    `wp:wrap*` child follows, so background shapes paint behind text.
  - `frame-valign` (top/center/bottom) implemented in
    `fp_FrameContainer::layout()`.
  - `fill-gradient` renders multi-stop linear gradients; gradient
    stop colors are serialized after `lumMod`/`lumOff`/`tint`/`shade`/
    `satMod` transforms and round-trip through `.abwn`.
  - `shape-path` (`a:custGeom`) is drawn as a real cairo path
    (M/L/C/Q/Z over a 1000x1000 box) instead of a bounding rectangle.
  - `image-src-rect` crops picture frames to the declared source
    rectangle.
  - `fill-alpha` applies fill transparency on screen and PDF.
  - Frame rotation/flip transforms now also apply on the PDF/export
    path, not just on screen.
  - Text-box insets map to per-side `xpad-left`/`xpad-right`/
    `ypad-top`/`ypad-bottom` frame properties instead of collapsing
    to `max()` — asymmetric `bodyPr` insets no longer squeeze text.
  - `a:headEnd`/`a:tailEnd` line-end decorations now paint on
    `prstGeom="line"` shapes (imported as `bar-w`/`bar-h` bars) —
    triangle, stealth, diamond, oval and open arrow heads sized
    sm/med/lg relative to the stroke width, colored with the line.
  - Vertical line shapes no longer collapse into invisible slivers:
    the importer never captured a top-level shape's `a:xfrm` extent
    (only group children were read), so `prstGeom="line"` bars were
    always treated as horizontal.  The Headline cover page's accent
    bar renders again.
  - `a:outerShdw` drop shadows now paint — the shape silhouette
    (custom `shape-path` when present, else the frame box) is
    rasterized, box-blurred by `blurRad` and masked in the shadow
    color at the `dist`/`dir` offset; `rotWithShape="0"` keeps the
    offset fixed in page space.  Shadow color and `a:alpha` opacity
    are imported too (`frame-shadow-*` properties, round-tripping
    through `.abwn`).
  - `wps:style` theme style references resolve per ECMA-376:
    `a:lnRef` supplies a default outline from the theme's
    `a:lnStyleLst` (width, dash and the ref's color child in place of
    `phClr`), `a:effectRef` applies the referenced `a:effectStyleLst`
    drop shadow, and `a:fontRef` provides the default theme font and
    text color for shape runs that set neither directly nor through
    their paragraph style.  Explicit `a:ln`/`a:effectLst` in `spPr`
    (even empty ones) still take precedence.
  - Vertical text boxes render — `wps:bodyPr@vert` imports as
    `frame-text-direction`; `vert`/`eaVert`/`mongolianVert` run lines
    top-to-bottom stacking right-to-left and `vert270` runs them
    bottom-to-top stacking left-to-right.  Text is laid out in a
    swapped logical space (line width is the box height) and rotated
    into the box with a cairo transform at paint time, so wrapping,
    alignment, `frame-valign` anchoring and `spAutoFit` growth all
    follow the rotated axes.  `wordArtVert*` modes approximate with
    the same 90° rotation (upright stacked glyphs are not yet
    supported).  Round-trips through `.abwn`.
  - `a:ln` outline details render — `a:ln@cmpd` compound strokes
    (`dbl`/`thickThin`/`thinThick`/`tri`) paint as parallel strands,
    the join children (`a:round`/`a:bevel`/`a:miter` with `@lim`),
    `a:ln@cap` end caps and `a:custDash`/`a:ds` custom dash patterns
    map onto the cairo stroke, `a:ln/a:gradFill` paints the outline
    as a linear gradient, and `a:ln@algn="in"` draws the stroke fully
    inside the shape edge.  Applies to uniform box borders (stroked
    as one closed rect so joins show), non-uniform edges, freeform
    `custGeom` paths (whose stroke previously never painted) and
    `prstGeom="line"` bars.  Frame borders also gained the
    double/triple/wave styles `fp_ContainerObject` already had.
  - `a:blipFill` image fills now honor `a:tile` — the blip repeats
    across the shape at its natural size scaled by `sx`/`sy`, with
    the tile grid anchored by `algn`, shifted by `tx`/`ty` and
    mirroring alternate tiles per `flip` — and `a:stretch` now
    respects its `a:fillRect` destination subrectangle (negative
    insets expand past the box and clip at the shape).  `a:srcRect`
    crops apply once and the cropped remainder re-stretches into the
    tile cell or fill rect on screen and PDF, where the generic
    image fill used to blit it unscaled or drop it for positioned
    frames.  Imported as the `image-tile`/`image-fill-rect`
    frame/image properties.
- **DOCX letter-spacing rendered** — `w:spacing` inside `w:rPr`
  (expanded/condensed character pitch in twentieths of a point) now
  imports as the `char-spacing` character property and renders by
  widening each glyph cluster's advance in the Pango shaper, so
  tracked-out titles (e.g. cover pages at 8 pt) match Word.  The
  importer previously dropped it: the element name is shared with
  paragraph spacing (`w:pPr/w:spacing`) whose handler consumed it
  unconditionally.  Round-trips through `.abwn`.
- **DOCX picture effects rendered** — `a:blip` children
  `a:duotone` (two-color luminance remap), `a:grayscl`, `a:lum`
  (brightness/contrast) and `a:alphaModFix` are imported as
  `image-duotone`/`image-grayscale`/`image-lum`/`image-alpha-mod`
  properties and applied to the raster pixels when the image is
  generated, covering both inline pictures and positioned picture
  fills.  Duotone colors resolve `a:schemeClr` theme slots,
  `a:srgbClr`, `a:prstClr`, `a:sysClr`, `a:scrgbClr` and `a:hslClr`,
  with the full DrawingML transform set (`shade`/`tint`/`lumMod`/
  `lumOff`/`satMod`/`satOff`/`hueMod`/`hueOff`) applied in HSL space
  — e.g. the Filigree cover's flowers render tinted toward the
  `accent1` theme color instead of staying flat gray.
- **DOCX text-box autofit shrink rendered** — `a:normAutofit` inside
  `wps:bodyPr` now imports `fontScale`/`lnSpcReduction` as
  `frame-font-scale`/`frame-linesp-reduction` frame properties.
  Runs inside such a frame render at the scaled size (the resolved
  size is pinned on a cloned attribute set, so sizes inherited from
  styles scale too) and the line spacing reduction squeezes each
  line box, matching the shrink Word already computed and stored.
  Round-trips through `.abwn`.
- **`.abwn` round-trip verified for all new drawing properties** —
  the DrawingML properties added above (`char-spacing`,
  `frame-shadow-*`, `image-*` effects/tiling, `frame-font-scale`,
  `frame-text-direction`, `line-*`/`outline-gradient` stroke extras)
  serialize and re-import loss-free; a `docx → .abwn → .abwn`
  property diff over the whole cover-page corpus plus the synthetic
  feature docs shows zero dropped properties.  The `a:ln` extras
  (`line-compound`, `line-join`, `line-miter-limit`, `line-cap`,
  `line-align`, `line-dash`, `line-custom-dash`, `outline-gradient`)
  were missing from the property registry — now registered in
  `pp_Property.cpp`, documented in `docs/ABWN-FORMAT.md` (new §4.4,
  refreshed §6 defaults table) and listed in `abwn.dtd`.
- **Built-in cover pages regenerated and re-verified** — all 17
  cover templates (`src/wp/covers/*.xml`) were rebuilt from fresh
  imports of the reference `.docx` designs, so they now carry the
  `char-spacing` tracking, theme-resolved fonts and verbatim image
  bytes the older exports lacked (e.g. Badge's tracked-out title,
  Integral's full-quality JPEG), and each was rendered against its
  source design for a pixel-level comparison.  Inserting a template
  cover while the caret sat inside an old frame could scramble the
  splice because frame-edit mode hijacked the view point; the
  template path now probes the piece table and builds the sentinel
  and trailing shell paragraph at document level instead.
- **Locale decimal-separator corruption fixed** — the OOXML
  importer serialized lengths with locale-sensitive `snprintf`
  (`xpos:3,7620in` under comma-decimal locales) while the abwn
  reader parses C-locale, so positions truncated to whole inches and
  `1,25pt` became a 72pt border.  Import now pins
  `LC_NUMERIC` to C for the whole load, and the unit parser accepts
  a single decimal comma for backward compatibility with existing
  files.
- **Font substitution extended** (`fonts/abinova-fonts.conf`) —
  Calibri Light, Segoe UI/Light/Semibold, Georgia (Gelasio),
  Tahoma, Arial Black, Impact, Consolas, Candara, Corbel,
  Constantia, Palatino Linotype/Book Antiqua, Franklin Gothic and
  Comic Sans MS now resolve to bundled metric-compatible or
  stylistically closest fonts instead of arbitrary fallbacks.
- **White strip over dark textboxes fixed** — every paragraph was
  treated as shaded because `shading-background-color` has a registry
  default of `white` and `PP_evalProperty()` returns table defaults;
  `fl_BlockLayout` now marks shading set only when the property is
  explicitly present, so lines stop painting opaque white bands over
  frame backgrounds.
- **ABWN format documented** — `abwn.dtd` expanded to the full
  element/attribute content model and `docs/ABWN-FORMAT.md` added:
  serialization conventions (props grammar, units, decimal commas,
  colors, OOXML angle/position units), element semantics, frame
  placement/paint/layout properties, `fill-gradient` and
  `shape-path` sub-grammars, AWML extension list, and the complete
  generated property reference.
- **`.doc` piece-table decoding hardened** — the bundled `wv` text
  retriever no longer reads out of bounds on corrupt files: the CLX
  `grpprl`/`PlcfPcd` block lengths are clamped to the declared CLX
  size, `wvConvertCPToFC` handles an empty piece table, out-of-range
  piece and `igrpprl` indices are rejected instead of dereferenced,
  empty `PlcBte` bin tables report failure instead of reading
  `bte[-1]`, and a failed iconv codepage conversion now yields a
  deterministic `?` instead of uninitialized memory. Verified with
  synthetic fast-saved-style documents mixing compressed
  (8-bit ANSI) and uncompressed UTF-16 pieces, including a
  CP1251 (Russian-lid) document — all pieces import in order with
  the correct characters.
- **`.doc` FIB validation and hardening** — the bundled `wv` parser
  now validates the File Information Block per MS-DOC 2.5 before
  trusting it: truncated or non-Word FIBs (`wIdent`, `csw`, `clw`,
  `cfclcb`, `fcMin > fcMac`) are rejected cleanly, every `fc`/`lcb`
  pair of `FibRgFcLcb` is clamped to the actual table-stream size so
  corrupt offsets can no longer drive wild seeks or giant
  allocations, memory-stream seeks/reads/writes are bounds-checked,
  and the `wvGetFIB*` readers report failure to their callers.
  Importer and parser crash sites found by fuzzing are fixed:
  annotation-owner strings (`wvGetGrpXst`), missing style slots
  (`grupe` NULL), bookmark `bkf`/`bkl` mismatches and out-of-range
  `ibkf` indices, and footnote/endnote/header counts derived from
  `lcb` values that could underflow. A 200-case FIB fuzz battery
  (mutated fields, truncations, garbage blobs) now yields only clean
  conversions or clean rejections — no crashes.
- **`.doc` paragraph formatting audit** — the bundled `wv` parser now
  understands the full MS-DOC paragraph-sprm table, including the
  Word 2000+ opcodes: full-COLORREF paragraph borders and shading
  (`sprmPBrc*`/`sprmPShd` BrcOperand/SHDOperand, alongside the legacy
  `*80` indexed-color forms), character-unit and line-unit indents and
  spacing (`sprmPDxc*`/`sprmPDyl*`, resolved against the document's
  Normal font), `sprmPDtap`/`sprmPIpgp`/`sprmPItap` table depth
  bookkeeping, contextual spacing, mirrored indents and the East-Asian
  typography toggles. Paragraph borders now import as real block
  borders (style, color, thickness, spacing, shadow) and paragraph
  shading as real block shading instead of text highlight colors;
  exact and at-least line heights, tab-stop leader characters and
  Word's extended justification values are mapped too. A latent bug
  that made every tab stop in `sprmPChgTabs`/`sprmPChgTabsPapx`
  decode with the first stop's type and leader is fixed. Verified
  with a synthetic document driving both PAPX decode paths — tabs
  with leaders, all four paragraph borders, spacing and shading
  import with the correct properties, and the `.doc` corpus still
  converts cleanly.
- **`.doc` character formatting audit** — the bundled `wv` parser now
  understands the full MS-DOC character-sprm table, including the
  Word 2000+ opcodes: full-COLORREF text color (`sprmCCv`), character
  shading (`sprmCShd` SHDOperand) and border (`sprmCBrc` BrcOperand)
  alongside the legacy `*80` indexed forms, modern language IDs
  (`sprmCRgLid0/1`), underline color (`sprmCCvUl`), `sprmCFNoProof`,
  `sprmCFWebHidden`, `sprmCFSpecVanish`/`sprmCFSdtVanish` style-
  separator visibility, `sprmCFitText`, `sprmCFELayout`,
  `sprmCCharScale`, revision-mark and East-Asian operands — all with
  spec-correct operand lengths so grpprls no longer desynchronise.
  Previously truncated operands (`sprmCPlain`, `sprmCIcoBi`,
  `sprmCHpsInc`, `sprmCFDiacColor`, `sprmCPropRMark90`,
  `sprmCDispFldRMark`, `sprmCHpsNew50`) now consume their full operand.
  Imported documents map these to character properties: COLORREF text
  color and character shading, double-strikethrough, raised/lowered
  runs (`sprmCHpsPos`), letter spacing (`sprmCDxaSpace` →
  `char-spacing`), kerning threshold (`sprmCHpsKern` → `char-kern`),
  horizontal scaling (`sprmCCharScale` → `char-width`), all-caps and
  small-caps (`text-transform`/`font-variant`), East-Asian emphasis
  marks (`sprmCKcd` → `char-emphasis`) and `sprmCFtcDefault` style font
  restoration. Verified with a synthetic document exercising both
  CHPX decode paths — all properties import and render correctly, and
  the `.doc` corpus still converts cleanly.
- **`.doc` list/numbering audit** — legacy Word list handling is now
  complete per MS-DOC: `LVLF` fields follow the spec layout
  (`fIndentSav`/`fConverted`/`fTentative`/`dxaIndentSav`/
  `ilvlRestartLim`/`grfhic`), `PlcfLst`/`PlfLfo` reads are bounded to
  their declared regions, and list resolution (`wvAssembleListPAP`)
  honours `LFOLVL` start-at and whole-`LVL` formatting overrides,
  clamps corrupt `ilfo`/`ilvl` indexes, applies `LSTF.rgistd`
  level-to-style links (a level linked to "Heading 1" now produces
  Heading-1 paragraphs), and re-resolves when complex-decode grpprls
  move a paragraph between lists. The importer now splits multi-level
  number text into `list-delim`/`list-decimal` so "1.a.i." labels
  compose correctly, maps bullet glyphs (including Symbol/Wingdings
  private-use codepoints) onto the matching Abi bullet type, takes
  the label font from the number's own character properties, and keys
  list instances by a collision-free composite (lsid+ilfo+level+
  format+restart) — fixed a real collision where a lower-roman level
  could merge into a lower-letter list. Verified with a synthetic
  multilevel `.doc`: numbered 1./1.a./1.a.i. levels, an embedded
  square-bullet level, a `•` bullet list and an LFOLVL restart at 5
  all import and render correctly on both decode paths, and the
  `.doc` corpus still converts cleanly.
- **`.doc` table import rework** — legacy Word tables now import
  structurally correct per MS-DOC 2.4.3-2.4.5: the bundled `wv`
  parser tracks table state per nesting depth (`itap`), detects
  inner-table row/cell marks on both decode paths, and decodes the
  spec layouts for vertical-merge, cell-TCGRF, padding, spacing,
  width and table-positioning sprms (including the `ftsDxa` and
  TVertMerge operand fixes). The importer keeps a per-depth table
  context so nested tables open inside their parent cell, vertical
  and horizontal merges produce proper row/column spans with
  merge-covered cells emitted as nothing, and cell borders, shading
  (indexed and full-color), margins, vertical alignment, row heights
  (exact/at-least), header rows and floating-table position all map
  to table/cell properties. Tables at document start and inside
  frames no longer drop their struxes, and merge-covered cells no
  longer leak orphan paragraphs into the table.

### Keyboard shortcuts (Word-compatible default map)

- **Default keymap aligned to MS Word** — the stock `default`
  binding set (`src/wp/ap/xp/ap_LB_Default.cpp`) now mirrors the
  Word shortcut table; shortcuts with no equivalent function are
  intentionally left unbound. The map is covered by a new unit test
  (`src/wp/ap/xp/t/ap_KeyBindings.t.cpp`) that resolves every
  documented shortcut against the live `EV_EditBindingMap`.
- **File** — Ctrl+Shift+S / F12 Save As, Shift+F12 Save,
  Ctrl+F12 Open, Ctrl+Shift+F12 Print, Ctrl+F2 print preview.
- **Editing** — Ctrl+Alt+V Paste Special, Ctrl+Shift+V paste
  formatting (format painter), F5 Go To.
- **Formatting** — Ctrl+Shift+X strikethrough, Ctrl+= subscript,
  Ctrl+Shift+= superscript, Ctrl+D font dialog.
- **Paragraph** — Ctrl+L align left, Ctrl+M / Ctrl+Shift+M
  indent/un-indent, Ctrl+0 toggle space-before-paragraph (new
  `toggleParaBefore` method), Ctrl+Q clear direct paragraph
  formatting (new `FV_View::resetBlockFormat()` — clears the block
  `props` attribute while keeping the paragraph style),
  Ctrl+Shift+N Normal style (new `setStyleNormal` method).
- **Insertions** — new char-insert edit methods for Word's symbol
  keys: Ctrl+- optional hyphen (U+00AD), Ctrl+Shift+-
  non-breaking hyphen (U+2011), Ctrl+Alt+- em dash,
  Ctrl+Alt+Shift+- en dash, Ctrl+Alt+C/R/T ©/®/™;
  Alt+Shift+P page numbers, Alt+Shift+D date & time,
  Alt+Shift+U column-sum field, Alt+Shift+X mark index entry.
- **Review** — Ctrl+Shift+E toggles track changes, Alt+↑/Alt+↓
  previous/next comment.
- **View & windows** — Ctrl+Alt+P print layout, Ctrl+Alt+N normal
  layout, Ctrl+Alt+S split window, Alt+Shift+C remove split,
  Ctrl+F6 / Ctrl+Shift+F6 cycle documents, Alt+F8 run script.
- **macOS Command-key parity** — `ev_UnixKeyboard.cpp` folds
  GDK_META_MASK/GDK_SUPER_MASK (⌘ under Quartz/XQuartz) into
  EV_EMS_CONTROL, so every Ctrl binding resolves as its Cmd
  equivalent; Cmd+Shift+Z resolves to `redo` via a platform-conditional
  binding (Ctrl+Shift+Z stays `undo` elsewhere), Option+←/→ move by
  word (`warpInsPtBOW`/`warpInsPtEOW`), Option+Delete deletes a word
  left (`delBOW`), Cmd+; spell check, Cmd+, Preferences.
- **Reassigned (Word takes precedence)** — Ctrl+K hyperlink
  (strikethrough moved to Ctrl+Shift+X), Ctrl+L align-left (was
  bullets), Ctrl+M indent (was symbol dialog), Ctrl+N fileNew on
  the unshifted key with Ctrl+Shift+N applying Normal (was
  new-from-template), Ctrl+Q clear paragraph formatting (was quit;
  quit stays on Alt+F4), Ctrl+Shift+V paste formatting (was a
  second paste), F12 Save As (was input-mode cycling —
  `cycleInputMode` remains available programmatically), Ctrl+=
  subscript and Ctrl+- optional hyphen (zoom remains on
  Ctrl+mouse-wheel).
- **Skipped (no function exists)** — double underline, word-only
  underline, small caps and all-caps format toggles, hanging
  indent, Styles-pane shortcut, format-copy, thesaurus, Shift+F5
  go-back, F8 extend-selection, vertical-block selection, all field
  lock/unlink/toggle shortcuts (F9/Alt+F9/Ctrl+F9/F11 family),
  mark-citation (requires call data), and table AutoSum variants.
- **Dead keybinding entries removed** — every active binding string in
  `ap_LB_*.cpp` is now verified against the registered edit-method
  table: the image double-click binding dropped `dlgFmtImage`
  (method deleted with the old image-format dialog) for
  `selectObject`, the vi `r`-prefix Ctrl+N/N bindings dropped
  `toolbarNew` (method deleted) for `fileNew`/`setStyleNormal`, and
  all commented-out rows referencing deleted mouse-context methods
  were removed.
- **Duplicate and contradictory mouse bindings fixed** — the default
  map carried a duplicated `_CF _B1` row and duplicated `_CTO`
  wheel-up/wheel-down rows; the spare copies are gone. The
  table-cell/TOC/frame/misspelling Ctrl+wheel-down rows had `zoomIn`
  left in the dead double-click slot — normalized to `zoomOut` to
  match every other wheel-down binding.

### User interface

- **Application renamed to Abinova** — all user-facing branding
  moved from AbiWord to Abinova: window title/WM_CLASS
  (`abinova`), executable (`abinova`), `PACKAGE`/`PACKAGE_NAME`
  in `configure.ac`, desktop file, AppStream metainfo, man page,
  `abiword.keys`, user config directory (`~/.config/abinova`,
  with automatic migration from `~/.config/abiword`), About
  dialog and help/documentation text. File-format identifiers
  (`abiword.*` metadata keys, `abiword:` ODF attributes, the
  AWML doctype) are intentionally unchanged for compatibility.
- **New native extension `.abwn`** — new documents save as
  `.abwn` by default (`DefaultSaveFormat`, first entry in the
  export filter list and `preferredSuffixForFileType`, so
  `--to=`/Save As all pick it); `.abw`, `.awt`, `.zabw`,
  `.abw.gz`, `.bzabw`, `.abw.bz2` remain fully readable, as do
  the `.abwn` compressed variants, and content sniffing still
  keys on the `<abiword>` root so the format is unchanged.
  `application/x-abinova` added as an importer mime alias.
- **Copyright attribution follows file provenance** — files
  created in this fork carry an Abinova-only copyright; files
  modified from upstream carry both AbiSource and Abinova;
  untouched files and vendored third-party code (wv, hunspell,
  wpd/wps/wpg sources) keep their original headers unchanged.
- **`.abwn` is now a distinct serialization** — saved files
  declare `<abinova>` as the root element with the doctype
  `<!DOCTYPE abinova PUBLIC "-//ABINOVA//DTD AWNL 1.0 Strict//EN">`
  and namespaces on this repository (`abwn.dtd` ships at the repo
  root); documents loaded from old `.abw` get their namespace
  attributes rewritten on export. The importer accepts both the
  `<abinova>` and `<abiword>` roots so every legacy file still
  opens. **The `.abw` serialization is write-only-off** — the
  exporter no longer registers `.abw`/`.zabw`/`.abw.gz` suffixes,
  the Save As filter offers only `.abwn` variants, `--to=abw`
  fails, and saving an opened `.abw` falls back to Save As →
  `.abwn`. Document metadata now declares
  `application/x-abinova` (which the exporter also accepts).
- **Native importer/exporter renamed to `Abinova_1`** —
  `ie_exp_AbiWord_1`, `ie_imp_AbiWord_1` and the shared
  `ie_impexp_AbiWord_1.h` are now `ie_exp_Abinova_1`,
  `ie_imp_Abinova_1` and `ie_impexp_Abinova_1.h`; the classes are
  `IE_Exp_Abinova_1`, `IE_Imp_Abinova_1`, their sniffers and the
  `s_Abinova_1_Listener`, with every reference
  (`pd_Document`, `ie_exp`, `ie_imp`, `ie_exp_AWT`,
  `ie_impexp_Register`, `ap_EditMethods`, `Makefile.am`) updated.
  The AbiSource copyright stays (the code is still a derivative
  work); `AbiWord`/`AbiSource` in comments, include guards and
  header banners were renamed. Wire-level identifiers
  (`<abiword>` root, `application/abiword` MIME aliases,
  `abiword.date_created`) are deliberately kept for
  compatibility.
- **XML serializer hardened** — `IE_Exp_XML` now keeps a
  start/end-element stack: `endElement()` on an empty stack is a
  logged no-op instead of corrupting output, and `closeHandle()`
  drains any elements a listener left open, so malformed `.abwn`
  output is structurally impossible.
- **Two latent exporter bugs fixed** — `_openTag` no longer emits
  span attributes/properties into the parent element when a `<c>`
  run is suppressed for having nothing to save (the #13708 case),
  and the `math`/`embed` export branches no longer dereference a
  null `pAP` when the strux's attr-prop index resolves to nothing.
- **`.abwn` content sniffer rewritten** — `recognizeContents` is a
  bounded first-six-lines scan over a magic table instead of
  per-magic length checks with manual byte arithmetic; same
  detection semantics.
- **Save As dialog bottom row** — the file-name field now sits in a
  shared grid directly above the "Save file as type" selector so
  both fields share one column (the name entry is exactly as wide as
  the type combo), the "Name:" and type labels are right-aligned,
  and the stray "_" mnemonic marker no longer renders in the
  type label (`gtk_label_new_with_mnemonic`).
- **Contextual Table Layout ribbon tab** — appears only while the
  caret is inside a table and returns focus to Home when it
  leaves. Groups: Table (Select/View Gridlines/Properties/Draw
  Table/Eraser/Delete), Rows & Columns (Insert Above, Insert
  Below, Insert Left, Insert Right, Merge Cells, Split Cells and
  Split Table as Writer-style large buttons), Cell Size (Auto-fit,
  Height/Width spin fields
  synced to the caret cell, Distribute Rows/Columns), Alignment
  (nine-way cell alignment grid, Text Direction, Cell Margins)
  and Data (Sort, Repeat Header Rows, Convert to Text).
  The Word Formula control is intentionally omitted.
- **Merge Cells / Split Cells are anchored popovers** — the old
  floating modeless dialogs opened at the top-left corner under
  GTK4 (no `gtk_window_move`); both are now real `GtkPopover`s
  anchored under their ribbon buttons, offering directional
  merge (left/right/above/below via the new `mergeCellsDir` edit
  method and `FV_View::cmdMergeCellsDir`) and six directional
  split options (`splitCellsDir` → `AP_CellSplitType`). Every
  Table Layout popover is verified anchored beneath its button.
- **Contextual Table Design ribbon tab** — a Word-style "Table
  Design" page appears next to Table Layout while the caret is in
  a table. Groups: Table Style Options (Header Row, Total Row,
  Banded Rows, First Column, Last Column, Banded Columns toggles
  persisted per-table and reflected live), a Table Styles
  thumbnail gallery strip with an always-visible scrollbar and a
  ▼ popover that shows the same tiles grouped into Plain Tables /
  Grid Tables / List Tables sections plus Modify Table Style…,
  Clear and New Table Style… action rows, and a Borders group
  (Shading colour picker, Border Styles popover, a ½ pt-style
  thickness combo, Pen Colour, a Borders preset menu and a Border
  Painter toggle). Tiles live-preview the style on hover and
  commit on click; the engine underneath is a pure recipe table
  (`fl_TableStyles.{h,cpp}` + a generator producing
  `fl_TableStylesBuiltin.cpp` from the real Word `styles.xml`
  built-in definitions, so the 99 styles carry authentic OOXML
  ids) with theme-colour (`theme:accentN[:tNN]`) resolution,
  inside-horizontal/inside-vertical border translation, and the
  six table-look flags. Applying a style balances
  `_changeCellParams`/`_restoreCellParams`, writes cell struxes
  at `pos + 1`, and skips cells belonging to nested tables.
- **Dialog centering restored on X11** — `XAP_UnixDialogHelper::
  centerDialog` now also positions non-modal dialogs over the
  centre of their transient parent via `gdk_x11_surface_move_to_`
  `rect` after map (Wayland relies on the transient-parent hint,
  which is set for all dialogs including About).
- **Ribbon entry/spin fields no longer lose keystrokes** — the
  window-level key controller fed typed characters to the
  document even when a ribbon editable (spin field, search
  entry) had focus; it now defers to the focused widget.
- **Cell-size spin fields hardened** — typing in the Height/Width
  fields applies the value on text-changed debounce/focus-out
  instead of GTK's `value-changed` commit (which raced the
  view-notification refresh and produced a write→readback
  feedback loop); the pending value is captured at change time,
  dimensions are formatted in the C locale, applies dedupe by
  last-applied value, and the caret's table position is captured
  on focus-in so applying after the caret moved still targets
  the right cell.
- **LibreOffice NotebookBar-style ribbon UI** — `GtkNotebook` ribbon
  built from `ap_Ribbon_Layouts.h`, modelled on LibreOffice Writer's
  `sw/uiconfig/swriter/ui/notebookbar.ui`: File / Home / Insert /
  References / Layout / Review / View / Help tabs plus a contextual
  Table tab (shown only while the caret is in a table); compact
  three-row group grids.
- **Rich ribbon controls** — items may reference either menu ids or
  toolbar ids (`AP_RibbonItem`); the Home tab carries the font-family
  and font-size combos (live `AbiFontCombo`, numeric-entry size
  combo), style combo, text/highlight color picker buttons, format
  painter, list preset buttons, indent/unindent, line-spacing and
  paragraph-spacing buttons; the Layout tab has 1/2/3-column preset
  buttons; the View tab has the zoom combo. Toolbar items dispatch
  through the same edit methods as the classic toolbar and their
  toggle/gray/string state refreshes from the same
  `EV_Toolbar_Action` state functions, so ribbon, menubar and
  toolbars stay in sync.
- **New ribbon groups** — Home: Editing (Find/Replace/Select All/Go
  To); Insert split into Pages/Tables/Illustrations/Links/Text/
  Symbols/Fields; new References tab (Table of Contents, Footnotes);
  Layout: Page Setup/Page Columns/Page Background.
- **References tab redesigned to match Word** — primary commands now
  render as large icon-over-caption buttons (Table of Contents,
  Footnote, Endnote, Insert Citation, Insert Caption, Insert Table of
  Figures, Cross-reference, Mark Entry, Insert Index, Mark Citation),
  secondary commands as small labelled dropdowns (Add Text, Update
  Table, Next Footnote/Show Notes, Manage Sources/Bibliography); all
  icons are drawn Cairo glyphs so no icon theme is required. The TOC
  gallery shows Word-style preview cards (heading + four indented
  levels with leaders and page numbers, per-preset styling) and the
  Manual Table inserts `Type chapter title (level N)` placeholders
  with right-aligned dot-leader page-number tabs.
- **Word-style References tab** — the References ribbon is fully
  functional: Table of Contents gallery with Word presets (Automatic/
  Classic/Contemporary/Formal/Modern/Simple plus Manual Table), Add
  Text levels 0–4, Update/Remove Table; Insert Footnote/Endnote,
  Next/Previous note navigation and Show Notes; Insert Caption with
  per-label numbering (Figure/Table/Equation + custom labels,
  above/below); Insert Table of Figures (a TOC driven by the caption
  style); Cross-reference (bookmark-text hyperlink or live page
  number); Mark Entry + Insert/Update Index (sorted entries, `:`
  sub-entries, live `page_ref` fields); Insert Citation, Manage
  Sources and Insert Bibliography (APA/MLA/Chicago/IEEE); Mark
  Citation + Insert/Update Table of Authorities grouped by category.
  Marked entries/citations are real document bookmarks, generated
  sections are wrapped in marker bookmarks for update/remove, and
  everything persists in `.abw`.
- **TOC gallery now offers the five built-in types under both
  sections** — Automatic (field-built, updates from headings) and
  Manual (static placeholder table) each list Classic, Contemporary,
  Modern, Formal and Simple preview cards; manual cards reuse the
  preset's look (`manual-<preset>` dispatch) and the inserted
  placeholder table inherits the preset's Contents/Header styles and
  tab-leader.
- **TOC engine fixes** — `fl_TOCLayout::fillTOC()` now formats the
  container after adding entries so a runtime-inserted TOC renders
  immediately instead of leaving a zero-height broken fragment that
  drew blank; `FL_DocLayout::fillLayouts()` refills every complete
  TOC (`isEndTOCIn()`) after load rather than relying on
  `isTOCEmpty()`, which could not detect TOCs partially filled by
  incremental `addBlock` calls during populate — mid-document TOCs
  previously showed only headings that followed the TOC.
- **References ribbon sizing** — all-large groups (Footnotes: five
  buttons, Index and Table of Authorities: three each) use
  homogeneous grid columns so every button is the same size; small
  icon+label buttons and dropdown captions wrap onto two lines
  instead of ellipsizing; button captions render slightly smaller so
  the wide tab fits without squeezing labels; Footnote/Endnote use a
  slimmer variant to leave room for Update Table.
- **Citations & Bibliography icons redesigned** — Insert Citation
  shows a page with a large quotation mark, Manage Sources is a
  standalone bookshelf of three coloured spines, and Bibliography is
  a standalone bulleted list; the two latter drop the cramped
  page-glyph + corner-badge composition that read as a muddy blob at
  16 px.
- **Citations group relaid out to fit** — the group no longer
  overflows into Captions: Insert Citation and Bibliography stack
  as small icon+label rows in the left column while Manage Sources
  becomes the group's tall button (24 px bookshelf glyph over a
  two-line "Manage / Sources" caption, vertically centred).
- **Footnote/endnote parity with Word** — `Ctrl+Alt+F` inserts a
  footnote and `Ctrl+Alt+D` inserts an endnote (`ap_LB_Default`
  binding table); endnotes now draw the same separator line above
  the first endnote container that footnotes have
  (`fp_EndnoteContainer::draw`, first-fragment only); null-page
  guards added to the endnote draw path and to footnote container
  page lookup.
- **Convert footnotes ↔ endnotes** — the Next Footnote dropdown on
  the References ribbon offers "Convert All Footnotes to Endnotes",
  "Convert All Endnotes to Footnotes" and "Swap Footnotes and
  Endnotes" (`footnoteToEndnote` / `endnoteToFootnote` / `noteSwap`
  edit methods). Conversion preserves note formatting by round-
  tripping each note's content through the RTF buffer, deletes the
  original note section, re-inserts the opposite note type at the
  body reference position, and wraps the whole operation in an
  atomic undo group. Also fixed a latent edit-method table
  misordering (`footnote*` entries were sorted after `format*`),
  which broke `bsearch` lookup for several existing commands.
- **Insert tab redesigned to match Word** — Pages (Cover Page, Blank
  Page, Page Break), Tables (Table), Illustrations (Pictures, Shapes,
  Icons, 3D Models, Screenshot), Media, Links (Hyperlink, Bookmark,
  Cross-reference), Comments (New comment), Header & Footer (Header,
  Footer, Page Numbers), Text (Text Box, WordArt, Drop Cap, Signature
  Line, Date and Time, Field, Object, LRM/RLM) and Symbols (Edit
  Equation, Symbol), all laid out as Word-style large
  icon-over-caption buttons with two-line labels. **Cover Page**
  opens a scrolling 3-column gallery of A4-portrait preview cards for
  twelve designs generated entirely in code — Austin, Banded, Crop,
  Facet, Filigree, Frame, Integral, Motion, Retrospect, Sideline,
  Whisp, Yearly — so no third-party artwork or licensing is involved.
  Covers pull the title/author from document metadata with
  placeholder fallbacks and are wrapped in a `_cover-page` marker
  bookmark; **Remove Current Cover** deletes the page break and
  restores the body. **Blank Page** inserts an empty page at the
  caret (new `insertBlankPage` edit method).
- **Header and Footer built-in galleries** — the Header and Footer
  ribbon buttons open Word-style dropdown galleries of preview cards:
  21 header designs (Blank, Blank (Three Columns), Austin, Badge,
  Banded, Crop, Facet Even/Odd, Feathered, Feathered 2, Filigree,
  Headlines, Integral, Ion Dark/Light, Retrospect, Semaphore,
  Slice 1/2, ViewMaster, Whisp) and 20 footer designs (the matching
  set including Slice, ViewMaster Horizontal/Vertical and Semaphore's
  "Page 1 of 1"). Presets are generated in code like the cover pages —
  shaded bands, border rules, tab-stop columns, small-caps
  placeholders and real `page_number`/`page_count` fields — applied
  by `FV_View::cmdInsertHeaderPreset`, which removes any existing
  header/footer, fills the new shadow and returns the caret to the
  body. Each gallery ends with Edit Header/Footer and Remove
  Header/Footer rows.
- **Insert illustrations dropdowns** — Pictures opens a popover with
  "This Device…" (normal image insert) and "Online Pictures…"
  (URL download + insert); Shapes opens a gallery of ~85
  LibreOffice-style SVG shapes (Basic, Arrows, Symbols, Stars,
  Callouts, Flowchart) recolored to the document accent at insert;
  Icons opens a searchable docked side panel of Lucide icons grouped
  by category; 3D Models opens a gallery of FluentUI 3D emoji PNGs;
  Screenshot captures a screen area via `gnome-screenshot` and
  inserts it; Media links video/audio files as `file://` hyperlinks.
- **Text group additions** — WordArt inserts styled placeholder text
  (Georgia, bold/italic, accent colors) via a preset popover; Draw
  Text Box / Draw Vertical Text Box (vertical uses the frame
  engine's `frame-rotation:90` property); Signature Line inserts a
  sign-here rule with Name/Title placeholders; Object opens a
  popover (file insert / RDF link).
- **Hyperlink dialog always available** — Insert > Link no longer
  greys out without a selection; the dialog gained Word's "Text to
  display" field (prefilled from the selection) and with no
  selection the typed text is inserted and linked.
- **Insert tab artwork attribution** — Lucide icons (ISC licence),
  FluentUI 3D emoji (MIT) and LibreOffice/Yaru shape SVGs
  (MPL-2.0/GPL-3) ship under `artwork/`; all other previews, cover
  pages and header/footer presets are generated in code.
- **RTF insert through Insert File** — the dedicated RTF button was
  removed; the file chooser handles `.rtf` (and the GTK4 open dialog
  now honours an explicit default file type instead of always
  forcing "Automatically Detected"). The Mail Merge Field ribbon
  button was removed from the ribbon (the feature is unchanged).
- **Explicit `toc-level` paragraph property** — `toc-level:0`
  excludes a paragraph from generated tables and `toc-level:1`–`4`
  include any paragraph at that level without a heading style; the
  TOC offer/fill logic honours it independently of style matching.
- **Word-style comments** — the old modal annotation dialog is gone.
  `Ctrl+Alt+M` (or Review → New comment, the Insert → Comments button,
  or the right-click New Comment item) anchors a comment to the
  selection or caret and drops the caret inside the comment body so
  typing starts immediately. A **Reviewing Pane** docks beside the
  document (Review → Show comments → Reviewing Pane) listing every
  comment as a card with author, date, an author-colour stripe and
  the comment text; clicking a card selects the anchored text, and
  each card offers Reply, Resolve/Unresolve and Delete plus a New
  Comment button below the list. Replies append extra paragraphs to
  the comment shadow and render on separate lines. Resolved comments
  persist through `annotation-resolved:1` and display dimmed with a
  "(Resolved)" badge. Review → Delete drops a menu (Delete Comment /
  Delete All Comments), Resolve toggles the comment at the caret,
  Previous/Next step through comments (`Ctrl+Alt+N` / `Ctrl+Alt+P`),
  and deleting a comment always preserves its anchored text. The pane
  auto-opens when a comment is inserted and refreshes on document
  changes (debounced so typing inside a comment is uninterrupted).
- **Comment anchors are visibly highlighted** — the text a comment is
  attached to is tinted with a pale shade of that comment's colour
  (like Word's comment-range highlight), drawn under the selection
  layer so selection still wins, and a bare-caret comment anchors to
  the word under the caret instead of an invisible zero-width point.
  Opening the Reviewing Pane also enables the annotation display
  preference automatically so anchors are always visible when
  comments are being browsed.
- **Multiple comments on the same text** — overlapping and same-range
  comment anchors are now representable in the `.abw` format: the
  Abinova-1 exporter tracks open `<ann>` elements as a nesting depth
  instead of a single flag, so anchors nest properly instead of one
  silently truncating the other into an empty anchor. Anonymous end
  objects pop the innermost open anchor, and section-boundary closes
  flush all open anchors. The importer already accepted nested
  `<ann>` elements, so old documents load unchanged and nested files
  still parse on older versions (they read the same object order).
- **Annotation robustness fixes** — `insertAnnotation` now reads the
  view-level selection anchor (the raw stored anchor could point
  elsewhere and fail with "blocks differ" when inserting at a bare
  caret); comments may no longer be inserted inside another comment's
  shadow (that produced nested `<annotate>` XML that could not be
  reloaded — span-level nesting over existing anchors stays legal);
  `changeStruxFmt` calls on embedded annotation struxes now pass
  `pos+1` like the rest of the code, fixing resolve/title/author
  updates that silently no-oped; comment anchors are no longer
  mistaken for real hyperlinks by the anti-nesting check.
- **Word-style Review ribbon tab** — the Review tab is rebuilt as
  Proofing / Language / Comments / Tracking / Changes groups. The
  Spelling & Grammar button opens a popover with spell and grammar
  toggles; Set Language opens a reworked dialog with "(no proofing)",
  a "Do not check spelling or grammar" checkbox (applies the `-none-`
  proofing code and desensitises the list) and a "Detect language
  automatically" checkbox that scores the text around the caret
  against every installed dictionary and selects the best match.
  Show Comments splits into Contextual (in-document annotations via
  the `DisplayAnnotations` preference) and List (the docked Reviewing
  Pane). The Tracking group holds a Track Changes checkable popover
  (Track Changes, Auto Revision, Start New Revision, Purge) and a
  Display-for-Review dropdown offering Word's four modes — Simple
  Markup draws a red change bar in the left margin on lines with
  revisions (`FV_View::setShowRevBars` + `fp_Line::draw`), All Markup
  shows inline markup, No Markup and Original hide it — and the
  button caption tracks the active mode. All icons are drawn Cairo
  glyphs.
- **Word-style Accept/Reject and Compare controls** — Accept and
  Reject are large ribbon buttons (same size as Reviewing Pane) with
  Word's full dropdown menus: Accept/Reject and Move to Next
  (`revisionAcceptNext`/`revisionRejectNext`), This Change, All
  Changes Shown (`PD_Document::acceptAllRevisionsUpTo` /
  `rejectAllRevisionsUpTo` — only revisions at or below the view's
  revision level, i.e. the ones currently displayed), All Changes,
  and All Changes and Stop Tracking. The buttons now enable whenever
  the document has revisions rather than only when the caret sits on
  one. The Compare group is a dropdown offering "Compare Documents…"
  and "Combine Documents…" (`revisionCombineDocuments`) which appends
  another open document's paragraphs to the current one as tracked
  insertions — skipping spans that are revision-deleted in the
  source — so the merge can be reviewed and accepted/rejected like
  any other change; both source documents are left unmodified.
  Untitled documents now show a real entry in the pick-list instead
  of a blank row.
- **Compare produces a legal blackline** — "Compare Documents…" now
  diffs the current document against a second open document at word
  level (a Myers O(ND) diff over paragraph/word tokens) and opens a
  NEW document containing the merged text where every difference is
  a real revision: words only in the original appear as deletion
  marks, words only in the revised version as insertion marks. The
  result is reviewed with the standard Accept/Reject tools — e.g.
  Accept All yields exactly the revised document — and the two
  source documents are never modified. Identical documents and
  inputs that are too large or too different for a bounded diff
  show a message instead. This replaces the old statistics-only
  comparison report.
- **Set Language dialog applies again** — the apply path in
  `s_doLangDlg` was dead code (a stale `k > 0` gate), so OK never
  changed anything; the selected language now applies to the
  selection/caret and the "Make default for document" checkbox
  writes the document-level `lang` property.
- **Review-tab engine fixes** — `rejectAllHigherRevisions(0)` now
  backs Reject All (the earlier `cmdFindRevision` loop silently did
  nothing in Simple/No Markup because hidden revision runs are
  skipped); `EnchantChecker::doesDictionaryExist` probes
  `enchant_broker_dict_exists` so language detection works under the
  enchant backend (`getMapping()` was always empty there); a
  use-after-free in `detectLanguage` (winning code pointed into the
  freed dictionary list) and inverted `getEditableBounds` flags in
  the sample-text feeder are fixed.
- **Review ribbon uniform sizing and popover alignment** — every
  Review control is now a large icon-over-caption button (Spelling
  and Grammar, Track Changes, Display for Review, Accept, Reject,
  Compare etc. match Set Language), and the dead "Hide Ink" button
  and empty Ink group are removed. Popover rows share a fixed-width
  leading icon slot so icon and check-mark rows align in one column;
  the check indicator is a drawn 16px glyph that swaps in place of
  the row icon, and every row in the Spelling/Track Changes/Display/
  Accept/Reject/Compare/Comments menus now has a drawn icon
  (including new auto-revision, new-revision and purge glyphs).
- **Status-bar word-count leak fixed** — `ap_sbf_WordCount`
  `g_strdup`'d its printf format but never freed it (24 bytes per
  frame, definitely lost under valgrind); a destructor now frees it,
  matching `ap_sbf_PageInfo`.
- **Word-style View ribbon tab** — the View tab is rebuilt as
  Document Views / Immersive / Show / Zoom / Window groups. Print
  Layout, Web Layout and Draft (Normal) are large radio buttons with
  drawn page glyphs; Focus (full screen) sits alone in the Immersive
  group; the Show group holds Ruler / Status Bar / Formatting Marks /
  Selection Pane checkboxes. The Zoom button opens a popover of
  presets (200 %/100 %/75 %/50 %, Page Width, One Page — active
  preset ticked, plus a Zoom… dialog row); Zoom to 100 %, One Page
  and Page Width are large buttons. The Window group offers New
  Window (`newWindow` frame clone) and a Switch Windows dropdown
  listing every open frame (current ticked, "Switch Windows…"
  picker past nine entries). Ribbon-only caption overrides rename
  classic labels for the ribbon (Draft, Focus, One Page, Ruler,
  Status Bar, Formatting Marks); the classic menubar is unchanged.
- **Word-style Home ribbon** — layout items now carry flags
  (`AP_RIBBON_FLAG_LARGE`, `AP_RIBBON_FLAG_ICONONLY`,
  `AP_RIBBON_FLAG_SPLIT`): Paste, Find, Replace and Select All render
  as large icon-over-caption buttons, bold/italic/underline/overline/
  super/subscript and the alignment buttons are compact glyph-only
  tiles, the Editing group's buttons fill the group height with
  centred wrapped captions, and group titles sit centred at the
  bottom of each group. Paste and the list buttons render as split
  buttons (icon click = action, arrow = dropdown); ribbon buttons
  fall back to the menu item's stock icon when no toolbar icon is
  registered (`abi_stock_from_menu_id`).
- **Live Styles gallery** — the Home Styles group embeds a
  horizontally-scrolling strip of preview tiles, one per displayed
  paragraph style in the document; each tile's caption is the
  localized style name rendered in the style's own resolved font
  family, weight, slant, underline, size (clamped) and color via
  `PD_Style::getPropertyExpand`. Clicking a tile applies the style
  through the same `AP_TOOLBAR_ID_FMT_STYLE` edit method as the style
  combo, and the tile matching the caret's current style is
  highlighted with an accent border (`abiword-style-active`). Tiles
  are populated lazily because the ribbon is constructed before the
  frame's view/document exist.
- **Interface switcher** — Help → Interface submenu (Classic Menus /
  Ribbon radio items) in both UIs; `RibbonUI` preference persists the
  choice; switching is live.
- **Home tab is the default ribbon tab** — the ribbon notebook
  explicitly selects the `home` page after building the tabs.
- **LibreOffice-style two-row Font group** — the Home ribbon Font
  group packs row-major via a new `AP_RIBBON_ITEM_ROWEND` layout item:
  row 1 holds the font family + size combos and Grow/Shrink/Clear
  Formatting, row 2 the glyph strip B/I/U/S/x²/x₂/highlight/font
  colour + Font dialog launcher. `AP_RIBBON_FLAG_GLYPH` buttons draw
  Pango-markup glyphs (bold **B**, italic *I*, underlined U, x², A⁺)
  instead of theme icons; font colour is a bold "A" with a red
  underline, highlight an "ab" on a yellow swatch. Ribbon buttons fall
  back to their text label when the theme lacks the icon.
  `AP_RIBBON_FLAG_EVEN` packs adjacent items into a homogeneous box so
  the Grow/Shrink/Change-Case buttons all get the same moderate width
  (`AP_RIBBON_FLAG_SLIM` trims their padding).
- **New formatting menu items** — Format → Text gains Grow Font /
  Shrink Font / Clear Formatting (`AP_MENU_ID_FMT_GROWFONT`,
  `FMT_SHRINKFONT`, `FMT_CLEARFMT` → `fontSizeIncrease`,
  `fontSizeDecrease`, new `clearFormatting` edit method wrapping
  `FV_View::resetCharFormat`). Insert gains Edit Equation
  (`editLatexAtPos`) and Help gains Credits (`helpCredits`) so the
  ribbon buttons that reference them resolve to real `GAction`s; the
  permanently-disabled Split Table entry was removed from the ribbon.
- **Word-style split Paste button** — the ribbon Paste control is a
  split button: the icon pastes immediately with formatting, the
  arrow opens a dropdown with "Paste Options:", "Keep Text Only"
  and "Paste Special…".
- **Paste Special dialog** — lists the clipboard's real formats and
  pastes the chosen one through the normal importer dispatch
  (`FV_View::cmdPasteAs` → `XAP_App::pasteFromClipboardWithFormat` →
  `AP_UnixApp::pasteDataToDocRange`). All `image/*` types group into
  a single "Picture" entry (best available format chosen:
  PNG > SVG > JPEG > …) and alias duplicates collapse into one row —
  `text/plain`/`UTF8_STRING`/`TEXT`/`STRING` → "Unformatted Text",
  `text/rtf`/`application/rtf` → RTF, `text/html`/`application/xhtml+xml`
  → HTML. The paste runs from an idle callback after the dialog
  closes, avoiding re-entrant clipboard access during modal response
  handling.
- **LibreOffice-style status bar** — `Page: n/m`, live
  `N words, N characters`, current paragraph style, insert/overwrite
  and input-mode indicators, document language, and a zoom cluster
  (`−` / slider / `+` / `NNN%` / 100% reset).
- **LibreOffice-style font box** — editable `GtkEntry` for the
  current font doubles as the search field: typing opens a popover
  directly below the field (flush with its left edge) whose
  `GtkListView`/`GtkFilterListModel` list filters live on the typed
  text, and clicking a row applies the font immediately. The arrow
  button drops the full list, preselecting and scrolling to the
  current font; keystrokes captured by the popup's seat grab are
  forwarded back to the entry so typing never dead-ends. Rows are
  bound lazily and render in their own typeface (only visible rows
  load fonts — the old cell renderer measured ~2000 fonts on popup
  open and froze the UI).
- **Change Case "Aa" dropdown** — single menu button on the Font
  group's top row (next to Grow/Shrink Font): clicking it drops a
  popover with five direct conversions (Sentence case, lowercase,
  UPPERCASE, Capitalize Every Word, tOGGLE cASE) wired to new
  `caseSentence`/`caseLower`/`caseUpper`/`caseTitle`/`caseToggle`
  edit methods over `FV_View::toggleCase`; no separate arrow widget.
- **Ribbon colour pickers rebuilt** — Font Color and Highlight now
  open a LibreOffice-style swatch grid (Automatic button, 40-colour
  standard palette, "Custom Color…" button). One click on a swatch
  applies the colour and closes; Custom Color opens a real
  `GtkColorChooserDialog` with Select/Cancel so the built-in picker
  is always reachable again — the embedded `GtkColorChooserWidget`
  it replaces had no reliable way back and only applied on
  double-click. Font colour defaults to black.
- **Page centering** — pages center horizontally when narrower than
  the window (LibreOffice behaviour); no phantom scrollbar.
- **Ruler redesign** — full-height bar, gray margin bands, white text
  band, bottom-anchored long/short tick hierarchy, zoom-exempt GUI-font
  numeric labels, flat triangle indent markers, black foreground text.
- **Column-gap ruler marker restyled** — the multi-column gap handle
  was a large dark hexagon drawn over the tick labels; it is now a
  small flat triangle on the bottom edge of the bar, matching the
  indent markers and keeping the numbers readable.
- **Dedicated dash-list toolbar button** — `doDashedList` edit method,
  toggle state, labels/tooltip, `tb_lists_dashed` icon.
- **Distinct list icons** — redrawn bullet/numbered/dash icons
  (LibreOffice-like bold style) plus pixel-perfect 16×16 variants so
  the markers stay legible at toolbar size.
- **Style dropdown cleaned** — list styles no longer clutter the
  paragraph-style combo (applied via the list buttons); the current
  list style still displays transiently.
- **Fixed About dialog** — logo via compiled-in icon name, proper
  program name, GPL-2.0 license, fork note.
- **Print preview / PDF export** — PDF restricted to 1.7 (ISO
  32000-1), Title metadata written, teardown order corrected.
- **Internal help bundled** — the `abiword-docs` manual converted to
  HTML (`help/`, 220 pages); Help buttons open the local copy.
- **Set Language dialog** — explicit Cancel/Apply; selection commits
  only on Apply.
- **Default file format preference** — Preferences → Documents can
  pick the save format from registered exporters (Debian #424037).
- **Paragraph group redesigned (LibreOffice-style)** — the Home
  Paragraph group now mirrors Writer: row 1 = bullet/numbering/
  multilevel split-buttons, indent-less/more, paragraph sort,
  show formatting marks; row 2 = alignment, line-spacing and
  paragraph-spacing dropdowns, borders, paragraph dialog launcher;
  the separate Lists group was merged in.
- **Bullet/Numbering/List libraries** — each list split-button drops
  a LibreOffice-style library popover: Bullet Library (13 glyph
  tiles incl. None), Numbering Library (1. / 1) / I. / A. / a) / a. /
  i. tiles), List Library (Current List + multi-level presets).
  Tiles invoke a new `doListType` edit method which retypes an
  existing list in place via `fl_AutoNum::setListType` (bullet
  styles switch without unlisting) and supports decimal/delimiter
  overrides (`%*%d`, `%L)`); "Define New ..." entries open the
  Bullets and Numbering dialog.
- **Paragraph sorting** — new sort control drops Ascending (A-Z) /
  Descending (Z-A); `FV_View::cmdSortParagraphs` orders the selected
  paragraphs (or current paragraph block) by case-folded UTF-8
  collation inside a single undo glob, moving only text so paragraph
  properties stay in place.
- **Slimmer split-button arrows** — the drop-arrow wedge on ribbon
  split-buttons is now ~12px with zero padding.
- **Word-style Borders dropdown** — the Paragraph group's borders
  button is now a single menu-button that drops the classic border
  menu: Bottom/Top/Left/Right Border, No Border, All/Outside/Inside
  Borders, Inside Horizontal Border (Inside Vertical, diagonal
  borders and View Gridlines are shown but disabled — no paragraph
  equivalent), Horizontal Line (breaks the paragraph and draws a
  bottom-edge rule), Draw Table (Insert Table dialog) and "Borders
  and Shading…" which opens the existing dialog. Each row shows a
  cairo-drawn edge-diagram icon; presets apply 0.5pt solid black
  borders through a new `paraBorder` edit method +
  `FV_View::cmdParaBorder` (per-block `changeStruxFmt`, single undo
  glob, "inside" = bottom edge on every selected block but the
  last).
- **LibreOffice-style Styles group + docked Styles pane** — gallery
  tiles now render two lines (styled "AaBbCcDdEe" sample over the
  localized style name), `<`/`>` scroll arrows appear at the strip
  edges when tiles overflow, and a new "Styles Pane" button opens a
  docked pane (`GtkPaned` end child on the document area): current
  style readout, "New Style…" (Styles dialog), "Apply a style" list
  with a "Clear Formatting" row and every displayed paragraph style
  rendered in its own formatting, a "List: Recommended/All Styles"
  filter and the guides checkboxes (disabled — no guides backend).
  The old in-group style combo and Stylist/Create items were
  removed; the hidden `FMT_STYLE` toolbar item still feeds its state
  to the tile highlight and the pane's current-style readout.
- **Word-compatible built-in style set** — the built-in styles
  (`pt_PT_Styles.cpp`) now match Microsoft Word's gallery: Normal
  (12 pt, 1.15 line spacing), No Spacing, Heading 1–9 (16/14/13/12/
  11/11/10/10/10 pt bold with Word's spacing-before/after, Heading 4
  also italic), Title (26 pt bold centred), Subtitle (14 pt italic
  centred), Quote and Intense Quote (italic / bold italic, 0.5"
  indent, 1.5 pt gray left border), Book Title, List Paragraph, and
  the character styles Emphasis, Strong, Subtle Emphasis, Intense
  Emphasis, Subtle Reference and Intense Reference. Character styles
  are now shown in the gallery and the Styles pane too (applying
  them sets the run-level style on the selection via the existing
  `changeSpanFmt` path), and the gallery tiles order like Word's.
  Old Abinova-only styles (Block Text, Plain Text, Chapter/Section/
  Numbered Heading) remain defined for document compatibility but
  are hidden from the Recommended list; the .doc importer's
  `s_translateStyleId` now maps Heading 5–9, Title, Subtitle, Strong
  and Emphasis to real built-ins.
- **Internal help window** — Help Contents / Search for Help /
  Credits no longer launch an external browser; they open an in-app
  "Abinova Help" window (`xap_UnixHelpWindow`, behind a new
  `XAP_AppImpl::openHelpWindow` virtual so other toolkits keep the
  old URL behaviour): a toolbar with Back/Home, a language selector
  (English / Français / Polski switching between the bundled
  `help/en-US`, `help/fr-FR` and `help/pl-PL` trees) and a live
  search field that scans every page of the current language and
  lists results as linked titles with context snippets. Pages are
  rendered from the bundled HTML into a `GtkTextView` (headings,
  bold/italic/mono/underline, list bullets, clickable links —
  external `http(s)`/`mailto:` links still open in the browser) with
  a page history for Back.
- **Visible ribbon group separators** — the separators between
  ribbon groups are now drawn as a real 1 px line
  (`separator.ribbon-group-sep` with an explicit border colour);
  the theme default was invisible, leaving e.g. the Help tab's Help
  and Interface groups visually merged.
- **Polish help converted to real UTF-8** — the 25 `help/pl-PL`
  pages declared `charset=UTF-8` but stored text in mixed
  UTF-8/Windows-1250, producing mojibake ("znalazÅ‚eÅ›"); each file
  was re-encoded keeping valid UTF-8 sequences and decoding the
  stray legacy bytes as CP1250, so Polish diacritics now render
  correctly.
- **File tab redesigned like the Help tab** — all items are large
  icon-over-label buttons (New, New using Template, Open, Save,
  Save As, Revert, Properties, Close | Page Setup, Print Preview,
  Print) with new icon mappings for the template and page-setup
  entries; the redundant "Save a Copy" item was removed from the
  ribbon (Save / Save As cover it). "Properties" is now captioned
  "Document Properties" with an information icon, and the Close
  button's X icon is tinted red.
- **Word-style Layout ribbon tab** — Page Setup group of large
  icon dropdown buttons: Margins (Normal/Narrow/Moderate/Wide/
  Mirrored preset gallery with page-glyph illustrations, current
  preset checkmarked, Custom Margins…), Orientation (Portrait/
  Landscape), Size (all `fp_PageSize` presets incl. newly added
  Executive and 8.5×13, scrollable extras, More Paper Sizes…),
  Columns (One/Two/Three with column glyphs, Left/Right disabled,
  More Columns… → Columns dialog), Breaks (Page/Column/Text
  Wrapping + Next Page/Continuous/Even/Odd section breaks),
  Line Numbers and Hyphenation (options dialogs that store the
  document properties even though the layout engine does not
  render them yet). Paragraph group gains Left/Right indent and
  Before/After spacing spin fields synced from the cursor's
  paragraph. Arrange group is fully functional: Position presets
  (top-left/center/right with square wrapping, More Layout
  Options… → the frame dialog), Wrap modes (Square / Top and
  Bottom / Behind Text / In Front of Text via `wrapObject`),
  Align (left/center/right via `frame-horiz-align`), Bring
  Forward / Send Backward Z-ordering, the Selection Pane toggle,
  and Group / Rotate popovers (below). Only Wrap's "In Line with
  Text" row stays disabled — the engine has no inline-frame mode.
- **Word-style Document dialog** — new `AP_DIALOG_ID_DOCUMENT`
  (`ap_Dialog_Document` + `ap_UnixDialog_Document`) with Margins
  (top/bottom/left/right, gutter + gutter position, multiple
  pages, live preview, Apply to: whole document/this section/
  this point forward) and Layout (section start, different
  odd/even and first-page headers/footers, header/footer from
  edge, vertical alignment, Line Numbers…/Borders… launchers)
  tabs, plus Page Setup… (opens the regular page-setup dialog)
  and Default… (writes the current page setup to the NORMAL
  template after confirmation). Invoked via `docSettings` from
  Format → Document and the Layout ribbon.
- **New layout edit methods** — `pageMargins`, `pageOrientation`,
  `pageSize`, `pageColumns`, `insColumnBreak`, `insSectionBreak`,
  `paraProp`, `sectProps`, `docProps`, `docSettings`,
  `arrangePosition`, `wrapObject`; plus
  `FV_View::setDocWideSectionFormat()` to apply section
  properties to every section in the document.
- **Edit-method table ordering fixed** — `s_arrayEditMethods`
  requires strcmp ordering for its binary-search lookup; the new
  methods were inserted unsorted (and the pre-existing
  `doNumbers`/`doDashedList` pair was swapped), which silently
  dropped them from dispatch — the layout popover actions did
  nothing until the array was re-sorted.
- **Dialog teardown crash fixed** — `AP_UnixDialog_Document`
  hand-built `GtkStringList` models for its `GtkDropDown`s and
  unref'd them immediately, leaving the dropdowns pointing at
  freed models (`g_list_model_get_n_items` SIGSEGV on destroy);
  the dropdowns now use `gtk_drop_down_new_from_strings`, and
  widget values are read in the `response` handler before the
  helper destroys the window.
- **Object Z-ordering (Bring Forward / Send Backward)** — the
  Arrange group's Bring Forward and Send Backward are now real
  popover menus matching Word: Bring Forward, Bring to Front,
  Bring in Front of Text / Send Backward, Send to Back, Send
  Behind Text. Frame stacking is driven by the new persistent
  `frame-stack-order` frame property (`pp_Property`,
  `fp_FrameContainer::getStackOrder`); `fp_Page` keeps each
  above/below-text layer sorted by rank and
  `restackFrameContainer` moves a frame one step or to the edge,
  while `FV_View::restackFrame` writes the new rank through
  `setFrameFormat` so reordering is undoable and survives
  save/reload in `.abw`. `frameSetTextLayer` switches
  `wrap-mode` between `above-text`/`below-text` for the
  text-layer commands. Sensitivity uses the new
  `ap_GetState_ObjSelected` (frame edit active, image selected
  or caret inside a frame).
- **Page Color / Page Image restyled as large ribbon buttons** —
  they now match Margins: a 24px drawn page glyph (paint-drop
  badge / picture badge) stacked over the caption, instead of a
  small icon beside the label.
- **Distinct Z-order icons** — Bring Forward and Send Backward
  use dedicated bare glyphs (a staircase of squares with a blue
  front square plus a bold up/down arrow) instead of page
  glyphs, so they are no longer confusable with the page-setup
  icons.
- **Columns dialog preview fixed for GTK4** — the preview
  graphics/preview are now created lazily inside the draw
  callback (`event_previewDraw(cr, width, height)`), because the
  drawing area has no usable allocation in `runModal()`; column
  count and "line between" update the preview live. Toggle
  buttons use `gtk_button_set_child` so the images unparent
  cleanly at teardown.
- **Slimmer spin-button +/- controls** — the Layout tab's
  indent/spacing `GtkSpinButton`s get a `ribbon-spin` class with
  zero-minimum, low-padding buttons.
- **Indent/Spacing fields aligned** — the spin labels ("Left:" vs
  "Right:", "Before:" vs "After:") had different widths, so the
  entry columns sat ragged; labels now share a fixed width so the
  spin boxes line up. Before/After also got proper drawn spacing
  glyphs (text lines + an arrow on the padded edge) instead of
  reusing the indent icons.
- **Selection Pane (Word-style object list)** — a docked right-side
  pane lists every frame object in the document front-to-back
  (text boxes, positioned images, table/embed wrappers) with a
  per-type icon and name. Rows select the object in the document
  (`FV_View::selectFrameObject` puts the frame into edit mode and
  moves the caret inside), an eye button toggles visibility, the
  bottom up/down buttons reorder the Z-layer, and a double-click on
  the name renames it. Two new persistent frame properties carry the
  state in `.abw`: `frame-hidden` (hidden frames are skipped by
  `fp_Page` drawing and hit-testing but keep their layout slot) and
  `frame-name` (falling back to `Text Box N`/`Picture N` defaults).
  All writes go through `FV_View::setFrameProp` so they are undoable.
  The pane shares the deck with the Styles pane (a `GtkStack` in the
  `GtkPaned` end child), toggles from the Arrange group's ribbon
  button (`sidebar-show-symbolic`) or `Alt+F10`, and refreshes when
  the document's frame set changes (`ap_UnixViewListener` →
  `refreshSelPane`, gated on an identity check so caret motion does
  not rebuild the list). Note: on GNOME, `Alt+F10` is the WM's
  toggle-maximized shortcut and never reaches the app — use the
  ribbon button there. `EV_UnixMenu::ensureAction` creates
  `GSimpleAction`s for ribbon-only menu ids so the button works
  without a classic-menubar entry.
- **Object grouping (Word-style Group/Ungroup)** — two or more
  objects ticked in the Selection Pane (or selected in frame edit)
  can be combined into one logical unit via the new persistent
  `frame-group` property (`FV_View::groupFrames` assigns the next
  free `gN` id and compacts the members into one contiguous Z-order
  block). Grouped objects move together — a whole-frame drag shifts
  every member by the same delta inside the same undo glob
  (`FV_FrameEdit::mouseRelease` → `FV_View::shiftFrameGroup`) —
  restack together (`fp_Page::restackFrameContainer` moves the whole
  group block while preserving member order), and rotate/flip
  together around the group's bounding-box centre
  (`FV_View::_groupTransform` orbits/mirrors each member's centre
  and updates its own rotation/flip props). Ungroup removes the id
  via `PTC_RemoveFmt` (an empty-value `PTC_AddFmt` write is a no-op,
  which initially left the property in place). The Selection Pane
  shows a `[gN]` badge per member and offers Group/Ungroup buttons —
  Group is insensitive until two objects are ticked
  (`ap_GetState_Groupable`).
- **Object rotation and flipping** — `frame-rotation`,
  `frame-flip-horiz` and `frame-flip-vert` are new persistent frame
  properties applied in `fp_FrameContainer::draw` as a cairo
  transform around the frame centre (rotation normalised to
  [0,360), flips as negative scales). The Layout ribbon's Rotate
  button opens a Word-style popover: Rotate Right 90° / Rotate Left
  90° / Flip Vertical / Flip Horizontal plus a custom-angle field
  (`frameRotateTo`). Selection handles follow the transform in
  `drawHandles`, hit-testing un-rotates the point back into frame
  space (`fp_FrameContainer::unrotatePoint` used by
  `fp_Page::mapXYToPosition`), and damage tracking uses the rotated
  ink bounding box (`getInkBounds`/`s_rotatedBounds`) so rotated
  content is not clipped or left undrawn. Rotation and flips survive
  `.abw` save/reload and also render in the PDF export path.
- **Built-in equation engine (mathview replacement)** — `PTO_Math`
  objects render natively again without the removed `mathview`/
  `lasem` plugin: a new `GR_GtkMathManager` embed manager
  (`src/af/gr/gtk/`) is registered by `XAP_App::initialize()` and
  drives a self-contained LaTeX-subset + MathML typesetter
  (`src/af/gr/xp/gr_MathTypesetter.*`, box-model layout drawn
  straight to Cairo — no external math library). Fractions, roots,
  scripts, sums/products/integrals with limits, big operators,
  matrices, delimiters, accents, `\text{}` and styled groups are
  supported in both inline and display style. The existing LaTeX
  dialog (`editLatexAtPos`), `fp_MathRun` layout and `.abw`
  serialization (LaTeX + MathML data items, SVG snapshots for
  persisted resources) all work through the manager.
- **Insert → Equation gallery** — Word-style Built-In gallery in
  the Symbols group: ten preset equations (Quadratic Formula,
  Pythagorean Theorem, Euler's Identity, Binomial Theorem, Fourier
  Series, Taylor Expansion, Gaussian Integral, Trig Identity,
  Expansion of a Sum, Area of Circle) shown as live typeset preview
  tiles plus an "Insert New Equation" row that opens the LaTeX
  dialog. Presets insert a `display:` or `inline:` equation via the
  new `insertEquation` edit method.
- **Contextual Equation ribbon tab** — appears whenever the caret
  or selection touches a math object (`FV_View::isInMath` checks the
  frag before the point and scans the selected range; contextual
  pages now carry a per-tab context key so table and equation tabs
  are tracked independently). The tab holds an Equation group
  (gallery + Display/inline toggle via `toggleEquationDisplay`), a
  41-glyph symbol palette and a 19-item structure palette
  (fractions, roots, integrals, sums, matrices, delimiters,
  accents). Palette buttons append their LaTeX snippet to the
  equation at the caret — re-rendering it in place through the new
  `equationInsertSymbol` edit method — or insert a new inline
  equation when no math object is under the caret.
- **Math render-loop fixed** — `fp_MathRun::_lookupProperties` used
  to `markAsDirty()` + `setNeedsRedraw()` on every layout pass,
  which rescheduled layout forever (~90% CPU on a document with
  equations); it now only dirties the run when width/ascent/descent
  actually change. The manager also renders inside the active paint
  context instead of nesting `beginPaint`/`endPaint` (which
  invalidated the draw surface mid-paint), and `makeSnapShot` skips
  rewriting an unchanged SVG data item so saves do not ping-pong.
- **Edit-method table ordering fixed** — the static
  `ap_EditMethods` table is searched with `bsearch`, so entries must
  stay alphabetically sorted; `insertEquation`,
  `insertLatexEquation`, `equationInsertSymbol` and
  `toggleEquationDisplay` were registered out of order and silently
  failed to dispatch (button clicks invoked the method name but the
  lookup returned NULL). All four are now in sorted position.
- **Real WordArt text effects** — new character-level properties
  `text-outline`, `text-gradient`, `text-shadow` and
  `text-reflection` are rendered by the Cairo text pipeline: a
  `GR_TextEffects` state on `GR_Graphics` (scoped per run from
  `fp_TextRun::_draw`) makes `GR_CairoGraphics` paint glyph
  *outlines* instead of plain `pango_cairo_show_glyph_string` —
  stroked outline under the fill, two-stop linear gradient fills,
  offset drop shadows and a vertically-mirrored reflection masked
  with an alpha fade. Effects apply on screen and in the
  print/PDF path, ride in `.abw` character props (unknown-prop
  tolerant on reload), and text stays fully editable —
  reformatting or re-styling just changes the props.
- **Word-style WordArt gallery** — the Insert → WordArt dropdown is
  now a 15-tile preset gallery (flat fills, outlines, gradient
  fills, shadows, reflections and combos) whose tiles are rendered
  live with the same effect pipeline (Pango glyph paths + Cairo
  fills). `insertWordArt` takes a `key=value;…` spec
  (`font/size/weight/italic/color/outline/gradient/shadow/reflect`)
  and applies the preset exactly — effect properties the spec does
  not mention are explicitly cleared so a re-styled WordArt does not
  inherit stale effects; legacy `fill-RRGGBB`/`outline-RRGGBB` specs
  still work.
- **File tab Settings group** — **Preferences** (Options dialog) and
  **RDF Settings** (RDF editor) are now reachable from the ribbon,
  replacing the classic Tools/RDF menu access; both carry themed
  icons via new `stock_mapping` entries.
- **Classic-menu leftovers pruned** — the remaining features with no
  ribbon home were removed entirely rather than left dangling:
  Web Preview, Mail Merge, the Tabs dialog (including the Paragraph
  dialog's Tabs button), the Format Frame/Image menu entries, the
  Direction submenu, the Stylist menu entry, document History,
  Revisions → New/Purge, Scripts, Text → Table conversion and the
  Recent Files list — along with their menu ids, action bindings,
  edit methods and dialog files. Internal code other subsystems
  still use (the Stylist picker inside Format TOC, the image dialog
  used by positioned images, mail-merge field machinery) stays.
- **Stock icon identifiers renamed** — the ~50 internal
  `abiword-*` action-icon lookup keys (`ABIWORD_STOCK_PREFIX` and
  the `stock_mapping` table) are now `abinova-*`; ribbon CSS classes
  and the online-picture temporary name renamed to match.
- **Preferences dialog slimmed down** — the empty "Application
  Startup" section and the whole "Spell Checking" tab (spell/grammar
  options are controlled elsewhere) were removed, along with the
  dialog's Help button; unused spell-option control ids, widget
  plumbing and strings were dropped.
- **Library renamed to `libabinova`** — the shared library builds as
  `libabinova-4.0.so` (was `libabiword-4.0.so`); the public API files
  moved to `wp/main/gtk/libabinova.{h,cpp}` with `libabinova_init`/
  `libabinova_init_noargs`/`libabinova_shutdown` entry points, the
  test library is `libabinova-4.0-test`, and pkg-config now ships
  `libabinova.pc`/`abinova-4.0.pc` (generated `.pc` artifacts dropped
  from the tree — they were stale and are gitignored).
- **Application id renamed to `io.github.janos_szenfner.Abinova`** —
  GApplication id, gresource prefix
  (`/io/github/janos_szenfner/Abinova`), desktop/metainfo filenames
  and the installed icon theme name (`abinova`) now follow the
  reverse-DNS scheme derived from the repository URL, so the running
  app, its icons and its resources share one identity.
- **Bundled manual replaced with a single page** — the 108-file
  inherited HTML manual (howto/info/interface/problems/tutorial/
  plugins trees, stale AbiWord content) was deleted; the internal
  help browser now ships one page, `help/en-US/index.html`, written
  for the current feature set, and missing pages (e.g. stale
  per-dialog help URLs) fall back to the index.
- **Changelog button on the Help ribbon** — `AP_MENU_ID_HELP_CHANGELOG`
  + `helpChangelog` edit method open the help window at
  `changelog.html`, generated at build time from `CHANGELOG.md` by
  `tools/changelog2html.sh` (awk, no extra deps); a drawn clock-badge
  page glyph serves as the icon, and the classic Help menu gained the
  same entry.
- **Find/Replace dialog rewritten from scratch** —
  `ap_UnixDialog_Replace` replaces the partially-ported GTK3-era
  implementation: `GtkEntry` + `GtkMenuButton` history popovers
  replace deprecated `GtkComboBox` for both find and replace text,
  action buttons enable/disable live on input state, replace-all
  reports a proper result message, the window title is applied
  separately so a `%` in a document name can no longer reach a
  printf-style format string, and popovers are cleaned up on close.
- **Spell Check dialog rewritten from scratch** —
  `ap_UnixDialog_Spell`: suggestions use a `GtkListBox` (first
  suggestion pre-selected and mirrored into "Change to"), response
  ids travel as connect data instead of a mutable pending-response
  member (no stale-callback state), and Change / Ignore / Ignore
  All / Add / Close all verified live.
- **Bullets and Numbering (Lists) dialog rewritten from scratch** —
  `ap_UnixDialog_Lists` (~1300 lines): Type/Style/Font use
  `GtkDropDown`s, the customize grid (format, delimiter, decimal,
  start-at, text/label align) syncs through `XAP_GtkSignalBlocker`
  guards without recursive update loops, the preview is a
  `GtkDrawingArea` sized via `gtk_widget_compute_bounds` with a
  size-request fallback, the three apply-mode radios and the Text
  Folding page are preserved, and modeless+modal lifecycles are
  explicit (`destroy()` clears `m_windowMain` before
  `gtk_window_destroy` so focus notifications can't re-enter a
  dying dialog). The old `Current_Dialog` global-callback hack is
  gone (`connectFocusModelessOther` takes a null optional hook).
- **GTK 4.14 `GtkDropDown` model-swap crash worked around** —
  swapping a drop-down's `GListModel` segfaulted inside
  `gtk_drop_down_set_model` (verified against a minimal
  reproducer): a model installed via `gtk_drop_down_new()` gets
  fewer internal references than one installed via `set_model`, so
  the first swap over-unrefs and corrupts the next model — and
  re-setting any previously-attached instance trips stale
  selection/factory state. All drop-downs are now created with a
  NULL model and every model (a fresh `GtkStringList` per swap)
  goes through `set_model` with consistent ref accounting.

### Tables (Word-style creation and context menus)

- **Word-style table grid picker** — the ribbon's Table button now
  opens a popover with a hoverable 10×8 cell grid: hovering paints
  the selected rectangle and a live `N × M` caption, clicking
  inserts a default table instantly (`insertTableGrid` edit method,
  `rows,cols` call data), no dialog round-trip. Under it sit
  "Insert Table…" (full dialog) and "Convert Text to Table". The
  ribbon button itself is unchanged — only its behavior.
- **Modernized Insert Table dialog** — Word's three AutoFit modes
  (AutoFit to contents / AutoFit to window / Fixed column width), a
  live miniature preview that redraws as the spins change, and
  remembered geometry: the last-used rows/columns/mode/width persist
  in the profile (`InsertTableLast*` pref keys) and preload on the
  next open. Reachable from the grid popover, the classic Table menu
  and the right-click menus.
- **Contextual right-click menus** — a new `ContextTable` layout
  (`EV_EMC_TABLE`, `ap_ML_ContextTable.h`) appears when the click
  position is inside a table: Cut/Copy/Paste, Insert Table…, Insert
  rows/columns, Delete rows/columns/table, Select cell/row/column/
  table, Merge/Split cells, Split table, all three AutoFit variants,
  Distribute rows/columns, Convert Table to Text, Format Table and
  View Gridlines — matching Word's structure. Right-click in plain
  text still gets `ContextText`, which gained a Table submenu with
  Insert Table, Convert Text to Table, insert/delete rows & columns,
  merge/split and Format Table. `_getMouseContext` now reports
  `EV_EMC_TABLE` for clicks in cells (also covers the cell-border
  hit-contexts) and `contextText` re-checks the position so stale
  context can't misroute the menu.
- **Word-style popup menu icons** — `EV_UnixMenu::_createMenuItem`
  resolves each popup item's toolbar icon
  (`AP_CreateToolbarLabelSet` + `abi_stock_from_toolbar_id`) and sets
  it as the `GMenuItem` icon attribute, so `GtkPopoverMenu` renders
  an icon+text column.
- **Convert Text to Table** (`textToTable`) — splits the selection
  into rows at paragraph breaks and cells at the delimiter passed as
  call data (`tabs`/`commas`/`spaces`/`all`); with no call data it
  auto-detects (tabs, then commas, then spaces). The selection is
  replaced by a populated table inside a single user-atomic glob, so
  it is one undo step.
- **Markdown-style table autoformat** — typing a `+---+---`-style
  ruler line and pressing Enter replaces it with a real one-row
  table whose column widths are proportional to the dash runs
  (`FV_View::_autoFormatTableOnEnter`, hooked into
  `insertParagraphBreak`; capped at 64 columns / 512 chars, inactive
  during selection, frame or header/footer editing, and inside
  tables).
- **GTK4 context-menu parenting fix** — `runModalContextMenu` no
  longer re-parents an already-parented `GtkPopover` (silences a
  `gtk_widget_set_parent` CRITICAL).
- **Draw Table pencil mode** — a real Word-style pencil: Draw Table
  (Table Layout tab or the Insert ▸ Table popover row) toggles a
  crosshair mode where press+drag shows a live dashed blue
  rubber-band preview, and release either inserts a table sized to
  the dragged rectangle (roughly a column per inch, a row per half
  inch) or — when the stroke starts inside an existing cell — splits
  that cell along the dominant axis (drag mostly sideways → vertical
  split, mostly down → horizontal split). Escape exits the mode.
  Fixes along the way: the preview was painted in layout units as if
  they were device pixels (drawn thousands of pixels off-screen,
  `AP_UnixFrameImpl::_postDocDrawTableRect` now converts via
  `tduD`), the created table's dimensions mixed layout units with
  device DPI (now converted), in-table drags inserted nested tables
  instead of splitting (hit-test now uses `mapXYToPosition` +
  `getTableAtPos` at the drag origin instead of trusting the warped
  insertion point), and the menu item was greyed out when the caret
  was not in a table.
- **Popover menu rows are real click targets** — popover action rows
  (`_popoverEmButton`, `_popoverTbButton`, border-row helpers) now
  have padding and full-width expanding labels instead of shrinking
  to the ~15 px label text, so e.g. the popover's Draw Table row
  clicks reliably across the whole row.
- **Popover restack flood** — `xap_gtk_popover_new`'s delayed raise
  is now a one-shot instead of a repeating `XRaiseWindow` every
  120 ms; under a compositor the flood kept cancelling the surface's
  pending frame and the popover could stay invisible.
- **Window wider than the screen / maximize** — ribbon tab pages now
  live in horizontally-scrolling containers so the window's minimum
  width (~2043 px, driven by the Home tab) no longer exceeds a
  1920 px screen; the window maximizes correctly.
- **Slimmer ribbon buttons on small screens** — large ribbon buttons
  get two width tiers: a ~2/3-width tier (8-char wrapping captions)
  on the Review, Layout, View, References, File, Equation and Help
  tabs plus Home's Paste, and an extra-slim tier (20 px icons,
  7-char captions that can break inside long words) on the Insert
  and Table Layout tabs. The `ribbon-xslim` style now trims button
  padding on all sides and also matches `GtkMenuButton`, which the
  plain `button` selector never reached.

### Ubuntu Launchpad bug fixes

- **LP#921756 / LP#1712097** — `GR_Graphics::tlu()`/`tluD()`/`tduD()`
  SIGABRT: zoom=0 / resolution=0 division guarded (inf→UB on
  float→int cast).
- **LP#1620709** — `std::string::assign` crash in the hyperlink
  dialog: NULL `getHyperlink()`/`getHyperlinkTitle()` guarded.
- **LP#1564143** — `pixbufForByteBuf` crash: unset `GError` no longer
  dereferenced on `gdk_pixbuf_loader_write()` failure.
- **LP#1577612** — IM `retrieve`/`delete` surrounding callbacks: null
  view guard + clamped position.
- **LP#1204037** — `FV_UnixSelectionHandles` ctor SIGABRT: null
  view/frame impl guarded in `_ensureTextHandle`.
- **LP#1628717** — `pf_Fragments`/`repairDoc` crash: deleted fragments
  tracked and skipped; `_removeHdrFtr()` stops at body sections.
- **LP#1248011** — ABW serializer emitted a duplicate `props`
  attribute (unopenable files): merged into the existing attribute.
- **LP#234756** — `LC_PAPER`/`LC_MEASUREMENT` now selects the default
  page size (`nl_langinfo` paper metrics on glibc).
- **LP#1386253** — `AP_TopRuler::mousePress` SIGABRT: signed-overflow
  UB in the ruler code path fixed by audit.
- **LP#995887 / LP#1031137 / LP#941566 / LP#1094243** — black /
  too-bright rulers and dialogs: resolved by the GTK4 Cairo/CSS draw
  path; ruler foreground is explicitly black.
- **LP#1629135** — `gdk_window_ref_cairo_surface`/`_beginPaint` crash:
  resolved by the GTK4 render model.
- **LP#1603245** — `motion_notify_event` cast crash: event
  controllers replaced the signal.
- **LP#1279020** — theme-change crash: styling is CSS-based now.
- **LP#1485796** — cogl/clutter crash: GTK4 does not use cogl/clutter.
- **LP#926419** — invisible typed text on Wayland: resolved by the
  GTK4 draw path.
- **LP#1141885** — `.docx` file association: DOCX is built-in and the
  mimetype is registered.
- **Closed with feature removals** — LP#1711244, LP#673045, LP#673052,
  LP#674721, LP#295596, LP#388971 (collab/goffice components deleted).

### Debian bug fixes

- **#1008160** — libwv-1.2 buffer overflows: vendored `wv-1.2.9`
  carries the bounded-copy patches (see MS Word section).
- **#1018988** — DOCX header/footer tables/images/math dropped or
  flattened: full listener-state stack now used in those parts.
- **#1019109** — math listener swallowed content after failed
  OMML→MathML conversion: unwinds cleanly.
- **#896745** — font size by keyboard: unlisted sizes read from the
  combo entry.
- **#1010880** — stale zoom display: unlisted percentages appended so
  the combo always shows the real zoom.
- **#704629** — corrupted Finnish menu strings fixed
  (Save/Tools/Table/View/Cut/Copy/Paste/Print).
- **#845137** — crash opening files: upstream r33154 revert carried.
- **#740403** — PDF save producing `.abw.saved`: export verified.
- **#485090** — keyboard table-cell resize (Ctrl+Alt+arrows).
- **#568670** — cell-border drag no longer pushes cells off-page.
- **#672151** — side margins only where a border style exists.
- **#363656** — RTF TOC round-trip (`fldrslt` + `PTX_SectionTOC`).
- **#620769 / #620768** — bidi field direction + RTL-mirrored table
  columns.
- **#528679** — permission-denied open errors surface a message
  instead of a silent empty page.
- **#1062453** — dead abisource.com URLs → gitlab.gnome.org.
- **#740635** — ruler disappearance: resolved by the GTK4 redraw path.
- **#572798** — vector-text PDF output with native-resolution images.

### Crash, memory-safety and correctness fixes

- **MHTML import works again** — the rewritten `UT_MHTStream` parser
  was never opened, so every `.mht` file failed to import. The
  importer now opens the stream (rewinding the input and reading it
  defensively), so multipart archives import their HTML body and
  embedded images.
- **MHTML body decoding fixed** — part bodies are now decoded per
  their `Content-Transfer-Encoding` over the raw body bytes instead of
  being re-wrapped line-by-line: raw (`7bit`/`8bit`/`binary`) parts no
  longer lose their `\r\n` line endings (which corrupted embedded
  binaries), quoted-printable no longer reads past the buffer on a
  truncated `=X` escape at end-of-input, and base64 runs to end-of-body
  rather than per line. Parts ending at a delimiter without a blank
  line are handled too.
- **MHTML export produces valid multipart files** — saved `.mht`
  archives now use standard `Name: value` header syntax, put every
  part's `--boundary` delimiter on its own line, end with a proper
  `--boundary--` closing delimiter, and write an English RFC 2822
  `Date` with the real timezone offset. Exported `.mht` files are
  recognized as MHTML again on reimport (previously the Markdown
  sniffer claimed them), and `cid:` image references now match
  `Content-ID` regardless of `<>` brackets, whitespace or %-encoding,
  with a `Content-Location` fallback that accepts absolute-vs-relative
  URLs — so a save/reimport round-trip keeps embedded images.
- **EPUB import hardened** — the importer now percent-decodes and
  normalizes rootfile paths and manifest hrefs (so OPF files at the
  archive root, `%20`-style names and `..`-relative chapters resolve),
  handles namespace-prefixed `container`/`package` documents and
  OPF 2.0/3.0 manifests, prefers the rootfile declared with the
  OEBPS media type, and skips missing manifest ids/files instead of
  aborting or crashing on them. Fixed crashes on missing rootfiles,
  absent manifest attributes and the shared zip-stream read position,
  plus several GsfInput/GsfOutput leaks.
- **EPUB import fidelity** — chapters now keep their images (imported
  as Abi data items with their declared media types), Dublin Core
  metadata (`dc:title`/`creator`/`language`/`date`/`subject`/etc.)
  lands in document properties, and the XHTML importer applies basic
  CSS: `<style>` blocks and `<link rel="stylesheet">` files are parsed
  for element/`.class`/`#id` selectors and combined with inline
  `style=` attributes in specificity order. Splicing chapters into the
  document no longer leaks the previous paragraph's formatting into
  the next one, and a chapter's opening paragraph keeps its own
  style/properties.
- **EPUB export correctness** — exported `.epub` files now pass
  `epubcheck` 5.2.1 cleanly: the OPF 3.0 package document carries
  `xml:lang`, `dcterms:modified` and a `urn:uuid:` identifier that the
  NCX `dtb:uid` matches verbatim, the navigation document is emitted as
  `nav.xhtml` with `properties="nav"` (unique nav ids), and the NCX
  reports the real TOC depth. Split-export filenames no longer derive
  from the internal `.part` scratch file, the first split chapter
  correctly maps to `index.xhtml`, stylesheets ship inside the package,
  and EPUB output emits XHTML5-legal markup (no `img@align`,
  `cellpadding`, or empty `rowspan`/`colspan`) with locale-independent
  CSS dimension formatting.
- **EPUB export fidelity** — chapter splitting is now configurable by
  heading level in the EPUB export options dialog (and via the
  `split-level` export property), the first document image is declared
  as the cover (`properties="cover-image"`, plus the EPUB 2
  `meta name="cover"` convention), and footnote/endnote references
  carry `epub:type="noteref"` links to `epub:type="footnote"` /
  `rearnote` aside sections. Note output in split chapters is no
  longer malformed: inline footnote/endnote sections used to close the
  containing paragraph early, which pushed note markup (and sometimes
  `</body>`) past the end of the document body; multi-paragraph notes
  now export in full.
- **EPUB import reconstructs real footnotes/endnotes** — an
  `<a epub:type="noteref">` pointing at an `epub:type="footnote"` /
  `rearnote` body (as written by Abinova's own export) is imported as a
  real note again instead of a dead "1" hyperlink plus a loose
  paragraph at the end of the chapter. A capture pass indexes each
  note body by id before the main XHTML parse, and the body is then
  replayed inside a real footnote/endnote strux at the reference.
- **Atomic file save** — `IE_Exp::writeFile` now exports to a
  `<name>.part` sibling and `rename()`s it over the target: a
  failed export, encryption failure or mid-write crash can no
  longer destroy the previous version on disk. Existing file
  permissions are preserved, file + directory `fsync`'d, temp
  files cleaned on every failure path; non-local (gvfs) targets
  keep direct writes.
- **Failed save no longer mutates document state** —
  `m_lastSavedAsType` is restored when `writeFile` fails (a
  rejected Save As used to silently switch the format of the next
  plain Save), and the save-dialog password is rolled back so a
  failed save can't change the document's encryption state.
- **Dialogs actually centre on their parent on X11** — the manual
  GTK4 centering ran on "realize" before the window had a size and
  never re-ran, so dialogs landed parent-centre-top-left (right and
  below centre). It now centres on "map" and once more from an idle
  after the size settles; the frame-less `abiRunModalDialog` path
  gets the same hook.
- **Password env var renamed** — `ABIWORD_PASSWORD` is now
  `ABINOVA_PASSWORD` for headless encrypted conversions (both ODF
  and `.abwn`).
- **Split Table crash fixed** — the command deleted the original
  table rows and re-inserted them as a new table, but tracked
  positions via `getPoint()` values that went stale mid-edit; it
  now tracks strux handles so the new table gets a valid layout.
- **Undo after word count no longer crashes** — `countWords`
  iterated page pointers that could belong to a torn-down layout.
- **`isInTable` handles strux-boundary caret positions** —
  whole-cell selections land the caret on table/cell strux
  boundaries; the check now resolves those positions so the
  Table Layout tab (and Merge Cells) stays reachable mid-selection.
- **Cell property writes go through `setCellFormat`** — calling
  `changeStruxFmt` with a properties vector on a cell strux could
  crash relayout; cell props now use the dedicated path.
- **Repeat-header/split-table state balanced** — `table-wait-index`
  was left bumped after the operation; `_restoreCellParams`
  restores it so subsequent table commands behave.
- **Crash diagnostics** — `catchSignals` now dumps a backtrace to
  stderr on fatal signals.
- **Canvas blanking fixed when selecting a transformed object** —
  `GR_CairoGraphics::getCairo()` implicitly calls `beginPaint()` when
  no paint is running; calling it from `draw()`/`drawHandles()`
  during the frame-edit redraw path (which runs outside a paint
  cycle) left the paint/group stack unbalanced and blanked the whole
  canvas on the next paint. Both call sites now only take the cairo
  context when `getPaintCount() > 0`.
- **Frame-layout teardown use-after-free fixed** —
  `~fl_FrameLayout` queries `getDocLayout()->getView()
  ->getFrameEdit()`, but `IE_Exp_Cairo::_writeDocument` (and similar
  teardowns) delete the `FV_View` before the layout — reading
  `FV_FrameEdit::m_pFrameLayout` off the freed view (caught by
  valgrind, surfaced as `munmap_chunk` on exit). `~FV_View` now
  calls `m_pLayout->setView(nullptr)` first, which also clears the
  stale `fp_Page::m_pView` pointers used by `expandDamageRect`.
- **Ribbon startup/teardown fixes** — the style-gallery nav buttons
  could be allocated below their CSS padding when the ribbon was
  squeezed at startup ("attempt to allocate GtkImage with width
  -19"); a `ribbon-nav` class with zero horizontal padding keeps
  the icon's allocation non-negative. `GtkNotebook` also emits
  `switch-page` while being disposed during window teardown —
  `AP_UnixRibbon::refresh()` then ran `getCurrentView()` on a
  frame whose view list was already gone; it is now guarded by
  `gtk_widget_in_destruction()`.
- **Ribbon/help audit fixes** — `cmdParaBorder` pushed `.c_str()`
  pointers of loop-scope `std::string`s into the property vector
  (dangling reads in `changeStruxFmt`, worked only via SSO luck);
  the deferred `gtk_paned_set_position` idle callback held a raw
  `this` (UAF if the frame closed before the idle ran — now a heap
  cell + weak ref on the paned widget); the help window's
  `_runSearch` leaked an anonymous tag per keystroke and rescanned
  ~220 files live (named-tag reuse + ~200 ms debounce); `bodyOnly()`
  now strips every head/script/style block instead of the first;
  copy operations deleted on `XAP_UnixHelpWindow` and
  `AP_UnixStylesPane` (both own resources and connect `this` to
  signals — a copy would double-free).
- **Click/drag selection offset** — clicks and drag-selections landed
  ~7 text rows below the pointer: `gdk_event_get_position()` returns
  *surface*-relative coordinates under GTK4, offset from the drawing
  area by the header bar + ribbon + rulers (~157 px). `EV_UnixMouse`
  now takes the gesture callbacks' widget-relative `x,y` for press,
  release and motion; the scroll controller's position is translated
  with `gtk_widget_compute_point`. `warpInsPtToXY` also refreshes the
  caret coords immediately after `_setPoint`.
- **Vertical scroll jumps at page changes** — the wheel step shrank
  from a fixed 60 px to 36 px per notch; `GDK_SCROLL_SMOOTH` deltas
  (touchpads) now scroll proportionally with fractional-notch
  accumulation instead of collapsing each event to a full step; and
  `vScrollChanged` coalesces pending scroll targets instead of
  dropping them, so rapid fine-grained scrolling no longer loses
  distance or snaps late. Wheel and smooth-scroll moves now also
  **glide**: a `GdkFrameClock` tick eases the view offset toward the
  target (~18 %/frame ease-out) and retargets a running animation on
  each new notch, with an instant fallback when the canvas is not
  realized — no more visible jumps at page transitions.
- **Project URLs and About** — "Check for Updates" and "Report a
  Bug" point at `github.com/janos-szenfner/Abinova` instead of the
  old GNOME GitLab project; the About dialog lists Janos Szenfner
  and links the fork's repository.
- **Same-application clipboard deadlock** — `gdk_clipboard_read_async`
  deadlocked when Abinova itself owned the clipboard (the async read
  calls back into our own `AbiContentProvider` on the main thread and
  wedges on a GLib mutex). `XAP_UnixClipboard::getData`/`getTextData`
  now detect a locally-owned clipboard (`gdk_clipboard_is_local`) and
  read the internal fake clipboard synchronously; the async path is
  only used for foreign owners. Fixed every paste path (Paste, Keep
  Text Only, Paste Special) and made same-app paste faster.
- **Insert → Symbol SIGFPE** — division by a 0×0 drawing-area
  allocation guarded; natural size used before realization.
- **Menubar/toolbar activation crash** — `GMenu` rebuild deferred to
  an idle instead of mutating the live model under an open popover.
- **Menu pointer-motion crash** — models rebuilt and swapped
  atomically instead of mutated under `GtkPopoverMenu`.
- **Replace dialog** — GTK3 stock id rendered literally; real label
  driven from the string set.
- **Stylist dialog** — literal `gtk-ok`/`gtk-apply` → localized
  labels.
- **Border & Shading crash** — stale `.ui` signal handler removed
  (unresolvable handlers are fatal in GTK4).
- **Word Count close killed the app** — auto-update timer stopped via
  `destroy()`.
- **Go To / Page Setup spin crashes** — spin buttons treated as
  `GtkEditable`, not `GtkEntry`.
- **Insert Table / table picker** — popover unparented at dispose.
- **Clip Art use-after-free** — `fill_store` idle cancelled on
  destruction.
- **Key-binding table out-of-bounds** — `EV_EVK_ToNumber` yields up
  to 0xffff but `m_pebChar->m_peb[256][4]` was indexed by it guarded
  only by a `UT_ASSERT` (compiled out in release); any keysym ≥ 256
  was an out-of-bounds read/write. `setBinding`/`removeBinding` now
  apply the same 65280-offset quick fix as `getBinding` and bail on
  out-of-range values.
- **Menu-layout table out-of-bounds** — `EV_Menu_Layout::
  setLayoutItem`/`getLayoutItem` indexed `m_layoutTable` guarded only
  by `UT_ASSERT`; now return `false`/`nullptr` out of range.
- **Table column/row access out-of-bounds** —
  `fp_TableContainer::getNthCol`/`getNthRow` indexed member vectors
  guarded only by `UT_ASSERT`; now return `nullptr` out of range.
- **`GR_Graphics::endDoubleBuffering`/`resumeDrawing` UB** — called
  `std::stack::top()` on a possibly empty stack; now bail early.
- **Tab-position buffer overflow** — `fl_BlockLayout` copied a tab
  position string into `char[32]` guarded only by `UT_ASSERT`; now
  clamped.
- **Signed-shift UB** — `1 << bitdex` for bitdex up to 31 shifted a
  signed int into the sign bit; now `1U << bitdex`.
- **`getLastItem` on empty vector** — `UT_GenericVector::getLastItem`
  indexed `m_pEntries[-1]` when empty (assert-only guard); now
  returns `T()` like `getFirstItem`.
- **`.abw` mime-type misclassification** — `strcmp(*attr,"image/svg")`
  without `== 0` made the SVG branch true for every other mime type,
  so `application/mathml+xml` and the generic embed fallback were
  unreachable.
- **Null `pAP` dereference in ODF export** —
  `ODe_AbiDocListener::_openAnnotation`/`_endAnnotation` and
  `ODe_Main_Listener` dereferenced a `nullptr` `pAP`/`pValue` after
  a failed `getAttrProp`/`getAttribute` when assertions compiled out.
- **Null `pAP` dereference in HTML export** —
  `ie_exp_HTML_Listener` TOC heading-style lookup.
- **Uninitialized members** — `s_LaTeX_Listener` (~20 members),
  `XAP_Dialog_Insert_Symbol`, `SpellChecker`, `XAP_Prefs`,
  `XAP_EncodingManager`, `XAP_UnixDialog_PluginManager` now
  initialize all members in their constructors.
- **`FV_View::m_pParentData` shadowed `AV_View::m_pParentData`** —
  two storage slots initialized to the same value; the duplicate is
  removed and the inherited member used.
- **`genImageFromRectangle` texture cast crash** — `GskRenderer`
  rasterization path with `GDK_IS_TEXTURE` guard (drags, image cache,
  ODF thumbnails).
- **`fillRect` null style-context** — `GTK_IS_STYLE_CONTEXT` critical
  guarded.
- **Ruler startup paint** — repaint queued on `map`/`resize`;
  `m_xScrollOffset` synced from the view at draw time; bands repaint
  on `AV_CHG_WINDOWSIZE`.
- **Invisible rulers (custom-widget draw)** — `drawImmediate` wrapped
  in `beginFrame`/`endFrame` so the backing surface blits to screen.
- **`abi_widget` dispose** — now chains to parent so child widgets
  unparent.
- **GTK3 remnant properties removed from dialogs** — `border-width`
  on `GtkGrid` (Lists ×2, Columns, Font Chooser) and `xpad`/`ypad` on
  `GtkLabel` (Paragraph, Styles, HTML Options) replaced with GTK4
  margin properties.
- **`clicked` on `GtkCheckButton`** — Columns "line between" now
  connects `toggled`; `GTK_BUTTON()` casts on radio check-buttons in
  Lists replaced with `gtk_check_button_*` API.
- **Dialog windows no longer map before parenting** — toplevel
  `visible` removed from 25 `.ui` files (builder mapped the window
  during construction); early `gtk_widget_show`/`set_visible` calls
  removed or moved after `gtk_window_set_transient_for` (Find/Replace,
  Word Count, Spell, Insert Hyperlink, Lists, Font Chooser, Columns,
  Paragraph, Go To, Insert Symbol). The "GtkDialog mapped without a
  transient parent" warnings are gone.
- **`accessible-role` guarded** — `abiSetupModelessDialog` only sets
  the role when none was assigned yet, matching `abiRunModalDialog`.
- **Menu model swap made safe** — a deferred rebuild now replaces the
  bound `GtkPopoverMenuBar`/`GtkPopoverMenu` widget instead of
  mutating its live model while a popover is realized; GTK4's stale
  internal `opened_submenu` pointer no longer emits
  `gtk_widget_get_mapped`/`unset_state_flags` criticals on activation.
- **Preview Cairo double-free** — preview draw callbacks reset the
  graphics object's Cairo to `nullptr` after drawing and the
  destructor no longer destroys a borrowed GTK draw-callback context
  (fixes the `cairo_destroy` assertion when closing dialogs such as
  Bullets & Numbering).
- **Go To teardown criticals** — spin-button pointers cleared before
  window destruction and `updatePosition` guards them; the notebook's
  `switch-page` during teardown no longer signals dead widgets.
- **Keyboard accelerators restored window-wide** — a toplevel
  `GtkEventControllerKey` feeds unhandled keys to the EV keyboard
  layer when focus is on a non-canvas widget, so Ctrl+F/Ctrl+G/etc.
  work regardless of focus (canvas focus still wins, no double
  handling).
- **Fontconfig noise silenced for dev runs** — the bundled font
  directory/config are only registered when they exist on disk.
- **Ruler font color** — detached donor style contexts return white in
  GTK4; `init3dColors` queries the real widget and the ruler forces
  black on its fixed light background.
- **Symbol table sort order** — `viewClassicUI`/`viewRibbonUI` kept in
  alphabetical order in the binary-searched edit-method table (a
  mis-sort crashed startup and broke neighbour lookups).
- **Missing-null guards** — `EV_UnixMenu::_buildItems` skips items
  with no action/label instead of dereferencing null.
- **`UT_StringPtrMap` purge allocator mismatch** — `freeData()`
  (`g_free`) for `g_strdup`'d values; `static_assert` blocks `void*`
  misuse at compile time.
- **vScroll/ZoomUpdate idle-source UAFs** — per-instance flags +
  tracked sources with destroy-notify cleanup.
- **Selection handles** — `FvTextHandle` reimplemented for GTK4 as
  overlay teardrop widgets driven by `GtkGestureDrag`, weak-ref'd.
- **Dialog close handling** — all modeless dialogs migrated to
  `close-request` (`destroy`/`delete-event` removed in GTK4) so
  cleanup runs on X-button close.
- **IM context lifetime** — all `m_imContext` uses guarded;
  `gtk_im_context_set_surrounding_with_selection` replaces the
  deprecated call.
- **`GtkCheckButton` API split** — all `GTK_TOGGLE_BUTTON` casts and
  `clicked` handlers on check buttons/radios converted
  (`toggled` + active-state guards) across ~20 dialogs.
- **`.ui` files** — all 43 converted to GTK4 builder syntax; dead
  properties, removed widget classes, and unresolvable signal
  handlers stripped; buttons restored (orphaned `action_area`
  children); `use-underline` mnemonics added.
- **Transient parents** — `abiRunModalDialog` falls back to the
  last-focused frame so dialogs no longer map parentless.
- **Accessible role** — only applied when unset (immutable in GTK4).
- **Keyboard focus** — document area `focusable` + grab on
  button-press (GTK4 doesn't auto-focus).
- **File dialogs** — double-click/Enter accept via gesture + key
  controllers (`file-activated` removed in GTK4).
- **Combo parent walk** — `gtk_widget_get_ancestor` replaces
  `get_parent` for GTK4's internal layout.
- **`gtk_widget_translate_coordinates` NULL-native** — startup crash
  guarded.
- **Drag-out temp-file symlink risk** — `g_file_open_tmp()` with
  O_EXCL/0600 replaces predictable `/tmp` names.
- **`m_szTmpFile` free** — `g_free` unconditionally (was `delete[]`
  and existence-gated).
- **`restoreRectangle`** — NULL check moved before dereference +
  save-vector bounds checks.
- **Memory-leak sweep** — `gtk_tree_model_get` strings, `GDir`,
  `GtkTreeModel`/`ListStore` refs, `GsfOutputMemory`, builder refs and
  preview resources freed on all paths repo-wide.
- **Bounds/UB sweep** — OOB keyval index, unremapped list-format
  index, `propBuffer[-1]`/`Rtbl[revId-1]` reads, post-scope `pMarker`
  deref, `pBMax`/`pAP`/`pBL` null derefs, past-end `pf_Frag` iterator,
  `end()` deref, unchecked second file open, `throw;` with no active
  exception, unterminated `strncpy`, `||`-for-`&&` dead branches,
  `unsigned*-1` tricks — all fixed repo-wide.
- **Zero definite leaks, zero errors** under valgrind memcheck on the
  DOCX import/export and EPUB import/export conversion paths.
- **Performance** — 7.8 MB / 40 000-paragraph document ↔ DOCX
  round-trips in ~5 s.
- **Insert → Bookmark / modeless-dialog crash** — response and close
  handlers in 8 dialogs (Lists, Replace, Stylist, Mail Merge,
  Merge/Split Cells, Format TOC, Insert Symbol) called
  `abiDestroyWidget` directly, bypassing `destroy()`/`modeless_cleanup()`;
  the dialog stayed registered with a dangling `m_windowMain` and the
  next focus notification cast freed memory. Close paths now run full
  `destroy()` cleanup for registered (modeless) dialogs.
- **`.ui` dialogs had no action buttons** — a plain `<child>` on a
  `GtkDialog` in GTK4 replaces the internal vbox holding the content
  AND action areas, so `gtk_dialog_add_button()` appended to an
  orphaned widget. A central fixup in `newDialogBuilder`/
  `newDialogBuilderFromResource` reparents the `.ui` child into the
  content area and rebuilds the vbox → buttons render again on every
  `.ui`-built dialog (Page Setup, Lists, Format TOC, …).
- **Ribbon mode** — buttons now carry the classic toolbar icons
  (method-name → icon map); the classic menubar and toolbars are
  hidden while the ribbon is active; the menubar's deferred model
  swap preserves widget visibility so it no longer reappears.
- **`GtkAboutDialog` invalid cast** — it is a `GtkWindow`, not a
  `GtkDialog`, in GTK4; presented directly instead of through
  `abiRunModalDialog`.
- **Preferences markup** — `localizeButtonMarkup` now locates the
  label recursively; `<b>Auto Save</b>` no longer renders literally.
- **Print "selected printer (null) could not be found"** — the code
  passed `GTK_PRINT_SETTINGS_PRINTER` (the literal string `"printer"`)
  as a printer name; GTK now picks the default printer when none is
  chosen.
- **Font combo `GtkExpression` use-after-free** —
  `gtk_drop_down_new`/`gtk_string_sorter_new` take ownership
  (`transfer-ownership="full"`); the explicit `gtk_expression_unref`
  left a dangling pointer (assertions + wrong filtering).
- **Open File dialog layout** — a stray `vexpand` inflated the
  file-type row into an ~85 px empty band; folder seeding moved after
  the modal setup so `set_current_folder` no longer cancels an
  in-flight enumeration ("Operation was cancelled" banner).
- **DOCX `w:lang` precedence** — `w:bidi` (complex-script) no longer
  overrides `w:val`; English docs declared with `bidi="ar-SA"` no
  longer import as Arabic.
- **DOCX `w:type` section-break off-by-one** — OOXML `w:type`
  describes how the section the `sectPr` *ends* relates to the
  previous section, but the importer applied it to the *next*
  section. `continuous` sections now flow on the same page — the
  2-page CV that opened as 4 pages now opens correctly.
- **Clip Art** — falls back to the source-tree `user/wp/clipart` when
  the installed `<libdir>/clipart` doesn't exist, so it works from
  the build tree.
- **Insert Symbol showed an empty grid** — the drawing-area draw
  callbacks never ran `drawImmediate` inside a `beginFrame`/`endFrame`
  pair, so painting landed on the backing surface but was never
  composited to screen; symbol clicks also painted without
  invalidating either drawing area. Both callbacks now wrap the paint
  in a frame and a `_queueDraws()` helper invalidates both areas after
  click/key/font/scroll updates.
- **Frame-close use-after-free fixed** — `EV_UnixMenu::_wd::s_onActivate`
  (and `s_onChangeState`) called `refreshMenu()` on the menu after
  `menuEvent()` returned, but actions like File → Close delete the
  frame — and with it the menu object itself. The handlers now check
  `XAP_App::findFrame()` before touching the menu again.
- **`abi_font_combo_dispose` double-free fixed** — `self->filter` was
  handed to `gtk_filter_list_model_new()` and `self->sel` to
  `gtk_list_view_new()`, both `(transfer full)`: the model/listview
  own them, so the extra `g_clear_object` calls unref'd objects that
  had already been finalized by widget teardown. The dispose path now
  silences callbacks via `updating`, disconnects the selection-model
  notify handler first, and only releases the objects the combo
  actually owns.
- **Ribbon teardown hardening** — toolbar-item `ctx->widget` pointers
  are now weak references (auto-null on widget finalize), the ribbon
  destructor detaches ctx-bound signal handlers on still-live widgets,
  and `AP_UnixRibbon::refresh()` bails once the frame is unregistered
  (which happens before the widget tree comes down) — previously a
  `switch-page` emitted mid-teardown walked dead widget pointers.
- **Builtin-styles dangling pointer fixed** —
  `pt_PieceTable::_loadBuiltinStyles` saved the `const char*` from
  `findNearestFont()` and then called `findNearestFont()` again; the
  API returns a pointer into a shared static buffer, so the saved
  family name was overwritten/freed before use (valgrind: 108 invalid
  reads). The family is now copied before the second lookup.
- **ODF exporter uninitialized state fixed** —
  `ODe_Text_Listener`'s second constructor never initialized
  `m_bAfter`, and the delayed master-page/page-break/column-break
  flags were only conditionally assigned yet unconditionally read in
  `_openParagraphDelayed()` (valgrind: conditional jumps on
  uninitialized values). Both constructors fully initialize and each
  paragraph now resets the delayed state.
- **Autosave rewritten end-to-end** — recovery copies are written to
  a central `autosave/` directory under the user config dir instead
  of beside the document (which could never work for unsaved
  documents and left `.bak~` files in user folders); file names are
  `<basename>-<uri-hash>.bak~` / `Untitled-<n>.bak~` so same-named
  documents in different folders no longer collide; writes go through
  a `.part` file renamed into place so a crash mid-write cannot leave
  a truncated backup; a `.info` sidecar records the original URI and
  timestamp; startup scans the directory and opens each leftover
  copy in its own window with the original filename restored and the
  document marked dirty; a summary message box reports how many
  documents were recovered.
- **Stale backups clean themselves up** — a successful regular save
  drops the pending recovery copy at the next autosave tick and a
  clean window close removes it immediately, so the recovery
  directory only ever holds files that represent actual unsaved work.
- **Autosave timer lifecycle fixed** — `setAutoSaveFile` /
  `setAutoSaveFilePeriod` now manage one persistent periodic timer
  (stale timer ids are reaped, disabling stops it, period changes
  restart it) instead of the previous ad-hoc create/cancel code
  (Bug 9329 still honoured).
- **Autosave no longer exports mid-mutation** — the old timer
  callback logged "no backup made" when the piece table was changing
  and then exported anyway; the tick now defers to the next period.
- **Backup filetype pinned to `.abwn`** — the recovery copy no longer
  relies on a hardcoded integer filetype index.
- **Static-analysis sweep (GCC `-fanalyzer`, full tree)** — the whole
  codebase was rebuilt under the GCC static analyzer and every
  high-signal finding triaged and fixed:
  - `XAP_Dialog_Modeless::BuildWindowName` wrote `name[width]` one byte
    past its 100-byte stack buffer when the window title filled the
    buffer (CWE-121 out-of-bounds write).
  - `EV_Toolbar_Label`'s bidi conversion path could leak one buffer on
    a partial allocation failure and read `fbdStr[0]` uninitialized;
    the pair is now `std::unique_ptr`-managed and guarded.
  - `EV_UnixToolbar::refreshToolbar`/`repopulateStyles` dereferenced
    `getNthItem()` results behind `UT_ASSERT` (a no-op in release
    builds) — now real NULL guards.
  - `UT_CRC32::Fill` dead-code cleanup — the `q &&` NULL check implied
    `new` could return NULL and the two word-at-a-time loops could
    never run (n was always 0); removed.
  - `XAP_Log` crashed on startup when the log `fopen` failed —
    `fprintf(NULL)`; constructor and `log()` now guard.
  - `XAP_FakeClipboard::addData` leaked the new `_ClipboardItem` when
    `addItem` failed.
  - `fl_HdrFtrSectionLayout::~` freed each `_PageHdrFtrShadowPair`'s
    shadow but never the pair itself — one heap leak per attached
    page on teardown.
  - `FV_View::insertHeaderFooter` dereferenced `getCurrentPage()`,
    `pDocL` and `pBL` unguarded (pre-layout NULLs).
  - `UT_GenericStringMap::insert` dereferenced `find_slot`'s NULL
    return on an empty table (m_nSlots == 0) — now grows and retries.
  - `px_ChangeHistory` dereferenced `getNthItem` results on three undo
    adjustment paths (out-of-range → NULL).
  - `AP_UnixRuler` gesture handlers dereferenced `dynamic_cast` results
    behind `UT_ASSERT` (three sites).
  - `IE_Exp` dereferenced `getNthItem`/`snifferForFileType` results on
    two paths; `IE_ImpGraphic` and `IE_Imp_TableHelper` on three more.
  - `fp_ShadowContainer`, `fp_AnnotationRun`, `fp_RDFAnchorRun`,
    `fp_EmbedRun` draw paths now early-return when `_getView()` /
    `getView()` is NULL instead of dereferencing it.
  - `fp_VerticalContainer` offset walks now NULL-check
    `getCorrectBrokenTOC`/`getMasterTable` and stop on a NULL container
    instead of calling `pCon->getContainer()` on NULL.
  - `AP_UnixDialog_Styles` guarded `enumStyles` output and per-item
    NULLs; `OXMLi_ListenerState_Image::charData` moved its NULL check
    ahead of the first dereference; `ODe_Table_Listener`,
    `ODi_ElementStack`, `UT_Timer::findTimer`, `XAP_Frame` message-box
    creation, `XAP_ResourceManager::write_xml`, `EV_UnixToolbar`
    widget-vector lookups, `XAP_UnixFrameImpl::_rebuildToolbar`,
    `FV_Selection` cell ranges and `fl_DocLayout` page numbering all
    gained real NULL guards in place of assert-only checks.
  - Verified: 1129 unit tests pass, `make check` green, Valgrind
    reports **0 errors and 0 definite leaks** on the full suite.
- **Piece-table fragment/undo corruption audit** — the fragment
  red-black tree (`pf_Fragments`) and the undo path got a targeted
  hardening pass against the "mystery corruption" bug class:
  - `pf_Fragments::erase()` never cleared `pf_Frag::m_pMyNode` — an
    unlinked fragment kept pointing at a node that was either freed
    or reassigned to the successor's fragment, so `getPos()`
    reported another fragment's position and a second
    `unlinkFrag()` could erase an innocent neighbour's node. The
    back-pointer is now invalidated on erase.
  - `appendFrag()` computed `find(sizeDocument()-1)`, which
    underflows to `find(UINT_MAX)` when the tree holds only
    zero-length fragments — an invalid iterator feeding
    `insertRight` could install a new fragment as the tree root.
    The in-order tail is now used directly.
  - `getFirst()`/`getLast()` used `find(0)`/`find(sizeDocument()-1)`
    — position-based lookups that silently skip zero-length
    fragments. Both now use the true in-order head/tail, with
    `getLast()` stepping back over a trailing EndOfDoc marker so
    callers keep their original "last content fragment" contract.
  - `findFirstFragBeforePos` guards the empty-tree / zero-size
    document cases instead of underflowing position math.
  - The undo span loop cached a `pf_Frag*` across `_deleteSpan()`
    calls that can free it or coalesce its neighbours — a
    use-after-free on the next iteration; positions/iterators are
    now recomputed after each mutation.
  - Undo format-mark paths dereferenced `getPrev()` without a NULL
    check; neighbour-traversal sites in `pt_PT_Append`,
    `pt_PT_DeleteSpan`, `pt_PT_DeleteStrux`, `pt_PT_InsertSpan`
    (including an `else if` reachable when `getPrev()` is NULL) and
    `pt_PT_InsertStrux` (section-frame position math) all gained
    guards.
  - New regression tests in `pf_Fragments.t.cpp` (44 assertions —
    the file previously held only `#if 0`-disabled stubs) cover
    empty-tree invariants, ordering/positions, the zero-length
    underflow, node invalidation on unlink, double unlink no-ops,
    head/tail unlinks and the PTS_Editing EOD transition; all pass
    with zero valgrind errors/leaks.
  - `m_embeddedStrux` paired begin/end note strux pointers but the
    deletion side only erased entries matching `beginNote` — deleting
    an `EndFootnote`/`EndEndnote`/`EndAnnotation` strux (or any note
    strux via `deleteStruxNoUpdate`/`deleteFragNoUpdate`) left a
    dangling pointer later dereferenced by `isInsideFootnote()` and
    `_checkSkipFootnote()`. A new `_removeFromEmbeddedStruxList()`
    helper drops every pair referencing the frag and is called from
    all three deletion paths before unlink+free.
  - Unchecked position lookups fixed: `deleteFmtMark()` dereferenced
    the result of `getMutFragFromPosition()` unconditionally;
    `_checkSkipFootnote()` and three `_realDeleteSpan()`/revision
    sites dereferenced `getFragFromPosition()` results that can be
    NULL; `fl_BlockLayout::getLength()` did the same for the
    preceding fragment; `getBounds()` no longer dereferences a NULL
    tail on an empty fragment tree.
  - `PP_AttrProp::_computeCheckSum()` now copies the bounded 8-byte
    prefix explicitly (`memcpy` + NUL) instead of `strncpy` relying
    on `hashcodeBytesAP`'s internal length clamp.
  - New `pt_PieceTable_embeddedStrux` test builds real footnote and
    endnote regions, deletes each side of the pair via
    `deleteStruxNoUpdate()` and asserts `isInsideFootnote()` reports
    correctly — a UAF regression guard under the suite's valgrind
    checks.
  - `UT_GenericVector` hardening — `getNthItem` rejected only
    over-range indices; a negative index read before the buffer.
    `setNthItem` and `insertItemAt` accepted negative indices into
    `m_pEntries[ndx]` (heap underflow **write**), and
    `deleteNthItem` passed unchecked `n` to `memmove` where
    `n > count` produced a huge byte count. All four now reject
    out-of-range indices in release builds.
  - `fp_Line::getLastVisRun()` read `s_pMapOfRunsV2L[count-1]` with
    `count` only asserted positive — a `count==0` release build
    read before the run map. Now guarded.
  - `FL_DocLayout::AnchoredObjectHelper()` dereferenced a `getNthPage`
    result without checking — out-of-range page index crashed the
    drag/image anchor path. Now returns false.
  - `UT_GrowBuf` (backs every block's text buffer): `del()` trusted
    `position+amount <= size` as an assert — a bad range underflowed
    the `memmove` length into a heap smash. `del()` and `truncate()`
    assigned the unchecked result of `g_try_realloc`, losing the
    buffer on shrink failure; `getPointer()` returned out-of-bounds
    pointers in release builds. All now validated.
  - Removed the dead `pDoc` fetch left in
    `fp_FieldMailMergeRun::calculateValue` by the mail-merge
    removal.
  - Verified: full suite 1186 tests / 0 failures with
    `ABINOVA_TEST_SRC_DIR` set (the earlier 5 `ie_abinova` fixture
    failures were a test-data path issue, not a code regression),
    `-fanalyzer` clean on all touched files and the ten largest
    fmt-layer files.
- **Memory-leak sweep** — fixed several real leaks found by auditing
  allocator/release pairing and running headless conversions under
  valgrind:
  - EPUB import no longer double-initializes documents: the importer
    called `createRawDocument()` on the main document (already set up
    by the file loader) and on each per-chapter scratch document before
    `importFile()` — orphaning a whole piece table with all its
    built-in styles (~100 KB per call) every time an `.epub` was
    opened or pasted.
  - The registered embeddable-manager prototypes (the built-in
    MathML/LaTeX manager) were never deleted; they are now released
    when the application shuts down.
  - `UT_UCS2_mbtowc`/`UT_UCS4_mbtowc` leaked a `GError` per failed
    iconv conversion — one per undecodable byte on bad input.
  - HTML copy-to-buffer leaked `GError`s and could leak the temp
    file on exporter-construction failure; DOCX export leaked a
    `GError` when the zip sink could not be created.
  - The text-rendering font-substitution path dropped a `PangoFont`
    reference for every substituted item.
- **Use-after-free sweep** — fixed several places where the app could
  use a pointer to already-freed objects:
  - Closing a document window while it is still loading no longer
    leaves the one-second "building document" timer firing on a dead
    frame (both the normal app and the embedded AbiWidget).
  - Closing a window mid-drag (text drag, inline-image drag or frame
    drag) now also stops the pending autoscroll timers instead of
    letting them run on the dead view.
  - Deferred dispatches — the ribbon's paste-special insertion, the
    key-repeat coalescing timer, and the style-strip arrow update —
    now verify the view/object is still alive before running.
  - The Insert Table popover's deferred insert no longer touches its
    freed pick state if the popover is destroyed first.
  - Restoring an older document version could read freed history data.
  - Semantic (RDF) metadata refreshes no longer read freed strings,
    and single-xmlid models now apply their computed properties
    instead of discarding and recomputing them forever.
- **Double-free / stale-pointer sweep** — fixed several places where
  released memory could still be touched:
  - Table-of-contents and table layout no longer inspect a just-deleted
    broken-page container when deciding which fragment to delete next.
  - The RTF importer's parser-state stack could attempt to `delete` an
    object that was never heap-allocated (the pending footnote-reference
    state is a member, not a `new`), which would corrupt the heap if a
    malformed document ever left it on the stack; the stack now
    recognizes and skips it.
  - Importing legacy `.doc` documents whose embedded images fail to
    decompress no longer leaks the image buffer; EPUB export no longer
    leaks file handles when a packaged file cannot be opened.
  - Error paths no longer leak file descriptors, `FILE*` handles, or
    GSF stream objects: DOCX export previously leaked ~20 archive
    stream objects per save (and the ODT/HTML copy-to-clipboard paths
    leaked the temporary file descriptor); EPUB export now cleans up
    its staging directory on failure; decompression of embedded
    archives no longer double-closes output files on write errors.
- **Smart-pointer / ownership sweep** — tightened a few places where
  heap allocation or shared ownership was gratuitous:
  - List-numbering objects (`fl_AutoNum`) no longer keep a removed
    parent list alive — the child→parent link is now a `weak_ptr`
    (the document and layout blocks own lists), so a deleted list
    stops being consulted by its children instead of surviving as a
    zombie.
  - The spell-check suggestion API returns a plain `std::vector`
    instead of a heap-allocated `unique_ptr<vector>`, and the
    colour-picker dialog returns `std::optional<UT_RGBColor>` rather
    than a heap-allocated colour.
  - The graphics clip rectangle and the image-run saved clip are
    `std::optional<UT_Rect>` values instead of heap-allocated
    `unique_ptr`s.
- **Null-dereference hardening on file-fed paths** — an audit of the
  `.doc`, RTF and ODF importers closed a set of crashes a corrupt
  document could trigger: `wvWideStrToMB` NULL/short results were
  dereferenced in TOC field parsing (`command + 5`, unchecked
  `strchr`/`strstr` on `\b`/`\\t` switches), bookmark-name lookup
  indexed the STTBF string table without a bounds check, and missing
  ODF attributes (`style:name`, `style:family`, `text:name`,
  `xlink:href`, `meta:name`, `text:note-class`, `text:display`) were
  assigned into `std::string`/`std::map` keys or `strcmp`'d unchecked.
  The ODF element stack gained NULL-safe `getStartTagName()` /
  `getStartTagAttribute()` accessors used at ~35 sites so elements
  nested shallower than expected no longer crash, and
  `ODi_ElementStack` itself no longer dereferences a NULL vector on
  destruction. RTF paste-table strux lookups now check
  `getStruxOfTypeFromPosition` before `getStruxPosition`, missing
  cell props no longer feed `atoi(NULL)`, and several
  `std::optional::value()` sites in layout code check `has_value()`
  first. Malformed `.fodt`/`.odt`/`.rtf`/`.doc` fixtures now convert
  or fail cleanly.
- **Strict-aliasing violations removed** — a sweep for object storage
  accessed through unrelated pointer types closed the remaining real
  cases: clipboard out-parameters no longer write a `void *` through a
  `const unsigned char **`/`const void **` pun (paste path and the
  fake-clipboard wrappers now use properly typed temporaries), widget
  weak pointers in the dialog helper, canvas graphics, ribbon and
  text-handle overlay now use `g_object_weak_ref` with a typed notify
  slot instead of `g_object_add_weak_pointer`'s `gpointer *` pun, the
  `.abwn` crypto `dlsym` table stores function pointers via `memcpy`,
  and the `.doc` parser/importer no longer reads `U8`/`U32` buffers
  through `U16` lvalues (footnote/endnote type codes are decoded
  little-endian portably, which also fixes a latent big-endian bug).
- **Narrowing/truncation audit (TS01)** — a `-Wconversion`/manual sweep
  of importer, layout and crypto code fixed the real narrowing bugs:
  the ODF decrypt path no longer truncates `gsf_off_t` stream sizes to
  `UT_sint32` (and now grows the inflate buffer instead of failing when
  `manifest:size` is absent or wrong), manifest-declared sizes outside
  `UT_uint32` range map to the "unknown" sentinel instead of wrapping,
  DOCX theme `scrgbClr` channels are rounded and clamped so >100% or
  negative values can't wrap to a wrong color, DOCX numbering clones no
  longer truncate list start values above 65535 to 16 bits, RTF font
  indexes outside 0–65535 reject the font-table entry instead of
  wrapping, ODF heading outline levels are clamped to `UT_uint8`, the
  caret blink timestamp moved from `long` to `gint64`, and a column
  distance calculation now squares its operands in `double` so large
  coordinates can't overflow 32-bit multiplication before `sqrt`.
- **Signed/unsigned-mixing audit (TS02)** — a `-Wsign-conversion`/
  manual sweep of importers, layout and the vendored wv parser closed
  the real wrap-to-huge and empty-container cases:
  - **Malformed `.docx` crash class fixed** — OOXML listener code
    dereferenced the element/section/context stacks without checking
    they were non-empty; a crafted part placing a handled tag at (or
    under) the wrong parent — e.g. `<w:pBdr><w:top/></w:pBdr>` with no
    paragraph, or property elements at shallow depth — crashed on
    `vector::back()`/`at(size()-2)` underflow and `stack::top()` on an
    empty stack.  New `OXMLi_contextBack`/`OXMLi_contextParent`/
    `OXMLi_elemTop`/`OXMLi_sectTop` accessors return safe defaults, and
    the orphan `w:pBdr` edge is skipped instead of aborting the import.
  - **`gsf_input_size` error sentinel wrapped huge** — the signed
    `gsf_off_t` result (`-1` on error) was assigned or clamped into
    `size_t`/`UT_uint32` at several sites; in the importer/graphic
    sniffers and the file-dialog preview a `-1` became ~4 GB and would
    have been used as the read count into a 4097-byte stack buffer.
    All sites now clamp through `gsf_off_t` first (XML, XHTML, EPUB
    sniffer, ODF RDF loader, `UT_ByteBuf::insertFromInput`, the legacy
    `.doc` summary-info stream, HTML clipboard read-back, and the three
    `UT_MIN(4096, gsf_input_size())` sniffers).
  - **`.doc` PICF offset check hardened** — `fcPic` (`S32`) is
    explicitly rejected when negative before comparing against the
    unsigned stream size, and the `wvEatOldGraphicHeader` "not found"
    sentinel is now an explicit `(U32)-1`.
  - **Revision color fix** — `fp_Run` never actually read the
    revision's id (`iId` stayed 0), so `iId-1` silently wrapped and
    the per-revision color never applied; the id is now read and the
    subtraction guarded.
  - Mixed-sign comparisons in `fp_FrameContainer`/`fp_MathRun`/
    `ie_imp_MsWord_97` now use consistent `UT_sint32`/`UT_uint32`
    types.  Verified: full build clean, malformed-`.docx` battery
    imports or rejects gracefully (previously crashed), valgrind
    quiet, `src/wp/test` suite PASS, `.doc`/`.rtf`/`.odt`/`.docx`
    corpus converts to PDF.
- **C-style casts modernized tree-wide (TS03)** — ~2,800 C-style
  casts across the `src/` tree were replaced with the correct
  `static_cast`/`reinterpret_cast`/`const_cast` forms, so conversions
  that used to silently accept anything are now checked by the
  compiler (pointer puns, pointer↔integer and byte-buffer aliasing are
  explicit `reinterpret_cast`; numeric/enum/class-hierarchy
  conversions are `static_cast`; `const` is only dropped where the
  code genuinely needs it).  The sweep exposed 13 places where a
  `(gpointer)` cast silently discarded pointee `const` on string
  literals and hash keys (TOC dialog widget data, style-combo names,
  ribbon tab keys, `ut_hash` value cleanup) — those now spell out the
  `const` drop as `const_cast<gpointer>(static_cast<const void
  *>(...))` instead of hiding it.
- **`reinterpret_cast` misuse cleaned up (TS04)** — downcast and
  upcast reinterprets across the class hierarchy (`PD_Document`,
  `GR_PangoRenderInfo`, `fp_TOCContainer`, `fp_FrameContainer`,
  `fl_BlockLayout`, `FV_View`, `PX_ChangeRecord_FmtMark`,
  `PP_Revision`) are now `static_cast` or plain implicit conversions,
  so the compiler verifies the relationship and the correct
  base-subobject adjustment is guaranteed.  Scalar bit-puns
  (`signedLoWord`/`signedHiWord`, RTF `\u` export of high Unicode
  values, the `PangoCoverage` layout probe in `getCoverage`) now use
  `memcpy` or a correctly-typed buffer instead of dereferencing
  storage through an unrelated type, and a `UT_uint32*` reinterpreted
  as `int*` for `gtk_widget_get_size_request` in the Lists dialog no
  longer turns an unset size request (`-1`) into a 4-billion-pixel
  preview.
- **RTF import: crash on stray closing brace fixed** — a `}` popped
  with an empty RTF state stack called `std::stack::top()` on an
  empty container (undefined behavior, crash on import); the pop is
  now guarded and falls back to the existing recovery path.
  A `docx -> rtf -> abinova` round-trip that previously aborted now
  converts cleanly.
- **Untyped-pointer audit (TS05)** — audited every `void*`/`gpointer`
  channel in `src/` for type-identity loss (callback user-data,
  `g_object` data keys, timer instance data, wvWare element props,
  hash maps and `UT_Vector` payloads; no `std::any`/`std::variant`/
  Boost equivalents exist in the tree).  The one real defect found —
  `PD_Document::setDataItemToken` accepted any pointer into a slot
  that is always an owned `g_strdup`'d MIME-type string, leaking the
  old value and letting a wrong-type pointer reach `g_free` or be
  read back as `char*` — is now typed `const char*` and frees the
  previous token.  The view-to-frame link (`AV_View::m_pParentData`,
  `getParentData`, `XAP_App::rememberFocussedFrame`) is now a real
  `XAP_Frame*` instead of `void*`, so passing a non-frame pointer is
  a compile-time error.
- **Virtual-destructor audit (OO01)** — a `-Wdelete-non-virtual-dtor`
  syntax scan of every translation unit plus a scripted
  class-hierarchy audit (1,046 classes, 721 polymorphic) found no
  live delete-through-base-pointer UB, but five polymorphic bases
  lacked a virtual destructor: `XAP_Drawable`, `XAP_CustomWidget`,
  `XAP_UnixCustomWidget`, `XAP_UnixDialog` and `AP_UnixRuler`.  All
  now have one, closing the latent UB hole for the dialog mixin and
  the drawable/ruler hierarchy.  Also fixed a bad `static_cast`
  between unrelated pointer types in `UT_CRC32::GetCrcByte`
  (regression from the TS03 cast sweep, hidden by stale depfiles).
- **Object-slicing audit (OO02)** — a scripted sweep of all 1,221
  classes for derived objects copied into by-value base
  params/returns/containers/members found no exploitable slicing:
  polymorphic classes are handled through pointers everywhere
  (`PP_Revision`, `fp_Run`, listener/importer hierarchies), and the
  one by-value hierarchy (`PD_URI`/`PD_Object`/`PD_Literal` stored in
  RDF lists and multimaps) documents `PD_Literal` slicing as
  intentional because it carries no members and its type survives in
  `m_objectType`.  Two latent spots hardened anyway:
  `PD_RDFModel::contains` no longer slices `PD_Object` down to
  `PD_URI`, and the (currently unused) `PD_URIListCompare` functor
  takes `const PD_URI&` so a future derived argument cannot slice.
- **Virtual-calls-in-ctor/dtor audit (OO03)** — a scripted audit of
  all 1,221 classes (direct calls plus ctor/dtor -> helper -> virtual
  chains) found one real dispatch bug: `PD_RDFMutation_XMLIDLimited`
  had no destructor, so when an XMLID-restricted RDF mutation died
  uncommitted the base `~PD_DocumentRDFMutation` auto-`commit()`
  statically resolved to the base implementation — which sees only
  the wrapper's always-empty attr/props and early-returns — silently
  skipping the delegate commit and the orphan `pkg:idref` link
  cleanup.  The class now commits (or honors `rollback()`) from its
  own destructor while the override still resolves.  No
  pure-virtual-call abort paths exist; all other flagged sites
  dispatch to implementations the calling class itself provides.
- **Rule-of-3/5 copy-semantics audit (OO04)** — swept the tree for
  classes that own raw resources (delete/free/unref in the
  destructor) while relying on implicit copy operations.  Two real
  violations fixed: `ODe_Style_Style` and `ODi_XMLRecorder` declared
  deep-copying `operator=`s but no copy constructor, so the implicit
  copy ctor shallow-copied owned `m_p*Props`/`m_XMLCalls` pointers —
  a guaranteed double-free on any copy-initialisation; both now have
  proper copy constructors.  `ODi_XMLRecorder::operator=` also
  self-assignment-guards and clears existing calls instead of
  appending, and three `ODe_Style_Style` property `operator=`s no
  longer drop members (`ParagraphProps::m_defaultStyle`,
  `ColumnProps::m_RelColumnWidth`, `CellProps::m_backgroundImage`).
  Copy operations are now `= delete`d on the remaining owners where a
  copy is a latent bug: the `FV_ViewDoubleBuffering` scope guard
  (which also left `m_pPainter` uninitialised), grammar-checker
  `PieceOfText`/`Abi_GrammarCheck`/`HunspellWrap`, ODF style stores
  `ODi_Style_Style_Family`/`ODe_Styles`/`ODe_DocumentData`, MHTML
  `UT_Multipart`, SVG `GR_RSVGVectorImage`, and the OXML element base
  `OXML_ObjectWithAttrProp` (covers the whole element hierarchy).

- **Accidental-hiding audit (OO05)** — a tree-wide
  `-Woverloaded-virtual` sweep (~620 translation units) found ~30
  places where a derived method hid a base overload instead of
  overriding it.  Real dispatch bugs fixed:
  `FV_View::notifyListeners` silently dropped the base's
  `pPrivateData` argument; `IE_Imp_RTF::supportsLoadStylesOnly` was
  `const` while the base method is not, so load-styles-only requests
  for RTF documents were ignored; `IE_Imp_XHTML::appendObject` was
  missing the base's third `props` parameter;
  `IE_Exp_OpenDocument::copyToBuffer` only offered the
  `UT_ByteBufPtr` overload so calls through `IE_Exp*` skipped the
  ODT-aware path; and the RDF semantic-item editor's
  `showEditorWindow` hid its base's `const&` virtuals.  The remaining
  sites were resolved with `using` declarations restoring the base
  overload sets (graphics `fillRect`/`importGraphic`, sniffer
  `recognizeContents`, doc-listener insert hooks, `initialize`
  chains, `createSpecialChangeRecord`, RDF `contains`/`add`, and
  friends).  Also repaired a clean-rebuild break where C++ casts had
  landed in the C-only Blowfish header.
- **Override/final audit (OO06)** — a full-tree `-Wsuggest-override`
  sweep found the codebase already `override`-clean except five
  MHTML sniffer methods, which are now marked.  All 37 leaf
  importer/exporter/graphic `*_Sniffer` classes are now declared
  `final` (they are factory-registered leaves never meant to be
  subclassed; `IE_Imp_RDF_Sniffer` itself stays non-final because
  `IE_Imp_RDF_Calendar_Sniffer` derives from it).  Signature drift in
  these classes will now fail the build instead of silently hiding.
- **Data-race audit (CON01)** — audited every shared mutable
  global/static reachable from a non-main context.  The application is
  effectively single-threaded: the only real worker thread is the
  Help ▸ Check for Updates `GTask`, which correctly confines its work
  to a thread-local result delivered back on the main context, and
  every `UT_Worker`/`UT_Timer`/idle path is a GLib main-loop source,
  not a thread.  The genuine shared-context state lives in signal
  handlers: the crash handler's nested-crash counter
  `s_signal_count` was a plain `gint` mutated inside the signal
  handler — now `volatile sig_atomic_t` as POSIX requires — and the
  test harness's `SIGALRM` watchdog could deadlock on the stdio lock
  if the alarm interrupted a `printf`, so it now uses
  async-signal-safe `write()` + `abort()`.  Also fixed a test-build
  break: `ut_uuid.t.cpp` had an invalid `static_cast` across
  unrelated pointer types left over from the cast sweep.
- **Deadlock / lock-ordering audit (CON02)** — audited every locking
  primitive in the tree.  There are no live mutex users anywhere:
  the only `GMutex` lives inside `UT_MutexImpl`, reachable only
  through `UT_Mutex`, which has zero call sites; no `std::mutex`,
  condition variable, atomic, `GAsyncQueue`, `g_once`, file-lock or
  `gdk_threads` path exists in `src/` or `thirdparty/` — so there are
  no multi-mutex paths and no lock ordering to invert.  The one
  worker thread (the update-check `GTask`) shares no mutable state
  and takes no locks, and nothing nests `gtk_main`/`gtk_dialog_run`.
  Fixed the one defect the audit surfaced: `UT_MutexImpl`'s homegrown
  recursive-lock emulation read `mLocker`/`iLockCount` before the
  mutex was held — and the constructor never initialized either —
  replaced with `GRecMutex`, which provides the intended recursive
  semantics directly.
- **Atomicity / memory-ordering audit (CON03)** — audited the tree
  for the atomic-bug classes: there are no atomics of any kind
  (no `std::atomic`, `g_atomic_*`, `__atomic`/`__sync` builtins,
  `GOnce` or `g_once_init` in `src/`, `thirdparty/` or `test/`), so
  no wrong `memory_order` or check-then-act-on-atomic defect can
  exist.  No plain flag is shared with the only real worker thread
  (the update-check `GTask` exchanges a thread-local result through
  `g_task_return_pointer`, and the dialog widgets it touches are
  `g_object_ref`'d before the task starts); the clipboard and
  drag-and-drop `done`/`abandoned` hand-off flags only ever move on
  the same main context, so timeout-vs-completion is sequential, not
  racy.  Manual refcounts (`AD_Document::m_iRefCount`,
  `fp_ContainerObject::m_iRef`, `XAP_Resource::m_ref_count`, the
  `GR_*::s_iInstanceCount` counters) are reached on the main thread
  only.  Signal-context shared state is already POSIX-correct
  (`volatile sig_atomic_t`, from the CON01 pass).  No changes
  needed.
- **Volatile-as-sync audit (CON04)** — swept `src/`,
  `thirdparty/` and `test/` for `volatile` used as a cross-thread
  flag: none exists.  The only real `volatile` uses are the two
  signal-handler flags made `volatile sig_atomic_t` in CON01
  (`s_signal_count` in `AP_UnixApp::catchSignals`, `trap_reached`
  in `ut_unixAssert.cpp`), which is the correct POSIX idiom —
  `std::atomic` is not required there and only
  `std::atomic_flag` is guaranteed signal-safe anyway.  The lone
  worker thread (update-check `GTask`, CON01–03) shares no
  mutable state, so there is nothing to migrate.  No changes
  needed.
- **Thread-lifecycle audit (CON05)** — swept `src/`,
  `thirdparty/` and `test/` for hand-managed threads: there are no
  `std::thread`, `std::jthread`, `std::async`, `pthread_create`,
  `g_thread_*` or `GThreadPool` uses anywhere (not even a
  `<thread>`/`<future>`/`<pthread>` include), so no join/detach or
  stop-token defects can exist; `UT_Worker`'s `CAN_USE_THREAD` mode
  is compiled out and all workers are GLib main-loop sources.  The
  one worker thread — the Help ▸ Check for Updates `GTask` — could
  let a C++ exception (`new`, `std::string` growth/slicing) escape
  its thread function into GLib's C frames, which would
  `terminate()` the process, and an early escape would leave the
  task uncompleted so the dialog would hang on "Checking for
  updates…".  The thread body is now guarded so the task always
  completes — degrading to "Could not check for updates." — and the
  completion callback's string assembly is guarded the same way
  while still releasing the UI refs.
- **Unbounded-copy / unsafe C-string audit (SEC01)** — swept `src/`
  and the bundled `wv` parser for `strcpy`/`strcat`/`sprintf`/
  `memcpy`/`strncpy` fed by untrusted sizes.  The worst findings
  were in list-label generation: a document-controlled
  `start-value` near `INT32_MAX` made `dec2ascii` write `value/26`
  repeated letters past its 30-byte stack buffer and made
  `dec2roman` build a multi-megabyte string that was `sprintf`'d
  into a 100-byte buffer, and the recursive `_getLabelstr`/
  `dec2hebrew` appended into a fixed 100-element label with no
  bound at all.  All label writes are now bounded (new `maxlen`
  parameter + guarded appends, roman numerals capped at 4999, and
  the position+start-value sum clamped to avoid signed overflow).
  Also fixed: a `strcpy` of a tar member name whose 100-byte field
  is not guaranteed NUL-terminated (untgz path), a
  `strcpy(buff+2, str+3)` over-read in the Adobe uniXXXX glyph-name
  decoder, an RTF-export `sprintf` of an 81-byte list delimiter
  into an 80-byte static buffer, several `strncpy` sites that
  could leave their destination unterminated (transparent-color
  strings, menu label-set language, columns-dialog units), and an
  out-of-bounds `p[len-1]` read when `ABINOVA_DATADIR` is empty or
  a lone quote.  A dozen further `sprintf`/`strcpy`/`strcat` calls
  on fixed buffers were converted to bounded `snprintf` as
  defensive hardening.
- **Format-string audit (SEC02)** — localized strings, GTK `.ui`
  markup templates, menu labels and document-derived strings were
  being passed to `printf`-family calls as the format argument, so a
  malformed translation or crafted input could be interpreted as
  arbitrary `%` directives.  Added
  `UT_checkedPrintfArgCount()` (`src/af/util/xp/ut_string.cpp`), a
  small parser that counts a format's argument requirements while
  rejecting `%n`, `*`-width/precision, unsupported conversions and
  malformed positional syntax.  All non-literal format sites now
  require an exact match before substituting and otherwise print the
  template verbatim — print status, print progress, ruler status
  messages, mark-revisions labels, spell-dictionary errors, menu
  computed labels, page-setup markup and the field page-reference
  message.  Also NUL-terminated `strncpy` destinations that
  `%s`-family calls could later read past (insert-bookmark dialog,
  `wv` style-name copies from `.doc` files).  New unit tests in
  `ut_string.t.cpp` cover the parser.
- **Injection / path-traversal / unsafe-I/O audit (SEC03)** — a
  crafted document could steer exports off the intended directory:
  data-item ids and structure ids embedded in `.abw`/`.abwn` files
  flowed verbatim into HTML/EPUB/LaTeX output filenames, ODT
  `Pictures/` members and `manifest.xml` paths, DOCX `word/media/`
  members and `header<id>.xml`/`footer<id>.xml` names, and MHTML
  `Content-Location:` headers — so a name like `../../hostile` could
  write outside the export directory and a name containing quotes or
  CR/LF could break out of an XML attribute or inject MIME headers.
  Added `UT_sanitizeFileName()` (`src/af/util/xp/ut_path.cpp`), a
  shared allowlist mapping document-controlled names to flat,
  separator/quote/control-character-free base names, applied at every
  export sink.  Also replaced three predictable names in the shared
  temp dir with `g_file_open_tmp()` exclusive creation
  (`UT_createTmpFile`, screenshot capture, online-picture download) —
  closing symlink-attack windows — and switched the screenshot
  `gnome-screenshot` invocation from a `g_strdup_printf` command
  string to argv-form `g_spawn_sync`, removing shell-quoting risk.
  No `system()`/`popen()`/`exec*()` calls exist in the tree; the EPUB
  importer and tar extractor were already hardened (E01).
- **Weak-PRNG / crypto audit (SEC04)** — ODF encrypted export
  generated its per-stream Blowfish salt and IV from
  `g_random_int_range()` (non-cryptographic Mersenne Twister) whenever
  `getrandom()` was unavailable — i.e. always, on non-Linux builds —
  silently producing predictable encryption parameters.  The entropy
  helper now tries `getrandom()` then `/dev/urandom` and fails the
  export cleanly when no OS entropy source exists rather than writing
  weakly-protected ciphertext.  Also fixed two latent gcrypt-build
  bugs in the same file (a decrypted-buffer leak on the error path and
  `gcry_cipher_close` on a possibly-invalid handle).  The rest of the
  audit was clean: `.abwn` encryption uses PBKDF2-SHA256 (600k rounds)
  + AES-256-GCM via OpenSSL EVP with a hard-fail entropy source, and
  all `UT_rand`/`rand()` users are non-security document IDs.
- **Sensitive-data lifetime hardening (SEC05)** — passwords and
  derived key material are now wiped before their memory is released,
  using compiler-proof zeroing (`UT_secureZero`/`UT_secureClearString`
  in `ut_misc`, `wvSecureClear` in `wv`, `memwipe` in the ODF crypto
  helpers — a plain `memset` at end of life can be deleted by the
  optimizer as a dead store).  Covered: `.doc` decrypt keeps no
  cleartext password in `wvParseStruct` past key derivation, per-block
  RC4 keys, MD5 contexts and XOR arrays are wiped on every exit, and
  decrypted whole-stream buffers (including the `wvDecrypt95` gsf
  buffer) are wiped before free; ODF import/export wipes the SHA-1
  password hash and PBKDF2 key, the decrypted-content buffer is wiped
  on every error path, and the inflate grow path no longer `realloc`s
  a plaintext buffer (a moved block would leave a plaintext copy in
  the freed allocation); `.abwn` decrypt wipes the plaintext vector
  before `shrink_to_fit` and on a wrong-password tag failure; the
  document's saved password (`PD_Document::m_savePassword`), the ODF
  importer's password member, and the save-dialog's encryption
  password are wiped on overwrite/destruction, and every
  `UT_UTF8String` password (import dialogs, `ABINOVA_PASSWORD`
  copies) is wiped when its buffer is freed.  Debug traces that
  printed password-derived values in the Word-95 key check were also
  removed.
- **XXE / external-resource hardening (SEC06)** — the shared libxml2
  readers ran with `XML_PARSE_NOENT` (entity substitution on) and no
  network/load restriction, so a crafted XML document could declare
  `SYSTEM` entities pointing at `file://` paths or remote URLs and
  have libxml2 read local files or fetch network resources while
  opening the document.  A new scoped guard
  (`UT_XML_UntrustedParseScope` in `ut_xml.h`) installs a process-wide
  libxml2 external-entity loader that refuses all loads only while an
  untrusted-document parse is active — scoped rather than permanent
  because trusted local resources (e.g. the bundled XSLT stylesheets
  libxslt loads through the same machinery) must keep working.
  Applied to `UT_XML::parse` (both entry points), `UT_HTML::parse`,
  the three document-controlled `xmlParseDoc` calls in the
  MathML/OMML converters, and the `xmlReadMemory` MathML typesetter
  parse; both push parsers also gained `XML_PARSE_NONET`.  Internal
  entities and predefined entities keep expanding as before.  The
  sweep also found that XHTML, Markdown and LaTeX importers resolved
  document-controlled image/stylesheet URIs through
  `UT_go_file_open`, which can fetch `http://`/`https://` and other
  remote schemes — a silent network request (SSRF/tracking-pixel risk)
  on document open.  New `UT_go_url_is_local()` now gates those
  references: plain paths and `file://` still resolve, remote schemes
  are skipped (the user-invoked "Insert Online Picture" feature is
  unaffected).  ODF embedded images (package-internal) and OOXML
  external relationships (already rejected) needed no change.
- **Exception-safety audit of destructors (EX01)** — C++ destructors
  are implicitly `noexcept`, so any allocation or throwing call that
  escapes one is an immediate `std::terminate` (crash on close,
  autosave cleanup, or error-path teardown).  A full sweep of all 378
  destructor definitions found and fixed every reachable throwing
  path: the RDF mutation destructors called `commit()` (which
  allocates attribute properties, SPARQL strings and librdf objects),
  `~XAP_App` ran the user-dictionary `save()` (hash enumeration +
  string buffers), `~fl_BlockLayout` could reach TOC relabeling
  (`std::stack` push), `~FL_SelectionPreserver` re-selected via view
  ops, and `~FV_ViewDoubleBuffering` drew on teardown — all now
  log-and-drop instead of terminating.  `~FL_DocLayout` deduplicated
  embed managers through an allocating `std::set`; it now walks the
  two manager maps allocation-free.  `~SpellManager` no longer builds
  an intermediate vector to delete its checkers.  `~XAP_Frame` builds
  the autosave `.info` name with glib instead of `std::string`
  concatenation.  There are no user-declared move operations in the
  tree, so the missing-`noexcept` leg was vacuous.
- **Catch-by-value audit (EX02)** — inventoried every `catch` clause
  in `src/`, `thirdparty/` and `test/` (114 sites): `src/` has zero
  typed catches — all 80 handlers are `catch(...)`, and the only
  typed-catch machinery (`UT_CATCH(x)` → `catch(x)`) has no call
  sites.  The 30 by-value catches that do exist are in vendored
  libwpd/libwps and catch empty standalone exception structs
  (`class FileException {}` — no base class, no members, no
  virtuals), so nothing can be sliced; they were left untouched to
  avoid upstream diff churn.  No changes needed.
- **Swallowed-exception audit (EX03)** — classified every `catch`
  body in `src/` (30 sites) and `thirdparty/` (83 sites): nearly all
  are deliberate `bad_alloc`→`nullptr` translations whose fallback is
  checked downstream, and the destructor/`noexcept` guards added in
  EX01 already log.  Three sites hid real failures with no trace and
  now emit `UT_DEBUGMSG`: `AP_App::saveRecoveryFiles` ran inside the
  `SIGSEGV` handler and a throwing `backup()` silently skipped that
  frame's recovery file; the update-check completion callback in
  `xap_UnixAppImpl` swallowed exceptions untraced; and
  `OXML_Element_Text::setText` could drop a text run from DOCX export
  without a word.  Behaviour is unchanged — the fixes add logging
  only.

### GTK4 port (core migration)

- **Frontend ported to GTK4** — event controllers replace event
  signals; `gtk_drawing_area_set_draw_func`; `GdkClipboard` + lazy
  `GdkContentProvider`; `GMenu`/`GSimpleAction`/`GtkPopoverMenuBar`;
  `GtkPopover` replaces popup windows + seat grabs; embedded
  `GtkFileChooserWidget`; `GtkCheckButton` groups replace
  `GtkRadioButton`; `AbiWidget` subclasses `GtkWidget` directly;
  nested-`GMainLoop` shim replaces `gtk_dialog_run`;
  `GtkColorChooserWidget` replaces goffice `GOComboColor`.
- **Persistent backing-surface rendering** — the canvas draws into a
  real image surface emulating the old window framebuffer; the draw
  callback blits it (recording surfaces can't be scraped).
- **Double-buffering machinery disabled on GTK4** — paints outside
  the draw callback are invisible anyway; leaked cairo groups gone.
- **UI event pump restored during load/print** — `_nullUpdate` drives
  `GMainContext` (bounded per call); large files no longer freeze the
  app solid while laying out.
- **Deprecated APIs replaced** — `g_action_map_add_action` for
  `g_simple_action_group_insert`; `gtk_widget_get_color` (4.10+) for
  style-context color queries.
- **GTK2-era `--enable-menubutton` dead code removed** — it used APIs
  that cannot compile under GTK4.
- **Print preview rewritten from scratch** — the GTK4 dialog
  (`xap_UnixDlg_PrintPreview`) renders each page once into a cairo
  recording surface (vector ops replay losslessly at any zoom), shows
  a scrolled page strip with drop shadows, a header bar with page
  navigation (previous/next arrows, spin entry, "/ N" label), zoom
  out/in buttons and a preset popover (50–400 %), Fit Width / Whole
  Page toggles, a Print button that hands off to the normal GTK print
  dialog, and Close/Escape to dismiss. The initial fit is deferred
  until the drawing area has a real allocation (it used to compute
  ~17 % zoom against a zero-size window), paper is painted white
  beneath the recorded print content (print graphics intentionally
  skip the paper fill), and Win32 `&` mnemonics are stripped from
  GTK labels. Menu and toolbar Print Preview actions now invoke this
  in-app dialog instead of the external viewer path.
- **Clipboard subsystem hardened** (`xap_UnixClipboard`) — the
  `GdkContentProvider` no longer holds a raw owner pointer that could
  outlive the clipboard object (providers are weak-ref'd and disowned
  at teardown); synchronous clipboard reads run a nested main loop
  bounded by a 5 s timeout + `GCancellable` so a dead peer can't
  freeze the editor, and abandoned reads free their context from the
  late callback; stream reads are capped at 64 MiB to stop hostile
  payloads exhausting memory; `canPaste()` now checks real clipboard
  formats instead of always returning true; foreign clipboard text
  is served from an owned buffer and never contaminates the local
  fake clipboard; the fake clipboard owns copies of its format names
  (previously raw `const char*` that could dangle); local clipboard
  reads hit the fake clipboard directly instead of round-tripping
  through our own async provider (which could deadlock).
- **Drag-and-drop hardened** (`xap_UnixFrameImpl`) — drop reads are
  bounded (64 MiB) and time out after 10 s with a `GCancellable`;
  `text/uri-list` payloads are NUL-terminated before
  `g_uri_list_extract_uris` (out-of-bounds read risk); dropped URIs
  are scheme-checked — only `file:` URIs reach the local
  document/image loaders, remote or hostile schemes are refused;
  `UT_go_get_mime_type` NULL results are handled instead of
  dereferenced; `s_loadImage`/`s_pasteText`/`s_pasteFile` guard
  against missing views so a drop on a not-yet-initialized frame
  can't crash.
- **Copying an untitled document no longer crashes** — the HTML
  clipboard exporter dereferenced NULL basenames from
  `UT_go_basename_from_uri()`/`getFileName()` in
  `IE_Exp_HTML_NavigationHelper`, `IE_Exp_HTML::_createChapter`,
  `IE_Exp_HTML::_writeDocument` and `IE_Exp_HTML_DataExporter`;
  untitled documents now export with an "untitled" fallback name.
- **`GR_CairoPrintGraphics` hardened** — `setResolutionRatio` rejects
  non-finite/non-positive ratios that would poison every font-size
  conversion; `startPrint`/`startPage`/`endPrint` NULL-check the
  cairo context and `endPrint` resets the show-page flag (also fixed
  a duplicated `GR_CairoPrintGraphics::` qualified name).
- **Last `GtkTreeView` dialogs ported to GTK4 list widgets** — the
  Insert Hyperlink bookmark list is now a `GtkListView` over a
  `GtkStringList`; the Revisions list is a `GtkColumnView` over a
  `GListStore` of row objects with clickable column-header sorting
  (still opening on newest-first by date) and double-click/Enter to
  accept; the Stylist is a `GtkListView` over a `GtkTreeListModel`
  with `GtkTreeExpander` rows — style categories still expand and
  can't be selected, double-click applies the style, and the current
  style auto-expands and scrolls into view.
- **Insert-table picker selection visible again** — the deprecated
  `gtk_style_context_get_color`/`get_background_color` lookups were
  replaced with `gtk_widget_get_color`, and the toolbar table-size
  grid no longer forces `GTK_STATE_FLAG_SELECTED` onto its own context
  mid-draw (a no-op in GTK4, which left selected cells invisible); it
  now renders cells through `.view` style donors, so the chosen rows ×
  columns highlight in the theme accent and the unselected grid shows
  again too.

### Performance

- **Font selector** — lazy `GtkListItemFactory` + incremental sort:
  popup is instant (was ~2.7 s+ blocked measuring ~2000 fonts).
- **Insert Symbol dialog opens ~10× faster** — the point-size search
  in `XAP_Draw_Symbol::setFontToGC` rescanned the font's whole glyph
  coverage (up to ~1M codepoints for big fonts) on every binary-search
  step and once more for the preview pane.  Coverage is
  size-independent, so it is now collected once per font.
- **Large-file load** — event pump during layout keeps the UI live on
  multi-MB documents.
- **`s_getDragInfo`** — static init flag; the MIME table no longer
  accumulates duplicates per call.
- **Image drag-out** — duplicated ~60-line block extracted to a shared
  helper.

### Fonts

- **Carlito is the default document font** — property table, `Normal`
  style, view default, all 63 `normal.awt-*` templates.
- **99 bundled fonts** under `fonts/` installed with the app and
  registered via `FcConfigAppFontAddDir`: Carlito (Calibri metric),
  Caladea (Cambria metric), Intos family (Aptos metric, from
  muglug/intos), Liberation set, DejaVu, OpenSymbol, Gentium,
  Gentium Book, Noto Sans/Serif, Source Sans 3 / Serif 4 / Code Pro,
  Linux Libertine / Biolinum — mirroring the LibreOffice/OpenOffice
  bundled collection with per-family licenses.
- **Font substitution config** — `fonts/abiword-fonts.conf` maps
  Calibri→Carlito, Cambria→Caladea, Aptos→Intos, Times New
  Roman→Liberation Serif, Arial→Liberation Sans, Courier
  New→Liberation Mono, etc.
- **Bundled fonts actually install** — stale `ABIWORD_DATADIR`/
  `ABIWORD_ICONDIR`/`ABIWORD_SERIES` variables left over from the
  rename meant `make install` dropped `fonts/` (and help, artwork,
  templates, mime info) into a literal `@ABIWORD_DATADIR@` directory,
  so the fontconfig substitution rules only ever loaded from the
  build tree.  All datadir references now use the `ABINOVA_*`
  substituted names and `Calibri Light`→Carlito etc. resolve after a
  real install too.

### Build system and repository cleanup

- **Plugins removed from the build** — `openxml`, `epub`, `grammar`,
  `mht`, `wmf`, `wordperfect`, `wpg` moved into `src/wp/impexp/` and
  `rsvg` deleted outright; regenerated `m4/plugin-list.m4` /
  `plugin-configure.m4` / `plugin-builtin.m4` / `plugin-makefiles.m4`;
  the loadable plugin list is now empty.
- **33 obsolete plugins removed** — applix, bmp, clarisworks, command,
  docbook, eml, garble, gimp, google, hancom, hrtext, iscii, kword,
  latex, loadbindings, mif, mswrite, openwriter, opml, paint,
  passepartout, pdb, pdf, presentation, s5, sdw, t602, testharness,
  urldict, wikipedia, wml, xslfo.
- **8 dead/GTK3-locked plugins removed** — aiksaurus, collab, gdict,
  goffice, mathview, ots, gda, psion (+ `HAVE_GO_MATH_EDITOR` paths,
  `--with-goffice`).
- **Dead platform backends removed** — Cocoa, Win32, Qt (~110 k
  lines); the tree carries a single GTK toolkit.
- **Minimum dependency versions raised to a known-good set** —
  GTK4 ≥ 4.14.5 (`GDK_VERSION_4_14` encode follows), GLib/GIO ≥
  2.80.0, Pango ≥ 1.52.1, cairo ≥ 1.18.0, librsvg ≥ 2.58.0,
  libgsf ≥ 1.14.51, FriBidi ≥ 1.0.13, libxslt ≥ 1.1.39,
  zlib ≥ 1.3, enchant-2 ≥ 2.3.3, Boost ≥ 1.83 — all match the
  versions the tree is developed and tested against.
- **Sandboxed newer-GTK development toolchain** —
  `tools/build-gtk-prefix.sh` builds GTK4 (default 4.22, the latest
  stable series) plus only the deps the distro is too old for (pango,
  harfbuzz, libepoxy, libdrm, wayland; glib overridable via
  `GLIB_MINVER`/`GLIB_REF`) into
  `~/.local/abinova-gtk-dev`, fetching standalone meson/ninja into
  the prefix so no system packages are needed; a generated `env.sh`
  switches `PKG_CONFIG_PATH`/`LD_LIBRARY_PATH` so a worktree build
  uses the new GTK while normal builds stay on the system GTK.
  Verified end-to-end: configure + full build + run on GTK 4.22.5.
- **Fixed: vendored `wv` config.h was untracked** — the hand-written
  `thirdparty/wv-1.2.9/config.h` was matched by the `config.h` rule
  in `.gitignore`, so fresh clones/worktrees failed compiling
  `wvConfig.c` (`xmlparse.h` not found); added a `!` negation and
  tracked the file.
- **macOS and Windows build support via GTK's native backends** —
  `tools/build-macos.sh` (Homebrew + GTK/Quartz) and
  `tools/build-windows-msys2.sh` (MSYS2 MINGW64/UCRT64 + GTK/Win32)
  install dependencies, run configure and build; configure.ac now
  detects the host and drops the `x11`/`gtk4-unix-print`
  pkg-config requirements off X11 platforms, the remaining Xlib
  calls are guarded by `GDK_WINDOWING_X11`/`HAVE_SIGACTION`/
  `HAVE_EXECINFO_H`, and `XParseGeometry` was replaced by a portable
  parser (`s_parseGeometry`) so `--geometry` still works everywhere.
- **Dead preprocessor branches resolved** — `TOOLKIT_*`,
  `XP_TARGET_*`, `XP_MAC`, constant `XAP_DONTUSE_XOR`; OS/compiler
  macros kept for future GTK4 ports to Windows/macOS.
- **`Old-Doc/` removed** — the folder holding historical pre-experiment
  documentation (old README/NEWS/ChangeLog/INSTALL/AUTHORS, design
  notes) deleted along with its `EXTRA_DIST` entries.
- **Credits feature removed** — ribbon Help button, classic Help menu
  item, `helpCredits` edit method, menu id, label/status strings,
  stock/toolbar icons, gresource alias, build refs, `credits.html`
  pages and nav links in all help locales.
- **`flatpak/` removed** — the stale upstream Flatpak manifest
  (old `com.abisource.AbiWord` app id, `abiword` command, patches
  for dependencies of long-deleted plugins) was deleted; it was
  not referenced by the build.
- **Classic menubar removed — ribbon is the only UI** — the
  `EV_UnixMenuBar` widget and the Help → Interface
  "Classic Menus"/"Ribbon" toggle were removed along with the
  `viewClassicUI`/`viewRibbonUI` edit methods, the `RibbonUI`
  preference, and the `HELP_UI*` menu ids/strings. `EV_UnixMenu`
  remains as the action/model container that backs the ribbon's
  `menu.*` GActions and the right-click context menus.
- **Plugin system removed entirely** — `xap_Module`,
  `xap_ModuleManager`, `xap_UnixModule`, the plugin-manager dialog
  (xp + gtk + `.ui`), `TOOLS_PLUGINS` menu/action/edit method, the
  `AutoLoadPlugins` preference and Options checkbox, `plugins/` and
  `src/plugins/` trees, `m4/plugin-list.m4` and the generated
  `plugin-*.m4` machinery, plus every configure/Makefile plugin
  hook. All former functionality is compiled into `libabinova`.
- **UI localization removed — English only** — `po/` (all `.po`
  catalogs and generated `.strings` files), `AP_DiskStringSet`,
  `XAP_DiskStringSet`, `loadStringsFromDisk`,
  `UT_getFallBackStringSetLocale`, the `StringSet`/
  `StringSetDirectory`/`UseEnvLocale` preferences, the Options
  language picker, the `--dumpstrings` debug flag, and localized
  desktop/metainfo entries. The UI always uses the built-in
  English string set; document-language features are unaffected.
- **French and Polish help removed** — the `help/fr-FR/` and
  `help/pl-PL/` documentation trees, the help-window language
  picker, and the `howtotranslation.html` guide (the localization
  system it documents is gone). Remaining en-US help no longer
  references the Plugin Manager dialog, the classic menubar or
  upstream `abisource.com` links (BugZilla/mailing list/CVS
  instructions replaced by the project repository).
- **`tools/` cleanup — Perl replaced by Python, dead scripts
  removed** — `cdump.pl` (the build-time image→C-array generator
  used for `ap_wp_sidebar.cpp`) and `generate-rtf.pl` (RTF keyword
  table generator) were replaced by `cdump.py` and
  `generate-rtf.py`; the Python versions produce byte-identical
  output and the RTF generator now emits the `bsearch`ed keyword
  table in strict `strcmp` order instead of trusting input-file
  order. `rtf-keywords.txt` was resynced with the shipped headers
  (15 missing keywords added — `abiembed`, `abilatexdata`, `rdf*`,
  `svgblip`, `brdrnone`, `deltamoveid`, `fillColor`, `ftech` and
  more — plus ordering fixes). Dead scripts removed:
  `generate_changelog.php` (immediately `die()`d — SVN-era) and
  `generate_changelog.py` (unused release-tag tooling).
  `release.sh` was rewritten for Abinova — version read from
  `configure.ac`, `abinova-*` tarballs, current branch, optional
  `SKIP_DISTCHECK`/`NO_GPG`. `clang-fmt.py` retained.
- **`user/wp/` data reduced** — `readme.txt`/`readme.abw` (stale
  upstream release notes, unreferenced by any code) removed, and
  the 72 per-locale `system.profile-*` files collapsed into a
  single `system.profile`: the locale-dependent defaults are now
  derived in `AP_Prefs::overlaySystemPrefs()` (ruler inches for
  en-US, cm elsewhere; RTL default for ar/dv/fa/he/ps/syr/ur/yi;
  smart quotes off under KOI8 encodings), while `system.profile`
  remains the administrator-override hook applied last. The
  `DefaultPageSize`/`DocumentLocale` attributes formerly carried
  by the eu/ro profiles had no corresponding preference keys and
  were dead weight.
- **Root directory audit** — removed `Doxyfile` (stale upstream
  Doxygen config, no docs pipeline), `dumpstrings.pl` (served the
  removed `--dumpstrings` flag), root `abiword.png` (unreferenced —
  the bundled icons under `icons/` are used instead) and the stray
  tracked test artifact `Untitled1.saved`. `AUTHORS.md` rewritten
  (it pointed at the deleted `AUTHORS.old` and listed the upstream
  maintainer). Desktop entry rebuilt: MimeType list now matches the
  actually-registered importers (added `x-abinova`, DOCX, EPUB,
  Markdown, LaTeX, MHT, WordPerfect, MS Works; removed dead-plugin
  formats). AppStream metainfo fixed: GitHub URLs, modern
  `<developer id=…>` tag, gettext tag and upstream 3.x release
  history dropped — `appstreamcli validate` passes. `abinova.keys`
  gained the `application/x-abinova` association.
- **Application ID renamed to `io.github.janos_szenfner.Abinova`**
  — the GApplication id (single-instance/D-Bus name), the
  gresource prefix `/com/abisource/Abinova` →
  `/io/github/janos_szenfner/Abinova`, the desktop file →
  `io.github.janos_szenfner.Abinova.desktop` (`Icon=abinova`), the
  metainfo file + `<id>`/`<launchable>`, and the icon-theme name
  (`abiword` → `abinova`, icon files renamed accordingly).
  `AC_INIT`'s bug-report URL now points at the GitHub repository.
  Internal `abiword-*` stock-icon identifiers and RDF/format
  identifiers are unchanged.
- **`.abwn` document header updated** — the informational comment
  now points at `https://github.com/janos-szenfner/Abinova` and
  names Abinova as the generator (the AWML doctype, namespaces and
  `abiword.*`/`dc.format` metadata keys are format identifiers and
  remain for compatibility).
- **Dead files removed** — `gr_UnixCairoImage`, `ut_PerlBindings`,
  `ut_stack`, dialog stubs, duplicate `ODc_Crypto`,
  `ie_exp_WordPerfect`, orphaned test fragments, `linkgrammarwrap`
  (unused since the hunspell switch), `--enable-menubutton` code.
- **Vendored third-party libraries** — `librevenge 0.0.6`,
  `libwpd 0.10.3`, `libwpg 0.3.4`, `libwps 0.4.14` (upgraded from
  0.4.11), `wv-1.2.9`, `hunspell-1.7.4` (upgraded from 1.7.0 —
  faster ICONV tries, lower dictionary memory use, XDG dictionary
  dirs, compound/affix/morphology fixes and a word-acceptance trace
  API) — all built as noinst convenience libs; no external downloads
  needed. The 43 MB `link-grammar-5.12.5` tree was dropped.
- **Build hardening** — `autoreconf` fixed for modern autoconf;
  vendored `AX_*` macros; GTK-only configure; `omml_xslt` install dir
  moved to `ABIWORD_DATADIR`; build-tree libtool in the test wrapper.
- **Historical documentation removed** (formerly `Old-Doc/`); `README.md`
  (feature overview + per-commit modification log) and this
  `CHANGELOG.md` (categorized changelog) carry the experimental
  no-warranty notice.
- **README restructured** — table of contents added; the
  NotebookBar ribbon description reorganized into a tab-by-tab
  section (File/Home/Insert/References/Layout/Review/View/Help +
  contextual Table/Equation tabs); stale claims fixed (remaining
  plugins, Mermaid rendering, `plugins/` layout row).
- **`.abw` format documented** — new README section covers the
  AWML document structure, content model, `props` syntax, all
  format extensions in this fork (`fileformat="1.2"` nested
  anchors, `text-*` effects, `frame-*` arrangement state, math
  `display`/`latexid`, `section-break`, `toc-level`,
  `annotation-resolved`) and an honest ODF-coverage comparison.
- **Classic menubar/embedded layouts deleted** — the ribbon is the
  only chrome: `ap_Menu_Layouts_MainMenu.h` (384-line classic File/
  Edit/View/… layout) and `ap_Menu_Layouts_Embedded.h` are gone;
  `ap_Menu_Layouts_All.h` keeps an empty `Main` stub so
  `EV_UnixMenuBar` still owns the shared action group and label set
  that the ribbon buttons and the right-click context popovers
  (`ap_ML_Context*.h`, all retained) resolve against.
- **Plugin-era dynamic menu API removed** — with no plugins left,
  nothing could mutate menus at runtime: `EV_Menu::addMenuItem`,
  `EV_Menu::_doAddMenuItem`/`EV_UnixMenu::_doAddMenuItem`,
  `EV_Menu_Layout::addLayoutItem`/`addFakeLayoutItem`/`m_iMaxId`,
  `EV_searchMenuLabel`, and the whole `XAP_Menu_Factory` mutation
  surface (`getNewID`, `addNewMenuAfter/Before`, `removeMenuItem`,
  `resetMenusToDefault`, `addNewLabel`, `removeLabel`,
  `resetLabelsToDefault`, `createContextMenu`, `removeContextMenu`,
  `GetMenuLabelSetLanguageCount`, `GetNthMenuLabelLanguageName`)
  deleted — the factory is now a pure static-table lookup
  (`CreateMenuLayout`/`FindContextMenu`/`CreateMenuLabelSet`).
  `xap_Menu_LabelSet.h` reduced to nothing and deleted.
- **Mail-merge feature removed** — intentional: the ribbon never
  mapped it. `ie_mailmerge.cpp/.h` (787 + 170 lines), the `--merge`
  CLI option, `AP_Convert::setMergeSource` plus its
  Save/Print listener classes, `PD_Document`'s merge map
  (`getMailMergeField`/`mailMergeFieldExists`/`setMailMergeField`/
  `clearMailMergeMap`/`m_mailMergeMap`), `ap_Args` plumbing,
  DLG_MailMerge_* strings and `test/wp/mailmerge/` fixtures all gone.
  `MERGEFIELD` field import stays for DOC/DOCX compatibility —
  `fp_FieldMailMergeRun` now always renders `«fieldname»`, matching
  Word's unmerged display, instead of consulting a map nothing could
  populate.
- **Evolution Data Server integration removed** — configure.ac no
  longer detects `evolution-data-server`/`libebook`; the
  `WITH_EVOLUTION_DATA_SERVER` blocks resolved to their non-EDS
  paths; the `IE_Imp_RDF_VCard`/`IE_Imp_RDF_VCard_Sniffer` importer
  classes (only reachable through that integration) deleted from
  `ie_imp_RDF.*` while `IE_Imp_RDF` and `IE_Imp_RDF_Calendar` stay.
- **Permanently-disabled RDF contact paths removed** —
  `AP_MENU_ID_RDFANCHOR_EXPORTSEMITEM` (dead item in the RDF-anchor
  context menu) and `AP_MENU_ID_RDF_SEMITEM_NEW_CONTACT_FROM_FILE`
  (in no layout, vCard import was an upstream stub) deleted with
  their edit methods (`rdfAnchorExportSemanticItem`,
  `rdfInsertNewContactFromFile`), action entries, label strings and
  the `ap_GetState_RDF_Contact` state function.

### Resolved root causes worth noting

- **"double free or corruption" after ODF export** — was a stale
  `opendocument.so` in the user plugin dir colliding on
  `ODe_Style_Style::m_NCStyleMappings` with `libabinova`, not an
  exporter bug. The same stale-`.so` hazard applied to
  `openxml.so`/`epub.so`/`grammar.so` — plugin binaries now load only
  for the remaining plugin set.

### Known issues / not done yet

- GTK4 dialog migration is mechanically complete but some dialogs may
  still have layout quirks.
- macOS/Windows GTK4 builds not yet verified.
- Ribbon tab/group labels are hard-coded English (localization
  pending).
