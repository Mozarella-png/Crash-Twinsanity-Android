#include "vu0.h"

#include "microprograms.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace NativeMath
{
namespace
{
constexpr u32 DataBytes = 0x1000;
constexpr u32 MicroInstructions = 0x200;
constexpr u32 One = 0x3F800000u;

alignas(16) u8 s_Data[DataBytes];

// Which set wrote each instruction of micro memory last (0 while nothing's said: the checks are off until a set is loaded)
u8 s_MicroOwner[MicroInstructions];
bool s_OwnersKnown = false;

// A field's MAC flag bits are at 3 - field (x the highest)
constexpr u32 MacShift(u32 field)
{
    return 3 - field;
}

bool Writes(u32 dest, u32 field)
{
    return ((dest >> MacShift(field)) & 1) != 0;
}

u32 OpAdd(u32 a, u32 b)
{
    return VuFloat::Add(a, b);
}

u32 OpSub(u32 a, u32 b)
{
    return VuFloat::Sub(a, b);
}

u32 OpMul(u32 a, u32 b)
{
    return VuFloat::Mul(a, b);
}

u32 OpMax(u32 a, u32 b)
{
    return VuFloat::Max(a, b);
}

u32 OpMin(u32 a, u32 b)
{
    return VuFloat::Min(a, b);
}

u32 OpMadd(u32 acc, u32 a, u32 b)
{
    return VuFloat::MulAdd(acc, a, b);
}

u32 OpMsub(u32 acc, u32 a, u32 b)
{
    return VuFloat::MulSub(acc, a, b);
}

void Fail(const char* message, u32 value)
{
    std::fprintf(stderr, "VU0: %s 0x%X\n", message, value);
    std::abort();
}
}

Vu0::Vu0() : vf{}, vi{}, acc{}, q(0), i(0), r(One), p(0), mac(0), clip(0), data(s_Data), discard_{}
{
    vf[0].v[3] = One;
}

Vu0& TheVu0()
{
    static Vu0 vu;
    return vu;
}

void SetVu0DataMemory(u8* memory)
{
    TheVu0().data = memory != nullptr ? memory : s_Data;
}

void Vu0::Lqc2(u32 ft, const void* address)
{
    if (ft == 0)
    {
        return;
    }

    std::memcpy(vf[ft].v, address, sizeof(Vf));
}

void Vu0::Sqc2(u32 fs, void* address) const
{
    std::memcpy(address, vf[fs].v, sizeof(Vf));
}

void Vu0::Qmtc2(u32 fs, const u32* words)
{
    if (fs == 0)
    {
        return;
    }

    std::memcpy(vf[fs].v, words, sizeof(Vf));
}

void Vu0::Qmfc2(u32 fs, u32* words) const
{
    std::memcpy(words, vf[fs].v, sizeof(Vf));
}

void Vu0::Ctc2(u32 reg, u32 value)
{
    constexpr u32 IRegister = 21;
    if (reg == 0)
    {
        return;
    }

    if (reg < 16)
    {
        vi[reg] = static_cast<u16>(value);
        return;
    }

    if (reg == IRegister)
    {
        i = value;
        return;
    }

    Fail("CTC2 to a control register the maths doesn't use", reg);
}

u32 Vu0::Cfc2(u32 reg) const
{
    constexpr u32 ClipRegister = 18;
    if (reg < 16)
    {
        return vi[reg];
    }

    if (reg == ClipRegister)
    {
        return clip;
    }

    Fail("CFC2 from a control register the maths doesn't use", reg);
    return 0;
}

// PCSX2's VU_MAC_UPDATE: the sign bit is the sign flag; a zero sets the zero flag; a denormal (none come out of the arithmetic)
// is the underflow flag and a zero, an exponent of 255 (none either) the overflow flag and the largest float
u32 Vu0::MacField(u32 field, u32 value)
{
    u32 shift = MacShift(field);
    u32 sign = value & VuFloat::SignBit;
    if (sign != 0)
    {
        mac |= 0x10u << shift;
    }
    else
    {
        mac &= ~(0x10u << shift);
    }

    if ((value & ~VuFloat::SignBit) == 0)
    {
        mac = (mac & ~(0x1100u << shift)) | (0x1u << shift);
        return value;
    }

    u32 exponent = value & VuFloat::ExponentMask;
    if (exponent == 0)
    {
        mac = (mac & ~(0x1000u << shift)) | (0x0101u << shift);
        return sign;
    }

    if (exponent == VuFloat::ExponentMask)
    {
        mac = (mac & ~(0x0101u << shift)) | (0x1000u << shift);
        return sign | VuFloat::MaxMagnitude;
    }

    mac &= ~(0x1101u << shift);
    return value;
}

void Vu0::ClearMacField(u32 field)
{
    mac &= ~(0x1111u << MacShift(field));
}

Vf* Vu0::Out(u32 fd)
{
    return fd == 0 ? &discard_ : &vf[fd];
}

// The sources are copies: a field written doesn't change what another reads (each field reads only its own anyway)
void Vu0::Apply(u32 dest, Vf* out, const Vf& fs, const Vf& ft, Binary op)
{
    Vf a = fs;
    Vf b = ft;
    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            out->v[field] = MacField(field, op(a.v[field], b.v[field]));
        }
        else
        {
            ClearMacField(field);
        }
    }
}

