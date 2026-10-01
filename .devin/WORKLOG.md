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
- 2026-10-01 D10: a:blipFill tiling — a:tile (tx/ty/sx/sy/flip/algn) and a:fillRect imported as image-tile/image-fill-rect props; new cairo blipFill painter in fp_FrameContainer tiles via a 2x flip supercell pattern, stretches into fillRect, applies srcRect once — synthetic docx PDF shows motif grid at correct scale/offset/mirroring; feathered/Crop/banded regression clean
- 2026-10-01 D11: abwn round-trip audit — registered missing a:ln props (line-align/cap/compound/custom-dash/dash/join/miter-limit, outline-gradient) in pp_Property.cpp; documented line-*/outline-gradient (new §4.4), image-tile/-fill-rect in ABWN-FORMAT.md + abwn.dtd; docx->abwn->abwn prop diff on 17-doc corpus + 9 synthetic feature docs: zero prop loss; shadow round-trip PDF renders correctly
- 2026-10-01 D12: corpus regression + cover sweep — all 17 cover docx -> pdf clean; regenerated covers/*.xml from fresh abwn exports (stale ones predated char-spacing/theme refs and recompressed integral's JPEG); wrapped-fragment renders match docx renders (max ~3% AA-level diff); finished cover-insert rework in fv_View_cmd.cpp (document-level paste probe + shell/sentinel build, frame-edit-mode safe; COVERDBG prints stripped) — spliced badge cover renders page-1 cover + page-2 body; header/footer-only docs still blank in PDF (pre-existing)
- 2026-10-01 DOC01: .doc piece-table audit — confirmed fcCompressed/utf-16 handling per MS-DOC 2.4.1; fixed OOB reads in wvGetCLX (cb/lcb clamps), wvConvertCPToFC (empty piece table), wvGetPCD_PLCF/wvGetBTE_PLCF* (len<4 underflow), wvGetBTE_FromFC (empty bin table deref), grpprl/cpiece index guards in decode_complex, deterministic '?' on failed codepage iconv — synthetic mixed compressed/UTF-16/CP1251 docs import in order via both decode paths; corpus regression clean
