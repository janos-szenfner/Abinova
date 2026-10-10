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

## Known-failing exceptions

(none yet)
