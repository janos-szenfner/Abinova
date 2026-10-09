#!/usr/bin/env python3
"""COVD03 synthetic .doc fixtures: exercise the DOC05-DOC10 importer
machinery that no committed corpus file reaches (per-file coverage of
ie_imp_MsWord_97.cpp was ~45%).

cov_table.doc  -- two tables: 2x2 with cell borders (TC80 brcs), full-color
                  cell shading (sprmTDefTableShdRaw), per-cell padding
                  (sprmTCellPadding), header row (sprmTTableHeader), exact
                  row height (sprmTDyaRowHeight), a vertical merge
                  (TCGRF.vertMerge restart/continue); then a single row
                  whose two cells are horizontally merged (TCGRF.horzMerge
                  first/covered).  Covers table_open/push/pop/row/cell,
                  _build_ColumnWidths, s_cellBoundIndex, s_cellMarginProp,
                  sConvertLineStyle, the covered-cell and vmerge span logic.
cov_list.doc   -- a 3-level numbered list (nfc decimal/lowerLetter/
                  lowerRoman, "%1.%2.%3." number text) and a single-level
                  bullet list; PlfLfo carries an LFOLVL startAt+formatting
                  override for the first LFO.  Covers wvAssembleListPAP,
                  s_mapDocToAbiListType/Delim/Style, s_fieldFontForList.
cov_marks.doc  -- a named bookmark (Sttbfbkmk + Plcfbkf/Plcfbkl), an
                  auto-numbered footnote and endnote (Plcffnd*/Plcfend*
                  PLCFs + ccpFtn/ccpEdn stories) and a supported
                  " TOC \\o "1-3" " field.  Covers _handleBookmarks/
                  _insertBookmark/_getBookmarkName, _handleNotes/
                  _insertFootnote/_insertEndnote and _insertTOC.
cov_annot.doc  -- two comments: one anchored to a range (SttbfAtnBkmk ->
                  PlcfAtnbkf/PlcfAtnbkl) and one point comment, with a
                  GrpXstAtnOwners author + LPXCharBuffer9 initials.
                  Covers _handleAnnotations, _insertAnnotationStart,
                  _insertAnnotationIfAppropriate, _handleAnnotationsText.

Extend the mkdoc07.py build() scaffolding: per-paragraph grpprl is now
written as real PAPX records into the PAPX FKP (BX13 entries), and the
FIB's story ccps (ccpFtn/ccpAtn/ccpEdn/...) can be set so subdocument
text following the main story decodes as notes/comments.
"""
import struct
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


# --- sprm helpers ---------------------------------------------------------

ITAP = lambda d=1: sprm(0x6649, struct.pack('<i', d))      # sprmPItap
FTTP = sprm(0x2417, b'\x01')                                # sprmPFTtp
ILFO = lambda n: sprm(0x460B, struct.pack('<h', n))         # sprmPIlfo
ILVL = lambda n: sprm(0x260A, bytes([n]))                   # sprmPIlvl


def brc80(linewidth=4, brctype=1, ico=0, space=0, shadow=0, frame=0):
    """4-byte BRC inside a TC80: dptLineWidth, brcType, ico, flags."""
    return bytes([linewidth, brctype, ico,
                  (space & 0x1f) | ((shadow & 1) << 5) | ((frame & 1) << 6)])


def tc80(horzMerge=0, vertMerge=0, vertAlign=0, textFlow=0, ftsWidth=0,
         fFitText=0, fNoWrap=0, fHideMark=0, wWidth=0, brcs=None):
    """20-byte TC80: TCGRF(2) + wWidth(2) + 4x BRC(4)."""
    grf = ((horzMerge & 3) | ((textFlow & 7) << 2) | ((vertMerge & 3) << 5)
           | ((vertAlign & 3) << 7) | ((ftsWidth & 7) << 9)
           | ((fFitText & 1) << 12) | ((fNoWrap & 1) << 13)
           | ((fHideMark & 1) << 14))
    out = struct.pack('<Hh', grf, wWidth)
    for b in (brcs if brcs is not None else [b'\x00' * 4] * 4):
        out += b
    assert len(out) == 20
    return out


def tdeftable(rgdxa, tcs):
    """sprmTDefTable operand: len(2) + itcMac(1) + rgdxa + rgtc.
    wv's wvApplysprmTDefTable anchors its oldpos one byte into the
    len field, so cb must be one more than the trailing body size or
    the TC block is misdetected as Word6-sized and the operand
    desyncs.  len = itc(1) + rgdxa + rgtc + 1."""
    itc = len(tcs)
    body = bytes([itc]) + b''.join(struct.pack('<h', x) for x in rgdxa)
    body += b''.join(tcs)
    return sprm(0xD608, struct.pack('<H', len(body) + 1) + body)


