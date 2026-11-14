"""Make the living-room banner that advertises the wrestling in the AI city.

Walt, 2026-11-13: "I want advertising for Wrestling in AI City on a banner in the living room."

A real picture from the game - Isla and Aisling on their knees in the opening clinch, shot
side-on in the wrestling lab (SourceArt/Wrestling/wrestling_clinch.png) - on the left, and the
house signs' look on the right: the ebony card, the glowing gold border, the lettering of
make_chess_sign.py / make_basketball_sign.py, so it reads as one of the house's own.

Ours, so the PNG goes in SourceArt/Wrestling/ (tracked) and place_wrestling_banner.py imports it.

    python Tools/Scripts/make_wrestling_banner.py

Needs Pillow.
"""

import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.path.join(HERE, "..", "..", "SourceArt", "Wrestling")
PHOTO = os.path.join(ART, "wrestling_clinch.png")
OUT = os.path.join(ART, "T_Sign_Wrestling.png")

GOLD = (255, 190, 70, 255)
CREAM = (255, 246, 225, 255)
MAT_BLUE = (70, 130, 230, 255)


def font_of(size, names=("impact.ttf", "ariblk.ttf", "arialbd.ttf")):
    for name in names:
        try:
            return ImageFont.truetype(os.path.join(os.environ.get("WINDIR", "C:/Windows"), "Fonts", name), size)
        except OSError:
            continue
    return ImageFont.load_default()


def fit(draw, text, names, size, width):
    """The largest size up to `size` at which `text` fits in `width`."""
    while size > 20:
        font = font_of(size, names)
        box = draw.textbbox((0, 0), text, font=font)
        if box[2] - box[0] <= width:
            return font, box
        size -= 4
    font = font_of(size, names)
    return font, draw.textbbox((0, 0), text, font=font)


def glow_text(img, xy, text, font, fill, glow, blur):
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(layer).text(xy, text, font=font, fill=glow)
    img.alpha_composite(layer.filter(ImageFilter.GaussianBlur(blur)))
    ImageDraw.Draw(img).text(xy, text, font=font, fill=fill)


def write_banner():
    W, H = 2048, 1024
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    # The card: ebony warming toward the bottom, rounded, as the house signs are.
    m, rad = 20, 70
    card = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(card)
    for y in range(m, H - m):
        t = (y - m) / (H - 2 * m)
        d.line([(m, y), (W - m, y)], fill=(int(18 + 22 * t), int(14 + 12 * t), int(22 + 6 * t), 255))
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).rounded_rectangle([m, m, W - m, H - m], radius=rad, fill=255)
    img.paste(card, (0, 0), mask)

    # The gold border, glowing.
    border = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(border).rounded_rectangle([m + 26, m + 26, W - m - 26, H - m - 26], radius=rad - 16, outline=GOLD, width=18)
    img.alpha_composite(border.filter(ImageFilter.GaussianBlur(16)))
    img.alpha_composite(border)

    # The picture: the clinch, in a blue frame like the mat's, on the left.
    inset = 90
    ph = H - 2 * inset
    photo = Image.open(PHOTO).convert("RGBA")
    pw = int(photo.width * ph / photo.height)
    photo = photo.resize((pw, ph), Image.LANCZOS)
    pmask = Image.new("L", (pw, ph), 0)
    ImageDraw.Draw(pmask).rounded_rectangle([0, 0, pw - 1, ph - 1], radius=36, fill=255)
    frame = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(frame).rounded_rectangle([inset - 10, inset - 10, inset + pw + 10, inset + ph + 10], radius=44, fill=MAT_BLUE)
    img.alpha_composite(frame.filter(ImageFilter.GaussianBlur(10)))
    img.alpha_composite(frame)
    img.paste(photo, (inset, inset), pmask)

    # The words, in the space to the right of the picture.
    left = inset + pw + 70
    width = W - left - 110
    draw = ImageDraw.Draw(img)
    y = 150
    font, box = fit(draw, "WRESTLING", ("impact.ttf", "ariblk.ttf"), 260, width)
    glow_text(img, (left - box[0], y - box[1]), "WRESTLING", font, CREAM, (255, 200, 90, 255), 14)
    y += box[3] - box[1] + 46

    font, box = fit(draw, "IN THE AI CITY", ("impact.ttf", "ariblk.ttf"), 150, width)
    glow_text(img, (left - box[0], y - box[1]), "IN THE AI CITY", font, GOLD, (255, 170, 60, 200), 10)
    y += box[3] - box[1] + 70

    small = ("arialbd.ttf", "segoeuib.ttf")
    for line in ("Two agents. One mat.", "Best of three falls."):
        font, box = fit(draw, line, small, 64, width)
        draw.text((left - box[0], y - box[1]), line, font=font, fill=CREAM)
        y += box[3] - box[1] + 26
    y += 40
    for line in ("Upstairs, at the end of the hall:", "through the AI City portal [C]"):
        font, box = fit(draw, line, small, 50, width)
        draw.text((left - box[0], y - box[1]), line, font=font, fill=(200, 210, 235, 255))
        y += box[3] - box[1] + 20

    os.makedirs(ART, exist_ok=True)
    img.save(OUT)
    return OUT


if __name__ == "__main__":
    print("banner:", os.path.normpath(write_banner()))
