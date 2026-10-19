// The native VU0 maths (src/platform/native/math/) against the reference: the PS2 side's own C++ and asm text
// (src/platform/ps2/{math,matrices,collisionmaths,bounds}.cpp, made into Ref_ functions by tools/make_reference.py) run on the
// reference VU0 (reference/: PCSX2's semantics, floats on the host FPU rounding towards zero, the microprograms run from the
// retail microcode's words with PCSX2's pipeline timing). Every function is run on both from the same random VU0 state with the
// same random inputs; the outputs and the whole VU0 state after must match to the bit.
//
//     math_tests [iterations per function (default 20000)] [seed]

#include "game/collision.h"
#include "game/hull.h"
#include "game/math.h"
#include "game/physics.h"
#include "platform/math.h"

#include "microprograms.h"
#include "probe.h"
#include "refvu0.h"
#include "vu0.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

// The reference's functions (tools/make_reference.py's renaming)
namespace Platform::Math
{
f32 Ref_Max(f32 first, f32 second);
f32 Ref_Min(f32 first, f32 second);
void Ref_SinCos(f32 first, f32 second, f32* out);
void Ref_SlerpRotations(const Vector4* from, const Vector4* to, f32 share, Vector4* out);
void Ref_EulerRotation(const Vector4* anglesNow, const Vector4* anglesNext, const Vector4* moveNow, const Vector4* moveNext,
                       Vector4* rotation, Vector4* move);
f32 Ref_ViewDistance(s32 view, const f32* point);
bool Ref_ParticleBlockView(s32 view, bool keepsTranslation, const f32* extents, const Matrix4x4* gravity, const f32* place,
                           f32 scale, Matrix4x4* matrix, f32* distance);
void Ref_TurnRotation(const Vector4* by, Vector4* rotation);
void Ref_Lerp(const Vector4* from, const Vector4* to, f32 share, Vector4* out);
void Ref_JointMatrix(const Vector4* rotation, const Vector4* parentScale, const Vector4* scale, const Vector4* translation,
                     const Matrix4x4* parent, Matrix4x4* out);
void Ref_MultiplyByParent(const Matrix4x4* matrix, const Matrix4x4* parent, Matrix4x4* out);
f32 Ref_DivideBySquareRoot(f32 value, f32 square);
void Ref_SetRay(const Vector4* start, const Vector4* end);
void Ref_StartRayTriangle(const Vector4* vertices, const Vector4* nearest);
bool Ref_FinishRayTriangle(Vector4* hit);
}

void Ref_VuRotateVector(const Matrix4x4* matrix, const Vector4* vector, Vector4* out);
void Ref_VuTransformPoint(const Matrix4x4* matrix, const Vector4* point, Vector4* out);
void Ref_MultiplyRotations(Vector4* out, const Vector4* a, const Vector4* b);
u32 Ref_PlaneThroughTriangle(Vector4* plane, const Vector4* first, const Vector4* second, const Vector4* third);
void Ref_VuTransformByRows(const Matrix4x4* rows, const Vector4* point, Vector4* out);
void Ref_InitIdentityMatrix(Matrix4x4* matrix);
void Ref_VuMultiplyMatrices(const Matrix4x4* a, const Matrix4x4* b, Matrix4x4* out);
void Ref_MatrixFromRotation(Matrix4x4* matrix, const Vector4* rotation);
void Ref_VuTranspose(const Matrix4x4* matrix, Matrix4x4* out);
void Ref_TransposeQuadwords(void* rows);
void Ref_VuTransposeRotation(const Matrix4x4* matrix, Matrix4x4* out);
void Ref_VuInvertRigid(Matrix4x4* out, const Matrix4x4* matrix);
void Ref_VuInvertRigidInPlace(Matrix4x4* matrix);
void Ref_StartTriangleEdgeTests(const Vector4* start, const Vector4* end, const CollisionHit* triangle, f32* values);
void Ref_FinishTriangleEdgeTests(f32* values);
void Ref_MakeVertexDifferences(const VertexDifferences* request);
void Ref_GroupPoints(Vector4* points, s32 groups);
void Ref_GroupsRange(const Vector4* groups, s32 count, const Vector4* axis, f32* range);
void Ref_SixteenGroupsRange(const Vector4* groups, const Vector4* axis, f32* range);
void Ref_LoadTriangleHullSupport(const CollisionHit* triangle, const Vector4* hullVertices);
void Ref_TriangleHullSupportRange(const Vector4* axis, f32* range);
void Ref_GetBbox(const Vector4* points, s32 count, Box* box, const Matrix4x4* matrix);
void Ref_PointsBox(const Vector4* points, s32 count, Box* box);
void Ref_TriangleBounds(Box* box, const Vector4* first, const Vector4* second, const Vector4* third);

