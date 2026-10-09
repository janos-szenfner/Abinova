#!/usr/bin/env python3
"""WPCOV synthetic fixtures: one WP6 .wpd kitchen-sink plus its OLE-style
structured-container twin, driving the WordPerfect importer's uncovered
paths (metadata, headers/footers, lists, tables, notes, spans, paragraph
props, page spans, substream handling).

cov_rich.wpd document stream:

- Extended Document Summary prefix packet (index type 0x12) carrying
  author/subject/publisher/category/keywords/language/abstract/typist
  records -> setDocumentMetaData.
- Header A (all pages), header B (even pages) and footer A groups
  (0xD6, prefix -> GeneralTextPacket subdocuments) -> openHeader /
  openFooter / the hdrftr capture+replay machinery.  Page-number
  position (0xD1/0x03, bottom center) -> _insertPageNumberParagraph +
  insertField inside the footer capture.
- Character formatting: attribute on/off groups (0xF2/0xF3) for bold,
  italic, underline, double underline, strikeout, super/subscript,
  small caps, extra large, small print, outline, shadow, redline;
  highlight on/off (0xFB/0xFC); character color + shading (0xD4/0x18,
  0xD4/0x19); font size + font face change (0xD4/0x1B, 0xD4/0x1A with
  an embedded font descriptor and a font-pool prefix id).
- Paragraph properties: justification center/right/full, line spacing,
  spacing after paragraph, first-line indent, left/right margin
  adjustment, and a tab set covering every alignment + leader kind.
- A hard left-tab tab function (0xE0/0x10) inside a paragraph ->
  insertTab.
- Two-column newspaper section (0xD2/0x02) with a hard column break
  back to one column.
- An ordered outline list (paragraph-number-on 0xD4/0x32 + paragraph
  number display reference 0xDA/0x0C/0x0D) with a second level, and an
  unordered list -> openOrderedListLevel/openUnorderedListLevel/
  openListElement incl. the iLevel>1 definition path.
- A footnote and an endnote (0xD7 on/off, prefix -> GeneralTextPacket)
  -> openFootnote/openEndnote + subdocuments.
- A 2x2 table (0xD4 0x2A/0x2C/0x2B + 0xC5/0xC6/0xBF) then a second
  table built from EOL-group cells (0xD0/0x0A,0x0B) carrying embedded
  row-information (header row, min height), cell spanning, fill colors,
  line color, cell information (vertical align, attributes) and the
  cell prefix flag (borders off) -> the openTableCell prop branches.
- A hard page break (0xC7) -> second page span, and an EOL-group hard
  EOL (0xD0/0x04) -> the multi-byte EOL path.

cov_ole.wpd: the same stream wrapped in a zip container with the single
member name libwpd looks for, "PerfectOffice_MAIN" -> the importer's
AbiWordperfectInputStream isStructured()/getSubStreamByName() branches
(gsf_infile_zip fallback: a real OLE is not needed for coverage of the
same code paths).

Packet ids are 1-based ordinals in the index:
  1 extended document summary      5 general text (footnote body)
  2 general text (header A)        6 general text (endnote body)
  3 general text (footer A)        7 font descriptor (font pool)
  4 general text (header B, even)
"""
import os
import struct
import sys
import zipfile

OUT = sys.argv[1] if len(sys.argv) > 1 else '/tmp'
os.makedirs(OUT, exist_ok=True)

EOL = b'\xcc'                       # WP6 single-byte hard EOL
ROW = b'\xc5'                       # table row (insertRow+insertCell)
CELL = b'\xc6'                      # table cell (insertCell)
TOFF = b'\xbf'                      # table off
EOC = b'\xc9'                       # hard end of column
EOP = b'\xc7'                       # hard end of page
HSPACE = b'\x81'                    # hard space
SOFTHY = b'\x82'                    # soft hyphen in line
HARDHY = b'\x84'                    # hard hyphen


