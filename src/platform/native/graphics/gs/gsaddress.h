#pragma once

// The GS's local memory's addresses (gsmemory.cpp's layouts): inline, since the rasteriser, the transfers and the hardware
// renderer work one out for every pixel they touch. gs.h includes it

namespace Gs
{
namespace MemoryLayout
{
// The blocks of a page: 32 blocks of 256 bytes. A 32 bit page is 8 by 4 blocks of 8 by 8 pixels, a 16 bit one 4 by 8 of 16 by 8,
// an 8 bit one 8 by 4 of 16 by 16, a 4 bit one 4 by 8 of 32 by 16. The blocks are numbered in a Z order of their x and y bits
constexpr u32 Block32(u32 bx, u32 by)
{
    return (bx & 1) | (by & 1) << 1 | (bx & 2) << 1 | (by & 2) << 2 | (bx & 4) << 2;
}

constexpr u32 Block16(u32 bx, u32 by)
{
    return (by & 1) | (bx & 1) << 1 | (by & 2) << 1 | (bx & 2) << 2 | (by & 4) << 2;
}

// PSMCT16S: the 16 bit page's blocks with the x bit 1 and y bit 2 swapped
constexpr u32 Block16S(u32 bx, u32 by)
{
    return (by & 1) | (bx & 1) << 1 | (by & 4) | (by & 2) << 2 | (bx & 2) << 3;
}

// The pixels of a block: 4 columns (each 64 bytes), a 32 bit column 8 by 2 pixels, a 16 bit one 16 by 2
constexpr u32 Column32(u32 x, u32 y)
{
    return (x & 1) | (y & 1) << 1 | (x & 6) << 1 | (y & 6) << 3;
}

constexpr u32 Column16(u32 x, u32 y)
{
    return (x & 8) >> 3 | (x & 1) << 1 | (y & 1) << 2 | (x & 2) << 2 | (x & 4) << 2 | (y & 6) << 4;
}

// The 8 and 4 bit columns (16 by 4 and 32 by 4 pixels) interleave their pixels across the column's words in a pattern that
// changes every other column: the manual's tables
constexpr u8 Column8Table[16][16] = {
    {0, 4, 16, 20, 32, 36, 48, 52, 2, 6, 18, 22, 34, 38, 50, 54},
    {8, 12, 24, 28, 40, 44, 56, 60, 10, 14, 26, 30, 42, 46, 58, 62},
    {33, 37, 49, 53, 1, 5, 17, 21, 35, 39, 51, 55, 3, 7, 19, 23},
    {41, 45, 57, 61, 9, 13, 25, 29, 43, 47, 59, 63, 11, 15, 27, 31},
    {96, 100, 112, 116, 64, 68, 80, 84, 98, 102, 114, 118, 66, 70, 82, 86},
    {104, 108, 120, 124, 72, 76, 88, 92, 106, 110, 122, 126, 74, 78, 90, 94},
    {65, 69, 81, 85, 97, 101, 113, 117, 67, 71, 83, 87, 99, 103, 115, 119},
    {73, 77, 89, 93, 105, 109, 121, 125, 75, 79, 91, 95, 107, 111, 123, 127},
    {128, 132, 144, 148, 160, 164, 176, 180, 130, 134, 146, 150, 162, 166, 178, 182},
    {136, 140, 152, 156, 168, 172, 184, 188, 138, 142, 154, 158, 170, 174, 186, 190},
    {161, 165, 177, 181, 129, 133, 145, 149, 163, 167, 179, 183, 131, 135, 147, 151},
    {169, 173, 185, 189, 137, 141, 153, 157, 171, 175, 187, 191, 139, 143, 155, 159},
    {224, 228, 240, 244, 192, 196, 208, 212, 226, 230, 242, 246, 194, 198, 210, 214},
    {232, 236, 248, 252, 200, 204, 216, 220, 234, 238, 250, 254, 202, 206, 218, 222},
    {193, 197, 209, 213, 225, 229, 241, 245, 195, 199, 211, 215, 227, 231, 243, 247},
    {201, 205, 217, 221, 233, 237, 249, 253, 203, 207, 219, 223, 235, 239, 251, 255},
};

// The 4 bit column table (a column 32 by 4 pixels, 128 nibbles): pixel i of each group of 8 across goes to nibble
// (i & 1) * 8 + (i >> 1) * 32 plus the group's 2, the second row 16 further; the third and fourth rows take the odd nibbles, and
// the halves of each group trade places in every other row pair (starting with the second pair in even columns, the first in odd
// ones)
struct Column4
{
    u16 value[16][32];
};

constexpr Column4 MakeColumn4()
{
    Column4 table = {};
    for (u32 y = 0; y < 16; y++)
    {
        u32 column = y >> 2;
        u32 row = y & 3;
        for (u32 x = 0; x < 32; x++)
        {
            u32 group = x >> 3;
            u32 i = x & 7;
            bool swapped = ((row >> 1) ^ (column & 1)) != 0;
            u32 j = swapped ? i ^ 4 : i;
            u32 nibble = (row & 1) * 16 + group * 2 + (j & 1) * 8 + (j >> 1) * 32 + (row >> 1);
            table.value[y][x] = static_cast<u16>(column * 128 + nibble);
        }
    }

    return table;
}

constexpr Column4 Column4Table = MakeColumn4();

// The 8 bit columns follow the same scheme over bytes
constexpr u32 Column8(u32 x, u32 y)
{
    return Column8Table[y & 15][x & 15];
}

constexpr u32 BlockMask = BlockCount - 1;
}

// The local memory's layouts: where a pixel of a format at (x, y) of a buffer (its base in blocks of 64 words, its width in
// 64 pixels) is, in units of the format's size (words for 32 and 24 bit, half words for 16 bit, bytes for 8 bit, nibbles for
// 4 bit; the H formats and the Z formats as their colour twins)
using namespace MemoryLayout;

inline u32 PixelAddress32(u32 x, u32 y, u32 bp, u32 bw)
{
    u32 page = (y >> 5) * bw + (x >> 6);
    u32 block = (bp + page * 32 + Block32((x >> 3) & 7, (y >> 3) & 3)) & BlockMask;
    return block << 6 | Column32(x & 7, y & 7);
}

inline u32 PixelAddress32Z(u32 x, u32 y, u32 bp, u32 bw)
{
    u32 page = (y >> 5) * bw + (x >> 6);
    u32 block = ((bp + page * 32 + Block32((x >> 3) & 7, (y >> 3) & 3)) ^ 0x18) & BlockMask;
    return block << 6 | Column32(x & 7, y & 7);
}

inline u32 PixelAddress16(u32 x, u32 y, u32 bp, u32 bw)
{
    u32 page = (y >> 6) * bw + (x >> 6);
    u32 block = (bp + page * 32 + Block16((x >> 4) & 3, (y >> 3) & 7)) & BlockMask;
    return block << 7 | Column16(x & 15, y & 7);
}

inline u32 PixelAddress16S(u32 x, u32 y, u32 bp, u32 bw)
{
    u32 page = (y >> 6) * bw + (x >> 6);
    u32 block = (bp + page * 32 + Block16S((x >> 4) & 3, (y >> 3) & 7)) & BlockMask;
    return block << 7 | Column16(x & 15, y & 7);
}

inline u32 PixelAddress16Z(u32 x, u32 y, u32 bp, u32 bw)
{
    u32 page = (y >> 6) * bw + (x >> 6);
    u32 block = ((bp + page * 32 + Block16((x >> 4) & 3, (y >> 3) & 7)) ^ 0x18) & BlockMask;
    return block << 7 | Column16(x & 15, y & 7);
}

inline u32 PixelAddress16SZ(u32 x, u32 y, u32 bp, u32 bw)
{
    u32 page = (y >> 6) * bw + (x >> 6);
    u32 block = ((bp + page * 32 + Block16S((x >> 4) & 3, (y >> 3) & 7)) ^ 0x18) & BlockMask;
    return block << 7 | Column16(x & 15, y & 7);
}

// 8 and 4 bit pages are 128 pixels wide: the buffer width (in 64 pixels) is halved for them
inline u32 PixelAddress8(u32 x, u32 y, u32 bp, u32 bw)
{
    u32 page = (y >> 6) * (bw >> 1) + (x >> 7);
    u32 block = (bp + page * 32 + Block32((x >> 4) & 7, (y >> 4) & 3)) & BlockMask;
    return block << 8 | Column8(x, y);
}

inline u32 PixelAddress4(u32 x, u32 y, u32 bp, u32 bw)
{
    u32 page = (y >> 7) * (bw >> 1) + (x >> 7);
    u32 block = (bp + page * 32 + Block16((x >> 5) & 3, (y >> 4) & 7)) & BlockMask;
    return block << 9 | Column4Table.value[y & 15][x & 31];
}
}
