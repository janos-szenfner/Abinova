#!/bin/bash
# rt-check.sh — multi-format round-trip corpus check for Abinova.
#
# Usage: tools/rt-check.sh <abinova-binary> <top-srcdir>
#
# Generalizes tools/epub-rt-check.sh to the whole import/export matrix.
# For every (fixture, format) leg in the matrix at the bottom:
#   1. fixture -> <fmt> export
#   2. format-specific structural validation of the output:
#      docx: zip with [Content_Types].xml + well-formed word/document.xml
#      odt:  'mimetype' member first+stored + well-formed content.xml
#      abwn: well-formed XML with an <abinova> root element
#      rtf/doc: '{\rtf' magic (the .doc exporter emits RTF)
#      html: contains an <html tag;  txt/md: non-empty
#   3. the export is re-imported and converted to txt; its normalized
#      text must equal the fixture's own --to=txt output.  The
#      normalizer collapses whitespace, trims edges and masks digit
#      runs so live fields (page numbers, times, dates) cannot flake.
#   4. legs marked img: additionally require the count of image data
#      items in fixture -> abwn to equal export -> abwn.
#
# Exit 0 if every leg passes, 1 otherwise.  The matrix only contains
# fixture/format pairs that are expected to round-trip; known
# fidelity gaps (markdown tables, positioned images in odt, ...) are
# simply not listed — add a leg when the gap is fixed.

set -u
BIN="${1:?usage: rt-check.sh <abinova-binary> <top-srcdir>}"
SRC="${2:?usage: rt-check.sh <abinova-binary> <top-srcdir>}"
TMPD=$(mktemp -d)
trap 'rm -rf "$TMPD"' EXIT
FAIL=0
PASS=0

norm() { tr -s '[:space:]' ' ' <"$1" | sed 's/[0-9]\{1,\}/#/g; s/^ *//; s/ *$//'; }
imgcount() { grep -o 'mime-type="image/' "$1" | wc -l; }

validate() { # $1=exported file $2=format
    case "$2" in
    docx)
        python3 - "$1" <<'PYEOF'
import sys, zipfile, xml.dom.minidom
zf = zipfile.ZipFile(sys.argv[1])
names = zf.namelist()
assert '[Content_Types].xml' in names, 'missing [Content_Types].xml'
assert 'word/document.xml' in names, 'missing word/document.xml'
d = xml.dom.minidom.parseString(zf.read('word/document.xml'))
assert d.documentElement.localName == 'document', 'bad document root'
PYEOF
        ;;
    odt)
        python3 - "$1" <<'PYEOF'
import sys, zipfile, xml.dom.minidom
zf = zipfile.ZipFile(sys.argv[1])
assert zf.namelist()[0] == 'mimetype', 'mimetype member not first'
assert zf.getinfo('mimetype').compress_type == zipfile.ZIP_STORED, 'mimetype compressed'
assert zf.read('mimetype') == b'application/vnd.oasis.opendocument.text', 'wrong mimetype'
xml.dom.minidom.parseString(zf.read('content.xml'))
PYEOF
        ;;
    abwn)
        python3 - "$1" <<'PYEOF'
import sys, xml.dom.minidom
d = xml.dom.minidom.parse(sys.argv[1])
assert d.documentElement.localName == 'abinova', 'bad document root'
PYEOF
        ;;
    rtf|doc)
        head -c5 "$1" | grep -q '^{\\rtf' ;;
    html)
        grep -qi '<html' "$1" ;;
    txt|md)
        [ -s "$1" ] && grep -q '[^[:space:]]' "$1" ;;
    *)
        [ -s "$1" ] ;;
    esac
}

