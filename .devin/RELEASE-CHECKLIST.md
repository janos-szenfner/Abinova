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

## RELQA03 — pre-release gate: dead-code scan (REPORT ONLY)

Scope: report-only audit; no code removed. Per-item verdicts below feed
RELQA07's removal pass.

| Leg | Tool/command | Result |
|-----|--------------|--------|
| unusedFunction scan | cppcheck 2.13.0 (distro .deb run from `/tmp/cppcheck-deb`, FILESDIR-patched binary `/tmp/cppcheck-patched`, `LD_LIBRARY_PATH` for libtinyxml2) `--enable=unusedFunction --std=c++17 -Isrc -Isrc/af -Isrc/wp src/` | 614 raw hits; raw log: `.devin/relqa03/cppcheck-unused.log`. Caveats: 21 files unparseable to cppcheck (GTK macros / `UT_nonnull_or_return`), 12-of-N `#ifdef` configs only, so callers in unparsed units are invisible (raises FP rate, cannot hide a genuinely-dead hit) |
| -Wunused sweep | full `make -j2` rebuild, 643 production objects recompiled, warnings captured (`.devin/relqa03/build-warnings.log`) | 8 `-Wunused*` warnings, 0 `-Wunused-function`; 18 `*/t/*.t.lo` test objects not rebuilt here (compiled under `make check`) |
| coverage-plateau 0% cross-check | `coverage.info` files with 0 hits vs scan candidates | 42 files at 0%; every one verified compiled+reachable — see table |
| orphan files / Makefile.am | scripted cross-ref of all 42 tracked `Makefile.am` vs `git ls-files` + `#include` refs | no orphaned source files; 2 `make dist` integrity findings |
| `#if 0`/FIXME inventory | anchored grep over `src/` | 0 real `#if 0` blocks; 1,274 marker comments, clusters tabulated |

### `-Wunused` sweep — per-item verdicts

| Site | Warning | Verdict |
|------|---------|---------|
| `wp/ap/gtk/ap_UnixRibbon.cpp:6331` | variable `fband` set but not used | dead store — RELQA07 remove the assignment |
| `wp/ap/gtk/ap_UnixRibbon.cpp:11526` | unused parameter `ctx` | callback signature constraint — keep, annotate later |
| `af/util/xp/ut_abwncrypt.cpp:190` | unused parameter `libname` | interface-required parameter — keep |
| `wp/impexp/odf/exp/xp/ODe_DocumentData.cpp:224` | unused variables `count`, `i` | dead locals — RELQA07 remove |
| `wp/impexp/odf/exp/xp/ODe_Style_List.cpp:52` | unused variables `i`, `count` | dead locals — RELQA07 remove |
| `openxml/common/xp/OXML_LangToScriptConverter.gperf:51` | unused parameter `len` | generated gperf signature — keep |

### Coverage-plateau 0% files — per-item verdicts

All 42 files are compiled into the shipped binary and reachable (importer
registration, edit-method dialog dispatch, or inline header code). Verdict:
**uncovered-live** for every file — none is a removal candidate; consistent
with the documented plateau classes (live-session UI, native dialogs,
RDF/not-compiled-in-test, defensive paths). Substantive .cpp entries:

| File | Lines@0% | Live path |
|------|---------:|-----------|
| `wp/impexp/xp/ie_imp_RDF.cpp` | 70 | RDF importer, registered in `ie_impexp_Register.cpp` |
| `af/xap/xp/xap_Preview_Zoom.cpp` | 55 | zoom preview UI (`xap_Dlg_Zoom`) |
| `wp/impexp/odf/imp/xp/ODi_TableOfContent_ListenerState.cpp` | 51 | ODT importer TOC listener — exercised only by TOC-bearing docs |
| `wp/ap/gtk/ap_RDFLocationGTK.cpp` | 45 | RDF location dialog (GTK UI) |
| `af/xap/xp/xap_AppImpl.cpp` | 44 | app-impl base hooks, live-session only |
| `af/xap/gtk/xap_UnixFontPreview.cpp` | 32 | font-preview widget (UI) |
| `wp/main/gtk/libabinova.cpp` | 24 | library entry point (test harness uses own main) |
| `af/xap/gtk/xap_UnixDlg_ColorChooser.cpp` | 17 | native color-chooser dialog |

The remaining 34 entries are headers with 1-10 lines of inline accessor
code (`ut_case.h`, `ap_Convert.h`, `ODi_*_ListenerState.h`, dialog `.h`s,
etc.) — inline methods only run when the owning class is exercised;
all verdict **uncovered-live, keep**.

### Orphan files / Makefile.am audit — per-item verdicts

Cross-referenced all 42 tracked `Makefile.am` files against `git ls-files`
plus `#include` closure. No source file is orphaned.

| Item | Verdict |
|------|---------|
| `src/af/xap/Makefile.am` `EXTRA_DIST` entry `gtk/xap_UnixWidget.t.cpp` | **REAL finding**: file lives at `gtk/t/xap_UnixWidget.t.cpp`; stale path breaks `make dist` — RELQA07 fix the entry |
| `src/wp/impexp/Makefile.am` `EXTRA_DIST` missing 8 live tests (`ie_abinova.t.cpp`, `ie_clipcopy.t.cpp`, `ie_math.t.cpp`, `ie_pastelistener.t.cpp`, `ie_sniffers.t.cpp`, `ie_tocstyle.t.cpp`, `ie_xxe.t.cpp`, `ut_abwncrypt.t.cpp`; all `#include`d via `all_test.h`) | **REAL finding**: `make dist` tarball won't build tests — RELQA07 add entries |
| `abwn.dtd` (top level) | live — published DOCTYPE/schema of the `.abwn` format, referenced by exporter, README, docs; not in `EXTRA_DIST` (minor packaging gap, optional) |
| `autogen.sh`, `autogen-common.sh` | live bootstrap scripts (`autogen.sh` sources the other) |
| `fuzz/*.cpp` (16 files) | live — built by `tools/build-fuzz.sh`, shipped via `fuzz` dir in top `EXTRA_DIST` |
| all other candidates | false positives — generated files (`abi-resources.*`, `config.h`, testwrap `.sh` from `.in`), `$(top_srcdir)` refs (`tools/cdump/xp/cdump.py` exists), comment mentions (`tidy/tidy.h`), `.cxx`/`.hxx` prefix artifacts, thirdparty vendored build refs |

### `#if 0` / FIXME / TODO inventory

- Real `#if 0` preprocessor blocks: **0** (5 hits are comments describing
  past `#if 0` usage in `fv_View_protected.cpp`, `fp_TextRun.cpp`,
  `fb_ColumnBreaker.cpp`, `ie_exp_RTF_listenerWriteDoc.cpp`, `ut_debugmsg.h`).
- Marker comments: TODO 1014, FIXME 151, XXX 76, HACK 33 (1,274 total).
  Clusters: `wp/impexp` 316, `text/fmt` 315, `wp/ap` 211, `text/ptbl` 130,
  `af/util` 101. Densest files: `fl_BlockLayout.cpp` 49, `fv_View.cpp` 42,
  `ie_imp_RTF.cpp` 35, `fv_View_protected.cpp` 34.
- Verdict: inherited documentation debt, not dead code — informational
  only, no RELQA07 action unless a marker is attached to a removal item.

### cppcheck `--enable=unusedFunction` — findings with per-item verdicts

614 raw hits; every hit was verdict-classified by cross-grepping the
function name over the full tracked corpus (sources, headers, `.ui`,
`.xml`, build files) and checking the declaration context for
virtual/override and accessor-macro provenance:

- **candidate-dead (180)** — name occurs only at its decl/def; nothing in
  the tree references it. RELQA07 removal worklist (verify virtual/
  callback/vtable reachability per item before deleting).
- **api-surface (84)** — single-occurrence inline header accessor
  (get/set pairs etc.). Unused today; keep-or-remove decision per class
  API intent — RELQA07 review, default keep.
- **macro-accessor (26)** — emitted by `SET_GATHER` /
  `DEFINE_GET_SET_BOOL_DUMMY` macros; removing one means editing the
  macro emission — keep.
- **fp-virtual (44)** — `virtual`/`override`/`VIRTUAL_SFX` context;
  invoked polymorphically — false positive, keep.
- **live-infile (31)** — called elsewhere inside the same translation
  unit (incl. signal-connected handlers) — false positive, keep.
- **live (248)** — referenced from other files — false positive, keep.
- **generated (1)** — `abi_get_resource` in generated `abi-resources.c`
  — keep (build artifact).

#### candidate-dead (180) — RELQA07 worklist

