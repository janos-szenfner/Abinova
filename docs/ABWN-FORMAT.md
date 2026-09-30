# ABWN — Abinova Word-processing Markup Language

`.abwn` is Abinova 4.x's native document format. It is an XML
serialization of the piece table, derived from AWML 1.0 (AbiWord
Markup Language): the vocabulary is unchanged except for the root
element, doctype and namespace, so AWML-aware readers that accept an
`<abinova>` root remain compatible.

- DTD: [`abwn.dtd`](../abwn.dtd) (structural schema)
- Exporter: `src/wp/impexp/xp/ie_exp_Abinova_1.cpp`
- Importer: `src/wp/impexp/xp/ie_imp_Abinova_1.cpp`
- Property registry: `src/text/ptbl/xp/pp_Property.cpp`
  (`PP_LEVEL_*` groups mirrored by the tables below)

## 1. File layout

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE abinova PUBLIC "-//ABISOURCE//DTD AWML 1.0 Strict//EN"
        "https://raw.githubusercontent.com/janos-szenfner/Abinova/main/abwn.dtd">
<abinova version="1.0" fileformat="4.0" template="false"
         xmlns="https://raw.githubusercontent.com/janos-szenfner/Abinova/main/abwn.dtd">
  <metadata><m key="dc.title">…</m>…</metadata>
  <styles><s name="Normal" type="P" props="…"/>…</styles>
  <lists><l id="0" props="…"/>…</lists>
  <pagesize pagetype="A4" orientation="portrait" width="210.0"
            height="297.0" units="mm" margin-scale="1.0"/>
  <section props="…">            <!-- body -->
    <p>…</p>
    <table><cell><p>…</p></cell></table>
    <frame props="frame-width:2,0000in; …"><p>…</p></frame>
  </section>
  <section type="annotation">…</section>
  <data><d name="image0" mime-type="image/png" base64="…"/></data>
</abinova>
```

Header/footer fragments are extra `<section>` elements referenced by
`header`/`footer` attributes on the body section. Footnote, endnote
and annotation content live in `type="footnote|endnote|annotation"`
sections.

## 2. Serialization conventions

### Property bags

Every element may carry `props="name:value; name:value; …"` plus a
stable `xid` object id. Properties are name/value pairs separated by
`;`. A property that is **absent** inherits its registry default (see
the tables below); a property with a table default therefore does not
need to be written unless it differs.

> **Round-trip warning:** property *values* must not themselves
> contain `;` or a bare `,` — the parser splits on them. Structured
> values therefore use their own sub-grammars (e.g.
> `fill-gradient` uses `:` and `,` field separators, `shape-path`
> uses space-separated tokens).

### Numbers and units

- The writer emits `,` as the decimal separator (locale-independent);
  the reader accepts both `,` and `.`.
- Lengths carry a unit suffix: `in`, `cm`, `mm`, `pt`, `pi`, `px`;
  no suffix means layout units (1/1440 inch).
- Angles: `frame-rotation` is degrees×60000 (OOXML convention);
  `fill-gradient` angle is also ×60000.
- Fractions/positions: `fill-alpha` is 0..1; gradient stops and
  `image-src-rect` values are in 1000ths of a percent (0..100000).

### Colors

`RRGGBB` hex, no `#`. `transparent` where a color is optional.

## 3. Elements

