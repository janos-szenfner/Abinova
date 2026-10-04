#!/bin/sh
# Coverage ratchet gate (COV10): fails when total first-party line
# coverage in an lcov tracefile drops below the required threshold.
#
# Usage: tools/coverage-gate.sh [tracefile] [min-percent]
#
#   tracefile    lcov tracefile to measure (default: coverage.info) —
#                pass the FILTERED first-party file produced by
#                tools/coverage.sh, not coverage.raw.info
#   min-percent  minimum total line coverage required (default: 50,
#                also settable via COVERAGE_MIN_PCT)
#
# Exit status: 0 when coverage >= threshold, 1 when below or the
# tracefile is missing/empty.  The threshold lives here (and in the
# check-coverage make target that calls this) so raising the ratchet
# later is a one-line change.

set -e

# awk printf/compares follow the locale decimal point — pin C.
LC_ALL=C
export LC_ALL

info=coverage.info
min=${COVERAGE_MIN_PCT:-50}

if [ $# -ge 1 ]; then info=$1; fi
if [ $# -ge 2 ]; then min=$2; fi

if [ ! -f "$info" ]; then
	echo "coverage-gate: cannot read $info --" >&2
	echo "  run 'make coverage' (or tools/coverage.sh) first" >&2
	exit 1
fi

awk -v min="$min" -v file="$info" '
	/^LF:/ { lf += substr($0, 4) + 0; next }
	/^LH:/ { lh += substr($0, 4) + 0; next }
	END {
		if (lf == 0) {
			printf "coverage-gate: %s records no lines -- empty tracefile?\n", \
				file > "/dev/stderr"
			exit 1
		}
		pct = 100.0 * lh / lf
		printf "coverage-gate: total line coverage %.2f%% (%d of %d), minimum %.2f%%\n", \
			pct, lh, lf, min + 0
		if (pct < min + 0) {
			printf "coverage-gate: FAIL -- coverage regressed below %.2f%%\n", \
				min + 0 > "/dev/stderr"
			exit 1
		}
		print "coverage-gate: PASS"
	}' "$info"
