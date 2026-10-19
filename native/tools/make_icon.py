#!/usr/bin/env python3
"""Makes the native build's app icon from the game's own art: Crash's head as the game's HUD/save icon has it (a 64x64 texture),
on a rounded square of blue (or green). The texture is the PS2's: its colours are stored at half intensity (the GS's modulate
doubles them, 0x80 being 1), so they're doubled back here. Nothing of the game's goes into the repository: the icon is made into
build/native/icon/ from the user's own copy.

    native/tools/make_icon.py <crash head texture png> [--background blue|green] [--out build/native/icon]

Writes icon_1024.png, the Linux sizes (icon_<n>.png), Windows' AppIcon.ico and on macOS AppIcon.icns (iconutil).
"""
import argparse, math, os, shutil, subprocess, sys
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw, ImageFilter

BACKGROUNDS = {
    # (centre, edge) of a radial gradient
    "blue": ((70, 170, 255), (10, 52, 150)),
    "green": ((120, 215, 90), (22, 105, 40)),
}

def full_intensity(texture):
    """The texture as the GS draws it: RGB doubled (stored at half), alpha already 0-255"""
    r, g, b, a = texture.convert("RGBA").split()
    double = lambda channel: channel.point(lambda v: min(255, v * 2))
    return Image.merge("RGBA", (double(r), double(g), double(b), a))

def upscale(image, size):
    """Up in steps (Lanczos), the colour unpremultiplied so the edges don't darken, then sharpened a little"""
    premultiplied = Image.new("RGBA", image.size)
    premultiplied.paste(image, (0, 0))
    current = image
    while current.width < size:
        step = min(size, current.width * 2)
        current = current.resize((step, step * image.height // image.width), Image.LANCZOS)
        rgb, alpha = current.convert("RGB"), current.getchannel("A")
        rgb = rgb.filter(ImageFilter.UnsharpMask(radius=1.2, percent=60, threshold=1))
        current = Image.merge("RGBA", (*rgb.split(), alpha))
    # A clean silhouette: the alpha's soft ramp narrowed
    alpha = current.getchannel("A").point(lambda v: max(0, min(255, (v - 40) * 255 // 175)))
    alpha = alpha.filter(ImageFilter.GaussianBlur(0.6))
    current.putalpha(alpha)
    return current

def squircle_mask(size, inset, radius):
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle((inset, inset, size - inset - 1, size - inset - 1), radius=radius, fill=255)
    return mask

def background(size, colours):
    centre, edge = colours
    gradient = Image.new("RGB", (size, size))
    pixels = gradient.load()
    cx, cy, reach = size / 2, size * 0.42, size * 0.75
    for y in range(size):
        for x in range(size):
            t = min(1.0, math.hypot(x - cx, y - cy) / reach) ** 1.3
            pixels[x, y] = tuple(round(c * (1 - t) + e * t) for c, e in zip(centre, edge))
    return gradient

def key_background(image):
    """A texture without alpha (drawn opaque over a plain background): the background's colour made transparent from the
    edges in, as far as it reaches (a flood fill, so the same colour inside the drawing stays), its edge softened"""
    marked = image.convert("RGB")
    marker = (255, 0, 255)
    width, height = marked.size
    for x, y in ((0, 0), (width - 1, 0), (0, height - 1), (width - 1, height - 1)):
        if marked.getpixel((x, y)) != marker:
            ImageDraw.floodfill(marked, (x, y), marker, thresh=40)
    mask = Image.eval(ImageChops.difference(marked, Image.new("RGB", marked.size, marker)).convert("L"),
                      lambda v: 0 if v == 0 else 255)
    keyed = image.copy()
    keyed.putalpha(mask.filter(ImageFilter.GaussianBlur(0.5)))
    return keyed

def make(texture_path, colour, out, flip=False):
    size = 1024
    # Apple's grid: the shape is 824 of 1024, corners 185
    inset, radius = 100, 185
    head = full_intensity(Image.open(texture_path))
    if flip:
        # A texture as the GS holds it (the native build's dumps, TWIN_HW_DUMP_TEXTURES): upside down
        head = head.transpose(Image.FLIP_TOP_BOTTOM)
    if head.getchannel("A").getextrema() == (255, 255):
        head = key_background(head)
    head = head.crop(head.getchannel("A").getbbox())
    target = 700
    head = upscale(head, target)

    icon = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    plate = background(size, BACKGROUNDS[colour]).convert("RGBA")
    plate.putalpha(squircle_mask(size, inset, radius))
    # A soft shadow under the plate, as macOS icons have
    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    shadow.putalpha(squircle_mask(size, inset, radius).point(lambda v: v * 70 // 255))
    icon.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(14)), (0, 10))
    icon.alpha_composite(plate)
    # The head, with a shadow of its own, centred a little high
    x, y = (size - head.width) // 2, (size - head.height) // 2 - 10
    head_shadow = Image.new("RGBA", head.size, (0, 0, 20, 0))
    head_shadow.putalpha(head.getchannel("A").point(lambda v: v * 110 // 255))
    icon.alpha_composite(head_shadow.filter(ImageFilter.GaussianBlur(12)), (x + 6, y + 16))
    icon.alpha_composite(head, (x, y))
    # Nothing outside the plate
    icon.putalpha(ImageChops.multiply(icon.getchannel("A"), squircle_mask(size, inset - 30, radius + 30)))

    out.mkdir(parents=True, exist_ok=True)
    icon.save(out / "icon_1024.png")
    for n in (512, 256, 128, 64, 48, 32, 16):
        icon.resize((n, n), Image.LANCZOS).save(out / f"icon_{n}.png")
    # Windows' icon (native/CMakeLists.txt puts it in the program's resources): every size Explorer asks for, in one file
    icon.save(out / "AppIcon.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    if sys.platform == "darwin" and shutil.which("iconutil"):
        iconset = out / "AppIcon.iconset"
        iconset.mkdir(exist_ok=True)
        for n in (16, 32, 128, 256, 512):
            icon.resize((n, n), Image.LANCZOS).save(iconset / f"icon_{n}x{n}.png")
            icon.resize((n * 2, n * 2), Image.LANCZOS).save(iconset / f"icon_{n}x{n}@2x.png")
        subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(out / "AppIcon.icns")], check=True)
        shutil.rmtree(iconset)
    return out / "icon_1024.png"

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("texture")
    parser.add_argument("--background", choices=BACKGROUNDS, default="blue")
    parser.add_argument("--out", default="build/native/icon")
    parser.add_argument("--flip", action="store_true", help="the texture is upside down (a dump of the GS's)")
    args = parser.parse_args()
    print(make(args.texture, args.background, Path(args.out), args.flip))