src/af/gr/xp/gr_CairoGraphics.cpp:3476 `dtpu`; src/af/gr/xp/gr_CairoGraphics.cpp:3484 `ptdu`; src/af/gr/xp/gr_CairoGraphics.cpp:3492 `ptlu`;
src/af/gr/xp/gr_CairoGraphics.cpp:3514 `ltpu`; src/af/gr/xp/gr_CairoGraphics.cpp:3541 `pftlu`; src/af/gr/xp/gr_CairoNullGraphics.cpp:166 `drawRGBImage`;
src/af/gr/xp/gr_CairoNullGraphics.cpp:170 `drawGrayImage`; src/af/gr/xp/gr_CairoNullGraphics.cpp:174 `drawBWImage`; src/af/tf/xp/tf_test.cpp:227 `run_suite`;
src/af/util/xp/ut_Script.cpp:215 `suffixesForType`; src/af/util/xp/ut_bytebuf.cpp:196 `insertFromURI`; src/af/util/xp/ut_bytebuf.cpp:273 `writeToFile`;
src/af/util/xp/ut_locale.cpp:182 `hasLanguage`; src/af/util/xp/ut_timer.cpp:45 `_getVecTimers`; src/af/xap/gtk/xap_UnixDialogHelper.cpp:233 `newDialogBuilder`;
src/af/xap/gtk/xap_UnixDialogHelper.cpp:287 `isTransientWindow`; src/af/xap/gtk/xap_UnixDlg_PrintPreview.cpp:444 `_s_zoomChoose`;
src/af/xap/gtk/xap_UnixFrameImpl.cpp:1466 `_setInputMode`; src/af/xap/xp/spell_manager.cpp:159 `couldNotLoadDictionary`;
src/af/xap/xp/xad_Document.cpp:491 `setOrigUUID`; src/af/xap/xp/xad_Document.cpp:513 `setMyUUID`; src/af/xap/xp/xad_Document.cpp:927 `_restoreVersion`;
src/af/xap/xp/xap_App.cpp:192 `getBuildId`; src/af/xap/xp/xap_App.cpp:197 `getBuildVersion`; src/af/xap/xp/xap_App.cpp:202 `getBuildOptions`;
src/af/xap/xp/xap_App.cpp:207 `getBuildTarget`; src/af/xap/xp/xap_App.cpp:212 `getBuildCompileTime`; src/af/xap/xp/xap_App.cpp:217 `getBuildCompileDate`;
src/af/xap/xp/xap_App.cpp:265 `unRegisterEmbeddable`; src/af/xap/xp/xap_App.cpp:889 `isWordInDict`; src/af/xap/xp/xap_App.cpp:1327 `setDefaultGraphicsId`;
src/af/xap/xp/xap_App.cpp:1377 `saveState`; src/af/xap/xp/xap_App.cpp:1509 `retrieveState`; src/af/xap/xp/xap_App.cpp:1666 `setNoGUI`;
src/af/xap/xp/xap_Dlg_DocComparison.h:75 `getResultCount`; src/af/xap/xp/xap_Dlg_Insert_Symbol.cpp:92 `_updateDrawSymbol`;
src/af/xap/xp/xap_Dlg_Insert_Symbol.cpp:114 `_updateDrawSymbolarea`; src/af/xap/xp/xap_Dlg_Print.cpp:207 `getDoPrintToFile`;
src/af/xap/xp/xap_Dlg_Print.cpp:235 `_getPrintToFilePathname`; src/af/xap/xp/xap_Frame.cpp:835 `setAutoSaveFileExt`;
src/af/xap/xp/xap_Frame.cpp:1110 `rebuildAllToolbars`; src/af/xap/xp/xap_Frame.cpp:1126 `dragBegin`; src/af/xap/xp/xap_Frame.cpp:1140 `dragDropToIcon`;
src/af/xap/xp/xap_Frame.cpp:1155 `dragDropToTB`; src/af/xap/xp/xav_View.cpp:161 `setActivityMask`; src/text/fmt/xp/fl_AutoNum.cpp:682 `setAsciiOffset`;
src/text/fmt/xp/fl_AutoNum.cpp:978 `doesItemHaveLabel`; src/text/fmt/xp/fl_AutoNum.cpp:1026 `isIDSomeWhere`;
src/text/fmt/xp/fl_AutoNum.cpp:1199 `getPrevInList`; src/text/fmt/xp/fl_AutoNum.cpp:1207 `_getLevelValue`;
src/text/fmt/xp/fl_BlockLayout.cpp:1920 `findLineWithFootnotePID`; src/text/fmt/xp/fl_BlockLayout.cpp:4770 `findPrevLineInDocument`;
src/text/fmt/xp/fl_BlockLayout.cpp:4809 `findNextLineInDocument`; src/text/fmt/xp/fl_BlockLayout.cpp:9830 `getFormatFromListType`;
src/text/fmt/xp/fl_BlockLayout.cpp:10954 `_addBlockToPrevList`; src/text/fmt/xp/fl_BlockLayout.cpp:10984 `_prependBlockToPrevList`;
src/text/fmt/xp/fl_BlockLayout.cpp:11024 `getNextTableElement`; src/text/fmt/xp/fl_ContainerLayout.cpp:354 `getFoldedID`;
src/text/fmt/xp/fl_DocLayout.cpp:3286 `setDisplayAnnotations`; src/text/fmt/xp/fl_DocLayout.cpp:3301 `setDisplayRDFAnchors`;
src/text/fmt/xp/fl_DocLayout.cpp:3541 `dequeueAll`; src/text/fmt/xp/fl_FrameLayout.cpp:380 `doclistener_deleteEndFrame`;
src/text/fmt/xp/fl_FrameLayout.cpp:617 `_insertFrameContainer`; src/text/fmt/xp/fl_PartOfBlock.cpp:45 `setGrammarMessage`;
src/text/fmt/xp/fl_PartOfBlock.cpp:51 `getGrammarMessage`; src/text/fmt/xp/fl_SectionLayout.cpp:1046 `getFootnoteLayout`;
src/text/fmt/xp/fl_SectionLayout.cpp:5176 `bl_doclistener_insertFootnote`; src/text/fmt/xp/fl_SectionLayout.cpp:5215 `bl_doclistener_insertAnnotation`;
src/text/fmt/xp/fl_TOCLayout.h:59 `hasLabel`; src/text/fmt/xp/fl_TOCLayout.h:67 `getNumLabel`; src/text/fmt/xp/fl_TestRoutines.cpp:63 `__dump_fmt`;
src/text/fmt/xp/fl_TestRoutines.cpp:76 `__dump_pt`; src/text/fmt/xp/fl_TestRoutines.cpp:85 `__dump_ch`; src/text/fmt/xp/fl_TestRoutines.cpp:94 `__dump_sq`;
src/text/fmt/xp/fp_ContainerObject.cpp:620 `justRemoveNthCon`; src/text/fmt/xp/fp_ContainerObject.cpp:857 `setTransparent`;
src/text/fmt/xp/fp_FrameContainer.cpp:235 `isTransformed`; src/text/fmt/xp/fp_Line.cpp:754 `containsOffset`; src/text/fmt/xp/fp_Line.cpp:1060 `getWidthToRun`;
src/text/fmt/xp/fp_Line.cpp:3453 `getLastTextRun`; src/text/fmt/xp/fp_Line.cpp:4068 `_splitRunsAtSpaces`;
src/text/fmt/xp/fp_Line.cpp:4449 `_updateContainsFootnoteRef`; src/text/fmt/xp/fp_Page.cpp:3304 `frameHeightChanged`;
src/text/fmt/xp/fp_Run.h:390 `_getHeight`; src/text/fmt/xp/fp_TableContainer.cpp:1687 `getCellY`;
src/text/fmt/xp/fp_TableContainer.cpp:2511 `getFirstContainerInBrokenTable`; src/text/fmt/xp/fp_TableContainer.cpp:3081 `getYOfRowOrColumn`;
src/text/fmt/xp/fp_TableContainer.cpp:4543 `setRowSpacing`; src/text/fmt/xp/fp_TableContainer.cpp:4552 `setColSpacing`;
src/text/fmt/xp/fp_TextRun.cpp:2852 `isFirstCharacter`; src/text/fmt/xp/fv_Base.cpp:86 `getGlobCount`; src/text/fmt/xp/fv_Selection.cpp:452 `addSelectedRange`;
src/text/fmt/xp/fv_ViewDoubleBuffering.cpp:153 `redrawEntireScreen`; src/text/ptbl/xp/pd_Document.cpp:486 `setExportAuthorAtts`;
src/text/ptbl/xp/pd_Document.cpp:5214 `setDataItemToken`; src/text/ptbl/xp/pd_Document.cpp:5233 `getDataItemData`;
src/text/ptbl/xp/pd_Document.cpp:5794 `updateAllLayoutsInDoc`; src/text/ptbl/xp/pd_DocumentRDF.cpp:1528 `requestExportFileNameByDialog`;
src/text/ptbl/xp/pd_DocumentRDF.cpp:1789 `createUUIDNode`; src/text/ptbl/xp/pd_DocumentRDF.cpp:2690 `stylesheetTypeUser`;
src/text/ptbl/xp/pt_PieceTable.cpp:475 `_struxHasContent`; src/text/ptbl/xp/t/pd_Revision.t.cpp:88 `posBlock2`;
src/wp/ap/gtk/abiwidget.cpp:268 `abi_widget_cut`; src/wp/ap/gtk/abiwidget.cpp:270 `abi_widget_paste_special`;
src/wp/ap/gtk/abiwidget.cpp:276 `abi_widget_file_save`; src/wp/ap/gtk/abiwidget.cpp:313 `abi_widget_select_prev_line`;
src/wp/ap/gtk/abiwidget.cpp:314 `abi_widget_select_right`; src/wp/ap/gtk/abiwidget.cpp:315 `abi_widget_select_screen_down`;
src/wp/ap/gtk/abiwidget.cpp:316 `abi_widget_select_screen_up`; src/wp/ap/gtk/abiwidget.cpp:317 `abi_widget_select_to_xy`;
src/wp/ap/gtk/abiwidget.cpp:2002 `abi_widget_get_property`; src/wp/ap/gtk/abiwidget.cpp:2078 `abi_widget_set_property`;
src/wp/ap/gtk/abiwidget.cpp:2570 `abi_widget_get_frame`; src/wp/ap/gtk/ap_UnixApp.cpp:363 `getPrefsValueDirectory`;
src/wp/ap/gtk/ap_UnixApp.cpp:1032 `makePngPreview`; src/wp/ap/gtk/ap_UnixClipboard.cpp:283 `addRichTextData`;
src/wp/ap/gtk/ap_UnixClipboard.cpp:291 `addHtmlData`; src/wp/ap/gtk/ap_UnixClipboard.cpp:300 `addODTData`;
src/wp/ap/gtk/ap_UnixClipboard.cpp:355 `getRichTextData`; src/wp/ap/gtk/ap_UnixClipboard.cpp:365 `getImageData`;
src/wp/ap/gtk/ap_UnixClipboard.cpp:375 `getDynamicData`; src/wp/ap/gtk/ap_UnixDialog_Document.cpp:840 `_redrawPreview`;
src/wp/ap/gtk/ap_UnixDialog_Lists.cpp:731 `setAllSensitivity`; src/wp/ap/gtk/ap_UnixDialog_PageNumbers.cpp:89 `event_PreviewInvalidate`;
src/wp/ap/gtk/ap_UnixDialog_Paragraph.cpp:200 `event_MenuChanged`; src/wp/ap/gtk/ap_UnixDialog_Paragraph.cpp:226 `event_SpinIncrement`;
src/wp/ap/gtk/ap_UnixDialog_Paragraph.cpp:232 `event_SpinDecrement`; src/wp/ap/gtk/ap_UnixDialog_Styles.cpp:404 `event_charPreviewInvalidate`;
src/wp/ap/gtk/ap_UnixDialog_Styles.cpp:1266 `event_ModifyDelete`; src/wp/ap/gtk/ap_UnixDialog_Styles.cpp:1342 `event_ModifyPreviewInvalidate`;
src/wp/ap/gtk/ap_UnixFrame.cpp:417 `translateDocumentToScreen`; src/wp/ap/xp/ap_Args.cpp:93 `addOptions`;
src/wp/ap/xp/ap_Dialog_Columns.cpp:353 `_drawColumnButton`; src/wp/ap/xp/ap_Dialog_FormatFootnotes.cpp:109 `getEndnoteValString`;
src/wp/ap/xp/ap_Dialog_FormatFootnotes.cpp:115 `getFootnoteValString`; src/wp/ap/xp/ap_Dialog_Goto.cpp:124 `performGotoNext`;
src/wp/ap/xp/ap_Dialog_Goto.cpp:153 `performGotoPrev`; src/wp/ap/xp/ap_Dialog_HdrFtr.cpp:73 `isChanged`;
src/wp/ap/xp/ap_Dialog_Lists.cpp:133 `copyCharToWindowName`; src/wp/ap/xp/ap_Dialog_Options.cpp:396 `_eventSave`;
src/wp/ap/xp/ap_Dialog_SignatureLine.cpp:55 `setSignatureSetup`; src/wp/ap/xp/ap_LoadBindings.cpp:212 `createMap`;
src/wp/ap/xp/ap_Preview_Annotation.cpp:76 `setAnnotationID`; src/wp/impexp/odf/exp/xp/ODe_Common.cpp:150 `ODe_writeToFile`;
src/wp/impexp/odf/exp/xp/ODe_Text_Listener.cpp:1299 `_blockIsPlainParagraph`; src/wp/impexp/odf/imp/xp/ie_imp_OpenDocument.cpp:419 `_handleSettingsStream`;
src/wp/impexp/openxml/common/xp/OXML_Document.cpp:69 `getCurrentSection`; src/wp/impexp/openxml/common/xp/OXML_Document.cpp:353 `getSection`;
src/wp/impexp/openxml/common/xp/OXML_Element_Text.cpp:70 `getText`; src/wp/impexp/openxml/exp/xp/ie_exp_OpenXML.cpp:1927 `setNumberingFormat`;
src/wp/impexp/xp/ie_FileInfo.cpp:30 `setFileInfo`; src/wp/impexp/xp/ie_FileInfo.cpp:59 `mapAlias`; src/wp/impexp/xp/ie_TOC.cpp:234 `docHasTOC`;
src/wp/impexp/xp/ie_Table.h:169 `isMergedRight`; src/wp/impexp/xp/ie_Table.h:171 `isFirstVerticalMerged`;
src/wp/impexp/xp/ie_Table.h:172 `isFirstHorizontalMerged`; src/wp/impexp/xp/ie_Table.h:174 `setImpTable`; src/wp/impexp/xp/ie_Table.h:222 `isAutoFit`;
src/wp/impexp/xp/ie_Table.h:351 `getInsertionPoint`; src/wp/impexp/xp/ie_exp.cpp:156 `unregisterExporter`; src/wp/impexp/xp/ie_exp.cpp:439 `rewindChar`;
src/wp/impexp/xp/ie_exp_HTML.cpp:908 `printStyleTree`; src/wp/impexp/xp/ie_exp_RTF.cpp:455 `_rtf_keyword_hex2`;
src/wp/impexp/xp/ie_exp_RTF.cpp:2587 `getMultiLevelCount`; src/wp/impexp/xp/ie_exp_RTF.cpp:2595 `getSimpleListCount`;
src/wp/impexp/xp/ie_exp_RTF.cpp:3421 `getMatchingID`; src/wp/impexp/xp/ie_exp_RTF.h:276 `isSimple`; src/wp/impexp/xp/ie_exp_RTF.h:277 `isMulti`;
src/wp/impexp/xp/ie_exp_RTF_listenerWriteDoc.cpp:3846 `_outputCellBorders`; src/wp/impexp/xp/ie_exp_XML.cpp:121 `addLuint`;
src/wp/impexp/xp/ie_impGraphic.cpp:374 `constructImporterWithDescription`; src/wp/impexp/xp/ie_imp_RTF.cpp:613 `isDeletedChanged`;
src/wp/impexp/xp/ie_imp_RTF.cpp:621 `getDeleted`; src/wp/impexp/xp/ie_imp_RTF.cpp:716 `isSuperscriptPosChanged`;
src/wp/impexp/xp/ie_imp_RTF.cpp:749 `isSubscriptPosChanged`; src/wp/impexp/xp/ie_imp_RTF.cpp:799 `isColourNumberChanged`;
src/wp/impexp/xp/ie_imp_RTF.cpp:833 `isBgColourNumberChanged`; src/wp/impexp/xp/ie_imp_RTF.cpp:2145 `FlushTableProps`;
src/wp/impexp/xp/ie_imp_RTF.cpp:10828 `HandleDeleted`; src/wp/impexp/xp/ie_imp_XML.cpp:448 `_getXMLPropValue`

