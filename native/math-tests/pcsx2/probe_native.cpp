// The PCSX2 probe's native side: probe_ps2.cpp's state set-up and read-out, an instruction at a time on the native VU0

#include "probe.h"

#include "vu0.h"

#include <cstring>

namespace Probe
{
using namespace NativeMath;

void SetState(const Start& start)
{
    Vu0& vu = TheVu0();
    for (u32 reg = 1; reg < 32; reg++)
    {
        vu.Lqc2(reg, start.vf[reg]);
    }

    for (u32 reg = 1; reg < 16; reg++)
    {
        vu.Ctc2(reg, start.vi[reg]);
    }

    vu.Ctc2(21, start.i);
    vu.AddaBc(XYZW, 31, 0, Fx);
    vu.Sqrt(30, Fx);
    vu.Rinit(29, Fx);
    for (u32 n = 0; n < 4; n++)
    {
        vu.Clip(28, 27);
    }
}

void ReadState(Result* result)
{
    Vu0& vu = TheVu0();
    for (u32 reg = 0; reg < 32; reg++)
    {
        vu.Sqc2(reg, result->vf[reg]);
    }

    vu.MaddBc(XYZW, 1, 0, 0, Fx);
    vu.Sqc2(1, result->acc);
    for (u32 reg = 1; reg < 16; reg++)
    {
        result->vi[reg] = vu.Cfc2(reg);
    }

    // CFC2 of MAC (16 bits), clip, R (its 23 bits of fraction), I, Q
    result->mac = vu.mac & 0xFFFF;
    result->clip = vu.clip;
    result->r = vu.r & 0x7FFFFF;
    result->i = vu.i;
    result->q = vu.q;
}
}
