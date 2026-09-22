# AbiWord GTK4 Experiment

An experimental fork of the AbiWord word processor, focused on:

- **Built-in OpenDocument (ODT/ODF) support** — import, export, flat
  XML (`.fodt`), encryption, and RDF metadata implemented in the core
  library rather than as a plugin.
- **Built-in Office Open XML (DOCX) support** — the former `openxml`
  plugin now lives in `src/wp/impexp/openxml/` and is compiled into
  the core library: open, save, save-as and edit of `.docx`, including
  document properties (`docProps/core.xml` + `docProps/app.xml`)
  round-trip.
- **Built-in EPUB support** — the former `epub` plugin now lives in
  `src/wp/impexp/epub/` and is compiled into the core library:
  EPUB 3.3 import and export, plus the export-options dialog.
- **Built-in grammar checking** — the former `grammar` plugin now
  lives in `src/wp/ap/grammar/` (vendored hunspell 1.7.0 backend);
  sentence-level checking runs through the existing grammar-squiggle
  pipeline, gated by the `AutoGrammarCheck` preference.
- **MS Word (.doc) support** — bundled `wv-1.2.9` in `thirdparty/`,
  patched for the buffer overflows reported against libwv-1.2.
- **Bundled fonts** — a curated, redistributable font collection is
  installed with the application and registered with fontconfig at
  startup, so documents render consistently without relying on
  system-installed fonts.
- **Built-in Markdown support** — open, edit, save and save-as for
  `.md` files, implemented in the core import/export library (not a
  plugin).
- **Built-in LaTeX support** — open, edit, save and save-as for
  `.tex` files, implemented in the core import/export library (not a
  plugin).
- **Repository cleanup** — obsolete plugins and dead files removed;
  Debian-reported bugs fixed against the actual implementation.

## Plugin cleanup

Two kinds of plugin removal were done: **integrated** plugins moved
into the core library (`src/wp/impexp/`, `src/wp/ap/`), and **removed**
plugins whose code was deleted outright.

### Integrated into the core library

These formats/features no longer ship as loadable plugins — they are
compiled into `libabiword` and are always available:

| Former plugin | Now | Notes |
|---------------|-----|-------|
| `opendocument` | `src/wp/impexp/odf/` | ODT/flat-ODT import+export, encryption, RDF |
| `openxml` | `src/wp/impexp/openxml/` | DOCX import+export, doc properties |
| `epub` | `src/wp/impexp/epub/` | EPUB 3.3/2 import+export + options dialog |
| `grammar` | `src/wp/ap/grammar/` | sentence checking via vendored Hunspell |
| `latex` | `src/wp/impexp/xp/ie_*_LaTeX.cpp` | `.tex` import+export |
| — (new) | `src/wp/impexp/xp/ie_*_Markdown.cpp` | `.md` import+export, was never a plugin |

Legacy `.doc` import is likewise built in via bundled `wv-1.2.9`
(`thirdparty/`), and Hunspell grammar checking is compiled in — none
of these appear in the plugin list any more.

### Removed plugins

Deleted because they were dead code, unmaintained, duplicated
built-in functionality, or depended on services/toolkits that no
longer exist:

