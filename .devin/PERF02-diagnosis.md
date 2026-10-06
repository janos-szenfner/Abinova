# PERF02 — PERF01 root-cause diagnosis (performance sanity pass)

Status: diagnosis complete — fix recipes below are for PERF03. No
implementation was done in this task.

Artifacts recovered from `/tmp/perf01/` (all reusable by PERF03):

- `mkbig.py` / `mkbig2.py` — synthetic mixed-content .abw generator
  (paragraphs + heading every ~90th, 6-cell table every ~300th, inline
  PNG every ~250th).
- Fixtures: `q.abw` 2,375 paras / 119 pages / 0.37 MB;
  `h.abw` 4,750 / 237p / 0.73 MB; `big.abw` 9,500 / 474p / 1.47 MB;
  `xxl.abw` 19,000 / 948p / 2.9 MB.
- Callgrind dumps: `cg.out` (q), `cg-h.out` (h), `cg-big.out` (big),
  `cg-key.out` (2 typed keystrokes at end of h.abw via `perf_drive`).
- Analysis scripts: `analyze.py` (self-Ir + call counts + edges),
  `diffcg*.py`/`diffbig.py`/`diffname.py` (scaling diffs),
  `callers*.py` (written this run — caller extraction),
  `a-{q,h,big,key}.txt`, `tree-*.txt`, `tb-big.txt` (annotate dumps).
- `perf_drive.cpp`/`perf_drive` — xvfb harness measuring
  loadDocument / settle / per-keystroke event+pump latency.
- `startup*.sh`, `run-startup.sh`, logs — startup-to-first-window runs.
- PERFDBG instrumentation left in the tree (now committed):
  `fl_SectionLayout.cpp::updateLayout` and
  `fv_View_protected.cpp::{_generalUpdate,_charInsert}` print phase
  timings to stderr. Keep until PERF03 verifies, then strip.

## Measured scaling — `abinova --to=pdf` convert

| fixture | paras | pages | wall (s) | wall ×/2x-size | callgrind Ir (G) | Ir ratio |
|---------|-------|-------|----------|----------------|------------------|----------|
| q       | 2,375 | 119   | 8.18     | —              | 20.70            | —        |
| h       | 4,750 | 237   | 17.71    | 2.17           | 42.75            | 2.06     |
| big     | 9,500 | 474   | 42.52    | 2.40           | 99.87            | 2.34     |
| xxl     | 19,000| 948   | 138.37   | 3.25           | —                | —        |

The per-doubling ratio climbs steadily toward 4 — the signature of a
quadratic component progressively dominating a linear baseline. This is
NOT machine noise: the same trend holds in instruction counts (Ir).

PERFDBG phase timings on big (wall): `updateLayout` fmtphase 3.72 s,
`breakSection` 1.17 s of 42.5 s total — the convert bulk is elsewhere;
see below.

## Confirmed quadratic #1 — `FL_DocLayout::findPage` and everything
##   that derives page order from it (THE dominant defect)

`findPage` (fl_DocLayout.cpp:2399) linear-scans `m_vecPages`
(`std::vector<std::unique_ptr<fp_Page>>`). Page neighbour links were
deliberately removed in favour of "derive from the vector", so:

- `fp_Page::getNext()` / `getPrev()` (fp_Page.cpp:1194/1204),
  `getPageNumber()` (:933), `setPageNumberInFrames()` (:210) each pay an
  O(pages) scan.
- `FV_View::_getPageXandYOffset` (fv_View_protected.cpp:1518) — hit for
  every `getPageScreenOffsets`/`getPageYOffset`, i.e. per drawn
  container/page — does `findPage` + `getMaxHeight` +
  `getWidthPrevPagesInRow` + `getPageViewLeftMargin` →
  `getMaxPageRowWidth`.
- `getMaxPageRowWidth` (fv_View.cpp:16825) loops rows calling
  `getWidthPagesInRow` (:16793), which calls `findPage` AND
  `getWidthPrevPagesInRow` (:16744), whose row-walk hops through
  `getNext()` — each hop O(pages). With `m_iNumHorizPages > 1` every
  geometry query costs O(pages × pages-per-row); the per-page outer loop
  makes it O(pages²) per sweep, and the sweep is re-issued per redraw
  item.
