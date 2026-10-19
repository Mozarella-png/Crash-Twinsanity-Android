"""Makes the static Fredoka weights the native UI's overlay reads (stb_truetype has no variable fonts) from the bundled variable
font: Fredoka-SemiBold.ttf (wght 600) and Fredoka-Bold.ttf (wght 700) next to it, at the default width. Needs fontTools.

    python3 native/tools/instance_fonts.py
"""
from pathlib import Path

from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

FONTS = Path(__file__).resolve().parents[1] / "third_party" / "fonts"
for weight, name in ((600, "SemiBold"), (700, "Bold")):
    font = TTFont(FONTS / "Fredoka[wdth,wght].ttf")
    static = instancer.instantiateVariableFont(font, {"wght": weight, "wdth": 100}, updateFontNames=False)
    static.save(FONTS / f"Fredoka-{name}.ttf")
    print(f"Fredoka-{name}.ttf")
