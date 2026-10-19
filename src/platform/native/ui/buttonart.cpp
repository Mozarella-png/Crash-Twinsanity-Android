// The pads' buttons drawn for the game's own texts (their glyphs' images, prompts.cpp) when there's no Xbox disc to read the Xbox's
// glyphs from (xboxart.cpp), in the style of the game's PlayStation glyphs: a face button is a dark grey disc with a black edge and
// a lighter rim, its letter in the pad's colour (the Xbox's A green, B red, X blue, Y yellow; the Switch's light grey) edged in
// black; a shoulder or a trigger is the game's grey bent key with its name set in darker grey. The letters are the settings
// screen's font (Fredoka SemiBold, through stb_truetype's distance fields). Drawn once each at four times the font's glyph cells
// (a face button's 31 x 39 texels, a shoulder's 37 x 39: the button in the cell's top, the room under it as the font has it), 4 x 4
// samples a pixel
#include "ui/bindings.h"
#include "ui/overlayinternal.h"

#include "../../../../native/third_party/stb/stb_truetype.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace NativeUi
{
namespace
{
constexpr s32 Samples = 4;
// The images: a texel of the font's cells four pixels
constexpr s32 FaceWidth = 124;
constexpr s32 ShoulderWidth = 148;
constexpr s32 Height = 156;
// The distance fields' reach (in samples) either side of a letter's edge
constexpr s32 FieldPadding = 16;
constexpr f32 FieldScale = 7.5f;

// RGBA, red the low byte
constexpr u32 Rgb(u32 hex)
{
    return 0xFF000000u | (hex >> 16 & 0xFF) | (hex & 0xFF00) | (hex & 0xFF) << 16;
}

u32 Mix(u32 a, u32 b, f32 t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    u32 out = 0xFF000000u;
    for (u32 shift = 0; shift < 24; shift += 8)
    {
        f32 value = static_cast<f32>(a >> shift & 0xFF) * (1.0f - t) + static_cast<f32>(b >> shift & 0xFF) * t;
        out |= static_cast<u32>(value + 0.5f) << shift;
    }

    return out;
}

// A text's signed distance (in samples, positive inside its letters) at each sample of an image
struct TextField
{
    s32 width = 0;
    s32 height = 0;
    std::vector<f32> distance;

    f32 At(f32 x, f32 y) const
    {
        s32 sx = static_cast<s32>(x * Samples);
        s32 sy = static_cast<s32>(y * Samples);
        if (sx < 0 || sy < 0 || sx >= width || sy >= height)
        {
            return -1000.0f;
        }

        return distance[static_cast<size_t>(sy) * width + sx];
    }
};

// The text in the settings' font, its capitals `cap` pixels tall, centred on (x, y) in an image of the size
bool LayText(const char* text, f32 cap, s32 imageWidth, s32 imageHeight, f32 x, f32 y, TextField* field)
{
    const stbtt_fontinfo* info = OverlayFontInfo(OverlayFont::Body);
    if (info == nullptr)
    {
        return false;
    }

    s32 x0 = 0;
    s32 y0 = 0;
    s32 x1 = 0;
    s32 y1 = 0;
    if (stbtt_GetCodepointBox(info, 'H', &x0, &y0, &x1, &y1) == 0 || y1 <= 0)
    {
        return false;
    }

    // In samples: the pen's places and the ink's extent along the line
    f32 scale = cap * Samples / static_cast<f32>(y1);
    std::vector<f32> pens;
    f32 pen = 0.0f;
    f32 inkLeft = 1e9f;
    f32 inkRight = -1e9f;
    for (const char* at = text; *at != '\0'; at++)
    {
        if (at != text)
        {
            pen += static_cast<f32>(stbtt_GetCodepointKernAdvance(info, at[-1], *at)) * scale;
        }

        pens.push_back(pen);
        stbtt_GetCodepointBox(info, *at, &x0, &y0, &x1, &y1);
        inkLeft = std::min(inkLeft, pen + static_cast<f32>(x0) * scale);
        inkRight = std::max(inkRight, pen + static_cast<f32>(x1) * scale);
        s32 advance = 0;
        s32 bearing = 0;
        stbtt_GetCodepointHMetrics(info, *at, &advance, &bearing);
        pen += static_cast<f32>(advance) * scale;
    }

    field->width = imageWidth * Samples;
    field->height = imageHeight * Samples;
    field->distance.assign(static_cast<size_t>(field->width) * field->height, -1000.0f);
    f32 left = x * Samples - (inkLeft + inkRight) * 0.5f;
    f32 baseline = y * Samples + cap * Samples * 0.5f;
    for (size_t i = 0; i < pens.size(); i++)
    {
        s32 width = 0;
        s32 height = 0;
        s32 offsetX = 0;
        s32 offsetY = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(info, scale, text[i], FieldPadding, 128, FieldScale, &width, &height, &offsetX,
                                                   &offsetY);
        if (sdf == nullptr)
        {
            continue;
        }

        s32 originX = static_cast<s32>(std::lround(left + pens[i])) + offsetX;
        s32 originY = static_cast<s32>(std::lround(baseline)) + offsetY;
        for (s32 row = 0; row < height; row++)
        {
            for (s32 column = 0; column < width; column++)
            {
                s32 fx = originX + column;
                s32 fy = originY + row;
                if (fx < 0 || fy < 0 || fx >= field->width || fy >= field->height)
                {
                    continue;
                }

                f32 distance = (static_cast<f32>(sdf[row * width + column]) - 128.0f) / FieldScale;
                f32& kept = field->distance[static_cast<size_t>(fy) * field->width + fx];
                kept = std::max(kept, distance);
            }
        }

        stbtt_FreeSDF(sdf, nullptr);
    }

    return true;
}

// Inside a rounded rectangle (centre, half sizes, radius)
bool InRounded(f32 x, f32 y, f32 cx, f32 cy, f32 hw, f32 hh, f32 radius)
{
    f32 qx = std::fabs(x - cx) - (hw - radius);
    f32 qy = std::fabs(y - cy) - (hh - radius);
    f32 outside = std::sqrt(std::max(qx, 0.0f) * std::max(qx, 0.0f) + std::max(qy, 0.0f) * std::max(qy, 0.0f));
    return outside + std::min(std::max(qx, qy), 0.0f) - radius <= 0.0f;
}

// The game's glyphs' greys and black
constexpr u32 Edge = Rgb(0x121212);
constexpr u32 RimTop = Rgb(0x6c6c6c);
constexpr u32 RimBottom = Rgb(0x2c2c2c);
constexpr u32 FaceTop = Rgb(0x303032);
constexpr u32 FaceBottom = Rgb(0x161618);
constexpr u32 KeyBevel = Rgb(0x707070);
constexpr u32 KeyTop = Rgb(0x565656);
constexpr u32 KeyBottom = Rgb(0x262626);
constexpr u32 KeyInk = Rgb(0x1a1a1a);
constexpr u32 KeyInkLight = Rgb(0x7a7a7a);

// A face button's sample: the disc in the cell's top, its letter
u32 FaceSample(f32 x, f32 y, const TextField& text, u32 letterTop, u32 letterBottom)
{
    constexpr f32 CentreX = FaceWidth * 0.5f;
    constexpr f32 CentreY = 66.0f;
    constexpr f32 Radius = 61.0f;
    f32 r = std::hypot(x - CentreX, y - CentreY);
    if (r > Radius)
    {
        return 0;
    }

    // The letter thinner than the font's (its strokes' outer 2 pixels its black edge), so a B's holes stay open small
    f32 shade = (y - (CentreY - Radius)) / (Radius * 2.0f);
    f32 distance = text.At(x, y);
    if (distance >= 2.0f * Samples)
    {
        return Mix(letterTop, letterBottom, shade);
    }

    if (distance >= 0.0f)
    {
        return Edge;
    }

    if (r > Radius - 4.0f)
    {
        return Edge;
    }

    if (r > Radius - 10.0f)
    {
        return Mix(RimTop, RimBottom, shade);
    }

    return Mix(FaceTop, FaceBottom, shade);
}

// A shoulder's sample: the bent key (its ends lower than its middle, its bottom more than its top) in the cell's top, its name set
// in
u32 ShoulderSample(f32 x, f32 y, const TextField& text)
{
    constexpr f32 CentreX = ShoulderWidth * 0.5f;
    constexpr f32 CentreY = 64.0f;
    constexpr f32 HalfWidth = 66.0f;
    constexpr f32 HalfHeight = 42.0f;
    constexpr f32 BendTop = 6.0f;
    constexpr f32 BendBottom = 18.0f;
    f32 across = (x - CentreX) / HalfWidth;
    f32 top = CentreY - HalfHeight + BendTop * across * across;
    f32 bottom = CentreY + HalfHeight + BendBottom * across * across;
    f32 bentY = CentreY - HalfHeight + (y - top) / (bottom - top) * HalfHeight * 2.0f;
    if (!InRounded(x, bentY, CentreX, CentreY, HalfWidth, HalfHeight, 22.0f))
    {
        return 0;
    }

    if (!InRounded(x, bentY, CentreX, CentreY, HalfWidth - 4.0f, HalfHeight - 4.0f, 18.0f))
    {
        return Edge;
    }

    f32 shade = (bentY - (CentreY - HalfHeight)) / (HalfHeight * 2.0f);
    // The name set in: dark, a light edge under it
    if (text.At(x, y) >= 0.0f)
    {
        return KeyInk;
    }

    if (text.At(x, y - 3.0f) >= 0.0f)
    {
        return Mix(KeyInkLight, KeyBottom, shade * 0.5f);
    }

    // A bevel along the top
    if (!InRounded(x, bentY, CentreX, CentreY + 6.0f, HalfWidth - 8.0f, HalfHeight - 4.0f, 16.0f))
    {
        return Mix(KeyBevel, KeyTop, shade * 4.0f);
    }

    // And a lighter lip along the bottom
    if (bentY > CentreY + HalfHeight - 10.0f)
    {
        return Mix(KeyBottom, KeyBevel, 0.45f);
    }

    return Mix(KeyTop, KeyBottom, shade);
}

struct Drawn
{
    bool made = false;
    ButtonArt art;
};
Drawn g_Drawn[2][ButtonArtLabels];

bool Draw(PromptStyle style, ButtonArtLabel label, ButtonArt* art)
{
    bool xbox = style == PromptXbox;
    bool face = label <= ArtY;
    static const char* const XboxNames[ButtonArtLabels] = {"A", "B", "X", "Y", "LB", "RB", "LT", "RT"};
    static const char* const SwitchNames[ButtonArtLabels] = {"A", "B", "X", "Y", "L", "R", "ZL", "ZR"};
    // The letters' colours, lighter at the top: the Xbox's green, red, blue and yellow; the Switch's light grey
    constexpr u32 XboxColours[4][2] = {{Rgb(0x6ec846), Rgb(0x288c1e)},
                                       {Rgb(0xf05a50), Rgb(0xb41e19)},
                                       {Rgb(0x64a0f0), Rgb(0x285abe)},
                                       {Rgb(0xfad250), Rgb(0xc89600)}};
    constexpr u32 SwitchColours[2] = {Rgb(0xe8e8e8), Rgb(0xa0a0a0)};
    const char* name = (xbox ? XboxNames : SwitchNames)[label];
    s32 width = face ? FaceWidth : ShoulderWidth;
    TextField text;
    f32 cap = face ? 62.0f : (std::strlen(name) > 1 ? 40.0f : 44.0f);
    if (!LayText(name, cap, width, Height, width * 0.5f, face ? 66.0f : 64.0f, &text))
    {
        return false;
    }

    u32 top = xbox ? XboxColours[face ? label : 0][0] : SwitchColours[0];
    u32 bottom = xbox ? XboxColours[face ? label : 0][1] : SwitchColours[1];
    art->width = width;
    art->height = Height;
    art->pixels.assign(static_cast<size_t>(width) * Height, 0);
    for (s32 py = 0; py < Height; py++)
    {
        for (s32 px = 0; px < width; px++)
        {
            f32 r = 0.0f;
            f32 g = 0.0f;
            f32 b = 0.0f;
            f32 a = 0.0f;
            for (s32 sy = 0; sy < Samples; sy++)
            {
                for (s32 sx = 0; sx < Samples; sx++)
                {
                    f32 x = static_cast<f32>(px) + (static_cast<f32>(sx) + 0.5f) / Samples;
                    f32 y = static_cast<f32>(py) + (static_cast<f32>(sy) + 0.5f) / Samples;
                    u32 colour = face ? FaceSample(x, y, text, top, bottom) : ShoulderSample(x, y, text);
                    if (colour != 0)
                    {
                        r += static_cast<f32>(colour & 0xFF);
                        g += static_cast<f32>((colour >> 8) & 0xFF);
                        b += static_cast<f32>((colour >> 16) & 0xFF);
                        a += 1.0f;
                    }
                }
            }

            if (a == 0.0f)
            {
                continue;
            }

            // Straight alpha: the colour the covered samples' average
            u32 alpha = static_cast<u32>(a / (Samples * Samples) * 255.0f + 0.5f);
            u32 red = static_cast<u32>(r / a + 0.5f);
            u32 green = static_cast<u32>(g / a + 0.5f);
            u32 blue = static_cast<u32>(b / a + 0.5f);
            art->pixels[static_cast<size_t>(py) * width + px] = red | (green << 8) | (blue << 16) | (alpha << 24);
        }
    }

    return true;
}
}

const ButtonArt* DrawnButtonArt(PromptStyle style, ButtonArtLabel label)
{
    if ((style != PromptXbox && style != PromptSwitch) || label >= ButtonArtLabels)
    {
        return nullptr;
    }

    Drawn& drawn = g_Drawn[style == PromptXbox ? 0 : 1][label];
    if (!drawn.made)
    {
        drawn.made = true;
        if (!Draw(style, label, &drawn.art))
        {
            drawn.art = {};
        }
    }

    return drawn.art.pixels.empty() ? nullptr : &drawn.art;
}

const ButtonArt* GlyphButtonArt(PromptStyle style, ButtonArtLabel label)
{
    // The disc's glyphs whole (its shoulders have none: they stay labels), else the drawn ones
    return HaveButtonArt() ? GetButtonArt(style, label) : DrawnButtonArt(style, label);
}
}
