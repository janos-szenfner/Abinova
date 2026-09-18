# Changes in this experiment

Per-commit log of the modifications made in this fork, newest first.
Older upstream history is not listed here.

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