- `setFramePageNumbers` (fl_DocLayout.cpp:1093) loops pages
  `iStartPage..countPages` calling `setPageNumberInFrames` → inner
  `findPage`: O(pages²) per call, and it is called once per column
  transition inside `_breakSection` (fb_ColumnBreaker.cpp:682,1484) plus
  on add/deletePage (fl_DocLayout.cpp:2464,2507). On big: **1,164 calls,
  7.44 G Ir** (~6.4 M Ir each).

Numbers:

| metric                          | q      | h      | big    |
|---------------------------------|--------|--------|--------|
| findPage inclusive Ir (G)       | 0.39   | 1.82   | 11.54  |
| share of total                  | 1.9 %  | 4.3 %  | 11.6 % |
| growth per doc doubling         | —      | 4.65x  | 6.35x  |
| page scans (predicate evals)    |        |        | 70.3 M |
| findPage calls                  |        |        | 454 k  |

Interactive run (`cg-key`, 2 keystrokes into h.abw under perf_drive):
**findPage inclusive = 162 G Ir ≈ 93 % of the 174.8 G run**,
8,505,604 calls / 981 M page-vector element visits. Caller breakdown:

- `fp_Page::getNext()` — 6.78 M calls into findPage
  ← `FV_View::getWidthPrevPagesInRow` (6.58 M), `getMaxHeight` (119 k),
  `_getPageForXY`, `fp_Page::_reformatColumns`, `getNewContainer`,
  statusbar.
- `getWidthPagesInRow` — 1.63 M ← `getMaxPageRowWidth` (1.63 M).
- `_getPageXandYOffset` — 79 k ← `getPageScreenOffsets` (78.3 k).
- `setPageNumberInFrames` 948, `setNeedsSectionBreak` 10 k,
  `_breakSection` 240, `getPageNumber` 1.7 k.

perf_drive timings on h.abw (callgrind-inflated): loadDocument 163 s;
keystroke[0] event 48.6 s + pump 483 s; keystroke[1] 27.7 s + 9.1 s.
Even allowing ~20-30x callgrind slowdown this is multiple seconds of
real latency per keystroke on a 237-page doc.

## Confirmed quadratic #2 — per-container O(n) ops in the column-fill
##   loop of `_breakSection`

`fb_ColumnBreaker::_breakSection`'s fill loop (fb_ColumnBreaker.cpp
~1158–1300) walks containers one-by-one and, per container, executes up
to three `fp_Container::findCon` scans (:1185, :1198, :1282 ×2) plus
`removeContainer` (fp_Column.cpp:818 — findCon + `vector::erase`
memmove) and `insertContainerAfter`. During initial pagination the
source column holds the section's whole unbroken remainder, so each
move scans/shifts O(containers) → O(containers²) per section break.

Numbers (element compares inside the fp_ContainerObject vector scan):

| metric                | q      | h       | big     |
|-----------------------|--------|---------|---------|
| findCon iterations    | 8.93 M | 34.5 M  | 136.4 M |
| growth per doubling   | —      | 3.87x   | 3.95x   |
| findCon calls         | 33.1 k | 66.2 k  | 158.6 k |
| inclusive Ir          | 0.55 G | 2.10 G  | 8.26 G  |

Also quadratic-adjacent: `__memcpy_avx_unaligned_erms` self-Ir grows
2.76x/3.23x per doubling while call count grows only 1.9x — per-call
memmove size grows, consistent with erase() on ever-larger vectors.

Same class at the layout level: `fl_BlockLayout::format` recomputes
`m_iLinePosInContainer = findCon(pLine)+1` (fl_BlockLayout.cpp:3089,
3634, 3692) once per formatted block.

## Confirmed quadratic #3 — `m_vecFormatLayout` dedup scans

`fl_DocSectionLayout::setNeedsReformat` linear-scans the vector before
push_back (fl_SectionLayout.cpp:182); `_clearNeedsReformat`
find+erase-loops (:158–174); `updateLayout` rescans it per formatted
block (:1906). During populate/full-format the list grows to ~#blocks
→ O(blocks²): 24.1 M → 96.6 M compares h→big (4.01x per doubling).

