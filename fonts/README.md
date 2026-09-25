# Bundled fonts

Abinova ships a set of freely redistributable fonts so that documents
render consistently on systems without the usual desktop font
collection installed. The fonts are installed under
`<AbiSuiteLibDir>/fonts` and registered with fontconfig at startup
(`src/af/xap/gtk/xap_UnixApp.cpp`).

The collection matches the font set bundled with LibreOffice/OpenOffice
(with the addition of the Intos family) so that documents exchanged
with those suites resolve the same metric-compatible fallback fonts.

## Families

| Directory    | Fonts                                              | Purpose / metric compatibility        | License     |
|--------------|----------------------------------------------------|----------------------------------------|-------------|
| `carlito`    | Carlito (Regular, Bold, Italic, Bold Italic)       | Calibri-compatible; default font      | OFL 1.1     |
| `caladea`    | Caladea (Regular, Bold, Italic, Bold Italic)       | Cambria-compatible                     | OFL 1.1     |
| `intos`      | Intos, Intos Display, Intos Narrow, Intos Serif    | Aptos-compatible                       | OFL 1.1     |
| `liberation` | Liberation Sans, Serif, Mono, Sans Narrow          | Arial / Times New Roman / Courier New / Arial Narrow-compatible | OFL 1.1 / custom (Narrow) |
| `dejavu`     | DejaVu Sans, Sans Condensed, Sans Mono, Serif, Serif Condensed | Wide Unicode coverage | custom permissive |
| `opensymbol` | OpenSymbol                                         | Symbol/bullet glyphs used by ODF suites | OFL 1.1    |
| `gentium`    | Gentium, Gentium Book                              | Document serif faces                   | OFL 1.1     |
| `noto`       | Noto Sans, Noto Serif                              | Google's core text families            | OFL 1.1     |
| `source`     | Source Sans 3, Source Serif 4, Source Code Pro     | Adobe's open text families             | OFL 1.1     |
| `libertine`  | Linux Libertine, Linux Biolinum                    | Serif/sans text families               | dual OFL 1.1 / GPL+FE |

Each family directory contains its license file.

## Provenance

- Carlito, Caladea — google/fonts repository (`ofl/carlito`, `ofl/caladea`)
- Intos — https://github.com/muglug/intos (`fonts/`, OFL-1.1)
- Liberation — liberationfonts releases 2.1.5; Sans Narrow 1.07.5
- DejaVu — dejavu-fonts 2.37
- OpenSymbol — LibreOffice `fonts-opensymbol` package (LibO 24.2)
- Gentium, Gentium Book — SIL font-gentium 7.000
- Noto Sans/Serif — notofonts hinted statics
- Source Sans 3 / Serif 4 / Code Pro — adobe-fonts releases 3.052R / 4.005R / 2.042R
- Linux Libertine / Biolinum — fonts-linuxlibertine 5.3.0

## Runtime registration

`XAP_UnixApp` calls `FcConfigAppFontAddDir()` on
`<AbiSuiteLibDir>/fonts` during startup. When running from the build
tree without installing, set `ABIWORD_DATADIR` to the repository root
so `<repo>/fonts` is picked up.
