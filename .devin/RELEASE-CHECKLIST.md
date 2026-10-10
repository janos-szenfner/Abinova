# Abinova 4.0.0 pre-release gate

Started by RELQA01 (suite battery A). Each leg records its result here;
the tag is blocked until every leg is green and the
'known-failing exceptions' section is empty.

## RELQA01 — suite battery A (clean build + core suite + make check + coverage gate)

| Leg | Command | Result |
|-----|---------|--------|
| Clean build | `make -C src clean && make -j2` (thirdparty already built, `make -C thirdparty` = up to date) | PASS — 645 compile units, BUILD_EXIT=0, incremental `make -j2` also clean |
| Core unit suite | `make -C src/wp/test check TESTS="unix/testwrap.sh"` (Abinova-test, all .t.cpp mains) | PASS — 13382 tests, 0 failures |
| Round-trip corpus | `make -C src/wp/test check TESTS="unix/rtwrap.sh"` | PASS — 382/382 legs, 0 failed |
| Portability guard | `make -C src/wp/test check TESTS="unix/portwrap.sh"` | PASS — portguard OK (no /tmp literals, 0 unistd leaks) |
| Display legs (dlgswrap/drvwrap) | deferred — owned by RELQA05; run early here only to feed the coverage counters | dlgswrap PASS 71/72 + 1 xfail; drvwrap PASS 72/72 dialogs clean + abi/ev/ruler/fmt/freeze/toc/markup/popovers legs ok (frame leg hit its designed 300s bound, not a failure) |
| Coverage gate | `tools/coverage.sh` + `tools/coverage-gate.sh` (floor 72%) | PASS — 73.19% lines (158842/217024) after the full leg set fed counters |

RELQA01 also landed the uncommitted sanitizer-hardening fixes the
capped AUDIT03 run left in the tree (frame/IM-context teardown ordering,
weak-ref'd ribbon widget bookkeeping, RTF annotation/lastData leaks,
layout AP + PNG reader-buffer ownership, UBSan vptr/memmove fixes,
TF_MAX_TEST_TIME + weak `__gcov_dump` plumbing for san-builds) — all
verified by the battery above.

## RELQA05 — suite battery B (wrap legs + headless smoke harnesses)

| Leg | Command | Result |
|-----|---------|--------|
| Core unit suite | `make -C src/wp/test check TESTS="unix/testwrap.sh"` | PASS — 13382 tests, 0 failures |
| Round-trip corpus | `make -C src/wp/test check TESTS="unix/rtwrap.sh"` | PASS — run twice, second run after the EPUB field fix below |
| Portability guard | `make -C src/wp/test check TESTS="unix/portwrap.sh"` | PASS — re-run after the EPUB field fix |
| Dialog smoke | `make -C src/wp/test check TESTS="unix/dlgswrap.sh"` (xvfb-run) | PASS — 71/72 dialogs clean + 1 coded xfail (same expected failure as RELQA01) |
| Display driver | `make -C src/wp/test check TESTS="unix/drvwrap.sh"` (xvfb-run) | PASS — 72/72 dialogs clean + abiwidget/ev/ruler/fmt/freeze/toc/markup/popover legs ok; frame leg hit its designed 300s bound (documented bound, not a failure) |
| EPUB round-trip | `tools/epub-rt-check.sh src/abinova test/wp/fields.doc test/wp/BillOfRights.abw test/wp/long_footnote.doc <docx>` | PASS — 4/4 packages valid + text round-trips; found and fixed two real exporter bugs (see below) |
| Fuzz corpora | `tools/check-fuzz.sh` | PASS — 28 corpora + regression seeds |
| Freeze heartbeat | `tools/check-freeze.sh` (xvfb-run) | PASS — worst main-loop gap 1173 ms under load |

Failures found and fixed in this battery:

- XHTML/EPUB3 content emitted the named HTML entity `&nbsp;`, which is
  undefined in strict XML — generated packages were malformed and
  rejected by conforming parsers. The exporter now emits `&#160;` when
  the tag writer is in XML mode (`html4:no`, XHTML, EPUB3) and keeps
  `&nbsp;` for HTML4.
- HTML-family exporters (HTML/XHTML/EPUB) emitted empty spans for
  computed fields in documents whose field results were never laid out
  (e.g. `.doc`-imported fields): `populateFields()` is now invoked
  once per export, matching the plain-text exporter, so live values
  (dates, page/word/char counts, filenames, cross-reference results)
  appear in the output.
- `tools/epub-rt-check.sh` normalizes `HH:MM:SS` values when diffing
  text so live `time` fields (re-evaluated at each run) do not produce
  false mismatches.

## RELQA02 — memcheck A (valgrind over importer/exporter suites)

| Leg | Command | Result |
|-----|---------|--------|
| Importer/exporter unit suites | `valgrind --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=99 --num-callers=20 --suppressions=tools/valgrind.supp src/wp/test/.libs/Abinova-test ie_ ut_abwncrypt` (G_DEBUG=gc-friendly, G_SLICE=always-malloc, LC_ALL=C) | PASS — 2579 tests, 0 failures; per-test memcheck/leak deltas all clean; exit 0; `ERROR SUMMARY: 0 errors`; `definitely lost: 0 bytes` |
| Bounded convert corpus | `tools/check-valgrind.sh` | PASS — 16 passed, 0 failed, 0 skipped |

Findings in this battery:

