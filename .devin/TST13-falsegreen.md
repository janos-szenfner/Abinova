# TST13 — why the TRACK03-class bug shipped past every test layer

The user's complaint: Simple Markup -> All Markup did not restore the
tracked insertions/deletions on screen, and Original did nothing
visible — yet `fv_StateCycle` and ui-drive `--markup` both reported
green.  This is the layer-by-layer post-mortem.

## The bug being missed (TRACK03, commit 729fe44)

With `isMarkRevisions` ON the view revision level is a boundary model:

- level `0` = "reveal all" — every marked change rendered inline;
- level `PD_MAX_REVISION` = every revision silently accepted — clean
  final text, i.e. what *No Markup* shows.

`revisionDisplayMode` mapped `all -> PD_MAX_REVISION` unconditionally,
so picking *All Markup* while tracking rendered exactly the No Markup
text.  Separately, `explodeRevisions`' Original branch
(`!bShow && iId==0`) required `!bMark`, so *Original* under tracking
rendered the union of deleted and inserted text instead of the
pre-edit document.

## Layer 1 — `fv_StateCycle` (pre-TRACK03 main)

The `markup modes cycle` main asserted, per transition:

- flag triple `isShowRevisions / isShowRevBars / getRevisionLevel`,
- `fieldCount()`, `text()` — i.e. `getTextInDocument`, the piece-table
  text, **display-mode invariant by definition**,
- `getHighestRevisionId()` — data preservation.

Nothing ever inspected the rendered/visible text.  `text()` answers
"what is in the document", not "what is on screen" — a stale or
wrongly-filtered layout passes every assert.

Its fixture also made the bug *unreachable*: it seeded a single
tracked **insertion** with `setMarkRevisions(true)` and turned marking
**off** before cycling.  The TRACK03 divergence lives in the
marking-ON level encoding — with marking off, `all -> PD_MAX` is the
*correct* encoding, so even a rendered-text assert on that fixture
state cannot see the bug.

## Layer 2 — ui-drive `--markup`

Same shape, one level up: `BillOfRights.abw` + one seeded tracked
insertion (`setMarkRevisions(true)` -> `cmdCharInsert` ->
`setMarkRevisions(false)` — marking OFF for the whole cycle).  Per row
click it asserted sensitivity, the flag triple, caption text, check
marks and revision-data preservation.  No rendered content anywhere.

Two independent reasons it could not catch the bug:

1. **Marking OFF.**  The fixture state never enters the marking-ON
   level encoding where 'all' diverges.  Under marking OFF the buggy
   mapping is semantically correct — flags AND render agree.
2. **Same-getter oracle.**  `_markupModeName()` and the `mode:*`
   check rows (`ap_UnixRibbon.cpp:_markupModeName/_evalCheckKind`)
   derive their answer from `isShowRevisions() / isShowRevBars() /
   getRevisionLevel()` — the exact getters the leg asserts.  In the
   buggy state (sR=true, bars=false, level=PD_MAX under tracking) the
   ribbon caption reads "All Markup" and the 'all' row ticks, because
   the indicator *believes the same lie the flags tell*.  A
   flag-vs-render divergence is invisible when both sides read the
   same derived state; only the rendered text/run census is an
   independent oracle.

## Layer 3 — `getRevisionLevel` derivation

`getRevisionLevel()` is itself derived: the `isMarkRevisions()` clamp
(fv_View.cpp) only engages with marking ON, so the suites' marking-OFF
state never exercised it — and the TRACK03-era fold-to-0 quirk
(tracked-but-clean doc mislabelled every mode "Original") likewise
lived on a path the fixtures never entered.

## What was hardened (leg 2)

- `fv_StateCycle`: full 4x4 ordered transition matrix (incl.
  self-transitions) x `isMarkRevisions{on,off}` on in-app seeded
  ins+del marks, asserting rendered `visibleText()` + flags + level +
  data preservation after EVERY transition; seeded random-order
  multi-cycle; a pinned-bug-shape negative control proving the content
  oracle distinguishes the pre-fix 'all' state from real All Markup.
- `fv_Revisions`: the same matrix driven over *imported* marks
  (`o06_revisions.docx` w:ins/w:del/moveFrom/moveTo) under both
  marking states; a pixel-level assert that Simple mode paints the
  left-margin revision bar while other modes do not.
- ui-drive `--markup`: fixture now seeds a tracked deletion AND
  insertion with unique sentinels; per-row-click asserts rendered
  visible text, not just flags/caption/ticks; a second pass drives the
  same transitions through the bare edit method — any widget-vs-method
  divergence fails loudly; a marking-ON pass exercises the TRACK03
  encoding directly.
- ui-drive `--revisions`: seeds a marked deletion alongside the
  insertions and asserts rendered final text after purge/accept so the
  destructive paths prove content, not just counts.
- Negative control: the new mains were re-run against a scratch build
  with the TRACK03 hunks reverted — they fail on exactly the
  flag-invisible content divergences described above.
