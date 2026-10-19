# The native UI's fonts

The overlay (src/platform/native/ui/overlay.cpp) draws the options screen in these, as the approved design
(native/design/options/SPEC.md) has them. They're bundled with the app (its Resources/fonts, with their licences).

| File | Font | Licence | Source |
|---|---|---|---|
| LuckiestGuy-Regular.ttf | Luckiest Guy (Astigmatic) | Apache License 2.0, LICENSE-LuckiestGuy.txt | https://github.com/google/fonts/tree/main/apache/luckiestguy |
| Fredoka[wdth,wght].ttf | Fredoka (The Fredoka Project Authors), variable | SIL Open Font License 1.1, OFL-Fredoka.txt | https://github.com/google/fonts/tree/main/ofl/fredoka |
| Fredoka-SemiBold.ttf, Fredoka-Bold.ttf | Fredoka's weights 600 and 700, made from the variable font by native/tools/instance_fonts.py (fontTools' instancer; stb_truetype reads no variable fonts) | SIL Open Font License 1.1 (no Reserved Font Name), OFL-Fredoka.txt | as above |
