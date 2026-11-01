#!/usr/bin/env python3
"""Convert the text PBM (P1) written by FakeDisplay::toPbm into a scaled PNG, with no third-party packages.

Usage: python tools/pbm_to_png.py in.pbm out.png [scale]   (default scale 5; lit pixels are drawn white on black)
"""
import struct
import sys
import zlib


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    scale = int(sys.argv[3]) if len(sys.argv) > 3 else 5
    tokens = open(sys.argv[1]).read().split()
    assert tokens[0] == "P1"
    w, h = int(tokens[1]), int(tokens[2])
    bits = "".join(tokens[3:])
    assert len(bits) == w * h
    rows = []
    for y in range(h):
        line = bytearray()
        for x in range(w):
            v = 255 if bits[y * w + x] == "1" else 0
            line += bytes([v]) * scale
        rows += [b"\x00" + bytes(line)] * scale  # filter byte 0 per scanline
    raw = b"".join(rows)

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w * scale, h * scale, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    open(sys.argv[2], "wb").write(png)
    return 0


if __name__ == "__main__":
    sys.exit(main())
