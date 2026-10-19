#pragma once

// The PS2's I/O processor, emulated to run the game's own sound modules: LIBSD.IRX (Sony's libsd, which drives the SPU2's
// registers and DMA), SDRDRV.IRX (libsd's RPC server) and STREAM.IRX (SCEE's MultiStream, the module the game's sound code
// sends its batches to), loaded from the disc and relocated into 2 MB of IOP memory. Their MIPS R3000 code is interpreted; the
// IOP kernel's libraries they import (threads, semaphores, event flags, interrupts, the hardware timer, memory, SIF RPC,
// cdvdman, sysclib, stdio) are the emulator's own, behaving as the IOP's do. The SPU2 (spu2.h) sits at its registers and on DMA
// channels 4 and 7; the disc is read from the image (Native::Disc).
//
// Time: the IOP's clock (36.864 MHz) advances by a cycle an instruction, and jumps ahead while every thread waits. The SPU2
// makes a sample every 768 cycles. Nothing here locks: the caller (audio.cpp) serialises every call
#include "common.h"

#include <vector>

namespace Iop
{
constexpr u32 ClockRate = 36864000;
constexpr u32 RamSize = 0x200000;

// Loads the modules from the disc and starts them (their start functions, then their threads until they wait). False when the
// disc doesn't have them
bool Boot();
bool Booted();

// Runs the IOP until the SPU2 has made so many samples, appending them (left, right, 16 bit) to out
void RunSamples(u32 samples, std::vector<s16>* out);

// A call of an RPC server on the IOP (the EE's sceSifCallRpc): the server's function is called with the data as the PS2's SIF
// sends it and its reply, size bytes, comes back. Calls are taken in order; Pending says whether one is still waiting
struct Call
{
    s32 server;
    u32 function;
    std::vector<u8> send;
    u32 replySize;
    std::vector<u8> reply;
    bool done = false;
};
// Queues the call: RunSamples (or RunUntilIdle) carries it out
void Post(Call* call);
// Runs the IOP until no thread is ready and no call is waiting, the clock moving on (and the SPU2's samples appended to out)
void RunUntilIdle(std::vector<s16>* out);
bool ServerExists(s32 server);

// The IOP's SIF DMA to the EE's memory: what it writes to an EE address (MultiStream's count of its runs, OpInitWait's)
using EeWriter = void (*)(u32 address, const u8* data, u32 size);
void SetEeWriter(EeWriter writer);

// An RPC server of the EE's the modules call (sceSifCallRpc): its function gets the data and returns its reply (rsize bytes of
// it go back). Called on whatever thread runs the I/O processor
using EeServer = const void* (*)(u32 function, void* data, u32 size);
void SetEeServer(s32 id, EeServer server);

// A line per thread (its state, what it waits for, where it is) and the clock, for the log
void LogState();

// The IOP's memory, and its heap (sysmem's AllocSysMemory, lowest first)
u8* Ram();
u32 Allocate(u32 size);
void Free(u32 address);
// The samples the SPU2 has made
u64 SamplesMade();
u64 Cycles();
}
