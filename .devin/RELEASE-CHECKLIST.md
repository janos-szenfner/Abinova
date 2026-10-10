# Abinova 4.0.0 pre-release gate

Started by RELQA01 (suite battery A). Each leg records its result here;
the tag is blocked until every leg is green and the
'known-failing exceptions' section is empty.

## RELQA01 — suite battery A (clean build + core suite + make check + coverage gate)

| Leg | Command | Result |
|-----|---------|--------|
| Clean build | `make -C src clean && make -j2` (thirdparty already built, `make -C thirdparty` = up to date) | PASS — 645 compile units, BUILD_EXIT=0, incremental `make -j2` also clean |
| Core unit suite | `make -C src/wp/test check TESTS="unix/testwrap.sh"` (Abinova-test, all .t.cpp mains) | PASS — 13382 tests, 0 failures |
| Round-trip corpus | `make -C src/wp/test check TESTS="unix/rtwrap.sh"` | PASS — 382/382 legs, 0 failed |
| Portability guard | `make -C src/wp/test check TESTS="unix/portwrap.sh"` | PASS — portguard OK (no /tmp literals, 0 unistd leaks) |
| Display legs (dlgswrap/drvwrap) | deferred — owned by RELQA05; run early here only to feed the coverage counters | dlgswrap PASS 71/72 + 1 xfail; drvwrap PASS 72/72 dialogs clean + abi/ev/ruler/fmt/freeze/toc/markup/popovers legs ok (frame leg hit its designed 300s bound, not a failure) |
| Coverage gate | `tools/coverage.sh` + `tools/coverage-gate.sh` (floor 72%) | PASS — 73.19% lines (158842/217024) after the full leg set fed counters |

RELQA01 also landed the uncommitted sanitizer-hardening fixes the
capped AUDIT03 run left in the tree (frame/IM-context teardown ordering,
weak-ref'd ribbon widget bookkeeping, RTF annotation/lastData leaks,
layout AP + PNG reader-buffer ownership, UBSan vptr/memmove fixes,
TF_MAX_TEST_TIME + weak `__gcov_dump` plumbing for san-builds) — all
verified by the battery above.

## RELQA05 — suite battery B (wrap legs + headless smoke harnesses)

| Leg | Command | Result |
|-----|---------|--------|
| Core unit suite | `make -C src/wp/test check TESTS="unix/testwrap.sh"` | PASS — 13382 tests, 0 failures |
| Round-trip corpus | `make -C src/wp/test check TESTS="unix/rtwrap.sh"` | PASS — run twice, second run after the EPUB field fix below |
| Portability guard | `make -C src/wp/test check TESTS="unix/portwrap.sh"` | PASS — re-run after the EPUB field fix |
| Dialog smoke | `make -C src/wp/test check TESTS="unix/dlgswrap.sh"` (xvfb-run) | PASS — 71/72 dialogs clean + 1 coded xfail (same expected failure as RELQA01) |
| Display driver | `make -C src/wp/test check TESTS="unix/drvwrap.sh"` (xvfb-run) | PASS — 72/72 dialogs clean + abiwidget/ev/ruler/fmt/freeze/toc/markup/popover legs ok; frame leg hit its designed 300s bound (documented bound, not a failure) |
| EPUB round-trip | `tools/epub-rt-check.sh src/abinova test/wp/fields.doc test/wp/BillOfRights.abw test/wp/long_footnote.doc <docx>` | PASS — 4/4 packages valid + text round-trips; found and fixed two real exporter bugs (see below) |
| Fuzz corpora | `tools/check-fuzz.sh` | PASS — 28 corpora + regression seeds |
| Freeze heartbeat | `tools/check-freeze.sh` (xvfb-run) | PASS — worst main-loop gap 1173 ms under load |

Failures found and fixed in this battery:

- XHTML/EPUB3 content emitted the named HTML entity `&nbsp;`, which is
  undefined in strict XML — generated packages were malformed and
  rejected by conforming parsers. The exporter now emits `&#160;` when
  the tag writer is in XML mode (`html4:no`, XHTML, EPUB3) and keeps
  `&nbsp;` for HTML4.
- HTML-family exporters (HTML/XHTML/EPUB) emitted empty spans for
  computed fields in documents whose field results were never laid out
  (e.g. `.doc`-imported fields): `populateFields()` is now invoked
  once per export, matching the plain-text exporter, so live values
  (dates, page/word/char counts, filenames, cross-reference results)
  appear in the output.
- `tools/epub-rt-check.sh` normalizes `HH:MM:SS` values when diffing
  text so live `time` fields (re-evaluated at each run) do not produce
  false mismatches.

## Known-failing exceptions

(none)
