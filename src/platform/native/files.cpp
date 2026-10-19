// The disc's files (the PS2's fileio over the I/O processor): read from the disc image. The game only reads the disc this way;
// opening for writing fails as the PS2's disc would
#include "native.h"
#include "platform/files.h"

#include <mutex>
#include <vector>

namespace
{
struct OpenFile
{
    bool open;
    Native::Disc::Entry entry;
    u32 position;
};

std::vector<OpenFile> g_Files;
std::mutex g_Lock;
}

void Platform::Files::Reset()
{
}

s32 Platform::Files::Open(const char* path, s32 flags)
{
    Native::Disc::Entry entry;
    if ((flags & OpenWrite) != 0 || !Native::Disc::Find(path, &entry))
    {
        return -1;
    }

    std::lock_guard lock(g_Lock);
    for (size_t i = 0; i < g_Files.size(); i++)
    {
        if (!g_Files[i].open)
        {
            g_Files[i] = {true, entry, 0};
            return static_cast<s32>(i);
        }
    }

    g_Files.push_back({true, entry, 0});
    return static_cast<s32>(g_Files.size() - 1);
}

s32 Platform::Files::Close(s32 file)
{
    std::lock_guard lock(g_Lock);
    if (file < 0 || static_cast<size_t>(file) >= g_Files.size() || !g_Files[file].open)
    {
        return -1;
    }

    g_Files[file].open = false;
    return 0;
}

s32 Platform::Files::Read(s32 file, void* buffer, s32 size)
{
    std::lock_guard lock(g_Lock);
    if (file < 0 || static_cast<size_t>(file) >= g_Files.size() || !g_Files[file].open || size < 0)
    {
        return -1;
    }

    OpenFile& open = g_Files[file];
    u32 left = open.entry.size - open.position;
    u32 count = static_cast<u32>(size) < left ? static_cast<u32>(size) : left;
    u32 read = Native::Disc::Read(open.entry.offset + open.position, buffer, count);
    open.position += read;
    return static_cast<s32>(read);
}

s32 Platform::Files::Write(s32, const void*, s32)
{
    return -1;
}

s32 Platform::Files::Seek(s32 file, s32 offset, Whence whence)
{
    std::lock_guard lock(g_Lock);
    if (file < 0 || static_cast<size_t>(file) >= g_Files.size() || !g_Files[file].open)
    {
        return -1;
    }

    OpenFile& open = g_Files[file];
    s64 base = whence == SeekSet ? 0 : whence == SeekCurrent ? open.position : open.entry.size;
    s64 position = base + offset;
    if (position < 0)
    {
        return -1;
    }

    open.position = static_cast<u32>(position);
    return static_cast<s32>(open.position);
}
