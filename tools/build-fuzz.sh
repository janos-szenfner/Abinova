#!/bin/sh
# build-fuzz.sh — optional clang/libFuzzer build for Abinova importers
#
# A libFuzzer target is only useful when the code under test carries
# coverage instrumentation, so this script maintains a SEPARATE clang
# build of the whole tree — the normal build stays GCC:
#
#   fuzz-build/tree/        rsync'd working-tree sources, configured as
#                           CC=clang CXX=clang++ -fsanitize=fuzzer-no-link,address
#   fuzz-build/fuzz_<name>  libtool-wrapped fuzz binaries, one per
#                           fuzz/fuzz_<name>.cpp, compiled + linked with
#                           -fsanitize=fuzzer,address
#
# The scratch tree is configured in-place (the repo's own srcdir is
# already configured, so autoconf refuses a VPATH build against it).
# Objects are excluded from the sync, so stale GCC .o files can never
# slip into the instrumented lib.
#
# Usage:
#   tools/build-fuzz.sh [fuzz_abw ...]     # default: every fuzz/fuzz_*.cpp
#
# Corpus-only smoke (no mutation, exits after the seeds):
#   ASAN_OPTIONS=detect_leaks=0 fuzz-build/fuzz_abw fuzz/corpus/abw -runs=0
#
# Bounded fuzz run (new corpus units land in the FIRST dir given, so
# point it at a scratch dir to keep fuzz/corpus/ clean):
#   ASAN_OPTIONS=detect_leaks=0 fuzz-build/fuzz_abw \
#       fuzz-build/corpus-abw fuzz/corpus/abw \
#       -max_total_time=25 -rss_limit_mb=1800 -timeout=20 \
#       -artifact_prefix=fuzz-build/artifacts/
#   (crashes/ooms/timeouts land as crash-*/oom-*/timeout-* files there)
#
# detect_leaks=0 is deliberate: one-time init allocations (fontconfig,
# the app singleton) report at exit as bogus empty crash- files and
# drown the real findings.  Real memory errors still abort mid-run.
#
# Env:
#   FUZZ_JOBS=N      parallel make jobs (default 2 — stays under the
#                    task-loop 2048MB RSS watchdog)
#   FUZZ_BUILDDIR    scratch root (default <repo>/fuzz-build)
#   FUZZ_RECONFIGURE=1  force a fresh configure of the scratch tree

set -e

SRCROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
BUILDDIR=${FUZZ_BUILDDIR:-$SRCROOT/fuzz-build}
TREE=$BUILDDIR/tree
JOBS=${FUZZ_JOBS:-2}

CLANG=${CLANG:-clang}
CLANGXX=${CLANGXX:-clang++}
SAN=-fsanitize=fuzzer-no-link,address
SANLINK=-fsanitize=fuzzer,address

command -v "$CLANGXX" >/dev/null 2>&1 || {
    echo "build-fuzz: clang++ not found (needed for -fsanitize=fuzzer)" >&2
    exit 1
}

# probe the fuzzer runtime before paying for a full build
probe_src=$BUILDDIR/.probe.cpp
mkdir -p "$BUILDDIR"
cat > "$probe_src" <<'EOF'
#include <cstdint>
#include <cstddef>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *, size_t) { return 0; }
EOF
if ! "$CLANGXX" $SANLINK "$probe_src" -o "$BUILDDIR/.probe" 2>"$BUILDDIR/.probe.err"; then
    echo "build-fuzz: clang++ cannot link -fsanitize=fuzzer,address:" >&2
    cat "$BUILDDIR/.probe.err" >&2
    exit 1
fi
rm -f "$probe_src" "$BUILDDIR/.probe" "$BUILDDIR/.probe.err"

echo "build-fuzz: syncing $SRCROOT -> $TREE"
mkdir -p "$TREE"
# Never mirror configure/make outputs — a stale gcc Makefile or
# config.status landing in the scratch tree silently un-instruments
# the build.  Anchored excludes where a tracked file could collide
# (thirdparty/wv-1.2.9/config.h is a real source).
rsync -a --delete \
    --exclude='/.git' --exclude='/fuzz-build' --exclude='/autom4te.cache' \
    --exclude='*.o' --exclude='*.lo' --exclude='*.la' \
    --exclude='*/.libs' --exclude='*/.deps' \
    --exclude='*.Po' --exclude='*.Plo' \
    --exclude='Makefile' \
    --exclude='/config.status' --exclude='/config.log' \
    --exclude='/config.h' --exclude='/stamp-h1' --exclude='/libtool' \
    --exclude='*.pc' \
    --exclude='/io.github.janos_szenfner.Abinova.metainfo.xml' \
    --exclude='/src/wp/test/unix/testwrap.sh' \
    --exclude='*-stamp' --exclude='*.trs' --exclude='*.gir' \
    --exclude='*.typelib' --exclude='vgcore.*' --exclude='*.saved' \
    --exclude='/src/abinova' --exclude='/src/wp/test/Abinova-test' \
    "$SRCROOT/" "$TREE/"

