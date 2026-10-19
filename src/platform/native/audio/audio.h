#pragma once

// The native build's sound: the game's own sound modules on the emulated I/O processor (iop.h) with the SPU2 (spu2.h), its
// output played through SDL (audio.cpp). The game reaches them as on the PS2: Platform::Audio and Platform::Stream's sound side
// through MultiStream's EE library (the PS2 platform's own files, src/platform/ps2/audio.cpp and multistream/), whose batches go
// to STREAM.IRX by AudioCallIop
#include "common.h"

namespace Native
{
// Starts the I/O processor with the modules (once). False when they can't be loaded
bool AudioBootIop();
// An RPC call of a server on the I/O processor (the EE's sceSifCallRpc): the data goes to the server's function, and its reply
// comes back to reply, then ended runs (on the calling thread). Waiting, the call is done when it returns; otherwise when
// AudioPollIop sees it done
void AudioCallIop(s32 server, u32 function, bool wait, const void* send, u32 sendSize, void* reply, u32 replySize,
                  void (*ended)(void*));
void AudioPollIop();
// The EE's server of MultiStream's fast load (MsFastLoadRpc) answers the I/O processor's calls from now on
void AudioServeFastLoad();
// A batch MultiStream sends: what plays on each voice noted for the player's volumes (volumes.h), and with
// libsd's EE side (libsdr's sceSdRemote, src/platform/ps2/sdr.cpp): a plain command of SDRDRV's with its arguments, waited for.
// Returns its result
s32 AudioSdRemote(s32 command, u32 a1 = 0, u32 a2 = 0, u32 a3 = 0, u32 a4 = 0, u32 a5 = 0);
// The I/O processor's memory: its heap (Platform::Io's on the PS2) and the EE's SIF DMA into it
u32 AudioIopAllocate(u32 size);
void AudioIopFree(u32 address);
void AudioWriteIop(u32 address, const void* data, u32 size);
// The sound processor's clock: the samples it has made; and waiting until it has made so many (frame-locked, making them)
u64 AudioSamplesMade();
void AudioWaitForSample(u64 sample);

// $TWINSANITY_AUDIO_TRACE the batch logged, a command a line (the status requests left out)
void AudioTraceBatch(const u16* batch);
}
