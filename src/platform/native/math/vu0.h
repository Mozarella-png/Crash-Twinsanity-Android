#pragma once

// VU0 for the native build's maths: its registers and its instructions, which the PS2 side's macro mode asm and the VU0
// microprograms it calls (vcallms) are translated onto an instruction at a time, in the PS2's order, so the native results are
// VU0's. The instructions' semantics are PCSX2's (pcsx2/VUops.cpp, the decomp's reference): see native/MATH.md.
//
// What macro mode and a microprogram's end leave is the architectural state: an instruction's results, its MAC flags and Q are
// there at once. Inside a microprogram the pipelines matter (flags 4 cycles late, Q 7 or 13 cycles after DIV, SQRT or RSQRT, a
// branch reading the old value of the integer register the instruction before it wrote, a lower instruction reading the old
// value of the register its upper instruction writes): the translations (microprograms.cpp) work those out ahead of time and
// say where each read gets its value, with the values kept in their own variables.

#include "common.h"

#include "vufloat.h"

namespace NativeMath
{
// The fields an instruction writes (its dest bits, x the highest as the encoding has them)
enum Dest : u32
{
    W = 1,
    Z = 2,
    ZW = 3,
    Y = 4,
    YW = 5,
    YZ = 6,
    YZW = 7,
    X = 8,
    XW = 9,
    XZ = 10,
    XZW = 11,
    XY = 12,
    XYW = 13,
    XYZ = 14,
    XYZW = 15,
};

// A broadcast field (the bc of ADDx, the fsf and ftf of DIV)
enum Field : u32
{
    Fx = 0,
    Fy = 1,
    Fz = 2,
    Fw = 3,
};

// A VF register's four floats as their bits
struct Vf
{
    u32 v[4];
};

class Vu0
{
public:
    // The VF registers, and two more past them that the microprograms' translations keep a register's old value in (a lower
    // instruction reads what its upper instruction's register had)
    static constexpr u32 Temporary = 32;
    static constexpr u32 SecondTemporary = 33;
    Vf vf[34];
    // The integer registers (16 bit)
    u16 vi[16];
    Vf acc;
    // Q (DIV, SQRT, RSQRT), I (LOI, CTC2 vi21), R (RINIT; its exponent 127), P (the EFU's)
    u32 q;
    u32 i;
    u32 r;
    u32 p;
    // The MAC flags: per field (x the highest) zero bits 0-3, sign 4-7, underflow 8-11, overflow 12-15
    u32 mac;
    // The clip flags: the last four CLIPs' six bits each, the latest in the low ones
    u32 clip;

    Vu0();

    // VU0's data memory (4 KB, addressed in quadwords that wrap round)
    u8* data;

    // Macro mode's moves between the EE and VU0 (LQC2, SQC2, QMTC2, QMFC2, CTC2, CFC2)
    void Lqc2(u32 ft, const void* address);
    void Sqc2(u32 fs, void* address) const;
    void Qmtc2(u32 fs, const u32* words);
    void Qmfc2(u32 fs, u32* words) const;
    // CTC2/CFC2 of vi01-vi15 and of I (21): the integer registers are 16 bit
    void Ctc2(u32 vi, u32 value);
    u32 Cfc2(u32 vi) const;

    // The FMAC instructions: fd (or ACC) = fs op ft, op ft's broadcast field (Bc), I or Q, per field of dest; MADD and MSUB
    // take ACC ± fs · ft. Each sets the MAC flags of the fields it writes and clears the others'
    void Add(u32 dest, u32 fd, u32 fs, u32 ft);
    void AddBc(u32 dest, u32 fd, u32 fs, u32 ft, Field bc);
    void AddI(u32 dest, u32 fd, u32 fs);
    void AddQ(u32 dest, u32 fd, u32 fs);
    void Sub(u32 dest, u32 fd, u32 fs, u32 ft);
    void SubBc(u32 dest, u32 fd, u32 fs, u32 ft, Field bc);
    void SubI(u32 dest, u32 fd, u32 fs);
    void SubQ(u32 dest, u32 fd, u32 fs);
    void Mul(u32 dest, u32 fd, u32 fs, u32 ft);
    void MulBc(u32 dest, u32 fd, u32 fs, u32 ft, Field bc);
    void MulI(u32 dest, u32 fd, u32 fs);
    void MulQ(u32 dest, u32 fd, u32 fs);
    void Madd(u32 dest, u32 fd, u32 fs, u32 ft);
    void MaddBc(u32 dest, u32 fd, u32 fs, u32 ft, Field bc);
    void MaddI(u32 dest, u32 fd, u32 fs);
    void MaddQ(u32 dest, u32 fd, u32 fs);
    void Msub(u32 dest, u32 fd, u32 fs, u32 ft);
    void MsubBc(u32 dest, u32 fd, u32 fs, u32 ft, Field bc);
    void MsubI(u32 dest, u32 fd, u32 fs);
    void MsubQ(u32 dest, u32 fd, u32 fs);
    void Adda(u32 dest, u32 fs, u32 ft);
    void AddaBc(u32 dest, u32 fs, u32 ft, Field bc);
    void AddaI(u32 dest, u32 fs);
    void AddaQ(u32 dest, u32 fs);
    void Suba(u32 dest, u32 fs, u32 ft);
    void SubaBc(u32 dest, u32 fs, u32 ft, Field bc);
    void SubaI(u32 dest, u32 fs);
    void SubaQ(u32 dest, u32 fs);
    void Mula(u32 dest, u32 fs, u32 ft);
    void MulaBc(u32 dest, u32 fs, u32 ft, Field bc);
    void MulaI(u32 dest, u32 fs);
    void MulaQ(u32 dest, u32 fs);
    void Madda(u32 dest, u32 fs, u32 ft);
    void MaddaBc(u32 dest, u32 fs, u32 ft, Field bc);
    void MaddaI(u32 dest, u32 fs);
    void MaddaQ(u32 dest, u32 fs);
    void Msuba(u32 dest, u32 fs, u32 ft);
    void MsubaBc(u32 dest, u32 fs, u32 ft, Field bc);
    void MsubaI(u32 dest, u32 fs);
    void MsubaQ(u32 dest, u32 fs);
    // ACC.xyz = fs.yzx · ft.zxy, and fd.xyz = ACC - fs.yzx · ft.zxy (the cross product's halves; the w field's MAC flags stay)
    void Opmula(u32 fs, u32 ft);
    void Opmsub(u32 fd, u32 fs, u32 ft);
    // No flags: MAX, MINI (signed magnitudes), ABS, the conversions
    void Max(u32 dest, u32 fd, u32 fs, u32 ft);
    void MaxBc(u32 dest, u32 fd, u32 fs, u32 ft, Field bc);
    void MaxI(u32 dest, u32 fd, u32 fs);
    void Mini(u32 dest, u32 fd, u32 fs, u32 ft);
    void MiniBc(u32 dest, u32 fd, u32 fs, u32 ft, Field bc);
    void MiniI(u32 dest, u32 fd, u32 fs);
    void Abs(u32 dest, u32 ft, u32 fs);
    void Ftoi(u32 dest, u32 ft, u32 fs, u32 fractionBits);
    void Itof(u32 dest, u32 ft, u32 fs, u32 fractionBits);
    // CLIPw.xyz fs, ft.w: the clip flags moved up six bits and fs's x, y and z against ±|ft.w| in the low six
    void Clip(u32 fs, u32 ft);

