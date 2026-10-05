#!/usr/bin/env python3
"""COV11 coverage fixtures.

Generates broad-coverage import fixtures into an output directory
(default ./test/wp/cov11 — pass a target dir as argv[1]):

  cov11.docx -- one kitchen-sink package exercising as many
                OXMLi_ListenerState_Valid element cases as possible:
                pPr/rPr variety, numbering + styles + fontTable +
                settings + footnotes/endnotes/comments/header/footer/
                theme parts, fields, hyperlinks, bookmarks, sdt,
                VML + DrawingML textboxes, inline/anchored images,
                tables (merge/nested/float), multiple sections, math
  cov11.odt  -- rich ODF package: styles + automatic styles, tables,
                lists, frames, notes, fields, links, bookmarks,
                sections, index marks, headers/footers
  cov11.wmf  -- minimal placeable WMF (rectangle + ellipse + text)

The fixtures only need to be well-formed enough to drive the importer
branches; they are not meant to be schema-perfect Word/LibreOffice
output.
"""
import os
import struct
import sys
import zlib
import zipfile

OUT = sys.argv[1] if len(sys.argv) > 1 else 'test/wp/cov11'
os.makedirs(OUT, exist_ok=True)


def mkpng(w, h, rgb):
    def chunk(t, d):
        return (struct.pack('>I', len(d)) + t + d
                + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff))
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)
    raw = b''.join(b'\x00' + rgb * w for _ in range(h))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr)
            + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


PNG_RED = mkpng(24, 16, b'\xcc\x20\x20')


def write(name, data):
    mode = 'wb' if isinstance(data, (bytes, bytearray)) else 'w'
    with open(os.path.join(OUT, name), mode) as f:
        f.write(data)
    print('wrote', os.path.join(OUT, name))


# ==== cov11.wmf =========================================================

def wmf_record(func, params=b''):
    # size in WORDs includes the size + function fields
    nwords = 3 + (len(params) + 1) // 2
    pad = params + (b'\x00' if len(params) % 2 else b'')
    return struct.pack('<IH', nwords, func) + pad


def make_wmf():
    recs = b''
    recs += wmf_record(0x0103, struct.pack('<H', 8))           # SETMAPMODE MM_ANISOTROPIC
    recs += wmf_record(0x020B, struct.pack('<hh', 0, 0))       # SETWINDOWORG
    recs += wmf_record(0x020C, struct.pack('<hh', 400, 300))   # SETWINDOWEXT
    recs += wmf_record(0x02FA, struct.pack('<Hhhi',            # CREATEPENINDIRECT
                                           0, 2, 0, 0x000000FF))
    recs += wmf_record(0x012D, struct.pack('<H', 0))           # SELECTOBJECT pen
    recs += wmf_record(0x041B, struct.pack('<hhhh',            # RECTANGLE
                                           200, 150, 100, 50))
    recs += wmf_record(0x02FC, struct.pack('<HiH',             # CREATEBRUSHINDIRECT
                                           0, 0x00FF00, 0))
    recs += wmf_record(0x012D, struct.pack('<H', 1))           # SELECTOBJECT brush
    recs += wmf_record(0x0418, struct.pack('<hhhh',            # ELLIPSE
                                           200, 250, 300, 350))
    recs += wmf_record(0x0214, struct.pack('<hh', 20, 20))     # MOVETO
    recs += wmf_record(0x0213, struct.pack('<hh', 120, 120))   # LINETO
    txt = b'WMF'
    # TEXTOUT params: length(2), string (padded to even), then y,x
    strfield = txt + (b'\x00' if len(txt) % 2 else b'')
    recs += wmf_record(0x0521, struct.pack('<H', len(txt)) + strfield
                       + struct.pack('<hh', 30, 30))
    recs += wmf_record(0x01F0, struct.pack('<H', 1))           # DELETEOBJECT brush
    recs += wmf_record(0x01F0, struct.pack('<H', 0))           # DELETEOBJECT pen
    recs += wmf_record(0x0000)                                 # EOF
    body = (struct.pack('<HHHIHIH', 1, 9, 0x0300,
                        9 + len(recs) // 2, 3, 8, 0)
            + recs)
    # Aldus placeable header
    inch = 1440
    bbox = struct.pack('<hhhh', 0, 0, 400 * inch // 300, 300 * inch // 300)
    ph = struct.pack('<I H', 0x9AC6CDD7, 0) + bbox + struct.pack('<HI', inch, 0)
    csum = 0
    for i in range(0, len(ph), 2):
        csum ^= struct.unpack_from('<H', ph, i)[0]
    return ph + struct.pack('<H', csum) + body


write('cov11.wmf', make_wmf())


# ==== cov11.docx ========================================================

W_NS = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
R_NS = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'
REL = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships/'

DOCX_HEAD = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
    '<w:document xmlns:w="%s" xmlns:r="%s" '
    'xmlns:wp="http://schemas.openxmlformats.org/drawingml/2006/'
    'wordprocessingDrawing" '
    'xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" '
    'xmlns:pic="http://schemas.openxmlformats.org/drawingml/2006/'
    'picture" '
    'xmlns:wps="http://schemas.microsoft.com/office/word/2010/'
    'wordprocessingShape" '
    'xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/'
    '2006" '
    'xmlns:v="urn:schemas-microsoft-com:vml" '
    'xmlns:o="urn:schemas-microsoft-com:office:office" '
    'xmlns:m="http://schemas.openxmlformats.org/officeDocument/2006/'
    'math" '
    'xmlns:w14="http://schemas.microsoft.com/office/word/2010/wordml">\n'
    '<w:body>\n' % (W_NS, R_NS))
DOCX_TAIL = '</w:body>\n</w:document>\n'

ROOT_RELS = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
    '<Relationships xmlns="http://schemas.openxmlformats.org/package/'
    '2006/relationships">'
    '<Relationship Id="rId1" Target="word/document.xml" '
    'Type="http://schemas.openxmlformats.org/officeDocument/2006/'
    'relationships/officeDocument"/>'
    '</Relationships>\n')


def make_docx(path, document_body, doc_rels, extra_parts, extra_types=()):
    ct = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
        '<Types xmlns="http://schemas.openxmlformats.org/package/'
        '2006/content-types">'
        '<Default Extension="rels" ContentType="application/vnd.'
        'openxmlformats-package.relationships+xml"/>'
        '<Default Extension="xml" ContentType="application/xml"/>'
        '<Default Extension="png" ContentType="image/png"/>'
        '<Default Extension="bin" ContentType="application/vnd.'
        'openxmlformats-officedocument.oleObject"/>')
    for pn, ctype in extra_types:
        ct += ('<Override PartName="%s" ContentType="%s"/>' % (pn, ctype))
    ct += '</Types>\n'
    rels = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
            '<Relationships xmlns="http://schemas.openxmlformats.org/'
            'package/2006/relationships">')
    for rid, rtype, target, *rest in doc_rels:
        mode = rest[0] if rest else ''
        rels += ('<Relationship Id="%s" Type="%s" Target="%s"%s/>'
                 % (rid, rtype, target, mode))
    rels += '</Relationships>\n'
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml', ct)
        z.writestr('_rels/.rels', ROOT_RELS)
        z.writestr('word/document.xml', DOCX_HEAD + document_body + DOCX_TAIL)
        z.writestr('word/_rels/document.xml.rels', rels)
        for name, data in extra_parts:
            z.writestr(name, data)
    print('wrote', path)


XMLDECL = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
W = 'xmlns:w="%s"' % W_NS

# ---- document.xml ----------------------------------------------------

P = []  # body parts

# 1. paragraphs exercising pPr variety
P.append(
    '<w:p><w:pPr><w:pStyle w:val="Heading1"/>'
    '<w:keepNext/><w:keepLines/><w:pageBreakBefore/>'
    '<w:widowControl w:val="0"/><w:suppressLineNumbers/>'
    '<w:contextualSpacing/><w:mirrorIndents/><w:snapToGrid w:val="0"/>'
    '<w:suppressAutoHypens/><w:bidi/><w:kinsoku/><w:overflowPunct/>'
    '<w:topLinePunct/><w:autoSpaceDE w:val="0"/><w:autoSpaceDN/>'
    '<w:adjustRightInd w:val="0"/><w:suppressOverlap/>'
    '<w:textboxTightWrap w:val="allLines"/>'
    '<w:wordWrap w:val="0"/>'
    '<w:framePr w:val="0" w:w="1000" w:h="500" w:vSpace="20" '
    'w:hSpace="20" w:x="100" w:y="100" w:hRule="atLeast" '
    'w:wrap="around" w:vAnchor="page" w:hAnchor="text" '
    'w:xAlign="left" w:yAlign="top" w:dropCap="none" '
    'w:lines="2" w:anchorLock="1"/>'
    '<w:divId w:val="1"/>'
    '<w:cnfStyle w:val="000000000010" w:firstRow="1"/>'
    '<w:textAlignment w:val="top"/><w:outlineLvl w:val="2"/>'
    '<w:pBdr>'
    '<w:top w:val="single" w:sz="4" w:space="1" w:color="FF0000" '
    'w:shadow="1" w:frame="1"/>'
    '<w:left w:val="dashed" w:sz="8" w:space="2" w:color="00FF00"/>'
    '<w:bottom w:val="double" w:sz="6" w:space="3" w:color="0000FF"/>'
    '<w:right w:val="dotted" w:sz="2" w:space="4" w:color="000000"/>'
    '<w:between w:val="thick" w:sz="12" w:space="1" w:color="808080"/>'
    '<w:bar w:val="single" w:sz="4" w:space="1" w:color="408080"/>'
    '</w:pBdr>'
    '<w:shd w:val="pct20" w:color="FF00FF" w:fill="FFFF00"/>'
    '<w:tabs>'
    '<w:tab w:val="left" w:leader="dot" w:pos="1440"/>'
    '<w:tab w:val="center" w:leader="hyphen" w:pos="2880"/>'
    '<w:tab w:val="right" w:leader="underscore" w:pos="4320"/>'
    '<w:tab w:val="decimal" w:leader="heavy" w:pos="5760"/>'
    '<w:tab w:val="bar" w:leader="middleDot" w:pos="7200"/>'
    '<w:tab w:val="clear" w:pos="8000"/>'
    '</w:tabs>'
    '<w:spacing w:before="120" w:after="60" w:beforeLines="20" '
    'w:afterLines="10" w:line="360" w:lineRule="exact" '
    'w:beforeAutospacing="0" w:afterAutospacing="1"/>'
    '<w:ind w:left="360" w:right="240" w:firstLine="180" '
    'w:hanging="0" w:leftChars="10" w:rightChars="5" '
    'w:firstLineChars="5" w:hangingChars="0"/>'
    '<w:jc w:val="both"/>'
    '<w:rPr><w:b/><w:i/></w:rPr>'
    '</w:pPr>'
    '<w:r><w:rPr><w:rStyle w:val="Emphasis"/>'
    '<w:rFonts w:ascii="Calibri" w:hAnsi="Calibri" '
    'w:eastAsia="MS Mincho" w:cs="Arial" w:hint="eastAsia"/>'
    '<w:b/><w:bCs/><w:i/><w:iCs/><w:caps/><w:smallCaps/>'
    '<w:strike/><w:dstrike/><w:outline/><w:shadow/><w:emboss/>'
    '<w:imprint/><w:noProof/><w:snapToGrid w:val="0"/>'
    '<w:vanish/><w:webHidden/><w:specVanish/>'
    '<w:color w:val="803080" w:themeColor="accent1" '
    'w:themeTint="80" w:themeShade="40"/>'
    '<w:spacing w:val="40"/><w:w w:val="120"/><w:kern w:val="28"/>'
    '<w:position w:val="10"/><w:sz w:val="28"/><w:szCs w:val="30"/>'
    '<w:highlight w:val="yellow"/>'
    '<w:u w:val="double" w:color="FF0000"/>'
    '<w:effect w:val="blinkBackground"/>'
    '<w:bdr w:val="single" w:sz="4" w:space="1" w:color="00AA00" '
    'w:shadow="1" w:frame="1"/>'
    '<w:shd w:val="clear" w:fill="C0C0C0"/>'
    '<w:fitText w:val="200" w:id="1"/>'
    '<w:vertAlign w:val="superscript"/><w:rtl/><w:cs/>'
    '<w:em w:val="dot"/>'
    '<w:lang w:val="en-US" w:eastAsia="ja-JP" w:bidi="ar-SA"/>'
    '<w:eastAsianLayout w:id="7" w:combine="1" '
    'w:combineBrackets="round" w:vert="1" w:vertCompress="1"/>'
    '<w:rPrChange w:id="44" w:author="Ed" '
    'w:date="2024-01-02T03:04:05Z"><w:rPr><w:b w:val="0"/>'
    '</w:rPr></w:rPrChange>'
    '</w:rPr><w:t xml:space="preserve"> styled run </w:t></w:r>'
    '<w:r><w:t> next</w:t></w:r>'
    '</w:p>\n')

# 2. run-level special elements
P.append(
    '<w:p><w:pPr><w:jc w:val="distribute"/></w:pPr>'
    '<w:r><w:t>a</w:t></w:r>'
    '<w:r><w:tab/></w:r>'
    '<w:r><w:t>b</w:t></w:r>'
    '<w:r><w:br w:type="textWrapping" w:clear="all"/></w:r>'
    '<w:r><w:t>c</w:t></w:r>'
    '<w:r><w:cr/></w:r>'
    '<w:r><w:t>d</w:t></w:r>'
    '<w:r><w:softHyphen/></w:r><w:r><w:t>soft</w:t></w:r>'
    '<w:r><w:noBreakHyphen/></w:r><w:r><w:t>nb</w:t></w:r>'
    '<w:r><w:sym w:font="Wingdings" w:char="F028"/></w:r>'
    '<w:r><w:dayShort/></w:r><w:r><w:dayLong/></w:r>'
    '<w:r><w:monthShort/></w:r><w:r><w:monthLong/></w:r>'
    '<w:r><w:yearShort/></w:r><w:r><w:yearLong/></w:r>'
    '<w:r><w:annotationRef/></w:r>'
    '<w:r><w:lastRenderedPageBreak/></w:r>'
    '<w:r><w:ptab w:relativeTo="margin" w:alignment="right" '
    'w:leader="dot"/></w:r>'
    '<w:r><w:t>tail</w:t></w:r>'
    '</w:p>\n')

