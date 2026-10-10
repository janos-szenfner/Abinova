# TRACK04 — track-changes surface audit

Every user-visible entry point of the track-changes feature, where it
lives, and how this audit drives it.  "Model" cells are driven
headless in `src/text/fmt/xp/t/fv_Revisions.t.cpp` through the real
edit methods (they all survive `CHECK_FRAME` headless — that macro is
a loading-lock, not a frame requirement) or the view/doc commands they
delegate to; "UI" cells need a real frame and are driven by the
ui-drive `--markup`/`--revisions` legs on the live display.

Real-marks fixtures: (a) in-app tracked edits seeded via
`setMarkRevisions(true)` + `cmdCharInsert`/`cmdCharDelete` in the test
harness — real `revision=` attrs in the piece table, and (b) the
committed `test/wp/tst04/o06_revisions.docx` fixture (real
`w:ins`/`w:del`/`moveFrom`/`moveTo`/paragraph-mark-deletion marks from
the OOXML importer).

## TRACKING

| Entry point | Path | Audit cell | Result |
|---|---|---|---|
| Track Changes toggle ON | ribbon `track` check row -> `toggleMarkRevisions` -> MarkRevisions dlg | ui-drive `--revisions` (dialog auto-answered, `isMarkRevisions` + level 0 asserted) | covered |
| Track Changes toggle OFF | same row, `pView->toggleMarkRevisions()` | ui-drive + model (`setMarkRevisions(false)`) | covered |
| Auto Revision toggle | ribbon `revauto` check row -> `toggleAutoRevision` (warns on OFF) | ui-drive `--revisions` (check state + flag) | covered |
| revisionNew | edit method only (no ribbon row; legacy binding) | code-read: opens MarkRevisions dlg `forceNew`, sets mark on | noted, unwired by design |
| Purge revisions | `AD_Document::purgeAllRevisions` — strings existed, no menu id/action/edit method | **wired** in this task: `AP_MENU_ID_TOOLS_REVISIONS_PURGE` + `revisionPurge` edit method + Track Changes popover row; ui-drive drives the confirm | fixed (was dead) |
| purgeHistory ("Purge History") | strings only, same dead pattern | left dead — separate History feature, not the revisions surface; filed | follow-up |

## DISPLAY

| Entry point | Path | Audit cell | Result |
|---|---|---|---|
| 4-mode selector | markup popover `mode:*` rows -> `revisionDisplayMode` | `fv_StateCycle` (flags+visibleText under both mark states) + ui-drive `--markup` (rows, checks, caption, sensitivity) | covered (TRACK03) |
| Show Revisions row | `toggleShowRevisions` | model: flag flip + visibleText both ways | covered |
| Show Before/After/AfterPrevious | `toggleShowRevisionsBefore/After/AfterPrevious` — no ribbon rows | model: flag combos + level transitions + visibleText (Before pairs `setRevisionLevel(0)` with `toggleShowRevisions`, so the rebuild still lands once on the final combo — verified, no bug) | covered |
| Compare Revisions… | `revisionSetViewLevel` -> ListRevisions dlg | ui-drive `--revisions` (dialog auto-answered, level applied) | covered |
| Display caption + checks | `_markupModeName`/`_evalCheckKind` in `AP_UnixRibbon` | ui-drive `--markup` asserts caption + tick per mode | covered (TRACK03) |
| Multi-view sync | `updateRevisionMode` on `PD_SIGNAL_REVISION_MODE_CHANGED` | model: auto-revision sync path asserted; per-view independence under manual mode noted as design | covered |
| Reviewing pane | ribbon `PANE` row -> `commentsPane` -> `XAP_FrameImpl::toggleCommentsPane` | ui-drive `--revisions` (frame-dependent) | covered |

## CHANGES

| Entry point | Path | Audit cell | Result |
|---|---|---|---|
| Find next/prev | ribbon buttons + context menu -> `cmdFindRevision` | model: selection lands on each revised run across block boundaries, in order; **fixed**: the block-advance loop never reset `pRun` after `getNextBlockInDocument`, so the search silently only scanned the caret's own block — plus no wrap; rewritten to restart the run chain per block (last run for prev) and wrap once at the document boundary | fixed + covered |
| Accept/reject at caret | popover rows + context menu -> `cmdAcceptRejectRevision` -> `PD_Document::acceptRejectRevision` | model: insertion accepted -> stays + unmarked; deletion accepted -> gone; reject inverts; revision count drops | covered |
| Accept/Reject and Move to Next | `revisionAcceptNext`/`revisionRejectNext` | model via real edit methods | covered |
| Accept/Reject All | `revisionAcceptAll`/`revisionRejectAll` -> doc-level iterators | model: text matches hand-accepted result, rev table cleared | covered |
| Accept/Reject All Shown | `revisionAcceptAllShown`/`revisionRejectAllShown` -> `getRevisionOpsLevel` | model: Original level 0 under tracking resolves to ops level and acts; hidden runs still processed | covered (TRACK03 fix) |
| …and Stop Tracking | `revisionAcceptAllStopTracking`/`revisionRejectAllStopTracking` | model: marks cleared + `isMarkRevisions` false | covered |
| Revision context menu | `contextRevision` -> `EV_EMC_REVISION` layout (accept/reject/find+cut/copy/paste) | code-read + ui-drive context-menu smoke (`getMouseContext` returns REVISION over marked runs — asserted in model cell) | covered |
| Compare/Combine Documents | `revisionCompareDocuments`/`revisionCombineDocuments` -> ListDocuments dlg | code-read: merged doc emitted as tracked ins/del into a new frame / appended block; dlg-bound, exercised by generic dialog smoke | covered by dlg smoke |

