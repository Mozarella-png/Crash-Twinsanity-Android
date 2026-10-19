#include "glyphs.h"

#include "hardware.h"
#include "renderthread.h"
#include "gs/gs.h"

#include "game/font.h"

#include <cmath>
#include <vector>

namespace
{
struct GlyphImage
{
    u32 width = 0;
    u32 height = 0;
    // RGBA with the GS's alpha (0x80 = 1.0)
    std::vector<u32> texels;
};

GlyphImage g_Images[256];
// Changed with every image set; a font's rectangles are made again when it's older than this
u32 g_Version = 0;
u32 g_Count = 0;

struct NotedFont
{
    const Font* font;
    const Vector4* glyphs;
    u32 version;
    // The codes this font's rectangles were made for (to take them back when an image goes)
    std::vector<Gs::Gs::GlyphOverride> made;
};

std::vector<NotedFont> g_Fonts;
}

bool NativeGraphicsSetGlyphImage(u8 code, const u32* rgba, int width, int height)
{
    GlyphImage& image = g_Images[code];
    bool had = !image.texels.empty();
    image.texels.clear();
    image.width = 0;
    image.height = 0;
    if (rgba != nullptr && width > 0 && height > 0)
    {
        image.width = static_cast<u32>(width);
        image.height = static_cast<u32>(height);
        image.texels.resize(static_cast<size_t>(width) * height);
        for (size_t i = 0; i < image.texels.size(); i++)
        {
            u32 c = rgba[i];
            u32 alpha = ((c >> 24) * 128 + 127) / 255;
            image.texels[i] = (c & 0xFFFFFF) | alpha << 24;
        }
    }

    g_Count += (image.texels.empty() ? 0 : 1) - (had ? 1 : 0);
    g_Version++;
    return true;
}

namespace NativeGraphics
{
void NoteFontGlyphs(const Font* font)
{
    if (font == nullptr || font->glyphs == nullptr)
    {
        return;
    }

    NotedFont* noted = nullptr;
    for (NotedFont& known : g_Fonts)
    {
        if (known.font == font)
        {
            noted = &known;
        }
    }

    if (noted == nullptr)
    {
        if (g_Count == 0)
        {
            return;
        }

        g_Fonts.push_back({font, nullptr, ~0u, {}});
        noted = &g_Fonts.back();
    }

    if (noted->version == g_Version && noted->glyphs == font->glyphs)
    {
        return;
    }

    // The font's rectangles taken back, then made for the images there are now (given to the GS on its side, in this order)
    std::vector<Gs::Gs::GlyphOverride> changes;
    for (Gs::Gs::GlyphOverride& old : noted->made)
    {
        old.texels.clear();
        changes.push_back(old);
    }

    noted->made.clear();
    noted->version = g_Version;
    noted->glyphs = font->glyphs;
    for (u32 code = 0; code < 256; code++)
    {
        const GlyphImage& image = g_Images[code];
        s32 index = static_cast<s32>(code) - font->firstCharacter;
        if (image.texels.empty() || index < 0 || index >= font->glyphCount)
        {
            continue;
        }

        // A glyph: its first corner's U and V (sixteenths of a texel; the page in the low bits of U's float), its width and height
        // (the page is upside down: the rectangle's other corner is above)
        const Vector4& glyph = font->glyphs[index];
        Gs::Gs::GlyphOverride rectangle;
        rectangle.u0 = static_cast<s32>(std::lround(glyph.x));
        rectangle.v0 = static_cast<s32>(std::lround(glyph.y));
        rectangle.u1 = rectangle.u0 + static_cast<s32>(std::lround(glyph.z));
        rectangle.v1 = rectangle.v0 - static_cast<s32>(std::lround(glyph.w));
        rectangle.width = image.width;
        rectangle.height = image.height;
        rectangle.texels = image.texels;
        noted->made.push_back(rectangle);
        noted->made.back().texels.clear();
        changes.push_back(std::move(rectangle));
    }

    RenderCall([changes = std::move(changes)]() mutable {
        Gs::Gs& gs = *GetHardware().gs;
        for (Gs::Gs::GlyphOverride& change : changes)
        {
            gs.SetGlyphOverride(std::move(change));
        }
    });
}
}
