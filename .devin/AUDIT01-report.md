# AUDIT01 — post-coverage hardening sweep

## Scope and method

- Leg (a): clang 18 static analyzer (`clang --analyze`, the engine
  scan-build drives) replayed over every real compile line of the tree
  — 645 translation units across `src/af`, `src/text`, `src/wp`
  (incl. all importers/exporters — the attacker-adjacent parsers got
  priority). Diagnostics landed as `*.plist`/log pairs; the plist
  litter was removed after triage.
- Leg (b): `tools/check-san.sh asan` — the whole suite under
  `-fsanitize=address,undefined` in `san-build/tree` (unit suite with
  LSan + `supp.txt`, ~250-invocation rt corpus with `detect_leaks=0`).
  DONE under AUDIT03/AUDIT04: the first attempt could not link
  `Abinova-test` (`__gcov_dump` was a strong ref — fixed, see Test
  infrastructure below). The completed run found one real first-party
  leak — `AP_UnixRibbon::createWidget()` left the `frame`/`grid`/
  `rowBox` group containers floating (never packed, never freed) for
  empty groups and left `grid` floating for row-major groups; they are
  now `ref_sink`+`unref`'d on those paths. The only remaining report
  was a single 32-byte GTK-internal `g_closure_invoke` allocation in
  libgtk's signal-dispatch machinery (pumped by `_nullUpdate` during
  load — no first-party ownership), added to `supp.txt` as documented
  library noise. The corpus leg then caught a real
  heap-buffer-overflow: `s_Abinova_1_Listener::_handleDataItems()`
  passed a raw `UT_ByteBuf` payload to `addStringUnchecked` —
  `UT_ByteBuf` is not NUL-terminated, so a base64 blob that exactly
  fills a 1024-byte chunk made `strlen` over-read (and an empty item
  passed `getPointer`'s nullptr straight into `strlen`); the payload
  is now NUL-terminated before the unchecked write.
  AUDIT03's other sanitizer fixes (frame/IM teardown
  ordering, ribbon weak-refs, RTF/PNG leaks, UBSan vptr+memmove) were
  landed and verified under RELQA01's normal-suite battery.
  Result: unit suite 13382 tests / 0 failures / 0 LSan findings,
  rt corpus 376 legs / 0 failed — zero unresolved real findings.

## Findings fixed (~54 files)

### Importers/exporters (leg (c) priority)

- `ie_imp_MsWord_97.cpp` — table row iteration dereferenced
  `cellbounds` without checking the lookup succeeded.
- `ie_imp_RTF.cpp` — `insertStrux` used `pView` before its null check
  and never checked `pRHyper`/`pBL`; `_appendHdrFtr`/`_appendField`
  missing guards; `szListId` could reach `g_ascii_strcasecmp` as null;
  `BasedOn`/`FollowedBy` fixed-size stack arrays converted to
  `std::vector` with a `-1` sentinel (a >N-style RTF overflowed them —
  a real stack smash in a file parser).
- `ie_imp_RTFObjectsAndPicts.cpp` — `LoadPictData` read/`SkipBackChar`'d
  an uninitialized `ch` when a binary payload declared `binaryLen <= 0`
  (stream desync on malformed input).
- `ie_impGraphic_GdkPixbuf.cpp` — suffix/mime arrays value-initialized
  and the match loop stops on null so `g_ascii_strcasecmp` never sees
  one.
- `ie_exp_HTML.cpp` — `PD_DocumentRange * range` leaked on the
  `_createChapter` early path.
- `ie_exp_HTML_Listener.cpp` — four `pAP` dereferences without checks.
- `ie_exp_RTF.cpp` — `strlen(nullptr)` crash paths when list-format
  lookups miss.
- `ODi_Office_Styles.h` — three getters passed possibly-null map keys.
- `ODe_Main_Listener.cpp` — missing `id` attribute left `pId` null into
  `strcmp`/the whole dispatch chain.
- `ODe_AbiDocListener.cpp` — stack POP underflow left
  `m_pCurrentImpl` null; PUSH path accepted a null impl.
- `OXMLi_ListenerState_Textbox.cpp` — unchecked document/state access.

### Layout / formatting

- `fl_BlockLayout.cpp` — `transferListFlags` dereferenced
  `getNextBlockInDocument()` and `pPrev->getAutoNum()` unchecked;
  `_doInsertTextSpan` leaked `pNewRun` on several early returns;
  `pSL` used unchecked after `insert()`; a `const char*` parameter
  reached `UT_...` calls unguarded. (Also fixed the previous attempt's
  `getAutoNum()` build break — it returns `const fl_AutoNumPtr&`.)
