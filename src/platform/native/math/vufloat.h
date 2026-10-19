#pragma once

// The PS2's float arithmetic on the host, on bit patterns: VU0's (macro mode and microprograms alike) and the few EE FPU
// instructions the game's maths keeps (MAX.S, MIN.S, RSQRT.S). The model is PCSX2's, the reference the decomp's results are
// checked against (pcsx2/VUops.cpp and FPU.cpp, run with its default settings: VU0 and the FPU round towards zero with
// denormals taken and given as zero, overflow clamped):
//
// - operands: an exponent of 0 is a signed zero (denormals are zero), an exponent of 255 the largest float of its sign (PCSX2's
//   vuDouble/fpuDouble clamp);
// - results: the exact result rounded towards zero to single precision, a result below the smallest normal float a zero of its
//   sign (flush to zero), above the largest float the largest float (rounding towards zero never gives an infinity);
// - multiply-adds (MADD, MSUB, OPMSUB) round the product first and then the sum (PCSX2's interpreter and microVU both do; the
//   VU's FMAC isn't fused);
// - DIV, SQRT and RSQRT: Q of a divisor of zero is the largest float with the operands' signs' exclusive or (RSQRT's 0 / 0 a
//   zero of that sign), SQRT and RSQRT take the magnitude of a negative operand, RSQRT rounds the square root and then the
//   quotient (two roundings, as PCSX2 does it).
//
// The host has no rounding towards zero in its ordinary arithmetic, so the roundings are done here exactly: a sum's error is
// recovered exactly (TwoSum in double precision), a product of two floats is exact in double precision, a quotient's and a
// square root's candidate are checked against the exact remainder (fma in double precision). Every result is the one the
// rounding gives for the exact value; native/math-tests checks it against the host FPU switched to rounding towards zero.
//
// TWIN_VU_FLOAT_HOST makes it the host's own IEEE arithmetic instead (rounding to nearest, denormals and infinities kept), for
// comparison only: the game's results then differ from the PS2's in the last bits.

#include "common.h"

#include <bit>
#include <cmath>
#include <cstring>

