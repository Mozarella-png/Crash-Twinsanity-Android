#include "movie.h"

#include "hardware.h"
#include "renderthread.h"
#include "gs/backend.h"
#include "gs/gs.h"
#include "renderer/gsvalues.h"
#include "renderer/renderer.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <vector>

namespace
{
constexpr u32 BufferWidthUnit = 1u << GsWidthShift;
constexpr u32 SettingWrites = 9;
constexpr u32 StripWrites = 4;
constexpr u32 StripWidth = 32 << GsSubpixelShift;
constexpr u32 QuadwordBytes = 0x10;
// The GS registers the packet writes (gs_g_* of libgs.h)
constexpr u64 RegisterPrmodeCont = 0x1A;
constexpr u64 RegisterFrame2 = 0x4D;
constexpr u64 RegisterXyOffset2 = 0x19;
constexpr u64 RegisterZbuf2 = 0x4F;
constexpr u64 RegisterTexFlush = 0x3F;
constexpr u64 RegisterTex02 = 0x07;
constexpr u64 RegisterTest2 = 0x48;
constexpr u64 RegisterPrim = 0x00;
constexpr u64 RegisterRgbaq = 0x01;
constexpr u64 RegisterUv = 0x03;
constexpr u64 RegisterXyz2 = 0x05;

const u64 MovieAttributes = 1;
const u64 MovieFrame = std::bit_cast<u64>(GS_FRAME{.fb_width = 8, .draw_mask = 0xFF000000});
const u64 MovieDepth = DepthNotWritten | ZbufZ32Format;
const u64 MovieTexture = std::bit_cast<u64>(
    GS_TEX0{.tex_width = 10, .tex_height = 10, .tex_cc = 1, .tex_funtion = GS_TEX_DECAL, .clut_loadmode = 1});
const u64 MovieTest = std::bit_cast<u64>(GS_TEST{.ztest_enable = 1, .ztest_method = GS_ZBUFF_GEQUAL});
}

namespace
{
void UploadMoviePicture(const u8* rgba, s32 width, s32 height, s32 stride, u32 depthBufferPage)
{
    Gs::Gs& gs = *NativeGraphics::GetHardware().gs;
    u32 base = depthBufferPage << GsPageBlocksShift;
    u32 bufferWidth = (static_cast<u32>(width) + BufferWidthUnit - 1) / BufferWidthUnit;
    // The hardware renderer keeps local memory on the GPU: it's told the pages before they're written and that they were, or the
    // movie draws its old copy (black)
    Gs::Backend* backend = gs.backend();
    Gs::PageSet pages;
    if (backend != nullptr && width > 0 && height > 0)
    {
        Gs::PagesOfRect(Gs::PSMCT32, base, bufferWidth, 0, 0, width - 1, height - 1, pages);
        backend->BeforeCpuWrite(pages);
    }

    for (s32 y = 0; y < height; y++)
    {
        const u8* row = rgba + static_cast<size_t>(y) * static_cast<size_t>(stride);
        for (s32 x = 0; x < width; x++)
        {
            const u8* pixel = row + x * 4;
            u32 colour = static_cast<u32>(pixel[0]) | static_cast<u32>(pixel[1]) << 8 | static_cast<u32>(pixel[2]) << 16 | 0x80000000u;
            gs.WritePixel(Gs::PSMCT32, static_cast<u32>(x), static_cast<u32>(y), base, bufferWidth, colour);
        }
    }

    if (backend != nullptr)
    {
        for (u32 page = 0; page < Gs::PageCount; page++)
        {
            if (pages.test(page))
            {
                backend->CpuWrote(page);
            }
        }
    }
}
}

