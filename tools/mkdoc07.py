#!/usr/bin/env python3
"""DOC07 synthetic fixtures: embedded pictures in .doc.

doc07_inline.doc  -- two Word8 inline pictures (0x01 chars):
                     PICF(mm=0x64) + OfficeArtInlineSpContainer payload
                     ([SpContainer][bare OfficeArtBlipPNG]) for PNG, and
                     [SpContainer][FBSE(embedded blip)] for JPEG.
doc07_float.doc   -- floating image (0x08 char): FSPA in PlcSpaMom,
                     OfficeArtDggContainer in 1Table whose BStore has an
                     FBSE with foDelay into the Data stream (delayed blip).
doc07_picf.doc    -- old-style inline picture: PICF(mm=0x62) followed by
                     a pre-97 "old graphic header" + raw 8bpp BMP data,
                     exercised through the escher-wrapping path.
"""
import struct
import zlib
import gi
gi.require_version('Gsf', '1')
from gi.repository import Gsf
import olefile

import os
import sys
OUT = sys.argv[1] if len(sys.argv) > 1 else '/tmp'
os.makedirs(OUT, exist_ok=True)
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   '..', 'test', 'wp', 'Word97Test.doc')


def p16(b, o, v): struct.pack_into('<H', b, o, v)
def p32(b, o, v): struct.pack_into('<I', b, o, v)
def sprm(op, operand): return struct.pack('<H', op) + operand


# --- images -------------------------------------------------------------

def mkpng(w, h, rgb):
    def chunk(t, d):
        return (struct.pack('>I', len(d)) + t + d
                + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff))
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)
    raw = b''.join(b'\x00' + rgb * w for _ in range(h))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr)
            + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def mkjpeg():
    """real 40x24 jpeg (requires PIL; falls back to a minimal stream)"""
    try:
        from PIL import Image
        import io
        im = Image.new('RGB', (40, 24), (255, 180, 0))
        buf = io.BytesIO()
        im.save(buf, 'JPEG')
        return buf.getvalue()
    except ImportError:
        return bytes.fromhex(
            'ffd8ffe000104a46494600010101006000600000'
            'ffdb0043000302020302020303030304030304050805050404050a07070608'
            '0c0a0c0c0b0a0b0b0d0e12100d0e110e0b0b1016101113141515150c0f171816'
            '141812141514ffc0000b080001000101011100ffc400140001000000000000'
            '0000000000000000000000000008ffda0008010100003f00d2cf20ffd9')


PNG_RED = mkpng(64, 32, b'\xff\x00\x00')     # 64x32 red
PNG_BLUE = mkpng(48, 48, b'\x00\x00\xff')    # 48x48 blue
JPEG = mkjpeg()


# --- escher helpers ------------------------------------------------------

def rec(ver, inst, fbt, payload):
    return struct.pack('<HHI', (inst << 4) | ver, fbt, len(payload)) + payload


def bare_blip(fbt, inst, data):
    """OfficeArtBlip record: rgbUid1 + [rgbUid2] + tag + data"""
    body = bytes(16)
    if inst & 1:
        body += bytes(16)
    body += b'\xff' + data
    return rec(0, inst, fbt, body)


def fbse_rec(fo_delay, embedded=b'', name=b''):
    """OfficeArtFBSE record (fbt 0xF007); blip embedded or delayed"""
    size = len(embedded)
    fbse = struct.pack('<BB16sHIIIBBBB',
                       6, 0,           # btWin32 (PNG), btMacOS
                       bytes(16),      # rgbUid
                       0xff,           # tag
                       size, 1,        # size, cRef
                       fo_delay,       # foDelay
                       0,              # usage
                       len(name),      # cbName (bytes)
                       0, 0)           # unused
    assert len(fbse) == 36
    return rec(0, 0, 0xF007, fbse + name + embedded)


def fopte(pid, op, fbid=0):
    return struct.pack('<HI', pid | (fbid << 14), op)


def sp_rec(spid, sptype=0x4B, flags=0x00000A00):
    # msofbtSp: spid(4) + flags(4); inst = shape type (75 = picture frame)
    return rec(0, sptype, 0xF00A, struct.pack('<II', spid, flags))


