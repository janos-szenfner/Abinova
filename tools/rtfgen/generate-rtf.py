#!/usr/bin/env python3
"""Generate the RTF keyword tables from rtf-keywords.txt.

Writes ie_imp_RTFKeywords.h and ie_imp_RTFKeywordIDs.h next to the
current directory; copy them into src/wp/impexp/xp/ after editing
rtf-keywords.txt. Replaces generate-rtf.pl.

Usage: generate-rtf.py rtf-keywords.txt
"""

import re
import sys

KEYWORD_RE = re.compile(r"\\ ?(clNoWrap|[a-zABCDFLRTW]*)(N)? ?((7\.0|95|97|2000|2002))?")

KEYWORD_IDS_HEADER = [
    "/* automatically generated, do no edit !*/",
    "#include <cstdint>",
    "typedef enum: uint16_t {",
    "\tRTF_UNKNOWN_KEYWORD = 0,",
    "\tRTF_KW_LF,",
    "\tRTF_KW_CR,",
    "\tRTF_KW_QUOTE,",
    "\tRTF_KW_HYPHEN,",
    "\tRTF_KW_STAR,",
    "\tRTF_KW_COLON,",
    "\tRTF_KW_BACKSLASH,",
    "\tRTF_KW_UNDERSCORE,",
    "\tRTF_KW_OPENCBRACE,",
    "\tRTF_KW_PIPE,",
    "\tRTF_KW_CLOSECBRACE,",
    "\tRTF_KW_TILDE,",
]

# the array is not const in C since it is sorted with qsort()
KEYWORDS_DECL = "_rtf_keyword rtfKeywords[] = {"
# fixed tokens sorting before 'A' in strcmp order
KEYWORDS_HEAD = [
    '\t{"\\n", false, false, NO_CONTEXT, RTF_KW_LF },',
    '\t{"\\r", false, false, NO_CONTEXT, RTF_KW_CR },',
    '\t{"\'", false, false, NO_CONTEXT, RTF_KW_QUOTE },',
    '\t{"*", false, false, NO_CONTEXT, RTF_KW_STAR },',
    '\t{"-", false, false, NO_CONTEXT, RTF_KW_HYPHEN },',
    '\t{":", false, false, NO_CONTEXT, RTF_KW_COLON },',
]
# fixed tokens sorting between 'Z' and 'a' in strcmp order
KEYWORDS_MID = [
    '\t{"\\\\", false, false, NO_CONTEXT, RTF_KW_BACKSLASH },',
    '\t{"_", false, false, NO_CONTEXT, RTF_KW_UNDERSCORE },',
]

KEYWORDS_FOOTER = [
    '\t{"{", false, false, NO_CONTEXT, RTF_KW_OPENCBRACE },',
    '\t{"|", false, false, NO_CONTEXT, RTF_KW_PIPE },',
    '\t{"}", false, false, NO_CONTEXT, RTF_KW_CLOSECBRACE },',
    '\t{"~", false, false, NO_CONTEXT, RTF_KW_TILDE }',
    "};",
]


def main() -> int:
    if len(sys.argv) != 2:
        sys.exit(f"Usage: {sys.argv[0]} rtf-keywords.txt")

    entries = {}
    try:
        with open(sys.argv[1], encoding="utf-8") as f:
            for line in f:
                m = KEYWORD_RE.match(line)
                if not m:
                    continue
                keyword = m.group(1)
                if keyword in entries:
                    print(f"Ignoring duplicate keyword {keyword}.")
                    continue

                hasparam = m.group(2) is not None
                version = m.group(3) or ""
                elem = f'\t{{"{keyword}", {"true" if hasparam else "false"}, '
                elem += f"false, NO_CONTEXT, RTF_KW_{keyword} }},"
                if version:
                    elem += f" /* {version} */"
                entries[keyword] = elem
    except OSError:
        sys.exit(f"Could not open file {sys.argv[1]}")

    # rtfKeywords is searched with bsearch() (after a runtime qsort),
    # so emit entries in strict strcmp order regardless of txt order:
    # symbols, then uppercase-keyed keywords, then backslash/underscore
    # tokens, then lowercase-keyed keywords.
    ordered = sorted(entries)
    upper = [k for k in ordered if k < "a"]
    lower = [k for k in ordered if k >= "a"]
    keywordids = list(KEYWORD_IDS_HEADER)
    keywordids += [f"\tRTF_KW_{k}," for k in ordered]
    keywordids += ["\tRTF_KW__END__", "} RTF_KEYWORD_ID;"]
    keywords = [KEYWORDS_DECL]
    keywords += KEYWORDS_HEAD
    keywords += [entries[k] for k in upper]
    keywords += KEYWORDS_MID
    keywords += [entries[k] for k in lower]
    keywords += KEYWORDS_FOOTER

    with open("ie_imp_RTFKeywordIDs.h", "w", encoding="utf-8") as f:
        f.write("\n".join(keywordids) + "\n")
    with open("ie_imp_RTFKeywords.h", "w", encoding="utf-8") as f:
        f.write("\n".join(keywords) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
