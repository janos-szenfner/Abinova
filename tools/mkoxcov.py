#!/usr/bin/env python3
"""COVD03 coverage fixture.

Generates test/wp/oxcov/cov.docx — a synthetic package aimed at the
element-validation and settings clusters the corpus never reaches in
src/wp/impexp/openxml/imp/xp/OXMLi_ListenerState_Valid.cpp:

  settings.xml  -- uncovered w:settings children (activeWritingStyle,
                   drawingGridVertical*/trackRevisions/save* flags,
                   smartTagType), w:compat leftovers and the whole
                   w:webSettings subtree (encoding, frameset/frame/
                   framesetSplitbar, divs/div/divsChild, margins,
                   pixelsPerInch, relyOnVML, targetScreenSz, ...)
  document.xml  -- w:background, form-field w:ffData subtree
                   (checkBox/ddList/textInput + shared children),
                   w:fldData, w:delInstrText, w:break, w:pgNum,
                   w:ruby w:rubyPr w:dirty, VML w:pict w:control +
                   w:movie, w:sdt w:sdtPr content-type markers
                   (bibliography/citation/comboBox/docPartObj/
                   dropDownList/equation/group/picture/richText),
                   w:customXml + w:smartTag property blocks, strux
                   change-tracking elements (numberingChange,
                   tblPrChange, tblGridChange, trPrChange,
                   tblPrExChange, tcPrChange, cellMerge), w:oMath,
                   w:altChunk -> real chunk.html part
  styles.xml    -- w:personal/-Compose/-Reply style children
  numbering.xml -- w:lvl w:suff

The fixture only needs to be well-formed enough to drive the importer
branches; it is not schema-perfect Word output.  Regenerate with:

    python3 tools/mkoxcov.py           (writes test/wp/oxcov/cov.docx)
"""
import os
import sys
import zipfile

OUT = sys.argv[1] if len(sys.argv) > 1 else 'test/wp/oxcov'
os.makedirs(OUT, exist_ok=True)

XMLDECL = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
W_NS = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
R_NS = ('http://schemas.openxmlformats.org/officeDocument/2006/'
        'relationships')
MC_NS = 'http://schemas.openxmlformats.org/markup-compatibility/2006'
V_NS = 'urn:schemas-microsoft-com:vml'
REL = ('http://schemas.openxmlformats.org/officeDocument/2006/'
       'relationships/')
CT_W = ('application/vnd.openxmlformats-officedocument.'
        'wordprocessingml.')


def write(name, data):
    mode = 'wb' if isinstance(data, (bytes, bytearray)) else 'w'
    with open(os.path.join(OUT, name), mode) as f:
        f.write(data)
    print('wrote', os.path.join(OUT, name))


# ---- settings.xml ----------------------------------------------------

# bare OnOff children of w:settings the corpus never carries
settings_flags = [
    'activeWritingStyle w:language="en-US" w:vendorID="64" '
    'w:dllVersion="131078" w:nlCheck="1" w:optionSet="0"',
    'drawingGridVerticalOrigin w:val="1440"',
    'drawingGridVerticalSpacing w:val="720"',
    'forceUpgrade',
    'ignoreMixedContent',
    'readModeInkLockDown',
    'saveInvalidXml',
    'savePreviewPicture',
    'saveThroughXslt w:val="t.xsl"',
    'saveXmlDataOnly',
    'showXMLTags',
    'trackRevisions',
    'displayHangulFixedWidth',
]
settings_xml = (XMLDECL +
    '<w:settings xmlns:w="%s" xmlns:r="%s">' % (W_NS, R_NS)
    + ''.join('<w:%s/>' % f for f in settings_flags)
    + '<w:smartTagType w:namespaceUri="urn:smart" w:name="tag"/>'
    '<w:compat>'
    + ''.join('<w:%s/>' % f for f in
              ['alignTablesRowByRow', 'allowSpaceOfSameStyleInTable',
               'doNotUseHTMLParagraphAutoSpacing',
               'underlineTabInNumList'])
    + '</w:compat>'
    # the whole w:webSettings subtree (HTML-view save settings)
    '<w:webSettings>'
    '<w:encoding w:val="utf-8"/>'
    '<w:optimizeForBrowser/>'
    '<w:relyOnVML/>'
    '<w:allowPNG/>'
    '<w:pixelsPerInch w:val="96"/>'
    '<w:targetScreenSz w:val="800x600"/>'
    '<w:saveSmartTagsAsXml/>'
    '<w:frameset><w:frameLayout w:val="rows"/>'
    '<w:frame><w:sourceFileName r:id="rIdFrSrc"/>'
    '<w:linkedToFile/><w:marW w:val="720"/><w:marH w:val="360"/>'
    '<w:scrollbar w:val="auto"/></w:frame>'
    '<w:framesetSplitbar><w:w w:val="2400"/>'
    '<w:color w:val="808080"/><w:noBorder/><w:flatBorders/>'
    '</w:framesetSplitbar></w:frameset>'
    '<w:divs><w:div w:id="1">'
    '<w:blockQuote/><w:bodyDiv/><w:divBdr w:val="single" '
    'w:sz="6" w:color="FF0000"/><w:marLeft w:val="720"/>'
    '<w:marRight w:val="720"/><w:marTop w:val="360"/>'
    '<w:marBottom w:val="360"/>'
    '<w:divsChild><w:div w:id="2"><w:marLeft w:val="1440"/></w:div>'
    '</w:divsChild>'
    '</w:div></w:divs>'
    '</w:webSettings>'
    '</w:settings>\n')


