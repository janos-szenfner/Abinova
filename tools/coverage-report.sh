#!/bin/sh
# Emit the committed coverage baseline report (.devin/COVERAGE.md):
# headline line/function/branch coverage, the countable-denominator
# definition, a per-directory breakdown, and the worst-covered
# directories/files ranked by uncovered lines.  TST08 — measurement
# and reporting only.
#
# Usage: tools/coverage-report.sh [tracefile|builddir] [--output FILE]
#
#   tracefile   filtered lcov tracefile (default: coverage.info, looked
#               up inside builddir when a directory is given)
#   --output    report path (default: .devin/COVERAGE.md next to the
#               tracefile — i.e. the source root in an in-tree build)
#
# The report wants branch coverage; if the given tracefile has no BRDA
# records a branch-enabled capture is run on the fly (needs a build
# configured --enable-coverage whose tests have already run).
#
# Called automatically at the end of tools/coverage.sh, so the
# committed report refreshes with every `make coverage` /
# `make check-coverage` run.

set -e

# awk printf %f follows the locale decimal point — pin C so the report
# renders identically on every box.
LC_ALL=C
export LC_ALL

tooldir=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)

info=coverage.info
output=
while [ $# -gt 0 ]; do
	case $1 in
	--output) output=$2; shift 2 ;;
	--output=*) output=${1#*=}; shift ;;
	*) info=$1; shift ;;
	esac
done

if [ -d "$info" ]; then
	info=$info/coverage.info
fi
if [ ! -f "$info" ]; then
	echo "coverage-report.sh: cannot read $info --" >&2
	echo "  run 'make coverage' (or tools/coverage.sh) first" >&2
	exit 1
fi

builddir=$(CDPATH='' cd -- "$(dirname "$info")" && pwd)
info=$builddir/$(basename "$info")

if [ -z "$output" ]; then
	output=$builddir/.devin/COVERAGE.md
fi

# Branch coverage is part of the report; when handed a line-only
# tracefile, capture again with lcov_branch_coverage=1.
if ! grep -qm1 '^BRDA:' "$info"; then
	br=$builddir/coverage.br.info
	raw=$builddir/coverage.brraw.info
	set -f # exclude globs must reach lcov verbatim, not expand in cwd
	set -- $(grep -v '^#' "$tooldir/coverage-excludes.txt" | grep -v '^[[:space:]]*$')
	set +f
	"$tooldir/lcov/lcov" --capture --directory "$builddir" \
		--output-file "$raw" \
		--rc lcov_branch_coverage=1 \
		--ignore-errors gcov,source,graph
	"$tooldir/lcov/lcov" --remove "$raw" "$@" \
		--output-file "$br" \
		--rc lcov_branch_coverage=1
	info=$br
fi

mkdir -p "$(dirname "$output")"

tmpd=$(mktemp -d) || exit 1
trap 'rm -rf "$tmpd"' EXIT

