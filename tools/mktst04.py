#!/usr/bin/env python3
"""TST04 importer regression fixtures.

Generates the minimal documents that pin the importer behaviors landed
in OXML02/OXML04/OXML06-08, ODF01, WP02 and MTH01-02 into an output
directory (default ./test/wp/tst04 relative to the CWD — pass the
target dir as argv[1]).

  o02_altchunk.docx   -- w:altChunk -> chunk1.html (OXML02)
  o04_fallback.docx   -- mc:AlternateContent chart w/ VML fallback,
                         bare c:chart and bare dgm diagram (OXML04)
  o06_revisions.docx  -- w:ins/w:del/w:moveFrom/w:moveTo + deleted
                         paragraph mark (OXML03/OXML06)
  o07_struxmarks.docx -- cellIns/cellDel + pPrChange/rPrChange/
                         sectPrChange (OXML07)
  o01_objects.odt     -- draw:object MathML + chart-with-preview (ODF01)
  wpd02_hdrftr.wpd    -- WP6 header/footer groups + footnote-in-header
                         (WP02; wpd04.wpd and wpd_img_*.wpd come from
                         mkwpd04.py/mkwpdimg.py)
  math.md / math.tex  -- $...$/$$...$$ and equation-env math (MTH01/02)

The .wpd builder follows the WP6.1 container layout used by
mkwpd04.py/mkwpdimg.py: function groups in the document stream whose
prefix ids reference index packets (GeneralTextPacket type 0x08 holds
the subdocument streams).
"""
import struct
import zlib
import zipfile
import os
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else 'test/wp/tst04'
os.makedirs(OUT, exist_ok=True)


def mkpng(w, h, rgb):
    def chunk(t, d):
        return (struct.pack('>I', len(d)) + t + d
                + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff))
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)
    raw = b''.join(b'\x00' + rgb * w for _ in range(h))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr)
            + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


PNG_BLUE = mkpng(48, 32, b'\x20\x20\xcc')
PNG_GREEN = mkpng(48, 32, b'\x20\xcc\x20')
PNG_RED = mkpng(48, 32, b'\xcc\x20\x20')


def write(name, data):
    mode = 'wb' if isinstance(data, (bytes, bytearray)) else 'w'
    with open(os.path.join(OUT, name), mode) as f:
        f.write(data)
    print('wrote', os.path.join(OUT, name))


# ==== docx scaffolding ====================================================

W_NS = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
R_NS = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'

DOCX_HEAD = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
    '<w:document xmlns:w="%s" xmlns:r="%s" '
    'xmlns:wp="http://schemas.openxmlformats.org/drawingml/2006/'
    'wordprocessingDrawing" '
    'xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" '
    'xmlns:pic="http://schemas.openxmlformats.org/drawingml/2006/'
    'picture" '
    'xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/'
    '2006" '
    'xmlns:c="http://schemas.openxmlformats.org/drawingml/2006/chart" '
    'xmlns:dgm="http://schemas.openxmlformats.org/drawingml/2006/'
    'diagram" '
    'xmlns:v="urn:schemas-microsoft-com:vml" '
    'xmlns:o="urn:schemas-microsoft-com:office:office">\n'
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

CONTENT_TYPES = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
    '<Types xmlns="http://schemas.openxmlformats.org/package/2006/'
    'content-types">'
    '<Default Extension="rels" ContentType="application/vnd.openxmlformats-'
    'package.relationships+xml"/>'
    '<Default Extension="xml" ContentType="application/xml"/>'
    '<Default Extension="png" ContentType="image/png"/>'
    '<Default Extension="html" ContentType="text/html"/>'
    '<Default Extension="rtf" ContentType="text/rtf"/>'
    '<Override PartName="/word/document.xml" ContentType="application/vnd.'
    'openxmlformats-officedocument.wordprocessingml.document.main+xml"/>'
    '<Override PartName="/word/settings.xml" ContentType="application/vnd.'
    'openxmlformats-officedocument.wordprocessingml.settings+xml"/>'
    '</Types>\n')