# 3. fields: simple + complex
P.append(
    '<w:p><w:fldSimple w:instr=" PAGE "><w:r><w:t>1</w:t></w:r>'
    '</w:fldSimple>'
    '<w:r><w:fldChar w:fldCharType="begin" w:dirty="1" '
    'w:fldLock="0"/></w:r>'
    '<w:r><w:instrText xml:space="preserve"> NUMPAGES </w:instrText>'
    '</w:r>'
    '<w:r><w:fldChar w:fldCharType="separate"/></w:r>'
    '<w:r><w:t>3</w:t></w:r>'
    '<w:r><w:fldChar w:fldCharType="end"/></w:r>'
    '<w:r><w:fldChar w:fldCharType="begin"/></w:r>'
    '<w:r><w:instrText> DATE \\@ "yyyy-MM-dd" \\* MERGEFORMAT '
    '</w:instrText></w:r>'
    '<w:r><w:fldChar w:fldCharType="separate"/></w:r>'
    '<w:r><w:t>2024-01-02</w:t></w:r>'
    '<w:r><w:fldChar w:fldCharType="end"/></w:r>'
    '<w:r><w:fldChar w:fldCharType="begin"/></w:r>'
    '<w:r><w:instrText> TIME \\@ "HH:mm" </w:instrText></w:r>'
    '<w:r><w:fldChar w:fldCharType="separate"/></w:r>'
    '<w:r><w:t>09:41</w:t></w:r>'
    '<w:r><w:fldChar w:fldCharType="end"/></w:r>'
    '<w:r><w:fldChar w:fldCharType="begin"/></w:r>'
    '<w:r><w:instrText> SEQ fig \\* ARABIC </w:instrText></w:r>'
    '<w:r><w:fldChar w:fldCharType="separate"/></w:r>'
    '<w:r><w:t>7</w:t></w:r>'
    '<w:r><w:fldChar w:fldCharType="end"/></w:r>'
    '<w:r><w:fldChar w:fldCharType="begin"/></w:r>'
    '<w:r><w:instrText> REF bk1 \\h </w:instrText></w:r>'
    '<w:r><w:fldChar w:fldCharType="separate"/></w:r>'
    '<w:r><w:t>bk</w:t></w:r>'
    '<w:r><w:fldChar w:fldCharType="end"/></w:r>'
    '<w:r><w:fldChar w:fldCharType="begin"/></w:r>'
    '<w:r><w:instrText> MERGEFIELD city </w:instrText></w:r>'
    '<w:r><w:fldChar w:fldCharType="separate"/></w:r>'
    '<w:r><w:t>Oslo</w:t></w:r>'
    '<w:r><w:fldChar w:fldCharType="end"/></w:r>'
    '<w:r><w:fldChar w:fldCharType="begin"/></w:r>'
    '<w:r><w:instrText> = 2+2 \\# "#,##0" </w:instrText></w:r>'
    '<w:r><w:fldChar w:fldCharType="separate"/></w:r>'
    '<w:r><w:t>4</w:t></w:r>'
    '<w:r><w:fldChar w:fldCharType="end"/></w:r>'
    '<w:r><w:fldChar w:fldCharType="begin"/></w:r>'
    '<w:r><w:instrText> FILENAME \\p </w:instrText></w:r>'
    '<w:r><w:fldChar w:fldCharType="separate"/></w:r>'
    '<w:r><w:t>cov11.docx</w:t></w:r>'
    '<w:r><w:fldChar w:fldCharType="end"/></w:r>'
    '</w:p>\n')

# 4. TOC field result
P.append(
    '<w:p><w:r><w:fldChar w:fldCharType="begin"/></w:r>'
    '<w:r><w:instrText xml:space="preserve"> TOC \\o "1-3" \\h \\z \\u '
    '</w:instrText></w:r>'
    '<w:r><w:fldChar w:fldCharType="separate"/></w:r>'
    '<w:r><w:t>Intro</w:t></w:r>'
    '<w:r><w:tab/></w:r><w:r><w:t>1</w:t></w:r></w:p>\n'
    '<w:p><w:r><w:t>Body</w:t></w:r><w:r><w:tab/></w:r>'
    '<w:r><w:t>2</w:t></w:r></w:p>\n'
    '<w:p><w:r><w:fldChar w:fldCharType="end"/></w:r></w:p>\n')

# 5. hyperlinks + bookmarks + proofErr + sdt + ruby + smartTag
P.append(
    '<w:p>'
    '<w:bookmarkStart w:id="11" w:name="bk1"/>'
    '<w:hyperlink r:id="rIdLink1" w:anchor="bk1" w:docLocation="loc" '
    'w:history="1" w:tooltip="tip">'
    '<w:r><w:rPr><w:rStyle w:val="Hyperlink"/></w:rPr>'
    '<w:t>linked</w:t></w:r></w:hyperlink>'
    '<w:hyperlink w:anchor="bk1"><w:r><w:t>jump</w:t></w:r></w:hyperlink>'
    '<w:bookmarkEnd w:id="11"/>'
    '<w:proofErr w:type="spellStart"/><w:r><w:t>mispell</w:t></w:r>'
    '<w:proofErr w:type="spellEnd"/>'
    '<w:proofErr w:type="gramStart"/><w:r><w:t>grm</w:t></w:r>'
    '<w:proofErr w:type="gramEnd"/>'
    '<w:proofErr w:type="matStart"/><w:proofErr w:type="matEnd"/>'
    '<w:proofErr w:type="corrStart"/><w:proofErr w:type="corrEnd"/>'
    '<w:proofErr w:type="prodStart"/><w:proofErr w:type="prodEnd"/>'
    '<w:proofErr w:type="dispStart"/><w:proofErr w:type="dispEnd"/>'
    '<w:proofErr w:type="advStart"/><w:proofErr w:type="advEnd"/>'
    '</w:p>\n'
    '<w:p><w:smtTag w:uri="urn:test" w:element="place">'
    '<w:r><w:t>smart</w:t></w:r></w:smtTag>'
    '<w:smartTag w:uri="urn:test2" w:element="city">'
    '<w:r><w:t>tagged</w:t></w:r></w:smartTag>'
    '</w:p>\n'
    '<w:p><w:ruby><w:rubyPr><w:rubyAlign w:val="center"/>'
    '<w:hps w:val="16"/><w:hpsRaise w:val="32"/>'
    '<w:hpsBaseText w:val="24"/><w:lid w:val="ja-JP"/></w:rubyPr>'
    '<w:rt><w:r><w:t>kan</w:t></w:r></w:rt>'
    '<w:rubyBase><w:r><w:t>base</w:t></w:r></w:rubyBase></w:ruby>'
    '</w:p>\n')

# 6. sdt block + inline
P.append(
    '<w:sdt><w:sdtPr>'
    '<w:rPr><w:b/></w:rPr>'
    '<w:alias w:val="aliasA"/><w:tag w:val="tagA"/>'
    '<w:id w:val="123456"/><w:lock w:val="sdtLocked"/>'
    '<w:placeholder><w:docPart w:val="DefaultPlaceholder_-1854013440"/>'
    '</w:placeholder><w:showingPlcHdr/>'
    '<w:dataBinding w:prefixMappings="ns" w:xpath="/root/x" '
    'w:storeItemID="{AAAAAAAA-0000-0000-0000-000000000000}"/>'
    '<w:temporary/><w:comboBox><w:listItem w:displayText="d1" '
    'w:value="v1"/></w:comboBox>'
    '</w:sdtPr><w:sdtEndPr><w:rPr><w:i/></w:rPr></w:sdtEndPr>'
    '<w:sdtContent><w:p><w:r><w:t>sdt body</w:t></w:r></w:p>'
    '</w:sdtContent></w:sdt>\n'
    '<w:p><w:sdt><w:sdtPr><w:id w:val="77"/><w:date '
    'w:fullDate="2024-05-06"><w:dateFormat w:val="yyyy"/><w:lid '
    'w:val="en-US"/><w:storeMappedDataAs w:val="dateTime"/>'
    '<w:calendar w:val="gregorian"/></w:date></w:sdtPr>'
    '<w:sdtContent><w:r><w:t>inline sdt</w:t></w:r></w:sdtContent>'
    '</w:sdt></w:p>\n')

# 7. revision-adjacent range markers + customXml + permStart
P.append(
    '<w:p>'
    '<w:moveFromRangeStart w:id="21" w:name="mv1"/>'
    '<w:r><w:t>movefrom</w:t></w:r><w:moveFromRangeEnd w:id="21"/>'
    '<w:moveToRangeStart w:id="22" w:name="mv1"/>'
    '<w:r><w:t>moveto</w:t></w:r><w:moveToRangeEnd w:id="22"/>'
    '<w:customXmlInsRangeStart w:id="31"/>'
    '<w:r><w:t>insrange</w:t></w:r><w:customXmlInsRangeEnd w:id="31"/>'
    '<w:customXmlDelRangeStart w:id="32"/>'
    '<w:customXmlDelRangeEnd w:id="32"/>'
    '<w:customXmlMoveFromRangeStart w:id="33"/>'
    '<w:customXmlMoveFromRangeEnd w:id="33"/>'
    '<w:customXmlMoveToRangeStart w:id="34"/>'
    '<w:customXmlMoveToRangeEnd w:id="34"/>'
    '<w:customXml w:element="cx" w:uri="urn:cx">'
    '<w:p><w:r><w:t>customxml</w:t></w:r></w:p></w:customXml>'
    '<w:permStart w:id="41" w:ed="everyone" w:edGrp="administrators" '
    'w:colFirst="0" w:colLast="1"/>'
    '<w:r><w:t>perm</w:t></w:r><w:permEnd w:id="41"/>'
    '</w:p>\n')

# 8. notes + comments + separators
P.append(
    '<w:p><w:r><w:t>noted</w:t></w:r>'
    '<w:r><w:rPr><w:rStyle w:val="FootnoteReference"/></w:rPr>'
    '<w:footnoteReference w:id="2"/></w:r>'
    '<w:r><w:rPr><w:rStyle w:val="EndnoteReference"/></w:rPr>'
    '<w:endnoteReference w:id="2"/></w:r>'
    '<w:commentRangeStart w:id="0"/>'
    '<w:r><w:t>commented</w:t></w:r><w:commentRangeEnd w:id="0"/>'
    '<w:r><w:rPr><w:rStyle w:val="CommentReference"/></w:rPr>'
    '<w:commentReference w:id="0"/></w:r>'
    '<w:r><w:separator/></w:r><w:r><w:continuationSeparator/></w:r>'
    '<w:r><w:footnoteRef/></w:r><w:r><w:endnoteRef/></w:r>'
    '</w:p>\n')

# 9. numbered list (needs numbering.xml part)
for lvl, numid, txt in ((0, 5, 'level0a'), (1, 5, 'level1a'),
                        (2, 5, 'level2a'), (0, 6, 'bullet1'),
                        (0, 7, 'roman1')):
    P.append(
        '<w:p><w:pPr><w:pStyle w:val="ListParagraph"/>'
        '<w:numPr><w:ilvl w:val="%d"/><w:numId w:val="%d"/></w:numPr>'
        '<w:ind w:left="720" w:hanging="360"/></w:pPr>'
        '<w:r><w:t>%s</w:t></w:r></w:p>\n' % (lvl, numid, txt))

# 10. inline + anchored drawing
P.append(
    '<w:p><w:r><w:drawing>'
    '<wp:inline distT="1" distB="2" distL="3" distR="4">'
    '<wp:extent cx="304800" cy="203200"/>'
    '<wp:effectExtent l="10" t="20" r="30" b="40"/>'
    '<wp:docPr id="51" name="inline1" descr="alt" title="tt" '
    'hidden="1"/>'
    '<wp:cNvGraphicFramePr><a:graphicFrameLocks '
    'noChangeAspect="1"/></wp:cNvGraphicFramePr>'
    '<a:graphic><a:graphicData uri="http://schemas.openxmlformats.'
    'org/drawingml/2006/picture">'
    '<pic:pic><pic:nvPicPr><pic:cNvPr id="52" name="p1"/>'
    '<pic:cNvPicPr><a:picLocks noChangeAspect="1"/></pic:cNvPicPr>'
    '</pic:nvPicPr>'
    '<pic:blipFill><a:blip r:embed="rIdImg1"><a:alphaModFix '
    'amt="60000"/><a:lum bright="10000" contrast="5000"/></a:blip>'
    '<a:srcRect l="1000" t="2000" r="3000" b="4000"/>'
    '<a:stretch><a:fillRect/></a:stretch></pic:blipFill>'
    '<pic:spPr><a:xfrm flipH="1" flipV="0" rot="5400000">'
    '<a:off x="0" y="0"/><a:ext cx="304800" cy="203200"/></a:xfrm>'
    '<a:prstGeom prst="roundRect"><a:avLst><a:gd name="adj" '
    'fmla="val 20000"/></a:avLst></a:prstGeom>'
    '<a:ln w="12700" cap="rnd" cmpd="dbl" algn="ctr">'
    '<a:solidFill><a:srgbClr val="123456"/></a:solidFill>'
    '<a:prstDash val="dash"/><a:round/></a:ln>'
    '<a:effectLst><a:outerShdw blurRad="40000" dist="20000" '
    'dir="2700000" rotWithShape="0"><a:srgbClr val="000000">'
    '<a:alpha val="40000"/></a:srgbClr></a:outerShdw></a:effectLst>'
    '</pic:spPr></pic:pic></a:graphicData></a:graphic>'
    '</wp:inline></w:drawing></w:r></w:p>\n'
    '<w:p><w:r><w:drawing>'
    '<wp:anchor distT="1" distB="2" distL="3" distR="4" '
    'simplePos="0" relativeHeight="5" behindDoc="1" locked="0" '
    'layoutInCell="1" allowOverlap="1">'
    '<wp:simplePos x="10" y="20"/>'
    '<wp:positionH relativeFrom="page"><wp:posOffset>914400'
    '</wp:posOffset></wp:positionH>'
    '<wp:positionV relativeFrom="margin"><wp:posOffset>457200'
    '</wp:posOffset></wp:positionV>'
    '<wp:extent cx="609600" cy="406400"/>'
    '<wp:effectExtent l="1" t="2" r="3" b="4"/>'
    '<wp:wrapSquare wrapText="bothSides"/>'
    '<wp:docPr id="53" name="anch1"/></wp:anchor>'
    '</w:drawing></w:r></w:p>\n'
    '<w:p><w:r><w:drawing>'
    '<wp:anchor distT="0" distB="0" distL="0" distR="0" '
    'simplePos="1" relativeHeight="6" behindDoc="0" locked="1" '
    'layoutInCell="0" allowOverlap="0">'
    '<wp:simplePos x="0" y="0"/>'
    '<wp:positionH relativeFrom="column"><wp:align>center</wp:align>'
    '</wp:positionH>'
    '<wp:positionV relativeFrom="paragraph"><wp:align>top</wp:align>'
    '</wp:positionV>'
    '<wp:extent cx="300000" cy="200000"/>'
    '<wp:wrapTight wrapText="largest"><wp:wrapPolygon '
    'edited="0"><wp:start x="0" y="0"/><wp:lineTo x="100" y="0"/>'
    '<wp:lineTo x="100" y="100"/><wp:lineTo x="0" y="100"/>'
    '</wp:wrapPolygon></wp:wrapTight>'
    '<wp:docPr id="54" name="anch2"/>'
    '<a:graphic><a:graphicData uri="http://schemas.openxmlformats.'
    'org/drawingml/2006/picture">'
    '<pic:pic><pic:nvPicPr><pic:cNvPr id="55" name="p2"/>'
    '<pic:cNvPicPr/></pic:nvPicPr>'
    '<pic:blipFill><a:blip r:embed="rIdImg1"/>'
    '<a:tile tx="100" ty="200" sx="50000" sy="50000" '
    'flip="x" algn="tl"/></pic:blipFill>'
    '<pic:spPr/></pic:pic></a:graphicData></a:graphic>'
    '</wp:anchor></w:drawing></w:r></w:p>\n'
    '<w:p><w:r><w:drawing>'
    '<wp:anchor distT="0" distB="0" distL="0" distR="0" '
    'simplePos="0" relativeHeight="7" behindDoc="0" locked="0" '
    'layoutInCell="1" allowOverlap="1">'
    '<wp:simplePos x="0" y="0"/>'
    '<wp:positionH relativeFrom="leftMargin"><wp:align>left'
    '</wp:align></wp:positionH>'
    '<wp:positionV relativeFrom="topMargin"><wp:align>bottom'
    '</wp:align></wp:positionV>'
    '<wp:extent cx="100000" cy="80000"/>'
    '<wp:wrapNone/><wp:docPr id="56" name="anch3"/>'
    '</wp:anchor></w:drawing></w:r></w:p>\n'
    '<w:p><w:r><w:drawing>'
    '<wp:anchor distT="0" distB="0" distL="0" distR="0" '
    'simplePos="0" relativeHeight="8" behindDoc="0" locked="0" '
    'layoutInCell="1" allowOverlap="1">'
    '<wp:simplePos x="0" y="0"/>'
    '<wp:positionH relativeFrom="insideMargin"><wp:align>right'
    '</wp:align></wp:positionH>'
    '<wp:positionV relativeFrom="insideMargin"><wp:posOffset>1000'
    '</wp:posOffset></wp:positionV>'
    '<wp:extent cx="100000" cy="80000"/>'
    '<wp:wrapTopAndBottom/><wp:docPr id="57" name="anch4"/>'
    '</wp:anchor></w:drawing></w:r></w:p>\n'
    '<w:p><w:r><w:drawing>'
    '<wp:anchor distT="0" distB="0" distL="0" distR="0" '
    'simplePos="0" relativeHeight="9" behindDoc="0" locked="0" '
    'layoutInCell="1" allowOverlap="1">'
    '<wp:simplePos x="0" y="0"/>'
    '<wp:positionH relativeFrom="outsideMargin"><wp:posOffset>-500'
    '</wp:posOffset></wp:positionH>'
    '<wp:positionV relativeFrom="outsideMargin"><wp:align>top'
    '</wp:align></wp:positionV>'
    '<wp:extent cx="100000" cy="80000"/>'
    '<wp:wrapThrough wrapText="largest"><wp:wrapPolygon edited="1">'
    '<wp:start x="0" y="0"/><wp:lineTo x="5" y="5"/></wp:wrapPolygon>'
    '</wp:wrapThrough><wp:docPr id="58" name="anch5"/>'
    '</wp:anchor></w:drawing></w:r></w:p>\n')

