#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# MOLECULAR: the host's screens (PPM) as PNG, with no library.   tools/ppm2png.py FILE.ppm ...
import struct
import sys
import zlib
from pathlib import Path


def png(path):
    d = Path(path).read_bytes()
    parts = d.split(b"\n", 1)
    w, h = [int(v) for v in parts[0].split()[1:3]]
    px = parts[1]
    raw = b"".join(b"\0" + px[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(t, b):
        return struct.pack(">I", len(b)) + t + b + struct.pack(">I", zlib.crc32(t + b) & 0xFFFFFFFF)
    out = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    out += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    Path(path).with_suffix(".png").write_bytes(out)
    Path(path).unlink()


for p in sys.argv[1:]:
    png(p)