namespace
{
using NativeMath::TheVu0;
using NativeMath::Vu0;

// ------------------------------------------------------------------------------------------------------------------------------
// Random values

u64 s_Seed = 0x9E3779B97F4A7C15ull;

u64 Next()
{
    s_Seed ^= s_Seed << 13;
    s_Seed ^= s_Seed >> 7;
    s_Seed ^= s_Seed << 17;
    return s_Seed;
}

u32 Bits32()
{
    return static_cast<u32>(Next() >> 16);
}

f32 FromBits(u32 bits)
{
    f32 value;
    std::memcpy(&value, &bits, 4);
    return value;
}

u32 ToBits(f32 value)
{
    u32 bits;
    std::memcpy(&bits, &value, 4);
    return bits;
}

f32 Uniform(f32 low, f32 high)
{
    return low + (high - low) * static_cast<f32>(Next() >> 40) / static_cast<f32>(1 << 24);
}

// A float of the kinds the maths meets, and the edges: ordinary values, magnitudes from 2^-40 to 2^40, values near 1 and its
// powers, zeros, the smallest normals and the largest floats, denormals, infinities and NaNs (the VU has none: clamped)
f32 AnyFloat()
{
    u32 kind = Next() % 16;
    switch (kind)
    {
    case 0: case 1: case 2: case 3: case 4:
        return Uniform(-4.0f, 4.0f);
    case 5: case 6:
        return std::ldexp(Uniform(1.0f, 2.0f), static_cast<int>(Next() % 81) - 40) * ((Next() & 1) ? -1.0f : 1.0f);
    case 7:
    {
        // A few units from a power of two
        u32 bits = (static_cast<u32>(Next() % 40 + 107) << 23) + static_cast<u32>(Next() % 9) - 4;
        return FromBits(bits | ((Next() & 1) << 31));
    }
    case 8:
    {
        static const u32 edges[] = {0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x00800000, 0x80800000, 0x7F7FFFFF,
                                    0xFF7FFFFF, 0x00000001, 0x807FFFFF, 0x7F800000, 0xFF800000, 0x7FC00000, 0x00800001,
                                    0x3F7FFFFF, 0x3F800001};
        return FromBits(edges[Next() % (sizeof(edges) / sizeof(edges[0]))]);
    }
    case 9:
        return FromBits(Bits32());
    default:
        return Uniform(-100.0f, 100.0f);
    }
}

// Mostly ordinary values (what the game gives these functions), now and then any
f32 GameFloat()
{
    return (Next() % 8) == 0 ? AnyFloat() : Uniform(-50.0f, 50.0f);
}

void RandomVector(Vector4* v, bool any)
{
    for (int n = 0; n < 4; n++)
        reinterpret_cast<f32*>(v)[n] = any ? AnyFloat() : GameFloat();
}

void RandomMatrix(Matrix4x4* m, bool any)
{
    for (int n = 0; n < 16; n++)
        reinterpret_cast<f32*>(m)[n] = any ? AnyFloat() : GameFloat();
}

void UnitQuaternion(Vector4* q)
{
    f32* v = reinterpret_cast<f32*>(q);
    f32 length = 0;
    for (int n = 0; n < 4; n++)
    {
        v[n] = Uniform(-1.0f, 1.0f);
        length += v[n] * v[n];
    }
    length = std::sqrt(length);
    for (int n = 0; n < 4; n++)
        v[n] /= length;
}

void Rotation(Matrix4x4* m)
{
    // A rotation (from a quaternion) and a translation
    Vector4 q;
    UnitQuaternion(&q);
    f32 x = q.x, y = q.y, z = q.z, w = q.w;
    f32 r[16] = {1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w), 0,
                 2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w), 0,
                 2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y), 0,
                 Uniform(-100, 100), Uniform(-100, 100), Uniform(-100, 100), 1};
    std::memcpy(m, r, sizeof(r));
}

// ------------------------------------------------------------------------------------------------------------------------------
// The two VU0s: made the same, compared

void RandomState()
{
    Vu0& native = TheVu0();
    Ref::Vu& ref = Ref::TheVu();
    for (int reg = 1; reg < 32; reg++)
        for (int field = 0; field < 4; field++)
            native.vf[reg].v[field] = ref.vf[reg][field] = (Next() % 4) == 0 ? Bits32() : ToBits(GameFloat());
    for (int field = 0; field < 4; field++)
    {
        native.vf[0].v[field] = ref.vf[0][field] = field == 3 ? 0x3F800000 : 0;
        native.acc.v[field] = ref.acc[field] = ToBits(GameFloat());
    }
    native.vi[0] = ref.vi[0] = 0;
    for (int reg = 1; reg < 16; reg++)
        native.vi[reg] = ref.vi[reg] = static_cast<u16>(Bits32());
    native.q = ref.q = ToBits(GameFloat());
    native.i = ref.i = ToBits(GameFloat());
    native.r = ref.r = 0x3F800000 | (Bits32() & 0x7FFFFF);
    native.p = ref.p = 0;
    native.mac = ref.macflag = ref.macVisible = Bits32() & 0xFFFF;
    native.clip = ref.clipflag = Bits32() & 0xFFFFFF;
}

void RandomData()
{
    // VU0's data memory: the particle views the culling programs read
    for (u32 word = 0; word < 1024; word++)
    {
        u32 value = ToBits(GameFloat());
        std::memcpy(TheVu0().data + word * 4, &value, 4);
        std::memcpy(Ref::TheVu().data + word * 4, &value, 4);
    }
}

std::string s_Failure;

bool SameState()
{
    const Vu0& native = TheVu0();
    const Ref::Vu& ref = Ref::TheVu();
    char text[256];
    for (int reg = 0; reg < 32; reg++)
        for (int field = 0; field < 4; field++)
            if (native.vf[reg].v[field] != ref.vf[reg][field])
            {
                std::snprintf(text, sizeof(text), "vf%02d.%c native %08X reference %08X", reg, "xyzw"[field],
                              native.vf[reg].v[field], ref.vf[reg][field]);
                s_Failure = text;
                return false;
            }
    for (int field = 0; field < 4; field++)
        if (native.acc.v[field] != ref.acc[field])
        {
            std::snprintf(text, sizeof(text), "ACC.%c native %08X reference %08X", "xyzw"[field], native.acc.v[field],
                          ref.acc[field]);
            s_Failure = text;
            return false;
        }
    for (int reg = 0; reg < 16; reg++)
        if (native.vi[reg] != ref.vi[reg])
        {
            std::snprintf(text, sizeof(text), "vi%02d native %04X reference %04X", reg, native.vi[reg], ref.vi[reg]);
            s_Failure = text;
            return false;
        }
    struct
    {
        const char* name;
        u32 native;
        u32 ref;
    } scalars[] = {{"Q", native.q, ref.q},
                   {"I", native.i, ref.i},
                   {"R", native.r, ref.r},
                   {"MAC", native.mac, ref.macVisible},
                   {"MAC (latest)", native.mac, ref.macflag},
                   {"clip", native.clip, ref.clipflag}};
    for (const auto& s : scalars)
        if (s.native != s.ref)
        {
            std::snprintf(text, sizeof(text), "%s native %08X reference %08X", s.name, s.native, s.ref);
            s_Failure = text;
            return false;
        }
    if (std::memcmp(native.data, ref.data, 4096) != 0)
    {
        s_Failure = "data memory";
        return false;
    }
    return true;
}

bool SameBytes(const void* a, const void* b, size_t size, const char* what)
{
    if (std::memcmp(a, b, size) == 0)
        return true;
    const auto* x = static_cast<const u32*>(a);
    const auto* y = static_cast<const u32*>(b);
    for (size_t word = 0; word < size / 4; word++)
        if (x[word] != y[word])
        {
            char text[256];
            std::snprintf(text, sizeof(text), "%s word %zu native %08X reference %08X", what, word, x[word], y[word]);
            s_Failure = text;
            return false;
        }
    s_Failure = what;
    return false;
}

