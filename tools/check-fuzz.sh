#!/bin/sh
# check-fuzz.sh — smoke-run the libFuzzer targets over their seed and
# regression corpora.
#
# Wired to `make check-fuzz` (top level).  This is a NON-FAILING
# exercise: it replays inputs and reports what it saw but exits 0 so a
# missing instrumented build or a still-open known bug never gates the
# normal build.  Set FUZZ_CHECK_STRICT=1 to make any crash or
# reproduction exit non-zero.
#
# Layout this script drives:
#   fuzz/fuzz_<fmt>.cpp     libFuzzer harness, built by tools/build-fuzz.sh
#   fuzz/corpus/<fmt>/      seed inputs — minimal valid + edge cases
#   fuzz/regress/<fmt>/     crash reproducers pinned by
#                           tools/fuzz-add-reproducer.sh — replayed
#                           every run so a fixed bug stays fixed
#
# Each target runs twice:
#   1. seed corpus:    one -runs=0 pass over fuzz/corpus/<fmt>
#   2. regress corpus: each file in fuzz/regress/<fmt> replayed
#                      individually (-runs=1) so one still-crashing
#                      input can't mask the rest
#
# A corpus crash is a NEW bug; a regress crash means the pinned bug is
# still open — both print a SUMMARY line, only strict mode fails.
#
# Env:
#   FUZZ_BUILDDIR        instrumented build root (default fuzz-build/)
#   FUZZ_TARGETS         target subset, e.g. "fuzz_doc fuzz_rtf"
#                        (default: every fuzz/fuzz_*.cpp with a binary)
#   FUZZ_CHECK_RSS_MB    per-run RSS cap (default 1200 — stays under the
#                        task-loop 2048MB watchdog)
#   FUZZ_CHECK_TIMEOUT   per-input hang timeout in seconds (default 15)
#   FUZZ_CHECK_STRICT=1  exit non-zero when anything crashes
#   FUZZ_CHECK_RUNS=N    mutation runs after corpus replay (default 0 —
#                        pure replay; bump for a bounded fuzz pass)

set -u

SRCROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
BUILDDIR=${FUZZ_BUILDDIR:-$SRCROOT/fuzz-build}
CORPUS=$SRCROOT/fuzz/corpus
REGRESS=$SRCROOT/fuzz/regress
ARTIFACTS=$BUILDDIR/artifacts
RSS=${FUZZ_CHECK_RSS_MB:-1200}
TIMEOUT=${FUZZ_CHECK_TIMEOUT:-15}
RUNS=${FUZZ_CHECK_RUNS:-0}
STRICT=${FUZZ_CHECK_STRICT:-0}

export ASAN_OPTIONS=detect_leaks=0

mkdir -p "$ARTIFACTS"

if [ -n "${FUZZ_TARGETS:-}" ]; then
    targets=$FUZZ_TARGETS
else
    targets=
    for src in "$SRCROOT"/fuzz/fuzz_*.cpp; do
        [ -e "$src" ] && targets="$targets $(basename "$src" .cpp)"
    done
fi

if [ -z "$targets" ]; then
    echo "check-fuzz: no fuzz/fuzz_*.cpp harnesses found"
    exit 0
fi

run_one() # target dir-or-file runs -> rc, echoes nothing
{
    "$BUILDDIR/$1" $2 -runs="$RUNS" \
        -rss_limit_mb="$RSS" -timeout="$TIMEOUT" \
        -artifact_prefix="$ARTIFACTS/" > "$ARTIFACTS/.last.$1.log" 2>&1
}

crash_summary() # logfile -> one-line reason
{
    grep -E "^(SUMMARY|ERROR): " "$1" 2>/dev/null | head -1
}

n_run=0; n_pass=0; n_skip=0
crashes=
reproduced=

for t in $targets; do
    fmt=${t#fuzz_}
    if [ ! -x "$BUILDDIR/$t" ]; then
        echo "check-fuzz: SKIP $t (not built — run tools/build-fuzz.sh)"
        n_skip=$((n_skip + 1))
        continue
    fi

    # 1. seed corpus replay (+ optional short bounded fuzz pass)
    cdir=$CORPUS/$fmt
    if [ -d "$cdir" ]; then
        n_seeds=$(find "$cdir" -type f | wc -l)
        if [ "$RUNS" -gt 0 ]; then
            # mutating run — new units land in the FIRST dir given, so
            # keep the committed seed corpus read-only behind a scratch
            mkdir -p "$BUILDDIR/corpus-$fmt"
            run_one "$t" "$BUILDDIR/corpus-$fmt $cdir"
        else
            run_one "$t" "$cdir"
        fi
        rc=$?
        n_run=$((n_run + 1))
        if [ "$rc" -eq 0 ]; then
            echo "check-fuzz: PASS $t corpus ($n_seeds seeds)"
            n_pass=$((n_pass + 1))
        else
            echo "check-fuzz: CRASH $t corpus ($n_seeds seeds): $(crash_summary "$ARTIFACTS/.last.$t.log")"
            echo "check-fuzz:   log $ARTIFACTS/.last.$t.log — pin it with tools/fuzz-add-reproducer.sh $fmt <artifact>"
            crashes="$crashes $t(corpus)"
        fi
    else
        echo "check-fuzz: SKIP $t corpus (no $cdir)"
    fi

    # 2. regress replay, one file at a time
    rdir=$REGRESS/$fmt
    [ -d "$rdir" ] || continue
    for f in "$rdir"/*; do
        [ -f "$f" ] || continue
        n_run=$((n_run + 1))
        run_one "$t" "$f"
        rc=$?
        if [ "$rc" -eq 0 ]; then
            echo "check-fuzz: PASS $t regress/$(basename "$f")"
            n_pass=$((n_pass + 1))
        else
            echo "check-fuzz: REPRODUCED $t regress/$(basename "$f"): $(crash_summary "$ARTIFACTS/.last.$t.log")"
            reproduced="$reproduced $t:$(basename "$f")"
        fi
    done
done

echo "check-fuzz: $n_pass/$n_run runs clean, $n_skip targets skipped"
[ -n "$crashes" ] && echo "check-fuzz: corpus crashes:$crashes"
[ -n "$reproduced" ] && echo "check-fuzz: regress reproductions (known bugs still open):$reproduced"

if [ "$STRICT" = 1 ] && { [ -n "$crashes" ] || [ -n "$reproduced" ]; }; then
    exit 1
fi
exit 0
