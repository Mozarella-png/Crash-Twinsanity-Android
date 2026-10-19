#pragma once

// A 16 bit stereo WAV file written as samples come (the sizes in its header written when it's closed)
#include "common.h"

#include <cstdio>

namespace Native
{
class WavWriter
{
public:
    WavWriter(const char* path, u32 rate) : m_File(std::fopen(path, "wb")), m_Rate(rate)
    {
        if (m_File != nullptr)
        {
            WriteHeader();
        }
    }

    ~WavWriter()
    {
        if (m_File != nullptr)
        {
            std::fseek(m_File, 0, SEEK_SET);
            WriteHeader();
            std::fclose(m_File);
        }
    }

    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;

    void Write(const s16* samples, size_t count)
    {
        if (m_File != nullptr)
        {
            std::fwrite(samples, sizeof(s16), count, m_File);
            m_Bytes += static_cast<u32>(count * sizeof(s16));
        }
    }

private:
    void Put32(u32 value)
    {
        std::fwrite(&value, 4, 1, m_File);
    }

    void Put16(u16 value)
    {
        std::fwrite(&value, 2, 1, m_File);
    }

    void WriteHeader()
    {
        std::fwrite("RIFF", 1, 4, m_File);
        Put32(36 + m_Bytes);
        std::fwrite("WAVEfmt ", 1, 8, m_File);
        Put32(16);
        Put16(1);
        Put16(2);
        Put32(m_Rate);
        Put32(m_Rate * 4);
        Put16(4);
        Put16(16);
        std::fwrite("data", 1, 4, m_File);
        Put32(m_Bytes);
    }

    FILE* m_File;
    u32 m_Rate;
    u32 m_Bytes = 0;
};
}
