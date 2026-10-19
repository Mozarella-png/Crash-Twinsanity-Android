// The I/O processor (iop.h): the R3000's interpreter, the modules' loading and linking, the kernel's libraries the modules import,
// the DMA channels of the SPU2, the CD/DVD drive's reads and the SIF RPC calls from the EE. Every kernel function behaves as the
// IOP's (PS2SDK's headers have their numbers and arguments); native/AUDIO.md lists where timing is the emulator's own
#include "iop.h"

// The options' fast loading (src/platform/native/ui/options.cpp)
bool NativeFastDisc();

#include "native.h"
#include "spu2.h"

#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_map>

namespace
{
// ---------------------------------------------------------------------------------------------------------------------------
// Memory
alignas(16) u8 g_Ram[Iop::RamSize];
// The hardware registers at 0x1F801000 (DMA, the SSBUS's configuration): kept as written
u32 g_Hardware[0x1000 / 4];
u8 g_Scratchpad[0x400];
u64 g_Cycles = 0;

// Addresses the emulator returns to: a thread's function ending, an interrupt handler's, an RPC server function's, a module's
// start function's
constexpr u32 ThreadExit = 0xBFC00000;
constexpr u32 InterruptReturn = 0xBFC00010;
constexpr u32 RpcReturn = 0xBFC00020;
constexpr u32 StartReturn = 0xBFC00030;

u32 Physical(u32 address)
{
    return address & 0x1FFFFFFF;
}

bool InRam(u32 physical)
{
    return physical < 0x800000;
}

// The DMA registers of the SPU2's channels: channel 4 (core 0) at 0x1F8010C0, channel 7 (core 1) at 0x1F801500, each MADR, BCR
// and CHCR
constexpr u32 Dma4Base = 0x1F8010C0;
constexpr u32 Dma7Base = 0x1F801500;
constexpr u32 ChcrStart = 0x01000000;

void WriteHardware32(u32 physical, u32 value);
u32 MadrNow(u32 physical);

template <typename T>
T Load(u32 address)
{
    u32 physical = Physical(address);
    if (InRam(physical))
    {
        T value;
        std::memcpy(&value, g_Ram + (physical & (Iop::RamSize - 1)), sizeof(T));
        return value;
    }

    if (physical >= Spu2::RegisterBase && physical < Spu2::RegisterBase + Spu2::RegisterSize)
    {
        if constexpr (sizeof(T) == 4)
        {
            return static_cast<T>(Spu2::Read(physical) | static_cast<u32>(Spu2::Read(physical + 2)) << 16);
        }
        else
        {
            return static_cast<T>(Spu2::Read(physical));
        }
    }

    if (physical >= 0x1F801000 && physical < 0x1F802000)
    {
        u32 word = MadrNow(physical & ~3u);
        return static_cast<T>(word >> ((physical & 3) * 8));
    }

    if (physical >= 0x1F800000 && physical < 0x1F800400)
    {
        T value;
        std::memcpy(&value, g_Scratchpad + (physical & 0x3FF), sizeof(T));
        return value;
    }

    static bool logged = false;
    if (!logged)
    {
        logged = true;
        Native::Log("iop: read of unmapped %#x", address);
    }

    return 0;
}

template <typename T>
void Store(u32 address, T value)
{
    u32 physical = Physical(address);
    if (InRam(physical))
    {
        std::memcpy(g_Ram + (physical & (Iop::RamSize - 1)), &value, sizeof(T));
        return;
    }

    if (physical >= Spu2::RegisterBase && physical < Spu2::RegisterBase + Spu2::RegisterSize)
    {
        Spu2::Write(physical, static_cast<u16>(value));
        if constexpr (sizeof(T) == 4)
        {
            Spu2::Write(physical + 2, static_cast<u16>(static_cast<u32>(value) >> 16));
        }

        return;
    }

    if (physical >= 0x1F801000 && physical < 0x1F802000)
    {
        u32 index = (physical - 0x1F801000) / 4;
        u32 shift = (physical & 3) * 8;
        u32 mask = sizeof(T) == 4 ? 0xFFFFFFFF : ((1u << (sizeof(T) * 8)) - 1) << shift;
        u32 word = (g_Hardware[index] & ~mask) | ((static_cast<u32>(value) << shift) & mask);
        WriteHardware32(physical & ~3u, word);
        return;
    }

    if (physical >= 0x1F800000 && physical < 0x1F800400)
    {
        std::memcpy(g_Scratchpad + (physical & 0x3FF), &value, sizeof(T));
        return;
    }

    static bool logged = false;
    if (!logged)
    {
        logged = true;
        Native::Log("iop: write of unmapped %#x", address);
    }
}

u8* RamAt(u32 address)
{
    return g_Ram + (Physical(address) & (Iop::RamSize - 1));
}

std::string ReadString(u32 address)
{
    std::string out;
    for (u32 i = 0; i < 0x400; i++)
    {
        char c = static_cast<char>(Load<u8>(address + i));
        if (c == 0)
        {
            break;
        }

        out += c;
    }

    return out;
}

// ---------------------------------------------------------------------------------------------------------------------------
// The CPU's state, a thread's or an interrupt handler's
struct Context
{
    u32 r[32];
    u32 pc;
    u32 npc;
    u32 hi;
    u32 lo;
};

enum Register : u32
{
    Zero = 0,
    V0 = 2,
    V1 = 3,
    A0 = 4,
    A1 = 5,
    A2 = 6,
    A3 = 7,
    Gp = 28,
    Sp = 29,
    Ra = 31,
};

// ---------------------------------------------------------------------------------------------------------------------------
// The kernel's objects
enum class State
{
    Dormant,
    Ready,
    Waiting,
};

enum class Wait
{
    None,
    Semaphore,
    EventFlag,
    Sleep,
    Delay,
    RpcLoop,
    CdSync,
};

struct Thread
{
    s32 id;
    Context context;
    u32 attr;
    u32 option;
    u32 entry;
    u32 stack;
    u32 stackSize;
    u32 gp;
    s32 initialPriority;
    s32 priority;
    State state = State::Dormant;
    Wait wait = Wait::None;
    s32 waitId = 0;
    u32 waitBits = 0;
    u32 waitMode = 0;
    u32 waitResult = 0;
    u64 wakeAt = 0;
    s32 wakeups = 0;
    u64 readyOrder = 0;
    // An RPC loop's queue, and the context it waits in while no call runs
    u32 rpcQueue = 0;
    Context rpcSaved;
    Iop::Call* rpcCall = nullptr;
};

struct Semaphore
{
    u32 attr;
    u32 option;
    s32 count;
    s32 max;
    std::deque<s32> waiting;
};

struct EventFlag
{
    u32 attr;
    u32 option;
    u32 bits;
};

struct Handler
{
    u32 function = 0;
    u32 argument = 0;
    u32 gp = 0;
    bool enabled = false;
};

struct HardTimer
{
    bool allocated = false;
    bool started = false;
    u32 compare = 0;
    u32 handler = 0;
    u32 argument = 0;
    u32 gp = 0;
    u64 fireAt = 0;
};

struct RpcServer
{
    s32 id;
    u32 data;
    u32 function;
    u32 buffer;
    u32 queue;
};

std::unordered_map<s32, Thread> g_Threads;
std::unordered_map<s32, Semaphore> g_Semaphores;
std::unordered_map<s32, EventFlag> g_EventFlags;
std::vector<RpcServer> g_Servers;
// RPC queues by address: the thread that serves them
std::unordered_map<u32, s32> g_Queues;
std::deque<Iop::Call*> g_Calls;
s32 g_NextId = 1;
u64 g_ReadyOrder = 0;
Thread* g_Current = nullptr;
bool g_Reschedule = false;

// Interrupts: the handlers by line, those raised and not yet handled, whether the CPU takes them, whether a handler is running
constexpr s32 Lines = 64;
Handler g_Handlers[Lines];
u64 g_Pending = 0;
bool g_InterruptsEnabled = true;
bool g_InInterrupt = false;
HardTimer g_Timers[4];
constexpr s32 TimerIdBase = 0x100;

// The SPU2's DMA channels (core 0's is 4, core 1's 7): when a transfer started ends
struct DmaChannel
{
    bool busy = false;
    bool automatic = false;
    u64 doneAt = 0;
    // An AutoDMA transfer's start and size (halfwords): MADR moves on as the core takes its data
    u32 start = 0;
    u32 halfwords = 0;
};
DmaChannel g_Dma[2];

// The drive: a read under way, and the callback (sceCdCallback)
struct CdRead
{
    bool busy = false;
    u64 doneAt = 0;
    u32 sector = 0;
    u32 sectors = 0;
    u32 buffer = 0;
};
CdRead g_Cd;
u32 g_CdCallback = 0;
u32 g_CdCallbackGp = 0;
// The drive's reading: DVD at 4x (the PS2 reads the game's discs at up to 4x), in IOP cycles a byte
constexpr u64 CdBytesPerSecond = 4 * 1385000;
constexpr u64 CdSeekCycles = Iop::ClockRate / 1000;
// A fast read's time: long enough for the module's own interrupt handling to see it as done later
constexpr u64 FastCdCycles = 2000;

// The SPU2's samples come every 768 cycles
u64 g_NextSample = Spu2::CyclesPerSample;
u64 g_SamplesMade = 0;
std::vector<s16>* g_Output = nullptr;

// Memory the modules allocate (sysmem): blocks of 256 bytes from the heap's start to the top of memory
struct Block
{
    u32 address;
    u32 size;
};
std::vector<Block> g_Allocated;
u32 g_HeapStart = 0;
constexpr u32 AllocationAlignment = 0x100;

// The interrupt handlers' stack, under the modules
constexpr u32 InterruptStackTop = 0x1000;

bool g_Booted = false;
// $TWINSANITY_AUDIO_DEBUG: the drive's and the DMA's work logged
const bool g_Debug = std::getenv("TWINSANITY_AUDIO_DEBUG") != nullptr;
// Where STREAM.IRX was loaded (for $TWINSANITY_IOP_WATCH's offsets)
u32 g_StreamBase = 0;

// ---------------------------------------------------------------------------------------------------------------------------
// sysmem
u32 Allocate(u32 mode, u32 size, u32 at)
{
    size = (size + AllocationAlignment - 1) & ~(AllocationAlignment - 1);
    if (size == 0)
    {
        return 0;
    }

    std::sort(g_Allocated.begin(), g_Allocated.end(), [](const Block& a, const Block& b) { return a.address < b.address; });
    // The gaps between the blocks
    std::vector<Block> gaps;
    u32 previous = g_HeapStart;
    for (const Block& block : g_Allocated)
    {
        if (block.address > previous)
        {
            gaps.push_back({previous, block.address - previous});
        }

        previous = std::max(previous, block.address + block.size);
    }

    if (previous < Iop::RamSize)
    {
        gaps.push_back({previous, Iop::RamSize - previous});
    }

    u32 found = 0;
    if (mode == 0)
    {
        for (const Block& gap : gaps)
        {
            if (gap.size >= size)
            {
                found = gap.address;
                break;
            }
        }
    }
    else if (mode == 1)
    {
        for (auto gap = gaps.rbegin(); gap != gaps.rend(); ++gap)
        {
            if (gap->size >= size)
            {
                found = gap->address + gap->size - size;
                break;
            }
        }
    }
    else
    {
        at &= ~(AllocationAlignment - 1);
        for (const Block& gap : gaps)
        {
            if (at >= gap.address && at + size <= gap.address + gap.size)
            {
                found = at;
                break;
            }
        }
    }

    if (found == 0)
    {
        return 0;
    }

    g_Allocated.push_back({found, size});
    return found;
}

void Free(u32 address)
{
    for (auto block = g_Allocated.begin(); block != g_Allocated.end(); ++block)
    {
        if (block->address == Physical(address))
        {
            g_Allocated.erase(block);
            return;
        }
    }
}

u32 LargestFree()
{
    std::sort(g_Allocated.begin(), g_Allocated.end(), [](const Block& a, const Block& b) { return a.address < b.address; });
    u32 largest = 0;
    u32 previous = g_HeapStart;
    for (const Block& block : g_Allocated)
    {
        if (block.address > previous)
        {
            largest = std::max(largest, block.address - previous);
        }

        previous = std::max(previous, block.address + block.size);
    }

    if (previous < Iop::RamSize)
    {
        largest = std::max(largest, Iop::RamSize - previous);
    }

    return largest;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Scheduling
void MakeReady(Thread& thread, bool keepPlace = false)
{
    thread.state = State::Ready;
    thread.wait = Wait::None;
    if (!keepPlace)
    {
        thread.readyOrder = ++g_ReadyOrder;
    }

    // A thread of higher priority than the running one takes over at once
    if (g_Current == nullptr || thread.priority < g_Current->priority || g_Current->state != State::Ready)
    {
        g_Reschedule = true;
    }
}

void Block(Wait wait, s32 id = 0)
{
    if (g_Current == nullptr)
    {
        Native::Log("iop: a wait in an interrupt handler");
        return;
    }

    g_Current->state = State::Waiting;
    g_Current->wait = wait;
    g_Current->waitId = id;
    g_Reschedule = true;
}

// Ends a wait: the value the waiting call returns
void Release(Thread& thread, u32 result)
{
    thread.context.r[V0] = result;
    MakeReady(thread);
}

Thread* PickThread()
{
    Thread* best = nullptr;
    for (auto& [id, thread] : g_Threads)
    {
        if (thread.state != State::Ready)
        {
            continue;
        }

        if (best == nullptr || thread.priority < best->priority ||
            (thread.priority == best->priority && thread.readyOrder < best->readyOrder))
        {
            best = &thread;
        }
    }

    return best;
}

Thread* FindThread(s32 id)
{
    if (id == 0)
    {
        return g_Current;
    }

    auto found = g_Threads.find(id);
    return found == g_Threads.end() ? nullptr : &found->second;
}

void Raise(s32 line)
{
    g_Pending |= 1ull << line;
    g_Reschedule = true;
}

// ---------------------------------------------------------------------------------------------------------------------------
// The interpreter
void Step(Context& c);
void CallHle(Context& c, u32 index);

// Runs a function to its end on its own context (an interrupt handler, a callback): how its v0 came back
u32 RunNested(u32 function, u32 gp, u32 a0, u32 a1 = 0, u32 a2 = 0)
{
    Context context = {};
    context.r[Gp] = gp;
    context.r[Sp] = InterruptStackTop - 0x10;
    context.r[A0] = a0;
    context.r[A1] = a1;
    context.r[A2] = a2;
    context.r[Ra] = InterruptReturn;
    context.pc = function;
    context.npc = function + 4;
    bool wasInInterrupt = g_InInterrupt;
    g_InInterrupt = true;
    for (u32 steps = 0; context.pc != InterruptReturn; steps++)
    {
        if (steps > 50000000)
        {
            Native::Log("iop: the handler at %#x doesn't return", function);
            break;
        }

        Step(context);
        g_Cycles++;
    }

    g_InInterrupt = wasInInterrupt;
    return context.r[V0];
}

void DispatchInterrupts()
{
    while (g_InterruptsEnabled && !g_InInterrupt && g_Pending != 0)
    {
        s32 line = __builtin_ctzll(g_Pending);
        g_Pending &= ~(1ull << line);
        const Handler& handler = g_Handlers[line];
        if (handler.function == 0 || !handler.enabled)
        {
            continue;
        }

        RunNested(handler.function, handler.gp, handler.argument);
        g_Reschedule = true;
    }
}

void FinishCdRead();

// The events whose time has come: the SPU2's samples, the hardware timers, DMA ends, the drive's reads and threads' delays
void ProcessEvents()
{
    while (g_Cycles >= g_NextSample)
    {
        g_NextSample += Spu2::CyclesPerSample;
        Spu2::Stereo sample = Spu2::Tick();
        g_SamplesMade++;
        if (g_Output != nullptr)
        {
            g_Output->push_back(static_cast<s16>(sample.left));
            g_Output->push_back(static_cast<s16>(sample.right));
        }
    }

    for (s32 core = 0; core < 2; core++)
    {
        DmaChannel& dma = g_Dma[core];
        if (dma.busy && !dma.automatic && g_Cycles >= dma.doneAt)
        {
            dma.busy = false;
            u32 chcr = (core == 0 ? Dma4Base : Dma7Base) + 8;
            g_Hardware[(chcr - 0x1F801000) / 4] &= ~ChcrStart;
            // The SPU2 raises the channel's interrupt
            Spu2::DmaFinished(core);
        }
    }

    for (HardTimer& timer : g_Timers)
    {
        if (timer.started && g_Cycles >= timer.fireAt)
        {
            if (!g_InterruptsEnabled || g_InInterrupt)
            {
                break;
            }

            u32 next = RunNested(timer.handler, timer.gp, timer.argument);
            if (next == 0)
            {
                timer.started = false;
            }
            else
            {
                timer.compare = next;
                timer.fireAt += next;
            }

            g_Reschedule = true;
        }
    }

    if (g_Cd.busy && g_Cycles >= g_Cd.doneAt)
    {
        FinishCdRead();
    }

    for (auto& [id, thread] : g_Threads)
    {
        if (thread.state == State::Waiting && thread.wait == Wait::Delay && g_Cycles >= thread.wakeAt)
        {
            Release(thread, 0);
        }
    }
}

u64 NextEvent()
{
    u64 next = g_NextSample;
    for (const DmaChannel& dma : g_Dma)
    {
        if (dma.busy && !dma.automatic)
        {
            next = std::min(next, dma.doneAt);
        }
    }

    for (const HardTimer& timer : g_Timers)
    {
        if (timer.started)
        {
            next = std::min(next, timer.fireAt);
        }
    }

    if (g_Cd.busy)
    {
        next = std::min(next, g_Cd.doneAt);
    }

    for (const auto& [id, thread] : g_Threads)
    {
        if (thread.state == State::Waiting && thread.wait == Wait::Delay)
        {
            next = std::min(next, thread.wakeAt);
        }
    }

    return next;
}

// A call waiting for its server's thread, now in its loop: the thread runs the server's function on the call's data
void DeliverCalls()
{
    for (auto call = g_Calls.begin(); call != g_Calls.end();)
    {
        const RpcServer* server = nullptr;
        for (const RpcServer& s : g_Servers)
        {
            if (s.id == (*call)->server)
            {
                server = &s;
            }
        }

        if (server == nullptr)
        {
            Native::Log("iop: an RPC call of server %#x, which isn't there", static_cast<u32>((*call)->server));
            (*call)->reply.assign((*call)->replySize, 0);
            (*call)->done = true;
            call = g_Calls.erase(call);
            continue;
        }

        auto queue = g_Queues.find(server->queue);
        Thread* thread = queue == g_Queues.end() ? nullptr : FindThread(queue->second);
        if (thread == nullptr || thread->state != State::Waiting || thread->wait != Wait::RpcLoop)
        {
            ++call;
            continue;
        }

        Iop::Call* c = *call;
        std::memcpy(RamAt(server->buffer), c->send.data(), c->send.size());
        thread->rpcSaved = thread->context;
        thread->rpcCall = c;
        Context& context = thread->context;
        context.r[A0] = c->function;
        context.r[A1] = server->buffer;
        context.r[A2] = static_cast<u32>(c->send.size());
        context.r[Ra] = RpcReturn;
        context.pc = server->function;
        context.npc = server->function + 4;
        MakeReady(*thread);
        call = g_Calls.erase(call);
    }
}

void FinishRpc(Thread& thread)
{
    Iop::Call* call = thread.rpcCall;
    u32 reply = thread.context.r[V0];
    call->reply.assign(call->replySize, 0);
    if (reply != 0)
    {
        std::memcpy(call->reply.data(), RamAt(reply), call->replySize);
    }

    call->done = true;
    thread.rpcCall = nullptr;
    thread.context = thread.rpcSaved;
    thread.state = State::Waiting;
    thread.wait = Wait::RpcLoop;
    g_Reschedule = true;
}

// Runs the thread until the clock reaches the limit or it gives way
void RunThread(Thread& thread, u64 limit)
{
    g_Current = &thread;
    g_Reschedule = false;
    Context& c = thread.context;
    while (g_Cycles < limit && !g_Reschedule)
    {
        switch (c.pc)
        {
        case ThreadExit:
            thread.state = State::Dormant;
            g_Reschedule = true;
            continue;
        case RpcReturn:
            FinishRpc(thread);
            continue;
        case StartReturn:
            // A module's start function returned: its thread is the boot's, which ends
            thread.state = State::Dormant;
            g_Reschedule = true;
            continue;
        default:
            break;
        }

        Step(c);
        g_Cycles++;
    }

    g_Current = nullptr;
}

void Advance(u64 target)
{
    while (g_Cycles < target)
    {
        ProcessEvents();
        DispatchInterrupts();
        DeliverCalls();
        Thread* thread = PickThread();
        u64 next = std::min(target, NextEvent());
        if (thread == nullptr)
        {
            g_Cycles = std::max(g_Cycles, next);
            continue;
        }

        RunThread(*thread, std::max(next, g_Cycles + 1));
    }
}

// ---------------------------------------------------------------------------------------------------------------------------
// The R3000's instructions (MIPS I). The loads' delay slots are left out: the modules' compiler never reads a loaded register
// in the next instruction
void Branch(Context& c, u32 target)
{
    c.npc = target;
}

void Step(Context& c)
{
    u32 pc = c.pc;
    u32 instruction = Load<u32>(pc);
    c.pc = c.npc;
    c.npc += 4;
    u32 op = instruction >> 26;
    u32 rs = (instruction >> 21) & 31;
    u32 rt = (instruction >> 16) & 31;
    u32 rd = (instruction >> 11) & 31;
    u32 sa = (instruction >> 6) & 31;
    s32 immediate = static_cast<s16>(instruction & 0xFFFF);
    u32 uimmediate = instruction & 0xFFFF;
    u32* r = c.r;
    auto set = [&](u32 reg, u32 value) {
        if (reg != 0)
        {
            r[reg] = value;
        }
    };
    switch (op)
    {
    case 0x00:
        switch (instruction & 0x3F)
        {
        case 0x00:
            set(rd, r[rt] << sa);
            break;
        case 0x02:
            set(rd, r[rt] >> sa);
            break;
        case 0x03:
            set(rd, static_cast<u32>(static_cast<s32>(r[rt]) >> sa));
            break;
        case 0x04:
            set(rd, r[rt] << (r[rs] & 31));
            break;
        case 0x06:
            set(rd, r[rt] >> (r[rs] & 31));
            break;
        case 0x07:
            set(rd, static_cast<u32>(static_cast<s32>(r[rt]) >> (r[rs] & 31)));
            break;
        case 0x08:
            Branch(c, r[rs]);
            break;
        case 0x09:
        {
            u32 target = r[rs];
            set(rd, pc + 8);
            Branch(c, target);
            break;
        }
        case 0x0C:
            // The import stubs' calls of the emulator's kernel: syscall with the function's index
            CallHle(c, (instruction >> 6) & 0xFFFFF);
            break;
        case 0x0D:
            Native::Log("iop: break at %#x", pc);
            break;
        case 0x10:
            set(rd, c.hi);
            break;
        case 0x11:
            c.hi = r[rs];
            break;
        case 0x12:
            set(rd, c.lo);
            break;
        case 0x13:
            c.lo = r[rs];
            break;
        case 0x18:
        {
            s64 product = static_cast<s64>(static_cast<s32>(r[rs])) * static_cast<s32>(r[rt]);
            c.lo = static_cast<u32>(product);
            c.hi = static_cast<u32>(product >> 32);
            break;
        }
        case 0x19:
        {
            u64 product = static_cast<u64>(r[rs]) * r[rt];
            c.lo = static_cast<u32>(product);
            c.hi = static_cast<u32>(product >> 32);
            break;
        }
        case 0x1A:
        {
            s32 n = static_cast<s32>(r[rs]);
            s32 d = static_cast<s32>(r[rt]);
            if (d == 0)
            {
                c.lo = n >= 0 ? 0xFFFFFFFF : 1;
                c.hi = static_cast<u32>(n);
            }
            else if (n == INT32_MIN && d == -1)
            {
                c.lo = static_cast<u32>(INT32_MIN);
                c.hi = 0;
            }
            else
            {
                c.lo = static_cast<u32>(n / d);
                c.hi = static_cast<u32>(n % d);
            }

            break;
        }
        case 0x1B:
        {
            u32 n = r[rs];
            u32 d = r[rt];
            if (d == 0)
            {
                c.lo = 0xFFFFFFFF;
                c.hi = n;
            }
            else
            {
                c.lo = n / d;
                c.hi = n % d;
            }

            break;
        }
        case 0x20:
        case 0x21:
            set(rd, r[rs] + r[rt]);
            break;
        case 0x22:
        case 0x23:
            set(rd, r[rs] - r[rt]);
            break;
        case 0x24:
            set(rd, r[rs] & r[rt]);
            break;
        case 0x25:
            set(rd, r[rs] | r[rt]);
            break;
        case 0x26:
            set(rd, r[rs] ^ r[rt]);
            break;
        case 0x27:
            set(rd, ~(r[rs] | r[rt]));
            break;
        case 0x2A:
            set(rd, static_cast<s32>(r[rs]) < static_cast<s32>(r[rt]) ? 1 : 0);
            break;
        case 0x2B:
            set(rd, r[rs] < r[rt] ? 1 : 0);
            break;
        default:
            Native::Log("iop: unknown instruction %08x at %#x", instruction, pc);
            break;
        }

        break;
    case 0x01:
    {
        bool link = (rt & 0x1E) == 0x10;
        bool taken = (rt & 1) != 0 ? static_cast<s32>(r[rs]) >= 0 : static_cast<s32>(r[rs]) < 0;
        if (link)
        {
            r[Ra] = pc + 8;
        }

        if (taken)
        {
            Branch(c, pc + 4 + (static_cast<u32>(immediate) << 2));
        }

        break;
    }
    case 0x02:
        Branch(c, (c.pc & 0xF0000000) | ((instruction & 0x3FFFFFF) << 2));
        break;
    case 0x03:
        r[Ra] = pc + 8;
        Branch(c, (c.pc & 0xF0000000) | ((instruction & 0x3FFFFFF) << 2));
        break;
    case 0x04:
        if (r[rs] == r[rt])
        {
            Branch(c, pc + 4 + (static_cast<u32>(immediate) << 2));
        }

        break;
    case 0x05:
        if (r[rs] != r[rt])
        {
            Branch(c, pc + 4 + (static_cast<u32>(immediate) << 2));
        }

        break;
    case 0x06:
        if (static_cast<s32>(r[rs]) <= 0)
        {
            Branch(c, pc + 4 + (static_cast<u32>(immediate) << 2));
        }

        break;
    case 0x07:
        if (static_cast<s32>(r[rs]) > 0)
        {
            Branch(c, pc + 4 + (static_cast<u32>(immediate) << 2));
        }

        break;
    case 0x08:
    case 0x09:
        set(rt, r[rs] + static_cast<u32>(immediate));
        break;
    case 0x0A:
        set(rt, static_cast<s32>(r[rs]) < immediate ? 1 : 0);
        break;
    case 0x0B:
        set(rt, r[rs] < static_cast<u32>(immediate) ? 1 : 0);
        break;
    case 0x0C:
        set(rt, r[rs] & uimmediate);
        break;
    case 0x0D:
        set(rt, r[rs] | uimmediate);
        break;
    case 0x0E:
        set(rt, r[rs] ^ uimmediate);
        break;
    case 0x0F:
        set(rt, uimmediate << 16);
        break;
    case 0x10:
        // COP0: mfc0 reads 0 (the status register's bits the modules test are the interrupts', which the kernel's calls handle),
        // mtc0 and rfe do nothing
        if (rs == 0)
        {
            set(rt, 0);
        }

        break;
    case 0x20:
        set(rt, static_cast<u32>(static_cast<s8>(Load<u8>(r[rs] + static_cast<u32>(immediate)))));
        break;
    case 0x21:
        set(rt, static_cast<u32>(static_cast<s16>(Load<u16>(r[rs] + static_cast<u32>(immediate)))));
        break;
    case 0x22:
    {
        u32 address = r[rs] + static_cast<u32>(immediate);
        u32 word = Load<u32>(address & ~3u);
        u32 shift = (address & 3) * 8;
        u32 mask = 0x00FFFFFFu >> shift;
        set(rt, (r[rt] & mask) | (word << (24 - shift)));
        break;
    }
    case 0x23:
        set(rt, Load<u32>(r[rs] + static_cast<u32>(immediate)));
        break;
    case 0x24:
        set(rt, Load<u8>(r[rs] + static_cast<u32>(immediate)));
        break;
    case 0x25:
        set(rt, Load<u16>(r[rs] + static_cast<u32>(immediate)));
        break;
    case 0x26:
    {
        u32 address = r[rs] + static_cast<u32>(immediate);
        u32 word = Load<u32>(address & ~3u);
        u32 shift = (address & 3) * 8;
        u32 mask = shift == 0 ? 0 : 0xFFFFFFFFu << (32 - shift);
        set(rt, (r[rt] & mask) | (word >> shift));
        break;
    }
    case 0x28:
        Store<u8>(r[rs] + static_cast<u32>(immediate), static_cast<u8>(r[rt]));
        break;
    case 0x29:
        Store<u16>(r[rs] + static_cast<u32>(immediate), static_cast<u16>(r[rt]));
        break;
    case 0x2A:
    {
        u32 address = r[rs] + static_cast<u32>(immediate);
        u32 aligned = address & ~3u;
        u32 word = Load<u32>(aligned);
        u32 shift = (address & 3) * 8;
        u32 mask = shift == 24 ? 0 : 0xFFFFFFFFu << (shift + 8);
        Store<u32>(aligned, (word & mask) | (r[rt] >> (24 - shift)));
        break;
    }
    case 0x2B:
        Store<u32>(r[rs] + static_cast<u32>(immediate), r[rt]);
        break;
    case 0x2E:
    {
        u32 address = r[rs] + static_cast<u32>(immediate);
        u32 aligned = address & ~3u;
        u32 word = Load<u32>(aligned);
        u32 shift = (address & 3) * 8;
        u32 mask = (1u << shift) - 1;
        if (shift == 0)
        {
            mask = 0;
        }

        Store<u32>(aligned, (word & mask) | (r[rt] << shift));
        break;
    }
    default:
        Native::Log("iop: unknown instruction %08x at %#x", instruction, pc);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------
// The DMA controller's registers: a CHCR write with the start bit starts the SPU2's transfer
void StartDma(s32 core)
{
    u32 base = core == 0 ? Dma4Base : Dma7Base;
    u32 madr = g_Hardware[(base - 0x1F801000) / 4] & 0xFFFFFF;
    u32 bcr = g_Hardware[(base + 4 - 0x1F801000) / 4];
    u32 chcr = g_Hardware[(base + 8 - 0x1F801000) / 4];
    // Sync mode 1: blocks of BCR's low half words, its high half of them
    u32 words = ((chcr >> 9) & 3) == 1 ? (bcr & 0xFFFF) * (bcr >> 16) : (bcr & 0xFFFF);
    if (words == 0)
    {
        words = 0x10000;
    }

    u32 halfwords = words * 2;
    auto* data = reinterpret_cast<u16*>(RamAt(madr));
    if (g_Debug)
    {
        Native::Log("iop: DMA %d %s %#x, %u halfwords", core == 0 ? 4 : 7, (chcr & 1) != 0 ? "from" : "to", madr, halfwords);
    }

    DmaChannel& dma = g_Dma[core];
    dma.busy = true;
    if ((chcr & 1) != 0)
    {
        u32 cycles = Spu2::DmaWrite(core, data, halfwords);
        dma.automatic = cycles == 0;
        dma.start = madr;
        dma.halfwords = halfwords;
        dma.doneAt = g_Cycles + cycles;
    }
    else
    {
        dma.automatic = false;
        dma.doneAt = g_Cycles + Spu2::DmaRead(core, data, halfwords);
    }
}

void WriteHardware32(u32 physical, u32 value)
{
    u32 index = (physical - 0x1F801000) / 4;
    g_Hardware[index] = value;
    if ((physical == Dma4Base + 8 || physical == Dma7Base + 8) && (value & ChcrStart) != 0)
    {
        StartDma(physical == Dma4Base + 8 ? 0 : 1);
    }
}

// A register's word: an AutoDMA channel's MADR where the core has taken its data to
u32 MadrNow(u32 physical)
{
    for (s32 core = 0; core < 2; core++)
    {
        const DmaChannel& dma = g_Dma[core];
        if (physical == (core == 0 ? Dma4Base : Dma7Base) && dma.busy && dma.automatic)
        {
            return dma.start + (dma.halfwords - Spu2::AdmaRemaining(core)) * 2;
        }
    }

    return g_Hardware[(physical - 0x1F801000) / 4];
}

// The SPU2's interrupts: its own line, and its DMA channels' (an AutoDMA transfer ends when the core has taken its data)
void SpuInterrupt(s32 line)
{
    if (line == Spu2::Dma4Interrupt || line == Spu2::Dma7Interrupt)
    {
        s32 core = line == Spu2::Dma4Interrupt ? 0 : 1;
        g_Dma[core].busy = false;
        g_Dma[core].automatic = false;
        u32 chcr = (core == 0 ? Dma4Base : Dma7Base) + 8;
        g_Hardware[(chcr - 0x1F801000) / 4] &= ~ChcrStart;
    }

    Raise(line);
}

// ---------------------------------------------------------------------------------------------------------------------------
// The drive
void FinishCdRead()
{
    g_Cd.busy = false;
    std::vector<u8> sectors(static_cast<size_t>(g_Cd.sectors) * 2048);
    u32 read = Native::Disc::Read(static_cast<u64>(g_Cd.sector) * 2048, sectors.data(), static_cast<u32>(sectors.size()));
    if (read != sectors.size())
    {
        Native::Log("iop: the drive read %u of %zu bytes at sector %u", read, sectors.size(), g_Cd.sector);
    }

    for (size_t i = 0; i < sectors.size(); i++)
    {
        Store<u8>(g_Cd.buffer + static_cast<u32>(i), sectors[i]);
    }

    for (auto& [id, thread] : g_Threads)
    {
        if (thread.state == State::Waiting && thread.wait == Wait::CdSync)
        {
            Release(thread, 0);
        }
    }

    // The callback's reason: SCECdFuncRead
    if (g_CdCallback != 0)
    {
        RunNested(g_CdCallback, g_CdCallbackGp, 1);
    }

    g_Reschedule = true;
}

// ---------------------------------------------------------------------------------------------------------------------------
// The kernel's libraries
u32 Argument(Context& c, u32 index)
{
    if (index < 4)
    {
        return c.r[A0 + index];
    }

    return Load<u32>(c.r[Sp] + 16 + (index - 4) * 4);
}

void Return(Context& c, u32 value)
{
    c.r[V0] = value;
}

std::string Format(Context& c, u32 formatAddress, u32 firstArgument)
{
    std::string format = ReadString(formatAddress);
    std::string out;
    u32 argument = firstArgument;
    for (size_t i = 0; i < format.size(); i++)
    {
        if (format[i] != '%')
        {
            out += format[i];
            continue;
        }

        std::string spec = "%";
        i++;
        while (i < format.size() && std::strchr("-+ #0123456789.l", format[i]) != nullptr)
        {
            if (format[i] != 'l')
            {
                spec += format[i];
            }

            i++;
        }

        if (i >= format.size())
        {
            break;
        }

        char conversion = format[i];
        char buffer[256];
        if (conversion == '%')
        {
            out += '%';
            continue;
        }

        u32 value = Argument(c, argument++);
        if (conversion == 's')
        {
            spec += 's';
            std::snprintf(buffer, sizeof(buffer), spec.c_str(), ReadString(value).c_str());
        }
        else if (conversion == 'c')
        {
            spec += 'c';
            std::snprintf(buffer, sizeof(buffer), spec.c_str(), static_cast<int>(value));
        }
        else if (conversion == 'd' || conversion == 'i')
        {
            spec += 'd';
            std::snprintf(buffer, sizeof(buffer), spec.c_str(), static_cast<s32>(value));
        }
        else
        {
            spec += conversion == 'p' ? 'x' : conversion;
            std::snprintf(buffer, sizeof(buffer), spec.c_str(), value);
        }

        out += buffer;
    }

    return out;
}

void Print(const std::string& text)
{
    std::string line = text;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
    {
        line.pop_back();
    }

    if (!line.empty())
    {
        Native::Log("iop: %s", line.c_str());
    }
}

// A kernel function of a library by its number
using HleFunction = void (*)(Context& c);
struct HleEntry
{
    const char* library;
    u32 number;
    const char* name;
    HleFunction function;
};

// thbase
void CreateThreadHle(Context& c)
{
    u32 parameters = c.r[A0];
    Thread thread;
    thread.id = g_NextId++;
    thread.attr = Load<u32>(parameters);
    thread.option = Load<u32>(parameters + 4);
    thread.entry = Load<u32>(parameters + 8);
    thread.stackSize = Load<u32>(parameters + 12);
    thread.initialPriority = static_cast<s32>(Load<u32>(parameters + 16));
    thread.priority = thread.initialPriority;
    thread.stack = Allocate(1, thread.stackSize, 0);
    // The creator's $gp is the thread's
    thread.gp = c.r[Gp];
    if (thread.stack == 0)
    {
        Return(c, static_cast<u32>(-400));
        return;
    }

    std::memset(RamAt(thread.stack), 0, thread.stackSize);
    s32 id = thread.id;
    g_Threads[id] = thread;
    Return(c, static_cast<u32>(id));
}

void StartThreadHle(Context& c)
{
    Thread* thread = FindThread(static_cast<s32>(c.r[A0]));
    if (thread == nullptr)
    {
        Return(c, static_cast<u32>(-407));
        return;
    }

    std::memset(&thread->context, 0, sizeof(Context));
    thread->context.r[A0] = c.r[A1];
    thread->context.r[Gp] = thread->gp;
    thread->context.r[Sp] = (thread->stack + thread->stackSize - 0x10) & ~0xFu;
    thread->context.r[Ra] = ThreadExit;
    thread->context.pc = thread->entry;
    thread->context.npc = thread->entry + 4;
    thread->priority = thread->initialPriority;
    thread->wakeups = 0;
    Return(c, 0);
    MakeReady(*thread);
}

void DeleteThreadHle(Context& c)
{
    auto found = g_Threads.find(static_cast<s32>(c.r[A0]));
    if (found != g_Threads.end() && &found->second != g_Current)
    {
        Free(found->second.stack);
        g_Threads.erase(found);
    }

    Return(c, 0);
}

void TerminateThreadHle(Context& c)
{
    Thread* thread = FindThread(static_cast<s32>(c.r[A0]));
    if (thread != nullptr && thread != g_Current)
    {
        thread->state = State::Dormant;
        thread->wait = Wait::None;
    }

    Return(c, 0);
}

void ChangeThreadPriorityHle(Context& c)
{
    Thread* thread = FindThread(static_cast<s32>(c.r[A0]));
    if (thread == nullptr)
    {
        Return(c, static_cast<u32>(-407));
        return;
    }

    thread->priority = static_cast<s32>(c.r[A1]);
    g_Reschedule = true;
    Return(c, 0);
}

void GetThreadIdHle(Context& c)
{
    Return(c, g_Current != nullptr ? static_cast<u32>(g_Current->id) : static_cast<u32>(-100));
}

void ReferThreadStatusHle(Context& c)
{
    Thread* thread = FindThread(static_cast<s32>(c.r[A0]));
    u32 info = c.r[A1];
    if (thread == nullptr)
    {
        Return(c, static_cast<u32>(-407));
        return;
    }

    // iop_thread_info_t: attr, option, status, entry, stack, stackSize, gpReg, initPriority, currentPriority, waitType,
    // waitId, wakeupCount
    u32 status = thread->state == State::Dormant ? 0x10 : thread->state == State::Waiting ? 0x04 : (thread == g_Current ? 1 : 2);
    u32 values[12] = {thread->attr, thread->option, status, thread->entry, thread->stack, thread->stackSize, thread->gp,
                      static_cast<u32>(thread->initialPriority), static_cast<u32>(thread->priority), 0, 0,
                      static_cast<u32>(thread->wakeups)};
    for (u32 i = 0; i < 12; i++)
    {
        Store<u32>(info + i * 4, values[i]);
    }

    Return(c, 0);
}

void SleepThreadHle(Context& c)
{
    Return(c, 0);
    if (g_Current->wakeups > 0)
    {
        g_Current->wakeups--;
        return;
    }

    Block(Wait::Sleep);
}

void WakeupThreadHle(Context& c)
{
    Thread* thread = FindThread(static_cast<s32>(c.r[A0]));
    Return(c, 0);
    if (thread == nullptr)
    {
        return;
    }

    if (thread->state == State::Waiting && thread->wait == Wait::Sleep)
    {
        Release(*thread, 0);
    }
    else
    {
        thread->wakeups++;
    }
}

void DelayThreadHle(Context& c)
{
    u64 cycles = static_cast<u64>(c.r[A0]) * Iop::ClockRate / 1000000;
    Return(c, 0);
    g_Current->wakeAt = g_Cycles + cycles;
    Block(Wait::Delay);
}

void USec2SysClockHle(Context& c)
{
    u64 clock = static_cast<u64>(c.r[A0]) * Iop::ClockRate / 1000000;
    Store<u32>(c.r[A1], static_cast<u32>(clock));
    Store<u32>(c.r[A1] + 4, static_cast<u32>(clock >> 32));
    Return(c, 0);
}

// thsemap
void CreateSemaHle(Context& c)
{
    u32 parameters = c.r[A0];
    Semaphore semaphore;
    semaphore.attr = Load<u32>(parameters);
    semaphore.option = Load<u32>(parameters + 4);
    semaphore.count = static_cast<s32>(Load<u32>(parameters + 8));
    semaphore.max = static_cast<s32>(Load<u32>(parameters + 12));
    s32 id = g_NextId++;
    g_Semaphores[id] = semaphore;
    Return(c, static_cast<u32>(id));
}

void DeleteSemaHle(Context& c)
{
    auto found = g_Semaphores.find(static_cast<s32>(c.r[A0]));
    if (found == g_Semaphores.end())
    {
        Return(c, static_cast<u32>(-408));
        return;
    }

    for (s32 id : found->second.waiting)
    {
        if (Thread* thread = FindThread(id))
        {
            Release(*thread, static_cast<u32>(-425));
        }
    }

    g_Semaphores.erase(found);
    Return(c, 0);
}

void SignalSemaHle(Context& c)
{
    auto found = g_Semaphores.find(static_cast<s32>(c.r[A0]));
    if (found == g_Semaphores.end())
    {
        Return(c, static_cast<u32>(-408));
        return;
    }

    Semaphore& semaphore = found->second;
    Return(c, 0);
    while (!semaphore.waiting.empty())
    {
        s32 id = semaphore.waiting.front();
        semaphore.waiting.pop_front();
        Thread* thread = FindThread(id);
        if (thread != nullptr && thread->state == State::Waiting && thread->wait == Wait::Semaphore)
        {
            Release(*thread, 0);
            return;
        }
    }

    if (semaphore.count >= semaphore.max)
    {
        Return(c, static_cast<u32>(-420));
        return;
    }

    semaphore.count++;
}

void WaitSemaHle(Context& c)
{
    s32 id = static_cast<s32>(c.r[A0]);
    auto found = g_Semaphores.find(id);
    if (found == g_Semaphores.end())
    {
        Return(c, static_cast<u32>(-408));
        return;
    }

    Return(c, 0);
    if (found->second.count > 0)
    {
        found->second.count--;
        return;
    }

    found->second.waiting.push_back(g_Current->id);
    Block(Wait::Semaphore, id);
}

void PollSemaHle(Context& c)
{
    auto found = g_Semaphores.find(static_cast<s32>(c.r[A0]));
    if (found == g_Semaphores.end() || found->second.count <= 0)
    {
        Return(c, static_cast<u32>(-419));
        return;
    }

    found->second.count--;
    Return(c, 0);
}

// thevent
constexpr u32 WaitOr = 1;
constexpr u32 WaitClear = 0x10;

bool EventSatisfied(u32 bits, u32 wanted, u32 mode)
{
    return (mode & WaitOr) != 0 ? (bits & wanted) != 0 : (bits & wanted) == wanted;
}

void WakeEventWaiters(s32 id)
{
    EventFlag& flag = g_EventFlags[id];
    for (auto& [threadId, thread] : g_Threads)
    {
        if (thread.state == State::Waiting && thread.wait == Wait::EventFlag && thread.waitId == id &&
            EventSatisfied(flag.bits, thread.waitBits, thread.waitMode))
        {
            if (thread.waitResult != 0)
            {
                Store<u32>(thread.waitResult, flag.bits);
            }

            if ((thread.waitMode & WaitClear) != 0)
            {
                flag.bits = 0;
            }

            Release(thread, 0);
        }
    }
}

void CreateEventFlagHle(Context& c)
{
    u32 parameters = c.r[A0];
    EventFlag flag;
    flag.attr = Load<u32>(parameters);
    flag.option = Load<u32>(parameters + 4);
    flag.bits = Load<u32>(parameters + 8);
    s32 id = g_NextId++;
    g_EventFlags[id] = flag;
    Return(c, static_cast<u32>(id));
}

void DeleteEventFlagHle(Context& c)
{
    g_EventFlags.erase(static_cast<s32>(c.r[A0]));
    Return(c, 0);
}

void SetEventFlagHle(Context& c)
{
    s32 id = static_cast<s32>(c.r[A0]);
    auto found = g_EventFlags.find(id);
    if (found == g_EventFlags.end())
    {
        Return(c, static_cast<u32>(-409));
        return;
    }

    found->second.bits |= c.r[A1];
    Return(c, 0);
    WakeEventWaiters(id);
}

void ClearEventFlagHle(Context& c)
{
    auto found = g_EventFlags.find(static_cast<s32>(c.r[A0]));
    if (found != g_EventFlags.end())
    {
        found->second.bits &= c.r[A1];
    }

    Return(c, 0);
}

void WaitEventFlagHle(Context& c)
{
    s32 id = static_cast<s32>(c.r[A0]);
    auto found = g_EventFlags.find(id);
    if (found == g_EventFlags.end())
    {
        Return(c, static_cast<u32>(-409));
        return;
    }

    u32 wanted = c.r[A1];
    u32 mode = c.r[A2];
    u32 result = c.r[A3];
    Return(c, 0);
    EventFlag& flag = found->second;
    if (EventSatisfied(flag.bits, wanted, mode))
    {
        if (result != 0)
        {
            Store<u32>(result, flag.bits);
        }

        if ((mode & WaitClear) != 0)
        {
            flag.bits = 0;
        }

        return;
    }

    g_Current->waitBits = wanted;
    g_Current->waitMode = mode;
    g_Current->waitResult = result;
    Block(Wait::EventFlag, id);
}

// intrman
void RegisterIntrHandlerHle(Context& c)
{
    s32 line = static_cast<s32>(c.r[A0]);
    if (line < 0 || line >= Lines)
    {
        Return(c, static_cast<u32>(-101));
        return;
    }

    g_Handlers[line].function = c.r[A2];
    g_Handlers[line].argument = c.r[A3];
    g_Handlers[line].gp = c.r[Gp];
    Return(c, 0);
}

void ReleaseIntrHandlerHle(Context& c)
{
    s32 line = static_cast<s32>(c.r[A0]);
    if (line >= 0 && line < Lines)
    {
        g_Handlers[line].function = 0;
    }

    Return(c, 0);
}

void EnableIntrHle(Context& c)
{
    s32 line = static_cast<s32>(c.r[A0]) & 0xFF;
    if (line < Lines)
    {
        g_Handlers[line].enabled = true;
    }

    Return(c, 0);
}

void DisableIntrHle(Context& c)
{
    s32 line = static_cast<s32>(c.r[A0]) & 0xFF;
    if (line < Lines)
    {
        g_Handlers[line].enabled = false;
    }

    if (c.r[A1] != 0)
    {
        Store<u32>(c.r[A1], static_cast<u32>(line));
    }

    Return(c, 0);
}

void CpuSuspendIntrHle(Context& c)
{
    bool was = g_InterruptsEnabled;
    if (c.r[A0] != 0)
    {
        Store<u32>(c.r[A0], was ? 1 : 0);
    }

    g_InterruptsEnabled = false;
    Return(c, was ? 0 : static_cast<u32>(-102));
}

void CpuResumeIntrHle(Context& c)
{
    g_InterruptsEnabled = c.r[A0] != 0;
    if (g_InterruptsEnabled && g_Pending != 0)
    {
        g_Reschedule = true;
    }

    Return(c, 0);
}

void QueryIntrContextHle(Context& c)
{
    Return(c, g_InInterrupt ? 1 : 0);
}

// timrman
void AllocHardTimerHle(Context& c)
{
    for (s32 i = 0; i < 4; i++)
    {
        if (!g_Timers[i].allocated)
        {
            g_Timers[i] = HardTimer{};
            g_Timers[i].allocated = true;
            Return(c, static_cast<u32>(TimerIdBase + i));
            return;
        }
    }

    Return(c, static_cast<u32>(-150));
}

HardTimer* TimerOf(u32 id)
{
    s32 index = static_cast<s32>(id) - TimerIdBase;
    return index >= 0 && index < 4 && g_Timers[index].allocated ? &g_Timers[index] : nullptr;
}

void FreeHardTimerHle(Context& c)
{
    if (HardTimer* timer = TimerOf(c.r[A0]))
    {
        *timer = HardTimer{};
    }

    Return(c, 0);
}

void SetTimerHandlerHle(Context& c)
{
    HardTimer* timer = TimerOf(c.r[A0]);
    if (timer == nullptr)
    {
        Return(c, static_cast<u32>(-151));
        return;
    }

    timer->compare = c.r[A1];
    timer->handler = c.r[A2];
    timer->argument = c.r[A3];
    timer->gp = c.r[Gp];
    Return(c, 0);
}

void SetupHardTimerHle(Context& c)
{
    Return(c, TimerOf(c.r[A0]) != nullptr ? 0 : static_cast<u32>(-151));
}

void StartHardTimerHle(Context& c)
{
    HardTimer* timer = TimerOf(c.r[A0]);
    if (timer == nullptr)
    {
        Return(c, static_cast<u32>(-151));
        return;
    }

    timer->started = true;
    timer->fireAt = g_Cycles + timer->compare;
    Return(c, 0);
}

void StopHardTimerHle(Context& c)
{
    if (HardTimer* timer = TimerOf(c.r[A0]))
    {
        timer->started = false;
    }

    Return(c, 0);
}

// sysmem
void AllocSysMemoryHle(Context& c)
{
    Return(c, Allocate(c.r[A0], c.r[A1], c.r[A2]));
}

void FreeSysMemoryHle(Context& c)
{
    Free(c.r[A0]);
    Return(c, 0);
}

void QueryMaxFreeMemSizeHle(Context& c)
{
    Return(c, LargestFree());
}

void QueryMemSizeHle(Context& c)
{
    Return(c, Iop::RamSize);
}

void KprintfHle(Context& c)
{
    Print(Format(c, c.r[A0], 1));
    Return(c, 0);
}

// stdio
void PrintfHle(Context& c)
{
    Print(Format(c, c.r[A0], 1));
    Return(c, 0);
}

// sysclib
void MemcpyHle(Context& c)
{
    u32 destination = c.r[A0];
    u32 source = c.r[A1];
    u32 size = c.r[A2];
    std::memmove(RamAt(destination), RamAt(source), size);
    Return(c, destination);
}

void MemsetHle(Context& c)
{
    std::memset(RamAt(c.r[A0]), static_cast<int>(c.r[A1] & 0xFF), c.r[A2]);
    Return(c, c.r[A0]);
}

void BzeroHle(Context& c)
{
    std::memset(RamAt(c.r[A0]), 0, c.r[A1]);
    Return(c, 0);
}

void StrcpyHle(Context& c)
{
    std::string source = ReadString(c.r[A1]);
    std::memcpy(RamAt(c.r[A0]), source.c_str(), source.size() + 1);
    Return(c, c.r[A0]);
}

void StrcatHle(Context& c)
{
    std::string destination = ReadString(c.r[A0]);
    std::string source = ReadString(c.r[A1]);
    std::memcpy(RamAt(c.r[A0] + static_cast<u32>(destination.size())), source.c_str(), source.size() + 1);
    Return(c, c.r[A0]);
}

void StrlenHle(Context& c)
{
    Return(c, static_cast<u32>(ReadString(c.r[A0]).size()));
}

void StrncmpHle(Context& c)
{
    u32 a = c.r[A0];
    u32 b = c.r[A1];
    u32 n = c.r[A2];
    s32 result = 0;
    for (u32 i = 0; i < n; i++)
    {
        u8 x = Load<u8>(a + i);
        u8 y = Load<u8>(b + i);
        if (x != y)
        {
            result = x - y;
            break;
        }

        if (x == 0)
        {
            break;
        }
    }

    Return(c, static_cast<u32>(result));
}

void StrstrHle(Context& c)
{
    std::string haystack = ReadString(c.r[A0]);
    std::string needle = ReadString(c.r[A1]);
    size_t found = haystack.find(needle);
    Return(c, found == std::string::npos ? 0 : c.r[A0] + static_cast<u32>(found));
}

void StrtolHle(Context& c)
{
    std::string text = ReadString(c.r[A0]);
    char* end = nullptr;
    long value = std::strtol(text.c_str(), &end, static_cast<int>(c.r[A2]));
    if (c.r[A1] != 0)
    {
        Store<u32>(c.r[A1], c.r[A0] + static_cast<u32>(end - text.c_str()));
    }

    Return(c, static_cast<u32>(value));
}

// The C library's character table (look_ctype_table): the type bits of a character, as PS2SDK's sysclib has them
void LookCtypeTableHle(Context& c)
{
    u32 ch = c.r[A0] & 0xFF;
    u32 bits = 0;
    if (ch >= 'A' && ch <= 'Z')
    {
        bits |= 0x01;
    }

    if (ch >= 'a' && ch <= 'z')
    {
        bits |= 0x02;
    }

    if (ch >= '0' && ch <= '9')
    {
        bits |= 0x04;
    }

    if (ch == ' ' || (ch >= 9 && ch <= 13))
    {
        bits |= 0x08;
    }

    if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))
    {
        bits |= 0x40;
    }

    Return(c, bits);
}

// loadcore
struct Library
{
    std::string name;
    u32 table;
};
std::vector<Library> g_Libraries;

void RegisterLibraryEntriesHle(Context& c)
{
    u32 table = c.r[A0];
    char name[9] = {};
    for (u32 i = 0; i < 8; i++)
    {
        name[i] = static_cast<char>(Load<u8>(table + 12 + i));
    }

    g_Libraries.push_back({name, table});
    Return(c, 0);
}

void FlushDcacheHle(Context& c)
{
    Return(c, 0);
}

// sifman: the IOP's own SIF DMA to the EE's memory, which the native build has no use for (the EE's loads through MultiStream
// aren't made natively)
void SifInitHle(Context& c)
{
    Return(c, 0);
}

void SifCheckInitHle(Context& c)
{
    Return(c, 1);
}

Iop::EeWriter g_EeWriter = nullptr;

// The transfers (SifDmaTransfer_t: the IOP's source, the EE's destination, the size, the attributes) are made at once
void SifSetDmaHle(Context& c)
{
    u32 transfers = c.r[A0];
    u32 count = c.r[A1];
    for (u32 i = 0; i < count; i++)
    {
        u32 source = Load<u32>(transfers + i * 16);
        u32 destination = Load<u32>(transfers + i * 16 + 4);
        u32 size = Load<u32>(transfers + i * 16 + 8);
        if (g_Debug)
        {
            Native::Log("iop: SIF DMA %#x -> EE %#x, %u bytes", source, destination, size);
        }

        if (g_EeWriter != nullptr)
        {
            g_EeWriter(destination, RamAt(source), size);
        }
    }

    Return(c, 1);
}

void SifDmaStatHle(Context& c)
{
    Return(c, static_cast<u32>(-1));
}

// sifcmd: the RPC servers the modules register; their calls come from the EE (Iop::Post)
void SifInitRpcHle(Context& c)
{
    Return(c, 0);
}

void SifSetRpcQueueHle(Context& c)
{
    g_Queues[c.r[A0]] = static_cast<s32>(c.r[A1]);
    Return(c, 0);
}

void SifRegisterRpcHle(Context& c)
{
    g_Servers.push_back({static_cast<s32>(c.r[A1]), c.r[A0], c.r[A2], c.r[A3], Argument(c, 6)});
    Return(c, 0);
}

void SifRpcLoopHle(Context& c)
{
    Return(c, 0);
    Block(Wait::RpcLoop);
}

void SifCheckStatRpcHle(Context& c)
{
    Return(c, 0);
}

// The modules' calls of the EE's servers (MultiStream's fast load): the native build's own (Iop::SetEeServer). A client's
// record (SifRpcClientData_t) is bound when its server word (at 0x24) isn't 0: the server's ID goes there
std::unordered_map<s32, Iop::EeServer> g_EeServers;
constexpr u32 ClientServer = 0x24;

void SifBindRpcHle(Context& c)
{
    u32 client = c.r[A0];
    s32 id = static_cast<s32>(c.r[A1]);
    Store<u32>(client + ClientServer, g_EeServers.contains(id) ? static_cast<u32>(id) : 0);
    Return(c, 0);
}

// sceSifCallRpc(client, function, mode, send, ssize, receive, rsize, end, endParameter): made at once, waiting or not
void SifCallRpcHle(Context& c)
{
    u32 client = c.r[A0];
    s32 id = static_cast<s32>(Load<u32>(client + ClientServer));
    auto found = g_EeServers.find(id);
    if (found == g_EeServers.end())
    {
        Return(c, static_cast<u32>(-1));
        return;
    }

    u32 send = Argument(c, 3);
    u32 sendSize = Argument(c, 4);
    u32 receive = Argument(c, 5);
    u32 receiveSize = Argument(c, 6);
    std::vector<u8> data(RamAt(send), RamAt(send) + sendSize);
    const void* reply = found->second(c.r[A1], data.data(), sendSize);
    if (reply != nullptr && receive != 0)
    {
        std::memcpy(RamAt(receive), reply, receiveSize);
    }

    Return(c, 0);
}

// ioman: the host's files ("host0:", "atfile:"), which the game doesn't use
void IoOpenHle(Context& c)
{
    Native::Log("iop: open %s (no host files)", ReadString(c.r[A0]).c_str());
    Return(c, static_cast<u32>(-1));
}

void IoFailHle(Context& c)
{
    Return(c, static_cast<u32>(-1));
}

// cdvdman
void CdInitHle(Context& c)
{
    Return(c, 1);
}

void CdReadHle(Context& c)
{
    if (g_Cd.busy)
    {
        Return(c, 0);
        return;
    }

    g_Cd.busy = true;
    g_Cd.sector = c.r[A0];
    g_Cd.sectors = c.r[A1];
    g_Cd.buffer = c.r[A2];
    // The drive's time for the read: a 4x DVD's, or with fast loading (NativeFastDisc, the options' "fast loading") as good as
    // none, the read being from the computer's own storage (PCSX2's "fast CDVD")
    g_Cd.doneAt = NativeFastDisc() ? g_Cycles + FastCdCycles
                                   : g_Cycles + CdSeekCycles + static_cast<u64>(g_Cd.sectors) * 2048 * Iop::ClockRate / CdBytesPerSecond;
    if (g_Debug)
    {
        Native::Log("iop: sceCdRead sector %u, %u sectors to %#x", g_Cd.sector, g_Cd.sectors, g_Cd.buffer);
    }

    Return(c, 1);
}

void CdSyncHle(Context& c)
{
    // Mode 1 asks without waiting: 1 while busy
    if ((c.r[A0] & 1) != 0)
    {
        Return(c, g_Cd.busy ? 1 : 0);
        return;
    }

    Return(c, 0);
    if (g_Cd.busy)
    {
        Block(Wait::CdSync);
    }
}

void CdGetErrorHle(Context& c)
{
    Return(c, 0);
}

void CdSearchFileHle(Context& c)
{
    u32 file = c.r[A0];
    std::string path = ReadString(c.r[A1]);
    Native::Disc::Entry entry;
    if (!Native::Disc::Find(path.c_str(), &entry))
    {
        Native::Log("iop: no %s on the disc", path.c_str());
        Return(c, 0);
        return;
    }

    if (g_Debug)
    {
        Native::Log("iop: sceCdSearchFile %s: sector %llu, %u bytes", path.c_str(), static_cast<unsigned long long>(entry.offset / 2048),
                    entry.size);
    }

    // sceCdlFILE: the sector, the size, the name and the date
    Store<u32>(file, static_cast<u32>(entry.offset / 2048));
    Store<u32>(file + 4, entry.size);
    size_t slash = path.find_last_of('\\');
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    for (u32 i = 0; i < 16; i++)
    {
        Store<u8>(file + 8 + i, i < name.size() ? static_cast<u8>(name[i]) : 0);
    }

    Return(c, 1);
}

void CdGetDiskTypeHle(Context& c)
{
    // SCECdPS2DVD
    Return(c, 0x14);
}

void CdDiskReadyHle(Context& c)
{
    // SCECdComplete
    Return(c, 2);
}

void CdCallbackHle(Context& c)
{
    if (g_Cd.busy)
    {
        Return(c, 0);
        return;
    }

    u32 previous = g_CdCallback;
    g_CdCallback = c.r[A0];
    g_CdCallbackGp = c.r[Gp];
    Return(c, previous != 0 ? previous : 1);
}

void CdMmodeHle(Context& c)
{
    Return(c, 1);
}

void UnknownHle(Context& c)
{
    Return(c, 0);
}

const HleEntry g_Hle[] = {
    {"thbase", 4, "CreateThread", CreateThreadHle},
    {"thbase", 5, "DeleteThread", DeleteThreadHle},
    {"thbase", 6, "StartThread", StartThreadHle},
    {"thbase", 10, "TerminateThread", TerminateThreadHle},
    {"thbase", 14, "ChangeThreadPriority", ChangeThreadPriorityHle},
    {"thbase", 20, "GetThreadId", GetThreadIdHle},
    {"thbase", 22, "ReferThreadStatus", ReferThreadStatusHle},
    {"thbase", 24, "SleepThread", SleepThreadHle},
    {"thbase", 25, "WakeupThread", WakeupThreadHle},
    {"thbase", 26, "iWakeupThread", WakeupThreadHle},
    {"thbase", 33, "DelayThread", DelayThreadHle},
    {"thbase", 39, "USec2SysClock", USec2SysClockHle},
    {"thsemap", 4, "CreateSema", CreateSemaHle},
    {"thsemap", 5, "DeleteSema", DeleteSemaHle},
    {"thsemap", 6, "SignalSema", SignalSemaHle},
    {"thsemap", 7, "iSignalSema", SignalSemaHle},
    {"thsemap", 8, "WaitSema", WaitSemaHle},
    {"thsemap", 9, "PollSema", PollSemaHle},
    {"thevent", 4, "CreateEventFlag", CreateEventFlagHle},
    {"thevent", 5, "DeleteEventFlag", DeleteEventFlagHle},
    {"thevent", 6, "SetEventFlag", SetEventFlagHle},
    {"thevent", 7, "iSetEventFlag", SetEventFlagHle},
    {"thevent", 8, "ClearEventFlag", ClearEventFlagHle},
    {"thevent", 9, "iClearEventFlag", ClearEventFlagHle},
    {"thevent", 10, "WaitEventFlag", WaitEventFlagHle},
    {"intrman", 4, "RegisterIntrHandler", RegisterIntrHandlerHle},
    {"intrman", 5, "ReleaseIntrHandler", ReleaseIntrHandlerHle},
    {"intrman", 6, "EnableIntr", EnableIntrHle},
    {"intrman", 7, "DisableIntr", DisableIntrHle},
    {"intrman", 17, "CpuSuspendIntr", CpuSuspendIntrHle},
    {"intrman", 18, "CpuResumeIntr", CpuResumeIntrHle},
    {"intrman", 23, "QueryIntrContext", QueryIntrContextHle},
    {"timrman", 4, "AllocHardTimer", AllocHardTimerHle},
    {"timrman", 6, "FreeHardTimer", FreeHardTimerHle},
    {"timrman", 20, "SetTimerHandler", SetTimerHandlerHle},
    {"timrman", 22, "SetupHardTimer", SetupHardTimerHle},
    {"timrman", 23, "StartHardTimer", StartHardTimerHle},
    {"timrman", 24, "StopHardTimer", StopHardTimerHle},
    {"sysmem", 4, "AllocSysMemory", AllocSysMemoryHle},
    {"sysmem", 5, "FreeSysMemory", FreeSysMemoryHle},
    {"sysmem", 6, "QueryMemSize", QueryMemSizeHle},
    {"sysmem", 7, "QueryMaxFreeMemSize", QueryMaxFreeMemSizeHle},
    {"sysmem", 14, "Kprintf", KprintfHle},
    {"stdio", 4, "printf", PrintfHle},
    {"sysclib", 8, "look_ctype_table", LookCtypeTableHle},
    {"sysclib", 12, "memcpy", MemcpyHle},
    {"sysclib", 14, "memset", MemsetHle},
    {"sysclib", 17, "bzero", BzeroHle},
    {"sysclib", 20, "strcat", StrcatHle},
    {"sysclib", 23, "strcpy", StrcpyHle},
    {"sysclib", 27, "strlen", StrlenHle},
    {"sysclib", 29, "strncmp", StrncmpHle},
    {"sysclib", 34, "strstr", StrstrHle},
    {"sysclib", 36, "strtol", StrtolHle},
    {"loadcore", 5, "FlushDcache", FlushDcacheHle},
    {"loadcore", 6, "RegisterLibraryEntries", RegisterLibraryEntriesHle},
    {"sifman", 5, "sceSifInit", SifInitHle},
    {"sifman", 7, "sceSifSetDma", SifSetDmaHle},
    {"sifman", 8, "sceSifDmaStat", SifDmaStatHle},
    {"sifman", 29, "sceSifCheckInit", SifCheckInitHle},
    {"sifcmd", 14, "sceSifInitRpc", SifInitRpcHle},
    {"sifcmd", 15, "sceSifBindRpc", SifBindRpcHle},
    {"sifcmd", 16, "sceSifCallRpc", SifCallRpcHle},
    {"sifcmd", 17, "sceSifRegisterRpc", SifRegisterRpcHle},
    {"sifcmd", 18, "sceSifCheckStatRpc", SifCheckStatRpcHle},
    {"sifcmd", 19, "sceSifSetRpcQueue", SifSetRpcQueueHle},
    {"sifcmd", 22, "sceSifRpcLoop", SifRpcLoopHle},
    {"ioman", 4, "open", IoOpenHle},
    {"ioman", 5, "close", IoFailHle},
    {"ioman", 6, "read", IoFailHle},
    {"ioman", 8, "lseek", IoFailHle},
    {"cdvdman", 4, "sceCdInit", CdInitHle},
    {"cdvdman", 6, "sceCdRead", CdReadHle},
    {"cdvdman", 8, "sceCdGetError", CdGetErrorHle},
    {"cdvdman", 10, "sceCdSearchFile", CdSearchFileHle},
    {"cdvdman", 11, "sceCdSync", CdSyncHle},
    {"cdvdman", 12, "sceCdGetDiskType", CdGetDiskTypeHle},
    {"cdvdman", 13, "sceCdDiskReady", CdDiskReadyHle},
    {"cdvdman", 37, "sceCdCallback", CdCallbackHle},
    {"cdvdman", 75, "sceCdMmode", CdMmodeHle},
};
constexpr u32 HleCount = sizeof(g_Hle) / sizeof(g_Hle[0]);

// The imports with no kernel function of the emulator's: logged once each
std::vector<std::string> g_UnknownImports;

void CallHle(Context& c, u32 index)
{
    // The stub returns to its caller after the call (its delay slot is the import's number, which does nothing)
    c.pc = c.r[Ra];
    c.npc = c.pc + 4;
    if (index < HleCount)
    {
        g_Hle[index].function(c);
        return;
    }

    u32 unknown = index - HleCount;
    if (unknown < g_UnknownImports.size())
    {
        Native::Log("iop: %s isn't emulated", g_UnknownImports[unknown].c_str());
    }

    UnknownHle(c);
}

// ---------------------------------------------------------------------------------------------------------------------------
// The modules: relocated where they're loaded, their imports linked to the emulator's kernel or to an earlier module's exports
struct Module
{
    const char* path;
    u32 base;
    u32 entry;
    u32 gp;
    u32 textSize;
};

u32 Read32(const std::vector<u8>& file, u32 offset)
{
    u32 value;
    std::memcpy(&value, file.data() + offset, 4);
    return value;
}

u16 Read16(const std::vector<u8>& file, u32 offset)
{
    u16 value;
    std::memcpy(&value, file.data() + offset, 2);
    return value;
}

// A retail bug of STREAM.IRX's: its allocator of the sound memory manages all of it, from its base to the top (0x1FFFFF), the
// reverbs' work areas included. Those are at the top: the first core's reverb's below 0x1FFFFF, the second's below 0x1DFFFF,
// each as large as its mode (up to 0xF6C0 bytes for the game's modes: game/sound.cpp's ReverbModeOf). With the memory full
// enough (the Italian voice banks are larger), sounds are placed there, and the module loses them: it skips the transfers that
// reach an area while its reverb is on (its own load of a sound and its compaction's reads, while it still counts the sound as
// there), it clears the area as it turns the reverb on, and the reverb writes it all the time. So the caverns' crystals were
// silent once the tunnel's reverb (Hall, the second core's) was on. The emulated SPU2 keeps the reverbs' areas in memory of
// their own (spu2.cpp), so a sound is safe anywhere and the module's guards go: its check of a transfer's range (0xCAB4) no
// longer reads the areas' sizes (it finds none), and its setting of a reverb (0xA414) no longer clears the area in the sound
// memory (the SPU2 clears its own as the reverb comes on). The allocator, and so where the sounds go, is the PS2's
constexpr u32 TransferCheckAt = 0xCAF8;
constexpr u32 TransferCheckWord = 0x8C430000;  // lw $v1, 0($v0): the core's area's size
constexpr u32 NoSize = 0x00001821;             // addu $v1, $zero, $zero
constexpr u32 AreaClearAt = 0xA564;            // jal sceSdClearEffectWorkArea (its import's stub)
constexpr u32 AreaClearStub = 0x12AFC;

void KeepReverbsApart(u32 base)
{
    u32 clear = 0x0C000000 | (((base + AreaClearStub) >> 2) & 0x3FFFFFF);
    if (Load<u32>(base + TransferCheckAt) != TransferCheckWord || Load<u32>(base + AreaClearAt) != clear)
    {
        Native::Log("iop: STREAM.IRX isn't the one known (its guards of the reverbs' areas left as they are)");
        return;
    }

    Store<u32>(base + TransferCheckAt, NoSize);
    Store<u32>(base + AreaClearAt, 0);
    Native::Log("iop: STREAM.IRX's sounds and the reverbs' areas kept apart");
}

bool LoadModule(const char* path, u32* nextBase, Module* module)
{
    Native::Disc::Entry entry;
    if (!Native::Disc::Find(path, &entry))
    {
        Native::Log("iop: no %s on the disc", path);
        return false;
    }

    std::vector<u8> file(entry.size);
    if (Native::Disc::Read(entry.offset, file.data(), entry.size) != entry.size)
    {
        return false;
    }

    // The IRX's ELF: its program header of the module's information (.iopmod: its entry, $gp, and the sizes of its text, data
    // and bss) and of its image, its sections' relocations
    u32 programHeaders = Read32(file, 0x1C);
    u16 programCount = Read16(file, 0x2C);
    u32 imageOffset = 0;
    u32 imageFileSize = 0;
    u32 imageMemorySize = 0;
    u32 iopmod = 0;
    for (u32 i = 0; i < programCount; i++)
    {
        u32 header = programHeaders + i * 32;
        u32 type = Read32(file, header);
        if (type == 0x70000080)
        {
            iopmod = Read32(file, header + 4);
        }
        else if (type == 1)
        {
            imageOffset = Read32(file, header + 4);
            imageFileSize = Read32(file, header + 16);
            imageMemorySize = Read32(file, header + 20);
        }
    }

    if (iopmod == 0 || imageMemorySize == 0)
    {
        Native::Log("iop: %s isn't an IRX", path);
        return false;
    }

    u32 base = (*nextBase + 0xFF) & ~0xFFu;
    module->path = path;
    module->base = base;
    module->entry = base + Read32(file, iopmod + 4);
    module->gp = base + Read32(file, iopmod + 8);
    module->textSize = Read32(file, iopmod + 12);
    std::memset(g_Ram + base, 0, imageMemorySize);
    std::memcpy(g_Ram + base, file.data() + imageOffset, imageFileSize);
    *nextBase = base + imageMemorySize;

    // Relocations: R_MIPS_32, R_MIPS_26, R_MIPS_HI16 (paired with the LO16 that follows) and R_MIPS_LO16
    u32 sectionHeaders = Read32(file, 0x20);
    u16 sectionCount = Read16(file, 0x30);
    for (u32 i = 0; i < sectionCount; i++)
    {
        u32 header = sectionHeaders + i * 40;
        if (Read32(file, header + 4) != 9)
        {
            continue;
        }

        u32 offset = Read32(file, header + 16);
        u32 size = Read32(file, header + 20);
        std::vector<u32> pendingHi;
        for (u32 at = 0; at < size; at += 8)
        {
            u32 where = base + Read32(file, offset + at);
            u32 type = Read32(file, offset + at + 4) & 0xFF;
            u32 word = Load<u32>(where);
            switch (type)
            {
            case 2:
                Store<u32>(where, word + base);
                break;
            case 4:
                Store<u32>(where, (word & 0xFC000000) | (((word & 0x3FFFFFF) + (base >> 2)) & 0x3FFFFFF));
                break;
            case 5:
                pendingHi.push_back(where);
                break;
            case 6:
            {
                s32 low = static_cast<s16>(word & 0xFFFF);
                for (u32 hiAddress : pendingHi)
                {
                    u32 hi = Load<u32>(hiAddress);
                    u32 value = ((hi & 0xFFFF) << 16) + static_cast<u32>(low) + base;
                    u32 high = (value + 0x8000) >> 16;
                    Store<u32>(hiAddress, (hi & 0xFFFF0000) | (high & 0xFFFF));
                }

                pendingHi.clear();
                Store<u32>(where, (word & 0xFFFF0000) | ((static_cast<u32>(low) + base) & 0xFFFF));
                break;
            }
            default:
                Native::Log("iop: %s has a relocation of type %u", path, type);
                break;
            }
        }
    }

    return true;
}

// The import tables (0x41E00000, then the library's name): each stub "jr ra; li $0, number" becomes a call of the emulator's
// kernel function, or a jump to the exporting module's function
void LinkImports(const Module& module, u32 textSize)
{
    for (u32 at = module.base; at < module.base + textSize; at += 4)
    {
        if (Load<u32>(at) != 0x41E00000)
        {
            continue;
        }

        char name[9] = {};
        for (u32 i = 0; i < 8; i++)
        {
            name[i] = static_cast<char>(Load<u8>(at + 12 + i));
        }

        for (u32 stub = at + 20; Load<u32>(stub) == 0x03E00008; stub += 8)
        {
            u32 delay = Load<u32>(stub + 4);
            if ((delay >> 16) != 0x2400)
            {
                break;
            }

            u32 number = delay & 0xFFFF;
            bool linked = false;
            for (const Library& library : g_Libraries)
            {
                if (library.name == name)
                {
                    u32 function = Load<u32>(library.table + 20 + number * 4);
                    Store<u32>(stub, 0x08000000 | ((function >> 2) & 0x3FFFFFF));
                    linked = true;
                }
            }

            for (u32 i = 0; i < HleCount && !linked; i++)
            {
                if (std::strcmp(g_Hle[i].library, name) == 0 && g_Hle[i].number == number)
                {
                    Store<u32>(stub, 0x0000000C | (i << 6));
                    linked = true;
                }
            }

            if (!linked)
            {
                char description[64];
                std::snprintf(description, sizeof(description), "%s's import %u (%s)", name, number, module.path);
                g_UnknownImports.push_back(description);
                Store<u32>(stub, 0x0000000C | ((HleCount + static_cast<u32>(g_UnknownImports.size()) - 1) << 6));
            }
        }
    }
}

// The module's start function on a thread of its own (the IOP's loader calls it with argc 0), run until it returns
bool StartModule(const Module& module)
{
    Thread thread;
    thread.id = g_NextId++;
    thread.stackSize = 0x1000;
    thread.stack = Allocate(1, thread.stackSize, 0);
    thread.gp = module.gp;
    thread.entry = module.entry;
    thread.initialPriority = 8;
    thread.priority = 8;
    thread.context = {};
    thread.context.r[Gp] = module.gp;
    thread.context.r[Sp] = thread.stack + thread.stackSize - 0x10;
    thread.context.r[Ra] = StartReturn;
    thread.context.pc = module.entry;
    thread.context.npc = module.entry + 4;
    s32 id = thread.id;
    g_Threads[id] = thread;
    MakeReady(g_Threads[id]);
    // Its threads start as it runs; the start is done once its own thread has ended
    for (u32 i = 0; i < 100000 && g_Threads[id].state != State::Dormant; i++)
    {
        Advance(g_Cycles + Spu2::CyclesPerSample);
    }

    u32 result = g_Threads[id].context.r[V0];
    Free(g_Threads[id].stack);
    g_Threads.erase(id);
    // The start's result: 0 (or 2) resident, 1 to be unloaded
    return (result & 3) != 1;
}
}

bool Iop::Boot()
{
    if (g_Booted)
    {
        return true;
    }

    std::memset(g_Ram, 0, sizeof(g_Ram));
    Spu2::Reset();
    Spu2::SetInterruptFunction(SpuInterrupt);
    // The modules load where the PS2's would be once the IOP's kernel, its IOPRP image's modules and the drivers loaded before
    // them took their place (native/AUDIO.md)
    constexpr u32 ModulesStart = 0x60000;
    u32 next = ModulesStart;
    g_HeapStart = ModulesStart;
    // In the PS2's order (Platform::System loads LIBSD, SDRDRV, STREAM)
    const char* paths[] = {"CRASH6\\SYS\\LIBSD.IRX", "CRASH6\\SYS\\SDRDRV.IRX", "CRASH6\\SYS\\STREAM.IRX"};
    for (const char* path : paths)
    {
        Module module;
        if (!LoadModule(path, &next, &module))
        {
            return false;
        }

        g_HeapStart = (next + AllocationAlignment - 1) & ~(AllocationAlignment - 1);
        if (g_Debug)
        {
            Native::Log("iop: %s at %#x (its code %#x bytes)", path, module.base, module.textSize);
        }

        LinkImports(module, module.textSize);
        if (std::strstr(path, "STREAM") != nullptr)
        {
            g_StreamBase = module.base;
            KeepReverbsApart(module.base);
        }

        if (!StartModule(module))
        {
            Native::Log("iop: %s didn't stay resident", path);
        }
    }

    g_Booted = true;
    Native::Log("iop: the sound modules run (%u KB of IOP memory free)", LargestFree() / 1024);
    return true;
}

bool Iop::Booted()
{
    return g_Booted;
}

// $TWIN_SOUND_BLOCKS: STREAM.IRX's sound memory blocks logged every 10 s of samples (its list at 0x1E460: an entry's address,
// size, next, previous and whether it's taken, 20 bytes; the list's first at 0x13404, its base at 0x12EFC)
void LogSoundBlocks()
{
    static const bool wanted = std::getenv("TWIN_SOUND_BLOCKS") != nullptr;
    static u64 next = 0;
    if (!wanted || g_StreamBase == 0 || g_SamplesMade < next)
    {
        return;
    }

    next = g_SamplesMade + 48000 * 10;
    u32 table = g_StreamBase + 0x1E460;
    s32 index = static_cast<s32>(Load<u32>(g_StreamBase + 0x13404));
    std::string line;
    u32 taken = 0;
    for (u32 n = 0; n < 512 && index >= 0 && index < 512; n++)
    {
        u32 entry = table + static_cast<u32>(index) * 20;
        u32 address = Load<u32>(entry);
        u32 size = Load<u32>(entry + 4);
        bool used = Load<u16>(entry + 0x10) != 0;
        taken += used ? size : 0;
        char text[48];
        std::snprintf(text, sizeof(text), " %s%#x+%#x", used ? "" : "free ", address, size);
        line += text;
        index = static_cast<s16>(Load<u16>(entry + 0xC));
    }

    Native::Log("iop: sound memory from %#x: %u bytes taken;%s", Load<u32>(g_StreamBase + 0x12EFC), taken, line.c_str());
}

void Iop::RunSamples(u32 samples, std::vector<s16>* out)
{
    LogSoundBlocks();
    g_Output = out;
    u64 target = g_NextSample + static_cast<u64>(samples - 1) * Spu2::CyclesPerSample;
    Advance(target + 1);
    ProcessEvents();
    g_Output = nullptr;
}

void Iop::Post(Call* call)
{
    g_Calls.push_back(call);
}

void Iop::RunUntilIdle(std::vector<s16>* out)
{
    g_Output = out;
    for (u32 i = 0; i < 100000; i++)
    {
        DeliverCalls();
        if (g_Calls.empty() && PickThread() == nullptr && g_Pending == 0)
        {
            break;
        }

        Advance(g_Cycles + 64);
    }

    g_Output = nullptr;
}

bool Iop::ServerExists(s32 server)
{
    for (const RpcServer& s : g_Servers)
    {
        if (s.id == server)
        {
            return true;
        }
    }

    return false;
}

void Iop::SetEeServer(s32 id, EeServer server)
{
    g_EeServers[id] = server;
}

void Iop::SetEeWriter(EeWriter writer)
{
    g_EeWriter = writer;
}

void Iop::LogState()
{
    static const char* waits[] = {"-", "semaphore", "event flag", "sleep", "delay", "rpc loop", "cd sync"};
    Native::Log("iop: cycle %llu, interrupts %s, pending %#llx, %zu calls queued", static_cast<unsigned long long>(g_Cycles),
                g_InterruptsEnabled ? "on" : "off", static_cast<unsigned long long>(g_Pending), g_Calls.size());
    // $TWINSANITY_IOP_WATCH: STREAM.IRX's words at these offsets (hex, comma separated)
    if (const char* watch = std::getenv("TWINSANITY_IOP_WATCH"))
    {
        std::string line;
        for (const char* at = watch; *at != 0;)
        {
            char* end = nullptr;
            u32 offset = static_cast<u32>(std::strtoul(at, &end, 16));
            char item[48];
            std::snprintf(item, sizeof(item), " %x=%x", offset, Load<u32>(g_StreamBase + offset));
            line += item;
            at = *end == ',' ? end + 1 : end;
            if (end == at && *at != 0 && *at != ',')
            {
                break;
            }
        }

        Native::Log("iop:   watch%s", line.c_str());
    }

    for (const auto& [id, thread] : g_Threads)
    {
        Native::Log("iop:   thread %d prio %d %s %s(%d) pc %#x ra %#x", id, thread.priority,
                    thread.state == State::Ready ? "ready" : thread.state == State::Waiting ? "waiting" : "dormant",
                    waits[static_cast<int>(thread.wait)], thread.waitId, thread.context.pc, thread.context.r[Ra]);
    }
}

u32 Iop::Allocate(u32 size)
{
    return ::Allocate(0, size, 0);
}

void Iop::Free(u32 address)
{
    ::Free(address);
}

u64 Iop::SamplesMade()
{
    return g_SamplesMade;
}

u8* Iop::Ram()
{
    return g_Ram;
}

u64 Iop::Cycles()
{
    return g_Cycles;
}
