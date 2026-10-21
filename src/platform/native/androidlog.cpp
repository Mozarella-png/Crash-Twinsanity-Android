// The Android build's log file: stdout and stderr go to <settings folder>/log.txt instead of logcat (which the phone doesn't
// keep). The app's settings folder is files/settings in the app's private folder: on a debug build Android Studio's Device
// File Explorer shows it as data/data/com.giopixel.crashtwinsanity/files/settings/log.txt. CrashActivity.java writes its own
// lines (the device, the activity's life, the disc import) to the same file.
//
// Every second the log gets a line on the game: the frames it made (how many, the interval's average and slowest, how many
// over 50 ms), the pictures shown and how many of them had nothing drawn since the one before (a black picture while the game
// runs). When the game makes no frame for 2 s the log says so (a stall: its thread is stuck, not just slow), every 5 s, and
// says when it goes on.
#if defined(__ANDROID__)
#include "native.h"

#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

namespace
{
using Clock = std::chrono::steady_clock;

// The counts of the second (the game's thread and the render's main thread both write them)
std::mutex g_Lock;
Clock::time_point g_LastFrame;
u32 g_Frames = 0;
double g_IntervalSum = 0.0;
double g_Slowest = 0.0;
u32 g_Slow = 0;
u32 g_Presented = 0;
u32 g_Blank = 0;
u64 g_LastPrimitives = 0;
bool g_Stalled = false;
u32 g_StalledSeconds = 0;

// The once-a-second line, and the stall's notes (on the watch's thread)
void Watch()
{
    using namespace std::chrono;
    for (;;)
    {
        std::this_thread::sleep_for(seconds(1));
        std::lock_guard<std::mutex> guard(g_Lock);
        const auto now = Clock::now();
        const bool started = g_LastFrame != Clock::time_point{};
        const double since = started ? duration<double>(now - g_LastFrame).count() : 0.0;
        if (started && since > 2.0)
        {
            if (!g_Stalled)
            {
                g_Stalled = true;
                g_StalledSeconds = 0;
                Native::Log("stall: no frame for %.1f s (the game's thread is stuck, or nearly)", since);
            }
            else if (++g_StalledSeconds % 5 == 0)
            {
                Native::Log("stall: still no frame after %.0f s", since);
            }
        }

        if (g_Frames != 0 || g_Presented != 0)
        {
            double average = g_Frames != 0 ? g_IntervalSum / g_Frames : 0.0;
            Native::Log("second: %u frames (%.1f ms average, %.1f ms slowest, %u over 50 ms); %u pictures, %u with nothing "
                        "drawn since the last",
                        g_Frames, average, g_Slowest, g_Slow, g_Presented, g_Blank);
        }
        g_Frames = 0;
        g_IntervalSum = 0.0;
        g_Slowest = 0.0;
        g_Slow = 0;
        g_Presented = 0;
        g_Blank = 0;
    }
}
}

void Native::StartAndroidLog()
{
    const char* dir = std::getenv("TWIN_SETTINGS_DIR");
    if (dir == nullptr || *dir == '\0')
    {
        return;
    }

    std::string path = std::string(dir) + "/log.txt";
    int file = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file < 0)
    {
        return;
    }

    dup2(file, STDOUT_FILENO);
    dup2(file, STDERR_FILENO);
    close(file);
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    Native::Log("log: the game's log, appended to %s", path.c_str());
    std::thread(Watch).detach();
}

void Native::GameFrameDone()
{
    const auto now = Clock::now();
    std::lock_guard<std::mutex> guard(g_Lock);
    if (g_LastFrame != Clock::time_point{})
    {
        double interval = std::chrono::duration<double, std::milli>(now - g_LastFrame).count();
        g_Frames++;
        g_IntervalSum += interval;
        g_Slowest = interval > g_Slowest ? interval : g_Slowest;
        g_Slow += interval > 50.0 ? 1 : 0;
    }

    g_LastFrame = now;
    if (g_Stalled)
    {
        g_Stalled = false;
        Native::Log("stall: frames again after %u s", g_StalledSeconds);
    }
}

void Native::AndroidPresented(u64 primitives)
{
    std::lock_guard<std::mutex> guard(g_Lock);
    g_Presented++;
    if (primitives == g_LastPrimitives)
    {
        g_Blank++;
    }

    g_LastPrimitives = primitives;
}
#endif