# 11. DrawingML textbox (wps:wsp) via graphicData
P.append(
    '<w:p><w:r><w:drawing><wp:inline>'
    '<wp:extent cx="1219200" cy="609600"/>'
    '<wp:docPr id="60" name="tb1"/>'
    '<a:graphic><a:graphicData uri="http://schemas.microsoft.com/'
    'office/word/2010/wordprocessingShape">'
    '<wps:wsp><wps:cNvSpPr txBox="1"/><wps:spPr bwMode="auto">'
    '<a:xfrm rot="0"><a:off x="0" y="0"/><a:ext cx="1219200" '
    'cy="609600"/></a:xfrm>'
    '<a:prstGeom prst="roundRect"><a:avLst/></a:prstGeom>'
    '<a:solidFill><a:srgbClr val="DDEEFF"><a:alpha val="70000"/>'
    '</a:srgbClr></a:solidFill>'
    '<a:ln w="9525"><a:solidFill><a:schemeClr val="accent1"/>'
    '</a:solidFill><a:prstDash val="sysDot"/></a:ln>'
    '<a:effectLst><a:glow rad="40000"><a:srgbClr val="FF0000"/>'
    '</a:glow></a:effectLst></wps:spPr>'
    '<wps:txbx><w:txbxContent><w:p><w:r><w:t>dml textbox'
    '</w:t></w:r></w:p></w:txbxContent></wps:txbx>'
    '<wps:bodyPr rot="0" spcFirstLastPara="0" vertOverflow="overflow" '
    'horzOverflow="overflow" vert="horz" wrap="none" lIns="91440" '
    'tIns="45720" rIns="91440" bIns="45720" numCol="1" spcCol="0" '
    'rtlCol="0" fromWordArt="0" anchor="ctr" anchorCtr="0" '
    'forceAA="0" upright="0" compatLnSpc="1"><a:normAutofit '
    'fontScale="90000" lnSpcReduction="10000"/></wps:bodyPr>'
    '<wps:style>'
    '<a:lnRef idx="2"><a:schemeClr val="accent1"><a:shade '
    'val="50000"/></a:schemeClr></a:lnRef>'
    '<a:fillRef idx="1"><a:schemeClr val="accent1"/></a:fillRef>'
    '<a:effectRef idx="0"><a:schemeClr val="accent1"/>'
    '</a:effectRef>'
    '<a:fontRef idx="minor"><a:schemeClr val="lt1"/></a:fontRef>'
    '</wps:style></wps:wsp>'
    '</a:graphicData></a:graphic></wp:inline></w:drawing></w:r>'
    '</w:p>\n')

# 12. VML pict shapes + textbox + object + ole
P.append(
    '<w:p><w:r><w:pict>'
    '<v:shapetype id="_x0000_t77" coordsize="21600,21600" o:spt="77" '
    'adj="10800" path="m@1,0 l@1,@1 0,@1 0,0@1,0 21600,21600e">'
    '<v:stroke joinstyle="miter"/><v:formulas>'
    '<v:f eqn="val #0"/><v:f eqn="sum @0 10800 0"/>'
    '<v:f eqn="prod @0 1 2"/></v:formulas>'
    '<v:path o:connecttype="segments"/>'
    '<o:lock v:ext="edit" aspectratio="f"/></v:shapetype>'
    '<v:shape id="vsh1" type="#_x0000_t77" '
    'style="position:absolute;margin-left:50pt;margin-top:10pt;'
    'width:80pt;height:60pt;rotation:15;z-index:3" '
    'fillcolor="#a0a0ff" strokecolor="#202020" strokeweight="2pt" '
    'adj="12000" o:spid="_x0000_s2051">'
    '<v:fill color2="#ffffff" type="gradient" angle="45"/>'
    '<v:stroke dashstyle="dash"/><v:shadow on="t" color="#808080" '
    'offset="3pt,3pt" opacity="50%"/>'
    '<v:textbox style="mso-fit-shape-to-text:t" inset="5pt,5pt,'
    '5pt,5pt"><w:txbxContent><w:p><w:r><w:t>vml box</w:t></w:r>'
    '</w:p></w:txbxContent></v:textbox>'
    '<w:wrap type="square" side="both" anchorx="page" anchory="margin"/>'
    '<v:anchorlock/></v:shape>'
    '</w:pict></w:r>'
    '<w:r><w:pict>'
    '<v:rect id="vr1" style="position:absolute;margin-left:150pt;'
    'margin-top:20pt;width:40pt;height:30pt" fillcolor="#ffcccc" '
    'strokecolor="#800000"/>'
    '</w:pict></w:r>'
    '<w:r><w:pict>'
    '<v:oval id="vo1" style="position:absolute;margin-left:200pt;'
    'margin-top:20pt;width:30pt;height:30pt"/>'
    '</w:pict></w:r>'
    '<w:r><w:pict>'
    '<v:line id="vl1" from="10pt,10pt" to="100pt,40pt" '
    'strokecolor="#0000ff" strokeweight="1.5pt" '
    'style="position:absolute"/>'
    '</w:pict></w:r>'
    '<w:r><w:pict>'
    '<v:roundrect id="vrr1" arcsize="0.2" '
    'style="position:absolute;margin-left:250pt;margin-top:20pt;'
    'width:40pt;height:30pt"/>'
    '</w:pict></w:r>'
    '<w:r><w:pict>'
    '<v:shape id="vimg" type="#_x0000_t75" '
    'style="position:absolute;margin-left:300pt;margin-top:10pt;'
    'width:24pt;height:16pt">'
    '<v:imagedata r:id="rIdImg1" o:title="vimg" '
    'o:href="http://example.invalid/x.png" o:relid="rIdImg1" '
    'pict="p" cropTop="0.1" cropBottom="0.1" cropLeft="0.1" '
    'cropRight="0.1" gain="1.2" blacklevel="0.1" gamma="0.8"/>'
    '</v:shape>'
    '</w:pict></w:r>'
    '<w:r><w:pict>'
    '<v:group id="vg1" coordsize="200,200" '
    'style="position:absolute;margin-left:350pt;margin-top:10pt;'
    'width:50pt;height:50pt">'
    '<v:rect id="vgr1" style="position:absolute;left:0;top:0;'
    'width:100;height:100"/>'
    '</v:group>'
    '</w:pict></w:r>'
    '<w:r><w:pict>'
    '<o:OLEObject Type="Embed" ProgID="Paint.Picture" '
    'ShapeID="_x0000_i1030" DrawAspect="Content" '
    'ObjectID="_1400000001" r:id="rIdOle1">'
    '<o:LinkType>Picture</o:LinkType>'
    '<o:LockedField>false</o:LockedField>'
    '<o:FieldCodes></o:FieldCodes>'
    '</o:OLEObject>'
    '<v:shape id="_x0000_i1030" type="#_x0000_t75" '
    'style="width:24pt;height:16pt" o:ole="">'
    '<v:imagedata r:id="rIdImg1" o:title="oleimg"/></v:shape>'
    '</w:pict></w:r>'
    '<w:r><w:object w:dxaOrig="480" w:dyaOrig="320">'
    '<v:shape id="obj1" type="#_x0000_t75" '
    'style="width:24pt;height:16pt">'
    '<v:imagedata r:id="rIdImg1" o:title="objimg"/></v:shape>'
    '<o:OLEObject Type="Embed" ProgID="Word.Document.8" '
    'ShapeID="obj1" DrawAspect="Icon" ObjectID="_1400000002" '
    'r:id="rIdOle2"/></w:object></w:r>'
    '</w:p>\n')

# 13. tables: merged cells, nested, floating, tblPrEx, headers
P.append(
    '<w:tbl><w:tblPr>'
    '<w:tblStyle w:val="TableGrid"/><w:tblW w:w="5000" w:type="dxa"/>'
    '<w:tblInd w:w="120" w:type="dxa"/>'
    '<w:jc w:val="center"/>'
    '<w:tblpPr w:leftFromText="100" w:rightFromText="100" '
    'w:topFromText="50" w:bottomFromText="50" w:vertAnchor="margin" '
    'w:horzAnchor="page" w:tblpXSpec="right" w:tblpYSpec="center"/>'
    '<w:tblOverlap w:val="overlap"/>'
    '<w:bidiVisual/><w:tblStyleRowBandSize w:val="2"/>'
    '<w:tblStyleColBandSize w:val="3"/>'
    '<w:tblBorders>'
    '<w:top w:val="single" w:sz="8" w:color="000000"/>'
    '<w:left w:val="single" w:sz="8" w:color="000000"/>'
    '<w:bottom w:val="single" w:sz="8" w:color="000000"/>'
    '<w:right w:val="single" w:sz="8" w:color="000000"/>'
    '<w:insideH w:val="dashed" w:sz="4" w:color="808080"/>'
    '<w:insideV w:val="dotted" w:sz="2" w:color="C0C0C0"/>'
    '<w:start w:val="single" w:sz="2" w:color="000000"/>'
    '<w:end w:val="single" w:sz="2" w:color="000000"/>'
    '<w:tl2br w:val="single" w:sz="2" w:color="000000"/>'
    '<w:tr2bl w:val="single" w:sz="2" w:color="000000"/>'
    '</w:tblBorders>'
    '<w:shd w:val="clear" w:fill="EFEFEF"/>'
    '<w:tblLayout w:type="fixed"/>'
    '<w:tblCellMar>'
    '<w:top w:w="40" w:type="dxa"/><w:left w:w="80" w:type="dxa"/>'
    '<w:bottom w:w="40" w:type="dxa"/><w:right w:w="80" w:type="dxa"/>'
    '<w:start w:w="80" w:type="dxa"/><w:end w:w="80" w:type="dxa"/>'
    '</w:tblCellMar>'
    '<w:tblCellSpacing w:w="20" w:type="dxa"/>'
    '<w:tblLook w:val="04A0" w:firstRow="1" w:lastRow="0" '
    'w:firstColumn="1" w:lastColumn="0" w:noHBand="0" w:noVBand="1"/>'
    '</w:tblPr>'
    '<w:tblGrid><w:gridCol w:w="1500"/><w:gridCol w:w="2000"/>'
    '<w:gridCol w:w="1500"/></w:tblGrid>'
    '<w:tr><w:trPr><w:cantSplit/><w:trHeight w:val="400" '
    'w:hRule="atLeast"/><w:tblHeader/><w:tblCellSpacing w:w="10" '
    'w:type="dxa"/><w:jc w:val="right"/>'
    '<w:tblPrEx><w:tblBorders><w:top w:val="double" w:sz="4" '
    'w:color="FF0000"/></w:tblBorders></w:tblPrEx></w:trPr>'
    '<w:tc><w:tcPr><w:tcW w:w="1500" w:type="dxa"/>'
    '<w:gridSpan w:val="2"/><w:tcBorders><w:top w:val="single" '
    'w:sz="4" w:color="00AA00"/></w:tcBorders>'
    '<w:shd w:val="clear" w:fill="FFE0E0"/>'
    '<w:noWrap/><w:tcMar><w:left w:w="30" w:type="dxa"/></w:tcMar>'
    '<w:textDirection w:val="tbRl"/>'
    '<w:tcFitText/><w:vAlign w:val="center"/><w:hideMark/>'
    '</w:tcPr>'
    '<w:p><w:r><w:t>hdr span</w:t></w:r></w:p></w:tc>'
    '<w:tc><w:tcPr><w:tcW w:w="1500" w:type="dxa"/>'
    '<w:vAlign w:val="top"/></w:tcPr>'
    '<w:p><w:r><w:t>h3</w:t></w:r></w:p></w:tc></w:tr>'
    '<w:tr><w:trPr><w:trHeight w:val="300" w:hRule="exact"/>'
    '<w:hidden/><w:cantSplit w:val="0"/></w:trPr>'
    '<w:tc><w:tcPr><w:tcW w:w="1500" w:type="dxa"/>'
    '<w:vMerge w:val="restart"/><w:vAlign w:val="bottom"/></w:tcPr>'
    '<w:p><w:r><w:t>vm</w:t></w:r></w:p></w:tc>'
    '<w:tc><w:tcPr><w:tcW w:w="2000" w:type="dxa"/></w:tcPr>'
    '<w:p><w:r><w:t>nested:</w:t></w:r></w:p>'
    '<w:tbl><w:tblPr><w:tblW w:w="0" w:type="auto"/></w:tblPr>'
    '<w:tblGrid><w:gridCol w:w="800"/><w:gridCol w:w="800"/>'
    '</w:tblGrid>'
    '<w:tr><w:tc><w:tcPr><w:tcW w:w="800" w:type="dxa"/></w:tcPr>'
    '<w:p><w:r><w:t>n1</w:t></w:r></w:p></w:tc>'
    '<w:tc><w:tcPr><w:tcW w:w="800" w:type="dxa"/></w:tcPr>'
    '<w:p><w:r><w:t>n2</w:t></w:r></w:p></w:tc></w:tr></w:tbl>'
    '<w:p><w:r><w:t></w:t></w:r></w:p></w:tc>'
    '<w:tc><w:tcPr><w:tcW w:w="1500" w:type="dxa"/>'
    '<w:hMerge w:val="restart"/></w:tcPr>'
    '<w:p><w:r><w:t>hm</w:t></w:r></w:p></w:tc>'
    '<w:tc><w:tcPr><w:hMerge w:val="continue"/></w:tcPr>'
    '<w:p><w:r><w:t></w:t></w:r></w:p></w:tc></w:tr>'
    '<w:tr><w:tc><w:tcPr><w:tcW w:w="1500" w:type="dxa"/>'
    '<w:vMerge w:val="continue"/></w:tcPr>'
    '<w:p><w:r><w:t></w:t></w:r></w:p></w:tc>'
    '<w:tc><w:tcPr><w:tcW w:w="2000" w:type="dxa"/></w:tcPr>'
    '<w:p><w:r><w:t>tail cell</w:t></w:r></w:p></w:tc></w:tr>'
    '</w:tbl>\n'
    '<w:p><w:r><w:t>after table</w:t></w:r></w:p>\n')