| Element | Role |
|---|---|
| `abinova` | root; `version`, `fileformat`, `template`, `xid-max` |
| `metadata`/`m` | Dublin Core + `abiword.*` keys |
| `history`, `rdf`, `changes`, `masterpages`, `notes` | pass-through payload blocks |
| `styles`/`s` | named styles (`type="P\|C"`, `basedon`, `followedby`) |
| `lists`/`l` | list definitions; `id`/`parentid`/`type`/`start-value`/`list-delim`/`list-decimal` are direct attributes, not props |
| `pagesize` | page geometry |
| `authors`/`author` | collaboration author ids |
| `section` | flow root; body first, then `type=`-qualified out-of-flow roots |
| `p` | paragraph; `style`, `listid`, `props` |
| `c` | styled run containing text |
| `pbr` | explicit page break |
| `a` | hyperlink (`xlink:href`) |
| `ann` | annotation span anchor; `annotation-id` prop links to `annotate` |
| `annotate` | comment body inside an `annotation` section (`author`, `date`, `initials`) |
| `bookmark` | named anchor (`type`, `name`) |
| `field` | computed content (`type`: `page_number`, `toc`, …) |
| `image` | inline/anchored picture; `dataid` → `data/d` item |
| `foot`, `endnote` | footnote/endnote markers inline in `p`; each **embeds the note's own block content** (`footnote-id`/`endnote-id`) — there are no separate note sections |
| `margin`, `textmeta` | margin notes / RDF anchors |
| `math` | MathML fragment (payload in `data`) |
| `embed` | embedded object (`dataid`) |
| `toc` | table-of-contents block |
| `table`/`cell` | tables; cell spanning via `cell-attach-*` props |
| `frame` | absolutely-positioned box (textboxes, drawing shapes, pictures) |
| `data`/`d` | named payloads (base64 when `base64` attr set) |

## 4. Frames and positioned objects

`<frame>` is Abinova's general positioned-object element: Word text
boxes, DrawingML shapes (`wps:wsp`), grouped shapes (`wpg`) and
anchored pictures all serialize to it. Geometry and paint are
described by FRAME-level properties.

### Placement

| Property | Meaning |
|---|---|
| `frame-page-xpos` / `frame-page-ypos` | box origin, page coordinates |
| `frame-column-xpos` / `-ypos` | origin relative to current column |
| `frame-width` / `frame-height` | box size |
| `position-to` | `frame-above-text` (anchored block) or `page-above-text` / `page-below-text` (floating) |
| `wrap-mode` | `wrapped-text`, `wrapped-topbot`, `wrapped-square`, `below-text` (`behindDoc`), `above-text` |
| `frame-stack-order` | z-order within the above/below vectors |
| `frame-rotation` | degrees×60000, clockwise |
| `frame-flip-horiz` / `frame-flip-vert` | mirror transforms |
| `base-halign` / `base-valign` | text alignment base inside the frame |

### Paint

| Property | Meaning |
|---|---|
| `background-color` | solid fill (`RRGGBB` or `transparent`) |
| `fill-gradient` | linear gradient, see §4.1 |
| `fill-alpha` | fill opacity 0..1 (OOXML `a:alpha`) |
| `shape-path` | custom geometry, see §4.2 |
| `bot-style`/`top-style`/`left-style`/`right-style` + `*-color`/`*-thickness` | border; `shape-path` suppresses the rectangular border and strokes the path instead |
| `frame-shadow` | `outer` = drop shadow (OOXML `a:outerShdw`); see §4.3 |

### Content layout

| Property | Meaning |
|---|---|
| `xpad` / `ypad` | inner padding (symmetric) |
| `xpad-left` / `xpad-right` / `ypad-top` / `ypad-bottom` | per-side padding (OOXML `lIns`/`rIns`/`tIns`/`bIns`); empty = use `xpad`/`ypad` |
| `frame-valign` | vertical alignment of content: `top` (default), `center`, `bottom` |
| `frame-text-direction` | OOXML `vert`/`vert270`/`eaVert`/`wordArtVert*` values (stored; vertical layout supported for `vert`/`vert270`) |
| `frame-text-dir-flag` | normalization flag emitted by the importer |

### Images inside frames

| Property | Meaning |
|---|---|
| `frame-type` | `image` for picture frames |
| `image-dataid` | `data/d` payload name |
| `image-src-rect` | source crop `l,t,r,b` in 1000ths of a percent (OOXML `a:srcRect`) |
| `image-duotone` | `loRRGGBB hiRRGGBB` luminance remap pair (OOXML `a:duotone`) |
| `image-grayscale` | `1` to convert to gray (OOXML `a:grayscl`) |
| `image-lum` | `bright contrast` fractions (OOXML `a:lum`) |
| `image-alpha-mod` | alpha multiplier 0..1 (OOXML `a:alphaModFix@amt`) |

### Grouped shapes

