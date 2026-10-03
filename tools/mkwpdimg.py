#!/usr/bin/env python3
"""WP03 synthetic fixtures: embedded pictures in .wpd (WP6 format).

Each fixture is "Before <box> after" where the box is a WP6 box-group
function (0xDF) whose prefix ids reference a GraphicsFilenamePacket
(index type 0x40) -> GraphicsCachedFileDataPacket (index type 0x6F)
holding a PNG. libwpd replays that as
openFrame -> insertBinaryObject -> closeFrame.

wpd_img_page.wpd -- page-anchored figure box (generalPositioningFlags
                    left at 0 -> libwpd "char" anchor workaround)
wpd_img_para.wpd -- paragraph-anchored box (general positioning
                    override data=0x01 -> anchor-type "paragraph")
wpd_img_char.wpd -- character-anchored box (override data=0x02 ->
                    anchor-type "as-char", an inline image)
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


PNG = mkpng(64, 48, b'\xcc\x20\x20')  # 64x48 red-ish


def box_function(genpos_data, width_wpu=2400, height_wpu=1800,
                 hoff_wpu=1200, voff_wpu=2400):
    """WP6 box group (0xDF), subgroup 0x02 (page-anchored box kind;
    the real anchor comes from generalPositioningFlags). Content is a
    graphics box (content type override = 0x03) sized in WPUs
    (1/1200")."""
    content = bytearray(14)                    # reserved
    content += b'\x00\x00'                     # override+wrap size (unused)
    content += b'\x00\x00'                     # override size (unused)

    # positioning override block (bit 0x4000 of the override flags)
    flags2 = 0x2000 | 0x1000 | 0x0800 | 0x0400
    body = b''
    if genpos_data is not None:
        flags2 |= 0x4000
        body += bytes([0xFF, genpos_data])     # mask / data
    body += bytes([0x00]) + struct.pack('<h', hoff_wpu) + bytes([0, 0])
    body += bytes([0x00]) + struct.pack('<h', voff_wpu)
    body += bytes([0x00]) + struct.pack('<H', width_wpu)
    body += bytes([0x00]) + struct.pack('<H', height_wpu)
    pos = struct.pack('<H', flags2) + body

    content += struct.pack('<H', 0x4000 | 0x2000)  # positioning+content bits
    content += struct.pack('<H', len(pos)) + pos

    # content override block (bit 0x2000): box content type = image
    cb = struct.pack('<HHB', 3, 0x4000, 0x03)
    content += cb

    size = 1 + 1 + 2 + 1 + 1 + 2 + 2 + len(content) + 2 + 1
    grp = bytearray()
    grp += b'\xDF'                             # WP6_TOP_BOX_GROUP
    grp += b'\x02'                             # page-anchored box
    grp += struct.pack('<H', size)
    grp += b'\x80'                             # flag: has prefix ids
    grp += b'\x01' + struct.pack('<H', 1)      # 1 prefix id: packet 1
    grp += struct.pack('<H', len(content)) + content
    grp += struct.pack('<H', size) + b'\xDF'   # trailer
    return bytes(grp)


def build(genpos_data):
    doc = bytearray()
    doc += b'Before '
    doc += box_function(genpos_data)
    doc += b' after'
    doc += b'\xcc'                             # hard EOL

    # index header + 2 index records (filename packet, cached data)
    idx_hdr_off = 0x18
    idx_hdr = struct.pack('<HH', 0, 3) + bytes(10)

    fn_pkt = struct.pack('<HH', 1, 2)          # 1 child: packet id 2
    pkt_off = idx_hdr_off + 14 + 2 * 14
    cached_off = pkt_off + len(fn_pkt)
    doc_off = cached_off + len(PNG)

    def indice(flags, typ, data_off, data):
        return struct.pack('<BBHHII', flags, typ, 1, 0, len(data), data_off)

    indices = (indice(0x01, 0x40, pkt_off, fn_pkt)
               + indice(0x00, 0x6F, cached_off, PNG))

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
    blob += idx_hdr + indices + fn_pkt + PNG
    assert len(blob) == doc_off
    blob += doc
    assert len(blob) == total
    return blob


for name, genpos in [('wpd_img_page', None),
                     ('wpd_img_para', 0x01),
                     ('wpd_img_char', 0x02)]:
    path = os.path.join(OUT, name + '.wpd')
    with open(path, 'wb') as f:
        f.write(build(genpos))
    print('wrote', path)