    // The lower instructions
    void Move(u32 dest, u32 ft, u32 fs);
    void Mr32(u32 dest, u32 ft, u32 fs);
    // Q's new value for DIV (fs.fsf / ft.ftf), SQRT (√ft.ftf) and RSQRT (fs.fsf / √ft.ftf); macro mode sets Q at once
    u32 DivValue(u32 fs, Field fsf, u32 ft, Field ftf) const;
    u32 SqrtValue(u32 ft, Field ftf) const;
    u32 RsqrtValue(u32 fs, Field fsf, u32 ft, Field ftf) const;
    void Div(u32 fs, Field fsf, u32 ft, Field ftf);
    void Sqrt(u32 ft, Field ftf);
    void Rsqrt(u32 fs, Field fsf, u32 ft, Field ftf);
    // LQ and SQ: data memory's quadword at vi[is] + offset
    void Lq(u32 dest, u32 ft, u32 is, s32 offset);
    void Sq(u32 dest, u32 fs, u32 it, s32 offset);
    void Iaddiu(u32 it, u32 is, u32 immediate);
    void Isubiu(u32 it, u32 is, u32 immediate);
    void Iaddi(u32 it, u32 is, s32 immediate);
    void Iadd(u32 id, u32 is, u32 it);
    void Isub(u32 id, u32 is, u32 it);
    void Iand(u32 id, u32 is, u32 it);
    void Ior(u32 id, u32 is, u32 it);
    // FMAND it, is with the MAC flags given (a microprogram's flags are some cycles late: the translation says which)
    void Fmand(u32 it, u32 is, u32 macFlags);
    void Rinit(u32 fs, Field fsf);
    void Rget(u32 dest, u32 ft);

private:
    using Binary = u32 (*)(u32, u32);
    using Ternary = u32 (*)(u32, u32, u32);
    u32 MacField(u32 field, u32 value);
    void ClearMacField(u32 field);
    void Apply(u32 dest, Vf* out, const Vf& fs, const Vf& ft, Binary op);
    void ApplyBroadcast(u32 dest, Vf* out, const Vf& fs, u32 bc, Binary op);
    void ApplyAcc(u32 dest, Vf* out, const Vf& fs, const Vf& ft, Ternary op);
    void ApplyAccBroadcast(u32 dest, Vf* out, const Vf& fs, u32 bc, Ternary op);
    Vf* Out(u32 fd);
    // Where writes to vf00 go
    Vf discard_;
};

// The native build's VU0
Vu0& TheVu0();

// VU0's data memory is its own unless the graphics side's VU0 is given (the particle views the culling programs read are
// written there by Platform::Graphics::LoadParticleView)
void SetVu0DataMemory(u8* memory);

// The microcode set loaded into VU0 (Platform::Graphics::UseHelperPrograms: 1 the standard set, 2 the culling one, 3 the decals'),
// which says what code a vcallms runs: a set only replaces the addresses it loads (the decals' set only 0x000-0x51F)
void Vu0ProgramsLoaded(u32 set);

// VCALLMS: the microprogram at an address of micro memory run to its end. The translated programs are the ones the maths
// calls; anything else (or another set's code at the address) stops the game with a message: nothing is made up
void Vu0CallMicroprogram(u32 address);
}