# One pass over the tracefile: per-file rows for the worst-files table,
# per-leaf-directory aggregates for the worst-dirs table, per-bucket
# aggregates (COV01 layout) for the breakdown table, and the totals.
awk -v DIRS="$tmpd/dirs.tsv" -v FILES_T="$tmpd/files.tsv" \
	-v TOTALS="$tmpd/totals.tsv" -v BUCKETS="$tmpd/buckets.tsv" \
	-v TOPORDER="af/util af/xap af/ev af/gr af/tf text/fmt text/ptbl wp/impexp wp/ap/xp wp/ap/gtk wp/ap/grammar wp/main (other)" '
	function top_bucket(rel,   a, n, two) {
		n = split(rel, a, "/")
		if (n < 2) return "(other)"
		two = a[1] "/" a[2]
		if (two == "wp/ap")
			return n >= 3 && a[3] ~ /^(xp|gtk|grammar)$/ \
				? two "/" a[3] : "(other)"
		if (two == "wp/main") return two
		if (two ~ /^af\/(util|xap|ev|gr|tf)$/ || \
		    two ~ /^text\/(fmt|ptbl)$/ || two == "wp/impexp")
			return two
		return "(other)"
	}
	function is_dir_bucket(b) {
		return b ~ /^af\// || b ~ /^text\// || b == "wp/impexp"
	}
	/^SF:/ {
		path = substr($0, 4)
		rel = path
		sub(/^.*\/src\//, "", rel)
		top = top_bucket(rel)
		n = split(rel, pa, "/")
		leaf = rel
		sub(/\/[^\/]*$/, "", leaf)
		if (leaf == rel) leaf = "(src root)"
		sub_ = ""
		if (is_dir_bucket(top))
			sub_ = n >= 4 ? top "/" pa[3] : top "/(top-level)"
		lh = lf = fnh = fnf = brh = brf = 0
		next
	}
	/^FNF:/ { fnf = substr($0, 5) + 0; next }
	/^FNH:/ { fnh = substr($0, 5) + 0; next }
	/^BRF:/ { brf = substr($0, 5) + 0; next }
	/^BRH:/ { brh = substr($0, 5) + 0; next }
	/^LF:/  { lf  = substr($0, 4) + 0; next }
	/^LH:/  { lh  = substr($0, 4) + 0; next }
	/^end_of_record/ {
		FILES[top]++
		LH[top] += lh; LF[top] += lf; BRH[top] += brh; BRF[top] += brf
		if (sub_ != "") {
			FILES[sub_]++
			LH[sub_] += lh; LF[sub_] += lf
			BRH[sub_] += brh; BRF[sub_] += brf
			if (!((top, sub_) in SUBSEEN)) {
				SUBSEEN[top, sub_] = 1
				SUBS[top, ++NSUB[top]] = sub_
			}
		}
		DLF[leaf] += lf; DLH[leaf] += lh; DBRF[leaf] += brf; DBRH[leaf] += brh
		printf "%d\t%s\t%d\t%d\n", lf - lh, rel, lh, lf > FILES_T
		tlh += lh; tlf += lf; tfnh += fnh; tfnf += fnf
		tbrh += brh; tbrf += brf; tfiles++
		next
	}
	END {
		for (d in DLF)
			printf "%d\t%s\t%d\t%d\n", DLF[d] - DLH[d], d, DLH[d], DLF[d] > DIRS
		printf "%d\t%d\t%d\t%d\t%d\t%d\t%d\n", \
			tlh, tlf, tfnh, tfnf, tbrh, tbrf, tfiles > TOTALS
		ntop = split(TOPORDER, tops, " ")
		for (i = 1; i <= ntop; i++) {
			t = tops[i]
			if (!(t in LF)) continue
			printf "%s\t%d\t%d\t%d\t%d\t%d\n", \
				t, FILES[t], LH[t], LF[t], BRH[t], BRF[t] > BUCKETS
			for (j = 2; j <= NSUB[t]; j++) {
				v = SUBS[t, j]
				for (k = j - 1; k >= 1 && SUBS[t, k] > v; k--)
					SUBS[t, k + 1] = SUBS[t, k]
				SUBS[t, k + 1] = v
			}
			for (j = 1; j <= NSUB[t]; j++) {
				s = SUBS[t, j]
				printf "  %s\t%d\t%d\t%d\t%d\t%d\n", \
					s, FILES[s], LH[s], LF[s], BRH[s], BRF[s] > BUCKETS
			}
		}
	}' "$info"

# Worst-covered leaf dirs / files, ranked by uncovered lines.
tab=$(printf '\t')
sort -t"$tab" -k1,1nr "$tmpd/dirs.tsv" | head -20 > "$tmpd/dirs-top.tsv"
sort -t"$tab" -k1,1nr "$tmpd/files.tsv" | head -15 > "$tmpd/files-top.tsv"

exclusion_reason() {
	case $1 in
	'/usr/*') echo "system headers and libraries — not our code" ;;
	'*/thirdparty/*') echo "vendored libraries (see thirdparty/VENDORED.json)" ;;
	'*/fuzz-build/*') echo "instrumented fuzz/sanitizer scratch tree" ;;
	'*/xp/t/*') echo "autotest sources (test drivers are not product code)" ;;
	'*/gtk/t/*') echo "autotest sources (test drivers are not product code)" ;;
	'*/wp/test/*') echo "autotest sources (test drivers are not product code)" ;;
	'*/af/tf/*') echo "test-harness support library (autotest tooling)" ;;
	'*/abi-resources.c') echo "generated (glib-compile-resources output)" ;;
	'*.gperf') echo "generated (gperf hash emitted as an #include)" ;;
	*) echo "—" ;;
	esac
}

