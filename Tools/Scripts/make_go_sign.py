"""Make the living-room door sign for Go: "GO" over a 9x9 board with a game on it.

Walt, 2026-10-10: "I like the park. please make a door to the park, like the chess door."
The card, the gold border and its glow, and the lettering are make_chess_sign.py's, so the two
doors read as a pair. Where the chess sign has the set's knight, this one has the board: drawn
here, top-down, in the Go table's own colours - kaya, ink lines, slate and shell - with the
stones of the opening AGoTable::PreviewStones lays out.

Ours, so the PNG goes in SourceArt/Go/ (tracked) and make_go_assets.py imports it.

    python Tools/Scripts/make_go_sign.py

Needs Pillow.
"""

import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "SourceArt", "Go", "T_Sign_Go.png")
WOOD = os.path.join(HERE, "..", "..", "SourceArt", "Go", "T_GoWood.png")

# The preview game's stones, in play order: Black first.
MOVES = ["E5", "C4", "G4", "F6", "F5", "G6", "D6", "C6", "D7", "H5", "H4", "C7", "C8", "B8",
         "D8", "G5", "H6", "H7", "J6", "J5", "G7"]


def point(name):
    x = ord(name[0]) - ord("A") - (1 if name[0] > "I" else 0)
    return x, int(name[1:]) - 1


def board_image(size):
    """The board, top-down: kaya with its grain, nine lines each way, the star points, stones."""
    margin = size * 0.08
    pitch = (size - 2 * margin) / 8.0
    if os.path.isfile(WOOD):
        # The table's grain, tinted the kaya yellow the way M_GoSurface tints it.
        grain = Image.open(WOOD).convert("RGB").resize((size, size))
        board = Image.merge("RGB", [ch.point(lambda v, k=k: int(v * k)) for ch, k in zip(grain.split(), (0.92, 0.70, 0.40))])
    else:
        board = Image.new("RGB", (size, size), (212, 160, 88))
    board = board.convert("RGBA")
    d = ImageDraw.Draw(board)
    ink = (30, 22, 16, 255)
    width = max(2, int(size * 0.004))
    for i in range(9):
        o = margin + i * pitch
        d.line([(o, margin), (o, size - margin)], fill=ink, width=width)
        d.line([(margin, o), (size - margin, o)], fill=ink, width=width)
    star = size * 0.011
    for sx, sy in ((2, 2), (6, 2), (4, 4), (2, 6), (6, 6)):
        cx, cy = margin + sx * pitch, size - margin - sy * pitch
        d.ellipse([cx - star, cy - star, cx + star, cy + star], fill=ink)

    # The stones, each with a soft shadow. No rules engine here: the sign shows the shape of
    # the game, and nothing in this opening is taken.
    placed = {}
    for i, name in enumerate(MOVES):
        placed[point(name)] = "B" if i % 2 == 0 else "W"
    radius = pitch * 0.47
    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    for (x, y), colour in placed.items():
        cx, cy = margin + x * pitch + radius * 0.10, size - margin - y * pitch + radius * 0.14
        sd.ellipse([cx - radius, cy - radius, cx + radius, cy + radius], fill=(0, 0, 0, 120))
    board.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(radius * 0.18)))
    for (x, y), colour in placed.items():
        cx, cy = margin + x * pitch, size - margin - y * pitch
        stone = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        st = ImageDraw.Draw(stone)
        base = (28, 28, 32, 255) if colour == "B" else (236, 233, 224, 255)
        st.ellipse([cx - radius, cy - radius, cx + radius, cy + radius], fill=base)
        # A highlight up and to the left, as the table's lamp-less light falls.
        hl = (120, 120, 128, 150) if colour == "B" else (255, 255, 255, 200)
        hr = radius * 0.42
        hx, hy = cx - radius * 0.32, cy - radius * 0.34
        glint = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        ImageDraw.Draw(glint).ellipse([hx - hr, hy - hr, hx + hr, hy + hr], fill=hl)
        stone.alpha_composite(glint.filter(ImageFilter.GaussianBlur(hr * 0.5)))
        mask = Image.new("L", (size, size), 0)
        ImageDraw.Draw(mask).ellipse([cx - radius, cy - radius, cx + radius, cy + radius], fill=255)
        board.paste(stone, (0, 0), mask)
    return board


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

    # The word: big, because it is two letters where chess has five.
    font = None
    for name in ("impact.ttf", "ariblk.ttf", "arialbd.ttf"):
        try:
            font = ImageFont.truetype(os.path.join(os.environ.get("WINDIR", "C:/Windows"), "Fonts", name), 420)
            break
        except OSError:
            continue
    font = font or ImageFont.load_default()
    word = "GO"
    text = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    td = ImageDraw.Draw(text)
    box = td.textbbox((0, 0), word, font=font)
    tx, ty = (W - (box[2] - box[0])) // 2 - box[0], 150 - box[1]
    td.text((tx, ty), word, font=font, fill=(255, 200, 90, 255))
    img.alpha_composite(text.filter(ImageFilter.GaussianBlur(16)))
    ImageDraw.Draw(img).text((tx, ty), word, font=font, fill=(255, 246, 225, 255))

    # The board, square, centred in the card below the word, with a warm halo behind it.
    size = 780   # inside the gold border, with room for its glow
    board = board_image(size)
    word_bottom = 150 + (box[3] - box[1])
    bx, by = (W - size) // 2, word_bottom + (H - m - 40 - word_bottom - size) // 2
    halo = Image.new("RGBA", (W, H), (255, 180, 80, 0))
    halo_mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(halo_mask).rectangle([bx, by, bx + size, by + size], fill=255)
    halo.putalpha(halo_mask.filter(ImageFilter.GaussianBlur(30)))
    img.alpha_composite(halo)
    img.alpha_composite(board, (bx, by))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    img.save(OUT)
    return OUT


if __name__ == "__main__":
    print("sign:", os.path.normpath(write_sign()))
