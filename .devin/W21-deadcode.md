# W21 — Final gate 3/4: dead-code sweep

Scope: the W-stream diff `v4.0.2..HEAD` (~137 files, ~21k added lines —
ribbon/control-socket/mini-toolbar/TOC/dialogs/etc.).

## Method

- Extracted every symbol introduced by the stream: **90** member
  declarations (`.h` diff), **202** method declarations, **34** file-local
  `s_*` static functions — then counted referencers across `src/` and
  hand-triaged everything with <=3 hits.
- Grepped added lines for unguarded output: `fprintf(stderr|g_printerr|
  std::cerr|printf`, plus `#if 0`/feature-test branches and
  FIXME/XXX/TODO/HACK markers.
- Checked the reverse: stream helpers that duplicate existing utility
  code.

## Fixed (this + previous interrupted run, verified)

| Area | What was dead |
|------|----------------|
| `fp_Run.h` + `fp_FieldZoteroRun.cpp` | members `m_sKey`, `m_sStyle` + getter `getZoteroKey` — the zotero-key/-style attrs are read straight off the field AP by `fv_View_cmd`/`zoteroRefresh`; the run never consulted them |
| `fp_Run.h` + `fp_FieldContentControlRun.cpp` | members `m_sTitle`, `m_sTag`, `m_sItems` + getters `isCheckbox`, `isChecked`, `getCtlType`, `getTag` — same story: attrs stay on the field object, read by the edit methods and the ribbon props popover |
| both run .cpps | 5 now-unneeded includes (`fp_Line.h`, `fl_BlockLayout.h`, `ut_debugmsg.h`) |
| `ap_UnixFrameImpl.{h,cpp}` | `hideMiniBar()` — decl+def, zero callers; hiding goes through `updateMiniBar` → `m_pMiniBar->hide()` and `_s_miniBarTopPressed` |
| `ap_Dialog_DocInspector.{h,cpp}` | `getCategoryMask(i)` and `isSelected(i)` — zero callers; the Unix front-end builds rows from count/title/found/summary and `getSelectedMask` reads masks internally |
| `fp_FieldTableSum.t.cpp` | 3 ad-hoc `fprintf(stderr,"table*sum fields…")` dumps |

## Audited clean — deliberately kept

- **All 34 added file-local statics**: every one called (`-Wunused-function`
  also clean).
- **All other added methods/members**: decl+def+≥1 caller. GTK dialog
  callbacks are inline-defined statics wired via `G_CALLBACK`; control-
  socket-facing edit methods are reachable by name.
- **`UT_DEBUGMSG` additions**: project-standard debug idiom, compiles out
  in release — not "stderr chatter on a normal run".
- **`ui_drive.cpp` fprintf/g_printerr**: test-driver diagnostics,
  intentional.
- **No added `#if 0`/dead preprocessor branches** (all added `#if`s are
  header guards or platform conditionals), **no added FIXME/XXX/TODO/HACK
  markers**.
- **Helper dedup**: `s_utf8SkipChar` already wraps `g_utf8_next_char`;
  no other trivially-safe duplication candidates found.
- `m_bChecked` (ContentControl run) kept: written+read in
  `calculateValue`, harmless member.

## Verify

- `cd src && make -j2` clean; no new warnings (66 warnings = W19's
  classified baseline: deprecated GTK getters, `-Wsign-compare`,
  `-Wzero-as-null-pointer-constant` in `ut_hash.h`).
- `Abinova-test`: `core.text.fmt.tablesum` both tests PASS (load + all
  asserts) with no `tablesum fields:` chatter; only pre-existing
  timing-flaky `fv_DragAutoScroll` fails (documented since W15).
- `Badge.docx`/`Austin.docx` → PDF rc=0, PNG render correct.
- Normal convert stderr carries no stream-added prints (only gcov
  coverage-build noise from `.gcda` checksum drift).