struct Result
{
    u32 runs = 0;
    u32 failures = 0;
    std::map<u32, u32> paths;  // microprogram instructions run -> how often (the paths taken)
};

std::map<std::string, Result> s_Results;
u32 s_Iterations = 20000;

// A test: run it the iterations' number of times; each run returns whether the two agreed
void Test(const std::string& name, const std::function<bool()>& run, bool microprogram = false)
{
    Result& result = s_Results[name];
    for (u32 n = 0; n < s_Iterations; n++)
    {
        Ref::TheVu().executed = 0;
        bool same = run() && SameState();
        result.runs++;
        if (microprogram)
            result.paths[Ref::TheVu().executed]++;
        if (!same)
        {
            if (result.failures < 5)
                std::printf("FAIL %s (run %u): %s\n", name.c_str(), n, s_Failure.c_str());
            result.failures++;
        }
    }
}

// Same random state and the inputs copied for both sides
#define BOTH(native, reference) \
    do                          \
    {                           \
        native;                 \
        reference;              \
    } while (0)

// ------------------------------------------------------------------------------------------------------------------------------
// The float arithmetic alone

void FloatTests()
{
    using namespace NativeMath::VuFloat;
    struct Case
    {
        const char* name;
        std::function<u32(u32, u32, u32)> native;
        std::function<u32(u32, u32, u32)> ref;
    } cases[] = {
        {"float Add", [](u32 a, u32 b, u32) { return Add(a, b); }, [](u32 a, u32 b, u32) { return Ref::Add(a, b); }},
        {"float Sub", [](u32 a, u32 b, u32) { return Sub(a, b); }, [](u32 a, u32 b, u32) { return Ref::Sub(a, b); }},
        {"float Mul", [](u32 a, u32 b, u32) { return Mul(a, b); }, [](u32 a, u32 b, u32) { return Ref::Mul(a, b); }},
        {"float MulAdd", [](u32 a, u32 b, u32 c) { return MulAdd(c, a, b); }, [](u32 a, u32 b, u32 c) { return Ref::Madd(c, a, b); }},
        {"float MulSub", [](u32 a, u32 b, u32 c) { return MulSub(c, a, b); }, [](u32 a, u32 b, u32 c) { return Ref::Msub(c, a, b); }},
        {"float Divide", [](u32 a, u32 b, u32) { return Divide(a, b); }, [](u32 a, u32 b, u32) { return Ref::Div(a, b); }},
        {"float SquareRoot", [](u32 a, u32, u32) { return SquareRoot(a); }, [](u32 a, u32, u32) { return Ref::Sqrt(a); }},
        {"float ReciprocalSquareRoot", [](u32 a, u32 b, u32) { return ReciprocalSquareRoot(a, b); },
         [](u32 a, u32 b, u32) { return Ref::Rsqrt(a, b); }},
        {"float FpuReciprocalSquareRoot", [](u32 a, u32 b, u32) { return FpuReciprocalSquareRoot(a, b); },
         [](u32 a, u32 b, u32) { return Ref::FpuRsqrt(a, b); }},
        {"float Max", [](u32 a, u32 b, u32) { return Max(a, b); }, [](u32 a, u32 b, u32) { return Ref::FpMax(a, b); }},
        {"float FpuMax", [](u32 a, u32 b, u32) { return FpuMax(a, b); }, [](u32 a, u32 b, u32) { return Ref::FpuMax(a, b); }},
        {"float FpuMin", [](u32 a, u32 b, u32) { return FpuMin(a, b); }, [](u32 a, u32 b, u32) { return Ref::FpuMin(a, b); }},
        {"float Min", [](u32 a, u32 b, u32) { return Min(a, b); }, [](u32 a, u32 b, u32) { return Ref::FpMin(a, b); }},
    };
    for (const Case& c : cases)
    {
        Result& result = s_Results[c.name];
        u32 count = s_Iterations * 100;
        for (u32 n = 0; n < count; n++)
        {
            u32 a, b, acc;
            u32 kind = Next() % 4;
            if (kind == 0)
            {
                a = Bits32();
                b = Bits32();
                acc = Bits32();
            }
            else if (kind == 1)
            {
                // Operands whose sum cancels or rounds at a power of two: near each other or far apart
                a = ToBits(AnyFloat());
                b = (a ^ ((Next() & 1) << 31)) + static_cast<u32>(Next() % 64) - 32;
                acc = ToBits(AnyFloat());
            }
            else
            {
                a = ToBits(AnyFloat());
                b = ToBits(AnyFloat());
                acc = ToBits(AnyFloat());
            }
            u32 x = c.native(a, b, acc);
            u32 y = c.ref(a, b, acc);
            result.runs++;
            if (x != y)
            {
                if (result.failures < 5)
                    std::printf("FAIL %s(%08X, %08X, %08X): native %08X reference %08X\n", c.name, a, b, acc, x, y);
                result.failures++;
            }
        }
    }
}

// ------------------------------------------------------------------------------------------------------------------------------
// The functions

