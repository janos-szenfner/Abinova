#!/usr/bin/env python3
"""Generate the built-in Table Style recipe table for Abinova.

Reads a Word 'styles.xml' (e.g. extracted from a default docx template:
  unzip -o default.docx word/styles.xml)
and emits a C++ file with a static recipe array consumed by
fl_TableStyles.cpp (struct FV_TableStyleBuiltin).

Theme colours are kept symbolic as "theme:<name>[:tNN][:sNN]" tokens
(t = blend toward white NN%, s = darken NN%) so a document theme
change can recolour styles; the engine resolves them at apply time.

Usage:
  gen_table_styles.py styles.xml > fl_TableStylesBuiltin.cpp
"""
import re
import sys

# OOXML part name -> FV_TableStylePart member
PART = {
    "firstRow":  "FV_TSP_FirstRow",
    "lastRow":   "FV_TSP_LastRow",
    "firstCol":  "FV_TSP_FirstCol",
    "lastCol":   "FV_TSP_LastCol",
    "band1Horz": "FV_TSP_Band1H",
    "band2Horz": "FV_TSP_Band2H",
    "band1Vert": "FV_TSP_Band1V",
    "band2Vert": "FV_TSP_Band2V",
}

BORDER_STYLE = {
    "single": "solid", "dashed": "dashed", "dotted": "dotted",
    "none": "none", "nil": "none",
    # renderer has no double/wave/thick - degrade to solid
    "double": "solid", "thick": "solid", "triple": "solid",
    "wave": "solid", "thickThinMediumGap": "solid",
    "thinThickMediumGap": "solid", "thickThinSmallGap": "solid",
    "thinThickSmallGap": "solid", "dashDotStroked": "dashed",
    "threeDEmboss": "solid", "threeDEngrave": "solid",
    "outset": "solid", "inset": "solid",
    "dashSmallGap": "dashed", "dotDash": "dashed", "dotDotDash": "dotted",
}

SIDES = {"top": "top", "bottom": "bot", "left": "left", "right": "right",
         "insideH": "insideh", "insideV": "insidev"}


def attr(node, name):
    m = re.search(r'w:%s="([^"]*)"' % name, node)
    return m.group(1) if m else None


def color_token(node):
    """border/rPr colour -> hex or theme token."""
    theme = attr(node, "themeColor")
    if theme:
        tok = "theme:" + theme
        t = attr(node, "themeTint")
        s = attr(node, "themeShade")
        if t:
            tok += ":t%d" % round(int(t, 16) / 2.55)
        if s:
            tok += ":s%d" % round(int(s, 16) / 2.55)
        return tok
    return attr(node, "color") or "auto"


def border_props(tcPr):
    out = []
    for side, ours in SIDES.items():
        m = re.search(r'<w:%s\b[^>]*>' % side, tcPr)
        if not m:
            continue
        b = m.group(0)
        val = BORDER_STYLE.get(attr(b, "val") or "none", "solid")
        sz = attr(b, "sz")
        thick = "%.2fpt" % (int(sz) / 8.0) if sz else "0.5pt"
        col = color_token(b)
        out += ["%s-style:%s" % (ours, val),
                "%s-color:%s" % (ours, col),
                "%s-thickness:%s" % (ours, thick)]
    return out


def accent_of(sid):
    """'...-AccentN' -> N (1-6) else None."""
    m = re.search(r"-Accent([1-6])$", sid)
    return m.group(1) if m else None