leg() { # $1=fixture path (relative to $SRC) $2=export format $3="img"|"-"
    local name="$1" fmt="$2" out="$TMPD/out.$2"
    echo "=== $name -> $fmt"
    if ! "$BIN" --to=txt --to-name="$TMPD/src.txt" "$SRC/$name" >/dev/null 2>&1; then
        echo "  FAIL: source -> txt"; FAIL=$((FAIL + 1)); return
    fi
    if ! "$BIN" --to="$fmt" --to-name="$out" "$SRC/$name" >/dev/null 2>&1; then
        echo "  FAIL: export"; FAIL=$((FAIL + 1)); return
    fi
    if ! validate "$out" "$fmt"; then
        echo "  FAIL: structure"; FAIL=$((FAIL + 1)); return
    fi
    if ! "$BIN" --to=txt --to-name="$TMPD/rt.txt" "$out" >/dev/null 2>&1; then
        echo "  FAIL: reimport"; FAIL=$((FAIL + 1)); return
    fi
    if [ "$(norm "$TMPD/src.txt")" != "$(norm "$TMPD/rt.txt")" ]; then
        echo "  FAIL: text differs"
        diff <(norm "$TMPD/src.txt") <(norm "$TMPD/rt.txt") | head -4
        FAIL=$((FAIL + 1)); return
    fi
    if [ "$3" = "img" ]; then
        "$BIN" --to=abwn --to-name="$TMPD/src.abwn" "$SRC/$name" >/dev/null 2>&1
        "$BIN" --to=abwn --to-name="$TMPD/rt.abwn" "$out" >/dev/null 2>&1
        local a b
        a=$(imgcount "$TMPD/src.abwn")
        b=$(imgcount "$TMPD/rt.abwn")
        if [ "$a" != "$b" ]; then
            echo "  FAIL: image data items $a -> $b"; FAIL=$((FAIL + 1)); return
        fi
        echo "  text+structure OK, $b image(s)"
    else
        echo "  text+structure OK"
    fi
    PASS=$((PASS + 1))
}

run() { # $1=fixture (relative to $SRC); rest = legs ('img:' prefix = image assert)
    local file="$1"; shift
    for spec in "$@"; do
        case "$spec" in
        img:*) leg "$file" "${spec#img:}" img ;;
        *)     leg "$file" "$spec" - ;;
        esac
    done
}

# ---- corpus matrix ---------------------------------------------------
# Rich native docs -> every export format
run test/wp/BillOfRights.abw            docx odt abwn html txt
run test/wp/table.abw                   docx odt rtf abwn html txt
run test/wp/Gettysburg.abw              docx odt rtf abwn txt
run test/wp/suite/chinese.abw           docx rtf abwn
run test/wp/Latin1.abw                  docx abwn
run test/wp/Spelling.abw                docx abwn
run test/wp/data.abw                    docx abwn
run test/wp/toc.abw                     abwn docx
# Images: data-item count must survive
run test/wp/image_props.abw             img:docx img:odt img:abwn
run test/wp/posimage.abw                img:docx img:abwn
# ODT input incl. tracked changes
run test/wp/odt/trackedchanges/document.odt  odt docx abwn
# Binary .doc input (wv) incl. .doc (RTF-hack) export leg
run test/wp/Word97Test.doc              doc rtf docx odt abwn
run test/wp/long_footnote.doc           docx odt abwn
# RTF input
run test/wp/bugs/836.rtf                rtf abwn
run test/wp/bugs/838.rtf                rtf abwn
run fuzz/corpus/rtf/seed_rtftest.rtf    rtf docx abwn
# Markdown input (md export leg covered by the .txt fixture below:
# md loses field values/tables so only plain docs round-trip text)
run test/wp/markdown-formatting.md      abwn txt
# Plain text input -> everything incl. markdown export
run test/auto/hello.dat.2.txt           txt docx abwn md
# DOCX input (fuzz seeds are real documents)
run fuzz/corpus/docx/seed_gettysburg.docx  docx odt rtf abwn
run fuzz/corpus/docx/seed_table.docx       docx rtf abwn
# WordPerfect input (import-only format): import -> abwn round-trip
run fuzz/corpus/wpd/seed_minimal.wpd    abwn txt
run fuzz/corpus/wpd/seed_groups.wpd     abwn
run fuzz/corpus/wpd/seed_prefixidx.wpd  abwn
# MHTML input
run fuzz/corpus/mht/seed_gettysburg.mht abwn docx

echo "rt-check: $PASS leg(s) passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
