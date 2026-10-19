#pragma once

// The tests' reference VU0, written apart from the native one (src/platform/native/math/) to check it: PCSX2's VU0 as its
// interpreter has it (pcsx2/VUops.cpp, VU0microInterp.cpp, VU0.cpp, FPU.cpp), floats on the host's FPU switched to rounding
// towards zero with PCSX2's operand clamps and results flushed, microprograms run from the retail microcode's words with
// PCSX2's pipeline timing, and the PS2 side's asm run from its own text (refasm.cpp).

#include <cstdint>
#include <string>
#include <vector>

namespace Ref
{
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

struct Vu
{
    u32 vf[32][4];
    u16 vi[16];
    u32 acc[4];
    u32 q;
    u32 i;
    u32 r;
    u32 p;
    // PCSX2's macflag (the latest) and VI[REG_MAC_FLAG] (what FMAND reads)
    u32 macflag;
    u32 macVisible;
    u32 clipflag;
    u8 data[4096];
    u64 micro[512];

    // Instructions run by the last microprogram, and how many cycles it took
    u32 executed;
    u64 cycles;
};

Vu& TheVu();

// The microcode set loaded (1 standard, 2 culling, 3 decals): its MPG loads written into micro memory
void LoadMicrocodeSet(u32 set);
// A microprogram run to its end from an address (bytes)
void RunMicroprogram(u32 address);

// Floats as PCSX2 computes them (exposed for the float tests)
u32 Add(u32 a, u32 b);
u32 Sub(u32 a, u32 b);
u32 Mul(u32 a, u32 b);
u32 Madd(u32 acc, u32 a, u32 b);
u32 Msub(u32 acc, u32 a, u32 b);
u32 Div(u32 fs, u32 ft);
u32 Sqrt(u32 ft);
u32 Rsqrt(u32 fs, u32 ft);
u32 FpuRsqrt(u32 fs, u32 ft);
u32 FpMax(u32 a, u32 b);
u32 FpMin(u32 a, u32 b);
u32 FpuMax(u32 a, u32 b);
u32 FpuMin(u32 a, u32 b);

// Macro mode: an upper instruction's word, or a lower one's (DIV, SQRT, RSQRT write Q), run at once
void MacroUpper(u32 code);
void MacroLower(u32 code, bool writesQ);

// The PS2 side's asm statements: the template's text and the operands (%0 up: the outputs, then the inputs)
struct Operand
{
    enum Kind
    {
        Integer,
        Float,
        OutInteger,
        OutFloat,
    } kind;
    u64 value;
    float floatValue;
    void* out;
};

Operand In(const void* pointer);
Operand In(s32 value);
Operand In(u32 value);
Operand In(float value);
Operand Out(u32& value);
Operand Out(float& value);

void Asm(const char* text, std::vector<Operand> operands);

// A 32 bit address window for asm that reads addresses as words (MakeVertexDifferences' request: LW of its pointers): an address
// below 4 GB inside a window is the host memory it maps
void MapLow(u32 base, void* host, u32 size);
void UnmapLow();
}