def make_docx(path, document_body, doc_rels, extra_parts):
    rels = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
            '<Relationships xmlns="http://schemas.openxmlformats.org/'
            'package/2006/relationships">')
    for rid, rtype, target in doc_rels:
        rels += ('<Relationship Id="%s" Type="%s" Target="%s"/>'
                 % (rid, rtype, target))
    rels += '</Relationships>\n'
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml', CONTENT_TYPES)
        z.writestr('_rels/.rels', ROOT_RELS)
        z.writestr('word/document.xml', DOCX_HEAD + document_body + DOCX_TAIL)
        z.writestr('word/_rels/document.xml.rels', rels)
        for name, data in extra_parts:
            z.writestr(name, data)
    print('wrote', path)


REL = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships/'

SETTINGS_TRACK = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
    '<w:settings xmlns:w="http://schemas.openxmlformats.org/'
    'wordprocessingml/2006/main"><w:trackChanges/></w:settings>\n')


# ==== o02_altchunk.docx ===================================================

make_docx(
    os.path.join(OUT, 'o02_altchunk.docx'),
    '<w:p><w:r><w:t>Before chunk.</w:t></w:r></w:p>\n'
    '<w:altChunk r:id="rIdChunk1"/>\n'
    '<w:p><w:r><w:t>After chunk.</w:t></w:r></w:p>\n'
    '<w:sectPr/>\n',
    [('rIdChunk1', REL + 'aFChunk', 'chunk1.html')],
    [('word/chunk1.html',
      '<html><body><p>Chunk grafted <b>BOLDCHUNK</b> here.</p>'
      '</body></html>')])


# ==== o04_fallback.docx ===================================================

chart_space = (
    '<?xml version="1.0" encoding="UTF-8"?>\n'
    '<c:chartSpace xmlns:c="http://schemas.openxmlformats.org/drawingml/'
    '2006/chart" xmlns:r="%s"><c:chart><c:plotArea/></c:chart>'
    '</c:chartSpace>\n' % R_NS)
dgm_data = (
    '<?xml version="1.0" encoding="UTF-8"?>\n'
    '<dgm:dataModel xmlns:dgm="http://schemas.openxmlformats.org/'
    'drawingml/2006/diagram"/>\n')

ac_chart_drawing = (
    '<w:p><w:r><w:drawing><mc:AlternateContent>'
    '<mc:Choice Requires="c">'
    '<wp:inline distT="0" distB="0" distL="0" distR="0">'
    '<wp:extent cx="1524000" cy="914400"/>'
    '<a:graphic><a:graphicData uri="http://schemas.openxmlformats.org/'
    'drawingml/2006/chart">'
    '<c:chart r:id="rIdChart1"/></a:graphicData></a:graphic>'
    '</wp:inline></mc:Choice>'
    '<mc:Fallback>'
    '<w:pict><v:shape id="chart_fb" type="#_x0000_t75" '
    'style="width:120pt;height:72pt">'
    '<v:imagedata r:id="rIdImg1" o:title="chart1"/>'
    '</v:shape></w:pict>'
    '</mc:Fallback>'
    '</mc:AlternateContent></w:drawing></w:r></w:p>\n')

bare_chart_drawing = (
    '<w:p><w:r><w:drawing>'
    '<wp:inline distT="0" distB="0" distL="0" distR="0">'
    '<wp:extent cx="1524000" cy="914400"/>'
    '<a:graphic><a:graphicData uri="http://schemas.openxmlformats.org/'
    'drawingml/2006/chart">'
    '<c:chart r:id="rIdChart2"/></a:graphicData></a:graphic>'
    '</wp:inline></w:drawing></w:r></w:p>\n')

bare_dgm_drawing = (
    '<w:p><w:r><w:drawing>'
    '<wp:inline distT="0" distB="0" distL="0" distR="0">'
    '<wp:extent cx="1524000" cy="914400"/>'
    '<a:graphic><a:graphicData uri="http://schemas.openxmlformats.org/'
    'drawingml/2006/diagram">'
    '<dgm:relIds r:dm="rIdDgm1" r:lo="rIdDgmLo" r:qs="rIdDgmQs" '
    'r:cs="rIdDgmCs"/></a:graphicData></a:graphic>'
    '</wp:inline></w:drawing></w:r></w:p>\n')

