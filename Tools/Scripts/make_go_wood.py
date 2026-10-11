# make_go_wood.py — the Go board's wood grain, drawn in code: SourceArt/Go/T_GoWood.png.
#
#   python Tools/Scripts/make_go_wood.py
#
# Plain Python, no libraries (this PC's Python has no numpy or PIL, and Unreal's does not
# need them either). make_go_assets.py imports the picture as /Game/Go/T_GoWood.
#
# WHY DRAWN AND NOT BOUGHT: everything on the Go table is made here, so nothing on it carries a
# vendor's "Allows usage with AI: No", and its renders can be looked at and fixed by eye.
#
# THE LOOK is kaya cut for a board: masame, straight grain - fine, close, nearly parallel
# lines with a slow sway, a few broader bands where the growth rings were wider, and a little
# per-pixel noise so it is not printed. It is near-white: M_GoSurface multiplies it by the
# board's Color, so the yellow of the wood is set on the table, not baked in here.

import math
import os
import struct
import zlib

SIZE = 1024
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                   "SourceArt", "Go", "T_GoWood.png")


def png(path, width, height, rows):
    """rows: bytes per row, RGB. A PNG is a signature and three chunks."""
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + row for row in rows)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def main():
    seed = 12345
    rows = []
    two_pi = 2.0 * math.pi
    for y in range(SIZE):
        v = y / SIZE
        row = bytearray()
        for x in range(SIZE):
            u = x / SIZE
            # Across the grain is y; along it, x. The lines sway a little as they run, and their
            # spacing wanders - a tree does not grow the same amount every year.
            across = y + 3.0 * math.sin(two_pi * (1.1 * u + 0.11)) + 1.2 * math.sin(two_pi * (2.3 * u + 1.7 * v))
            across += 14.0 * math.sin(across / 83.0) + 6.0 * math.sin(across / 29.0 + 1.3)
            fine = (0.5 + 0.5 * math.sin(two_pi * across / 7.0)) ** 10   # thin dark lines, not stripes
            fine *= 0.55 + 0.45 * (0.5 + 0.5 * math.sin(across / 53.0 + 2.0 * u))
            ring = 0.5 + 0.5 * math.sin(two_pi * (across / SIZE * 7.0 + 0.3 * math.sin(two_pi * u)))
            late = 0.5 + 0.5 * math.sin(two_pi * across / 41.0)
            late = late ** 3
            seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
            grit = (seed / 0x7FFFFFFF - 0.5) * 0.035
            value = 0.97 - 0.16 * fine - 0.05 * ring - 0.07 * late + grit
            value = min(1.0, max(0.0, value))
            row += bytes((int(255 * value), int(255 * value * 0.975), int(255 * value * 0.93)))
        rows.append(bytes(row))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    png(OUT, SIZE, SIZE, rows)
    print("wrote", OUT, os.path.getsize(OUT), "bytes")


if __name__ == "__main__":
    main()
