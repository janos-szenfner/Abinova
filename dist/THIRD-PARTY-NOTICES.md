# Third-Party Notices and License Audit (LIC01)

Abinova itself is distributed under the GNU General Public License,
version 2 or later (see `COPYING`). This document records the audit of
every bundled third-party component, artwork gallery, font, generated
asset, and vendored library, together with the license each ships
under and where its license text lives in the install tree.

Audit date: 2026-10-08. Method: every `thirdparty/` directory, every
`artwork/`/`icons/`/`fonts/`/`user/wp/clipart/` asset directory, all
generated resources, and all bundle/packaging scripts were inspected
for license files and provenance.

## 1. Vendored libraries (`thirdparty/`)

All vendored libraries are convenience libraries statically linked
into `libabinova`. Their license texts are installed with the
application under `<datadir>/licenses/<name>/` (see §8).

| Directory | Version | Upstream | License | License files |
|---|---|---|---|---|
| hunspell-1.7.4 | 1.7.4 | hunspell.github.io | MPL-1.1 / GPL-2.0+ / LGPL-2.1+ (tri-license) | `COPYING`, `COPYING.LESSER`, `COPYING.MPL`, `license.hunspell`, `license.myspell` |
| librevenge-0.0.6 | 0.0.6 | sourceforge.net/p/libwpd | LGPL-2.1+ / MPL-2.0 | `COPYING.LGPL`, `COPYING.MPL` |
| libwpd-0.10.3 | 0.10.3 | libwpd.sourceforge.net | LGPL-2.1+ / MPL-2.0 | `COPYING.LGPL`, `COPYING.MPL` |
| libwpg-0.3.4 | 0.3.4 | libwpg.sourceforge.net | LGPL-2.1+ / MPL-2.0 | `COPYING.LGPL`, `COPYING.MPL` |
| libwps-0.4.14 | 0.4.14 | libwps.sourceforge.net | LGPL-2.1+ / MPL-2.0 | `COPYING.LGPL`, `COPYING.MPL` |
| wv-1.2.9 | 1.2.9 | wvware.sourceforge.net | GPL-2.0+ | `COPYING` (restored from the upstream tarball in LIC01) |

Provenance, tarball checksums, and the local patch series for each
vendored library are recorded in `thirdparty/VENDORED.json`;
`tools/vendor-check.py` verifies the tree against upstream.

## 2. Bundled artwork (`artwork/`)

| Component | Source | License | License file |
|---|---|---|---|
| `artwork/icons/` (299 SVGs + `index.txt`) | Lucide icons (lucide.dev) | ISC | `artwork/lucide-LICENSE.txt` |
| `artwork/3d/` (109 PNGs) | Microsoft FluentUI Emoji (github.com/microsoft/fluentui-emoji) | MIT | `artwork/fluentui-emoji-LICENSE.txt` |
| `artwork/shapes/` (93 SVGs in 6 galleries) | LibreOffice `breeze_svg` icon theme (`icon-themes/breeze_svg/cmd/lc_*.svg`), KDE Breeze by the KDE Visual Design Group; rescaled to 32px with an added overlay path | GPL-2.0+ | `artwork/shapes/LICENSE.txt` (provenance note + full GPL-2.0 text) |

## 3. Application icons (`icons/`)

The Abinova app icon set (`hicolor_apps_*_abinova.*`, `abinova.icns`,
`abinova.ico`) is original artwork created for this project, GPL-2.0+.
See `icons/README`.

## 4. Clipart (`user/wp/clipart/`)

27 PNG images inherited verbatim from the AbiWord source tree
(GPL-2.0+). `tux_bordelais.png` is Larry Ewing's Tux, used under his
acknowledgment notice, reproduced in `user/wp/clipart/README`. The
gallery installs only with `--enable-clipart`.

## 5. Bundled fonts (`fonts/`)

Every shipped font directory contains its license file, installed into
`<datadir>/fonts/` via `nobase_dist_fonts_DATA`:

| Directory | License file | License |
|---|---|---|
| `caladea/` | `OFL.txt` | SIL Open Font License 1.1 |
| `carlito/` | `OFL.txt` | SIL OFL 1.1 (RFN: Carlito) |
| `dejavu/` | `LICENSE` | Bitstream Vera license + public-domain additions |
| `gentium/` | `OFL.txt` | SIL OFL 1.1 (RFN: Gentium, SIL) |
| `intos/` | `LICENSE.txt` | SIL OFL 1.1 |
| `liberation/` | `COPYRIGHT`, `License-Narrow.txt` | SIL OFL 1.1 (Liberation); GPL-2.0+ with font exception (Liberation Narrow) |
| `libertine/` | `COPYRIGHT` | Debian copyright-format file (GPL/OFL per file set) |
| `noto/` | `OFL.txt` | SIL OFL 1.1 |
| `opensymbol/` | `COPYRIGHT` | Debian copyright-format file (LGPL/MPL per upstream) |
| `source/` | `LICENSE-Source*.md` | SIL OFL 1.1 (RFN: Source) |

