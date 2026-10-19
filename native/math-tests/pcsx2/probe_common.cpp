// A probe case run on the functions of whichever side this is built into (see probe.h)

#include "probe.h"

#include "game/collision.h"
#include "game/hull.h"
#include "game/math.h"
#include "game/physics.h"
#include "platform/graphics.h"
#include "platform/math.h"

namespace Probe
{
namespace
{
void Copy(void* to, const void* from, u32 bytes)
{
    auto* out = static_cast<u8*>(to);
    const auto* in = static_cast<const u8*>(from);
    for (u32 n = 0; n < bytes; n++)
    {
        out[n] = in[n];
    }
}

void Clear(void* to, u32 bytes)
{
    auto* out = static_cast<u8*>(to);
    for (u32 n = 0; n < bytes; n++)
    {
        out[n] = 0;
    }
}

u32 Bits(f32 value)
{
    u32 bits;
    Copy(&bits, &value, 4);
    return bits;
}
}

void RunCase(const Case& probe, Result* result)
{
    namespace M = Platform::Math;
    Clear(result, sizeof(Result));
    alignas(16) Start start;
    MakeStart(probe.seed, &start);
    // The inputs as vectors (a copy: functions write into some), the outputs
    alignas(16) Vector4 in[InputBytes / 16];
    Copy(in, probe.input, InputBytes);
    auto* out = reinterpret_cast<Vector4*>(result->output);
    auto* outFloats = reinterpret_cast<f32*>(result->output);
    auto matrix = [&in](u32 at) { return reinterpret_cast<Matrix4x4*>(&in[at]); };
    auto* outMatrix = reinterpret_cast<Matrix4x4*>(result->output);
    const u32* ints = probe.ints;
    const f32* floats = probe.floats;

    SetState(start);
    switch (probe.function)
    {
    case SinCos:
        M::SinCos(floats[0], floats[1], outFloats);
        break;
    case Slerp:
        M::SlerpRotations(&in[0], &in[1], floats[0], &out[0]);
        break;
    case Euler:
        M::EulerRotation(&in[0], &in[1], &in[2], &in[3], &out[0], &out[1]);
        break;
    case Turn:
        M::TurnRotation(&in[0], &in[1]);
        out[0] = in[1];
        break;
    case Lerp:
        M::Lerp(&in[0], &in[1], floats[0], &out[0]);
        break;
    case Joint:
        M::JointMatrix((ints[0] & 1) ? &in[0] : nullptr, (ints[0] & 2) ? &in[1] : nullptr, (ints[0] & 4) ? &in[2] : nullptr,
                       (ints[0] & 8) ? &in[3] : nullptr, (ints[0] & 16) ? matrix(4) : nullptr, outMatrix);
        break;
    case MultiplyByParent:
        M::MultiplyByParent(matrix(0), matrix(4), outMatrix);
        break;
    case RayTriangles:
    {
        // start, end, nearest, then the triangles: each tested and finished, the nearest and the hit after each
        M::SetRay(&in[0], &in[1]);
        Vector4 nearest = in[2];
        u32 count = ints[0];
        for (u32 n = 0; n < count; n++)
        {
            M::StartRayTriangle(&in[3 + n * 3], &nearest);
            bool hit = M::FinishRayTriangle(&nearest);
            out[n * 2] = nearest;
            reinterpret_cast<u32*>(&out[n * 2 + 1])[0] = hit ? 1 : 0;
        }
        break;
    }
    case ViewDistance:
        result->value = Bits(M::ViewDistance(static_cast<s32>(ints[0]), floats));
        break;
    case ParticleView:
    {
        f32 distance;
        bool outside = M::ParticleBlockView(static_cast<s32>(ints[0]), ints[1] != 0, reinterpret_cast<const f32*>(&in[0]),
                                            matrix(1), reinterpret_cast<const f32*>(&in[5]), floats[0], outMatrix, &distance);
        result->value = outside ? 1 : 0;
        outFloats[16] = distance;
        break;
    }
    case Max:
        result->value = Bits(M::Max(floats[0], floats[1]));
        break;
    case Min:
        result->value = Bits(M::Min(floats[0], floats[1]));
        break;
    case DivideBySquareRoot:
        result->value = Bits(M::DivideBySquareRoot(floats[0], floats[1]));
        break;
    case RotateVector:
        VuRotateVector(matrix(0), &in[4], &out[0]);
        break;
    case TransformPoint:
        VuTransformPoint(matrix(0), &in[4], &out[0]);
        break;
    case MultiplyRotations:
        ::MultiplyRotations(&out[0], &in[0], &in[1]);
        break;
    case PlaneThroughTriangle:
        result->value = ::PlaneThroughTriangle(&out[0], &in[0], &in[1], &in[2]);
        break;
    case TransformByRows:
        VuTransformByRows(matrix(0), &in[4], &out[0]);
        break;
    case InitIdentity:
        Copy(outMatrix, matrix(0), sizeof(Matrix4x4));
        InitIdentityMatrix(outMatrix);
        break;
    case MultiplyMatrices:
        VuMultiplyMatrices(matrix(0), matrix(4), outMatrix);
        break;
    case MatrixFromRotation:
        Copy(outMatrix, matrix(1), sizeof(Matrix4x4));
        ::MatrixFromRotation(outMatrix, &in[0]);
        break;
    case Transpose:
        VuTranspose(matrix(0), outMatrix);
        break;
    case TransposeQuadwords:
        ::TransposeQuadwords(matrix(0));
        Copy(outMatrix, matrix(0), sizeof(Matrix4x4));
        break;
    case TransposeRotation:
        VuTransposeRotation(matrix(0), outMatrix);
        break;
    case InvertRigid:
        VuInvertRigid(outMatrix, matrix(0));
        break;
    case InvertRigidInPlace:
        VuInvertRigidInPlace(matrix(0));
        Copy(outMatrix, matrix(0), sizeof(Matrix4x4));
        break;
    case EdgeTests:
    {
        alignas(16) CollisionHit triangle;
        Clear(&triangle, sizeof(triangle));
        triangle.vertices[0] = in[2];
        triangle.vertices[1] = in[3];
        triangle.vertices[2] = in[4];
        StartTriangleEdgeTests(&in[0], &in[1], &triangle, outFloats);
        FinishTriangleEdgeTests(outFloats + 8);
        break;
    }
    case VertexDifferences:
    {
        ::VertexDifferences request;
        request.matrix = matrix(0);
        request.vertices = &in[8];
        request.count = ints[0];
        request.otherMatrix = matrix(4);
        request.otherVertices = &in[19];
        request.otherCount = ints[1];
        request.differences = &out[20];
        request.moved = &out[0];
        MakeVertexDifferences(&request);
        break;
    }
    case GroupPoints:
        ::GroupPoints(&in[0], static_cast<s32>(ints[0]));
        Copy(out, in, 16 * 16);
        break;
    case GroupsRange:
        ::GroupsRange(&in[0], static_cast<s32>(ints[0]), &in[18], outFloats);
        break;
    case SixteenGroupsRange:
        ::SixteenGroupsRange(&in[0], &in[54], outFloats);
        break;
    case HullSupport:
    {
        alignas(16) CollisionHit triangle;
        Clear(&triangle, sizeof(triangle));
        triangle.vertices[0] = in[0];
        triangle.vertices[1] = in[1];
        triangle.vertices[2] = in[2];
        LoadTriangleHullSupport(&triangle, &in[3]);
        for (u32 n = 0; n < ints[0]; n++)
        {
            TriangleHullSupportRange(&in[11 + n], outFloats + n * 2);
        }
        break;
    }
    case GetBbox:
        ::GetBbox(&in[0], static_cast<s32>(ints[0]), reinterpret_cast<Box*>(out), matrix(12));
        break;
    case PointsBox:
        ::PointsBox(&in[0], static_cast<s32>(ints[0]), reinterpret_cast<Box*>(out));
        break;
    case TriangleBounds:
        ::TriangleBounds(reinterpret_cast<Box*>(out), &in[0], &in[1], &in[2]);
        break;
    }
    ReadState(result);
}
}
