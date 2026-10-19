#pragma once

// The emulated graphics hardware the native renderer's packets run on: the GS, the GIF, VU0 and VU1 with their VIFs, and the
// DMA channels the renderer starts (VIF0, VIF1, the GIF). A chain sent on a channel runs to its end at once: the PS2 runs it while
// the game goes on, and the game waits for it before it touches what it reads (the renderer's buffers are double buffered for
// that), so running it right away gives what the PS2 draws.

#include "common.h"

namespace Gs
{
class Gs;
}

namespace Ee
{
class Gif;
class Vu;
class Vif;
}

namespace NativeGraphics
{
struct Hardware
{
    Gs::Gs* gs;
    Ee::Gif* gif;
    Ee::Vu* vu0;
    Ee::Vu* vu1;
    Ee::Vif* vif0;
    Ee::Vif* vif1;
};

// The hardware, made the first time it's asked for
Hardware& GetHardware();

// A DMA chain (its first tag) sent on VIF0's, VIF1's (both with their tags' VIF codes) or the GIF's channel
void SendVif0Chain(const void* chain);
void SendVif1Chain(const void* chain);
void SendGifChain(const void* chain);
}