def group(gid, sub, nondele=b'', dele=b'', prefix_ids=()):
    """WP6 variable-length group:
    [id][sub][size][flags][prefix ids][sizeNonDele][nondele][dele][size][id]"""
    flags = 0x80 if prefix_ids else 0x00
    size = (1 + 1 + 2 + 1 + (1 + 2 * len(prefix_ids) if prefix_ids else 0)
            + 2 + len(nondele) + len(dele) + 2 + 1)
    g = bytes([gid, sub]) + struct.pack('<H', size) + bytes([flags])
    if prefix_ids:
        g += bytes([len(prefix_ids)])
        g += b''.join(struct.pack('<H', p) for p in prefix_ids)
    g += struct.pack('<H', len(nondele)) + nondele + dele
    g += struct.pack('<H', size) + bytes([gid])
    return g


def fixed(gid, data):
    """WP6 fixed-length group: [id][data][id]."""
    return bytes([gid]) + data + bytes([gid])


def attr_on(a):
    return fixed(0xF2, bytes([a]))


def attr_off(a):
    return fixed(0xF3, bytes([a]))


def gen_text(text):
    """GeneralTextPacket payload: one text block holding a WP6
    subdocument stream (text + hard EOL)."""
    stream = text + EOL
    return (struct.pack('<H', 1) + b'\x00' * 4
            + struct.pack('<I', len(stream)) + stream)


def wpchars(s):
    """Encode text as WP6 (characterSet<<8 | char) u16 words."""
    if isinstance(s, str):
        s = s.encode('ascii')
    return b''.join(struct.pack('<H', c) for c in s)


def summary_packet(records):
    """Extended Document Summary packet: repeated
    [groupLen][tagID][skip u16][name wpchars NUL][data wpchars NUL]."""
    out = b''
    for tag, name, data in records:
        rec = (struct.pack('<H', tag) + b'\x00\x00'
               + wpchars(name) + b'\x00\x00'
               + wpchars(data) + b'\x00\x00')
        out += struct.pack('<H', len(rec) + 2) + rec
    return out


def font_descriptor(name):
    """WP6 font descriptor packet: 22 characteristic bytes, then the
    u16 name length (bytes) and the wpchar name."""
    nm = wpchars(name) + b'\x00\x00'
    return b'\x00' * 22 + struct.pack('<H', len(nm)) + nm


def eol_group(sub, records=()):
    """EOL group (0xD0): nondeletable body = u16 deletable-subfn size
    (0) followed by fixed-size embedded subfunction records."""
    body = struct.pack('<H', 0)
    for typ, payload, size in records:
        rec = bytes([typ]) + payload
        body += rec + b'\x00' * (size - len(rec))
    return group(0xD0, sub, body)


ROW_INFORMATION = 128
CELL_INFORMATION = 132
CELL_SPANNING = 133
CELL_FILL_COLORS = 134
CELL_LINE_COLOR = 135
CELL_PREFIX_FLAG = 139


def table_column(w):
    """0xD4/0x2C table column: flags, width, gutters, attributes,
    alignment, abs pos, number type, currency."""
    return group(0xD4, 0x2C, bytes([0]) + struct.pack('<H', w)
                 + struct.pack('<HH', 0, 0)
                 + struct.pack('<I', 0)
                 + bytes([0]) + struct.pack('<HH', 0, 0) + bytes([0]))


def table_open():
    """Table definition on + two column definitions + definition off."""
    on = group(0xD4, 0x2A, bytes([0, 3]) + struct.pack('<H', 0))  # pos 3 = full
    off = group(0xD4, 0x2B)
    return on + table_column(2400) + table_column(2400) + off


