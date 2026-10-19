#pragma once

// The EE's 128 bit registers as the maths' asm moves words through them (LQ, SQ, QMFC2, QMTC2 and the MMI word shuffles), so the
// translations keep the asm's steps

#include "common.h"

#include <cstring>

namespace NativeMath
{
struct Quad
{
    u32 w[4];
};

inline Quad LoadQuad(const void* address)
{
    Quad quad;
    std::memcpy(quad.w, address, sizeof(quad.w));
    return quad;
}

inline void StoreQuad(void* address, const Quad& quad)
{
    std::memcpy(address, quad.w, sizeof(quad.w));
}

// PEXTLW rd, rs, rt: the low words interleaved (rt's first)
inline Quad Pextlw(const Quad& rs, const Quad& rt)
{
    return {{rt.w[0], rs.w[0], rt.w[1], rs.w[1]}};
}

// PEXTUW rd, rs, rt: the high words interleaved (rt's first)
inline Quad Pextuw(const Quad& rs, const Quad& rt)
{
    return {{rt.w[2], rs.w[2], rt.w[3], rs.w[3]}};
}

// PCPYLD rd, rs, rt: rt's low doubleword, then rs's
inline Quad Pcpyld(const Quad& rs, const Quad& rt)
{
    return {{rt.w[0], rt.w[1], rs.w[0], rs.w[1]}};
}

// PCPYUD rd, rs, rt: rs's high doubleword, then rt's
inline Quad Pcpyud(const Quad& rs, const Quad& rt)
{
    return {{rs.w[2], rs.w[3], rt.w[2], rt.w[3]}};
}
}
