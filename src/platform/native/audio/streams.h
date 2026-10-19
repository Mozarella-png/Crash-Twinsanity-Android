#pragma once

// Platform::Stream's sound side natively (stream.cpp calls these): what the PS2's Platform::Stream does through MultiStream for
// the channels that stream into the sound processor (sound banks' samples, music), done the PS2's way on the emulated I/O
// processor. Reads into the EE's memory stay stream.cpp's own
#include "common.h"

namespace Native::AudioStreams
{
void Initialise(s32 channels);
void Update();
void FileOpened(s32 file, const char* path);
void FileClosed(s32 file);
void AttachBuffer(s32 channel, u32 size, u32 soundAddress, u32 soundSize);
void DetachBuffer(s32 channel);
u32 FreeBufferMemory();
void ReadSoundBank(s32 channel, u32 bank, s32 file, u32 offset, u32 size);
bool IsReading(s32 channel);
// Waits until the channel's sound bank read is done
void Wait(s32 channel);
}