- `fl_DocLayout.cpp` — real precedence bug: `pMyC && A ||
  pMyC->...` dereferenced null on the second operand.
- `fl_DocListener.cpp` — `m_pLayout` null deref when `pFrame` non-null;
  potential division by zero in doc-size math.
- `fl_SectionLayout`, `fl_TOCLayout`, `fl_ContainerLayout` —
  unchecked container/strux pointers on flagged paths.
- `fp_TableContainer.cpp` — `getNthCol()`/`getNthRow()` return null
  out-of-range; four call sites crashed on edge layouts; division by
  zero on empty row data; uninitialized compound-assign; an
  unconditional `pCol` deref inside a debug message.
- `fp_Page.cpp` — three unchecked container lookups.
- `fp_Run.cpp` — `draw` dereferenced `getBlock()`/`getDocLayout()`
  unchecked during selection painting.
- `fp_TextRun.cpp` — two `text` buffer leaks on early exits.
- `fp_MathRun.cpp` / `fg_GraphicRaster.cpp` / `fg_GraphicVector.cpp` —
  `pSpanAP`/`m_pSpanAP` could still be null after the AP fetch.
- `fb_LineBreaker.cpp` — `_moveBackToFirstNonBlankData` lacked an entry
  guard.
- `fv_FrameEdit.cpp` — `isFrameAtPos` success with a failed strux
  lookup left `pBL`/`pFCon` null; two `getFrameStrings` call sites
  ignored `false` and used null `*pCloseBL`.
- `fv_View.cpp` — `pTemp` leaked when early `return`s skipped the
  clean-up tail (converted to `break`); a null container deref.
- `fv_View_protected.cpp` — `getAttribute` chains on nullable layout
  objects; garbage-value compare on an uninitialized accumulator.
- `fv_View_cmd.cpp` — `pBl1` deref without a check.
- `gr_MathTypesetter.cpp` — `attr()` returns `thread_local`
  `std::string::c_str()`; the mfenced code kept pointers across a
  second call that clobbered them.
- `gr_CairoImage.cpp` — unchecked image/buffer pointer.

### Piece table / styles

- `pt_PT_ChangeStrux/DeleteStrux/FmtMark/InsertObject.cpp`,
  `pd_Style.cpp`, `pp_Property.cpp` — `pDoc` is explicitly allowed to
  be null (documented at the call site); four
  `pDoc->getAttrProp()` sites now guard.
- `pl_ListenerCoupleCloser.cpp` — unchecked doc/layout pointer.
- `pf_Fragments.cpp` — eight early `return false` paths skipped
  `endMultiStepGlob()`; wrapped in a small file-scope RAII guard so
  every exit closes the glob; plus a null-`pf` guard.

### App framework / UI

- `xad_Document.cpp` — `strcmp` on a pair of nullable UUIDs.
- `xap_Dlg_Language.cpp` — `getNthLangName` null into the sort
  comparator/`strcmp`; uninitialized `m_ppLanguagesCode`.
- `xap_Dlg_DocComparison.cpp`, `xap_UnixDlg_FileOpenSaveAs.cpp`,
  `ut_misc.cpp` — null/unchecked-pointer fixes on flagged paths.
- `xap_Frame.h` — `nullUpdate()` guarded `m_pFrameImpl`.
- `ap_UnixStylesPane.cpp` — `refresh(m_szCurrent)`: `g_strdup`'d a
  pointer the call itself could free (self-assign UAF).
- `ap_TopRuler.cpp` — `getWidth()` tested `m_pG==null && pG==null`
  then unconditionally dereferenced `m_pG` (crashes pre-realize when
  the view has graphics but the ruler widget doesn't — now falls back
  to the view's `GR_Graphics`); `getTabToggleAreaWidth()` dereferenced
  `pG` in the non-print branch after allowing it to be null.
- `ap_EditMethods.cpp` — `fileNewUsingTemplate` could reach
  `pFrame->loadDocument` with `pFrame` null when invoked viewless;
  `s_AskForPathname` unconditionally wrote `*ieft` though its contract
  permits null; `pDoc->isDirty()` sat outside its `pDoc` guard in the
  remote-save path; annotation preview dereferenced
  `pView->getGraphics()` unchecked (incl. a debug-only deref).
- `ap_Frame.cpp` — `getNewZoom()` dereferenced `pF` after a clone-list
  loop that can leave it null (frame not yet registered).
- `ap_Toolbar_Layouts.cpp` — `insertItemBefore/After` leaked the
  caller-donated `plt` when the anchor id wasn't found (now `DELETEP`
  on the miss path).
