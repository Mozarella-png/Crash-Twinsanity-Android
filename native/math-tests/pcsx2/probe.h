#pragma once

// The PCSX2 probe: cases of the VU0 maths (inputs made on the host, tests.cpp --probe-inputs), run by the PS2 build's own
// functions in PCSX2 (probe_ps2.cpp, built into a scratch copy of the PS2 build, run_probe.py) and by the native ones
// (tests.cpp --probe-check), and their results compared. RunCase (probe_common.cpp) is the same source on both sides: it calls
// the functions by their names, which are the PS2 build's on the PS2 and src/platform/native/math/'s natively. The VU0 state
// before a case comes from its seed (integer arithmetic only, the same on both) through the same instructions, the whole state
// after it is read out the same way.

#include "common.h"

namespace Probe
{
enum Function : u32
{
    SinCos = 1,
    Slerp,
    Euler,
    Turn,
    Lerp,
    Joint,
    MultiplyByParent,
    RayTriangles,
    ViewDistance,
    ParticleView,
    Max,
    Min,
    DivideBySquareRoot,
    RotateVector,
    TransformPoint,
    MultiplyRotations,
    PlaneThroughTriangle,
    TransformByRows,
    InitIdentity,
    MultiplyMatrices,
    MatrixFromRotation,
    Transpose,
    TransposeQuadwords,
    TransposeRotation,
    InvertRigid,
    InvertRigidInPlace,
    EdgeTests,
    VertexDifferences,
    GroupPoints,
    GroupsRange,
    SixteenGroupsRange,
    HullSupport,
    GetBbox,
    PointsBox,
    TriangleBounds,
    FunctionCount,
};

constexpr u32 InputBytes = 1024;
constexpr u32 OutputBytes = 1536;
constexpr u32 DataBytes = 4096;

struct alignas(16) Case
{
    u32 function;
    u32 seed;
    u32 ints[6];
    f32 floats[4];
    u32 unused[4];
    alignas(16) u8 input[InputBytes];
};

struct alignas(16) Result
{
    u32 vf[32][4];
    u32 acc[4];
    u32 vi[16];
    // MAC, clip, R's 23 bits, I, Q, the function's result (if any)
    u32 mac;
    u32 clip;
    u32 r;
    u32 i;
    u32 q;
    u32 value;
    u32 unused[2];
    alignas(16) u8 output[OutputBytes];
};

// The file of cases: a header, the VU0 data memory the culling programs read (written before the cases), the cases
struct alignas(16) Header
{
    u32 magic;
    u32 count;
    u32 unused[2];
};
constexpr u32 Magic = 0x4D505242;
constexpr u32 Done = 0x600DF00D;

// The VU0 state a seed gives, as words (integer arithmetic only): vf01-vf31, vi01-vi15, I
struct alignas(16) Start
{
    u32 vf[32][4];
    u32 vi[16];
    u32 i;
};

inline u32 NextSeed(u32* x)
{
    *x ^= *x << 13;
    *x ^= *x >> 17;
    *x ^= *x << 5;
    return *x;
}

inline void MakeStart(u32 seed, Start* start)
{
    u32 x = seed | 1;
    auto floatBits = [&x]() {
        u32 bits = NextSeed(&x);
        if ((bits & 15) == 0)
        {
            return NextSeed(&x);
        }

        // A sign, an exponent from 2^-20 to 2^20, a fraction
        u32 exponent = 107 + (NextSeed(&x) % 41);
        return (bits & 0x80000000u) | (exponent << 23) | (NextSeed(&x) & 0x7FFFFF);
    };
    for (u32 reg = 0; reg < 32; reg++)
    {
        for (u32 field = 0; field < 4; field++)
        {
            start->vf[reg][field] = floatBits();
        }
    }

    // vf27-vf31 make ACC, Q, R and the clip flags: ordinary values
    for (u32 reg = 27; reg < 32; reg++)
    {
        for (u32 field = 0; field < 4; field++)
        {
            u32 exponent = 117 + (NextSeed(&x) % 21);
            start->vf[reg][field] = (NextSeed(&x) & 0x80000000u) | (exponent << 23) | (NextSeed(&x) & 0x7FFFFF);
        }
    }

    for (u32 reg = 0; reg < 16; reg++)
    {
        start->vi[reg] = NextSeed(&x) & 0xFFFF;
    }

    start->i = floatBits();
}

// Each side's own: the state set from the start (lqc2 vf01-vf31, ctc2 vi01-vi15 and I, ACC = vf31 + 0, Q = √vf30.x, R from
// vf29.x, the clip flags from four CLIPs of vf28 against vf27.w), and the state read out after (sqc2 vf00-vf31, ACC + 0 · 0,
// cfc2 of the integer registers, MAC, clip, R, I, Q)
void SetState(const Start& start);
void ReadState(Result* result);

void RunCase(const Case& probe, Result* result);
}
