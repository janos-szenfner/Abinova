#!/bin/sh
# Per-directory coverage breakdown from an lcov tracefile.
#
# Reads the first-party tracefile produced by tools/coverage.sh
# (coverage.info) and aggregates line hits/found per source directory
# so each COV* task can cite an honest number for its directory.
# Top-level rows are the COV01 buckets; indented rows break each
# bucket into its immediate subdirectories (xp, gtk, unix, ...).
#
# Usage: tools/coverage-dirs.sh [tracefile] [--worklog FILE]
#
#   tracefile   lcov tracefile to read (default: coverage.info)
#   --worklog   append a dated baseline block (heading + table) to
#               FILE (typically .devin/WORKLOG.md)

set -e

# awk printf %f follows the locale decimal point — pin C so the table
# renders the same on every box.
LC_ALL=C
export LC_ALL

info=coverage.info
worklog=

while [ $# -gt 0 ]; do
	case $1 in
	--worklog) worklog=$2; shift 2 ;;
	--worklog=*) worklog=${1#*=}; shift ;;
	*) info=$1; shift ;;
	esac
done

if [ ! -f "$info" ]; then
	echo "coverage-dirs.sh: cannot read $info" >&2
	exit 1
fi

emit_table() {
	printf '| Directory | Lines hit | Lines found | Coverage |\n'
	printf '|-----------|-----------|-------------|----------|\n'

	awk '
		function top_bucket(rel,   a, n, two) {
			n = split(rel, a, "/")
			if (n < 2) return "(other)"
			two = a[1] "/" a[2]
			if (two == "wp/ap")
				return n >= 3 && a[3] ~ /^(xp|gtk|grammar)$/ \
					? two "/" a[3] : "(other)"
			if (two ~ /^af\/(util|xap|ev|gr|tf)$/ || \
			    two ~ /^text\/(fmt|ptbl)$/ || two == "wp/impexp")
				return two
			return "(other)"
		}
		# directory buckets get subrows per immediate child dir
		function is_dir_bucket(b) {
			return b ~ /^af\// || b ~ /^text\// || b == "wp/impexp"
		}
		# NB: a bare > in printf args is output redirection — compute
		# the percentage first.
		function row(indent, name, lh, lf,   pct) {
			pct = lf > 0 ? 100.0 * lh / lf : 0
			printf "| %s%s | %d | %d | %.1f%% |\n", indent, name, lh, lf, pct
		}
		/^SF:/ {
			path = substr($0, 4)
			sub(/^.*\/src\//, "", path)
			top = top_bucket(path)
			sub_ = ""
			if (is_dir_bucket(top)) {
				if (split(path, a, "/") >= 4)
					sub_ = top "/" a[3]
				else
					sub_ = top "/(top-level)"
				if (!((top, sub_) in SUBSEEN)) {
					SUBSEEN[top, sub_] = 1
					NSUB[top]++
					SUBS[top, NSUB[top]] = sub_
				}
			}
			next
		}
		/^LF:/ { lf = substr($0, 4) + 0; next }
		/^LH:/ {
			lh = substr($0, 4) + 0
			HIT[top] += lh; FOUND[top] += lf
			if (sub_ != "") {
				HIT[sub_] += lh; FOUND[sub_] += lf
			}
			lh = 0; lf = 0
			next
		}
		END {
			ntop = split(TOPORDER, tops, " ")
			for (i = 1; i <= ntop; i++) {
				t = tops[i]
				if (!(t in FOUND)) continue
				row("", t, HIT[t], FOUND[t])
				th += HIT[t]; tf += FOUND[t]
				# insertion-sort the subs (few per bucket)
				for (j = 2; j <= NSUB[t]; j++) {
					v = SUBS[t, j]
					for (k = j - 1; k >= 1 && SUBS[t, k] > v; k--)
						SUBS[t, k + 1] = SUBS[t, k]
					SUBS[t, k + 1] = v
				}
				for (j = 1; j <= NSUB[t]; j++) {
					s = SUBS[t, j]
					row("  ", s, HIT[s], FOUND[s])
				}
			}
			row("", "TOTAL", th, tf)
		}' TOPORDER="af/util af/xap af/ev af/gr af/tf text/fmt text/ptbl wp/impexp wp/ap/xp wp/ap/gtk wp/ap/grammar (other)" "$info"
}

emit_table

if [ -n "$worklog" ]; then
	{
		printf -- '- %s coverage baseline (`make check` under `--enable-coverage`; tools/coverage-dirs.sh):\n\n' "$(date +%F)"
		emit_table | sed 's/^/  /'
		printf '\n'
	} >> "$worklog"
	echo "coverage-dirs.sh: baseline appended to $worklog" >&2
fi
