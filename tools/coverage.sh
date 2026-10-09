#!/bin/sh
# Capture gcov coverage data from the build tree and produce an lcov
# tracefile plus an HTML report, using the lcov copy vendored in
# tools/lcov/ (no system lcov required).
#
# Requires a build configured with --enable-coverage and instrumented
# code that has been exercised (e.g. `make check`) so that .gcda files
# exist next to the .gcno notes in the build tree.
#
# Usage: tools/coverage.sh [builddir]
#
#   builddir  - top of the build tree to scan (default: current dir)

set -e

tooldir=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
builddir=${1:-.}

LCOV="$tooldir/lcov/lcov"
GENHTML="$tooldir/lcov/genhtml"

cd "$builddir"

if ! find . -name '*.gcda' -print -quit | grep -q .; then
	echo "coverage.sh: no .gcda files under $builddir --" >&2
	echo "  configure with --enable-coverage, build, then run 'make check' first" >&2
	exit 1
fi

info_raw=coverage.raw.info
info=coverage.info
outdir=coverage-html

# Capture all counters, branch data included (the committed
# .devin/COVERAGE.md report is line + branch coverage).  Ignore
# 'source' (headers/sources that moved or were generated), 'graph'
# (incomplete .gcno notes) and 'gcov' (a .gcda being rewritten by a
# still-running test) so a single odd object can't sink the run.
"$LCOV" --capture --directory . --output-file "$info_raw" \
	--rc lcov_branch_coverage=1 \
	--ignore-errors gcov,source,graph

# Restrict the report to first-party sources.  The exclusion globs live
# in tools/coverage-excludes.txt — they are the "countable denominator"
# documented in .devin/COVERAGE.md (system headers, vendored
# thirdparty/, the fuzz-build scratch tree, autotest sources and
# generated files).
set -f # patterns must reach lcov verbatim, not glob-expand in cwd
set -- $(grep -v '^#' "$tooldir/coverage-excludes.txt" | grep -v '^[[:space:]]*$')
set +f
"$LCOV" --remove "$info_raw" "$@" \
	--output-file "$info" \
	--rc lcov_branch_coverage=1

"$GENHTML" "$info" --output-directory "$outdir" \
	--title "Abinova test coverage" --legend \
	--rc genhtml_branch_coverage=0 \
	--ignore-errors source

"$LCOV" --summary "$info" --rc lcov_branch_coverage=1 | tee coverage-summary.txt

# Per-directory breakdown (COV01): the COV task targets each cite a
# directory, so emit an aggregate row per bucket.  Record the table
# into .devin/WORKLOG.md explicitly with:
#   tools/coverage-dirs.sh coverage.info --worklog .devin/WORKLOG.md
"$tooldir/coverage-dirs.sh" "$info" | tee coverage-dirs.txt

# Committed baseline report (TST08): .devin/COVERAGE.md with the
# denominator definition, headline numbers, per-directory breakdown and
# the worst-covered directories/files.
"$tooldir/coverage-report.sh" "$info" --output .devin/COVERAGE.md
