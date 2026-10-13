"""Draw Freakbook's pictures: the five programmers' profile pictures and the photos they post.

Walt, 2026-10-12: "pictures are comical drawn things for now". So they are cartoons, drawn here
in code - wobbly outlines, flat colours, Comic Sans - and nobody could take the five for real
people. Every picture docs/FREAKBOOK.md lists, plus one for each want when it comes true.

Ours, so the PNGs go in SourceArt/Freakbook/ (tracked) and make_freakbook_assets.py imports them
as /Game/Freakbook/T_FB_<name>. The names are the ones FreakbookModel.cpp uses.

    python Tools/Scripts/make_freakbook_pictures.py          (all of them)
    python Tools/Scripts/make_freakbook_pictures.py --sheet  (also a contact sheet in %TEMP%)

Needs Pillow. Deterministic: the wobble comes from a seeded random per picture.
"""

import io
import math
import os
import random
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "SourceArt", "Freakbook")
FONTS = os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts")

W, H = 480, 360          # a posted photo
AV = 256                 # a profile picture

OUTLINE = (34, 30, 38)
SKIN = (246, 205, 170)
SKIN_DARK = (214, 160, 120)
PAPER = (252, 250, 240)

R = random.Random(1)
CHANGED = []             # pictures this run actually rewrote


def font(size, bold=False):
    name = "comicbd.ttf" if bold else "comic.ttf"
    path = os.path.join(FONTS, name)
    if os.path.exists(path):
        return ImageFont.truetype(path, size)
    return ImageFont.load_default()


# ---------------------------------------------------------------------------------------------
# A hand: everything wobbles a little, so nothing looks ruled.

def wobble(points, amp=1.6, step=14.0, closed=True):
    out = []
    n = len(points)
    for i in range(n if closed else n - 1):
        (x0, y0), (x1, y1) = points[i], points[(i + 1) % n]
        length = math.hypot(x1 - x0, y1 - y0)
        parts = max(1, int(length / step))
        for k in range(parts):
            t = k / parts
            out.append((x0 + (x1 - x0) * t + R.uniform(-amp, amp), y0 + (y1 - y0) * t + R.uniform(-amp, amp)))
    if not closed:
        out.append(points[-1])
    return out


def poly(d, points, fill, width=4, amp=1.6):
    pts = wobble(points, amp)
    if fill is not None:
        d.polygon(pts, fill=fill)
    if width > 0:
        d.line(pts + [pts[0]], fill=OUTLINE, width=width, joint="curve")


def rect(d, x0, y0, x1, y1, fill, width=4, amp=1.6):
    poly(d, [(x0, y0), (x1, y0), (x1, y1), (x0, y1)], fill, width, amp)


def ellipse(d, cx, cy, rx, ry, fill, width=4, amp=1.2, start=0.0, end=360.0, closed=True):
    steps = max(18, int((rx + ry) / 3))
    pts = []
    for i in range(steps + 1):
        a = math.radians(start + (end - start) * i / steps)
        pts.append((cx + math.cos(a) * rx + R.uniform(-amp, amp), cy + math.sin(a) * ry + R.uniform(-amp, amp)))
    if closed and fill is not None:
        d.polygon(pts, fill=fill)
    if width:
        d.line(pts if not closed else pts + [pts[0]], fill=OUTLINE, width=width, joint="curve")


def line(d, points, width=4, color=OUTLINE, amp=1.2):
    d.line(wobble(points, amp, closed=False), fill=color, width=width, joint="curve")


def heart(d, cx, cy, s, fill=(230, 60, 110)):
    d.ellipse([cx - s, cy - s * 0.75, cx, cy + s * 0.2], fill=fill)
    d.ellipse([cx, cy - s * 0.75, cx + s, cy + s * 0.2], fill=fill)
    d.polygon([(cx - s * 0.97, cy - s * 0.12), (cx + s * 0.97, cy - s * 0.12), (cx, cy + s * 1.0)], fill=fill)


def text(d, xy, words, size=22, bold=False, color=OUTLINE, anchor="mm"):
    d.text(xy, words, font=font(size, bold), fill=color, anchor=anchor)


def background(color, size=(W, H)):
    img = Image.new("RGB", size, color)
    return img, ImageDraw.Draw(img)


# ---------------------------------------------------------------------------------------------
# The five, as cartoons. look: hair, shirt, and what makes each one who they are.

LOOKS = {
    "dee":     dict(shirt=(150, 110, 190), hair=(205, 205, 210), style="bun", glasses=True),
    "gunther": dict(shirt=(190, 60, 55), hair=(165, 155, 150), style="horseshoe", mustache=True, check=True),
    "brent":   dict(shirt=(45, 70, 120), hair=(110, 75, 45), style="short", goatee=True, vest=True),
    "margo":   dict(shirt=(90, 170, 110), hair=(170, 90, 50), style="ponytail", hoodie=True),
    "jaxx":    dict(shirt=(240, 140, 50), hair=(60, 50, 45), style="messy", headphones=True),
}