def build():
    # ---------------- document body ----------------
    doc = bytearray()

    # headers / footers (styles pass attaches them to the first page)
    doc += group(0xD6, 0x00, bytes([0x03]), prefix_ids=[2])  # header A all
    doc += group(0xD6, 0x02, bytes([0x03]), prefix_ids=[3])  # footer A all
    doc += group(0xD6, 0x01, bytes([0x02]), prefix_ids=[4])  # header B even

    # page number position: bottom center (6) -> page-number field
    # synthesized inside the footer
    doc += group(0xD1, 0x03, struct.pack('<HBHHBH', 0, 0, 0, 2400, 6, 0)
                 + struct.pack('<HH', 24, 0)
                 + bytes([0, 0, 0, 100])
                 + struct.pack('<H', 0) + bytes([0]))

    # --- paragraph 1: formatting spans ---------------------------------
    doc += b'Start. '
    doc += attr_on(12) + b'BOLD' + attr_off(12)
    doc += attr_on(8) + b'ITAL' + attr_off(8)
    doc += attr_on(14) + b'UND' + attr_off(14)
    doc += attr_on(11) + b'DUND' + attr_off(11)
    doc += attr_on(13) + b'STRK' + attr_off(13)
    doc += attr_on(5) + b'SUP' + attr_off(5)
    doc += attr_on(6) + b'SUB' + attr_off(6)
    doc += attr_on(15) + b'SCAP' + attr_off(15)
    doc += attr_on(0) + b'XL' + attr_off(0)
    doc += attr_on(3) + b'SMALL' + attr_off(3)
    doc += attr_on(7) + b'OUTL' + attr_off(7)
    doc += attr_on(9) + b'SHDW' + attr_off(9)
    doc += attr_on(10) + b'REDL' + attr_off(10)
    doc += attr_on(16) + b'BLNK' + attr_off(16)
    doc += fixed(0xFB, bytes([0xFF, 0xFF, 0x00, 0x64])) + b'HIGH'
    doc += fixed(0xFC, bytes([0x00, 0x00, 0x00, 0x64]))
    doc += group(0xD4, 0x18, bytes([0xFF, 0x00, 0x00])) + b'REDC'
    doc += group(0xD4, 0x18, bytes([0x00, 0x00, 0x00]))
    doc += group(0xD4, 0x19, bytes([40])) + b'SHAD'
    doc += group(0xD4, 0x19, bytes([100]))
    doc += HSPACE + b'hardspace' + SOFTHY + b'softhyph' + HARDHY + b'hardhyph'
    # font size + font face change (prefix id 7 = font descriptor pool)
    doc += group(0xD4, 0x1B, struct.pack('<H', 1400), prefix_ids=[7]) + b'BIGFONT'
    doc += group(0xD4, 0x1B, struct.pack('<H', 600), prefix_ids=[7]) + b'smallfont'
    doc += group(0xD4, 0x1A,
                 struct.pack('<HHHH', 1200, 0, 0, 1200),
                 font_descriptor('Ignored Embedded Name'),
                 prefix_ids=[7]) + b'SERIFFONT'
    doc += group(0xD4, 0x1A,
                 struct.pack('<HHHH', 1200, 0, 0, 1200),
                 prefix_ids=[0]) + b'deffont'
    # extended character (charset 1 typographic, char 0x41) + a
    # <=0x20 international character byte
    doc += fixed(0xF0, struct.pack('<H', (1 << 8) | 0x41))
    doc += bytes([0x10])                     # extended international char
    doc += EOL

    # --- paragraph 2: paragraph properties -----------------------------
    doc += group(0xD3, 0x05, bytes([2])) + b'Centered line' + EOL
    doc += group(0xD3, 0x05, bytes([3])) + b'Right line' + EOL
    doc += group(0xD3, 0x05, bytes([1])) + b'Justified line' + EOL
    doc += group(0xD3, 0x05, bytes([4])) + b'Alljust line' + EOL
    doc += group(0xD3, 0x01, struct.pack('<I', (2 << 16)))   # line spacing 2.0
    doc += group(0xD3, 0x0A, struct.pack('<I', (2 << 16))
                 + struct.pack('<H', 600))                 # spacing after
    doc += group(0xD3, 0x0B, struct.pack('<h', 600))         # first-line indent
    doc += group(0xD3, 0x0C, struct.pack('<h', 300))         # left margin adj
    doc += group(0xD3, 0x0D, struct.pack('<h', 300))         # right margin adj
    # tab set: absolute definition, 4 stops covering all alignments
    # and each leader kind (dot, hyphen, underscore)
    stops = (bytes([0x00]) + struct.pack('<H', 1200)      # left
             + bytes([0x01]) + struct.pack('<H', 2400)    # center
             + bytes([0x12]) + struct.pack('<H', 3600)    # right + leader '.'
             + bytes([0x13]) + struct.pack('<H', 4800)    # decimal + leader '_'? -> type3
             + bytes([0x33]) + struct.pack('<H', 6000))   # decimal + leader '_'
    doc += group(0xD3, 0x04, bytes([0]) + struct.pack('<H', 0)
                 + bytes([5]) + stops)
    doc += b'tabbed'
    doc += group(0xE0, 0x10, struct.pack('<H', 2400))     # hard left tab
    doc += b'aftertab' + EOL

    # --- two-column section --------------------------------------------
    cols = (bytes([0]) + struct.pack('<I', 0) + bytes([2])          # type, rowspacing, n
            + bytes([0]) + struct.pack('<H', 0x8000)                # col1 rel width 50%
            + bytes([0]) + struct.pack('<H', 0x2000)                # gutter
            + bytes([0]) + struct.pack('<H', 0x8000))               # col2 rel width 50%
    doc += group(0xD2, 0x02, cols)
    doc += b'colone' + EOC + b'coltwo' + EOL
    doc += group(0xD2, 0x02, bytes([0]) + struct.pack('<I', 0) + bytes([1]))

    # --- footnote + endnote ---------------------------------------------
    doc += b'with a note'
    doc += group(0xD7, 0x00, prefix_ids=[5])              # footnote on
    doc += group(0xD7, 0x01)                              # footnote off
    doc += b' and an endnote'
    doc += group(0xD7, 0x02, prefix_ids=[6])              # endnote on
    doc += group(0xD7, 0x03)                              # endnote off
    doc += b' done' + EOL

    # --- unordered list first (list stack empty -> guaranteed unordered):
    # para_number_on + bullet char + off, no display reference ->
    # openUnorderedListLevel with text:bullet-char.  The parastyle-begin
    # group clears the putative display-reference flag.
    doc += group(0xD3, 0x0E, struct.pack('<H', 9) + bytes([1] * 8) + bytes([0]))
    for txt in (b'Bullet one', b'Bullet two'):
        doc += group(0xDD, 0x04)                            # parastyle begin on p1
        doc += group(0xD4, 0x32, struct.pack('<HB', 9, 0) + bytes([0]))
        doc += b'-'
        doc += group(0xD4, 0x33)
        doc += group(0xE0, 0x10, struct.pack('<H', 1200))
        doc += txt + EOL

    # a normal paragraph at level 0 fully pops the unordered stack
    doc += b'Between lists' + EOL

    # --- ordered list: para_number_on(hash,level) -> display ref on ->
    # the literal number text -> display ref off -> para_number_off ->
    # tab -> item text.  This is the order real WP6 emits: text that
    # arrives while a display ref is open becomes the list label.
    doc += group(0xD3, 0x0E, struct.pack('<H', 7) + bytes([1] * 8) + bytes([0]))
    for level, num, txt in ((0, b'1.', b'First item'),
                            (0, b'2.', b'Second item'),
                            (1, b'a.', b'Nested item'),
                            (0, b'3.', b'Third item')):
        doc += group(0xD4, 0x32, struct.pack('<HB', 7, level) + bytes([0]))
        doc += group(0xDA, 0x0C, bytes([level]))       # para num display on
        doc += num
        doc += group(0xDA, 0x0D)                       # display off
        doc += group(0xD4, 0x33)                       # para number off
        doc += group(0xE0, 0x10, struct.pack('<H', 1200 + level * 1200))
        doc += txt + EOL

    # --- table 1: single-byte cell functions ------------------------------
    doc += table_open()
    doc += ROW + b'h1' + CELL + b'h2' + ROW + b'a1' + CELL + b'a2' + TOFF
    doc += EOL

    # --- table 2: EOL-group cells with embedded cell info ----------------
    doc += table_open()
    # header row (flag 0x04) with a min height (flag 0x02 + u16) and a
    # bottom-aligned filled cell
    doc += eol_group(0x0B, [
        (ROW_INFORMATION, bytes([0x04 | 0x02 | 0x10])
         + struct.pack('<H', 1200), 5),
        (CELL_FILL_COLORS, bytes([0xFF, 0, 0, 100, 0xCC, 0xCC, 0xCC, 100]),
         10),
        (CELL_INFORMATION, bytes([0x03, 2, 2])
         + struct.pack('<HH', 0x40, 0), 9),
    ]) + b'hr1'
    doc += eol_group(0x0A, [
        (CELL_PREFIX_FLAG, bytes([0x0F]), 3),   # all borders off
        (CELL_INFORMATION, bytes([0, 0, 1]) + struct.pack('<HH', 0, 0), 9),
    ]) + b'hr2'
    # row 2: a colspan=2 cell + line-colored bordered cell
    doc += eol_group(0x0B, [
        (CELL_SPANNING, bytes([2, 1]), 4),
        (CELL_FILL_COLORS, bytes([0, 0xFF, 0, 100, 0, 0, 0xFF, 100]), 10),
        (CELL_LINE_COLOR, bytes([0xFF, 0, 0, 100]), 6),
    ]) + b'span'
    doc += TOFF + EOL

    # --- second page span ------------------------------------------------
    doc += EOP
    doc += b'page two' + EOL
    # multi-byte EOL-group hard EOL variant
    doc += b'via group'
    doc += eol_group(0x04)

    # ---------------- prefix packets ----------------
    summary = summary_packet([
        (5,  b'Author', b'WP Author'),            # AUTHOR -> initial-creator
        (46, b'Subject', b'WP Subject'),          # SUBJECT -> dc:subject
        (33, b'Publisher', b'WP Publisher'),      # PUBLISHER -> dc:publisher
        (10, b'Category', b'WP Category'),        # CATEGORY -> dc:type
        (26, b'Keywords', b'wpk1 wpk2'),          # KEYWORDS -> meta:keyword
        (27, b'Language', b'en-US'),              # LANGUAGE -> dc:language
        (1,  b'Abstract', b'WP abstract text'),   # ABSTRACT -> dc:description
        (48, b'Typist', b'WP Typist'),            # TYPIST -> dc:creator
    ])

    packets = [
        (0x00, 0x12, summary),                        # id 1
        (0x00, 0x08, gen_text(b'HDRTXT page')),       # id 2
        # footer holds a literal page-number display reference
        # (0xDA/0x04 + 0xDA/0x05) -> insertField "text:page-number"
        (0x00, 0x08, gen_text(b'FTRTXT pg '
                             + group(0xDA, 0x04, bytes([0]))
                             + group(0xDA, 0x05)
                             + b' tail')),            # id 3
        (0x00, 0x08, gen_text(b'HDRBTXT even')),      # id 4
        (0x00, 0x08, gen_text(b'Foot note text.')),   # id 5
        (0x00, 0x08, gen_text(b'End note text.')),    # id 6
        (0x00, 0x55, font_descriptor('Coverage Serif')),  # id 7
    ]

    idx_hdr_off = 0x18
    idx_hdr = struct.pack('<HH', 0, len(packets) + 1) + bytes(10)

    def indice(flags, typ, data_off, data):
        return struct.pack('<BBHHII', flags, typ, 1, 0, len(data), data_off)

    data_off = idx_hdr_off + 14 + len(packets) * 14
    indices = b''
    payload = b''
    for flags, typ, data in packets:
        indices += indice(flags, typ, data_off + len(payload), data)
        payload += data
    doc_off = data_off + len(payload)

    hdr = bytearray(idx_hdr_off)
    hdr[0] = 0xFF
    hdr[1:4] = b'WPC'
    struct.pack_into('<I', hdr, 4, doc_off)
    hdr[8:12] = bytes([0x01, 0x0A, 0x02, 0x01])
    struct.pack_into('<H', hdr, 12, 0)                  # encryption
    struct.pack_into('<H', hdr, 14, idx_hdr_off)
    total = doc_off + len(doc)
    struct.pack_into('<H', hdr, 20, total)              # file size field

    blob = bytes(hdr)
    assert len(blob) == idx_hdr_off
    blob += idx_hdr + indices + payload
    assert len(blob) == doc_off
    blob += doc
    assert len(blob) == total
    return blob


wpd = build()
path = os.path.join(OUT, 'cov_rich.wpd')
with open(path, 'wb') as f:
    f.write(wpd)
print('wrote', path)

# structured-container twin: "PerfectOffice_MAIN" inside a zip — same
# importer code path as a real OLE compound file (gsf falls back from
# msole to zip for any structured container)
path = os.path.join(OUT, 'cov_ole.wpd')
with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr('PerfectOffice_MAIN', wpd)
print('wrote', path)
