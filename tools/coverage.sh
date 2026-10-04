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

# Capture all counters.  Ignore 'source' (headers/sources that moved or
# were generated) and 'graph' (incomplete .gcno notes) so a single odd
# object can't sink the run.
"$LCOV" --capture --directory . --output-file "$info_raw" \
	--rc lcov_branch_coverage=0 \
	--ignore-errors source,graph

# Restrict the report to first-party sources: drop system headers,
# vendored thirdparty/, the instrumented fuzz-build scratch tree, test
# drivers (xp/t/, gtk/t/, wp/test/) and generated resources.
"$LCOV" --remove "$info_raw" \
	'/usr/*' \
	'*/thirdparty/*' \
	'*/fuzz-build/*' \
	'*/xp/t/*' \
	'*/gtk/t/*' \
	'*/wp/test/*' \
	'*/abi-resources.c' \
	'*.gperf' \
	--output-file "$info" \
	--rc lcov_branch_coverage=0

"$GENHTML" "$info" --output-directory "$outdir" \
	--title "Abinova test coverage" --legend \
	--rc genhtml_branch_coverage=0 \
	--ignore-errors source

"$LCOV" --summary "$info" --rc lcov_branch_coverage=0 | tee coverage-summary.txt
