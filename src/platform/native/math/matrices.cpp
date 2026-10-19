// src/platform/ps2/matrices.cpp natively: the maths library's VU0 half, its asm an instruction at a time on the native VU0 (the
// registers left as the PS2's are), the EE's 128 bit moves as mmi.h's

#include "game/collision.h"
#include "game/math.h"

#include "mmi.h"
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
}

void VuRotateVector(const Matrix4x4* matrix, const Vector4* vector, Vector4* out)
{
    Vu0& vu = TheVu0();
    vu.Lqc2(27, Bytes(matrix) + 0x0);
    vu.Lqc2(28, Bytes(matrix) + 0x10);
    vu.Lqc2(29, Bytes(matrix) + 0x20);
    vu.Lqc2(31, vector);
    vu.Lqc2(26, vector);
    vu.MulaBc(XYZ, 27, 31, Fx);
    vu.MaddaBc(XYZ, 28, 31, Fy);
    vu.MaddBc(XYZ, 26, 29, 31, Fz);
    vu.Sqc2(26, out);
}

void VuTransformPoint(const Matrix4x4* matrix, const Vector4* point, Vector4* out)
{
    Vu0& vu = TheVu0();
    vu.Lqc2(27, Bytes(matrix) + 0x0);
    vu.Lqc2(31, point);
    vu.Lqc2(28, Bytes(matrix) + 0x10);
    vu.Lqc2(29, Bytes(matrix) + 0x20);
    vu.Lqc2(30, Bytes(matrix) + 0x30);
    vu.MulaBc(XYZW, 27, 31, Fx);
    vu.MaddaBc(XYZW, 28, 31, Fy);
    vu.MaddaBc(XYZW, 29, 31, Fz);
    vu.MaddBc(XYZW, 26, 30, 0, Fw);
    vu.Sqc2(26, out);
}

void MultiplyRotations(Vector4* out, const Vector4* a, const Vector4* b)
{
    Vu0& vu = TheVu0();
    vu.Lqc2(31, a);
    vu.Lqc2(30, b);
    vu.Mul(XYZW, 1, 31, 30);
    vu.Opmula(31, 30);
    vu.MaddaBc(XYZ, 31, 30, Fw);
    vu.MaddaBc(XYZ, 30, 31, Fw);
    vu.SubaBc(W, 1, 1, Fz);
    vu.MsubaBc(W, 0, 1, Fy);
    vu.Opmsub(29, 30, 31);
    vu.MsubBc(W, 29, 0, 1, Fx);
    vu.Sqc2(29, out);
}

u32 PlaneThroughTriangle(Vector4* plane, const Vector4* first, const Vector4* second, const Vector4* third)
{
    Vu0& vu = TheVu0();
    alignas(16) f32 length[4];
    vu.Lqc2(8, first);
    vu.Lqc2(9, second);
    vu.Lqc2(10, third);
    vu.Sub(XYZW, 9, 9, 8);
    vu.Sub(XYZW, 10, 10, 8);
    vu.Opmula(9, 10);
    vu.Opmsub(10, 10, 9);
    vu.Mul(XYZ, 11, 10, 10);
    vu.AddBc(X, 11, 11, 11, Fy);
    vu.AddBc(X, 11, 11, 11, Fz);
    vu.Sqrt(11, Fx);
    // vwaitq: macro mode has Q at once
    vu.AddQ(X, 11, 0);
    vu.Sqc2(11, length);
    vu.Div(0, Fw, 11, Fx);
    vu.MulQ(XYZ, 10, 10);
    vu.Mul(XYZ, 9, 10, 8);
    vu.AddBc(X, 9, 9, 9, Fy);
    vu.AddBc(X, 9, 9, 9, Fz);
    vu.Sub(W, 10, 0, 0);
    vu.SubBc(W, 10, 10, 9, Fx);
    vu.Sqc2(10, plane);
    return Epsilon < length[0];
}

void VuTransformByRows(const Matrix4x4* rows, const Vector4* point, Vector4* out)
{
    Vu0& vu = TheVu0();
    vu.Lqc2(27, Bytes(rows) + 0x0);
    vu.Lqc2(31, point);
    vu.Lqc2(28, Bytes(rows) + 0x10);
    vu.Lqc2(29, Bytes(rows) + 0x20);
    vu.Lqc2(30, Bytes(rows) + 0x30);
    vu.MulaBc(XYZW, 27, 31, Fx);
    vu.MaddaBc(XYZW, 28, 31, Fy);
    vu.MaddaBc(XYZW, 29, 31, Fz);
    vu.MaddBc(XYZW, 26, 30, 31, Fw);
    vu.Sqc2(26, out);
}