- `abiwidget.cpp` — `AV_CHG_ALL` notification dereferenced `m_pView`
  and `getDocument()` unchecked.
- `ap_UnixDialog_RDFEditor.cpp` — `rowToStatement` passed nullable
  `row->subj/pred/obj` into `PD_URI`/`PD_Object` nonnull ctors.
- `ap_UnixDialog_Styles.cpp` — `_populateModify` kept `szCurrentStyle`
  across a second `getCurrentStyle()` call that reuses a static
  `std::string` buffer — a live use-after-reallocation of the inner
  pointer.
- `gr_RenderInfo.cpp` — static char/width/advance buffers
  value-initialized (the early-return path of
  `_stripLigaturePlaceHolders` could leave them never-written).

### Test infrastructure

- `libabinova.t.cpp`, `ap_Grammar.t.cpp`, `ut_unix.t.cpp` — the
  COVD-era `__gcov_dump()` calls were unconditional strong refs, so
  `check-san.sh`'s instrumented tree (no coverage) failed to link
  `Abinova-test` at all — the sanitizer leg could not run. The
  declarations are now weak with NULL-guarded calls: counters dump
  under `--enable-coverage`, the symbol resolves to NULL elsewhere.

## Triaged as noise (not fixed, with reason)

- `ap_TopRuler` marker/rect helpers (`_getParagraphMarkerRects`,
  `_getTabStopRect`, `_getColumnMarkerRect`, `_displayStatusMessage`):
  `m_pG` is created on widget realize and every path to these runs
  through realized-widget event handlers.
- `ap_LeftRuler.cpp:780` — `pTInfo->m_pCell` is only ever assigned
  under `if(pCur)` in `fv_View` (guaranteed non-null); the same object
  was already dereferenced two statements earlier.
- `ap_Dialog_Lists.cpp:1013` — `m_isListAtPoint` is only true when
  `pBL` is non-null (assigned from the same expression).
- `ap_EditMethods.cpp:9019` — `strcmp(pLang, s)`: `s` is non-null
  whenever `props_out` is non-empty (it pushed `s`).
- `OXMLi_ListenerState_Styles.cpp:147` — `val` is checked by the
  `_error_if_fail(...)` + `UT_return_if_fail` pair immediately above.
- `ie_exp_OpenXML_Listener.cpp:1436/1495` — `document` only exists
  between `openDocument`/`closeDocument` (lifecycle-guarded).
- `pd_Document.cpp:2938` — `isFootnote(nullptr)` returns false, so the
  guarded block never runs with `pf` null.
- `pf_Fragments.cpp:620` — `pn->item` at that point is always an
  internal node; the null `item` is only on the `m_pLeaf` sentinel,
  which can never be a parent.
- `xap_App.cpp:317` — `_getKbdLanguage()` is `virtual` and the only
  implementation returns nullptr/static storage; the "leak" needs an
  allocating override that doesn't exist.
- `ut_crc32.cpp:156` — flagged "garbage" is the *caller's* buffer
  contents; the function fills `q[0..n)` before reading.
- `ut_stringbuf.{h,cpp}` — `copy()`/`grow()` already null-guard the
  flagged internals.
- `predefined_ops.h` — inside libstdc++; symptom of the caller-uninit
  class above.
- All `deadcode.DeadStores` — dead writes in legacy AbiWord idiom, not
  defects; mass-rewriting them adds churn without fixing behavior.

## Verification

- `cd src && make -j2` — clean build, `abinova` links.
- `tools/check-san.sh asan` — DONE under AUDIT04 (AUDIT03 capped at
  restarts before the instrumented re-verify finished): unit suite
  `testwrap.sh` 13382 tests / 0 failures / 0 LSan reports under
  `-fsanitize=address,undefined`, corpus `rtwrap.sh` 376 legs / 0
  failed.
- Headless converts (AUDIT02 audit-verify): Badge.docx and
  Word97Test.doc -> PDF render correctly.
- `make check` — full suite green under RELQA01/RELQA05 (testwrap
  13382/0, rtwrap 382/382, portwrap/dlgswrap/drvwrap legs PASS or
  documented-xfail), recorded in `.devin/RELEASE-CHECKLIST.md`.
