#!/usr/bin/env python3
"""Generate built-in cover-page templates for Abinova.

Reads .abwn exports of the reference .docx cover pages and produces
paste-fragments the app can splice into a document via
IE_Imp_Abinova_1::pasteFromBuffer() (clipboard mode):

  * <section> is stripped -- frames/blocks become children of the
    destination section (its page size/margins apply).
  * the <p> carrying <pbr/> and everything after it is dropped; the
    inserter adds a conditional page break instead.
  * data items (<d>) are kept and their names are prefixed with
    "cover-<id>-" so they cannot collide with document data items;
    dataid="..."/strux-image-dataid="..." refs are rewritten.
  * xid attributes are stripped (they must be unique per document).
  * the two styles the covers use are emitted renamed to
    cover_Normal / cover_NoSpacing and style="" refs rewritten, so
    they can never mutate the document's own styles.

Usage: tools/mkcovers.py <srcdir> <outdir>
  srcdir: directory containing <id>.abwn template exports
  outdir: where <id>.xml fragments + thumbs/<id>.png are written
          (also writes thumbs only if 'abinova' + pdftoppm are usable)

Template <id>s must match the preset ids used by the ribbon gallery
(see s_coverPresets in src/text/fmt/xp/fv_View_cmd.cpp).
"""

import base64
import os
import re
import subprocess
import sys

STYLE_MAP = {
    "_Normal": "cover_Normal",
    "No Spacing": "cover_NoSpacing",
}

# style defs we ship (kept minimal & identical across all templates)
STYLE_DEFS = {
    "cover_Normal": '<s name="cover_Normal" type="P" basedon="Normal"/>',
    "cover_NoSpacing": '<s name="cover_NoSpacing" type="P" '
                      'basedon="Normal" '
                      'props="lang:en-US; font-size:11.000000pt; '
                      'char-kern:0.000000pt"/>',
}

XMLNS = (
    ' version="4.0.0" xml:space="preserve"'
    ' xmlns:xlink="http://www.w3.org/1999/xlink"'
    ' xmlns:svg="http://www.w3.org/2000/svg"'
    ' xmlns:fo="http://www.w3.org/1999/XSL/Format"'
    ' xmlns:math="http://www.w3.org/1998/Math/MathML"'
    ' xmlns:dc="http://purl.org/dc/elements/1.1/"'
    ' xmlns:awml="http://www.abisource.com/awml.dtd"'
)