# configure once (or when the sanitizer flag set changed)
stamp=$BUILDDIR/.configure-stamp
wanted="CC=$CLANG CXX=$CLANGXX $SAN -Wno-error=int-conversion"
if [ ! -f "$TREE/src/Makefile" ] || [ ! -f "$stamp" ] || \
   [ "$(cat "$stamp")" != "$wanted" ] || [ -n "$FUZZ_RECONFIGURE" ]; then
    # -Wno-error= on C: clang promotes int-conversion and
    # implicit-function-declaration to hard errors; the vendored C
    # (wv) predates that — GCC treats them as warnings in the normal
    # build, so relax them here the same way. C++ stays strict.
    echo "build-fuzz: configuring instrumented tree"
    (cd "$TREE" && ./configure --enable-maintainer-mode \
        CC="$CLANG" CXX="$CLANGXX -std=c++17" \
        CFLAGS="-g -O1 $SAN -fno-omit-frame-pointer -Wno-error=int-conversion -Wno-error=implicit-function-declaration" \
        CXXFLAGS="-g -O1 $SAN -fno-omit-frame-pointer" \
        LDFLAGS="$SAN" \
        > "$BUILDDIR/configure.out" 2>&1) || {
        tail -20 "$BUILDDIR/configure.out" >&2
        exit 1
    }
    printf '%s\n' "$wanted" > "$stamp"
fi

echo "build-fuzz: building instrumented lib (-j$JOBS)"
make -C "$TREE" -j"$JOBS" > "$BUILDDIR/build.out" 2>&1 || {
    tail -30 "$BUILDDIR/build.out" >&2
    exit 1
}

# compile + link flags come from the instrumented tree's own generated
# Makefile so -I/-D/-l sets track the real build (DEFS carries
# -DHAVE_CONFIG_H, DEFAULT_INCLUDES covers config.h; DEPS_LIBS covers
# the gsf/glib/gobject symbols the harness calls directly — the shared
# lib does not re-export them)
APDIR=$TREE/src/wp/ap
FUZZ_FLAGS=$(
    cd "$APDIR" && make -s -f - __fuzz_flags <<'EOF'
include Makefile
__fuzz_flags:
	@echo $(DEFS) $(DEFAULT_INCLUDES) $(CPPFLAGS) $(WP_CPPFLAGS) $(DEPS_CFLAGS)
__fuzz_libs:
	@echo $(DEPS_LIBS)
EOF
)
FUZZ_LIBS=$(
    cd "$APDIR" && make -s -f - __fuzz_libs <<'EOF'
include Makefile
__fuzz_flags:
	@echo $(DEFS) $(DEFAULT_INCLUDES) $(CPPFLAGS) $(WP_CPPFLAGS) $(DEPS_CFLAGS)
__fuzz_libs:
	@echo $(DEPS_LIBS)
EOF
)
FUZZ_CPPFLAGS=$FUZZ_FLAGS

targets="$*"
if [ -z "$targets" ]; then
    for f in "$SRCROOT"/fuzz/fuzz_*.cpp; do
        [ -e "$f" ] && targets="$targets $(basename "$f" .cpp)"
    done
fi
[ -n "$targets" ] || { echo "build-fuzz: no fuzz/fuzz_*.cpp sources" >&2; exit 1; }

for t in $targets; do
    src=$SRCROOT/fuzz/$t.cpp
    [ -f "$src" ] || { echo "build-fuzz: no such target source $src" >&2; exit 1; }
    echo "build-fuzz: building $t"
    # -I paths in FUZZ_CPPFLAGS are relative to src/wp/ap — compile there
    (cd "$APDIR" && "$CLANGXX" $FUZZ_CPPFLAGS $SAN \
        -g -O1 -fno-omit-frame-pointer -c "$src" -o "$BUILDDIR/$t.o")
    "$TREE/libtool" --mode=link --tag=CXX "$CLANGXX" $SANLINK \
        -g "$BUILDDIR/$t.o" "$TREE/src/libabinova-4.0.la" \
        $FUZZ_LIBS \
        -o "$BUILDDIR/$t" > "$BUILDDIR/$t.link.out" 2>&1 || {
        cat "$BUILDDIR/$t.link.out" >&2
        exit 1
    }
    echo "build-fuzz: $BUILDDIR/$t ready (real binary in $BUILDDIR/.libs/)"
done