def tc_props(tcPr, sid=None):
    out = []
    if "<w:tcBorders>" in tcPr:
        out += border_props(tcPr)
    m = re.search(r'<w:shd\b[^>]*>', tcPr)
    if m:
        s = m.group(0)
        theme = attr(s, "themeFill")
        if theme:
            tok = "theme:" + theme
            t = attr(s, "themeFillTint")
            sh = attr(s, "themeFillShade")
            if t:
                tok += ":t%d" % round(int(t, 16) / 2.55)
            if sh:
                tok += ":s%d" % round(int(sh, 16) / 2.55)
            acc = accent_of(sid or "")
            if theme == "text1" and not t:
                # bare/shaded text1 is pure black, which the gallery
                # must not use: accent variants take their dark
                # shade instead, neutral styles take dark gray
                # (a t85 tint resolves to #262626)
                if acc:
                    tok = "theme:accent%s:s50" % acc
                else:
                    tok = "theme:text1:t85"
            out.append("background-color:" + tok)
        else:
            fill = attr(s, "fill")
            if fill and fill != "auto":
                out.append("background-color:" + fill)
    return out


def rPr_props(rPr):
    out = []
    if re.search(r'<w:b\s*/>|<w:b\s>', rPr):
        out.append("font-weight:bold")
    if re.search(r'<w:i\s*/>|<w:i\s>', rPr):
        out.append("font-style:italic")
    m = re.search(r'<w:color\b[^>]*>', rPr)
    if m:
        tok = color_token(m.group(0))
        if tok != "auto":
            out.append("color:" + tok)
    return out


# Office theme palette used to judge whether a fill needs light text
_PALETTE = {
    "accent1": "4472C4", "accent2": "ED7D31", "accent3": "A5A5A5",
    "accent4": "FFC000", "accent5": "5B9BD5", "accent6": "70AD47",
    "text1": "000000", "text2": "44546A",
    "background1": "FFFFFF", "background2": "E7E6E6",
}


def _resolve(tok):
    """theme:name[:tNN][:sNN] -> (r,g,b); None when unresolvable."""
    if not tok or tok in ("auto", "transparent"):
        return None
    if tok.startswith("theme:"):
        parts = tok[6:].split(":")
        base = _PALETTE.get(parts[0])
        if not base:
            return None
        r, g, b = (int(base[i:i + 2], 16) for i in (0, 2, 4))
        for mod in parts[1:]:
            v = float(mod[1:]) / 100.0
            if mod[0] == "t":   # keep NN%, rest toward white
                r, g, b = (c + (255 - c) * (1 - v) for c in (r, g, b))
            elif mod[0] == "s":  # keep NN% (scale toward black)
                r, g, b = (c * v for c in (r, g, b))
        return int(r + .5), int(g + .5), int(b + .5)
    if re.match(r"^[0-9A-Fa-f]{6}$", tok):
        return tuple(int(tok[i:i + 2], 16) for i in (0, 2, 4))
    return None


def ensure_readable_text(cell, char):
    """Word puts light text on dark fills via its rPr; some builtin
    recipes leave the colour implicit.  When a part's fill resolves
    dark and no text colour is set, fall back to background1 (white)
    so the text stays legible."""
    if char and re.search(r"\bcolor:", char):
        return char
    dark = False
    for bg in re.findall(r"background-color:([^;\s]+)", cell or ""):
        c = _resolve(bg)
        if c and (0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]) < 140:
            dark = True
    if dark:
        return (char + "; " if char else "") + "color:theme:background1"
    return char


# styles the modern Word gallery shows: Table Grid, Plain Table 1-5,
# Grid Table 1 Light - 7 Colorful and List Table 1 Light - 7 Colorful
# (neutral + 6 accents each).  The older Light/Medium/Dark/Colorful
# families and the Word-2003 "Table*" styles stay registered for
# compatibility but are hidden from the gallery.
_GALLERY_ID = re.compile(r"^(TableGrid$|PlainTable|GridTable|ListTable)")


def style_family(style_id, name):
    n = name.lower()
    if "list" in n:
        return "FV_TSF_List"
    if "plain" in n or n in ("table grid", "no style, no grid"):
        return "FV_TSF_Plain"
    return "FV_TSF_Grid"


