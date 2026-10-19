// Debugging aid (--snapshots DIR): the picture the game shows saved as a PNG every couple of seconds, so a headless run can be
// looked at. Called once a frame (Platform::Pads::Read, which the game calls every frame)
#include "graphics/display.h"
#include "native.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
std::string g_Folder;
std::chrono::steady_clock::time_point g_Last;
u32 g_Count = 0;

u32 Crc(const u8* data, size_t size, u32 crc = 0xFFFFFFFF)
{
    for (size_t i = 0; i < size; i++)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
        {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
        }
    }

    return crc;
}

void Be32(std::vector<u8>& out, u32 value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        out.push_back(static_cast<u8>(value >> shift));
    }
}

void Chunk(std::vector<u8>& out, const char* type, const std::vector<u8>& data)
{
    Be32(out, static_cast<u32>(data.size()));
    std::vector<u8> typed(type, type + 4);
    typed.insert(typed.end(), data.begin(), data.end());
    out.insert(out.end(), typed.begin(), typed.end());
    Be32(out, Crc(typed.data(), typed.size()) ^ 0xFFFFFFFF);
}

// An RGBA PNG with stored (uncompressed) deflate blocks
bool WritePng(const std::string& path, const u32* pixels, u32 width, u32 height)
{
    std::vector<u8> raw;
    raw.reserve((width * 4 + 1) * height);
    for (u32 y = 0; y < height; y++)
    {
        raw.push_back(0);
        const u8* row = reinterpret_cast<const u8*>(pixels + y * width);
        raw.insert(raw.end(), row, row + width * 4);
    }

    std::vector<u8> zlib = {0x78, 0x01};
    u32 a = 1, b = 0;
    for (u8 byte : raw)
    {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }

    for (size_t at = 0; at < raw.size() || at == 0;)
    {
        size_t length = std::min<size_t>(65535, raw.size() - at);
        bool last = at + length >= raw.size();
        zlib.push_back(last ? 1 : 0);
        zlib.push_back(static_cast<u8>(length));
        zlib.push_back(static_cast<u8>(length >> 8));
        zlib.push_back(static_cast<u8>(~length));
        zlib.push_back(static_cast<u8>(~length >> 8));
        zlib.insert(zlib.end(), raw.begin() + static_cast<long>(at), raw.begin() + static_cast<long>(at + length));
        at += length;
        if (last)
        {
            break;
        }
    }

    Be32(zlib, (b << 16) | a);
    std::vector<u8> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<u8> header;
    Be32(header, width);
    Be32(header, height);
    header.insert(header.end(), {8, 6, 0, 0, 0});
    Chunk(png, "IHDR", header);
    Chunk(png, "IDAT", zlib);
    Chunk(png, "IEND", {});
    FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr)
    {
        return false;
    }

    std::fwrite(png.data(), 1, png.size(), file);
    std::fclose(file);
    return true;
}
}

namespace Native
{
void SetSnapshotFolder(const char* folder)
{
    g_Folder = folder;
    std::error_code ignored;
    std::filesystem::create_directories(g_Folder, ignored);
}

void SnapshotFrame()
{
    if (g_Folder.empty())
    {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (g_Count != 0 && now - g_Last < std::chrono::seconds(2))
    {
        return;
    }

    const NativeGraphics::Picture& picture = NativeGraphics::LastPicture();
    if (picture.width == 0 || picture.pixels.size() < static_cast<size_t>(picture.width) * picture.height)
    {
        return;
    }

    g_Last = now;
    char name[64];
    std::snprintf(name, sizeof(name), "/frame_%04u.png", g_Count++);
    // Kept to 1024 wide at most (every other pixel of a bigger picture: a 4x picture's snapshot is 16 MB otherwise, its PNG
    // uncompressed, and a long test run fills the disk)
    u32 step = 1;
    while (picture.width / step > 1024)
    {
        step *= 2;
    }

    if (step == 1)
    {
        WritePng(g_Folder + name, picture.pixels.data(), picture.width, picture.height);
        return;
    }

    u32 width = picture.width / step;
    u32 height = picture.height / step;
    std::vector<u32> small(static_cast<size_t>(width) * height);
    for (u32 y = 0; y < height; y++)
    {
        for (u32 x = 0; x < width; x++)
        {
            small[static_cast<size_t>(y) * width + x] = picture.pixels[static_cast<size_t>(y * step) * picture.width + x * step];
        }
    }

    WritePng(g_Folder + name, small.data(), width, height);
}
}
