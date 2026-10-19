// src/platform/ps2/collisionmaths.cpp natively: the collision's and the physics probes' VU0 half, its asm an instruction at a time
// on the native VU0 (results handed on in VU0's registers where the PS2's are), the EE's 128 bit moves as mmi.h's

#include "game/collision.h"
#include "game/physics.h"

#include "mmi.h"
#include "vu0.h"

using namespace NativeMath;

namespace
{
const u8* Bytes(const void* address)
{
    return static_cast<const u8*>(address);
}

// SW of a register's low word (after QMFC2)
void StoreWord(f32* to, const Quad& quad)
{
    std::memcpy(to, &quad.w[0], sizeof(u32));
}

Quad Move(Vu0& vu, u32 fs)
{
    Quad quad;
    vu.Qmfc2(fs, quad.w);
    return quad;
}
}

void StartTriangleEdgeTests(const Vector4* start, const Vector4* end, const CollisionHit* triangle, f32* values)
{
    Vu0& vu = TheVu0();
    vu.Lqc2(4, start);
    vu.Lqc2(5, end);
    vu.Lqc2(1, Bytes(triangle) + 0x0);
    vu.Lqc2(2, Bytes(triangle) + 0x10);
    vu.Lqc2(3, Bytes(triangle) + 0x20);
    vu.Sub(XYZ, 7, 5, 4);
    vu.Sub(XYZ, 10, 2, 1);
    vu.Sub(XYZ, 11, 3, 1);
    vu.Sub(XYZ, 15, 2, 1);
    vu.Sub(XYZ, 20, 3, 2);
    vu.Sub(XYZ, 25, 1, 3);
    vu.Opmula(10, 11);
    vu.Opmsub(11, 11, 10);
    vu.Opmula(7, 15);
    vu.Opmsub(16, 15, 7);
    vu.Sub(XYZ, 12, 4, 1);
    vu.Sub(XYZ, 13, 5, 1);
    vu.Opmula(7, 20);
    vu.Opmsub(21, 20, 7);
    vu.Mul(XYZ, 12, 12, 11);
    vu.Mul(XYZ, 13, 13, 11);
    vu.Opmula(7, 25);
    vu.Opmsub(26, 25, 7);
    vu.AddBc(X, 12, 12, 12, Fy);
    vu.AddBc(X, 13, 13, 13, Fy);
    vu.Sub(XYZ, 17, 1, 4);
    vu.Sub(XYZ, 18, 3, 4);
    vu.AddBc(X, 12, 12, 12, Fz);
    vu.AddBc(X, 13, 13, 13, Fz);
    vu.Sub(XYZ, 22, 2, 4);
    vu.Sub(XYZ, 23, 1, 4);
    vu.Sub(XYZ, 27, 3, 4);
    Quad r8 = Move(vu, 12);
    vu.Sub(XYZ, 28, 2, 4);
    Quad r9 = Move(vu, 13);
    vu.Mul(XYZ, 17, 17, 16);
    StoreWord(values + 0, r8);
    vu.Mul(XYZ, 18, 18, 16);
    StoreWord(values + 1, r9);
}

void FinishTriangleEdgeTests(f32* values)
{
    Vu0& vu = TheVu0();
    vu.Mul(XYZ, 22, 22, 21);
    vu.Mul(XYZ, 23, 23, 21);
    vu.Mul(XYZ, 27, 27, 26);
    vu.Mul(XYZ, 28, 28, 26);
    vu.AddBc(X, 17, 17, 17, Fy);
    vu.AddBc(X, 18, 18, 18, Fy);
    vu.AddBc(X, 22, 22, 22, Fy);
    vu.AddBc(X, 23, 23, 23, Fy);
    vu.AddBc(X, 27, 27, 27, Fy);
    vu.AddBc(X, 28, 28, 28, Fy);
    vu.AddBc(X, 17, 17, 17, Fz);
    vu.AddBc(X, 18, 18, 18, Fz);
    vu.AddBc(X, 22, 22, 22, Fz);
    vu.AddBc(X, 23, 23, 23, Fz);
    vu.AddBc(X, 27, 27, 27, Fz);
    vu.AddBc(X, 28, 28, 28, Fz);
    StoreWord(values + 0, Move(vu, 17));
    StoreWord(values + 1, Move(vu, 18));
    StoreWord(values + 2, Move(vu, 22));
    StoreWord(values + 3, Move(vu, 23));
    StoreWord(values + 4, Move(vu, 27));
    StoreWord(values + 5, Move(vu, 28));
}