void MathTests()
{
    namespace M = Platform::Math;
    Test("Platform::Math::Max", [] {
        RandomState();
        f32 a = AnyFloat(), b = AnyFloat();
        u32 x = ToBits(M::Max(a, b)), y = ToBits(M::Ref_Max(a, b));
        return SameBytes(&x, &y, 4, "result");
    });
    Test("Platform::Math::Min", [] {
        RandomState();
        f32 a = AnyFloat(), b = AnyFloat();
        u32 x = ToBits(M::Min(a, b)), y = ToBits(M::Ref_Min(a, b));
        return SameBytes(&x, &y, 4, "result");
    });
    Test("Platform::Math::DivideBySquareRoot", [] {
        RandomState();
        f32 a = (Next() % 2) ? 1.0f : AnyFloat();
        f32 b = (Next() % 2) ? Uniform(0.0f, 1000.0f) : AnyFloat();
        u32 x = ToBits(M::DivideBySquareRoot(a, b)), y = ToBits(M::Ref_DivideBySquareRoot(a, b));
        return SameBytes(&x, &y, 4, "result");
    });
    Test("Platform::Math::SinCos", [] {
        RandomState();
        f32 a = (Next() % 4) ? Uniform(-10.0f, 10.0f) : AnyFloat();
        f32 b = (Next() % 4) ? Uniform(-1000.0f, 1000.0f) : AnyFloat();
        f32 x[4], y[4];
        M::SinCos(a, b, x);
        M::Ref_SinCos(a, b, y);
        return SameBytes(x, y, sizeof(x), "values");
    }, true);
    Test("Platform::Math::SlerpRotations", [] {
        RandomState();
        Vector4 from, to, x, y;
        UnitQuaternion(&from);
        u32 kind = Next() % 5;
        if (kind == 0)
        {
            // Within 0.999 (the straight interpolation), either sign
            to = from;
            f32* t = reinterpret_cast<f32*>(&to);
            for (int n = 0; n < 4; n++)
                t[n] += Uniform(-0.02f, 0.02f);
            if (Next() & 1)
                for (int n = 0; n < 4; n++)
                    t[n] = -t[n];
        }
        else if (kind == 1)
            RandomVector(&to, true);
        else
            UnitQuaternion(&to);
        if (Next() % 8 == 0)
            RandomVector(&from, true);
        f32 share = (Next() % 8) ? Uniform(0.0f, 1.0f) : AnyFloat();
        M::SlerpRotations(&from, &to, share, &x);
        M::Ref_SlerpRotations(&from, &to, share, &y);
        return SameBytes(&x, &y, sizeof(x), "out");
    }, true);
    Test("Platform::Math::EulerRotation", [] {
        RandomState();
        Vector4 a, b, c, d, x1, x2, y1, y2;
        for (Vector4* v : {&a, &b})
            for (int n = 0; n < 4; n++)
                reinterpret_cast<f32*>(v)[n] = (Next() % 8) ? Uniform(-7.0f, 7.0f) : AnyFloat();
        b.w = (Next() % 8) ? Uniform(0.0f, 1.0f) : AnyFloat();
        RandomVector(&c, false);
        RandomVector(&d, false);
        M::EulerRotation(&a, &b, &c, &d, &x1, &x2);
        M::Ref_EulerRotation(&a, &b, &c, &d, &y1, &y2);
        return SameBytes(&x1, &y1, 16, "rotation") && SameBytes(&x2, &y2, 16, "move");
    }, true);
    Test("Platform::Math::TurnRotation", [] {
        RandomState();
        Vector4 by, r1, r2;
        UnitQuaternion(&by);
        UnitQuaternion(&r1);
        if (Next() % 8 == 0)
            RandomVector(&r1, true);
        r2 = r1;
        M::TurnRotation(&by, &r1);
        M::Ref_TurnRotation(&by, &r2);
        return SameBytes(&r1, &r2, 16, "rotation");
    }, true);
    Test("Platform::Math::Lerp", [] {
        RandomState();
        Vector4 a, b, x, y;
        RandomVector(&a, Next() % 8 == 0);
        RandomVector(&b, Next() % 8 == 0);
        f32 share = (Next() % 8) ? Uniform(0.0f, 1.0f) : AnyFloat();
        M::Lerp(&a, &b, share, &x);
        M::Ref_Lerp(&a, &b, share, &y);
        return SameBytes(&x, &y, 16, "out");
    });
    Test("Platform::Math::JointMatrix", [] {
        RandomState();
        Vector4 rotation, parentScale, scale, translation;
        Matrix4x4 parent, x, y;
        UnitQuaternion(&rotation);
        for (Vector4* v : {&parentScale, &scale})
            for (int n = 0; n < 4; n++)
                reinterpret_cast<f32*>(v)[n] = (Next() % 8) ? Uniform(0.1f, 3.0f) : AnyFloat();
        RandomVector(&translation, false);
        Rotation(&parent);
        u32 which = static_cast<u32>(Next());
        const Vector4* r = (which & 1) ? &rotation : nullptr;
        const Vector4* ps = (which & 2) ? &parentScale : nullptr;
        const Vector4* s = (which & 4) ? &scale : nullptr;
        const Vector4* t = (which & 8) ? &translation : nullptr;
        const Matrix4x4* p = (which & 16) ? &parent : nullptr;
        M::JointMatrix(r, ps, s, t, p, &x);
        M::Ref_JointMatrix(r, ps, s, t, p, &y);
        return SameBytes(&x, &y, sizeof(x), "out");
    }, true);
    Test("Platform::Math::MultiplyByParent", [] {
        RandomState();
        Matrix4x4 a, b, x, y;
        RandomMatrix(&a, Next() % 8 == 0);
        RandomMatrix(&b, Next() % 8 == 0);
        M::MultiplyByParent(&a, &b, &x);
        M::Ref_MultiplyByParent(&a, &b, &y);
        return SameBytes(&x, &y, sizeof(x), "out");
    }, true);
    Test("Platform::Math::Ray triangles", [] {
        // A ray set, then a few triangles tested in a row the way the collision does (each Finish before the next Start)
        RandomState();
        Vector4 start, end;
        RandomVector(&start, false);
        RandomVector(&end, false);
        start.w = end.w = 1.0f;
        M::SetRay(&start, &end);
        M::Ref_SetRay(&start, &end);
        Vector4 nearest;
        RandomVector(&nearest, false);
        nearest.w = (Next() % 2) ? 1.0e30f : Uniform(0.0f, 2.0f);
        Vector4 nearestRef = nearest;
        u32 count = 1 + Next() % 4;
        for (u32 n = 0; n < count; n++)
        {
            alignas(16) Vector4 triangle[3];
            for (Vector4& v : triangle)
            {
                // Mostly around the ray, so it's hit now and then
                f32 t = Uniform(-0.2f, 1.2f);
                v.x = start.x + (end.x - start.x) * t + Uniform(-30.0f, 30.0f);
                v.y = start.y + (end.y - start.y) * t + Uniform(-30.0f, 30.0f);
                v.z = start.z + (end.z - start.z) * t + Uniform(-30.0f, 30.0f);
                v.w = (Next() % 8) ? 1.0f : GameFloat();
            }
            if (Next() % 16 == 0)
                RandomVector(&triangle[Next() % 3], true);
            M::StartRayTriangle(triangle, &nearest);
            M::Ref_StartRayTriangle(triangle, &nearestRef);
            bool x = M::FinishRayTriangle(&nearest);
            bool y = M::Ref_FinishRayTriangle(&nearestRef);
            if (x != y)
            {
                s_Failure = "hit";
                return false;
            }
            if (!SameBytes(&nearest, &nearestRef, 16, "nearest") || !SameState())
                return false;
        }
        return true;
    }, true);
    Ref::LoadMicrocodeSet(2);
    NativeMath::Vu0ProgramsLoaded(2);
    RandomData();
    Test("Platform::Math::ViewDistance", [] {
        RandomState();
        s32 view = static_cast<s32>(Next() % 8) * 19;
        f32 point[3] = {GameFloat(), GameFloat(), GameFloat()};
        u32 x = ToBits(M::ViewDistance(view, point));
        u32 y = ToBits(M::Ref_ViewDistance(view, point));
        return SameBytes(&x, &y, 4, "distance");
    }, true);
    Test("Platform::Math::ParticleBlockView", [] {
        RandomState();
        if (Next() % 64 == 0)
            RandomData();
        s32 view = static_cast<s32>(Next() % 8) * 19;
        bool keeps = Next() & 1;
        f32 extents[3] = {Uniform(0, 20), Uniform(0, 20), Uniform(0, 20)};
        Matrix4x4 gravity, x, y;
        RandomMatrix(&gravity, false);
        f32 place[3] = {GameFloat(), GameFloat(), GameFloat()};
        f32 scale = Uniform(0.0f, 1.0f);
        f32 d1, d2;
        bool o1 = M::ParticleBlockView(view, keeps, extents, &gravity, place, scale, &x, &d1);
        bool o2 = M::Ref_ParticleBlockView(view, keeps, extents, &gravity, place, scale, &y, &d2);
        if (o1 != o2)
        {
            s_Failure = "outside";
            return false;
        }
        return SameBytes(&x, &y, sizeof(x), "matrix") && SameBytes(&d1, &d2, 4, "distance");
    }, true);
    Ref::LoadMicrocodeSet(1);
    NativeMath::Vu0ProgramsLoaded(1);
}

