// The PCSX2 probe's PS2 side (built only into a scratch copy of the PS2 build by run_probe.py, never into the game): the cases
// of g_ProbeFile run on the PS2 build's own maths at the start of Main, the results in g_ProbeResults, g_ProbeDone set at the end

#include "probe.h"

#include "platform/graphics.h"

extern "C"
{
    extern const u8 g_ProbeFile[];
    alignas(16) Probe::Result g_ProbeResults[PROBE_CASES];
    volatile u32 g_ProbeDone = 0;
}

namespace Probe
{
void SetState(const Start& start)
{
    asm volatile("lqc2 $vf1, 0x10(%0)\n\t"
                 "lqc2 $vf2, 0x20(%0)\n\t"
                 "lqc2 $vf3, 0x30(%0)\n\t"
                 "lqc2 $vf4, 0x40(%0)\n\t"
                 "lqc2 $vf5, 0x50(%0)\n\t"
                 "lqc2 $vf6, 0x60(%0)\n\t"
                 "lqc2 $vf7, 0x70(%0)\n\t"
                 "lqc2 $vf8, 0x80(%0)\n\t"
                 "lqc2 $vf9, 0x90(%0)\n\t"
                 "lqc2 $vf10, 0xA0(%0)\n\t"
                 "lqc2 $vf11, 0xB0(%0)\n\t"
                 "lqc2 $vf12, 0xC0(%0)\n\t"
                 "lqc2 $vf13, 0xD0(%0)\n\t"
                 "lqc2 $vf14, 0xE0(%0)\n\t"
                 "lqc2 $vf15, 0xF0(%0)\n\t"
                 "lqc2 $vf16, 0x100(%0)\n\t"
                 "lqc2 $vf17, 0x110(%0)\n\t"
                 "lqc2 $vf18, 0x120(%0)\n\t"
                 "lqc2 $vf19, 0x130(%0)\n\t"
                 "lqc2 $vf20, 0x140(%0)\n\t"
                 "lqc2 $vf21, 0x150(%0)\n\t"
                 "lqc2 $vf22, 0x160(%0)\n\t"
                 "lqc2 $vf23, 0x170(%0)\n\t"
                 "lqc2 $vf24, 0x180(%0)\n\t"
                 "lqc2 $vf25, 0x190(%0)\n\t"
                 "lqc2 $vf26, 0x1A0(%0)\n\t"
                 "lqc2 $vf27, 0x1B0(%0)\n\t"
                 "lqc2 $vf28, 0x1C0(%0)\n\t"
                 "lqc2 $vf29, 0x1D0(%0)\n\t"
                 "lqc2 $vf30, 0x1E0(%0)\n\t"
                 "lqc2 $vf31, 0x1F0(%0)\n\t"
                 "lw $8, 0x204(%0)\n\tctc2.ni $8, $vi1\n\t"
                 "lw $8, 0x208(%0)\n\tctc2.ni $8, $vi2\n\t"
                 "lw $8, 0x20C(%0)\n\tctc2.ni $8, $vi3\n\t"
                 "lw $8, 0x210(%0)\n\tctc2.ni $8, $vi4\n\t"
                 "lw $8, 0x214(%0)\n\tctc2.ni $8, $vi5\n\t"
                 "lw $8, 0x218(%0)\n\tctc2.ni $8, $vi6\n\t"
                 "lw $8, 0x21C(%0)\n\tctc2.ni $8, $vi7\n\t"
                 "lw $8, 0x220(%0)\n\tctc2.ni $8, $vi8\n\t"
                 "lw $8, 0x224(%0)\n\tctc2.ni $8, $vi9\n\t"
                 "lw $8, 0x228(%0)\n\tctc2.ni $8, $vi10\n\t"
                 "lw $8, 0x22C(%0)\n\tctc2.ni $8, $vi11\n\t"
                 "lw $8, 0x230(%0)\n\tctc2.ni $8, $vi12\n\t"
                 "lw $8, 0x234(%0)\n\tctc2.ni $8, $vi13\n\t"
                 "lw $8, 0x238(%0)\n\tctc2.ni $8, $vi14\n\t"
                 "lw $8, 0x23C(%0)\n\tctc2.ni $8, $vi15\n\t"
                 "lw $8, 0x240(%0)\n\tctc2.ni $8, $vi21\n\t"
                 "vaddax.xyzw $ACC, $vf31, $vf0x\n\t"
                 "vsqrt $Q, $vf30x\n\t"
                 "vwaitq\n\t"
                 "vrinit $R, $vf29x\n\t"
                 "vclipw.xyz $vf28, $vf27w\n\t"
                 "vclipw.xyz $vf28, $vf27w\n\t"
                 "vclipw.xyz $vf28, $vf27w\n\t"
                 "vclipw.xyz $vf28, $vf27w\n\t"
                 "vnop"
                 :
                 : "r"(&start)
                 : "$8", "memory");
}

void ReadState(Result* result)
{
    asm volatile("sqc2 $vf0, 0x0(%0)\n\t"
                 "sqc2 $vf1, 0x10(%0)\n\t"
                 "sqc2 $vf2, 0x20(%0)\n\t"
                 "sqc2 $vf3, 0x30(%0)\n\t"
                 "sqc2 $vf4, 0x40(%0)\n\t"
                 "sqc2 $vf5, 0x50(%0)\n\t"
                 "sqc2 $vf6, 0x60(%0)\n\t"
                 "sqc2 $vf7, 0x70(%0)\n\t"
                 "sqc2 $vf8, 0x80(%0)\n\t"
                 "sqc2 $vf9, 0x90(%0)\n\t"
                 "sqc2 $vf10, 0xA0(%0)\n\t"
                 "sqc2 $vf11, 0xB0(%0)\n\t"
                 "sqc2 $vf12, 0xC0(%0)\n\t"
                 "sqc2 $vf13, 0xD0(%0)\n\t"
                 "sqc2 $vf14, 0xE0(%0)\n\t"
                 "sqc2 $vf15, 0xF0(%0)\n\t"
                 "sqc2 $vf16, 0x100(%0)\n\t"
                 "sqc2 $vf17, 0x110(%0)\n\t"
                 "sqc2 $vf18, 0x120(%0)\n\t"
                 "sqc2 $vf19, 0x130(%0)\n\t"
                 "sqc2 $vf20, 0x140(%0)\n\t"
                 "sqc2 $vf21, 0x150(%0)\n\t"
                 "sqc2 $vf22, 0x160(%0)\n\t"
                 "sqc2 $vf23, 0x170(%0)\n\t"
                 "sqc2 $vf24, 0x180(%0)\n\t"
                 "sqc2 $vf25, 0x190(%0)\n\t"
                 "sqc2 $vf26, 0x1A0(%0)\n\t"
                 "sqc2 $vf27, 0x1B0(%0)\n\t"
                 "sqc2 $vf28, 0x1C0(%0)\n\t"
                 "sqc2 $vf29, 0x1D0(%0)\n\t"
                 "sqc2 $vf30, 0x1E0(%0)\n\t"
                 "sqc2 $vf31, 0x1F0(%0)\n\t"
                 "vmaddx.xyzw $vf1, $vf0, $vf0x\n\t"
                 "sqc2 $vf1, 0x200(%0)\n\t"
                 "cfc2.ni $8, $vi1\n\tsw $8, 0x214(%0)\n\t"
                 "cfc2.ni $8, $vi2\n\tsw $8, 0x218(%0)\n\t"
                 "cfc2.ni $8, $vi3\n\tsw $8, 0x21C(%0)\n\t"
                 "cfc2.ni $8, $vi4\n\tsw $8, 0x220(%0)\n\t"
                 "cfc2.ni $8, $vi5\n\tsw $8, 0x224(%0)\n\t"
                 "cfc2.ni $8, $vi6\n\tsw $8, 0x228(%0)\n\t"
                 "cfc2.ni $8, $vi7\n\tsw $8, 0x22C(%0)\n\t"
                 "cfc2.ni $8, $vi8\n\tsw $8, 0x230(%0)\n\t"
                 "cfc2.ni $8, $vi9\n\tsw $8, 0x234(%0)\n\t"
                 "cfc2.ni $8, $vi10\n\tsw $8, 0x238(%0)\n\t"
                 "cfc2.ni $8, $vi11\n\tsw $8, 0x23C(%0)\n\t"
                 "cfc2.ni $8, $vi12\n\tsw $8, 0x240(%0)\n\t"
                 "cfc2.ni $8, $vi13\n\tsw $8, 0x244(%0)\n\t"
                 "cfc2.ni $8, $vi14\n\tsw $8, 0x248(%0)\n\t"
                 "cfc2.ni $8, $vi15\n\tsw $8, 0x24C(%0)\n\t"
                 "cfc2.ni $8, $vi17\n\tsw $8, 0x250(%0)\n\t"
                 "cfc2.ni $8, $vi18\n\tsw $8, 0x254(%0)\n\t"
                 "cfc2.ni $8, $vi20\n\tsw $8, 0x258(%0)\n\t"
                 "cfc2.ni $8, $vi21\n\tsw $8, 0x25C(%0)\n\t"
                 "cfc2.ni $8, $vi22\n\tsw $8, 0x260(%0)"
                 :
                 : "r"(result)
                 : "$8", "memory");
}
}

extern "C" void RunMathProbe()
{
    const auto* header = reinterpret_cast<const Probe::Header*>(g_ProbeFile);
    const u8* data = g_ProbeFile + sizeof(Probe::Header);
    const auto* cases = reinterpret_cast<const Probe::Case*>(data + Probe::DataBytes);
    // VU0's data memory (the particle views the culling programs read), from the EE's side
    auto* vu0Data = reinterpret_cast<volatile u32*>(0x11004000);
    for (u32 word = 0; word < Probe::DataBytes / 4; word++)
    {
        vu0Data[word] = reinterpret_cast<const u32*>(data)[word];
    }

    for (u32 n = 0; n < header->count && n < PROBE_CASES; n++)
    {
        bool culling = cases[n].function == Probe::ViewDistance || cases[n].function == Probe::ParticleView;
        Platform::Graphics::UseHelperPrograms(culling ? Platform::Graphics::CullingPrograms : Platform::Graphics::StandardPrograms,
                                              true);
        Probe::RunCase(cases[n], &g_ProbeResults[n]);
    }

    Platform::Graphics::UseHelperPrograms(Platform::Graphics::StandardPrograms, true);
    g_ProbeDone = Probe::Done;
}
