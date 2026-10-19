// The GS's local memory layouts (the GS User's Manual's "page", "block" and "column" arrangements of each pixel format)

#include "gs.h"
#include "backend.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace Gs
{
using namespace MemoryLayout;

u32 BitsPerPixel(u32 psm)
{
    switch (psm)
    {
    case PSMCT32:
    case PSMCT24:
    case PSMT8H:
    case PSMT4HL:
    case PSMT4HH:
    case PSMZ32:
    case PSMZ24:
        return 32;
    case PSMCT16:
    case PSMCT16S:
    case PSMZ16:
    case PSMZ16S:
        return 16;
    case PSMT8:
        return 8;
    case PSMT4:
        return 4;
    default:
        return 0;
    }
}

u32 Gs::ReadPixel(u32 psm, u32 x, u32 y, u32 bp, u32 bw) const
{
    const u32* vm32 = reinterpret_cast<const u32*>(vm_);
    const u16* vm16 = reinterpret_cast<const u16*>(vm_);
    switch (psm)
    {
    case PSMCT32:
        return vm32[PixelAddress32(x, y, bp, bw)];
    case PSMCT24:
        return vm32[PixelAddress32(x, y, bp, bw)] & 0xFFFFFF;
    case PSMCT16:
        return vm16[PixelAddress16(x, y, bp, bw)];
    case PSMCT16S:
        return vm16[PixelAddress16S(x, y, bp, bw)];
    case PSMT8:
        return vm_[PixelAddress8(x, y, bp, bw)];
    case PSMT4:
    {
        u32 address = PixelAddress4(x, y, bp, bw);
        return (vm_[address >> 1] >> ((address & 1) << 2)) & 0xF;
    }
    case PSMT8H:
        return vm32[PixelAddress32(x, y, bp, bw)] >> 24;
    case PSMT4HL:
        return (vm32[PixelAddress32(x, y, bp, bw)] >> 24) & 0xF;
    case PSMT4HH:
        return vm32[PixelAddress32(x, y, bp, bw)] >> 28;
    case PSMZ32:
        return vm32[PixelAddress32Z(x, y, bp, bw)];
    case PSMZ24:
        return vm32[PixelAddress32Z(x, y, bp, bw)] & 0xFFFFFF;
    case PSMZ16:
        return vm16[PixelAddress16Z(x, y, bp, bw)];
    case PSMZ16S:
        return vm16[PixelAddress16SZ(x, y, bp, bw)];
    default:
        return 0;
    }
}

void Gs::WritePixel(u32 psm, u32 x, u32 y, u32 bp, u32 bw, u32 value)
{
    u32* vm32 = reinterpret_cast<u32*>(vm_);
    u16* vm16 = reinterpret_cast<u16*>(vm_);
    switch (psm)
    {
    case PSMCT32:
        {
        u32 a = PixelAddress32(x, y, bp, bw);
        vm32[a] = value;
        MarkWritten(a >> 11);
    }
        break;
    case PSMCT24:
    {
        u32 a = PixelAddress32(x, y, bp, bw);
        MarkWritten(a >> 11);
        u32& word = vm32[a];
        word = (word & 0xFF000000) | (value & 0xFFFFFF);
        break;
    }
    case PSMCT16:
        {
        u32 a = PixelAddress16(x, y, bp, bw);
        vm16[a] = static_cast<u16>(value);
        MarkWritten(a >> 12);
    }
        break;
    case PSMCT16S:
        {
        u32 a = PixelAddress16S(x, y, bp, bw);
        vm16[a] = static_cast<u16>(value);
        MarkWritten(a >> 12);
    }
        break;
    case PSMT8:
        {
        u32 a = PixelAddress8(x, y, bp, bw);
        vm_[a] = static_cast<u8>(value);
        MarkWritten(a >> 13);
    }
        break;
    case PSMT4:
    {
        u32 address = PixelAddress4(x, y, bp, bw);
        MarkWritten(address >> 14);
        u8& byte = vm_[address >> 1];
        u32 shift = (address & 1) << 2;
        byte = static_cast<u8>((byte & ~(0xF << shift)) | (value & 0xF) << shift);
        break;
    }
    case PSMT8H:
    {
        u32 a = PixelAddress32(x, y, bp, bw);
        MarkWritten(a >> 11);
        u32& word = vm32[a];
        word = (word & 0x00FFFFFF) | value << 24;
        break;
    }
    case PSMT4HL:
    {
        u32 a = PixelAddress32(x, y, bp, bw);
        MarkWritten(a >> 11);
        u32& word = vm32[a];
        word = (word & 0xF0FFFFFF) | (value & 0xF) << 24;
        break;
    }
    case PSMT4HH:
    {
        u32 a = PixelAddress32(x, y, bp, bw);
        MarkWritten(a >> 11);
        u32& word = vm32[a];
        word = (word & 0x0FFFFFFF) | (value & 0xF) << 28;
        break;
    }
    case PSMZ32:
        {
        u32 a = PixelAddress32Z(x, y, bp, bw);
        vm32[a] = value;
        MarkWritten(a >> 11);
    }
        break;
    case PSMZ24:
    {
        u32 a = PixelAddress32Z(x, y, bp, bw);
        MarkWritten(a >> 11);
        u32& word = vm32[a];
        word = (word & 0xFF000000) | (value & 0xFFFFFF);
        break;
    }
    case PSMZ16:
        {
        u32 a = PixelAddress16Z(x, y, bp, bw);
        vm16[a] = static_cast<u16>(value);
        MarkWritten(a >> 12);
    }
        break;
    case PSMZ16S:
        {
        u32 a = PixelAddress16SZ(x, y, bp, bw);
        vm16[a] = static_cast<u16>(value);
        MarkWritten(a >> 12);
    }
        break;
    default:
        break;
    }
}

// The 8 KB page a pixel of a format is in
u32 PixelPage(u32 psm, u32 x, u32 y, u32 bp, u32 bw)
{
    switch (psm)
    {
    case PSMCT16:
        return PixelAddress16(x, y, bp, bw) >> 12;
    case PSMCT16S:
        return PixelAddress16S(x, y, bp, bw) >> 12;
    case PSMZ16:
        return PixelAddress16Z(x, y, bp, bw) >> 12;
    case PSMZ16S:
        return PixelAddress16SZ(x, y, bp, bw) >> 12;
    case PSMT8:
        return PixelAddress8(x, y, bp, bw) >> 13;
    case PSMT4:
        return PixelAddress4(x, y, bp, bw) >> 14;
    case PSMZ32:
    case PSMZ24:
        return PixelAddress32Z(x, y, bp, bw) >> 11;
    default:
        return PixelAddress32(x, y, bp, bw) >> 11;
    }
}

namespace
{
// The pages of a rectangle by its pixels' addresses: every block of every format is at least 8 pixels each way, so a pixel every 8
// (and the last row and column) meets them all
void PagesOfRectByPixels(u32 psm, u32 bp, u32 bw, s32 left, s32 top, s32 right, s32 bottom, PageSet& pages)
{
    auto columns = [&](s32 y) {
        for (s32 x = left;; x += 8)
        {
            s32 at = std::min(x, right);
            pages.set(PixelPage(psm, static_cast<u32>(at) & 2047, static_cast<u32>(y) & 2047, bp, bw) & (PageCount - 1));
            if (at == right)
            {
                break;
            }
        }
    };
    for (s32 y = top;; y += 8)
    {
        s32 at = std::min(y, bottom);
        columns(at);
        if (at == bottom)
        {
            break;
        }
    }
}

// A pixel's page is (bp + its page in the buffer * 32 + its block in the page) >> 5, the block below 32 (the Z formats' XOR only
// swaps blocks within a page): with bp a page's start, each page sized region of the buffer is one page, so the pages are those
// of the regions the rectangle meets, the same set as PagesOfRectByPixels gives, without working out the pixels' addresses (the
// hardware renderer asks this for every primitive's buffers). False when bp isn't a page's start or the rectangle wraps
bool PagesOfRectByRegions(u32 psm, u32 bp, u32 bw, s32 left, s32 top, s32 right, s32 bottom, PageSet& pages)
{
    if ((bp & 31) != 0 || left < 0 || top < 0 || right >= 2048 || bottom >= 2048)
    {
        return false;
    }

    u32 widthShift = 6;
    u32 heightShift = 5;
    u32 pagesPerRow = bw;
    switch (psm)
    {
    case PSMCT16:
    case PSMCT16S:
    case PSMZ16:
    case PSMZ16S:
        heightShift = 6;
        break;
    case PSMT8:
        widthShift = 7;
        heightShift = 6;
        pagesPerRow = bw >> 1;
        break;
    case PSMT4:
        widthShift = 7;
        heightShift = 7;
        pagesPerRow = bw >> 1;
        break;
    default:
        break;
    }

    u32 first = bp >> 5;
    for (u32 row = static_cast<u32>(top) >> heightShift; row <= static_cast<u32>(bottom) >> heightShift; row++)
    {
        for (u32 column = static_cast<u32>(left) >> widthShift; column <= static_cast<u32>(right) >> widthShift; column++)
        {
            pages.set((first + row * pagesPerRow + column) & (PageCount - 1));
        }
    }

    return true;
}

// $TWIN_GS_CHECK_PAGES: the regions' pages checked against the pixels' (random rectangles of every format once, then every call)
bool CheckPages()
{
    static const bool check = std::getenv("TWIN_GS_CHECK_PAGES") != nullptr;
    static bool tested = false;
    if (check && !tested)
    {
        tested = true;
        static const u32 formats[] = {PSMCT32, PSMCT24, PSMCT16, PSMCT16S, PSMT8, PSMT4, PSMT8H, PSMT4HL, PSMT4HH,
                                      PSMZ32,  PSMZ24,  PSMZ16,  PSMZ16S};
        u32 seed = 12345;
        auto next = [&](u32 below) {
            seed = seed * 1664525u + 1013904223u;
            return (seed >> 8) % below;
        };
        u32 mismatches = 0;
        for (u32 i = 0; i < 200000; i++)
        {
            u32 psm = formats[next(sizeof(formats) / sizeof(formats[0]))];
            u32 bp = next(512) * 32;
            u32 bw = 1 + next(32);
            s32 left = static_cast<s32>(next(1024));
            s32 top = static_cast<s32>(next(1024));
            s32 right = left + static_cast<s32>(next(i % 4 == 0 ? 1024 : 64));
            s32 bottom = top + static_cast<s32>(next(i % 4 == 0 ? 1024 : 64));
            PageSet byPixels;
            PageSet byRegions;
            PagesOfRectByPixels(psm, bp, bw, left, top, right, bottom, byPixels);
            PagesOfRectByRegions(psm, bp, bw, left, top, right, bottom, byRegions);
            if (byPixels != byRegions && mismatches++ < 8)
            {
                std::fprintf(stderr, "[native] gs pages: psm %x bp %x bw %u %d,%d-%d,%d: %zu pages by pixels, %zu by regions\n", psm, bp,
                             bw, left, top, right, bottom, byPixels.count(), byRegions.count());
            }
        }

        std::fprintf(stderr, "[native] gs pages: 200000 rectangles checked, %u mismatches\n", mismatches);
    }

    return check;
}
}

void PagesOfRect(u32 psm, u32 bp, u32 bw, s32 left, s32 top, s32 right, s32 bottom, PageSet& pages)
{
    if (right < left || bottom < top)
    {
        return;
    }

    if (CheckPages())
    {
        PageSet byPixels;
        PageSet byRegions;
        PagesOfRectByPixels(psm, bp, bw, left, top, right, bottom, byPixels);
        if (PagesOfRectByRegions(psm, bp, bw, left, top, right, bottom, byRegions) && byRegions != byPixels)
        {
            std::fprintf(stderr, "[native] gs pages mismatch: psm %x bp %x bw %u %d,%d-%d,%d\n", psm, bp, bw, left, top, right, bottom);
        }

        pages |= byPixels;
        return;
    }

    static const bool byRegions = std::getenv("TWIN_GS_PAGES_BY_PIXELS") == nullptr;
    if (!byRegions || !PagesOfRectByRegions(psm, bp, bw, left, top, right, bottom, pages))
    {
        PagesOfRectByPixels(psm, bp, bw, left, top, right, bottom, pages);
    }
}

void FullPagesOfRect(u32 psm, u32 bp, u32 bw, s32 left, s32 top, s32 right, s32 bottom, PageSet& pages)
{
    // The rectangle's whole 8x8 cells (every format's block is a whole number of them, so a cell is in one page), counted by
    // page: a page is covered when all its cells are
    u32 bits = BitsPerPixel(psm == PSMCT24 || psm == PSMZ24 ? PSMCT32 : psm);
    if (bits == 0 || right < left || bottom < top)
    {
        return;
    }

    const u32 cellsInPage = 8192 * 8 / bits / 64;
    s32 x0 = (left + 7) & ~7;
    s32 y0 = (top + 7) & ~7;
    s32 x1 = (right + 1) & ~7;
    s32 y1 = (bottom + 1) & ~7;
    if (x1 <= x0 || y1 <= y0)
    {
        return;
    }

    std::vector<u16> counts(PageCount, 0);
    for (s32 y = y0; y < y1; y += 8)
    {
        for (s32 x = x0; x < x1; x += 8)
        {
            counts[PixelPage(psm, static_cast<u32>(x) & 2047, static_cast<u32>(y) & 2047, bp, bw) & (PageCount - 1)]++;
        }
    }

    for (u32 page = 0; page < PageCount; page++)
    {
        if (counts[page] >= cellsInPage)
        {
            pages.set(page);
        }
    }
}
}
