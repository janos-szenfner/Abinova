#!/usr/bin/env python3
"""Emit a C byte array for a binary file.

Replaces the old cdump.pl (itself a cross-compile-friendly
replacement for the original cdump binary). Used at build time to
turn xp/sidebar.png into ap_wp_sidebar.cpp.

Usage: cdump.py datafile arrayname
"""

import sys


def main() -> int:
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} datafile arrayname", file=sys.stderr)
        return 1

    path, name = sys.argv[1], sys.argv[2]
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError:
        sys.exit(f"Could not open file {path}")

    out = sys.stdout.write
    out(f"unsigned char {name} [] = {{\n")
    for i in range(0, len(data), 16):
        out("".join(f"0x{b:02x}," for b in data[i : i + 16]) + "\n")
    out(f"}};\nunsigned long {name}_sizeof = sizeof({name});\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
