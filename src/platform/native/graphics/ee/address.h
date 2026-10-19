#pragma once

// The renderer's 32 bit DMA addresses on a 64 bit host. The PS2 renderer writes main memory addresses into its DMA tags (a
// packet's next tag, the data a REF sends, the code an MPG loads); the native renderer keeps those packets as they are, with
// each host address made a 32 bit one: its 64 MB window of the host's address space takes one of 63 slots (slot 0 is the null
// address), the address being the slot and the offset in the window. A tag's address is turned back into the host's when the
// chain is run. Data the hardware reads contiguously is contiguous in the host's memory too, so a read may go past its window.

#include "common.h"

namespace Ee
{
u32 DmaAddress(const void* pointer);
u8* HostAddress(u32 address);
}