// Every vertex of both hulls through its matrix into moved (the first's, then the other's), then every vertex of the first less
// every vertex of the other into differences (the w of each the first matrix's third row's: vf3's left from it). The vertexes are
// read one past each list's end. Counts of 0 run on until the count wraps around (the asm's 32 bit counters)
void MakeVertexDifferences(const VertexDifferences* request)
{
    Vu0& vu = TheVu0();
    const u8* matrix = Bytes(request->matrix);
    const u8* vertex = Bytes(request->vertices);
    u32 count = request->count;
    const u8* otherMatrix = Bytes(request->otherMatrix);
    const u8* otherVertex = Bytes(request->otherVertices);
    u32 otherCount = request->otherCount;
    auto* difference = reinterpret_cast<u8*>(request->differences);
    auto* moved = reinterpret_cast<u8*>(request->moved);
    vu.Lqc2(1, matrix + 0x0);
    vu.Lqc2(2, matrix + 0x10);
    vu.Lqc2(3, matrix + 0x20);
    vu.Lqc2(4, matrix + 0x30);
    vu.Lqc2(5, otherMatrix + 0x0);
    vu.Lqc2(6, otherMatrix + 0x10);
    vu.Lqc2(7, otherMatrix + 0x20);
    vu.Lqc2(8, otherMatrix + 0x30);
    vu.Lqc2(10, vertex);
    do
    {
        vu.MulaBc(XYZW, 1, 10, Fx);
        vu.MaddaBc(XYZW, 2, 10, Fy);
        vu.MaddaBc(XYZW, 3, 10, Fz);
        vu.MaddBc(XYZW, 11, 4, 10, Fw);
        count--;
        moved += 0x10;
        vu.Lqc2(10, vertex + 0x10);
        vu.Sqc2(11, moved - 0x10);
        vertex += 0x10;
    } while (count != 0);

    vu.Lqc2(10, otherVertex);
    u8* otherMoved = moved;
    do
    {
        vu.MulaBc(XYZW, 5, 10, Fx);
        vu.MaddaBc(XYZW, 6, 10, Fy);
        vu.MaddaBc(XYZW, 7, 10, Fz);
        vu.MaddBc(XYZW, 11, 8, 10, Fw);
        otherCount--;
        moved += 0x10;
        vu.Lqc2(10, otherVertex + 0x10);
        vu.Sqc2(11, moved - 0x10);
        otherVertex += 0x10;
    } while (otherCount != 0);

    const u8* first = reinterpret_cast<const u8*>(request->moved);
    u32 firsts = request->count;
    do
    {
        vu.Lqc2(1, first);
        const u8* second = otherMoved;
        u32 seconds = request->otherCount;
        do
        {
            vu.Lqc2(2, second);
            vu.Sub(XYZ, 3, 1, 2);
            seconds--;
            second += 0x10;
            vu.Sqc2(3, difference);
            difference += 0x10;
        } while (seconds != 0);

        firsts--;
        first += 0x10;
    } while (firsts != 0);
}

// Groups of four points (x, y, z, w each) made their xs, ys and zs (three quadwords), in place
void GroupPoints(Vector4* points, s32 groups)
{
    auto* read = reinterpret_cast<u8*>(points);
    auto* write = reinterpret_cast<u8*>(points);
    s32 left = groups;
    do
    {
        Quad r10 = LoadQuad(read + 0x0);
        Quad r11 = LoadQuad(read + 0x10);
        Quad r12 = LoadQuad(read + 0x20);
        Quad r13 = LoadQuad(read + 0x30);
        Quad r14 = Pextlw(r11, r10);
        Quad r15 = Pextuw(r11, r10);
        r11 = Pextlw(r13, r12);
        Quad r24 = Pextuw(r13, r12);
        r10 = Pcpyld(r11, r14);
        r11 = Pcpyud(r14, r11);
        r12 = Pcpyld(r24, r15);
        StoreQuad(write + 0x0, r10);
        StoreQuad(write + 0x10, r11);
        StoreQuad(write + 0x20, r12);
        left = static_cast<s32>(static_cast<u32>(left) - 1);
        read += 0x40;
        write += 0x30;
    } while (left != 0);
}

namespace
{
// The range's start: vf(least) all 2^65 (LUI 0x6000 is 0x60000000 in the low word: the other words are made zero first), vf(most)
// all -2^65. QMTC2 of $10 puts its upper half in the other words, which VSUB makes zero whatever they were
void StartRange(Vu0& vu, u32 least, u32 most)
{
    constexpr u32 TwoTo65 = 0x60000000;
    Quad r10 = {{TwoTo65, 0, 0, 0}};
    vu.Qmtc2(least, r10.w);
    vu.Sub(YZW, least, least, least);
    vu.AddBc(YZW, least, least, least, Fx);
    vu.Sub(XYZW, most, most, most);
    vu.Sub(XYZW, most, most, least);
}

void FinishRange(Vu0& vu, u32 least, u32 most, f32* range)
{
    vu.MiniBc(X, 8, least, least, Fy);
    vu.MiniBc(Z, 9, least, least, Fw);
    vu.MiniBc(X, 10, 8, 9, Fz);
    Quad r8;
    vu.Qmfc2(10, r8.w);
    vu.MaxBc(X, 11, most, most, Fy);
    vu.MaxBc(Z, 12, most, most, Fw);
    vu.MaxBc(X, 13, 11, 12, Fz);
    Quad r9;
    vu.Qmfc2(13, r9.w);
    StoreWord(range + 0, r8);
    StoreWord(range + 1, r9);
}
}