def spcontainer(*children):
    return rec(0xF, 0, 0xF004, b''.join(children))


# --- PICF ----------------------------------------------------------------

def picf(mm, payload, dxagoal=1440, dyagoal=720, mx=1000, my=1000,
         cropt=0, cropb=0, cropl=0, cropr=0, name=b''):
    """Word8 PICF: 68-byte header + payload."""
    hdr = struct.pack('<IH', 68 + len(payload), 68)          # lcb, cbHeader
    hdr += struct.pack('<hhhh', mm, 0, 0, 0)                 # MFP
    hdr += bytes(14)                                          # obj
    hdr += struct.pack('<hh', dxagoal, dyagoal)
    hdr += struct.pack('<HH', mx, my)
    hdr += struct.pack('<hhhh', cropl, cropt, cropr, cropb)
    hdr += b'\x00'                                            # bits
    hdr += b'\x00'                                            # bpp
    hdr += bytes(16)                                          # 4x Brc80
    hdr += struct.pack('<hh', 0, 0)                           # dxa/dyaOrigin
    hdr += struct.pack('<h', 0)                               # cProps
    assert len(hdr) == 68
    if mm == 0x66 and name:
        payload = bytes([len(name)]) + name + payload
        hdr = struct.pack('<IH', 68 + len(payload), 68) + hdr[6:]
    return hdr + payload


# --- document builder ----------------------------------------------------

