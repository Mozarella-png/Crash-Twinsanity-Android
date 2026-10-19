#include "renderer.h"

#include "platform/graphics.h"

#include "../ee/vu.h"
#include "../hardware.h"
#include "math/vu0.h"

#include <cstring>

// VU0's microcode sets (the game's maths helpers, which the asm's macro mode calls with vcallms): the set loaded and the DMA
// chain to VIF0 that loads each (sets 1 to 3; nothing gives set 0 one), a set loaded, and the copier of the third set that puts
// quadwords into VU0's memory

namespace
{
struct Vu0ProgramSets
{
    u32 loaded;
    const void* chains[4];
};

// The pause between two looks at the busy channel
constexpr s32 BusyPause = 0x1D;
}

extern "C"
{
    // The DMA chains to VIF0 of the three sets (in .vutext): the standard one, the culling's and the decals'
    // Native: places in the .vutext blob (0x2D9D90 on), which the data converter brings over whole
    extern const u8 g_VuText[] RETAIL(D_002D9D90);
    // GCC 2.9x's initialisation function (for every priority: the sets' chains, set 1 loaded) and the module's global constructor
    void InitVu0Programs(s32 initialise, s32 priority) RETAIL(FUN_002b2070);
    void ConstructVu0ProgramsModule() RETAIL(FUN_002b2230);
}

void SelectVu0Programs(u8* programs, u32 set, bool wait)
{
    auto* sets = reinterpret_cast<Vu0ProgramSets*>(programs);
    if (sets->loaded == set)
    {
        return;
    }

    StartDmaChain(Vif0Channel, sets->chains[set], true);
    sets->loaded = set;
    // Native: the maths' VU0 runs the microprograms of the set loaded
    NativeMath::Vu0ProgramsLoaded(set);
    if (!wait)
    {
        return;
    }

}

// The copier (VU0's microprogram 0 of the third set) started with vi01 set, then given eight quadwords at a time in vf01-vf08
// with the address in vi02 and how many blocks are left in vi03, clearing vi01 when it takes them (the interlocked move waits for
// it)
void SendToVu0(u8*, const void* source, s32 quadwords, s32 address)
{
    s32 blocks = quadwords >> 3;
    if ((quadwords & 7) != 0)
    {
        blocks++;
    }

    // Native: the quadwords put into the emulated VU0's memory where the copier would put them
    (void)blocks;
    Ee::Vu& vu = *NativeGraphics::GetHardware().vu0;
    for (s32 quadword = 0; quadword < quadwords; quadword++)
    {
        u32 at = (static_cast<u32>(address + quadword) * 16) & (vu.dataSize() - 1);
        std::memcpy(vu.data() + at, static_cast<const u8*>(source) + quadword * 16, 16);
    }
}

void InitVu0Programs(s32 initialise, s32 priority)
{
    if (priority != static_cast<s32>(DefaultInitPriority) || initialise == 0)
    {
        return;
    }

    auto* sets = reinterpret_cast<Vu0ProgramSets*>(g_Vu0Programs);
    sets->loaded = 0;
    constexpr u32 VuText = 0x2D9D90;
    sets->chains[Platform::Graphics::CullingPrograms] = g_VuText + (0x2E4EB0 - VuText);
    sets->chains[Platform::Graphics::StandardPrograms] = g_VuText + (0x2E6110 - VuText);
    sets->chains[Platform::Graphics::DecalPrograms] = g_VuText + (0x2E5BD0 - VuText);
    SelectVu0Programs(g_Vu0Programs, Platform::Graphics::StandardPrograms, false);
}

void ConstructVu0ProgramsModule()
{
    InitVu0Programs(1, DefaultInitPriority);
}

// Native: the program sets' record (a word and four pointers) is bigger than the 0x14 bytes the split gives G_UnkDmaRelated
alignas(16) u8 g_Vu0Programs[0x40];
