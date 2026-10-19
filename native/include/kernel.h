#pragma once

// The native build's stand-in for PS2SDK's kernel.h: the PS2 platform files it reuses (native/reused_ps2.txt) include it

// Interrupts on and off around the TTY's output (libc.cpp): the native build's process has no interrupts to hold off
inline int DIntr()
{
    return 0;
}

inline int EIntr()
{
    return 0;
}

// The EE's uncached view of an address (MultiStream's reads of what the IOP wrote behind the cache): the host has no cache
#define UNCACHED_SEG(x) (x)
