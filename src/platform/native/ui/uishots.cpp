// Debugging aid ($TWIN_UI_SHOTS=DIR, with --headless): the game's own menus driven by scripted presses and their pictures saved as
// PNGs, to look at the native build's options pages next to the retail ones without playing up to them. Straight from the start-up
// to the main menu (past the logos and the title, through the game controller's next state, as --quick does), then the options:
// each page entered through the game's own menu input and saved once its fade is over. $TWIN_UI_RETAIL=1 leaves the native items
// out (NativeUi::NativeItemsShown), for the retail pages to compare with
#include "ui/bindings.h"

#include "graphics/display.h"
#include "native.h"

#include "game/gamecontroller.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace NativeUi
{
namespace
{
enum StepKind
{
    Press,
    // The menus' select and back as the prompts' style has them (MenuSelectBit, MenuBackBit)
    Select,
    Back,
    Shot,
    // The mouse onto a line (buttons: its index among the lines drawn last frame), and onto one and clicked
    MouseLine,
    MouseClick,
    Quit,
};

struct Step
{
    u32 frame;
    StepKind kind;
    u32 buttons;
    const char* name;
};

// Frames from the main menu's first (the game's frames, 50 a second): a press is held 4 frames, a page's fade takes half a second
constexpr Step Script[] = {
    {80, Shot, 0, "main_menu"},
    {90, Press, PadBitDown, nullptr},
    {100, Press, PadBitDown, nullptr},
    {110, Select, 0, nullptr},
    {170, Shot, 0, "options"},
    {180, Select, 0, nullptr},
    {240, Shot, 0, "graphic_options"},
    {250, Press, PadBitDown, nullptr},
    {260, Press, PadBitDown, nullptr},
    {300, Shot, 0, "graphic_options_third_item"},
    {310, Press, PadBitDown, nullptr},
    {320, Press, PadBitDown, nullptr},
    {360, Shot, 0, "graphic_options_fifth_item"},
    {370, Back, 0, nullptr},
    {420, Press, PadBitDown, nullptr},
    {430, Select, 0, nullptr},
    {490, Shot, 0, "sound_options"},
    {500, Back, 0, nullptr},
    {550, Press, PadBitDown, nullptr},
    {560, Select, 0, nullptr},
    {620, Shot, 0, "game_options"},
    {630, Back, 0, nullptr},
    {680, Press, PadBitDown, nullptr},
    {690, Select, 0, nullptr},
    {750, Shot, 0, "controls"},
    {760, Press, PadBitDown, nullptr},
    {770, Press, PadBitDown, nullptr},
    {780, Press, PadBitDown, nullptr},
    {790, Press, PadBitDown, nullptr},
    {800, Press, PadBitDown, nullptr},
    {810, Press, PadBitDown, nullptr},
    {820, Press, PadBitDown, nullptr},
    {830, Press, PadBitDown, nullptr},
    {880, Shot, 0, "controls_scrolled"},
    {890, MouseLine, 1, nullptr},
    {930, Shot, 0, "controls_mouse_hover"},
    {940, MouseClick, 3, nullptr},
    {980, Shot, 0, "controls_mouse_click"},
    {990, Quit, 0, nullptr},
};
constexpr u32 PressFrames = 4;

// $TWIN_UI_SCRIPT=newgame: the main menu's new game chosen, then a picture every second (a select each few seconds for whatever
// the game asks), for following a new game's saving
constexpr u32 NewGameFrames = 2500;

bool NewGameScript()
{
    const char* script = std::getenv("TWIN_UI_SCRIPT");
    return script != nullptr && std::string(script) == "newgame";
}

std::string g_Folder;
bool g_Checked = false;
u32 g_MenuFrame = 0;
bool g_InMenu = false;

void Be32(std::vector<u8>& out, u32 value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        out.push_back(static_cast<u8>(value >> shift));
    }
}

u32 Crc(const u8* data, size_t size)
{
    u32 crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
        {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
        }
    }

    return crc ^ 0xFFFFFFFF;
}

void Chunk(std::vector<u8>& out, const char* type, const std::vector<u8>& data)
{
    Be32(out, static_cast<u32>(data.size()));
    std::vector<u8> typed(type, type + 4);
    typed.insert(typed.end(), data.begin(), data.end());
    out.insert(out.end(), typed.begin(), typed.end());
    Be32(out, Crc(typed.data(), typed.size()));
}

