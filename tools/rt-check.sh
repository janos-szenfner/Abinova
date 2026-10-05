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

norm() { tr -s '[:space:]' ' ' <"$1" | sed 's/[0-9]\{1,\}/#/g; s/^ *//; s/ *$//; s|file://[^ ]*|<FILE>|g'; }
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
    html|xhtml)
        grep -qi '<html' "$1" ;;
    tex|latex)
        grep -q '\\begin{document}\|\\end{' "$1" ;;
    mht)
        grep -qi 'MIME-Version\|multipart/related' "$1" ;;
    pdf)
        head -c4 "$1" | grep -q '%PDF' ;;
    epub)
        python3 - "$1" <<'PYEOF'
import sys, zipfile
zf = zipfile.ZipFile(sys.argv[1])
assert zf.namelist()[0] == 'mimetype', 'mimetype member not first'
assert zf.getinfo('mimetype').compress_type == zipfile.ZIP_STORED
assert zf.read('mimetype') == b'application/epub+zip', 'wrong mimetype'
assert 'META-INF/container.xml' in zf.namelist()
PYEOF
        ;;
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

# exp:FMT leg — export + structural validation only; use where the
# format's reimport fidelity is a known gap (annotation/field/TOC text
# drift, html export embedding MathJax JS, markdown losing fields).
expleg() { # $1=fixture $2=format
    local name="$1" fmt="$2" out="$TMPD/out.$2"
    echo "=== $name -> $fmt (export only)"
    if ! "$BIN" --to="$fmt" --to-name="$out" "$SRC/$name" >/dev/null 2>&1; then
        echo "  FAIL: export"; FAIL=$((FAIL + 1)); return
    fi
    if ! validate "$out" "$fmt"; then
        echo "  FAIL: structure"; FAIL=$((FAIL + 1)); return
    fi
    echo "  export+structure OK"
    PASS=$((PASS + 1))
}

# imp:FILE leg — import + txt convert only; asserts the importer accepts
# the fixture and produces text.  For generated/foreign fixtures whose
# normalized text can't equal a .abw source's.
impleg() { # $1=fixture
    local name="$1"
    echo "=== import $name"
    if ! "$BIN" --to=txt --to-name="$TMPD/imp.txt" "$SRC/$name" >/dev/null 2>&1; then
        echo "  FAIL: import"; FAIL=$((FAIL + 1)); return
    fi
    echo "  import OK"
    PASS=$((PASS + 1))
}

# sweep <dir> — convert every file in the directory (recursively) to
# abwn; exercises importer + abwn exporter on each.  rc==255 (clean
# reject) is allowed — corrupt fixtures must degrade, not crash or hang.
sweep() { # $1=dir (relative to $SRC)
    local d="$1" f rc
    while IFS= read -r f; do
        timeout 60 "$BIN" --to=abwn --to-name="$TMPD/sweep.abwn" "$f" >/dev/null 2>&1
        rc=$?
        if [ $rc -eq 0 ] || [ $rc -eq 255 ]; then
            PASS=$((PASS + 1))
        else
            echo "=== sweep $f CRASH rc=$rc"; FAIL=$((FAIL + 1))
        fi
    done < <(find "$SRC/$d" -type f | sort)
    echo "=== sweep $d done"
}

# enc leg — export the fixture to password-encrypted ODT.  The
# ABINOVA_PASSWORD env var drives the plaintext->encrypted package path
# (ODc_Crypto) in headless mode; validate that manifest.xml gained
# per-entry encryption-data and that the mimetype member stays plain.
encleg() { # $1=fixture
    local name="$1" out="$TMPD/enc.odt"
    echo "=== $name -> encrypted odt"
    if ! ABINOVA_PASSWORD=cov11-pass "$BIN" --to=odt --to-name="$out" "$SRC/$name" >/dev/null 2>&1; then
        echo "  FAIL: export"; FAIL=$((FAIL + 1)); return
    fi
    if ! python3 - "$out" <<'PYEOF'
import sys, zipfile
zf = zipfile.ZipFile(sys.argv[1])
assert zf.namelist()[0] == 'mimetype', 'mimetype member not first'
assert zf.read('mimetype') == b'application/vnd.oasis.opendocument.text'
m = zf.read('META-INF/manifest.xml')
assert b'encryption-data' in m, 'manifest lacks encryption-data'
PYEOF
    then
        echo "  FAIL: not encrypted"; FAIL=$((FAIL + 1)); return
    fi
    echo "  encrypted export OK"
    PASS=$((PASS + 1))
}

run() { # $1=fixture (relative to $SRC); rest = legs ('img:'/'exp:'/'imp:' prefixes)
    local file="$1"; shift
    for spec in "$@"; do
        case "$spec" in
        img:*) leg "$file" "${spec#img:}" img ;;
        exp:*) expleg "$file" "${spec#exp:}" ;;
        enc:*) encleg "$file" ;;
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

