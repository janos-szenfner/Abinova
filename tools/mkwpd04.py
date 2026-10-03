#!/usr/bin/env python3
"""WP04 synthetic fixture: hyperlink + page-number field + comment +
text box in one .wpd (WP6 format).

The document stream reads "See <comment> page <field> <textbox>
<linked image> end" where:

- the comment is a WP6 character group (0xD4/0x1D) whose prefix id
  references a CommentAnnotationPacket (index type 0x1B) -> a
  GeneralTextPacket (index type 0x08) holding the comment body
  subdocument -> libwpd openComment/closeComment.
- the field is a display-number-reference group (0xDA/0x05) ->
  insertField(text:page-number).
- the text box is a box group (0xDF) with content type 0x01 and a
  GeneralTextPacket prefix -> openFrame/openTextBox/closeTextBox.
- the linked image is a graphics box (0x03) whose prefix ids carry a
  HyperlinkPacket (type 0x07, UTF-16 target) plus the usual
  GraphicsFilenamePacket -> CachedFileDataPacket chain ->
  openLink/openFrame/insertBinaryObject/closeFrame/closeLink.

Packet ids are 1-based ordinals in the index:
  1 general text (comment body)   4 graphics filename (child 5)
  2 comment annotation (pid 1)    5 cached file data (PNG)
  3 general text (textbox body)   6 hyperlink
"""
import struct
import zlib
import os
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else '/tmp'
os.makedirs(OUT, exist_ok=True)


def mkpng(w, h, rgb):
    def chunk(t, d):
        return (struct.pack('>I', len(d)) + t + d
                + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff))
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)
    raw = b''.join(b'\x00' + rgb * w for _ in range(h))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr)
            + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


PNG = mkpng(64, 48, b'\x20\x20\xcc')  # 64x48 blue-ish


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


def box_function(prefix_ids, content_type, genpos=None, width_wpu=2400,
                 height_wpu=1800, hoff_wpu=1200, voff_wpu=2400):
    """WP6 box group (0xDF), page-anchored kind; the real anchor comes
    from generalPositioningFlags (None -> page). Sizes in WPUs
    (1/1200")."""
    content = bytearray(14)                    # reserved
    content += b'\x00\x00'                     # override+wrap size (unused)
    content += b'\x00\x00'                     # override size (unused)

    # positioning override block (bit 0x4000 of the override flags)
    flags2 = 0x2000 | 0x1000 | 0x0800 | 0x0400
    body = b''
    if genpos is not None:
        flags2 |= 0x4000
        body += bytes([0xFF, genpos])          # mask / data
    body += bytes([0x00]) + struct.pack('<h', hoff_wpu) + bytes([0, 0])
    body += bytes([0x00]) + struct.pack('<h', voff_wpu)
    body += bytes([0x00]) + struct.pack('<H', width_wpu)
    body += bytes([0x00]) + struct.pack('<H', height_wpu)
    pos = struct.pack('<H', flags2) + body

    content += struct.pack('<H', 0x4000 | 0x2000)  # positioning+content bits
    content += struct.pack('<H', len(pos)) + pos

    # content override block (bit 0x2000): box content type
    content += struct.pack('<HHB', 3, 0x4000, content_type)
    return group(0xDF, 0x02, bytes(content), prefix_ids=prefix_ids)


def gen_text(text):
    """GeneralTextPacket payload: one text block holding a WP6
    subdocument stream (text + hard EOL)."""
    stream = text + b'\xcc'
    return struct.pack('<H', 1) + b'\x00' * 4 + struct.pack('<I', len(stream)) + stream


def build():
    doc = bytearray()
    doc += b'See '
    doc += group(0xD4, 0x1D, prefix_ids=[2])   # comment -> ann packet 2 -> text 1
    doc += b' page '
    doc += group(0xDA, 0x05)                   # page number display off -> field
    doc += b' '
    doc += box_function([3], 0x01, voff_wpu=2400)   # text box -> gen text 3
    doc += b' '
    doc += box_function([4, 6], 0x03, genpos=0x01, voff_wpu=5400)  # linked image
    doc += b' end'
    doc += b'\xcc'                             # hard EOL

    packets = [
        (0x00, 0x08, gen_text(b'A comment.')),              # id 1
        (0x00, 0x1B, struct.pack('<HHB', 1, 1, 0)),         # id 2 -> text 1
        (0x00, 0x08, gen_text(b'Text in a box.')),          # id 3
        (0x01, 0x40, struct.pack('<HH', 1, 5)),             # id 4 -> child 5
        (0x00, 0x6F, PNG),                                  # id 5
        (0x00, 0x07, 'https://example.com/'.encode('utf-16-le')
         + b'\x00\x00'),                                  # id 6
    ]

    # index header + indice records
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
    struct.pack_into('<H', hdr, 12, 0)         # encryption
    struct.pack_into('<H', hdr, 14, idx_hdr_off)
    total = doc_off + len(doc)
    struct.pack_into('<H', hdr, 20, total)     # file size field

    blob = bytes(hdr)
    assert len(blob) == idx_hdr_off
    blob += idx_hdr + indices + payload
    assert len(blob) == doc_off
    blob += doc
    assert len(blob) == total
    return blob


path = os.path.join(OUT, 'wpd04.wpd')
with open(path, 'wb') as f:
    f.write(build())
print('wrote', path)
