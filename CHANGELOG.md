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

### User interface

- **LibreOffice-style ribbon UI** — `GtkNotebook` ribbon built from
  `ap_Ribbon_Layouts.h` with File / Home / Insert / Layout / Review /
  View / Help tabs plus a contextual Table tab (shown only while the
  caret is in a table); compact three-row group grids; every button
  binds to the same `menu.<action>` GAction as the menubar, so state,
  edit methods and dynamic labels are identical.
- **Interface switcher** — Help → Interface submenu (Classic Menus /
  Ribbon radio items) in both UIs; `RibbonUI` preference persists the
  choice; switching is live.
- **LibreOffice-style status bar** — `Page: n/m`, live
  `N words, N characters`, current paragraph style, insert/overwrite
  and input-mode indicators, document language, and a zoom cluster
  (`−` / slider / `+` / `NNN%` / 100% reset).
- **LibreOffice-style font box** — editable `GtkEntry` + dropdown
  arrow; type a name and Enter to apply; lazy `GtkDropDown` +
  `GtkSortListModel` list where every visible row renders in its own
  typeface with type-to-search (only visible rows load fonts — the old
  cell renderer measured ~2000 fonts on popup open and froze the UI).
- **Page centering** — pages center horizontally when narrower than
  the window (LibreOffice behaviour); no phantom scrollbar.
- **Ruler redesign** — full-height bar, gray margin bands, white text
  band, bottom-anchored long/short tick hierarchy, zoom-exempt GUI-font
  numeric labels, flat triangle indent markers, black foreground text.
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