## 6. Other vendored content

| Component | License | Note |
|---|---|---|
| `xsltml/` (MathML→LaTeX XSLT) | MIT-style, © 2001–2003 Vasil Yaroshevich | license text in `xsltml/README`, installed via `xsltml_DATA` |
| `src/wp/impexp/odf/common/xp/crypto/blowfish/` | Apache-2.0 (OpenSSL Project) | `LICENSE.txt` in-tree, installed to `<datadir>/licenses/openssl-blowfish/` |
| `src/wp/impexp/odf/common/xp/crypto/` (sha1, hmac, pbkdf2, memxor) | GPL-2.0+ (gnulib, FSF) | per-file GPL headers; same license as the project |
| `tools/lcov/` (build-time only) | GPL-2.0+ | `tools/lcov/COPYING`; developer tooling, not shipped in bundles |

## 7. Generated and embedded assets

- `src/wp/ap/xp/ap_wp_sidebar_static.cpp` — embedded sidebar PNG
  (Inkscape-authored), imported with the AbiWord 3.1.90 snapshot;
  GPL-2.0+ via `COPYING`.
- `test/` fixtures and `fuzz/regress/` crashers — AbiWord test corpus
  files and locally generated reproducers; GPL-2.0+ via `COPYING`.
- `user/wp/templates/` (`.awt` files) — project-authored templates;
  GPL-2.0+ via `COPYING`.
- Header/footer ribbon presets (`FV_HdrLine` tables in
  `fv_View_cmd.cpp`) — original code; the preset names follow Word's
  gallery as functional labels, but the layouts are simple text/rule
  arrangements written in code, containing no copied assets.

**Removed in LIC01 (open-source-clean remediation):** the 17
frame-based cover-page templates (`src/wp/covers/*.xml` +
`thumbs/*.png`) and `tools/mkcovers.py`. Audit found they were
mechanical conversions of Microsoft Word's built-in cover-page `.docx`
files — including embedded Microsoft raster artwork — with no
redistribution grant. `FV_View::cmdInsertCoverPage` retains only the
four code-generated presets (Frame, Motion, Sideline, Yearly), which
are original property-line designs licensed GPL-2.0+.

## 8. How notices ship in bundles

`make install` (`install-data-local` in `Makefile.am`) installs:

- `COPYING`, `COPYRIGHT.TXT`, and `dist/THIRD-PARTY-NOTICES.md` into
  `<datadir>/`;
- each vendored library's `COPYING*`/`license*` files into
  `<datadir>/licenses/<lib>/`;
- `artwork/` wholesale (carrying all three gallery license files);
- `fonts/` with per-directory license files; `xsltml/README`;
- `user/wp/clipart/README` when clipart is enabled.

All bundle packagers (`dist/linux-bundle.sh`, `tools/build-macos.sh`,
`tools/build-windows-msys2.sh`, the Flatpak manifest) stage a
`make install` tree and flatten `<datadir>/` into the bundle, so every
notice above reaches the shipped artifacts on all platforms.

## 9. Compatibility statement

Main license GPL-2.0+ vs. bundled components:

- GPL-2.0+ (wv, Breeze shapes, clipart, project assets): same license.
- LGPL-2.1+ (libwpd/libwpg/libwps/librevenge, Hunspell option):
  permissive for linking; LGPL notices + license texts ship per §8.
- MPL-1.1/MPL-2.0 (Hunspell, librevenge stack): weak copyleft;
  file-level only, compatible with GPL distribution; MPL texts ship.
- ISC/MIT (Lucide, FluentUI emoji, xsltml): permissive; required
  notices ship verbatim in the tree and bundles.
- Apache-2.0 (OpenSSL Blowfish): Apache-2.0 is GPLv3-compatible;
  because Abinova is GPL-2.0-**or-later**, the combination is
  distributable under GPLv3 terms where the licenses are compatible.
  The Apache-2.0 text ships in `licenses/openssl-blowfish/`.
- SIL OFL 1.1 (fonts): designed for bundling alongside GPL software;
  Reserved Font Name obligations affect derivative fonts only, not
  the application; each font ships its `OFL.txt`.
- Liberation Narrow (GPL-2.0+ with font exception) and DejaVu
  (public-domain additions over Bitstream Vera license): compatible.

**Result: no non-free, closed, or unknown-license component remains in
the tree.** The only unlicensed-content finding (Microsoft-derived
cover templates) was remediated by removal, documented in §7.
