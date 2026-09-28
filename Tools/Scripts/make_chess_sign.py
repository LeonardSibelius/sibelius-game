"""Make the living-room door sign for chess: "CHESS" over a big knight.

Walt, 2026-09-28: "a chess portal with a chess knight image on it", beside the bowling
door. The card, glow and lettering follow make_bowling_art.py's sign. The knight is not
drawn by hand: it is a render, in profile, of the chess set's own knight mesh, read
straight from the vendor FBX - so the knight on the door is the knight on the board.

Because it is made from the vendor's model, the PNG is written beside the vendor source
in Tools/VendorSource/ChessSet/ (gitignored), and Tools/Scripts/import_chess_set.py
imports it into /Game/ChessSet with the rest of the set.

    python Tools/Scripts/make_chess_sign.py

Needs numpy and Pillow.
"""

import os
import struct
import zlib

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "..", "VendorSource", "ChessSet")
FBX = os.path.join(SOURCE, "source", "chess.fbx")
OUT = os.path.join(SOURCE, "T_Sign_Chess.png")
KNIGHT = "knight_white_low"


# ------------------------------------------------------------------------------------
# Just enough of the binary FBX format to pull one mesh's vertices and polygons out.

def read_fbx(path):
    data = open(path, "rb").read()
    wide = struct.unpack_from("<I", data, 23)[0] >= 7500

    def prop(p):
        t = chr(data[p]); p += 1
        fixed = {"Y": ("<h", 2), "C": ("<?", 1), "I": ("<i", 4), "F": ("<f", 4), "D": ("<d", 8), "L": ("<q", 8)}
        if t in fixed:
            fmt, size = fixed[t]
            return struct.unpack_from(fmt, data, p)[0], p + size
        if t in "fdlib":
            n, enc, clen = struct.unpack_from("<III", data, p); p += 12
            raw = data[p:p + clen]; p += clen
            if enc == 1:
                raw = zlib.decompress(raw)
            dt = {"f": "<f4", "d": "<f8", "l": "<i8", "i": "<i4", "b": "u1"}[t]
            return np.frombuffer(raw, dtype=dt, count=n), p
        if t in "SR":
            n = struct.unpack_from("<I", data, p)[0]; p += 4
            return data[p:p + n], p + n
        raise ValueError("unknown FBX property type " + t)

    def node(p):
        if wide:
            end, nprops, _ = struct.unpack_from("<QQQ", data, p); p += 24
        else:
            end, nprops, _ = struct.unpack_from("<III", data, p); p += 12
        nlen = data[p]; p += 1
        if end == 0:
            return None, p
        name = data[p:p + nlen].decode(); p += nlen
        props = []
        for _ in range(nprops):
            v, p = prop(p)
            props.append(v)
        kids = []
        while p < end - (25 if wide else 13):
            k, p = node(p)
            if k is None:
                break
            kids.append(k)
        return {"name": name, "props": props, "kids": kids}, end

    top, pos = [], 27
    while pos < len(data) - 200:
        n, pos = node(pos)
        if n is None:
            break
        top.append(n)
    return top


def knight_mesh():
    top = {n["name"]: n for n in read_fbx(FBX)}
    objects = top["Objects"]["kids"]
    model_id = next(o["props"][0] for o in objects
                    if o["name"] == "Model" and o["props"][1].split(b"\x00")[0].decode() == KNIGHT)
    geom_id = next(c["props"][1] for c in top["Connections"]["kids"]
                   if c["props"][0] == b"OO" and c["props"][2] == model_id
                   and any(o["name"] == "Geometry" and o["props"][0] == c["props"][1] for o in objects))
    geom = next(o for o in objects if o["name"] == "Geometry" and o["props"][0] == geom_id)
    kid = {k["name"]: k for k in geom["kids"]}
    verts = kid["Vertices"]["props"][0].reshape(-1, 3)
    faces, face = [], []
    for i in kid["PolygonVertexIndex"]["props"][0]:
        if i < 0:
            face.append(-i - 1)
            faces.append(face)
            face = []
        else:
            face.append(int(i))
    return verts, faces


# ------------------------------------------------------------------------------------

