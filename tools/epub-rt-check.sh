#!/bin/bash
# epub-rt-check.sh — EPUB roundtrip + package validation for Abinova.
#
# Usage: tools/epub-rt-check.sh <abinova-binary> <input-doc> [more docs...]
#
# For each document:
#   1. doc -> epub export
#   2. structural validation (no epubcheck needed): mimetype first+stored,
#      every member XML well-formed, container -> OPF resolves, manifest
#      covers all members, spine idrefs resolve, NCX dtb:uid matches the
#      OPF unique identifier, all internal hrefs exist
#   3. epub -> abwn reimport, then compare normalized text and count
#      image data items in both directions
#
# Exit 0 if every document passes, 1 otherwise.

set -u
BIN="$1"; shift
TMPD=$(mktemp -d)
trap 'rm -rf "$TMPD"' EXIT
FAIL=0

norm() { tr -s '[:space:]' ' ' < "$1" | tr -d '\n' | sed -E 's/[0-9]{1,2}:[0-9]{2}:[0-9]{2}/TIME/g;s/^ *//;s/ *$//'; }
imgcount() { grep -o 'mime-type="image/' "$1" 2>/dev/null | wc -l; }

for SRC in "$@"; do
    EPUB="$TMPD/$(basename "$SRC").epub"
    echo "=== $SRC"
    "$BIN" --to=epub --to-name="$EPUB" "$SRC" -e "split-level:2" || { echo "  export FAIL"; FAIL=1; continue; }

    python3 - "$EPUB" <<'PYEOF'
import sys, zipfile, xml.dom.minidom
zf = zipfile.ZipFile(sys.argv[1])
names = zf.namelist()
errs = []
# mimetype first and stored
if names[0] != 'mimetype' or zf.read('mimetype') != b'application/epub+zip':
    errs.append('mimetype not first/wrong')
elif zf.getinfo('mimetype').compress_type != zipfile.ZIP_STORED:
    errs.append('mimetype compressed')
# all XML members well-formed
docs = {}
for n in names:
    if n.endswith(('.xml', '.xhtml', '.opf', '.ncx')):
        try: docs[n] = xml.dom.minidom.parseString(zf.read(n))
        except Exception as e: errs.append('%s: %s' % (n, e))
# container -> opf
rootfile = docs['META-INF/container.xml'].getElementsByTagName('rootfile')[0].getAttribute('full-path')
if rootfile not in names: errs.append('rootfile missing: ' + rootfile)
opf = docs[rootfile]
import posixpath
opfdir = posixpath.dirname(rootfile)
# manifest covers all members (minus mimetype/container)
items = {}
for it in opf.getElementsByTagName('item'):
    items[it.getAttribute('id')] = (it.getAttribute('href'), it.getAttribute('media-type'))
for n in names:
    if n in ('mimetype', 'META-INF/container.xml', rootfile): continue
    rel = n[len(opfdir)+1:] if opfdir else n
    if rel not in [h for h, _ in items.values()]:
        errs.append('not in manifest: ' + n)
# spine resolves
for ir in opf.getElementsByTagName('itemref'):
    if ir.getAttribute('idref') not in items:
        errs.append('spine idref unresolved: ' + ir.getAttribute('idref'))
# ncx uid == opf identifier
uid = opf.documentElement.getAttribute('unique-identifier')
idc = [e.firstChild.data for e in opf.getElementsByTagName('dc:identifier') if e.getAttribute('id') == uid]
ncxu = [m.getAttribute('content') for m in docs[opfdir + '/toc.ncx'].getElementsByTagName('meta') if m.getAttribute('name') == 'dtb:uid']
if idc and ncxu and idc[0] != ncxu[0]: errs.append('dtb:uid != opf identifier')
# internal hrefs exist
for n, d in docs.items():
    if not n.endswith('.xhtml'): continue
    base = posixpath.dirname(n)
    for tag in ('img', 'a', 'link'):
        for e in d.getElementsByTagName(tag):
            h = e.getAttribute('src' if tag == 'img' else 'href')
            if not h or h.startswith(('http', 'mailto:', '#')): continue
            t = posixpath.normpath(posixpath.join(base, h.split('#')[0]))
            if t not in names: errs.append('%s: dangling %s' % (n, h))
print('  package: ' + ('OK (%d members, %d spine)' % (len(names), len(opf.getElementsByTagName('itemref'))) if not errs else 'FAIL ' + '; '.join(errs)))
sys.exit(1 if errs else 0)
PYEOF
    [ $? -ne 0 ] && { FAIL=1; continue; }

    RT="$TMPD/rt.abwn"; TXT_S="$TMPD/s.txt"; TXT_R="$TMPD/r.txt"
    "$BIN" --to=txt --to-name="$TXT_S" "$SRC"
    "$BIN" --to=abwn --to-name="$RT" "$EPUB" || { echo "  reimport FAIL"; FAIL=1; continue; }
    "$BIN" --to=txt --to-name="$TXT_R" "$EPUB"
    [ "$(norm "$TXT_S")" = "$(norm "$TXT_R")" ] && echo "  text: OK" || { echo "  text: DIFF"; diff <(norm "$TXT_S") <(norm "$TXT_R") | head -5; FAIL=1; }
    echo "  images: $(imgcount "$RT") data items in reimport"
done
exit $FAIL