# 14. math (m:oMath + oMathPara)
P.append(
    '<w:p><m:oMathPara><m:oMathParaPr><m:jc m:val="center"/>'
    '</m:oMathParaPr>'
    '<m:oMath><m:sSup><m:e><m:r><w:rPr><w:noProof/></w:rPr>'
    '<m:rPr><m:sty m:p="plain"/></m:rPr><m:t>x</m:t></m:r></m:e>'
    '<m:sup><m:r><m:t>2</m:t></m:r></m:sup></m:sSup>'
    '<m:f><m:fPr><m:type m:val="bar"/></m:fPr>'
    '<m:num><m:r><m:t>y</m:t></m:r></m:num>'
    '<m:den><m:r><m:t>z</m:t></m:r></m:den></m:f>'
    '<m:rad><m:deg><m:r><m:t>2</m:t></m:r></m:deg>'
    '<m:e><m:r><m:t>a</m:t></m:r></m:e></m:rad>'
    '<m:nary><m:naryPr><m:chr m:val="&#x222B;"/><m:limLoc '
    'm:val="undOvr"/></m:naryPr>'
    '<m:sub><m:r><m:t>0</m:t></m:r></m:sub>'
    '<m:sup><m:r><m:t>1</m:t></m:r></m:sup>'
    '<m:e><m:r><m:t>dt</m:t></m:r></m:e></m:nary>'
    '<m:d><m:dPr><m:begChr m:val="("/><m:endChr m:val=")"/>'
    '</m:dPr><m:e><m:r><m:t>b</m:t></m:r></m:e></m:d>'
    '<m:func><m:fName><m:r><m:t>sin</m:t></m:r></m:fName>'
    '<m:e><m:r><m:t>t</m:t></m:r></m:e></m:func>'
    '<m:limLow><m:e><m:r><m:t>lim</m:t></m:r></m:e>'
    '<m:lim><m:r><m:t>0</m:t></m:r></m:lim></m:limLow>'
    '<m:limUpp><m:e><m:r><m:t>log</m:t></m:r></m:e>'
    '<m:lim><m:r><m:t>10</m:t></m:r></m:lim></m:limUpp>'
    '<m:m><m:mPr><m:mcs><m:mc><m:mcPr><m:count m:val="2"/>'
    '</m:mcPr></m:mc></m:mcs></m:mPr>'
    '<m:mr><m:e><m:r><m:t>1</m:t></m:r></m:e>'
    '<m:e><m:r><m:t>2</m:t></m:r></m:e></m:mr>'
    '<m:mr><m:e><m:r><m:t>3</m:t></m:r></m:e>'
    '<m:e><m:r><m:t>4</m:t></m:r></m:e></m:mr></m:m>'
    '<m:groupChr><m:groupChrPr><m:chr m:val="&#x23DF;"/>'
    '<m:pos m:val="bot"/></m:groupChrPr>'
    '<m:e><m:r><m:t>g</m:t></m:r></m:e></m:groupChr>'
    '<m:bar><m:barPr><m:pos m:val="top"/></m:barPr>'
    '<m:e><m:r><m:t>h</m:t></m:r></m:e></m:bar>'
    '<m:acc><m:accPr><m:chr m:val="&#x0302;"/></m:accPr>'
    '<m:e><m:r><m:t>i</m:t></m:r></m:e></m:acc>'
    '<m:sSub><m:e><m:r><m:t>u</m:t></m:r></m:e>'
    '<m:sub><m:r><m:t>j</m:t></m:r></m:sub></m:sSub>'
    '<m:sSubSup><m:e><m:r><m:t>v</m:t></m:r></m:e>'
    '<m:sub><m:r><m:t>k</m:t></m:r></m:sub>'
    '<m:sup><m:r><m:t>l</m:t></m:r></m:sup></m:sSubSup>'
    '<m:sPre><m:sub><m:r><m:t>m</m:t></m:r></m:sub>'
    '<m:sup><m:r><m:t>n</m:t></m:r></m:sup>'
    '<m:e><m:r><m:t>o</m:t></m:r></m:e></m:sPre>'
    '<m:box><m:e><m:r><m:t>bx</m:t></m:r></m:e></m:box>'
    '<m:eqArr><m:eqArrPr><m:maxDist m:val="0"/></m:eqArrPr>'
    '<m:e><m:r><m:t>eq1</m:t></m:r></m:e>'
    '<m:e><m:r><m:t>eq2</m:t></m:r></m:e></m:eqArr>'
    '<m:borderBox><m:borderBoxPr><m:hideTop m:val="1"/>'
    '</m:borderBoxPr><m:e><m:r><m:t>bb</m:t></m:r></m:e>'
    '</m:borderBox>'
    '<m:phant><m:phantPr><m:show m:val="0"/></m:phantPr>'
    '<m:e><m:r><m:t>ph</m:t></m:r></m:e></m:phant>'
    '</m:oMath></m:oMathPara></w:p>\n')

# 15. mid-document section break via pPr sectPr
P.append(
    '<w:p><w:pPr><w:sectPr>'
    '<w:type w:val="nextPage"/>'
    '<w:pgSz w:w="11906" w:h="16838" w:code="9"/>'
    '<w:pgMar w:top="1440" w:right="1440" w:bottom="1440" '
    'w:left="1440" w:header="720" w:footer="720" w:gutter="200"/>'
    '<w:cols w:num="2" w:sep="1" w:space="360" w:equalWidth="0">'
    '<w:col w:w="4000" w:space="360"/><w:col w:w="3000"/></w:cols>'
    '<w:docGrid w:type="lines" w:linePitch="312" w:charSpace="20480"/>'
    '<w:paperSrc w:first="1" w:other="2"/>'
    '<w:pgBorders w:offsetFrom="page" w:zOrder="front" '
    'w:display="firstPage"><w:top w:val="single" w:sz="8" '
    'w:space="4" w:color="FF0000"/></w:pgBorders>'
    '<w:lnNumType w:countBy="5" w:start="0" w:distance="240" '
    'w:restart="newPage"/>'
    '<w:pgNumType w:fmt="upperRoman" w:start="7" w:chapStyle="1" '
    'w:chapSep="period"/>'
    '<w:vAlign w:val="center"/><w:noEndnote/>'
    '<w:textDirection w:val="lr"/><w:bidi/>'
    '<w:rtlGutter/><w:printerSettings r:id="rIdPrn1"/>'
    '<w:footnotePr><w:footnote w:id="0"/><w:numFmt w:val="decimal"/>'
    '<w:numStart w:val="1"/><w:numRestart w:val="continuous"/></w:footnotePr>'
    '<w:endnotePr><w:endnote w:id="0"/><w:numFmt w:val="lowerRoman"/>'
    '</w:endnotePr>'
    '<w:sectPrChange w:id="61" w:author="Ed" '
    'w:date="2024-01-02T03:04:05Z"><w:sectPr><w:pgSz w:w="12240" '
    'w:h="15840"/></w:sectPr></w:sectPrChange>'
    '</w:sectPr></w:pPr></w:p>\n'
    '<w:p><w:r><w:t>section two</w:t></w:r></w:p>\n')

# 16. final sectPr w/ header/footer references + titlePg + formProt
P.append(
    '<w:sectPr>'
    '<w:headerReference w:type="default" r:id="rIdHdr1"/>'
    '<w:headerReference w:type="even" r:id="rIdHdr2"/>'
    '<w:headerReference w:type="first" r:id="rIdHdr3"/>'
    '<w:footerReference w:type="default" r:id="rIdFtr1"/>'
    '<w:footerReference w:type="even" r:id="rIdFtr2"/>'
    '<w:footerReference w:type="first" r:id="rIdFtr3"/>'
    '<w:type w:val="oddPage"/>'
    '<w:pgSz w:w="12240" w:h="15840" w:orient="landscape"/>'
    '<w:pgMar w:top="1134" w:right="850" w:bottom="1134" '
    'w:left="1701" w:header="708" w:footer="851" w:gutter="0"/>'
    '<w:cols w:space="708"/><w:docGrid w:type="default"/>'
    '<w:formProt w:val="0"/><w:titlePg/>'
    '<w:footnotePr><w:pos w:val="beneathText"/>'
    '<w:numFmt w:val="chicago"/></w:footnotePr>'
    '<w:endnotePr><w:pos w:val="docEnd"/></w:endnotePr>'
    '</w:sectPr>\n')

document_body = ''.join(P)

# ---- supporting parts --------------------------------------------------

styles_xml = (XMLDECL +
    '<w:styles %s><w:docDefaults>'
    '<w:rPrDefault><w:rPr><w:rFonts w:ascii="Calibri"/>'
    '<w:sz w:val="22"/><w:lang w:val="en-US"/></w:rPr></w:rPrDefault>'
    '<w:pPrDefault><w:pPr><w:spacing w:line="240" '
    'w:lineRule="auto"/></w:pPr></w:pPrDefault></w:docDefaults>'
    '<w:latentStyles w:defLockedState="0" w:defUIPriority="99" '
    'w:defSemiHidden="1" w:defUnhideWhenUsed="1" w:defQFormat="0" '
    'w:count="3"><w:lsdException w:name="Normal" w:locked="0" '
    'w:uiPriority="0" w:semiHidden="0" w:unhideWhenUsed="0" '
    'w:qFormat="1"/><w:lsdException w:name="heading 1" '
    'w:uiPriority="9" w:qFormat="1"/></w:latentStyles>'
    '<w:style w:type="paragraph" w:styleId="Normal" w:default="1">'
    '<w:name w:val="Normal"/><w:qFormat/><w:rsid w:val="00AA0001"/>'
    '<w:uiPriority w:val="0"/><w:pPr/><w:rPr/></w:style>'
    '<w:style w:type="paragraph" w:styleId="Heading1">'
    '<w:name w:val="heading 1"/><w:aliases w:val="H1"/>'
    '<w:basedOn w:val="Normal"/><w:next w:val="Normal"/>'
    '<w:link w:val="Heading1Char"/><w:uiPriority w:val="9"/>'
    '<w:qFormat/><w:locked w:val="0"/><w:semiHidden w:val="0"/>'
    '<w:unhideWhenUsed w:val="0"/><w:rsid w:val="00AA0002"/>'
    '<w:pPr><w:keepNext/><w:spacing w:before="240" w:after="0"/>'
    '<w:outlineLvl w:val="0"/></w:pPr>'
    '<w:rPr><w:b/><w:sz w:val="32"/></w:rPr></w:style>'
    '<w:style w:type="paragraph" w:styleId="ListParagraph">'
    '<w:name w:val="List Paragraph"/><w:basedOn w:val="Normal"/>'
    '<w:pPr><w:ind w:left="720"/></w:pPr></w:style>'
    '<w:style w:type="character" w:styleId="Emphasis" '
    'w:default="0"><w:name w:val="Emphasis"/>'
    '<w:basedOn w:val="DefaultParagraphFont"/><w:qFormat/>'
    '<w:rPr><w:i/></w:rPr></w:style>'
    '<w:style w:type="character" w:styleId="DefaultParagraphFont" '
    'w:default="1"><w:name w:val="Default Paragraph Font"/>'
    '<w:semiHidden/><w:unhideWhenUsed/></w:style>'
    '<w:style w:type="character" w:styleId="Hyperlink">'
    '<w:name w:val="Hyperlink"/><w:rPr><w:color w:val="0563C1"/>'
    '<w:u w:val="single"/></w:rPr></w:style>'
    '<w:style w:type="character" w:styleId="Heading1Char">'
    '<w:name w:val="Heading 1 Char"/><w:basedOn '
    'w:val="DefaultParagraphFont"/><w:qFormat/>'
    '<w:rPr><w:b/></w:rPr></w:style>'
    '<w:style w:type="character" w:styleId="FootnoteReference">'
    '<w:name w:val="footnote reference"/><w:rPr>'
    '<w:vertAlign w:val="superscript"/></w:rPr></w:style>'
    '<w:style w:type="character" w:styleId="EndnoteReference">'
    '<w:name w:val="endnote reference"/><w:rPr>'
    '<w:vertAlign w:val="superscript"/></w:rPr></w:style>'
    '<w:style w:type="character" w:styleId="CommentReference">'
    '<w:name w:val="comment reference"/><w:rPr><w:sz w:val="16"/>'
    '</w:rPr></w:style>'
    '<w:style w:type="character" w:styleId="NoProof">'
    '<w:name w:val="No Proof"/><w:rPr><w:noProof/></w:rPr></w:style>'
    '<w:style w:type="table" w:styleId="TableGrid">'
    '<w:name w:val="Table Grid"/><w:basedOn w:val="TableNormal"/>'
    '<w:pPr/><w:rPr/>'
    '<w:tblPr><w:tblBorders><w:top w:val="single" w:sz="4"/>'
    '</w:tblBorders><w:tblInd w:w="0" w:type="dxa"/></w:tblPr>'
    '<w:trPr/><w:tcPr/>'
    '<w:tblStylePr w:type="firstRow"><w:pPr/><w:rPr><w:b/></w:rPr>'
    '<w:tblPr/><w:trPr/><w:tcPr><w:shd w:fill="DDDDDD"/></w:tcPr>'
    '</w:tblStylePr>'
    '<w:tblStylePr w:type="band1Horz"><w:rPr><w:i/></w:rPr>'
    '</w:tblStylePr>'
    '<w:tblStylePr w:type="lastRow"><w:rPr><w:b/></w:rPr>'
    '</w:tblStylePr>'
    '<w:tblStylePr w:type="firstCol"/><w:tblStylePr w:type="lastCol"/>'
    '<w:tblStylePr w:type="band1Vert"/><w:tblStylePr '
    'w:type="band2Vert"/><w:tblStylePr w:type="band2Horz"/>'
    '<w:tblStylePr w:type="neCell"/><w:tblStylePr w:type="nwCell"/>'
    '<w:tblStylePr w:type="seCell"/><w:tblStylePr w:type="swCell"/>'
    '</w:style>'
    '<w:style w:type="table" w:styleId="TableNormal" w:default="1">'
    '<w:name w:val="Normal Table"/><w:uiPriority w:val="99"/>'
    '<w:semiHidden/><w:unhideWhenUsed/>'
    '<w:tblPr><w:tblCellMar><w:top w:w="0" w:type="dxa"/>'
    '</w:tblCellMar></w:tblPr></w:style>'
    '<w:style w:type="numbering" w:styleId="NoList" w:default="1">'
    '<w:name w:val="No List"/><w:uiPriority w:val="99"/>'
    '<w:semiHidden/><w:unhideWhenUsed/></w:style>'
    '<w:style w:type="paragraph" w:styleId="FootnoteText">'
    '<w:name w:val="footnote text"/><w:pPr/><w:rPr><w:sz w:val="18"/>'
    '</w:rPr></w:style>'
    '<w:style w:type="paragraph" w:styleId="EndnoteText">'
    '<w:name w:val="endnote text"/><w:rPr><w:sz w:val="18"/>'
    '</w:rPr></w:style>'
    '</w:styles>' % W)