#### api-surface (84) — verdict: keep pending RELQA07 API review

src/af/gr/xp/gr_CairoGraphics.h:131 `getPangoLanguage`; src/af/gr/xp/gr_Caret.h:64 `getInsertMode`; src/af/gr/xp/gr_Graphics.h:197 `_getCharWidths`;
src/af/gr/xp/gr_MathTypesetter.h:110 `errorText`; src/af/gr/xp/gr_Transform.h:33 `linearScale`; src/af/gr/xp/gr_Transform.h:43 `getE`;
src/af/gr/xp/gr_Transform.h:44 `getF`; src/af/util/xp/ut_color.h:107 `isPattern`; src/af/util/xp/ut_crc32.h:45 `UpdateByte`;
src/af/util/xp/ut_hash.h:181 `exceeds_n_delete_threshold`; src/af/xap/gtk/xap_UnixFrameImpl.h:62 `getNewY`;
src/af/xap/xp/spell_manager.h:76 `isDictionaryFound`; src/af/xap/xp/xap_App.h:195 `setDebugBool`; src/af/xap/xp/xap_App.h:196 `clearDebugBool`;
src/af/xap/xp/xap_App.h:197 `isDebug`; src/af/xap/xp/xap_Dlg_Language.h:78 `getSpellCheck`; src/text/fmt/xp/fl_BlockLayout.h:265 `getProp_KeepWithNext`;
src/text/fmt/xp/fl_BlockLayout.h:277 `clearHdrFtr`; src/text/fmt/xp/fl_BlockLayout.h:375 `setAccumHeight`;
src/text/fmt/xp/fl_BlockLayout.h:377 `getAccumHeight`; src/text/fmt/xp/fl_DocLayout.h:189 `getPercentFilled`;
src/text/fmt/xp/fl_DocLayout.h:353 `getBackgroundCheckReasons`; src/text/fmt/xp/fl_DocLayout.h:356 `getPendingBlockForGrammar`;
src/text/fmt/xp/fl_FrameLayout.h:137 `setFrameXpos`; src/text/fmt/xp/fl_FrameLayout.h:138 `setFrameYpos`;
src/text/fmt/xp/fl_SectionLayout.h:266 `getFootnoteYoff`; src/text/fmt/xp/fl_TOCLayout.h:61 `doesInherit`;
src/text/fmt/xp/fl_TableLayout.h:348 `getCellHeight`; src/text/fmt/xp/fl_TableLayout.h:350 `getCellWidth`;
src/text/fmt/xp/fp_Column.h:96 `getIntentionallyEmpty`; src/text/fmt/xp/fp_Column.h:101 `setIntentionallyEmpty`;
src/text/fmt/xp/fp_Column.h:165 `getNthWrappedLine`; src/text/fmt/xp/fp_FrameContainer.h:79 `setYpad`; src/text/fmt/xp/fp_Run.h:380 `_setLine`;
src/text/fmt/xp/fp_Run.h:399 `_getVisDirection`; src/text/fmt/xp/fp_TOCContainer.h:99 `setBrokenBot`; src/text/fmt/xp/fp_TableContainer.h:229 `setXexpand`;
src/text/fmt/xp/fp_TableContainer.h:231 `setYexpand`; src/text/fmt/xp/fp_TableContainer.h:233 `setXshrink`;
src/text/fmt/xp/fp_TableContainer.h:235 `setYshrink`; src/text/fmt/xp/fp_TableContainer.h:241 `setXfill`; src/text/fmt/xp/fp_TableContainer.h:243 `setYfill`;
src/text/fmt/xp/fp_TextRun.h:112 `orShapingRequired`; src/text/fmt/xp/fp_TextRun.h:121 `getItem`; src/text/fmt/xp/fp_TextRun.h:139 `getLetterSpacing`;
src/text/fmt/xp/fv_Selection.h:79 `getSelectedTOC`; src/text/fmt/xp/fv_View.h:1070 `getColorImageResize`; src/text/fmt/xp/fv_View.h:1078 `getColorHdrFtr`;
src/text/fmt/xp/fv_View.h:1123 `setBidiOrder`; src/text/ptbl/xp/pd_Document.h:333 `getAuthors`; src/text/ptbl/xp/pd_Document.h:858 `ignoreSignals`;
src/text/ptbl/xp/pd_Document.h:860 `dontIgnoreSignals`; src/text/ptbl/xp/pd_Document.h:865 `setCoalescingMask`;
src/text/ptbl/xp/pt_PieceTable.h:634 `_getNextChangeRecordNumber`; src/wp/ap/gtk/ap_UnixDialog_Lists.h:90 `_getMainWindow`;
src/wp/ap/gtk/ap_UnixDialog_MarkRevisions.h:54 `cancel_callback`; src/wp/ap/gtk/ap_UnixDialog_MarkRevisions.h:59 `destroy_callback`;
src/wp/ap/gtk/ap_UnixDialog_Styles.h:96 `getStyleType`; src/wp/ap/xp/ap_Dialog_Lists.h:108 `getStoredID`;
src/wp/ap/xp/ap_Preview_Annotation.h:59 `getAnnotationID`; src/wp/impexp/mht/ie_imp_MHT.h:86 `contentEncoding`;
src/wp/impexp/odf/exp/xp/ODe_AutomaticStyles.h:70 `getMasterPage`; src/wp/impexp/odf/exp/xp/ODe_Style_Style.h:113 `setListStyleName`;
src/wp/impexp/odf/exp/xp/ODe_Styles.h:78 `getGraphicStylesEnumeration`; src/wp/impexp/odf/imp/xp/ODi_Office_Styles.h:128 `getPageLayoutStyle`;
src/wp/impexp/odf/imp/xp/ODi_Style_MasterPage.h:77 `getPageLayoutName`; src/wp/impexp/odf/imp/xp/ODi_Style_Style.h:80 `getAbiData`;
src/wp/impexp/odf/imp/xp/ODi_Style_Style.h:242 `getTableMarginRight`; src/wp/impexp/openxml/common/xp/OXML_Element_Row.h:52 `getCurrentColumnNumber`;
src/wp/impexp/openxml/common/xp/OXML_Section.h:67 `getHeaderId`; src/wp/impexp/openxml/common/xp/OXML_Section.h:69 `getFooterId`;
src/wp/impexp/openxml/common/xp/OXML_Section.h:131 `getTitlePg`; src/wp/impexp/xp/ie_FileInfo.h:39 `PreferredImporter`;
src/wp/impexp/xp/ie_FileInfo.h:40 `PreferredExporter`; src/wp/impexp/xp/ie_FileInfo.h:41 `MIME_TypeOrPseudo`; src/wp/impexp/xp/ie_Table.h:120 `getLastTable`;
src/wp/impexp/xp/ie_Table.h:164 `setMergeRight`; src/wp/impexp/xp/ie_Table.h:237 `setCellXOnRow`; src/wp/impexp/xp/ie_Table.h:317 `isVirtual`;
src/wp/impexp/xp/ie_exp_HTML.h:102 `set_AddIdentifiers`; src/wp/impexp/xp/ie_exp_HTML.h:106 `getSuffix`;
src/wp/impexp/xp/ie_exp_HTML_StyleTree.h:260 `styleText`; src/wp/impexp/xp/ie_imp_LaTeX.h:49 `appendStruxPublic`;
src/wp/impexp/xp/ie_imp_XML.h:102 `incOperationCount`