OOXML `wpg`/`v:group` children are flattened: each child becomes its
own `<frame>` with absolute page coordinates already resolved, so no
group-transform state survives into abwn.

### 4.1 `fill-gradient` grammar

```
fill-gradient: lin:<angle60000>,<pos>:<RRGGBB>[,<pos>:<RRGGBB>]*
```

- `<angle60000>` — gradient direction in 1/60000 degree
  (`5400000` = 90°).
- `<pos>` — stop position, 0..100000 (1000ths of a percent).
- Final colors after `lumMod`/`lumOff`/`tint`/`shade`/`satMod`
  transforms are stored (the importer resolves transforms at
  `</a:srgbClr|a:schemeClr>`).

Example:

```
props="fill-gradient:lin:0,0:DAE3F3,100000:8FAADC; background-color:8FAADC"
```

`background-color` is always written too as the solid fallback.

### 4.2 `shape-path` grammar

Custom geometry (OOXML `a:custGeom`, normalized to a 1000×1000
coordinate space; y grows downward):

```
shape-path:M x y  L x y  C x1 y1 x2 y2 x3 y3  Q x1 y1 x2 y2  Z
```

- `M` move, `L` line, `C` cubic Bézier, `Q` quadratic Bézier, `Z` close.
- Coordinates are integers 0..1000 mapping onto the frame box.
- The path is used both as clip and as stroke for `*-style` borders.

Example (diamond):

```
props="frame-width:2,0000in; frame-height:1,0000in;
       shape-path:M 500 0 L 1000 500 L 500 1000 L 0 500 Z;
       background-color:4472C4"
```

### 4.3 `frame-shadow-*` properties

OOXML `a:outerShdw` drop shadow, painted as a blurred silhouette of
the shape (its `shape-path` when present, else the frame box) masked
in the shadow color:

| Property | Meaning |
|---|---|
| `frame-shadow` | `outer` when an outer shadow is present (`none` otherwise) |
| `frame-shadow-offset` | `a:outerShdw@dist` — offset distance (length) |
| `frame-shadow-dir` | `a:outerShdw@dir` — offset direction, degrees×60000 |
| `frame-shadow-blur` | `a:outerShdw@blurRad` — blur radius (length) |
| `frame-shadow-color` | shadow color `RRGGBB` |
| `frame-shadow-alpha` | shadow opacity 0..1 (color child `a:alpha`) |
| `frame-shadow-rot` | `a:outerShdw@rotWithShape` — `1` rotates the offset with the shape (default), `0` keeps it fixed in page space |

Example:

```
props="frame-shadow:outer; frame-shadow-offset:4,00pt;
       frame-shadow-dir:2700000; frame-shadow-blur:6,00pt;
       frame-shadow-color:000000; frame-shadow-alpha:0,450"
```

## 5. Extensions over AWML

Properties added beyond the AbiWord registry, all round-tripped:

- **Frame**: `frame-valign`, `frame-rotation`, `frame-flip-horiz`,
  `frame-flip-vert`, `frame-stack-order`, `frame-text-direction`,
  `frame-text-dir-flag`, `frame-page-xpos/ypos`,
  `frame-column-xpos/ypos`, `position-to`, `wrap-mode`,
  `fill-gradient`, `fill-alpha`, `shape-path`, `image-src-rect`,
  `image-duotone`, `image-grayscale`, `image-lum`, `image-alpha-mod`
  (OOXML `a:blip` effects),
  `frame-shadow*` (OOXML `a:outerShdw`, see §4.3),
  `text-warp` (OOXML `a:prstTxWarp` name, stored for fidelity).
- **Character**: OOXML fidelity props such as `em`, `fit-text`,
  `kern`, `char-position`, `char-spacing`, `text-outline-*`,
  `text-fill-*`, `text-shadow-*`, `office-*` decorations.
- **Block**: `shading-*`, border `*-style`/`*-color`/`*-space`/
  `*-thickness`, `keep-lines`, `widow-ctrl`, `list-*`, `outline-level`.
- **Document**: `document-*` settings mapped from `settings.xml`.
- **Other**: `altchunk-path`/`altchunk-format` (unresolved
  `w:altChunk` references kept as links), `comment-*`,
  `textmeta-xmlid`/`rdfa-*` (RDFa), `col-space`, `table-row-*`,
  `cell-attach-*`.

