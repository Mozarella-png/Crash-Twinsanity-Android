#pragma once

// The VU0 microprograms the maths calls, translated from the retail microcode (microprograms.cpp, made by
// native/math-tests/tools/vu0translate.py), and the microcode sets' layout in micro memory

#include "vu0.h"

namespace NativeMath
{
// Instructions [first, end) of micro memory (8 bytes each)
struct MicroInstructionRange
{
    u32 first;
    u32 end;
};

struct Microprogram
{
    // Where it starts (bytes), the sets (bit n: set n) whose code is the translated program's at every instruction it can run,
    // those instructions
    u32 address;
    u32 sets;
    const MicroInstructionRange* ranges;
    u32 rangeCount;
    void (*run)(Vu0& vu);
};

// What a set's DMA chain loads into micro memory (its MPG codes)
struct MicrocodeSetLoads
{
    u32 set;
    const MicroInstructionRange* ranges;
    u32 count;
};

const Microprogram* FindMicroprogram(u32 address);
const MicrocodeSetLoads* MicrocodeSetLoadsOf(u32 set);
const Microprogram* Microprograms(u32* count);
}
