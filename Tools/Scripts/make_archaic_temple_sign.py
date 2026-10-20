"""Make the living-room door sign for the Archaic Language Temple.

Walt, 2026-10-19: a hidden door "next to poker made visible with V and call it the Archaic
Language Temple". The card, the gold border and its glow, and the lettering are
make_chess_sign.py's, so it reads with the CHESS, BOWLING and GO doors - but this one is seen only
in Code Vision, like the office's and the attic's secret doors. Under the name, a temple front
drawn here in gold line: pediment, entablature, six columns, steps; the five old languages cut
into the frieze.

Ours, so the PNG goes in SourceArt/ArchaicTemple/ (tracked) and place_temple_door.py imports it.

    python Tools/Scripts/make_archaic_temple_sign.py

Needs Pillow.
"""

import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "SourceArt", "ArchaicTemple", "T_Sign_ArchaicTemple.png")

GOLD = (255, 190, 70, 255)
CREAM = (255, 246, 225, 255)


def font(size, names=("impact.ttf", "ariblk.ttf", "arialbd.ttf")):
    for name in names:
        try:
            return ImageFont.truetype(os.path.join(os.environ.get("WINDIR", "C:/Windows"), "Fonts", name), size)
        except OSError:
            continue
    return ImageFont.load_default()


def glowing_text(img, word, f, cy):
    W = img.width
    text = Image.new("RGBA", img.size, (0, 0, 0, 0))
    td = ImageDraw.Draw(text)
    box = td.textbbox((0, 0), word, font=f)
    tx, ty = (W - (box[2] - box[0])) // 2 - box[0], cy - (box[3] - box[1]) // 2 - box[1]
    td.text((tx, ty), word, font=f, fill=(255, 200, 90, 255))
    img.alpha_composite(text.filter(ImageFilter.GaussianBlur(14)))
    ImageDraw.Draw(img).text((tx, ty), word, font=f, fill=CREAM)


def temple(W, H, top, bottom):
    """A temple front in gold line, from y=top to y=bottom, centred."""
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    left, right = 190, W - 190   # inside the border, steps and all
    width = right - left
    line = 14

    # Pediment: a low triangle, as the Greeks built it (about 1:4).
    ped_base = top + int(0.13 * width)
    d.polygon([(left - 20, ped_base), ((left + right) // 2, top), (right + 20, ped_base)], outline=GOLD, width=line)
    # Entablature, with the frieze between two bands.
    frieze_top, frieze_bottom = ped_base + 22, ped_base + 120
    d.rectangle([left - 20, ped_base, right + 20, frieze_top], fill=GOLD)
    d.rectangle([left - 20, frieze_bottom, right + 20, frieze_bottom + 22], fill=GOLD)
    # Steps.
    steps_top = bottom - 3 * 30
    for i in range(3):
        y = steps_top + i * 30
        d.rectangle([left - 20 - i * 22, y, right + 20 + i * 22, y + 18], fill=GOLD)
    # Six columns, fluted with two lines each, capital and base.
    col_top, col_bottom = frieze_bottom + 22, steps_top - 12
    n = 6
    span = width / (n - 1)
    cw = 46
    for i in range(n):
        cx = int(left + i * span)
        d.rectangle([cx - cw // 2 - 14, col_top, cx + cw // 2 + 14, col_top + 18], fill=GOLD)      # capital
        d.rectangle([cx - cw // 2, col_top + 18, cx + cw // 2, col_bottom - 14], outline=GOLD, width=8)
        for k in (-1, 1):
            fx = cx + k * cw // 6
            d.line([(fx, col_top + 30), (fx, col_bottom - 26)], fill=GOLD, width=4)
        d.rectangle([cx - cw // 2 - 10, col_bottom - 14, cx + cw // 2 + 10, col_bottom], fill=GOLD)  # base
    return layer, (frieze_top, frieze_bottom)


def write_sign():
    W, H = 1024, 2048
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    # The card: ebony, warming toward the bottom, rounded like a playing card.
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
                                           outline=GOLD, width=22)
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(18)))
    img.alpha_composite(glow)

    # The name, three words stacked.
    big = font(190)
    for word, cy in (("ARCHAIC", 250), ("LANGUAGE", 470), ("TEMPLE", 690)):
        glowing_text(img, word, big, cy)

    # The temple, glowing like the border.
    front, (f_top, f_bottom) = temple(W, H, 900, 1850)
    img.alpha_composite(front.filter(ImageFilter.GaussianBlur(16)))
    img.alpha_composite(front)

    # The frieze: the five, dark on gold would vanish, so cream on the card between the bands.
    small = font(31, ("arialbd.ttf", "ariblk.ttf"))
    names = "FORTRAN · COBOL · BASIC · JAVA · PYTHON"
    dd = ImageDraw.Draw(img)
    box = dd.textbbox((0, 0), names, font=small)
    tx = (W - (box[2] - box[0])) // 2 - box[0]
    ty = (f_top + f_bottom) // 2 - (box[3] - box[1]) // 2 - box[1]
    dd.text((tx, ty), names, font=small, fill=CREAM)

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    img.save(OUT)
    print("wrote", os.path.normpath(OUT), img.size)


if __name__ == "__main__":
    write_sign()
