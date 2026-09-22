# Changelog

All notable changes in this experimental AbiWord GTK4 fork, grouped by
category. Based on upstream AbiWord 3.1.90 (`5e3e1cc` import).

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
- **Built-in ODF encryption both ways** — decrypt on open (GTK
  password dialog / `ABIWORD_PASSWORD` env var); encrypt on save via
  "Encrypt with password" in the ODF save dialog. PBKDF2-SHA1 +
  Blowfish CFB64 (vendored from OpenSSL 4.0.2, Apache-2.0), no
  libgcrypt dependency.
- **Built-in RDF metadata** — SAX-based `ODi_RDFParser` importer and
  `toRDFXML` serializer; `manifest.rdf` round-trips, no libredland.
- **Built-in Markdown import/export** — `.md`/`.markdown`/`.mdown`/
  `.mkd`/`.mkdn`, CommonMark + Zettlr compendium: ATX/setext headings,
  emphasis/strong/strike/code, links, autolinks, images, nested
  bullet/ordered/task lists, blockquotes, fenced+indented code,
  horizontal rules, GFM pipe tables with alignment, hard breaks;
  documents round-trip through Markdown.
- **Markdown importer extended** — YAML frontmatter parses into
  document metadata (`dc.title`/`dc.creator`/`dc.date`/`dc.subject`/
  `abiword.keywords`); reference links/images resolve from `[id]: url`
  definitions; `[^id]` footnotes become real AbiWord footnote objects;
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
  (`heading 1`, `list bullet`, …) resolve to AbiWord builtins instead
  of duplicate custom styles.
- **DOCX headers/footers** — tables, images, math and textboxes inside
  header/footer parts now import (was Common+Field states only);
  math listener unwinds cleanly on conversion failure.
- **Vendored `wv-1.2.9`** for legacy `.doc`, patched for the buffer
  overflows Debian reported in libwv-1.2 (bounded sprm walks,
  page-bound memcpys, FKP record clamps, `vsnprintf`); verified with a
  600-case byte-mutation ASan fuzz run.
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

### User interface

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
  Old AbiWord-only styles (Block Text, Plain Text, Chapter/Section/
  Numbered Heading) remain defined for document compatibility but
  are hidden from the Recommended list; the .doc importer's
  `s_translateStyleId` now maps Heading 5–9, Title, Subtitle, Strong
  and Emphasis to real built-ins.
- **Internal help window** — Help Contents / Search for Help /
  Credits no longer launch an external browser; they open an in-app
  "AbiWord Help" window (`xap_UnixHelpWindow`, behind a new
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
  paragraph. Arrange group renders Position/Wrap/Bring/Send/
  Selection Pane/Align/Group/Rotate, with unsupported entries
  visibly disabled rather than mis-wired.
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
  Bug" point at `github.com/janos-szenfner/Exp-Abi` instead of the
  old GNOME GitLab project; the About dialog lists Janos Szenfner
  and links the fork's repository.
- **Same-application clipboard deadlock** — `gdk_clipboard_read_async`
  deadlocked when AbiWord itself owned the clipboard (the async read
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

### Build system and repository cleanup

- **Plugins removed from the build** — `openxml`, `epub`, `grammar`
  deleted from `plugins/` (sources live in `src/` now); regenerated
  `m4/plugin-list.m4` / `plugin-configure.m4` / `plugin-builtin.m4` /
  `plugin-makefiles.m4`; no default plugins remain. Remaining loadable
  plugins: `mht`, `rsvg`, `wmf`, `wordperfect`, `wpg`.
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
- **Dead preprocessor branches resolved** — `TOOLKIT_*`,
  `XP_TARGET_*`, `XP_MAC`, constant `XAP_DONTUSE_XOR`; OS/compiler
  macros kept for future GTK4 ports to Windows/macOS.
- **Dead files removed** — `gr_UnixCairoImage`, `ut_PerlBindings`,
  `ut_stack`, dialog stubs, duplicate `ODc_Crypto`,
  `ie_exp_WordPerfect`, orphaned test fragments, `linkgrammarwrap`
  (unused since the hunspell switch), `--enable-menubutton` code.
- **Vendored third-party libraries** — `librevenge 0.0.6`,
  `libwpd 0.10.3`, `libwpg 0.3.4`, `libwps 0.4.14` (upgraded from
  0.4.11), `wv-1.2.9`, `hunspell-1.7.0` — all built as noinst
  convenience libs; no external downloads needed. The 43 MB
  `link-grammar-5.12.5` tree was dropped.
- **Build hardening** — `autoreconf` fixed for modern autoconf;
  vendored `AX_*` macros; GTK-only configure; `omml_xslt` install dir
  moved to `ABIWORD_DATADIR`; build-tree libtool in the test wrapper.
- **Historical documentation preserved** in `Old-Doc/`; new
  `README.md`, `CHANGES.md` and this `CHANGELOG.md` carry the
  experimental no-warranty notice.

### Resolved root causes worth noting

- **"double free or corruption" after ODF export** — was a stale
  `opendocument.so` in the user plugin dir colliding on
  `ODe_Style_Style::m_NCStyleMappings` with `libabiword`, not an
  exporter bug. The same stale-`.so` hazard applied to
  `openxml.so`/`epub.so`/`grammar.so` — plugin binaries now load only
  for the remaining plugin set.

### Known issues / not done yet

- GTK4 dialog migration is mechanically complete but some dialogs may
  still have layout quirks.
- macOS/Windows GTK4 builds not yet verified.
- ~150 suspicious duplicate msgstrs in `fi-FI.po` need a Finnish
  speaker.
- Ribbon tab/group labels are hard-coded English (localization
  pending).
