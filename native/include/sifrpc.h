#pragma once

// The native build's stand-in for PS2SDK's sifrpc.h: the types of the SIF RPC's records the reused PS2 MultiStream files
// (src/platform/ps2/multistream/commands.cpp, streams.cpp) declare. The native build's MultiStream transport
// (src/platform/native/audio/multistream.cpp) calls the emulated I/O processor instead of the SIF
#include <tamtypes.h>

typedef void (*SifRpcEndFunc_t)(void* data);

typedef struct SifRpcClientData
{
    void* server;
    u32 unused[8];
} SifRpcClientData_t;

typedef struct SifRpcServerData
{
    u32 unused[16];
} SifRpcServerData_t;

typedef struct SifRpcDataQueue
{
    u32 unused[8];
} SifRpcDataQueue_t;