numbering_xml = (XMLDECL +
    '<w:numbering %s xmlns:r="%s" '
    'xmlns:v="urn:schemas-microsoft-com:vml" '
    'xmlns:o="urn:schemas-microsoft-com:office:office">'
    '<w:numPicBullet w:numPicBulletId="1">'
    '<w:pict><v:shape id="pb1" o:spid="_x0000_i2001" '
    'type="#_x0000_t75" style="width:6pt;height:6pt">'
    '<v:imagedata r:id="rIdImg1"/></v:shape></w:pict>'
    '</w:numPicBullet>'
    '<w:abstractNum w:abstractNumId="0" w:multiLevelType="hybridMultilevel">'
    '<w:nsid w:val="11111111"/><w:tmpl w:val="22222222"/>'
    '<w:name w:val="outline"/><w:styleLink w:val="Heading1"/>'
    '<w:numStyleLink w:val="NoList"/>'
    '<w:lvl w:ilvl="0"><w:start w:val="1"/>'
    '<w:numFmt w:val="decimal"/><w:pStyle w:val="Heading1"/>'
    '<w:isLgl/><w:lvlRestart w:val="0"/>'
    '<w:lvlText w:val="%%1."/><w:lvlJc w:val="left"/>'
    '<w:pPr><w:tabs><w:tab w:val="num" w:pos="720"/></w:tabs>'
    '<w:ind w:left="720" w:hanging="360"/></w:pPr>'
    '<w:rPr><w:rFonts w:ascii="Calibri" w:hint="default"/>'
    '</w:rPr></w:lvl>'
    '<w:lvl w:ilvl="1"><w:start w:val="1"/>'
    '<w:numFmt w:val="lowerLetter"/><w:lvlText w:val="%%1.%%2.)"/>'
    '<w:lvlJc w:val="left"/><w:pPr><w:ind w:left="1440" '
    'w:hanging="360"/></w:pPr><w:legacy w:legacy="1" '
    'w:legacySpace="720" w:legacyIndent="360"/></w:lvl>'
    '<w:lvl w:ilvl="2"><w:start w:val="1"/>'
    '<w:numFmt w:val="lowerRoman"/><w:lvlText w:val="%%3)"/>'
    '<w:lvlJc w:val="left"/><w:pPr><w:ind w:left="2160" '
    'w:hanging="360"/></w:pPr></w:lvl>'
    '<w:lvl w:ilvl="3" w:tplc="0409001D"><w:start w:val="1"/>'
    '<w:numFmt w:val="upperRoman"/><w:lvlText w:val="%%4."/>'
    '<w:lvlJc w:val="left"/></w:lvl>'
    '<w:lvl w:ilvl="4"><w:start w:val="1"/>'
    '<w:numFmt w:val="upperLetter"/><w:lvlText w:val="%%5."/>'
    '</w:lvl>'
    '<w:lvl w:ilvl="5"><w:start w:val="1"/>'
    '<w:numFmt w:val="ordinal"/><w:lvlText w:val="%%6."/></w:lvl>'
    '<w:lvl w:ilvl="6"><w:start w:val="1"/>'
    '<w:numFmt w:val="cardinalText"/><w:lvlText w:val="%%7."/>'
    '</w:lvl>'
    '<w:lvl w:ilvl="7"><w:start w:val="1"/>'
    '<w:numFmt w:val="ordinalText"/><w:lvlText w:val="%%8."/>'
    '</w:lvl>'
    '<w:lvl w:ilvl="8"><w:start w:val="1"/>'
    '<w:numFmt w:val="ideographDigital"/><w:lvlText w:val="%%9."/>'
    '</w:lvl>'
    '</w:abstractNum>'
    '<w:abstractNum w:abstractNumId="1" '
    'w:multiLevelType="singleLevel"><w:lvl w:ilvl="0">'
    '<w:start w:val="1"/><w:numFmt w:val="bullet"/>'
    '<w:lvlText w:val="&#xF0B7;"/><w:lvlJc w:val="left"/>'
    '<w:pPr><w:ind w:left="720" w:hanging="360"/></w:pPr>'
    '<w:rPr><w:rFonts w:ascii="Symbol" w:hAnsi="Symbol" '
    'w:hint="default"/></w:rPr></w:lvl></w:abstractNum>'
    '<w:abstractNum w:abstractNumId="2" '
    'w:multiLevelType="multilevel"><w:lvl w:ilvl="0">'
    '<w:start w:val="1"/><w:numFmt w:val="upperRoman"/>'
    '<w:lvlText w:val="%%1."/><w:lvlJc w:val="left"/></w:lvl>'
    '</w:abstractNum>'
    '<w:abstractNum w:abstractNumId="3" '
    'w:multiLevelType="singleLevel"><w:lvl w:ilvl="0">'
    '<w:start w:val="1"/><w:numFmt w:val="bullet"/>'
    '<w:lvlText w:val="&#xF0B7;"/>'
    '<w:lvlPicBulletId w:val="1"/></w:lvl></w:abstractNum>'
    '<w:num w:numId="5"><w:abstractNumId w:val="0"/>'
    '<w:lvlOverride w:ilvl="0"><w:startOverride w:val="5"/>'
    '<w:lvl w:ilvl="0"><w:numFmt w:val="decimalEnclosedCircle"/>'
    '</w:lvl></w:lvlOverride>'
    '<w:lvlOverride w:ilvl="1"><w:startOverride w:val="2"/>'
    '</w:lvlOverride></w:num>'
    '<w:num w:numId="6"><w:abstractNumId w:val="1"/></w:num>'
    '<w:num w:numId="7"><w:abstractNumId w:val="2"/></w:num>'
    '<w:num w:numId="8"><w:abstractNumId w:val="3"/></w:num>'
    '</w:numbering>' % (W, R_NS))

settings_flags = [
    'writeProtection', 'view w:val="outline"', 'zoom w:percent="120"',
    'removePersonalInformation', 'removeDateAndTime',
    'doNotDisplayPageBoundries', 'displayBackgroundShape',
    'printPostScriptOverText', 'printFractionalCharacterWidth',
    'printFormsData', 'embedTrueTypeFonts', 'embedSystemFonts',
    'saveSubsetFonts', 'saveFormsData', 'mirrorMargins',
    'alignBordersAndEdges', 'bordersDoNotSurroundHeader',
    'bordersDoNotSurroundFooter', 'gutterAtTop', 'hideSpellingErrors',
    'hideGrammaticalErrors', 'proofState w:spelling="clean" '
    'w:grammar="clean"', 'formsDesign', 'linkStyles',
    'stylePaneFormatFilter w:val="3F01"',
    'stylePaneSortMethod w:val="name"',
    'documentType w:val="letter"', 'revisionView w:markup="1" '
    'w:comments="1" w:insDel="1" w:formatting="1" '
    'w:inkAnnotations="1"', 'documentProtection w:edit="readOnly" '
    'w:formatting="1" w:enforcement="0"',
    'autoFormatOverride', 'styleLockTheme', 'styleLockQFset',
    'defaultTabStop w:val="360"', 'autoHyphenation',
    'consecutiveHyphenLimit w:val="3"',
    'hyphenationZone w:val="425"', 'doNotHyphenateCaps',
    'showEnvelope', 'summaryLength w:val="30"',
    'clickAndTypeStyle w:val="Heading1"',
    'defaultTableStyle w:val="TableGrid"', 'evenAndOddHeaders',
    'bookFoldRevPrinting', 'bookFoldPrinting',
    'bookFoldPrintingSheets w:val="8"',
    'drawingGridHorizontalSpacing w:val="120"',
    'drawingGridVerticalSpacing w:val="120"',
    'displayHorizontalDrawingGridEvery w:val="2"',
    'displayVerticalDrawingGridEvery w:val="2"',
    'doNotUseMarginsForDrawingGridOrigin',
    'drawingGridHorizontalOrigin w:val="240"',
    'drawingGridVerticalOrigin w:val="240"',
    'doNotUseEastAsianBreakRules', 'useAltKinsokuLineBreakRules',
    'useAnsiKerningPairs', 'useFELayout', 'useNormalStyleForList',
    'usePrinterMetrics', 'useSingleBorderforContiguousCells',
    'useWord2002TableStyleRules', 'useWord97LineBreakRules',
    'useXSLTWhenSaving', 'allowPNG', 'alwaysShowPlaceholderText',
    'alwaysMergeEmptyNamespace', 'cachedColBalance',
    'characterSpacingControl w:val="doNotCompress"',
    'footnoteLayoutLikeWW8', 'shapeLayoutLikeWW8',
    'autoSpaceLikeWord95', 'balanceSingleByteDoubleByteWidth',
    'doNotAutoCompressPictures', 'doNotAutofitConstrainedTables',
    'doNotBreakConstrainedForcedTable', 'doNotBreakWrappedTables',
    'doNotDemarcateInvalidXml', 'doNotEmbedSmartTags',
    'doNotExpandShiftReturn', 'doNotIncludeSubdocsInStats',
    'doNotLeaveBackslashAlone', 'doNotOrganizeInFolder',
    'doNotRelyOnCSS', 'doNotSaveAsSingleFile', 'doNotShadeFormData',
    'doNotSnapToGridInCell', 'doNotSuppressBlankLines',
    'doNotSuppressIndentation', 'doNotSuppressParagraphBorders',
    'doNotTrackFormatting', 'doNotTrackMoves',
    'doNotUseIndentAsNumberingTabStop', 'doNotUseLongFileNames',
    'doNotValidateAgainstSchema', 'doNotVertAlignCellWithSp',
    'doNotVertAlignInTxbx', 'doNotWrapTextWithPunct',
    'lineWrapLikeWord6', 'mwSmallCaps', 'noColumnBalance',
    'noExtraLineSpacing', 'noLeading', 'noLineBreaksAfter',
    'noLineBreaksBefore', 'noPunctuationKerning', 'noResizeAllowed',
    'noSpaceRaiseLower', 'noTabHangInd', 'printBodyTextBeforeHeader',
    'printColBlack', 'printTwoOnOne', 'selectFldWithFirstOrLastChar',
    'semiHidden', 'showBreaksInFrames', 'spaceForUL',
    'spacingInWholePoints', 'splitPgBreakAndParaMark',
    'strictFirstAndLastChars', 'subFontBySize', 'subDoc',
    'suppressBottomSpacing', 'suppressSpacingAtTopOfPage',
    'suppressSpBfAfterPgBrk', 'suppressTopSpacing',
    'suppressTopSpacingWP', 'swapBordersFacingPages',
    'truncateFontHeightsLikeWP6', 'uiCompat97To2003',
    'ulTrailSpace', 'underlineTabInNumLists', 'updateFields w:val="1"',
    'usePrinterMetrics', 'wpJustification', 'wpSpaceWidth w:val="120"',
    'wrapTrailSpaces', 'decimalSymbol w:val=","',
    'listSeparator w:val=";"',
]
settings_xml = (XMLDECL + '<w:settings %s '
    'xmlns:r="%s" '
    'xmlns:v="urn:schemas-microsoft-com:vml" '
    'xmlns:o="urn:schemas-microsoft-com:office:office">' % (W, R_NS)
    + ''.join('<w:%s/>' % f for f in settings_flags)
    + '<w:captions><w:autoCaptions/><w:caption w:name="app" '
    'w:pos="above" w:chapNum="1" w:heading="1" w:noLabel="0" '
    'w:numFmt="decimal" w:sep="period"/></w:captions>'
    '<w:compat>'
    + ''.join('<w:%s/>' % f for f in
              ['adjustLineHeightInTable', 'applyBreakingRules',
               'autofitToFirstFixedWidthCell', 'compatSetting '
               'w:name="compatibilityMode" w:uri="http://schemas.'
               'microsoft.com/office/word" w:val="15"',
               'convMailMergeEsc', 'doNotVertAlignCellWithSp',
               'forgetLastTabAlignment', 'growAutofit',
               'layoutRawTableWidth', 'layoutTableRowsApart',
               'noLeading', 'noSpaceRaiseLower',
               'useSingleBorderforContiguousCells',
               'useWord2002TableStyleRules', 'wpJustification',
               'wpSpaceWidth w:val="160"'])
    + '</w:compat>'
    '<w:rsids><w:rsidRoot w:val="00AA0001"/>'
    '<w:rsid w:val="00AA0001"/><w:rsid w:val="00AA0002"/></w:rsids>'
    '<m:mathPr xmlns:m="http://schemas.openxmlformats.org/'
    'officeDocument/2006/math"><m:mathFont m:val="Cambria Math"/>'
    '<m:brkBin m:val="before"/><m:brkBinSub m:val="--"/>'
    '<m:smallFrac m:val="0"/><m:dispDef/><m:lMargin m:val="0"/>'
    '<m:rMargin m:val="0"/><m:defJc m:val="centerGroup"/>'
    '<m:wrapIndent m:val="1440"/><m:intLim m:val="subSup"/>'
    '<m:naryLim m:val="undOvr"/></m:mathPr>'
    '<w:themeFontLang w:val="en-US" w:eastAsia="ja-JP" '
    'w:bidi="ar-SA"/>'
    '<w:clrSchemeMapping w:bg1="lt1" w:t1="dk1" w:bg2="lt2" '
    'w:t2="dk2" w:accent1="accent1" w:accent2="accent2" '
    'w:accent3="accent3" w:accent4="accent4" w:accent5="accent5" '
    'w:accent6="accent6" w:hyperlink="hyperlink" '
    'w:followedHyperlink="followedHyperlink"/>'
    '<w:shapeDefaults><o:shapedefaults v:ext="edit" spidmax="2049" '
    'xmlns:o="urn:schemas-microsoft-com:office:office"/>'
    '<o:shapelayout v:ext="edit" '
    'xmlns:o="urn:schemas-microsoft-com:office:office"/>'
    '</w:shapeDefaults>'
    '<w:attachedTemplate r:id="rIdTpl1"/>'
    '<w:attachedSchema w:val="schema1"/>'
    '<w:mailMerge><w:mainDocumentType w:val="formLetters"/>'
    '<w:linkToQuery/><w:dataType w:val="textFile"/>'
    '<w:connectString w:val="constr"/>'
    '<w:query w:val="q"/><w:dataSource r:id="rIdDs1"/>'
    '<w:headerSource r:id="rIdHs1"/>'
    '<w:doNotSuppressBlankLines/>'
    '<w:destination w:val="newDocument"/>'
    '<w:addressFieldName w:val="af1"/>'
    '<w:mailSubject w:val="msub"/>'
    '<w:mailAsAttachment w:val="0"/>'
    '<w:viewMergedData/><w:activeRecord w:val="1"/>'
    '<w:checkErrors w:val="1"/>'
    '<w:odso><w:udl w:val="u"/><w:table w:val="t"/>'
    '<w:src r:id="rIdDs1"/><w:colDelim w:val="9"/>'
    '<w:type w:val="database"/>'
    '<w:fHdr/><w:fieldMapData><w:type w:val="dbColumn"/>'
    '<w:name w:val="nm"/><w:mappedName w:val="mn"/>'
    '<w:dynamicAddress/><w:lid w:val="en-US"/></w:fieldMapData>'
    '<w:recipientData><w:active w:val="1"/><w:column w:val="1"/>'
    '<w:uniqueTag w:val="dGFn"/></w:recipientData></w:odso>'
    '</w:mailMerge>'
    '<w:hdrShapeDefaults/>'
    '<w:footnotePr><w:footnote w:id="0"/><w:footnote w:id="1"/></w:footnotePr>'
    '<w:endnotePr><w:endnote w:id="0"/></w:endnotePr>'
    '<w:autoCaptions><w:autoCaption w:name="app"/>'
    '</w:autoCaptions>'
    '<w:docVars><w:docVar w:name="dv1" w:val="v1"/></w:docVars>'
    '<w:uiPriority w:val="1"/>'
    '</w:settings>')

