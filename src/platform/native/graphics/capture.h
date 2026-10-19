#pragma once

// Recordings of what the game sends the graphics hardware, to play back without the game (the harness's --replay): the state
// of VU1, VIF1, the GIF and the GS when the recording starts, then VIF1's words and the GIF's path 3 quadwords in the order
// they're sent, and the PCRTC's registers at each picture shown. Played back, the same build draws the same pictures, so a
// change to the hardware's emulation can be checked against real frames picture for picture.
//
//     TWIN_CAPTURE=FILE:FIRST:COUNT   the frames after the FIRST picture shown, COUNT of them, recorded into FILE

#include "common.h"

#include <functional>

namespace Gs
{
struct DisplayImage;
}

namespace NativeGraphics
{
// The recording's hooks (cheap when nothing records)
void CaptureVif1(const u32* words, u32 count);
void CaptureGif(const u32* words, u32 count);
// A picture about to be shown: starts, continues and ends a recording
void CaptureShown();

// A recording played back on the hardware: each picture shown read out and handed over with its number and the time it took
// to draw (ms). Returns false when the file can't be read
bool Replay(const char* path, const std::function<void(u32 frame, const Gs::DisplayImage& image, double ms)>& shown,
            bool readPictures = true);
}
