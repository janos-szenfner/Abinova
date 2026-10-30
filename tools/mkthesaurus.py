#!/usr/bin/env python3
"""mkthesaurus.py — build the bundled reduced English thesaurus.

The app resolves MyThes pairs (th_<lang>_v2.idx/.dat) in the system
dictionary dirs first and falls back to the bundled copy in
<datadir>/abinova-4.0/thesaurus/.  The bundled copy is a subset of
the system mythes-en-us data limited to the words that actually
occur in this repository's documentation and the ~/Documents/*.docx
reference corpus (top-N by frequency), so it stays ~1 MB instead of
the full ~18 MB upstream file.

Data provenance: libreoffice-dictionaries en thesaurus, word lists
Copyright 2000-2018 Kevin Atkinson <kevina@gnu.org>, 2016 Benjamin
Titze — see the license paragraph in ATTRIBUTION.md.

Usage:
    tools/mkthesaurus.py [--top N] [--out thesaurus]
                         [--mythes /usr/share/mythes]
                         [--corpus DIR ...]
"""

import argparse
import glob
import os
import re
import zipfile
from collections import Counter

WORD = re.compile(r"[A-Za-z][a-z]{2,20}")

# ultra-common everyday words the documentation corpus may not rank
# high enough — a thesaurus that misses "happy" is useless
SEED_WORDS = """
able about above accept across act action add afraid after again age ago
agree air all allow almost alone along already also always amount angry
answer any appear area arm around arrive art ask asleep baby back bad bag
ball bank base beautiful bed begin believe below best better big bird bit
black blood blue board body book born both box boy brain brave break
bright bring brother brown build business busy but buy call calm can
capital car care careful carry case cat catch cause center certain chance
change charge child children choice choose city claim class clean clear
close cloth cold color come common company complete consider contain
continue control cool corner correct cost could count country course
cover create cross cry cup current cut dark daughter day dead deal dear
death decide deep demand describe design desk develop die different
difficult direction discover discuss disease distance do dog door down
draw dream dress drink drive drop dry during each early earth east easy
eat edge effect effort either else empty end energy enjoy enough enter
equal especially even evening event ever every exact example except
exciting exercise exist expect experience explain express eye face fact
fail fair fall family famous far fast fat father fear feel few field
fight fill final find fine finger finish fire first fish fit five fix
flat floor flow fly follow food foot for force foreign forest forget
form forward four free friend front fruit full fun funny future game
garden gather general gentle get gift girl give glad glass go god gold
good great green ground group grow guess guide hair half hand happen
happy hard hat have he head hear heart heat heavy help her here high
hill him his history hit hold home hope horse hot hour house how huge
human hundred hungry hunt hurt idea if important improve include
increase indeed inside interest interesting into island it job join joy
jump just keep key kind king kiss kitchen knee know lady lake land large
last late laugh law lazy lead learn least leave left leg less letter
level life light like line list listen little live long look lose loud
love low lucky machine mad main make man many map mark market marry
matter may mean measure meet member memory men metal middle might mile
milk mind mine minute miss moment money month moon more morning most
mother mountain mouth move much music must my name narrow nation
nature near nearly necessary neck need never new news next nice night
no none north nose not note nothing notice now number object ocean of
off offer office often oil old on once one only open opposite or order
other our out outside over own page pain pair paper parent part party
pass past path pay peace people perfect perhaps person pick picture
piece place plan plane plant play pleasant please point poor popular
position possible power prepare present pretty price problem produce
promise proper proud prove public pull push put quality question quick
quiet quite rain raise reach read ready real realize reason receive
recent red remember repeat reply report rest result return rich ride
right ring rise river road rock roll room round rule run sad safe same
save say scene school sea season seat second secret see seem sell send
sense serious serve set settle several shape share sharp she ship shoe
shop short should shout show sick side sight sign silly simple since
sing single sister sit six size skin sky sleep slow small smart smile
snow so soft soldier some son song soon sorry sound south speak special
speed spend spring stand star start state station stay step still stop
store storm story strange street strong student study stupid subject
succeed such sudden sugar suggest summer sun support sure surprise
sweet swim table take talk tall taste teach team tell ten term terrible
test than thank that the their them then there these they thick thin
thing think third this those thought three through throw time tired to
today together too top total touch toward town track train travel tree
trip true trust try turn twelve two ugly under understand unit until up
use usual valuable very village visit voice wait walk wall want war warm
wash watch water wave way we weak wear weather week weight welcome well
west wet what wheel when where which while white who whole why wide
wife wild will win wind window wine winter wise wish with without woman
wonder wonderful wood word work world worry worth would write wrong
year yellow yes yesterday yet you young
"""

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def corpus_words(extra_dirs):
    """rank words by how often they appear in our docs + corpus"""
    cnt = Counter()
    files = []
    for pat in ("help/**/*", "docs/**/*", "*.md", "*.txt"):
        files += [f for f in glob.glob(os.path.join(REPO_ROOT, pat),
                                       recursive=True)
                  if os.path.isfile(f)]
    for d in extra_dirs:
        files += glob.glob(os.path.join(d, "**", "*.docx"),
                           recursive=True)
    for f in files:
        if f.endswith(".docx"):
            try:
                with zipfile.ZipFile(f) as z:
                    for n in z.namelist():
                        if n.startswith("word/") and n.endswith(".xml"):
                            cnt.update(WORD.findall(
                                z.read(n).decode("utf-8", "ignore")))
            except Exception:
                pass
        else:
            try:
                cnt.update(WORD.findall(
                    open(f, encoding="utf-8", errors="ignore").read()))
            except Exception:
                pass
    return cnt