bool WritePng(const std::string& path, const u32* pixels, u32 width, u32 height)
{
    std::vector<u8> raw;
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

    for (size_t at = 0;;)
    {
        size_t length = std::min<size_t>(65535, raw.size() - at);
        bool last = at + length >= raw.size();
        zlib.insert(zlib.end(), {static_cast<u8>(last ? 1 : 0), static_cast<u8>(length), static_cast<u8>(length >> 8),
                                 static_cast<u8>(~length), static_cast<u8>(~length >> 8)});
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

bool Active()
{
    if (!g_Checked)
    {
        g_Checked = true;
        const char* folder = std::getenv("TWIN_UI_SHOTS");
        if (folder != nullptr && folder[0] != '\0')
        {
            g_Folder = folder;
            std::error_code ignored;
            std::filesystem::create_directories(g_Folder, ignored);
            Native::Log("ui shots: the menus driven to %s", folder);
        }
    }

    return !g_Folder.empty();
}
}

bool WritePngFile(const std::string& path, const u32* pixels, u32 width, u32 height)
{
    return WritePng(path, pixels, width, height);
}

bool NativeItemsShown()
{
    const char* retail = std::getenv("TWIN_UI_RETAIL");
    return retail == nullptr || retail[0] != '1';
}

// The script's frame (from GameController::Update, through Frame): to the main menu, then the steps
void UiShotsFrame()
{
    if (!Active())
    {
        return;
    }

    GameController* controller = G_GameController;
    if (controller == nullptr)
    {
        return;
    }

    u32 state = controller->State();
    if (!g_InMenu)
    {
        if (state == GameController::StateVivendiLogo)
        {
            controller->SetNextState(GameController::StateMainMenu);
        }
        else if (state == GameController::StateMainMenu)
        {
            g_InMenu = true;
            g_MenuFrame = 0;
        }

        return;
    }

    g_MenuFrame++;
    if (NewGameScript())
    {
        if (g_MenuFrame % 50 == 0)
        {
            const NativeGraphics::Picture& picture = NativeGraphics::LastPicture();
            char name[64];
            std::snprintf(name, sizeof(name), "/newgame_%04u.png", g_MenuFrame / 50);
            if (picture.width != 0)
            {
                WritePng(g_Folder + name, picture.pixels.data(), picture.width, picture.height);
            }
        }

        if (g_MenuFrame == NewGameFrames)
        {
            Native::Log("ui shots: done");
            std::exit(0);
        }

        return;
    }

    for (const Step& step : Script)
    {
        if (step.kind == Shot && step.frame == g_MenuFrame)
        {
            const NativeGraphics::Picture& picture = NativeGraphics::LastPicture();
            std::string path = g_Folder + "/" + step.name + ".png";
            if (picture.width != 0 && WritePng(path, picture.pixels.data(), picture.width, picture.height))
            {
                Native::Log("ui shots: %s (%ux%u)", path.c_str(), picture.width, picture.height);
            }
        }
        else if ((step.kind == MouseLine || step.kind == MouseClick) && step.frame == g_MenuFrame)
        {
            if (!ScriptMouseToLine(step.buttons, step.kind == MouseClick))
            {
                Native::Log("ui shots: no line %u for the mouse", step.buttons);
            }
        }
        else if (step.kind == Quit && step.frame == g_MenuFrame)
        {
            Native::Log("ui shots: done");
            std::exit(0);
        }
    }
}

// The script's buttons this frame (ReadBindings adds them)
u32 UiShotsButtons()
{
    if (!Active() || !g_InMenu)
    {
        return 0;
    }

    u32 buttons = 0;
    if (NewGameScript())
    {
        // New game at 80, then select every 4 seconds from 400 for what the game asks
        u32 at = g_MenuFrame;
        bool pressing = (at >= 80 && at < 80 + PressFrames) || (at >= 400 && at % 200 < PressFrames);
        return pressing ? MenuSelectBit() : 0;
    }

    for (const Step& step : Script)
    {
        if (g_MenuFrame >= step.frame && g_MenuFrame < step.frame + PressFrames)
        {
            buttons |= step.kind == Press ? step.buttons : step.kind == Select ? MenuSelectBit() : step.kind == Back ? MenuBackBit() : 0;
        }
    }

    return buttons;
}
}
