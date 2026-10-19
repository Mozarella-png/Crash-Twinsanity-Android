// The native graphics' harness: the game's own start-up as far as its renderer and files (Main's first steps and StartGame's
// readers and archive), then the game's own resources (the font, a screen picture of the legal screen's) read by the game's own
// readers and drawn through the game's overlay with the frames the game's renderer controller makes, the PCRTC's pictures written
// as PNGs. Nothing here draws by itself: it only asks the game's code to, the way the game's widgets do.
//
//   graphics-harness OUTPUT_FOLDER [--frames N] [--text "..."] [--picture Language\Legal\English]
//   graphics-harness OUTPUT_FOLDER --replay RECORDING [--scale N] [--bench 1]   (a TWIN_CAPTURE recording played back)
//   --render-thread 1: the GS's side on the render thread, as the game has it (graphics/renderthread.h); off by default

#include "graphics/png.h"

#include "native.h"

#include "game/colour.h"
#include "game/context.h"
#include "game/controllers.h"
#include "game/filestream.h"
#include "game/gamecontroller.h"
#include "game/font.h"
#include "game/memory.h"
#include "game/movie.h"
#include "game/overlay.h"
#include "game/readers.h"
#include "game/renderer.h"
#include "game/shapes.h"
#include "game/stream.h"
#include "game/widgets.h"
#include "gcc2.h"
#include "platform/graphics.h"
#include "platform/system.h"

#include "graphics/display.h"
#include "graphics/resolution.h"
#include "graphics/ee/vif.h"
#include "graphics/ee/vu.h"
#include "graphics/gs/gs.h"
#include "graphics/hardware.h"
#include "graphics/renderthread.h"
#include "graphics/capture.h"
#include "graphics/gs/backend.h"
#include "graphics/hw/hwgs.h"
#include "graphics/settings.h"
#include "graphics/glyphs.h"
#include <cmath>
#include <vector>
#include "graphics/presenter.h"
#include "graphics/window.h"
#include <SDL3/SDL.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C"
{
    extern u32 g_FrameBufferPage RETAIL(D_0030AAFC);
    void NativeApplyDataFixups();
    extern u32 g_OlegPictureColour RETAIL(D_0030A708);
    extern RenderTargetDescription* g_RenderTarget RETAIL(G_FontRendererRel);
}

namespace Native
{
void StartLowMemory();
}

namespace
{
constexpr s32 StreamChannels = 9;
constexpr u32 TileCount = 8;

// A buffer of GS memory as a picture (its colours, alpha opaque)
void SaveBuffer(const std::string& folder, const char* name, u32 psm, u32 bp, u32 bw, u32 width, u32 height)
{
    Gs::Gs& gs = *NativeGraphics::GetHardware().gs;
    std::vector<uint32_t> pixels(width * height);
    for (u32 y = 0; y < height; y++)
    {
        for (u32 x = 0; x < width; x++)
        {
            u32 c = gs.ReadPixel(psm, x, y, bp, bw);
            if (psm == Gs::PSMCT16 || psm == Gs::PSMCT16S)
            {
                c = (c & 0x1F) << 3 | ((c >> 5) & 0x1F) << 11 | ((c >> 10) & 0x1F) << 19;
            }

            pixels[y * width + x] = c | 0xFF000000u;
        }
    }

    WritePng(folder + "/" + name + ".png", pixels.data(), width, height);
}

void SavePicture(const std::string& folder, const char* name)
{
    const NativeGraphics::Picture& picture = NativeGraphics::LastPicture();
    std::string path = folder + "/" + name + ".png";
    if (picture.width == 0 || !WritePng(path, picture.pixels.data(), picture.width, picture.height))
    {
        std::fprintf(stderr, "harness: no picture for %s\n", path.c_str());
        return;
    }

    std::printf("harness: %s (%ux%u)\n", path.c_str(), picture.width, picture.height);
}

// The game's frame as GameContextPrototype's steps make it, with what the harness queued drawn by the renderer's overlay
void RenderFrame(Renderer* renderer)
{
    GameRendererController* controller = G_GameRendererController;
    controller->EndFrame();
    g_RenderTarget = renderer->target;
    controller->DrawRendererScenes();
    controller->AfterEndFrame();
    controller->AfterGameRender();
    controller->Render();
    controller->BeginFrame();
}
}