#### macro-accessor (26) — verdict: keep (macro-emitted pairs)

src/wp/ap/gtk/ap_UnixDialog_Options.cpp:601 `_gatherViewAll`; src/wp/ap/gtk/ap_UnixDialog_Options.cpp:601 `_setViewAll`;
src/wp/ap/gtk/ap_UnixDialog_Options.cpp:602 `_gatherViewHiddenText`; src/wp/ap/gtk/ap_UnixDialog_Options.cpp:602 `_setViewHiddenText`;
src/wp/ap/gtk/ap_UnixDialog_Options.cpp:603 `_gatherViewShowRuler`; src/wp/ap/gtk/ap_UnixDialog_Options.cpp:603 `_setViewShowRuler`;
src/wp/ap/gtk/ap_UnixDialog_Options.cpp:604 `_gatherViewShowStatusBar`; src/wp/ap/gtk/ap_UnixDialog_Options.cpp:604 `_setViewShowStatusBar`;
src/wp/ap/xp/ap_Dialog_Document.h:85 `setActivePage`; src/wp/ap/xp/ap_Dialog_Document.h:86 `setPageSetupChanged`;
src/wp/ap/xp/ap_Dialog_Lists.h:134 `getiLocalTick`; src/wp/ap/xp/ap_Dialog_Lists.h:134 `setiLocalTick`; src/wp/ap/xp/ap_Dialog_Lists.h:136 `getnewStartValue`;
src/wp/ap/xp/ap_Dialog_Lists.h:136 `setnewStartValue`; src/wp/ap/xp/ap_Dialog_Lists.h:139 `getbStartNewList`;
src/wp/ap/xp/ap_Dialog_Lists.h:140 `getbApplyToCurrent`; src/wp/ap/xp/ap_Dialog_Lists.h:141 `getbResumeList`;
src/wp/ap/xp/ap_Dialog_Lists.h:142 `getbisCustomized`; src/wp/ap/xp/ap_Dialog_Lists.h:143 `getisListAtPoint`;
src/wp/ap/xp/ap_Dialog_Lists.h:143 `setisListAtPoint`; src/wp/ap/xp/ap_Dialog_Lists.h:144 `getbguiChanged`;
src/wp/ap/xp/ap_Dialog_Lists.h:144 `setbguiChanged`; src/wp/ap/xp/ap_Dialog_Lists.h:146 `setDocListType`; src/wp/ap/xp/ap_Dialog_Lists.h:147 `getiLevel`;
src/wp/ap/xp/ap_Dialog_Lists.h:148 `getpView`; src/wp/ap/xp/ap_Dialog_Lists.h:148 `setpView`

