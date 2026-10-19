#pragma once

// A PSS file (Sony's MPEG-2 program stream) read from the disc image: its packs' PES packets split into the video's elementary
// stream (stream 0xE0) and the sound's bytes (private stream 1, the sub-stream FF A0 00 channel: Sony's PCM, its 0x28 byte
// "SShd"/"SSbd" header taken off), as the PS2's libmpeg demultiplexer (src/platform/ps2/movie/mpegdemux.cpp) hands them out
#include "common.h"

#include <vector>

namespace NativeMovie
{
// The sound's header (the PS2's MovieAudioHeader)
struct SoundHeader
{
    u32 type = 0;
    u32 rate = 0;
    u32 channels = 0;
    u32 interleave = 0;
};

class Pss
{
public:
    // False when the disc has no such file
    bool Open(const char* path, u32 audioChannel);
    // The next PES packet of the video: false at the file's end
    bool NextVideo(std::vector<u8>* data);
    // Demultiplexes on until the sound has at least bytes waiting (or the file ends). Returns the bytes waiting
    size_t FillSound(size_t bytes);
    // Takes up to size bytes of the sound
    size_t TakeSound(u8* to, size_t size);
    size_t SoundWaiting() const;
    bool Ended() const;
    const SoundHeader& Header() const;

private:
    // The next packet of either stream into its queue: false at the end
    bool Demultiplex();
    bool Fill(size_t bytes);

    u64 m_Offset = 0;
    u32 m_Size = 0;
    u32 m_Read = 0;
    u32 m_SubStream = 0;
    std::vector<u8> m_Buffer;
    size_t m_Position = 0;
    bool m_Ended = false;
    std::vector<std::vector<u8>> m_Video;
    std::vector<u8> m_Sound;
    size_t m_SoundRead = 0;
    // The sound's header bytes still to take off its stream
    u32 m_SoundHeaderLeft = 0x28;
    std::vector<u8> m_HeaderBytes;
    SoundHeader m_Header;
};
}