int main(int argc, char** argv)
{
    Native::ParseArguments(argc, argv);
    Native::StartLowMemory();
    if (argc < 2)
    {
        std::fprintf(stderr, "graphics-harness OUTPUT_FOLDER [--frames N] [--text TEXT] [--picture PATH]\n");
        return 1;
    }

    std::string folder = argv[1];
    if (std::getenv("TWIN_PAGE_TEST") != nullptr)
    {
        Gs::PageSet pages;
        Gs::PagesOfRect(Gs::PSMZ16, 0x1800, 8, 0, 0, 15, 519, pages);
        for (u32 p = 0; p < Gs::PageCount; p++)
        {
            if (pages.test(p))
            {
                std::printf("%x ", p);
            }
        }

        std::printf("\n%x %x %x\n", Gs::PixelPage(Gs::PSMZ16, 0, 0, 0x1800, 8), Gs::PixelPage(Gs::PSMZ16, 15, 519, 0x1800, 8),
                    Gs::PixelPage(Gs::PSMZ16, 0, 63, 0x1800, 8));
        return 0;
    }

    u32 frames = 3;
    bool saving = true;
    int scale = 1;
    std::string text = "THREE YEARS AGO...";
    std::string picturePath = "Language\\Legal\\English.psm";
    std::string replay;
    int postFilter = -1;
    int glyphCode = -1;
    bool hardware = false;
    bool texturePack = false;
    bool renderThread = false;
    int loops = 1;
    u32 benchFrames = 0;
    for (int i = 2; i + 1 < argc; i += 2)
    {
        if (std::strcmp(argv[i], "--frames") == 0)
        {
            frames = static_cast<u32>(std::atoi(argv[i + 1]));
        }
        else if (std::strcmp(argv[i], "--scale") == 0)
        {
            scale = std::atoi(argv[i + 1]);
        }
        else if (std::strcmp(argv[i], "--bench") == 0)
        {
            frames = static_cast<u32>(std::atoi(argv[i + 1]));
            benchFrames = frames;
            saving = false;
        }
        else if (std::strcmp(argv[i], "--text") == 0)
        {
            text = argv[i + 1];
        }
        else if (std::strcmp(argv[i], "--picture") == 0)
        {
            picturePath = argv[i + 1];
        }
        else if (std::strcmp(argv[i], "--glyph") == 0)
        {
            // A test image (a red ring on a half transparent square) for the glyph of this character
            glyphCode = static_cast<int>(static_cast<u8>(argv[i + 1][0]));
        }
        else if (std::strcmp(argv[i], "--post-filter") == 0)
        {
            // Pictures shown in a hidden window through the presenter's post filter (1: the UI's CRT filter)
            postFilter = std::atoi(argv[i + 1]);
        }
        else if (std::strcmp(argv[i], "--hardware") == 0)
        {
            // The hardware renderer (OpenGL) draws, at --scale; "0" leaves the software GS
            hardware = std::atoi(argv[i + 1]) != 0;
        }
        else if (std::strcmp(argv[i], "--loops") == 0)
        {
            // The recording played this many times (for profiling)
            loops = std::max(1, std::atoi(argv[i + 1]));
        }
        else if (std::strcmp(argv[i], "--texture-pack") == 0)
        {
            // Replacement textures on (PCSX2's packs, from $TWIN_TEXTURES_DIR or the settings folder)
            texturePack = std::atoi(argv[i + 1]) != 0;
        }
        else if (std::strcmp(argv[i], "--replay") == 0)
        {
            replay = argv[i + 1];
        }
        else if (std::strcmp(argv[i], "--render-thread") == 0)
        {
            renderThread = std::atoi(argv[i + 1]) != 0;
        }
    }

    NativeGraphics::SetRenderThreadWanted(renderThread);

    // A recording (TWIN_CAPTURE) played back: its pictures as replayNNN.png (--bench: only the times)
    if (!replay.empty())
    {
        NativeGraphics::SetPacing(false);
        NativeGraphics::StartRenderThread();
        NativeGraphicsSetRenderer(hardware ? 1 : 0);
        NativeGraphicsSetResolution(scale, 0, 0, false);
        NativeGraphicsSetTexturePack(texturePack);
        NativeGraphics::RenderSync();
        if (hardware && !NativeGraphics::HardwareRendererActive())
        {
            std::fprintf(stderr, "graphics-harness: no hardware renderer\n");
            return 1;
        }

        double total = 0.0;
        u32 count = 0;
        bool read = true;
        for (int loop = 0; loop + 1 < loops && read; loop++)
        {
            read = NativeGraphics::Replay(replay.c_str(), [&](u32, const Gs::DisplayImage&, double ms) {
                total += ms;
                count++;
            }, benchFrames == 0);
        }

        read = read && NativeGraphics::Replay(replay.c_str(), [&](u32 frame, const Gs::DisplayImage& image, double ms) {
            total += ms;
            count++;
            std::printf("frame %u: %.1f ms\n", frame, ms);
            if (benchFrames == 0 && image.width != 0)
            {
                char name[32];
                std::snprintf(name, sizeof(name), "/replay%03u.png", frame);
                WritePng(folder + name, image.pixels.data(), image.width, image.height);
                // The hardware renderer's own picture, at its scale
                Gs::DisplayImage gpu;
                if (hardware && NativeGraphics::HardwareReadDisplay(gpu))
                {
                    std::snprintf(name, sizeof(name), "/replay%03u_gpu.png", frame);
                    WritePng(folder + name, gpu.pixels.data(), gpu.width, gpu.height);
                }
            }
        });
        if (!read)
        {
            std::fprintf(stderr, "graphics-harness: can't play %s\n", replay.c_str());
            return 1;
        }

        std::printf("replay: %u frames, %.2f ms a frame\n", count, count != 0 ? total / count : 0.0);
        if (hardware)
        {
            NativeGraphics::HardwareStats stats = NativeGraphics::GetHardwareStats();
            std::printf("hardware: %llu drawn, %llu left to software, %llu batches, %llu to memory, %llu from memory, %llu CPU "
                        "readbacks, %llu copies, %llu GPU decodes, %llu content hits\n",
                        (unsigned long long)stats.drawn, (unsigned long long)stats.fallbacks, (unsigned long long)stats.batches,
                        (unsigned long long)stats.downloads, (unsigned long long)stats.uploads, (unsigned long long)stats.readbacks,
                        (unsigned long long)stats.copies, (unsigned long long)stats.decodes, (unsigned long long)stats.contentHits);
        }
        if (const char* dumpVu1 = std::getenv("TWIN_DUMP_VU1"))
        {
            // VU1's micro memory and data memory as the replay left them
            Ee::Vu& vu1 = *NativeGraphics::GetHardware().vu1;
            if (FILE* out = std::fopen(dumpVu1, "wb"))
            {
                std::fwrite(vu1.code(), 8, vu1.codeSize(), out);
                std::fwrite(vu1.data(), 1, vu1.dataSize(), out);
                std::fclose(out);
            }
        }
        return 0;
    }

    SDL_Window* hidden = nullptr;
    if (postFilter >= 0)
    {
        SDL_Init(SDL_INIT_VIDEO);
        hidden = SDL_CreateWindow("graphics-harness", 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        if (hidden == nullptr)
        {
            std::fprintf(stderr, "graphics-harness: no window: %s\n", SDL_GetError());
            return 1;
        }

        NativeGraphicsAttachWindow(hidden);
        NativeGraphicsSetPostFilter(postFilter);
    }

    if (glyphCode >= 0)
    {
        std::vector<u32> image(32 * 40);
        for (int y = 0; y < 40; y++)
        {
            for (int x = 0; x < 32; x++)
            {
                float dx = x - 15.5f;
                float dy = y - 19.5f;
                float r = std::sqrt(dx * dx + dy * dy);
                bool ring = r > 9.0f && r < 14.0f;
                // A bar across the top rows, to show which way up it is
                bool top = y < 4;
                image[y * 32 + x] = ring ? 0xFF0000FFu : top ? 0xFF00FF00u : 0x80FFFFFFu;
            }
        }

        bool supported = NativeGraphicsSetGlyphImage(static_cast<u8>(glyphCode), image.data(), 32, 40);
        std::printf("harness: glyph image for %c %s\n", glyphCode, supported ? "set" : "not supported");
    }

    Native::OpenDiscImage();
    NativeApplyDataFixups();
    NativeGraphics::SetPacing(false);
    NativeGraphicsSetRenderer(hardware ? 1 : 0);
    NativeGraphicsSetResolution(scale, 0, 0, false);
    // No movie controller: the harness draws the scene
    G_GameMovieController = nullptr;

    // Main's start, then the renderer, the readers and the archive as StartUp and StartGame make them
    RunStaticConstructors();
    Platform::System::Initialise();
    char program[] = "graphics-harness";
    char* arguments[] = {program};
    GameContext* context = ParseArguments(1, arguments);
    Platform::System::StartServices();
    InitStreamSystem(StreamChannels);
    UpdateStreamSystem(g_StreamSystem);
    void* memory = MemoryAllocate(sizeof(GameRendererController));
    G_GameRendererController = GameRendererController::Construct(memory, g_ScreenWidth, g_ScreenHeight, g_Pal);
    RenderTargetDescription description;
    RenderTargetDescription::Construct(&description, G_GameRendererController);
    description.displayWidth = g_ScreenWidth;
    description.displayHeight = g_ScreenHeight;
    description.width = g_ScreenWidth;
    description.height = g_ScreenHeight;
    u32 black;
    GetColor(&black, ColourBlack);
    description.clearColor = black;
    description.offsetX = 0;
    description.offsetY = 0;
    Renderer* renderer = G_GameRendererController->CreateRenderer(&description, 1);
    renderer->flags.draws = 1;
    G_Renderer_ = renderer;
    for (u32 index = 0; index < ReadersStorageCount; index++)
    {
        GameReadersStorage* storage = InitReadersStorage(index);
        if (context->archivePath.length != 0)
        {
            FileStreamOpenArchive(storage->stream, context->archivePath.string, 1, storage);
        }
    }

    // The game's font, as the game controller reads it
    auto* font = static_cast<Font*>(MemoryAllocate(sizeof(Font)));
    Font::Construct(font);
    font->Read("StartUp\\Fonts\\Crash_Euro");
    font->width = g_FontSize.x;
    font->height = g_FontSize.y;
    std::printf("harness: font of %d glyphs, %d pages\n", font->glyphCount, font->pageCount);

    // A screen picture's eight tiles, read by the sprites' own reader as OLEG's picture reader does
    std::vector<Sprite> tiles(TileCount);
    {
        MemoryStream stream;
        MemoryStream::ConstructFromFile(&stream, picturePath.c_str(), false);
        for (Sprite& tile : tiles)
        {
            Sprite::Construct(&tile);
            CallVirtual<void>(&tile, tile.vtable, Shape2D::ReadSlot, static_cast<Stream*>(&stream));
        }

        stream.Destroy(DestroyOnly);
    }

    G_GameRendererController->BeginFrame();
    auto started = std::chrono::steady_clock::now();
    for (u32 frame = 0; frame < frames; frame++)
    {
        // The tiles placed as TiledPicture::Draw places them (the picture's middle at the screen's, the whole screen), in the
        // colour OLEG shows its pictures in; the text as the game queues its texts
        Vector2 corner = {0.0f, 0.0f};
        Vector2 size = {0.25f, 0.5f};
        Matrix4x4 matrix;
        InitIdentityMatrix(&matrix);
        matrix.m[0][0] = size.x;
        matrix.m[1][1] = size.y;
        renderer->colour = g_OlegPictureColour;
        for (u32 tile = 0; tile < TileCount; tile++)
        {
            matrix.m[3][0] = corner.x + size.x * static_cast<f32>(tile % 4);
            matrix.m[3][1] = corner.y + size.y * static_cast<f32>(tile / 4);
            QueuePlacedShape(renderer, &matrix, &tiles[tile], 4);
        }

        renderer->font = font;
        renderer->textAlignment.value = TextAlignment::Centred;
        GetColor(&renderer->colour, ColourWhite);
        // (The legal screen's references have the text off the screen; a glyph test puts it in the middle)
        if (glyphCode >= 0)
        {
            QueueText(renderer, text.c_str(), 0.35f, 0.75f);
        }
        else
        {
            QueueText(renderer, text.c_str(), 256.0f, 256.0f);
        }
        RenderFrame(renderer);
        if (!saving)
        {
            continue;
        }

        char name[32];
        std::snprintf(name, sizeof(name), "frame%02u", frame);
        SavePicture(folder, name);
        std::snprintf(name, sizeof(name), "drawn%02u", frame);
        if (scale == 1)
        {
            SaveBuffer(folder, name, Gs::PSMCT32, g_FrameBufferPage * 32, 8, 512, 512);
        }
        NativeGraphics::Hardware& hardware = NativeGraphics::GetHardware();
        std::printf("harness: %llu primitives, %llu pixels, %u VU1 programs, %llu VU1 instructions\n",
                    static_cast<unsigned long long>(hardware.gs->primitives), static_cast<unsigned long long>(hardware.gs->pixels),
                    hardware.vif1->programs, static_cast<unsigned long long>(hardware.vu1->executed));
    }

    if (postFilter >= 0)
    {
        int inUse = NativeGraphicsPostFilterInUse();
        std::printf("harness: post filter %d asked for, %d in use\n", postFilter, inUse);
        if (inUse != postFilter)
        {
            return 1;
        }
    }

    if (!saving)
    {
        auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        std::printf("harness: %u frames, %.2f ms a frame\n", frames, seconds * 1000.0 / frames);
    }

    return 0;
}

