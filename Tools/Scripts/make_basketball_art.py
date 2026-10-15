"""Make the basketball's leather for the three-point contest's balls: T_Basketball.png, a
latitude-longitude picture for the engine's sphere - orange pebbled leather with the black seams of
an eight-panel ball (the equator, one great circle through the poles, and the two curved seams).

Drawn here, by code, like the Go board's grain (make_go_wood.py): ours, so the PNG goes in
SourceArt/Basketball/ (tracked) and make_basketball_assets.py imports it. Where on the ball the
seams fall does not matter - it spins - so the sphere's UV layout needs no matching.

    python Tools/Scripts/make_basketball_art.py

Needs Pillow.
"""

import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(HERE, "..", "..", "SourceArt", "Basketball")


def write_ball():
    W, H = 1024, 512
    rng = random.Random(11)
    img = Image.new("RGB", (W, H), (214, 92, 30))
    px = img.load()
    # Pebbling: small random darker and lighter specks, softened.
    for _ in range(W * H // 6):
        x, y = rng.randrange(W), rng.randrange(H)
        k = rng.choice((0.78, 0.86, 1.08))
        r, g, b = px[x, y]
        px[x, y] = (min(255, int(r * k)), min(255, int(g * k)), min(255, int(b * k)))
    img = img.filter(ImageFilter.GaussianBlur(0.8))

    d = ImageDraw.Draw(img)
    seam = (26, 16, 12)
    w = 9
    # The equator, and the great circle through the poles (two meridians half a turn apart).
    d.line([(0, H / 2), (W, H / 2)], fill=seam, width=w)
    for u in (0.0, 0.5):
        x = u * W
        d.line([(x, 0), (x, H)], fill=seam, width=w)
    d.line([(W - 2, 0), (W - 2, H)], fill=seam, width=w)
    # The two curved seams: each bows away from the meridian, widest at the equator.
    for u0 in (0.25, 0.75):
        pts = []
        for i in range(0, H + 1, 4):
            lat = (i / H - 0.5) * math.pi
            x = (u0 + 0.10 * math.cos(lat) * (1 if u0 < 0.5 else -1)) * W
            pts.append((x, i))
        d.line(pts, fill=seam, width=w, joint="curve")
        pts = [(2 * u0 * W - x, y) for x, y in pts]
        d.line(pts, fill=seam, width=w, joint="curve")

    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, "T_Basketball.png")
    img.save(path)
    return path


if __name__ == "__main__":
    print("ball:", os.path.normpath(write_ball()))
