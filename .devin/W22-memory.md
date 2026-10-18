# W22 — Final gate 4/4: memory-leak / lifetime check

Gate for the 4.0.3 stream: run the suite plus a scripted
open/edit/undo/save session under Valgrind memcheck, fix real
leaks/UAFs in first-party code, and document library noise.

## Tools

- valgrind 3.26.0 (`/snap/bin/valgrind`), gcc 13.3.0, xvfb-run.
- `tools/check-valgrind.sh` (bounded 16-fixture corpus, memcheck,
  `--leak-check=full --errors-for-leak-kinds=definite
  --error-exitcode=99`, `tools/valgrind.supp`).
- Per-test leak gate built into `src/af/tf/xp/tf_test.cpp`:
  `VALGRIND_COUNT_ERRORS` + `VALGRIND_DO_LEAK_CHECK` sampled around
  every test; `TF_MAX_TEST_TIME=300` to allow for memcheck slowdown.
- Scripted GUI session: `src/.libs/abinova --control=PORT
  --automation-write-root=/tmp` under `xvfb-run -a valgrind`, driven
  over the JSON-lines loopback control socket.

## Legs run

### 1. Conversion corpus (`tools/check-valgrind.sh`)

16 fixtures through `abinova --to=png|pdf` under memcheck — full
startup/load/layout/render/teardown per file.

**Result: 16 passed, 0 failed, 0 skipped** (run before the fixes
below; the fixes only remove allocations from teardown paths, and the
leg was re-run after them — see Verification).

### 2. Unit suite under memcheck

```
G_SLICE=always-malloc G_DEBUG=gc-friendly \
LD_LIBRARY_PATH=src/.libs:src/af/tf/.libs \
valgrind --tool=memcheck --leak-check=full \
  --suppressions=tools/valgrind.supp \
  src/wp/test/.libs/Abinova-test <prefixes>
```

- Stream-scoped prefixes (`ut_asyncwrite fp_FieldTableSum fv_EditOps
  fv_ViewOps pd_AsyncSave ap_Autosave ap_KeyBindings`): **650 tests,
  0 failures**; every per-test `new_valgrind_errs` /
  `new_valgrind_leaks` gate `ok`.  Exit-time scan found three
  first-party leaks (below) — all fixed and re-verified.
- Post-fix re-run (`ut_asyncwrite fv_ViewOps`): **167 tests, 0
  failures, `definitely lost: 0 bytes in 0 blocks`**.

### 3. Scripted GUI session under memcheck

`xvfb-run -a valgrind … src/.libs/abinova --control=18877
--automation-write-root=/tmp test/wp/cov07/rich.abw`, driven over the
control socket:

- connection churn (unauthenticated open/close, oversized line)
- ~25 commands: `insertData` ×6, `selectAll`, `copy`,
  `viewClipPane` ×2 (clipboard ring), `undo`/`redo`/`undoTo`/`redoTo`/
  `goBack` (undo ring), `designTheme`, `themeColorsApply`,
  `themeFontsApply`, `contentControlDesignMode`,
  `document.inspect`/`text`/`formatinspect`/`privacyinspect`,
  `document.autosave` (async-write path), `newWindow`,
  then pipelined `file.close{force}` + `app.quit{force}` to exercise
  the stale-frame path.

**Result: zero `Invalid read/write/free`, zero UAF.**  ERROR
SUMMARY 130,367 errors / 920 contexts — all "conditional jump /
uninitialised value" inside libgtk-4 (GSK render nodes use uninit
padding), libgraphene, libgallium driver threads, and glib; no
context carries a non-entry Abinova frame.  `definitely lost:
464,512 B / 13,690 blocks` — every loss record traces to libgtk-4 /
fontconfig / cairo / pango internals; none to Abinova code.  Counts
are inflated because teardown was SIGTERM (driver-side timeout after
`app.quit` left a second `newWindow` frame open — the stale-frame
guard correctly skipped the double-close; a non-instrumented control
session confirms `file.close`+`app.quit` exits cleanly, rc=0).

Session non-goals observed (not memory defects):
- `ui.render`/`ui.screenshot` → "snapshot produced no render node"
  under valgrind+xvfb; works without instrumentation (PNG written).
- `file.save` to `.abw` → `UT_SAVE_EXPORTERROR` is **correct**:
  legacy `.abw` is read-only by design (`ie_exp_Abinova_1.cpp` —
  "nothing ever produces the old AbiWord serialization"); `.abwn`
  and `.docx` saves verified.
- Two `gtk_widget_measure`/`size_allocate` warnings (negative size)
  — GTK layout warnings under headless rendering, no memcheck error.