void MatrixTests()
{
    Test("VuRotateVector (TransformVector_)", [] {
        RandomState();
        Matrix4x4 m;
        Vector4 v, x, y;
        RandomMatrix(&m, Next() % 8 == 0);
        RandomVector(&v, Next() % 8 == 0);
        VuRotateVector(&m, &v, &x);
        Ref_VuRotateVector(&m, &v, &y);
        return SameBytes(&x, &y, 16, "out");
    });
    Test("VuTransformPoint (MultiplyMatrixByVector)", [] {
        RandomState();
        Matrix4x4 m;
        Vector4 v, x, y;
        RandomMatrix(&m, Next() % 8 == 0);
        RandomVector(&v, Next() % 8 == 0);
        VuTransformPoint(&m, &v, &x);
        Ref_VuTransformPoint(&m, &v, &y);
        return SameBytes(&x, &y, 16, "out");
    });
    Test("MultiplyRotations (RotateVecByVec)", [] {
        RandomState();
        Vector4 a, b, x, y;
        RandomVector(&a, Next() % 8 == 0);
        RandomVector(&b, Next() % 8 == 0);
        MultiplyRotations(&x, &a, &b);
        Ref_MultiplyRotations(&y, &a, &b);
        return SameBytes(&x, &y, 16, "out");
    });
    Test("PlaneThroughTriangle (FUN_0018d830)", [] {
        RandomState();
        Vector4 a, b, c, x, y;
        RandomVector(&a, Next() % 8 == 0);
        RandomVector(&b, Next() % 8 == 0);
        RandomVector(&c, Next() % 8 == 0);
        if (Next() % 8 == 0)
            c = b;
        u32 r1 = PlaneThroughTriangle(&x, &a, &b, &c);
        u32 r2 = Ref_PlaneThroughTriangle(&y, &a, &b, &c);
        return SameBytes(&r1, &r2, 4, "result") && SameBytes(&x, &y, 16, "plane");
    });
    Test("VuTransformByRows (FUN_0018ecc0)", [] {
        RandomState();
        Matrix4x4 m;
        Vector4 v, x, y;
        RandomMatrix(&m, Next() % 8 == 0);
        RandomVector(&v, Next() % 8 == 0);
        VuTransformByRows(&m, &v, &x);
        Ref_VuTransformByRows(&m, &v, &y);
        return SameBytes(&x, &y, 16, "out");
    });
    Test("InitIdentityMatrix", [] {
        RandomState();
        Matrix4x4 x, y;
        RandomMatrix(&x, true);
        y = x;
        InitIdentityMatrix(&x);
        Ref_InitIdentityMatrix(&y);
        return SameBytes(&x, &y, sizeof(x), "matrix");
    });
    Test("VuMultiplyMatrices (MultiplyMatrices)", [] {
        RandomState();
        Matrix4x4 a, b, x, y;
        RandomMatrix(&a, Next() % 8 == 0);
        RandomMatrix(&b, Next() % 8 == 0);
        if (Next() % 4 == 0)
        {
            // Into one of its operands
            x = y = a;
            VuMultiplyMatrices(&x, &b, &x);
            Ref_VuMultiplyMatrices(&y, &b, &y);
        }
        else
        {
            VuMultiplyMatrices(&a, &b, &x);
            Ref_VuMultiplyMatrices(&a, &b, &y);
        }
        return SameBytes(&x, &y, sizeof(x), "out");
    });
    Test("MatrixFromRotation (Rotate_)", [] {
        RandomState();
        Vector4 q;
        Matrix4x4 x, y;
        UnitQuaternion(&q);
        if (Next() % 8 == 0)
            RandomVector(&q, true);
        RandomMatrix(&x, true);
        y = x;
        MatrixFromRotation(&x, &q);
        Ref_MatrixFromRotation(&y, &q);
        return SameBytes(&x, &y, sizeof(x), "matrix");
    });
    Test("VuTranspose (FUN_0018ee40)", [] {
        RandomState();
        Matrix4x4 m, x, y;
        RandomMatrix(&m, true);
        VuTranspose(&m, &x);
        Ref_VuTranspose(&m, &y);
        return SameBytes(&x, &y, sizeof(x), "out");
    });
    Test("TransposeQuadwords (FUN_0018ee88)", [] {
        RandomState();
        Matrix4x4 x, y;
        RandomMatrix(&x, true);
        y = x;
        TransposeQuadwords(&x);
        Ref_TransposeQuadwords(&y);
        return SameBytes(&x, &y, sizeof(x), "rows");
    });
    Test("VuTransposeRotation (CopyMatrix_)", [] {
        RandomState();
        Matrix4x4 m, x, y;
        RandomMatrix(&m, true);
        VuTransposeRotation(&m, &x);
        Ref_VuTransposeRotation(&m, &y);
        return SameBytes(&x, &y, sizeof(x), "out");
    });
    Test("VuInvertRigid (FUN_0018ef18)", [] {
        RandomState();
        Matrix4x4 m, x, y;
        if (Next() % 4)
            Rotation(&m);
        else
            RandomMatrix(&m, true);
        VuInvertRigid(&x, &m);
        Ref_VuInvertRigid(&y, &m);
        return SameBytes(&x, &y, sizeof(x), "out");
    });
    Test("VuInvertRigidInPlace (FUN_0018ef98)", [] {
        RandomState();
        Matrix4x4 x, y;
        Rotation(&x);
        y = x;
        VuInvertRigidInPlace(&x);
        Ref_VuInvertRigidInPlace(&y);
        return SameBytes(&x, &y, sizeof(x), "matrix");
    });
}