def render_knight(size_px):
    """The knight seen from its side, flat-shaded per face and painted far to near."""
    verts, faces = knight_mesh()
    # Y is up. The head points along the model's Z, so looking along X shows the profile.
    # Screen: right = -Z (the knight faces left, the way heraldry draws it), down = -Y.
    sx, sy, depth = -verts[:, 2], -verts[:, 1], verts[:, 0]
    lo_x, hi_x, lo_y, hi_y = sx.min(), sx.max(), sy.min(), sy.max()
    scale = size_px / max(hi_x - lo_x, hi_y - lo_y)
    w, h = int((hi_x - lo_x) * scale) + 4, int((hi_y - lo_y) * scale) + 4
    px = (sx - lo_x) * scale + 2
    py = (sy - lo_y) * scale + 2

    light = np.array([-0.55, 0.65, 0.52])    # from the upper left, toward the viewer
    light /= np.linalg.norm(light)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    order = sorted(range(len(faces)), key=lambda f: depth[faces[f]].mean())   # far (low X) first
    for f in order:
        idx = faces[f]
        a, b, c = verts[idx[0]], verts[idx[1]], verts[idx[2]]
        n = np.cross(b - a, c - a)
        norm = np.linalg.norm(n)
        if norm < 1e-9:
            continue
        n /= norm
        # The view looks from +X, and on screen right is -Z: map the normal into view space.
        view_n = np.array([-n[2], n[1], n[0]])
        if view_n[2] < 0:
            view_n = -view_n        # the vendor's winding is not ours to rely on
        lambert = max(0.0, float(view_n @ np.array([light[0], light[1], light[2]])))
        spec = max(0.0, lambert - 0.8) * 3.0
        tone = 0.28 + 0.72 * lambert
        colour = (int(min(255, 236 * tone + 60 * spec)), int(min(255, 214 * tone + 55 * spec)),
                  int(min(255, 176 * tone + 45 * spec)), 255)                          # boxwood ivory
        draw.polygon([(px[i], py[i]) for i in idx], fill=colour)
    return img


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
                                           outline=(255, 190, 70, 255), width=22)
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(18)))
    img.alpha_composite(glow)

    # A row of board squares along the bottom, for the knight to stand on.
    board = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    bd = ImageDraw.Draw(board)
    sq, left, top = 104, (W - 8 * 104) // 2, H - 330
    for row in range(2):
        for col in range(8):
            light_sq = (row + col) % 2 == 0
            bd.rectangle([left + col * sq, top + row * sq, left + (col + 1) * sq - 1, top + (row + 1) * sq - 1],
                         fill=(226, 196, 150, 255) if light_sq else (98, 60, 34, 255))
    img.alpha_composite(board)

    # The word.
    font = None
    for name in ("impact.ttf", "ariblk.ttf", "arialbd.ttf"):
        try:
            font = ImageFont.truetype(os.path.join(os.environ.get("WINDIR", "C:/Windows"), "Fonts", name), 230)
            break
        except OSError:
            continue
    font = font or ImageFont.load_default()
    word = "CHESS"
    text = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    td = ImageDraw.Draw(text)
    box = td.textbbox((0, 0), word, font=font)
    tx, ty = (W - (box[2] - box[0])) // 2 - box[0], 140 - box[1]
    td.text((tx, ty), word, font=font, fill=(255, 200, 90, 255))
    img.alpha_composite(text.filter(ImageFilter.GaussianBlur(14)))
    ImageDraw.Draw(img).text((tx, ty), word, font=font, fill=(255, 246, 225, 255))

    # The knight, big, standing on the squares, with a warm halo.
    knight = render_knight(1320)
    kx = (W - knight.width) // 2
    ky = top - knight.height + 30
    halo = Image.new("RGBA", (W, H), (255, 180, 80, 0))
    halo_mask = Image.new("L", (W, H), 0)
    halo_mask.paste(knight.getchannel("A"), (kx, ky))
    halo.putalpha(halo_mask.filter(ImageFilter.GaussianBlur(30)))
    img.alpha_composite(halo)
    img.alpha_composite(knight, (kx, ky))

    img.save(OUT)
    return OUT, knight.size


if __name__ == "__main__":
    path, size = write_sign()
    print("sign:", os.path.normpath(path), "knight %d x %d px" % size)