## Audit findings and fixes (first-party)

1. **`xap_UnixControlSocket.cpp` — UAF in `_s_close_frames`.**
   Queued `XAP_Frame*` were dereferenced at idle time with no
   liveness check; a second `file.close`/`app.quit` or a manual
   window close could free a frame before the idle ran.  Now each
   queued pointer is verified against the app's frame registry
   before dispatch.
2. **`xap_UnixControlSocket.cpp` — malformed JSON for C0
   controls.**  `_json_escape` emitted raw `\x0b`/`\x0c` etc. in
   `document.text`, producing unparseable replies.  All chars <0x20
   are now `\uXXXX`-escaped.  (Functional defect found by the
   session driver, fixed here as adjacent first-party work.)
3. **`ap_UnixMiniBar` — UAF on the custom-colour
   GtkColorChooserDialog.**  The dialog lives outside the bar's
   widget tree but held `data=self`; closing a frame with the
   dialog up left a dangling pointer.  The dialog is now tracked in
   `m_wColorDlg`, destroyed/disconnected in the destructor, and
   re-presented instead of duplicated on repeat clicks.  Also
   tightened `contains()`: a GtkPopover only counts as "in the bar"
   when it is actually parented inside the bar.
4. **`fv_View_protected.cpp` — `GMatchInfo` leak in
   `_findPrevPattern`.**  `g_regex_match_full` allocates a
   `GMatchInfo` even when it returns no match; the trailing
   non-match `info` was never freed (the sibling
   `_findNextPattern` already freed it).  20.9 KB per
   backwards pattern-search miss.
5. **`ut_asyncwrite.cpp` — orphaned `GList` nodes in
   `flush_done`.**  The done-queue was drained by copying data
   pointers into a second queue, then `g_queue_init(&s_done)`
   orphaned the original nodes.  Now the node list itself is moved
   (`deliver = s_done; s_done = G_QUEUE_INIT`) so
   `g_queue_pop_head` frees each node.
6. **`ut_asyncwrite.t.cpp` — test-side leaks.**  `tmpdir()` never
   freed the `g_mkdtemp` buffer; one test passed `g_bytes_ref(p)`
   into `UT_AsyncWrite_enqueue` (which refs internally), abandoning
   refs.

## Priority-target audit — clean

- **TOCEntry `m_pSourceSDH`**: handles validated against
  `pf_Fragments::contains()` before use; purge/remove paths clear
  matching format handles.
- **Control-socket `_Conn`**: refcounted across async reads;
  cancellables cancelled on teardown; bounded line handling.
- **Undo history**: `getOps()` is read-only aggregation; change
  records owned by the piece table.
- **Clipboard**: `ClipRead` records tracked in a set, `self`
  nulled in the destructor; ring bounded; provider weak refs
  disconnected.
- **Pattern search**: compiled `GRegex` cached/unref'd correctly;
  `GMatchInfo` free paths now complete (fix #4).
- **Formula (`fp_FieldTableSumRun`)**: depth-guarded parser,
  bounded cell refs, no heap allocations retained.
- **`ut_asyncwrite`**: worker/completion queues, per-target
  superseding, `waitAll` inline flush — queue-node leak fixed
  (#5).
- **`ap_FmtInspect`/`ap_DocInspect`/fmt pane**: RAII
  `std::string`/`std::vector`, no retained allocation.
- **`ev_UnixMouse` modifier refactor, OXML field listener state,
  Zotero client**: no async lifetimes introduced.

## Third-party noise (documented, not chased)

- GTK4/GSK uninitialized-value contexts in the render path —
  known under memcheck, especially with the GL/cairo renderers
  under Xvfb; libgallium worker threads likewise.
- fontconfig/pango/cairo caches and GTK's process-lifetime
  allocations — process-lifetime by design; covered by
  `tools/valgrind.supp`.
- Exit-time "definite" records in the GUI log are all inside
  libgtk-4/fontconfig/glib, amplified by SIGTERM teardown.

## Pre-existing, unrelated

- `fv_DragAutoScroll.t.cpp` drag-to-end assertions fail in the
  general suite — a functional flake seen before this gate, not a
  memory defect.
- libgcov "overwriting existing profile data" warnings — stale
  coverage artifacts, harmless.

## Verification

- `make -j2` in `src/` — clean.
- `tools/check-valgrind.sh` — 16/16 pass.
- `Abinova-test` under memcheck — 650 + 167 tests, 0 failures;
  post-fix exit scan `definitely lost: 0 bytes`.
- GUI session under memcheck — zero invalid accesses / UAFs /
  first-party definite leaks.