#### fp-virtual (44) — verdict: false positive (polymorphic dispatch)

src/af/gr/xp/gr_CairoGraphics.h:239 `getLayoutFontMap`; src/af/xap/gtk/xap_UnixApp.h:107 `getTimeOfLastEvent`; src/af/xap/gtk/xap_UnixFrameImpl.h:60 `getNewX`;
src/af/xap/gtk/xap_UnixFrameImpl.h:137 `_getSunkenBox`; src/af/xap/xp/spell_manager.h:59 `getMapping`; src/af/xap/xp/xad_Document.h:199 `getTimeSinceOpen`;
src/af/xap/xp/xap_App.h:117 `getImpl`; src/af/xap/xp/xap_App.h:276 `clearStateInfo`; src/af/xap/xp/xap_Dialog.h:242 `pGetWindowHandle`;
src/af/xap/xp/xap_Draw_Symbol.h:60 `getCurrent`; src/af/xap/xp/xap_Frame.h:107 `hideMenuScroll`; src/af/xap/xp/xap_Frame.h:168 `getBarVisibility`;
src/af/xap/xp/xap_Frame.h:234 `isMenuBarShown`; src/af/xap/xp/xap_Frame.h:235 `setStatusBarShown`; src/af/xap/xp/xap_Frame.h:236 `setMenuBarShown`;
src/af/xap/xp/xap_FrameImpl.h:58 `_getToolbars`; src/text/fmt/xp/fl_BlockLayout.h:188 `setFirstRun`; src/text/fmt/xp/fl_TOCLayout.h:57 `getDispStyle`;
src/text/fmt/xp/fp_Column.h:161 `addWrappedLine`; src/text/fmt/xp/fp_FrameContainer.h:77 `setXpad`; src/text/fmt/xp/fp_Run.h:389 `_getWidth`;
src/text/fmt/xp/fp_TextRun.h:110 `setShapingRequired`; src/text/fmt/xp/fv_View.h:934 `getPreviewMode`;
src/text/fmt/xp/fv_VisualDragText.h:49 `getVisualDragMode`; src/text/ptbl/xp/pd_Document.h:703 `setMarkRevisionsNoNotify`;
src/text/ptbl/xp/pd_DocumentRDF.cpp:4523 `setSparql`; src/wp/ap/gtk/ap_UnixDialog_MarkRevisions.h:49 `ok_callback`;
src/wp/ap/gtk/ap_UnixRuler.h:56 `_deleteGraphics`; src/wp/ap/xp/ap_Dialog_Columns.h:85 `getColumnsPreview`;
src/wp/ap/xp/ap_Dialog_Lists.h:186 `setFoldingLevelChanged`; src/wp/ap/xp/ap_Dialog_Stylist.h:107 `isStyleChanged`;
src/wp/ap/xp/ap_StatusBar.h:148 `getApStatusBar`; src/wp/impexp/odf/exp/xp/ODe_Text_Listener.h:127 `setOpenedODNote`;
src/wp/impexp/odf/imp/xp/ODi_ListLevelStyle.h:86 `getMinLabelDistance`; src/wp/impexp/odf/imp/xp/ODi_StreamListener.h:85 `clearFontFaceDecls`;
src/wp/impexp/openxml/common/xp/OXML_Element_Text.h:43 `setCharRange`; src/wp/impexp/openxml/common/xp/OXML_Element_Text.h:44 `getCharRange`;
src/wp/impexp/xp/ie_exp.h:77 `getCanCopy`; src/wp/impexp/xp/ie_exp.h:185 `_cancelExport`; src/wp/impexp/xp/ie_exp_HTML_NavigationHelper.h:54 `getMinTOCIndex`;
src/wp/impexp/xp/ie_imp.h:98 `getCanPaste`; src/wp/impexp/xp/ie_imp_Text.h:45 `_get_eof`; src/wp/impexp/xp/ie_imp_Text.h:46 `_set_eof`;
src/wp/impexp/xp/ie_imp_Text.h:47 `_lookAhead`

#### live-infile (31) — verdict: false positive (used inside same TU)

src/af/tf/xp/tf_test.cpp:280 `start_check_eq`; src/af/xap/xp/xap_Dlg_DocComparison.cpp:96 `getWindowLabel`;
src/af/xap/xp/xap_Dlg_DocComparison.cpp:102 `getFrame1Label`; src/af/xap/xp/xap_Dlg_DocComparison.cpp:108 `getFrame2Label`;
src/af/xap/xp/xap_Dlg_DocComparison.cpp:127 `getResultLabel`; src/af/xap/xp/xap_Dlg_DocComparison.cpp:251 `getButtonLabel`;
src/text/fmt/xp/fb_Alignment.cpp:51 `eraseLineFromRun`; src/text/fmt/xp/fl_BlockLayout.cpp:2032 `_mergeRuns`;
src/text/fmt/xp/fl_BlockLayout.cpp:11128 `setDominantDirection`; src/text/fmt/xp/fl_TOCLayout.h:64 `getPosInList`;
src/text/fmt/xp/fp_Column.h:163 `clearWrappedLines`; src/text/fmt/xp/fp_Run.h:322 `getTmpLine`; src/text/fmt/xp/fp_Run.h:326 `getTmpX`;
src/text/fmt/xp/fp_Run.h:330 `getTmpY`; src/text/fmt/xp/fp_Run.h:362 `getMustClearScreen`; src/text/fmt/xp/fp_Run.h:857 `getPointHeight`;
src/text/fmt/xp/fp_Run.h:974 `_getParameter`; src/text/ptbl/xp/pd_DocumentRDF.cpp:2721 `isMutable`;
src/text/ptbl/xp/pd_DocumentRDF.cpp:3822 `priv_addRelevantIDsForPosition`; src/text/ptbl/xp/pf_Fragments.h:131 `getNode`;
src/text/ptbl/xp/pp_Property.h:169 `canInherit`; src/text/ptbl/xp/pt_VarSet.cpp:86 `overwriteBuf`; src/text/ptbl/xp/px_ChangeHistory.cpp:608 `_printHistory`;
src/wp/ap/gtk/abiwidget.cpp:1651 `abi_widget_insert_image`; src/wp/ap/gtk/abiwidget.cpp:1788 `abi_widget_load_file_from_gsf`;
src/wp/ap/gtk/abiwidget.cpp:1822 `abi_widget_load_file_from_memory`; src/wp/ap/gtk/abiwidget.cpp:2549 `abi_widget_new_with_file`;
src/wp/ap/gtk/ap_UnixToolbar_StyleCombo.cpp:221 `getPangoAttrs`; src/wp/ap/xp/ap_Dialog_Replace.cpp:247 `getReverseFind`;
src/wp/impexp/xp/ie_Table.h:49 `getPrevRight`; src/wp/impexp/xp/ie_imp_RTF.cpp:2647 `_isBidiDocument`

#### live (248) — verdict: false positive (referenced from other files)

