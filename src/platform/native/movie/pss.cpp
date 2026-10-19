// A PSS file's demultiplexing (pss.h)
#include "pss.h"

#include "native.h"

#include <algorithm>
#include <cstring>

namespace
{
constexpr u32 PackStart = 0xBA;
constexpr u32 ProgramEnd = 0xB9;
constexpr u32 VideoStream = 0xE0;
constexpr u32 PrivateStream1 = 0xBD;
// The PES header's flags and PES_header_data_length after the packet's length
constexpr u32 PesFlagsBytes = 3;
// The sub-stream of Sony's PCM sound: FF A0 00, then the channel
constexpr u32 PcmSubStream = 0xFFA00000;
constexpr u32 SubStreamBytes = 4;
// The sound's header: "SShd", its size and 0x18 bytes, then "SSbd" and the body's size
constexpr u32 SoundHeaderBytes = 0x28;
constexpr size_t ReadChunk = 0x10000;

u32 Be32(const u8* bytes)
{
    return static_cast<u32>(bytes[0]) << 24 | static_cast<u32>(bytes[1]) << 16 | static_cast<u32>(bytes[2]) << 8 | bytes[3];
}

u32 Le32(const u8* bytes)
{
    return static_cast<u32>(bytes[0]) | static_cast<u32>(bytes[1]) << 8 | static_cast<u32>(bytes[2]) << 16 |
           static_cast<u32>(bytes[3]) << 24;
}
}

bool NativeMovie::Pss::Open(const char* path, u32 audioChannel)
{
    Native::Disc::Entry entry;
    if (!Native::Disc::Find(path, &entry))
    {
        Native::Log("movie: no %s on the disc", path);
        return false;
    }

    m_Offset = entry.offset;
    m_Size = entry.size;
    m_SubStream = PcmSubStream | (audioChannel & 0xFF);
    return true;
}

// Keeps at least bytes after the position in the buffer (fewer at the file's end)
bool NativeMovie::Pss::Fill(size_t bytes)
{
    if (m_Position > ReadChunk)
    {
        m_Buffer.erase(m_Buffer.begin(), m_Buffer.begin() + static_cast<std::ptrdiff_t>(m_Position));
        m_Position = 0;
    }

    while (m_Buffer.size() - m_Position < bytes && m_Read < m_Size)
    {
        u32 size = static_cast<u32>(std::min<size_t>(ReadChunk, m_Size - m_Read));
        size_t at = m_Buffer.size();
        m_Buffer.resize(at + size);
        Native::Disc::Read(m_Offset + m_Read, m_Buffer.data() + at, size);
        m_Read += size;
    }

    return m_Buffer.size() - m_Position >= bytes;
}

bool NativeMovie::Pss::Demultiplex()
{
    while (!m_Ended)
    {
        if (!Fill(4))
        {
            m_Ended = true;
            break;
        }

        const u8* at = m_Buffer.data() + m_Position;
        if (at[0] != 0 || at[1] != 0 || at[2] != 1)
        {
            // Not at a start code: on to the next byte
            m_Position++;
            continue;
        }

        u32 code = at[3];
        if (code == ProgramEnd)
        {
            m_Ended = true;
            break;
        }

        if (code == PackStart)
        {
            // MPEG-2's pack header: 14 bytes and its stuffing
            if (!Fill(14))
            {
                m_Ended = true;
                break;
            }

            m_Position += 14 + (m_Buffer[m_Position + 13] & 7);
            continue;
        }

        if (code < 0xBB)
        {
            m_Position += 4;
            continue;
        }

        if (!Fill(6))
        {
            m_Ended = true;
            break;
        }

        u32 length = static_cast<u32>(m_Buffer[m_Position + 4]) << 8 | m_Buffer[m_Position + 5];
        if (!Fill(6 + length))
        {
            m_Ended = true;
            break;
        }

        const u8* packet = m_Buffer.data() + m_Position;
        m_Position += 6 + length;
        if (code != VideoStream && code != PrivateStream1)
        {
            continue;
        }

        // MPEG-2's PES header: the flags, the header's length and its fields
        u32 headerLength = packet[8];
        u32 dataStart = 6 + PesFlagsBytes + headerLength;
        if (dataStart > 6 + length)
        {
            continue;
        }

        const u8* data = packet + dataStart;
        u32 dataLength = 6 + length - dataStart;
        if (code == VideoStream)
        {
            m_Video.emplace_back(data, data + dataLength);
            return true;
        }

        if (dataLength < SubStreamBytes || Be32(data) != m_SubStream)
        {
            continue;
        }

        data += SubStreamBytes;
        dataLength -= SubStreamBytes;
        while (m_SoundHeaderLeft != 0 && dataLength != 0)
        {
            m_HeaderBytes.push_back(*data++);
            dataLength--;
            if (--m_SoundHeaderLeft == 0 && m_HeaderBytes.size() == SoundHeaderBytes)
            {
                m_Header.type = Le32(&m_HeaderBytes[8]);
                m_Header.rate = Le32(&m_HeaderBytes[12]);
                m_Header.channels = Le32(&m_HeaderBytes[16]);
                m_Header.interleave = Le32(&m_HeaderBytes[20]);
            }
        }

        m_Sound.insert(m_Sound.end(), data, data + dataLength);
        return true;
    }

    return false;
}

bool NativeMovie::Pss::NextVideo(std::vector<u8>* data)
{
    while (m_Video.empty())
    {
        if (!Demultiplex())
        {
            return false;
        }
    }

    *data = std::move(m_Video.front());
    m_Video.erase(m_Video.begin());
    return true;
}

size_t NativeMovie::Pss::FillSound(size_t bytes)
{
    while (SoundWaiting() < bytes && Demultiplex())
    {
    }

    return SoundWaiting();
}

size_t NativeMovie::Pss::TakeSound(u8* to, size_t size)
{
    size = std::min(size, SoundWaiting());
    std::memcpy(to, m_Sound.data() + m_SoundRead, size);
    m_SoundRead += size;
    if (m_SoundRead > ReadChunk)
    {
        m_Sound.erase(m_Sound.begin(), m_Sound.begin() + static_cast<std::ptrdiff_t>(m_SoundRead));
        m_SoundRead = 0;
    }

    return size;
}

size_t NativeMovie::Pss::SoundWaiting() const
{
    return m_Sound.size() - m_SoundRead;
}

bool NativeMovie::Pss::Ended() const
{
    return m_Ended && m_Video.empty();
}

const NativeMovie::SoundHeader& NativeMovie::Pss::Header() const
{
    return m_Header;
}