fonttable_xml = (XMLDECL +
    '<w:fonts %s xmlns:r="%s"><w:font w:name="Calibri">'
    '<w:altName w:val="Carlito"/><w:panose1 w:val="020F0502020204030204"/>'
    '<w:charset w:val="00"/><w:family w:val="swiss"/>'
    '<w:pitch w:val="variable"/><w:embedRegular r:id="rIdF1"/>'
    '<w:embedBold r:id="rIdF2"/><w:embedItalic r:id="rIdF3"/>'
    '<w:embedBoldItalic r:id="rIdF4"/>'
    '<w:sig w:usb0="E00002FF" w:usb1="4000ACFF" w:usb2="00000001" '
    'w:usb3="00000000" w:csb0="0000009F" w:csb1="00000000"/>'
    '</w:font>'
    '<w:font w:name="MS Mincho"><w:family w:val="roman"/>'
    '<w:charset w:val="80"/><w:pitch w:val="fixed"/>'
    '<w:notTrueType/></w:font>'
    '<w:font w:name="Symbol"><w:family w:val="auto"/>'
    '<w:charset w:val="02"/></w:font>'
    '</w:fonts>' % (W, R_NS))

footnotes_xml = (XMLDECL +
    '<w:footnotes %s><w:footnote w:type="separator" w:id="0">'
    '<w:p><w:r><w:separator/></w:r></w:p></w:footnote>'
    '<w:footnote w:type="continuationSeparator" w:id="1">'
    '<w:p><w:r><w:continuationSeparator/></w:r></w:p></w:footnote>'
    '<w:footnote w:type="continuationNotice" w:id="3">'
    '<w:p><w:r><w:t>cont</w:t></w:r></w:p></w:footnote>'
    '<w:footnote w:id="2"><w:p><w:pPr><w:pStyle '
    'w:val="FootnoteText"/></w:pPr><w:r><w:footnoteRef/></w:r>'
    '<w:r><w:t>note one</w:t></w:r></w:p></w:footnote>'
    '<w:footnote w:id="4"><w:p><w:r><w:t>unused note</w:t></w:r>'
    '</w:p></w:footnote></w:footnotes>' % W)

endnotes_xml = (XMLDECL +
    '<w:endnotes %s><w:endnote w:type="separator" w:id="0">'
    '<w:p><w:r><w:separator/></w:r></w:p></w:endnote>'
    '<w:endnote w:type="continuationSeparator" w:id="1">'
    '<w:p><w:r><w:continuationSeparator/></w:r></w:p></w:endnote>'
    '<w:endnote w:id="2"><w:p><w:r><w:endnoteRef/></w:r>'
    '<w:r><w:t>end one</w:t></w:r></w:p></w:endnote>'
    '</w:endnotes>' % W)

comments_xml = (XMLDECL +
    '<w:comments %s><w:comment w:id="0" w:author="Ann Author" '
    'w:date="2024-03-04T05:06:07Z" w:initials="AA">'
    '<w:p><w:r><w:annotationRef/></w:r>'
    '<w:r><w:t>comment body</w:t></w:r></w:p></w:comment>'
    '</w:comments>' % W)

hdr1 = (XMLDECL + '<w:hdr %s><w:p><w:pPr><w:pStyle '
    'w:val="Header"/></w:pPr><w:r><w:t>default hdr</w:t></w:r>'
    '<w:r><w:fldChar w:fldCharType="begin"/></w:r>'
    '<w:r><w:instrText> PAGE </w:instrText></w:r>'
    '<w:r><w:fldChar w:fldCharType="end"/></w:r></w:p>'
    '<w:tbl><w:tblGrid><w:gridCol w:w="1000"/></w:tblGrid>'
    '<w:tr><w:tc><w:p><w:r><w:t>hc</w:t></w:r></w:p></w:tc></w:tr>'
    '</w:tbl></w:hdr>' % W)
hdr2 = (XMLDECL + '<w:hdr %s><w:p><w:r><w:t>even hdr</w:t></w:r>'
    '</w:p></w:hdr>' % W)
hdr3 = (XMLDECL + '<w:hdr %s><w:p><w:r><w:t>first hdr</w:t></w:r>'
    '</w:p></w:hdr>' % W)
ftr1 = (XMLDECL + '<w:ftr %s><w:p><w:r><w:t>default ftr</w:t></w:r>'
    '</w:p></w:ftr>' % W)
ftr2 = (XMLDECL + '<w:ftr %s><w:p><w:r><w:t>even ftr</w:t></w:r>'
    '</w:p></w:ftr>' % W)
ftr3 = (XMLDECL + '<w:ftr %s><w:p><w:r><w:t>first ftr</w:t></w:r>'
    '</w:p></w:ftr>' % W)

theme_xml = (XMLDECL +
    '<a:theme xmlns:a="http://schemas.openxmlformats.org/drawingml/'
    '2006/main" name="T"><a:themeElements>'
    '<a:clrScheme name="C"><a:dk1><a:sysClr val="windowText" '
    'lastClr="000000"/></a:dk1><a:lt1><a:sysClr val="window" '
    'lastClr="FFFFFF"/></a:lt1><a:dk2><a:srgbClr val="1F497D"/>'
    '</a:dk2><a:lt2><a:srgbClr val="EEECE1"/></a:lt2>'
    '<a:accent1><a:srgbClr val="4F81BD"/></a:accent1>'
    '<a:accent2><a:srgbClr val="C0504D"/></a:accent2>'
    '<a:accent3><a:srgbClr val="9BBB59"/></a:accent3>'
    '<a:accent4><a:srgbClr val="8064A2"/></a:accent4>'
    '<a:accent5><a:srgbClr val="4BACC6"/></a:accent5>'
    '<a:accent6><a:srgbClr val="F79646"/></a:accent6>'
    '<a:hlink><a:srgbClr val="0000FF"/></a:hlink>'
    '<a:folHlink><a:srgbClr val="800080"/></a:folHlink></a:clrScheme>'
    '<a:fontScheme name="F"><a:majorFont><a:latin typeface="Cambria"/>'
    '<a:ea typeface=""/><a:cs typeface=""/></a:majorFont>'
    '<a:minorFont><a:latin typeface="Calibri"/><a:ea typeface=""/>'
    '<a:cs typeface=""/></a:minorFont></a:fontScheme>'
    '<a:fmtScheme name="Fmt"><a:fillStyleLst>'
    '<a:solidFill><a:schemeClr val="phClr"/></a:solidFill>'
    '<a:solidFill><a:schemeClr val="phClr"/></a:solidFill>'
    '<a:solidFill><a:schemeClr val="phClr"/></a:solidFill>'
    '</a:fillStyleLst><a:lnStyleLst>'
    '<a:ln w="9525" cap="flat" cmpd="sng" algn="ctr">'
    '<a:solidFill><a:schemeClr val="phClr"/></a:solidFill>'
    '<a:prstDash val="solid"/></a:ln>'
    '<a:ln w="25400"><a:solidFill><a:schemeClr val="phClr"/>'
    '</a:solidFill></a:ln>'
    '<a:ln w="38100"><a:solidFill><a:schemeClr val="phClr"/>'
    '</a:solidFill></a:ln></a:lnStyleLst>'
    '<a:effectStyleLst><a:effectStyle><a:effectLst/></a:effectStyle>'
    '<a:effectStyle><a:effectLst/></a:effectStyle>'
    '<a:effectStyle><a:effectLst><a:outerShdw blurRad="40000" '
    'dist="20000" dir="5400000" rotWithShape="0">'
    '<a:srgbClr val="000000"><a:alpha val="40000"/></a:srgbClr>'
    '</a:outerShdw></a:effectLst></a:effectStyle></a:effectStyleLst>'
    '<a:bgFillStyleLst><a:solidFill><a:schemeClr val="phClr"/>'
    '</a:solidFill></a:bgFillStyleLst></a:fmtScheme>'
    '</a:themeElements><a:objectDefaults/><a:extraClrSchemeLst/>'
    '</a:theme>')

CT_W = 'application/vnd.openxmlformats-officedocument.wordprocessingml.'
make_docx(
    os.path.join(OUT, 'cov11.docx'),
    document_body,
    [('rIdStyles', REL + 'styles', 'styles.xml'),
     ('rIdSettings', REL + 'settings', 'settings.xml'),
     ('rIdNumbering', REL + 'numbering', 'numbering.xml'),
     ('rIdFontTable', REL + 'fontTable', 'fontTable.xml'),
     ('rIdTheme', REL + 'theme', 'theme/theme1.xml'),
     ('rIdFootnotes', REL + 'footnotes', 'footnotes.xml'),
     ('rIdEndnotes', REL + 'endnotes', 'endnotes.xml'),
     ('rIdComments', REL + 'comments', 'comments.xml'),
     ('rIdHdr1', REL + 'header', 'header1.xml'),
     ('rIdHdr2', REL + 'header', 'header2.xml'),
     ('rIdHdr3', REL + 'header', 'header3.xml'),
     ('rIdFtr1', REL + 'footer', 'footer1.xml'),
     ('rIdFtr2', REL + 'footer', 'footer2.xml'),
     ('rIdFtr3', REL + 'footer', 'footer3.xml'),
     ('rIdImg1', REL + 'image', 'media/image1.png'),
     ('rIdLink1', REL + 'hyperlink', 'http://example.invalid/x',
      ' TargetMode="External"'),
     ('rIdOle1', REL + 'oleObject', 'oleObject1.bin'),
     ('rIdOle2', REL + 'oleObject', 'oleObject2.bin'),
     ('rIdPrn1', REL + 'printerSettings', 'printerSettings1.bin'),
     ('rIdTpl1', REL + 'attachedTemplate', 'template.dotx',
      ' TargetMode="External"'),
     ('rIdDs1', REL + 'oleObject', 'datasource.bin'),
     ('rIdHs1', REL + 'oleObject', 'headersource.bin'),
     ('rIdF1', REL + 'font', 'fonts/f1.odttf'),
     ('rIdF2', REL + 'font', 'fonts/f2.odttf'),
     ('rIdF3', REL + 'font', 'fonts/f3.odttf'),
     ('rIdF4', REL + 'font', 'fonts/f4.odttf'),
    ],
    [('word/styles.xml', styles_xml),
     ('word/settings.xml', settings_xml),
     ('word/numbering.xml', numbering_xml),
     ('word/fontTable.xml', fonttable_xml),
     ('word/theme/theme1.xml', theme_xml),
     ('word/footnotes.xml', footnotes_xml),
     ('word/endnotes.xml', endnotes_xml),
     ('word/comments.xml', comments_xml),
     ('word/header1.xml', hdr1),
     ('word/header2.xml', hdr2),
     ('word/header3.xml', hdr3),
     ('word/footer1.xml', ftr1),
     ('word/footer2.xml', ftr2),
     ('word/footer3.xml', ftr3),
     ('word/media/image1.png', PNG_RED),
     ('word/oleObject1.bin', b'\x01' * 32),
     ('word/oleObject2.bin', b'\x02' * 32),
     ('word/printerSettings1.bin', b'\x03' * 16),
     ('word/datasource.bin', b'\x04' * 16),
     ('word/headersource.bin', b'\x05' * 16),
     ('word/fonts/f1.odttf', b'\x00' * 32),
    ],
    extra_types=[
        ('/word/document.xml', CT_W + 'document.main+xml'),
        ('/word/styles.xml', CT_W + 'styles+xml'),
        ('/word/settings.xml', CT_W + 'settings+xml'),
        ('/word/numbering.xml', CT_W + 'numbering+xml'),
        ('/word/fontTable.xml', CT_W + 'fontTable+xml'),
        ('/word/theme/theme1.xml', 'application/vnd.openxmlformats-'
         'officedocument.theme+xml'),
        ('/word/footnotes.xml', CT_W + 'footnotes+xml'),
        ('/word/endnotes.xml', CT_W + 'endnotes+xml'),
        ('/word/comments.xml', CT_W + 'comments+xml'),
        ('/word/header1.xml', CT_W + 'header+xml'),
        ('/word/header2.xml', CT_W + 'header+xml'),
        ('/word/header3.xml', CT_W + 'header+xml'),
        ('/word/footer1.xml', CT_W + 'footer+xml'),
        ('/word/footer2.xml', CT_W + 'footer+xml'),
        ('/word/footer3.xml', CT_W + 'footer+xml'),
        ('/word/fonts/f1.odttf', 'application/vnd.openxmlformats-'
         'officedocument.obfuscatedFont'),
    ])