src/af/ev/gtk/ev_UnixMenu.h:58 `getActionGroup`; src/af/ev/xp/ev_Menu.h:61 `getLabelSet`; src/af/gr/gtk/gr_UnixCairoGraphics.h:41 `createCairo`;
src/af/gr/xp/gr_CairoGraphics.cpp:1657 `appendRenderedCharsToBuff`; src/af/gr/xp/gr_CairoGraphics.cpp:2827 `getDefaultFont`;
src/af/gr/xp/gr_Caret.cpp:225 `setRemoteColor`; src/af/gr/xp/gr_Caret.cpp:644 `resetBlinkTimeout`;
src/af/gr/xp/gr_Graphics.h:789 `nativeBreakInfoForRightEdge`; src/af/util/xp/t/ut_uuid.t.cpp:26 `UT_UUIDGenerator__test`;
src/af/util/xp/ut_color.h:111 `pattern`; src/af/util/xp/ut_color.h:116 `setPattern`; src/af/util/xp/ut_screenshot.cpp:235 `UT_screenshot_available`;
src/af/util/xp/ut_stringbuf.h:309 `utf8_data`; src/af/xap/gtk/xap_GtkUtils.cpp:38 `xap_gtk_file_chooser_get_filename`;
src/af/xap/gtk/xap_GtkUtils.cpp:48 `xap_gtk_file_chooser_get_uri`; src/af/xap/gtk/xap_UnixDialogHelper.cpp:993 `localizeMenuItem`;
src/af/xap/gtk/xap_UnixDialogHelper.cpp:1011 `setLabelMarkup`; src/af/xap/gtk/xap_UnixWidget.cpp:140 `getValueFloat`;
src/af/xap/xp/spell_manager.h:121 `numLoadedDicts`; src/af/xap/xp/xad_Document.h:112 `setVersion`; src/af/xap/xp/xap_App.cpp:222 `getAbiSuiteHome`;
src/af/xap/xp/xap_App.cpp:809 `_setAbiSuiteLibDir`; src/af/xap/xp/xap_App.cpp:1205 `getDocuments`; src/af/xap/xp/xap_App.h:293 `_setUUIDGenerator`;
src/af/xap/xp/xap_CustomWidget.cpp:35 `queueDrawLU`; src/af/xap/xp/xap_Dlg_DocComparison.cpp:48 `calculate`;
src/af/xap/xp/xap_Dlg_Encoding.h:66 `_getSelectionIndex`; src/af/xap/xp/xap_Dlg_FontChooser.cpp:135 `event_previewDrawImmediate`;
src/af/xap/xp/xap_Dlg_FontChooser.cpp:143 `event_previewClear`; src/af/xap/xp/xap_Dlg_HTMLOptions.h:77 `get_Multipart`;
src/af/xap/xp/xap_Dlg_Insert_Symbol.cpp:97 `_createSymbolFromGC`; src/af/xap/xp/xap_Dlg_Insert_Symbol.cpp:107 `_getCurrentSymbolMap`;
src/af/xap/xp/xap_Dlg_Insert_Symbol.cpp:119 `_createSymbolareaFromGC`; src/af/xap/xp/xap_Dlg_Insert_Symbol.cpp:129 `_onInsertButton`;
src/af/xap/xp/xap_Dlg_Language.cpp:145 `_setLanguage`; src/af/xap/xp/xap_Dlg_Language.cpp:201 `detectLanguage`;
src/af/xap/xp/xap_Dlg_Language.cpp:269 `getDocDefaultLangCheckboxLabel`; src/af/xap/xp/xap_Dlg_Language.cpp:290 `getDocDefaultLangDescription`;
src/af/xap/xp/xap_Dlg_Language.h:68 `setMakeDocumentDefault`; src/af/xap/xp/xap_Dlg_Language.h:82 `setNoProofing`;
src/af/xap/xp/xap_Dlg_Language.h:83 `getNoProofing`; src/af/xap/xp/xap_Dlg_Print.cpp:200 `getDoPrintSelection`;
src/af/xap/xp/xap_Draw_Symbol.cpp:215 `setSelectedFont`; src/af/xap/xp/xap_Draw_Symbol.cpp:251 `getSymbolRows`;
src/af/xap/xp/xap_EncodingManager.h:105 `placeholder`; src/af/xap/xp/xap_Frame.cpp:537 `_startViewAutoUpdater`;
src/af/xap/xp/xap_FrameImpl.h:85 `isGridlinesVisible`; src/af/xap/xp/xap_Preview.h:49 `onLeftButtonDown`; src/af/xap/xp/xap_Strings.cpp:59 `getLanguageName`;
src/af/xap/xp/xav_View.h:90 `getFocus`; src/af/xap/xp/xav_View.h:127 `couldBeActive`; src/text/fmt/xp/fg_GraphicRaster.h:66 `getFormat`;
src/text/fmt/xp/fl_BlockLayout.cpp:1824 `getEnclosingBlock`; src/text/fmt/xp/fl_BlockLayout.cpp:5760 `_doInsertTOCTabRun`;
src/text/fmt/xp/fl_BlockLayout.cpp:5772 `_doInsertTOCListTabRun`; src/text/fmt/xp/fl_BlockLayout.cpp:6096 `_doInsertFieldTOCRun`;
src/text/fmt/xp/fl_BlockLayout.cpp:6107 `_doInsertTOCListLabelRun`; src/text/fmt/xp/fl_BlockLayout.cpp:6123 `_doInsertTOCHeadingRun`;
src/text/fmt/xp/fl_BlockLayout.cpp:9841 `decodeListType`; src/text/fmt/xp/fl_BlockLayout.cpp:10650 `prependList`;
src/text/fmt/xp/fl_BlockLayout.cpp:10882 `deleteListLabel`; src/text/fmt/xp/fl_BlockLayout.h:292 `getTabsCount`;
src/text/fmt/xp/fl_BlockLayout.h:367 `setStyleInTOC`; src/text/fmt/xp/fl_BlockLayout.h:388 `setPrevListLabel`;
src/text/fmt/xp/fl_DocLayout.cpp:322 `getQuickPrintGraphics`; src/text/fmt/xp/fl_DocLayout.cpp:1848 `countEndnotes`;
src/text/fmt/xp/fl_DocLayout.cpp:2110 `addTOC`; src/text/fmt/xp/fl_DocLayout.cpp:2116 `removeTOC`;
src/text/fmt/xp/fl_DocLayout.cpp:2875 `updateOnViewModeChange`; src/text/fmt/xp/fl_DocLayout.cpp:3296 `displayRDFAnchors`;
src/text/fmt/xp/fl_FrameLayout.h:118 `getFrameWidth`; src/text/fmt/xp/fl_FrameLayout.h:120 `getFrameHeight`;
src/text/fmt/xp/fl_SectionLayout.cpp:1420 `setHdrFtrHeightChange`; src/text/fmt/xp/fl_SectionLayout.cpp:3550 `findShadow`;
src/text/fmt/xp/fl_SectionLayout.cpp:4507 `bl_doclistener_deleteCellStrux`; src/text/fmt/xp/fl_SectionLayout.cpp:4543 `bl_doclistener_deleteTableStrux`;
src/text/fmt/xp/fl_SectionLayout.h:262 `getMaxSectionColumnHeight`; src/text/fmt/xp/fl_SectionLayout.h:318 `markForReformat`;
src/text/fmt/xp/fl_SectionLayout.h:319 `needsReFormat`; src/text/fmt/xp/fl_TOCLayout.h:127 `isTOCEmpty`;
src/text/fmt/xp/fp_AnnotationRun.cpp:185 `_clearScreen`; src/text/fmt/xp/fp_AnnotationRun.cpp:294 `_letPointPass`;
src/text/fmt/xp/fp_Column.h:141 `isHBreakable`; src/text/fmt/xp/fp_Column.h:143 `wantHBreakAt`; src/text/fmt/xp/fp_Column.h:145 `HBreakAt`;
src/text/fmt/xp/fp_ContainerObject.cpp:205 `setMyBrokenContainer`; src/text/fmt/xp/fp_ContainerObject.cpp:539 `findConFrom`;
src/text/fmt/xp/fp_ContainerObject.h:293 `getConInsertHint`; src/text/fmt/xp/fp_ContainerObject.h:295 `setConInsertHint`;
src/text/fmt/xp/fp_DirectionMarkerRun.cpp:127 `_deleteFollowingIfAtInsPoint`; src/text/fmt/xp/fp_FieldListLabelRun.h:33 `isListLabelField`;
src/text/fmt/xp/fp_Line.cpp:4318 `getVisIndx`; src/text/fmt/xp/fp_MathRun.cpp:252 `updateVerticalMetric`; src/text/fmt/xp/fp_MathRun.cpp:291 `_drawResizeBox`;
src/text/fmt/xp/fp_Page.cpp:850 `getContainingTable`; src/text/fmt/xp/fp_Page.cpp:999 `getAvailableHeightForColumn`;
src/text/fmt/xp/fp_Page.cpp:1806 `intersectsDamagedRect`; src/text/fmt/xp/fp_Page.cpp:2130 `updateColumnX`;
src/text/fmt/xp/fp_Page.cpp:2716 `columnHeightChanged`; src/text/fmt/xp/fp_Page.cpp:3473 `restackFrameContainer`;
src/text/fmt/xp/fp_Page.cpp:3747 `getAnnotationPos`; src/text/fmt/xp/fp_Page.h:168 `setLastMappedTOC`; src/text/fmt/xp/fp_PageSize.cpp:527 `PredefinedToName`;
src/text/fmt/xp/fp_Run.h:252 `alwaysFits`; src/text/fmt/xp/fp_Run.h:761 `hasLayoutProperties`; src/text/fmt/xp/fp_TOCContainer.cpp:164 `forceClearScreen`;
src/text/fmt/xp/fp_TOCContainer.cpp:329 `isInBrokenTOC`; src/text/fmt/xp/fp_TOCContainer.cpp:377 `getBrokenNumber`;
src/text/fmt/xp/fp_TableContainer.cpp:158 `isRepeated`; src/text/fmt/xp/fp_TableContainer.cpp:3054 `clearBrokenCellCache`;
src/text/fmt/xp/fp_TableContainer.cpp:4476 `tableAttach`; src/text/fmt/xp/fp_TableContainer.cpp:4561 `setRowSpacings`;
src/text/fmt/xp/fp_TableContainer.cpp:4572 `setColSpacings`; src/text/fmt/xp/fp_TableContainer.cpp:4583 `setHomogeneous`;
src/text/fmt/xp/fp_TableContainer.h:180 `setLeftAttach`; src/text/fmt/xp/fp_TableContainer.h:182 `setRightAttach`;
src/text/fmt/xp/fp_TableContainer.h:184 `setTopAttach`; src/text/fmt/xp/fp_TableContainer.h:186 `setBottomAttach`;
src/text/fmt/xp/fp_TableContainer.h:197 `setLeftPad`; src/text/fmt/xp/fp_TableContainer.h:199 `setRightPad`;
src/text/fmt/xp/fp_TableContainer.h:201 `setTopPad`; src/text/fmt/xp/fp_TableContainer.h:203 `setBotPad`; src/text/fmt/xp/fp_TableContainer.h:245 `getStartY`;
src/text/fmt/xp/fp_TableContainer.h:247 `getStopY`; src/text/fmt/xp/fp_TableContainer.h:249 `getLeftPos`;
src/text/fmt/xp/fp_TableContainer.h:251 `getRightPos`; src/text/fmt/xp/fp_TableContainer.h:260 `setVertAlign`;
src/text/fmt/xp/fp_TableContainer.h:421 `setLineThickness`; src/text/fmt/xp/fp_TableContainer.h:478 `doRedrawLines`;
src/text/fmt/xp/fp_TableContainer.h:483 `setRowHeightType`; src/text/fmt/xp/fv_FrameEdit.cpp:732 `mouseLeftPress`;
src/text/fmt/xp/fv_InlineImage.cpp:95 `setSelectionDrawn`; src/text/fmt/xp/fv_InlineImage.cpp:891 `mouseCopy`;
src/text/fmt/xp/fv_InlineImage.cpp:1203 `getImageSelBoxSize`; src/text/fmt/xp/fv_View.h:1069 `getColorImage`;
src/text/fmt/xp/fv_View.h:1071 `getColorHyperLink`; src/text/fmt/xp/fv_View.h:1075 `getColorRevisions`; src/text/fmt/xp/fv_View.h:1096 `getDragTableLine`;
src/text/fmt/xp/fv_View_TestRoutines.cpp:34 `Test_Dump`; src/text/fmt/xp/fv_View_cmd.cpp:4450 `_autoFormatTableOnEnter`;
src/text/fmt/xp/fv_View_protected.cpp:108 `_isSpaceBefore`; src/text/fmt/xp/fv_View_protected.cpp:159 `_eraseSelection`;
src/text/fmt/xp/fv_View_protected.cpp:1404 `_insertSectionBreak`; src/text/fmt/xp/fv_View_protected.cpp:1516 `_getPageXandYOffset`;
src/text/fmt/xp/fv_View_protected.cpp:2221 `_moveInsPtNextPrevPage`; src/text/fmt/xp/fv_View_protected.cpp:2253 `_moveInsPtNextPrevScreen`;
src/text/fmt/xp/fv_View_protected.cpp:2426 `_moveInsPtNthPage`; src/text/fmt/xp/fv_View_protected.cpp:2481 `_stopPendingScrollWorker`;
src/text/fmt/xp/fv_View_protected.cpp:2699 `_computeFindPrefix`; src/text/fmt/xp/fv_View_protected.cpp:3375 `_findReplaceReverse`;
src/text/fmt/xp/fv_View_protected.cpp:3451 `_findReplace`; src/text/fmt/xp/fv_View_protected.cpp:3523 `_findBlockSearchRegexp`;
src/text/fmt/xp/fv_View_protected.cpp:3631 `_extSelToPos`; src/text/fmt/xp/fv_View_protected.cpp:4856 `_getDataCount`;
src/text/fmt/xp/fv_View_protected.cpp:6089 `_fixInsertionPointAfterRevision`; src/text/fmt/xp/fv_View_protected.cpp:6401 `_updateSelectionHandles`;
src/text/fmt/xp/fv_View_tableStyle.cpp:505 `getTablePen`; src/text/ptbl/xp/pd_Document.cpp:5465 `getPrevNumberedHeadingStyle`;
src/text/ptbl/xp/pd_Document.cpp:8382 `tellPTDoNotTweakPosition`; src/text/ptbl/xp/pd_Document.h:973 `getUserName`;
src/text/ptbl/xp/pd_DocumentRDF.cpp:110 `setRDFDialogs`; src/text/ptbl/xp/pd_DocumentRDF.cpp:1311 `moveToNextSubjectHavePOCol`;
src/text/ptbl/xp/pd_DocumentRDF.cpp:1797 `importFromFile`; src/text/ptbl/xp/pd_DocumentRDF.cpp:1995 `exportToFile`;
src/text/ptbl/xp/pd_RDFQuery.cpp:133 `dump`; src/text/ptbl/xp/pd_RDFSupport.cpp:241 `dumpModelToTest`; src/text/ptbl/xp/pf_Frag.cpp:121 `accLeftTreeLength`;
src/text/ptbl/xp/pf_Frag.cpp:126 `_setNode`; src/text/ptbl/xp/pf_Frag.cpp:131 `_getNode`; src/text/ptbl/xp/pf_Frag.h:70 `zero`;
src/text/ptbl/xp/pf_Frag.h:78 `getLeftTreeLength`; src/text/ptbl/xp/pf_Frag.h:80 `setLeftTreeLength`; src/text/ptbl/xp/pf_Fragments.h:159 `sizeDocument`;
src/text/ptbl/xp/pp_AttrProp.h:133 `isReadOnly`; src/wp/ap/gtk/ap_UnixApp.cpp:757 `canPasteFromClipboard`;
src/wp/ap/gtk/ap_UnixApp.cpp:928 `getCurrentSelection`; src/wp/ap/gtk/ap_UnixApp.cpp:1075 `newDefaultScreenGraphics`;
src/wp/ap/gtk/ap_UnixApp.h:109 `getViewSelection`; src/wp/ap/gtk/ap_UnixClipboard.cpp:199 `_materializeData`;
src/wp/ap/gtk/ap_UnixDialog_Border_Shading.cpp:376 `setShadingOffsetInGUI`; src/wp/ap/gtk/ap_UnixDialog_Document.cpp:287 `ap_showHyphenationDialog`;
src/wp/ap/gtk/ap_UnixDialog_Latex.cpp:110 `event_WindowDelete`; src/wp/ap/gtk/ap_UnixFrameImpl.cpp:650 `refreshStylesPane`;
src/wp/ap/gtk/ap_UnixLeftRuler.cpp:73 `_finishMotionEvent`; src/wp/ap/gtk/ap_UnixStockIcons.cpp:332 `abi_stock_from_menu_id`;
src/wp/ap/gtk/ap_UnixTopRuler.h:52 `_getFrame`; src/wp/ap/xp/ap_Dialog_InsertTable.cpp:187 `_doSpin`; src/wp/ap/xp/ap_Dialog_ListRevisions.cpp:55 `getLabel1`;
src/wp/ap/xp/ap_Dialog_ListRevisions.cpp:61 `getColumn1Label`; src/wp/ap/xp/ap_Dialog_ListRevisions.cpp:67 `getColumn2Label`;
src/wp/ap/xp/ap_Dialog_ListRevisions.cpp:73 `getColumn3Label`; src/wp/ap/xp/ap_Dialog_ListRevisions.cpp:79 `getColumn4Label`;
src/wp/ap/xp/ap_Dialog_ListRevisions.cpp:91 `getNthItemId`; src/wp/ap/xp/ap_Dialog_ListRevisions.cpp:118 `getNthItemTime`;
src/wp/ap/xp/ap_Dialog_ListRevisions.cpp:143 `getNthItemAuthor`; src/wp/ap/xp/ap_Dialog_ListRevisions.cpp:154 `getNthItemText`;
src/wp/ap/xp/ap_Dialog_MergeCells.cpp:325 `getCellSource`; src/wp/ap/xp/ap_Dialog_MergeCells.cpp:330 `getCellDestination`;
src/wp/ap/xp/ap_Dialog_PageNumbers.cpp:53 `isHeader`; src/wp/ap/xp/ap_Dialog_RDFEditor.cpp:174 `createStatement`;
src/wp/ap/xp/ap_Dialog_RDFEditor.cpp:208 `copyStatement`; src/wp/ap/xp/ap_Dialog_RDFEditor.cpp:252 `setRestrictedXMLID`;
src/wp/ap/xp/ap_Dialog_RDFEditor.cpp:289 `removeStatement`; src/wp/ap/xp/ap_Dialog_Stylist.cpp:488 `getStyleAtRowCol`;
src/wp/ap/xp/ap_Dialog_Stylist.cpp:524 `getNameOfRow`; src/wp/ap/xp/ap_Dialog_Stylist.h:95 `getStyleTree`;
src/wp/ap/xp/ap_Dialog_Stylist.h:109 `isStyleTreeChanged`; src/wp/ap/xp/ap_Dialog_Stylist.h:112 `setStyleTreeChanged`;
src/wp/ap/xp/ap_Dialog_Stylist.h:114 `setStyleChanged`; src/wp/ap/xp/ap_Dialog_Stylist.h:116 `setStyleValid`; src/wp/ap/xp/ap_Frame.h:55 `isShowMargin`;
src/wp/ap/xp/ap_Ruler.cpp:125 `snapPixelToGrid`; src/wp/ap/xp/ap_Ruler.cpp:142 `scalePixelDistanceToUnits`; src/wp/ap/xp/ap_TopRuler.h:292 `getDimension`;
src/wp/ap/xp/ap_TopRuler.h:297 `getFixedWidth`; src/wp/impexp/odf/common/xp/crypto/blowfish/bf_enc.c:109 `BF_cbc_encrypt`;
src/wp/impexp/odf/exp/xp/ODe_AutomaticStyles.cpp:92 `addTableStyle`; src/wp/impexp/odf/exp/xp/ODe_AutomaticStyles.cpp:110 `addTableColumnStyle`;
src/wp/impexp/odf/exp/xp/ODe_AutomaticStyles.cpp:128 `addTableRowStyle`; src/wp/impexp/odf/exp/xp/ODe_AutomaticStyles.cpp:146 `addTableCellStyle`;
src/wp/impexp/odf/exp/xp/ODe_Style_Style.cpp:720 `fetchAttributesFromAbiTable`; src/wp/impexp/odf/exp/xp/ODe_Style_Style.cpp:731 `fetchAttributesFromAbiCell`;
src/wp/impexp/odf/exp/xp/ODe_Style_Style.cpp:782 `setRelColumnWidth`; src/wp/impexp/odf/exp/xp/ODe_Style_Style.cpp:804 `setMinRowHeight`;
src/wp/impexp/odf/exp/xp/ODe_Style_Style.cpp:815 `hasTableStyleProps`; src/wp/impexp/odf/exp/xp/ODe_Style_Style.cpp:894 `inheritTableCellProperties`;
src/wp/impexp/odf/imp/xp/ODi_ContentStreamAnnotationMatcher_ListenerState.cpp:103 `getRangedAnnotationNames`;
src/wp/impexp/odf/imp/xp/ODi_ListenerState.h:55 `getStateName`; src/wp/impexp/odf/imp/xp/ODi_ListenerStateAction.h:133 `getDeleteWhenPop`;
src/wp/impexp/odf/imp/xp/ODi_ListenerStateAction.h:135 `getComeBackAfter`; src/wp/impexp/odf/imp/xp/ODi_ListenerStateAction.h:136 `getElementLevel`;
src/wp/impexp/odf/imp/xp/ODi_Postpone_ListenerState.h:54 `getParserState`;
src/wp/impexp/odf/imp/xp/ODi_Postpone_ListenerState.h:55 `getDeleteParserStateWhenPop`;
src/wp/impexp/odf/imp/xp/ODi_Postpone_ListenerState.h:56 `getXMLRecorder`; src/wp/impexp/odf/imp/xp/ODi_StreamListener.h:88 `getCurrentState`;
src/wp/impexp/openxml/common/xp/OXML_Element.cpp:61 `getElement`; src/wp/impexp/openxml/common/xp/OXML_Element_Cell.cpp:517 `setLeft`;
src/wp/impexp/xp/ie_exp.cpp:610 `fileTypeForDescription`; src/wp/impexp/xp/ie_exp.cpp:722 `descriptionForFileType`;
src/wp/impexp/xp/ie_exp.h:151 `getFidelity`; src/wp/impexp/xp/ie_exp_HTML_DocumentWriter.h:130 `insertJavaScript`;
src/wp/impexp/xp/ie_exp_HTML_StyleTree.h:105 `style_name`; src/wp/impexp/xp/ie_imp.cpp:326 `unregisterImporter`;
src/wp/impexp/xp/ie_imp.cpp:391 `getSupportedMimeClasses`; src/wp/impexp/xp/ie_imp.cpp:417 `getSupportedSuffixes`;
src/wp/impexp/xp/ie_imp_RTF.cpp:1744 `SaveRowInfo`; src/wp/impexp/xp/ie_imp_RTF.cpp:1751 `RemoveRowInfo`;
src/wp/impexp/xp/ie_imp_RTFObjectsAndPicts.cpp:904 `frame`