def face(d, cx, cy, r, look, mood="smile"):
    """A head of radius r centred at (cx, cy)."""
    hair = look["hair"]
    style = look["style"]
    if style == "ponytail":
        ellipse(d, cx + r * 1.05, cy - r * 0.1, r * 0.35, r * 0.6, hair, 3)
    if style == "bun":
        ellipse(d, cx, cy - r * 1.05, r * 0.42, r * 0.34, hair, 3)
    ellipse(d, cx - r * 0.98, cy + r * 0.05, r * 0.16, r * 0.24, SKIN, 3)   # ears
    ellipse(d, cx + r * 0.98, cy + r * 0.05, r * 0.16, r * 0.24, SKIN, 3)
    ellipse(d, cx, cy, r, r * 1.04, SKIN, 4)
    if style == "bun":
        ellipse(d, cx, cy - r * 0.55, r * 0.95, r * 0.5, hair, 3, start=180, end=360)
    elif style == "horseshoe":
        ellipse(d, cx - r * 0.86, cy - r * 0.32, r * 0.16, r * 0.26, hair, 3)
        ellipse(d, cx + r * 0.86, cy - r * 0.32, r * 0.16, r * 0.26, hair, 3)
        line(d, [(cx - r * 0.3, cy - r * 0.85), (cx - r * 0.1, cy - r * 0.95)], 3)   # the last three hairs
        line(d, [(cx, cy - r * 1.0), (cx + r * 0.05, cy - r * 1.15)], 3)
    elif style == "short":
        ellipse(d, cx, cy - r * 0.6, r * 0.98, r * 0.45, hair, 3, start=180, end=360)
    elif style == "ponytail":
        ellipse(d, cx, cy - r * 0.55, r * 1.0, r * 0.55, hair, 3, start=180, end=360)
    elif style == "messy":
        pts = []
        for i in range(11):
            a = math.radians(180 + 18 * i)
            rr = r * (1.08 if i % 2 else 0.86)
            pts.append((cx + math.cos(a) * rr, cy - r * 0.15 + math.sin(a) * rr * 0.9))
        pts.append((cx + r * 0.95, cy - r * 0.25))
        pts.append((cx - r * 0.95, cy - r * 0.25))
        poly(d, pts, hair, 3)
    # Eyes.
    ey = cy - r * 0.08
    if mood == "asleep":
        line(d, [(cx - r * 0.45, ey), (cx - r * 0.2, ey + 4)], 3)
        line(d, [(cx + r * 0.2, ey + 4), (cx + r * 0.45, ey)], 3)
    else:
        for sx in (-1, 1):
            ellipse(d, cx + sx * r * 0.33, ey, r * 0.11, r * 0.14, (255, 255, 255), 2)
            d.ellipse([cx + sx * r * 0.33 - r * 0.05, ey - r * 0.05 + 2, cx + sx * r * 0.33 + r * 0.05, ey + r * 0.07], fill=OUTLINE)
    if look.get("glasses"):
        for sx in (-1, 1):
            ellipse(d, cx + sx * r * 0.33, ey, r * 0.24, r * 0.22, None, 3, closed=False)
        line(d, [(cx - r * 0.1, ey), (cx + r * 0.1, ey)], 3)
    if mood in ("sad", "tears", "pitiful"):
        for sx in (-1, 1):
            line(d, [(cx + sx * r * 0.18, ey - r * 0.32), (cx + sx * r * 0.48, ey - r * 0.24)], 3)   # worried brows
    # Mouth.
    my = cy + r * 0.45
    if mood in ("smile", "tears"):
        ellipse(d, cx, my - r * 0.12, r * 0.32, r * 0.2, None, 3, start=20, end=160, closed=False)
    elif mood == "grin":
        poly(d, [(cx - r * 0.35, my - r * 0.12), (cx + r * 0.35, my - r * 0.12), (cx, my + r * 0.18)], (255, 255, 255), 3)
    elif mood in ("sad", "pitiful"):
        ellipse(d, cx, my + r * 0.08, r * 0.26, r * 0.14, None, 3, start=200, end=340, closed=False)
    elif mood == "asleep":
        ellipse(d, cx, my, r * 0.08, r * 0.08, None, 3)
    else:
        line(d, [(cx - r * 0.2, my), (cx + r * 0.2, my)], 3)
    if look.get("mustache"):
        poly(d, [(cx - r * 0.42, my - r * 0.12), (cx, my - r * 0.28), (cx + r * 0.42, my - r * 0.12), (cx, my - r * 0.05)], look["hair"], 3)
    if look.get("goatee"):
        poly(d, [(cx - r * 0.15, my + r * 0.2), (cx + r * 0.15, my + r * 0.2), (cx, my + r * 0.5)], look["hair"], 3)
    if mood == "tears":
        for sx in (-1, 1):
            ellipse(d, cx + sx * r * 0.42, ey + r * 0.35, r * 0.07, r * 0.11, (120, 190, 255), 2)
    if look.get("headphones"):
        ellipse(d, cx, cy - r * 0.1, r * 1.12, r * 1.12, None, 6, start=190, end=350, closed=False)
        rect(d, cx - r * 1.25, cy - r * 0.15, cx - r * 0.9, cy + r * 0.4, (60, 60, 70), 3)
        rect(d, cx + r * 0.9, cy - r * 0.15, cx + r * 1.25, cy + r * 0.4, (60, 60, 70), 3)