{
	printf '# Abinova test coverage baseline\n\n'
	printf 'Generated by `tools/coverage-report.sh` on %s from `%s` —\n' \
		"$(date +%F)" "$(basename "$info")"
	printf 'an `--enable-coverage` build exercised by `make check` (unit\n'
	printf 'suite, round-trip corpus, and the dialog/UI legs under xvfb).\n'
	printf 'Regenerate with `make coverage` or `make check-coverage`.\n\n'

	printf '## Headline\n\n'
	printf '| Metric | Covered | Countable | Coverage |\n'
	printf '|--------|---------|-----------|----------|\n'
	awk -F '\t' '
		{ tlh=$1; tlf=$2; tfnh=$3; tfnf=$4; tbrh=$5; tbrf=$6 }
		END {
			lpct = tlf > 0 ? 100.0*tlh/tlf : 0
			fpct = tfnf > 0 ? 100.0*tfnh/tfnf : 0
			bpct = tbrf > 0 ? 100.0*tbrh/tbrf : 0
			printf "| Lines | %d | %d | %.2f%% |\n", tlh, tlf, lpct
			printf "| Functions | %d | %d | %.2f%% |\n", tfnh, tfnf, fpct
			printf "| Branches | %d | %d | %.2f%% |\n", tbrh, tbrf, bpct
		}' "$tmpd/totals.tsv"
	printf '\nUser directive: **>= 90%%** line coverage of the countable\n'
	printf 'denominator below. Enforced floor: `make check-coverage` fails\n'
	printf 'below the ratchet in tools/coverage-gate.sh.\n\n'

	printf '## The countable denominator\n\n'
	printf 'Counted: every first-party C/C++ source under `src/` that this\n'
	printf 'Linux build compiles — `af/` (app framework), `text/` (piece\n'
	printf 'table and layout) and `wp/` (importers/exporters, application\n'
	printf 'layer, entry points) — including compiled inline/template code\n'
	printf 'attributed to headers.\n\n'
	printf 'Excluded via lcov filters (tools/coverage-excludes.txt):\n\n'
	grep -v '^#' "$tooldir/coverage-excludes.txt" | grep -v '^[[:space:]]*$' |
		while IFS= read -r pat; do
			printf -- '- `%s` — %s\n' "$pat" "$(exclusion_reason "$pat")"
		done
	printf '\nExcluded by construction: code that never compiles in this\n'
	printf 'build contributes no lines to the denominator — `#ifdef\n'
	printf 'G_OS_WIN32`/`__APPLE__`/other-OS branches and whole files the\n'
	printf 'build skips.  There is no way to count them short of building\n'
	printf 'on those platforms; treat their absence as a documented\n'
	printf 'limitation, not hidden uncovered code.\n\n'
	printf 'Counted but flagged: `src/wp/main/gtk` (process entry points)\n'
	printf 'shows as its own row.\n\n'

	printf '## Per-directory breakdown\n\n'
	printf '| Directory | Files | Lines hit | Lines found | Lines %% | Branches hit | Branches found | Branches %% | Uncovered lines |\n'
	printf '|-----------|-------|-----------|-------------|---------|--------------|----------------|-----------|-----------------|\n'
	awk -F '\t' '
		{ pct = $4 > 0 ? 100.0*$3/$4 : 0
		  bpct = $6 > 0 ? 100.0*$5/$6 : 0
		  printf "| %s | %d | %d | %d | %.1f%% | %d | %d | %.1f%% | %d |\n", \
			$1, $2, $3, $4, pct, $5, $6, bpct, $4 - $3 }' \
		"$tmpd/buckets.tsv"
	awk -F '\t' '{ lpct = $2 > 0 ? 100.0*$1/$2 : 0; \
		bpct = $6 > 0 ? 100.0*$5/$6 : 0; \
		printf "| **TOTAL** | %d | %d | %d | %.1f%% | %d | %d | %.1f%% | %d |\n", \
		$7, $1, $2, lpct, $5, $6, bpct, $2 - $1 }' \
		"$tmpd/totals.tsv"
	printf '\n'

	printf '## Worst-covered directories\n\n'
	printf 'Leaf directories ranked by uncovered lines (the exclusions above\n'
	printf 'already remove generated/test/vendored code, so what remains is\n'
	printf 'code that matters).\n\n'
	printf '| Directory | Uncovered | Lines found | Line %% |\n'
	printf '|-----------|-----------|-------------|----------|\n'
	awk -F '\t' '{ pct = $4 > 0 ? 100.0*$3/$4 : 0; \
		printf "| %s | %d | %d | %.1f%% |\n", $2, $1, $4, pct }' \
		"$tmpd/dirs-top.tsv"
	printf '\n'

	printf '## Worst-covered files\n\n'
	printf '| File | Uncovered | Lines found | Line %% |\n'
	printf '|------|-----------|-------------|----------|\n'
	awk -F '\t' '{ pct = $4 > 0 ? 100.0*$3/$4 : 0; \
		printf "| %s | %d | %d | %.1f%% |\n", $2, $1, $4, pct }' \
		"$tmpd/files-top.tsv"
	printf '\n'
} > "$output"

echo "coverage-report.sh: wrote $output" >&2
