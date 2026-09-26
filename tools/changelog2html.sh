#!/bin/sh
# changelog2html.sh — convert CHANGELOG.md into the single-page
# changelog.html served by the built-in help browser
# (help/<lang>/changelog.html, opened by the ribbon's Changelog
# button).  The help browser renders a small HTML subset (h1-h3,
# b, i, code, ul/li, p, a, hr), so the converter emits exactly that.
#
# Usage: changelog2html.sh [CHANGELOG.md] [output.html]
#   defaults: CHANGELOG.md  ->  help/en-US/changelog.html

set -e

src=${1:-CHANGELOG.md}
out=${2:-help/en-US/changelog.html}

[ -f "$src" ] || { echo "changelog2html: cannot read $src" >&2; exit 1; }

awk '
function flush() {
	# emit the buffered block: buf holds joined lines, mode is
	# "none", "p" or "li"
	if (mode == "li") {
		if (!inlist) { print "<ul>"; inlist = 1 }
		printf "<li>%s</li>\n", inline(buf)
	} else if (mode == "p") {
		if (inlist) { print "</ul>"; inlist = 0 }
		printf "<p>%s</p>\n", inline(buf)
	} else if (inlist) {
		print "</ul>"; inlist = 0
	}
	buf = ""; mode = "none"
}
function closelist() {
	if (inlist) { print "</ul>"; inlist = 0 }
}
function head(level, text) {
	flush(); closelist()
	printf "<h%d>%s</h%d>\n", level, inline(text), level
}
function inline(t) {
	# **bold**
	while (match(t, /\*\*[^*]+\*\*/)) {
		seg = substr(t, RSTART + 2, RLENGTH - 4)
		t = substr(t, 1, RSTART - 1) "<b>" seg "</b>" substr(t, RSTART + RLENGTH)
	}
	# `code`
	while (match(t, /`[^`]+`/)) {
		seg = substr(t, RSTART + 1, RLENGTH - 2)
		t = substr(t, 1, RSTART - 1) "<code>" seg "</code>" substr(t, RSTART + RLENGTH)
	}
	return t
}
BEGIN {
	print "<!DOCTYPE html>"
	print "<html><head><meta charset=\"utf-8\">"
	print "<title>Abinova Changelog</title></head><body>"
	inlist = 0; buf = ""; mode = "none"
}
{
	line = $0
	gsub(/&/, "\\&amp;", line)
	gsub(/</, "\\&lt;", line)
	gsub(/>/, "\\&gt;", line)

	if (line ~ /^####+[ ]/)     { sub(/^####+[ ]*/, "", line); head(3, line); next }
	if (line ~ /^###[ ]/)       { sub(/^###[ ]*/, "", line);  head(3, line); next }
	if (line ~ /^##[ ]/)        { sub(/^##[ ]*/, "", line);   head(2, line); next }
	if (line ~ /^#[ ]/)         { sub(/^#[ ]*/, "", line);    head(1, line); next }
	if (line ~ /^---+[ ]*$/)    { flush(); closelist(); print "<hr>"; next }
	if (line ~ /^[ ]*-[ ]/)     {
		sub(/^[ ]*-[ ]*/, "", line)
		flush()                      # closes previous item/para; keeps <ul> open state via inlist
		mode = "li"; buf = line
		next
	}
	if (line ~ /^[ ]*$/)        { flush(); next }
	# continuation: join into the open block with a space so
	# hard-wrapped bold/code spans still match
	if (mode == "none") mode = "p"
	buf = buf (buf == "" ? "" : " ") line
}
END { flush(); print "</body></html>" }
' "$src" > "$out"

echo "changelog2html: wrote $out" >&2
