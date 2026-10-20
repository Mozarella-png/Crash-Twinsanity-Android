#pragma once

#include "common.h"

#include <tuple>
#include <type_traits>

// The retail code passes arguments the way GCC 2.9x's EABI does: integers and pointers in $a0-$a7 in their order, floats in
// $f12-$f19 in theirs. The C++ is n32, where every argument has a position and goes in that position's register of its kind:
// the two agree when a function takes no floats, or nothing but floats. A function taking both goes through a thunk that moves
// the registers, made from its C++ declaration, which gets the retail name with "_n32" (RETAIL_N32):
//
// - EABI_EXPORT(name, function): the C++ function, for the asm (and the retail vtables and function pointers) that call it by
//   its retail name;
// - EABI_IMPORT(name, function): an asm function, for the C++ that calls it through its declaration.
//
// Calls through the retail vtables and function pointers use the EABI's registers whatever is behind them (CallEabi).
#if defined(TWIN_NATIVE) && defined(_WIN32)
// Windows' x64 convention gives an argument the register of its position whatever its kind (rcx or xmm0, rdx or xmm1...), so
// a retail call with the EABI's order (the object, then the floats among the integers anywhere) misses a function declared in
// another: the functions the retail code calls with floats and integers mixed take the System V convention there, which, as the
// EABI, AAPCS64 (macOS) and Linux's, fills the integers' and the floats' registers each in their own order. Calls through the
// retail vtables and pointers with mixed arguments go the same way (CallEabi, gcc2.h's CallVirtual)
#define TWIN_EABI_CC __attribute__((sysv_abi))
#define RETAIL_N32(name) RETAIL(name) TWIN_EABI_CC
#elif defined(TWIN_NATIVE)
#define TWIN_EABI_CC
#define RETAIL_N32(name) RETAIL(name)
#else
#define RETAIL_N32(name) asm(#name "_n32")
#endif
#if defined(__INTELLISENSE__) || defined(TWIN_NATIVE)
// VS Code's C/C++ extension doesn't take asm made of constant expressions. The native build has one calling convention, so
// there's nothing to thunk
#define EABI_EXPORT(name, function)
#define EABI_IMPORT(name, function)
#else
#define EABI_EXPORT(name, function) asm((Abi::Thunk<decltype(function)>(Abi::Direction::Export, #name)))
#define EABI_IMPORT(name, function) asm((Abi::Thunk<decltype(function)>(Abi::Direction::Import, #name)))
#endif

namespace Abi
{
constexpr u32 ArgumentRegisters = 8;
// The registers of the first argument of each kind: $a0 ($4) and $f12
constexpr u32 FirstIntegerArgumentRegister = 4;
constexpr u32 FirstFloatArgumentRegister = 12;

enum class Direction
{
    // EABI to n32
    Export,
    // n32 to EABI
    Import,
};

template <typename T>
constexpr bool IsFloat = std::is_same_v<std::remove_cv_t<T>, f32>;

template <typename T>
consteval void CheckArgument()
{
    static_assert(IsFloat<T> || std::is_integral_v<T> || std::is_enum_v<T> || std::is_pointer_v<T> || std::is_reference_v<T>,
                  "only integers, pointers and floats are passed the same way by both conventions");
    static_assert(IsFloat<T> || sizeof(T) <= 8, "128 bit values aren't passed the same way");
}

template <typename Function>
struct Signature;

template <typename Result, typename... Args>
struct Signature<Result(Args...)>
{
    static constexpr u32 Count = sizeof...(Args);
    static constexpr bool Floats[Count + 1] = {IsFloat<Args>..., false};
};

// A pointer, which picks one of overloaded functions (static_cast to its type).
// Spelled out rather than derived from Signature<Result(Args...)>: clang 18 crashes on a partial specialization that
// derives from another one (the probe in native/tools/convert_data.py stops there). The members are the same.
template <typename Result, typename... Args>
struct Signature<Result (*)(Args...)>
{
    static constexpr u32 Count = sizeof...(Args);
    static constexpr bool Floats[Count + 1] = {IsFloat<Args>..., false};
};

// Methods take the object first
template <typename Class, typename Result, typename... Args>
struct Signature<Result (Class::*)(Args...)>
{
    static constexpr u32 Count = sizeof...(Args) + 1;
    static constexpr bool Floats[Count + 1] = {false, IsFloat<Args>..., false};
};

template <typename Class, typename Result, typename... Args>
struct Signature<Result (Class::*)(Args...) const>
{
    static constexpr u32 Count = sizeof...(Args) + 1;
    static constexpr bool Floats[Count + 1] = {false, IsFloat<Args>..., false};
};

struct AsmText
{
    char text[2048]{};
    u32 length = 0;

