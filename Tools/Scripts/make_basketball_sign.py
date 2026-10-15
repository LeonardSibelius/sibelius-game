"""Make the living-room door sign for the three-point contest: "BASKETBALL" over a ball dropping
through a hoop.

Walt, 2026-10-15: "let's do the basketball shooting contest - can the door go in front of
SM_Curtains_A7?" The card, the gold border and its glow, and the lettering are the chess and
Go signs' (make_chess_sign.py, make_go_sign.py), so the doors read as a row. Where those have
the knight and the board, this one has a basketball falling into an orange rim and a white net -
all drawn here, by code, like the Go board.

Ours, so the PNG goes in SourceArt/Basketball/ (tracked) and make_basketball_assets.py imports it.

    python Tools/Scripts/make_basketball_sign.py

Needs Pillow.
"""

import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "SourceArt", "Basketball", "T_Sign_Basketball.png")

LEATHER = (226, 106, 32)
SEAM = (34, 20, 14, 255)
RIM = (232, 72, 28, 255)
NET = (246, 244, 236, 255)


def font_of(size):
    for name in ("impact.ttf", "ariblk.ttf", "arialbd.ttf"):
        try:
            return ImageFont.truetype(os.path.join(os.environ.get("WINDIR", "C:/Windows"), "Fonts", name), size)
        except OSError:
            continue
    return ImageFont.load_default()