void NativeGraphicsUploadMoviePicture(const u8* rgba, s32 width, s32 height, s32 stride)
{
    if (!NativeGraphics::RenderThreadActive())
    {
        UploadMoviePicture(rgba, width, height, stride, g_DepthBufferPage);
        return;
    }

    // The picture copied for the render thread, which writes it into local memory in its place among the GS's data
    std::vector<u8> copy(static_cast<size_t>(std::max(width, 0)) * 4 * static_cast<size_t>(std::max(height, 0)));
    for (s32 y = 0; y < height; y++)
    {
        std::memcpy(copy.data() + static_cast<size_t>(y) * width * 4, rgba + static_cast<size_t>(y) * stride, static_cast<size_t>(width) * 4);
    }

    NativeGraphics::RenderCall([copy = std::move(copy), width, height, page = g_DepthBufferPage] {
        UploadMoviePicture(copy.data(), width, height, width * 4, page);
    });
}

void NativeGraphicsDrawMoviePicture(const Platform::Movie::Area& area, s32 width, s32 height, u32 skippedRows)
{
    u32 textureWidth = static_cast<u32>(width) / BufferWidthUnit;
    if (static_cast<u32>(width) % BufferWidthUnit != 0)
    {
        textureWidth++;
    }

    u32 x = static_cast<u32>(area.x) << GsSubpixelShift;
    u32 y = static_cast<u32>(area.y) << GsSubpixelShift;
    u32 areaWidth = static_cast<u32>(area.width) << GsSubpixelShift;
    u32 areaHeight = static_cast<u32>(area.height) << GsSubpixelShift;
    u32 strips = areaWidth / StripWidth;
    RenderBucket& bucket = g_FrameBuckets.buckets[BucketMovies];
    u8* packet = g_DmaChains[bucket.chain].next;
    bucket.last[1] = Address(packet);
    u32* tag = reinterpret_cast<u32*>(packet);
    u32 quadwords = strips * StripWrites + SettingWrites + 1;
    tag[0] = quadwords | CountTag;
    tag[1] = 0;
    tag[2] = VifFlushA;
    tag[3] = quadwords | VifDirect;
    u64* at = reinterpret_cast<u64*>(packet + QuadwordBytes);
    auto write = [&at](u64 low, u64 high) {
        at[0] = low;
        at[1] = high;
        at += 2;
    };

    write(AddressDataTag(strips * StripWrites + SettingWrites, GS_PRIM_SPRITE), GifAddressData);
    write(MovieAttributes, RegisterPrmodeCont);
    write(g_FrameBufferPage | MovieFrame, RegisterFrame2);
    write(0, RegisterXyOffset2);
    write(g_DepthBufferPage | MovieDepth, RegisterZbuf2);
    write(0, RegisterTexFlush);
    write((g_DepthBufferPage << GsPageBlocksShift) | static_cast<u64>(textureWidth) << GsTextureWidthShift | MovieTexture,
          RegisterTex02);
    write(MovieTest, RegisterTest2);
    write(TexturedSprite, RegisterPrim);
    write(White, RegisterRgbaq);
    if (strips != 0)
    {
        u32 step = static_cast<u32>(width) / strips << GsSubpixelShift;
        u64 top = static_cast<u64>((skippedRows << GsSubpixelShift) + GsHalfTexel) << GsUvVShift;
        u64 bottom = static_cast<u64>(((static_cast<u32>(height) - skippedRows) << GsSubpixelShift) + GsHalfTexel) << GsUvVShift;
        u64 depth = u64{GsFrontDepth} << GsXyzDepthShift;
        u32 left = GsHalfTexel;
        for (u32 strip = 0; strip < strips; strip++)
        {
            u32 screenLeft = x + strip * StripWidth;
            write(left | top, RegisterUv);
            write(screenLeft | static_cast<u64>(y) << GsXyzYShift | depth, RegisterXyz2);
            write((left + step) | bottom, RegisterUv);
            write((screenLeft + StripWidth) | static_cast<u64>(y + areaHeight) << GsXyzYShift | depth, RegisterXyz2);
            left += step;
        }
    }

    DmaChain& chain = g_DmaChains[bucket.chain];
    chain.next = reinterpret_cast<u8*>(at);
    u8* next = chain.next;
    u32* nextTag = reinterpret_cast<u32*>(next);
    bucket.last = nextTag;
    nextTag[0] = NextTag;
    nextTag[1] = 0;
    nextTag[2] = 0;
    nextTag[3] = 0;
    chain.next = next + QuadwordBytes;
}
