"""Contact sheet of consecutive lab frames: sheet.py <shots dir> <first> <count> <step> <out.png> [cols]"""
import sys
from PIL import Image, ImageDraw

src, first, count, step, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), sys.argv[5]
cols = int(sys.argv[6]) if len(sys.argv) > 6 else 4
tiles = []
for k in range(count):
    n = first + k * step
    try:
        im = Image.open(f"{src}/Wrestle_{n:03d}.png").convert("RGB")
    except OSError:
        break
    w, h = im.size
    # the middle of the frame, where the mat is
    im = im.crop((int(w * 0.2), int(h * 0.15), int(w * 0.8), int(h * 0.9))).resize((480, 352))
    ImageDraw.Draw(im).text((6, 4), str(n), fill=(255, 255, 0))
    tiles.append(im)
rows = (len(tiles) + cols - 1) // cols
sheet = Image.new("RGB", (cols * 480, rows * 352))
for i, im in enumerate(tiles):
    sheet.paste(im, ((i % cols) * 480, (i // cols) * 352))
sheet.save(out)
print(len(tiles), "tiles ->", out)