## 6. Property reference

The complete registry (default values and inheritance) is generated
from `src/text/ptbl/xp/pp_Property.cpp`:

### Document-level properties (20)

| Property | Default | Inherited |
|---|---|---|
| `document-auto-hyphenation` | `0` | no |
| `document-book-fold-printing` | `0` | no |
| `document-book-fold-rev` | `0` | no |
| `document-clr-scheme-mapping` | `*(empty)*` | no |
| `document-consecutive-hyphen-limit` | `*(empty)*` | no |
| `document-decimal-symbol` | `*(empty)*` | no |
| `document-default-tab-stop` | `*(empty)*` | no |
| `document-do-not-track-formatting` | `0` | no |
| `document-do-not-track-moves` | `0` | no |
| `document-even-odd-headers` | `0` | no |
| `document-gutter-at-top` | `0` | no |
| `document-hyphenation-zone` | `*(empty)*` | no |
| `document-list-separator` | `*(empty)*` | no |
| `document-mirror-margins` | `0` | no |
| `document-protected` | `0` | no |
| `document-protection-mode` | `*(empty)*` | no |
| `document-remove-date-info` | `0` | no |
| `document-remove-personal-info` | `0` | no |
| `document-track-changes` | `0` | no |
| `document-zoom` | `*(empty)*` | no |

### Section properties (67)

| Property | Default | Inherited |
|---|---|---|
| `background-color` | `transparent` | no |
| `column-gap` | `0.25in` | no |
| `column-line` | `off` | no |
| `columns` | `1` | no |
| `footer` | `*(empty)*` | no |
| `footer-even` | `*(empty)*` | no |
| `footer-first` | `*(empty)*` | no |
| `footer-last` | `*(empty)*` | no |
| `header` | `*(empty)*` | no |
| `header-even` | `*(empty)*` | no |
| `header-first` | `*(empty)*` | no |
| `header-last` | `*(empty)*` | no |
| `page-border-art` | `*(empty)*` | no |
| `page-border-bottom` | `none` | no |
| `page-border-bottom-art` | `*(empty)*` | no |
| `page-border-bottom-color` | `auto` | no |
| `page-border-bottom-shadow` | `0` | no |
| `page-border-bottom-space` | `0pt` | no |
| `page-border-bottom-thickness` | `0pt` | no |
| `page-border-display` | `all` | no |
| `page-border-left` | `none` | no |
| `page-border-left-art` | `*(empty)*` | no |
| `page-border-left-color` | `auto` | no |
| `page-border-left-shadow` | `0` | no |
| `page-border-left-space` | `0pt` | no |
| `page-border-left-thickness` | `0pt` | no |
| `page-border-offset` | `page` | no |
| `page-border-right` | `none` | no |
| `page-border-right-art` | `*(empty)*` | no |
| `page-border-right-color` | `auto` | no |
| `page-border-right-shadow` | `0` | no |
| `page-border-right-space` | `0pt` | no |
| `page-border-right-thickness` | `0pt` | no |
| `page-border-shadow` | `0` | no |
| `page-border-top` | `none` | no |
| `page-border-top-art` | `*(empty)*` | no |
| `page-border-top-color` | `auto` | no |
| `page-border-top-shadow` | `0` | no |
| `page-border-top-space` | `0pt` | no |
| `page-border-top-thickness` | `0pt` | no |
| `page-margin-bottom` | `1in` | no |
| `page-margin-footer` | `0.0in` | no |
| `page-margin-header` | `0.0in` | no |
| `page-margin-left` | `1in` | no |
| `page-margin-right` | `1in` | no |
| `page-margin-top` | `1in` | no |
| `section-doc-grid` | `*(empty)*` | no |
| `section-doc-grid-char-space` | `*(empty)*` | no |
| `section-doc-grid-line-pitch` | `*(empty)*` | no |
| `section-endnote-suppress` | `0` | no |
| `section-footnote-line-thickness` | `0.005in` | no |
| `section-footnote-yoff` | `0.01in` | no |
| `section-form-protected` | `0` | no |
| `section-ln-count-by` | `*(empty)*` | no |
| `section-ln-distance` | `*(empty)*` | no |
| `section-ln-restart` | `*(empty)*` | no |
| `section-ln-start` | `*(empty)*` | no |
| `section-max-column-height` | `0in` | no |
| `section-paper-src-first` | `*(empty)*` | no |
| `section-paper-src-other` | `*(empty)*` | no |
| `section-restart` | `*(empty)*` | no |
| `section-restart-value` | `*(empty)*` | no |
| `section-rtl-gutter` | `0` | no |
| `section-space-after` | `0.25in` | no |
| `section-text-direction` | `*(empty)*` | no |
| `section-y-align` | `top` | no |
| `toc-id` | `0` | no |

