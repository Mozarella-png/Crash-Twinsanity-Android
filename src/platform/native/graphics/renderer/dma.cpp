#include "renderer.h"

#include "../hardware.h"

// The renderer's DMA channels (src/platform/ps2/renderer/dma.cpp's functions): native, a chain sent on a channel runs on the
// hardware's emulation at once, so a channel is never busy and its waits return at once

namespace
{
// The chain of a channel by its number: VIF0's and VIF1's with their tags' VIF codes, the GIF's
void Run(s32 channel, const void* chain)
{
    switch (channel)
    {
    case Vif0Channel:
        NativeGraphics::SendVif0Chain(chain);
        break;
    case Vif1Channel:
        NativeGraphics::SendVif1Chain(chain);
        break;
    case GifChannel:
        NativeGraphics::SendGifChain(chain);
        break;
    default:
        break;
    }
}
}

bool IsDmaChannelBusy(s32)
{
    return false;
}

void StartDmaChain(s32 channel, const void* chain, bool)
{
    Run(channel, chain);
}

s32 WaitForDmaChannel(s32 channel)
{
    g_RendererDma[channel].sending = 0;
    return 1;
}

s32 WaitForVif1Dma()
{
    return WaitForDmaChannel(Vif1Channel);
}

void SetDmaRegisterPointers()
{
    for (u32 channel = 0; channel < DmaChannels; channel++)
    {
        RendererDmaChannel& dma = g_RendererDma[channel];
        dma.registers = nullptr;
        dma.statusBit = 1u << channel;
        dma.sending = 0;
    }
}

s32 FinishDMATransferAll()
{
    for (s32 channel = 0; channel < DmaChannels; channel++)
    {
        WaitForDmaChannel(channel);
    }

    return 1;
}

// The renderer's state the PS2 keeps in .bss whose types hold pointers: defined natively (the data converter leaves them out)
RendererDmaChannel g_RendererDma[DmaChannels];
DmaChain g_DmaChains[10];
VuProgram g_VuPrograms[VuProgramCount];