void CollisionTests()
{
    Test("Start/FinishTriangleEdgeTests (FUN_00293340, FUN_002933e4)", [] {
        RandomState();
        Vector4 start, end;
        CollisionHit triangle;
        RandomVector(&start, Next() % 8 == 0);
        RandomVector(&end, Next() % 8 == 0);
        for (Vector4& v : triangle.vertices)
            RandomVector(&v, Next() % 8 == 0);
        triangle.surface = 0;
        triangle.unused32 = 0;
        f32 x[6], y[6];
        std::memset(x, 0, sizeof(x));
        std::memset(y, 0, sizeof(y));
        StartTriangleEdgeTests(&start, &end, &triangle, x);
        Ref_StartTriangleEdgeTests(&start, &end, &triangle, y);
        if (!SameBytes(x, y, 8, "values") || !SameState())
            return false;
        FinishTriangleEdgeTests(x);
        Ref_FinishTriangleEdgeTests(y);
        return SameBytes(x, y, sizeof(x), "values");
    });
    Test("MakeVertexDifferences (FUN_0029345c)", [] {
        RandomState();
        alignas(16) Vector4 vertices[10], other[10], moved1[20], moved2[20], differences1[64], differences2[64];
        Matrix4x4 m, n;
        RandomMatrix(&m, Next() % 8 == 0);
        RandomMatrix(&n, Next() % 8 == 0);
        for (Vector4& v : vertices)
            RandomVector(&v, Next() % 8 == 0);
        for (Vector4& v : other)
            RandomVector(&v, Next() % 8 == 0);
        std::memset(moved1, 0, sizeof(moved1));
        std::memset(moved2, 0, sizeof(moved2));
        std::memset(differences1, 0, sizeof(differences1));
        std::memset(differences2, 0, sizeof(differences2));
        // The vertexes are read one past each list's end
        u32 count = 1 + Next() % 8;
        u32 otherCount = 1 + Next() % 8;
        VertexDifferences a = {&m, vertices, count, &n, other, otherCount, differences1, moved1};
        MakeVertexDifferences(&a);
        // The reference reads the request's pointers as the PS2's 32 bit words: the buffers mapped at low addresses for it
        Ref::MapLow(0x10000000, &m, sizeof(m));
        Ref::MapLow(0x11000000, vertices, sizeof(vertices));
        Ref::MapLow(0x12000000, &n, sizeof(n));
        Ref::MapLow(0x13000000, other, sizeof(other));
        Ref::MapLow(0x14000000, differences2, sizeof(differences2));
        Ref::MapLow(0x15000000, moved2, sizeof(moved2));
        alignas(16) u32 request[8] = {0x10000000, 0x11000000, count, 0x12000000, 0x13000000, otherCount, 0x14000000, 0x15000000};
        Ref_MakeVertexDifferences(reinterpret_cast<const VertexDifferences*>(request));
        Ref::UnmapLow();
        return SameBytes(moved1, moved2, sizeof(moved1), "moved") && SameBytes(differences1, differences2, sizeof(differences1), "differences");
    });
    Test("GroupPoints (FUN_00293540)", [] {
        RandomState();
        alignas(16) Vector4 x[16], y[16];
        for (Vector4& v : x)
            RandomVector(&v, true);
        std::memcpy(y, x, sizeof(x));
        s32 groups = 1 + static_cast<s32>(Next() % 4);
        GroupPoints(x, groups);
        Ref_GroupPoints(y, groups);
        return SameBytes(x, y, sizeof(x), "points");
    });
    Test("GroupsRange (FUN_0029359c)", [] {
        RandomState();
        alignas(16) Vector4 groups[3 * 6];
        for (Vector4& v : groups)
            RandomVector(&v, Next() % 16 == 0);
        Vector4 axis;
        RandomVector(&axis, Next() % 8 == 0);
        s32 count = 1 + static_cast<s32>(Next() % 5);
        f32 x[2], y[2];
        GroupsRange(groups, count, &axis, x);
        Ref_GroupsRange(groups, count, &axis, y);
        return SameBytes(x, y, sizeof(x), "range");
    });
    Test("SixteenGroupsRange (FUN_00293624)", [] {
        RandomState();
        alignas(16) Vector4 groups[3 * 18];
        for (Vector4& v : groups)
            RandomVector(&v, Next() % 16 == 0);
        Vector4 axis;
        RandomVector(&axis, Next() % 8 == 0);
        f32 x[2], y[2];
        SixteenGroupsRange(groups, &axis, x);
        Ref_SixteenGroupsRange(groups, &axis, y);
        return SameBytes(x, y, sizeof(x), "range");
    });
    Test("Load/TriangleHullSupport(Range) (FUN_002936d8, FUN_002938bc)", [] {
        RandomState();
        CollisionHit triangle;
        alignas(16) Vector4 hull[8];
        for (Vector4& v : triangle.vertices)
            RandomVector(&v, Next() % 8 == 0);
        triangle.surface = 0;
        triangle.unused32 = 0;
        for (Vector4& v : hull)
            RandomVector(&v, Next() % 8 == 0);
        LoadTriangleHullSupport(&triangle, hull);
        Ref_LoadTriangleHullSupport(&triangle, hull);
        if (!SameState())
            return false;
        u32 axes = 1 + Next() % 3;
        for (u32 n = 0; n < axes; n++)
        {
            Vector4 axis;
            RandomVector(&axis, Next() % 8 == 0);
            f32 x[2], y[2];
            TriangleHullSupportRange(&axis, x);
            Ref_TriangleHullSupportRange(&axis, y);
            if (!SameBytes(x, y, sizeof(x), "range") || !SameState())
                return false;
        }
        return true;
    });
}

