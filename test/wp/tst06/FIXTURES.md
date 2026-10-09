# TST06 — golden-image render regression suite

`src/text/fmt/xp/t/fv_GoldenCovers.t.cpp` paints page 0 of a headless
`FV_View` into an ARGB32 image surface and diffs the raster against the
reference PNGs in `goldens/`.  A render regression — a lost fill, a
missing custGeom band, a stray opaque overlay like the COVER04 seal —
moves far more pixels than the per-golden tolerance; the suite also
proves the diff is not degenerate by requiring a different preset's
render to *exceed* the tolerance.

## Files

- `goldens/cover-<preset>.png` — render of `cmdInsertCoverPage(<preset>)`
  for each of the 21 presets, at 50% zoom.
- `goldens/docx-<preset>.png` — render of the imported tst05 corpus docx
  (`COVER_FIXTURES_DIR` required; that corpus is not committed).
- `goldens/toc.png`, `goldens/table.png` — TOC and table renders.

## Regenerating

    ABINOVA_REGEN_GOLDENS=1 ./Abinova-test "cover preset" "golden diff" \
        "toc and table" "docx cover"

Run from `src/wp/test` with the usual test env.  A missing golden
**fails** without the flag — regen is a deliberate act, never silent.

## Tolerances

- Goldens: 6% of pixels may differ (channel delta > 40).  The slack
  absorbs the `@date` line ("Month YYYY") in whip/semaphore/ion whose
  glyphs drift monthly, plus antialiasing noise; a same-pipeline render
  normally diffs at <1%.
- docx-vs-preset composition: both renders are downscaled to a
  128x166 tile and compared at a per-pair tolerance baked into
  `s_pairs` (~2x the measured diff).  Loosest are `integral` (50%,
  photographic asset vs flat vector approximation) and `crop` /
  `ion-dark` (35%, deliberately simplified block geometry).
