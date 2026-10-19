// A sampling profiler for Windows (testing, $TWIN_PROFILE=FILE): a thread of its own stops every other thread of the process about
// once a millisecond, takes its stack's return addresses (by the unwinding tables: the executable's and the DLLs', the OpenGL
// driver's among them) and lets it go on. After $TWIN_PROFILE_DELAY seconds (default 15: past the loading) it samples for
// $TWIN_PROFILE_SECONDS (default 20), then writes FILE: a line a sample, "thread: address address ..." from the innermost frame
// out, each address as module+offset. native/tools/profile_report.py makes a report of it with the functions' names
#if defined(_WIN32)
#include "native.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>

namespace
{
constexpr u32 MostFrames = 24;

struct Sample
{
    DWORD thread;
    u32 count;
    DWORD64 frames[MostFrames];
};

// The modules' bases and names, for writing the addresses as module+offset
struct Module
{
    DWORD64 base;
    DWORD64 end;
    std::string name;
};

std::vector<Module> Modules()
{
    std::vector<Module> modules;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return modules;
    }

    MODULEENTRY32 entry = {};
    entry.dwSize = sizeof(entry);
    for (BOOL ok = Module32First(snapshot, &entry); ok; ok = Module32Next(snapshot, &entry))
    {
        modules.push_back({reinterpret_cast<DWORD64>(entry.modBaseAddr),
                           reinterpret_cast<DWORD64>(entry.modBaseAddr) + entry.modBaseSize, entry.szModule});
    }

    CloseHandle(snapshot);
    return modules;
}

std::vector<DWORD> OtherThreads()
{
    std::vector<DWORD> threads;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return threads;
    }

    THREADENTRY32 entry = {};
    entry.dwSize = sizeof(entry);
    for (BOOL ok = Thread32First(snapshot, &entry); ok; ok = Thread32Next(snapshot, &entry))
    {
        if (entry.th32OwnerProcessID == GetCurrentProcessId() && entry.th32ThreadID != GetCurrentThreadId())
        {
            threads.push_back(entry.th32ThreadID);
        }
    }

    CloseHandle(snapshot);
    return threads;
}

// The stack of a stopped thread from its context (RtlVirtualUnwind: nothing it calls may take a lock the thread holds, so no
// allocation here)
u32 Walk(CONTEXT context, DWORD64* frames)
{
    u32 count = 0;
    while (count < MostFrames && context.Rip != 0)
    {
        frames[count++] = context.Rip;
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
        if (function == nullptr)
        {
            // A leaf function: its return address is at the top of the stack
            if (context.Rsp == 0)
            {
                break;
            }

            context.Rip = *reinterpret_cast<DWORD64*>(context.Rsp);
            context.Rsp += 8;
            continue;
        }

        PVOID handlerData = nullptr;
        DWORD64 establisher = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context, &handlerData, &establisher, nullptr);
    }

    return count;
}

void Profile(std::string path, double delay, double seconds)
{
    std::this_thread::sleep_for(std::chrono::duration<double>(delay));
    Native::Log("profile: sampling for %.0f s", seconds);
    std::vector<Sample> samples;
    samples.reserve(static_cast<size_t>(seconds * 1000 * 8));
    std::unordered_map<DWORD, HANDLE> handles;
    auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
    // The threads listed again twice a second (a snapshot of the system's threads takes milliseconds)
    std::vector<DWORD> threads;
    auto listed = std::chrono::steady_clock::time_point();
    while (std::chrono::steady_clock::now() < end)
    {
        if (std::chrono::steady_clock::now() - listed > std::chrono::milliseconds(500))
        {
            threads = OtherThreads();
            listed = std::chrono::steady_clock::now();
        }

        for (DWORD id : threads)
        {
            HANDLE& thread = handles[id];
            if (thread == nullptr)
            {
                thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, id);
            }

            if (thread == nullptr || SuspendThread(thread) == static_cast<DWORD>(-1))
            {
                continue;
            }

            CONTEXT context = {};
            context.ContextFlags = CONTEXT_FULL;
            Sample sample = {};
            sample.thread = id;
            if (GetThreadContext(thread, &context))
            {
                sample.count = Walk(context, sample.frames);
            }

            ResumeThread(thread);
            if (sample.count != 0)
            {
                samples.push_back(sample);
            }
        }

        std::this_thread::sleep_for(std::chrono::microseconds(900));
    }

    std::vector<Module> modules = Modules();
    FILE* file = std::fopen(path.c_str(), "w");
    if (file == nullptr)
    {
        Native::Log("profile: can't write %s", path.c_str());
        return;
    }

    for (const Sample& sample : samples)
    {
        std::fprintf(file, "%lu:", static_cast<unsigned long>(sample.thread));
        for (u32 i = 0; i < sample.count; i++)
        {
            DWORD64 address = sample.frames[i];
            const Module* in = nullptr;
            for (const Module& module : modules)
            {
                if (address >= module.base && address < module.end)
                {
                    in = &module;
                    break;
                }
            }

            if (in != nullptr)
            {
                std::fprintf(file, " %s+%llx", in->name.c_str(), static_cast<unsigned long long>(address - in->base));
            }
            else
            {
                std::fprintf(file, " ?+%llx", static_cast<unsigned long long>(address));
            }
        }

        std::fputc('\n', file);
    }

    std::fclose(file);
    Native::Log("profile: %zu samples in %s", samples.size(), path.c_str());
}
}

namespace Native
{
void StartProfiler()
{
    const char* path = std::getenv("TWIN_PROFILE");
    if (path == nullptr)
    {
        return;
    }

    const char* delay = std::getenv("TWIN_PROFILE_DELAY");
    const char* seconds = std::getenv("TWIN_PROFILE_SECONDS");
    std::thread(Profile, std::string(path), delay != nullptr ? std::atof(delay) : 15.0,
                seconds != nullptr ? std::atof(seconds) : 20.0)
        .detach();
}
}
#endif