def main(path):
    xml = open(path, encoding="utf8").read()
    styles = re.findall(
        r'<w:style\b[^>]*w:type="table"[^>]*>(.*?)</w:style>', xml, re.S)
    head = re.findall(
        r'<w:style\b[^>]*w:type="table"[^>]*>', xml)

    print("/* generated by tools/dev/gen_table_styles.py - do not edit */")
    print('#include "fl_TableStyles.h"\n')
    print("extern const FV_TableStyleBuiltin s_tableStyleBuiltin[] = {")
    hidden = []

    n = 0
    for hdr, body in zip(head, styles):
        sid = re.search(r'w:styleId="([^"]*)"', hdr).group(1)
        nm = re.search(r'<w:name w:val="([^"]*)"', body).group(1)
        if sid == "TableNormal":
            continue            # invisible base style
        fam = style_family(sid, nm)

        parts = []
        # whole-table part: tblPr/tcPr/rPr at style level (the
        # conditional tblStylePr blocks are excluded first so their
        # tcPr/rPr don't leak into the whole-table defaults)
        whole = re.sub(r"<w:tblStylePr\b.*?</w:tblStylePr>", "",
                       body, flags=re.S)
        wp, wch = [], []
        tp = re.search(r'<w:tblPr>(.*?)</w:tblPr>', whole, re.S)
        if tp and "<w:tblBorders>" in tp.group(1):
            wb = re.sub(r"<w:tblBorders>|</w:tblBorders>", "",
                        re.search(r"<w:tblBorders>.*?</w:tblBorders>",
                                  tp.group(1), re.S).group(0))
            wp = border_props(wb)
        tc = re.search(r"<w:tcPr>(.*?)</w:tcPr>", whole, re.S)
        if tc:
            wp += tc_props(tc.group(1), sid)
        rp = re.search(r"<w:rPr>(.*?)</w:rPr>", whole, re.S)
        if rp:
            wch = rPr_props(rp.group(1))
        if wp or wch:
            parts.append(("FV_TSP_Whole", "; ".join(wp),
                          ensure_readable_text("; ".join(wp),
                                               "; ".join(wch))))

        for pm in re.finditer(
                r'<w:tblStylePr w:type="([^"]*)">(.*?)</w:tblStylePr>',
                body, re.S):
            ptype, pbody = pm.group(1), pm.group(2)
            if ptype not in PART:
                continue
            cell = []
            tc = re.search(r"<w:tcPr>(.*?)</w:tcPr>", pbody, re.S)
            if tc:
                cell = tc_props(tc.group(1), sid)
            char = []
            rp = re.search(r"<w:rPr>(.*?)</w:rPr>", pbody, re.S)
            if rp:
                char = rPr_props(rp.group(1))
            if cell or char:
                parts.append((PART[ptype], "; ".join(cell),
                              ensure_readable_text("; ".join(cell),
                                                   "; ".join(char))))

        if not parts:
            continue
        if not _GALLERY_ID.match(sid):
            hidden.append((sid, nm, fam, parts))
            continue
        print('\t{ "%s", "%s", %s, %d, {' % (sid, nm, fam, len(parts)))
        for p, cell, char in parts:
            print('\t\t{ %s, "%s", "%s" },' % (p, cell, char))
        print("\t} },")
        n += 1
    print("\t{ nullptr, nullptr, FV_TSF_Plain, 0, {} }\n};")
    sys.stderr.write("%d gallery styles emitted\n" % n)

    print("\n/* registered but hidden from the gallery (legacy / older-")
    print(" * generation styles kept for document compatibility) */")
    print("extern const FV_TableStyleBuiltin s_tableStyleBuiltinHidden[] = {")
    for sid, nm, fam, parts in hidden:
        print('\t{ "%s", "%s", %s, %d, {' % (sid, nm, fam, len(parts)))
        for p, cell, char in parts:
            print('\t\t{ %s, "%s", "%s" },' % (p, cell, char))
        print("\t} },")
    print("\t{ nullptr, nullptr, FV_TSF_Plain, 0, {} }\n};")
    sys.stderr.write("%d hidden styles emitted\n" % len(hidden))


if __name__ == "__main__":
    main(sys.argv[1])
