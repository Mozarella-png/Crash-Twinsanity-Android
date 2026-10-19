#pragma once

#include <stddef.h>
#include <tamtypes.h>

typedef float f32;

// GCC for the R5900 cuts decimal float literals down to single precision (it rounds towards zero, as the R5900's FPU does),
// where retail's compiler rounded them to the nearest: 0.1f comes out a bit below retail's 0.1. Rounded(0.1) is the float
// nearest to a double literal, as retail has it; literals single precision holds exactly (0.5f, 1.25f) don't need it
consteval f32 Rounded(double value)
{
    // A double's 52 bits of fraction rounded to a float's 23 (to even when it's halfway), its exponent biased by 127 in place
    // of 1023; the sign is the top bit of both, a carry out of the fraction goes into the exponent
    constexpr unsigned int DoubleFractionBits = 52;
    constexpr unsigned int FloatFractionBits = 23;
    constexpr unsigned int DroppedBits = DoubleFractionBits - FloatFractionBits;
    constexpr unsigned int DoubleExponentMask = 0x7FF;
    constexpr unsigned int DoubleBias = 1023;
    constexpr unsigned int FloatBias = 127;
    constexpr unsigned int SignBit = 0x80000000u;
    unsigned long long bits = __builtin_bit_cast(unsigned long long, value);
    unsigned int sign = static_cast<unsigned int>(bits >> 32) & SignBit;
    unsigned int exponent =
        static_cast<unsigned int>((bits >> DoubleFractionBits) & DoubleExponentMask) - DoubleBias + FloatBias;
    unsigned long long mantissa = bits & ((1ull << DoubleFractionBits) - 1);
    unsigned int kept = static_cast<unsigned int>(mantissa >> DroppedBits);
    unsigned long long rest = mantissa & ((1ull << DroppedBits) - 1);
    constexpr unsigned long long Half = 1ull << (DroppedBits - 1);
    if (rest > Half || (rest == Half && (kept & 1) != 0))
    {
        kept++;
    }

    return __builtin_bit_cast(f32, (sign | exponent << FloatFractionBits) + kept);
}

// What the R5900's variable shifts (sllv, srlv, srav) take of a shift: its low 5 bits. A shift the C++ can't keep below 32 is
// masked with it, as the retail code's wrap
constexpr u32 ShiftMask = 0x1F;

// A function or variable by the name the retail executable's symbols give it (the asm's and the Ghidra project's): a function
// defined with it takes the retail one's place, a variable declared with it is the retail data the split keeps
#if defined(TWIN_NATIVE) && defined(__APPLE__)
// Mach-O's C symbols start with an underscore (and a name starting with L would be the assembler's own local label)
#define RETAIL(name) asm("_" #name)
#else
#define RETAIL(name) asm(#name)
#endif

#ifdef TWIN_NATIVE
// The PS2's memory below the game's (the kernel's) at a low address, which the retail code reads through null pointers in a few
// places (docs/RETAIL_BUGS.md). The native build has nothing mapped there: reads and writes there are done on a stand-in of it
// (src/platform/native/lowmemory.cpp, native/NATIVE.md)
extern "C" u8 g_NativeLowMemory[];
constexpr u32 NativeLowMemoryBytes = 0x100000;
#endif

// The bytes the retail code allocates for an object by a fixed size (its class's PS2 size): natively the object's pointers are 8
// bytes, so it can take up to twice the room (native/NATIVE.md)
#ifdef TWIN_NATIVE
constexpr u32 NativeObjectBytes(u32 ps2Bytes)
{
    return ps2Bytes * 2;
}
#else
constexpr u32 NativeObjectBytes(u32 ps2Bytes)
{
    return ps2Bytes;
}
#endif

// Checks a struct against the size the game's code gives it. VS Code's C/C++ extension (__INTELLISENSE__) lays structs out
// for an x86 target, where 64 bit members are only 4 byte aligned: the compiler checks them
#if defined(__INTELLISENSE__) || defined(TWIN_NATIVE)
// (TWIN_NATIVE: the native build's pointers are 64 bit, so the structs holding them are laid out for the host, not the PS2)
#define CHECK_SIZE(type, size)
#define CHECK_OFFSET(type, member, offset)
#define CHECK_LAYOUT(condition)
#else
// Any other check of the PS2's layout (offsets added up, a template's size)
#define CHECK_LAYOUT(condition) static_assert(condition)
#define CHECK_SIZE(type, size) static_assert(sizeof(type) == (size), #type " must be " #size " bytes")
#define CHECK_OFFSET(type, member, offset) static_assert(offsetof(type, member) == (offset), #type "::" #member " must be at " #offset)
#endif
