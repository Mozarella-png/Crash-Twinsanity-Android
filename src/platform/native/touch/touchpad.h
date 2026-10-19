// The Android touch overlay's state (android/overlay, TouchBridge.java sends it through JNI at every touch). The pads read it
// once a frame, in play only: its buttons join the pad's buttons, and its stick replaces the left stick while it's held.
#pragma once

#include <cstdint>

namespace Touch
{
// The overlay's buttons (the PS2's button bits, pads.cpp) and its stick (0x80 the middle, up 0)
void SetState(uint32_t buttons, uint8_t leftX, uint8_t leftY, bool stickActive);
// Whether the game is in play: the overlay draws and takes touches only then; in the menus they go to the mouse
void SetInPlay(bool inPlay);
bool InPlay();
// Adds the overlay's input to the pad's read
void Apply(uint32_t* buttons, uint8_t* leftX, uint8_t* leftY);
}