void Vu0::ApplyBroadcast(u32 dest, Vf* out, const Vf& fs, u32 bc, Binary op)
{
    Vf a = fs;
    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            out->v[field] = MacField(field, op(a.v[field], bc));
        }
        else
        {
            ClearMacField(field);
        }
    }
}

void Vu0::ApplyAcc(u32 dest, Vf* out, const Vf& fs, const Vf& ft, Ternary op)
{
    Vf a = fs;
    Vf b = ft;
    Vf accumulator = acc;
    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            out->v[field] = MacField(field, op(accumulator.v[field], a.v[field], b.v[field]));
        }
        else
        {
            ClearMacField(field);
        }
    }
}

void Vu0::ApplyAccBroadcast(u32 dest, Vf* out, const Vf& fs, u32 bc, Ternary op)
{
    Vf a = fs;
    Vf accumulator = acc;
    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            out->v[field] = MacField(field, op(accumulator.v[field], a.v[field], bc));
        }
        else
        {
            ClearMacField(field);
        }
    }
}

void Vu0::Add(u32 dest, u32 fd, u32 fs, u32 ft)
{
    Apply(dest, Out(fd), vf[fs], vf[ft], OpAdd);
}

void Vu0::AddBc(u32 dest, u32 fd, u32 fs, u32 ft, Field bc)
{
    ApplyBroadcast(dest, Out(fd), vf[fs], vf[ft].v[bc], OpAdd);
}

void Vu0::AddI(u32 dest, u32 fd, u32 fs)
{
    ApplyBroadcast(dest, Out(fd), vf[fs], i, OpAdd);
}

void Vu0::AddQ(u32 dest, u32 fd, u32 fs)
{
    ApplyBroadcast(dest, Out(fd), vf[fs], q, OpAdd);
}

void Vu0::Sub(u32 dest, u32 fd, u32 fs, u32 ft)
{
    Apply(dest, Out(fd), vf[fs], vf[ft], OpSub);
}

void Vu0::SubBc(u32 dest, u32 fd, u32 fs, u32 ft, Field bc)
{
    ApplyBroadcast(dest, Out(fd), vf[fs], vf[ft].v[bc], OpSub);
}

void Vu0::SubI(u32 dest, u32 fd, u32 fs)
{
    ApplyBroadcast(dest, Out(fd), vf[fs], i, OpSub);
}

void Vu0::SubQ(u32 dest, u32 fd, u32 fs)
{
    ApplyBroadcast(dest, Out(fd), vf[fs], q, OpSub);
}

void Vu0::Mul(u32 dest, u32 fd, u32 fs, u32 ft)
{
    Apply(dest, Out(fd), vf[fs], vf[ft], OpMul);
}

void Vu0::MulBc(u32 dest, u32 fd, u32 fs, u32 ft, Field bc)
{
    ApplyBroadcast(dest, Out(fd), vf[fs], vf[ft].v[bc], OpMul);
}

void Vu0::MulI(u32 dest, u32 fd, u32 fs)
{
    ApplyBroadcast(dest, Out(fd), vf[fs], i, OpMul);
}

void Vu0::MulQ(u32 dest, u32 fd, u32 fs)
{
    ApplyBroadcast(dest, Out(fd), vf[fs], q, OpMul);
}

void Vu0::Madd(u32 dest, u32 fd, u32 fs, u32 ft)
{
    ApplyAcc(dest, Out(fd), vf[fs], vf[ft], OpMadd);
}