def load_index(idx_path):
    idx = {}
    with open(idx_path, encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.rstrip("\n")
            if "|" not in line:
                continue
            w, off = line.rsplit("|", 1)
            try:
                idx.setdefault(w, []).append(int(off))
            except ValueError:
                pass
    return idx


def dat_record(data, off):
    """return the whole record (header + meaning lines) at off"""
    end = data.find(b"\n", off)
    if end < 0:
        return b""
    nmean = int(data[off:end].split(b"|")[1])
    p = end + 1
    for _ in range(nmean):
        e = data.find(b"\n", p)
        if e < 0:
            e = len(data)
        p = e + 1
    return data[off:p]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--top", type=int, default=2500)
    ap.add_argument("--out", default=os.path.join(REPO_ROOT,
                                                "thesaurus"))
    ap.add_argument("--mythes", default="/usr/share/mythes")
    ap.add_argument("--corpus", action="append",
                    default=[os.path.expanduser("~/Documents")])
    a = ap.parse_args()

    idx = load_index(os.path.join(a.mythes, "th_en_US_v2.idx"))
    data = open(os.path.join(a.mythes, "th_en_US_v2.dat"), "rb").read()

    cnt = corpus_words(a.corpus)
    cand = set()
    for w, _ in cnt.most_common():
        w = w.lower()
        if w in idx and " " not in w:
            cand.add(w)
        if len(cand) >= a.top:
            break
    cand.update(w for w in SEED_WORDS.split() if w in idx)
    cand = sorted(cand)

    os.makedirs(a.out, exist_ok=True)
    out_idx = open(os.path.join(a.out, "th_en_US_v2.idx"), "w",
                   encoding="utf-8")
    out_dat = open(os.path.join(a.out, "th_en_US_v2.dat"), "wb")
    out_idx.write("UTF-8\n")
    out_dat.write(b"UTF-8\n")
    for w in cand:
        for off in idx[w]:
            rec = dat_record(data, off)
            if not rec:
                continue
            pos = out_dat.tell()
            out_idx.write(f"{w}|{pos}\n")
            out_dat.write(rec)
    out_idx.close()
    out_dat.close()

    size = os.path.getsize(os.path.join(a.out, "th_en_US_v2.dat"))
    print(f"{len(cand)} words -> {a.out} ({size // 1024} KB dat)")


if __name__ == "__main__":
    main()
