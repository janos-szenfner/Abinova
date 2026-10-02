#!/bin/sh
# fuzz-add-reproducer.sh — pin a fuzzer-found crash as a regression input
#
# Usage:
#   tools/fuzz-add-reproducer.sh <fmt> <artifact> [artifact ...]
#
# <fmt> is the corpus format name (doc, docx, abw, odt, mht, rtf, wpd —
# the fuzz_<fmt> suffix).  Each file is copied into
# fuzz/regress/<fmt>/ under a stable name and then replayed once
# against the built fuzz_<fmt> binary to report whether it still
# reproduces.  Files are kept regardless of the replay result: a
# reproducer that no longer crashes is proof a fix landed, one that
# still does pins an open bug.
#
# Typical flow after a bounded run finds something:
#   ASAN_OPTIONS=detect_leaks=0 fuzz-build/fuzz_doc \
#       fuzz-build/corpus-doc fuzz/corpus/doc \
#       -max_total_time=25 -rss_limit_mb=1800 -timeout=20 \
#       -artifact_prefix=fuzz-build/artifacts/
#   -> Test unit written to fuzz-build/artifacts/crash-<sha>
#   tools/fuzz-add-reproducer.sh doc fuzz-build/artifacts/crash-<sha>
#   make check-fuzz            # replays it every run from now on
#
# Env: same FUZZ_BUILDDIR / FUZZ_CHECK_RSS_MB / FUZZ_CHECK_TIMEOUT
# knobs as check-fuzz.sh; FUZZ_VERIFY=0 skips the replay.

set -u

SRCROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
BUILDDIR=${FUZZ_BUILDDIR:-$SRCROOT/fuzz-build}
RSS=${FUZZ_CHECK_RSS_MB:-1200}
TIMEOUT=${FUZZ_CHECK_TIMEOUT:-15}
VERIFY=${FUZZ_VERIFY:-1}

usage() {
    echo "usage: $0 <fmt> <artifact> [artifact ...]" >&2
    echo "  fmt: doc docx abw odt mht rtf wpd (or a fuzz_<fmt> target)" >&2
    exit 2
}

[ $# -ge 2 ] || usage

fmt=${1#fuzz_}
shift

[ -f "$SRCROOT/fuzz/fuzz_$fmt.cpp" ] || {
    echo "fuzz-add-reproducer: no harness fuzz/fuzz_$fmt.cpp" >&2
    exit 1
}

rdir=$SRCROOT/fuzz/regress/$fmt
mkdir -p "$rdir"

for f in "$@"; do
    [ -f "$f" ] || { echo "fuzz-add-reproducer: no such file $f" >&2; exit 1; }

    base=$(basename "$f")
    # libFuzzer artifact names (crash-/oom-/timeout-<sha>) are already
    # unique; anything else gets a repro- prefix plus the content hash
    # so identical inputs dedupe by name
    case $base in
        crash-*|oom-*|timeout-*) name=$base ;;
        *) name="repro-$(sha1sum "$f" | cut -c1-12)-$base" ;;
    esac

    dest=$rdir/$name
    if [ -f "$dest" ]; then
        echo "fuzz-add-reproducer: already pinned $fmt/$name"
    else
        cp "$f" "$dest"
        echo "fuzz-add-reproducer: pinned $dest"
    fi

    if [ "$VERIFY" = 1 ] && [ -x "$BUILDDIR/fuzz_$fmt" ]; then
        ASAN_OPTIONS=detect_leaks=0 "$BUILDDIR/fuzz_$fmt" "$dest" \
            -runs=1 -rss_limit_mb="$RSS" -timeout="$TIMEOUT" \
            -artifact_prefix="$BUILDDIR/artifacts/" \
            > "$BUILDDIR/artifacts/.verify.$fmt.log" 2>&1 && rc=0 || rc=$?
        if [ "$rc" -eq 0 ]; then
            echo "fuzz-add-reproducer: replay clean — bug appears fixed (kept as regression)"
        else
            grep -E "^(SUMMARY|ERROR): " "$BUILDDIR/artifacts/.verify.$fmt.log" | head -1
            echo "fuzz-add-reproducer: REPRODUCES — crash is still live"
        fi
    fi
done