void Vu0::MaddBc(u32 dest, u32 fd, u32 fs, u32 ft, Field bc)
{
    ApplyAccBroadcast(dest, Out(fd), vf[fs], vf[ft].v[bc], OpMadd);
}

void Vu0::MaddI(u32 dest, u32 fd, u32 fs)
{
    ApplyAccBroadcast(dest, Out(fd), vf[fs], i, OpMadd);
}

void Vu0::MaddQ(u32 dest, u32 fd, u32 fs)
{
    ApplyAccBroadcast(dest, Out(fd), vf[fs], q, OpMadd);
}

void Vu0::Msub(u32 dest, u32 fd, u32 fs, u32 ft)
{
    ApplyAcc(dest, Out(fd), vf[fs], vf[ft], OpMsub);
}

void Vu0::MsubBc(u32 dest, u32 fd, u32 fs, u32 ft, Field bc)
{
    ApplyAccBroadcast(dest, Out(fd), vf[fs], vf[ft].v[bc], OpMsub);
}

void Vu0::MsubI(u32 dest, u32 fd, u32 fs)
{
    ApplyAccBroadcast(dest, Out(fd), vf[fs], i, OpMsub);
}

void Vu0::MsubQ(u32 dest, u32 fd, u32 fs)
{
    ApplyAccBroadcast(dest, Out(fd), vf[fs], q, OpMsub);
}

void Vu0::Adda(u32 dest, u32 fs, u32 ft)
{
    Apply(dest, &acc, vf[fs], vf[ft], OpAdd);
}

void Vu0::AddaBc(u32 dest, u32 fs, u32 ft, Field bc)
{
    ApplyBroadcast(dest, &acc, vf[fs], vf[ft].v[bc], OpAdd);
}

void Vu0::AddaI(u32 dest, u32 fs)
{
    ApplyBroadcast(dest, &acc, vf[fs], i, OpAdd);
}

void Vu0::AddaQ(u32 dest, u32 fs)
{
    ApplyBroadcast(dest, &acc, vf[fs], q, OpAdd);
}

void Vu0::Suba(u32 dest, u32 fs, u32 ft)
{
    Apply(dest, &acc, vf[fs], vf[ft], OpSub);
}

void Vu0::SubaBc(u32 dest, u32 fs, u32 ft, Field bc)
{
    ApplyBroadcast(dest, &acc, vf[fs], vf[ft].v[bc], OpSub);
}

void Vu0::SubaI(u32 dest, u32 fs)
{
    ApplyBroadcast(dest, &acc, vf[fs], i, OpSub);
}

void Vu0::SubaQ(u32 dest, u32 fs)
{
    ApplyBroadcast(dest, &acc, vf[fs], q, OpSub);
}

void Vu0::Mula(u32 dest, u32 fs, u32 ft)
{
    Apply(dest, &acc, vf[fs], vf[ft], OpMul);
}

void Vu0::MulaBc(u32 dest, u32 fs, u32 ft, Field bc)
{
    ApplyBroadcast(dest, &acc, vf[fs], vf[ft].v[bc], OpMul);
}

void Vu0::MulaI(u32 dest, u32 fs)
{
    ApplyBroadcast(dest, &acc, vf[fs], i, OpMul);
}

void Vu0::MulaQ(u32 dest, u32 fs)
{
    ApplyBroadcast(dest, &acc, vf[fs], q, OpMul);
}

void Vu0::Madda(u32 dest, u32 fs, u32 ft)
{
    ApplyAcc(dest, &acc, vf[fs], vf[ft], OpMadd);
}

void Vu0::MaddaBc(u32 dest, u32 fs, u32 ft, Field bc)
{
    ApplyAccBroadcast(dest, &acc, vf[fs], vf[ft].v[bc], OpMadd);
}

void Vu0::MaddaI(u32 dest, u32 fs)
{
    ApplyAccBroadcast(dest, &acc, vf[fs], i, OpMadd);
}

void Vu0::MaddaQ(u32 dest, u32 fs)
{
    ApplyAccBroadcast(dest, &acc, vf[fs], q, OpMadd);
}

void Vu0::Msuba(u32 dest, u32 fs, u32 ft)
{
    ApplyAcc(dest, &acc, vf[fs], vf[ft], OpMsub);
}

void Vu0::MsubaBc(u32 dest, u32 fs, u32 ft, Field bc)
{
    ApplyAccBroadcast(dest, &acc, vf[fs], vf[ft].v[bc], OpMsub);
}