make_docx(
    os.path.join(OUT, 'o04_fallback.docx'),
    ac_chart_drawing + bare_chart_drawing + bare_dgm_drawing
    + '<w:p><w:r><w:t>tail</w:t></w:r></w:p>\n<w:sectPr/>\n',
    [('rIdChart1', REL + 'chart', 'charts/chart1.xml'),
     ('rIdImg1', REL + 'image', 'media/image1.png'),
     ('rIdChart2', REL + 'chart', 'charts/chart2.xml'),
     ('rIdDgm1', REL + 'diagramData', 'diagrams/data1.xml'),
     ('rIdDgmLo', REL + 'diagramLayout', 'diagrams/layout1.xml'),
     ('rIdDgmQs', REL + 'diagramQuickStyle', 'diagrams/qs1.xml'),
     ('rIdDgmCs', REL + 'diagramColors', 'diagrams/cs1.xml')],
    [('word/media/image1.png', PNG_BLUE),
     ('word/charts/chart1.xml', chart_space),
     ('word/charts/chart2.xml', chart_space),
     ('word/diagrams/data1.xml', dgm_data),
     ('word/diagrams/layout1.xml', '<?xml version="1.0"?>\n<dgm:layoutDef '
      'xmlns:dgm="http://schemas.openxmlformats.org/drawingml/2006/'
      'diagram"/>\n'),
     ('word/diagrams/qs1.xml', '<?xml version="1.0"?>\n<dgm:styleDef '
      'xmlns:dgm="http://schemas.openxmlformats.org/drawingml/2006/'
      'diagram"/>\n'),
     ('word/diagrams/cs1.xml', '<?xml version="1.0"?>\n<dgm:colorsDef '
      'xmlns:dgm="http://schemas.openxmlformats.org/drawingml/2006/'
      'diagram"/>\n')])


# ==== o06_revisions.docx ==================================================

def ins(rid, author, date, text):
    return ('<w:ins w:id="%d" w:author="%s" w:date="%s"><w:r><w:t xml:space='
            '"preserve">%s</w:t></w:r></w:ins>' % (rid, author, date, text))


def dele(rid, author, date, text):
    return ('<w:del w:id="%d" w:author="%s" w:date="%s"><w:r><w:delText '
            'xml:space="preserve">%s</w:delText></w:r></w:del>'
            % (rid, author, date, text))


rev_body = '<w:p>'
rev_body += ins(1, 'Alice', '2026-01-02T10:00:00Z', 'INSA')
rev_body += ins(2, 'Alice', '2026-01-02T10:00:00Z', 'INSB')
rev_body += '<w:r><w:t xml:space="preserve"> mid </w:t></w:r>'
rev_body += dele(3, 'Bob', '2026-01-03T11:00:00Z', 'DELONE')
rev_body += ins(4, 'Carol', '2026-01-04T12:00:00Z', 'INSC')
rev_body += '</w:p>\n'
rev_body += ('<w:p><w:moveFrom w:id="5" w:name="mv1" w:author="Dan" '
             'w:date="2026-01-05T13:00:00Z"><w:r><w:delText>MOVEDTEXT'
             '</w:delText></w:r></w:moveFrom></w:p>\n')
rev_body += ('<w:p><w:moveTo w:id="6" w:name="mv1" w:author="Dan" '
             'w:date="2026-01-05T13:00:00Z"><w:r><w:t>MOVEDTEXT</w:t>'
             '</w:r></w:moveTo></w:p>\n')
rev_body += ('<w:p><w:pPr><w:rPr><w:del w:id="7" w:author="Eve" '
             'w:date="2026-01-06T14:00:00Z"/></w:rPr></w:pPr>'
             '<w:r><w:t>aftermark</w:t></w:r></w:p>\n')
rev_body += '<w:p><w:r><w:t>tail</w:t></w:r></w:p>\n<w:sectPr/>\n'

make_docx(os.path.join(OUT, 'o06_revisions.docx'), rev_body,
          [('rIdSettings', REL + 'settings', 'settings.xml')],
          [('word/settings.xml', SETTINGS_TRACK)])


# ==== o07_struxmarks.docx =================================================