void InitIdentityMatrix(Matrix4x4* matrix)
{
    // vf0 is (0, 0, 0, 1): its rotation by a word gives the third row, a w of it added to a zero the others
    Vu0& vu = TheVu0();
    vu.Sqc2(0, Bytes(matrix) + 0x30);
    vu.Sub(XYZW, 29, 29, 29);
    vu.Sub(XYZW, 28, 28, 28);
    vu.Mr32(XYZW, 30, 0);
    vu.AddBc(Y, 29, 0, 0, Fw);
    vu.AddBc(X, 28, 0, 0, Fw);
    vu.Sqc2(30, Bytes(matrix) + 0x20);
    vu.Sqc2(29, Bytes(matrix) + 0x10);
    vu.Sqc2(28, Bytes(matrix) + 0x0);
}

void VuMultiplyMatrices(const Matrix4x4* a, const Matrix4x4* b, Matrix4x4* out)
{
    Vu0& vu = TheVu0();
    vu.Lqc2(27, Bytes(b) + 0x0);
    vu.Lqc2(23, Bytes(a) + 0x0);
    vu.Lqc2(28, Bytes(b) + 0x10);
    vu.Lqc2(29, Bytes(b) + 0x20);
    vu.Lqc2(30, Bytes(b) + 0x30);
    vu.MulaBc(XYZW, 27, 23, Fx);
    vu.Lqc2(24, Bytes(a) + 0x10);
    vu.MaddaBc(XYZW, 28, 23, Fy);
    vu.MaddaBc(XYZW, 29, 23, Fz);
    vu.MaddBc(XYZW, 23, 30, 23, Fw);
    vu.MulaBc(XYZW, 27, 24, Fx);
    vu.Lqc2(25, Bytes(a) + 0x20);
    vu.MaddaBc(XYZW, 28, 24, Fy);
    vu.MaddaBc(XYZW, 29, 24, Fz);
    vu.MaddBc(XYZW, 24, 30, 24, Fw);
    vu.MulaBc(XYZW, 27, 25, Fx);
    vu.Lqc2(26, Bytes(a) + 0x30);
    vu.MaddaBc(XYZW, 28, 25, Fy);
    vu.MaddaBc(XYZW, 29, 25, Fz);
    vu.MaddBc(XYZW, 25, 30, 25, Fw);
    vu.MulaBc(XYZW, 27, 26, Fx);
    vu.MaddaBc(XYZW, 28, 26, Fy);
    vu.MaddaBc(XYZW, 29, 26, Fz);
    vu.MaddBc(XYZW, 26, 30, 26, Fw);
    vu.Sqc2(23, Bytes(out) + 0x0);
    vu.Sqc2(24, Bytes(out) + 0x10);
    vu.Sqc2(25, Bytes(out) + 0x20);
    vu.Sqc2(26, Bytes(out) + 0x30);
}

void MatrixFromRotation(Matrix4x4* matrix, const Vector4* rotation)
{
    // The rotation scaled by the square root of 2 (in I), so the products of its parts come out doubled; the rows' w 0
    constexpr u32 SquareRootOf2 = 0x3FB504F3;
    constexpr u32 IRegister = 21;
    Vu0& vu = TheVu0();
    vu.Ctc2(IRegister, SquareRootOf2);
    vu.Lqc2(1, rotation);
    vu.Mula(XYZ, 1, 1);
    vu.MulI(XYZW, 2, 1);
    vu.Madd(XYZ, 28, 1, 1);
    vu.AddBc(XYZ, 30, 0, 0, Fw);
    vu.Opmula(2, 2);
    vu.MsubBc(XYZ, 1, 2, 2, Fw);
    vu.MaddBc(XYZ, 2, 2, 2, Fw);
    vu.Mr32(W, 28, 0);
    vu.AddaBc(XYZ, 0, 0, Fw);
    vu.Mr32(W, 29, 0);
    vu.MsubaBc(YZ, 30, 28, Fx);
    vu.Mr32(W, 30, 0);
    vu.MsubBc(Z, 30, 30, 28, Fy);
    vu.Mr32(Y, 3, 1);
    vu.MsubaBc(X, 30, 28, Fy);
    vu.Mr32(W, 2, 2);
    vu.MsubBc(Y, 29, 30, 28, Fz);
    vu.Mr32(Y, 28, 2);
    vu.MsubBc(X, 28, 30, 28, Fz);
    vu.Mr32(X, 30, 2);
    vu.AddBc(Z, 28, 0, 1, Fy);
    vu.Mr32(X, 29, 3);
    vu.AddBc(Y, 30, 0, 1, Fx);
    vu.Mr32(Z, 29, 2);
    vu.Sqc2(28, Bytes(matrix) + 0x0);
    vu.Sqc2(30, Bytes(matrix) + 0x20);
    vu.Sqc2(29, Bytes(matrix) + 0x10);
}