# ---- styles.xml ------------------------------------------------------

styles_xml = (XMLDECL +
    '<w:styles xmlns:w="%s">' % W_NS +
    '<w:style w:type="paragraph" w:styleId="Normal" w:default="1">'
    '<w:name w:val="Normal"/></w:style>'
    # glossary/docPart style flags as style children
    '<w:style w:type="paragraph" w:styleId="Note">'
    '<w:name w:val="Note"/><w:personal/><w:personalCompose/>'
    '<w:personalReply/></w:style>'
    '</w:styles>\n')


# ---- numbering.xml ----------------------------------------------------

numbering_xml = (XMLDECL +
    '<w:numbering xmlns:w="%s">' % W_NS +
    '<w:abstractNum w:abstractNumId="0"><w:lvl w:ilvl="0">'
    '<w:start w:val="1"/><w:numFmt w:val="decimal"/>'
    '<w:lvlText w:val="%1."/><w:suff w:val="space"/>'
    '</w:lvl></w:abstractNum>'
    '<w:num w:numId="1"><w:abstractNumId w:val="0"/></w:num>'
    '</w:numbering>\n')


# ---- document.xml ----------------------------------------------------

CH = 'w:id="%d" w:author="cov" w:date="2024-06-01T10:00:00Z"'

# form-field ffData variants (checkBox, ddList, textInput)
ffdata_block = (
    '<w:p>'
    '<w:r><w:fldChar w:fldCharType="begin"><w:ffData>'
    '<w:name w:val="chk1"/><w:enabled/><w:calcOnExit/>'
    '<w:statusText w:val="status"/><w:helpText w:val="help"/>'
    '<w:entryMacro w:val="mIn"/><w:exitMacro w:val="mOut"/>'
    '<w:checkBox><w:sizeAuto/><w:checked/><w:default/></w:checkBox>'
    '</w:ffData></w:fldChar></w:r>'
    '<w:r><w:fldChar w:fldCharType="separate">'
    '<w:fldData>AAAABBBBCCCC</w:fldData></w:fldChar></w:r>'
    '<w:r><w:fldChar w:fldCharType="end"/></w:r>'
    '</w:p>'
    '<w:p>'
    '<w:r><w:fldChar w:fldCharType="begin"><w:ffData>'
    '<w:ddList w:result="1"><w:listEntry w:val="optA"/>'
    '<w:listEntry w:val="optB"/><w:default w:val="0"/>'
    '<w:result w:val="1"/></w:ddList>'
    '<w:textInput><w:format w:val="UPPERCASE"/>'
    '<w:maxLength w:val="10"/><w:default w:val="txt"/>'
    '</w:textInput>'
    '</w:ffData></w:fldChar></w:r>'
    '<w:r><w:fldChar w:fldCharType="end"/></w:r>'
    '</w:p>')

# run-level strays: w:delInstrText, w:break, w:pgNum, w:ruby>rubyPr>dirty
run_misc = (
    '<w:p>'
    '<w:r><w:delInstrText xml:space="preserve"> PAGE </w:delInstrText>'
    '</w:r>'
    '<w:r><w:break/><w:pgNum/><w:t>after-break</w:t></w:r>'
    '<w:ruby><w:rubyPr><w:dirty w:val="on"/></w:rubyPr>'
    '<w:rt><w:r><w:t>rt</w:t></w:r></w:rt>'
    '<w:rubyBase><w:r><w:t>base</w:t></w:r></w:rubyBase></w:ruby>'
    '</w:p>')