void Vu0::MsubaI(u32 dest, u32 fs)
{
    ApplyAccBroadcast(dest, &acc, vf[fs], i, OpMsub);
}

void Vu0::MsubaQ(u32 dest, u32 fs)
{
    ApplyAccBroadcast(dest, &acc, vf[fs], q, OpMsub);
}

void Vu0::Opmula(u32 fs, u32 ft)
{
    Vf a = vf[fs];
    Vf b = vf[ft];
    acc.v[Fx] = MacField(Fx, VuFloat::Mul(a.v[Fy], b.v[Fz]));
    acc.v[Fy] = MacField(Fy, VuFloat::Mul(a.v[Fz], b.v[Fx]));
    acc.v[Fz] = MacField(Fz, VuFloat::Mul(a.v[Fx], b.v[Fy]));
}

void Vu0::Opmsub(u32 fd, u32 fs, u32 ft)
{
    Vf a = vf[fs];
    Vf b = vf[ft];
    Vf accumulator = acc;
    Vf* out = Out(fd);
    out->v[Fx] = MacField(Fx, VuFloat::MulSub(accumulator.v[Fx], a.v[Fy], b.v[Fz]));
    out->v[Fy] = MacField(Fy, VuFloat::MulSub(accumulator.v[Fy], a.v[Fz], b.v[Fx]));
    out->v[Fz] = MacField(Fz, VuFloat::MulSub(accumulator.v[Fz], a.v[Fx], b.v[Fy]));
}

// MAX and MINI write nothing to vf00 and set no flags
void Vu0::Max(u32 dest, u32 fd, u32 fs, u32 ft)
{
    if (fd == 0)
    {
        return;
    }

    Vf a = vf[fs];
    Vf b = vf[ft];
    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            vf[fd].v[field] = VuFloat::Max(a.v[field], b.v[field]);
        }
    }
}

void Vu0::MaxBc(u32 dest, u32 fd, u32 fs, u32 ft, Field bc)
{
    if (fd == 0)
    {
        return;
    }

    Vf a = vf[fs];
    u32 b = vf[ft].v[bc];
    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            vf[fd].v[field] = VuFloat::Max(a.v[field], b);
        }
    }
}

void Vu0::MaxI(u32 dest, u32 fd, u32 fs)
{
    if (fd == 0)
    {
        return;
    }

    Vf a = vf[fs];
    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            vf[fd].v[field] = VuFloat::Max(a.v[field], i);
        }
    }
}

void Vu0::Mini(u32 dest, u32 fd, u32 fs, u32 ft)
{
    if (fd == 0)
    {
        return;
    }

    Vf a = vf[fs];
    Vf b = vf[ft];
    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            vf[fd].v[field] = VuFloat::Min(a.v[field], b.v[field]);
        }
    }
}

void Vu0::MiniBc(u32 dest, u32 fd, u32 fs, u32 ft, Field bc)
{
    if (fd == 0)
    {
        return;
    }

    Vf a = vf[fs];
    u32 b = vf[ft].v[bc];
    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            vf[fd].v[field] = VuFloat::Min(a.v[field], b);
        }
    }
}

void Vu0::MiniI(u32 dest, u32 fd, u32 fs)
{
    if (fd == 0)
    {
        return;
    }

    Vf a = vf[fs];
    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            vf[fd].v[field] = VuFloat::Min(a.v[field], i);
        }
    }
}

void Vu0::Abs(u32 dest, u32 ft, u32 fs)
{
    if (ft == 0)
    {
        return;
    }

    Vf a = vf[fs];
    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            vf[ft].v[field] = a.v[field] & ~VuFloat::SignBit;
        }
    }
}

// FTOIn: the float times 2^n, from 2^31 up the largest integer of its sign, else cut towards zero
void Vu0::Ftoi(u32 dest, u32 ft, u32 fs, u32 fractionBits)
{
    if (ft == 0)
    {
        return;
    }

    Vf a = vf[fs];
    u32 scale = One + (fractionBits << 23);
    for (u32 field = 0; field < 4; field++)
    {
        if (!Writes(dest, field))
        {
            continue;
        }

        u32 scaled = fractionBits != 0 ? VuFloat::Mul(a.v[field], scale) : VuFloat::Operand(a.v[field]);
        if ((scaled & VuFloat::ExponentMask) >= 0x4F000000u)
        {
            vf[ft].v[field] = (scaled & VuFloat::SignBit) != 0 ? 0x80000000u : 0x7FFFFFFFu;
        }
        else
        {
            vf[ft].v[field] = static_cast<u32>(static_cast<s32>(VuFloat::AsFloat(scaled)));
        }
    }
}