### Block (paragraph) properties (100)

| Property | Default | Inherited |
|---|---|---|
| `adjust-right-ind` | `1` | no |
| `altchunk-format` | `*(empty)*` | no |
| `altchunk-path` | `*(empty)*` | no |
| `auto-space-de` | `1` | no |
| `auto-space-dn` | `1` | no |
| `baseline-align` | `auto` | no |
| `border-merge` | `0` | yes |
| `border-shadow-merge` | `0` | yes |
| `bot-shadow` | `0` | no |
| `bot-shadow-color` | `grey` | no |
| `bot-space` | `0.02in` | no |
| `contextual-spacing` | `0` | no |
| `default-tab-interval` | `0.5in` | no |
| `format` | `%*%d.` | yes |
| `keep-together` | `no` | no |
| `keep-with-next` | `no` | no |
| `kinsoku` | `1` | no |
| `left-shadow` | `0` | no |
| `left-shadow-color` | `grey` | no |
| `left-space` | `0.02in` | no |
| `line-height` | `1.0` | no |
| `list-decimal` | `.` | yes |
| `list-delim` | `%L` | yes |
| `list-tag` | `0` | no |
| `margin-bottom` | `0in` | no |
| `margin-left` | `0in` | no |
| `margin-right` | `0in` | no |
| `margin-top` | `0in` | no |
| `mirror-indents` | `0` | no |
| `orphans` | `2` | no |
| `outline-level` | `*(empty)*` | no |
| `overflow-punct` | `1` | no |
| `right-shadow` | `0` | no |
| `right-shadow-color` | `grey` | no |
| `right-space` | `0.02in` | no |
| `shading-background-color` | `white` | no |
| `shading-foreground-color` | `white` | no |
| `shading-pattern` | `0` | no |
| `snap-to-grid` | `1` | no |
| `start-value` | `1` | yes |
| `suppress-auto-hyphens` | `0` | no |
| `suppress-line-numbers` | `0` | no |
| `tabstops` | `*(empty)*` | no |
| `text-folded` | `0` | no |
| `text-folded-id` | `0` | no |
| `text-indent` | `0in` | no |
| `toc-dest-style2` | `Contents 2` | no |
| `toc-dest-style3` | `Contents 3` | no |
| `toc-dest-style4` | `Contents 4` | no |
| `toc-has-heading` | `1` | no |
| `toc-has-label1` | `1` | no |
| `toc-has-label2` | `1` | no |
| `toc-has-label3` | `1` | no |
| `toc-has-label4` | `1` | no |
| `toc-heading` | `Contents` | no |
| `toc-heading-style` | `Contents Header` | no |
| `toc-indent1` | `0.5in` | no |
| `toc-indent2` | `0.5in` | no |
| `toc-indent3` | `0.5in` | no |
| `toc-indent4` | `0.5in` | no |
| `toc-label-after1` | `*(empty)*` | no |
| `toc-label-after2` | `*(empty)*` | no |
| `toc-label-after3` | `*(empty)*` | no |
| `toc-label-after4` | `*(empty)*` | no |
| `toc-label-before1` | `*(empty)*` | no |
| `toc-label-before2` | `*(empty)*` | no |
| `toc-label-before3` | `*(empty)*` | no |
| `toc-label-before4` | `*(empty)*` | no |
| `toc-label-inherits1` | `1` | no |
| `toc-label-inherits2` | `1` | no |
| `toc-label-inherits3` | `1` | no |
| `toc-label-inherits4` | `1` | no |
| `toc-label-start1` | `1` | no |
| `toc-label-start2` | `1` | no |
| `toc-label-start3` | `1` | no |
| `toc-label-start4` | `1` | no |
| `toc-label-type1` | `numeric` | no |
| `toc-label-type2` | `numeric` | no |
| `toc-label-type3` | `numeric` | no |
| `toc-label-type4` | `numeric` | no |
| `toc-level` | `*(empty)*` | no |
| `toc-page-type1` | `numeric` | no |
| `toc-page-type2` | `numeric` | no |
| `toc-page-type3` | `numeric` | no |
| `toc-page-type4` | `numeric` | no |
| `toc-range-bookmark` | `*(empty)*` | no |
| `toc-source-style1` | `Heading 1` | no |
| `toc-source-style2` | `Heading 2` | no |
| `toc-source-style3` | `Heading 3` | no |
| `toc-source-style4` | `Heading 4` | no |
| `toc-tab-leader1` | `dot` | no |
| `toc-tab-leader2` | `dot` | no |
| `toc-tab-leader3` | `dot` | no |
| `toc-tab-leader4` | `dot` | no |
| `top-line-punct` | `0` | no |
| `top-shadow` | `0` | no |
| `top-shadow-color` | `grey` | no |
| `top-space` | `0.02in` | no |
| `widows` | `2` | no |
| `word-wrap` | `1` | no |