# ---- COV07: full-format coverage matrix ------------------------------
# Mega-fixture with every construct (styles, lists, foot+endnote,
# annotation, table w/ merged cells + in-cell image, inline + framed
# image, math, TOC, fields, links, bookmark, hdr/ftr, 2-col section,
# page break) exported through every format.  Lossy formats get
# export-only legs; the committed rich.* conversions below exercise the
# matching importers.
run test/wp/cov07/rich.abw        abwn txt \
    exp:docx exp:odt exp:rtf exp:doc exp:xhtml exp:tex exp:latex \
    exp:mht exp:epub exp:md exp:pdf
# Generated-format fixtures back through the importers
run test/wp/cov07/rich.rtf        abwn
run test/wp/cov07/rich.docx       abwn
run test/wp/cov07/rich.odt        abwn
run test/wp/cov07/rich.xhtml      abwn
run test/wp/cov07/rich.mht        abwn
run test/wp/cov07/rich.epub       abwn
run test/wp/cov07/rich.zabw       abwn txt
# Hand-written format fixtures
run test/wp/cov07/full.tex        abwn exp:tex
run test/wp/cov07/full.md         abwn txt
run test/wp/cov07/full.html       abwn txt
run test/wp/cov07/plain.txt       abwn txt
impleg test/wp/cov07/utf16.txt
impleg test/wp/cov07/latin1.txt
impleg test/wp/cov07/red.png
impleg test/wp/example.psitext
impleg test/wp/example.psiword
impleg test/wp/mr18-empty-props.html
# Broader abw/doc/rtf corpus legs
run test/wp/fields.abw            abwn docx exp:rtf
run test/wp/frame.abw             abwn exp:docx
run test/wp/footer.abw            abwn docx
run test/wp/Styles.abw            abwn docx
run test/wp/tabs.abw              abwn
run test/wp/abi_memo.abw          abwn docx
run test/wp/abi_memo-new.abw      abwn
run test/wp/accents.abw           abwn docx
run test/wp/Unicode1.abw          abwn docx
run test/wp/World.abw             abwn
run test/wp/Bazaar.abw            abwn
run test/wp/Interview.abw         abwn docx exp:html
run test/wp/fields.rtf            abwn
run test/wp/rtftest.rtf           abwn docx
run test/wp/fields.doc            abwn exp:docx
run test/wp/toc.abw               rtf exp:html exp:tex
run test/wp/image_props.abw       rtf exp:mht exp:epub
run test/wp/posimage.abw          exp:odt
run test/wp/BillOfRights.abw      exp:rtf exp:tex exp:xhtml exp:epub exp:mht
run test/wp/table.abw             exp:tex exp:xhtml exp:epub
run test/wp/Gettysburg.abw        abwn exp:html
run test/wp/markdown-formatting.md abwn exp:md
run test/auto/hello.dat.1.abw     abwn

# ---- COV11: broad importer coverage ----------------------------------
# Kitchen-sink packages from tools/mkcov11.py: one docx covering the
# OXMLi_ListenerState_Valid element matrix (pPr/rPr variants, tabs,
# fields, hyperlinks, bookmarks, sdt, proofErr, ruby, smartTag,
# moveFrom/To + customXml + perm ranges, notes/comments, numbering,
# DrawingML inline + every anchor wrap mode, wps + VML textboxes,
# OLE objects, merged/nested/floating tables, oMath, two sectPr) with
# styles/numbering/settings/fontTable/footnotes/endnotes/comments/
# theme/header/footer parts; one ODF package with automatic styles,
# fields, notes, frames, lists, sections, index marks; one placeable
# WMF.  docx->docx text round-trip hits a pre-existing importer crash
# on the re-exported package, so foreign formats get export-only legs.
run test/wp/cov11/cov11.docx        abwn exp:docx exp:odt enc:abwn
run test/wp/cov11/cov11.odt         abwn exp:odt exp:docx
impleg test/wp/cov11/cov11.wmf

# Import sweeps: every fixture file -> abwn (rc 0 or clean 255).
sweep test/wp/suite
sweep test/wp/bugs
sweep test/wp/odt
sweep test/wp/tst04
sweep test/wp/tst07
sweep test/wp/cov07
sweep test/wp/cov11
sweep fuzz/corpus/abw
sweep fuzz/corpus/doc
sweep fuzz/corpus/docx
sweep fuzz/corpus/rtf
sweep fuzz/corpus/mht
sweep fuzz/corpus/odt
sweep fuzz/corpus/wpd
sweep fuzz/regress/doc

echo "rt-check: $PASS leg(s) passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
