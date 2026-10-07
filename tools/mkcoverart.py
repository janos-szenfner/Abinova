#!/usr/bin/env python3
"""Generate the cover-page artwork bundled into fv_CoverAssets.cpp.

The Word cover originals ship embedded raster art that LIC01's license
audit found non-redistributable, so this script paints ORIGINAL art
(GPL-2.0+, same license as Abinova) in the same spirit: a page of light
feather plumes for "feathered", two flourish ornaments for "filgree", a
stylized cypress-road landscape for "integral" and a translucent white
facet texture for "facet".  All drawing is deterministic (fixed seeds)
so the committed output is reproducible.

Outputs:
  src/wp/ap/gtk/covers/*.png|jpg   the art, kept in-tree for inspection
  src/text/fmt/xp/fv_CoverAssets.cpp  the same bytes as C arrays the
                                    cover code registers as document
                                    data items (frame strux-image-dataid
                                    picture fills)

Run:  python3 tools/mkcoverart.py
"""

import io
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "src", "wp", "ap", "gtk", "covers")
OUT_CPP = os.path.join(ROOT, "src", "text", "fmt", "xp", "fv_CoverAssets.cpp")

SS = 4  # supersample factor for anti-aliased strokes


# ----------------------------------------------------------------- helpers

def qbez(p0, p1, p2, t):
    """Quadratic bezier point."""
    u = 1.0 - t
    return (u * u * p0[0] + 2 * u * t * p1[0] + t * t * p2[0],
            u * u * p0[1] + 2 * u * t * p1[1] + t * t * p2[1])


def qbez_tan(p0, p1, p2, t):
    """Quadratic bezier tangent (unnormalized)."""
    return (2 * (1 - t) * (p1[0] - p0[0]) + 2 * t * (p2[0] - p1[0]),
            2 * (1 - t) * (p1[1] - p0[1]) + 2 * t * (p2[1] - p1[1]))


def tapered_stroke(draw, pts, r0, r1, color):
    """Paint a tapered stroke as a smooth polygon between two edge
    curves offset along the path normals."""
    n = len(pts)
    left, right = [], []
    for i, (x, y) in enumerate(pts):
        j = min(i + 1, n - 1)
        k = max(i - 1, 0)
        tx, ty = pts[j][0] - pts[k][0], pts[j][1] - pts[k][1]
        tl = math.hypot(tx, ty) or 1.0
        nx, ny = -ty / tl, tx / tl
        r = r0 + (r1 - r0) * (i / max(1, n - 1))
        left.append((x + nx * r, y + ny * r))
        right.append((x - nx * r, y - ny * r))
    draw.polygon(left + right[::-1], fill=color)


def leaf(draw, tip, base, width, color):
    """A pointed leaf between base and tip, bulged on both sides."""
    dx, dy = tip[0] - base[0], tip[1] - base[1]
    L = math.hypot(dx, dy) or 1.0
    ux, uy = dx / L, dy / L
    px, py = -uy, ux
    pts = []
    for i in range(13):
        t = i / 12.0
        w = width * math.sin(math.pi * t) ** 0.7
        pts.append((base[0] + dx * t + px * w,
                    base[1] + dy * t + py * w))
    for i in range(12, -1, -1):
        t = i / 12.0
        w = width * math.sin(math.pi * t) ** 0.7
        pts.append((base[0] + dx * t - px * w,
                    base[1] + dy * t - py * w))
    draw.polygon(pts, fill=color)


# ------------------------------------------------------------------ feathers

