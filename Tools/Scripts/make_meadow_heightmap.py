# make_meadow_heightmap.py — the battlefield, as a 16-bit heightmap.
#
# RUN THIS ANYWHERE. It needs no editor and no Unreal at all:
#
#   "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" ^
#       Tools/Scripts/make_meadow_heightmap.py
#
# Writes Content/Environments/Heightmaps/HM_Meadow.png, which UE imports as a Landscape
# (Landscape mode -> New -> Import from File).
#
# ---------------------------------------------------------------------------
# WHY A MEADOW AND NOT A FOREST.
#
# The Swarm power wants armies, and an army you cannot see is not an army. Trees occlude
# the exact thing the feature exists to show, and dense foliage is overdraw - the most
# expensive thing you can put between a camera and a crowd. Strip the trees and the whole
# frame budget goes to pawns instead of leaves.
#
# The forests are already gone from this project (Content/Forest is 1 MB and empty; the
# five entries in MapsToCook do not include one), so this replaces nothing. It is new
# ground, built for the fight.
#
# ---------------------------------------------------------------------------
# THE SHAPE: A BOWL, NOT A CONE.
#
# Flat in the middle so the fight has a floor and the navmesh is trivial. Hills around
# the rim, because a ridge line is where an army reads - silhouetted against sky, above
# the player, visible from anywhere in the bowl. That is the shot.
#
# The rise is a smoothstep, not a linear ramp: a linear slope meets the flat centre at a
# visible crease, and the eye finds the seam immediately. Smoothstep leaves no crease.
#
# The noise matters more than it looks. A perfectly radial bowl reads as a golf bunker -
# obviously machined, and it kills the shot. A few octaves of value noise, weighted
# toward the rim so the floor stays flat and navigable, make the hills read as terrain
# while the meadow stays a meadow.
#
# ---------------------------------------------------------------------------
# UNREAL'S SIZE RULES. Landscape resolutions are (components x quads) + 1, so valid
# sizes are 505, 1009, 2017 and so on. 1009 with the default 100 uu/quad is a bowl about
# a kilometre across, which is right for hundreds of demons on a ridge: far enough that
# the horde reads as distant, near enough to charge.
#
# Import settings to use, so the numbers below mean what they say:
#   Scale        100, 100, 100   (default)
#   Height range  the Z scale below turns the 16-bit range into RIM_HEIGHT_CM
#
# 16-bit greyscale PNG, written by hand - no PIL, no numpy, nothing to install. PNG is a
# simple enough container that a dependency would be the more fragile choice.

import math
import os
import struct
import zlib

# ---- the knobs ------------------------------------------------------------
SIZE = 1009                # UE landscape size: (components * quads) + 1
FLAT_RADIUS = 0.42         # 0..1 of half-width. Inside this it is dead flat.
RIM_RADIUS = 0.97          # where the hills top out
RIM_HEIGHT_CM = 6000.0     # how tall the hills stand above the meadow floor
NOISE_CELLS = 7            # value-noise grid; higher = busier hills
NOISE_STRENGTH = 0.22      # of the rim height, at the rim
NOISE_OCTAVES = 4
FLOOR_WOBBLE_CM = 45.0     # a little life in the flat, so it is not a table
SEED = 20260826

OUT_DIR = os.path.join("Content", "Environments", "Heightmaps")
OUT_PNG = os.path.join(OUT_DIR, "HM_Meadow.png")

# UE maps the full 16-bit range to 512m by default at Z scale 100.
UE_FULL_RANGE_CM = 51200.0


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


class ValueNoise:
    """Cheap lattice noise. Deterministic from SEED so the battlefield is the same
    every time it is generated - a landscape that changes under you between runs is
    not a level, it is a surprise."""

    def __init__(self, cells, seed):
        self.cells = cells
        self.seed = seed

    def _rand(self, ix, iy):
        h = (ix * 374761393 + iy * 668265263 + self.seed * 1442695040888963407) & 0xFFFFFFFF
        h = (h ^ (h >> 13)) * 1274126177 & 0xFFFFFFFF
        return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0

    def at(self, u, v):
        x = u * self.cells
        y = v * self.cells
        ix, iy = int(math.floor(x)), int(math.floor(y))
        fx, fy = smoothstep(x - ix), smoothstep(y - iy)
        a = self._rand(ix, iy)
        b = self._rand(ix + 1, iy)
        c = self._rand(ix, iy + 1)
        d = self._rand(ix + 1, iy + 1)
        return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def build():
    half = (SIZE - 1) / 2.0
    octaves = [ValueNoise(NOISE_CELLS * (2 ** o), SEED + o * 977) for o in range(NOISE_OCTAVES)]

    rows = []
    for y in range(SIZE):
        # Filter byte 0 (None) per PNG scanline.
        row = bytearray(b"\x00")
        ny = (y - half) / half
        for x in range(SIZE):
            nx = (x - half) / half
            r = math.hypot(nx, ny)

            # The bowl.
            if r <= FLAT_RADIUS:
                rise = 0.0
            else:
                t = (r - FLAT_RADIUS) / max(1e-6, RIM_RADIUS - FLAT_RADIUS)
                rise = smoothstep(t)

            # Noise, faded out toward the middle so the floor stays flat and the
            # navmesh over it stays trivial.
            n = 0.0
            amp = 1.0
            total = 0.0
            for o, noise in enumerate(octaves):
                n += noise.at(x / (SIZE - 1.0), y / (SIZE - 1.0)) * amp
                total += amp
                amp *= 0.5
            n = (n / total) * 2.0 - 1.0

            height_cm = rise * RIM_HEIGHT_CM
            height_cm += n * NOISE_STRENGTH * RIM_HEIGHT_CM * rise
            height_cm += n * FLOOR_WOBBLE_CM * (1.0 - rise)

            # Sit the floor at mid-range so sculpting has room in both directions.
            v = 32768 + int(round(height_cm / UE_FULL_RANGE_CM * 65535.0))
            v = max(0, min(65535, v))
            row += struct.pack(">H", v)
        rows.append(bytes(row))
    return b"".join(rows)


def chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))


def main():
    raw = build()
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 16, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")

    os.makedirs(OUT_DIR, exist_ok=True)
    with open(OUT_PNG, "wb") as f:
        f.write(png)

    print("wrote %s" % OUT_PNG)
    print("  %d x %d, 16-bit greyscale, %.1f KB" % (SIZE, SIZE, len(png) / 1024.0))
    print("  arena  ~%.0f m across at 100 uu/quad" % ((SIZE - 1) * 100 / 100.0))
    print("  flat floor to r=%.2f, hills to %.0f m at the rim"
          % (FLAT_RADIUS, RIM_HEIGHT_CM / 100.0))


if __name__ == "__main__":
    main()