# VML picture carrying w:control + w:movie children
pict_block = (
    '<w:p><w:r><w:pict>'
    '<v:shape id="s1" style="width:10pt;height:10pt">'
    '<w:control r:id="rIdOle1" w:name="ctl" w:shapeid="s1"/>'
    '<w:movie r:id="rIdOle1"/>'
    '</v:shape></w:pict></w:r></w:p>'
    '<w:p><w:r><w:object><v:shape id="s2" '
    'style="width:10pt;height:10pt">'
    '<w:control r:id="rIdOle1" w:name="ctl2" w:shapeid="s2"/>'
    '</v:shape></w:object></w:r></w:p>')

# sdt content-type markers — one sdt per sdtPr type element
def sdt(type_elem):
    tag = type_elem[3:type_elem.find('>')].split()[0].rstrip('/')
    return ('<w:sdt><w:sdtPr><w:alias w:val="t"/><w:tag w:val="tg"/>'
            '<w:id w:val="42"/><w:lock w:val="sdtLocked"/>'
            '<w:temporary/>' + type_elem + '</w:sdtPr>'
            '<w:sdtContent><w:p><w:r><w:t>sdt-' + tag +
            '</w:t></w:r></w:p></w:sdtContent></w:sdt>')

sdt_block = ''.join(sdt(t) for t in [
    '<w:bibliography/>', '<w:citation/>', '<w:comboBox/>',
    '<w:docPartObj><w:docPartGallery w:val="g"/>'
    '<w:docPartCategory><w:name w:val="c"/>'
    '<w:gallery w:val="g2"/></w:docPartCategory>'
    '<w:docPartUnique/></w:docPartObj>',
    '<w:docPartList><w:docPartGallery w:val="g"/>'
    '<w:docPartCategory><w:name w:val="c"/></w:docPartCategory>'
    '</w:docPartList>',
    '<w:dropDownList/>', '<w:equation/>', '<w:group/>',
    '<w:picture/>', '<w:richText/>', '<w:text/>'])

# customXml / smartTag property blocks
xmltag_block = (
    '<w:customXml w:element="e" w:uri="urn:cx" w:itemID="{DEADBEEF}">'
    '<w:customXmlPr><w:placeholder w:val="ph"/>'
    '<w:attr w:uri="urn:a" w:name="n" w:val="v"/>'
    '</w:customXmlPr>'
    '<w:p><w:r><w:t>customxml</w:t></w:r></w:p></w:customXml>'
    '<w:p><w:smartTag w:uri="urn:st" w:element="e">'
    '<w:smartTagPr><w:attr w:name="n" w:val="v"/></w:smartTagPr>'
    '<w:r><w:t>smarttag</w:t></w:r></w:smartTag></w:p>')

# table exercising every *Change revision element + trPr grid extras
table_block = (
    '<w:tbl>'
    '<w:tblPr><w:tblW w:w="4320" w:type="dxa"/>'
    '<w:tblPrChange ' + CH % 30 + '><w:tblPr>'
    '<w:tblW w:w="0" w:type="auto"/></w:tblPr></w:tblPrChange>'
    '</w:tblPr>'
    '<w:tblGrid><w:gridCol w:w="2160"/><w:gridCol w:w="2160"/>'
    '<w:tblGridChange ' + CH % 31 + '><w:tblGrid>'
    '<w:gridCol w:w="1080"/></w:tblGrid></w:tblGridChange>'
    '</w:tblGrid>'
    '<w:tr><w:trPr>'
    '<w:gridBefore w:val="1"/><w:gridAfter w:val="1"/>'
    '<w:wBefore w:w="720" w:type="dxa"/>'
    '<w:wAfter w:w="720" w:type="dxa"/>'
    '<w:trPrChange ' + CH % 32 + '><w:trPr/></w:trPrChange>'
    '</w:trPr>'
    '<w:tblPrEx><w:tblPrExChange ' + CH % 33 + '>'
    '<w:tblPrEx/></w:tblPrExChange></w:tblPrEx>'
    '<w:tc><w:tcPr><w:tcW w:w="2160" w:type="dxa"/>'
    '<w:tcPrChange ' + CH % 34 + '><w:tcPr/></w:tcPrChange>'
    '</w:tcPr><w:p><w:r><w:t>cellA</w:t></w:r></w:p></w:tc>'
    '<w:tc><w:tcPr><w:tcW w:w="2160" w:type="dxa"/>'
    '<w:cellMerge ' + CH % 35 + ' w:vMerge="restart" w:val="cont"/>'
    '</w:tcPr><w:p><w:r><w:t>cellB</w:t></w:r></w:p></w:tc>'
    '</w:tr>'
    '<w:tr><w:tc><w:p><w:r><w:t>c1</w:t></w:r></w:p></w:tc>'
    '<w:tc><w:p><w:r><w:t>c2</w:t></w:r></w:p></w:tc></w:tr>'
    '</w:tbl>')