// ITOFn: the integer made a float (rounded towards zero) and divided by 2^n
void Vu0::Itof(u32 dest, u32 ft, u32 fs, u32 fractionBits)
{
    if (ft == 0)
    {
        return;
    }

    Vf a = vf[fs];
    u32 scale = One - (fractionBits << 23);
    for (u32 field = 0; field < 4; field++)
    {
        if (!Writes(dest, field))
        {
            continue;
        }

        double value = static_cast<s32>(a.v[field]);
#ifndef TWIN_VU_FLOAT_HOST
        u32 converted = VuFloat::TruncateToFloat(value);
#else
        u32 converted = VuFloat::AsBits(static_cast<f32>(value));
#endif
        vf[ft].v[field] = fractionBits != 0 ? VuFloat::Mul(converted, scale) : converted;
    }
}

// PCSX2's CLIP: |ft.w| (a denormal taken as the largest denormal) against x, y and z as signed magnitudes
void Vu0::Clip(u32 fs, u32 ft)
{
    u32 w = vf[ft].v[Fw];
    s32 limit = static_cast<s32>((w & VuFloat::ExponentMask) != 0 ? (w & ~VuFloat::SignBit) : 0x007FFFFFu);
    u32 flags = clip << 6;
    for (u32 field = 0; field < 3; field++)
    {
        u32 value = vf[fs].v[field];
        if (static_cast<s32>(value) > limit)
        {
            flags |= 1u << (field * 2);
        }

        if (static_cast<s32>(value ^ VuFloat::SignBit) > limit)
        {
            flags |= 2u << (field * 2);
        }
    }

    clip = flags & 0xFFFFFF;
}

void Vu0::Move(u32 dest, u32 ft, u32 fs)
{
    if (ft == 0)
    {
        return;
    }

    Vf a = vf[fs];
    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            vf[ft].v[field] = a.v[field];
        }
    }
}

// MR32: the fields moved one place round (x from y, ..., w from x)
void Vu0::Mr32(u32 dest, u32 ft, u32 fs)
{
    if (ft == 0)
    {
        return;
    }

    Vf a = vf[fs];
    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            vf[ft].v[field] = a.v[(field + 1) & 3];
        }
    }
}

u32 Vu0::DivValue(u32 fs, ::NativeMath::Field fsf, u32 ft, ::NativeMath::Field ftf) const
{
    return VuFloat::Divide(vf[fs].v[fsf], vf[ft].v[ftf]);
}

u32 Vu0::SqrtValue(u32 ft, ::NativeMath::Field ftf) const
{
    return VuFloat::SquareRoot(vf[ft].v[ftf]);
}

u32 Vu0::RsqrtValue(u32 fs, ::NativeMath::Field fsf, u32 ft, ::NativeMath::Field ftf) const
{
    return VuFloat::ReciprocalSquareRoot(vf[fs].v[fsf], vf[ft].v[ftf]);
}

void Vu0::Div(u32 fs, ::NativeMath::Field fsf, u32 ft, ::NativeMath::Field ftf)
{
    q = DivValue(fs, fsf, ft, ftf);
}

void Vu0::Sqrt(u32 ft, ::NativeMath::Field ftf)
{
    q = SqrtValue(ft, ftf);
}

void Vu0::Rsqrt(u32 fs, ::NativeMath::Field fsf, u32 ft, ::NativeMath::Field ftf)
{
    q = RsqrtValue(fs, fsf, ft, ftf);
}

// VU0's data memory wraps round at 4 KB; addresses with bit 0x4000 (VU1's registers seen from VU0) aren't modelled
void Vu0::Lq(u32 dest, u32 ft, u32 is, s32 offset)
{
    u16 address = static_cast<u16>((static_cast<s16>(vi[is]) + offset) * 16);
    if ((address & 0x4000) != 0)
    {
        Fail("LQ from VU1's registers", address);
    }

    if (ft == 0)
    {
        return;
    }

    u32 words[4];
    std::memcpy(words, data + (address & (DataBytes - 1)), sizeof(words));
    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            vf[ft].v[field] = words[field];
        }
    }
}