## Dominant constant factor (linear, but the biggest single block)

`PP_evalProperty` — 2.87 M calls, **30.7 G inclusive (30.8 %)** of the
big convert, reached via `fp_Run::lookupProperties` (155.7 k calls,
26.2 G) ← `fb_LineBreaker::breakParagraph` ← `fl_BlockLayout::format` —
i.e. ~18 property evaluations per run per format pass. `findFont`
139.7 k calls / 0.48 G. Not quadratic but the top inclusive cost;
caching resolved per-run props or memoizing prop evals across a format
pass is worth ~a third of layout.

Genuinely linear and fine: `PD_StruxIterator::_findFrag` (9.74 M calls,
2.0x scaling), `GR_CairoGraphics::_measureExtent` (602 k, 2.0x),
string-map lookups (46.9 M, 2.0x), `fp_Container::getNthCon`.

## Convert-path split (big, callgrind inclusive)

`_writeDocument` 97.5 G → `fillLayouts` 64.0 G (initial format/break)
+ `s_actuallyPrint` 26.6 G (page render loop; contains part of the
findPage load via per-page geometry) + export tail.

## Startup

`run-startup.out`: first mapped window 75.4 s / 77.2 s / 79.5 s under
bare Xvfb (no WM). Orders of magnitude too slow — unexplained (not
profiled; D-Bus/fontconfig/layout of recovery files are candidates).
Worth one bounded look in PERF03 but separate from the doc-size
quadratics.

## Fix recipe for PERF03 (ordered by leverage)

1. **Make `findPage` O(1).** Give `fp_Page` a cached index
   (`m_iPageIndex`) maintained by `FL_DocLayout` on page insert/remove
   (shift the suffix — structural changes are rare), or keep a
   `std::unordered_map<const fp_Page*, int>` next to `m_vecPages` with
   lazy rebuild/dirty watermark. `findPage`, `getNext`, `getPrev`,
   `getPageNumber`, `setPageNumberInFrames`, `_getPageXandYOffset` all
   collapse to O(1) automatically. (Do not restore next/prev links —
   they were removed deliberately to prevent desync.)
2. **Index, don't hop, in the row-width helpers.**
   `getWidthPrevPagesInRow`/`getWidthPagesInRow` already compute page
   indices — iterate `getNthPage(i)` (O(1)) instead of `getNext()`
   hops. Optionally cache per-(numHorizPages, page-generation) row
   width/height prefix sums since these are re-queried per drawn item.
   Same for `getMaxHeight` and `calculateNumHorizPages` inner loops.
3. **`setFramePageNumbers`**: pass the page index the loop already has
   into `setPageNumberInFrames` (kill the inner `findPage`), and bound
   the renumber range or coalesce the per-column calls into one pass at
   break end.
4. **`_breakSection` fill loop**: containers are visited in section
   order — keep a running index into the source column's vector instead
   of `findCon`, and move contiguous ranges with one
   `erase(first,last)`/insert rather than per-element scan+memmove. For
   `m_iLinePosInContainer`, track the index at insert time instead of
   rescanning.
5. **`m_vecFormatLayout` dedup**: add `m_bInFormatQueue` on
   `fl_ContainerLayout` (set on enqueue, cleared on dequeue/clear) —
   push_back without scanning; same flag answers the updateLayout:1906
   membership test.
6. **Prop/font caching**: memoize `PP_evalProperty` results keyed by
   (prop, span-AP, block-AP, section-AP) within a format pass, or cache
   the resolved per-run attr-prop so `lookupProperties` isn't redone
   identically after every keystroke; check `GR_Graphics::findFont` for
   a usable (family,size,style) cache first.
7. **Verify**: rebuild fixtures (`mkbig.py`), callgrind h + big
   converts before/after — expect scan counts to collapse and per-
   doubling Ir ratio → ~2.0; `perf_drive` keystroke on h under callgrind
   as a spot check; `make -j2` clean + `make check` PASS + corpus
   docx→pdf smoke. Remove the PERFDBG instrumentation after verifying.

## What NOT to do

- Don't touch `_measureExtent`/`_findFrag`/string maps — linear.
- Don't micro-optimize inside `PP_evalProperty` itself before the caller
  count is cut.