## INTERACTIONS

| Cell | Audit | Result |
|---|---|---|
| undo/redo of accept/reject | model: undo restores revision attr + text, redo re-applies | covered |
| undo after display-mode change | model: mode flips are view state — undo is a no-op on the doc | covered |
| edit inside/at edges of a marked insertion | model: type at start/middle/end of a marked run + spanning delete | covered |
| delete spanning marked + unmarked text | model: tracked delete over mixed selection keeps marks coherent | covered |
| paste over revised text | model-level equivalent (delete+insert across marks); real clipboard path is ui-drive territory | covered |
| strux marks (cellIns/cellDel/pPrChange) | docx fixture carries a deleted paragraph mark (`<w:del>` in `pPr`) — display modes asserted on it | covered |
| annotations in revised runs | model: annotation anchored inside a marked insertion survives all four modes | covered |
| fields under hidden revisions | `fv_StateCycle` asserts field count is display-invariant | covered (TRACK03) |
| insertion-point cleanup | `_fixInsertionPointAfterRevision`: mark off with caret inside a marked run -> next typed char unmarked | covered |

## PERSISTENCE

| Cell | Audit | Result |
|---|---|---|
| abwn round-trip: marks + history + show/mark state | model: `writeToFile` -> `readFromFile`, assert `getHighestRevisionId`, `isMarkRevisions`, `isShowRevisions`, `getShowRevisionId`, history rows | covered |
| docx w:ins/w:del import | o06 fixture load — ins/del/moveFrom/moveTo/para-mark-del land as real revision attrs | covered |
| docx export round-trip | OXML08 landed the exporter; o06->abwn->docx re-export leg in ui-drive | covered |
| RTF `\revtbl`/`\revised`/`\deleted` | REV01 landed author/date; exercised by rt corpus | covered (prior) |
| ODF tracked changes | ODF02/ODF03 landed | covered (prior) |

## INVARIANTS asserted in every cell

- display/display-level ops never change `text()` (the piece-table
  content) nor `getRevisions().size()` — hidden is never deleted;
- accept/reject ops only ever reduce `getHighestRevisionId`, never
  resurrect data;
- `markupStateIs` flag combos land on the intended state after every
  transition (the TRACK01 wedge class).

## Findings fixed in this task

1. **Dead purge entry** — `MENU_LABEL/STATUSLINE_TOOLS_REVISIONS_PURGE`
   strings were orphaned: no `AP_MENU_ID`, no action-set row, no edit
   method, no callers of `AD_Document::purgeAllRevisions`.  Wired:
   `AP_MENU_ID_TOOLS_REVISIONS_PURGE`, `revisionPurge` edit method,
   action-set row gated on `ap_GetState_HasRevisions`, and a "Purge
   Revisions…" row in the Track Changes popover.  The doc call itself
   already prompts before destroying history.
2. **`cmdFindRevision` could not leave the starting block** — after
   the run chain went null at the end of the caret's block,
   `pBL` advanced via `getNextBlockInDocument()` but `pRun` was
   never re-pointed at the new block's runs, so `while(pRun)`
   skipped every subsequent block: Find Next only ever found a
   revision inside the paragraph the caret sat in, and the
   section loop was dead code (`getNextBlockInDocument` already
   crosses sections).  It also never wrapped.  Rewritten: per-block
   run-chain restart (last run for prev), wrap once at the document
   boundary, caret restored on total failure.
3. (checked, no bug) `toggleShowRevisionsBefore` pairs
   `setRevisionLevel(0)` with `toggleShowRevisions()` — the latter
   still performs the layout rebuild, so the pair lands on a
   consistent rendered state; `setRevisionLevel` also persists the
   doc show-level and notifies chrome.  Left as-is.

## Follow-ups filed (out of scope here)

- `MENU_LABEL/STATUSLINE_TOOLS_HISTORY_PURGE` are orphaned the same
  way — belongs to the History surface, not Revisions.
- `revisionNew`/`revisionSelect`/the legacy SHOW_BEFORE/AFTER menu ids
  remain edit-method-only (no ribbon rows); the Before/After level
  commands are subsumed by the 4-mode selector.