strux_body = (
    '<w:p><w:pPr><w:jc w:val="center"/>'
    '<w:pPrChange w:id="20" w:author="Alice" w:date="2026-02-01T09:00:00Z">'
    '<w:pPr><w:jc w:val="left"/></w:pPr></w:pPrChange></w:pPr>'
    '<w:r><w:rPr><w:b/>'
    '<w:rPrChange w:id="21" w:author="Alice" w:date="2026-02-01T09:00:00Z">'
    '<w:rPr/></w:rPrChange></w:rPr><w:t>changed para</w:t></w:r></w:p>\n'
    '<w:tbl><w:tblPr><w:tblW w:w="6000" w:type="dxa"/></w:tblPr>'
    '<w:tblGrid><w:gridCol w:w="3000"/><w:gridCol w:w="3000"/></w:tblGrid>'
    '<w:tr><w:tc>'
    '<w:tcPr><w:tcW w:w="3000" w:type="dxa"/>'
    '<w:cellIns w:id="22" w:author="Bob" w:date="2026-02-02T10:00:00Z"/>'
    '</w:tcPr><w:p><w:r><w:t>inserted cell</w:t></w:r></w:p></w:tc>'
    '<w:tc>'
    '<w:tcPr><w:tcW w:w="3000" w:type="dxa"/>'
    '<w:cellDel w:id="23" w:author="Bob" w:date="2026-02-02T10:00:00Z"/>'
    '</w:tcPr><w:p><w:r><w:t>deleted cell</w:t></w:r></w:p></w:tc>'
    '</w:tr></w:tbl>\n'
    '<w:p><w:r><w:t>tail</w:t></w:r></w:p>\n'
    '<w:sectPr><w:pgSz w:w="11906" w:h="16838"/>'
    '<w:sectPrChange w:id="24" w:author="Carol" w:date="2026-02-03T'
    '11:00:00Z"><w:sectPr><w:pgSz w:w="12240" w:h="15840"/></w:sectPr>'
    '</w:sectPrChange></w:sectPr>\n')

make_docx(os.path.join(OUT, 'o07_struxmarks.docx'), strux_body, [], [])


# ==== o01_objects.odt =====================================================

MATHML = (
    '<?xml version="1.0" encoding="UTF-8"?>\n'
    '<math:math xmlns:math="http://www.w3.org/1998/Math/MathML">'
    '<math:mrow><math:mi>x</math:mi><math:mo>+</math:mo><math:mn>1'
    '</math:mn></math:mrow></math:math>\n')
CHART_DOC = (
    '<?xml version="1.0" encoding="UTF-8"?>\n'
    '<office:document xmlns:office="urn:oasis:names:tc:opendocument:'
    'xmlns:office:1.0" office:version="1.2"/>\n')
ODT_CONTENT = (
    '<?xml version="1.0" encoding="UTF-8"?>\n'
    '<office:document-content '
    'xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" '
    'xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" '
    'xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" '
    'xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0" '
    'xmlns:xlink="http://www.w3.org/1999/xlink" '
    'xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" '
    'office:version="1.2">\n'
    '<office:automatic-styles>'
    '<style:style style:name="fr1" style:family="graphic">'
    '<style:graphic-properties draw:fill="none" draw:stroke="none"/>'
    '</style:style>'
    '</office:automatic-styles>\n'
    '<office:body><office:text>\n'
    '<text:p text:style-name="Standard">Object: '
    '<draw:frame draw:style-name="fr1" text:anchor-type="as-char" '
    'svg:width="1.2in" svg:height="0.5in">'
    '<draw:object xlink:href="./Object1/"/>'
    '<draw:image xlink:href="ObjectReplacements/preview1.png"/>'
    '</draw:frame></text:p>\n'
    '<text:p text:style-name="Standard">Chart: '
    '<draw:frame draw:style-name="fr1" text:anchor-type="as-char" '
    'svg:width="1.2in" svg:height="0.5in">'
    '<draw:object xlink:href="./Object2/"/>'
    '<draw:image xlink:href="ObjectReplacements/preview2.png"/>'
    '</draw:frame></text:p>\n'
    '</office:text></office:body></office:document-content>\n')
ODT_MANIFEST = (
    '<?xml version="1.0" encoding="UTF-8"?>\n'
    '<manifest:manifest xmlns:manifest="urn:oasis:names:tc:opendocument:'
    'xmlns:manifest:1.0" manifest:version="1.2">'
    '<manifest:file-entry manifest:full-path="/" '
    'manifest:media-type="application/vnd.oasis.opendocument.text"/>'
    '<manifest:file-entry manifest:full-path="content.xml" '
    'manifest:media-type="text/xml"/>'
    '<manifest:file-entry manifest:full-path="Object1/" '
    'manifest:media-type="application/vnd.oasis.opendocument.formula"/>'
    '<manifest:file-entry manifest:full-path="Object2/" '
    'manifest:media-type="application/vnd.oasis.opendocument.chart"/>'
    '<manifest:file-entry '
    'manifest:full-path="ObjectReplacements/preview1.png" '
    'manifest:media-type="image/png"/>'
    '<manifest:file-entry '
    'manifest:full-path="ObjectReplacements/preview2.png" '
    'manifest:media-type="image/png"/>'
    '</manifest:manifest>\n')