void BoundsTests()
{
    Test("GetBbox", [] {
        RandomState();
        alignas(16) Vector4 points[12];
        for (Vector4& v : points)
            RandomVector(&v, Next() % 8 == 0);
        Matrix4x4 m;
        RandomMatrix(&m, Next() % 8 == 0);
        s32 count = 1 + static_cast<s32>(Next() % 12);
        Box x, y;
        GetBbox(points, count, &x, &m);
        Ref_GetBbox(points, count, &y, &m);
        return SameBytes(&x, &y, sizeof(x), "box");
    });
    Test("PointsBox (FUN_00201bb8)", [] {
        RandomState();
        alignas(16) Vector4 points[12];
        for (Vector4& v : points)
            RandomVector(&v, true);
        s32 count = 1 + static_cast<s32>(Next() % 12);
        Box x, y;
        PointsBox(points, count, &x);
        Ref_PointsBox(points, count, &y);
        return SameBytes(&x, &y, sizeof(x), "box");
    });
    Test("TriangleBounds (FUN_00201b90)", [] {
        RandomState();
        Vector4 a, b, c;
        RandomVector(&a, true);
        RandomVector(&b, true);
        RandomVector(&c, true);
        Box x, y;
        TriangleBounds(&x, &a, &b, &c);
        Ref_TriangleBounds(&y, &a, &b, &c);
        return SameBytes(&x, &y, sizeof(x), "box");
    });
}

// ------------------------------------------------------------------------------------------------------------------------------
// The PCSX2 probe (pcsx2/probe.h): cases made here, run by the PS2 build in PCSX2 and checked here against the native functions

void ProbeCase(Probe::Case* c, u32 function)
{
    std::memset(c, 0, sizeof(*c));
    c->function = function;
    c->seed = Bits32() | 1;
    auto* v = reinterpret_cast<Vector4*>(c->input);
    auto* f = reinterpret_cast<f32*>(c->input);
    for (u32 n = 0; n < Probe::InputBytes / 4; n++)
        f[n] = GameFloat();
    for (int n = 0; n < 4; n++)
        c->floats[n] = GameFloat();
    switch (function)
    {
    case Probe::SinCos:
        c->floats[0] = (Next() % 4) ? Uniform(-10.0f, 10.0f) : AnyFloat();
        c->floats[1] = (Next() % 4) ? Uniform(-1000.0f, 1000.0f) : AnyFloat();
        break;
    case Probe::Slerp:
    {
        UnitQuaternion(&v[0]);
        u32 kind = Next() % 4;
        if (kind == 0)
        {
            v[1] = v[0];
            f32* t = reinterpret_cast<f32*>(&v[1]);
            for (int n = 0; n < 4; n++)
                t[n] += Uniform(-0.02f, 0.02f);
            if (Next() & 1)
                for (int n = 0; n < 4; n++)
                    t[n] = -t[n];
        }
        else
            UnitQuaternion(&v[1]);
        if (kind == 1)
            for (int n = 0; n < 4; n++)
                reinterpret_cast<f32*>(&v[1])[n] = -reinterpret_cast<f32*>(&v[1])[n];
        c->floats[0] = Uniform(0.0f, 1.0f);
        break;
    }
    case Probe::Euler:
        for (int n = 0; n < 8; n++)
            f[n] = Uniform(-7.0f, 7.0f);
        v[1].w = Uniform(0.0f, 1.0f);
        break;
    case Probe::Turn:
        UnitQuaternion(&v[0]);
        UnitQuaternion(&v[1]);
        break;
    case Probe::Lerp:
        c->floats[0] = Uniform(0.0f, 1.0f);
        break;
    case Probe::Joint:
        UnitQuaternion(&v[0]);
        for (int n = 4; n < 12; n++)
            f[n] = Uniform(0.1f, 3.0f);
        Rotation(reinterpret_cast<Matrix4x4*>(&v[4]));
        c->ints[0] = Bits32() & 31;
        break;
    case Probe::RayTriangles:
    {
        v[0].w = v[1].w = 1.0f;
        v[2].w = (Next() % 2) ? 1.0e30f : Uniform(0.0f, 2.0f);
        c->ints[0] = 1 + Next() % 4;
        for (u32 n = 0; n < 12; n++)
        {
            f32 t = Uniform(-0.2f, 1.2f);
            Vector4& p = v[3 + n];
            p.x = v[0].x + (v[1].x - v[0].x) * t + Uniform(-30.0f, 30.0f);
            p.y = v[0].y + (v[1].y - v[0].y) * t + Uniform(-30.0f, 30.0f);
            p.z = v[0].z + (v[1].z - v[0].z) * t + Uniform(-30.0f, 30.0f);
            p.w = 1.0f;
        }
        break;
    }
    case Probe::ViewDistance:
        c->ints[0] = static_cast<u32>(Next() % 8) * 19;
        break;
    case Probe::ParticleView:
        c->ints[0] = static_cast<u32>(Next() % 8) * 19;
        c->ints[1] = Next() & 1;
        for (int n = 0; n < 3; n++)
            f[n] = Uniform(0.0f, 20.0f);
        c->floats[0] = Uniform(0.0f, 1.0f);
        break;
    case Probe::Max:
    case Probe::Min:
        c->floats[0] = AnyFloat();
        c->floats[1] = AnyFloat();
        break;
    case Probe::DivideBySquareRoot:
        c->floats[0] = (Next() % 2) ? 1.0f : GameFloat();
        c->floats[1] = Uniform(0.0f, 1000.0f);
        break;
    case Probe::InvertRigid:
    case Probe::InvertRigidInPlace:
        Rotation(reinterpret_cast<Matrix4x4*>(&v[0]));
        break;
    case Probe::MatrixFromRotation:
        UnitQuaternion(&v[0]);
        break;
    case Probe::VertexDifferences:
        c->ints[0] = 1 + Next() % 8;
        c->ints[1] = 1 + Next() % 8;
        break;
    case Probe::GroupPoints:
        c->ints[0] = 1 + Next() % 4;
        break;
    case Probe::GroupsRange:
        c->ints[0] = 1 + Next() % 5;
        break;
    case Probe::HullSupport:
        c->ints[0] = 1 + Next() % 3;
        break;
    case Probe::GetBbox:
    case Probe::PointsBox:
        c->ints[0] = 1 + Next() % 12;
        break;
    }
}