# paragraph w/ numPr > numberingChange + a w:oMath named element
misc2 = (
    '<w:p><w:pPr><w:numPr><w:ilvl w:val="0"/><w:numId w:val="1"/>'
    '<w:numberingChange ' + CH % 36 + ' w:original="1."/>'
    '</w:numPr></w:pPr><w:r><w:t>numbered</w:t></w:r></w:p>'
    '<w:p><w:oMath><w:r><w:t>x+1</w:t></w:r></w:oMath></w:p>'
    '<w:altChunk r:id="rIdChunk"><w:altChunkPr><w:matchSrc/>'
    '</w:altChunkPr></w:altChunk>')

document_body = (
    '<w:p><w:r><w:t>oxcov</w:t></w:r></w:p>'
    + ffdata_block + run_misc + pict_block + sdt_block
    + xmltag_block + table_block + misc2)

document_xml = (XMLDECL +
    '<w:document xmlns:w="%s" xmlns:r="%s" xmlns:v="%s" '
    'xmlns:mc="%s" mc:Ignorable="w14">'
    '<w:background w:color="CCEEFF" w:themeColor="background1"/>'
    '<w:body>' % (W_NS, R_NS, V_NS, MC_NS)
    + document_body +
    '<w:sectPr><w:pgSz w:w="12240" w:h="15840"/>'
    '<w:pgMar w:top="1440" w:right="1440" w:bottom="1440" '
    'w:left="1440"/></w:sectPr>'
    '</w:body></w:document>\n')

chunk_html = ('<html><body><p>chunked <b>alt</b> text</p>'
              '</body></html>\n')

content_types = (XMLDECL +
    '<Types xmlns="http://schemas.openxmlformats.org/package/'
    '2006/content-types">'
    '<Default Extension="rels" ContentType="application/vnd.'
    'openxmlformats-package.relationships+xml"/>'
    '<Default Extension="xml" ContentType="application/xml"/>'
    '<Default Extension="html" ContentType="text/html"/>'
    '<Override PartName="/word/document.xml" ContentType="'
    + CT_W + 'document.main+xml"/>'
    '<Override PartName="/word/styles.xml" ContentType="'
    + CT_W + 'styles+xml"/>'
    '<Override PartName="/word/settings.xml" ContentType="'
    + CT_W + 'settings+xml"/>'
    '<Override PartName="/word/numbering.xml" ContentType="'
    + CT_W + 'numbering+xml"/>'
    '</Types>\n')

root_rels = (XMLDECL +
    '<Relationships xmlns="http://schemas.openxmlformats.org/'
    'package/2006/relationships">'
    '<Relationship Id="rId1" Target="word/document.xml" '
    'Type="' + REL + 'officeDocument"/>'
    '</Relationships>\n')

doc_rels = (XMLDECL +
    '<Relationships xmlns="http://schemas.openxmlformats.org/'
    'package/2006/relationships">'
    '<Relationship Id="rIdStyles" Type="' + REL + 'styles" '
    'Target="styles.xml"/>'
    '<Relationship Id="rIdSettings" Type="' + REL + 'settings" '
    'Target="settings.xml"/>'
    '<Relationship Id="rIdNumbering" Type="' + REL + 'numbering" '
    'Target="numbering.xml"/>'
    '<Relationship Id="rIdChunk" Type="' + REL + 'aFChunk" '
    'Target="chunk.html"/>'
    '<Relationship Id="rIdOle1" Type="' + REL + 'oleObject" '
    'Target="ole1.bin"/>'
    '<Relationship Id="rIdFrSrc" Type="' + REL + 'image" '
    'Target="framesrc.html" TargetMode="External"/>'
    '</Relationships>\n')

path = os.path.join(OUT, 'cov.docx')
with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr('[Content_Types].xml', content_types)
    z.writestr('_rels/.rels', root_rels)
    z.writestr('word/document.xml', document_xml)
    z.writestr('word/_rels/document.xml.rels', doc_rels)
    z.writestr('word/styles.xml', styles_xml)
    z.writestr('word/settings.xml', settings_xml)
    z.writestr('word/numbering.xml', numbering_xml)
    z.writestr('word/chunk.html', chunk_html)
    z.writestr('word/ole1.bin', b'\x01' * 32)
print('wrote', path)