def ball_image(size):
    """A basketball: orange leather lit from the upper left, with its eight-panel seams."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    r = size / 2.0 - 4
    cx = cy = size / 2.0
    # Leather, shaded: brighter toward the light, darker toward the far edge.
    shade = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    px = shade.load()
    lx, ly = cx - r * 0.35, cy - r * 0.40
    for y in range(size):
        for x in range(size):
            dx, dy = x - cx, y - cy
            if dx * dx + dy * dy > r * r:
                continue
            light = 1.0 - min(1.0, math.hypot(x - lx, y - ly) / (r * 1.9))
            k = 0.55 + 0.6 * light
            px[x, y] = (min(255, int(LEATHER[0] * k)), min(255, int(LEATHER[1] * k)), min(255, int(LEATHER[2] * k)), 255)
    img.alpha_composite(shade)

    d = ImageDraw.Draw(img)
    w = max(4, int(size * 0.022))
    # The two great-circle seams seen side-on: a straight vertical and a gently bowed horizontal.
    d.line([(cx, cy - r), (cx, cy + r)], fill=SEAM, width=w)
    d.arc([cx - r, cy - r * 0.18, cx + r, cy + r * 0.18], 0, 180, fill=SEAM, width=w)
    # The two curved seams that make the eight panels, bowing in from either side.
    d.arc([cx - r * 1.55, cy - r, cx - r * 0.25, cy + r], -62, 62, fill=SEAM, width=w)
    d.arc([cx + r * 0.25, cy - r, cx + r * 1.55, cy + r], 118, 242, fill=SEAM, width=w)
    # Pebbled grain: a faint scatter of darker dots (seeded, so the sign is the same every run;
    # a golden-angle spiral here drew a visible ring).
    rng = random.Random(7)
    for _ in range(int(size * 1.4)):
        a = rng.uniform(0.0, 2 * math.pi)
        rr = r * math.sqrt(rng.random()) * 0.96
        x, y = cx + rr * math.cos(a), cy + rr * math.sin(a)
        d.point((x, y), fill=(120, 50, 16, 90))
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
    out = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    out.paste(img, (0, 0), mask)
    return out


def hoop_layer(W, H, cx, rim_y, rim_w):
    """The back of the rim and the net hang behind the ball; the front of the rim goes over it."""
    back = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    front = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    bd, fd = ImageDraw.Draw(back), ImageDraw.Draw(front)
    rim_h = rim_w * 0.24
    box = [cx - rim_w / 2, rim_y - rim_h / 2, cx + rim_w / 2, rim_y + rim_h / 2]
    thick = int(rim_w * 0.045)

    # The net: diamond mesh tapering down from the rim.
    depth = rim_w * 0.78
    bottom = rim_w * 0.55
    strands = 12
    rows = 6
    pts = []
    for row in range(rows + 1):
        t = row / rows
        half = (rim_w / 2) * (1 - t) + (bottom / 2) * t
        y = rim_y + depth * t
        line = []
        for s in range(strands + 1):
            u = s / strands + (0.5 / strands if row % 2 else 0.0)
            x = cx - half + 2 * half * min(1.0, u)
            line.append((x, y + math.sin(u * math.pi) * rim_h * 0.25 * (1 - t)))
        pts.append(line)
    nw = max(3, int(rim_w * 0.012))
    for row in range(rows):
        for s in range(strands + 1):
            a = pts[row][s]
            b1 = pts[row + 1][max(0, s - 1)] if row % 2 == 0 else pts[row + 1][s]
            b2 = pts[row + 1][s] if row % 2 == 0 else pts[row + 1][min(strands, s + 1)]
            bd.line([a, b1], fill=NET, width=nw)
            bd.line([a, b2], fill=NET, width=nw)

    bd.arc(box, 180, 360, fill=RIM, width=thick)     # the far half of the rim
    fd.arc(box, 0, 180, fill=RIM, width=thick)       # the near half
    return back, front


def write_sign():
    W, H = 1024, 2048
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    # The card: ebony, warming toward the bottom, rounded like a playing card (the chess sign's).
    card = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(card)
    m, rad = 24, 90
    for y in range(m, H - m):
        t = (y - m) / (H - 2 * m)
        d.line([(m, y), (W - m, y)], fill=(int(22 + 26 * t), int(14 + 14 * t), int(10 + 8 * t), 255))
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).rounded_rectangle([m, m, W - m, H - m], radius=rad, fill=255)
    img.paste(card, (0, 0), mask)

    # A gold border, glowing.
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(glow).rounded_rectangle([m + 30, m + 30, W - m - 30, H - m - 30], radius=rad - 20,
                                           outline=(255, 190, 70, 255), width=22)
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(18)))
    img.alpha_composite(glow)

    # The word, as large as fits inside the border: ten letters where GO has two.
    word = "BASKETBALL"
    size = 300
    while size > 60:
        font = font_of(size)
        box = ImageDraw.Draw(img).textbbox((0, 0), word, font=font)
        if box[2] - box[0] <= W - 2 * m - 120:
            break
        size -= 6
    text = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    td = ImageDraw.Draw(text)
    tx, ty = (W - (box[2] - box[0])) // 2 - box[0], 170 - box[1]
    td.text((tx, ty), word, font=font, fill=(255, 200, 90, 255))
    img.alpha_composite(text.filter(ImageFilter.GaussianBlur(14)))
    ImageDraw.Draw(img).text((tx, ty), word, font=font, fill=(255, 246, 225, 255))
    word_bottom = 170 + (box[3] - box[1])

    # Below the word: a ball dropping into the hoop, with a warm halo behind them both.
    cx = W // 2
    rim_w = 600
    rim_y = int(H * 0.62)
    ball = 470
    bx, by = cx - ball // 2, rim_y - int(ball * 0.86)
    by = max(by, word_bottom + 120)

    halo = Image.new("RGBA", (W, H), (255, 180, 80, 0))
    halo_mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(halo_mask).ellipse([cx - 380, by - 60, cx + 380, rim_y + 520], fill=200)
    halo.putalpha(halo_mask.filter(ImageFilter.GaussianBlur(60)))
    img.alpha_composite(halo)

    back, front = hoop_layer(W, H, cx, rim_y, rim_w)
    img.alpha_composite(back)
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).ellipse([bx + 16, by + 26, bx + ball + 16, by + ball + 26], fill=(0, 0, 0, 130))
    img.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(20)))
    img.alpha_composite(ball_image(ball), (bx, by))
    img.alpha_composite(front)

    # Motion: three short speed lines above the ball, falling.
    sd = ImageDraw.Draw(img)
    for i, off in enumerate((-110, 0, 110)):
        top = by - 150 + abs(off) * 0.4
        sd.line([(cx + off, top), (cx + off, top + 90)], fill=(255, 220, 150, 170 - i * 20), width=10)

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    img.save(OUT)
    return OUT


if __name__ == "__main__":
    print("sign:", os.path.normpath(write_sign()))