def extract(abwn_path, ident):
    src = open(abwn_path, encoding="utf-8").read()

    # --- data items -------------------------------------------------
    data_sec = ""
    m = re.search(r"(?s)<data>(.*?)</data>", src)
    if m:
        items = re.findall(r"(?s)<d [^>]*>.*?</d>", m.group(1))
        for it in items:
            nm = re.search(r'\bname="([^"]+)"', it)
            if nm:
                it = it.replace(
                    'name="%s"' % nm.group(1),
                    'name="cover-%s-%s"' % (ident, nm.group(1)), 1)
            data_sec += "    " + it.strip() + "\n"

    # --- section children -------------------------------------------
    sec = re.search(r"(?s)<section\b[^>]*>(.*?)</section>", src)
    if not sec:
        raise SystemExit("%s: no <section>" % abwn_path)
    body = sec.group(1)

    # Drop the FIRST TOP-LEVEL <p> carrying <pbr/> and everything
    # after it (the page-2 body anchor).  Nested pbr paragraphs
    # (e.g. inside a frame's table cell) belong to the design.
    cut = None
    depth = 0
    pos = 0
    for m in re.finditer(
            r"<(/?)(frame|table|cell|p|section)\b([^>]*)>|</(frame|table|cell|p|section)>",
            body):
        tok = m.group(0)
        if tok.startswith("</"):
            depth = max(0, depth - 1)
            continue
        name = m.group(2)
        selfclose = m.group(3).rstrip().endswith("/")
        if name == "p" and depth == 0:
            # find the matching </p>
            if selfclose:
                seg_end = m.end()
                seg = body[m.start():seg_end]
            else:
                close = body.find("</p>", m.end())
                seg_end = close + 4 if close >= 0 else m.end()
                seg = body[m.start():seg_end]
            if "<pbr" in seg:
                cut = m.start()
                break
        if not selfclose and name in ("frame", "table", "cell"):
            depth += 1
    if cut is not None:
        body = body[:cut]

    # strip xid attributes (they must be unique per document)
    body = re.sub(r'\s+xid="[^"]*"', "", body)

    # rename style refs
    used_styles = set()
    for old, new in STYLE_MAP.items():
        if re.search(r'style="%s"' % re.escape(old), body):
            used_styles.add(new)
            body = re.sub(r'style="%s"' % re.escape(old),
                          'style="%s"' % new, body)

    # rename data item refs
    for rid in set(re.findall(r'rId\d+', body)):
        body = body.replace('dataid="%s"' % rid,
                            'dataid="cover-%s-%s"' % (ident, rid))
        body = body.replace('strux-image-dataid="%s"' % rid,
                            'strux-image-dataid="cover-%s-%s"'
                            % (ident, rid))
        body = body.replace('dataid:%s' % rid,
                            'dataid:cover-%s-%s' % (ident, rid))

    # sanity: warn about references we don't rewrite
    for m2 in re.finditer(r'(xlink:href|name)="([^"]+)"', body):
        if m2.group(2).startswith("cover-"):
            continue
        if m2.group(2) and not m2.group(2).startswith("http"):
            print("  note: %s has ref %s=%s"
                  % (os.path.basename(abwn_path), m2.group(1),
                     m2.group(2)))

    styles_xml = ""
    if used_styles:
        styles_xml = "<styles>\n"
        for s in sorted(used_styles):
            styles_xml += "    " + STYLE_DEFS[s] + "\n"
        styles_xml += "</styles>\n"

    data_xml = ""
    if data_sec:
        data_xml = "<data>\n" + data_sec + "</data>\n"

    frag = ("<abinova" + XMLNS + ">\n" + styles_xml + data_xml +
            body.strip() + "\n</abinova>\n")
    return frag


def render_thumb(abwn_path, out_png, bindir=None):
    """Render page 1 of the source to a small PNG."""
    pdf = "/tmp/mkcovers_%s.pdf" % os.path.basename(abwn_path)
    abinova = os.path.join(bindir, "abinova") if bindir else "abinova"
    try:
        subprocess.run([abinova, "--to=pdf", abwn_path, "-o", pdf],
                       check=True, capture_output=True, timeout=60)
        subprocess.run(["pdftoppm", "-f", "1", "-l", "1", "-png",
                        "-r", "96", pdf, "/tmp/mkcovers_thumb"],
                       check=True, capture_output=True, timeout=30)
        tmp_png = "/tmp/mkcovers_thumb-1.png"
        # scale to 285x402 (3x the 95x134 card)
        from PIL import Image
        im = Image.open(tmp_png)
        im.thumbnail((285, 402), Image.LANCZOS)
        im.save(out_png)
        os.unlink(tmp_png)
        os.unlink(pdf)
        return True
    except Exception as e:
        print("  thumb render failed for %s: %s"
              % (abwn_path, e))
        return False


def main():
    srcdir = sys.argv[1] if len(sys.argv) > 1 else "/tmp/covers"
    outdir = sys.argv[2] if len(sys.argv) > 2 else \
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     "..", "src", "wp", "covers")
    bindir = sys.argv[3] if len(sys.argv) > 3 else None
    thumbs = os.path.join(outdir, "thumbs")
    os.makedirs(thumbs, exist_ok=True)

    # source file name -> gallery preset id (see s_coverPresets)
    ID_MAP = {
        "filgree": "filigree",
        "headiness": "headline",
        "whip": "whisp",
    }

    for fn in sorted(os.listdir(srcdir)):
        if not fn.endswith(".abwn"):
            continue
        stem = fn[:-5].lower()
        ident = ID_MAP.get(stem, stem.replace("-", ""))
        frag = extract(os.path.join(srcdir, fn), ident)
        outp = os.path.join(outdir, ident + ".xml")
        open(outp, "w", encoding="utf-8").write(frag)
        print("wrote %s (%d bytes)" % (outp, len(frag)))
        render_thumb(os.path.join(srcdir, fn),
                     os.path.join(thumbs, ident + ".png"), bindir)


if __name__ == "__main__":
    main()