### Character (run) properties (23)

| Property | Default | Inherited |
|---|---|---|
| `bgcolor` | `transparent` | yes |
| `char-emphasis` | `*(empty)*` | yes |
| `char-kern` | `0pt` | yes |
| `char-spacing` | `0pt` | yes |
| `char-width` | `100` | yes |
| `color` | `000000` | yes |
| `display` | `inline` | yes |
| `font-family` | `Carlito` | yes |
| `font-size` | `12pt` | yes |
| `font-stretch` | `normal` | yes |
| `font-style` | `normal` | yes |
| `font-variant` | `normal` | yes |
| `font-weight` | `normal` | yes |
| `height` | `0in` | no |
| `homogeneous` | `1` | no |
| `lang` | `en-US` | yes |
| `list-style` | `None` | yes |
| `no-proof` | `0` | yes |
| `text-decoration` | `none` | yes |
| `text-position` | `normal` | yes |
| `text-transform` | `none` | yes |
| `vert-position` | `0pt` | yes |
| `width` | `0in` | no |

### Table/row/cell properties (54)

| Property | Default | Inherited |
|---|---|---|
| `bot-attach` | `*(empty)*` | no |
| `bot-color` | `000000` | no |
| `bot-style` | `1` | no |
| `bot-thickness` | `1px` | no |
| `cell-fit-text` | `0` | no |
| `cell-hide-mark` | `0` | no |
| `cell-margin-bottom` | `0.002in` | no |
| `cell-margin-left` | `0.002in` | no |
| `cell-margin-right` | `0.002in` | no |
| `cell-margin-top` | `0.002in` | no |
| `cell-no-wrap` | `0` | no |
| `cell-text-direction` | `*(empty)*` | no |
| `header-row` | `*(empty)*` | no |
| `left-attach` | `*(empty)*` | no |
| `left-color` | `000000` | no |
| `left-style` | `1` | no |
| `left-thickness` | `1px` | no |
| `right-attach` | `*(empty)*` | no |
| `right-color` | `000000` | no |
| `right-style` | `1` | no |
| `right-thickness` | `1px` | no |
| `table-bidi-visual` | `0` | no |
| `table-border` | `0.1in` | no |
| `table-caption` | `*(empty)*` | no |
| `table-col-spacing` | `0.03in` | no |
| `table-column-leftpos` | `0.0in` | no |
| `table-column-props` | `*(empty)*` | no |
| `table-description` | `*(empty)*` | no |
| `table-float-halign` | `*(empty)*` | no |
| `table-float-hanchor` | `*(empty)*` | no |
| `table-float-margin-bottom` | `*(empty)*` | no |
| `table-float-margin-left` | `*(empty)*` | no |
| `table-float-margin-right` | `*(empty)*` | no |
| `table-float-margin-top` | `*(empty)*` | no |
| `table-float-valign` | `*(empty)*` | no |
| `table-float-vanchor` | `*(empty)*` | no |
| `table-float-x` | `*(empty)*` | no |
| `table-float-y` | `*(empty)*` | no |
| `table-line-thickness` | `0.8pt` | no |
| `table-line-type` | `1` | no |
| `table-look` | `*(empty)*` | no |
| `table-margin-bottom` | `0.01in` | no |
| `table-margin-left` | `0.005in` | no |
| `table-margin-right` | `0.005in` | no |
| `table-margin-top` | `0.01in` | no |
| `table-max-extra-margin` | `0.05` | no |
| `table-position` | `left` | no |
| `table-row-props` | `*(empty)*` | no |
| `table-row-spacing` | `0.01in` | no |
| `top-attach` | `*(empty)*` | no |
| `top-color` | `000000` | no |
| `top-style` | `1` | no |
| `top-thickness` | `1px` | no |
| `vert-align` | `0` | no |

