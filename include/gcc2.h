#pragma once

#include "abi.h"
#include "common.h"

// The game was built with GCC 2.9x, whose C++ ABI modern compilers don't speak. Its classes keep their vtable pointer after the
// members of the first class that declares a virtual function (not at offset 0), and a vtable is an array of entries of 8 bytes:
// the offset to add to the object, an index only virtual bases use, and the function. Entry 0 is empty (there's no RTTI).
// Classes whose vtables are still the retail ones keep that layout and call their virtual functions through CallVirtual.
struct GccVTableEntry
{
    s16 delta;
    s16 index;
    void* function;
};
CHECK_SIZE(GccVTableEntry, 8);

#ifdef TWIN_NATIVE
// Natively the function behind a retail vtable entry is C++ returning its value at its own width, and a call through the
// entry reads it at the width the caller asks for (CallVirtual<u32> of a bool function). The PS2's and Apple's arm64 callees
// extend a narrow value to the whole register; x86-64's leave the bits above it as they were, so the caller has to know. The
// data converter (native/tools/convert_data.py) marks an entry whose function returns a narrow integer in its index, which
// only virtual bases use (none of the game's)
enum GccNarrowReturn : u16
{
    GccReturnU8 = 0x1000,
    GccReturnS8 = 0x2000,
    GccReturnU16 = 0x3000,
    GccReturnS16 = 0x4000,
    GccReturnMask = 0x7000,
};

template <typename Result, typename... Args>
inline Result CallNarrowed(const GccVTableEntry& entry, void* self, Args... args)
{
    // (Nested: sizeof isn't asked of void)
    if constexpr (std::is_integral_v<Result>)
    {
        if constexpr (sizeof(Result) > 1)
        {
            switch (static_cast<u16>(entry.index) & GccReturnMask)
            {
            case GccReturnU8:
                return static_cast<Result>(Abi::CallEabi<u8>(entry.function, self, args...));
            case GccReturnS8:
                return static_cast<Result>(Abi::CallEabi<s8>(entry.function, self, args...));
            case GccReturnU16:
                return static_cast<Result>(Abi::CallEabi<u16>(entry.function, self, args...));
            case GccReturnS16:
                return static_cast<Result>(Abi::CallEabi<s16>(entry.function, self, args...));
            default:
                break;
            }
        }
    }

    return Abi::CallEabi<Result>(entry.function, self, args...);
}
#endif

template <typename Result, typename... Args>
inline Result CallVirtual(const void* object, const GccVTableEntry* vtable, u32 slot, Args... args)
{
    const GccVTableEntry& entry = vtable[slot];
    void* self = const_cast<u8*>(static_cast<const u8*>(object)) + entry.delta;
#ifdef TWIN_NATIVE
    // Natively a plain call, in the EABI's stand-in when the arguments mix floats and integers (Abi::EabiPointer)
    return CallNarrowed<Result>(entry, self, args...);
#else
    // The vtables hold the retail functions (or thunks to C++), which take their floats the EABI's way
    if constexpr (Abi::IsMixed<void*, Args...>)
    {
        return Abi::CallEabi<Result>(entry.function, self, args...);
    }
    else
    {
        return reinterpret_cast<Result (*)(void*, Args...)>(entry.function)(self, args...);
    }
#endif
}

// A GCC 2.9x pointer to a data member: the member's offset plus 1, 0 being the null pointer
#define GCC2_MEMBER_POINTER(type, member) (static_cast<u32>(offsetof(type, member)) + 1)

// A GCC 2.9x destructor takes a second argument: bit 0 frees the object after destroying it (a member array's elements are
// destroyed with none)
enum DestructorFlags : u32
{
    DestroyElement = 0,
    DestroyOnly = 2,
    DestroyAndFree = 3,
};

// GCC 2.9x's __main: main runs the static constructors (the retail __CTOR_LIST__) through it, once
extern "C" void RunStaticConstructors() RETAIL(FUN_002d6dc8);

// The priority of the static constructors that don't give one (GCC's DEFAULT_INIT_PRIORITY): a file's static initialisation
// (__static_initialization_and_destruction_0) runs its constructors when it's called with it and its first argument set, its
// destructors with that argument clear
constexpr u32 DefaultInitPriority = 0xFFFF;