odt_path = os.path.join(OUT, 'o01_objects.odt')
with zipfile.ZipFile(odt_path, 'w') as z:
    z.writestr(zipfile.ZipInfo('mimetype'),
               'application/vnd.oasis.opendocument.text',
               compress_type=zipfile.ZIP_STORED)
    z.writestr('META-INF/manifest.xml', ODT_MANIFEST,
               compress_type=zipfile.ZIP_DEFLATED)
    z.writestr('content.xml', ODT_CONTENT,
               compress_type=zipfile.ZIP_DEFLATED)
    z.writestr('Object1/content.xml', MATHML,
               compress_type=zipfile.ZIP_DEFLATED)
    z.writestr('Object2/content.xml', CHART_DOC,
               compress_type=zipfile.ZIP_DEFLATED)
    z.writestr('ObjectReplacements/preview1.png', PNG_GREEN,
               compress_type=zipfile.ZIP_DEFLATED)
    z.writestr('ObjectReplacements/preview2.png', PNG_RED,
               compress_type=zipfile.ZIP_DEFLATED)
print('wrote', odt_path)


# ==== wpd02_hdrftr.wpd (WP6.1) ============================================

def wp6_group(gid, sub, nondele=b'', dele=b'', prefix_ids=()):
    """WP6 variable-length group, same wire layout as mkwpd04.py."""
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


def wp6_gen_text(stream):
    """GeneralTextPacket payload: one text block of a WP6 subdocument."""
    return (struct.pack('<H', 1) + b'\x00' * 4
            + struct.pack('<I', len(stream)) + stream)


# packet ids: 1 = header subdocument, 2 = footnote subdocument
#             (referenced from inside the header), 3 = footer subdocument
footnote_on = wp6_group(0xD7, 0x00, prefix_ids=[2])
footnote_off = wp6_group(0xD7, 0x01)
header_stream = (b'Header text.' + footnote_on + footnote_off + b'\xcc')

doc = bytearray()
doc += b'Body text.'
doc += wp6_group(0xD6, 0x00, nondele=b'\x03', prefix_ids=[1])   # header A all
doc += wp6_group(0xD6, 0x02, nondele=b'\x03', prefix_ids=[3])   # footer A all
doc += b'\xcc'

packets = [
    (0x00, 0x08, wp6_gen_text(header_stream)),                  # id 1
    (0x00, 0x08, wp6_gen_text(b'Note in header.\xcc')),         # id 2
    (0x00, 0x08, wp6_gen_text(b'Footer text.\xcc')),            # id 3
]

idx_hdr_off = 0x18
idx_hdr = struct.pack('<HH', 0, len(packets) + 1) + bytes(10)

data_off = idx_hdr_off + 14 + len(packets) * 14
indices = b''
payload = b''
for flags, typ, data in packets:
    indices += struct.pack('<BBHHII', flags, typ, 1, 0,
                           len(data), data_off + len(payload))
    payload += data
doc_off = data_off + len(payload)

hdr = bytearray(idx_hdr_off)
hdr[0] = 0xFF
hdr[1:4] = b'WPC'
struct.pack_into('<I', hdr, 4, doc_off)
hdr[8:12] = bytes([0x01, 0x0A, 0x02, 0x01])
struct.pack_into('<H', hdr, 12, 0)
struct.pack_into('<H', hdr, 14, idx_hdr_off)
total = doc_off + len(doc)
struct.pack_into('<H', hdr, 20, total)

blob = bytes(hdr) + idx_hdr + indices + payload + bytes(doc)
assert len(blob) == total
write('wpd02_hdrftr.wpd', blob)


# ==== math.md / math.tex ==================================================

write('math.md',
      '# Math test\n\n'
      'Inline $x^2 + y_i$ here.\n\n'
      'Display:\n\n'
      '$$e^{i\\pi} + 1 = 0$$\n')

write('math.tex',
      '\\documentclass{article}\n'
      '\\begin{document}\n'
      'Inline $a^2+b^2=c^2$ math.\n\n'
      '\\begin{equation}\n'
      '  \\frac{1}{2} + \\sqrt{x}\n'
      '\\end{equation}\n'
      '\\end{document}\n')
