# AbiWord GTK4 Experiment

An experimental fork of the AbiWord word processor, focused on:

- **Built-in OpenDocument (ODT/ODF) support** — import, export, flat
  XML (`.fodt`), encryption, and RDF metadata implemented in the core
  library rather than as a plugin.
- **Bundled fonts** — a curated, redistributable font collection is
  installed with the application and registered with fontconfig at
  startup, so documents render consistently without relying on
  system-installed fonts.
- **Built-in Markdown support** — open, edit, save and save-as for
  `.md` files, implemented in the core import/export library (not a
  plugin).
- **Repository cleanup** — obsolete plugins and dead files removed;
  Debian-reported bugs fixed against the actual implementation.

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
- Remaining plugins: `epub`, `grammar`, `mht`, `openxml`, `rsvg`,
  `wmf`, `wordperfect`, `wpg`.
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
  - Export writes the same constructs back, so a document round-trips
    through Markdown without losing its formatting structure.
- **EPUB plugin modernized to EPUB 3.3**: `version="3.0"` packages
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
- **Grammar checker switched to Hunspell**: the grammar plugin no
  longer uses link-grammar. A vendored `hunspell-1.7.0` is built in
  `thirdparty/` and the plugin's sentence walker now flags each
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
  imported and converted to HTML (`help/`, 220 pages); Help buttons
  open the local copy instead of a dead `file://` URL or the website.
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

See `CHANGES.md` for the per-commit modification log.

## Repository layout

| Path | Contents |
|------|----------|
| `src/` | Application and library source (GTK port) |
| `plugins/` | Remaining loadable plugins |
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

## License

AbiWord itself remains under its original license — see `COPYING` and
`COPYRIGHT.TXT`. Third-party bundled fonts carry their own licenses
(mostly OFL 1.1) in `fonts/<family>/`; provenance is documented in
`fonts/README.md`. Vendored Blowfish code is Apache-2.0 (see
`src/wp/impexp/odf/common/xp/blowfish/`).
