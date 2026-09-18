# Changes in this experiment

Per-commit log of the modifications made in this fork, newest first.
Older upstream history is not listed here.

## Font collection + default font (this commit)

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
