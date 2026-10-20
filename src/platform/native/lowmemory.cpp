// The PS2's low memory for the retail code's null pointers. On the PS2 the game runs with the kernel's RAM at address 0: a read
// through a null pointer (or a small offset from one) returns kernel memory and goes on, a write changes it (docs/RETAIL_BUGS.md,
// "Low addresses"); several are reached in normal play. Nothing can be mapped there natively (macOS keeps the first 4 GB
// unmapped), so a load or store faulting below LowMemoryLimit is done here instead: the faulting instruction is decoded and
// carried out on g_NativeLowMemory, and the program goes on after it. A jump or call through such an address crashes, as on the
// PS2. g_NativeLowMemory holds zeros unless a dump of the PS2's (build/native/ps2_low_memory.bin) is given (native/NATIVE.md)
// Windows: lowmemory_win32.cpp
#if !defined(_WIN32)
#include "native.h"

#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <execinfo.h>
#include <fstream>
#include <sys/ucontext.h>
#include <unistd.h>

// Bionic (Android) has no backtrace(): the NDK declares execinfo.h without it. On Android the dumps just skip the frames;
// everywhere else they go to stderr as before.
#if defined(__ANDROID__)
static void WriteBacktrace(void** frames, int count)
{
    (void)frames;
    (void)count;
}
#else
static void WriteBacktrace(void** frames, int count)
{
    backtrace_symbols_fd(frames, backtrace(frames, count), 2);
}
#endif

extern "C"
{
    alignas(16) u8 g_NativeLowMemory[NativeLowMemoryBytes] = {};
}

namespace
{
#if defined(__aarch64__)
constexpr uiptr LowMemoryLimit = NativeLowMemoryBytes;

struct sigaction g_Previous[2];
u32 g_Emulated = 0;

// The registers of the interrupted thread: x0-x30, sp, pc and the 32 vector registers
struct Registers
{
#if defined(__APPLE__)
    explicit Registers(ucontext_t* context) : state(&context->uc_mcontext->__ss), vectors(&context->uc_mcontext->__ns)
    {
    }

    u64& X(u32 index)
    {
        return index == 29 ? reinterpret_cast<u64&>(state->__fp) : index == 30 ? reinterpret_cast<u64&>(state->__lr)
                                                                              : reinterpret_cast<u64&>(state->__x[index]);
    }
    u64& Sp()
    {
        return reinterpret_cast<u64&>(state->__sp);
    }
    u64& Pc()
    {
        return reinterpret_cast<u64&>(state->__pc);
    }
    u8* Vector(u32 index)
    {
        return reinterpret_cast<u8*>(&vectors->__v[index]);
    }

    __darwin_arm_thread_state64* state;
    __darwin_arm_neon_state64* vectors;
#else
    explicit Registers(ucontext_t* context) : machine(&context->uc_mcontext)
    {
        // The vector registers are in the reserved area's FPSIMD record
        auto* record = reinterpret_cast<_aarch64_ctx*>(machine->__reserved);
        while (record->magic != 0 && record->magic != FPSIMD_MAGIC)
        {
            record = reinterpret_cast<_aarch64_ctx*>(reinterpret_cast<u8*>(record) + record->size);
        }

        simd = record->magic == FPSIMD_MAGIC ? reinterpret_cast<fpsimd_context*>(record) : nullptr;
    }

    u64& X(u32 index)
    {
        return reinterpret_cast<u64&>(machine->regs[index]);
    }
    u64& Sp()
    {
        return reinterpret_cast<u64&>(machine->sp);
    }
    u64& Pc()
    {
        return reinterpret_cast<u64&>(machine->pc);
    }
    u8* Vector(u32 index)
    {
        return simd != nullptr ? reinterpret_cast<u8*>(&simd->vregs[index]) : nullptr;
    }

