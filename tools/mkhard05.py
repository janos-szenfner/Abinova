#!/usr/bin/env python3
"""HARD05 structural-limit ("layout bomb") fixtures.

Generates degenerate-but-well-formed documents into an output
directory (default /tmp/hard05 — pass a target dir as argv[1]).
Each file is tiny-to-moderate on disk but requests unbounded work at
import/layout; the HARD05 load caps (PT_LOAD_MAX_* in pt_Types.h,
FP_TABLE_MAX_ROWS/COLS, the ODF repeat clamps) must make every one
either open truncated-but-valid or degrade cleanly inside a timeout:

  bomb_attach.abwn   -- 2 cells carrying bot-attach/right-attach in
                        the millions (document-supplied table extents)
  bomb_depth.abwn    -- 100 nested <table> in <cell> (depth cap 32)
  bomb_lists.abwn    -- 10000 <l> defs, all chained on parentid
                        (count cap 8192 / ancestor cap 64)
  bomb_styles.abwn   -- 100000 <s> style defs (cap 65536)
  bomb_frags.abwn    -- ~400k paragraphs (>1e6 fragments, cap 1e6)
  bomb_rows.fodt     -- table:number-rows-repeated="1000000000"
  bomb_cols.fodt     -- table:number-columns-repeated="1000000000"
  bomb_span.fodt     -- 1e9 col/row spans on one cell
  bomb_odfnest.fodt  -- <text:list> nested 200 deep
  bomb_all.abwn      -- every abwn bomb combined in one document
"""
import os
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else '/tmp/hard05'
os.makedirs(OUT, exist_ok=True)

ABWN_NS = 'xmlns="https://raw.githubusercontent.com/janos-szenfner/Abinova/main/abwn.dtd"'


def write(name, data):
    mode = 'wb' if isinstance(data, (bytes, bytearray)) else 'w'
    with open(os.path.join(OUT, name), mode) as f:
        f.write(data)
    print('wrote', os.path.join(OUT, name))


def abwn(body, prelude=''):
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            f'<abinova {ABWN_NS} version="4.0">\n'
            f'{prelude}<section>\n{body}\n</section>\n</abinova>\n')


# ==== bomb_attach.abwn ==================================================
# One cell claiming a 5M-row extent and one claiming 2M columns.
write('bomb_attach.abwn', abwn(
    '<table xid="1">'
    '<cell xid="2" props="left-attach:0; top-attach:0; right-attach:1;'
    ' bot-attach:5000000"><p><c>rows</c></p></cell>'
    '<cell xid="3" props="left-attach:1; top-attach:0;'
    ' right-attach:2000000; bot-attach:1"><p><c>cols</c></p></cell>'
    '</table>\n<p><c>after table</c></p>'))


# ==== bomb_depth.abwn ===================================================
depth = 100
open_t = '<table><cell props="left-attach:0; top-attach:0;' \
         ' right-attach:1; bot-attach:1">' * depth
close_t = '</cell></table>' * depth
write('bomb_depth.abwn', abwn(
    f'{open_t}<p><c>deepest</c></p>{close_t}\n<p><c>after</c></p>'))


# ==== bomb_lists.abwn ===================================================
# 10000 list defs chained parentid -> previous (count + ancestor depth)
lists = ['<lists>']
for i in range(1, 10001):
    lists.append(f'<l id="{i}" parentid="{i - 1}" type="0"'
                 f' list-style="Numbered"'
                 f' start-value="1" list-delim="%L."/>')
lists.append('</lists>')
write('bomb_lists.abwn', abwn(
    '<p listid="1"><c>list para</c></p>', ''.join(lists)))


# ==== bomb_styles.abwn ==================================================
styles = ['<styles>']
for i in range(100000):
    styles.append(f'<s name="Sty{i}" type="P" basedon="Normal"'
                  f' followedby="Normal" props="font-size:{8 + (i % 40)}pt"/>')
styles.append('</styles>')
write('bomb_styles.abwn', abwn(
    '<p><c>styled doc</c></p>', ''.join(styles)))


# ==== bomb_frags.abwn ===================================================
# ~400k paragraphs; each emits a block strux + fmt marks + a span —
# well over the 1M fragment cap from a ~11MB file.
write('bomb_frags.abwn', abwn('<p><c>x</c></p>\n' * 400000))


# ==== ODF helpers =======================================================
def fodt(body):
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            '<office:document'
            ' xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"'
            ' xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0"'
            ' xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"'
            ' office:version="1.2"'
            ' office:mimetype="application/vnd.oasis.opendocument.text">'
            '<office:body><office:text>'
            f'{body}'
            '</office:text></office:body></office:document>\n')


def cell(txt='c'):
    return f'<table:table-cell><text:p>{txt}</text:p></table:table-cell>'


# ==== bomb_rows.fodt ====================================================
write('bomb_rows.fodt', fodt(
    '<table:table table:name="b">'
    '<table:table-column table:number-columns-repeated="3"/>'
    '<table:table-row table:number-rows-repeated="1000000000">'
    f'{cell()}{cell()}{cell()}'
    '</table:table-row></table:table>'
    '<text:p>after</text:p>'))


# ==== bomb_cols.fodt ====================================================
write('bomb_cols.fodt', fodt(
    '<table:table table:name="b">'
    '<table:table-column table:number-columns-repeated="1000000000"/>'
    f'<table:table-row>{cell()}</table:table-row>'
    '</table:table><text:p>after</text:p>'))


# ==== bomb_span.fodt ====================================================
write('bomb_span.fodt', fodt(
    '<table:table table:name="b">'
    '<table:table-column table:number-columns-repeated="2"/>'
    '<table:table-row>'
    '<table:table-cell table:number-columns-spanned="1000000000"'
    ' table:number-rows-spanned="1000000000">'
    '<text:p>span</text:p></table:table-cell>'
    '<table:covered-table-cell/>'
    f'{cell()}</table:table-row>'
    '<table:table-row>'
    '<table:covered-table-cell/>'
    f'{cell()}</table:table-row>'
    '</table:table><text:p>after</text:p>'))


# ==== bomb_odfnest.fodt =================================================
nest = 200
ol = '<text:list><text:list-item>' * nest
cl = '</text:list-item></text:list>' * nest
write('bomb_odfnest.fodt', fodt(
    f'{ol}<text:p>deep item</text:p>{cl}<text:p>after</text:p>'))


# ==== bomb_all.abwn =====================================================
all_body = (
    '<table><cell props="left-attach:0; top-attach:0; right-attach:1;'
    ' bot-attach:5000000"><p><c>rows</c></p></cell></table>\n'
    + open_t + '<p><c>deep</c></p>' + close_t + '\n'
    + '<p><c>tail</c></p>\n')
all_prelude = (
    '<styles>' + ''.join(
        f'<s name="S{i}" type="P"/>' for i in range(70000))
    + '</styles><lists>' + ''.join(
        f'<l id="{i}" parentid="{i - 1}"'
        ' type="0" list-style="Bulleted"'
        ' start-value="1" list-delim="%L."/>'
        for i in range(1, 9001))
    + '</lists>')
write('bomb_all.abwn', abwn(all_body, all_prelude))