void Vu0::Sq(u32 dest, u32 fs, u32 it, s32 offset)
{
    u16 address = static_cast<u16>((static_cast<s16>(vi[it]) + offset) * 16);
    if ((address & 0x4000) != 0)
    {
        Fail("SQ to VU1's registers", address);
    }

    u8* to = data + (address & (DataBytes - 1));
    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            std::memcpy(to + field * 4, &vf[fs].v[field], 4);
        }
    }
}

void Vu0::Iaddiu(u32 it, u32 is, u32 immediate)
{
    if (it != 0)
    {
        vi[it] = static_cast<u16>(vi[is] + immediate);
    }
}

void Vu0::Isubiu(u32 it, u32 is, u32 immediate)
{
    if (it != 0)
    {
        vi[it] = static_cast<u16>(vi[is] - immediate);
    }
}

void Vu0::Iaddi(u32 it, u32 is, s32 immediate)
{
    if (it != 0)
    {
        vi[it] = static_cast<u16>(vi[is] + immediate);
    }
}

void Vu0::Iadd(u32 id, u32 is, u32 it)
{
    if (id != 0)
    {
        vi[id] = static_cast<u16>(vi[is] + vi[it]);
    }
}

void Vu0::Isub(u32 id, u32 is, u32 it)
{
    if (id != 0)
    {
        vi[id] = static_cast<u16>(vi[is] - vi[it]);
    }
}

void Vu0::Iand(u32 id, u32 is, u32 it)
{
    if (id != 0)
    {
        vi[id] = static_cast<u16>(vi[is] & vi[it]);
    }
}

void Vu0::Ior(u32 id, u32 is, u32 it)
{
    if (id != 0)
    {
        vi[id] = static_cast<u16>(vi[is] | vi[it]);
    }
}

void Vu0::Fmand(u32 it, u32 is, u32 macFlags)
{
    if (it != 0)
    {
        vi[it] = static_cast<u16>(vi[is] & (macFlags & 0xFFFF));
    }
}

void Vu0::Rinit(u32 fs, ::NativeMath::Field fsf)
{
    r = One | (vf[fs].v[fsf] & 0x007FFFFFu);
}

void Vu0::Rget(u32 dest, u32 ft)
{
    if (ft == 0)
    {
        return;
    }

    for (u32 field = 0; field < 4; field++)
    {
        if (Writes(dest, field))
        {
            vf[ft].v[field] = r;
        }
    }
}

void Vu0ProgramsLoaded(u32 set)
{
    const MicrocodeSetLoads* loads = MicrocodeSetLoadsOf(set);
    if (loads == nullptr)
    {
        Fail("no such microcode set", set);
    }

    for (u32 load = 0; load < loads->count; load++)
    {
        for (u32 instruction = loads->ranges[load].first; instruction < loads->ranges[load].end; instruction++)
        {
            s_MicroOwner[instruction] = static_cast<u8>(set);
        }
    }

    s_OwnersKnown = true;
}

void Vu0CallMicroprogram(u32 address)
{
    const Microprogram* program = FindMicroprogram(address);
    if (program == nullptr)
    {
        Fail("no translation of the microprogram at", address);
    }

    if (s_OwnersKnown)
    {
        // Every instruction the program can run should be one of the sets' whose code there is the translated one. The retail
        // game calls a program with another set loaded in one place: the chunks' loading (BackgroundWork, after the frame's draw
        // left the culling set loaded) makes instances whose follow camera restarts and casts its view with the standard set's
        // ray-triangle test (0xAB8), so the PS2 runs the culling set's code there and gets whatever it gives. The native build
        // runs the program the call means (the right test) and says so once a program (NATIVE.md, "VU0 programs called with
        // another set loaded")
        for (u32 range = 0; range < program->rangeCount; range++)
        {
            for (u32 instruction = program->ranges[range].first; instruction < program->ranges[range].end; instruction++)
            {
                if (((program->sets >> s_MicroOwner[instruction]) & 1) == 0)
                {
                    static u32 reported[64];
                    static u32 reportedCount = 0;
                    bool known = false;
                    for (u32 i = 0; i < reportedCount; i++)
                    {
                        known = known || reported[i] == address;
                    }

                    if (!known && reportedCount < 64)
                    {
                        reported[reportedCount++] = address;
                        std::fprintf(stderr,
                                     "VU0: the microprogram 0x%X called with set %u's code at instruction 0x%X (the program's sets "
                                     "0x%X): run as translated\n",
                                     address, s_MicroOwner[instruction], instruction, program->sets);
                    }

                    range = program->rangeCount;
                    break;
                }
            }
        }
    }

    program->run(TheVu0());
}
}
