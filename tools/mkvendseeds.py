#!/usr/bin/env python3
"""Generate the vendored-parser fuzz seed corpus for VEND02.

Writes minimal-valid + edge-case inputs for the direct vendored-API
libFuzzer targets (fuzz_libwps / fuzz_libwpg):

    fuzz/corpus/libwps/   MS Works / Lotus / Quattro / MS Write raw
                          headers (libwps WPSHeader::constructHeader
                          magic prefixes, per its own detection table)
    fuzz/corpus/libwpg/   WPG1/WPG2 headers (libwpg WPGHeader layout:
                          FF 'W' 'P' 'C', u32 startOfDocument,
                          productType=1, fileType=0x16, version)

The libwpd target reuses fuzz/corpus/wpd (real WP6 files) — those are
copied to fuzz/corpus/libwpd by this script too so every target has a
self-contained corpus dir matching tools/check-fuzz.sh's mapping.

Run: python3 tools/mkvendseeds.py
"""
import os
import shutil
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORPUS = os.path.join(ROOT, "fuzz", "corpus")


def write(rel, data):
    path = os.path.join(CORPUS, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    print("wrote %s (%d bytes)" % (rel, len(data)))


# ---------- libwps raw-format headers (WPSHeader::constructHeader) --

# Works v2 text: val[0] < 6 && val[1] == 0xFE
write("libwps/seed_works_v2.wps", bytes([0x02, 0xFE]) + b"Works v2 text body" + bytes(64))

# Works v1 database: val[0] == 0xFF && val[1] == 0x54
write("libwps/seed_works_db.wks", bytes([0xFF, 0x54]) + bytes(range(96)))

# Works spreadsheet v3: val[0] == 0xFF && val[1] == 0 && val[2] == 2
write("libwps/seed_works_ss.wks", bytes([0xFF, 0x00, 0x02]) + bytes(128))

# Lotus spreadsheet: 00 00 1A
write("libwps/seed_lotus.wk1", bytes([0x00, 0x00, 0x1A, 0x00, 0x10]) + bytes(256))

# Quattro Pro wq1/wq2: 00 00 02 00 20|21 51
write("libwps/seed_quattro.wq1", bytes([0x00, 0x00, 0x02, 0x00, 0x20, 0x51]) + bytes(128))

# Microsoft Write: 31 BE 00 00 00 AB then u16 == 0 (not DosWord)
write("libwps/seed_write.wri",
      bytes([0x31, 0xBE, 0x00, 0x00, 0x00, 0xAB, 0x00, 0x00]) + bytes(512))

# PocketWord: 7B 5C 70 77 69 15  ({\pwi...)
write("libwps/seed_pocketword.pwd", bytes([0x7B, 0x5C, 0x70, 0x77, 0x69, 0x15]) + bytes(128))

# Multiplan v1: 08 E7
write("libwps/seed_multiplan.mp", bytes([0x08, 0xE7]) + bytes(64))

# edge cases: header-only truncation and pure garbage
write("libwps/edge_truncated.wps", bytes([0x02, 0xFE]))
write("libwps/edge_garbage.wps", bytes((i * 37 + 11) & 0xFF for i in range(512)))

# ---------- libwpg headers + records -------------------------------

def wpg_header(start_of_doc, major):
    # WPGHeader::load reads 26 bytes:
    #   0-3 identifier FF 'W' 'P' 'C', 4-7 u32 startOfDocument,
    #   8 productType, 9 fileType, 10 majorVersion, 11 minorVersion,
    #   12-13 u16 encryptionKey, 14-15 u16 startOfPacketData
    h = bytes([0xFF, 0x57, 0x50, 0x43])
    h += struct.pack("<I", start_of_doc)
    h += bytes([0x01, 0x16, major, 0x00])
    h += struct.pack("<H", 0)          # encryption key
    h += struct.pack("<H", 0)          # start of packet data
    h += bytes(26 - len(h))
    return h

def wpg_rec(rectype, payload):
    # WPG1 record: type byte, then variable-length integer; a length
    # <= 0xFE encodes itself — recordLength counts the length byte too
    # (m_recordEnd = tell() + len - 1 after reading the length)
    return bytes([rectype, len(payload) + 1]) + payload

# valid-ish WPG1: Start WPG (skip2 + w/h), Rectangle (x,y,w,h s16),
# End WPG, terminator 0x00
wpg1 = wpg_header(0x1A, 0x01)
wpg1 += wpg_rec(0x0F, bytes(2) + struct.pack("<HH", 1200, 800))   # Start WPG
wpg1 += wpg_rec(0x07, struct.pack("<hhhh", 10, 10, 500, 300))     # Rectangle
wpg1 += wpg_rec(0x10, b"")                                         # End WPG
wpg1 += b"\x00"
write("libwpg/seed_wpg1_rect.wpg", wpg1)

# WPG2: same header major=2; WPG2 records use a different prefix but
# the parser scans record headers defensively — a plausible blob is a
# fine seed
wpg2 = wpg_header(0x1A, 0x02)
wpg2 += bytes([0x0B]) + bytes(32)                                  # record-ish blob
wpg2 += b"\x00"
write("libwpg/seed_wpg2.wpg", wpg2)

# WPG1 with a group + colormap record for more parser reach
wpg1b = wpg_header(0x1A, 0x01)
wpg1b += wpg_rec(0x0F, bytes(2) + struct.pack("<HH", 2400, 1600))  # Start WPG
wpg1b += wpg_rec(0x0E, bytes(3 * 16))                              # Colormap
wpg1b += wpg_rec(0x06, struct.pack("<hhhh", 0, 0, 100, 100) * 2)   # Polyline-ish
wpg1b += wpg_rec(0x10, b"")
wpg1b += b"\x00"
write("libwpg/seed_wpg1_group.wpg", wpg1b)

# edge cases: header only, wrong magic, garbage
write("libwpg/edge_header_only.wpg", wpg_header(0x1A, 0x01))
write("libwpg/edge_badmagic.wpg", b"\xFFWPD" + bytes(64))
write("libwpg/edge_garbage.wpg", bytes((i * 53 + 7) & 0xFF for i in range(300)))

# ---------- libwpd: reuse the real WP6 seeds ------------------------

os.makedirs(os.path.join(CORPUS, "libwpd"), exist_ok=True)
for name in sorted(os.listdir(os.path.join(CORPUS, "wpd"))):
    src = os.path.join(CORPUS, "wpd", name)
    dst = os.path.join(CORPUS, "libwpd", name)
    shutil.copyfile(src, dst)
    print("copied wpd/%s -> libwpd/%s" % (name, name))

# a WP6 header-shaped edge file: \xFF'WPC' prefix then truncation
write("libwpd/edge_truncated_hdr.wpd",
      bytes([0xFF, 0x57, 0x50, 0x43]) + bytes(12))

print("done")
