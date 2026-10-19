// src/platform/ps2/bounds.cpp natively: the boxes the game works out on VU0, its asm an instruction at a time on the native VU0

#include "game/collision.h"
#include "game/hull.h"
#include "game/math.h"

#include "vu0.h"

using namespace NativeMath;

namespace
{
const u8* Bytes(const void* address)
{
    return static_cast<const u8*>(address);
}

u8* Bytes(void* address)
{
    return static_cast<u8*>(address);
}

// The points' box so far: the first point in both registers
void StartBox(const Vector4* point)
{
    Vu0& vu = TheVu0();
    vu.Lqc2(1, point);
    vu.Lqc2(2, point);
}

void TakeIntoBox(const Vector4* point)
{
    Vu0& vu = TheVu0();
    vu.Lqc2(3, point);
    vu.Mini(XYZ, 1, 1, 3);
    vu.Max(XYZ, 2, 2, 3);
}

void StoreBox(Box* box)
{
    Vu0& vu = TheVu0();
    vu.Sqc2(1, Bytes(box) + 0x0);
    vu.Sqc2(2, Bytes(box) + 0x10);
}
}

void GetBbox(const Vector4* points, s32 count, Box* box, const Matrix4x4* matrix)
{
    // The matrix's rows in vf04-vf07, the first point moved into both registers
    Vu0& vu = TheVu0();
    vu.Lqc2(4, Bytes(matrix) + 0x0);
    vu.Lqc2(5, Bytes(matrix) + 0x10);
    vu.Lqc2(6, Bytes(matrix) + 0x20);
    vu.Lqc2(7, Bytes(matrix) + 0x30);
    vu.Lqc2(1, &points[0]);
    vu.Lqc2(2, &points[0]);
    vu.MulaBc(XYZW, 4, 1, Fx);
    vu.MaddaBc(XYZW, 5, 1, Fy);
    vu.MaddaBc(XYZW, 6, 1, Fz);
    vu.MaddBc(XYZW, 1, 7, 1, Fw);
    vu.MulaBc(XYZW, 4, 2, Fx);
    vu.MaddaBc(XYZW, 5, 2, Fy);
    vu.MaddaBc(XYZW, 6, 2, Fz);
    vu.MaddBc(XYZW, 2, 7, 2, Fw);
    for (s32 index = 1; index < count; index++)
    {
        vu.Lqc2(3, &points[index]);
        vu.MulaBc(XYZW, 4, 3, Fx);
        vu.MaddaBc(XYZW, 5, 3, Fy);
        vu.MaddaBc(XYZW, 6, 3, Fz);
        vu.MaddBc(XYZW, 3, 7, 3, Fw);
        vu.Mini(XYZ, 1, 1, 3);
        vu.Max(XYZ, 2, 2, 3);
    }

    StoreBox(box);
}

void PointsBox(const Vector4* points, s32 count, Box* box)
{
    StartBox(&points[0]);
    for (s32 index = 1; index < count; index++)
    {
        TakeIntoBox(&points[index]);
    }

    StoreBox(box);
}

void TriangleBounds(Box* box, const Vector4* first, const Vector4* second, const Vector4* third)
{
    // The corners' w stay what vf11 and vf12 had
    Vu0& vu = TheVu0();
    vu.Lqc2(8, first);
    vu.Lqc2(9, second);
    vu.Lqc2(10, third);
    vu.Mini(XYZ, 11, 8, 9);
    vu.Max(XYZ, 12, 8, 9);
    vu.Mini(XYZ, 11, 11, 10);
    vu.Max(XYZ, 12, 12, 10);
    vu.Sqc2(11, Bytes(box) + 0x0);
    vu.Sqc2(12, Bytes(box) + 0x10);
}