def body(d, cx, top, w, h, look, arms="down", cape=False):
    """Shoulders and a torso under a head; arms 'down', 'up' (flexing), 'hug' or 'ear'."""
    shirt = look["shirt"]
    if cape:
        poly(d, [(cx - w * 0.45, top + 6), (cx + w * 0.45, top + 6), (cx + w * 0.9, top + h * 1.1), (cx - w * 0.9, top + h * 1.1)], (205, 35, 40), 4)
    poly(d, [(cx - w * 0.5, top + h * 0.25), (cx - w * 0.38, top), (cx + w * 0.38, top), (cx + w * 0.5, top + h * 0.25),
             (cx + w * 0.45, top + h), (cx - w * 0.45, top + h)], shirt, 4)
    if look.get("check"):
        for k in range(1, 5):
            y = top + h * k / 5.0
            line(d, [(cx - w * 0.44, y), (cx + w * 0.44, y)], 2, (150, 40, 40), 0.6)
            x = cx - w * 0.44 + w * 0.88 * k / 5.0
            line(d, [(x, top + 6), (x, top + h)], 2, (150, 40, 40), 0.6)
    if look.get("vest"):
        poly(d, [(cx - w * 0.22, top), (cx + w * 0.22, top), (cx + w * 0.1, top + h), (cx - w * 0.1, top + h)], (230, 230, 225), 3)
    if look.get("hoodie"):
        line(d, [(cx - w * 0.1, top + 4), (cx - w * 0.12, top + h * 0.45)], 3)
        line(d, [(cx + w * 0.1, top + 4), (cx + w * 0.12, top + h * 0.45)], 3)
    arm = max(7, int(w * 0.13))
    if arms == "down":
        line(d, [(cx - w * 0.45, top + h * 0.2), (cx - w * 0.55, top + h * 0.8)], arm, shirt)
        line(d, [(cx + w * 0.45, top + h * 0.2), (cx + w * 0.55, top + h * 0.8)], arm, shirt)
    elif arms == "up":
        for sx in (-1, 1):
            line(d, [(cx + sx * w * 0.45, top + h * 0.2), (cx + sx * w * 0.85, top + h * 0.15), (cx + sx * w * 0.85, top - h * 0.35)], arm, shirt)
            ellipse(d, cx + sx * w * 0.85, top - h * 0.4, arm * 0.7, arm * 0.7, SKIN, 3)
    elif arms == "hug":
        line(d, [(cx - w * 0.45, top + h * 0.25), (cx - w * 0.1, top + h * 0.55)], arm, shirt)
        line(d, [(cx + w * 0.45, top + h * 0.25), (cx + w * 0.1, top + h * 0.6)], arm, shirt)
    elif arms == "ear":
        line(d, [(cx - w * 0.45, top + h * 0.2), (cx - w * 0.55, top + h * 0.8)], arm, shirt)
        line(d, [(cx + w * 0.45, top + h * 0.2), (cx + w * 0.62, top - h * 0.05)], arm, shirt)


def person(d, who, cx, cy, r, mood="smile", arms="down", cape=False):
    look = LOOKS[who]
    body(d, cx, cy + r * 0.9, r * 2.2, r * 1.9, look, arms, cape)
    face(d, cx, cy, r, look, mood)