    constexpr void Append(const char* string)
    {
        while (*string != '\0')
        {
            text[length++] = *string++;
        }
    }

    constexpr void Append(u32 number)
    {
        char digits[10]{};
        u32 count = 0;
        do
        {
            digits[count++] = static_cast<char>('0' + number % 10);
            number /= 10;
        } while (number != 0);

        while (count != 0)
        {
            text[length++] = digits[--count];
        }
    }

    constexpr const char* data() const
    {
        return text;
    }

    constexpr u32 size() const
    {
        return length;
    }
};

// The thunk's moves: an argument goes from its EABI register (its index among its kind) to its n32 one (its position) on the way
// in, back on the way out. An argument's n32 register is never below its EABI one, so exporting moves the last argument first
// and importing the first, and nothing is overwritten before it's moved
template <typename Function>
consteval AsmText Thunk(Direction direction, const char* name)
{
    using Info = Signature<Function>;
    static_assert(Info::Count <= ArgumentRegisters, "arguments on the stack need a thunk of their own");

    u32 eabiIndexes[ArgumentRegisters + 1]{};
    u32 integers = 0;
    u32 floats = 0;
    for (u32 i = 0; i < Info::Count; i++)
    {
        eabiIndexes[i] = Info::Floats[i] ? floats++ : integers++;
    }

    AsmText text;
    const char* symbol = name;
    const char* target = name;
    // The C++ function's symbol, RETAIL_N32's
    char n32Name[128]{};
    u32 length = 0;
    while (name[length] != '\0')
    {
        n32Name[length] = name[length];
        length++;
    }

    for (const char* suffix = "_n32"; *suffix != '\0'; suffix++)
    {
        n32Name[length++] = *suffix;
    }

    if (direction == Direction::Export)
    {
        target = n32Name;
    }
    else
    {
        symbol = n32Name;
    }

    text.Append("\t.pushsection .text.");
    text.Append(symbol);
    text.Append(", \"ax\", @progbits\n\t.globl ");
    text.Append(symbol);
    text.Append("\n\t.type ");
    text.Append(symbol);
    text.Append(", @function\n\t.set push\n\t.set noreorder\n");
    text.Append(symbol);
    text.Append(":\n");
    for (u32 step = 0; step < Info::Count; step++)
    {
        u32 position = direction == Direction::Export ? Info::Count - 1 - step : step;
        u32 eabi = eabiIndexes[position];
        if (eabi == position)
        {
            continue;
        }

        u32 from = direction == Direction::Export ? eabi : position;
        u32 to = direction == Direction::Export ? position : eabi;
        if (Info::Floats[position])
        {
            text.Append("\tmov.s $f");
            text.Append(FirstFloatArgumentRegister + to);
            text.Append(", $f");
            text.Append(FirstFloatArgumentRegister + from);
        }
        else
        {
            text.Append("\tmove $");
            text.Append(FirstIntegerArgumentRegister + to);
            text.Append(", $");
            text.Append(FirstIntegerArgumentRegister + from);
        }

        text.Append("\n");
    }

    text.Append("\tj ");
    text.Append(target);
    text.Append("\n\tnop\n\t.set pop\n\t.size ");
    text.Append(symbol);
    text.Append(", . - ");
    text.Append(symbol);
    text.Append("\n\t.popsection\n");
    return text;
}

// Whether a call with these arguments needs its registers moved
template <typename... Args>
constexpr bool IsMixed = (IsFloat<Args> || ...) && !(IsFloat<Args> && ...);

#ifdef TWIN_NATIVE
// A pointer to a function taking these arguments as the retail code passes them: the host's own convention, or the EABI's
// stand-in (TWIN_EABI_CC) when they mix floats and integers
template <typename Result, typename... Args>
struct EabiPointer
{
    using Type = Result (*)(Args...);
};

#ifdef _WIN32
template <typename Result, typename... Args>
    requires IsMixed<Args...>
struct EabiPointer<Result, Args...>
{
    using Type = Result(TWIN_EABI_CC*)(Args...);
};
#endif

#ifdef _WIN32
// The functions declared RETAIL_N32 (System V's), by address (src/platform/native/abi_win32.cpp, made before main)
constexpr u32 SysvTableSize = 2048;
extern const void* g_SysvTable[SysvTableSize];

inline u32 SysvSlot(const void* function)
{
    return static_cast<u32>((reinterpret_cast<u64>(function) * 0x9E3779B97F4A7C15ull) >> 53);
}

inline bool IsSysvFunction(const void* function)
{
    for (u32 slot = SysvSlot(function);; slot = (slot + 1) & (SysvTableSize - 1))
    {
        const void* held = g_SysvTable[slot];
        if (held == function)
        {
            return true;
        }

        if (held == nullptr)
        {
            return false;
        }
    }
}

// An argument a function in Windows' own convention gets: its floats are none (it takes none: one taking floats and integers
// is RETAIL_N32's)
template <typename T>
inline auto KeptArgument(T value)
{
    if constexpr (IsFloat<T>)
    {
        return std::tuple<>();
    }
    else
    {
        return std::tuple<T>(value);
    }
}

// $TWIN_ABI_TRACE: each function called across the conventions logged once (a mixed call to one of Windows' own, or a call
// without floats and integers mixed to one of System V's)
extern bool g_TraceCrossCalls;
void NoteCrossCall(const void* function, bool mixed);
// --selftest-abi (src/platform/native/abi_win32.cpp)
int SelfTest();

template <typename Result, typename... Kept>
inline Result CallKept(const void* function, std::tuple<Kept...> kept)
{
    return std::apply(reinterpret_cast<Result (*)(Kept...)>(const_cast<void*>(function)), kept);
}
#endif

// The native build's functions all take their arguments one way: a call through a retail vtable or function pointer is a call.
// On Windows the call goes the way of the function behind the pointer, whatever its arguments: a vtable's slot can hold a
// function taking a float with its integers (System V's) and one that leaves the float out (Windows'), as the slot's caller
// passes it to both (the EABI passes the integers alike either way)
template <typename Result, typename... Args>
inline Result CallEabi(const void* function, Args... args)
{
#ifdef _WIN32
    if (IsSysvFunction(function))
    {
        if constexpr (!IsMixed<Args...>)
        {
            if (g_TraceCrossCalls)
            {
                NoteCrossCall(function, false);
            }
        }

        return reinterpret_cast<Result(TWIN_EABI_CC*)(Args...)>(const_cast<void*>(function))(args...);
    }

    if constexpr (IsMixed<Args...>)
    {
        if (g_TraceCrossCalls)
        {
            NoteCrossCall(function, true);
        }

        return CallKept<Result>(function, std::tuple_cat(KeptArgument(args)...));
    }
    else
    {
        return reinterpret_cast<Result (*)(Args...)>(const_cast<void*>(function))(args...);
    }
#else
    return reinterpret_cast<typename EabiPointer<Result, Args...>::Type>(const_cast<void*>(function))(args...);
#endif
}
#else
// Calls an EABI function through a pointer: CallEabiTrampoline (src/abi.cpp) loads the arguments' registers from these arrays.
// Integers go in sign extended, like every 32 bit value in a 64 bit register
extern "C" u64 CallEabiInteger(const void* function, const u64* integers, const f32* floats) asm("CallEabiTrampoline");
extern "C" f32 CallEabiFloat(const void* function, const u64* integers, const f32* floats) asm("CallEabiTrampoline");

template <typename T>
inline void Pack(T value, u64* integers, u32& integerCount, f32* floats, u32& floatCount)
{
    CheckArgument<T>();
    if constexpr (IsFloat<T>)
    {
        floats[floatCount++] = value;
    }
    else if constexpr (std::is_pointer_v<T>)
    {
        integers[integerCount++] = static_cast<u64>(static_cast<s64>(static_cast<s32>(reinterpret_cast<u32>(value))));
    }
    else if constexpr (sizeof(T) == 8)
    {
        integers[integerCount++] = static_cast<u64>(value);
    }
    else
    {
        integers[integerCount++] = static_cast<u64>(static_cast<s64>(static_cast<s32>(value)));
    }
}

template <typename Result, typename... Args>
inline Result CallEabi(const void* function, Args... args)
{
    static_assert(sizeof...(Args) <= ArgumentRegisters, "arguments on the stack aren't passed");
    u64 integers[ArgumentRegisters]{};
    f32 floats[ArgumentRegisters]{};
    u32 integerCount = 0;
    u32 floatCount = 0;
    (Pack(args, integers, integerCount, floats, floatCount), ...);
    if constexpr (IsFloat<Result>)
    {
        return CallEabiFloat(function, integers, floats);
    }
    else if constexpr (std::is_void_v<Result>)
    {
        CallEabiInteger(function, integers, floats);
    }
    else if constexpr (std::is_pointer_v<Result>)
    {
        return reinterpret_cast<Result>(static_cast<u32>(CallEabiInteger(function, integers, floats)));
    }
    else
    {
        return static_cast<Result>(CallEabiInteger(function, integers, floats));
    }
}

#endif
}
