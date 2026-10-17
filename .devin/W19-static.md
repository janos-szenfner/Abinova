# W19 — Final gate 1/4: static analysis sweep

Date: 2026-10-18
Scope: all source files touched by the WordCraft parity stream,
`git diff v4.0.2..HEAD` = 115 files / 69 translation units.

## Tools

| Tool | Status | Coverage |
|------|--------|----------|
| `gcc 13.3 -fanalyzer` | ran | 19 TUs: all 14 stream-authored non-test files + xap_UnixControlSocket, xap_UnixClipboard, fp_FieldTableSumRun, px_ChangeHistory, pd_Document |
| `clang 18 --analyze` | ran | all 69 touched TUs (test TUs needed `-I src/af/tf/xp` for tf_test.h) |
| `cppcheck` | not installed | — |
| `clang-tidy` | not installed | — |

Compile lines were extracted per-Makefile via `make -n` (same flags as
the real build, minus `--coverage`).

## Compiler-warning sweep

`cd src && make -j2` with all 69 touched TUs forced to recompile:
170 warnings emitted. Each warning site was classified by
`git blame` vs the v4.0.2 baseline.

### NEW warnings — all fixed

| Site | Warning | Fix |
|------|---------|-----|
| xap_UnixControlSocket.cpp:807 | -Wsign-compare | loop var `UT_uint32 i` -> `UT_sint32 i` (matches getFrameCount/getFrame) |
| ap_UnixDialog_DocInspector.cpp:62 | -Wunused-variable `rc` | `UT_UNUSED(rc)` (rc only feeds UT_DEBUGMSG) |
| ap_UnixDialog_Zotero.cpp:65 | -Wunused-variable `rc` | `UT_UNUSED(rc)` |
| fp_FieldTableSumRun.cpp:1234 | -Wformat-truncation `%.*f` | real defect: picture `decimals` unbounded by fmt length and buf[128] could not hold %f of a large double; clamped decimals<=15 and buf->512 |

### Baseline warnings in touched code — also cleaned where free

- fv_View_cmd.cpp: 41x -Wmissing-field-initializers on FV_HdrLine
  preset tables (pre-existing; `szFieldProps` predates the tag).
  Fixed anyway: `szFieldProps` now has a `= nullptr` default member
  initializer — keeps aggregate-ness under C++17, zero call-site churn.
- Remaining baseline warnings left as-is: -Wsign-compare loops
  (ap_UnixRibbon x6, ap_Dialog_Replace x3, xap_Frame x2, gr_RenderInfo.h,
  ap_Menu_Functions, ap_UnixDialog_{Replace,Spell}, fv_View_protected),
  -Wunused-parameter (ap_EditMethods:1759, xap_UnixControlSocket:1326,
  ap_UnixRibbon:12019), -Wshadow x2 — all on lines that predate v4.0.2.

### GTK deprecation warnings — wontfix (documented)

New stream files emit the same -Wdeprecated-declarations noise the rest
of the tree does (gtk_dialog_get_content_area, GtkColorChooser,
gtk_widget_get_allocation, GtkStyleContext). These are the project's
sanctioned idioms — `gtk_dialog_get_content_area` is used by the shared
`xap_UnixDialogHelper` itself and 33 untouched files use the same APIs;
the version floor is pinned at GDK 4.14. Migrating 2 new dialogs to
GtkColorDialog/GtkWindow while the rest of the tree stays on GtkDialog
would diverge from convention; cross-cutting cleanup belongs in its own
task.

## Analyzer findings

### gcc -fanalyzer (19 TUs)

| Finding | Sites | Verdict |
|---------|-------|---------|
| `use of NULL 'pView' where non-null expected` — fp_FieldTableFormulaRun::calculateValue dereffed `_getView()` unguarded | fp_FieldTableSumRun.cpp:1289 | **REAL — FIXED** (`!pView` early-out alongside isPieceTableChanging) |
| `argument 'this' NULL` on `+[]` captureless lambdas converted to function pointers | ut_asyncwrite.cpp:72,116; xap_UnixControlSocket.cpp:1435 | FP — known gcc13 analyzer artifact on _FUN thunks; static member functions have no `this` |
| `operator new` "possibly-NULL" | ut_asyncwrite.cpp:248; xap_UnixControlSocket.cpp:1372; ap_UnixDialog_Zotero.cpp:41 | FP — throwing new never returns NULL |
| `check of 'job' for NULL after dereferencing` | ut_asyncwrite.cpp:59 | FP — the "check" is `delete job`, legal on NULL; caller contract makes job non-null |
| `use of uninitialized value '<unknown>'` | ~9 sites incl. ut_findpattern:247,298; xap_UnixControlSocket:137,1388; fp_FieldTableSumRun:657,1046,1052,1069; ap_UnixClipPane:56; fp_Run.h:355 | FP — '<unknown>' marks analyzer-lost values through std::string/std::string::compare paths; no nameable variable to initialize |
| `_Hashtable_alloc::_M_allocate_buckets` "leak" | 2x in pd_Document.cpp | FP — analyzer loses bucket-array ownership inside unordered_set<string> / unordered_map<uint,shared_ptr<fl_AutoNum>>; standard containers |
| `<command-line>: missing terminating "` | 9 | harness artifact — a quoted -D split by word-splitting; TUs still analyzed rc=0 |

### clang --analyze (69 TUs)

| Finding | Sites | Verdict |
|---------|-------|---------|
| `deadcode.DeadStores` `posEnd` written, never read | ap_FmtInspect.cpp:198 | **REAL — FIXED** (removed dead posEnd var + its two stores) |
| `core.CallAndMessage` null object deref | fl_BlockLayout.cpp:249, fp_Run.cpp:1424, fv_View.cpp:12845, fv_View_protected.cpp:4186, pd_Document.cpp:3529 | baseline — all on pre-v4.0.2 lines; e.g. fl_BlockLayout ctor relies on the block-construction invariant (m_pSectionLayout set by containing layout); latent, not stream-introduced |
| `core.NullDereference` array access | fv_View_protected.cpp:6578 | baseline (OLD line) |
| `core.UndefinedBinaryOperatorResult` garbage operand | fv_View_protected.cpp:5903 | baseline (OLD line) |
| `core.NonNullParamChecker` NULL to nonnull param | ap_EditMethods.cpp:10064 | baseline (OLD line) |
| `cplusplus.StringChecker` param must not be null | fl_BlockLayout.cpp:10124 | baseline (OLD line) |
| `cplusplus.NewDeleteLeaks` | xap_App.cpp:277 (`setKbdLanguage(_getKbdLanguage())`) | baseline (OLD line) |
| `deadcode.DeadStores` | 101 sites | all on pre-v4.0.2 lines — baseline churn; only the one stream-new store was fixed |

## Verification

- `cd src && make -j2` — clean; recompiling the touched TUs now emits
  zero NEW warnings (remaining output is baseline deprecation/sign-
  compare noise predating the stream).
- Re-analysis confirms both real diagnostics are gone
  (fp_FieldTableSumRun pView null-arg; ap_FmtInspect dead store).
- `Abinova-test fp_FieldTableSum` — 13/13 pass (formula eval,
  `50.00` picture formatting, post-edit recalc all intact).
