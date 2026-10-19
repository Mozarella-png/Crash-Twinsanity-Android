// The mouse in play: its movement is the right stick (the camera, and Crash's head look) and its wheel turns are keys. While the
// game is played (GameController's StatePlaying, running, no letterboxed cutscene, the settings shut, the window in front) the
// window holds the mouse in SDL's relative mode, the cursor hidden; anywhere else the mouse is free for the menus' cursor.
// Each of the pad's reads turns the motion since the last one into the stick's deflection: the speed (points a second) over the
// sensitivity's full-deflection speed, so a slow move is a small push and a flick saturates; with no motion it falls back to the
// middle within 50 ms, so it never drifts
#include "ui/bindings.h"

#include "native.h"

#include "game/context.h"
#include "game/gamecontroller.h"
#include "game/oleg.h"

#include <atomic>
#include <chrono>
#include <cmath>

namespace NativeUi
{
namespace
{
using Clock = std::chrono::steady_clock;

// The motion since the last read (the window's points), counted only while the window holds the mouse
std::atomic<f32> g_DeltaX{0.0f};
std::atomic<f32> g_DeltaY{0.0f};
std::atomic<bool> g_Relative{false};
// The wheel's turns since they were taken, counted in play
std::atomic<s32> g_WheelUp{0};
std::atomic<s32> g_WheelDown{0};
std::atomic<bool> g_InPlay{false};
SDL_Window* g_Window = nullptr;
f32 g_StickX = 0.0f;
f32 g_StickY = 0.0f;
Clock::time_point g_LastRead;
bool g_HaveRead = false;
// The wheel's keys held this read (a turn holds its key for one read, then lets go for one)
bool g_WheelHeld[2] = {};

// The sensitivity's full deflection: 2400 points a second over the setting (5: 480 points a second)
constexpr f32 FullSpeedAtOne = 2400.0f;
// The game's own stick dead zone swallows small pushes: a moving mouse pushes at least this far
constexpr f32 LeastPush = 0.2f;
constexpr f32 DecaySeconds = 0.05f;

void AddAtomic(std::atomic<f32>& to, f32 by)
{
    f32 value = to.load();
    while (!to.compare_exchange_weak(value, value + by))
    {
    }
}

bool WatchMouseLook(void*, SDL_Event* event)
{
    switch (event->type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        if (g_Relative)
        {
            AddAtomic(g_DeltaX, event->motion.xrel);
            AddAtomic(g_DeltaY, event->motion.yrel);
        }

        break;
    case SDL_EVENT_MOUSE_WHEEL:
        if (g_InPlay)
        {
            f32 y = event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event->wheel.y : event->wheel.y;
            if (y > 0.0f)
            {
                g_WheelUp++;
            }
            else if (y < 0.0f)
            {
                g_WheelDown++;
            }
        }

        break;
    default:
        break;
    }

    return true;
}

// Whether the game is being played (not paused, in a menu, the settings or a cutscene)
bool Playing()
{
    GameController* controller = G_GameController;
    if (controller == nullptr || controller->State() != GameController::StatePlaying || g_GameState != GameStateRunning ||
        SettingsOpen())
    {
        return false;
    }

    // A cutscene's bars (OLEG's letterbox) showing
    u32 bars = controller->oleg.letterboxBottom.flags.state;
    return bars != Widget::StateAppearing && bars != Widget::StateShown && bars != Widget::StateDisappearing;
}

f32 Push(f32 delta, f32 seconds, f32 fullSpeed)
{
    f32 speed = std::fabs(delta) / seconds;
    f32 push = std::min(1.0f, LeastPush + (1.0f - LeastPush) * speed / fullSpeed);
    return delta < 0.0f ? -push : push;
}
}

s32 MouseButtonKey(u8 sdlButton)
{
    switch (sdlButton)
    {
    case SDL_BUTTON_LEFT:
        return KeyMouseLeft;
    case SDL_BUTTON_RIGHT:
        return KeyMouseRight;
    case SDL_BUTTON_MIDDLE:
        return KeyMouseMiddle;
    case SDL_BUTTON_X1:
        return KeyMouse4;
    case SDL_BUTTON_X2:
        return KeyMouse5;
    default:
        return -1;
    }
}

void StartMouseLook()
{
    static bool watching = false;
    if (!watching)
    {
        watching = true;
        SDL_AddEventWatch(WatchMouseLook, nullptr);
    }
}

void MouseLookFrame()
{
    bool playing = Playing();
    g_InPlay = playing;
    if (!playing)
    {
        g_WheelUp = 0;
        g_WheelDown = 0;
    }

    // The window holds the mouse in play with mouse look on, in front (headless: never)
    SDL_Window* focus = SDL_GetKeyboardFocus();
    bool want = playing && IsOn(OptionMouseLook) && focus != nullptr && !Native::GetSettings().headless;
    if (want == g_Relative)
    {
        return;
    }

    if (want)
    {
        g_Window = focus;
        SDL_SetWindowRelativeMouseMode(g_Window, true);
    }
    else if (g_Window != nullptr)
    {
        SDL_SetWindowRelativeMouseMode(g_Window, false);
    }

    g_DeltaX = 0.0f;
    g_DeltaY = 0.0f;
    g_StickX = 0.0f;
    g_StickY = 0.0f;
    g_Relative = want;
}

bool MouseLookActive()
{
    return g_Relative;
}

bool MouseLookStick(f32* x, f32* y)
{
    Clock::time_point now = Clock::now();
    f32 seconds = g_HaveRead ? std::chrono::duration<f32>(now - g_LastRead).count() : 1.0f / 60.0f;
    seconds = std::min(0.1f, std::max(0.001f, seconds));
    g_LastRead = now;
    g_HaveRead = true;
    f32 dx = g_DeltaX.exchange(0.0f);
    f32 dy = g_DeltaY.exchange(0.0f);
    if (!g_Relative)
    {
        g_StickX = 0.0f;
        g_StickY = 0.0f;
        *x = 0.0f;
        *y = 0.0f;
        return false;
    }

    f32 sensitivity = static_cast<f32>(std::max<s32>(1, GetOption(OptionMouseSensitivity)));
    f32 fullSpeed = FullSpeedAtOne / sensitivity;
    // Each axis by its own motion; an axis without any falls back to the middle
    f32 decay = std::max(0.0f, 1.0f - seconds / DecaySeconds);
    g_StickX = dx != 0.0f ? Push(dx, seconds, fullSpeed) : g_StickX * decay;
    g_StickY = dy != 0.0f ? Push(dy, seconds, fullSpeed) : g_StickY * decay;
    if (std::fabs(g_StickX) < 0.01f)
    {
        g_StickX = 0.0f;
    }

    if (std::fabs(g_StickY) < 0.01f)
    {
        g_StickY = 0.0f;
    }

    // The game's right stick turns the camera the other way to a mouse's look (the PS2's stick moves the camera round Crash):
    // the mouse's motion taken the other way on both axes, so moving it right looks right and up looks up ("invert mouse Y" and
    // the accessibility's "invert X" turn them back)
    *x = -g_StickX;
    *y = IsOn(OptionInvertMouseY) ? g_StickY : -g_StickY;
    return g_StickX != 0.0f || g_StickY != 0.0f;
}

void TakeWheel(bool* up, bool* down)
{
    for (u32 which = 0; which < 2; which++)
    {
        std::atomic<s32>& turns = which == 0 ? g_WheelUp : g_WheelDown;
        if (g_WheelHeld[which])
        {
            g_WheelHeld[which] = false;
        }
        else if (turns > 0)
        {
            turns--;
            g_WheelHeld[which] = true;
        }
    }

    *up = g_WheelHeld[0];
    *down = g_WheelHeld[1];
}

void MouseLookTestMotion(f32 dx, f32 dy)
{
    AddAtomic(g_DeltaX, dx);
    AddAtomic(g_DeltaY, dy);
}

void MouseLookForceForTest(bool relative)
{
    g_Relative = relative;
}
}