    mcontext_t* machine;
    fpsimd_context* simd;
#endif
};

s64 SignExtend(u64 value, u32 bits)
{
    u64 sign = 1ull << (bits - 1);
    return static_cast<s64>((value ^ sign) - sign);
}

bool InLowMemory(u64 address, u32 bytes)
{
    return address < LowMemoryLimit && address + bytes <= LowMemoryLimit;
}

// One register's load or store at an address of low memory (size in bytes; signed loads extend to 32 or 64 bits)
bool Access(Registers& registers, bool vector, bool load, u32 size, bool signExtend, bool to32, u32 rt, u64 address)
{
    if (!InLowMemory(address, size))
    {
        return false;
    }

    u8* memory = g_NativeLowMemory + address;
    if (vector)
    {
        u8* reg = registers.Vector(rt);
        if (reg == nullptr)
        {
            return false;
        }

        if (load)
        {
            std::memset(reg, 0, 16);
            std::memcpy(reg, memory, size);
        }
        else
        {
            std::memcpy(memory, reg, size);
        }

        return true;
    }

    if (load)
    {
        u64 value = 0;
        std::memcpy(&value, memory, size);
        if (signExtend)
        {
            s64 extended = SignExtend(value, size * 8);
            value = to32 ? static_cast<u32>(extended) : static_cast<u64>(extended);
        }

        if (rt != 31)
        {
            registers.X(rt) = value;
        }
    }
    else
    {
        u64 value = rt == 31 ? 0 : registers.X(rt);
        std::memcpy(memory, &value, size);
    }

    return true;
}

u64 Base(Registers& registers, u32 rn)
{
    return rn == 31 ? registers.Sp() : registers.X(rn);
}

void WriteBase(Registers& registers, u32 rn, u64 value)
{
    if (rn == 31)
    {
        registers.Sp() = value;
    }
    else
    {
        registers.X(rn) = value;
    }
}

// The single register loads and stores' size and kind from size (bits 31-30), V (26) and opc (23-22)
bool SingleKind(u32 instruction, bool* vector, u32* bytes, bool* load, bool* signExtend, bool* to32)
{
    u32 size = instruction >> 30;
    u32 opc = (instruction >> 22) & 3;
    *vector = ((instruction >> 26) & 1) != 0;
    if (*vector)
    {
        u32 scale = ((opc & 2) << 1) | size;
        if (scale > 4)
        {
            return false;
        }

        *bytes = 1u << scale;
        *load = (opc & 1) != 0;
        *signExtend = false;
        *to32 = false;
        return true;
    }

    *bytes = 1u << size;
    switch (opc)
    {
    case 0:
        *load = false;
        *signExtend = false;
        *to32 = false;
        return true;
    case 1:
        *load = true;
        *signExtend = false;
        *to32 = false;
        return true;
    case 2:
        // A signed load to 64 bits (size 3: a prefetch, done by doing nothing)
        *load = true;
        *signExtend = size != 3;
        *to32 = false;
        return size != 3 || true;
    default:
        *load = true;
        *signExtend = true;
        *to32 = true;
        return size < 2;
    }
}

bool Emulate(Registers& registers)
{
    u32 instruction;
    std::memcpy(&instruction, reinterpret_cast<const void*>(registers.Pc()), sizeof(instruction));
    u32 rt = instruction & 31;
    u32 rn = (instruction >> 5) & 31;
    // A prefetch (PRFM, PRFUM: size 3, opc 2 of the single register forms) does nothing
    if ((instruction & 0x3B000000) == 0x39000000 || (instruction & 0x3B000000) == 0x38000000)
    {
        if (((instruction >> 26) & 1) == 0 && (instruction >> 30) == 3 && ((instruction >> 22) & 3) == 2)
        {
            return true;
        }
    }

    // Load/store register, unsigned immediate: x x 1 1 1 V 0 1 opc imm12 Rn Rt
    if ((instruction & 0x3B000000) == 0x39000000)
    {
        bool vector, load, signExtend, to32;
        u32 bytes;
        if (!SingleKind(instruction, &vector, &bytes, &load, &signExtend, &to32))
        {
            return false;
        }

        if (!vector && (instruction >> 30) == 3 && ((instruction >> 22) & 3) == 2)
        {
            return true;
        }

        u64 address = Base(registers, rn) + ((instruction >> 10) & 0xFFF) * bytes;
        return Access(registers, vector, load, bytes, signExtend, to32, rt, address);
    }

    // Load/store register, unscaled, post- and pre-indexed, register offset: x x 1 1 1 V 0 0 opc ...
    if ((instruction & 0x3B000000) == 0x38000000)
    {
        bool vector, load, signExtend, to32;
        u32 bytes;
        if (!SingleKind(instruction, &vector, &bytes, &load, &signExtend, &to32))
        {
            return false;
        }

        u32 mode = (instruction >> 10) & 3;
        u64 base = Base(registers, rn);
        if (((instruction >> 21) & 1) != 0)
        {
            if (mode != 2)
            {
                return false;
            }

            // Register offset: Rm extended by option, shifted by the size when S
            u32 rm = (instruction >> 16) & 31;
            u32 option = (instruction >> 13) & 7;
            u64 offset = rm == 31 ? 0 : registers.X(rm);
            switch (option)
            {
            case 2:
                offset = static_cast<u32>(offset);
                break;
            case 6:
                offset = static_cast<u64>(static_cast<s64>(static_cast<s32>(offset)));
                break;
            default:
                break;
            }

            if (((instruction >> 12) & 1) != 0)
            {
                offset <<= __builtin_ctz(bytes);
            }

            return Access(registers, vector, load, bytes, signExtend, to32, rt, base + offset);
        }

        s64 offset = SignExtend((instruction >> 12) & 0x1FF, 9);
        u64 address = mode == 1 ? base : base + offset;
        if (!Access(registers, vector, load, bytes, signExtend, to32, rt, address))
        {
            return false;
        }

        if (mode == 1 || mode == 3)
        {
            WriteBase(registers, rn, base + offset);
        }

        return true;
    }

    // Load/store pair: opc x 1 0 1 V 0 mode L imm7 Rt2 Rn Rt (mode 01 post, 10 offset, 11 pre)
    if ((instruction & 0x3A000000) == 0x28000000)
    {
        u32 opc = instruction >> 30;
        bool vector = ((instruction >> 26) & 1) != 0;
        bool load = ((instruction >> 22) & 1) != 0;
        u32 mode = (instruction >> 23) & 3;
        u32 rt2 = (instruction >> 10) & 31;
        u32 bytes = vector ? 4u << opc : opc == 2 ? 8 : 4;
        bool signExtend = !vector && opc == 1;
        if (mode == 0)
        {
            return false;
        }

        u64 base = Base(registers, rn);
        s64 offset = SignExtend((instruction >> 15) & 0x7F, 7) * bytes;
        u64 address = mode == 1 ? base : base + offset;
        if (!Access(registers, vector, load, bytes, signExtend, false, rt, address) ||
            !Access(registers, vector, load, bytes, signExtend, false, rt2, address + bytes))
        {
            return false;
        }

        if (mode == 1 || mode == 3)
        {
            WriteBase(registers, rn, base + offset);
        }

        return true;
    }

    return false;
}

void OnFault(int signal, siginfo_t* info, void* context)
{
    auto address = reinterpret_cast<uiptr>(info->si_addr);
    Registers registers(static_cast<ucontext_t*>(context));
    if (address < LowMemoryLimit && registers.Pc() != address && Emulate(registers))
    {
        registers.Pc() += 4;
        if (g_Emulated++ == 0 && Native::GetSettings().logStubs)
        {
            // Once: the PS2's way with its null pointers is being followed (see native/NATIVE.md)
            static const char message[] = "[native] a retail null pointer read/write went to the PS2's low memory\n";
            ::write(2, message, sizeof(message) - 1);
        }

        return;
    }

    // Anything else: the fault it is (the previous handler, else the default, which ends the program), said first with where it
    // happened, since the default ends the program without a word
    char message[128];
    int length = std::snprintf(message, sizeof(message), "[native] fatal %s at %#llx (pc %#llx), backtrace:\n",
                               signal == SIGBUS ? "SIGBUS" : "SIGSEGV", static_cast<unsigned long long>(address),
                               static_cast<unsigned long long>(registers.Pc()));
    ::write(2, message, static_cast<size_t>(length));
#if defined(__APPLE__)
    const auto& state = static_cast<ucontext_t*>(context)->uc_mcontext->__ss;
    for (int i = 0; i < 29; i += 4)
    {
        length = std::snprintf(message, sizeof(message), "  x%-2d %016llx  x%-2d %016llx  x%-2d %016llx  x%-2d %016llx\n", i,
                               static_cast<unsigned long long>(state.__x[i]), i + 1,
                               static_cast<unsigned long long>(i + 1 < 29 ? state.__x[i + 1] : state.__fp), i + 2,
                               static_cast<unsigned long long>(i + 2 < 29 ? state.__x[i + 2] : state.__lr), i + 3,
                               static_cast<unsigned long long>(i + 3 < 29 ? state.__x[i + 3] : state.__sp));
        ::write(2, message, static_cast<size_t>(length));
    }
#endif
    void* frames[48];
    WriteBacktrace(frames, 48);
    struct sigaction& previous = g_Previous[signal == SIGBUS ? 1 : 0];
    sigaction(signal, &previous, nullptr);
}
#elif defined(__x86_64__)
// The x86-64 way: the faulting instruction's memory operand is found (its base register, or its index) and that register moved
// by the address of g_NativeLowMemory, so the same instruction, run again, reads or writes the buffer; the processor's trap flag
// stops it after that one instruction and the register is put back (left alone when the instruction wrote it: a load into its
// own base). Nothing of the instruction's meaning is needed, so every load and store the compiler makes works
constexpr uiptr LowMemoryLimit = NativeLowMemoryBytes - 64;

struct sigaction g_Previous[3];
u32 g_Emulated = 0;

// The step in progress on this thread: the register moved, what it held and what it holds with the shift
struct Step
{
    bool stepping = false;
    int reg = 0;
    u64 original = 0;
    u64 shifted = 0;
};
thread_local Step t_Step;

// RFLAGS' trap flag: a debug trap (SIGTRAP) after the next instruction
constexpr u64 TrapFlag = 0x100;

#if defined(__APPLE__)
using MachineState = __darwin_x86_thread_state64;
MachineState& StateOf(void* context)
{
    return static_cast<ucontext_t*>(context)->uc_mcontext->__ss;
}

u64& Reg(MachineState& s, int n)
{
    switch (n)
    {
    case 0: return reinterpret_cast<u64&>(s.__rax);
    case 1: return reinterpret_cast<u64&>(s.__rcx);
    case 2: return reinterpret_cast<u64&>(s.__rdx);
    case 3: return reinterpret_cast<u64&>(s.__rbx);
    case 4: return reinterpret_cast<u64&>(s.__rsp);
    case 5: return reinterpret_cast<u64&>(s.__rbp);
    case 6: return reinterpret_cast<u64&>(s.__rsi);
    case 7: return reinterpret_cast<u64&>(s.__rdi);
    case 8: return reinterpret_cast<u64&>(s.__r8);
    case 9: return reinterpret_cast<u64&>(s.__r9);
    case 10: return reinterpret_cast<u64&>(s.__r10);
    case 11: return reinterpret_cast<u64&>(s.__r11);
    case 12: return reinterpret_cast<u64&>(s.__r12);
    case 13: return reinterpret_cast<u64&>(s.__r13);
    case 14: return reinterpret_cast<u64&>(s.__r14);
    default: return reinterpret_cast<u64&>(s.__r15);
    }
}
u64& Rip(MachineState& s) { return reinterpret_cast<u64&>(s.__rip); }
u64& Flags(MachineState& s) { return reinterpret_cast<u64&>(s.__rflags); }
#else
using MachineState = mcontext_t;
MachineState& StateOf(void* context)
{
    return static_cast<ucontext_t*>(context)->uc_mcontext;
}

// gregs' indexes of the general registers, in the instruction encoding's order (rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8-r15)
constexpr int GeneralRegisters[] = {REG_RAX, REG_RCX, REG_RDX, REG_RBX, REG_RSP, REG_RBP, REG_RSI, REG_RDI,
                                    REG_R8,  REG_R9,  REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15};
u64& Reg(MachineState& s, int n)
{
    return reinterpret_cast<u64&>(s.gregs[GeneralRegisters[n]]);
}
u64& Rip(MachineState& s) { return reinterpret_cast<u64&>(s.gregs[REG_RIP]); }
u64& Flags(MachineState& s) { return reinterpret_cast<u64&>(s.gregs[REG_EFL]); }
#endif

// The memory operand of the instruction at p: its base and index registers (-1: none) and the index's scale. False for what has
// none to move (a register operand, a RIP-relative or absolute address, the string instructions and the moffs forms)
bool FindMemoryOperand(const u8* p, int* base, int* index, int* scale)
{
    bool rexX = false;
    bool rexB = false;
    for (;;)
    {
        u8 b = *p;
        if (b == 0x66 || b == 0x67 || b == 0xF2 || b == 0xF3 || b == 0xF0 || b == 0x2E || b == 0x36 || b == 0x3E || b == 0x26 ||
            b == 0x64 || b == 0x65)
        {
            if (b == 0x67)
            {
                return false;
            }

            p++;
            continue;
        }

        break;
    }

    if ((*p & 0xF0) == 0x40)
    {
        rexX = (*p & 2) != 0;
        rexB = (*p & 1) != 0;
        p++;
    }

    if (*p == 0xC5)
    {
        // VEX, two bytes: C5, R/vvvv/L/pp, then the opcode
        p += 3;
    }
    else if (*p == 0xC4)
    {
        // VEX, three bytes: C4, R/X/B/mmmmm, W/vvvv/L/pp, then the opcode (the bits are inverted)
        rexX = (p[1] & 0x40) == 0;
        rexB = (p[1] & 0x20) == 0;
        p += 4;
    }
    else if (*p == 0x62)
    {
        // EVEX: not made by the compiler for this build
        return false;
    }
    else
    {
        u8 op = *p++;
        if (op == 0x0F)
        {
            u8 second = *p++;
            if (second == 0x38 || second == 0x3A)
            {
                p++;
            }
        }
        else if ((op >= 0xA0 && op <= 0xA7) || (op >= 0xAA && op <= 0xAF) || op == 0xD7 || (op >= 0x50 && op <= 0x5F) ||
                 (op >= 0x70 && op <= 0x7F) || (op >= 0xB0 && op <= 0xBF) || (op >= 0x90 && op <= 0x9F) || op >= 0xE0)
        {
            return false;
        }
        else
        {
            // A one byte opcode: its ModRM byte follows
        }
    }

    u8 modrm = *p++;
    u8 mod = modrm >> 6;
    u8 rm = modrm & 7;
    if (mod == 3)
    {
        return false;
    }

    *scale = 1;
    *index = -1;
    if (rm == 4)
    {
        u8 sib = *p++;
        *scale = 1 << (sib >> 6);
        int i = ((sib >> 3) & 7) | (rexX ? 8 : 0);
        *index = i == 4 ? -1 : i;
        if ((sib & 7) == 5 && mod == 0)
        {
            // No base: a 32 bit displacement in its place
            *base = -1;
        }
        else
        {
            *base = (sib & 7) | (rexB ? 8 : 0);
        }

        return true;
    }

    if (rm == 5 && mod == 0)
    {
        return false;
    }

    *base = rm | (rexB ? 8 : 0);
    return true;
}

void OnFault(int signal, siginfo_t* info, void* context)
{
    MachineState& state = StateOf(context);
    auto address = reinterpret_cast<uiptr>(info->si_addr);
    int base = -1;
    int index = -1;
    int scale = 1;
    if (address < LowMemoryLimit && !t_Step.stepping && FindMemoryOperand(reinterpret_cast<const u8*>(Rip(state)), &base, &index, &scale) &&
        !(base >= 0 && base == index))
    {
        const u64 shift = reinterpret_cast<u64>(g_NativeLowMemory);
        int reg = base >= 0 ? base : index;
        u64 add = base >= 0 ? shift : shift / static_cast<u64>(scale);
        if (reg >= 0 && reg != 4)
        {
            static const bool trace = std::getenv("TWIN_LOWMEM_TRACE") != nullptr;
            if (trace)
            {
                char line[160];
                int length = std::snprintf(line, sizeof(line), "[native] lowmem: rip %#llx address %#llx base %d index %d\n",
                                           static_cast<unsigned long long>(Rip(state)), static_cast<unsigned long long>(address), base,
                                           index);
                ::write(2, line, static_cast<size_t>(length));
            }

            u64& r = Reg(state, reg);
            t_Step = {true, reg, r, r + add};
            r += add;
            Flags(state) |= TrapFlag;
            if (g_Emulated++ == 0 && Native::GetSettings().logStubs)
            {
                static const char message[] = "[native] a retail null pointer read/write went to the PS2's low memory\n";
                ::write(2, message, sizeof(message) - 1);
            }

            return;
        }
    }

    char message[160];
    const u8* code = reinterpret_cast<const u8*>(Rip(state));
    int length = std::snprintf(message, sizeof(message),
                               "[native] fatal %s at %#llx (rip %#llx: %02x %02x %02x %02x %02x %02x %02x %02x), backtrace:\n",
                               signal == SIGBUS ? "SIGBUS" : "SIGSEGV", static_cast<unsigned long long>(address),
                               static_cast<unsigned long long>(Rip(state)), code[0], code[1], code[2], code[3], code[4], code[5],
                               code[6], code[7]);
    ::write(2, message, static_cast<size_t>(length));
    void* frames[48];
    WriteBacktrace(frames, 48);
    sigaction(signal, &g_Previous[signal == SIGBUS ? 1 : 0], nullptr);
}

// After the one instruction: the register back as it was, unless the instruction wrote it
void OnTrap(int signal, siginfo_t* info, void* context)
{
    MachineState& state = StateOf(context);
    if (!t_Step.stepping)
    {
        return;
    }

    u64& r = Reg(state, t_Step.reg);
    if (r == t_Step.shifted)
    {
        r = t_Step.original;
    }

    Flags(state) &= ~TrapFlag;
    t_Step.stepping = false;
}
#endif
}

