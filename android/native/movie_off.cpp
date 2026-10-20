// The Android build's movies, until FFmpeg is built for Android (the desktop build links the LGPL FFmpeg that
// native/tools/build_ffmpeg.sh makes for the host). Open reports that the movie couldn't be opened, and the game takes that
// as the PS2's failure path (src/game/movie.cpp skips the movie and gives the renderer its memory back). Everything else
// does nothing: a movie that never opens is never stepped, drawn or closed.
#include "platform/movie.h"

#include "native.h"

namespace Platform::Movie
{
struct Player
{
    s32 unused;
};

void Construct(Player* player)
{
    player->unused = 0;
}

void BeginPresenting(Player*, void (*)())
{
}

bool Open(Player*, const char* file, u32, s32, void*)
{
    Native::Log("movie: %s not played (the Android build has no movie decoder yet)", file);
    return false;
}

void StartSound(Player*, f32)
{
}

bool Step(Player*)
{
    return false;
}

void WaitFrame(Player*)
{
}

void QueuePicture(Player*)
{
}

void Draw(Player*, const Area&, s32, s32, u32)
{
}

void Close(Player*)
{
}
}

namespace Native
{
void LogMovieLibraries()
{
    Native::Log("movie: no decoder in the Android build");
}
}
