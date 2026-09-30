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
- 2026-09-30 D04: a:outerShdw drop shadows now paint —
  s_paintFrameShadow in fp_FrameContainer::draw (under the frame
  transform, before the fill) blurs a rasterized silhouette of the
  shape (custGeom path or box) and masks it at dist/dir in the
  shadow color; importer captures the color child, a:alpha and
  rotWithShape into new frame-shadow-alpha/-rot props — synthetic
  docx->pdf shows blurred/colored/offset shadows on rect and
  custGeom diamond, abwn round-trip preserves all props, banded.docx
  regression-free
- 2026-09-30 D05: wps:style theme refs resolve — lnRef->lnStyleLst outline default, effectRef->effectStyleLst shadow, fontRef->default run font/color; theme parses effectStyleLst+lnStyleLst — verified on synthetic Austin.docx (theme shadow+outline render, explicit effectLst suppresses, fontRef recolors), abwn round-trip OK, corpus regression clean
- 2026-10-01 D06: a:blip effects imported+rendered — a:duotone/a:grayscl/a:lum/a:alphaModFix captured in Image listener, stored as image-* props, pixel-applied in GR_UnixImage::applyBlipEffects — filgree.pdf flowers tint (160,172,193) matching duotone lerp; synthetic docx verifies lum+alphaModFix; Austin regression clean
- 2026-10-01 D07: a:normAutofit imported+rendered — fontScale/lnSpcReduction -> frame-font-scale/frame-linesp-reduction; fp_Run::lookupProperties scales resolved font-size via cloned span AP, fp_Line::recalcHeight shrinks line boxes — synthetic docx PDF shows 24.4pt->12.2pt glyphs, 28pt->11pt pitch; abwn round-trip identical; corpus regression clean
- 2026-10-01 D08: wps:bodyPr@vert rendered — frame-text-direction now rotates the text stack 90deg (vert/eaVert/mongolianVert/wordArtVert*) or 270deg (vert270); fp_FrameContainer swaps content getWidth/getHeight so lines wrap in the logical space, draw() rotates children via cairo translate/rotate around the inner-box origin, clip rect inverse-rotated to land on the physical box, spAutoFit grows width, mapXYToPosition un-rotates hit tests — synthetic docx PDF shows vert270 bottom-to-top L->R and vert top-to-bottom R->L boxes; abwn round-trip keeps all 4 values; Austin/Badge/filgree/retrospect regression clean
- 2026-10-01 D09: a:ln completeness — @cmpd strands, round/bevel/miter joins, @cap, custDash, a:ln gradFill and algn=in painted via new cairo outline path on borders, custGeom shapes and line bars; fp_FrameContainer::_drawLine gains double/triple/wave — synthetic docx renders double-line compound borders; feathered/Austin/retrospect/slice-dark regression clean