namespace NativeMath::VuFloat
{
constexpr u32 SignBit = 0x80000000u;
constexpr u32 ExponentMask = 0x7F800000u;
constexpr u32 MaxMagnitude = 0x7F7FFFFFu;

inline f32 AsFloat(u32 bits)
{
    return std::bit_cast<f32>(bits);
}

inline u32 AsBits(f32 value)
{
    return std::bit_cast<u32>(value);
}

// An operand as PCSX2 reads it: denormals zero, infinities and NaNs the largest float of their sign
inline u32 Operand(u32 bits)
{
    u32 exponent = bits & ExponentMask;
    if (exponent == 0)
    {
        return bits & SignBit;
    }

    if (exponent == ExponentMask)
    {
        return (bits & SignBit) | MaxMagnitude;
    }

    return bits;
}

#ifndef TWIN_VU_FLOAT_HOST

// A double (finite) rounded towards zero to a float, below the smallest normal float a zero of its sign. Exact when the
// double is the exact value, or when the exact value is within half a double's unit of it and no float lies between (the
// callers make sure of that)
inline u32 TruncateToFloat(double value)
{
    f32 nearest = static_cast<f32>(value);
    if (std::fabs(static_cast<double>(nearest)) > std::fabs(value))
    {
        // Rounding to nearest went away from zero (or overflowed to an infinity): the float below it
        u32 bits = AsBits(nearest) - 1;
        nearest = AsFloat(bits);
    }

    u32 bits = AsBits(nearest);
    if ((bits & ExponentMask) == 0)
    {
        return std::signbit(value) ? SignBit : 0;
    }

    return bits;
}

// The float one unit further from zero (its magnitude's bits one more)
inline u32 NextAway(u32 bits)
{
    return bits + 1;
}

// a + b of two operands (Operand's): the double sum and its exact error (TwoSum), rounded towards zero
inline u32 AddOperands(u32 a, u32 b)
{
    double x = AsFloat(a);
    double y = AsFloat(b);
    double sum = x + y;
    double yPart = sum - x;
    double error = (x - (sum - yPart)) + (y - yPart);
    if (error == 0.0)
    {
        return TruncateToFloat(sum);
    }

    f32 asFloat = static_cast<f32>(sum);
    if (static_cast<double>(asFloat) == sum)
    {
        // The double sum is a float, the exact one a little off it: below it in magnitude the float under it, else it
        if ((error < 0.0) == (sum > 0.0))
        {
            u32 bits = AsBits(asFloat) - 1;
            if ((bits & ExponentMask) == 0)
            {
                return AsBits(asFloat) & SignBit;
            }

            return bits;
        }

        return TruncateToFloat(sum);
    }

    // No float lies between the double sum and the exact one
    return TruncateToFloat(sum);
}

inline u32 Add(u32 a, u32 b)
{
    return AddOperands(Operand(a), Operand(b));
}

inline u32 Sub(u32 a, u32 b)
{
    return AddOperands(Operand(a), Operand(b) ^ SignBit);
}

inline u32 MultiplyOperands(u32 a, u32 b)
{
    // Two floats' product is exact in double precision
    double product = static_cast<double>(AsFloat(a)) * static_cast<double>(AsFloat(b));
    return TruncateToFloat(product);
}

inline u32 Mul(u32 a, u32 b)
{
    return MultiplyOperands(Operand(a), Operand(b));
}

// acc + a · b and acc - a · b: the product rounded, then the sum. PCSX2 reads the product as it came out of the
// multiplication (no clamp: rounding towards zero gives none to clamp)
inline u32 MulAdd(u32 acc, u32 a, u32 b)
{
    return AddOperands(Operand(acc), MultiplyOperands(Operand(a), Operand(b)));
}

inline u32 MulSub(u32 acc, u32 a, u32 b)
{
    return AddOperands(Operand(acc), MultiplyOperands(Operand(a), Operand(b)) ^ SignBit);
}

// The quotient of two operands rounded towards zero (the divisor not zero): the double quotient's float below it, moved by a
// unit when the exact remainder says it's off
inline u32 DivideOperands(u32 a, u32 b)
{
    double dividend = std::fabs(static_cast<double>(AsFloat(a)));
    double divisor = std::fabs(static_cast<double>(AsFloat(b)));
    u32 sign = (a ^ b) & SignBit;
    if (dividend == 0.0)
    {
        return sign;
    }

    u32 magnitude = TruncateToFloat(dividend / divisor);
    // An exact remainder: magnitude · divisor is exact (two floats), the difference fits in a double
    if (magnitude != 0 && std::fma(-static_cast<double>(AsFloat(magnitude)), divisor, dividend) < 0.0)
    {
        magnitude--;
    }

    if (magnitude < MaxMagnitude)
    {
        u32 next = NextAway(magnitude);
        if ((magnitude & ExponentMask) == 0)
        {
            next = 0x00800000u;
        }

        if (std::fma(-static_cast<double>(AsFloat(next)), divisor, dividend) >= 0.0)
        {
            magnitude = next;
        }
    }

    if ((magnitude & ExponentMask) == 0)
    {
        return sign;
    }

    return magnitude | sign;
}

// The square root of an operand's magnitude rounded towards zero
inline u32 SquareRootOperand(u32 a)
{
    double value = std::fabs(static_cast<double>(AsFloat(a)));
    if (value == 0.0)
    {
        return 0;
    }

    u32 root = TruncateToFloat(std::sqrt(value));
    // root² is exact in double precision
    if (static_cast<double>(AsFloat(root)) * static_cast<double>(AsFloat(root)) > value)
    {
        root--;
    }

    if (root < MaxMagnitude)
    {
        double next = AsFloat(NextAway(root));
        if (next * next <= value)
        {
            root = NextAway(root);
        }
    }

    return root;
}

#else

// The host's IEEE arithmetic (for comparison builds only)
inline u32 Add(u32 a, u32 b)
{
    return AsBits(AsFloat(a) + AsFloat(b));
}

inline u32 Sub(u32 a, u32 b)
{
    return AsBits(AsFloat(a) - AsFloat(b));
}

inline u32 Mul(u32 a, u32 b)
{
    return AsBits(AsFloat(a) * AsFloat(b));
}

inline u32 MulAdd(u32 acc, u32 a, u32 b)
{
    f32 product = AsFloat(a) * AsFloat(b);
    return AsBits(AsFloat(acc) + product);
}

inline u32 MulSub(u32 acc, u32 a, u32 b)
{
    f32 product = AsFloat(a) * AsFloat(b);
    return AsBits(AsFloat(acc) - product);
}

inline u32 DivideOperands(u32 a, u32 b)
{
    return AsBits(AsFloat(a) / AsFloat(b));
}

inline u32 SquareRootOperand(u32 a)
{
    return AsBits(std::sqrt(std::fabs(AsFloat(a))));
}

#endif

// VU0's DIV: Q = fs / ft. A divisor of zero (or a denormal) gives the largest float with the signs' exclusive or (0 / 0 too)
inline u32 Divide(u32 fs, u32 ft)
{
    u32 dividend = Operand(fs);
    u32 divisor = Operand(ft);
    if ((divisor & ~SignBit) == 0)
    {
        return ((fs ^ ft) & SignBit) | MaxMagnitude;
    }

    return DivideOperands(dividend, divisor);
}

// VU0's SQRT: Q = √|ft|
inline u32 SquareRoot(u32 ft)
{
    return SquareRootOperand(Operand(ft));
}

// VU0's RSQRT: Q = fs / √|ft|, the root rounded first. A zero divisor gives the largest float with the signs' exclusive or, or
// a zero of that sign when fs is zero as well
inline u32 ReciprocalSquareRoot(u32 fs, u32 ft)
{
    u32 dividend = Operand(fs);
    u32 divisor = Operand(ft);
    if ((divisor & ~SignBit) == 0)
    {
        u32 sign = (fs ^ ft) & SignBit;
        if ((dividend & ~SignBit) != 0)
        {
            return sign | MaxMagnitude;
        }

        return sign;
    }

    u32 root = SquareRootOperand(divisor);
    return DivideOperands(dividend, root);
}

// VU0's MAX and MINI: the floats compared as signed magnitudes on their bits (+0 above -0; denormals and infinities compared as
// they are)
inline u32 Max(u32 a, u32 b)
{
    s32 x = static_cast<s32>(a);
    s32 y = static_cast<s32>(b);
    if (x < 0 && y < 0)
    {
        return static_cast<u32>(x < y ? x : y);
    }

    return static_cast<u32>(x > y ? x : y);
}

inline u32 Min(u32 a, u32 b)
{
    s32 x = static_cast<s32>(a);
    s32 y = static_cast<s32>(b);
    if (x < 0 && y < 0)
    {
        return static_cast<u32>(x > y ? x : y);
    }

    return static_cast<u32>(x < y ? x : y);
}

// The EE FPU's MAX.S and MIN.S as PCSX2 runs them by default (its recompiler, recCommutativeOp): both operands clamped
// (infinities and NaNs the largest float of their sign) and denormals taken as zeros of their sign, then compared. Its MAXSS and
// MINSS give one operand or the other for +0 against -0 by its register allocation: here +0 is above -0 as in its interpreter
inline u32 FpuMax(u32 a, u32 b)
{
    return Max(Operand(a), Operand(b));
}

inline u32 FpuMin(u32 a, u32 b)
{
    return Min(Operand(a), Operand(b));
}

// The EE FPU's RSQRT.S (fs / √ft), as PCSX2 does it with its default settings: a divisor of zero or a denormal gives the largest
// float with fs's sign (PCSX2's recompiler; its interpreter takes ft's: they differ only for a negative zero), a negative
// divisor its magnitude's root; else the root and the quotient each rounded towards zero. Operands with an exponent of 255
// (which the PS2's FPU never makes) are clamped first, as PCSX2's interpreter's fpuDouble does
inline u32 FpuReciprocalSquareRoot(u32 fs, u32 ft)
{
    if ((ft & ExponentMask) == 0)
    {
        return (fs & SignBit) | MaxMagnitude;
    }

    u32 root = SquareRootOperand(Operand(ft) & ~SignBit);
    return DivideOperands(Operand(fs), root);
}
}