def draw_feather(draw, base, tip, bend, span, color, rng, wpx=2.4):
    """One plume: a gently curved shaft with drooping barbs either side.

    bend is the perpendicular offset of the shaft's control point,
    span the barb reach in pixels (at 4x scale)."""
    dx, dy = tip[0] - base[0], tip[1] - base[1]
    L = math.hypot(dx, dy) or 1.0
    ux, uy = dx / L, dy / L
    px, py = -uy, ux
    ctrl = ((base[0] + tip[0]) / 2 + px * bend,
            (base[1] + tip[1]) / 2 + py * bend)

    # shaft
    shaft = [qbez(base, ctrl, tip, i / 40.0) for i in range(41)]
    draw.line(shaft, fill=color, width=int(wpx), joint="curve")

    nbarbs = max(12, int(L / (span * 0.22)))
    for i in range(nbarbs):
        t = 0.06 + 0.92 * i / (nbarbs - 1)
        sx, sy = qbez(base, ctrl, tip, t)
        tx, ty = qbez_tan(base, ctrl, tip, t)
        tl = math.hypot(tx, ty) or 1.0
        tx, ty = tx / tl, ty / tl
        nx, ny = -ty, tx
        # leaf silhouette: barbs longest just before the tip half
        prof = math.sin(math.pi * (t ** 0.75)) ** 0.8
        bl = span * prof * (0.85 + 0.3 * rng.random())
        for sgn in (1.0, -1.0):
            # barbs lean toward the tip (~35 deg off the shaft) and droop
            bx = ux * 0.55 + sgn * nx * 0.835
            by = uy * 0.55 + sgn * ny * 0.835
            ex = sx + bx * bl
            ey = sy + by * bl
            # droop the far end downward along +y and curl the tip
            cx = sx + bx * bl * 0.55
            cy = sy + by * bl * 0.55 + bl * 0.06
            ex += ux * bl * 0.12
            ey += uy * bl * 0.12 + bl * 0.16
            seg = [qbez((sx, sy), (cx, cy), (ex, ey), j / 6.0)
                   for j in range(7)]
            a = int(190 + 60 * rng.random())
            draw.line(seg, fill=color[:3] + (a,), width=int(wpx * 0.75),
                      joint="curve")


