# Changes in this experiment

Per-commit log of the modifications made in this fork, newest first.
Older upstream history is not listed here.

## Object grouping and rotation (Word-style Group/Rotate)

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

## File ribbon tab: large icon buttons, "Save a Copy" removed

- File tab groups (Document, Print) now use the Help-tab style —
  large icon-over-label buttons instead of the compact text list.
- Added icon mappings for "New using Template"
  (x-office-document-template) and "Page Setup"
  (document-page-setup).
- "Save a Copy" (AP_MENU_ID_FILE_EXPORT) dropped from the ribbon —
  duplicate of Save / Save As.

## Word-compatible styles, internal help window, visible group separators

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

## Borders dropdown and docked Styles pane

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

## Paragraph group redesign, list libraries, paragraph sort

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

## Font box with inline search, Change Case dropdown, colour pickers, eased scrolling

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

## Ribbon Font group redesign, selection-offset fix, finer scrolling, Mermaid rendering

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

## Paste split button, DOCX layout fidelity, Markdown coverage

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

## Ribbon UI (LibreOffice-style) + interface switcher

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

## Ribbon UI preparation

- `src/wp/ap/xp/ap_Ribbon_Layouts.h` added: a data table that maps the
  existing `AP_MENU_ID_*` actions into LibreOffice-Writer-style ribbon
  tabs and groups (Home / Insert / Layout / Review / View / Help plus a
  contextual Table tab). It consumes the same EV_Menu_ActionSet and
  EV_Menu_LabelSet as the menubar, so a future ribbon widget
  (e.g. a GtkNotebook of button groups) binds identical actions,
  labels and state functions. The classic menubar stays the default.

## GTK4 dialog migration (`d206c3e`)

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

## Resolved: ODF export "double free or corruption" (environment)

- Root cause found via `LD_PRELOAD=libasan`: a stale `opendocument.so`
  in `~/.config/abiword/abiword/plugins/` and `libabiword-3.1.so` both
  exported `ODe_Style_Style::m_NCStyleMappings`. Symbol interposition
  unified the storage while both DSOs registered a static destructor →
  `~map()` ran twice on one object. Removing the stale plugin fixed
  it (37/37 clean runs; previously ~15–30% crash rate).

## Font collection + default font (`5e2e2ed`)

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
  `README.md` + this file; `AM_INIT_AUTOMAKE` switched to `foreign`
  mode since GNU-required doc files moved.

## `ed9979c` — ODF: encrypted export + save-dialog password UI

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

## `349b528` — ODF: flat-XML import, built-in Blowfish, built-in RDF

- `.fodt` flat ODF import: sniffer + stream-rewind fix, multi-pass over
  the single XML, `office:binary-data` base64 images.
- Vendored Blowfish from OpenSSL 4.0.2 (Apache-2.0); removed the
  `#ifndef HAVE_GCRYPT` guard that blocked decryption;
  `ABIWORD_PASSWORD` env fallback.
- `ODi_RDFParser`: built-in SAX RDF/XML parser; built-in `toRDFXML`
  serializer — RDF works with no libredland. `manifest.rdf` now gets a
  manifest entry on export.

## `7ea3f4e` — ODF moved into the core library

- `opendocument` plugin removed; importer/exporter live in
  `src/wp/impexp/odf/`. Open/save/save-as for `.odt` without any plugin.

## `3deb954` — openxml fixes + xsltml restored

- Tables, images, and math inside headers/footers (listener-state
  fixes); shared MathML→LaTeX XSLT data restored for the core math path.

## `f2ec3d7` — table + RTL + RTF fixes

- Ctrl+Alt+arrow keyboard table-cell resize; drag redistribution keeps
  right cells on-page; borders no longer expose hidden margins.
- TOC survives RTF export (`fldrslt` + `PTX_SectionTOC` import).
- RTL field bidi direction + mirrored table columns.

## `6cc3ec4` — plugin/dead-code cleanup

- Removed obsolete plugins and unused source files. Remaining:
  `epub`, `grammar`, `mht`, `openxml`, `rsvg`, `wmf`, `wordperfect`,
  `wpg`.

## `bd5b115` — Debian bug fixes

- Vendored + patched libwv; surfaced open errors to the UI; default
  save format fix; dead-URL cleanup.

## Earlier commits (`105b780` … `dd113a2`)

- `UT_StringPtrMap` purge allocator mismatch + `void*` delete UB;
  launchpad crash fixes + `LC_PAPER` default page size; dead
  GTK3-bound plugins removed; `mht` made a self-contained MHTML
  importer; plugin dependencies vendored into `thirdparty/`; epub GTK3
  dialog and rsvg `unique_ptr`/array-delete fixes.