// The least and the most of the grouped points' dot products with the axis (from 2^65 and -2^65), into the range. The groups are
// read one past the last
void GroupsRange(const Vector4* groups, s32 count, const Vector4* axis, f32* range)
{
    Vu0& vu = TheVu0();
    const u8* group = Bytes(groups);
    s32 left = count;
    // The upper half of $10 isn't known (LUI leaves it): VSUB makes the words it puts in vf06 zero whatever they are
    StartRange(vu, 6, 7);
    vu.Lqc2(1, axis);
    vu.Lqc2(2, group + 0x0);
    vu.Lqc2(3, group + 0x10);
    vu.Lqc2(4, group + 0x20);
    do
    {
        vu.MulaBc(XYZW, 2, 1, Fx);
        vu.MaddaBc(XYZW, 3, 1, Fy);
        vu.MaddBc(XYZW, 5, 4, 1, Fz);
        left = static_cast<s32>(static_cast<u32>(left) - 1);
        group += 0x30;
        vu.Lqc2(2, group + 0x0);
        vu.Lqc2(3, group + 0x10);
        vu.Lqc2(4, group + 0x20);
        vu.Mini(XYZW, 6, 6, 5);
        vu.Max(XYZW, 7, 7, 5);
    } while (left != 0);

    FinishRange(vu, 6, 7, range);
}

// The same of 16 groups (64 points), two at a time
void SixteenGroupsRange(const Vector4* groups, const Vector4* axis, f32* range)
{
    Vu0& vu = TheVu0();
    StartRange(vu, 30, 31);
    vu.Lqc2(1, axis);
    const u8* group = Bytes(groups);
    s32 left = 8;
    vu.Lqc2(2, group + 0x0);
    vu.Lqc2(3, group + 0x10);
    vu.Lqc2(4, group + 0x20);
    vu.Lqc2(5, group + 0x30);
    vu.Lqc2(6, group + 0x40);
    vu.Lqc2(7, group + 0x50);
    do
    {
        vu.MulaBc(XYZW, 2, 1, Fx);
        vu.MaddaBc(XYZW, 3, 1, Fy);
        vu.MaddBc(XYZW, 28, 4, 1, Fz);
        vu.MulaBc(XYZW, 5, 1, Fx);
        vu.MaddaBc(XYZW, 6, 1, Fy);
        vu.MaddBc(XYZW, 29, 7, 1, Fz);
        group += 0x60;
        vu.Mini(XYZW, 30, 30, 28);
        left--;
        vu.Max(XYZW, 31, 31, 28);
        vu.Lqc2(2, group + 0x0);
        vu.Lqc2(3, group + 0x10);
        vu.Lqc2(4, group + 0x20);
        vu.Mini(XYZW, 30, 30, 29);
        vu.Lqc2(5, group + 0x30);
        vu.Lqc2(6, group + 0x40);
        vu.Lqc2(7, group + 0x50);
        vu.Max(XYZW, 31, 31, 29);
    } while (left != 0);

    FinishRange(vu, 30, 31, range);
}

namespace
{
// One group: four differences in vf16-vf19 (the first in the register given) made the xs, ys and zs of three registers
void Group(Vu0& vu, u32 first, u32 xs, u32 ys, u32 zs)
{
    Quad r10;
    Quad r11;
    Quad r12;
    Quad r13;
    vu.Qmfc2(first, r10.w);
    vu.Qmfc2(17, r11.w);
    vu.Qmfc2(18, r12.w);
    vu.Qmfc2(19, r13.w);
    Quad r14 = Pextlw(r11, r10);
    Quad r15 = Pextuw(r11, r10);
    r11 = Pextlw(r13, r12);
    Quad r24 = Pextuw(r13, r12);
    r10 = Pcpyld(r11, r14);
    r11 = Pcpyud(r14, r11);
    r12 = Pcpyld(r24, r15);
    vu.Qmtc2(xs, r10.w);
    vu.Qmtc2(ys, r11.w);
    vu.Qmtc2(zs, r12.w);
}
}

