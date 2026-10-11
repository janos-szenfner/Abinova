# AUDIT06 — post-queue code-quality sweep

Scope: final-state sweep of the drained task queue (TRACK03/04, TST13,
ALIGN01/02, KEYB01/02, DRAG01, TBL01/02). Four legs; trivially-safe
findings fixed in one commit per area.

## (a) Dead code / leftover instrumentation — FIXED

Committed `2908953` (-147 lines, 7 files):

- **RULER_TRACE tracing** — 23 `getenv("RULER_TRACE")`-gated `fprintf`
  blocks in `ap_UnixRuler.cpp` (9), `ap_TopRuler.cpp` (6) and
  `ap_LeftRuler.cpp` (8), left over from the RUL02/RUL03 ruler
  debugging series. Removed; drag-sequencing logic unchanged.
- **NAVDBG** — unconditional `fprintf` in
  `AP_UnixFrameImpl::toggleNavPane()` printing on every nav-pane
  toggle. Removed.
- **DBG prints** — six unconditional `fprintf(stderr, "DBG …")` calls
  in `ap_EditMethods.cpp` (insertBlankPage, s_doInsertPageNumbers ×2,
  s_doBullets, doListType ×2), predating the series but live on every
  invocation. Removed.
- **Dead test accessors** — DRAG01 added `isAutoScrollActive()` /
  `getScrollAccelExtra()` to all three autoscroll classes; only the
  `FV_VisualDragText` pair is used by `fv_DragAutoScroll.t.cpp`.
  Removed from `fv_FrameEdit.h` and `fv_InlineImage.h`.
  `FV_View::hasPendingScrollWorker()` stays (test-used).

Verified NOT leftovers / kept:

- `ABINOVA_BREAK_DEBUG` / `BRKDBG` in `fb_ColumnBreaker.cpp` —
  documented opt-in field diagnostic for layout non-convergence.
- `#if BENCHLAYOUT` — compile-time benchmark toggle (off).
- `fl_/fp_TestRoutines.cpp` `__dump*` printers, `ap_Convert.cpp` CLI
  status messages, `getlogfile()`/`logfile` diagnostics — intentional.
- `UT_DEBUGMSG`/`xxx_UT_DEBUGMSG` sites — DEBUG-gated convention.
- ALIGN02's `BOUNDS`/`LINE` fprintfs and TST13's — inside `.t.cpp`
  test files (failure diagnostics), not product code.
- No dead `#if` branches or commented-out blocks introduced by the
  series; all new product helpers checked for callers
  (`getRevisionOpsLevel`, `_acceptRejectRevisionInner`,
  `s_hangingIndentStep`, new Defun edit methods — all used).

## (b) Duplication — partially FIXED

Committed `884e64f` (+37/-35, `ap_EditMethods.cpp`):

- `hangingIndent`/`unHangingIndent` (KEYB02) carried two copies of the
  read-format/adjust/write-back block differing only in sign/clamp;
  now share `s_hangingIndentStep(pView, bUnhang)`.

Checked and found healthy:

- Table-strux index handling post-TBL01 — `getStruxPosition(sdh) + 1`
  convention applied consistently in `fv_View_tableStyle.cpp`;
  `fv_enumTable` `posTable`/`posCell` usage is uniform.
- Revision-level mapping post-TRACK03 — centralized in
  `FV_View::getRevisionLevel()` + `getRevisionOpsLevel()`; the
  `revisionDisplayMode` edit method is the single mode handler; ribbon
  items dispatch through it. No parallel copies.
- Keybinding tables post-KEYB01/02 — the per-keymap `NVKTable`s
  (Default/Emacs/vi/dead-keys) are the established architecture, not
  duplication; the Word-parity additions went to the right rows.

Filed for design: the autoscroll machinery (`m_pAutoScrollTimer`,
`m_pScrollWorker`, `m_bScrollWorkerRunning`, `m_iScrollExtra` +
`_autoScroll`/`_actuallyScroll`/`_stopScrollWorkers`) is triplicated
across `FV_VisualDragText`, `FV_VisualInlineImage`, `FV_FrameEdit` —
with a fourth variant on `FV_View` — under different base classes.
Consolidating into a shared helper/mixin changes class structure:
scoped row `SCROLL01` added to TASKS.md.

## (c) Performance — no regressions found

- `fp_Line::layout()`'s new `resetJustification(false)` on non-justify
  passes (ALIGN01): `GR_CairoGraphics::resetJustification` early-outs
  on `!RI.m_pJustify`, so the cost is one loop over the line's runs —
  constant per layout, no added O(text) work.
- KEYB01 select-all: the `IS_SELECTALL` early-out runs once per
  Ctrl+A, not per keystroke.
- TRACK04 revision-find: bounded two-pass block walk on an explicit
  user command; not on a paint/layout path.
- DRAG01: worker lifecycle only; no new per-motion work. The
  `_actuallyScroll` ticks still early-out and self-teardown.
- No hot-path allocations introduced in the series diffs.

## (d) Security — held

- The fix series touched no importer/parser code: `git diff
  729fe44^..HEAD -- src/wp/impexp` is empty; `src/af` changes are
  `ev_UnixKeyboard.cpp` + `xap_UnixFrameImpl.cpp` only (key delivery).
- Bounded-parse guards intact: `ie_imp_MHT.cpp` bounded-slab boundary
  compare, `ie_imp_Abinova_1.cpp` bounded newline seek and the
  AUDIT-era importer bounds unchanged since FINAL01/CPP09/MTH04.
- `pp_AttrProp.cpp` (KEYB02 empty-attribute fix) reviewed — no new
  unbounded reads.

## Verification

- `make -j2`: clean (only pre-existing warnings; one new unused-param
  introduced mid-sweep was fixed before commit).
- `make -C wp/test check TESTS=unix/testwrap.sh`: PASS — 18,192 tests,
  0 failures.
