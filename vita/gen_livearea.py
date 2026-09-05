#!/usr/bin/env python3
"""Generate placeholder LiveArea art so the repo stays text-only.

Writes 24-bit PNGs with stdlib only (struct + zlib): icon0, bg, startup.
Real artwork replaces these before any public release; the flat colours
below are deliberately neutral so nobody mistakes them for final art.

Usage: python3 gen_livearea.py <outdir>
"""

import struct
import sys
import zlib
from pathlib import Path


def write_png(path, width, height, rgb):
    def chunk(tag, payload):
        out = struct.pack(">I", len(payload)) + tag + payload
        return out + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    row = b"\x00" + bytes(rgb) * width
    raw = row * height
    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )
    path.write_bytes(png)


def main():
    if len(sys.argv) != 2:
        print("usage: gen_livearea.py <outdir>", file=sys.stderr)
        return 1
    out = Path(sys.argv[1])
    contents = out / "livearea" / "contents"
    contents.mkdir(parents=True, exist_ok=True)

    # Dark slate placeholders (icon 256x256, bg 840x500, startup 280x158).
    write_png(out / "icon0.png", 256, 256, (27, 42, 58))
    write_png(contents / "bg.png", 840, 500, (21, 33, 46))
    write_png(contents / "startup.png", 280, 158, (27, 42, 58))
    return 0


if __name__ == "__main__":
    sys.exit(main())
