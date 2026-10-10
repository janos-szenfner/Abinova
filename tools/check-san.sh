#!/bin/sh
# check-san.sh — build a sanitizer-instrumented copy of the tree and run
# the `make check` suite inside it.
#
# Wired to `make check-asan` / `make check-ubsan` (top level).  Both
# targets call this script: ONE scratch build carries
# -fsanitize=address,undefined, so a single instrumented suite run
# reports both error classes — the two target names exist so the usual
# sanitizer spellings are directly discoverable.  For a hand-rolled
# instrumented build, configure --with-sanitizer=<list> does the same
# flag injection and plain `make check` runs under it.
#
# Layout this script maintains (mirroring tools/build-fuzz.sh):
#   san-build/tree/   rsync'd working-tree sources, configured
#                     --enable-maintainer-mode --with-sanitizer=
#                     address,undefined.  The scratch tree is configured
#                     in-place because the repo's own srcdir is already
#                     configured — autoconf refuses a VPATH build
#                     against it.  Objects are excluded from the sync
#                     so stale non-instrumented .o files can never
#                     slip in.
#
# The suite runs in two passes (automake TESTS override):
#   1. unit suite   TESTS=unix/testwrap.sh — the whole Abinova-test
#      binary.  Leak checking ON by default; testwrap.sh writes
#      src/wp/test/supp.txt and exports LSAN_OPTIONS for it.
#   2. corpus       TESTS=unix/rtwrap.sh — the rt-check round-trip
#      matrix through the instrumented src/abinova, ~250 CLI
#      invocations across every importer/exporter.  detect_leaks=0
#      here: one-time fontconfig/pango/gio init caches would report
#      as exit-time leaks on every leg — memory/UB errors still abort
#      the leg and fail the suite.  Leak gating on the corpus is
#      check-valgrind's job.
#
# This is a permanent gate for the UB01-07 and DOC15-21 hardening: any
# OOB/UAF/UB regression in first-party or vendored code aborts the
# instrumented run and fails make check.
#
# Env:
#   SAN_JOBS=N          parallel make jobs (default 2 — stays under the
#                       task-loop 2048MB RSS watchdog)
#   SAN_BUILDDIR        scratch root (default <repo>/san-build)
#   SAN_SANITIZERS      -fsanitize list (default address,undefined)
#   SAN_RECONFIGURE=1   force a fresh configure of the scratch tree
#   SAN_DETECT_LEAKS=0/1  LSan leak checking for the unit suite
#                       (default 1 — supp.txt covers known library
#                       caches; set 0 to gate only on errors)
#   SAN_CHECK_CORPUS=0/1  also run the rtwrap corpus (default 1)
#
# Runtime cost: a full tree rebuild on first run (instrumentation adds
# ~1.5-2x compile time) plus an instrumented suite run — plan for tens
# of minutes, not seconds.  Incremental runs only pay for changed
# objects.  Normal builds stay sanitizer-free: this is opt-in only.

set -e

MODE=${1:-san}
SRCROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
BUILDDIR=${SAN_BUILDDIR:-$SRCROOT/san-build}
TREE=$BUILDDIR/tree
JOBS=${SAN_JOBS:-2}
SANITIZERS=${SAN_SANITIZERS:-address,undefined}
LEAKS=${SAN_DETECT_LEAKS:-1}
CORPUS=${SAN_CHECK_CORPUS:-1}

echo "check-san ($MODE): instrumented tree at $TREE (sanitizers: $SANITIZERS)"

mkdir -p "$TREE"
# Never mirror configure/make outputs — a stale Makefile or
# config.status landing in the scratch tree silently un-instruments
# the build.  Anchored excludes where a tracked file could collide
# (thirdparty/wv-1.2.9/config.h is a real source).
rsync -a --delete \
    --exclude='/.git' --exclude='/fuzz-build' --exclude='/san-build' \
    --exclude='/autom4te.cache' \
    --exclude='*.o' --exclude='*.lo' --exclude='*.la' \
    --exclude='*/.libs' --exclude='*/.deps' \
    --exclude='*.Po' --exclude='*.Plo' \
    --exclude='*.gcno' --exclude='*.gcda' \
    --exclude='/coverage.info' --exclude='/coverage.raw.info' \
    --exclude='/coverage-summary.txt' --exclude='/coverage-html' \
    --exclude='Makefile' \
    --exclude='/config.status' --exclude='/config.log' \
    --exclude='/config.h' --exclude='/stamp-h1' --exclude='/libtool' \
    --exclude='*.pc' \
    --exclude='/io.github.janos_szenfner.Abinova.metainfo.xml' \
    --exclude='/src/wp/test/unix/testwrap.sh' \
    --exclude='/src/wp/test/unix/rtwrap.sh' \
    --exclude='*-stamp' --exclude='*.trs' --exclude='*.gir' \
    --exclude='*.typelib' --exclude='vgcore.*' --exclude='*.saved' \
    --exclude='/src/abinova' --exclude='/src/wp/test/Abinova-test' \
    "$SRCROOT/" "$TREE/"

# configure once (or when the sanitizer flag set changed)
stamp=$BUILDDIR/.configure-stamp
wanted="SANITIZERS=$SANITIZERS"
if [ ! -f "$TREE/src/Makefile" ] || [ ! -f "$stamp" ] || \
   [ "$(cat "$stamp")" != "$wanted" ] || [ -n "${SAN_RECONFIGURE:-}" ]; then
    echo "check-san: configuring instrumented tree"
    (cd "$TREE" && ./configure --enable-maintainer-mode \
        --with-sanitizer="$SANITIZERS" \
        > "$BUILDDIR/configure.out" 2>&1) || {
        tail -20 "$BUILDDIR/configure.out" >&2
        exit 1
    }
    printf '%s\n' "$wanted" > "$stamp"
fi

echo "check-san: building instrumented tree (-j$JOBS)"
make -C "$TREE" -j"$JOBS" > "$BUILDDIR/build.out" 2>&1 || {
    tail -30 "$BUILDDIR/build.out" >&2
    exit 1
}

run_tests() # $1=TESTS value $2=detect_leaks $3=label
{
    echo "check-san: running $3 (TESTS=$1, detect_leaks=$2)"
    if ASAN_OPTIONS="halt_on_error=1:abort_on_error=1:detect_leaks=$2" \
       UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" \
       TF_MAX_TEST_TIME=${SAN_MAX_TEST_TIME:-300} \
       make -C "$TREE/src/wp/test" check TESTS="$1" \
           > "$BUILDDIR/check-$3.out" 2>&1; then
        echo "check-san: $3 PASS"
    else
        echo "check-san: $3 FAIL — tail of test-suite.log:" >&2
        tail -40 "$TREE/src/wp/test/test-suite.log" >&2 || true
        echo "check-san: full make output: $BUILDDIR/check-$3.out" >&2
        exit 1
    fi
}

run_tests "unix/testwrap.sh" "$LEAKS" unit
if [ "$CORPUS" = "1" ]; then
    run_tests "unix/rtwrap.sh" 0 corpus
fi

echo "check-san: suite clean under -fsanitize=$SANITIZERS"