def build(path, pieces, lid, data_stream=None, extra_fclcb=None,
          fcomplex=True):
    """pieces: (bytes, compressed?, papx_grpprl|None, chpx|None).
    data_stream: bytes for the 'Data' OLE stream.
    extra_fclcb: {pair_index: bytes} appended to 1Table."""
    ole = olefile.OleFileIO(SRC)
    wd = bytearray(ole.openstream('WordDocument').read())
    tbl = bytearray(ole.openstream('1Table').read())

    fcMin = 0x400
    pos = fcMin
    cps = [0]
    pcd_fcs = []
    para_fcs = [fcMin]
    nchars = 0
    for data, compressed, _, _ in pieces:
        n = len(data) // (1 if compressed else 2)
        pcd_fcs.append((pos * 2 | 0x40000000) if compressed else pos)
        pos += len(data)
        cps.append(cps[-1] + n)
        para_fcs.append(pos)
        nchars += n
    fcMac = pos
    ccpText = nchars

    new_wd = bytearray(wd[:fcMin])
    for data, compressed, _, _ in pieces:
        new_wd += data
    new_wd += b'\x00' * (0x600 - len(new_wd))

    papx = bytearray(512)
    for i, fc in enumerate(para_fcs):
        p32(papx, 4 * i, fc)
    papx[511] = len(pieces)
    new_wd += papx
    papx_pn = 3

    new_wd += b'\x00' * (4 * 512 - len(new_wd))

    # CHPX FKP at pn=4: rgfc[n+1] + rgb[n] offset bytes + chpx data + crun
    chpx_fkp = bytearray(512)
    for i, fc in enumerate(para_fcs):
        p32(chpx_fkp, 4 * i, fc)
    chpx_pos = 510                      # stack chpx records from the end
    for i, (_, _, _, chpx) in enumerate(pieces):
        if chpx is None:
            continue
        rec_b = bytes([len(chpx)]) + bytes(chpx)
        chpx_pos -= len(rec_b) + 1
        chpx_pos &= ~1
        chpx_fkp[chpx_pos:chpx_pos + len(rec_b)] = rec_b
        chpx_fkp[4 * (len(pieces) + 1) + i] = chpx_pos // 2
    chpx_fkp[511] = len(pieces)
    new_wd += chpx_fkp
    cbMac = len(new_wd)

    base = len(tbl)
    clx = bytearray()
    plcfpcd = bytearray()
    for cp in cps:
        plcfpcd += struct.pack('<I', cp)
    for i, fc in enumerate(pcd_fcs):
        plcfpcd += struct.pack('<BBIH', 0, 0, fc, 0)
    clx += b'\x02' + struct.pack('<I', len(plcfpcd)) + plcfpcd
    fcClx = base
    tbl += clx

    def bte_plc(fcs, pns):
        out = bytearray()
        for fc in fcs:
            out += struct.pack('<I', fc)
        for pn in pns:
            out += struct.pack('<I', pn)
        return out

    fcPapx = len(tbl); tbl += bte_plc([fcMin, para_fcs[-1]], [papx_pn])
    cbPapx = len(tbl) - fcPapx
    fcChpx = len(tbl); tbl += bte_plc([fcMin, fcMac], [4])
    cbChpx = len(tbl) - fcChpx
    fcSed = len(tbl)
    tbl += struct.pack('<II', 0, ccpText)
    tbl += struct.pack('<HIHI', 0, 0xffffffff, 0, 0xffffffff)

    pairvals = {}
    if extra_fclcb:
        for idx, blob in extra_fclcb.items():
            pairvals[idx] = (len(tbl), len(blob))
            tbl += blob

    p16(new_wd, 0x06, lid)
    flags = struct.unpack_from('<H', new_wd, 0x0A)[0]
    if fcomplex:
        flags |= 0x0004
    else:
        flags &= ~0x0004
    p16(new_wd, 0x0A, flags)
    p32(new_wd, 0x18, fcMin)
    p32(new_wd, 0x1C, fcMac)
    cbW = struct.unpack_from('<H', new_wd, 0x20)[0]
    rgLw_off = 0x20 + 2 + cbW * 2 + 2
    p32(new_wd, rgLw_off + 0, cbMac)        # cbMac
    p32(new_wd, rgLw_off + 3 * 4, ccpText)  # ccpText
    fc_off = rgLw_off + 22 * 4
    fclcb_off = fc_off + 2
    p32(new_wd, fclcb_off + 33 * 8, fcClx)
    p32(new_wd, fclcb_off + 33 * 8 + 4, len(clx))
    p32(new_wd, fclcb_off + 13 * 8, fcPapx)
    p32(new_wd, fclcb_off + 13 * 8 + 4, cbPapx)
    p32(new_wd, fclcb_off + 12 * 8, fcChpx)
    p32(new_wd, fclcb_off + 12 * 8 + 4, cbChpx)
    p32(new_wd, fclcb_off + 6 * 8, fcSed)
    p32(new_wd, fclcb_off + 6 * 8 + 4, 20)
    for idx, (fc, lcb) in pairvals.items():
        p32(new_wd, fclcb_off + idx * 8, fc)
        p32(new_wd, fclcb_off + idx * 8 + 4, lcb)

    out = Gsf.OutputStdio.new(path)
    doc = Gsf.OutfileMSOle.new(out)
    for name in ('\x01CompObj', '\x05SummaryInformation',
                 '\x05DocumentSummaryInformation'):
        ch = doc.new_child(name, False)
        ch.write(ole.openstream(name).read())
        ch.close()
    ch = doc.new_child('WordDocument', False)
    ch.write(bytes(new_wd))
    ch.close()
    ch = doc.new_child('1Table', False)
    ch.write(bytes(tbl))
    ch.close()
    if data_stream is not None:
        ch = doc.new_child('Data', False)
        ch.write(bytes(data_stream))
        ch.close()
    doc.close()
    print(f'wrote {path}: ccpText={ccpText} pieces={len(pieces)}')


# ==== doc07_inline.doc ====================================================
# Data stream: PICF(mm=0x64) + [SpContainer][bare PNG blip] then
# PICF(mm=0x66 w/ name) + [SpContainer][FBSE(embedded JPEG blip)]
png_sp = spcontainer(
    sp_rec(0x0801),
    rec(0, 1, 0xF00B, fopte(0x104, 0)))           # OPT, pib=0 (unused)
png_payload = png_sp + bare_blip(0xF01E, 0x6E0, PNG_RED)

jpg_sp = spcontainer(sp_rec(0x0802))
jpg_payload = jpg_sp + fbse_rec(0xffffffff,
                                bare_blip(0xF01D, 0x46A, JPEG))

data = picf(0x64, png_payload, dxagoal=1440, dyagoal=720)
fc_pic1 = 0
fc_pic2 = len(data)
data += picf(0x66, jpg_payload, dxagoal=720, dyagoal=720,
             name=b'photo.jpg')