// The triangle's three vertexes less the box hull's eight (24 differences) grouped four at a time into vf2-vf19 for
// TriangleHullSupportRange (the last group's first difference made in vf1)
void LoadTriangleHullSupport(const CollisionHit* triangle, const Vector4* hullVertices)
{
    Vu0& vu = TheVu0();
    const u8* vertices = Bytes(hullVertices);
    vu.Lqc2(20, Bytes(triangle) + 0x0);
    vu.Lqc2(21, Bytes(triangle) + 0x10);
    vu.Lqc2(22, Bytes(triangle) + 0x20);
    for (u32 vertex = 0; vertex < 8; vertex++)
    {
        vu.Lqc2(23 + vertex, vertices + vertex * 0x10);
    }

    vu.Sub(XYZ, 16, 20, 23);
    vu.Sub(XYZ, 17, 20, 24);
    vu.Sub(XYZ, 18, 20, 25);
    vu.Sub(XYZ, 19, 20, 26);
    Group(vu, 16, 2, 3, 4);
    vu.Sub(XYZ, 16, 20, 27);
    vu.Sub(XYZ, 17, 20, 28);
    vu.Sub(XYZ, 18, 20, 29);
    vu.Sub(XYZ, 19, 20, 30);
    Group(vu, 16, 5, 6, 7);
    vu.Sub(XYZ, 16, 21, 23);
    vu.Sub(XYZ, 17, 21, 24);
    vu.Sub(XYZ, 18, 21, 25);
    vu.Sub(XYZ, 19, 21, 26);
    Group(vu, 16, 8, 9, 10);
    vu.Sub(XYZ, 16, 21, 27);
    vu.Sub(XYZ, 17, 21, 28);
    vu.Sub(XYZ, 18, 21, 29);
    vu.Sub(XYZ, 19, 21, 30);
    Group(vu, 16, 11, 12, 13);
    vu.Sub(XYZ, 16, 22, 23);
    vu.Sub(XYZ, 17, 22, 24);
    vu.Sub(XYZ, 18, 22, 25);
    vu.Sub(XYZ, 19, 22, 26);
    Group(vu, 16, 14, 15, 16);
    vu.Sub(XYZ, 1, 22, 27);
    vu.Sub(XYZ, 17, 22, 28);
    vu.Sub(XYZ, 18, 22, 29);
    vu.Sub(XYZ, 19, 22, 30);
    Group(vu, 1, 17, 18, 19);
}

// The least and the most of the 24 differences' dot products with the axis, into the range
void TriangleHullSupportRange(const Vector4* axis, f32* range)
{
    Vu0& vu = TheVu0();
    vu.Lqc2(1, axis);
    vu.MulaBc(XYZW, 2, 1, Fx);
    vu.MaddaBc(XYZW, 3, 1, Fy);
    vu.MaddBc(XYZW, 31, 4, 1, Fz);
    vu.MulaBc(XYZW, 5, 1, Fx);
    vu.MaddaBc(XYZW, 6, 1, Fy);
    vu.MaddBc(XYZW, 20, 7, 1, Fz);
    vu.MulaBc(XYZW, 8, 1, Fx);
    vu.MaddaBc(XYZW, 9, 1, Fy);
    vu.MaddBc(XYZW, 30, 10, 1, Fz);
    vu.Mini(XYZW, 21, 31, 20);
    vu.Max(XYZW, 20, 31, 20);
    vu.MulaBc(XYZW, 11, 1, Fx);
    vu.MaddaBc(XYZW, 12, 1, Fy);
    vu.Mini(XYZW, 22, 21, 30);
    vu.MaddBc(XYZW, 31, 13, 1, Fz);
    vu.MulaBc(XYZW, 14, 1, Fx);
    vu.Max(XYZW, 21, 20, 30);
    vu.MaddaBc(XYZW, 15, 1, Fy);
    vu.Mini(XYZW, 20, 22, 31);
    vu.MaddBc(XYZW, 30, 16, 1, Fz);
    vu.MulaBc(XYZW, 17, 1, Fx);
    vu.Max(XYZW, 21, 21, 31);
    vu.MaddaBc(XYZW, 18, 1, Fy);
    vu.Mini(XYZW, 31, 20, 30);
    vu.MaddBc(XYZW, 1, 19, 1, Fz);
    vu.Max(XYZW, 20, 21, 30);
    vu.Mini(XYZW, 31, 31, 1);
    vu.Max(XYZW, 1, 20, 1);
    vu.MiniBc(X, 31, 31, 31, Fy);
    vu.MiniBc(Z, 31, 31, 31, Fw);
    vu.MaxBc(X, 1, 1, 1, Fy);
    vu.MaxBc(Z, 1, 1, 1, Fw);
    vu.MiniBc(X, 30, 31, 31, Fz);
    vu.MaxBc(X, 31, 1, 1, Fz);
    Quad r8;
    Quad r9;
    vu.Qmfc2(30, r8.w);
    vu.Qmfc2(31, r9.w);
    StoreWord(range + 0, r8);
    StoreWord(range + 1, r9);
}