void VuTranspose(const Matrix4x4* matrix, Matrix4x4* out)
{
    Quad r8 = LoadQuad(Bytes(matrix) + 0x0);
    Quad r9 = LoadQuad(Bytes(matrix) + 0x10);
    Quad r10 = LoadQuad(Bytes(matrix) + 0x20);
    Quad r11 = LoadQuad(Bytes(matrix) + 0x30);
    Quad r12 = Pextlw(r9, r8);
    Quad r13 = Pextuw(r9, r8);
    Quad r14 = Pextlw(r11, r10);
    Quad r15 = Pextuw(r11, r10);
    r8 = Pcpyld(r14, r12);
    r9 = Pcpyud(r12, r14);
    r10 = Pcpyld(r15, r13);
    r11 = Pcpyud(r13, r15);
    StoreQuad(Bytes(out) + 0x0, r8);
    StoreQuad(Bytes(out) + 0x10, r9);
    StoreQuad(Bytes(out) + 0x20, r10);
    StoreQuad(Bytes(out) + 0x30, r11);
}

void TransposeQuadwords(void* rows)
{
    Quad r8 = LoadQuad(Bytes(rows) + 0x0);
    Quad r9 = LoadQuad(Bytes(rows) + 0x10);
    Quad r10 = LoadQuad(Bytes(rows) + 0x20);
    Quad r11 = LoadQuad(Bytes(rows) + 0x30);
    Quad r12 = Pextlw(r9, r8);
    Quad r13 = Pextuw(r9, r8);
    Quad r14 = Pextlw(r11, r10);
    Quad r15 = Pextuw(r11, r10);
    r8 = Pcpyld(r14, r12);
    r9 = Pcpyud(r12, r14);
    r10 = Pcpyld(r15, r13);
    r11 = Pcpyud(r13, r15);
    StoreQuad(Bytes(rows) + 0x0, r8);
    StoreQuad(Bytes(rows) + 0x10, r9);
    StoreQuad(Bytes(rows) + 0x20, r10);
    StoreQuad(Bytes(rows) + 0x30, r11);
}

void VuTransposeRotation(const Matrix4x4* matrix, Matrix4x4* out)
{
    // The fourth row taken as vf0's (0, 0, 0, 1)
    Vu0& vu = TheVu0();
    Quad r8 = LoadQuad(Bytes(matrix) + 0x0);
    Quad r9 = LoadQuad(Bytes(matrix) + 0x10);
    Quad r10 = LoadQuad(Bytes(matrix) + 0x20);
    Quad r11;
    vu.Qmfc2(0, r11.w);
    Quad r12 = Pextlw(r9, r8);
    Quad r13 = Pextuw(r9, r8);
    Quad r14 = Pextlw(r11, r10);
    Quad r15 = Pextuw(r11, r10);
    r8 = Pcpyld(r14, r12);
    r9 = Pcpyud(r12, r14);
    r10 = Pcpyld(r15, r13);
    StoreQuad(Bytes(out) + 0x0, r8);
    StoreQuad(Bytes(out) + 0x10, r9);
    StoreQuad(Bytes(out) + 0x20, r10);
    vu.Sqc2(0, Bytes(out) + 0x30);
}

// The inverse of a rotation and a translation: the rotation transposed, the translation turned by it and negated
void VuInvertRigid(Matrix4x4* out, const Matrix4x4* matrix)
{
    Vu0& vu = TheVu0();
    vu.SubBc(X, 28, 0, 0, Fw);
    Quad r8 = LoadQuad(Bytes(matrix) + 0x0);
    Quad r9 = LoadQuad(Bytes(matrix) + 0x10);
    Quad r10 = LoadQuad(Bytes(matrix) + 0x20);
    Quad r11 = LoadQuad(Bytes(matrix) + 0x30);
    vu.Qmtc2(31, r11.w);
    vu.MulBc(XYZ, 31, 31, 28, Fx);
    Quad r12 = Pextlw(r9, r8);
    Quad r13 = Pextuw(r9, r8);
    Quad r14 = Pextlw(r11, r10);
    Quad r15 = Pextuw(r11, r10);
    r8 = Pcpyld(r14, r12);
    r9 = Pcpyud(r12, r14);
    r10 = Pcpyld(r15, r13);
    vu.Move(W, 31, 0);
    vu.Qmtc2(28, r8.w);
    vu.Qmtc2(29, r9.w);
    vu.Qmtc2(30, r10.w);
    vu.MulaBc(XYZ, 28, 31, Fx);
    vu.MaddaBc(XYZ, 29, 31, Fy);
    vu.MaddBc(XYZ, 31, 30, 31, Fz);
    vu.Sub(W, 28, 28, 28);
    vu.Sub(W, 29, 29, 29);
    vu.Sub(W, 30, 30, 30);
    vu.Sqc2(31, Bytes(out) + 0x30);
    vu.Sqc2(28, Bytes(out) + 0x0);
    vu.Sqc2(29, Bytes(out) + 0x10);
    vu.Sqc2(30, Bytes(out) + 0x20);
}

void VuInvertRigidInPlace(Matrix4x4* matrix)
{
    VuInvertRigid(matrix, matrix);
}
