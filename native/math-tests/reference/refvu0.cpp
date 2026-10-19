// The reference VU0: floats on the host FPU rounding towards zero (PCSX2's settings), the instructions decoded from their words
// and run with PCSX2's VU0 interpreter's pipeline timing. Kept apart from the native implementation on purpose: nothing here is
// shared with src/platform/native/math/

#include "refvu0.h"

#include "microcode.h"

#include <algorithm>
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#pragma STDC FENV_ACCESS ON

namespace Ref
{
namespace
{
Vu s_Vu;

float F(u32 bits)
{
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}

u32 B(float value)
{
    u32 bits;
    std::memcpy(&bits, &value, 4);
    return bits;
}

// PCSX2's vuDouble (clamping on, its default)
float VuDouble(u32 f)
{
    switch (f & 0x7F800000)
    {
    case 0:
        return F(f & 0x80000000);
    case 0x7F800000:
        return F((f & 0x80000000) | 0x7F7FFFFF);
    }
    return F(f);
}

// The host FPU rounding towards zero for one operation; its results' denormals flushed to zero as PCSX2's FTZ does
struct TowardsZero
{
    TowardsZero()
    {
        std::fesetround(FE_TOWARDZERO);
    }
    ~TowardsZero()
    {
        std::fesetround(FE_TONEAREST);
    }
};

float Ftz(float value)
{
    u32 bits = B(value);
    if ((bits & 0x7F800000) == 0)
    {
        return F(bits & 0x80000000);
    }
    return value;
}

[[noreturn]] void Die(const char* what, u32 value)
{
    std::fprintf(stderr, "reference VU0: %s %08X\n", what, value);
    std::abort();
}
}

Vu& TheVu()
{
    return s_Vu;
}

u32 Add(u32 a, u32 b)
{
    TowardsZero zero;
    volatile float x = VuDouble(a);
    volatile float y = VuDouble(b);
    volatile float r = x + y;
    return B(Ftz(r));
}

u32 Sub(u32 a, u32 b)
{
    TowardsZero zero;
    volatile float x = VuDouble(a);
    volatile float y = VuDouble(b);
    volatile float r = x - y;
    return B(Ftz(r));
}

u32 Mul(u32 a, u32 b)
{
    TowardsZero zero;
    volatile float x = VuDouble(a);
    volatile float y = VuDouble(b);
    volatile float r = x * y;
    return B(Ftz(r));
}

u32 Madd(u32 acc, u32 a, u32 b)
{
    TowardsZero zero;
    volatile float x = VuDouble(a);
    volatile float y = VuDouble(b);
    volatile float product = x * y;
    volatile float flushed = Ftz(product);
    volatile float c = VuDouble(acc);
    volatile float r = c + flushed;
    return B(Ftz(r));
}

u32 Msub(u32 acc, u32 a, u32 b)
{
    TowardsZero zero;
    volatile float x = VuDouble(a);
    volatile float y = VuDouble(b);
    volatile float product = x * y;
    volatile float flushed = Ftz(product);
    volatile float c = VuDouble(acc);
    volatile float r = c - flushed;
    return B(Ftz(r));
}

// _vuDIV
u32 Div(u32 fsBits, u32 ftBits)
{
    TowardsZero zero;
    volatile float ft = VuDouble(ftBits);
    volatile float fs = VuDouble(fsBits);
    if (ft == 0.0f)
    {
        return ((ftBits ^ fsBits) & 0x80000000) ? 0xFF7FFFFF : 0x7F7FFFFF;
    }
    volatile float q = fs / ft;
    return B(VuDouble(B(Ftz(q))));
}

// _vuSQRT
u32 Sqrt(u32 ftBits)
{
    TowardsZero zero;
    volatile float ft = VuDouble(ftBits);
    volatile float q = std::sqrt(std::fabs(static_cast<float>(ft)));
    return B(VuDouble(B(Ftz(q))));
}

// _vuRSQRT
u32 Rsqrt(u32 fsBits, u32 ftBits)
{
    TowardsZero zero;
    volatile float ft = VuDouble(ftBits);
    volatile float fs = VuDouble(fsBits);
    if (ft == 0.0f)
    {
        bool negative = ((ftBits ^ fsBits) & 0x80000000) != 0;
        if (fs != 0.0f)
        {
            return negative ? 0xFF7FFFFF : 0x7F7FFFFF;
        }
        return negative ? 0x80000000 : 0;
    }
    volatile float temp = std::sqrt(std::fabs(static_cast<float>(ft)));
    volatile float q = fs / Ftz(temp);
    return B(VuDouble(B(Ftz(q))));
}

// RSQRT.S as PCSX2's recompiler has it (recRSQRThelper1): ft zero (denormals too, DAZ) the largest float of fs's sign, else
// sqrt(|ft|) and fs / it rounding towards zero; operands with an exponent of 255 clamped first (fpuDouble)
u32 FpuRsqrt(u32 fsBits, u32 ftBits)
{
    if ((ftBits & 0x7F800000) == 0)
    {
        return (fsBits & 0x80000000) | 0x7F7FFFFF;
    }
    TowardsZero zero;
    volatile float t = std::sqrt(std::fabs(VuDouble(ftBits)));
    volatile float r = VuDouble(fsBits) / t;
    return B(Ftz(r));
}

u32 FpMax(u32 a, u32 b)
{
    return (static_cast<s32>(a) < 0 && static_cast<s32>(b) < 0) ? static_cast<u32>(std::min<s32>(a, b))
                                                                  : static_cast<u32>(std::max<s32>(a, b));
}

u32 FpMin(u32 a, u32 b)
{
    return (static_cast<s32>(a) < 0 && static_cast<s32>(b) < 0) ? static_cast<u32>(std::max<s32>(a, b))
                                                                  : static_cast<u32>(std::min<s32>(a, b));
}

// MAX.S and MIN.S as PCSX2's recompiler runs them: fpuFloat2 (infinities and NaNs clamped by sign), MAXSS/MINSS with DAZ
u32 FpuMax(u32 a, u32 b)
{
    return FpMax(B(VuDouble(a)), B(VuDouble(b)));
}

u32 FpuMin(u32 a, u32 b)
{
    return FpMin(B(VuDouble(a)), B(VuDouble(b)));
}

namespace
{
// ------------------------------------------------------------------------------------------------------------------------------
// The instructions' semantics (PCSX2's), on an instruction word. Macro mode's instructions are the upper ones' words (with the
// lower ones' DIV, SQRT, WAITQ, MOVE, MR32 the macro assembler makes in refasm.cpp)

u32 s_QStaged;  // PCSX2's VU->q (what DIV computed, written to Q when the FDIV pipeline is done)

u32 MacUpdate(int shift, u32 v)
{
    float f = F(v);
    u32 s = v & 0x80000000;
    u32& mac = s_Vu.macflag;
    if (s)
        mac |= 0x0010u << shift;
    else
        mac &= ~(0x0010u << shift);
    if (f == 0)
    {
        mac = (mac & ~(0x1100u << shift)) | (0x0001u << shift);
        return v;
    }
    switch ((v >> 23) & 0xFF)
    {
    case 0:
        mac = (mac & ~(0x1000u << shift)) | (0x0101u << shift);
        return s;
    case 255:
        mac = (mac & ~(0x0101u << shift)) | (0x1000u << shift);
        return s | 0x7F7FFFFF;
    default:
        mac = (mac & ~(0x1101u << shift));
        return v;
    }
}

void MacClear(int shift)
{
    s_Vu.macflag &= ~(0x1111u << shift);
}

u32 s_Discard[4];

u32* Dst(u32 fd, bool toAcc)
{
    if (toAcc)
        return s_Vu.acc;
    if (fd == 0)
        return s_Discard;
    return s_Vu.vf[fd];
}

enum Arith
{
    AAdd,
    ASub,
    AMul,
    AMadd,
    AMsub,
    AMax,
    AMin,
};

u32 Compute(Arith op, u32 acc, u32 a, u32 b)
{
    switch (op)
    {
    case AAdd:
        return Add(a, b);
    case ASub:
        return Sub(a, b);
    case AMul:
        return Mul(a, b);
    case AMadd:
        return Madd(acc, a, b);
    case AMsub:
        return Msub(acc, a, b);
    case AMax:
        return FpMax(a, b);
    case AMin:
        return FpMin(a, b);
    }
    return 0;
}

// An FMAC instruction: kind 0 vector ft, 1 broadcast field, 2 I, 3 Q
void Fmac(Arith op, bool toAcc, u32 dest, u32 fd, u32 fs, u32 ft, int kind, u32 bc)
{
    bool minMax = op == AMax || op == AMin;
    if (minMax && fd == 0)
        return;
    u32 broadcast = kind == 1 ? s_Vu.vf[ft][bc] : kind == 2 ? s_Vu.i : s_Vu.q;
    u32* dst = Dst(fd, toAcc);
    for (int field = 0; field < 4; field++)
    {
        int shift = 3 - field;
        if ((dest >> shift) & 1)
        {
            u32 b = kind == 0 ? s_Vu.vf[ft][field] : broadcast;
            u32 value = Compute(op, s_Vu.acc[field], s_Vu.vf[fs][field], b);
            dst[field] = minMax ? value : MacUpdate(shift, value);
        }
        else if (!minMax)
        {
            MacClear(shift);
        }
    }
}

void ExecUpper(u32 code)
{
    u32 ft = (code >> 16) & 31;
    u32 fs = (code >> 11) & 31;
    u32 fd = (code >> 6) & 31;
    u32 dest = (code >> 21) & 15;
    u32 op = code & 63;
    static const Arith groups[] = {AAdd, ASub, AMadd, AMsub, AMax, AMin, AMul};
    if (op < 0x1C)
    {
        Fmac(groups[op >> 2], false, dest, fd, fs, ft, 1, op & 3);
        return;
    }
    switch (op)
    {
    case 0x1C: Fmac(AMul, false, dest, fd, fs, ft, 3, 0); return;
    case 0x1D: Fmac(AMax, false, dest, fd, fs, ft, 2, 0); return;
    case 0x1E: Fmac(AMul, false, dest, fd, fs, ft, 2, 0); return;
    case 0x1F: Fmac(AMin, false, dest, fd, fs, ft, 2, 0); return;
    case 0x20: Fmac(AAdd, false, dest, fd, fs, ft, 3, 0); return;
    case 0x21: Fmac(AMadd, false, dest, fd, fs, ft, 3, 0); return;
    case 0x22: Fmac(AAdd, false, dest, fd, fs, ft, 2, 0); return;
    case 0x23: Fmac(AMadd, false, dest, fd, fs, ft, 2, 0); return;
    case 0x24: Fmac(ASub, false, dest, fd, fs, ft, 3, 0); return;
    case 0x25: Fmac(AMsub, false, dest, fd, fs, ft, 3, 0); return;
    case 0x26: Fmac(ASub, false, dest, fd, fs, ft, 2, 0); return;
    case 0x27: Fmac(AMsub, false, dest, fd, fs, ft, 2, 0); return;
    case 0x28: Fmac(AAdd, false, dest, fd, fs, ft, 0, 0); return;
    case 0x29: Fmac(AMadd, false, dest, fd, fs, ft, 0, 0); return;
    case 0x2A: Fmac(AMul, false, dest, fd, fs, ft, 0, 0); return;
    case 0x2B: Fmac(AMax, false, dest, fd, fs, ft, 0, 0); return;
    case 0x2C: Fmac(ASub, false, dest, fd, fs, ft, 0, 0); return;
    case 0x2D: Fmac(AMsub, false, dest, fd, fs, ft, 0, 0); return;
    case 0x2F: Fmac(AMin, false, dest, fd, fs, ft, 0, 0); return;
    case 0x2E:
    {
        // _vuOPMSUB: every operand read first
        u32 t[3] = {s_Vu.vf[ft][0], s_Vu.vf[ft][1], s_Vu.vf[ft][2]};
        u32 s[3] = {s_Vu.vf[fs][0], s_Vu.vf[fs][1], s_Vu.vf[fs][2]};
        u32* dst = Dst(fd, false);
        dst[0] = MacUpdate(3, Msub(s_Vu.acc[0], s[1], t[2]));
        dst[1] = MacUpdate(2, Msub(s_Vu.acc[1], s[2], t[0]));
        dst[2] = MacUpdate(1, Msub(s_Vu.acc[2], s[0], t[1]));
        return;
    }
    }
    if (op < 0x3C)
        Die("upper", code);
    u32 special = (((code >> 6) & 31) << 2) | (code & 3);
    if (special < 0x10 || (special >= 0x18 && special < 0x1C))
    {
        Fmac(groups[special >> 2], true, dest, 0, fs, ft, 1, special & 3);
        return;
    }
    switch (special)
    {
    case 0x1C: Fmac(AMul, true, dest, 0, fs, ft, 3, 0); return;
    case 0x1E: Fmac(AMul, true, dest, 0, fs, ft, 2, 0); return;
    case 0x20: Fmac(AAdd, true, dest, 0, fs, ft, 3, 0); return;
    case 0x21: Fmac(AMadd, true, dest, 0, fs, ft, 3, 0); return;
    case 0x22: Fmac(AAdd, true, dest, 0, fs, ft, 2, 0); return;
    case 0x23: Fmac(AMadd, true, dest, 0, fs, ft, 2, 0); return;
    case 0x24: Fmac(ASub, true, dest, 0, fs, ft, 3, 0); return;
    case 0x25: Fmac(AMsub, true, dest, 0, fs, ft, 3, 0); return;
    case 0x26: Fmac(ASub, true, dest, 0, fs, ft, 2, 0); return;
    case 0x27: Fmac(AMsub, true, dest, 0, fs, ft, 2, 0); return;
    case 0x28: Fmac(AAdd, true, dest, 0, fs, ft, 0, 0); return;
    case 0x29: Fmac(AMadd, true, dest, 0, fs, ft, 0, 0); return;
    case 0x2A: Fmac(AMul, true, dest, 0, fs, ft, 0, 0); return;
    case 0x2C: Fmac(ASub, true, dest, 0, fs, ft, 0, 0); return;
    case 0x2D: Fmac(AMsub, true, dest, 0, fs, ft, 0, 0); return;
    case 0x2E:
    {
        // _vuOPMULA (no w flags touched)
        u32 x = Mul(s_Vu.vf[fs][1], s_Vu.vf[ft][2]);
        u32 y = Mul(s_Vu.vf[fs][2], s_Vu.vf[ft][0]);
        u32 z = Mul(s_Vu.vf[fs][0], s_Vu.vf[ft][1]);
        s_Vu.acc[0] = MacUpdate(3, x);
        s_Vu.acc[1] = MacUpdate(2, y);
        s_Vu.acc[2] = MacUpdate(1, z);
        return;
    }
    case 0x1D:
        if (ft == 0)
            return;
        for (int field = 0; field < 4; field++)
            if ((dest >> (3 - field)) & 1)
                s_Vu.vf[ft][field] = s_Vu.vf[fs][field] & 0x7FFFFFFF;
        return;
    case 0x2F:
        return;
    case 0x1F:
    {
        s32 value = static_cast<s32>(s_Vu.vf[ft][3]);
        value = (value & 0x7F800000) ? (value & 0x7FFFFFFF) : 0x007FFFFF;
        u32 flags = s_Vu.clipflag << 6;
        for (int field = 0; field < 3; field++)
        {
            if (static_cast<s32>(s_Vu.vf[fs][field]) > value)
                flags |= 1u << (field * 2);
            if (static_cast<s32>(s_Vu.vf[fs][field] ^ 0x80000000) > value)
                flags |= 2u << (field * 2);
        }
        s_Vu.clipflag = flags & 0xFFFFFF;
        return;
    }
    }
    if (special >= 0x10 && special < 0x18)
    {
        static const u32 shifts[] = {0, 4, 12, 15};
        u32 offset = shifts[special & 3];
        if (ft == 0)
            return;
        for (int field = 0; field < 4; field++)
        {
            if (!((dest >> (3 - field)) & 1))
                continue;
            u32 value = s_Vu.vf[fs][field];
            if (special < 0x14)
            {
                // intToFloat: the integer's float (rounding towards zero), times 2^-offset
                TowardsZero zero;
                volatile float f = static_cast<float>(static_cast<s32>(value));
                if (offset)
                    f = f * F(0x3F800000 - (offset << 23));
                s_Vu.vf[ft][field] = B(f);
            }
            else
            {
                // floatToInt (the host's DAZ: a denormal operand is a zero)
                u32 operand = (value & 0x7F800000) == 0 ? (value & 0x80000000) : value;
                volatile float f = F(operand);
                if (offset)
                {
                    TowardsZero zero;
                    f = Ftz(f * F(0x3F800000 + (offset << 23)));
                }
                u32 u = B(f);
                if ((u & 0x7F800000) >= 0x4F000000)
                    s_Vu.vf[ft][field] = (u & 0x80000000) ? 0x80000000 : 0x7FFFFFFF;
                else
                    s_Vu.vf[ft][field] = static_cast<u32>(static_cast<s32>(F(u)));
            }
        }
        return;
    }
    Die("upper special", code);
}

int Imm11(u32 code)
{
    return (code & 0x400) ? static_cast<int>(code & 0x3FF) - 0x400 : static_cast<int>(code & 0x3FF);
}

// PCSX2's VI backup (the old value a branch right after an integer instruction reads)
int s_BackupCycles;
u32 s_BackupReg;
u16 s_BackupOld;
int s_Branch;
u32 s_BranchPc;
u32 s_Pc;

void BackupVi(u32 reg)
{
    if (s_BackupCycles && reg == s_BackupReg)
    {
        s_BackupCycles = 2;
        return;
    }
    s_BackupCycles = 2;
    s_BackupReg = reg;
    s_BackupOld = s_Vu.vi[reg];
}

s16 ViForBranch(u32 reg)
{
    if (s_BackupCycles > 0 && s_BackupReg == reg)
        return static_cast<s16>(s_BackupOld);
    return static_cast<s16>(s_Vu.vi[reg]);
}

u32* Mem(u32 address)
{
    if (address & 0x4000)
        Die("VU1 registers", address);
    return reinterpret_cast<u32*>(s_Vu.data + (address & 0xFFF));
}

void ExecLower(u32 code)
{
    u32 ft = (code >> 16) & 31;
    u32 fs = (code >> 11) & 31;
    u32 fd = (code >> 6) & 31;
    u32 it = ft & 15, is = fs & 15, id = fd & 15;
    u32 dest = (code >> 21) & 15;
    u32 fsf = (code >> 21) & 3;
    u32 ftf = (code >> 23) & 3;
    u32 top = code >> 25;
    auto setBranch = [&]() {
        s_Branch = 2;
        s_BranchPc = (s_Pc + Imm11(code) * 8) & 0xFFF;
    };
    if (top == 0x40)
    {
        u32 low = code & 63;
        switch (low)
        {
        case 0x30: if (id) { BackupVi(id); s_Vu.vi[id] = s_Vu.vi[is] + s_Vu.vi[it]; } return;
        case 0x31: if (id) { BackupVi(id); s_Vu.vi[id] = s_Vu.vi[is] - s_Vu.vi[it]; } return;
        case 0x32:
        {
            s16 imm = (code >> 6) & 0x1F;
            imm = static_cast<s16>((imm & 0x10 ? 0xFFF0 : 0) | (imm & 0xF));
            if (it) { BackupVi(it); s_Vu.vi[it] = static_cast<u16>(static_cast<s16>(s_Vu.vi[is]) + imm); }
            return;
        }
        case 0x34: if (id) { BackupVi(id); s_Vu.vi[id] = s_Vu.vi[is] & s_Vu.vi[it]; } return;
        case 0x35: if (id) { BackupVi(id); s_Vu.vi[id] = s_Vu.vi[is] | s_Vu.vi[it]; } return;
        }
        if (low < 0x3C)
            Die("lower", code);
        u32 key = ((code & 3) << 5) | ((code >> 6) & 31);
        switch (key)
        {
        case (0 << 5) | 0x0C: // MOVE
            if (ft == 0) return;
            for (int f = 0; f < 4; f++) if ((dest >> (3 - f)) & 1) s_Vu.vf[ft][f] = s_Vu.vf[fs][f];
            return;
        case (1 << 5) | 0x0C: // MR32
        {
            if (ft == 0) return;
            u32 tx = s_Vu.vf[fs][0];
            if (dest & 8) s_Vu.vf[ft][0] = s_Vu.vf[fs][1];
            if (dest & 4) s_Vu.vf[ft][1] = s_Vu.vf[fs][2];
            if (dest & 2) s_Vu.vf[ft][2] = s_Vu.vf[fs][3];
            if (dest & 1) s_Vu.vf[ft][3] = tx;
            return;
        }
        case (0 << 5) | 0x0E: s_QStaged = Div(s_Vu.vf[fs][fsf], s_Vu.vf[ft][ftf]); return;
        case (1 << 5) | 0x0E: s_QStaged = Sqrt(s_Vu.vf[ft][ftf]); return;
        case (2 << 5) | 0x0E: s_QStaged = Rsqrt(s_Vu.vf[fs][fsf], s_Vu.vf[ft][ftf]); return;
        case (3 << 5) | 0x0E: return; // WAITQ
        case (3 << 5) | 0x0B: return; // NOP
        case (2 << 5) | 0x10: s_Vu.r = 0x3F800000 | (s_Vu.vf[fs][fsf] & 0x007FFFFF); return; // RINIT
        case (1 << 5) | 0x10: // RGET
            if (ft == 0) return;
            for (int f = 0; f < 4; f++) if ((dest >> (3 - f)) & 1) s_Vu.vf[ft][f] = s_Vu.r;
            return;
        case (0 << 5) | 0x0F: // MTIR
            if (it == 0) return;
            BackupVi(it);
            s_Vu.vi[it] = static_cast<u16>(s_Vu.vf[fs][fsf]);
            return;
        case (1 << 5) | 0x0F: // MFIR
            if (ft == 0) return;
            for (int f = 0; f < 4; f++) if ((dest >> (3 - f)) & 1) s_Vu.vf[ft][f] = static_cast<u32>(static_cast<s32>(static_cast<s16>(s_Vu.vi[is])));
            return;
        }
        Die("lower special", code);
    }
    switch (top)
    {
    case 0x00: // LQ
    {
        if (ft == 0) return;
        u16 address = static_cast<u16>((Imm11(code) + static_cast<s16>(s_Vu.vi[is])) * 16);
        u32* p = Mem(address);
        for (int f = 0; f < 4; f++) if ((dest >> (3 - f)) & 1) s_Vu.vf[ft][f] = p[f];
        return;
    }
    case 0x01: // SQ
    {
        u16 address = static_cast<u16>((Imm11(code) + static_cast<s16>(s_Vu.vi[it])) * 16);
        u32* p = Mem(address);
        for (int f = 0; f < 4; f++) if ((dest >> (3 - f)) & 1) p[f] = s_Vu.vf[fs][f];
        return;
    }
    case 0x08:
        if (it) { BackupVi(it); s_Vu.vi[it] = static_cast<u16>(s_Vu.vi[is] + (((code >> 10) & 0x7800) | (code & 0x7FF))); }
        return;
    case 0x09:
        if (it) { BackupVi(it); s_Vu.vi[it] = static_cast<u16>(s_Vu.vi[is] - (((code >> 10) & 0x7800) | (code & 0x7FF))); }
        return;
    case 0x1A: // FMAND
        if (it == 0) return;
        s_Vu.vi[it] = s_Vu.vi[is] & (s_Vu.macVisible & 0xFFFF);
        return;
    case 0x20: setBranch(); return;
    case 0x28: if (ViForBranch(it) == ViForBranch(is)) setBranch(); return;
    case 0x29: if (ViForBranch(it) != ViForBranch(is)) setBranch(); return;
    case 0x2C: if (ViForBranch(is) < 0) setBranch(); return;
    case 0x2D: if (ViForBranch(is) > 0) setBranch(); return;
    case 0x2E: if (ViForBranch(is) <= 0) setBranch(); return;
    case 0x2F: if (ViForBranch(is) >= 0) setBranch(); return;
    }
    Die("lower", code);
}

// ------------------------------------------------------------------------------------------------------------------------------
// The timing: PCSX2's _VURegsNum of an instruction and its pipelines

enum Pipe
{
    PNone,
    PFmac,
    PFdiv,
    PIalu,
    PBranch,
};

struct Regs
{
    Pipe pipe = PNone;
    u32 vfWrite = 0, vfWriteMask = 0;
    u32 read0 = 0, read0Mask = 0, read1 = 0, read1Mask = 0;
    int cycles = 0;
    bool writesQ = false;
};

Regs UpperRegs(u32 code)
{
    Regs r;
    u32 ft = (code >> 16) & 31, fs = (code >> 11) & 31, fd = (code >> 6) & 31, dest = (code >> 21) & 15;
    u32 op = code & 63;
    r.pipe = PFmac;
    auto bcMask = [](u32 bc) { return 8u >> bc; };
    if (op < 0x1C)
    {
        r.vfWrite = fd; r.vfWriteMask = dest; r.read0 = fs; r.read0Mask = dest; r.read1 = ft; r.read1Mask = bcMask(op & 3);
        return r;
    }
    if (op < 0x28)
    {
        r.vfWrite = fd; r.vfWriteMask = dest; r.read0 = fs; r.read0Mask = dest;
        return r;
    }
    if (op < 0x3C)
    {
        r.vfWrite = fd; r.read0 = fs; r.read1 = ft;
        r.vfWriteMask = r.read0Mask = r.read1Mask = (op == 0x2E) ? 0xE : dest;
        return r;
    }
    u32 special = (((code >> 6) & 31) << 2) | (code & 3);
    if (special < 0x10 || (special >= 0x18 && special < 0x1C))
    {
        r.read0 = fs; r.read0Mask = dest; r.read1 = ft; r.read1Mask = bcMask(special & 3);
        return r;
    }
    if (special >= 0x10 && special < 0x18)
    {
        r.vfWrite = ft; r.vfWriteMask = dest; r.read0 = fs; r.read0Mask = dest;
        return r;
    }
    switch (special)
    {
    case 0x1C: case 0x1E: case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
        r.read0 = fs; r.read0Mask = dest;
        return r;
    case 0x28: case 0x29: case 0x2A: case 0x2C: case 0x2D:
        r.read0 = fs; r.read0Mask = dest; r.read1 = ft; r.read1Mask = dest;
        return r;
    case 0x2E:
        r.read0 = fs; r.read0Mask = 0xE; r.read1 = ft; r.read1Mask = 0xE;
        return r;
    case 0x1D:
        r.vfWrite = ft; r.vfWriteMask = dest; r.read0 = fs; r.read0Mask = dest;
        return r;
    case 0x1F:
        r.read0 = fs; r.read0Mask = 0xE; r.read1 = ft; r.read1Mask = 1;
        return r;
    case 0x2F:
        r.pipe = PNone;
        return r;
    }
    Die("upper regs", code);
}

Regs LowerRegs(u32 code)
{
    Regs r;
    u32 ft = (code >> 16) & 31, fs = (code >> 11) & 31, dest = (code >> 21) & 15;
    u32 fsf = (code >> 21) & 3, ftf = (code >> 23) & 3;
    u32 top = code >> 25;
    auto bcMask = [](u32 bc) { return 8u >> bc; };
    if (top == 0x40)
    {
        u32 low = code & 63;
        if (low == 0x30 || low == 0x31 || low == 0x32 || low == 0x34 || low == 0x35)
        {
            r.pipe = PIalu;
            return r;
        }
        u32 key = ((code & 3) << 5) | ((code >> 6) & 31);
        switch (key)
        {
        case (0 << 5) | 0x0C:
            r.pipe = ft == 0 ? PNone : PFmac; r.vfWrite = ft; r.vfWriteMask = dest; r.read0 = fs; r.read0Mask = dest;
            return r;
        case (1 << 5) | 0x0C:
            r.pipe = PFmac; r.vfWrite = ft; r.vfWriteMask = dest; r.read0 = fs; r.read0Mask = (dest >> 1) | ((dest << 3) & 8);
            return r;
        case (0 << 5) | 0x0E: case (2 << 5) | 0x0E:
            r.pipe = PFdiv; r.read0 = fs; r.read0Mask = bcMask(fsf); r.read1 = ft; r.read1Mask = bcMask(ftf);
            r.writesQ = true; r.cycles = key == ((0 << 5) | 0x0E) ? 7 : 13;
            return r;
        case (1 << 5) | 0x0E:
            r.pipe = PFdiv; r.read1 = ft; r.read1Mask = bcMask(ftf); r.writesQ = true; r.cycles = 7;
            return r;
        case (3 << 5) | 0x0E:
            r.pipe = PFdiv;
            return r;
        case (3 << 5) | 0x0B:
            return r;
        case (2 << 5) | 0x10:
            r.pipe = PFmac; r.read0 = fs; r.read0Mask = bcMask(fsf);
            return r;
        case (1 << 5) | 0x10:
            r.pipe = PFmac; r.vfWrite = ft; r.vfWriteMask = dest;
            return r;
        case (0 << 5) | 0x0F:
            r.pipe = PFmac; r.read0 = fs; r.read0Mask = bcMask(fsf);
            return r;
        case (1 << 5) | 0x0F:
            r.pipe = PFmac; r.vfWrite = ft; r.vfWriteMask = dest;
            return r;
        }
        Die("lower regs", code);
    }
    switch (top)
    {
    case 0x00: r.pipe = PFmac; r.vfWrite = ft; r.vfWriteMask = dest; return r;
    case 0x01: r.pipe = PFmac; r.read0 = fs; r.read0Mask = dest; return r;
    case 0x08: case 0x09: r.pipe = PIalu; return r;
    case 0x1A: r.pipe = PFmac; return r;
    case 0x20: case 0x28: case 0x29: case 0x2C: case 0x2D: case 0x2E: case 0x2F: r.pipe = PBranch; return r;
    }
    Die("lower regs", code);
}

struct FmacEntry
{
    u64 start;
    int latency;
    u32 regUpper, maskUpper, regLower, maskLower;
    u32 mac;
};

FmacEntry s_Fmac[4];
int s_FmacRead, s_FmacWrite, s_FmacCount;
bool s_FdivEnabled;
u64 s_FdivStart;
int s_FdivLatency;
u32 s_FdivValue;
u64 s_Cycle;
int s_Ebit;

void FmacStall(u32 reg, u32 mask)
{
    int pipe = s_FmacRead;
    for (int n = 0; n < s_FmacCount; n++, pipe = (pipe + 1) & 3)
    {
        const FmacEntry& e = s_Fmac[pipe];
        if (s_Cycle - e.start >= static_cast<u64>(e.latency))
            continue;
        if ((e.regUpper == reg && (e.maskUpper & mask)) || (e.regLower == reg && (e.maskLower & mask)))
        {
            u64 newCycle = e.start + e.latency;
            if (newCycle > s_Cycle)
                s_Cycle = newCycle;
        }
    }
}

void FmacStalls(const Regs& r)
{
    if (r.read0)
        FmacStall(r.read0, r.read0Mask);
    if (r.read1)
        FmacStall(r.read1, r.read1Mask);
}

void TestPipes()
{
    bool flushed;
    do
    {
        flushed = false;
        while (s_FmacCount > 0)
        {
            FmacEntry& e = s_Fmac[s_FmacRead];
            if (s_Cycle - e.start < static_cast<u64>(e.latency))
                break;
            s_Vu.macVisible = e.mac;
            s_FmacRead = (s_FmacRead + 1) & 3;
            s_FmacCount--;
            flushed = true;
        }
        if (s_FdivEnabled && s_Cycle - s_FdivStart >= static_cast<u64>(s_FdivLatency))
        {
            s_FdivEnabled = false;
            s_Vu.q = s_FdivValue;
            flushed = true;
        }
    } while (flushed);
}

void FlushAll()
{
    if (s_FdivEnabled)
    {
        s_FdivEnabled = false;
        s_Vu.q = s_FdivValue;
    }
    while (s_FmacCount > 0)
    {
        s_Vu.macVisible = s_Fmac[s_FmacRead].mac;
        s_FmacRead = (s_FmacRead + 1) & 3;
        s_FmacCount--;
    }
}

// One instruction pair, as PCSX2's _vu0Exec. Whether the program goes on
bool Step()
{
    s_Cycle++;
    u64 word = s_Vu.micro[(s_Pc & 0xFFF) / 8];
    u32 lower = static_cast<u32>(word);
    u32 upper = static_cast<u32>(word >> 32);
    s_Pc = (s_Pc + 8) & 0xFFF;
    s_Vu.executed++;
    if (upper & 0x40000000)
        s_Ebit = 2;
    if (upper & 0x38000000)
        Die("M, D or T bit", upper);
    Regs ur = UpperRegs(upper);
    Regs lr;
    u64 before = s_Cycle - 1;
    if (ur.pipe == PFmac)
        FmacStalls(ur);
    if (upper & 0x80000000)
    {
        TestPipes();
        if (s_BackupCycles > 0)
            s_BackupCycles -= static_cast<int>(std::min<u64>(s_Cycle - before, static_cast<u64>(s_BackupCycles)));
        ExecUpper(upper);
        s_Vu.i = lower;
    }
    else
    {
        lr = LowerRegs(lower);
        if (lr.pipe == PFmac)
            FmacStalls(lr);
        else if (lr.pipe == PFdiv)
        {
            FmacStalls(lr);
            if (s_FdivEnabled && s_FdivStart + s_FdivLatency > s_Cycle)
                s_Cycle = s_FdivStart + s_FdivLatency;
        }
        TestPipes();
        if (s_BackupCycles > 0)
            s_BackupCycles -= static_cast<int>(std::min<u64>(s_Cycle - before, static_cast<u64>(s_BackupCycles)));
        bool discard = false;
        u32 saved = 0;
        u32 old[4];
        if (ur.vfWrite)
        {
            if (lr.vfWrite == ur.vfWrite)
                discard = true;
            if (lr.read0 == ur.vfWrite || lr.read1 == ur.vfWrite)
            {
                saved = ur.vfWrite;
                std::memcpy(old, s_Vu.vf[saved], 16);
            }
        }
        ExecUpper(upper);
        if (!discard)
        {
            u32 now[4];
            if (saved)
            {
                std::memcpy(now, s_Vu.vf[saved], 16);
                std::memcpy(s_Vu.vf[saved], old, 16);
            }
            ExecLower(lower);
            if (saved)
                std::memcpy(s_Vu.vf[saved], now, 16);
        }
    }
    if (ur.pipe == PFmac || lr.pipe == PFmac)
    {
        FmacEntry& e = s_Fmac[s_FmacWrite];
        e = {};
        s_FmacCount++;
        if (ur.pipe == PFmac)
        {
            e.start = s_Cycle;
            e.latency = 4;
            e.regUpper = ur.vfWrite;
            e.maskUpper = ur.vfWriteMask;
            e.mac = s_Vu.macflag;
        }
        if (lr.pipe == PFmac)
        {
            e.start = s_Cycle;
            e.latency = 4;
            e.regLower = lr.vfWrite;
            e.maskLower = lr.vfWriteMask;
            e.mac = s_Vu.macflag;
        }
    }
    if (lr.pipe == PFdiv && lr.writesQ)
    {
        s_FdivEnabled = true;
        s_FdivStart = s_Cycle;
        s_FdivLatency = lr.cycles;
        s_FdivValue = s_QStaged;
    }
    if (s_Branch > 0)
    {
        if (s_Branch-- == 1)
            s_Pc = s_BranchPc;
    }
    bool goesOn = true;
    if (s_Ebit > 0)
    {
        if (s_Ebit-- == 1)
        {
            s_BackupCycles = 0;
            FlushAll();
            goesOn = false;
        }
    }
    if (ur.pipe == PFmac || lr.pipe == PFmac)
        s_FmacWrite = (s_FmacWrite + 1) & 3;
    return goesOn;
}
}

void LoadMicrocodeSet(u32 set)
{
    for (const MicrocodeLoad& load : g_MicrocodeLoads)
    {
        if (load.set != set)
            continue;
        for (u32 n = 0; n < load.count; n++)
        {
            s_Vu.micro[load.address / 8 + n] = static_cast<u64>(load.words[n * 2]) | (static_cast<u64>(load.words[n * 2 + 1]) << 32);
        }
    }
}

void RunMicroprogram(u32 address)
{
    s_Pc = address & 0xFFF;
    s_Cycle = 1000;
    s_FmacRead = s_FmacWrite = s_FmacCount = 0;
    s_FdivEnabled = false;
    s_Branch = 0;
    s_Ebit = 0;
    s_BackupCycles = 0;
    s_Vu.executed = 0;
    while (Step())
    {
        if (s_Vu.executed > 100000)
            Die("runaway program", address);
    }
    s_Vu.cycles = s_Cycle - 1000;
}

// Macro mode: the instruction's word run at once, its results (flags, Q) there at once (PCSX2's SYNCMSFLAGS, SYNCFDIV)
void MacroUpper(u32 code)
{
    ExecUpper(code);
    s_Vu.macVisible = s_Vu.macflag;
}

void MacroLower(u32 code, bool writesQ)
{
    ExecLower(code);
    if (writesQ)
    {
        s_Vu.q = s_QStaged;
    }
}
}