def tdeftable_shd(shds):
    """sprmTDefTableShdRaw: len(1) + nop x SHD10 (cvFore, cvBack, ipat)."""
    body = b''.join(struct.pack('<IIH', f, b, ipat) for f, b, ipat in shds)
    return sprm(0xD670, bytes([len(body)]) + body)


def tcellpadding(first, lim, mask, w, fts=3):
    """sprmTCellPadding: len + itcFirst + itcLim + mask + fts + w."""
    return sprm(0xD632, bytes([6, first, lim, mask, fts])
                + struct.pack('<h', w))


THEAD = sprm(0x3404, b'\x01')                          # sprmTTableHeader
TROWH = lambda tw: sprm(0x9407, struct.pack('<h', tw))  # sprmTDyaRowHeight


# --- OLE/FIB writer ---------------------------------------------------------

def build(path, pieces, lid, ccps=None, data_stream=None,
          extra_fclcb=None, fcomplex=True, ccp_main=None):
    """pieces: (bytes, compressed?, papx_grpprl|None, chpx|None) -- each
    piece is one paragraph (must end in 0x0D or a 0x07 cell/row mark).
    ccps: {fibRgLw index: ccp} for subdocument stories
    (4=ftn, 5=hdr, 6=mcr, 7=atn, 8=edn, 9=txbx, 10=hdrtxbx).
    ccp_main: chars in the main story when pieces also carry
    subdocument text (defaults to all pieces)."""
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
    ccpText = nchars if ccp_main is None else ccp_main

    new_wd = bytearray(wd[:fcMin])
    for data, compressed, _, _ in pieces:
        new_wd += data
    new_wd += b'\x00' * (0x600 - len(new_wd))

    # PAPX FKP at pn=3: rgfc[n+1] + BX13[n] (offset + 12-byte PHE) +
    # papx records stacked from the end + crun.
    papx = bytearray(512)
    for i, fc in enumerate(para_fcs):
        p32(papx, 4 * i, fc)
    bx_base = 4 * (len(pieces) + 1)
    papx_pos = 510
    for i, (_, _, grpprl, _) in enumerate(pieces):
        if not grpprl:
            continue
        grp = bytes(grpprl)
        if len(grp) % 2:
            grp += b'\x00'
        cb = 2 + len(grp)          # istd + grpprl, in bytes
        cw = cb // 2               # cb in words
        rec_b = bytes([cw]) + struct.pack('<H', 0) + grp
        papx_pos -= len(rec_b)
        papx_pos &= ~1
        papx[papx_pos:papx_pos + len(rec_b)] = rec_b
        papx[bx_base + 13 * i] = papx_pos // 2
    papx[511] = len(pieces)
    new_wd += papx
    papx_pn = 3

    new_wd += b'\x00' * (4 * 512 - len(new_wd))

    # CHPX FKP at pn=4 (same as mkdoc07).
    chpx_fkp = bytearray(512)
    for i, fc in enumerate(para_fcs):
        p32(chpx_fkp, 4 * i, fc)
    chpx_pos = 510
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
    for idx, ccp in (ccps or {}).items():
        p32(new_wd, rgLw_off + idx * 4, ccp)
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
    print(f'wrote {path}: ccpText={ccpText} pieces={len(pieces)} '
          f'ccps={ccps}')


# ==== cov_table.doc =========================================================
# table 1: 2 rows x 2 cols; borders, shading, padding, header row,
# exact row height, cell 0 vertically merged across both rows.
brd = brc80(linewidth=8, brctype=1)              # 1pt solid
grid = [0, 2880, 5760]                            # two 2in columns
row1_tap = (ITAP() + FTTP + tdeftable(grid, [
        tc80(vertMerge=3, brcs=[brd] * 4),        # vmerge restart
        tc80(brcs=[brd] * 4)]) + THEAD + TROWH(-720)
        + tcellpadding(0, 2, 0x0f, 120)
        + tdeftable_shd([(0, 0x0000FF00, 0),      # cell0 green back
                         (0, 0x000000FF, 0)]))    # cell1 red back
row2_tap = (ITAP() + FTTP + tdeftable(grid, [
        tc80(vertMerge=1, brcs=[brd] * 4),        # vmerge continue
        tc80(brcs=[brd] * 4)]))
# table 2: one row, two cells horizontally merged into one.
hrow_tap = (ITAP() + FTTP + tdeftable(grid, [
        tc80(horzMerge=2, brcs=[brd] * 4),
        tc80(horzMerge=1, brcs=[brd] * 4)]))