# ==== cov11.odt =========================================================

O = 'urn:oasis:names:tc:opendocument:xmlns:'
content_xml = (XMLDECL +
    '<office:document-content '
    'xmlns:office="%soffice/1.0" '
    'xmlns:text="%stext/1.0" '
    'xmlns:style="%sstyle/1.0" '
    'xmlns:fo="%sxsl-fo-compatible/1.0" '
    'xmlns:svg="%ssvg-compatible/1.0" '
    'xmlns:table="%stable/1.0" '
    'xmlns:draw="%sdrawing/1.0" '
    'xmlns:xlink="http://www.w3.org/1999/xlink" '
    'xmlns:number="%sdatastyle/1.0" '
    'xmlns:config="%sconfig/1.0" '
    'xmlns:form="%sform/1.0" '
    'xmlns:field="urn:openoffice:names:experimental:ooo-ms-interop:'
    'schemas:field:1.0" '
    'office:version="1.2">'
    % (O, O, O, O, O, O, O, O, O, O)
    + '<office:scripts/>'
    '<office:font-face-decls>'
    '<style:font-face style:name="Calibri" svg:font-family="Calibri" '
    'style:font-family-generic="swiss" style:font-pitch="variable"/>'
    '<style:font-face style:name="Cambria" '
    'svg:font-family="Cambria"/></office:font-face-decls>'
    '<office:automatic-styles>'
    '<style:style style:name="P1" style:family="paragraph">'
    '<style:paragraph-properties fo:margin-top="0.2in" '
    'fo:margin-bottom="0.1in" fo:text-align="justify" '
    'style:justify-single-word="true" fo:text-indent="0.25in" '
    'style:text-autospace="ideograph-alpha" '
    'fo:break-before="page" fo:keep-with-next="always" '
    'fo:orphans="3" fo:widows="4" fo:hyphenate="true" '
    'fo:hyphenation-remainder-char-count="2" '
    'fo:hyphenation-push-char-count="3" '
    'fo:hyphenation-ladder-count="2" '
    'fo:border="0.06pt solid #FF0000" fo:padding="0.05in" '
    'fo:background-color="#FFFFAA" style:shadow="1pt 1pt #808080" '
    'style:tab-stop-distance="0.4in">'
    '<style:tab-stops><style:tab-stop style:position="2in" '
    'style:type="center" style:leader-style="dotted" '
    'style:leader-text="."/></style:tab-stops>'
    '<style:drop-cap style:length="2" style:distance="0.05in"/>'
    '<style:background-image/></style:paragraph-properties>'
    '<style:text-properties fo:font-size="14pt" '
    'fo:font-style="italic" fo:font-weight="bold" '
    'fo:color="#224466" style:text-underline-style="solid" '
    'style:text-underline-type="double" '
    'style:text-underline-color="font-color" '
    'style:text-line-through-style="solid" '
    'style:text-line-through-type="double" '
    'fo:letter-spacing="0.05em" fo:text-transform="uppercase" '
    'fo:text-shadow="1pt 1pt #999999" '
    'style:font-name="Calibri" fo:language="en" fo:country="US" '
    'style:text-position="super 80%" '
    'style:text-relief="embossed" style:text-outline="true" '
    'style:text-blinking="false" style:text-combine="letters" '
    'style:text-combine-start-char="(" '
    'style:text-combine-end-char=")" style:text-emphasize="dot" '
    'style:text-emphasize-position="below" '
    'style:text-rotation-angle="0" style:text-scale="110%" '
    'style:letter-kerning="true" style:text-overline-style="solid" '
    'style:text-overline-type="double" '
    'style:text-overline-color="#FF0000" '
    'style:text-overline-width="auto" style:text-overline-mode="continuous" '
    'style:text-underline-mode="continuous" '
    'style:text-line-through-mode="continuous"/></style:style>'
    '<style:style style:name="P2" style:family="paragraph">'
    '<style:paragraph-properties style:line-break="strict" '
    'style:punctuation-wrap="hanging" '
    'style:text-align-vertical="top" '
    'style:page-number="0" fo:line-height="150%"/>'
    '<style:text-properties fo:font-size="10pt" '
    'style:text-position="sub 50%" fo:text-transform="none"/>'
    '</style:style>'
    '<style:style style:name="T1" style:family="text">'
    '<style:text-properties fo:color="#008000" '
    'style:text-underline-style="wave" '
    'style:text-underline-color="#FF0000"/></style:style>'
    '<style:style style:name="Tbl" style:family="table">'
    '<style:table-properties style:width="5in" '
    'table:align="margins" style:may-break-between-rows="false"/>'
    '</style:style>'
    '<style:style style:name="Tc" style:family="table-cell">'
    '<style:table-cell-properties fo:border="0.5pt solid #000" '
    'fo:background-color="#EEEEFF" fo:padding="0.05in" '
    'style:vertical-align="middle" style:rotation-angle="0" '
    'style:rotation-align="bottom" style:shrink-to-fit="false"/>'
    '</style:style>'
    '<style:style style:name="Tr" style:family="table-row">'
    '<style:table-row-properties fo:min-row-height="0.4in" '
    'style:row-height="0.5in"/></style:style>'
    '<style:style style:name="Tcol" style:family="table-column">'
    '<style:table-column-properties style:column-width="1.5in" '
    'style:rel-column-width="1"/></style:style>'
    '<style:style style:name="Frame1" style:family="graphic">'
    '<style:graphic-properties draw:style-name="Frame1" '
    'style:vertical-pos="top" style:vertical-rel="paragraph" '
    'style:horizontal-pos="right" style:horizontal-rel="page" '
    'svg:x="2in" svg:y="0.5in" svg:width="1.5in" svg:height="0.8in" '
    'fo:padding="0.05in" fo:border="0.5pt solid #006699" '
    'style:wrap="dynamic" style:number-wrapped-paragraphs="2" '
    'style:wrap-contour="false" fo:margin-left="0.1in" '
    'style:print-content="true"/></style:style>'
    '<style:style style:name="FrameImg" style:family="graphic">'
    '<style:graphic-properties style:wrap="none" svg:width="0.6in" '
    'svg:height="0.4in" style:mirror="horizontal"/></style:style>'
    '<style:style style:name="List1" style:family="list">'
    '<text:list-level-style-number text:level="1" '
    'style:num-format="1" text:display-levels="1" '
    'style:num-prefix="(" style:num-suffix=")" '
    'style:num-letter-sync="true" text:start-value="3"/>'
    '<text:list-level-style-number text:level="2" '
    'style:num-format="a" text:display-levels="2"/>'
    '<text:list-level-style-number text:level="3" '
    'style:num-format="i" text:display-levels="3"/>'
    '<text:list-level-style-bullet text:level="4" '
    'style:num-format=" " text:bullet-char="&#x2022;"/>'
    '<text:list-level-style-image text:level="5" '
    'xlink:href="Pictures/p.png" xlink:type="simple" '
    'xlink:show="embed" xlink:actuate="onLoad"/>'
    '</style:style>'
    '<number:number-style style:name="N1">'
    '<number:number number:decimal-places="2" '
    'number:min-integer-digits="3" number:grouping="true" '
    'number:display-factor="100"/>'
    '<number:text> USD</number:text></number:number-style>'
    '<number:date-style style:name="D1">'
    '<number:year number:style="long"/><number:text>-</number:text>'
    '<number:month number:style="long" '
    'number:textual="true"/></number:date-style>'
    '<number:boolean-style style:name="B1"><number:boolean/>'
    '</number:boolean-style>'
    '<number:percentage-style style:name="Pct1">'
    '<number:number number:decimal-places="1"/></number:percentage-style>'
    '<number:text-style style:name="Tx1"><number:text-content/>'
    '</number:text-style>'
    '</office:automatic-styles>'
    '<office:body><office:text '
    'text:use-soft-page-breaks="true">'
    '<text:tracked-changes/>'
    '<text:sequence-decls><text:sequence-decl text:name="fig" '
    'text:display-outline-level="1"/></text:sequence-decls>'

    '<text:h text:style-name="Heading_20_1" text:outline-level="1">'
    'Heading one</text:h>'
    '<text:h text:style-name="Heading_20_2" text:outline-level="2" '
    'text:is-list-header="true">Heading two</text:h>'
    '<text:h text:style-name="Heading_20_3" text:outline-level="3" '
    'text:restart-numbering="true" text:start-value="5">H3</text:h>'

    '<text:p text:style-name="P1">para <text:span '
    'text:style-name="T1">styled</text:span> body '
    '<text:s/>spaces<text:tab/>tabbed<text:line-break/>broken '
    '<text:bookmark-start text:name="bm1"/>marked'
    '<text:bookmark-end text:name="bm1"/> '
    '<text:bookmark text:name="bm2"/> '
    '<text:soft-page-break/>next'
    '</text:p>'
    '<text:p text:style-name="P2">second '
    '<text:a xlink:href="http://example.invalid/" '
    'xlink:type="simple">link</text:a> and '
    '<text:a xlink:href="#bm1" xlink:type="simple">inner</text:a>'
    '</text:p>'

    '<text:p><text:page-number text:select-page="current" '
    'text:page-adjust="2" text:fixed="false"/> '
    '<text:page-count/> <text:date '
    'style:data-style-name="D1" text:date-value="2024-01-02" '
    'text:fixed="true"/> <text:time '
    'text:time-value="09:41:00" text:fixed="true"/> '
    '<text:print-date/><text:print-time/>'
    '<text:creation-date/><text:creation-time/>'
    '<text:editing-duration/>'
    '<text:modification-date/><text:modification-time/>'
    '<text:description/><text:user-defined text:name="ud1">'
    'ud</text:user-defined>'
    '<text:sender-firstname/><text:sender-lastname/>'
    '<text:sender-initials/><text:sender-title/>'
    '<text:sender-position/><text:sender-email/>'
    '<text:sender-phone-private/><text:sender-company/>'
    '<text:sender-street/><text:sender-city/>'
    '<text:sender-postal-code/><text:sender-country/>'
    '<text:author-name/><text:author-initials/>'
    '<text:chapter text:display="name" text:outline-level="1"/>'
    '<text:file-name text:display="full"/>'
    '<text:template-name/>'
    '<text:word-count/><text:paragraph-count/>'
    '<text:character-count/><text:table-count/>'
    '<text:image-count/><text:object-count/>'
    '<text:page-variable-get/><text:page-variable-set '
    'text:active="true"/>'
    '<text:variable-set text:name="v1" office:value="42" '
    'office:value-type="float" text:formula="ooow:1+1">v1'
    '</text:variable-set>'
    '<text:variable-get text:name="v1"/>'
    '<text:variable-input text:name="v2" office:value-type="string" '
    'text:description="d" text:formula="ooow:x">v2'
    '</text:variable-input>'
    '<text:sequence text:name="fig" text:formula="ooow:fig+1" '
    'text:num-format="1" style:num-letter-sync="true"/>'
    '<text:expression text:name="e1" office:value-type="float" '
    'office:value="3" text:formula="ooow:1+2" '
    'text:display="formula"/>'
    '<text:reference-ref text:name="r1" text:ref-name="bm1" '
    'text:reference-format="page">r</text:reference-ref>'
    '<text:bookmark-ref text:ref-name="bm1" '
    'text:reference-format="text">b</text:bookmark-ref>'
    '<text:note-ref text:note-class="footnote" '
    'text:ref-name="n1" text:reference-format="page"/>'
    '<text:sequence-ref text:ref-name="s1" '
    'text:reference-format="caption"/>'
    '<text:get-ref text:name="g1">g</text:get-ref>'
    '<text:sheet-name/><text:title/>'
    '<text:initial-creator/><text:editing-cycles/>'
    '<text:hidden-text text:is-hidden="true" '
    'text:string-value="hidden" '
    'text:condition="ooow:0">cond</text:hidden-text>'
    '<text:hidden-paragraph text:is-hidden="true" '
    'text:condition="ooow:0"/>'
    '<text:placeholder text:placeholder-type="text" '
    'text:description="ph">ph</text:placeholder>'
    '<text:measure text:kind="value">3cm</text:measure>'
    '<text:user-field-decl office:value-type="float" '
    'office:value="9.5" text:name="uf1"/>'
    '<text:user-field-get text:name="uf1"/>'
    '<text:user-field-input text:name="uf1"/>'
    '<text:dde-connection-decl office:name="dde1" '
    'text:command-application="app" '
    'text:command-item="item" text:command-element="elem" '
    'text:automatic-update="false"/>'
    '<text:subject/><text:database-name '
    'text:database-name="db" text:table-name="tb" '
    'text:table-type="table"/>'
    '<text:database-display text:database-name="db" '
    'text:table-name="tb" text:column-name="c"/>'
    '<text:database-next text:database-name="db" '
    'text:table-name="tb"/>'
    '<text:database-row-select text:database-name="db" '
    'text:table-name="tb" text:row-number="2"/>'
    '<text:conditional-text text:condition="ooow:1" '
    'text:string-value-if-true="t" '
    'text:string-value-if-false="f">c</text:conditional-text>'
    '<text:execute-macro text:target="mac" '
    'text:name="m">m</text:execute-macro>'
    '<text:biblio-mark text:identifier="b1" '
    'text:bibliography-type="article" text:author="a" '
    'text:title="t" text:year="2024"/>'
    '</text:p>'

    '<text:p>note one<text:note text:id="fn1" '
    'text:note-class="footnote">'
    '<text:note-citation text:label="*" text:body="b">*</text:note-citation>'
    '<text:note-body><text:p>foot body</text:p></text:note-body>'
    '</text:note> end note<text:note text:note-class="endnote">'
    '<text:note-citation>1</text:note-citation>'
    '<text:note-body><text:p>end body</text:p></text:note-body>'
    '</text:note> ann<office:annotation '
    'text:name="ann1" office:display="true">'
    '<dc:creator xmlns:dc="http://purl.org/dc/elements/1.1/">Me'
    '</dc:creator><dc:date '
    'xmlns:dc="http://purl.org/dc/elements/1.1/">2024-01-02'
    '</dc:date><text:p>ann body</text:p></office:annotation>'
    '<office:annotation-end text:name="ann1"/>'
    '</text:p>'

    '<text:alphabetical-index-mark text:string-value="im" '
    'text:key1="k1" text:key2="k2" text:main-entry="true"/>'
    '<text:bibliography-mark text:identifier="b2"/>'
    '<text:toc-mark-start text:id="tm1"/>toc mark'
    '<text:toc-mark-end text:id="tm1"/>'
    '<text:toc-mark text:string-value="tocentry"/>'
    '<text:user-index-mark-start text:id="ui1"/>uim'
    '<text:user-index-mark-end text:id="ui1"/>'
    '<text:user-index-mark text:string-value="uim2"/>'

    '<text:list text:style-name="List1" '
    'text:continue-numbering="true" '
    'text:continue-list="L0"><text:list-header>'
    '<text:p>list hdr</text:p></text:list-header>'
    '<text:list-item><text:p>item one</text:p>'
    '<text:list text:style-name="List1">'
    '<text:list-item><text:p>nested</text:p></text:list-item>'
    '</text:list></text:list-item>'
    '<text:list-item><text:p>item two</text:p></text:list-item>'
    '<text:list-item text:start-value="9"><text:p>restarted'
    '</text:p></text:list-item></text:list>'

    '<text:section text:name="sec1" text:style-name="S1" '
    'text:protected="true" text:protection-key="aGk=" '
    'text:display="always" text:condition="ooow:1">'
    '<text:section-source xlink:href="x" '
    'text:section-name="s" xlink:type="simple" '
    'xlink:show="embed"/>'
    '<text:p>sectioned</text:p></text:section>'

    '<table:table table:name="tbl1" table:style-name="Tbl" '
    'table:protected="false" table:print-ranges="A1:B2">'
    '<table:table-column table:style-name="Tcol" '
    'table:number-columns-repeated="2"/>'
    '<table:table-header-rows><table:table-row '
    'table:style-name="Tr">'
    '<table:table-cell table:style-name="Tc" '
    'table:number-columns-spanned="2" '
    'office:value-type="string">'
    '<text:p>header</text:p></table:table-cell>'
    '<table:covered-table-cell/>'
    '</table:table-row></table:table-header-rows>'
    '<table:table-row>'
    '<table:table-cell office:value-type="float" office:value="42" '
    'table:formula="of:=1+2" office:currency="USD" '
    'table:number-columns-spanned="1" '
    'table:number-rows-spanned="2" '
    'office:string-value="x">'
    '<text:p>cell</text:p></table:table-cell>'
    '<table:covered-table-cell table:number-columns-repeated="1"/>'
    '<table:table-cell office:value-type="boolean" '
    'office:boolean-value="true"/>'
    '<table:table-cell office:value-type="date" '
    'office:date-value="2024-01-02"/>'
    '<table:table-cell office:value-type="time" '
    'office:time-value="PT01H02M03S"/>'
    '<table:table-cell office:value-type="percentage" '
    'office:value="0.5"/>'
    '<table:table-cell office:value-type="currency" '
    'office:currency="EUR" office:value="9.99"/>'
    '<table:table-cell office:value-type="void" '
    'table:formula="of:=X"/>'
    '</table:table-row>'
    '<table:table-row>'
    '<table:covered-table-cell/>'
    '<table:table-cell><text:p>last</text:p></table:table-cell>'
    '</table:table-row></table:table>'

    '<text:p><draw:frame draw:style-name="Frame1" '
    'text:anchor-type="paragraph" svg:x="2in" svg:y="0.3in" '
    'svg:width="1.5in" svg:height="0.8in" draw:z-index="0" '
    'draw:name="fr1" draw:text-style-name="P2">'
    '<draw:text-box><text:p>boxed</text:p></draw:text-box>'
    '</draw:frame></text:p>'
    '<text:p><draw:frame text:anchor-type="char" svg:width="0.6in" '
    'svg:height="0.4in" draw:name="fr2">'
    '<draw:image xlink:href="Pictures/p.png" xlink:type="simple" '
    'xlink:show="embed" xlink:actuate="onLoad"/>'
    '<draw:object xlink:href="./obj1" xlink:type="simple"/>'
    '<svg:title>img title</svg:title>'
    '<svg:desc>img desc</svg:desc></draw:frame></text:p>'
    '<text:p><draw:frame text:anchor-type="as-char" '
    'svg:width="0.5in" svg:height="0.3in" draw:name="fr3">'
    '<draw:contour-polygon svg:width="1" svg:height="1" '
    'svg:viewBox="0 0 1 1" draw:recreate-on-edit="false"/>'
    '<draw:image xlink:href="Pictures/p2.png"/></draw:frame>'
    '</text:p>'
    '<text:p><draw:rect svg:width="0.5in" svg:height="0.3in"/>'
    '<draw:ellipse svg:width="0.4in" svg:height="0.2in"/>'
    '<draw:line svg:x1="0" svg:y1="0" svg:x2="1in" svg:y2="1in"/>'
    '</text:p>'

    '<text:p><text:change text:change-id="c0"/>'
    'mid'
    '<text:change-start text:change-id="c1"/>chg'
    '<text:change-end text:change-id="c1"/> done'
    '<text:deletion office:change-id="c2"/>'
    '</text:p>'
    + '</office:text></office:body></office:document-content>')

