#pragma once

// The UI's glyph images: a code of the game's fonts drawn from an RGBA image (red in the low byte, top row first) in the quad the
// PS2's glyph gets, through the same drawing (its colour, blend and filtering). nullptr gives the font's own glyph back. True
// when it's supported (it is)
#include "common.h"

bool NativeGraphicsSetGlyphImage(u8 code, const u32* rgba, int width, int height);

class Font;

namespace NativeGraphics
{
// A font's texts are about to be drawn: its glyph rectangles of the codes with images made the GS's glyph images
void NoteFontGlyphs(const Font* font);
}