pieces = [
    (b'Inline pictures follow.\r', True, None, None),
    (b'\x01', True, None, sprm(0x6A03, struct.pack('<I', fc_pic1))),
    (b' middle text \r', True, None, None),
    (b'\x01', True, None, sprm(0x6A03, struct.pack('<I', fc_pic2))),
    (b' end.\r', True, None, None),
]
build(OUT + '/doc07_inline.doc', pieces, 0x0409, data_stream=data)

# ==== doc07_float.doc =====================================================
# Data stream holds the blip record at foDelay offset.
fo_delay = 0
data_f = bare_blip(0xF01E, 0x6E0, PNG_BLUE)

# DggContainer: Dgg + BstoreContainer(FBSE w/ foDelay)
dgg_children = (rec(0, 0, 0xF006, struct.pack('<II', 1, 0x1000))
                + rec(0xF, 0, 0xF001, fbse_rec(fo_delay)))
dgg = rec(0xF, 0, 0xF000, dgg_children)

# DgContainer: Dg + SpgrContainer(group sp + picture sp w/ pib=1)
dg_children = (rec(0, 1, 0xF008, struct.pack('<II', 2, 0x1000))
               + rec(0xF, 1, 0xF003,
                     spcontainer(
                         rec(0, 0, 0xF009, bytes(16)),  # spgr rc
                         sp_rec(0x0400, 0x5, 0x0A))     # group shape
                     + spcontainer(
                         sp_rec(0x0401, 0x4B, 0x0A),    # picture frame
                         rec(0, 1, 0xF00B,
                             fopte(0x104, 1, 1)))))    # pib=1, fBid
dgcont = rec(0xF, 1, 0xF002, dg_children)

# FSPA for the floating shape, anchored at cp of the \x08 char.
# pieces: [text(23 cps incl \r)] [\x08 at cp 22] -> wait, compute below
lead = b'Floating picture: '
pre_cps = len(lead)
fspa = struct.pack('<IiiiiHi',
                   0x0401,          # spid
                   2880, 720,       # xaLeft, yaTop
                   2880 + 960,      # xaRight  (0.67in wide)
                   720 + 960,       # yaBottom
                   (0 << 0) | (0 << 1) | (0 << 3) | (2 << 5) | (0 << 9),
                   0)               # cTxbx
plcfspa = struct.pack('<II', pre_cps, pre_cps + 1) + fspa

pieces_f = [
    (lead, True, None, None),
    (b'\x08', True, None, sprm(0x0855, b'\x01')),   # sprmCFSpec
    (b' after the picture.\r', True, None, None),
]
build(OUT + '/doc07_float.doc', pieces_f, 0x0409, data_stream=data_f,
      extra_fclcb={40: plcfspa, 50: dgg + dgcont})

# ==== doc07_picf.doc ======================================================
# old-format picture: 0x00090001 old graphic header + BMP data, mm=0x62
bih = struct.pack('<IiiHHIIiiII', 40, 4, 4, 1, 8, 0, 16, 2835, 2835, 0, 0)
palette = b''.join(struct.pack('<BBBB', i, i, 255 - i, 0)
                   for i in range(256))
bits = bytes([1, 2, 3, 4] * 4)
bmp_data = bih + palette + bits

old_hdr = (struct.pack('<I', 0x00090001) + struct.pack('<H', 0x0300)
           + struct.pack('<I', 0) + struct.pack('<H', 0)
           + struct.pack('<I', 0) + struct.pack('<H', 0)
           + struct.pack('<I', 4)                    # entry -> lene2=2
           + struct.pack('<H', 0x0f43)               # BMP marker
           + struct.pack('<I', 0x00cc0020)
           + struct.pack('<H', 0)                    # 0x0f43 extra word
           + struct.pack('<HH', 4, 4)
           + struct.pack('<I', 0)
           + struct.pack('<HH', 4, 4)
           + struct.pack('<I', 0))

data_p = picf(0x62, old_hdr + bmp_data, dxagoal=576, dyagoal=576)
pieces_p = [
    (b'Old picture: ', True, None, None),
    (b'\x01', True, None, sprm(0x6A03, struct.pack('<I', 0))),
    (b' done.\r', True, None, None),
]
build(OUT + '/doc07_picf.doc', pieces_p, 0x0409, data_stream=data_p)