styles_odt_xml = (XMLDECL +
    '<office:document-styles '
    'xmlns:office="%soffice/1.0" xmlns:text="%stext/1.0" '
    'xmlns:style="%sstyle/1.0" xmlns:fo="%sxsl-fo-compatible/1.0" '
    'xmlns:svg="%ssvg-compatible/1.0" '
    'xmlns:table="%stable/1.0" xmlns:draw="%sdrawing/1.0" '
    'xmlns:number="%sdatastyle/1.0" '
    'xmlns:layout-grid="%slayout-grid-features/1.0" '
    'office:version="1.2">'
    % (O, O, O, O, O, O, O, O, O)
    + '<office:styles>'
    '<style:default-style style:family="paragraph">'
    '<style:paragraph-properties fo:hyphenate="false"/>'
    '<style:text-properties fo:font-size="12pt"/>'
    '<style:table-properties/><style:table-row-properties/>'
    '<style:table-column-properties/><style:table-cell-properties/>'
    '<style:graphic-properties/><style:drawing-page-properties/>'
    '</style:default-style>'
    '<style:style style:name="Standard" style:family="paragraph" '
    'style:class="text"><style:paragraph-properties/>'
    '<style:text-properties/></style:style>'
    '<style:style style:name="Heading_20_1" '
    'style:family="paragraph" style:parent-style-name="Standard" '
    'style:next-style-name="Standard" '
    'style:default-outline-level="1">'
    '<style:text-properties fo:font-weight="bold"/></style:style>'
    '<style:style style:name="Heading_20_2" '
    'style:family="paragraph" style:parent-style-name="Heading_20_1">'
    '</style:style>'
    '<style:style style:name="Heading_20_3" '
    'style:family="paragraph" style:parent-style-name="Heading_20_2">'
    '</style:style>'
    '<style:style style:name="Text_20_body" '
    'style:family="paragraph"><style:paragraph-properties '
    'fo:margin-top="0.1in"/></style:style>'
    '<style:style style:name="Footnote" style:family="paragraph">'
    '</style:style>'
    '<style:style style:name="Endnote" style:family="paragraph">'
    '</style:style>'
    '<style:style style:name="S1" style:family="section">'
    '<style:section-properties text:dont-balance-text-columns="true" '
    'style:editable="false"><style:columns fo:column-count="2" '
    'fo:column-gap="0.2in"><style:column-sep '
    'style:width="0.01in" style:color="#000000" '
    'style:height="100%" style:vertical-align="middle"/>'
    '<style:column style:rel-width="1"/><style:column '
    'style:rel-width="2"/></style:columns>'
    '</style:section-properties></style:style>'
    '<style:style style:name="T1" style:family="text">'
    '<style:text-properties/></style:style>'
    '</office:styles>'
    '<office:automatic-styles>'
    '<style:page-layout style:name="pm1">'
    '<style:page-layout-properties fo:page-width="8.5in" '
    'fo:page-height="11in" style:print-orientation="portrait" '
    'fo:margin-top="1in" fo:margin-bottom="1in" '
    'fo:margin-left="1.25in" fo:margin-right="1.25in" '
    'style:num-format="1" style:num-letter-sync="false" '
    'style:paper-tray-name="tray" fo:background-color="#FFFFFF" '
    'style:register-true="false" style:writing-mode="lr-tb" '
    'style:footnote-max-height="3in" '
    'layout-grid:display="false">'
    '<style:footnote-sep style:width="0.01in" '
    'style:distance-before-sep="0.04in" '
    'style:distance-after-sep="0.04in" '
    'style:adjustment="left" style:rel-width="25%" '
    'style:color="#000000"/>'
    '<style:background-image/></style:page-layout-properties>'
    '<style:header-style><style:header-footer-properties '
    'fo:min-height="0.5in" fo:margin-bottom="0.1in" '
    'svg:height="0.5in" style:dynamic-spacing="true"/>'
    '</style:header-style>'
    '<style:footer-style><style:header-footer-properties '
    'fo:min-height="0.4in"/></style:footer-style>'
    '</style:page-layout>'
    '</office:automatic-styles>'
    '<office:master-styles>'
    '<style:handout-master/>'
    '<draw:layer-set><draw:layer draw:name="layout"/>'
    '<draw:layer draw:name="background" draw:display="false"/>'
    '<draw:layer draw:name="measurelines"/>'
    '<draw:layer draw:name="controls"/></draw:layer-set>'
    '<style:master-page style:name="Standard" '
    'style:page-layout-name="pm1" '
    'style:next-style-name="Standard">'
    '<style:header><text:p>hdr <text:page-number/></text:p>'
    '</style:header>'
    '<style:footer><text:p>ftr</text:p></style:footer>'
    '<style:header-left style:display="false"><text:p>hl</text:p>'
    '</style:header-left>'
    '<style:header-right style:display="false"><text:p>hr</text:p>'
    '</style:header-right>'
    '<style:footer-left style:display="false"><text:p>fl</text:p>'
    '</style:footer-left>'
    '<style:first-page-number>3</style:first-page-number>'
    '</style:master-page>'
    '<style:master-page style:name="First_20_Page" '
    'style:page-layout-name="pm1" '
    'style:next-style-name="Standard">'
    '<style:header style:display="false"/><style:footer/>'
    '</style:master-page>'
    '<style:master-page style:name="Left_20_Page" '
    'style:page-layout-name="pm1" style:next-style-name="Standard">'
    '</style:master-page>'
    '<style:master-page style:name="Right_20_Page" '
    'style:page-layout-name="pm1" style:next-style-name="Standard">'
    '</style:master-page>'
    '</office:master-styles>'
    '</office:document-styles>')

meta_xml = (XMLDECL +
    '<office:document-meta xmlns:office="%soffice/1.0" '
    'xmlns:dc="http://purl.org/dc/elements/1.1/" '
    'xmlns:xlink="http://www.w3.org/1999/xlink" '
    'xmlns:meta="%smeta/1.0" office:version="1.2">' % (O, O)
    + '<office:meta><meta:generator>mkcov11</meta:generator>'
    '<dc:title>cov11 title</dc:title>'
    '<dc:description>desc</dc:description>'
    '<dc:subject>subj</dc:subject>'
    '<dc:creator>Ann</dc:creator>'
    '<dc:date>2024-01-02T03:04:05</dc:date>'
    '<dc:language>en-US</dc:language>'
    '<meta:keyword>k1</meta:keyword><meta:keyword>k2</meta:keyword>'
    '<meta:initial-creator>Ann</meta:initial-creator>'
    '<meta:creation-date>2024-01-01T00:00:00</meta:creation-date>'
    '<meta:editing-cycles>3</meta:editing-cycles>'
    '<meta:editing-duration>PT1H</meta:editing-duration>'
    '<meta:document-statistic meta:table-count="1" '
    'meta:image-count="1" meta:object-count="0" '
    'meta:page-count="2" meta:paragraph-count="20" '
    'meta:word-count="100" meta:character-count="500"/>'
    '<meta:user-defined meta:name="ud1" '
    'meta:value-type="float" meta:value="4.2">x</meta:user-defined>'
    '<meta:template xlink:href="tpl.ott" xlink:title="tpl"/>'
    '<meta:auto-reload xlink:href="next.odt" '
    'meta:delay="PT5S"/>'
    '<meta:hyperlink-behaviour/>'
    '</office:meta></office:document-meta>')

manifest_xml = (XMLDECL +
    '<manifest:manifest '
    'xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:'
    '1.0" manifest:version="1.2">'
    '<manifest:file-entry manifest:full-path="/" '
    'manifest:media-type="application/vnd.oasis.opendocument.text"/>'
    '<manifest:file-entry manifest:full-path="content.xml" '
    'manifest:media-type="text/xml"/>'
    '<manifest:file-entry manifest:full-path="styles.xml" '
    'manifest:media-type="text/xml"/>'
    '<manifest:file-entry manifest:full-path="meta.xml" '
    'manifest:media-type="text/xml"/>'
    '<manifest:file-entry manifest:full-path="Pictures/p.png" '
    'manifest:media-type="image/png"/>'
    '<manifest:file-entry manifest:full-path="Pictures/p2.png" '
    'manifest:media-type="image/png"/>'
    '</manifest:manifest>')

odt_path = os.path.join(OUT, 'cov11.odt')
with zipfile.ZipFile(odt_path, 'w') as z:
    z.writestr(zipfile.ZipInfo('mimetype'),
               'application/vnd.oasis.opendocument.text',
               compress_type=zipfile.ZIP_STORED)
    z.writestr('content.xml', content_xml,
               compress_type=zipfile.ZIP_DEFLATED)
    z.writestr('styles.xml', styles_odt_xml,
               compress_type=zipfile.ZIP_DEFLATED)
    z.writestr('meta.xml', meta_xml,
               compress_type=zipfile.ZIP_DEFLATED)
    z.writestr('META-INF/manifest.xml', manifest_xml,
               compress_type=zipfile.ZIP_DEFLATED)
    z.writestr('Pictures/p.png', PNG_RED,
               compress_type=zipfile.ZIP_DEFLATED)
    z.writestr('Pictures/p2.png', PNG_RED,
               compress_type=zipfile.ZIP_DEFLATED)
print('wrote', odt_path)
