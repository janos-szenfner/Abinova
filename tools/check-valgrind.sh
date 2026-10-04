#!/bin/sh
# check-valgrind.sh — bounded corpus of headless converts under
# valgrind, gating the DOC15/DOC20/UB-series hardening.
#
# Wired to `make check-valgrind` (top level).  Each fixture below is
# converted to PDF by the NORMAL (uninstrumented) build — one
# src/.libs/abinova process per file — under:
#
#   valgrind --leak-check=full --errors-for-leak-kinds=definite
#            --error-exitcode=99 --num-callers=20
#            --suppressions=tools/valgrind.supp
#
# so any memcheck error OR any definite leak fails the leg (the same
# gating DOC20 used; 'still reachable' library caches are not errors,
# and the suppression file only covers leaks OWNED by third-party
# library code — gio module singletons and fontconfig/cairo internals).
# .libs/abinova is run directly because the libtool wrapper would make
# valgrind trace the wrapper's /bin/sh instead of the binary.
#
# Bounded by design: a representative importer per format rather than
# the full rt-check matrix — valgrind is ~10-20x slower than native,
# and the task-loop RSS watchdog favors many small runs.  Skips
# cleanly (exit 0) when valgrind or the built binary is absent so the
# target is safe on hosts without the tooling.
#
# Env:
#   VALGRIND         valgrind binary (default valgrind)
#   SAN_VG_TIMEOUT   per-file timeout in seconds (default 600)
#   SAN_VG_LOGDIR    per-file log dir (default <repo>/san-build/valgrind-logs)
#   SAN_VG_EXTRA     extra files to check (space-separated, repo-relative)

set -u

SRCROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
BIN=$SRCROOT/src/.libs/abinova
LOGDIR=${SAN_VG_LOGDIR:-$SRCROOT/san-build/valgrind-logs}
TIMEOUT=${SAN_VG_TIMEOUT:-600}

if ! command -v "${VALGRIND:-valgrind}" >/dev/null 2>&1; then
    echo "check-valgrind: SKIP (valgrind not installed)"
    exit 0
fi
if [ ! -x "$BIN" ]; then
    echo "check-valgrind: SKIP (no built $BIN — run make first)"
    exit 0
fi

# One representative importer path per format, preferring fixtures
# that exercise the hardened code (wv piece table, OXML revisions/
# altChunk/fallback, ODF tracked changes, WP headers/images, MHTML
# multipart, markdown/latex math).
CORPUS="
test/wp/BillOfRights.abw
test/wp/table.abw
test/wp/Word97Test.doc
test/wp/long_footnote.doc
test/wp/odt/trackedchanges/document.odt
test/wp/tst04/o01_objects.odt
test/wp/tst04/o02_altchunk.docx
test/wp/tst04/o06_revisions.docx
test/wp/tst04/wpd04.wpd
test/wp/tst04/math.tex
test/wp/markdown-formatting.md
fuzz/corpus/rtf/seed_rtftest.rtf
fuzz/corpus/docx/seed_gettysburg.docx
fuzz/corpus/wpd/seed_minimal.wpd
fuzz/corpus/mht/seed_gettysburg.mht
test/auto/hello.dat.2.txt
${SAN_VG_EXTRA:-}
"

mkdir -p "$LOGDIR"
TMPD=$(mktemp -d)
trap 'rm -rf "$TMPD"' EXIT

n_pass=0; n_fail=0; n_skip=0
fails=
for f in $CORPUS; do
    [ -n "$f" ] || continue
    if [ ! -f "$SRCROOT/$f" ]; then
        echo "check-valgrind: SKIP $f (missing fixture)"
        n_skip=$((n_skip + 1))
        continue
    fi
    name=$(basename "$f")
    log="$LOGDIR/$name.log"
    if timeout "$TIMEOUT" \
        env G_DEBUG=gc-friendly G_SLICE=always-malloc \
            LD_LIBRARY_PATH="$SRCROOT/src/.libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        "${VALGRIND:-valgrind}" \
            --leak-check=full --errors-for-leak-kinds=definite \
            --error-exitcode=99 --num-callers=20 \
            --suppressions="$SRCROOT/tools/valgrind.supp" \
            "$BIN" --to=pdf --to-name="$TMPD/out.pdf" "$SRCROOT/$f" \
            > "$log" 2>&1; then
        echo "check-valgrind: PASS $f"
        n_pass=$((n_pass + 1))
    else
        rc=$?
        reason="rc=$rc"
        grep -m1 -E "ERROR SUMMARY: [1-9]|definitely lost: [1-9]|Invalid (read|write)" "$log" \
            | head -1 | grep -q . && reason="$reason $(grep -m1 -E 'ERROR SUMMARY|Invalid (read|write)|definitely lost' "$log")"
        echo "check-valgrind: FAIL $f ($reason) — log $log"
        n_fail=$((n_fail + 1))
        fails="$fails $f"
    fi
done

echo "check-valgrind: $n_pass passed, $n_fail failed, $n_skip skipped"
[ "$n_fail" -eq 0 ]
