#!/usr/bin/env python3
"""Draws the Windows program's icon, native/icon/AppIcon.ico: two spiral eyes, one bigger than the other, on an orange tile.
Our own drawing, made here from shapes alone (nothing of the game's art or the box's). Each size is drawn for itself, eight
times larger then averaged down, with less detail the smaller it is: at 64 pixels the spirals have fewer turns, from 48 down
they're a ring round a pupil, at 20 and 16 a pupil.

    .venv\\Scripts\\python.exe native\\tools\\draw_icon.py [--out native/icon/AppIcon.ico] [--preview sheet.png]

--preview also writes every size on light and dark backgrounds, at their size and enlarged, to look at.
"""
import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parents[2]
SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)
SCALE = 8

TILE_TOP, TILE_BOTTOM, TILE_EDGE = (255, 170, 60), (232, 84, 14), (170, 52, 6)
INK = (43, 16, 62)
WHITE, SHADOW = (255, 255, 255), (150, 44, 4, 150)
# (centre x, centre y, half width, half height) in the icon's width: apart, a little orange between them
EYES = ((0.305, 0.53, 0.143, 0.215), (0.68, 0.49, 0.168, 0.25))


def oval(unit, x, y, rx, ry, grow=0.0):
    return [unit(x - rx - grow), unit(y - ry - grow), unit(x + rx + grow), unit(y + ry + grow)]


def draw(size):
    """The icon at size pixels"""
    n = size * SCALE
    unit = lambda value: value * n
    # Lines at least a pixel and a quarter wide, whatever the size
    line = lambda value: max(value, 1.25 / size)
    image = Image.new("RGBA", (n, n))

    # The tile: a rounded square, orange from top to bottom, its edge darker; to the edges at the smallest sizes
    inset = 0.0 if size <= 24 else 0.035
    radius = 0.22 if size <= 24 else 0.2
    box = [unit(inset), unit(inset), unit(1 - inset) - 1, unit(1 - inset) - 1]
    mask = Image.new("L", (n, n))
    ImageDraw.Draw(mask).rounded_rectangle(box, unit(radius), fill=255)
    gradient = Image.linear_gradient("L").resize((n, n))
    tile = Image.composite(Image.new("RGBA", (n, n), TILE_BOTTOM + (255,)), Image.new("RGBA", (n, n), TILE_TOP + (255,)),
                           gradient)
    image.paste(tile, (0, 0), mask)
    pen = ImageDraw.Draw(image)
    pen.rounded_rectangle(box, unit(radius), outline=TILE_EDGE + (255,), width=round(unit(line(0.014))))

    outline = line(0.03 if size >= 48 else 0.045)
    if size >= 48:
        # The eyes' shadow on the tile, a little lower
        shadow = Image.new("RGBA", (n, n))
        for x, y, rx, ry in EYES:
            ImageDraw.Draw(shadow).ellipse(oval(unit, x, y + 0.025, rx, ry, outline / 2), fill=SHADOW)
        image.alpha_composite(shadow)
    for index, (x, y, rx, ry) in enumerate(EYES):
        pen.ellipse(oval(unit, x, y, rx, ry, outline / 2), fill=INK + (255,))
        pen.ellipse(oval(unit, x, y, rx, ry, -outline / 2), fill=WHITE + (255,))
        inner_x, inner_y = rx - outline / 2, ry - outline / 2
        if size < 64:
            # Too small for a spiral: a pupil, and from 24 pixels a ring round it. They look different ways: the small
            # eye's up and in, the big one's down and out
            px, py = (x + 0.015, y - 0.02) if index == 0 else (x + 0.012, y + 0.02)
            if size >= 24:
                ring = line(0.035)
                pen.ellipse(oval(unit, px, py, inner_x * 0.62, inner_y * 0.62), fill=INK + (255,))
                pen.ellipse(oval(unit, px, py, inner_x * 0.62, inner_y * 0.62, -ring), fill=WHITE + (255,))
            pen.ellipse(oval(unit, px, py, rx * 0.3 if size >= 24 else rx * 0.5, ry * 0.3 if size >= 24 else ry * 0.42),
                        fill=INK + (255,))
            continue
        # A spiral from the middle out, the two eyes' turning opposite ways, their ends low and outside
        turns, reach = (3.0 if size >= 128 else 2.25), 0.8
        width = inner_x * reach / turns * 0.5
        way = 1 if index == 0 else -1
        start = (0.75 * math.pi if index == 0 else 0.25 * math.pi) - way * turns * 2 * math.pi
        points = []
        steps = 600
        for step in range(steps + 1):
            t = step / steps
            angle = start + way * t * turns * 2 * math.pi
            points.append((unit(x + inner_x * reach * t * math.cos(angle)),
                           unit(y + inner_y * reach * t * math.sin(angle))))
        pen.line(points, fill=INK + (255,), width=round(unit(width)), joint="curve")
        for end in (points[0], points[-1]):
            pen.ellipse([end[0] - unit(width) / 2, end[1] - unit(width) / 2, end[0] + unit(width) / 2,
                         end[1] + unit(width) / 2], fill=INK + (255,))
    return image.resize((size, size), Image.BOX)


def preview(images, path):
    """Every size on a light and a dark background: at its size, then the small ones four times larger"""
    small = [(size, image) for size, image in zip(SIZES, images) if size <= 48]
    width = max(sum(size + 16 for size in SIZES), sum(size * 4 + 16 for size, _ in small)) + 16
    row = lambda sizes: max(sizes) + 16
    height = 16 + row(SIZES) + row([size * 4 for size, _ in small])
    sheet = Image.new("RGB", (width, 2 * height))
    for band, background in enumerate(((243, 243, 243), (32, 32, 32))):
        top = band * height
        sheet.paste(background, (0, top, width, top + height))
        x = 16
        for size, image in zip(SIZES, images):
            sheet.paste(image, (x, top + 16 + 256 - size), image)
            x += size + 16
        x = 16
        for size, image in small:
            big = image.resize((size * 4, size * 4), Image.NEAREST)
            sheet.paste(big, (x, top + 16 + row(SIZES) + 192 - size * 4), big)
            x += size * 4 + 16
    sheet.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", default=str(HERE / "native" / "icon" / "AppIcon.ico"))
    parser.add_argument("--preview", help="a picture of every size to look at")
    arguments = parser.parse_args()
    images = [draw(size) for size in SIZES]
    largest = images[-1]
    largest.save(arguments.out, format="ICO", sizes=[(size, size) for size in SIZES], append_images=images[:-1])
    if arguments.preview:
        preview(images, arguments.preview)


if __name__ == "__main__":
    main()