- **Collaboration: `collab`, `ots`** — the real-time collaboration
  framework depended on abandoned telepathy/loudmouth-era backends
  (XMPP share, TCP tunnel, Sugar, service recordings) that no longer
  build or have servers to talk to. Removing it also closed the
  Launchpad crash reports filed against the collab accounts/dialogs
  (LP#1711244, LP#673045, LP#673052, LP#674721, LP#295596).
- **Web services: `google`, `wikipedia`, `urldict`, `gdict`** —
  talked to discontinued Google/Wikipedia/dict.org endpoints over
  GTK2-era networking.
- **Obsolete formats/importers**: `aiksaurus` (thesaurus plugin —
  thesaurus is core), `applix`, `bmp`, `clarisworks`, `docbook`,
  `eml`, `garble`, `gda` (GNOME-DB), `gimp`, `goffice` (Gnumeric
  component embedding — also closed LP#388971), `hancom`, `hrtext`,
  `iscii`, `kword`, `loadbindings`, `mathview` (GtkMathView widget —
  MathML is handled in-core), `mif`, `mswrite`, `openwriter`,
  `opml`, `paint`, `passepartout`, `pdb`, `pdf` (experimental PDF
  import — PDF remains an export target), `presentation`, `psion`,
  `s5`, `sdw`, `t602`, `testharness`, `wml`, `xslfo`.
- **Developer/misc**: `command` (remote-control pipe) and the
  GTK2-only `--enable-menubutton` code path.

### Remaining plugins

`mht` (self-contained MHTML importer), `rsvg`, `wmf`, `wordperfect`
(libwpd/libwps vendored), `wpg`.

> **Disclaimer:** This is an experimental project. It is provided
> **as is, without any warranty** of any kind, express or implied.
> The author(s) accept **no responsibility or liability** for any
> damage, data loss, or other consequences arising from its use.
> Use at your own risk.

## Highlights of changes

### Document formats

- **Built-in ODF core** (`src/wp/impexp/odf/`): the old `opendocument`
  plugin was removed and its implementation migrated into the core
  import/export library.
  - Open / save / save-as for `.odt`.
  - Flat ODF (`.fodt`) import, including `office:binary-data` embedded
    images decoded via base64.
  - **Encrypted ODF**: decrypt on open with a GTK password dialog
    (`ABIWORD_PASSWORD` env var for headless use); encrypt on save via
    a "Encrypt with password" checkbox + password/confirm fields in
    the ODF Save As dialog. Crypto is self-contained: PBKDF2-SHA1 +
    Blowfish CFB64 (vendored from OpenSSL 4.0.2, Apache-2.0) — no
    libgcrypt dependency.
  - **RDF metadata**: built-in RDF/XML parser (`ODi_RDFParser`) and
    serializer (`toRDFXML`) — no libredland dependency. `manifest.rdf`
    round-trips through open/save.
- **OpenXML fixes**: listener-state fixes for footer tables and
  equations; shared XSLT data restored.
- Remaining plugins: `mht`, `rsvg`, `wmf`, `wordperfect`, `wpg`
  (`openxml`, `epub` and `grammar` moved into the core library).
- **Built-in Markdown** (`src/wp/impexp/xp/ie_imp_Markdown.cpp` /
  `ie_exp_Markdown.cpp`): full read/write for `.md`, `.markdown`,
  `.mdown`, `.mkd`, `.mkdn` and the `text/markdown` MIME type.
  Syntax follows CommonMark plus the Zettlr Markdown Compendium
  (https://docs.zettlr.com/en/editor/markdown-compendium.html):
  - ATX (`#`) and setext (`===` / `---`) headings mapped to the
    `Heading 1`-`Heading 4` paragraph styles.
  - `**bold**`, `*italic*`, `***both***`, `~~strike~~`, inline
    `` `code` `` (Courier New) with backslash escapes.
  - `[text](url)` hyperlinks (real AbiWord link objects),
    `<scheme://...>` autolinks, `![alt](path)` image embedding.
  - Bullet (`-`/`*`/`+`), ordered (`1.`) and task (`[ ]`/`[x]`)
    lists, nested by indentation, as real AbiWord lists.
  - `>` blockquotes (`Block Text` style), fenced/indented code
    blocks (`Plain Text` style), `---`/`***` horizontal rules
    (paragraph bottom border), GFM pipe tables with column
    alignment, and hard line breaks (two trailing spaces or `\`).
  - YAML frontmatter (`---` at file start) is parsed into document
    metadata (`dc.title`, `dc.creator`, `dc.date`, `dc.subject`,
    `abiword.keywords`) instead of being shown as text.
  - Reference links/images (`[text][id]` + `[id]: url` definitions)
    resolve; definition lines are consumed rather than displayed.
  - Footnotes (`[^id]` refs + `[^id]: text` definitions) become real
    AbiWord footnote objects.
  - Inline `$...$` and fenced `$$...$$`/`math` blocks are imported
    as styled math text (no MathML renderer — equations keep their
    TeX source, italicised).
  - Mermaid fenced blocks keep their source under a `Plain Text`
    style (no diagram renderer — the code is preserved verbatim).
  - Raw HTML blocks are reduced to their readable text content;
    `<!-- -->` comments are dropped.
  - Emoji shortcodes (`:smile:`, `:heart:`, …) convert to Unicode.
  - Export writes the same constructs back, so a document round-trips
    through Markdown without losing its formatting structure.
- **Built-in LaTeX** (`src/wp/impexp/xp/ie_imp_LaTeX.cpp` /
  `ie_exp_LaTeX.cpp`): full read/write for `.tex`, `.latex` and
  `.ltx` files — the old `latex` plugin was removed and its exporter
  migrated into the core import/export library, and a new importer
  was added. Syntax follows the LaTeX project
  (https://www.latex-project.org/):
  - `\documentclass` / `\usepackage` preamble parsing,
    `\title` / `\author` / `\date` with `\maketitle` mapped to the
    `Title` style.
  - `\part`, `\chapter`, `\section` … `\subparagraph` (starred forms
    included) mapped to `Heading 1`-`Heading 4`.
  - `\textbf`, `\textit`, `\emph`, `\texttt`, `\underline`,
    `\textsuperscript` / `\textsubscript` and the `{ \bf ... }`-style
    declarations mapped to real character formatting.
  - `itemize` / `enumerate` / `description` environments as real
    AbiWord lists, including nesting.
  - `quote` / `quotation` / `verse` (`Block Text` style),
    `verbatim` / `lstlisting` (`Plain Text` + Courier New),
    `center` / `flushleft` / `flushright` alignment environments,
    `tabular` / `array` / `longtable` tables, `\includegraphics`
    image embedding, `\footnote` (real footnote objects),
    `\hrule` (paragraph bottom border) and `\newpage` /
    `\clearpage` / `\pagebreak` page breaks.
  - Comments (`%`), escaped specials (`\%` `\&` `\_` `\#` `\{`
    `\}` `\$`), `~` non-breaking spaces, quote and dash ligatures
    (` `` `, `''`, `---`, `--`), accent commands (`\'`, `\"`, `\^`,
    `\~`, ``\` ``, `\c`, …) and Latin-1 ligature commands
    (`\ae`, `\oe`, `\ss`, …).
  - Inline `$...$`, `$$...$$` and `\( ... \)` math plus
    `equation` / `align` / `displaymath` environments are imported
    as styled text; existing MathML equations are exported back to
    LaTeX through the built-in MathML→LaTeX converter
    (`ie_math_convert`, xsltml stylesheets).
  - Export writes `\documentclass` + preamble, sectioning commands,
    lists, tables and formatting commands, so a document round-trips
    through LaTeX without losing its formatting structure.
- **EPUB support modernized to EPUB 3.3** (now built-in): `version="3.0"` packages
  with the required `dcterms:modified` metadata, `properties="nav"`
  on the navigation document and `properties="mathml"` on MathML
  content (replacing the draft-era `mathml="true"` and `profile`
  attributes), `xmlns:epub` declared on content documents, BCP 47
  language tags (`en-US`, not `en_US`), and `urn:uuid:` identifiers.
  EPUB 3 is now the default export version; EPUB 2 remains available.
- **WordPerfect plugin refreshed**: vendored `libwps` updated from
  0.4.11 to 0.4.14 (current upstream); `libwpd-0.10.3` already
  matches upstream.
- **DOCX importer audited against modern OOXML / LibreOffice
  behaviour**: `mc:AlternateContent` markup-compatibility blocks are
  now handled per the spec — the `mc:Choice` branch is consumed and
  the entire `mc:Fallback` subtree is suppressed. Previously both
  branches were parsed, duplicating textboxes/drawings produced by
  Word 2010+ and LibreOffice. `w:sdt`/`w:sdtContent` content
  controls, tracked-changes containers (`w:ins`, `w:del`,
  `w:moveFrom`, `w:moveTo`) and `w14`/`w15` extension namespaces
  were verified to parse correctly. Legacy `.doc` continues through
  bundled `wv-1.2.9`.
- **DOCX layout fidelity — multi-page pagination fixes**: a real-world
  CV that rendered on ~3 pages instead of Word's 2 exposed a chain of
  importer bugs, all now fixed:
  - `w:docDefaults` no longer hijacks the `Normal` style: the
    document's real `Normal` style is imported as `_Normal` and
    unstyled paragraphs resolve to it, so stray
    `w:after`/`w:line`/`w:sz` docDefaults stop inflating every
    paragraph.
  - Theme fonts resolve correctly: `w:themeFontLang` maps font
    ranges to *languages* (e.g. `en-US` → `Latn`) but theme
    `<a:font>` entries are keyed by ISO-15924 script and the Latin
    typeface lives under `latin` — lookups now fall back to the
    range's default script, so `asciiTheme="minorHAnsi"` yields
    Calibri instead of silently degrading to Times New Roman.
  - `w:rFonts` resolution now only considers the Latin-range
    attributes (`ascii`/`asciiTheme`, `hAnsi`/`hAnsiTheme`); a style
    that sets only `w:eastAsia`/`w:cs` (e.g. Verdana for CJK) no
    longer leaks that face onto Latin text — it inherits instead.
  - `w:contextualSpacing` is honoured: consecutive same-style
    paragraphs (e.g. list items) collapse their inter-paragraph
    margins instead of summing them.
  - `w:pgMar` is applied to the section whose `sectPr` carries it
    instead of globally overwriting every section with the last
    sectPr's margins.
  - Paragraph-mark run properties (`w:pPr/w:rPr`) no longer leak
    `w:highlight`/`w:shd` onto the whole paragraph (only `w:sz`,
    which controls empty-paragraph height, is taken), and the
    paragraph that carries a `w:sectPr` break no longer paints
    borders — matching Word's border merging. The `.abw` format
    gained a `section-break` paragraph property for this (see
    below).
  - `.abw` extension: paragraphs may carry `section-break:1` to mark
    the paragraph whose mark terminates a Word section; layout uses
    it to suppress borders on the empty break mark. Older AbiWord
    versions ignore the unknown property safely.
- **Grammar checker switched to Hunspell** (now built-in): the checker no
  longer uses link-grammar. A vendored `hunspell-1.7.0` is built in
  `thirdparty/` and the sentence walker now flags each
  misspelled word with the existing grammar-squiggle path. The
  `link-grammar-5.12.5` third-party tree (~43 MB) was removed.
  English dictionaries are found under `/usr/share/hunspell`,
  `/usr/share/myspell` and the `/usr/local` equivalents.
- **LibreOffice-style status bar**: the bottom bar now shows, left
  to right, page `Page: n/m`, live `N words, N characters` (via
  `FV_View::countWords`), the current paragraph style, insert /
  overwrite and input-mode indicators, document language and the
  zoom slider with `-`/`+` buttons and a percentage that opens the
  Zoom dialog.
- **LibreOffice-style font selector**: each entry in the toolbar
  font-name dropdown is rendered in its own typeface (the closed
  combo shows the active font the same way). The widget was ported
  from `GtkComboBox` to `GtkDropDown` + `GtkSortListModel` with a
  lazy `GtkListItemFactory`, so only visible rows load a font —
  the old cell-renderer path measured every one of ~2000 fonts on
  popup open and froze the UI for seconds. Type-to-search is
  enabled on the dropdown.
- **LibreOffice NotebookBar-style ribbon UI**: an alternative
  interface modelled on LibreOffice Writer's NotebookBar
  (`sw/uiconfig/swriter/ui/notebookbar.ui`). A `GtkNotebook`
  presents File / Home / Insert / References / Layout / Review /
  View / Help tabs — **Home is the default tab** — plus a contextual
  Table tab that appears only while the caret is inside a table.
  Groups mix compact
  three-row button grids with Word-style large icon-over-caption
  buttons (Paste, Find, Replace, Select All) and glyph-only tiles
  (bold/italic/underline, alignment). Rich controls: the Home tab
  carries the live font-family and font-size combos, the style
  combo, text/highlight color pickers, format painter, list
  presets, indent and line-spacing buttons, plus a live Styles
  gallery — a horizontally-scrolling strip of tiles that renders
  each paragraph style's name in the style's own formatting and
  applies it on click; the View tab has the zoom combo.
  The **Layout tab** is Word-style: a Page Setup group of large
  dropdown buttons — **Margins** (Normal / Narrow / Moderate /
  Wide / Mirrored gallery with page-glyph illustrations, the
  current preset checkmarked, plus Custom Margins…),
  **Orientation** (Portrait / Landscape), **Size** (the full
  `fp_PageSize` list including Executive and 8.5×13, scrollable,
  plus More Paper Sizes…), **Columns** (One / Two / Three,
  Left / Right disabled, More Columns…), **Breaks** (page /
  column breaks and next-page / continuous / even / odd section
  breaks), **Line Numbers** and **Hyphenation** (option dialogs
  that store the document properties pending layout-engine
  support); a Paragraph group with Left/Right indent and
  Before/After spacing spin fields synced to the caret; and a
  working Arrange group — Position presets, Wrap modes and Align
  dropdowns, Bring Forward / Send Backward Z-ordering (persistent
  `frame-stack-order`), a docked **Selection Pane** (front-to-back
  object list with hide/reorder/rename via `frame-hidden` and
  `frame-name`), and Word-style **Group** and **Rotate** popovers:
  objects ticked in the Selection Pane combine into a persistent
  `frame-group` that drags, restacks and rotates as one unit, and
  Rotate offers Right/Left 90°, horizontal/vertical flips and a
  custom angle (`frame-rotation`/`frame-flip-*` — all persisted in
  `.abw` and honoured by PDF export).
  Custom Margins… / More Paper Sizes… / Format → Document open a
  Word-style **Document dialog** (Margins + Layout tabs, Page
  Setup…, Default… → NORMAL template, Apply to whole document /
  section / point-forward). The Clipboard group is a Word-style
  **split Paste button**: clicking the icon pastes immediately with
  formatting, while the arrow opens "Paste Options:" with **Keep
  Text Only** and **Paste Special…**. Paste Special lists the real
  clipboard formats — all `image/*` types are grouped as one
  "Picture" entry (best format auto-selected: PNG > SVG > JPEG…)
  and alias duplicates (text/plain vs UTF8_STRING, text/rtf vs
  application/rtf, text/html vs xhtml) are collapsed. Ribbon items
  dispatch through the same `menu.*` GActions and toolbar edit
  methods as the classic UI, so enablement, toggle and combo state
  stay in sync. Switch between interfaces via Help → Interface
  (ribbon is the default; the choice persists in the `RibbonUI`
  preference).
- **Word-compatible built-in styles**: the style set in
  `pt_PT_Styles.cpp` now matches Word — `Normal` at 1.15 line
  spacing, `No Spacing`, `Title` (26 pt bold centred), `Subtitle`,
  `Heading 1`-`9` respecified to Word's sizes and spacing (16 / 14 /
  13 / 12-italic / 11 / 11 / 10 / 10 / 10 pt, keep-with-next),
  `Quote` / `Intense Quote` (0.5″ indent, 1.5 pt gray left rule),
  `Book Title`, `List Paragraph`, and character styles `Emphasis`,
  `Strong`, `Subtle`/`Intense Emphasis`, `Subtle`/`Intense
  Reference`. Legacy AbiWord-only styles (`Block Text`,
  `Plain Text`, `Chapter`/`Section`/`Numbered Heading`) stay defined
  but are hidden from the Recommended list, and the `* List`
  pseudo-styles are filtered out of the gallery and pane entirely.
  The .doc importer maps `Heading 5`-`9`, `Title`, `Subtitle`,
  `Strong` and `Emphasis` again. The ribbon gallery renders tiles in
  Word order (`Normal`, `No Spacing`, `Heading 1`-`3`, `Title`,
  `Subtitle`, emphasis styles, quotes, …) with `<`/`>` overflow
  arrows, and a **Styles Pane** button docks a live side pane
  (current style readout, New Style…, Recommended/All filter, styled
  rows, Clear Formatting). Ribbon groups are separated by a visible
  1 px line, the Help tab uses large icon buttons, and **Check for
  Updates** queries the GitHub releases/tags API in a background
  thread and reports the result in a symmetric in-app dialog with a
  download link when a newer version exists.
- **Same-application clipboard deadlock fixed**: pasting data that
  AbiWord itself had copied wedged the UI forever — the async
  `gdk_clipboard_read_async` path called back into our own
  `AbiContentProvider` on the main thread and deadlocked on a GLib
  mutex. `XAP_UnixClipboard::getData`/`getTextData` now detect a
  locally-owned clipboard (`gdk_clipboard_is_local`) and read the
  internal clipboard synchronously; the async round-trip is only
  used for foreign clipboard owners. Fixes all paste paths (Paste,
  Keep Text Only, Paste Special) and makes same-app paste faster.

### GTK4 runtime fixes (this round)

- **Menubar pointer-motion crash (auto-close)**: an "enter" motion
  controller called `refreshMenu`, which could run `g_menu_remove_all()`
  on the live `GMenuModel` while a `GtkPopoverMenu` was open — GTK then
  crashed inside `gtk_popover_menu_remove_child` and the whole app
  exited. Menu models are now rebuilt and swapped atomically via
  `gtk_menu_button_set_menu_model`/`gtk_menu_bar_set_menu_model`
  instead of mutated in place.
- **Keyboard input restored**: the document drawing area had
  `can-focus` but not `focusable`, so `grab_focus()` silently failed
  and keystrokes never reached the view.
- **Border & Shading crash**: a stale `<signal>` handler
  (`on_cbtBorderColorButton_color_set`) in the dialog `.ui` file had
  no matching callback in the binary; GTK4 Builder treats unresolved
  handlers as fatal. All `.ui` files were swept for dead handler
  names and GTK3-removed widget classes.
- **Dead `clicked` signals on `GtkCheckButton`**: GTK4 check buttons
  do not emit `clicked`; the Zoom dialog radios (all six), Page
  Numbers radios (all five), and similar controls in the New,
  Format Frame, Format Footnotes and Image dialogs were rewired to
  `toggled` (with active-state guards for radio groups).
- **Word Count dialog killed the app on close**: the Close response
  destroyed the widget tree but left the one-second auto-update timer
  firing on dead widgets; it now routes through `destroy()`.
- **Go To dialog spin buttons**: `GtkSpinButton`s were cast to
  `GtkEntry`, silently returning empty text; the helper now takes
  `GtkEditable`.
- **Toolbar overstretch**: `GtkComboBoxText` widgets propagate
  `hexpand` from their internal entry, letting the font-size combo
  swallow the whole toolbar; `hexpand` is now explicitly disabled and
  the entry width capped.
- **Internal help bundled**: the upstream `abiword-docs` manual was
  imported and converted to HTML (`help/`, 220 pages). Help buttons
  open a built-in help browser (`xap_UnixHelpWindow`) — a popup
  window with Back/Home navigation, clickable cross-page links, a
  language selector (English / Français / Polski over the bundled
  `help/<lang>` trees), and live search across every page of the
  selected language with titled results and match snippets. Dialog
  F1 help targets route there too. The 25 Polish pages were repaired
  to true UTF-8 (they were mixed UTF-8/Windows-1250 and rendered as
  mojibake).
- **Ruler redesign** (`ap_TopRuler.cpp`): full-height bar, bottom-
  anchored tick hierarchy, gray margin bands, and inch/half-inch
  numeric labels drawn with the GUI font so they stay a constant
  size at any zoom.
- **Status-bar zoom control**: `−` / slider / `+` / percentage at the
  right end of the bottom bar (LibreOffice style); the percentage
  opens the Zoom dialog, buttons use the `zoomIn`/`zoomOut` edit
  methods, and the control tracks external zoom changes.
- **Font-selector freeze**: opening the toolbar font dropdown froze
  the UI — the `GtkComboBox` cell renderer with a per-row `family`
  attribute measured every one of the ~2000 installed fonts on
  popup open (≈2.7 s blocked in `gtk_combo_box_popup` alone, more
  with the per-frame preview popover). The widget was rewritten on
  `GtkDropDown` + `GtkSortListModel` (`incremental`) with a lazy
  `GtkListItemFactory`, so only visible rows ever load a font;
  the prelight→popover preview machinery was removed entirely
  (each row is already rendered in its own typeface).
- **Styles dialog dead radios**: the three "style type" radio
  `GtkCheckButton`s were still connected to `clicked` — the filter
  never worked in GTK4. Rewired to `toggled` with an active-state
  guard.
- **Set Language dialog**: now has explicit Cancel/Apply buttons;
  the selection is committed only on Apply (previously any close —
  including the window X — silently applied whatever was selected).
- **`GtkCheckButton` cast as `GtkToggleButton`**: in GTK4 the two are
  siblings, so every `GTK_TOGGLE_BUTTON()` cast on a check button
  warned and `gtk_toggle_button_get_active()` always returned FALSE —
  breaking checkbox/radio state reads across the Lists, Options,
  Format TOC, Mark Revisions, Break, HTML Options and EPUB export
  dialogs (Columns' column-order toggle was fixed surgically; its
  real toggle buttons were left alone). All verified sites now use
  `gtk_check_button_get_active()`/`set_active()`.
- **Input-method context lifetime**: `gtk_im_context_focus_out` and
  friends asserted on a NULL context (visible in the runtime log);
  all `m_imContext` uses are now guarded, the destructor clears the
  pointer after unref, and the deprecated
  `gtk_im_context_set_surrounding()` was replaced by
  `gtk_im_context_set_surrounding_with_selection()`.
- **Clip Art dialog use-after-free**: `g_idle_add(fill_store, this)`
  could fire after the dialog object was destroyed; the idle source
  is now tracked and cancelled in the destructor.
- **Dialogs mapped without a transient parent**: the two-argument
  `abiRunModalDialog()` overload skipped `abiSetupModalDialog`, so
  dialogs run through it (Styles, About, Clip Art errors, …) mapped
  parentless; it now falls back to the last-focused frame's top-level
  window.
- **Duplicate `accessible-role` critical**: GTK4's accessible role is
  immutable once set; `abiRunModalDialog` only applies its role now
  when the widget has none.
- **`genImageFromRectangle` paintable/texture crash**: screenshots of
  the drawing area (visual text/frame drags, inline-image caching,
  ODF thumbnail generation) cast a `GtkRenderNodePaintable` to
  `GdkTexture` — the checked cast warned but returned non-NULL, so
  `gdk_texture_download` ran on a non-texture and could crash. The
  path now snapshots to a `GskRenderNode` and rasterizes through the
  native `GskRenderer` with a `GDK_IS_TEXTURE` guard.
- **Zoom reset + page centering**: the status-bar zoom cluster gained
  a `zoom-original` button that jumps straight to 100%, and
  `getPageViewLeftMargin()` now centers the page horizontally whenever
  the zoomed page is narrower than the window (LibreOffice behaviour)
  instead of pinning it to a fixed left margin — this also feeds the
  layout width, so no phantom scrollbar appears.
- **Ruler redrawn to LibreOffice proportions**: the top ruler was
  rebuilt slimmer (22px vs 32px) with a narrower tab-type strip;
  indent markers are now flat LO-style triangles (up-pointing left/
  right indents at the bottom band, down-pointing first-line indent at
  the top band, small square combined-drag handle under the left
  indent), scaled from the marker rects instead of hardcoded pixels;
  margin markers are clamped inside the bar.
- **LibreOffice-style font box**: `AbiFontCombo` is now an editable
  `GtkEntry` + dropdown arrow — type a font name and press Enter (or
  leave the field) to apply it, including fonts not installed on the
  system. The arrow opens the lazy `GtkDropDown` list: instant popup,
  every visible row in its own typeface, type-to-search, incremental
  sort. The collapsed entry shows plain GUI-font text like LO.

### Debian bug audit

- **#896745 font size by keyboard** — fixed: typed sizes not in the
  dropdown are now read from the combo's entry instead of
  `gtk_combo_box_text_get_active_text` (which returned NULL).
- **#1010880 stale zoom display** — fixed: the toolbar zoom combo
  appends unlisted percentages (e.g. dialog-set 125%) so it always
  shows the real zoom; the status-bar percentage likewise syncs on
  every view notification.
- **#704629 Finnish translations** — fixed the reported menu items
  (`Save`, `Tools`, `Table`, `View`, `Cut`, `Copy`, `Paste`, `Print`
  were copy-paste corrupted in `fi-FI.po`/`.strings`). ~150 further
  suspicious duplicate msgstrs remain; a full pass needs a Finnish
  speaker.
- **#845137 crash opening files** — already resolved: upstream
  reverted svn r33154 (table-breaking change that caused the
  crashes); this tree carries the reverted code in
  `fb_ColumnBreaker.cpp`.
- **#740403 PDF save produced `.abw.saved`** — verified fixed: PDF
  export works (`--to=pdf` produces valid PDF 1.7); the `.saved`
  files were crash backups from a then-broken build.
- **#926419 typed text invisible on Wayland** — resolved by the
  GTK4 port: the old GTK3 expose-based draw path was replaced.
- **#740635 ruler disappears on tab-type cycling** — resolved by
  the GTK4 redraw path; the tab-toggle machinery is intact.
- **#572798 PDF export settings (wishlist)** — cairo PDF output is
  vector text with native-resolution embedded images, so the
  original quality complaint no longer applies; a settings dialog
  remains a possible future enhancement.

### Ubuntu Launchpad bug fixes

The 67 open abiword reports on Launchpad were audited; the following
were fixed in this tree:

- **LP#921756 / LP#1712097** — `GR_Graphics::tlu()/tluD()/tduD()`
  SIGABRT: guarded against zoom=0 and resolution=0 division producing
  inf→UB on float→int cast.
- **LP#1620709** — `std::string::assign` crash in the hyperlink
  dialog path: NULL-guarded `getHyperlink()`/`getHyperlinkTitle()`.
- **LP#1564143** — `pixbufForByteBuf` crash: no longer dereferences an
  unset `GError` when `gdk_pixbuf_loader_write()` fails.
- **LP#1577612** — `_imRetrieveSurrounding_cb`/`_imDeleteSurrounding_cb`
  crash: NULL view guard + clamped surrounding-text position.
- **LP#1204037** — `FV_UnixSelectionHandles` ctor SIGABRT: null
  view/frame impl guard in `_ensureTextHandle`.
- **LP#1628717** — `pf_Fragments`/`repairDoc` crash: fragments deleted
  during repair are tracked and skipped in subsequent passes;
  `_removeHdrFtr()` stops at body sections so a missing closing strux
  can't swallow the document.
- **LP#1248011** — ABW exporter emitted a duplicate `props` attribute
  producing invalid XML it could not reopen: merged into the existing
  attribute instead.
- **LP#234756** — `LC_PAPER`/`LC_MEASUREMENT` locale settings are now
  honored for the default page size via
  `nl_langinfo(_NL_PAPER_WIDTH/_NL_PAPER_HEIGHT)`.
- **LP#1386253** — `AP_TopRuler::mousePress` SIGABRT: audit-fixed
  signed-overflow UB in the ruler code (same code path).
- **LP#995887 / LP#1031137 / LP#941566 / LP#1094243** — black/too-bright
  rulers and dialogs: resolved by the GTK4 port (Cairo/CSS drawing
  replaced the old `GtkStyle` color lookups); ruler foreground is now
  explicitly black on the fixed light background.
- **LP#1629135** — `gdk_window_ref_cairo_surface`/`_beginPaint` crash:
  resolved by the GTK4 render model.
- **LP#1603245** — `g_type_check_instance_cast` in `motion_notify_event`:
  resolved (event controllers replaced motion-notify signals).
- **LP#1279020** — crash on theme change: styling is now CSS-based.
- **LP#1485796** — cogl/clutter `XRRGetScreenResources` crash: GTK4 no
  longer uses cogl/clutter.
- **LP#926419** — typed text invisible on Wayland: resolved by the
  GTK4 draw path.
- **LP#1141885** — `.docx` not associated with abiword: the format is
  now built-in; `abiword.keys` registers the DOCX/ODT/EPUB mimetypes.

Feature-removal closures (component deleted): LP#1711244, LP#673045,
LP#673052, LP#674721, LP#295596, LP#388971 (collab/goffice plugins).

### Fonts

- **Carlito is the default document font** (replacing Times New Roman):
  default property table, `Normal` style construction, the view's
  default font resolution, and all 63 `normal.awt-*` templates were
  updated.
- **99 bundled font files** under `fonts/`, installed to
  `<AbiSuiteLibDir>/fonts` and registered at startup via
  `FcConfigAppFontAddDir` (`src/af/xap/gtk/xap_UnixApp.cpp`). The set
  mirrors the LibreOffice/OpenOffice bundled font collection plus the
  Intos family:

  | Family | Metric-compatible with |
  |--------|------------------------|
  | Carlito | Calibri (default font) |
  | Caladea | Cambria |
  | Intos / Intos Display / Narrow / Serif | Aptos |
  | Liberation Sans / Serif / Mono / Sans Narrow | Arial / Times New Roman / Courier New / Arial Narrow |
  | DejaVu Sans / Serif / Mono / Condensed | — (wide coverage) |
  | OpenSymbol | Symbol fallback used by ODF suites |
  | Gentium, Gentium Book | document serif faces |
  | Noto Sans, Noto Serif | core text families |
  | Source Sans 3, Source Serif 4, Source Code Pro | Adobe open families |
  | Linux Libertine, Linux Biolinum | serif/sans text families |

- **Font substitution rules** (`fonts/abiword-fonts.conf`): loaded via
  `FcConfigParseAndLoad` at startup; maps common document font names
  (Calibri→Carlito, Cambria→Caladea, Aptos→Intos, Times New
  Roman→Liberation Serif, Arial→Liberation Sans, Courier
  New→Liberation Mono, etc.) so documents keep their layout when the
  original fonts are absent. See `fonts/README.md` for provenance and
  licenses.

### Bug fixes (Debian-reported)

- Keyboard table-cell resize (Ctrl+Alt+arrows) implemented.
- Table border rendering no longer exposes hidden margins.
- TOC round-trip on RTF export (`fldrslt` export + `PTX_SectionTOC`
  import).
- RTL field bidi direction + mirrored table columns.
- Down-arrow page-boundary navigation audited (boundary clamp already
  correct).

See `CHANGELOG.md` for the categorized changelog of all changes.

## Per-commit modification log

Per-commit log of the modifications made in this fork, newest first.
Older upstream history is not listed here.

### Layout tab: indent/spacing field alignment

- The four Paragraph spin rows used natural-width labels
  ("Left:"/"Right:"/"Before:"/"After:"), so the spin entries sat
  ragged; labels now share a fixed width and the spin columns line
  up within each group.
- Before/After gained drawn spacing glyphs (three text lines + a
  blue arrow on the padded edge) instead of reusing the indent
  icons.

### Object grouping and rotation (Word-style Group/Rotate)

- New persistent frame properties: `frame-rotation` (degrees,
  normalised to [0,360)), `frame-flip-horiz`, `frame-flip-vert` and
  `frame-group` (shared `gN` id per group).
- `fp_FrameContainer::draw`/`drawHandles` apply the transform with a
  cairo save/rotate/scale/restore around the frame centre;
  `getInkBounds`/`unrotatePoint` give rotated damage bounds and
  inverse hit-testing (`fp_Page::mapXYToPosition`); damage checks in
  `fp_Page` use the ink bounds so rotated frames repaint fully.
- `FV_View` gained `rotateFrame`/`setFrameRotation`/`flipFrame`,
  `groupFrames`/`ungroupFrames`, `shiftFrameGroup` (group drag delta)
  and `_groupTransform` (members orbit/mirror around the group
  bounding-box centre); multi-object ticks live in `m_vecGroupSel`
  (`getGroupSel`/`groupSelCount`/`clearGroupSel`/`isGroupSel`).
- `FV_FrameEdit::mouseRelease` shifts all group members by the drag
  delta inside the same undo glob (drags only, not resizes).
- `fp_Page::restackFrameContainer` is group-aware: the whole group
  block moves as one Z-order unit keeping member order.
- New edit methods: `frameRotateR90/L90/180`, `frameRotateTo`
  (custom angle via call data), `frameFlipHoriz/Vert`, `frameGroup`,
  `frameUngroup`; `ap_GetState_Groupable` gates Group on ≥2 ticked
  objects.
- Layout ribbon: Arrange group's Group and Rotate buttons are now
  real popover menus (Group/Ungroup; Rotate Right 90°/Left 90°/
  Flip Vertical/Flip Horizontal/custom angle + Set) with drawn
  glyphs; the disabled placeholder path was removed for them.
- Selection Pane: per-row tick checkboxes drive the multi-object
  selection, `[gN]` badges mark group members, and Group/Ungroup
  buttons sit in the bottom bar with live sensitivity.
- Fix: ungroup writes `PTC_RemoveFmt` — `PTC_AddFmt` with an empty
  value silently keeps the property.
- Fix: `getCairo()` outside a paint implicitly calls `beginPaint()`
  and unbalanced the canvas group stack (blank canvas when selecting
  a rotated frame); the transform is only applied when
  `getPaintCount() > 0`.
- Fix: `~FV_View` clears `m_pLayout->setView(nullptr)` —
  `~fl_FrameLayout` read `FV_FrameEdit::m_pFrameLayout` off the
  already-deleted view during teardown (valgrind UAF /
  `munmap_chunk` on `--to=` exit).
- Verified: build clean; on-screen group/ungroup badges and Z-order;
  `frame-group`/`frame-rotation`/`frame-flip-horiz`/`frame-hidden`
  all round-trip through `.abw` save/reload; 45° custom angle and
  flips render correctly in `--to=pdf` output.

### Selection Pane: docked object list with hide/reorder/rename

- New Selection pane docked on the right (sharing a `GtkStack` deck
  with the Styles pane) listing every frame object front-to-back;
  rows select the object in the document, an eye toggle hides/shows
  it, up/down buttons reorder the Z-layer, double-click renames it.
- Two new persistent frame properties: `frame-hidden` (skipped by
  `fp_Page` drawing and hit-testing) and `frame-name` (fallback
  `Text Box N`/`Picture N` labels); all writes go through
  `FV_View::setFrameProp` so they are undoable.
- Frame-targeted operations use explicit `fl_FrameLayout*` overloads
  (`restackFrame`, `selectFrameObject`, `getFrameLayouts`) instead of
  resolving through the caret.
- Toggled from the Arrange group's ribbon button
  (`sidebar-show-symbolic`) or `Alt+F10` (GNOME eats Alt+F10 as its
  toggle-maximized WM binding — the ribbon button is the reliable
  access); `EV_UnixMenu::ensureAction` creates `GSimpleAction`s for
  ribbon-only menu ids so the button works without a classic-menubar
  entry.

### Layout ribbon: working Z-ordering, Page Color/Image restyle, Columns preview

- Bring Forward / Send Backward popovers wire real Z-order ops onto a
  new persistent `frame-stack-order` property; `fp_Page` keeps frame
  layers sorted by rank and `FV_View::restackFrame` writes ranks
  through `setFrameFormat` so reordering is undoable and survives
  `.abw` round-trips. Bring/Send to Front/Back and In Front of /
  Behind Text included.
- Page Color / Page Image are now large buttons (drawn glyph over
  caption) matching the Margins control, with paint-drop / picture
  badge glyphs; Bring Forward / Send Backward get distinct staircase
  icons.
- Columns dialog preview draws again on GTK4: graphics + preview are
  created lazily in the draw callback, spin/toggle changes repaint
  live, and toggle-button pixmaps use `gtk_button_set_child` for
  clean teardown.
- New `ap_GetState_ObjSelected` menu state (frame edit active, image
  selected, or caret inside a frame); Layout indent/spacing spin
  buttons slimmed via `.ribbon-spin` CSS.

### Ribbon startup allocation warning + teardown crash fix

- Style-gallery nav buttons (go-previous/go-next) could be allocated
  below their CSS padding when the ribbon was squeezed during
  startup ("attempt to allocate GtkImage with width -19"); a
  `ribbon-nav` class with zero horizontal padding fixes it.
- `GtkNotebook` emits `switch-page` while being disposed during
  window teardown; `AP_UnixRibbon::refresh()` then called
  `getCurrentView()` on a frame whose view list was already gone —
  now guarded by `gtk_widget_in_destruction()`.

### Word-style Layout ribbon tab + Document dialog

- Page Setup group: large icon dropdown buttons for Margins
  (Normal/Narrow/Moderate/Wide/Mirrored gallery with page glyphs,
  current preset checked, Custom Margins…), Orientation
  (Portrait/Landscape), Size (full `fp_PageSize` list + new
  Executive and 8.5×13, More Paper Sizes…), Columns (1/2/3 + More
  Columns…), Breaks (page/column + next/continuous/even/odd section
  breaks), Line Numbers and Hyphenation (option dialogs storing
  document properties pending engine support). Paragraph
  indent/spacing spin fields synced to the caret.
- New `AP_DIALOG_ID_DOCUMENT` (XP + GTK) with Margins and Layout
  tabs, Page Setup…, Print… and Default… (NORMAL template) actions;
  reached via `docSettings` (Format → Document, ribbon).
- New edit methods: `pageMargins`, `pageOrientation`, `pageSize`,
  `pageColumns`, `insColumnBreak`, `insSectionBreak`, `paraProp`,
  `sectProps`, `docProps`, `docSettings`, `arrangePosition`,
  `wrapObject`; plus `FV_View::setDocWideSectionFormat()`.
- Fixes: `s_arrayEditMethods` requires strcmp order for its
  binary-search lookup — the new methods were inserted unsorted (and
  `doNumbers`/`doDashedList` were already swapped), silently dropping
  them from dispatch; `AP_UnixDialog_Document` hand-built
  `GtkStringList` models and unref'd them early (SIGSEGV in
  `g_list_model_get_n_items` on teardown) — dropdowns now use
  `gtk_drop_down_new_from_strings`.

### Audit fixes: dangling pointers, idle UAF, help search hygiene

- `cmdParaBorder` pushed `.c_str()` pointers from loop-scope
  `std::string`s into the property vector — dangling reads in
  `changeStruxFmt` (worked only via SSO luck); replaced with
  static-storage property names.
- The deferred `gtk_paned_set_position` idle held a raw `this` — UAF
  if the frame closed before the idle ran; now a heap cell + weak
  ref on the paned widget.
- Help-window `_runSearch` created a fresh anonymous tag per
  keystroke and never dropped old link tags (unbounded tag-table
  growth); named-tag reuse + shared `_clearLinkTags`, and
  search-changed is debounced ~200 ms instead of rescanning ~220
  files per keystroke.
- `bodyOnly()` now strips every head/script/style block, not just
  the first; deleted copy ops on `XAP_UnixHelpWindow` and
  `AP_UnixStylesPane`; `-Wenum-compare` fix on the FILE_CLOSE check.

### File tab polish: Document Properties, red Close, wrapped captions

- `FILE_PROPERTIES` renamed to "Document Properties" (it is the
  document's metadata, not app settings) and re-iconed to
  `dialog-information-symbolic`.
- `FILE_CLOSE` uses `window-close-symbolic` tinted red via a
  `.ribbon-close` class — applied to the icon only, so just the X
  glyph renders red like Word's destructive controls.
- Large icon buttons wrap captions onto two lines instead of
  ellipsizing ("Document Properties", "New using Template").

### File ribbon tab: large icon buttons, "Save a Copy" removed

- File tab groups (Document, Print) now use the Help-tab style —
  large icon-over-label buttons instead of the compact text list.
- Added icon mappings for "New using Template"
  (x-office-document-template) and "Page Setup"
  (document-page-setup).
- "Save a Copy" (AP_MENU_ID_FILE_EXPORT) dropped from the ribbon —
  duplicate of Save / Save As.

### Word-compatible styles, internal help window, visible group separators

- Built-in styles reworked to match Word: Normal gets 1.15 line
  spacing; new No Spacing, Title (26 pt bold centred), Subtitle
  (14 pt italic centred), List Paragraph, Quote / Intense Quote
  (italic / bold italic, 0.5" indent, 1.5 pt gray left border),
  Book Title; Heading 1-9 respecified to Word's sizes and spacing
  (16/14/13/12i/11/11/10/10/10, keep-with-next); new character
  styles Emphasis, Strong, Subtle/Intense Emphasis, Subtle/Intense
  Reference. Block Text, Plain Text, Chapter/Section/Numbered
  Heading kept but hidden from Recommended. .doc importer maps
  Heading 5-9, Title, Subtitle, Strong, Emphasis again.
- Gallery shows character styles too and orders tiles like Word
  (Normal, No Spacing, Heading 1-3, Title, Subtitle, emphasis
  styles, quotes, Book Title, List Paragraph); the Styles pane
  lists char styles as well.
- New internal help browser (`xap_UnixHelpWindow`, wired through
  `XAP_AppImpl::openHelpWindow`): Back/Home buttons, English /
  Français / Polski language dropdown over the bundled
  help/<lang> trees, live search across all pages of the current
  language with linked results and snippets, HTML rendered into a
  GtkTextView with clickable links; Help Contents / Search for Help
  / Credits edit methods open it instead of a browser.
- Repaired the 25 Polish help pages to true UTF-8 (they were a mix
  of UTF-8 and Windows-1250 → mojibake).
- Ribbon group separators are drawn with an explicit 1 px border
  colour so the line is actually visible (Help ↔ Interface etc.).

### Borders dropdown and docked Styles pane

- Borders button becomes a single menu-button dropping the
  Word-style border menu: edge presets (Bottom/Top/Left/Right, No
  Border, All/Outside/Inside, Inside Horizontal), Horizontal Line,
  Draw Table, Borders and Shading…; rows carry cairo-drawn
  edge-diagram icons; unsupported entries (Inside Vertical,
  diagonals, View Gridlines) render disabled.
- New `paraBorder` edit method + `FV_View::cmdParaBorder`: applies
  0.5pt solid black edges per selected block via `changeStruxFmt`
  inside one undo glob; "none" removes all four edges; "inside"
  borders the bottom of every block but the last; "hline" breaks
  the paragraph and draws a bottom-edge rule.
- Styles gallery redesigned to the LibreOffice strip: two-line
  tiles (styled "AaBbCcDdEe" sample + localized name), edge scroll
  arrows on overflow, "Styles Pane" button.
- New docked Styles pane (`ap_UnixStylesPane`, GtkPaned end child
  on the document area): title + close, current-style readout,
  New Style…/Select All, apply-a-style list rendered in each
  style's own formatting incl. a Clear Formatting row,
  Recommended/All Styles filter, guides checkboxes (disabled).
- Styles group slimmed to gallery + pane button; the FMT_STYLE
  combo stays in the layout hidden so its toolbar state still
  drives the tile highlight and the pane's current-style readout.

### Paragraph group redesign, list libraries, paragraph sort

- Paragraph group rebuilt LibreOffice-style: bullet/numbering/
  multilevel split-buttons, indents, sort and pilcrow on row 1;
  alignment, spacing dropdowns, borders and the paragraph dialog on
  row 2; Lists group merged in.
- New library popovers behind each list arrow: Bullet Library (13
  glyph tiles), Numbering Library (8 format tiles), List Library
  (current + multilevel presets); "Define New ..." entries open the
  Bullets and Numbering dialog.
- New `doListType` edit method + `FV_View::cmdApplyListType` /
  `cmdRemoveListFormat`: retypes an existing list through
  `fl_AutoNum::setListType` (no unlist), supports decimal/delim
  overrides (`%*%d`, `%L)`), numbered styles restart at 1 after a
  bulleted list, and "None" strips lists in a single pass.
- New paragraph sort (ascending/descending) —
  `FV_View::cmdSortParagraphs` + sort dropdown: case-folded UTF-8
  collation, one undo glob, paragraph properties preserved.
- Split-button drop arrows slimmed to ~12px.

### Font box with inline search, Change Case dropdown, colour pickers, eased scrolling

- `AbiFontCombo` rebuilt: the editable entry is now the search field —
  typing opens a custom `GtkPopover` anchored under the left edge of
  the field whose `GtkListView` + `GtkFilterListModel` list filters
  live on the typed text; the arrow button drops the full sorted list
  preselected at the current font; rows render in their own typeface
  via lazy factory binding. Keys captured by the popup's seat grab are
  forwarded to the entry (text, Backspace/Delete, Enter, Escape) so
  typing never dead-ends. The old `GtkDropDown` is gone.
- New Change Case "Aa" button on the Font group's top row: a single
  `GtkMenuButton` whose click drops a popover with Sentence case /
  lowercase / UPPERCASE / Capitalize Every Word / tOGGLE cASE — five
  new edit methods calling `FV_View::toggleCase` directly (no
  separate arrow widget); Clear Formatting moved next to the font
  colour button; Grow/Shrink/Aa share a homogeneous box
  (`AP_RIBBON_FLAG_EVEN`) so all three get the same width, and
  `AP_RIBBON_FLAG_SLIM` trims their padding to a moderate size.
- Ribbon colour pickers (Font Color, Highlight) replaced: a
  LibreOffice-style swatch grid with Automatic button + "Custom
  Color…" `GtkColorChooserDialog` (Select/Cancel). One swatch click
  applies immediately and closes; the previous embedded
  `GtkColorChooserWidget` had no way back and only applied on
  double-click. Font colour defaults to black.
- Vertical scrolling now eases: a `GdkFrameClock` tick glides the view
  offset toward the target (ease-out, retargets on each new notch),
  falling back to instant scroll when the canvas is unrealized.
- "Check for Updates" / "Report a Bug" now open the fork's GitHub
  repo; the About dialog lists Janos Szenfner and links the repo URL.

### Ribbon Font group redesign, selection-offset fix, finer scrolling, Mermaid rendering

- Ribbon Font group rebuilt as a LibreOffice-style two-row group: a
  new `AP_RIBBON_ITEM_ROWEND` layout kind switches a group to
  row-major packing (per-row `GtkBox`es, so narrow glyph buttons no
  longer share grid columns with the wide font combo). Row 1: font
  family + size combos, Grow/Shrink Font, Clear Formatting. Row 2:
  B/I/U/S/x²/x₂ text glyphs, highlight + font-colour glyph buttons,
  Font dialog launcher.
- New `AP_RIBBON_FLAG_GLYPH` draws Pango-markup glyphs on buttons
  (bold B, italic I, underlined U, struck S, x²/x₂, A⁺/A⁻); colour
  buttons got descriptive glyphs (bold "A" with red underline = font
  colour, "ab" on yellow = highlight); icon-only buttons fall back to
  their text label when the icon theme lacks the named icon.
- New menu items and wiring: Format → Text → Grow Font / Shrink Font
  / Clear Formatting (`clearFormatting` edit method →
  `FV_View::resetCharFormat`); Insert → Edit Equation; Help → Credits.
  Every ribbon button now resolves to a real `GAction` (the
  always-disabled Split Table entry was dropped from the ribbon).
  Note: the edit-method table is bsearch'd — new entries must be
  inserted in alphabetical order.
- Fixed the selection-offset bug: clicks and drag-selections landed
  ~7 rows below the pointer because `gdk_event_get_position()` yields
  surface-relative coordinates under GTK4 (~157 px off: header bar +
  ribbon + rulers). `EV_UnixMouse::{mouseClick,mouseUp,mouseMotion,
  mouseScroll}` now take the gesture callbacks' widget-relative x/y;
  the scroll controller translates its position via
  `gtk_widget_compute_point`.
- Finer vertical scrolling: discrete wheel steps reduced from 60 px
  to 36 px per notch; `GDK_SCROLL_SMOOTH` touchpad deltas scroll
  proportionally with fractional-notch accumulators instead of
  collapsing to a direction; `vScrollChanged` coalesces the pending
  scroll target instead of dropping intermediate positions.
- Mermaid fenced blocks in Markdown now render as embedded PNGs via a
  new built-in Cairo renderer (`ut_mermaid.cpp`): flowchart, sequence,
  Gantt, class and pie diagrams supported.

### Paste split button, DOCX layout fidelity, Markdown coverage

- Paste is now a split button on the ribbon Home tab: the main icon
  pastes immediately, the arrow opens a "Paste Options:" popover with
  "Keep Text Only" and "Paste Special…". Paste Special opens a dialog
  listing the real clipboard formats; clipboard aliases
  (`text/plain`/`UTF8_STRING`, `text/rtf`/`application/rtf`,
  `text/html`/`application/xhtml+xml`) are deduplicated and every
  `image/*` flavour collapses into a single "Picture" entry that
  pastes the best available format.
- Fixed a deadlock that froze every paste path: when AbiWord owns the
  clipboard itself, `gdk_clipboard_read_async` wedges on a GLib mutex
  inside the local content provider. `XAP_UnixClipboard::getData` and
  `getTextData` now check `gdk_clipboard_is_local()` and read the
  synchronous fake clipboard instead.
- New format-targeted paste path: `FV_View::cmdPasteAs(mime)` →
  `XAP_App::pasteFromClipboardWithFormat` → `AP_UnixApp`, reusing the
  normal importer dispatch for the chosen clipboard format.
- DOCX pagination/fidelity fixes (canonical CV now renders on 2
  pages, matching the original):
  - `OXML_FontManager::getValidFont` falls back to the range's
    default theme script when the `w:themeFontLang`-mapped script has
    no `<a:font>` entry (`en-US`→`Latn` has none; the Latin typeface
    lives under `latin`), instead of hardcoding Times New Roman.
  - `w:rFonts` resolution now only uses the Latin-range attributes
    (`w:ascii`/`w:asciiTheme`, `w:hAnsi`/`w:hAnsiTheme`); `w:eastAsia`
    and `w:cs` no longer leak onto Latin text and font-family is left
    to inherit when no Latin font is set.
  - `w:pgMar` is applied to the section its `w:sectPr` terminates
    (per-section margins) instead of overwriting one document-global
    value; `OXML_Document::addToPT` keeps per-section margins.
  - New `contextual-spacing` block property implements OOXML
    `w:contextualSpacing`: no margin between adjacent paragraphs that
    share a style carrying the flag (`fp_Line` margin collapse).
  - Unstyled `w:p` paragraphs default to the document's real Normal
    style (`_Normal`) instead of the docDefaults-derived `Normal`.
  - Paragraph-mark `w:pPr/w:rPr/w:sz` sets empty-paragraph line
    height; paragraph-mark shading is ignored; `w:pBdr` paragraph
    borders import/export (`w:between`/`w:bar` ignored); section-break
    paragraphs don't paint paragraph borders.
- Markdown importer extended: YAML frontmatter → document metadata,
  `[id]: url` reference definitions and reference links/images,
  `[^id]` footnotes as real footnote objects, `<!-- -->` comments
  dropped, `---` rules as bottom-bordered paragraphs, inline `$…$`/
  fenced `$$…$$`/`math` as styled text, Mermaid blocks verbatim, raw
  HTML reduced to text, `:emoji:` shortcodes to Unicode. Fixed
  backslash handling inside code spans (`` `\` `` now renders
  literally, per CommonMark escapes don't apply in code).
- RTF exporter fixes: `ie_exp_RTF_MsWord97ListMulti` is stored by
  `unique_ptr` (vector reallocation no longer shallow-copies its owned
  `UT_Vector` levels into dangling pointers), `addLevel` no longer
  inserts the same level pointer twice (double-free on teardown),
  null `getNthList`/`getListAtLevel` results are guarded, and a
  `-Wlogical-op` dead condition was fixed. docx→rtf conversion of the
  canonical CV now exits 0 instead of aborting in teardown.

### Ribbon UI (LibreOffice-style) + interface switcher

- `src/wp/ap/gtk/ap_UnixRibbon.{h,cpp}` added: a GtkNotebook-based
  ribbon built from `ap_Ribbon_Layouts.h`. Tabs follow LibreOffice
  Writer (File / Home / Insert / Layout / Review / View / Help plus a
  contextual Table tab); groups render as compact three-row grids.
  Every button binds to the *same* `"menu.<action>"` GAction the
  classic menubar uses, so enable/check state, edit methods and
  dynamic labels (e.g. "About abiword") stay identical.
- Help > Interface submenu added (Classic Menus / Ribbon radio items,
  `viewClassicUI`/`viewRibbonUI` edit methods, `ap_GetState_UI` state
  function). The same pair also lives in the ribbon's Help tab, so the
  mode can be changed from either UI.
- `RibbonUI` preference persists the choice; switching is live and
  keeps the document open. The ribbon refreshes on every view notify
  (same pattern as the toolbar listener), which also drives the
  contextual Table tab (shown only while the caret is in a table).
- `XAP_FrameImpl::setRibbonMode()` is a no-op default so platforms
  without a ribbon are unaffected; `EV_UnixMenu::lookupAction()`
  exposes the per-item GAction for external widgets.

### Ribbon UI preparation

- `src/wp/ap/xp/ap_Ribbon_Layouts.h` added: a data table that maps the
  existing `AP_MENU_ID_*` actions into LibreOffice-Writer-style ribbon
  tabs and groups (Home / Insert / Layout / Review / View / Help plus a
  contextual Table tab). It consumes the same EV_Menu_ActionSet and
  EV_Menu_LabelSet as the menubar, so a future ribbon widget
  (e.g. a GtkNotebook of button groups) binds identical actions,
  labels and state functions. The classic menubar stays the default.

### GTK4 dialog migration (`d206c3e`)

- **All 43 `.ui` files converted to GTK4 builder syntax**: GTK3's
  `internal-child="vbox"`/`"action_area"` structure removed (GTK4's
  builder orphans the action-area widget entirely — buttons never
  attached to the dialog). Action buttons now live inside the content
  box with `action-widget` response wiring preserved.
- Grid `<packing>` converted to `<layout>` children; removed
  `can-default`/`has-default`/`border-width`/`inconsistent` and other
  GTK3-only properties; `GtkEventBox`/`GtkMenuBar`/`GtkImageMenuItem`
  replaced (RDF editor menu → `GMenu` + `GtkPopoverMenuBar`, localized
  via the menu model).
- 12px content margins added to all dialogs (GTK4 dropped the
  implicit dialog padding).
- **Checkbutton API split**: GTK4 `GtkCheckButton` is no longer a
  `GtkToggleButton`/`GtkButton` — fixed `GTK_TOGGLE_BUTTON`/
  `gtk_toggle_button_*`/`gtk_button_set_label` misuse on checkbuttons
  in 11+ dialog sources, the shared `localizeButton*` helpers, and
  callback signatures (`s_auto_save_toggled`,
  `s_auto_colsize_toggled`).
- **Ruler fix**: `XAP_UnixCustomWidget::_fe::draw` now wraps
  `drawImmediate` in `beginFrame`/`endFrame` so the ruler (and all
  custom-widget preview panes) composite to screen — committed as
  `f4a7f2b`.
- **Table popover**: grid cells now use the widget's own style
  context (detached contexts have no theme colors in GTK4).
- Resource regeneration: `abi-resources.c/.h` now depend on the
  referenced `.ui` files.

### Resolved: ODF export "double free or corruption" (environment)

- Root cause found via `LD_PRELOAD=libasan`: a stale `opendocument.so`
  in `~/.config/abiword/abiword/plugins/` and `libabiword-3.1.so` both
  exported `ODe_Style_Style::m_NCStyleMappings`. Symbol interposition
  unified the storage while both DSOs registered a static destructor →
  `~map()` ran twice on one object. Removing the stale plugin fixed
  it (37/37 clean runs; previously ~15–30% crash rate).

### Font collection + default font (`5e2e2ed`)

- **Carlito is the default document font**: `pp_Property.cpp` default
  `font-family`, `Normal` style construction in `pt_PT_Styles.cpp`,
  the view default in `fv_View.cpp`, and all 63 `normal.awt-*`
  templates switched from Times New Roman to Carlito.
- **Bundled fonts** under `fonts/` (99 files): Carlito, Caladea, the
  Intos family (Sans/Display/Narrow/Serif, from
  github.com/muglug/intos), Liberation (+ Sans Narrow), DejaVu,
  OpenSymbol, Gentium + Gentium Book, Noto Sans/Serif, Source Sans 3 /
  Serif 4 / Code Pro, Linux Libertine / Biolinum — mirroring the
  LibreOffice/OpenOffice bundled set. Installed to
  `<AbiSuiteLibDir>/fonts` via the new `fonts/Makefile.am`.
- **Runtime registration**: `XAP_UnixApp` calls
  `FcConfigAppFontAddDir` + `FcConfigParseAndLoad` on
  `fonts/abiword-fonts.conf`, which maps common document font names
  (Calibri→Carlito, Aptos→Intos, Times New Roman→Liberation Serif,
  Arial→Liberation Sans, Courier New→Liberation Mono, …) onto the
  bundled metric-compatible fonts.
- **Docs**: historical documentation moved to `Old-Doc/`; new
  `README.md` + `CHANGELOG.md`; `AM_INIT_AUTOMAKE` switched to
  `foreign` mode since GNU-required doc files moved.

### `ed9979c` — ODF: encrypted export + save-dialog password UI

- "Encrypt with password" checkbox + password/confirm entries in the
  ODF Save/Save-As dialog (visible only for ODF formats).
- `ODc_Crypto::encrypt` + `_encryptPackage`: two-pass export — deflate
  + Blowfish CFB64 per stream, random salt/IV, PBKDF2-SHA1;
  `manifest:encryption-data` entries; `mimetype` stays plaintext.
- Password plumbing: dialog → `getEncryptionPassword()` → transient
  `PD_Document::setSavePassword()`; decrypted docs keep their password
  on re-save; `ABIWORD_PASSWORD` covers headless use.
- Cross-validated against libgcrypt as an independent implementation.
- Fix: unstyled `<text:p>` was closed as `</text:h>` in the ODF text
  listener (malformed content.xml).

### `349b528` — ODF: flat-XML import, built-in Blowfish, built-in RDF

- `.fodt` flat ODF import: sniffer + stream-rewind fix, multi-pass over
  the single XML, `office:binary-data` base64 images.
- Vendored Blowfish from OpenSSL 4.0.2 (Apache-2.0); removed the
  `#ifndef HAVE_GCRYPT` guard that blocked decryption;
  `ABIWORD_PASSWORD` env fallback.
- `ODi_RDFParser`: built-in SAX RDF/XML parser; built-in `toRDFXML`
  serializer — RDF works with no libredland. `manifest.rdf` now gets a
  manifest entry on export.

### `7ea3f4e` — ODF moved into the core library

- `opendocument` plugin removed; importer/exporter live in
  `src/wp/impexp/odf/`. Open/save/save-as for `.odt` without any plugin.

### `3deb954` — openxml fixes + xsltml restored

- Tables, images, and math inside headers/footers (listener-state
  fixes); shared MathML→LaTeX XSLT data restored for the core math path.

### `f2ec3d7` — table + RTL + RTF fixes

- Ctrl+Alt+arrow keyboard table-cell resize; drag redistribution keeps
  right cells on-page; borders no longer expose hidden margins.
- TOC survives RTF export (`fldrslt` + `PTX_SectionTOC` import).
- RTL field bidi direction + mirrored table columns.

### `6cc3ec4` — plugin/dead-code cleanup

- Removed obsolete plugins and unused source files. Remaining:
  `epub`, `grammar`, `mht`, `openxml`, `rsvg`, `wmf`, `wordperfect`,
  `wpg`.

### `bd5b115` — Debian bug fixes

- Vendored + patched libwv; surfaced open errors to the UI; default
  save format fix; dead-URL cleanup.

### Earlier commits (`105b780` … `dd113a2`)

- `UT_StringPtrMap` purge allocator mismatch + `void*` delete UB;
  launchpad crash fixes + `LC_PAPER` default page size; dead
  GTK3-bound plugins removed; `mht` made a self-contained MHTML
  importer; plugin dependencies vendored into `thirdparty/`; epub GTK3
  dialog and rsvg `unique_ptr`/array-delete fixes.

## Repository layout

| Path | Contents |
|------|----------|
| `src/` | Application and library source (GTK port) |
| `plugins/` | Remaining loadable plugins (mht, rsvg, wmf, wordperfect, wpg) |
| `fonts/` | Bundled fonts + licenses + substitution config |
| `user/` | Templates, dictionaries, clipart |
| `Old-Doc/` | Historical documentation (pre-experiment) |
| `tools/` | Development/test helpers |

Historical design documents, the old README, NEWS, ChangeLog, INSTALL
and build notes were moved to `Old-Doc/` and are kept for reference.

## Building

Standard autotools flow:

```bash
./autogen.sh          # or: autoreconf --install --force
./configure
make -C src           # builds libabiword + the abiword binary
sudo make install     # installs binary, data files, and fonts/
```

Running from the build tree without installing: set `ABIWORD_DATADIR`
to the repository root so the app picks up `<repo>/fonts`.

Headless conversions (also usable for smoke tests):

```bash
ABIWORD_DATADIR=$PWD src/abiword --to=odt input.abw -o out.odt
ABIWORD_PASSWORD=secret src/abiword --to=abw encrypted.odt -o out.abw
```

## Known issues

- The GTK4 dialog migration is in progress — `.ui` files were
  mechanically converted from GTK3 markup; some dialogs may still have
  layout or widget-type quirks.
- macOS/Windows GTK4 builds not yet verified.

### Resolved: ODF export teardown crash

The intermittent "double free or corruption" after `.odt` export was
**not** in the exporter. A stale `opendocument.so` plugin binary (from
before ODF moved into the core library) in
`~/.config/abiword/abiword/plugins/` exported the same
`ODe_Style_Style::m_NCStyleMappings` symbol as `libabiword`. The
dynamic linker unified the symbol but both DSOs registered a static
destructor, so `~map()` ran twice on one object. If you built from an
older tree, delete leftover `opendocument.so` files from the plugin
directory.

## Libraries and references

The project builds on, vendors, or took design cues from the
following projects:

**Design references**

- [LibreOffice](https://www.libreoffice.org/) — the ribbon UI is
  modelled on Writer's NotebookBar
  (`sw/uiconfig/swriter/ui/notebookbar.ui`), and the status bar,
  font selector, ruler and Paste split-button follow its behaviour.
- [CommonMark](https://commonmark.org/) and the
  [Zettlr Markdown Compendium](https://docs.zettlr.com/en/editor/markdown-compendium.html)
  — Markdown import/export syntax.
- [LaTeX project](https://www.latex-project.org/) — `.tex`
  import/export syntax.
- [EPUB 3.3 specification](https://www.w3.org/TR/epub-33/) — ebook
  import/export.

**Build dependencies** (system, via pkg-config)

- [GTK 4](https://gtk.org/) + gtk4-unix-print — UI toolkit
- [GLib](https://docs.gtk.org/glib/) / GIO — core platform library
- [Pango](https://pango.gnome.org/) — text shaping
- [cairo](https://cairographics.org/) (pdf/ps/fc/pangocairo) —
  rendering and PDF/PS export
- [libgsf](https://gitlab.gnome.org/GNOME/libgsf) — OLE2/ZIP
  container I/O (`.doc`, `.docx`, `.epub`)
- [fontconfig](https://www.freedesktop.org/wiki/Software/fontconfig/)
  — bundled-font registration and substitution
- [FriBidi](https://github.com/fribidi/fribidi) — bidirectional text
- [libxslt](https://gitlab.gnome.org/GNOME/libxslt) — XSLT
  (MathML↔LaTeX stylesheets)
- [zlib](https://zlib.net/), [libpng](https://libpng.org/),
  [libjpeg](https://ijg.org/) — image/archive support
- [enchant-2](https://rrthomas.github.io/enchant/) — spell-checker
  abstraction
- [librsvg](https://gitlab.gnome.org/GNOME/librsvg) — SVG rendering
  (rsvg plugin)
- [Boost](https://www.boost.org/) headers
- X11 — X11/XWayland platform glue

**Vendored in `thirdparty/` / `fonts/`**

- [Hunspell 1.7.0](https://hunspell.github.io/) — spell/grammar
  checking (`thirdparty/hunspell-*`)
- [wv 1.2.9](https://github.com/AbiWord/wv) — MS Word `.doc` import,
  patched for the reported buffer overflows
- [libwpd](https://libwpd.sourceforge.io/) +
  [libwps](https://libwps.sourceforge.io/) — WordPerfect/MS Works
  import (wordperfect plugin)
- Blowfish CFB64 — vendored from
  [OpenSSL](https://www.openssl.org/) (Apache-2.0) for encrypted ODF
- [xsltml](http://xsltml.sourceforge.net/) — MathML→LaTeX XSLT
  stylesheets
- Bundled fonts (`fonts/`): [Carlito](https://github.com/googlefonts/carlito)
  (Calibri-compatible), [Caladea](https://github.com/googlefonts/caladea)
  (Cambria-compatible), Intos (Aptos-compatible), Liberation
  (Arial/Times New Roman/Courier New), DejaVu, OpenSymbol, Gentium,
  Noto, Source Sans/Serif/Code, Linux Libertine/Biolinum — see
  `fonts/README.md` for provenance and licenses.

## License

AbiWord itself remains under its original license — see `COPYING` and
`COPYRIGHT.TXT`. Third-party bundled fonts carry their own licenses
(mostly OFL 1.1) in `fonts/<family>/`; provenance is documented in
`fonts/README.md`. Vendored Blowfish code is Apache-2.0 (see
`src/wp/impexp/odf/common/xp/blowfish/`).