build(OUT + '/cov_table.doc', [
    (b'r1c1\x07', True, ITAP(), None),
    (b'r1c2\x07', True, ITAP(), None),
    (b'\x07', True, row1_tap, None),
    (b'r2c1\x07', True, ITAP(), None),            # covered by vmerge
    (b'r2c2\x07', True, ITAP(), None),
    (b'\x07', True, row2_tap, None),
    (b'gap\r', True, None, None),
    (b'wide cell\x07', True, ITAP(), None),
    (b'\x07', True, ITAP(), None),                # covered by hmerge
    (b'\x07', True, hrow_tap, None),
    (b'tail\r', True, None, None),
], 0x0409)


# ==== cov_list.doc ==========================================================
def lstf(lsid, simple=0):
    return (struct.pack('<II', lsid, 0) + bytes(18)
            + bytes([simple & 1, 0]))


def lvlf(istart, nfc, jc=0, cbPapx=0, cbChpx=0):
    return (struct.pack('<IBB', istart, nfc, jc & 3) + bytes(9)
            + bytes([0]) + struct.pack('<II', 0, 0)
            + bytes([cbChpx, cbPapx, 0, 0]))


def lvl(istart, nfc, numtext, chpx_grpprl=b''):
    out = lvlf(istart, nfc, cbChpx=len(chpx_grpprl))
    out += chpx_grpprl
    chars = numtext.encode('utf-16-le')
    out += struct.pack('<H', len(numtext)) + chars
    return out


# PlcfLst: count + LSTFs + LVLs (multi-level list first, then simple)
plst = struct.pack('<H', 2)
plst += lstf(0x40000001, simple=0) + lstf(0x40000002, simple=1)
# LST 0: 9 levels; level n uses nfc [0,4,2,0,...] -> 1.a.i.
nfcs = [0, 4, 2, 0, 0, 0, 0, 0, 0]
bold_chpx = sprm(0x0835, b'\x01')               # sprmCBold
for i in range(9):
    txt = ''.join('%%%d.' % (k + 1) for k in range(i + 1))
    plst += lvl(1, nfcs[i], txt,
                chpx_grpprl=bold_chpx if i == 0 else b'')
# LST 1: single bullet level (nfc 0x17, Wingdings bullet char 0xF0A7)
plst += lvl(1, 0x17, '\uf0a7')

# PlfLfo: count + LFOs + LFOLVLs (+LVL overrides)
plfo = struct.pack('<I', 2)
plfo += struct.pack('<IIIB3s', 0x40000001, 0, 0, 1, b'\x00' * 3)
plfo += struct.pack('<IIIB3s', 0x40000002, 0, 0, 0, b'\x00' * 3)
# LFO 0 has one LFOLVL override for ilvl 0: fStartAt|fFormatting ->
# start at 5 with its own LVL (decimal "%1.")
plfo += struct.pack('<IBBH', 5, 0, 0x30, 0)
plfo += lvl(5, 0, '%1.')

build(OUT + '/cov_list.doc', [
    (b'first item\r', True, ILFO(1) + ILVL(0), None),
    (b'second level\r', True, ILFO(1) + ILVL(1), None),
    (b'third level\r', True, ILFO(1) + ILVL(2), None),
    (b'bullet\r', True, ILFO(2) + ILVL(0), None),
    (b'tail\r', True, None, None),
], 0x0409, extra_fclcb={73: plst, 74: plfo})


# ==== cov_marks.doc =========================================================
def sttbf16(strings, extra=b'', extradatalen=0):
    """STTBF of UTF-16 strings + per-string extra data."""
    out = struct.pack('<HHH', 0xFFFF, len(strings), extradatalen)
    for s, x in zip(strings, extra or [b''] * len(strings)):
        b = s.encode('utf-16-le')
        out += struct.pack('<H', len(s)) + b + x
    return out


def plcf(cps, item_bytes):
    """U32 cps followed by per-item bytes."""
    return b''.join(struct.pack('<I', c) for c in cps) + item_bytes


# main text: 'Para one.\r' + bookmarked span + footnote ref 0x02 +
# endnote ref 0x02 + a supported " TOC \o "1-3" " field.
# cp map (each piece is one paragraph/run boundary):
pieces = [
    (b'Para one.\r', True, None, None),          # cps 0-9
    (b'See bookmarked ', True, None, None),      # 10-24
    (b'text', True, None, None),                 # 25-28  (bm range)
    (b' and note', True, None, None),            # 29-37
    (b'\x02', True, None, sprm(0x0855, b'\x01')), # 38 ftn ref (CFSpec)
    (b' plus end', True, None, None),            # 39-47
    (b'\x02', True, None, sprm(0x0855, b'\x01')), # 48 edn ref
    (b' too.\r', True, None, None),              # 49-54
    (b'\x13', True, None, sprm(0x0855, b'\x01')), # 55 field begin
    (b' TOC \\o "1-3" ', True, None, None),      # 56-69 instr
    (b'\x14', True, None, sprm(0x0855, b'\x01')), # 70 sep
    (b'Contents', True, None, None),             # 71-78 result
    (b'\x15', True, None, sprm(0x0855, b'\x01')), # 79 end
    (b' after\r', True, None, None),             # 80-86
    # --- footnote story (ccp 87..101) ---
    (b'\x02', True, None, sprm(0x0855, b'\x01')), # 87 ftn mark
    (b'footnote body\r', True, None, None),      # 88-101
    # --- endnote story (ccp 102..115) ---
    (b'\x02', True, None, sprm(0x0855, b'\x01')), # 102 edn mark
    (b'endnote body\r', True, None, None),       # 103-115
]

