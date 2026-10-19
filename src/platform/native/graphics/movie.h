#pragma once

// The movies' pictures for the native movie player (Platform::Movie): a decoded picture put where the PS2's player puts it, and
// the picture drawn the way the PS2's player draws it

#include "common.h"

#include "platform/movie.h"

// A decoded picture (RGBA bytes, rows stride bytes apart) sent to the GS: the PS2's player has the IPU's 32 bit pictures sent
// into the depth buffer's memory (a buffer as wide as the picture rounded up to 64 pixels) by the GIF at the vertical blank
// (QueuePicture). Native: its alpha is the IPU's 0x80 for every pixel
void NativeGraphicsUploadMoviePicture(const u8* rgba, s32 width, s32 height, s32 stride);

// The last picture sent drawn over the area of the frame (Platform::Movie::Draw's packet: 32 pixel wide strips of sprites into
// the movie bucket, the picture's width by height pixels without skippedRows rows at the top and the bottom). The player calls
// it once it has sent two pictures
void NativeGraphicsDrawMoviePicture(const Platform::Movie::Area& area, s32 width, s32 height, u32 skippedRows);