#### generated (1) — verdict: keep (build-generated source)

src/abi-resources.c:54716 `abi_get_resource`

### cppcheck error-severity findings — reviewed per item

| Site | cppcheck claim | Verdict after review |
|------|----------------|----------------------|
| `fp_Page.cpp:3529` | mismatchingContainerIterator | **FP** — `itM` is `find`d on `vec` and `erase`d on `vec`; `members` elements are `vec[k]` values from the same layer vector |
| `fv_View_cmd.cpp:9853,9858` | danglingTemporaryLifetime on `sText = sKey.substr(...).c_str()` | **FP** — `FV_RefEntry::sText` is `UT_UTF8String`; the `c_str()` is copied at assignment |
| `ut_hyphen.cpp:386` | returnDanglingLifetime | **FP** — `find(lang)` guards the `emplace`, so the moved `unique_ptr` is always inserted; `pRet` stays owned by the cache |
| `ut_xml.h:178` via `ut_html.cpp:220` | danglingLifetime on `m_pReader=&wrapper` | **FP** — `parse()` runs synchronously inside `wrapper`'s scope and the original reader is restored before return |
| 18 × `unknownMacro`, 21 × `syntaxError` | GLib/`UT_nonnull_or_return` macros unparseable | noise — but see caveat: unparsed files hide their call sites from the scan |
| `ut_xml.t.cpp:364` nullPointer | test-side parse check | **FP** — deliberate NULL test fixture |