ccpText_n = 87
bkmk = sttbf16(['bmOne'])
pbkf = plcf([25, 29], struct.pack('<hH', 0, 0))  # 1 BKF: ibkl=0
pbkl = plcf([29, ccpText_n], struct.pack('<h', 0))  # 1 BKL: ibkf=0

# footnotes: ref plc = n+1 cps + n U16 FRD type flags (1 = auto-number)
fnd_ref = plcf([38, ccpText_n], struct.pack('<H', 1))
# ftn txt plc = n+2 cps within the ftn story
fnd_txt = plcf([0, 15, 15], b'')
# endnotes: same layout, story cp-relative
end_ref = plcf([48, ccpText_n], struct.pack('<H', 1))
end_txt = plcf([0, 14, 14], b'')

build(OUT + '/cov_marks.doc', pieces, 0x0409,
      ccps={4: 15, 8: 14},                        # ccpFtn=15, ccpEdn=14
      ccp_main=ccpText_n,
      extra_fclcb={2: fnd_ref, 3: fnd_txt, 21: bkmk, 22: pbkf,
                   23: pbkl, 46: end_ref, 47: end_txt})


# ==== cov_annot.doc =========================================================
# 'Normal text\r' + 'Commented span' + 0x05 + ' more.\r' + 'Point\x05 here.\r'
# story cps: atn bodies live after the main text (ccpAtn).
#   comment 0: ranged anchor over 'Commented span' (lTagBkmk=7)
#   comment 1: point comment (lTagBkmk=-1)
main = [
    (b'Normal text\r', True, None, None),        # cps 0-11
    (b'Commented span', True, None, None),       # 12-25
    (b'\x05', True, None, sprm(0x0855, b'\x01')), # 26 ref mark 0
    (b' more.\r', True, None, None),             # 27-33
    (b'Point', True, None, None),                # 34-38
    (b'\x05', True, None, sprm(0x0855, b'\x01')), # 39 ref mark 1
    (b' here.\r', True, None, None),             # 40-46
]
atns = [
    (b'\x05', True, None, sprm(0x0855, b'\x01')), # 47 body0 mark
    (b'first comment\r', True, None, None),      # 48-61
    (b'\x05', True, None, sprm(0x0855, b'\x01')), # 62 body1 mark
    (b'second comment\r', True, None, None),     # 63-77
]
ccpText_a = 47
# PlcfandRef: n+1 cps + n ATRDPre10 (30 bytes each)
def atrd(ltag, ibst, initials):
    xst = [len(initials)] + [ord(c) for c in initials]
    xst += [0] * (10 - len(xst))
    return (b''.join(struct.pack('<H', c) for c in xst)
            + struct.pack('<hHHl', ibst, 0, 0, ltag))


and_ref = plcf([26, 39, ccpText_a],
               atrd(7, 0, 'DV') + atrd(-1, 0, 'DV'))
# PlcfandTxt carries n+2 cps (trailing story terminator, like fndTxt)
and_txt = plcf([0, 15, 31, 31], b'')              # bodies [0,15) [15,31)

# SttbfAtnBkmk: one entry whose extradata ATNBE carries lTag=7;
# PlcfAtnbkf/PlcfAtnbkl map it to cps [12, 26).
atnbkmk = sttbf16([''], extra=[struct.pack('<HIi', 0, 7, 0)],
                  extradatalen=10)
atnbkf = plcf([12, 26], struct.pack('<hH', 0, 0))
atnbkl = plcf([26, ccpText_a], struct.pack('<h', 0))

# GrpXstAtnOwners: a run of XSTs (cch + U16 chars)
owners = struct.pack('<H', 8) + 'Devin QA'.encode('utf-16-le')

build(OUT + '/cov_annot.doc', main + atns, 0x0409,
      ccps={7: 32},                               # ccpAtn=32
      ccp_main=ccpText_a,
      extra_fclcb={4: and_ref, 5: and_txt, 36: owners,
                   37: atnbkmk, 42: atnbkf, 43: atnbkl})