def caption(d, words, y=H - 22, size=20, color=OUTLINE):
    text(d, (W // 2, y), words, size, True, color)


def save(img, name):
    """Write the PNG only if it changed. Rewriting all 27 at once looks like ransomware to
    Acronis Active Protection (it flagged python.exe on 2026-10-12), and an unchanged picture
    has no business touching the disk anyway."""
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".png")
    buffer = io.BytesIO()
    img.save(buffer, format="PNG")
    data = buffer.getvalue()
    if os.path.exists(path):
        with open(path, "rb") as old:
            if old.read() == data:
                return name
    with open(path, "wb") as new:
        new.write(data)
    CHANGED.append(name)
    return name


# ---------------------------------------------------------------------------------------------
# PROFILE PICTURES

def avatar(who, bg, mood="smile"):
    R.seed(sum(ord(c) for c in who))   # not hash(): Python salts string hashes per run
    img, d = background(bg, (AV, AV))
    look = LOOKS[who]
    body(d, AV / 2, AV * 0.66, AV * 0.62, AV * 0.5, look)
    face(d, AV / 2, AV * 0.44, AV * 0.25, look, mood)
    return save(img, who + "_avatar")


def leonard_avatar():
    """Leonard never uploaded a picture: the grey default, like everyone's father."""
    R.seed(71)
    img, d = background((214, 216, 222), (AV, AV))
    grey = (160, 164, 172)
    d.ellipse([AV * 0.32, AV * 0.18, AV * 0.68, AV * 0.56], fill=grey)
    d.pieslice([AV * 0.14, AV * 0.58, AV * 0.86, AV * 1.3], 180, 360, fill=grey)
    return save(img, "leonard_avatar")


# ---------------------------------------------------------------------------------------------
# DEE - COBOL, 74

def dee_freezer():
    R.seed(101)
    img, d = background((214, 236, 248))
    text(d, (W // 2, 34), "CRYO-RETIRE 3000", 34, True, (20, 80, 150))
    text(d, (W // 2, 70), "\"Stay fresh until the batch fails.\"", 18)
    rect(d, 90, 150, 390, 300, (250, 250, 252))                      # the chest freezer
    rect(d, 80, 120, 400, 152, (236, 238, 244))                      # its lid, open
    rect(d, 120, 160, 360, 210, (190, 225, 245), 3)                  # frost inside
    ellipse(d, 240, 182, 50, 18, (255, 235, 245), 3)                 # her little pillow
    rect(d, 300, 230, 380, 280, (255, 225, 90), 3)                   # the price tag
    text(d, (340, 246), "THAW FEE", 13, True)
    text(d, (340, 266), "$1,000,000", 13, True)
    caption(d, "21 cubic feet. Pillow included.", H - 30)
    return save(img, "dee_freezer")


def dee_cat():
    R.seed(102)
    img, d = background((238, 230, 214))
    for i in range(6):                                               # fan-fold green-bar paper
        y0 = 120 + i * 30
        rect(d, 40, y0, 440, y0 + 30, (215, 240, 215) if i % 2 else PAPER, 2, 0.8)
    for x in range(52, 440, 24):
        d.ellipse([x, 124, x + 6, 130], fill=(180, 180, 170))
    text(d, (240, 105), "STMT-RUN  LISTING  PAGE 4410", 16, True)
    ellipse(d, 240, 220, 110, 52, (130, 130, 135))                   # the cat, asleep
    ellipse(d, 160, 196, 40, 36, (130, 130, 135))
    poly(d, [(132, 172), (140, 148), (154, 166)], (130, 130, 135), 3)
    poly(d, [(170, 164), (184, 146), (190, 170)], (130, 130, 135), 3)
    line(d, [(146, 196), (160, 200)], 3)
    line(d, [(166, 200), (178, 196)], 3)
    line(d, [(345, 230), (400, 260), (420, 245)], 8, (130, 130, 135))
    text(d, (330, 160), "z z z", 26, True, (90, 90, 120))
    caption(d, "She has read the listing more than the AI has.", H - 30, 18)
    return save(img, "dee_cat")


def dee_parka():
    R.seed(103)
    img, d = background((40, 46, 58))
    for x in (30, 360):                                              # server racks
        rect(d, x, 40, x + 90, 330, (70, 76, 90))
        for y in range(60, 320, 22):
            rect(d, x + 8, y, x + 82, y + 14, (52, 56, 68), 2, 0.5)
            d.ellipse([x + 70, y + 4, x + 76, y + 10], fill=(80, 230, 120) if (y // 22) % 3 else (240, 80, 70))
    look = dict(LOOKS["dee"], shirt=(230, 120, 160))                 # the parka
    body(d, 240, 190, 190, 140, look, "hug")
    ellipse(d, 240, 196, 92, 40, (245, 245, 245), 4)                 # its fur hood rim
    face(d, 240, 150, 52, look, "neutral")
    rect(d, 214, 236, 238, 296, (120, 160, 210), 3)                  # her thermos
    rect(d, 380, 14, 470, 44, (250, 250, 250), 3)
    text(d, (425, 29), "61\u00b0F", 20, True, (30, 90, 200))
    caption(d, "Parka, thermos, reading glasses.", H - 14, 18, (235, 235, 240))
    return save(img, "dee_parka")


def dee_binder():
    R.seed(104)
    img, d = background((250, 232, 238))
    person(d, "dee", 240, 120, 54, "tears", "hug")
    rect(d, 190, 200, 290, 320, (40, 90, 190))                       # the binder
    rect(d, 196, 206, 214, 314, (30, 70, 160), 2)
    text(d, (250, 240), "STMT-RUN", 14, True, (255, 255, 255))
    text(d, (250, 262), "1983", 18, True, (255, 255, 255))
    for x, y in ((80, 80), (400, 90), (90, 250), (395, 240)):
        heart(d, x, y, 18)
    caption(d, "212 pages. The coffee ring on page 40 is still there.", H - 16, 16)
    return save(img, "dee_binder")


# ---------------------------------------------------------------------------------------------
# GUNTHER - C, 58

def gunther_gate():
    R.seed(201)
    img, d = background((22, 26, 54))
    for _ in range(26):
        x, y = R.uniform(0, W), R.uniform(0, 150)
        d.ellipse([x, y, x + 3, y + 3], fill=(240, 240, 200))
    ellipse(d, 400, 60, 30, 30, (250, 245, 200))                     # moon
    rect(d, -10, 250, W + 10, H + 10, (70, 72, 80), 0)               # the lane
    line(d, [(0, 250), (W, 250)], 4)
    for x in range(20, W, 70):
        rect(d, x, 300, x + 36, 308, (230, 220, 90), 0, 0.4)
    rect(d, 70, 150, 120, 260, (230, 200, 60))                       # the ticket machine
    rect(d, 80, 165, 110, 190, (40, 50, 60), 2)
    text(d, (95, 178), "3:14", 12, True, (120, 255, 140))
    rect(d, 118, 200, 134, 260, (200, 200, 205), 3)                  # the pivot post
    arm = [(126, 205), (300, 30), (310, 40), (136, 214)]             # raised, saluting
    poly(d, arm, (250, 250, 250), 3)
    for k in range(1, 5):
        t = k / 5.0
        x, y = 126 + (300 - 126) * t, 205 + (30 - 205) * t
        poly(d, [(x, y), (x + 14, y - 14), (x + 22, y - 6), (x + 8, y + 8)], (220, 40, 40), 0)
    text(d, (340, 190), "no car.", 28, True, (240, 240, 240))
    caption(d, "Fifth Street Garage, Dayton. 3:14 a.m.", H - 18, 18, (240, 240, 240))
    return save(img, "gunther_gate")


def gunther_raft():
    R.seed(202)
    img, d = background((180, 176, 168))
    rect(d, -10, 250, W + 10, H + 10, (150, 146, 140), 0)            # garage floor
    for i in range(6):                                               # the raft: circuit boards
        x0 = 60 + i * 62
        rect(d, x0, 190 + (i % 2) * 8, x0 + 60, 250 + (i % 2) * 8, (40, 140, 70), 3)
        for j in range(3):
            d.rectangle([x0 + 10 + j * 14, 205 + (i % 2) * 8, x0 + 18 + j * 14, 213 + (i % 2) * 8], fill=(20, 20, 20))
        line(d, [(x0 + 6, 240 + (i % 2) * 8), (x0 + 54, 228 + (i % 2) * 8)], 2, (220, 200, 90), 0.5)
    line(d, [(240, 190), (240, 60)], 6, (150, 110, 60))              # the mast
    poly(d, [(244, 66), (330, 120), (244, 170)], (245, 245, 235), 3)
    text(d, (282, 120), "40%", 22, True, (200, 40, 40))
    for x in (120, 300, 360):                                        # it leaks
        ellipse(d, x, 275, 22, 6, (120, 170, 230), 2)
        ellipse(d, x + 6, 262, 3, 6, (120, 170, 230), 2)
    caption(d, "It floats if nobody is on it.", H - 22, 20)
    return save(img, "gunther_raft")


def gunther_drawing():
    R.seed(203)
    img, d = background((206, 214, 222))
    rect(d, 80, 40, 400, 260, (50, 54, 60))                          # the monitor
    rect(d, 96, 56, 384, 244, (10, 30, 20))
    for i, row in enumerate(("Segmentation fault", "(core dumped)", "$ _")):
        text(d, (110, 80 + i * 26), row, 16, False, (90, 230, 110), "lm")
    rect(d, 220, 270, 260, 300, (50, 54, 60))
    rect(d, 170, 300, 310, 316, (50, 54, 60))
    rect(d, 250, 110, 380, 230, PAPER, 3)                            # the drawing, taped on
    rect(d, 290, 102, 340, 116, (240, 230, 160), 0)
    ellipse(d, 315, 172, 44, 38, (140, 95, 55), 3)                   # a brown potato with eyes
    d.ellipse([300, 160, 306, 166], fill=OUTLINE)
    d.ellipse([322, 160, 328, 166], fill=OUTLINE)
    ellipse(d, 314, 182, 10, 5, None, 2, start=20, end=160, closed=False)
    rect(d, 20, 120, 90, 180, (255, 240, 110), 2)
    text(d, (55, 150), "MOTIV-\nATION", 12, True)
    caption(d, "Never seen a real one.", H - 22, 20)
    return save(img, "gunther_drawing")


def gunther_coconut():
    R.seed(204)
    img, d = background((250, 236, 190))
    person(d, "gunther", 220, 130, 56, "tears", "ear")
    ellipse(d, 300, 108, 34, 30, (130, 85, 50), 4)                   # the real one
    for k in range(6):
        line(d, [(280 + k * 7, 86), (284 + k * 7, 94)], 2)
    text(d, (390, 70), "slosh", 22, True, (90, 140, 210))
    text(d, (395, 100), "slosh", 18, True, (90, 140, 210))
    caption(d, "He can hear the island.", H - 22, 20)
    return save(img, "gunther_coconut")


# ---------------------------------------------------------------------------------------------
# BRENT - Java, 44

def brent_diagram():
    R.seed(301)
    img, d = background((246, 246, 244))
    rect(d, 20, 20, 460, 300, (255, 255, 255))
    boxes = []
    for i in range(16):
        x, y = R.uniform(40, 380), R.uniform(40, 250)
        boxes.append((x, y))
    for i, (x, y) in enumerate(boxes):
        for j in (i * 3 % 16, i * 7 % 16, (i + 5) % 16):
            if j != i:
                line(d, [(x + 30, y + 12), (boxes[j][0] + 30, boxes[j][1] + 12)], 2, (60, 90, 200), 0.6)
    names = ["Factory", "FactoryFactory", "Bean", "BeanImpl", "Abstract", "Manager", "Helper", "Util",
             "Proxy", "ProxyFactory", "Visitor", "Adapter", "Impl2", "ImplV2", "Base", "Service"]
    for (x, y), name in zip(boxes, names):
        rect(d, x, y, x + 64, y + 24, (255, 252, 210), 2, 0.6)
        text(d, (x + 32, y + 12), name, 10, True)
    caption(d, "AbstractClaimProcessorFactoryBeanImplV2, in context.", H - 30, 16)
    return save(img, "brent_diagram")


def brent_cape():
    R.seed(302)
    img, d = background((204, 206, 196))
    rect(d, 20, 60, 120, 330, (150, 150, 140))                       # cubicle walls
    rect(d, 360, 60, 460, 330, (150, 150, 140))
    rect(d, 380, 90, 440, 130, (255, 240, 120), 2)
    text(d, (410, 110), "TPS", 14, True)
    person(d, "brent", 240, 110, 50, "grin", "up", cape=True)
    text(d, (240, 30), "THE GARBAGE COLLECTOR", 22, True, (200, 30, 40))
    caption(d, "Dressing room. (Cubicle 4-117.)", H - 18, 18)
    return save(img, "brent_cape")


def brent_chair():
    R.seed(303)
    img, d = background((214, 220, 226))
    rect(d, 170, 80, 310, 220, (60, 60, 70))                         # the chair back
    poly(d, [(160, 90), (320, 90), (350, 280), (130, 280)], (205, 35, 40), 4)   # the cape, drying
    rect(d, 150, 220, 330, 240, (60, 60, 70))
    line(d, [(240, 240), (240, 300)], 8)
    for a in (200, 240, 280):
        line(d, [(240, 300), (a - 30 + (a - 240) * 0.6, 330)], 6)
    ellipse(d, 410, 130, 40, 40, (230, 230, 235), 4)                 # a desk fan
    line(d, [(410, 170), (410, 260)], 6)
    for k in range(3):
        line(d, [(360 - k * 8, 110 + k * 22), (330 - k * 8, 108 + k * 22)], 2, (120, 120, 140))
    caption(d, "Drying between meetings.", H - 22, 20)
    return save(img, "brent_chair")


def brent_belt():
    R.seed(304)
    img, d = background((40, 30, 50))
    for k in range(9):
        a = math.radians(-90 + (k - 4) * 16)
        line(d, [(240, 330), (240 + math.cos(a) * 400, 330 + math.sin(a) * 400)], 18, (70, 55, 85), 0)
    person(d, "brent", 240, 120, 50, "grin", "up", cape=True)
    poly(d, [(150, 70), (330, 70), (340, 100), (140, 100)], (240, 200, 60), 3)   # the belt, held high
    ellipse(d, 240, 85, 34, 26, (255, 225, 90), 3)
    text(d, (240, 86), "CHAMP", 12, True)
    caption(d, "Gold-colored. Plastic. Perfect.", H - 22, 20, (250, 240, 220))
    return save(img, "brent_belt")


# ---------------------------------------------------------------------------------------------
# MARGO - Python, 31

def dashboard(d, views):
    rect(d, 14, 14, 466, 346, (255, 255, 255), 3)
    rect(d, 14, 14, 466, 50, (60, 120, 90), 3)
    text(d, (30, 32), "Kibble Sales - Weekly", 18, True, (255, 255, 255), "lm")
    for i, h in enumerate((60, 90, 40, 120, 80, 100)):              # bars
        rect(d, 40 + i * 30, 200 - h, 62 + i * 30, 200, (90, 170, 110), 2, 0.6)
    ellipse(d, 330, 140, 54, 54, (255, 200, 90), 3)                  # the pie: mostly pie
    poly(d, [(330, 140), (384, 140), (370, 106)], (230, 120, 90), 2)
    line(d, [(40, 300), (100, 270), (160, 285), (220, 240), (280, 250), (340, 215), (440, 225)], 3, (60, 90, 200))
    if views is not None:
        rect(d, 250, 250, 450, 330, (255, 240, 240), 3)
        text(d, (350, 290), "VIEWS: %d" % views, 30, True, (200, 40, 40))


def margo_dashboard():
    R.seed(401)
    img, d = background((230, 236, 232))
    dashboard(d, 0)
    return save(img, "margo_dashboard")


def margo_dashboard_page():
    R.seed(402)
    img, d = background((230, 236, 232))
    dashboard(d, None)
    return save(img, "margo_dashboard_page")


def dog(d, cx, cy, s, coat, tie=None):
    ellipse(d, cx, cy + s * 0.9, s * 0.9, s * 0.7, coat, 3)
    ellipse(d, cx, cy, s * 0.7, s * 0.62, coat, 3)
    ellipse(d, cx - s * 0.62, cy + s * 0.1, s * 0.22, s * 0.45, (110, 75, 45), 3)
    ellipse(d, cx + s * 0.62, cy + s * 0.1, s * 0.22, s * 0.45, (110, 75, 45), 3)
    d.ellipse([cx - s * 0.3, cy - s * 0.15, cx - s * 0.16, cy - s * 0.01], fill=OUTLINE)
    d.ellipse([cx + s * 0.16, cy - s * 0.15, cx + s * 0.3, cy - s * 0.01], fill=OUTLINE)
    ellipse(d, cx, cy + s * 0.2, s * 0.14, s * 0.1, OUTLINE, 2)
    if tie:
        poly(d, [(cx - s * 0.1, cy + s * 0.55), (cx + s * 0.1, cy + s * 0.55), (cx + s * 0.14, cy + s * 1.2), (cx, cy + s * 1.35), (cx - s * 0.14, cy + s * 1.2)], tie, 2)


def margo_dogs():
    R.seed(403)
    img, d = background((236, 228, 210))
    rect(d, 60, 70, 420, 110, (255, 255, 255), 3)
    text(d, (240, 90), "RECIPE VOTE: CHICKEN   9 NO - 1 YES", 14, True)
    coats = [(210, 170, 110), (240, 240, 235), (150, 100, 60), (230, 200, 150), (90, 80, 70)]
    for i, coat in enumerate(coats):
        dog(d, 70 + i * 85, 170, 30, coat, (60, 90, 200) if i == 2 else None)
    rect(d, 20, 230, 460, 280, (160, 110, 70))                       # the boardroom table
    caption(d, "No slide deck. Just vibes.", H - 26, 20)
    return save(img, "margo_dogs")


def margo_park():
    R.seed(404)
    img, d = background((170, 215, 240))
    rect(d, -10, 220, W + 10, H + 10, (110, 180, 90), 0)
    line(d, [(0, 220), (W, 220)], 3)
    look = LOOKS["margo"]
    jeans = (70, 90, 150)
    line(d, [(230, 205), (250, 262), (300, 268)], 16, jeans)          # legs: knee down, shin back
    line(d, [(215, 210), (228, 266), (276, 272)], 16, jeans)
    line(d, [(225, 195), (130, 200)], 34, look["shirt"])               # her back, level
    line(d, [(140, 205), (128, 268)], 13, look["shirt"])               # arms straight down to the grass
    line(d, [(160, 206), (156, 268)], 13, look["shirt"])
    ellipse(d, 126, 270, 9, 6, SKIN, 2)
    ellipse(d, 156, 270, 9, 6, SKIN, 2)
    face(d, 96, 178, 32, look, "pitiful")
    dog(d, 380, 190, 34, (210, 170, 110))
    ellipse(d, 320, 288, 12, 12, (230, 240, 70), 3)
    caption(d, "\"User research.\"", H - 24, 22)
    return save(img, "margo_park")


def margo_framed():
    R.seed(405)
    img, d = background((246, 232, 210))
    rect(d, 230, 40, 450, 200, (180, 130, 60), 6)                    # the frame
    rect(d, 250, 60, 430, 180, (255, 255, 250), 3)
    text(d, (340, 105), "VIEWS:", 26, True, (200, 40, 40))
    text(d, (340, 148), "1", 46, True, (200, 40, 40))
    person(d, "margo", 120, 140, 48, "tears", "hug")
    caption(d, "Printed. Framed. Hung at eye level.", H - 22, 20)
    return save(img, "margo_framed")


# ---------------------------------------------------------------------------------------------
# JAXX - JavaScript, 24

def jaxx_banner():
    R.seed(501)
    img, d = background((240, 240, 244))
    rect(d, 10, 10, 470, 46, (30, 60, 110))
    text(d, (24, 28), "First Second National Bank", 18, True, (255, 255, 255), "lm")
    rect(d, 40, 70, 440, 340, (255, 255, 255), 5)                    # the banner, enormous
    text(d, (220, 110), "WE USE COOKIES", 32, True)
    text(d, (240, 150), "and 1,400 packages", 18)
    for i in range(3):
        ellipse(d, 130 + i * 110, 210, 34, 30, (210, 160, 90), 3)
        for k in range(4):
            d.ellipse([118 + i * 110 + k * 7, 200 + (k % 2) * 12, 124 + i * 110 + k * 7, 206 + (k % 2) * 12], fill=(90, 50, 30))
    rect(d, 330, 280, 400, 310, (60, 160, 90), 3)
    text(d, (365, 295), "Accept", 12, True, (255, 255, 255))
    ellipse(d, 404, 100, 16, 16, (240, 240, 240), 3)
    text(d, (404, 100), "X", 18, True)
    text(d, (404, 136), "everyone", 13, True, (200, 40, 40))
    text(d, (404, 152), "clicks", 13, True, (200, 40, 40))
    text(d, (404, 168), "this", 13, True, (200, 40, 40))
    return save(img, "jaxx_banner")


def jaxx_desk():
    R.seed(502)
    img, d = background((52, 48, 62))
    for x0 in (30, 290):                                             # two monitors
        rect(d, x0, 50, x0 + 160, 170, (30, 30, 36))
        rect(d, x0 + 8, 58, x0 + 152, 162, (20, 40, 60))
    text(d, (110, 90), "npm install", 14, False, (150, 220, 255))
    rect(d, 50, 110, 170, 124, (60, 70, 90), 2, 0.5)
    rect(d, 50, 110, 80, 124, (110, 220, 140), 0, 0.3)
    text(d, (110, 142), "40 min left", 12, False, (200, 200, 220))
    text(d, (370, 100), "framework 9", 14, False, (255, 160, 140))
    text(d, (370, 124), "DEPRECATED", 14, True, (255, 110, 100))
    rect(d, 0, 230, W, H, (110, 80, 60), 0)                          # the desk
    rect(d, 220, 150, 250, 230, (250, 240, 210))                     # the candle
    ellipse(d, 235, 136, 9, 16, (255, 200, 60), 2)
    ellipse(d, 235, 140, 4, 8, (255, 120, 40), 0)
    rect(d, 140, 250, 340, 330, PAPER, 3)                            # vellum, with a dragon B
    text(d, (180, 286), "B", 40, True, (190, 140, 30))
    text(d, (270, 290), "locked", 22, False)
    line(d, [(300, 200), (360, 300)], 4, (240, 240, 240))            # the quill
    poly(d, [(300, 200), (290, 160), (320, 190)], (245, 245, 245), 2)
    return save(img, "jaxx_desk")


def jaxx_robe():
    R.seed(503)
    img, d = background((230, 220, 236))
    look = dict(LOOKS["jaxx"], shirt=(120, 80, 50))
    poly(d, [(190, 170), (290, 170), (330, 340), (150, 340)], look["shirt"], 4)
    line(d, [(196, 260), (284, 260)], 6, (200, 170, 90))             # the rope belt
    face(d, 240, 120, 48, look, "smile")
    rect(d, 320, 170, 400, 230, (255, 255, 255), 3)                  # the costume-shop tag
    text(d, (360, 190), "MONK", 14, True)
    text(d, (360, 210), "$29.99", 14, True, (200, 40, 40))
    caption(d, "Trying it on. For fit.", 30, 22)
    return save(img, "jaxx_robe")


def jaxx_candle():
    R.seed(504)
    img, d = background((40, 32, 28))
    rect(d, 80, 120, 400, 300, (245, 232, 196), 4)                   # the scroll
    ellipse(d, 80, 210, 16, 90, (230, 214, 170), 3)
    ellipse(d, 400, 210, 16, 90, (230, 214, 170), 3)
    text(d, (240, 190), "LEONARD", 44, True, (214, 168, 40))
    text(d, (240, 240), "clicked Accept", 20, False, (120, 80, 40))
    for x in (40, 440):
        rect(d, x - 12, 230, x + 12, 330, (250, 240, 210))
        ellipse(d, x, 218, 8, 14, (255, 200, 60), 2)
    caption(d, "Gold leaf. Badly. But gold.", 60, 24, (250, 220, 120))
    return save(img, "jaxx_candle")


PICTURES = [
    lambda: avatar("dee", (230, 214, 246)),
    lambda: avatar("gunther", (214, 232, 214), "neutral"),
    lambda: avatar("brent", (214, 222, 240)),
    lambda: avatar("margo", (246, 230, 210), "pitiful"),
    lambda: avatar("jaxx", (250, 236, 200)),
    leonard_avatar,
    dee_freezer, dee_cat, dee_parka, dee_binder,
    gunther_gate, gunther_raft, gunther_drawing, gunther_coconut,
    brent_diagram, brent_cape, brent_chair, brent_belt,
    margo_dashboard, margo_dashboard_page, margo_dogs, margo_park, margo_framed,
    jaxx_banner, jaxx_desk, jaxx_robe, jaxx_candle,
]


def contact_sheet(names):
    cols = 5
    tw, th = 240, 180
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * tw, rows * (th + 20)), (255, 255, 255))
    d = ImageDraw.Draw(sheet)
    for i, name in enumerate(names):
        img = Image.open(os.path.join(OUT, name + ".png")).convert("RGB")
        img.thumbnail((tw, th))
        x, y = (i % cols) * tw, (i // cols) * (th + 20)
        sheet.paste(img, (x + (tw - img.width) // 2, y))
        d.text((x + tw // 2, y + th + 10), name, font=font(12), fill=(0, 0, 0), anchor="mm")
    path = os.path.join(tempfile.gettempdir(), "freakbook_sheet.png")
    sheet.save(path)
    return path


if __name__ == "__main__":
    made = [draw() for draw in PICTURES]
    print("drew %d pictures in %s; %d changed: %s" % (len(made), os.path.normpath(OUT), len(CHANGED), ", ".join(CHANGED) or "none"))
    if "--sheet" in sys.argv:
        print("sheet:", contact_sheet(made))
