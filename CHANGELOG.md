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
- **Built-in ODF encryption both ways** — decrypt on open (GTK
  password dialog / `ABIWORD_PASSWORD` env var); encrypt on save via
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
- **Application id renamed to `io.github.janos_szenfner.Abinova`** —
  GApplication id, gresource prefix
  (`/io/github/janos_szenfner/Abinova`), desktop/metainfo filenames
  and the installed icon theme name (`abinova`) now follow the
  reverse-DNS scheme derived from the repository URL, so the running
  app, its icons and its resources share one identity.

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
  Bug" point at `github.com/janos-szenfner/Exp-Abi` instead of the
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
  hook. All former functionality is compiled into `libabiword`.
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
  now points at `https://github.com/janos-szenfner/Exp-Abi` and
  names Abinova as the generator (the AWML doctype, namespaces and
  `abiword.*`/`dc.format` metadata keys are format identifiers and
  remain for compatibility).
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
- Ribbon tab/group labels are hard-coded English (localization
  pending).