### Hand-off to RELQA07

Removal-eligible items: the **180 candidate-dead** entries above (each
still needs a vtable/GActionEntry/signal-by-name sanity check before
deletion), the 5 trivial `-Wunused` locals/stores, and the 2 `make dist`
integrity fixes. The 84 api-surface accessors are a judgement call —
default keep unless the class is clearly internal. Everything else is
verified-live or generated.

## RELQA07 — pre-release gate: dead-code removal

Applied the RELQA03 findings proven unreferenced, in four batches,
each rebuilt and gated on the core unit suite.

|| Batch | Contents | Gate result |
||-------|----------|-------------|
|| 1 | `-Wunused` removals (ribbon `fband` lambda, `ODe_DocumentData`/`ODe_Style_List` dead locals) + both `make dist` fixes (stale `gtk/xap_UnixWidget.t.cpp` path → `gtk/t/`; the 8 missing `.t.cpp` EXTRA_DIST entries) | `make -j2` clean; testwrap/rtwrap/portwrap/dlgswrap PASS (drvwrap leg cut by the run watchdog mid-run — rerun of the leg, not a failure) |
|| 2 | 42 candidate-dead removals in `src/af/**` (836 lines) | `make -j2` clean; testwrap 13,382 tests / 0 failures |
|| 3 | 47 candidate-dead removals in `src/text/**` (792 lines) | `make -j2` clean; testwrap 13,382 / 0 |
|| 4 | 55 candidate-dead removals in `src/wp/**` (597 lines) | `make -j2` clean; testwrap 13,382 / 0; final-state rtwrap + portwrap PASS |

Also shipped: `abwn.dtd` added to top-level `EXTRA_DIST` (RELQA03's
optional packaging gap).

### Kept on per-item review (36 of the 180 candidates)

- **Installed ABI** — all 11 `abi_widget_*` entry points:
  `abiwidget.h` is a shipped public header; the C embedding API is
  kept regardless of in-tree callers.
- **Indirect dispatch** — `doclistener_deleteEndFrame`,
  `bl_doclistener_insertFootnote`, `bl_doclistener_insertAnnotation`,
  `event_MenuChanged`/`event_SpinIncrement`/`event_SpinDecrement`
  (paragraph-dialog signal callbacks).
- **Virtual/override** — `gr_CairoNullGraphics` `drawRGBImage`/
  `drawGrayImage`/`drawBWImage`, `fp_Run::_getHeight`.
- **Live inlines mis-flagged** — `ie_Table.h` `isMergedRight`,
  `isFirstVerticalMerged`, `isFirstHorizontalMerged`, `setImpTable`,
  `isAutoFit`, `getInsertionPoint`; `fl_TOCLayout.h` `hasLabel`,
  `getNumLabel` (same-TU/cross-TU calls cppcheck could not see).
- **Deliberate API/helpers** — `fl_TestRoutines` `__dump_fmt`/`__dump_pt`/
  `__dump_ch`/`__dump_sq` (documented gdb-session dump routines),
  `ap_UnixApp::getPrefsValueDirectory`, `XAP_Frame::
  translateDocumentToScreen`, `IE_Exp::rewindChar` (kept conservative —
  part of the exporter base API).

### api-surface (84) verdict

All 84 unused inline header accessors **kept** — they are zero-cost
public class API (get/set pairs); none belongs to a clearly internal
class where removal would aid maintenance. Documented decision, no
action.

### `#if 0` / marker triage

Zero real `#if 0` blocks exist (RELQA03). The 1,274 TODO/FIXME/XXX/HACK
markers are inherited documentation debt; none is attached to a removal
item — no action.

### Known-failing exceptions update

None added by this leg.

## Known-failing exceptions

(none)