### Frame / positioned-object properties (51)

| Property | Default | Inherited |
|---|---|---|
| `bounding-space` | `0.05in` | no |
| `fill-alpha` | `1.0` | no |
| `fill-gradient` | `*(empty)*` | no |
| `frame-col-xpos` | `0.0in` | no |
| `frame-col-ypos` | `0.0in` | no |
| `frame-expand-height` | `0.0in` | no |
| `frame-flip-horiz` | `0` | no |
| `frame-flip-vert` | `0` | no |
| `frame-group` | `*(empty)*` | no |
| `frame-height` | `0.0in` | no |
| `frame-hidden` | `0` | no |
| `frame-horiz-align` | `left` | no |
| `frame-min-height` | `0.0in` | no |
| `frame-name` | `*(empty)*` | no |
| `frame-page-xpos` | `0.0in` | no |
| `frame-page-ypos` | `0.0in` | no |
| `frame-pref-column` | `0` | no |
| `frame-pref-page` | `0` | no |
| `frame-rel-width` | `0.5` | no |
| `frame-rotation` | `0` | no |
| `frame-shadow` | `none` | no |
| `frame-shadow-alpha` | `0.5` | no |
| `frame-shadow-blur` | `0pt` | no |
| `frame-shadow-color` | `000000` | no |
| `frame-shadow-dir` | `0` | no |
| `frame-shadow-offset` | `0pt` | no |
| `frame-shadow-rot` | `1` | no |
| `frame-stack-order` | `0` | no |
| `frame-text-direction` | `*(empty)*` | no |
| `frame-type` | `textbox` | no |
| `frame-valign` | `top` | no |
| `frame-width` | `0.0in` | no |
| `image-alpha-mod` | `*(empty)*` | no |
| `image-duotone` | `*(empty)*` | no |
| `image-grayscale` | `0` | no |
| `image-lum` | `*(empty)*` | no |
| `image-src-rect` | `*(empty)*` | no |
| `line-end-arrow` | `none` | no |
| `line-end-arrow-len` | `med` | no |
| `line-end-arrow-w` | `med` | no |
| `line-start-arrow` | `none` | no |
| `line-start-arrow-len` | `med` | no |
| `line-start-arrow-w` | `med` | no |
| `position-to` | `block-above-text` | no |
| `shape-path` | `*(empty)*` | no |
| `text-warp` | `none` | no |
| `wrap-mode` | `above-text` | no |
| `xpad` | `0.03in` | no |
| `xpad-left` | `*(empty)*` | no |
| `xpad-right` | `*(empty)*` | no |
| `xpos` | `0.0in` | no |
| `ypad` | `0.03in` | no |
| `ypad-bottom` | `*(empty)*` | no |
| `ypad-top` | `*(empty)*` | no |
| `ypos` | `0.0in` | no |

### Field properties (2)

| Property | Default | Inherited |
|---|---|---|
| `field-color` | `dcdcdc` | yes |
| `field-font` | `NULL` | yes |
