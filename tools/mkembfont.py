#!/usr/bin/env python3
"""Build a .docx whose fontTable embeds an obfuscated (odttf) font.

usage: mkembfont.py FONT.TTF "Declared Family" OUT.docx [--badkey] [--same-name]

The payload is XOR-obfuscated exactly the way Word writes it
(ECMA-376 Part 4 section 2.8.1 "Font Embedding"): the 16 bytes of the
w:fontKey GUID in REVERSED order, XORed over the first 32 bytes of the
font binary.

  --badkey    writes a fontKey that does NOT match the obfuscation key
              (negative fixture: import must fall back, not crash)
  --same-name declares w:name equal to the font's real family
"""
import sys
import zipfile
import uuid

CT = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
<Default Extension="fntdata" ContentType="application/vnd.openxmlformats-officedocument.obfuscatedFont"/>
<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
<Override PartName="/word/fontTable.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.fontTable+xml"/>
</Types>"""

ROOT_RELS = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
</Relationships>"""

DOC_RELS = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/fontTable" Target="fontTable.xml"/>
</Relationships>"""

FONT_RELS = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rIdF1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/font" Target="fonts/font1.fntdata"/>
</Relationships>"""

FONTTABLE = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:fonts xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
<w:font w:name="{name}"><w:family w:val="swiss"/><w:pitch w:val="variable"/><w:embedRegular r:id="rIdF1" w:fontKey="{{{key}}}"/></w:font>
<w:font w:name="Calibri"><w:family w:val="swiss"/></w:font>
</w:fonts>"""

DOCUMENT = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body>
<w:p><w:r><w:rPr><w:rFonts w:ascii="{name}" w:hAnsi="{name}"/><w:sz w:val="48"/></w:rPr><w:t>Embedded font: The quick brown fox 0123456789</w:t></w:r></w:p>
<w:p><w:r><w:rPr><w:sz w:val="48"/></w:rPr><w:t>Default font: The quick brown fox 0123456789</w:t></w:r></w:p>
<w:sectPr><w:pgSz w:w="12240" w:h="15840"/><w:pgMar w:top="1440" w:right="1440" w:bottom="1440" w:left="1440" w:header="720" w:footer="720" w:gutter="0"/></w:sectPr>
</w:body></w:document>"""


def patch_name_table(ttf: bytearray, new_family: str) -> None:
    """Rewrite family name records (nameIDs 1/4/16) of an sfnt in
    place.  The replacement must have the same character count for
    ASCII-safe names so the byte length is unchanged."""
    import struct
    num = struct.unpack(">H", ttf[4:6])[0]
    for i in range(num):
        rec = 12 + i * 16
        if ttf[rec:rec + 4] == b"name":
            off, ln = struct.unpack(">II", ttf[rec + 8:rec + 16])
            break
    else:
        raise ValueError("no name table")
    count, strbase = struct.unpack(">HH", ttf[off + 2:off + 6])
    for i in range(count):
        r = off + 6 + i * 12
        pid, eid, lid, nid, slen, soff = struct.unpack(
            ">HHHHHH", ttf[r:r + 12])
        if nid not in (1, 4, 16):
            continue
        enc = "utf-16-be" if pid in (0, 3) else "latin-1"
        data = new_family.encode(enc)
        if len(data) != slen:
            continue  # different length — skip rather than corrupt
        ttf[off + strbase + soff: off + strbase + soff + slen] = data


def main() -> int:
    font_path, declared, out = sys.argv[1], sys.argv[2], sys.argv[3]
    bad = "--badkey" in sys.argv
    rename = None
    for a in sys.argv[4:]:
        if a.startswith("--rename="):
            rename = a.split("=", 1)[1]
    ttf = bytearray(open(font_path, "rb").read())
    if rename:
        patch_name_table(ttf, rename)

    key = uuid.UUID("11223344-5566-7788-99aa-bbccddeeff00")
    # obfuscation key = the GUID's 16 bytes reversed
    rev = key.bytes[::-1]
    obf = ttf
    for i in range(min(32, len(obf))):
        obf[i] ^= rev[i % 16]
    # negative fixture: same payload, wrong key declared
    declared_key = "00112233-4455-6677-8899-aabbccddeeff" if bad \
        else str(key).upper()

    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", CT)
        z.writestr("_rels/.rels", ROOT_RELS)
        z.writestr("word/document.xml", DOCUMENT.format(name=declared))
        z.writestr("word/_rels/document.xml.rels", DOC_RELS)
        z.writestr("word/fontTable.xml",
                   FONTTABLE.format(name=declared, key=declared_key))
        z.writestr("word/_rels/fontTable.xml.rels", FONT_RELS)
        z.writestr("word/fonts/font1.fntdata", bytes(obf))
    print(f"{out}: embedded {font_path} as '{declared}'"
          f" (badkey={bad})")
    return 0


if __name__ == "__main__":
    sys.exit(main() if len(sys.argv) >= 4 else 2)
