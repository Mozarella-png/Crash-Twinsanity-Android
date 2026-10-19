// Platform::Math natively: src/platform/ps2/math.cpp's asm an instruction at a time on the native VU0 (vu0.h), its microprograms
// the retail microcode's translations (microprograms.cpp), and the EE FPU's MAX.S, MIN.S and RSQRT.S as PCSX2 has them

#include "platform/math.h"

#include "game/math.h"

#include "vu0.h"

namespace
{
using namespace NativeMath;

u32 Bits(f32 value)
{
    return VuFloat::AsBits(value);
}

f32 Float(u32 bits)
{
    return VuFloat::AsFloat(bits);
}
}

namespace Platform::Math
{
f32 Max(f32 first, f32 second)
{
    return Float(VuFloat::FpuMax(Bits(first), Bits(second)));
}

f32 Min(f32 first, f32 second)
{
    return Float(VuFloat::FpuMin(Bits(first), Bits(second)));
}

void SinCos(f32 first, f32 second, f32* out)
{
    Vu0& vu = TheVu0();
    alignas(16) f32 angles[4] = {first, 0.0f, second, 0.0f};
    alignas(16) f32 values[4];
    vu.Lqc2(20, angles);
    Vu0CallMicroprogram(0xF0);
    vu.Sqc2(31, values);
    for (u32 index = 0; index < 4; index++)
    {
        out[index] = values[index];
    }
}

void SlerpRotations(const Vector4* from, const Vector4* to, f32 share, Vector4* out)
{
    Vu0& vu = TheVu0();
    alignas(16) f32 shares[4] = {share, share, share, 1.0f};
    vu.Lqc2(1, to);
    vu.Lqc2(31, from);
    vu.Lqc2(29, shares);
    Vu0CallMicroprogram(0x1C0);
    vu.Sqc2(1, out);
}

void EulerRotation(const Vector4* anglesNow, const Vector4* anglesNext, const Vector4* moveNow, const Vector4* moveNext,
                   Vector4* rotation, Vector4* move)
{
    Vu0& vu = TheVu0();
    vu.Lqc2(29, moveNow);
    vu.Lqc2(28, moveNext);
    vu.Lqc2(31, anglesNow);
    vu.Lqc2(30, anglesNext);
    Vu0CallMicroprogram(0x818);
    vu.Sqc2(1, rotation);
    vu.Sqc2(2, move);
}

f32 ViewDistance(s32 view, const f32* point)
{
    Vu0& vu = TheVu0();
    alignas(16) f32 place[4] = {point[0], point[1], point[2], 1.0f};
    alignas(16) f32 distance[4];
    vu.Lqc2(1, place);
    vu.Ctc2(1, static_cast<u32>(view));
    Vu0CallMicroprogram(0xCB0);
    vu.Sqc2(30, distance);
    return distance[0];
}

bool ParticleBlockView(s32 view, bool keepsTranslation, const f32* extents, const Matrix4x4* gravity, const f32* place, f32 scale,
                       Matrix4x4* matrix, f32* distance)
{
    Vu0& vu = TheVu0();
    alignas(16) f32 box[4] = {extents[0], extents[1], extents[2], 1.0f};
    alignas(16) f32 at[4] = {place[0], place[1], place[2], 1.0f};
    alignas(16) f32 pull[4] = {scale, 0.0f, 0.0f, 1.0f};
    alignas(16) f32 far[4];
    const auto* rows = reinterpret_cast<const u8*>(gravity);
    auto* out = reinterpret_cast<u8*>(matrix);
    vu.Lqc2(2, box);
    vu.Lqc2(3, rows);
    vu.Lqc2(4, rows + 0x10);
    vu.Lqc2(5, rows + 0x20);
    vu.Lqc2(6, at);
    vu.Ctc2(1, static_cast<u32>(view));
    vu.Lqc2(7, pull);
    Vu0CallMicroprogram(keepsTranslation ? 0xB90 : 0xA08);
    vu.Sqc2(30, far);
    vu.Sqc2(29, out);
    vu.Sqc2(28, out + 0x10);
    vu.Sqc2(27, out + 0x20);
    vu.Sqc2(26, out + 0x30);
    u32 outside = vu.Cfc2(2);
    *distance = far[0];
    return outside != 0;
}

void TurnRotation(const Vector4* by, Vector4* rotation)
{
    Vu0& vu = TheVu0();
    vu.Lqc2(1, rotation);
    vu.Lqc2(31, by);
    Vu0CallMicroprogram(0xA70);
    vu.Sqc2(1, rotation);
}

void Lerp(const Vector4* from, const Vector4* to, f32 share, Vector4* out)
{
    Vu0& vu = TheVu0();
    alignas(16) f32 shares[4] = {0.0f, 0.0f, 0.0f, share};
    vu.Lqc2(31, shares);
    vu.Lqc2(30, from);
    vu.Lqc2(29, to);
    // vaddax.xyz ACC, vf30, vf0x; vmsubaw.xyz ACC, vf30, vf31w; vmaddw.xyz vf29, vf29, vf31w; vmove.w vf29, vf0
    vu.AddaBc(XYZ, 30, 0, Fx);
    vu.MsubaBc(XYZ, 30, 31, Fw);
    vu.MaddBc(XYZ, 29, 29, 31, Fw);
    vu.Move(W, 29, 0);
    vu.Sqc2(29, out);
}

void JointMatrix(const Vector4* rotation, const Vector4* parentScale, const Vector4* scale, const Vector4* translation,
                 const Matrix4x4* parent, Matrix4x4* out)
{
    Vu0& vu = TheVu0();
    if (rotation != nullptr)
    {
        if (parentScale != nullptr)
        {
            vu.Lqc2(29, parentScale);
        }
        else
        {
            // vmaxw.xyzw vf29, vf0, vf0w
            vu.MaxBc(XYZW, 29, 0, 0, Fw);
        }

        vu.Lqc2(31, rotation);
        if (scale != nullptr)
        {
            vu.Lqc2(30, scale);
            Vu0CallMicroprogram(0x668);
        }
        else
        {
            Vu0CallMicroprogram(0x548);
        }
    }
    else
    {
        // The identity's rows: vsub.xyzw vf2, vf2, vf2; vsub.xyzw vf1, vf1, vf1; vmr32.xyzw vf3, vf0; vaddw.y vf2, vf0, vf0w;
        // vaddw.x vf1, vf0, vf0w
        vu.Sub(XYZW, 2, 2, 2);
        vu.Sub(XYZW, 1, 1, 1);
        vu.Mr32(XYZW, 3, 0);
        vu.AddBc(Y, 2, 0, 0, Fw);
        vu.AddBc(X, 1, 0, 0, Fw);
    }

    if (translation != nullptr)
    {
        vu.Lqc2(4, translation);
    }
    else
    {
        vu.Move(XYZW, 4, 0);
    }

    if (parent != nullptr)
    {
        const auto* rows = reinterpret_cast<const u8*>(parent);
        vu.Lqc2(5, rows);
        vu.Lqc2(6, rows + 16);
        vu.Lqc2(7, rows + 32);
        vu.Lqc2(8, rows + 48);
        Vu0CallMicroprogram(0x790);
    }

    auto* rows = reinterpret_cast<u8*>(out);
    vu.Sqc2(1, rows);
    vu.Sqc2(2, rows + 16);
    vu.Sqc2(3, rows + 32);
    vu.Sqc2(4, rows + 48);
}

void MultiplyByParent(const Matrix4x4* matrix, const Matrix4x4* parent, Matrix4x4* out)
{
    Vu0& vu = TheVu0();
    const auto* rows = reinterpret_cast<const u8*>(matrix);
    const auto* parentRows = reinterpret_cast<const u8*>(parent);
    vu.Lqc2(1, rows);
    vu.Lqc2(2, rows + 16);
    vu.Lqc2(3, rows + 32);
    vu.Lqc2(4, rows + 48);
    vu.Lqc2(5, parentRows);
    vu.Lqc2(6, parentRows + 16);
    vu.Lqc2(7, parentRows + 32);
    vu.Lqc2(8, parentRows + 48);
    Vu0CallMicroprogram(0x790);
    auto* outRows = reinterpret_cast<u8*>(out);
    vu.Sqc2(1, outRows);
    vu.Sqc2(2, outRows + 16);
    vu.Sqc2(3, outRows + 32);
    vu.Sqc2(4, outRows + 48);
}

f32 DivideBySquareRoot(f32 value, f32 square)
{
    return Float(VuFloat::FpuReciprocalSquareRoot(Bits(value), Bits(square)));
}

void SetRay(const Vector4* start, const Vector4* end)
{
    Vu0& vu = TheVu0();
    vu.Lqc2(4, start);
    vu.Lqc2(5, end);
}

// The PS2 runs the microprogram while the caller goes on; nothing between touches VU0, so running it here gives the same
void StartRayTriangle(const Vector4* vertices, const Vector4* nearest)
{
    Vu0& vu = TheVu0();
    const auto* bytes = reinterpret_cast<const u8*>(vertices);
    vu.Lqc2(1, bytes);
    vu.Lqc2(2, bytes + 16);
    vu.Lqc2(3, bytes + 32);
    vu.Lqc2(6, nearest);
    Vu0CallMicroprogram(0xAB8);
}

bool FinishRayTriangle(Vector4* hit)
{
    // The microprogram's flags when the triangle was hit nearer
    constexpr u32 Nearer = 0x900;
    Vu0& vu = TheVu0();
    u32 flags = vu.Cfc2(2);
    if (flags != Nearer)
    {
        return false;
    }

    vu.Sqc2(31, hit);
    return true;
}
}