bool g_IsMicroprogramFunction[Probe::FunctionCount] = {};

int MakeProbeInputs(const char* path)
{
    std::vector<Probe::Case> cases;
    for (u32 function = 1; function < Probe::FunctionCount; function++)
    {
        bool microprogram = function <= Probe::ParticleView;
        u32 count = microprogram ? 48 : 12;
        for (u32 n = 0; n < count; n++)
        {
            Probe::Case c;
            ProbeCase(&c, function);
            cases.push_back(c);
        }
    }
    FILE* out = std::fopen(path, "wb");
    Probe::Header header = {Probe::Magic, static_cast<u32>(cases.size()), {0, 0}};
    std::fwrite(&header, sizeof(header), 1, out);
    std::vector<u8> data(Probe::DataBytes);
    for (u32 word = 0; word < Probe::DataBytes / 4; word++)
    {
        u32 value = ToBits(GameFloat());
        std::memcpy(&data[word * 4], &value, 4);
    }
    std::fwrite(data.data(), 1, data.size(), out);
    std::fwrite(cases.data(), sizeof(Probe::Case), cases.size(), out);
    std::fclose(out);
    std::printf("%zu cases (%zu bytes) in %s\n", cases.size(), sizeof(Probe::Header) + Probe::DataBytes + cases.size() * sizeof(Probe::Case), path);
    return 0;
}

std::vector<u8> ReadFile(const char* path)
{
    std::vector<u8> bytes;
    FILE* in = std::fopen(path, "rb");
    if (in == nullptr)
    {
        std::printf("no %s\n", path);
        std::exit(2);
    }
    u8 buffer[65536];
    size_t got;
    while ((got = std::fread(buffer, 1, sizeof(buffer), in)) > 0)
        bytes.insert(bytes.end(), buffer, buffer + got);
    std::fclose(in);
    return bytes;
}

int CheckProbe(const char* inputsPath, const char* resultsPath)
{
    std::vector<u8> inputs = ReadFile(inputsPath);
    std::vector<u8> results = ReadFile(resultsPath);
    const auto* header = reinterpret_cast<const Probe::Header*>(inputs.data());
    const u8* data = inputs.data() + sizeof(Probe::Header);
    const auto* cases = reinterpret_cast<const Probe::Case*>(data + Probe::DataBytes);
    u32 count = header->count;
    if (results.size() < count * sizeof(Probe::Result))
    {
        std::printf("the results have %zu bytes, %u cases need %zu\n", results.size(), count, count * sizeof(Probe::Result));
        return 2;
    }
    std::memcpy(TheVu0().data, data, Probe::DataBytes);
    std::map<u32, std::pair<u32, u32>> perFunction;
    u32 failures = 0;
    for (u32 n = 0; n < count; n++)
    {
        const Probe::Case& c = cases[n];
        bool culling = c.function == Probe::ViewDistance || c.function == Probe::ParticleView;
        NativeMath::Vu0ProgramsLoaded(culling ? 2 : 1);
        alignas(16) Probe::Result native;
        Probe::RunCase(c, &native);
        const auto& ps2 = reinterpret_cast<const Probe::Result*>(results.data())[n];
        auto& tally = perFunction[c.function];
        tally.first++;
        if (std::memcmp(&native, &ps2, sizeof(native)) != 0)
        {
            tally.second++;
            if (failures++ < 20)
            {
                const auto* a = reinterpret_cast<const u32*>(&native);
                const auto* b = reinterpret_cast<const u32*>(&ps2);
                for (u32 word = 0; word < sizeof(native) / 4; word++)
                    if (a[word] != b[word])
                    {
                        std::printf("case %u (function %u): word 0x%X native %08X PCSX2 %08X\n", n, c.function, word * 4, a[word], b[word]);
                        break;
                    }
            }
        }
    }
    for (const auto& [function, tally] : perFunction)
        std::printf("function %2u: %3u cases, %u differ\n", function, tally.first, tally.second);
    std::printf(failures ? "%u cases differ from PCSX2\n" : "all %u cases match PCSX2\n", failures ? failures : count);
    return failures ? 1 : 0;
}
}

int main(int argc, char** argv)
{
    if (argc > 2 && std::strcmp(argv[1], "--probe-inputs") == 0)
        return MakeProbeInputs(argv[2]);
    if (argc > 3 && std::strcmp(argv[1], "--probe-check") == 0)
        return CheckProbe(argv[2], argv[3]);
    if (argc > 1)
        s_Iterations = static_cast<u32>(std::strtoul(argv[1], nullptr, 0));
    if (argc > 2)
        s_Seed = std::strtoull(argv[2], nullptr, 0) | 1;
    std::printf("seed 0x%llX, %u runs per function\n", static_cast<unsigned long long>(s_Seed), s_Iterations);
    Ref::LoadMicrocodeSet(1);
    NativeMath::Vu0ProgramsLoaded(1);
    FloatTests();
    MathTests();
    MatrixTests();
    CollisionTests();
    BoundsTests();
    u32 failed = 0;
    for (const auto& [name, result] : s_Results)
    {
        std::printf("%-62s %8u runs %s", name.c_str(), result.runs, result.failures ? "" : "ok");
        if (result.failures)
            std::printf("%u FAILED", result.failures);
        if (!result.paths.empty())
        {
            std::printf("  (instructions run:");
            for (const auto& [instructions, count] : result.paths)
                std::printf(" %u×%u", instructions, count);
            std::printf(")");
        }
        std::printf("\n");
        failed += result.failures ? 1 : 0;
    }
    std::printf(failed ? "%u functions FAILED\n" : "all passed\n", failed);
    return failed ? 1 : 0;
}
