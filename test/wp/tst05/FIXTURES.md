# TST05 — DOCX fidelity corpus fixtures

`src/wp/impexp/xp/t/ie_covercorpus.t.cpp` imports each file below and
asserts the listed expectations against the re-exported `.abwn` markup.

The fixtures are Microsoft-template-derived `.docx` files and are **not
committed** (licensing).  Point `COVER_FIXTURES_DIR` at the directory
holding them (e.g. `~/Documents`); when the variable is unset or a file
is missing the test prints `SKIP` and passes trivially.

| fixture | expectations asserted |
|---------|-----------------------|
| badge-footer.docx | `<section type="footer">` + `header` struxes; custGeom seal frame (`shape-path` + `background-color:4472C4`); `page_number` field carrying run props (`font-weight:bold`, `char-spacing:1pt`, `color:E7E6E6`) — COVER06/07/08 |
| crop-header.docx | header+footer struxes; `page_number` field; >=2 frames; `44546A` gray band; corner band is a `shape-path` custGeom |
| integral-footer.docx | header+footer struxes; `page_number` field; `ED7D31` orange badge paragraph bgcolor |
| viewmaster-footer-horizontal.docx | header+footer struxes; `page_number` field; >=3 frames; `000000` band |
| viewmaster-footer-vertical.docx | same as horizontal |
| feathered.docx | `frame-type:image` frame with `strux-image-dataid` + `image/png` data item; image bottom (`frame-page-ypos`+`frame-height`) reaches >=10.9in of the 11in page (COVER06); >=2 `shape-path` frames |
| facet.docx | blue base band = `shape-path` frame with `background-color:4472C4`; overlay = `frame-type:image` with `strux-image-dataid`; `image/png` data item (COVER05) |
| whip.docx | >=20 `shape-path` frames (source has 23 `<a:custGeom>`); >=25 frames total |
| Badge.docx | `shape-path` seal; `char-spacing:8pt` tracked title (D01); `4472C4` |
| filgree.docx | exactly 2 inline `<image>` + 2 `image/png` data items; both carry `image-duotone:123065 FFFFFF` (D06) |
| integral.docx | >=1 inline `<image>` + image data item |
| Austin.docx | >=6 frames, `[Document title]` text |
| banded.docx | >=3 frames, `[Document title]` text |
| Crop.docx | >=5 frames, >=2 `shape-path`, `[Document Title]` |
| headiness.docx | >=3 frames, `[Document Title]` |
| ion-dark.docx | >=5 frames, >=2 `shape-path`, `[Document title]` |
| ion-light.docx | >=2 frames, `[Document title]` |
| retrospect.docx | >=3 frames, `[Document title]` |
| semaphore.docx | >=5 frames, `[Document title]` |
| slice-dark.docx | >=2 frames, `[Document title]` |
| slice-light.docx | >=2 frames, `[Document title]` |
| viewmaster.docx | >=3 frames, `[Document title]` |

## Known gaps (asserted-at-current-behavior, not the ideal)

- slice-dark/slice-light declare 5 `<a:custGeom>` shapes each inside a
  `wpg:wgp` group under `mc:AlternateContent`; grouped custGeom children
  are not imported today, so the sweep only pins their textbox frames.
- badge-footer.docx's `Rectangle 49` accent1 band (footer2.xml) does not
  survive import; the seal + styled page field do.
