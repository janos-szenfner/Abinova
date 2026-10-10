## Coverage plateau — closing assessment (COVD11)

The coverage drive (TST01-TST12 baselines, COV01-COV15 gates,
COVD01-COVD10 waves) landed 16 bounded test waves and this
re-measurement is the program's closing number. It sits below
the 90% user directive; the residual ~58k uncovered lines are
the honest plateau, not unfinished test work. They group into
five classes, none of which another `.t.cpp` wave can retire
without padding tests that assert nothing:

1. **Live-session UI paths.** Edit methods, ruler drag
   geometry, frame/edit-mode transitions, clipboard and
   drag-and-drop arms that only do real work against a live
   peer or a real pointer sequence.  The xvfb-driven
   `ui-drive` legs already exercise everything a scripted
   driver can reach — the rest needs a human at a session.
   Concentration: `wp/ap/xp/ap_EditMethods.cpp` (~3.7k
   uncovered lines), `ap_TopRuler.cpp`, `fv_View*.cpp`.

2. **Modal/native dialog paths.** Print machinery, the native
   GtkPrint dialog, file-chooser previews and per-dialog user
   flows past what `dialog-smoke`/`ui-drive` cover —
   `wp/ap/gtk` residual (~5.8k) and `af/xap/gtk` (~2.4k).

3. **Format-variant arms in the legacy importers.** Twenty-
   year-old document constructs — Word6/PICF-era `.doc`
   shapes, codepage variants, RTF arms no living producer
   emits — that are only reachable through hand-synthesized
   binary fixtures; the synthesizable subset was already
   covered (mkdoccov/mkwpdcov/mkoxcov generators).
   Concentration: `ie_imp_RTF.cpp`, `ie_imp_MsWord_97.cpp`,
   `OXMLi_ListenerState_Valid.cpp` (rare settings elements).

4. **Defensive error/recovery branches.** Every hardening
   audit (MEM/UB/SEC/DOC series) added guards that fire only
   on corrupt input or OS failure states — reachable in
   principle, but each needs a bespoke corrupt fixture; the
   fuzz corpora and edge-fixture sweeps already cover the
   ones cheaply synthesizable.  This is also why branch
   coverage (~44%) trails line coverage.

5. **Redland/RDF paths.** `pd_DocumentRDF.cpp` (~60%) — the
   built-in SPARQL subset is tested, but the
   `WITH_REDLAND`-gated code is not compiled in this build
   and the remainder needs a live Redland installation.

The denominator already excludes code that cannot be counted
honestly: vendored third-party libraries, autotest sources,
generated files, and other-OS branches that never compile in
this Linux build (`G_OS_WIN32`/`__APPLE__` paths are covered
by the PORT-series syntax batteries, not runtime tests).

The ratchet in `tools/coverage-gate.sh` tracks the plateau
(`COVERAGE_MIN_PCT`, currently 72%) so future work cannot
silently regress the level the drive reached.