def gen_feathers():
    """Full-page scatter of light plumes (cover 8.02 x 10.5 in)."""
    W, H = 920, 1205
    rng = random.Random(20261007)
    img = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    ink = (158, 158, 176, 255)
    # (base x,y tip x,y bend span) in final-pixel space; partial plumes
    # clip off the page edge like the original layout does
    plumes = [
        ((-20, 225), (198, 78), -35, 175),
        ((224, 35), (448, 164), -45, 165),
        ((655, -25), (535, 205), 50, 180),
        ((895, 130), (655, 275), -50, 200),
        ((935, 285), (715, 450), 35, 180),
        ((-25, 450), (190, 550), 45, 190),
        ((535, 330), (760, 550), 50, 205),
        ((205, 480), (430, 715), -50, 225),
        ((-35, 655), (130, 860), 50, 200),
        ((550, 600), (845, 775), -60, 200),
        ((155, 860), (445, 1015), 50, 225),
        ((600, 915), (895, 1025), -45, 180),
        ((-25, 1015), (260, 1145), -45, 190),
        ((450, 1070), (760, 1195), 50, 180),
    ]
    for (b, t, bend, span) in plumes:
        draw_feather(d, (b[0] * SS, b[1] * SS), (t[0] * SS, t[1] * SS),
                     bend * SS, span * SS, ink, rng, wpx=3.4)
    img = img.resize((W, H), Image.LANCZOS)
    # single-colour line art: keep it luminance+alpha (halves the file)
    # with the alpha quantized so the PNG compresses sanely
    la = img.convert("LA")
    la.putalpha(la.getchannel("A").point(lambda v: v // 16 * 16))
    return la


# ------------------------------------------------------------------ filigree

def cbez(p0, p1, p2, p3, t):
    """Cubic bezier point."""
    u = 1.0 - t
    return (u**3 * p0[0] + 3 * u * u * t * p1[0] +
            3 * u * t * t * p2[0] + t**3 * p3[0],
            u**3 * p0[1] + 3 * u * u * t * p1[1] +
            3 * u * t * t * p2[1] + t**3 * p3[1])


def spiral(cx, cy, r_out, r_in, turns, phase, mirror=1, n=48):
    """Archimedean-ish spiral polyline, outer end first."""
    pts = []
    for i in range(n):
        t = i / (n - 1.0)
        a = phase + mirror * t * turns * 2 * math.pi
        r = r_out + (r_in - r_out) * t
        pts.append((cx + r * math.cos(a), cy - r * math.sin(a)))
    return pts


def gen_filgree():
    """The large symmetric flourish (Word's is ~1.55 x 0.82 in):
    a shallow vase silhouette of two mirrored S-arms that taper into
    volute curls, with a central upright fleur and small leaflets."""
    W, H = 341, 181
    img = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    col = (148, 152, 170, 255)
    cx = W * SS / 2
    base_y = H * SS - 14 * SS
    for sgn in (1, -1):
        # arm: from the centre bottom out and up, ending below the volute
        arm = [cbez((cx, base_y),
                    (cx + sgn * 118 * SS, base_y - 8 * SS),
                    (cx + sgn * 160 * SS, base_y - 82 * SS),
                    (cx + sgn * 118 * SS, base_y - 102 * SS),
                    i / 40.0) for i in range(41)]
        tapered_stroke(d, arm, 13 * SS, 7 * SS, col)
        # volute curling inward-downward off the arm's tip
        sp = spiral(cx + sgn * 116 * SS, base_y - 118 * SS,
                    38 * SS, 6 * SS, 1.45, -0.5 * math.pi,
                    mirror=-sgn)
        tapered_stroke(d, sp, 8 * SS, 2.5 * SS, col)
        # small leaflet tucked under the arm mid-way
        leaf(d, (cx + sgn * 112 * SS, base_y - 30 * SS),
             (cx + sgn * 142 * SS, base_y - 66 * SS), 10 * SS, col)
    # central upright: stem + fleur teardrop
    tapered_stroke(d, [(cx, base_y - i * SS * 2.6) for i in range(24)],
                   11 * SS, 5 * SS, col)
    leaf(d, (cx, base_y - 56 * SS), (cx, base_y - 108 * SS),
         13 * SS, col)
    # flanking beads
    for sgn in (1, -1):
        d.ellipse([cx + sgn * 30 * SS - 5 * SS, base_y - 52 * SS,
                   cx + sgn * 30 * SS + 5 * SS, base_y - 42 * SS],
                  fill=col)
    img = img.resize((W, H), Image.LANCZOS)
    return img


def gen_filgree_small():
    """The little sprig ornament (~0.83 x 0.52 in): a centre fleur with
    two short curling arms."""
    W, H = 183, 115
    img = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    col = (148, 152, 170, 255)
    cx = W * SS / 2
    base_y = H * SS - 10 * SS
    # stem
    tapered_stroke(d, [(cx, base_y - i * SS * 2.0) for i in range(20)],
                   7 * SS, 4 * SS, col)
    # centre fleur
    leaf(d, (cx, base_y - 34 * SS), (cx, base_y - 78 * SS), 10 * SS, col)
    # two short arms sweeping out and up with tiny volutes
    for sgn in (1, -1):
        arm = [cbez((cx, base_y - 12 * SS),
                    (cx + sgn * 55 * SS, base_y - 14 * SS),
                    (cx + sgn * 68 * SS, base_y - 52 * SS),
                    (cx + sgn * 46 * SS, base_y - 60 * SS),
                    i / 24.0) for i in range(25)]
        tapered_stroke(d, arm, 6 * SS, 3.5 * SS, col)
        sp = spiral(cx + sgn * 44 * SS, base_y - 66 * SS,
                    20 * SS, 4 * SS, 1.3, -0.5 * math.pi, mirror=-sgn)
        tapered_stroke(d, sp, 4 * SS, 2 * SS, col)
    img = img.resize((W, H), Image.LANCZOS)
    return img


# ----------------------------------------------------------------- integral

def cypress(d, x, ytop, ybot, wmax, col, rng):
    """A dark columnar cypress silhouette with a wobbly edge."""
    pts_l, pts_r = [], []
    n = 30
    phase = rng.uniform(0, 6.28)
    for i in range(n + 1):
        t = i / n
        y = ybot + (ytop - ybot) * t
        # width profile: pointed tip, swelling body
        w = wmax * (math.sin(math.pi * min(1.0, t * 1.05)) ** 0.55)
        w *= 0.92 + 0.16 * math.sin(phase + t * 9.0)
        pts_l.append((x - w, y))
        pts_r.append((x + w, y))
    d.polygon(pts_l + pts_r[::-1], fill=col)


def gen_integral():
    """Stylized cypress-road landscape (flat illustration, ~3.4x4.2in)."""
    W, H = 816, 1016
    rng = random.Random(7719)
    img = Image.new("RGB", (W, H))
    d = ImageDraw.Draw(img)
    # sky: dusty mauve -> pale gold horizon
    top = (148, 122, 118)
    hor = (232, 214, 168)
    horizon = int(H * 0.60)
    for y in range(horizon):
        t = y / horizon
        col = tuple(int(top[c] + (hor[c] - top[c]) * (t ** 1.15))
                    for c in range(3))
        d.line([(0, y), (W, y)], fill=col)
    # haze glow around the low sun, right of centre
    glow = Image.new("L", (W, H), 0)
    gd = ImageDraw.Draw(glow)
    gd.ellipse([W * 0.30, H * 0.38, W * 0.95, H * 0.75], fill=90)
    glow = glow.filter(ImageFilter.GaussianBlur(60))
    sun = Image.new("RGB", (W, H), (245, 235, 200))
    img = Image.composite(sun, img, glow.point(lambda v: v // 3))
    d = ImageDraw.Draw(img)
    # rolling hills
    hills = [((150, 138, 88), 0.585, 26), ((110, 116, 70), 0.63, 40),
             ((82, 92, 56), 0.70, 60)]
    for col, y0, amp in hills:
        pts = [(0, H)]
        for x in range(0, W + 1, 8):
            y = H * y0 + math.sin(x / W * 4.6 + y0 * 20) * amp \
                + math.sin(x / W * 11.0 + y0 * 50) * amp * 0.3
            pts.append((x, y))
        pts.append((W, H))
        d.polygon(pts, fill=col)
    # road: light gravel tapering to a vanishing point on the right
    van = (W * 0.74, H * 0.685)
    left = [(W * 0.02, H), (W * 0.22, H * 0.86), (W * 0.42, H * 0.76), van]
    right = [(W * 0.98, H), (W * 0.88, H * 0.87), (W * 0.80, H * 0.75), van]
    d.polygon(left + right[::-1], fill=(186, 172, 148))
    # faint wheel tracks
    for tt in (0.35, 0.65):
        tr = []
        for i in range(24):
            t = i / 23.0
            lx = left[0][0] + (van[0] - left[0][0]) * t
            rx = right[0][0] + (van[0] - right[0][0]) * t
            x = lx + (rx - lx) * tt
            y = left[0][1] + (van[1] - left[0][1]) * (t ** 0.9)
            tr.append((x, y))
        d.line(tr, fill=(156, 142, 120), width=3)
    # cypress rows: tall at left foreground, receding along the road
    trees = [
        (0.145, 0.055, 0.685, 34), (0.30, 0.30, 0.66, 26),
        (0.42, 0.42, 0.655, 20), (0.52, 0.50, 0.655, 15),
        (0.60, 0.555, 0.655, 12), (0.665, 0.585, 0.655, 9),
        (0.86, 0.18, 0.75, 30), (0.985, 0.30, 0.85, 24),
    ]
    for fx, ytopf, ybotf, w in trees:
        cypress(d, W * fx, H * ytopf, H * ybotf, w, (44, 52, 38), rng)
    # warm vignette + light grain
    grain = Image.effect_noise((W, H), 12)
    img = Image.composite(img, img.point(lambda v: int(v * 0.96)),
                          grain.point(lambda v: 24))
    return img


# -------------------------------------------------------------------- facet

def gen_facet():
    """Translucent white facet texture painted over the accent band."""
    W, H = 1200, 200
    img = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    s = SS
    # big white triangular facets at varied alpha (like the original,
    # where facet faces are different white opacity levels)
    facets = [
        ((0, 0, 1200, 0, 720, 200), 45),
        ((0, 0, 720, 200, 0, 150), 90),
        ((0, 150, 720, 200, 0, 200), 140),
        ((1200, 0, 1200, 200, 760, 90), 110),
        ((720, 200, 1200, 200, 1200, 110), 70),
        ((0, 200, 380, 200, 0, 175), 235),
    ]
    for tri, a in facets:
        pts = [(tri[i] * s, tri[i + 1] * s) for i in (0, 2, 4)]
        d.polygon(pts, fill=(255, 255, 255, a))
    # hairline facet edges
    for (x0, y0, x1, y1, a) in ((0, 150, 1200, 12, 200),
                                (0, 176, 1200, 96, 150),
                                (760, 90, 1200, 60, 120)):
        d.line([(x0 * s, y0 * s), (x1 * s, y1 * s)],
               fill=(168, 168, 168, a), width=s)
    img = img.resize((W, H), Image.LANCZOS)
    return img


# -------------------------------------------------------------------- emit

def c_array(name, data):
    out = ["static const unsigned char %s[] = {" % name]
    for i in range(0, len(data), 12):
        out.append(",".join("0x%02x" % b for b in data[i:i + 12]) + ",")
    out.append("};")
    return "\n".join(out)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    assets = []

    def add(aid, filename, mime, img, fmt):
        path = os.path.join(OUT_DIR, filename)
        img.save(path, fmt)
        buf = io.BytesIO()
        img.save(buf, fmt)
        assets.append((aid, mime, buf.getvalue()))
        print("wrote %s (%d bytes)" % (path, len(buf.getvalue())))

    add("feathers", "cover-feathers.png", "image/png",
        gen_feathers(), "PNG")
    add("filgree", "cover-filgree.png", "image/png",
        gen_filgree(), "PNG")
    add("filgree-small", "cover-filgree-small.png", "image/png",
        gen_filgree_small(), "PNG")
    add("integral", "cover-integral.jpg", "image/jpeg",
        gen_integral(), "JPEG")
    add("facet-band", "cover-facet-band.png", "image/png",
        gen_facet(), "PNG")

    names = []
    with open(OUT_CPP, "w") as f:
        f.write("/* Generated by tools/mkcoverart.py -- do not edit.\n"
                " * Original Abinova cover artwork (GPL-2.0+), drawn\n"
                " * programmatically; the Word originals ship\n"
                " * non-redistributable raster art (LIC01), so these\n"
                " * are our own equivalents. */\n\n"
                "#include \"fv_CoverAssets.h\"\n\n")
        for aid, mime, data in assets:
            sym = "s_cover_%s" % aid.replace("-", "_")
            f.write(c_array(sym, data) + "\n\n")
            names.append((aid, mime, sym))
        f.write("static const FV_CoverAsset s_coverAssets[] = {\n")
        for aid, mime, sym in names:
            f.write("\t{ \"%s\", \"%s\", %s, sizeof(%s) },\n"
                    % (aid, mime, sym, sym))
        f.write("\t{ nullptr, nullptr, nullptr, 0 }\n};\n\n")
        f.write("const FV_CoverAsset * FV_coverAssetById(const char * szId)\n"
                "{\n"
                "\tfor (const FV_CoverAsset * p = s_coverAssets; p->szId; ++p)\n"
                "\t\tif (szId && 0 == strcmp(szId, p->szId))\n"
                "\t\t\treturn p;\n"
                "\treturn nullptr;\n"
                "}\n")
    print("wrote %s" % OUT_CPP)


if __name__ == "__main__":
    main()
