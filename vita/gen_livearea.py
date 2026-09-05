#!/usr/bin/env python3
"""Generate placeholder LiveArea art so the repo stays text-only.

Writes stdlib-only (struct + zlib) PNGs that the Vita firmware accepts:
bit_depth=8, color_type=3 (256-entry indexed palette). The installer
rejects truecolor/grayscale PNGs with 0x8010113D, and icon0 must be
128x128 while the LiveArea background must be named bg0.png.

Usage: python3 gen_livearea.py <outdir>
"""

import struct
import sys
import zlib
from pathlib import Path


def write_vita_png(path, width, height, rgb):
    """Write a solid-color 8-bit palette PNG the Vita installer accepts."""
    def chunk(tag, payload):
        out = struct.pack(">I", len(payload)) + tag + payload
        return out + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    # Palette: our color first, rest zero-padded to exactly 256 entries.
    palette = bytes(rgb) + b"\x00" * (768 - 3)
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0)
    raw = b"".join(b"\x00" + b"\x00" * width for _ in range(height))
    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"PLTE", palette)
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

    # Dark slate placeholders. Sizes/names per Vita specs:
    # icon0 128x128, pic0 960x544, bg0 840x500, startup 280x158.
    write_vita_png(out / "icon0.png", 128, 128, (27, 42, 58))
    write_vita_png(out / "pic0.png", 960, 544, (21, 33, 46))
    write_vita_png(contents / "bg0.png", 840, 500, (21, 33, 46))
    write_vita_png(contents / "startup.png", 280, 158, (27, 42, 58))
    return 0


if __name__ == "__main__":
    sys.exit(main())
