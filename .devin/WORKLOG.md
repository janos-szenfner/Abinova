# Devin taskloop worklog

One line per completed task run. Format:
`- <YYYY-MM-DD> <ID>: <what changed> — <verification>`

Runs are driven by `.devin/taskloop.sh`; see `.devin/TASKS.md` for the
queue and `.devin/RUNNER_PROMPT.md` for the per-run contract.

- 2026-09-30 SETUP: task list (D01-D12 docx render, DOC01-DOC14 .doc
  import, E01-E05 epub), runner prompt, watchdog script created.
- 2026-09-30 D01: w:rPr/w:spacing no longer swallowed by the paragraph
  w:spacing handler (dispatch on pPr vs rPr parent); char-spacing prop
  consumed by fp_TextRun -> GR_ShapingInfo::m_iLetterSpacing ->
  cluster-advance widening in GR_CairoGraphics::shape (+XP path) —
  Badge.docx title/subtitle show 8pt/7pt tracking in PDF render;
  char-spacing round-trips abwn->abwn
- 2026-09-30 D02: fixed leftover ABIWORD_DATADIR/ABIWORD_ICONDIR/
  ABIWORD_SERIES variables in Makefile.am files so `make install`
  actually installs fonts/abinova-fonts.conf (plus help, artwork,
  templates, mime-info, icons) under <datadir>/abinova-4.0 — staged
  DESTDIR install verified; strace confirms the conf is parsed at
  runtime from both build tree and installed libdir; fc-match under
  the app config resolves "Calibri Light"->Carlito (Noto Sans
  systemwide); Badge.docx PDF embeds Carlito and the tracked title
  wraps cleanly
- 2026-09-30 D03: painted OOXML headEnd/tailEnd line decorations on
  bar frames (triangle/stealth/diamond/oval/arrow; sm/med/lg scale);
  gated ends to a:ln context (skips a14:hiddenLine dummies); fixed
  dead top-level shape-extent capture (context ancestor-only
  off-by-one) so vertical lines become bar-w; regenerated headline +
  feathered cover fragments — synthetic docx->pdf shows all types on
  horizontal and vertical bars; abwn round-trip preserves the props