namespace Native
{
// Loads the PS2's low memory dump when there's one, and starts emulating the retail code's low accesses
void StartLowMemory()
{
    std::ifstream dump("build/native/ps2_low_memory.bin", std::ios::binary);
    if (dump)
    {
        dump.read(reinterpret_cast<char*>(g_NativeLowMemory), NativeLowMemoryBytes);
        Log("the PS2's low memory from build/native/ps2_low_memory.bin");
    }

#if defined(__aarch64__)
    struct sigaction action = {};
    action.sa_sigaction = OnFault;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    sigaction(SIGSEGV, &action, &g_Previous[0]);
    sigaction(SIGBUS, &action, &g_Previous[1]);
#elif defined(__x86_64__)
    struct sigaction action = {};
    action.sa_sigaction = OnFault;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    sigaction(SIGSEGV, &action, &g_Previous[0]);
    sigaction(SIGBUS, &action, &g_Previous[1]);
    action.sa_sigaction = OnTrap;
    sigaction(SIGTRAP, &action, &g_Previous[2]);
#else
    Log("the PS2's low memory isn't emulated on this machine's processor: a retail null read will crash");
#endif
}
}

// A free of memory that isn't the game's pool (memory.cpp): where it came from, once a place
void NativeBadFree(void* memory, const void* pages, const void* pagesEnd)
{
    static int reported = 0;
    if (reported++ < 8)
    {
        char message[128];
        int length = std::snprintf(message, sizeof(message), "[native] a free of %p, outside the small pages %p-%p, left alone:\n", memory, pages, pagesEnd);
        ::write(2, message, static_cast<size_t>(length));
        void* frames[24];
        WriteBacktrace(frames, 24);
    }
}

bool NativeDebugHeap()
{
    static const bool debug = std::getenv("TWIN_DEBUG_HEAP") != nullptr;
    return debug;
}

void NativeHeapProblem(const char* what, void* memory)
{
    char message[160];
    int length = std::snprintf(message, sizeof(message), "[native] heap: %s (%p):\n", what, memory);
    ::write(2, message, static_cast<size_t>(length));
    void* frames[16];
    WriteBacktrace(frames, 16);
}
#endif