- One new definite-leak record: a 96-byte `g_hash_table_new_full`
  allocation inside `libgvfsdbus.so`, created when
  `g_vfs_get_file_for_uri` first dlopens the GVFS D-Bus module
  (reached via `UT_go_basename_from_uri` on the HTML/EPUB export
  path). The tree code unrefs the returned `GFile` correctly; the
  table is a process-lifetime singleton owned by the GVFS module
  with no release API — same class as the existing
  `gio-module-singleton` suppression. Committed as
  `gvfs-dbus-module-table` in `tools/valgrind.supp` (definite kind
  only); re-run exits 0 with the record suppressed.
- The remaining `indirectly lost: 29,844 bytes in 1,220 blocks` are
  children of the suppressed third-party roots (fontconfig config
  parse/caches, glib `g_atomic_rc_box_alloc0`, pango language cache,
  GVFS module table, pthread TLS). Every loss record's allocation
  site was verified to be in third-party code — no definite or
  indirect leak is owned by tree code, so nothing to fix this leg.

## RELQA06 — memcheck B (valgrind over formatter + af/core suites)

| Leg | Command | Result |
|-----|---------|--------|
| Formatter suites fv1 (ViewModes/ViewOps/FootnoteDelete/HdrFtr*/MouseContext/FieldContext/TOCContext) | `valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=definite --error-exitcode=99 --num-callers=20 --suppressions=tools/valgrind.supp src/wp/test/.libs/Abinova-test <prefixes>` (G_DEBUG=gc-friendly, G_SLICE=always-malloc, LC_ALL=C, TF_MAX_TEST_TIME=300) | PASS — 416 tests, 0 failures; `ERROR SUMMARY: 0 errors`; `definitely lost: 0 bytes` |
| Formatter suites fv2 (FrameContext/ImageProps/PosObjectContext/EditOps/TableOps/RefsTOC/StateCycle/PasteTag/SignatureLine) | same | PASS — 996 tests, 0 failures; 0 errors / 0 definite in parent AND forked child (18 suppressed fork-snapshot records) |
| Formatter suites fv3 (StructContent/GoldenCovers/Fuzz) | same + `ABINOVA_FUZZ_BUDGET_SCALE=30` | PASS — 861 tests, 0 failures; 0 errors / 0 definite |
| Core layout/ptable (`fl_ fp_ pf_ pt_ pp_ pd_ px_`) | same | PASS — 2,199 tests, 0 failures; 0 errors / 0 definite |
| af/util (`ut_`) | same | PASS — 3,221 tests, 0 failures; 0 errors / 0 definite in parent AND forked child |
| af ev/gr/xap/xad/tf (`ev_ gr_ xap_ xad_ tf_`) | same | PASS — 2,785 tests, 0 failures; 0 errors / 0 definite |
| wp/ap (`ap_`) | same | PASS — 354 tests, 0 failures; 0 errors / 0 definite in parent AND 3 dict_child forked children |

Totals: 10,832 tests across 7 batch processes + forked children, zero
memcheck errors, zero definite leaks, zero first-party-owned indirect
leaks. Together with RELQA02's `ie_*` + `ut_abwncrypt` leg this sweeps
every `Abinova-test` suite under memcheck (the `ut_` prefix re-covered
`ut_abwncrypt` — harmless overlap).

Findings in this battery (all fixed or dispositioned):

- `GR_Caret::_getCursorBlinkTime()` returned an uninitialised blink
  time whenever `gtk_settings_get_default()` is NULL (headless test
  processes) — the uninit value fed `g_object_get`'s out-param and
  propagated into `UT_UNIXTimer::set` conditionals. Now initialised to
  the GTK default (1200 ms) and the `g_object_get` is guarded.
- `GR_CairoGraphics::measureString()` only zero-filled the trailing
  `pWidths[]` entries on the success path; on `!bMeasureOk` callers
  still read the full span and got stack garbage. The tail-fill now
  runs unconditionally when `pWidths` is given.
- `fp_TabRun::_draw()` read `wid[150]`, one slot past the 150 entries
  `measureString(tmp, 1, 150, wid)` initialises — an uninitialised
  stack read whenever a tab leader exhausted all 150 samples. Loop
  bound corrected.
- `fv_Fuzz` guard budgets are wall-clock; under valgrind's ~10–40x
  slowdown a legitimate step outlived its budget and the watchdog
  reap leaked in-flight allocations (75 KB of definite loss records in
  the grammar-check dictionary-load path — victim of the reap, not a
  leak). New `ABINOVA_FUZZ_BUDGET_SCALE` env multiplies every guard
  budget for instrumented runs; the wedge-detection negative control
  is scale-aware.
- New committed suppressions in `tools/valgrind.supp`:
  `librsvg-handle-init-cond` (librsvg 2.50 Rust core evaluates
  conditionals on uninitialised bytes inside its own class-init,
  reached via `rsvg_handle_new`) and four forked-child snapshot-leak
  classes (`pango-fc-worker-thread`, `gvfs-gtask-worker-pool`,
  `gio-worker-mainloop`, `gio-worker-ctxqueue`) — several tests fork
  without exec (dict_child, run_assert_in_child, g_spawn helpers);
  the child inherits the parent's heap snapshot and `_exit()`s without
  library teardown, reporting third-party worker/module allocations as
  "definitely lost" and flipping the child exit status to 99 under
  `--error-exitcode`. All allocation sites verified third-party-owned.
- Residual `indirectly lost` totals per batch (20 KB–1.05 MB) all root
  at third-party allocation sites verified by stack inspection:
  `FcFontSetList` via `pango_fc_font_map_cache_clear` (the ~1 MB
  fontconfig/pango-ft2 cache tree), gtk/gio worker-thread hash tables,
  expat/fontconfig parse internals. Zero first-party-owned roots.

## Known-failing exceptions

(none)
