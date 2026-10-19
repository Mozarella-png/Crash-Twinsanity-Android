#include "capture.h"

#include "hardware.h"
#include "renderthread.h"
#include "state.h"
#include "ee/gif.h"
#include "ee/vif.h"
#include "ee/vu.h"
#include "gs/backend.h"
#include "gs/gs.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace NativeGraphics
{
namespace
{
enum Record : u32
{
    RecordState = 1,
    RecordVif1 = 2,
    RecordGif = 3,
    RecordShown = 4,
};

constexpr char Magic[8] = {'T', 'W', 'G', 'S', 'C', 'A', 'P', '1'};

struct Capture
{
    bool parsed = false;
    std::string path;
    u32 first = 0;
    u32 count = 0;
    u32 shown = 0;
    u32 recorded = 0;
    FILE* file = nullptr;
};

Capture g_Capture;

void WriteRecord(u32 kind, const void* data, size_t size)
{
    u32 header[2] = {kind, static_cast<u32>(size)};
    std::fwrite(header, sizeof(header), 1, g_Capture.file);
    if (size > 0)
    {
        std::fwrite(data, size, 1, g_Capture.file);
    }
}

// The PCRTC's registers, as a shown record carries them
struct Pcrtc
{
    u64 pmode;
    u64 smode2;
    u64 dispfb[2];
    u64 display[2];
    u64 bgcolor;
};

Pcrtc ReadPcrtc(const Gs::Gs& gs)
{
    Pcrtc r;
    std::memcpy(&r.pmode, &gs.pmode, 8);
    std::memcpy(&r.smode2, &gs.smode2, 8);
    std::memcpy(r.dispfb, gs.dispfb, 16);
    std::memcpy(r.display, gs.display, 16);
    r.bgcolor = gs.bgcolor;
    return r;
}

void WritePcrtc(Gs::Gs& gs, const Pcrtc& r)
{
    std::memcpy(&gs.pmode, &r.pmode, 8);
    std::memcpy(&gs.smode2, &r.smode2, 8);
    std::memcpy(gs.dispfb, r.dispfb, 16);
    std::memcpy(gs.display, r.display, 16);
    gs.bgcolor = r.bgcolor;
}

template <typename Io>
void HardwareState(Io& io)
{
    Hardware& h = GetHardware();
    h.gs->Serialize(io);
    h.vu1->Serialize(io);
    h.vif1->Serialize(io);
    h.gif->Serialize(io);
}
}

void CaptureVif1(const u32* words, u32 count)
{
    if (g_Capture.file != nullptr)
    {
        WriteRecord(RecordVif1, words, count * 4);
    }
}

void CaptureGif(const u32* words, u32 count)
{
    if (g_Capture.file != nullptr)
    {
        WriteRecord(RecordGif, words, count * 4);
    }
}

void CaptureShown()
{
    Capture& c = g_Capture;
    if (!c.parsed)
    {
        c.parsed = true;
        const char* setting = std::getenv("TWIN_CAPTURE");
        if (setting != nullptr)
        {
            std::string text = setting;
            size_t second = text.rfind(':');
            size_t first = second == std::string::npos ? std::string::npos : text.rfind(':', second - 1);
            if (first == std::string::npos)
            {
                std::fprintf(stderr, "graphics: TWIN_CAPTURE is FILE:FIRST:COUNT\n");
            }
            else
            {
                c.path = text.substr(0, first);
                c.first = static_cast<u32>(std::atoi(text.c_str() + first + 1));
                c.count = static_cast<u32>(std::atoi(text.c_str() + second + 1));
            }
        }
    }

    if (c.path.empty())
    {
        return;
    }

    // The GS's side read from here: everything queued for the render thread done first (the PCRTC's registers too)
    if (c.file != nullptr || c.shown == c.first)
    {
        RenderSync();
    }

    if (c.file != nullptr)
    {
        Pcrtc pcrtc = ReadPcrtc(*GetHardware().gs);
        WriteRecord(RecordShown, &pcrtc, sizeof(pcrtc));
        if (++c.recorded == c.count)
        {
            std::fclose(c.file);
            c.file = nullptr;
            c.path.clear();
            std::fprintf(stderr, "graphics: recorded %u frames\n", c.count);
        }
    }
    else if (c.shown == c.first)
    {
        c.file = std::fopen(c.path.c_str(), "wb");
        if (c.file == nullptr)
        {
            std::fprintf(stderr, "graphics: can't write %s\n", c.path.c_str());
            c.path.clear();
        }
        else
        {
            std::fwrite(Magic, sizeof(Magic), 1, c.file);
            std::vector<u8> state;
            StateWriter writer(state);
            Saver saver{writer};
            HardwareState(saver);
            WriteRecord(RecordState, state.data(), state.size());
            std::fprintf(stderr, "graphics: recording %u frames into %s\n", c.count, c.path.c_str());
        }
    }

    c.shown++;
}

bool Replay(const char* path, const std::function<void(u32 frame, const Gs::DisplayImage& image, double ms)>& shown,
            bool readPictures)
{
    FILE* file = std::fopen(path, "rb");
    if (file == nullptr)
    {
        return false;
    }

    char magic[8];
    if (std::fread(magic, sizeof(magic), 1, file) != 1 || std::memcmp(magic, Magic, sizeof(magic)) != 0)
    {
        std::fclose(file);
        return false;
    }

    Hardware& h = GetHardware();
    std::vector<u8> data;
    u32 frame = 0;
    auto started = std::chrono::steady_clock::now();
    for (;;)
    {
        u32 header[2];
        if (std::fread(header, sizeof(header), 1, file) != 1)
        {
            break;
        }

        data.resize(header[1]);
        if (header[1] > 0 && std::fread(data.data(), header[1], 1, file) != 1)
        {
            break;
        }

        switch (header[0])
        {
        case RecordState:
        {
            // (the GS's side idle while its state is read in from here)
            RenderSync();
            StateReader reader(data.data(), data.size());
            Loader loader{reader};
            HardwareState(loader);
            if (reader.failed())
            {
                std::fclose(file);
                return false;
            }

            break;
        }
        case RecordVif1:
            if (RenderThreadRunsVu1())
            {
                RenderQueueVif1(reinterpret_cast<const u32*>(data.data()), header[1] / 4);
            }
            else
            {
                h.vif1->Process(reinterpret_cast<const u32*>(data.data()), header[1] / 4);
            }

            RenderFlush();
            break;
        case RecordGif:
            h.gif->Transfer(2, data.data(), header[1] / 16);
            RenderFlush();
            break;
        case RecordShown:
        {
            Pcrtc pcrtc;
            std::memcpy(&pcrtc, data.data(), sizeof(pcrtc));
            // On the GS's side (the render thread's, waited for, when there is one): the picture read and handed over
            RenderCallAndWait([&] {
                WritePcrtc(*h.gs, pcrtc);
                Gs::DisplayImage image;
                if (readPictures)
                {
                    h.gs->ReadDisplay(image);
                }
                else if (h.gs->backend() != nullptr)
                {
                    // As the game shows a picture with the hardware renderer: everything drawn, nothing read back
                    h.gs->backend()->Flush();
                }

                auto now = std::chrono::steady_clock::now();
                shown(frame++, image, std::chrono::duration<double, std::milli>(now - started).count());
                started = std::chrono::steady_clock::now();
            });
            break;
        }
        default:
            break;
        }
    }

    std::fclose(file);
    return true;
}
}
